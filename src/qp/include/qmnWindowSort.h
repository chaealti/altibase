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
 * $Id: qmnWindowSort.h 28920 2008-10-28 00:35:44Z jmkim $
 *
 * Description :
 *    WNST(WiNdow SorT) Node
 *
 * ��� ���� :
 *    ���� �ǹ̸� ������ ���� �ٸ� �ܾ �����ϸ� �Ʒ��� ����.
 *    - Analytic Funtion = Window Function
 *    - Analytic Clause = Window Clause = Over Clause
 *
 * ��� :
 *    WNST(Window Sort)
 *
 **********************************************************************/

#ifndef _O_QMN_WNST_H_
#define _O_QMN_WNST_H_ 1

#include <mt.h>
#include <mtdTypes.h>
#include <qmc.h>
#include <qmcSortTempTable.h>
#include <qmnDef.h>
#include <qmnAggregation.h>

// BUG-40409 row_number_limit
#define QMN_WNST_PUSH_RANK_MAX      (1024)

//-----------------
// Code Node Flags
//-----------------

/* qmncWNST.flag                                    */
/* WNST�� ���� ����� ����  */
#define QMNC_WNST_STORE_MASK                   (0x00000007)
#define QMNC_WNST_STORE_NONE                   (0x00000000)
#define QMNC_WNST_STORE_ONLY                   (0x00000001)
#define QMNC_WNST_STORE_PRESERVED_ORDER        (0x00000002)
#define QMNC_WNST_STORE_SORTING                (0x00000003)
#define QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER  (0x00000004)
#define QMNC_WNST_STORE_LIMIT_SORTING          (0x00000005)

//-----------------
// Data Node Flags
//-----------------

/* qmndWNST.flag                                     */
// First Initialization Done
#define QMND_WNST_INIT_DONE_MASK           (0x00000001)
#define QMND_WNST_INIT_DONE_FALSE          (0x00000000)
#define QMND_WNST_INIT_DONE_TRUE           (0x00000001)

// BUG-33663 Ranking Function
// window node�� ������(aggregate�� update) ����
typedef enum qmcWndExecMethod
{
    QMC_WND_EXEC_NONE = 0,
    QMC_WND_EXEC_PARTITION_ORDER_UPDATE,
    QMC_WND_EXEC_PARTITION_UPDATE,
    QMC_WND_EXEC_ORDER_UPDATE,
    QMC_WND_EXEC_AGGR_UPDATE,
    QMC_WND_EXEC_PARTITION_ORDER_WINDOW_UPDATE,
    QMC_WND_EXEC_ORDER_WINDOW_UPDATE,
    QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LAG,
    QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LEAD,
    QMC_WND_EXEC_ORDER_UPDATE_LAG,
    QMC_WND_EXEC_ORDER_UPDATE_LEAD,
    QMC_WND_EXEC_PARTITION_ORDER_UPDATE_NTILE,
    QMC_WND_EXEC_ORDER_UPDATE_NTILE,
    QMC_WND_EXEC_MAX
} qmcWndExecMethod;

typedef enum qmcWndWindowValueType
{
    QMC_WND_WINDOW_VALUE_LONG = 0,
    QMC_WND_WINDOW_VALUE_DOUBLE,
    QMC_WND_WINDOW_VALUE_DATE,
    QMC_WND_WINDOW_VALUE_NUMERIC,
    QMC_WND_WINDOW_VALUE_NULL,
    QMC_WND_WINDOW_VALUE_MAX
} qmcWndWindowValueType;

typedef struct qmcWndWindowValue
{
    qmcWndWindowValueType type;
    SLong                 longType;
    SDouble               doubleType;
    mtdDateType           dateType;
    mtdDateField          dateField;
    SChar                 numericType[MTD_NUMERIC_SIZE_MAXIMUM];
} qmcWndWindowValue;

typedef struct qmcWndWindow
{
    UInt             rowsOrRange;
    qtcOverWindowOpt startOpt;
    qtcWindowValue   startValue;
    qtcOverWindowOpt endOpt;
    qtcWindowValue   endValue;
} qmcWndWindow;

