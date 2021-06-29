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
 * $Id: qmnProject.h 90527 2021-04-09 04:25:41Z jayce.park $
 *
 * Description :
 *     PROJ(PROJection) Node
 *
 *     ������ �𵨿��� projection�� �����ϴ� Plan Node �̴�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_PROJ_H_
#define _O_QMN_PROJ_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qmcMemSortTempTable.h>

//-----------------
// Code Node Flags
//-----------------

// qmncPROJ.flag
// Limit�� ���������� ���� ����
# define QMNC_PROJ_LIMIT_MASK               (0x00000001)
# define QMNC_PROJ_LIMIT_FALSE              (0x00000000)
# define QMNC_PROJ_LIMIT_TRUE               (0x00000001)

// qmncPROJ.flag
// Top Projection���� ���Ǵ� ���� ����
#define QMNC_PROJ_TOP_MASK                  (0x00000002)
#define QMNC_PROJ_TOP_FALSE                 (0x00000000)
#define QMNC_PROJ_TOP_TRUE                  (0x00000002)

// qmncPROJ.flag
// Indexable MIN-MAX ����ȭ�� ����Ǿ������� ����
#define QMNC_PROJ_MINMAX_MASK               (0x00000004)
#define QMNC_PROJ_MINMAX_FALSE              (0x00000000)
#define QMNC_PROJ_MINMAX_TRUE               (0x00000004)

/*
 * PROJ-1071 Parallel Query
 * qmncPROJ.flag
 * subquery �� top ����..
 */
#define QMNC_PROJ_QUERYSET_TOP_MASK         (0x00000008)
#define QMNC_PROJ_QUERYSET_TOP_FALSE        (0x00000000)
#define QMNC_PROJ_QUERYSET_TOP_TRUE         (0x00000008)

#define QMNC_PROJ_TOP_RESULT_CACHE_MASK     (0x00000010)
#define QMNC_PROJ_TOP_RESULT_CACHE_FALSE    (0x00000000)
#define QMNC_PROJ_TOP_RESULT_CACHE_TRUE     (0x00000010)

//-----------------
// Data Node Flags
//-----------------

// qmndPROJ.flag
# define QMND_PROJ_INIT_DONE_MASK           (0x00000001)
# define QMND_PROJ_INIT_DONE_FALSE          (0x00000000)
# define QMND_PROJ_INIT_DONE_TRUE           (0x00000001)

// qmndPROJ.flag
// when ::doIt is executed 
#define QMND_PROJ_FIRST_DONE_MASK           (0x00000002)
#define QMND_PROJ_FIRST_DONE_FALSE          (0x00000000)
#define QMND_PROJ_FIRST_DONE_TRUE           (0x00000002)


/*---------------------------------------------------------------------
 *  Example)
 *      SELECT i2, i4 FROM T1 WHERE i2 > 3;
 *      ---------------------
 *      | i1 | i2 | i3 | i4 |
 *      ~~~~~~~~~~~~~~~~~~~~~
 *       mtc->mtc->mtc->mtc
 *        |    |    |    |
 *       qtc  qtc  qtc  qtc
 *             |         |
 *            qmc  ->   qmc
 *
 *  in qmncPROJ
 *      myTarget : i2 -> i4
 *  in qmndPROJ
 ----------------------------------------------------------------------*/


typedef struct qmncPROJ  
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan               plan;
    UInt                  flag;
    UInt                  planID;

    //---------------------------------
    // Target ���� ����
    //---------------------------------
    UInt                  targetCount;  // Target Column�� ����
    qmsTarget           * myTarget;     // Target ����
    qcParseSeqCaches    * nextValSeqs;  // Sequence Next Value ����
    qtcNode             * level;        // LEVEL Pseudo Column ����

    /* PROJ-1715 Hierarchy Pseudo Column */
    qtcNode             * isLeaf;

    // PROJ-2551 simple query ����ȭ
    idBool                isSimple;    // simple target
    qmnValueInfo        * simpleValues;
    UInt                * simpleValueSizes;
    UInt                * simpleResultOffsets;
    UInt                  simpleResultSize;
    
    //---------------------------------
    // Limitation ���� ����
    //---------------------------------
    qmsLimit            * limit;

    //---------------------------------
    // LOOP ���� ����
    //---------------------------------
    qtcNode             * loopNode;
    qtcNode             * loopLevel;        // LOOP_LEVEL Pseudo Column ����    

    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    
    UInt                  myTargetOffset; // myTarget�� ����
    
} qmncPROJ;

