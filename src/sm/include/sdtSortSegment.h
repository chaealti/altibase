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
 * $Id
 **********************************************************************/

#ifndef _O_SDT_SORT_SEGMENT_H_
#define _O_SDT_SORT_SEGMENT_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdtDef.h>
#include <smuMemStack.h>
#include <smErrorCode.h>
#include <sdtTempPage.h>
#include <sdtWAExtentMgr.h>
#include <sctTableSpaceMgr.h>

class sdtSortSegment
{
public:

    static IDE_RC initializeStatic();
    static void   destroyStatic();
    static IDE_RC resetWASegmentSize( ULong aNewWorkAreaSize );
    static IDE_RC resizeWASegmentPool( UInt aNewWASegmentCount );

    static void lock()
    {
        (void)mMutex.lock( NULL );
    }
    static void unlock()
    {
        (void)mMutex.unlock();
    }

    static UInt getWAExtentCount()
    {
        return calcWAExtentCount( smuProperty::getSortAreaSize() ) ;
    }
    static UInt calcWAExtentCount( ULong aSortAreaSize )
    {
        return (UInt)(( aSortAreaSize + SDT_WAEXTENT_SIZE - 1 ) / SDT_WAEXTENT_SIZE);
    }
    static ULong getWAPageCount()
    {
        return calcWAPageCount( smuProperty::getSortAreaSize() ) ;
    }

    static ULong calcWAPageCount( ULong aSortAreaSize )
    {
        return (ULong)calcWAExtentCount( aSortAreaSize) * SDT_WAEXTENT_PAGECOUNT ;
    }
    static ULong getWASegmentSize()
    {
        return calcWASegmentSize( getWAPageCount() );
    }
    static ULong calcWASegmentSize( ULong aWAPageCount )
    {
        return SDT_SORT_SEGMENT_HEADER_SIZE + ( aWAPageCount * ID_SIZEOF(sdtWCB) );
    }

    static ULong getNPageHashMapPoolSize()
    {
        return (ULong)mNHashMapPool.getNodeSize() * mNHashMapPool.getNodeCount();
    };

    static ULong getSegPoolSize()
    {
        return (ULong)mWASegmentPool.getNodeSize() * mWASegmentPool.getNodeCount();
    };

    /******************************************************************
     * Segment
     ******************************************************************/
    static IDE_RC createSortSegment( smiTempTableHeader * aHeader,
                                     ULong                aInitWorkAreaSize );

    static UInt getMaxWAPageCount(sdtSortSegHdr* aWASegment )
    {
        return aWASegment->mMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT;
    };

    static UInt getWASegmentUsedPageCount(sdtSortSegHdr* aWASegment )
    {
        return (( aWASegment->mAllocWAPageCount + SDT_WAEXTENT_PAGECOUNT - 1 )
                / SDT_WAEXTENT_PAGECOUNT ) * SDT_WAEXTENT_PAGECOUNT;
    };

    static IDE_RC dropSortSegment(sdtSortSegHdr* aWASegment );
    static IDE_RC clearSortSegment(sdtSortSegHdr* aWASegment );
    static IDE_RC truncateSortSegment( sdtSortSegHdr* aWASegment );

    static IDE_RC expandFreeWAExtent( sdtSortSegHdr* aWASeg );
    static void   clearAllWCB( sdtSortSegHdr* aWASegment );

    /******************************************************************
     * Group
     ******************************************************************/
    static IDE_RC createWAGroup( sdtSortSegHdr    * aWASegment,
                                 sdtGroupID         aWAGroupID,
                                 UInt               aPageCount,
                                 sdtWAReusePolicy   aPolicy );
    static IDE_RC resetWAGroup( sdtSortSegHdr     * aWASegment,
                                sdtGroupID          aWAGroupID,
                                idBool              aWait4Flush );
    static IDE_RC clearWAGroup( sdtSortGroup      * aGrpInfo );

    inline static void initWCB( sdtWCB       * aWCBPtr );

    /************ Hint ó�� ***************/
    inline static sdtWCB * getHintWCB( sdtSortGroup * aGrpInfo );
    inline static void setHintWCB( sdtSortGroup * aGroupInfo,
                                   sdtWCB       * aHintWCBPtr );
    inline static sdtSortGroup *getWAGroupInfo( sdtSortSegHdr* aWASegment,
                                                sdtGroupID     aWAGroupID );

    inline static UInt   getWAGroupPageCount( sdtSortGroup * aGrpInfo )
    {
        return aGrpInfo->mEndWPID - aGrpInfo->mBeginWPID;
    };

