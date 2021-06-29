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
 * $Id: qmgSet.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     SET Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_SET_H_
#define _O_QMG_SET_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmsParseTree.h>

//---------------------------------------------------
// Set Graph�� Define ���
//---------------------------------------------------

//---------------------------------------------------
// ���� Graph�� qmgSet���� �ƴ����� ���� ����
// Plan Node ���� �� ���
//    - ���� Graph�� qmgSet�̸� PROJ Node�� ����
//    - ���� Graph�� qmgSet�� �ƴϸ� PROJ Node�� �������� ����
//---------------------------------------------------

// qmgSET.graph.flag
#define QMG_SET_PARENT_TYPE_SET_MASK    (0x10000000)
#define QMG_SET_PARENT_TYPE_SET_FALSE   (0x00000000)
#define QMG_SET_PARENT_TYPE_SET_TRUE    (0x10000000)

//---------------------------------------------------
// SET Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgSET
{
    qmgGraph      graph;          // ���� Graph ����

    qmsSetOpType  setOp;
    UInt          hashBucketCnt;    
    
} qmgSET;

//---------------------------------------------------
// SET Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgSet
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmgGraph    * aLeftGraph,
                         qmgGraph    * aRightGraph, 
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    static IDE_RC  makeUnion( qcStatement * aStatement,
                              qmgSET      * aMyGraph,
                              idBool        aIsUnionAll );

    static IDE_RC  makeIntersect( qcStatement * aStatement,
                                  qmgSET      * aMyGraph );

    static IDE_RC  makeMinus( qcStatement * aStatement,
                              qmgSET      * aMyGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

private:
    static IDE_RC  makeChild( qcStatement * aStatement,
                              qmgSET      * aMyGraph );

    //--------------------------------------------------
    // ����ȭ�� ���� �Լ�
    //--------------------------------------------------

    // PROJ-1486 Multiple Bag Union

    // Multiple Bag Union ����ȭ ����
    static IDE_RC optMultiBagUnion( qcStatement * aStatement,
                                    qmgSET      * aSETGraph );

    // Multiple Children �� ����
    static IDE_RC linkChildGraph( qcStatement  * aStatement,
                                  qmgGraph     * aChildGraph,
                                  qmgChildren ** aChildren );
};

#endif /* _O_QMG_SET_H_ */

