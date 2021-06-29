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
 * $Id: qmnScan.h 89517 2020-12-15 06:08:38Z donovan.seo $
 *
 * Description :
 *     SCAN Node
 *     ������ �𵨿��� selection�� �����ϴ� Plan Node �̴�.
 *     Storage Manager�� ���� �����Ͽ� selection�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_SCAN_H_
#define _O_QMN_SCAN_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qmx.h>

//-----------------
// Code Node Flags
//-----------------

/* qmncSCAN.flag                                     */
// Iterator Traverse Direction
// ���� : smiDef.h �� �ִ� ���� ����ϴ� ���� Buggy��.
#define QMNC_SCAN_TRAVERSE_MASK            (0x00000001)
#define QMNC_SCAN_TRAVERSE_FORWARD         (0x00000000)
#define QMNC_SCAN_TRAVERSE_BACKWARD        (0x00000001)

/* qmncSCAN.flag                                     */
// IN Subquery�� Key Range�� ���ǰ� �ִ����� ����
#define QMNC_SCAN_INSUBQ_KEYRANGE_MASK     (0x00000002)
#define QMNC_SCAN_INSUBQ_KEYRANGE_FALSE    (0x00000000)
#define QMNC_SCAN_INSUBQ_KEYRANGE_TRUE     (0x00000002)

/* qmncSCAN.flag                                     */
// Foreign Key �˻縦 ���� Previous ��� ����
#define QMNC_SCAN_PREVIOUS_ENABLE_MASK     (0x00000004)
#define QMNC_SCAN_PREVIOUS_ENABLE_FALSE    (0x00000000)
#define QMNC_SCAN_PREVIOUS_ENABLE_TRUE     (0x00000004)

/* qmncSCAN.flag                                     */
// �����Ǵ� table�� fixed table �Ǵ� performance view������ ������ ��Ÿ��.
// fixed table �Ǵ� performance view�� ���ؼ���
// table�� ���� IS LOCK�� ���� �ʵ��� �ϱ� ����.
#define QMNC_SCAN_TABLE_FV_MASK            (0x00000008)
#define QMNC_SCAN_TABLE_FV_FALSE           (0x00000000)
#define QMNC_SCAN_TABLE_FV_TRUE            (0x00000008)

// Proj-1360 Queue
/* qmncSCAN.flag                                     */
// �����Ǵ� table queue �ΰ��, dequeue �Ŀ� ���ڵ� ������ ��Ÿ��
#define QMNC_SCAN_TABLE_QUEUE_MASK         (0x00000010)
#define QMNC_SCAN_TABLE_QUEUE_FALSE        (0x00000000)
#define QMNC_SCAN_TABLE_QUEUE_TRUE         (0x00000010)

// PROJ-1502 PARTITIONED DISK TABLE
/* qmncSCAN.flag                                     */
// partition�� ���� scan���� ���θ�
#define QMNC_SCAN_FOR_PARTITION_MASK       (0x00000020)
#define QMNC_SCAN_FOR_PARTITION_FALSE      (0x00000000)
#define QMNC_SCAN_FOR_PARTITION_TRUE       (0x00000020)

// To fix BUG-12742
// index�� �ݵ�� ����ؾ� �ϴ��� ���θ� ��Ÿ����.
#define QMNC_SCAN_FORCE_INDEX_SCAN_MASK    (0x00000040)
#define QMNC_SCAN_FORCE_INDEX_SCAN_FALSE   (0x00000000)
#define QMNC_SCAN_FORCE_INDEX_SCAN_TRUE    (0x00000040)

// Indexable MIN-MAX ����ȭ�� ����� ���
#define QMNC_SCAN_NOTNULL_RANGE_MASK       (0x00000080)
#define QMNC_SCAN_NOTNULL_RANGE_FALSE      (0x00000000)
#define QMNC_SCAN_NOTNULL_RANGE_TRUE       (0x00000080)

