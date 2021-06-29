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
 

#include <smDef.h>
#include <smErrorCode.h>
#include <smcTable.h>
#include <sdcReq.h>
#include <sdc.h>
#include <sdp.h>
#include <sdrMtxStack.h>
#include <smxTrans.h>

/*
 *   *** DML Log ���� ***
 *
 * DML Log�� 5���� ����(layer)���� �����Ǿ� �ִ�.
 *
 * ------------------------------------         --
 * | 1. Log Header Layer               |         |
 * ------------------------------------   --     |
 * | 2. Undo Info Layer(only Undo)     |   |     |
 * ------------------------------------    |     |
 * | 3. Update Info Layer(only Update) |   |(1)  |(2)
 * ------------------------------------    |     |
 * | 4. Row Image Layer                |   |     |
 * ------------------------------------   --     |
 * | 5. RP Info Layer(only RP)         |         |
 * ------------------------------------         --
 *
 * (1) - undo page�� ����ϴ� ����
 *
 * (2) - log file�� ����ϴ� ����
 *
 *
 *
 *   *** DML Log Type ***
 *
 *   SDR_SDC_INSERT_ROW_PIECE,
 *   SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE,
 *   SDR_SDC_UPDATE_ROW_PIECE,
 *   SDR_SDC_OVERWRITE_ROW_PIECE,
 *   SDR_SDC_CHANGE_ROW_PIECE_LINK,
 *   SDR_SDC_DELETE_FIRST_COLUMN_PIECE,
 *   SDR_SDC_ADD_FIRST_COLUMN_PIECE,
 *   SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE,
 *   SDR_SDC_DELETE_ROW_PIECE,
 *   SDR_SDC_LOCK_ROW,
 *
 *
 *
 *   *** Undo Record Type ***
 *
 *   SDC_UNDO_INSERT_ROW_PIECE
 *   SDC_UNDO_INSERT_ROW_PIECE_FOR_UPDATE
 *   SDC_UNDO_UPDATE_ROW_PIECE
 *   SDC_UNDO_OVERWRITE_ROW_PIECE
 *   SDC_UNDO_CHANGE_ROW_PIECE_LINK
 *   SDC_UNDO_DELETE_FIRST_COLUMN_PIECE
 *   SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE
 *   SDC_UNDO_DELETE_ROW_PIECE
 *   SDC_UNDO_LOCK_ROW
 *
 *
 *
 *   SDR_SDC_INSERT_ROW_PIECE,
 *   SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_INSERT_ROW_PIECE,
 *   SDC_UNDO_INSERT_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_UPDATE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)], [column count(2)],
 *   | [column desc set size(1)], [column desc set(1~128)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_UPDATE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)], [column count(2)],
 *   | [column desc set size(1)], [column desc set(1~128)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_OVERWRITE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_OVERWRITE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_CHANGE_ROW_PIECE_LINK
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4), next slotnum(2)]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_CHANGE_ROW_PIECE_LINK
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4), next slotnum(2)]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_DELETE_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_DELETE_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [column len(1 or 3),column data]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [column seq(2),column id(4)]
 *   -----------------------------------------------------------------------------------
 *
 *   SDR_SDC_ADD_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [column len(1 or 3),column data]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_DELETE_ROW_PIECE,
 *   SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_DELETE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *
 *
 *   SDR_SDC_LOCK_ROW
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 *   SDC_UNDO_LOCK_ROW
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 * */

/***********************************************************************
 *
 * Description : ���̺� �������� ��ġ�� ȹ���ϸ鼭 CTS�� �Ҵ��ϱ⵵ �Ѵ�.
 *
 * aStatistics   - [IN]  �������
 * aMtx          - [IN]  �̴� Ʈ����� ������
 * aSpaceID      - [IN]  ���̺����̽� ID
 * aRowSID       - [IN]  Row�� SlotID
 * aPageReadMode - [IN]  ������ �б��� (Single or Multiple)
 * aTargetRow    - [OUT] RowPiece�� ������
 * aCTSlotIdx    - [OUT] �Ҵ���� CTS ����
 *
 **********************************************************************/
IDE_RC sdcRow::prepareUpdatePageByPID( idvSQL           * aStatistics,
                                       sdrMtx           * aMtx,
                                       scSpaceID          aSpaceID,
                                       scPageID           aPageID,
                                       sdbPageReadMode    aPageReadMode,
                                       UChar           ** aPagePtr,
                                       UChar            * aCTSlotIdx )
{
    idBool            sDummy;
    sdrMtxStartInfo   sStartInfo;
    UChar           * sPagePtr = NULL;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aPagePtr != NULL );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aPageID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          aPageReadMode,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL ) != IDE_SUCCESS );

    /* PROJ-2162 Inconsistent page �߰� */
    IDE_TEST_RAISE( sdpPhyPage::isConsistentPage( sPagePtr ) == ID_FALSE,
                    ERR_INCONSISTENT_PAGE );

    if ( aCTSlotIdx != NULL )
    {
        IDE_TEST( sdcTableCTL::allocCTSAndSetDirty(
                                          aStatistics,
                                          aMtx,          /* aFixMtx */
                                          &sStartInfo,   /* for Logging */
                                          (sdpPhyPageHdr*)sPagePtr,
                                          aCTSlotIdx ) != IDE_SUCCESS );
    }

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_PAGE )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aSpaceID,
                                  aPageID ) );
    }
    IDE_EXCEPTION_END;

    *aPagePtr = NULL;

    if ( aCTSlotIdx != NULL )
    {
        *aCTSlotIdx = SDP_CTS_IDX_NULL;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : ���̺� �������� ��ġ�� ȹ���ϸ鼭 CTS�� �Ҵ��ϱ⵵ �Ѵ�.
 *
 * aStatistics   - [IN]  �������
 * aMtx          - [IN]  �̴� Ʈ����� ������
 * aSpaceID      - [IN]  ���̺����̽� ID
 * aRowSID       - [IN]  Row�� SlotID
 * aPageReadMode - [IN]  ������ �б��� (Single or Multiple)
 * aTargetRow    - [OUT] RowPiece�� ������
 * aCTSlotIdx    - [OUT] �Ҵ���� CTS ����
 *
 **********************************************************************/
IDE_RC sdcRow::prepareUpdatePageBySID( idvSQL           * aStatistics,
                                       sdrMtx           * aMtx,
                                       scSpaceID          aSpaceID,
                                       sdSID              aRowSID,
                                       sdbPageReadMode    aPageReadMode,
                                       UChar           ** aTargetRow,
                                       UChar            * aCTSlotIdx )
{
    UChar  * sSlotDirPtr;
    UChar  * sPagePtr;

    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aTargetRow != NULL );

    IDE_TEST( prepareUpdatePageByPID( aStatistics,
                                      aMtx,
                                      aSpaceID,
                                      SD_MAKE_PID(aRowSID),
                                      aPageReadMode,
                                      &sPagePtr,
                                      aCTSlotIdx ) != IDE_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sPagePtr );
    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                       SD_MAKE_SLOTNUM(aRowSID),
                                                       aTargetRow  )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aTargetRow = NULL;

    if ( aCTSlotIdx != NULL )
    {
        *aCTSlotIdx = SDP_CTS_IDX_NULL;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  �ϳ��� row�� data page�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::insert( idvSQL            * aStatistics,
                       void              * aTrans,
                       void              * aTableInfoPtr,
                       void              * aTableHeader,
                       smSCN               aCSInfiniteSCN,
                       SChar            **,//aRetRow
                       scGRID            * aRowGRID,
                       const smiValue    * aValueList,
                       UInt                aFlag )
{
    sdcRowPieceInsertInfo  sInsertInfo;
    UInt                   sTotalColCount;
    UShort                 sTrailingNullCount;
    UShort                 sCurrColSeq = 0;
    UInt                   sCurrOffset = 0;
    idBool                 sReplicate;
    UChar                  sRowFlag;
    UChar                  sNextRowFlag       = SDC_ROWHDR_FLAG_ALL;
    idBool                 sIsUndoLogging;
    idBool                 sNeedUndoRec;
    smOID                  sTableOID;
    sdSID                  sFstRowPieceSID    = SD_NULL_SID;
    sdSID                  sInsertRowPieceSID = SD_NULL_SID;
    sdSID                  sNextRowPieceSID   = SD_NULL_SID;

    IDE_ERROR( aTrans        != NULL );
    IDE_ERROR( aTableInfoPtr != NULL );
    IDE_ERROR( aTableHeader  != NULL );
    IDE_ERROR( aRowGRID      != NULL );
    IDE_ERROR( aValueList    != NULL );

    /* FIT/ART/sm/Projects/PROJ-2118/insert/insert.ts */
    IDU_FIT_POINT( "1.PROJ-2118@sdcRow::insert" );

    if ( SM_UNDO_LOGGING_IS_OK(aFlag) )
    {
        sIsUndoLogging = ID_TRUE;
    }
    else
    {
        sIsUndoLogging = ID_FALSE;
    }

    /* BUG-21866: Disk Table�� Insert�� Insert Undo Reco�� ������� ����.
     *
     * ID_FALSE�̸� Insert�� Undo Rec�� ������� �ʴ´�. Triger�� Foreign Key
     * �� ���� ���� Insert Undo Rec�� ���ʿ��ϴ�. �������� ��� ����
     * gSmiGlobalCallBackList.checkNeedUndoRecord�� ����Ѵ�.
     * */
    if ( SM_INSERT_NEED_INSERT_UNDOREC_IS_OK(aFlag) )
    {
        sNeedUndoRec   = ID_TRUE;
    }
    else
    {
        sNeedUndoRec   = ID_FALSE;
    }

    sTableOID      = smLayerCallback::getTableOID( aTableHeader );
    sTotalColCount = smLayerCallback::getColumnCount( aTableHeader );
    IDE_ASSERT( sTotalColCount > 0 );

    /* qp�� �Ѱ��� valuelist�� ��ȿ���� üũ�Ѵ�. */
    IDE_DASSERT( validateSmiValue( aValueList,
                                   sTotalColCount)
                 == ID_TRUE );

    sReplicate = smLayerCallback::needReplicate( (smcTableHeader*)aTableHeader,
                                                 aTrans );
    if ( smcTable::isTransWaitReplicationTable( (const smcTableHeader*)aTableHeader )
         == ID_TRUE )
    {
        smLayerCallback::setIsTransWaitRepl( aTrans, ID_TRUE );
    }
    else
    {
        /* do nothing */
    }

    /* insert ������ �����ϱ� ����,
     * sdcRowPieceInsertInfo �ڷᱸ���� �ʱ�ȭ �Ѵ�. */
    makeInsertInfo( aTableHeader,
                    aValueList,
                    sTotalColCount,
                    &sInsertInfo );
    
    IDE_DASSERT( validateInsertInfo( &sInsertInfo,
                                     sTotalColCount,
                                     0 /* aFstColumnSeq */ )
                 == ID_TRUE );

    /* trailing null�� ������ ����Ѵ�. */
    sTrailingNullCount = getTrailingNullCount(&sInsertInfo,
                                              sTotalColCount);
    IDE_ASSERT_MSG( sTotalColCount >= sTrailingNullCount,
                    "Table OID           : %"ID_vULONG_FMT"\n"
                    "Total Column Count  : %"ID_UINT32_FMT"\n"
                    "Trailing Null Count : %"ID_UINT32_FMT"\n",
                    sTableOID,
                    sTotalColCount,
                    sTrailingNullCount );

    /* insert�ÿ� trailing null�� �������� �ʴ´�.
     * �׷��� sTotalColCount���� trailing null�� ������ ����. */
    sTotalColCount    -= sTrailingNullCount;

    if ( sTotalColCount == 0 )
    {
        /* NULL ROW�� ���(��� column�� ���� NULL�� ROW)
         * row header�� �����ϴ� row�� insert�Ѵ�.
         * analyzeRowForInsert()�� �� �ʿ䰡 ����. */
        SDC_INIT_INSERT_INFO(&sInsertInfo);

        /* NULL ROW�� ���� row header�� �����ϹǷ�
         * row header size�� rowpiece size �̴�. */
        sInsertInfo.mRowPieceSize = SDC_ROWHDR_SIZE;

        sRowFlag = SDC_ROWHDR_FLAG_NULLROW;

        /* BUG-32278 The previous flag is set incorrectly in row flag when
         * a rowpiece is overflowed. */
        IDE_TEST_RAISE( validateRowFlag(sRowFlag, sNextRowFlag) != ID_TRUE,
                        error_invalid_rowflag );

        IDE_TEST( insertRowPiece( aStatistics,
                                  aTrans,
                                  aTableInfoPtr,
                                  aTableHeader,
                                  &sInsertInfo,
                                  sRowFlag,
                                  sNextRowPieceSID,
                                  &aCSInfiniteSCN,
                                  sNeedUndoRec,
                                  sIsUndoLogging,
                                  sReplicate,
                                  &sInsertRowPieceSID )
                  != IDE_SUCCESS );

        sFstRowPieceSID = sInsertRowPieceSID;
    }
    else
    {
        /* NULL ROW�� �ƴ� ��� */

        /* column data���� �����Ҷ��� �ڿ������� ä���� �����Ѵ�.
         *
         * ����
         * 1. rowpiece write�Ҷ�,
         *    next rowpiece link ������ �˾ƾ� �ϹǷ�,
         *    ���� column data���� ���� �����ؾ� �Ѵ�.
         *
         * 2. ���ʺ��� �˲� ä���� �����ϸ�
         *    first page�� ���������� ������ ���ɼ��� ����,
         *    �׷��� update�ÿ� row migration �߻����ɼ��� ��������.
         * */
        sCurrColSeq = sTotalColCount  - 1;
        sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                            sCurrColSeq );
        while(1)
        {
            IDE_TEST( analyzeRowForInsert(
                          (sdpPageListEntry*)
                          smcTable::getDiskPageListEntry(aTableHeader),
                          sCurrColSeq,
                          sCurrOffset,
                          sNextRowPieceSID,
                          sTableOID,
                          &sInsertInfo ) != IDE_SUCCESS );

            sRowFlag = calcRowFlag4Insert( &sInsertInfo,
                                           sNextRowPieceSID );

            /* BUG-32278 The previous flag is set incorrectly in row flag when
             * a rowpiece is overflowed. */
            IDE_TEST_RAISE( validateRowFlag(sRowFlag, sNextRowFlag) != ID_TRUE,
                            error_invalid_rowflag );

            IDE_TEST( insertRowPiece( aStatistics,
                                      aTrans,
                                      aTableInfoPtr,
                                      aTableHeader,
                                      &sInsertInfo,
                                      sRowFlag,
                                      sNextRowPieceSID,
                                      &aCSInfiniteSCN,
                                      sNeedUndoRec,
                                      sIsUndoLogging,
                                      sReplicate,
                                      &sInsertRowPieceSID )
                      != IDE_SUCCESS );

            if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(&sInsertInfo) == ID_TRUE )
            {
                sFstRowPieceSID = sInsertRowPieceSID;
                break;
            }
            else
            {
                if ( sInsertInfo.mStartColOffset == 0)
                {
                    if ( sInsertInfo.mStartColSeq == 0 )
                    {
                        ideLog::log( IDE_ERR_0,
                                     "StartColSeq: %"ID_UINT32_FMT"\n",
                                     sInsertInfo.mStartColSeq );

                        ideLog::logMem( IDE_DUMP_0,
                                        (UChar*)&sInsertInfo,
                                        ID_SIZEOF(sdcRowPieceInsertInfo),
                                        "InsertInfo Dump:" );

                        ideLog::logMem( IDE_DUMP_0,
                                        (UChar*)aTableHeader,
                                        ID_SIZEOF(smcTableHeader),
                                        "TableHeader Dump:" );

                        IDE_ASSERT( 0 );
                    }

                    sCurrColSeq = sInsertInfo.mStartColSeq - 1;
                    sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                                        sCurrColSeq );
                }
                else
                {
                    sCurrColSeq = sInsertInfo.mStartColSeq;
                    sCurrOffset = sInsertInfo.mStartColOffset;
                }
                sNextRowPieceSID = sInsertRowPieceSID;
            }

            sNextRowFlag = sRowFlag;
        }
    }

    if (aTableInfoPtr != NULL)
    {
        smLayerCallback::incRecCntOfTableInfo( aTableInfoPtr );
    }

    SC_MAKE_GRID_WITH_SLOTNUM( *aRowGRID,
                               smLayerCallback::getTableSpaceID( aTableHeader ),
                               SD_MAKE_PID( sFstRowPieceSID ),
                               SD_MAKE_SLOTNUM( sFstRowPieceSID ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_rowflag )
    {
        ideLog::log( IDE_ERR_0,
                     "CurrColSeq:         %"ID_UINT32_FMT"\n"
                     "CurrOffset:         %"ID_UINT32_FMT"\n"
                     "TableColCount:      %"ID_UINT32_FMT"\n"
                     "TrailingNullCount:  %"ID_UINT32_FMT"\n"
                     "RowFlag:            %"ID_UINT32_FMT"\n"
                     "NextRowFlag:        %"ID_UINT32_FMT"\n"
                     "NextRowPieceSID:    %"ID_vULONG_FMT"\n"
                     "FstRowPieceSID:     %"ID_vULONG_FMT"\n",
                     sCurrColSeq,
                     sCurrOffset,
                     smLayerCallback::getColumnCount( aTableHeader ),
                     sTrailingNullCount,
                     sRowFlag,
                     sNextRowFlag,
                     sNextRowPieceSID,
                     sFstRowPieceSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)&sInsertInfo,
                        ID_SIZEOF(sdcRowPieceInsertInfo),
                        "[ Insert Info ]" );

        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG,
                                 "invalid row flag") );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : �������� rowpiece�� �����Ѵ�.
 *
 *
 **********************************************************************/
IDE_RC sdcRow::insertRowPiece(
                     idvSQL                      *aStatistics,
                     void                        *aTrans,
                     void                        *aTableInfoPtr,
                     void                        *aTableHeader,
                     const sdcRowPieceInsertInfo *aInsertInfo,
                     UChar                        aRowFlag,
                     sdSID                        aNextRowPieceSID,
                     smSCN                       *aCSInfiniteSCN,
                     idBool                       aIsNeedUndoRec,
                     idBool                       aIsUndoLogging,
                     idBool                       aReplicate,
                     sdSID                       *aInsertRowPieceSID )
{
    sdrMtx               sMtx;
    sdrMtxStartInfo      sStartInfo;
    UInt                 sDLogAttr;
    UChar                sNewCTSlotIdx;

    sdcRowHdrInfo        sRowHdrInfo;
    smSCN                sFstDskViewSCN;
    scSpaceID            sTableSpaceID;
    smOID                sTableOID;
    sdpPageListEntry   * sEntry;
    smrContType          sContType;

    UChar              * sFreePagePtr;
    UChar              * sAllocSlotPtr;
    sdSID                sAllocSlotSID;
    scGRID               sAllocSlotGRID;
    idBool               sAllocNewPage;
    UInt                 sState  = 0;
    sdcUndoSegment     * sUDSegPtr;

    IDE_ASSERT( aTrans             != NULL );
    IDE_ASSERT( aTableInfoPtr      != NULL );
    IDE_ASSERT( aTableHeader       != NULL );
    IDE_ASSERT( aInsertInfo        != NULL );
    IDE_ASSERT( aInsertRowPieceSID != NULL );

    sEntry = (sdpPageListEntry*)
        (smcTable::getDiskPageListEntry(aTableHeader));
    IDE_ASSERT( sEntry != NULL );

    sAllocSlotPtr = NULL;

    sTableOID     = smLayerCallback::getTableOID( aTableHeader );
    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );

    sDLogAttr  = ( SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DML );
    sDLogAttr |= ( aReplicate == ID_TRUE ?
                   SM_DLOG_ATTR_REPLICATE : SM_DLOG_ATTR_NORMAL );

    /* INSERT�� ���� Undo�� �ʿ����, Undo�� Segment�� Drop�Ѵ�.
     * ���δ� ��Ƽ�� ADD�� �ִ�. */
    if ( aIsUndoLogging == ID_TRUE )
    {
        sDLogAttr |= SM_DLOG_ATTR_UNDOABLE;
    }

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
    {
        if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(aInsertInfo) == ID_TRUE )
        {
            if ( aInsertInfo->mLobDescCnt > 0)
            {
                sContType = SMR_CT_CONTINUE;
            }
            else
            {
                sContType = SMR_CT_END;
            }
            
            sAllocNewPage =  sdpPageList::needAllocNewPage( sEntry,
                                                            aInsertInfo->mRowPieceSize );
        }
        else
        {
            sAllocNewPage = ID_TRUE;
            sContType     = SMR_CT_CONTINUE;
        }
    }
    else
    {
        /*
         * BUG-28060 [SD] update�� row migration�̳� change row piece������ �߻��Ҷ�
         * �������� ������������ �þ �� ����.
         *
         * Insert4Update��������߿� HeadRowPiece���� �߻��� �� �ִ�
         * Row Migration ����� �׿� RowPiece���� �߻��� �� �ִ� chage
         * Row Piece ���꿡�� insertRowPiece �߻��� �������� �з��� �÷�����
         * ���� �ʾƵ� ������ �������� �Ҵ��ϰ� �ϸ� �ȵǰ� ������ ��������� ����
         * �������� Ž���ؼ� insert4Upt ������ �����ؾ��Ѵ�.
         */
        sAllocNewPage = sdpPageList::needAllocNewPage( sEntry,
                                                       aInsertInfo->mRowPieceSize );

        /* insert rowpiece for update�� ���  */
        /* update�� ���, DML log�� ��� ����� �Ŀ�
         * replication�� ���� PK Log�� ����Ѵ�.
         * �׷��Ƿ� PK Log�� conttype�� SMR_CT_END�� �ȴ�.
         */
        sContType = SMR_CT_CONTINUE;
    }

    IDE_TEST( smxTrans::allocTXSegEntry( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr )
              != IDE_SUCCESS );
    sState=1;

    IDE_TEST( sdpPageList::findPage(
                                  aStatistics,
                                  sTableSpaceID,
                                  sEntry,
                                  aTableInfoPtr,
                                  aInsertInfo->mRowPieceSize,
                                  smLayerCallback::getMaxRows( aTableHeader ),
                                  sAllocNewPage,
                                  ID_TRUE,  /* Need SlotEntry */
                                  &sMtx,
                                  &sFreePagePtr,
                                  &sNewCTSlotIdx ) 
              != IDE_SUCCESS );

    /* PROJ-2162 Inconsistent page �߰� */
    IDE_TEST_RAISE( sdpPhyPage::isConsistentPage( sFreePagePtr ) == ID_FALSE,
                    ERR_INCONSISTENT_PAGE );

    IDE_TEST( sdpPageList::allocSlot( sFreePagePtr,
                                      aInsertInfo->mRowPieceSize,
                                      (UChar**)&sAllocSlotPtr,
                                      &sAllocSlotSID )
              != IDE_SUCCESS );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*) 
                                         sdpPhyPage::getHdr( sAllocSlotPtr ) )
                == smLayerCallback::getTableOID( aTableHeader ) );

    IDE_ASSERT( sAllocSlotPtr != NULL );

    SC_MAKE_GRID_WITH_SLOTNUM( sAllocSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(sAllocSlotSID),
                               SD_MAKE_SLOTNUM(sAllocSlotSID) );

    if ( aIsNeedUndoRec == ID_TRUE )
    {
        /* BUG-24906 DML�� UndoRec ����ϴ� UNDOTBS ���������ϸ� ������� !!
         * ���⼭ ���ܻ�Ȳ�� �߻��ϸ� ����Ÿ�������� �����Ҵ��� ���� ��ȿȭ���Ѿ��Ѵ�. */
        /* insert rowpiece ���꿡 ���� undo record�� �ۼ��Ѵ�. */
        sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aTrans );
        IDE_TEST( sUDSegPtr->addInsertRowPieceUndoRec(
                                           aStatistics,
                                           &sStartInfo,
                                           sTableOID,
                                           sAllocSlotGRID,
                                           aInsertInfo,
                                           SDC_IS_HEAD_ROWPIECE(aRowFlag) )
                  != IDE_SUCCESS );
    }

    sState = 2;

    /* �������� ��������� ����Ǿ����Ƿ�
     * �������� ���¸� �缳���Ѵ�. */
    IDE_TEST( sdpPageList::updatePageState( aStatistics,
                                            sTableSpaceID,
                                            sEntry,
                                            sdpPhyPage::getPageStartPtr(sAllocSlotPtr),
                                            &sMtx ) 
              != IDE_SUCCESS );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    /* sdcRowHdrInfo �ڷᱸ���� row header ������ �����Ѵ�. */
    SDC_INIT_ROWHDR_INFO( &sRowHdrInfo,
                          sNewCTSlotIdx,
                          *aCSInfiniteSCN,
                          SD_NULL_SID,
                          aInsertInfo->mColCount,
                          aRowFlag,
                          sFstDskViewSCN );

    /* �Ҵ���� slot�� rowpiece ������ write�Ѵ�. */
    writeRowPiece4IRP( sAllocSlotPtr,
                       &sRowHdrInfo,
                       aInsertInfo,
                       aNextRowPieceSID );

    IDE_DASSERT_MSG( aInsertInfo->mRowPieceSize == getRowPieceSize( sAllocSlotPtr ),
                     "Invalid RowPieceSize"
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     aInsertInfo->mRowPieceSize,
                     getRowPieceSize( sAllocSlotPtr ) );

    sdrMiniTrans::setRefOffset(&sMtx, sTableOID);

    /* insert rowpiece ������ redo undo log�� �ۼ��Ѵ�. */
    IDE_TEST( writeInsertRowPieceRedoUndoLog( sAllocSlotPtr,
                                              sAllocSlotGRID,
                                              &sMtx,
                                              aInsertInfo,
                                              aReplicate )
              != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-32539/BUG-32539.tc */
    IDU_FIT_POINT( "1.BUG-32539@sdcRow::insertRowPiece" );

    IDE_TEST( sdcTableCTL::bind( &sMtx,
                                 sTableSpaceID,
                                 sAllocSlotPtr,
                                 SDP_CTS_IDX_UNLK, /* aOldCTSlotIdx */
                                 sNewCTSlotIdx,
                                 SD_MAKE_SLOTNUM(sAllocSlotSID),
                                 0,         /* aFSCreditSize4OldRowHdrEx */
                                 0,         /* aNewFSCreditSize */
                                 ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    /* FIT/ART/sm/Bugs/BUG-19122/BUG-19122.sql */
    IDU_FIT_POINT( "1.BUG-19122@sdcRow::insertRowPiece" );

    sState = 0;
    /* Insert ������ ������ Mini Transaction�� Commit�Ѵ� */
    IDE_TEST( sdrMiniTrans::commit( &sMtx, sContType )
              != IDE_SUCCESS );

    if ( (aInsertInfo->mLobDescCnt > 0) &&
         (aInsertInfo->mIsUptLobByAPI != ID_TRUE) )
    {
        if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
        {
            IDE_TEST( writeAllLobCols( aStatistics,
                                       aTrans,
                                       aTableHeader,
                                       aInsertInfo,
                                       sAllocSlotGRID )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( updateAllLobCols( aStatistics,
                                        &sStartInfo,
                                        aTableHeader,
                                        aInsertInfo,
                                        sAllocSlotGRID )
                      != IDE_SUCCESS );
        }
    }

    *aInsertRowPieceSID = sAllocSlotSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_PAGE )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  sdpPhyPage::getSpaceID( sFreePagePtr ),
                                  sdpPhyPage::getPageID( sFreePagePtr ) ) );
    }

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch ( sState )
    {
        case 2:
            /* BUG-24906 DML�� UndoRec ����ϴ� UNDOTBS ���������ϸ� ������� !!
             * ���⼭ Mtx Rollback�� Assertion �߻��ϵ��� �Ǿ� ����.
             * �ֳ��ϸ� BIND_CTS Logical �α�Ÿ�Կ� ���ؼ� �������� �̹� ����Ǿ���,
             * �̸� ������ �� ���� �����̴�.
             */
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
            break;

        case 1:
            if ( sAllocSlotPtr != NULL )
            {
                IDE_ASSERT( sdpPhyPage::freeSlot4SID( 
                                          (sdpPhyPageHdr*)sFreePagePtr,
                                          sAllocSlotPtr,
                                          SD_MAKE_SLOTNUM(sAllocSlotSID),
                                          aInsertInfo->mRowPieceSize )
                             == IDE_SUCCESS );
            }

            /* BUGBUG
             * BUG-32666     [sm-disk-collection] The failure of DRDB Insert
             * makes a unlogged slot.
             * �� ���װ� ������ �Ŀ� Rollback���� �����ؾ� �Ѵ�. */
            IDE_ASSERT( sdrMiniTrans::commit(&sMtx) == IDE_SUCCESS );

        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*******************************************************************************
 * Description : Direct-Path INSERT ������� aValueList�� Insert �Ѵ�.
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aTrans          - [IN] Insert�� �����ϴ� TX
 *      aDPathSegInfo   - [IN] Direct-Path INSERT�� ���� �ڷᱸ��
 *      aTableHeader    - [IN] Insert�� Table�� Header
 *      aCSInfiniteSCN  - [IN]
 *      aValueList      - [IN] Insert�� Value��
 *      aRowGRID        - [OUT]
 ******************************************************************************/
IDE_RC sdcRow::insertAppend( idvSQL            * aStatistics,
                             void              * aTrans,
                             void              * aDPathSegInfo,
                             void              * aTableHeader,
                             smSCN               aCSInfiniteSCN,
                             const smiValue    * aValueList,
                             scGRID            * aRowGRID )
{
    sdpDPathSegInfo      * sDPathSegInfo;
    sdcRowPieceInsertInfo  sInsertInfo;
    UInt                   sTotalColCount;
    UShort                 sTrailingNullCount;
    UShort                 sCurrColSeq;
    UInt                   sCurrOffset;
    UChar                  sRowFlag;
    smOID                  sTableOID;
    sdSID                  sFstRowPieceSID    = SD_NULL_SID;
    sdSID                  sInsertRowPieceSID = SD_NULL_SID;
    sdSID                  sNextRowPieceSID   = SD_NULL_SID;

    IDE_ASSERT( aTrans        != NULL );
    IDE_ASSERT( aDPathSegInfo != NULL );
    IDE_ASSERT( aTableHeader  != NULL );
    IDE_ASSERT( aValueList    != NULL );
    IDE_ASSERT( aRowGRID      != NULL );
    IDE_ASSERT_MSG( SM_SCN_IS_INFINITE( aCSInfiniteSCN ),
                    "Cursor Infinite SCN : %"ID_UINT64_FMT"\n",
                    aCSInfiniteSCN );

    IDE_DASSERT( smLayerCallback::needReplicate( (smcTableHeader*)aTableHeader,
                                                 aTrans ) == ID_FALSE );

    sTableOID      = smLayerCallback::getTableOID( aTableHeader );
    sTotalColCount = smLayerCallback::getColumnCount( aTableHeader );
    IDE_ASSERT( sTotalColCount > 0 );

    IDE_DASSERT( validateSmiValue(aValueList,
                                  sTotalColCount)
                 == ID_TRUE );

    sDPathSegInfo = (sdpDPathSegInfo*)aDPathSegInfo;
   
    /* insert ������ �����ϱ� ����,
     * sdcRowPieceInsertInfo �ڷᱸ���� �ʱ�ȭ �Ѵ�. */
    makeInsertInfo( aTableHeader,
                    aValueList,
                    sTotalColCount,
                    &sInsertInfo );

    IDE_DASSERT( validateInsertInfo( &sInsertInfo,
                                     sTotalColCount,
                                     0 /* aFstColumnSeq */ )
                 == ID_TRUE );

    /* trailing null�� ������ ����Ѵ�. */
    sTrailingNullCount = getTrailingNullCount(&sInsertInfo,
                                              sTotalColCount);
    IDE_ASSERT_MSG( sTotalColCount >= sTrailingNullCount,
                    "Table OID           : %"ID_UINT32_FMT"\n"
                    "Total Column Count  : %"ID_UINT32_FMT"\n"
                    "Trailing Null Count : %"ID_UINT32_FMT"\n",
                    sTableOID,
                    sTotalColCount,
                    sTrailingNullCount );

    /* insert�ÿ� trailing null�� �������� �ʴ´�.
     * �׷��� sTotalColCount���� trailing null�� ������ ����. */
    sTotalColCount    -= sTrailingNullCount;

    if ( sTotalColCount == 0 )
    {
        /* NULL ROW�� ���(��� column�� ���� NULL�� ROW)
         * row header�� �����ϴ� row�� insert�Ѵ�.
         * analyzeRowForInsert()�� �� �ʿ䰡 ����. */
        SDC_INIT_INSERT_INFO(&sInsertInfo);

        /* NULL ROW�� ���� row header�� �����ϹǷ�
         * row header size�� rowpiece size �̴�. */
        sInsertInfo.mRowPieceSize = SDC_ROWHDR_SIZE;

        sRowFlag = SDC_ROWHDR_FLAG_NULLROW;

        IDE_TEST( insertRowPiece4Append( aStatistics,
                                         aTrans,
                                         aTableHeader,
                                         sDPathSegInfo,
                                         &sInsertInfo,
                                         sRowFlag,
                                         sNextRowPieceSID,
                                         &aCSInfiniteSCN,
                                         &sInsertRowPieceSID )
                  != IDE_SUCCESS );

        sFstRowPieceSID = sInsertRowPieceSID;
    }
    else
    {
        /* NULL ROW�� �ƴ� ��� */

        /* column data���� �����Ҷ��� �ڿ������� ä���� �����Ѵ�.
         *
         * ����
         * 1. rowpiece write�Ҷ�,
         *    next rowpiece link ������ �˾ƾ� �ϹǷ�,
         *    ���� column data���� ���� �����ؾ� �Ѵ�.
         *
         * 2. ���ʺ��� �˲� ä���� �����ϸ�
         *    first page�� ���������� ������ ���ɼ��� ����,
         *    �׷��� update�ÿ� row migration �߻����ɼ��� ��������.
         * */
        sCurrColSeq = sTotalColCount  - 1;
        sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                            sCurrColSeq );
        while(1)
        {
            IDE_TEST( analyzeRowForInsert((sdpPageListEntry*)
                                          smcTable::getDiskPageListEntry(aTableHeader),
                                          sCurrColSeq,
                                          sCurrOffset,
                                          sNextRowPieceSID,
                                          sTableOID,
                                          &sInsertInfo )
                      != IDE_SUCCESS );

            sRowFlag = calcRowFlag4Insert( &sInsertInfo,
                                           sNextRowPieceSID );

            IDE_TEST( insertRowPiece4Append( aStatistics,
                                             aTrans,
                                             aTableHeader,
                                             sDPathSegInfo,
                                             &sInsertInfo,
                                             sRowFlag,
                                             sNextRowPieceSID,
                                             &aCSInfiniteSCN,
                                             &sInsertRowPieceSID )
                      != IDE_SUCCESS );

            if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(&sInsertInfo) == ID_TRUE )
            {
                sFstRowPieceSID = sInsertRowPieceSID;
                break;
            }
            else
            {
                if ( sInsertInfo.mStartColOffset == 0 )
                {
                    sCurrColSeq = sInsertInfo.mStartColSeq - 1;
                    sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                                        sCurrColSeq );
                }
                else
                {
                    sCurrColSeq = sInsertInfo.mStartColSeq;
                    sCurrOffset = sInsertInfo.mStartColOffset;
                }
                sNextRowPieceSID = sInsertRowPieceSID;
            }
        }
    }

    if (sDPathSegInfo != NULL)
    {
        sDPathSegInfo->mRecCount++;
    }

    SC_MAKE_GRID_WITH_SLOTNUM( *aRowGRID,
                               smLayerCallback::getTableSpaceID( aTableHeader ),
                               SD_MAKE_PID( sFstRowPieceSID ),
                               SD_MAKE_SLOTNUM( sFstRowPieceSID ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*******************************************************************************
 * Description : Append ������� InsertRowPiece�� �����Ѵ�.
 ******************************************************************************/
IDE_RC sdcRow::insertRowPiece4Append(
                            idvSQL                      *aStatistics,
                            void                        *aTrans,
                            void                        *aTableHeader,
                            sdpDPathSegInfo             *aDPathSegInfo,
                            const sdcRowPieceInsertInfo *aInsertInfo,
                            UChar                        aRowFlag,
                            sdSID                        aNextRowPieceSID,
                            smSCN                       *aCSInfiniteSCN,
                            sdSID                       *aInsertRowPieceSID )

{
    sdrMtx               sMtx;
    sdrMtxStartInfo      sStartInfo;
    UInt                 sDLogAttr;

    sdcRowHdrInfo        sRowHdrInfo;
    smSCN                sFstDskViewSCN;
    scSpaceID            sTableSpaceID;
    sdpPageListEntry    *sEntry;
#ifdef DEBUG
    smOID                sTableOID;
#endif
    UChar               *sFreePagePtr;
    UChar               *sAllocSlotPtr;
    sdSID                sAllocSlotSID;
    UChar                sNewCTSlotIdx;
    UInt                 sState    = 0;

    IDE_ASSERT( aTrans             != NULL );
    IDE_ASSERT( aTableHeader       != NULL );
    IDE_ASSERT( aDPathSegInfo      != NULL );
    IDE_ASSERT( aInsertInfo        != NULL );
    IDE_ASSERT( aInsertRowPieceSID != NULL );
    IDE_ASSERT( aInsertInfo->mIsInsert4Upt == ID_FALSE );
    IDE_ASSERT( ((smxTrans*)aTrans)->getTXSegEntry() != NULL );

    sEntry =
        (sdpPageListEntry*)(smcTable::getDiskPageListEntry(aTableHeader));
    IDE_ASSERT( sEntry != NULL );

#ifdef DEBUG
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );
#endif
    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );

    // sStartInfo for allocate TXSEG Entry
    sDLogAttr           = SM_DLOG_ATTR_DEFAULT;
    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_NOLOGGING;

    sDLogAttr |= SM_DLOG_ATTR_EXCEPT_INSERT_APPEND_PAGE;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr )
              != IDE_SUCCESS );
    sState=1;

    IDE_TEST( sdpPageList::findSlot4AppendRec(
                     aStatistics,
                     sTableSpaceID,
                     sEntry,
                     aDPathSegInfo,
                     aInsertInfo->mRowPieceSize,
                     smLayerCallback::isTableLoggingMode(aTableHeader),
                     &sMtx,
                     &sFreePagePtr,
                     &sNewCTSlotIdx ) 
              != IDE_SUCCESS );

    IDE_TEST( sdpPageList::allocSlot( sFreePagePtr,
                                      aInsertInfo->mRowPieceSize,
                                      (UChar**)&sAllocSlotPtr,
                                      &sAllocSlotSID )
              != IDE_SUCCESS );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*) 
                                         sdpPhyPage::getHdr( sAllocSlotPtr ) )
        == smLayerCallback::getTableOID( aTableHeader ) );

    IDE_ASSERT( sAllocSlotPtr != NULL );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    SDC_INIT_ROWHDR_INFO( &sRowHdrInfo,
                          sNewCTSlotIdx,
                          *aCSInfiniteSCN,
                          SD_NULL_SID,
                          aInsertInfo->mColCount,
                          aRowFlag,
                          sFstDskViewSCN );

    /* �Ҵ���� slot�� rowpiece ������ write�Ѵ�. */
    writeRowPiece4IRP( sAllocSlotPtr,
                       &sRowHdrInfo,
                       aInsertInfo,
                       aNextRowPieceSID );

    IDE_DASSERT_MSG( aInsertInfo->mRowPieceSize ==
                     getRowPieceSize( sAllocSlotPtr ),
                     "Invalid RowPieceSize"
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     sTableOID,
                     aInsertInfo->mRowPieceSize,
                     getRowPieceSize( sAllocSlotPtr ) );

    IDE_TEST( sdcTableCTL::bind( &sMtx,
                                 sTableSpaceID,
                                 sAllocSlotPtr,
                                 SDP_CTS_IDX_UNLK, /* aOldCTSlotIdx */
                                 sNewCTSlotIdx,
                                 SD_MAKE_SLOTNUM(sAllocSlotSID),
                                 0,         /* aFSCreditSize4RowHdrEx */
                                 0,         /* aNewFSCreditSize */
                                 ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    *aInsertRowPieceSID = sAllocSlotSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx)
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_INSERT_ROW_PIECE,
 *   SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeInsertRowPieceRedoUndoLog(
                    UChar                       *aSlotPtr,
                    scGRID                       aSlotGRID,
                    sdrMtx                      *aMtx,
                    const sdcRowPieceInsertInfo *aInsertInfo,
                    idBool                       aReplicate )
{
    sdrLogType    sLogType;
    UShort        sLogSize      = 0;
    UShort        sLogSize4RP   = 0;
    UShort        sRowPieceSize = 0;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aInsertInfo != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
    {
        sLogType = SDR_SDC_INSERT_ROW_PIECE;
    }
    else
    {
        sLogType = SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE;
    }

    if ( aReplicate == ID_TRUE )
    {
        sLogSize4RP = calcInsertRowPieceLogSize4RP(aInsertInfo);
    }
    else
    {
        sLogSize4RP = 0;
    }

    sRowPieceSize = aInsertInfo->mRowPieceSize;
    sLogSize      = sRowPieceSize + sLogSize4RP;


    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  sRowPieceSize )
              != IDE_SUCCESS );

    if ( aReplicate == ID_TRUE )
    {
        IDE_TEST( writeInsertRowPieceLog4RP( aInsertInfo,
                                             aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  delete rowpiece undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteRowPieceCLR( UChar     *aSlotPtr,
                                       scGRID     aSlotGRID,
                                       sdrMtx    *aMtx )
{
    sdrLogType    sLogType;
    UShort        sLogSize      = 0;
    UShort        sRowPieceSize = 0;

    IDE_ASSERT( aSlotPtr != NULL );
    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    /* BUG-23989
     * [TSM] delete rollback�� ����
     * restart redo�� ���������� ������� �ʽ��ϴ�. */
    sLogType = SDR_SDC_INSERT_ROW_PIECE_FOR_DELETEUNDO;

    sRowPieceSize = getRowPieceSize(aSlotPtr);
    sLogSize      = sRowPieceSize;

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  sRowPieceSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-39507
 *
 **********************************************************************/
IDE_RC sdcRow::isUpdatedRowBySameStmt( idvSQL          * aStatistics,
                                       void            * aTrans,
                                       smSCN             aStmtViewSCN,
                                       void            * aTableHeader,
                                       scGRID            aSlotGRID,
                                       smSCN             aCSInfiniteSCN,
                                       idBool          * aIsUpdatedRowBySameStmt )
{
    sdSID       sCurrRowPieceSID;

    IDE_ERROR( aTrans        != NULL );
    IDE_ERROR( aTableHeader  != NULL );
    IDE_ERROR( SC_GRID_IS_NOT_NULL(aSlotGRID));
    IDE_ERROR_MSG( SM_SCN_IS_VIEWSCN(aStmtViewSCN),
                   "Statement SCN : %"ID_UINT64_FMT"\n",
                   aStmtViewSCN );

    sCurrRowPieceSID = SD_MAKE_SID( SC_MAKE_PID(aSlotGRID),
                                    SC_MAKE_SLOTNUM(aSlotGRID) );

    IDE_TEST( isUpdatedRowPieceBySameStmt( aStatistics,
                                           aTrans,
                                           aTableHeader,
                                           &aStmtViewSCN,
                                           &aCSInfiniteSCN,
                                           sCurrRowPieceSID,          
                                           aIsUpdatedRowBySameStmt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-39507
 *
 **********************************************************************/
IDE_RC sdcRow::isUpdatedRowPieceBySameStmt( idvSQL            * aStatistics,
                                            void              * aTrans,
                                            void              * aTableHeader,
                                            smSCN             * aStmtViewSCN,
                                            smSCN             * aCSInfiniteSCN,
                                            sdSID               aCurrRowPieceSID,
                                            idBool            * aIsUpdatedRowBySameStmt )
{
    UChar             * sUpdateSlot;
    sdrMtx              sMtx;
    sdrMtxStartInfo     sStartInfo;
    sdrSavePoint        sSP;
    UInt                sDLogAttr;
    sdcRowHdrInfo       sOldRowHdrInfo;
    sdcUpdateState      sUptState       = SDC_UPTSTATE_UPDATABLE;
    UShort              sState          = 0;
    UChar               sNewCTSlotIdx   = 0;
    smTID               sWaitTransID;

    IDE_ASSERT( aTrans                  != NULL );
    IDE_ASSERT( aTableHeader            != NULL );
    IDE_ASSERT( aCurrRowPieceSID        != SD_NULL_SID );
    IDE_ASSERT( aIsUpdatedRowBySameStmt != NULL );

    sDLogAttr  = SM_DLOG_ATTR_DEFAULT;
    sDLogAttr |= ( SM_DLOG_ATTR_DML | SM_DLOG_ATTR_UNDOABLE );

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( smxTrans::allocTXSegEntry( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr) != IDE_SUCCESS );
    sState = 1;


    sdrMiniTrans::setSavePoint( &sMtx, &sSP );

    IDE_TEST( prepareUpdatePageBySID( aStatistics,
                                      &sMtx,
                                      smLayerCallback::getTableSpaceID( aTableHeader ),
                                      aCurrRowPieceSID,
                                      SDB_SINGLE_PAGE_READ,
                                      &sUpdateSlot,
                                      &sNewCTSlotIdx ) != IDE_SUCCESS );

    IDE_DASSERT( isHeadRowPiece(sUpdateSlot) == ID_TRUE );

    /* old row header info�� ���Ѵ�. */
    getRowHdrInfo( sUpdateSlot, &sOldRowHdrInfo );

    // head rowpiece�� �ƴϸ� �ȵȴ�
    IDE_TEST( SDC_IS_HEAD_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_FALSE );

    IDE_TEST( canUpdateRowPieceInternal(
                                    aStatistics,
                                    &sStartInfo,
                                    sUpdateSlot,
                                    smxTrans::getTSSlotSID( sStartInfo.mTrans ),
                                    SDB_SINGLE_PAGE_READ,
                                    aStmtViewSCN,
                                    aCSInfiniteSCN,
                                    ID_FALSE, // aIsUptLobByAPI,
                                    &sUptState,
                                    &sWaitTransID )
              != IDE_SUCCESS );


    if ( sUptState == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION )
    {
        *aIsUpdatedRowBySameStmt = ID_TRUE;
    }
    else
    {
        *aIsUpdatedRowBySameStmt = ID_FALSE;
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx, SMR_CT_CONTINUE) != IDE_SUCCESS );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  �ϳ��� row�� ���� update ������ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::update( idvSQL               * aStatistics,
                       void                 * aTrans,
                       smSCN                  aStmtViewSCN,
                       void                 * aTableInfoPtr,
                       void                 * aTableHeader,
                       SChar                * /*aOldRow*/,
                       scGRID                 aSlotGRID,
                       SChar               ** /*aRetRow*/,
                       scGRID               * /*aRetUpdateSlotGSID*/,
                       const smiColumnList  * aColumnList,
                       const smiValue       * aValueList,
                       const smiDMLRetryInfo* aDMLRetryInfo,
                       smSCN                  aCSInfiniteSCN,
                       sdcColInOutMode      * aValueModeList,
                       ULong                * /*aModifyIdxBit*/,
                       idBool                 aForbiddenToRetry )
{
    sdcRowUpdateStatus   sUpdateStatus;
    sdcPKInfo            sPKInfo;
    sdSID                sCurrRowPieceSID;
    sdSID                sPrevRowPieceSID;
    sdSID                sNextRowPieceSID;
    sdSID                sLstInsertRowPieceSID;
    idBool               sReplicate;
    idBool               sIsRowPieceDeletedByItSelf;
    sdcRetryInfo         sRetryInfo = {NULL, ID_FALSE, NULL, {NULL,NULL,0},{NULL,NULL,0},0};
    scSpaceID            sTableSpaceID;

    IDE_ERROR( aTrans        != NULL );
    IDE_ERROR( aTableInfoPtr != NULL );
    IDE_ERROR( aTableHeader  != NULL );
    IDE_ERROR( aColumnList   != NULL );
    IDE_ERROR( aValueList    != NULL );
    IDE_ERROR( SC_GRID_IS_NOT_NULL(aSlotGRID));
    IDE_ERROR_MSG( SM_SCN_IS_VIEWSCN(aStmtViewSCN),
                   "Statement SCN : %"ID_UINT64_FMT"\n",
                   aStmtViewSCN );

    IDE_DASSERT( validateSmiColumnList( aColumnList )
                 == ID_TRUE );

    /* FIT/ART/sm/Projects/PROJ-2118/update/update.ts */
    IDU_FIT_POINT( "1.PROJ-2118@sdcRow::update" );

    sCurrRowPieceSID = SD_MAKE_SID( SC_MAKE_PID(aSlotGRID),
                                    SC_MAKE_SLOTNUM(aSlotGRID) );
    sPrevRowPieceSID = SD_NULL_SID;
    sNextRowPieceSID = SD_NULL_SID;

    /* update�� �����ϱ� ���� update ���������� �ʱ�ȭ�Ѵ�. */
    initRowUpdateStatus( aColumnList, &sUpdateStatus );

    IDE_DASSERT( validateSmiValue( aValueList,
                                   sUpdateStatus.mTotalUpdateColCount )
                 == ID_TRUE );

    setRetryInfo( aDMLRetryInfo,
                  &sRetryInfo );

    sReplicate = smLayerCallback::needReplicate( (smcTableHeader*)aTableHeader,
                                                 aTrans );

    /*
     * aValueModeList�� NULL�� �ƴ� ���� finishWrite���� Lob �÷��� update�ϴ� ����̴�.
     * out-mode value�� update �ϴ� �α״� replication�� ������ �ʵ��� ���´�.
     */
    
    if ( aValueModeList != NULL )
    {
        if ( (*aValueModeList) == SDC_COLUMN_OUT_MODE_LOB )
        {
            sReplicate = ID_FALSE;
        }
    }

    if ( sReplicate == ID_TRUE )
    {
        if ( smcTable::isTransWaitReplicationTable( (const smcTableHeader*)aTableHeader )
             == ID_TRUE )
        {
            smLayerCallback::setIsTransWaitRepl( aTrans, ID_TRUE );
        }
        else
        {
            /* do nothing */
        }
        /* replication�� �ɸ� ���
         * pk log�� ����ؾ� �ϹǷ�
         * sdcPKInfo �ڷᱸ���� �ʱ�ȭ�Ѵ�. */
        makePKInfo( aTableHeader, &sPKInfo );
    }

    while(1)
    {
        IDE_TEST( updateRowPiece( aStatistics,
                                  aTrans,
                                  aTableInfoPtr,
                                  aTableHeader,
                                  &aStmtViewSCN,
                                  &aCSInfiniteSCN,
                                  aColumnList,
                                  aValueList,
                                  &sRetryInfo,
                                  sPrevRowPieceSID,
                                  sCurrRowPieceSID,
                                  sReplicate,
                                  aValueModeList,
                                  aForbiddenToRetry,
                                  &sPKInfo,
                                  &sUpdateStatus,
                                  &sNextRowPieceSID,
                                  &sLstInsertRowPieceSID,
                                  &sIsRowPieceDeletedByItSelf )
                  != IDE_SUCCESS );

        if ( isUpdateEnd( sNextRowPieceSID,
                          &sUpdateStatus,
                          sReplicate,
                          &sPKInfo )
             == ID_TRUE )
        {
            if ( sRetryInfo.mIsAlreadyModified == ID_TRUE )
            {
                sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );

                IDE_TEST( checkRetryRemainedRowPiece( aStatistics,
                                                      aTrans,
                                                      sTableSpaceID,
                                                      sNextRowPieceSID,
                                                      aForbiddenToRetry,
                                                      aStmtViewSCN,
                                                      &sRetryInfo )
                          != IDE_SUCCESS );
            }

            break;
        }

        if ( sLstInsertRowPieceSID != SD_NULL_SID )
        {
            /* split�� �߻��Ͽ� ���ο� rowpiece�� �߰��� ���,
             * ���� ���� �߰��� rowpiece��
             * next rowpiece�� prev rowpiece�� �ȴ�.*/
            sPrevRowPieceSID = sLstInsertRowPieceSID;
        }
        else
        {
            if ( sIsRowPieceDeletedByItSelf == ID_TRUE )
            {
                /* rowpiece�� ������ ������ ���,
                 * sPrevRowPieceSID�� ������ �ʴ´�. */
            }
            else
            {
                sPrevRowPieceSID = sCurrRowPieceSID;
            }
        }
        sCurrRowPieceSID = sNextRowPieceSID;
    }

    // ���������� update�Ǿ��ٸ� savepoint�� �̹� unset�Ǿ��ִ�.
    IDE_ASSERT( sRetryInfo.mISavepoint == NULL );

    if ( sReplicate == ID_TRUE)
    {
        /* replication�� �ɸ� ��� pk log�� ����Ѵ�. */
        IDE_TEST( writePKLog( aStatistics,
                              aTrans,
                              aTableHeader,
                              aSlotGRID,
                              &sPKInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sRetryInfo.mISavepoint != NULL )
    {
        // ���� ��Ȳ���� savepoint�� ���� set�Ǿ� ���� �� �ִ�.
        IDE_ASSERT( smxTrans::unsetImpSavepoint4LayerCall( aTrans,
                                                           sRetryInfo.mISavepoint )
                    == IDE_SUCCESS );

        sRetryInfo.mISavepoint = NULL;
    }

    if ( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( aTrans,
                                   IDV_STAT_INDEX_UPDATE_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader*)aTableHeader,
                                  SMC_INC_RETRY_CNT_OF_UPDATE );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *  aStatistics         - [IN] �������
 *  aTrans              - [IN] Ʈ����� ����
 *  aTableHeader        - [IN] ���̺� ���
 *  aStmtViewSCN        - [IN] stmt scn
 *  aCSInfiniteSCN      - [IN] cursor infinite scn
 *  aRetryInfo          - [IN] retry ���θ� �Ǵ��ϱ����� ����
 *  aReplicate          - [IN] replication ����
 *  aForbiddenToRetry   - [IN] retry error �ø��°� ����, abort �߻�
 *
 *  aPKInfo             - [IN/OUT]
 *  aUpdateStatus       - [IN/OUT]
 *  aNextRowPieceSID           - [OUT] next rowpiece sid
 *  aLstInsertRowPieceSID      - [OUT]
 *  aIsRowPieceDeletedByItSelf - [OUT]
 **********************************************************************/
IDE_RC sdcRow::updateRowPiece( idvSQL              *aStatistics,
                               void                *aTrans,
                               void                *aTableInfoPtr,
                               void                *aTableHeader,
                               smSCN               *aStmtViewSCN,
                               smSCN               *aCSInfiniteSCN,
                               const smiColumnList *aColList,
                               const smiValue      *aValueList,
                               sdcRetryInfo        *aRetryInfo,
                               const sdSID          aPrevRowPieceSID,
                               sdSID                aCurrRowPieceSID,
                               idBool               aReplicate,
                               sdcColInOutMode     *aValueModeList,
                               idBool               aForbiddenToRetry,
                               sdcPKInfo           *aPKInfo,
                               sdcRowUpdateStatus  *aUpdateStatus,
                               sdSID               *aNextRowPieceSID,
                               sdSID               *aLstInsertRowPieceSID,
                               idBool              *aIsRowPieceDeletedByItSelf )
{
    UChar                      *sUpdateSlot;
    sdrMtx                      sMtx;
    sdrMtxStartInfo             sStartInfo;
    sdrSavePoint                sSP;
    UInt                        sDLogAttr;
    scSpaceID                   sTableSpaceID;
    smOID                       sTableOID;
    sdcRowHdrInfo               sOldRowHdrInfo;
    sdcRowHdrInfo               sNewRowHdrInfo;
    sdcRowPieceUpdateInfo       sUpdateInfo;
    sdcRowPieceOverwriteInfo    sOverwriteInfo;
    sdcSupplementJobInfo        sSupplementJobInfo;
    smSCN                       sFstDskViewSCN;
    scGRID                      sCurrRowPieceGRID;
    sdSID                       sNextRowPieceSID      = SD_NULL_SID;
    sdSID                       sLstInsertRowPieceSID = SD_NULL_SID;
    sdcUpdateState              sUptState                  = SDC_UPTSTATE_UPDATABLE;
    UShort                      sState                     = 0;
    idBool                      sIsCopyPKInfo              = ID_FALSE;
    idBool                      sIsRowPieceDeletedByItSelf = ID_FALSE;
    UChar                       sNewCTSlotIdx = 0;
    idBool                      sIsUptLobByAPI;
    UChar                       sRowFlag;
    UChar                       sPrevRowFlag;
    UInt                        sRetryCnt = 0;

    IDE_ASSERT( aTrans                     != NULL );
    IDE_ASSERT( aTableInfoPtr              != NULL );
    IDE_ASSERT( aTableHeader               != NULL );
    IDE_ASSERT( aColList                   != NULL );
    IDE_ASSERT( aValueList                 != NULL );
    IDE_ASSERT( aPKInfo                    != NULL );
    IDE_ASSERT( aUpdateStatus              != NULL );
    IDE_ASSERT( aNextRowPieceSID           != NULL );
    IDE_ASSERT( aLstInsertRowPieceSID      != NULL );
    IDE_ASSERT( aIsRowPieceDeletedByItSelf != NULL );
    IDE_ASSERT( aCurrRowPieceSID           != SD_NULL_SID );

    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );

    SC_MAKE_GRID_WITH_SLOTNUM( sCurrRowPieceGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(aCurrRowPieceSID),
                               SD_MAKE_SLOTNUM(aCurrRowPieceSID) );

    sDLogAttr  = SM_DLOG_ATTR_DEFAULT;
    sDLogAttr |= ( aReplicate == ID_TRUE ?
                  SM_DLOG_ATTR_REPLICATE : SM_DLOG_ATTR_NORMAL );
    sDLogAttr |= ( SM_DLOG_ATTR_DML | SM_DLOG_ATTR_UNDOABLE );

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( smxTrans::allocTXSegEntry( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

reentry :
    
    sRetryCnt++;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr) != IDE_SUCCESS );
    sState = 1;

    sdrMiniTrans::setSavePoint( &sMtx, &sSP );

    IDE_TEST( prepareUpdatePageBySID( aStatistics,
                                      &sMtx,
                                      sTableSpaceID,
                                      aCurrRowPieceSID,
                                      SDB_SINGLE_PAGE_READ,
                                      &sUpdateSlot,
                                      &sNewCTSlotIdx ) != IDE_SUCCESS );

#ifdef DEBUG
    if ( aPrevRowPieceSID == SD_NULL_SID )
    {
        IDE_DASSERT( isHeadRowPiece(sUpdateSlot) == ID_TRUE );
    }
    else
    {
        IDE_DASSERT( isHeadRowPiece(sUpdateSlot) == ID_FALSE );
    }
#endif

    /* old row header info�� ���Ѵ�. */
    getRowHdrInfo( sUpdateSlot, &sOldRowHdrInfo );

    /* BUG-32278 The previous flag is set incorrectly in row flag when
     * a rowpiece is overflowed.
     *
     * ���׷� ���� row flag�� P flag�� �߸� ���� �ִ� ������ �� �ִ�.
     * �� ��� flag ���� ������ ���ش�. */
    if ( (*aNextRowPieceSID != SD_NULL_SID) && (sRetryCnt == 1) )
    {
        IDE_ASSERT( aUpdateStatus->mPrevRowPieceRowFlag
                    != SDC_ROWHDR_FLAG_ALL );

        sPrevRowFlag = aUpdateStatus->mPrevRowPieceRowFlag;
        sRowFlag     = sOldRowHdrInfo.mRowFlag;

        IDE_TEST_RAISE( validateRowFlag(sPrevRowFlag, sRowFlag) != ID_TRUE,
                        error_invalid_rowflag );
    }

    if ( SDC_IS_HEAD_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_TRUE )
    {
        sIsUptLobByAPI = ( aValueModeList != NULL ) ? ID_TRUE : ID_FALSE;

        /* head rowpiece�� ���,
         * update ������ ������ �� �ִ��� �˻��ؾ� �Ѵ�. */
        IDE_TEST( canUpdateRowPiece( aStatistics,
                                     &sMtx,
                                     &sSP,
                                     sTableSpaceID,
                                     aCurrRowPieceSID,
                                     SDB_SINGLE_PAGE_READ,
                                     aStmtViewSCN,
                                     aCSInfiniteSCN,
                                     sIsUptLobByAPI,
                                     &sUpdateSlot,
                                     &sUptState,
                                     &sNewCTSlotIdx,
                                     /* BUG-31359
                                      * Wait���� ������ ������ ����
                                      * ID_ULONG_MAX ��ŭ ��ٸ��� �Ѵ�. */
                                     ID_ULONG_MAX )
                  != IDE_SUCCESS );

        if ( sUptState == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION )
        {
            IDE_TEST_RAISE( aRetryInfo->mRetryInfo == NULL,
                            invalid_invisible_myupdateversion );

            IDE_TEST_RAISE( aRetryInfo->mRetryInfo->mIsRowRetry == ID_FALSE,
                            invalid_invisible_myupdateversion );
        }
    }
    else
    {
        IDE_TEST( sdcTableCTL::checkAndMakeSureRowStamping(
                                      aStatistics,
                                      &sStartInfo,
                                      sUpdateSlot,
                                      SDB_SINGLE_PAGE_READ )
                  != IDE_SUCCESS );
    }

    // Row Stamping�� ����Ǿ����� ������ �Ŀ� �ٽ� OldRowHdrInfo�� ��´�.
    getRowHdrInfo( sUpdateSlot, &sOldRowHdrInfo );

    /* BUG-30911 - [5.3.7] 2 rows can be selected during executing index scan
     *             on unique index. 
     * deleteRowPiece() ����. */
    if ( SM_SCN_IS_DELETED(sOldRowHdrInfo.mInfiniteSCN) == ID_TRUE )
    {
        /* ���� ���� ����ϰ� releaseLatch */
        IDE_RAISE( rebuild_already_modified );
    }

    if ( ( sUptState == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED ) ||
         ( ( aRetryInfo->mIsAlreadyModified == ID_TRUE ) &&
         ( sRetryCnt == 1 ) ) ) // �Լ� ���� retry �ÿ��� �ٽ� Ȯ�� �� �ʿ����.
    {
        if ( aRetryInfo->mRetryInfo != NULL )
        {
            IDE_TEST( checkRetry( aTrans,
                                  &sMtx,
                                  &sSP,
                                  sUpdateSlot,
                                  sOldRowHdrInfo.mRowFlag,
                                  aForbiddenToRetry,
                                  *aStmtViewSCN,
                                  aRetryInfo,
                                  sOldRowHdrInfo.mColCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* ���� ���� ����ϰ� releaseLatch */
            IDE_RAISE( rebuild_already_modified );
        }
    }

    sNextRowPieceSID = getNextRowPieceSID(sUpdateSlot);

    /* update ������ �����ϱ� ���� supplement job info�� �ʱ�ȭ�Ѵ�. */
    SDC_INIT_SUPPLEMENT_JOB_INFO(sSupplementJobInfo);

    /* update ������ �����ϱ� ���� update info�� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( makeUpdateInfo( sUpdateSlot,
                              aColList,
                              aValueList,
                              aCurrRowPieceSID,
                              sOldRowHdrInfo.mColCount,
                              aUpdateStatus->mFstColumnSeq,
                              aValueModeList,
                              &sUpdateInfo )
              != IDE_SUCCESS );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          sNewCTSlotIdx,
                          *aCSInfiniteSCN,
                          SD_NULL_RID,
                          sOldRowHdrInfo.mColCount,
                          sOldRowHdrInfo.mRowFlag,
                          sFstDskViewSCN );

    /* old row header info�� new row header info��
     * update info �ڷᱸ���� �Ŵܴ�. */
    sUpdateInfo.mOldRowHdrInfo = &sOldRowHdrInfo;
    sUpdateInfo.mNewRowHdrInfo = &sNewRowHdrInfo;

    IDE_TEST( analyzeRowForUpdate( sUpdateSlot,
                                   aColList,
                                   aValueList,
                                   sOldRowHdrInfo.mColCount,
                                   aUpdateStatus->mFstColumnSeq,
                                   aUpdateStatus->mLstUptColumnSeq,
                                   &sUpdateInfo )
              != IDE_SUCCESS );

    /*
     * SQL�� Full Update�� �߻��ϴ� ��� ���� LOB �� ������ �־�� �Ѵ�.
     * API�� ȣ��� ���� API�� ������ �ֹǷ� ���� ���� �۾��� �ʿ� ����.
     */
    if ( (sUpdateInfo.mUptBfrLobDescCnt > 0) &&
         (sUpdateInfo.mIsUptLobByAPI != ID_TRUE) )
    {
        SDC_ADD_SUPPLEMENT_JOB( &sSupplementJobInfo,
                                SDC_SUPPLEMENT_JOB_REMOVE_OLD_OUT_MODE_LOB );
    }
    
    if ( aReplicate == ID_TRUE )
    {
        /* replication�� �ɸ� ���,
         * rowpiece���� pk column ������ �ִ����� ã�Ƽ�,
         * pk column ������ �����Ѵ�. */
        IDE_ASSERT( aPKInfo != NULL );
        if ( sIsCopyPKInfo != ID_TRUE )
        {
            if ( aPKInfo->mTotalPKColCount !=
                 aPKInfo->mCopyDonePKColCount )
            {
                copyPKInfo( sUpdateSlot,
                            &sUpdateInfo,
                            sOldRowHdrInfo.mColCount,
                            aPKInfo );

                sIsCopyPKInfo = ID_TRUE;
            }
        }
    }

    if ( sUpdateInfo.mIsDeleteFstColumnPiece == ID_TRUE )
    {
        /* rowpiece���� ù��° �÷��� update�ؾ� �ϴµ�,
         * ù��° �÷��� prev rowpiece�κ��� �̾��� ���,
         * delete first column piece ������ �����ؾ� �Ѵ�. */

        /* ���� rowpiece�� ������ ����� �÷��� update�Ǵ� ���,
         * �� �÷��� first piece�� �����ϰ� �ִ� rowpiece����
         * update ������ ����ϹǷ�, ������ rowpiece������
         * �ش� column piece�� �����ؾ� �Ѵ�. */

        if ( ( sOldRowHdrInfo.mColCount == 1 ) &&
             ( sUpdateInfo.mIsTrailingNullUpdate == ID_FALSE ) )
        {
            /* rowpiece���� �÷������� 1���̰�,
             * trailing null update�� �߻����� �ʾ�����
             * rowpiece ��ü�� remove �Ѵ�. */
            IDE_TEST( removeRowPiece4Upt( aStatistics,
                                          aTableHeader,
                                          &sMtx,
                                          &sStartInfo,
                                          sUpdateSlot,
                                          &sUpdateInfo,
                                          aReplicate )
                      != IDE_SUCCESS );

            sIsRowPieceDeletedByItSelf = ID_TRUE;

            if ( SM_IS_FLAG_ON( sOldRowHdrInfo.mRowFlag,
                                SDC_ROWHDR_N_FLAG )
                != ID_TRUE )
            {
                aUpdateStatus->mUpdateDoneColCount++;
                aUpdateStatus->mFstColumnSeq++;
            }

            /* rowpiece�� remove�ϰ� �Ǹ�
             * previous rowpiece�� link������ ������ �־�� �ϹǷ�
             * supplement job�� ����Ѵ�. */
            SDC_ADD_SUPPLEMENT_JOB( &sSupplementJobInfo,
                                    SDC_SUPPLEMENT_JOB_CHANGE_ROWPIECE_LINK );

            sSupplementJobInfo.mNextRowPieceSID = sNextRowPieceSID;

            IDE_CONT(cont_update_end);
        }
        else
        {
            /* rowpiece���� ù��° column piece�� �����Ѵ�. */
            IDE_TEST( deleteFstColumnPiece( aStatistics,
                                            aTrans,
                                            aTableHeader,
                                            aCSInfiniteSCN,
                                            &sMtx,
                                            &sStartInfo,
                                            sUpdateSlot,
                                            aCurrRowPieceSID,
                                            &sUpdateInfo,
                                            sNextRowPieceSID,
                                            aReplicate )
                      != IDE_SUCCESS );

            aUpdateStatus->mUpdateDoneColCount++;
            aUpdateStatus->mFstColumnSeq++;

            sState = 0;
            /* Update ������ ������ Mini Transaction�� Commit�Ѵ� */
            IDE_TEST( sdrMiniTrans::commit( &sMtx, SMR_CT_CONTINUE )
                      != IDE_SUCCESS );

            /* delete first column piece ������ ������ ��,
             * update rowpiece ������ �ٽ� �����Ѵ�. */
            goto reentry;
        }
    }
    IDE_ASSERT( sUpdateInfo.mIsDeleteFstColumnPiece == ID_FALSE );

    if ( sUpdateInfo.mIsTrailingNullUpdate == ID_TRUE )
    {
        /* trailing null update�� �����ϴ� ��� */

        /* trailing null update ó���� last rowpiece������ ó���ؾ� �Ѵ�. */
        if (SDC_IS_LAST_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_FALSE )
        {
            ideLog::log( IDE_ERR_0,
                         "SpaceID: %"ID_UINT32_FMT", "
                         "TableOID: %"ID_vULONG_FMT", "
                         "RowPieceSID: %"ID_UINT64_FMT"\n",
                         sTableSpaceID,
                         sTableOID,
                         aCurrRowPieceSID );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aTableHeader,
                            ID_SIZEOF(smcTableHeader),
                            "TableHeader Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sUpdateSlot,
                                         "Page Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&sOldRowHdrInfo,
                            ID_SIZEOF(sdcRowHdrInfo),
                            "OldRowHdrInfo Dump:" );

            IDE_ASSERT( 0 );
        }

        if ( aUpdateStatus->mTotalUpdateColCount <=
             ( aUpdateStatus->mUpdateDoneColCount +
               sUpdateInfo.mUptAftInModeColCnt    +
               sUpdateInfo.mUptAftLobDescCnt ) )
        {
            ideLog::log( IDE_ERR_0,
                         "SpaceID: %"ID_UINT32_FMT", "
                         "TableOID: %"ID_vULONG_FMT", "
                         "RowPieceSID: %"ID_UINT64_FMT", "
                         "UpdateStatus.TotalUpdateColCount: %"ID_UINT32_FMT", "
                         "UpdateStatus.UpdateDoneColCount: %"ID_UINT32_FMT", "
                         "UpdateInfo.mUptAftInModeColCnt: %"ID_UINT32_FMT", "
                         "UpdateInfo.mUptAftLobDescCnt: %"ID_UINT32_FMT"\n",
                         sTableSpaceID,
                         sTableOID,
                         aCurrRowPieceSID,
                         aUpdateStatus->mTotalUpdateColCount,
                         aUpdateStatus->mUpdateDoneColCount,
                         sUpdateInfo.mUptAftInModeColCnt,
                         sUpdateInfo.mUptAftLobDescCnt );
            
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aUpdateStatus,
                            ID_SIZEOF(sdcRowUpdateStatus),
                            "UpdateStatus Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&sUpdateInfo,
                            ID_SIZEOF(sdcRowPieceUpdateInfo),
                            "UpdateInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aTableHeader,
                            ID_SIZEOF(smcTableHeader),
                            "TableHeader Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sUpdateSlot,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        /* BUG-32234: If update operation fails to make trailing null,
         * debug information needs to be recorded. */
        if ( makeTrailingNullUpdateInfo( aTableHeader,
                                         sUpdateInfo.mNewRowHdrInfo,
                                         &sUpdateInfo,
                                         aUpdateStatus->mFstColumnSeq,
                                         aColList,
                                         aValueList,
                                         aValueModeList )
             != IDE_SUCCESS )
        {
            ideLog::logMem( IDE_DUMP_0,
                            sdpPhyPage::getPageStartPtr(sUpdateSlot),
                            SD_PAGE_SIZE,
                            "[ Page Dump ]\n" );

            IDE_ERROR( 0 );
        }
    }

    if ( ( sUpdateInfo.mUptAftInModeColCnt == 0 ) &&
         ( sUpdateInfo.mUptAftLobDescCnt == 0 ) )
    {
        // Head Row Piece��� Lock Row�� ��´�.
        // ������, LOB API Update�� ��� �̹� ���� �ִ�.
        if ( ( SDC_IS_HEAD_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_TRUE ) &&
             ( sUpdateInfo.mIsUptLobByAPI != ID_TRUE ))
        {
            // head rowpiece�� implicit lock�� �Ǵ�.
            IDE_TEST( lockRow( aStatistics,
                               &sMtx,
                               &sStartInfo,
                               aCSInfiniteSCN,
                               sTableOID,
                               sUpdateSlot,
                               aCurrRowPieceSID,
                               sUpdateInfo.mOldRowHdrInfo->mCTSlotIdx,
                               sUpdateInfo.mNewRowHdrInfo->mCTSlotIdx,
                               ID_FALSE ) /* aIsExplicitLock */
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do..
        }
    }
    else
    {
        IDE_TEST( doUpdate( aStatistics,
                            aTrans,
                            aTableInfoPtr,
                            aTableHeader,
                            &sMtx,
                            &sStartInfo,
                            sUpdateSlot,
                            sCurrRowPieceGRID,
                            &sUpdateInfo,
                            aUpdateStatus->mFstColumnSeq,
                            aReplicate,
                            sNextRowPieceSID,
                            &sOverwriteInfo,
                            &sSupplementJobInfo,
                            &sLstInsertRowPieceSID )
                  != IDE_SUCCESS );
    }

    /* update ������� ������ �����Ѵ�. */
    resetRowUpdateStatus( &sOldRowHdrInfo,
                          &sUpdateInfo,
                          aUpdateStatus );


    IDE_EXCEPTION_CONT(cont_update_end);

    sState = 0;
    /* Update ������ ������ Mini Transaction�� Commit�Ѵ� */
    IDE_TEST( sdrMiniTrans::commit( &sMtx, SMR_CT_CONTINUE )
              != IDE_SUCCESS );

    *aNextRowPieceSID           = sNextRowPieceSID;
    *aLstInsertRowPieceSID      = sLstInsertRowPieceSID;
    *aIsRowPieceDeletedByItSelf = sIsRowPieceDeletedByItSelf;

    if ( SDC_EXIST_SUPPLEMENT_JOB( &sSupplementJobInfo ) == ID_TRUE )
    {
        IDE_TEST( doSupplementJob( aStatistics,
                                   &sStartInfo,
                                   aTableHeader,
                                   sCurrRowPieceGRID,
                                   *aCSInfiniteSCN,
                                   aPrevRowPieceSID,
                                   &sSupplementJobInfo,
                                   &sUpdateInfo,
                                   &sOverwriteInfo )
                  != IDE_SUCCESS );
    }

    if ( sUpdateInfo.mUptAftLobDescCnt > 0 )
    {
        aUpdateStatus->mUpdateDoneColCount += sUpdateInfo.mUptAftLobDescCnt;
    }

    /* FIT/ART/sm/Design/Recovery/Bugs/BUG-17451/BUG-17451.tc */
    IDU_FIT_POINT( "1.BUG-17451@sdcRow::updateRowPiece" );    

    /* BUG-32278 The previous flag is set incorrectly in row flag when
     * a rowpiece is overflowed. */
    aUpdateStatus->mPrevRowPieceRowFlag = sOldRowHdrInfo.mRowFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION( rebuild_already_modified )
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE );

            SChar    sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            sdpCTS * sCTS;
            smSCN    sFSCNOrCSCN;
            UChar    sCTSlotIdx = sOldRowHdrInfo.mCTSlotIdx;
            sdcRowHdrExInfo sRowHdrExInfo;

            if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
            {
                sCTS = sdcTableCTL::getCTS( sdpPhyPage::getHdr(sUpdateSlot),sCTSlotIdx );
                SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );
            }
            else
            {
                getRowHdrExInfo( sUpdateSlot, &sRowHdrExInfo );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowHdrExInfo.mFSCNOrCSCN );
            }

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[RECORD VALIDATION(U)] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CSInfiniteSCN:%"ID_UINT64_FMT", "
                             "CTSlotIdx:%"ID_UINT32_FMT", "
                             "FSCNOrCSCN:%"ID_UINT64_FMT", "
                             "InfiniteSCN:%"ID_UINT64_FMT", "
                             "Deleted:%s ",
                             sTableSpaceID,
                             sTableOID,
                             SM_SCN_TO_LONG( *aStmtViewSCN ),
                             SM_SCN_TO_LONG( *aCSInfiniteSCN ),
                             sCTSlotIdx,
                             SM_SCN_TO_LONG( sFSCNOrCSCN ),
                             SM_SCN_TO_LONG( sOldRowHdrInfo.mInfiniteSCN ),
                             SM_SCN_IS_DELETED( sOldRowHdrInfo.mInfiniteSCN)?"Y":"N" );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_RETRY_Already_Modified ) );
        }

        IDE_ASSERT( releaseLatchForAlreadyModify( &sMtx, &sSP )
                    == IDE_SUCCESS );
    }
    IDE_EXCEPTION( invalid_invisible_myupdateversion )
    {
        // �̷� ���� ����� �Ѵ�.
        ideLog::log( IDE_ERR_0,
                     "SpaceID: %"ID_UINT32_FMT", "
                     "TableOID: %"ID_vULONG_FMT", "
                     "RowPieceSID: %"ID_UINT64_FMT", "
                     "StatementSCN: %"ID_UINT64_FMT", "
                     "CSInfiniteSCN: %"ID_UINT64_FMT", "
                     "IsUptLobByAPI: %"ID_UINT32_FMT"\n",
                     sTableSpaceID,
                     sTableOID,
                     aCurrRowPieceSID,
                     SM_SCN_TO_LONG( *aStmtViewSCN ),
                     SM_SCN_TO_LONG( *aCSInfiniteSCN ),
                     sIsUptLobByAPI );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "TableHeader Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sUpdateSlot,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&sOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        IDE_ASSERT(0);

    }
    IDE_EXCEPTION( error_invalid_rowflag )
    {
        ideLog::log( IDE_ERR_0,
                     "TableColCount:   %"ID_UINT32_FMT"\n"
                     "PrevRowFlag:     %"ID_UINT32_FMT"\n"
                     "RowFlag:         %"ID_UINT32_FMT"\n"
                     "PrevRowPieceSID: %"ID_vULONG_FMT"\n"
                     "CurrRowPieceSID: %"ID_vULONG_FMT"\n"
                     "NextRowPieceSID: %"ID_vULONG_FMT"\n",
                     smLayerCallback::getColumnCount( aTableHeader ),
                     sPrevRowFlag,
                     sRowFlag,
                     aPrevRowPieceSID,
                     aCurrRowPieceSID,
                     *aNextRowPieceSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateStatus,
                        ID_SIZEOF(sdcRowUpdateStatus),
                        "[ Update Status ]" );

        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG,
                                 "invalid row flag") );
        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::doUpdate( idvSQL                      *aStatistics,
                         void                        *aTrans,
                         void                        *aTableInfoPtr,
                         void                        *aTableHeader,
                         sdrMtx                      *aMtx,
                         sdrMtxStartInfo             *aStartInfo,
                         UChar                       *aSlotPtr,
                         scGRID                       aSlotGRID,
                         const sdcRowPieceUpdateInfo *aUpdateInfo,
                         UShort                       aFstColumnSeq,
                         idBool                       aReplicate,
                         sdSID                        aNextRowPieceSID,
                         sdcRowPieceOverwriteInfo    *aOverwriteInfo,
                         sdcSupplementJobInfo        *aSupplementJobInfo,
                         sdSID                       *aLstInsertRowPieceSID )
{
    sdpSelfAgingFlag   sCheckFlag;

    IDE_ASSERT( aTrans                != NULL );
    IDE_ASSERT( aTableInfoPtr         != NULL );
    IDE_ASSERT( aTableHeader          != NULL );
    IDE_ASSERT( aMtx                  != NULL );
    IDE_ASSERT( aStartInfo            != NULL );
    IDE_ASSERT( aSlotPtr              != NULL );
    IDE_ASSERT( aUpdateInfo           != NULL );
    IDE_ASSERT( aOverwriteInfo        != NULL );
    IDE_ASSERT( aSupplementJobInfo    != NULL );
    IDE_ASSERT( aLstInsertRowPieceSID != NULL );

    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    /*
     * << Update�� Self-Aging ���� >>
     *
     * Self-Aging�� Update�ø��� �߻��ϴ� ���� �ƴϴ�. �������� AVFS�� ����
     * NewRowPieceSize�� �Ҵ����� ���Ҹ�ŭ ������ Self-Aging�� �õ��غ���.
     * Self-Aging���� ���� AVFS�� �� �� Ȯ���� ���� �ֱ⶧���� ������������
     * ����� NewRowPiece�� ������������ �ٷ� ������ ������ ���� �ֱ� �����̴�.
     */
    if ( sdpPhyPage::canAllocSlot( sdpPhyPage::getHdr(aSlotPtr),
                                   aUpdateInfo->mNewRowPieceSize,
                                   ID_FALSE,
                                   SDP_1BYTE_ALIGN_SIZE ) == ID_FALSE )
    {
        IDE_TEST( sdcTableCTL::checkAndRunSelfAging(
                                  aStatistics,
                                  aStartInfo,
                                  sdpPhyPage::getHdr(aSlotPtr),
                                  &sCheckFlag ) != IDE_SUCCESS );
    }

    if ( ( aUpdateInfo->mIsTrailingNullUpdate == ID_TRUE ) ||
         ( canReallocSlot( aSlotPtr, aUpdateInfo->mNewRowPieceSize )
           != ID_TRUE ) )
    {
        IDE_TEST( processOverflowData( aStatistics,
                                       aTrans,
                                       aTableInfoPtr,
                                       aTableHeader,
                                       aMtx,
                                       aStartInfo,
                                       aSlotPtr,
                                       aSlotGRID,
                                       aUpdateInfo,
                                       aFstColumnSeq,
                                       aReplicate,
                                       aNextRowPieceSID,
                                       aOverwriteInfo,
                                       aSupplementJobInfo,
                                       aLstInsertRowPieceSID )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aUpdateInfo->mIsUpdateInplace == ID_TRUE )
        {
            IDE_TEST( updateInplace( aStatistics,
                                     aTableHeader,
                                     aMtx,
                                     aStartInfo,
                                     aSlotPtr,
                                     aSlotGRID,
                                     aUpdateInfo,
                                     aReplicate )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( updateOutplace( aStatistics,
                                      aTableHeader,
                                      aMtx,
                                      aStartInfo,
                                      aSlotPtr,
                                      aSlotGRID,
                                      aUpdateInfo,
                                      aNextRowPieceSID,
                                      aReplicate )
                      != IDE_SUCCESS );
        }

        /*
         * SQL�� Full Update�� �߻��ϴ� ��� ���� LOB�� ������ ���־�� �Ѵ�.
         * API�� ȣ��� ���� API�� ���ֹǷ� ���� ���ִ� �۾��� �ʿ� ����.
         */
        if ( (aUpdateInfo->mUptAftLobDescCnt > 0) &&
             (aUpdateInfo->mIsUptLobByAPI != ID_TRUE) )
        {
            SDC_ADD_SUPPLEMENT_JOB( aSupplementJobInfo,
                                    SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_UPDATE );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  supplement job���� �ΰ��� ������ �ִ�.
 *  1. lob update
 *  2. change rowpiece link
 *
 **********************************************************************/
IDE_RC sdcRow::doSupplementJob( idvSQL                            *aStatistics,
                                sdrMtxStartInfo                   *aStartInfo,
                                void                              *aTableHeader,
                                scGRID                             aSlotGRID,
                                smSCN                              aCSInfiniteSCN,
                                sdSID                              aPrevRowPieceSID,
                                const sdcSupplementJobInfo        *aSupplementJobInfo,
                                const sdcRowPieceUpdateInfo       *aUpdateInfo,
                                const sdcRowPieceOverwriteInfo    *aOverwriteInfo )
{
    IDE_ASSERT( aStartInfo            != NULL );
    IDE_ASSERT( aTableHeader          != NULL );
    IDE_ASSERT( aSupplementJobInfo    != NULL );
    IDE_ASSERT( aUpdateInfo           != NULL );
    IDE_ASSERT( aOverwriteInfo        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));
    IDE_ASSERT( SDC_EXIST_SUPPLEMENT_JOB( aSupplementJobInfo ) == ID_TRUE );

    if ( ( aSupplementJobInfo->mJobType & SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_UPDATE )
         == SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_UPDATE )
    {
        if ( aUpdateInfo->mUptAftLobDescCnt <= 0 )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aUpdateInfo,
                            ID_SIZEOF(sdcRowPieceUpdateInfo),
                            "UpdateInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aSupplementJobInfo,
                            ID_SIZEOF(sdcSupplementJobInfo),
                            "SupplementJobInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&aSlotGRID,
                            ID_SIZEOF(scGRID),
                            "SlotGRID Dump:" );

            ideLog::log( IDE_ERR_0,
                         "CSInfiniteSCN: %"ID_UINT64_FMT", "
                         "PrevRowPieceSID: %"ID_UINT64_FMT"\n",
                         aCSInfiniteSCN,
                         aPrevRowPieceSID );

            IDE_ASSERT( 0 );
        }

        IDE_TEST( updateAllLobCols( aStatistics,
                                    aStartInfo,
                                    aTableHeader,
                                    aUpdateInfo,
                                    aUpdateInfo->mNewRowHdrInfo->mColCount,
                                    aSlotGRID )
                  != IDE_SUCCESS );
    }

    if ( ( aSupplementJobInfo->mJobType & SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_OVERWRITE )
          == SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_OVERWRITE )
    {
        if ( aOverwriteInfo->mUptAftLobDescCnt <= 0 )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aOverwriteInfo,
                            ID_SIZEOF(sdcRowPieceOverwriteInfo),
                            "OverwriteInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aSupplementJobInfo,
                            ID_SIZEOF(sdcSupplementJobInfo),
                            "SupplementJobInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&aSlotGRID,
                            ID_SIZEOF(scGRID),
                            "SlotGRID Dump:" );

            ideLog::log( IDE_ERR_0,
                         "CSInfiniteSCN: %"ID_UINT64_FMT", "
                         "PrevRowPieceSID: %"ID_UINT64_FMT"\n",
                         aCSInfiniteSCN,
                         aPrevRowPieceSID );

            IDE_ASSERT( 0 );
        }


        IDE_TEST( updateAllLobCols( aStatistics,
                                    aStartInfo,
                                    aTableHeader,
                                    aOverwriteInfo,
                                    aOverwriteInfo->mNewRowHdrInfo->mColCount,
                                    aSlotGRID )
                  != IDE_SUCCESS );
    }    

    if ( ( aSupplementJobInfo->mJobType & SDC_SUPPLEMENT_JOB_CHANGE_ROWPIECE_LINK )
         == SDC_SUPPLEMENT_JOB_CHANGE_ROWPIECE_LINK  )
    {
        IDE_TEST( changeRowPieceLink( aStatistics,
                                      aStartInfo->mTrans,
                                      aTableHeader,
                                      &aCSInfiniteSCN,
                                      aPrevRowPieceSID,
                                      aSupplementJobInfo->mNextRowPieceSID )
                  != IDE_SUCCESS );
    }

    // Update Column�� ������ LOB Descriptor�̾��� ���
    // Old LOB Page�� �����Ѵ�.
    if ( ( aSupplementJobInfo->mJobType & SDC_SUPPLEMENT_JOB_REMOVE_OLD_OUT_MODE_LOB )
         == SDC_SUPPLEMENT_JOB_REMOVE_OLD_OUT_MODE_LOB )
    {
        if ( aUpdateInfo->mUptBfrLobDescCnt <= 0 )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aUpdateInfo,
                            ID_SIZEOF(sdcRowPieceUpdateInfo),
                            "UpdateInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aSupplementJobInfo,
                            ID_SIZEOF(sdcSupplementJobInfo),
                            "SupplementJobInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&aSlotGRID,
                            ID_SIZEOF(scGRID),
                            "SlotGRID Dump:" );

            ideLog::log( IDE_ERR_0,
                         "CSInfiniteSCN: %"ID_UINT64_FMT", "
                         "PrevRowPieceSID: %"ID_UINT64_FMT"\n",
                         SM_SCN_TO_LONG( aCSInfiniteSCN ),
                         aPrevRowPieceSID );

            IDE_ASSERT( 0 );
        }

        removeOldLobPage4Upt( aStatistics,
                              aStartInfo->mTrans,
                              aUpdateInfo );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  update ���������� �ʱ�ȭ�Ѵ�.
 *
 **********************************************************************/
void sdcRow::initRowUpdateStatus( const smiColumnList     *aColumnList,
                                  sdcRowUpdateStatus      *aUpdateStatus )
{
    const smiColumnList  *sColumnList;
    UShort                sUpdateColCount  = 0;
    UShort                sLstUptColumnSeq;

    IDE_ASSERT( aColumnList   != NULL );
    IDE_ASSERT( aUpdateStatus != NULL );

    sColumnList = aColumnList;

    while(sColumnList != NULL)
    {
        sUpdateColCount++;
        sLstUptColumnSeq = SDC_GET_COLUMN_SEQ(sColumnList->column);
        sColumnList      = sColumnList->next;
    }

    aUpdateStatus->mTotalUpdateColCount = sUpdateColCount;

    aUpdateStatus->mUpdateDoneColCount  = 0;
    aUpdateStatus->mFstColumnSeq        = 0;
    aUpdateStatus->mLstUptColumnSeq     = sLstUptColumnSeq;
    aUpdateStatus->mPrevRowPieceRowFlag = SDC_ROWHDR_FLAG_ALL;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
void sdcRow::resetRowUpdateStatus( const sdcRowHdrInfo          *aRowHdrInfo,
                                   const sdcRowPieceUpdateInfo  *aUpdateInfo,
                                   sdcRowUpdateStatus           *aUpdateStatus )
{
    const sdcColumnInfo4Update  *sLstColumnInfo;
    UShort                       sColCount;
    UChar                        sRowFlag;

    IDE_ASSERT( aRowHdrInfo   != NULL );
    IDE_ASSERT( aUpdateInfo   != NULL );
    IDE_ASSERT( aUpdateStatus != NULL );

    sColCount = aRowHdrInfo->mColCount;
    sRowFlag  = aRowHdrInfo->mRowFlag;
    sLstColumnInfo = aUpdateInfo->mColInfoList + (sColCount-1);

    // reset mFstColumnSeq
    if ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE )
    {
        aUpdateStatus->mFstColumnSeq += (sColCount - 1);
    }
    else
    {
        aUpdateStatus->mFstColumnSeq += sColCount;
    }

    // reset mUpdateDoneColCount
    if ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE )
    {
        /* mColumn�� ���� ���� update �÷����� ���θ� �Ǵ��Ѵ�.
         * mColumn == NULL : update �÷� X
         * mColumn != NULL : update �÷� O */
        if ( SDC_IS_UPDATE_COLUMN( sLstColumnInfo->mColumn ) == ID_FALSE )
        {
            /* last column�� update �÷��� �ƴ� ��� */
            aUpdateStatus->mUpdateDoneColCount +=
                aUpdateInfo->mUptAftInModeColCnt;
        }
        else
        {
            /* last column�� update �÷��� ��� */
            
            /*
             * ����� ���� row piece���� �ش� �÷��� ������ ���
             * mUpdateDoneColCount�� �����ǹǷ� ���⼭�� -1 ���ش�.
             */
            aUpdateStatus->mUpdateDoneColCount +=
                (aUpdateInfo->mUptAftInModeColCnt - 1);
        }
    }
    else
    {
        aUpdateStatus->mUpdateDoneColCount +=
            aUpdateInfo->mUptAftInModeColCnt;
    }
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
idBool sdcRow::isUpdateEnd( sdSID                       aNextRowPieceSID,
                            const sdcRowUpdateStatus   *aUpdateStatus,
                            idBool                      aReplicate,
                            const sdcPKInfo            *aPKInfo )
{
    idBool    sRet;

    IDE_ASSERT( aUpdateStatus != NULL );

    if ( aNextRowPieceSID == SD_NULL_SID )
    {
        return ID_TRUE;
    }

    if ( aUpdateStatus->mTotalUpdateColCount <
         aUpdateStatus->mUpdateDoneColCount )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateStatus,
                        ID_SIZEOF(sdcRowUpdateStatus),
                        "UpdateStatus Dump:" );
        IDE_ASSERT( 0 );
    }


    if ( aUpdateStatus->mTotalUpdateColCount ==
         aUpdateStatus->mUpdateDoneColCount)
    {
        if (aReplicate == ID_TRUE)
        {
            IDE_ASSERT( aPKInfo != NULL );

            if ( aPKInfo->mTotalPKColCount < aPKInfo->mCopyDonePKColCount )
            {
                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)aPKInfo,
                                ID_SIZEOF(sdcPKInfo),
                                "PKInfo Dump:" );
                IDE_ASSERT( 0 );
            }

            if ( aPKInfo->mTotalPKColCount ==
                 aPKInfo->mCopyDonePKColCount )
            {
                sRet = ID_TRUE;
            }
            else
            {
                sRet = ID_FALSE;
            }
        }
        else
        {
            sRet = ID_TRUE;
        }
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::updateInplace( idvSQL                      *aStatistics,
                              void                        *aTableHeader,
                              sdrMtx                      *aMtx,
                              sdrMtxStartInfo             *aStartInfo,
                              UChar                       *aSlotPtr,
                              scGRID                       aSlotGRID,
                              const sdcRowPieceUpdateInfo *aUpdateInfo,
                              idBool                       aReplicate )
{
    sdcRowHdrInfo               * sNewRowHdrInfo;
    const sdcColumnInfo4Update  * sColumnInfo;
    UChar                       * sColPiecePtr;
    UInt                          sColLen;
    sdSID                         sSlotSID;
    sdSID                         sUndoSID = SD_NULL_SID;
    UShort                        sColCount;
    scGRID                        sSlotGRID;
    scSpaceID                     sTableSpaceID;
    smOID                         sTableOID;
    UShort                        sColSeqInRowPiece;
    SShort                        sFSCreditSize = 0;
    sdcUndoSegment              * sUDSegPtr;

    IDE_ASSERT( aTableHeader  != NULL );
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aStartInfo    != NULL );
    IDE_ASSERT( aSlotPtr      != NULL );
    IDE_ASSERT( aUpdateInfo   != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( ( aUpdateInfo->mIsUpdateInplace == ID_FALSE ) ||
         ( (aUpdateInfo->mUptAftInModeColCnt +
            aUpdateInfo->mUptAftLobDescCnt ) <= 0 ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) !=
           ( aUpdateInfo->mUptBfrInModeColCnt +
             aUpdateInfo->mUptBfrLobDescCnt ) ) ||
         ( aUpdateInfo->mOldRowPieceSize !=
                 aUpdateInfo->mNewRowPieceSize ) ||
         ( aUpdateInfo->mOldRowHdrInfo->mColCount !=
                 aUpdateInfo->mNewRowHdrInfo->mColCount ) ||
         ( aUpdateInfo->mOldRowHdrInfo->mRowFlag  !=
                 aUpdateInfo->mNewRowHdrInfo->mRowFlag ) )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "NewRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "TableHeader Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "SlotGRID Dump:" );

        IDE_ASSERT( 0 );
    }

    sNewRowHdrInfo = aUpdateInfo->mNewRowHdrInfo;
    sColCount      = sNewRowHdrInfo->mColCount;

    sTableSpaceID  = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID      = smLayerCallback::getTableOID( aTableHeader );

    sSlotSID       = aUpdateInfo->mRowPieceSID;
    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(sSlotSID),
                               SD_MAKE_SLOTNUM(sSlotSID) );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)(aStartInfo->mTrans) );
    if ( sUDSegPtr->addUpdateRowPieceUndoRec( aStatistics,
                                              aStartInfo,
                                              sTableOID,
                                              aSlotPtr,
                                              sSlotGRID,
                                              aUpdateInfo,
                                              aReplicate,
                                              &sUndoSID) != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� ��  */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }

    sNewRowHdrInfo->mUndoSID = sUndoSID;

    sFSCreditSize = calcFSCreditSize( aUpdateInfo->mOldRowPieceSize,
                                      aUpdateInfo->mNewRowPieceSize );

    if ( sFSCreditSize != 0 )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "NewRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "TableHeader Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "SlotGRID Dump:" );

        IDE_ASSERT( 0 );
    }

    writeRowHdr( aSlotPtr, sNewRowHdrInfo );

    sColPiecePtr = getDataArea( aSlotPtr );

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        sColPiecePtr = getColPiece( sColPiecePtr,
                                    &sColLen,
                                    NULL,   /* aColVal */
                                    NULL ); /* In Out Mode */

        IDE_ASSERT_MSG( sColumnInfo->mOldValueInfo.mValue.length == sColLen,
                        "TableOID         : %"ID_vULONG_FMT"\n"
                        "Old value length : %"ID_UINT32_FMT"\n"
                        "Column length    : %"ID_UINT32_FMT"\n",
                        sTableOID,
                        sColumnInfo->mOldValueInfo.mValue.length,
                        sColLen );

        // 1. Update Column�� �ƴ� ���
        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_FALSE )
        {
            sColPiecePtr += sColLen;
            continue;
        }

        // 2. Update ������ ���İ� NULL�� ���
        if ( SDC_IS_NULL(&(sColumnInfo->mNewValueInfo.mValue)) == ID_TRUE )
        {
            /* NULL value�� �ٽ� NULL�� update �ϴ� ���,
             * nothing to do */
            if ( SDC_IS_NULL(&sColumnInfo->mOldValueInfo.mValue) == ID_FALSE )
            {
                ideLog::log( IDE_ERR_0,
                             "ColSeqInRowPiece: %"ID_UINT32_FMT", "
                             "ColCount: %"ID_UINT32_FMT", "
                             "ColLen: %"ID_UINT32_FMT"\n",
                             sColSeqInRowPiece,
                             sColCount,
                             sColLen );

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             aSlotPtr,
                                             "Page Dump:" );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sColumnInfo,
                                ID_SIZEOF(sdcColumnInfo4Update),
                                "ColumnInfo Dump:" );

                IDE_ASSERT( 0 );
            }
            
            continue;
        }

        // 3. Update ������ ���İ� Empty�� ���
        if ( SDC_IS_EMPTY(&(sColumnInfo->mNewValueInfo.mValue)) == ID_TRUE )
        {
            if ( (SDC_IS_EMPTY(&(sColumnInfo->mOldValueInfo.mValue)) == ID_FALSE) ||
                 (SDC_IS_LOB_COLUMN(sColumnInfo->mColumn)            == ID_FALSE) )
            {
                ideLog::log( IDE_ERR_0,
                             "ColSeqInRowPiece: %"ID_UINT32_FMT", "
                             "ColCount: %"ID_UINT32_FMT", "
                             "ColLen: %"ID_UINT32_FMT"\n",
                             sColSeqInRowPiece,
                             sColCount,
                             sColLen );

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             aSlotPtr,
                                             "Page Dump:" );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sColumnInfo,
                                ID_SIZEOF(sdcColumnInfo4Update),
                                "ColumnInfo Dump:" );

                IDE_ASSERT( 0 );
            }

            continue;
        }

        if ( sColumnInfo->mOldValueInfo.mInOutMode !=
             sColumnInfo->mNewValueInfo.mInOutMode )
        {
            ideLog::log( IDE_ERR_0,
                         "ColSeqInRowPiece: %"ID_UINT32_FMT", "
                         "ColCount: %"ID_UINT32_FMT", "
                         "ColLen: %"ID_UINT32_FMT"\n",
                         sColSeqInRowPiece,
                         sColCount,
                         sColLen );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aSlotPtr,
                                         "Page Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Update),
                            "ColumnInfo Dump:" );

            IDE_ASSERT( 0 );
        }

        // 4. Update

        /* update inplace ������ �����Ϸ���,
         * update �÷��� ũ�Ⱑ ���ؼ��� �ȵȴ�.(old size == new size) */
        IDE_ASSERT_MSG( sColumnInfo->mOldValueInfo.mValue.length ==
                        sColumnInfo->mNewValueInfo.mValue.length,
                        "Table OID        : %"ID_vULONG_FMT"\n"
                        "Old value length : %"ID_UINT32_FMT"\n"
                        "New value length : %"ID_UINT32_FMT"\n",
                        sTableOID,
                        sColumnInfo->mOldValueInfo.mValue.length,
                        sColumnInfo->mNewValueInfo.mValue.length );

        /* new value�� write�Ѵ�. */
        ID_WRITE_AND_MOVE_DEST( sColPiecePtr,
                                (UChar*)sColumnInfo->mNewValueInfo.mValue.value,
                                sColumnInfo->mNewValueInfo.mValue.length );
    }

    IDE_DASSERT_MSG( aUpdateInfo->mNewRowPieceSize ==
                     getRowPieceSize(aSlotPtr),
                     "Invalid Row Piece Size"
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     sTableOID,
                     aUpdateInfo->mNewRowPieceSize,
                     getRowPieceSize( aSlotPtr ) );

    sdrMiniTrans::setRefOffset(aMtx, sTableOID);

    IDE_TEST( writeUpdateRowPieceRedoUndoLog( aSlotPtr,
                                              aSlotGRID,
                                              aUpdateInfo,
                                              aReplicate,
                                              aMtx )
              != IDE_SUCCESS );
    /*
     * �ش� Ʈ����ǰ� ������� Row Piece�� Update�ϴ� ��쿡��
     * RowPiece�� CTSlotIdx�� Row TimeStamping�� ó���Ǿ�
     * SDP_CTS_IDX_NULL�� �ʱ�ȭ�Ǿ� �ִ�.
     * �̶��� Bind CTS�� �����Ͽ� RefCnt�� ������Ų��.
     * RefCnt�� ������ Row Piece�� �ߺ� ���� �۾�������
     * ���� �ѹ��� �����Ѵ�.
     */
    IDE_TEST( sdcTableCTL::bind(
                     aMtx,
                     sTableSpaceID,
                     aSlotPtr,
                     aUpdateInfo->mOldRowHdrInfo->mCTSlotIdx,
                     aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                     SD_MAKE_SLOTNUM(sSlotSID),
                     aUpdateInfo->mOldRowHdrInfo->mExInfo.mFSCredit,
                     sFSCreditSize,
                     ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::updateOutplace( idvSQL                      *aStatistics,
                               void                        *aTableHeader,
                               sdrMtx                      *aMtx,
                               sdrMtxStartInfo             *aStartInfo,
                               UChar                       *aOldSlotPtr,
                               scGRID                       aSlotGRID,
                               const sdcRowPieceUpdateInfo *aUpdateInfo,
                               sdSID                        aNextRowPieceSID,
                               idBool                       aReplicate )
{
    sdcRowHdrInfo      * sNewRowHdrInfo = aUpdateInfo->mNewRowHdrInfo;
    UChar              * sNewSlotPtr;
    sdpPageListEntry   * sEntry;
    scSpaceID            sTableSpaceID;
    smOID                sTableOID;
    SShort               sFSCreditSize;
    sdSID                sUndoSID = SD_NULL_SID;
    sdcUndoSegment     * sUDSegPtr;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aOldSlotPtr  != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( ( aUpdateInfo->mIsUpdateInplace == ID_TRUE ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) <= 0 ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) !=
           ( aUpdateInfo->mUptBfrInModeColCnt +
             aUpdateInfo->mUptBfrLobDescCnt ) ) ||
         ( aUpdateInfo->mOldRowHdrInfo->mColCount !=
           aUpdateInfo->mNewRowHdrInfo->mColCount ) ||
         ( aUpdateInfo->mOldRowHdrInfo->mRowFlag  !=
           aUpdateInfo->mNewRowHdrInfo->mRowFlag ) )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "NewRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "TableHeader Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aOldSlotPtr,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "SlotGRID Dump:" );

        ideLog::log( IDE_ERR_0,
                     "NextRowPieceSID: %"ID_UINT64_FMT"\n",
                     aNextRowPieceSID );

        IDE_ASSERT( 0 );
    }

    sEntry = (sdpPageListEntry*)(smcTable::getDiskPageListEntry(aTableHeader));
    IDE_ASSERT( sEntry != NULL );

    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)(aStartInfo->mTrans) );
    if ( sUDSegPtr->addUpdateRowPieceUndoRec( aStatistics,
                                              aStartInfo,
                                              sTableOID,
                                              aOldSlotPtr,
                                              aSlotGRID,
                                              aUpdateInfo,
                                              aReplicate,
                                              &sUndoSID) != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� ��  */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }

    IDE_TEST( reallocSlot( aUpdateInfo->mRowPieceSID,
                           aOldSlotPtr,
                           aUpdateInfo->mNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewSlotPtr )
              != IDE_SUCCESS );
    aOldSlotPtr = NULL;
    IDE_ASSERT( sNewSlotPtr != NULL );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( sNewSlotPtr ) )
                == smLayerCallback::getTableOID( aTableHeader ) );


    sFSCreditSize = calcFSCreditSize( aUpdateInfo->mOldRowPieceSize,
                                      aUpdateInfo->mNewRowPieceSize );

    if ( sFSCreditSize != 0 )
    {
        // reallocSlot�� �����Ŀ�, Segment�� ���� ���뵵
        // ���濬���� �����Ѵ�.
        IDE_TEST( sdpPageList::updatePageState(
                                      aStatistics,
                                      sTableSpaceID,
                                      sEntry,
                                      sdpPhyPage::getPageStartPtr(sNewSlotPtr),
                                      aMtx )
                  != IDE_SUCCESS );
    }

    sNewRowHdrInfo->mUndoSID = sUndoSID;

    if ( aUpdateInfo->mColInfoList[(sNewRowHdrInfo->mColCount - 1)].mColumn != NULL )
    {
        SM_SET_FLAG_OFF(sNewRowHdrInfo->mRowFlag, SDC_ROWHDR_N_FLAG);
    }

    (void)writeRowPiece4URP( sNewSlotPtr,
                             sNewRowHdrInfo,
                             aUpdateInfo,
                             aNextRowPieceSID );

    IDE_DASSERT_MSG( aUpdateInfo->mNewRowPieceSize == getRowPieceSize( sNewSlotPtr ),
                     "Invalid Row Piece Size - "
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     sTableOID,
                     aUpdateInfo->mNewRowPieceSize,
                     getRowPieceSize( sNewSlotPtr ) );

    sdrMiniTrans::setRefOffset(aMtx, sTableOID);

    IDE_TEST( writeUpdateRowPieceRedoUndoLog( sNewSlotPtr,
                                              aSlotGRID,
                                              aUpdateInfo,
                                              aReplicate,
                                              aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdcTableCTL::bind(
                     aMtx,
                     sTableSpaceID,
                     sNewSlotPtr,
                     aUpdateInfo->mOldRowHdrInfo->mCTSlotIdx,
                     aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                     SD_MAKE_SLOTNUM(aUpdateInfo->mRowPieceSID),
                     aUpdateInfo->mOldRowHdrInfo->mExInfo.mFSCredit,
                     sFSCreditSize,
                     ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Update Row Piece(Inplace, Outplace)�� Undo Record�� ����
 *
 *   SDC_UNDO_UPDATE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)], [column count(2)],
 *   | [column desc set size(1)], [column desc set(1~128)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
UChar* sdcRow::writeUpdateRowPieceUndoRecRedoLog(
                                    UChar                       *aWritePtr,
                                    const UChar                 *aOldSlotPtr,
                                    const sdcRowPieceUpdateInfo *aUpdateInfo )
{
    UChar                        *sWritePtr;
    sdcColumnDescSet              sColDescSet;
    UChar                         sColDescSetSize;
    const sdcColumnInfo4Update   *sColumnInfo;
    SShort                        sChangeSize;
    UChar                         sFlag;
    UShort                        sColCount;
    UShort                        sUptColCnt;
    UShort                        sDoWriteCnt;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aOldSlotPtr != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    if ( ( aUpdateInfo->mOldRowHdrInfo->mColCount !=
           aUpdateInfo->mNewRowHdrInfo->mColCount ) ||
         ( ( aUpdateInfo->mUptBfrInModeColCnt +
             aUpdateInfo->mUptBfrLobDescCnt ) <= 0 ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) !=
           ( aUpdateInfo->mUptBfrInModeColCnt +
             aUpdateInfo->mUptBfrLobDescCnt ) ) )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "NewRowHdrInfo Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aWritePtr,
                                     "Write Undo Page Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aOldSlotPtr,
                                     "Old Data Page Dump:" );

        IDE_ASSERT( 0 );
    }

    sColCount = aUpdateInfo->mOldRowHdrInfo->mColCount;

    sWritePtr = aWritePtr;

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE;
    if ( aUpdateInfo->mIsUpdateInplace == ID_TRUE )
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_INPLACE;
    }
    else
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_OUTPLACE;
    }
    ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sFlag );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = aUpdateInfo->mOldRowPieceSize -
                  aUpdateInfo->mNewRowPieceSize;

    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            &sChangeSize,
                            ID_SIZEOF(sChangeSize));

    sUptColCnt = aUpdateInfo->mUptBfrInModeColCnt + aUpdateInfo->mUptBfrLobDescCnt;

    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            &(sUptColCnt),
                            ID_SIZEOF(sUptColCnt) );

    sColDescSetSize = calcColumnDescSetSize( sColCount );
    ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sColDescSetSize );

    calcColumnDescSet( aUpdateInfo, sColCount, &sColDescSet );
    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            &sColDescSet,
                            sColDescSetSize );

    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            aOldSlotPtr,
                            SDC_ROWHDR_SIZE );

    sDoWriteCnt = 0;

    for ( sColSeqInRowPiece = 0 ;
         sColSeqInRowPiece < sColCount;
         sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            sWritePtr = writeColPiece( sWritePtr,
                                       (UChar*)sColumnInfo->mOldValueInfo.mValue.value,
                                       sColumnInfo->mOldValueInfo.mValue.length,
                                       sColumnInfo->mOldValueInfo.mInOutMode );
            sDoWriteCnt++;
        }
    }

    if ( sUptColCnt != sDoWriteCnt )
    {
        ideLog::log( IDE_ERR_0,
                     "UptColCnt: %"ID_UINT32_FMT", "
                     "DoWriteCnt: %"ID_UINT32_FMT", "
                     "ColCount: %"ID_UINT32_FMT"\n",
                     sUptColCnt,
                     sDoWriteCnt,
                     sColCount );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aWritePtr,
                                     "Write Undo Page Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aOldSlotPtr,
                                     "Old Data Page Dump:" );

        IDE_ASSERT( 0 );
    }

    return sWritePtr;
}


/***********************************************************************
 *
 * Description : Update Row Piece�� Redo, Undo Log
 *
 *   SDR_SDC_UPDATE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)], [column count(2)],
 *   | [column desc set size(1)], [column desc set(1~128)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeUpdateRowPieceRedoUndoLog(
                        const UChar                 *aSlotPtr,
                        scGRID                       aSlotGRID,
                        const sdcRowPieceUpdateInfo *aUpdateInfo,
                        idBool                       aReplicate,
                        sdrMtx                      *aMtx )
{
    sdcColumnDescSet              sColDescSet;
    UChar                         sColDescSetSize;
    const sdcColumnInfo4Update   *sColumnInfo;
    sdrLogType                    sLogType;
    UShort                        sLogSize;
    UShort                        sUptColCnt;
    UShort                        sDoWriteCnt;
    SShort                        sChangeSize;
    UChar                         sFlag;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( ( aUpdateInfo->mOldRowHdrInfo->mColCount !=
           aUpdateInfo->mNewRowHdrInfo->mColCount ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) <= 0 ) ||
         ( ( aUpdateInfo->mUptAftInModeColCnt +
             aUpdateInfo->mUptAftLobDescCnt ) !=
           ( aUpdateInfo->mUptBfrInModeColCnt +
             aUpdateInfo->mUptBfrLobDescCnt ) ) )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "NewRowHdrInfo Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "SlotGRID Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );


        IDE_ASSERT( 0 );
    }

    sColCount = aUpdateInfo->mNewRowHdrInfo->mColCount;

    sLogType = SDR_SDC_UPDATE_ROW_PIECE;

    sLogSize = calcUpdateRowPieceLogSize( aUpdateInfo,
                                          ID_FALSE,     /* aIsUndoRec */
                                          aReplicate );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE;
    if ( aUpdateInfo->mIsUpdateInplace == ID_TRUE )
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_INPLACE;
    }
    else
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_OUTPLACE;
    }
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sFlag,
                                  ID_SIZEOF(sFlag) )
              != IDE_SUCCESS );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = aUpdateInfo->mNewRowPieceSize -
                  aUpdateInfo->mOldRowPieceSize;

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &sChangeSize,
                                   ID_SIZEOF(UShort) )
            != IDE_SUCCESS );

    sUptColCnt = aUpdateInfo->mUptAftInModeColCnt + aUpdateInfo->mUptAftLobDescCnt;

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&(sUptColCnt),
                                  ID_SIZEOF(sUptColCnt))
              != IDE_SUCCESS );

    sColDescSetSize = calcColumnDescSetSize( sColCount );
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sColDescSetSize,
                                  ID_SIZEOF(sColDescSetSize) )
              != IDE_SUCCESS );

    calcColumnDescSet( aUpdateInfo, sColCount, &sColDescSet );
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sColDescSet,
                                  sColDescSetSize )
              != IDE_SUCCESS );


    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)aSlotPtr,
                                  SDC_ROWHDR_SIZE )
              != IDE_SUCCESS );

    sDoWriteCnt = 0;

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            IDE_TEST( writeColPieceLog( aMtx,
                                        (UChar*)sColumnInfo->mNewValueInfo.mValue.value,
                                        sColumnInfo->mNewValueInfo.mValue.length,
                                        sColumnInfo->mNewValueInfo.mInOutMode )
                      != IDE_SUCCESS );

            sDoWriteCnt++ ;
        }
    }

    if ( sUptColCnt != sDoWriteCnt )
    {
        ideLog::log( IDE_ERR_0,
                     "UptColCnt: %"ID_UINT32_FMT", "
                     "DoWriteCnt: %"ID_UINT32_FMT", "
                     "ColCount: %"ID_UINT32_FMT"\n",
                     sUptColCnt,
                     sDoWriteCnt,
                     sColCount );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    if ( aReplicate == ID_TRUE )
    {
        IDE_TEST( writeUpdateRowPieceLog4RP( aUpdateInfo,
                                             ID_FALSE,
                                             aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  update rowpiece undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *  UPDATE_ROW_PIECE �α״� redo, undo �α��� ������ ���� ������
 *  undo record���� undo info layer�� ���븸 ����,
 *  clr redo log�� �ۼ��ϸ� �ȴ�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeUpdateRowPieceCLR( const UChar   *aUndoRecHdr,
                                       scGRID         aSlotGRID,
                                       sdSID          aUndoSID,
                                       sdrMtx        *aMtx )
{
    const UChar        *sUndoRecBodyPtr;
    sdrLogType          sLogType;
    UShort              sLogSize;
    UChar              *sSlotDirPtr;
    UShort              sUndoRecSize;

    IDE_ASSERT( aUndoRecHdr != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType     = SDR_SDC_UPDATE_ROW_PIECE;
    sSlotDirPtr  = sdpPhyPage::getSlotDirStartPtr( (UChar*)aUndoRecHdr );
    sUndoRecSize = sdcUndoSegment::getUndoRecSize(
                          sSlotDirPtr,
                          SD_MAKE_SLOTNUM( aUndoSID ) );

    /* undo info layer�� ũ�⸦ ����. */
    sLogSize     = sUndoRecSize -
                   SDC_UNDOREC_HDR_SIZE -
                   ID_SIZEOF(scGRID);

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sUndoRecBodyPtr = (const UChar*)
        sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)sUndoRecBodyPtr,
                                  sLogSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  update inplace rowpiece ���꿡 ���� redo or undo�� �����ϴ� �Լ��̴�.
 *  UPDATE_ROW_PIECE �α״� redo, undo �α��� ������ ����.
 *
 **********************************************************************/ 
IDE_RC sdcRow::redo_undo_UPDATE_INPLACE_ROW_PIECE( 
                    sdrMtx              *aMtx, 
                    UChar               *aLogPtr,
                    UChar               *aSlotPtr,
                    sdcOperToMakeRowVer  aOper4RowPiece )
{
    UChar             *sLogPtr         = aLogPtr;
    UChar             *sSlotPtr        = aSlotPtr;
    UShort             sColCount;
    UShort             sUptColCount;
    UShort             sDoUptCount;
    sdcColumnDescSet   sColDescSet;
    UChar              sColDescSetSize;
    UShort             sColSeq;
    UChar              sFlag;
    SShort             sChangeSize;
    UInt               sLength;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );

    SDC_GET_ROWHDR_FIELD(sSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);


    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );
    IDE_ERROR_MSG( (sFlag & SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_MASK)
                   == SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_INPLACE,
                   "sFlag : %"ID_UINT32_FMT,
                   sFlag );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sChangeSize,
                          ID_SIZEOF(sChangeSize) );
    IDE_ERROR_MSG( sChangeSize == 0,
                   "sChangeSize : %"ID_UINT32_FMT,
                   sChangeSize );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sUptColCount,
                          ID_SIZEOF(sUptColCount) );
    IDE_ERROR( sUptColCount > 0 );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sColDescSetSize );
    IDE_ERROR( sColDescSetSize > 0 );

    SDC_CD_ZERO(&sColDescSet);
    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sColDescSet,
                          sColDescSetSize );

    if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
    {
        IDE_TEST( sdcTableCTL::unbind(
                            aMtx,
                            sSlotPtr,          /* Undo�Ǳ��� RowPiecePtr */
                            *(UChar*)sSlotPtr, /* aCTSlotIdxBfrUndo */
                            *(UChar*)sLogPtr,  /* aCTSlotIdxAftUndo */
                            0,                 /* aFSCreditSize */
                            ID_FALSE )         /* aDecDeleteRowCnt */
                  != IDE_SUCCESS );
    }

    ID_WRITE_AND_MOVE_SRC( sSlotPtr,
                           sLogPtr,
                           SDC_ROWHDR_SIZE );

    sSlotPtr    = getDataArea(sSlotPtr);
    sDoUptCount = 0;

    for ( sColSeq = 0; sColSeq < sColCount; sColSeq++ )
    {
        if ( SDC_CD_ISSET( sColSeq, &sColDescSet ) == ID_TRUE )
        {
            /* update column�� ��� */

            sLength = getColPieceLen(sLogPtr);

            ID_WRITE_AND_MOVE_BOTH( sSlotPtr,
                                    sLogPtr,
                                    SDC_GET_COLPIECE_SIZE(sLength) );
            sDoUptCount++;
            if ( sDoUptCount == sUptColCount )
            {
                break;
            }
        }
        else
        {
            /* update column�� �ƴ� ���,
             * next column���� �̵��Ѵ�. */
            sSlotPtr = getNxtColPiece(sSlotPtr);
        }
    }

    if ( sUptColCount != sDoUptCount )
    {
        ideLog::log( IDE_ERR_0,
                     "UptColCount: %"ID_UINT32_FMT", "
                     "DoUptCount: %"ID_UINT32_FMT", "
                     "ColCount: %"ID_UINT32_FMT"\n",
                     sUptColCount,
                     sDoUptCount,
                     sColCount );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sSlotPtr,
                                     "Slot Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        aLogPtr,
                        SD_PAGE_SIZE,
                        "LogPtr Dump:" );

        IDE_ERROR( 0 );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  update outplace rowpiece ���꿡 ���� redo or undo�� �����ϴ� �Լ��̴�.
 *  UPDATE_ROW_PIECE �α״� redo, undo �α��� ������ ����.
 *
 **********************************************************************/
IDE_RC sdcRow::redo_undo_UPDATE_OUTPLACE_ROW_PIECE(
                                sdrMtx              *aMtx,
                                UChar               *aLogPtr,
                                UChar               *aSlotPtr,
                                sdSID                aSlotSID,
                                sdcOperToMakeRowVer  aOper4RowPiece,
                                UChar               *aRowBuf4MVCC,
                                UChar              **aNewSlotPtr4Undo,
                                SShort              *aFSCreditSize )
{
    UChar             *sWritePtr;
    UChar              sOldSlotImage[SD_PAGE_SIZE];
    UChar             *sOldSlotImagePtr;
    UChar             *sOldSlotPtr      = aSlotPtr;
    UChar             *sNewSlotPtr;
    UChar             *sLogPtr          = (UChar*)aLogPtr;
    sdcColumnDescSet   sColDescSet;
    UChar              sColDescSetSize;
    UShort             sOldRowPieceSize;
    UShort             sNewRowPieceSize;
    SShort             sChangeSize;
    UShort             sColCount;
    UShort             sUptColCount;
    UShort             sDoUptCount;
    UShort             sColSeq;
    UChar              sFlag;
    UChar              sRowFlag;
    UInt               sLength;
    idBool             sReserveFreeSpaceCredit;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );

    sOldRowPieceSize = getRowPieceSize(sOldSlotPtr);
    SDC_GET_ROWHDR_FIELD(sOldSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);
    SDC_GET_ROWHDR_1B_FIELD(sOldSlotPtr, SDC_ROWHDR_FLAG, sRowFlag);

    /* reallocSlot()�� �ϴ� ���߿�
     * compact page ������ �߻��� �� �ֱ� ������
     * old slot image�� temp buffer�� �����ؾ� �Ѵ�. */
    sOldSlotImagePtr = sOldSlotImage;
    idlOS::memcpy( sOldSlotImagePtr,
                   sOldSlotPtr,
                   sOldRowPieceSize );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );
    IDE_ERROR_MSG( (sFlag & SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_MASK)
                   == SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_OUTPLACE,
                   "sFlag : %"ID_UINT32_FMT,
                   sFlag );


    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sChangeSize,
                          ID_SIZEOF(sChangeSize) );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sUptColCount,
                          ID_SIZEOF(sUptColCount) );
    IDE_ERROR( sUptColCount > 0 );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sColDescSetSize );
    IDE_ERROR( sColDescSetSize > 0 );

    SDC_CD_ZERO(&sColDescSet);
    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sColDescSet,
                          sColDescSetSize );

    if ( aOper4RowPiece == SDC_MVCC_MAKE_VALROW )
    {
        /* MVCC�� ���� undo�� �����ϴ� ���,
         * reallocSlot()�� �ϸ� �ȵǰ�,
         * ���ڷ� �Ѿ�� temp buffer pointer�� ����ؾ� �Ѵ�. */
        IDE_ERROR( aRowBuf4MVCC != NULL );
        sNewSlotPtr = aRowBuf4MVCC;
    }
    else
    {
        sNewRowPieceSize = sOldRowPieceSize + (sChangeSize);

        if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
        {
            IDE_ERROR( aFSCreditSize != NULL );

            *aFSCreditSize =
                sdcRow::calcFSCreditSize( sNewRowPieceSize,
                                          sOldRowPieceSize );

            IDE_TEST( sdcTableCTL::unbind( aMtx,
                                           sOldSlotPtr, /* Undo�Ǳ��� RowPiecePtr */
                                           *(UChar*)sOldSlotPtr,
                                           *(UChar*)sLogPtr,
                                           *aFSCreditSize,
                                           ID_FALSE ) /* aDecDeleteRowCnt */
                      != IDE_SUCCESS );
        }

        /*
         * ###   FSC �÷���   ###
         *
         * DML �����߿��� �翬�� FSC�� reserve �ؾ� �Ѵ�.
         * �׷��� redo�� undo�ÿ��� ��� �ؾ� �ϳ�?
         *
         * redo�� DML ������ �ٽ� �����ϴ� ���̹Ƿ�,
         * DML �����Ҷ��� �����ϰ� FSC�� reserve �ؾ� �Ѵ�.
         *
         * �ݸ� undo�ÿ��� FSC�� reserve�ϸ� �ȵȴ�.
         * �ֳ��ϸ� FSC�� DML ������ undo��ų���� ����ؼ�
         * ������ �����صδ� ���̹Ƿ�,
         * undo�ÿ��� ������ reserve�ص� FSC��
         * �������� �ǵ���(restore)�־�� �ϰ�,
         * undo�ÿ� �� �ٽ� FSC�� reserve�Ϸ��� �ؼ��� �ȵȴ�.
         *
         * clr�� undo�� ���� redo�̹Ƿ� undo���� �����ϰ�
         * FSC�� reserve�ϸ� �ȵȴ�.
         *
         * �� ������ ��츦 �����Ͽ�
         * FSC reserve ó���� �ؾ� �ϴµ�,
         * ��(upinel9)�� �α׸� ����Ҷ� FSC reserve ���θ� �÷��׷� ���ܼ�,
         * redo�� undo�ÿ��� �� �÷��׸� ����
         * reallocSlot()�� �ϵ��� �����Ͽ���.
         *
         * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
         * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         */
        if ( (sFlag & SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_MASK) ==
             SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE )
        {
            sReserveFreeSpaceCredit = ID_TRUE;
        }
        else
        {
            sReserveFreeSpaceCredit = ID_FALSE;
        }

        IDE_TEST( reallocSlot( aSlotSID,
                               sOldSlotPtr,
                               sNewRowPieceSize,
                               sReserveFreeSpaceCredit,
                               &sNewSlotPtr )
                  != IDE_SUCCESS );
         sOldSlotPtr = NULL;

        *aNewSlotPtr4Undo = sNewSlotPtr;
    }

    IDE_ERROR( sNewSlotPtr != NULL );

    idlOS::memcpy( sNewSlotPtr,
                   sLogPtr,
                   SDC_ROWHDR_SIZE );

    sWritePtr = sNewSlotPtr;

    SDC_MOVE_PTR_TRIPLE( sWritePtr,
                         sLogPtr,
                         sOldSlotImagePtr,
                         SDC_ROWHDR_SIZE );

    if ( SDC_IS_LAST_ROWPIECE(sRowFlag) != ID_TRUE )
    {
        ID_WRITE_AND_MOVE_BOTH( sWritePtr,
                                sOldSlotImagePtr,
                                SDC_EXTRASIZE_FOR_CHAINING );
    }

    sDoUptCount = 0;

    for ( sColSeq = 0; sColSeq < sColCount; sColSeq++ )
    {
        if ( SDC_CD_ISSET( sColSeq, &sColDescSet ) == ID_TRUE )
        {
            /* update column�� ���,
             * log�� column value�� new slot�� ����Ѵ�. */
            sLength = getColPieceLen(sLogPtr);

            ID_WRITE_AND_MOVE_BOTH( sWritePtr,
                                    sLogPtr,
                                    SDC_GET_COLPIECE_SIZE(sLength) );
            sDoUptCount++;
        }
        else
        {
            /* update column�� �ƴ� ���,
             * old slot�� column value�� new slot�� ����Ѵ�. */
            sLength = getColPieceLen(sOldSlotImagePtr);

            ID_WRITE_AND_MOVE_DEST( sWritePtr,
                                    sOldSlotImagePtr,
                                    SDC_GET_COLPIECE_SIZE(sLength) );
        }

        sOldSlotImagePtr = getNxtColPiece(sOldSlotImagePtr);
    }

    if ( sUptColCount != sDoUptCount )
    {
        ideLog::log( IDE_ERR_0,
                     "UptColCount: %"ID_UINT32_FMT", "
                     "DoUptCount: %"ID_UINT32_FMT", "
                     "ColCount: %"ID_UINT32_FMT"\n",
                     sUptColCount,
                     sDoUptCount,
                     sColCount );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Slot Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        aLogPtr,
                        SD_PAGE_SIZE,
                        "LogPtr Dump:" );

        IDE_ERROR( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *  ###   overwrite rowpiece ������ �߻��ϴ� ���   ###
 *   1. overflow �߻�
 *   2. trailing null update
 *
 *
 *  ###   overwrite rowpiece ����  VS  update rowpiece ����   ###
 *
 *  overwrite rowpiece ����
 *   . rowpiece�� ������ ���� �� �ִ�.(column count, row flag)
 *   . logging�� rowpiece ��ü�� �α��Ѵ�.
 *
 *  update rowpiece ����
 *   . rowpiece�� ������ ������ �ʴ´�.(column count, row flag)
 *   . logging�� update�� column ������ �α��Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::overwriteRowPiece(
    idvSQL                         *aStatistics,
    void                           *aTableHeader,
    sdrMtx                         *aMtx,
    sdrMtxStartInfo                *aStartInfo,
    UChar                          *aOldSlotPtr,
    scGRID                          aSlotGRID,
    const sdcRowPieceOverwriteInfo *aOverwriteInfo,
    sdSID                           aNextRowPieceSID,
    idBool                          aReplicate )
{
    sdcRowHdrInfo      * sNewRowHdrInfo = aOverwriteInfo->mNewRowHdrInfo;
    UChar              * sNewSlotPtr;
    sdpPageListEntry   * sEntry;
    scSpaceID            sTableSpaceID;
    smOID                sTableOID;
    sdSID                sUndoSID      = SD_NULL_SID;
    SShort               sFSCreditSize = 0;
    sdcUndoSegment     * sUDSegPtr;

    IDE_ASSERT( aTableHeader   != NULL );
    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aStartInfo     != NULL );
    IDE_ASSERT( aOldSlotPtr    != NULL );
    IDE_ASSERT( aOverwriteInfo != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sEntry =
        (sdpPageListEntry*)(smcTable::getDiskPageListEntry(aTableHeader));
    IDE_ASSERT( sEntry != NULL );

    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );

    IDE_ERROR( getSlotPtr( aMtx, aSlotGRID, &sNewSlotPtr ) == IDE_SUCCESS );
    IDE_ERROR( sNewSlotPtr  == aOldSlotPtr );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aMtx->mTrans );
    if ( sUDSegPtr->addOverwriteRowPieceUndoRec( aStatistics,
                                                 aStartInfo,
                                                 sTableOID,
                                                 aOldSlotPtr,
                                                 aSlotGRID,
                                                 aOverwriteInfo,
                                                 aReplicate,
                                                 &sUndoSID ) != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� ��  */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }

    IDE_TEST( reallocSlot( aOverwriteInfo->mRowPieceSID,
                           aOldSlotPtr,
                           aOverwriteInfo->mNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewSlotPtr )
              != IDE_SUCCESS );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*)
                                         sdpPhyPage::getHdr( sNewSlotPtr ) )
                == sTableOID );


    aOldSlotPtr = NULL;
    IDE_ASSERT( sNewSlotPtr != NULL );

    sFSCreditSize = calcFSCreditSize( aOverwriteInfo->mOldRowPieceSize,
                                      aOverwriteInfo->mNewRowPieceSize );

    if ( sFSCreditSize != 0 )
    {
        // reallocSlot�� �����Ŀ�, Segment�� ���� ���뵵
        // ���濬���� �����Ѵ�.
        IDE_TEST( sdpPageList::updatePageState( aStatistics,
                                                sTableSpaceID,
                                                sEntry,
                                                sdpPhyPage::getPageStartPtr(sNewSlotPtr),
                                                aMtx )
                  != IDE_SUCCESS );
    }

    sNewRowHdrInfo->mUndoSID = sUndoSID;
    writeRowPiece4ORP( sNewSlotPtr,
                       sNewRowHdrInfo,
                       aOverwriteInfo,
                       aNextRowPieceSID );

    IDE_DASSERT_MSG( aOverwriteInfo->mNewRowPieceSize ==
                     getRowPieceSize( sNewSlotPtr ),
                     "Invalid Row Piece Size - "
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     sTableOID,
                     aOverwriteInfo->mNewRowPieceSize,
                     getRowPieceSize( sNewSlotPtr ) );

    sdrMiniTrans::setRefOffset(aMtx, sTableOID);

    IDE_TEST( writeOverwriteRowPieceRedoUndoLog( sNewSlotPtr,
                                                 aSlotGRID,
                                                 aOverwriteInfo,
                                                 aReplicate,
                                                 aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdcTableCTL::bind(
                  aMtx,
                  sTableSpaceID,
                  sNewSlotPtr,
                  aOverwriteInfo->mOldRowHdrInfo->mCTSlotIdx,
                  aOverwriteInfo->mNewRowHdrInfo->mCTSlotIdx,
                  SC_MAKE_SLOTNUM(aSlotGRID),
                  aOverwriteInfo->mOldRowHdrInfo->mExInfo.mFSCredit,
                  sFSCreditSize,
                  ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDC_UNDO_OVERWRITE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *                    next slotnum(2)] [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
UChar* sdcRow::writeOverwriteRowPieceUndoRecRedoLog( UChar                          *aWritePtr,
                                                     const UChar                    *aOldSlotPtr,
                                                     const sdcRowPieceOverwriteInfo *aOverwriteInfo )
{
    UChar     *sWritePtr = aWritePtr;
    UShort     sRowPieceSize;
    UChar      sFlag;
    SShort     sChangeSize;

    IDE_ASSERT( aWritePtr      != NULL );
    IDE_ASSERT( aOldSlotPtr    != NULL );
    IDE_ASSERT( aOverwriteInfo != NULL );

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE;
    ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sFlag );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = aOverwriteInfo->mOldRowPieceSize -
                  aOverwriteInfo->mNewRowPieceSize;
    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            &sChangeSize,
                            ID_SIZEOF(sChangeSize) );

    IDE_DASSERT_MSG( aOverwriteInfo->mOldRowPieceSize ==
                     getRowPieceSize( aOldSlotPtr ),
                     "Invalid Row Piece Size"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     aOverwriteInfo->mNewRowPieceSize,
                     getRowPieceSize( aOldSlotPtr ) );

    sRowPieceSize = aOverwriteInfo->mOldRowPieceSize;

    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            aOldSlotPtr,
                            sRowPieceSize );

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_OVERWRITE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4),
 *   |                next slotnum(2)], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeOverwriteRowPieceRedoUndoLog(
                       UChar                          *aSlotPtr,
                       scGRID                          aSlotGRID,
                       const sdcRowPieceOverwriteInfo *aOverwriteInfo,
                       idBool                          aReplicate,
                       sdrMtx                         *aMtx )
{
    sdrLogType       sLogType;
    UShort           sLogSize;
    UShort           sRowPieceSize;
    UChar            sFlag;
    SShort           sChangeSize;

    IDE_ASSERT( aSlotPtr       != NULL );
    IDE_ASSERT( aOverwriteInfo != NULL );
    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_OVERWRITE_ROW_PIECE;

    IDE_DASSERT_MSG( aOverwriteInfo->mNewRowPieceSize ==
                     getRowPieceSize( aSlotPtr ),
                     "Invalid Row Piece Size"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     aOverwriteInfo->mNewRowPieceSize,
                     getRowPieceSize( aSlotPtr ) );

    sRowPieceSize = aOverwriteInfo->mNewRowPieceSize;

    sLogSize = calcOverwriteRowPieceLogSize( aOverwriteInfo,
                                             ID_FALSE,      /* aIsUndoRec */
                                             aReplicate );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE;
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sFlag,
                                  ID_SIZEOF(sFlag))
              != IDE_SUCCESS );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = aOverwriteInfo->mNewRowPieceSize -
                  aOverwriteInfo->mOldRowPieceSize;

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sChangeSize,
                                  ID_SIZEOF(UShort))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  sRowPieceSize )
              != IDE_SUCCESS );

    if ( aReplicate == ID_TRUE )
    {
        IDE_TEST( writeOverwriteRowPieceLog4RP( aOverwriteInfo,
                                                ID_FALSE,
                                                aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  overwrite rowpiece undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *  OVERWRITE_ROW_PIECE �α״� redo, undo �α��� ������ ���� ������
 *  undo record���� undo info layer�� ���븸 ����,
 *  clr redo log�� �ۼ��ϸ� �ȴ�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeOverwriteRowPieceCLR( const UChar    * aUndoRecHdr,
                                          scGRID           aSlotGRID,
                                          sdSID            aUndoSID,
                                          sdrMtx         * aMtx )
{
    const UChar        *sUndoRecBodyPtr;
    UChar              *sSlotDirStartPtr;
    sdrLogType          sLogType;
    UShort              sLogSize;

    IDE_ASSERT( aUndoRecHdr != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_OVERWRITE_ROW_PIECE;

    /* undo info layer�� ũ�⸦ ����. */
    sSlotDirStartPtr = sdpPhyPage::getSlotDirStartPtr(
                                      (UChar*)aUndoRecHdr );

    sLogSize = sdcUndoSegment::getUndoRecSize(
                               sSlotDirStartPtr,
                               SD_MAKE_SLOTNUM(aUndoSID) );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sUndoRecBodyPtr = (const UChar*)
        sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)sUndoRecBodyPtr,
                                  sLogSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  overwrite rowpiece ���꿡 ���� redo or undo�� �����ϴ� �Լ��̴�.
 *  OVERWRITE_ROW_PIECE �α״� redo, undo �α��� ������ ����.
 *
 **********************************************************************/
IDE_RC sdcRow::redo_undo_OVERWRITE_ROW_PIECE(
                       sdrMtx              *aMtx,
                       UChar               *aLogPtr,
                       UChar               *aSlotPtr,
                       sdSID                aSlotSID,
                       sdcOperToMakeRowVer  aOper4RowPiece,
                       UChar               *aRowBuf4MVCC,
                       UChar              **aNewSlotPtr4Undo,
                       SShort              *aFSCreditSize )
{
    UChar             *sOldSlotPtr      = aSlotPtr;
    UChar             *sNewSlotPtr;
    UChar             *sLogPtr          = aLogPtr;
    UShort             sOldRowPieceSize;
    UShort             sNewRowPieceSize;
    UChar              sFlag;
    SShort             sChangeSize;
    idBool             sReserveFreeSpaceCredit;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sChangeSize,
                          ID_SIZEOF(sChangeSize) );

    sOldRowPieceSize = getRowPieceSize(sOldSlotPtr);
    sNewRowPieceSize = getRowPieceSize(sLogPtr);

    if ( (sOldRowPieceSize + (sChangeSize)) != sNewRowPieceSize )
    {
        ideLog::log( IDE_ERR_0,
                     "SlotSID: %"ID_UINT64_FMT", "
                     "OldRowPieceSize: %"ID_UINT32_FMT", "
                     "NewRowPieceSize: %"ID_UINT32_FMT", "
                     "ChangeSize: %"ID_INT32_FMT"\n",
                     aSlotSID,
                     sOldRowPieceSize,
                     sNewRowPieceSize,
                     sChangeSize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        aLogPtr,
                        SD_PAGE_SIZE,
                        "LogPtr Dump:" );

        IDE_ERROR( 0 );
    }


    if ( aOper4RowPiece == SDC_MVCC_MAKE_VALROW )
    {
        /* MVCC�� ���� undo�� �����ϴ� ���,
         * reallocSlot()�� �ϸ� �ȵǰ�,
         * ���ڷ� �Ѿ�� temp buffer pointer�� ����ؾ� �Ѵ�. */
        IDE_ERROR( aRowBuf4MVCC != NULL );
        sNewSlotPtr = aRowBuf4MVCC;
    }
    else
    {
        if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
        {
            IDE_ERROR( aFSCreditSize != NULL );

            *aFSCreditSize = sdcRow::calcFSCreditSize(
                                        sNewRowPieceSize,
                                        sOldRowPieceSize );

            IDE_TEST( sdcTableCTL::unbind(
                          aMtx,
                          sOldSlotPtr,         /* Undo�Ǳ��� RowPiecePtr */
                          *(UChar*)sOldSlotPtr,/* aCTSlotIdxBfrUndo */
                          *(UChar*)sLogPtr,    /* aCTSlotIdxAftUndo */
                          *aFSCreditSize,
                          ID_FALSE )           /* aDecDeleteRowCnt */
                      != IDE_SUCCESS );
        }

        /*
         * ###   FSC �÷���   ###
         *
         * DML �����߿��� �翬�� FSC�� reserve �ؾ� �Ѵ�.
         * �׷��� redo�� undo�ÿ��� ��� �ؾ� �ϳ�?
         *
         * redo�� DML ������ �ٽ� �����ϴ� ���̹Ƿ�,
         * DML �����Ҷ��� �����ϰ� FSC�� reserve �ؾ� �Ѵ�.
         *
         * �ݸ� undo�ÿ��� FSC�� reserve�ϸ� �ȵȴ�.
         * �ֳ��ϸ� FSC�� DML ������ undo��ų���� ����ؼ�
         * ������ �����صδ� ���̹Ƿ�,
         * undo�ÿ��� ������ reserve�ص� FSC��
         * �������� �ǵ���(restore)�־�� �ϰ�,
         * undo�ÿ� �� �ٽ� FSC�� reserve�Ϸ��� �ؼ��� �ȵȴ�.
         *
         * clr�� undo�� ���� redo�̹Ƿ� undo���� �����ϰ�
         * FSC�� reserve�ϸ� �ȵȴ�.
         *
         * �� ������ ��츦 �����Ͽ�
         * FSC reserve ó���� �ؾ� �ϴµ�,
         * ��(upinel9)�� �α׸� ����Ҷ� FSC reserve ���θ� �÷��׷� ���ܼ�,
         * redo�� undo�ÿ��� �� �÷��׸� ����
         * reallocSlot()�� �ϵ��� �����Ͽ���.
         *
         * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
         * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         */
        if ( ( sFlag & SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_MASK ) ==
             SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE )
        {
            sReserveFreeSpaceCredit = ID_TRUE;
        }
        else
        {
            sReserveFreeSpaceCredit = ID_FALSE;
        }

        IDE_TEST( reallocSlot( aSlotSID,
                               sOldSlotPtr,
                               sNewRowPieceSize,
                               sReserveFreeSpaceCredit,
                               &sNewSlotPtr )
                  != IDE_SUCCESS );
        sOldSlotPtr = NULL;

        *aNewSlotPtr4Undo = sNewSlotPtr;
    }
    IDE_ERROR( sNewSlotPtr != NULL );

    idlOS::memcpy( sNewSlotPtr,
                   sLogPtr,
                   sNewRowPieceSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::processOverflowData( idvSQL                      *aStatistics,
                                    void                        *aTrans,
                                    void                        *aTableInfoPtr,
                                    void                        *aTableHeader,
                                    sdrMtx                      *aMtx,
                                    sdrMtxStartInfo             *aStartInfo,
                                    UChar                       *aSlotPtr,
                                    scGRID                       aSlotGRID,
                                    const sdcRowPieceUpdateInfo *aUpdateInfo,
                                    UShort                       aFstColumnSeq,
                                    idBool                       aReplicate,
                                    sdSID                        aNextRowPieceSID,
                                    sdcRowPieceOverwriteInfo    *aOverwriteInfo,
                                    sdcSupplementJobInfo        *aSupplementJobInfo,
                                    sdSID                       *aLstInsertRowPieceSID )
{
    sdcRowPieceInsertInfo  sInsertInfo;
    sdcRowHdrInfo         *sNewRowHdrInfo;
    UInt                   sColCount;
    UShort                 sCurrColSeq;
    UInt                   sCurrOffset;
    UChar                  sRowFlag;
    smOID                  sTableOID;
    sdSID                  sInsertRowPieceSID    = SD_NULL_SID;
    sdSID                  sLstInsertRowPieceSID = SD_NULL_SID;
    sdSID                  sNextRowPieceSID      = aNextRowPieceSID;
    smSCN                  sInfiniteSCN;
    UChar                  sNextRowFlag          = SDC_ROWHDR_FLAG_ALL; 

    IDE_ASSERT( aTrans                != NULL );
    IDE_ASSERT( aTableInfoPtr         != NULL );
    IDE_ASSERT( aTableHeader          != NULL );
    IDE_ASSERT( aMtx                  != NULL );
    IDE_ASSERT( aStartInfo            != NULL );
    IDE_ASSERT( aSlotPtr              != NULL );
    IDE_ASSERT( aUpdateInfo           != NULL );
    IDE_ASSERT( aOverwriteInfo        != NULL );
    IDE_ASSERT( aSupplementJobInfo    != NULL );
    IDE_ASSERT( aLstInsertRowPieceSID != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sTableOID      = smLayerCallback::getTableOID( aTableHeader );
    sNewRowHdrInfo = aUpdateInfo->mNewRowHdrInfo;
    sColCount      = sNewRowHdrInfo->mColCount;
    sInfiniteSCN   = sNewRowHdrInfo->mInfiniteSCN;

    makeInsertInfoFromUptInfo( aTableHeader,
                               aUpdateInfo,
                               sColCount,
                               aFstColumnSeq,
                               &sInsertInfo );
    
    IDE_DASSERT( validateInsertInfo( &sInsertInfo,
                                     sColCount,
                                     aFstColumnSeq )
                 == ID_TRUE );

    /* column data���� �����Ҷ��� �ڿ������� ä���� �����Ѵ�.
     *
     * ����
     * 1. rowpiece write�Ҷ�,
     *    next rowpiece link ������ �˾ƾ� �ϹǷ�,
     *    ���� column data���� ���� �����ؾ� �Ѵ�.
     *
     * 2. ���ʺ��� �˲� ä���� �����ϸ�
     *    first page�� ���������� ������ ���ɼ��� ����,
     *    �׷��� update�ÿ� row migration �߻����ɼ��� ��������.
     * */
    sCurrColSeq = sColCount - 1;
    sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                        sCurrColSeq );
    while(1)
    {
        IDE_TEST( analyzeRowForInsert(
                      (sdpPageListEntry*)
                      smcTable::getDiskPageListEntry(aTableHeader),
                      sCurrColSeq,
                      sCurrOffset,
                      sNextRowPieceSID,
                      sTableOID,
                      &sInsertInfo ) != IDE_SUCCESS );

        if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(&sInsertInfo)
             == ID_TRUE )
        {
            /* overflow data�߿��� first piece�� ���� rowpiece��
             * ����Ǿ� �ִ� ���������� ó���ؾ� �ϹǷ� break �Ѵ�. */
            break;
        }

        sRowFlag = calcRowFlag4Update( sNewRowHdrInfo->mRowFlag,
                                       &sInsertInfo,
                                       sNextRowPieceSID );

        /* BUG-32278 The previous flag is set incorrectly in row flag when
         * a rowpiece is overflowed. */
        IDE_TEST_RAISE( validateRowFlag(sRowFlag, sNextRowFlag) != ID_TRUE,
                        error_invalid_rowflag );

        if ( insertRowPiece( aStatistics,
                             aTrans,
                             aTableInfoPtr,
                             aTableHeader,
                             &sInsertInfo,
                             sRowFlag,
                             sNextRowPieceSID,
                             &sInfiniteSCN,
                             ID_TRUE, /* aIsNeedUndoRec */
                             ID_TRUE,
                             aReplicate,
                             &sInsertRowPieceSID )
             != IDE_SUCCESS )
        {
            /* InsertRowpiece ���� �����ϸ�, Undo, �Ǵ� DataPage ��������.
             * �ȿ��� ���� ����ó�� ���ֱ� ������, ���⼭�� MtxUndo��
             * ó������ */
            sdrMiniTrans::setIgnoreMtxUndo( aMtx );
            IDE_TEST( 1 );
        }

        /* 
         * BUG-37529 [sm-disk-collection] [DRDB] The change row piece logic
         * generates invalid undo record.
         * update���� rowPiece�� �����ϴ� page�� overflowData�� insert�ϴ°���
         * page self aging�� �߻��ߴٴ� �ǹ��̴�.
         * page self aging���� compactPage�� ����Ǹ� update���� rowPiece�� ��ġ��
         * �Ȱ�����. �̷����� ���ڷ� ���޹��� aSlotPtr�� ���̻� ��ȿ�� slot��
         * ��ġ�� ����Ű�� �������� GSID�� �̿��� slot�� ��ġ�� �ٽñ��Ѵ�.
         */
        if ( SD_MAKE_PID(sInsertRowPieceSID) == SC_MAKE_PID( aSlotGRID ) )
        {
            IDE_ERROR( getSlotPtr( aMtx, aSlotGRID, &aSlotPtr ) == IDE_SUCCESS );
        }

        if ( sInsertInfo.mStartColOffset == 0 )
        {
            /* BUG-27199 Update Row��, calcRowFlag4Update �Լ����� �߸��� Į����
             * �������� Chaining ���θ� �Ǵ��ϰ� �ֽ��ϴ�.
             * Į������ insertRowPiece�� ���� ���ʺ��� ���ʴ�� ��ϵʿ� ����,
             * ���� Į���� ������ Į���� Chaining�ž������� �Ǵ��ϴ� NFlag��
             * ����Ǿ�� �մϴ�.*/
            SM_SET_FLAG_OFF( sNewRowHdrInfo->mRowFlag, SDC_ROWHDR_N_FLAG ); 
            sCurrColSeq = sInsertInfo.mStartColSeq - 1;
            sCurrOffset = getColDataSize2Store( &sInsertInfo,
                                                sCurrColSeq );
        }
        else
        {
            /* BUG-27199 Update Row��, calcRowFlag4Update �Լ����� �߸��� Į����
             * �������� Chaining ���θ� �Ǵ��ϰ� �ֽ��ϴ�.  */
            SM_SET_FLAG_ON( sNewRowHdrInfo->mRowFlag, SDC_ROWHDR_N_FLAG ); 
            sCurrColSeq = sInsertInfo.mStartColSeq;
            sCurrOffset = sInsertInfo.mStartColOffset;
        }

        if ( sLstInsertRowPieceSID == SD_NULL_SID )
        {
            sLstInsertRowPieceSID = sInsertRowPieceSID;
        }
        
        sNextRowPieceSID = sInsertRowPieceSID;
        sNextRowFlag     = sRowFlag;
    }

    /* overflow data�� �޺κ��� �� �������� �����ϴٰ�,
     * ���� �����͸� ���� ���������� ó���Ѵ�. */

    IDE_TEST( processRemainData( aStatistics,
                                 aTrans,
                                 aTableInfoPtr,
                                 aTableHeader,
                                 aMtx,
                                 aStartInfo,
                                 aSlotPtr,
                                 aSlotGRID,
                                 &sInsertInfo,
                                 aUpdateInfo,
                                 aReplicate,
                                 sNextRowPieceSID,
                                 sNextRowFlag,
                                 aOverwriteInfo,
                                 aSupplementJobInfo,
                                 &sInsertRowPieceSID )
              != IDE_SUCCESS );

    if ( sLstInsertRowPieceSID == SD_NULL_SID )
    {
        sLstInsertRowPieceSID = sInsertRowPieceSID;
    }

    *aLstInsertRowPieceSID = sLstInsertRowPieceSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_rowflag )
    {
        ideLog::log( IDE_ERR_0,
                     "CurrColSeq:           %"ID_UINT32_FMT"\n"
                     "CurrOffset:           %"ID_UINT32_FMT"\n"
                     "TableColCount:        %"ID_UINT32_FMT"\n"
                     "FstColumnSeq:         %"ID_UINT32_FMT"\n"
                     "RowFlag:              %"ID_UINT32_FMT"\n"
                     "NextRowFlag:          %"ID_UINT32_FMT"\n"
                     "sNextRowPieceSID:     %"ID_vULONG_FMT"\n"
                     "aNextRowPieceSID:     %"ID_vULONG_FMT"\n"
                     "LstInsertRowPieceSID: %"ID_vULONG_FMT"\n",
                     sCurrColSeq,
                     sCurrOffset,
                     smLayerCallback::getColumnCount( aTableHeader ),
                     aFstColumnSeq,
                     sRowFlag,
                     sNextRowFlag,
                     sNextRowPieceSID,
                     aNextRowPieceSID,
                     sLstInsertRowPieceSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ Old Row Hdr Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo) ,
                        "[ New Row Hdr Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "[ SlotGRID ]\n" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)&sInsertInfo,
                        ID_SIZEOF(sdcRowPieceInsertInfo),
                        "[ Insert Info ]"  );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "[ Update Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aOverwriteInfo,
                        ID_SIZEOF(sdcRowPieceOverwriteInfo),
                        "[ Overwrite Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aSupplementJobInfo,
                        ID_SIZEOF(sdcSupplementJobInfo),
                        "[ Supplement Job Info ]");

        ideLog::logMem( IDE_DUMP_0,
                        aSlotPtr,
                        SD_PAGE_SIZE,
                        "[ Dump Row Data ]\n");

        ideLog::logMem( IDE_DUMP_0,
                        sdpPhyPage::getPageStartPtr(aSlotPtr),
                        SD_PAGE_SIZE,
                        "[ Dump Page ]\n" );

        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                 "invalid row flag") );
        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  split �Ŀ� ���� �ִ� �����͸� ó���Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::processRemainData( idvSQL                      *aStatistics,
                                  void                        *aTrans,
                                  void                        *aTableInfoPtr,
                                  void                        *aTableHeader,
                                  sdrMtx                      *aMtx,
                                  sdrMtxStartInfo             *aStartInfo,
                                  UChar                       *aSlotPtr,
                                  scGRID                       aSlotGRID,
                                  const sdcRowPieceInsertInfo *aInsertInfo,
                                  const sdcRowPieceUpdateInfo *aUpdateInfo,
                                  idBool                       aReplicate,
                                  sdSID                        aNextRowPieceSID,
                                  UChar                        aNextRowFlag, 
                                  sdcRowPieceOverwriteInfo    *aOverwriteInfo,
                                  sdcSupplementJobInfo        *aSupplementJobInfo,
                                  sdSID                       *aInsertRowPieceSID )
{
    UChar sRowFlag = aUpdateInfo->mOldRowHdrInfo->mRowFlag;

    IDE_ASSERT( aTrans               != NULL );
    IDE_ASSERT( aTableInfoPtr        != NULL );
    IDE_ASSERT( aTableHeader         != NULL );
    IDE_ASSERT( aMtx                 != NULL );
    IDE_ASSERT( aStartInfo           != NULL );
    IDE_ASSERT( aSlotPtr             != NULL );
    IDE_ASSERT( aInsertInfo          != NULL );
    IDE_ASSERT( aUpdateInfo          != NULL );
    IDE_ASSERT( aOverwriteInfo       != NULL );
    IDE_ASSERT( aSupplementJobInfo   != NULL );
    IDE_ASSERT( aInsertRowPieceSID   != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( canReallocSlot( aSlotPtr, aInsertInfo->mRowPieceSize ) == ID_TRUE )
    {
        /* remain data�� ���� ���������� ó���� �� ������
         * overwrite rowpiece ������ �����Ѵ�. */
        makeOverwriteInfo( aInsertInfo,
                           aUpdateInfo,
                           aNextRowPieceSID,
                           aOverwriteInfo );

        IDE_TEST( overwriteRowPiece( aStatistics,
                                     aTableHeader,
                                     aMtx,
                                     aStartInfo,
                                     aSlotPtr,
                                     aSlotGRID,
                                     aOverwriteInfo,
                                     aNextRowPieceSID,
                                     aReplicate )
                  != IDE_SUCCESS );

        /*
         * SQL�� Full Update�� �߻��ϴ� ��� ���� LOB�� ������ ���־�� �Ѵ�.
         * API�� ȣ��� ���� API�� ���ֹǷ� ���� ���ִ� �۾��� �ʿ� ����.
         */
        if ( (aOverwriteInfo->mUptAftLobDescCnt > 0) &&
             (aOverwriteInfo->mIsUptLobByAPI != ID_TRUE) )
        {
            SDC_ADD_SUPPLEMENT_JOB( aSupplementJobInfo,
                                    SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_OVERWRITE );
        }
    }
    else
    {
        /* remain data�� ���� ���������� ó���� �� ���� ��� */

        if ( SDC_IS_HEAD_ROWPIECE(sRowFlag) == ID_TRUE )
        {
            /* head rowpiece�� ���,
             * migrate rowpiece data ������ �����Ѵ�. */
            IDE_TEST( migrateRowPieceData( aStatistics,
                                           aTrans,
                                           aTableInfoPtr,
                                           aTableHeader,
                                           aMtx,
                                           aStartInfo,
                                           aSlotPtr,
                                           aSlotGRID,
                                           aInsertInfo,
                                           aUpdateInfo,
                                           aReplicate,
                                           aNextRowPieceSID,
                                           aNextRowFlag,
                                           aOverwriteInfo,
                                           aInsertRowPieceSID )
                      != IDE_SUCCESS );
        }
        else
        {
            /* head rowpiece�� �ƴϸ�,
             * change rowpiece ������ �����Ѵ�. */
            IDE_TEST( changeRowPiece( aStatistics,
                                      aTrans,
                                      aTableInfoPtr,
                                      aTableHeader,
                                      aMtx,
                                      aStartInfo,
                                      aSlotPtr,
                                      aSlotGRID,
                                      aUpdateInfo,
                                      aInsertInfo,
                                      aReplicate,
                                      aNextRowPieceSID,
                                      aNextRowFlag,
                                      aSupplementJobInfo,
                                      aInsertRowPieceSID )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  change rowpiece ���� = insert rowpiece ���� + remove rowpiece ����
 *
 **********************************************************************/
IDE_RC sdcRow::changeRowPiece( idvSQL                      *aStatistics,
                               void                        *aTrans,
                               void                        *aTableInfoPtr,
                               void                        *aTableHeader,
                               sdrMtx                      *aMtx,
                               sdrMtxStartInfo             *aStartInfo,
                               UChar                       *aSlotPtr,
                               scGRID                       aSlotGRID,
                               const sdcRowPieceUpdateInfo *aUpdateInfo,
                               const sdcRowPieceInsertInfo *aInsertInfo,
                               idBool                       aReplicate,
                               sdSID                        aNextRowPieceSID,
                               UChar                        aNextRowFlag, 
                               sdcSupplementJobInfo        *aSupplementJobInfo,
                               sdSID                       *aInsertRowPieceSID )
{
    sdcRowHdrInfo    *sNewRowHdrInfo     = aUpdateInfo->mNewRowHdrInfo;
    UChar             sInheritRowFlag    = sNewRowHdrInfo->mRowFlag;
    UChar             sRowFlag;
    sdSID             sInsertRowPieceSID = SD_NULL_SID;

    IDE_ASSERT( aTrans              != NULL );
    IDE_ASSERT( aTableInfoPtr       != NULL );
    IDE_ASSERT( aTableHeader        != NULL );
    IDE_ASSERT( aMtx                != NULL );
    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aSlotPtr            != NULL );
    IDE_ASSERT( aUpdateInfo         != NULL );
    IDE_ASSERT( aInsertInfo         != NULL );
    IDE_ASSERT( aSupplementJobInfo  != NULL );
    IDE_ASSERT( aInsertRowPieceSID  != NULL );

    sRowFlag = calcRowFlag4Update( sInheritRowFlag,
                                   aInsertInfo,
                                   aNextRowPieceSID );

    /* FIT/ART/sm/Bugs/BUG-37529/BUG-37529.tc */
    IDU_FIT_POINT( "1.BUG-37529@sdcRow::changeRowPiece" );

    /* BUG-32278 The previous flag is set incorrectly in row flag when
     * a rowpiece is overflowed. */
    IDE_TEST_RAISE( validateRowFlag(sRowFlag, aNextRowFlag) != ID_TRUE,
                    error_invalid_rowflag );

    IDE_TEST( insertRowPiece( aStatistics,
                              aTrans,
                              aTableInfoPtr,
                              aTableHeader,
                              aInsertInfo,
                              sRowFlag,
                              aNextRowPieceSID,
                              &sNewRowHdrInfo->mInfiniteSCN,
                              ID_TRUE, /* aIsNeedUndoRec */
                              ID_TRUE,
                              aReplicate,
                              &sInsertRowPieceSID )
              != IDE_SUCCESS );

    /* 
     * BUG-37529 [sm-disk-collection] [DRDB] The change row piece logic
     * generates invalid undo record.
     * update���� rowPiece�� �����ϴ� page�� overflowData�� insert�ϴ°���
     * page self aging�� �߻��ߴٴ� �ǹ��̴�.
     * page self aging���� compactPage�� ����Ǹ� update���� rowPiece�� ��ġ��
     * �Ȱ�����. �̷����� ���ڷ� ���޹��� aSlotPtr�� ���̻� ��ȿ�� slot��
     * ��ġ�� ����Ű�� �������� GSID�� �̿��� slot�� ��ġ�� �ٽñ��Ѵ�.
     */
    if ( SD_MAKE_PID(sInsertRowPieceSID) == SC_MAKE_PID( aSlotGRID ) )
    {
        IDE_ERROR( getSlotPtr( aMtx, aSlotGRID, &aSlotPtr ) == IDE_SUCCESS );
    }

    IDE_TEST( removeRowPiece4Upt(
                  aStatistics,
                  aTableHeader,
                  aMtx,
                  aStartInfo,
                  aSlotPtr,
                  aUpdateInfo,
                  aReplicate ) != IDE_SUCCESS );

    /* rowpiece�� remove�Ǿ
     * previous rowpiece�� link������ ������ �־�� �ϹǷ�
     * supplement job�� ����Ѵ�. */
    SDC_ADD_SUPPLEMENT_JOB( aSupplementJobInfo,
                            SDC_SUPPLEMENT_JOB_CHANGE_ROWPIECE_LINK );

    aSupplementJobInfo->mNextRowPieceSID = sInsertRowPieceSID;

    *aInsertRowPieceSID = sInsertRowPieceSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_rowflag )
    {
        ideLog::log( IDE_ERR_0,
                     "TableColCount:        %"ID_UINT32_FMT"\n"
                     "InheritFlag:          %"ID_UINT32_FMT"\n"
                     "RowFlag:              %"ID_UINT32_FMT"\n"
                     "NextRowFlag:          %"ID_UINT32_FMT"\n"
                     "NextRowPieceSID:      %"ID_vULONG_FMT"\n",
                     smLayerCallback::getColumnCount( aTableHeader ),
                     sInheritRowFlag,
                     sRowFlag,
                     aNextRowFlag,
                     aNextRowPieceSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)sNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ New Row Hdr Info ]"  );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aInsertInfo,
                        ID_SIZEOF(sdcRowPieceInsertInfo),
                        "[ Insert Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "[ Update Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aSupplementJobInfo,
                        ID_SIZEOF(sdcSupplementJobInfo),
                        "[ Supplement Job Info ]" );

        ideLog::logMem( IDE_DUMP_0, aSlotPtr, SD_PAGE_SIZE ,
                        "[ Dump Row Data ]\n");

        ideLog::logMem( IDE_DUMP_0,
                        sdpPhyPage::getPageStartPtr(aSlotPtr),
                        SD_PAGE_SIZE, "[ Dump Page ]\n"  );
        
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                 "invalid row flag") );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  migrate rowpiece data ���� = insert rowpiece ���� + overwrite rowpiece ����
 *
 **********************************************************************/
IDE_RC sdcRow::migrateRowPieceData( idvSQL                      *aStatistics,
                                    void                        *aTrans,
                                    void                        *aTableInfoPtr,
                                    void                        *aTableHeader,
                                    sdrMtx                      *aMtx,
                                    sdrMtxStartInfo             *aStartInfo,
                                    UChar                       *aSlotPtr,
                                    scGRID                       aSlotGRID,
                                    const sdcRowPieceInsertInfo *aInsertInfo,
                                    const sdcRowPieceUpdateInfo *aUpdateInfo,
                                    idBool                       aReplicate,
                                    sdSID                        aNextRowPieceSID,
                                    UChar                        aNextRowFlag,
                                    sdcRowPieceOverwriteInfo    *aOverwriteInfo,
                                    sdSID                       *aInsertRowPieceSID )
{
    UChar    sInheritRowFlag    = aUpdateInfo->mNewRowHdrInfo->mRowFlag;
    UChar    sRowFlag;
    sdSID    sInsertRowPieceSID = SD_NULL_SID;

    IDE_ASSERT( aTrans              != NULL );
    IDE_ASSERT( aTableInfoPtr       != NULL );
    IDE_ASSERT( aTableHeader        != NULL );
    IDE_ASSERT( aMtx                != NULL );
    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aSlotPtr            != NULL );
    IDE_ASSERT( aInsertInfo         != NULL );
    IDE_ASSERT( aUpdateInfo         != NULL );
    IDE_ASSERT( aOverwriteInfo      != NULL );
    IDE_ASSERT( aInsertRowPieceSID  != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    SM_SET_FLAG_OFF(sInheritRowFlag, SDC_ROWHDR_H_FLAG);

    sRowFlag = calcRowFlag4Update( sInheritRowFlag,
                                   aInsertInfo,
                                   aNextRowPieceSID );

    /* FIT/ART/sm/Bugs/BUG-33008/BUG-33008.tc */
    IDU_FIT_POINT( "1.BUG-33008@sdcRow::migrateRowPieceData" );

    /* BUG-32278 The previous flag is set incorrectly in row flag when
     * a rowpiece is overflowed. */
    IDE_TEST_RAISE( validateRowFlag(sRowFlag, aNextRowFlag) != ID_TRUE,
                    error_invalid_rowflag );

    IDE_TEST( insertRowPiece( aStatistics,
                              aTrans,
                              aTableInfoPtr,
                              aTableHeader,
                              aInsertInfo,
                              sRowFlag,
                              aNextRowPieceSID,
                              &(aUpdateInfo->mNewRowHdrInfo->mInfiniteSCN),
                              ID_TRUE, /* aIsNeedUndoRec */
                              ID_TRUE,
                              aReplicate,
                              &sInsertRowPieceSID )
              != IDE_SUCCESS );

    /* 
     * BUG-37529 [sm-disk-collection] [DRDB] The change row piece logic
     * generates invalid undo record.
     * update���� rowPiece�� �����ϴ� page�� overflowData�� insert�ϴ°���
     * page self aging�� �߻��ߴٴ� �ǹ��̴�.
     * page self aging���� compactPage�� ����Ǹ� update���� rowPiece�� ��ġ��
     * �Ȱ�����. �̷����� ���ڷ� ���޹��� aSlotPtr�� ���̻� ��ȿ�� slot��
     * ��ġ�� ����Ű�� �������� GSID�� �̿��� slot�� ��ġ�� �ٽñ��Ѵ�.
     */
    if ( SD_MAKE_PID(sInsertRowPieceSID) == SC_MAKE_PID( aSlotGRID ) )
    {
        IDE_ERROR( getSlotPtr( aMtx, aSlotGRID, &aSlotPtr ) == IDE_SUCCESS );
    }

    /* rowpiece���� row header�� ����� row data�� ��� �����Ѵ�. */
    IDE_TEST( truncateRowPieceData( aStatistics,
                                    aTableHeader,
                                    aMtx,
                                    aStartInfo,
                                    aSlotPtr,
                                    aSlotGRID,
                                    aUpdateInfo,
                                    sInsertRowPieceSID,
                                    aReplicate,
                                    aOverwriteInfo )
              != IDE_SUCCESS );

    *aInsertRowPieceSID = sInsertRowPieceSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_rowflag )
    {
        ideLog::log( IDE_ERR_0,
                     "TableColCount:        %"ID_UINT32_FMT"\n"
                     "InheritFlag:          %"ID_UINT32_FMT"\n"
                     "RowFlag:              %"ID_UINT32_FMT"\n"
                     "NextRowFlag:          %"ID_UINT32_FMT"\n"
                     "NextRowPieceSID:      %"ID_vULONG_FMT"\n",
                     smLayerCallback::getColumnCount( aTableHeader ),
                     sInheritRowFlag,
                     sRowFlag,
                     aNextRowFlag,
                     aNextRowPieceSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ Old Row Hdr Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ New Row Hdr Info ]"  );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "[ SlotGRID ]\n" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aInsertInfo,
                        ID_SIZEOF(sdcRowPieceInsertInfo),
                        "[ Insert Info ]" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "[ Update Info ]"  );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aOverwriteInfo,
                        ID_SIZEOF(sdcRowPieceOverwriteInfo),
                        "[ Overwrite Info ]" );

        ideLog::logMem( IDE_DUMP_0, aSlotPtr, SD_PAGE_SIZE,
                        "[ Dump Row Data ]\n" );

        ideLog::logMem( IDE_DUMP_0,
                        sdpPhyPage::getPageStartPtr(aSlotPtr),
                        SD_PAGE_SIZE, "[ Dump Page ]\n" );

        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                 "invalid row flag") );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece���� row header�� ����� row data�� ��� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::truncateRowPieceData( idvSQL                         *aStatistics,
                                     void                           *aTableHeader,
                                     sdrMtx                         *aMtx,
                                     sdrMtxStartInfo                *aStartInfo,
                                     UChar                          *aSlotPtr,
                                     scGRID                          aSlotGRID,
                                     const sdcRowPieceUpdateInfo    *aUpdateInfo,
                                     sdSID                           aNextRowPieceSID,
                                     idBool                          aReplicate,
                                     sdcRowPieceOverwriteInfo       *aOverwriteInfo )
{
    IDE_ASSERT( aTableHeader        != NULL );
    IDE_ASSERT( aMtx                != NULL );
    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aSlotPtr            != NULL );
    IDE_ASSERT( aUpdateInfo         != NULL );
    IDE_ASSERT( aOverwriteInfo      != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    SDC_INIT_OVERWRITE_INFO(aOverwriteInfo, aUpdateInfo);

    aOverwriteInfo->mNewRowHdrInfo->mColCount = 0;
    aOverwriteInfo->mNewRowHdrInfo->mRowFlag  = SDC_ROWHDR_H_FLAG;

    aOverwriteInfo->mUptAftInModeColCnt       = 0;
    aOverwriteInfo->mUptAftLobDescCnt         = 0;
    aOverwriteInfo->mLstColumnOverwriteSize   = 0;
    aOverwriteInfo->mNewRowPieceSize =
        SDC_ROWHDR_SIZE + SDC_EXTRASIZE_FOR_CHAINING;

    if ( canReallocSlot( aSlotPtr, aOverwriteInfo->mNewRowPieceSize )
         == ID_FALSE )
    {
        ideLog::log( IDE_ERR_0,
                     "NewRowPieceSize: %"ID_UINT32_FMT"\n",
                     aOverwriteInfo->mNewRowPieceSize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&aSlotGRID,
                        ID_SIZEOF(scGRID),
                        "SlotGRID Dump:" );

        IDE_ASSERT( 0 );
    }

    IDE_TEST( overwriteRowPiece( aStatistics,
                                 aTableHeader,
                                 aMtx,
                                 aStartInfo,
                                 aSlotPtr,
                                 aSlotGRID,
                                 aOverwriteInfo,
                                 aNextRowPieceSID,
                                 aReplicate )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece���� next link ������ �����ϰų� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::changeRowPieceLink(
                     idvSQL               * aStatistics,
                     void                 * aTrans,
                     void                 * aTableHeader,
                     smSCN                * aCSInfiniteSCN,
                     sdSID                  aSlotSID,
                     sdSID                  aNextRowPieceSID )
{
    sdrMtx                  sMtx;
    sdrMtxStartInfo         sStartInfo;
    UInt                    sDLogAttr     = ( SM_DLOG_ATTR_DEFAULT |
                                              SM_DLOG_ATTR_UNDOABLE ) ;
    sdrMtxLogMode           sMtxLogMode   = SDR_MTX_LOGGING;
    UChar                 * sSlotPtr;
    UChar                 * sWritePtr;
    sdpPageListEntry      * sEntry;
    scSpaceID               sTableSpaceID;
    smOID                   sTableOID;
    scGRID                  sSlotGRID;
    sdSID                   sUndoSID = SD_NULL_SID;
    sdcRowHdrInfo           sOldRowHdrInfo;
    sdcRowHdrInfo           sNewRowHdrInfo;
    smSCN                   sFstDskViewSCN;
    UChar                 * sNewSlotPtr;
    UChar                   sNewCTSlotIdx;
    SShort                  sFSCreditSize = 0;
    UShort                  sOldRowPieceSize;
    UShort                  sState        = 0;
    sdcUndoSegment        * sUDSegPtr;

    IDE_ASSERT( aTrans       != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aSlotSID     != SD_NULL_SID );

    sEntry =
        (sdpPageListEntry*)(smcTable::getDiskPageListEntry(aTableHeader));
    IDE_ASSERT( sEntry != NULL );

    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );

    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(aSlotSID),
                               SD_MAKE_SLOTNUM(aSlotSID) );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   sMtxLogMode,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr)
              != IDE_SUCCESS );
    sState = 1;

    sdrMiniTrans::makeStartInfo( &sMtx, &sStartInfo );

    IDE_TEST( prepareUpdatePageBySID( aStatistics,
                                      &sMtx,
                                      sTableSpaceID,
                                      aSlotSID,
                                      SDB_SINGLE_PAGE_READ,
                                      &sSlotPtr,
                                      &sNewCTSlotIdx ) != IDE_SUCCESS );

    getRowHdrInfo( sSlotPtr, &sOldRowHdrInfo );

    /* SDC_IS_LAST_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_TRUE
     * last rowpiece�� next link ���濬���� �߻��� ���� ����. */
    if ( ( SM_SCN_IS_DELETED(sOldRowHdrInfo.mInfiniteSCN) == ID_TRUE ) ||
         ( SDC_IS_LAST_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_TRUE ) )
    {
        ideLog::log( IDE_ERR_0,
                     "SpaceID: %"ID_UINT32_FMT", "
                     "SlotSID: %"ID_UINT32_FMT"\n",
                     sTableSpaceID,
                     aSlotSID );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&sOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "OldRowHdrInfo Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aTrans );
    if ( sUDSegPtr->addChangeRowPieceLinkUndoRec(
                            aStatistics,
                            &sStartInfo,
                            sTableOID,
                            sSlotPtr,
                            sSlotGRID,
                            (aNextRowPieceSID == SD_NULL_SID ? ID_TRUE : ID_FALSE),
                            &sUndoSID)
        != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� ��
         * */
        sdrMiniTrans::setIgnoreMtxUndo( &sMtx );
        IDE_TEST( 1 );
    }

    if ( aNextRowPieceSID == SD_NULL_SID )
    {
        sOldRowPieceSize = getRowPieceSize(sSlotPtr);
        sFSCreditSize    =
            calcFSCreditSize( sOldRowPieceSize,
                              (sOldRowPieceSize - SDC_EXTRASIZE_FOR_CHAINING) );
    }

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          sNewCTSlotIdx,
                          *aCSInfiniteSCN,
                          sUndoSID,
                          sOldRowHdrInfo.mColCount,
                          sOldRowHdrInfo.mRowFlag,
                          sFstDskViewSCN );

    if ( aNextRowPieceSID == SD_NULL_SID )
    {
        /* next link ������ �����ϴ� ��� */

        /* last rowpiece�� delete first column piece �������� ����
         * ������ remove�Ǵ� ���, prev rowpiece�� next link ������ �����ϰ�
         * L flag�� �����޾ƾ� �Ѵ�. */
        SM_SET_FLAG_ON(sNewRowHdrInfo.mRowFlag, SDC_ROWHDR_L_FLAG);

        IDE_TEST( truncateNextLink( aSlotSID,
                                    sSlotPtr,
                                    &sNewRowHdrInfo,
                                    &sNewSlotPtr )
                  != IDE_SUCCESS );

        sdrMiniTrans::setRefOffset(&sMtx, sTableOID);

        IDE_TEST( writeChangeRowPieceLinkRedoUndoLog( sNewSlotPtr,
                                                      sSlotGRID,
                                                      &sMtx,
                                                      aNextRowPieceSID )
                  != IDE_SUCCESS );

        IDE_TEST( sdcTableCTL::bind(
                      &sMtx,
                      sTableSpaceID,
                      sNewSlotPtr,
                      sOldRowHdrInfo.mCTSlotIdx,
                      sNewRowHdrInfo.mCTSlotIdx,
                      SD_MAKE_SLOTNUM(aSlotSID),
                      sOldRowHdrInfo.mExInfo.mFSCredit,
                      sFSCreditSize,
                      ID_FALSE ) /* aIncDeleteRowCnt */
                  != IDE_SUCCESS );

        // reallocSlot�� �����Ŀ�, Segment�� ���� ���뵵 ���濬���� �����Ѵ�.
        IDE_TEST( sdpPageList::updatePageState(
                      aStatistics,
                      sTableSpaceID,
                      sEntry,
                      sdpPhyPage::getPageStartPtr(sNewSlotPtr),
                      &sMtx )
                  != IDE_SUCCESS );
    }
    else
    {
        /* next link ������ �����ϴ� ��� */

        sWritePtr = sSlotPtr;

        sWritePtr = writeRowHdr( sWritePtr, &sNewRowHdrInfo );

        sWritePtr = writeNextLink( sWritePtr, aNextRowPieceSID );

        if ( sFSCreditSize != 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "FSCreditSize: %"ID_UINT32_FMT"\n",
                         sFSCreditSize );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        sdrMiniTrans::setRefOffset(&sMtx, sTableOID);

        IDE_TEST( writeChangeRowPieceLinkRedoUndoLog( sSlotPtr,
                                                      sSlotGRID,
                                                      &sMtx,
                                                      aNextRowPieceSID )
                  != IDE_SUCCESS );

         IDE_TEST( sdcTableCTL::bind( &sMtx,
                                      sTableSpaceID,
                                      sSlotPtr,
                                      sOldRowHdrInfo.mCTSlotIdx,
                                      sNewRowHdrInfo.mCTSlotIdx,
                                      SD_MAKE_SLOTNUM(aSlotSID),
                                      sOldRowHdrInfo.mExInfo.mFSCredit,
                                      sFSCreditSize,
                                      ID_FALSE ) /* aIncDeleteRowCnt */
                   != IDE_SUCCESS );
    }


    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece���� next link ������ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::truncateNextLink( sdSID                   aSlotSID,
                                 UChar                  *aOldSlotPtr,
                                 const sdcRowHdrInfo    *aNewRowHdrInfo,
                                 UChar                 **aNewSlotPtr )
{
    UChar                  *sDataAreaPtr;
    UChar                  *sWritePtr;
    UChar                  *sNewSlotPtr;
    UChar                   sOldRowPieceBodyImage[SD_PAGE_SIZE];
    UShort                  sRowPieceBodySize;
    UShort                  sNewRowPieceSize;

    IDE_ASSERT( aOldSlotPtr    != NULL );
    IDE_ASSERT( aNewRowHdrInfo != NULL );
    IDE_ASSERT( aNewSlotPtr    != NULL );

    sRowPieceBodySize = getRowPieceBodySize(aOldSlotPtr);
    sDataAreaPtr      = getDataArea(aOldSlotPtr);

    /* rowpiece body�� temp buffer�� �����صд�. */
    idlOS::memcpy( sOldRowPieceBodyImage,
                   sDataAreaPtr,
                   sRowPieceBodySize );

    sNewRowPieceSize =
        getRowPieceSize(aOldSlotPtr) - SDC_EXTRASIZE_FOR_CHAINING;

    IDE_TEST( reallocSlot( aSlotSID,
                           aOldSlotPtr,
                           sNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewSlotPtr )
              != IDE_SUCCESS );
    aOldSlotPtr = NULL;

    sWritePtr = sNewSlotPtr;
    sWritePtr = writeRowHdr( sWritePtr,
                             aNewRowHdrInfo );

    /* rowpiece header�� ����� �Ŀ�,
     * next link ������ ������� �ʰ�,
     * rowpiece body�� ����Ѵ�. */
    idlOS::memcpy( sWritePtr,
                   sOldRowPieceBodyImage,
                   sRowPieceBodySize );

    IDE_ERROR_MSG( sNewRowPieceSize == getRowPieceSize( sNewSlotPtr ),
                   "Calc Size : %"ID_UINT32_FMT"\n"
                   "Slot Size : %"ID_UINT32_FMT"\n",
                   sNewRowPieceSize,
                   getRowPieceSize( sNewSlotPtr ) );

    *aNewSlotPtr = sNewSlotPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_CHANGE_ROW_PIECE_LINK
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [next pid(4), next slotnum(2)]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeChangeRowPieceLinkRedoUndoLog(
    UChar          *aSlotPtr,
    scGRID          aSlotGRID,
    sdrMtx         *aMtx,
    sdSID           aNextRowPieceSID )
{
    sdrLogType    sLogType;
    UShort        sLogSize;
    UChar         sFlag;
    SShort        sChangeSize;
    scPageID      sNextPageID;
    scSlotNum     sNextSlotNum;
    UChar         sRowFlag;

    IDE_ASSERT( aSlotPtr != NULL );
    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    SDC_GET_ROWHDR_1B_FIELD(aSlotPtr, SDC_ROWHDR_FLAG, sRowFlag);

    sLogType = SDR_SDC_CHANGE_ROW_PIECE_LINK;

    sLogSize = ID_SIZEOF(sFlag) +
        ID_SIZEOF(sChangeSize) +
        SDC_ROWHDR_SIZE+
        SDC_EXTRASIZE_FOR_CHAINING;

    IDE_TEST( sdrMiniTrans::writeLogRec(
                  aMtx,
                  aSlotGRID,
                  NULL,
                  sLogSize,
                  sLogType ) != IDE_SUCCESS);

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE;

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sFlag,
                                  ID_SIZEOF(sFlag))
              != IDE_SUCCESS );

    if ( aNextRowPieceSID == SD_NULL_SID )
    {
        /* next link ������ �����ϴ� ��� */
        IDE_ASSERT( SDC_IS_LAST_ROWPIECE(sRowFlag) == ID_TRUE );
        sChangeSize = -((SShort)SDC_EXTRASIZE_FOR_CHAINING);
    }
    else
    {
        /* next link ������ �����ϴ� ��� */
        IDE_ASSERT( SDC_IS_LAST_ROWPIECE(sRowFlag) == ID_FALSE );
        sChangeSize = 0;
    }

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sChangeSize,
                                  ID_SIZEOF(sChangeSize))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  SDC_ROWHDR_SIZE )
              != IDE_SUCCESS);


    sNextPageID   = SD_MAKE_PID( aNextRowPieceSID );
    sNextSlotNum  = SD_MAKE_SLOTNUM( aNextRowPieceSID );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sNextPageID,
                                  ID_SIZEOF(sNextPageID) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sNextSlotNum,
                                  ID_SIZEOF(sNextSlotNum) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  change rowpiece link undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *  CHANGE_ROW_PIECE_LINK �α״� redo, undo �α��� ������ ���� ������
 *  undo record���� undo info layer�� ���븸 ����,
 *  clr redo log�� �ۼ��ϸ� �ȴ�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeChangeRowPieceLinkCLR(
    const UChar            *aUndoRecHdr,
    scGRID                  aSlotGRID,
    sdSID                   aUndoSID,
    sdrMtx                 *aMtx )
{
    const UChar        *sUndoRecBodyPtr;
    UChar              *sSlotDirStartPtr;
    sdrLogType          sLogType;
    UShort              sLogSize;

    IDE_ASSERT( aUndoRecHdr != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_CHANGE_ROW_PIECE_LINK;

    sSlotDirStartPtr = sdpPhyPage::getSlotDirStartPtr(
                                      (UChar*)aUndoRecHdr);

    /* undo info layer�� ũ�⸦ ����. */
    sLogSize = sdcUndoSegment::getUndoRecSize(
                      sSlotDirStartPtr,
                      SD_MAKE_SLOTNUM(aUndoSID) );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sUndoRecBodyPtr = (const UChar*)
        sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)sUndoRecBodyPtr,
                                  sLogSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::undo_CHANGE_ROW_PIECE_LINK(
                             sdrMtx               *aMtx,
                             UChar                *aLogPtr,
                             UChar                *aSlotPtr,
                             sdSID                 aSlotSID,
                             sdcOperToMakeRowVer   aOper4RowPiece,
                             UChar                *aRowBuf4MVCC,
                             UChar               **aNewSlotPtr4Undo,
                             SShort               *aFSCreditSize )
{
    UChar              sOldSlotImage[SD_PAGE_SIZE];
    UChar             *sOldSlotImagePtr;
    UChar             *sOldSlotPtr      = aSlotPtr;
    UChar             *sNewSlotPtr;
    UChar             *sLogPtr          = (UChar*)aLogPtr;
    UChar             *sUndoPageStartPtr;
    SChar             *sDumpBuf;
    UShort             sOldRowPieceSize;
    UShort             sNewRowPieceSize;
    UChar              sFlag;
    SShort             sChangeSize;
    UShort             sColCount;
    UShort             sColSeqInRowPiece;
    UChar              sRowFlag;
    scPageID           sPageID;
    UInt               sLength;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );

    sOldRowPieceSize = getRowPieceSize(sOldSlotPtr);
    SDC_GET_ROWHDR_FIELD(sOldSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);

    /* reallocSlot()�� �ϴ� ���߿�
     * compact page ������ �߻��� �� �ֱ� ������
     * old slot image�� temp buffer�� �����ؾ� �Ѵ�. */
    sOldSlotImagePtr = sOldSlotImage;
    idlOS::memcpy( sOldSlotImagePtr,
                   sOldSlotPtr,
                   sOldRowPieceSize );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sChangeSize,
                          ID_SIZEOF(sChangeSize) );

    if ( (sChangeSize != 0) &&
         (sChangeSize != SDC_EXTRASIZE_FOR_CHAINING) )
    {
        // BUG-28807 Case-23941 sdcRow::undo_CHANGE_ROW_PIECE_LINK ������
        //           ASSERT�� ���� ����� �ڵ� �߰�

        sUndoPageStartPtr = sdpPhyPage::getPageStartPtr( sLogPtr );

        ideLog::log( IDE_ERR_0,
                     "ChangeSize(%"ID_UINT32_FMT") is Invalid."
                     " ( != 0 and != %"ID_UINT32_FMT" )\n"
                     "Undo Record Offset : %"ID_UINT32_FMT"\n"
                     "Row Piece Size     : %"ID_UINT32_FMT"\n"
                     "Row Column Count   : %"ID_UINT32_FMT"\n",
                     sChangeSize,
                     SDC_EXTRASIZE_FOR_CHAINING,
                     sLogPtr - sUndoPageStartPtr, // LogPtr�� Page �������� Offset
                     sOldRowPieceSize,
                     sColCount );

        // Dump Row Data Hex Data
        // sOldSlotPtr : sdcRow::fetchRowPiece::sTmpBuffer[SD_PAGE_SIZE]
        ideLog::logMem( IDE_DUMP_0,
                        sOldSlotPtr,
                        SD_PAGE_SIZE,
                        "[ Hex Dump Row Data ]\n" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sUndoPageStartPtr,
                                     "[ Hex Dump Undo Page ]\n" );

        if ( iduMemMgr::calloc( IDU_MEM_SM_SDC, 
                                1,
                                ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                (void**)&sDumpBuf )
            == IDE_SUCCESS )
        {

            (void)sdcUndoSegment::dump( sUndoPageStartPtr,
                                        sDumpBuf,
                                        IDE_DUMP_DEST_LIMIT );
            ideLog::log( IDE_ERR_0, "%s\n", sDumpBuf );

            (void) iduMemMgr::free( sDumpBuf );
        }
        IDE_ERROR( 0 );
    }

    if ( aOper4RowPiece == SDC_MVCC_MAKE_VALROW )
    {
        /* MVCC�� ���� undo�� �����ϴ� ���,
         * reallocSlot()�� �ϸ� �ȵǰ�,
         * ���ڷ� �Ѿ�� temp buffer pointer�� ����ؾ� �Ѵ�. */
        IDE_ERROR( aRowBuf4MVCC != NULL );
        sNewSlotPtr = aRowBuf4MVCC;
    }
    else
    {
        sNewRowPieceSize = sOldRowPieceSize + (sChangeSize);

        if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
        {
            IDE_ERROR( aFSCreditSize != NULL );
            *aFSCreditSize = calcFSCreditSize( sNewRowPieceSize,
                                              sOldRowPieceSize );

            IDE_TEST( sdcTableCTL::unbind(
                          aMtx,
                          sOldSlotPtr,         /* Undo�Ǳ��� RowPiecePtr */
                          *(UChar*)sOldSlotPtr,/* aCTSlotIdxBfrUndo */
                          *(UChar*)sLogPtr,    /* aCTSlotIdxAftUndo */
                          *aFSCreditSize,
                          ID_FALSE )           /* aDecDeleteRowCnt */
                      != IDE_SUCCESS );
        }

        IDE_TEST( reallocSlot( aSlotSID,
                               sOldSlotPtr,
                               sNewRowPieceSize,
                               ID_FALSE, /* aReserveFreeSpaceCredit */
                               &sNewSlotPtr )
                  != IDE_SUCCESS );
        sOldSlotPtr = NULL;

        *aNewSlotPtr4Undo = sNewSlotPtr;
    }

    IDE_ERROR( sNewSlotPtr != NULL );

    SDC_GET_ROWHDR_1B_FIELD(sLogPtr, SDC_ROWHDR_FLAG, sRowFlag);
    IDE_ERROR( SDC_IS_LAST_ROWPIECE(sRowFlag) == ID_FALSE );
    ID_WRITE_AND_MOVE_BOTH( sNewSlotPtr, sLogPtr, SDC_ROWHDR_SIZE );

    ID_READ_VALUE( sLogPtr, &sPageID, ID_SIZEOF(sPageID) );
    IDE_ERROR( sPageID  != SC_NULL_PID );
    ID_WRITE_AND_MOVE_BOTH( sNewSlotPtr, sLogPtr, ID_SIZEOF(scPageID) );

    ID_WRITE_AND_MOVE_BOTH( sNewSlotPtr, sLogPtr, ID_SIZEOF(scSlotNum) );

    sOldSlotImagePtr = getDataArea(sOldSlotImagePtr);

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sLength = getColPieceLen(sOldSlotImagePtr);

        ID_WRITE_AND_MOVE_BOTH( sNewSlotPtr,
                                sOldSlotImagePtr,
                                SDC_GET_COLPIECE_SIZE(sLength) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcUpdateRowPieceLogSize(
                   const sdcRowPieceUpdateInfo *aUpdateInfo,
                   idBool                       aIsUndoRec,
                   idBool                       aIsReplicate )
{
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sLogSize             = 0;
    UShort                        sRPInfoLayerSize     = 0;
    UShort                        sUndoInfoLayerSize   = 0;
    UShort                        sUpdateInfoLayerSize = 0;
    UShort                        sRowImageLayerSize   = 0;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT_MSG( aUpdateInfo->mOldRowHdrInfo->mColCount ==
                    aUpdateInfo->mNewRowHdrInfo->mColCount,
                    "Old Row Column Count : %"ID_UINT32_FMT"\n"
                    "New Row Column Count : %"ID_UINT32_FMT"\n",
                    aUpdateInfo->mOldRowHdrInfo->mColCount,
                    aUpdateInfo->mNewRowHdrInfo->mColCount );

    sColCount = aUpdateInfo->mNewRowHdrInfo->mColCount;

    if ( aIsReplicate == ID_TRUE )
    {
        sRPInfoLayerSize =
            calcUpdateRowPieceLogSize4RP(aUpdateInfo, aIsUndoRec);
    }

    if ( aIsUndoRec == ID_TRUE )
    {
        sUndoInfoLayerSize = SDC_UNDOREC_HDR_SIZE +
                             ID_SIZEOF(scGRID);
    }

    sUpdateInfoLayerSize =
        ID_SIZEOF(UChar)  +               // flag
        ID_SIZEOF(UShort) +               // changesize
        ID_SIZEOF(UShort) +               // colcount
        ID_SIZEOF(UChar)  +               // column descset size
        calcColumnDescSetSize(sColCount); // column descset

    sRowImageLayerSize += SDC_ROWHDR_SIZE;

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            if ( aIsUndoRec == ID_TRUE )
            {
                sRowImageLayerSize +=
                    SDC_GET_COLPIECE_SIZE(sColumnInfo->mOldValueInfo.mValue.length);
            }
            else
            {
                sRowImageLayerSize +=
                    SDC_GET_COLPIECE_SIZE(sColumnInfo->mNewValueInfo.mValue.length);
            }
        }
    }

    sLogSize =
        sRPInfoLayerSize +
        sUndoInfoLayerSize +
        sUpdateInfoLayerSize +
        sRowImageLayerSize;

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcOverwriteRowPieceLogSize(
                       const sdcRowPieceOverwriteInfo  *aOverwriteInfo,
                       idBool                           aIsUndoRec,
                       idBool                           aIsReplicate )
{
    UShort    sLogSize             = 0;
    UShort    sRPInfoLayerSize     = 0;
    UShort    sUndoInfoLayerSize   = 0;
    UShort    sRowImageLayerSize   = 0;
    UShort    sUpdateInfoLayerSize = 0;

    IDE_ASSERT( aOverwriteInfo != NULL );

    if ( aIsReplicate == ID_TRUE )
    {
        sRPInfoLayerSize =
            calcOverwriteRowPieceLogSize4RP(aOverwriteInfo, aIsUndoRec);
    }

    if ( aIsUndoRec == ID_TRUE )
    {
        sUndoInfoLayerSize = SDC_UNDOREC_HDR_SIZE + ID_SIZEOF(scGRID);
    }

    // flag(1), size(2)
    sUpdateInfoLayerSize = (1) + (2);

    if ( aIsUndoRec == ID_TRUE )
    {
        sRowImageLayerSize = aOverwriteInfo->mOldRowPieceSize;
    }
    else
    {
        sRowImageLayerSize = aOverwriteInfo->mNewRowPieceSize;
    }

    sLogSize =
        sRPInfoLayerSize +
        sUndoInfoLayerSize +
        sUpdateInfoLayerSize +
        sRowImageLayerSize;

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcDeleteFstColumnPieceLogSize(
    const sdcRowPieceUpdateInfo *aUpdateInfo,
    idBool                       aIsReplicate )
{
    const sdcColumnInfo4Update   *sFstColumnInfo;
    UShort                        sLogSize             = 0;
    UShort                        sRPInfoLayerSize     = 0;
    UShort                        sUndoInfoLayerSize   = 0;
    UShort                        sUpdateInfoLayerSize = 0;
    UShort                        sRowImageLayerSize   = 0;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aUpdateInfo->mIsDeleteFstColumnPiece == ID_TRUE );

    if ( aIsReplicate == ID_TRUE )
    {
        // column count(2)
        // column seq(2)
        // column id(4)
        sRPInfoLayerSize = (2) + (2) + (4);
    }

    sUndoInfoLayerSize = SDC_UNDOREC_HDR_SIZE + ID_SIZEOF(scGRID);

    // flag(1), size(2)
    sUpdateInfoLayerSize = (1) + (2);

    sRowImageLayerSize += SDC_ROWHDR_SIZE;

    sFstColumnInfo = aUpdateInfo->mColInfoList;
    sRowImageLayerSize +=
        SDC_GET_COLPIECE_SIZE(sFstColumnInfo->mOldValueInfo.mValue.length);

    sLogSize =
        sRPInfoLayerSize +
        sUndoInfoLayerSize +
        sUpdateInfoLayerSize +
        sRowImageLayerSize;

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_UPDATE_ROW_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)], [column count(2)],
 *   | [column desc set size(1)], [column desc set(1~128)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [ {column len(1 or 3),column data} ... ]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 *   SDR_SDC_UPDATE_ROW_PIECE �α��� Row Image Layer���� update�� column ������ ��ϵȴ�.
 *   Row Image Layer�� ��ϵ� column ������ ������ redo�� �Ϸ���,
 *   �� �÷����� local sequence(sequence in rowpiece)�� �˾ƾ� �ϴµ�,
 *   �̷� ������ Update Info Layer�� column desc set���� ����Ѵ�.
 *
 *   column desc set�� unix�� file desc set�� ���� �����ϴ�.
 *   column desc set�� 1bit�� �ϳ��� �÷��� ǥ���ϴµ�,
 *   update �÷��� ��� �ش� ��Ʈ�� 1�� �����ϰ�,
 *   update �÷��� �ƴϸ� �ش� ��Ʈ�� 0���� �����Ѵ�.
 *
 *   ���� ��� �ϳ��� rowpiece�� 8���� �÷��� ����Ǿ� �ִµ�,
 *   �� �߿��� 3,5�� �÷��� update�Ǵ� ���,
 *   column desc set�� 00010100�� �ȴ�.
 *
 *   column desc set�� unix�� file desc set�� �ٸ����� �Ѱ��� �ִ�.
 *   unix�� file desc set�� 128byte ����ũ��������,
 *   column desc set�� rowpiece�� ����� �÷��� ������ ����
 *   ũ�Ⱑ �������̴�.(1byte ~ 128byte)
 *
 *   �÷��� ������ 8�� �����̸� column desc set�� 1byte�̰�,
 *   �÷��� ������ 8~16�� �����̸� column desc set�� 2byte�̰�,
 *   �÷��� ������ 1016~1024�� �����̸� column desc set�� 128byte�̴�.
 *
 **********************************************************************/
void sdcRow::calcColumnDescSet( const sdcRowPieceUpdateInfo *aUpdateInfo,
                                UShort                       aColCount,
                                sdcColumnDescSet            *aColDescSet)
{
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aColDescSet != NULL );
    IDE_ASSERT( aColCount    > 0    );

    /* column desc set �ʱ�ȭ */
    SDC_CD_ZERO(aColDescSet);

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            /* update �÷��� ��� �ش� bit�� 1�� �����Ѵ�. */
            SDC_CD_SET(sColSeqInRowPiece, aColDescSet);
        }
    }
}


/***********************************************************************
 *
 * Description :
 *
 *   column desc set�� rowpiece�� ����� �÷��� ������ ����
 *   ũ�Ⱑ �������̴�.(1byte ~ 128byte)
 *
 *   �÷��� ������ 8�� �����̸� column desc set�� 1byte�̰�,
 *   �÷��� ������ 8~16�� �����̸� column desc set�� 2byte�̰�,
 *   �÷��� ������ 1016~1024�� �����̸� column desc set�� 128byte�̴�.
 *
 **********************************************************************/
UChar sdcRow::calcColumnDescSetSize( UShort    aColCount )
{
    IDE_ASSERT_MSG( aColCount <= SMI_COLUMN_ID_MAXIMUM,
                    "Column Count :%"ID_UINT32_FMT"\n",
                    aColCount );

    return (UChar)( (aColCount + 7) / 8 );
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcInsertRowPieceLogSize4RP(
                       const sdcRowPieceInsertInfo *aInsertInfo )
{
    const sdcColumnInfo4Insert   *sColumnInfo;
    UShort                        sCurrColSeq;
    UShort                        sLogSize;

    IDE_ASSERT( aInsertInfo != NULL );

    sLogSize  = (0);
    sLogSize += (2);    // column count

    for ( sCurrColSeq = aInsertInfo->mStartColSeq;
          sCurrColSeq <= aInsertInfo->mEndColSeq;
          sCurrColSeq++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sCurrColSeq;

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) == ID_TRUE )
        {
            if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
            {
                sLogSize += (2);    // column seq
                sLogSize += (4);    // column id
                sLogSize += (4);    // column total length
            }
            else
            {
                if ( sColumnInfo->mIsUptCol == ID_TRUE )
                {
                    sLogSize += (2);    // column seq
                    sLogSize += (4);    // column id
                    sLogSize += (4);    // column total length
                }
            }
        }
    }

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcUpdateRowPieceLogSize4RP(
                       const sdcRowPieceUpdateInfo *aUpdateInfo,
                       idBool                       aIsUndoRec )
{
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UShort                        sLogSize;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT_MSG( aUpdateInfo->mOldRowHdrInfo->mColCount ==
                    aUpdateInfo->mNewRowHdrInfo->mColCount,
                    "Old Row Column Count : %"ID_UINT32_FMT"\n"
                    "New Row Column Count : %"ID_UINT32_FMT"\n",
                    aUpdateInfo->mOldRowHdrInfo->mColCount,
                    aUpdateInfo->mNewRowHdrInfo->mColCount );

    sColCount = aUpdateInfo->mNewRowHdrInfo->mColCount;

    sLogSize  = (0);
    sLogSize += (2);    // column count

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
        //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
        // In Mode Ȯ�� �� Undo Rec Log �̸� Old Value �� Ȯ���ϵ��� ����
        if ( SDC_IS_IN_MODE_UPDATE_COLUMN( sColumnInfo,
                                           aIsUndoRec ) == ID_TRUE )
        {
            sLogSize += (2);    // column seq
            sLogSize += (4);    // column id

            if ( aIsUndoRec != ID_TRUE )
            {
                sLogSize += (4);    // column total length
            }
        }
    }

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcOverwriteRowPieceLogSize4RP(
                       const sdcRowPieceOverwriteInfo *aOverwriteInfo,
                       idBool                          aIsUndoRec )
{
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UShort                        sLogSize;

    IDE_ASSERT( aOverwriteInfo != NULL );

    if ( aIsUndoRec == ID_TRUE )
    {
        sColCount = aOverwriteInfo->mOldRowHdrInfo->mColCount;
    }
    else
    {
        sColCount = aOverwriteInfo->mNewRowHdrInfo->mColCount;
    }

    sLogSize  = (0);
    sLogSize += (2);    // column count

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aOverwriteInfo->mColInfoList + sColSeqInRowPiece;

        // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
        //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
        // In Mode Ȯ�� �� Undo Rec Log �̸� Old Value �� Ȯ���ϵ��� ����
        if ( SDC_IS_IN_MODE_UPDATE_COLUMN( sColumnInfo,
                                           aIsUndoRec ) == ID_TRUE )
        {
            sLogSize += (2);    // column seq in rowpiece
            sLogSize += (4);    // column id

            if ( aIsUndoRec != ID_TRUE )
            {
                sLogSize += (4);    // column total length
            }
        }
    }

    if ( aIsUndoRec == ID_TRUE )
    {
        // ( column seq(2) + column id(4) ) * Trailing Null Update Count
        sLogSize += ( (2 + 4) * aOverwriteInfo->mTrailingNullUptCount );
    }

    return sLogSize;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::calcDeleteRowPieceLogSize4RP(
    const UChar                    *aSlotPtr,
    const sdcRowPieceUpdateInfo    *aUpdateInfo )
{
    UShort sLogSize;
    UShort sLogSizePerColumn;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    sLogSize  = (0);
    sLogSize += (2);    // column count

    sLogSizePerColumn = (2) + (4);  // column seq(2) + column id(4)

    sLogSize += (sLogSizePerColumn * aUpdateInfo->mUptBfrInModeColCnt);

    return sLogSize;
}

/***********************************************************************
 *
 * Description :
 *  ���� rowpiece�� ������ ����� �÷��� update�Ǵ� ���,
 *  �� �÷��� first piece�� �����ϰ� �ִ� rowpiece����
 *  update ������ ����ϹǷ�, ������ rowpiece������
 *  �ش� column piece�� �����ؾ� �Ѵ�.
 *
 *  �� �Լ��� rowpiece���� ù��° column piece��
 *  �����ϴ� ������ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::deleteFstColumnPiece(
                     idvSQL                      *aStatistics,
                     void                        *aTrans,
                     void                        *aTableHeader,
                     smSCN                       *aCSInfiniteSCN,
                     sdrMtx                      *aMtx,
                     sdrMtxStartInfo             *aStartInfo,
                     UChar                       *aOldSlotPtr,
                     sdSID                        aCurrRowPieceSID,
                     const sdcRowPieceUpdateInfo *aUpdateInfo,
                     sdSID                        aNextRowPieceSID,
                     idBool                       aReplicate )
{
    UChar                        *sWritePtr;
    const sdcColumnInfo4Update   *sColumnInfo;
    const sdcRowHdrInfo          *sOldRowHdrInfo;
    sdcRowHdrInfo                 sNewRowHdrInfo;
    smSCN                         sFstDskViewSCN;
    UChar                        *sNewSlotPtr;
    sdpPageListEntry             *sEntry;
    scGRID                        sSlotGRID;
    scSpaceID                     sTableSpaceID;
    smOID                         sTableOID;
    sdSID                         sUndoSID = SD_NULL_SID;
    UChar                         sRowFlag;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    SShort                        sFSCreditSize = 0;
    UChar                         sNewRowFlag;
    sdcUndoSegment              * sUDSegPtr;


    IDE_ASSERT( aTrans       != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aOldSlotPtr  != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );
    IDE_ASSERT( aUpdateInfo->mIsDeleteFstColumnPiece == ID_TRUE );

    sOldRowHdrInfo = aUpdateInfo->mOldRowHdrInfo;
    sColCount      = sOldRowHdrInfo->mColCount;
    sRowFlag       = sOldRowHdrInfo->mRowFlag;

    /* head rowpiece�� delete first column piece ������ �߻��� �� ����. */
    IDE_ASSERT( SDC_IS_HEAD_ROWPIECE(sRowFlag) == ID_FALSE );
    IDE_ASSERT( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_F_FLAG )
                == ID_FALSE );
    IDE_ASSERT( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_P_FLAG )
                == ID_TRUE );
    IDE_ASSERT( sColCount >= 1 );

    sEntry = (sdpPageListEntry*)
             (smcTable::getDiskPageListEntry(aTableHeader));

    IDE_ASSERT( sEntry != NULL );

    sTableSpaceID  = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID      = smLayerCallback::getTableOID( aTableHeader );

    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(aCurrRowPieceSID),
                               SD_MAKE_SLOTNUM(aCurrRowPieceSID) );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aTrans );
    if ( sUDSegPtr->addDeleteFstColumnPieceUndoRec( aStatistics,
                                                    aStartInfo,
                                                    sTableOID,
                                                    aOldSlotPtr,
                                                    sSlotGRID,
                                                    aUpdateInfo,
                                                    aReplicate,
                                                    &sUndoSID ) != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� �� */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }

    IDE_TEST( reallocSlot( aUpdateInfo->mRowPieceSID,
                           aOldSlotPtr,
                           aUpdateInfo->mNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewSlotPtr )
              != IDE_SUCCESS );


    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( sNewSlotPtr ) )
                == smLayerCallback::getTableOID( aTableHeader ) );

    aOldSlotPtr = NULL;
    IDE_ASSERT( sNewSlotPtr != NULL );

    IDE_TEST( sdpPageList::updatePageState( aStatistics,
                                            sTableSpaceID,
                                            sEntry,
                                            sdpPhyPage::getPageStartPtr(sNewSlotPtr),
                                            aMtx )
              != IDE_SUCCESS );

    sFSCreditSize = calcFSCreditSize( aUpdateInfo->mOldRowPieceSize,
                                      aUpdateInfo->mNewRowPieceSize );
    if ( sFSCreditSize < 0 )
    {
        ideLog::log( IDE_ERR_0,
                     "SpaceID: %"ID_UINT32_FMT", "
                     "TableOID: %"ID_vULONG_FMT", "
                     "RowPieceSID: %"ID_UINT64_FMT", "
                     "FSCreditSize: %"ID_INT32_FMT"\n",
                     sTableSpaceID,
                     sTableOID,
                     aCurrRowPieceSID,
                     sFSCreditSize );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "UpdateInfo Dump:" );

        IDE_ASSERT( 0 );
    }

    sNewRowFlag = aUpdateInfo->mNewRowHdrInfo->mRowFlag;

    /* ù��° column piece�� �����ϰ� �Ǹ�,
     * �ι��� column piece�� ù���� column piece�� �Ǵµ�,
     * �ι�° column piece�� prev rowpiece�� ������ ����� �� �ƴϹǷ�,
     * P flag�� off ��Ų��. */
    SM_SET_FLAG_OFF( sNewRowFlag, SDC_ROWHDR_P_FLAG );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    /* first column piece�� ���ŵǹǷ�,
     * column count�� �ϳ� ���ҽ�Ų��. */
    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                          *aCSInfiniteSCN,
                          sUndoSID,
                          (sOldRowHdrInfo->mColCount - 1),
                          sNewRowFlag,
                          sFstDskViewSCN );

    sWritePtr = sNewSlotPtr;
    sWritePtr = writeRowHdr( sWritePtr, &sNewRowHdrInfo );

    /* ���� RowPiece�� NextLink ������ �־��ٸ� �״�� ����Ѵ�.
     * ������ �ʿ��ϴٸ� ���Ŀ� SupplementJobInfo�� ó���� �ȴ�. */
    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        sWritePtr = writeNextLink( sWritePtr, aNextRowPieceSID );
    }

    /* first column piece�� �����ؾ� �ϹǷ�
     * loop�� 1���� �����Ѵ�. */
    for ( sColSeqInRowPiece = 1 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        sWritePtr = writeColPiece( sWritePtr,
                                   (UChar*)sColumnInfo->mOldValueInfo.mValue.value,
                                   sColumnInfo->mOldValueInfo.mValue.length,
                                   sColumnInfo->mOldValueInfo.mInOutMode );
    }

    IDE_DASSERT_MSG( aUpdateInfo->mNewRowPieceSize ==
                     getRowPieceSize( sNewSlotPtr ),
                     "Invalid Row Piece Size"
                     "Table OID : %"ID_vULONG_FMT"\n"
                     "Calc Size : %"ID_UINT32_FMT"\n"
                     "Slot Size : %"ID_UINT32_FMT"\n",
                     sTableOID,
                     aUpdateInfo->mNewRowPieceSize,
                     getRowPieceSize( sNewSlotPtr ) );

    sdrMiniTrans::setRefOffset(aMtx, sTableOID);

    IDE_TEST( writeDeleteFstColumnPieceRedoUndoLog( sNewSlotPtr,
                                                    sSlotGRID,
                                                    aUpdateInfo,
                                                    aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdcTableCTL::bind(
                      aMtx,
                      sTableSpaceID,
                      sNewSlotPtr,
                      aUpdateInfo->mOldRowHdrInfo->mCTSlotIdx,
                      aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                      SD_MAKE_SLOTNUM(aCurrRowPieceSID),
                      aUpdateInfo->mOldRowHdrInfo->mExInfo.mFSCredit,
                      sFSCreditSize,
                      ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description : UndoRecord�� ����Ѵ�.
 *
 *   SDC_UNDO_DELETE_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | [undo record header], [scGRID]
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [column len(1 or 3),column data]
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [column seq(2),column id(4)]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
UChar* sdcRow::writeDeleteFstColumnPieceRedoLog(
                    UChar                       *aWritePtr,
                    const UChar                 *aOldSlotPtr,
                    const sdcRowPieceUpdateInfo *aUpdateInfo )
{
    UChar                        *sWritePtr;
    const sdcColumnInfo4Update   *sFstColumnInfo;
    UChar                         sFlag;
    SShort                        sChangeSize;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aOldSlotPtr != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aUpdateInfo->mIsDeleteFstColumnPiece == ID_TRUE );

    sWritePtr = aWritePtr;

    sFstColumnInfo = aUpdateInfo->mColInfoList;

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE;
    ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sFlag );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = SDC_GET_COLPIECE_SIZE(
                      sFstColumnInfo->mOldValueInfo.mValue.length);
    IDE_ASSERT( sChangeSize > 0 );
    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            &sChangeSize,
                            ID_SIZEOF(sChangeSize) );

    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            aOldSlotPtr,
                            SDC_ROWHDR_SIZE );

    sWritePtr = writeColPiece( sWritePtr,
                               (UChar*)sFstColumnInfo->mOldValueInfo.mValue.value,
                               sFstColumnInfo->mOldValueInfo.mValue.length,
                               sFstColumnInfo->mOldValueInfo.mInOutMode );

    return sWritePtr;
}

/***********************************************************************
 *
 * Description : RowPiece�� ���� RedoUndo �α׸� ����Ѵ�.
 *
 *   SDR_SDC_DELETE_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteFstColumnPieceRedoUndoLog(
    const UChar                 *aSlotPtr,
    scGRID                       aSlotGRID,
    const sdcRowPieceUpdateInfo *aUpdateInfo,
    sdrMtx                      *aMtx )
{
    sdrLogType                    sLogType;
    UShort                        sLogSize;
    const sdcColumnInfo4Update   *sFstColumnInfo;
    UChar                         sFlag;
    SShort                        sChangeSize;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));
    IDE_ASSERT( aUpdateInfo->mIsDeleteFstColumnPiece == ID_TRUE );

    sLogType = SDR_SDC_DELETE_FIRST_COLUMN_PIECE;

    sLogSize = ID_SIZEOF(sFlag) +
               ID_SIZEOF(sChangeSize) +
               SDC_ROWHDR_SIZE;

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sFstColumnInfo = aUpdateInfo->mColInfoList;

    /*
     * ###   FSC �÷���   ###
     *
     * FSC �÷��� ��������� ���� �ڼ��� ������ sdcRow.h�� �ּ��� �����϶�
     *
     * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
     * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
     */
    sFlag  = 0;
    sFlag |= SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE;
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sFlag,
                                  ID_SIZEOF(sFlag))
              != IDE_SUCCESS );

    /* redo log : change size = new rowpiece size - old rowpiece size
     * undo rec : change size = old rowpiece size - new rowpiece size */
    sChangeSize = -(SDC_GET_COLPIECE_SIZE(sFstColumnInfo->mOldValueInfo.mValue.length));

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sChangeSize,
                                  ID_SIZEOF(sChangeSize))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (UChar*)aSlotPtr,
                                  SDC_ROWHDR_SIZE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  delete first column piece undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *  undo record�� ����� ������
 *  ADD_FIRST_COLUMN_PIECE �α� Ÿ������ �α׿� ����Ѵ�.
 *
 *   SDR_SDC_ADD_FIRST_COLUMN_PIECE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)], [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header], [column len(1 or 3),column data]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteFstColumnPieceCLR(
    const UChar             *aUndoRecHdr,
    scGRID                   aSlotGRID,
    sdSID                    aUndoSID,
    sdrMtx                  *aMtx )
{
    const UChar        *sUndoRecBodyPtr;
    UChar          * sSlotDirStartPtr;
    sdrLogType          sLogType;
    UShort              sLogSize;

    IDE_ASSERT( aUndoRecHdr != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_ADD_FIRST_COLUMN_PIECE;

    sSlotDirStartPtr =
        sdpPhyPage::getSlotDirStartPtr((UChar*)aUndoRecHdr);

    /* undo info layer�� ũ�⸦ ����. */
    sLogSize = sdcUndoSegment::getUndoRecSize(
                      sSlotDirStartPtr,
                      SD_MAKE_SLOTNUM( aUndoSID ) );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sUndoRecBodyPtr = (const UChar*)
        sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)sUndoRecBodyPtr,
                                  sLogSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::undo_DELETE_FIRST_COLUMN_PIECE(
    sdrMtx               *aMtx,
    UChar                *aLogPtr,
    UChar                *aSlotPtr,
    sdSID                 aSlotSID,
    sdcOperToMakeRowVer   aOper4RowPiece,
    UChar                *aRowBuf4MVCC,
    UChar               **aNewSlotPtr4Undo )
{
    UChar             *sWritePtr;
    UChar              sOldSlotImage[SD_PAGE_SIZE];
    UChar             *sOldSlotImagePtr;
    UChar             *sOldSlotPtr      = aSlotPtr;
    UChar             *sNewSlotPtr;
    UChar             *sLogPtr          = (UChar*)aLogPtr;
    UShort             sOldRowPieceSize;
    UShort             sNewRowPieceSize;
    UChar              sFlag;
    SShort             sChangeSize;
    UShort             sColCount;
    UShort             sColSeq;
    UChar              sRowFlag;
    UInt               sLength;
    SShort             sFSCreditSize;
    idBool             sReserveFreeSpaceCredit;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );

    sOldRowPieceSize = getRowPieceSize(sOldSlotPtr);
    SDC_GET_ROWHDR_FIELD(sOldSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);
    SDC_GET_ROWHDR_1B_FIELD(sOldSlotPtr, SDC_ROWHDR_FLAG,   sRowFlag);

    /* reallocSlot()�� �ϴ� ���߿�
     * compact page ������ �߻��� �� �ֱ� ������
     * old slot image�� temp buffer�� �����ؾ� �Ѵ�. */
    sOldSlotImagePtr = sOldSlotImage;
    idlOS::memcpy( sOldSlotImagePtr,
                   sOldSlotPtr,
                   sOldRowPieceSize );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );

    ID_READ_AND_MOVE_PTR( sLogPtr,
                          &sChangeSize,
                          ID_SIZEOF(sChangeSize) );

    if ( aOper4RowPiece == SDC_MVCC_MAKE_VALROW )
    {
        /* MVCC�� ���� undo�� �����ϴ� ���,
         * reallocSlot()�� �ϸ� �ȵǰ�,
         * ���ڷ� �Ѿ�� temp buffer pointer�� ����ؾ� �Ѵ�. */
        IDE_ERROR( aRowBuf4MVCC != NULL );
        sNewSlotPtr = aRowBuf4MVCC;
    }
    else
    {
        sNewRowPieceSize = sOldRowPieceSize + (sChangeSize);

        if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
        {
            sFSCreditSize = sdcRow::calcFSCreditSize(
                                        sNewRowPieceSize,
                                        sOldRowPieceSize );

            IDE_TEST( sdcTableCTL::unbind(
                          aMtx,
                          sOldSlotPtr,         /* Undo�Ǳ��� RowPiecePtr */
                          *(UChar*)sOldSlotPtr,/* aCTSlotIdxBfrUndo */
                          *(UChar*)sLogPtr,    /* aCTSlotIdxAftUndo */
                          sFSCreditSize,
                          ID_FALSE )           /* aDecDeleteRowCnt */
                      != IDE_SUCCESS );
        }

        /*
         * ###   FSC �÷���   ###
         *
         * DML �����߿��� �翬�� FSC�� reserve �ؾ� �Ѵ�.
         * �׷��� redo�� undo�ÿ��� ��� �ؾ� �ϳ�?
         *
         * redo�� DML ������ �ٽ� �����ϴ� ���̹Ƿ�,
         * DML �����Ҷ��� �����ϰ� FSC�� reserve �ؾ� �Ѵ�.
         *
         * �ݸ� undo�ÿ��� FSC�� reserve�ϸ� �ȵȴ�.
         * �ֳ��ϸ� FSC�� DML ������ undo��ų���� ����ؼ�
         * ������ �����صδ� ���̹Ƿ�,
         * undo�ÿ��� ������ reserve�ص� FSC��
         * �������� �ǵ���(restore)�־�� �ϰ�,
         * undo�ÿ� �� �ٽ� FSC�� reserve�Ϸ��� �ؼ��� �ȵȴ�.
         *
         * clr�� undo�� ���� redo�̹Ƿ� undo���� �����ϰ�
         * FSC�� reserve�ϸ� �ȵȴ�.
         *
         * �� ������ ��츦 �����Ͽ�
         * FSC reserve ó���� �ؾ� �ϴµ�,
         * ��(upinel9)�� �α׸� ����Ҷ� FSC reserve ���θ� �÷��׷� ���ܼ�,
         * redo�� undo�ÿ��� �� �÷��׸� ����
         * reallocSlot()�� �ϵ��� �����Ͽ���.
         *
         * redo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE
         * undo     : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         * clr redo : SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_FALSE
         */
        if ( (sFlag & SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_MASK) ==
             SDC_UPDATE_LOG_FLAG_RESERVE_FREESPACE_CREDIT_TRUE )
        {
            sReserveFreeSpaceCredit = ID_TRUE;
        }
        else
        {
            sReserveFreeSpaceCredit = ID_FALSE;
        }

        IDE_TEST( reallocSlot( aSlotSID,
                               sOldSlotPtr,
                               sNewRowPieceSize,
                               sReserveFreeSpaceCredit,
                               &sNewSlotPtr )
                  != IDE_SUCCESS );
         sOldSlotPtr = NULL;

        *aNewSlotPtr4Undo = sNewSlotPtr;
    }

    IDE_ERROR( sNewSlotPtr != NULL );


    idlOS::memcpy( sNewSlotPtr,
                   sLogPtr,
                   SDC_ROWHDR_SIZE );

    sWritePtr = sNewSlotPtr;

    SDC_MOVE_PTR_TRIPLE( sWritePtr,
                         sLogPtr,
                         sOldSlotImagePtr,
                         SDC_ROWHDR_SIZE );

    if ( SDC_IS_LAST_ROWPIECE(sRowFlag) != ID_TRUE )
    {
        ID_WRITE_AND_MOVE_BOTH( sWritePtr,
                                sOldSlotImagePtr,
                                SDC_EXTRASIZE_FOR_CHAINING );
    }

    // add first column piece
    {
        sLength = getColPieceLen(sLogPtr);

        ID_WRITE_AND_MOVE_BOTH( sWritePtr,
                                sLogPtr,
                                SDC_GET_COLPIECE_SIZE(sLength) );
    }

    for ( sColSeq = 0 ; sColSeq < sColCount ; sColSeq++ )
    {
        sLength = getColPieceLen(sOldSlotImagePtr);

        ID_WRITE_AND_MOVE_BOTH( sWritePtr,
                                sOldSlotImagePtr,
                                SDC_GET_COLPIECE_SIZE(sLength) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeUpdateRowPieceLog4RP(
                       const sdcRowPieceUpdateInfo *aUpdateInfo,
                       idBool                       aIsUndoRec,
                       sdrMtx                      *aMtx )
{
    const smiColumn              *sColumn;
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UShort                        sUptInModeColCnt;
    UShort                        sWriteCount = 0;
    UInt                          sColumnId;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT_MSG( aUpdateInfo->mOldRowHdrInfo->mColCount ==
                    aUpdateInfo->mNewRowHdrInfo->mColCount,
                    "Old Row Column Count : %"ID_UINT32_FMT"\n"
                    "New Row Column Count : %"ID_UINT32_FMT"\n",
                    aUpdateInfo->mOldRowHdrInfo->mColCount,
                    aUpdateInfo->mNewRowHdrInfo->mColCount );
    IDE_ASSERT_MSG( ( aUpdateInfo->mUptAftInModeColCnt + aUpdateInfo->mUptAftLobDescCnt ) ==
                    ( aUpdateInfo->mUptBfrInModeColCnt + aUpdateInfo->mUptBfrLobDescCnt ),
                    "After In Mode Column Count  : %"ID_UINT32_FMT"\n"
                    "After Update LobDesc Count  : %"ID_UINT32_FMT"\n"
                    "Before In Mode Column Count : %"ID_UINT32_FMT"\n"
                    "Before Update LobDesc Count : %"ID_UINT32_FMT"\n",
                    aUpdateInfo->mUptAftInModeColCnt,
                    aUpdateInfo->mUptAftLobDescCnt,
                    aUpdateInfo->mUptBfrInModeColCnt,
                    aUpdateInfo->mUptBfrLobDescCnt );

    sColCount = aUpdateInfo->mNewRowHdrInfo->mColCount;
    
    // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
    //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
    if ( aIsUndoRec == ID_TRUE )
    {
        // ������ In Mode���� Update Column�� ��
        sUptInModeColCnt = aUpdateInfo->mUptBfrInModeColCnt;
    }
    else
    {
        // In Mode�� Update�Ϸ��� Column�� ��
        sUptInModeColCnt = aUpdateInfo->mUptAftInModeColCnt;
    }
    
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&(sUptInModeColCnt),
                                  ID_SIZEOF(sUptInModeColCnt))
              != IDE_SUCCESS );

    sWriteCount = 0;
    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
        //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
        // In Mode Ȯ�� �� Undo Rec Log �̸� Old Value �� Ȯ���ϵ��� ����
        if ( SDC_IS_IN_MODE_UPDATE_COLUMN( sColumnInfo,
                                           aIsUndoRec ) == ID_TRUE )
        {
            sColumn   = sColumnInfo->mColumn;
            sColumnId = (UInt)sColumn->id;

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColSeqInRowPiece,
                                          ID_SIZEOF(sColSeqInRowPiece) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnId,
                                          ID_SIZEOF(sColumnId) )
                      != IDE_SUCCESS );

            if ( aIsUndoRec != ID_TRUE )
            {
                IDE_TEST( sdrMiniTrans::write(aMtx,
                                              (void*)&sColumnInfo->mNewValueInfo.mValue.length,
                                              ID_SIZEOF(UInt) )
                          != IDE_SUCCESS );
            }
            sWriteCount++;
        }
    }

    // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
    //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
    // �α׸� ��� ��� �Ͽ����� Ȯ��
    IDE_ASSERT_MSG( sUptInModeColCnt == sWriteCount,
                    "UptInModeColCount: %"ID_UINT32_FMT", "
                    "sWriteCount: %"ID_UINT32_FMT", "
                    "ColSeqInRowPiece: %"ID_UINT32_FMT", "
                    "ColCount: %"ID_UINT32_FMT"\n",
                    sUptInModeColCnt,
                    sWriteCount,
                    sColSeqInRowPiece,
                    sColCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeOverwriteRowPieceLog4RP(
                       const sdcRowPieceOverwriteInfo *aOverwriteInfo,
                       idBool                          aIsUndoRec,
                       sdrMtx                         *aMtx )
{
    const smiColumn              *sColumn;
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UShort                        sColSeqInTrailingNull;
    UShort                        sUptInModeColCount = 0;
    UShort                        sTrailingNullUptCount;
    UShort                        sWriteCount = 0;
    UInt                          sColumnId;

    IDE_ASSERT( aOverwriteInfo != NULL );
    IDE_ASSERT( aMtx           != NULL );

    if ( aIsUndoRec == ID_TRUE )
    {
        sColCount          = aOverwriteInfo->mOldRowHdrInfo->mColCount;
        sUptInModeColCount = aOverwriteInfo->mUptInModeColCntBfrSplit;
    }
    else
    {
        sColCount          = aOverwriteInfo->mNewRowHdrInfo->mColCount;
        sUptInModeColCount = aOverwriteInfo->mUptAftInModeColCnt;
    }

    /* write log */
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&(sUptInModeColCount),
                                  ID_SIZEOF(sUptInModeColCount))
              != IDE_SUCCESS );

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aOverwriteInfo->mColInfoList + sColSeqInRowPiece;

        // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
        //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
        // In Mode Ȯ�� �� Undo Rec Log �̸� Old Value �� Ȯ���ϵ��� ����
        if ( SDC_IS_IN_MODE_UPDATE_COLUMN( sColumnInfo,
                                           aIsUndoRec ) == ID_TRUE )
        {
            sColumn   = sColumnInfo->mColumn;

            sColumnId = (UInt)sColumn->id;

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColSeqInRowPiece,
                                          ID_SIZEOF(sColSeqInRowPiece) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnId,
                                          ID_SIZEOF(sColumnId) )
                      != IDE_SUCCESS );

            if ( aIsUndoRec != ID_TRUE )
            {
                IDE_TEST( sdrMiniTrans::write(aMtx,
                                              (void*)&sColumnInfo->mNewValueInfo.mValue.length,
                                              ID_SIZEOF(UInt) )
                          != IDE_SUCCESS );
            }

            sWriteCount++;
        }
    }

    if ( aIsUndoRec == ID_TRUE )
    {
        /* trailing null�� update�� ���,
         * undo log�� �߰����� ������ ����Ͽ�
         * replication�� �̸� �ľ��� �� �ֵ��� �Ѵ�.
         * trailing null update �÷��� ������ŭ
         * column seq ������ ID_USHORT_MAX, column id ������ ID_UINT_MAX�� ����ϱ��
         * RP���� �����Ͽ���. */
        sColSeqInTrailingNull = (UShort)ID_USHORT_MAX;
        sColumnId             = (UInt)ID_UINT_MAX;
        sTrailingNullUptCount = aOverwriteInfo->mTrailingNullUptCount;

        while( sTrailingNullUptCount != 0 )
        {
            sColumnInfo = aOverwriteInfo->mColInfoList + sColSeqInRowPiece;

            // BUG-31134 Insert Undo Record�� �߰��Ǵ� RP Info��
            //           Before Image�� �������� �ۼ��Ǿ�� �մϴ�.
            // Trailing Null Column�� ��� In Mode Update Column�̴�.
            IDE_ASSERT( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mOldValueInfo ) == ID_TRUE );
            IDE_ASSERT( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColSeqInTrailingNull,
                                          ID_SIZEOF(sColSeqInTrailingNull) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnId,
                                          ID_SIZEOF(sColumnId) )
                      != IDE_SUCCESS );

            sWriteCount++;

            sColSeqInRowPiece++;

            sTrailingNullUptCount--;
        }
    }

    IDE_ASSERT_MSG( sUptInModeColCount == sWriteCount,
                    "UptInModeColCount: %"ID_UINT32_FMT", "
                    "sWriteCount: %"ID_UINT32_FMT", "
                    "ColSeqInRowPiece: %"ID_UINT32_FMT", "
                    "ColCount: %"ID_UINT32_FMT"\n",
                    sUptInModeColCount,
                    sWriteCount,
                    sColSeqInRowPiece,
                    sColCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [column seq(2),column id(4)]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteFstColumnPieceLog4RP(
    const sdcRowPieceUpdateInfo *aUpdateInfo,
    sdrMtx                      *aMtx )
{
    const smiColumn              *sFstColumn;
    const sdcColumnInfo4Update   *sFstColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UInt                          sColumnId;

    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aMtx        != NULL );

    sColCount = 1;
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sColCount,
                                  ID_SIZEOF(sColCount) )
              != IDE_SUCCESS );

    sColSeqInRowPiece = 0;
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sColSeqInRowPiece,
                                  ID_SIZEOF(sColSeqInRowPiece) )
              != IDE_SUCCESS );

    sFstColumnInfo = aUpdateInfo->mColInfoList;
    sFstColumn     = sFstColumnInfo->mColumn;
    sColumnId      = (UInt)sFstColumn->id;

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&sColumnId,
                                  ID_SIZEOF(sColumnId) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteRowPieceLog4RP(
    void                           *aTableHeader,
    const UChar                    *aSlotPtr,
    const sdcRowPieceUpdateInfo    *aUpdateInfo,
    sdrMtx                         *aMtx )
{
    const smiColumn              *sColumn;
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount;
    UShort                        sColSeqInRowPiece;
    UShort                        sTrailingNullUptCount;
    UInt                          sColumnId;
    UShort                        sDoWriteCount;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aSlotPtr     != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );
    IDE_ASSERT( aMtx         != NULL );

    sColCount = aUpdateInfo->mOldRowHdrInfo->mColCount;

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)&(aUpdateInfo->mUptBfrInModeColCnt),
                                  ID_SIZEOF(aUpdateInfo->mUptBfrInModeColCnt) )
              != IDE_SUCCESS );

    sDoWriteCount = 0;

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_IN_MODE_UPDATE_COLUMN( sColumnInfo,
                                           ID_TRUE /* aIsUndoRec */ )
             == ID_TRUE )
        {
            sColumn   = sColumnInfo->mColumn;

            sColumnId = (UInt)sColumn->id;

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColSeqInRowPiece,
                                          ID_SIZEOF(sColSeqInRowPiece) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnId,
                                          ID_SIZEOF(sColumnId) )
                      != IDE_SUCCESS );

            sDoWriteCount++;
        }
    }

    sTrailingNullUptCount = aUpdateInfo->mTrailingNullUptCount;
    sColumnId             = (UInt)ID_UINT_MAX;
    sColSeqInRowPiece     = (UShort)ID_USHORT_MAX;

    while( sTrailingNullUptCount != 0 )
    {
        /* trailing null�� update�� ���,
         * undo log�� �߰����� ������ ����Ͽ�
         * replication�� �̸� �ľ��� �� �ֵ��� �Ѵ�.
         * trailing null update �÷��� ������ŭ
         * column seq ������ ID_USHORT_MAX, column id ������ ID_UINT_MAX�� ����ϱ��
         * RP���� �����Ͽ���. */


        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sColSeqInRowPiece,
                                      ID_SIZEOF(sColSeqInRowPiece) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sColumnId,
                                      ID_SIZEOF(sColumnId) )
                  != IDE_SUCCESS );

        sDoWriteCount++;

        sTrailingNullUptCount--;
    }

    IDE_ASSERT_MSG( aUpdateInfo->mUptBfrInModeColCnt == sDoWriteCount,
                    "TableOID            : %"ID_vULONG_FMT"\n"
                    "TrailingNullUptCount: %"ID_UINT32_FMT"\n"
                    "UptInModeColCnt     : %"ID_UINT32_FMT"\n"
                    "DoWriteCount        : %"ID_UINT32_FMT"\n"
                    "ColCount            : %"ID_UINT32_FMT"\n",
                    smLayerCallback::getTableOID( aTableHeader ),
                    aUpdateInfo->mTrailingNullUptCount,
                    aUpdateInfo->mUptAftInModeColCnt,
                    sDoWriteCount,
                    sColCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   -----------------------------------------------------------------------------------
 *   | [column count(2)], [ {column seq(2),column id(4), column total len(4)} ... ]
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeInsertRowPieceLog4RP( const sdcRowPieceInsertInfo *aInsertInfo,
                                          sdrMtx                      *aMtx )
{
    const smiColumn              *sColumn;
    const sdcColumnInfo4Insert   *sColumnInfo;
    UShort                        sInModeColCnt;
    UShort                        sUptInModeColCnt;
    UShort                        sCurrColSeq;
    UInt                          sColumnId;
    idBool                        sDoWrite;
    UShort                        sColSeqInRowPiece = 0;

    IDE_ASSERT( aInsertInfo != NULL );
    IDE_ASSERT( aMtx        != NULL );

    if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
    {
        /* insert�� ���,
         * ��� �÷��� ������ ����Ѵ�.(lob Desc ����) */
        sInModeColCnt =
            aInsertInfo->mColCount - aInsertInfo->mLobDescCnt;
        
        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sInModeColCnt,
                                      ID_SIZEOF(sInModeColCnt) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* update�� ���,
         * update�� �÷��� �������� ����Ѵ�.(LOB Desc ����)
         * replication�� update �÷��� �������� �����ؾ� �ϱ� �����̴�. */
        sUptInModeColCnt = 0;

        for ( sCurrColSeq = aInsertInfo->mStartColSeq;
              sCurrColSeq <= aInsertInfo->mEndColSeq;
              sCurrColSeq++ )
        {
            sColumnInfo = aInsertInfo->mColInfoList + sCurrColSeq;

            if ( (sColumnInfo->mIsUptCol == ID_TRUE) &&
                 (SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) == ID_TRUE) )            
            {
                sUptInModeColCnt++;
            }
        }

        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sUptInModeColCnt,
                                      ID_SIZEOF(sUptInModeColCnt) )
                  != IDE_SUCCESS );
    }

    for ( sCurrColSeq = aInsertInfo->mStartColSeq, sColSeqInRowPiece = 0;
          sCurrColSeq <= aInsertInfo->mEndColSeq;
          sCurrColSeq++, sColSeqInRowPiece++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sCurrColSeq;

        sDoWrite = ID_FALSE;

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) == ID_TRUE )
        {
            if ( aInsertInfo->mIsInsert4Upt != ID_TRUE )
            {
                sDoWrite = ID_TRUE;
            }
            else
            {
                if ( sColumnInfo->mIsUptCol == ID_TRUE )
                {
                    sDoWrite = ID_TRUE;
                }
            }
        }
        
        if ( sDoWrite == ID_TRUE )
        {
            sColumn   = sColumnInfo->mColumn;

            sColumnId = (UInt)sColumn->id;

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColSeqInRowPiece,
                                          ID_SIZEOF(sColSeqInRowPiece) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnId,
                                          ID_SIZEOF(sColumnId) )
                      != IDE_SUCCESS );

            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sColumnInfo->mValueInfo.mValue.length,
                                          ID_SIZEOF(UInt) )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::remove( idvSQL                * aStatistics,
                       void                  * aTrans,
                       smSCN                   aStmtViewSCN,
                       void                  * aTableInfoPtr,
                       void                  * aTableHeader,
                       SChar                 * /* aRow */,
                       scGRID                  aSlotGRID,
                       smSCN                   aCSInfiniteSCN,
                       const smiDMLRetryInfo * aDMLRetryInfo,
                       idBool                  /*aIsDequeue*/,
                       idBool                  aForbiddenToRetry )
{
    sdSID          sCurrRowPieceSID;
    sdSID          sNextRowPieceSID;
    sdcPKInfo      sPKInfo;
    idBool         sReplicate;
    idBool         sSkipFlag;
    UShort         sFstColumnSeq = 0;
    sdcRetryInfo   sRetryInfo = {NULL, ID_FALSE, NULL, {NULL,NULL,0},{NULL,NULL,0},0};

    IDE_ERROR( aTrans              != NULL );
    IDE_ERROR( aTableInfoPtr       != NULL );
    IDE_ERROR( aTableHeader        != NULL );
    IDE_ERROR( SC_GRID_IS_NOT_NULL(aSlotGRID));

    /* FIT/ART/sm/Projects/PROJ-2118/delete/delete.ts */
    IDU_FIT_POINT( "1.PROJ-2118@sdcRow::remove" );

    sReplicate = smLayerCallback::needReplicate( (smcTableHeader*)aTableHeader,
                                                 aTrans );

    setRetryInfo( aDMLRetryInfo,
                  &sRetryInfo );

    if ( sReplicate == ID_TRUE )
    {
        if ( smcTable::isTransWaitReplicationTable( (const smcTableHeader*)aTableHeader )
             == ID_TRUE )
        {
            smLayerCallback::setIsTransWaitRepl( aTrans, ID_TRUE );
        }
        else
        {
            /* do nothing */
        }

        /* replication�� �ɸ� ���
         * pk log�� ����ؾ� �ϹǷ�
         * sdcPKInfo �ڷᱸ���� �ʱ�ȭ�Ѵ�. */
        makePKInfo( aTableHeader, &sPKInfo );
    }

    sCurrRowPieceSID = SD_MAKE_SID_FROM_GRID( aSlotGRID );

    while(1)
    {
        IDE_TEST( removeRowPiece( aStatistics,
                                  aTrans,
                                  aTableHeader,
                                  sCurrRowPieceSID,
                                  &aStmtViewSCN,
                                  &aCSInfiniteSCN,
                                  &sRetryInfo,
                                  sReplicate,
                                  aForbiddenToRetry,
                                  &sPKInfo,
                                  &sFstColumnSeq,
                                  &sSkipFlag,
                                  &sNextRowPieceSID)
                  != IDE_SUCCESS );

        IDE_TEST_CONT( sSkipFlag == ID_TRUE, SKIP_REMOVE );

        if ( sNextRowPieceSID == SD_NULL_SID )
        {
            break;
        }

        sCurrRowPieceSID = sNextRowPieceSID;
    }

    if (aTableInfoPtr != NULL)
    {
        smLayerCallback::decRecCntOfTableInfo( aTableInfoPtr );
    }

    IDE_ASSERT( sRetryInfo.mISavepoint == NULL );

    if ( sReplicate == ID_TRUE)
    {
        IDE_ASSERT_MSG( sPKInfo.mTotalPKColCount == sPKInfo.mCopyDonePKColCount,
                        "TableOID           : %"ID_vULONG_FMT"\n"
                        "TotalPKColCount    : %"ID_UINT32_FMT"\n"
                        "CopyDonePKColCount : %"ID_UINT32_FMT"\n",
                        smLayerCallback::getTableOID( aTableHeader ),
                        sPKInfo.mTotalPKColCount,
                        sPKInfo.mCopyDonePKColCount );

        /* replication�� �ɸ� ��� pk log�� ����Ѵ�. */
        IDE_TEST( writePKLog( aStatistics,
                              aTrans,
                              aTableHeader,
                              aSlotGRID,
                              &sPKInfo )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT(SKIP_REMOVE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sRetryInfo.mISavepoint != NULL )
    {
        // ���� ��Ȳ���� savepoint�� ���� set�Ǿ� ���� �� �ִ�.
        IDE_ASSERT( smxTrans::unsetImpSavepoint4LayerCall( aTrans,
                                                           sRetryInfo.mISavepoint )
                    == IDE_SUCCESS );

        sRetryInfo.mISavepoint = NULL;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece���� �����ϴ� ��� lob column�� ���� removeLob�� �Ѵ�.
 *  �׸��� rowpiece���� body�� �����ϰ� header�� �����.
 *  ��Ƽ���̽��� record base mvcc�̱� ������ row�� delete�Ǵ���
 *  aging�Ǳ� ������ row header�� ���ܵξ�� �Ѵ�.
 *
 *  aStatistics         - [IN] �������
 *  aTrans              - [IN] Ʈ����� ����
 *  aTableHeader        - [IN] ���̺� ���
 *  aSlotSID            - [IN] slot sid
 *  aStmtViewSCN        - [IN] stmt scn
 *  aCSInfiniteSCN      - [IN] cursor infinite scn
 *  aRetryInfo          - [IN] retry ���θ� �Ǵ��ϱ����� ����
 *  aReplicate          - [IN] replication ����
 *  aForbiddenToRetry   - [IN] retry error �ø��°� ����, abort �߻�
 *  aPKInfo             - [IN/OUT]
 *  aFstColumnSeq       - [IN/OUT] rowpiece���� ù��° column piece��
 *                                 global sequence number.
 *  aSkipFlag           - [OUT]
 *  aNextRowPieceSID    - [OUT] next rowpiece sid
 *
 **********************************************************************/
IDE_RC sdcRow::removeRowPiece( idvSQL                *aStatistics,
                               void                  *aTrans,
                               void                  *aTableHeader,
                               sdSID                  aSlotSID,
                               smSCN                 *aStmtViewSCN,
                               smSCN                 *aCSInfiniteSCN,
                               sdcRetryInfo          *aRetryInfo,
                               idBool                 aReplicate,
                               idBool                 aForbiddenToRetry,
                               sdcPKInfo             *aPKInfo,
                               UShort                *aFstColumnSeq,
                               idBool                *aSkipFlag,
                               sdSID                 *aNextRowPieceSID )
{
    sdrMtx                   sMtx;
    UInt                     sDLogAttr        = SM_DLOG_ATTR_DEFAULT;
    sdrSavePoint             sSP;
    scSpaceID                sTableSpaceID;
    sdrMtxStartInfo          sStartInfo;
    UChar                  * sDeleteSlot;
    scGRID                   sSlotGRID;
    sdSID                    sUndoSID         = SD_NULL_SID;
    sdSID                    sNextRowPieceSID = SD_NULL_SID;
    smrContType              sContType        = SMR_CT_CONTINUE;
    idBool                   sSkipFlag        = ID_FALSE;
    sdcUpdateState           sRetFlag         = SDC_UPTSTATE_UPDATABLE;
    sdcRowHdrInfo            sOldRowHdrInfo;
    sdcRowHdrInfo            sNewRowHdrInfo;
    sdcRowPieceUpdateInfo    sUpdateInfo4PKLog;
    smSCN                    sFstDskViewSCN;
    UInt                     sState           = 0;
    UChar                    sNewCTSlotIdx;
    smSCN                    sInfiniteAndDelBit;
    UShort                   sOldRowPieceSize;
    UShort                   sNewRowPieceSize;
    SShort                   sFSCreditSize    = 0;
    SShort                   sChangeSize      = 0;
    UChar                  * sNewDeleteSlot;
    sdpPageListEntry       * sEntry;
    UInt                     sLobColCountInTable;
    sdcLobInfoInRowPiece     sLobInfo;
    UChar                    sNewRowFlag;
    sdcUndoSegment         * sUDSegPtr;

    IDE_ASSERT( aTrans              != NULL );
    IDE_ASSERT( aTableHeader        != NULL );
    IDE_ASSERT( aPKInfo             != NULL );
    IDE_ASSERT( aSkipFlag           != NULL );
    IDE_ASSERT( aFstColumnSeq       != NULL );
    IDE_ASSERT( aNextRowPieceSID    != NULL );
    IDE_ASSERT( aSlotSID            != SD_NULL_SID );

    sTableSpaceID       = smLayerCallback::getTableSpaceID( aTableHeader );
    sLobColCountInTable = smLayerCallback::getLobColumnCount( aTableHeader );

    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(aSlotSID),
                               SD_MAKE_SLOTNUM(aSlotSID) );

    sDLogAttr = ( aReplicate == ID_TRUE ?
                  SM_DLOG_ATTR_REPLICATE : SM_DLOG_ATTR_NORMAL );
    sDLogAttr |= ( SM_DLOG_ATTR_DML | SM_DLOG_ATTR_UNDOABLE );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   sDLogAttr )
              != IDE_SUCCESS );
    sState = 1;

    sdrMiniTrans::makeStartInfo( &sMtx, &sStartInfo );

    IDE_TEST( smxTrans::allocTXSegEntry( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    sdrMiniTrans::setSavePoint( &sMtx, &sSP );

    IDE_TEST( prepareUpdatePageBySID( aStatistics,
                                      &sMtx,
                                      sTableSpaceID,
                                      aSlotSID,
                                      SDB_SINGLE_PAGE_READ,
                                      &sDeleteSlot,
                                      &sNewCTSlotIdx )
              != IDE_SUCCESS );

    getRowHdrInfo( sDeleteSlot, &sOldRowHdrInfo );

    if ( SDC_IS_HEAD_ROWPIECE(sOldRowHdrInfo.mRowFlag) == ID_TRUE )
    {
        /* head rowpiece�� ���,
         * remove ������ ������ �� �ִ��� �˻��ؾ� �Ѵ�. */
        IDE_TEST( canUpdateRowPiece( aStatistics,
                                     &sMtx,
                                     &sSP,
                                     sTableSpaceID,
                                     aSlotSID,
                                     SDB_SINGLE_PAGE_READ,
                                     aStmtViewSCN,
                                     aCSInfiniteSCN,
                                     ID_FALSE, /* aIsUptLobByAPI */
                                     &sDeleteSlot,
                                     &sRetFlag,
                                     &sNewCTSlotIdx,
                                     /* BUG-31359
                                      * Wait���� ������ ������ ����
                                      * ID_ULONG_MAX ��ŭ ��ٸ��� �Ѵ�. */
                                     ID_ULONG_MAX )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdcTableCTL::checkAndMakeSureRowStamping( aStatistics,
                                                            &sStartInfo,
                                                            sDeleteSlot,
                                                            SDB_SINGLE_PAGE_READ )
                  != IDE_SUCCESS );
    }

    // Row Stamping�� ����Ǿ����� ������ �Ŀ� �ٽ� RowHdrInfo�� ��´�.
    getRowHdrInfo( sDeleteSlot, &sOldRowHdrInfo );

    /* BUG-30911 - [5.3.7] 2 rows can be selected during executing index scan
     *             on unique index. 
     * smmDatabase::getCommitSCN()���� Trans�� CommitSCN�� LstSystemSCN����
     * ���߿� �����ǵ��� �����Ͽ���.
     * ���� T1�� row A�� �����ϰ� commit �Ͽ����� SystemSCN�� ����������
     * T1 Trans�� CommitSCN�� �������� ���� �� �ְ�,
     * �̶� �� �ٸ� Trans T2�� ���� row A�� �����ϰ��� �� ��, T1�� �����ߴ�
     * Index Key�� visible ���·� �Ǵ��Ͽ� row A�� �����Ϸ� �� �Լ��� ������
     * �ȴ�.
     * ���� T2�� row stamping�� �����ϱ� ���� T1�� CommitSCN�� �����ϰ� �Ǹ�
     * ���⼭ �ش� Row�� Deleted �÷��װ� �����Ȱ��� ���Եȴ�. */
    if ( SM_SCN_IS_DELETED(sOldRowHdrInfo.mInfiniteSCN) == ID_TRUE )
    {
        /* ���� ���� ����ϰ� releaseLatch */
        IDE_RAISE( rebuild_already_modified );
    }

    if ( ( sRetFlag == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED ) ||
         ( aRetryInfo->mIsAlreadyModified == ID_TRUE ) )
    {
        if ( aRetryInfo->mRetryInfo != NULL )
        {
            IDE_TEST( checkRetry( aTrans,
                                  &sMtx,
                                  &sSP,
                                  sDeleteSlot,
                                  sOldRowHdrInfo.mRowFlag,
                                  aForbiddenToRetry,
                                  *aStmtViewSCN,
                                  aRetryInfo,
                                  sOldRowHdrInfo.mColCount )
                      != IDE_SUCCESS );
        }
        else
        {
            /* ���� ���� ����ϰ� releaseLatch */
            IDE_RAISE( rebuild_already_modified );
        }
    }

    sNextRowPieceSID = getNextRowPieceSID(sDeleteSlot);

    if ( aReplicate == ID_TRUE )
    {
        IDE_ASSERT( aPKInfo != NULL );

        IDE_TEST( makeUpdateInfo( sDeleteSlot,
                                  NULL, // aColList
                                  NULL, // aValueList
                                  aSlotSID,
                                  sOldRowHdrInfo.mColCount,
                                  *aFstColumnSeq,
                                  NULL, // aLobInfo4Update
                                  &sUpdateInfo4PKLog )
                  != IDE_SUCCESS );

        /* TASK-5030 */
        if ( smcTable::isSupplementalTable((smcTableHeader *)aTableHeader) == ID_TRUE )
        {
            IDE_ASSERT( aPKInfo != NULL );

            sUpdateInfo4PKLog.mOldRowHdrInfo = &sOldRowHdrInfo;

            IDE_TEST( sdcRow::analyzeRowForDelete4RP( aTableHeader,
                                                      sOldRowHdrInfo.mColCount,
                                                      *aFstColumnSeq,
                                                      &sUpdateInfo4PKLog )
                      != IDE_SUCCESS );
        }

        if ( aPKInfo->mTotalPKColCount !=
             aPKInfo->mCopyDonePKColCount )
        {
            copyPKInfo( sDeleteSlot,
                        &sUpdateInfo4PKLog,
                        sOldRowHdrInfo.mColCount,
                        aPKInfo );
        }
    }

    /* BUG-24331
     * [5.3.1] lob column �� ���� row�� delete�Ǿ
     * lob�� ���� free�� �������� ����. */
    if ( sLobColCountInTable > 0 )
    {
        IDE_TEST( analyzeRowForLobRemove( aTableHeader,
                                          sDeleteSlot,
                                          *aFstColumnSeq,
                                          sOldRowHdrInfo.mColCount,
                                          &sLobInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        sLobInfo.mLobDescCnt = 0;
    }
    
    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aTrans );
    /* TASK-5030 */
    if ( smcTable::isSupplementalTable((smcTableHeader *)aTableHeader) == ID_TRUE )
    {
        if ( sUDSegPtr->addDeleteRowPieceUndoRec(
                                        aStatistics,
                                        &sStartInfo,
                                        smLayerCallback::getTableOID(aTableHeader),
                                        sDeleteSlot,
                                        sSlotGRID,
                                        ID_FALSE, /* aIsDelete4Upt */
                                        aReplicate,
                                        &sUpdateInfo4PKLog, /* aUpdateInfo */
                                        &sUndoSID ) != IDE_SUCCESS )
        {
            /* Undo ������������ ������ ���. MtxUndo �����ص� �� */
            sdrMiniTrans::setIgnoreMtxUndo( &sMtx );
            IDE_TEST( 1 );
        }
    }
    else
    {
        if ( sUDSegPtr->addDeleteRowPieceUndoRec(
                                        aStatistics,
                                        &sStartInfo,
                                        smLayerCallback::getTableOID(aTableHeader),
                                        sDeleteSlot,
                                        sSlotGRID,
                                        ID_FALSE, /* aIsDelete4Upt */
                                        aReplicate,
                                        NULL,     /* aUpdateInfo */
                                        &sUndoSID ) != IDE_SUCCESS )
        {
            /* Undo ������������ ������ ���. MtxUndo �����ص� �� */
            sdrMiniTrans::setIgnoreMtxUndo( &sMtx );
            IDE_TEST( 1 );
        }
    }

    sOldRowPieceSize = getRowPieceSize( sDeleteSlot );
    sNewRowPieceSize = SDC_ROWHDR_SIZE;

    sChangeSize = sOldRowPieceSize - sNewRowPieceSize;

    if ( sChangeSize < 0 )
    {
        ideLog::log( IDE_ERR_0,
                     "OldRowPieceSize: %"ID_UINT32_FMT", "
                     "NewRowPieceSize: %"ID_UINT32_FMT"\n",
                     sOldRowPieceSize,
                     sNewRowPieceSize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sDeleteSlot,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    IDE_TEST( reallocSlot( aSlotSID,
                           sDeleteSlot,
                           sNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewDeleteSlot )
              != IDE_SUCCESS );
    sDeleteSlot = NULL;
    IDE_ASSERT( sNewDeleteSlot != NULL );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( sNewDeleteSlot ) )
                == smLayerCallback::getTableOID( aTableHeader ) );

    sEntry = (sdpPageListEntry*)smcTable::getDiskPageListEntry(aTableHeader);
    IDE_ASSERT( sEntry != NULL );

    /* BUG-25762 Disk Page���� Delete ������ �߻��Ҷ� Page �������
     *           �� ���� ���¸� �������� �ʴ� ��찡 ����.
     *
     * �ּ� Rowpiece Size ������ ��쿡�� Delete �ÿ���
     * Rowpiece ����� ���ܵα� ������ ���������� �����ϴ� ũ�Ⱑ ������
     * �ּ� Rowpice ��ŭ�� �����ϱ� ������ �������� �������ũ��� ������
     * ������, Delete ������ ������ Free�ϴ� �����̹Ƿ� ���Ŀ� Aging�� �ɼ�
     * �ֵ��� ������� ���¸� ����õ��Ͽ��� �Ѵ�.
     * ( ��������� Ž���ϴ� �ٸ� Ʈ������� ���� �ֵ��� �ؾ��Ѵ� )
     *
     * �������� ������� ���º��� ������ Delete�� RowPiece�� ����Ͽ�
     * Aging�� ������� ũ�⸦ ����ϱ� ������ Page�� ������� ���°� �����
     * �� �ִ�. */
    IDE_TEST( sdpPageList::updatePageState(
                                aStatistics,
                                sTableSpaceID,
                                sEntry,
                                sdpPhyPage::getPageStartPtr(sNewDeleteSlot),
                                &sMtx ) != IDE_SUCCESS );

    sFSCreditSize = calcFSCreditSize( sOldRowPieceSize,
                                      sNewRowPieceSize );
    IDE_ASSERT( sFSCreditSize >= 0 );

    SM_SET_SCN( &sInfiniteAndDelBit, aCSInfiniteSCN );
    SM_SET_SCN_DELETE_BIT( &sInfiniteAndDelBit );

    sNewRowFlag = sOldRowHdrInfo.mRowFlag;
    SM_SET_FLAG_ON( sNewRowFlag, SDC_ROWHDR_L_FLAG );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aTrans);

    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          sNewCTSlotIdx,
                          sInfiniteAndDelBit,
                          sUndoSID,
                          0, //mColCount
                          sNewRowFlag,
                          sFstDskViewSCN );

    writeRowHdr( sNewDeleteSlot, &sNewRowHdrInfo );

    sdrMiniTrans::setRefOffset( &sMtx,
                                smLayerCallback::getTableOID( aTableHeader ) );

    IDE_TEST( writeDeleteRowPieceRedoUndoLog( (UChar*)sNewDeleteSlot,
                                              sSlotGRID,
                                              ID_FALSE,
                                              -(sChangeSize),
                                              &sMtx )
              != IDE_SUCCESS);

    IDE_TEST( sdcTableCTL::bind( &sMtx,
                                 sTableSpaceID,
                                 sNewDeleteSlot,
                                 sOldRowHdrInfo.mCTSlotIdx,
                                 sNewCTSlotIdx,
                                 SD_MAKE_SLOTNUM(aSlotSID),
                                 sOldRowHdrInfo.mExInfo.mFSCredit,
                                 sFSCreditSize,
                                 ID_TRUE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx, sContType ) != IDE_SUCCESS );

    /* BUG-24331
     * [5.3.1] lob column �� ���� row�� delete�Ǿ
     * lob�� ���� free�� �������� ����. */
    if ( sLobInfo.mLobDescCnt > 0 )
    {
        IDE_TEST( removeAllLobCols( aStatistics,
                                    aTrans,
                                    &sLobInfo )
                  != IDE_SUCCESS );
    }

    /* reset aFstColumnSeq */
    if ( SM_IS_FLAG_ON( sOldRowHdrInfo.mRowFlag, SDC_ROWHDR_N_FLAG )
         == ID_TRUE )
    {
        *aFstColumnSeq += (sOldRowHdrInfo.mColCount - 1);
    }
    else
    {
        *aFstColumnSeq += sOldRowHdrInfo.mColCount;
    }

    *aNextRowPieceSID = sNextRowPieceSID;

    *aSkipFlag = sSkipFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION( rebuild_already_modified );
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE );

            SChar    sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            sdpCTS * sCTS;
            smSCN    sFSCNOrCSCN;
            UChar    sCTSlotIdx = sOldRowHdrInfo.mCTSlotIdx;
            sdcRowHdrExInfo sRowHdrExInfo;

            if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
            {
                sCTS = sdcTableCTL::getCTS( sdpPhyPage::getHdr(sDeleteSlot),sCTSlotIdx );
                SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );
            }
            else
            {
                getRowHdrExInfo( sDeleteSlot, &sRowHdrExInfo );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowHdrExInfo.mFSCNOrCSCN );
            }

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[RECORD VALIDATION(R)] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CSInfiniteSCN:%"ID_UINT64_FMT", "
                             "CTSlotIdx:%"ID_UINT32_FMT", "
                             "FSCNOrCSCN:%"ID_UINT64_FMT", "
                             "InfiniteSCN:%"ID_UINT64_FMT", "
                             "Deleted:%s ",
                             ((smcTableHeader *)aTableHeader)->mSpaceID,
                             ((smcTableHeader *)aTableHeader)->mSelfOID,
                             SM_SCN_TO_LONG( *aStmtViewSCN ),
                             SM_SCN_TO_LONG( *aCSInfiniteSCN ),
                             sCTSlotIdx,
                             SM_SCN_TO_LONG( sFSCNOrCSCN ),
                             SM_SCN_TO_LONG( sOldRowHdrInfo.mInfiniteSCN ),
                             SM_SCN_IS_DELETED( sOldRowHdrInfo.mInfiniteSCN)?"Y":"N" );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_RETRY_Already_Modified ) );
        }

        IDE_ASSERT( releaseLatchForAlreadyModify( &sMtx, &sSP )
                    == IDE_SUCCESS );
    }

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    if ( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( aTrans,
                                   IDV_STAT_INDEX_DELETE_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader*)aTableHeader,
                                  SMC_INC_RETRY_CNT_OF_DELETE );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  remove rowpiece for update ������ �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::removeRowPiece4Upt( idvSQL                         *aStatistics,
                                   void                           *aTableHeader,
                                   sdrMtx                         *aMtx,
                                   sdrMtxStartInfo                *aStartInfo,
                                   UChar                          *aDeleteSlot,
                                   const sdcRowPieceUpdateInfo    *aUpdateInfo,
                                   idBool                          aReplicate )
{
    UChar            *sDeleteSlot = aDeleteSlot;
    sdSID             sUndoSID    = SD_NULL_SID;
    sdcRowHdrInfo     sNewRowHdrInfo;
    scSpaceID         sTableSpaceID;
    smOID             sTableOID;
    scGRID            sSlotGRID;
    sdSID             sSlotSID;
    SShort            sFSCreditSize    = 0;
    SShort            sChangeSize      = 0;
    UChar            *sNewSlotPtr;
    sdpPageListEntry *sEntry;
    smSCN             sInfiniteAndDelBit;
    UChar             sNewRowFlag;
    UShort            sNewRowPieceSize;
    UShort            sOldRowPieceSize;
    smSCN             sFstDskViewSCN;
    sdcUndoSegment   *sUDSegPtr;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aDeleteSlot  != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );

    sTableSpaceID = smLayerCallback::getTableSpaceID( aTableHeader );
    sTableOID     = smLayerCallback::getTableOID( aTableHeader );
    sSlotSID      = aUpdateInfo->mRowPieceSID;

    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sTableSpaceID,
                               SD_MAKE_PID(sSlotSID),
                               SD_MAKE_SLOTNUM(sSlotSID) );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)aMtx->mTrans );
    if ( sUDSegPtr->addDeleteRowPieceUndoRec( aStatistics,
                                              aStartInfo,
                                              smLayerCallback::getTableOID(aTableHeader),
                                              sDeleteSlot,
                                              sSlotGRID,
                                              ID_TRUE,
                                              aReplicate,
                                              aUpdateInfo,
                                              &sUndoSID ) 
        != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� �� */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }


    sOldRowPieceSize = getRowPieceSize( sDeleteSlot );
    sNewRowPieceSize = SDC_ROWHDR_SIZE;

    /* ExtraSize CtsInfo�� �߰��Ǹ� ������ �ɼ� �ִ�. */
    sChangeSize = sOldRowPieceSize - sNewRowPieceSize;

    IDE_TEST( reallocSlot( sSlotSID,
                           sDeleteSlot,
                           sNewRowPieceSize,
                           ID_TRUE, /* aReserveFreeSpaceCredit */
                           &sNewSlotPtr )
              != IDE_SUCCESS );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( sNewSlotPtr ) )
                == smLayerCallback::getTableOID( aTableHeader ) );

    sDeleteSlot = NULL;
    IDE_ASSERT( sNewSlotPtr != NULL );

    sEntry = (sdpPageListEntry*)smcTable::getDiskPageListEntry(aTableHeader);
    IDE_ASSERT( sEntry != NULL );

    IDE_TEST( sdpPageList::updatePageState( aStatistics,
                                            sTableSpaceID,
                                            sEntry,
                                            sdpPhyPage::getPageStartPtr(sNewSlotPtr),
                                            aMtx ) != IDE_SUCCESS );

    sFSCreditSize = calcFSCreditSize( sOldRowPieceSize,
                                      sNewRowPieceSize );
    if ( sFSCreditSize < 0 )
    {
        ideLog::log( IDE_ERR_0,
                     "OldRowPieceSize: %"ID_UINT32_FMT", "
                     "NewRowPieceSize: %"ID_UINT32_FMT"\n",
                     sOldRowPieceSize,
                     sNewRowPieceSize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sDeleteSlot,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    SM_SET_SCN( &sInfiniteAndDelBit,
                &aUpdateInfo->mNewRowHdrInfo->mInfiniteSCN );
    SM_SET_SCN_DELETE_BIT( &sInfiniteAndDelBit );

    sNewRowFlag = aUpdateInfo->mOldRowHdrInfo->mRowFlag;
    SM_SET_FLAG_ON( sNewRowFlag, SDC_ROWHDR_L_FLAG );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aStartInfo->mTrans);

    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                          sInfiniteAndDelBit,
                          sUndoSID,
                          0, /* ColCount */
                          sNewRowFlag,
                          sFstDskViewSCN );

    writeRowHdr( sNewSlotPtr, &sNewRowHdrInfo );

    sdrMiniTrans::setRefOffset(aMtx, sTableOID);

    IDE_TEST( writeDeleteRowPieceRedoUndoLog( sNewSlotPtr,
                                              sSlotGRID,
                                              ID_TRUE,
                                              -(sChangeSize),
                                              aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdcTableCTL::bind( aMtx,
                                 sTableSpaceID,
                                 sNewSlotPtr,
                                 aUpdateInfo->mOldRowHdrInfo->mCTSlotIdx,
                                 aUpdateInfo->mNewRowHdrInfo->mCTSlotIdx,
                                 SD_MAKE_SLOTNUM(sSlotSID),
                                 aUpdateInfo->mOldRowHdrInfo->mExInfo.mFSCredit,
                                 sFSCreditSize,
                                 ID_TRUE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_DELETE_ROW_PIECE,
 *   SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [size(2)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeDeleteRowPieceRedoUndoLog( UChar           *aSlotPtr,
                                               scGRID           aSlotGRID,
                                               idBool           aIsDelete4Upt,
                                               SShort           aChangeSize,
                                               sdrMtx          *aMtx )
{
    sdrLogType    sLogType;
    UShort        sLogSize;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    if ( aIsDelete4Upt != ID_TRUE )
    {
        sLogType = SDR_SDC_DELETE_ROW_PIECE;
    }
    else
    {
        sLogType = SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE;
    }

    sLogSize = SDC_ROWHDR_SIZE + ID_SIZEOF(SShort);

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &aChangeSize,
                                  ID_SIZEOF(SShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  SDC_ROWHDR_SIZE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  replication�� �ɸ� ���,
 *  DML �α�(update or delete)�� ��� ����� �� PK Log�� ����Ѵ�.
 *  receiver���� DML�� ������ target row�� ã������ pk ������ �ʿ��ϱ� �����̴�.
 *
 *   SDR_SDC_PK_LOG
 *   -------------------------------------------------------------------
 *   | [sdrLogHdr],
 *   | [pk size(2)], [pk col count(2)],
 *   | [ {column id(4),column len(1 or 3), column data} ... ]
 *   -------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writePKLog( idvSQL             *aStatistics,
                           void               *aTrans,
                           void               *aTableHeader,
                           scGRID              aSlotGRID,
                           const sdcPKInfo    *aPKInfo )
{
    const sdcColumnInfo4PK   *sColumnInfo;
    UInt                      sColumnId;
    sdcColInOutMode           sInOutMode;

    sdrLogType    sLogType;
    UShort        sLogSize;
    sdrMtx        sMtx;
    UShort        sState = 0;
    UShort        sPKInfoSize;
    UShort        sPKColCount;
    UShort        sPKColSeq;

    IDE_ASSERT( aTrans       != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aPKInfo      != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   (SM_DLOG_ATTR_DEFAULT |
                                    SM_DLOG_ATTR_REPLICATE) )
              != IDE_SUCCESS );
    sState = 1;

    sLogType    = SDR_SDC_PK_LOG;
    sLogSize    = calcPKLogSize(aPKInfo);

    sPKColCount = aPKInfo->mTotalPKColCount;
    /* pk log���� pk size filed�� pk info size��
     * ���Ե��� �ʱ� ������ sLogSize���� 2byte�� ����. */
    sPKInfoSize = sLogSize - (2);

    sdrMiniTrans::setRefOffset( &sMtx,
                                smLayerCallback::getTableOID( aTableHeader ) );

    IDE_TEST(sdrMiniTrans::writeLogRec( &sMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   &sPKInfoSize,
                                   ID_SIZEOF(sPKInfoSize) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   &sPKColCount,
                                   ID_SIZEOF(sPKColCount) )
              != IDE_SUCCESS );

    for ( sPKColSeq = 0 ; sPKColSeq < sPKColCount ; sPKColSeq++ )
    {
        sColumnInfo = aPKInfo->mColInfoList + sPKColSeq;
        sColumnId   = (UInt)sColumnInfo->mColumn->id;

        IDE_TEST( sdrMiniTrans::write( &sMtx,
                                       (void*)&sColumnId,
                                       ID_SIZEOF(sColumnId) )
                  != IDE_SUCCESS );
        
        sInOutMode = sColumnInfo->mInOutMode;

        IDE_TEST( writeColPieceLog( &sMtx,
                                    (UChar*)sColumnInfo->mValue.value,
                                    sColumnInfo->mValue.length,
                                    sInOutMode ) != IDE_SUCCESS );
    }

    /* pk log�� DML log�� ��� ����� ��,
     * ���� �������� ����ϱ� ������
     * conttype�� SMR_CT_END�̴�. */
    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx, SMR_CT_END )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1)
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_PK_LOG
 *   -------------------------------------------------------------------
 *   | [sdrLogHdr],
 *   | [pk size(2)], [pk col count(2)],
 *   | [ {column id(4),column len(1 or 3), column data} ... ]
 *   -------------------------------------------------------------------
 *
 **********************************************************************/
UShort sdcRow::calcPKLogSize( const sdcPKInfo    *aPKInfo )
{
    const sdcColumnInfo4PK   *sColumnInfo;
    UShort                    sLogSize;
    UShort                    sPKColSeq;

    IDE_ASSERT( aPKInfo != NULL );

    sLogSize  = 0;
    sLogSize += ID_SIZEOF(UShort);  // pk size(2)
    sLogSize += ID_SIZEOF(UShort);  // column count(2)

    for ( sPKColSeq = 0; sPKColSeq < aPKInfo->mTotalPKColCount; sPKColSeq++ )
    {
        sLogSize += ID_SIZEOF(UInt);  // column id(4)

        sColumnInfo = aPKInfo->mColInfoList + sPKColSeq;
        sLogSize += SDC_GET_COLPIECE_SIZE(sColumnInfo->mValue.length);
    }

    return sLogSize;
}


/***********************************************************************
 *
 * Description :
 *  head rowpiece���� �����ؼ� fetch �۾��� �Ϸ��Ҷ����� next rowpiece�� ��� ���󰡸鼭
 *  fetch�ؾ��� column value�� dest buffer�� �����Ѵ�.
 *
 *  aStatistics      - [IN]  �������
 *  aMtx             - [IN]  mini Ʈ�����
 *  aSP              - [IN]  save point
 *  aTrans           - [IN]  Ʈ����� ����
 *  aTableSpaceID    - [IN]  tablespace id
 *  aSlotPtr         - [IN]  slot pointer
 *  aIsPersSlot      - [IN]  Persistent�� slot ����, Row Cache�κ���
 *                           �о�� ��쿡 ID_FALSE�� ������
 *  aPageReadMode    - [IN]  page read mode(SPR or MPR)
 *  aFetchColumnList - [IN]  fetch column list
 *  aFetchVersion    - [IN]  �ֽ� ���� fetch���� ���θ� ��������� ����
 *  aMyTSSlotSID     - [IN]  TSSlot's SID
 *  aMyStmtSCN       - [IN]  stmt scn
 *  aInfiniteSCN     - [IN]  infinite scn
 *  aIndexInfo4Fetch - [IN]  fetch�ϸ鼭 vrow�� ����� ���,
 *                           �� ���ڷ� �ʿ��� ������ �ѱ��.(only for index)
 *  aLobInfo4Fetch   - [IN]  lob descriptor ������ fetch�Ϸ��� �Ҷ�
 *                           �� ���ڷ� �ʿ��� ������ �ѱ��.(only for lob)
 *  aRowTemplate     - [IN]
 *  aDestRowBuf          - [OUT] dest buffer
 *  aIsRowDeleted        - [OUT] row�� delete mark�� �����Ǿ� �ִ��� ����
 *  aIsPageLatchReleased - [OUT] fetch�߿� page latch�� Ǯ�ȴ��� ����
 *  aIsSkipAssert    - [IN]
 **********************************************************************/
IDE_RC sdcRow::fetch( idvSQL                     * aStatistics,
                      sdrMtx                     * aMtx,
                      sdrSavePoint               * aSP,
                      void                       * aTrans,
                      scSpaceID                    aTableSpaceID,
                      UChar                      * aSlotPtr,
                      idBool                       aIsPersSlot,
                      sdbPageReadMode              aPageReadMode,
                      const smiFetchColumnList   * aFetchColumnList,
                      smFetchVersion               aFetchVersion,
                      sdSID                        aMyTSSlotSID,
                      const smSCN                * aMyStmtSCN,
                      const smSCN                * aInfiniteSCN,
                      sdcIndexInfo4Fetch         * aIndexInfo4Fetch,
                      sdcLobInfo4Fetch           * aLobInfo4Fetch,
                      smcRowTemplate             * aRowTemplate,
                      UChar                      * aDestRowBuf,
                      idBool                     * aIsRowDeleted,
                      idBool                     * aIsPageLatchReleased,
                      idBool                       aIsSkipAssert )
{
    UChar                 * sCurrSlotPtr;
    sdcRowFetchStatus       sFetchStatus;
    sdSID                   sNextRowPieceSID;
    UShort                  sState = 0;
    idBool                  sIsDeleted;
    idBool                  sIsPageLatchReleased = ID_FALSE;

    IDE_ERROR( (aTrans != NULL) || ( aFetchVersion == SMI_FETCH_VERSION_LAST ) );
    IDE_ERROR( aSlotPtr             != NULL );
    IDE_ERROR( aDestRowBuf          != NULL );
    IDE_ERROR( aIsRowDeleted        != NULL );
    IDE_ERROR( aIsPageLatchReleased != NULL );

    /* head rowpiece���� fetch�� �����ؾ� �Ѵ�. */
    IDE_ERROR_RAISE( isHeadRowPiece(aSlotPtr) == ID_TRUE, err_no_head_row_piece );

    IDE_DASSERT( validateSmiFetchColumnList( aFetchColumnList )
                 == ID_TRUE );

    /* FIT/ART/sm/Projects/PROJ-2118/select/select.ts */
    IDU_FIT_POINT( "1.PROJ-2118@sdcRow::fetch" );

    sNextRowPieceSID = SD_NULL_SID;
    sCurrSlotPtr     = aSlotPtr;

    /* fetch�� �����ϱ� ���� fetch ���������� �ʱ�ȭ�Ѵ�. */
    initRowFetchStatus( aFetchColumnList, &sFetchStatus );

    IDE_TEST( fetchRowPiece( aStatistics,
                             aTrans,
                             aTableSpaceID, //BUG-45598, TBS_ID
                             sCurrSlotPtr,
                             aIsPersSlot,
                             aFetchColumnList,
                             aFetchVersion,
                             aMyTSSlotSID,
                             aPageReadMode,
                             aMyStmtSCN,
                             aInfiniteSCN,
                             aIndexInfo4Fetch,
                             aLobInfo4Fetch,
                             aRowTemplate,
                             &sFetchStatus,
                             aDestRowBuf,
                             aIsRowDeleted,
                             &sNextRowPieceSID,
                             aIsSkipAssert )
              != IDE_SUCCESS );

    IDE_TEST_CONT( *aIsRowDeleted == ID_TRUE, skip_deleted_row );

    IDE_TEST_CONT( (sFetchStatus.mTotalFetchColCount ==
                     sFetchStatus.mFetchDoneColCount),
                    fetch_end );

    /* BUG-23319
     * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
    /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
     * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
     * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
     * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
     * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */

    /* BUG-26647 Disk row cache - caching �� �������� latch�� ���� �ʰ� ���
     * ���� ������ latch�� Ǯ������ �ʾƵ� �ȴ�. */
    if ( ( sNextRowPieceSID != SD_NULL_SID ) &&
         ( aIsPersSlot == ID_TRUE ) )
    {
        sIsPageLatchReleased = ID_TRUE;

        if ( aMtx != NULL )
        {
            IDE_ASSERT( aSP != NULL );
            sdrMiniTrans::releaseLatchToSP( aMtx, aSP );
        }
        else
        {
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sCurrSlotPtr )
                      != IDE_SUCCESS );
        }
        sCurrSlotPtr = NULL;
    }


    while( sNextRowPieceSID != SD_NULL_SID )
    {
        if ( sFetchStatus.mTotalFetchColCount <
             sFetchStatus.mFetchDoneColCount )
        {
            ideLog::log( IDE_ERR_0,
                         "TotalFetchColCount: %"ID_UINT32_FMT", "
                         "FetchDoneColCount: %"ID_UINT32_FMT"\n",
                         sFetchStatus.mTotalFetchColCount,
                         sFetchStatus.mFetchDoneColCount );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&sFetchStatus,
                            ID_SIZEOF(sdcRowFetchStatus),
                            "FetchStatus Dump:" );

            IDE_ASSERT( 0 );
        }

        if ( sFetchStatus.mTotalFetchColCount ==
             sFetchStatus.mFetchDoneColCount)
        {
            break;
        }

        IDU_FIT_POINT( "BUG-48070@sdcRow::fetch::getPageBySID" );

        IDE_TEST( sdbBufferMgr::getPageBySID( aStatistics,
                                              aTableSpaceID,
                                              sNextRowPieceSID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL, /* aMtx */
                                              (UChar**)&sCurrSlotPtr )
                  != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( fetchRowPiece( aStatistics,
                                 aTrans,
                                 aTableSpaceID, //BUG-45598, TBS_ID
                                 sCurrSlotPtr,
                                 ID_TRUE, /* aIsPersSlot */
                                 aFetchColumnList,
                                 aFetchVersion,
                                 aMyTSSlotSID,
                                 SDB_SINGLE_PAGE_READ,
                                 aMyStmtSCN,
                                 aInfiniteSCN,
                                 aIndexInfo4Fetch,
                                 aLobInfo4Fetch,
                                 aRowTemplate,
                                 &sFetchStatus,
                                 aDestRowBuf,
                                 &sIsDeleted,
                                 &sNextRowPieceSID,
                                 aIsSkipAssert )
                  != IDE_SUCCESS );

        if ( sIsDeleted == ID_TRUE )
        {
            if ( aFetchVersion == SMI_FETCH_VERSION_LAST )
            {
                /*
                 * BUG-42420
                 * SMI_FETCH_VERSION_LAST������
                 * 1st row split�� version�� 2,3,n..th row split�� version�� �ٸ� ���ִ�.
                 * (��, 1st row split�� delete�� �ƴϾ, �ٸ� row split���� delete �����ϼ��ִ�.)
                 * �������� �Ѵ�.
                 */

                *aIsRowDeleted = ID_TRUE;

                sState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     (UChar *)sCurrSlotPtr )
                          != IDE_SUCCESS );

                IDE_CONT( skip_deleted_row );
            }
            else
            {
                /* BUG-46125 codesonar warning ���� */
                if( aInfiniteSCN != NULL )
                {
                    ideLog::log( IDE_ERR_0,
                                 "Fetch Info\n"
                                 "TransID:        %"ID_UINT32_FMT"\n"
                                 "FstDskViewSCN:  %"ID_UINT64_FMT"\n"
                                 "FetchVersion:   %"ID_UINT32_FMT"\n"
                                 "MyStmtSCN:      %"ID_UINT64_FMT"\n"
                                 "InfiniteSCN:    %"ID_UINT64_FMT"\n",
                                 smxTrans::getTransID( aTrans ),
                                 SM_SCN_TO_LONG( smxTrans::getFstDskViewSCN( aTrans ) ),
                                 aFetchVersion,
                                 SM_SCN_TO_LONG( *aMyStmtSCN ),
                                 SM_SCN_TO_LONG( *aInfiniteSCN ) );
                }
                else
                {
                    ideLog::log( IDE_ERR_0,
                                 "Fetch Info\n"
                                 "TransID:        %"ID_UINT32_FMT"\n"
                                 "FstDskViewSCN:  %"ID_UINT64_FMT"\n"
                                 "FetchVersion:   %"ID_UINT32_FMT"\n",
                                 smxTrans::getTransID( aTrans ),
                                 SM_SCN_TO_LONG( smxTrans::getFstDskViewSCN( aTrans ) ),
                                 aFetchVersion );
                }

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             aSlotPtr,
                                             "Head Page Dump:" );

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             sCurrSlotPtr,
                                             "Current Page Dump:" );

                IDE_ASSERT( 0 );
            }
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sCurrSlotPtr )
                  != IDE_SUCCESS );
    }

    if ( sFetchStatus.mTotalFetchColCount >
         sFetchStatus.mFetchDoneColCount )
    {
        /* trailing null value�� fetch�Ϸ��� ��� */
        IDE_TEST( doFetchTrailingNull( aFetchColumnList,
                                       aIndexInfo4Fetch,
                                       aLobInfo4Fetch,
                                       &sFetchStatus,
                                       aDestRowBuf )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( fetch_end );

    if ( sFetchStatus.mTotalFetchColCount != sFetchStatus.mFetchDoneColCount )
    {
        ideLog::log( IDE_ERR_0,
                     "TotalFetchColCount: %"ID_UINT32_FMT", "
                     "FetchDoneColCount: %"ID_UINT32_FMT"\n",
                     sFetchStatus.mTotalFetchColCount,
                     sFetchStatus.mFetchDoneColCount );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&sFetchStatus,
                        ID_SIZEOF(sdcRowFetchStatus),
                        "FetchStatus Dump:" );

        IDE_ASSERT( 0 );
    }


    IDE_EXCEPTION_CONT( skip_deleted_row );

    *aIsPageLatchReleased = sIsPageLatchReleased;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_no_head_row_piece );
    {
        ideLog::log( IDE_SM_0, "sdcRow::fetch() - No Head Row Piece");
    }
    IDE_EXCEPTION_END;

    *aIsPageLatchReleased = sIsPageLatchReleased;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sCurrSlotPtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :
 *  �ڽ��� �о���� ������ ���ϰ�, rowpiece�� fetch�ؾ��� �÷������� �ִ���
 *  �м��� ��, fetch �۾��� �����Ѵ�.
 *
 *  aStatistics      - [IN]  �������
 *  aTrans           - [IN]  Ʈ����� ����
 *  aTableSpaceID    - [IN] 
 *  aCurrSlotPtr     - [IN]  slot pointer
 *  aIsPersSlot      - [IN]  Persistent�� slot ����, Row Cache�κ���
 *                           �о�� ��쿡 ID_FALSE�� ������
 *  aFetchColumnList - [IN]  fetch column list
 *  aFetchVersion    - [IN]  �ֽ� ���� fetch���� ���θ� ��������� ����
 *  aMyTSSlotSID     - [IN]  TSS's SID
 *  aPageReadMode    - [IN]  page read mode(SPR or MPR)
 *  aMyStmtSCN       - [IN]  stmt scn
 *  aInfiniteSCN     - [IN]  infinite scn
 *  aIndexInfo4Fetch - [IN]  fetch�ϸ鼭 vrow�� ����� ���,
 *                           �� ���ڷ� �ʿ��� ������ �ѱ��.(only for index)
 *  aLobInfo4Fetch   - [IN]  lob descriptor ������ fetch�Ϸ��� �Ҷ�
 *                           �� ���ڷ� �ʿ��� ������ �ѱ��.(only for lob)
 *  aRowTemplate     - [IN] 
 *  aFetchStatus     - [IN/OUT] fetch ������� ����
 *  aDestRowBuf      - [OUT] dest buffer
 *  aIsRowDeleted    - [OUT] row piece�� delete mark�� �����Ǿ� �ִ��� ����
 *  aNextRowPieceSID - [OUT] next rowpiece sid
 *  aIsSkipAssert    - [IN]
 **********************************************************************/
IDE_RC sdcRow::fetchRowPiece( idvSQL                     * aStatistics,
                              void                       * aTrans,
                              scSpaceID                    aTableSpaceID,
                              UChar                      * aCurrSlotPtr,
                              idBool                       aIsPersSlot,
                              const smiFetchColumnList   * aFetchColumnList,
                              smFetchVersion               aFetchVersion,
                              sdSID                        aMyTSSlotSID,
                              sdbPageReadMode              aPageReadMode,
                              const smSCN                * aMyStmtSCN,
                              const smSCN                * aInfiniteSCN,
                              sdcIndexInfo4Fetch         * aIndexInfo4Fetch,
                              sdcLobInfo4Fetch           * aLobInfo4Fetch,
                              smcRowTemplate             * aRowTemplate,
                              sdcRowFetchStatus          * aFetchStatus,
                              UChar                      * aDestRowBuf,
                              idBool                     * aIsRowDeleted,
                              sdSID                      * aNextRowPieceSID,
                              idBool                       aIsSkipAssert )
{
    UChar                * sSlotPtr;
    sdcRowHdrInfo          sRowHdrInfo;
    UChar                  sOldVersionBuf[SD_PAGE_SIZE]; /* UNDO PAGE �κ��� OLD VERSION PIECE ���� */
    idBool                 sDoMakeOldVersion = ID_FALSE;
    UShort                 sFetchColCount;
    sdSID                  sUndoSID = SD_NULL_SID;
    smiFetchColumnList   * sLstFetchedColumn;
    UShort                 sLstFetchedColumnLen;
    UChar                * sColPiecePtr;
    UChar                * sCurPageHdr = NULL;
    scPageID               sCorruptedPID = 0;

    IDE_ASSERT( (aTrans != NULL) || (aFetchVersion == SMI_FETCH_VERSION_LAST) );
    IDE_ASSERT( aCurrSlotPtr     != NULL );
    IDE_ASSERT( aFetchStatus     != NULL );
    IDE_ASSERT( aDestRowBuf      != NULL );
    IDE_ASSERT( aNextRowPieceSID != NULL );

    if ( aFetchVersion == SMI_FETCH_VERSION_LAST )
    {
        sSlotPtr = aCurrSlotPtr;
    }
    else
    {
        IDE_TEST( getValidVersion( aStatistics,
                                   aCurrSlotPtr,
                                   aIsPersSlot,
                                   aFetchVersion,
                                   aMyTSSlotSID,
                                   aPageReadMode,
                                   aMyStmtSCN,
                                   aInfiniteSCN,
                                   aTrans,
                                   aLobInfo4Fetch,
                                   &sDoMakeOldVersion,
                                   &sUndoSID,
                                   sOldVersionBuf )
                  != IDE_SUCCESS );

        /*  BUG-24216
         *  [SD-���ɰ���] makeOldVersion() �Ҷ�����
         *  rowpiece �����ϴ� ������ delay ��Ų��. */
        if ( sDoMakeOldVersion == ID_TRUE )
        {
            /* old version�� �д� ���,
             * sOldVersionBuf�� old version�� ������� �����Ƿ�
             * �̸� �̿��ؾ� �Ѵ�. */
            sSlotPtr = sOldVersionBuf;
        }
        else
        {
            /* old version�� ���� �ʴ� ���(�ֽ� ������ �д� ���)
             * sOldVersionBuf�� �ֽ� ������ ����Ǿ� ���� �ʴ�.
             * (�ֽ� ������ �д� ��� memcpy�ϴ� ����� �ٿ���
             * ������ �������ѷ� �Ͽ��� ������
             * sOldVersionBuf�� �ֽ� ������ �������� �ʴ´�.)
             * �׷��� ���ڷ� �Ѿ�� slot pointer�� �̿��ؾ� �Ѵ�. */
            sSlotPtr = aCurrSlotPtr;
        }
    }

    /* BUG-45598: fetch�ؾ��� row�� ���� �������� ���� ���� ��, �������� �߿�
     * inconsistent�� �������� ������ ��� fetch�߿��� �Ǻ� ����. �� �� ������
     * ���� skip �� ��
     */
    if ( sdrCorruptPageMgr::getCorruptPageReadPolicy() != SDB_CORRUPTED_PAGE_READ_FATAL )
    {
        if ( sDoMakeOldVersion != ID_TRUE )
        {
            sCurPageHdr = sdpPhyPage::getPageStartPtr( sSlotPtr );

            /* oldVersion�̸� getValidVersion���� undoRecord�� ����鼭 ������ 
             * �Ǵ� �������� ���ؼ� ó����( �������� Corrupt �Ǿ����� ������� 
             * ����� �� ����. )  
             */
            
            if ( ( sdbBufferMgr::isPageReadError( sSlotPtr ) == ID_TRUE ) )
            {
                sCorruptedPID = ( (sdpPhyPageHdr*)sCurPageHdr )->mPageID;

                if ( smuProperty::getCrashTolerance() != 0 )
                {
                    IDE_CONT( skip_inconsistent_row );
                }
                else
                {
                    IDE_RAISE( error_page_corrupted );
                }
            }
        }
    }

    getRowHdrInfo( sSlotPtr, &sRowHdrInfo );
    sColPiecePtr = getDataArea( sSlotPtr );

    IDE_TEST_CONT( SM_SCN_IS_DELETED(sRowHdrInfo.mInfiniteSCN) == ID_TRUE,
                    skip_deleted_rowpiece );

    /* fetch �� column�� ������ �� ���� ����. */
    IDE_TEST_CONT( aFetchColumnList == NULL, skip_no_fetch_column );

    if ( SDC_IS_HEAD_ONLY_ROWPIECE(sRowHdrInfo.mRowFlag) == ID_TRUE )
    {
        /* row migration�� �߻��Ͽ�
         * row header�� �����ϴ� rowpiece�� ��� */
        if ( sRowHdrInfo.mColCount != 0 )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&sRowHdrInfo,
                            ID_SIZEOF(sdcRowHdrInfo),
                            "RowHdrInfo Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        IDE_DASSERT( isHeadRowPiece(sSlotPtr) == ID_TRUE );

        IDE_CONT(skip_header_only_rowpiece);
    }

    if ( sRowHdrInfo.mColCount == 0 )
    {
        /* NULL ROW�� ���(��� column�� ���� NULL�� ROW) */
        if ( ( sRowHdrInfo.mRowFlag != SDC_ROWHDR_FLAG_NULLROW ) ||
             ( getRowPieceSize(sSlotPtr) != SDC_ROWHDR_SIZE ) )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)&sRowHdrInfo,
                            ID_SIZEOF(sdcRowHdrInfo),
                            "RowHdrInfo Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        IDE_DASSERT( isHeadRowPiece(sSlotPtr) == ID_TRUE );

        IDE_CONT(skip_null_row);
    }

    if ( aFetchStatus->mTotalFetchColCount != 0 )
    {
        /* ã�� row piece (sColPiecePtr)�� row buffer (aDestRowBuf)�� ������. */
        IDE_TEST( doFetch( sColPiecePtr,
                           aRowTemplate,
                           aFetchStatus,
                           aLobInfo4Fetch,
                           aIndexInfo4Fetch,
                           sUndoSID,
                           &sRowHdrInfo,
                           aDestRowBuf,
                           &sFetchColCount,
                           &sLstFetchedColumnLen,
                           &sLstFetchedColumn,
                           aIsSkipAssert ) 
                  != IDE_SUCCESS );

        /* fetch ������� ������ �����Ѵ�. */
        resetRowFetchStatus( &sRowHdrInfo,
                             sFetchColCount,
                             sLstFetchedColumn,
                             sLstFetchedColumnLen,
                             aFetchStatus );
    }

    IDE_EXCEPTION_CONT( skip_deleted_rowpiece );
    IDE_EXCEPTION_CONT( skip_header_only_rowpiece );
    IDE_EXCEPTION_CONT( skip_null_row );
    IDE_EXCEPTION_CONT( skip_no_fetch_column );
    IDE_EXCEPTION_CONT( skip_inconsistent_row );    

    if ( aIsRowDeleted != NULL )
    {
        *aIsRowDeleted = isDeleted(sSlotPtr);
    }

    *aNextRowPieceSID = getNextRowPieceSID(sSlotPtr);

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_page_corrupted )
    {
        switch( sdrCorruptPageMgr::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
                IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                          aTableSpaceID,
                                          sCorruptedPID ));
                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          aTableSpaceID,
                                          sCorruptedPID ));
                break;
            default:
                break;
        }
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::doFetch( UChar               * aColPiecePtr,
                        smcRowTemplate      * aRowTemplate,
                        sdcRowFetchStatus   * aFetchStatus,
                        sdcLobInfo4Fetch    * aLobInfo4Fetch,
                        sdcIndexInfo4Fetch  * aIndexInfo4Fetch,
                        sdSID                 aUndoSID,
                        sdcRowHdrInfo       * aRowHdrInfo,
                        UChar               * aDestRowBuf,
                        UShort              * aFetchColumnCount,
                        UShort              * aLstFetchedColumnLen,
                        smiFetchColumnList ** aLstFetchedColumn,
                        idBool                aIsSkipAssert )
{
    const smiFetchColumnList   * sCurrFetchCol;
    const smiFetchColumnList   * sLstFetchedCol;
    const smiColumn            * sFetchColumn;
    sdcValue4Fetch               sFetchColumnValue;
    smcColTemplate             * sFetchColTemplate;
    smcColTemplate             * sColTemplate;
    sdcLobDesc                   sLobDesc;
    UShort                       sCurFetchColumnSeq;
    UShort                       sColSeqInRowPiece;
    UShort                       sAdjustOffet = 0;
    UShort                       sAdjustOffet4RowPiece;
    UShort                       sFstColumnStartOffset;
    UShort                       sFstsColumnLen;
    UShort                       sCurVarColumnIdx;
    UShort                       sCumulativeAdjustOffset = 0;
    UShort                       sMaxColSeqInRowPiece;
    UShort                       sFstColumnSeq;
    UShort                       sFetchCount = 0;
    UInt                         sCopyOffset;
    idBool                       sIsPrevFlag;
    idBool                       sIsVariableColumn;
    UChar                      * sColStartPtr; 
    UChar                      * sAdjustedColStartPtr; 

    IDE_DASSERT( aColPiecePtr                   != NULL );
    IDE_DASSERT( aDestRowBuf                    != NULL );
    IDE_DASSERT( aRowTemplate                   != NULL );
    IDE_DASSERT( aRowHdrInfo                    != NULL );
    IDE_DASSERT( aFetchStatus                   != NULL );
    IDE_DASSERT( aFetchStatus->mFstFetchConlumn != NULL );
    IDE_DASSERT( aLstFetchedColumn              != NULL );

    sFetchColumnValue.mValue.length = 0;
    sIsPrevFlag          = SM_IS_FLAG_ON(aRowHdrInfo->mRowFlag, SDC_ROWHDR_P_FLAG);
    sCopyOffset          = aFetchStatus->mAlreadyCopyedSize;
    sCurFetchColumnSeq   = SDC_GET_COLUMN_SEQ(aFetchStatus->mFstFetchConlumn->column);
    sFstColumnSeq        = aFetchStatus->mFstColumnSeq;
    sColSeqInRowPiece    = sCurFetchColumnSeq - sFstColumnSeq;
    sCurVarColumnIdx     = aRowTemplate->mColTemplate[sFstColumnSeq].mVariableColIdx;
    sMaxColSeqInRowPiece = aRowHdrInfo->mColCount - 1;

    /* fetch�Ϸ��� �÷��� prev rowpiece���� �̾��� ���,
     * offset�� ����ؼ� callback �Լ��� ȣ���ؾ� �Ѵ�.
     * �̿� ���� ���� �ڵ� �̴�.*/
    IDE_TEST_RAISE( !((sCopyOffset == 0) ||
                     ((sCopyOffset       != 0) &&
                      (sIsPrevFlag       == ID_TRUE) &&
                      (sColSeqInRowPiece == 0))), error_connected_rowpiece );

    sColTemplate          = &aRowTemplate->mColTemplate[ sFstColumnSeq ];
    sFstColumnStartOffset = sColTemplate->mColStartOffset;

    /* notnull�� �ƴϰų� variable column�� �ƴ� column�� slpit�� ��쿡 ���� ó��
     * row piece���� offset ���� */
    if ( (sColTemplate->mStoredSize != SMC_UNDEFINED_COLUMN_LEN) && 
         (sIsPrevFlag == ID_TRUE) )
    {
        sFstsColumnLen = getColumnStoredLen( aColPiecePtr );

        if ( ( sFstColumnSeq + 1 ) < aRowTemplate->mTotalColCount )
        {
            sColTemplate   = &aRowTemplate->mColTemplate[ sFstColumnSeq + 1 ];
            IDE_DASSERT( sFstsColumnLen <= sColTemplate->mColStartOffset );

             /* row piece���� �ι�° mColStartOffset���� ù��° column�� ���̸� ����.
              * �̴� fetch�Ϸ��� column�� ���� row piece�� non variable column�� ��
              * ���̸� ���ϱ� �����̴�.
              * ���� �� ���� aColPiecePtr(row piece���� column data���� ��ġ)
              * ���� ���ش�.(sAdjustedColStartPtr)
              * ���� fetch�Ҷ� fetch��� column�� mColStartOffset�� ���� offset ������ �Ѵ�.*/
            sAdjustOffet = sColTemplate->mColStartOffset - sFstsColumnLen;
        }
        else
        {
            /* row piece�� ù��° column sequence + 1 �� row�� ��ü column���� ���ٸ�
             * �ش� column�� sColSeqInRowPiece�� �׻� 0 �̴�
             * row�� slplit�Ǿ� row�� ������ column�� �ٸ� �������� ����� ���*/
            IDE_ASSERT( (sFstColumnSeq + 1) == aRowTemplate->mTotalColCount );
            IDE_ASSERT( sColSeqInRowPiece == 0 );
        }

        sAdjustOffet4RowPiece = 
                ( sColSeqInRowPiece == 0 ) ? sFstColumnStartOffset : sAdjustOffet;
    }
    else
    {
        sAdjustOffet          = sFstColumnStartOffset;
        sAdjustOffet4RowPiece = sFstColumnStartOffset;
    }

    sLstFetchedCol = aFetchStatus->mFstFetchConlumn;

    for ( sCurrFetchCol = aFetchStatus->mFstFetchConlumn; 
          sCurrFetchCol != NULL; 
          sCurrFetchCol = sCurrFetchCol->next )
    {
        IDE_DASSERT( sCurrFetchCol->columnSeq == SDC_GET_COLUMN_SEQ(sCurrFetchCol->column) );
        sFetchColTemplate    = &aRowTemplate->mColTemplate[ sCurrFetchCol->columnSeq ];
        sCurFetchColumnSeq   = sCurrFetchCol->columnSeq;
        sFetchColumn         = sCurrFetchCol->column;
        sColSeqInRowPiece    = sCurFetchColumnSeq - sFstColumnSeq;
        sAdjustedColStartPtr = aColPiecePtr - sAdjustOffet4RowPiece;

        /* ���� row piece���� fetch�� column�� ���̻� ����. */
        if ( sMaxColSeqInRowPiece < sColSeqInRowPiece )
        {
            break;
        }

        /* notnull�� �ƴ� column�� variable column�� ���� ���̸� ����
         * fetch�Ϸ��� colum�� offset�� ���Ѵ�.
         * variable column���� offset ����*/
        if ( aRowTemplate->mVariableColCount != 0 )
        {
            for ( ; sCurVarColumnIdx < sFetchColTemplate->mVariableColIdx; sCurVarColumnIdx++ )
            {
                /* aRowTemplate->mVariableColOffset�� �м��Ϸ��� variable colum�տ� �����ϴ�
                 * non variable column���� ���̸� ��Ÿ����.
                 * �̴� �м��Ϸ��� non variable column�� ���� offset�� ���ϱ� ���� ���ȴ�.
                 * �����Ǿ� ����Ǵ� ���� �ƴϱ� ������
                 * �Ź� �ش��ϴ� column�� �´� ���� ���� column�� ��ġ�� ���ؾ� �Ѵ�. */
                sColStartPtr = sAdjustedColStartPtr 
                               + aRowTemplate->mVariableColOffset[ sCurVarColumnIdx ] 
                               + sCumulativeAdjustOffset; /* ���� ���� variable column���� ���� */

                sCumulativeAdjustOffset += getColumnStoredLen( sColStartPtr );
            }
        }

        /* rowPiece�� ù��° column�� ������ column��
         * split�Ǿ� ���� �� �ֱ� ������ rowTemplate��
         * ������ �̿����� �ʰ� column ������ �����´�*/
        if ( (sFetchColTemplate->mStoredSize != SMC_UNDEFINED_COLUMN_LEN) &&
             (sCopyOffset == 0) && 
             (sColSeqInRowPiece != sMaxColSeqInRowPiece) )
        {
            sIsVariableColumn = ID_FALSE;
        }
        else
        {
            sIsVariableColumn = ID_TRUE;
        }

        /* fetch�Ϸ��� column�� ������ �����´�. */
        getColumnValue( sAdjustedColStartPtr,
                        sCumulativeAdjustOffset,
                        sIsVariableColumn,
                        sFetchColTemplate,
                        &sFetchColumnValue );

        if ( aIndexInfo4Fetch == NULL )
        {
            if ( SDC_IS_LOB_COLUMN( sFetchColumn ) == ID_TRUE )
            {
                makeLobColumnInfo( aLobInfo4Fetch,
                                   &sLobDesc,
                                   &sCopyOffset,
                                   &sFetchColumnValue );
            }

            /* BUG-32278 The previous flag is set incorrectly in row flag when
             * a rowpiece is overflowed. */
            IDE_TEST_RAISE( sFetchColumnValue.mValue.length > sFetchColumn->size, 
                            error_disk_column_size );
 
            /* �����͸� �����Ҷ�, MT datatype format���� �������� �ʰ�
             * raw value�� �����Ѵ�.(���� ������ ���̱� ���ؼ��̴�.)
             * �׷��� ����� data�� QP���� ������ ��,
             * memcpy�� �ϸ� �ȵǰ�, QP�� ������ callback �Լ��� ȣ���ؾ� �Ѵ�.
             * �� callback �Լ����� raw value�� MT datatype format���� ��ȯ�Ѵ�. */
             /* BUG-31582 [sm-disk-collection] When an fetch error occurs
              * in DRDB, output the information. */
            if ( ((smiCopyDiskColumnValueFunc)sCurrFetchCol->copyDiskColumn)( 
                                            sFetchColumn->size, 
                                            aDestRowBuf + sFetchColumn->offset,
                                            sCopyOffset,
                                            sFetchColumnValue.mValue.length,
                                            sFetchColumnValue.mValue.value ) != IDE_SUCCESS )
            {
                dumpErrorCopyDiskColumn( aColPiecePtr,
                                         sFetchColumn,
                                         aUndoSID,
                                         &sFetchColumnValue,
                                         sCopyOffset,
                                         sColSeqInRowPiece,
                                         aRowHdrInfo );

                (void)smcTable::dumpRowTemplate( aRowTemplate );

                IDE_ASSERT( 0 );
            }
        }
        else
        {
            /* lob column�� index�� ����� ����. */
            IDE_DASSERT( SDC_IS_LOB_COLUMN( sFetchColumn ) == ID_FALSE );

            /* special fetch for index */
            /* index���� ������ callback �Լ��� ȣ���Ͽ�
             * index�� ���ϴ� format�� �÷������� �����. */
            IDE_TEST( aIndexInfo4Fetch->mCallbackFunc4Index(
                                                 sFetchColumn,
                                                 sCopyOffset,
                                                 &sFetchColumnValue.mValue,
                                                 aIndexInfo4Fetch)
                      != IDE_SUCCESS );
        }

        sFetchCount++;
        sCopyOffset           = 0;
        sLstFetchedCol        = sCurrFetchCol;
        sAdjustOffet4RowPiece = sAdjustOffet;
    }

    *aLstFetchedColumn    = (smiFetchColumnList*)sLstFetchedCol;
    *aLstFetchedColumnLen = sFetchColumnValue.mValue.length;
    *aFetchColumnCount    = sFetchCount;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( error_connected_rowpiece )
    {
        IDE_ASSERT_MSG( ( aIsSkipAssert == ID_TRUE ),
                          "CopyOffset       : %"ID_UINT32_FMT"\n"
                          "Is Prev          : %"ID_UINT32_FMT"\n"
                          "ColSeqInRowPiece : %"ID_UINT32_FMT"\n",
                          sCopyOffset, 
                          sIsPrevFlag, 
                          sColSeqInRowPiece );
    }

    IDE_EXCEPTION( error_disk_column_size )
    {
        if( aIsSkipAssert == ID_FALSE )
        {
            dumpErrorDiskColumnSize( aColPiecePtr,
                                     sFetchColumn,
                                     aUndoSID,
                                     aDestRowBuf,
                                     &sFetchColumnValue,
                                     sCopyOffset,
                                     sColSeqInRowPiece,
                                     aRowHdrInfo );

            (void)smcTable::dumpRowTemplate( aRowTemplate );

            IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, 
                                     "fail to copy disk column") );

            IDE_DASSERT( 0 );
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void sdcRow::dumpErrorDiskColumnSize( UChar           * aColPiecePtr,
                                      const smiColumn * aFetchColumn,
                                      sdSID             aUndoSID,
                                      UChar           * aDestRowBuf,
                                      sdcValue4Fetch  * aFetchColumnValue,
                                      UInt              aCopyOffset,
                                      UShort            aColSeqInRowPiece,
                                      sdcRowHdrInfo   * aRowHdrInfo )
{
    scPageID                     sPageID;
    scOffset                     sOffset;

    sPageID = sdpPhyPage::getPageID(
        (UChar*)sdpPhyPage::getPageStartPtr(aColPiecePtr));
    
    sOffset = sdpPhyPage::getOffsetFromPagePtr(aColPiecePtr);

    ideLog::log( IDE_ERR_0,
                 "Error - DRDBRow:doFetch :\n"
                 "ColumnID    : %"ID_UINT32_FMT"\n"
                 "ColumnFlag  : %"ID_UINT32_FMT"\n"
                 "ColumnSize  : %"ID_UINT32_FMT"\n"
                 "ColumnOffset: %"ID_UINT32_FMT"\n"
                 "CopyOffset  : %"ID_UINT32_FMT"\n"
                 "ColSeq      : %"ID_UINT32_FMT"\n"
                 "ColCount    : %"ID_UINT32_FMT"\n"
                 "RowFlag     : %"ID_UINT32_FMT"\n"
                 "length      : %"ID_UINT32_FMT"\n"
                 "PageID      : %"ID_UINT32_FMT"\n"
                 "Offset      : %"ID_UINT32_FMT"\n"
                 "UndoSID     : PID  : %"ID_UINT32_FMT"\n"
                 "            : Slot : %"ID_UINT32_FMT"\n",
                 aFetchColumn->id,
                 aFetchColumn->flag,
                 aFetchColumn->size,
                 aFetchColumn->offset,
                 aCopyOffset,
                 aColSeqInRowPiece,
                 aRowHdrInfo->mColCount,
                 aRowHdrInfo->mRowFlag,
                 aFetchColumnValue->mValue.length,
                 sPageID,
                 sOffset,
                 SD_MAKE_PID( aUndoSID ),
                 SD_MAKE_SLOTNUM( aUndoSID ) );

    ideLog::logMem( IDE_ERR_0,
                    (UChar*)aFetchColumnValue->mValue.value,
                    aFetchColumnValue->mValue.length,
                    "[ Value ]" );

    ideLog::logMem( IDE_ERR_0,
                    aDestRowBuf,
                    aFetchColumn->offset,
                    "[ Dest Row Buf ]" );

    sdpPhyPage::tracePage( IDE_ERR_0,
                           aColPiecePtr,
                           NULL ,
                           "[ Dump Page ]");
}

void sdcRow::dumpErrorCopyDiskColumn( UChar           * aColPiecePtr,
                                      const smiColumn * aFetchColumn,
                                      sdSID             aUndoSID,
                                      sdcValue4Fetch  * aFetchColumnValue,
                                      UInt              aCopyOffset,
                                      UShort            aColSeqInRowPiece,
                                      sdcRowHdrInfo   * aRowHdrInfo )
{
    ideLog::log( IDE_ERR_0, 
                 "Error - DRDBRow:doFetch :\n"
                 "ColumnID    : %"ID_UINT32_FMT"\n"
                 "ColumnFlag  : %"ID_UINT32_FMT"\n"
                 "ColumnSize  : %"ID_UINT32_FMT"\n"
                 "ColumnOffset: %"ID_UINT32_FMT"\n"
                 "CopyOffset  : %"ID_UINT32_FMT"\n"
                 "UndoSID     : PID  : %"ID_UINT32_FMT"\n"
                 "            : Slot : %"ID_UINT32_FMT"\n"
                 "ColSeq      : %"ID_UINT32_FMT"\n"
                 "ColCount    : %"ID_UINT32_FMT"\n"
                 "RowFlag     : %"ID_UINT32_FMT"\n"
                 "length      : %"ID_UINT32_FMT"\n",
                 aFetchColumn->id,
                 aFetchColumn->flag,
                 aFetchColumn->size,
                 aFetchColumn->offset,
                 aCopyOffset,
                 SD_MAKE_PID( aUndoSID ),
                 SD_MAKE_SLOTNUM( aUndoSID ),
                 aColSeqInRowPiece,
                 aRowHdrInfo->mColCount,
                 aRowHdrInfo->mRowFlag,
                 aFetchColumnValue->mValue.length );

    ideLog::logMem( IDE_ERR_0,
                    (UChar*)aFetchColumnValue->mValue.value,
                    aFetchColumnValue->mValue.length );
     
    //UndoRecord�κ��� ������ ��찡 �ƴϸ�, Page�� Dump��
    if ( aUndoSID == SD_NULL_SID )
    {
        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aColPiecePtr,
                                     "Undo Page" );
    }
}

void sdcRow::makeLobColumnInfo( sdcLobInfo4Fetch * aLobInfo4Fetch,
                                sdcLobDesc       * aLobDesc,
                                UInt             * aCopyOffset,
                                sdcValue4Fetch   * aFetchColumnValue )
{

    smiValue     sLobValue;

    IDE_DASSERT( aLobDesc          != NULL );
    IDE_DASSERT( aCopyOffset       != NULL );
    IDE_DASSERT( aFetchColumnValue != NULL );
    IDE_DASSERT( (aFetchColumnValue->mInOutMode == SDC_COLUMN_IN_MODE) || 
                 (aFetchColumnValue->mInOutMode == SDC_COLUMN_OUT_MODE_LOB) );

    sLobValue.value  = aLobDesc;
    sLobValue.length = ID_SIZEOF(sdcLobDesc);

    // aLobInfo4Fetch �� NULL�� ���� QP���� Fetch�� ȣ���� ����̴�.
    // QP�� In Mode Lob�϶����� Lob Descriptor�� ���� �Ѵ�.
    // �׷��Ƿ� In Mode Lob �� ��� ���� LOB Descriptor��
    // �����Ͽ� �ѱ��.
    if ( aLobInfo4Fetch == NULL )
    {
        if ( aFetchColumnValue->mInOutMode == SDC_COLUMN_IN_MODE )
        {
            sdcLob::initLobDesc( aLobDesc );

            if ( SDC_IS_NULL( &aFetchColumnValue->mValue ) == ID_TRUE )
            {
                aLobDesc->mLobDescFlag = SDC_LOB_DESC_NULL_TRUE;
            }

            aLobDesc->mLastPageSize = (*aCopyOffset) + aFetchColumnValue->mValue.length;

            *aCopyOffset = 0;
        }
        else
        {
            IDE_ASSERT( *aCopyOffset == 0 );

            idlOS::memcpy( aLobDesc,
                           aFetchColumnValue->mValue.value,
                           ID_SIZEOF(sdcLobDesc) );
        }

        aFetchColumnValue->mValue.value = sLobValue.value;
        aFetchColumnValue->mValue.length = sLobValue.length;
    }
    else
    {
        // aLobInfo4Fetch �� NULL�� �ƴ� ����
        // sdcLob���� fetch�� ȣ���� ����̴�. InOut
        // Mode�� �������� �ʰ� Column Data�� �ѱ��.
        aLobInfo4Fetch->mInOutMode = aFetchColumnValue->mInOutMode;
    }
}

/***********************************************************************
 *
 * Description :
 *  trailing null�� �����ϴ� ���,
 *  NULL value�� dest buf�� �������ش�.
 *
 *  aFetchColumnList    - [IN]     fetch column list
 *  aIndexInfo4Fetch    - [IN]     fetch�ϸ鼭 vrow�� ����� ���,
 *                                 �� �ڷᱸ���� �ʿ��� �������� �ѱ��.
 *                                 normal fetch�� ���� NULL�̴�.
 *  aFetchStatus        - [IN/OUT] fetch ������� ����
 *  aDestRowBuf         - [OUT]    dest buf
 *
 **********************************************************************/
IDE_RC sdcRow::doFetchTrailingNull( const smiFetchColumnList * aFetchColumnList,
                                    sdcIndexInfo4Fetch       * aIndexInfo4Fetch,
                                    sdcLobInfo4Fetch         * aLobInfo4Fetch,
                                    sdcRowFetchStatus        * aFetchStatus,
                                    UChar                    * aDestRowBuf )
{
    const smiFetchColumnList * sFetchColumnList;
    const smiFetchColumnList * sCurrFetchCol;
    UShort                     sLoop;
    smiValue                   sDummyNullValue;
    smiValue                   sDummyLobValue;
    smiValue                 * sDummyValue;
    sdcLobDesc                 sLobDesc;
    smiValue                   sDummyDicNullValue;
    smcTableHeader           * sDicTable = NULL;


    IDE_ASSERT( aFetchColumnList != NULL );
    IDE_ASSERT( aFetchStatus     != NULL );
    IDE_ASSERT( aDestRowBuf      != NULL );

    sDummyNullValue.length = 0;
    sDummyNullValue.value  = NULL;
    sDummyLobValue.value  = &sLobDesc;
    sDummyLobValue.length = ID_SIZEOF(sdcLobDesc);

    sFetchColumnList = aFetchColumnList;

    /* BUG-23371
     * smiFetchColumnList�� list�� �����Ǿ�������
     * fetch column�� ���� ����ϴ� ��찡 �ֽ��ϴ�. */
    for(sLoop = 0; sLoop < aFetchStatus->mFetchDoneColCount; sLoop++)
    {
        sFetchColumnList = sFetchColumnList->next;
    }

    if ( sFetchColumnList == NULL )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aFetchStatus,
                        ID_SIZEOF(sdcRowFetchStatus),
                        "FetchStatus Dump:" );

        IDE_ASSERT( 0 );
    }

    for ( sCurrFetchCol = aFetchStatus->mFstFetchConlumn; 
          sCurrFetchCol != NULL; 
          sCurrFetchCol = sCurrFetchCol->next ) 
    {
        if ( SDC_IS_LOB_COLUMN( sCurrFetchCol->column ) != ID_TRUE )
        {
            /* PROJ-2429 Dictionary based data compress for on-disk DB */
            if ( (sCurrFetchCol->column->flag & SMI_COLUMN_COMPRESSION_MASK)
                 != SMI_COLUMN_COMPRESSION_TRUE )
            {
                sDummyValue = &sDummyNullValue;
            }
            else
            {
                /* dictionary compressed column�� TrailingNull�� ���ԵǾ��ִٸ�
                 * �ش� column�� null ���� ����Ǿ��ִ� dictionary table�� �ش� OID��
                 * fetch �ؾ� �Ѵ�.
                 * alter table t1 add column c20 �� ���� add column����
                 * dictionary compressed column �� �߰� �Ȱ�� �ش� column��
                 * TrailingNull�� ���� �ȴ�. */
                IDE_TEST( smcTable::getTableHeaderFromOID( 
                            sCurrFetchCol->column->mDictionaryTableOID,
                            (void**)&sDicTable ) != IDE_SUCCESS );

                sDummyDicNullValue.length = ID_SIZEOF(smOID);
                sDummyDicNullValue.value  = &sDicTable->mNullOID;
                
                sDummyValue = &sDummyDicNullValue;
            }
        }
        else
        {
            if ( aLobInfo4Fetch == NULL )
            {
                (void)sdcLob::initLobDesc( &sLobDesc );

                sLobDesc.mLobDescFlag = SDC_LOB_DESC_NULL_TRUE;
                
                // QP ���� LOB Desc�� ������� ȣ���� ���
                sDummyValue = &sDummyLobValue;
            }
            else
            {
                // sm ���� LOB Column Piece�� ��� ���� ȣ���Ѱ��
                sDummyValue                = &sDummyNullValue;
                aLobInfo4Fetch->mInOutMode = SDC_COLUMN_IN_MODE;
            }
        }

        /* BUG-23911
         * Index�� ���� Row Fetch�������� Trailing Null�� ���� ����� �ȵǾ����ϴ�. */
        /* trailing null value�� fetch�ϴ� ��쿡��,
         * [normal fetch] or [special fetch for index] ������
         * �����Ͽ� ó���Ѵ�. */
        if ( aIndexInfo4Fetch == NULL )
        {
            /* normal fetch */
            IDE_ASSERT( ((smiCopyDiskColumnValueFunc)sCurrFetchCol->copyDiskColumn)( 
                                sCurrFetchCol->column->size,
                                aDestRowBuf + sCurrFetchCol->column->offset,
                                0, /* aDestValueOffset */
                                sDummyValue->length,
                                sDummyValue->value ) 
                      == IDE_SUCCESS);
        }
        else
        {
            /* special fetch for index */

            IDE_TEST( aIndexInfo4Fetch->mCallbackFunc4Index(
                                                 sCurrFetchCol->column,
                                                 0, /* aCopyOffset */
                                                 sDummyValue,
                                                 aIndexInfo4Fetch)
                      != IDE_SUCCESS );
        }

        aFetchStatus->mFetchDoneColCount++;
        aFetchStatus->mFstColumnSeq++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
void sdcRow::initRowFetchStatus( const smiFetchColumnList    *aFetchColumnList,
                                 sdcRowFetchStatus           *aFetchStatus )
{
    const smiFetchColumnList    *sFetchColumnList;
    UShort                       sFetchColCount = 0;

    IDE_ASSERT( aFetchStatus != NULL );

    sFetchColumnList = aFetchColumnList;

    while(sFetchColumnList != NULL)
    {
        sFetchColCount++;
        sFetchColumnList = sFetchColumnList->next;
    }

    aFetchStatus->mTotalFetchColCount  = sFetchColCount;

    aFetchStatus->mFetchDoneColCount   = 0;
    aFetchStatus->mFstColumnSeq        = 0;
    aFetchStatus->mAlreadyCopyedSize   = 0;
    aFetchStatus->mFstFetchConlumn     = aFetchColumnList;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
void sdcRow::resetRowFetchStatus( const sdcRowHdrInfo * aRowHdrInfo,
                                  UShort                aFetchColumnCnt,
                                  smiFetchColumnList  * aLstFetchedColumn,
                                  UShort                aLstFetchedColumnLen,
                                  sdcRowFetchStatus   * aFetchStatus )
{
    idBool                 sIsSpltLstColumn;
    UShort                 sColCount;
    UShort                 sColumnSeq;
    UChar                  sRowFlag;

    IDE_DASSERT( aRowHdrInfo       != NULL );
    IDE_DASSERT( aFetchStatus      != NULL );
    IDE_DASSERT( (aLstFetchedColumn != NULL) || 
                ((aLstFetchedColumn == NULL) && ( aFetchColumnCnt == 0 )));

    sColCount = aRowHdrInfo->mColCount;
    sRowFlag  = aRowHdrInfo->mRowFlag;

    if ( aFetchColumnCnt != 0 )
    {
        sColumnSeq = SDC_GET_COLUMN_SEQ( aLstFetchedColumn->column );

        if ( ( (sColumnSeq - aFetchStatus->mFstColumnSeq) == (sColCount - 1 ) ) &&
             ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE ))
        {
            sIsSpltLstColumn               = ID_TRUE;
            aFetchStatus->mFstFetchConlumn = aLstFetchedColumn;
        }
        else
        {
            sIsSpltLstColumn               = ID_FALSE;
            aFetchStatus->mFstFetchConlumn = aLstFetchedColumn->next;
        }
    }
    else
    {
        sIsSpltLstColumn = ID_FALSE;
    }

    // reset mFstColumnSeq
    if ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG )
         == ID_TRUE )
    {
        aFetchStatus->mFstColumnSeq += (sColCount - 1);
    }
    else
    {
        aFetchStatus->mFstColumnSeq += sColCount;
    }

    // reset mAlreadyCopyedSize
    if ( SM_IS_FLAG_ON(sRowFlag, SDC_ROWHDR_N_FLAG) && 
         (sIsSpltLstColumn == ID_TRUE) )
    {
        if ( ( sColCount == 1 ) &&
             ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_P_FLAG )
               == ID_TRUE ) )
        {
            aFetchStatus->mAlreadyCopyedSize += aLstFetchedColumnLen;
        }
        else
        {
            aFetchStatus->mAlreadyCopyedSize  = aLstFetchedColumnLen;
        }
    }
    else
    {
        aFetchStatus->mAlreadyCopyedSize = 0;
    }

    // reset mFetchDoneColCount
    if ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG )
         == ID_TRUE )
    {
        /* mColumn�� ���� ���� fetch �÷����� ���θ� �Ǵ��Ѵ�.
         * mColumn == NULL : fetch �÷� X
         * mColumn != NULL : fetch �÷� O */
        if ( sIsSpltLstColumn == ID_FALSE )
        {
            /* last column�� fetch �÷��� �ƴ� ��� */
            aFetchStatus->mFetchDoneColCount += aFetchColumnCnt;
        }
        else
        {
            /* last column�� fetch �÷��� ��� */
            aFetchStatus->mFetchDoneColCount += (aFetchColumnCnt - 1);
        }
    }
    else
    {
        aFetchStatus->mFetchDoneColCount += aFetchColumnCnt;
    }
}


/***********************************************************************
 * FUNCTION DESCRIPTION : sdcRecord::lockRecord( )
 *   !!!�� �Լ��� Repeatable Read�� Select-For-Update�ÿ��� ȣ��ȴ�.!!!
 *   �� �Լ��� Ư�� Record�� dummy update�� �����Ͽ� Record Level
 *   Lock�� �����Ѵ�.
 *   �� �Լ������� ������ Row Header�� TID�� BeginSCN�� ���� Stmt��
 *   ������ ����Ǹ� Row ��ü�� ������ �������� �ʴ´�.
 *   �� �Լ������� ������ Undo Log Record�� Update column count�� 0��
 *   ���� �����ϸ� ������ Update undo log�� �����ϴ�.
 *   �� �Լ��� isUpdatable()�Լ��� ����� �Ŀ� ȣ���Ͽ��� �Ѵ�.
 *
 * ���ο� undo log slot�� �Ҵ�޴´�.
 * undo log record�� �� undo no�� �Ҵ��Ѵ�.
 * undo log record�� log type�� UPDATE type���� �Ѵ�.
 * undo log record�� column count �� 0���� �Ѵ�.
 * undo log record�� aCurRecPtr->TID, aCurRecPtr->beginSCN,
 *     aCurRecPtr->rollPtr�� �����Ѵ�.
 *
 * aCurRecPtr->TID�� aMyTssSlot���� �����Ѵ�.
 * aCurRecPtr->beginSCN�� aMyStmtSCN���� �����Ѵ�.
 * aCurRecPtr->rollPtr�� �Ҵ� ���� undo record�� RID�� �����Ѵ�.
 **********************************************************************/
IDE_RC sdcRow::lock( idvSQL       *aStatistics,
                     UChar        *aSlotPtr,
                     sdSID         aSlotSID,
                     smSCN        *aInfiniteSCN,
                     sdrMtx       *aMtx,
                     UInt          aCTSlotIdx,
                     idBool*       aSkipLockRec )
{
    sdrMtxStartInfo      sStartInfo;
    sdSID                sMyTSSlotSID;
    smSCN                sMyFstDskViewSCN;
    UChar              * sPagePtr;
    smOID                sTableOID;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_DASSERT( isDeleted(aSlotPtr) == ID_FALSE );
    IDE_DASSERT( isHeadRowPiece(aSlotPtr) == ID_TRUE );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    IDE_TEST( smxTrans::allocTXSegEntry( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    sMyFstDskViewSCN = smxTrans::getFstDskViewSCN( aMtx->mTrans );
    sMyTSSlotSID     = smxTrans::getTSSlotSID( aMtx->mTrans );

    // BUG-15074
    // ���� tx�� lock record�� �ι� ȣ���� �� �ΰ��� lock record��
    // �����Ǵ� ���� �����ϱ� ���� �̹� lock�� �ִ��� �˻��Ѵ�.
    IDE_TEST( isAlreadyMyLockExistOnRow(
                       &sMyTSSlotSID,
                       &sMyFstDskViewSCN,
                       aSlotPtr,
                       aSkipLockRec ) != IDE_SUCCESS );

    if ( *aSkipLockRec == ID_FALSE )
    {
        sPagePtr    = sdpPhyPage::getPageStartPtr( (void*)aSlotPtr );
        sTableOID   = sdpPhyPage::getTableOID( sPagePtr );

        IDE_TEST( lockRow( aStatistics,
                           aMtx,
                           &sStartInfo,
                           aInfiniteSCN,
                           sTableOID,
                           aSlotPtr,
                           aSlotSID,
                           *(UChar*)SDC_GET_ROWHDR_FIELD_PTR(
                               aSlotPtr,
                               SDC_ROWHDR_CTSLOTIDX ),
                           aCTSlotIdx,
                           ID_TRUE ) /* aIsExplicitLock */
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do...
    }

    IDU_FIT_POINT( "BUG-45401@sdcRow::lock" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  row level lock���� �ΰ��� ������ �ִ�.
 *  1. explicit lock
 *   -> select for update, repeatable read
 *
 *  2. implicit lock
 *   -> update�ÿ� head rowpiece�� update�� �÷��� �ϳ��� ���� ���,
 *      MVCC�� ���ؼ� head rowpiece�� implicit lock�� �Ǵ�.
 *
 * aStatistics        - [IN] �������
 * aMtx               - [IN] Mtx ������
 * aStartInfo         - [IN] Mtx StartInfo
 * aCSInfiniteSCN     - [IN] Cursor Infinite SCN ������
 * aTableOID          - [IN] Table OID
 * aSlotPtr           - [IN] Row Slot ������
 * aCTSlotIdxBfrLock  - [IN] Lock Row �ϱ����� Row�� CTSlotIdx
 * aCTSLotIdxAftLock  - [IN] Lock Row ������ Row�� CTSlotIdx
 * aIsExplicitLock    - [IN] Implicit/Explicit Lock ����
 *
 **********************************************************************/
IDE_RC sdcRow::lockRow( idvSQL                      *aStatistics,
                        sdrMtx                      *aMtx,
                        sdrMtxStartInfo             *aStartInfo,
                        smSCN                       *aCSInfiniteSCN,
                        smOID                        aTableOID,
                        UChar                       *aSlotPtr,
                        sdSID                        aSlotSID,
                        UChar                        aCTSlotIdxBfrLock,
                        UChar                        aCTSlotIdxAftLock,
                        idBool                       aIsExplicitLock )
{
    sdcRowHdrInfo    sOldRowHdrInfo;
    sdcRowHdrInfo    sNewRowHdrInfo;
    scGRID           sSlotGRID;
    sdSID            sUndoSID = SD_NULL_SID;
    UChar            sNewCTSlotIdx;
    smSCN            sNewRowCommitSCN;
    smSCN            sFstDskViewSCN;
    scSpaceID        sSpaceID;
    sdcUndoSegment * sUDSegPtr;

    IDE_ASSERT( aMtx                != NULL );
    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aCSInfiniteSCN      != NULL );
    IDE_ASSERT( aSlotPtr            != NULL );
    IDE_DASSERT( isDeleted(aSlotPtr) == ID_FALSE );

    sSpaceID = sdpPhyPage::getSpaceID( sdpPhyPage::getPageStartPtr(aSlotPtr));

    SC_MAKE_GRID_WITH_SLOTNUM( sSlotGRID,
                               sSpaceID,
                               SD_MAKE_PID(aSlotSID),
                               SD_MAKE_SLOTNUM(aSlotSID) );

    sUDSegPtr = smxTrans::getUDSegPtr( (smxTrans*)(aStartInfo->mTrans) );
    if ( sUDSegPtr->addLockRowUndoRec( aStatistics,
                                       aStartInfo,
                                       aTableOID,
                                       aSlotPtr,
                                       sSlotGRID,
                                       aIsExplicitLock,
                                       &sUndoSID ) != IDE_SUCCESS )
    {
        /* Undo ������������ ������ ���. MtxUndo �����ص� �� */
        sdrMiniTrans::setIgnoreMtxUndo( aMtx );
        IDE_TEST( 1 );
    }

    getRowHdrInfo( aSlotPtr, &sOldRowHdrInfo );

    sNewCTSlotIdx = aCTSlotIdxAftLock;

    if ( aIsExplicitLock == ID_TRUE )
    {
        /* Explicit Lock�� RowHdr�� LockBit�� �����ϰ�,
         * CommitSCN�� �������� �ʴ´�. */
        SDC_SET_CTS_LOCK_BIT( sNewCTSlotIdx );
        SM_SET_SCN( &sNewRowCommitSCN, aCSInfiniteSCN );
    }
    else
    {
        SM_SET_SCN( &sNewRowCommitSCN, aCSInfiniteSCN );
    }

    sFstDskViewSCN = smxTrans::getFstDskViewSCN(aStartInfo->mTrans);

    SDC_INIT_ROWHDR_INFO( &sNewRowHdrInfo,
                          sNewCTSlotIdx,
                          sNewRowCommitSCN,
                          sUndoSID,
                          sOldRowHdrInfo.mColCount,
                          sOldRowHdrInfo.mRowFlag,
                          sFstDskViewSCN );

    writeRowHdr( aSlotPtr, &sNewRowHdrInfo );

    sdrMiniTrans::setRefOffset(aMtx, aTableOID);

    IDE_TEST( writeLockRowRedoUndoLog( aSlotPtr,
                                       sSlotGRID,
                                       aMtx,
                                       aIsExplicitLock )
              != IDE_SUCCESS );

    IDE_TEST( sdcTableCTL::bind( aMtx,
                                 sSpaceID,
                                 aSlotPtr,
                                 aCTSlotIdxBfrLock,
                                 aCTSlotIdxAftLock,
                                 SD_MAKE_SLOTNUM(aSlotSID),
                                 sOldRowHdrInfo.mExInfo.mFSCredit,
                                 0,         /* aFSCreditSize */
                                 ID_FALSE ) /* aIncDeleteRowCnt */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   SDR_SDC_LOCK_ROW
 *   -----------------------------------------------------------------------------------
 *   | [sdrLogHdr]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *   | [flag(1)]
 *   -----------------------------------------------------------------------------------
 *   | [row header]
 *   -----------------------------------------------------------------------------------
 *   | NULL
 *   -----------------------------------------------------------------------------------
 *
 **********************************************************************/
IDE_RC sdcRow::writeLockRowRedoUndoLog( UChar     *aSlotPtr,
                                        scGRID     aSlotGRID,
                                        sdrMtx    *aMtx,
                                        idBool     aIsExplicitLock )
{
    sdrLogType    sLogType;
    UShort        sLogSize;
    UChar         sFlag;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_LOCK_ROW;
    sLogSize = ID_SIZEOF(sFlag) + SDC_ROWHDR_SIZE;

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sFlag = 0;
    if ( aIsExplicitLock == ID_TRUE)
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_LOCK_TYPE_EXPLICIT;
    }
    else
    {
        sFlag |= SDC_UPDATE_LOG_FLAG_LOCK_TYPE_IMPLICIT;
    }
    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  &sFlag,
                                  ID_SIZEOF(sFlag) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  aSlotPtr,
                                  SDC_ROWHDR_SIZE )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  lock row undo�� ���� CLR�� ����ϴ� �Լ��̴�.
 *  LOCK_ROW �α״� redo, undo �α��� ������ ���� ������
 *  undo record���� undo info layer�� ���븸 ����,
 *  clr redo log�� �ۼ��ϸ� �ȴ�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeLockRowCLR( const UChar     *aUndoRecHdr,
                                scGRID           aSlotGRID,
                                sdSID            aUndoSID,
                                sdrMtx          *aMtx )
{
    const UChar        *sUndoRecBodyPtr;
    UChar              *sSlotDirStartPtr;
    sdrLogType          sLogType;
    UShort              sLogSize;

    IDE_ASSERT( aUndoRecHdr != NULL );
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sLogType = SDR_SDC_LOCK_ROW;
    sSlotDirStartPtr = sdpPhyPage::getSlotDirStartPtr( 
                                      (UChar*)aUndoRecHdr );

    /* undo info layer�� ũ�⸦ ����. */
    sLogSize = sdcUndoSegment::getUndoRecSize( 
                               sSlotDirStartPtr,
                               SD_MAKE_SLOTNUM( aUndoSID ) );

    IDE_TEST(sdrMiniTrans::writeLogRec( aMtx,
                                        aSlotGRID,
                                        NULL,
                                        sLogSize,
                                        sLogType )
             != IDE_SUCCESS);

    sUndoRecBodyPtr = (const UChar*)
        sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

    IDE_TEST( sdrMiniTrans::write(aMtx,
                                  (void*)sUndoRecBodyPtr,
                                  sLogSize)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  lock row ���꿡 ���� redo or undo�� �����ϴ� �Լ��̴�.
 *  LOCK_ROW �α״� redo, undo �α��� ������ ����.
 *
 **********************************************************************/
IDE_RC sdcRow::redo_undo_LOCK_ROW( sdrMtx              *aMtx,
                                   UChar               *aLogPtr,
                                   UChar               *aSlotPtr,
                                   sdcOperToMakeRowVer  aOper4RowPiece )
{
    UChar             *sLogPtr         = aLogPtr;
    UChar             *sSlotPtr        = aSlotPtr;
    UChar              sFlag;

    IDE_ERROR( aLogPtr  != NULL );
    IDE_ERROR( aSlotPtr != NULL );
    IDE_ERROR( isHeadRowPiece(aSlotPtr) == ID_TRUE );

    ID_READ_1B_AND_MOVE_PTR( sLogPtr, &sFlag );

    if ( aOper4RowPiece == SDC_UNDO_MAKE_OLDROW )
    {
        IDE_TEST( sdcTableCTL::unbind( aMtx,
                                       sSlotPtr,
                                       *(UChar*)sSlotPtr,
                                       *(UChar*)sLogPtr,
                                       0          /* aFSCreditSize */,
                                       ID_FALSE ) /* aDecDeleteRowCnt */
                  != IDE_SUCCESS );
    }

    ID_WRITE_AND_MOVE_BOTH( sSlotPtr,
                            sLogPtr,
                            SDC_ROWHDR_SIZE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : �� Ʈ������� �������� RowPiece ���� ��ȯ
 *
 * << �ڽ��� �������� Row���� Ȯ���ϴ� ��� >>
 *
 * ���δٸ� Ʈ������� TSS RID�� ������ �� �־ TxBSCN�� �����Ҽ� ������,
 * ����, TxBSCN�� �����ص� TSS RID�� �����Ҽ� ����.
 * �ֳ��ϸ�, ������ FstDskViewSCN�� ���� Ʈ����ǰ����� TSS Slot�� ������
 * �Ұ����ѵ�, TSS ���׸�Ʈ�� ExtDir�� ������� ���ϱ� �����̴�.
 *
 *
 * << Legacy Transaction�� commit ���� update�� row�� ID_FALSE ���� >>
 *
 * Legacy Transaction�� ���, commit �ϱ� ���� update�� row�� commit �Ŀ�
 * FAC�� �ٽ� �����ϴ� ��찡 �����. Legacy transaction�� commit �� �ڽ���
 * �ʱ�ȭ �ϸ鼭 mTXSegEntry�� NULL�� �ʱ�ȭ �ϱ� ������ TSSlotSID�� NULL�̴�.
 * ���� commit ���Ŀ� �ش� row�� �����ؼ� �� �Լ�, isMyTransUpdating()��
 * ȣ���ϴ���, �ڽ��� TSSlotSID�� NULL�̱� ������ ID_FALSE�� �����Ѵ�.
 *
 * �ڽ��� commit �ϱ� ������ �����ߴ� row���� ���θ� �Ǵ��ϱ� ���ؼ���
 * sdcRow::isMyLegacyTransUpdated() �Լ��� ����ؾ� �Ѵ�.
 *
 *
 * aPagePtr         - [IN] ������ Ptr
 * aSlotPtr         - [IN] RowPiece Slot Ptr
 * aMyFstDskViewSCN - [IN] Ʈ������� ù��° Disk Stmt�� ViewSCN
 * aMyTSSlotSID     - [IN] Ʈ������� TSSlot SID
 *
 ***********************************************************************/
idBool sdcRow::isMyTransUpdating( UChar    * aPagePtr,
                                  UChar    * aSlotPtr,
                                  smSCN      aMyFstDskViewSCN,
                                  sdSID      aMyTSSlotSID,
                                  smSCN    * aFstDskViewSCN )
{
    sdpCTS        * sCTS;
    UChar           sCTSlotIdx;
    smSCN           sFstDskViewSCN;
    idBool          sIsMyTrans = ID_FALSE;
    sdcRowHdrExInfo sRowHdrExInfo;
    scPageID        sMyTSSlotPID; 
    scSlotNum       sMyTSSlotNum;
    scPageID        sRowTSSlotPID; 
    scSlotNum       sRowTSSlotNum;

    IDE_DASSERT( aSlotPtr != NULL );
    IDE_ASSERT( SM_SCN_IS_VIEWSCN(aMyFstDskViewSCN) == ID_TRUE );
    
    SM_INIT_SCN( &sFstDskViewSCN );

    SDC_GET_ROWHDR_1B_FIELD( aSlotPtr,
                             SDC_ROWHDR_CTSLOTIDX,
                             sCTSlotIdx );

    sCTSlotIdx = SDC_UNMASK_CTSLOTIDX( sCTSlotIdx );

    /* TASK-6105 IDE_TEST_RAISE�� ���� ����� ũ�� ������ if������
     * �ٷ� return �Ѵ�. */
    if ( sCTSlotIdx == SDP_CTS_IDX_UNLK )
    {
        SM_SET_SCN( aFstDskViewSCN, &sFstDskViewSCN );

        return sIsMyTrans;
    }

    sMyTSSlotPID = SD_MAKE_PID( aMyTSSlotSID );
    sMyTSSlotNum = SD_MAKE_SLOTNUM( aMyTSSlotSID );

    if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
    {
        sCTS = sdcTableCTL::getCTS( (sdpPhyPageHdr*)aPagePtr,
                                    sCTSlotIdx );

        sRowTSSlotPID = sCTS->mTSSPageID;
        sRowTSSlotNum = sCTS->mTSSlotNum;

        SM_SET_SCN( &sFstDskViewSCN, &sCTS->mFSCNOrCSCN );
    }
    else
    {
        if ( sCTSlotIdx != SDP_CTS_IDX_NULL )
        {
            ideLog::log( IDE_ERR_0,
                         "CTSlotIdx: %"ID_xINT32_FMT"\n",
                         sCTSlotIdx );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aPagePtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        getRowHdrExInfo( aSlotPtr, &sRowHdrExInfo );

        sRowTSSlotPID = sRowHdrExInfo.mTSSPageID;
        sRowTSSlotNum = sRowHdrExInfo.mTSSlotNum;

        SM_SET_SCN( &sFstDskViewSCN, &sRowHdrExInfo.mFSCNOrCSCN );
    }

    if ( (SM_SCN_IS_EQ( &aMyFstDskViewSCN, &sFstDskViewSCN )) &&
         (sRowTSSlotPID == sMyTSSlotPID) &&
         (sRowTSSlotNum == sMyTSSlotNum) )
    {
        sIsMyTrans = ID_TRUE;
    }

    SM_SET_SCN( aFstDskViewSCN, &sFstDskViewSCN );

    return sIsMyTrans;
}


/***********************************************************************
 * Description : Legacy Cursor�� ���� transaction�� ���� �� Ŀ���� RowPiece
 *              ���� ��ȯ
 *
 *  Legacy Cursor�� InfiniteSCN�� TID�� row piece�� InfiniteSCN�� TID��
 * ��ġ�ϸ� �ϴ� LegacyTrans�� update & commit ���� ���ɼ��� �ִ�. �׷���
 * TID�� ���� �� �� �����Ƿ�, LegacyTransMgr�� �ش� TID�� LegacyTrans��
 * CommitSCN�� �޾ƿͼ� �ѹ� �� Ȯ���Ѵ�.
 *  ���� ��� row piece�� data page�� ���� �ִٸ� CTS�� ���� row piece��
 * CommitSCN�� ȹ���ϰ�, undo page�� ���� �ִٸ� undo record�κ��� row piece��
 * CommitSCN�� ���� �� �ִ�. �̷��� ���� row piece�� CommitSCN�� LegacyTrans��
 * CommitSCN�� ���Ͽ� ��ġ�ϸ� ���������� ��� row piece�� update & commit ��
 * ���� ���� ��� row piece�� ������ �ϴ� Legacy Cursor���� Ȯ���� �� �ִ�.
 *
 * aTrans           - [IN] ��ȸ�� ��û�ϴ� Ʈ������� ������
 * aLegacyCSInfSCN  - [IN] Legacy Cursor�� InfiniteSCN
 * aRowInfSCN       - [IN] Ȯ�� ��� record�� InfiniteSCN
 * aRowCommitSCN    - [IN] Ȯ�� ��� record�� CommitSCN
 ***********************************************************************/
idBool sdcRow::isMyLegacyTransUpdated( void   * aTrans,
                                       smSCN    aLegacyCSInfSCN,
                                       smSCN    aRowInfSCN,
                                       smSCN    aRowCommitSCN )
{
    idBool  sIsMyLegacyTrans    = ID_FALSE;
    smSCN   sLegacyCommitSCN;

    ACP_UNUSED( aRowInfSCN );
    IDE_DASSERT( SM_SCN_IS_INFINITE( aLegacyCSInfSCN ) == ID_TRUE );
    IDE_DASSERT( SM_SCN_IS_INFINITE( aRowInfSCN ) == ID_TRUE );
    IDE_DASSERT( SM_SCN_IS_NOT_INFINITE( aRowCommitSCN ) == ID_TRUE );

    IDE_TEST_CONT( smxTrans::getLegacyTransCnt(aTrans) == 0,
                    is_not_legacy_trans );

    sLegacyCommitSCN = smLayerCallback::getLegacyTransCommitSCN( SM_GET_TID_FROM_INF_SCN(aLegacyCSInfSCN) );

    if ( SM_SCN_IS_EQ( &sLegacyCommitSCN, &aRowCommitSCN ) )
    {
        sIsMyLegacyTrans = ID_TRUE;
    }
    else
    {
        /* do nothing */
    }

    IDE_EXCEPTION_CONT( is_not_legacy_trans );
    
    return sIsMyLegacyTrans;
}

/***********************************************************************
 * Description :
 *     slot�� �̹� �� lock record�� �ְų� Ȥ�� update ���̶��
 *     lock record�� ������ �ʿ����.
 *     �� �Լ��� lockRecord()�� ȣ���ϱ� ���� �ҷ�����.
 ***********************************************************************/
IDE_RC sdcRow::isAlreadyMyLockExistOnRow( sdSID     * aMyTSSlotSID,
                                          smSCN     * aMyFstDskViewSCN,
                                          UChar     * aSlotPtr,
                                          idBool    * aRetExist )
{
    idBool   sRetExist;
    idBool   sIsMyTrans;
    smSCN    sFstDskViewSCN; 

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aRetExist   != NULL );

    sRetExist = ID_FALSE;

    if ( SDC_IS_CTS_LOCK_BIT(
             *SDC_GET_ROWHDR_FIELD_PTR( aSlotPtr, SDC_ROWHDR_CTSLOTIDX ) ) )
    {
        sIsMyTrans = isMyTransUpdating( sdpPhyPage::getPageStartPtr(aSlotPtr),
                                        aSlotPtr,
                                        *aMyFstDskViewSCN,
                                        *aMyTSSlotSID,
                                        &sFstDskViewSCN );

        if ( sIsMyTrans == ID_TRUE )
        {
            sRetExist = ID_TRUE;
        }
    }

    *aRetExist = sRetExist;

    return IDE_SUCCESS;
}


/***********************************************************************
 *
 * Description :
 *  insert rowpiece ����ÿ�, rowpiece ������ slot�� ����Ѵ�.
 *
 **********************************************************************/
UChar* sdcRow::writeRowPiece4IRP( UChar                       *aWritePtr,
                                  const sdcRowHdrInfo         *aRowHdrInfo,
                                  const sdcRowPieceInsertInfo *aInsertInfo,
                                  sdSID                        aNextRowPieceSID )
{
    UChar                        *sWritePtr;
    UShort                        sCurrColSeq;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aRowHdrInfo != NULL );
    IDE_ASSERT( aInsertInfo != NULL );

    sWritePtr = aWritePtr;

    /* row header ������ write �Ѵ�. */
    sWritePtr = writeRowHdr(sWritePtr, aRowHdrInfo);

    if ( aInsertInfo->mColCount == 0 )
    {
        if ( ( aInsertInfo->mIsInsert4Upt == ID_TRUE ) ||
             ( aInsertInfo->mRowPieceSize != SDC_ROWHDR_SIZE ) ||
             ( aRowHdrInfo->mRowFlag != SDC_ROWHDR_FLAG_NULLROW ) )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aRowHdrInfo,
                            ID_SIZEOF(sdcRowHdrInfo),
                            "RowHdrInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aInsertInfo,
                            ID_SIZEOF(sdcRowPieceInsertInfo),
                            "InsertInfo Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aWritePtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        /* NULL ROW�� ��� row header�� ����ϰ� skip �Ѵ�. */
        IDE_CONT( skip_null_row );
    }

    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        sWritePtr = writeNextLink( sWritePtr, aNextRowPieceSID );
    }

    for ( sCurrColSeq = aInsertInfo->mStartColSeq;
          sCurrColSeq <= aInsertInfo->mEndColSeq;
          sCurrColSeq++ )
    {
        sWritePtr = writeColPiece4Ins( sWritePtr,
                                       aInsertInfo,
                                       sCurrColSeq );
    }

    IDE_EXCEPTION_CONT( skip_null_row );

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *  update rowpiece ����ÿ�, rowpiece ������ slot�� ����Ѵ�.
 *
 **********************************************************************/
UChar* sdcRow::writeRowPiece4URP( UChar                       *aWritePtr,
                                  const sdcRowHdrInfo         *aRowHdrInfo,
                                  const sdcRowPieceUpdateInfo *aUpdateInfo,
                                  sdSID                        aNextRowPieceSID )

{
    UChar                        *sWritePtr;
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount = aRowHdrInfo->mColCount;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aRowHdrInfo != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    sWritePtr = aWritePtr;
    sWritePtr = writeRowHdr( sWritePtr, aRowHdrInfo );

    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        sWritePtr = writeNextLink( sWritePtr, aNextRowPieceSID );
    }

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            /* update �÷��� ���, new value�� ����Ѵ�. */
            sWritePtr = writeColPiece( sWritePtr,
                                       (UChar*)sColumnInfo->mNewValueInfo.mValue.value,
                                       sColumnInfo->mNewValueInfo.mValue.length,
                                       sColumnInfo->mNewValueInfo.mInOutMode );
        }
        else
        {
            /* update �÷��� �ƴϸ�, old value�� ����Ѵ�. */
            sWritePtr = writeColPiece( sWritePtr,
                                       (UChar*)sColumnInfo->mOldValueInfo.mValue.value,
                                       sColumnInfo->mOldValueInfo.mValue.length,
                                       sColumnInfo->mOldValueInfo.mInOutMode );
        }
    }

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *  overwrite rowpiece ����ÿ�, rowpiece ������ slot�� ����Ѵ�.
 *
 **********************************************************************/
UChar* sdcRow::writeRowPiece4ORP(
                    UChar                           *aWritePtr,
                    const sdcRowHdrInfo             *aRowHdrInfo,
                    const sdcRowPieceOverwriteInfo  *aOverwriteInfo,
                    sdSID                            aNextRowPieceSID )
{
    UChar                        *sWritePtr;
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sColCount = aRowHdrInfo->mColCount;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aWritePtr        != NULL );
    IDE_ASSERT( aRowHdrInfo      != NULL );
    IDE_ASSERT( aOverwriteInfo   != NULL );

    sWritePtr = aWritePtr;

    sWritePtr = writeRowHdr( sWritePtr, aRowHdrInfo );

    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        sWritePtr = writeNextLink( sWritePtr, aNextRowPieceSID );
    }

    if ( SDC_IS_HEAD_ONLY_ROWPIECE(aRowHdrInfo->mRowFlag) == ID_TRUE )
    {
        /* row migration�� �߻��� ���,
         * row header�� ����ϰ� skip�Ѵ�. */
        if ( aRowHdrInfo->mColCount != 0 )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aRowHdrInfo,
                            ID_SIZEOF(sdcRowHdrInfo),
                            "RowHdrInfo Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aWritePtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        IDE_CONT( skip_head_only_rowpiece );
    }

    /* last column�� ���� column������ loop�� ����. */
    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount - 1;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aOverwriteInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            /* update �÷��� ���, new value�� ����Ѵ�. */
            sWritePtr = writeColPiece( sWritePtr,
                                       (UChar*)sColumnInfo->mNewValueInfo.mValue.value,
                                       sColumnInfo->mNewValueInfo.mValue.length,
                                       sColumnInfo->mNewValueInfo.mInOutMode );
        }
        else
        {
            /* update �÷��� �ƴϸ�, old value�� ����Ѵ�. */
            sWritePtr = writeColPiece( sWritePtr,
                                       (UChar*)sColumnInfo->mOldValueInfo.mValue.value,
                                       sColumnInfo->mOldValueInfo.mValue.length,
                                       sColumnInfo->mOldValueInfo.mInOutMode );
        }
    }

    /* overwrite rowpiece ������ ���
     * split�� �߻����� �� �ֱ� ������, last column�� �����Ҷ�
     * �߷��� ũ��(mLstColumnOverwriteSize)��ŭ�� �����ؾ� �Ѵ�. */
    sColumnInfo = aOverwriteInfo->mColInfoList + (sColCount - 1);

    if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
    {
        /* update �÷��� ���, new value�� ����Ѵ�. */
        sWritePtr = writeColPiece( sWritePtr,
                                   (UChar*)sColumnInfo->mNewValueInfo.mValue.value,
                                   aOverwriteInfo->mLstColumnOverwriteSize,
                                   sColumnInfo->mNewValueInfo.mInOutMode );
    }
    else
    {
        /* update �÷��� �ƴϸ�, old value�� ����Ѵ�. */
        sWritePtr = writeColPiece( sWritePtr,
                                   (UChar*)sColumnInfo->mOldValueInfo.mValue.value,
                                   aOverwriteInfo->mLstColumnOverwriteSize,
                                   sColumnInfo->mOldValueInfo.mInOutMode );
    }


    IDE_EXCEPTION_CONT(skip_head_only_rowpiece);

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UChar* sdcRow::writeRowHdr( UChar                *aWritePtr,
                            const sdcRowHdrInfo  *aRowHdrInfo )
{
    scPageID    sPID;
    scSlotNum   sSlotNum;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aRowHdrInfo != NULL );

    SDC_SET_ROWHDR_1B_FIELD( aWritePtr,
                             SDC_ROWHDR_CTSLOTIDX,
                             aRowHdrInfo->mCTSlotIdx );

    SDC_SET_ROWHDR_FIELD( aWritePtr,
                          SDC_ROWHDR_COLCOUNT,
                          &(aRowHdrInfo->mColCount) );

    SDC_SET_ROWHDR_FIELD( aWritePtr,
                          SDC_ROWHDR_INFINITESCN,
                          &(aRowHdrInfo->mInfiniteSCN) );

    sPID     = SD_MAKE_PID(aRowHdrInfo->mUndoSID);
    sSlotNum = SD_MAKE_SLOTNUM(aRowHdrInfo->mUndoSID);

    SDC_SET_ROWHDR_FIELD(aWritePtr, SDC_ROWHDR_UNDOPAGEID, &sPID);
    SDC_SET_ROWHDR_FIELD(aWritePtr, SDC_ROWHDR_UNDOSLOTNUM, &sSlotNum);

    SDC_SET_ROWHDR_1B_FIELD(aWritePtr, 
                            SDC_ROWHDR_FLAG, 
                            aRowHdrInfo->mRowFlag);

    writeRowHdrExInfo(aWritePtr, &aRowHdrInfo->mExInfo);

    return (aWritePtr + SDC_ROWHDR_SIZE);
}

/***********************************************************************
 *
 * Description : RowPiece�� ��������� Ȯ������(����Ʈ�����)�� ����Ѵ�.
 *
 *
 **********************************************************************/
UChar* sdcRow::writeRowHdrExInfo( UChar                 * aSlotPtr,
                                  const sdcRowHdrExInfo * aRowHdrExInfo )
{
    IDE_ASSERT( aSlotPtr      != NULL );
    IDE_ASSERT( aRowHdrExInfo != NULL );

    SDC_SET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_TSSLOTPID,
                          &(aRowHdrExInfo->mTSSPageID) );

    SDC_SET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_TSSLOTNUM,
                          &(aRowHdrExInfo->mTSSlotNum) );

    SDC_SET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_FSCREDIT,
                          &(aRowHdrExInfo->mFSCredit));

    SDC_SET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_FSCNORCSCN,
                          &(aRowHdrExInfo->mFSCNOrCSCN) );

    return (aSlotPtr + SDC_ROWHDR_SIZE);
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UChar* sdcRow::writeNextLink( UChar    *aWritePtr,
                              sdSID     aNextRowPieceSID )
{
    UChar                        *sWritePtr;
    scPageID                      sPageId;
    scSlotNum                     sSlotNum;

    IDE_ASSERT( aWritePtr        != NULL );
    IDE_ASSERT( aNextRowPieceSID != SD_NULL_SID );

    sWritePtr = aWritePtr;

    /* next page id(4byte) */
    sPageId = SD_MAKE_PID(aNextRowPieceSID);
    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            (UChar*)&sPageId,
                            ID_SIZEOF(sPageId) );

    /* next slot num(2byte) */
    sSlotNum = SD_MAKE_SLOTNUM(aNextRowPieceSID);
    ID_WRITE_AND_MOVE_DEST( sWritePtr,
                            (UChar*)&sSlotNum,
                            ID_SIZEOF(sSlotNum) );

    return sWritePtr;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UChar* sdcRow::writeColPiece4Ins( UChar                       *aWritePtr,
                                  const sdcRowPieceInsertInfo *aInsertInfo,
                                  UShort                       aColSeq )
{
    const sdcColumnInfo4Insert   *sColumnInfo;
    UChar                        *sWritePtr;
    UChar                        *sSrcPtr;
    UInt                          sStartOffset;
    UInt                          sEndOffset;
    UShort                        sSize;

    IDE_ASSERT( aWritePtr   != NULL );
    IDE_ASSERT( aInsertInfo != NULL );

    sWritePtr   = aWritePtr;
    sColumnInfo = aInsertInfo->mColInfoList + aColSeq;

    if ( aColSeq == aInsertInfo->mStartColSeq )
    {
        sStartOffset = aInsertInfo->mStartColOffset;
    }
    else
    {
        sStartOffset = 0;
    }

    if ( aColSeq == aInsertInfo->mEndColSeq )
    {
        sEndOffset = aInsertInfo->mEndColOffset;
    }
    else
    {
        sEndOffset = sColumnInfo->mValueInfo.mValue.length;
    }

    sSize     = sEndOffset - sStartOffset;
    sSrcPtr   = (UChar*)(sColumnInfo->mValueInfo.mValue.value) + sStartOffset;

    sWritePtr = writeColPiece( sWritePtr,
                               sSrcPtr,
                               sSize,
                               sColumnInfo->mValueInfo.mInOutMode );

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *
 *  column length(1byte or 3byte)
 *
 *  (1) [0xFF(1)]
 *  (2) [0xFE(1)] [column length(2)] [column data]
 *  (3)           [column length(1)] [column data]
 *
 *
 *  (1) column ���̿� �����ϴ� NULL ���� 0xFF�� ǥ���Ѵ�.
 *  (2) column length�� 250byte���� ũ�ٸ�
 *      ó�� 1byte�� 0xFE�̰�, ������ 2byte��
 *      column length�̴�.
 *  (3) column length�� 250byte���� ������
 *      ó�� 1byte�� column length�̴�.
 *
 **********************************************************************/
UChar* sdcRow::writeColPiece( UChar             *aWritePtr,
                              const UChar       *aColValuePtr,
                              UShort             aColLen,
                              sdcColInOutMode    aInOutMode )
{
    UChar   *sWritePtr;
    UChar    sPrefix;
    UChar    sSmallLen;
    UShort   sLargeLen;

    IDE_ASSERT( aWritePtr    != NULL );

    sWritePtr = aWritePtr;

    if ( (aColValuePtr == NULL) && (aColLen == 0) )
    {
        /* NULL column */
        sPrefix = (UChar)SDC_NULL_COLUMN_PREFIX;
        ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sPrefix );
    }
    else
    {
        if ( aInOutMode == SDC_COLUMN_IN_MODE )
        {
            if ( aColLen > SDC_COLUMN_LEN_STORE_SIZE_THRESHOLD )
            {
                sPrefix = (UChar)SDC_LARGE_COLUMN_PREFIX;
                ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sPrefix );

                sLargeLen = (UShort)aColLen;
                ID_WRITE_AND_MOVE_DEST( sWritePtr, (UChar*)&sLargeLen, ID_SIZEOF(sLargeLen) );
            }
            else
            {
                sSmallLen = (UChar)aColLen;
                ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sSmallLen );
            }
        }
        else
        {
            if ( (aInOutMode != SDC_COLUMN_OUT_MODE_LOB) ||
                 (aColLen    != ID_SIZEOF(sdcLobDesc)) )
            {
                ideLog::log( IDE_ERR_0,
                             "InOutMode: %"ID_UINT32_FMT", "
                             "ColLen: %"ID_UINT32_FMT"\n",
                             aInOutMode,
                             aColLen );

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             sWritePtr,
                                             "Page Dump:" );

                IDE_ASSERT( 0 );
            }

            sPrefix = (UChar)SDC_LOB_DESC_COLUMN_PREFIX;
            ID_WRITE_1B_AND_MOVE_DEST( sWritePtr, &sPrefix );
        }

        /* write column value */
        if ( (aColValuePtr != NULL) && (aColLen > 0) )
        {
            ID_WRITE_AND_MOVE_DEST( sWritePtr, aColValuePtr, aColLen );
        }
    }

    return sWritePtr;
}


/***********************************************************************
 *
 * Description :
 *
 *  column length(1byte or 3byte)
 *
 *  (1) [0xFF(1)]
 *  (2) [0xFE(1)] [column length(2)] [column data]
 *  (3)           [column length(1)] [column data]
 *
 *
 *  (1) column ���̿� �����ϴ� NULL ���� 0xFF�� ǥ���Ѵ�.
 *  (2) column length�� 250byte���� ũ�ٸ�
 *      ó�� 1byte�� 0xFE�̰�, ������ 2byte��
 *      column length�̴�.
 *  (3) column length�� 250byte���� ������
 *      ó�� 1byte�� column length�̴�.
 *  (4) Out Mode LOB Column�̸� Empty Lob Desc�� ����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::writeColPieceLog( sdrMtx            *aMtx,
                                 UChar             *aColValuePtr,
                                 UShort             aColLen,
                                 sdcColInOutMode    aInOutMode )
{
    UChar    sPrefix;
    UChar    sSmallLen;
    UShort   sLargeLen;

    IDE_ASSERT( aMtx != NULL );

    if ( (aColValuePtr == NULL) && (aColLen == 0) )
    {
        /* NULL column */
        sPrefix = (UChar)SDC_NULL_COLUMN_PREFIX;
        IDE_TEST( sdrMiniTrans::write(aMtx,
                                      (void*)&sPrefix,
                                      ID_SIZEOF(sPrefix) )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aInOutMode == SDC_COLUMN_IN_MODE )
        {
            if ( aColLen > SDC_COLUMN_LEN_STORE_SIZE_THRESHOLD )
            {
                sPrefix = (UChar)SDC_LARGE_COLUMN_PREFIX;
                IDE_TEST( sdrMiniTrans::write(aMtx,
                                              (void*)&sPrefix,
                                              ID_SIZEOF(sPrefix) )
                          != IDE_SUCCESS );

                sLargeLen = (UShort)aColLen;
                IDE_TEST( sdrMiniTrans::write(aMtx,
                                              (void*)&sLargeLen,
                                              ID_SIZEOF(sLargeLen) )
                          != IDE_SUCCESS );
            }
            else
            {
                sSmallLen = (UChar)aColLen;
                IDE_TEST( sdrMiniTrans::write(aMtx,
                                              (void*)&sSmallLen,
                                              ID_SIZEOF(sSmallLen) )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            if ( (aInOutMode != SDC_COLUMN_OUT_MODE_LOB) ||
                 (aColLen    != ID_SIZEOF(sdcLobDesc)) )
            {
                ideLog::log( IDE_ERR_0,
                             "InOutMode: %"ID_UINT32_FMT", "
                             "ColLen: %"ID_UINT32_FMT"\n",
                             aInOutMode,
                             aColLen );
                IDE_ASSERT( 0 );
            }

            sPrefix = (UChar)SDC_LOB_DESC_COLUMN_PREFIX;
            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)&sPrefix,
                                          ID_SIZEOF(sPrefix) )
                      != IDE_SUCCESS );
        }

        /* write column value */
        if ( (aColValuePtr != NULL) && (aColLen > 0) )
        {
            IDE_TEST( sdrMiniTrans::write(aMtx,
                                          (void*)aColValuePtr,
                                          aColLen )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  insert into�� �м��ϰ� rowpiece�� ũ�⸦ ����ϸ鼭
 *  row�� � �κ��� �󸶳� ���������� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::analyzeRowForInsert( sdpPageListEntry      * aPageEntry,
                                    UShort                  aCurrColSeq,
                                    UInt                    aCurrOffset,
                                    sdSID                   aNextRowPieceSID,
                                    smOID                   aTableOID,
                                    sdcRowPieceInsertInfo  *aInsertInfo )
{
    const sdcColumnInfo4Insert   *sColumnInfo;
    const smiColumn              *sInsCol;
    const smiValue               *sInsVal;
    UShort                        sColSeq;
    UInt                          sColDataSize        = 0;
    UShort                        sMaxRowPieceSize;
    SShort                        sMinColPieceSize    = SDC_MIN_COLPIECE_SIZE;
    UShort                        sCurrRowPieceSize   = 0;
    SShort                        sStoreSize          = 0;
    UShort                        sLobDescCnt         = 0;

    IDE_ASSERT( aPageEntry  != NULL );
    IDE_ASSERT( aInsertInfo != NULL );

    sMaxRowPieceSize = SDC_MAX_ROWPIECE_SIZE(
                           sdpPageList::getSizeOfInitTrans( aPageEntry ) );
    sColSeq          = aCurrColSeq;
    aInsertInfo->mStartColSeq = aInsertInfo->mEndColSeq = sColSeq;

    sColDataSize = aCurrOffset;
    aInsertInfo->mStartColOffset = aInsertInfo->mEndColOffset = sColDataSize;

    sCurrRowPieceSize += SDC_ROWHDR_SIZE;
    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        sCurrRowPieceSize += SDC_EXTRASIZE_FOR_CHAINING;
    }

    while(1)
    {
        sColumnInfo = aInsertInfo->mColInfoList + sColSeq;
        sInsVal     = &(sColumnInfo->mValueInfo.mValue);
        sInsCol     = sColumnInfo->mColumn;

        /* FIT/ART/sm/Projects/PROJ-2118/insert/insert.ts */
        IDU_FIT_POINT( "1.PROJ-2118@sdcRow::analyzeRowForInsert" );

        IDE_ERROR_RAISE_MSG( (sInsVal->length <= sInsCol->size) &&
                             ( ((sInsVal->length > 0) && (sInsVal->value != NULL)) ||
                               (sInsVal->length == 0) ),
                             err_invalid_column_size,
                             "Table OID     : %"ID_vULONG_FMT"\n"
                             "Column Seq    : %"ID_UINT32_FMT"\n"
                             "Column Size   : %"ID_UINT32_FMT"\n"
                             "Column Offset : %"ID_UINT32_FMT"\n"
                             "Value Length  : %"ID_UINT32_FMT"\n",
                             aTableOID,
                             sColSeq,
                             sInsCol->size,
                             sInsCol->offset,
                             sInsVal->length );

        if ( (UInt)sMaxRowPieceSize >=
             ( (UInt)sCurrRowPieceSize +
               (UInt)SDC_GET_COLPIECE_SIZE(sColDataSize) ) )
        {
            /* column data�� ��� ������ �� �ִ� ��� */

            sCurrRowPieceSize += SDC_GET_COLPIECE_SIZE(sColDataSize);
            sColDataSize = 0;
            aInsertInfo->mStartColSeq    = sColSeq;
            aInsertInfo->mStartColOffset = sColDataSize;

            /* lob column counting */
            if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) != ID_TRUE )
            {
                sLobDescCnt++;
            }

            /* ���� ó���� column���� �̵� */
            if ( sColSeq == 0)
            {
                break;
            }
            else
            {
                sColSeq--;
                sColDataSize = getColDataSize2Store( aInsertInfo,
                                                     sColSeq );
            }
        }
        else
        {
            /* column data�� ��� ������ �� ���� ���
             * (������ �����ϵ簡, �ƴ� �ٸ� �������� �����ϵ簡 �ؾ���) */

            if ( isDivisibleColumn( sColumnInfo )  == ID_TRUE )
            {
                /* ���� rowpiece�� ������ ������ �� �ִ�
                 * �÷��� ��� ex) char, varchar */

                sStoreSize = sMaxRowPieceSize
                             - sCurrRowPieceSize
                             - SDC_MAX_COLUMN_LEN_STORE_SIZE;

                if ( (SShort)sStoreSize >= (SShort)sMinColPieceSize )
                {
                    /* column data�� ������ �����Ѵ�. */

                    sCurrRowPieceSize += SDC_GET_COLPIECE_SIZE(sStoreSize);
                    sColDataSize -= sStoreSize;
                    aInsertInfo->mStartColSeq    = sColSeq;
                    aInsertInfo->mStartColOffset = sColDataSize;
                }
                else
                {
                    /* ������ size�� min colpiece size���� ���� ���,
                     * column data�� ������ �������� �ʴ´�.
                     * �ٸ� �������� �����Ѵ�. */
                }
            }
            else
            {
                /* ���� rowpiece�� ������ ������ �� ����
                 * �÷��� ��� ex) ������ ������Ÿ�� */

                /* column data�� �ٸ� �������� �����ؾ� �Ѵ�. */
            }
            break;
        }
    }

    aInsertInfo->mRowPieceSize = sCurrRowPieceSize;
    aInsertInfo->mLobDescCnt   = sLobDescCnt;
    aInsertInfo->mColCount     =
        (aInsertInfo->mEndColSeq - aInsertInfo->mStartColSeq) + 1;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_COLUMN_SIZE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::analyzeRowForUpdate( UChar                  *aSlotPtr,
                                    const smiColumnList    *aColList,
                                    const smiValue         *aValueList,
                                    UShort                  aColCount,
                                    UShort                  aFstColumnSeq,
                                    UShort                  aLstUptColumnSeq,
                                    sdcRowPieceUpdateInfo  *aUpdateInfo )
{
    sdcColumnInfo4Update   *sColumnInfo;
    UChar                   sOldRowFlag;
    UInt                    sNewRowPieceSize       = 0;
    idBool                  sDeleteFstColumnPiece  = ID_FALSE;
    idBool                  sUpdateInplace         = ID_TRUE;
    UShort                  sColCount              = aColCount;
    UShort                  sUptAftInModeColCnt    = 0;
    UShort                  sUptBfrInModeColCnt    = 0;
    UShort                  sUptAftLobDescCnt      = 0;
    UShort                  sUptBfrLobDescCnt      = 0;
    UShort                  sFstColumnPieceSize;
    UShort                  sColSeqInRowPiece;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aColList    != NULL );
    IDE_ASSERT( aValueList  != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    /* update�ؾ��ϴ� �÷��� ã�� ������ �����ϸ鼭
     * new rowpiece size�� ���� ����Ѵ�.
     * ������ ���� ������������ for loop �ϳ��� ���̱� ���ؼ��̴�. */
    sNewRowPieceSize += SDC_ROWHDR_SIZE;

    SDC_GET_ROWHDR_1B_FIELD(aSlotPtr, SDC_ROWHDR_FLAG, sOldRowFlag);

    /* �ʿ��� Extra Size�� ����Ѵ� */
    if ( SDC_IS_LAST_ROWPIECE(sOldRowFlag) == ID_FALSE )
    {
        sNewRowPieceSize += SDC_EXTRASIZE_FOR_CHAINING;
    }

    /* row piece�� ����� �÷����
     * update column list�� ���ϸ鼭
     * update�� �����ؾ� �ϴ� �÷��� ã�´�.
     * �̶� ���� ����Ʈ�� column sequence ������
     * ���ĵǾ� �ֱ� ������, merge join ������� �񱳸� �Ѵ�.
     * nested loop ����� ������� �ʴ´�.
     * */

    /* �� for loop�� row piece���� ��� �÷��� ��ȸ�Ѵ�. */
    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN(sColumnInfo->mColumn) == ID_TRUE )
        {
            /* FIT/ART/sm/Projects/PROJ-2118/update/update.ts */
            IDU_FIT_POINT( "1.PROJ-2118@sdcRow::analyzeRowForUpdate" );

            /* update�ؾ��ϴ� �÷��� ã�� ��� */
            if ( (sColSeqInRowPiece == 0) &&
                 (SM_IS_FLAG_ON(sOldRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE) )
            {
                /* rowpiece���� ù��° �÷��� update�ؾ� �ϴµ�,
                 * ù��° �÷��� prev rowpiece�κ��� �̾��� ���,
                 * delete first column piece ������ �����ؾ� �Ѵ�. */

                /* ���� rowpiece�� ������ ����� �÷��� update�Ǵ� ���,
                 * �� �÷��� first piece�� �����ϰ� �ִ� rowpiece����
                 * update ������ ����ϹǷ�, ������ rowpiece������
                 * �ش� column piece�� �����ؾ� �Ѵ�. */
                IDE_ASSERT( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mOldValueInfo ) == ID_TRUE );
                
                sDeleteFstColumnPiece = ID_TRUE;
                sFstColumnPieceSize   =
                    SDC_GET_COLPIECE_SIZE(sColumnInfo->mOldValueInfo.mValue.length);

                break;
            }

            if ( SDC_IS_IN_MODE_COLUMN(sColumnInfo->mNewValueInfo) == ID_TRUE )
            {
                /* �Ϲ� column�� update�ϴ� ��� */
                sUptAftInModeColCnt++;
            }
            else
            {
                /* lob column�� update�ϴ� ��� */
                sUptAftLobDescCnt++;

                IDE_ERROR( sColumnInfo->mNewValueInfo.mValue.length
                           == ID_SIZEOF(sdcLobDesc) );
            }

            sNewRowPieceSize +=
                SDC_GET_COLPIECE_SIZE(sColumnInfo->mNewValueInfo.mValue.length);

            if ( sColumnInfo->mOldValueInfo.mValue.length !=
                 sColumnInfo->mNewValueInfo.mValue.length )
            {
                /* �÷��� ũ�Ⱑ ���ϸ� update inplace ������ ������ �� ����. */
                sUpdateInplace = ID_FALSE;
            }
            else
            {
                if ( (sColumnInfo->mOldValueInfo.mValue.length == 0) &&
                     (sColumnInfo->mNewValueInfo.mValue.length == 0) )
                {
                    if ( SDC_IS_NULL(&(sColumnInfo->mOldValueInfo.mValue)) &&
                         SDC_IS_EMPTY(&(sColumnInfo->mNewValueInfo.mValue)) )
                    {
                        sUpdateInplace = ID_FALSE;
                    }
                    
                    if ( SDC_IS_EMPTY(&(sColumnInfo->mOldValueInfo.mValue)) &&
                         SDC_IS_NULL(&(sColumnInfo->mNewValueInfo.mValue)) )
                    {
                        sUpdateInplace = ID_FALSE;
                    }
                }
                else 
                {
                    /* nothing to do */
                }
                
                if ( (sColSeqInRowPiece == sColCount-1) &&
                     (SM_IS_FLAG_ON(sOldRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE) )
                {
                    /* rowpiece���� ������ �÷��� update�ؾ� �ϴµ�,
                     * ������ �÷��� next rowpiece�� ��� �̾����� ���,
                     * update inplace ������ ������ �� ����. */
                    sUpdateInplace = ID_FALSE;
                }
                else
                {
                    // In Out Mode�� �ٸ��� Prefix�� �ٽ� ����ؾ�
                    // �ϹǷ� InPlace�� update�� �� ����.
                    if ( sColumnInfo->mOldValueInfo.mInOutMode !=
                         sColumnInfo->mNewValueInfo.mInOutMode )
                    {
                        sUpdateInplace = ID_FALSE;
                    }
                }
            }

            if ( SDC_IS_IN_MODE_COLUMN(sColumnInfo->mOldValueInfo) == ID_TRUE )
            {
                sUptBfrInModeColCnt++;
            }
            else
            {
                /* PROJ-1862 ������ LOB Descriptor���� Column��
                 * Update�Ǹ� ���� LOB Object�� �����ϱ� ����
                 * Old LOB Descriptor�� ���� Ȯ���صд�. */
                sUptBfrLobDescCnt++;
            }
        }
        else
        {
            /* update�ؾ� �ϴ� �÷��� �ƴ� ��� */
            sNewRowPieceSize +=
                SDC_GET_COLPIECE_SIZE(sColumnInfo->mOldValueInfo.mValue.length);
        }
    }

    if ( sDeleteFstColumnPiece == ID_TRUE )
    {
        /* delete first column piece ������ �����ؾ� �ϴ� ��� */
        aUpdateInfo->mUptAftInModeColCnt       = 0;
        aUpdateInfo->mUptBfrInModeColCnt       = 1;
        aUpdateInfo->mUptAftLobDescCnt         = 0;
        aUpdateInfo->mUptBfrLobDescCnt         = 0;
        aUpdateInfo->mIsDeleteFstColumnPiece   = ID_TRUE;
        aUpdateInfo->mNewRowPieceSize
            = aUpdateInfo->mOldRowPieceSize - sFstColumnPieceSize;
    }
    else
    {
        aUpdateInfo->mUptAftInModeColCnt       = sUptAftInModeColCnt;
        aUpdateInfo->mUptBfrInModeColCnt       = sUptBfrInModeColCnt;
        aUpdateInfo->mUptAftLobDescCnt         = sUptAftLobDescCnt;
        aUpdateInfo->mUptBfrLobDescCnt         = sUptBfrLobDescCnt;
        aUpdateInfo->mIsDeleteFstColumnPiece   = sDeleteFstColumnPiece;
        aUpdateInfo->mIsUpdateInplace          = sUpdateInplace;
        aUpdateInfo->mNewRowPieceSize          = sNewRowPieceSize;
    }

    aUpdateInfo->mIsTrailingNullUpdate = ID_FALSE;
    if ( SDC_IS_LAST_ROWPIECE(sOldRowFlag) == ID_TRUE )
    {
        /* last rowpiece�� ���,
         * trailing null�� update�Ǵ����� üũ�ؾ� �Ѵ�. */
        if ( aLstUptColumnSeq > (aFstColumnSeq + sColCount - 1) )
        {
            aUpdateInfo->mIsTrailingNullUpdate = ID_TRUE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  rowpiece���� �����ϴ� lob column ������ �м��Ͽ�,
 *  sdcLobInfoInRowPiece�� �����.
 *
 *  aTableHeader   - [IN]  ���̺� ���
 *  aSlotPtr       - [IN]  slot pointer
 *  aFstColumnSeq  - [IN]  rowpiece���� ù��° column piece��
 *                         global sequence number
 *  aColCount      - [IN]  column count in rowpiece
 *  aLobInfo       - [OUT] lob column ������ �����ϴ� �ڷᱸ��
 *
 **********************************************************************/
IDE_RC sdcRow::analyzeRowForLobRemove( void                 * aTableHeader,
                                       UChar                * aSlotPtr,
                                       UShort                 aFstColumnSeq,
                                       UShort                 aColCount,
                                       sdcLobInfoInRowPiece * aLobInfo )
{
    sdcColumnInfo4Lob        *sColumnInfo;
    const smiColumn          *sColumn;
    UShort                    sColumnSeq;
    UShort                    sColumnSeqInRowPiece;
    UShort                    sLobColCount = 0;
    UShort                    sOffset      = 0;
    sdcColInOutMode           sInOutMode;
    UChar                    *sColPiecePtr;
    UInt                      sColLen;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aSlotPtr     != NULL );

    sColPiecePtr = getDataArea( aSlotPtr );
    sOffset = 0;

    for ( sColumnSeqInRowPiece = 0;
          sColumnSeqInRowPiece < aColCount;
          sColumnSeqInRowPiece++, sColPiecePtr += sColLen )
    {
        sColPiecePtr = getColPiece( sColPiecePtr,
                                    &sColLen,
                                    NULL, /* aColVal */
                                    &sInOutMode );

        if ( sInOutMode == SDC_COLUMN_IN_MODE )
        {
            continue;
        }

        if ( sColLen != ID_SIZEOF(sdcLobDesc) )
        {
            ideLog::log( IDE_ERR_0,
                         "ColLen: %"ID_UINT32_FMT", "
                         "ColumnSeqInRowPiece: %"ID_UINT32_FMT", "
                         "ColCount: %"ID_UINT32_FMT"\n",
                         sColLen,
                         sColumnSeqInRowPiece,
                         aColCount );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aTableHeader,
                            ID_SIZEOF(smcTableHeader),
                            "TableHeader Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        // LOB Column���� Ȯ��
        sColumnSeq  = aFstColumnSeq + sColumnSeqInRowPiece;
        sColumn     = smLayerCallback::getColumn( aTableHeader, sColumnSeq );

        if ( SDC_IS_LOB_COLUMN(sColumn) == ID_FALSE)
        {
            ideLog::log( IDE_ERR_0,
                         "FstColumnSeq: %"ID_UINT32_FMT", "
                         "ColumnSeq: %"ID_UINT32_FMT"\n",
                         aFstColumnSeq,
                         sColumnSeqInRowPiece );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sColumn,
                            ID_SIZEOF(smiColumn),
                            "Column Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aTableHeader,
                            ID_SIZEOF(smcTableHeader),
                            "TableHeader Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         aSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }

        // LOB Remove Info�� ���
        sColumnInfo = aLobInfo->mColInfoList + sLobColCount;
        sColumnInfo->mColumn  = sColumn;
        // BUG-25638 [SD] sdcRow::analyzeRowForLobRemove���� Lob Offset ������ �߸��ϰ� �ֽ��ϴ�.
        sColumnInfo->mLobDesc =
            (sdcLobDesc*)(aLobInfo->mSpace4CopyLobDesc + sOffset);

        idlOS::memcpy( sColumnInfo->mLobDesc,
                       sColPiecePtr,
                       ID_SIZEOF(sdcLobDesc) );

        sOffset += ID_SIZEOF(sdcLobDesc);

        sLobColCount++;
    }

    aLobInfo->mLobDescCnt = sLobColCount;

    return IDE_SUCCESS;
}


/***********************************************************************
 *
 * Description : TASK-5030
 *      Full XLOG�� ���� analyzeRowForUpdate()�� ������ ������
 *      �Լ��� �ۼ��Ѵ�.
 *
 * aTableHeader  - [IN] table header
 * aColCount     - [IN] row piece ���� column count
 * aFstColumnSeq - [IN] row piece�� ù column�� ��ü���� ���°
 *                      column����
 * aUpdateInfo   - [OUT] FXLog�� ���� ������ ����
 **********************************************************************/
IDE_RC sdcRow::analyzeRowForDelete4RP( void                  * aTableHeader,
                                       UShort                  aColCount,
                                       UShort                  aFstColumnSeq,
                                       sdcRowPieceUpdateInfo * aUpdateInfo )
{
    const smiColumn       * sUptCol;
    sdcColumnInfo4Update  * sColumnInfo;
    UInt                    sColumnIndex        = 0;
    UShort                  sColCount           = aColCount;
    UShort                  sUptBfrInModeColCnt = 0;
    UShort                  sColSeqInRowPiece;
    UShort                  sColumnSeq;
    idBool                  sFind;

    IDE_ASSERT( aUpdateInfo != NULL );

    /* �� for loop�� row piece���� ��� �÷��� ��ȸ�Ѵ�. */
    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;
        /* column�� row sequence�� ���Ѵ�. */
        sColumnSeq  = aFstColumnSeq + sColSeqInRowPiece;

        sFind = ID_FALSE;

        /* column list�� ���ĵǾ� �����Ƿ� sort merge join�� ����ϰ� ��ȸ�ϸ� �ȴ� */
        for ( /*������ sColumnIndex�� �ٽ� ã�� �ʿ� ���� */ ;
              sColumnIndex < smLayerCallback::getColumnCount( aTableHeader ) ;
              sColumnIndex++ )
        {
            sUptCol = smLayerCallback::getColumn( aTableHeader, sColumnIndex );

            if ( SDC_GET_COLUMN_SEQ(sUptCol) == sColumnSeq )
            {
                sFind = ID_TRUE;
                break;
            }
        }

        if ( sFind == ID_TRUE )
        {
            /* smiColumn ������ �����Ѵ�. */
            sColumnInfo->mColumn = (const smiColumn*)sUptCol; // �� ����

            if ( SDC_IS_IN_MODE_COLUMN(sColumnInfo->mOldValueInfo) == ID_TRUE )
            {
                sUptBfrInModeColCnt++;
            }
            else
            {
                /* do nothing */
            }
        }
        else
        {
            /* do nothing */
        }
    }
    
    aUpdateInfo->mUptBfrInModeColCnt    = sUptBfrInModeColCnt;
    aUpdateInfo->mIsTrailingNullUpdate  = ID_FALSE;

    return IDE_SUCCESS;
}


/***********************************************************************
 *
 * Description :
 *  insert info�� trailing null�� � �ִ����� ����ϴ� �Լ��̴�.
 *
 **********************************************************************/
UShort sdcRow::getTrailingNullCount(
    const sdcRowPieceInsertInfo *aInsertInfo,
    UShort                       aTotalColCount)
{
    const sdcColumnInfo4Insert   *sColumnInfo;
    UShort                        sTrailingNullCount;
    SShort                        sColSeq;

    IDE_ASSERT( aInsertInfo != NULL );

    sTrailingNullCount = 0;

    /* trailing null count�� ����ϱ� ����
     * ������(reverse order)���� loop�� ����. */
    for ( sColSeq = aTotalColCount-1;
          sColSeq >= 0;
          sColSeq -- )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sColSeq;
        /* null value���� üũ */
        if ( SDC_IS_NULL(&(sColumnInfo->mValueInfo.mValue)) != ID_TRUE )
        {
            break;
        }

        // PROJ-1864 Disk In Mode LOB
        // Lob Desc�� Value�� Null�� �� ����.
        IDE_ASSERT( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) == ID_TRUE );

        sTrailingNullCount++;
    }

    return sTrailingNullCount;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UInt sdcRow::getColDataSize2Store( const sdcRowPieceInsertInfo  *aInsertInfo,
                                   UShort                        aColSeq )
{
    const sdcColumnInfo4Insert   *sColumnInfo;
    UInt                          sStoreSize;

    IDE_ASSERT( aInsertInfo != NULL );

    sColumnInfo = aInsertInfo->mColInfoList + aColSeq;

    sStoreSize = sColumnInfo->mValueInfo.mValue.length;

    return sStoreSize;
}


/***********************************************************************
 *
 * Description :
 *  ���ڷ� ���� �÷��� �������� rowpiece�� ������ ������ �� �ִ� �÷������� �˻��Ѵ�.
 *
 **********************************************************************/
idBool sdcRow::isDivisibleColumn(const sdcColumnInfo4Insert    *aColumnInfo)
{
    idBool    sRet;

    IDE_ASSERT( aColumnInfo != NULL );

    /* lob column���� üũ */
    if ( SDC_IS_LOB_COLUMN( aColumnInfo->mColumn ) == ID_TRUE )
    {
        if ( SDC_IS_IN_MODE_COLUMN( aColumnInfo->mValueInfo ) == ID_TRUE )
        {
            sRet = ID_TRUE;
        }
        else
        {
            sRet = ID_FALSE;
        }
    }
    else
    {
        /* divisible flag üũ */
        if ( ( aColumnInfo->mColumn->flag & SMI_COLUMN_DATA_STORE_DIVISIBLE_MASK)
             == SMI_COLUMN_DATA_STORE_DIVISIBLE_FALSE )
        {
            sRet = ID_FALSE;
        }
        else
        {
            sRet = ID_TRUE;
        }
    }

    return sRet;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
sdSID sdcRow::getNextRowPieceSID( const UChar    *aSlotPtr )
{
    UChar        sRowFlag;
    scPageID     sPageID;
    scSlotNum    sSlotNum;
    sdSID        sNextSID = SD_NULL_SID;

    IDE_ASSERT( aSlotPtr != NULL );

    SDC_GET_ROWHDR_1B_FIELD( aSlotPtr, SDC_ROWHDR_FLAG, sRowFlag );

    if ( SDC_IS_LAST_ROWPIECE(sRowFlag) == ID_TRUE )
    {
        sNextSID = SD_NULL_SID;
    }
    else
    {
        ID_READ_VALUE( (aSlotPtr + SDC_ROW_NEXT_PID_OFFSET),
                       &sPageID,
                       ID_SIZEOF(sPageID) );
        ID_READ_VALUE( (aSlotPtr + SDC_ROW_NEXT_SNUM_OFFSET),
                       &sSlotNum,
                       ID_SIZEOF(sSlotNum) );

        sNextSID = SD_MAKE_SID( sPageID, sSlotNum );
    }

    return sNextSID;
}


/***********************************************************************
 *
 * Description :
 *  sdcRowPieceInsertInfo �ڷᱸ���� �ʱ�ȭ�Ѵ�.
 *
 **********************************************************************/
void sdcRow::makeInsertInfo( void                   *aTableHeader,
                             const smiValue         *aValueList,
                             UShort                  aTotalColCount,
                             sdcRowPieceInsertInfo  *aInsertInfo )
{
    sdcColumnInfo4Insert    * sColumnInfo;
    UShort                    sColSeq;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aValueList   != NULL );
    IDE_ASSERT( aInsertInfo  != NULL );

    /* �ڷᱸ�� �ʱ�ȭ */
    SDC_INIT_INSERT_INFO(aInsertInfo);

    for ( sColSeq = 0; sColSeq < aTotalColCount; sColSeq++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sColSeq;

        /* table header�� �̿��Ͽ� �÷������� ���´�. */
        sColumnInfo->mColumn
            = smLayerCallback::getColumn( aTableHeader, sColSeq );

        /* �÷��� ���� �����Ѵ�. */
        sColumnInfo->mValueInfo.mValue = *(aValueList + sColSeq);

        sColumnInfo->mIsUptCol = ID_FALSE;

        if ( SDC_GET_COLUMN_INOUT_MODE(sColumnInfo->mColumn,
                                       sColumnInfo->mValueInfo.mValue.length)
             == SDC_COLUMN_OUT_MODE_LOB )
        {
            sColumnInfo->mValueInfo.mOutValue = sColumnInfo->mValueInfo.mValue;
            
            sColumnInfo->mValueInfo.mValue.value  = &sdcLob::mEmptyLobDesc;
            sColumnInfo->mValueInfo.mValue.length = ID_SIZEOF(sdcLobDesc);
            sColumnInfo->mValueInfo.mInOutMode    = SDC_COLUMN_OUT_MODE_LOB;
        }
        else
        {
            sColumnInfo->mValueInfo.mOutValue.value  = NULL;
            sColumnInfo->mValueInfo.mOutValue.length = 0;
            
            sColumnInfo->mValueInfo.mInOutMode = SDC_COLUMN_IN_MODE;
        }
    }
}


/***********************************************************************
 *
 * Description :
 *
 **********************************************************************/
IDE_RC sdcRow::makeUpdateInfo( UChar                  * aSlotPtr,
                               const smiColumnList    * aColList,
                               const smiValue         * aValueList,
                               sdSID                    aSlotSID,
                               UShort                   aColCount,
                               UShort                   aFstColumnSeq,
                               sdcColInOutMode        * aValueModeList,
                               sdcRowPieceUpdateInfo  * aUpdateInfo )
{
    UChar                 * sColPiecePtr;
    sdcColumnInfo4Update  * sColumnInfo;
    UShort                  sOffset;
    UInt                    sColLen;
    UChar                 * sColVal;
    UShort                  sColSeqInRowPiece;
    UShort                  sColumnSeq;
    sdcColInOutMode         sInOutMode;
    const smiValue        * sUptVal;
    const smiColumn       * sUptCol;
    const smiColumnList   * sUptColList;
    sdcColInOutMode       * sUptValMode;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    if ( aValueModeList != NULL )
    {
        aUpdateInfo->mIsUptLobByAPI = ID_TRUE;
    }
    else
    {
        aUpdateInfo->mIsUptLobByAPI = ID_FALSE;
    }

    sColPiecePtr = getDataArea( aSlotPtr );
    sOffset = 0;

    sUptColList = aColList;
    sUptVal     = aValueList;
    sUptValMode = aValueModeList;

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;
        sColumnInfo->mColumn = NULL;

        sColumnSeq = aFstColumnSeq + sColSeqInRowPiece;

        /*
         * 1. set old value
         */
        
        sColPiecePtr = getColPiece( sColPiecePtr,
                                    &sColLen,
                                    &sColVal,
                                    &sInOutMode );

        if ( (sColLen == 0) && (sColVal == NULL) )
        {
            /* NULL column�� ��� */
            sColumnInfo->mOldValueInfo.mValue.length = 0;
            sColumnInfo->mOldValueInfo.mValue.value  = NULL;
            sColumnInfo->mOldValueInfo.mInOutMode    = SDC_COLUMN_IN_MODE;
        }
        else
        {
            /* old column length ���� */
            sColumnInfo->mOldValueInfo.mValue.length = sColLen;
            sColumnInfo->mOldValueInfo.mValue.value =
                aUpdateInfo->mSpace4CopyOldValue + sOffset;
            sColumnInfo->mOldValueInfo.mInOutMode = sInOutMode;

            /* old column value ���� */
            if ( sColLen > 0 )
            {
                idlOS::memcpy( (void*)sColumnInfo->mOldValueInfo.mValue.value,
                               sColPiecePtr,
                               sColLen );
            }

            /* offset ������ */
            sOffset += sColLen;
        }

        /*
         * 2. set new value
         */
        
        sColumnInfo->mNewValueInfo.mValue.value     = NULL;
        sColumnInfo->mNewValueInfo.mValue.length    = 0;
        sColumnInfo->mNewValueInfo.mOutValue.value  = NULL;
        sColumnInfo->mNewValueInfo.mOutValue.length = 0;
        sColumnInfo->mNewValueInfo.mInOutMode       = SDC_COLUMN_IN_MODE;
        
        while( sUptColList != NULL )
        {
            sUptCol = sUptColList->column;

            if ( SDC_GET_COLUMN_SEQ(sUptCol) == sColumnSeq )
            {
                IDE_ERROR_RAISE_MSG( ( (sUptVal->length <= sUptCol->size) &&
                                       ( ((sUptVal->length > 0)  && (sUptVal->value != NULL)) ||
                                         (sUptVal->length == 0) ) ),
                                     err_invalid_column_size,
                                    "Column Seq    : %"ID_UINT32_FMT"\n"
                                    "Column Size   : %"ID_UINT32_FMT"\n"
                                    "Column Offset : %"ID_UINT32_FMT"\n"
                                    "Value Length  : %"ID_UINT32_FMT"\n",
                                    sColumnSeq,
                                    sUptCol->size,
                                    sUptCol->offset,
                                    sUptVal->length );

                /* smiColumn, smiValue, IsLobDesc ������ �����Ѵ�. */
                sColumnInfo->mColumn = (const smiColumn*)sUptCol;
                
                sColumnInfo->mNewValueInfo.mValue = *sUptVal;
                
                sColumnInfo->mNewValueInfo.mOutValue.value = NULL;
                sColumnInfo->mNewValueInfo.mOutValue.length = 0;

                if ( sUptValMode == NULL )
                {
                    if ( SDC_GET_COLUMN_INOUT_MODE(sColumnInfo->mColumn,
                                                   sColumnInfo->mNewValueInfo.mValue.length)
                         == SDC_COLUMN_OUT_MODE_LOB )
                    {
                        sColumnInfo->mNewValueInfo.mOutValue =
                            sColumnInfo->mNewValueInfo.mValue;
            
                        sColumnInfo->mNewValueInfo.mValue.value  = &sdcLob::mEmptyLobDesc;
                        sColumnInfo->mNewValueInfo.mValue.length = ID_SIZEOF(sdcLobDesc);
                        sColumnInfo->mNewValueInfo.mInOutMode    = SDC_COLUMN_OUT_MODE_LOB;
                    }
                    else
                    {
                        sColumnInfo->mNewValueInfo.mInOutMode = SDC_COLUMN_IN_MODE;
                    }
                }
                else
                {
                    sColumnInfo->mNewValueInfo.mInOutMode = *sUptValMode;
                }

                break;
            }

            /* update column�� sequence�� row piece column��
             * sequence���� ũ�� list�� �̵��ϸ� �ȵǹǷ� break �Ѵ�. */
            if ( SDC_GET_COLUMN_SEQ(sUptCol) > sColumnSeq )
            {
                break;
            }

            /* update column list�� update value list�� �� ��(pair)�̹Ƿ�,
             * list�� �̵���ų���� ���� �̵����� �־�� �Ѵ�. */
            sUptColList = sUptColList->next;
            sUptVal++;
            
            if ( sUptValMode != NULL )
            {
                sUptValMode++;
            }
        }

        /*
         * 3. next column piece�� �̵�
         */
        
        sColPiecePtr += sColLen;
    }

    aUpdateInfo->mUptAftInModeColCnt     = 0;
    aUpdateInfo->mUptBfrInModeColCnt     = 0;
    aUpdateInfo->mUptAftLobDescCnt       = 0;
    aUpdateInfo->mUptBfrLobDescCnt       = 0;
    aUpdateInfo->mTrailingNullUptCount   = 0;
    aUpdateInfo->mOldRowPieceSize        = getRowPieceSize(aSlotPtr);
    aUpdateInfo->mNewRowPieceSize        = 0;
    aUpdateInfo->mIsDeleteFstColumnPiece = ID_FALSE;
    aUpdateInfo->mIsUpdateInplace        = ID_TRUE;
    aUpdateInfo->mIsTrailingNullUpdate   = ID_FALSE;
    aUpdateInfo->mRowPieceSID            = aSlotSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_COLUMN_SIZE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Retry Info�� Statement retry column list��
 *               Row retry column list�� Ȯ���ؼ�
 *               ���� retry ��Ȳ�̸� ������ ��ȯ�Ѵ�
 *
 *  aTrans            - [IN] Ʈ����� ����
 *  aMtx              - [IN] Mtx ������
 *  aMtxSavePoint     - [IN] Mtx�� ���� Savepoint
 *  aRowSlotPtr       - [IN] Ȯ�� �� row�� pointer
 *  aRowFlag          - [IN] row flag
 *  aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻�
 *  aStmtViewSCN      - [IN] stmt scn
 *  aRetryInfo        - [IN/OUT] retry info
 *  aColCount         - [IN] column count in row piece
 **********************************************************************/
IDE_RC sdcRow::checkRetry( void         * aTrans,
                           sdrMtx       * aMtx,
                           sdrSavePoint * aSP,
                           UChar        * aRowSlotPtr,
                           UInt           aRowFlag,
                           idBool         aForbiddenToRetry,
                           smSCN          aStmtViewSCN,
                           sdcRetryInfo * aRetryInfo,
                           UShort         aColCount )
{
    IDE_ASSERT( aRetryInfo->mRetryInfo != NULL );

    // LOB �˻� �ӽ��ڵ�, ������ ����
    if ( SDC_IS_HEAD_ROWPIECE( aRowFlag ) == ID_TRUE )
    {
        IDE_TEST_RAISE( existLOBCol( aRetryInfo ) == ID_TRUE,
                        REBUILD_ALREADY_MODIFIED );
    }

    if ( aColCount > 0 )
    {
        // Statement Retry ���� �˻�
        IDE_TEST_RAISE(( ( aRetryInfo->mStmtRetryColLst.mCurColumn != NULL ) &&
                         ( isSameColumnValue( aRowSlotPtr,
                                              aRowFlag,
                                              &(aRetryInfo->mStmtRetryColLst),
                                              aRetryInfo->mColSeqInRow,
                                              aColCount ) == ID_FALSE ) ),
                       REBUILD_ALREADY_MODIFIED );

        // Row Retry ���� �˻�
        if ( ( aRetryInfo->mRowRetryColLst.mCurColumn != NULL ) &&
             ( isSameColumnValue( aRowSlotPtr,
                                  aRowFlag,
                                  &(aRetryInfo->mRowRetryColLst),
                                  aRetryInfo->mColSeqInRow,
                                  aColCount ) == ID_FALSE ) )
        {
            if ( aRetryInfo->mISavepoint != NULL )
            {
                IDE_TEST( smxTrans::abortToImpSavepoint4LayerCall( aTrans,
                                                                   aRetryInfo->mISavepoint )
                          != IDE_SUCCESS );
            }

            IDE_RAISE( ROW_RETRY );
        }

    }

    // ���� �˻� �� ������ �����ִ��� Ȯ���Ѵ�
    if ( SDC_NEED_RETRY_CHECK( aRetryInfo ) )
    {
        aRetryInfo->mIsAlreadyModified = ID_TRUE;

        if ( SM_IS_FLAG_ON( aRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE )
        {
            aRetryInfo->mColSeqInRow += aColCount - 1;
        }
        else
        {
            aRetryInfo->mColSeqInRow += aColCount;
        }

        if ( SDC_IS_HEAD_ROWPIECE( aRowFlag ) == ID_TRUE )
        {
            // Head row piece �̰� ���� �˻��� ������ ������,
            // imp save point�� ����Ѵ�
            IDE_TEST( smxTrans::setImpSavepoint4Retry( aTrans,
                                                       &(aRetryInfo->mISavepoint) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        aRetryInfo->mIsAlreadyModified = ID_FALSE;

        if ( aRetryInfo->mISavepoint != NULL )
        {
            IDE_TEST( smxTrans::unsetImpSavepoint4LayerCall( aTrans,
                                                             aRetryInfo->mISavepoint )
                      != IDE_SUCCESS );

            aRetryInfo->mISavepoint = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ROW_RETRY );
    {
        IDE_SET( ideSetErrorCode(smERR_RETRY_Row_Retry) );
    }
    IDE_EXCEPTION( REBUILD_ALREADY_MODIFIED )
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE );

            SChar    sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            sdpCTS * sCTS;
            smSCN    sFSCNOrCSCN;
            UChar    sCTSlotIdx;
            sdcRowHdrInfo   sOldRowHdrInfo;
            sdcRowHdrExInfo sRowHdrExInfo;

            getRowHdrInfo( aRowSlotPtr, &sOldRowHdrInfo );
            sCTSlotIdx = sOldRowHdrInfo.mCTSlotIdx;

            if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
            {
                sCTS = sdcTableCTL::getCTS( sdpPhyPage::getHdr(aRowSlotPtr),sCTSlotIdx );
                SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );
            }
            else
            {
                getRowHdrExInfo( aRowSlotPtr, &sRowHdrExInfo );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowHdrExInfo.mFSCNOrCSCN );
            }

            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[RECORD VALIDATION(RE)] "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CTSlotIdx:%"ID_UINT32_FMT", "
                             "FSCNOrCSCN:%"ID_UINT64_FMT", "
                             "InfiniteSCN:%"ID_UINT64_FMT", "
                             "Deleted:%s ",
                             aStmtViewSCN,
                             sCTSlotIdx,
                             SM_SCN_TO_LONG( sFSCNOrCSCN ),
                             SM_SCN_TO_LONG( sOldRowHdrInfo.mInfiniteSCN ),
                             SM_SCN_IS_DELETED( sOldRowHdrInfo.mInfiniteSCN)?"Y":"N" );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_RETRY_Already_Modified ) );
        }
    }
    IDE_EXCEPTION_END;

    if ( aRetryInfo->mISavepoint != NULL )
    {
        IDE_ASSERT( smxTrans::unsetImpSavepoint4LayerCall( aTrans,
                                                           aRetryInfo->mISavepoint )
                    == IDE_SUCCESS );

        aRetryInfo->mISavepoint = NULL;
    }

    if ( aMtx != NULL )
    {
        IDE_ASSERT( releaseLatchForAlreadyModify( aMtx, aSP )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : update�ϰ� ���� row piece�鿡 ���ؼ�
 *               retry ���θ� Ȯ���Ѵ�.
 *
 *   aStatistics       - [IN] �������
 *   aTrans            - [IN] Ʈ����� ����
 *   aSpaceID          - [IN] Tablespace ID
 *   aRowPieceSID      - [IN] �˻��ؾ� �� Row piece�� SID
 *   aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻�
 *   aStmtViewSCN      - [IN] stmt scn
 *   aRetryInfo        - [IN/OUT] retry info
 **********************************************************************/
IDE_RC sdcRow::checkRetryRemainedRowPiece( idvSQL         * aStatistics,
                                           void           * aTrans,
                                           scSpaceID        aSpaceID,
                                           sdSID            aRowPieceSID,
                                           idBool           aForbiddenToRetry,
                                           smSCN            aStmtViewSCN,
                                           sdcRetryInfo   * aRetryInfo )
{
    UInt                sState = 0;
    UChar             * sCurSlotPtr;
    sdcRowHdrInfo       sRowHdrInfo;

    IDE_ASSERT( aTrans     != NULL );
    IDE_ASSERT( aRetryInfo != NULL );

    while( aRowPieceSID != SD_NULL_SID )
    {
        if ( aRetryInfo->mIsAlreadyModified == ID_FALSE )
        {
            // QP���� ������ column list�� ��� �˻��Ͽ���
            break;
        }

        IDE_TEST( sdbBufferMgr::getPageBySID( aStatistics,
                                              aSpaceID,
                                              aRowPieceSID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL, // aMtx
                                              (UChar**)&sCurSlotPtr )
                  != IDE_SUCCESS );
        sState = 1;

        getRowHdrInfo( sCurSlotPtr, &sRowHdrInfo );

        IDE_TEST( checkRetry( aTrans,
                              NULL, // sMtx,
                              NULL, // Savepoint,
                              sCurSlotPtr,
                              sRowHdrInfo.mRowFlag,
                              aForbiddenToRetry,
                              aStmtViewSCN,
                              aRetryInfo,
                              sRowHdrInfo.mColCount )
                  != IDE_SUCCESS );

        aRowPieceSID = getNextRowPieceSID( sCurSlotPtr );

        /* update���� ȣ��ȴ�. lock row�� ���� �����Ƿ�
         * latch�� Ǯ� Next SID�� ����ȴ�. */
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             sCurSlotPtr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               sCurSlotPtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Retry Info�� column�߿� LOB�� �ִ����� �˻�.
 *               DML skip Retry���� Disk LOB �� �����ϸ�
 *               �� �Լ��� �����ؾ� �Ѵ�.
 **********************************************************************/
idBool sdcRow::existLOBCol( sdcRetryInfo  * aRetryInfo )
{
    const smiColumnList * sChkColList;
    idBool                sIsRetryByLOB = ID_FALSE;

    for ( sChkColList = aRetryInfo->mRetryInfo->mStmtRetryColLst ;
          ( sChkColList != NULL ) && ( sIsRetryByLOB == ID_FALSE );
          sChkColList = sChkColList->next )
    {
        // where���� LOB Column�� �ִ� ��� �ϴ�����
        if ( SDC_IS_LOB_COLUMN( sChkColList->column ) == ID_TRUE )
        {
            sIsRetryByLOB = ID_TRUE;
        }
    }

    for ( sChkColList = aRetryInfo->mRetryInfo->mRowRetryColLst ;
          ( sChkColList != NULL ) && ( sIsRetryByLOB == ID_FALSE );
          sChkColList = sChkColList->next )
    {
        // set���� LOB Column�� �ִ� ��� �ϴ�����
        if ( SDC_IS_LOB_COLUMN( sChkColList->column ) == ID_TRUE )
        {
            sIsRetryByLOB = ID_TRUE;
        }
    }

    return sIsRetryByLOB;
}

/***********************************************************************
 * Description : Retry Info�� column list�� value list�� �޾Ƽ�
 *               ���� ������ ������ Ȯ���Ѵ�.
 *
 *  aRowSlotPtr  - [IN] Ȯ�� �� row�� pointer
 *  aRowFlag     - [IN] row flag
 *  aRetryInfo   - [IN] retry info
 *  aFstColSeq   - [IN] first column seq in row piece
 *  aColCount    - [IN] column count in row piece
 **********************************************************************/
idBool sdcRow::isSameColumnValue( UChar               * aSlotPtr,
                                  UInt                  aRowFlag,
                                  sdcRetryCompColumns * aCompColumns,
                                  UShort                aFstColSeq,
                                  UShort                aColCount )
{
    const smiValue      * sChkVal;
    const smiColumnList * sChkColList;
    const smiColumn     * sChkCol;
    UChar               * sColPiecePtr;
    UChar               * sColVal;
    UInt                  sColLen;
    UShort                sColSeqInRowPiece = 0;
    UShort                sColSeq    = 0;
    UShort                sOffset    = 0;
    sdcColInOutMode       sInOutMode;
    idBool                sIsSameColumnValue = ID_TRUE;

    // value list�� �������� ��ƾ� �Ѵ�
    // comp cursor�� ��� delete �� �� ���� set column list�� ���� �� �ִ�.
    IDE_ASSERT( aSlotPtr                 != NULL );
    IDE_ASSERT( aCompColumns             != NULL );
    IDE_ASSERT( aCompColumns->mCurColumn != NULL );
    IDE_ASSERT( aCompColumns->mCurValue  != NULL );
    IDE_ASSERT( aColCount                >  0 );

    sChkColList = aCompColumns->mCurColumn;
    sColSeq     = SDC_GET_COLUMN_SEQ( sChkColList->column );
    sOffset     = aCompColumns->mCurOffset;

    if ( sOffset > 0 )
    {
        /* sOffset ���� �����Ǿ� ������
         * 1. first column seq �� ����
         * 2. prev flag�� �����Ǿ� �־�� �Ѵ�.
         * */
        if ( ( sColSeq != aFstColSeq ) ||
             ( SM_IS_FLAG_ON( aRowFlag, SDC_ROWHDR_P_FLAG ) == ID_FALSE ))
        {
            sIsSameColumnValue = ID_FALSE;

            IDE_CONT( SKIP_CHECK );
        }
    }

    // ������ row piece�� �˻��ϴ� column�� ���� ���
    // �� �˻� ������ ���� �������� ���Ѱ��
    IDE_TEST_CONT( ( aFstColSeq + aColCount ) <= sColSeq ,
                    SKIP_CHECK );

    sColPiecePtr = getColPiece( getDataArea( aSlotPtr ),
                                &sColLen,
                                &sColVal,
                                &sInOutMode );

    /* roop���� ���������� Ż���ϴ� ����
     * 1. qp���� ������ column list�� column seq�� row piece�� ������
     *    column seq�� �Ѿ ���, ���� row piece�� Ȯ���Ѵ�.
     * 2. qp���� ������ column list�� null�� ���, ��� �˻��Ͽ���.
     * 3. ���� row piece�� �̾��� column value�� �߰��� ���
     */
    sChkVal = aCompColumns->mCurValue;

    while( ( aFstColSeq + aColCount ) > sColSeq )
    {
        /* Row�� column value�� ũ�Ⱑ QP���� ������ �� ���� ũ�� */
        while( ( sColSeqInRowPiece + aFstColSeq ) < sColSeq )
        {
            sColPiecePtr += sColLen;
            sColPiecePtr = getColPiece( sColPiecePtr,
                                        &sColLen,
                                        &sColVal,
                                        &sInOutMode );
            sColSeqInRowPiece++;
        }

        if ( ( sColLen + sOffset ) > sChkVal->length )
        {
            /* Row�� column value�� ũ�Ⱑ QP���� ������ �� ���� ũ�� */
            sIsSameColumnValue = ID_FALSE;
            break;
        }

        if ( idlOS::memcmp( (UChar*)sChkVal->value + sOffset,
                            sColPiecePtr,
                            sColLen ) != 0 )
        {
            sIsSameColumnValue = ID_FALSE;
            break;
        }

        if ( ( sColLen + sOffset ) < sChkVal->length )
        {
            if ( ( ( aFstColSeq + aColCount - 1 ) == sColSeq ) &&
                 ( SM_IS_FLAG_ON( aRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE ) )
            {
                // ������ column�̰� next row piece�� �ִ� ���
                // �缳��
                aCompColumns->mCurOffset = sOffset + sColLen;
            }
            else
            {
                // ������ column�� �ƴ� ��� ���̰� �ٸ����̴�.
                sIsSameColumnValue = ID_FALSE;
            }

            break;
        }

        if ( ( ( aFstColSeq + aColCount - 1 ) == sColSeq ) &&
             ( SM_IS_FLAG_ON( aRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE ) )
        {
            // ���̰� �� �°� �������µ� next�� �ִ� ���
            // size�� �ٸ����̴�.
            sIsSameColumnValue = ID_FALSE;
            break;
        }

        aCompColumns->mCurOffset = 0;

        /* next column piece�� �̵� */
        sChkVal++;
        sOffset = 0;
        sChkColList = sChkColList->next;

        if ( sChkColList == NULL )
        {
            /* QP���� ���� �� Column list�� ��� �˻��Ͽ��� */
            break;
        }

        sChkCol = sChkColList->column;
        IDE_ASSERT( sColSeq <= SDC_GET_COLUMN_SEQ( sChkCol ) );
        sColSeq = SDC_GET_COLUMN_SEQ( sChkCol );
    }

    aCompColumns->mCurValue  = sChkVal;
    aCompColumns->mCurColumn = sChkColList;

    IDE_EXCEPTION_CONT( SKIP_CHECK );

    return sIsSameColumnValue;
}

/***********************************************************************
 *
 * Description :
 *  update�ÿ� insert rowpiece for update ������ �ϱ� ���ؼ�
 *  sdcRowPieceUpdateInfo�κ��� sdcRowPieceInsertInfo�� ����� �Լ��̴�.
 *
 **********************************************************************/
void sdcRow::makeInsertInfoFromUptInfo( void                         *aTableHeader,
                                        const sdcRowPieceUpdateInfo  *aUpdateInfo,
                                        UShort                        aColCount,
                                        UShort                        aFstColumnSeq,
                                        sdcRowPieceInsertInfo        *aInsertInfo )
{
    const sdcColumnInfo4Update  *sColInfo4Upt;
    sdcColumnInfo4Insert        *sColInfo4Ins;
    UShort                       sColSeqInRowPiece;
    UShort                       sColumnSeq;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );
    IDE_ASSERT( aInsertInfo  != NULL );

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColInfo4Upt = aUpdateInfo->mColInfoList + sColSeqInRowPiece;
        sColInfo4Ins = aInsertInfo->mColInfoList + sColSeqInRowPiece;

        /* mColumn�� ���� ���� update �÷����� ���θ� �Ǵ��Ѵ�.
         * mColumn == NULL : update �÷� X
         * mColumn != NULL : update �÷� O */
        if ( SDC_IS_UPDATE_COLUMN( sColInfo4Upt->mColumn ) == ID_TRUE )
        {
            /* update �÷��� ��� */
            sColInfo4Ins->mColumn =
                (const smiColumn*)sColInfo4Upt->mColumn;

            sColInfo4Ins->mValueInfo.mValue = sColInfo4Upt->mNewValueInfo.mValue;
            sColInfo4Ins->mValueInfo.mOutValue = sColInfo4Upt->mNewValueInfo.mOutValue;
            
            sColInfo4Ins->mValueInfo.mInOutMode =
                sColInfo4Upt->mNewValueInfo.mInOutMode;

            sColInfo4Ins->mIsUptCol = ID_TRUE;
        }
        else
        {
            /* update �÷��� �ƴ� ��� */
            sColumnSeq = aFstColumnSeq + sColSeqInRowPiece;

            sColInfo4Ins->mColumn =
                smLayerCallback::getColumn( aTableHeader, sColumnSeq );

            sColInfo4Ins->mValueInfo.mValue = sColInfo4Upt->mOldValueInfo.mValue;
            sColInfo4Ins->mValueInfo.mOutValue.value = NULL;
            sColInfo4Ins->mValueInfo.mOutValue.length = 0;

            sColInfo4Ins->mValueInfo.mInOutMode =
                sColInfo4Upt->mOldValueInfo.mInOutMode;

            sColInfo4Ins->mIsUptCol = ID_FALSE;
        }
    }

    aInsertInfo->mStartColSeq       = 0;
    aInsertInfo->mStartColOffset    = 0;
    aInsertInfo->mEndColSeq         = 0;
    aInsertInfo->mEndColOffset      = 0;
    aInsertInfo->mRowPieceSize      = 0;
    aInsertInfo->mColCount          = 0;
    aInsertInfo->mLobDescCnt        = 0;
    aInsertInfo->mIsInsert4Upt      = ID_TRUE;
    aInsertInfo->mIsUptLobByAPI     = aUpdateInfo->mIsUptLobByAPI;
}


/***********************************************************************
 *
 * Description :
 *
 **********************************************************************/
void sdcRow::makeOverwriteInfo( const sdcRowPieceInsertInfo    *aInsertInfo,
                                const sdcRowPieceUpdateInfo    *aUpdateInfo,
                                sdSID                           aNextRowPieceSID,
                                sdcRowPieceOverwriteInfo       *aOverwriteInfo )
{
    const sdcColumnInfo4Update   *sColumnInfo;
    UShort                        sUptAftInModeColCnt = 0;
    UShort                        sUptAftLobDescCnt   = 0;
    UShort                        sColSeqInRowPiece;

    IDE_ASSERT( aInsertInfo    != NULL );
    IDE_ASSERT( aUpdateInfo    != NULL );
    IDE_ASSERT( aOverwriteInfo != NULL );
    IDE_ASSERT( SDC_IS_FIRST_PIECE_IN_INSERTINFO( aInsertInfo )
                 == ID_TRUE );

    SDC_INIT_OVERWRITE_INFO(aOverwriteInfo, aUpdateInfo);

    aOverwriteInfo->mNewRowHdrInfo->mColCount = aInsertInfo->mColCount;

    aOverwriteInfo->mNewRowHdrInfo->mRowFlag =
        calcRowFlag4Update( aUpdateInfo->mNewRowHdrInfo->mRowFlag,
                            aInsertInfo,
                            aNextRowPieceSID );

    aOverwriteInfo->mNewRowPieceSize = aInsertInfo->mRowPieceSize;

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece <= aInsertInfo->mEndColSeq;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_TRUE )
        {
            if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mNewValueInfo ) == ID_TRUE )
            {
                sUptAftInModeColCnt++;
            }
            else
            {
                sUptAftLobDescCnt++;
            }
        }
    }

    aOverwriteInfo->mLstColumnOverwriteSize = aInsertInfo->mEndColOffset;

    aOverwriteInfo->mUptAftInModeColCnt = sUptAftInModeColCnt;
    aOverwriteInfo->mUptAftLobDescCnt   = sUptAftLobDescCnt;
}

/***********************************************************************
 *
 * Description :
 *  pk ������ �ʱ�ȭ�Ѵ�.
 *
 **********************************************************************/
void sdcRow::makePKInfo( void         *aTableHeader,
                         sdcPKInfo    *aPKInfo )
{
    void                *sIndexHeader;
    const smiColumn     *sColumn;
    sdcColumnInfo4PK    *sColumnInfo;
    UShort               sColCount;
    UShort               sColSeq;
    UShort               sOffset = 0;
    UShort               sPhysicalColumnID;

    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aPKInfo      != NULL );

    /* 0��° index�� PK index�̴�. */
    sIndexHeader = (void*)smLayerCallback::getTableIndex( aTableHeader, 0 );

    sColCount = smLayerCallback::getColumnCountOfIndexHeader( sIndexHeader );

    for(sColSeq = 0; sColSeq < sColCount; sColSeq++)
    {
        sPhysicalColumnID =
            ( *(smLayerCallback::getColumnIDPtrOfIndexHeader( sIndexHeader, sColSeq ))
              & SMI_COLUMN_ID_MASK );

        sColumn = smLayerCallback::getColumn( aTableHeader, sPhysicalColumnID );

        sColumnInfo = aPKInfo->mColInfoList + sColSeq;
        sColumnInfo->mColumn                  = sColumn;
        sColumnInfo->mValue.length = 0;
        sColumnInfo->mValue.value  = aPKInfo->mSpace4CopyPKValue + sOffset;

        sOffset += sColumn->size;

    }

    aPKInfo->mTotalPKColCount    = sColCount;
    aPKInfo->mCopyDonePKColCount = 0;
    aPKInfo->mFstColumnSeq       = 0;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece���� pk column�� �ִ����� ã��,
 *  ���� ��� pk column ������ sdcPKInfo�� �����Ѵ�.
 *
 **********************************************************************/
void sdcRow::copyPKInfo( const UChar                   *aSlotPtr,
                         const sdcRowPieceUpdateInfo   *aUpdateInfo,
                         UShort                         aColCount,
                         sdcPKInfo                     *aPKInfo )
{
    const sdcColumnInfo4Update   *sColumnInfo4Upt;
    sdcColumnInfo4PK             *sColumnInfo4PK;
    UChar                        *sWritePtr;
    UShort                        sColumnSeq;
    UShort                        sColSeqInRowPiece;
    UShort                        sPKColSeq;
    UShort                        sPKColCount;
    UShort                        sFstColumnSeq;
    UChar                         sRowFlag;
    idBool                        sFind;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );
    IDE_ASSERT( aPKInfo     != NULL );
    IDE_ASSERT_MSG( aPKInfo->mTotalPKColCount > aPKInfo->mCopyDonePKColCount,
                    "Total PK Column Count     : %"ID_UINT32_FMT"\n"
                    "Copy Done PK Column Count : %"ID_UINT32_FMT"\n",
                    aPKInfo->mTotalPKColCount,
                    aPKInfo->mCopyDonePKColCount );

    SDC_GET_ROWHDR_1B_FIELD(aSlotPtr, SDC_ROWHDR_FLAG, sRowFlag);

    sPKColCount   = aPKInfo->mTotalPKColCount;
    sFstColumnSeq = aPKInfo->mFstColumnSeq;

    /* �� for loop�� row piece���� ��� �÷��� ��ȸ�Ѵ�. */
    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo4Upt = aUpdateInfo->mColInfoList + sColSeqInRowPiece;
        sColumnSeq      = (sFstColumnSeq + sColSeqInRowPiece);

        sFind     = ID_FALSE;
        sPKColSeq = 0;

        while(1)
        {
            sColumnInfo4PK = aPKInfo->mColInfoList + sPKColSeq;
            if ( SDC_GET_COLUMN_SEQ(sColumnInfo4PK->mColumn) == sColumnSeq )
            {
                sFind = ID_TRUE;
                break;
            }
            else
            {
                if ( (sPKColSeq == (sPKColCount-1)) )
                {
                    sFind = ID_FALSE;
                    break;
                }
                else
                {
                    sPKColSeq++;
                }
            }
        }

        if ( sFind == ID_TRUE )
        {
            /* pk column�� ã�� ��� */
            if ( sColumnInfo4PK->mColumn->size < 
                             (sColumnInfo4PK->mValue.length +
                                  sColumnInfo4Upt->mOldValueInfo.mValue.length) )
            {
                ideLog::log( IDE_ERR_0,
                             "Column->size: %"ID_UINT32_FMT", "
                             "Value Length: %"ID_UINT32_FMT", "
                             "Old Value Length: %"ID_UINT32_FMT"\n",
                             sColumnInfo4PK->mColumn->size,
                             sColumnInfo4PK->mValue.length,
                             sColumnInfo4Upt->mOldValueInfo.mValue.length );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sColumnInfo4Upt,
                                ID_SIZEOF(sdcColumnInfo4Update),
                                "ColumnInfo4Upt Dump:" );

                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)sColumnInfo4PK,
                                ID_SIZEOF(sdcColumnInfo4PK),
                                "ColumnInfo4PK Dump:" );

                IDE_ASSERT( 0 );
            }

            sWritePtr =
                (UChar*)sColumnInfo4PK->mValue.value +
                sColumnInfo4PK->mValue.length;

            /* pk column value ���� */
            idlOS::memcpy( sWritePtr,
                           sColumnInfo4Upt->mOldValueInfo.mValue.value,
                           sColumnInfo4Upt->mOldValueInfo.mValue.length );

            sColumnInfo4PK->mValue.length +=
                sColumnInfo4Upt->mOldValueInfo.mValue.length;

            sColumnInfo4PK->mInOutMode =
                sColumnInfo4Upt->mOldValueInfo.mInOutMode;

            if ( ( sColSeqInRowPiece == (aColCount-1) ) &&
                ( SM_IS_FLAG_ON(sRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE ) )
            {
                /* Nothing to do */
            }
            else
            {
                aPKInfo->mCopyDonePKColCount++;
            }
        }
    }

    // reset mFstColumnSeq
    if ( SM_IS_FLAG_ON( sRowFlag, SDC_ROWHDR_N_FLAG ) == ID_TRUE )
    {
        aPKInfo->mFstColumnSeq += (aColCount - 1);
    }
    else
    {
        aPKInfo->mFstColumnSeq += aColCount;
    }
}


/***********************************************************************
 *
 * Description :
 *  sdcRowPieceUpdateInfo�� trailing null update column ������ �߰��Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::makeTrailingNullUpdateInfo(
                                void                  * aTableHeader,
                                sdcRowHdrInfo         * aRowHdrInfo,
                                sdcRowPieceUpdateInfo * aUpdateInfo,
                                UShort                  aFstColumnSeq,
                                const smiColumnList   * aColList,
                                const smiValue        * aValueList,
                                const sdcColInOutMode * aValueModeList )
{
    const smiValue          * sUptVal;
    const smiColumnList     * sUptColList;
    const sdcColInOutMode   * sUptValMode; 
    sdcColumnInfo4Update    * sColumnInfo;
    UShort                    sColCount;
    UShort                    sColumnSeq;
    UShort                    sColumnSeqInRowPiece;
    idBool                    sFind;
    UInt                      sTotalColCount;
    smiValue                  sDummyNullValue;

    IDE_ASSERT( aTableHeader    != NULL );
    IDE_ASSERT( aRowHdrInfo     != NULL );
    IDE_ASSERT( aUpdateInfo     != NULL );
    IDE_ASSERT( aColList        != NULL );
    IDE_ASSERT( aValueList      != NULL );

    sDummyNullValue.length = 0;
    sDummyNullValue.value  = NULL;

    sColCount      = aRowHdrInfo->mColCount;
    sTotalColCount = smLayerCallback::getColumnCount( aTableHeader );

    sUptColList = aColList;
    sUptVal     = aValueList;
    sUptValMode = aValueModeList;

    /* ù��° trailing null column sequence��
     * �������� ����� ������ column sequence + 1 �̴�. */
    sColumnSeqInRowPiece = sColCount;
    sColumnSeq           = aFstColumnSeq + sColumnSeqInRowPiece;

    while( SDC_GET_COLUMN_SEQ(sUptColList->column) < sColumnSeq )
    {
        /* trailing null update column�� ���ö�����
         * update column list�� �̵���Ų��. */
        IDE_ASSERT( sUptColList->next != NULL );

        sUptColList = sUptColList->next;
        sUptVal++;
    }
    /* �ݵ�� trailing null update column�� �����ؾ� �Ѵ�. */
    IDE_ASSERT( sUptColList != NULL );

    IDU_FIT_POINT_RAISE( "1.BUG-47745@sdcRow::makeTrailingNullUpdateInfo", error_getcolumn );

    while(sUptColList != NULL)
    {
        /* BUG-32234: If update operation fails to make trailing null,
         * debug information needs to be recorded. */
        IDE_TEST_RAISE( sColumnSeq >= sTotalColCount, error_getcolumn );

        sColumnInfo = aUpdateInfo->mColInfoList + sColumnSeqInRowPiece;

        /* old value�� �翬�� NULL�̴�. */
        sColumnInfo->mOldValueInfo.mValue.length = 0;
        sColumnInfo->mOldValueInfo.mValue.value  = NULL;
        sColumnInfo->mOldValueInfo.mInOutMode    = SDC_COLUMN_IN_MODE;

        if ( SDC_GET_COLUMN_SEQ(sUptColList->column) == sColumnSeq )
        {
            /* trailing null update �÷��� ã�� ��� */
            sColumnInfo->mColumn = (const smiColumn*)sUptColList->column;
            sColumnInfo->mNewValueInfo.mValue = *sUptVal;
            sColumnInfo->mNewValueInfo.mInOutMode = SDC_COLUMN_IN_MODE;
            /* BUG-44082 [PROJ-2506] Insure++ Warning
             * - �ʱ�ȭ�� �ʿ��մϴ�.
             * - sdcRow::makeUpdateInfo()�� �����Ͽ����ϴ�.(sdcRow.cpp:12156)
             */
            sColumnInfo->mNewValueInfo.mOutValue.value  = NULL;
            sColumnInfo->mNewValueInfo.mOutValue.length = 0;
            sColumnInfo->mNewValueInfo.mInOutMode = SDC_COLUMN_IN_MODE;

            sFind = ID_TRUE;

            if ( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE )
            {
                if ( sUptValMode == NULL )
                {
                    if ( SDC_GET_COLUMN_INOUT_MODE(sColumnInfo->mColumn,
                                                   sColumnInfo->mNewValueInfo.mValue.length)
                         == SDC_COLUMN_OUT_MODE_LOB )
                    {
                        sColumnInfo->mNewValueInfo.mOutValue =
                            sColumnInfo->mNewValueInfo.mValue;
            
                        sColumnInfo->mNewValueInfo.mValue.value  = &sdcLob::mEmptyLobDesc;
                        sColumnInfo->mNewValueInfo.mValue.length = ID_SIZEOF(sdcLobDesc);
                        sColumnInfo->mNewValueInfo.mInOutMode    = SDC_COLUMN_OUT_MODE_LOB;
                    }
                    else
                    {
                        sColumnInfo->mNewValueInfo.mOutValue.value  = NULL;
                        sColumnInfo->mNewValueInfo.mOutValue.length = 0;
                        sColumnInfo->mNewValueInfo.mInOutMode       = SDC_COLUMN_IN_MODE;

                    }
                }
                else
                {
                    sColumnInfo->mNewValueInfo.mInOutMode = *sUptValMode;
                }
            }
        }
        else
        {
            /* �ٸ� trailing null column�� update������ ���ؼ�,
             * ���޾� 1byte NULL(0xFF)�� ����ǰ� �� ���.
             *
             * ex)
             *  t1 ���̺� �ټ����� �÷�(c1, c2, c3, c4, c5)�� �ִ�.
             *  insert���� c1, c2�� �����ϰ� �������� ��� trailing null�̾���.
             *  �̶� c4 trailing null column�� update�ϰ� �Ǹ�,
             *  c4 ������ c3 �÷��� ���� trailing null�� �ƴϰ� �ǹǷ�
             *  1byte NULL(0xFF)�� ����ȴ�.
             *  ������ c4 ���� c5 �÷��� ������ trailing null�̴�.
             */
            sColumnInfo->mColumn =
                (smiColumn*)smLayerCallback::getColumn( aTableHeader,
                                                        sColumnSeq );

            /* BUG-32234: If update operation fails to make trailing null,
             * debug information needs to be recorded. */
            IDE_TEST_RAISE( sColumnInfo->mColumn == NULL, error_getcolumn );
            
            sColumnInfo->mNewValueInfo.mValue     = sDummyNullValue;
            sColumnInfo->mNewValueInfo.mInOutMode = SDC_COLUMN_IN_MODE;

            sFind = ID_FALSE;
        }

        aUpdateInfo->mTrailingNullUptCount++;
        aUpdateInfo->mUptBfrInModeColCnt++;

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mNewValueInfo ) == ID_TRUE )
        {
            aUpdateInfo->mUptAftInModeColCnt++;
        }
        else
        {
            // PROJ-1862 In-Mode LOB ���� ���Ͽ� LOB Column�� ������
            // Trailing NULL, ���Ŀ� Out Mode �� �� �ֽ��ϴ�.
            aUpdateInfo->mUptAftLobDescCnt++;

            IDE_ERROR( sColumnInfo->mNewValueInfo.mValue.length
                       == ID_SIZEOF(sdcLobDesc) );
        }
        
        aUpdateInfo->mNewRowPieceSize +=
            SDC_GET_COLPIECE_SIZE(sColumnInfo->mNewValueInfo.mValue.length);

        aRowHdrInfo->mColCount++;

        /* ���� �÷��� ó���ϱ� ����
         * column sequence ������Ŵ */
        sColumnSeqInRowPiece++;
        sColumnSeq++;

        if ( sFind == ID_TRUE )
        {
            /* trailing null update �÷��� ã�� ���,
             * list�� next�� �̵��Ѵ�. */
            sUptColList = sUptColList->next;
            sUptVal++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_getcolumn );
    {
        ideLog::log( IDE_ERR_0,
                     "TotalColCount:       %"ID_UINT32_FMT"\n"
                     "ColCount:            %"ID_UINT32_FMT"\n"
                     "ColumnSeq:           %"ID_UINT32_FMT"\n"
                     "FstColumnSeq:        %"ID_UINT32_FMT"\n"
                     "TotalColCount:       %"ID_UINT32_FMT"\n"
                     "ColumnSeqInRowPiece: %"ID_UINT32_FMT"\n",
                     sTotalColCount,
                     sColCount,
                     sColumnSeq,
                     aFstColumnSeq,
                     smLayerCallback::getColumnCount( aTableHeader ),
                     sColumnSeqInRowPiece );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aTableHeader,
                        ID_SIZEOF(smcTableHeader),
                        "[ Table Header ]\n"  );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo->mOldRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ Old Row Header Info ]\n" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo->mNewRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "[ New Row Header Info ]\n" );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar *)aUpdateInfo,
                        ID_SIZEOF(sdcRowPieceUpdateInfo),
                        "[ Update Info ]\n");

        sUptColList = aColList;
        sUptVal     = aValueList;
            
        while(sUptColList != NULL)
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)(sUptColList->column),
                            ID_SIZEOF(smiColumn),
                            "[ Column ]\n" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)sUptVal,
                            ID_SIZEOF(smiValue),
                            "[ Value ]\n" );
                
            sUptColList = sUptColList->next;
            sUptVal++;
        }

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, 
                                  "Make Trailing Null Update Info" ) );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  rowpiece�� ũ�⸦ ����ϴ� �Լ��̴�.
 *  rowpiece size�� ���� �������� �ʱ� ������,
 *  �ʿ�� rowpiece�� ũ�⸦ ����ؾ� �Ѵ�.
 *  (rowpiece�� size�� �˾ƾ� �ϴ� ���� ���� �ʴ�.)
 *
 **********************************************************************/
UShort sdcRow::getRowPieceSize( const UChar    *aSlotPtr )
{
    UShort    sRowPieceSize = 0;
    UShort    sColCount;
    UChar    *sColPiecePtr;
    UInt      sColLen;
    UShort    sColSeqInRowPiece;

    IDE_ASSERT( aSlotPtr != NULL );

    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);

    sColPiecePtr = getDataArea( aSlotPtr );
    sRowPieceSize = (sColPiecePtr - aSlotPtr);

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < sColCount;
          sColSeqInRowPiece++ )
    {
        sColPiecePtr = getColPiece( sColPiecePtr,
                                    &sColLen,
                                    NULL,   // aColVal
                                    NULL ); // aInOutMode

        sRowPieceSize += SDC_GET_COLPIECE_SIZE(sColLen);

        sColPiecePtr += sColLen;
    }

    // BUG-27453 Klocwork SM
    if ( sRowPieceSize > SDC_MAX_ROWPIECE_SIZE_WITHOUT_CTL )
    {
        ideLog::log( IDE_ERR_0,
                     "ColCount: %"ID_UINT32_FMT", "
                     "RowPieceSize: %"ID_UINT32_FMT"\n",
                     sColCount,
                     sRowPieceSize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    return sRowPieceSize;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UShort sdcRow::getRowPieceBodySize( const UChar    *aSlotPtr )
{
    UShort    sRowPieceBodySize = 0;
    UChar     sRowFlag;

    IDE_ASSERT( aSlotPtr != NULL );

    SDC_GET_ROWHDR_1B_FIELD(aSlotPtr, SDC_ROWHDR_FLAG, sRowFlag);

    sRowPieceBodySize =
        getRowPieceSize(aSlotPtr) - SDC_ROWHDR_SIZE;

    if ( SDC_IS_LAST_ROWPIECE(sRowFlag) != ID_TRUE )
    {
        sRowPieceBodySize -= SDC_EXTRASIZE_FOR_CHAINING;
    }

    // BUG-27453 Klocwork SM
    if ( sRowPieceBodySize > SDC_MAX_ROWPIECE_SIZE_WITHOUT_CTL )
    {
        ideLog::log( IDE_ERR_0,
                     "RowPieceBodySize: %"ID_UINT32_FMT"\n",
                     sRowPieceBodySize );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }


    return sRowPieceBodySize;
}


/***********************************************************************
 *
 * Description :
 *  minimum rowpiece size�� ��ȯ�Ѵ�.
 *
 **********************************************************************/
UShort sdcRow::getMinRowPieceSize()
{
    return SDC_MIN_ROWPIECE_SIZE;
}


/***********************************************************************
 *
 * Description :
 *
 **********************************************************************/
void sdcRow::getColumnPiece( const UChar    *aSlotPtr,
                             UShort          aColumnSeq,
                             UChar          *aColumnValueBuf,
                             UShort          aColumnValueBufLen,
                             UShort         *aColumnLen )
{
    UChar    *sColPiecePtr;
    UShort    sColCount;
    UInt      sColumnLen;
    UShort    sCopySize;
    UShort    sLoop;

    IDE_ASSERT( aSlotPtr        != NULL );
    IDE_ASSERT( aColumnValueBuf != NULL );
    IDE_ASSERT( aColumnLen      != NULL );
    IDE_ASSERT( aColumnValueBufLen > 0  );

    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);
    IDE_ASSERT_MSG( aColumnSeq < sColCount,
                    "Column Seq : %"ID_UINT32_FMT"\n"
                    "Column Cnt : %"ID_UINT32_FMT"\n",
                    aColumnSeq,
                    sColCount );

    sColPiecePtr = getDataArea( aSlotPtr );

    sLoop = 0;
    while( sLoop != aColumnSeq )
    {
        sColPiecePtr = getNxtColPiece(sColPiecePtr);
        sLoop++;
    }

    sColPiecePtr = getColPiece( sColPiecePtr,
                                &sColumnLen,
                                NULL,   // aColVal
                                NULL ); // aInOutMode

    if ( sColumnLen > aColumnValueBufLen )
    {
        sCopySize = aColumnValueBufLen;
    }
    else
    {
        sCopySize = sColumnLen;
    }

    idlOS::memcpy( aColumnValueBuf,
                   sColPiecePtr,
                   sCopySize );

    *aColumnLen = sColumnLen;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
UChar* sdcRow::getNxtColPiece( const UChar    *aColPtr )
{
    UChar     *sColPiecePtr = (UChar*)aColPtr;
    UInt      sColLen;

    IDE_ASSERT( aColPtr != NULL );

    sColLen = getColPieceLen( sColPiecePtr );

    sColPiecePtr += SDC_GET_COLPIECE_SIZE(sColLen);

    return sColPiecePtr;
}


/***********************************************************************
 * Description : getColPieceLen
 *
 *   aColPtr - [IN] ���̸� Ȯ���� Column Piece�� Ptr
 **********************************************************************/
UInt sdcRow::getColPieceLen( const UChar    *aColPtr )
{
    UInt  sColLen;

    (void)getColPieceInfo( aColPtr,
                           NULL, /* aPrefix */
                           &sColLen,
                           NULL, /* aColVal */
                           NULL, /* aColStoreSize */
                           NULL  /* aInOutMode */ );
    return sColLen;
}

/***********************************************************************
 *
 * Description :
 *  insert rowpiece ������ �����ϱ� ����, rowpiece�� flag�� ����ϴ� �Լ��̴�.
 *  insert DML ����ÿ��� �� �Լ��� ȣ���ϰ�, update DML ����ÿ���
 *  calcRowFlag4Update() �Լ��� ȣ���ؾ� �Ѵ�.
 *
 **********************************************************************/
UChar sdcRow::calcRowFlag4Insert( const sdcRowPieceInsertInfo  *aInsertInfo,
                                  sdSID                         aNextRowPieceSID )
{
    const sdcColumnInfo4Insert   *sFstColumnInfo;
    const sdcColumnInfo4Insert   *sLstColumnInfo;
    UChar                         sRowFlag;

    IDE_ASSERT( aInsertInfo != NULL );
    IDE_ASSERT( aInsertInfo->mIsInsert4Upt == ID_FALSE );

    /* ��� flag�� ������ ���¿���, ������ üũ�ϸ鼭
     * flag�� �ϳ��� off ��Ų��. */
    sRowFlag = SDC_ROWHDR_FLAG_ALL;

    if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(aInsertInfo) != ID_TRUE )
    {
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_H_FLAG);
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_F_FLAG);
    }
    
    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        /* next rowpiece�� �����ϸ�
         * L flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_L_FLAG);
    }

    sFstColumnInfo = aInsertInfo->mColInfoList + aInsertInfo->mStartColSeq;
    if ( SDC_IS_IN_MODE_COLUMN( sFstColumnInfo->mValueInfo ) != ID_TRUE )
    {
        if ( aInsertInfo->mStartColOffset != 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "StartColOffset: %"ID_UINT32_FMT"\n",
                         aInsertInfo->mStartColOffset );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aInsertInfo,
                            ID_SIZEOF(sdcRowPieceInsertInfo),
                            "InsertInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sFstColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Insert),
                            "FstColumnInfo Dump:" );

            IDE_ASSERT( 0 );
        }

         /* lob desc�� ������ �������� �����Ƿ�
          * P flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_P_FLAG);
    }
    else
    {
        if ( aInsertInfo->mStartColOffset == 0 )
        {
            /* column value�� ó������ �����ϴ� ���
             * P flag�� off �Ѵ�. */
            SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_P_FLAG);
        }
    }

    sLstColumnInfo = aInsertInfo->mColInfoList + aInsertInfo->mEndColSeq;
    if ( SDC_IS_IN_MODE_COLUMN( sLstColumnInfo->mValueInfo ) != ID_TRUE )
    {
        if ( aInsertInfo->mEndColOffset != ID_SIZEOF(sdcLobDesc) )
        {
            ideLog::log( IDE_ERR_0,
                         "EndColOffset: %"ID_UINT32_FMT"\n",
                         aInsertInfo->mEndColOffset );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aInsertInfo,
                            ID_SIZEOF(sdcRowPieceInsertInfo),
                            "InsertInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sLstColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Insert),
                            "LstColumnInfo Dump:" );

            IDE_ASSERT( 0 );
        }


        /* lob desc�� ������ �������� �����Ƿ�
         * N flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_N_FLAG);
    }
    else
    {
        if ( aInsertInfo->mEndColOffset ==
            sLstColumnInfo->mValueInfo.mValue.length )
        {
            /* column value�� ������ �κб��� �����ϴ� ���,
             * N flag�� off �Ѵ�. */
            SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_N_FLAG);
        }
    }

    return sRowFlag;
}


/***********************************************************************
 *
 * Description :
 *  insert rowpiece ������ �����ϱ� ����, rowpiece�� flag�� ����ϴ� �Լ��̴�.
 *  update DML ����ÿ��� �� �Լ��� ȣ���ϰ�, insert DML ����ÿ���
 *  calcRowFlag4Insert() �Լ��� ȣ���ؾ� �Ѵ�.
 *
 **********************************************************************/
UChar sdcRow::calcRowFlag4Update( UChar                         aInheritRowFlag,
                                  const sdcRowPieceInsertInfo  *aInsertInfo,
                                  sdSID                         aNextRowPieceSID )
{
    const sdcColumnInfo4Insert   *sFstColumnInfo;
    const sdcColumnInfo4Insert   *sLstColumnInfo;
    UChar                         sRowFlag;

    IDE_ASSERT( aInsertInfo != NULL );
    IDE_ASSERT( aInsertInfo->mIsInsert4Upt == ID_TRUE );

    /* rowpiece�� old rowflag�� �������� ����,
     * HFL flag�� ������ üũ�ϸ鼭 off ��Ű��,
     * PN flag�� ������ üũ�ϸ鼭 on or off ��Ų��. */
    sRowFlag = aInheritRowFlag;

    if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(aInsertInfo) != ID_TRUE )
    {
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_H_FLAG);
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_F_FLAG);
    }
    
    if ( aNextRowPieceSID != SD_NULL_SID )
    {
        /* next rowpiece�� �����ϸ�
         * L flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_L_FLAG);
    }

    sFstColumnInfo = aInsertInfo->mColInfoList + aInsertInfo->mStartColSeq;
    if ( SDC_IS_IN_MODE_COLUMN( sFstColumnInfo->mValueInfo ) != ID_TRUE )
    {
        if ( aInsertInfo->mStartColOffset != 0 )
        {
            ideLog::log( IDE_ERR_0,
                         "StartColOffset: %"ID_UINT32_FMT"\n",
                         aInsertInfo->mStartColOffset );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aInsertInfo,
                            ID_SIZEOF(sdcRowPieceInsertInfo),
                            "InsertInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sFstColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Insert),
                            "FstColumnInfo Dump:" );

            IDE_ASSERT( 0 );
        }


         /* lob desc�� ������ �������� �����Ƿ�
          * P flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_P_FLAG);
    }
    else
    {
        if ( sFstColumnInfo->mIsUptCol == ID_TRUE )
        {
            if ( aInsertInfo->mStartColOffset == 0 )
            {
                /* ù��° column�� update column�ε�,
                 * �ش� rowpiece���� column value��
                 * ù�κк��� �����ϴ� ��� P flag�� off �Ѵ�. */
                SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_P_FLAG);
            }
            else
            {
                /* ù��° column�� update column�ε�,
                 * �ش� rowpiece���� column value��
                 * �߰��κк��� �����ϴ� ��� P flag�� on �Ѵ�. */
                SM_SET_FLAG_ON(sRowFlag, SDC_ROWHDR_P_FLAG);
            }
        }
        else
        {
            if ( aInsertInfo->mStartColOffset == 0 )
            {
                /*
                 * BUG-32278 The previous flag is set incorrectly in row flag when
                 * a rowpiece is overflowed.
                 *
                 * ù��° �÷��� ��쿡�� ��� ���� P flag�� �׷��� ����Ѵ�.
                 * ��� ���� flag�� ù��° �÷��� ���� �����̱� �����̴�.
                 * �� �� �÷��� ������ P flag�� off�Ѵ�.
                 */
                if ( aInsertInfo->mStartColSeq != 0 )
                {
                    SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_P_FLAG);
                }
            }
            else
            {
                /* ù��° column�� update column�� �ƴ�����,
                 * split�� ó���ϴ� ��������
                 * column piece�� ����������
                 * column piece�� �߰��κк��� �����ϰ� �Ǿ��ٸ�
                 * P flag�� on �Ѵ�. */
                SM_SET_FLAG_ON(sRowFlag, SDC_ROWHDR_P_FLAG);
            }
        }
    }

    sLstColumnInfo = aInsertInfo->mColInfoList + aInsertInfo->mEndColSeq;
    if ( SDC_IS_IN_MODE_COLUMN( sLstColumnInfo->mValueInfo ) != ID_TRUE )
    {
        if ( aInsertInfo->mEndColOffset != ID_SIZEOF(sdcLobDesc) )
        {
            ideLog::log( IDE_ERR_0,
                         "EndColOffset: %"ID_UINT32_FMT"\n",
                         aInsertInfo->mEndColOffset );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aInsertInfo,
                            ID_SIZEOF(sdcRowPieceInsertInfo),
                            "InsertInfo Dump:" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)sLstColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Insert),
                            "LstColumnInfo Dump:" );

            IDE_ASSERT( 0 );
        }


        /* lob desc�� ������ �������� �����Ƿ�
         * N flag�� off �Ѵ�. */
        SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_N_FLAG);
    }
    else
    {
        if ( sLstColumnInfo->mIsUptCol == ID_TRUE )
        {
            if ( aInsertInfo->mEndColOffset ==
                 sLstColumnInfo->mValueInfo.mValue.length )
            {
                /* ������ column�� update column�ε�,
                 * �ش� rowpiece���� column value��
                 * ������ �κб��� �����ϴ� ���, N flag�� off �Ѵ�. */
                SM_SET_FLAG_OFF(sRowFlag, SDC_ROWHDR_N_FLAG);
            }
            else
            {
                /* ������ column�� update column�ε�,
                 * �ش� rowpiece���� column value��
                 * �߰� �κб����� �����ϴ� ���, N flag�� on �Ѵ�. */
                SM_SET_FLAG_ON(sRowFlag, SDC_ROWHDR_N_FLAG);
            }
        }
        else
        {
            if ( aInsertInfo->mEndColOffset ==
                 sLstColumnInfo->mValueInfo.mValue.length )
            {
                /* ������ column�� update column�� �ƴϸ�,
                 * �ش� rowpiece���� column piece��
                 * ������ �κб��� �����ϴ���
                 * N flag�� off ��Ű�� �ʴ´�.
                 * next rowpiece�� ������ column�� piece��
                 * �������� �ְ� �������� �ֱ� �����̴�. */
            }
            else
            {
                /* ������ column�� update column�� �ƴ�����,
                 * split�� ó���ϴ� ��������
                 * column piece�� ����������
                 * column piece�� �߰��κб����� �����ϰ� �Ǿ��ٸ�
                 * N flag�� on �Ѵ�. */
                SM_SET_FLAG_ON(sRowFlag, SDC_ROWHDR_N_FLAG);
            }
        }
    }

    return sRowFlag;
}


/***********************************************************************
 *
 * Description :
 *  ������������ slot�� ���Ҵ��� �� �ִ����� �˻��ϴ� �Լ��̴�.
 *
 *  aSlotPtr     - [IN] slot pointer
 *  aNewSlotSize - [IN] ���Ҵ��Ϸ��� slot�� ũ��
 *
 **********************************************************************/
idBool sdcRow::canReallocSlot( UChar    *aSlotPtr,
                               UInt      aNewSlotSize )
{
    sdpPhyPageHdr      *sPhyPageHdr;
    UShort              sOldSlotSize;
    UShort              sReserveSize4Chaining;
    UInt                sTotalAvailableSize;

    IDE_ASSERT( aSlotPtr != NULL );

    sPhyPageHdr = sdpPhyPage::getHdr( aSlotPtr );

    if ( aNewSlotSize > (UInt)SDC_MAX_ROWPIECE_SIZE(
                                         sdpPhyPage::getSizeOfCTL(sPhyPageHdr)) )
    {
        return ID_FALSE;
    }

    sOldSlotSize = getRowPieceSize( aSlotPtr );

    /* BUG-25395
     * [SD] alloc slot, free slot�ÿ�
     * row chaining�� ���� ����(6byte)�� ����ؾ� �մϴ�. */
    if ( sOldSlotSize < SDC_MIN_ROWPIECE_SIZE )
    {
        /* slot�� ũ�Ⱑ minimum rowpiece size���� ���� ���,
         * row migration�� �߻������� �������� ��������� �����ϸ�
         * slot ���Ҵ翡 �����Ͽ� ������ ����Ѵ�.
         * �׷��� �̷� ��쿡 ����Ͽ�
         * alloc slot�ÿ�
         * (min rowpiece size - slot size)��ŭ�� ������
         * �߰��� �� Ȯ���� �ξ���.
         */
        sReserveSize4Chaining = SDC_MIN_ROWPIECE_SIZE - sOldSlotSize;
    }
    else
    {
        sReserveSize4Chaining = 0;
    }

    sTotalAvailableSize =
        sOldSlotSize + sReserveSize4Chaining +
        sdpPhyPage::getAvailableFreeSize(sPhyPageHdr);

    if ( sTotalAvailableSize >= aNewSlotSize )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}


/***********************************************************************
 *
 * Description :
 *
 *  reallocSlot = freeSlot + allocSlot
 *
 *  FSC(Free Space Credit)
 *  : update �������� ���� slot�� ũ�Ⱑ �پ�� ���,
 *    �������� ��������� �þ�� �ȴ�.
 *    ���� �� ������ Ʈ������� COMMIT(or ROLLBACK)�Ǳ� ����
 *    �������� �ݳ��ϰ� �Ǹ� �ٸ� Ʈ������� �� ������ ����ع�����
 *    ROLLBACK ������ �Ұ����ϰ� �� �� �ִ�.
 *
 *    �׷��� Ʈ������� COMMIT(or ROLLBACK)�Ǳ� ��������
 *    �� ������ �������� �ݳ����� �ʰ�, ROLLBACK�� ���� reserve�� �δµ�
 *    �� ������ FSC(Free Space Credit)��� �Ѵ�.
 *
 *    EX)
 *     TOFS : 1000 byte, AVSP : 1000 byte
 *
 *     s[0] : 500 byte  ==> 100 byte
 *
 *     TOFS : 1400 byte, AVSP : 1000 byte, FSC : 400 byte
 *
 **********************************************************************/
IDE_RC sdcRow::reallocSlot( sdSID           aSlotSID,
                            UChar          *aOldSlotPtr,
                            UShort          aNewSlotSize,
                            idBool          aReserveFreeSpaceCredit,
                            UChar         **aNewSlotPtr )
{
    sdpPhyPageHdr      *sPhyPageHdr;
    scSlotNum           sOldSlotNum;
    scSlotNum           sNewSlotNum;
    scOffset            sNewSlotOffset;
    UChar              *sNewSlotPtr;
    UShort              sOldSlotSize;
    UShort              sFSCredit;
    UShort              sAvailableFreeSize;
#ifdef DEBUG
    UChar              *sSlotDirPtr;
#endif

    IDE_ASSERT( aSlotSID    != SD_NULL_SID );
    IDE_ASSERT( aOldSlotPtr != NULL );

    sOldSlotSize = getRowPieceSize( aOldSlotPtr );
    if ( sOldSlotSize == aNewSlotSize )
    {
        /* slot size�� ��ȭ�� ���� ���,
         * ���� slot�� �����Ѵ�. */
        sNewSlotPtr = aOldSlotPtr;
        IDE_CONT(skip_realloc);
    }

    sPhyPageHdr = sdpPhyPage::getHdr(aOldSlotPtr);

    IDE_DASSERT( canReallocSlot( aOldSlotPtr, aNewSlotSize )
                 == ID_TRUE );

    sOldSlotNum = SD_MAKE_SLOTNUM(aSlotSID);

    IDE_ERROR( sdpPhyPage::freeSlot4SID( sPhyPageHdr,
                                         aOldSlotPtr,
                                         sOldSlotNum,
                                         sOldSlotSize )
                == IDE_SUCCESS );

    IDE_ERROR( sdpPhyPage::allocSlot4SID( sPhyPageHdr,
                                          aNewSlotSize,
                                          &sNewSlotPtr,
                                          &sNewSlotNum,
                                          &sNewSlotOffset )
               == IDE_SUCCESS );

    if ( sOldSlotNum != sNewSlotNum )
    {
        ideLog::log( IDE_ERR_0,
                     "OldSlotNum: %"ID_UINT32_FMT", "
                     "OldSlotSize: %"ID_UINT32_FMT", "
                     "NewSlotSize: %"ID_UINT32_FMT", "
                     "NewSlotNum: %"ID_UINT32_FMT", "
                     "NewSlotOffset: %"ID_UINT32_FMT"\n",
                     sOldSlotNum,
                     sOldSlotSize,
                     aNewSlotSize,
                     sNewSlotNum,
                     sNewSlotOffset );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     (UChar*)sPhyPageHdr,
                                     "Page Dump:" );

        IDE_ERROR( 0 );
    }

    IDE_ERROR( sNewSlotPtr != NULL );

#ifdef DEBUG
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sNewSlotPtr );
    IDE_DASSERT( sdpSlotDirectory::validate(sSlotDirPtr) == ID_TRUE );
#endif

    if ( aReserveFreeSpaceCredit == ID_TRUE )
    {
        if ( sOldSlotSize > aNewSlotSize )
        {
            /* update�� ���� slot�� ũ�Ⱑ �پ��� ��쿡��
             * FSC�� reserve�Ѵ�. */
            sFSCredit = calcFSCreditSize( sOldSlotSize, aNewSlotSize );

            sAvailableFreeSize =
                sdpPhyPage::getAvailableFreeSize(sPhyPageHdr);

            if ( sAvailableFreeSize < sFSCredit )
            {
                ideLog::log( IDE_ERR_0,
                             "OldSlotSize: %"ID_UINT32_FMT", "
                             "NewSlotSize: %"ID_UINT32_FMT", "
                             "AvailableFreeSize: %"ID_UINT32_FMT", "
                             "FSCSize: %"ID_UINT32_FMT"\n",
                             sOldSlotSize,
                             aNewSlotSize,
                             sAvailableFreeSize,
                             sFSCredit );

                (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                             (UChar*)sPhyPageHdr,
                                             "Page Dump:" );

                IDE_ERROR( 0 );
            }

            sAvailableFreeSize -= sFSCredit;

            /* available size�� FSC ũ�⸸ŭ ����
             * �ٸ� Ʈ������� �� ������ ������� ���ϰ� �Ѵ�. */
            sdpPhyPage::setAvailableFreeSize( sPhyPageHdr,
                                              sAvailableFreeSize );
        }
        else
        {
            /* update�� ���� slot�� ũ�Ⱑ �þ ��쿡��
             * FSC�� reserve�� �ʿ䰡 ����.
             * �ֳ��ϸ� old slot size�� new slot size���� �����Ƿ�
             * rollback�ÿ� old slot size��ŭ�� ������
             * �׻� Ȯ���� �� �ֱ� �����̴�. */
        }
    }

    IDE_EXCEPTION_CONT(skip_realloc);

    *aNewSlotPtr = sNewSlotPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  FSC(Free Space Credit) ũ�⸦ ����Ѵ�.
 *
 *  aOldRowPieceSize - [IN] old rowpiece size
 *  aNewRowPieceSize - [IN] new rowpiece size
 *
 **********************************************************************/
SShort sdcRow::calcFSCreditSize( UShort    aOldRowPieceSize,
                                 UShort    aNewRowPieceSize )
{
    UShort    sOldAllocSize = aOldRowPieceSize;
    UShort    sNewAllocSize = aNewRowPieceSize;

    if ( aOldRowPieceSize < SDC_MIN_ROWPIECE_SIZE )
    {
        sOldAllocSize = SDC_MIN_ROWPIECE_SIZE;
    }

    if ( aNewRowPieceSize < SDC_MIN_ROWPIECE_SIZE )
    {
        sNewAllocSize = SDC_MIN_ROWPIECE_SIZE;
    }

    return (sOldAllocSize - sNewAllocSize);
}


/***********************************************************************
 *
 * Description :
 * record header�� delete ���θ� �����Ѵ�.
 *
 * Implementation :
 *
 **********************************************************************/
idBool sdcRow::isDeleted(const UChar    *aSlotPtr)
{
    smSCN   sInfiniteSCN;

    IDE_ASSERT( aSlotPtr != NULL );

    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_INFINITESCN, &sInfiniteSCN);

    return (SM_SCN_IS_DELETED(sInfiniteSCN) ? ID_TRUE : ID_FALSE);
}


/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 *
 **********************************************************************/
idBool sdcRow::isHeadRowPiece( const UChar    *aSlotPtr )
{
    UChar    sRowFlag;

    IDE_ASSERT( aSlotPtr != NULL );

    SDC_GET_ROWHDR_1B_FIELD(aSlotPtr, SDC_ROWHDR_FLAG, sRowFlag);

    if ( SM_IS_FLAG_ON(sRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}


/***********************************************************************
 *
 * Description : RowPiece�κ��� ������ �����Ѵ�.
 *
 *
 * aSlotPtr    - [IN]  Slot ������
 * aRowHdrInfo - [OUT] RowHdr ������ ���� �ڷᱸ�� ������
 *
 **********************************************************************/
void sdcRow::getRowHdrInfo( const UChar      *aSlotPtr,
                            sdcRowHdrInfo    *aRowHdrInfo )
{
    scPageID    sPID;
    scSlotNum   sSlotNum;

    IDE_ASSERT( aSlotPtr    != NULL );
    IDE_ASSERT( aRowHdrInfo != NULL );

    SDC_GET_ROWHDR_1B_FIELD( aSlotPtr,
                             SDC_ROWHDR_CTSLOTIDX,
                             aRowHdrInfo->mCTSlotIdx );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_INFINITESCN,
                          &(aRowHdrInfo->mInfiniteSCN) );

    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_UNDOPAGEID,  &sPID);
    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_UNDOSLOTNUM,  &sSlotNum);
    aRowHdrInfo->mUndoSID = SD_MAKE_SID( sPID, sSlotNum );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_COLCOUNT,
                          &(aRowHdrInfo->mColCount) );

    SDC_GET_ROWHDR_1B_FIELD( aSlotPtr,
                             SDC_ROWHDR_FLAG,
                             aRowHdrInfo->mRowFlag );

    getRowHdrExInfo(aSlotPtr, &aRowHdrInfo->mExInfo);
}

/***********************************************************************
 *
 * Description : RowPiece�κ��� Ȯ�� ������ �����Ѵ�.
 *
 *
 * aSlotPtr    - [IN]  Slot ������
 * aRowHdrInfo - [OUT] RowHdr Ȯ�������� ���� �ڷᱸ�� ������
 *
 **********************************************************************/
void sdcRow::getRowHdrExInfo( const UChar      *aSlotPtr,
                              sdcRowHdrExInfo  *aRowHdrExInfo )
{
    IDE_ASSERT( aSlotPtr      != NULL );
    IDE_ASSERT( aRowHdrExInfo != NULL );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_TSSLOTPID,
                          &aRowHdrExInfo->mTSSPageID );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_TSSLOTNUM,
                          &aRowHdrExInfo->mTSSlotNum );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_FSCREDIT,
                          &aRowHdrExInfo->mFSCredit );

    SDC_GET_ROWHDR_FIELD( aSlotPtr,
                          SDC_ROWHDR_FSCNORCSCN,
                          &aRowHdrExInfo->mFSCNOrCSCN );
}

/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 *
 **********************************************************************/
void sdcRow::getRowUndoSID( const UChar    *aSlotPtr,
                            sdSID          *aUndoSID )
{
    scPageID    sPID;
    scSlotNum   sSlotNum;

    IDE_ASSERT( aSlotPtr != NULL );
    IDE_ASSERT( aUndoSID != NULL );

    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_UNDOPAGEID, &sPID);
    SDC_GET_ROWHDR_FIELD(aSlotPtr, SDC_ROWHDR_UNDOSLOTNUM, &sSlotNum);

    *aUndoSID = SD_MAKE_SID(sPID, sSlotNum);
}


#ifdef DEBUG
/*******************************************************************************
 * Description : aValueList�� value �鿡 ������ ������ �����Ѵ�.
 *
 * Parameters :
 *      aValueList  - [IN] ������ smiValue ����Ʈ
 *      aCount      - [IN] aValueList�� ����ִ� value�� ����
 ******************************************************************************/
idBool sdcRow::validateSmiValue( const smiValue  *aValueList,
                                 UInt             aCount )
{

    const smiValue  *sCurrVal = aValueList;
    UInt             sLoop;

    IDE_DASSERT( aValueList != NULL );
    IDE_DASSERT( aCount > 0 );

    for ( sLoop = 0; sLoop != aCount; ++sLoop, ++sCurrVal )
    {
        if ( sCurrVal->length != 0 )
        {
            IDE_DASSERT_MSG( sCurrVal->value != NULL,
                             "ValueLst[%"ID_UINT32_FMT"/%"ID_UINT32_FMT"] "
                             "ValuePtr : %"ID_UINT32_FMT", "
                             "Length   : %"ID_UINT32_FMT"\n",
                             sLoop,
                             aCount,
                             sCurrVal->value,
                             sCurrVal->length );
        }
    }

    return ID_TRUE;
}

/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 *
 **********************************************************************/
idBool sdcRow::validateSmiColumnList( const smiColumnList   *aColumnList )
{
    const smiColumnList  *sColumnList;
    UInt                  sPrevColumnID;

    if ( aColumnList == NULL )
    {
        return ID_TRUE;
    }

    sPrevColumnID = aColumnList->column->id;
    sColumnList = aColumnList->next;

    while(sColumnList != NULL)
    {
        /* column list��
         * column id ������������
         * ���ĵǾ� �־�� �Ѵ�.
         * (analyzeRowForUpdate() ����) */
        IDE_ASSERT_MSG( sColumnList->column->id > sPrevColumnID,
                        "prev Column ID: %"ID_UINT32_FMT"\n"
                        "Curr Column ID: %"ID_UINT32_FMT"\n",
                        sPrevColumnID,
                        sColumnList->column->id );

        sPrevColumnID = sColumnList->column->id;
        sColumnList   = sColumnList->next;
    }

    return ID_TRUE;
}

/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 *
 **********************************************************************/
idBool sdcRow::validateSmiFetchColumnList(
                            const smiFetchColumnList   *aFetchColumnList )
{
    const smiFetchColumnList    *sFetchColumnList;
    UInt                         sPrevColumnID;

    if ( aFetchColumnList == NULL )
    {
        return ID_TRUE;
    }

    sPrevColumnID = aFetchColumnList->column->id;
    sFetchColumnList = aFetchColumnList->next;

    while(sFetchColumnList != NULL)
    {
        if ( sFetchColumnList->column->id <= sPrevColumnID )
        {
            ideLog::log( IDE_ERR_0,
                         "CurrColumnID: %"ID_UINT32_FMT", "
                         "prevColumnID: %"ID_UINT32_FMT"\n",
                         sFetchColumnList->column->id,
                         sPrevColumnID );

            /* fetch column list��
             * column id ������������
             * ���ĵǾ� �־�� �Ѵ�.
             * (analyzeRowForFetch() ����) */
            IDE_ASSERT(0);
        }

        sPrevColumnID    = sFetchColumnList->column->id;
        sFetchColumnList = sFetchColumnList->next;
    }

    return ID_TRUE;
}
#endif


/***********************************************************************
 *
 * Description :
 * 
 *    BUG-32278 The previous flag is set incorrectly in row flag when
 *    a rowpiece is overflowed.
 *
 *    ����� �� rowpiece�� rowflag�� �����Ѵ�.
 *    
 **********************************************************************/
idBool sdcRow::validateRowFlag( UChar aRowFlag, UChar aNextRowFlag )
{
    idBool sIsValid   = ID_TRUE;
    idBool sCheckNext = ID_TRUE;

    if ( aNextRowFlag == SDC_ROWHDR_FLAG_ALL )
    {
        sCheckNext = ID_FALSE;
    }

    if ( sCheckNext == ID_TRUE )
    {
        sIsValid = validateRowFlagForward( aRowFlag, aNextRowFlag );
        if ( sIsValid == ID_TRUE )
        {
            sIsValid = validateRowFlagBackward( aRowFlag, aNextRowFlag );
        }
    }
    else
    {
        // H Flag
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE )
        {
            if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE )
            {
                ideLog::log( IDE_ERR_0, "Invalid H Flag" );
                sIsValid = ID_FALSE;
            }
        }

        // F Flag
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE )
        {
            if ( (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE) )
            {
                ideLog::log( IDE_ERR_0, "Invalid F Flag" );
                sIsValid = ID_FALSE;
            }
        }

        // N Flag
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE )
        {
            if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_L_FLAG) == ID_TRUE )
            {
                ideLog::log( IDE_ERR_0, "Invalid N Flag" );
                sIsValid = ID_FALSE;
            }
        }

        // P Flag
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE )
        {
            if ( (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE) ||
                 (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) )
            {
                ideLog::log( IDE_ERR_0, "Invalid P Flag" );
                sIsValid = ID_FALSE;
            }
        }

        // L Flag
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_L_FLAG) == ID_TRUE )
        {
            if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE )
            {
                ideLog::log( IDE_ERR_0, "Invalid L Flag" );
                sIsValid = ID_FALSE;
            }
        }
    }

    if ( sIsValid == ID_FALSE )
    {
        ideLog::log( IDE_ERR_0,
                     "[ Invalid RowFlag ]\n"
                     "* RowFlag:        %"ID_UINT32_FMT"\n"
                     "* NextRowFalg:    %"ID_UINT32_FMT"\n",
                     aRowFlag,
                     aNextRowFlag );
    }

    return sIsValid;
}


/***********************************************************************
 *
 * Description :
 * 
 *    BUG-32278 The previous flag is set incorrectly in row flag when
 *    a rowpiece is overflowed.
 *
 *    aRowFlag ���� ������� aNextRowFlag�� ��ȿ���� �����Ѵ�.
 *    
 **********************************************************************/
idBool sdcRow::validateRowFlagForward( UChar aRowFlag, UChar aNextRowFlag )
{
    idBool  sIsValid = ID_TRUE;
    
    // Common
    if ( (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_L_FLAG) == ID_TRUE) ||
         (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE) )
    {
        ideLog::log( IDE_ERR_0, "Forward: Invalid Flag" );
        sIsValid = ID_FALSE;
    }

    // H Flag
    if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE )
    {
        if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE )
        {
            ideLog::log( IDE_ERR_0, "Forward: Invalid H Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // F Flag
    if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE )
    {
        if ( (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_P_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) )
        {
            ideLog::log( IDE_ERR_0, "Forward: Invalid F Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // N Flag
    if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE )
    {
        if ( (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_P_FLAG) != ID_TRUE) )
        {
            ideLog::log( IDE_ERR_0, "Forward: Invalid N Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // P Flag
    if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE )
    {
        if ( (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_H_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_F_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) )
        {
            ideLog::log( IDE_ERR_0, "Forward: Invalid P Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // L Flag
    if ( SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_L_FLAG) == ID_TRUE )
    {
        ideLog::log( IDE_ERR_0, "Forward: Invalid L Flag" );
        sIsValid = ID_FALSE;
    }

    return sIsValid;
}


/***********************************************************************
 * 
 * Description :
 * 
 *    BUG-32278 The previous flag is set incorrectly in row flag when
 *    a rowpiece is overflowed.
 *
 *    aNextRowFlag ���� ������� aRowFlag�� ��ȿ���� �����Ѵ�.
 *    
 **********************************************************************/
idBool sdcRow::validateRowFlagBackward( UChar aRowFlag, UChar aNextRowFlag )
{
    idBool  sIsValid = ID_TRUE;

    // Common
    if ( (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_L_FLAG) == ID_TRUE) ||
         (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE) )
    {
        ideLog::log( IDE_ERR_0, "Backward: Invalid Flag" );
        sIsValid = ID_FALSE;
    }
    
    // H Flag
    if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_H_FLAG) == ID_TRUE )
    {
        ideLog::log( IDE_ERR_0, "Backward: Invalid H Flag" );
        sIsValid = ID_FALSE;
    }

    // F Flag
    if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE )
    {
        if ( (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_H_FLAG) != ID_TRUE) ||
             (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE) ||
             (SM_IS_FLAG_ON(aRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE) )
        {
            ideLog::log( IDE_ERR_0, "Backward: Invalid F Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // N Flag
    if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_N_FLAG) == ID_TRUE )
    {
        if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_L_FLAG) == ID_TRUE )
        {
            ideLog::log( IDE_ERR_0, "Backward: Invalid N Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // P Flag
    if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_P_FLAG) == ID_TRUE )
    {
        if ( (SM_IS_FLAG_ON(aRowFlag,     SDC_ROWHDR_N_FLAG) != ID_TRUE) ||
             (SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_F_FLAG) == ID_TRUE) )
        {
            ideLog::log( IDE_ERR_0, "Backward: Invalid P Flag" );
            sIsValid = ID_FALSE;
        }
    }

    // L Flag
    if ( SM_IS_FLAG_ON(aNextRowFlag, SDC_ROWHDR_L_FLAG) == ID_TRUE )
    {
        // NA
    }

    return sIsValid;
}


/***********************************************************************
 *
 * Description :
 * 
 *   BUG-32234: A row needs validity of column infomation when it is
 *   inserted or updated.
 *
 * Implementation :
 *
 **********************************************************************/
idBool sdcRow::validateInsertInfo( sdcRowPieceInsertInfo    * aInsertInfo,
                                   UShort                     aColCount,
                                   UShort                     aFstColumnSeq )
{
    idBool                        sIsValid = ID_TRUE;
    const sdcColumnInfo4Insert  * sColumnInfo;
    const smiValue              * sValue;
    const smiColumn             * sColumn;
    UShort                        sColSeq;
    UShort                        i;

    for ( i = 0; i < aColCount; i++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + i;
        sColSeq     = aFstColumnSeq + i;
        
        sValue      = &(sColumnInfo->mValueInfo.mValue);
        sColumn     = sColumnInfo->mColumn;

        // check column sequence
        if ( SDC_GET_COLUMN_SEQ(sColumn) != sColSeq )
        {
            ideLog::log( IDE_ERR_0, "Invalid Column Sequence\n" );
            sIsValid = ID_FALSE;
            break;
        }

        // check column size
        if ( SDC_IS_NULL(sValue) != ID_TRUE )
        {
            if ( sValue->length > sColumn->size )
            {
                ideLog::log( IDE_ERR_0, "Invalid Column Size\n" );
                sIsValid = ID_FALSE;
                break;
            }
        }
    }

    // print debug info
    if ( sIsValid == ID_FALSE )
    {
        ideLog::log( IDE_ERR_0,
                     "i:            %"ID_UINT32_FMT"\n"
                     "ColCount:     %"ID_UINT32_FMT"\n"
                     "FstColumnSeq: %"ID_UINT32_FMT"\n",
                     i,
                     aColCount,
                     aFstColumnSeq );

        if ( sValue != NULL )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)sValue,
                            ID_SIZEOF(smiValue),
                            "[ Value ]\n" );
        }

        if ( sColumn != NULL )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)sColumn,
                            ID_SIZEOF(smiColumn),
                            "[ Column ]\n" );
        }

        for ( i = 0; i < aColCount; i++ )
        {
            sColumnInfo = aInsertInfo->mColInfoList + i;
            sValue      = &(sColumnInfo->mValueInfo.mValue);
            sColumn     = sColumnInfo->mColumn;

            ideLog::log( IDE_ERR_0, "i: %"ID_UINT32_FMT"\n", i );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar *)sColumnInfo,
                            ID_SIZEOF(sdcColumnInfo4Insert),
                            "[ Column Info ]\n"  );

            if ( sValue != NULL )
            {
                ideLog::logMem( IDE_DUMP_0,
                                (UChar *)sValue,
                                ID_SIZEOF(smiValue),
                                "[ smiValue ]\n");
            }

            if ( sColumn != NULL )
            {
                ideLog::logMem( IDE_DUMP_0,
                                (UChar *)sColumn,
                                ID_SIZEOF(smiColumn),
                                "[ smiColumn ]\n" );
            }
        }
    }

    return sIsValid;
}

/***********************************************************************
 *
 * Description : �ֽ� Row ������ �ڽ� ������ �� �ִ��� ��ü���� �Ǵ�
 *
 * aStatitics          - [IN] �������
 * aMtx                - [IN] Mtx ������
 * aMtxSavePoint       - [IN] Mtx�� ���� Savepoint
 * aSpaceID            - [IN] ���̺����̽� ID
 * aSlotSID            - [IN] Target Row�� ���� Slot ID
 * aPageReadMode       - [IN] page read mode(SPR or MPR)
 * aStmtViewSCN        - [IN] Statment�� SCN ( ViewSCN )
 * aCSInfiniteSCN      - [IN] Cursor�� InfiniteSCN
 * aTargetRow          - [IN/OUT] Target Row�� ������
 * aUptState           - [OUT] ���� �Ǵܿ� ���� ��ü���� ���°�
 * aCTSlotIdx          - [IN/OUT] �Ҵ��� CTSlot ��ȣ
 *
 **********************************************************************/
IDE_RC sdcRow::canUpdateRowPiece( idvSQL                 * aStatistics,
                                  sdrMtx                 * aMtx,
                                  sdrSavePoint           * aMtxSavePoint,
                                  scSpaceID                aSpaceID,
                                  sdSID                    aSlotSID,
                                  sdbPageReadMode          aPageReadMode,
                                  smSCN                  * aStmtViewSCN,
                                  smSCN                  * aCSInfiniteSCN,
                                  idBool                   aIsUptLobByAPI,
                                  UChar                 ** aTargetRow,
                                  sdcUpdateState         * aUptState,
                                  UChar                  * aNewCTSlotIdx,
                                  ULong                    aLockWaitMicroSec )
{
    UChar          * sTargetRow;
    sdcUpdateState   sUptState;
    sdrMtxStartInfo  sStartInfo;
    smTID            sWaitTransID;
    idvWeArgs        sWeArgs;
    UChar          * sPagePtr;
    scPageID         sPageID;
    UChar          * sSlotDirPtr;
    UInt             sWeState = 0;

    IDE_DASSERT( sdrMiniTrans::validate(aMtx) == ID_TRUE );

    IDE_ASSERT( aMtxSavePoint  != NULL );
    IDE_ASSERT( aTargetRow     != NULL );
    IDE_ASSERT( aUptState      != NULL );
    IDE_ASSERT( aCSInfiniteSCN != NULL );

    IDE_ASSERT_MSG( SM_SCN_IS_INFINITE( *aCSInfiniteSCN ),
                    "Cursor Infinite SCN : %"ID_UINT64_FMT"\n",
                    *aCSInfiniteSCN );

    IDV_SQL_OPTIME_BEGIN( aStatistics,
                          IDV_OPTM_INDEX_DRDB_DML_RECORD_LOCK_VALIDATE );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    sTargetRow = *aTargetRow;

    while(1)
    {
       IDE_TEST( canUpdateRowPieceInternal(
                             aStatistics,
                             &sStartInfo,
                             (UChar*)sTargetRow,
                             smxTrans::getTSSlotSID( sStartInfo.mTrans ),
                             aPageReadMode,
                             aStmtViewSCN,
                             aCSInfiniteSCN,
                             aIsUptLobByAPI,
                             &sUptState,
                             &sWaitTransID )
                 != IDE_SUCCESS );

        if ( (sUptState == SDC_UPTSTATE_UPDATABLE) ||
             (sUptState == SDC_UPTSTATE_INVISIBLE_MYUPTVERSION) ||
             (sUptState == SDC_UPTSTATE_ALREADY_DELETED) ||
             (sUptState == SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED))
        {
            break;
        }

        IDE_ASSERT( sUptState    == SDC_UPTSTATE_UPDATE_BYOTHER );
        IDE_ASSERT( sWaitTransID != SM_NULL_TID );

        IDE_ASSERT( aMtxSavePoint->mLogSize == 0 );
        IDE_TEST_RAISE( sdrMtxStack::getCurrSize(&aMtx->mLatchStack) != 1,
                        err_invalid_mtx_state );
        IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, aMtxSavePoint )
                  != IDE_SUCCESS );

        IDV_WEARGS_SET( &sWeArgs,
                        IDV_WAIT_INDEX_ENQ_DATA_ROW_LOCK_CONTENTION,
                        0, /* WaitParam1 */
                        0, /* WaitParam2 */
                        0  /* WaitParam3 */ );

        IDE_ASSERT( smLayerCallback::getTransByTID( sWaitTransID )
                    != sStartInfo.mTrans );

        IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );
        sWeState = 1;

        IDE_TEST( smLayerCallback::waitLockForTrans(
                              sStartInfo.mTrans,
                              sWaitTransID,
                              aSpaceID,
                              /* BUG-31359
                               * SELECT ... FOR UPDATE NOWAIT command on disk table
                               * waits for commit of a transaction on other session.
                               *
                               * Wait Time�� ������ ID_ULONG_MAX ��� Cursor property
                               * ���� ��� �� ���� ����ϵ��� �����Ѵ�.
                               * Cursor Property���� ���� ���ϸ� ID_ULONG_MAX��
                               * �� �� �ִ�. */
                              aLockWaitMicroSec ) != IDE_SUCCESS );

        sWeState = 0;
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );


        /* 
         * BUG-37274 repeatable read from disk table is incorrect behavior. 
         * ���� waitLockForTrans�Լ����� �ٸ� tx������ insert�� row��
         * �б����� ����ϴ� �߿� �ش� row�� insert�� tx�� rollback�Ǹ� slot��
         * unused���·� �ٲ�� �ȴ�. �̸� �����ϱ� ���� logic�� �߰���.
         */
        sPageID  = SD_MAKE_PID(aSlotSID);

        IDE_TEST( prepareUpdatePageByPID( aStatistics,
                                          aMtx,
                                          aSpaceID,
                                          sPageID,
                                          aPageReadMode,
                                          &sPagePtr,
                                          aNewCTSlotIdx) 
                  != IDE_SUCCESS );

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sPagePtr );

        IDE_ASSERT( ((sdpPhyPageHdr*)sPagePtr)->mPageID == sPageID );

        if ( sdpSlotDirectory::isUnusedSlotEntry( sSlotDirPtr,
                                                  SD_MAKE_SLOTNUM(aSlotSID) ) 
             == ID_TRUE )
        {
            break;
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                sSlotDirPtr,
                                                SD_MAKE_SLOTNUM(aSlotSID),
                                                &sTargetRow )
                  != IDE_SUCCESS );

        *aTargetRow = sTargetRow;
    }

    IDV_SQL_OPTIME_END( aStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_RECORD_LOCK_VALIDATE );

    *aUptState = sUptState;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_mtx_state );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_Invalid_Mtx_LatchStack_Size) );
    }
    IDE_EXCEPTION_END;

    if ( sWeState != 0 )
    {
        IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
    }

    IDV_SQL_OPTIME_END( aStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_RECORD_LOCK_VALIDATE );

    *aUptState = sUptState;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : �ֽ� Row ������ �ڽ� ������ �� �ִ��� ��ü���� �Ǵ�
 *
 * aStatitics          - [IN] �������
 * aStartInfo          - [IN] Mtx ���� ����
 * aTargetRow          - [IN/OUT] Target Row�� ������
 * aMyTSSlotSID        - [IN] �ڽ��� TSSlot SID
 * aPageReadMode       - [IN] page read mode(SPR or MPR)
 * aStmtViewSCN        - [IN] Statment�� SCN ( ViewSCN )
 * aCSInfiniteSCN      - [IN] Cursor�� InfiniteSCN
 * aUptState           - [OUT] ���� �Ǵܿ� ���� ��ü���� ���°�
 * aWaitTransID        - [OUT] ����ϴ� ��� ����ؾ��� ��� Ʈ������� ID
 *
 **********************************************************************/
IDE_RC sdcRow::canUpdateRowPieceInternal( idvSQL            * aStatistics,
                                          sdrMtxStartInfo   * aStartInfo,
                                          UChar             * aTargetRow,
                                          sdSID               aMyTSSlotSID,
                                          sdbPageReadMode     aPageReadMode,
                                          smSCN             * aStmtViewSCN,
                                          smSCN             * aCSInfiniteSCN,
                                          idBool              aIsUptLobByAPI,
                                          sdcUpdateState    * aUptState,
                                          smTID             * aWaitTransID )
{
    UChar           * sTargetRow;
    smSCN             sRowInfSCN;
    smSCN             sRowCSCN;
    smSCN             sFSCNOrCSCN;
    sdcUpdateState    sUptState;
    idBool            sIsMyTrans;
    UChar           * sPagePtr;
    UChar             sCTSlotIdx;
    smSCN             sMyFstDskViewSCN;
    UInt              sState      = 0;
    idBool            sIsSetDirty = ID_FALSE;
    sdrMtx            sMtx;
    idBool            sIsLockBit   = ID_FALSE;
    idBool            sTrySuccess;
    void            * sObjPtr;
    SShort            sFSCreditSize = 0;
    smSCN             sFstDskViewSCN;
    smxTrans        * sTrans = (smxTrans*)aStartInfo->mTrans;

    IDE_ASSERT( aTargetRow != NULL );
    IDE_ASSERT( aUptState  != NULL );
    IDE_ASSERT( SM_SCN_IS_VIEWSCN(*aStmtViewSCN) );
    IDE_ASSERT( SM_SCN_IS_INFINITE(*aCSInfiniteSCN) );
    IDE_ERROR_RAISE( isHeadRowPiece(aTargetRow) == ID_TRUE, err_no_head_row_piece );

    *aWaitTransID    = SM_NULL_TID;
    sPagePtr         = sdpPhyPage::getPageStartPtr(aTargetRow);
    sTargetRow       = aTargetRow;
    sUptState        = SDC_UPTSTATE_NULL;
    sMyFstDskViewSCN = smxTrans::getFstDskViewSCN( aStartInfo->mTrans );

    sIsMyTrans = isMyTransUpdating( sPagePtr,
                                    aTargetRow,
                                    sMyFstDskViewSCN,
                                    aMyTSSlotSID,
                                    &sFstDskViewSCN );

    SDC_GET_ROWHDR_1B_FIELD(sTargetRow, SDC_ROWHDR_CTSLOTIDX, sCTSlotIdx);
    SDC_GET_ROWHDR_FIELD(sTargetRow, SDC_ROWHDR_INFINITESCN, &sRowInfSCN);
    SDC_GET_ROWHDR_FIELD(sTargetRow, SDC_ROWHDR_FSCNORCSCN, &sFSCNOrCSCN);

    if ( SDC_IS_CTS_LOCK_BIT(sCTSlotIdx) == ID_TRUE )
    {
        sIsLockBit = ID_TRUE;
    }

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    if ( (sCTSlotIdx != SDP_CTS_IDX_UNLK) && (sIsMyTrans == ID_FALSE) )
    {
        sdcTableCTL::getChangedTransInfo( sTargetRow,
                                          &sCTSlotIdx,
                                          &sObjPtr,
                                          &sFSCreditSize,
                                          &sFSCNOrCSCN );

        if ( SDC_CTS_SCN_IS_COMMITTED(sFSCNOrCSCN) )
        {
            /* To Fix BUG-23648 Data �������� RTS ������ CTS�� ���ؼ�
             *                  Row Stamping�� �߻��Ͽ� Redo�� FATAL �߻�!!
             *
             * sdnbBTree::checkUniqueness���� ������ ���
             * S-latch �� ȹ�� �Ǿ� �ֱ� ������ X-latch�� �������ش�. */

            sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                                sPagePtr,
                                                aPageReadMode,
                                                &sTrySuccess );

            if ( sTrySuccess == ID_TRUE )
            {
                // CTS ���º��� 'T' -> 'R'
                IDE_TEST( sdcTableCTL::logAndRunRowStamping( &sMtx,
                                                             sCTSlotIdx,
                                                             sObjPtr,
                                                             sFSCreditSize,
                                                             &sFSCNOrCSCN )
                          != IDE_SUCCESS );

                SDC_GET_ROWHDR_1B_FIELD( sTargetRow,
                                         SDC_ROWHDR_CTSLOTIDX,
                                         sCTSlotIdx );
                IDE_ERROR( sCTSlotIdx == SDP_CTS_IDX_UNLK );

                sIsSetDirty = ID_TRUE;
            }

            sIsLockBit = ID_FALSE;
        }
        else
        {
            IDE_TEST( sdcTableCTL::logAndRunDelayedRowStamping( aStatistics,
                                                                &sMtx,
                                                                sCTSlotIdx,
                                                                sObjPtr,
                                                                aPageReadMode,
                                                                aWaitTransID,
                                                                &sRowCSCN )
                      != IDE_SUCCESS );

            if ( *aWaitTransID == SM_NULL_TID )
            {
                IDE_ERROR( SM_SCN_IS_NOT_INFINITE(sRowCSCN) == ID_TRUE );
                SM_SET_SCN( &sFSCNOrCSCN, &sRowCSCN );

                sIsSetDirty = ID_TRUE;
                sIsLockBit  = ID_FALSE;
            }
        }
    }

    if ( sIsSetDirty == ID_TRUE )
    {
        IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, sPagePtr )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


    if ( SDC_CTS_SCN_IS_COMMITTED(sFSCNOrCSCN) && (sIsLockBit == ID_FALSE) )
    {
        if ( SM_SCN_IS_GT(&sFSCNOrCSCN, aStmtViewSCN) )
        {
            sUptState = SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED;
        }
        else
        {
            if ( SM_SCN_IS_DELETED( sRowInfSCN ) )
            {
                sUptState = SDC_UPTSTATE_ALREADY_DELETED;
            }
            else
            {
                sUptState = SDC_UPTSTATE_UPDATABLE;
            }
        }
    }
    else
    {
        if ( sIsMyTrans == ID_TRUE )
        {
            if ( (sIsLockBit == ID_TRUE) ||
                 (SM_SCN_IS_GT( aCSInfiniteSCN, &sRowInfSCN )) )
            {
                /* PROJ-2694 Fetch Across Rollback
                 * ���� Tx���� holdable cursor�� ������ �� ������ row���
                 * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����. */
                if ( ( sTrans != NULL ) &&
                     ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                     ( SM_SCN_IS_GT( &sTrans->mCursorOpenInfSCN, &sRowInfSCN ) == ID_TRUE ) )
                {
                    sTrans->mIsReusableRollback = ID_FALSE;
                }

                sUptState = SDC_UPTSTATE_UPDATABLE;
            }
            else
            {
                if ( aIsUptLobByAPI == ID_TRUE )
                {
                    // 1. API �� ���� LOB Update�� ��� SCN�� ���� �� �ִ�.
                    // 2. Replication�� ���� Lob API Update�� ���
                    //    ������ Row SCN�� �� Ŭ���� �ִ�.
                    //    Lock Bit�� ���� �����Ƿ� �����Ѵ�.
                    IDE_ASSERT( (SM_SCN_IS_EQ(aCSInfiniteSCN, &sRowInfSCN)) ||
                                (smxTrans::getLogTypeFlagOfTrans(
                                                            aStartInfo->mTrans)
                                 != SMR_LOG_TYPE_NORMAL) );

                    sUptState = SDC_UPTSTATE_UPDATABLE;
                }
                else
                {
                    sUptState = SDC_UPTSTATE_INVISIBLE_MYUPTVERSION;
                }
            }
        }
        else
        {
            sUptState = SDC_UPTSTATE_UPDATE_BYOTHER;
        }
    }

    *aUptState = sUptState;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_no_head_row_piece );
    {
        ideLog::log( IDE_SM_0, "sdcRow::canUpdateRowPieceInternal() - No Head Row Piece");
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    *aUptState = SDC_UPTSTATE_NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Already modified ���� ó�� �Լ�
 *
 *    aMtx           - [IN] Mtx ������
 *    aMtxSavePoint  - [IN] Mtx�� ���� Savepoint
 *
 **********************************************************************/
IDE_RC sdcRow::releaseLatchForAlreadyModify( sdrMtx       * aMtx,
                                             sdrSavePoint * aMtxSavePoint )
{
    IDE_ASSERT( aMtxSavePoint->mLogSize == 0 );

    IDE_TEST_RAISE( sdrMtxStack::getCurrSize( &(aMtx->mLatchStack) ) != 1,
                    err_invalid_mtx_state );

    IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx,
                                              aMtxSavePoint )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_mtx_state );
    {
        ideLog::log( IDE_ERR_0,
                     "Internal mtx misuse\n"
                     "LogSize : %"ID_UINT32_FMT", LatchStack : %"ID_INT32_FMT,
                     aMtxSavePoint->mLogSize,
                     sdrMtxStack::getCurrSize( &(aMtx->mLatchStack) ) );
        IDE_SET( ideSetErrorCode( smERR_ABORT_Invalid_Mtx_LatchStack_Size ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  MVCC ������ �˻��ϸ鼭 �ڽ��� �о���� row�� ã��������
 *  old version�� �����.
 *
 *  aStatistics    - [IN]  �������
 *  aTargetRow     - [IN]  ���� �˻縦 �� row
 *  aIsPersSlot    - [IN]  Persistent�� slot ����, Row Cache�κ���
 *                         �о�� ��쿡 ID_FALSE�� ������
 *  aFetchVersion  - [IN]  � ������ row�� ���� ���� ����
 *  aMyTSSlotSID   - [IN]  TSS's SID
 *  aPageReadMode  - [IN]  page read mode(SPR or MPR)
 *  aMyViewSCN     - [IN]  cursor view scn
 *  aCSInfiniteSCN - [IN]  cursor infinite scn
 *  aTrans         - [IN]  Ʈ����� ����
 *  aLobInfo4Fetch - [IN]  lob descriptor ������ fetch�Ϸ��� �Ҷ�
 *                             �� ���ڷ� �ʿ��� ������ �ѱ��.(only for lob)
 *  aDoMakeOldVersion  - [OUT] old version�� �����Ͽ����� ����
 *  aUndoSID           - [OUT] old version���κ��� Row�� �����Դ��� ����
 *  aOldVersionBuf     - [OUT] dest buffer.
 *                             �� ���ۿ� old version�� �����Ѵ�.
 *                             �ֽ� ������ ��� memcpy�ϴ� �����
 *                             �ٿ��� ������ �������ѷ� �Ͽ��� ������
 *                             �ֽ� ������ �������� �ʴ´�.
 *
 **********************************************************************/
IDE_RC sdcRow::getValidVersion( idvSQL             * aStatistics,
                                UChar              * aTargetRow,
                                idBool               aIsPersSlot,
                                smFetchVersion       aFetchVersion,
                                sdSID                aMyTSSlotSID,
                                sdbPageReadMode      aPageReadMode,
                                const smSCN        * aMyViewSCN,
                                const smSCN        * aCSInfiniteSCN,
                                void               * aTrans,
                                sdcLobInfo4Fetch   * aLobInfo4Fetch,
                                idBool             * aDoMakeOldVersion,
                                sdSID              * aUndoSID,
                                UChar              * aOldVersionBuf )
{
    smOID            sTableOID;
    smSCN            sRowCSCN;
    smSCN            sRowInfSCN;
    UChar           *sTargetRow;
    sdSID            sCurrUndoSID      = SD_NULL_SID;
    UChar           *sPagePtr;
    idBool           sIsMyTrans;
    UChar            sCTSlotIdx;
    smTID            sDummyTID4Wait;
    smSCN            sMyFstDskViewSCN;
    idBool           sMorePrvVersion   = ID_FALSE;
    idBool           sDoMakeOldVersion = ID_FALSE;
    idBool           sTrySuccess;
    SShort           sFSCreditSize;
    void            *sObjPtr;
    smxTrans        *sTrans            = (smxTrans*)aTrans;

    IDE_ASSERT( SM_SCN_IS_VIEWSCN(*aMyViewSCN) );
    IDE_ASSERT( aTargetRow        != NULL );
    IDE_ASSERT( aDoMakeOldVersion != NULL );
    IDE_ASSERT( aOldVersionBuf    != NULL );
    IDE_ASSERT( aTrans            != NULL );
    IDE_ASSERT( aFetchVersion     != SMI_FETCH_VERSION_LAST );

    sPagePtr    = sdpPhyPage::getPageStartPtr(aTargetRow);
    sTableOID   = sdpPhyPage::getTableOID( sPagePtr );

    sTargetRow       = aTargetRow;
    sMorePrvVersion  = ID_TRUE;
    sMyFstDskViewSCN = smxTrans::getFstDskViewSCN( aTrans );

    sIsMyTrans = isMyTransUpdating( sPagePtr,
                                    aTargetRow,
                                    sMyFstDskViewSCN,
                                    aMyTSSlotSID,
                                    &sRowCSCN );

    SDC_GET_ROWHDR_1B_FIELD( aTargetRow, SDC_ROWHDR_CTSLOTIDX, sCTSlotIdx );
    sCTSlotIdx = SDC_UNMASK_CTSLOTIDX(sCTSlotIdx);

    /* TASK-6105 CTS�� ���°� SDP_CTS_IDX_UNLK�ϰ�� row header���� commit SCN��
     * �����;� �Ѵ�. */
    if ( sCTSlotIdx == SDP_CTS_IDX_UNLK )
    {
        IDE_ASSERT( SM_SCN_IS_INIT(sRowCSCN) == ID_TRUE );
        SDC_GET_ROWHDR_FIELD( sTargetRow, SDC_ROWHDR_FSCNORCSCN, &sRowCSCN );
        IDE_DASSERT( SDC_CTS_SCN_IS_COMMITTED(sRowCSCN) );
    }

    SDC_GET_ROWHDR_FIELD( sTargetRow, SDC_ROWHDR_INFINITESCN, &sRowInfSCN );

    /* TASK-6105 �� �ڵ忡�� sRowCSCN�� ������������ commit SCN���� Ȯ���ϰ�
     * commit SCN�� �ƴѰ�� commitSCN�� ���Ѵ�. */
    /* stamping�� �ʿ��� ���̽��� ��� �̸� stamping ���� */
    if ( (sIsMyTrans == ID_FALSE) && (SDC_CTS_SCN_IS_NOT_COMMITTED(sRowCSCN)) ) 
    {
        sdcTableCTL::getChangedTransInfo( sTargetRow,
                                          &sCTSlotIdx,
                                          &sObjPtr,
                                          &sFSCreditSize,
                                          &sRowCSCN );

        /* BUG-26647 - Caching �� �������� ���ؼ��� Stamping��
            * �������� �ʰ� CommitSCN�� Ȯ���Ѵ�. */
        if ( aIsPersSlot == ID_TRUE )
        {
            IDE_TEST( sdcTableCTL::runDelayedStamping( aStatistics,
                                                       aTrans,
                                                       sCTSlotIdx,
                                                       sObjPtr,
                                                       aPageReadMode,
                                                       *aMyViewSCN,
                                                       &sTrySuccess,
                                                       &sDummyTID4Wait, 
                                                       &sRowCSCN,
                                                       &sFSCreditSize )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdcTableCTL::getCommitSCN( aStatistics,
                                                 aTrans,
                                                 sCTSlotIdx,
                                                 sObjPtr,
                                                 *aMyViewSCN,
                                                 &sDummyTID4Wait,  
                                                 &sRowCSCN )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* do nothing */
    }

    /* Commit���� ���� TSS Slot �� INFINITE �� ���� ������. sdcTSSlot::init() ���� 
     * �״ϱ� SM_SCN_IS_COMMITSCN ���� �˻����� */
    if ( SM_SCN_IS_COMMITSCN(sRowCSCN)   &&
         SDC_CTS_SCN_IS_LEGACY(sRowCSCN) )
    {
        IDE_ERROR( sIsMyTrans == ID_FALSE );

        if ( isMyLegacyTransUpdated( aTrans,
                                     *aCSInfiniteSCN,
                                     sRowInfSCN,
                                     sRowCSCN )
             == ID_TRUE )
        {
            sIsMyTrans = ID_TRUE;
        }
    }


    while( ID_TRUE )
    {
        if ( sIsMyTrans == ID_TRUE )
        {
            /* cursor level visibility */
            if ( aLobInfo4Fetch == NULL )
            {
                /* normal fetch */
                if ( SM_SCN_IS_GT( aCSInfiniteSCN, &sRowInfSCN ) )
                {
                    if( ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                        ( SM_SCN_IS_GT( &sTrans->mCursorOpenInfSCN, &sRowInfSCN ) ) )
                    {
                        /* PROJ-2694 Fetch Across Rollback
                         * ���� Tx���� holdable cursor�� ������ �� ������ row��� 
                         * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����.*/
                        sTrans->mIsReusableRollback = ID_FALSE;
                    }
                    break; // It's a valid version
                }
            }
            else
            {
                /* lobdesc fetch */

                if ( aLobInfo4Fetch->mOpenMode == SMI_LOB_READ_WRITE_MODE )
                {
                    /* lobdesc fetch(READ_WRITE MODE) */

                    /* lob partial update�� ������ ��,
                     * �ڽ��� update�� version�� ��������
                     * cursor infinite scn�� row scn�� ���� ��쵵
                     * �� �� �־�� �Ѵ�.
                     * �׷��� lob cursor�� READ_WRITE MODE�� ���� ���,
                     * infinite scn ���Ҷ� GT�� �ƴ� GE�� ���Ѵ�. */
                    if ( SM_SCN_IS_GE( aCSInfiniteSCN, &sRowInfSCN ) )
                    {
                        if ( ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                             ( SM_SCN_IS_GE( &sTrans->mCursorOpenInfSCN, &sRowInfSCN ) ) )
                        {
                            /* PROJ-2694 Fetch Across Rollback
                             * ���� Tx���� holdable cursor�� ������ �� ������ row��� 
                             * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����.*/
                            sTrans->mIsReusableRollback = ID_FALSE;
                        }
                        break; // It's a valid version
                    }
                }
                else
                {
                    /* lobdesc fetch(READ MODE) */
                    if ( SM_SCN_IS_GT( aCSInfiniteSCN, &sRowInfSCN ) )
                    {
                        /* PROJ-2694 Fetch Across Rollback
                         * ���� Tx���� cursor�� ������ �� ������ row��� 
                         * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����.*/
                        sTrans->mIsReusableRollback = ID_FALSE;

                        break; // It's a valid version
                    }
                }
            }
        }
        else /* sIsMyTrans == ID_FALSE */
        {
            if ( SDC_CTS_SCN_IS_COMMITTED(sRowCSCN) == ID_TRUE )
            {
                if ( SM_SCN_IS_GE(aMyViewSCN, &sRowCSCN) )
                {
                    /* valid version */
                    break;
                }
            }
        }

        /* undo record�� ã�ƺ���. */
        while( ID_TRUE )
        {
            getRowUndoSID( sTargetRow, &sCurrUndoSID );

            /*  BUG-24216
             *  [SD-���ɰ���] makeOldVersion() �Ҷ�����
             *  rowpiece �����ϴ� ������ delay ��Ų��. */
            if ( sDoMakeOldVersion == ID_FALSE )
            {
                idlOS::memcpy( aOldVersionBuf,
                               aTargetRow,
                               getRowPieceSize(aTargetRow) );
                sTargetRow = aOldVersionBuf;
                sDoMakeOldVersion = ID_TRUE;
            }

            IDE_TEST( makeOldVersionWithFix( aStatistics,
                                             sCurrUndoSID,
                                             sTableOID,
                                             sTargetRow,
                                             &sMorePrvVersion )
                      != IDE_SUCCESS );

            if ( sMorePrvVersion == ID_TRUE )
            {
                SDC_GET_ROWHDR_1B_FIELD( sTargetRow,
                                         SDC_ROWHDR_CTSLOTIDX,
                                         sCTSlotIdx );
                SDC_GET_ROWHDR_FIELD( sTargetRow,
                                      SDC_ROWHDR_INFINITESCN,
                                      &sRowInfSCN );
                SDC_GET_ROWHDR_FIELD( sTargetRow,
                                      SDC_ROWHDR_FSCNORCSCN,
                                      &sRowCSCN );

                if ( sCTSlotIdx == SDP_CTS_IDX_UNLK )
                {
                    IDE_ERROR( SDC_CTS_SCN_IS_COMMITTED(sRowCSCN) == ID_TRUE );

                    if ( SDC_CTS_SCN_IS_LEGACY(sRowCSCN) == ID_TRUE )
                    {
                        sIsMyTrans = isMyLegacyTransUpdated(
                                                    aTrans,
                                                    *aCSInfiniteSCN,
                                                    sRowInfSCN,
                                                    sRowCSCN );
                    }
                    else
                    {
                        sIsMyTrans = ID_FALSE;
                    }

                    break;
                }
                else
                {
                    IDE_ERROR( SDC_CTS_SCN_IS_NOT_COMMITTED(sRowCSCN)
                               == ID_TRUE );

                    if ( sIsMyTrans == ID_FALSE )
                    {
                        continue;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                break; // No more previous version
            }
        } /* while(ID_TRUE) */

        if ( ( aFetchVersion == SMI_FETCH_VERSION_LASTPREV ) &&
             ( sIsMyTrans    == ID_FALSE ) )
        {
            /* ���� ������ undp record ������ Ȯ���Ѵ�
             * delete index key���� ��� */
            break;
        }

        if ( sMorePrvVersion == ID_FALSE )
        {
            break; // No more previous version
        }
    } /* while (ID_TRUE) */

    *aDoMakeOldVersion = sDoMakeOldVersion;
    *aUndoSID          = sCurrUndoSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * undo record�κ��� old version�� �����.
 **********************************************************************/
IDE_RC sdcRow::makeOldVersionWithFix( idvSQL         *aStatistics,
                                      sdSID           aUndoSID,
                                      smOID           aTableOID,
                                      UChar          *aDestSlotPtr,
                                      idBool         *aContinue )
{

    UChar         *sUndoRecHdr;
    idBool         sDummy;
    UInt           sState=0;

    IDE_ASSERT( aDestSlotPtr != NULL );

    if ( aUndoSID == SD_NULL_SID )
    {
        // Insert
        sUndoRecHdr = NULL;
        IDE_TEST( makeOldVersion( sUndoRecHdr,
                                  aTableOID,
                                  aDestSlotPtr,
                                  aContinue) != IDE_SUCCESS );
    }
    else
    {
        // Update/Delete
        IDE_TEST( sdbBufferMgr::getPageBySID(
                                  aStatistics,
                                  SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                  aUndoSID,
                                  SDB_S_LATCH,
                                  SDB_WAIT_NORMAL,
                                  NULL, /* aMtx */
                                  &sUndoRecHdr,
                                  &sDummy ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( makeOldVersion( sUndoRecHdr,
                                  aTableOID,
                                  aDestSlotPtr,
                                  aContinue ) != IDE_SUCCESS );


        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                           sUndoRecHdr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage(
                       aStatistics,
                       sUndoRecHdr ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 * undo record�κ��� old version�� �����.
 * dest buffer�� copy�Ѵ�.
 *
 * - insert �� ���ڵ��� ���
 *   undo segment���� insert�α� ���´�.
 *   ���� ���ڵ忡 delete bit�� set�ؼ� �����ؾ� �Ѵ�.
 * - delete, update �� ���ڵ��� ���
 *   ���� ���ڵ� header�� ���� record header�� �ٲپ�� �Ѵ�.
 **********************************************************************/
IDE_RC sdcRow::makeOldVersion( UChar         * aUndoRecHdr,
                               smOID           aTableOID,
                               UChar         * aDestSlotPtr,
                               idBool        * aContinue )
{
    sdcUndoRecType    sType;
    UChar           * sUndoRecBody;
    smOID             sTableOID;
    UChar             sFlag;
    smSCN             sInfiniteSCN;
    UShort            sChangeSize;

    IDE_ERROR( aDestSlotPtr != NULL );
    IDE_ERROR( aContinue    != NULL );

    if ( aUndoRecHdr == NULL )
    {
        // Insert�� ��츸 aUndoRecHdr �� NULL�� ����.
        SDC_GET_ROWHDR_FIELD( aDestSlotPtr,
                              SDC_ROWHDR_INFINITESCN,
                              &sInfiniteSCN );
        SM_SET_SCN_DELETE_BIT( &sInfiniteSCN );
        SDC_SET_ROWHDR_FIELD( aDestSlotPtr,
                              SDC_ROWHDR_INFINITESCN,
                              &sInfiniteSCN );
        *aContinue = ID_FALSE;
    }
    else
    {
        sUndoRecBody =
            sdcUndoRecord::getUndoRecBodyStartPtr((UChar*)aUndoRecHdr);

        SDC_GET_UNDOREC_HDR_1B_FIELD( aUndoRecHdr,
                                      SDC_UNDOREC_HDR_TYPE,
                                      &sType );

        sdcUndoRecord::getTableOID( aUndoRecHdr, &sTableOID );

        IDE_ERROR( aTableOID == sTableOID );

        switch( sType )
        {
        case SDC_UNDO_UPDATE_ROW_PIECE :

            ID_READ_1B_VALUE( sUndoRecBody, &sFlag );

            if ( (sFlag & SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_MASK) ==
                 SDC_UPDATE_LOG_FLAG_UPDATE_METHOD_INPLACE )
            {
                IDE_TEST( redo_undo_UPDATE_INPLACE_ROW_PIECE( NULL,    /* aMtx */
                                                              sUndoRecBody,
                                                              aDestSlotPtr,
                                                              SDC_MVCC_MAKE_VALROW )
                        != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( redo_undo_UPDATE_OUTPLACE_ROW_PIECE( NULL,        /* aMtx */
                                                               sUndoRecBody,
                                                               aDestSlotPtr,
                                                               SD_NULL_SID, /* aSlotSID */
                                                               SDC_MVCC_MAKE_VALROW,
                                                               aDestSlotPtr,
                                                               NULL,        /* aNewSlotPtr4Undo */
                                                               NULL )       /* aFSCreditSize */
                        != IDE_SUCCESS );
            }
            break;

        case SDC_UNDO_OVERWRITE_ROW_PIECE :
            IDE_TEST( redo_undo_OVERWRITE_ROW_PIECE( NULL,  /* aMtx */
                                                     sUndoRecBody,
                                                     aDestSlotPtr,
                                                     SD_NULL_SID, /* aSlotSID */
                                                     SDC_MVCC_MAKE_VALROW,
                                                     aDestSlotPtr,
                                                     NULL,  /* aNewSlotPtr4Undo */
                                                     NULL ) /* aFSCreditSize */
                    != IDE_SUCCESS );
            break;

        case SDC_UNDO_CHANGE_ROW_PIECE_LINK :
            IDE_TEST( undo_CHANGE_ROW_PIECE_LINK( NULL,
                                                  sUndoRecBody,
                                                  aDestSlotPtr,
                                                  SD_NULL_SID, /* aSlotSID */
                                                  SDC_MVCC_MAKE_VALROW,
                                                  aDestSlotPtr,
                                                  NULL,  /* aNewSlotPtr4Undo */
                                                  NULL ) /* aFSCreditSize */
                    != IDE_SUCCESS );
            break;

        case SDC_UNDO_DELETE_FIRST_COLUMN_PIECE :
            IDE_TEST( undo_DELETE_FIRST_COLUMN_PIECE( NULL,        /* aMtx */
                                                      sUndoRecBody,
                                                      aDestSlotPtr,
                                                      SD_NULL_SID, /* aSlotSID */
                                                      SDC_MVCC_MAKE_VALROW,
                                                      aDestSlotPtr,
                                                      NULL )       /* aNewSlotPtr4Undo */
                    != IDE_SUCCESS );
            break;

        case SDC_UNDO_DELETE_ROW_PIECE :
        case SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE:

            IDE_ERROR( isDeleted(aDestSlotPtr) == ID_TRUE );

            ID_READ_AND_MOVE_PTR( sUndoRecBody,
                                  &sChangeSize,
                                  ID_SIZEOF(SShort) );

            idlOS::memcpy( aDestSlotPtr,
                           sUndoRecBody,
                           sChangeSize + SDC_ROWHDR_SIZE );

            IDE_ERROR( isDeleted(aDestSlotPtr) == ID_FALSE );
            break;

        case SDC_UNDO_LOCK_ROW :
            IDE_TEST( redo_undo_LOCK_ROW( NULL,  /* aMtx */
                                          sUndoRecBody,
                                          aDestSlotPtr,
                                          SDC_MVCC_MAKE_VALROW )
                      != IDE_SUCCESS );
            break;

        default:
            IDE_ERROR(0);
            break;
        }

        *aContinue = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::free( idvSQL            *aStatistics,
                     sdrMtx            *aMtx,
                     void              *aTableHeader,
                     scGRID             aSlotGRID,
                     UChar             *aSlotPtr )
{
    sdrMtxStartInfo    sStartInfo;
    void              *sTrans;
    sdpPageListEntry  *sEntry;
    UShort             sColCount;
    UChar             *sSlotPtr;
    idBool             sTrySuccess;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID));

    sTrans = sdrMiniTrans::getTrans( aMtx );
    IDE_ASSERT( sTrans != NULL );

    SDC_GET_ROWHDR_FIELD( aSlotPtr, SDC_ROWHDR_COLCOUNT, &sColCount);

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    /* Free�� Row�� �ִ� Page�� ���ؼ� XLatch�� ��´�. */
    IDE_TEST( sdbBufferMgr::getPageByGRID( aStatistics,
                                           aSlotGRID,
                                           SDB_X_LATCH,
                                           SDB_WAIT_NORMAL,
                                           aMtx,
                                           (UChar**)&sSlotPtr,
                                           &sTrySuccess) != IDE_SUCCESS );

    sEntry = (sdpPageListEntry*)
        smcTable::getDiskPageListEntry(aTableHeader);

    IDE_TEST( sdpPageList::freeSlot( aStatistics,
                                     SC_MAKE_SPACE(aSlotGRID),
                                     sEntry,
                                     aSlotPtr,
                                     aSlotGRID,
                                     aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : lob  page��  lob�� write�Ѵ�.
 *               �� write�� SDR_SDP_LOB_WRITE_PIECE log�� write�Ѵ�.
 *
 * Implementation :
 *
 *   aStatistics  - [IN] ��� �ڷ�
 *   aTrans       - [IN] Transaction
 *   aTableHeader - [IN] Table Header
 *   aInsertInfo  - [IN] Insert Info
 *   aRowGRID     - [IN] Row�� GRID
 **********************************************************************/
IDE_RC sdcRow::writeAllLobCols( idvSQL                      * aStatistics,
                                void                        * aTrans,
                                void                        * aTableHeader,
                                const sdcRowPieceInsertInfo * aInsertInfo,
                                scGRID                        aRowGRID )
{
    const sdcColumnInfo4Insert  * sColumnInfo;
    const smiValue              * sValue;
    UShort                        sColSeq;
    UShort                        sColSeqInRowPiece;
    UShort                        sLobDescCnt      = 0;
    UShort                        sDoWriteLobCount = 0;
    smrContType                   sContType;
    smLobViewEnv                  sLobViewEnv;
    sdcLobColBuffer               sLobColBuf;
    sdcLobDesc                    sLobDesc;

    IDE_ASSERT( aTrans       != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aInsertInfo  != NULL );
    IDE_ASSERT( aInsertInfo->mLobDescCnt > 0 );
    IDE_ASSERT( aInsertInfo->mIsInsert4Upt == ID_FALSE );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aRowGRID) );

    for ( sColSeq           = aInsertInfo->mStartColSeq,
          sColSeqInRowPiece = 0;
          sColSeq          <= aInsertInfo->mEndColSeq;
          sColSeq++, sColSeqInRowPiece++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sColSeq;

        if ( SDC_IS_IN_MODE_COLUMN(sColumnInfo->mValueInfo) == ID_TRUE )
        {
            continue;
        }

        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );
        IDE_ASSERT( SDC_IS_NULL(&(sColumnInfo->mValueInfo.mValue)) == ID_FALSE );

        sLobDescCnt++;

        /* lob desc */
        (void)sdcLob::initLobDesc(&sLobDesc);

        /* lob col buffer */
        sLobColBuf.mBuffer     = (UChar*)&sLobDesc;
        sLobColBuf.mInOutMode  = SDC_COLUMN_OUT_MODE_LOB;
        sLobColBuf.mLength     = ID_SIZEOF(sdcLobDesc);
        sLobColBuf.mIsNullLob  = ID_FALSE;

        /* lob view env */
        sdcLob::initLobViewEnv(&sLobViewEnv);

        sLobViewEnv.mTable                  = aTableHeader;
        sLobViewEnv.mTID                    = smxTrans::getTransID(aTrans);
        sLobViewEnv.mOpenMode               = SMI_LOB_READ_WRITE_MODE;
        sLobViewEnv.mLobColBuf              = &sLobColBuf;
        sLobViewEnv.mColSeqInRowPiece       = sColSeqInRowPiece;
        sLobViewEnv.mLobCol                 = *(sColumnInfo->mColumn);
        
        SC_COPY_GRID( aRowGRID, sLobViewEnv.mGRID );

        /* value */
        sValue = &sColumnInfo->mValueInfo.mOutValue;
        
        if ( SDC_IS_FIRST_PIECE_IN_INSERTINFO(aInsertInfo)
            == ID_TRUE )
        {
            if ( sDoWriteLobCount == (aInsertInfo->mLobDescCnt - 1) )
            {
                sContType = SMR_CT_END;
            }
            else
            {
                sContType = SMR_CT_CONTINUE;
            }
        }
        else
        {
            sContType = SMR_CT_CONTINUE;
        }

        IDE_TEST( writeLobColUsingSQL(aStatistics,
                                      aTrans,
                                      &sLobViewEnv,
                                      0, /* aOffset */
                                      sValue->length,
                                      (UChar*)sValue->value,
                                      sContType)
                  != IDE_SUCCESS );

        sDoWriteLobCount++;

        if ( sLobDescCnt == aInsertInfo->mLobDescCnt )
        {
            // Lob Desc Cnt��ŭ �ݿ������� �������´�.
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Insert For Update���� lob�� update ��Ų��.
 *
 *   aStatistics    - [IN] ��� �ڷ�
 *   aTrans         - [IN] Transaction
 *   aTableHeader   - [IN] Table Header
 *   aInsertInfo    - [IN] Insert Info
 *   aRowGRID       - [IN] Row�� GRID
 **********************************************************************/
IDE_RC sdcRow::updateAllLobCols( idvSQL                      * aStatistics,
                                 sdrMtxStartInfo             * aStartInfo,
                                 void                        * aTableHeader,
                                 const sdcRowPieceInsertInfo * aInsertInfo,
                                 scGRID                        aRowGRID )
{
    const sdcColumnInfo4Insert  * sColumnInfo;
    const smiValue              * sValue;
    UShort                        sColSeq;
    UShort                        sLobDescCnt = 0;
    UShort                        sColSeqInRowPiece;
    smLobViewEnv                  sLobViewEnv;
    sdcLobColBuffer               sLobColBuf;
    sdcLobDesc                    sLobDesc;
    smrContType                   sContType = SMR_CT_CONTINUE;
    

    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aInsertInfo  != NULL );
    IDE_ASSERT( aInsertInfo->mLobDescCnt > 0 );
    IDE_ASSERT( aInsertInfo->mIsInsert4Upt == ID_TRUE );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aRowGRID) );

    for ( sColSeq           = aInsertInfo->mStartColSeq,
          sColSeqInRowPiece = 0;
          sColSeq          <= aInsertInfo->mEndColSeq;
          sColSeq++, sColSeqInRowPiece++ )
    {
        sColumnInfo = aInsertInfo->mColInfoList + sColSeq;

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mValueInfo ) == ID_TRUE )
        {
            continue;
        }

        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );

        sLobDescCnt++;

        if ( sColumnInfo->mIsUptCol == ID_FALSE )
        {
            continue;
        }

        /* lob desc */
        (void)sdcLob::initLobDesc(&sLobDesc);

        /* lob col buffer */
        sLobColBuf.mBuffer     = (UChar*)&sLobDesc;
        sLobColBuf.mInOutMode  = SDC_COLUMN_OUT_MODE_LOB;
        sLobColBuf.mLength     = ID_SIZEOF(sdcLobDesc);
        sLobColBuf.mIsNullLob  = ID_FALSE;

        /* lob view env */
        sdcLob::initLobViewEnv(&sLobViewEnv);

        sLobViewEnv.mTable                  = aTableHeader;
        sLobViewEnv.mTID                    = smxTrans::getTransID(aStartInfo->mTrans);
        sLobViewEnv.mOpenMode               = SMI_LOB_READ_WRITE_MODE;
        sLobViewEnv.mLobColBuf              = &sLobColBuf;
        sLobViewEnv.mColSeqInRowPiece       = sColSeqInRowPiece;
        sLobViewEnv.mLobCol                 = *(sColumnInfo->mColumn);
        
        SC_COPY_GRID( aRowGRID, sLobViewEnv.mGRID );

        /* value */
        sValue = &sColumnInfo->mValueInfo.mOutValue;
        
        IDE_TEST( writeLobColUsingSQL(aStatistics,
                                      aStartInfo->mTrans,
                                      &sLobViewEnv,
                                      0, /* aOffset */
                                      sValue->length,
                                      (UChar*)sValue->value,
                                      sContType)
                  != IDE_SUCCESS );
        
        if ( sLobDescCnt == aInsertInfo->mLobDescCnt )
        {
            // Lob Desc Cnt��ŭ �ݿ������� �������´�.
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Update Row Piece���� lob�� update ��Ų��.
 *
 *   aStatistics  - [IN] ��� �ڷ�
 *   aTrans       - [IN] Transaction
 *   aTableHeader - [IN] Table Header
 *   aUpdateInfo  - [IN] Update Info
 *   aColCount    - [IN] Column�� Count
 *   aRowGRID     - [IN] Row�� GRID
 **********************************************************************/
IDE_RC sdcRow::updateAllLobCols( idvSQL                      * aStatistics,
                                 sdrMtxStartInfo             * aStartInfo,
                                 void                        * aTableHeader,
                                 const sdcRowPieceUpdateInfo * aUpdateInfo,
                                 UShort                        aColCount,
                                 scGRID                        aRowGRID )
{
    const sdcColumnInfo4Update  * sColumnInfo;
    const smiValue              * sValue;
    UShort                        sColSeqInRowPiece;
    UShort                        sLobDescCnt = 0 ;
    smLobViewEnv                  sLobViewEnv;
    sdcLobColBuffer               sLobColBuf;
    sdcLobDesc                    sLobDesc;
    smrContType                   sContType = SMR_CT_CONTINUE;
    

    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aTableHeader != NULL );
    IDE_ASSERT( aUpdateInfo  != NULL );
    IDE_ASSERT( aUpdateInfo->mUptAftLobDescCnt > 0 );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aRowGRID) );

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeqInRowPiece;

        /* mColumn�� ���� ���� update �÷����� ���θ� �Ǵ��Ѵ�.
         * mColumn == NULL : update �÷� X
         * mColumn != NULL : update �÷� O */
        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_FALSE )
        {
            continue;
        }

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mNewValueInfo ) == ID_TRUE )
        {
            continue;
        }

        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );

        sLobDescCnt++;

        /* lob desc */
        (void)sdcLob::initLobDesc(&sLobDesc);

        /* lob col buffer */
        sLobColBuf.mBuffer     = (UChar*)&sLobDesc;
        sLobColBuf.mInOutMode  = SDC_COLUMN_OUT_MODE_LOB;
        sLobColBuf.mLength     = ID_SIZEOF(sdcLobDesc);
        sLobColBuf.mIsNullLob  = ID_FALSE;

        /* lob view env */
        sdcLob::initLobViewEnv(&sLobViewEnv);

        sLobViewEnv.mTable                  = aTableHeader;
        sLobViewEnv.mTID                    = smxTrans::getTransID(aStartInfo->mTrans);
        sLobViewEnv.mOpenMode               = SMI_LOB_READ_WRITE_MODE;
        sLobViewEnv.mLobColBuf              = &sLobColBuf;
        sLobViewEnv.mColSeqInRowPiece       = sColSeqInRowPiece;
        sLobViewEnv.mLobCol                 = *(sColumnInfo->mColumn);
        
        SC_COPY_GRID( aRowGRID, sLobViewEnv.mGRID );

        /* value */
        sValue = &sColumnInfo->mNewValueInfo.mOutValue;
        
        IDE_TEST( writeLobColUsingSQL(aStatistics,
                                      aStartInfo->mTrans,
                                      &sLobViewEnv,
                                      0, /* aOffset */
                                      sValue->length,
                                      (UChar*)sValue->value,
                                      sContType)
                  != IDE_SUCCESS );
        
        if ( sLobDescCnt == aUpdateInfo->mUptAftLobDescCnt )
        {
            // Lob Desc Cnt��ŭ �ݿ������� �������´�.
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Overwrite Row Piece���� lob�� update ��Ų��.
 *
 *   aStatistics     - [IN] ��� �ڷ�
 *   aTrans          - [IN] Transaction
 *   aTableHeader    - [IN] Table Header
 *   aOverwriteInfo  - [IN] Overwrite Info
 *   aColCount       - [IN] Column�� Count
 *   aRowGRID        - [IN] Row�� GRID
 **********************************************************************/
IDE_RC sdcRow::updateAllLobCols( idvSQL                         * aStatistics,
                                 sdrMtxStartInfo                * aStartInfo,
                                 void                           * aTableHeader,
                                 const sdcRowPieceOverwriteInfo * aOverwriteInfo,
                                 UShort                           aColCount,
                                 scGRID                           aRowGRID )
{
    const sdcColumnInfo4Update  * sColumnInfo;
    const smiValue              * sValue;
    UShort                        sColSeqInRowPiece;
    smLobViewEnv                  sLobViewEnv;
    sdcLobColBuffer               sLobColBuf;
    sdcLobDesc                    sLobDesc;
    smrContType                   sContType = SMR_CT_CONTINUE;

    IDE_ASSERT( aStartInfo      != NULL );
    IDE_ASSERT( aTableHeader    != NULL );
    IDE_ASSERT( aOverwriteInfo  != NULL );
    IDE_ASSERT( aOverwriteInfo->mUptAftLobDescCnt > 0 );
    IDE_ASSERT( SC_GRID_IS_NOT_NULL(aRowGRID) );

    for ( sColSeqInRowPiece = 0 ;
          sColSeqInRowPiece < aColCount;
          sColSeqInRowPiece++ )
    {
        sColumnInfo = aOverwriteInfo->mColInfoList + sColSeqInRowPiece;

        /* mColumn�� ���� ���� update �÷����� ���θ� �Ǵ��Ѵ�.
         * mColumn == NULL : update �÷� X
         * mColumn != NULL : update �÷� O */
        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) == ID_FALSE )
        {
            continue;
        }

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mNewValueInfo ) == ID_TRUE )
        {
            continue;
        }

        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );

        /* lob desc */
        (void)sdcLob::initLobDesc(&sLobDesc);

        /* lob col buffer */
        sLobColBuf.mBuffer     = (UChar*)&sLobDesc;
        sLobColBuf.mInOutMode  = SDC_COLUMN_OUT_MODE_LOB;
        sLobColBuf.mLength     = ID_SIZEOF(sdcLobDesc);
        sLobColBuf.mIsNullLob  = ID_FALSE;

        /* lob view env */
        sdcLob::initLobViewEnv(&sLobViewEnv);

        sLobViewEnv.mTable                  = aTableHeader;
        sLobViewEnv.mTID                    = smxTrans::getTransID(aStartInfo->mTrans);
        sLobViewEnv.mOpenMode               = SMI_LOB_READ_WRITE_MODE;
        sLobViewEnv.mLobColBuf              = &sLobColBuf;
        sLobViewEnv.mColSeqInRowPiece       = sColSeqInRowPiece;
        sLobViewEnv.mLobCol                 = *(sColumnInfo->mColumn);
        
        SC_COPY_GRID( aRowGRID, sLobViewEnv.mGRID );

        /* value */
        sValue = &sColumnInfo->mNewValueInfo.mOutValue;
        
        IDE_TEST( writeLobColUsingSQL(aStatistics,
                                      aStartInfo->mTrans,
                                      &sLobViewEnv,
                                      0, /* aOffset */
                                      sValue->length,
                                      (UChar*)sValue->value,
                                      sContType)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  lob data���� remove �Ѵ�.(lob page���� old lob page list�� �ű��.)
 *
 *  aStatistics    - [IN] �������
 *  aTrans         - [IN] Ʈ����� ����
 *  aLobInfo       - [IN] lob column ������ �����ϴ� �ڷᱸ��
 *
 **********************************************************************/
IDE_RC sdcRow::removeAllLobCols( idvSQL                         *aStatistics,
                                 void                           *aTrans,
                                 const sdcLobInfoInRowPiece     *aLobInfo )
{
    const sdcColumnInfo4Lob      *sColumnInfo;
    UShort                        sLobColumnSeq;

    IDE_ASSERT( aTrans   != NULL );
    IDE_ASSERT( aLobInfo != NULL );

    for ( sLobColumnSeq = 0;
          sLobColumnSeq < aLobInfo->mLobDescCnt;
          sLobColumnSeq++ )
    {
        sColumnInfo = aLobInfo->mColInfoList + sLobColumnSeq;
        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );

        IDE_TEST( sdcLob::removeLob( aStatistics,
                                     aTrans,
                                     sColumnInfo->mLobDesc,
                                     sColumnInfo->mColumn )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
  * Description :
 *  lob data���� remove �Ѵ�.(lob page���� old lob page list�� �ű��.)
 *
 *  aStatistics    - [IN] �������
 *  aTrans         - [IN] Ʈ����� ����
 *  aUpdateInfo    - [IN] Update Info
  **********************************************************************/
IDE_RC sdcRow::removeOldLobPage4Upt( idvSQL                       *aStatistics,
                                     void                         *aTrans,
                                     const sdcRowPieceUpdateInfo  *aUpdateInfo)
{
    const sdcColumnInfo4Update  *sColumnInfo;
    sdcLobDesc                   sLobDesc;
    UShort                       sColSeq;
    UShort                       sColCnt;
    UShort                       sLobDescCnt = 0;

    IDE_ASSERT( aTrans      != NULL );
    IDE_ASSERT( aUpdateInfo != NULL );

    sColCnt = aUpdateInfo->mOldRowHdrInfo->mColCount;

    for ( sColSeq = 0 ; sColSeq < sColCnt ; sColSeq++ )
    {
        sColumnInfo = aUpdateInfo->mColInfoList + sColSeq;

        if ( SDC_IS_IN_MODE_COLUMN( sColumnInfo->mOldValueInfo ) == ID_TRUE )
        {
            continue;
        }

        sLobDescCnt++;

        if ( SDC_IS_UPDATE_COLUMN( sColumnInfo->mColumn ) != ID_TRUE )
        {
            continue;
        }

        IDE_ASSERT( SDC_IS_LOB_COLUMN(sColumnInfo->mColumn) == ID_TRUE );

        idlOS::memcpy( &sLobDesc,
                       sColumnInfo->mOldValueInfo.mValue.value,
                       ID_SIZEOF(sdcLobDesc) );

        IDE_TEST( sdcLob::removeLob( aStatistics,
                                     aTrans,
                                     &sLobDesc,
                                     sColumnInfo->mColumn )
                  != IDE_SUCCESS );

        if ( sLobDescCnt == aUpdateInfo->mUptBfrLobDescCnt )
        {
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Lob Descriptor�� ���Ե� Page�� X Latch�� ���
 *               Lob Desc �� �о�´�. Latch�� �����Ѵ�.
 *
 *   aStatistics - [IN]  �������
 *   aLobViewEnv - [IN]  �ڽ��� ������ LOB������ ����
 *   aMtx        - [IN]  Mini Transaction
 *   aRow        - [IN]
 *   aLobDesc    - [OUT] ���� Lob Descriptor
 *   aLobColSeqInRowPiece - [OUT]  XXX
 *   aCTSlotIdx  - [OUT] CTS Slot Index
 **********************************************************************/
IDE_RC sdcRow::getLobDesc4Write( idvSQL             * aStatistics,
                                 const smLobViewEnv * aLobViewEnv,
                                 sdrMtx             * aMtx,
                                 UChar             ** aRow,
                                 UChar             ** aLobDesc,
                                 UShort             * aLobColSeqInRowPiece,
                                 UChar              * aCTSlotIdx )
{
    UChar          *sLobDesc;
    UChar          *sRow;
    scGRID          sRowPieceGRID;
    UShort          sColSeqInRowPiece;

    IDE_DASSERT( aLobViewEnv != NULL );
    IDE_DASSERT( aMtx        != NULL );
    IDE_DASSERT( aRow        != NULL );
    IDE_DASSERT( aLobDesc    != NULL );
    IDE_DASSERT( SC_GRID_IS_NOT_NULL(aLobViewEnv->mGRID) );

    SC_COPY_GRID(aLobViewEnv->mGRID, sRowPieceGRID);
    sColSeqInRowPiece = aLobViewEnv->mColSeqInRowPiece;

    IDE_TEST( sdcRow::prepareUpdatePageBySID(
                             aStatistics,
                             aMtx,
                             SC_MAKE_SPACE(sRowPieceGRID),
                             SD_MAKE_SID_FROM_GRID(sRowPieceGRID),
                             SDB_SINGLE_PAGE_READ,
                             &sRow,
                             aCTSlotIdx)
              != IDE_SUCCESS );

    (void)sdcRow::getLobDescInRowPiece(sRow,
                                       sColSeqInRowPiece,
                                       &sLobDesc);

    *aRow                 = (UChar*)sRow;
    *aLobDesc             = (UChar*)sLobDesc;
    *aLobColSeqInRowPiece = sColSeqInRowPiece;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
void sdcRow::getLobDescInRowPiece( UChar    * aRow,
                                   UShort     aColSeqInRowPiece,
                                   UChar   ** aLobDesc )
{
    UChar          * sColPiecePtr;
    UShort           sColCount;
    UShort           sColSeq;
    UInt             sColLen;
    sdcColInOutMode  sInOutMode;

    IDE_ASSERT( aRow     != NULL );
    IDE_ASSERT( aLobDesc != NULL );

    SDC_GET_ROWHDR_FIELD(aRow, SDC_ROWHDR_COLCOUNT, &sColCount);

    if ( sColCount <= aColSeqInRowPiece )
    {
        (void)ideLog::log( IDE_ERR_0,
                           "ColCount: %"ID_UINT32_FMT", "
                           "ColSeqInRowPiece: %"ID_UINT32_FMT"\n",
                           sColCount,
                           aColSeqInRowPiece );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aRow,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    sColPiecePtr = getDataArea(aRow);

    sColSeq = 0;
    while( sColSeq < aColSeqInRowPiece )
    {
        sColPiecePtr = getNxtColPiece(sColPiecePtr);
        sColSeq++;
    }

    sColPiecePtr = getColPiece( sColPiecePtr,
                                &sColLen,
                                NULL, /* aColVal */
                                &sInOutMode );

    IDE_ASSERT( sColLen    == ID_SIZEOF(sdcLobDesc) );
    IDE_ASSERT( sInOutMode == SDC_COLUMN_OUT_MODE_LOB );

    *aLobDesc = (UChar*)sColPiecePtr;
}

/***********************************************************************
 * Description : insert SQL �������� lob�� value�� insert�ϴ� �Լ��̴�.
 *  1. prepare4 append
 *  2. write.
 *
 *     aStatistics  -[IN] ����ڷ�
 *     aTrans       -[IN] Transaction Object
 *     aTableHeader -[IN] Table Header
 *     aRowGRID     -[IN] Row GRID
 *     aLobColInfo  -[IN] LOB Column Info
 *     aContType    -[IN] Log Continue Type
 **********************************************************************/
IDE_RC sdcRow::writeLobColUsingSQL( idvSQL          * aStatistics,
                                    void            * aTrans,
                                    smLobViewEnv    * aLobViewEnv,
                                    UInt              aOffset,
                                    UInt              aPieceLen,
                                    UChar           * aPieceVal,
                                    smrContType       aContType )
{
    /*
     * write
     */
    
    IDE_TEST( sdcLob::write(aStatistics,
                            aTrans,
                            aLobViewEnv,
                            0, /* aLobLocator */
                            aOffset,
                            aPieceLen,
                            aPieceVal,
                            ID_FALSE /* aFromAPI */,
                            aContType)
              != IDE_SUCCESS );

    /*
     * update LOBDesc
     */
    
    IDE_TEST( updateLobDesc(aStatistics,
                            aTrans,
                            aLobViewEnv,
                            aContType)
              != IDE_SUCCESS );
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdcRow::updateLobDesc( idvSQL        * aStatistics,
                              void          * aTrans,
                              smLobViewEnv  * aLobViewEnv,
                              smrContType     aContType )
{
    sdrMtx            sMtx;
    sdcLobColBuffer * sLobColBuf;
    UChar           * sLobDescPtr = NULL;
    UChar           * sRow = NULL;
    UShort            sLobColSeqInRowPiece;
    UInt              sState = 0;

    IDE_ERROR( aLobViewEnv != NULL );
    IDE_ERROR( aLobViewEnv->mLobColBuf != NULL );

    sLobColBuf = (sdcLobColBuffer*)aLobViewEnv->mLobColBuf;

    IDE_TEST( sdrMiniTrans::begin(aStatistics,
                                  &sMtx,
                                  aTrans,
                                  SDR_MTX_LOGGING,
                                  ID_FALSE, /*MtxUndoable(PROJ-2162)*/
                                  SM_DLOG_ATTR_DEFAULT)
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( getLobDesc4Write(aStatistics,
                               aLobViewEnv,
                               &sMtx,
                               &sRow,
                               &sLobDescPtr,
                               &sLobColSeqInRowPiece,
                               NULL) /* aCTSlotIdx */
              != IDE_SUCCESS );

    /*
     * write
     */
    
    IDE_ERROR( sLobColBuf->mBuffer    != NULL );
    IDE_ERROR( sLobColBuf->mLength    == ID_SIZEOF(sdcLobDesc) );
    IDE_ERROR( sLobColBuf->mInOutMode == SDC_COLUMN_OUT_MODE_LOB );
    IDE_ERROR( sLobColBuf->mIsNullLob == ID_FALSE );
    
    idlOS::memcpy( sLobDescPtr,
                   sLobColBuf->mBuffer,
                   sLobColBuf->mLength );

    /*
     * write log
     */

    IDE_TEST( sdrMiniTrans::writeLogRec(
                              &sMtx,
                              sRow,
                              NULL,
                              ID_SIZEOF(UShort) + /* sLobColSeqInRowPiece */
                              ID_SIZEOF(sdcLobDesc),
                              SDR_SDC_LOB_UPDATE_LOBDESC)
              != IDE_SUCCESS );
                                         
    IDE_TEST( sdrMiniTrans::write(
                  &sMtx,
                  &sLobColSeqInRowPiece,
                  ID_SIZEOF(UShort))
              != IDE_SUCCESS);

    IDE_TEST( sdrMiniTrans::write(
                  &sMtx,
                  sLobColBuf->mBuffer,
                  ID_SIZEOF(sdcLobDesc))
              != IDE_SUCCESS);

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx, aContType)
              != IDE_SUCCESS);
    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  lob cursor open�ÿ�, replication�� ����
 *  lob cursor open �α׿� pk ������ ���� ����Ѵ�.
 *  �̶� lob�� row�κ��� pk ������ ������ ����
 *  �� �Լ��� ȣ���Ѵ�.
 *
 **********************************************************************/
IDE_RC sdcRow::getPKInfo( idvSQL              *aStatistics,
                          void                *aTrans,
                          scSpaceID           aTableSpaceID,
                          void                *aTableHeader,
                          UChar               *aSlotPtr,
                          sdSID                aSlotSID,
                          sdcPKInfo           *aPKInfo )
{
    UChar   *sCurrSlotPtr;
    sdSID    sCurrRowPieceSID;
    sdSID    sNextRowPieceSID;
    UShort   sState = 0;

    IDE_ASSERT( aTrans        != NULL );
    IDE_ASSERT( aTableHeader  != NULL );
    IDE_ASSERT( aSlotPtr      != NULL );
    IDE_ASSERT( aPKInfo       != NULL );
    IDE_DASSERT( isHeadRowPiece(aSlotPtr) == ID_TRUE );
    IDE_DASSERT( smLayerCallback::needReplicate( (smcTableHeader*)aTableHeader,
                                                 aTrans) == ID_TRUE );

    makePKInfo( aTableHeader, aPKInfo );

    sNextRowPieceSID = SD_NULL_SID;
    sCurrSlotPtr     = aSlotPtr;

    IDE_TEST( getPKInfoInRowPiece( sCurrSlotPtr,
                                   aSlotSID,
                                   aPKInfo,
                                   &sNextRowPieceSID )
              != IDE_SUCCESS );

    sCurrRowPieceSID = sNextRowPieceSID;

    while( sCurrRowPieceSID != SD_NULL_SID )
    {
        if ( aPKInfo->mTotalPKColCount < aPKInfo->mCopyDonePKColCount )
        {
            ideLog::log( IDE_ERR_0,
                         "SpaceID: %"ID_UINT32_FMT", "
                         "SlotSID: %"ID_UINT64_FMT", "
                         "CurrRowPieceSID: %"ID_UINT64_FMT", "
                         "NextRowPieceSID: %"ID_UINT64_FMT", "
                         "TatalPKColCount: %"ID_UINT32_FMT", "
                         "CopyDonePKColCount: %"ID_UINT32_FMT"\n",
                         aTableSpaceID,
                         aSlotSID,
                         sCurrRowPieceSID,
                         sNextRowPieceSID,
                         aPKInfo->mTotalPKColCount,
                         aPKInfo->mCopyDonePKColCount );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aPKInfo,
                            ID_SIZEOF(sdcPKInfo),
                            "PKInfo Dump:" );

            (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                         sCurrSlotPtr,
                                         "Page Dump:" );

            IDE_ASSERT( 0 );
        }


        if ( aPKInfo->mTotalPKColCount ==
             aPKInfo->mCopyDonePKColCount )
        {
            break;
        }

        IDE_TEST( sdbBufferMgr::getPageBySID( aStatistics,
                                              aTableSpaceID,
                                              sCurrRowPieceSID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL, /* aMtx */
                                              (UChar**)&sCurrSlotPtr )
                  != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( getPKInfoInRowPiece( sCurrSlotPtr,
                                       sCurrRowPieceSID,
                                       aPKInfo,
                                       &sNextRowPieceSID )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sCurrSlotPtr )
                  != IDE_SUCCESS );

        sCurrRowPieceSID = sNextRowPieceSID;
    }

    if ( aPKInfo->mTotalPKColCount != aPKInfo->mCopyDonePKColCount )
    {
        ideLog::log( IDE_ERR_0,
                     "SpaceID: %"ID_UINT32_FMT", "
                     "SlotSID: %"ID_UINT64_FMT", "
                     "CurrRowPieceSID: %"ID_UINT64_FMT", "
                     "NextRowPieceSID: %"ID_UINT64_FMT", "
                     "TatalPKColCount: %"ID_UINT32_FMT", "
                     "CopyDonePKColCount: %"ID_UINT32_FMT"\n",
                     aTableSpaceID,
                     aSlotSID,
                     sCurrRowPieceSID,
                     sNextRowPieceSID,
                     aPKInfo->mTotalPKColCount,
                     aPKInfo->mCopyDonePKColCount );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aPKInfo,
                        ID_SIZEOF(sdcPKInfo),
                        "PKInfo Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     sCurrSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sCurrSlotPtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *
 **********************************************************************/
IDE_RC sdcRow::getPKInfoInRowPiece( UChar               *aSlotPtr,
                                    sdSID                aSlotSID,
                                    sdcPKInfo           *aPKInfo,
                                    sdSID               *aNextRowPieceSID )
{
    sdcRowHdrInfo           sRowHdrInfo;
    sdcRowPieceUpdateInfo   sUpdateInfo;
    sdSID                   sNextRowPieceSID   = SD_NULL_SID;
    UShort                  sDummyFstColumnSeq = 0;

    IDE_ASSERT( aSlotPtr         != NULL );
    IDE_ASSERT( aPKInfo          != NULL );
    IDE_ASSERT( aNextRowPieceSID != NULL );

    if ( aPKInfo->mTotalPKColCount == aPKInfo->mCopyDonePKColCount )
    {
        ideLog::log( IDE_ERR_0,
                     "SlotSID: %"ID_UINT64_FMT", "
                     "TatalPKColCount: %"ID_UINT32_FMT", "
                     "CopyDonePKColCount: %"ID_UINT32_FMT"\n",
                     aSlotSID,
                     aPKInfo->mTotalPKColCount,
                     aPKInfo->mCopyDonePKColCount );

        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)aPKInfo,
                        ID_SIZEOF(sdcPKInfo),
                        "PKInfo Dump:" );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }


    getRowHdrInfo( aSlotPtr, &sRowHdrInfo );

    if ( SM_SCN_IS_DELETED(sRowHdrInfo.mInfiniteSCN) == ID_TRUE )
    {
        ideLog::logMem( IDE_DUMP_0,
                        (UChar*)&sRowHdrInfo,
                        ID_SIZEOF(sdcRowHdrInfo),
                        "RowHdrInfo Dump: " );

        (void)sdpPhyPage::tracePage( IDE_ERR_0,
                                     aSlotPtr,
                                     "Page Dump:" );

        IDE_ASSERT( 0 );
    }

    sNextRowPieceSID = getNextRowPieceSID(aSlotPtr);

    IDE_TEST( makeUpdateInfo( aSlotPtr,
                              NULL, // aColList
                              NULL, // aValueList
                              aSlotSID,
                              sRowHdrInfo.mColCount,
                              sDummyFstColumnSeq,
                              NULL,  // aLobInfo4Update
                              &sUpdateInfo )
              != IDE_SUCCESS );

    copyPKInfo( aSlotPtr,
                &sUpdateInfo,
                sRowHdrInfo.mColCount,
                aPKInfo );

    *aNextRowPieceSID = sNextRowPieceSID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-4007 [SM]PBT�� ���� ��� �߰�
 * Page���� Row�� Dump�Ͽ� ��ȯ�Ѵ�.
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. */

IDE_RC sdcRow::dump( UChar *aPage ,
                     SChar *aOutBuf ,
                     UInt   aOutSize )
{
    UChar               * sSlotDirPtr;
    sdpSlotDirHdr       * sSlotDirHdr;
    sdpSlotEntry        * sSlotEntry;
    UShort                sSlotCount;
    UShort                sSlotNum;
    UShort                sOffset;
    sdcRowHdrInfo         sRowHdrInfo;
    sdSID                 sNextRowPieceSID;
    UChar               * sSlot;
    UShort                sColumnSeq;
    UShort                sColumnLen;
    UChar                 sTempColumnBuf[ SD_PAGE_SIZE ];
    SChar                 sDumpColumnBuf[ SD_PAGE_SIZE ];

    IDE_ASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( (UChar*)aPage );
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
    sSlotCount  = sSlotDirHdr->mSlotEntryCount;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Table Row Begin ----------\n" );

    for ( sSlotNum=0; sSlotNum < sSlotCount; sSlotNum++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sSlotNum);

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%3"ID_UINT32_FMT"[%04"ID_xINT32_FMT"]..............................\n",
                             sSlotNum,
                             *sSlotEntry );

        if ( SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) == ID_TRUE )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Unused slot\n");
            /* BUG-31534 [sm-util] dump utility and fixed table 
             *           do not consider unused slot. */
            continue;
        }

        sOffset = SDP_GET_OFFSET( sSlotEntry );
        if ( SD_PAGE_SIZE < sOffset )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Invalid slot offset\n" );
            continue;
        }

        sSlot = sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );
        sdcRow::getRowHdrInfo( sSlot, &sRowHdrInfo );
        sNextRowPieceSID = sdcRow::getNextRowPieceSID( sSlot );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "mCTSlotIdx          : %"ID_UINT32_FMT"\n"
                             "mInfiniteSCN        : %"ID_UINT64_FMT"\n"
                             "mUndoSID            : %"ID_UINT64_FMT"\n"
                             "mColCount           : %"ID_UINT32_FMT"\n"
                             "mRowFlag            : %"ID_UINT32_FMT"\n"
                             "mExInfo.mTSSPageID  : %"ID_UINT32_FMT"\n"
                             "mExInfo.mTSSlotNum  : %"ID_UINT32_FMT"\n"
                             "mExInfo.mFSCredit   : %"ID_UINT32_FMT"\n"
                             "mExInfo.mFSCNOrCSCN : %"ID_UINT64_FMT"\n"
                             "NextRowPieceSID     : %"ID_UINT64_FMT"\n",
                             sRowHdrInfo.mCTSlotIdx,
                             SM_SCN_TO_LONG( sRowHdrInfo.mInfiniteSCN ),
                             sRowHdrInfo.mUndoSID,
                             sRowHdrInfo.mColCount,
                             sRowHdrInfo.mRowFlag,
                             sRowHdrInfo.mExInfo.mTSSPageID,
                             sRowHdrInfo.mExInfo.mTSSlotNum,
                             sRowHdrInfo.mExInfo.mFSCredit,
                             SM_SCN_TO_LONG( sRowHdrInfo.mExInfo.mFSCNOrCSCN ),
                             sNextRowPieceSID );

        for ( sColumnSeq = 0;
              sColumnSeq < sRowHdrInfo.mColCount;
              sColumnSeq++ )
        {
            idlOS::memset( sTempColumnBuf,
                           0x00,
                           SD_PAGE_SIZE );

            sdcRow::getColumnPiece( sSlot,
                                    sColumnSeq,
                                    (UChar*)sTempColumnBuf,
                                    SD_PAGE_SIZE,
                                    &sColumnLen );

            ideLog::ideMemToHexStr( sTempColumnBuf, 
                                    ( sColumnLen < SD_PAGE_SIZE )
                                        ? sColumnLen : SD_PAGE_SIZE, 
                                    IDE_DUMP_FORMAT_NORMAL,
                                    sDumpColumnBuf,
                                    SD_PAGE_SIZE );

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "ColumnSeq : %"ID_UINT32_FMT" %s\n",
                                 sColumnSeq,
                                 sDumpColumnBuf );
        }
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- Table Row End   ----------\n" );

    return IDE_SUCCESS;
}
