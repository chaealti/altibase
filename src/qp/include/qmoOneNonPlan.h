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
 * $Id: qmoOneNonPlan.h 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Plan Generator
 *
 *     One-child Non-materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - SCAN ���
 *         - FILT ���
 *         - PROJ ���
 *         - HIER ���
 *         - GRBY ���
 *         - AGGR ���
 *         - CUNT ���
 *         - VIEW ���
 *         - VSCN ���
 *         - INST ���
 *         - UPTE ���
 *         - DETE ���
 *         - MOVE ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_ONE_CHILD_NON_MATER_H_
#define _O_QMO_ONE_CHILD_NON_MATER_H_ 1

#include <qc.h>
#include <qmoDef.h>
#include <qmn.h>
#include <qmgDef.h>
#include <qmoDependency.h>
#include <qmoPredicate.h>
#include <qmoNormalForm.h>
#include <qmoSubquery.h>

//------------------------------
//qmoSCANInfo.flag
//------------------------------
#define QMO_SCAN_INFO_INITIALIZE               (0x00000000)

#define QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK    (0x00000010)
#define QMO_SCAN_INFO_NOTNULL_KEYRANGE_FALSE   (0x00000000)
#define QMO_SCAN_INFO_NOTNULL_KEYRANGE_TRUE    (0x00000010)

#define QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK    (0x00000020)
#define QMO_SCAN_INFO_FORCE_INDEX_SCAN_TRUE    (0x00000020)
#define QMO_SCAN_INFO_FORCE_INDEX_SCAN_FALSE   (0x00000000)

#define QMO_SCAN_INFO_FORCE_RID_SCAN_MASK      (0x00000040)
#define QMO_SCAN_INFO_FORCE_RID_SCAN_TRUE      (0x00000040)
#define QMO_SCAN_INFO_FORCE_RID_SCAN_FALSE     (0x00000000)

//------------------------------
//SCAN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_SCAN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE        |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
// PROJ-2638 SDSE����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_SDSE_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE        |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//FILT����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_FILT_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//FILT����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_PROJ_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//GRBY����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_GRBY_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE      |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_MATERIALIZATION|    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE  )

//------------------------------
//AGGR����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_AGGR_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE      |    \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE        |    \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_MATERIALIZATION|    \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE           |    \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE   |    \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE  )

//------------------------------
//CUNT����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_CUNT_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE        |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//VSCN����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_VSCN_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_TRUE           |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//VIEW����� dependency�� ȣ���� ���� flag
//------------------------------
// Created View, Inline View���� ����� VIEW
#define QMO_VIEW_EXPLICIT_DEPENDENCY                    \
    ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
      QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE        |     \
      QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
      QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
      QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
      QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

// SET�Ǵ� Store and Search���� ���� �����Ǵ� VIEW
#define QMO_VIEW_IMPLICIT_DEPENDENCY                    \
    ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE     |     \
      QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
      QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
      QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
      QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
      QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//CNTR����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_CNTR_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//INST����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_INST_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//UPTE����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_UPTE_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//DETE����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_DETE_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//MOVE����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_MOVE_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//DLAY����� dependency�� ȣ���� ���� flag
//------------------------------
#define QMO_DLAY_DEPENDENCY ( QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_FALSE    |     \
                              QMO_DEPENDENCY_STEP2_BASE_TABLE_FALSE       |     \
                              QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE     |     \
                              QMO_DEPENDENCY_STEP2_SETNODE_FALSE          |     \
                              QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_FALSE  |     \
                              QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_FALSE )

//------------------------------
//makePROJ()�Լ��� �ʿ��� flag
//------------------------------

//TOP , non-TOP�� ����
#define QMO_MAKEPROJ_TOP_MASK                  (0x00000001)
#define QMO_MAKEPROJ_TOP_FALSE                 (0x00000000)
#define QMO_MAKEPROJ_TOP_TRUE                  (0x00000001)

