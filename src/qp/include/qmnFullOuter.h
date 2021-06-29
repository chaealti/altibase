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
 * $Id: qmnFullOuter.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     FOJN(Full Outer JoiN) Node
 *
 *     ������ �𵨿��� Full Outer Join�� �����ϴ� Plan Node �̴�.
 *     �پ��� Join Method���� ���� ����� ���¿� ���� �����ȴ�.
 *  
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Nested Loop Join �迭
 *         - Sort-based Join �迭
 *         - Hash-based Join �迭
 *
 *     Full Outer Join�� ũ�� ������ ���� �� �ܰ�� ����ȴ�.
 *         - Left Outer Join Phase
 *             : Left Outer Join�� �����ϳ�, �����ϴ� Right Row�� ���Ͽ�
 *               Hit Flag�� Setting�Ͽ� ���� Phase�� ���� ó���� �غ��Ѵ�.
 *         - Right Outer Join Phase
 *             : Right Row�� Hit ���� ���� Row�� ��� Left�� ����
 *               Null Padding�� �����Ѵ�.
 *           
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_FULL_OUTER_H_
#define _O_QMN_FULL_OUTER_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncFOJN.flag
// Right Child Plan�� ����
#define QMNC_FOJN_RIGHT_CHILD_MASK         (0x00000001)
#define QMNC_FOJN_RIGHT_CHILD_HASH         (0x00000000)
#define QMNC_FOJN_RIGHT_CHILD_SORT         (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndFOJN.flag
// First Initialization Done
#define QMND_FOJN_INIT_DONE_MASK           (0x00000001)
#define QMND_FOJN_INIT_DONE_FALSE          (0x00000000)
#define QMND_FOJN_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncFOJN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // FOJN ���� ����
    //---------------------------------

    qtcNode      * filter;     // ON Condition ���� Filter ����

} qmncFOJN;

typedef struct qmndFOJN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;

    //---------------------------------
    // FOJN ���� ����
    //---------------------------------
    
    setHitFlagFunc      setHitFlag;  // Child(HASH, SORT)�� ���� �Լ�

} qmndFOJN;

class qmnFOJN
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

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // ȣ��Ǿ�� �ȵ�
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // Left Outer Join Phase : ���ο� Left Row�� ���� ó��
    static IDE_RC doItLeftHitPhase( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag );

    // Left Outer Join Phase : ���ο� Right Row�� ���� ó��
    static IDE_RC doItRightHitPhase( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag );

    // Right Outer Join Phase : ���� ���� �Լ�
    static IDE_RC doItFirstNonHitPhase( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Right Outer Join Phase : ���� ���� �Լ�
    static IDE_RC doItNextNonHitPhase( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmncFOJN   * aCodePlan,
                             qmndFOJN   * aDataPlan );

};

#endif /* _O_QMN_FULL_OUTER_H_ */
