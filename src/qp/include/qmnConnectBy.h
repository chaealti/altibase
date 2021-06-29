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
 * $Id: qmnConnectBy.h 89364 2020-11-27 02:03:38Z donovan.seo $
 ***********************************************************************/

#ifndef _O_QMN_CONNECT_BY_H_
#define _O_QMN_COONECT_BY_H_ 1

#include <mt.h>
#include <qmc.h>
#include <qmnDef.h>
#include <qmcSortTempTable.h>
#include <qmcMemSortTempTable.h>

#define QMNC_CNBY_CHILD_VIEW_MASK          (0x00000001)
#define QMNC_CNBY_CHILD_VIEW_FALSE         (0x00000000)
#define QMNC_CNBY_CHILD_VIEW_TRUE          (0x00000001)

#define QMNC_CNBY_IGNORE_LOOP_MASK         (0x00000002)
#define QMNC_CNBY_IGNORE_LOOP_FALSE        (0x00000000)
#define QMNC_CNBY_IGNORE_LOOP_TRUE         (0x00000002)

#define QMNC_CNBY_FROM_DUAL_MASK           (0x00000004)
#define QMNC_CNBY_FROM_DUAL_FALSE          (0x00000000)
#define QMNC_CNBY_FROM_DUAL_TRUE           (0x00000004)

#define QMNC_CNBY_JOIN_MASK                (0x00000008)
#define QMNC_CNBY_JOIN_FALSE               (0x00000000)
#define QMNC_CNBY_JOIN_TRUE                (0x00000008)

#define QMNC_CNBY_SIBLINGS_MASK            (0x00000010)
#define QMNC_CNBY_SIBLINGS_FALSE           (0x00000000)
#define QMNC_CNBY_SIBLINGS_TRUE            (0x00000010)

/* BUG-48300 */
#define QMNC_CNBY_ISLEAF_MASK              (0x00000020)
#define QMNC_CNBY_ISLEAF_FALSE             (0x00000000)
#define QMNC_CNBY_ISLEAF_TRUE              (0x00000020)

/* BUG-48300 */
#define QMNC_CNBY_FUNCTION_MASK            (0x00000040)
#define QMNC_CNBY_FUNCTION_FALSE           (0x00000000)
#define QMNC_CNBY_FUNCTION_TRUE            (0x00000040)

/* BUG-48300 */
#define QMNC_CNBY_LEVEL_MASK               (0x00000080)
#define QMNC_CNBY_LEVEL_FALSE              (0x00000000)
#define QMNC_CNBY_LEVEL_TRUE               (0x00000080)

/* First Initialization Done */
#define QMND_CNBY_INIT_DONE_MASK           (0x00000001)
#define QMND_CNBY_INIT_DONE_FALSE          (0x00000000)
#define QMND_CNBY_INIT_DONE_TRUE           (0x00000001)

#define QMND_CNBY_ALL_FALSE_MASK           (0x00000002)
#define QMND_CNBY_ALL_FALSE_FALSE          (0x00000000)
#define QMND_CNBY_ALL_FALSE_TRUE           (0x00000002)

#define QMND_CNBY_USE_SORT_TEMP_MASK       (0x00000004)
#define QMND_CNBY_USE_SORT_TEMP_FALSE      (0x00000000)
#define QMND_CNBY_USE_SORT_TEMP_TRUE       (0x00000004)

#define QMND_CNBY_BLOCKSIZE 64

typedef struct qmnCNBYItem
{
    smiCursorPosInfo    pos;
    SLong               lastKey;
    SLong               level;
    void              * rowPtr;
    smiTableCursor    * mCursor;
    scGRID              mRid;
} qmnCNBYItem;

typedef struct qmnCNBYStack
{
    qmnCNBYItem    items[QMND_CNBY_BLOCKSIZE];
    UInt           nextPos;
    UInt           currentLevel;
    UShort         myRowID;
    UShort         baseRowID;
    qmcdSortTemp * baseMTR;
    qmnCNBYStack * prev;
    qmnCNBYStack * next;
} qmnCNBYStack;

