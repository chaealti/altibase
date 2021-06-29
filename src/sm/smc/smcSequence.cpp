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
 * $Id: smcSequence.cpp 88753 2020-10-05 00:19:19Z khkwak $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smp.h>
#include <smcSequence.h>
#include <smcReq.h>
#include <smx.h>

IDE_RC smcSequence::createSequence( void*             aTrans,
                                    SLong             aStartSequence,
                                    SLong             aIncSequence,
                                    SLong             aSyncInterval,
                                    SLong             aMaxSequence,
                                    SLong             aMinSequence,
                                    UInt              aFlag,
                                    smcTableHeader**  aTable )
{

    smcTableHeader  * sHeader;
    smOID             sFixOid;
    smcTableHeader    sHeaderArg;
    UInt              sState = 0;
    smcSequenceInfo   sSequence;
    scPageID          sHeaderPageID = 0;
    SChar           * sNewFixRowPtr;
    smSCN             sInfiniteSCN;
    scPageID          sPageID;
    smiSegAttr        sSegmentAttr;
    smiSegStorageAttr sSegmentStoAttr;
    smTID             sTID;

    sTID = smxTrans::getTransID( aTrans );

    // BUG-37607 Transaction ID should be recorded in the slot containing table header.
    SM_SET_SCN_INFINITE_AND_TID( &sInfiniteSCN, sTID );
    
    /* ----------------------------
     * [1] Catalog Table�� ���Ͽ� IX lock ��û
     * ---------------------------*/
    IDE_TEST( smLayerCallback::lockTableModeIX( aTrans,
                                                SMC_TABLE_LOCK( SMC_CAT_TABLE ) )
              != IDE_SUCCESS );

    /* ----------------------------
     * [2] ���ο� Table�� ���� Table Header������
     * Catalog Table���� �Ҵ�޴´�.
     * ---------------------------*/
    IDE_TEST( smpFixedPageList::allocSlot( aTrans,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           NULL,
                                           SMC_CAT_TABLE->mSelfOID,
                                           &(SMC_CAT_TABLE->mFixed.mMRDB),
                                           &sNewFixRowPtr,
                                           sInfiniteSCN,
                                           SMC_CAT_TABLE->mMaxRow,
                                           SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT |
                                           SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER)
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sFixOid = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );
    
    /* ----------------------------
     * [3] ���ο� Table�� ���� Table Header������
     * Catalog Table���� �Ҵ�޾����Ƿ�,
     * New Version List�� �߰��Ѵ�.
     * �� �Լ��� [2] ����.
     * ---------------------------*/
    sHeaderPageID = SM_MAKE_PID(sFixOid);
    sState        = 1;
    sHeader       = (smcTableHeader*)(sNewFixRowPtr + ID_SIZEOF(smpSlotHeader));
    
    sSequence.mCurSequence     = SMC_INIT_SEQUENCE;
    sSequence.mStartSequence   = aStartSequence;
    sSequence.mIncSequence     = aIncSequence;
    sSequence.mSyncInterval    = aSyncInterval;
    sSequence.mMinSequence     = aMinSequence;
    sSequence.mMaxSequence     = aMaxSequence;
    sSequence.mLstSyncSequence = aStartSequence;
    sSequence.mFlag            = aFlag;

    /* ������� �ʴ� �Ӽ��� �ʱ�ȭ�� ���ش�. */
    idlOS::memset( &sSegmentAttr, 0x00, ID_SIZEOF(smiSegAttr));
    idlOS::memset( &sSegmentStoAttr, 0x00, ID_SIZEOF(smiSegStorageAttr));
    
    /* stack ������ table header �� �ʱ�ȭ */
    IDE_TEST( smcTable::initTableHeader( aTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA,
                                         0,
                                         0,
                                         0,
                                         sFixOid,
                                         SMI_TABLE_REPLICATION_DISABLE |
                                            SMI_TABLE_META |
                                            SMI_TABLE_LOCK_ESCALATION_DISABLE,
                                         &sSequence,
                                         SMC_TABLE_SEQUENCE,
                                         SMI_OBJECT_NONE,
                                         ID_ULONG_MAX,
                                         sSegmentAttr,
                                         sSegmentStoAttr,
                                         0, // parallel degree
                                         &sHeaderArg )
              != IDE_SUCCESS );

    /* ���� table header ���� ���� */
    idlOS::memcpy( sHeader, &sHeaderArg, ID_SIZEOF(smcTableHeader));

    IDE_TEST( smcTable::initLockAndRuntimeItem( sHeader ) != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       sFixOid,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_NEW_INSERT_FIXED_SLOT )
              != IDE_SUCCESS );

    //insert dirty page into dirty page list
    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sHeaderPageID) != IDE_SUCCESS);
    
    * aTable = sHeader;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if(sState == 1)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sHeaderPageID);
        IDE_POP();
    }

    return IDE_FAILURE;
    
}

