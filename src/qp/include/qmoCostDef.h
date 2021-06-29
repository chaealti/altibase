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
 * $Id: qmoCostDef.h 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *
 *    Cost Factor ����� ����
 *
 *    ��� ��� �� ����Ǵ� ������� ���⿡ ������.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_COST_DEF_H_
#define _O_QMO_COST_DEF_H_ 1

// Join�� DNF �� ó���ϴ� ���� �ٶ������� �ʴ�.
// �̿� ���� Penalty�� �ο��Ѵ�.
#define QMO_COST_DNF_JOIN_PENALTY                                  (10)

//----------------------------------------------
// ������ �� ���� ����� ���
//----------------------------------------------
#define QMO_COST_INVALID_COST                                      (-1)

// �ּ� ���� ����
#define QMO_COST_DISK_MINIMUM_BUFFER                               (10)

//----------------------------------------------------------------------
// �߰� ��� ���� ���
//----------------------------------------------------------------------

// RID ���� ��İ� Push Projection�� ���� Value ���� ������� ������.
typedef enum
{
    QMO_STORE_RID = 0,
    QMO_STORE_VAL,
    QMO_STORE_MAXMAX
} qmoStoreMethod;

//----------------------------------------------
// temp table type
//----------------------------------------------

#define QMO_COST_STORE_SORT_TEMP                                    (0)
#define QMO_COST_STORE_HASH_TEMP                                    (1)

#define QMO_COST_DEFAULT_NODE_CNT                                   (1)

#define QMO_COST_FIRST_ROWS_FACTOR_DEFAULT                        (1.0)

#define QMO_COST_EPS              (1e-8)
#define QMO_COST_IS_GREATER(x, y) (((x) > ((y) + QMO_COST_EPS)) ? ID_TRUE : ID_FALSE)
#define QMO_COST_IS_LESS(x, y)    (((x) < ((y) - QMO_COST_EPS)) ? ID_TRUE : ID_FALSE)
#define QMO_COST_IS_EQUAL(x, y)   ((((x)-(y) > -QMO_COST_EPS) && ((x)-(y) < QMO_COST_EPS)) ? ID_TRUE : ID_FALSE)

#endif /* _O_QMO_COST_DEF_H_ */
