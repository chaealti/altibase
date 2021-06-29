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
 * $Id: qmoViewMerging.h 23857 2008-03-19 02:36:53Z sungminee $
 *
 * Description :
 *     PROJ-1413 Simple View Merging�� ���� �ڷ� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_VIEW_MERGING_H_
#define _O_QMO_VIEW_MERGING_H_ 1

#include <qc.h>
#include <qmsParseTree.h>
#include <qmoDependency.h>

#define MAKE_TUPLE_RETRY_COUNT    (100)

#define DEFAULT_VIEW_NAME         ((SChar*) "NONAME")
#define DEFAULT_VIEW_NAME_LEN     (6)

//---------------------------------------------------
// View Merging�� rollback�� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoViewRollbackInfo
{
    //---------------------------------------------------
    // Hint���� rollback ����
    //---------------------------------------------------

    idBool                    hintMerged;
    
    // join method hint�� merge�� ������ ��带 ����Ѵ�.
    qmsJoinMethodHints      * lastJoinMethod;
    
    // table access hint�� merge�� ������ ��带 ����Ѵ�.
    qmsTableAccessHints     * lastTableAccess;

    // BUG-22236
    // ���� interResultType�� ����Ѵ�. 
    qmoInterResultType        interResultType;    // DISK/MEMORY

    //---------------------------------------------------
    // Target List�� rollback ����
    //---------------------------------------------------

    idBool                    targetMerged;
    
    //---------------------------------------------------
    // From ���� rollback ����
    //---------------------------------------------------

    idBool                    fromMerged;
    
    // ���� query block�� from �� merge�� ù��° from�� ����Ѵ�.
    qmsFrom                 * firstFrom;
    
    // ���� query block�� from �� merge�� ������ from�� ����Ѵ�.
    qmsFrom                 * lastFrom;

    // merge�� tuple variable�� �̸��� ����Ѵ�.
    qcNamePosition          * oldAliasName;
    
    // merge�� tuple variable�� �̸��� ����Ѵ�.
    qcNamePosition          * newAliasName;
    
    //---------------------------------------------------
    // Where ���� rollback ����
    //---------------------------------------------------

    idBool                    whereMerged;
    
    // merge�� ���� where���� ��带 ����Ѵ�.
    qtcNode                 * currentWhere;

    // merge�� ���� where���� ��带 ����Ѵ�.
    qtcNode                 * underWhere;
    
} qmoViewRollbackInfo;


class qmoViewMerging
{
public:

    //------------------------------------------
    // (simple) view merging�� ����
    //------------------------------------------
    
    static IDE_RC  doTransform( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet );

    static IDE_RC  doTransformSubqueries( qcStatement  * aStatement,
                                          qtcNode      * aNode );

    static IDE_RC  doTransformForMultiDML( qcStatement  * aStatement,
                                           qmsQuerySet  * aQuerySet );
private:

    //------------------------------------------
    // simple view merging�� ����
    //------------------------------------------

    // query block�� ��ȸ�ϸ� simple view merging ����
    static IDE_RC  processTransform( qcStatement  * aStatement,
                                     qcParseTree  * aParseTree,
                                     qmsQuerySet  * aQuerySet );

    // query set�� ��ȸ�ϸ� simple view merging ����
    static IDE_RC  processTransformForQuerySet( qcStatement  * aStatement,
                                                qmsQuerySet  * aQuerySet,
                                                idBool       * aIsTransformed );

    // joined table�� ���� query set�� ��ȸ�ϸ� simple view merging ����
    static IDE_RC  processTransformForJoinedTable( qcStatement  * aStatement,
                                                   qmsFrom      * aFrom );

    // Expression�� ���Ե� subquery���� ã�� simple view merging ����
    static IDE_RC  processTransformForExpression( qcStatement * aStatement,
                                                  qtcNode     * aNode );

    //------------------------------------------
    // simple view �˻�
    //------------------------------------------

    // query block�� simple query���� �˻�
    static IDE_RC  isSimpleQuery( qmsParseTree * aParseTree,
                                  idBool       * aIsSimpleQuery );

    //------------------------------------------
    // merge ���� �˻�
    //------------------------------------------

    // ���� query block�� ���� query block�� merge ���� �˻�
    static IDE_RC  canMergeView( qcStatement  * aStatement,
                                 qmsSFWGH     * aCurrentSFWGH,
                                 qmsSFWGH     * aUnderSFWGH,
                                 qmsTableRef  * aUnderTableRef,
                                 idBool       * aCanMergeView );

    // ���� query block�� merge ���� �˻�
    static IDE_RC  checkCurrentSFWGH( qmsSFWGH     * aSFWGH,
                                      idBool       * aCanMerge );
    
    // ���� query block�� merge ���� �˻�
    static IDE_RC  checkUnderSFWGH( qcStatement  * aStatement,
                                    qmsSFWGH     * aSFWGH,
                                    qmsTableRef  * aTableRef,
                                    idBool       * aCanMerge );

    // dependency �˻�
    static IDE_RC  checkDependency( qmsSFWGH     * aCurrentSFWGH,
                                    qmsSFWGH     * aUnderSFWGH,
                                    idBool       * aCanMerge );

    // normal form �˻�
    static IDE_RC  checkNormalForm( qcStatement  * aStatement,
                                    qmsSFWGH     * aCurrentSFWGH,
                                    qmsSFWGH     * aUnderSFWGH,
                                    idBool       * aCanMerge );
    
    //------------------------------------------
    // merging
    //------------------------------------------