// PROJ-1624 global non-partitioned index
// index table scan ���θ� ��Ÿ����.
#define QMNC_SCAN_INDEX_TABLE_SCAN_MASK    (0x00000200)
#define QMNC_SCAN_INDEX_TABLE_SCAN_FALSE   (0x00000000)
#define QMNC_SCAN_INDEX_TABLE_SCAN_TRUE    (0x00000200)

// PROJ-1624 global non-partitioned index
// rid scan�� �ݵ�� ����ؾ� �ϴ��� ���θ� ��Ÿ����.
#define QMNC_SCAN_FORCE_RID_SCAN_MASK      (0x00000400)
#define QMNC_SCAN_FORCE_RID_SCAN_FALSE     (0x00000000)
#define QMNC_SCAN_FORCE_RID_SCAN_TRUE      (0x00000400)

/* PROJ-1832 New database link */
#define QMNC_SCAN_REMOTE_TABLE_MASK        (0x00000800)
#define QMNC_SCAN_REMOTE_TABLE_FALSE       (0x00000000)
#define QMNC_SCAN_REMOTE_TABLE_TRUE        (0x00000800)

/* BUG-37077 REMOTE_TABLE_STORE */
#define QMNC_SCAN_REMOTE_TABLE_STORE_MASK  (0x00001000)
#define QMNC_SCAN_REMOTE_TABLE_STORE_FALSE (0x00000000)
#define QMNC_SCAN_REMOTE_TABLE_STORE_TRUE  (0x00001000)

/* session temporary table */
#define QMNC_SCAN_TEMPORARY_TABLE_MASK     (0x00002000)
#define QMNC_SCAN_TEMPORARY_TABLE_FALSE    (0x00000000)
#define QMNC_SCAN_TEMPORARY_TABLE_TRUE     (0x00002000)

/* BUG-42639 Mornitaring query */
#define QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK  (0x00004000)
#define QMNC_SCAN_FAST_SELECT_FIXED_TABLE_FALSE (0x00000000)
#define QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE  (0x00004000)

//-----------------
// Data Node Flags
//-----------------

/* qmndSCAN.flag                                     */
// First Initialization Done
#define QMND_SCAN_INIT_DONE_MASK           (0x00000001)
#define QMND_SCAN_INIT_DONE_FALSE          (0x00000000)
#define QMND_SCAN_INIT_DONE_TRUE           (0x00000001)

/* qmndSCAN.flag                                     */
// Cursor Status
#define QMND_SCAN_CURSOR_MASK              (0x00000002)
#define QMND_SCAN_CURSOR_CLOSED            (0x00000000)
#define QMND_SCAN_CURSOR_OPEN              (0x00000002)

/* qmndSCAN.flag                                     */
// Constant Filter�� ���� ���
#define QMND_SCAN_ALL_FALSE_MASK           (0x00000004)
#define QMND_SCAN_ALL_FALSE_FALSE          (0x00000000)
#define QMND_SCAN_ALL_FALSE_TRUE           (0x00000004)

/* qmndSCAN.flag                                     */
// IN SUBQUERY KEYRANGE�� ���� ���� ����
// IN SUBQUERY KEYRANGE�� ������ ��쿡�� ��ȿ�ϸ�,
// �� �̻� Key Range�� ������ �� ���� �� ����ȴ�.
#define QMND_SCAN_INSUBQ_RANGE_BUILD_MASK     (0x00000008)
#define QMND_SCAN_INSUBQ_RANGE_BUILD_SUCCESS  (0x00000000)
#define QMND_SCAN_INSUBQ_RANGE_BUILD_FAILURE  (0x00000008)

/* qmndSCAN.flag                                     */
// plan�� selected method�� ���õǾ����� ���θ� ���Ѵ�.
#define QMND_SCAN_SELECTED_METHOD_SET_MASK    (0x00000010)
#define QMND_SCAN_SELECTED_METHOD_SET_FALSE   (0x00000000)
#define QMND_SCAN_SELECTED_METHOD_SET_TRUE    (0x00000010)

