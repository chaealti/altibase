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
 * $Id: qmnFilter.h 20233 2007-08-06 01:58:21Z sungminee $ 
 *
 * Description :
 *     CNTR(CouNTeR) Node
 *
 *     ������ �𵨿��� selection�� �����ϴ� Plan Node �̴�.
 *     SCAN ���� �޸� Storage Manager�� ���� �������� �ʰ�,
 *     �̹� selection�� record�� ���� selection�� �����Ѵ�..
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_CNTR_H_
#define _O_QMN_CNTR_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------


//-----------------
// Data Node Flags
//-----------------

/* qmndCNTR.flag                                     */
// First Initialization Done
#define QMND_CNTR_INIT_DONE_MASK           (0x00000001)
#define QMND_CNTR_INIT_DONE_FALSE          (0x00000000)
#define QMND_CNTR_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncCNTR  
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // Predicate ����
    //---------------------------------
    
    qtcNode      * stopFilter;     // Stop Filter
    
    //---------------------------------
    // Rownum ���
    //---------------------------------

    UShort         rownumRowID;    // rownum row
    
} qmncCNTR;

typedef struct qmndCNTR
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan       plan;
    doItFunc       doIt;
    UInt         * flag;

    //---------------------------------
    // CNTR ���� ����
    //---------------------------------

    mtcTuple     * rownumTuple; // rownum of current row.
    
    SLong        * rownumPtr;
    SLong          rownumValue;
    
} qmndCNTR;

class qmnCNTR
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
    
    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // �׻� ������ ������ �� ���� ���
    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );
    
    // CNTR�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� CNTR�� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );
    
private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncCNTR   * aCodePlan,
                             qmndCNTR   * aDataPlan );

    //------------------------
    // Plan Display ���� �Լ�
    //------------------------

    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncCNTR     * aCodePlan,
                                      qmndCNTR     * aDataPlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

};

#endif /* _O_QMN_CNTR_H_ */
