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

#ifndef O_SDT_TEMP_ROW_H_
#define O_SDT_TEMP_ROW_H_ 1

#include <smDef.h>
#include <sdtDef.h>
#include <smiDef.h>
#include <smiTempTable.h>
#include <sdtTempPage.h>
#include <sdtSortSegment.h>

class sdtTempRow
{
public:

    inline static IDE_RC append( sdtSortSegHdr     * aWASegment,
                                 sdtGroupID          aWAGroupID,
                                 sdtTempPageType     aPageType,
                                 UInt                aCuttingOffset,
                                 sdtSortInsertInfo * aTRPInfo,
                                 sdtSortInsertResult * aTRInsertResult );

    static IDE_RC allocNewPage( sdtSortSegHdr    * aWASegment,
                                sdtGroupID         aWAGroupID,
                                sdtWCB           * aPrevWCBPtr,
                                sdtTempPageType    aPageType,
                                UInt             * aRowPageCount,
                                sdtWCB          ** aNewWCBPtr );

    static void   initTRInsertResult( sdtSortInsertResult * aTRInsertResult );

    inline static IDE_RC appendRowPiece(sdtSortSegHdr     * aWASegment,
                                        sdtWCB            * aWCBPtr,
                                        UInt                aCuttingOffset,
                                        sdtSortInsertInfo * aTRPInfo,
                                        sdtSortInsertResult * aTRInsertResult );

    inline static IDE_RC fetch( sdtSortSegHdr   * aWASegment,
                                sdtGroupID        aGroupID,
                                UChar           * aRowBuffer,
                                sdtSortScanInfo * aTRPInfo );

    static IDE_RC fetchChainedRowPiece( sdtSortSegHdr   * aWASegment,
                                        sdtGroupID        aGroupID,
                                        UChar           * aRowBuffer,
                                        sdtSortScanInfo * aTRPInfo );

    static IDE_RC fetchByGRID( sdtSortSegHdr   * aWASegment,
                               sdtGroupID        aGroupID,
                               scGRID            aGRID,
                               UInt              aValueLength,
                               UChar           * aRowBuffer,
                               sdtSortScanInfo * aTRPInfo );

    inline static IDE_RC filteringAndFetchByGRID( smiSortTempCursor* aTempCursor,
                                                  scGRID          aTargetGRID,
                                                  UChar        ** aRow,
                                                  scGRID        * aRowGRID,
                                                  idBool        * aResult);

    inline static IDE_RC filteringAndFetch( smiSortTempCursor * aTempCursor,
                                            sdtSortTRPHdr  * aTRPHeader,
                                            UChar         ** aRow,
                                            scGRID         * aRowGRID,
                                            idBool         * aResult);

private:

    static IDE_RC updateChainedRowPiece( smiSortTempCursor* aTempCursor,
                                         smiValue         * aValueList );
public:
    inline static IDE_RC update( smiSortTempCursor* aTempCursor,
                                 smiValue         * aValueList );
    inline static void setHitFlag( smiSortTempCursor* aTempCursor,
                                   UInt               aHitSeq );
    inline static idBool isHitFlagged( smiSortTempCursor* aTempCursor,
                                       ULong           aHitSeq );

public:
    /* Dump�Լ��� */
    static void dumpTempTRPHeader( void       * aTRPHeader,
                                   SChar      * aOutBuf,
                                   UInt         aOutSize );
    static void dumpTempTRPInfo4Insert( void       * aTRPInfo,
                                        SChar      * aOutBuf,
                                        UInt         aOutSize );
    static void dumpTempPageByRow( void  * aPagePtr,
                                   SChar * aOutBuf,
                                   UInt    aOutSize );
    static void dumpRowWithCursor( void   * aTempCursor,
                                   SChar  * aOutBuf,
                                   UInt     aOutSize );
};

/**************************************************************************
 * Description :
 * �ش� WAPage�� Row�� �����Ѵ�.
 * ���� ������ ������ Row�� ���Ҵٸ� ���ʺ��� Row�� �����ϰ� ���� RowValue����
 * ���� ������ ��ġ�� RID�� ����ü�� ���� ��ȯ�Ѵ� . �̶� rowPiece ����ü��
 * rowValue�� smiValue���·� ������ �ִµ�, �� value�� ���� smiValue��
 * pointing�ϴ� �����̴�. ���� �ش� Slot�� �̹� Value�� �ִٸ�, ���� Row��
 * �Ǵ��ϰ� �׳� �о�ִ´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aPageType      - ���� Page�� Type
 * aCuttingOffset - �� �� ������ Row�鸸 �����Ѵ�. ������ ������ �����.
 *                  sdtSortModule::copyExtraRow ����
 * aTRPInfo       - ������ Row
 * <OUT>
 * aTRInsertResult- ������ ���
 ***************************************************************************/