/* qmndSCAN.flag                                     */
// doItFirst���� cursor�� restart �ʿ� ���θ� ���Ѵ�.
#define QMND_SCAN_RESTART_CURSOR_MASK       (0x00000020)
#define QMND_SCAN_RESTART_CURSOR_FALSE      (0x00000000)
#define QMND_SCAN_RESTART_CURSOR_TRUE       (0x00000020)

/* PROJ-2632 */
#define QMND_SCAN_SERIAL_EXECUTE_MASK       (0x00000040)
#define QMND_SCAN_SERIAL_EXECUTE_FALSE      (0x00000000)
#define QMND_SCAN_SERIAL_EXECUTE_TRUE       (0x00000040)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT * FROM T1 WHERE i2 > 3;
 *      ---------------------
 *      | i1 | i2 | i3 | i4 |
 *      ~~~~~~~~~~~~~~~~~~~~~
 *       mtc->mtc->mtc->mtc
 *        |    |    |    |
 *       qtc  qtc  qtc  qtc
 ----------------------------------------------------------------------*/

// PROJ-1446 Host variable�� ������ ���� ����ȭ
// qmncSCAN�� ������ �ִ� ���� filter/key range ��������
// �ϳ��� �ڷᱸ���� ����
typedef struct qmncScanMethod
{
    qcmIndex     * index;

    //---------------------------------
    // Predicate ����
    //---------------------------------

    // Key Range
    qtcNode      * fixKeyRange;          // Fixed Key Range
    qtcNode      * fixKeyRange4Print;    // Printable Fixed Key Range
    qtcNode      * varKeyRange;          // Variable Key Range
    qtcNode      * varKeyRange4Filter;   // Variable Fixed Key Range

    // Key Filter
    qtcNode      * fixKeyFilter;         // Fixed Key Filter
    qtcNode      * fixKeyFilter4Print;   // Printable Fixed Key Filter
    qtcNode      * varKeyFilter;         // Variable Key Filter
    qtcNode      * varKeyFilter4Filter;  // Variable Fixed Key Filter

    // Filter
    qtcNode      * constantFilter;    // Constant Filter
    qtcNode      * filter;            // Filter
    qtcNode      * lobFilter;       // Lob Filter ( BUG-25916 ) 
    qtcNode      * subqueryFilter;    // Subquery Filter

    // PROJ-1789 PROWID
    qtcNode      * ridRange;
} qmncScanMethod;

typedef struct qmncSCAN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // SCAN ���� ����
    //---------------------------------

    UShort         tupleRowID;          // Tuple ID

    const void   * table;               // Table Handle
    smSCN          tableSCN;            // Table SCN

    qcmAccessOption accessOption;       /* PROJ-2359 Table/Partition Access Option */

    void         * dumpObject;          // PROJ-1618 On-line Dump

    UInt           lockMode;            // Lock Mode
    smiCursorProperties cursorProperty; // Cursor Property
    qmsLimit     * limit;               // qmsParseTree�� limit�� ����

    /* PROJ-1832 New database link */
    SChar        * databaseLinkName;
    SChar        * remoteQuery;
    
    // PROJ-2551 simple query ����ȭ
    idBool         isSimple;    // simple index scan or simple full scan
    UShort         simpleValueCount;
    qmnValueInfo * simpleValues;
    UInt           simpleValueBufSize;  // c-type value�� mt-type value�� ��ȯ
    idBool         simpleUnique;  // �ִ� 1���� ���
    idBool         simpleRid;     // rid scan�� ���
    UInt           simpleCompareOpCount;
    mtcColumn    * mSimpleColumns;
    
    //---------------------------------
    // Predicate ����
    //---------------------------------
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� ������ �ִ� ���� filter/key range���� ������
    // qmncScanMethod �ڷ� ������ ����
    qmncScanMethod  method;

    // NNF Filter
    qtcNode      * nnfFilter;

    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition      remoteUserName;     // Remote User Name
    qmsNamePosition      remoteObjectName;   // Remote Object Name
    
    qmsNamePosition      tableOwnerName;     // Table Owner Name
    qmsNamePosition      tableName;          // Table Name
    qmsNamePosition      aliasName;          // Alias Name

    /* BUG-44520 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Partition Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Partition Name�� �����ϵ��� �����մϴ�.
     */
    SChar                partitionName[QC_MAX_OBJECT_NAME_LEN + 1];

    /* BUG-44633 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Index ID�� �����ϵ��� �����մϴ�.
     */
    UInt                 partitionIndexId;

    // PROJ-1502 PARTITIONED DISK TABLE
    qmsPartitionRef    * partitionRef;
    qmsTableRef        * tableRef;

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    qmoScanDecisionFactor * sdf;

    /* PROJ-2632 */
    UInt                  mSerialFilterOffset; /* in Execute, Dataplan Offset */
    UInt                  mSerialFilterSize;   /* in Execute, Total Entry Allocate Size */
    UInt                  mSerialFilterCount;  /* in Execute, Total Entry Set Count */
    mtxSerialFilterInfo * mSerialFilterInfo;

} qmncSCAN;

