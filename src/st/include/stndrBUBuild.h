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
 *
 * Description :
 *
 * Implementation Of Parallel Building Disk Index
 *
 **********************************************************************/

#ifndef _O_STNDR_BUBUILD_H_
#define _O_STNDR_BUBUILD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <idv.h>
#include <smcDef.h>
#include <smnDef.h>
#include <stndrDef.h>
#include <stndrModule.h>
#include <smuQueueMgr.h>


typedef enum {
    STNDR_NODE_LKEY = 0,
    STNDR_NODE_IKEY
} stndrNodeType;

typedef enum {
    STNDR_SORT_X = 0,
    STNDR_SORT_Y
} stndrSortType;

class stndrBUBuild : public idtBaseThread
{
public:

    /* ������ �ʱ�ȭ */
    IDE_RC initialize( UInt             aTotalThreadCnt,
                       UInt             aID,
                       smcTableHeader * aTable,
                       smnIndexHeader * aIndex,
                       UInt             aMergePageCnt,
                       UInt             aAvailablePageSize,
                       UInt             aSortAreaSize,
                       UInt             aMaxKeyMapCount,
                       idBool           aIsNeedValidation,
                       UInt             aBuildFlag,
                       idvSQL*          aStatistics );

    IDE_RC destroy();         /* ������ ���� */

    /* Index Build Main */
    static IDE_RC main( idvSQL          *aStatistics,
                        void            *aTrans,
                        smcTableHeader  *aTable,
                        smnIndexHeader  *aIndex,
                        UInt             aThreadCnt,
                        UInt             aTotalSortAreaSize,
                        UInt             aTotalMergePageCnt,
                        idBool           aIsNeedValidation,
                        UInt             aBuildFlag );

    stndrBUBuild();
    virtual ~stndrBUBuild();

    inline UInt getErrorCode() { return mErrorCode; };
    inline ideErrorMgr* getErrorMgr() { return &mErrorMgr; };

    static UInt getAvailablePageSize();

    static UInt getMinimumBuildKeyLength( UInt aBuildKeyValueLength );

    /* callback functions */
    static void getCPFromLBuildKey( UChar            ** aBuildKeyMap,
                                    UInt                aPos,
                                    stndrCenterPoint  * aCenterPoint );
    
    static void getCPFromIBuildKey( UChar            ** aBuildKeyMap,
                                    UInt                aPos,
                                    stndrCenterPoint  * aCenterPoint );

    static SInt compareLBuildKeyAndCP( stndrCenterPoint    aCenterPoint,
                                       void              * aBuildKey,
                                       stndrSortType       aSortType );

    static SInt compareIBuildKeyAndCP( stndrCenterPoint    aCenterPoint,
                                       void              * aBuildKey,
                                       stndrSortType       aSortType );

    static UInt getLBuildKeyLength( UInt aBuildKeyValueLength );

    static UInt getIBuildKeyLength( UInt aBuildKeyValueLength );

    static void insertLBuildKey( sdpPhyPageHdr * aNode,
                                 SShort          aSlotSeq,
                                 void          * aBuildKey );

    static void insertIBuildKey( sdpPhyPageHdr * aNode,
                                 SShort          aSlotSeq,
                                 void          * aBuildKey );

    static SInt compareLBuildKeyAndLBuildKey( void          * aBuildKey1,
                                              void          * aBuildKey2,
                                              stndrSortType   aSortType );   

    static SInt compareIBuildKeyAndIBuildKey( void          * aBuildKey1,
                                              void          * aBuildKey2,
                                              stndrSortType   aSortType );

    static idBool checkSortLBuildKey( UInt             aHead,
                                      UInt             aTail,
                                      UChar         ** aBuildKeyMap,
                                      stndrSortType    aSortType );

    static idBool checkSortIBuildKey( UInt             aHead,
                                      UInt             aTail,
                                      UChar         ** aBuildKeyMap,
                                      stndrSortType    aSortType );

