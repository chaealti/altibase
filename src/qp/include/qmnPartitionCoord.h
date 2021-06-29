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
 * $Id: qmnPartitionCoord.h 20233 2007-02-01 01:58:21Z shsuh $
 *
 * Description :
 *     Partition Coordinator(PCRD) Node
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

#ifndef _O_QMN_PARTITION_COORD_H_
#define _O_QMN_PARTITION_COORD_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncPCRD.flag�� qmncSCAN.flag�� �����Ѵ�.

//-----------------
// Data Node Flags
//-----------------

// qmndPCRD.flag
// First Initialization Done
#define QMND_PCRD_INIT_DONE_MASK           (0x00000001)
#define QMND_PCRD_INIT_DONE_FALSE          (0x00000000)
#define QMND_PCRD_INIT_DONE_TRUE           (0x00000001)

#define QMND_PCRD_ALL_FALSE_MASK           (0x00000002)
#define QMND_PCRD_ALL_FALSE_FALSE          (0x00000000)
#define QMND_PCRD_ALL_FALSE_TRUE           (0x00000002)

/* qmndPCRD.flag                                     */
// Cursor Status
#define QMND_PCRD_INDEX_CURSOR_MASK        (0x00000004)
#define QMND_PCRD_INDEX_CURSOR_CLOSED      (0x00000000)
#define QMND_PCRD_INDEX_CURSOR_OPEN        (0x00000004)

/* qmndPCRD.flag                                     */
// IN SUBQUERY KEYRANGE�� ���� ���� ����
// IN SUBQUERY KEYRANGE�� ������ ��쿡�� ��ȿ�ϸ�,
// �� �̻� Key Range�� ������ �� ���� �� ����ȴ�.
#define QMND_PCRD_INSUBQ_RANGE_BUILD_MASK     (0x00000008)
#define QMND_PCRD_INSUBQ_RANGE_BUILD_SUCCESS  (0x00000000)
#define QMND_PCRD_INSUBQ_RANGE_BUILD_FAILURE  (0x00000008)

class qmcInsertCursor;

typedef struct qmncPCRD
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
    
    // Key Range
    qtcNode        * fixKeyRange;          // Fixed Key Range
    qtcNode        * fixKeyRange4Print;    // Printable Fixed Key Range
    qtcNode        * varKeyRange;          // Variable Key Range
    qtcNode        * varKeyRange4Filter;   // Variable Fixed Key Range

    // Key Filter
    qtcNode        * fixKeyFilter;         // Fixed Key Filter
    qtcNode        * fixKeyFilter4Print;   // Printable Fixed Key Filter
    qtcNode        * varKeyFilter;         // Variable Key Filter
    qtcNode        * varKeyFilter4Filter;  // Variable Fixed Key Filter

    // Filter
    qtcNode        * constantFilter;    // Constant Filter
    qtcNode        * filter;            // Filter
    qtcNode        * lobFilter;         // Lob Filter ( BUG-25916 ) 
    qtcNode        * subqueryFilter;    // Subquery Filter
   
    /* PROJ-2705 memory parittion simple query */
    idBool           mIsSimple;          // simple index scan or simple full scan
    qmnValueInfo     mSimpleValues;
    UInt             mSimpleCompareOpCount;
    
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
} qmncPCRD;

typedef struct qmndPCRD
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

    qmnChildren          ** childrenArea;
    UInt                    selectedChildrenCount;
    UInt                    curChildNo;

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

    smiRange              * fixKeyRangeArea; // Fixed Key Range ����
    smiRange              * fixKeyRange;     // Fixed Key Range
    UInt                    fixKeyRangeSize; // Fixed Key Range ũ��

    smiRange              * fixKeyFilterArea; //Fixed Key Filter ����
    smiRange              * fixKeyFilter;    // Fixed Key Filter
    UInt                    fixKeyFilterSize;// Fixed Key Filter ũ��

    smiRange              * varKeyRangeArea; // Variable Key Range ����
    smiRange              * varKeyRange;     // Variable Key Range
    UInt                    varKeyRangeSize; // Variable Key Range ũ��

    smiRange              * varKeyFilterArea; //Variable Key Filter ����
    smiRange              * varKeyFilter;    // Variable Key Filter
    UInt                    varKeyFilterSize;// Variable Key Filter ũ��

    smiRange              * notNullKeyRange; // Not Null Key Range
    
    smiRange              * keyRange;        // ���� Key Range
    smiRange              * keyFilter;       // ���� Key Filter

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
    
} qmndPCRD;

