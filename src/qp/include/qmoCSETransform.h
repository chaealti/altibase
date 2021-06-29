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
 * $Id$
 *
 * Description : PROJ-2242 Common Subexpression Elimination Transformation
 *
 *       - QTC_NODE_JOIN_OPERATOR_EXIST �� ��� ���� ����
 *       - subquery, host variable, GEOMETRY type arguments ����
 *       - __OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION property �� ����
 *
 * ��� ���� :
 *
 *            1. Idempotent law
 *             - A and A = A
 *             - A or A = A
 *            2. Absorption law
 *             - A and (A or B) = A
 *             - A or (A and B) = A
 *
 * ��� ���� :
 *
 * ��� : CSE (Common Subexpression Elimination)
 *        NNF (Not Normal Form)
 *
 *****************************************************************************/

#ifndef _Q_QMO_CSE_TRANSFORM_H_
#define _Q_QMO_CSE_TRANSFORM_H_ 1

#include <ide.h>
#include <qtcDef.h>
#include <qmsParseTree.h>

typedef enum qmoCSECompareResult
{
    QMO_CSE_COMPARE_RESULT_BOTH = 1,
    QMO_CSE_COMPARE_RESULT_WIN,
    QMO_CSE_COMPARE_RESULT_LOSE
} qmoCSECompareResult;

//-----------------------------------------------------------
// CSE Transform ���� �Լ�
//-----------------------------------------------------------

class qmoCSETransform
{
public:

    // ���� NNF ������ ��� �������� ���� CSE transformation
    // - Where clause
    // - On condition
    // - Having clause
    static IDE_RC doTransform4NNF( qcStatement * aStatement,
                                   qmsQuerySet * aQuerySet,
                                   idBool        aIsNNF );

    // CSE transformation
    static IDE_RC doTransform( qcStatement * aStatement,
                               qtcNode    ** aNode,
                               idBool        aIsNNF,
                               idBool        aIsWhere = ID_FALSE,
                               qmsHints    * aHints = NULL );

    // �������� oracle style outer mask ���翩�� �˻�
    static IDE_RC doCheckOuter( qtcNode  * aNode,
                                idBool   * aExistOuter );

private:

    // From ���� onCondition �� ���� CSE transformation (���)
    static IDE_RC doTransform4From( qcStatement * aStatement,
                                    qmsFrom     * aFrom,
                                    idBool        aIsNNF );

    // NNF �� ��ø�� logical operator ����
    static IDE_RC unnestingAndOr4NNF( qcStatement * aStatement,
                                      qtcNode     * aNode );

    // Idempotent law �� Absorption law ���� (NNF, CNF, DNF)
    static IDE_RC idempotentAndAbsorption( qcStatement * aStatement,
                                           qtcNode     * aNode );

    // doIdempotentAndAbsorption ������ ���� ��ø��� ��
    static IDE_RC compareNode( qcStatement         * aStatement,
                               qtcNode             * aTarget,
                               qtcNode             * aCompare,
                               qmoCSECompareResult * aResult );

    // NNF �� ���� �ϳ��� ���ڸ� ���� logical operator ����
    static IDE_RC removeLogicalNode4NNF( qcStatement * aStatement,
                                         qtcNode    ** aNode );

};

#endif  /* _Q_QMO_CSE_TRANSFORM_H_ */
