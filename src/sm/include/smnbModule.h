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
 * $Id: smnbModule.h 89495 2020-12-14 05:19:22Z emlee $
 **********************************************************************/

#ifndef _O_SMNB_MODULE_H_
# define _O_SMNB_MODULE_H_ 1

# include <smnDef.h>
# include <smDef.h>
# include <smnbDef.h>
# include <idu.h>

extern smnIndexModule smnbModule;

class smnbBTree
{
public:

    /*************************************************/
    /* PUBLIC FUNCTIONS                              */
    /*************************************************/

    static IDE_RC prepareIteratorMem( const smnIndexModule* aModule );

    static IDE_RC releaseIteratorMem( const smnIndexModule* aModule );

    static IDE_RC prepareFreeNodeMem( const smnIndexModule* aModule );

    static IDE_RC releaseFreeNodeMem( const smnIndexModule* aModule );

    static IDE_RC freeAllNodeList( idvSQL         * aStatistics,
                                   smnIndexHeader * aIndex,
                                   void           * aTrans );
    
    static IDE_RC freeNode( smnbNode* aNodes );
    
    static IDE_RC create( idvSQL             * aStatistics,
                          smcTableHeader     * aTable,
                          smnIndexHeader     * aIndex,
                          smiSegAttr         * /*aSegAttr*/,
                          smiSegStorageAttr  * /*aSegStoAttr*/,
                          smnInsertFunc      * aInsert,
                          smnIndexHeader    ** aRebuildIndexHeader,
                          ULong                aSmoNo );

    static IDE_RC buildIndex( idvSQL*               aStatistics,
                              void*                 aTrans,
                              smcTableHeader*       aTable,
                              smnIndexHeader*       aIndex,
                              smnGetPageFunc        aGetPageFunc,
                              smnGetRowFunc         aGetRowFunc,
                              SChar*                aNullPtr,
                              idBool                aIsNeedValidation,
                              idBool                aIsPersistent,
                              UInt                  aParallelDegree,
                              UInt                  aBuildFlag,
                              UInt                  aTotalRecCnt );

    static IDE_RC drop( smnIndexHeader * aIndex );

    static IDE_RC init( smnbIterator        * aIterator,
                        void                * aTrans,
                        smcTableHeader      * aTable,
                        smnIndexHeader      * aIndex,
                        void                * aDumpObject,
                        const smiRange      * aKeyRange,
                        const smiRange      * aKeyFilter,
                        const smiCallBack   * aRowFilter,
                        UInt                  aFlag,
                        smSCN                 aSCN,
                        smSCN                 aInfinite,
                        idBool                aUntouchable,
                        smiCursorProperties * aProperties,
                        const smSeekFunc   ** aSeekFunc,
                        smiStatement        * aStatement );

    static IDE_RC dest( smnbIterator * /*aIterator*/ );

    static IDE_RC isRowUnique( void          * aTrans,
                               smnbStatistic * aIndexStat,
                               smSCN           aStmtViewSCN,
                               void          * aRow,
                               smTID         * aTid,
                               SChar        ** aExistUniqueRow,
                               idBool          aForbiddenToRetry );

    static SInt compareRows( smnbStatistic      * aIndexStat,
                             const smnbColumn   * aColumns,
                             const smnbColumn   * aFence,
                             const void         * aRow1,
                             const void         * aRow2 );
    
    static SInt compareRowsAndPtr2( smnbStatistic      * aIndexStat,
                                    const smnbColumn   * aColumns,
                                    const smnbColumn   * aFence,
                                    SChar              * aRow1,
                                    SChar              * aRow2,
                                    idBool             * aIsEqual );

    static SInt compareKeys( smnbStatistic      * aIndexStat,
                             const smnbColumn   * aColumns,
                             const smnbColumn   * aFence,
                             void               * aKey1,
                             SChar              * aRow1,
                             UInt                 aPartialKeySize,
                             SChar              * aRow2,
                             idBool             * aIsSuccess );

    static idBool isNullColumn( smnbColumn * aColumn,
                                smiColumn  * aSmiColumn,
                                SChar      * aRow );
    
    /* BUG-30074 disk table�� unique index���� NULL key ���� ��/
    *           non-unique index���� deleted key �߰� ��
    *           Cardinality�� ��Ȯ���� ���� �������ϴ�.
    * Key ��ü�� Null���� Ȯ���Ѵ�. ��� Null�̾�� �Ѵ�. */
    static idBool isNullKey( smnbHeader * aIndex,
                             SChar      * aRow );

    static IDE_RC insertRowUnique( idvSQL           * aStatistics,
                                   void             * aTrans,
                                   void             * aTable,
                                   void             * aIndex,
                                   smSCN              aInfiniteSCN,
                                   SChar            * aRow,
                                   SChar            * aNull,
                                   idBool             aUniqueCheck,
                                   smSCN              aStmtViewSCN,
                                   void             * aRowSID,
                                   SChar           ** aExistUniqueRow,
                                   ULong              aInsertWaitTime,
                                   idBool             aForbiddenToRetry );

    static IDE_RC insertRow( idvSQL          * aStatistics,
                             void            * aTrans,
                             void            * aTable,
                             void            * aIndex,
                             smSCN             aInfiniteSCN,
                             SChar           * aRow,
                             SChar           * aNull,
                             idBool            aUniqueCheck,
                             smSCN             aStmtViewSCN,
                             void            * aRowSID,
                             SChar          ** aExistUniqueRow,
                             ULong             aInsertWaitTime,
                             idBool            aForbiddenToRetry );

    static void merge(smnbINode* a_pLeftNode,
                      smnbINode* a_pRightNode);
    
    static IDE_RC deleteNA( idvSQL*           aStatistics,
                            void*             aTrans,
                            void*             aIndex,
                            SChar*            aRow,
                            smiIterator *     aIterator,
                            sdRID             aRowRID);

    static IDE_RC freeSlot( void    * aIndex,
                            SChar   * aRow,
                            idBool    aIgnoreNotFoundKey,
                            idBool  * aIsExistFreeKey );

    static IDE_RC freeSlotByLeafFullScan( smnbHeader   * aIndexHeader,
                                          SChar        * aRow );

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure of
     * index aging. */
    static IDE_RC checkExistKey( void   * aIndex,
                                 SChar  * aRow );

    /* TASK-4990 changing the method of collecting index statistics
     * ���� ������� ���� ��� */
    static IDE_RC gatherStat( idvSQL         * aStatistics,
                              void           * aTrans,
                              SFloat           aPercentage,
                              SInt             aDegree,
                              smcTableHeader * aHeader,
                              void           * aTotalTableArg,
                              smnIndexHeader * aIndex,
                              void           * aStats,
                              idBool           aDynamicMode );