typedef struct qmndScanFixedTable
{
    UChar               * recRecord;
    UChar               * traversePtr;
    ULong                 readRecordPos;
    iduFixedTableMemory   memory;
} qmndScanFixedTable;
//-----------------------------------------------------------------
// [Data ������ SCAN ���]
// - Predicate ����
//    A3�� �޸� ���� Key Range Pointer�� �����Ѵ�.
//    �̴� IN SUBQUERY KEYRANGE��� ���� Key Range ������
//    ����, ���а� �Բ� �߻��� �� �־� Variable Key Range������
//    ����� �� �ֱ� �����̴�.
//-----------------------------------------------------------------
typedef struct qmndSCAN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan                    plan;
    doItFunc                    doIt;
    UInt                      * flag;

    //---------------------------------
    // SCAN ���� ����
    //---------------------------------

    // fix BUG-9052
    // subquery filter�� outer column ������
    // outer column�� ������ store and search��
    // ������ϵ��� �ϱ� ���ؼ� ... qmnSCAN::readRow()�Լ��ּ�����
    // printPlan()������ ACCESS count display��
    // DataPlan->plan.myTuple->modify���� �� ���� ���ֵ��� �Ѵ�.
    UInt                        subQFilterDepCnt;

    // PROJ-1071
    smiTableCursor            * cursor;          // Cursor
    smiCursorProperties         cursorProperty;  // Cursor ���� ����
    smiCursorType               cursorType;      // PROJ-1502 PARTITIONED DISK TABLE
    UInt                        lockMode;        // Lock Mode
    qmndScanFixedTable          fixedTable;         // BUG-42639 Monitoring Query
    smiFixedTableProperties     fixedTableProperty; // BUG-42639 Mornitoring Query

    //---------------------------------
    // Update�� ���� Column List
    //---------------------------------

    smiColumnList             * updateColumnList;   // Update�� �ƴ� ���, NULL�� ����
    idBool                      inplaceUpdate;      // inplace update

    //---------------------------------
    // Predicate ����
    //---------------------------------

    smiRange                  * fixKeyRangeArea; // Fixed Key Range ����
    smiRange                  * fixKeyRange;     // Fixed Key Range
    UInt                        fixKeyRangeSize; // Fixed Key Range ũ��

    smiRange                  * fixKeyFilterArea; //Fixed Key Filter ����
    smiRange                  * fixKeyFilter;    // Fixed Key Filter
    UInt                        fixKeyFilterSize;// Fixed Key Filter ũ��

    smiRange                  * varKeyRangeArea; // Variable Key Range ����
    smiRange                  * varKeyRange;     // Variable Key Range
    UInt                        varKeyRangeSize; // Variable Key Range ũ��

    smiRange                  * varKeyFilterArea; //Variable Key Filter ����
    smiRange                  * varKeyFilter;    // Variable Key Filter
    UInt                        varKeyFilterSize;// Variable Key Filter ũ��

    smiRange                  * notNullKeyRange; // Not Null Key Range
    
    smiRange                  * keyRange;        // ���� Key Range
    smiRange                  * keyFilter;       // ���� Key Filter
    smiRange                  * ridRange;

    // Filter ���� CallBack ����
    smiCallBack                 callBack;        // Filter CallBack
    qtcSmiCallBackDataAnd       callBackDataAnd; //
    qtcSmiCallBackData          callBackData[3]; // �� ������ Filter�� ������.

    //---------------------------------
    // Merge Join ���� ����
    //---------------------------------

    smiCursorPosInfo            cursorInfo;

    //---------------------------------
    // Disk Table ���� ����
    //---------------------------------

    void                      * nullRow;  // Disk Table�� ���� null row
    scGRID                      nullRID;

    //---------------------------------
    // Trigger ó���� ���� ����
    //---------------------------------

    // PROJ-1705
    // 1. update, delete�� ��� trigger row�� �ʿ俩�� ����
    //   �� ������ cusor open�� fetch column���������� ���̸�,
    //   trigger row�� �ʿ��� ��� ( ���̺� ��ü �÷� ��ġ )
    //   trigger row�� �ʿ����� ���� ��� ( validation�� ������ �÷��� ���ؼ� ��ġ )
    // 2. partitioned table�� update��
    //    rowMovement�� ��� ���̺� ��ü �÷� ��ġ
    idBool                      isNeedAllFetchColumn;    

    // PROJ-1071 Parallel query
    UInt                        mOrgModifyCnt;
    UInt                        mOrgSubQFilterDepCnt;

    /* PROJ-2402 Parallel Table Scan */
    UInt                        mAccessCnt4Parallel;

    /* PROJ-2632 */
    mtxSerialExecuteData      * mSerialExecuteData;
} qmndSCAN;

