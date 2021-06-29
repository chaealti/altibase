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
 * $Id: qmgInsert.h 53265 2012-05-18 00:05:06Z seo0jun $
 *
 * Description :
 *     Insert Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_MULTI_INSERT_H_
#define _O_QMG_MULTI_INSERT_H_ 1

#include <qc.h>
#include <qmgDef.h>

//---------------------------------------------------
// Insert Graph�� Define ���
//---------------------------------------------------


//---------------------------------------------------
// Multi-Insert Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgMTIT
{
    qmgGraph             graph;    // ���� Graph ����

    //---------------------------------
    // insert ���� ����
    //---------------------------------
    
} qmgMTIT;

//---------------------------------------------------
// Mutlti-Insert Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgMultiInsert
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmgGraph    * aChildGraph,
                         qmgGraph   ** aGraph );

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

#endif /* _O_QMG_MULTI_INSERT_H_ */