    static IDE_RC NA( void );

    static IDE_RC beforeFirst( smnbIterator*       aIterator,
                               const smSeekFunc**  aSeekFunc );
    //BUG-48230: beforeFirst for Queue
    static IDE_RC beforeFirstInternalQ( smnbIterator* a_pIterator );
    static IDE_RC beforeFirstQ( void*               aIterator);

    static IDE_RC afterLast( smnbIterator*       aIterator,
                             const smSeekFunc**  aSeekFunc );
    
    static IDE_RC beforeFirstU( smnbIterator*       aIterator,
                                const smSeekFunc**  aSeekFunc );
    
    //BUG-48230: afterLast for Queue
    static IDE_RC afterLastQ( void*               aIterator);

    static IDE_RC afterLastU( smnbIterator*       aIterator,
                              const smSeekFunc**  aSeekFunc );
    
    static IDE_RC beforeFirstR( smnbIterator*       aIterator,
                                const smSeekFunc**  aSeekFunc );

    static IDE_RC afterLastR( smnbIterator*       aIterator,
                              const smSeekFunc**  aSeekFunc );
    
    static IDE_RC fetchNext( smnbIterator  * a_pIterator,
                             const void   ** aRow );

    //BUG-48230: fetchNext for Queue
    static IDE_RC fetchNextQ( smnbIterator  * a_pIterator,
                              const void   ** aRow );

    static IDE_RC fetchPrev( smnbIterator* aIterator,
                             const void**  aRow );

    static IDE_RC fetchNextU( smnbIterator* aIterator,
                              const void**  aRow );
    
    static IDE_RC fetchPrevU( smnbIterator* aIterator,
                              const void**  aRow );
    

    static IDE_RC retraverse( idvSQL*       aStatistics,
                              smnbIterator* aIterator,
                              const void**  /*aRow*/ );

    static inline IDE_RC initLeafNode( smnbLNode  * a_pNode,
                                       smnbHeader * aIndexHeader,
                                       IDU_LATCH    a_latch );
    static inline void initInternalNode( smnbINode *a_pNode,
                                         smnbHeader * aIndexHeader,
                                         IDU_LATCH a_latch );

    static inline void lockTree( smnbHeader * a_pIndex );
    static inline void unlockTree( smnbHeader * a_pIndex );

    /* For Make Disk Image When server is stopped normally */
    static IDE_RC makeDiskImage(smnIndexHeader* a_pIndex,
                                smnIndexFile*   a_pIndexFile);
    static IDE_RC makeNodeListFromDiskImage(smcTableHeader *a_pTable,
                                            smnIndexHeader *a_pIndex);

    static IDE_RC getPosition( smnbIterator *     aIterator,
                               smiCursorPosInfo * aPosInfo );
    
    static IDE_RC setPosition( smnbIterator     * aIterator,
                               smiCursorPosInfo * aPosInfo );

    static IDE_RC allocIterator( void ** aIteratorMem );
    
    static IDE_RC freeIterator( void * aIteratorMem);

    // To Fix BUG-15670
    // Row Pointer�� �̿��Ͽ� Key Column�� ���� Value Ptr ȹ��
    // To Fix BUG-24449
    // Ű ���̸� �������� MT �Լ��� �̿��ؾ� �Ѵ�
    static IDE_RC getKeyValueAndSize( SChar         * aRowPtr,
                                      smnbColumn    * aIndexColumn,
                                      void          * aKeyValue,
                                      UShort        * aKeySize );

    static void logIndexNode( smnIndexHeader * aIndex,
                              smnbNode       * aNode );
    static void logIndexHeader( smnIndexHeader * aHeader );
    static void dumpIndexHeader( smnbHeader * aHeader,
                                 SChar      * aOutBuf,
                                 UInt         aOutSize );

    static IDE_RC makeKeyFromRow( smnbHeader   * aIndex,
                                  SChar        * aRow,
                                  void         * aKey );

    static inline void setInternalSlot( smnbINode   * aNode,
                                        SShort        aIdx,
                                        smnbNode    * aChildPtr,
                                        SChar       * aRowPtr,
                                        void        * aKey );

    static inline void getInternalSlot( smnbNode   ** aChildPtr,
                                        SChar      ** aRowPtr,
                                        void       ** aKey,
                                        smnbINode   * aNode,
                                        SShort        aIdx );

    static inline void setLeafSlot( smnbLNode   * aNode,
                                    SShort        aIdx,
                                    SChar       * aRowPtr,
                                    void        * aKey );

    static inline void getLeafSlot( SChar      ** aRowPtr,
                                    void       ** aKey,
                                    smnbLNode   * aNode,
                                    SShort        aIdx );

    static inline void copyLeafSlots( smnbLNode * aDest,
                                      SShort      aDestStartIdx,
                                      smnbLNode * aSrc,
                                      SShort      aSrcStartIdx,
                                      SShort      aSrcEndIdx );

    static void setNodeSplitRate( void ); /* BUG-40509 */

    static UInt getNodeSplitRate( void ); /* BUG-40509 */

    /* PROJ-2614 Memory Index Reorganization */
    static IDE_RC keyReorganization( smnbHeader * aIndex );

private:

    /*************************************************/
    /* PRIVATE FUNCTIONS                             */
    /*************************************************/

    static SInt compareRowsAndPtr( smnbStatistic    * aIndexStat,
                                   const smnbColumn * aColumns,
                                   const smnbColumn * aFence,
                                   const void       * aRow1,
                                   const void       * aRow2 );
    
    static SInt compareKeyVarColumn( smnbColumn * aColumn,
                                     void       * aKey,
                                     UInt         aPartialKeySize,
                                     SChar      * aRow );

    static SInt compareVarColumn( const smnbColumn * aColumn,
                                  const void       * aRow1,
                                  const void       * aRow2 );   

    static idBool isVarNull( smnbColumn * aColumn,
                             smiColumn  * aSmiColumn,
                             SChar      * aRow );
 
    static UInt getMaxKeyValueSize( smnIndexHeader *aIndexHeader );

    // BUG-19249
    static inline UInt getKeySize( UInt aKeyValueSize );

    static IDE_RC insertIntoInternalNode( smnbHeader  * aHeader,
                                          smnbINode   * a_pINode,
                                          void        * a_pRow,
                                          void        * aKey,
                                          void        * a_pChild );

    /* BUG-47206 : findKeyInLeaf(), findKeyInInternal() �Լ��� �����Ѵ�. */
    static IDE_RC findKeyInNode( smnbHeader   * aHeader,
                                 smnbNode     * aNode,
                                 SInt           aMinimum,
                                 SInt           aMaximum,
                                 SChar        * aRow,
                                 SInt         * aSlot );