    inline static UInt   getAllocableWAGroupPageCount( sdtSortSegHdr* aWASegment,
                                                       sdtGroupID     aWAGroupID );
    static UInt   getAllocableWAGroupPageCount( sdtSortGroup   * aGrpInfo );

    inline static scPageID getFirstWPageInWAGroup( sdtSortSegHdr* aWASegment,
                                                   sdtGroupID     aWAGroupID )
    {
        return aWASegment->mGroup[ aWAGroupID ].mBeginWPID;
    };

    static scPageID getLastWPageInWAGroup( sdtSortSegHdr* aWASegment,
                                           sdtGroupID     aWAGroupID )
    {
        return aWASegment->mGroup[ aWAGroupID ].mEndWPID;
    };

    static IDE_RC getFreeWAPageFIFO( sdtSortSegHdr* aWASegment,
                                     sdtSortGroup * aGrpInfo,
                                     sdtWCB      ** aWCBPtr );
    static IDE_RC getFreeWAPageINMEMORY( sdtSortSegHdr* aWASegment,
                                         sdtSortGroup * aGrpInfo,
                                         sdtWCB      ** aWCBPtr );
    static IDE_RC getFreeWAPageLRU( sdtSortSegHdr* aWASegment,
                                    sdtSortGroup * aGrpInfo,
                                    sdtWCB      ** aWCBPtr );
    static IDE_RC getFreeWAPageLRUfromDisk( sdtSortSegHdr* aWASegment,
                                            sdtSortGroup * aGrpInfo,
                                            sdtWCB      ** aWCBPtr );
    static IDE_RC mergeWAGroup(sdtSortSegHdr    * aWASegment,
                               sdtGroupID         aWAGroupID1,
                               sdtGroupID         aWAGroupID2,
                               sdtWAReusePolicy   aPolicy );
    static IDE_RC splitWAGroup(sdtSortSegHdr    * aWASegment,
                               sdtGroupID         aSrcWAGroupID,
                               sdtGroupID         aDstWAGroupID,
                               sdtWAReusePolicy   aPolicy );

    static IDE_RC dropAllWAGroup( sdtSortSegHdr* aWASegment );
    static IDE_RC dropWAGroup(sdtSortSegHdr* aWASegment,
                              sdtGroupID     aWAGroupID,
                              idBool         aWait4Flush);

    /******************************************************************
     * Page �ּ� ���� Operation
     ******************************************************************/
    inline static void convertFromWGRIDToNGRID( sdtSortSegHdr* aWASegment,
                                                scGRID         aWGRID,
                                                scGRID       * aNGRID );
    inline static IDE_RC getWCBByNPID( sdtSortSegHdr* aWASegment,
                                       sdtGroupID     aWAGroupID,
                                       scPageID       aNPID,
                                       sdtWCB      ** aRetWCB );
    inline static IDE_RC getWCBByGRID( sdtSortSegHdr* aWASegment,
                                       sdtGroupID     aWAGroupID,
                                       scGRID         aGRID,
                                       sdtWCB      ** aWCBPtr );

    /******************************************************************
     * Page ���� ���� Operation
     ******************************************************************/
    /*************************  DirtyPage  ****************************/
    static IDE_RC makeInitPage( sdtSortSegHdr* aWASegment,
                                sdtWCB       * aWCBPtr,
                                idBool         aWait4Flush);
    inline static void bookedFree(sdtSortSegHdr* aWASegment,
                                  scPageID       aWPID);

    /*************************  Fix Page  ****************************/
    inline static void fixWAPage(sdtSortSegHdr* aWASegment,
                                 scPageID       aWPID);
    inline static void unfixWAPage(sdtSortSegHdr* aWASeg,
                                   scPageID       aWPID);
    inline static void fixWAPage(sdtWCB       * aWCBPtr );
    inline static void unfixWAPage(sdtWCB     * aWCBPtr );

    inline static idBool isFixedPage(sdtWCB         * aWCBPtr );
    /******************************************************************
     * Page�������� Operation
     ******************************************************************/
    inline static IDE_RC getPagePtrByGRID( sdtSortSegHdr * aWASegment,
                                           sdtGroupID      aGroupID,
                                           scGRID          aGRID,
                                           UChar        ** aNSlotPtr );

