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
 * $Id: $
 **********************************************************************/

#ifndef _O_SDT_SORT_MODULE_H_
#define _O_SDT_SORT_MODULE_H_ 1

#include <idu.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdtSortDef.h>
#include <sdnbDef.h>
#include <sdtDef.h>
#include <sdtTempRow.h>
#include <sdtWASortMap.h>

class sdtSortModule
{
public:
    static IDE_RC init( smiTempTableHeader * aHeader );
    static IDE_RC destroy( smiTempTableHeader * aHeader);

    /************************* Module Function ***************************/
    inline static IDE_RC insert(smiTempTableHeader  * aHeader,
                                smiValue            * aValue );
    static IDE_RC sort(smiTempTableHeader * aHeader);

private:
    static IDE_RC calcEstimatedStats( smiTempTableHeader * aHeader );

    /***************************************************************
     * Open cursor operations
     ***************************************************************/
    static IDE_RC openCursorInMemoryScan( smiTempTableHeader * aHeader,
                                          smiSortTempCursor  * aCursor );
    static IDE_RC openCursorMergeScan( smiTempTableHeader * aHeader,
                                       smiSortTempCursor  * aCursor );
    static IDE_RC openCursorIndexScan( smiTempTableHeader * aHeader,
                                       smiSortTempCursor  * aCursor );
    static IDE_RC openCursorScan( smiTempTableHeader * aHeader,
                                  smiSortTempCursor  * aCursor );
    static IDE_RC traverseInMemoryScan( smiTempTableHeader * aHeader,
                                        const smiCallBack  * aCallBack,
                                        idBool               aDirection,
                                        SInt               * aSeq );
    static IDE_RC traverseIndexScan( smiTempTableHeader * aHeader,
                                     smiSortTempCursor  * aCursor );
    /***************************************************************
     * fetch operations
     ***************************************************************/
    static IDE_RC fetchInMemoryScanForward( smiSortTempCursor* aCursor,
                                            UChar  ** aRow,
                                            scGRID  * aRowGRID );
    static IDE_RC fetchInMemoryScanBackward(smiSortTempCursor* aCursor,
                                            UChar  ** aRow,
                                            scGRID  * aRowGRID );
    static IDE_RC fetchMergeScan(smiSortTempCursor* aCursor,
                                 UChar  ** aRow,
                                 scGRID  * aRowGRID );
    static IDE_RC fetchIndexScanForward(smiSortTempCursor* aCursor,
                                        UChar  ** aRow,
                                        scGRID  * aRowGRID );
    static IDE_RC fetchIndexScanBackward(smiSortTempCursor* aCursor,
                                         UChar  ** aRow,
                                         scGRID  * aRowGRID );
    static IDE_RC fetchScan(smiSortTempCursor* aCursor,
                            UChar  ** aRow,
                            scGRID  * aRowGRID );
    /***************************************************************
     * store/restore cursor operations
     ***************************************************************/
    static IDE_RC storeCursorInMemoryScan(smiSortTempCursor* aCursor,
                                          smiTempPosition * aPosition ); 
    static IDE_RC restoreCursorInMemoryScan(smiSortTempCursor* aCursor,
                                            smiTempPosition * aPosition );
    static IDE_RC storeCursorMergeScan(smiSortTempCursor* aCursor,
                                       smiTempPosition * aPosition );
    static IDE_RC restoreCursorMergeScan(smiSortTempCursor* aCursor,
                                         smiTempPosition * aPosition );
    static IDE_RC storeCursorIndexScan(smiSortTempCursor* aCursor,
                                       smiTempPosition * aPosition );
    static IDE_RC restoreCursorIndexScan(smiSortTempCursor* aCursor,
                                         smiTempPosition * aPosition );
    static IDE_RC storeCursorScan(smiSortTempCursor* aCursor,
                                  smiTempPosition * aPosition );
    static IDE_RC restoreCursorScan(smiSortTempCursor* aCursor,
                                    smiTempPosition * aPosition );

    /***************************************************************
     * State transition operation
     ***************************************************************/
    static IDE_RC extractNSort(smiTempTableHeader * aHeader);
    static IDE_RC merge(smiTempTableHeader * aHeader);
    static IDE_RC makeIndex(smiTempTableHeader * aHeader);
    static IDE_RC makeLNodes(smiTempTableHeader * aHeade,
                             idBool             * aNeedINode );
    static IDE_RC makeINodes(smiTempTableHeader * aHeader,
                             scPageID           * aChildNPID,
                             idBool             * aNeedMoreINode );
    static IDE_RC inMemoryScan(smiTempTableHeader * aHeader);
    static IDE_RC mergeScan(smiTempTableHeader * aHeader);
    static IDE_RC indexScan(smiTempTableHeader * aHeader);
    static IDE_RC scan(smiTempTableHeader * aHeader);

