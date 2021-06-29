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
 * $Id: qmnHashDist.h 82075 2018-01-17 06:39:52Z jina.kim $ 
 *
 * Description :
 *     HSDS(HaSh DiStinct) Node
 *
 *     ������ �𵨿��� hash-based distinction ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Hash-based Distinction
 *         - Set Union
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_HASH_DIST_H_
#define _O_QMN_HASH_DIST_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcHashTempTable.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncHSDS.flag
// HSDS ��尡 Top Query���� ���Ǵ����� ����
// Top Query���� ���� ��� ��� ����� ������ �� �ִ� �ݸ�,
// Subquery�� View���� �����ϴ� ���� ��� ����� ������ ��
// ó���Ͽ��� �Ѵ�.
#define QMNC_HSDS_IN_TOP_MASK                (0x00000001)
#define QMNC_HSDS_IN_TOP_TRUE                (0x00000000)
#define QMNC_HSDS_IN_TOP_FALSE               (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndHSDS.flag
// First Initialization Done
#define QMND_HSDS_INIT_DONE_MASK             (0x00000001)
#define QMND_HSDS_INIT_DONE_FALSE            (0x00000000)
#define QMND_HSDS_INIT_DONE_TRUE             (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT DISTINCT i1, i2 FROM T1;
 *                                    
 *  in qmncHSDS                                       
 *      myNode : T1 -> T1.i1 -> T1.i2
 *  in qmndHSDS
 *      mtrNode  : T1 -> T1.i1 -> T1.i2
 *      hashNode : T1.i1 -> T1.i2
 ----------------------------------------------------------------------*/

typedef struct qmncHSDS
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // HASH ���� ����
    //---------------------------------

    UShort         baseTableCount;  // PROJ-1473    
    qmcMtrNode   * myNode;          // ���� Column�� ����
    UShort         depTupleRowID;   // Dependent Tuple ID

    UInt           bucketCnt;       // Hash Bucket�� ����

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;   // ���� Column�� ��ġ
    
} qmncHSDS;

typedef struct qmndHSDS
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;        

    //---------------------------------
    // HSDS ���� ����
    //---------------------------------
    
    UInt                mtrRowSize;  // ���� Row�� ũ��
    
    qmdMtrNode        * mtrNode;     // ���� Column�� ����
    qmdMtrNode        * hashNode;    // Hashing Column�� ��ġ

    mtcTuple          * depTuple;    // Dependent Tuple�� ��ġ
    UInt                depValue;    // Dependent Value
    
    qmcdHashTemp      * hashMgr;     // Hash Temp Table

    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndHSDS;

class qmnHSDS
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

    // Top Query���� ���� ����� ȣ�� �Լ�
    static IDE_RC doItDependent( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    // Subquery ������ ���� ����� ���� ���� �Լ�
    static IDE_RC doItFirstIndependent( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Subquery ������ ���� ����� ���� ���� �Լ�
    static IDE_RC doItNextIndependent( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

private:

    //-----------------------------
    // �ʱ�ȭ ���� �Լ�
    //-----------------------------
    
    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncHSDS   * aCodePlan,
                             qmndHSDS   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncHSDS   * aCodePlan,
                               qmndHSDS   * aDataPlan );

    // Hashing Column�� ���� ��ġ
    static IDE_RC initHashNode( qmndHSDS   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncHSDS   * aCodePlan,
                                 qmndHSDS   * aDataPlan );

    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qcTemplate * aTemplate,
                                   qmncHSDS   * aCodePlan,
                                   qmndHSDS   * aDataPlan,
                                   idBool     * aDependent );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndHSDS   * aDataPlan );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // �˻��� Row�� �̿��� Tuple Set�� ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndHSDS   * aDataPlan );
};

#endif /*  _O_QMN_HSDS_DIST_H_ */

