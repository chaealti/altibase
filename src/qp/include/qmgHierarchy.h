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
 * $Id: qmgHierarchy.h 82330 2018-02-23 00:32:59Z donovan.seo $
 *
 * Description :
 *     Hierarchy Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_HIERARCHY_H_
#define _O_QMG_HIERARCHY_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmsParseTree.h>
#include <qmoCnfMgr.h>

//---------------------------------------------------
// Hierarchy Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgHIER
{
    qmgGraph graph;  // ���� Graph ����
    
    // Hierarchy Graph�� ���� ����
    qmsHierarchy     * myHierarchy;    // qmsSFWGH::hierarchy�� ����Ŵ

    // startWith ����
    qmoCNF           * startWithCNF;
    UInt               mStartWithMethodCnt;
    qmoAccessMethod  * mStartWithAccessMethod;
    qmoAccessMethod  * mSelectedStartWithMethod;
    qcmIndex         * mSelectedStartWithIdx;

    // connectBy ����
    qmoCNF           * connectByCNF;
    UInt               mConnectByMethodCnt;
    qmoAccessMethod  * mConnectByAccessMethod;
    qmoAccessMethod  * mSelectedConnectByMethod;
    qcmIndex         * mSelectedConnectByIdx;
} qmgHIER;

//---------------------------------------------------
// Hierarchy Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgHierarchy
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         qmgGraph     * aChildGraph,
                         qmsFrom      * aFrom,
                         qmsHierarchy * aHierarchy,
                         qmgGraph    ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

private:
    static IDE_RC optimizeStartWith( qcStatement * aStatement,
                                     qmgHIER     * aMyGraph,
                                     qmsTableRef * aTableRef,
                                     UInt          aIndexCnt );

    static IDE_RC searchTableID( qtcNode * aNode,
                                 UShort  * aTableID,
                                 idBool  * aFind );

    static IDE_RC searchFrom( UShort     aTableID,
                              qmsFrom  * aFrom,
                              qmsFrom ** aFindFrom,
                              idBool   * aFind );
};

#endif /* _O_QMG_HIERARCHY_H_ */