    /***************************************************************
     * SortOperations
     ***************************************************************/
    static IDE_RC sortSortGroup(smiTempTableHeader * aHeader);
    static IDE_RC quickSort( smiTempTableHeader * aHeader,
                             UInt                 aLeftPos,
                             UInt                 aRightPos );
    static IDE_RC mergeSort( smiTempTableHeader * aHeader,
                             SInt                 aLeftBeginPos,
                             SInt                 aLeftEndPos,
                             SInt                 aRightBeginPos,
                             SInt                 aRightEndPos );
    static IDE_RC compareGRIDAndGRID(smiTempTableHeader * aHeader,
                                     sdtGroupID           aGrpID,
                                     scGRID               aSrcGRID,
                                     scGRID               aDstGRID,
                                     SInt               * aResult );
    static IDE_RC compare( smiTempTableHeader * aHeader,
                           sdtSortScanInfo    * aSrcTRPInfo,
                           sdtSortScanInfo    * aDstTRPInfo,
                           SInt               * aResult );


    /***************************************************************
     * Copy or Store row operations
     ***************************************************************/
    static IDE_RC storeSortedRun(smiTempTableHeader * aHeader);
    static IDE_RC copyRowByPtr( smiTempTableHeader * aHeader,
                                UChar              * aSrcPtr,
                                sdtCopyPurpose      aPurpose,
                                sdtTempPageType      aPageType,
                                scGRID               aChildGRID,
                                sdtSortInsertResult* aTRPInfo );
    static IDE_RC copyRowByGRID( smiTempTableHeader * aHeader,
                                 scGRID               aSrcGRID,
                                 sdtCopyPurpose      aPurpose,
                                 sdtTempPageType      aPageType,
                                 scGRID               aChildGRID,
                                 sdtSortInsertResult* aTRPInfo );
    static IDE_RC copyExtraRow( smiTempTableHeader    * aHeader,
                                sdtSortInsertInfo * aTRPInfo );

    /***************************************************************
     * Heap Operation for Merge
     ***************************************************************/
    inline static UInt calcMaxMergeRunCount(
        smiTempTableHeader * aHeader,
        UInt                 aRowPageCount );
    static IDE_RC heapInit(smiTempTableHeader * aHeader);
    static IDE_RC buildLooserTree(smiTempTableHeader * aHeader);
    static IDE_RC heapPop( smiTempTableHeader * aHeader );
    static IDE_RC findAndSetLoserSlot( smiTempTableHeader * aHeader,
                                       UInt                 aPos,
                                       UInt               * aChild );
    static IDE_RC readNextRowByRun( smiTempTableHeader   * aHeader,
                                    sdtTempMergeRunInfo * aRun );
    static IDE_RC readRunPage( smiTempTableHeader   * aHeader,
                               UInt                   aRunNo,
                               UInt                   aPageSeq,
                               scPageID             * aNextPID,
                               idBool                 aReadNextNPID );
    inline static void getGRIDFromRunInfo( smiTempTableHeader   * aHeader,
                                           sdtTempMergeRunInfo * aRunInfo,
                                           scGRID               * aGRID );
    inline static scPageID getWPIDFromRunInfo( smiTempTableHeader * aHeader,
                                               UInt                 aRunNo,
                                               UInt                 aPageSeq );
    static IDE_RC makeMergePosition( smiTempTableHeader  * aHeader,
                                     void               ** aMergePosition );
    static IDE_RC makeMergeRuns( smiTempTableHeader  * aHeader,
                                 void                * aMergePosition );

    static IDE_RC makeScanPosition( smiTempTableHeader  * aHeader,
                                    scPageID           ** aMergePosition );
};

/**************************************************************************
 * Description :
 * �� ��Ȳ�� �м��Ͽ� MergeRun �ִ� ������ ���Ѵ�.
 ***************************************************************************/
