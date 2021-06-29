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
 * $Id: qmnAggregation.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     AGGR(AGGRegation) Node
 *
 *     ������ �𵨿��� ������ ���� ����� �����ϴ� Plan Node �̴�.
 *
 *         - Sort-based Grouping
 *         - Distinct Aggregation
 *
 *    - Distinct Column�� Disk Temp Table�� ����ϴ�, Memory Temp Table��
 *      ��뿡 ���� ���� AGGR Node�� Row ������ ���� Tuple Set�� ������
 *      Memory Storage���� �Ѵ�.
 *
 *    - Distinct Column��  Disk�� ������ ���, Plan�� flag������ DISK�� �Ǹ�,
 *      �� Distinct Column�� ���� Tuple Set�� Storage ���� Disk Type�� �ȴ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_AGGREGATION_H_
#define _O_QMN_AGGREGATION_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcHashTempTable.h>
#include <qmnDef.h>


//-----------------
// Code Node Flags
//-----------------

// qmncAGGR.flag - has child GRBY
// GROUP BY�� �����ϴ� ���� ����
#define QMNC_AGGR_GROUPED_MASK             (0x00000001)
#define QMNC_AGGR_GROUPED_FALSE            (0x00000000)
#define QMNC_AGGR_GROUPED_TRUE             (0x00000001)

//-----------------
// Data Node Flags
//-----------------

// qmndAGGR.flag
// First Initialization Done
#define QMND_AGGR_INIT_DONE_MASK           (0x00000001)
#define QMND_AGGR_INIT_DONE_FALSE          (0x00000000)
#define QMND_AGGR_INIT_DONE_TRUE           (0x00000001)

//------------------------------------------
// [qmdDistNode]
//    Aggregation�� Distinct Argument�� ����
//    ���� ������ ����ϴ� ����̴�.
//    (qmdMtrNode)�� ���������� Type Casting�ǹǷ�
//    �� �ڷ� ������ ���濡 �����Ͽ��� �Ѵ�.
//------------------------------------------

typedef struct qmdDistNode
{
    qtcNode           * dstNode;
    mtcTuple          * dstTuple;
    mtcColumn         * dstColumn;
    qmdDistNode       * next;
    qtcNode           * srcNode;
    mtcTuple          * srcTuple;
    mtcColumn         * srcColumn;

    qmcMtrNode        * myNode;
    qmdMtrFunction      func;
    UInt                flag;

    // qmdMtrNode�� casting�ϱ� ������ qmdMtrNode�� �����ϰ� �߰��Ѵ�.
    smiFetchColumnList * fetchColumnList; // PROJ-1705
    mtcTemplate        * tmplate;
    
    // for qmdDistNode
    qmcdHashTemp        hashMgr;    // Hash Temp Table
    void              * mtrRow;
    idBool              isDistinct; // Column�� Distinction ����
} qmdDistNode;

//------------------------------------------
// [qmdAggrNode]
//    Aggregation�� ����ϴ� ����
//    Distinct Argument�� ���� ������ ����
//    �ٸ� ó���� �����ϴ� ����̴�.
//    (qmdMtrNode)�� ���������� Type Casting�ǹǷ�
//    �� �ڷ� ������ ���濡 �����Ͽ��� �Ѵ�.
//------------------------------------------

typedef struct qmdAggrNode
{
    qtcNode        * dstNode;
    mtcTuple       * dstTuple;
    mtcColumn      * dstColumn;
    qmdAggrNode    * next;
    qtcNode        * srcNode;
    mtcTuple       * srcTuple;
    mtcColumn      * srcColumn;

    qmcMtrNode     * myNode;
    qmdMtrFunction   func;
    UInt             flag;

    // qmdMtrNode�� casting�ϱ� ������ qmdMtrNode�� �����ϰ� �߰��Ѵ�.
    smiFetchColumnList * fetchColumnList; // PROJ-1705
    mtcTemplate        * tmplate;
    
    // for qmdAggrNode
    // if DISTINCT AGGREGATION available, or NULL
    qmdDistNode    * myDist;
} qmdAggrNode;

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT i0, SUM(i2 + 1), i1, COUNT(DISTINCT i2), SUM(DISTINCT i2)
 *      FROM T1 GROUP BY i0, i1;
 *
 *  in qmncAGGR
 *      myNode :   SUM() -> COUNT(D) -> SUM(D) -> i0 -> i1
 *      distNode : DISTINCT i2
 *  in qmndAGGR
 *      mtrNode   : SUM() -> COUNT(D) -> SUM(D) -> i0 -> i1
 *      aggrNode  : SUM() -> COUNT(D) -> SUM(D)
 *      distNode  : DISTINCT i2
 *      groupNode : i0 (->i1)
 ----------------------------------------------------------------------*/

