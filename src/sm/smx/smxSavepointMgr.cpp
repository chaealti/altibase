/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: smxSavepointMgr.cpp 90824 2021-05-13 05:35:21Z minku.kang $
 **********************************************************************/

#include <idl.h>
#include <idErrorCode.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smr.h>
#include <smxTrans.h>
#include <smxSavepointMgr.h>
#include <smxReq.h>
#include <svrRecoveryMgr.h>

iduMemPool smxSavepointMgr::mSvpPool;

IDE_RC smxSavepointMgr::initializeStatic()
{
    return mSvpPool.initialize(IDU_MEM_SM_SMX,
                               (SChar*)"SAVEPOINT_MEMORY_POOL",
                               ID_SCALABILITY_SYS,
                               ID_SIZEOF(smxSavepoint),
                               SMX_SAVEPOINT_POOL_SIZE,
                               IDU_AUTOFREE_CHUNK_LIMIT,		/* ChunkLimit */
                               ID_TRUE,							/* UseMutex */
                               IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                               ID_FALSE,						/* ForcePooling */
                               ID_TRUE,							/* GarbageCollection */
                               ID_TRUE,                         /* HWCacheLine */
                               IDU_MEMPOOL_TYPE_LEGACY          /* mempool type */);
}

IDE_RC smxSavepointMgr::destroyStatic()
{
    return mSvpPool.destroy();
}

void smxSavepointMgr::initialize(smxTrans * /*a_pTrans*/)
{

    /* Explicit Savepoint List�� �ʱ�ȭ �Ѵ�. */
    mExpSavepoint.mPrvSavepoint = &mExpSavepoint;
    mExpSavepoint.mNxtSavepoint = &mExpSavepoint;

    /* Implicit Savepoint List�� �ʱ�ȭ �Ѵ�. */
    mImpSavepoint.mPrvSavepoint = &mImpSavepoint;
    mImpSavepoint.mNxtSavepoint = &mImpSavepoint;

    /* setReplSavepoint���� �����ϱ⶧���� �ʱ�ȭ�ؾ� �մϴ�. */
    mImpSavepoint.mReplImpSvpStmtDepth = 0;

    mReservePsmSvp = ID_FALSE;

    // TASK-7244 PSM partial rollback in Sharding
    mReserveShardPsmSvp = ID_FALSE;

    mPsmSavepoint.mOIDNode = NULL;

    /* BUG-45368 */
    mIsUsedPreparedSP = ID_FALSE;
    
    mDDLInfoCount = 0;
}