IDE_RC smcSequence::readSequenceCurr( smcTableHeader * aTableHeader,
                                      SLong          * aValue )
{

    smcSequenceInfo *sSequence;
    
    sSequence = &(aTableHeader->mSequence);

    IDE_TEST_RAISE(sSequence->mCurSequence == (SLong)SMC_INIT_SEQUENCE,
                   err_invalide_sequence);
    
    *aValue = sSequence->mCurSequence;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalide_sequence);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SequenceNotInitialized));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}
/*
 * for internal use, this function sets the current value of sequence to aValue
 */
IDE_RC smcSequence::setSequenceCurr( void*            aTrans,
                                     smcTableHeader * aTableHeader,
                                     SLong            aValue )
{
    smcSequenceInfo   *sCurSequence;
    smcSequenceInfo    sAfterSequence;
    SLong              sIncInterval;
    UInt               sState = 0;
    scPageID           sPageID = 0;
    UInt               sCircular;

    sCurSequence = &(aTableHeader->mSequence);
    sCircular    = sCurSequence->mFlag & SMI_SEQUENCE_CIRCULAR_MASK;
    sAfterSequence = *sCurSequence;

    /*the sequence table cannot use this function*/
    IDE_TEST_RAISE( ( sCurSequence->mFlag & SMI_SEQUENCE_TABLE_MASK ) == SMI_SEQUENCE_TABLE_TRUE, ERR_INTERNAL ); 

    if ( sAfterSequence.mSyncInterval == 0 )
    {
        sIncInterval = sAfterSequence.mIncSequence;
    }
    else
    {
        sIncInterval = sAfterSequence.mIncSequence * sAfterSequence.mSyncInterval;
    }

    IDE_TEST_RAISE( sCurSequence->mCurSequence == (SLong)SMC_INIT_SEQUENCE, ERR_INVALIDE_SEQUENCE );
    IDE_TEST_RAISE( ( aValue > sCurSequence->mMaxSequence  ) || 
                    ( aValue <  sCurSequence->mMinSequence ), 
                    ERR_INVALIDE_PARAM );

    /* set current sequence value */
    sAfterSequence.mCurSequence = aValue ;

    if ( sCurSequence->mIncSequence > 0 )
    {
        /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
        if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
        {
            sAfterSequence.mLstSyncSequence =
                sAfterSequence.mCurSequence + sIncInterval;
        }
        else
        {
            /* MAX �� �ʰ��ϸ� MIN ���� sync */
            /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
            if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
            {
                sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
            }
            else
            {
                sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
            }
        }
    }
    else
    {
        /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
        if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
        {
            sAfterSequence.mLstSyncSequence =
                sAfterSequence.mCurSequence + sIncInterval;
        }
        else
        {
            /* MIN �̸��� �Ǹ� MAX �� sync */
            /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
            if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
            {
                sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
            }
            else
            {
                sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
            }
        }
    }

    sPageID = SM_MAKE_PID( aTableHeader->mSelfOID );
    sState = 1;

    IDE_TEST( smrUpdate::updateSequenceAtTableHead(
                    NULL, /* idvSQL* */
                    aTrans,
                    aTableHeader->mSelfOID,
                    SM_MAKE_OFFSET( aTableHeader->mSelfOID )
                    + SMP_SLOT_HEADER_SIZE,
                    NULL,
                    &sAfterSequence,
                    ID_SIZEOF(smcSequenceInfo) )
                != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID )
                != IDE_SUCCESS );

    aTableHeader->mSequence = sAfterSequence;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALIDE_SEQUENCE );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_SequenceNotInitialized ) );
    }
    IDE_EXCEPTION( ERR_INTERNAL );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
        ideLog::log( IDE_SM_0, "set current operation about sequence table is not allowed" );
    }
    IDE_EXCEPTION( ERR_INVALIDE_PARAM );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
        ideLog::log( IDE_SM_0, "set current sequence values is invalid: %"ID_INT64_FMT, aValue );
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID );
        IDE_POP();
    }
    else
    {
        /*do nothing*/
    }
    return IDE_FAILURE;
}

