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
 * $Id$
 *
 * Description :
 *     Anti join graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_ANTI_JOIN_H_
#define _O_QMG_ANTI_JOIN_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoJoinMethod.h>
#include <qmoPredicate.h>

enum qmgAntiJoinMethod
{
    QMG_ANTI_JOIN_METHOD_NESTED = 0,
    QMG_ANTI_JOIN_METHOD_HASH,
    QMG_ANTI_JOIN_METHOD_SORT,
    QMG_ANTI_JOIN_METHOD_MERGE,
    QMG_ANTI_JOIN_METHOD_COUNT
};

//---------------------------------------------------
// Anti join graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgAntiJoin
{
public:
    // Join Relation�� �����ϴ� qmgJoin Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmgGraph    * aLeftGraph,
                         qmgGraph    * aRightGraph,
                         qmgGraph    * aGraph);

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

};

#endif /* _O_QMG_ANTI_JOIN_H_ */

