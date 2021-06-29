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
 * $Id: qmnHash.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     HASH(HASH) Node
 *
 *     ������ �𵨿��� hashing ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - Hash Join
 *         - Hash-based Left Outer Join
 *         - Hash-based Full Outer Join
 *         - Store And Search
 *
 *     HASH ���� Two Pass Hash Join� ���� ��,
 *     �������� Temp Table�� ������ �� �ִ�.
 *     ����, ��� ���� �� �˻� �� �̿� ���� ����� ����Ͽ��� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_HASH_H_
#define _O_QMN_HASH_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcHashTempTable.h>
#include <qmnDef.h>

//---------------------------------------------------
// - Two-Pass Hash Join���
//   Temp Table�� ���� ���� ������ �� ������,
//   ���� 1���� Temp Table�� Primary�� ���Ǹ�,
//   ������ Temp Table�� Secondary�� ���ȴ�.
// - HASH ���� Master Table�� ���� �޸� �Ҵ� ��
//   Null Rowȹ���� �����ϰ� �ȴ�.
//---------------------------------------------------

#define QMN_HASH_PRIMARY_ID                        (0)

//-----------------
// Code Node Flags
//-----------------

//----------------------------------------------------
// qmncHASH.flag
// ���� ����� ����
// - HASHING : �Ϲ����� Hashing���� ����
// - DISTINCT : Distinct Hashing���� ����
//              Store And Search�� ���� �� ������
//----------------------------------------------------

#define QMNC_HASH_STORE_MASK              (0x00000003)
#define QMNC_HASH_STORE_NONE              (0x00000000)
#define QMNC_HASH_STORE_HASHING           (0x00000001)
#define QMNC_HASH_STORE_DISTINCT          (0x00000002)

//----------------------------------------------------
// qmncHASH.flag
// �˻� ����� ����
// - SEQUENTIAL : ���� �˻�
//                Two-Pass Hash Join�� left���� ���
// - RANGE      : ���� �˻�
//                Hash Join�� Right���� ���
// - SEARCH     : STORE AND SEARCH �˻�
//----------------------------------------------------

#define QMNC_HASH_SEARCH_MASK             (0x00000030)
#define QMNC_HASH_SEARCH_NONE             (0x00000000)
#define QMNC_HASH_SEARCH_SEQUENTIAL       (0x00000010)
#define QMNC_HASH_SEARCH_RANGE            (0x00000020)
#define QMNC_HASH_SEARCH_STORE_SEARCH     (0x00000030)

//----------------------------------------------------
// qmncHASH.flag
// Store And Search�� NULL ���� ������ �˻�
// Hash�� �̿��� Store And Search�� ������ ����
// ������ ���� ����̴�.
//    - �� Column�� ���� Filter
//        - NOT NULL Constraint�� �����ϸ�,
//          NULL�˻� �ʿ� ����.
//        - NOT NULL Constraint�� �������� ������,
//          NULL�˻� �ʿ���
//    - ���� Column�� ���� Filter
//        - �ݵ�� NOT NULL Constraints�� �����ؾ�
//          �ϸ�, ���� NULL�˻� �ʿ� ����
//  NULL_CHECK_FALSE : NULL CHECK�� �ʿ� ����
//  NULL_CHECK_TRUE  : NULL CHECK �ʿ���.
//----------------------------------------------------

#define QMNC_HASH_NULL_CHECK_MASK         (0x00000100)
#define QMNC_HASH_NULL_CHECK_FALSE        (0x00000000)
#define QMNC_HASH_NULL_CHECK_TRUE         (0x00000100)

//-----------------
// Data Node Flags
//-----------------

// qmndHASH.flag
// First Initialization Done
#define QMND_HASH_INIT_DONE_MASK          (0x00000001)
#define QMND_HASH_INIT_DONE_FALSE         (0x00000000)
#define QMND_HASH_INIT_DONE_TRUE          (0x00000001)

