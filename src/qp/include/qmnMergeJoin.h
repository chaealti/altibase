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
 * $Id: qmnMergeJoin.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     MGJN(MerGe JoiN) Node
 *
 *     ������ �𵨿��� Merge Join ������ �����ϴ� Plan Node �̴�.
 *
 *     Join Method�� Merge Join���� ����ϸ�, Left�� Right Child�δ�
 *     ������ ���� Plan Node���� ������ �� �ִ�.
 *
 *         - SCAN Node
 *         - SORT Node
 *         - MGJN Node
 *
 *     To Fix PR-11944
 *     Right ���� MGJN�� ������ �� ������ ������ ���� Node���� ������ �� �ִ�.
 *
 *         - SCAN Node
 *         - SORT Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_MERGE_JOIN_H_
#define _O_QMN_MERGE_JOIN_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncMGJN.flag
// Left Child Plan�� ����
#define QMNC_MGJN_LEFT_CHILD_MASK          (0x00000007)
#define QMNC_MGJN_LEFT_CHILD_NONE          (0x00000000)
#define QMNC_MGJN_LEFT_CHILD_SCAN          (0x00000001)
#define QMNC_MGJN_LEFT_CHILD_SORT          (0x00000002)
#define QMNC_MGJN_LEFT_CHILD_MGJN          (0x00000003)
#define QMNC_MGJN_LEFT_CHILD_PCRD          (0x00000004)

// qmncMGJN.flag
// Right Child Plan�� ����
#define QMNC_MGJN_RIGHT_CHILD_MASK         (0x00000030)
#define QMNC_MGJN_RIGHT_CHILD_NONE         (0x00000000)
#define QMNC_MGJN_RIGHT_CHILD_SCAN         (0x00000010)
#define QMNC_MGJN_RIGHT_CHILD_SORT         (0x00000020)
#define QMNC_MGJN_RIGHT_CHILD_PCRD         (0x00000030)

// qmncMGJN.flag
// PROJ-1718 Subquery unnesting
// Join�� ����
#define QMNC_MGJN_TYPE_MASK                (0x00000300)
#define QMNC_MGJN_TYPE_INNER               (0x00000000)
#define QMNC_MGJN_TYPE_SEMI                (0x00000100)
#define QMNC_MGJN_TYPE_ANTI                (0x00000200)
#define QMNC_MGJN_TYPE_UNDEFINED           (0x00000300)

//-----------------
// Data Node Flags
//-----------------

// qmndMGJN.flag
// First Initialization Done
#define QMND_MGJN_INIT_DONE_MASK           (0x00000001)
#define QMND_MGJN_INIT_DONE_FALSE          (0x00000000)
#define QMND_MGJN_INIT_DONE_TRUE           (0x00000001)

// qmndMGJN.flag
// Cursor�� ����Ǿ� �ִ� ���� ����
// ��, ������ ������ Data�� �����߾������� ����
#define QMND_MGJN_CURSOR_STORED_MASK       (0x00000002)
#define QMND_MGJN_CURSOR_STORED_FALSE      (0x00000000)
#define QMND_MGJN_CURSOR_STORED_TRUE       (0x00000002)

#define QMND_MGJN_RIGHT_DATA_MASK          (0x00000004)
#define QMND_MGJN_RIGHT_DATA_NONE          (0x00000000)
#define QMND_MGJN_RIGHT_DATA_EXIST         (0x00000004)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT * FROM T1,T2 WHERE T1.i1 = T2.i1 AND T1.i2 + T2.i2 > 0;
 *
 *                   [MGJN]
 *                    /  \
 *                   /    \
 *                [SCAN] [SCAN]
 *                  T1     T2
 *
 *   qmncMGJN �� ����
 *
 *       - myNode : Join Predicate�� Right Column�� ����
 *                  Ex) T2.i1�� ����
 *
 *       - mergeJoinPredicate : Merge Joinable Predicate�� ����
 *                  Ex) T1.i1 = T2.i1
 *
 *       - storedJoinPredicate : Merge Joinable Predicate�� ���� Column��
 *                  ���� �� �ֵ��� ����
 *                  Ex) T1.i1 = Stored[T2.i1]
 *
 *       - compareLeftRight : Merge Joinable Predicate�� �����ڰ� ��ȣ��
 *                  ��� �����ϸ� ��� �񱳰� �����ϵ��� ����
 *                  Ex) T1.i1 >= T2.i1  (>=)�� �����ؾ� ��
 *
 *       - joinFilter : Merge Joinable Predicate�� �ƴ� Join Filter
 *                  Ex) T1.i2 + T2.i2 > 0
 *
 *   Merge Join �˰����� ���� ������ ����
 *       - "4.2.2.2 Query Executor Design - Part 2.doc"
 *           : Merge Join ����
 *       - "4.2.2.3 Query Executor Design - Part 3.doc"
 *           : MGJN Node ����
 ----------------------------------------------------------------------*/