IDE_RC smcSequence::readSequenceNext( void*                aTrans,
                                      smcTableHeader*      aTableHeader,
                                      SLong*               aValue,
                                      smiSeqTableCallBack* aCallBack )
{
    smcSequenceInfo*   sCurSequence;
    smcSequenceInfo    sAfterSequence;
    SLong              sIncInterval;
    UInt               sState = 0;
    scPageID           sPageID = 0;
    UInt               sCircular;
    UInt               sSeqTable;
    idBool             sNeedSync;
    UInt               sScale;
    
    sCurSequence = &(aTableHeader->mSequence);
    sCircular    = sCurSequence->mFlag & SMI_SEQUENCE_CIRCULAR_MASK;
    sSeqTable    = sCurSequence->mFlag & SMI_SEQUENCE_TABLE_MASK;
    sScale       = sCurSequence->mFlag & SMI_SEQUENCE_SCALE_MASK;

    /* ����ġ�� 0�� sequence(constant sequence)�� SKIP */
    IDE_TEST_CONT( sCurSequence->mIncSequence == 0, SKIP_INC_SEQUENCE );

    sNeedSync      = ID_FALSE;
    sAfterSequence = *sCurSequence;

    /* BUG-31094 Sequence�� NEXTVAL�� MAX����ŭ ������ �Ŀ���
     * server restart�� ������ �ʱ�ȭ�˴ϴ�.
     *
     * nocache sequence�� sync interval�� 0���� �����Ǳ� ������,
     * cache 1�� �����ؼ� �α��ؾ� ��
     */
    if ( sAfterSequence.mSyncInterval == 0 )
    {
        sIncInterval = sAfterSequence.mIncSequence;
    }
    else
    {
        sIncInterval = sAfterSequence.mIncSequence * sAfterSequence.mSyncInterval;
    }
    
    if ( sCurSequence->mIncSequence > 0 )
    {
        if (sCurSequence->mCurSequence == (SLong)SMC_INIT_SEQUENCE)
        {
            sNeedSync = ID_TRUE;
            
            /* PROJ-2365 nextval�� sequence table���� ��´�.
             * ����) selectCurrVal�� updateLastVal ���̿� ���ܰ� �߻��ؼ��� �ȵȴ�.
             */
            if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
            {
                IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                    aCallBack->info )
                          != IDE_SUCCESS );
            }
            else
            {
                sAfterSequence.mCurSequence = sCurSequence->mStartSequence;
            }
            
            /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
            if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
            {
                sAfterSequence.mLstSyncSequence =
                    sAfterSequence.mCurSequence + sIncInterval;
            }
            else
            {
                /* MAX �� �ʰ��ϸ� MIN ���� sync */
                /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                {
                    sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                }
                else
                {
                    sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                }
            }
            
            if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
            {
                IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                    aCallBack->info )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* BUG-31094 Sequence�� NEXTVAL�� MAX����ŭ ������ �Ŀ���
             * server restart�� ������ �ʱ�ȭ�˴ϴ�.
             *
             * ����ġ�� ����� �� sequence�� sync�ؾ� �ϴ� ���� ������ ����.
             * ù°, ������ sync�ߴ� ������ current���� ũ�ų� ���� ���� ��
             * ��°, MAX������ Ŀ�� ��� MIN + SYNC interval�� �����ϰ� sync    */
            if ( sCurSequence->mCurSequence + sCurSequence->mIncSequence >
                 sCurSequence->mMaxSequence )
            {
                IDE_TEST_RAISE( sCircular == SMI_SEQUENCE_CIRCULAR_DISABLE,
                                err_max_sequence );
                
                /* MAX������ Ŀ�� ��� MIN���� �����ϰ� sync */
                sNeedSync = ID_TRUE;
                
                /* PROJ-2365 nextval�� sequence table���� ��´�. */
                if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                {
                    IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                        aCallBack->info )
                              != IDE_SUCCESS );
                }
                else
                {
                    sAfterSequence.mCurSequence = sCurSequence->mMinSequence;
                }
                
                /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
                if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
                {
                    sAfterSequence.mLstSyncSequence =
                        sAfterSequence.mCurSequence + sIncInterval;
                }
                else
                {
                    /* MAX �� �ʰ��ϸ� MIN ���� sync */
                    /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                    if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                    {
                        sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                    }
                    else
                    {
                        sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                    }
                }
            
                if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                {
                    IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                        aCallBack->info )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do. */
                }
            }
            else
            {
                /* server restart�� ��� */
                if ( sAfterSequence.mCurSequence == sAfterSequence.mLstSyncSequence )
                {
                    sNeedSync = ID_TRUE;
                    
                    /* PROJ-2365 nextval�� sequence table���� ��´�. */
                    if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                    {
                        IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                            aCallBack->info )
                                  != IDE_SUCCESS );
                        
                        sAfterSequence.mCurSequence += sAfterSequence.mIncSequence;
                    }
                    else
                    {
                        sAfterSequence.mCurSequence = sAfterSequence.mLstSyncSequence +
                            sAfterSequence.mIncSequence;
                    }
                    
                    /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
                    if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
                    {
                        sAfterSequence.mLstSyncSequence =
                            sAfterSequence.mCurSequence + sIncInterval;
                    }
                    else
                    {
                        /* MAX �� �ʰ��ϸ� MIN ���� sync */
                        /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                        if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                        {
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                        }
                        else
                        {
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                        }
                    }
            
                    if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                    {
                        IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                            aCallBack->info )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
                else
                {
                    sAfterSequence.mCurSequence += sAfterSequence.mIncSequence;
                    
                    /* MAX�� �ʰ��Ͽ� MIN�� �� ���� skip */
                    if ( ( sAfterSequence.mCurSequence >= sAfterSequence.mLstSyncSequence ) &&
                         ( sCurSequence->mMinSequence != sAfterSequence.mLstSyncSequence ) )
                    {
                        sNeedSync = ID_TRUE;
                        
                        /* PROJ-2365 nextval�� sequence table���� ��´�. */
                        if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                        {
                            IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                                aCallBack->info )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            sAfterSequence.mCurSequence = sAfterSequence.mLstSyncSequence;
                        }

                        if ( sCurSequence->mMinSequence == sAfterSequence.mCurSequence )
                        {
                            /* CYCLE�� �߻��� ��� */
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                        }
                        else
                        {
                            /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
                            if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
                            {
                                sAfterSequence.mLstSyncSequence =
                                    sAfterSequence.mCurSequence + sIncInterval;
                            }
                            else
                            {
                                /* MAX �� �ʰ��ϸ� MIN ���� sync */
                                /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                                if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                                {
                                    sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                                }
                                else
                                {
                                    sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                                }
                            }
                        }
            
                        if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                        {
                            IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                                aCallBack->info )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Nothing to do. */
                        }
                        
                        /* CYCLE�� �߻��� ��� */
                        IDE_TEST_RAISE( ( sCurSequence->mMinSequence == sAfterSequence.mCurSequence ) &&
                                        ( sCircular == SMI_SEQUENCE_CIRCULAR_DISABLE ),
                                        err_max_sequence );
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
            }
        }
    }
    else /* if ( sCurSequence->mIncSequence < 0 ) */
    {
        if (sCurSequence->mCurSequence == (SLong)SMC_INIT_SEQUENCE)
        {
            sNeedSync = ID_TRUE;
            
            /* PROJ-2365 nextval�� sequence table���� ��´�. */
            if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
            {
                IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                    aCallBack->info )
                          != IDE_SUCCESS );
            }
            else
            {
                sAfterSequence.mCurSequence = sCurSequence->mStartSequence;
            }
            
            /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
            if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
            {
                sAfterSequence.mLstSyncSequence =
                    sAfterSequence.mCurSequence + sIncInterval;
            }
            else
            {
                /* MIN �̸��� �Ǹ� MAX �� sync */
                /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                {
                    sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                }
                else
                {
                    sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                }
            }
            
            if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
            {
                IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                    aCallBack->info )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* BUG-31094 Sequence�� NEXTVAL�� MAX����ŭ ������ �Ŀ���
             * server restart�� ������ �ʱ�ȭ�˴ϴ�.
             *
             * ����ġ�� ������ �� sequence�� sync�ؾ� �ϴ� ���� ������ ����.
             * ù°, ������ sync�ߴ� ������ current���� �۰ų� ���� ���� ��
             * ��°, MIN������ �۾��� ��� MAX + SYNC interval�� �����ϰ� sync    */
            if ( sCurSequence->mCurSequence + sCurSequence->mIncSequence <
                 sCurSequence->mMinSequence )
            {
                IDE_TEST_RAISE( sCircular == SMI_SEQUENCE_CIRCULAR_DISABLE,
                                err_min_sequence );

                /* MIN������ �۾����� ��� MAX���� �����ϰ� SYNC */
                sNeedSync = ID_TRUE;
                
                /* PROJ-2365 nextval�� sequence table���� ��´�. */
                if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                {
                    IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                        aCallBack->info )
                              != IDE_SUCCESS );
                }
                else
                {
                    sAfterSequence.mCurSequence = sCurSequence->mMaxSequence;
                }
                
                /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
                if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
                {
                    sAfterSequence.mLstSyncSequence =
                        sAfterSequence.mCurSequence + sIncInterval;
                }
                else
                {
                    /* MIN �̸��� �Ǹ� MAX �� sync */
                    /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                    if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                    {
                        sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                    }
                    else
                    {
                        sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                    }
                }
            
                if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                {
                    IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                        aCallBack->info )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do. */
                }
            }
            else
            {
                /* server restart�� ��� */
                if ( sAfterSequence.mCurSequence == sAfterSequence.mLstSyncSequence )
                {
                    sNeedSync = ID_TRUE;
                    
                    /* PROJ-2365 nextval�� sequence table���� ��´�. */
                    if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                    {
                        IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                            aCallBack->info )
                                  != IDE_SUCCESS );
                        
                        sAfterSequence.mCurSequence += sAfterSequence.mIncSequence;
                    }
                    else
                    {
                        sAfterSequence.mCurSequence = sAfterSequence.mLstSyncSequence +
                            sAfterSequence.mIncSequence;
                    }
                    
                    /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
                    if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
                    {
                        sAfterSequence.mLstSyncSequence =
                            sAfterSequence.mCurSequence + sIncInterval;
                    }
                    else
                    {
                        /* MIN �̸��� �Ǹ� MAX �� sync */
                        /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                        if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                        {
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                        }
                        else
                        {
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                        }
                    }
            
                    if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                    {
                        IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                            aCallBack->info )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
                else
                {
                    sAfterSequence.mCurSequence += sAfterSequence.mIncSequence;

                    /* MIN �̸��� �Ǿ� MAX�� �� ���� skip */
                    if ( ( sAfterSequence.mCurSequence <= sAfterSequence.mLstSyncSequence ) &&
                         ( sCurSequence->mMaxSequence != sAfterSequence.mLstSyncSequence ) )
                    {
                        sNeedSync = ID_TRUE;

                        /* PROJ-2365 nextval�� sequence table���� ��´�. */
                        if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                        {
                            IDE_TEST( aCallBack->selectCurrVal( & sAfterSequence.mCurSequence,
                                                                aCallBack->info )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            sAfterSequence.mCurSequence = sAfterSequence.mLstSyncSequence;
                        }
                        
                        if ( sCurSequence->mMaxSequence == sAfterSequence.mCurSequence )
                        {
                            /* CYCLE�� �߻��� ��� */
                            sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                        }
                        else
                        {
                            /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
                            if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
                            {
                                sAfterSequence.mLstSyncSequence =
                                    sAfterSequence.mCurSequence + sIncInterval;
                            }
                            else
                            {
                                /* MIN �̸��� �Ǹ� MAX �� sync */
                                /* BUG-37864 no cycle�� ��쿡�� mLstSyncSequence�� cycle���� �ʽ��ϴ�. */
                                if ( sCircular == SMI_SEQUENCE_CIRCULAR_ENABLE )
                                {
                                    sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
                                }
                                else
                                {
                                    sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
                                }
                            }
                        }
            
                        if ( sSeqTable == SMI_SEQUENCE_TABLE_TRUE )
                        {
                            IDE_TEST( aCallBack->updateLastVal( sAfterSequence.mLstSyncSequence,
                                                                aCallBack->info )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Nothing to do. */
                        }
                        
                        /* CYCLE�� �߻��� ��� */
                        IDE_TEST_RAISE( ( sCurSequence->mMaxSequence == sAfterSequence.mCurSequence ) &&
                                        ( sCircular == SMI_SEQUENCE_CIRCULAR_DISABLE ),
                                        err_min_sequence );
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
            }
        }
    }

    if ( sNeedSync == ID_TRUE )
    {
        sPageID = SM_MAKE_PID(aTableHeader->mSelfOID);
        sState = 1;
        
        IDE_TEST(smrUpdate::updateSequenceAtTableHead(
                     NULL, /* idvSQL* */
                     aTrans,
                     aTableHeader->mSelfOID,
                     SM_MAKE_OFFSET(aTableHeader->mSelfOID)
                     + SMP_SLOT_HEADER_SIZE,
                     NULL,
                     &sAfterSequence,
                     ID_SIZEOF(smcSequenceInfo))
                 != IDE_SUCCESS);
        
        
        aTableHeader->mSequence = sAfterSequence;
            
        sState = 0;
        IDE_TEST(smmDirtyPageMgr::insDirtyPage(
                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID)
                 != IDE_SUCCESS);
    }
    else
    {
        sCurSequence->mCurSequence = sAfterSequence.mCurSequence;
    }

    IDE_EXCEPTION_CONT( SKIP_INC_SEQUENCE );

    *aValue = sCurSequence->mCurSequence;

    /* TASK-7217 Sharded sequence */
    if ( aCallBack != NULL )
    {
        aCallBack->scale = sScale;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_max_sequence);
    IDE_SET(ideSetErrorCode(smERR_ABORT_SequenceReachMaxValue));

    IDE_EXCEPTION(err_min_sequence);
    IDE_SET(ideSetErrorCode(smERR_ABORT_SequenceReachMinValue));
        
    IDE_EXCEPTION_END;

    if(sState == 1)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID);
        IDE_POP();
    }

    return IDE_FAILURE;
    
}