// qmndHash.flag
// ����� �ش��ϴ� ���� NULL������ ����
// Store And Search�� ��� ���� ���� �˻� ����� �޸��Ѵ�.
#define QMND_HASH_CONST_NULL_MASK         (0x00000002)
#define QMND_HASH_CONST_NULL_FALSE        (0x00000000)
#define QMND_HASH_CONST_NULL_TRUE         (0x00000002)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT * FROM T1,T2 WHERE T1.i1 = T2.i1 + 3;
 *
 *  in qmncHASH
 *      myNode : T2 -> T2.i1 + 3
 *               ^^
 *               baseTableCount : 1
 *      filter   : ( = )
 *      filterConst : T1.i1
 *  in qmndHASH
 *      mtrNode  : T2 -> T2.i1 + 3
 *      hashNode : T2.i1 + 3
 ----------------------------------------------------------------------*/

struct qmncHASH;
struct qmndHASH;

typedef IDE_RC (* qmnHashSearchFunc ) (  qcTemplate * aTemplate,
                                         qmncHASH   * aCodePlan,
                                         qmndHASH   * aDataPlan,
                                         qmcRowFlag * aFlag );

typedef struct qmncHASH
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

    UShort         baseTableCount;
    qmcMtrNode   * myNode;          // ���� Column ����

    UShort         depTupleRowID;   // dependent tuple ID

    //---------------------------------
    // Temp Table ���� ����
    //---------------------------------

    UInt           bucketCnt;      // Bucket�� ����
    UInt           tempTableCnt;   // Temp Table�� ����

    //---------------------------------
    // Range �˻� ����
    //---------------------------------

    qtcNode      * filter;         // if filter t1.i1 = t2.i1(right)
    qtcNode      * filterConst;    // filterConst is t1.i1
                                   //          JOIN
                                   //        |       |
                                   //    SCAN(t1)   HASH
                                   //                |
                                   //               SCAN(t2)

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;     // ���� Column�� ��ġ

} qmncHASH;

typedef struct qmndHASH
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // HASH ���� ����
    //---------------------------------

    UInt                mtrRowSize;   // ���� Row�� ũ��

    qmdMtrNode        * mtrNode;      // ���� Column ����
    qmdMtrNode        * hashNode;     // Hashing Column�� ��ġ

    mtcTuple          * depTuple;     // Dependent Tuple ��ġ
    UInt                depValue;     // Dependent Value

    qmcdHashTemp      * hashMgr;      // ���� ���� Hash Temp Table
    SLong               mtrTotalCnt;  // ����� Data�� ����

    qmnHashSearchFunc   searchFirst;  // �˻� �Լ�
    qmnHashSearchFunc   searchNext;   // �˻� �Լ�

    UInt                currTempID;    // ���� Temp Table ID
    idBool              isNullStore;   // Null�� Store�ϰ� �ִ��� ����
    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndHASH;