typedef struct qmndPROJ
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan       plan;
    doItFunc       doIt;        
    UInt         * flag;        

    //---------------------------------
    // PROJ ���� ����
    //---------------------------------

    ULong          tupleOffset; // Top Projection���� ���� Record�� ũ�� ����

    //---------------------------------
    // Limitation ���� ����
    //---------------------------------
    
    doItFunc       limitExec;    // only for limit option
    ULong          limitCurrent; // ���� Limit ��
    ULong          limitStart;   // ���� Limit ��
    ULong          limitEnd;     // ���� Limit ��

    //---------------------------------
    // Loop ���� ����
    //---------------------------------

    SLong        * loopCurrentPtr;
    SLong          loopCurrent; // ���� Loop ��
    SLong          loopCount;   // ���� Limit ��

    //---------------------------------
    // Null Row ���� ����
    //---------------------------------

    UInt           rowSize;    // Row�� �ִ� Size
    void         * nullRow;    // Null Row
    mtcColumn    * nullColumn; // Null Row�� Column ����

    // BUG-48776
    // Scalar sub-query result keeping
    void         * mKeepValue;
    mtcColumn    * mKeepColumn;

    // PROJ-2462 ResultCache
    // Top Result Cache�� ���� ��� VMTR ������ PROJ
    // �����ȴ� �̶� PROJ �� ViewSCAN ��Ȱ�� �Ѵ�.
    qmcdMemSortTemp * memSortMgr;
    qmdMtrNode      * memSortRecord;
    SLong             recordCnt;
    SLong             recordPos;
    UInt              nullRowSize;
} qmndPROJ;

class qmnPROJ
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

    // Non-Top Projection�� ����
    static IDE_RC doItProject( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    // Top Projection�� ����
    static IDE_RC doItTopProject( qcTemplate * aTemplate,
                                  qmnPlan    * aPlan,
                                  qmcRowFlag * aFlag );

    // Limit Projection�� ����
    static IDE_RC doItWithLimit( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    //------------------------
    // Direct External Call only for root node
    //------------------------

    // Target�� �ִ� ũ�� ȹ��
    static IDE_RC getRowSize( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              UInt       * aSize );

    // Target ���� ȹ��
    static IDE_RC getCodeTargetPtr( qmnPlan    * aPlan,
                                    qmsTarget ** aTarget );

    // Communication Buffer�� ���� ������ ũ��
    static ULong  getActualSize( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan );

    // PROJ-1075 target column�� ����
    static UInt   getTargetCount( qmnPlan    * aPlan );

    // Target�� �ִ� ũ�� ���
    static IDE_RC getMaxRowSize( qcTemplate * aTemplate,
                                 qmncPROJ   * aCodePlan,
                                 UInt       * aSize );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncPROJ   * aCodePlan,
                             qmndPROJ   * aDataPlan );
    
    // Null Row�� ����
    static IDE_RC makeNullRow( qcTemplate * aTemplate,
                               qmncPROJ   * aCodePlan,
                               qmndPROJ   * aDataPlan );

    // LEVEL pseudo column�� �� �ʱ�ȭ
    static IDE_RC initLevel( qcTemplate * aTemplate,
                             qmncPROJ   * aCodePlan );

    // Flag�� ���� ���� �Լ� ����
    static IDE_RC setDoItFunction( qmncPROJ   * aCodePlan,
                                   qmndPROJ   * aDataPlan );

    //------------------------
    // doIt ���� �Լ�
    //------------------------

    static IDE_RC readSequence( qcTemplate * aTemplate,
                                qmncPROJ   * aCodePlan,
                                qmndPROJ   * aDataPlan );

    static void setLoopCurrent( qcTemplate * aTemplate,
                                qmnPlan    * aPlan );
    
    //------------------------
    // Plan Display ���� �Լ�
    //------------------------

    // Target ������ ����Ѵ�.
    static IDE_RC printTargetInfo( qcTemplate   * aTemplate,
                                   qmncPROJ     * aCodePlan,
                                   iduVarString * aString );

    // PROJ-2462 Result Cache
    static IDE_RC getVMTRInfo( qcTemplate * aTemplate,
                               qmncPROJ   * aCodePlan,
                               qmndPROJ   * aDataPlan );

    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmncPROJ   * aCodePlan,
                               qmndPROJ   * aDataPlan );
    //PROJ-2462 Result Cache
    static IDE_RC doItVMTR( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );
};

#endif /* _O_QMN_PROJ_H_ */
