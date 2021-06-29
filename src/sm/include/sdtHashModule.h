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

#ifndef _O_SDT_HASH_MODULE_H_
#define _O_SDT_HASH_MODULE_H_ 1

#include <smiTempTable.h>
#include <idu.h>
#include <idl.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdtHashDef.h>
#include <smuMemStack.h>
#include <smErrorCode.h>
#include <sdtWAExtentMgr.h>

typedef struct sdtHashSlot
{
    scPageID  mPageID;
    scOffset  mOffset;
    UShort    mHashSet;
    ULong     mRowPtrOrHashSet;
}sdtHashSlot;

typedef struct sdtSubHashKey
{
    scPageID mPageID;
    scOffset mOffset;
    UShort   mHashHigh;
}sdtSubHashKey;

typedef struct sdtSubHash
{
    UChar    mHashLow;
    UChar    mKeyCount;
    UShort   mNextOffset;
    scPageID mNextPageID;

    sdtSubHashKey mKey[1];
}sdtSubHash;

#define SDT_SUB_HASH_SIZE (ID_SIZEOF(sdtSubHash) - ID_SIZEOF(sdtSubHashKey))

#define SDT_SUBHASH_MAX_KEYCOUNT            (255)
#define SDT_ALLOC_SUBHASH_MIN_KEYCOUNT      (64)

/******************************** SMALL HASH SET ***********************************/
#define SDT_EXIST_SMALL_HASHSET( aHashSlot, aHashValue )                \
    (( (aHashSlot)->mHashSet & ( 0x0001 << ( (aHashValue) >> 28 ) )) != 0 )

#define SDT_NOT_EXIST_SMALL_HASHSET( aHashSlot, aHashValue )            \
    (( (aHashSlot)->mHashSet & ( 0x0001 << ( (aHashValue) >> 28 ) )) == 0 )

#define SDT_ADD_SMALL_HASHSET( aHashSlot, aHashValue )                  \
    (aHashSlot)->mHashSet |= ( 0x0001 << ( (aHashValue) >> 28 ) );

/******************************** LARGE HASH SET ***********************************/
#define SDT_EXIST_LARGE_HASHSET( aHashSlot, aHashValue )                                            \
    (( 0 != ((aHashSlot)->mRowPtrOrHashSet & ((ULong)(0x0000000000000001) <<  ((aHashValue) >> 26 )))) && \
     ( 0 != ( (aHashSlot)->mHashSet        & ((UShort)(0x00000001) << (((aHashValue) >> 22 ) & 0x0000000F )))))

#define SDT_NOT_EXIST_LARGE_HASHSET( aHashSlot, aHashValue )                                        \
    (( 0 == ((aHashSlot)->mRowPtrOrHashSet & ((ULong)(0x0000000000000001) <<  ((aHashValue) >> 26 )))) || \
     ( 0 == ( (aHashSlot)->mHashSet        & ((UShort)(0x00000001) << (((aHashValue) >> 22 ) & 0x0000000F )))))

#define SDT_ADD_LARGE_HASHSET( aHashSlot, aHashHigh )                                               \
    (aHashSlot)->mRowPtrOrHashSet |= ((ULong)(0x0000000000000001) << ( (aHashHigh) >> 10 ));        \
    (aHashSlot)->mHashSet         |= ((UShort)(0x00000001) << (((aHashHigh) >> 6 ) & 0x0000000F ));

/******************************** HASH VALUE ***********************************/
#define SDT_MAKE_HASH_LOW( aHashValue )                 \
    (UChar)( ((aHashValue) & 0x0000FF00 ) >> 8 )

#define SDT_MAKE_HASH_HIGH( aHashValue )        \
    (UShort)( (aHashValue) >> 16 )

/******************************** FINE HASH SLOT ********************************/
#define SDT_GET_HASH_MAP_IDX( aHashValue, aHashSlotCount )      \
    ( (aHashValue) % (aHashSlotCount) )

#define SDT_GET_HASHSLOT_IN_WAPAGE( aWCBPtr, aMapIdx )    \
    (((sdtHashSlot*)(aWCBPtr)->mWAPagePtr) + ( (aMapIdx) % (SD_PAGE_SIZE / ID_SIZEOF(sdtHashSlot))))

#define SDT_GET_HASHSLOT_WCB( aWASegment, aMapIdx )    \
    ((aWASegment)->mWCBMap + ( sMapIdx / (SD_PAGE_SIZE / ID_SIZEOF(sdtHashSlot))))

/******************************** Single Row ***********************************/
#define SDT_IS_SINGLE_ROW( aOffset )            \
    (((aOffset) & 0x8000 ) != 0 )

#define SDT_IS_NOT_SINGLE_ROW( aOffset )        \
    (((aOffset) & 0x8000 ) == 0 )

#define SDT_SET_SINGLE_ROW( aOffset )           \
    ( (aOffset) |= 0x8000 )

#define SDT_DEL_SINGLE_FLAG_IN_OFFSET( aOffset )        \
    ((aOffset) % SD_PAGE_SIZE )

/******************************** fix Page ***********************************/
#define SDT_IS_FIXED_PAGE( aWCBPtr )  ( (aWCBPtr)->mFix > 0 )
#define SDT_IS_NOT_FIXED_PAGE( aWCBPtr )  ( (aWCBPtr)->mFix == 0 )


class sdtHashModule
{
public:
    /************************ class init/dest ***********************/

    static IDE_RC initializeStatic();

    static void   destroyStatic();

    /************************ property ***********************/
    static IDE_RC resetWASegmentSize( ULong aNewWorkAreaSize );
    static IDE_RC resizeWASegmentPool( UInt   aNewWASegmentCount );
    static void resetTempHashGroupPageCount();
    static UInt calcTempHashGroupExtentCount( UInt aMaxWAExtentCount );
    static void resetTempSubHashGroupPageCount();
    static UInt calcTempSubHashGroupExtentCount( UInt aMaxWAExtentCount );

    static ULong calcOptimalWorkAreaSize( UInt  aRowPageCount );
    static ULong calcSubOptimalWorkAreaSizeByRowCnt( ULong aRowCount );
    static ULong calcSubOptimalWorkAreaSizeBySubHashExtCnt( UInt aSubHashExtentCount );

    static void lock()
    {
        (void)mMutex.lock( NULL );
    }
    static void unlock()
    {
        (void)mMutex.unlock();
    }

    static void initHashTempHeader( smiTempTableHeader * aHeader);
    static void initHashGrp( sdtHashSegHdr     * aWASeg );


    /************************ for view ***********************/
    static void estimateHashSize( smiTempTableHeader * aHeader);
    static UInt getSubHashPageCount()
    {
        return mSubHashPageCount;
    };
    static UInt getHashSlotPageCount()
    {
        return mHashSlotPageCount;
    };
    static UInt getMaxWAExtentCount()
    {
        return mMaxWAExtentCount ;
    };
    static UInt getMaxWAPageCount( sdtHashSegHdr* aWASegment )
    {
        return aWASegment->mMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT;
    };

    static ULong getNPageHashMapPoolSize()
    {
        return (ULong)mNHashMapPool.getNodeSize() * mNHashMapPool.getNodeCount();
    };

    static ULong getSegPoolSize()
    {
        return (ULong)mWASegmentPool.getNodeSize() * mWASegmentPool.getNodeCount();
    };

    static UInt getWASegmentUsedPageCount(sdtHashSegHdr* aWASegment )
    {
        return ( aWASegment->mAllocWAPageCount + SDT_WAEXTENT_PAGECOUNT - 1 )
            / SDT_WAEXTENT_PAGECOUNT * SDT_WAEXTENT_PAGECOUNT;
    };
    static UInt   getNExtentCount( sdtHashSegHdr* aWASegment )
    {
        return getRowNExtentCount( aWASegment ) + getSubHashNExtentCount( aWASegment );
    }
    /************************ Interface ***********************/
    static IDE_RC createHashSegment( smiTempTableHeader * aHeader,
                                     ULong                aInitWorkAreaSize );

    static IDE_RC dropHashSegment( sdtHashSegHdr* aWASegment );

    static IDE_RC truncateHashSegment( sdtHashSegHdr* aWASegment );

    static IDE_RC clearHashSegment(sdtHashSegHdr* aWASegment );

    inline static IDE_RC insert( smiTempTableHeader * aHeader,
                                 smiValue   * aValue,
                                 UInt         aHashValue,
                                 scGRID     * aGRID,
                                 idBool     * aResult );
    static IDE_RC append( sdtHashSegHdr     * aWASegment,
                          sdtHashInsertInfo * aTRPInfo,
                          sdtHashInsertResult * aTRInsertResult );

