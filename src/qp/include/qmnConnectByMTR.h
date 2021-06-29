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
 * $Id $
 *
 * Description :
 *     CMTR(Connect By MaTeRialization) Node
 *
 *     ������ �𵨿��� Hierarchy�� ���� Materialization�� �����ϴ� Node�̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_CONNECT_BY_MTR_H_
#define _O_QMN_CONNECT_BY_MTR_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcSortTempTable.h>
#include <qmnDef.h>

// First Initialization Done
#define QMND_CMTR_INIT_DONE_MASK           (0x00000001)
#define QMND_CMTR_INIT_DONE_FALSE          (0x00000000)
#define QMND_CMTR_INIT_DONE_TRUE           (0x00000001)

// when Prepare-Execute, Execute is Done
#define QMND_CMTR_PRINTED_MASK             (0x00000002)
#define QMND_CMTR_PRINTED_FALSE            (0x00000000)
#define QMND_CMTR_PRINTED_TRUE             (0x00000002)

typedef struct qmncCMTR
{
    /* Code ���� ���� ���� */
    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    /* CMTR ���� ���� */
    qmcMtrNode   * myNode;

    /* Data ���� ���� ���� */
    UInt           mtrNodeOffset;

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
} qmncCMTR;

typedef struct qmndCMTR
{
    /* Data ���� ���� ���� */
    qmndPlan         plan;
    doItFunc         doIt;      
    UInt           * flag;      

    /* CMTR ���� ���� */
    qmdMtrNode     * mtrNode;
    qmdMtrNode    ** priorNode;
    UInt             priorCount;
    qmcdSortTemp   * sortMgr;
    UInt             rowSize;
    void           * mtrRow;

    /* PROJ-2462 Result Cache */
    qmndResultCache  resultData;
} qmndCMTR;

class qmnCMTR
{
public:

    /* �ʱ�ȭ */
    static IDE_RC init( qcTemplate * aTemplate,
                        qmnPlan    * aPlan );

    /* ���� �Լ�(ȣ��Ǹ� �ȵ�) */
    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    static IDE_RC padNull( qcTemplate * aTemplate,
                           qmnPlan    * aPlan );

    /* Plan ���� ��� */
    static IDE_RC printPlan( qcTemplate   * aTemplate,
                             qmnPlan      * aPlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

    /* HIER���� Null Row�� ȹ���ϱ� ���� ȣ�� */
    static IDE_RC getNullRow( qcTemplate       * aTemplate,
                              qmnPlan          * aPlan,
                              void             * aRow );

    /* HIER���� Row Size�� ȹ���ϱ� ���� ȣ�� */
    static IDE_RC getNullRowSize( qcTemplate       * aTemplate,
                                  qmnPlan          * aPlan,
                                  UInt             * aRowSize );

    /* HIER���� tuple�� ȹ���ϱ� ���� ȣ�� */
    static IDE_RC getTuple( qcTemplate       * aTemplate,
                            qmnPlan          * aPlan,
                            mtcTuple        ** aTuple );
    
    static IDE_RC setPriorNode( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qtcNode    * aPrior );

    static IDE_RC comparePriorNode( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    void       * aPriorRow,
                                    void       * aSearchRow,
                                    idBool     * aResult );
private:

    /* ���� �ʱ�ȭ */
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncCMTR   * aCodePlan,
                             qmndCMTR   * aDataPlan );

    /* ���� Column�� �ʱ�ȭ */
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncCMTR   * aCodePlan,
                               qmndCMTR   * aDataPlan );

    /* Temp Table�� �ʱ�ȭ */
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncCMTR   * aCodePlan,
                                 qmndCMTR   * aDataPlan );

    /* Child�� ���� ����� ���� */
    static IDE_RC storeChild( qcTemplate * aTemplate,
                              qmncCMTR   * aCodePlan,
                              qmndCMTR   * aDataPlan );

    /* ���� Row�� ���� */
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndCMTR   * aDataPlan );
};

#endif /* _O_QMN_CONNECT_BY_MTR_H_ */