    inline static IDE_RC getPageWithFix( sdtSortSegHdr * aWASegment,
                                         sdtGroupID      aGroupID,
                                         scGRID          aGRID,
                                         UInt          * aWPID,
                                         UChar        ** aWAPagePtr,
                                         UInt          * aSlotCount );
    inline static IDE_RC getPage( sdtSortSegHdr * aWASegment,
                                  sdtGroupID      aGroupID,
                                  scGRID          aGRID,
                                  UInt          * aWPID,
                                  UChar        ** aWAPagePtr );
    inline static void getSlot( UChar         * aWAPagePtr,
                                UInt            aSlotCount,
                                UInt            aSlotNo,
                                UChar        ** aNSlotPtr,
                                idBool        * aIsValid );

    inline static sdtWCB * getWCBInternal(sdtSortSegHdr * aWASegment,
                                          scPageID        aWPID );
    inline static sdtWCB * getWCBWithLnk(sdtSortSegHdr * aWASegment,
                                         scPageID        aWPID );
    inline static UChar*  getWAPagePtr( sdtSortSegHdr * aWASegment,
                                        sdtGroupID      aGroupID,
                                        sdtWCB        * aWCBPtr );
    inline static UChar*  getWAPagePtr( sdtWCB        * aWCBPtr );

    inline static  IDE_RC chkAndSetWAPagePtr( sdtSortSegHdr * aWASegment,
                                              sdtWCB        * aWCBPtr );
    static  IDE_RC setWAPagePtr( sdtSortSegHdr * aWASegment,
                                 sdtWCB        * aWCBPtr );

    inline static scPageID  getNPID( sdtSortSegHdr* aWASegment,
                                     scPageID       aWPID )
    {
        return getNPID( getWCBInternal( aWASegment, aWPID ) );
    };

    inline static scPageID  getNPID( sdtWCB * aWCBPtr )
    {
        return aWCBPtr->mNPageID;
    }

    static scPageID  getWPageID( sdtSortSegHdr* aWASegment,
                                 sdtWCB       * aWCBPtr )
    {
        return (scPageID)( aWCBPtr - aWASegment->mWCBMap );
    }

    /*************************************************
     * NormalPage Operation
     *************************************************/
    static IDE_RC assignNPage(sdtSortSegHdr* aWASegment,
                              sdtWCB       * aWCBPtr,
                              scPageID       aNPageID);
    static IDE_RC unassignNPage(sdtSortSegHdr* aWASegment,
                                sdtWCB       * aWCBPtr );
    static IDE_RC moveWAPage( sdtSortSegHdr* aWASegment,
                              sdtWCB       * aSrcWCBPtr,
                              sdtWCB       * aDstWCBPtr );

    inline static UInt getNPageHashValue( sdtSortSegHdr* aWASegment,
                                          scPageID       aNPID);
    inline static sdtWCB * findWCB( sdtSortSegHdr* aWASegment,
                                    scPageID       aNPID);
    static IDE_RC readNPageFromDisk(sdtSortSegHdr* aWASegment,
                                    sdtGroupID     aWAGroupID,
                                    sdtWCB       * aWCBPtr,
                                    scPageID       aNPageID);
    static IDE_RC readNPage(sdtSortSegHdr* aWASegment,
                            sdtGroupID     aWAGroupID,
                            sdtWCB       * aWCBPtr,
                            scPageID       aNPageID);
    static IDE_RC writeNPage( sdtSortSegHdr* aWASegment,
                              sdtWCB       * aWCBPtr );
    static IDE_RC pushFreePage(sdtSortSegHdr* aWASegment,
                               scPageID       aNPID);
    static IDE_RC allocAndAssignNPage(sdtSortSegHdr* aWASegment,
                                      sdtWCB       * aTargetWCBPtr );

    static UInt   getNExtentCount( sdtSortSegHdr* aWASegment )
    {
        return aWASegment->mNExtFstPIDList.mCount;
    }

    static idBool   isWorkAreaTbsID( scSpaceID aSpaceID )
    {
        return ( aSpaceID == SDT_SPACEID_WORKAREA ) ? ID_TRUE : ID_FALSE ;
    }
public:

    static void moveLRUListToTopInternal( sdtSortSegHdr* aWASegment,
                                          sdtSortGroup * aGrpInfo,
                                          sdtWCB       * aWCBPtr );

private:
    inline static void moveLRUListToTop( sdtSortSegHdr* aWASegment,
                                         sdtGroupID     aWAGroupID,
                                         sdtWCB       * aWCBPtr );

#ifdef DEBUG
    static IDE_RC validateLRUList( sdtSortSegHdr* aWASegment,
                                   sdtSortGroup * aGrpInfo );
#endif
public:
    static sdtGroupID   findGroup( sdtSortSegHdr* aWASegment,
                                   scPageID       aWPageID );