class qmnPCRD
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

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // �⺻���� �Լ�.
    // TODO1502:
    // doItFirst, doItNext�� �ɰ����� ��.

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
    // Direct External Call
    //------------------------

    // Cursor�� ������
    static IDE_RC storeCursor( qcTemplate * aTemplate,
                               qmnPlan    * aPlan );

    // ������ Cursor ��ġ�� ������
    static IDE_RC restoreCursor( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan );

    // khshim moved this function from private section to public.
    // for Referential integrity check upon delete/update
    // Key Range, Key Filter, Filter ����
    static IDE_RC makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                         qmncPCRD   * aCodePlan,
                                         qmndPCRD   * aDataPlan );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------
    
    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncPCRD   * aCodePlan,
                             qmndPCRD   * aDataPlan );

    // partition filter�� ���� ���� �Ҵ�
    static IDE_RC allocPartitionFilter( qcTemplate * aTemplate,
                                        qmncPCRD   * aCodePlan,
                                        qmndPCRD   * aDataPlan );

    // Fixed Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyRange( qcTemplate * aTemplate,
                                    qmncPCRD   * aCodePlan,
                                    qmndPCRD   * aDataPlan );

    // Fixed Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyFilter( qcTemplate * aTemplate,
                                     qmncPCRD   * aCodePlan,
                                     qmndPCRD   * aDataPlan );

    // Variable Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyRange( qcTemplate * aTemplate,
                                    qmncPCRD   * aCodePlan,
                                    qmndPCRD   * aDataPlan );

    // Variable Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyFilter( qcTemplate * aTemplate,
                                     qmncPCRD   * aCodePlan,
                                     qmndPCRD   * aDataPlan );

    // Not Null Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocNotNullKeyRange( qcTemplate * aTemplate,
                                        qmncPCRD   * aCodePlan,
                                        qmndPCRD   * aDataPlan );
    
    //------------------------
    // Plan Display ���� �Լ�
    //------------------------
    
    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncPCRD     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Partition Key Range�� �� ������ ���
    static IDE_RC printPartKeyRangeInfo( qcTemplate   * aTemplate,
                                         qmncPCRD     * aCodePlan,
                                         ULong          aDepth,
                                         iduVarString * aString );

    // Partition Filter�� �� ������ ���
    static IDE_RC printPartFilterInfo( qcTemplate   * aTemplate,
                                       qmncPCRD     * aCodePlan,
                                       ULong          aDepth,
                                       iduVarString * aString );

    // Key Range�� �� ������ ���
    static IDE_RC printKeyRangeInfo( qcTemplate   * aTemplate,
                                     qmncPCRD     * aCodePlan,
                                     ULong          aDepth,
                                     iduVarString * aString );

    // Key Filter�� �� ������ ���
    static IDE_RC printKeyFilterInfo( qcTemplate   * aTemplate,
                                      qmncPCRD     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Filter�� �� ������ ���
    static IDE_RC printFilterInfo( qcTemplate   * aTemplate,
                                   qmncPCRD     * aCodePlan,
                                   ULong          aDepth,
                                   iduVarString * aString );

    static IDE_RC printAccessInfo( qmndPCRD     * aDataPlan,
                                   iduVarString * aString,
                                   qmnDisplay     aMode );

    //------------------------
    // PCRD�� doIt() ���� �Լ�
    //------------------------
    
    static IDE_RC readRow( qcTemplate * aTemplate,
                           qmncPCRD   * aCodePlan,
                           qmndPCRD   * aDataPlan,
                           qmcRowFlag * aFlag );

    static IDE_RC makePartitionFilter( qcTemplate * aTemplate,
                                       qmncPCRD   * aCodePlan,
                                       qmndPCRD   * aDataPlan );

    static IDE_RC partitionFiltering( qcTemplate * aTemplate,
                                      qmncPCRD   * aCodePlan,
                                      qmndPCRD   * aDataPlan );

    static IDE_RC makeChildrenArea( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan );
    
    static IDE_RC makeChildrenIndex( qmndPCRD   * aDataPlan );
    
    // index cursor�� partition row�� �д´�.
    static IDE_RC readRowWithIndex( qcTemplate * aTemplate,
                                    qmncPCRD   * aCodePlan,
                                    qmndPCRD   * aDataPlan,
                                    qmcRowFlag * aFlag );

    // index cursor�� index row�� �д´�.
    static IDE_RC readIndexRow( qcTemplate * aTemplate,
                                qmncPCRD   * aCodePlan,
                                qmndPCRD   * aDataPlan,
                                void      ** aIndexRow );
    
    // IN Subquery KeyRange�� ���� ��õ�
    static IDE_RC reReadIndexRow4InSubRange( qcTemplate * aTemplate,
                                             qmncPCRD   * aCodePlan,
                                             qmndPCRD   * aDataPlan,
                                             void      ** aIndexRow );
    
    // index row�� partition row�� �д´�.
    static IDE_RC readRowWithIndexRow( qcTemplate * aTemplate,
                                       qmndPCRD   * aDataPlan,
                                       qmcRowFlag * aFlag );
    
    //------------------------
    // Cursor ���� �Լ�
    //------------------------

    // Cursor�� Open
    static IDE_RC openIndexCursor( qcTemplate * aTemplate,
                                   qmncPCRD   * aCodePlan,
                                   qmndPCRD   * aDataPlan );

    // Cursor�� Restart
    static IDE_RC restartIndexCursor( qmncPCRD   * aCodePlan,
                                      qmndPCRD   * aDataPlan );
};

#endif /* _O_QMN_PARTITION_COORD_H_ */
