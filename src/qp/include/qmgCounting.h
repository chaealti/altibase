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
 * $Id: qmgCounting.h 20233 2007-02-01 01:58:21Z shsuh $
 *
 * Description :
 *     Counting Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_COUNTING_H_
#define _O_QMG_COUNTING_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoPredicate.h>

//---------------------------------------------------
// Counting Graph�� Define ���
//---------------------------------------------------

//---------------------------------------------------
// Counting Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgCNTG
{
    qmgGraph         graph;      // ���� Graph ����

    qmoPredicate   * stopkeyPredicate;
    qmoPredicate   * filterPredicate;

    SLong            stopRecordCount;   // stopkey�� ���õǴ� ���ڵ� ��
    
} qmgCNTG;

//---------------------------------------------------
// Counting Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgCounting
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         qmoPredicate * aRownumPredicate,
                         qmgGraph     * aChildGraph,
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

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

private:
    
    // Left Child�� Graph�κ��� Preserved Order�� �����Ѵ�.
    static IDE_RC makeOrderFromChild( qcStatement * aStatement,
                                      qmgGraph    * aGraph );
};

#endif /* _O_QMG_COUNTING_H_ */