//indexable min-max�� ��� ����
#define QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK     (0x00000010)
#define QMO_MAKEPROJ_INDEXABLE_MINMAX_FALSE    (0x00000000)
#define QMO_MAKEPROJ_INDEXABLE_MINMAX_TRUE     (0x00000010)

/* PROJ-1071 Parallel Query */
#define QMO_MAKEPROJ_QUERYSET_TOP_MASK         (0x00000020)
#define QMO_MAKEPROJ_QUERYSET_TOP_FALSE        (0x00000000)
#define QMO_MAKEPROJ_QUERYSET_TOP_TRUE         (0x00000020)

//------------------------------
// makeGRBY()�Լ��� �ʿ��� flag
// ������ �뵵�� ����
// - DISTINCTION : SELECT Target�� ���� Distinction
// - GROUPING    : GROUP BY�� ���� Grouping
// - DISTAGGR    : Distinct Aggregation�� ���� Distinction
//------------------------------

#define QMO_MAKEGRBY_SORT_BASED_METHOD_MASK    (0x00000003)
#define QMO_MAKEGRBY_SORT_BASED_DISTINCTION    (0x00000000)
#define QMO_MAKEGRBY_SORT_BASED_GROUPING       (0x00000001)
#define QMO_MAKEGRBY_SORT_BASED_DISTAGGR       (0x00000002)

//------------------------------
//makeAGGR()�Լ��� �ʿ��� flag
//------------------------------

#define QMO_MAKEAGGR_TEMP_TABLE_MASK           (0x00000001)
#define QMO_MAKEAGGR_MEMORY_TEMP_TABLE         (0x00000000)
#define QMO_MAKEAGGR_DISK_TEMP_TABLE           (0x00000001)

//------------------------------
//makeVIEW()�Լ��� �ʿ��� flag
//------------------------------

#define QMO_MAKEVIEW_FROM_MASK                 (0x0000000F)
#define QMO_MAKEVIEW_FROM_PROJECTION           (0x00000000)
#define QMO_MAKEVIEW_FROM_SELECTION            (0x00000001)
#define QMO_MAKEVIEW_FROM_SET                  (0x00000002)

// PROJ-2462 Result Cache
#define QMO_MAKEPROJ_TOP_RESULT_CACHE_MASK     (0x00000040)
#define QMO_MAKEPROJ_TOP_RESULT_CACHE_FALSE    (0x00000000)
#define QMO_MAKEPROJ_TOP_RESULT_CACHE_TRUE     (0x00000040)

//---------------------------------------------------
// One-Child Non-Meterialized Plan�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

/*
 * PROJ-2402 Parallel Table Scan
 */
typedef struct qmoSCANParallelInfo
{
    UInt mDegree;
    UInt mSeqNo;
} qmoSCANParallelInfo;

typedef struct qmoSCANInfo
{
    UInt                   flag;               // (1)  Indexable Min , Max�� ����,
                                               //      Not Null KeyRange �ʿ� ����
    qmoPredicate         * predicate;          // (2)  (keyRange , keyFilter ,
                                               //      Filter)�� ��� ������
                                               //      Predicate ����
    qmoPredicate         * constantPredicate;  // (4)  constant
    qmoPredicate         * ridPredicate;       // (11) rid predicate
    qcmIndex             * index;              // (5)  index ����
    qmsLimit             * limit;              // (6)  limit���� ����
    qmgPreservedOrder    * preservedOrder;     // (7)  preserved Order�� ���� ����
    qmoScanDecisionFactor* sdf;                // (8)  makeSCAN���� �ĺ� ������ ����
    qtcNode              * nnfFilter;          // (9)  NNF Filter
    qmoSCANParallelInfo    mParallelInfo;
} qmoSCANInfo;