IDE_RC smcSequence::alterSequence( void                * aTrans,
                                   smcTableHeader      * aTableHeader,
                                   SLong                 aIncSequence,
                                   SLong                 aSyncInterval,
                                   SLong                 aMaxSequence,
                                   SLong                 aMinSequence,
                                   UInt                  aFlag,
                                   idBool                aIsRestart,
                                   SLong                 aStartSequence,
                                   SLong               * aLastSyncSeq )
{
    smcSequenceInfo *sCurSequence;
    smcSequenceInfo  sAfterSequence;
    SLong            sIncInterval;
    UInt             sState  = 0;
    scPageID         sPageID = 0;

    IDE_TEST( smcTable::validateTable(
                  aTrans, 
                  aTableHeader, 
                  SCT_VAL_DDL_DML, // ���̺����̽� Validation �ɼ�
                  SMC_LOCK_MUTEX )
              != IDE_SUCCESS );
    
    sCurSequence  = &(aTableHeader->mSequence);
    sAfterSequence = *sCurSequence;

    sAfterSequence.mIncSequence      = aIncSequence;
    sAfterSequence.mSyncInterval     = aSyncInterval;
    sAfterSequence.mMaxSequence      = aMaxSequence;
    sAfterSequence.mMinSequence      = aMinSequence;
    sAfterSequence.mFlag             = aFlag;

    /* TASK-7217 Sharded sequence */
    if ( aIsRestart == ID_TRUE )
    {
        sAfterSequence.mStartSequence = aStartSequence;
        sAfterSequence.mCurSequence   = (SLong)SMC_INIT_SEQUENCE;
    }

    if ( sAfterSequence.mCurSequence == (SLong)SMC_INIT_SEQUENCE )
    {
        sAfterSequence.mLstSyncSequence = sAfterSequence.mStartSequence;
    }
    else
    {
        /* nocache sequence�� sync interval�� 0���� �����Ǳ� ������,
         * cache 1�� �����ؼ� �α��ؾ� ��
         */
        if ( sAfterSequence.mSyncInterval == 0 )
        {
            sIncInterval = sAfterSequence.mIncSequence;
        }
        else
        {
            sIncInterval = sAfterSequence.mIncSequence * sAfterSequence.mSyncInterval;
        }
        
        if ( sAfterSequence.mIncSequence > 0 )
        {
            /* ����ġ * SYNC interval ���� MAX�� �ʰ��ϴ��� �Ǵ� */
            if ( sIncInterval <= sCurSequence->mMaxSequence - sAfterSequence.mCurSequence )
            {
                sAfterSequence.mLstSyncSequence =
                    sAfterSequence.mCurSequence + sIncInterval;
            }
            else
            {
                /* MAX �� �ʰ��ϸ� MIN ���� sync */
                sAfterSequence.mLstSyncSequence = sCurSequence->mMinSequence;
            }
        }
        else /* if ( sAfterSequence.mIncSequence < 0 ) */
        {
            /* ����ġ * SYNC interval ���� MIN ���� �۾������� �Ǵ� */
            if ( sIncInterval >= sCurSequence->mMinSequence - sAfterSequence.mCurSequence )
            {
                sAfterSequence.mLstSyncSequence =
                    sAfterSequence.mCurSequence + sIncInterval;
            }
            else
            {
                /* MIN �̸��� �Ǹ� MAX �� sync */
                sAfterSequence.mLstSyncSequence = sCurSequence->mMaxSequence;
            }
        }
    }

    /* last sync seq�� ��ȯ�Ѵ�. */
    *aLastSyncSeq = sAfterSequence.mLstSyncSequence;
    
    sPageID = SM_MAKE_PID(aTableHeader->mSelfOID);
    sState = 1;
    
    IDE_TEST(smrUpdate::updateSequenceAtTableHead(
                 NULL, /* idvSQL* */
                 aTrans,
                 aTableHeader->mSelfOID,
                 SM_MAKE_OFFSET(aTableHeader->mSelfOID)
                 + SMP_SLOT_HEADER_SIZE,
                 sCurSequence,
                 &sAfterSequence,
                 ID_SIZEOF(smcSequenceInfo))
             != IDE_SUCCESS);

    
    aTableHeader->mSequence = sAfterSequence;

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sPageID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState == 1)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID);
        IDE_POP();
    }

    return IDE_FAILURE;
    
}

