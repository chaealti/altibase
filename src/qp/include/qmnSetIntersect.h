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
 * $Id: qmnSetIntersect.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     SITS(Set IntTerSection) Node
 *
 *     ������ �𵨿��� hash-based set intersection ������
 *     �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_SET_INTERSECT_H_
#define _O_QMN_SET_INTERSECT_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcHashTempTable.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncSITS.flag
// SITS ��尡 Top Query���� ���Ǵ����� ����
// Top Query���� ���� ��� ��� ����� ������ �� �ִ� �ݸ�,
// Subquery�� View���� �����ϴ� ���� ��� ����� ������ ��
// ó���Ͽ��� �Ѵ�.
#define QMNC_SITS_IN_TOP_MASK                (0x00000001)
#define QMNC_SITS_IN_TOP_TRUE                (0x00000000)
#define QMNC_SITS_IN_TOP_FALSE               (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndSITS.flag
// First Initialization Done
#define QMND_SITS_INIT_DONE_MASK           (0x00000001)
#define QMND_SITS_INIT_DONE_FALSE          (0x00000000)
#define QMND_SITS_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncSITS
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // SITS ���� ����
    //---------------------------------
    
    qmcMtrNode   * myNode;              // ���� Column�� ����

    UShort         leftDepTupleRowID;   // Left Dependent Tuple
    UShort         rightDepTupleRowID;  // Right Dependent Tuple
    
    UInt           bucketCnt;           // Hash Bucket�� ����

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------
    
    UInt           mtrNodeOffset;       // ���� Column�� ��ġ
    
} qmncSITS;

typedef struct qmndSITS
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // SITS ���� ����
    //---------------------------------

    UInt                mtrRowSize;    // ���� Row�� ũ��
    
    qmdMtrNode        * mtrNode;       // ���� Column�� ����

    void              * temporaryRightRow;  // temporary right-side row
    
    mtcTuple          * leftDepTuple;  // Left Dependent Tuple�� ��ġ
    UInt                leftDepValue;  // Left Dependent Value

    mtcTuple          * rightDepTuple; // Right Dependent Tuple�� ��ġ
    UInt                rightDepValue; // Right Dependent Value

    qmcdHashTemp      * hashMgr;       // Hash Temp Table
    
    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndSITS;

class qmnSITS
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

    // Top Query���� ���Ǵ� ���, ���� ���� �Լ�
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // Top Query���� ���Ǵ� ���, ���� ���� �Լ�
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Sub Query���� ���Ǵ� ���, ���� ���� �Լ�
    static IDE_RC doItFirstIndependent( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Sub Query���� ���Ǵ� ���, ���� ���� �Լ�
    static IDE_RC doItNextIndependent(  qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

private:

    //-----------------------------
    // �ʱ�ȭ ���� �Լ�
    //-----------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncSITS   * aCodePlan,
                             qmndSITS   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncSITS   * aCodePlan,
                               qmndSITS   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncSITS   * aCodePlan,
                                 qmndSITS   * aDataPlan );

    // Left Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkLeftDependency( qmndSITS   * aDataPlan,
                                       idBool     * aDependent );

    // Right Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkRightDependency( qmndSITS   * aDataPlan,
                                        idBool     * aDependent );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // Left�� �����Ͽ� distinct hashing ���� ����
    static IDE_RC storeLeft( qcTemplate * aTemplate,
                             qmncSITS   * aCodePlan,
                             qmndSITS   * aDataPlan );

    // Child�� ����� Stack�� �̿��Ͽ� ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndSITS   * aDataPlan );

    // Subquery���� ���Ǵ� ��� Intersected Row���� ��� ����
    static IDE_RC setIntersectedRows( qcTemplate * aTemplate,
                                      qmncSITS   * aCodePlan,
                                      qmndSITS   * aDataPlan );

    // PR-24190
    // select i1(varchar(30)) from t1 minus select i1(varchar(250)) from t2;
    // ����� ���� ����������
    static IDE_RC setRightChildMtrRow( qcTemplate * aTemplate,
                                       qmndSITS   * aDataPlan,
                                       idBool     * aIsSetMtrRow );
    
};

#endif /* _O_QMN_SET_INTERSECT_H_ */
