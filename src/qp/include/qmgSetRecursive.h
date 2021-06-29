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
 * $Id: qmgSetRecursive.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     UNION ALL RECRUSVIE Graph�� ���� ����
 *
 **********************************************************************/

#ifndef _O_QMG_SET_RECURSIVE_H_
#define _O_QMG_SET_RECURSIVE_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmsParseTree.h>

//---------------------------------------------------
// Set Graph�� Define ���
//---------------------------------------------------

//---------------------------------------------------
// RECURSIVE VIEW Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgRUNION
{
    qmgGraph       graph;       // ���� Graph ����

    qmgGraph     * recursiveViewGraph;   // right query�� ���� recursive view graph
    
} qmgRUNION;

//---------------------------------------------------
// RECURSIVE VIEW �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgSetRecursive
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aLeftGraph,
                         qmgGraph    * aRightGraph,
                         qmgGraph    * aRecursiveViewGraph,
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement,
                             qmgGraph    * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement    * aStatement,
                             const qmgGraph * aParent,
                             qmgGraph       * aGraph );

    static IDE_RC  makeUnionAllRecursive( qcStatement * aStatement,
                                          qmgRUNION   * aMyGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );
    
private:
    static IDE_RC  makeChild( qcStatement * aStatement,
                              qmgRUNION   * aMyGraph );
};

#endif /* _O_QMG_SET_RECURSIVE_H_ */