    /* BUG-47206 : findRowInLeaf(), findRowInInternal() �Լ��� �����Ѵ�. */
    static void findRowInNode( smnbHeader   * aHeader,
                               smnbNode     * aNode,
                               SInt           aMinimum,
                               SInt           aMaximum,
                               SChar        * aRow,
                               SInt         * aSlot );

    /* BUG-47206 : findSlotInLeaf(), findSotInInternal() �Լ��� �����Ѵ�. */
    static IDE_RC findSlotInNode( smnbHeader  * aHeader,
                                  smnbNode    * aNode,
                                  void        * aRow,
                                  SInt        * aSlot );

    static IDE_RC insertIntoLeafNode( smnbHeader      * aHeader,
                                      smnbLNode       * aLeafNode,
                                      SChar           * aRow,
                                      SInt              aSlotPos );

    static IDE_RC updateStat4Insert( smnIndexHeader  * aPersIndex,
                                     smnbHeader      * aIndex,
                                     smnbLNode       * aLeafNode,
                                     SInt              aSlotPos, 
                                     SChar           * aRow,
                                     idBool            aIsUniqueIndex );

    static SChar* getPrvLNodeLstRow( smnbLNode * a_pLeafNode );
    static SChar* getNxtLNodeFstRow( smnbLNode * a_pLeafNode );

    /* BUG-47206 : findFirstSlotInLeaf(), findFirstSlotInInternal() �Լ��� �����Ѵ�. */
    static inline void findFirstSlotInNode( smnbHeader          * aHeader,
                                            const smiCallBack   * aCallBack,
                                            const smnbNode      * aNode,
                                            SInt                * aSlot );

    static IDE_RC findLast( smnbHeader       * aIndexHeader,
                            const smiCallBack* a_pCallBack,
                            SInt*              a_pDepth,
                            smnbStack*         a_pStack,
                            SInt*              a_pSlot );
    
    static IDE_RC afterLastInternal( smnbIterator* a_pIterator );
    
    static inline void findLastSlotInInternal( smnbHeader          * aHeader,
                                               const smiCallBack   * aCallBack,
                                               const smnbINode     * aNode,
                                               SInt                  aMinimum,
                                               SInt                  aMaximum,
                                               SInt                * aSlot );

    static inline void findLastSlotInLeaf( smnbHeader          * aHeader,
                                           const smiCallBack   * aCallBack,
                                           const smnbLNode     * aNode,
                                           SInt                  aMinimum,
                                           SInt                  aMaximum,
                                           SInt                * aSlot );

    static IDE_RC findLstLeaf( smnbHeader        * aIndexHeader,
                               const smiCallBack * a_pCallBack,
                               SInt              * a_pDepth,
                               smnbStack         * a_pStack,
                               smnbLNode        ** a_pLNode );

    static IDE_RC splitInternalNode( smnbHeader      * a_pIndexHeader,
                                     void            * a_pRow,
                                     void            * aKey,
                                     void            * a_pChild,
                                     SInt              a_nSlotPos,
                                     smnbINode       * a_pINode,
                                     smnbINode       * a_pNewINode,
                                     void           ** a_pSepKeyRow,
                                     void           ** aSepKey );

    static IDE_RC splitLeafNode( smnbHeader       * aIndexHeader,
                                 SInt               aSlotPos,
                                 smnbLNode        * aLeafNode,
                                 smnbLNode        * aNewLeafNode,
                                 smnbLNode       ** aTargetLeaf,
                                 SInt             * aTargetSeq );

    /* BUG-47206
       ������ ����� �����ϴ� findFirst(=findFstLeaf) �Լ��� findPosition �Լ���
       findPosition ���� �����Ѵ�. */
    static IDE_RC findPosition( smnbHeader         * aIndexHeader,
                                const smiCallBack  * aCallBack,
                                SChar              * aRow,
                                SInt               * aDepth,
                                smnbStack          * aStack,
                                smnbLNode         ** aLNode);

    static IDE_RC checkUniqueness( smnIndexHeader   * aIndexHeader,
                                   void             * a_pTrans,
                                   smnbStatistic    * aIndexStat,
                                   const smnbColumn * a_pColumns,
                                   const smnbColumn * a_pFence,
                                   smnbLNode        * a_pLeafNode,
                                   SInt               a_nSlotPos,
                                   SChar            * a_pRow,
                                   smSCN              aStmtViewSCN,
                                   idBool             sIsTreeLatched,
                                   smTID            * a_pTransID,
                                   idBool           * sIsRetraverse,
                                   SChar           ** aExistUniqueRow,
                                   SChar           ** a_diffRow,
                                   SInt             * a_diffSlotPos,
                                   idBool             aForbiddenToRetry );

    static void deleteSlotInLeafNode( smnbLNode*        a_pLeafNode,
                                      SChar*            a_pRow,
                                      SInt*             a_pSlotPos );
    
    static IDE_RC updateStat4Delete( smnIndexHeader  * aPersIndex,
                                     smnbHeader      * aIndex,
                                     smnbLNode       * a_pLeafNode,
                                     SChar           * a_pRow,
                                     SInt            * a_pSlotPos,
                                     idBool            aIsUniqueIndex);

    static IDE_RC deleteSlotInInternalNode( smnbINode*        a_pInternalNode,
                                            SInt              a_nSlotPos );

    static IDE_RC rebuildMinMaxStat( smnIndexHeader * aPersistentIndexHeader,
                                     smnbHeader     * aRuntimeIndexHeader,
                                     idBool           aMinStat );

    static IDE_RC analyzeNode( smnIndexHeader * aPersistentIndexHeader,
                               smnbHeader     * aRuntimeIndexHeader,
                               smnbLNode      * aTargetNode,
                               SLong          * aClusteringFactor,
                               SLong          * aNumDist,
                               SLong          * aKeyCount,
                               SLong          * aMetaSpace,
                               SLong          * aUsedSpace,
                               SLong          * aAgableSpace, 
                               SLong          * aFreeSpace );

    static void getNextNode4Stat( smnbHeader     * aIdxHdr,
                                  idBool           aBackTraverse,
                                  idBool         * aNodeLatched,
                                  UInt           * aIndexHeight,
                                  smnbLNode     ** aCurNode );

    static IDE_RC beforeFirstInternal( smnbIterator* a_pIterator );
    
    static IDE_RC getNextNode( smnbIterator * aIterator,
                               idBool       * aIsRestart );

    static IDE_RC getPrevNode( smnbIterator   * aIterator,
                               idBool         * aIsRestart );

    static IDE_RC fetchNextR( smnbIterator* aIterator );

