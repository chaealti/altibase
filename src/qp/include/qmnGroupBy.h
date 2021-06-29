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
 * $Id: qmnGroupBy.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     GRBY(GRoup BY) Node
 *
 *     ������ �𵨿��� ������ ���� ����� �����ϴ� Plan Node �̴�.
 *
 *         - Sort-based Distinction
 *         - Sort-based Grouping
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_GROUP_BY_H_
#define _O_QMN_GROUP_BY_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// qmncGRBY.flag
// GRBY ����� ���뵵
//    - Sort-based Distinction
//    - Sort-based Grouping
#define QMNC_GRBY_METHOD_MASK              (0x00000003)
#define QMNC_GRBY_METHOD_NOT_DEFINED       (0x00000000)
#define QMNC_GRBY_METHOD_DISTINCTION       (0x00000001)
#define QMNC_GRBY_METHOD_GROUPING          (0x00000002)

//-----------------
// Data Node Flags
//-----------------

// qmndGRBY.flag
// First Initialization Done
#define QMND_GRBY_INIT_DONE_MASK           (0x00000001)
#define QMND_GRBY_INIT_DONE_FALSE          (0x00000000)
#define QMND_GRBY_INIT_DONE_TRUE           (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT i0, i1 SUM(DISTINCT i2) FROM T1 GROUP BY i0, i1;
 *      Plan : PROJ - AGGR - GRBY - SCAN
 *  in qmncGRBY
 *      myNode : i0 -> i1  // no need base table
 *  in qmndGRBY
 *      mtrNode  : T1 -> i0 -> i1
 ----------------------------------------------------------------------*/

typedef struct qmncGRBY
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // GRBY ���� ����
    //---------------------------------

    // Base Table Count�� �������� �ʴ´�.
    // �̴� ���� ���� ��ü�� ���� Node���� �ʿ��� �����̱� �����̴�.
    UShort         baseTableCount;
    qmcMtrNode   * myNode;

    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;

} qmncGRBY;

typedef struct qmndGRBY
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // GRBY ���� ����
    //---------------------------------

    qmdMtrNode        * mtrNode;    // ���� ��� Column
    qmdMtrNode        * groupNode;  // Group Node�� ��ġ

    //---------------------------------
    // ���� Group �Ǵ��� ���� �ڷ� ����
    //---------------------------------

    UInt                mtrRowSize; // ������ Row�� Size
    void              * mtrRow[2];  // �񱳸� ���� �� ���� ����
    UInt                mtrRowIdx;  // swap within mtrRow[0] and mtr2Row[1]

    void              * nullRow;    // Null Row

} qmndGRBY;

class qmnGRBY
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

    // ���� GRBY�� ����
    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // ���� Record������ ���θ� �Ǵ�
    static IDE_RC doItGroup( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    // �ٸ� Record�� ����
    static IDE_RC doItDistinct( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    // TODO - A4 Grouping Set Integration
    static IDE_RC doItLast( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncGRBY   * aCodePlan,
                             qmndGRBY   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncGRBY   * aCodePlan,
                               qmndGRBY   * aDataPlan );

    // Grouping ��� Column�� ��ġ ����
    static IDE_RC initGroupNode( qmndGRBY   * aDataPlan );

    // ������ Row�� ���� �Ҵ� �� Null Row ����
    static IDE_RC allocMtrRow( qcTemplate * aTemplate,
                               qmndGRBY   * aDataPlan );

    //------------------------
    // GRBY �� doIt() ���� �Լ�
    //------------------------

    // ���� Row�� ����.
    static IDE_RC setMtrRow( qcTemplate * aTemplate,
                             qmndGRBY   * aDataPlan );

    // Tuple Set�� ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmndGRBY   * aDataPlan );

    // �� Row���� ���� ���� �Ǵ�
    static SInt   compareRows( qmndGRBY   * aDataPlan );

    //------------------------
    // Grouping Set ���� �Լ�
    //------------------------

    // TODO - A4 : Grouping Set Integration
    /*
      static IDE_RC isCurrentGroupingColumn( qcStatement * aStatement,
      qtcNode     * aGroupNode,
      qtcNode     * aMtrRelGraphNode,
      idBool      * aExist );
    */

};

#endif /* _O_QMN_GROUP_BY_H_ */
