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
 * $Id: qmnPPCRD.h 90438 2021-04-02 08:20:57Z ahra.cho $
 *
 * Description :
 *     Parallel Partition Coordinator(PPCRD) Node
 *
 *     Partitioned table�� ���� scan�� �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Partition Coordinator
 *
 *     Multi Children(partition�� ���� SCAN) �� ���� Data�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_PPCRD_H_
#define _O_QMN_PPCRD_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncPPCRD.flag�� qmncSCAN.flag�� �����Ѵ�.

//-----------------
// Data Node Flags
//-----------------

// qmndPPCRD.flag
// First Initialization Done
#define QMND_PPCRD_INIT_DONE_MASK           (0x00000001)
#define QMND_PPCRD_INIT_DONE_FALSE          (0x00000000)
#define QMND_PPCRD_INIT_DONE_TRUE           (0x00000001)

#define QMND_PPCRD_ALL_FALSE_MASK           (0x00000002)
#define QMND_PPCRD_ALL_FALSE_FALSE          (0x00000000)
#define QMND_PPCRD_ALL_FALSE_TRUE           (0x00000002)

/* qmndPPCRD.flag                                     */
// Cursor Status
#define QMND_PPCRD_INDEX_CURSOR_MASK        (0x00000004)
#define QMND_PPCRD_INDEX_CURSOR_CLOSED      (0x00000000)
#define QMND_PPCRD_INDEX_CURSOR_OPEN        (0x00000004)

/* qmndPPCRD.flag                                     */
// IN SUBQUERY KEYRANGE�� ���� ���� ����
// IN SUBQUERY KEYRANGE�� ������ ��쿡�� ��ȿ�ϸ�,
// �� �̻� Key Range�� ������ �� ���� �� ����ȴ�.
#define QMND_PPCRD_INSUBQ_RANGE_BUILD_MASK     (0x00000008)
#define QMND_PPCRD_INSUBQ_RANGE_BUILD_SUCCESS  (0x00000000)
#define QMND_PPCRD_INSUBQ_RANGE_BUILD_FAILURE  (0x00000008)

/*
 * BUG-38294
 * mapping info: which SCAN run by which PRLQ
 */
typedef struct qmnPRLQChildMap
{
    qmnPlan* mSCAN;
    qmnPlan* mPRLQ;
} qmnPRLQChildMap;

typedef struct qmncPPCRD
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan          plan;
    UInt             flag;
    UInt             planID;

    //---------------------------------
    // ���� ����
    //---------------------------------

    qmsTableRef    * tableRef;
    UShort           tupleRowID; // Tuple ID

    /* PROJ-2464 hybrid partitioned table ���� */
    UShort           partitionedTupleID;

    UInt             selectedPartitionCount;
    UInt             partitionCount;
    UInt             PRLQCount;
    
    qtcNode        * partitionKeyRange;
    qtcNode        * partitionFilter;
    qtcNode        * nnfFilter;

    // Index
    qcmIndex       * index;                // selected index

    // Index Table�� ����
    UShort           indexTupleRowID;      // index table tuple id
    const void     * indexTableHandle;     // index table handle
    smSCN            indexTableSCN;        // index table SCN

    // Index Table�� Index ����
    qcmIndex       * indexTableIndex;      // real index
    
    // Filter
    qtcNode        * constantFilter;    // Constant Filter 
    qtcNode        * subqueryFilter;    // Subquery Filter
    
    const void     * mPrePruningPartHandle;  // BUG-48800 partition Table Handle
    smSCN            mPrePruningPartSCN;     // BUG-48800 partition Table SCN

    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition  tableOwnerName;     // Table Owner Name
    qmsNamePosition  tableName;          // Table Name
    qmsNamePosition  aliasName;          // Alias Name

    const void     * table;               // Table Handle
    smSCN            tableSCN;            // Table SCN

    //PROJ-2249
    qmnRangeSortedChildren * rangeSortedChildrenArray;
} qmncPPCRD;