typedef struct qmncMGJN
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // MGJN ���� ����
    //---------------------------------

    qmcMtrNode    * myNode;               // ������ Right Column ����

    // Merge Joinable Predicate ���� ����
    qtcNode       * mergeJoinPred;        // Merge Join Predicate
    
    qtcNode       * storedMergeJoinPred;  // ���� �����Ϳ� ���ϴ� Predicate
    qtcNode       * compareLeftRight;     // �ε�ȣ �������� ����� ��Һ�

    qtcNode       * joinFilter;           // ��Ÿ Join Predicate

    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt            mtrNodeOffset;        // ���� Column�� ��ġ
    
} qmncMGJN;

typedef struct qmndMGJN
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;        

    //---------------------------------
    // MGJN ���� ����
    //---------------------------------

    qmdMtrNode        * mtrNode;        // ���� Right Column�� ����
    UInt                mtrRowSize;     // ���� Right Column�� ũ��
    
} qmndMGJN;

class qmnMGJN
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

    // ȣ��Ǹ� �ȵ�
    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    static IDE_RC doItInner( qcTemplate * aTemplate,
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

    static IDE_RC doItSemi( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    static IDE_RC doItAnti( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncMGJN   * aCodePlan,
                             qmndMGJN   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncMGJN   * aCodePlan,
                               qmndMGJN   * aDataPlan );

    //------------------------
    // ���� ���� �Լ�
    //------------------------

    // Merge Join Algorithm�� ����
    static IDE_RC mergeJoin( qcTemplate * aTemplate,
                             qmncMGJN   * aCodePlan,
                             qmndMGJN   * aDataPlan,
                             idBool       aRightExist,
                             qmcRowFlag * aFlag );

    // Merge Join�� ����� �ְų� ���� ������ �ݺ� ����
    static IDE_RC loopMerge( qcTemplate * aTemplate,
                             qmncMGJN   * aCodePlan,
                             qmndMGJN   * aDataPlan,
                             qmcRowFlag * aFlag );

    // Merge Join ������ �˻��ϰ� ���� ���θ� ����.
    static IDE_RC checkMerge( qcTemplate * aTemplate,
                              qmncMGJN   * aCodePlan,
                              qmndMGJN   * aDataPlan,
                              idBool     * aContinueNeed,
                              qmcRowFlag * aFlag );
    
    // ���� Stored Merge ���� �˻�
    static IDE_RC checkFirstStoredMerge( qcTemplate * aTemplate,
                                         qmncMGJN   * aCodePlan,
                                         qmndMGJN   * aDataPlan,
                                         qmcRowFlag * aFlag );

    // Stored Merge Join ������ �˻��ϰ� ���� ���θ� ����.
    static IDE_RC checkStoredMerge( qcTemplate * aTemplate,
                                    qmncMGJN   * aCodePlan,
                                    qmndMGJN   * aDataPlan,
                                    idBool     * aContinueNeed,
                                    qmcRowFlag * aFlag );
    
    // Merge Join Predicate ���� �� Cursor ����
    static IDE_RC manageCursor( qcTemplate * aTemplate,
                                qmncMGJN   * aCodePlan,
                                qmndMGJN   * aDataPlan );

    // ������ Left �Ǵ� Right Row�� ȹ���Ѵ�.
    static IDE_RC readNewRow( qcTemplate * aTemplate,
                              qmncMGJN   * aCodePlan,
                              idBool     * aReadLeft,
                              qmcRowFlag * aFlag );

    // Join Filter�� ���� ���� �˻�
    static IDE_RC checkJoinFilter( qcTemplate * aTemplate,
                                   qmncMGJN   * aCodePlan,
                                   idBool     * aResult );
    
    //------------------------
    // Ŀ�� ���� �Լ�
    //------------------------

    // Right Child�� Ŀ���� ����
    static IDE_RC storeRightCursor( qcTemplate * aTemplate,
                                    qmncMGJN   * aCodePlan );

    // Right Child�� Ŀ���� ����
    static IDE_RC restoreRightCursor( qcTemplate * aTemplate,
                                    qmncMGJN   * aCodePlan,
                                    qmndMGJN   * aDataPlan );

};



#endif /* _O_QMN_MERGE_JOIN_H_ */