    inline static IDE_RC openHashCursor( smiHashTempCursor * aCursor,
                                         UChar            ** aRow,
                                         scGRID            * aRowGRID );
    inline static IDE_RC openUpdateHashCursor( smiHashTempCursor * aCursor,
                                               UChar            ** aRow,
                                               scGRID            * aRowGRID );
    static IDE_RC openFullScanCursor( smiHashTempCursor * aCursor,
                                      UChar            ** aRow,
                                      scGRID            * aRowGRID );

    static IDE_RC initHashCursor( smiHashTempCursor  * aCursor,
                                  UInt                 aCursorType );
    inline static IDE_RC fetchHashNext( smiHashTempCursor * aCursor,
                                        UChar            ** aRow,
                                        scGRID            * aRowGRID );

    inline static IDE_RC fetchFullNext( smiHashTempCursor  * aCursor,
                                        UChar             ** aRow,
                                        scGRID             * aRowGRID  );

    inline static IDE_RC fetchFromGRID( smiTempTableHeader * aHeader,
                                        scGRID               aGRID,
                                        void               * aDestRowBuf );

    inline static idBool checkHashSlot( smiHashTempCursor* aHashCursor,
                                        UInt               aHashValue );

    inline static idBool checkUpdateHashSlot( smiHashTempCursor* aHashCursor,
                                              UInt               aHashValue );

    inline static void   setHitFlag( smiHashTempCursor * aCursor );
    inline static idBool isHitFlagged( smiHashTempCursor * aCursor );

    inline static IDE_RC update( smiHashTempCursor * aTempCursor,
                                 smiValue          * aValueList );

private:
    /************************ Insert ***********************/
    static IDE_RC appendRowPiece( sdtWCB              * aWCBPtr,
                                  sdtHashInsertInfo   * aTRPInfo,
                                  sdtHashInsertResult * aTRInsertResult );

    static IDE_RC chkUniqueInsertableInMem( smiTempTableHeader  * aHeader,
                                            const smiValue      * aValueList,
                                            UInt                  aHashValue,
                                            sdtHashTRPHdr       * aTRPHeader,
                                            idBool              * aResult );

    static IDE_RC chkUniqueInsertableOutMem( smiTempTableHeader  * aHeader,
                                             const smiValue      * aValueList,
                                             UInt                  aHashValue,
                                             sdtHashSlot         * aHashSlot,
                                             idBool              * aResult );
    static IDE_RC chkUniqueInsertableSingleRow( smiTempTableHeader  * aHeader,
                                                sdtHashSegHdr       * aWASegment,
                                                const smiValue      * aValueList,
                                                scPageID              aSingleRowPageID,
                                                scOffset              aSingleRowOffset,
                                                idBool              * aResult );
    static IDE_RC chkUniqueInsertableSubHash( smiTempTableHeader  * aHeader,
                                              sdtHashSegHdr       * aWASegment,
                                              const smiValue      * aValueList,
                                              UInt                  aHashValue,
                                              scPageID              aSubHashPageID,
                                              scOffset              aSubHashOffset,
                                              idBool              * aResult );
    static IDE_RC allocNewPage( sdtHashSegHdr     * aWASegment,
                                sdtHashInsertInfo * aTRPInfo,
                                sdtWCB           ** aNewWCBPtr );

    static void setInsertHintWCB( sdtHashSegHdr * aSegHdr,
                                  sdtWCB        * aHintWCBPtr );

    static idBool compareRowAndValue( smiTempTableHeader * aHeader,
                                      const void         * aRow,
                                      const smiValue     * aValueList );

    static IDE_RC reserveSubHash( sdtHashSegHdr * aWASegment,
                                  sdtHashGroup  * aGroup,
                                  SInt            aReqMinSlotCount,
                                  sdtSubHash   ** aSubHashPtr,
                                  UInt          * aSlotMaxCount,
                                  scPageID      * aPageID,
                                  scOffset      * aOffset );

    static IDE_RC mergeSubHash( sdtHashSegHdr * aWASegment );
    static IDE_RC mergeSubHashOneSlot( sdtHashSegHdr* aWASegment,
                                       sdtHashSlot  * aHashSlot,
                                       UInt           aSlotIdx );
    static IDE_RC buildSubHash( sdtHashSegHdr * aWASegment );
    static void addSubHashPageFreeOffset( sdtHashSegHdr * aWASegment,
                                          scOffset        aFreeOffset )
    {
        ((sdtHashPageHdr*)aWASegment->mSubHashWCBPtr->mWAPagePtr)->mFreeOffset += aFreeOffset;
    };

    static IDE_RC prepareOutMemoryHash( sdtHashSegHdr * aWASegment );

    static IDE_RC checkExtentTerm4Insert( sdtHashSegHdr    * aWASegment,
                                          sdtHashInsertInfo* aTRPInfo );
    static IDE_RC lackTotalWAPage( sdtHashSegHdr    * aWASegment,
                                   sdtHashInsertInfo* aTRPInfo );

    /*********************** Fetch **************************/
    static void initToHashScan( sdtHashSegHdr     * aWASegment );
    static void initToFullScan( sdtHashSegHdr     * aWASegment );
    static IDE_RC getNextNPageForFullScan( smiHashTempCursor  * aCursor );
    inline static IDE_RC getHashSlotPtr( sdtHashSegHdr  * aWASegment,
                                         UInt    aHashValue,
                                         void ** aSlotPtr );

    inline static IDE_RC fetch( sdtHashSegHdr   * aWASegment,
                                UChar           * aRowBuffer,
                                sdtHashScanInfo * aTRPInfo );

    static IDE_RC fetchChainedRowPiece( sdtHashSegHdr   * aWASegment,
                                        UChar           * aRowBuffer,
                                        sdtHashScanInfo * aTRPInfo );

    static IDE_RC filteringAndFetch( smiHashTempCursor * aTempCursor,
                                     sdtHashTRPHdr     * aTRPHeader,
                                     UChar             * aRow,
                                     idBool            * aResult);

    inline static IDE_RC fetchHashScanInternalPtr(smiHashTempCursor * aCursor,
                                                  sdtHashTRPHdr     * aTRPHeader,
                                                  UChar            ** aRow,
                                                  scGRID            * aRowGRID );
    inline static IDE_RC fetchHashUpdate( smiHashTempCursor * aCursor,
                                          UChar            ** aRow,
                                          scGRID            * aRowGRID );
    static IDE_RC fetchHashScanInternalGRID(smiHashTempCursor * aCursor,
                                            UChar            ** aRow,
                                            scGRID            * aRowGRID );
    static IDE_RC fetchHashScanInternalSubHash( smiHashTempCursor * aCursor,
                                                UChar            ** aRow,
                                                scGRID            * aRowGRID );
    static IDE_RC buildSubHashAndResetInsertInfo( sdtHashSegHdr    * aWASegment,
                                                  sdtHashInsertInfo* aTRPInfo );
    static UInt bSearchSubHash( sdtSubHash * aSubHash, UInt aHashValue );
    static void quickSortSubHash( sdtSubHashKey arr[], SInt left, SInt right );
    /*********************** Update **************************/
    static IDE_RC updateChainedRowPiece( smiHashTempCursor * aTempCursor,
                                         smiValue          * aValueList );

    /******************************** Segment *******************************/

    static IDE_RC expandFreeWAExtent( sdtHashSegHdr* aWASeg );
    static void   clearAllWCBs( sdtHashSegHdr* aWASegment );

    inline static void initWCB( sdtWCB       * aWCBPtr );

    inline static void initTempPage( sdtHashPageHdr  * aPageHdr,
                                     sdtTempPageType   aType,
                                     scPageID          aSelfPID );

    static IDE_RC getFreeWAPage( sdtHashSegHdr * aWASegment,
                                 sdtHashGroup  * aGroup,
                                 sdtWCB       ** aWCBPtr );

    static UInt calcWAExtentCount( ULong aHashAreaSize )
    {
        return (UInt)(( aHashAreaSize + SDT_WAEXTENT_SIZE - 1 ) / SDT_WAEXTENT_SIZE ) ;
    }
    static UInt getWAPageCount()
    {
        return getMaxWAExtentCount() * SDT_WAEXTENT_PAGECOUNT ;
    }

    static UInt calcWAPageCount( ULong aHashAreaSize )
    {
        return calcWAExtentCount( aHashAreaSize) * SDT_WAEXTENT_PAGECOUNT ;
    }
    static ULong getWASegmentSize()
    {
        return calcWASegmentSize( getWAPageCount() );
    }
    static ULong calcWASegmentSize( ULong aWAPageCount )
    {
        return SDT_HASH_SEGMENT_HEADER_SIZE + ( aWAPageCount * ID_SIZEOF(sdtWCB) );
    }