typedef struct qmcWndNode
{
    qmcMtrNode       * overColumnNode;   /* 'PARTITION BY','ORDER BY' ���� */
    qmcMtrNode       * aggrNode;         /* �м� �Լ� ���� */
    qmcMtrNode       * aggrResultNode;   /* �� �м� �Լ� ����� ����� Sort Temp�� Į�� ���� */
    qmcWndWindow       window;           /* PROJ-1805 window clause */
    qmcWndExecMethod   execMethod;       /* �м� �Լ��� ������ */
    qmcWndNode       * next;
} qmcWndNode;

typedef struct qmdWndNode
{
    qmdMtrNode       * overColumnNode;    /* 'PARTITION BY','ORDER BY' ���� */
    qmdMtrNode       * orderByColumnNode; /* 'ORDER BY' ���� ������ */
    qmdAggrNode      * aggrNode;          /* �м� �Լ� ���� */
    qmdMtrNode       * aggrResultNode;    /* �� �м� �Լ� ����� ����� Sort Temp�� Į�� ���� */
    qmcWndWindow     * window;
    qmcWndExecMethod   execMethod;        /* �м� �Լ��� ������ */
    qmdWndNode       * next;
} qmdWndNode;



/*---------------------------------------------------------------------
 * Code Plan�� ����
 *
 * SELECT sum(i1) OVER ( PARTITION BY i1 ) AS F1,
 *        sum(i2) OVER ( PARTITION BY i1 ) AS F2,
 *        sum(i2) OVER ( PARTITION BY i2 ) AS F3,
 *        sum(i2) OVER ( PARTITION BY i1, i2 ) AS F4,
 *        sum(DISTINCT i2) OVER ( PARTITION BY i1 ) AS F5,
 *   FROM T1;
 *
 * ���� ������ �Ʒ��� ���� Code Plan�� ������.
 * Data Plan�� ������ ������ �����Ƿ� �̸� �����ϸ� �����ϴµ� ������ ��
 * - F1,F2,F3,F4,F5: aggregation �Լ� ������� �����ϴ� Į��
 * - @F1,@F2,@F3,@F4,@F5: aggregation �Լ��� �߰� ����� �����ϴ� Į��
 *
 *    qmncWNST
 *   +--------+
 *   |  myNode|-->i1-->i2-->F1-->F2-->F3-->F4-->F5
 *   |aggrNode|-->@F1-->@F2-->@F3-->@F4-->@F5
 *   |distNode|-->i2
 *   |sortNode|-------------------->[*][*]-------->i2
 *   | wndNode|-->[*][*]            /
 *   +--------+    |  |            i1-->i2
 *                 |  \____________               qmcWndNode
 *         qmcWndNode              \_____________+--------------+
 *        +--------------+                       |overColumnNode|-->i2
 *        |overColumnNode|-->i1                  |      aggrNode|-->@F3
 *        |      aggrNode|-->@F1-->@F2-->@F5     |aggrResultNode|-->F3
 *        |aggrResultNode|-->F1--->F2--->F5      |          next|
 *        |          next|                       +--------------+
 *        +-----------|--+
 *                    |
 *         qmcWndNode |
 *        +--------------+
 *        |overColumnNode|-->i1-->i2
 *        |      aggrNode|-->@F4
 *        |aggrResultNode|-->F4
 *        |          next|
 *        +--------------+
 *
 ----------------------------------------------------------------------*/