// Leaf ���(CNBY , CUNT) ��带 ���� �Է� ����
typedef struct qmoLeafInfo
{
    qmoPredicate      * predicate;          // (2)  (keyRange , keyFilter ,
                                            //      Filter)�� ��� ������
                                            //      Predicate ����
    qmoPredicate      * levelPredicate;     // (3)  level�� ���Ե� Predicate����
    qmoPredicate      * constantPredicate;  // (4)  constant
    qmoPredicate      * ridPredicate;
    qmoPredicate      * connectByRownumPred;// (5)  Connect by��rownum�� ���Ե�Predicate����
    qcmIndex          * index;              // (6)  index ����
    qmgPreservedOrder * preservedOrder;     // (7)  preserved Order�� ���� ����
    qmoScanDecisionFactor * sdf;            // (8)  makeSCAN���� �ĺ� ������ ����
    qtcNode           * nnfFilter;          // (9)  NNF Filter
    idBool              forceIndexScan;     // (10)
    //  CNBY������ (2),(3),(4),(5),(6),(7),(9)
    //  CUNT������ (2),(4),(6),(8),(10)
    //  �� ���δ�.
} qmoLeafInfo;

// PROJ-2205 rownum in DML
// insert operator ��带 ���� �Է� ����
typedef struct qmoINSTInfo
{
    qmsTableRef       * tableRef;
    
    qcmColumn         * columns;
    qcmColumn         * columnsForValues;
    
    qmmMultiRows      * rows;
    UInt                valueIdx;
    UInt                canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                compressedTuple;

    void              * queueMsgIDSeq;

    qmsHints          * hints;

    qcParseSeqCaches  * nextValSeqs;
    
    idBool              multiInsertSelect;
    idBool              insteadOfTrigger;
    
    qcmParentInfo     * parentConstraints;
    qdConstraintSpec  * checkConstrList;
    
    qmmReturnInto     * returnInto;

    /* PROJ-1090 Function-based Index */
    qmsTableRef       * defaultExprTableRef;
    qcmColumn         * defaultExprColumns;

    // BUG-43063 insert nowait
    ULong               lockWaitMicroSec;
    
} qmoINSTInfo;

// PROJ-2205 rownum in DML
// update operator ��带 ���� �Է� ����
typedef struct qmoUPTEInfo
{
    /* PROJ-2204 JOIN UPDATE, DELETE */
    qmsTableRef       * updateTableRef;
    
    qcmColumn         * columns;
    smiColumnList     * updateColumnList;
    UInt                updateColumnCount;
    
    qmmValueNode      * values;
    qmmSubqueries     * subqueries;
    UInt                valueIdx;
    UInt                canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                compressedTuple;
    mtdIsNullFunc     * isNull;
    
    qcParseSeqCaches  * nextValSeqs;
    
    idBool              insteadOfTrigger;
    
    qmoUpdateType       updateType;
    
    smiCursorType       cursorType;
    idBool              inplaceUpdate;
    qmsTableRef       * insertTableRef;
    idBool              isRowMovementUpdate;
    
    qmsLimit          * limit;
    
    qcmParentInfo     * parentConstraints;
    qcmRefChildInfo   * childConstraints;
    qdConstraintSpec  * checkConstrList;
    
    qmmReturnInto     * returnInto;

    /* PROJ-1090 Function-based Index */
    qmsTableRef       * defaultExprTableRef;
    qcmColumn         * defaultExprColumns;
    qcmColumn         * defaultExprBaseColumns;

    /* PROJ-2714 Multiple Update Delete support */
    qmmMultiTables     * mTableList;
} qmoUPTEInfo;

// PROJ-2205 rownum in DML
// delete operator ��带 ���� �Է� ����
typedef struct qmoDETEInfo
{
    qmsTableRef       * deleteTableRef;
    idBool              insteadOfTrigger;

    qmsLimit          * limit;

    qcmRefChildInfo   * childConstraints;

    qmmReturnInto     * returnInto;

    /* PROJ-2714 Multiple Update Delete support */
    qmmDelMultiTables * mTableList;
} qmoDETE;