class qmnHASH
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

    // ù��° ���� �Լ�
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� ���� �Լ�
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Store And Search�� ù��° ���� �Լ�
    static IDE_RC doItFirstStoreSearch( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Store And Search�� ���� ���� �Լ�
    static IDE_RC doItNextStoreSearch( qcTemplate * aTemplate,
                                       qmnPlan    * aPlan,
                                       qmcRowFlag * aFlag );

    //------------------------
    // Direct External Call
    //------------------------

    // Hit �˻� ���� ����
    static void setHitSearch( qcTemplate * aTemplate,
                              qmnPlan    * aPlan );

    // Non-Hit �˻� ���� ����
    static void setNonHitSearch( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan );

    // Record�� Hit ǥ��
    static IDE_RC setHitFlag( qcTemplate * aTemplate,
                              qmnPlan    * aPlan );

    // Record�� Hit ǥ��Ǿ����� ����
    static idBool isHitFlagged( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
private:

    //-----------------------------
    // �ʱ�ȭ ���� �Լ�
    //-----------------------------

    // ���� �ʱ�ȭ �Լ�
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncHASH   * aCodePlan,
                             qmndHASH   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncHASH   * aCodePlan,
                               qmndHASH   * aDataPlan );

    // Hashing Column�� ���� ��ġ
    static IDE_RC initHashNode( qmncHASH   * aCodePlan,
                                qmndHASH   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncHASH   * aCodePlan,
                                 qmndHASH   * aDataPlan );

    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qcTemplate * aTemplate,
                                   qmncHASH   * aCodePlan,
                                   qmndHASH   * aDataPlan,
                                   idBool     * aDependent );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // Hash Temp Table�� �����Ѵ�.
    static IDE_RC storeAndHashing( qcTemplate * aTemplate,
                                   qmncHASH   * aCodePlan,
                                   qmndHASH   * aDataPlan );

    // ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndHASH   * aDataPlan );

    // ���� Row�� Null ���� �˻�
    static IDE_RC checkNullExist( qmncHASH   * aCodePlan,
                                  qmndHASH   * aDataPlan );

    // ���� Row�� Hash Key �� ȹ��
    static IDE_RC getMtrHashKey( qmndHASH   * aDataPlan,
                                 UInt       * aHashKey );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // ��� ������ Hash Key�� ȹ��
    static IDE_RC getConstHashKey( qcTemplate * aTemplate,
                                   qmncHASH   * aCodePlan,
                                   qmndHASH   * aDataPlan,
                                   UInt       * aHashKey );

    // ���� Row�� �̿��� Tuple Set�� ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndHASH   * aDataPlan );

    //-----------------------------
    // �˻� ���� �Լ�
    //-----------------------------

    // �˻� �Լ��� ����
    static IDE_RC setSearchFunction( qmncHASH   * aCodePlan,
                                     qmndHASH   * aDataPlan );

    // ȣ��Ǿ�� �ȵ�
    static IDE_RC searchDefault( qcTemplate * aTemplate,
                                 qmncHASH   * aCodePlan,
                                 qmndHASH   * aDataPlan,
                                 qmcRowFlag * aFlag );

    // ù��° ���� �˻�
    static IDE_RC searchFirstSequence( qcTemplate * aTemplate,
                                       qmncHASH   * aCodePlan,
                                       qmndHASH   * aDataPlan,
                                       qmcRowFlag * aFlag );
    // ���� ���� �˻�
    static IDE_RC searchNextSequence( qcTemplate * aTemplate,
                                      qmncHASH   * aCodePlan,
                                      qmndHASH   * aDataPlan,
                                      qmcRowFlag * aFlag );

    // ù��° ���� �˻�
    static IDE_RC searchFirstFilter( qcTemplate * aTemplate,
                                     qmncHASH   * aCodePlan,
                                     qmndHASH   * aDataPlan,
                                     qmcRowFlag * aFlag );

    // ���� ���� �˻�
    static IDE_RC searchNextFilter( qcTemplate * aTemplate,
                                    qmncHASH   * aCodePlan,
                                    qmndHASH   * aDataPlan,
                                    qmcRowFlag * aFlag );

    // ù��° Non-Hit �˻�
    static IDE_RC searchFirstNonHit( qcTemplate * aTemplate,
                                     qmncHASH   * aCodePlan,
                                     qmndHASH   * aDataPlan,
                                     qmcRowFlag * aFlag );

    // ���� Non-Hit �˻�
    static IDE_RC searchNextNonHit( qcTemplate * aTemplate,
                                    qmncHASH   * aCodePlan,
                                    qmndHASH   * aDataPlan,
                                    qmcRowFlag * aFlag );

    /* PRORJ-2339 */
    // ù��° Hit �˻�
    static IDE_RC searchFirstHit( qcTemplate * aTemplate,
                                  qmncHASH   * aCodePlan,
                                  qmndHASH   * aDataPlan,
                                  qmcRowFlag * aFlag );

    // ���� Hit �˻�
    static IDE_RC searchNextHit( qcTemplate * aTemplate,
                                 qmncHASH   * aCodePlan,
                                 qmndHASH   * aDataPlan,
                                 qmcRowFlag * aFlag );

};

#endif /* _O_QMN_HASH_H_ */
