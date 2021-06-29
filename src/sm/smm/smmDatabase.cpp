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
* $Id:$
**********************************************************************/


#include <idl.h>
#include <idm.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smu.h>
#include <smm.h>
#include <smmReq.h>

/* ------------------------------------------------
 * [] global variable
 * ----------------------------------------------*/
smmMemBase         smmDatabase::mMemBaseBackup;
smmMemBase        *smmDatabase::mDicMemBase        = NULL;
smSCN              smmDatabase::mLstSystemSCN;

iduMutex           smmDatabase::mMtxSCN;

IDE_RC smmDatabase::initialize()
{
    mDicMemBase               = NULL;

    SM_INIT_SCN(&mLstSystemSCN);

    IDE_TEST(mMtxSCN.initialize( (SChar*)"SMM_SCN_MUTEX",
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC smmDatabase::destroy()
{
    /* BUG-33675 mDicMemBase pointer should be freed after destroying Tablespace */
    mDicMemBase                 = NULL;
    IDE_TEST(mMtxSCN.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}


/* �����ͺ��̽� ������ membase�� �ʱ�ȭ�Ѵ�.
 * aDBName           [IN] �����ͺ��̽� �̸�
 * aDbFilePageCount  [IN] �ϳ��� �����ͺ��̽� ������ ������ Page��
 * aChunkPageCount   [IN] �ϳ��� Expand Chunk�� Page��
 * aDBCharSet        [IN] �����ͺ��̽� ĳ���� ��(PROJ-1579 NCHAR)
 * aNationalCharSet  [IN] ���ų� ĳ���� ��(PROJ-1579 NCHAR)
 */
IDE_RC smmDatabase::initializeMembase( smmTBSNode * aTBSNode,
                                       SChar      * aDBName,
                                       vULong       aDbFilePageCount,
                                       vULong       aChunkPageCount,
                                       SChar      * aDBCharSet,
                                       SChar      * aNationalCharSet )
{
    UInt           i;
    PDL_Time_Value sTimeValue;
    
    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    IDE_DASSERT( aDBName != NULL );
    IDE_DASSERT( aDBName[0] != '\0' );
    IDE_DASSERT( aDbFilePageCount > 0 );
    IDE_DASSERT( aChunkPageCount > 0 );

    // PROJ-1579 NCHAR
    IDE_DASSERT( aDBCharSet != NULL );
    IDE_DASSERT( aDBCharSet[0] != '\0' );
    IDE_DASSERT( aNationalCharSet != NULL );
    IDE_DASSERT( aNationalCharSet[0] != '\0' );

    IDE_ASSERT( idlOS::strlen( aDBName ) < SM_MAX_DB_NAME );
    IDE_ASSERT( idlOS::strlen( iduGetSystemInfoString() )
                < IDU_SYSTEM_INFO_LENGTH );

    idlOS::strncpy(aTBSNode->mMemBase->mDBname,
                   aDBName,
                   SM_MAX_DB_NAME - 1 );
    aTBSNode->mMemBase->mDBname[ SM_MAX_DB_NAME - 1 ] = '\0';

    idlOS::strncpy(aTBSNode->mMemBase->mProductSignature,
                   iduGetSystemInfoString(),
                   IDU_SYSTEM_INFO_LENGTH - 1 );
    aTBSNode->mMemBase->mProductSignature[ IDU_SYSTEM_INFO_LENGTH - 1 ] = '\0';

    smuMakeUniqueDBString(aTBSNode->mMemBase->mDBFileSignature);

    aTBSNode->mMemBase->mVersionID          = smVersionID;
    aTBSNode->mMemBase->mCompileBit         = iduCompileBit;
    aTBSNode->mMemBase->mBigEndian          = iduBigEndian;
    aTBSNode->mMemBase->mLogSize            = smuProperty::getLogFileSize();
    aTBSNode->mMemBase->mDBFilePageCount    = aDbFilePageCount;
    aTBSNode->mMemBase->mTxTBLSize          = smuProperty::getTransTblSize();

    SM_INIT_SCN( &(aTBSNode->mMemBase->mSystemSCN) );

    // PROJ-1579 NCHAR
    idlOS::strncpy(aTBSNode->mMemBase->mDBCharSet,
                   aDBCharSet,
                   IDN_MAX_CHAR_SET_LEN - 1 );
    aTBSNode->mMemBase->mDBCharSet[ IDN_MAX_CHAR_SET_LEN - 1 ] = '\0';

    // PROJ-1579 NCHAR
    idlOS::strncpy(aTBSNode->mMemBase->mNationalCharSet,
                   aNationalCharSet,
                   IDN_MAX_CHAR_SET_LEN - 1 );
    aTBSNode->mMemBase->mNationalCharSet[ IDN_MAX_CHAR_SET_LEN - 1 ] = '\0';


    // BUG-15197 sun 5.10 x86 ���� createdb�� SEGV ���
    sTimeValue = idlOS::gettimeofday();
    aTBSNode->mMemBase->mTimestamp = (struct timeval)sTimeValue;

    for ( i = 0; i < SMM_PINGPONG_COUNT; i++ )
    {
        aTBSNode->mMemBase->mDBFileCount[i] = 0;
    }

    aTBSNode->mMemBase->mAllocPersPageCount = 0;


    // Expand Chunk���� ���� ����
    aTBSNode->mMemBase->mExpandChunkPageCnt = aChunkPageCount ;
    aTBSNode->mMemBase->mCurrentExpandChunkCnt = 0;

    // Free Page List�� �ʱ�ȭ �Ѵ�.
    for ( i = 0; i< SMM_MAX_FPL_COUNT; i++ )
    {
        aTBSNode->mMemBase->mFreePageLists[ i ].mFirstFreePageID = SM_NULL_PID ;
        aTBSNode->mMemBase->mFreePageLists[ i ].mFreePageCount = 0;
    }

    aTBSNode->mMemBase->mFreePageListCount = SMM_FREE_PAGE_LIST_COUNT;


    return IDE_SUCCESS;
}

/******************************************************************************
 * mDicMemBase->mSystemSCN �� SCN_SYNC_INTERVAL ��ŭ ���� ��Ų��.
 * �������� lockSCNMtx �� ��� ȣ��Ǿ�� �Ѵ�.
 *****************************************************************************/
IDE_RC smmDatabase::setSystemSCN( smSCN * aSystemSCN )
{
    smSCN     sAddedSCN;

    SM_SET_SCN( &sAddedSCN, aSystemSCN );
    SM_ADD_SCN( &sAddedSCN, smuProperty::getSCNSyncInterval()); // 80000

    // logging : SMR_SMM_MEMBASE_SET_SYSTEM_SCN
    IDE_TEST( smLayerCallback::setSystemSCN( sAddedSCN ) != IDE_SUCCESS );

    SM_SET_SCN( &(mDicMemBase->mSystemSCN), &sAddedSCN ); 
    // dead code�� �ǽɵȴ� .... �ٵ� �����ϱ� �ϴ� ����
    SM_SET_SCN( &(mMemBaseBackup.mSystemSCN) , &sAddedSCN );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                SMM_MEMBASE_PAGEID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * �־��� SCN ���� LstSystemSCN�� �����Ѵ�.
 *****************************************************************************/
IDE_RC smmDatabase::setLstSystemSCN( smSCN * aLstSystemSCNPtr, smSCN * aNewLstSystemSCNPtr )
{
    smSCN     sLstSystemSCN;
    smSCN   * sPersSysSCNPtr;
    smSCN     sPrev;
    SInt      sState = 0;

    IDU_FIT_POINT_RAISE( "smmDatabase::setLstSystemSCN::invalidValueOfSystemSCN",
                         err_invaild_SCN );
    IDE_TEST_RAISE( SM_SCN_IS_SYSTEMSCN( (*aLstSystemSCNPtr) ) != ID_TRUE, err_invaild_SCN );
    
    while(1)
    {
        sLstSystemSCN  = getLstSystemSCN();
        
        /* �־��� SCN�� SystemSCN ���� ������ ���� �Ѵ�. */
        if ( SM_SCN_IS_GE( &sLstSystemSCN, aLstSystemSCNPtr ) )
        {
            if ( aNewLstSystemSCNPtr != NULL )
            {
                SM_SET_SCN( aNewLstSystemSCNPtr, &sLstSystemSCN );
            }
            break;           
        }

        sPersSysSCNPtr = getSystemSCN();

        if ( SM_SCN_IS_GT( aLstSystemSCNPtr, sPersSysSCNPtr ) )
        {
            IDE_ASSERT( lockSCNMtx() == IDE_SUCCESS);
            sState = 1;
            /* �׻��̿� getSystemSCN() ���� �ٸ� �����忡 ���� ���������� �ִ�.
               LOCK ��� �ٽ�Ȯ�� */
            sPersSysSCNPtr = getSystemSCN();

            if ( SM_SCN_IS_GT( aLstSystemSCNPtr, sPersSysSCNPtr ) )
            {
                IDE_TEST( setSystemSCN( aLstSystemSCNPtr ) != IDE_SUCCESS );
            }
            sState = 0;
            IDE_ASSERT( unlockSCNMtx() == IDE_SUCCESS );
        }

        sPrev = acpAtomicCas64( &mLstSystemSCN, (*aLstSystemSCNPtr), sLstSystemSCN );

        if ( sPrev == sLstSystemSCN ) /* CAS SUCCESS */
        {
            if ( aNewLstSystemSCNPtr != NULL )
            {
                SM_SET_SCN( aNewLstSystemSCNPtr, aLstSystemSCNPtr );
            }
            break;
        }
#ifdef DEBUG 
        else /* CAS FAILURE */
        {
            /* Cas ������ ������
               1.�ٸ� Ʈ����ǿ����� SystemSCN�� �������� aLstSystemSCNPtr���� ũ�ٸ� ��.
               2.�ٸ� Ʈ������� SystemSCN�� ������������ aLstSystemSCNPtr���� �۴ٸ�
                  aLstSystemSCNPtr ���� �������Ѿ� �Ѵ� 
               3.sPrev �� sLstSystemSCN ���� ������ ����.. */
            IDE_DASSERT_MSG ( sPrev > sLstSystemSCN,
                              "sPrev : %"ID_UINT64_FMT","
                              "sLstSystemSCN :"ID_UINT64_FMT","
                              "aLstSystemSCNPtr :"ID_UINT64_FMT"\n",
                              sPrev,
                              sLstSystemSCN,
                              aLstSystemSCNPtr );
        }
#endif
    } // while

    validateCommitSCN();

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invaild_SCN )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_SCN, (*aLstSystemSCNPtr) ) )
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        (void)unlockSCNMtx();
    }

    return IDE_FAILURE;
}

/*
 * Statement Begin�� View�� �����ϱ� ���� ȣ��Ǵ� �Լ�
 * ���� : 32��Ʈ�� ���ؼ��� 64��Ʈ SCN�� Atomic�ϰ� �б� ���� ���� LSB,MSB�� parity bit��
 *        �̿��ϱ� ���� �ڵ��̴�.
 *
 *       64��Ʈ�� ������ �Ǿ��� ��쿡�� �Ʒ��� �ڵ尡 ��ǻ� �ǹ̰� ������,
 *       32��Ʈ�� ��쿡�� ����� ū �ǹ̸� ������.
 *
 *       if(SM_GET_SCN_HIGHVBIT(a_pSCN) == SM_GET_SCN_LOWVBIT(a_pSCN))
 *       {
 *           break;
 *       }
 */

void smmDatabase::getViewSCN(smSCN *a_pSCN)
{
    while(1)
    {
        SM_GET_SCN(a_pSCN, &mLstSystemSCN);

        if(SM_GET_SCN_HIGHVBIT(a_pSCN) == SM_GET_SCN_LOWVBIT(a_pSCN))
        {
            break;
        }

        idlOS::thr_yield();
    }

    SM_SET_SCN_VIEW_BIT(a_pSCN);
}


/*
 *  Tx�� commit�� ������ SCN���� ���� ����
 *
 *  - Non-Atomic tuple-set reading�� �����ϱ� ���� ������ ���� ������
 *    Tx commit�۾��� �����Ѵ�.
 *
 *    1. Commit SCN�� �Ҵ� �޴´�.
 *
 *    2. Temporary SCN�� Persistent SCN�� �����ϸ�, Persistent SCN + Gap��
 *       �����ϰ�, �α��Ѵ�.
 *
 *    3. ������ SCN(Commit-SCN)�� Temporary SCN���� assign�Ѵ�.
 *
 *    4. callback�� �̿��ؼ� SCN�� ���ڷ� �Ѿ�� Status �������� �Ҵ��ϵ�,
 *       �ݵ�� SCN, Status ������� assign �Ѵ�. (smxTrans.cpp::setTransCommitSCN())
 *       �ֳ��ϸ�, Ư�� Tuple�� Validation �������� �ش� Tuple�� ���� ������
 *       �� �������� ������ ��, non-blocking �˰����� �̿��Ͽ�
 *       Transaction ��ü�� status�� SCN�� �о �̿��ϴµ�
 *       �� ���� Status -> SCN�� ������� �б� �����̴�.
 *       [ ���� => smxTrans::getTransCommitSCN() ]
 *
 *         TX            SCN   Status
 *       Write �� :   ------------------> (commit ��)
 *       Read  �� :  <------------------  (tuple Validation ��)
 *
 *       % Tx��  status�� commit�̶� tx�� commitSCN�� infinite�ϼ� �ִ�.
 *        Tx�� commit�� end�� �ϰԵǴµ� �̶�  commitSCN�� inifinite��
 *        ���ʱ�ȭ �Ǳ⶧���̴�.
 *
 *    5. ������ Commit-SCN�� Temporary SCN, System SCN�� �ݿ��Ѵ�.
 *
 */


IDE_RC smmDatabase::getCommitSCN( void    * aTrans,
                                  idBool    aIsLegacyTrans,
                                  void    * aStatus )
{

    smSCN     sCommitSCN;
    smSCN     spTempSysSCN;
    smSCN*    spPersSysSCN;

    smSCN     sCasNew;
    smSCN     sCasOld;

#ifdef ALTIBASE_FIT_CHECK
    SInt      sState = 0;
#endif

#if 0
    {
    IDE_ASSERT( lockSCNMtx() == IDE_SUCCESS);
    sState = 1;

    // 1. Get Last Temporary SYSTEM-SCN
    spTempSysSCN = getLstSystemSCN();
    sCommitSCN  = spTempSysSCN;

    // 2. Increase for getting Commit-SCN
    SM_INCREASE_SCN(&sCommitSCN);

    IDE_ASSERT( SM_SCN_IS_SYSTEMSCN(sCommitSCN) == ID_TRUE );

    // 3. Check whether the Temporary SCN overrun the Persistent SCN
    spPersSysSCN = getSystemSCN();

    if( SM_SCN_IS_GT(spTempSysSCN, spPersSysSCN) ||
        SM_SCN_IS_EQ(spTempSysSCN, spPersSysSCN) )
    {
        smSCN     sAddedSCN;

        SM_SET_SCN(&sAddedSCN, spPersSysSCN);

        if(smuProperty::getSCNSyncInterval() > 80000)
        {

            ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                        SM_TRC_MEMORY_SCN_SYNC_INTERVAL_FATAL,
                        smuProperty::getSCNSyncInterval());
            IDE_ASSERT(0);
        }

        SM_ADD_SCN(&sAddedSCN, smuProperty::getSCNSyncInterval())

        if(SM_SCN_IS_GT(getSystemSCN(), &sAddedSCN))
        {
            ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                        SM_TRC_MEMORY_INVALID_SCN,
                        SM_SCN_TO_LONG( *(getSystemSCN()) ),
                        SM_SCN_TO_LONG( sAddedSCN ) );
            IDE_ASSERT(0);
        }

        IDE_TEST( smLayerCallback::setSystemSCN( sAddedSCN ) != IDE_SUCCESS );


        SM_SET_SCN(spPersSysSCN, &sAddedSCN);

        IDE_TEST(smmDirtyPageMgr::insDirtyPage(
                                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                     SMM_MEMBASE_PAGEID)
                 != IDE_SUCCESS);
    }
    IDL_MEM_BARRIER;
    }