UInt sdtSortModule::calcMaxMergeRunCount(
    smiTempTableHeader * aHeader,
    UInt                 aRowPageCount )
{
    ULong                  sSortGroupSize;
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                   sMergeRunCount;

    /* �������� Slot �� ���� ������,
     * �ϳ��� ���� ZeroBase�� OneBase�� �����ϰ�
     * �ϳ��� �� ���� (�����صξ) Run�� �ϳ��� ��� �������ϱ� ���� */
    sSortGroupSize =
        sdtSortSegment::getAllocableWAGroupPageCount( sWASeg, SDT_WAGROUPID_SORT )
        * SD_PAGE_SIZE
        - ID_SIZEOF( sdtTempMergeRunInfo ) * 2;

    /* �ѹ��� Merge�� �� �ִ� Run�� ������ ����Ѵ�.
     *
     * ������ ��������.
     * MaxMergeRunCount = SortGroupSize / ( SlotSize * 3 + RunSize );
     * (���⼭ Run�� ������ Run�� ������ ���� �ִ� Page, 8192�̴�.)
     * �̴� Run �ϳ��� SlotSize*3 + RunSize ��ŭ�� �ʿ��ϴٴ� ���̴�.
     * RunSize�� �翬�� �׷������� Slot�� * 3��ŭ �����Ǵ� ������ ������ ����.
     *
     * Slot�� 2�� ���� ��ŭ Ŀ������ Ȯ���Ѵ�.
     * 1���� Run�� 1���� Slot��,
     * 2���� Run�� 3���� Slot��,
     * 4���� Run�� 7���� Slot�� �ʿ���Ѵ�.
     * 1,2,4,8, �̷������� Ŀ������ ������, �̻������δ� SlotCount*2-1 �̴�.
     *
     * ���� 64���� ���, 127���� �ʿ��ϴ�. 1+2+4+8+16+32+64 = 127�̱� �����̴�.
     * ������ �־��� ���, �� Slot�� 65���� ���,
     * 1+2+4+8+16+32+64+65 = 192���� �ʿ��ϴ�.
     * ���� 3�� ���ϸ� �־��� ��츦 ����� �� �ֱ⿡, *3 �Ѵ�. */

    /* �߰��� MaxRowPageCount �� �ϳ��� Row�� ����ϴ� �ִ� Page������ ���ϱ�2
     * �� �Ͽ� ����ϴµ�, �̴� Row�� �ΰ� �ø��� �����̴�.
     * �׷��� �ϳ��� Row�� Fetch������� ���ϰ� ���� Row�� �̸�
     * HeapPop�س���, Fetch����� Row�� �������� �ʴ´�. */
    sMergeRunCount = sSortGroupSize /
        ( ID_SIZEOF( sdtTempMergeRunInfo ) * 3 +
          aRowPageCount * 2 * SD_PAGE_SIZE );

    return sMergeRunCount;
}

/**************************************************************************
 * Description :
 *      RunInfo�� ��������, �� RunInfo�� ����Ű�� GRID�� ����.
 * <IN>
 * aHeader        - ��� Table
 * aRunInfo       - ������ �Ǵ� RunInfo
 * <OUT>
 * aGRID          - �ش� Run�� ����Ű�� ��ġ
 ***************************************************************************/
void sdtSortModule::getGRIDFromRunInfo( smiTempTableHeader   * aHeader,
                                        sdtTempMergeRunInfo * aRunInfo,
                                        scGRID               * aGRID )
{
    SC_MAKE_GRID( *aGRID,
                  SDT_SPACEID_WORKAREA,
                  getWPIDFromRunInfo( aHeader,
                                      aRunInfo->mRunNo,
                                      aRunInfo->mPIDSeq ),
                  aRunInfo->mSlotNo );
}

/**************************************************************************
 * Description :
 *      RunInfo�� ��������, �� ����� WPID�� ��
 *
 *
 * +-------------+---------------+---------------+---------------+
 * | WAMap(Heap) |     Run 2     |     Run 1     |     Run 0     |
 * |             +---+---+---+---+---+---+---+---+---+---+---+---+
 * |             | 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 |
 * |             |   |   |   |   |   |   |   |   |   |   |   |   |
 * +-------------+---+---+---+---+---+---+---+---+---+---+---+---+
 *                <------------->                                ^
 *                 MergeRunSize                             sLastWPID
 *
 *
 * <IN>
 * aHeader        - ��� Table
 * aRunInfo       - ������ �Ǵ� RunInfo
 * <OUT>
 * aGRID          - �ش� Run�� ����Ű�� ��ġ
 ***************************************************************************/
scPageID sdtSortModule::getWPIDFromRunInfo( smiTempTableHeader * aHeader,
                                            UInt                 aRunNo,
                                            UInt                 aPageSeq )
{
    scPageID sRetPID;
    scPageID sLastWPID;

    IDE_ASSERT( aHeader->mMergeRunSize > 0 );

    if ( aRunNo == SDT_TEMP_RUNINFO_NULL )
    {
        return SC_NULL_PID;
    }

    sLastWPID = sdtSortSegment::getLastWPageInWAGroup(
        (sdtSortSegHdr*)aHeader->mWASegment,
        SDT_WAGROUPID_SORT ) - 1;

    sRetPID = sLastWPID -
        ( ( aHeader->mMergeRunSize * aRunNo )
          + ( aPageSeq % aHeader->mMergeRunSize ) );

    IDE_ASSERT( sRetPID <= sLastWPID );
    IDE_ASSERT( sRetPID >=  sdtSortSegment::getFirstWPageInWAGroup(
                    (sdtSortSegHdr*)aHeader->mWASegment,
                    SDT_WAGROUPID_SORT ) );

    return sRetPID;
}