typedef struct qmncWNST
{
    //---------------------------------
    // Code ���� ���� ����
    //---------------------------------

    qmnPlan        plan;
    UInt           flag;
    UInt           planID;

    //---------------------------------
    // WNST ���� ����
    //---------------------------------

    // �ʿ��� mtrNode
    qmcMtrNode    * myNode;       // Sort Temp Table�� ���� Į���� ����
    qmcMtrNode    * distNode;     // Aggregation�Լ��� ������ distinct ����
                                  // ex) SUM( DISTINCT i1 )
    qmcMtrNode    * aggrNode;     // Aggregation �߰� ��� ������ ���� ���
    qmcMtrNode   ** sortNode;     // ����Ű�� ���� (�ϳ��� ����Ű�� �ټ��� Į������ ����)
    qmcWndNode   ** wndNode;      // �� ����Ű�� �ش��ϴ� Window Clause ����
                                  // ( =Analytic Clause ���� )
    // mtrNode�� ���õ� count ����
    SDouble         storeRowCount;    // Sort Node�� ����� ���ڵ� ���� ( �������� )
    UInt            sortKeyCnt; // ����Ű�� ����
    UShort          baseTableCount;

    // dependency
    UShort          depTupleRowID; // Dependent Tuple ID
    qcComponentInfo * componentInfo; // PROJ-2462 Result Cache
    //---------------------------------
    // Data ���� ����
    //---------------------------------

    UInt           mtrNodeOffset;  // ���� Column�� Data ���� ��ġ
    UInt           distNodeOffset; // distinction�� Data ���� ��ġ
    UInt           aggrNodeOffset; // Aggregation�� ���� Data ���� ��ġ
    UInt           sortNodeOffset; // ����Ű�� ���� Column�� Data ���� ��ġ
    UInt           wndNodeOffset;  // qmdWndNode�� ���� Data ���� ��ġ
    UInt           sortMgrOffset;  // �ӽ�: PROJ-1431 �Ϸ� �� �ݺ� ����
                                   // ��� ����� ����!
                                   // Sort Manager�� ���� Data ������ ��ġ
} qmncWNST;


typedef struct qmndWNST
{
    //---------------------------------
    // Data ���� ���� ����
    //---------------------------------

    qmndPlan            plan;
    doItFunc            doIt;
    UInt              * flag;

    //---------------------------------
    // WNST ���� ����
    //---------------------------------

    qmcdSortTemp      * sortMgr;        // ���� Sort Temp Table Manager
    qmcdSortTemp      * sortMgrForDisk; // Disk Sort�� ���, �̸�
                                        // �Ҵ��� ���� Sort Temp Table
                                        // Manager�� ���� ��ġ (��
                                        // qmncWNST.sortKeyCnt ��ŭ
                                        // �Ҵ� )
    qmdMtrNode        * mtrNode; // Sort Temp Table�� ���� Į���� ����
    UInt                distNodeCnt; // Distinct Argument�� ����
    qmdDistNode       * distNode;    // Distinct ���� Column ����
    qmdAggrNode       * aggrNode;    // �м� �Լ� ����    
    qmdMtrNode       ** sortNode; // ����Ű�� ���� (�ϳ��� ����Ű�� �ټ��� Į������ ����)
    qmdWndNode       ** wndNode;

    smiCursorPosInfo    cursorInfo; // Store,Restore�� ������ ��ġ ����
    smiCursorPosInfo    partitionCursorInfo;

    mtcTuple          * depTuple; // Dependent Tuple ��ġ
    UInt                depValue; // Dependent Value

    //---------------------------------
    // ���� �ٸ� PARTITION�� ���� �ڷ� ����
    //---------------------------------

    UInt                mtrRowSize;  // ������ Row�� Size
    void              * mtrRow[2];   // ��� ������ ���� �� ���� ����
    UInt                mtrRowIdx;   // �� ���� ���� ��, ���� row�� offset

    /* PROJ-2462 Result Cache */
    qmndResultCache     resultData;
} qmndWNST;

class qmnWNST
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
    