IDE_RC sdtTempRow::append( sdtSortSegHdr     * aWASegment,
                           sdtGroupID          aWAGroupID,
                           sdtTempPageType     aPageType,
                           UInt                aCuttingOffset,
                           sdtSortInsertInfo * aTRPInfo,
                           sdtSortInsertResult * aTRInsertResult )
{
    UInt            sRowPageCount = 1;
    sdtWCB        * sWCBPtr;
    sdtSortGroup  * sGroupInfo = sdtSortSegment::getWAGroupInfo( aWASegment,
                                                                 aWAGroupID );

    IDE_DASSERT( SDT_TR_HEADER_SIZE_BASE ==
                 ( IDU_FT_SIZEOF( sdtSortTRPHdr, mTRFlag )
                   + IDU_FT_SIZEOF( sdtSortTRPHdr, mDummy )
                   + IDU_FT_SIZEOF( sdtSortTRPHdr, mHitSequence )
                   + IDU_FT_SIZEOF( sdtSortTRPHdr, mValueLength ) ) );

    IDE_DASSERT( SDT_TR_HEADER_SIZE_FULL ==
                 ( SDT_TR_HEADER_SIZE_BASE +
                   + IDU_FT_SIZEOF( sdtSortTRPHdr, mNextGRID )
                   + IDU_FT_SIZEOF( sdtSortTRPHdr, mChildGRID )) );

    idlOS::memset( aTRInsertResult, 0, ID_SIZEOF( sdtSortInsertResult ) );

    sWCBPtr = sdtSortSegment::getHintWCB( sGroupInfo );

    if ( sWCBPtr == NULL )
    {
        /* ���ʻ����� ��� */
        IDE_TEST( allocNewPage( aWASegment,
                                aWAGroupID,
                                sWCBPtr,
                                aPageType,
                                &sRowPageCount,
                                &sWCBPtr )
                  != IDE_SUCCESS );
    }

    while( sWCBPtr != NULL )
    {
        IDE_TEST( appendRowPiece( aWASegment,
                                  sWCBPtr,
                                  aCuttingOffset,
                                  aTRPInfo,
                                  aTRInsertResult )
                  != IDE_SUCCESS );

        IDE_DASSERT(( sGroupInfo->mPolicy != SDT_WA_REUSE_INMEMORY ) ||
                    (( sWCBPtr->mNPageID == SM_NULL_PID ) &&
                     ( aTRInsertResult->mHeadRowpieceGRID.mSpaceID != aWASegment->mSpaceID )));

        if ( aTRInsertResult->mComplete == ID_TRUE )
        {
            break;
        }

        IDE_TEST( allocNewPage( aWASegment,
                                aWAGroupID,
                                sWCBPtr,
                                aPageType,
                                &sRowPageCount,
                                &sWCBPtr )
                  != IDE_SUCCESS );
    }

    sdtSortSegment::setHintWCB( sGroupInfo, sWCBPtr );

    aTRInsertResult->mRowPageCount = sRowPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    dumpTempTRPInfo4Insert,
                                    aTRPInfo );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * RowPiece�ϳ��� �� �������� �����Ѵ�.
 * Append���� ����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ������ ��� WPID
 * aWPagePtr      - ������ ��� Page�� Pointer ��ġ
 * aSlotNo        - ������ ��� Slot
 * aCuttingOffset - �� �� ������ Row�鸸 �����Ѵ�. ������ ������ �����.
 *                  sdtSortModule::copyExtraRow ����
 * aTRPInfo       - ������ Row
 * <OUT>
 * aTRInsertResult- ������ ���
 ***************************************************************************/
