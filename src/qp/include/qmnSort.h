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
 * $Id: qmnSort.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     SORT(SORT) Node
 *
 *     ������ �𵨿��� sorting ������ �����ϴ� Plan Node �̴�.
 *
 *     ������ ���� ����� ���� ���ȴ�.
 *         - ORDER BY
 *         - Sort-based Grouping
 *         - Sort-based Distinction
 *         - Sort Join
 *         - Merge Join
 *         - Sort-based Left Outer Join
 *         - Sort-based Full Outer Join
 *         - Store And Search
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_SORT_H_
#define _O_QMN_SORT_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcSortTempTable.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

//-----------------------------------------------------
// qmncSORT.flag
// ���� ����� ����
// - ONLY     : ���常 ����
// - PRESERVE : ������ Sorting ���� ������ �����
// - SORTING  : Sorting�� ����
//-----------------------------------------------------

#define QMNC_SORT_STORE_MASK               (0x00000003)
#define QMNC_SORT_STORE_NONE               (0x00000000)
#define QMNC_SORT_STORE_ONLY               (0x00000001)
#define QMNC_SORT_STORE_PRESERVE           (0x00000002)
#define QMNC_SORT_STORE_SORTING            (0x00000003)

//-----------------------------------------------------
// qmncSORT.flag
// �˻� ����� ����
// - SEQUENTIAL : ���� �˻�
// - RANGE      : ���� �˻�
//-----------------------------------------------------

#define QMNC_SORT_SEARCH_MASK              (0x00000004)
#define QMNC_SORT_SEARCH_SEQUENTIAL        (0x00000000)
#define QMNC_SORT_SEARCH_RANGE             (0x00000004)

//-----------------
// Data Node Flags
//-----------------

// qmndSORT.flag
// First Initialization Done
#define QMND_SORT_INIT_DONE_MASK           (0x00000001)
#define QMND_SORT_INIT_DONE_FALSE          (0x00000000)
#define QMND_SORT_INIT_DONE_TRUE           (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT T1.i1, T1.i3, T2.i2 FROM T1,T2 ORDER BY MOD(T1.i2,10);
 *                                            ^^^^^^^^^^^^^^^^^^^^^^
 *  in qmncSORT
 *      myNode : T1 ->  MOD(T1.i2,10)
 *               ^^
 *               base table count : 1
 *  in qmndSORT
 *      mtrNode  : T1 -> MOD(T1.i2,10)
 *      sortNode : MOD(T1.i2,10)
 ----------------------------------------------------------------------*/

struct qmncSORT;
struct qmndSORT;

typedef IDE_RC (* qmnSortSearchFunc ) (  qcTemplate * aTemplate,
                                         qmncSORT   * aCodePlan,
                                         qmndSORT   * aDataPlan,
                                         qmcRowFlag * aFlag );

typedef struct qmncSORT
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // SORT ���� ����
    //---------------------------------

    UShort         baseTableCount;   // Base Table�� ����
    UShort         depTupleRowID;    // Dependent Tuple ID
    SDouble        storeRowCount;    // Sort Node�� ����� ���ڵ� ���� ( �������� )
    qmcMtrNode   * myNode;           // ���� Column ����
    qtcNode      * range;            // Range �˻��� ���� Key Range

    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache

    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;    // size : ID_SIZEOF(qmdMtrNode) * count

} qmncSORT;

typedef struct qmndSORT
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // SORT ���� ����
    //---------------------------------

    UInt                mtrRowSize;   // ���� Row�� ũ��

    qmdMtrNode        * mtrNode;      // ���� Column ����
    qmdMtrNode        * sortNode;     // ���� Column�� ��ġ

    mtcTuple          * depTuple;     // Dependent Tuple ��ġ
    UInt                depValue;     // Dependent Value

    qmcdSortTemp      * sortMgr;      // Sort Temp Table

    qmnSortSearchFunc   searchFirst;  // �˻� �Լ�
    qmnSortSearchFunc   searchNext;   // �˻� �Լ�

    //---------------------------------
    // Merge Join ���� ����
    //---------------------------------

    smiCursorPosInfo    cursorInfo;   // Ŀ�� ���� ����

    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndSORT;

