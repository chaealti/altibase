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
 * $Id: qmgSelection.h 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Selection Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_SELECTION_H_
#define _O_QMG_SELECTION_H_ 1

#include <qc.h>
#include <qmgDef.h>


//---------------------------------------------------
// Selection Graph�� Define ���
//---------------------------------------------------

// PROJ-1446 Host variable�� ������ ���� ����ȭ
// local definition
// optimize() �Լ��� getBestAccessMethod() �Լ����� �ǻ���뿡 ����.
#define QMG_NOT_USED_SCAN_HINT        (0)
#define QMG_USED_SCAN_HINT            (1)
#define QMG_USED_ONLY_FULL_SCAN_HINT  (2)

// To Fix PR-11937
// qmgSELT.graph.flag
// FULL SCAN ��Ʈ�� ����Ǿ������� ����
//    FULL SCAN ��Ʈ�� ����� ���
//    �ش� Table�� right�� �Ǿ� Index Nested Loop Join�� ����� �� ����.
#define QMG_SELT_FULL_SCAN_HINT_MASK            (0x20000000)
#define QMG_SELT_FULL_SCAN_HINT_FALSE           (0x00000000)
#define QMG_SELT_FULL_SCAN_HINT_TRUE            (0x20000000)

// qmgSELT.graph.flag
// Not Null Key Range Flag�� ���Ǵ� ���
// (1) indexable Min Max�� ����� Selection Graph�� ���
#define QMG_SELT_NOTNULL_KEYRANGE_MASK          (0x40000000)
#define QMG_SELT_NOTNULL_KEYRANGE_FALSE         (0x00000000)
#define QMG_SELT_NOTNULL_KEYRANGE_TRUE          (0x40000000)

// PROJ-1502 PARTITIONED DISK TABLE
// qmgPartition �׷��������� ���� ���� flag�� �����Ѵ�.
// QMG_SELT_FULL_SCAN_HINT_MASK
// QMG_SELT_NOTNULL_KEYRANGE_MASK

// PROJ-1502 PARTITIONED DISK TABLE
// partition�� ���� selection graph�� ���
#define QMG_SELT_PARTITION_MASK                 (0x80000000)
#define QMG_SELT_PARTITION_FALSE                (0x00000000)
#define QMG_SELT_PARTITION_TRUE                 (0x80000000)

#define QMG_SELT_FLAG_CLEAR                     (0x00000000)

/*
 * PROJ-2402 Parallel Table Scan
 * qmgSELT.mFlag
 */
#define QMG_SELT_PARALLEL_SCAN_MASK             (0x00000001)
#define QMG_SELT_PARALLEL_SCAN_TRUE             (0x00000001)
#define QMG_SELT_PARALLEL_SCAN_FALSE            (0x00000000)

/*
 * PROJ-2641 Hierarchy Query Index
 */
#define QMG_BEST_ACCESS_METHOD_HIERARCHY_MASK   (0x00000001)
#define QMG_BEST_ACCESS_METHOD_HIERARCHY_FALSE  (0x00000000)
#define QMG_BEST_ACCESS_METHOD_HIERARCHY_TRUE   (0x00000001)

#define QMG_HIERARCHY_QUERY_DISK_IO_ADJUST_VALUE (10)

//---------------------------------------------------
// Selection Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgSELT
{
    qmgGraph          graph;    // ���� Graph ����

    qmsLimit        * limit;    // SCAN Limit ����ȭ �����, limit ���� ����

    //------------------------------------------------
    // Access Method�� ���� ����
    //     - selectedIndex : ���õ� AccessMethod�� FULL SCAN�� �ƴ� ���,
    //                       ���õ� AccessMethod Index
    //     - accessMethodCnt : �ش� Table�� index ���� + 1
    //     - accessMethod : �� accessMethod ������ Cost ����
    //------------------------------------------------
    qcmIndex        * selectedIndex;
    qmoAccessMethod * selectedMethod; // ���õ� Access Method
    UInt              accessMethodCnt;
    qmoAccessMethod * accessMethod;

    qmoScanDecisionFactor * sdf;

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition�� ���� selection graph�� ���
    qmsPartitionRef * partitionRef;

    /* BUG-44659 �̻�� Partition�� ��� ������ ����ϴٰ�,
     *           Graph�� Partition/Column/Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  qmgSELT���� Partition Name�� �����ϵ��� �����մϴ�.
     */
    SChar             partitionName[QC_MAX_OBJECT_NAME_LEN + 1];

    idBool            forceIndexScan;
    idBool            forceRidScan;    // index table scan�� ���

    UInt              mFlag;
} qmgSELT;