IDE_RC sdtTempRow::appendRowPiece( sdtSortSegHdr     * aWASegment,
                                   sdtWCB            * aWCBPtr,
                                   UInt                aCuttingOffset,
                                   sdtSortInsertInfo * aTRPInfo,
                                   sdtSortInsertResult * aTRInsertResult )
{
    UInt           sRowPieceSize;
    UInt           sRowPieceHeaderSize;
    UInt           sMinSize;
    UInt           sSlotSize;
    UChar        * sSlotPtr;
    UChar        * sRowPtr;
    UChar        * sWAPagePtr;
    UInt           sBeginOffset;
    UInt           sEndOffset;
    sdtSortTRPHdr* sTRPHeader;
    scPageID       sWPID;
    scSlotNum      sSlotNo;

    IDE_ERROR( aWCBPtr != NULL );

    sWAPagePtr = aWCBPtr->mWAPagePtr;
    sSlotNo = sdtTempPage::allocSlotDir( sWAPagePtr );

    /* Slot�Ҵ��� ������ ��� */
    IDE_TEST_CONT( sSlotNo == SDT_TEMP_SLOT_UNUSED, SKIP );

    sWPID = sdtSortSegment::getWPageID(aWASegment, aWCBPtr);
    /* FreeSpace��� */
    sRowPieceHeaderSize = SDT_TR_HEADER_SIZE( aTRPInfo->mTRPHeader.mTRFlag );
    sRowPieceSize       = sRowPieceHeaderSize + aTRPInfo->mValueLength;

    IDE_ERROR_MSG( aCuttingOffset < sRowPieceSize,
                   "CuttingOffset : %"ID_UINT32_FMT"\n"
                   "RowPieceSize  : %"ID_UINT32_FMT"\n",
                   aCuttingOffset,
                   sRowPieceHeaderSize );

    if ( SM_IS_FLAG_ON( aTRPInfo->mTRPHeader.mTRFlag, SDT_TRFLAG_UNSPLIT ) )
    {
        sMinSize = sRowPieceSize - aCuttingOffset;
    }
    else
    {
        sMinSize = IDL_MIN( smuProperty::getTempRowSplitThreshold(),
                            sRowPieceSize - aCuttingOffset);
    }

    /* Slot�Ҵ��� �õ��� */
    sSlotPtr = sdtTempPage::allocSlot( (sdtSortPageHdr*)sWAPagePtr,
                                       sSlotNo,
                                       sRowPieceSize - aCuttingOffset,
                                       sMinSize,
                                       &sSlotSize );
    /* �Ҵ� ���� */
    IDE_TEST_CONT( sSlotSize == 0, SKIP );

    /* �Ҵ��� Slot ������ */
    aTRInsertResult->mHeadRowpiecePtr = (sdtSortTRPHdr*)sSlotPtr;

    if ( sSlotSize == sRowPieceSize )
    {
        /* Cutting�� �͵� ����, �׳� ���� Copy�ϸ� �� */
        aTRInsertResult->mComplete = ID_TRUE;

        /*Header ��� */
        sTRPHeader = (sdtSortTRPHdr*)sSlotPtr;
        idlOS::memcpy( sTRPHeader,
                       &aTRPInfo->mTRPHeader,
                       sRowPieceHeaderSize );
        sTRPHeader->mValueLength = aTRPInfo->mValueLength;

        /* ���� Row�� ���� ��ġ */
        sRowPtr = ((UChar*)aTRPInfo->mValueList[ 0 ].value)
            - aTRPInfo->mColumns[ 0 ].mColumn.offset;

        idlOS::memcpy( sSlotPtr + sRowPieceHeaderSize,
                       sRowPtr,
                       aTRPInfo->mValueLength );

        /* ���� �Ϸ��� */
        aTRPInfo->mValueLength = 0;

        SC_MAKE_GRID( aTRInsertResult->mHeadRowpieceGRID,
                      SDT_SPACEID_WORKAREA,
                      sWPID,
                      sSlotNo );

        sdtSortSegment::convertFromWGRIDToNGRID(
            aWASegment,
            aTRInsertResult->mHeadRowpieceGRID,
            &aTRInsertResult->mHeadRowpieceGRID );

        SC_MAKE_NULL_GRID( aTRPInfo->mTRPHeader.mNextGRID );
    }
    else
    {
        /* Row�� �ɰ��� �� ���� */
        /*********************** Range ��� ****************************/
        /* ����ϴ� OffsetRange�� ����� */
        sBeginOffset = sRowPieceSize - sSlotSize; /* �䱸���� ������ ��ŭ */
        sEndOffset   = aTRPInfo->mValueLength;
        IDE_ERROR( sEndOffset > sBeginOffset );

        /*********************** Header ��� **************************/
        sTRPHeader = (sdtSortTRPHdr*)sSlotPtr;
        aTRInsertResult->mHeadRowpiecePtr = sTRPHeader;

        idlOS::memcpy( sTRPHeader,
                       &aTRPInfo->mTRPHeader,
                       sRowPieceHeaderSize );
        sTRPHeader->mValueLength = sEndOffset - sBeginOffset;

        if ( sBeginOffset == aCuttingOffset )
        {
            /* ��û�� Write �Ϸ� */
            aTRInsertResult->mComplete = ID_TRUE;
        }
        else
        {
            aTRInsertResult->mComplete = ID_FALSE;
        }

        if ( sBeginOffset > 0 )
        {
            /* ���� ���� �κ��� ���ƴ� ������,
             * ������ TRPHeader���� NextGRID�� ���̰� (������ �����϶��)
             * ����� TRPHeader���� Head�� ����. ( Head�� �ƴϴϱ�. ) */
            SM_SET_FLAG_ON( aTRPInfo->mTRPHeader.mTRFlag,
                            SDT_TRFLAG_NEXTGRID );
            SM_SET_FLAG_OFF( sTRPHeader->mTRFlag,
                             SDT_TRFLAG_HEAD );
        }

        sSlotPtr += sRowPieceHeaderSize;

        /* ���� Row�� ���� ��ġ */
        sRowPtr = ((UChar*)aTRPInfo->mValueList[ 0 ].value)
            - aTRPInfo->mColumns[ 0 ].mColumn.offset;
        idlOS::memcpy( sSlotPtr,
                       sRowPtr + sBeginOffset,
                       sEndOffset - sBeginOffset );

        aTRPInfo->mValueLength -= sTRPHeader->mValueLength;

        SC_MAKE_GRID( aTRInsertResult->mHeadRowpieceGRID,
                      SDT_SPACEID_WORKAREA,
                      sWPID,
                      sSlotNo );

        sdtSortSegment::convertFromWGRIDToNGRID(
            aWASegment,
            aTRInsertResult->mHeadRowpieceGRID,
            &aTRInsertResult->mHeadRowpieceGRID );

        if ( SM_IS_FLAG_ON( aTRPInfo->mTRPHeader.mTRFlag, SDT_TRFLAG_NEXTGRID ) )
        {
            SC_COPY_GRID( aTRInsertResult->mHeadRowpieceGRID,
                          aTRPInfo->mTRPHeader.mNextGRID );
        }
        else
        {
            SC_MAKE_NULL_GRID( aTRPInfo->mTRPHeader.mNextGRID );
        }
    }

    if( aWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT )
    {
        aWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
    }
    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Row�� Ư�� Column�� �����Ѵ�.
 * Update�� Row�� ó�� �����Ҷ����� ����� ������ Ȯ���߱� ������ �ݵ��
 * Row�� ���� ���� ���� �����Ѵ�
 *
 * <IN>
 * aTempCursor    - ��� Ŀ��
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtTempRow::update( smiSortTempCursor * aTempCursor,
                           smiValue          * aValueList )
{
    const smiColumnList * sUpdateColumn;
    const smiColumn * sColumn;
    UChar           * sRowPos      = NULL;
    sdtSortTRPHdr   * sTRPHeader   = NULL;
    sdtWCB          * sWCBPtr;

    if( aTempCursor->mTTHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aTempCursor->mTTHeader->mStatistics ) != IDE_SUCCESS);
        aTempCursor->mTTHeader->mCheckCnt = 0;
    }

    sTRPHeader = (sdtSortTRPHdr*)aTempCursor->mRowPtr;

    IDE_ERROR( sTRPHeader != NULL );
    IDE_ERROR( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

    if ( sTRPHeader->mValueLength >= aTempCursor->mUpdateEndOffset )
    {
        sRowPos = aTempCursor->mRowPtr + SDT_TR_HEADER_SIZE( sTRPHeader->mTRFlag ) ;
        sUpdateColumn = aTempCursor->mUpdateColumns;

        while( sUpdateColumn != NULL )
        {
            sColumn = ( smiColumn*)sUpdateColumn->column;

            IDE_ERROR( (sColumn->offset + aValueList->length )
                       <= sTRPHeader->mValueLength ); // �Ѿ�� �߸��� ���̴�.

            idlOS::memcpy( sRowPos + sColumn->offset,
                           ((UChar*)aValueList->value),
                           aValueList->length );

            aValueList++;
            sUpdateColumn = sUpdateColumn->next;
        }

        if ( ( SC_GRID_IS_NULL( aTempCursor->mGRID ) ) ||
             ( SC_MAKE_SPACE( aTempCursor->mGRID ) == SDT_SPACEID_WORKAREA ) ||
             ( SC_MAKE_SPACE( aTempCursor->mGRID ) == SDT_SPACEID_WAMAP ) )
        {
            // InMemory �����̱⿡ SetDirty�� �ʿ� ����
            IDE_DASSERT((((smiTempTableHeader*)aTempCursor->mTTHeader)->mTTState != SMI_TTSTATE_SORT_MERGE ) &&
                        (((smiTempTableHeader*)aTempCursor->mTTHeader)->mTTState != SMI_TTSTATE_SORT_MERGESCAN ));
        }
        else
        {
            sWCBPtr = sdtSortSegment::findWCB( (sdtSortSegHdr*)aTempCursor->mWASegment,
                                               SC_MAKE_PID( aTempCursor->mGRID ));
            IDE_ERROR( sWCBPtr != NULL );
            IDE_ASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

            sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
        }
    }
    else
    {
        IDE_TEST( updateChainedRowPiece( aTempCursor,
                                         aValueList ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    dumpRowWithCursor,
                                    aTempCursor );

    smiTempTable::checkAndDump( aTempCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * page�� �����ϴ� Row ��ü�� rowInfo���·� �д´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aRowPtr        - Row�� ��ġ Pointer
 * aValueLength   - Row�� Value�κ��� �� ����
 * aRowBuffer     - �ɰ��� Column�� �����ϱ� ���� Buffer
 * <OUT>
 * aTRPInfo       - Fetch�� ���
 ***************************************************************************/
IDE_RC sdtTempRow::fetch( sdtSortSegHdr        * aWASegment,
                          sdtGroupID             aGroupID,
                          UChar                * aRowBuffer,
                          sdtSortScanInfo      * aTRPInfo )
{
    UChar sFlag = aTRPInfo->mTRPHeader->mTRFlag;

    IDE_DASSERT( SM_IS_FLAG_ON( sFlag, SDT_TRFLAG_HEAD ) );

    if ( SM_IS_FLAG_ON( sFlag, SDT_TRFLAG_NEXTGRID ) )
    {
        IDE_TEST( fetchChainedRowPiece( aWASegment,
                                        aGroupID,
                                        aRowBuffer,
                                        aTRPInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        aTRPInfo->mValuePtr    = ((UChar*)aTRPInfo->mTRPHeader) + SDT_TR_HEADER_SIZE( sFlag );
        aTRPInfo->mValueLength = aTRPInfo->mTRPHeader->mValueLength;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Filtering��ġ�� fetch��.
 *
 * <IN>
 * aTempCursor    - ��� Ŀ��
 * aTargetGRID    - ��� Row�� GRID
 * <OUT>
 * aRow           - ��� Row�� ���� Bufer
 * aRowGRID       - ��� Row�� ���� GRID
 * aResult        - Filtering ��� ����
 ***************************************************************************/
IDE_RC sdtTempRow::filteringAndFetchByGRID( smiSortTempCursor * aTempCursor,
                                            scGRID           aTargetGRID,
                                            UChar         ** aRow,
                                            scGRID         * aRowGRID,
                                            idBool         * aResult)
{
    smiTempTableHeader * sHeader = aTempCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)sHeader->mWASegment;
    sdtSortTRPHdr      * sTRPHeader;

    /* ���� Fetch�ϱ� ���� ���� PieceHeader�� ���� First���� Ȯ�� */
    IDE_TEST( sdtSortSegment::getPagePtrByGRID( sWASeg,
                                                aTempCursor->mWAGroupID,
                                                aTargetGRID,
                                                (UChar**)&sTRPHeader )
              != IDE_SUCCESS );

    IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

    IDE_TEST( filteringAndFetch( aTempCursor,
                                 sTRPHeader,
                                 aRow,
                                 aRowGRID,
                                 aResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Filtering��ġ�� fetch��.
 *
 * <IN>
 * aTempCursor    - ��� Ŀ��
 * aTargetPtr     - ��� Row�� Ptr
 * <OUT>
 * aRow           - ��� Row�� ���� Bufer
 * aRowGRID       - ��� Row�� ���� GRID
 * aResult        - Filtering ��� ����
 ***************************************************************************/
IDE_RC sdtTempRow::filteringAndFetch( smiSortTempCursor * aTempCursor,
                                      sdtSortTRPHdr  * aTRPHeader,
                                      UChar         ** aRow,
                                      scGRID         * aRowGRID,
                                      idBool         * aResult)
{
    smiTempTableHeader * sHeader;
    sdtSortSegHdr      * sWASeg;
    sdtSortScanInfo      sTRPInfo;
    UInt                 sFlag;
    idBool               sHit;
    idBool               sNeedHit;

    *aResult = ID_FALSE;

    sHeader = aTempCursor->mTTHeader;
    sWASeg  = (sdtSortSegHdr*)aTempCursor->mWASegment;

    IDE_DASSERT( SM_IS_FLAG_ON( aTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

    sFlag = aTempCursor->mTCFlag;

    /* HItFlagüũ�� �ʿ��Ѱ�? */
    if ( sFlag & SMI_TCFLAG_HIT_MASK )
    {
        /* Hit������ ũ�� �� Flag�̴�.
         * IgnoreHit -> Hit��ü�� ��� ����
         * Hit       -> Hit�� �͸� ������
         * NoHit     -> Hit �ȵ� �͸� ������
         *
         * �� if���� IgnoreHit�� �ƴ����� �����ϴ� ���̰�.
         * �Ʒ� IF���� Hit���� NoHit���� �����ϴ� ���̴�. */
        sNeedHit = ( sFlag & SMI_TCFLAG_HIT ) ? ID_TRUE : ID_FALSE;
        sHit = ( aTRPHeader->mHitSequence == sHeader->mHitSequence ) ? ID_TRUE : ID_FALSE;

        IDE_TEST_CONT( sHit != sNeedHit, SKIP );
    }
    else
    {
        /* nothing to do */
    }

    sTRPInfo.mTRPHeader      = aTRPHeader;
    sTRPInfo.mFetchEndOffset = sHeader->mRowSize;

    IDE_TEST( fetch( sWASeg,
                     aTempCursor->mWAGroupID,
                     sHeader->mRowBuffer4Fetch,
                     &sTRPInfo )
              != IDE_SUCCESS );
    IDE_DASSERT( sTRPInfo.mValueLength <= sHeader->mRowSize );

    if ( sFlag & SMI_TCFLAG_FILTER_KEY )
    {
        IDE_TEST( aTempCursor->mKeyFilter->minimum.callback(
                      aResult,
                      sTRPInfo.mValuePtr,
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempCursor->mKeyFilter->minimum.data )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( *aResult == ID_FALSE, SKIP );

        IDE_TEST( aTempCursor->mKeyFilter->maximum.callback(
                      aResult,
                      sTRPInfo.mValuePtr,
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempCursor->mKeyFilter->maximum.data )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( *aResult == ID_FALSE, SKIP );
    }
    else
    {
        /* nothing to do */
    }

    if ( sFlag & SMI_TCFLAG_FILTER_ROW )
    {
        IDE_TEST( aTempCursor->mRowFilter->callback(
                      aResult,
                      sTRPInfo.mValuePtr,
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempCursor->mRowFilter->data )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( *aResult == ID_FALSE, SKIP );
    }
    else
    {
        /* nothing to do */
    }

    *aResult             = ID_TRUE;
    aTempCursor->mRowPtr = (UChar*)aTRPHeader;
    SC_COPY_GRID( aTempCursor->mGRID, *aRowGRID );

    sdtSortSegment::convertFromWGRIDToNGRID( sWASeg,
                                             *aRowGRID,
                                             aRowGRID );

    idlOS::memcpy( *aRow,
                   sTRPInfo.mValuePtr,
                   sTRPInfo.mValueLength );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * HitFlag�� �����մϴ�.
 *
 * <IN>
 * aCursor      - ������ Row�� ����Ű�� Ŀ��
 * aHitSeq      - ������ Hit��
 ***************************************************************************/
void sdtTempRow::setHitFlag( smiSortTempCursor* aTempCursor,
                             UInt               aHitSeq )
{
    sdtSortSegHdr * sWASeg     = (sdtSortSegHdr*)aTempCursor->mTTHeader->mWASegment;
    sdtSortTRPHdr * sTRPHeader = (sdtSortTRPHdr*)aTempCursor->mRowPtr;
    sdtWCB        * sWCBPtr;
    scSpaceID       sSpaceID      = SC_MAKE_SPACE( aTempCursor->mGRID );
    smiTempTableHeader * sHeader = aTempCursor->mTTHeader;

#ifdef DEBUG
    if ( sHeader->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN )
    {
        IDE_DASSERT( sHeader->mFetchGroupID == SDT_WAGROUPID_NONE );
        IDE_DASSERT( sSpaceID == SDT_SPACEID_WAMAP );
    }
    else
    {
        if ( ( sHeader->mTTState == SMI_TTSTATE_SORT_INDEXSCAN ) ||
             ( sHeader->mTTState == SMI_TTSTATE_SORT_SCAN )  )
        {
            IDE_DASSERT( ( sSpaceID != SDT_SPACEID_WAMAP ) &&
                         ( sSpaceID != SDT_SPACEID_WORKAREA ) );
        }
        else
        {
            IDE_DASSERT( ( sHeader->mTTState == SMI_TTSTATE_SORT_MERGE ) ||
                         ( sHeader->mTTState == SMI_TTSTATE_SORT_MERGESCAN ) );
        }
    }
    IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );
    IDE_DASSERT( ID_SIZEOF( sTRPHeader->mHitSequence ) == ID_SIZEOF( aHitSeq ) );
#endif

    if ( sTRPHeader->mHitSequence != aHitSeq )
    {
        sTRPHeader->mHitSequence = aHitSeq;

        if ( ( SC_GRID_IS_NULL( aTempCursor->mGRID ) ) ||
             ( sSpaceID == SDT_SPACEID_WAMAP ) )
        {
            /* InMemory �����̱⿡ SetDirty�� �ʿ� ���� */
        }
        else
        {
            if ( sSpaceID == SDT_SPACEID_WORKAREA )
            {
                /* BUG-44074 merge �߰��� ȣ��Ǵ� ��� GRID �� heap�� �ִ� run ����.
                   run �� ���󰡼� setdirty �����ָ� �ش� �������� hitseq ������ ������ ������� �ִ�. */
                if ( ( sHeader->mTTState == SMI_TTSTATE_SORT_MERGE ) ||
                     ( sHeader->mTTState == SMI_TTSTATE_SORT_MERGESCAN ) )
                {
                    /* �ݵ�� �־�� �Ѵ�. */
                    sWCBPtr = sdtSortSegment::findWCB( sWASeg,
                                                       SC_MAKE_PID( aTempCursor->mGRID ));
                    IDE_ASSERT( sWCBPtr != NULL );
                    IDE_ASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

                    sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* setDirty! */
                sWCBPtr = sdtSortSegment::findWCB( sWASeg,
                                                   SC_MAKE_PID( aTempCursor->mGRID ));
                IDE_ASSERT( sWCBPtr != NULL );
                IDE_ASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

                sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
            }
        }//else
    }
    else
    {
        // ( sTRPHeader->mHitSequence == aHitSeq )
        // ���� ������ ó�� �� �ʿ����.
    }

    return;
}

/**************************************************************************
 * Description :
 * HitFlag�� �ִ��� �˻��Ѵ�.
 *
 * <IN>
 * aHitSeq            - ���� Hit Sequence
 * aIsHitFlagged      - Hit Flag ����
 ***************************************************************************/
idBool sdtTempRow::isHitFlagged( smiSortTempCursor * aTempCursor,
                                 ULong               aHitSeq )
{
    sdtSortTRPHdr* sTRPHeader = (sdtSortTRPHdr*)aTempCursor->mRowPtr;

    IDE_DASSERT( ID_SIZEOF( sTRPHeader->mHitSequence ) ==
                 ID_SIZEOF( aHitSeq ) );

    // No Type Casting. (because of above assertion code)
    if ( sTRPHeader->mHitSequence == aHitSeq )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

#endif
