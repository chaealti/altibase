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
 * $Id: qmcMemSortTempTable.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * Memory Sort Temp Table
 *   - Memory Sort Temp Table ��ü�� Quick Sort Algorithm�� ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/

#ifndef _O_QMC_MEM_SORT_TEMP_TABLE_H_
#define _O_QMC_MEM_SORT_TEMP_TABLE_H_  1

#include <idl.h>
#include <iduMemory.h>
#include <idv.h>
#include <mt.h>
#include <qmc.h>
#include <qmcMemory.h>

#define QMC_MEM_SORT_MAX_ELEMENTS                       (1024)
#define QMC_MEM_SORT_SHIFT                                (10)
#define QMC_MEM_SORT_MASK          ID_LONG(0x00000000000003FF)

// BUG-41048 Improve orderby sort algorithm.
// ����Ÿ ������ 16���� �������� insertion sort �� �����ϴ�.
#define QMC_MEM_SORT_SMALL_SIZE                          (16)

// BUG-41048 Improve orderby sort algorithm.
// 3���������� �߰����� ���Ҷ� �Ҽ��� �̿��Ѵ�.
#define QMC_MEM_SORT_PRIME_NUMBER                        (17)

//------------------------------------------------
// TASK-6445 Timsort
//------------------------------------------------

// Run�� �ּҰ��� minrun�� ���Ѽ�
// minrun�� ���Ѽ��� (MINRUN_FENCE / 2) �̴�.
#define QMC_MEM_SORT_MINRUN_FENCE                        (64)
// Galloping Mode ���� Winning Count �ʱⰪ
#define QMC_MEM_SORT_GALLOPING_WINCNT                     (7)
// Galloping Mode ���� Winning Count�� �ּҰ�
#define QMC_MEM_SORT_GALLOPING_WINCNT_MIN                 (1)

//------------------------------------------------
// Sorting�� Stack Overflow ������ ���� �ڷ� ����
//------------------------------------------------

typedef struct qmcSortStack
{
    SLong low;    // QuickSort Partition�� Low ��
    SLong high;   // QuickSort Partition�� High ��
} qmcSortStack;

//------------------------------------------------
// Record���� ������ �����ϱ� ���� �迭
//------------------------------------------------

typedef struct qmcSortArray
{
    qmcSortArray  * parent;
    SLong           depth;
    SLong           numIndex;
    SLong           shift;
    SLong           mask;
    SLong           index;
    qmcSortArray ** elements;
    qmcSortArray  * internalElem[QMC_MEM_SORT_MAX_ELEMENTS];
} qmcSortArray;

//------------------------------------------------
// TASK-6445 Timsort
// �̹� ���ĵ� Run�� ������ �ڷ� ����
//------------------------------------------------
typedef struct qmcSortRun 
{
    SLong mStart;  // ���� ����
    SLong mEnd;    // �� ����
    UInt  mLength; // ����
} qmcSortRun;

//------------------------------------------------
// [Memory Sort Temp Table ��ü]
//    * mSortNode
//        - ORDER BY�� ���� ���, list�� ������ �� ������,
//        - Range�˻��� ���ؼ��� �ϳ��� ������ �� �ִ�.
//------------------------------------------------

typedef struct qmcdMemSortTemp
{
    qcTemplate            * mTemplate;   // Template ����
    qmcMemory             * mMemoryMgr;     // Sort Array�� ���� Memory Mgr
    iduMemory             * mMemory;

    qmcSortArray          * mArray;      // Record������ ���� Sort Array
    SLong                   mIndex;      // ���� Record�� ��ġ

    qmdMtrNode            * mSortNode;   // Sorting�� Column ����

    smiRange              * mRange;      // Range�˻��� ���� Multi-Range ��ü
    smiRange              * mCurRange;   // Multi-Range�� ���� Range
    SLong                   mFirstKey;   // Range�� �����ϴ� Minimum Key
    SLong                   mLastKey;    // Range�� �����ϴ� Maximum Key
    UInt                    mUseOldSort; // BUG-41398
    idvSQL                * mStatistics;  // Session Event

    qmcSortRun            * mRunStack;     // Run�� ������ Stack
    UInt                    mRunStackSize; // Run Stack�� ����� Run�� ����
    qmcSortArray          * mTempArray;    // Merging �� �ʿ��� �ӽ� ����

} qmcdMemSortTemp;

class qmcMemSort
{
public:

    //------------------------------------------------
    // Memory Sort Temp Table�� ����
    //------------------------------------------------

    // �ʱ�ȭ�� �Ѵ�.
    static IDE_RC init( qmcdMemSortTemp * aTempTable,
                        qcTemplate      * aTemplate,
                        iduMemory       * aMemory,
                        qmdMtrNode      * aSortNode );