#endif

    /* BUG-47367 Tx�� commitSCN�� �����ϱ� �� ���¸� Status�� ǥ���� �ش�.
     * �ش� ǥ�ð� �Ǿ� �ִ� Tx�� ��� checkSCN �������� getTransCommitSCN �Լ��� ����
     * �������� commitSCN�� �����Ǳ⸦ ��ٷȴ� ���� �о�� �ȴ�. */
    if( aTrans != NULL )
    {
        smLayerCallback::setTransStatus( aTrans,
                                         SMX_TX_PRECOMMIT );
    }

    /* BUG-47367 �׻� LstSystemSCN < PersSystemSCN �̾���Ѵ�. */
    while(1)
    {
        spTempSysSCN = getLstSystemSCN();
        spPersSysSCN = getSystemSCN();

        if ( SM_SCN_IS_GE( &spTempSysSCN, spPersSysSCN ) )
        {
            IDE_ASSERT( lockSCNMtx() == IDE_SUCCESS);
#ifdef ALTIBASE_FIT_CHECK
            sState = 1;
#endif
            /* �׻��̿� getSystemSCN() ���� �ٸ� �����忡 ���� ���������� �ִ�.
               LOCK ��� �ٽ�Ȯ�� */

            spTempSysSCN = getLstSystemSCN();
            spPersSysSCN = getSystemSCN();

            if ( SM_SCN_IS_GE( &spTempSysSCN, spPersSysSCN ) )
            {
                setSystemSCN( spPersSysSCN );
            }
#ifdef ALTIBASE_FIT_CHECK
            sState = 0;
#endif
            IDE_ASSERT( unlockSCNMtx() == IDE_SUCCESS );
        }

        /* SM_INCREASE_SCN �� ���� ������ �ö󰡾��Ѵ�. */
        sCasNew = spTempSysSCN + 8;
        sCasOld = acpAtomicCas64( &mLstSystemSCN, sCasNew, spTempSysSCN );

        if ( sCasOld == spTempSysSCN )
        {
            /* CAS SUCCESS */
            sCommitSCN = sCasNew;
            break;
        }
    }

    /* CASE-6985 ���� ������ startup�ϸ� user�� ���̺�,
     * �����Ͱ� ��� �����*/
    validateCommitSCN();

    /* 
     * 4. Callback for strict ordered setting of Tx SCN & Status
     *
     * aTrans == NULL�� ���� System SCN�� ������ ��ų����̴�.
     * Delete Thread���� ���� �ִ� Aging��� OID���� ó���ϱ�����
     * Commit SCN�� ������Ų��.
     *
     * BUG-30911 - 2 rows can be selected during executing index scan
     *             on unique index. 
     *
     * LstSystemSCN�� ���� ������Ű�� Ʈ����ǿ� CommitSCN�� �����ؾ� �Ѵ�.
     * �� �����߾��µ�, BUG-31248 �� ���� �ٽ� ���� �մϴ�.
     */
    if( aTrans != NULL )
    {
        smLayerCallback::setTransSCNnStatus( aTrans, aIsLegacyTrans, &sCommitSCN, aStatus );
    }
    else
    {
        IDE_DASSERT( aTrans == NULL );
        IDE_DASSERT( aStatus == NULL ); 
        IDE_DASSERT( aIsLegacyTrans == ID_FALSE );
    }

    IDU_FIT_POINT( "1.BUG-30911@smmDatabase::getCommitSCN" );

    return IDE_SUCCESS;