//---------------------------------------------------
// Selection Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgSelection
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmsFrom     * aFrom,
                         qmgGraph   ** aGraph );

    static IDE_RC  init( qcStatement     * aStatement,
                         qmsQuerySet     * aQuerySet,
                         qmsFrom         * aFrom,
                         qmsPartitionRef * aPartitionRef,
                         qmgGraph       ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC optimize( qcStatement * aStatement,
                            qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC makePlan( qcStatement * aStatement,
                            const qmgGraph * aParent, 
                            qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC printGraph( qcStatement  * aStatement,
                              qmgGraph     * aGraph,
                              ULong          aDepth,
                              iduVarString * aString );

    static IDE_RC printAccessMethod( qcStatement     * aStatement,
                                     qmoAccessMethod * aAccessMethod,
                                     ULong             aDepth,
                                     iduVarString    * aString );

    // PROJ-1502 PARTITIONED DISK TABLE - BEGIN -
    static IDE_RC optimizePartition(
        qcStatement * aStatement,
        qmgGraph    * aGraph );

    static IDE_RC makePlanPartition(
        qcStatement    * aStatement,
        const qmgGraph * aParent, 
        qmgGraph       * aGraph );

    static IDE_RC printGraphPartition(
        qcStatement  * aStatement,
        qmgGraph     * aGraph,
        ULong          aDepth,
        iduVarString * aString );
    // PROJ-1502 PARTITIONED DISK TABLE - END -

    // Preserved Order�� ����� �Լ�
    static IDE_RC makePreservedOrder( qcStatement        * aStatement,
                                      qmoIdxCardInfo     * aIdxCardInfo,
                                      UShort               aTable,
                                      qmgPreservedOrder ** aPreservedOrder );

    // ���� ���� accessMethod�� ã���ִ� �Լ�
    static IDE_RC getBestAccessMethod(qcStatement     * aStatement,
                                      qmgGraph        * aGraph,
                                      qmoStatistics   * aStatInfo,
                                      qmoPredicate    * aPredicate,
                                      qmoAccessMethod * aAccessMethod,
                                      qmoAccessMethod ** aSelectedAccessMethod,
                                      UInt             * aAccessMethodCnt,
                                      qcmIndex        ** aSelectedIndex,
                                      UInt             * aSelectedScanHint,
                                      UInt               aParallelDegree,
                                      UInt               aFlag );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    static IDE_RC getBestAccessMethodInExecutionTime(
        qcStatement     * aStatement,
        qmgGraph        * aGraph,
        qmoStatistics   * aStatInfo,
        qmoPredicate    * aPredicate,
        qmoAccessMethod * aAccessMethod,
        qmoAccessMethod ** aSelectedAccessMethod );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� graph�� ���� access method�� �ٲ� ���
    // selection graph�� sdf�� disable ��Ų��.
    static IDE_RC alterSelectedIndex( qcStatement * aStatement,
                                      qmgSELT     * aGraph,
                                      qcmIndex    * aNewIndex );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� JOIN graph���� ANTI�� ó���� ��
    // ���� SELT graph�� �����ϴµ� �̶� �� �Լ���
    // ���ؼ� �����ϵ��� �ؾ� �����ϴ�.
    static IDE_RC copySELTAndAlterSelectedIndex(
        qcStatement * aStatement,
        qmgSELT     * aSource,
        qmgSELT    ** aTarget,
        qcmIndex    * aNewSelectedIndex,
        UInt          aWhichOne );

    // PROJ-1502 PARTITIONED DISK TABLE
    // push-down join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
    static IDE_RC setJoinPushDownPredicate( qmgSELT       * aGraph,
                                            qmoPredicate ** aPredicate );

    // PROJ-1502 PARTITIONED DISK TABLE
    // push-down non-join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
    static IDE_RC setNonJoinPushDownPredicate( qmgSELT       * aGraph,
                                               qmoPredicate ** aPredicate );

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

    // View Graph�� �����Ͽ� aGraph�� left�� ����
    static IDE_RC makeViewGraph( qcStatement * aStatement, qmgGraph * aGraph );

    // PROJ-1624 global non-partitioned index
    // index table scan�� graph�� �����Ѵ�.
    static IDE_RC alterForceRidScan( qcStatement * aStatement,
                                     qmgGraph    * aGraph );

    // PROJ-2242
    static void setFullScanMethod( qcStatement      * aStatement,
                                   qmoStatistics    * aStatInfo,
                                   qmoPredicate     * aPredicate,
                                   qmoAccessMethod  * aAccessMethod,
                                   UInt               aParallelDegree,
                                   idBool             aInExecutionTime );

    /* TASK-7219 Non-shard DML */
    static void isForcePushedPredForShardView( mtcNode * aNode,
                                               idBool  * aIsFound );
private:
    // SCAN�� ����
    static IDE_RC makeTableScan( qcStatement * aStatement,
                                 qmgSELT     * aMyGraph );

    // VSCN�� ����
    static IDE_RC makeViewScan( qcStatement * aStatement,
                                qmgSELT     * aMyGraph );

    // PROJ-2528 recursive with
    // recursive VSCN
    static IDE_RC makeRecursiveViewScan( qcStatement * aStatement,
                                         qmgSELT     * aMyGraph );
    
    /* PROJ-2402 Parallel Table Scan */
    static IDE_RC makeParallelScan(qcStatement* aStatement, qmgSELT* aMyGraph);
    static void setParallelScanFlag(qcStatement* aStatement, qmgGraph* aGraph);

    // VIEW index hint�� view ���� base table�� ����
    static IDE_RC setViewIndexHints( qcStatement         * aStatement,
                                     qmsTableRef         * aTableRef );

    // VIEW index hint�� �����ϱ� ���� set�� �ƴ� querySet�� ã�� hint ����
    static IDE_RC findQuerySet4ViewIndexHints( qcStatement   * aStatement,
                                               qmsQuerySet       * aQuerySet,
                                               qmsTableAccessHints * aAccessHint );

    // VIEW index hint�� �ش��ϴ� base table�� ã�Ƽ� hint ����
    static IDE_RC findBaseTableNSetIndexHint( qcStatement * aStatement,
                                              qmsFrom     * aFrom,
                                              qmsTableAccessHints * aAccessHint );

    // VIEW index hint�� �ش� base table�� ����
    static IDE_RC setViewIndexHintInBaseTable( qcStatement * aStatement,
                                               qmsFrom     * aFrom,
                                               qmsTableAccessHints * aAccessHint );

    // PROJ-1473 view�� ���� push projection ����
    static IDE_RC doViewPushProjection( qcStatement  * aViewStatement,
                                        qmsTableRef  * aViewTableRef,
                                        qmsQuerySet  * aQuerySet );

    // PROJ-1473 view�� ���� push projection ����
    static IDE_RC doPushProjection( qcStatement  * aViewStatement,
                                    qmsTableRef  * aViewTableRef,
                                    qmsQuerySet  * aQuerySet );    

    // fix BUG-20939
    static void setViewPushProjMask( qcStatement * aStatement,
                                     qtcNode     * aNode );

    // BUG-18367 view�� ���� push selection ����
    static IDE_RC doViewPushSelection( qcStatement  * aStatement,
                                       qmsTableRef  * aTableRef,
                                       qmoPredicate * aPredicate,
                                       qmgGraph     * aGraph,
                                       idBool       * aIsPushed );

    // PROJ-2242
    static IDE_RC setIndexScanMethod( qcStatement      * aStatement,
                                      qmgGraph         * aGraph,
                                      qmoStatistics    * aStatInfo,
                                      qmoIdxCardInfo   * aIdxCardInfo,
                                      qmoPredicate     * aPredicate,
                                      qmoAccessMethod  * aAccessMethod,
                                      idBool             aIsMemory,
                                      idBool             aInExecutionTime );

    // view�� preserved order�� �����ϰ�, Table ID�� �����ϴ� �Լ�
    static IDE_RC copyPreservedOrderFromView( qcStatement        * aStatement,
                                              qmgGraph           * aChildGraph,
                                              UShort               aTableId,
                                              qmgPreservedOrder ** aNewOrder );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // host variable�� ���� ����ȭ�� ���� �غ��Ѵ�.
    static IDE_RC prepareScanDecisionFactor( qcStatement * aStatement,
                                             qmgSELT     * aGraph );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // predicate�� SDF�� ������ ���� offset�� �����Ѵ�.
    static IDE_RC setSelectivityOffset( qcStatement  * aStatement,
                                        qmoPredicate * aPredicate );

    static IDE_RC setMySelectivityOffset( qcStatement  * aStatement,
                                          qmoPredicate * aPredicate );

    static IDE_RC setTotalSelectivityOffset( qcStatement  * aStatement,
                                             qmoPredicate * aPredicate );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // getBestAccessMethod()���� �и��� �Լ�
    // �־��� access method�� �߿��� �ֻ��� method�� �����Ѵ�.
    static IDE_RC selectBestMethod( qcStatement      * aStatement,
                                    qcmTableInfo     * aTableInfo,
                                    qmoAccessMethod  * aAccessMethod,
                                    UInt               aAccessMethodCnt,
                                    qmoAccessMethod ** aSelected );

    static IDE_RC mergeViewTargetFlag( qcStatement * aDescView,
                                       qcStatement * aSrcView );
};

#endif /* _O_QMG_SELECTION_H_ */
