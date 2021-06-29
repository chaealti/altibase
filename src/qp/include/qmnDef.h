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
 * $Id: qmnDef.h 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Description :
 *     ��� Execution Plan Node �� ������ ���� ������ ������.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_DEF_H_
#define _O_QMN_DEF_H_ 1

#include <qc.h>
#include <qtc.h>
#include <qmc.h>
#include <qmoKeyRange.h>
#include <qmoPredicate.h>
#include <smiDef.h>

/***********************************************************************
 * [Data Plan�� �ʱ�ȭ�� ���� ����]
 *
 * Execution Node�� Data Plan�� ������
 * PREPARE�� �Ϸ�Ǿ�����, EXECUTION�� �Ϸ�Ǿ������� ǥ���ϱ� ����
 * mask ���鿡 ���� �����̴�.
 * �̿� ���� flag�� mask�� ����ϴ� ������ Data Plan�� ���� Memory ������
 * ��� �ʱ�ȭ�ϴ� ���ϸ� ���ְ�, �ʿ��� �ʱ�ȭ���� �����ϱ� �����̴�.
 * ����, ��� Data Plan�� flag�� ���� �κ��� ����Ǿ� �ִµ�,
 * �� �ǹ̴� ������ ����.
 * ������ Bit : [0001] - first init()�� ������ ���� ���� ����
 *                     - 0�� ��� �����ؾ� ��.
 **********************************************************************/

//------------------------------------------
// qcTemplate.planFlag
//------------------------------------------
// after PVO, set this flag
// ��� Plan Node�� firstInit()�� ������ �� �ֵ��� �Ѵ�.
#define QMN_PLAN_PREPARE_DONE_MASK           (0x00000000)

/***********************************************************************
 * [DML Cursor�� ���� ����]
 **********************************************************************/

// PROJ-2205 rownum in DML
// DML�� ���� Ŀ���� �߰�����
// UPDATE, DELETE, MOVE�� PCRD, SCAN ���̿� FILT���� �ٸ� ������
// ������ �� �����Ƿ� cursorInfo�� �̿��Ͽ� ������ �����Ѵ�.
typedef struct qmnCursorInfo
{
    //-----------------------------------------
    // PCRD, SCAN�� ������ UPDATE, DELETE, MOVE���� �����ϴ� ����
    //-----------------------------------------

    // SCAN�� ��� table cursor Ȥ�� partition cursor
    // PCRD�� ��� index table cursor
    smiTableCursor  * cursor;

    // SCAN Ȥ�� PCRD���� ���õ� index (null/local/global)
    qcmIndex        * selectedIndex;
    mtcTuple        * selectedIndexTuple;
    
    /* PROJ-2359 Table/Partition Access Option */
    qcmAccessOption   accessOption;

    //-----------------------------------------
    // UPDATE, DELETE, MOVE�� ������ PCRD, SCAN���� �����ϴ� ����
    //-----------------------------------------
    
    smiColumnList   * updateColumnList;     // update column list
    smiCursorType     cursorType;           // DML cursor type
    idBool            isRowMovementUpdate;  // row movement update�� ��� (insert-delete)
    idBool            inplaceUpdate;        // inplace update
    UInt              lockMode;             // Lock Mode

    /* PROJ-2464 hybrid partitioned table ���� */
    smiColumnList   * stmtRetryColLst;
    smiColumnList   * rowRetryColLst;
} qmnCursorInfo;

/***********************************************************************
 * [Plan Display�� ���� ����]
 **********************************************************************/

// Plan Tree�� Display�ϱ� ���� String�� �Ѱ谪�� ������ ���
// �Ʒ��� ���� �ִ� ���̸� 32K�� �����Ѵ�.
#define QMN_MAX_PLAN_LENGTH                   (32 * 1024)
#define QMN_PLAN_PRINT_LENGTH                       (256)

#define QMN_PLAN_PRINT_NON_MTR_ID                    (-1)

