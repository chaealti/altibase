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
 * $Id: qmnLeftOuter.h 90785 2021-05-06 07:26:22Z hykim $
 *
 * Description :
 *     LOJN(Left Outer JoiN) Node
 *
 *     ������ �𵨿��� Left Outer Join�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *         - Full Outer Join�� Anti-Outer ����ȭ �����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_LEFT_OUTER_H_
#define _O_LEFT_OUTER_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------
// qmncLOJN.flag
// PROJ-2750
#define QMNC_LOJN_SKIP_RIGHT_COND_MASK     (0x00000001)
#define QMNC_LOJN_SKIP_RIGHT_COND_FALSE    (0x00000000)
#define QMNC_LOJN_SKIP_RIGHT_COND_TRUE     (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndLOJN.flag
// First Initialization Done
#define QMND_LOJN_INIT_DONE_MASK           (0x00000001)
#define QMND_LOJN_INIT_DONE_FALSE          (0x00000000)
#define QMND_LOJN_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncLOJN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // LOJN ���� ����
    //---------------------------------

    qtcNode      * filter;    // ON Condition���� Filter ����
    
} qmncLOJN;

typedef struct qmndLOJN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;

    //---------------------------------
    // LOJN ���� ����
    //---------------------------------
    
    setHitFlagFunc      setHitFlag;  // Child(HASH)�� ���� �Լ�
    UInt                mSkipRightCnt;   // PROJ-2750

} qmndLOJN;

class qmnLOJN
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

    // ���ο� Left Row�� ���� ó��
    static IDE_RC doItLeft( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // ���ο� Right Row�� ���� ó��
    static IDE_RC doItRight( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // Inverse Hash 
    static IDE_RC doItInverseLeft( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   qmcRowFlag * aFlag );

    static IDE_RC doItInverseRight( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag );

    static IDE_RC doItInverseNonHitFirst( qcTemplate * aTemplate,
                                          qmnPlan    * aPlan,
                                          qmcRowFlag * aFlag );

    static IDE_RC doItInverseNonHitNext( qcTemplate * aTemplate,
                                         qmnPlan    * aPlan,
                                         qmcRowFlag * aFlag );

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmncLOJN   * aCodePlan,
                             qmndLOJN   * aDataPlan );

    //------------------------
    // ��� �Լ�
    //------------------------

    // PROJ-2750
    static void printSkipRightCnt( qcTemplate   * aTemplate,
                                   iduVarString * aString,
                                   UInt           aFlag,
                                   UInt           aSkipRightCnt,
                                   qmnDisplay     aMode );

};

#endif /* _O_LEFT_OUTER_H_ */