class qmnSCAN
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

    /* PROJ-2402 */
    static IDE_RC readyIt( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           UInt         aTID );

    /* PROJ-1832 New database link */
    static IDE_RC printRemotePlan( qmnPlan      * aPlan,
                                   ULong          aDepth,
                                   iduVarString * aString );

    static IDE_RC printLocalPlan( qcTemplate   * aTemplate,
                                  qmnPlan      * aPlan,
                                  ULong          aDepth,
                                  iduVarString * aString,
                                  qmnDisplay     aMode );

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // �׻� ������ ������ �� ���� ���
    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    // ���� SCAN�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� SCAN�� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // ���� Fixed Table SCAN�� ����
    static IDE_RC doItFirstFixedTable( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    // ���� Fixed Table SCAN�� ����
    static IDE_RC doItNextFixedTable( qcTemplate * aTemplate,
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
                                         qmncSCAN   * aCodePlan,
                                         qmndSCAN   * aDataPlan );

    static IDE_RC makeRidRange(qcTemplate* aTemplate,
                               qmncSCAN  * aCodePlan,
                               qmndSCAN  * aDataPlan);

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // plan���� data ������ selected method�� ���õǾ����� �˷��ش�.
    static IDE_RC notifyOfSelectedMethodSet( qcTemplate * aTemplate,
                                             qmncSCAN   * aCodePlan );

    // PCRD���� ���� cursor�� ���� �ֵ��� ������
    static IDE_RC openCursorForPartition( qcTemplate * aTemplate,
                                          qmncSCAN   * aCodePlan,
                                          qmndSCAN   * aDataPlan );

    // PROJ-1624 global non-partitioned index
    // PCRD���� ���� readRow�� �� �� �ֵ��� ������
    static IDE_RC readRowFromGRID( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   scGRID       aRowGRID,
                                   qmcRowFlag * aFlag );

    // code plan�� qmncScanMethod�� ����� ��,
    // sdf�� candidate�� ���� qmncScanMethod�� ����� �� �Ǵ��Ѵ�.
    static qmncScanMethod * getScanMethod( qcTemplate * aTemplate,
                                           qmncSCAN   * aCodePlan );

    static void resetExecInfo4Subquery(qcTemplate *aTemplate, qmnPlan *aPlan);

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncSCAN   * aCodePlan,
                             qmndSCAN   * aDataPlan );

    // Fixed Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyRange( qcTemplate * aTemplate,
                                    qmncSCAN   * aCodePlan,
                                    qmndSCAN   * aDataPlan );

    // Fixed Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocFixKeyFilter( qcTemplate * aTemplate,
                                     qmncSCAN   * aCodePlan,
                                     qmndSCAN   * aDataPlan );

    // Variable Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyRange( qcTemplate * aTemplate,
                                    qmncSCAN   * aCodePlan,
                                    qmndSCAN   * aDataPlan );

    // Variable Key Filter�� ���� Range ���� �Ҵ�
    static IDE_RC allocVarKeyFilter( qcTemplate * aTemplate,
                                     qmncSCAN   * aCodePlan,
                                     qmndSCAN   * aDataPlan );

    // Not Null Key Range�� ���� Range ���� �Ҵ�
    static IDE_RC allocNotNullKeyRange( qcTemplate * aTemplate,
                                        qmncSCAN   * aCodePlan,
                                        qmndSCAN   * aDataPlan );

    static IDE_RC allocRidRange(qcTemplate* aTemplate,
                                qmncSCAN  * aCodePlan,
                                qmndSCAN  * aDataPlan);

    //------------------------
    // Cursor ���� �Լ�
    //------------------------

    // Cursor�� Open
    static IDE_RC openCursor( qcTemplate * aTemplate,
                              qmncSCAN   * aCodePlan,
                              qmndSCAN   * aDataPlan );

    // Cursor�� Restart
    static IDE_RC restartCursor( qmncSCAN   * aCodePlan,
                                 qmndSCAN   * aDataPlan );

    // cursor close will be executed by aTemplate->cursorMgr
    //     after execution of a query
    // static IDE_RC closeCursor( qmncSCAN * aCodePlan,
    //                            qmndSCAN * aDataPlan );

    //------------------------
    // SCAN�� doIt() ���� �Լ�
    //------------------------

    // �ϳ��� record�� ����
    static IDE_RC readRow( qcTemplate * aTemplate,
                           qmncSCAN   * aCodePlan,
                           qmndSCAN   * aDataPlan,
                           qmcRowFlag * aFlag);

    // �ϳ��� FixedTable record�� ����
    static IDE_RC readRowFixedTable( qcTemplate * aTemplate,
                                     qmncSCAN   * aCodePlan,
                                     qmndSCAN   * aDataPlan,
                                     qmcRowFlag * aFlag );

    // IN Subquery KeyRange�� ���� ��õ�
    static IDE_RC reRead4InSubRange( qcTemplate * aTemplate,
                                     qmncSCAN   * aCodePlan,
                                     qmndSCAN   * aDataPlan,
                                     void      ** aRow );

    //------------------------
    // Plan Display ���� �Լ�
    //------------------------

    // Access������ ����Ѵ�.
    static IDE_RC printAccessInfo( qmncSCAN     * aCodePlan,
                                   qmndSCAN     * aDataPlan,
                                   iduVarString * aString,                                   
                                   qmnDisplay      aMode );

    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncSCAN     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString,
                                      qmnDisplay     aMode );

    // Key Range�� �� ������ ���
    static IDE_RC printKeyRangeInfo( qcTemplate   * aTemplate,
                                     qmncSCAN     * aCodePlan,
                                     ULong          aDepth,
                                     iduVarString * aString );

    // Key Filter�� �� ������ ���
    static IDE_RC printKeyFilterInfo( qcTemplate   * aTemplate,
                                      qmncSCAN     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

    // Filter�� �� ������ ���
    static IDE_RC printFilterInfo( qcTemplate   * aTemplate,
                                   qmncSCAN     * aCodePlan,
                                   ULong          aDepth,
                                   iduVarString * aString,
                                   qmnDisplay     aMode );

    //fix BUG-17553
    static void addAccessCount( qmncSCAN   * aPlan,
                                qcTemplate * aTemplate,
                                ULong        aCount );
};

#endif /* _O_QMN_SCAN_H_ */
