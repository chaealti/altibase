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
 * $Id: qmnLimitSort.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     LMST(LiMit SorT) Node
 *
 *     ������ �𵨿��� sorting ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Limit Order By
 *             : SELECT * FROM T1 ORDER BY I1 LIMIT 10;
 *         - Store And Search
 *             : MIN, MAX ������ ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_LIMIT_SORT_H_
#define _O_QMN_LIMIT_SORT_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcSortTempTable.h>
#include <qmnDef.h>

// Limit Sort�� ����� �� �ִ� �ִ� Limit ����
#define QMN_LMST_MAXIMUM_LIMIT_CNT              (65536)

//-----------------
// Code Node Flags
//-----------------

// qmncLMST.flag
// LMST ����� �뵵
// - ORDERBY     : Limit Order By�� ���� ���
// - STORESEARCH : Store And Search�� ���� ���

#define QMNC_LMST_USE_MASK                 (0x00000001)
#define QMNC_LMST_USE_ORDERBY              (0x00000000)
#define QMNC_LMST_USE_STORESEARCH          (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndLMST.flag
// First Initialization Done
#define QMND_LMST_INIT_DONE_MASK           (0x00000001)
#define QMND_LMST_INIT_DONE_FALSE          (0x00000000)
#define QMND_LMST_INIT_DONE_TRUE           (0x00000001)

typedef struct qmncLMST
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // LMST ���� ����
    //---------------------------------

    UShort         baseTableCount;     // Base Table�� ����
    qmcMtrNode   * myNode;           // ���� Column ����

    UShort         depTupleRowID;    // Dependent Tuple ID
    ULong          limitCnt;         // ������ Row�� ��

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------
    
    UInt           mtrNodeOffset;    // ���� Column�� ��ġ
    
    
} qmncLMST;

typedef struct qmndLMST
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;        

    //---------------------------------
    // LMST ���� ����
    //---------------------------------

    UInt                mtrRowSize;   // ���� Row�� ũ��

    qmdMtrNode        * mtrNode;      // ���� Column ����
    qmdMtrNode        * sortNode;     // ���� Column�� ��ġ

    mtcTuple          * depTuple;     // Dependent Tuple ��ġ
    UInt                depValue;     // Dependent Value 

    qmcdSortTemp      * sortMgr;      // Sort Temp Table
    ULong               mtrTotalCnt;  // ���� ��� Row�� ����
    idBool              isNullStore; 
    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndLMST;

class qmnLMST
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

private:

    //-----------------------------
    // �ʱ�ȭ ���� �Լ�
    //-----------------------------

    // ���� �ʱ�ȭ �Լ�
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncLMST   * aCodePlan,
                             qmndLMST   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncLMST   * aCodePlan,
                               qmndLMST   * aDataPlan );

    // ���� Column�� ��ġ �˻�
    static IDE_RC initSortNode( qmncLMST   * aCodePlan,
                                qmndLMST   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncLMST   * aCodePlan,
                                 qmndLMST   * aDataPlan );

    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qcTemplate * aTemplate,
                                   qmncLMST   * aCodePlan,
                                   qmndLMST   * aDataPlan,
                                   idBool     * aDependent );

    //------------------------
    // mapping by doIt() function pointer
    //------------------------

    // ȣ��Ǿ�� �ȵ�.
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // LIMIT ORDER BY�� ���� ���� ����
    static IDE_RC doItFirstOrderBy( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag );

    // LIMIT ORDER BY�� ���� ���� ����
    static IDE_RC doItNextOrderBy( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   qmcRowFlag * aFlag );

    // Store And Search�� ���� ���� ����
    static IDE_RC doItFirstStoreSearch( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Store And Search�� ���� ���� ����
    static IDE_RC doItNextStoreSearch( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    // Store And Search�� ���� ������ ����
    static IDE_RC doItLastStoreSearch( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    // BUG-37681 limitCnt 0 �϶� �����ϴ� �Լ�
    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // ORDER BY�� ���� ����
    static IDE_RC store4OrderBy( qcTemplate * aTemplate,
                                 qmncLMST   * aCodePlan,
                                 qmndLMST   * aDataPlan );

    // Store And Search�� ���� ����
    static IDE_RC store4StoreSearch( qcTemplate * aTemplate,
                                     qmncLMST   * aCodePlan,
                                     qmndLMST   * aDataPlan );

    // �ϳ��� Row�� ����
    static IDE_RC addOneRow( qcTemplate * aTemplate,
                             qmndLMST   * aDataPlan );
    
    // ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndLMST   * aDataPlan );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndLMST   * aDataPlan );
    
};



#endif /* _O_QMN_LIMIT_SORT_H_ */