typedef struct qmncAGGR
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // AGGR ���� ����
    //---------------------------------

    // Base Table ������ �����Ѵ�.
    // Aggregation�� ��� �ڿ������� Push Projectionȿ���� ����� �����̴�.
    UShort         baseTableCount;
    qmcMtrNode   * myNode;
    qmcMtrNode   * distNode;          // distinct aggregation�� argument

    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;     // ���� Column�� Data ���� ��ġ
    UInt           aggrNodeOffset;    // Aggregation�� Data ���� ��ġ
    UInt           distNodeOffset;    // distinction�� Data ���� ��ġ

} qmncAGGR;


typedef struct qmndAGGR
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // AGGR ���� ����
    //---------------------------------

    qmdMtrNode        * mtrNode;     // ���� Column ����
    qmdMtrNode        * groupNode;   // Group Column�� ��ġ

    UInt                distNodeCnt; // Distinct Argument�� ����
    qmdDistNode       * distNode;    // Distinct ���� Column ����
    UInt                aggrNodeCnt; // Aggr Column ����
    qmdAggrNode       * aggrNode;    // Aggr Column ����

    //---------------------------------
    // ���� �ٸ� Group�� ���� �ڷ� ����
    //---------------------------------

    UInt                mtrRowSize;  // ������ Row�� Size
    void              * mtrRow[2];   // ��� ������ ���� �� ���� ����
    UInt                mtrRowIdx;   // wap within mtrRow[0] and mtr2Row[1]

    void              * nullRow;     // Null Row
} qmndAGGR;

class qmnAGGR
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

    // One-Group Aggregation�� ����
    static IDE_RC doItAggregation( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   qmcRowFlag * aFlag );

    // Multi-Group Aggregation�� ���� ����
    static IDE_RC doItGroupAggregation( qcTemplate * aTemplate,
                                        qmnPlan    * aPlan,
                                        qmcRowFlag * aFlag );

    // Multi-Group Aggregation�� ���� ����
    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    // Record ������ ����
    static IDE_RC doItLast( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncAGGR   * aCodePlan,
                             qmndAGGR   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncAGGR   * aCodePlan,
                               qmndAGGR   * aDataPlan );

    // Distinct Column�� �ʱ�ȭ
    static IDE_RC initDistNode( qcTemplate * aTemplate,
                                qmncAGGR   * aCodePlan,
                                qmndAGGR   * aDataPlan );

    // Aggregation Column�� �ʱ�ȭ
    static IDE_RC initAggrNode( qcTemplate * aTemplate,
                                qmncAGGR   * aCodePlan,
                                qmndAGGR   * aDataPlan );

    // Aggregation Column������ ����
    static IDE_RC linkAggrNode( qmncAGGR   * aCodeNode,
                                qmndAGGR   * aDataNode );

    // Group Column�� �ʱ�ȭ
    static IDE_RC initGroupNode( qmncAGGR   * aCodePlan,
                                 qmndAGGR   * aDataPlan );

    // �� Row�� ���� ���� ����
    static IDE_RC allocMtrRow( qcTemplate * aTemplate,
                               qmndAGGR   * aDataPlan );

    // Null Row�� ����
    static IDE_RC makeNullRow(qcTemplate * aTemplate,
                              qmndAGGR   * aDataPlan );

    //----------------------------------------------------
    // AGGR �� doIt() ���� �Լ�
    //----------------------------------------------------

    //-----------------------------
    // Aggregation �ʱ�ȭ ����
    //-----------------------------

    // Distinct Column�� ���� Temp Table�� Clear��
    static IDE_RC clearDistNode( qmndAGGR   * aDataPlan );

    // Aggregation�� �ʱ�ȭ�� Group ���� ����
    static IDE_RC initAggregation( qcTemplate * aTemplate,
                                   qmndAGGR   * aDataPlan );

    // Grouping ���� ����
    static IDE_RC setGroupColumns( qcTemplate * aTemplate,
                                   qmndAGGR   * aDataPlan );

    //-----------------------------
    // Aggregation ���� ����
    //-----------------------------

    // Aggregation�� �����Ѵ�.
    static IDE_RC execAggregation( qcTemplate * aTemplate,
                                   qmndAGGR   * aDataPlan );

    // Aggregation Argument�� Distinction ���θ� �Ǵ�
    static IDE_RC setDistMtrColumns( qcTemplate * aTemplate,
                                     qmndAGGR * aDataPlan );

    //-----------------------------
    // Aggregation ������ ����
    //-----------------------------

    // Aggregation�� ������
    static IDE_RC finiAggregation( qcTemplate * aTemplate,
                                   qmndAGGR   * aDataPlan );

    // ����� Tuple Set�� ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndAGGR   * aDataPlan );

    // ���ο� Group�� �ö�� ����� ó��
    static IDE_RC setNewGroup( qcTemplate * aTemplate,
                               qmndAGGR   * aDataPlan );

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

#endif /* _O_QMN_AGGREGATION_H_ */