class qmnSORT
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

    // ���� ���� �Լ�
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� ���� �Լ�
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // TODO - A4 Grouping Set Integration
    static IDE_RC doItLast( qcTemplate * aTemplate,
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

    // Merge Join������ Cursor ����
    static IDE_RC storeCursor( qcTemplate * aTemplate,
                               qmnPlan    * aPlan );

    // Merge Join������ Cursor ����
    static IDE_RC restoreCursor( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan );

private:

    //-----------------------------
    // �ʱ�ȭ ���� �Լ�
    //-----------------------------

    // ���� �ʱ�ȭ �Լ�
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncSORT   * aCodePlan,
                             qmndSORT   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncSORT   * aCodePlan,
                               qmndSORT   * aDataPlan );

    // ���� Column�� ��ġ �˻�
    static IDE_RC initSortNode( qmncSORT   * aCodePlan,
                                qmndSORT   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncSORT   * aCodePlan,
                                 qmndSORT   * aDataPlan );

    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qcTemplate * aTemplate,
                                   qmncSORT   * aCodePlan,
                                   qmndSORT   * aDataPlan,
                                   idBool     * aDependent );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // ���� �� ���� ����
    static IDE_RC storeAndSort( qcTemplate * aTemplate,
                                qmncSORT   * aCodePlan,
                                qmndSORT   * aDataPlan );

    // �ϳ��� Row�� ����
    static IDE_RC addOneRow( qcTemplate * aTemplate,
                             qmndSORT   * aDataPlan );

    // ���� Row�� ����
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndSORT   * aDataPlan );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    // �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndSORT   * aDataPlan );

    //-----------------------------
    // �˻� ���� �Լ�
    //-----------------------------

    // �˻� �Լ��� ����
    static IDE_RC setSearchFunction( qmncSORT   * aCodePlan,
                                     qmndSORT   * aDataPlan );

    // ȣ��Ǿ�� �ȵ�
    static IDE_RC searchDefault( qcTemplate * aTemplate,
                                 qmncSORT   * aCodePlan,
                                 qmndSORT   * aDataPlan,
                                 qmcRowFlag * aFlag );

    // ù��° ���� �˻�
    static IDE_RC searchFirstSequence( qcTemplate * aTemplate,
                                       qmncSORT   * aCodePlan,
                                       qmndSORT   * aDataPlan,
                                       qmcRowFlag * aFlag );

    // ���� ���� �˻�
    static IDE_RC searchNextSequence( qcTemplate * aTemplate,
                                      qmncSORT   * aCodePlan,
                                      qmndSORT   * aDataPlan,
                                      qmcRowFlag * aFlag );

    // ù��° Range �˻�
    static IDE_RC searchFirstRangeSearch( qcTemplate * aTemplate,
                                          qmncSORT   * aCodePlan,
                                          qmndSORT   * aDataPlan,
                                          qmcRowFlag * aFlag );

    // ���� Range �˻�
    static IDE_RC searchNextRangeSearch( qcTemplate * aTemplate,
                                         qmncSORT   * aCodePlan,
                                         qmndSORT   * aDataPlan,
                                         qmcRowFlag * aFlag );

    // ù��° Hit �˻�
    static IDE_RC searchFirstHit( qcTemplate * aTemplate,
                                  qmncSORT   * aCodePlan,
                                  qmndSORT   * aDataPlan,
                                  qmcRowFlag * aFlag );

    // ���� Hit �˻�
    static IDE_RC searchNextHit( qcTemplate * aTemplate,
                                 qmncSORT   * aCodePlan,
                                 qmndSORT   * aDataPlan,
                                 qmcRowFlag * aFlag );

    // ù��° Non-Hit �˻�
    static IDE_RC searchFirstNonHit( qcTemplate * aTemplate,
                                     qmncSORT   * aCodePlan,
                                     qmndSORT   * aDataPlan,
                                     qmcRowFlag * aFlag );

    // ���� Non-Hit �˻�
    static IDE_RC searchNextNonHit( qcTemplate * aTemplate,
                                    qmncSORT   * aCodePlan,
                                    qmndSORT   * aDataPlan,
                                    qmcRowFlag * aFlag );

    //-----------------------------
    // A4 - Grouping Set ���� �Լ�
    //-----------------------------

    /*
      static IDE_RC isCurrentGroupingColumn( qcStatement * aStatement,
      qtcNode     * aGroupNode,
      qtcNode     * aMtrRelGraphNode,
      idBool      * aExist );
    */

};


#endif /* _O_QMN_SORT_H_ */