/**************************************************************************
 * Description :
 * �����͸� �����Ѵ�. InsertNSort, Insert ���¿���
 * �Ѵ�.
 *
 * <IN>
 * aTable     - ��� Table
 * aValue     - ������ Value
 * aHashValue - ������ HashValue (HashTemp�� ��ȿ )
 * <OUT>
 * aGRID      - ������ ��ġ
 * aResult    - ������ �����Ͽ��°�?(UniqueViolation Check�� )
 ***************************************************************************/
IDE_RC sdtSortModule::insert( smiTempTableHeader  * aHeader,
                              smiValue            * aValue )
{
    sdtSortSegHdr     * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortInsertResult sTRInsertResult;
    sdtSortInsertInfo   sScanInfo;
    UInt                sWAMapIdx;
    idBool              sResetSortGroup = ID_FALSE;

    IDE_ERROR( (aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT) ||
               (aHeader->mTTState == SMI_TTSTATE_SORT_INSERTONLY) );

    if( aHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aHeader->mStatistics )
                  != IDE_SUCCESS );
        aHeader->mCheckCnt = 0;
    }

    while( 1 )
    {
        sScanInfo.mTRPHeader.mTRFlag      = SDT_TRFLAG_HEAD;
        sScanInfo.mTRPHeader.mHitSequence = 0;
        sScanInfo.mTRPHeader.mValueLength = 0; /* append�ϸ鼭 ������ */
        SC_MAKE_NULL_GRID( sScanInfo.mTRPHeader.mNextGRID );
        SC_MAKE_NULL_GRID( sScanInfo.mTRPHeader.mChildGRID );
        sScanInfo.mColumnCount = aHeader->mColumnCount;
        sScanInfo.mColumns     = aHeader->mColumns;
        sScanInfo.mValueLength = aHeader->mRowSize;
        sScanInfo.mValueList   = aValue;

        IDE_TEST( sdtTempRow::append( sWASeg,
                                      aHeader->mSortGroupID,
                                      SDT_TEMP_PAGETYPE_INMEMORYGROUP,
                                      0, /* CuttingOffset */
                                      &sScanInfo,
                                      &sTRInsertResult )
                  != IDE_SUCCESS );

        /* WAMap�� ���� ���� */
        if ( sTRInsertResult.mComplete == ID_TRUE )
        {
            /* ù RowPiece���� ������ �����Ͽ��� */
            IDE_TEST( sdtWASortMap::expand(
                          &sWASeg->mSortMapHdr,
                          SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID ),
                          &sWAMapIdx )
                      != IDE_SUCCESS );

            if ( sWAMapIdx != SDT_WASLOT_UNUSED  )
            {
                /* SlotȮ�� ���� */
                IDE_ERROR( SC_GRID_IS_NOT_NULL(
                               sTRInsertResult.mHeadRowpieceGRID ) );
                IDE_TEST( sdtWASortMap::setvULong(
                              &sWASeg->mSortMapHdr,
                              sWAMapIdx,
                              (vULong*)&sTRInsertResult.mHeadRowpiecePtr )
                          != IDE_SUCCESS );
                break;
            }
        }

        /* Reset �ѹ� �ߴµ��� ���� �����ϴ°� ���� �� ���� */
        IDE_ERROR( sResetSortGroup == ID_FALSE );

        /* BUG-46438 �ѹ��� �� �ǵ� ���� ���� ������ ������ �۴ٸ�,
         * sort temp�� ��� �� �� ����.*/
        IDE_TEST_RAISE( sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) == 0 ,
                        error_invalid_sortareasize );

        /* ������������ Row�� KeySlot�� ���Կ� �����ϸ�
         * �ش� ���� �����ؼ� ���� */
        if ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT )
        {
            IDE_TEST( sortSortGroup( aHeader ) != IDE_SUCCESS );
        }
        IDE_TEST( storeSortedRun(aHeader) != IDE_SUCCESS );
        sResetSortGroup = ID_TRUE;
    }

    aHeader->mRowCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_sortareasize);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_SORTAREASIZE ) );
    }
    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aHeader );

    return IDE_FAILURE;
}

#endif /* _O_SDT_SORT_MODULE_H_ */
