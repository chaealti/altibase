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
 * $Id: qmnViewMaterialize.h 82075 2018-01-17 06:39:52Z jina.kim $ 
 *
 * Description :
 *     VMTR(View MaTeRialization) Node
 *
 *     ������ �𵨿��� View�� ���� Materialization�� �����ϴ� Node�̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_VIEW_MATERIALIZE_H_
#define _O_QMN_VIEW_MATERIALIZE_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcSortTempTable.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------


//-----------------
// Data Node Flags
//-----------------

// qmndVMTR.flag
// First Initialization Done
#define QMND_VMTR_INIT_DONE_MASK           (0x00000001)
#define QMND_VMTR_INIT_DONE_FALSE          (0x00000000)
#define QMND_VMTR_INIT_DONE_TRUE           (0x00000001)

// qmndVMTR.flag
// when Prepare-Execute, Execute is Done
#define QMND_VMTR_PRINTED_MASK            (0x00000002)
#define QMND_VMTR_PRINTED_FALSE           (0x00000000)
#define QMND_VMTR_PRINTED_TRUE            (0x00000002)

typedef struct qmncVMTR
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // VMTR ���� ����
    //---------------------------------

    qmcMtrNode   * myNode;    
    // PROJ-2415 Grouping Sets Clause
    UShort         depTupleID;    // Dependent Tuple ID

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    
    UInt           mtrNodeOffset;
} qmncVMTR;

typedef struct qmndVMTR
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan         plan;
    doItFunc         doIt;      
    UInt           * flag;      

    //---------------------------------
    // VMTR ���� ����
    //---------------------------------
    // PROJ-2415 Grouping Sets Clause
    mtcTuple       * myTuple;     // Tuple ��ġ
    mtcTuple       * depTuple;    // Dependent Tuple ��ġ
    UInt             depValue;    // Dependent Value
    
    qmdMtrNode     * mtrNode;     
    qmcdSortTemp   * sortMgr;
    UInt             viewRowSize;
    void           * mtrRow;
    
    /* PROJ-2462 Result Cache */
    qmndResultCache  resultData;
} qmndVMTR;

class qmnVMTR
{
public:

    //------------------------
    // Base Function Pointer
    //------------------------

    // �ʱ�ȭ
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    // ���� �Լ�(ȣ��Ǹ� �ȵ�)
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
    // External Direct Call
    //------------------------

    //  VSCN���� VMTR�� �����ϱ� ���� ���� ����
    static IDE_RC getCursorInfo( qcTemplate       * aTemplate,
                                 qmnPlan          * aPlan,
                                 void            ** aTableHandle,
                                 void            ** aIndexHandle,
                                 qmcdMemSortTemp ** aMemSortTemp,
                                 qmdMtrNode      ** aMemSortRecord );

    // VSCN���� Null Row�� ȹ���ϱ� ���� ȣ��
    static IDE_RC getNullRowDisk( qcTemplate       * aTemplate,
                                  qmnPlan          * aPlan,
                                  void             * aRow,
                                  scGRID           * aRowRid );

    // VSCN���� Null Row�� ȹ���ϱ� ���� ȣ��
    static IDE_RC getNullRowMemory( qcTemplate *  aTemplate,
                                    qmnPlan    *  aPlan,
                                    void       ** aRow );

    // VSCN���� Row Size�� ȹ���ϱ� ���� ȣ��
    static IDE_RC getNullRowSize( qcTemplate       * aTemplate,
                                  qmnPlan          * aPlan,
                                  UInt             * aRowSize );

    // VSCN���� Tuple�� ȹ���ϱ� ���� ȣ��
    static IDE_RC getTuple( qcTemplate       * aTemplate,
                            qmnPlan          * aPlan,
                            mtcTuple        ** aTuple );

    // PROJ-2582 recursive with
    // SREC���� mtrNode�� ȹ���ϱ� ���� ȣ��
    static IDE_RC getMtrNode( qcTemplate       * aTemplate,
                              qmnPlan          * aPlan,
                              qmdMtrNode      ** aNode );
    
    static IDE_RC resetDependency( qcTemplate  * aTemplate,
                                   qmnPlan     * aPlan );
    
private:

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncVMTR   * aCodePlan,
                             qmndVMTR   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncVMTR   * aCodePlan,
                               qmndVMTR   * aDataPlan );

    // Temp Table�� �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncVMTR   * aCodePlan,
                                 qmndVMTR   * aDataPlan );

    // Child�� ���� ����� ����
    static IDE_RC storeChild( qcTemplate * aTemplate,
                              qmncVMTR   * aCodePlan,
                              qmndVMTR   * aDataPlan );

    // ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndVMTR   * aDataPlan );

    // PROJ-2415 Grouping Sets Clause
    // Dependency �˻�
    static IDE_RC checkDependency( qcTemplate * /* aTemplate */,
                                   qmncVMTR   * /* aCodePlan */,
                                   qmndVMTR   * aDataPlan,
                                   idBool     * aDependent );
};

#endif /* _O_QMN_VIEW_MATERIALIZE_H_ */