    static UInt getLNodeKeyLength( UInt aNodeKeyValueLength );

    static UInt getINodeKeyLength( UInt aNodeKeyValueLength );

    static UInt getFreeSize4EmptyLNode( stndrHeader * aIndexHeader );

    static UInt getFreeSize4EmptyINode( stndrHeader * /* aIndexHeader */ );

    static IDE_RC insertLNodeKey( sdrMtx        * aMtx,
                                  stndrHeader   * aIndexHeader,
                                  sdpPhyPageHdr * aCurrPage,
                                  SShort          aSlotSeq,
                                  UInt            aNodeKeyValueLength,
                                  void          * aBuildKey );

    static IDE_RC insertINodeKey( sdrMtx        * aMtx,
                                  stndrHeader   * aIndexHeader,
                                  sdpPhyPageHdr * aCurrPage,
                                  SShort          aSlotSeq,
                                  UInt            aNodeKeyValueLength,
                                  void          * aBuildKey );
    
    static IDE_RC updateStatistics( idvSQL          * aStatistics,
                                    sdrMtx          * aMtx,
                                    smnIndexHeader  * aIndex,
                                    SLong             aKeyCount,
                                    SLong             aNumDist );
private:
    UInt               mLeftSizeThreshold;  // ����Ű�� �������� ������ ������ ����

    idBool             mFinished;    /* ������ ���� ���� flag */
    idBool *           mContinue;    /* ������ �ߴ� ���� flag */
    UInt               mErrorCode;
    ideErrorMgr        mErrorMgr;

    UInt               mTotalThreadCnt;
    UInt               mTotalMergeCnt;
    UInt               mID;          /* ������ ��ȣ */

    idvSQL*            mStatistics; /* TASK-2356 Altibase Wait Interface
                                     * ��������� ��Ȯ�� �����Ϸ���,
                                     * ������ ������ŭ �־�� �Ѵ�.
                                     * ����� ��������� �������� �ʰ�,
                                     * SessionEvent�� üũ�ϴ� �뵵�� ����Ѵ�. */
    void             * mTrans;
    smcTableHeader   * mTable;
    smnIndexHeader   * mIndex;
    UChar           ** mKeyMap;       /* �۾�����   ref-map */
    UChar            * mKeyBuffer;    /* �۾�����   Pointer */
    UInt               mKeyBufferSize;/* �۾������� ũ�� */
    iduStackMgr        mSortStack;        //BUG-27403 for Quicksort

    UInt               mMaxKeyMapCount;  /* �۾� �������� �۾� ������
                                            KeyMap �� �ִ� ���� */
    smuQueueMgr        mRunQueue;        /* sorted run �� ���� queue */
    smuQueueMgr        mPartitionQueue;  /* partition  �� ���� queue */
    UInt               mFreePageCnt;
    idBool             mIsNeedValidation;
    sdrMtxLogMode      mLoggingMode;
    idBool             mIsForceMode;
    UInt               mBuildFlag;
    UInt               mPhase;         /* �������� �۾� �ܰ� */
    iduStackMgr        mFreePage;      /* Free temp page head */
    UInt               mMergePageCount;
    idBool             mIsSuccess;
    ULong              mPropagatedKeyCount;   /* ���� height �� node ������
                                                 ��� �ʿ����� ����ϱ� ���� ���� */
    ULong              mKeyCount4Partition;   /* 1���� Y Sorted Run ��
                                                 ���� key count */
    UInt               mFreeSize4EmptyNode;   /* empty node �� �����
                                                 (slotEntry �� slot �� ���� ����) */
    UShort             mKeyCount4EmptyNode;   // empty node �� ���� key count

    virtual void run();                         /* main ���� ��ƾ */

    /* Phase 1. Key Extraction & In-Memory Sort */
    IDE_RC extractNSort();
    /* Phase 2. merge sorted run */
    IDE_RC merge( UInt            aMergePageCount,
                  stndrNodeType   aNodeType,
                  stndrSortType   aSortType );