    static inline IDE_RC destroyNode( smnbNode * a_pNode );

    static inline void lockNode( smnbLNode * a_pLeafNode );
    static inline void unlockNode( smnbLNode * a_pLeafNodne );
    
    static IDU_LATCH getLatchValueOfINode( volatile smnbINode* aNodePtr );

    static IDU_LATCH getLatchValueOfLNode( volatile smnbLNode* aNodePtr );

    /* TASK-4990 changing the method of collecting index statistics */
    static IDE_RC setMinMaxStat( smnIndexHeader * aCommonHeader,
                                 SChar          * aRowPtr,
                                 smnbColumn     * aIndexColumn,
                                 idBool           aIsMinStat );

    static inline idBool needToUpdateStat( idBool aIsMemTBS );

    static IDE_RC dumpIndexNode( smnIndexHeader * aIndex,
                                 smnbNode       * aNode,
                                 SChar          * aOutBuf,
                                 UInt             aOutSize );

    static IDE_RC prepareNodes( smnbHeader    * aIndexHeader,
                                smnbStack     * aStack,
                                SInt            aDepth,
                                smnbNode     ** aNewNodeList,
                                UInt          * aNewNodeCount );

    static void getPreparedNode( smnbNode    ** aNodeList,
                                 UInt         * aNodeCount,
                                 void        ** aNode );

    static void setIndexProperties();

    static inline void copyInternalSlots( smnbINode * aDest,
                                          SShort      aDestStartIdx,
                                          smnbINode * aSrc,
                                          SShort      aSrcStartIdx,
                                          SShort      aSrcEndIdx );
    static inline void shiftInternalSlots( smnbINode    * aNode,
                                           SShort         aStartIdx,
                                           SShort         aEndIdx,
                                           SShort         aShift );
    static inline void shiftLeafSlots( smnbLNode    * aNode,
                                       SShort         aStartIdx,
                                       SShort         aEndIdx,
                                       SShort         aShift );

    /* BUG-47206 : findFirstRowInLeaf(), findFirstRowInInternal() �Լ��� �����Ѵ�. */
    static void findFirstRowInNode( const smiCallBack   * aCallBack,
                                    const smnbNode      * aNode,
                                    SInt                  aMinimum,
                                    SInt                  aMaximum,
                                    SInt                * aSlot );

    /* BUG-47206 : findFirstKeyInLeaf(), findFirstKeyInInternal() �Լ��� �����Ѵ�. */
    static void findFirstKeyInNode( smnbHeader          * aHeader,
                                    const smiCallBack   * aCallBack,
                                    const smnbNode      * aNode,
                                    SInt                  aMinimum,
                                    SInt                  aMaximum,
                                    SInt                * aSlot );

    static void findLastRowInInternal( const smiCallBack   * aCallBack,
                                       const smnbINode     * aNode,
                                       SInt                  aMinimum,
                                       SInt                  aMaximum,
                                       SInt                * aSlot );
    static void findLastKeyInInternal( smnbHeader          * aHeader,
                                       const smiCallBack   * aCallBack,
                                       const smnbINode     * aNode,
                                       SInt                  aMinimum,
                                       SInt                  aMaximum,
                                       SInt                * aSlot );
    static void findLastRowInLeaf( const smiCallBack   * aCallBack,
                                   const smnbLNode     * aNode,
                                   SInt                  aMinimum,
                                   SInt                  aMaximum,
                                   SInt                * aSlot );
    static void findLastKeyInLeaf( smnbHeader          * aHeader,
                                   const smiCallBack   * aCallBack,
                                   const smnbLNode     * aNode,
                                   SInt                  aMinimum,
                                   SInt                  aMaximum,
                                   SInt                * aSlot );

    static IDE_RC setDirectKeyIndex( smnbHeader         * aHeader,
                                     smnIndexHeader     * aIndex,
                                     const smiColumn    * aColumn,
                                     UInt                 aMtdAlign );

    /* BUG-47206 : isFullKeyInLeafSlot(), isFullKeyInInternalSlot() �Լ��� �����Ѵ�. */
    static inline idBool isFullKey( smnbHeader      * aIndex,
                                    const smnbNode  * aNode,
                                    SShort            aIdx );

    static iduMemPool   mIteratorPool;

    /* PROJ-2613 Key Redistibution in MRDB Index */
    static inline idBool checkEnableKeyRedistribution( smnbHeader      * aIndex,
                                                       const smnbLNode * aLNode,
                                                       const smnbINode * aINode );
    
    static inline SInt calcKeyRedistributionPosition( smnbHeader    * aIndex,
                                                      SInt            aCurSlotCount,
                                                      SInt            aNxtSlotCount );

    static IDE_RC keyRedistribute( smnbHeader * aIndex,
                                   smnbLNode  * aCurNode,
                                   smnbLNode  * aNxtNode,
                                   SInt         aKeyRedistributeCount );

    static IDE_RC keyRedistributionPropagate( smnbHeader * aIndex,
                                              smnbINode  * aINode,
                                              smnbLNode  * aLNode,
                                              SChar      * aOldRowPtr );
    /* PROJ-2613 end */

    /* BUG-44043, BUG-46428 */
    static inline void findNextSlotInLeaf( smnbHeader       * index,
                                           const smnbLNode  * aNode,
                                           SInt               aMinimum,
                                           SInt               aMaximum,
                                           SChar            * aSearchRow,
                                           SInt             * aSlot );

    static inline idBool checkEnableReorgInNode( smnbLNode * aLNode,
                                                 smnbINode * aINode,
                                                 SShort      aSlotMaxCount );

    static void setInconsistentIndex( smnIndexHeader * aIndex );

    static IDE_RC makeRootNode( smnIndexHeader * aIndex,
                                SChar          * aRow,
                                idBool           aIsUniqueIndex );

    static IDE_RC processNodeSplit( smnIndexHeader * aIndex,
                                    smnbStack      * aStack,
                                    SInt             aDepth,
                                    smnbLNode      * aCurLNode,
                                    SChar          * aRow,
                                    SInt             aSlotPos,
                                    idBool           aIsUniqueIndex );

    static IDE_RC processKeyRedistribution( smnIndexHeader * aIndex, 
                                            smnbINode      * aCurINode,
                                            smnbLNode      * aCurLNode,
                                            idBool         * aIsTreeLatched,
                                            idBool         * aIsNodeLatched );

    static void dumpNodeLink( smnbLNode * aLNode );

public:
    static smmSlotList  mSmnbNodePool;
    static void*        mSmnbFreeNodeList;
    static UInt         mNodeSize;
    static UInt         mNodeSplitRate; /* BUG-40509 */

private:
    static UInt         mIteratorSize;
};

