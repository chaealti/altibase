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
 * $Id: qmgMerge.h 53265 2012-05-18 00:05:06Z seo0jun $
 *
 * Description :
 *     Merge Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_MERGE_H_
#define _O_QMG_MERGE_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Merge Graph�� Define ���
//---------------------------------------------------


//---------------------------------------------------
// Merge Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgMRGE
{
    qmgGraph             graph;    // ���� Graph ����

    //---------------------------------
    // merge ���� ����
    //---------------------------------

    // child statements
    qcStatement        * selectSourceStatement;
    qcStatement        * selectTargetStatement;
    
    qcStatement        * updateStatement;
    qcStatement        * deleteStatement;
    qcStatement        * insertStatement;
    qcStatement        * insertNoRowsStatement;  // BUG-37535

    // insert where clause
    qtcNode            * whereForInsert;

} qmgMRGE;

//---------------------------------------------------
// Merge Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgMerge
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement  * aStatement,
                         qmgGraph    ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement    * aStatement,
                             const qmgGraph * aParent,
                             qmgGraph       * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );
};

#endif /* _O_QMG_MERGE_H_ */

