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
 * $Id: qmnGroupAgg.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     GRAG(GRoup AGgregation) Node
 *
 *     ������ �𵨿��� Hash-based Grouping ������ �����ϴ� Plan Node �̴�.
 *
 *     Aggregation�� Group By�� ���¿� ���� ������ ���� ������ �Ѵ�.
 *         - Grouping Only
 *         - Aggregation Only
 *         - Group Aggregation
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMN_GROUG_AGG_H_
#define _O_QMN_GROUG_AGG_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmcHashTempTable.h>
#include <qmnDef.h>

//-----------------
// Code Node Flags
//-----------------

// PROJ-2444
#define QMNC_GRAG_PARALLEL_STEP_MASK       (0x0000000F)
#define QMNC_GRAG_NOPARALLEL               (0x00000000)
#define QMNC_GRAG_PARALLEL_STEP_AGGR       (0x00000001)
#define QMNC_GRAG_PARALLEL_STEP_MERGE      (0x00000002)

//-----------------
// Data Node Flags
//-----------------

// qmndGRAG.flag
// First Initialization Done
#define QMND_GRAG_INIT_DONE_MASK           (0x00000001)
#define QMND_GRAG_INIT_DONE_FALSE          (0x00000000)
#define QMND_GRAG_INIT_DONE_TRUE           (0x00000001)

/*---------------------------------------------------------------------
 *  Example)
 *      SELECT i0 % 5, SUM(i1), AVG(i2 + 1) FROM T1 GROUP BY i0 % 5;
 *
 *  in qmncGRAG                                       
 *      myNode : SUM(i1)->SUM(i2+1)->COUNT(i2+1)-> i0 % 5
 *  in qmndGRAG
 *      mtrNode  : SUM(i1)->SUM(i2+1)->COUNT(i2+1)-> i0 % 5
 *      aggrNode : SUM(i1)-> .. until groupNode
 *      groupNode : i0 % 5
 ----------------------------------------------------------------------*/

struct qmncGRAG;

typedef struct qmncGRAG
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // GRAG ���� ����
    //---------------------------------

    UShort         baseTableCount;      // PROJ-1473    
    qmcMtrNode   * myNode;              // ���� Column�� ����
    UInt           bucketCnt;           // Bucket�� ����

    UShort         depTupleRowID;       // Dependent Tuple�� ID

    qcComponentInfo * componentInfo;    // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------
    
    UInt           mtrNodeOffset;       // ���� Column�� ��ġ
    UInt           aggrNodeOffset;      // Aggr Column�� ��ġ

    //---------------------------------
    // TODO - Grouping Set ���� ����
    //---------------------------------
    
    // idBool         isMultiGroup;     // Grouping Set�� �Ǵ� ����
    // qmsGroupIdNode  * groupIdNode;
    
} qmncGRAG;

typedef struct qmndGRAG
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------
    qmndPlan            plan;
    doItFunc            doIt;        
    UInt              * flag;        

    //---------------------------------
    // GRAG ���� ����
    //---------------------------------

    UInt                mtrRowSize;  // ���� Row�� ũ��
    
    qmdMtrNode        * mtrNode;     // ���� Column ����
    
    UInt                aggrNodeCnt; // Aggr Column ����
    qmdMtrNode        * aggrNode;    // Aggr Column ����
    
    qmdMtrNode        * groupNode;   // Grouping Column �� ��ġ

    qmcdHashTemp      * hashMgr;     // Hash Temp Table

    mtcTuple          * depTuple;    // Dependent Tuple ��ġ
    UInt                depValue;    // Dependent Value
    idBool              isNoData;    // Grag Data�� ���� ���

    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
    void              * resultRow;
} qmndGRAG;

class qmnGRAG
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

    static IDE_RC readyIt( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           UInt         aTID );

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

    // Aggregation Only������ ���� ���� �Լ�
    static IDE_RC doItNoData( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag );

private:

    //------------------------
    // �ʱ�ȭ ���� �Լ�
    //------------------------
    
    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncGRAG   * aCodePlan,
                             qmndGRAG   * aDataPlan );

    // ���� Column�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmncGRAG   * aCodePlan,
                               qmndGRAG   * aDataPlan );

    // Aggregation Column�� �ʱ�ȭ
    static IDE_RC initAggrNode( qcTemplate * aTemplate,
                                qmncGRAG   * aCodePlan,
                                qmndGRAG   * aDataPlan );

    // Grouping Column�� ��ġ ����
    static IDE_RC initGroupNode( qmncGRAG   * aCodePlan,
                                 qmndGRAG   * aDataPlan );

    // Temp Table �ʱ�ȭ
    static IDE_RC initTempTable( qcTemplate * aTemplate,
                                 qmncGRAG   * aCodePlan,
                                 qmndGRAG   * aDataPlan );
    
    // Dependent Tuple�� ���� ���� �˻�
    static IDE_RC checkDependency( qcTemplate * aTemplate,
                                   qmncGRAG   * aCodePlan,
                                   qmndGRAG   * aDataPlan,
                                   idBool     * aDependent );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    static IDE_RC storeTempTable( qcTemplate * aTemplate,
                                  qmncGRAG   * aCodePlan,
                                  qmndGRAG   * aDataPlan );

    // Aggregation���� �����Ͽ� �ϳ��� Row�� ����
    static IDE_RC aggregationOnly( qcTemplate * aTemplate,
                                   qmncGRAG   * aCodePlan,
                                   qmndGRAG   * aDataPlan );
    
    // Grouping�� �Ͽ� ����
    static IDE_RC groupingOnly( qcTemplate * aTemplate,
                                qmncGRAG   * aCodePlan,
                                qmndGRAG   * aDataPlan );

    // Group Aggregation�� �����Ͽ� ����
    static IDE_RC groupAggregation( qcTemplate * aTemplate,
                                    qmncGRAG   * aCodePlan,
                                    qmndGRAG   * aDataPlan );

    static IDE_RC aggrOnlyMerge( qcTemplate * aTemplate,
                                 qmncGRAG   * aCodePlan,
                                 qmndGRAG   * aDataPlan );

    static IDE_RC groupOnlyMerge( qcTemplate * aTemplate,
                                  qmncGRAG   * aCodePlan,
                                  qmndGRAG   * aDataPlan );

    static IDE_RC groupAggrMerge( qcTemplate * aTemplate,
                                  qmncGRAG   * aCodePlan,
                                  qmndGRAG   * aDataPlan );

    // PROJ-1473
    static IDE_RC setBaseColumnRow( qcTemplate  * aTemplate,
                                    qmncGRAG    * aCodePlan,
                                    qmndGRAG    * aDataPlan );    

    // Grouping Column�� ���� ����
    static IDE_RC setGroupRow( qcTemplate * aTemplate,
                               qmndGRAG   * aDataPlan );

    // Aggregation Column�� ���� �ʱ�ȭ ����
    static IDE_RC aggrInit( qcTemplate * aTemplate,
                            qmndGRAG   * aDataPlan );

    // Aggregation Column�� ���� ���� ����
    static IDE_RC aggrDoIt( qcTemplate * aTemplate,
                            qmndGRAG   * aDataPlan );

    //-----------------------------
    // ���� ���� �Լ�
    //-----------------------------

    static IDE_RC aggrMerge( qcTemplate * aTemplate,
                             qmndGRAG   * aDataPlan,
                             void       * aSrcRow );

    // Aggregation Column�� ���� ������
    static IDE_RC aggrFini( qcTemplate * aTemplate,
                            qmndGRAG   * aDataPlan );

    // ���� Row�� �̿��Ͽ� Tuple Set�� ����
    static IDE_RC setTupleSet( qcTemplate * aTemplate,
                               qmncGRAG   * aCodePlan,
                               qmndGRAG   * aDataPlan );

    // TODO - A4 Grouping Set Integration
    /*
    static IDE_RC isCurrentGroupingColumn( qcStatement * aStatement,
                                           qtcNode     * aGroupNode,
                                           qmdMtrNode  * aGroup,
                                           idBool      * aExist );
    */
};

#endif /* _O_QMN_GROUG_AGG_H_ */