// Plan Display Option�� ���� ���Ƿ�
// ALTER SESSION SET EXPLAIN PLAN = ON ��
// ALTER SESSION SET EXPLAIN PLAN = ONLY�� �����ϱ� ���� ����̴�.
typedef enum
{
    QMN_DISPLAY_ALL = 0, // ���� ����� ��� Display ��
    QMN_DISPLAY_CODE     // Code ������ �������� Display ��
} qmnDisplay;

// PROJ-2551 simple query ����ȭ
typedef enum
{
    QMN_VALUE_TYPE_INIT = 0,
    QMN_VALUE_TYPE_CONST_VALUE,
    QMN_VALUE_TYPE_HOST_VALUE,
    QMN_VALUE_TYPE_COLUMN,
    QMN_VALUE_TYPE_PROWID,
    QMN_VALUE_TYPE_SYSDATE,
    QMN_VALUE_TYPE_TO_CHAR
} qmnValueType;

typedef enum
{
    QMN_VALUE_OP_INIT = 0,
    QMN_VALUE_OP_ASSIGN,
    QMN_VALUE_OP_ADD,
    QMN_VALUE_OP_SUB,
    QMN_VALUE_OP_EQUAL,
    QMN_VALUE_OP_LT,
    QMN_VALUE_OP_LE,
    QMN_VALUE_OP_GT,
    QMN_VALUE_OP_GE
} qmnValueOp;

typedef struct qmnValueColumnInfo
{
    UShort      table;
    mtcColumn   column;
    void      * info;
} qmnValueColumnInfo;
    
typedef struct qmnValueInfo
{
    mtcColumn      column;
    qmnValueType   type;
    qmnValueOp     op;
    idBool         isQueue; // BUG-45715 support ENQUEUE

    union
    {
        qmnValueColumnInfo    columnVal;
        void                * constVal;
        UShort                id;  // bind param id
    } value;
    
} qmnValueInfo;

#define QMN_INIT_VALUE_INFO( val )      \
{                                       \
    (val)->type = QMN_VALUE_TYPE_INIT;  \
    (val)->op = QMN_VALUE_OP_INIT;      \
    (val)->isQueue = ID_FALSE;          \
    (val)->value.constVal = NULL;       \
}

/***********************************************************************
 * [Code Plan�� ���� ����]
 *
 **********************************************************************/

// Execution Plan �� ����
typedef enum
{
    QMN_NONE = 0,
    QMN_SCAN,      // SCAN plan
    QMN_VIEW,      // VIEW plan
    QMN_PROJ,      // PROJect plan
    QMN_FILT,      // FILTer plan
    QMN_SORT,      // SORT plan
    QMN_LMST,      // LiMit SorT plan  (A4���� ���� �߰�)
    QMN_HASH,      // HASH plan
    QMN_HSDS,      // HaSh DiStinct plan
    QMN_GRAG,      // GRoup AGgregation plan
    QMN_AGGR,      // AGGRegation plan
    QMN_GRBY,      // GRoup BY plan
    QMN_CUNT,      // CoUNT(*) plan    (A4���� ���� �߰�)
    QMN_CONC,      // CONCatenation plan
    QMN_JOIN,      // JOIN plan
    QMN_MGJN,      // MerGe JoiN plan
    QMN_BUNI,      // Bag UNIon plan
    QMN_SITS,      // Set InTerSect plan
    QMN_SDIF,      // Set DIFference plan
    QMN_LOJN,      // Left Outer JoiN plan
    QMN_FOJN,      // Full Outer JoiN plan
    QMN_AOJN,      // Anti left Outer JoiN plan
    QMN_VMTR,      // View MaTeRialize plan
    QMN_VSCN,      // View SCaN plan
    QMN_GRST,      // GRouping SeTs plan
    QMN_MULTI_BUNI,// Multiple Bag UNIon plan
    QMN_PCRD,      // PROJ-1502 Partition CoRDinator plan
    QMN_CNTR,      // PROJ-1405 CouNTeR plan
    QMN_WNST,      // PROJ-1762 Analytic Function
    QMN_CNBY,      // PROJ-1715
    QMN_CMTR,      // PROJ-1715
    QMN_INST,      // PROJ-2205
    QMN_MTIT,      // BUG-36596 multi-table insert
    QMN_UPTE,      // PROJ-2205
    QMN_DETE,      // PROJ-2205
    QMN_MOVE,      // PROJ-2205
    QMN_MRGE,      // PROJ-2205
    QMN_ROLL,      // PROJ-1353
    QMN_CUBE,      // PROJ-1353
    QMN_PRLQ,      // PROJ-1071 Parallel Query
    QMN_PPCRD,     // PROJ-1071 Parallel Query
    QMN_PSCRD,     /* PROJ-2402 Parallel Table Scan */
    QMN_SREC,      // PROJ-2582 recursive with
    QMN_DLAY,      // BUG-43493 delayed execution
    QMN_SDSE,      // PROJ-2638
    QMN_SDEX,      // PROJ-2638
    QMN_SDIN,      // PROJ-2638
    QMN_MAX_MAX
} qmnType;

