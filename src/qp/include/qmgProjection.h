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
 * $Id: qmgProjection.h 90577 2021-04-13 05:07:51Z jayce.park $
 *
 * Description :
 *     Projection Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_PROJECTION_H_
#define _O_QMG_PROJECTION_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Projection Graph�� Define ���
//---------------------------------------------------

// qmgPROJ.graph.flag
// ��� ���ۿ� �� PROJ NODE ���� ����
#define QMG_PROJ_COMMUNICATION_TOP_PROJ_MASK      (0x10000000)
#define QMG_PROJ_COMMUNICATION_TOP_PROJ_FALSE     (0x00000000)
#define QMG_PROJ_COMMUNICATION_TOP_PROJ_TRUE      (0x10000000)

// qmgPROJ.graph.flag
// View Optimization Type�� View Materialization���� �ƴ����� ���� ����
#define QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK         (0x20000000)
#define QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE        (0x00000000)
#define QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE         (0x20000000)

// qmgPROJ.graph.flag
// PROJ-2462 ResultCache
#define QMG_PROJ_TOP_RESULT_CACHE_MASK          (0x40000000)
#define QMG_PROJ_TOP_RESULT_CACHE_FALSE         (0x00000000)
#define QMG_PROJ_TOP_RESULT_CACHE_TRUE          (0x40000000)

/* Proj-1715 */
#define QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK         (0x80000000)
#define QMG_PROJ_VIEW_OPT_TIP_CMTR_FALSE        (0x00000000)
#define QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE         (0x80000000)

//---------------------------------------------------
// subquery ����ȭ �� ���� ������ ��Ÿ�� flag ����
//---------------------------------------------------

/* qmgPROJ.subqueryTipFlag                             */
//---------------------------------
// subquery ����ȭ �� ���� ����
//  1. _TIP_NONE : ����� subquery ����ȭ ���� ����
//  2. _TIP_TRANSFORMNJ  : transform NJ ����
//  3. _TIP_STORENSEARCH : store and search ����
//  4. _TIP_IN_KEYRANGE  : IN���� subquery keyRange ����
//  5. _TIP_KEYRANGE     : subquery keyRange ����
//---------------------------------
# define QMG_PROJ_SUBQUERY_TIP_MASK                (0x00000007)
# define QMG_PROJ_SUBQUERY_TIP_NONE                (0x00000000)
# define QMG_PROJ_SUBQUERY_TIP_TRANSFORMNJ         (0x00000001)
# define QMG_PROJ_SUBQUERY_TIP_STORENSEARCH        (0x00000002)
# define QMG_PROJ_SUBQUERY_TIP_IN_KEYRANGE         (0x00000003)
# define QMG_PROJ_SUBQUERY_TIP_KEYRANGE            (0x00000004)

/* qmgPROJ.subqueryTipFlag                                        */
// store and search ����ȭ ���� ����Ǿ��� ����� ����������
# define QMG_PROJ_SUBQUERY_STORENSEARCH_MASK       (0x00000070)
# define QMG_PROJ_SUBQUERY_STORENSEARCH_NONE       (0X00000000)
# define QMG_PROJ_SUBQUERY_STORENSEARCH_HASH       (0x00000010)
# define QMG_PROJ_SUBQUERY_STORENSEARCH_LMST       (0x00000020)
# define QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY  (0x00000040)

/* qmgPROJ.subqueryTipFlag                                        */
//---------------------------------
// store and search�� HASH�� ����� ���, not null �˻� ���� ����.
// _HASH_NOTNULLCHECK_FALSE: ���� �÷��� ���� not null �˻縦 ���� �ʾƵ� ��.
// _HASH_NOTNULLCHECK_TRUE : ���� �÷��� ���� not null �˻縦 �ؾ� ��.
//---------------------------------
# define QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_MASK  (0x00000100)
# define QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_FALSE (0x00000000)
# define QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_TRUE  (0x00000100)

//---------------------------------------------------
// Projection Graph �� �����ϱ� ���� �ڷ� ����
//    - Top Graph ����, indexable MIN MAX, VMTR ���δ� flag�� ���� �˾Ƴ�
//---------------------------------------------------

typedef struct qmgPROJ
{
    qmgGraph   graph;    // ���� Graph ����

    qmsLimit  * limit;   // limit ����
    qtcNode   * loopNode;// loop ����
    qmsTarget * target;  // target ����

    //-----------------------------------------------
    // subquery ����ȭ �� ���� ������ ���� �ڷᱸ��
    // subqueryTipFlag : subquery ����ȭ �� ���� ����
    // storeNSearchPred: store and search�� HASH�� ����� ��쿡
    //                   plan tree���� ó���� �� �ֵ���
    //                   ���� predicate ������ �޾��ش�.
    // hashBucketCnt   : store and search�� HASH�� ����� ��� �ʿ� 
    //-----------------------------------------------
    
    UInt            subqueryTipFlag;
    qtcNode       * storeNSearchPred;
    UInt            hashBucketCnt;
} qmgPROJ;

//---------------------------------------------------
// Projection Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgProjection
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aChildGraph,
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
    // Projection Graph�� preserved order�� �����Ѵ�.
    static IDE_RC makePreservedOrder( qcStatement       * aStatement,
                                      qmsTarget         * aTarget,
                                      qmgPreservedOrder * aChildPreservedOrder,
                                      qmgPreservedOrder ** aMyPreservedOrder );

    //-----------------------------------------
    // To Fix PR-7307
    // makePlan()�� ���� �Լ�
    //-----------------------------------------

    static IDE_RC  makeChildPlan( qcStatement * aStatement,
                                  qmgPROJ     * aGraph );

    static IDE_RC  makePlan4Proj( qcStatement * aStatement,
                                  qmgPROJ     * aGraph,
                                  UInt          aPROJFlag );

    // View Materialization�� ���� Plan Tree ����
    static IDE_RC  makePlan4ViewMtr( qcStatement    * aStatement,
                                     qmgPROJ        * aGraph,
                                     UInt             aPROJFlag );

    // Store And Search�� ���� Plan Tree ����
    static IDE_RC  makePlan4StoreSearch( qcStatement    * aStatement,
                                         qmgPROJ        * aGraph,
                                         UInt             aPROJFlag );

    // In Subquery KeyRange�� ���� Plan Tree ����
    static IDE_RC  makePlan4InKeyRange( qcStatement * aStatement,
                                        qmgPROJ     * aGraph,
                                        UInt          aPROJFlag );

    // Subquery KeyRange�� ���� Plan Tree ����
    static IDE_RC  makePlan4KeyRange( qcStatement * aStatement,
                                      qmgPROJ     * aGraph,
                                      UInt          aPROJFlag );

    // BUG-48779 Projection�� limit�� scan���� ������ �� �ִ��� Ȯ���Ѵ�.
    static void canPushDownLimitIntoScan( qmgGraph * aGraph,
                                          idBool   * aIsScanLimit );
};

#endif /* _O_QMG_PROJECTION_H_ */