#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    if( sState != 0 )
    {
        IDE_ASSERT( unlockSCNMtx() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
#endif
}

/*
 * ���� System Commit Number�� Valid���� �����Ѵ�.
 * 
 * BUG-47367 getCommitSCN���� SCNMtx�� ������ persSystemSCN�� ������Ű�� ������ �����ʿ� ����
 * Lock�� ���� �ʰ� LstSystemSCN�� ���� �����µ� persSystemSCN�� ������ Ȯ���ϴ� ������� �ٲ۴�.
 * 
 */
void smmDatabase::validateCommitSCN()
{
    smSCN sSCN;
    /* CASE-6985 ���� ������ startup�ϸ� user�� ���̺�,
     * �����Ͱ� ��� �����: ���� Membase�� �ִ� SystemSCN��
     * �׻� Transaction���� �Ҵ�Ǵ� m_lstSystemSCN���� �׻�
     * Ŀ�� �Ѵ�. */
    sSCN = getLstSystemSCN();

    if( SM_SCN_IS_GT(&sSCN, getSystemSCN()) )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                    "Invalid System SCN:%llu, \
                    Last SCN:%llu\n",
                    SM_SCN_TO_LONG( *(getSystemSCN()) ),
                    SM_SCN_TO_LONG( sSCN ));
        IDE_ASSERT(SM_SCN_IS_LT(&sSCN, getSystemSCN()));
    }
}

