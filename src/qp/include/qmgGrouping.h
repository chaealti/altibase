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
 * $Id: qmgGrouping.h 88575 2020-09-15 02:33:04Z ahra.cho $
 *
 * Description :
 *     Grouping Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_GROUPING_H_
#define _O_QMG_GROUPING_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoDef.h>

//---------------------------------------------------
// Grouping Graph�� Define ���
//---------------------------------------------------

// qmgGROP.graph.flag
#define QMG_GROP_TYPE_MASK                      (0x10000000)
#define QMG_GROP_TYPE_GENERAL                   (0x00000000)
#define QMG_GROP_TYPE_NESTED                    (0x10000000)

// qmgGROP.graph.flag
#define QMG_GROP_OPT_TIP_MASK                   (0x0F000000)
#define QMG_GROP_OPT_TIP_NONE                   (0x00000000)
#define QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY      (0x01000000)
#define QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG  (0x02000000)
#define QMG_GROP_OPT_TIP_COUNT_STAR             (0x03000000)
#define QMG_GROP_OPT_TIP_INDEXABLE_MINMAX       (0x04000000)
#define QMG_GROP_OPT_TIP_ROLLUP                 (0x05000000)
#define QMG_GROP_OPT_TIP_CUBE                   (0x06000000)
#define QMG_GROP_OPT_TIP_GROUPING_SETS          (0x07000000)

//---------------------------------------------------
// Grouping Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgGROP
{
    qmgGraph           graph;         // ���� Graph ����

    //--------------------------------------------------
    // Grouping Graph ���� ����
    //    - agg : aggregation ���� 
    //      �Ϲ� Grouping�� ���, aggsDepth1�� ����
    //      Nested Grouping�� ���, aggsDepth2�� ����
    //    - group : group by ����
    //    - having : having ����
    //    - distAggArg : Aggregation�� argument�� bucket count ����
    //      (�� ���, Sort based grouping���θ� ���� ����)
    //
    //--------------------------------------------------

    qmsAggNode       * aggregation;
    qmsConcatElement * groupBy;
    qtcNode          * having;
    qmoPredicate     * havingPred;
    qmoDistAggArg    * distAggArg;    
    
    UInt               hashBucketCnt; // Hash Based Grouping�� ���, ����
    
} qmgGROP;

//---------------------------------------------------
// Grouping Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgGrouping
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aChildGraph,
                         idBool        aIsNested, 
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

private:
    static IDE_RC makeChildPlan( qcStatement * aStatement,
                                 qmgGROP     * aMyGraph );

    static IDE_RC makePRLQAndChildPlan( qcStatement * aStatement,
                                        qmgGROP     * aMyGraph );

    static IDE_RC makeHashGroup( qcStatement * aStatement,
                                 qmgGROP     * aMyGraph );

    static IDE_RC makeSortGroup( qcStatement * aStatement,
                                 qmgGROP     * aMyGraph );

    static IDE_RC makeCountAll( qcStatement * aStatement,
                                qmgGROP     * aMyGraph );

    static IDE_RC makeIndexMinMax( qcStatement * aStatement,
                                   qmgGROP     * aMyGraph );

    static IDE_RC makeIndexDistAggr( qcStatement * aStatement,
                                     qmgGROP     * aMyGraph );

    // PROJ-1353 Rollup Plan ����
    static IDE_RC makeRollup( qcStatement    * aStatement,
                              qmgGROP        * aMyGraph );

    // PROJ-1353 Cube Plan ����
    static IDE_RC makeCube( qcStatement    * aStatement,
                            qmgGROP        * aMyGraph );

    // Grouping Method�� ����(GROUP BY �� ���� ����ȭ�� ����)
    static IDE_RC setGroupingMethod( qcStatement        * aStatement,
                                     qmgGROP            * aGroupGraph,
                                     SDouble              aRecordSize,
                                     SDouble              aAggrCost );

    // GROUP BY �÷� ������ �̿��Ͽ�Order �ڷ� ������ �����Ѵ�.
    static IDE_RC makeGroupByOrder( qcStatement        * aStatement,
                                    qmsConcatElement   * aGroupBy,
                                    qmgPreservedOrder ** sNewGroupByOrder );
                                  
    // indexable group by ����ȭ
    static IDE_RC indexableGroupBy( qcStatement        * aStatement,
                                    qmgGROP            * aGroupGraph,
                                    idBool             * aIndexableGroupBy );

    // count(*) ����ȭ
    static IDE_RC countStar( qcStatement      * aStatement,
                             qmgGROP          * aGroupGraph,
                             idBool           * aCountStar );
    
    
    // GROUP BY�� ���� ����� optimize tip�� ����
    static IDE_RC nonGroupOptTip( qcStatement * aStatement,
                                  qmgGROP     * aGroupGraph,
                                  SDouble       aRecordSize,
                                  SDouble       aAggrCost );

    // group column���� bucket count�� ����
    static IDE_RC getBucketCnt4Group( qcStatement  * aStatement,
                                      qmgGROP      * aGroupGraph,
                                      UInt           aHintBucketCnt,
                                      UInt         * aBucketCnt );

    // Preserved Order ����� ����Ͽ��� ����� ��� ���
    static IDE_RC getCostByPrevOrder( qcStatement      * aStatement,
                                      qmgGROP          * aGroupGraph,
                                      SDouble          * aAccessCost,
                                      SDouble          * aDiskCost,
                                      SDouble          * aTotalCost );

    /* PROJ-1353 Group Extension Preserved Order�� ���� ( ROLLUP ) */
    static IDE_RC setPreOrderGroupExtension( qcStatement * aStatement,
                                             qmgGROP     * aGroupGraph,
                                             SDouble       aRecordSize );

    static idBool checkParallelEnable( qmgGROP * aMyGraph );

    /* BUG-48132 */
    static void getPropertyGroupingMethod( qcStatement        * aStatement,
                                           qmoGroupMethodType * aGroupingMethod );     
};

#endif /* _O_QMG_GROUPING_H_ */