    // Temp Table���� Sort Array ������ �����ϰ� �ʱ�ȭ�Ѵ�.
    static IDE_RC clear( qmcdMemSortTemp * aTempTable );

    // ��� Record�� Hit Flag�� Reset��
    static IDE_RC clearHitFlag( qmcdMemSortTemp * aTempTable );

    //------------------------------------------------
    // Memory Sort Temp Table�� ����
    //------------------------------------------------

    // �����͸� �߰��Ѵ�.
    static IDE_RC attach( qmcdMemSortTemp * aTempTable,
                          void            * aRow );

    // ������ �����Ѵ�.
    static IDE_RC sort( qmcdMemSortTemp * aTempTable );

    // Limit Sorting�� ������
    static IDE_RC shiftAndAppend( qmcdMemSortTemp * aTempTable,
                                  void            * aRow,
                                  void           ** aReturnRow );

    // Min-Max ������ ���� Limit Sorting�� ������
    static IDE_RC changeMinMax( qmcdMemSortTemp * aTempTable,
                                void            * aRow,
                                void           ** aReturnRow );

    //------------------------------------------------
    // Memory Sort Temp Table�� �˻�
    //------------------------------------------------

    // �ش� ��ġ�� �����͸� �����´�.
    static IDE_RC getElement( qmcdMemSortTemp * aTempTable,
                              SLong aIndex,
                              void ** aElement );

    //----------------------------
    // ���� �˻�
    //----------------------------

    // ù��° ���� �˻�
    static IDE_RC getFirstSequence( qmcdMemSortTemp * aTempTable,
                                    void           ** aRow );

    // ���� ���� �˻�
    static IDE_RC getNextSequence( qmcdMemSortTemp * aTempTable,
                                   void           ** aRow );
    //----------------------------
    // Range �˻�
    //----------------------------

    // ù��° Range �˻�
    static IDE_RC getFirstRange( qmcdMemSortTemp * aTempTable,
                                 smiRange        * aRange,
                                 void           ** aRow );

    // ���� Range �˻�
    static IDE_RC getNextRange( qmcdMemSortTemp * aTempTable,
                                void           ** aRow );

    //----------------------------
    // Hit �˻�
    //----------------------------

    // ù��° Hit �˻�
    static IDE_RC getFirstHit( qmcdMemSortTemp * aTempTable,
                               void           ** aRow );

    // ���� Hit �˻�
    static IDE_RC getNextHit( qmcdMemSortTemp * aTempTable,
                              void           ** aRow );

    //----------------------------
    // Non-Hit �˻�
    //----------------------------

    // ù��° Non-Hit �˻�
    static IDE_RC getFirstNonHit( qmcdMemSortTemp * aTempTable,
                                  void           ** aRow );

    // ���� Non-Hit �˻�
    static IDE_RC getNextNonHit( qmcdMemSortTemp * aTempTable,
                                 void           ** aRow );

    //------------------------------------------------
    // Memory Sort Temp Table�� ��Ÿ �Լ�
    //------------------------------------------------

    // ��ü �������� ������ �����ش�.
    static IDE_RC getNumOfElements( qmcdMemSortTemp * aTempTable,
                                    SLong           * aNumElements );

    // ���� Cursor ��ġ ����
    static IDE_RC getCurrPosition( qmcdMemSortTemp * aTempTable,
                                   SLong           * aPosition );

    // ���� Cursor ��ġ�� ����
    static IDE_RC setCurrPosition( qmcdMemSortTemp * aTempTable,
                                   SLong             aPosition );

    //------------------------------------------------
    // Window Sort (WNST)�� ���� ���
    //------------------------------------------------
     
    // ���� ����Ű(sortNode)�� ����
    static IDE_RC setSortNode( qmcdMemSortTemp  * aTempTable,
                               const qmdMtrNode * aSortNode );

    /* PROJ-2334 PMT memory variable column */
    static void changePartitionColumn( qmdMtrNode  * aNode,
                                       void        * aData );
    
private:

    //------------------------------------------------
    // Record ���� ���� �Լ�
    //------------------------------------------------

    // ������ ��ġ�� Record ���� ��ġ�� �˻��Ѵ�.
    static void   get( qmcdMemSortTemp * aTempTable,
                       qmcSortArray* aArray,
                       SLong aIndex,
                       void*** aElement );

    // Sort Array�� ���� ������ ������Ų��.
    static IDE_RC increase( qmcdMemSortTemp * aTempTable,
                            qmcSortArray* aArray );

    // Sort Array�� ���� ������ ������Ų��.
    static IDE_RC increase( qmcdMemSortTemp * aTempTable,
                            qmcSortArray* aArray,
                            SLong * aReachEnd );

    //------------------------------------------------
    // Sorting ���� �Լ�
    //------------------------------------------------