    /***********************************************************
     * Dump�� �Լ���
     ***********************************************************/
    static sdtWAReusePolicy getReusePolicy( sdtSortSegHdr* aWASegment,
                                            scPageID       aWAPageID );
    static void exportSortSegmentToFile( sdtSortSegHdr* aWASegment );
    static IDE_RC importSortSegmentFromFile( SChar         * aFileName,
                                             sdtSortSegHdr** aSortSegHdr,
                                             sdtSortSegHdr * aRawSortSegHdr,
                                             UChar        ** aPtr );
    static IDE_RC resetWCBInfo4Dump( sdtSortSegHdr * aWASegment,
                                     UChar         * aOldWASegPtr,
                                     UInt            aMaxPageCount,
                                     UChar        ** aPtr );
    static void dumpWASegment( void           * aWASegment,
                               SChar          * aOutBuf,
                               UInt             aOutSize );
    static void dumpWAGroup( sdtSortSegHdr  * aWASegment,
                             sdtGroupID       aWAGroupID ,
                             SChar          * aOutBuf,
                             UInt             aOutSize );
    
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
} ;


/**************************************************************************
 * Description :
 *      HintPage������
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
sdtWCB * sdtSortSegment::getHintWCB( sdtSortGroup * aGrpInfo )
{
    return aGrpInfo->mHintWCBPtr;
}

/**************************************************************************
 * Description :
 *      HintPage�� ������.
 *      HintPage�� unfix�Ǹ� �ȵǱ⿡ �������ѵ�
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
void sdtSortSegment::setHintWCB( sdtSortGroup * aGroupInfo,
                                 sdtWCB       * aHintWCBPtr )
{
    if ( aGroupInfo->mHintWCBPtr != aHintWCBPtr )
    {
        if ( aGroupInfo->mHintWCBPtr != NULL )
        {
            unfixWAPage( aGroupInfo->mHintWCBPtr );
        }

        aGroupInfo->mHintWCBPtr = aHintWCBPtr;

        if ( aHintWCBPtr != NULL )
        {
            fixWAPage( aHintWCBPtr );
        }
    }
}

/**************************************************************************
 * Description :
 * GroupInfo�� ������ �����´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
sdtSortGroup *sdtSortSegment::getWAGroupInfo( sdtSortSegHdr* aWASegment,
                                              sdtGroupID     aWAGroupID )
{
    return &aWASegment->mGroup[ aWAGroupID ];
}

/**************************************************************************
 * Description :
 *      ���߿� unassign�ɶ� Free�ǵ���, ǥ���صд�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - WAPageID
 ***************************************************************************/
void sdtSortSegment::bookedFree( sdtSortSegHdr* aWASegment,
                                 scPageID       aWPID )
{
    sdtWCB   * sWCBPtr;

    sWCBPtr = getWCBInternal( aWASegment, aWPID );

    if ( sWCBPtr->mNPageID == SC_NULL_PID )
    {
        /* �Ҵ� �Ǿ����� ������, �����ص� �ȴ�. */
    }
    else
    {
        sWCBPtr->mBookedFree = ID_TRUE;
    }
}

/**************************************************************************
 * Description :
 *      �ش� WAPage�� discard���� �ʵ��� �����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - WAPageID
 ***************************************************************************/
void sdtSortSegment::fixWAPage( sdtSortSegHdr* aWASegment,
                                scPageID       aWPID)
{
    sdtWCB   * sWCBPtr;

    sWCBPtr = getWCBInternal( aWASegment, aWPID );

    aWASegment->mHintWCBPtr = sWCBPtr;

    fixWAPage( sWCBPtr );
}

void sdtSortSegment::fixWAPage( sdtWCB       * aWCBPtr )
{
    aWCBPtr->mFix++;

    IDE_DASSERT( aWCBPtr->mNxtUsedWCBPtr != NULL );

    IDE_ASSERT( aWCBPtr->mFix > 0 );
}

/**************************************************************************
 * Description :
 *      FixPage�� Ǯ�� Discard�� ����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - WAPageID
 ***************************************************************************/
void sdtSortSegment::unfixWAPage( sdtSortSegHdr* aWASeg,
                                  scPageID       aWPID)
{
    sdtWCB   * sWCBPtr;

    sWCBPtr = getWCBInternal( aWASeg, aWPID );

    unfixWAPage( sWCBPtr );
}

