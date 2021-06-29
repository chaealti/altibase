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
 * $Id: smiStatistics.cpp
 **********************************************************************/
/**************************************************************
 * FILE DESCRIPTION : smiStatistics.cpp                       *
 * -----------------------------------------------------------*
 * TASK-4990 changing the method of collecting index statistics
 * ���� ������� ���� ���
 **************************************************************/

#include <idl.h>
#include <ide.h>
#include <smErrorCode.h>
#include <smi.h>
#include <sml.h>
#include <smc.h>
#include <smn.h>
#include <smx.h>
#include <sgmManager.h>
#include <smrRecoveryMgr.h>

smiStatistics           smiStatistics::mStatThread;
idBool                  smiStatistics::mDone = ID_FALSE;

smiSystemStat           smiStatistics::mSystemStat;
iduMutex                smiStatistics::mStatMutex;
UInt                    smiStatistics::mAtomicA;
UInt                    smiStatistics::mAtomicB;

/******************************************************************
 * DESCRIPTION : ������� �ֱ��� ���� Thread ���۽�Ŵ
 ******************************************************************/
IDE_RC smiStatistics::initializeStatic()
{
    mAtomicA = 0;
    mAtomicB = 0;
    mDone    = ID_FALSE;

    IDE_TEST( mStatMutex.initialize( (SChar*)"GLOBAL_STAT_MUTEX",
                                     IDU_MUTEX_KIND_NATIVE,
                                     IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    idlOS::memset( &mSystemStat, 0x00, ID_SIZEOF(smiSystemStat) );

    IDU_FIT_POINT( "smiStatistics::initializeStatic::mStatThreadStart" );

    IDE_TEST( mStatThread.start() != IDE_SUCCESS );

    IDE_TEST( smmManager::getSystemStatFromMemBase( &mSystemStat ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : ������� �ֱ��� ���� Thread �����Ŵ
 ******************************************************************/
IDE_RC smiStatistics::finalizeStatic()
{
    mDone = ID_TRUE;
    IDE_TEST_RAISE( mStatThread.join() != IDE_SUCCESS,
                    ERR_THREAD_JOIN );

    IDE_TEST( mStatMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_THREAD_JOIN );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_Systhrjoin ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : ������� �ֱ��� ���� Thread ������
 ******************************************************************/
void smiStatistics::run()
{
    idvSQL               sDummyStatistics;
    SChar              * sNxtRowPtr;
    SChar              * sCurRowPtr;
    smpSlotHeader      * sSlotHeader;
    smcTableHeader     * sTable;
    smSCN                sSCN;
    idBool               sDoing = ID_FALSE;
    smxTrans           * sTrans;
    SLong                sCurNumRow;
    SLong                sNumRowGap;
    UInt                 sState = 0;

    IDU_FIT_POINT( "smiStatistics::run::alloc" );
    idlOS::memset( &sDummyStatistics, 0, ID_SIZEOF( idvSQL ) );
    IDE_TEST(smxTransMgr::alloc(&sTrans) != IDE_SUCCESS);

    IDE_EXCEPTION_CONT( RETRY );

    while( mDone == ID_FALSE )
    {
        if( sDoing == ID_FALSE )
        {
            idlOS::sleep( smuProperty::getDBMSStatAutoInterval() );
        }
        else
        {
            /* nothing to do ... */
        }

        if( smuProperty::getDBMSStatAutoInterval() == 0 )
        {
            idlOS::sleep( 10 );
            sDoing = ID_FALSE;
        }
        else
        {
            sDoing = ID_TRUE;

            IDE_TEST( smcRecord::nextOIDall(
                    (smcTableHeader *)smmManager::m_catTableHeader,
                    NULL, 
                    &sNxtRowPtr )
                != IDE_SUCCESS );

            while( sNxtRowPtr != NULL )
            {
                sSlotHeader = (smpSlotHeader *)sNxtRowPtr;
                sTable      = (smcTableHeader *)(sSlotHeader + 1);
                SM_GET_SCN( (smSCN*)&sSCN,
                            (smSCN*)&sSlotHeader->mCreateSCN );

                /* is Active Table? */
                if( ( SM_SCN_IS_INFINITE(sSCN) == ID_FALSE ) &&
                    ( sTable->mType == SMC_TABLE_NORMAL ) &&
                    ( smcTable::isDropedTable(sTable) == ID_FALSE) &&
                    ( sctTableSpaceMgr::hasState( sTable->mSpaceID,
                                                  SCT_SS_INVALID_DISK_TBS ) 
                      == ID_FALSE ) )
                {
                    IDE_TEST( getTableStatNumRow( sNxtRowPtr, // BUG-42323
                                                  ID_TRUE, /* current */
                                                  NULL,    /* aTrans */
                                                  &sCurNumRow )
                              != IDE_SUCCESS );

                    if( sTable->mStat.mNumRow > sCurNumRow )
                    {
                        sNumRowGap = sTable->mStat.mNumRow - sCurNumRow;
                    }
                    else
                    {
                        sNumRowGap = sCurNumRow - sTable->mStat.mNumRow;
                    }
                    IDE_DASSERT( sNumRowGap >= 0 );

                    if( sNumRowGap > ( sTable->mStat.mNumRow *
                        smuProperty::getDBMSStatAutoPercentage() / 100 ) )
                    {
                        IDE_ASSERT( sTrans->begin( NULL,
                                                   ( SMI_TRANSACTION_REPL_NONE |
                                                     SMI_COMMIT_WRITE_NOWAIT ),
                                                   SMX_NOT_REPL_TX_ID )
                                    == IDE_SUCCESS);
                        /* dummy set resource group */
                        smxTrans::setRSGroupID((void*)sTrans, 0);
                        sState = 1;

                        IDE_TEST( gatherTableStatInternal( 
                                                        &sDummyStatistics,
                                                        (void*)sTrans,
                                                        sNxtRowPtr,
                                                        0.0f,     /*Percentage auto */
                                                        1,        /*Degree*/
                                                        NULL,     /*TotalTableArg*/
                                                        ID_FALSE )/*NoInvalidate*/
                                  != IDE_SUCCESS );
                        sDoing = ID_TRUE;

                        sState = 0;
                        IDE_TEST(sTrans->commit() != IDE_SUCCESS);
                    } /* if Gap > threshold*/
                } /* if ActiveTable */

                sCurRowPtr = sNxtRowPtr;

                IDE_TEST( smcRecord::nextOIDall(
                        (smcTableHeader *)smmManager::m_catTableHeader,
                        sCurRowPtr,
                        &sNxtRowPtr )
                    != IDE_SUCCESS );
            } /* while - next table*/
        } /* if - interval==0*/
    } /* while done==false */

    IDE_TEST(smxTransMgr::freeTrans(sTrans) != IDE_SUCCESS);

    return ;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        (void) sTrans->abort( ID_FALSE, /* aIsLegacyTrans */
                              NULL      /* aLegacyTrans */ );
    default:
        break;
    }

    IDE_TEST_CONT( ideGetErrorCode() == smERR_ABORT_Table_Not_Found,
                    RETRY );

    ideLog::log( IDE_SERVER_0, ideGetErrorMsg(ideGetErrorCode()) );
    IDE_ASSERT( 0 );

    return ;
}

/**************************************************************
 * DESCRIPTION : 
 *     System ���� ��������� �����մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 **************************************************************/
IDE_RC smiStatistics::gatherSystemStats( idBool  aSaveMemBase )
{
    UInt            sState = 0;
    smiSystemStat   sSystemStat;

    IDE_TEST( getSystemStatSReadTime( ID_TRUE, /* current */
                                      &sSystemStat.mSReadTime )
              != IDE_SUCCESS );
    IDE_TEST( getSystemStatMReadTime( ID_TRUE, /* current */
                                      &sSystemStat.mMReadTime )
              != IDE_SUCCESS );
    IDE_TEST( getSystemStatDBFileMultiPageReadCount( 
                    ID_TRUE, /* current */
                    &sSystemStat.mDBFileMultiPageReadCount )
              != IDE_SUCCESS );

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    mSystemStat.mCreateTV       = smiGetCurrTime();
    mSystemStat.mSReadTime      = sSystemStat.mSReadTime;
    mSystemStat.mMReadTime      = sSystemStat.mMReadTime;
    mSystemStat.mDBFileMultiPageReadCount =
                                    sSystemStat.mDBFileMultiPageReadCount;

    if( aSaveMemBase == ID_TRUE )
    {
        IDE_TEST( smmManager::setSystemStatToMemBase( &mSystemStat ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    ideLog::log( IDE_SM_0, 
                 "====================================================\n"
                 "GATHER_SYSTEM_STAT:\n"
                 "SReadTime                : %"ID_DOUBLE_G_FMT"\n"
                 "MReadTime                : %"ID_DOUBLE_G_FMT"\n"
                 "DBFileMultiPageReadCount : %"ID_INT64_FMT"\n"
                 "HashTime                 : %"ID_DOUBLE_G_FMT"\n"
                 "CompareTime              : %"ID_DOUBLE_G_FMT"\n",
                 mSystemStat.mSReadTime,                /* SDOUBLE */
                 mSystemStat.mMReadTime,                /* SDOUBLE */
                 mSystemStat.mDBFileMultiPageReadCount, /* SLONG */
                 mSystemStat.mHashTime,                 /* SDOUBLE */
                 mSystemStat.mCompareTime );            /* SDOUBLE */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     �� Table�� ��������� �����մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aTable               - [IN]  ������� ������ Table
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::gatherTableStats( idvSQL * aStatistics,
                                        void   * aSmiTrans,
                                        void   * aTable,
                                        SFloat   aPercentage, 
                                        SInt     aDegree,
                                        void   * aTotalTableArg, 
                                        SChar    aNoInvalidate )
{
    return gatherTableStatInternal( aStatistics,
                                    ((smiTrans*)aSmiTrans)->mTrans,
                                    aTable,
                                    aPercentage, 
                                    aDegree, 
                                    aTotalTableArg,
                                    aNoInvalidate );
}

/**************************************************************
 * DESCRIPTION : 
 *     �� Table�� ��������� �����մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aSmxTrans            - [IN]  Transaction
 *  aTable               - [IN]  ������� ������ Table
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::gatherTableStatInternal( idvSQL   * aStatistics,
                                               void     * aSmxTrans,
                                               void     * aTable,
                                               SFloat     aPercentage, 
                                               SInt       aDegree, 
                                               void     * aTotalTableArg, 
                                               SChar      aNoInvalidate )
{
    smnIndexModule     * sFullScanModule;
    smcTableHeader     * sTableHeader;
    void               * sIndexHeader;
    SLong                sNumPage       = 0;
    SFloat               sPercentage    = 0.0;
    SInt                 sDegree        = 0;
    UInt                 sIndexCount    = 0;
    UInt                 sState = 0;
    UInt                 i      = 0;

    sTableHeader = (smcTableHeader*)
        ( ( (UChar*)aTable ) + SMP_SLOT_HEADER_SIZE );

    /****************************** Lock ****************************/
    IDE_TEST( lock4GatherStat( aSmxTrans,
                               sTableHeader,
                               ID_TRUE )
              != IDE_SUCCESS );
    sState = 1;

    IDU_FIT_POINT( "smiStatistics::gatherTableStatInternal::validation" );

    /* Lock�� ���� ���¿��� ��ȿ�� Table���� Ȯ���� */
    IDE_ERROR( sTableHeader->mType == SMC_TABLE_NORMAL );
    IDE_ERROR( smcTable::isDropedTable( sTableHeader ) == ID_FALSE );
    IDE_ERROR( sTableHeader->mColumnCount > 0 );

    /********************* GatherTableStat O(1) *********************/
    IDE_TEST( getTableStatNumPage( aTable,
                                   ID_TRUE, /* current */
                                   &sNumPage )
              != IDE_SUCCESS );
    sPercentage = getAdjustedPercentage( aPercentage, sNumPage );
    sDegree     = getAdjustedDegree( aDegree );

    /********************* GatherTableStat O(N) *********************/
    sFullScanModule = gSmnSequentialScan.mModule[
        SMN_GET_BASE_TABLE_TYPE_ID( sTableHeader->mFlag ) ];
    IDE_ASSERT( sFullScanModule != NULL );

    if( ( sFullScanModule->mGatherStat != NULL ) &&
        ( smuProperty::getDBMSStatGatherInternalStat() == 1 ) )
    {
        IDE_TEST( sFullScanModule->mGatherStat( aStatistics,
                                                aSmxTrans,
                                                sPercentage,
                                                sDegree,
                                                sTableHeader,
                                                aTotalTableArg,
                                                NULL,  /*indexHeader*/
                                                NULL,  /*smiAllStat*/
                                                ID_FALSE ) /*DynamicMode*/
                  != IDE_SUCCESS );
    }

    /********************* GatherIndexStat O(N) ********************/
    sIndexCount = smcTable::getIndexCount( sTableHeader );
    for( i = 0 ; i < sIndexCount ; i ++ )
    {
        sIndexHeader = (void*)smcTable::getTableIndex( sTableHeader, i );

        /* gatherIndexStat �Լ��� �̿��ϵ�, NoInvalidate �ɼ��� �༭
         * �� gatherTableStat������ Rebuild���� �ϰ� �� */
        IDE_TEST( gatherIndexStatInternal( aStatistics,
                                           aSmxTrans,
                                           sIndexHeader,
                                           aPercentage,
                                           aDegree,
                                           ID_TRUE ) /* NoInvalidate */
                  != IDE_SUCCESS );
    }

    checkNoInvalidate( sTableHeader, aNoInvalidate );

    sState = 0;
    IDE_TEST( unlock4GatherStat( sTableHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        unlock4GatherStat( sTableHeader );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION :
 *     �� Table�� ��� ��������� ��ȯ�մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aTrans               - [IN]  Transaction
 *  aTable               - [IN]  ������� ������ Table
 *  aAllStats            - [IN]  ��������� ������ �޸�
 *  aPercentage          - [IN]  Sampling ����
 *  aDynamicMode         - [IN]  ���� ������� ����
 *                               ID_TRUE : ���� ��������� �����Ѵ�.
 **************************************************************/
IDE_RC smiStatistics::getTableAllStat( idvSQL        * aStatistics,
                                       smiTrans      * aTrans,
                                       void          * aTable,
                                       smiAllStat    * aAllStats,
                                       SFloat          aPercentage,
                                       idBool          aDynamicMode )
{
    smnIndexModule     * sFullScanModule = NULL;
    smnIndexModule     * sIndexModule    = NULL;
    smcTableHeader     * sTableHeader    = NULL;
    smnIndexHeader     * sIndexHeader    = NULL;
    smiColumn          * sColumn         = NULL;
    void               * sSmxTrans       = NULL;
    UInt                 i               = 0;
    UInt                 sState          = 0;

    sTableHeader = ((smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable));
    sSmxTrans    = aTrans->mTrans;

    /****************************** Lock ****************************/
    IDE_TEST( lock4GatherStat( sSmxTrans,
                               sTableHeader,
                               ID_FALSE ) /* IS_LOCK */
              != IDE_SUCCESS );
    sState = 1;

    /* Lock�� ���� ���¿��� ��ȿ�� Table���� Ȯ���� */
    IDE_ERROR( sTableHeader->mType == SMC_TABLE_NORMAL );
    IDE_ERROR( smcTable::isDropedTable( sTableHeader ) == ID_FALSE );
    IDE_ERROR( sTableHeader->mColumnCount > 0 );

    /********************* GatherTableStat O(N) *********************/

    if ( isValidTableStat( sTableHeader ) == ID_FALSE )
    {
        if ( aDynamicMode == ID_FALSE )
        {
            // �Ϲ� ��� �϶��� 0���� �ʱ�ȭ �Ѵ�.
            idlOS::memset( &aAllStats->mTableStat,
                           0,
                           ID_SIZEOF( smiTableStat ) );

            idlOS::memset( aAllStats->mColumnStat,
                           0,
                           ID_SIZEOF( smiColumnStat ) * aAllStats->mColumnCount );
        }
        else
        {
            // ���̳��� ����϶��� ��������� �����Ѵ�.
            sFullScanModule = gSmnSequentialScan.mModule[
                SMN_GET_BASE_TABLE_TYPE_ID( sTableHeader->mFlag ) ];
            IDE_ERROR( sFullScanModule != NULL );

            if ( ( sFullScanModule->mGatherStat != NULL ) &&
                 ( smuProperty::getDBMSStatGatherInternalStat() == 1 ) )
            {
                IDE_TEST( sFullScanModule->mGatherStat( aStatistics,
                                                        sSmxTrans,
                                                        aPercentage,
                                                        1,
                                                        sTableHeader,
                                                        NULL,
                                                        NULL, /*indexHeader*/
                                                        aAllStats,
                                                        aDynamicMode )
                        != IDE_SUCCESS );
            }
            else
            {
                idlOS::memset( &aAllStats->mTableStat,
                               0,
                               ID_SIZEOF( smiTableStat ) );

                idlOS::memset( aAllStats->mColumnStat,
                               0,
                               ID_SIZEOF( smiColumnStat ) * aAllStats->mColumnCount );
            }
        }
    }
    else
    {
        // ��������� ��ȿ�Ҷ��� ���� �������ش�.
        idlOS::memcpy( &aAllStats->mTableStat,
                       &sTableHeader->mStat,
                       ID_SIZEOF( smiTableStat ) );

        for ( i = 0 ; i < aAllStats->mColumnCount; i++ )
        {
            sColumn = (smiColumn*)smcTable::getColumn( sTableHeader, i );

            idlOS::memcpy( &(aAllStats->mColumnStat[i]),
                           &(sColumn->mStat),
                           ID_SIZEOF( smiColumnStat ) );
        }
    }


    /********************* GatherIndexStat O(N) ********************/

    for ( i = 0 ; i < aAllStats->mIndexCount; i++ )
    {
        sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex( sTableHeader, i );

        if ( isValidIndexStat( sIndexHeader ) == ID_FALSE )
        {
            if ( aDynamicMode == ID_FALSE )
            {
                // �Ϲ� ��� �϶��� 0���� �ʱ�ȭ �Ѵ�.
                idlOS::memset( &(aAllStats->mIndexStat[i]),
                               0,
                               ID_SIZEOF( smiIndexStat ) );
            }
            else
            {
                // ���̳��� ����϶��� ��������� �����Ѵ�.
                sIndexModule = (smnIndexModule*)sIndexHeader->mModule;

                if ( sIndexModule->mGatherStat != NULL )
                {
                    IDE_TEST( sIndexModule->mGatherStat( aStatistics,
                                                         sSmxTrans,
                                                         aPercentage,
                                                         1,
                                                         sTableHeader,
                                                         NULL, /*TotalTableArg*/
                                                         sIndexHeader,
                                                         &(aAllStats->mIndexStat[i]),
                                                         aDynamicMode )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Rtree �� �ǹ� ���� ��� */
                    idlOS::memset( &(aAllStats->mIndexStat[i]),
                                   0,
                                   ID_SIZEOF( smiIndexStat ) );
                }
            }
        }
        else
        {
            // BUG-47946
            IDE_ERROR_MSG( sIndexHeader->mStat.mId == sIndexHeader->mId,
                           "Invalid Statistics Index ID : "
                           "%"ID_UINT32_FMT" != %"ID_UINT32_FMT"\n",
                           sIndexHeader->mId,
                           sIndexHeader->mStat.mId );

            // ��������� ��ȿ�Ҷ��� ���� �������ش�.
            idlOS::memcpy( &(aAllStats->mIndexStat[i]),
                           &sIndexHeader->mStat,
                           ID_SIZEOF( smiIndexStat ) );
        }

        // BUG-47884
        // qmoStat::getIndexStatistics���� SM�� QP ��ġ�ϱ� ���ؼ� index id�� �ʿ��մϴ�.
        aAllStats->mIndexStat[i].mId = sIndexHeader->mId;
    }

    sState = 0;
    IDE_TEST( unlock4GatherStat( sTableHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
    case 1:
        unlock4GatherStat( sTableHeader );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     �� Index�� ��������� �����մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aIndex               - [IN]  ������� ������ Index
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::gatherIndexStats( idvSQL * aStatistics,
                                        void   * aSmiTrans,
                                        void   * aIndex,
                                        SFloat   aPercentage,
                                        SInt     aDegree,
                                        SChar    aNoInvalidate )
{
    return gatherIndexStatInternal( aStatistics,
                                    ((smiTrans*)aSmiTrans)->mTrans,
                                    aIndex,
                                    aPercentage, 
                                    aDegree, 
                                    aNoInvalidate );
}

/**************************************************************
 * DESCRIPTION : 
 *     �� Index�� ��������� �����մϴ�.
 *
 *  aStatistics          - [IN]  �������
 *  aSmxTrans            - [IN]  Transaction
 *  aIndex               - [IN]  ������� ������ Index
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::gatherIndexStatInternal( idvSQL       * aStatistics,
                                               void         * aSmxTrans,
                                               void         * aIndex,
                                               SFloat         aPercentage,
                                               SInt           aDegree,
                                               SChar          aNoInvalidate )
{
    smcTableHeader     * sTableHeader   = NULL;
    smnIndexHeader     * sIndexHeader;
    smnIndexModule     * sIndexModule;
    SLong                sNumPage       = 0;
    SFloat               sPercentage;
    SInt                 sDegree;
    UInt                 sState = 0;

    /* BUG-42105 */
    smnRuntimeHeader * sRuntimeHeader;

    sIndexHeader = (smnIndexHeader*)aIndex;
   
    /* BUG-42105 */ 
    sRuntimeHeader = (smnRuntimeHeader*)sIndexHeader->mHeader;

    /* BUG-42105 : �ε����� disable�� ��� ��� ���� ������ skip �Ѵ�. */
    if( (sIndexHeader->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_DISABLE )
    { 
        ideLog::log( IDE_SM_0, 
                     "====================================================\n"
                     "SKIP GATHER INDEX STAT : DISABLE INDEX\n"
                     "Index Name: %s\n"
                     "Index ID:   %"ID_UINT32_FMT"\n"
                     "Flag:       %"ID_UINT32_FMT"\n"    
                     "====================================================\n",
                     sIndexHeader->mName,
                     sIndexHeader->mId,
                     sIndexHeader->mFlag );                         

        IDE_CONT( SKIP_GATHER_INDEX_STAT_DISABLE );
    }

    /* BUG-42105 : �ε����� inconsitent �� ��� ��� ���� ������ skip �Ѵ�. */
    if ( ( smnManager::getIsConsistentOfIndexHeader( aIndex ) != ID_TRUE ) )
    {
        if ( sRuntimeHeader != NULL )
        {
            ideLog::log( IDE_SM_0, 
                         "====================================================\n"
                         "SKIP GATHER INDEX STAT : INCONSISTENT INDEX\n"
                         "Index Name: %s\n"
                         "Index ID:   %"ID_UINT32_FMT"\n"
                         "Consistent: %"ID_UINT32_FMT"\n" 
                         "====================================================\n",
                         sIndexHeader->mName,
                         sIndexHeader->mId,
                         sRuntimeHeader->mIsConsistent ); 
        }
        else
        {
            ideLog::log( IDE_SM_0, 
                         "====================================================\n"
                         "SKIP GATHER INDEX STAT : INCONSISTENT INDEX\n"
                         "Index Name: %s\n"
                         "Index ID:   %"ID_UINT32_FMT"\n"
                         "Runtime Index header is NULL\n"
                         "====================================================\n",
                         sIndexHeader->mName,
                         sIndexHeader->mId ); 
        }

        IDE_CONT(SKIP_GATHER_INDEX_STAT_INCONSISTENT );
    }

    IDU_FIT_POINT( "smiStatistics::gatherIndexStatInternal::getTableHeaderFromOID" );

    IDE_TEST( smcTable::getTableHeaderFromOID( sIndexHeader->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    /****************************** Lock ****************************/
    IDE_TEST( lock4GatherStat( aSmxTrans,
                               sTableHeader,
                               ID_TRUE ) 
                != IDE_SUCCESS );
    sState = 1;

    /********************* GatherIndexStat *********************/
    IDE_TEST( getIndexStatNumPage( aIndex,
                                   ID_TRUE, /* current */
                                   &sNumPage )
              != IDE_SUCCESS );

    sPercentage = getAdjustedPercentage( aPercentage, sNumPage );
    sDegree     = getAdjustedDegree( aDegree );

    sIndexModule = (smnIndexModule*)sIndexHeader->mModule;
    if( sIndexModule->mGatherStat != NULL )
    {
        IDE_TEST( sIndexModule->mGatherStat( aStatistics,
                                             aSmxTrans,
                                             sPercentage,
                                             sDegree,
                                             sTableHeader,
                                             NULL, /*TotalTableArg*/
                                             sIndexHeader,
                                             NULL, /*smiIndexStat*/
                                             ID_FALSE ) /*DynamicMode*/
                  != IDE_SUCCESS );
    }
    else
    {
        /* Rtree �� �ǹ� ���� ��� */
    }

    checkNoInvalidate( sTableHeader, aNoInvalidate );

    sState = 0;
    IDE_TEST( unlock4GatherStat( sTableHeader ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_GATHER_INDEX_STAT_DISABLE );
    IDE_EXCEPTION_CONT( SKIP_GATHER_INDEX_STAT_INCONSISTENT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        unlock4GatherStat( sTableHeader );
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     ������� ������ ���� Lock�� ����
 *
 *  aSmxTrans            - [IN]  Transaction
 *  aTableHeader         - [IN]  Lock���� Table
 *  aNeedIXLock          - [IN]  True �ΰ�� table IX LOCK �� ��´�.
 *                               False �ΰ�� table IS Lock.
 *
 **************************************************************/
IDE_RC smiStatistics::lock4GatherStat( void           * aSmxTrans,
                                       smcTableHeader * aTableHeader,
                                       idBool           aNeedIXLock )
{

    /* Tablespace��  Lock�� ��´�. */
    IDE_TEST( sctTableSpaceMgr::lockAndValidateTBS(
                    aSmxTrans,
                    smcTable::getTableSpaceID((void*)aTableHeader),
                    SCT_VAL_DDL_DML,
                    ID_TRUE,   /* intent lock  ���� */
                    ID_FALSE,  /* exclusive lock */
                    ID_ULONG_MAX ) /* lock wait micro sec */
        != IDE_SUCCESS );

    if ( aNeedIXLock == ID_TRUE )
    {
        IDE_TEST( smlLockMgr::lockTableModeIX( aSmxTrans,
                                               SMC_TABLE_LOCK( aTableHeader ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* BUG-42420 
         * Dynamic sampling������ IS Lock�� ����Ѵ�. */
        IDE_TEST( smlLockMgr::lockTableModeIS( aSmxTrans,
                                               SMC_TABLE_LOCK( aTableHeader ) )
                  != IDE_SUCCESS );
    }

    IDE_TEST_RAISE( smcTable::isDropedTable( aTableHeader ) == ID_TRUE,
                    TABLE_NOT_FOUND );

    return IDE_SUCCESS;

    IDE_EXCEPTION( TABLE_NOT_FOUND );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_Table_Not_Found ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     ������� ������ ���� ���� Lock�� ������
 *     TransactionLock�� Commit�� Ǯ�״� ����
 *
 *  aTableHeader         - [IN]  Lock���� Table
 **************************************************************/
IDE_RC smiStatistics::unlock4GatherStat( smcTableHeader * /* aTableHeader */ )
{
    /* nothing to do ... */

    return IDE_SUCCESS;
}

/**************************************************************
 * DESCRIPTION : 
 *    Table�� Logging �ϰ� SetDirty�Ͽ� Durable�ϵ��� �����Ѵ�.
 *
 *  aSmxTrans            - [IN]  ����ϴ� Transaction
 *  aHeader              - [IN]  ��� ���̹�
 **************************************************************/
IDE_RC smiStatistics::storeTableStat( void           * aSmxTrans,
                                      smcTableHeader * aHeader)
{
    if( aSmxTrans != NULL )
    {
        IDU_FIT_POINT( "smiStatistics::storeTableStat::physicalUpdate" );

        IDE_TEST( smrUpdate::physicalUpdate( 
                NULL, /* idvSQL* */
                aSmxTrans,
                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                SM_MAKE_PID( aHeader->mSelfOID ),
                SM_MAKE_OFFSET( aHeader->mSelfOID ) + SMP_SLOT_HEADER_SIZE,
                NULL,          /* aBeforeImage */
                0,             /* aBeforeImageSize */
                (SChar*)aHeader,             /*aAfterImage1 */
                ID_SIZEOF( smcTableHeader ), /*aAfterImage2 */
                NULL,          /* aAfterImage2 */
                0 )            /* aAfterImageSize2 */
            != IDE_SUCCESS);

        IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                SM_MAKE_PID( aHeader->mSelfOID ))
            != IDE_SUCCESS);
    }
    else
    {
        /* Nologging */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *    Column�� Logging �ϰ� SetDirty�Ͽ� Durable�ϵ��� �����Ѵ�.
 *
 *  aSmxTrans            - [IN]  ����ϴ� Transaction
 *  aHeader              - [IN]  ��� ���̹�
 *  aColumnID            - [IN]  ��� Column�� ID
 **************************************************************/
IDE_RC smiStatistics::storeColumnStat( void           * aSmxTrans,
                                       smcTableHeader * aHeader,
                                       UInt             aColumnID )
{
    smiColumn              * sColumn;
    smOID                    sColOID;
    UChar                  * sColumnPage;
    scOffset                 sOffset;

    if( aSmxTrans != NULL )
    {
        sColumn = smcTable::getColumnAndOID( aHeader,
                                             aColumnID,
                                             &sColOID );

        IDE_TEST( smmManager::getPersPagePtr( 
                        SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                        SM_MAKE_PID( sColOID ),
                        (void**)&sColumnPage )
            != IDE_SUCCESS);

        sOffset = (vULong)((UChar*)sColumn - sColumnPage);

        IDE_TEST( smrUpdate::physicalUpdate( 
                NULL, /* idvSQL* */
                aSmxTrans,
                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                SM_MAKE_PID( sColOID ),
                sOffset,
                NULL,          /* aBeforeImage */
                0,             /* aBeforeImageSize */
                (SChar*)sColumn,
                ID_SIZEOF( smiColumn ),
                NULL,          /* aAfterImage2 */
                0 )            /* aAfterImageSize2 */
            != IDE_SUCCESS);

        IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                SM_MAKE_PID(sColOID))
            != IDE_SUCCESS);
    }
    else
    {
        /* Nologging */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *    Index�� Logging �ϰ� SetDirty�Ͽ� Durable�ϵ��� �����Ѵ�.
 *
 *  aSmxTrans            - [IN]  ����ϴ� Transaction
 *  aHeader              - [IN]  ��� ���̹�
 *  aColumnID            - [IN]  ��� Column�� ID
 **************************************************************/
IDE_RC smiStatistics::storeIndexStat( void           * aSmxTrans,
                                      smcTableHeader * aHeader,
                                      smnIndexHeader * aIndex )
{
    UInt             sIndexHeaderSize;
    UInt             sIndexSlot;
    SInt             sIndexOffset;
    scPageID         sPageID = SC_NULL_PID;
    scOffset         sOffset = SC_NULL_OFFSET;

    if( aSmxTrans != NULL )
    {
        sIndexHeaderSize = smnManager::getSizeOfIndexHeader();

        IDE_ASSERT( smcTable::findIndexVarSlot2AlterAndDrop(aHeader,
                                                            aIndex, 
                                                            sIndexHeaderSize,
                                                            &sIndexSlot,
                                                            &sIndexOffset)
                    == IDE_SUCCESS );

        sPageID = SM_MAKE_PID( aHeader->mIndexes[sIndexSlot].fstPieceOID );
        sOffset = SM_MAKE_OFFSET( aHeader->mIndexes[sIndexSlot].fstPieceOID )
                  + ID_SIZEOF(smVCPieceHeader) 
                  + sIndexOffset;

        IDE_TEST( smrUpdate::physicalUpdate( NULL, /* Statistics */
                                             aSmxTrans,
                                             SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                             sPageID,
                                             sOffset,
                                             NULL,          /* aBeforeImage */
                                             0,             /* aBeforeImageSize */
                                             (SChar*)aIndex,/* after image */
                                             sIndexHeaderSize,
                                             NULL,   /* after image */
                                             0 )     /* after imagesize */
                  != IDE_SUCCESS);

        (void)smmDirtyPageMgr::insDirtyPage(
                                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                    sPageID);
    }
    else
    {
        /* Nologging */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/**************************************************************
 * DESCRIPTION : 
 *     System ��������� ������
 *
 * aSReadTime            - [IN]  Single block read time
 * aMReadTime            - [IN]  Milti block read time
 * aMReadPageCount       - [IN]  DB_FILE_MULTIPAGE_READ_COUNT
 * aHashTime             - [IN]  Hashing time
 * aCompareTime          - [IN]  Column value compare time
 * aStoreTime            - [IN]  temp table store time
 **************************************************************/
IDE_RC smiStatistics::setSystemStatsByUser( SDouble * aSReadTime,
                                            SDouble * aMReadTime,
                                            SLong   * aMReadPageCount,
                                            SDouble * aHashTime,
                                            SDouble * aCompareTime,
                                            SDouble * aStoreTime )
{
    UInt      sState = 0;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    mSystemStat.mCreateTV   = smiGetCurrTime();
    if( aSReadTime != NULL )
    {
        mSystemStat.mSReadTime = *aSReadTime;
    }
    if( aMReadTime != NULL )
    {
        mSystemStat.mMReadTime = *aMReadTime;
    }
    if( aMReadPageCount != NULL )
    {
        mSystemStat.mDBFileMultiPageReadCount = *aMReadPageCount;
    }
    if( aHashTime != NULL )
    {
        mSystemStat.mHashTime = *aHashTime;
    }
    if( aCompareTime != NULL )
    {
        mSystemStat.mCompareTime = *aCompareTime;
    }
    if( aStoreTime != NULL )
    {
        mSystemStat.mStoreTime = *aStoreTime;
    }

    /*
     * BUG-37403  system statistics are not stored properly.
     */
    IDE_TEST( smmManager::setSystemStatToMemBase( &mSystemStat ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     System ��������� �ʱ�ȭ��
 *
 **************************************************************/
IDE_RC smiStatistics::clearSystemStats( void )
{
    UInt      sState = 0; 

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1; 

    mSystemStat.mCreateTV = 0;
    mSystemStat.mSReadTime = 0; 
    mSystemStat.mMReadTime = 0; 
    mSystemStat.mDBFileMultiPageReadCount = 0; 
    mSystemStat.mHashTime = 0; 
    mSystemStat.mCompareTime = 0; 
    mSystemStat.mStoreTime = 0; 

    /*   
     * BUG-37403  system statistics are not stored properly.
     */
    IDE_TEST( smmManager::setSystemStatToMemBase( &mSystemStat ) != IDE_SUCCESS );

    sState = 0; 
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {    
        unlock4SetStat();
    }    

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Table ��������� ������
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aTable               - [IN]  ������� ������ Table
 *  aNumRow              - [IN]  Row����
 *  aNumPage             - [IN]  Page����
 *  aAverageRowLength             - [IN]  ��� Row����
 *  aOneRowReadTime      - [IN]  1 Row read time
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::setTableStatsByUser( void    * aSmiTrans,
                                           void    * aTable, 
                                           SLong   * aNumRow,
                                           SLong   * aNumPage,
                                           SLong   * aAverageRowLength,
                                           SDouble * aOneRowReadTime,
                                           SChar     aNoInvalidate )
{
    smcTableHeader    * sHeader;
    smiTableStat      * sStat;
    UInt                sState = 0;

    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);
    sStat   = &sHeader->mStat;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    sStat->mCreateTV   = smiGetCurrTime();
    if( aNumRow != NULL )
    {
        sStat->mNumRow = *aNumRow;
    }
    if( aNumPage != NULL )
    {
        sStat->mNumPage = *aNumPage;
    }
    if( aAverageRowLength != NULL )
    {
        sStat->mAverageRowLen = *aAverageRowLength;
    }
    if( aOneRowReadTime != NULL )
    {
        sStat->mOneRowReadTime = *aOneRowReadTime;
    }

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    IDE_TEST( storeTableStat( ((smiTrans*)aSmiTrans)->mTrans, sHeader ) 
              != IDE_SUCCESS );

    checkNoInvalidate( sHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Table ��������� �ʱ�ȭ
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aTable               - [IN]  ������� ������ Table
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::clearTableStats( void    * aSmiTrans,
                                       void    * aTable, 
                                       SChar     aNoInvalidate )
{
    smcTableHeader    * sHeader;
    smiTableStat      * sStat;
    UInt                sState = 0;

    sHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);
    sStat   = &sHeader->mStat;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    sStat->mCreateTV = 0;
    sStat->mNumRow = 0;
    sStat->mNumPage = 0;
    sStat->mAverageRowLen = 0;
    sStat->mOneRowReadTime = 0;

    sStat->mNumRowChange = 0;
    sStat->mSampleSize = 0;
    sStat->mMetaSpace = 0;
    sStat->mUsedSpace = 0;
    sStat->mAgableSpace = 0;
    sStat->mFreeSpace = 0;
    sStat->mNumCachedPage = 0;  /*BUG-42095 */

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    IDE_TEST( storeTableStat( ((smiTrans*)aSmiTrans)->mTrans, sHeader ) 
              != IDE_SUCCESS );

    checkNoInvalidate( sHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Index ������� �ϳ��� ������
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aIndex               - [IN]  ��� �ε���
 *  aNumRow              - [IN]  Row����
 *  aNumPage             - [IN]  Page����
 *  aNumDist             - [IN]  NumberOfDistinctValue
 *  aClusteringFactor    - [IN]  ClusteringFactor
 *  aIndexHeight         - [IN]  �ε��� ����
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::setIndexStatsByUser( void   * aSmiTrans,
                                           void   * aIndex, 
                                           SLong  * aKeyCount,
                                           SLong  * aNumPage,
                                           SLong  * aNumDist,
                                           SLong  * aAvgSlotCnt,
                                           SLong  * aClusteringFactor,
                                           SLong  * aIndexHeight,
                                           SChar    aNoInvalidate )
{
    smcTableHeader     * sTableHeader;
    smnIndexHeader     * sPersHeader;
    smnRuntimeHeader   * sRunHeader;
    smiIndexStat       * sStat;

    sPersHeader = (smnIndexHeader*)aIndex;
    sRunHeader  = (smnRuntimeHeader*)sPersHeader->mHeader;
    sStat       = &sPersHeader->mStat;

    SMI_INDEX_SETSTAT( sRunHeader,
                       setIndexStatInternal( sStat,
                                             aKeyCount,
                                             aNumPage,
                                             aNumDist,
                                             aAvgSlotCnt,
                                             aClusteringFactor,
                                             aIndexHeight,
                                             NULL, /*aMetaSpace*/
                                             NULL, /*aUsedSpace*/
                                             NULL, /*aAgableSpace*/
                                             NULL, /*aFreeSpace*/
                                             NULL /*aSampleSize*/ ) );

    IDE_TEST( smcTable::getTableHeaderFromOID( sPersHeader->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );
    IDE_TEST( storeIndexStat( ((smiTrans*)aSmiTrans)->mTrans, 
                              sTableHeader, 
                              sPersHeader  )
              != IDE_SUCCESS );
    checkNoInvalidate( sTableHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Index ������� �ϳ��� �ʱ�ȭ
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aIndex               - [IN]  ��� �ε���
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::clearIndexStats( void   * aSmiTrans,
                                       void   * aIndex, 
                                       SChar    aNoInvalidate )
{
    smcTableHeader     * sTableHeader;
    smnIndexHeader     * sPersHeader;
    smnRuntimeHeader   * sRunHeader;
    smiIndexStat       * sStat;

    sPersHeader = (smnIndexHeader*)aIndex;
    
    /* BUG-41939 : disable�� index�� ������� �ʱ�ȭ ���� �ʴ´�. */
    IDE_TEST_CONT( ((sPersHeader->mFlag) & SMI_INDEX_USE_MASK) != SMI_INDEX_USE_ENABLE,
                   SKIP_CLEAR_DISABLE_IDX);

    sRunHeader  = (smnRuntimeHeader*)sPersHeader->mHeader;
    sStat       = &sPersHeader->mStat;

    SMI_INDEX_SETSTAT( sRunHeader,
                       clearIndexStatInternal( sStat ) );

    IDE_TEST( smcTable::getTableHeaderFromOID( sPersHeader->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );
    IDE_TEST( storeIndexStat( ((smiTrans*)aSmiTrans)->mTrans, 
                              sTableHeader, 
                              sPersHeader  )
              != IDE_SUCCESS );
    checkNoInvalidate( sTableHeader, aNoInvalidate );

    /* BUG-41939 */
    IDE_EXCEPTION_CONT( SKIP_CLEAR_DISABLE_IDX );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Index ��������� Null ���ذ��� ������
 *
 *  aIndexStat           - [IN]  ��� �ε���
 *  aNumRow              - [IN]  Row����
 *  aNumPage             - [IN]  Page����
 *  aNumDist             - [IN]  NumberOfDistinctValue
 *  aClusteringFactor    - [IN]  ClusteringFactor
 *  aIndexHeight         - [IN]  �ε��� ����
 *  aMetaSpace           - [IN]  PageHeader, ExtDir�� Meta ����
 *  aUsedSpace           - [IN]  ���� ������� ����
 *  aAgableSpace         - [IN]  ���߿� Aging������ ����
 *  aFreeSpace           - [IN]  Data������ ������ �� ����
 *  aSampleSize          - [IN]  ���ø� ����
 *  aNumCachedPage       - [IN]  Cache�� Page ����
 **************************************************************/
void smiStatistics::setIndexStatInternal( smiIndexStat * aStat,
                                          SLong        * aKeyCount,
                                          SLong        * aNumPage,
                                          SLong        * aNumDist,
                                          SLong        * aAvgSlotCnt,
                                          SLong        * aClusteringFactor,
                                          SLong        * aIndexHeight,
                                          SLong        * aMetaSpace,
                                          SLong        * aUsedSpace,
                                          SLong        * aAgableSpace,
                                          SLong        * aFreeSpace,
                                          SFloat       * aSampleSize )
{
    aStat->mCreateTV   = smiGetCurrTime();

    if ( aKeyCount != NULL )
    {
        aStat->mKeyCount = *aKeyCount;
    }
    else
    {
        // Nothing to do.
    }

    if ( aNumPage != NULL )
    {
        aStat->mNumPage = *aNumPage;
    }
    else
    {
        // Nothing to do.
    }

    if ( aNumDist != NULL )
    {
        aStat->mNumDist = *aNumDist;
    }
    else
    {
        // Nothing to do.
    }

    if ( aAvgSlotCnt != NULL )
    {
        aStat->mAvgSlotCnt = *aAvgSlotCnt;
    }
    else
    {
        // Nothing to do.
    }

    if ( aClusteringFactor != NULL )
    {
        aStat->mClusteringFactor = *aClusteringFactor;
    }
    else
    {
        // Nothing to do.
    }

    if ( aIndexHeight != NULL )
    {
        aStat->mIndexHeight = *aIndexHeight;
    }
    else
    {
        // Nothing to do.
    }

    if ( aMetaSpace != NULL )
    {
        aStat->mMetaSpace = *aMetaSpace;
    }
    else
    {
        // Nothing to do.
    }

    if ( aUsedSpace != NULL )
    {
        aStat->mUsedSpace = *aUsedSpace;
    }
    else
    {
        // Nothing to do.
    }

    if ( aAgableSpace != NULL )
    {
        aStat->mAgableSpace = *aAgableSpace;
    }
    else
    {
        // Nothing to do.
    }

    if ( aFreeSpace != NULL )
    {
        aStat->mFreeSpace = *aFreeSpace;
    }
    else
    {
        // Nothing to do.
    }

    if ( aSampleSize != NULL )
    {
        aStat->mSampleSize = *aSampleSize;
    }
    else
    {
        // Nothing to do.
    }
}

/**************************************************************
 * DESCRIPTION : 
 *     Index ��������� Null ���ذ��� ������
 *
 *  aIndexStat           - [IN]  ��� �ε���
 **************************************************************/
void smiStatistics::clearIndexStatInternal( smiIndexStat * aStat )
{
    aStat->mCreateTV = 0;
    aStat->mKeyCount = 0;
    aStat->mNumPage = 0;
    aStat->mNumDist = 0;
    aStat->mAvgSlotCnt = 0;
    aStat->mClusteringFactor = 0;
    aStat->mIndexHeight = 0;
    aStat->mMetaSpace = 0;
    aStat->mUsedSpace = 0;
    aStat->mAgableSpace = 0;
    aStat->mFreeSpace = 0;

    aStat->mSampleSize = 0;
   
    idlOS::memset( aStat->mMinValue,
                   0,
                   SMI_MAX_MINMAX_VALUE_SIZE );
    idlOS::memset( aStat->mMaxValue,
                   0,
                   SMI_MAX_MINMAX_VALUE_SIZE );
}



/**************************************************************
 * DESCRIPTION : 
 *     Column ������� �ϳ��� ������
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aTable               - [IN]  ��� ���̺�
 *  aColumnID            - [IN]  ��� Į�� ID
 *  aNumDist             - [IN]  NumDist��
 *  aNumNull             - [IN]  NumDist��
 *  aAverageColumnLength             - [IN]  ��� Į�� ����
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::setColumnStatsByUser( void   * aSmiTrans,
                                            void   * aTable, 
                                            UInt     aColumnID, 
                                            SLong  * aNumDist,
                                            SLong  * aNumNull,
                                            SLong  * aAverageColumnLength,
                                            UChar  * aMinValue,
                                            UChar  * aMaxValue,
                                            SChar    aNoInvalidate )
{
    smcTableHeader    * sTableHeader;
    smiColumn         * sColumn;
    smiColumnStat     * sStat;
    UInt                sColumnSeq;
    UInt                sState = 0;

    sColumnSeq = aColumnID  & SMI_COLUMN_ID_MASK;

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);
    sColumn = (smiColumn*)smcTable::getColumn( 
                    (const void*)sTableHeader, 
                    sColumnSeq );
    sStat = &sColumn->mStat;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    sStat->mCreateTV   = smiGetCurrTime();
    if( aNumDist != NULL )
    {
        sStat->mNumDist = *aNumDist;
    }
    if( aNumNull != NULL )
    {
        sStat->mNumNull = *aNumNull;
    }
    if( aAverageColumnLength != NULL )
    {
        sStat->mAverageColumnLen = *aAverageColumnLength;
    }
    // BUG-40290 SET_COLUMN_STATS min, max ����
    if( aMinValue != NULL )
    {
        idlOS::memcpy( sStat->mMinValue,
                       aMinValue,
                       MAX_MINMAX_VALUE_SIZE );
    }
    if( aMaxValue != NULL )
    {
        idlOS::memcpy( sStat->mMaxValue,
                       aMaxValue,
                       MAX_MINMAX_VALUE_SIZE );
    }

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    IDE_TEST( storeColumnStat( ((smiTrans*)aSmiTrans)->mTrans, 
                               sTableHeader,
                               sColumnSeq )
              != IDE_SUCCESS );
    checkNoInvalidate( sTableHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}

/**************************************************************
 * DESCRIPTION : 
 *     Column ������� �ϳ��� �ʱ�ȭ
 *
 *  aStatistics          - [IN]  �������
 *  aSmiTrans            - [IN]  Transaction
 *  aTable               - [IN]  ��� ���̺�
 *  aColumnID            - [IN]  ��� Į�� ID
 *  aNoInvalidate        - [IN]  Plan �籸�� ���Ұ��ΰ�
 **************************************************************/
IDE_RC smiStatistics::clearColumnStats( void   * aSmiTrans,
                                        void   * aTable, 
                                        UInt     aColumnID, 
                                        SChar    aNoInvalidate )
{
    smcTableHeader    * sTableHeader;
    smiColumn         * sColumn;
    smiColumnStat     * sStat;
    UInt                sColumnSeq;
    UInt                sState = 0;

    sColumnSeq = aColumnID  & SMI_COLUMN_ID_MASK;

    sTableHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aTable);
    sColumn = (smiColumn*)smcTable::getColumn( 
                    (const void*)sTableHeader, 
                    sColumnSeq );
    sStat = &sColumn->mStat;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    sStat->mCreateTV = 0;
    sStat->mNumDist = 0;
    sStat->mNumNull = 0;
    sStat->mAverageColumnLen = 0;

    sStat->mSampleSize = 0;
    idlOS::memset( sStat->mMinValue,
                   0,
                   SMI_MAX_MINMAX_VALUE_SIZE );
    idlOS::memset( sStat->mMaxValue,
                   0,
                   SMI_MAX_MINMAX_VALUE_SIZE );

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    IDE_TEST( storeColumnStat( ((smiTrans*)aSmiTrans)->mTrans, 
                               sTableHeader,
                               sColumnSeq )
              != IDE_SUCCESS );
    checkNoInvalidate( sTableHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlock4SetStat();
    }

    return IDE_FAILURE;
}


/******************************************************************
 * DESCRIPTION : 
 *    Total Table�� Column ������� �������� ���� Argument �ʱ�ȭ
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aTableArgument       - [OUT] ������ Argument(ColumnArgument����)
 ******************************************************************/
IDE_RC smiStatistics::beginTotalTableStat( void     * aTotalTableHandle,
                                           SFloat     aPercentage,
                                           void    ** aTotalTableArg )
{
    smcTableHeader  * sTableHeader = NULL;
    void            * sTableArg    = NULL;

    sTableHeader = 
        (smcTableHeader *)( ( (UChar*)aTotalTableHandle ) + SMP_SLOT_HEADER_SIZE );

    IDE_TEST( beginTableStat( sTableHeader,
                              aPercentage,
                              ID_FALSE,
                              &sTableArg )
              != IDE_SUCCESS );
    
    *aTotalTableArg = sTableArg;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aTotalTableArg = NULL;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    ������ ����� Total Table Header�� ������
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aTableArgument       - [IN]  �м� ����� �����ص� ����
 ******************************************************************/
IDE_RC smiStatistics::setTotalTableStat( void   * aTotalTableHandle,
                                         void   * aTotalTableArg )
{
    smcTableHeader         * sTableHeader   = NULL;
    smiStatTableArgument   * sTabArg        = NULL;
    smiStatColumnArgument  * sColArg        = NULL;
    smiColumnStat          * sStat          = NULL;
    UInt                     sState         = 0;
    UInt                     i              = 0;

    sTableHeader = 
        (smcTableHeader *)( ( (UChar*)aTotalTableHandle ) + SMP_SLOT_HEADER_SIZE );

    sTabArg = (smiStatTableArgument*)aTotalTableArg;

    IDE_TEST( lock4SetStat() != IDE_SUCCESS );
    sState = 1;

    sTableHeader->mStat.mCreateTV   = smiGetCurrTime();

    if ( sTabArg->mAnalyzedRowCount > 0 )
    {
        for( i = 0 ; i < sTableHeader->mColumnCount ; i ++ )
        {
            sColArg = &sTabArg->mColumnArgument[i];
            sStat   = &sColArg->mColumn->mStat;

            /* GlobalHash, LocalHash ������ setTableStat() ����. */
            if ( sColArg->mLocalGroupCount > 0 )
            {
                IDE_ERROR( ( sColArg->mLocalNumDist / 
                                ( sColArg->mLocalGroupCount + 1) ) 
                           <= sColArg->mGlobalNumDist );

                IDE_ERROR( sColArg->mGlobalNumDist <=
                           sColArg->mLocalNumDist );
            }
            else
            {
                IDE_ERROR( sColArg->mLocalNumDist 
                           == sColArg->mGlobalNumDist );
            }

            sColArg->mNumDist =
                 ( sColArg->mGlobalNumDist 
                   + sColArg->mLocalNumDist * sColArg->mLocalGroupCount )
                 / ( sColArg->mLocalGroupCount + 1 );

            sStat->mCreateTV = smiGetCurrTime();
            sStat->mNumDist  = sColArg->mNumDist;
            sStat->mNumNull  = sColArg->mNullCount;
            sStat->mAverageColumnLen = sColArg->mAccumulatedSize /
                    sTabArg->mAnalyzedRowCount;
        }
    }
    else
    {
        for( i = 0 ; i < sTableHeader->mColumnCount ; i ++ )
        {
            sColArg = &sTabArg->mColumnArgument[i];
            sStat   = &sColArg->mColumn->mStat;

            /* BUG-41130 */
            sStat->mCreateTV= 0;
            sStat->mNumDist = 0;
            sStat->mNumNull = 0;
            sStat->mAverageColumnLen = 0;
        }
    }

    sState = 0;
    IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        (void) unlock4SetStat();
    case 0:
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Total Table�� Column ������� ������ ����
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aTableArgument       - [IN]  �м� ����� �����ص�
 ******************************************************************/
IDE_RC smiStatistics::endTotalTableStat( void   * aTotalTableHandle,
                                         void   * aTotalTableArg,
                                         SChar    aNoInvalidate )
{
    void            * sTableArg    = aTotalTableArg;
    smcTableHeader  * sTableHeader =
        (smcTableHeader *)( ( (UChar*)aTotalTableHandle ) + SMP_SLOT_HEADER_SIZE );

    // ȣ�� �����, Table Argument�� ����
    IDE_TEST( endTableStat( sTableHeader, sTableArg, ID_FALSE ) != IDE_SUCCESS );

    // SCN Update
    checkNoInvalidate( sTableHeader, aNoInvalidate );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Table �� Column ������� ������ ���� Argument �ʱ�ȭ
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aPercentage          - [IN]  Sampling ����
 *  aDegree              - [IN]  ParallelDegree
 *  aDynamicMode         - [IN]  ���� ������� ����
 *  aTableArgument       - [OUT] ������ Argument(ColumnArgument����)
 ******************************************************************/
IDE_RC smiStatistics::beginTableStat( smcTableHeader   * aHeader,
                                      SFloat             aPercentage,
                                      idBool             aDynamicMode,
                                      void            ** aTableArgument )
{
    smiStatTableArgument   * sTableArgument;
    smiStatColumnArgument  * sColArg;
    UInt                     sState = 0;
    UInt                     i;

    IDU_LIMITPOINT_RAISE(
        "smiStatistics::beginTableStat::malloc1",
        insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                       1,
                                       ID_SIZEOF( smiStatTableArgument ),
                                       (void**) &sTableArgument )
                    != IDE_SUCCESS, 
                    insufficient_memory );
    sState = 1;

    IDU_LIMITPOINT_RAISE(
        "smiStatistics::beginTableStat::malloc2",
        insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                       1,
                                       ID_SIZEOF( smiColumn ),
                                       (void**) &sTableArgument->mBlankColumn )
                    != IDE_SUCCESS, 
                    insufficient_memory );
    sState = 2;

    IDU_LIMITPOINT_RAISE(
        "smiStatistics::beginTableStat::malloc3",
        insufficient_memory );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                       aHeader->mColumnCount,
                                       ID_SIZEOF( smiStatColumnArgument ),
                                       (void**) &sTableArgument->mColumnArgument )
                    != IDE_SUCCESS, 
                    insufficient_memory );
    sState = 3;

    /* �м��ϴµ� �ʿ��� �Լ��� ã�Ƽ� ������ */
    for( i = 0; i < aHeader->mColumnCount ; i ++ )
    {
        sColArg = &sTableArgument->mColumnArgument[i];
        sColArg->mColumn = (smiColumn*)smcTable::getColumn( aHeader, i );

        IDE_TEST( gSmiGlobalCallBackList.findHash(
                    sColArg->mColumn,
                    &sColArg->mHashFunc )
                != IDE_SUCCESS);

        IDE_TEST( gSmiGlobalCallBackList.findActualSize(
                    sColArg->mColumn,
                    &sColArg->mActualSizeFunc )
                != IDE_SUCCESS );

        IDE_TEST( gSmiGlobalCallBackList.findIsNull(
                    sColArg->mColumn,
                    sColArg->mColumn->flag,
                    &sColArg->mIsNull )
                  != IDE_SUCCESS );

        IDE_TEST(gSmiGlobalCallBackList.findCompare(
                    sColArg->mColumn,
                    sColArg->mColumn->flag
                            | SMI_COLUMN_COMPARE_NORMAL,
                    &sColArg->mCompare )
                != IDE_SUCCESS );

        IDE_TEST( gSmiGlobalCallBackList.getNonStoringSize(
                    sColArg->mColumn,
                    &sColArg->mMtdHeaderLength )
                  != IDE_SUCCESS );

        IDE_TEST( gSmiGlobalCallBackList.findCopyDiskColumnValue4DataType(
                    sColArg->mColumn,
                    &sColArg->mCopyDiskColumnFunc )
                != IDE_SUCCESS );

        IDE_TEST(gSmiGlobalCallBackList.findKey2String(
                    sColArg->mColumn,
                    sColArg->mColumn->flag,
                    &sColArg->mKey2String )
                 != IDE_SUCCESS );
    }

    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( traceTableStat( aHeader,
                                " BEFORE - GATHER TABLE STATISTICS" )
                != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sTableArgument->mSampleSize = aPercentage;

    (*aTableArgument) = (void*)sTableArgument;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 3:
        (void) iduMemMgr::free( sTableArgument->mColumnArgument );
    case 2:
        (void) iduMemMgr::free( sTableArgument->mBlankColumn );
    case 1:
        (void) iduMemMgr::free( sTableArgument );
    default:
        break;
    }

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    TableArgument�� �������� Row�� �޾� �� Row�� �м���.
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aTableArgument       - [IN]  �м� ����� �����ص�
 *  aRow                 - [IN]  �м����
 ******************************************************************/
IDE_RC smiStatistics::analyzeRow4Stat( smcTableHeader * aHeader,
                                       void           * aTableArgument,
                                       void           * aTotalTableArg,
                                       UChar          * aRow )
{
    smiStatTableArgument  * sTabArg;
    smiStatColumnArgument * sColArg;
    smiStatTableArgument  * sTotalTabArg;
    smiStatColumnArgument * sTotalColArg;
    UInt                    sRowSize = 0;
    SChar                 * sValue;
    UInt                    sLength  = 0;
    UInt                    sColumnCount;
    UInt                    sHashValue;
    UInt                    sColumnSize;
    UInt                    i;
    smOID                   sDictionaryTableOID;
    smiColumn             * sDictionaryColumn;

    sTabArg      = (smiStatTableArgument*)aTableArgument;
    sColumnCount = aHeader->mColumnCount;

    /* PROJ-2339
     * 
     * aTotalTableArg�� �� ���̺� ���� ��������� �����ؼ� �޴� �ӽð�������,
     * DBMS���� Partitioned Table ��ü�� ���� ��������� ���ϱ� ���� �ʿ��� argument�̴�.
     * Index Table, Non-Partitioned Table������ aTotalTableArg�� NULL�̴�.
     */
    if ( aTotalTableArg != NULL )
    {
        sTotalTabArg = (smiStatTableArgument*)aTotalTableArg;
    }
    else
    {
        sTotalTabArg = NULL;
    }

    /* Column���� ��ȸ�� */
    for( i = 0 ; i < sColumnCount ; i ++ )
    {
        sColArg = &sTabArg->mColumnArgument[i];

        if ( sTotalTabArg != NULL )
        {
            // partitioned table
            sTotalColArg = &sTotalTabArg->mColumnArgument[i];
        }
        else
        {
            sTotalColArg = NULL;
        }

        if ( (sColArg->mColumn->flag & SMI_COLUMN_COMPRESSION_MASK)
            == SMI_COLUMN_COMPRESSION_FALSE )
        {
            switch (sColArg->mColumn->flag & SMI_COLUMN_TYPE_MASK)
            {
                case SMI_COLUMN_TYPE_FIXED:
                    sValue = (SChar*)aRow + sColArg->mColumn->offset;
                    break;
                case SMI_COLUMN_TYPE_VARIABLE:
                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* BUG-40069
                     * GEOMETRY �� ���ؼ��� ��������� �� �ʿ䰡 ����. */
                    if( sColArg->mColumn->size > SMP_VC_PIECE_MAX_SIZE )
                    {
                        continue;
                    }
                    else
                    {
                        // nothing to do
                    }

                    sValue = sgmManager::getVarColumn( (SChar*)aRow, sColArg->mColumn, &sLength);
                    break;
                case SMI_COLUMN_TYPE_LOB:
                    /* skip updating stats */
                    continue;
                default:
                    IDE_ASSERT(0);
            }
        }
        else
        {
            // PROJ-2264
            sValue = (SChar*)smiGetCompressionColumn( aRow,
                                                      sColArg->mColumn,
                                                      ID_TRUE, //aUseColumnOffset
                                                      &sLength );
            
        }

        /* NDV - Number of Distinct Value */
        if ( sValue != NULL )
        {
            sHashValue = sColArg->mHashFunc( 
                0x01020304, /* mtc::hashInitialValue */
                sColArg->mColumn,
                sValue );
        }
        else
        {
            sHashValue = 0x01020304;
        }

        /* LocalHash */
        if ( SMI_STAT_GET_HASH_VALUE( sColArg->mLocalHashTable, 
                                     sHashValue ) == 0 )
        {
            SMI_STAT_SET_HASH_VALUE( sColArg->mLocalHashTable, 
                                     sHashValue );
            sColArg->mLocalNumDist ++;
            sColArg->mLocalNumDistPerGroup ++;

            if ( sColArg->mLocalNumDistPerGroup >= SMI_STAT_HASH_THRESHOLD )
            {
                idlOS::memset( sColArg->mLocalHashTable,
                               0,
                               ID_SIZEOF( sColArg->mLocalHashTable ) );
                sColArg->mLocalGroupCount++;
                sColArg->mLocalNumDistPerGroup = 0;
            }
        }
        /* GlobalHash */
        if ( SMI_STAT_GET_HASH_VALUE( sColArg->mGlobalHashTable, 
                                     sHashValue ) == 0 )
        {
            SMI_STAT_SET_HASH_VALUE( sColArg->mGlobalHashTable, 
                                     sHashValue );
            sColArg->mGlobalNumDist ++;
        }
        IDE_ERROR( sColArg->mGlobalNumDist <= sColArg->mLocalNumDist );


        /* Column Size */
        if ( sValue != NULL )
        {
            sColumnSize = sColArg->mActualSizeFunc( sColArg->mColumn,
                                                    sValue );
        }
        else
        {
            sColumnSize = ID_SIZEOF(void*);
        }
        sColArg->mAccumulatedSize += sColumnSize;
        sRowSize                  += sColumnSize;

        /* Null */
        if ( (sValue == NULL) ||
            (sColArg->mIsNull( sColArg->mColumn, sValue )
                       == ID_TRUE) )
        {
            sColArg->mNullCount ++;
        }
        else
        {
            // BUG-37484
            if( gSmiGlobalCallBackList.needMinMaxStatistics( sColArg->mColumn )
                == ID_TRUE )
            {
                if( (sColArg->mColumn->flag & SMI_COLUMN_COMPRESSION_MASK)
                    == SMI_COLUMN_COMPRESSION_FALSE )
                {
                    IDE_TEST( compareAndSetMinMaxValue( sTabArg,
                                                        (UChar*)sValue,
                                                        sTabArg->mColumnArgument[i].mColumn,
                                                        i,
                                                        sColumnSize )
                              != IDE_SUCCESS );
                }
                else
                {
                    // PROJ-2264
                    sDictionaryTableOID = sColArg->mColumn->mDictionaryTableOID;

                    smiGetSmiColumnFromMemTableOID( sDictionaryTableOID,
                                                    0,
                                                    &sDictionaryColumn );

                    IDE_TEST( compareAndSetMinMaxValue( sTabArg,
                                                        (UChar*)sValue,
                                                        sDictionaryColumn,
                                                        i,
                                                        sColumnSize )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // Nothing to do.
            }
        }

        /* PROJ-2339 Gathering Cumulative Statistics */
        if ( sTotalColArg != NULL )
        {
            IDE_TEST( analyzeRow4TotalStat( sTotalColArg,
                                            sColArg,
                                            sValue,
                                            sHashValue,
                                            sColumnSize )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do : non-partitioned table
        }
    }

    sTabArg->mAnalyzedRowCount ++;
    sTabArg->mAccumulatedSize += sRowSize;

    /* PROJ-2339 Gathering Cumulative Statistics */
    if ( sTotalTabArg != NULL )
    {
        sTotalTabArg->mAnalyzedRowCount ++;
        sTotalTabArg->mAccumulatedSize += sRowSize;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    ColArg�� ���� ���� ��������� �����ϱ� ���� �Լ�
 *
 *  aTotalColArg         - [IN] Total Table�� Column Argument
 *  aCurrColArg          - [IN] ���� Table�� Column Argument
 *  aValue               - [IN] ���� Row�� Value
 *  aHashValue           - [IN] ���� Row�� Hash Value
 *  aColumnSize          - [IN] ���� Row�� Column Size
 ******************************************************************/
IDE_RC smiStatistics::analyzeRow4TotalStat( smiStatColumnArgument * aTotalColArg, 
                                            smiStatColumnArgument * aCurrColArg,
                                            SChar                 * aValue, 
                                            UInt                    aHashValue, 
                                            UInt                    aColumnSize )
{
    IDE_ASSERT( aTotalColArg != NULL );

    /* LocalHash */
    if ( SMI_STAT_GET_HASH_VALUE( aTotalColArg->mLocalHashTable, 
                                  aHashValue ) == 0 )
    {
        SMI_STAT_SET_HASH_VALUE( aTotalColArg->mLocalHashTable, 
                                 aHashValue );
        aTotalColArg->mLocalNumDist ++;
        aTotalColArg->mLocalNumDistPerGroup ++;

        if ( aTotalColArg->mLocalNumDistPerGroup >= SMI_STAT_HASH_THRESHOLD )
        {
            idlOS::memset( aTotalColArg->mLocalHashTable,
                           0,
                           ID_SIZEOF( aTotalColArg->mLocalHashTable ) );
            aTotalColArg->mLocalGroupCount++;
            aTotalColArg->mLocalNumDistPerGroup = 0;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    /* GlobalHash */
    if ( SMI_STAT_GET_HASH_VALUE( aTotalColArg->mGlobalHashTable, 
                                  aHashValue ) == 0 )
    {
        SMI_STAT_SET_HASH_VALUE( aTotalColArg->mGlobalHashTable, 
                                 aHashValue );
        aTotalColArg->mGlobalNumDist ++;
    }
    else
    {
        // Nothing to do.
    }
    IDE_ERROR( aTotalColArg->mGlobalNumDist <= aTotalColArg->mLocalNumDist );

    /* Column Size */
    aTotalColArg->mAccumulatedSize += aColumnSize;

    /* Null */
    if ( ( aValue == NULL ) ||
         ( aCurrColArg->mIsNull( aCurrColArg->mColumn, aValue ) == ID_TRUE) )
    {
        aTotalColArg->mNullCount ++;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    ������ ���� ���Ͽ� Min �Ǵ� Max���̸�, ��������.
 *
 *  aTableArgument       - [IN]  �м� ����� �����ص�
 *  aRow                 - [IN]  �м����
 *  aColumnIdx           - [IN]  Į�� ��ȣ
 *  aColumnSize          - [IN]  �ش� Į���� ũ��
 *  aMin                 - [IN]  Min���� ���� Check�ΰ�?(�ƴϸ� Max)
 ******************************************************************/
IDE_RC smiStatistics::compareAndSetMinMaxValue( smiStatTableArgument * aTableArgument,
                                                UChar                * aRow,
                                                smiColumn            * aColumn,
                                                UInt                   aColumnIdx,
                                                UInt                   aColumnSize )
{
    smiStatColumnArgument * sColumnArgument;
    UInt                    sColumnValueSize;
    smiValueInfo            sValueInfo1;
    smiValueInfo            sValueInfo2;
    UChar                 * sTargetValue;
    UInt                  * sTargetLength;
    SInt                    sTargetDirection;
    idBool                  sNeedSetValue;

    idBool                  sIsMin = ID_TRUE;
    SInt                    i;

    sColumnArgument = &aTableArgument->mColumnArgument[ aColumnIdx ];

    /* set value to compare value */
    sValueInfo1.column = aTableArgument->mBlankColumn;
    sValueInfo1.value  = aRow;
    sValueInfo1.length = aColumnSize;
    sValueInfo1.flag   = SMI_OFFSET_USELESS;

    for( i = 0; i < 2; i++ ) /* Min �ѹ�, Max �ѹ� */
    {
        if( sIsMin == ID_TRUE )
        {
            sTargetValue     = (UChar*)sColumnArgument->mMinValue;
            sTargetLength    = &sColumnArgument->mMinLength;
            sTargetDirection = +1;
        }
        else
        {
            sTargetValue     = (UChar*)sColumnArgument->mMaxValue;
            sTargetLength    = &sColumnArgument->mMaxLength;
            sTargetDirection = -1;
        }

        /* PROJ-2180 valueForModule
           SMI_OFFSET_USELESS �� ���ϴ� �÷��� mBlankColumn �� ����ؾ� �Ѵ�.
           Compare �Լ����� valueForModule �� ȣ������ �ʰ�
           offset �� ����Ҽ� �ֱ� �����̴�. */
        sValueInfo2.column = aTableArgument->mBlankColumn;
        sValueInfo2.value  = sTargetValue;
        sValueInfo2.length = *sTargetLength;
        sValueInfo2.flag   = SMI_OFFSET_USELESS;

        if( (*sTargetLength) == 0 )
        {
            sNeedSetValue = ID_TRUE;
        }
        else
        {
            sNeedSetValue = ( sColumnArgument->mCompare( &sValueInfo1,
                                                         &sValueInfo2 )
                              * sTargetDirection ) < 0 ? ID_TRUE : ID_FALSE;
        }

        if( sNeedSetValue == ID_TRUE )
        {
            sColumnValueSize = IDL_MIN( aColumnSize,
                                        SMI_MAX_MINMAX_VALUE_SIZE ) 
                               - sColumnArgument->mMtdHeaderLength;

            IDE_TEST( sColumnArgument->mCopyDiskColumnFunc( 
                        aColumn->size,
                        sTargetValue,
                        0, // aCopyOffset,
                        sColumnValueSize,
                        aRow
                        + sColumnArgument->mMtdHeaderLength )
                    != IDE_SUCCESS );

            *sTargetLength = aColumnSize;
        }
        else
        {
            /* do nothing */
        }

        sIsMin = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    �ش� Table�� OneRowReadTime ���� ������ ������
 *
 * aTableArgument       - [IN]  �м� ����� �����ص� ����
 * aReadRowTime         - [IN]  Row �ϳ��� �д� ���� �ð�
 * aReadRowCnt          - [IN]  ���� row ����
 ******************************************************************/
IDE_RC smiStatistics::updateOneRowReadTime( void  * aTableArgument,
                                            SLong   aReadRowTime,
                                            SLong   aReadRowCnt )
{
    smiStatTableArgument   * sTabArg;

    sTabArg = (smiStatTableArgument*)aTableArgument;

    if( (sTabArg->mReadRowTime < sTabArg->mReadRowTime + aReadRowTime) &&
        (sTabArg->mReadRowCnt < sTabArg->mReadRowCnt + aReadRowCnt) )
    {
        sTabArg->mReadRowTime   += aReadRowTime;
        sTabArg->mReadRowCnt    += aReadRowCnt;
    }
    else
    {
        sTabArg->mReadRowTime   = aReadRowTime;
        sTabArg->mReadRowCnt    = aReadRowCnt;
    }

    return IDE_SUCCESS;
}

/******************************************************************
 * DESCRIPTION : 
 *    �ش� Table�� ��뷮�� ������.
 *
 * aTableArgument       - [IN]  �м� ����� �����ص� ����
 * aMetaSpace           - [IN]  Meta ���� ��뷮�� ������
 * aUsedSpace           - [IN]  Used ���� ��뷮�� ������
 * aAgableSpace         - [IN]  Agable ���� ��뷮�� ������
 * aFreeSpace           - [IN]  Free ���� ��뷮�� ������
 ******************************************************************/
IDE_RC smiStatistics::updateSpaceUsage( void  * aTableArgument,
                                        SLong   aMetaSpace,
                                        SLong   aUsedSpace,
                                        SLong   aAgableSpace,
                                        SLong   aFreeSpace )
{
    smiStatTableArgument   * sTabArg;

    sTabArg = (smiStatTableArgument*)aTableArgument;

    sTabArg->mMetaSpace   += aMetaSpace;
    sTabArg->mUsedSpace   += aUsedSpace;
    sTabArg->mAgableSpace += aAgableSpace;
    sTabArg->mFreeSpace   += aFreeSpace;
    sTabArg->mAnalyzedPageCount ++;

    return IDE_SUCCESS;
}

/******************************************************************
 * DESCRIPTION : 
 *    ������ ����� ������
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aTableArgument       - [IN]  �м� ����� �����ص� ����
 *  aAllStats            - [IN]  ���̳��� ����϶� ��������� �����ϴ� ����
 *  aDynamicMode         - [IN]  ���� ������� ����
 ******************************************************************/
IDE_RC smiStatistics::setTableStat( smcTableHeader * aHeader,
                                    void           * aSmxTrans,
                                    void           * aTableArgument,
                                    smiAllStat     * aAllStats,
                                    idBool           aDynamicMode )
{
    smiTableStat           * sTableStat = NULL;
    smiStatTableArgument   * sTabArg;
    smiStatColumnArgument  * sColArg;
    smiColumnStat          * sStat;
    SFloat                   sPercentage    = 0.0;
    UInt                     sState         = 0;
    UInt                     i;

    sTabArg = (smiStatTableArgument*)aTableArgument;

    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( lock4SetStat() != IDE_SUCCESS );

        sState = 1;

        sTableStat = &(aHeader->mStat);

    }
    else
    {
        sTableStat = &(aAllStats->mTableStat);
    }

    /******************************************************************
     * ������� ������
     ******************************************************************/
    sTableStat->mCreateTV    = smiGetCurrTime();

    IDE_TEST( getTableStatNumRow( ((smpSlotHeader*)aHeader - 1),
                                  ID_TRUE,
                                  NULL,
                                  &(sTableStat->mNumRow) )
              != IDE_SUCCESS );

    IDE_TEST( getTableStatNumPage( ((smpSlotHeader*)aHeader - 1),
                                   ID_TRUE,
                                   &(sTableStat->mNumPage) )
              != IDE_SUCCESS );

    if ( aDynamicMode == ID_FALSE )
    {
        if ( sTabArg->mReadRowCnt > 0 )
        {
            sTableStat->mOneRowReadTime = (SDouble)sTabArg->mReadRowTime / (SDouble)sTabArg->mReadRowCnt;
        }
        else
        {
            /* ���� �� ���� */
        }
    }
    else
    {
        // ���̳��� ����϶��� mOneRowReadTime�� �������� �ʴ´�.
        sTableStat->mOneRowReadTime = 0.0;
    }

    sTableStat->mSampleSize = sTabArg->mSampleSize;

    if ( sTableStat->mSampleSize < 1.0f )
    {
        sPercentage = (SFloat)sTabArg->mAnalyzedPageCount / sTableStat->mNumPage;
    }
    else
    {
        sPercentage = sTableStat->mSampleSize;
    }

    if ( sTabArg->mAnalyzedPageCount > 0 )
    {
        sTableStat->mMetaSpace = (SLong) (sTabArg->mMetaSpace / sPercentage);
        sTableStat->mUsedSpace = (SLong) (sTabArg->mUsedSpace / sPercentage);
        sTableStat->mAgableSpace = (SLong) (sTabArg->mAgableSpace / sPercentage);
        sTableStat->mFreeSpace = (SLong) (sTabArg->mFreeSpace / sPercentage);
    }
    else
    {
        sTableStat->mMetaSpace   = 0;
        sTableStat->mUsedSpace   = 0;
        sTableStat->mAgableSpace = 0;
        sTableStat->mFreeSpace   = 0;
    }

    if ( sTabArg->mAnalyzedRowCount > 0 )
    {
        sTableStat->mAverageRowLen = sTabArg->mAccumulatedSize / sTabArg->mAnalyzedRowCount;

        for ( i = 0 ; i < aHeader->mColumnCount ; i++ )
        {
            sColArg = &sTabArg->mColumnArgument[i];

            if ( aDynamicMode == ID_FALSE )
            {
                sStat   = &sColArg->mColumn->mStat;
            }
            else
            {
                sStat = &(aAllStats->mColumnStat[i]);
            }

            /* Hash Table�� �� �д�. �ϳ��� LocalHash(���� LH) �ٸ� �ϳ���
             * GlobalHash(���� GH)�� Ī�Ѵ�.
             * LocalHash�� ���� ���� NDV���� LD, GlobalHash�� ���� ����
             * NDV���� GD �̶� �Ѵ�.
             * LocalHash�� LD�� Threshold(T)�� �����Ҷ�����, �ʱ�ȭ�Ѵ�.
             *      - T�� HashSize/8 �̴�.
             *      - �ʱ�ȭ�� Ƚ���� G �̴�.
             * �̶� ���� �ݵ�� LD <= GD <= LD*(G+1) ���� �Ѵ�.
             * => GD/(G+1) + LD*G/(G+1)
             * => (GD + LD*G)/(G+1)
             *
             * G���� ������ ��������, GN���� ��Ȯ�ϴ�. �ֳ��ϸ� NDV �� ��ü��
             * Hashũ�⺸�� �۾ұ� ������ Threshold�� ���� ������ ���̱�
             * �����̴�.
             * G���� ũ�� Ŭ����, LN*L ���� ��Ȯ�ϴ�. �ֳ��ϸ� NDV���� Ŀ��
             * Threshold�� ���� ������ ���̱� �����̴�. */
            if ( sColArg->mLocalGroupCount > 0 )
            {
                /* Local ��� ���� <= Global ��� ���� */
                IDE_ERROR( ( sColArg->mLocalNumDist / 
                                ( sColArg->mLocalGroupCount + 1) ) 
                           <= sColArg->mGlobalNumDist );
                /* Local ��ü ���� > Global ��ü ���� */
                IDE_ERROR( sColArg->mGlobalNumDist <=
                           sColArg->mLocalNumDist );
            }
            else
            {
                IDE_ERROR( sColArg->mLocalNumDist 
                           == sColArg->mGlobalNumDist );
            }

            sColArg->mNumDist =
                 ( sColArg->mGlobalNumDist 
                   + sColArg->mLocalNumDist * sColArg->mLocalGroupCount )
                 / ( sColArg->mLocalGroupCount + 1 );


            sStat->mCreateTV= smiGetCurrTime();
            sStat->mNumDist = sColArg->mNumDist 
                    * sTableStat->mNumRow / sTabArg->mAnalyzedRowCount;
            sStat->mNumNull =  sColArg->mNullCount 
                    * sTableStat->mNumRow / sTabArg->mAnalyzedRowCount ;
            sStat->mAverageColumnLen = sColArg->mAccumulatedSize /
                    sTabArg->mAnalyzedRowCount;

            idlOS::memcpy( sStat->mMinValue,
                           sColArg->mMinValue,
                           SMI_MAX_MINMAX_VALUE_SIZE );
            idlOS::memcpy( sStat->mMaxValue,
                           sColArg->mMaxValue,
                           SMI_MAX_MINMAX_VALUE_SIZE );
        }
    }
    else
    {
        sTableStat->mAverageRowLen = 0;
        for ( i = 0 ; i < aHeader->mColumnCount ; i++ )
        {
            sColArg = &sTabArg->mColumnArgument[i];

            if ( aDynamicMode == ID_FALSE )
            {
                sStat   = &sColArg->mColumn->mStat;
            }
            else
            {
                sStat = &(aAllStats->mColumnStat[i]);
            }

            /* BUG-41130 */
            sStat->mCreateTV= 0;
            sStat->mNumDist = 0;
            sStat->mNumNull = 0;
            sStat->mAverageColumnLen = 0;
            idlOS::memset( sStat->mMinValue,
                           0,
                           SMI_MAX_MINMAX_VALUE_SIZE );
            idlOS::memset( sStat->mMaxValue,
                           0,
                           SMI_MAX_MINMAX_VALUE_SIZE );

        }
    }

    sTableStat->mNumCachedPage = 0; /* BUG-42095 */

    /* PROJ-2492 Dynamic sample selection */
    // ���̳��� ����϶��� ��������� �������� �ʴ´�.
    if ( aDynamicMode == ID_FALSE )
    {
        sState = 0;

        IDE_TEST( unlock4SetStat() != IDE_SUCCESS );

        IDE_TEST( storeTableStat( aSmxTrans, aHeader ) != IDE_SUCCESS );
        for ( i = 0 ; i < aHeader->mColumnCount ; i++ )
        {
            IDE_TEST( storeColumnStat( aSmxTrans, 
                                       aHeader,
                                       i )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        (void) unlock4SetStat();
    case 0:
        break;
    default:
        break;
    }

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Table �� Column ������� ������ ����
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aTableArgument       - [IN]  �м� ����� �����ص�
 *  aDynamicMode         - [IN]  ���� ������� ����
 ******************************************************************/
IDE_RC smiStatistics::endTableStat( smcTableHeader * aHeader,
                                    void           * aTableArgument,
                                    idBool           aDynamicMode )
{
    smiStatTableArgument   * sTabArg;
    smiStatColumnArgument  * sColArg;
    UInt                     sState = 3;

    sTabArg = (smiStatTableArgument*)aTableArgument;
    sColArg = sTabArg->mColumnArgument;

    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( traceTableStat( aHeader,
                                  " AFTER - GATHER TABLE STATISTICS" )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sState = 2;
    IDE_TEST( iduMemMgr::free( sColArg ) != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( iduMemMgr::free( sTabArg->mBlankColumn ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sTabArg ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 3:
        (void) iduMemMgr::free( sColArg );
    case 2:
        (void) iduMemMgr::free( sTabArg->mBlankColumn );
    case 1:
        (void) iduMemMgr::free( sTabArg );
    default:
        break;
    }

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Index ������� ���� ����
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aPersHeader          - [IN]  Persistent Index Header
 *  aPercentage          - [IN]  Sampling Percentage
 *  aDynamicMode         - [IN]  ���� ������� ����
 ******************************************************************/
IDE_RC smiStatistics::beginIndexStat( smcTableHeader   * aTableHeader,
                                      smnIndexHeader   * aPersHeader,
                                      idBool             aDynamicMode )
{
    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( traceIndexStat( aTableHeader,
                                  aPersHeader,
                                " BEFORE - GATHER INDEX STATISTICS" )
                != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    MinMax���� ������� ������ ����� ������
 *
 *  aIndex               - [IN]  ��� �ε���
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aStat                - [IN]  ���̳��� ����϶� ��������� �����ϴ� ����
 *  aIndexStat           - [IN]  �м� ����� �����ص� ����
 *  aDynamicMode         - [IN]  ���� ������� ����
 ******************************************************************/
IDE_RC smiStatistics::setIndexStatWithoutMinMax( smnIndexHeader * aIndex,
                                                 void           * aSmxTrans,
                                                 smiIndexStat   * aStat,
                                                 smiIndexStat   * aIndexStat,
                                                 idBool           aDynamicMode,
                                                 UInt             aStatFlag )
{
    smcTableHeader     * sTableHeader = NULL;
    smnRuntimeHeader   * sRunHeader;

    sRunHeader = (smnRuntimeHeader*)aIndex->mHeader;

    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
        if ( ( aStatFlag & SMI_INDEX_BUILD_RT_STAT_MASK )
               == SMI_INDEX_BUILD_RT_STAT_UPDATE )
        {
            SMI_INDEX_SETSTAT( sRunHeader,
                               setIndexStatInternal( &aIndex->mStat,
                                                     &aIndexStat->mKeyCount,
                                                     &aIndexStat->mNumPage,
                                                     &aIndexStat->mNumDist,
                                                     &aIndexStat->mAvgSlotCnt,
                                                     &aIndexStat->mClusteringFactor,
                                                     &aIndexStat->mIndexHeight,
                                                     &aIndexStat->mMetaSpace,
                                                     &aIndexStat->mUsedSpace,
                                                     &aIndexStat->mAgableSpace,
                                                     &aIndexStat->mFreeSpace,
                                                     &aIndexStat->mSampleSize ) );
        }

        IDE_TEST( smcTable::getTableHeaderFromOID( aIndex->mTableOID,
                                                   (void**)&sTableHeader )
                  != IDE_SUCCESS );
        IDE_TEST( storeIndexStat( aSmxTrans, sTableHeader, aIndex )
                  != IDE_SUCCESS );

    }
    else
    {
        idlOS::memcpy( aStat, aIndexStat, ID_SIZEOF(smiIndexStat) );

        aStat->mCreateTV    = smiGetCurrTime();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Row�� ������� ���� ���Թ�Ľ�, NumDist���� +1,-1 ������
 *
 *  aIndex               - [IN]  ��� �ε���
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aNumDist             - [IN]  ���ϰų� �� NumDist��
 ******************************************************************/
IDE_RC smiStatistics::incIndexNumDist( smnIndexHeader * aIndex,
                                       void           * aSmxTrans,
                                       SLong            aNumDist )
{
    smcTableHeader     * sTableHeader;
    smnRuntimeHeader   * sRunHeader;

    sRunHeader = (smnRuntimeHeader*)aIndex->mHeader;

    SMI_INDEX_SETSTAT( sRunHeader, aIndex->mStat.mNumDist += aNumDist );

    IDE_TEST( smcTable::getTableHeaderFromOID( aIndex->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    IDE_TEST( storeIndexStat( aSmxTrans, sTableHeader, aIndex ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Min�� ��������� ������
 *
 *  aIndex               - [IN]  ��� �ε���
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aMinValue            - [IN]  ������ Min��
 ******************************************************************/
IDE_RC smiStatistics::setIndexMinValue( smnIndexHeader * aIndex,
                                        void           * aSmxTrans,
                                        UChar          * aMinValue,
                                        UInt             aStatFlag )
{
    smcTableHeader     * sTableHeader = NULL;
    smnRuntimeHeader   * sRunHeader;

    sRunHeader = (smnRuntimeHeader*)aIndex->mHeader;

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    if ( ( aStatFlag & SMI_INDEX_BUILD_RT_STAT_MASK )
           == SMI_INDEX_BUILD_RT_STAT_UPDATE )
    {
        SMI_INDEX_SETSTAT( sRunHeader,
            if( aMinValue == NULL )
            {
                idlOS::memset( aIndex->mStat.mMinValue,
                            0x00,
                            ID_SIZEOF(aIndex->mStat.mMinValue) );
            }
            else
            {
                idlOS::memcpy( aIndex->mStat.mMinValue,
                            aMinValue,
                            ID_SIZEOF(aIndex->mStat.mMinValue) );
            }
        );
    }

    IDE_TEST( smcTable::getTableHeaderFromOID( aIndex->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    IDE_TEST( storeIndexStat( aSmxTrans,
                              sTableHeader,
                              aIndex )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    ������ ����� ������
 *
 *  aIndex               - [IN]  ��� �ε���
 *  aSmxTrans            - [IN]  Ʈ�����
 *  aMaxValue            - [IN]  ������ Max��
 ******************************************************************/
IDE_RC smiStatistics::setIndexMaxValue( smnIndexHeader * aIndex,
                                        void           * aSmxTrans,
                                        UChar          * aMaxValue,
                                        UInt             aStatFlag )
{
    smcTableHeader     * sTableHeader = NULL;
    smnRuntimeHeader   * sRunHeader;

    sRunHeader = (smnRuntimeHeader*)aIndex->mHeader;

    /* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰� */
    if ( ( aStatFlag & SMI_INDEX_BUILD_RT_STAT_MASK )
           == SMI_INDEX_BUILD_RT_STAT_UPDATE )
    {
        SMI_INDEX_SETSTAT( sRunHeader,
            if( aMaxValue == NULL )
            {
                idlOS::memset( aIndex->mStat.mMaxValue,
                                0x00,
                                ID_SIZEOF(aIndex->mStat.mMaxValue) );
            }
            else
            {
                idlOS::memcpy( aIndex->mStat.mMaxValue,
                                aMaxValue,
                                ID_SIZEOF(aIndex->mStat.mMaxValue) );
            } );
    }

    IDE_TEST( smcTable::getTableHeaderFromOID( aIndex->mTableOID,
                                               (void**)&sTableHeader )
              != IDE_SUCCESS );

    IDE_TEST( storeIndexStat( aSmxTrans,
                              sTableHeader,
                              aIndex )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/******************************************************************
 * DESCRIPTION : 
 *    Index ������� ���� ����
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aPersHeader          - [IN]  Persistent Index Header
 *  aDynamicMode         - [IN]  ���� ������� ����
 ******************************************************************/
IDE_RC smiStatistics::endIndexStat( smcTableHeader   * aTableHeader,
                                    smnIndexHeader   * aPersHeader,
                                    idBool             aDynamicMode )
{
    /* PROJ-2492 Dynamic sample selection */
    if ( aDynamicMode == ID_FALSE )
    {
        IDE_TEST( traceIndexStat( aTableHeader,
                                  aPersHeader,
                                  " AFTER - GATHER INDEX STATISTICS" )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Table��������� Trace���Ͽ� �����
 *
 *  aHeader              - [IN]  ��� ���̺�
 *  aTitle               - [IN]  TraceFile�� ���� Ÿ��Ʋ
 ******************************************************************/
IDE_RC smiStatistics::traceTableStat( smcTableHeader  * aHeader,
                                      const SChar     * aTitle )
{
    smiColumn     * sColumn;
    smiColumnStat * sStat;
    SChar           sStrMinValueBuf[SMI_DBMSSTAT_STRING_VALUE_SIZE];
    SChar           sStrMaxValueBuf[SMI_DBMSSTAT_STRING_VALUE_SIZE];
    UInt            sState = 0;
    UInt            i;

    ideLogEntry sLog( IDE_SM_0 );
    sState = 1;
    sLog.appendFormat( "========================================\n"
                       "%s\n"
                       "========================================\n"
                       " Table OID        : %"ID_vULONG_FMT"\n"
                       "\n"
                       " NumRow           : %"ID_INT64_FMT"\n"
                       " NumPage          : %"ID_INT64_FMT"\n"
                       " AverageRowLen    : %"ID_INT64_FMT"\n",
                       aTitle,
                       aHeader->mSelfOID,               /* smOID(vULong) */
                       aHeader->mStat.mNumRow,          /* SLong */
                       aHeader->mStat.mNumPage,         /* SLong */
                       aHeader->mStat.mAverageRowLen ); /* SLong */

    for( i = 0 ; i < aHeader->mColumnCount ; i ++ )
    {
        sColumn = (smiColumn*)smcTable::getColumn( aHeader, i );
        sStat   = &sColumn->mStat;

        IDE_TEST( getValueString( sColumn,
                                  (UChar*)sStat->mMinValue,
                                  SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                  sStrMinValueBuf )
                  != IDE_SUCCESS );
        IDE_TEST( getValueString( sColumn,
                                  (UChar*)sStat->mMaxValue,
                                  SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                  sStrMaxValueBuf )
                  != IDE_SUCCESS );

        sLog.appendFormat( "------------------------\n"
                           " COLUMN %"ID_UINT32_FMT"\n"
                           " NumDist           : %"ID_INT64_FMT"\n"
                           " NumNull           : %"ID_INT64_FMT"\n"
                           " AverageColumnLen  : %"ID_INT64_FMT"\n"
                           " Min Value(24b)    : %s\n"
                           " Max Value(24b)    : %s\n",
                           i,
                           sStat->mNumDist,          /* SLong */
                           sStat->mNumNull,          /* SLong */
                           sStat->mAverageColumnLen, /* SLong */
                           sStrMinValueBuf,
                           sStrMaxValueBuf );
    }
    sLog.append( "========================================\n" );
    sState = 0;
    sLog.write();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        sLog.write();
    default:
        break;
    }

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Index��������� Trace���Ͽ� �����
 *
 *  aTableHeader         - [IN]  ��� ���̺�
 *  aPersHeader          - [IN]  ��� Index
 *  aTitle               - [IN]  TraceFile�� ���� Ÿ��Ʋ
 ******************************************************************/
IDE_RC smiStatistics::traceIndexStat( smcTableHeader  * aTableHeader,
                                      smnIndexHeader  * aPersHeader,
                                      const SChar     * aTitle )
{
    smiColumn     * sColumn;
    SChar           sStrMinValueBuf[SMI_DBMSSTAT_STRING_VALUE_SIZE];
    SChar           sStrMaxValueBuf[SMI_DBMSSTAT_STRING_VALUE_SIZE];

    /* Index Min/Max�� 0�� Column�� ����� */
    sColumn = (smiColumn*)smcTable::getColumn( 
        aTableHeader,
        aPersHeader->mColumns[0] & SMI_COLUMN_ID_MASK );


    IDE_TEST( getValueString( sColumn,
                              (UChar*)aPersHeader->mStat.mMinValue,
                              SMI_DBMSSTAT_STRING_VALUE_SIZE,
                              sStrMinValueBuf )
              != IDE_SUCCESS );
    IDE_TEST( getValueString( sColumn,
                              (UChar*)aPersHeader->mStat.mMaxValue,
                              SMI_DBMSSTAT_STRING_VALUE_SIZE,
                              sStrMaxValueBuf )
              != IDE_SUCCESS );

    ideLog::log( IDE_SM_0,
                 "========================================\n"
                 "%s\n"
                 "========================================\n"
                 " Index Name: %s\n"
                 " Index ID:   %"ID_UINT32_FMT"\n"
                 " \n"
                 " NumPage:          %"ID_INT64_FMT"\n"
                 " ClusteringFactor: %"ID_INT64_FMT"\n"
                 " IndexHeight:      %"ID_INT64_FMT"\n"
                 " NumDist:          %"ID_INT64_FMT"\n"
                 " KeyCount:         %"ID_INT64_FMT"\n"
                 " Min Value:        %s\n" 
                 " Max Value:        %s\n" 
                 "========================================\n",
                 aTitle,
                 aPersHeader->mName,
                 aPersHeader->mId,                     /* UInt */
                 aPersHeader->mStat.mNumPage,          /* SLong */
                 aPersHeader->mStat.mClusteringFactor, /* SLong */
                 aPersHeader->mStat.mIndexHeight,      /* SLong */
                 aPersHeader->mStat.mNumDist,          /* SLong */
                 aPersHeader->mStat.mKeyCount,         /* SLong */
                 sStrMinValueBuf,
                 sStrMaxValueBuf );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * DESCRIPTION : 
 *    Key2String �Լ��� �̿���, Value�� String���� ��ȯ��
 *
 *  aColumn              - [IN]  ��� Į��
 *  aValue               - [IN]  ��ȯ�� ���� ��
 *  aBufferLength        - [OUT] ��ȯ�� String�� ������ ������ ����
 *  aBuffer              - [OUT] ��ȯ�� String�� ������ ����
 ******************************************************************/
IDE_RC smiStatistics::getValueString( smiColumn * aColumn,
                                      UChar     * aValue,
                                      UInt        aBufferLength,
                                      SChar     * aBuffer )
{
    smiKey2StringFunc   sKey2String;
    IDE_RC              sReturn;

    IDE_TEST(gSmiGlobalCallBackList.findKey2String( aColumn,
                                                    aColumn->flag,
                                                    &sKey2String )
        != IDE_SUCCESS );

    IDE_TEST( sKey2String( aColumn,
                           aValue,
                           aColumn->size,
                           (UChar*) SM_DUMP_VALUE_DATE_FMT,
                           idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                           (UChar*)aBuffer,
                           &aBufferLength,
                           &sReturn ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiStatistics::buildSystemRecord( void                * aHeader,
                                         iduFixedTableMemory * aMemory )
{
    smiDBMSStat4Perf sPerf;

    if( isValidSystemStat() )
    {
        idlOS::memset( &sPerf, 0, ID_SIZEOF( smiDBMSStat4Perf ) );

        smuUtility::getTimeString( mSystemStat.mCreateTV,
                                   SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                   sPerf.mCreateTime );

        sPerf.mType                     = 'S';
        sPerf.mSReadTime                = mSystemStat.mSReadTime;
        sPerf.mMReadTime                = mSystemStat.mMReadTime;
        sPerf.mDBFileMultiPageReadCount = mSystemStat.mDBFileMultiPageReadCount;
        sPerf.mHashTime                 = mSystemStat.mHashTime;
        sPerf.mCompareTime              = mSystemStat.mCompareTime;
        sPerf.mStoreTime                = mSystemStat.mStoreTime;

        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *)&sPerf)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
IDE_RC smiStatistics::buildTableRecord( smcTableHeader      * aTableHeader,
                                        void                * aHeader,
                                        iduFixedTableMemory * aMemory )
{
    smiDBMSStat4Perf   sPerf;
    smiTableStat     * sStat;

    sStat = &aTableHeader->mStat;

    /*
     * BUG-36941   [sm] Wrong memory reference in isValidTableStat() causes wrong statistics.
     */
    if( isValidTableStat( aTableHeader ) == ID_TRUE )
    {
        idlOS::memset( &sPerf, 0, ID_SIZEOF( smiDBMSStat4Perf ) );

        smuUtility::getTimeString( sStat->mCreateTV,
                                   SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                   sPerf.mCreateTime );

        sPerf.mType              = 'T';
        sPerf.mTargetID          = aTableHeader->mSelfOID;
        sPerf.mSampleSize        = sStat->mSampleSize;
        sPerf.mNumRowChange      = sStat->mNumRowChange;
        sPerf.mNumRow            = sStat->mNumRow;
        sPerf.mNumPage           = sStat->mNumPage;
        sPerf.mAvgLen            = sStat->mAverageRowLen;
        sPerf.mOneRowReadTime    = sStat->mOneRowReadTime;
        sPerf.mMetaSpace         = sStat->mMetaSpace;
        sPerf.mUsedSpace         = sStat->mUsedSpace;
        sPerf.mAgableSpace       = sStat->mAgableSpace;
        sPerf.mFreeSpace         = sStat->mFreeSpace;

        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *)&sPerf)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
IDE_RC smiStatistics::buildIndexRecord( smcTableHeader      * aTableHeader,
                                        smnIndexHeader      * aIndexHeader,
                                        void                * aHeader,
                                        iduFixedTableMemory * aMemory )
{
    smiDBMSStat4Perf    sPerf;
    smiIndexStat      * sStat;
    smiColumn         * sColumn;
    smiKey2StringFunc   sKey2String;
    UInt                sValueLength;
    IDE_RC              sReturn;

    sStat = &aIndexHeader->mStat;
    if( isValidIndexStat( (void*)aIndexHeader ) )
    {
        idlOS::memset( &sPerf, 0, ID_SIZEOF( smiDBMSStat4Perf ) );

        smuUtility::getTimeString( sStat->mCreateTV,
                                   SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                   sPerf.mCreateTime );

        /* Index Min/Max�� 0�� Column�� ����� */
        sColumn = (smiColumn*)smcTable::getColumn( 
                aTableHeader, aIndexHeader->mColumns[0] & SMI_COLUMN_ID_MASK);

        IDE_TEST(gSmiGlobalCallBackList.findKey2String(
                sColumn,
                sColumn->flag,
                &sKey2String )
            != IDE_SUCCESS );

        sPerf.mType              = 'I';
        sPerf.mTargetID          = aIndexHeader->mId;
        sPerf.mSampleSize        = sStat->mSampleSize;
        sPerf.mNumPage           = sStat->mNumPage;
        sPerf.mNumDist           = sStat->mNumDist;
        sPerf.mAvgSlotCnt        = sStat->mAvgSlotCnt;
        sPerf.mClusteringFactor  = sStat->mClusteringFactor;
        sPerf.mIndexHeight       = sStat->mIndexHeight;
        sPerf.mNumRow            = sStat->mKeyCount;
        sPerf.mMetaSpace         = sStat->mMetaSpace;
        sPerf.mUsedSpace         = sStat->mUsedSpace;
        sPerf.mAgableSpace       = sStat->mAgableSpace;
        sPerf.mFreeSpace         = sStat->mFreeSpace;

        sValueLength = SMI_DBMSSTAT_STRING_VALUE_SIZE;
        IDE_TEST( sKey2String(
                sColumn,
                sStat->mMinValue,
                sColumn->size,
                (UChar*) SM_DUMP_VALUE_DATE_FMT,
                idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                (UChar*)sPerf.mMinValue,
                &sValueLength,
                &sReturn ) 
            != IDE_SUCCESS );

        sValueLength = SMI_DBMSSTAT_STRING_VALUE_SIZE;
        IDE_TEST( sKey2String(
                sColumn,
                sStat->mMaxValue,
                sColumn->size,
                (UChar*) SM_DUMP_VALUE_DATE_FMT,
                idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                (UChar*)sPerf.mMaxValue,
                &sValueLength,
                &sReturn ) 
            != IDE_SUCCESS );

        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *)&sPerf)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
IDE_RC smiStatistics::buildColumnRecord( smcTableHeader      * aTableHeader,
                                         smiColumn           * aColumn,
                                         void                * aHeader,
                                         iduFixedTableMemory * aMemory )
{
    smiDBMSStat4Perf   sPerf;
    smiColumnStat    * sStat;
    smiKey2StringFunc  sKey2String;
    UInt               sValueLength;
    IDE_RC             sReturn;

    sStat = &aColumn->mStat;
    if( isValidColumnStat( aTableHeader,
                           ( aColumn->id & SMI_COLUMN_ID_MASK ) ) )
    {
        idlOS::memset( &sPerf, 0, ID_SIZEOF( smiDBMSStat4Perf ) );

        smuUtility::getTimeString( sStat->mCreateTV,
                                   SMI_DBMSSTAT_STRING_VALUE_SIZE,
                                   sPerf.mCreateTime );

        IDE_TEST(gSmiGlobalCallBackList.findKey2String(
                aColumn,
                aColumn->flag,
                &sKey2String )
            != IDE_SUCCESS );

        sPerf.mType       = 'C';
        sPerf.mTargetID   = aTableHeader->mSelfOID;
        sPerf.mColumnID   = aColumn->id;
        sPerf.mSampleSize = sStat->mSampleSize;
        sPerf.mNumDist    = sStat->mNumDist;
        sPerf.mNumNull    = sStat->mNumNull;
        sPerf.mAvgLen     = sStat->mAverageColumnLen;

        sValueLength = SMI_DBMSSTAT_STRING_VALUE_SIZE;
        IDE_TEST( sKey2String(
                aColumn,
                sStat->mMinValue,
                aColumn->size,
                (UChar*) SM_DUMP_VALUE_DATE_FMT,
                idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                (UChar*)sPerf.mMinValue,
                &sValueLength,
                &sReturn ) 
            != IDE_SUCCESS );

        sValueLength = SMI_DBMSSTAT_STRING_VALUE_SIZE;
        IDE_TEST( sKey2String(
                aColumn,
                sStat->mMaxValue,
                aColumn->size,
                (UChar*) SM_DUMP_VALUE_DATE_FMT,
                idlOS::strlen( SM_DUMP_VALUE_DATE_FMT ),
                (UChar*)sPerf.mMaxValue,
                &sValueLength,
                &sReturn ) 
            != IDE_SUCCESS );

        IDE_TEST(iduFixedTable::buildRecord(aHeader,
                                            aMemory,
                                            (void *)&sPerf)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
IDE_RC smiStatistics::buildDBMSStatRecord( idvSQL              * /*aStatistics*/,
                                           void                * aHeader,
                                           void                * /*aDumpObj*/,
                                           iduFixedTableMemory * aMemory )
{
    SChar              * sNxtRowPtr;
    SChar              * sCurRowPtr;
    smpSlotHeader      * sSlotHeader;
    smcTableHeader     * sTable;
    smnIndexHeader     * sIndexHeader;
    smSCN                sSCN;
    smiColumn          * sColumn;
    smxTrans           * sTrans;
    smxSavepoint       * sISavepoint = NULL;
    UInt                 sDummy = 0;
    UInt                 sState = 0 ;
    UInt                 i;

    if ( aMemory->useExternalMemory() == ID_TRUE )
    {
        /* BUG-43006 FixedTable Indexing Filter */
        sTrans = (smxTrans*)((smiFixedTableProperties *)aMemory->getContext())->mTrans;
    }
    else
    {
        sTrans = (smxTrans*)((smiIterator*)aMemory->getContext())->trans;
    }
    IDE_TEST( sTrans->setImpSavepoint( &sISavepoint,
                                       sDummy )
             != IDE_SUCCESS );

    IDE_TEST( buildSystemRecord( aHeader,
                                 aMemory )
              != IDE_SUCCESS );

    IDE_TEST( smcRecord::nextOIDall(
                                (smcTableHeader *)smmManager::m_catTableHeader,
                                NULL, 
                                &sNxtRowPtr )
              != IDE_SUCCESS );

    while( sNxtRowPtr != NULL )
    {
        sSlotHeader = (smpSlotHeader *)sNxtRowPtr;
        sTable      = (smcTableHeader *)(sSlotHeader + 1);
        SM_GET_SCN( (smSCN*)&sSCN,
                    (smSCN*)&sSlotHeader->mCreateSCN );

        if( ( SM_SCN_IS_INFINITE(sSCN) == ID_FALSE ) &&
            ( sTable->mType == SMC_TABLE_NORMAL ) &&
            ( smcTable::isDropedTable(sTable) == ID_FALSE) &&
            ( sctTableSpaceMgr::hasState( sTable->mSpaceID,
                                          SCT_SS_INVALID_DISK_TBS ) 
              == ID_FALSE ) )
        {
            IDE_TEST( lock4GatherStat( (void*)sTrans, 
                                       sTable,
                                       ID_TRUE ) != IDE_SUCCESS );
            sState = 1;

            IDE_TEST( buildTableRecord( sTable,
                                        aHeader,
                                        aMemory )
                      != IDE_SUCCESS );

            for( i = 0 ; i < sTable->mColumnCount ; i ++ )
            {
                sColumn = (smiColumn*)smcTable::getColumn( sTable, i );
                IDE_TEST( buildColumnRecord( sTable,
                                             sColumn,
                                             aHeader,
                                             aMemory )
                          != IDE_SUCCESS );
            }
            for( i = 0 ; i < smcTable::getIndexCount( sTable ) ; i ++ )
            {
                sIndexHeader = 
                    (smnIndexHeader*) smcTable::getTableIndex( sTable, i );

                IDE_TEST( buildIndexRecord( sTable,
                                            sIndexHeader,
                                            aHeader,
                                            aMemory )
                          != IDE_SUCCESS );
            }

            sState = 0;
            IDE_TEST( unlock4GatherStat( sTable ) != IDE_SUCCESS );

        }

        sCurRowPtr = sNxtRowPtr;

        IDE_TEST( smcRecord::nextOIDall(
                      (smcTableHeader *)smmManager::m_catTableHeader,
                      sCurRowPtr,
                      &sNxtRowPtr )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sTrans->abortToImpSavepoint( sISavepoint )
              != IDE_SUCCESS );
    IDE_TEST( sTrans->unsetImpSavepoint( sISavepoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        unlock4GatherStat( sTable );
    default:
        break;
    }

    return IDE_FAILURE;
}

static iduFixedTableColDesc  gDBMSStatColDesc[]=
{
    {
        (SChar*)"DATE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mCreateTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mCreateTime),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SAMPLE_SIZE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mSampleSize),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mSampleSize),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NUM_ROW_CHANGE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mNumRowChange),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mNumRowChange),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"TYPE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mType),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mType),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"SREAD_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mSReadTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mSReadTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MREAD_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mMReadTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mMReadTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MREAD_PAGE_COUNT",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mDBFileMultiPageReadCount),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mDBFileMultiPageReadCount),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"HASH_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mHashTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mHashTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"COMPARE_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mCompareTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mCompareTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"STORE_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mStoreTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mStoreTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"TARGET_ID",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mTargetID),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mTargetID),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {

        (SChar*)"COLUMN_ID",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mColumnID),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mColumnID),
        IDU_FT_TYPE_UINTEGER,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NUM_ROW",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mNumRow),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mNumRow),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NUM_PAGE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mNumPage),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mNumPage),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NUM_DIST",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mNumDist),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mNumDist),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"NUM_NULL",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mNumNull),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mNumNull),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"AVG_LEN",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mAvgLen),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mAvgLen),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"ONE_ROW_READ_TIME",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mOneRowReadTime),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mOneRowReadTime),
        IDU_FT_TYPE_DOUBLE,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"AVG_SLOT_COUNT",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mAvgSlotCnt),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mAvgSlotCnt),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"INDEX_HEIGHT",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mIndexHeight),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mIndexHeight),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"CLUSTERING_FACTOR",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mClusteringFactor),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mClusteringFactor),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MIN",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mMinValue),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mMinValue),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"MAX",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mMaxValue),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mMaxValue),
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"META_SPACE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mMetaSpace),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mMetaSpace),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"USED_SPACE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mUsedSpace),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mUsedSpace),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"AGEABLE_SPACE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mAgableSpace),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mAgableSpace),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        (SChar*)"FREE_SPACE",
        IDU_FT_OFFSETOF(smiDBMSStat4Perf,mFreeSpace),
        IDU_FT_SIZEOF(smiDBMSStat4Perf,mFreeSpace),
        IDU_FT_TYPE_UBIGINT,
        NULL,
        0, 0,NULL // for internal use
    },
    {
        NULL,
        0,
        0,
        IDU_FT_TYPE_CHAR,
        NULL,
        0, 0,NULL // for internal use
    }
};

// X$DBMS_STATS
iduFixedTableDesc  gDBMSStatsDesc=
{
    (SChar *)"X$DBMS_STATS",
    smiStatistics::buildDBMSStatRecord,
    gDBMSStatColDesc,
    IDU_STARTUP_META,
    0,
    0,
    IDU_FT_DESC_TRANS_USE,
    NULL
};

/******************************************************************
 * DESCRIPTION : 
 *    BUG-44814 DDL ����� ���̺��� ������Ǵ� ��� ������ ��� ������ DDL ���������� �����ؾ� �մϴ�. 
 *              ���̺�, �÷� ��������� �����ϴ� �Լ�
 *
 *  aDstTable               - [IN]  ��� ���̺�
 *  aSrcTable               - [IN]  �ҽ� ���̺�
 *  aSkipColumn             - [IN]  skip�� �÷� ���� array
 *                                  ������� NULL �� ����
 *  aSkipCnt                - [IN]  aSkipColumn array ����
 ******************************************************************/
void smiStatistics::copyTableStats( const void * aDstTable, const void * aSrcTable, UInt * aSkipColumn, UInt aSkipCnt )
{
    smcTableHeader  * sDstHeader;
    smcTableHeader  * sSrcHeader;
    smiColumn       * sDstColumn;
    smiColumn       * sSrcColumn;
    UInt              i;
    UInt              sDstID;
    UInt              sSrcID;
    UInt              sRemainCnt;

    sDstHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aDstTable);
    sSrcHeader = (smcTableHeader*)SMI_MISC_TABLE_HEADER(aSrcTable);

    sRemainCnt = IDL_MIN( sDstHeader->mColumnCount, sSrcHeader->mColumnCount );

    idlOS::memcpy( &sDstHeader->mStat, &sSrcHeader->mStat, ID_SIZEOF(smiTableStat) );

    sDstID = 0;
    sSrcID = 0;

    while ( 1 )
    {
        /* alter table remove column ����� �� ��쿡 aSkipColumn �� �����ȴ�.
           ���� aSrcTable �� �͸� skip �ϸ� �ȴ�. */
        for ( i = 0; i < aSkipCnt; ++i )
        {
            if ( aSkipColumn[i] == sSrcID )
            {
                break;
            }
            else
            {
                // nothing todo.
            }
        }

        if ( i == aSkipCnt )
        {
            sDstColumn = (smiColumn*)smcTable::getColumn( SMI_MISC_TABLE_HEADER(aDstTable), sDstID );
            sSrcColumn = (smiColumn*)smcTable::getColumn( SMI_MISC_TABLE_HEADER(aSrcTable), sSrcID );

            idlOS::memcpy( &sDstColumn->mStat, &sSrcColumn->mStat, ID_SIZEOF(smiColumnStat) );
            
            ++sDstID;
            ++sSrcID;
            --sRemainCnt;

            if ( sRemainCnt == 0 )
            {
                break;
            }
            else
            {
                // nothing todo.
            }
        }
        else
        {
            ++sSrcID;
        }
    }
}

/******************************************************************
 * DESCRIPTION : 
 *    BUG-44814 DDL ����� ���̺��� ������Ǵ� ��� ������ ��� ������ DDL ���������� �����ؾ� �մϴ�. 
 *              index ��������� �����ϴ� �Լ�
 *
 *  aDstIndex               - [IN]  ��� �ε���
 *  aSrcIndex               - [IN]  �ҽ� �ε���
 ******************************************************************/
void smiStatistics::copyIndexStats( const void * aDstIndex, const void * aSrcIndex )
{
    smnIndexHeader * sDstHeader;
    smnIndexHeader * sSrcHeader;
    
    sDstHeader = (smnIndexHeader*)aDstIndex;
    sSrcHeader = (smnIndexHeader*)aSrcIndex;
    
    idlOS::memcpy( &sDstHeader->mStat, &sSrcHeader->mStat, ID_SIZEOF(smiIndexStat) );
}

/******************************************************************
 * DESCRIPTION : 
 *    BUG-44964 �÷� min, max ��������� �����ϴ� �Լ�
 *
 *  aTable                  - [IN]  ��� ���̺�
 *  aColumnID               - [IN]  ��� �÷� id
 ******************************************************************/
IDE_RC smiStatistics::removeColumnMinMax( const void * aTable, UInt aColumnID )
{
    smiColumn   * sColumn;
    smiNullFunc   sSetNull;

    sColumn = (smiColumn*)smcTable::getColumn( SMI_MISC_TABLE_HEADER(aTable), aColumnID );

    IDE_TEST( gSmiGlobalCallBackList.findNull( sColumn,
                                               sColumn->flag,
                                               &sSetNull )
              != IDE_SUCCESS );

    sSetNull( sColumn, sColumn->mStat.mMinValue );
    sSetNull( sColumn, sColumn->mStat.mMaxValue );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