// BUG-19249
inline UInt smnbBTree::getKeySize( UInt aKeyValueSize )
{
    return idlOS::align8( aKeyValueSize + ID_SIZEOF(SChar*));
}

/* PROJ-2433 */
#define SMNB_DEFAULT_MAX_KEY_SIZE() \
    ( smuProperty::getMemBtreeDefaultMaxKeySize() );

/* PROJ-2433 
 *
 * internal node�� �ʱ�ȭ�Ѵ�
 *
 * - direct key index�� ����ϴ��� �׻� row pointer �� �־���Ѵ�
 *   : non-unique index���� ���� key�� ������ insert�Ǹ�, row ������ �ּ� ������ ������ �����Ѵ�.
 *   : uninque index��� �����Ҽ��ְ�����, ������ index ũ�⸦ �����ϴ°��� leaf node �̴�.
 */
inline void smnbBTree::initInternalNode( smnbINode  * a_pNode,
                                         smnbHeader * aIndexHeader,
                                         IDU_LATCH    a_latch )
{
    idlOS::memset( a_pNode,
                   0,
                   mNodeSize );

    a_pNode->prev           = NULL;
    a_pNode->next           = NULL;
    SM_SET_SCN_CONST( &(a_pNode->scn), 0 );
    a_pNode->freeNodeList   = NULL;
    a_pNode->latch          = a_latch;
    a_pNode->flag           = SMNB_NODE_TYPE_INTERNAL;
    a_pNode->prevSPtr       = NULL;
    a_pNode->nextSPtr       = NULL;
    a_pNode->sequence       = 0;
    a_pNode->mKeySize       = aIndexHeader->mKeySize;
    a_pNode->mMaxSlotCount  = aIndexHeader->mINodeMaxSlotCount;
    a_pNode->mSlotCount     = 0;

    /* �׻� row poniter ���� */
    a_pNode->mRowPtrs = (SChar **)&(a_pNode->mChildPtrs[a_pNode->mMaxSlotCount]);

    if ( a_pNode->mKeySize == 0 )
    {
        a_pNode->mKeys = NULL;
    }
    else /* KEY INDEX */
    {
        a_pNode->mKeys = (void  **)&(a_pNode->mRowPtrs[a_pNode->mMaxSlotCount]); 
    }
}

/* PROJ-2433 
 *
 * leaf node�� �ʱ�ȭ�Ѵ�
 */
