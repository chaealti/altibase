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
 * $Id: smaRefineDB.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <idl.h>
#include <smm.h>
#include <smc.h>
#include <smn.h>
#include <smp.h>
#include <smaDef.h>
#include <smaRefineDB.h>
#include <sdd.h>
#include <sct.h>
#include <svcRecord.h>

IDE_RC smaRefineDB::refineTempCatalogTable(smxTrans       * aTrans,
                                           smcTableHeader * aHeader )
{

    IDE_TEST( smcTable::initLockAndRuntimeItem( aHeader ) != IDE_SUCCESS );

    // temp catalog table�� �Ҵ�� ��� page�� �ý��ۿ� ��ȯ�Ѵ�.
    IDE_TEST(smcTable::dropTablePageListPending(aTrans,
                                                aHeader,
                                                ID_TRUE)
             != IDE_SUCCESS);

    // ��� temp tablespace�� �ʱ�ȭ��Ų��.
    IDE_TEST( sctTableSpaceMgr::resetAllTempTBS((void*)aTrans)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
/*
    Catalog Table��  Slot�� Free�ϰ� Free Slot List�� �Ŵܴ�.

    [IN] aTrans   - slot�� free�Ϸ��� transaction
    [IN] aSlotPtr - slot�� ������
 */
IDE_RC smaRefineDB::freeCatalogSlot( smxTrans  * aTrans,
                                     SChar     * aSlotPtr )
{
    scPageID sPageID = SMP_SLOT_GET_PID(aSlotPtr);

    IDE_TEST( smpFixedPageList::setFreeSlot(
                  aTrans,
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  sPageID,
                  aSlotPtr,
                  SMP_TABLE_NORMAL) != IDE_SUCCESS );

    smpFixedPageList::addFreeSlotToFreeSlotList(
        smpFreePageList::getFreePageHeader(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
            sPageID),
        aSlotPtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaRefineDB::refineCatalogTableVarPage(smxTrans       * aTrans,
                                              smcTableHeader * aHeader)
{
    SChar           * sCurPtr;
    SChar           * sNxtPtr;
    smOID             sCurPieceOID;
    smOID             sNxtPieceOID;
    smVCPieceHeader * sVCPieceHeaderPtr;
    scPageID          sPageID;
    UInt              sIdx;
    UInt              i;
    UInt              sIndexHeaderSize;
    SChar           * sSrc;
    scGRID          * sIndexSegGRID;
    void            * sPagePtr;


    /* ----------------------------
     * [1] Table�� LockItem�� �� Mutex��
     * �ʱ�ȭ�Ѵ�.
     * ---------------------------*/
    IDE_TEST( smcTable::initLockAndRuntimeItem( aHeader )
              != IDE_SUCCESS );

    /* ----------------------------
     * [2] Table�� Variable ������ ���� �о�
     * ����ȭ(Obsolete)�� ���� �����Ѵ�.
     * ---------------------------*/

    sCurPtr = NULL;
    sCurPieceOID = SM_NULL_OID;

    sIndexHeaderSize = smnManager::getSizeOfIndexHeader();

    while(1)
    {
        IDE_TEST( smpVarPageList::nextOIDallForRefineDB( aHeader->mSpaceID,
                                                         aHeader->mVar.mMRDB,
                                                         sCurPieceOID,
                                                         sCurPtr,
                                                         &sNxtPieceOID,
                                                         &sNxtPtr,
                                                         &sIdx)
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }

        sVCPieceHeaderPtr = (smVCPieceHeader *)sNxtPtr;
        sSrc              = sNxtPtr + ID_SIZEOF(smVCPieceHeader);

        if( SM_VCPIECE_IS_DISK_INDEX( sVCPieceHeaderPtr->flag) )
        {
            for( i = 0; i < sVCPieceHeaderPtr->length; i += sIndexHeaderSize,
                     sSrc += sIndexHeaderSize )
            {
                /* PROJ-2433
                 * smnIndexHeader ����ü�� mDropFlag�� UInt->UShort�� ���� */
                if ( (smnManager::getIndexDropFlag( sSrc ) == (UShort)SMN_INDEX_DROP_FALSE ) &&
                     (smnManager::isIndexEnabled( sSrc ) == ID_TRUE ) )
                {
                    continue;
                }
                else
                {
                    /* nothing to do */
                }

                /* BUG-33803 ALL INDEX DISABLE pending ���� ���� ���� ������
                 * �����, ���� �籸�� �� �ٷ� ��� table�� DROP �ϸ�,
                 * mHeader�� �����Ⱚ�� ����ִ� ���¿��� index drop�� �õ��Ͽ�
                 * segmentation fault�� �߻��Ѵ�. ���� disable ������ index��
                 * refine �������� mHeader�� NULL�� ������ �ش�. */
                ((smnIndexHeader*)sSrc)->mHeader = NULL;
                sIndexSegGRID = smnManager::getIndexSegGRIDPtr(sSrc);

                if(SC_GRID_IS_NOT_NULL(*sIndexSegGRID))
                {
                    // xxxx �ּ�
                    IDE_TEST( sdpSegment::freeIndexSeg4Entry(
                                  NULL,
                                  SC_MAKE_SPACE( *sIndexSegGRID ),
                                  aTrans,
                                  sNxtPieceOID + i + ID_SIZEOF(smVCPieceHeader),
                                  SDR_MTX_LOGGING)
                              != IDE_SUCCESS );
                }
            }
        }

        sPageID = SM_MAKE_PID(sNxtPieceOID);

        /* ----------------------------
         * Variable Slot�� Delete Flag��
         * ������ ��� ����ȭ�� Row�̴�.
         * ---------------------------*/
        IDE_ASSERT( ( sVCPieceHeaderPtr->flag & SM_VCPIECE_FREE_MASK )
                    == SM_VCPIECE_FREE_NO );

        IDE_ASSERT( smmManager::getPersPagePtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                sPageID,
                                                &sPagePtr )
                    == IDE_SUCCESS );
        sIdx = smpVarPageList::getVarIdx( sPagePtr );
        (aHeader->mVar.mMRDB[sIdx].mRuntimeEntry->mInsRecCnt)++;

        sCurPieceOID = sNxtPieceOID;
        sCurPtr      = sNxtPtr;
    }

    for( i = 0; i < SM_VAR_PAGE_LIST_COUNT ; i++ )
    {
        // FreePageList[0]���� N���� FreePageList�� FreePage���� �����ְ�
        smpFreePageList::distributePagesFromFreePageList0ToTheOthers( (&(aHeader->mVar.mMRDB[i])) );

        // EmptyPage(������������ʴ� FreePage)�� �ʿ��̻��̸�
        // FreePagePool�� �ݳ��ϰ� FreePagePool���� �ʿ��̻��̸�
        // DB�� �ݳ��Ѵ�.
        IDE_TEST(smpFreePageList::distributePagesFromFreePageList0ToFreePagePool(
                     aTrans,
                     SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                     &(aHeader->mVar.mMRDB[i]) )
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smaRefineDB::refineCatalogTableFixedPage(smxTrans       * aTrans,
                                                smcTableHeader * aHeader)
{

    smpSlotHeader  * sSlotHeaderPtr;
    smcTableHeader * sHeader;
    SChar          * sCurPtr;
    SChar          * sNxtPtr;
    smSCN            sSCN;
    UInt             sIndexCount;
    UInt             sRecordCount = 0;
    scPageID         sCurPageID = SM_NULL_PID;
    scPageID         sPrvPageID = SM_NULL_PID;

    /* ----------------------------
     * [1] Table�� Fixed ������ ���� �о�
     * ����ȭ(Obsolete)�� ���� �����Ѵ�.
     * ---------------------------*/

    sCurPtr = NULL;

    while(1)
    {
        IDE_TEST( smpFixedPageList::nextOIDallForRefineDB(
                      aHeader->mSpaceID,
                      &(aHeader->mFixed.mMRDB),
                      sCurPtr,
                      & sNxtPtr,
                      & sCurPageID )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }

        sSlotHeaderPtr = (smpSlotHeader *)sNxtPtr;

        sSCN = sSlotHeaderPtr->mCreateSCN;

        /* ----------------------------
         * Table�� Row�� ���°� �ּ���
         * 1) ���Ѵ밡 �ƴϵ簡
         * 2) Drop flag�� �����Ǿ� �־�� �Ѵ�.
         * ---------------------------*/
        /* BUG-14975: Delete Row Alloc�� ��ٷ� Undo�� ASSERT�߻�.
        IDE_ASSERT( SM_SCN_IS_NOT_INFINITE( sSCN ) ||
                    (sSlotHeaderPtr->mDropFlag == SMP_SLOT_DROP_TRUE) );
        */

        /*
         * ----------------------------
         * ������ ��쿡 free Slot
         * 3) delete bit�� �����Ǿ� �ְ� �������� ���� ���
         * (���̺� �����ϴٰ� ������ ��쿡�� DropFlag�� FALSE�̰�,
         *  DELETE BIT�� �����Ǿ� ������ �ִ�.)
         * ---------------------------
         */
        if( SMP_SLOT_IS_NOT_DROP( sSlotHeaderPtr ) &&
            SM_SCN_IS_DELETED( sSCN ) )
        {
            IDE_TEST( freeCatalogSlot(aTrans,
                                      sNxtPtr) != IDE_SUCCESS );

            sCurPtr = sNxtPtr;
            continue;
        }

        sHeader = (smcTableHeader *)(sSlotHeaderPtr + 1);

        if( SMP_SLOT_IS_DROP( sSlotHeaderPtr ) ||
            SM_SCN_IS_DELETED( sSCN ) )
        {
            /* BUG-30378 ������������ Drop�Ǿ����� refine���� �ʴ�
             * ���̺��� �����մϴ�.
             * (CASE-26385)
             *
             * Ab-normal ���� ���ᰡ �ƴ� ���� ���ῴ�ٸ�
             * Used : true, drop : true�� �����̸� Index�� �ִ�
             * ���̺��� �����ؼ��� �ȵȴ�.
             * (����� Sequence���� used:true, drop:true�� ���·�
             *  ������ �� �ִ�. ) */
            sIndexCount = smcTable::getIndexCount( sHeader );

            if( ( smrRecoveryMgr::isABShutDown() == ID_FALSE ) &&
                ( sIndexCount != 0 ) )
            {
                ideLog::log(
                    IDE_SERVER_0,
                    "InternalError [%s:%u]\n"
                    "Invalid table header.\n"
                    "IndexCount : %u\n",
                    (SChar *)idlVA::basename(__FILE__) ,
                    __LINE__ ,
                    sIndexCount );
                smpFixedPageList::dumpSlotHeader( sSlotHeaderPtr );
                smcTable::dumpTableHeader( sHeader );

                // ����� ����϶��� ������ ���δ�.
                IDE_DASSERT( 0 );
            }


            // memory ,�Ǵ�  disk table drop pending operation��
            // �ϰ�, table header slot�� free�Ѵ�.

            IDE_ERROR( (sHeader->mFlag & SMI_TABLE_TYPE_MASK) !=
                       SMI_TABLE_TEMP_LEGACY );
            if( (sHeader->mFlag & SMI_TABLE_TYPE_MASK) != SMI_TABLE_FIXED )
            {
                IDE_TEST( freeTableHdr( aTrans,
                                        sHeader,
                                        SMP_SLOT_GET_FLAGS( sSlotHeaderPtr ),
                                        sNxtPtr )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            sRecordCount++;
            sIndexCount = smcTable::getIndexCount(aHeader);
            IDE_ASSERT( sIndexCount == 0);

            smcTable::addToTotalIndexCount( smcTable::getIndexCount(sHeader) );

            /*
            * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
            * ��ȿ�� ���ڵ尡 �ִٸ� Scan List�� �߰��Ѵ�.
            */
            if( sCurPageID != sPrvPageID )
            {
                IDE_TEST( smpFixedPageList::linkScanList(
                                                aHeader->mSpaceID,
                                                sCurPageID,
                                                &(aHeader->mFixed.mMRDB) )
                          != IDE_SUCCESS );
                sPrvPageID = sCurPageID;
            }
        }

        sCurPtr = sNxtPtr;
    }

    smpFreePageList::distributePagesFromFreePageList0ToTheOthers(
                                                  &(aHeader->mFixed.mMRDB) );

    IDE_TEST(smpFreePageList::distributePagesFromFreePageList0ToFreePagePool(
                                             aTrans,
                                             aHeader->mSpaceID,
                                             &(aHeader->mFixed.mMRDB) )
             != IDE_SUCCESS);


    if(sRecordCount != 0)
    {
        IDE_TEST(smmManager::initSCN() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaRefineDB::freeTableHdr( smxTrans       *aTrans,
                                  smcTableHeader *aTableHdr,
                                  ULong           aSlotFlag,
                                  SChar          *aNxtPtr )
{
    scPageID sPageID;

    IDE_DASSERT( aTableHdr != NULL );

    // Refine SKIP�� Tablespace���� üũ
    if ( sctTableSpaceMgr::hasState( aTableHdr->mSpaceID,
                                     SCT_SS_SKIP_REFINE ) == ID_TRUE )
    {
        // fix BUG-17784  ���� ������ drop �������� disk table header��
        // refine �� free�Ǿ� ����Ǵ� ��� �߻�

        // DROP/OFFLINE/DISCARD�� Tablespace�� ���
        // Catalog Table Slot�� �ݳ��Ѵ�.
        IDE_TEST( freeCatalogSlot( aTrans,
                                   aNxtPtr ) != IDE_SUCCESS );
    }
    else
    {
        if( ( aSlotFlag & SMP_SLOT_DROP_MASK )
            == SMP_SLOT_DROP_TRUE )
        {
            IDE_TEST( smcTable::dropTablePending( NULL,
                                                  aTrans,
                                                  aTableHdr,
                                                  ID_FALSE )
                      != IDE_SUCCESS );

            IDE_TEST( smcTable::finLockAndRuntimeItem(aTableHdr)
                      != IDE_SUCCESS );
        }

        sPageID = SMP_SLOT_GET_PID(aNxtPtr);

        IDE_TEST(smpFixedPageList::setFreeSlot(aTrans,
                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                    sPageID,
                    aNxtPtr,
                    SMP_TABLE_NORMAL)
                != IDE_SUCCESS );

        smpFixedPageList::addFreeSlotToFreeSlotList(
                smpFreePageList::getFreePageHeader(
                    SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                    sPageID),
                aNxtPtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaRefineDB::refineTable(smxTrans            * aTrans,
                                smapRebuildJobItem  * aJobItem)
{
    UInt    sTableType = SMN_GET_BASE_TABLE_TYPE(aJobItem->mTable->mFlag);
    UInt    sTableOID;
    smrRTOI sRTOI;

    sTableOID = aJobItem->mTable->mSelfOID;

    smrRecoveryMgr::initRTOI( &sRTOI );
    sRTOI.mCause    = SMR_RTOI_CAUSE_PROPERTY;
    sRTOI.mType     = SMR_RTOI_TYPE_TABLE;
    sRTOI.mTableOID = sTableOID;
    sRTOI.mState    = SMR_RTOI_STATE_CHECKED;

    if( smrRecoveryMgr::isIgnoreObjectByProperty( &sRTOI ) == ID_TRUE )
    {
        /* PROJ-2162 RestartRiskReduction 
         * Table�� Consistent�Ҷ����� Refine�� ������ */
        IDE_CALLBACK_SEND_SYM("S");
        IDE_TEST( smrRecoveryMgr::startupFailure( &sRTOI, 
                                                  ID_FALSE )  // is redo
                  != IDE_SUCCESS);
        IDE_CONT( SKIP );
    }
    else
    {
        /* nothing to do ... */
    }

    if( smcTable::isTableConsistent( (void*)aJobItem->mTable ) == ID_FALSE )
    {
        /* PROJ-2162 RestartRiskReduction 
         * Table�� Consistent�Ҷ����� Refine�� ������ */
        smrRecoveryMgr::initRTOI( &sRTOI );
        sRTOI.mCause    = SMR_RTOI_CAUSE_OBJECT;
        sRTOI.mType     = SMR_RTOI_TYPE_TABLE;
        sRTOI.mTableOID = sTableOID;
        sRTOI.mState    = SMR_RTOI_STATE_CHECKED;
        IDE_CALLBACK_SEND_SYM("F");
        IDE_TEST( smrRecoveryMgr::startupFailure( &sRTOI, 
                                                  ID_FALSE )  // is redo
                  != IDE_SUCCESS);
        IDE_CONT( SKIP );
    }
    else
    {
        /* nothing to do ... */
    }

    /* PROJ-2162 RestartRiskReduction
     * Refine ���и� �����, Refine�ϴ� Table�� OID�� ����� */
    ideLog::log( IDE_SM_0,
                 "====================================================\n"
                 " [MRDB_TBL_REFINE_BEGIN] TABLEOID : %llu\n"
                 "====================================================",
                 sTableOID );

    if( smpFixedPageList::refinePageList( aTrans,
                aJobItem->mTable->mSpaceID,
                sTableType,
                &(aJobItem->mTable->mFixed.mMRDB) )
            != IDE_SUCCESS)
    {
        IDE_TEST( smrRecoveryMgr::refineFailureWithTable( sTableOID ) 
                  != IDE_SUCCESS );
        IDE_CONT( SKIP );
    }
    else
    {
        /* nothing to do ... */
    }

    if(smpVarPageList::refinePageList( 
                aTrans,
                aJobItem->mTable->mSpaceID,
                aJobItem->mTable->mVar.mMRDB )
            != IDE_SUCCESS)
    {
        IDE_TEST( smrRecoveryMgr::refineFailureWithTable( sTableOID ) 
                  != IDE_SUCCESS );
        IDE_CONT( SKIP );
    }
    else
    {
        /* nothing to do ... */
    }

    ideLog::log( IDE_SM_0,
                 "====================================================\n"
                 " [MRDB_TBL_REFINE_END]   TABLEOID : %llu\n"
                 "====================================================",
                sTableOID );

    IDE_CALLBACK_SEND_SYM(".");

    IDE_EXCEPTION_CONT( SKIP );

    aJobItem->mFinished = ID_TRUE;

    aJobItem->mTable->mSequence.mCurSequence =
        aJobItem->mTable->mSequence.mLstSyncSequence;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description
 *    ��� volatile table���� �ʱ�ȭ�Ѵ�.
 *    �� �Լ��� Server startup service �ܰ迡�� ȣ��Ǿ�� �Ѵ�.
 *    �ֳ��ϸ� makeNullRow callback ȣ��� mt ����� �ʱ�ȭ�Ǿ� �־��
 *    �ϱ� �����̴�.
 ************************************************************************/
IDE_RC smaRefineDB::initAllVolatileTables()
{
    SChar          *sNxtTablePtr;
    SChar          *sCurTablePtr;
    smcTableHeader *sCurTable;
    smpSlotHeader  *sSlot;
    smSCN           sSCN;
    smxTrans       *sTrans;
    smSCN           sInfiniteSCN;
    SLong           sValueBuffer[SM_PAGE_SIZE/ID_SIZEOF(SLong)];
    smiColumnList   sColList[SMI_COLUMN_ID_MAXIMUM];
    smiValue        sNullRow[ID_SIZEOF(smiValue) * SMI_COLUMN_ID_MAXIMUM];
    smOID           sNullRowOID;
    smnIndexHeader *sRebuildIndexList[SMC_INDEX_TYPE_COUNT];


    SM_INIT_SCN( &sInfiniteSCN );

    IDE_TEST(smxTransMgr::alloc((smxTrans**)&sTrans) != IDE_SUCCESS);
    IDE_ASSERT( sTrans->begin(NULL,
                              ( SMI_TRANSACTION_REPL_NONE |
                                SMI_COMMIT_WRITE_NOWAIT ),
                              SMX_NOT_REPL_TX_ID )
             == IDE_SUCCESS);

    sCurTablePtr = NULL;

    while (ID_TRUE)
    {
        IDE_TEST(smcRecord::nextOIDall((smcTableHeader *)smmManager::m_catTableHeader,
                                       sCurTablePtr,
                                       &sNxtTablePtr)
                 != IDE_SUCCESS);

        if (sNxtTablePtr != NULL)
        {
            sSlot     = (smpSlotHeader *)sNxtTablePtr;
            sCurTable = (smcTableHeader *)(sSlot + 1);
            SM_GET_SCN( &sSCN, &(sSlot)->mCreateSCN );

            sCurTablePtr = sNxtTablePtr;

            if( SMI_TABLE_TYPE_IS_VOLATILE( sCurTable ) == ID_FALSE )
            {
                continue;
            }

            /* BUGBUG �Ʒ� �� if ������ �������� �Ѵ�.
               �ֳ��ϸ� �� �Լ��� ȣ��Ǿ��� �ÿ���
               īŻ�α� ���̺��� refine ������ ���Ʊ⶧����
               drop�� TBS�� Table���� ��� ó���� �Ǿ���. */
            if ( sctTableSpaceMgr::hasState( sCurTable->mSpaceID,
                                             SCT_SS_SKIP_REFINE ) == ID_TRUE )
            {
                continue;
            }
            if( SMP_SLOT_IS_DROP( sSlot ) )
            {
                continue;
            }
            if( SM_SCN_IS_DELETED( sSCN ) )
            {
                continue;
            }

            /* Table header�� page list entry �ʱ�ȭ */
            IDE_TEST( smcTable::initLockAndRuntimeItem( sCurTable )
                      != IDE_SUCCESS );

            /* NULL ROW ������ */
            IDE_TEST(smcTable::makeNullRowByCallback(sCurTable,
                                                     sColList,
                                                     sNullRow,
                                                     (SChar*)sValueBuffer)
                     != IDE_SUCCESS);

            /* �� ���̺� NULL ROW �����ϱ� */
            IDE_TEST(svcRecord::makeNullRow(sTrans,
                                            sCurTable,
                                            sInfiniteSCN,
                                            (const smiValue *)sNullRow,
                                            SM_FLAG_MAKE_NULLROW,
                                            &sNullRowOID)
                     != IDE_SUCCESS);

            IDE_TEST( smcTable::setNullRow( sTrans,
                                            sCurTable,
                                            SMI_TABLE_VOLATILE,
                                            &sNullRowOID )
                      != IDE_SUCCESS );

            /* Index �ʱ�ȭ �����ϱ� */
            IDE_TEST(smnManager::prepareRebuildIndexs(sCurTable,
                                                      sRebuildIndexList)
                     != IDE_SUCCESS);
         }
         else
         {
             break;
         }
    }

    IDE_TEST(sTrans->commit() != IDE_SUCCESS);
    IDE_TEST(smxTransMgr::freeTrans(sTrans) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : BUG-24518 [MDB] Shutdown Phase���� �޸� ���̺�
 *               Compaction�� �ʿ��մϴ�.
 *
 * �����ͺ��̽����� ��� �޸�/��Ÿ ���̺� ���ؼ� Compaction�� �����Ͽ�
 * ���� server start�� VSZ�� �������� �ʵ��� ���´�.
 ************************************************************************/
IDE_RC smaRefineDB::compactTables( void )
{

    smpSlotHeader  * sSlotHeaderPtr;
    smcTableHeader * sTableHeader;
    smcTableHeader * sCatalogHeader;
    SChar          * sCurPtr;
    SChar          * sNxtPtr;
    smSCN            sSCN;
    smxTrans       * sTrans = NULL;


    IDE_TEST( smxTransMgr::alloc( (smxTrans**)&sTrans ) != IDE_SUCCESS );
    IDE_ASSERT( sTrans->begin( NULL,
                               ( SMI_TRANSACTION_REPL_NONE |
                                 SMI_COMMIT_WRITE_NOWAIT ),
                               SMX_NOT_REPL_TX_ID )
              == IDE_SUCCESS );

    sCatalogHeader = (smcTableHeader *)smmManager::m_catTableHeader;

    sCurPtr = NULL;

    /*
     * ��� ���̺� ���ؼ�
     */
    while(1)
    {
        IDE_TEST( smcRecord::nextOIDall( sCatalogHeader,
                                         sCurPtr,
                                         & sNxtPtr )
                  != IDE_SUCCESS );

        if( sNxtPtr == NULL )
        {
            break;
        }

        sSlotHeaderPtr = (smpSlotHeader *)sNxtPtr;
        sSCN = sSlotHeaderPtr->mCreateSCN;
        sTableHeader = (smcTableHeader *)(sSlotHeaderPtr + 1);
        sCurPtr = sNxtPtr;

        /*
         * INCONSISTENT ���̺����̽��� Skip
         */
        if ( sctTableSpaceMgr::hasState( sTableHeader->mSpaceID,
                                         SCT_SS_SKIP_REFINE ) == ID_TRUE )
        {
            continue;
        }

        /*
         * ����ڿ� ���ؼ� ������ ���̺��� Skip
         */
        if( SMP_SLOT_IS_DROP( sSlotHeaderPtr ) )
        {
            continue;
        }

        /*
         * ���̺� �����ϴٰ� �ѹ�� ���� Skip
         */
        if( SM_SCN_IS_DELETED( sSCN ) )
        {
            continue;
        }

        /*
         * �޸��̰ų� ��Ÿ ���̺� ���ؼ��� Compaction�� �����Ѵ�.
         */
        if( (SMI_TABLE_TYPE_IS_MEMORY( sTableHeader ) == ID_TRUE) ||
            (SMI_TABLE_TYPE_IS_META( sTableHeader )   == ID_TRUE) )
        {
            IDE_TEST( smcTable::compact( sTrans, 
                                         sTableHeader, 
                                         0 )   /* # of page (0:ALL) */ 
                      != IDE_SUCCESS );
        }
    }

    IDE_TEST( sTrans->commit() != IDE_SUCCESS );
    IDE_TEST( smxTransMgr::freeTrans( sTrans )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sTrans != NULL)
    {
        sTrans->abort( ID_FALSE, /* aIsLegacyTrans */
                       NULL      /* aLegacyTrans */ );
        smxTransMgr::freeTrans( sTrans);
    }

    return IDE_FAILURE;
}