IDE_RC smxSavepointMgr::destroy( void * aSmiTrans )
{

    smxSavepoint *sSavepoint;
    smxSavepoint *sNxtSavepoint;

    //free save point buffer
    sSavepoint = mExpSavepoint.mNxtSavepoint;

    while(sSavepoint != &mExpSavepoint)
    {
        if ( sSavepoint->mDDLTargetTableInfo != NULL )
        {
            smiTrans::removeDDLTargetTableInfo( (smiTrans *)aSmiTrans, sSavepoint->mDDLTargetTableInfo );
            sSavepoint->mDDLTargetTableInfo = NULL;    
            mDDLInfoCount -= 1;
        }

        sNxtSavepoint = sSavepoint->mNxtSavepoint;

        if ( sSavepoint != &mPreparedSP )
        {
            IDE_TEST(mSvpPool.memfree(sSavepoint) != IDE_SUCCESS);
        }
        else
        {
            /* nothing to do */
        }

        sSavepoint = sNxtSavepoint;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*************************************************************************
 * Description: aSavepointName�� �ش��ϴ� Explicit Savepoint��
 *              Transaction�� Explicit Savepoint List���� ã�´�. ����
 *              aDoRemove�� ID_TRUE�̸� Savepoint List���� �����Ѵ�.
 *              ã�� Explicit Savepoint�� Return�Ѵ�. ������ NULL
 *
 * aSavepointName - [IN] Savepoint �̸�.
 * aDoRemove      - [IN] aSavepointName�� �ش��ϴ� Savepoint�� Explicit SVP
 *                       List���� ���Ÿ� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE
 * ***********************************************************************/
smxSavepoint* smxSavepointMgr::findInExpSvpList(const SChar *aSavepointName,
                                                idBool       aDoRemove)
{
    smxSavepoint *sCurSavepoint = NULL;
    smxSavepoint *sPrvSavepoint = NULL;

    sPrvSavepoint = mExpSavepoint.mPrvSavepoint;

    while(sPrvSavepoint != &mExpSavepoint)
    {
        if(idlOS::strcmp(aSavepointName, sPrvSavepoint->mName) == 0)
        {
            if(aDoRemove == ID_TRUE)
            {
                /* Explicit Savepoint List���� �����Ѵ�. */
                removeFromLst(sPrvSavepoint);
            }

            break;
        }

        sCurSavepoint = sPrvSavepoint;
        sPrvSavepoint = sCurSavepoint->mPrvSavepoint;
    }

    if(sPrvSavepoint == &mExpSavepoint)
    {
        sPrvSavepoint = NULL;
    }

    return sPrvSavepoint;
}

/*************************************************************************
 * Description: aSavepointName�� �ش��ϴ� Explicit Savepoint�� ���� ���θ�
 *              �����Ѵ�.
 *
 * aSavepointName - [IN] Savepoint �̸�.
 * ***********************************************************************/
idBool smxSavepointMgr::isExistExpSavepoint(const SChar *aSavepointName)
{
    /* BUG-48489 */
    return ( findInExpSvpList( aSavepointName, ID_FALSE ) != NULL ) ? ID_TRUE : ID_FALSE;
}

/*************************************************************************
 * Description: aStmtDepth�� �ش��ϴ� Implicit Savepoint��
 *              Transaction�� Implicit Savepoint List���� ã�´�.
 *              ã�� Implicit Savepoint�� Return�Ѵ�. ������ NULL
 *
 * aStmtDepth - [IN] Savepoint�� �����ɶ��� Statement�� Depth.
 * ***********************************************************************/
smxSavepoint* smxSavepointMgr::findInImpSvpList(UInt aStmtDepth)
{
    smxSavepoint *sCurSavepoint = NULL;
    smxSavepoint *sPrvSavepoint = NULL;

    sPrvSavepoint = mImpSavepoint.mPrvSavepoint;

    while(sPrvSavepoint != &mImpSavepoint)
    {
        if( sPrvSavepoint->mStmtDepth == aStmtDepth)
        {
            break;
        }

        sCurSavepoint = sPrvSavepoint;
        sPrvSavepoint = sCurSavepoint->mPrvSavepoint;
    }

    if(sPrvSavepoint == &mImpSavepoint)
    {
        sPrvSavepoint = NULL;
    }

    return sPrvSavepoint;
}

/*************************************************************************
 * Description: aStmtDepth �� ���� ū ���� ��ȯ�Ѵ�.
 * ***********************************************************************/
UInt smxSavepointMgr::getStmtDepthFromImpSvpList()
{
    smxSavepoint *sCurSavepoint = NULL;
    smxSavepoint *sPrvSavepoint = NULL;
    UInt          sStmtDepth    = 0;

    sPrvSavepoint = mImpSavepoint.mPrvSavepoint;

    while(sPrvSavepoint != &mImpSavepoint)
    {
        if( sPrvSavepoint->mStmtDepth > sStmtDepth)
        {
            sStmtDepth = sPrvSavepoint->mStmtDepth;
        }

        sCurSavepoint = sPrvSavepoint;
        sPrvSavepoint = sCurSavepoint->mPrvSavepoint;
    }

    return sStmtDepth;
}

IDE_RC smxSavepointMgr::setExpSavepoint( smxTrans    * aTrans,
                                         const SChar * aSavepointName,
                                         smOID         aOldTableOID,
                                         UInt          aOldPartOIDCount,
                                         smOID       * aOldPartOIDArray,
                                         smOID         aNewTableOID,
                                         UInt          aNewPartOIDCount,
                                         smOID       * aNewPartOIDArray,
                                         smxOIDNode  * aOIDNode,
                                         smLSN       * aLSN,
                                         svrLSN        aVolLSN,
                                         ULong         aLockSequence )

{
    smxSavepoint          * sSavepoint;
    smiDDLTargetTableInfo * sDDLTargetTableInfo = NULL;

    sSavepoint = findInExpSvpList( aSavepointName, ID_TRUE );

    if( sSavepoint == NULL )
    {
        //alloc new savepoint
        /* smxSavepointMgr_setExpSavepoint_alloc_Savepoint.tc */
        IDU_FIT_POINT("smxSavepointMgr::setExpSavepoint::alloc::Savepoint");
        IDE_TEST( alloc( &sSavepoint ) != IDE_SUCCESS );

        if ( idlOS::strcmp( aSavepointName, SM_DDL_INFO_SAVEPOINT ) != 0 )
        {
            sSavepoint->mDDLTargetTableInfo = NULL;
            idlOS::strncpy( sSavepoint->mName, 
                            aSavepointName,
                            SMX_MAX_SVPNAME_SIZE );
        }
        else
        {
            IDE_TEST( smiTrans::backupDDLTargetTableInfo( (smiTrans*)aTrans->mSmiTransPtr,
                                                          aOldTableOID,
                                                          aOldPartOIDCount,
                                                          aOldPartOIDArray,
                                                          aNewTableOID,
                                                          aNewPartOIDCount,
                                                          aNewPartOIDArray,
                                                          &sDDLTargetTableInfo ) 
                      != IDE_SUCCESS );
            sSavepoint->mDDLTargetTableInfo = sDDLTargetTableInfo;

            mDDLInfoCount += 1;
            idlOS::snprintf( sSavepoint->mName, SMX_MAX_SVPNAME_SIZE,
                             "%s_%"ID_UINT32_FMT"\0",
                             aSavepointName,
                             mDDLInfoCount );
        }
    }

    addToSvpListTail( &mExpSavepoint, sSavepoint );

    sSavepoint->mUndoLSN      = *aLSN;
    sSavepoint->mVolatileLSN  = aVolLSN;
    sSavepoint->mOIDNode      = aOIDNode;
    sSavepoint->mOffset       = aOIDNode->mOIDCnt;
    sSavepoint->mLockSequence = aLockSequence;
    //PROJ-1608 recovery From Replication (recovery�� ���� svp �α� ���)
    if( (aTrans->mLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
        (aTrans->mLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) )
    {
        IDE_TEST( smrLogMgr::writeSetSvpLog( NULL, /* idvSQL* */
                                             aTrans, aSavepointName )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sDDLTargetTableInfo != NULL )
    {
        smiTrans::removeDDLTargetTableInfo( (smiTrans*)aTrans->mSmiTransPtr, sDDLTargetTableInfo );
        sDDLTargetTableInfo = NULL;
    }

    return IDE_FAILURE;
}

void smxSavepointMgr::updateOIDList(smxTrans     *aTrans,
                                    smxSavepoint *aSavepoint)
{
    smxOIDNode   *sCurOIDNode;
    UInt          i;
    smxOIDInfo   *sArrOIDList;
    UInt          sOffset;
    smxOIDNode   *sOIDNodeListHead;

    sCurOIDNode  = aSavepoint->mOIDNode;
    sOffset      = aSavepoint->mOffset;
    sOIDNodeListHead = &(aTrans->mOIDList->mOIDNodeListHead);

    IDE_ASSERT( sCurOIDNode != NULL );

    if(sCurOIDNode == sOIDNodeListHead)
    {
        sCurOIDNode = sOIDNodeListHead->mNxtNode;
    }

    while(sCurOIDNode != sOIDNodeListHead)
    {
        sArrOIDList = sCurOIDNode->mArrOIDInfo;

        for(i = sOffset; i < sCurOIDNode->mOIDCnt; i++)
        {
            /*
              - BUG-14127
              Savepoint�� Partial Rollback�� �ѹ� ó���� OID��
              Flag���� �ι� ó������ �ʵ��� ó��.
            */
            if((sArrOIDList[i].mFlag & SM_OID_ACT_SAVEPOINT)
               == SM_OID_ACT_SAVEPOINT)
            {
                /*
                   SM_OID_ACT_SAVEPOINT���� clear��Ų��. �̴�
                   �ٽ� ���� savepoint �������� rollback�� �ٽ�
                   �� routine���� ������ ���� �����ϱ� �����̴�.
                */
                sArrOIDList[i].mFlag &=
                    smxOidSavePointMask[sArrOIDList[i].mFlag
                         & SM_OID_OP_MASK].mAndMask;

                sArrOIDList[i].mFlag |=
                    smxOidSavePointMask[sArrOIDList[i].mFlag
                         & SM_OID_OP_MASK].mOrMask;
            }
        }

        sCurOIDNode = sCurOIDNode->mNxtNode;
        sOffset = 0;
    }


    /* BUG-32737 [sm-transation] The ager may ignore aging job that made
     * from the PSM failure.
     * PSM Abort�϶��� Aging�� �� �ֵ���, setAgingFlag �� */
    // To Fix BUG-14126
    // Insert OID�� ���� ���, need Aging�� True�� �����ؾ�  Ager�� �޸�
    aTrans->mOIDList->setAgingFlag( ID_TRUE );

}

IDE_RC smxSavepointMgr::abortToExpSavepoint(smxTrans    *aTrans,
                                            const SChar *aSavepointName)
{
    smxSavepoint *sCurSavepoint;
    smxSavepoint *sNxtSavepoint;

    sCurSavepoint = findInExpSvpList(aSavepointName, ID_FALSE);

    IDE_TEST_RAISE(sCurSavepoint == NULL, err_savepoint_NotFound);

    updateOIDList(aTrans, sCurSavepoint);

    // PROJ-1553 Replication self-deadlock
    // abort to savepoint�� undo�۾��� �ϱ� ���� abortToSavepoint log��
    // ���� �ﵵ�� �ٲ۴�.
    // abortToSavepoint log�� replication������ ����ϱ� ������ ������ ����.
    // PROJ-1608 recovery From Replication (recovery�� ���� svp �α� ���)
    if( (aTrans->mLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
        (aTrans->mLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) )
    {
        IDE_TEST(smrLogMgr::writeAbortSvpLog(NULL, /* idvSQL* */
                                             aTrans, aSavepointName)
                 != IDE_SUCCESS);
    }

    //abort Transaction...
    IDE_TEST(smrRecoveryMgr::undoTrans( aTrans->mStatistics,
                                        aTrans,
                                        &sCurSavepoint->mUndoLSN)
             != IDE_SUCCESS);

    /* PROJ-1594 Volatile TBS */
    /* Volatile TBS�� ���ؼ��� undo�� �����Ѵ�. */
    IDE_TEST(svrRecoveryMgr::undoTrans( &aTrans->mVolatileLogEnv,
                                        sCurSavepoint->mVolatileLSN )
             != IDE_SUCCESS);

    // BUG-22576
    // partial abort�ϱ� ���� private page list�� table��
    // �޾ƾ� �Ѵ�.
    IDE_TEST(aTrans->addPrivatePageListToTableOnPartialAbort()
             != IDE_SUCCESS);

    if ( mDDLInfoCount > 0 )
    {
        rollbackDDLTargetTableInfo( aSavepointName );
    }

    IDE_TEST( smLayerCallback::partialItemUnlock( aTrans->mSlotN,
                                                  sCurSavepoint->mLockSequence,
                                                  ID_FALSE )
              != IDE_SUCCESS );

    sCurSavepoint = sCurSavepoint->mNxtSavepoint;

    while(sCurSavepoint != &mExpSavepoint)
    {
        sNxtSavepoint = sCurSavepoint->mNxtSavepoint;

        freeMem(sCurSavepoint);

        sCurSavepoint = sNxtSavepoint;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_savepoint_NotFound);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundSavepoint));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * Description: Implicit savepoint�� �����մϴ�. Explicit�� �޸� Savepoint
 *              ������ �����ϱ� ���� ������ �޸𸮸� �������� �ʰ�
 *              Statement�� �ִ� Savepoint������ �̿��մϴ�.
 *
 *              1: Savepoint�� �ʱ�ȭ �Ѵ�.
 *              2: Implicit Savepoint List�� �߰��Ѵ�.
 *
 * aSavepoint    - [IN] Savepoint�� �����ɶ��� Statement�� Depth.
 * aStmtDepth    - [IN] Statment Depth
 * aOIDNode      - [IN] Implicit Savepoint�� ���۽��� OID List�� ������ Node
 * aLSN          - [IN] Implicit Savepoint�� �����ɶ� Transaction�� �����
 *                      ������ Log LSN
 * aLockSequence - [IN] Transction�� Lock�� ��� ���� ���������� ����� Lock
 *                      Sequence
 * ***********************************************************************/
IDE_RC smxSavepointMgr::setImpSavepoint( smxSavepoint ** aSavepoint,
                                         UInt            aStmtDepth,
                                         smxOIDNode    * aOIDNode,
                                         smLSN         * aLSN,
                                         svrLSN          aVolLSN,
                                         ULong           aLockSequence )
{
    smxSavepoint *sNewSavepoint;

    IDE_ASSERT( aSavepoint != NULL );

    /* Implicit Savepoint�� update statement�� begin�� �����ǰ�
     * End�� �����ȴ�. ������ �ߺ��� statment�� ����Ʈ�� �̹�
     * �����ϴ� ���� ����. */
    IDE_DASSERT( findInImpSvpList( aStmtDepth ) == NULL );

    /* smxSavepointMgr_setImpSavepoint_alloc_NewSavepoint.tc */
    IDU_FIT_POINT("smxSavepointMgr::setImpSavepoint::alloc::NewSavepoint");
    IDE_TEST( alloc( &sNewSavepoint ) != IDE_SUCCESS );

    sNewSavepoint->mUndoLSN       = *aLSN;
    sNewSavepoint->mOIDNode       = aOIDNode;
    sNewSavepoint->mOffset        = aOIDNode->mOIDCnt;
    sNewSavepoint->mLockSequence  = aLockSequence;
    sNewSavepoint->mDDLTargetTableInfo = NULL;

    /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
     * Partial Rollback�� �����ؾ� �մϴ�. */
    sNewSavepoint->mStmtDepth   = aStmtDepth;

    sNewSavepoint->mReplImpSvpStmtDepth
        = SMI_STATEMENT_DEPTH_NULL;

    /* PROJ-1594 Volatile TBS */
    sNewSavepoint->mVolatileLSN = aVolLSN;

    addToSvpListTail( &mImpSavepoint, sNewSavepoint );

    *aSavepoint = sNewSavepoint;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * Description: Implicit Savepoint�αװ� ������ ���� ó������ Replication
 *              ��� �α�(Sender�� ������ �� �α�)�� �ش� Transaction�ǿ���
 *              ��ϵ� ��� ȣ��ȴ�. Implicit Savepoint�� mReplImpSvpStmtDepth
 *              �� ���� Implicit Savepoint�� ���� ū mReplImpSvpStmtDepth + 1�� �ؼ�
 *              ����Ѵ�. �׸��� ���� Savepoint�� mReplImpSvpStmtDepth��
 *              SMI_STATEMENT_DEPTH_NULL�̸� �ڽ��� mReplImpSvpStmtDepth��
 *              �����ϰ� �ٲ۴�. �������� �̵��ϸ鼭 SMI_STATEMENT_DEPTH_NULL
 *              �� �ƴҶ����� �ݺ��Ѵ�.
 *
 *              �̿� ���� �ڼ��� ������ smxDef.h�� smxSavepoint�� �����Ͻñ� �ٶ��ϴ�.
 *
 * aSavepoint   - [IN] Replication Log�� ����� Statement�� Implicit Savepoint.
 *                     ���� ���� Transaction�� ���������� ������ Savepoint
 * ***********************************************************************/
void smxSavepointMgr::setReplSavepoint( smxSavepoint *aSavepoint )
{
    smxSavepoint *sCurSavepoint;

    sCurSavepoint = aSavepoint->mPrvSavepoint;

    /* ���� SVP�߿��� ���� ū mReplImpSvpStmtDepth�� ã�´�. */
    while( sCurSavepoint != &mImpSavepoint )
    {
        if( sCurSavepoint->mReplImpSvpStmtDepth
            != SMI_STATEMENT_DEPTH_NULL )
        {
            break;
        }

        sCurSavepoint = sCurSavepoint->mPrvSavepoint;
    }

    aSavepoint->mReplImpSvpStmtDepth =
        sCurSavepoint->mReplImpSvpStmtDepth + 1;

    sCurSavepoint = sCurSavepoint->mNxtSavepoint;

    while( sCurSavepoint != aSavepoint )
    {
        IDE_ASSERT( sCurSavepoint->mReplImpSvpStmtDepth ==
                    SMI_STATEMENT_DEPTH_NULL );
        sCurSavepoint->mReplImpSvpStmtDepth =
            aSavepoint->mReplImpSvpStmtDepth;

        sCurSavepoint = sCurSavepoint->mNxtSavepoint;
    }
}

/*************************************************************************
 * Description: Implicit Savepoint�� Implicit Savepoint List���� �����Ѵ�.
 *
 * aSavepoint   - [IN] ������ Implicit Savepoint.
 * ***********************************************************************/
IDE_RC smxSavepointMgr::unsetImpSavepoint( smxSavepoint* aSavepoint )
{
    IDE_TEST( freeMem( aSavepoint ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * Description: Implicit Savepoint���� Partial Rollback�Ѵ�.
 *
 * aTrans     - [IN] Transaction Pointer
 * aSavepoint - [IN] Partial Rollback�� Implicit Savepoint
 *************************************************************************/
IDE_RC smxSavepointMgr::abortToImpSavepoint( smxTrans*     aTrans,
                                             smxSavepoint* aSavepoint )
{
    // Abort Transaction...
    updateOIDList( aTrans, aSavepoint );

    /*
       PROJ-1553 Replication self-deadlock
       abort to savepoint�� undo�۾��� �ϱ� ���� abortToSavepoint log��
       ���� �ﵵ�� �ٲ۴�.
       abortToSavepoint log�� replication������ ����ϱ� ������ ������ ����.
    */
    //PROJ-1608 recovery From Replication (recovery�� ���� svp �α� ���)
    if( ( ( aTrans->mLogTypeFlag == SMR_LOG_TYPE_NORMAL ) ||
          ( aTrans->mLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY ) ) &&
        /* Replication Implicit SVP Log�� ��ϵ����� �ִ�. */
        ( aSavepoint->mReplImpSvpStmtDepth != SMI_STATEMENT_DEPTH_NULL ) ) // case 3.
    {
        /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
         * Partial Rollback�� �����ؾ� �մϴ�.

           Implicit SVP Name: SMR_IMPLICIT_SVP_NAME +
                              Savepoint�� mReplImpSvpStmtDepth�� */
        idlOS::snprintf( aSavepoint->mName,
                         SMX_MAX_SVPNAME_SIZE,
                        "%s%d",
                         SMR_IMPLICIT_SVP_NAME,
                         aSavepoint->mReplImpSvpStmtDepth );

        IDE_TEST( smrLogMgr::writeAbortSvpLog( NULL, /* idvSQL* */
                                               aTrans,
                                               aSavepoint->mName )
                  != IDE_SUCCESS );
    }

    IDE_TEST( smrRecoveryMgr::undoTrans( aTrans->mStatistics,
                                         aTrans,
                                         &(aSavepoint->mUndoLSN) )
              != IDE_SUCCESS );

    /* PROJ-1594 Volatile TBS */
    /* Volatile TBS�� ���ؼ��� undo�� �����Ѵ�. */
    IDE_TEST(svrRecoveryMgr::undoTrans(&aTrans->mVolatileLogEnv,
                                       aSavepoint->mVolatileLSN)
             != IDE_SUCCESS);

    // BUG-22576
    // partial abort�ϱ� ���� private page list�� table��
    // �޾ƾ� �Ѵ�.
    IDE_TEST(aTrans->addPrivatePageListToTableOnPartialAbort()
             != IDE_SUCCESS);

    IDE_TEST( aTrans->executePendingList(ID_FALSE) != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::partialItemUnlock( aTrans->mSlotN,
                                                  aSavepoint->mLockSequence,
                                                  ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� Transaction���� ���������� Begin�� Normal Statment��
 *               Replication�� ���� Savepoint�� �����Ǿ����� Check�ϰ�
 *               ������ �ȵǾ� �ִٸ� Replication�� ���ؼ� Savepoint��
 *               �����Ѵ�. �����Ǿ� ������ ID_TRUE, else ID_FALSE
 *
 ***********************************************************************/
idBool smxSavepointMgr::checkAndSetImpSVP4Repl()
{
    smxSavepoint *sCurSavepoint;
    idBool        sSetImpSVP = ID_TRUE;

    /* ���������� Begin�� Savepoint�� Implicit Svp List����
     * �����´�.*/
    sCurSavepoint = mImpSavepoint.mPrvSavepoint;

    /* ������ Statment���� Update�� �����ϱ� ������ ������ Savepoint
       �� �����ϸ� �ȴ� */
    IDE_ASSERT( sCurSavepoint != NULL );

    if( sCurSavepoint->mReplImpSvpStmtDepth == SMI_STATEMENT_DEPTH_NULL )
    {
        /* Replication�� ���ؼ� Implicit SVP�� ������ ���� ���⶧����
         * ID_FALSE */
        sSetImpSVP = ID_FALSE;

        /*  SMI_STATEMENT_DEPTH_NULL�� �ƴϸ� Implici SVP���ķ�
         *  Savepoint�� ������ ���̴�. */
        setReplSavepoint( sCurSavepoint );
    }

    return sSetImpSVP;
}

void smxSavepointMgr::reservePsmSvp( smxOIDNode  *aOIDNode,
                                     smLSN       *aLSN,
                                     svrLSN       aVolLSN,
                                     ULong        aLockSequence,
                                     idBool       aIsShard )
{
    mPsmSavepoint.mUndoLSN      = *aLSN;
    mPsmSavepoint.mVolatileLSN  = aVolLSN;
    mPsmSavepoint.mOIDNode      = aOIDNode;
    mPsmSavepoint.mOffset       = aOIDNode->mOIDCnt;
    mPsmSavepoint.mLockSequence = aLockSequence;

    mReservePsmSvp = ID_TRUE;

    // TASK-7244 PSM partial rollback in Sharding
    if ( aIsShard == ID_TRUE )
    {
        mReserveShardPsmSvp = ID_TRUE;
    }
}

IDE_RC smxSavepointMgr::writePsmSvp( smxTrans* aTrans )
{
    IDE_DASSERT( mReservePsmSvp == ID_TRUE );
    //PROJ-1608 recovery From Replication (recovery�� ���� svp �α� ���)
    if( (aTrans->mLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
        (aTrans->mLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) )
    {
        IDE_TEST(smrLogMgr::writeSetSvpLog( NULL, /* idvSQL* */
                                            aTrans,
                                            SMX_PSM_SAVEPOINT_NAME )
                 != IDE_SUCCESS);
    }

    mReservePsmSvp = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smxSavepointMgr::abortToPsmSvp(smxTrans* aTrans)
{
    /* BUG-45106 reservePsmSvp�� �������� �ʰ� �ش� �Լ��� ���� ���
                 ����Ʈ Ž�� �� ���� ������ ���� �����Ƿ� ���� ó�� �ؾ� �Ѵ�. */
    if ( mPsmSavepoint.mOIDNode == NULL )
    {   
        ideLog::log( IDE_SM_0, "Invalid PsmSavepoint" );
        IDE_ASSERT( 0 );
    }   
    else
    {   
        /* do nothing... */
    }   

    updateOIDList(aTrans, &mPsmSavepoint);

    // PROJ-1553 Replication self-deadlock
    // abort to savepoint�� undo�۾��� �ϱ� ���� abortToSavepoint log��
    // ���� �ﵵ�� �ٲ۴�.
    // abortToSavepoint log�� replication������ ����ϱ� ������ ������ ����.
    // PROJ-1608 recovery From Replication (recovery�� ���� svp �α� ���)
    if( ( (aTrans->mLogTypeFlag == SMR_LOG_TYPE_NORMAL) ||
          (aTrans->mLogTypeFlag == SMR_LOG_TYPE_REPL_RECOVERY) ) &&
        (mReservePsmSvp == ID_FALSE) )
    {
        IDE_TEST(smrLogMgr::writeAbortSvpLog( NULL, /* idvSQL* */
                                              aTrans,
                                              SMX_PSM_SAVEPOINT_NAME )
                 != IDE_SUCCESS);
    }

    IDE_TEST(smrRecoveryMgr::undoTrans(aTrans->mStatistics,
                                       aTrans,
                                       &mPsmSavepoint.mUndoLSN)
             != IDE_SUCCESS);

    /* PROJ-1594 Volatile TBS */
    /* Volatile TBS�� ���ؼ��� undo�� �����Ѵ�. */
    IDE_TEST(svrRecoveryMgr::undoTrans(&aTrans->mVolatileLogEnv,
                                       mPsmSavepoint.mVolatileLSN)
             != IDE_SUCCESS);

    // BUG-22576
    // partial abort�ϱ� ���� private page list�� table��
    // �޾ƾ� �Ѵ�.
    IDE_TEST(aTrans->addPrivatePageListToTableOnPartialAbort()
             != IDE_SUCCESS);

    IDE_TEST( smLayerCallback::partialItemUnlock( aTrans->mSlotN,
                                                  mPsmSavepoint.mLockSequence,
                                                  ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*************************************************************************
 * Description: Implicit Savepoint���� ��� ���̺� ���� IS���� �����ϰ�,
 *              temp table�ϰ��� Ư���� IX���� �����Ѵ�.
 *
 * aTrans     - [IN] Savepoint����
 * aLockSlot  - [IN] Lock Slot Pointer
 * ***********************************************************************/
IDE_RC smxSavepointMgr::unlockSeveralLock( smxTrans *aTrans,
                                           ULong     aLockSequence )
{
    return smLayerCallback::partialItemUnlock( aTrans->mSlotN,
                                               aLockSequence,
                                               ID_TRUE/*Unlock Several Lock*/ );
}

void smxSavepointMgr::rollbackDDLTargetTableInfo( const SChar * aSavepointName )
{
    smxSavepoint *sSavepoint;
    smxSavepoint *sPrvSavepoint;

    //free save point buffer
    sSavepoint = mExpSavepoint.mPrvSavepoint;

    if ( mDDLInfoCount > 0 )
    {
        while ( sSavepoint != &mExpSavepoint )
        {
            if ( sSavepoint->mDDLTargetTableInfo != NULL )
            {
                if ( sSavepoint->mDDLTargetTableInfo->mOldTableInfo != NULL )
                {
                    smiTrans::restoreDDLTargetOldTableInfo( sSavepoint->mDDLTargetTableInfo );
                }
                else if ( sSavepoint->mDDLTargetTableInfo->mNewTableInfo != NULL )
                {
                    smiTrans::destroyDDLTargetNewTableInfo( sSavepoint->mDDLTargetTableInfo );
                }
                else
                {
                    /* nothing to do */
                }

                sSavepoint->mDDLTargetTableInfo = NULL;
                mDDLInfoCount -= 1;
            }

            if ( aSavepointName != NULL )
            {
                if ( idlOS::strcmp( aSavepointName, sSavepoint->mName ) == 0)
                {
                    break;
                }
            }

            sPrvSavepoint = sSavepoint->mPrvSavepoint;
            sSavepoint = sPrvSavepoint;
        }
    }
}