IDE_RC smmDatabase::checkVersion(smmMemBase *aMembase)
{

    UInt s_DbVersion;
    UInt s_SrcVersion;

    UInt sCheckVersion;
    UInt sCheckBit;
    UInt sCheckEndian;
    UInt sCheckLogFileSize;

    IDE_DASSERT( aMembase != NULL );

    /* ------------------------------------------------
     * [0] Read From Properties
     * ----------------------------------------------*/
    sCheckVersion = smuProperty::getCheckStartupVersion();
    sCheckBit = smuProperty::getCheckStartupBitMode();
    sCheckEndian = smuProperty::getCheckStartupEndian();
    sCheckLogFileSize = smuProperty::getCheckStartupLogSize();

    /* ------------------------------------------------
     * [1] Product Version Check
     * ----------------------------------------------*/

    if (sCheckVersion != 0)
    {
        s_DbVersion   = aMembase->mVersionID & SM_CHECK_VERSION_MASK;
        s_SrcVersion  = smVersionID & SM_CHECK_VERSION_MASK;

        IDE_TEST_RAISE (s_DbVersion != s_SrcVersion, version_mismatch_error);
    }

    /* ------------------------------------------------
     * [2] Bit Mode Check 32/64
     * ----------------------------------------------*/
    if (sCheckBit != 0)
    {
        IDE_TEST_RAISE(aMembase->mCompileBit != iduCompileBit,
                       version_mismatch_error);
    }

    /* ------------------------------------------------
     * [3] Endian Check
     * ----------------------------------------------*/
    if (sCheckEndian != 0)
    {
        IDE_TEST_RAISE(aMembase->mBigEndian != iduBigEndian,
                       version_mismatch_error);
    }

    /* ------------------------------------------------
     * [4] Log Size Check
     * ----------------------------------------------*/
    if (sCheckLogFileSize != 0)
    {
        IDE_TEST_RAISE(aMembase->mLogSize != smuProperty::getLogFileSize(),
                       version_mismatch_error);
    }

    /* ------------------------------------------------
     * [5] Transaction Table Size
     * ----------------------------------------------*/
    IDE_TEST_RAISE(aMembase->mTxTBLSize > smuProperty::getTransTblSize(),
                   version_mismatch_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(version_mismatch_error);
    {
        SChar s_diskVer[32];
        UInt  s_diskVersion = aMembase->mVersionID;

        idlOS::memset(s_diskVer, 0, 32);
        idlOS::snprintf(s_diskVer, 32,
                        "%"ID_xINT32_FMT".%"ID_xINT32_FMT".%"ID_xINT32_FMT,
                        ((s_diskVersion & SM_MAJOR_VERSION_MASK) >> 24),
                        ((s_diskVersion & SM_MINOR_VERSION_MASK) >> 16),
                        (s_diskVersion  & SM_PATCH_VERSION_MASK));

        IDE_SET(ideSetErrorCode(smERR_ABORT_BACKUP_DISK_INVALID,
                                s_diskVer,
                                (UInt)aMembase->mCompileBit,
                                (aMembase->mBigEndian == ID_TRUE) ? "BIG" : "LITTLE",
                                (ULong)aMembase->mLogSize,
                                (UInt)aMembase->mTxTBLSize,

                                smVersionString,
                                (UInt)iduCompileBit,
                                (iduBigEndian == ID_TRUE) ? "BIG" : "LITTLE",
                                (ULong)smuProperty::getLogFileSize(),
                                smuProperty::getTransTblSize()));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smmDatabase::checkMembaseIsValid()
{

    IDE_TEST_RAISE(idlOS::strcmp(mMemBaseBackup.mDBname, mDicMemBase->mDBname) != 0,
                   err_invalid_membase);

    IDE_TEST_RAISE(idlOS::strcmp(mMemBaseBackup.mProductSignature,
                                 mDicMemBase->mProductSignature) != 0,
                   err_invalid_membase);
    IDE_TEST_RAISE(idlOS::strcmp(mMemBaseBackup.mDBFileSignature,
                                 mDicMemBase->mDBFileSignature) != 0,
                   err_invalid_membase);

    IDE_TEST_RAISE(mMemBaseBackup.mVersionID != mDicMemBase->mVersionID,
                   err_invalid_membase);
    IDE_TEST_RAISE(mMemBaseBackup.mCompileBit != mDicMemBase->mCompileBit,
                   err_invalid_membase);
    IDE_TEST_RAISE(mMemBaseBackup.mBigEndian != mDicMemBase->mBigEndian,
                   err_invalid_membase);
    IDE_TEST_RAISE(mMemBaseBackup.mLogSize != mDicMemBase->mLogSize,
                   err_invalid_membase);
    IDE_TEST_RAISE(mMemBaseBackup.mDBFilePageCount !=
                   mDicMemBase->mDBFilePageCount,
                   err_invalid_membase);
    IDE_TEST_RAISE(mMemBaseBackup.mTxTBLSize !=
                   mDicMemBase->mTxTBLSize,
                   err_invalid_membase);

    // PROJ-1579 NCHAR
    IDE_TEST_RAISE(idlOS::strcmp(mMemBaseBackup.mDBCharSet, 
                                 mDicMemBase->mDBCharSet) != 0,
                   err_invalid_membase);

    // PROJ-1579 NCHAR
    IDE_TEST_RAISE(idlOS::strcmp(mMemBaseBackup.mNationalCharSet, 
                                 mDicMemBase->mNationalCharSet) != 0,
                   err_invalid_membase);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_membase);
    {
        dumpMembase();
        IDE_SET(ideSetErrorCode(smERR_FATAL_MEMBASE_INVALID));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*
 * Expand Chunk���õ� ������Ƽ ������ ����� �� ������ üũ�Ѵ�.
 *
 * 1. �ϳ��� Chunk���� ������ �������� ���� Free Page List�� �й��� ��,
 *    �ּ��� �ѹ��� �й谡 �Ǵ��� üũ
 *
 *    �������� : Chunk�� �������������� >= 2 * List�� �й��� Page�� * List ��
 *
 * aChunkDataPageCount [IN] Expand Chunk���� ������������ ��
 *                          ( FLI Page�� ������ Page�� �� )
 */
IDE_RC smmDatabase::checkExpandChunkProps(smmMemBase * aMemBase)
{
    IDE_DASSERT( aMemBase != NULL );

    // ����ȭ�� Free Page List����  createdb������ �ٸ� ���
    IDE_TEST_RAISE(aMemBase->mFreePageListCount !=
                   SMM_FREE_PAGE_LIST_COUNT,
                   different_page_list_count );

    // Expand Chunk���� Page���� createdb������ �ٸ� ���
    IDE_TEST_RAISE(aMemBase->mExpandChunkPageCnt !=
                   smuProperty::getExpandChunkPageCount() ,
                   different_expand_chunk_page_count );

    //  Expand Chunk�� �߰��� ��
    //  ( �����ͺ��̽� Chunk�� ���� �Ҵ�� ��  )
    //  �� ���� Free Page���� �������� ����ȭ�� Free Page List�� �й�ȴ�.
    //
    //  �� ��, ������ Free Page List�� �ּ��� �ϳ��� Free Page��
    //  �й�Ǿ�� �ϵ��� �ý����� ��Ű���İ� ����Ǿ� �ִ�.
    //
    //  ���� Expand Chunk���� Free Page���� ������� �ʾƼ�,
    //  PER_LIST_DIST_PAGE_COUNT ���� ��� Free Page List�� �й��� ����
    //  ���ٸ� ������ �߻���Ų��.
    //
    //  Expand Chunk���� Free List Info Page�� ������
    //  ���������������� Free Page List�� �й�ǹǷ�, �� ������ üũ�ؾ� �Ѵ�.
    //  �׷���, �̷��� ��� ������ �Ϲ� ����ڰ� �����ϱ⿡�� �ʹ� �����ϴ�.
    //
    //  Expand Chunk�� ������ ���� Free Page List�� �ι��� �й��� �� ������ŭ
    //  ����� ũ�⸦ �������� �����Ѵ�.
    //
    //  ���ǽ� : EXPAND_CHUNK_PAGE_COUNT <=
    //           2 * PER_LIST_DIST_PAGE_COUNT * PAGE_LIST_GROUP_COUNT
   IDE_TEST_RAISE(
        aMemBase->mExpandChunkPageCnt
        <
        2 * SMM_PER_LIST_DIST_PAGE_COUNT * aMemBase->mFreePageListCount,
        err_too_many_per_list_page_count );


    return IDE_SUCCESS;

    IDE_EXCEPTION(different_expand_chunk_page_count);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_DIFFERENT_DB_EXPAND_CHUNK_PAGE_COUNT,
                                SMM_FREE_PAGE_LIST_COUNT,
                                aMemBase->mExpandChunkPageCnt ));
    }
    IDE_EXCEPTION(different_page_list_count);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_DIFFERENT_DB_FREE_PAGE_LIST_COUNT,
                                SMM_FREE_PAGE_LIST_COUNT,
                                aMemBase->mFreePageListCount ));
    }
    IDE_EXCEPTION( err_too_many_per_list_page_count );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TOO_MANY_PER_LIST_PAGE_COUNT_ERROR,
                                (ULong) aMemBase->mExpandChunkPageCnt,
                                (ULong) SMM_PER_LIST_DIST_PAGE_COUNT,
                                (ULong) aMemBase->mFreePageListCount ));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;

}


