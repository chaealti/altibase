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
 * $Id: qmoAnsiJoinOrder.h 54400 2012-07-18 03:48:06Z junokun $
 *
 * Description :
 *     ANSI Join Ordering
 *
 *     ANSI style �� �ۼ��� join ���� inner join �� ������ ������ �� �ֵ���
 *     ó���Ѵ�.
 *
 *     ���� cost �� ����Ͽ� inner/outer join �� ������ ������ �� �ֵ���
 *     �����ؾ� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_ANSI_JOIN_ORDER_H_
#define _O_QMO_ANSI_JOIN_ORDER_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoPredicate.h>

//---------------------------------------------------
// Ansi join ���� ������ ���� �Լ����� ��Ƴ��� class
//---------------------------------------------------
class qmoAnsiJoinOrder
{
public:

    static IDE_RC clonePredicate2List( qcStatement   * aStatement,
                                       qmoPredicate  * aPred1,
                                       qmoPredicate ** aPredList );

    static IDE_RC traverseFroms( qcStatement * aStatement,
                                 qmsFrom     * aFrom,
                                 qmsFrom    ** aFromTree,
                                 qmsFrom    ** aFromArr,
                                 qcDepInfo   * aFromArrDep,
                                 idBool      * aMakeFail );

    static IDE_RC mergeOuterJoinGraph2myGraph( qmoCNF * aCNF );

    static IDE_RC fixOuterJoinGraphPredicate( qcStatement * aStatement,
                                              qmoCNF      * aCNF );

private:

    static IDE_RC appendFroms( qcStatement  * aStatement,
                               qmsFrom     ** aFromArr,
                               qmsFrom      * aFrom );

    static IDE_RC cloneFrom( qcStatement * aStatement,
                             qmsFrom     * aFrom1,
                             qmsFrom    ** aFrom2 );
};

#endif /* _O_QMO_CRT_CNF_MGR_H_ */
