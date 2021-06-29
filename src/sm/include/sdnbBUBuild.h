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
 * $Id: sdnbBUBuild.h
 *
 * Description :
 *
 * Implementation Of Parallel Building Disk Index
 *
 **********************************************************************/

#ifndef _O_SDNB_BUBUILD_H_
#define _O_SDNB_BUBUILD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <idv.h>
#include <smcDef.h>
#include <smnDef.h>
#include <sdnbDef.h>
#include <sdnbModule.h>
#include <smuQueueMgr.h>

class sdnbBUBuild : public idtBaseThread
{
public:

    /* ������ �ʱ�ȭ */
    IDE_RC initialize( UInt             aTotalThreadCnt,
                       UInt             aID,
                       smcTableHeader * aTable,
                       smnIndexHeader * aIndex,
                       UInt             aMergePageCnt,
                       UInt             aAvailablePageSize,
                       UInt             aSortAreaPageCnt,
                       UInt             aInsertableKeyCnt,
                       idBool           aIsNeedValidation,
                       UInt             aBuildFlag,
                       idvSQL*          aStatistics );

    IDE_RC destroy();         /* ������ ���� */

    /* Index Build Main */
    static IDE_RC main( idvSQL          *aStatistics,
                        smcTableHeader  *aTable,
                        smnIndexHeader  *aIndex,
                        UInt             aThreadCnt,
                        ULong            aTotalSortAreaSize,
                        UInt             aTotalMergePageCnt,
                        idBool           aIsNeedValidation,
                        UInt             aBuildFlag,
                        UInt             aStatFlag );
    
    sdnbBUBuild();
    virtual ~sdnbBUBuild();

    inline UInt getErrorCode() { return mErrorCode; };
    inline ideErrorMgr* getErrorMgr() { return &mErrorMgr; };
    
    static IDE_RC updateStatistics( idvSQL         * aStatistics,
                                    sdrMtx         * aMtx,
                                    smnIndexHeader * aIndex,
                                    scPageID         aMinNode,
                                    scPageID         aMaxNode,
                                    SLong            aNumPages,
                                    SLong            aClusteringFactor,
                                    SLong            aIndexHeight,
                                    SLong            aNumDist,
                                    SLong            aKeyCount,
                                    UInt             aStatFlag );

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
    sdnbLKey         **mKeyMap;       /* �۾�����   ref-map */
    UChar            * mKeyBuffer;    /* �۾�����   Pointer */
    UInt               mKeyBufferSize;/* �۾������� ũ�� */
    iduStackMgr        mSortStack;        //BUG-27403 for Quicksort

    UInt               mInsertableMaxKeyCnt; /* �۾� �������� �۾� ������
                                             KeyMap�� �ִ� ���� */
    smuQueueMgr        mPIDBlkQueue;      /* merge block�� ���� queue */
    UInt               mFreePageCnt;
    idBool             mIsNeedValidation;
    sdrMtxLogMode      mLoggingMode;
    idBool             mIsForceMode;
    UInt               mBuildFlag;
    UInt               mPhase;         /* �������� �۾� �ܰ� */
    iduStackMgr        mFreePage;      /* Free temp page head */
    UInt               mMergePageCount;
    idBool             mIsSuccess;

    virtual void run();                         /* main ���� ��ƾ */

    /* Phase 1. Key Extraction & In-Memory Sort */
    IDE_RC extractNSort( sdnbStatistic * aIndexStat );
    /* Phase 2. merge sorted run */
    IDE_RC merge( sdnbStatistic * aIndexStat );
    /* Phase 3. make tree */    
    IDE_RC makeTree( sdnbBUBuild    * aThreads, 
                     UInt             aThreadCnt,
                     UInt             aMergePageCnt,
                     sdnbStatistic  * aIndexStat,
                     UInt             aStatFlag );

    /* ������ �۾� ���� ��ƾ */
    static IDE_RC threadRun( UInt         aPhase,
                             UInt         aThreadCnt,
                             sdnbBUBuild *aThreads );
    
    /* quick sort with KeyInfoMap*/
    IDE_RC quickSort( sdnbStatistic *aIndexStat,
                      UInt           aHead,
                      UInt           aTail );

    /* check sorted block */
    idBool checkSort( sdnbStatistic *aIndexStat,
                      UInt           aHead,
                      UInt           aTail );

    /* �۾� ������ �ִ� sorted block�� Temp �������� �̵� */
    IDE_RC storeSortedRun( UInt    aHead,
                           UInt    aTail,
                           UInt  * aLeftPos,
                           UInt  * aLeftSize);

    IDE_RC preparePages( UInt aNeedPageCnt );

    /* Temp ������ ���� free list ���� */
    IDE_RC allocPage( sdrMtx         * aMtx,
                      scPageID       * aPageID,
                      sdpPhyPageHdr ** aPageHdr,
                      sdnbNodeHdr   ** aNodeHdr,
                      sdpPhyPageHdr  * aPrevPageHdr,
                      scPageID       * sPrevPID,
                      scPageID       * sNextPID );

    IDE_RC freePage( scPageID aPageID );
    IDE_RC mergeFreePage( sdnbBUBuild  * aThreads,
                          UInt           aThreadCnt );
    IDE_RC removeFreePage();
    
    /* KeyValue�κ��� KeySize�� ��´�*/
    UShort getKeySize( UChar *aKeyValue );

    /* KeyMap���� swap */
    void   swapKeyMap( UInt aPos1,
                       UInt aPos2 );

    IDE_RC heapInit( sdnbStatistic    * aIndexStat,
                     UInt               aRunCount,
                     UInt               aHeapMapCount,
                     SInt             * aHeapMap,
                     sdnbMergeRunInfo * aMergeRunInfo );

    IDE_RC heapPop( sdnbStatistic    * aIndexStat,
                    UInt               aMinIdx,
                    idBool           * aIsClosedRun,
                    UInt               aHeapMapCount,
                    SInt             * aHeapMap,
                    sdnbMergeRunInfo * aMergeRunInfo );
    
    IDE_RC write2LeafNode( sdnbStatistic  *aIndexStat,
                           sdrMtx         *aMtx,
                           sdpPhyPageHdr **aPage,
                           sdnbStack      *aStack,
                           SShort         *aSlotSeq,
                           sdnbKeyInfo    *aKeyInfo,
                           idBool         *aIsDupValue );
    
    IDE_RC write2ParentNode( sdrMtx         *aMtx,
                             sdnbStack      *aStack,
                             UShort          aHeight,
                             sdnbKeyInfo    *aKeyInfo,
                             scPageID        aPrevPID,
                             scPageID        aChildPID );    
    
    IDE_RC getKeyInfoFromLKey( scPageID      aPageID,
                               SShort        aSlotSeq,
                               sdpPhyPageHdr *aPage,
                               sdnbKeyInfo   *aKeyInfo );
    // To fix BUG-17732
    idBool isNull( sdnbHeader * aHeader,
                   UChar      * aRow );
};

#endif // _O_SDNB_BUBUILD_H_