    /*************************** Page ���� Operation ***************************/
    static void fixWAPage( sdtWCB * aWCBPtr )
    {
        aWCBPtr->mFix++;
        IDE_DASSERT( aWCBPtr->mFix > 0 );
    };
    static void unfixWAPage( sdtWCB * aWCBPtr )
    {
        IDE_DASSERT( aWCBPtr->mFix > 0 );
        aWCBPtr->mFix--;
    };
    static IDE_RC getSlotPtr( sdtHashSegHdr * aWASegment,
                              sdtHashGroup  * aFetchGrp,
                              scPageID        aNPageID,
                              scOffset        aOffset,
                              UChar        ** aNSlotPtr,
                              sdtWCB       ** aFixedWCBPtr );
    inline static sdtWCB * getWCBPtr(sdtHashSegHdr * aWASegment,
                                     scPageID        aWPID )
    {
        IDE_DASSERT( getMaxWAPageCount( aWASegment ) > aWPID );
        return aWASegment->mWCBMap + aWPID ;
    };

    static  IDE_RC setWAPagePtr4HashSlot( sdtHashSegHdr * aWASegment,
                                          sdtWCB        * aWCBPtr );
    static  IDE_RC set64WAPagesPtr( sdtHashSegHdr * aWASegment,
                                    sdtWCB        * aWCBPtr );

    inline static scPageID  getNPID( sdtWCB * aWCBPtr )
    {
        return aWCBPtr->mNPageID;
    }

    static scPageID  getWPageID( sdtHashSegHdr* aWASegment,
                                 sdtWCB       * aWCBPtr )
    {
        return (scPageID)( aWCBPtr - aWASegment->mWCBMap );
    }

    static IDE_RC assignNPage(sdtHashSegHdr* aWASegment,
                              sdtWCB       * aWCBPtr,
                              scPageID       aNPageID);
    static IDE_RC unassignNPage(sdtHashSegHdr* aWASegment,
                                sdtWCB       * aWCBPtr );

    inline static UInt getNPageHashValue( sdtHashSegHdr* aWASegment,
                                          scPageID       aNPID)
    {
        return  ( aNPID ) % aWASegment->mNPageHashBucketCnt;
    };

    static sdtWCB * findWCB( sdtHashSegHdr* aWASegment,
                             scPageID       aNPID);

    static IDE_RC buildNPageHashMap( sdtHashSegHdr* aWASegment );


    static IDE_RC readNPage( sdtHashSegHdr* aWASegment,
                             sdtWCB       * aWCBPtr,
                             scPageID       aNPageID);

    static IDE_RC readNPages( sdtHashSegHdr* aWASegment,
                              scPageID       aFstNPageID,
                              UInt           aPageCount,
                              sdtWCB       * aFstWCBPtr,
                              idBool         aNeedFixPage );

    static IDE_RC preReadNExtents( sdtHashSegHdr * aWASegment,
                                   scPageID        aFstNPageID,
                                   UInt            aPageCount);
    static sdtWCB * getFreePage4PreRead( sdtHashSegHdr * aWASegment,
                                         scPageID        aFstNPageID,
                                         scPageID        aEndNPageID,
                                         sdtWCB        * aPrvWCBPtr );

   static IDE_RC writeNPage( sdtHashSegHdr* aWASegment,
                             sdtWCB       * aWCBPtr );

    static IDE_RC allocAndAssignNPage( sdtHashSegHdr    * aWASegment,
                                       sdtNExtFstPIDList* aNExtFstPIDList,
                                       sdtWCB           * aTargetWCBPtr );

    static UInt   getRowNExtentCount( sdtHashSegHdr* aWASegment )
    {
        return aWASegment->mNExtFstPIDList4Row.mCount;
    }
    static UInt   getSubHashNExtentCount( sdtHashSegHdr* aWASegment )
    {
        return aWASegment->mNExtFstPIDList4SubHash.mCount;
    }

    static UInt allocSlot( sdtHashPageHdr * aPageHdr,
                           UInt             aNeedSize,
                           UChar         ** aSlotPtr );

    static sdtTempPageType getType( sdtHashPageHdr *aPageHdr )
    {
        return (sdtTempPageType)(aPageHdr->mType);
    };

    /* FreeSpace�� ������ Offset */
    static UInt getFreeOffset( sdtHashPageHdr *aPageHdr )
    {
        return aPageHdr->mFreeOffset;
    };
    static void setFreeOffset( sdtHashPageHdr *aPageHdr,
                               scOffset        aFreeOffset )
    {
        aPageHdr->mFreeOffset = aFreeOffset;
    };

public:
    /***********************************************************
     * Dump�� �Լ���
     ***********************************************************/

    static void exportHashSegmentToFile( sdtHashSegHdr* aWASegment );
    static IDE_RC importHashSegmentFromFile( SChar         * aFileName,
                                             sdtHashSegHdr** aHashSegHdr,
                                             sdtHashSegHdr * aRawHashSegHdr,
                                             UChar        ** aPtr );
    static IDE_RC resetWCBInfo4Dump( sdtHashSegHdr * aWASegment,
                                     UChar         * aOldWASegPtr,
                                     UInt            aMaxPageCount,
                                     UChar        ** aPtr );
    static void dumpHashSegment( sdtHashSegHdr * aWASegment,
                                 SChar         * aOutBuf,
                                 UInt            aOutSize );
    static void dumpHashPageHeaders( sdtHashSegHdr  * aWASegment,
                                     SChar          * aOutBuf,
                                     UInt             aOutSize );

    static void dumpWAHashMap( sdtHashSegHdr * aWASeg,
                               SChar         * aOutBuf,
                               UInt            aOutSize );
    static void dumpWAHashPage( void     * aPagePtr,
                                scPageID   aWAPageID,
                                idBool     aTitle,
                                SChar    * aOutBuf,
                                UInt       aOutSize );
    static void dumpTempTRPHeader( void     * aTRPHeader,
                                   SChar    * aOutBuf,
                                   UInt       aOutSize );
    static void dumpTempTRPInfo4Insert( void       * aTRPInfo,
                                        SChar      * aOutBuf,
                                        UInt         aOutSize );
    static void dumpHashTempPage( sdtHashSegHdr  * aWASegment,
                                  scPageID         aWAPageID,
                                  SChar          * aOutBuf,
                                  UInt             aOutSize );
    static void dumpHashRowPage( void   * aPagePtr,
                                 SChar  * aOutBuf,
                                 UInt     aOutSize );

    static void dumpSubHash( sdtSubHash * aSubHash,
                             SChar      * aOutBuf,
                             UInt         aOutSize );

    static void dumpWCBs( void           * aWASegment,
                          SChar          * aOutBuf,
                          UInt             aOutSize );

    static void dumpWCB( void           * aWCB,
                         SChar          * aOutBuf,
                         UInt             aOutSize );

private:
    static iduMutex      mMutex;
    static smuMemStack   mWASegmentPool;
    static smuMemStack   mNHashMapPool;
    static UInt          mMaxWAExtentCount;
    static smiColumn     mBlankColumn;
    static UInt          mSubHashPageCount;
    static UInt          mHashSlotPageCount;
} ;

/**************************************************************************
 * Description : hash value�� �ش��ϴ� Hash slot�� �����´�.
 *               ������ ���� �����´�.
 *               Total WA�� �����ϸ� malloc�ؼ� �����´�.
 *
 * aWASegment  - [IN] Hash Temp Segment
 * aHashValue  - [IN] HashSlot��ġ�� ��Ÿ�� Hash Value
 * aSlotPtr    - [OUT] ��ȯ�� Hash Slot
 ***************************************************************************/