    /* ������ �۾� ���� ��ƾ */
    static IDE_RC threadRun( UInt         aPhase,
                             UInt         aThreadCnt,
                             stndrBUBuild *aThreads );

    /* quick sort with KeyInfoMap */
    IDE_RC quickSort( UInt              aHead,
                      UInt              aTail,
                      stndrNodeType     aNodeType,
                      stndrSortType     aSortType );

    /* check sorted block */
    idBool checkSort( UInt    aHead,
                      UInt    aTail );

    /* �۾� ������ �ִ� sorted block�� Temp �������� �̵� */
    IDE_RC storeSortedRun( UInt             aHead,
                           UInt             aTail,
                           UInt           * aLeftPos,
                           UInt           * aLeftSize,
                           stndrNodeType    aNodeType );

    IDE_RC preparePages( UInt aNeedPageCnt );

    /* Temp ������ ���� free list ���� */
    IDE_RC allocAndLinkPage( sdrMtx         * aMtx,
                             scPageID       * aPageID,
                             sdpPhyPageHdr ** aPageHdr,
                             stndrNodeHdr   ** aNodeHdr,
                             sdpPhyPageHdr  * aPrevPageHdr,
                             scPageID       * sPrevPID,
                             scPageID       * sNextPID );

    IDE_RC freePage( scPageID aPageID );
    IDE_RC collectFreePage( stndrBUBuild  * aThreads,
                            UInt           aThreadCnt );
    IDE_RC removeFreePage();

    IDE_RC heapInit( UInt            aRunCount,
                     UInt            aHeapMapCount,
                     SInt          * aHeapMap,
                     stndrRunInfo  * aRunInfo,
                     stndrNodeType   aNodeType,
                     stndrSortType   aSortType );

    IDE_RC heapPop( UInt               aMinIdx,
                    idBool           * aIsClosedRun,
                    UInt               aHeapMapCount,
                    SInt             * aHeapMap,
                    stndrRunInfo     * aRunInfo,
                    stndrNodeType      aNodeType,
                    stndrSortType      aSortType );

    IDE_RC getBuildKeyFromInfo( scPageID         aPageID,
                                SShort           aSlotSeq,
                                sdpPhyPageHdr  * aPage,
                                void          ** aBuildKey );

    void swapBuildKeyMap( UChar ** aBuildKeyMap,
                          UInt     aPos1,
                          UInt     aPos2 );

    IDE_RC collectRun( stndrBUBuild   * aThreads,
                       UInt             aThreadCnt );

    /* Phase 3. Make Partition */
    void makePartition( stndrNodeType aNodeType );

    IDE_RC mergeX( stndrBUBuild   * aThreads,
                   UInt             aThreadCnt,
                   UInt             aMergePageCnt,
                   stndrNodeType    aNodeType );

    IDE_RC getBuildKeyFromRun( idBool            * aIsClosedRun,
                               stndrRunInfo      * aRunInfo,
                               void             ** aBuildKey,
                               stndrNodeType       aNodeType );

    /* Phase 4. Serial In-Memory Y Sort & Y Merge */
    IDE_RC sortY( UInt          aMergePageCnt,
                  stndrNodeType aNodeType );

    idBool isNodeFull( UChar * aPage );
    
    IDE_RC write2Node( sdrMtx          *aMtx,
                       UShort           aHeight,
                       sdpPhyPageHdr  **aPage,
                       scPageID       * aPageID,
                       SShort          *aSlotSeq,
                       void            *aBuildKey,
                       stndrNodeType    aNodeType );

    /* Phase 5. Make Node (leaf or internal) */
    IDE_RC makeNodeAndSort( UShort           aHeight,
                            scPageID       * aLastNodePID,
                            UInt           * aNodeCount,
                            stndrNodeType    aNodeType );
};

#endif // _O_STNDR_BUBUILD_H_
