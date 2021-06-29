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
 * $Id: qmoDnfMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     DNF Critical Path Manager
 *
 *     DNF Normalized Form�� ���� ����ȭ�� �����ϰ�
 *     �ش� Graph�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_DNF_MGR_H_
#define _O_QMO_DNF_MGR_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoCnfMgr.h>
#include <qmgDnf.h>

//---------------------------------------------------
// DNF Critical Path�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoDNF
{   
    qtcNode       * normalDNF;    // where���� DNF�� normalize�� ���
    qmgGraph      * myGraph;      // DNF Form ��� graph�� top
    qmsQuerySet   * myQuerySet;
    SDouble         cost;        // DNF Total Cost

    //------------------------------------------------------
    // CNF Graph ���� �ڷ� ����
    //
    //   - cnfCnt : CNF�� ���� ( = normalDNF�� AND�� ���� )
    //   - myCNF  : CNF �迭
    //------------------------------------------------------
    
    UInt            cnfCnt;      // CNF �� ����

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    UInt            madeCnfCnt;  // optmization�� ������ cnf�� ����
                                 // removeOptimizationInfo���� ����

    qmoCNF        * myCNF;       // CNF���� �迭

    //------------------------------------------------------
    // DNF Graph ���� �ڷ� ����
    //
    //   - dnfGraphCnt : �����ؾ��� DNF Graph ���� ( = CnfCount - 1 )
    //   - dnfGraph    : Dnf Graph �迭
    //------------------------------------------------------
    UInt            dnfGraphCnt;
    qmgDNF        * dnfGraph;

    //------------------------------------------------
    // Not Normal Form : (~(���� DNF�� Predicate))���� List
    //                   �ߺ� data�� ������ �ϱ� ����
    //    - notNormalForm�� ���� : dnfGraphCnt
    //------------------------------------------------
    
    qtcNode      ** notNormal;
    
} qmoDNF;

//---------------------------------------------------
// DNF Critical Path�� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmoDnfMgr 
{
public:

    // DNF Critical Path ���� �� �ʱ�ȭ
    static IDE_RC    init( qcStatement * aStatement,
                           qmoDNF      * aDNF,
                           qmsQuerySet * aQuerySet,
                           qtcNode     * aNormalDNF);
    
    // DNF Critical Path�� ���� ����ȭ �� Graph ����
    static IDE_RC    optimize( qcStatement * aStatement,
                               qmoDNF      * aDNF,
                               SDouble       aCnfCost );
   
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // optimization�� ���� ������ ���� �ʿ䰡 ���� ��
    // �� �Լ��� �߰��ϸ� �ȴ�. 
    static IDE_RC    removeOptimizationInfo( qcStatement * aStatement,
                                             qmoDNF      * aDNF );

private:
    // Dnf Not Normal Form���� �迭�� ����� �Լ�
    static IDE_RC    makeNotNormal( qcStatement * aStatement,
                                    qmoDNF      * aDNF );
    
    // Dnf Not Normal Form ����� �Լ�
    static IDE_RC    makeDnfNotNormal( qcStatement * aStatement,
                                       qtcNode     * aNormalForm,
                                       qtcNode    ** aDnfNotNormal );

    // PROJ-1405 DNF normal form���� rownum predicate�� �����Ѵ�.
    static IDE_RC    removeRownumPredicate( qcStatement * aStatement,
                                            qmoDNF      * aDNF );
};


#endif /* _O_QMO_DNF_MGR_H_ */