void sdtSortSegment::unfixWAPage( sdtWCB       * aWCBPtr )
{
    IDE_ASSERT( aWCBPtr->mFix > 0 );

    aWCBPtr->mFix--;
}

/**************************************************************************
 * Description :
 *     Page �ּҷ� HashValue����
 *
 * <IN>
 * aNSpaceID,aNPID- Page�ּ�
 ***************************************************************************/
UInt sdtSortSegment::getNPageHashValue( sdtSortSegHdr* aWASegment,
                                        scPageID       aNPID )
{
    return  ( aNPID ) % aWASegment->mNPageHashBucketCnt;
}

/**************************************************************************
 * Description :
 *     NPageHash�� ������ �ش� NPage�� ���� WPage�� ã�Ƴ�
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aNPID          - Page�ּ�
 ***************************************************************************/
sdtWCB * sdtSortSegment::findWCB( sdtSortSegHdr* aWASegment,
                                  scPageID       aNPID )
{
    UInt       sHashValue = getNPageHashValue( aWASegment, aNPID );
    sdtWCB   * sWCBPtr;

    sWCBPtr = aWASegment->mNPageHashPtr[ sHashValue ];

    while( sWCBPtr != NULL )
    {
        /* HashValue�� �¾ƾ� �� */
        IDE_DASSERT( getNPageHashValue( aWASegment, sWCBPtr->mNPageID )
                     == sHashValue );

        if ( sWCBPtr->mNPageID == aNPID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        sWCBPtr  = sWCBPtr->mNextWCB4Hash;
    }

    return sWCBPtr;
}

/**************************************************************************
 * Description :
 *     WA������ GRID�� NGRID�� ������
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWGRID         - ������ ���� WGRID
 * <OUT>
 * aNGRID         - ���� ��
 ***************************************************************************/
void sdtSortSegment::convertFromWGRIDToNGRID( sdtSortSegHdr* aWASegment,
                                              scGRID         aWGRID,
                                              scGRID       * aNGRID )
{
    sdtWCB         * sWCBPtr;

    if ( SC_MAKE_SPACE( aWGRID ) == SDT_SPACEID_WORKAREA )
    {
        /* WGRID���� �ϰ�, InMemoryGroup�ƴϾ�� �� */
        sWCBPtr = getWCBInternal( aWASegment,
                                  SC_MAKE_PID( aWGRID ) );

        /* NPID�� Assign �Ǿ�߸� �ǹ� ���� */
        if ( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT )
        {
            SC_MAKE_GRID( *aNGRID,
                          aWASegment->mSpaceID,
                          sWCBPtr->mNPageID,
                          SC_MAKE_OFFSET( aWGRID ) );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }
}

/**************************************************************************
 * Description :
 *     WASegment������ �ش� NPage�� ã�´�.
 *     �������� ������, Group���� VictimPage�� ã�Ƽ� �о �����ش�.
 *     �׷��� ã�Ƽ� WCB�� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aNSpaceID,aNPID- Page�ּ�
 * <OUT>
 * aRetWCB        - ���� ��
 ***************************************************************************/
IDE_RC sdtSortSegment::getWCBByNPID(sdtSortSegHdr* aWASegment,
                                    sdtGroupID     aWAGroupID,
                                    scPageID       aNPID,
                                    sdtWCB      ** aRetWCB )
{
    sdtWCB      * sWCBPtr = NULL;
    sdtSortGroup* sGrpInfo;

    if ( ( aWASegment->mHintWCBPtr->mNPageID == aNPID ) )
    {
        (*aRetWCB) = aWASegment->mHintWCBPtr;
        IDE_ASSERT( aWASegment->mHintWCBPtr->mWAPagePtr != NULL );
    }
    else
    {
        (*aRetWCB) = findWCB( aWASegment, aNPID );

        if ( (*aRetWCB) == NULL )
        {
            /* WA�� �������� �������� ���� ���, �� �������� Group�� �Ҵ�޾�
             * Read��*/
            IDE_ASSERT ( aWAGroupID != SDT_WAGROUPID_NONE );

            sGrpInfo  = getWAGroupInfo( aWASegment, aWAGroupID );

            switch( sGrpInfo->mPolicy )
            {
                case SDT_WA_REUSE_FIFO:
                    IDE_TEST( getFreeWAPageFIFO( aWASegment,
                                                 sGrpInfo,
                                                 &sWCBPtr )
                              != IDE_SUCCESS );
                    break;
                case SDT_WA_REUSE_LRU:
                    IDE_TEST( getFreeWAPageLRU( aWASegment,
                                                sGrpInfo,
                                                &sWCBPtr )
                              != IDE_SUCCESS );
                    break;
                default:
                    IDE_ASSERT(0);
                    break;
            }

            IDE_TEST( readNPage( aWASegment,
                                 aWAGroupID,
                                 sWCBPtr,
                                 aNPID )
                      != IDE_SUCCESS );
            (*aRetWCB) = sWCBPtr;
        }
        else
        {
            /* nothing to do */
            IDE_ASSERT( (*aRetWCB)->mWAPagePtr != NULL );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_DUMP_0,
                 "aWAGroupID : %u\n"
                 "aNSpaceID  : %u\n"
                 "aNPID      : %u\n"
                 "sWPID      : %u\n",
                 aWAGroupID,
                 aWASegment->mSpaceID,
                 aNPID,
                 (sWCBPtr != NULL) ? getWPageID( aWASegment, sWCBPtr ) : SD_NULL_PID );

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * GRID�� �������� WPID�� �����´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aGRID          - ������ ���� GRID
 * <OUT>
 * aRetPID        - ��� WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::getWCBByGRID( sdtSortSegHdr* aWASegment,
                                     sdtGroupID     aWAGroupID,
                                     scGRID         aGRID,
                                     sdtWCB       **aWCBPtr )
{
    if ( SC_MAKE_SPACE( aGRID ) == SDT_SPACEID_WORKAREA )
    {
        (*aWCBPtr) = NULL;
    }
    else
    {
        IDE_TEST( sdtSortSegment::getWCBByNPID( aWASegment,
                                                aWAGroupID,
                                                SC_MAKE_PID( aGRID ),
                                                aWCBPtr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 *     WGRID�� �������� �ش� Slot�� �����͸� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWAPagePtr     - ��� WAPage�� ������
 * aGroupID       - ��� GroupID
 * aGRID          - ��� GRID ��
 * <OUT>
 * aNSlotPtr      - ���� ����
 * aIsValid       - Slot��ȣ�� �ùٸ���? ( Page�� �������, overflow����� )
 ***************************************************************************/
IDE_RC sdtSortSegment::getPagePtrByGRID( sdtSortSegHdr * aWASegment,
                                         sdtGroupID      aGroupID,
                                         scGRID          aGRID,
                                         UChar        ** aNSlotPtr )

{
    UChar         * sWAPagePtr = NULL;
    scPageID        sWPID;
    idBool          sIsValid;

    IDE_TEST( getPage( aWASegment,
                       aGroupID,
                       aGRID,
                       &sWPID,
                       &sWAPagePtr )
              != IDE_SUCCESS );

    getSlot( sWAPagePtr,
             sdtTempPage::getSlotCount( sWAPagePtr ),
             SC_MAKE_OFFSET( aGRID ),
             aNSlotPtr,
             &sIsValid );

    IDE_ERROR( sIsValid == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 *     getPage�� �����ϵ�, �ش� Page�� Fix��Ű�⵵ �Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGroupID       - ��� GroupID
 * aGRID          - ��� GRID ��
 * <OUT>
 * aWPID          - ���� WPID
 * aWAPagePtr     - ���� WAPage�� ��ġ
 * aSlotCount     - �ش� page�� SlotCount
 ***************************************************************************/
IDE_RC sdtSortSegment::getPageWithFix( sdtSortSegHdr * aWASegment,
                                       sdtGroupID      aGroupID,
                                       scGRID          aGRID,
                                       UInt          * aWPID,
                                       UChar        ** aWAPagePtr,
                                       UInt          * aSlotCount )
{
    if ( *aWPID != SC_NULL_PID )
    {
        IDE_DASSERT( *aWAPagePtr != NULL );
        unfixWAPage( aWASegment, *aWPID );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( getPage( aWASegment,
                       aGroupID,
                       aGRID,
                       aWPID,
                       aWAPagePtr )
              != IDE_SUCCESS );

    *aSlotCount = sdtTempPage::getSlotCount( *aWAPagePtr );

    fixWAPage( aWASegment, *aWPID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 *     GRID�� �������� Page�� ���� ������ ��´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGroupID       - ��� GroupID
 * aGRID          - ��� GRID ��
 * <OUT>
 * aWPID          - ���� WPID
 * aWAPagePtr     - ���� WAPage�� ��ġ
 * aSlotCount     - �ش� page�� SlotCount
 ***************************************************************************/
IDE_RC sdtSortSegment::getPage( sdtSortSegHdr * aWASegment,
                                sdtGroupID      aGroupID,
                                scGRID          aGRID,
                                UInt          * aWPID,
                                UChar        ** aWAPagePtr )
{
    sdtWCB    * sWCBPtr;
    scSpaceID   sSpaceID;
    scPageID    sPageID;

    IDE_DASSERT( SC_GRID_IS_NOT_NULL( aGRID ));

    sSpaceID = SC_MAKE_SPACE( aGRID );
    sPageID  =  SC_MAKE_PID( aGRID );

    if ( isWorkAreaTbsID( sSpaceID ) == ID_TRUE )
    {
        sWCBPtr  = getWCBInternal( aWASegment, sPageID );
        (*aWPID) = sPageID;
        (*aWAPagePtr) = getWAPagePtr( aWASegment,
                                      aGroupID,
                                      sWCBPtr );
    }
    else
    {
        IDE_TEST_RAISE( sctTableSpaceMgr::isTempTableSpace( sSpaceID ) != ID_TRUE,
                        tablespaceID_error );

        IDE_TEST( getWCBByNPID( aWASegment,
                                aGroupID,
                                sPageID,
                                &sWCBPtr ) != IDE_SUCCESS );
        (*aWPID)      =  getWPageID( aWASegment , sWCBPtr );

        (*aWAPagePtr) = sWCBPtr->mWAPagePtr;

        IDE_ASSERT( sWCBPtr->mWAPagePtr != NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( tablespaceID_error );
    {
        ideLog::log( IDE_SM_0,
                     "[FAILURE] Fatal error during fetch disk temp table"
                     "(Tablespace ID : %"ID_UINT32_FMT", Page ID : %"ID_UINT32_FMT")",
                     sSpaceID,
                     sPageID );
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/**************************************************************************
 * Description :
 *     getPage�� ������ ��������  Slot�� ��´�.
 *
 * <IN>
 * aWAPagePtr     - ��� WAPage�� ������
 * aSlotCount     - ��� Page�� Slot ����
 * aSlotNo        - ���� ���� ��ȣ
 * <OUT>
 * aNSlotPtr      - ���� ����
 * aIsValid       - Slot��ȣ�� �ùٸ���? ( Page�� �������, overflow����� )
 ***************************************************************************/
void sdtSortSegment::getSlot( UChar         * aWAPagePtr,
                              UInt            aSlotCount,
                              UInt            aSlotNo,
                              UChar        ** aNSlotPtr,
                              idBool        * aIsValid )
{
    if ( aSlotCount > aSlotNo )
    {
        *aNSlotPtr = aWAPagePtr +
            sdtTempPage::getSlotOffset( aWAPagePtr, (scSlotNum)aSlotNo );
        *aIsValid  = ID_TRUE;
    }
    else
    {
        /* Slot�� �Ѿ ���.*/
        *aNSlotPtr = aWAPagePtr;
        *aIsValid  = ID_FALSE;
    }
}

/**************************************************************************
 * Description :
 *     WPID�� �������� WAPage�� �����͸� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGroupID       - ��� GroupID
 * aWPID          - ��� Page
 ***************************************************************************/
UChar* sdtSortSegment::getWAPagePtr(sdtSortSegHdr * aWASegment,
                                    sdtGroupID      aGroupID,
                                    sdtWCB        * aWCBPtr )
{
    IDE_ASSERT( aWCBPtr->mWAPagePtr != NULL );

    if ( aWASegment->mHintWCBPtr != aWCBPtr )
    {
        moveLRUListToTop( aWASegment,
                          aGroupID,
                          aWCBPtr );

        aWASegment->mHintWCBPtr = aWCBPtr;
    }
    else
    {
        /* nothing to do */
    }

    return aWCBPtr->mWAPagePtr;
}


UChar * sdtSortSegment::getWAPagePtr( sdtWCB        * aWCBPtr )
{
    IDE_DASSERT( aWCBPtr != NULL );
    IDE_DASSERT( aWCBPtr->mWAPagePtr != NULL );

    return aWCBPtr->mWAPagePtr;
}


IDE_RC sdtSortSegment::chkAndSetWAPagePtr( sdtSortSegHdr * aWASegment,
                                           sdtWCB        * aWCBPtr )
{
    IDE_ASSERT( aWCBPtr->mNxtUsedWCBPtr != NULL );

    if ( aWCBPtr->mWAPagePtr == NULL )
    {
        return setWAPagePtr(aWASegment, aWCBPtr);
    }
    return IDE_SUCCESS;
}


/**************************************************************************
 * Description :
 *     WPID�� �������� WCB�� ��ġ�� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� Page ID
 ***************************************************************************/
sdtWCB * sdtSortSegment::getWCBInternal( sdtSortSegHdr * aWASegment,
                                         scPageID        aWPID )
{
    IDE_DASSERT( getMaxWAPageCount( aWASegment ) > aWPID );

    return &aWASegment->mWCBMap[aWPID];
}

/**************************************************************************
 * Description :
 *     WPID�� �������� WCB�� ��ġ�� ��ȯ�Ѵ�.
 *     Used List�� ����Ǿ� ���� ������ �����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� Page ID
 ***************************************************************************/
sdtWCB * sdtSortSegment::getWCBWithLnk( sdtSortSegHdr * aWASegment,
                                        scPageID        aWPID )
{
    sdtWCB * sWCBPtr;

    IDE_DASSERT( getMaxWAPageCount( aWASegment ) > aWPID );

    sWCBPtr = &aWASegment->mWCBMap[aWPID];

    if ( sWCBPtr->mNxtUsedWCBPtr == NULL )
    {
        sWCBPtr->mNxtUsedWCBPtr = aWASegment->mUsedWCBPtr;
        aWASegment->mUsedWCBPtr = sWCBPtr;
    }

    return sWCBPtr;
}

/**************************************************************************
 * Description :
 * WCB�� �������� Page�� Fix���θ� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� WPID
 ***************************************************************************/
idBool sdtSortSegment::isFixedPage( sdtWCB       * aWCBPtr )
{
    return (aWCBPtr->mFix > 0) ? ID_TRUE : ID_FALSE;
}

/**************************************************************************
 * Description :
 * WCB�� �ʱ�ȭ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWCBPtr        - ��� WCBPtr
 * aWPID          - �ش� WCB�� ���� WPAGEID
 ***************************************************************************/
void sdtSortSegment::initWCB( sdtWCB  * aWCBPtr )
{
    aWCBPtr->mWPState    = SDT_WA_PAGESTATE_INIT;
    aWCBPtr->mNPageID    = SC_NULL_PID;
    aWCBPtr->mFix        = 0;
    aWCBPtr->mBookedFree = ID_FALSE;
    aWCBPtr->mNextWCB4Hash = NULL;
}

/**************************************************************************
 * Description :
 * Map�� �����ϴ� ��ġ�� ��, ��� ������ Group�� ũ�⸦ Page������ �޴´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
UInt sdtSortSegment::getAllocableWAGroupPageCount( sdtSortSegHdr* aWASegment,
                                                   sdtGroupID     aWAGroupID )
{
    sdtSortGroup   * sGrpInfo = getWAGroupInfo( aWASegment, aWAGroupID );

    return getAllocableWAGroupPageCount( sGrpInfo );
}

void sdtSortSegment::moveLRUListToTop( sdtSortSegHdr* aWASegment,
                                       sdtGroupID     aWAGroupID,
                                       sdtWCB       * aWCBPtr )
{
    sdtSortGroup   * sGrpInfo;
    scPageID         sWPageID;

    if ( ( aWAGroupID != SDT_WAGROUPID_NONE ) &&
         ( aWAGroupID != SDT_WAGROUPID_INIT ) &&
         ( aWAGroupID != SDT_WAGROUPID_MAX )  &&
         ( aWCBPtr->mLRUPrevPID != SM_NULL_PID )) // NULL : �̹� top�̰ų� LRU�� �ƴϴ�.
    {
        sGrpInfo = &aWASegment->mGroup[ aWAGroupID ];

        sWPageID = getWPageID( aWASegment,
                               aWCBPtr );

        if ( ( sGrpInfo->mPolicy       == SDT_WA_REUSE_LRU ) &&
             ( sGrpInfo->mReuseWPIDTop != sWPageID ) )
        {
            IDE_DASSERT(( sGrpInfo->mBeginWPID <= sWPageID ) &&
                        ( sGrpInfo->mEndWPID   >  sWPageID ));

            moveLRUListToTopInternal( aWASegment,
                                      sGrpInfo,
                                      aWCBPtr );
        }
    }

    return ;
}


#endif //_O_SDT_SORT_SEGMENT_H_
