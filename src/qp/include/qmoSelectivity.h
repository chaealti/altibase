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
 * $Id:  $
 *
 * Description : Selectivity Manager
 *
 *        - Unit predicate �� ���� selectivity ���
 *        - qmoPredicate �� ���� selectivity ���
 *        - qmoPredicate list �� ���� ���� selectivity ���
 *        - qmoPredicate wrapper list �� ���� ���� selectivity ���
 *        - �� graph �� ���� selectivity ���
 *        - �� graph �� ���� output record count ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qtcDef.h>
#include <qcmTableInfo.h>
#include <qmoPredicate.h>
#include <qmoCnfMgr.h>

#ifndef _Q_QMO_NEW_SELECTIVITY_H_
#define _Q_QMO_NEW_SELECTIVITY_H_ 1

//---------------------------------------------------
// feasibility�� ���డ���� predicate�� �������� �ʴ� ���,
// selectivity�� ����� �� ������ ��ȯ�Ѵ�.
//---------------------------------------------------
# define QMO_SELECTIVITY_NOT_EXIST               (1)

//---------------------------------------------------
// selectivity�� �����Ҽ� ������ ����Ѵ�.
//---------------------------------------------------
# define QMO_SELECTIVITY_UNKNOWN               (0.05)

//---------------------------------------------------
// PROJ-1353 
// qmgGrouping �� ���� ROLLUP factor
//---------------------------------------------------
# define QMO_SELECTIVITY_ROLLUP_FACTOR           (0.75)