//------------------------------------------------
// Code Plan�� Flag ����
//------------------------------------------------

// qmnPlan.flag
#define QMN_PLAN_FLAG_CLEAR                  (0x00000000)

// qmnPlan.flag
// Plan Node �� ����ϴ� ���� ��ü�� ����
#define QMN_PLAN_STORAGE_MASK                (0x00000001)
#define QMN_PLAN_STORAGE_MEMORY              (0x00000000)
#define QMN_PLAN_STORAGE_DISK                (0x00000001)

// qmnPlan.flag
// Plan Node �� outer column reference�� ���� ���� ����
#define QMN_PLAN_OUTER_REF_MASK              (0x00000002)
#define QMN_PLAN_OUTER_REF_FALSE             (0x00000000)
#define QMN_PLAN_OUTER_REF_TRUE              (0x00000002)

// qmnPlan.flag
// Plan Node �� temp table�� ������ fixed rid�� ������ �ϴ��� ����
#define QMN_PLAN_TEMP_FIXED_RID_MASK         (0x00000004)
#define QMN_PLAN_TEMP_FIXED_RID_FALSE        (0x00000000)
#define QMN_PLAN_TEMP_FIXED_RID_TRUE         (0x00000004)

// qmnPlan.flag
// PROJ-2242
#define QMN_PLAN_JOIN_METHOD_TYPE_MASK       (0x000000F0)
#define QMN_PLAN_JOIN_METHOD_FULL_NL         (0x00000010)
#define QMN_PLAN_JOIN_METHOD_INDEX           (0x00000020)
#define QMN_PLAN_JOIN_METHOD_FULL_STORE_NL   (0x00000030)
#define QMN_PLAN_JOIN_METHOD_ANTI            (0x00000040)
#define QMN_PLAN_JOIN_METHOD_INVERSE_INDEX   (0x00000050)
#define QMN_PLAN_JOIN_METHOD_ONE_PASS_HASH   (0x00000060)
#define QMN_PLAN_JOIN_METHOD_TWO_PASS_HASH   (0x00000070)
#define QMN_PLAN_JOIN_METHOD_INVERSE_HASH    (0x00000080)
#define QMN_PLAN_JOIN_METHOD_ONE_PASS_SORT   (0x00000090)
#define QMN_PLAN_JOIN_METHOD_TWO_PASS_SORT   (0x000000A0)
#define QMN_PLAN_JOIN_METHOD_INVERSE_SORT    (0x000000B0)
#define QMN_PLAN_JOIN_METHOD_MERGE           (0x000000C0)

/* qmnPlan.flag
 * PROJ-1071 Parallel Query
 *
 * parallel execute ���� ������
 * ������ PRLQ �� �ִ��� �������� ���� ������
 */

#define QMN_PLAN_NODE_EXIST_MASK         (0x00003000)

#define QMN_PLAN_PRLQ_EXIST_MASK         (0x00001000)
#define QMN_PLAN_PRLQ_EXIST_TRUE         (0x00001000)
#define QMN_PLAN_PRLQ_EXIST_FALSE        (0x00000000)

