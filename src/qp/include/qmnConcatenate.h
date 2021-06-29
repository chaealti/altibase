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
 * $Id: qmnConcatenate.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     CONC(CONCaternation) Node
 *
 *     ������ �𵨿��� Concatenation�� �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *        - DNF�� ó��
 *        - Full Outer Join�� Anti-Outer ����ȭ
 *
 *     Left Child�� ���� Data�� Right Child�� ���� Data��
 *     ��� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_CONCATENATE_H_
#define _O_QMN_CONCATENATE_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndCONC.flag
// First Initialization Done
#define QMND_CONC_INIT_DONE_MASK           (0x00000001)
#define QMND_CONC_INIT_DONE_FALSE          (0x00000000)
#define QMND_CONC_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncCONC
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

} qmncCONC;

typedef struct qmndCONC
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

} qmndCONC;

class qmnCONC
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

    // Left Child�� ���� ó��
    static IDE_RC doItLeft( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Right Child�� ���� ó��
    static IDE_RC doItRight( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

private:

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmndCONC   * aDataPlan );

};

#endif /* _O_QMN_CONCATENATE_H_ */