//---------------------------------------------------
// Selectivity�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoSelectivity
{
public:

    //-----------------------------------------------
    // SCAN ���� selectivity ���
    //-----------------------------------------------

    // qmoPredicate.mySelectivity ���
    // (BUGBUG : host ���� ���� ���ε� ��, �� ����)
    static IDE_RC setMySelectivity( qcStatement   * aStatement ,
                                    qmoStatistics * aStatInfo,
                                    qcDepInfo     * aDepInfo,
                                    qmoPredicate  * aPredicate );

    // qmoPredicate.totalSelectivity ���
    // (BUGBUG : host ���� ���� ���ε� ��, �� ����)
    static IDE_RC setTotalSelectivity( qcStatement   * aStatement,
                                       qmoStatistics * aStatInfo,
                                       qmoPredicate  * aPredicate );

    // execution time�� selectivity �ٽ� ���
    // host ������ �ִ� ��� ���ε��� ���� �����Ͽ� ���Ѵ�.
    // qmoPredicate.mySelectivityOffset ��
    // qmoPredicate.totalSelectivityOffset �� ȹ���Ѵ�.
    static IDE_RC recalculateSelectivity( qcTemplate    * aTemplate,
                                          qmoStatistics * aStatInfo,
                                          qcDepInfo     * aDepInfo,
                                          qmoPredicate  * aRootPredicate );

    // index�� keyNDV �� �̿��Ͽ� selectivity �� ����Ѵ�.
    // ������� ���ϴ� ��쿡�� getSelectivity4PredWrapper �� ȣ����
    static IDE_RC getSelectivity4KeyRange( qcTemplate      * aTemplate,
                                           qmoStatistics   * aStatInfo,
                                           qmoIdxCardInfo  * aIdxCardInfo,
                                           qmoPredWrapper  * aKeyRange,
                                           SDouble         * aSelectivity,
                                           idBool            aInExecutionTime );

    // scan method �� ���õ� qmoPredWrapper list �� ���� ���� selectivity ��ȯ
    static IDE_RC getSelectivity4PredWrapper( qcTemplate     * aTemplate,
                                              qmoPredWrapper * aPredWrapper,
                                              SDouble        * aSelectivity,
                                              idBool           aInExecutionTime );

    // qmoPredicate list �� ���� ���� totalSelectivity ��ȯ
    // qmgSelection and qmgHierarchy ����ȭ �������� �̿�
    static IDE_RC getTotalSelectivity4PredList( qcStatement  * aStatement,
                                                qmoPredicate * aPredList,
                                                SDouble      * aSelectivity,
                                                idBool         aInExecutionTime );

    //-----------------------------------------------
    // JOIN ���� selectivity, output record count, joinOrderFactor ���
    //-----------------------------------------------

    // Join �� ���� qmoPredicate.mySelectivity ���
    static IDE_RC setMySelectivity4Join( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qmoPredicate * aJoinPredList,
                                         idBool         aIsSetNext );

    // qmgJoin �� ���� selectivity ���
    static IDE_RC setJoinSelectivity( qcStatement  * aStatement,
                                      qmgGraph     * aGraph,
                                      qmoPredicate * aJoinPred,
                                      SDouble      * aSelectivity );

    // qmgLeftOuter, qmgFullOuter �� ���� selectivity ���
    static IDE_RC setOuterJoinSelectivity( qcStatement  * aStatement,
                                           qmgGraph     * aGraph,
                                           qmoPredicate * aWherePred,
                                           qmoPredicate * aOnJoinPred,
                                           qmoPredicate * aOneTablePred,
                                           SDouble      * aSelectivity );

    // qmgLeftOuter �� ���� output record count ���
    static IDE_RC setLeftOuterOutputCnt( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qcDepInfo    * aLeftDepInfo,
                                         qmoPredicate * aWherePred,
                                         qmoPredicate * aOnJoinPred,
                                         qmoPredicate * aOneTablePred,
                                         SDouble        aLeftOutputCnt,
                                         SDouble        aRightOutputCnt,
                                         SDouble      * aOutputRecordCnt );

    // qmgFullOuter �� ���� output record count ���
    static IDE_RC setFullOuterOutputCnt( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qcDepInfo    * aLeftDepInfo,
                                         qmoPredicate * aWherePred,
                                         qmoPredicate * aOnJoinPred,
                                         qmoPredicate * aOneTablePred,
                                         SDouble        aLeftOutputCnt,
                                         SDouble        aRightOutputCnt,
                                         SDouble      * aOutputRecordCnt );

    // Join ordering ���� JoinOrderFactor, JoinSize ���
    static IDE_RC setJoinOrderFactor( qcStatement  * aStatement,
                                      qmgGraph     * aJoinGraph,
                                      qmoPredicate * aJoinPred,
                                      SDouble      * aJoinOrderFactor,
                                      SDouble      * aJoinSize );

    //-----------------------------------------------
    // �� �� graph �� ���� selectivity, output record count ���
    //-----------------------------------------------

    // qmgHierarchy �� ���� selectivity ���
    static IDE_RC setHierarchySelectivity( qcStatement  * aStatement,
                                           qmoPredicate * aWherePredList,
                                           qmoCNF       * aConnectByCNF,
                                           qmoCNF       * aStartWithCNF,
                                           idBool         aInExecutionTime,
                                           SDouble      * aSelectivity );

    // qmgCounting �� ���� selectivity ���
    static IDE_RC setCountingSelectivity( qmoPredicate * aStopkeyPred,
                                          SLong          aStopRecordCnt,
                                          SDouble        aInputRecordCnt,
                                          SDouble      * aSelectivity );

    // qmgCounting �� ���� output record count ���
    static IDE_RC setCountingOutputCnt( qmoPredicate * aStopkeyPred,
                                        SLong          aStopRecordCnt,
                                        SDouble        aInputRecordCnt,
                                        SDouble      * aOutputRecordCnt );

    // qmgProjection �� ���� selectivity ���
    static IDE_RC setProjectionSelectivity( qmsLimit * aLimit,
                                            SDouble    aInputRecordCnt,
                                            SDouble  * aSelectivity );

    // qmgGrouping �� ���� selectivity ���
    static IDE_RC setGroupingSelectivity( qmgGraph     * aGraph,
                                          qmoPredicate * aHavingPred,
                                          SDouble        aInputRecordCnt,
                                          SDouble      * aSelectivity );

    // qmgGrouping �� ���� output record count ���
    static IDE_RC setGroupingOutputCnt( qcStatement      * aStatement,
                                        qmgGraph         * aGraph,
                                        qmsConcatElement * aGroupBy,
                                        SDouble            aInputRecordCnt,
                                        SDouble            aSelectivity,
                                        SDouble          * aOutputRecordCnt );

    // qmgDistinction �� ���� output record count ���
    static IDE_RC setDistinctionOutputCnt( qcStatement * aStatement,
                                           qmsTarget   * aTarget,
                                           SDouble       aInputRecordCnt,
                                           SDouble     * aOutputRecordCnt );

    // qmgSet �� ���� output record count ���
    static IDE_RC setSetOutputCnt( qmsSetOpType   aSetOpType,
                                   SDouble        aLeftOutputRecordCnt,
                                   SDouble        aRightOutputRecordCnt,
                                   SDouble      * aOutputRecordCnt );

    // qmoPredicate �� ���� mySelectivity �� ��ȯ
    static SDouble getMySelectivity( qcTemplate   * aTemplate,
                                     qmoPredicate * aPredicate,
                                     idBool         aInExecutionTime );

    // PROJ-2582 recursive with
    static IDE_RC setSetRecursiveOutputCnt( SDouble   aLeftOutputRecordCnt,
                                            SDouble   aRightOutputRecordCnt,
                                            SDouble * aOutputRecordCnt );
    
private:

    //-----------------------------------------------
    // SCAN ���� selectivity ���
    //-----------------------------------------------

    /* qmoPredicate.mySelectivity */

    // execution time�� �Ҹ��� setMySelectivity �Լ�
    // host ������ ���� ���ε��� �� scan method �籸���� ����
    // template->data + qmoPredicate.mySelectivityOffset ��
    // mySelectivity �� �缳���Ѵ�. (qmo::optimizeForHost ����)
    static IDE_RC setMySelectivityOffset( qcTemplate    * aTemplate,
                                          qmoStatistics * aStatInfo,
                                          qcDepInfo     * aDepInfo,
                                          qmoPredicate  * aPredicate );

    /* qmoPredicate.totalSelectivity */

    // execution time�� �Ҹ��� setTotalSelectivity �Լ�
    // host ������ ���� ���ε��� �� scan method �籸���� ����
    // template->data + qmoPredicate.totalSelectivityOffset ��
    // totalSelectivity �� �缳���Ѵ�. (qmo::optimizeForHost ����)
    static IDE_RC setTotalSelectivityOffset( qcTemplate    * aTemplate,
                                             qmoStatistics * aStatInfo,
                                             qmoPredicate  * aPredicate );

    // qmoPredicate �� ���� totalSelectivity �� ��ȯ
    inline static SDouble getTotalSelectivity( qcTemplate   * aTemplate,
                                               qmoPredicate * aPredicate,
                                               idBool         aInExecutionTime );

    // setTotalSelectivity ���� ȣ��Ǿ� ���յ� total selectivity ���
    static IDE_RC getIntegrateMySelectivity( qcTemplate    * aTemplate,
                                             qmoStatistics * aStatInfo,
                                             qmoPredicate  * aPredicate,
                                             SDouble       * aTotalSelectivity,
                                             idBool          aInExecutionTime );

    /* Unit selectivity */

    // Unit predicate �� ���� scan selectivity ���
    static IDE_RC getUnitSelectivity( qcTemplate    * aTemplate,
                                      qmoStatistics * aStatInfo,
                                      qcDepInfo     * aDepInfo,
                                      qmoPredicate  * aPredicate,
                                      qtcNode       * aCompareNode,
                                      SDouble       * aSelectivity,
                                      idBool          aInExecutionTime );
                                       
    // =, !=(<>) �� ���� unit selectivity ���
    static IDE_RC getEqualSelectivity( qcTemplate    * aTemplate,
                                       qmoStatistics * aStatInfo,
                                       qcDepInfo     * aDepInfo,
                                       qmoPredicate  * aPredicate,
                                       qtcNode       * aCompareNode,
                                       SDouble       * aSelectivity );

    // IS NULL, IS NOT NULL �� ���� unit selectivity ���
    static IDE_RC getIsNullSelectivity( qcTemplate    * aTemplate,
                                        qmoStatistics * aStatInfo,
                                        qmoPredicate  * aPredicate,
                                        qtcNode       * aCompareNode,
                                        SDouble       * aSelectivity );

    // >, >=, <, <=, BETWEEN, NOT BETWEEN �� ���� unit selectivity ���
    static IDE_RC getGreaterLessBeetweenSelectivity( qcTemplate    * aTemplate,
                                                     qmoStatistics * aStatInfo,
                                                     qmoPredicate  * aPredicate,
                                                     qtcNode       * aCompareNode,
                                                     SDouble       * aSelectivity,
                                                     idBool          aInExecutionTime );

    // getGreaterLessBeetweenSelectivity ���� ȣ��Ǿ� MIN, MAX selectivity ���
    static IDE_RC getMinMaxSelectivity( qcTemplate    * aTemplate,
                                        qmoStatistics * aStatInfo,
                                        qtcNode       * aCompareNode,
                                        qtcNode       * aColumnNode,
                                        qtcNode       * aValueNode,
                                        SDouble       * aSelectivity );

    // getMinMaxSelectivity, getIntegrateSelectivity ���� ȣ��
    // NUMBER group �� min-max selectivity ����� ���� double �� ��ȯ�� ��ȯ
    static IDE_RC getConvertToDoubleValue( mtcColumn     * aColumnColumn,
                                           mtcColumn     * aMaxValueColumn,
                                           mtcColumn     * aMinValueColumn,
                                           void          * aColumnMaxValue,
                                           void          * aColumnMinValue,
                                           void          * aMaxValue,
                                           void          * aMinValue,
                                           mtdDoubleType * aDoubleColumnMaxValue,
                                           mtdDoubleType * aDoubleColumnMinValue,
                                           mtdDoubleType * aDoubleMaxValue,
                                           mtdDoubleType * aDoubleMinValue );

    // LIKE, NOT LIKE �� ���� unit selectivity ���
    static IDE_RC getLikeSelectivity( qcTemplate    * aTemplate,
                                      qmoStatistics * aStatInfo,
                                      qmoPredicate  * aPredicate,
                                      qtcNode       * aCompareNode,
                                      SDouble       * aSelectivity,
                                      idBool          aInExecutionTime );

    // getLikeSelectivity ���� ȣ��Ǿ� keyLength ��ȯ
    static IDE_RC getLikeKeyLength( const mtdCharType   * aSource,
                                    const mtdCharType   * aEscape,
                                    UShort              * aKeyLength );

    // IN, NOT IN �� ���� unit selectivity ���
    static IDE_RC getInSelectivity( qcTemplate    * aTemplate,
                                    qmoStatistics * aStatInfo,
                                    qcDepInfo     * aDepInfo,
                                    qmoPredicate  * aPredicate,
                                    qtcNode       * aCompareNode,
                                    SDouble       * aSelectivity );

    // EQUALS, NOTEQUALS �� ���� unit selectivity ���
    static IDE_RC getEqualsSelectivity( qmoStatistics * aStatInfo,
                                        qmoPredicate  * aPredicate,
                                        qtcNode       * aCompareNode,
                                        SDouble       * aSelectivity );

    // LNNVL �� ���� unit selectivity ���
    static IDE_RC getLnnvlSelectivity( qcTemplate    * aTemplate,
                                       qmoStatistics * aStatInfo,
                                       qcDepInfo     * aDepInfo,
                                       qmoPredicate  * aPredicate,
                                       qtcNode       * aCompareNode,
                                       SDouble       * aSelectivity,
                                       idBool          aInExecutionTime );

    //-----------------------------------------------
    // JOIN ���� selectivity ���
    //-----------------------------------------------

    // Outer join ON ���� one table predicate �� ���� mySelectivity ���
    static IDE_RC setMySelectivity4OneTable( qcStatement  * aStatement,
                                             qmgGraph     * aLeftGraph,
                                             qmgGraph     * aRightGraph,
                                             qmoPredicate * aOneTablePred );

    // Join predicate list �� ���� ���� mySelectivity ��ȯ
    static IDE_RC getMySelectivity4PredList( qmoPredicate  * aPredList,
                                             SDouble       * aSelectivity );

    // BUG-37918 join selectivity ����
    static SDouble getEnhanceSelectivity4Join( qcStatement  * aStatement,
                                               qmgGraph     * aGraph,
                                               qmoStatistics* aStatInfo,
                                               qtcNode      * aNode);

    // Join predicate �� ���� unit selectivity �� ��ȯ�ϰ�
    // ���� �ε��� ��밡�ɿ��θ� �����Ѵ�.
    static IDE_RC getUnitSelectivity4Join( qcStatement * aStatement,
                                           qmgGraph    * aGraph,
                                           qtcNode     * aCompareNode,
                                           idBool      * aIsEqual,
                                           SDouble     * aSelectivity );

    // qmoPredicate list �� ���� ���� semi join selectivity ��ȯ
    static IDE_RC getSemiJoinSelectivity( qcStatement   * aStatement,
                                          qmgGraph      * aGraph,
                                          qcDepInfo     * aLeftDepInfo,
                                          qmoPredicate  * aPredList,
                                          idBool          aIsLeftSemi,
                                          idBool          aIsSetNext,
                                          SDouble       * aSelectivity );

    // Join predicate �� ���� semi join unit selectivity ��ȯ
    static IDE_RC getUnitSelectivity4Semi( qcStatement  * aStatement,
                                           qmgGraph     * aGraph,
                                           qcDepInfo    * aLeftDepInfo,
                                           qtcNode      * aCompareNode,
                                           idBool         aIsLeftSemi,
                                           SDouble      * aSelectivity );

    // qmoPredicate list �� ���� ���� anti join selectivity ��ȯ
    static IDE_RC getAntiJoinSelectivity( qcStatement   * aStatement,
                                          qmgGraph      * aGraph,
                                          qcDepInfo     * aLeftDepInfo,
                                          qmoPredicate  * aPredList,
                                          idBool          aIsLeftAnti,
                                          idBool          aIsSetNext,
                                          SDouble       * aSelectivity );

    // Join predicate �� ���� anti join unit selectivity ��ȯ
    static IDE_RC getUnitSelectivity4Anti( qcStatement  * aStatement,
                                           qmgGraph     * aGraph,
                                           qcDepInfo    * aLeftDepInfo,
                                           qtcNode      * aCompareNode,
                                           idBool         aIsLeftAnti,
                                           SDouble      * aSelectivity );

    //-----------------------------------------------
    // JOIN ordering ����
    //-----------------------------------------------

    // setJoinOrderFactor �Լ����� ȣ��
    // ����ȭ ����(predicate �̱��� ����) join predicate�� selectivity ���
    static IDE_RC getSelectivity4JoinOrder( qcStatement  * aStatement,
                                            qmgGraph     * aJoinGraph,
                                            qmoPredicate * aJoinPred,
                                            SDouble      * aSelectivity );

    // getSelectivity4JoinOrder �Լ����� ȣ��
    // ����ȭ ����(predicate �̱��� ����) join predicate�� selectivity ����
    static IDE_RC getReviseSelectivity4JoinOrder (
                                          qcStatement    * aStatement,
                                          qmgGraph       * aJoinGraph,
                                          qcDepInfo      * aChildDepInfo,
                                          qmoIdxCardInfo * aIdxCardInfo,
                                          qmoPredicate   * aPredicate,
                                          SDouble        * aSelectivity,
                                          idBool         * aIsRevise );

    // getReviseSelectivity4JoinOrder �Լ����� ȣ��
    // Equal Predicate ���� �˻�
    static IDE_RC isEqualPredicate( qmoPredicate * aPredicate,
                                    idBool       * aIsEqual );
};

#endif  /* _Q_QMO_NEW_SELECTIVITY_H_ */

