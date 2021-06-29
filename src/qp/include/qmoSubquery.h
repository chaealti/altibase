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
 * $Id: qmoSubquery.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Subquery Manager
 *
 *     ���� ó�� �߿� �����ϴ� Subquery�� ���� ����ȭ�� �����Ѵ�.
 *     Subquery�� ���Ͽ� ����ȭ�� ���� Graph ����,
 *     Graph�� �̿��� Plan Tree ������ ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_SUBQUERY_H_
#define _O_QMO_SUBQUERY_H_ 1

#include <qc.h>
#include <qmoPredicate.h>
#include <qmgProjection.h>

//---------------------------------------------------
// Subquery�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

enum qmoSubqueryType
{
    QMO_TYPE_NONE = 0,
    QMO_TYPE_A,
    QMO_TYPE_N,
    QMO_TYPE_J,
    QMO_TYPE_JA
};


//---------------------------------------------------
// Subquery�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoSubquery
{
public:

    //---------------------------------------------------
    // subquery�� ���� plan ����
    //---------------------------------------------------

    // Subquery�� ���� Plan Tree ����
    static IDE_RC    makePlan( qcStatement  * aStatement,
                               UShort         aTupleID,
                               qtcNode      * aNode );

    //---------------------------------------------------
    // ���� ���������� subquery�� ���� ����ȭ �� ����� graph ����
    //---------------------------------------------------

    //---------------------------------
    // where�� subquery�� ���� ó���� �Ѵ�.
    // subquery ��带 ã�Ƽ�,
    // predicate�� �Ǵ� expresstion���� ���� ó���� �ϰ� �ȴ�.
    // predicate�����ڰ� ȣ��
    //---------------------------------
    static IDE_RC    optimize( qcStatement   * aStatement,
                               qtcNode       * aNode,
                               idBool          aTryKeyRange );

    // Subquery�� ���� Graph ����
    static IDE_RC    makeGraph( qcStatement  * aSubQStatement );

    //---------------------------------
    // Expression�� subquery�� ���� ó��
    // qmo::optimizeInsert()���� ȣ�� :
    //      INSERT INTO T1 VALUES ( (SELECT SUM(I2) FROM T2) );
    // qmo::optimizeUpdate()���� ȣ�� :
    //      UPDATE SET I1=(SELECT SUM(I1) FROM T2) WHERE I1>1;
    //      UPDATE SET (I1,I2)=(SELECT I1,I2 FROM T2) WHERE I1>1;
    //---------------------------------
    // BUG-32854 ��� ���������� ���ؼ� MakeGraph ���Ŀ� MakePlan�� �ؾ� �Ѵ�.
    static IDE_RC    optimizeExprMakeGraph( qcStatement * aStatement,
                                            UInt          aTupleID,
                                            qtcNode     * aNode );

    static IDE_RC    optimizeExprMakePlan( qcStatement * aStatement,
                                           UInt          aTupleID,
                                           qtcNode     * aNode );
private:

    // Subquery type�� �Ǵ��Ѵ�.
    static IDE_RC    checkSubqueryType( qcStatement     * aStatement,
                                        qtcNode         * aSubqueryNode,
                                        qmoSubqueryType * aType );

    // BUG-36575
    static void      checkSubqueryDependency( qcStatement * aStatement,
                                              qmsQuerySet * aQuerySet,
                                              qtcNode     * aSubQNode,
                                              idBool      * aExist );
    
    // select���� ���� expression�� subquery ó��
    static IDE_RC     optimizeExpr4Select( qcStatement * aStatement,
                                           qtcNode     * aNode);

    // predicate�� subquery�� ���� subquery ����ȭ
    static IDE_RC    optimizePredicate( qcStatement * aStatement,
                                        qtcNode     * aCompareNode,
                                        qtcNode     * aNode,
                                        qtcNode     * aSubQNode,
                                        idBool        aTryKeyRange );

    // transform NJ ����ȭ �� ����
    static IDE_RC   transformNJ( qcStatement     * aStatement,
                                 qtcNode         * aCompareNode,
                                 qtcNode         * aNode,
                                 qtcNode         * aSubQNode,
                                 qmoSubqueryType   aSubQType,
                                 idBool            aSubqueryIsSet,
                                 idBool          * aIsTransform );

    // transform NJ ����ȭ �� �����
    // predicate column�� subquery target column�� not null �˻�
    static IDE_RC   checkNotNull( qcStatement  * aStatement,
                                  qtcNode      * aNode,
                                  qtcNode      * aSubQNode,
                                  idBool       * aIsNotNull );

    // transform NJ, store and search ����ȭ �� �����
    // subquery target column�� ���� not null �˻�
    static IDE_RC   checkNotNullSubQTarget( qcStatement * aStatement,
                                            qtcNode     * aSubQNode,
                                            idBool      * aIsNotNull );

    // transform NJ ����ȭ �� �����
    // subquery target column�� �ε����� �����ϴ��� �˻�
    static IDE_RC   checkIndex4SubQTarget( qcStatement * aStatement,
                                           qtcNode     * aSubQNode,
                                           idBool      * aIsExistIndex );

    // transform NJ ����ȭ �� �����
    // ���ο� predicate�� ����, subquery�� ���� where���� ����
    static IDE_RC   makeNewPredAndLink( qcStatement * aStatement,
                                        qtcNode     * aNode,
                                        qtcNode     * aSubQNode );

    // store and search ����ȭ �� ����
    static IDE_RC   storeAndSearch( qcStatement   * aStatement,
                                    qtcNode       * aCompareNode,
                                    qtcNode       * aNode,
                                    qtcNode       * aSubQNode,
                                    qmoSubqueryType aSubQType,
                                    idBool          aSubqueryIsSet );

    // store and search ����ȭ �� �����
    // IN(=ANY), NOT IN(!=ALL)�� ���� ������ ����
    static IDE_RC   setStoreFlagIN( qcStatement * aStatement,
                                    qtcNode     * aCompareNode,
                                    qtcNode     * aNode,
                                    qtcNode     * aSubQNode,
                                    qmsQuerySet * aQuerySet,
                                    qmgPROJ     * sPROJ,
                                    idBool        aSubqueryIsSet );

    // store and search ����ȭ �� �����
    // =ALL, !=ANY�� ���� ������ ����
    static IDE_RC   setStoreFlagEqualAll( qcStatement * aStatement,
                                          qtcNode     * aNode,
                                          qtcNode     * aSubQNode,
                                          qmsQuerySet * aQuerySet,
                                          qmgPROJ     * aPROJ,
                                          idBool        aSubqueryIsSet );

    // IN���� subquery keyRange ����ȭ �� ����
    static IDE_RC   inSubqueryKeyRange( qtcNode        * aCompareNode,
                                        qtcNode        * aSubQNode,
                                        qmoSubqueryType  aSubQType );

    // subquery keyRange ����ȭ �� ����
    static IDE_RC   subqueryKeyRange( qtcNode        * aCompareNode,
                                      qtcNode        * aSubQNode,
                                      qmoSubqueryType  aSubQType );

    // Expression�� constant subquery ����ȭ �� ����
    static IDE_RC   constantSubquery( qcStatement * aStatement,
                                      qtcNode     * aSubQNode );

    // store and search, IN subquery keyRange, transformNJ �Ǵܽ� �ʿ���
    // column cardinality�� ���Ѵ�.
    static IDE_RC   getColumnCardinality( qcStatement * aStatement,
                                          qtcNode     * aColumnNode,
                                          SDouble     * aCardinality );

    // store and search, IN subquery keyRange, transformNJ �Ǵܽ� �ʿ���
    // subquery Target cardinality�� ���Ѵ�.
    static IDE_RC   getSubQTargetCardinality( qcStatement * aStatement,
                                              qtcNode     * aSubQNode,
                                              SDouble     * aCardinality );
};

#endif /* _O_QMO_SUBQUERY_H_ */