IDE_RC sdtHashModule::getHashSlotPtr( sdtHashSegHdr  * aWASegment,
                                      UInt             aHashValue,
                                      void          ** aSlotPtr )
{
    UInt       sMapIdx;
    sdtWCB   * sWCBPtr;

    sMapIdx = SDT_GET_HASH_MAP_IDX( aHashValue,
                                    aWASegment->mHashSlotCount );

    sWCBPtr = SDT_GET_HASHSLOT_WCB( aWASegment,
                                    sMapIdx );

    if ( sWCBPtr->mWAPagePtr == NULL )
    {
        /* TOTAL WA�� �����ص� 1���� �޾ƿ´�. */
        IDE_TEST( setWAPagePtr4HashSlot( aWASegment, sWCBPtr ) != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( sWCBPtr->mNxtUsedWCBPtr != NULL );
    }

    *aSlotPtr = (void*) SDT_GET_HASHSLOT_IN_WAPAGE( sWCBPtr, sMapIdx );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : �����͸� �����Ѵ�.
 *               Unique �� ��� ���� �ִ��� Ȯ���Ѵ�.
 *               Unique ����, insert 4 update ���� ��쿡��
 *               Unique check�� ���� �� �� �ִ�.
*
 * aHeader      - [IN] ��� Temp Table Header
 * aValue       - [IN] ������ Value
 * aHashValue   - [IN] ������ HashValue
 * aGRID        - [OUT] ������ ��ġ
 * aResult      - [OUT] ������ �����Ͽ��°�?( UniqueViolation Check�� )
 ***************************************************************************/
IDE_RC sdtHashModule::insert( smiTempTableHeader  * aHeader,
                              smiValue            * aValue,
                              UInt                  aHashValue,
                              scGRID              * aGRID,
                              idBool              * aResult )
{
    sdtHashSegHdr      * sWASeg = (sdtHashSegHdr*)aHeader->mWASegment;
    sdtHashInsertInfo    sTRPInfo4Insert;
    sdtHashInsertResult  sTRInsertResult;
    sdtHashSlot        * sHashSlot;

    IDE_DASSERT(( sWASeg->mOpenCursorType == SMI_HASH_CURSOR_NONE ) ||
                ( sWASeg->mOpenCursorType == SMI_HASH_CURSOR_HASH_UPDATE ));

    if( aHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aHeader->mStatistics ) != IDE_SUCCESS);
        aHeader->mCheckCnt = 0;
    }

    aHeader->mTTState = SMI_TTSTATE_HASH_INSERT;

    IDE_TEST( getHashSlotPtr( sWASeg,
                              aHashValue,
                              (void**)&sHashSlot )
              != IDE_SUCCESS );

    if ( sWASeg->mUnique == SDT_HASH_UNIQUE )
    {
        /* group by�� ���� ���� �ִ��� ���� Ȯ���ϰ� insert�Ѵ�.
         * �׷��Ƿ� group by�� ���� ���� ������ �ȵǰ� ���⿡���� ������ �Ѵ�.
         * property�� ������ ���� �� �� �ִ�. */
        if (( sWASeg->mOpenCursorType == SMI_HASH_CURSOR_NONE ) ||
            (( sWASeg->mOpenCursorType == SMI_HASH_CURSOR_HASH_UPDATE ) &&
             ( smuProperty::getTmpCheckUnique4Update() == ID_TRUE )))
        {
            /* Insert�� �Ϸ� �� �� ������ small head set�� �����ȴ�.*/
            if ( SDT_EXIST_SMALL_HASHSET( sHashSlot, aHashValue ) )
            {
                if ( sWASeg->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
                {
                    IDE_TEST( chkUniqueInsertableInMem( aHeader,
                                                        aValue,
                                                        aHashValue,
                                                        (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet,
                                                        aResult )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( chkUniqueInsertableOutMem( aHeader,
                                                         aValue,
                                                         aHashValue,
                                                         sHashSlot,
                                                         aResult )
                              != IDE_SUCCESS );
                }
                IDE_TEST_CONT( *aResult == ID_FALSE , ERR_UNIQUENESS_VIOLATION );
            }
            else
            {
                /* Hash Set�� ������ ���� Row�̴�. �ٷ� Insert�ϸ� �ȴ�.*/
            }
        }
    }
    aHeader->mRowCount++;
    *aResult = ID_TRUE;

    sTRPInfo4Insert.mTRPHeader.mTRFlag      = SDT_TRFLAG_CHILDGRID;
    sTRPInfo4Insert.mTRPHeader.mIsRow       = SDT_HASH_ROW_HEADER;
    sTRPInfo4Insert.mTRPHeader.mHitSequence = 0;
    sTRPInfo4Insert.mTRPHeader.mValueLength = 0; /* append�ϸ鼭 ������ */
    sTRPInfo4Insert.mTRPHeader.mHashValue   = aHashValue;

    sTRPInfo4Insert.mTRPHeader.mNextRowPiecePtr = NULL;
    sTRPInfo4Insert.mTRPHeader.mChildRowPtr     = (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet;
    sTRPInfo4Insert.mTRPHeader.mChildPageID     = sHashSlot->mPageID;
    sTRPInfo4Insert.mTRPHeader.mChildOffset     = sHashSlot->mOffset;
    sTRPInfo4Insert.mTRPHeader.mNextPageID      = 0;
    sTRPInfo4Insert.mTRPHeader.mNextOffset      = 0;

    sTRPInfo4Insert.mColumnCount = aHeader->mColumnCount;
    sTRPInfo4Insert.mColumns     = aHeader->mColumns;
    sTRPInfo4Insert.mValueLength = aHeader->mRowSize;
    sTRPInfo4Insert.mValueList   = aValue;
    sTRInsertResult.mComplete    = ID_FALSE;

    IDE_TEST( append( sWASeg,
                      &sTRPInfo4Insert,
                      &sTRInsertResult )
              != IDE_SUCCESS );

    IDE_ERROR( sTRInsertResult.mComplete == ID_TRUE );
    IDE_DASSERT( sTRPInfo4Insert.mTRPHeader.mChildRowPtr == (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet );

    IDE_DASSERT( sTRInsertResult.mHeadRowpiecePtr->mChildPageID == sHashSlot->mPageID );
    IDE_DASSERT( sTRInsertResult.mHeadRowpiecePtr->mChildOffset == sHashSlot->mOffset );
    IDE_DASSERT( sTRInsertResult.mHeadRowpiecePtr->mChildRowPtr == (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet );

    /* �޾Ƶ� hash slot�� ���� */
    SDT_ADD_SMALL_HASHSET( sHashSlot, aHashValue );
    sHashSlot->mPageID          = sTRInsertResult.mHeadPageID;
    sHashSlot->mOffset          = sTRInsertResult.mHeadOffset;
    sHashSlot->mRowPtrOrHashSet = (ULong)sTRInsertResult.mHeadRowpiecePtr;

    IDE_DASSERT(((sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet)->mHashValue == aHashValue );

    SC_MAKE_GRID( *aGRID,
                  aHeader->mSpaceID,
                  sTRInsertResult.mHeadPageID,
                  sTRInsertResult.mHeadOffset );

    IDE_EXCEPTION_CONT( ERR_UNIQUENESS_VIOLATION );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : hash value�� �ش��ϴ� Hash slot�� �����ϴ��� Ȯ���ϰ�
 *               ������ �����´�.
 *               HASHSET���� �뷫���� ������ Ȯ�� �� �� �־, ����
 *               hash value�� ������� Hash Set���� 60~90% �̸� �� �� �ִ�.
 *               ���� ���� ȣ��Ǵ� �Լ��� ���ɿ� �Ű� ��� �Ѵ�.
 *               Cursor�� Hash Slot�� Hash Value�� �����ؼ� ��ȯ�Ѵ�.
 *
 * aHashCursor - [IN] Ž���� ��� �� cursor
 * aHashValue  - [IN] ã�� Hash value ��
 ***************************************************************************/
idBool sdtHashModule::checkHashSlot( smiHashTempCursor* aHashCursor,
                                     UInt               aHashValue )
{
    sdtWCB      * sWCBPtr;
    sdtHashSlot * sHashSlot;
    UInt          sMapIdx;

    sMapIdx = SDT_GET_HASH_MAP_IDX( aHashValue,
                                    aHashCursor->mHashSlotCount );

    sWCBPtr = SDT_GET_HASHSLOT_WCB( (sdtHashSegHdr*)aHashCursor->mWASegment,
                                    sMapIdx );

    if ( sWCBPtr->mWAPagePtr == NULL )
    {
        return ID_FALSE;
    }

    IDE_DASSERT( sWCBPtr->mNxtUsedWCBPtr != NULL );

    sHashSlot = SDT_GET_HASHSLOT_IN_WAPAGE( sWCBPtr, sMapIdx );

    if ( aHashCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        /* in memory �� ��� small hash set Ȯ�� */
        if ( SDT_NOT_EXIST_SMALL_HASHSET( sHashSlot, aHashValue ) )
        {
            return ID_FALSE;
        }
    }
    else
    {
        if ( SDT_IS_SINGLE_ROW( sHashSlot->mOffset ) )
        {
            /* Single Row�� ��� Hash valueȮ�� */
            if ( (UInt)sHashSlot->mRowPtrOrHashSet != aHashValue )
            {
                return ID_FALSE;
            }
        }
        else
        {
            /* out memory �� ��� small hash set Ȯ�� */
            if ( SDT_NOT_EXIST_LARGE_HASHSET( sHashSlot, aHashValue ) )
            {
                return ID_FALSE;
            }
        }
    }

    aHashCursor->mHashSlot  = sHashSlot;
    aHashCursor->mHashValue = aHashValue;

    return ID_TRUE;
}

/**************************************************************************
 * Description : hash value�� �ش��ϴ� Hash slot�� �����ϴ��� Ȯ���ϰ�
 *               ������ �����´�. update cursor�����̴�.
 *               HASHSET���� �뷫���� ������ Ȯ�� �� �� �־, ����
 *               hash value�� ������� Hash Set���� 60~90% �̸� �� �� �ִ�.
 *               ���� ���� ȣ��Ǵ� �Լ��� ���ɿ� �Ű� ��� �Ѵ�.
 *               update cursor�� insert�� �Բ� �����ϹǷ�
 *               small hash set �ۿ� ��� �� �� ����.
 *               ��Ȯ���� ���� ��������.
 *
 * aHashCursor - [IN] Ž���� ��� �� cursor
 * aHashValue  - [IN] ã�� Hash value ��
 ***************************************************************************/
idBool sdtHashModule::checkUpdateHashSlot( smiHashTempCursor* aHashCursor,
                                           UInt               aHashValue )
{
    sdtWCB      * sWCBPtr;
    sdtHashSlot * sHashSlot;
    UInt          sMapIdx;

    sMapIdx = SDT_GET_HASH_MAP_IDX( aHashValue,
                                    aHashCursor->mHashSlotCount );

    sWCBPtr = SDT_GET_HASHSLOT_WCB( (sdtHashSegHdr*)aHashCursor->mWASegment,
                                    sMapIdx );

    if ( sWCBPtr->mWAPagePtr != NULL )
    {
        IDE_DASSERT( sWCBPtr->mNxtUsedWCBPtr != NULL );

        sHashSlot = SDT_GET_HASHSLOT_IN_WAPAGE( sWCBPtr, sMapIdx );

        /* insert 4 update�� insert�� ���ÿ� select�ϹǷ�
         * insert �Ϸ� �Ŀ� �ۼ��ϴ� large hash set�� ����.*/
        if ( SDT_EXIST_SMALL_HASHSET( sHashSlot, aHashValue ) )
        {
            /* fetch �߿� �ǵ��� cursor���� �Ϸ��ϰ� wa segment�� ������� ��������
             * ���� �� �а��ε�, insert for update ������ fetch���߿� ���� �� �� �־
             * ��¿�� ���� ���⿡�� �����Ѵ�.*/
            aHashCursor->mIsInMemory = ((sdtHashSegHdr*)aHashCursor->mWASegment)->mIsInMemory;

            /* offset�� single row�� ������ hash value�� ���غ���.
             * single row �� �ƴϸ� open cursor���� Ȯ���Ѵ�. */
            if (( aHashCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY ) ||
                ( SDT_IS_NOT_SINGLE_ROW( sHashSlot->mOffset )) ||
                ( (UInt)sHashSlot->mRowPtrOrHashSet == aHashValue ))
            {
                aHashCursor->mHashSlot  = sHashSlot;
                aHashCursor->mHashValue = aHashValue;
                return ID_TRUE;
            }
        }
    }

    aHashCursor->mWCBPtr = NULL;

    return ID_FALSE;
}

/***************************************************************************
 * Description : Hash Cursor�� Open�ϰ�, Cursor�� �����Ǿ� �ִ�
 *               Hash Slot�� Hash Value�� �������� ù�� ° row�� �����´�.
 *               ���� ������ Hash Scan�� �ƴϾ��ٸ�,
 *               WorkArea ������ Hash Scan�� ��︮���� �����Ѵ�.
 *               �ݵ�� Insert �� �Ϸ� �� �Ŀ��� Hash Cursor�� Open�� �� ������
 *               �ѹ� Hash Cursor�� open�Ǹ� �� �Ŀ� Insert �� �� ����.
 *
 * aCursor  - [IN]  Ž���� ���Ǵ� cursor
 * aRow     - [OUT] ã�� Row
 * aRowGRID - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::openHashCursor( smiHashTempCursor  * aCursor,
                                      UChar             ** aRow,
                                      scGRID             * aRowGRID )
{
    sdtHashSlot   * sHashSlot  = (sdtHashSlot*)aCursor->mHashSlot;
    sdtHashSegHdr * sWASeg;

    // Hash Cursor�����̴�.
    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_HASHSCAN ) );

    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) );
    /* Hash���� OrderScan�� �Ұ����� */
    IDE_DASSERT( SM_IS_FLAG_OFF( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) );
    /* Row�� ��ġ : page�� fix ���� �����Ƿ� �ٲ�� �־ ���� ���� �ʿ� */

    sWASeg = (sdtHashSegHdr*)aCursor->mWASegment;
    if ( sWASeg->mOpenCursorType != SMI_HASH_CURSOR_HASH_SCAN )
    {
        if( aCursor->mIsInMemory == SDT_WORKAREA_OUT_MEMORY )
        {
            initToHashScan( sWASeg );
            aCursor->mTTHeader->mTTState = SMI_TTSTATE_HASH_FETCH_HASHSCAN;
        }
        sWASeg->mOpenCursorType = SMI_HASH_CURSOR_HASH_SCAN;
        aCursor->mTTHeader->mTTState = SMI_TTSTATE_HASH_FETCH_HASHSCAN;
    }

    aCursor->mRowPtr      = NULL;
    aCursor->mChildRowPtr = NULL;
    aCursor->mWAPagePtr   = NULL;

    if ( aCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        /* InMemory �����̴�, Row Pointer�� Ž���Ѵ�. */
        aCursor->mChildPageID = sHashSlot->mPageID;
        aCursor->mChildOffset = sHashSlot->mOffset;

        return fetchHashScanInternalPtr( aCursor,
                                         (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet,
                                         aRow,
                                         aRowGRID );
    }
    else
    {
        /* Out Memory �����̴�, ChildPageID, Offset���� Ž���Ѵ�. */
        if ( SDT_IS_SINGLE_ROW( sHashSlot->mOffset ) )
        {
            /* CheckHashSlot���� HashValue�� �̹� Ȯ�� �Ͽ���. */
            IDE_DASSERT( (UInt)sHashSlot->mRowPtrOrHashSet == aCursor->mHashValue );

            /* Single���� Single flag�� ��ϵǾ� ����
             * DISCARD & Single������ ��ȯ�ϸ� �� */
            aCursor->mChildOffset = SDT_DEL_SINGLE_FLAG_IN_OFFSET( sHashSlot->mOffset );
            aCursor->mChildPageID = sHashSlot->mPageID;

            aCursor->mSubHashPtr = NULL;
            aCursor->mSubHashIdx = 0;

            return fetchHashScanInternalGRID( aCursor,
                                              aRow,
                                              aRowGRID );
        }
        else
        {
            /* Sub Hash�� �������� Ž���Ѵ�. */
            IDE_DASSERT( SDT_EXIST_LARGE_HASHSET( sHashSlot, aCursor->mHashValue ) );

            aCursor->mSubHashPtr = NULL;
            aCursor->mSubHashIdx = 0;

            aCursor->mChildPageID = sHashSlot->mPageID;
            aCursor->mChildOffset = sHashSlot->mOffset;

            return fetchHashScanInternalSubHash( aCursor,
                                                 aRow,
                                                 aRowGRID );
        }
    }
}

/***************************************************************************
 * Description : Update�� hash scan�̴�. Hash Cursor�ʹ� �ٸ��� fetch,
 *               update, Insert�� ����� �Բ� �����ϹǷ� Pointer, Sub Hash,
 *               Single Row ���� ��� ���������� Ž���ؾ� �ϸ�
 *               Cursor open�ÿ� Work Area�� ������ ������� �ʴ´�.
 *               �ַ� Group By� ���ȴ�.
 *               ���� ù��° row�� �Բ� �������� Unique ���� �̹Ƿ� Next�� ����.
 *
 * aCursor    - [IN] Hash Temp Cursor
 * aRow       - [OUT] ã�� Row,
 * aRowGRID   - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::openUpdateHashCursor( smiHashTempCursor  * aCursor,
                                            UChar             ** aRow,
                                            scGRID             * aRowGRID )
{
    // Hash Cursor�����̴�.
    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_HASHSCAN ) );

    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) );
    /* Hash���� OrderScan�� �Ұ����� */
    IDE_DASSERT( SM_IS_FLAG_OFF( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) );

    aCursor->mRowPtr      = NULL;
    aCursor->mChildRowPtr = NULL;

    /* Cache ������ : UniqueHash �� ������� �ʴ´�. */
    aCursor->mWAPagePtr = NULL;

    return fetchHashUpdate( aCursor,
                            aRow,
                            aRowGRID );
}

/**************************************************************************
 * Description : ���� Row�� �޾ƿ´�. Hash Scan�����̴�.
 *               Unique�� Next�� �����Ƿ� ������ �ʴ´�..
 *
 * aCursor  - [IN] ��� Cursor
 * aRow     - [OUT] ã�� Row,
 * aRowGRID - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::fetchHashNext( smiHashTempCursor * aCursor,
                                     UChar            ** aRow,
                                     scGRID            * aRowGRID )
{
    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_HASHSCAN ) );

    IDE_DASSERT( aCursor->mWAPagePtr == NULL );
    IDE_DASSERT( ((sdtHashSegHdr*)aCursor->mWASegment)->mOpenCursorType == SMI_HASH_CURSOR_HASH_SCAN );

    /* �ռ� ���� row�� �ְ�, ���� value�� ���� Next Row�� ã���� �Դ�. */
    IDE_DASSERT( aCursor->mChildPageID != SM_NULL_PID );

    if ( aCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        return fetchHashScanInternalPtr( aCursor,
                                         (sdtHashTRPHdr*)aCursor->mChildRowPtr,
                                         aRow,
                                         aRowGRID );
    }
    else
    {
        return fetchHashScanInternalSubHash( aCursor,
                                             aRow,
                                             aRowGRID );
    }
    /* Fetch Next���� Single Row Ž���� ����.
     * Single Row�� Hash Slot���� Row�� ����ų ����,
     * Ȥ�� Unique Insert Ȯ�� �� ����, updaet cursor����,
     * In Memory Row�� ������ row���� Disk�� ���� �� Next Row��
     * ����ų ��쿡 �����Ѵ�.
     * �Ϲ� ���� Hash Scan������ alloc cursor ������
     * In Memory Row�� ������ �ʰ� ��� Sub Hash�� ����� ���Ȱ�
     * �� Fetch First���� Hash Slot���� ����Ű�� Row �� �̹� �о���.
     * �׷��Ƿ� Fetch Next������ Single Row�� ������ �ʴ´�. */
}

/**************************************************************************
 * Description : Cursor������ �������� Row�� �޾ƿ´�.
 *               InMemory ���¿��� Row Pointer�� ���󰡸� Ž���Ѵ�.
 *               First, Next�� ��� ���ȴ�. fetch update������ ������ �ʴ´�.
 *
 * aCursor     - [IN] ��� Cursor
 * aTRPHeader  - [IN] Ž�� �� ù��° Row
 * aRow        - [OUT] ã�� Row
 * aRowGRID    - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::fetchHashScanInternalPtr( smiHashTempCursor * aCursor,
                                                sdtHashTRPHdr     * aTRPHeader,
                                                UChar            ** aRow,
                                                scGRID            * aRowGRID )
{
    idBool          sResult = ID_FALSE;
    sdtHashTRPHdr * sPrvTRPHeader = NULL;
    UInt            sLoop;

    if( aCursor->mTTHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aCursor->mTTHeader->mStatistics ) != IDE_SUCCESS);
        aCursor->mTTHeader->mCheckCnt = 0;
    }

    sLoop = 0;

    // ���� Ž��
    do
    {
        sLoop++;
        IDE_DASSERT( SM_IS_FLAG_ON( aTRPHeader->mTRFlag, SDT_TRFLAG_CHILDGRID ) );
        IDE_DASSERT( SM_IS_FLAG_ON( aTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

        if ( aTRPHeader->mHashValue == aCursor->mHashValue )
        {
            IDE_TEST( filteringAndFetch( aCursor,
                                         aTRPHeader,
                                         *aRow,
                                         &sResult )
                      != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                break;
            }
        }
        else
        {
            // HashValue���� �ٸ�
        }

        sPrvTRPHeader = aTRPHeader;
        aTRPHeader    = sPrvTRPHeader->mChildRowPtr ;

    } while( aTRPHeader != NULL );

    if ( sResult == ID_FALSE )
    {
        SC_MAKE_NULL_GRID(*aRowGRID);
        *aRow = NULL;
    }
    else
    {
        IDE_DASSERT( aTRPHeader != NULL );
        IDE_DASSERT( aCursor->mRowPtr != NULL );

        if ( sPrvTRPHeader != NULL )
        {
            SC_MAKE_GRID( *aRowGRID,
                          aCursor->mTTHeader->mSpaceID,
                          sPrvTRPHeader->mChildPageID,
                          sPrvTRPHeader->mChildOffset );
        }
        else
        {
            /* ù�� ° Row�� ������ ��� */
            SC_MAKE_GRID( *aRowGRID,
                          aCursor->mTTHeader->mSpaceID,
                          aCursor->mChildPageID,
                          aCursor->mChildOffset );
        }

        aCursor->mChildPageID = aTRPHeader->mChildPageID;
        aCursor->mChildOffset = aTRPHeader->mChildOffset;

        aCursor->mChildRowPtr = (UChar*)aTRPHeader->mChildRowPtr;
    }

    aCursor->mTTHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aCursor->mTTHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Cursor������ �������� Row�� �޾ƿ´�.
 *               InMemory ���¿��� Row Pointer�� ���󰡸� Ž���Ѵ�.
 *               fetch update �������� ���ȴ�.
 *
 * aCursor  - [IN] ��� Cursor
 * aRow     - [OUT] ã�� Row
 ***************************************************************************/
IDE_RC sdtHashModule::fetchHashUpdate( smiHashTempCursor * aCursor,
                                       UChar            ** aRow,
                                       scGRID            * aRowGRID )
{
    sdtHashTRPHdr * sTRPHeader;
    sdtHashTRPHdr * sPrvTRPHeader;
    sdtWCB        * sWCBPtr = NULL;
    idBool          sResult;
    UInt            sLoop;
    sdtHashSlot   * sHashSlot  = (sdtHashSlot*)aCursor->mHashSlot;

    /* hash slot �� row�� �ݵ�� �ִ�. null pid�� offset�� �ƴϴ�. */
    IDE_ASSERT( SDT_EXIST_SMALL_HASHSET( sHashSlot, aCursor->mHashValue ) );
    IDE_ERROR( ((sdtHashSegHdr*)aCursor->mWASegment)->mOpenCursorType == SMI_HASH_CURSOR_HASH_UPDATE );

    sLoop = 0;

    /* next row ptr�̰ų� single row�� hash value�̴�. */
    sTRPHeader = (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet;

    if (( SDT_IS_NOT_SINGLE_ROW( sHashSlot->mOffset )) &&
        ( sTRPHeader != NULL ))
    {
        sResult = ID_FALSE;
        sPrvTRPHeader = NULL;
        // ���� Ž��
        do
        {
            sLoop++;
            IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_CHILDGRID ) );
            IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

            if ( sTRPHeader->mHashValue == aCursor->mHashValue )
            {
                IDE_TEST( filteringAndFetch( aCursor,
                                             sTRPHeader,
                                             *aRow,
                                             &sResult )
                          != IDE_SUCCESS );

                if ( sResult == ID_TRUE )
                {
                    break;
                }
            }
            else
            {
                // HashValue���� �ٸ�
            }

            sPrvTRPHeader = sTRPHeader;
            sTRPHeader    = sPrvTRPHeader->mChildRowPtr ;

            // Next Offset�� Single Row���� Ȯ��
            if ( SDT_IS_SINGLE_ROW( sPrvTRPHeader->mChildOffset ) )
            {
                break;
            }

        } while( sTRPHeader != NULL );

        /* ã�Ҵ�. */
        if ( sResult == ID_TRUE )
        {
            IDE_DASSERT( aCursor->mRowPtr != NULL );

            aCursor->mChildPageID = sTRPHeader->mChildPageID;
            aCursor->mChildOffset = sTRPHeader->mChildOffset;

            if ( sPrvTRPHeader != NULL )
            {
                SC_MAKE_GRID( *aRowGRID,
                              aCursor->mTTHeader->mSpaceID,
                              sPrvTRPHeader->mChildPageID,
                              sPrvTRPHeader->mChildOffset );
            }
            else
            {
                /* ù��° ptr�� ��� */
                SC_MAKE_GRID( *aRowGRID,
                              aCursor->mTTHeader->mSpaceID,
                              sHashSlot->mPageID,
                              sHashSlot->mOffset );
            }

            if ( ((sdtHashSegHdr*)aCursor->mWASegment)->mNPageHashPtr != NULL )
            {
                /* fetchHashScanInternalPtr �� mWCBPtr�� �������� �ʴ´�.
                 * In Memory������ mWCBPtr�� �������� �ʾƵ� �ȴ�.
                 * update ��� wcb�� �����Ͽ� dirty �� write�� �ϰ� �ȴ�.
                 */
                sWCBPtr = findWCB( (sdtHashSegHdr*)aCursor->mWASegment,
                                   aRowGRID->mPageID );
                IDE_ASSERT( sWCBPtr != NULL );
                /* mWCBPtr�� ������ �Ʒ��ʿ��� �Ѵ�. */
            }

            /* ã������ ������ �۾� ����*/
            IDE_CONT( CONT_COMPLETE );
        }
        else
        {
            /* ã�� �������� sPrvTRPHeader�� �ݵ�� �����Ѵ�.*/
            aCursor->mChildPageID = sPrvTRPHeader->mChildPageID;
            aCursor->mChildOffset = sPrvTRPHeader->mChildOffset;
        }
    }
    else
    {
        /* single row�� Ȯ���ؾ� �ϴ� ��� �̰ų�
         * sub hash���� ������ �ϴ� ��� */
        aCursor->mChildPageID = sHashSlot->mPageID;
        aCursor->mChildOffset = sHashSlot->mOffset;
    }

    if ( aCursor->mChildPageID != SD_NULL_PID )
    {
        if ( SDT_IS_SINGLE_ROW( aCursor->mChildOffset ) )
        {
            if ( (UInt)((ULong)sTRPHeader) == aCursor->mHashValue )
            {
                /* Single Row �ϳ� �ִ� ��� */
                aCursor->mChildOffset = SDT_DEL_SINGLE_FLAG_IN_OFFSET( aCursor->mChildOffset );

                return fetchHashScanInternalGRID( aCursor,
                                                  aRow,
                                                  aRowGRID );
            }
            else
            {
                /* single row������ mismatch
                 * HashSlot���� �ٷ� ���� ����� HashValue�� �ٸ� �� ����.
                 * checkHashSlotUpdate���� �̹� Ȯ���߱� �����̴�.
                 * ������, Row pointer list�� ������ Row�� Next�� �������� �ִ�.
                 * �� ��쿡�� HashValue�� Ȯ���ؾ� �Ѵ�.*/
                aCursor->mChildPageID = SD_NULL_PID;
                aCursor->mChildOffset = 0;
                SC_MAKE_NULL_GRID(*aRowGRID);
                *aRow = NULL;
            }
        }
        else
        {
            aCursor->mSubHashPtr = NULL;
            aCursor->mSubHashIdx = 0;

            return fetchHashScanInternalSubHash( aCursor,
                                                 aRow,
                                                 aRowGRID );
        }
    }
    else
    {
        /* ã�� ���ߴ�. */
        SC_MAKE_NULL_GRID(*aRowGRID);
        *aRow = NULL;
    }

    IDE_EXCEPTION_CONT( CONT_COMPLETE );

    aCursor->mWCBPtr = sWCBPtr;

    aCursor->mTTHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aCursor->mTTHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� ��� Row�� ������� FetchNext�� �����ɴϴ�.
 *
 * aCursor  - [IN] ��� Cursor
 * aRow     - [OUT] ��� Row
 ***************************************************************************/
IDE_RC sdtHashModule::fetchFullNext( smiHashTempCursor * aCursor,
                                     UChar            ** aRow,
                                     scGRID            * aRowGRID )
{
    sdtHashPageHdr * sPageHdr;
    scOffset         sCurOffset;
    scOffset         sNxtOffset;
    sdtHashTRPHdr  * sTRPHeaderPtr;
    idBool           sResult      = ID_FALSE;
    idBool           sIsFixedPage = ID_FALSE;

    IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FULLSCAN ) );
    IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FULLSCAN ) );
    IDE_DASSERT( ((sdtHashSegHdr*)aCursor->mWASegment)->mOpenCursorType == SMI_HASH_CURSOR_FULL_SCAN );

    if( aCursor->mTTHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aCursor->mTTHeader->mStatistics ) != IDE_SUCCESS);
        aCursor->mTTHeader->mCheckCnt = 0;
    }

    while( aCursor->mWAPagePtr != NULL )
    {
        sPageHdr = (sdtHashPageHdr*)aCursor->mWAPagePtr;

        IDE_DASSERT_MSG((sPageHdr->mSelfNPID == aCursor->mChildPageID ),
                        "%d != %d\n",
                        sPageHdr->mSelfNPID ,
                        aCursor->mChildPageID);

        IDE_ASSERT( sPageHdr->mFreeOffset <= SD_PAGE_SIZE );

        /* Page���� Slot���� Ž���� */
        for( sCurOffset = aCursor->mChildOffset ;
             sCurOffset < sPageHdr->mFreeOffset ;
             sCurOffset = sNxtOffset )
        {
            IDE_DASSERT( sCurOffset + ID_SIZEOF(sdtHashTRPHdr) < SD_PAGE_SIZE );
            sTRPHeaderPtr = (sdtHashTRPHdr*)((UChar*)sPageHdr + sCurOffset );

            IDE_DASSERT( sTRPHeaderPtr->mValueLength < SD_PAGE_SIZE );
            IDE_DASSERT( sTRPHeaderPtr->mIsRow == SDT_HASH_ROW_HEADER );

            sNxtOffset = idlOS::align8( sCurOffset +
                                        ID_SIZEOF( sdtHashTRPHdr ) +
                                        sTRPHeaderPtr->mValueLength );

            if ( SM_IS_FLAG_OFF( sTRPHeaderPtr->mTRFlag, ( SDT_TRFLAG_HEAD | SDT_TRFLAG_CHILDGRID ) ) )
            {
                /* Header Row Piece�� �ƴϸ� �����Ѵ�. */
                continue;
            }

            if ( aCursor->mIsInMemory == SDT_WORKAREA_OUT_MEMORY )
            {
                sIsFixedPage = ID_TRUE;
                fixWAPage( (sdtWCB*)(aCursor->mWCBPtr) );
            }

            /* aCursor->mGRID �� �����ϸ� �ȴ�.
             * mChileGRID, mChileRowPtr �� �ʿ����*/
            IDE_TEST( filteringAndFetch( aCursor,
                                         sTRPHeaderPtr,
                                         *aRow,
                                         &sResult )
                      != IDE_SUCCESS );

            if ( sIsFixedPage == ID_TRUE )
            {
                sIsFixedPage = ID_FALSE;
                unfixWAPage( (sdtWCB*)(aCursor->mWCBPtr) );
            }

            if ( sResult == ID_TRUE )
            {
                /* ã���� �ٷ� return�Ѵ�. */
                aCursor->mChildOffset = sNxtOffset ;

                SC_MAKE_GRID( *aRowGRID,
                              aCursor->mTTHeader->mSpaceID,
                              aCursor->mChildPageID,
                              sCurOffset );

                IDE_CONT( SKIP );
            }
        }

        /* Next Page �� �����´�. */
        if ( aCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
        {
            /* InMemory�����̴�. WCB Array�� �������� Next Page�� �����´�.*/
            aCursor->mWCBPtr = ((sdtWCB*)aCursor->mWCBPtr)+ 1;

            if ( aCursor->mWCBPtr < aCursor->mEndWCBPtr )
            {
                aCursor->mWAPagePtr   = ((sdtWCB*)aCursor->mWCBPtr)->mWAPagePtr;
                aCursor->mChildPageID = ((sdtWCB*)aCursor->mWCBPtr)->mNPageID;
                aCursor->mChildOffset = ID_SIZEOF(sdtHashPageHdr);
            }
            else
            {
                /* ������ page���� �о���. */
                aCursor->mWCBPtr    = NULL;
                aCursor->mWAPagePtr = NULL;
            }
        }
        else
        {
            /* Out Memory�����̴�. Normal Extent�� �������� Next Page�� �о�´�.*/
            IDE_TEST( getNextNPageForFullScan( aCursor ) != IDE_SUCCESS );
        }
    }

    /*���̻� next�� ���� ����̴�. ��� ���� �� �ش�. */
    aCursor->mChildPageID = SM_NULL_PID ;
    aCursor->mChildOffset = 0 ;
    aCursor->mRowPtr      = NULL;
    aCursor->mSeq         = aCursor->mHashSlotCount;

    SC_MAKE_NULL_GRID(*aRowGRID);
    *aRow = NULL;

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : GRID�� �������� row�� �о�´�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aGRID        - [IN] �о���� Row�� GRID
 * aDestRowBuf  - [IN] �ɰ��� Column�� �����ϱ� ���� Buffer
 ***************************************************************************/
IDE_RC sdtHashModule::fetchFromGRID( smiTempTableHeader * aHeader,
                                     scGRID               aGRID,
                                     void               * aDestRowBuf )
{
    sdtHashSegHdr      * sWASeg  = (sdtHashSegHdr*)aHeader->mWASegment;
    sdtHashScanInfo      sTRPInfo;
    UChar              * sPtr;
    sdtWCB             * sWCBPtr;
    idBool               sIsFixedPage = ID_FALSE;

    IDE_DASSERT( ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WAMAP ) &&
                 ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WORKAREA ) );

    if ( sWASeg->mNPageHashPtr == NULL )
    {
        /* GRID�� �������� Ž���ϹǷ� Hash Map�� �ʿ��ϴ�.
         * ������ �����. */
        IDE_TEST( sdtHashModule::buildNPageHashMap( sWASeg ) != IDE_SUCCESS );
    }
    IDE_TEST( sdtHashModule::getSlotPtr( sWASeg,
                                         &sWASeg->mFetchGrp,
                                         aGRID.mPageID,
                                         aGRID.mOffset,
                                         &sPtr,
                                         &sWCBPtr )
              != IDE_SUCCESS );

    IDE_DASSERT( SM_IS_FLAG_ON( ((sdtHashTRPHdr*)sPtr)->mTRFlag, SDT_TRFLAG_HEAD ) );

    sTRPInfo.mTRPHeader      = (sdtHashTRPHdr*)sPtr;
    sTRPInfo.mFetchEndOffset = aHeader->mRowSize;

    if ( sWASeg->mIsInMemory == SDT_WORKAREA_OUT_MEMORY )
    {
        sdtHashModule::fixWAPage( sWCBPtr );
        sIsFixedPage = ID_TRUE;
    }

    IDE_TEST( sdtHashModule::fetch( sWASeg,
                                    (UChar*)aDestRowBuf,
                                    &sTRPInfo )
              != IDE_SUCCESS );

    if ( sIsFixedPage == ID_TRUE )
    {
        sIsFixedPage = ID_FALSE;
        sdtHashModule::unfixWAPage( sWCBPtr );
    }

    if ( aDestRowBuf != sTRPInfo.mValuePtr )
    {
        IDE_DASSERT( SM_IS_FLAG_OFF( ((sdtHashTRPHdr*)sPtr)->mTRFlag, SDT_TRFLAG_NEXTGRID ) );

        idlOS::memcpy( aDestRowBuf,
                       sTRPInfo.mValuePtr,
                       sTRPInfo.mValueLength );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        sdtHashModule::unfixWAPage( sWCBPtr );
    }

    smiTempTable::checkAndDump( aHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * page�� �����ϴ� Row ��ü�� rowInfo���·� �д´�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aRowBuffer   - [IN] �ɰ��� Column�� �����ϱ� ���� Buffer
 * aTRPInfo     - [OUT] Fetch�� ���
 ***************************************************************************/
IDE_RC sdtHashModule::fetch( sdtHashSegHdr    * aWASegment,
                             UChar            * aRowBuffer,
                             sdtHashScanInfo  * aTRPInfo )
{
    UChar   sFlag = aTRPInfo->mTRPHeader->mTRFlag;

    IDE_DASSERT( SM_IS_FLAG_ON( sFlag, SDT_TRFLAG_HEAD ) );

    if ( SM_IS_FLAG_OFF( sFlag, SDT_TRFLAG_NEXTGRID ) )
    {
        aTRPInfo->mValuePtr    = ((UChar*)aTRPInfo->mTRPHeader) + SDT_HASH_TR_HEADER_SIZE_FULL ;
        aTRPInfo->mValueLength = aTRPInfo->mTRPHeader->mValueLength;

        return IDE_SUCCESS;
    }
    else
    {
        return fetchChainedRowPiece( aWASegment,
                                     aRowBuffer,
                                     aTRPInfo );
    }
}

/**************************************************************************
 * Description :
 * Row�� Ư�� Column�� �����Ѵ�.
 * Update�� Row�� ó�� �����Ҷ����� ����� ������ Ȯ���߱� ������ �ݵ��
 * Row�� ���� ���� ���� �����Ѵ�
 *
 * aTempCursor    - [IN] ��� Ŀ��
 * aValue         - [IN] ������ Value
 ***************************************************************************/
IDE_RC sdtHashModule::update( smiHashTempCursor * aTempCursor,
                              smiValue          * aValueList )
{
    const smiColumnList * sUpdateColumn;
    const smiColumn    * sColumn;
    UChar              * sRowPos;
    sdtWCB             * sWCBPtr;
    sdtHashTRPHdr      * sTRPHeader;

    sTRPHeader = (sdtHashTRPHdr*)aTempCursor->mRowPtr;

    IDE_ERROR( sTRPHeader != NULL );
    IDE_ERROR( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

    /* full scan�̰ų�, group by�� insert with scan�̰ų�,
     * �ٸ� ��찡 �߰��Ǹ�, write n page �� ���ؼ� ����ؾ� �Ѵ�.*/

    if ( sTRPHeader->mValueLength >= aTempCursor->mUpdateEndOffset )
    {
        sRowPos = aTempCursor->mRowPtr + SDT_HASH_TR_HEADER_SIZE_FULL ;
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

        /* 1. in memory������ mWCBPtr�� null�� �´�, out memory�̰ų�,
         *    insert with scan ��Ȳ���� ������ extent���� Ȯ�� �� ��쿡
         *    mWCBPtr�� ������ �Ǿ ����, �� �� ���� �ݵ�� dirty ����� �Ѵ�.
         * 2. full scan������ dirty�� �Ѵ�,
         *    ������� �����ϹǷ�, ���߿� extent������ ��Ƽ� Write �Ѵ�.
         * */
        if ( aTempCursor->mWCBPtr != NULL )
        {
            sWCBPtr = (sdtWCB*)aTempCursor->mWCBPtr;

            IDE_DASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

            sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
        }
        else
        {
            IDE_DASSERT( aTempCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY );
        }
    }
    else
    {
        IDE_TEST( updateChainedRowPiece( aTempCursor,
                                         aValueList ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aTempCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * HitFlag�� �����մϴ�.
 *
 * aCursor      - [IN] ������ Row�� ����Ű�� Ŀ��
 * aHitSeq      - [IN] ������ Hit��
 ***************************************************************************/
void sdtHashModule::setHitFlag( smiHashTempCursor * aCursor )
{
    sdtHashTRPHdr * sTRPHeader = (sdtHashTRPHdr*)aCursor->mRowPtr;
    UInt            sHitSeq    = aCursor->mTTHeader->mHitSequence;
    sdtWCB        * sWCBPtr;

    IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );
    IDE_DASSERT( ID_SIZEOF( sTRPHeader->mHitSequence ) == ID_SIZEOF( sHitSeq ) );

    if ( sTRPHeader->mHitSequence != sHitSeq )
    {
        sTRPHeader->mHitSequence = sHitSeq;

        if ( aCursor->mWCBPtr != NULL )
        {
            sWCBPtr = (sdtWCB*)aCursor->mWCBPtr;

            IDE_DASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

            sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
        }
        else
        {
            IDE_ASSERT( aCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY );
        }
    }
    else
    {
        /* ( sTRPHeader->mHitSequence == aHitSeq )
         * ���� ������ ó�� �� �ʿ����. */
    }
    return;
}

/**************************************************************************
 * Description : HitFlag�� �˻��ϰ� �� ����� Return�Ѵ�.
 *
 * aCursor  - [IN] Ȯ�� �� ���� Row�� ��ġ�� ����Ű�� Cursor
 ***************************************************************************/
idBool sdtHashModule::isHitFlagged( smiHashTempCursor * aCursor )
{
    IDE_DASSERT( ID_SIZEOF( ((sdtHashTRPHdr*)aCursor->mRowPtr)->mHitSequence ) ==
                 ID_SIZEOF( aCursor->mTTHeader->mHitSequence ) );

    if ( ((sdtHashTRPHdr*)aCursor->mRowPtr)->mHitSequence ==
         aCursor->mTTHeader->mHitSequence )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

#endif /* _O_SDT_HASH_MODULE_H_ */