private:

    //----------------------------------------------------
    // �ʱ�ȭ ���� �Լ�
    //----------------------------------------------------

    //------------------------
    // ���� �ʱ�ȭ ���� �Լ�
    // firstInit()
    //------------------------

    // ���� �ʱ�ȭ
    static IDE_RC firstInit( qcTemplate     * aTemplate,
                             const qmncWNST * aCodePlan,
                             qmndWNST       * aDataPlan );

    // ���� Į�� ������ (Materialize ���) ����
    static IDE_RC initMtrNode( qcTemplate     * aTemplate,
                               const qmncWNST * aCodePlan,
                               qmndWNST       * aDataPlan );

    // Analytic Function ���ھ� DISTINCT�� �ִ� ��� ���� ����
    static IDE_RC initDistNode( qcTemplate     * aTemplate,
                                const qmncWNST * aCodePlan,
                                qmdDistNode    * aDistNode,
                                UInt           * aDistNodeCnt );

    // Reporting Aggregation�� ó���ϴ� Į��(�߰���) ������ ����
    static IDE_RC initAggrNode( qcTemplate         * aTemplate,
                                const qmcMtrNode   * aCodeNode,
                                const qmdDistNode  * aDistNode,
                                const UInt           aDistNodeCnt,
                                qmdAggrNode        * aAggrNode );
    
    // ��� ����Ű�� ������ ����
    static IDE_RC initSortNode( const qmncWNST * aCodePlan,
                                const qmndWNST * aDataPlan,
                                qmdMtrNode    ** aSortNode );
    
    // �����ؼ� ����ϴ� mtrNode�� ���� ����
    static IDE_RC initCopiedMtrNode( const qmndWNST   * aDataPlan,
                                     const qmcMtrNode * aCodeNode,
                                     qmdMtrNode       * aDataNode );

    // �����ؼ� ����ϴ� aggrNode�� ���� ����
    static IDE_RC initCopiedAggrNode( const qmndWNST   * aDataPlan,
                                      const qmcMtrNode * aCodeNode,
                                      qmdAggrNode      * aAggrNode );

    // aggrResultNode�� ���� ���� 
    static IDE_RC initAggrResultMtrNode( const qmndWNST   * aDataPlan,
                                         const qmcMtrNode * aCodeNode,
                                         qmdMtrNode       * aDataNode );
    
    // Window Clause (Analytic Clause) ������ ��� qmdWndNode�� ����
    static IDE_RC initWndNode( const qmncWNST    * aCodePlan,
                               const qmndWNST    * aDataPlan,
                               qmdWndNode       ** aWndNode );

    // Sort Temp Table�� �ʱ�ȭ��
    static IDE_RC initTempTable( qcTemplate      * aTemplate,
                                 const qmncWNST  * aCodePlan,
                                 qmndWNST        * aDataPlan,
                                 qmcdSortTemp    * aSortMgr );

    
    //------------------------
    // Analytic Function ���� ���� �Լ�
    // performAnalyticFunctions()
    //------------------------

    // ��� Analytic Function�� �����Ͽ� Temp Table�� ����
    static IDE_RC performAnalyticFunctions( qcTemplate     * aTemplate,
                                            const qmncWNST * aCodePlan,
                                            qmndWNST       * aDataPlan,
                                            UInt             aFlag );

    // Child�� �ݺ� �����Ͽ� Temp Table�� ����
    static IDE_RC insertRowsFromChild( qcTemplate     * aTemplate,
                                       const qmncWNST * aCodePlan,
                                       qmndWNST       * aDataPlan );

    /* BUG-40354 pushed rank */
    /* Child�� �ݺ� �����Ͽ� ���� n���� rocord�� ���� memory temp�� ���� */
    static IDE_RC insertLimitedRowsFromChild( qcTemplate     * aTemplate,
                                              const qmncWNST * aCodePlan,
                                              qmndWNST       * aDataPlan,
                                              SLong            aLimitNum );

    // Reporting Aggregation�� �����ϰ� ����� Temp Table�� Update
    static IDE_RC aggregateAndUpdate( qcTemplate       * aTemplate,
                                      qmndWNST         * aDataPlan,
                                      const qmdWndNode * aWndNode );
    
    //------------------------
    // Reporting Aggregation ���� �Լ�
    //------------------------

    // ��Ƽ�� ������ �������� ���� ���, ��ü�� ���� aggregation ����
    static IDE_RC aggregationOnly( qcTemplate        * aTemplate,
                                   qmndWNST          * aDataPlan,
                                   const qmdAggrNode * aAggrNode,
                                   const qmdMtrNode  * aAggrResultNode );
    
    // ��Ƽ�� ���� aggregation ����
    static IDE_RC partitionAggregation( qcTemplate        * aTemplate,
                                        qmndWNST          * aDataPlan,
                                        const qmdMtrNode  * aOverColumnNode,
                                        const qmdAggrNode * aAggrNode,
                                        const qmdMtrNode  * aAggrResultNode );


    // ��Ƽ�� ���� order�� ���� aggregation ����
    static IDE_RC partitionOrderByAggregation( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );

    static IDE_RC orderByAggregation( qcTemplate  * aTemplate,
                                      qmndWNST    * aDataPlan,
                                      qmdMtrNode  * aOverColumnNode,
                                      qmdAggrNode * aAggrNode,
                                      qmdMtrNode  * aAggrResultNode );

    static IDE_RC updateAggrRows( qcTemplate * aTemplate,
                                  qmndWNST   * aDataPlan,
                                  qmdMtrNode * aAggrResultNode,
                                  qmcRowFlag * aFlag,
                                  SLong        aExecAggrCount );

    // Distinct Column�� ���� Temp Table�� Clear��
    static IDE_RC clearDistNode( qmdDistNode * aDistNode,
                                 const UInt    aDistNodeCnt );

    // Aggregation�� �ʱ�ȭ�� Group ���� ����
    static IDE_RC initAggregation( qcTemplate        * aTemplate,
                                   const qmdAggrNode * aAggrNode );
    
    // Aggregation�� ����
    static IDE_RC execAggregation( qcTemplate         * aTemplate,
                                   const qmdAggrNode  * aAggrNode,
                                   void               * aAggrInfo,
                                   qmdDistNode        * aDistNode,
                                   const UInt           aDistNodeCnt );

    // Distinct Column�� ����
    static IDE_RC setDistMtrColumns( qcTemplate        * aTemplate,
                                     qmdDistNode       * aDistNode,
                                     const UInt          aDistNodeCnt );
    
    // Aggregation�� ������
    static IDE_RC finiAggregation( qcTemplate        * aTemplate,
                                   const qmdAggrNode * aAggrNode );

    // ������ �� Row���� ���� ���θ� �Ǵ�
    static IDE_RC compareRows( const qmndWNST   * aDataPlan,
                               const qmdMtrNode * aMtrNode,
                               qmcRowFlag       * aFlag );

    // �񱳸� ���� ������ �Ҵ�
    static IDE_RC allocMtrRow( qcTemplate     * aTemplate,
                               const qmncWNST * aCodePlan,
                               const qmndWNST * aDataPlan,                               
                               void           * aMtrRow[2] );

    // ù ��° ���ڵ带 ������
    static IDE_RC getFirstRecord( qcTemplate  * aTemplate,
                                  qmndWNST    * aDataPlan,
                                  qmcRowFlag  * aFlag );
    
    // ���� ���ڵ带 ������
    static IDE_RC getNextRecord( qcTemplate  * aTemplate,
                                 qmndWNST    * aDataPlan,
                                 qmcRowFlag  * aFlag );


    //----------------------------------------------------
    // WNST�� doIt() ���� �Լ�
    //----------------------------------------------------

    // ���� Row�� �����Ѵ�.
    static IDE_RC setMtrRow( qcTemplate     * aTemplate,
                             qmndWNST       * aDataPlan );
    
    // �˻��� ���� Row�� �������� Tuple Set�� �����Ѵ�.
    static IDE_RC setTupleSet( qcTemplate   * aTemplate,
                               qmdMtrNode   * aMtrNode,
                               void         * aRow );

    
    //----------------------------------------------------
    // �÷� ��� printPlan() ���� �Լ�
    //----------------------------------------------------

    static IDE_RC printAnalyticFunctionInfo( qcTemplate     * aTemplate,
                                             const qmncWNST * aCodePlan,
                                                   qmndWNST * aDataPlan,
                                             ULong            aDepth,
                                             iduVarString   * aString,
                                             qmnDisplay       aMode );

    static IDE_RC printLinkedColumns( qcTemplate       * aTemplate,
                                      const qmcMtrNode * aNode,
                                      iduVarString     * aString );
    
    static IDE_RC printLinkedColumns( qcTemplate    * aTemplate,
                                      qmdMtrNode    * aNode,
                                      iduVarString  * aString );
    
    static IDE_RC printWindowNode( qcTemplate       * aTemplate,
                                   const qmcWndNode * aWndNode,
                                   ULong              aDepth,
                                   iduVarString     * aString );

    /* PROJ-1805 Window Clause */
    static IDE_RC windowAggregation( qcTemplate   * aTemplate,
                                     qmndWNST     * aDataPlan,
                                     qmcWndWindow * aWindow,
                                     qmdMtrNode   * aOverColumnNode,
                                     qmdMtrNode   * aOrderByColumn,
                                     qmdAggrNode  * aAggrNode,
                                     qmdMtrNode   * aAggrResultNode,
                                     idBool         aIsPartition );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionUnPrecedUnFollowRows( qcTemplate  * aTemplate,
                                                 qmndWNST    * aDataPlan,
                                                 qmdMtrNode  * aOverColumnNode,
                                                 qmdAggrNode * aAggrNode,
                                                 qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderUnPrecedUnFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionUnPrecedCurrentRows( qcTemplate  * aTemplate,
                                                qmndWNST    * aDataPlan,
                                                qmdMtrNode  * aOverColumnNode,
                                                qmdAggrNode * aAggrNode,
                                                qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderUnPrecedCurrentRows( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionUnPrecedPrecedRows( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               SLong         aEndValue,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderUnPrecedPrecedRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           SLong         aEndValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionUnPrecedFollowRows( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               SLong         aEndValue,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderUnPrecedFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           SLong         aEndValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionCurrentUnFollowRows( qcTemplate  * aTemplate,
                                                qmndWNST    * aDataPlan,
                                                qmdMtrNode  * aOverColumnNode,
                                                qmdAggrNode * aAggrNode,
                                                qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderCurrentUnFollowRows( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC currentCurrentRows( qcTemplate  * aTemplate,
                                      qmndWNST    * aDataPlan,
                                      qmdAggrNode * aAggrNode,
                                      qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionCurrentFollowRows( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              SLong         aEndValue,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderCurrentFollowRows( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          SLong         aEndValue,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedUnFollowRows( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               SLong         aStartValue,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedUnFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           SLong         aStartValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedCurrentRows( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              SLong         aStartValue,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedCurrentRows( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          SLong         aStartValue,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedPrecedRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aStartValue,
                                             SLong         aEndValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedPrecedRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aStartValue,
                                         SLong         aEndValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aStartValue,
                                             SLong         aEndValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedFollowRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aStartValue,
                                         SLong         aEndValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionFollowUnFollowRows( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               SLong         aStartValue,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderFollowUnFollowRows( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           SLong         aStartValue,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionFollowFollowRows( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             SLong         aStartValue,
                                             SLong         aEndValue,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderFollowFollowRows( qcTemplate  * aTemplate,
                                         qmndWNST    * aDataPlan,
                                         SLong         aStartValue,
                                         SLong         aEndValue,
                                         qmdAggrNode * aAggrNode,
                                         qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionCurrentUnFollowRange( qcTemplate  * aTemplate,
                                                 qmndWNST    * aDataPlan,
                                                 qmdMtrNode  * aOverColumnNode,
                                                 qmdAggrNode * aAggrNode,
                                                 qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderCurrentUnFollowRange( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC currentCurrentRange( qcTemplate  * aTemplate,
                                       qmndWNST    * aDataPlan,
                                       qmdMtrNode  * aOverColumnNode,
                                       qmdAggrNode * aAggrNode,
                                       qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC calculateInterval( qcTemplate        * aTemplate,
                                     qmcdSortTemp      * aTempTable,
                                     qmdMtrNode        * aNode,
                                     void              * aRow,
                                     SLong               aInterval,
                                     UInt                aIntervalType,
                                     qmcWndWindowValue * aValue,
                                     idBool              aIsPreceding );
    /* PROJ-1805 Window Clause */
    static IDE_RC compareRangeValue( qcTemplate        * aTemplate,
                                     qmcdSortTemp      * aTempTable,
                                     qmdMtrNode        * aNode,
                                     void              * aRow,
                                     qmcWndWindowValue * aValue1,
                                     idBool              aIsSkipMode,
                                     idBool            * aResult );
    /* PROJ-1805 Window Clause */
    static IDE_RC updateOneRowNextRecord( qcTemplate * aTemplate,
                                          qmndWNST   * aDataPlan,
                                          qmdMtrNode * aAggrResultNode,
                                          qmcRowFlag * aFlag );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionUnPrecedPrecedFollowRange( qcTemplate  * aTemplate,
                                                      qmndWNST    * aDataPlan,
                                                      qmdMtrNode  * aOverColumnNode,
                                                      qmdMtrNode  * aOrderByColumn,
                                                      SLong         aEndValue,
                                                      SInt          aEndlType,
                                                      qmdAggrNode * aAggrNode,
                                                      qmdMtrNode  * aAggrResultNode,
                                                      idBool        aIsPreceding );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderUnPrecedPrecedFollowRange( qcTemplate  * aTemplate,
                                                  qmndWNST    * aDataPlan,
                                                  qmdMtrNode  * aOrderByColumn,
                                                  SLong         aEndValue,
                                                  SInt          aEndType,
                                                  qmdAggrNode * aAggrNode,
                                                  qmdMtrNode  * aAggrResultNode,
                                                  idBool        aIsPreceding );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionCurrentFollowRange( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               qmdMtrNode  * aOrderByColumn,
                                               SLong         aEndValue,
                                               SInt          aEndType,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderCurrentFollowRange( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOrderByColumn,
                                           SLong         aEndValue,
                                           SInt          aEndType,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedFollowUnFollowRange( qcTemplate  * aTemplate,
                                                      qmndWNST    * aDataPlan,
                                                      qmdMtrNode  * aOverColumnNode,
                                                      qmdMtrNode  * aOrderByColumn,
                                                      SLong         aStartValue,
                                                      SInt          aStartType,
                                                      qmdAggrNode * aAggrNode,
                                                      qmdMtrNode  * aAggrResultNode,
                                                      idBool        aIsPreceding );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedFollowUnFollowRange( qcTemplate  * aTemplate,
                                                  qmndWNST    * aDataPlan,
                                                  qmdMtrNode  * aOrderByColumn,
                                                  SLong         aStartValue,
                                                  SInt          aStartType,
                                                  qmdAggrNode * aAggrNode,
                                                  qmdMtrNode  * aAggrResultNode,
                                                  idBool        aIsPreceding );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedCurrentRange( qcTemplate  * aTemplate,
                                               qmndWNST    * aDataPlan,
                                               qmdMtrNode  * aOverColumnNode,
                                               qmdMtrNode  * aOrderByColumn,
                                               SLong         aStartValue,
                                               SInt          aStartType,
                                               qmdAggrNode * aAggrNode,
                                               qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedCurrentRange( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOrderByColumn,
                                           SLong         aStartValue,
                                           SInt          aStartType,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );

    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedPrecedRange( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              qmdMtrNode  * aOrderByColumn,
                                              SLong         aStartValue,
                                              SInt          aStartType,
                                              SLong         aEndValue,
                                              SInt          aEndType,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedPrecedRange( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdMtrNode  * aOrderByColumn,
                                          SLong         aStartValue,
                                          SInt          aStartType,
                                          SLong         aEndValue,
                                          SInt          aEndType,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionPrecedFollowRange( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              qmdMtrNode  * aOrderByColumn,
                                              SLong         aStartValue,
                                              SInt          aStartType,
                                              SLong         aEndValue,
                                              SInt          aEndType,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderPrecedFollowRange( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdMtrNode  * aOrderByColumn,
                                          SLong         aStartValue,
                                          SInt          aStartType,
                                          SLong         aEndValue,
                                          SInt          aEndType,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC partitionFollowFollowRange( qcTemplate  * aTemplate,
                                              qmndWNST    * aDataPlan,
                                              qmdMtrNode  * aOverColumnNode,
                                              qmdMtrNode  * aOrderByColumn,
                                              SLong         aStartValue,
                                              SInt          aStartType,
                                              SLong         aEndValue,
                                              SInt          aEndType,
                                              qmdAggrNode * aAggrNode,
                                              qmdMtrNode  * aAggrResultNode );
    /* PROJ-1805 Window Clause */
    static IDE_RC orderFollowFollowRange( qcTemplate  * aTemplate,
                                          qmndWNST    * aDataPlan,
                                          qmdMtrNode  * aOrderByColumn,
                                          SLong         aStartValue,
                                          SInt          aStartType,
                                          SLong         aEndValue,
                                          SInt          aEndType,
                                          qmdAggrNode * aAggrNode,
                                          qmdMtrNode  * aAggrResultNode );

    /* PROJ-1804 Ranking Function */
    static IDE_RC partitionOrderByLagAggr( qcTemplate  * aTemplate,
                                           qmndWNST    * aDataPlan,
                                           qmdMtrNode  * aOverColumnNode,
                                           qmdAggrNode * aAggrNode,
                                           qmdMtrNode  * aAggrResultNode );

    /* PROJ-1804 Ranking Function */
    static IDE_RC orderByLagAggr( qcTemplate  * aTemplate,
                                  qmndWNST    * aDataPlan,
                                  qmdAggrNode * aAggrNode,
                                  qmdMtrNode  * aAggrResultNode );

    /* PROJ-1804 Ranking Function */
    static IDE_RC partitionOrderByLeadAggr( qcTemplate  * aTemplate,
                                            qmndWNST    * aDataPlan,
                                            qmdMtrNode  * aOverColumnNode,
                                            qmdAggrNode * aAggrNode,
                                            qmdMtrNode  * aAggrResultNode );

    /* PROJ-1804 Ranking Function */
    static IDE_RC orderByLeadAggr( qcTemplate  * aTemplate,
                                   qmndWNST    * aDataPlan,
                                   qmdAggrNode * aAggrNode,
                                   qmdMtrNode  * aAggrResultNode );

    /* PROJ-1804 Ranking Function */
    static IDE_RC getPositionValue( qcTemplate  * aTemplate,
                                    qmdAggrNode * aAggrNode,
                                    SLong       * aNumber );
    
    /* BUG-40279 lead, lag with ignore nulls */
    static IDE_RC checkNullAggregation( qcTemplate   * aTemplate,
                                        qmdAggrNode  * aAggrNode,
                                        idBool       * aIsNull );

    /* BUG-40354 pushed rank */
    static IDE_RC getMinLimitValue( qcTemplate * aTemplate,
                                    qmdWndNode * aWndNode,
                                    SLong      * aNumber );

    /* BUG-43086 support Ntile analytic function */
    static IDE_RC getNtileValue( qcTemplate  * aTemplate,
                                 qmdAggrNode * aAggrNode,
                                 SLong       * aNumber );

    /* BUG-43086 support Ntile analytic function */
    static IDE_RC partitionOrderByNtileAggr( qcTemplate  * aTemplate,
                                             qmndWNST    * aDataPlan,
                                             qmdMtrNode  * aOverColumnNode,
                                             qmdAggrNode * aAggrNode,
                                             qmdMtrNode  * aAggrResultNode );

    /* BUG-43086 support Ntile analytic function */
    static IDE_RC orderByNtileAggr( qcTemplate  * aTemplate,
                                    qmndWNST    * aDataPlan,
                                    qmdAggrNode * aAggrNode,
                                    qmdMtrNode  * aAggrResultNode );
};

#endif /* _O_WMN_WNST_H_ */
