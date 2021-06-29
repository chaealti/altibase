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
 * $Id: qmnView.h 82075 2018-01-17 06:39:52Z jina.kim $ 
 *
 * Description :
 *     VIEW(VIEW) Node
 *
 *     ������ �𵨿��� ���� Table�� ǥ���ϱ� ���� Node�̴�..
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_VIEW_H_
#define _O_QMN_VIEW_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndVIEW.flag
// First Initialization Done
#define QMND_VIEW_INIT_DONE_MASK           (0x00000001)
#define QMND_VIEW_INIT_DONE_FALSE          (0x00000000)
#define QMND_VIEW_INIT_DONE_TRUE           (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT * FROM (SELECT i1, i2 FROM T1)V1;
 *
 *           ---------------------
 *       T1  | i1 | i2 | i3 | i4 |
 *           ~~~~~~~~~~~~~~~~~~~~~
 *           -----------
 *       V1  | i1 | i2 |
 *           ~~~~~~~~~~~
 *            mtc->mtc
 *             |    |   
 *            qtc  qtc  
 *
 *  in qmncSCAN                                       
 *      myNode : qmc i1
 *      inTarget : T1.i1 -> T1.i2
 *  in qmndGRAG
 ----------------------------------------------------------------------*/

typedef struct qmncVIEW  
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // VIEW ���� ����
    //---------------------------------

    qtcNode      * myNode;    // View�� Column ����

    //---------------------------------
    // Display ���� ����
    //---------------------------------
    
    qmsNamePosition viewOwnerName;
    qmsNamePosition viewName;
    qmsNamePosition aliasName;

} qmncVIEW;

typedef struct qmndVIEW
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan       plan;
    doItFunc       doIt;        
    UInt         * flag;        

    //---------------------------------
    // VIEW ���� ����
    //---------------------------------

} qmndVIEW;

class qmnVIEW
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

    // ���� PROJ �� ����� Record�� ������.
    static IDE_RC doItProject(  qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

private:

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncVIEW   * aCodePlan,
                             qmndVIEW   * aDataPlan );

    // Stack�� ���� ������ �̿��Ͽ� View Record�� ������.
    static IDE_RC setTupleRow( qcTemplate * aTemplate,
                               qmncVIEW   * aCodePlan,
                               qmndVIEW   * aDataPlan );
};

#endif /* _O_QMN_VIEW_H_ */
