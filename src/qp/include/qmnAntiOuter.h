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
 * $Id: qmnAntiOuter.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     AOJN(Anti Outer JoiN) Node
 *
 *     ������ �𵨿��� Full Outer Join�� ó���� ����
 *     Ư���� ������ �����ϴ� Plan Node �̴�.
 *
 *     Left Outer Join�� �帧�� �����ϳ� ������ ���� ū ���̰� �ִ�.
 *        - Left Row�� �����ϴ� Right Row�� ���� ��츦 �˻��Ѵ�.
 *        
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_ANTI_OUTER_H_
#define _O_QMN_ANTI_OUTER_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndAOJN.flag
// First Initialization Done
#define QMND_AOJN_INIT_DONE_MASK           (0x00000001)
#define QMND_AOJN_INIT_DONE_FALSE          (0x00000000)
#define QMND_AOJN_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncAOJN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // AOJN ���� ����
    //---------------------------------
    
    qtcNode      * filter;    // ON Condition���� Filter ����
    
} qmncAOJN;

typedef struct qmndAOJN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

} qmndAOJN;

class qmnAOJN
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

    // ���ο� Left Row�� ���� ó��
    static IDE_RC doItLeft( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------
    
    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmndAOJN   * aDataPlan );

};

#endif /* _O_QMN_ANTI_OUTER_H_ */
