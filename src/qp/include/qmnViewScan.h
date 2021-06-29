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
 * $Id: qmnViewScan.h 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *     VSCN(View SCaN) Node
 *
 *     ������ �𵨿��� Materialized View�� ����
 *     Selection�� �����ϴ� Node�̴�.
 *
 *     ���� VMTR ����� ���� ��ü�� ���� �ٸ� ������ �ϰ� �Ǹ�,
 *     Memory Temp Table�� ��� Memory Sort Temp Table ��ü��
 *        interface�� ���� ����Ͽ� �����ϸ�,
 *     Disk Temp Table�� ��� table handle�� index handle�� ���
 *        ������ Cursor�� ���� Sequetial Access�Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_VIEW_SCAN_H_
#define _O_QMN_VIEW_SCAN_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------
// Data Node Flags
//-----------------

// qmndVSCN.flag
// First Initialization Done
#define QMND_VSCN_INIT_DONE_MASK           (0x00000001)
#define QMND_VSCN_INIT_DONE_FALSE          (0x00000000)
#define QMND_VSCN_INIT_DONE_TRUE           (0x00000001)

#define QMND_VSCN_DISK_CURSOR_MASK         (0x00000002)
#define QMND_VSCN_DISK_CURSOR_CLOSED       (0x00000000)
#define QMND_VSCN_DISK_CURSOR_OPENED       (0x00000002)

typedef struct qmncVSCN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // VSCN ���� ����
    //---------------------------------

    UShort         tupleRowID;

    //---------------------------------
    // Display ���� ����
    //---------------------------------

    qmsNamePosition  viewOwnerName;
    qmsNamePosition  viewName;
    qmsNamePosition  aliasName;

} qmncVSCN;

typedef struct qmndVSCN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan             plan;
    doItFunc             doIt;
    UInt               * flag;

    //---------------------------------
    // VSCN ���� ����
    //---------------------------------

    UInt                 nullRowSize;   // Null row Size
    void               * nullRow;       // Null Row
    scGRID               nullRID;       // Null Row�� RID

    //---------------------------------
    // Memory Temp Table ���� ����
    //---------------------------------

    qmcdMemSortTemp    * memSortMgr;
    qmdMtrNode         * memSortRecord;
    SLong                recordCnt;     // VMTR�� Record ����
    SLong                recordPos;     // ���� Recrord ��ġ

    //---------------------------------
    // Disk Temp Table ���� ����
    //---------------------------------

    void               * tableHandle;
    void               * indexHandle;

    smiSortTempCursor  * tempCursor;         // tempCursor
} qmndVSCN;

class qmnVSCN
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

    // Memory Temp Table ��� �� ���� ���� �Լ�
    static IDE_RC doItFirstMem( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    // Memory Temp Table ��� �� ���� ���� �Լ�
    static IDE_RC doItNextMem( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // Disk Temp Table ��� �� ���� ���� �Լ�
    static IDE_RC doItFirstDisk( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    // Disk Temp Table ��� �� ���� ���� �Լ�
    static IDE_RC doItNextDisk( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    // PROJ-2582 recursive with
    static IDE_RC touchDependency( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan );
    
private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncVSCN   * aCodePlan,
                             qmndVSCN   * aDataPlan );

    // PROJ-2415 Grouping Sets Clause
    // ���� �ʱ�ȭ ���� �ʱ�ȭ�� ����
    static IDE_RC initForChild( qcTemplate * aTemplate,
                                qmncVSCN   * aCodePlan,
                                qmndVSCN   * aDataPlan );
    
    // Tuple�� �����ϴ� Column�� offset ������
    static IDE_RC refineOffset( qcTemplate * aTemplate,
                                qmncVSCN   * aCodePlan,
                                qmndVSCN   * aDataPlan );

    // VMTR child�� ����
    static IDE_RC execChild( qcTemplate * aTemplate,
                             qmncVSCN   * aCodePlan );

    // Null Row�� ȹ��.
    static IDE_RC getNullRow( qcTemplate * aTemplate,
                              qmncVSCN   * aCodePlan,
                              qmndVSCN   * aDataPlan );

    // tuple set ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmncVSCN   * aCodePlan,
                               qmndVSCN   * aDataPlan );

    //------------------------
    // Disk ���� ���� �Լ�
    //------------------------

    static IDE_RC openCursor( qmndVSCN   * aDataPlan );

};

#endif /* _O_QMN_VIEW_SCAN_H_ */