// PROJ-2205 rownum in DML
// move operator ��带 ���� �Է� ����
typedef struct qmoMOVEInfo
{
    qmsTableRef       * targetTableRef;

    qcmColumn         * columns;

    qmmValueNode      * values;
    UInt                valueIdx;
    UInt                canonizedTuple;
    // PROJ-2264 Dictionary table
    UInt                compressedTuple;

    qcParseSeqCaches  * nextValSeqs;
    
    qmsLimit          * limit;

    qcmParentInfo     * parentConstraints;
    qcmRefChildInfo   * childConstraints;
    qdConstraintSpec  * checkConstrList;

    /* PROJ-1090 Function-based Index */
    qmsTableRef       * defaultExprTableRef;
    qcmColumn         * defaultExprColumns;
    
} qmoMOVE;

//---------------------------------------------------
// One-Child Non-Materialized Plan�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoOneNonPlan
{
public:

    // SCAN ����� ����
    static IDE_RC    makeSCAN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmsFrom      * aFrom ,
                               qmoSCANInfo  * aLeafInfo ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    // PROJ-1502 PARTITIONED DISK TABLE
    // SCAN(for PARTITION) ����� ����
    static IDE_RC    makeSCAN4Partition( qcStatement     * aStatement,
                                         qmsQuerySet     * aQuerySet,
                                         qmsFrom         * aFrom,
                                         qmoSCANInfo     * aLeafInfo,
                                         qmsPartitionRef * aPartitionRef,
                                         qmnPlan        ** aPlan );

    static IDE_RC    initFILT( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qtcNode      * aPredicate ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeFILT( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qtcNode      * aPredicate ,
                               qmoPredicate * aConstantPredicate ,
                               qmnPlan      * aChildPlan ,
                               qmnPlan      * aPlan );

    // PROJ ����� ����
    static IDE_RC    initPROJ( qcStatement  * aStatement,
                               qmsQuerySet  * aQuerySet,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makePROJ( qcStatement  * /*aStatement*/,
                               qmsQuerySet  * aQuerySet,
                               UInt           aFlag,
                               qmsLimit     * aLimit,
                               qtcNode      * aLoopNode,
                               qmnPlan      * aChildPlan,
                               qmnPlan      * aPlan );

    // GRBY ����� ����
    static IDE_RC    initGRBY( qcStatement       * aStatement ,
                               qmsQuerySet       * aQuerySet ,
                               qmsAggNode        * aAggrNode ,
                               qmsConcatElement  * aGroupColumn,
                               qmnPlan          ** aPlan );

    static IDE_RC    initGRBY( qcStatement       * aStatement ,
                               qmsQuerySet       * aQuerySet ,
                               qmnPlan           * aParent,
                               qmnPlan          ** aPlan );

    static IDE_RC    makeGRBY( qcStatement      * aStatement ,
                               qmsQuerySet      * aQuerySet ,
                               UInt               aFlag ,
                               qmnPlan          * aChildPlan ,
                               qmnPlan          * aPlan );

    // AGGR ����� ����
    static IDE_RC    initAGGR( qcStatement       * aStatement ,
                               qmsQuerySet       * aQuerySet ,
                               qmsAggNode        * aAggrNode ,
                               qmsConcatElement  * aGroupColumn,
                               qmnPlan           * aParent,
                               qmnPlan          ** aPlan );

    // AGGR ����� ����
    static IDE_RC    makeAGGR( qcStatement      * aStatement ,
                               qmsQuerySet      * aQuerySet ,
                               UInt               aFlag ,
                               qmoDistAggArg    * aDistAggArg,
                               qmnPlan          * aChildPlan ,
                               qmnPlan          * aPlan );

    // CUNT ����� ����
    static IDE_RC    initCUNT( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeCUNT( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmsFrom      * aFrom ,
                               qmsAggNode   * aCountNode,
                               qmoLeafInfo  * aLeafInfo ,
                               qmnPlan      * aPlan );

    // VIEW ����� ����
    static IDE_RC    initVIEW( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aParent ,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeVIEW( qcStatement  * aStatement ,
                                qmsQuerySet  * aQuerySet ,
                                qmsFrom      * aFrom ,
                                UInt           aFlag ,
                                qmnPlan      * aChildPlan,
                                qmnPlan      * aPlan );

    // VSCN ����� ����
    static IDE_RC    initVSCN( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmsFrom      * aFrom ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeVSCN( qcStatement  * aStatement ,
                                qmsQuerySet  * aQuerySet ,
                                qmsFrom      * aFrom ,
                                qmnPlan      * aChildPlan,
                                qmnPlan      * aPlan );

    // PROJ-1405
    // CNTR ����� ����
    static IDE_RC    initCNTR( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmnPlan       * aParent ,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeCNTR( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmoPredicate * aStopkeyPredicate ,
                               qmnPlan      * aChildPlan ,
                               qmnPlan      * aPlan );

    // PROJ-2205 rownum in DML
    // INST ����� ����
    static IDE_RC    initINST( qcStatement   * aStatement ,
                               qmnPlan      ** aPlan );
    
    static IDE_RC    makeINST( qcStatement   * aStatement ,
                               qmoINSTInfo   * aINSTInfo ,
                               qmnPlan       * aChildPlan ,
                               qmnPlan       * aPlan );
    
    // PROJ-2205 rownum in DML
    // UPTE ����� ����
    static IDE_RC    initUPTE( qcStatement   * aStatement ,
                               qmnPlan      ** aPlan );
    
    static IDE_RC    makeUPTE( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmoUPTEInfo   * aUPTEInfo ,
                               qmnPlan       * aChildPlan ,
                               qmnPlan       * aPlan );

    static IDE_RC    initMultiUPTE( qcStatement     * aStatement,
                                    qmsQuerySet     * aQuerySet,
                                    qmmMultiTables  * aTables,
                                    qmnPlan        ** aPlan );

    static IDE_RC    makeMultiUPTE( qcStatement   * aStatement,
                                    qmsQuerySet   * aQuerySet,
                                    qmoUPTEInfo   * aUPTEInfo,
                                    qmnPlan       * aChildPlan,
                                    qmnPlan       * aPlan );
    // PROJ-2205 rownum in DML
    // DETE ����� ����
    static IDE_RC    initDETE( qcStatement   * aStatement ,
                               qmnPlan      ** aPlan );
    
    static IDE_RC    makeDETE( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmoDETEInfo   * aDETEInfo ,
                               qmnPlan       * aChildPlan ,
                               qmnPlan       * aPlan );
    
    static IDE_RC    initMultiDETE( qcStatement        * aStatement,
                                    qmsQuerySet        * aQuerySet,
                                    qmmDelMultiTables  * aTables,
                                    qmnPlan           ** aPlan );

    static IDE_RC    makeMultiDETE( qcStatement   * aStatement,
                                    qmsQuerySet   * aQuerySet,
                                    qmoDETEInfo   * aDETEInfo,
                                    qmnPlan       * aChildPlan,
                                    qmnPlan       * aPlan );
    // PROJ-2205 rownum in DML
    // MOVE ����� ����
    static IDE_RC    initMOVE( qcStatement   * aStatement ,
                               qmnPlan      ** aPlan );
    
    static IDE_RC    makeMOVE( qcStatement   * aStatement ,
                               qmsQuerySet   * aQuerySet ,
                               qmoMOVEInfo   * aMOVEInfo ,
                               qmnPlan       * aChildPlan ,
                               qmnPlan       * aPlan );
    
    static IDE_RC    initDLAY( qcStatement  * aStatement ,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeDLAY( qcStatement  * aStatement ,
                               qmsQuerySet  * aQuerySet ,
                               qmnPlan      * aChildPlan ,
                               qmnPlan      * aPlan );

    // PROJ-2638
    static IDE_RC    initSDSE( qcStatement  * aStatement,
                               qmnPlan      * aParent,
                               qmnPlan     ** aPlan );

    static IDE_RC    makeSDSE( qcStatement       * aStatement,
                               qmnPlan           * aParent,
                               qcNamePosition    * aShardQuery,
                               sdiAnalyzeInfo    * aShardAnalysis,
                               qcShardParamInfo  * aShardParamInfo, /* TASK-7219 */
                               UShort              aShardParamCount,
                               qmgGraph          * aGraph,
                               qmnPlan           * aPlan );

    /* TASK-7219 Non-shard DML */
    static void setOutRefBindReassign( qtcNode          * aFilter,
                                       qcShardParamInfo * aShardParamInfo );

    static UInt getOutRefBindDataSize( mtcTemplate * aTemplate,
                                       qmncSDSE    * aCodePlan );

    static IDE_RC    initSDEX( qcStatement   * aStatement,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeSDEX( qcStatement      * aStatement,
                               qcNamePosition   * aShardQuery,
                               sdiAnalyzeInfo   * aShardAnalysis,
                               qcShardParamInfo * aShardParamInfo, /* TASK-7219 Non-shard DML */
                               UShort             aShardParamCount,
                               qmnPlan          * aPlan );

    static IDE_RC    initSDIN( qcStatement   * aStatement ,
                               qmnPlan      ** aPlan );

    static IDE_RC    makeSDIN( qcStatement    * aStatement ,
                               qmoINSTInfo    * aINSTInfo ,
                               qcNamePosition * aShardQuery ,
                               sdiAnalyzeInfo * aShardAnalysis ,
                               qmnPlan        * aChildPlan ,
                               qmnPlan        * aPlan );

    //�־��� ������ ���� Predicate ó��
    static IDE_RC    processPredicate( qcStatement     * aStatement ,
                                       qmsQuerySet     * aQuerySet ,
                                       qmoPredicate    * aPredicate ,
                                       qmoPredicate    * aConstantPredicate ,
                                       qmoPredicate    * aRidPredicate,
                                       qcmIndex        * aIndex ,
                                       UShort            aTupleRowID ,
                                       qtcNode        ** aConstantFilter ,
                                       qtcNode        ** aFilter ,
                                       qtcNode        ** aLobFilter,
                                       qtcNode        ** aSubqueryFilter ,
                                       qtcNode        ** aVarKeyRange ,
                                       qtcNode        ** aVarKeyFilter ,
                                       qtcNode        ** aVarKeyRange4Filter ,
                                       qtcNode        ** aVarKeyFilter4Filter ,
                                       qtcNode        ** aFixKeyRange ,
                                       qtcNode        ** aFixKeyFilter ,
                                       qtcNode        ** aFixKeyRange4Print ,
                                       qtcNode        ** aFixKeyFilter4Print ,
                                       qtcNode        ** aRidRange,
                                       idBool          * aInSubQueryKeyRange );

    static IDE_RC makeRidRangePredicate(qmoPredicate*  aInRidPred,
                                        qmoPredicate*  aInOtherPred,
                                        qmoPredicate** aOutRidPred,
                                        qmoPredicate** aOutOtherPred,
                                        qtcNode**      aRidRange);

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // processPredicate ȣ���� filter, subquery filter, key range�� ����
    // ó���� �Ѵ�.
    static IDE_RC postProcessScanMethod( qcStatement    * aStatement,
                                         qmncScanMethod * aMethod,
                                         idBool         * aScanLimit );

    //index�� preserved order�� ���� traverse direction�� ����
    static IDE_RC    setDirectionInfo( UInt               * aFlag ,
                                       qcmIndex           * aIndex,
                                       qmgPreservedOrder * aPreserveredOrder );

private:

    //�ش� Tuple�� ���� Storage �Ӽ��� ã�� flag�� �����Ѵ�.
    static IDE_RC    setTableTypeFromTuple( qcStatement   * aStatement ,
                                            UInt            aTupleID ,
                                            UInt          * aFlag );

    //�ش� Tuple�� ���� Storage�� �޸� ���̺����� ã�´�
    static idBool    isMemoryTableFromTuple( qcStatement   * aStatement ,
                                             UShort          aTupleID );

    //FixKey(range ,filter) �� VarKey(range ,filter)�� ����
    static IDE_RC    classifyFixedNVariable( qcStatement    * aStatement ,
                                             qmsQuerySet    * aQuerySet ,
                                             qmoPredicate  ** aKeyPred ,
                                             qtcNode       ** aFixKey ,
                                             qtcNode       ** aFixKey4Print ,
                                             qtcNode       ** aVarKey ,
                                             qtcNode       ** aVarKey4Filter ,
                                             qmoPredicate  ** aFilter );


    // fix BUG-19074
    static IDE_RC postProcessCuntMethod( qcStatement    * aStatement,
                                         qmncScanMethod * aMethod );

    /*
     * PROJ-1832 New database link
     */

    static IDE_RC allocAndCopyRemoteTableInfo(
        qcStatement         * aStatement,
        struct qmsTableRef  * aTableRef,
        SChar              ** aDatabaseLinkName,
        SChar              ** aRemoteQuery );

    static IDE_RC setCursorPropertiesForRemoteTable(
        qcStatement         * aStatement,
        smiCursorProperties * aCursorProperties,
        idBool                aIsStore, /* BUG-37077 REMOTE_TABLE_STORE */
        SChar               * aDatabaseLinkName,
        SChar               * aRemoteQuery,
        UInt                  aColumnCount,
        qcmColumn           * aColumns );

    // PROJ-2551 simple query ����ȭ
    static IDE_RC checkSimplePROJ( qcStatement  * aStatement,
                                   qmncPROJ     * aPROJ );

    static IDE_RC checkSimpleSCAN( qcStatement  * aStatement,
                                   qmncSCAN     * aSCAN );

    static IDE_RC checkSimpleSCANValue( qcStatement  * aStatement,
                                        qmnValueInfo * aValueInfo,
                                        qtcNode      * aColumnNode,
                                        qtcNode      * aValueNode,
                                        UInt         * aOffset,
                                        idBool       * aIsSimple );
    
    static IDE_RC checkSimpleINST( qcStatement  * aStatement,
                                   qmncINST     * aINST );
    
    static IDE_RC checkSimpleINSTValue( qcStatement  * aStatement,
                                        qmnValueInfo * aValueInfo,
                                        mtcColumn    * aColumn,
                                        qmmValueNode * aValueNode,
                                        UInt         * aOffset,
                                        idBool       * aIsSimple );
    
    static IDE_RC checkSimpleUPTE( qcStatement  * aStatement,
                                   qmncUPTE     * aUPTE );
    
    static IDE_RC checkSimpleUPTEValue( qcStatement  * aStatement,
                                        qmnValueInfo * aValueInfo,
                                        mtcColumn    * aColumn,
                                        qmmValueNode * aValueNode,
                                        UInt         * aOffset,
                                        idBool       * aIsSimple );
    
    static IDE_RC checkSimpleDETE( qcStatement  * aStatement,
                                   qmncDETE     * aDETE );

    /* PROJ-2632 */
    static IDE_RC buildSerialFilterInfo(  qcStatement          * aStatement,
                                          qmsHints             * aHints,
                                          qtcNode              * aFilter,
                                          UInt                 * aFilterSize,
                                          UInt                 * aFilterCount,
                                          mtxSerialFilterInfo ** aInfo );

};

#endif /* _O_QMO_ONE_CHILD_NON_MATER_H_ */