typedef struct qmncCNBY
{
    qmnPlan         plan;
    UInt            flag;
    UInt            planID;

    UShort          myRowID;
    UShort          baseRowID;
    UShort          priorRowID;
    UShort          levelRowID;
    UShort          isLeafRowID;
    UShort          stackRowID;          /* Pseudo Column Stack Addr RowID */
    UShort          sortRowID;
    UShort          rownumRowID;

    qtcNode       * startWithConstant;   /* Start With Constant Filter */
    qtcNode       * startWithFilter;     /* Start With Filter */
    qtcNode       * startWithSubquery;   /* Start With Subquery Filter */
    qtcNode       * startWithNNF;        /* Start With NNF Filter */

    qtcNode       * connectByKeyRange;   /* Connect By Key Range */
    qtcNode       * connectByConstant;   /* Connect By Constant Filter */
    qtcNode       * connectByFilter;     /* Connect By Filter */
    qtcNode       * levelFilter;         /* Connect By Level Filter */
    qtcNode       * connectBySubquery;   /* Connect By Subquery Filter */
    qtcNode       * priorNode;           /* Prior Node */
    qtcNode       * rownumFilter;        /* Connect By Rownum Filter */

    qmcMtrNode    * sortNode;            /* Sort Temp Column */
    qmcMtrNode    * baseSortNode;        /* base Sort Temp Column */
    qmcMtrNode    * mBaseMtrNode;        /* base Sort Temp Column */

    /* PROJ-2641 Hierarchy query Index */
    const void    * mTableHandle;
    smSCN           mTableSCN;
    qcmIndex      * mIndex;

    /* PROJ-2641 Hierarchy query Index Predicate */
    /* Key Range */
    qtcNode       * mFixKeyRange;         /* Fixed Key Range */
    qtcNode       * mFixKeyRange4Print;   /* Printable Fixed Key Range */
    qtcNode       * mVarKeyRange;         /* Variable Key Range */
    qtcNode       * mVarKeyRange4Filter;  /* Variable Fixed Key Range */
    /* Key Filter */
    qtcNode       * mFixKeyFilter;        /* Fixed Key Filter */
    qtcNode       * mFixKeyFilter4Print;  /* PrintableFixed Key Filter */
    qtcNode       * mVarKeyFilter;        /* Variable Key Filter */
    qtcNode       * mVarKeyFilter4Filter; /* Variable Fixed Key Filter */

    UInt            mtrNodeOffset;       /* Mtr node Offset */
    UInt            baseSortOffset;
    UInt            mSortNodeOffset;
    UInt            sortMTROffset;       /* Sort Mgr Offset */
    UInt            mBaseSortMTROffset;
} qmncCNBY;

typedef struct qmndCNBY
{
    qmndPlan        plan;
    doItFunc        doIt;
    UInt          * flag;

    mtcTuple      * childTuple;
    mtcTuple      * priorTuple;
    mtcTuple      * sortTuple;

    SLong         * levelPtr;
    SLong           levelValue;
    SLong           levelDummy;

    SLong         * rownumPtr;
    SLong           rownumValue;

    SLong         * isLeafPtr;
    SLong           isLeafDummy;
    SLong         * stackPtr;

    void          * nullRow;
    scGRID          mNullRID;
    void          * prevPriorRow;
    scGRID          mPrevPriorRID;

    SLong           startWithPos;
    qmdMtrNode    * mBaseMtrNode;
    qmdMtrNode    * mtrNode;
    qmcdSortTemp  * baseMTR;
    qmcdSortTemp  * sortMTR;

    qmnCNBYStack  * firstStack;
    qmnCNBYStack  * lastStack;

    /*
     * PROJ-2641 Hierarchy query Index Predicate ����
     */
    smiRange      * mFixKeyRangeArea;  /* Fixed Key Range ���� */
    smiRange      * mFixKeyRange;      /* Fixed Key Range */
    UInt            mFixKeyRangeSize;  /* Fixed Key Range ũ�� */

    smiRange      * mFixKeyFilterArea; /* Fixed Key Filter ���� */
    smiRange      * mFixKeyFilter;     /* Fixed Key Filter */
    UInt            mFixKeyFilterSize; /* Fixed Key Filter ũ�� */

    smiRange      * mVarKeyRangeArea;  /* Variable Key Range ���� */
    smiRange      * mVarKeyRange;      /* Variable Key Range */
    UInt            mVarKeyRangeSize;  /* Variable Key Range ũ�� */

    smiRange      * mVarKeyFilterArea; /* Variable Key Filter ���� */
    smiRange      * mVarKeyFilter;     /* Variable Key Filter */
    UInt            mVarKeyFilterSize; /* Variable Key Filter ũ�� */

    smiRange      * mKeyRange;         /* ���� Key Range */
    smiRange      * mKeyFilter;        /* ���� Key Filter */

    smiCursorProperties   mCursorProperty;
    smiCallBack           mCallBack;
    qtcSmiCallBackDataAnd mCallBackDataAnd;
    qtcSmiCallBackData    mCallBackData[3]; /* �� ������ Filter�� ������. */
} qmndCNBY;