// PROJ-1579 NCHAR
SChar* smmDatabase::getDBCharSet()
{
    SChar* sDBCharSet;

    if ( mDicMemBase != NULL )
    {
        sDBCharSet = mDicMemBase->mDBCharSet;
    }
    else
    {
        sDBCharSet = (SChar*)"";
    }

    return sDBCharSet;
}

SChar* smmDatabase::getNationalCharSet()
{
    SChar* sNationalCharSet;

    if ( mDicMemBase != NULL )
    {
        sNationalCharSet = mDicMemBase->mNationalCharSet;
    }
    else
    {
        sNationalCharSet = (SChar*)"";
    }

    return sNationalCharSet;
}


void smmDatabase::dumpMembase()
{
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE1);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE2, mDicMemBase->mDBname);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE3, mDicMemBase->mProductSignature);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE4, mDicMemBase->mDBFileSignature);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE5, mDicMemBase->mVersionID);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE6, mDicMemBase->mCompileBit);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE7, mDicMemBase->mBigEndian);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE8, mDicMemBase->mLogSize);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE9, mDicMemBase->mDBFilePageCount);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE10, mDicMemBase->mTxTBLSize);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE11, mDicMemBase->mDBCharSet);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE12, mDicMemBase->mNationalCharSet);

    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE13);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE2, mMemBaseBackup.mDBname);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE3, mMemBaseBackup.mProductSignature);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE4, mMemBaseBackup.mDBFileSignature);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE5, mMemBaseBackup.mVersionID);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE6, mMemBaseBackup.mCompileBit);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE7, mMemBaseBackup.mBigEndian);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE8, mMemBaseBackup.mLogSize);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE9, mMemBaseBackup.mDBFilePageCount);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE10, mMemBaseBackup.mTxTBLSize);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE11, mDicMemBase->mDBCharSet);
    ideLog::log(SM_TRC_LOG_LEVEL_MEMORY, SM_TRC_MEMORY_DUMP_MEMBASE12, mDicMemBase->mNationalCharSet);
}