#define QMN_PLAN_MTR_EXIST_MASK          (0x00002000)
#define QMN_PLAN_MTR_EXIST_TRUE          (0x00002000)
#define QMN_PLAN_MTR_EXIST_FALSE         (0x00000000)

#define QMN_PLAN_INIT_THREAD_ID          (0)

#define QMN_PLAN_RESULT_CACHE_EXIST_MASK  (0x00004000)
#define QMN_PLAN_RESULT_CACHE_EXIST_TRUE  (0x00004000)
#define QMN_PLAN_RESULT_CACHE_EXIST_FALSE (0x00000000)

#define QMN_PLAN_GARG_PARALLEL_MASK       (0x00008000)
#define QMN_PLAN_GARG_PARALLEL_TRUE       (0x00008000)
#define QMN_PLAN_GARG_PARALLEL_FALSE      (0x00000000)

#define QMN_PLAN_DEFAULT_DEPENDENCY_VALUE (0xFFFFFFFF)

//------------------------------------------------
// Code Plan Node�� ���������� ������ �Լ� ������
//------------------------------------------------

// �� Plan Node�� �ʱ�ȭ�� �����ϴ� �Լ�
typedef IDE_RC (* initFunc ) ( qcTemplate * aTemplate,
                               qmnPlan    * aNode );

// �� Plan Node�� ������ ����� �����ϴ� �Լ�
typedef IDE_RC (* doItFunc ) ( qcTemplate * aTemplate,
                               qmnPlan    * aNode,
                               qmcRowFlag * aFlag );

// PROJ-2444
// parallel plan �� �����ϱ� ���� �ݵ�� �ؾ��ϴ� ������ �����ϴ� �Լ�
typedef IDE_RC (* readyItFunc ) ( qcTemplate * aTemplate,
                                  qmnPlan    * aNode,
                                  UInt         aTID );

// �� Plan Node�� Null Padding�� �����ϴ� �Լ�
typedef IDE_RC (* padNullFunc) ( qcTemplate * aTemplate,
                                 qmnPlan    * aNode );

// �� Plan Node�� Plan ������ Display�ϴ� �Լ�
typedef IDE_RC (* printPlanFunc) ( qcTemplate   * aTemplate,
                                   qmnPlan      * aNode,
                                   ULong          aDepth,
                                   iduVarString * aString,
                                   qmnDisplay     aMode );

/***********************************************************************
 *  [ Plan �� ���� ]
 *
 *  ---------------------------------------------
 *    Plan      |  left   |  right  | children
 *  ---------------------------------------------
 *  unary plan  |    O    |  null   |   null
 *  ---------------------------------------------
 *  binary plan |    O    |    O    |   null
 *  ---------------------------------------------
 *  multi  plan |   null  |  null   |    O
 *  ----------------------------------------------
 *
 **********************************************************************/

/***********************************************************************
 * Multiple Children�� �����ϱ� ���� �ڷ� ����
 *      ( PROJ-1486 Multiple Bag Union )
 **********************************************************************/

typedef struct qmnChildren
{
    struct qmnPlan     * childPlan;     // Child Plan
    struct qmnChildren * next;
} qmnChildren;

typedef struct qmnChildrenIndex
{
    smOID                childOID;      // child partition oid
    struct qmnPlan     * childPlan;     // Child Plan
} qmnChildrenIndex;

// PROJ-2209 prepare phase partition reference sort
typedef struct qmnRangeSortedChildren
{
    struct qmnChildren * children;       // sorted children
    qcmColumn          * partKeyColumns; // partition key column
    qmsPartitionRef    * partitionRef;   // sorted partition reference
}qmnRangeSortedChildren;

//----------------------------------------------
// ��� Code Node���� �������� ������ Plan ����
//----------------------------------------------

