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
 * $Id: qmoCrtPathMgr.h 86122 2019-09-04 07:20:21Z donovan.seo $
 *
 * Description :
 *     Critical Path Manager
 *
 *     FROM, WHERE�� �����Ǵ� Critical Path�� ���� ����ȭ�� �����ϰ�
 *     �̿� ���� Graph�� �����Ѵ�.
 *
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_CRT_PATH_MGR_H_
#define _O_QMO_CRT_PATH_MGR_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoCnfMgr.h>
#include <qmoDnfMgr.h>

//---------------------------------------------------
// Critical Path�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoCrtPath
{
    qmoNormalType   normalType;    // CNF �Ǵ� DNF

    qmoDNF        * crtDNF;
    qmoCNF        * crtCNF;

    qmoCNF        * currentCNF;    // ���� CNF( Plan Tree ���� �� ��� )

    qmgGraph      * myGraph;       // critical path�� ������ ��� graph

    //-------------------------------------------------------------------
    // PROJ-1405 ROWNUM ���� �ڷ� ����
    //
    // currentNormalType   : ���� critical path�� predicate�� ó���ϴ�
    //                       Normalization Type ����
    // rownumPredicate4CNF : critical path�� CNF,NNF�� predicate�� ó���Ҷ���
    //                       rownumPredicate ����
    // rownumPredicate4CNF : critical path�� DNF�� predicate�� ó���Ҷ���
    //                       rownumPredicate ����
    // rownumPredicate     : critical path�� ������ ����ȭ���·�
    //                       rownumPredicate ����
    // myQuerySet          : ROWNUM ��� ����
    //-------------------------------------------------------------------

    qmoNormalType   currentNormalType;
    
    qmoPredicate  * rownumPredicateForCNF;
    qmoPredicate  * rownumPredicateForDNF;
    qmoPredicate  * rownumPredicate;
    
    qmsQuerySet   * myQuerySet;
    idBool          mIsOnlyNL;
} qmoCrtPath;

//---------------------------------------------------
// Critical Path�� ���� �Լ�
//---------------------------------------------------

class qmoCrtPathMgr
{
public:

    // Critical Path ���� �� �ʱ�ȭ
    static IDE_RC    init( qcStatement * aStatement,
                           qmsQuerySet * aQuerySet,
                           qmoCrtPath ** aCrtPath );

    // Critical Path�� ���� ����ȭ �� Graph ����
    static IDE_RC    optimize( qcStatement * aStatement,
                               qmoCrtPath  * aCrtPath );

    // Normalization Type ����
    static IDE_RC    decideNormalType( qcStatement   * aStatement,
                                       qmsFrom       * aFrom,
                                       qtcNode       * aWhere,
                                       qmsHints      * aHint,
                                       idBool          aCNFOnly,
                                       qmoNormalType * aNormalType);

    // PROJ-1405
    // Rownum Predicate�� rownumPredicate�� ����
    static IDE_RC    addRownumPredicate( qmsQuerySet  * aQuerySet,
                                         qmoPredicate * aPredicate );
    
    // PROJ-1405
    // Rownum Predicate�� rownumPredicate�� ����
    static IDE_RC    addRownumPredicateForNode( qcStatement  * aStatement,
                                                qmsQuerySet  * aQuerySet,
                                                qtcNode      * aNode,
                                                idBool         aNeedCopy );

    // BUG-35155 Partial CNF
    static IDE_RC decideNormalType4Where( qcStatement   * aStatement,
                                           qmsFrom       * aFrom,
                                           qtcNode       * aWhere,
                                           qmsHints      * aHint,
                                           idBool          aIsCNFOnly,
                                           qmoNormalType * aNormalType);

private:

    // from���� view�� �ִ��� �˻�
    static IDE_RC    existsViewinFrom( qmsFrom * aFrom,
                                       idBool  * aIsExistView );
};


#endif /* _O_QMO_CRT_PATH_MGR_H_ */