    // ���� query block�� ���� (view) query block�� merge ����
    static IDE_RC  processMerging( qcStatement  * aStatement,
                                   qmsSFWGH     * aCurrentSFWGH,
                                   qmsSFWGH     * aUnderSFWGH,
                                   qmsFrom      * aUnderFrom,
                                   idBool       * aIsMerged );

    // hint ���� merge ����
    static IDE_RC  mergeForHint( qmsSFWGH            * aCurrentSFWGH,
                                 qmsSFWGH            * aUnderSFWGH,
                                 qmsFrom             * aUnderFrom,
                                 qmoViewRollbackInfo * aRollbackInfo,
                                 idBool              * aIsMerged );

    // hint ���� rollback ����
    static IDE_RC  rollbackForHint( qmsSFWGH            * aCurrentSFWGH,
                                    qmoViewRollbackInfo * aRollbackInfo );

    // target list�� merge ����
    static IDE_RC  mergeForTargetList( qcStatement         * aStatement,
                                       qmsSFWGH            * aUnderSFWGH,
                                       qmsTableRef         * aUnderTableRef,
                                       qmoViewRollbackInfo * aRollbackInfo,
                                       idBool              * aIsMerged );

    // target�� ���� �÷��� ���� merge ����
    static IDE_RC  mergeForTargetColumn( qcStatement         * aStatement,
                                         qmsColumnRefList    * aColumnRef,
                                         qtcNode             * aTargetColumn,
                                         idBool              * aIsMerged );

    // target�� ����� ���� merge ����
    static IDE_RC  mergeForTargetValue( qcStatement         * aStatement,
                                        qmsColumnRefList    * aColumnRef,
                                        qtcNode             * aTargetColumn,
                                        idBool              * aIsMerged );

    // target�� expression�� ���� merge ����
    static IDE_RC  mergeForTargetExpression( qcStatement         * aStatement,
                                             qmsSFWGH            * aUnderSFWGH,
                                             qmsColumnRefList    * aColumnRef,
                                             qtcNode             * aTargetColumn,
                                             idBool              * aIsMerged );

    // target list�� rollback ����
    static IDE_RC  rollbackForTargetList( qmsTableRef         * aUnderTableRef,
                                          qmoViewRollbackInfo * aRollbackInfo );

    // from ���� merget ����
    static IDE_RC  mergeForFrom( qcStatement         * aStatement,
                                 qmsSFWGH            * aCurrentSFWGH,
                                 qmsSFWGH            * aUnderSFWGH,
                                 qmsFrom             * aUnderFrom,
                                 qmoViewRollbackInfo * aRollbackInfo,
                                 idBool              * aIsMerged );

    // tuple variable�� ����
    static IDE_RC  makeTupleVariable( qcStatement    * aStatement,
                                      qcNamePosition * aViewName,
                                      qcNamePosition * aTableName,
                                      idBool           aIsNewTableName,
                                      qcNamePosition * aNewTableName,
                                      idBool         * aIsCreated );

    // from ���� rollback ����
    static IDE_RC  rollbackForFrom( qmsSFWGH            * aCurrentSFWGH,
                                    qmsSFWGH            * aUnderSFWGH,
                                    qmoViewRollbackInfo * aRollbackInfo );

    // where ���� merge ����
    static IDE_RC  mergeForWhere( qcStatement         * aStatement,
                                  qmsSFWGH            * aCurrentSFWGH,
                                  qmsSFWGH            * aUnderSFWGH,
                                  qmoViewRollbackInfo * aRollbackInfo,
                                  idBool              * aIsMerged );

    // where ���� rollback ����
    static IDE_RC  rollbackForWhere( qmsSFWGH            * aCurrentSFWGH,
                                     qmsSFWGH            * aUnderSFWGH,
                                     qmoViewRollbackInfo * aRollbackInfo );

    // merge�� view�� ����
    static IDE_RC  removeMergedView( qmsSFWGH     * aSFWGH );

    //------------------------------------------
    // validation
    //------------------------------------------

    // query set�� validation ����
    static IDE_RC  validateQuerySet( qcStatement  * aStatement,
                                     qmsQuerySet  * aQuerySet );

    // SFWGH�� validation ����
    static IDE_RC  validateSFWGH( qcStatement  * aStatement,
                                  qmsSFWGH     * aSFWGH );

    // order by ���� validation ����
    static IDE_RC  validateOrderBy( qcStatement  * aStatement,
                                    qmsParseTree * aParseTree );
    
public:
    // qtcNode�� validation ����
    static IDE_RC  validateNode( qcStatement  * aStatement,
                                 qtcNode      * aNode );
private:
    // BUG-27526
    // over ���� validation ����
    static IDE_RC  validateNode4OverClause( qcStatement  * aStatement,
                                            qtcNode      * aNode );

    // ���ŵ� view�� dependency �˻�
    static IDE_RC  checkViewDependency( qcStatement  * aStatement,
                                        qcDepInfo    * aDepInfo );
    
    // PROJ-2418
    // qmsFrom�� �ִ� Lateral View�� ���� �ٽ� Validation
    static IDE_RC  validateFrom( qmsFrom * aFrom );

    //------------------------------------------
    // ��Ÿ ��ó�� �Լ�
    //------------------------------------------

    // merge�� view�� ���� ���� view reference ����
    static IDE_RC  modifySameViewRef( qcStatement  * aStatement );
};

#endif /* _O_QMO_VIEW_MERGING_H_ */
