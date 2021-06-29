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
 
/******************************************************************************
 * $Id: qmoListTransform.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description : BUG-36438 List Transformation
 *
 * ��� ���� :
 *
 *****************************************************************************/
#ifndef _Q_QMO_LIST_TRANSFORM_H_
#define _Q_QMO_LIST_TRANSFORM_H_ 1

#include <ide.h>
#include <qtcDef.h>
#include <qmsParseTree.h>

#define QMO_LIST_TRANSFORM_ARGUMENTS_COUNT  2
#define QMO_LIST_TRANSFORM_DEPENDENCY_COUNT 2

//-----------------------------------------------------------
// LIST Transform ���� �Լ�
//-----------------------------------------------------------
class qmoListTransform
{
public:

    // ���� NNF ������ ��� �������� ���� LIST transformation
    // - Where clause
    // - On condition
    // - Having clause
    static IDE_RC doTransform( qcStatement * aStatement,
                               qmsQuerySet * aQuerySet );

private:

    // From ���� onCondition �� ���� LIST transformation (���)
    static IDE_RC doTransform4From( qcStatement * aStatement,
                                    qmsFrom     * aFrom );

    // LIST transformation
    static IDE_RC listTransform( qcStatement * aStatement,
                                 qtcNode    ** aNode );

    // Predicate list ����
    static IDE_RC makePredicateList( qcStatement  * aStatement,
                                     qtcNode      * aCompareNode,
                                     qtcNode     ** aResult );

    // ��ȯ ���� �˻�
    static IDE_RC checkCondition( qtcNode     * aNode,
                                  idBool      * aResult );

    // Predicate ����
    static IDE_RC makePredicate( qcStatement  * aStatement,
                                 qtcNode      * aPredicate,
                                 qtcNode      * aOperand1,
                                 qtcNode      * aOperand2,
                                 qtcNode     ** aResult );
};

#endif  /* _Q_QMO_LIST_TRANSFORM_H_ */