typedef struct qmndPPCRD
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan                plan;
    doItFunc                doIt;
    UInt                  * flag;

    //---------------------------------
    // Partition SCAN ���� ����
    //---------------------------------

    smiRange              * partitionFilterArea;
    smiRange              * partitionFilter;
    UInt                    partitionFilterSize;
    
    idBool                  isRowMovementUpdate;

    //---------------------------------
    // Disk Table ���� ����
    //---------------------------------

    void                  * nullRow;  // Disk Table�� ���� null row
    scGRID                  nullRID;

    /* PROJ-2464 hybrid partitioned table ���� */
    void                  * diskRow;

    qmnChildren          ** childrenSCANArea;
    qmnChildren           * childrenPRLQArea;
    UInt                    selectedChildrenCount;
    UInt                    lastChildNo;
    qmnChildren           * curPRLQ;
    qmnChildren           * prevPRLQ;

    // PROJ-1705
    // 1. update, delete�� ��� trigger row�� �ʿ俩�� ���� 
    //   �� ������ cusor open�� fetch column���������� ���̸�,
    //   trigger row�� �ʿ��� ��� ( ���̺� ��ü �÷� ��ġ )
    //   trigger row�� �ʿ����� ���� ��� ( validation�� ������ �÷��� ���ؼ� ��ġ )
    // 2. partitioned table�� update��
    //    rowMovement�� ��� ���̺� ��ü �÷� ��ġ 
    idBool                  isNeedAllFetchColumn;    

    //---------------------------------
    // Index Table Scan�� ���� ����
    //---------------------------------

    mtcTuple              * indexTuple;
    
    // fix BUG-9052
    // subquery filter�� outer column ������
    // outer column�� ������ store and search��
    // ������ϵ��� �ϱ� ���ؼ� ... qmnSCAN::readRow()�Լ��ּ�����
    // printPlan()������ ACCESS count display��
    // DataPlan->plan.myTuple->modify���� �� ���� ���ֵ��� �Ѵ�.
    UInt                    subQFilterDepCnt;

    smiTableCursor          cursor;          // Cursor
    smiCursorProperties     cursorProperty;  // Cursor ���� ����
    smiCursorType           cursorType;      // PROJ-1502 PARTITIONED DISK TABLE
    UInt                    lockMode;        // Lock Mode

    //---------------------------------
    // Update�� ���� Column List
    //---------------------------------

    smiColumnList         * updateColumnList;   // Update�� �ƴ� ���, NULL�� ����
    idBool                  inplaceUpdate;      // inplace update

    //---------------------------------
    // Predicate ����
    //---------------------------------

    // Filter ���� CallBack ����
    smiCallBack             callBack;        // Filter CallBack
    qtcSmiCallBackDataAnd   callBackDataAnd; //
    qtcSmiCallBackData      callBackData[3]; // �� ������ Filter�� ������.
    
    //---------------------------------
    // Merge Join ���� ����
    //---------------------------------

    smiCursorPosInfo        cursorInfo;

    //---------------------------------
    // Child Index ���� ����
    //---------------------------------
    
    qmnChildrenIndex      * childrenIndex;

    // PROJ-2249 range partition filter intersect count
    UInt                  * rangeIntersectCountArray;

    /* BUG-38294 */
    UInt                    mSCANToPRLQMapCount; 
    qmnPRLQChildMap       * mSCANToPRLQMap;
} qmndPPCRD;

class qmnPPCRD
{
public:

    //------------------------
    // Base Function Pointer
    //------------------------

    // �ʱ�ȭ
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ���� �Լ�
    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    // Null Padding
    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );

    // Plan ���� ���
    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

private:

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // ȣ��Ǿ�� �ȵ�
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // �׻� ������ ������ �� ���� ���
    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------
    
    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncPPCRD  * aCodePlan,
                             qmndPPCRD  * aDataPlan );

    // partition filter�� ���� ���� �Ҵ�
    static IDE_RC allocPartitionFilter( qcTemplate * aTemplate,
                                        qmncPPCRD  * aCodePlan,
                                        qmndPPCRD  * aDataPlan );

    
    //------------------------
    // Plan Display ���� �Լ�
    //------------------------
    
    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncPPCRD    * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Partition Key Range�� �� ������ ���
    static IDE_RC printPartKeyRangeInfo( qcTemplate   * aTemplate,
                                         qmncPPCRD    * aCodePlan,
                                         ULong          aDepth,
                                         iduVarString * aString );

    // Partition Filter�� �� ������ ���
    static IDE_RC printPartFilterInfo( qcTemplate   * aTemplate,
                                       qmncPPCRD    * aCodePlan,
                                       ULong          aDepth,
                                       iduVarString * aString );

    // Filter�� �� ������ ���
    static IDE_RC printFilterInfo( qcTemplate   * aTemplate,
                                   qmncPPCRD    * aCodePlan,
                                   ULong          aDepth,
                                   iduVarString * aString );

    static IDE_RC printAccessInfo( qmndPPCRD    * aDataPlan,
                                   iduVarString * aString,
                                   qmnDisplay     aMode );

    static IDE_RC makePartitionFilter( qcTemplate * aTemplate,
                                       qmncPPCRD  * aCodePlan,
                                       qmndPPCRD  * aDataPlan );

    static IDE_RC partitionFiltering( qcTemplate * aTemplate,
                                      qmncPPCRD  * aCodePlan,
                                      qmndPPCRD  * aDataPlan );

    static void makeChildrenSCANArea( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan );
    
    static void makeChildrenPRLQArea( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan );
};

#endif /* _O_QMN_PPCRD_H_ */
