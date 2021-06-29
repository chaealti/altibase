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
 * $Id: qmo.h 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Query Optimizer
 *
 *     Optimizer�� �����ϴ� �ֻ��� Interface�� ������
 *     Graph ���� �� Plan Tree�� �����Ѵ�.
 *
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_H_
#define _O_QMO_H_ 1

#include <qc.h>
#include <qmoDef.h>
#include <qmsParseTree.h>
#include <qmc.h>
#include <qmmParseTree.h>

//---------------------------------------------------
// Optimizer ���� �Լ�
//---------------------------------------------------

class qmo
{
public:

    // SELECT ������ ���� ����ȭ
    static IDE_RC    optimizeSelect( qcStatement * aStatement );

    // INSERT ������ ���� ����ȭ
    static IDE_RC    optimizeInsert( qcStatement * aStatement );
    static IDE_RC    optimizeMultiInsertSelect( qcStatement * aStatement );
    
    // UPDATE ������ ���� ����ȭ
    static IDE_RC    optimizeUpdate( qcStatement * aStatement );

    // DELETE ������ ���� ����ȭ
    static IDE_RC    optimizeDelete( qcStatement * aStatement );

    // MOVE ������ ���� ����ȭ
    static IDE_RC    optimizeMove( qcStatement * aStatement );

    /* PROJ-1988 Implement MERGE statement */
    static IDE_RC	 optimizeMerge( qcStatement * aStatement );

    // Shard DML
    static IDE_RC    optimizeShardDML( qcStatement * aStatement );

    // Shard Insert
    static IDE_RC    optimizeShardInsert( qcStatement * aStatement );

    // Sub SELECT ������ ���� ����ȭ ( CREATE AS SELECT )
    static IDE_RC    optimizeCreateSelect( qcStatement * aStatement );


    // Graph�� ���� �� �ʱ�ȭ
    // Optimizer �ܺο��� ȣ���ϸ� �ȵ�.
    // optimizeSelect�� qmoSubquery::makeGraph�� ȣ��
    static IDE_RC    makeGraph( qcStatement * aStatement );

    // PROJ-2205 Rownum in DML
    static IDE_RC    makeInsertGraph( qcStatement * aStatement );
    static IDE_RC    makeMultiInsertGraph( qcStatement * aStatement );

    // PROJ-2205 Rownum in DML
    static IDE_RC    makeUpdateGraph( qcStatement * aStatement );

    // PROJ-2205 Rownum in DML
    static IDE_RC    makeDeleteGraph( qcStatement * aStatement );

    // PROJ-2205 Rownum in DML
    static IDE_RC    makeMoveGraph( qcStatement * aStatement );

    // PROJ-2205 Rownum in DML
    static IDE_RC    makeMergeGraph( qcStatement * aStatement );

    // Shard Insert
    static IDE_RC    makeShardInsertGraph( qcStatement * aStatement );

    // PROJ-1413
    // Query Transformation�� ����
    // ParseTree�� Transformed ParseTree�� �����Ѵ�.
    static IDE_RC    doTransform( qcStatement * aStatement );
 
    static IDE_RC    doTransformSubqueries( qcStatement * aStatement,
                                            qtcNode     * aPredicate );

    // ���� Join Graph�� �ش��ϴ� dependencies���� �˻�
    static IDE_RC    currentJoinDependencies( qmgGraph  * aJoinGraph,
                                              qcDepInfo * aDependencies,
                                              idBool    * aIsCurrent );

    static IDE_RC    currentJoinDependencies4JoinOrder( qmgGraph  * aJoinGraph,
                                                        qcDepInfo * aDependencies,
                                                        idBool    * aIsCurrent );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // bind �� �Ʒ� �Լ��� ���ؼ� scan method�� �缳���Ѵ�.
    static IDE_RC    optimizeForHost( qcStatement * aStatement );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // �� �Լ��� ���ؼ� �缳���� access method�� ���Ѵ�.
    // execution�ÿ� dataPlan�� ���� ȣ��ȴ�.
    static qmncScanMethod * getSelectedMethod(
        qcTemplate            * aTemplate,
        qmoScanDecisionFactor * aSDF,
        qmncScanMethod        * aDefaultMethod );

    // PROJ-2462 Result Cache Hint�� Property �� ���ؼ� Result Cache���θ�
    // ǥ���Ѵ�.
    static IDE_RC setResultCacheFlag( qcStatement * aStatement );

    // PROJ-2462 Result Cache
    static IDE_RC initResultCacheStack( qcStatement   * aStatement,
                                        qmsQuerySet   * aQuerySet,
                                        UInt            aPlanID,
                                        idBool          aIsTopResult,
                                        idBool          aIsVMTR );

    // PROJ-2462 Result Cache
    static void makeResultCacheStack( qcStatement      * aStatement,
                                      qmsQuerySet      * aQuerySet,
                                      UInt               aPlanID,
                                      UInt               aPlanFlag,
                                      ULong              aTupleFlag,
                                      qmcMtrNode       * aMtrNode,
                                      qcComponentInfo ** aInfo,
                                      idBool             aIsTopResult );

    // PROJ-2462 Result Cache
    static void flushResultCacheStack( qcStatement * aStatement );

    static void addTupleID2ResultCacheStack( qcStatement * aStatement,
                                             UShort        aTupleID );

    /* PROJ-2714 Multiple Update Delete support */
    static IDE_RC optimizeMultiUpdate( qcStatement * aStatement );
    static IDE_RC optimizeMultiDelete( qcStatement * aStatement );

    /* TASK-7219 Non-shard DML */
    static IDE_RC removeOutRefPredPushedForce( qmoPredicate ** aPredicate );

private:
    // PROJ-2462 Result Cache
    static IDE_RC pushComponentInfo( qcTemplate    * aTemplate,
                                     iduVarMemList * aMemory,
                                     UInt            aPlanID ,
                                     idBool          aIsVMTR );

    // PROJ-2462 Result Cache
    static void popComponentInfo( qcTemplate       * aTemplate,
                                  idBool             aIsPossible,
                                  qcComponentInfo ** aInfo );

    static IDE_RC makeTopResultCacheGraph( qcStatement * aStatement );
    static void checkQuerySet( qmsQuerySet * aQuerySet, idBool * aIsPossible );
    static void checkFromTree( qmsFrom * aFrom, idBool * aIsPossible );

    /* BUG-44228  merge ����� disk table�̰� hash join �� ��� �ǵ����� �ʴ� ������ ���� �˴ϴ�. */
    static IDE_RC adjustValueNodeForMerge( qcStatement  * aStatement,
                                           qmcAttrDesc  * sResultDesc,
                                           qmmValueNode * sValueNode );

    /* BUG-44228  merge ����� disk table�̰� hash join �� ��� �ǵ����� �ʴ� ������ ���� �˴ϴ�. */
    static IDE_RC adjustArgumentNodeForMerge( qcStatement  * aStatement,
                                              mtcNode      * sSrcNode,
                                              mtcNode      * sDstNode );
};

#endif /* _O_QMO_H_ */