typedef struct qmnPlan
{
    qmnType       type;            // plan type
    UInt          flag;            // ��Ÿ Mask ����
    UInt          offset;          // Data Node offset in templete

    SDouble       qmgOuput;        // PROJ-2242
    SDouble       qmgAllCost;      // PROJ-2242

    initFunc      init;            // init function
    readyItFunc   readyIt;         // readyIt function
    doItFunc      doIt;            // doIt function
    padNullFunc   padNull;         // null padding function
    printPlanFunc printPlan;       // print plan function

    // Invariant ���� ó���� ���� dependency
    // Plan Node �� ������ dependencies �� ��ǥ dependency
    qcDepInfo     depInfo;
    ULong         dependency;
    ULong         outerDependency; // ��ǥ outer dependency

    qmnPlan     * left;            // left child node
    qmnPlan     * right;           // right child node
    qmnChildren * children;        // multi children node
    qmnChildren * childrenPRLQ;    // multi children node for parallel queue

    qmcAttrDesc * resultDesc;

    UInt          mParallelDegree;
} qmnPlan;

//----------------------------------------------
// ��� Data Plan ���� �������� ������ Plan ����
//----------------------------------------------
typedef struct qmndPlan
{
    mtcTuple      * myTuple;        // Tuple ��ġ
    UInt            mTID;           // Node �� ����� thread id
} qmndPlan;

//----------------------------------------------
// Key Range, Key Filter, Filter������ ���� �ڷ� ����
//----------------------------------------------

typedef struct qmnCursorPredicate
{
    // Input Information
    qcmIndex              * index;               // Input: Index ����
    UShort                  tupleRowID;          // Input: Tuple ����

    smiRange              * fixKeyRangeArea;     // Input: Fixed Key Range Area
    smiRange              * fixKeyRange;         // Input: Fixed Key Range
    qtcNode               * fixKeyRangeOrg;      // Input: Fixed Key Range Node
    UInt                    fixKeyRangeSize;

    smiRange              * varKeyRangeArea;     // Input: Variable KeyRange Area
    smiRange              * varKeyRange;         // Input: Variable Key Range
    qtcNode               * varKeyRangeOrg;      // Input: Variable Key Range Node
    qtcNode               * varKeyRange4FilterOrg;   // Input: Variable Fixed Key Range Node
    UInt                    varKeyRangeSize;

    smiRange              * fixKeyFilterArea;    // Input: Fixed Key Filter Area
    smiRange              * fixKeyFilter;        // Input: Fixed Key Filter
    qtcNode               * fixKeyFilterOrg;     // Input: Fixed Key Filter Node
    UInt                    fixKeyFilterSize;

    smiRange              * varKeyFilterArea;    // Input: Var Key Filter Area
    smiRange              * varKeyFilter;        // Input: variable Key Filter
    qtcNode               * varKeyFilterOrg;     // Input: Variable Key Filter Node
    qtcNode               * varKeyFilter4FilterOrg;  // Input: Variable Fixed Key Filter Node
    UInt                    varKeyFilterSize;

    smiRange              * notNullKeyRange;     // Input: Not Null Key Range

    qtcNode               * filter;              // Input: Filter

    // Output Information
    smiCallBack           * filterCallBack;      // Output: Filter CallBack
    qtcSmiCallBackDataAnd * callBackDataAnd;     // Output: Filter And
    qtcSmiCallBackData    * callBackData;        // Output: Filter Data

    /* PROJ-2632 */
    mtxSerialFilterInfo   * mSerialFilterInfo;
    mtxSerialExecuteData  * mSerialExecuteData;

    smiRange              * keyRange;            // Output: Key Range
    smiRange              * keyFilter;           // Output: Key Filter
} qmnCursorPredicate;

/* PROJ-1353 GROUPING, GROUPING_ID Funciton�� ������ ����ü */
typedef struct qmnGrouping
{
    SInt   type;
    SInt * index;
} qmnGrouping;

/* PROJ-2339 Inverse Hash Join, Full Ouhter Join���� ������ */
typedef IDE_RC (* setHitFlagFunc) ( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan );

/* PROJ-2339 Inverse Hash Join ���� ��� */
typedef idBool (* isHitFlaggedFunc) ( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan );

/* BUG-42467 */
typedef enum qmndResultCacheStatus
{
    QMND_RESULT_CACHE_INIT = 0,
    QMND_RESULT_CACHE_HIT,
    QMND_RESULT_CACHE_MISS
} qmndResultCacheStatus;