IDE_RC smcSequence::resetSequence( void                * aTrans,
                                   smcTableHeader      * aTableHeader )
{
    smcSequenceInfo * sCurSequence;
    smcSequenceInfo   sAfterSequence;
    UInt              sState  = 0;
    scPageID          sPageID = 0;

    IDE_TEST( smcTable::validateTable( aTrans, 
                                       aTableHeader, 
                                       SCT_VAL_DDL_DML, // ���̺����̽� Validation �ɼ�
                                       SMC_LOCK_MUTEX )
              != IDE_SUCCESS );
    
    sCurSequence   = &(aTableHeader->mSequence);
    sAfterSequence = *sCurSequence;

    sAfterSequence.mCurSequence     = SMC_INIT_SEQUENCE;
    sAfterSequence.mLstSyncSequence = sCurSequence->mStartSequence;

    sPageID = SM_MAKE_PID( aTableHeader->mSelfOID );
    sState = 1;
    
    IDE_TEST( smrUpdate::updateSequenceAtTableHead( NULL, /* idvSQL* */
                                                    aTrans,
                                                    aTableHeader->mSelfOID,
                                                    SM_MAKE_OFFSET(aTableHeader->mSelfOID)
                                                    + SMP_SLOT_HEADER_SIZE,
                                                    sCurSequence,
                                                    &sAfterSequence,
                                                    ID_SIZEOF(smcSequenceInfo))
              != IDE_SUCCESS );
    
    aTableHeader->mSequence = sAfterSequence;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                             sPageID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID );
        IDE_POP();
    }

    return IDE_FAILURE;
}

IDE_RC smcSequence::refineSequence( smcTableHeader * aTableHeader )
{

    smcSequenceInfo *sSequence;
    
    sSequence = &(aTableHeader->mSequence);

    IDE_TEST_RAISE(sSequence->mCurSequence == (SLong)SMC_INIT_SEQUENCE,
                   err_invalide_sequence);
    
    /* BUG-37874 the functionality to flush a cache of sequence is required.
     * sequence�� server start�� refine�ܰ迡�� sequence�� cur value��
     * last sync value�� ����ȴ�. �̷��� �Ǹ� server�� re-start�Ǿ�����
     * �����ϰ� �ٽ� ä���� �ϰ� �ȴ�. �ٽ� ä���� �����ϱ� ���� sequence�� 
     * ���� refine�� �����ϴ� �Լ��� �߰��Ѵ�. */
    sSequence->mCurSequence = sSequence->mLstSyncSequence;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalide_sequence);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_SequenceNotInitialized));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