inline IDE_RC smnbBTree::initLeafNode( smnbLNode  * a_pNode,
                                       smnbHeader * aIndexHeader,
                                       IDU_LATCH    a_latch )
{
    SChar   sBuf[IDU_MUTEX_NAME_LEN];

    // To fix BUG-17337
    idlOS::memset( a_pNode,
                   0,
                   mNodeSize );

    a_pNode->prev        = NULL;
    a_pNode->next        = NULL;
    SM_SET_SCN_CONST( &(a_pNode->scn), 0 );
    a_pNode->freeNodeList= NULL;
    a_pNode->latch       = a_latch;
    a_pNode->flag        = SMNB_NODE_TYPE_LEAF;
    a_pNode->prevSPtr    = NULL;
    a_pNode->nextSPtr    = NULL;

    /* init Node Latch */
    idlOS::snprintf( sBuf, IDU_MUTEX_NAME_LEN, "SMNB_MUTEX_NODE_%"ID_UINT64_FMT,
                     (ULong)a_pNode );
    IDE_TEST( a_pNode->nodeLatch.initialize( sBuf,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    a_pNode->sequence      = 0;
    a_pNode->mKeySize      = aIndexHeader->mKeySize;
    a_pNode->mMaxSlotCount = aIndexHeader->mLNodeMaxSlotCount;
    a_pNode->mSlotCount    = 0;

    a_pNode->mRowPtrs = (SChar **)( ((SChar *)a_pNode) + ID_SIZEOF( smnbLNode ) );

    if ( a_pNode->mKeySize == 0 )
    {
        a_pNode->mKeys = NULL;
    }
    else /* DIRECT KEY INDEX */
    {
        a_pNode->mKeys = (void **)&(a_pNode->mRowPtrs[a_pNode->mMaxSlotCount]);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


inline IDE_RC smnbBTree::destroyNode( smnbNode * a_pNode )
{
    smnbLNode   * s_pLeafNode;

    IDE_DASSERT( a_pNode != NULL );

    if ( SMNB_IS_LEAF_NODE( a_pNode ) )
    {
        s_pLeafNode = (smnbLNode*)a_pNode;
        IDE_TEST( s_pLeafNode->nodeLatch.destroy() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

inline void smnbBTree::lockTree( smnbHeader * a_pIndex )
{
    IDE_DASSERT( a_pIndex != NULL );
    (void)a_pIndex->mTreeMutex.lock( NULL /* idvSQL */ ); /* always IDE_SUCCESS return */
}

inline void smnbBTree::unlockTree( smnbHeader * a_pIndex )
{
    IDE_DASSERT( a_pIndex != NULL );
    (void)a_pIndex->mTreeMutex.unlock(); /* always IDE_SUCCESS return */
}

inline void smnbBTree::lockNode( smnbLNode * a_pLeafNode )
{
    IDE_DASSERT( a_pLeafNode != NULL );
    IDE_DASSERT( SMNB_IS_LEAF_NODE( a_pLeafNode ) );
    (void)a_pLeafNode->nodeLatch.lock( NULL /* idvSQL */ ); /* always IDE_SUCCESS return */
}

inline void smnbBTree::unlockNode( smnbLNode * a_pLeafNode )
{
    IDE_DASSERT( a_pLeafNode != NULL );
    IDE_DASSERT( SMNB_IS_LEAF_NODE( a_pLeafNode ) );
    (void)a_pLeafNode->nodeLatch.unlock(); /* always IDE_SUCCESS return */
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::setInternalSlot                 *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * INTERNAL NODE�� aIdx��° SLOT��
 * child pointer, row pointer, direct key�� �����Ѵ�.
 *
 * - direct key�� memcpy�� �̷�����°Ϳ� ����
 *
 * aNode     - [IN]  INTERNAL NODE
 * aIdx      - [IN]  slot index
 * aChildPtr - [IN]  child pointer
 * aRowPtr   - [IN]  row pointer
 * aKey      - [IN]  direct key pointer
 *********************************************************************/
inline void smnbBTree::setInternalSlot( smnbINode   * aNode,
                                        SShort        aIdx,
                                        smnbNode    * aChildPtr,
                                        SChar       * aRowPtr,
                                        void        * aKey )
{
    IDE_DASSERT( aIdx >= 0 );

    aNode->mChildPtrs[aIdx] = aChildPtr;
    aNode->mRowPtrs  [aIdx] = aRowPtr;

    if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
    { 
        if ( aKey != NULL )
        {
            idlOS::memcpy( SMNB_GET_KEY_PTR( aNode, aIdx ),
                           aKey,
                           aNode->mKeySize ); 
        }
        else
        { 
            idlOS::memset( SMNB_GET_KEY_PTR( aNode, aIdx ),
                           0,
                           aNode->mKeySize ); 
        }
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::getInternalSlot                 *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * INTERNAL NODE�� aIdx��° SLOT��
 * child pointer, row pointer, direct key pointer�� �����´�.
 *
 * aChildPtr - [OUT] child pointer
 * aRowPtr   - [OUT] row pointer
 * aKey      - [OUT] direct key pointer
 * aNode     - [IN]  INTERNAL NODE
 * aIdx      - [IN]  slot index
 *********************************************************************/
inline void smnbBTree::getInternalSlot( smnbNode ** aChildPtr,
                                        SChar    ** aRowPtr,
                                        void     ** aKey,
                                        smnbINode * aNode,
                                        SShort      aIdx )
{
    if ( aChildPtr != NULL )
    {
        *aChildPtr = aNode->mChildPtrs[aIdx];
    }
    else
    {
        /* nothing todo */
    }

    if ( aRowPtr != NULL )
    {
        *aRowPtr = aNode->mRowPtrs[aIdx];
    }
    else
    {
        /* nothing todo */
    }

    if ( aKey != NULL )
    {
        if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
        {
            *aKey = SMNB_GET_KEY_PTR( aNode, aIdx );
        }
        else
        {
            *aKey = NULL; /* ���̼����ȵǾ��ִ°��NULL��ȯ*/
        }
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::copyInternalSlots               *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * INTERNAL NODE ���� Ư�������� slot���� �ٸ� INTERNAL NODE �� �����Ѵ�.
 *
 * - aSrc�� aSrcSTartIdx���� aSrcEndIdx������ slot����
 *   aDest�� aDestStartIdex�� �����Ѵ�.
 * - aSrc, aDest�� �ٸ� NODE �̾�� �Ѵ�.
 *
 * aDest         - [IN] ��� INTERNAL NODE
 * aDestStartIdx - [IN] ��� ���� slot index
 * aSrc          - [IN] ���� INTERNAL NODE
 * aSrcStartIdx  - [IN] ������ ���� slot index
 * aSrcEndIdx    - [IN] ������ ������ slot index
 *********************************************************************/
inline void smnbBTree::copyInternalSlots( smnbINode * aDest,
                                          SShort      aDestStartIdx,
                                          smnbINode * aSrc,
                                          SShort      aSrcStartIdx,
                                          SShort      aSrcEndIdx )
{
    IDE_DASSERT( aSrcStartIdx <= aSrcEndIdx );

    idlOS::memcpy( &aDest->mChildPtrs[aDestStartIdx],
                   &aSrc ->mChildPtrs[aSrcStartIdx],
                   ID_SIZEOF( aDest->mChildPtrs[0] ) * ( aSrcEndIdx - aSrcStartIdx + 1 ) );

    idlOS::memcpy( &aDest->mRowPtrs[aDestStartIdx],
                   &aSrc ->mRowPtrs[aSrcStartIdx],
                   ID_SIZEOF( aDest->mRowPtrs[0] ) * ( aSrcEndIdx - aSrcStartIdx + 1 ) );

    if ( SMNB_IS_DIRECTKEY_IN_NODE( aDest ) == ID_TRUE )
    {
        idlOS::memcpy( SMNB_GET_KEY_PTR( aDest, aDestStartIdx ),
                       SMNB_GET_KEY_PTR( aSrc, aSrcStartIdx ),
                       aDest->mKeySize * ( aSrcEndIdx - aSrcStartIdx + 1 ) );
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::shiftInternalSlots              *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * INTERNAL NODE ������ Ư�������� slot���� shift ��Ų��.
 *
 * - aShift�� ����̸� slot���� ���������� shift �ϰ�,
 *   aShift�� �����̸� slot���� �������� shift �Ѵ�.
 *
 * aNode      - [IN] INTERNAL NODE
 * aStartIdx  - [IN] shift ��ų ���� slot index
 * aEndIdx    - [IN] shift ��ų ������ slot index
 * aShift     - [IN] shift��
 *********************************************************************/
inline void smnbBTree::shiftInternalSlots( smnbINode * aNode,
                                           SShort      aStartIdx,
                                           SShort      aEndIdx,
                                           SShort      aShift )
{
    SInt i = 0;

    IDE_DASSERT( aStartIdx <= aEndIdx );

    /* BUG-41787
     * �������� index node�� slot �̵��� memmove() �Լ��� ����Ͽ��µ�
     * slot �̵��߿� pointer�� �����ϸ� �ϼ����� ���� �ּҰ��� �о segment fault�� �߻��Ҽ�����.
     * �׷���, �Ʒ��� ���� for���� ����Ͽ� slot�� �ϳ��� �̵��ϵ��� �����Ͽ���.
     *
     * direct key�� ���� �ּҰ��� �ƴϹǷ�, ������ ���� ���� */

    if ( aShift > 0 ) 
    {
        for ( i = aEndIdx;
              i >= aStartIdx;
              i-- )
        {
            aNode->mChildPtrs[ i + aShift ] =  aNode->mChildPtrs[ i ];
            aNode->mRowPtrs[ i + aShift ] =  aNode->mRowPtrs[ i ];
        }
    }
    else
    {
        /* nothing todo */
    }

    if ( aShift < 0)
    {
        for ( i = aStartIdx;
              i <= aEndIdx;
              i++ )
        {
            aNode->mChildPtrs[ i + aShift ] =  aNode->mChildPtrs[ i ];
            aNode->mRowPtrs[ i + aShift ] =  aNode->mRowPtrs[ i ];
        }
    }
    else
    {
        /* nothing todo */
    }

    /* Shift direct key */
    if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
    {
        idlOS::memmove( ((SChar *)aNode->mKeys) + ( aNode->mKeySize * ( aStartIdx + aShift ) ),
                        ((SChar *)aNode->mKeys) + ( aNode->mKeySize * ( aStartIdx ) ),
                        aNode->mKeySize * ( aEndIdx - aStartIdx + 1 ) );
    }
    else
    {
        /* nothing todo */
    }

}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::setLeafSlot                     *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * LEAF NODE�� aIdx��° SLOT��
 * row pointer, direct key�� �����Ѵ�.
 *
 * - direct key�� memcpy�� �̷�����°Ϳ� ����
 *
 * aNode     - [IN]  LEAF NODE
 * aIdx      - [IN]  slot index
 * aRowPtr   - [IN]  row pointer
 * aKey      - [IN]  direct key pointer
 *********************************************************************/
inline void smnbBTree::setLeafSlot( smnbLNode * aNode,
                                    SShort      aIdx,
                                    SChar     * aRowPtr,
                                    void      * aKey )
{
    IDE_DASSERT( aIdx >= 0 );

    aNode->mRowPtrs[aIdx] = aRowPtr;

    if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
    {
        if ( aKey != NULL)
        {
            idlOS::memcpy( SMNB_GET_KEY_PTR( aNode, aIdx ),
                           aKey,
                           aNode->mKeySize ); 
        }
        else
        { 
            idlOS::memset( SMNB_GET_KEY_PTR( aNode, aIdx ),
                           0,
                           aNode->mKeySize ); 
        }
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::getLeafSlot                     *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * LEAF NODE�� aIdx��° SLOT��
 * row pointer, direct key pointer�� �����´�.
 *
 * aRowPtr   - [OUT] row pointer
 * aKey      - [OUT] direct key pointer
 * aNode     - [IN]  LEAF NODE
 * aIdx      - [IN]  slot index
 *********************************************************************/
inline void smnbBTree::getLeafSlot( SChar       ** aRowPtr,
                                    void        ** aKey,
                                    smnbLNode   *  aNode,
                                    SShort         aIdx )
{
    if ( aRowPtr != NULL )
    {
        *aRowPtr = aNode->mRowPtrs[aIdx];
    }
    else
    {
        /* nothing todo */
    }

    if ( aKey != NULL )
    {
        if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
        {
            *aKey = SMNB_GET_KEY_PTR( aNode, aIdx );
        }
        else
        {
            *aKey = NULL; /* ���̼����ȵǾ��ִ°��NULL��ȯ*/
        }
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::copyLeafSlots                   *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * LEAF NODE ���� Ư�������� slot���� �ٸ� LEAF NODE �� �����Ѵ�.
 *
 * - aSrc�� aSrcSTartIdx���� aSrcEndIdx������ slot����
 *   aDest�� aDestStartIdex�� �����Ѵ�.
 * - aSrc, aDest�� �ٸ� NODE �̾�� �Ѵ�.
 *
 * aDest         - [IN] ��� LEAF NODE
 * aDestStartIdx - [IN] ��� ���� slot index
 * aSrc          - [IN] ���� LEAF NODE
 * aSrcStartIdx  - [IN] ������ ���� slot index
 * aSrcEndIdx    - [IN] ������ ������ slot index
 *********************************************************************/
inline void smnbBTree::copyLeafSlots( smnbLNode * aDest,
                                      SShort      aDestStartIdx,
                                      smnbLNode * aSrc,
                                      SShort      aSrcStartIdx,
                                      SShort      aSrcEndIdx )
{
    IDE_DASSERT( aSrcStartIdx <= aSrcEndIdx );

    idlOS::memcpy( &aDest->mRowPtrs[aDestStartIdx],
                   &aSrc ->mRowPtrs[aSrcStartIdx],
                   ID_SIZEOF( aDest->mRowPtrs[0] ) * ( aSrcEndIdx - aSrcStartIdx + 1 ) );

    if ( SMNB_IS_DIRECTKEY_IN_NODE( aDest ) == ID_TRUE )
    {
        idlOS::memcpy( SMNB_GET_KEY_PTR( aDest, aDestStartIdx ),
                       SMNB_GET_KEY_PTR( aSrc, aSrcStartIdx ),
                       aDest->mKeySize * ( aSrcEndIdx - aSrcStartIdx + 1 ) );
    }
    else
    {
        /* nothing todo */
    }
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::shiftLeafSlots                  *
 * ------------------------------------------------------------------*
 * PROJ-2433 Direct Key Index
 * LEAF NODE ������ Ư�������� slot���� shift ��Ų��.
 *
 * - aShift�� ����̸� slot���� ���������� shift �ϰ�,
 *   aShift�� �����̸� slot���� �������� shift �Ѵ�.
 *
 * aNode      - [IN] LEAF NODE
 * aStartIdx  - [IN] shift ��ų ���� slot index
 * aEndIdx    - [IN] shift ��ų ������ slot index
 * aShift     - [IN] shift��
 *********************************************************************/
inline void smnbBTree::shiftLeafSlots( smnbLNode * aNode,
                                       SShort      aStartIdx,
                                       SShort      aEndIdx,
                                       SShort      aShift )
{
    SInt i = 0;

    IDE_DASSERT( aStartIdx <= aEndIdx );

    /* BUG-41787
     * �������� index node�� slot �̵��� memmove() �Լ��� ����Ͽ��µ�
     * slot �̵��߿� pointer�� �����ϸ� �ϼ����� ���� �ּҰ��� �о segment fault�� �߻��Ҽ�����.
     * �׷���, �Ʒ��� ���� for���� ����Ͽ� slot�� �ϳ��� �̵��ϵ��� �����Ͽ���.
     *
     * direct key�� ���� �ּҰ��� �ƴϹǷ�, ������ ���� ���� */

    if ( aShift > 0 ) 
    {
        for ( i = aEndIdx;
              i >= aStartIdx;
              i-- )
        {
            aNode->mRowPtrs[ i + aShift ] = aNode->mRowPtrs[ i ];
        }
    }
    else
    {
        /* nothing todo */
    }

    if ( aShift < 0)
    {
        for ( i = aStartIdx;
              i <= aEndIdx;
              i++ )
        {
            aNode->mRowPtrs[ i + aShift ] = aNode->mRowPtrs[ i ];
        }
    }
    else
    {
        /* nothing todo */
    }

    /* Shift direct key */
    if ( SMNB_IS_DIRECTKEY_IN_NODE( aNode ) == ID_TRUE )
    {
        idlOS::memmove( ((SChar *)aNode->mKeys) + ( aNode->mKeySize * ( aStartIdx + aShift ) ),
                        ((SChar *)aNode->mKeys) + ( aNode->mKeySize * ( aStartIdx ) ),
                        aNode->mKeySize * ( aEndIdx - aStartIdx + 1 ) );
    }
    else
    {
        /* nothing todo */
    }
}

/* BUG-40509 Change Memory Index Node Split Rate */
inline void smnbBTree::setNodeSplitRate()
{
    mNodeSplitRate = smuProperty::getMemoryIndexUnbalancedSplitRate();
}
inline UInt smnbBTree::getNodeSplitRate()
{
    return mNodeSplitRate;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::calcKeyRedistributionPosition   *
 * ------------------------------------------------------------------*
 * PROJ-2613 Key Redistribution in MRDB Index
 * Ű ��й谡 ������� ���θ� �Ǵ��Ѵ�.
 * ���� ���θ� �Ǵ��ϴ� ������ ������ ����.
 *  1. MEM_INDEX_KEY_REDISTRIBUTION�� ��������(1)
 *  2. �ش� �ε����� Ű ��й� ����� ����ϵ��� ���õǾ� ����
 *  3. Ű ��й谡 ������ ����� �̿� ��尡 ����
 *  4. �̿� ��尡 Ű ��й谡 ������ ���� �θ� ��尡 ����
 *  5. �̿� ����� �� slot�� ����� ����
 * ���� �ټ����� ������ ��� ������ ��쿡�� Ű ��й踦 �����Ѵ�.
 *
 * aIndex    - [IN] Index Header
 * aLNode    - [IN] Ű ��й踦 �������� �Ǵ��ؾ� �ϴ� leaf node
 * aINode    - [IN] Ű ��й踦 ������ leaf node�� �θ� ���
 *********************************************************************/
inline idBool smnbBTree::checkEnableKeyRedistribution( smnbHeader      * aIndex,
                                                       const smnbLNode * aLNode,
                                                       const smnbINode * aINode )
{
    idBool  sRet = ID_FALSE;

    if ( aINode != NULL )
    {
        if ( smuProperty::getMemIndexKeyRedistribute() == ID_TRUE )
        {
            if ( ( (smnbLNode*)aINode->mChildPtrs[( aINode->mSlotCount ) - 1] ) != aLNode )
            {
                if ( aLNode->nextSPtr != NULL )
                {
                    /* �̿� ��忡 ����� ������ �ִ��� �Ǵ��ϴ� ������
                     * MEM_INDEX_KEY_REDISTRIBUTION_STANDARD_RATE ������Ƽ�� ����Ѵ�. */
                    if ( ( aLNode->nextSPtr->mSlotCount ) <
                         ( ( SMNB_LEAF_SLOT_MAX_COUNT( aIndex ) *
                             smuProperty::getMemIndexKeyRedistributionStandardRate() / 100 ) ) )
                    {
                        sRet = ID_TRUE;
                    }
                    else
                    {
                        /* �̿� ��忡 ����� ������ ���� ��� Ű ��й踦 �������� �ʴ´�. */
                    }
                }
                else
                {
                    /* �̿� ��尡 ���� ���� Ű ��й踦 ������ �� ����. */
                }
            }
            else
            {
                /* �ش� ��尡 �θ����� ������ �ڽ��� ��쿡�� Ű ��й踦 �������� �ʴ´�. */
            }
        }
        else
        {
            /* ������Ƽ�� ���� �ִٸ� Ű ��й踦 �������� �ʴ´�. */
        }
    }
    else
    {
        /* ��Ʈ ��忡 ���� ���� �� ��� �ش� ����� �θ� ��尡 �������� �ʴ´�.
         * �� ���� �̿� ��嵵 ���� ������ Ű ��й踦 �������� �ʴ´�. */
    }
    return sRet;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::calcKeyRedistributionPosition   *
 * ------------------------------------------------------------------*
 * PROJ-2613 Key Redistribution in MRDB Index
 * �ش� ��� �� Ű ��й谡 ���۵� ��ġ�� ����Ѵ�.
 * ���� �̿� ���� �̵��� ���۵� ��ġ�� ����� �����ؾ� �ϳ�
 * �ڵ��� ������ ���� �̵��� ���۵� ��ġ�� �ƴ�, �̵� ��ų slot�� ���� �����ϵ��� �Ѵ�.
 *
 * aIndex         - [IN] Index Header
 * aCurSlotCount  - [IN] Ű ��й谡 ����� ���(src node)�� slot count
 * aNxtSlotCount  - [IN] Ű ��й�� �̵��� ���(dest node)�� slot count
 *********************************************************************/
inline SInt smnbBTree::calcKeyRedistributionPosition( smnbHeader    * aIndex,
                                                      SInt            aCurSlotCount,
                                                      SInt            aNxtSlotCount )
{
    SInt    sStartPosition = 0;

    sStartPosition = ( aCurSlotCount + aNxtSlotCount + 1 ) / 2;

    return (SInt)( SMNB_LEAF_SLOT_MAX_COUNT( aIndex ) - sStartPosition );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : smnbBTree::checkEnableReorgInNode          *
 * ------------------------------------------------------------------*
 * PROJ-2614 Memory Index Reorganization
 * �� ��尣 ���� ���� ���θ� Ȯ���Ͽ� �����Ѵ�.
 *
 * aLNode         - [IN] ���� ������ Ȯ���ϱ� ���� leaf node
 * aINode         - [IN] ���� ��� leaf node�� �θ���
 * aSlotMaxCount  - [IN] ��尡 �ִ� ������ �ִ� slot�� ��
 *********************************************************************/
inline idBool smnbBTree::checkEnableReorgInNode( smnbLNode * aLNode,
                                                 smnbINode * aINode,
                                                 SShort      aSlotMaxCount )
{
    idBool  sEnableReorg = ID_FALSE;
    smnbLNode   * sCurLNode = aLNode;
    smnbLNode   * sNxtLNode = (smnbLNode*)aLNode->nextSPtr;

    IDE_ERROR( aLNode != NULL );
    IDE_ERROR( aINode != NULL );

    if ( sNxtLNode != NULL )
    {
        if ( sNxtLNode->nextSPtr != NULL )
        {
            if ( ( (smnbLNode*)aINode->mChildPtrs[ ( aINode->mSlotCount ) - 1 ] ) != aLNode )
            {
                if ( ( sCurLNode->mSlotCount + sNxtLNode->mSlotCount ) < aSlotMaxCount )
                {
                    sEnableReorg = ID_TRUE;
                }
                else
                {
                    /* �� ��带 ������ �� �ϳ��� ��忡 ���� �ʴ´ٸ�
                     * ������ ������ �� ����. */
                }
            }
            else
            {
                /* ���� ��尡 �θ� ����� ������ �ڽ��̶�� ���� ����
                 * �θ� �ٸ��Ƿ� ������ ������ �� ����. */
            }
        }
        else
        {
            /* fetchNext�� Ʈ���� ������ ��忡�� ��� ���� ��� ������ �߻��� �� �����Ƿ�
             * Ʈ���� ������ ��忡 ���ؼ��� ������ �������� �ʴ´�.*/
        }
    }
    else
    {
        /* ���� ��尡 ���ų� Ʈ���� ������ ����� ������ ������ �� ����. */
    }
    IDE_EXCEPTION_END;

    return sEnableReorg;
}
#endif /* _O_SMNB_MODULE_H_ */
