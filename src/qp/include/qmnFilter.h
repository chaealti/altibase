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
 * $Id: qmnFilter.h 82075 2018-01-17 06:39:52Z jina.kim $ 
 *
 * Description :
 *     FILT(FILTer) Node
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

#ifndef _O_QMN_FILT_H_
#define _O_QMN_FILT_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------


//-----------------
// Data Node Flags
//-----------------

/* qmndFILT.flag                                     */
// First Initialization Done
#define QMND_FILT_INIT_DONE_MASK           (0x00000001)
#define QMND_FILT_INIT_DONE_FALSE          (0x00000000)
#define QMND_FILT_INIT_DONE_TRUE           (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT * FROM T1,T2 WHERE T1.i1 = T2.i1 AND T1.i2 > T2.ii + T1.i3;
 *                                                  ^^^^^^^^^^^^^^^^^^^^^
 *                           {qtc > }
 *                               |
 *                       ---------------
 *                       |             |
 *                  {qtc T1.i2}   { qtc + }
 *                                     |
 *                             -----------------
 *                             |                |
 *                       {qtc T2.i1}       { qtc T1.i3 }
 *
 *  in qmncFILT                                       
 *      filter : qtc >
 *  in qmndFILT
 ----------------------------------------------------------------------*/

typedef struct qmncFILT  
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
    
    qtcNode      * constantFilter;    // Constant Filter
    qtcNode      * filter;            // Normal Filter(Subquery Filter����)
    
} qmncFILT;

typedef struct qmndFILT
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan       plan;
    doItFunc       doIt;
    UInt         * flag;

} qmndFILT;

class qmnFILT
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
    
    // FILT�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qmncFILT   * aCodePlan,
                             qmndFILT   * aDataPlan );

    //------------------------
    // Plan Display ���� �Լ�
    //------------------------

    // Predicate�� �� ������ ����Ѵ�.
    static IDE_RC printPredicateInfo( qcTemplate   * aTemplate,
                                      qmncFILT     * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString );

};

#endif /* _O_QMN_FILT_H_ */