/* PROJ-2462 Result Cache */
typedef struct qmndResultCache
{
    UInt                 * flag;
    UInt                   missCount;
    UInt                   hitCount;
    iduMemory            * memory;
    UInt                   memoryIdx;
    SLong                  tablesModify;
    qmndResultCacheStatus  status;
} qmndResultCache;

//----------------------------------------------
// ��� Plan�� ���������� ����ϴ� ���
//----------------------------------------------

class qmn
{
public:
    // Subquery�� Plan�� Display��
    static IDE_RC printSubqueryPlan( qcTemplate   * aTemplate,
                                     qtcNode      * aSubQuery,
                                     ULong          aDepth,
                                     iduVarString * aString,
                                     qmnDisplay     aMode );

    // PROJ-2242
    static void printMTRinfo( iduVarString * aString,
                              ULong          aDepth,
                              qmcMtrNode   * aMtrNode,
                              const SChar  * aMtrName,
                              UShort         aSelfID,
                              UShort         aRefID1,
                              UShort         aRefID2 );

    // PROJ-2242
    static void printJoinMethod( iduVarString * aString,
                                 UInt           aqmnflag );

    // PROJ-2242
    static void printCost( iduVarString * aString,
                           SDouble        aCost );

    // SCAN, HIER, CUNT �� Cursor�� ����ϴ� ��忡�� ���
    // Key Range, Key Filter, Filter�� ������.
    static IDE_RC makeKeyRangeAndFilter( qcTemplate         * aTemplate,
                                         qmnCursorPredicate * aPredicate );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // scan �Ǵ� cunt plan�� ���� table handle�� table scn�� ��´�.
    static IDE_RC getReferencedTableInfo( qmnPlan     * aPlan,
                                          const void ** aTableHandle,
                                          smSCN       * aTableSCN,
                                          idBool      * aIsFixedTable );

    // qmo::optimizeForHost���� selected method�� ������ ��
    // plan���� �˷��ش�.
    static IDE_RC notifyOfSelectedMethodSet( qcTemplate * aTemplate,
                                             qmnPlan    * aPlan );

    // PROJ-1705
    // ��ũ���̺� ���� null row ����
    static IDE_RC makeNullRow( mtcTuple   * aTuple,
                               void       * aNullRow );
    
    static IDE_RC printResult( qcTemplate        * aTemplate,
                               ULong               aDepth,
                               iduVarString      * aString,
                               const qmcAttrDesc * aResultDesc );

    static void printSpaceDepth(iduVarString* aString, ULong aDepth);

    // PROJ-2462 Result Cache
    static void printResultCacheInfo( iduVarString    * aString,
                                      ULong             aDepth,
                                      qmnDisplay        aMode,
                                      idBool            aIsInit,
                                      qmndResultCache * aResultData );

    // PROJ-2462 Result Cache
    static void printResultCacheRef( iduVarString    * aString,
                                     ULong             aDepth,
                                     qcComponentInfo * aComInfo );

    static void setDisplayInfo( qmsTableRef      * aTableRef,
                                qmsNamePosition  * aTableOwnerName,
                                qmsNamePosition  * aTableName,
                                qmsNamePosition  * aAliasName );

private:

    // Key Range�� ����
    static IDE_RC makeKeyRange ( qcTemplate         * aTemplate,
                                 qmnCursorPredicate * aPredicate,
                                 qtcNode           ** aFilter );

    // Key Filter�� ����
    static IDE_RC makeKeyFilter( qcTemplate         * aTemplate,
                                 qmnCursorPredicate * aPredicate,
                                 qtcNode           ** aFilter );

    // Filter CallBack ����
    static IDE_RC makeFilter( qcTemplate         * aTemplate,
                              qmnCursorPredicate * aPredicate,
                              qtcNode            * aFirstFilter,
                              qtcNode            * aSecondFilter,
                              qtcNode            * aThirdFilter );
};

#endif /* _O_QMN_DEF_H_ */
