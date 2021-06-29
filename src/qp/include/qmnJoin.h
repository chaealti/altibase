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
 * $Id: qmnJoin.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     JOIN(JOIN) Node
 *
 *     ������ �𵨿��� cartesian product�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Cartesian Product
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_JOIN_H_
#define _O_QMN_JOIN_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmncJOIN.flag
// PROJ-1718 Subquery unnesting
// Join�� ����
#define QMNC_JOIN_TYPE_MASK                (0x00000003)
#define QMNC_JOIN_TYPE_INNER               (0x00000000)
#define QMNC_JOIN_TYPE_SEMI                (0x00000001)
#define QMNC_JOIN_TYPE_ANTI                (0x00000002)
#define QMNC_JOIN_TYPE_UNDEFINED           (0x00000003)

// qmndJOIN.flag
// First Initialization Done
#define QMND_JOIN_INIT_DONE_MASK           (0x00000001)
#define QMND_JOIN_INIT_DONE_FALSE          (0x00000000)
#define QMND_JOIN_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncJOIN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;
    qtcNode      * filter;

    // PROJ-2551 simple query ����ȭ
    idBool         isSimple;
    
} qmncJOIN;

typedef struct qmndJOIN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // JOIN ���� ����
    //---------------------------------

    setHitFlagFunc      setHitFlag;    // Child(HASH)�� ���� �Լ�
    isHitFlaggedFunc    isHitFlagged;  // Child(HASH)�� ���� �Լ�

} qmndJOIN;

class qmnJOIN
{
public:

    //------------------------
    // Base Function Pointer
    //------------------------

    // �ʱ�ȭ
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ���� �Լ�
    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    // Null Padding
    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );

    // Plan ���� ���
    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

private:

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // ȣ��Ǹ� �ȵ�
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // Inner join, Semi join (Inverse Index NL)
    // Left���� �Ѱ�, Right���� �Ѱ� ����
    static IDE_RC doItLeft( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Inner join, Semi join (Inverse Index NL)
    // ������ Left�� �ΰ� Right���� �Ѱ� ����
    static IDE_RC doItRight( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // Semi join
    static IDE_RC doItSemi( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Anti join
    static IDE_RC doItAnti( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Semi join (Inverse Sort), Anti join (Inverse Hash/Sort)
    static IDE_RC doItInverse( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // Semi join (Inverse Sort), Anti join (Inverse Hash/Sort)
    static IDE_RC doItInverseHitFirst( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    // Semi join (Inverse Sort), Anti join (Inverse Hash/Sort)
    static IDE_RC doItInverseHitNext( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan,
                                      qmcRowFlag * aFlag );

    // Semi join (Inverse Sort), Anti join (Inverse Hash/Sort)
    static IDE_RC doItInverseNonHitFirst( qcTemplate * aTemplate,
                                          qmnPlan    * aPlan,
                                          qmcRowFlag * aFlag );

    // Semi join (Inverse Sort), Anti join (Inverse Hash/Sort)
    static IDE_RC doItInverseNonHitNext( qcTemplate * aTemplate,
                                         qmnPlan    * aPlan,
                                         qmcRowFlag * aFlag );

    static IDE_RC checkFilter( qcTemplate * aTemplate,
                               qmncJOIN   * aCodePlan,
                               UInt         aRightFlag,
                               idBool     * aRetry );

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmncJOIN   * aCodePlan,
                             qmndJOIN   * aDataPlan );

};

#endif /* _O_QMN_JOIN_H_ */