#ifdef DEBUG
/***********************************************************************
 * Description : BUG-31862 resize transaction table without db migration
 *      MemBase�� ������Ƽ�� Ʈ����� ���̺� ����� ���Ѵ�.
 *      ������Ƽ�� ���� MemBase�� ������ Ŀ�� �ϸ�, 2^N ���̾�� �Ѵ�.
 *
 * Implementation :
 *
 * [IN] aMembase - ���̺����̽��� �⺣�̽�
 *
 **********************************************************************/
IDE_RC smmDatabase::checkTransTblSize(smmMemBase * aMemBase)
{
    UInt    sTransTblSize   = smuProperty::getTransTblSize();

    IDE_DASSERT( aMemBase != NULL );
    IDE_DASSERT( sTransTblSize >= aMemBase->mTxTBLSize );
    
    return IDE_SUCCESS;
}
#endif

/***********************************************************************
 * Description : BUG-31862 resize transaction table without db migration
 *      ������Ƽ�� TRANSACTION_TABLE_SIZE ������
 *      mDicMemBase�� mTxTBLSize�� Ȯ���Ѵ�. 
 *      ���� 2^N�� �����ϴ�.
 *
 * Implementation :
 *
 **********************************************************************/
IDE_RC smmDatabase::refineTransTblSize()
{
    UInt    sTransTblSize   = smuProperty::getTransTblSize();

    IDE_DASSERT( checkTransTblSize(mDicMemBase) == IDE_SUCCESS );

    if ( sTransTblSize > mDicMemBase->mTxTBLSize )
    {
        setTxTBLSize(mDicMemBase, sTransTblSize);

        IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                    SMM_MEMBASE_PAGEID)
                  != IDE_SUCCESS);

        makeMembaseBackup();
    }
    else
    {
        /* do nothing */
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