class qmnCNBY
{
public:
    static IDE_RC init( qcTemplate * aTemplate, qmnPlan * aPlan );

    static IDE_RC doIt( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag );

    static IDE_RC padNull( qcTemplate * aTemplate, qmnPlan * aPlan );

    static IDE_RC printPlan( qcTemplate    * aTemplate,
                             qmnPlan       * aPlan,
                             ULong           aDepth,
                             iduVarString  * aString,
                             qmnDisplay      aMode );

    static IDE_RC doItDefault( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag );

    static IDE_RC doItFirst( qcTemplate * aTemplate,
                             qmnPlan    * aPlan,
                             qmcRowFlag * aFlag );

    static IDE_RC doItNext( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag );

    static IDE_RC doItAllFalse( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    static IDE_RC doItFirstDual( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    static IDE_RC doItNextDual( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );

    static IDE_RC doItFirstTable( qcTemplate * aTemplate,
                                  qmnPlan    * aPlan,
                                  qmcRowFlag * aFlag );

    static IDE_RC doItNextTable( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    static IDE_RC doItFirstTableDisk( qcTemplate * aTemplate,
                                      qmnPlan    * aPlan,
                                      qmcRowFlag * aFlag );

    static IDE_RC doItNextTableDisk( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag );

    static IDE_RC doItFirstJoin( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 qmcRowFlag * aFlag );

    static IDE_RC doItNextJoin( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag );
private:

    static IDE_RC firstInit( qcTemplate * aTemplate,
                             qmncCNBY   * aCodePlan,
                             qmndCNBY   * aDataPlan );

    static IDE_RC firstInitForJoin( qcTemplate * aTemplate,
                                    qmncCNBY   * aCodePlan,
                                    qmndCNBY   * aDataPlan );
    /* CMTR child�� ���� */
    static IDE_RC execChild( qcTemplate * aTemplate,
                             qmncCNBY   * aCodePlan,
                             qmndCNBY   * aDataPlan );

    /* Null Row�� ȹ�� */
    static IDE_RC getNullRow( qcTemplate * aTemplate,
                              qmncCNBY   * aCodePlan,
                              qmndCNBY   * aDataPlan );

    static IDE_RC refineOffset( qcTemplate * aTemplate,
                                qmncCNBY   * aCodePlan,
                                mtcTuple   * aTuple );

    /* Sort Temp Table �ʱ�ȭ */
    static IDE_RC initSortTemp( qcTemplate * aTemplate,
                                qmndCNBY   * aDataPlan );

    /* Sort Temp Mtr Node �ʱ�ȭ */
    static IDE_RC initSortMtrNode( qcTemplate * aTemplate,
                                   qmncCNBY   * aCodePlan,
                                   qmcMtrNode * aCodeNode,
                                   qmdMtrNode * aMtrNode );

    /* Sort Temp Table ���� */
    static IDE_RC makeSortTemp( qcTemplate * aTemplate,
                                qmndCNBY   * aDataPlan );

    /* Sort Temp �� �׿��� Row�����͸� ���� */
    static IDE_RC restoreTupleSet( qcTemplate * aTemplate,
                                   qmdMtrNode * aMtrNode,
                                   void       * aRow );

    /* Item �ʱ�ȭ */
    static IDE_RC initCNBYItem( qmnCNBYItem * aItem );

    /* ���ÿ��� ���� Plan�� �÷��� item ���� */
    static IDE_RC setCurrentRow( qcTemplate * aTemplate,
                                 qmncCNBY   * aCodePlan,
                                 qmndCNBY   * aDataPlan,
                                 UInt         aPrev );

    /* Loop�� �߻� ���� üũ */
    static IDE_RC checkLoop( qmncCNBY * aCodePlan,
                             qmndCNBY * aDataPlan,
                             void     * aRow,
                             idBool   * aIsLoop );

    /* Loop�� �߻� ���� üũ */
    static IDE_RC checkLoopDisk( qmncCNBY * aCodePlan,
                                 qmndCNBY * aDataPlan,
                                 idBool   * aIsLoop );
    /* Stack Block �Ҵ� */
    static IDE_RC allocStackBlock( qcTemplate * aTemplate,
                                   qmndCNBY   * aDataPlan );

    /* ���ÿ� Item �߰� */
    static IDE_RC pushStack( qcTemplate  * aTemplate,
                             qmncCNBY    * aCodePlan,
                             qmndCNBY    * aDataPlan,
                             qmnCNBYItem * aItem );

    /* ���� ������ �ڷ� Search */
    static IDE_RC searchNextLevelData( qcTemplate  * aTemplate,
                                       qmncCNBY    * aCodePlan,
                                       qmndCNBY    * aDataPlan,
                                       qmnCNBYItem * aItem,
                                       idBool      * aExist );

    /* ���� ������ �ڷ� Search */
    static IDE_RC searchSiblingData( qcTemplate * aTemplate,
                                     qmncCNBY   * aCodePlan,
                                     qmndCNBY   * aDataPlan,
                                     idBool     * aBreak );

    /* baseMTR���� ���� �˻��� ���� */
    static IDE_RC searchSequnceRow( qcTemplate  * aTemplate,
                                    qmncCNBY    * aCodePlan,
                                    qmndCNBY    * aDataPlan,
                                    qmnCNBYItem * aOldItem,
                                    qmnCNBYItem * aNewItem );

    /* sortMTR���� KeyRange �˻��� ���� */
    static IDE_RC searchKeyRangeRow( qcTemplate  * aTemplate,
                                     qmncCNBY    * aCodePlan,
                                     qmndCNBY    * aDataPlan,
                                     qmnCNBYItem * aOldItem,
                                     qmnCNBYItem * aNewItem );

    /* baseMTR tuple ���� */
    static IDE_RC setBaseTupleSet( qcTemplate * aTemplate,
                                   qmndCNBY   * aDataPlan,
                                   const void * aRow );

    /* myTuple ���� */
    static IDE_RC copyTupleSet( qcTemplate * aTemplate,
                                qmncCNBY   * aCodePlan,
                                mtcTuple   * aDstTuple );

    static IDE_RC initForTable( qcTemplate * aTemplate,
                                qmncCNBY   * aCodePlan,
                                qmndCNBY   * aDataPlan );

    /* ���ÿ��� ���� Plan�� �÷��� item ���� */
    static IDE_RC setCurrentRowTable( qmndCNBY   * aDataPlan,
                                      UInt         aPrev );

    static IDE_RC pushStackTable( qcTemplate  * aTemplate,
                                  qmndCNBY    * aDataPlan,
                                  qmnCNBYItem * aItem );

    /* ���� ������ �ڷ� Search */
    static IDE_RC searchNextLevelDataTable( qcTemplate  * aTemplate,
                                            qmncCNBY    * aCodePlan,
                                            qmndCNBY    * aDataPlan,
                                            idBool      * aExist );

    static IDE_RC searchSequnceRowTable( qcTemplate  * aTemplate,
                                         qmncCNBY    * aCodePlan,
                                         qmndCNBY    * aDataPlan,
                                         qmnCNBYItem * aItem,
                                         idBool      * aIsExist );

    static IDE_RC makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                         qmncCNBY   * aCodePlan,
                                         qmndCNBY   * aDataPlan );

    static IDE_RC makeSortTempTable( qcTemplate  * aTemplate,
                                     qmncCNBY    * aCodePlan,
                                     qmndCNBY    * aDataPlan );

    static IDE_RC searchSiblingDataTable( qcTemplate * aTemplate,
                                          qmncCNBY   * aCodePlan,
                                          qmndCNBY   * aDataPlan,
                                          idBool     * aBreak );
    /* ���� ������ �ڷ� Search */
    static IDE_RC searchNextLevelDataSortTemp( qcTemplate  * aTemplate,
                                               qmncCNBY    * aCodePlan,
                                               qmndCNBY    * aDataPlan,
                                               idBool      * aExist );

    static IDE_RC searchKeyRangeRowTable( qcTemplate  * aTemplate,
                                          qmncCNBY    * aCodePlan,
                                          qmndCNBY    * aDataPlan,
                                          qmnCNBYItem * aItem,
                                          idBool        aIsNextLevel,
                                          idBool      * aIsExist );

    static IDE_RC setCurrentRowTableDisk( qmndCNBY   * aDataPlan,
                                          UInt         aPrev );

    static IDE_RC searchSequnceRowTableDisk( qcTemplate  * aTemplate,
                                             qmncCNBY    * aCodePlan,
                                             qmndCNBY    * aDataPlan,
                                             qmnCNBYItem * aItem,
                                             idBool      * aIsExist );

    static IDE_RC searchSiblingDataTableDisk( qcTemplate * aTemplate,
                                              qmncCNBY   * aCodePlan,
                                              qmndCNBY   * aDataPlan,
                                              idBool     * aBreak );

    static IDE_RC searchKeyRangeRowTableDisk( qcTemplate  * aTemplate,
                                              qmncCNBY    * aCodePlan,
                                              qmndCNBY    * aDataPlan,
                                              qmnCNBYItem * aItem,
                                              idBool        aIsNextLevel,
                                              idBool      * aIsExist );

    static IDE_RC searchNextLevelDataTableDisk( qcTemplate  * aTemplate,
                                                qmncCNBY    * aCodePlan,
                                                qmndCNBY    * aDataPlan,
                                                idBool      * aExist );

    static IDE_RC searchNextLevelDataSortTempDisk( qcTemplate  * aTemplate,
                                                   qmncCNBY    * aCodePlan,
                                                   qmndCNBY    * aDataPlan,
                                                   idBool      * aExist );

    static IDE_RC pushStackTableDisk( qcTemplate  * aTemplate,
                                      qmndCNBY    * aDataPlan,
                                      qmnCNBYItem * aItem );

    static IDE_RC searchNextLevelDataForJoin( qcTemplate  * aTemplate,
                                              qmncCNBY    * aCodePlan,
                                              qmndCNBY    * aDataPlan,
                                              qmnCNBYItem * aItem,
                                              idBool      * aExist );

    static IDE_RC searchSiblingDataForJoin( qcTemplate * aTemplate,
                                            qmncCNBY   * aCodePlan,
                                            qmndCNBY   * aDataPlan,
                                            idBool     * aBreak );

    static IDE_RC searchKeyRangeRowForJoin( qcTemplate  * aTemplate,
                                            qmncCNBY    * aCodePlan,
                                            qmndCNBY    * aDataPlan,
                                            qmnCNBYItem * aOldItem,
                                            qmnCNBYItem * aNewItem );
};

#endif /* _O_QMN_COONECT_BY_H_ */