    // ������ ��ġ �������� quick sorting�� �����Ѵ�.
    static IDE_RC quicksort( qmcdMemSortTemp * aTempTable,
                             SLong aLow,
                             SLong aHigh );

    // Partition�������� ������ �����Ѵ�.
    static IDE_RC partition( qmcdMemSortTemp * aTempTable,
                             SLong    aLow,
                             SLong    aHigh,
                             SLong  * aPartition,
                             idBool * aAllEqual );

    // �� Record���� �� �Լ�(Sorting �ÿ��� ���)
    static SInt   compare( qmcdMemSortTemp * aTempTable,
                           const void      * aElem1,
                           const void      * aElem2 );

    //------------------------------------------------
    // Sequential �˻� ���� �Լ�
    //------------------------------------------------

    // �˻��� ���� ���� Record�� ��ġ�� �̵��Ѵ�.
    static IDE_RC beforeFirstElement( qmcdMemSortTemp * aTempTable );

    // ���� Record�� �˻��Ѵ�.
    static IDE_RC getNextElement( qmcdMemSortTemp * aTempTable,
                                  void ** aElement );

    //------------------------------------------------
    // Range �˻� ���� �Լ�
    //------------------------------------------------

    // Key Range�� �����Ѵ�.
    static void   setKeyRange( qmcdMemSortTemp * aTempTable,
                               smiRange * aRange );

    // Key Range�� �����ϴ� ������ Record�� �˻��Ѵ�.
    static IDE_RC getFirstKey( qmcdMemSortTemp * aTempTable,
                               void ** aElement );

    // Key Range�� �����ϴ� ���� Record�� �˻��Ѵ�.
    static IDE_RC getNextKey( qmcdMemSortTemp * aTempTable,
                              void ** aElement );

    // Range�� �����ϴ� Minimum Record�� ��ġ�� ����
    static IDE_RC setFirstKey( qmcdMemSortTemp * aTempTable,
                               idBool * aResult );

    // Range�� �����ϴ� Maximum Record�� ��ġ�� ����
    static IDE_RC setLastKey( qmcdMemSortTemp * aTempTable );

    // Limit Sorting���� ������ ��ġ �˻�
    static IDE_RC findInsertPosition( qmcdMemSortTemp * aTempTable,
                                      void            * aRow,
                                      SLong           * aInsertPos );

    //--------------------------------------------------
    // TASK-6445 Timsort
    //--------------------------------------------------

    static IDE_RC timsort( qmcdMemSortTemp * aTempTable,
                           SLong aLow,
                           SLong aHigh );

    static SLong calcMinrun( SLong aLength );


    static void searchRun( qmcdMemSortTemp * aTempTable,
                           SLong             aStart,
                           SLong             aFence,
                           SLong           * aRunLength,
                           idBool          * aReverseOrder );

    static IDE_RC insertionSort( qmcdMemSortTemp * aTempTable,
                                 SLong             aLow, 
                                 SLong             aHigh );

    static void reverseOrder( qmcdMemSortTemp * aTempTable,
                              SLong             aLow, 
                              SLong             aHigh );

    static IDE_RC collapseStack( qmcdMemSortTemp * aTempTable );

    static IDE_RC collapseStackForce( qmcdMemSortTemp * aTempTable );

    static IDE_RC mergeRuns( qmcdMemSortTemp * aTempTable,
                             UInt              aRunStackIdx );

    static SLong gallopLeft( qmcdMemSortTemp  * aTempTable,
                             qmcSortArray     * aArray,
                             void            ** aKey,
                             SLong              aBase,
                             SLong              aLength,
                             SLong              aHint );

    static SLong gallopRight( qmcdMemSortTemp  * aTempTable,
                              qmcSortArray     * aArray,
                              void            ** aKey,
                              SLong              aBase,
                              SLong              aLength,
                              SLong              aHint );

    static IDE_RC mergeLowerRun( qmcdMemSortTemp * aTempTable,
                                 SLong             aBase1,
                                 SLong             aLen1,
                                 SLong             aBase2,
                                 SLong             aLen2 );

    static IDE_RC mergeHigherRun( qmcdMemSortTemp * aTempTable,
                                  SLong             aBase1,
                                  SLong             aLen1,
                                  SLong             aBase2,
                                  SLong             aLen2 );

    // Utility
    static IDE_RC moveArrayToArray( qmcdMemSortTemp * aTempTable,
                                    qmcSortArray    * aSrcArray,
                                    SLong             aSrcBase,
                                    qmcSortArray    * aDestArray,
                                    SLong             aDestBase,
                                    SLong             aMoveLength,
                                    idBool            aIsOverlapped ); 

    static void getLeafArray( qmcSortArray  * aArray,
                              SLong           aIndex,
                              qmcSortArray ** aRetArray,
                              SLong         * aRetIdx );
};

#endif /* _O_QMC_MEM_SORT_TEMP_TABLE_H_ */
