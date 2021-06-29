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
 * $Id: qmnConnectBy.cpp 89364 2020-11-27 02:03:38Z donovan.seo $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qmnConnectByMTR.h>
#include <qmnConnectBy.h>
#include <qmcSortTempTable.h>
#include <qcuProperty.h>
#include <qmoUtil.h>
#include <qcg.h>

typedef enum qmnConnectBystartWithFilter {
   START_WITH_FILTER = 0,
   START_WITH_SUBQUERY,
   START_WITH_NNF,
   START_WITH_MAX,
} qmnConnectBystartWithFilter;

/**
 * Sort Temp Table ����
 *
 *  BaseTable�� ���� Row �����͸� ��� Sort Temp Table�� �����Ѵ�.
 *  ���⼭ BaseTable�� HMTR�� ��� ���� ������ ���̺��̴�.
 */
IDE_RC qmnCNBY::makeSortTemp( qcTemplate * aTemplate,
                              qmndCNBY   * aDataPlan )
{
    qmdMtrNode * sNode      = NULL;
    void       * sSearchRow = NULL;
    SInt         sCount     = 0;
    UInt         sFlag      = 0;

    sFlag = QMCD_SORT_TMP_STORAGE_MEMORY;

    IDE_TEST( qmcSortTemp::init( aDataPlan->sortMTR,
                                 aTemplate,
                                 ID_UINT_MAX,
                                 aDataPlan->mtrNode,
                                 aDataPlan->mtrNode,
                                 0,
                                 sFlag)
              != IDE_SUCCESS );

    do
    {
        if ( sCount == 0 )
        {
            IDE_TEST( qmcSortTemp::getFirstSequence( aDataPlan->baseMTR,
                                                     &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->baseMTR,
                                                    &sSearchRow )
                      != IDE_SUCCESS );
        }

        if ( sSearchRow != NULL )
        {
            aDataPlan->childTuple->row = sSearchRow;
            IDE_TEST( setBaseTupleSet( aTemplate,
                                       aDataPlan,
                                       sSearchRow )
                      != IDE_SUCCESS );

            IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMTR,
                                          &aDataPlan->sortTuple->row )
                      != IDE_SUCCESS );

            for ( sNode = aDataPlan->mtrNode;
                  sNode != NULL;
                  sNode = sNode->next )
            {
                IDE_TEST( sNode->func.setMtr( aTemplate,
                                              sNode,
                                              aDataPlan->sortTuple->row )
                          != IDE_SUCCESS );
            }
            IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMTR,
                                           aDataPlan->sortTuple->row )
                      != IDE_SUCCESS );
            sCount++;
        }
        else
        {
            break;
        }
    } while ( sSearchRow != NULL );

    IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMTR )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Restore Tuple Set
 *
 *  Sort Temp���� ã�� Row �����Ϳ��� �ּ� ���� �о� ���� BaseTable�� RowPtr�� �����Ѵ�.
 */
IDE_RC qmnCNBY::restoreTupleSet( qcTemplate * aTemplate,
                                 qmdMtrNode * aMtrNode,
                                 void       * aRow )
{
    qmdMtrNode * sNode = NULL;

    for ( sNode = aMtrNode; sNode != NULL; sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate, sNode, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Initialize CNBYInfo
 *   CNBYInfo ����ü�� �ʱ�ȭ�Ѵ�.
 */
IDE_RC qmnCNBY::initCNBYItem( qmnCNBYItem * aItem )
{
    aItem->lastKey = 0;
    aItem->level   = 1;
    aItem->rowPtr  = NULL;
    aItem->mCursor = NULL;

    return IDE_SUCCESS;
}

/**
 * Set Current Row
 *
 *  �� ���� �ϳ��� Row�� ���� Plan���� �÷��ִµ� CONNECT_BY_ISLEAF ���� �˱� ���ؼ���
 *  ���� Row�� �о�� �� �ʿ䰡 �ִ�. ���� CNBYStack�� ���� ����� ���� ������ �÷���
 *  ���� �ٸ��� �ȴ�. �� �Լ��� ���ؼ� �� ��° �� ���� �÷����� �����Ѵ�.
 */
IDE_RC qmnCNBY::setCurrentRow( qcTemplate * aTemplate,
                               qmncCNBY   * aCodePlan,
                               qmndCNBY   * aDataPlan,
                               UInt         aPrev )
{
    qmnCNBYStack * sStack = NULL;
    mtcColumn    * sColumn;
    UInt           sIndex = 0;
    UInt           i;

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos >= aPrev )
    {
        sIndex = sStack->nextPos - aPrev;
    }
    else
    {
        sIndex = aPrev - sStack->nextPos;
        IDE_DASSERT( sIndex <= QMND_CNBY_BLOCKSIZE );
        sIndex = QMND_CNBY_BLOCKSIZE - sIndex;
        sStack = sStack->prev;
    }

    IDE_TEST_RAISE( sStack == NULL, ERR_INTERNAL )

    aDataPlan->plan.myTuple->row = sStack->items[sIndex].rowPtr;

    *aDataPlan->levelPtr    = sStack->items[sIndex].level;
    aDataPlan->firstStack->currentLevel = *aDataPlan->levelPtr;

    aDataPlan->prevPriorRow = aDataPlan->priorTuple->row;
    if ( sIndex > 0 )
    {
        aDataPlan->priorTuple->row = sStack->items[sIndex - 1].rowPtr;
        
        IDE_TEST( setBaseTupleSet( aTemplate,
                                   aDataPlan,
                                   aDataPlan->priorTuple->row )
                  != IDE_SUCCESS );
        
        IDE_TEST( copyTupleSet( aTemplate,
                                aCodePlan,
                                aDataPlan->priorTuple )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aDataPlan->firstStack->currentLevel >= QMND_CNBY_BLOCKSIZE )
        {
            sStack = sStack->prev;
            aDataPlan->priorTuple->row =
                    sStack->items[QMND_CNBY_BLOCKSIZE - 1].rowPtr;
            
            IDE_TEST( setBaseTupleSet( aTemplate,
                                       aDataPlan,
                                       aDataPlan->priorTuple->row )
                      != IDE_SUCCESS );
            
            IDE_TEST( copyTupleSet( aTemplate,
                                    aCodePlan,
                                    aDataPlan->priorTuple )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->priorTuple->row = aDataPlan->nullRow;

            // PROJ-2362 memory temp ���� ȿ���� ����
            sColumn = aDataPlan->priorTuple->columns;
            for ( i = 0; i < aDataPlan->priorTuple->columnCount; i++, sColumn++ )
            {
                if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
                {
                    sColumn->module->null( sColumn,
                                           sColumn->column.value );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }

    IDE_TEST( setBaseTupleSet( aTemplate,
                               aDataPlan,
                               aDataPlan->plan.myTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( copyTupleSet( aTemplate,
                            aCodePlan,
                            aDataPlan->plan.myTuple )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnCNBY::setCurrentRow",
                                  "The stack pointer is null" ));
    }

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Do It First
 *
 *  ���� ó�� ȣ��Ǵ� �Լ��� START WITH ó�� �� CONNECT BY ó��
 *
 *  START WITH�� BaseTable( HMTR ) ���� ������� ���ʷ� �о ������ ���ϸ� ó���ȴ�.
 *  1. START WITH�� Constant Filter�� Init�������� Flag�� ���� ���õǾ� ó���ȴ�.
 *  2. START WITH Filter �˻�.
 *  3. START WITH SubQuery Filter �˻�.
 *  4. START WITH ������ �����ϸ� ���� 1�� �ǹǷ� CNBYStack�� ���� ó���� �ִ´�.
 *  5. �� Row������ CONNECT BY CONSTANT FILTER �˻縦 �����Ѵ�.
 *  6. ���ǿ� �����ϸ� CONNECT_BY_ISLEAF�� �˱����� ���������� �ڷᰡ �ִ��� ã�ƺ���.
 *     �������� �ڷᰡ �ִٸ� doItNext�� ȣ���ϵ��� �Լ������͸� �������ش�.
 */
IDE_RC qmnCNBY::doItFirst( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
    SInt        sCount     = 0;
    idBool      sJudge     = ID_FALSE;
    idBool      sExist     = ID_FALSE;
    void      * sSearchRow = NULL;
    qmncCNBY  * sCodePlan  = (qmncCNBY *)aPlan;
    qmndCNBY  * sDataPlan  = (qmndCNBY *)( aTemplate->tmplate.data +
                                           aPlan->offset );
    qmnCNBYItem sReturnItem;
    qtcNode   * sFilters[START_WITH_MAX];
    SInt        i;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );
    IDE_TEST( initCNBYItem( &sReturnItem) != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_NONE;
    /* BUG-47820 start with ������ level pseudo column�� ���� ��� �������. */
    sDataPlan->levelValue = 0;
    *sDataPlan->levelPtr = 0;

    sFilters[START_WITH_FILTER]   = sCodePlan->startWithFilter;
    sFilters[START_WITH_SUBQUERY] = sCodePlan->startWithSubquery;
    sFilters[START_WITH_NNF]      = sCodePlan->startWithNNF;

    do
    {
        sJudge  = ID_FALSE;

        if ( sDataPlan->startWithPos == 0 )
        {
            IDE_TEST( qmcSortTemp::getFirstSequence( sDataPlan->baseMTR,
                                                     &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( ( sCount == 0 ) &&
                 ( sCodePlan->connectByKeyRange == NULL ) )
            {
                IDE_TEST( qmcSortTemp::restoreCursor( sDataPlan->baseMTR,
                                                      &sDataPlan->lastStack->items[0].pos )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( qmcSortTemp::getNextSequence( sDataPlan->baseMTR,
                                                    &sSearchRow )
                      != IDE_SUCCESS );
        }

        IDE_TEST_CONT( sSearchRow == NULL, NORMAL_EXIT );

        sDataPlan->plan.myTuple->row = sSearchRow;
        sDataPlan->priorTuple->row = sSearchRow;
        sDataPlan->startWithPos++;
        sCount++;

        IDE_TEST( setBaseTupleSet( aTemplate,
                                   sDataPlan,
                                   sDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( copyTupleSet( aTemplate,
                                sCodePlan,
                                sDataPlan->plan.myTuple )
                  != IDE_SUCCESS );

        IDE_TEST( copyTupleSet( aTemplate,
                                sCodePlan,
                                sDataPlan->priorTuple )
                  != IDE_SUCCESS );

        for ( i = 0 ; i < START_WITH_MAX; i ++ )
        {
            sJudge = ID_TRUE;
            if ( sFilters[i] != NULL )
            {
                if ( i == START_WITH_SUBQUERY )
                {
                    sDataPlan->plan.myTuple->modify++;
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( qtc::judge( &sJudge,
                                      sFilters[i],
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            if ( sJudge == ID_FALSE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
    } while ( sJudge == ID_FALSE );

    *aFlag = QMC_ROW_DATA_EXIST;
    sDataPlan->levelValue = 1;
    *sDataPlan->levelPtr = 1;

    /* 4. STORE STACK */
    IDE_TEST( qmcSortTemp::storeCursor( sDataPlan->baseMTR,
                                        &sDataPlan->lastStack->items[0].pos)
              != IDE_SUCCESS );

    IDE_TEST( initCNBYItem( &sDataPlan->lastStack->items[0])
              != IDE_SUCCESS );
    sDataPlan->lastStack->items[0].rowPtr = sSearchRow;
    sDataPlan->lastStack->nextPos = 1;

    /* 5. Check CONNECT BY CONSTANT FILTER */
    if ( sCodePlan->connectByConstant != NULL )
    {
        IDE_TEST( qtc::judge( &sJudge,
                              sCodePlan->connectByConstant,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    /* 6. Search Next Level Data */
    if ( sJudge == ID_TRUE )
    {
        IDE_TEST( searchNextLevelData( aTemplate,
                                       sCodePlan,
                                       sDataPlan,
                                       &sReturnItem,
                                       &sExist )
                  != IDE_SUCCESS );
        if ( sExist == ID_TRUE )
        {
            *sDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     2 )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnCNBY::doItNext;
        }
        else
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     1 )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnCNBY::doItFirst;
        }
    }
    else
    {
        *sDataPlan->isLeafPtr = 1;
        sDataPlan->doIt = qmnCNBY::doItFirst;
    }
    sDataPlan->plan.myTuple->modify++;

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItAllFalse( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
    qmndCNBY     * sDataPlan = NULL;

    sDataPlan       = (qmndCNBY *)(aTemplate->tmplate.data + aPlan->offset);

    /* 1. START WITH CONSTANT FILTER ALL FALSE or TRUE */
    IDE_DASSERT( ( *sDataPlan->flag & QMND_CNBY_ALL_FALSE_MASK ) ==
                 QMND_CNBY_ALL_FALSE_TRUE );

    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;
}

/**
 * Initialize
 *
 *  CNBY Plan�� �ʱ�ȭ�� �����Ѵ�.
 *
 *  1. �ʱ�ȭ ������ START WITH CONSTANT FILTER�� �˻��ϸ� Flag�� �������ش�.
 */
IDE_RC qmnCNBY::init( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
    idBool         sJudge    = ID_FALSE;
    qmncCNBY     * sCodePlan = NULL;
    qmndCNBY     * sDataPlan = NULL;
    qmnCNBYStack * sStack = NULL;

    sCodePlan         = (qmncCNBY *)aPlan;
    sDataPlan         = (qmndCNBY *)(aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag   = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt   = qmnCNBY::doItDefault;

    /* First initialization */
    if ( (*sDataPlan->flag & QMND_CNBY_INIT_DONE_MASK)
         == QMND_CNBY_INIT_DONE_FALSE )
    {
        if ( ( sCodePlan->flag & QMNC_CNBY_JOIN_MASK )
             == QMNC_CNBY_JOIN_FALSE )
        {
            IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( firstInitForJoin( aTemplate, sCodePlan, sDataPlan ) != IDE_SUCCESS );
        }
    }
    else
    {
        sDataPlan->startWithPos = 0;
        for ( sStack = sDataPlan->firstStack;
              sStack != NULL;
              sStack = sStack->next )
        {
            sStack->nextPos = 0;
        }

        if ( ( sCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
             == QMNC_CNBY_CHILD_VIEW_FALSE )
        {
            IDE_TEST( sCodePlan->plan.left->init( aTemplate,
                                                  sCodePlan->plan.left )
                      != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }
    }

    /* 1. START WITH CONSTANT FILTER */
    if ( sCodePlan->startWithConstant != NULL )
    {
        IDE_TEST( qtc::judge( & sJudge,
                              sCodePlan->startWithConstant,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    if ( sJudge == ID_TRUE )
    {
        *sDataPlan->flag &= ~QMND_CNBY_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_CNBY_ALL_FALSE_FALSE;

        if ( ( sCodePlan->flag & QMNC_CNBY_JOIN_MASK )
             == QMNC_CNBY_JOIN_FALSE )
        {
            if ( ( sCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
                 == QMNC_CNBY_CHILD_VIEW_TRUE )
            {
                sDataPlan->doIt = qmnCNBY::doItFirst;
            }
            else
            {
                if ( ( sCodePlan->flag & QMNC_CNBY_FROM_DUAL_MASK )
                       == QMNC_CNBY_FROM_DUAL_TRUE )
                {
                    sDataPlan->doIt = qmnCNBY::doItFirstDual;
                }
                else
                {
                    if ( ( sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
                           == QMN_PLAN_STORAGE_DISK )
                    {
                        sDataPlan->doIt = qmnCNBY::doItFirstTableDisk;
                    }
                    else
                    {
                        sDataPlan->doIt = qmnCNBY::doItFirstTable;
                    }
                }
            }
        }
        else
        {
            if ( ( ( sCodePlan->flag & QMNC_CNBY_SIBLINGS_MASK )
                   == QMNC_CNBY_SIBLINGS_FALSE ) &&
                 ( sCodePlan->connectByKeyRange != NULL ) )
            {
                sDataPlan->doIt = qmnCNBY::doItFirstJoin;
            }
            else
            {
                sDataPlan->doIt = qmnCNBY::doItFirst;
            }
        }
    }
    else
    {
        *sDataPlan->flag &= ~QMND_CNBY_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_CNBY_ALL_FALSE_TRUE;
        sDataPlan->doIt = qmnCNBY::doItAllFalse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Do It
 *  Do It �� ��Ȳ�� ���� doItFirst�� doItNext�� ȣ��ȴ�.
 */
IDE_RC qmnCNBY::doIt( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * aFlag )
{
    qmndCNBY * sDataPlan =
        (qmndCNBY *) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( sDataPlan->doIt( aTemplate, aPlan, aFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmnCNBY::padNull( qcTemplate * aTemplate,
                         qmnPlan    * aPlan )
{
    qmncCNBY  * sCodePlan = NULL;
    qmndCNBY  * sDataPlan = NULL;
    mtcColumn * sColumn;
    UInt        i;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)(aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_CNBY_INIT_DONE_MASK)
         == QMND_CNBY_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( sCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK  )
          == QMNC_CNBY_CHILD_VIEW_TRUE )
    {
        sDataPlan->plan.myTuple->row = sDataPlan->nullRow;

        // PROJ-2362 memory temp ���� ȿ���� ����
        sColumn = sDataPlan->plan.myTuple->columns;
        for ( i = 0; i < sDataPlan->plan.myTuple->columnCount; i++, sColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
            {
                sColumn->module->null( sColumn,
                                       sColumn->column.value );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
                  != IDE_SUCCESS );

        if ( ( sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
               == QMN_PLAN_STORAGE_DISK )
        {
            idlOS::memcpy( sDataPlan->priorTuple->row,
                           sDataPlan->nullRow, sDataPlan->priorTuple->rowOffset );
            SC_COPY_GRID( sDataPlan->mNullRID, sDataPlan->priorTuple->rid );
        }
        else
        {
            sDataPlan->priorTuple->row = sDataPlan->nullRow;
        }
    }

    // Null Padding�� record�� ���� ����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItDefault( qcTemplate * /* aTemplate */,
                             qmnPlan    * /* aPlan */,
                             qmcRowFlag * /* aFlag */ )
{
    return IDE_FAILURE;
}

/**
 * Check HIER LOOP
 *
 *  ������ �˻��Ͽ� ���� Row�� �ִ� ���� �Ǻ��Ѵ�.
 *  IGNORE LOOP�� NOCYCLE Ű���尡 ���ٸ� ������ ��������.
 *
 *  connect by level < 10 �� ���� ������ ��� ��
 *  connectByFilter �� NULL�ε� levelFilter�� �����Ұ�� ���� �˻縦 ���� �ʴ´�.
 */
IDE_RC qmnCNBY::checkLoop( qmncCNBY * aCodePlan,
                           qmndCNBY * aDataPlan,
                           void     * aRow,
                           idBool   * aIsLoop )
{
    qmnCNBYStack * sStack = NULL;
    idBool          sLoop  = ID_FALSE;
    UInt            i      = 0;
    void          * sRow   = NULL;

    IDE_TEST_CONT( (aCodePlan->connectByFilter == NULL) &&
                   (aCodePlan->levelFilter != NULL) &&
                   (((aDataPlan->mKeyRange == smiGetDefaultKeyRange()) ||
                     (aDataPlan->mKeyRange == NULL))),
                   NORMAL_EXIT );

    IDE_TEST_CONT( (aCodePlan->connectByFilter == NULL) &&
                   (aCodePlan->rownumFilter != NULL) &&
                   (((aDataPlan->mKeyRange == smiGetDefaultKeyRange()) ||
                     (aDataPlan->mKeyRange == NULL))),
                   NORMAL_EXIT );

    *aIsLoop = ID_FALSE;
    for ( sStack = aDataPlan->firstStack;
          sStack != NULL;
          sStack = sStack->next )
    {
        for ( i = 0; i < sStack->nextPos; i ++ )
        {
            sRow = sStack->items[i].rowPtr;

            if ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
                 == QMNC_CNBY_IGNORE_LOOP_TRUE )
            {
                if ( sRow == aRow )
                {
                    sLoop = ID_TRUE;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST_RAISE( sRow == aRow, err_loop_detected );
            }
        }

        if ( sLoop == ID_TRUE )
        {
            *aIsLoop = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_loop_detected )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_HIER_LOOP ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::checkLoopDisk( qmncCNBY * aCodePlan,
                               qmndCNBY * aDataPlan,
                               idBool   * aIsLoop )
{
    qmnCNBYStack * sStack = NULL;
    idBool          sLoop = ID_FALSE;
    UInt            i     = 0;

    IDE_TEST_CONT( (aCodePlan->connectByFilter == NULL) &&
                   (aCodePlan->levelFilter != NULL) &&
                   (((aDataPlan->mKeyRange == smiGetDefaultKeyRange()) ||
                     (aDataPlan->mKeyRange == NULL))),
                   NORMAL_EXIT );

    IDE_TEST_CONT( (aCodePlan->connectByFilter == NULL) &&
                   (aCodePlan->rownumFilter != NULL) &&
                   (((aDataPlan->mKeyRange == smiGetDefaultKeyRange()) ||
                     (aDataPlan->mKeyRange == NULL))),
                   NORMAL_EXIT );

    *aIsLoop = ID_FALSE;
    for ( sStack = aDataPlan->firstStack;
          sStack != NULL;
          sStack = sStack->next )
    {
        for ( i = 0; i < sStack->nextPos; i++ )
        {
            if ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
                 == QMNC_CNBY_IGNORE_LOOP_TRUE )
            {
                if ( SC_GRID_IS_EQUAL( sStack->items[i].mRid,
                                       aDataPlan->plan.myTuple->rid ) )
                {
                    sLoop = ID_TRUE;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST_RAISE( SC_GRID_IS_EQUAL( sStack->items[i].mRid,
                                                  aDataPlan->plan.myTuple->rid ),
                                err_loop_detected );
            }
        }

        if ( sLoop == ID_TRUE )
        {
            *aIsLoop = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_loop_detected )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_HIER_LOOP ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Search Sequence Row
 *
 *  Connect By �������� KeyRange�� ���� ��� baseMTR�� ���� ���ʷ� ã�´�.
 *  1. aHier���ڰ� ���� ���� ���� ������ ã�� ��� �̹Ƿ� ó������ ã�´�.
 *     aHier���ڰ� �ִٸ� �������� Resotre�ϰ� ���� ���� �д´�.
 *  2. ã�� ���� PriorTuple�� ���� ���ٸ� ���� ���� �д´�.
 *      1)Prior row�� ã�� Row�� ���� �� Skip ���� ������ �׻� Loop�� �߻��ϰ� �ȴ�.
 *      2)����Ŭ�� ��� constant filter������ �̷���� ��쿡�� Ư���� loop üũ�� ���ؼ�
 *        loop�� ������ ������ ctrl+c�� ���� �� �ִ�. ������ ��Ƽ���̽��� �̷��� �ɰ��
 *        ������ �׿��߸� �ϴ� ��Ȳ�� ������. ���� �⺻������ contant filter������
 *        �̷���� ��쿡�� loop üũ�� ���ش�.
 *      3) ������ ����ϴ� ���ϳ��� ���ܴ� connect by level < 10 �� ���� ������ �ܵ�����
 *        ������ ��쿡 ������ ����Ѵ�. ���� prior row�� search row �� ���� ��쵵
 *        skip���� �ʴ´�. ���� constantfilter != NULl �� prior �� search row��
 *        ���� ��쿡�� skip �� �ϰԵȴ�.
 *  3. Filter�� �˻��ؼ� �ùٸ� ����ġ üũ�Ѵ�.
 *  4. �ߺ��� �������� �˻��Ѵ�.
 *  5. �ùٸ� ���� ���ö� ���� ���� Row�� �д´�.
 */
IDE_RC qmnCNBY::searchSequnceRow( qcTemplate  * aTemplate,
                                  qmncCNBY    * aCodePlan,
                                  qmndCNBY    * aDataPlan,
                                  qmnCNBYItem * aOldItem,
                                  qmnCNBYItem * aNewItem )
{
    void      * sSearchRow = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sIsAllSame = ID_TRUE;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;

    aNewItem->rowPtr = NULL;

    /* 1. Get SearchRow */
    if ( aOldItem == NULL )
    {
        IDE_TEST( qmcSortTemp::getFirstSequence( aDataPlan->baseMTR,
                                                 &sSearchRow )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->baseMTR,
                                              &aOldItem->pos )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->baseMTR,
                                                &sSearchRow )
                  != IDE_SUCCESS );
    }

    while ( sSearchRow != NULL )
    {
        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        aDataPlan->plan.myTuple->row = sSearchRow;

        IDE_TEST( setBaseTupleSet( aTemplate,
                                   aDataPlan,
                                   aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( copyTupleSet( aTemplate,
                                aCodePlan,
                                aDataPlan->plan.myTuple )
                  != IDE_SUCCESS );

        /* 2. Get Next Row */
        if ( ( sSearchRow == aDataPlan->priorTuple->row ) &&
             ( aCodePlan->connectByConstant != NULL ) )
        {
            IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->baseMTR,
                                                    &sSearchRow )
                      != IDE_SUCCESS );
            aDataPlan->plan.myTuple->row = sSearchRow;

            IDE_TEST( setBaseTupleSet( aTemplate,
                                       aDataPlan,
                                       aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( copyTupleSet( aTemplate,
                                    aCodePlan,
                                    aDataPlan->plan.myTuple )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( sSearchRow == NULL )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        /* 3. Judge ConnectBy Filter */
        sJudge = ID_FALSE;
        if ( aCodePlan->connectByFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectByFilter, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        /* BUG-39401 need subquery for connect by clause */
        if ( ( aCodePlan->connectBySubquery != NULL ) &&
             ( sJudge == ID_TRUE ) )
        {
            aDataPlan->plan.myTuple->modify++;
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectBySubquery, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
                 == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            if ( ( aCodePlan->flag & QMNC_CNBY_JOIN_MASK )
                 == QMNC_CNBY_JOIN_FALSE )
            {
                /* 5. Compare Prior Row and Search Row Column */
                IDE_TEST( qmnCMTR::comparePriorNode( aTemplate,
                                                     aCodePlan->plan.left,
                                                     aDataPlan->priorTuple->row,
                                                     aDataPlan->plan.myTuple->row,
                                                     &sIsAllSame)
                           != IDE_SUCCESS );
            }
            else
            {
                for ( sNode = aCodePlan->priorNode;
                      sNode != NULL;
                      sNode = (qtcNode *)sNode->node.next )
                {
                    sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                    sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                    sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                    sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                    sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );
                    if ( sRow1Size == sRow2Size )
                    {
                        if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                             == 0 )
                        {
                            /* Nothing to do */
                        }
                        else
                        {
                            sIsAllSame = ID_FALSE;
                            break;
                        }
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
            }

            if ( sIsAllSame == ID_TRUE )
            {
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sJudge == ID_TRUE )
        {
            /* 4. Check Hier Loop */
            IDE_TEST( checkLoop( aCodePlan,
                                 aDataPlan,
                                 aDataPlan->plan.myTuple->row,
                                 &sLoop )
                      != IDE_SUCCESS );
            if ( sLoop == ID_FALSE )
            {
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->baseMTR,
                                                    &aNewItem->pos )
                          != IDE_SUCCESS );
                aNewItem->rowPtr  = sSearchRow;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        /* 5. Get Next Row */
        IDE_TEST( qmcSortTemp::getNextSequence( aDataPlan->baseMTR,
                                                &sSearchRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Search Key Range Row
 *
 *   ������ ������ ������ Key Range�� ������ Row�� Search �ϴ� �Լ�.
 *   1. aHier �μ��� ���ٸ� Key Range�� �����ϰ� ���� ó�� Row�� �����´�.
 *   2. aHier �μ��� �ִٸ� Last Key �� �����ϰ� ���� ����Ÿ�� ���������� �̵� �� ����
 *      Row�� �����´�.
 *   3. ������ SearchRow�� sortMTR�� �������̹Ƿ� �̸� baseMTR�� SearchRow�� �������ش�.
 *   4. Connect By Filter�� ������ �´��� �Ǵ��� ����.
 *   5. Prior column1 = column2 ���� Prior�� Row �������� column1�� SearchRow�� column1
 *      �� ���� �� ���� ���� ���ٸ� Loop�� �ִٰ� �Ǵ��ѵڿ� ���� NextRow�� �д´�.
 *   6. SearchRow�� Loop�� �ִ��� Ȯ���� ����.
 *   7. Loop�� ���ٸ� Position, Last Key, Row Pointer  �����Ѵ�.
 *   8. Filter�� ���� �ɷ����ų� Loop�� �ִٰ� �ǴܵǸ� ���� ���� �е��� �Ѵ�.
 */
IDE_RC qmnCNBY::searchKeyRangeRow( qcTemplate  * aTemplate,
                                   qmncCNBY    * aCodePlan,
                                   qmndCNBY    * aDataPlan,
                                   qmnCNBYItem * aOldItem,
                                   qmnCNBYItem * aNewItem )
{
    void      * sSearchRow = NULL;
    void      * sPrevRow   = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sNextRow   = ID_FALSE;
    idBool      sIsAllSame = ID_FALSE;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;

    aNewItem->rowPtr = NULL;

    if ( aOldItem == NULL )
    {
        /* 1. Get First Range Row */
        IDE_TEST( qmcSortTemp::getFirstRange( aDataPlan->sortMTR,
                                              aCodePlan->connectByKeyRange,
                                              &sSearchRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. Restore Position and Get Next Row */
        IDE_TEST( qmcSortTemp::setLastKey( aDataPlan->sortMTR,
                                           aOldItem->lastKey )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMTR,
                                              &aOldItem->pos )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                             &sSearchRow )
                  != IDE_SUCCESS );
    }

    while ( sSearchRow != NULL )
    {
        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        sPrevRow = aDataPlan->plan.myTuple->row;

        /* 3. Restore baseMTR Row Pointer */
        IDE_TEST( restoreTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   sSearchRow )
                  != IDE_SUCCESS );
        aDataPlan->plan.myTuple->row = aDataPlan->mtrNode->srcTuple->row;

        IDE_TEST( setBaseTupleSet( aTemplate,
                                   aDataPlan,
                                   aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( copyTupleSet( aTemplate,
                                aCodePlan,
                                aDataPlan->plan.myTuple )
                  != IDE_SUCCESS );

        /* 4. Judge Connect By Filter */
        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->connectByFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        if ( sJudge == ID_FALSE )
        {
            sNextRow = ID_TRUE;
        }
        else
        {
            sNextRow = ID_FALSE;
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            if ( ( aCodePlan->flag & QMNC_CNBY_JOIN_MASK )
                 == QMNC_CNBY_JOIN_FALSE )
            {
                /* 5. Compare Prior Row and Search Row Column */
                IDE_TEST( qmnCMTR::comparePriorNode( aTemplate,
                                                     aCodePlan->plan.left,
                                                     aDataPlan->priorTuple->row,
                                                     aDataPlan->plan.myTuple->row,
                                                     &sIsAllSame )
                          != IDE_SUCCESS );
            }
            else
            {
                for ( sNode = aCodePlan->priorNode;
                      sNode != NULL;
                      sNode = (qtcNode *)sNode->node.next )
                {
                    sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                    sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                    sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                    sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                    sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );
                    if ( sRow1Size == sRow2Size )
                    {
                        if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                             == 0 )
                        {
                            /* Nothing to do */
                        }
                        else
                        {
                            sIsAllSame = ID_FALSE;
                            break;
                        }
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
            }

            if ( sIsAllSame == ID_TRUE )
            {
                sNextRow = ID_TRUE;
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sJudge == ID_TRUE )
        {
            /* 6. Check HIER LOOP */
            IDE_TEST( checkLoop( aCodePlan,
                                 aDataPlan,
                                 aDataPlan->plan.myTuple->row,
                                 &sLoop )
                      != IDE_SUCCESS );

            /* 7. Store Position and lastKey and Row Pointer */
            if ( sLoop == ID_FALSE )
            {
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMTR,
                                                    &aNewItem->pos )
                          != IDE_SUCCESS );
                IDE_TEST( qmcSortTemp::getLastKey( aDataPlan->sortMTR,
                                                   &aNewItem->lastKey )
                          != IDE_SUCCESS );
                aNewItem->rowPtr = aDataPlan->plan.myTuple->row;
            }
            else
            {
                sNextRow = ID_TRUE;
            }
        }
        else
        {
            aDataPlan->plan.myTuple->row = sPrevRow;

            IDE_TEST( setBaseTupleSet( aTemplate,
                                       aDataPlan,
                                       aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );

            IDE_TEST( copyTupleSet( aTemplate,
                                    aCodePlan,
                                    aDataPlan->plan.myTuple )
                      != IDE_SUCCESS );
        }

        /* 8. Search Next Row Pointer */
        if ( sNextRow == ID_TRUE )
        {
            IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                                 &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sSearchRow = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Allocate Stack Block
 *
 *   CNBYStack Black�� ���� �Ҵ��Ѵ�.
 */
IDE_RC qmnCNBY::allocStackBlock( qcTemplate * aTemplate,
                                 qmndCNBY   * aDataPlan )
{
    qmnCNBYStack * sStack = NULL;

    IDU_FIT_POINT( "qmnCNBY::allocStackBlock::cralloc::sStack",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( aTemplate->stmt->qmxMem->cralloc( ID_SIZEOF( qmnCNBYStack ),
                                                (void **)&sStack )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sStack == NULL, err_mem_alloc );

    aDataPlan->lastStack->next        = sStack;
    sStack->prev                      = aDataPlan->lastStack;
    sStack->nextPos                   = 0;
    aDataPlan->lastStack              = sStack;
    aDataPlan->lastStack->myRowID     = aDataPlan->firstStack->myRowID;
    aDataPlan->lastStack->baseRowID   = aDataPlan->firstStack->baseRowID;
    aDataPlan->lastStack->baseMTR     = aDataPlan->firstStack->baseMTR;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_mem_alloc )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ));
    }

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Push Stack
 *
 *   1. Stack�� ã�� Row�� nextPos��ġ�� �����Ѵ�.
 *   2. ���� ������ ã�� �� �ֵ��� priorTuple�� ã�� Row �����͸� �����Ѵ�.
 *   3. nextPos�� ��ġ�� �������� ���´�.
 */
IDE_RC qmnCNBY::pushStack( qcTemplate  * aTemplate,
                           qmncCNBY    * aCodePlan,
                           qmndCNBY    * aDataPlan,
                           qmnCNBYItem * aItem )
{
    qmnCNBYStack * sStack = NULL;

    sStack = aDataPlan->lastStack;

    sStack->items[sStack->nextPos] = *aItem;

    aDataPlan->priorTuple->row = aItem->rowPtr;

    IDE_TEST( setBaseTupleSet( aTemplate,
                               aDataPlan,
                               aDataPlan->priorTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( copyTupleSet( aTemplate,
                            aCodePlan,
                            aDataPlan->priorTuple )
              != IDE_SUCCESS );

    ++sStack->nextPos;

    if ( sStack->nextPos >= QMND_CNBY_BLOCKSIZE )
    {
        if ( sStack->next == NULL )
        {
            IDE_TEST( allocStackBlock( aTemplate, aDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->lastStack          = sStack->next;
            aDataPlan->lastStack->nextPos = 0;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Search Next Level Data
 *  This funcion search data using key range or filter.
 *  1. Level Filter �˻縦 �����Ѵ�.
 *  2. Key Ragne�� ������ ���� Key Range Search�� Sequence Search �� �Ѵ�.
 *  3. �����Ͱ� �ִٸ� ���ÿ� �����͸� �ִ´�.
 */
IDE_RC qmnCNBY::searchNextLevelData( qcTemplate  * aTemplate,
                                     qmncCNBY    * aCodePlan,
                                     qmndCNBY    * aDataPlan,
                                     qmnCNBYItem * aItem,
                                     idBool      * aExist )
{
    idBool sCheck = ID_FALSE;
    idBool sExist = ID_FALSE;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    if ( aCodePlan->connectByKeyRange != NULL )
    {
        /* 2. Search Key Range Row */
        IDE_TEST( searchKeyRangeRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     NULL,
                                     aItem)
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. Search Sequence Row */
        IDE_TEST( searchSequnceRow( aTemplate,
                                    aCodePlan,
                                    aDataPlan,
                                    NULL,
                                    aItem)
                  != IDE_SUCCESS );
    }

    /* 3. Push Stack */
    if ( aItem->rowPtr != NULL )
    {
        aItem->level = ++aDataPlan->levelValue;
        IDE_TEST( pushStack( aTemplate,
                             aCodePlan,
                             aDataPlan,
                             aItem )
                  != IDE_SUCCESS );
        sExist = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Search Sibling Data
 *  ���� ������ �����͸� �߰��Ҽ� ������ ���� ������ �����͸� ã�´�.
 *  Prior Row�� ���� �����ͷ� �Űܳ��� Stack�� nextPos�� �Űܳ��ƾ��Ѵ�.
 *   1. nextPos�� �׻� ���� ���� ������ ����Ű�µ� �̸� �������� �Űܳ��´�.
 *      nextPos�� 0 �ΰ��� �ٸ� ���� ���� �ִٴ� ���̴�. ó�� ���۽ÿ��� 1
 *   2. Prior Row�� nextPos�� ���� ���� �ʿ��ϴ�.
 *   3. �� ���� ���� �ٸ� Key Range�� �����Ǿ� ������ ���� CNBYInfo�� �ʿ��ϴ�.
 *   4. ���� CNBYInfo�� ���ڷ� �־ Search�� �ϸ� ���� ���� �����͸� ��Եȴ�.
 *   5. �����Ͱ� �ִٸ� �̸� Stack�� �ְ� isLeaf�� ���� �ٽ� ���� ���� �����͸� ã�´�.
 *   6. �����Ͱ� ���ٸ� ���� isLeaf�� ������ ���� ���� 0�̶�� �̸� 1�� �ٲٰ�
 *      setCurrentRow �� ���� Stack�� �����Ѵ�. ���� �ƴ϶�� ���� ��带 ã���� �Ѵ�.
 */
IDE_RC qmnCNBY::searchSiblingData( qcTemplate * aTemplate,
                                   qmncCNBY   * aCodePlan,
                                   qmndCNBY   * aDataPlan,
                                   idBool     * aBreak )
{
    qmnCNBYStack  * sStack      = NULL;
    qmnCNBYStack  * sPriorStack = NULL;
    qmnCNBYItem   * sItem       = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sCheck      = ID_FALSE;
    UInt            sPriorPos   = 0;
    qmnCNBYItem     sNewItem;

    IDE_TEST( initCNBYItem( &sNewItem) != IDE_SUCCESS );

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos == 0 )
    {
        sStack = sStack->prev;
        aDataPlan->lastStack = sStack;
    }
    else
    {
        /* Nothing to do */
    }

    /* 1. nextPos�� �׻� ���� Stack�� ����Ű�Ƿ� �̸� ���� ���ѳ��´�. */
    --sStack->nextPos;

    if ( sStack->nextPos <= 0 )
    {
        sPriorStack = sStack->prev;
    }
    else
    {
        sPriorStack = sStack;
    }

    --aDataPlan->levelValue;

    /* 2. Set Prior Row  */
    sPriorPos = sPriorStack->nextPos - 1;
    aDataPlan->priorTuple->row = sPriorStack->items[sPriorPos].rowPtr;

    IDE_TEST( setBaseTupleSet( aTemplate,
                               aDataPlan,
                               aDataPlan->priorTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( copyTupleSet( aTemplate,
                            aCodePlan,
                            aDataPlan->priorTuple )
              != IDE_SUCCESS );

    /* 3. Set Current CNBY Info */
    sItem = &sStack->items[sStack->nextPos];

    /* 4. Search Siblings Data */
    if ( aCodePlan->connectByKeyRange != NULL )
    {
        IDE_TEST( searchKeyRangeRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     sItem,
                                     &sNewItem )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( searchSequnceRow( aTemplate,
                                    aCodePlan,
                                    aDataPlan,
                                    sItem,
                                    &sNewItem)
                  != IDE_SUCCESS );
    }

    /* 5. Push Siblings Data and Search Next level */
    if ( sNewItem.rowPtr != NULL )
    {

        /* BUG-39434 The connect by need rownum pseudo column.
         * Siblings �� ã�� ���� ���ÿ� �ֱ� ���� rownum�� �˻��ؾ��Ѵ�.
         */
        if ( aCodePlan->rownumFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sCheck,
                                  aCodePlan->rownumFilter,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sCheck = ID_TRUE;
        }

        if ( sCheck == ID_TRUE )
        {
            sNewItem.level = ++aDataPlan->levelValue;
            IDE_TEST( pushStack( aTemplate,
                                 aCodePlan,
                                 aDataPlan,
                                 &sNewItem )
                      != IDE_SUCCESS );
            IDE_TEST( searchNextLevelData( aTemplate,
                                           aCodePlan,
                                           aDataPlan,
                                           &sNewItem,
                                           &sIsRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        if ( sIsRow == ID_TRUE )
        {
            *aDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     2 )
                      != IDE_SUCCESS );
        }
        else
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     1 )
                      != IDE_SUCCESS );
        }
        *aBreak = ID_TRUE;
    }
    else
    {
        /* 6. isLeafPtr set 1 */
        if ( *aDataPlan->isLeafPtr == 0 )
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     0 )
                      != IDE_SUCCESS );
            *aBreak = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * doItNext
 *
 *   CONNECT_BY_ISLEAF�� ���� �˱����� ���� ���ܿ� ���� ��尡 �ִ����� ����.
 *   1. doItNext������ �׻� Data�� Exist �����ϵ��� flag ������ �Ѵ�.
 *      doItFirst���� ����Ÿ�� ���� ���θ� ������. �����Ѵ�.
 *   2. PriorTuple�� Row�����͸� ������ ã�� Row �����ͷ� �ٲ��ش�.
 *   3. ������ Leaf ��尡 �ƴ϶�� NextLevel�� ã�´�.
 *   4. ���� ��带 ãġ ���� ��� isLeaf�� 0�̶�� Leaf�� 1 ǥ���ѵڿ� ���� ��带 �ٷ�
 *      �� ���� �������ش�.
 *   5. ������ ��� �� isLeaf�� 1�ε��� �����Ͱ� ���°���� ���� ��带 ã���ش�
 *   6. ���� 1���� ������ �Ž��� �ö� ���� ���� �����Ͱ� ���ٸ� doItFirst�� ���� ����
 *      Start�� �����Ѵ�.
 *   7. isLeafPtr�� 0 �̰� �����Ͱ� �����Ƿ� ���� ���� ���� ������ �������ش�.
 */
IDE_RC qmnCNBY::doItNext( qcTemplate * aTemplate,
                          qmnPlan    * aPlan,
                          qmcRowFlag * aFlag )
{
    qmncCNBY      * sCodePlan   = NULL;
    qmndCNBY      * sDataPlan   = NULL;
    qmnCNBYStack  * sStack      = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sBreak      = ID_FALSE;
    qmnCNBYItem     sNewItem;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)( aTemplate->tmplate.data + aPlan->offset );
    sStack    = sDataPlan->lastStack;

    /* 1. Set qmcRowFlag */
    *aFlag = QMC_ROW_DATA_EXIST;
    IDE_TEST( initCNBYItem( &sNewItem) != IDE_SUCCESS );

    /* 2. Set PriorTupe Row Previous */
    sDataPlan->priorTuple->row = sDataPlan->prevPriorRow;

    IDE_TEST( setBaseTupleSet( aTemplate,
                               sDataPlan,
                               sDataPlan->priorTuple->row )
              != IDE_SUCCESS );

    IDE_TEST( copyTupleSet( aTemplate,
                            sCodePlan,
                            sDataPlan->priorTuple )
              != IDE_SUCCESS );

    /* 3. Search Next Level Data */
    if ( *sDataPlan->isLeafPtr == 0 )
    {
        IDE_TEST( searchNextLevelData( aTemplate,
                                       sCodePlan,
                                       sDataPlan,
                                       &sNewItem,
                                       &sIsRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsRow == ID_FALSE )
    {
        /* 4. Set isLeafPtr and Set CurrentRow Previous */
        if ( *sDataPlan->isLeafPtr == 0 )
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     1 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 5. Search Sibling Data */
            while ( sDataPlan->levelValue > 1 )
            {
                sBreak = ID_FALSE;
                IDE_TEST( searchSiblingData( aTemplate,
                                             sCodePlan,
                                             sDataPlan,
                                             &sBreak )
                          != IDE_SUCCESS );
                if ( sBreak == ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        if ( sDataPlan->levelValue <= 1 )
        {
            /* 6. finished hierarchy data new start */
            for ( sStack = sDataPlan->firstStack;
                  sStack != NULL;
                  sStack = sStack->next )
            {
                sStack->nextPos = 0;
            }
            IDE_TEST( doItFirst( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
        }
        else
        {
            sDataPlan->plan.myTuple->modify++;
        }
    }
    else
    {
        /* 7. Set Current Row */
        IDE_TEST( setCurrentRow( aTemplate,
                                 sCodePlan,
                                 sDataPlan,
                                 2 )
                  != IDE_SUCCESS );
        sDataPlan->plan.myTuple->modify++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::refineOffset( qcTemplate * aTemplate,
                              qmncCNBY   * aCodePlan,
                              mtcTuple   * aTuple )
{
    mtcTuple  * sCMTRTuple;

    IDE_ASSERT( aTuple != NULL );
    IDE_ASSERT( aTuple->columnCount > 0 );

    if ( ( aCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
         == QMNC_CNBY_CHILD_VIEW_TRUE )
    {
        IDE_TEST( qmnCMTR::getTuple( aTemplate,
                                     aCodePlan->plan.left,
                                     & sCMTRTuple )
                  != IDE_SUCCESS );

        // PROJ-2362 memory temp ���� ȿ���� ����
        // CMTR�� columns�� �����Ѵ�.
        IDE_DASSERT( aTuple->columnCount == sCMTRTuple->columnCount );

        idlOS::memcpy( (void*)aTuple->columns,
                       (void*)sCMTRTuple->columns,
                       ID_SIZEOF(mtcColumn) * sCMTRTuple->columnCount );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Initialize Sort Mtr Node
 *
 *   connect by ������ ���� ����Ǵ� Sort Mtr Node Initialize
 *   ���� ��� ) connect by prior id = pid;
 *   ���� pid�� sort mtr node�� �ȴ�.
 */
IDE_RC qmnCNBY::initSortMtrNode( qcTemplate * aTemplate,
                                 qmncCNBY   * /*aCodePlan*/,
                                 qmcMtrNode * aCodeNode,
                                 qmdMtrNode * aMtrNode )
{
    UInt sHeaderSize = 0;

    sHeaderSize = QMC_MEMSORT_TEMPHEADER_SIZE;

    IDE_TEST( qmc::linkMtrNode( aCodeNode, aMtrNode )
              != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aMtrNode,
                                0 )
              != IDE_SUCCESS );

    IDE_TEST( qmc::refineOffsets( aMtrNode, sHeaderSize )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               &aTemplate->tmplate,
                               aMtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 *  First Initialize
 *    1. Child Plan�� �����Ų��.
 *    2. CNBY ���� ���� �ʱ�ȭ�� �Ѵ�.
 *    3. NULL ROW�� �ʱ�ȭ �Ѵ�.
 *    4. CNBYStack �� �Ҵ��ϰ� �ʱ�ȭ �Ѵ�.
 *    5. Order siblings by column�� �ִٸ� base Table�� Sort ��Ų��.
 *    6. Key Range�� �ִٸ� Sort Temp�� �����Ѵ�.
 */
IDE_RC qmnCNBY::firstInit( qcTemplate * aTemplate,
                           qmncCNBY   * aCodePlan,
                           qmndCNBY   * aDataPlan )
{
    iduMemory  * sMemory   = NULL;
    qmcMtrNode * sTmp      = NULL;
    qmdMtrNode * sNode     = NULL;
    qmdMtrNode * sSortNode = NULL;
    qmdMtrNode * sBaseNode = NULL;
    qmdMtrNode * sPrev     = NULL;
    idBool       sFind     = ID_FALSE;
    mtcTuple   * sTuple    = NULL;

    sMemory = aTemplate->stmt->qmxMem;

    /* 1. Child Plan HMTR execute */
    IDE_TEST( execChild( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    /* 2. HIER ���� ������ �ʱ�ȭ */
    aDataPlan->plan.myTuple = &aTemplate->tmplate.rows[aCodePlan->myRowID];
    aDataPlan->childTuple   = &aTemplate->tmplate.rows[aCodePlan->baseRowID];
    aDataPlan->priorTuple   = &aTemplate->tmplate.rows[aCodePlan->priorRowID];


    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_LEVEL_MASK )
         == QMNC_CNBY_LEVEL_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->levelRowID];
        aDataPlan->levelPtr     = (SLong *)sTuple->row;
        aDataPlan->levelValue   = *(aDataPlan->levelPtr);
    }
    else
    {
        aDataPlan->levelDummy = 0;
        aDataPlan->levelPtr   = &aDataPlan->levelDummy;
        aDataPlan->levelValue = 0;
    }

    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_ISLEAF_MASK )
         == QMNC_CNBY_ISLEAF_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->isLeafRowID];
        aDataPlan->isLeafPtr    = (SLong *)sTuple->row;
    }
    else
    {
        aDataPlan->isLeafDummy = 0;
        aDataPlan->isLeafPtr = &aDataPlan->isLeafDummy;
    }

    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_FUNCTION_MASK )
         == QMNC_CNBY_FUNCTION_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->stackRowID];
        aDataPlan->stackPtr     = (SLong *)sTuple->row;
    }

    if ( aCodePlan->rownumFilter != NULL )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->rownumRowID];
        aDataPlan->rownumPtr    = (SLong *)sTuple->row;
        aDataPlan->rownumValue  = *(aDataPlan->rownumPtr);
    }
    else
    {
        /* Nothing to do */
    }
    aDataPlan->startWithPos = 0;
    aDataPlan->mKeyRange     = NULL;
    aDataPlan->mKeyFilter    = NULL;

    IDE_TEST( refineOffset( aTemplate, aCodePlan, aDataPlan->plan.myTuple )
              != IDE_SUCCESS );

    IDE_TEST( refineOffset( aTemplate, aCodePlan, aDataPlan->priorTuple )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->myRowID )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->priorRowID )
              != IDE_SUCCESS );

    /* 3. Null Row �ʱ�ȭ */
    IDE_TEST( getNullRow( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    /* 4. CNBY Stack alloc */
    IDU_FIT_POINT( "qmnCNBY::firstInit::cralloc::aDataPlan->firstStack",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( sMemory->cralloc( ID_SIZEOF( qmnCNBYStack),
                                (void **)&(aDataPlan->firstStack ) )
              != IDE_SUCCESS );

    aDataPlan->firstStack->nextPos = 0;
    aDataPlan->firstStack->myRowID   = aCodePlan->myRowID;
    aDataPlan->firstStack->baseRowID = aCodePlan->baseRowID;
    aDataPlan->firstStack->baseMTR   = aDataPlan->baseMTR;
    aDataPlan->lastStack = aDataPlan->firstStack;

    SMI_CURSOR_PROP_INIT( &(aDataPlan->mCursorProperty), NULL, NULL );
    aDataPlan->mCursorProperty.mParallelReadProperties.mThreadCnt = 1;
    aDataPlan->mCursorProperty.mParallelReadProperties.mThreadID  = 1;
    aDataPlan->mCursorProperty.mStatistics = aTemplate->stmt->mStatistics;

    aDataPlan->mtrNode = (qmdMtrNode *)( aTemplate->tmplate.data +
                                         aCodePlan->mtrNodeOffset );
    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_FUNCTION_MASK )
         == QMNC_CNBY_FUNCTION_TRUE )
    {
        /* 4.1 Set Connect By Stack Address */
        *aDataPlan->stackPtr = (SLong)(aDataPlan->firstStack);
    }

    if ( ( aCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
         == QMNC_CNBY_CHILD_VIEW_TRUE )
    {
        /* 5. Order siblings by column�� �ִٸ� base Table�� Sort ��Ų��. */
        if ( aCodePlan->baseSortNode != NULL )
        {
            sBaseNode = ( qmdMtrNode * )( aTemplate->tmplate.data +
                                          aCodePlan->baseSortOffset );
            sSortNode = sBaseNode;
            for ( sTmp = aCodePlan->baseSortNode;
                  sTmp != NULL;
                  sTmp = sTmp->next )
            {
                for ( sNode = aDataPlan->baseMTR->recordNode;
                      sNode != NULL;
                      sNode = sNode->next )
                {
                    if ( sTmp->dstNode->node.baseColumn ==
                         sNode->dstNode->node.column )
                    {
                        *sSortNode = *sNode;
                        sSortNode->next = NULL;
                        sSortNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;

                        if ( (sTmp->flag & QMC_MTR_SORT_ORDER_MASK)
                             == QMC_MTR_SORT_ASCENDING)
                        {
                            sSortNode->flag |= QMC_MTR_SORT_ASCENDING;
                        }
                        else
                        {
                            sSortNode->flag |= QMC_MTR_SORT_DESCENDING;
                        }

                        IDE_TEST(qmc::setCompareFunction( sSortNode )
                                 != IDE_SUCCESS );
                        sFind = ID_TRUE;

                        sPrev = sSortNode;

                        if ( sTmp->next != NULL )
                        {
                            sSortNode++;
                            sPrev->next = sSortNode;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            if ( sFind == ID_TRUE )
            {
                IDE_TEST( qmcSortTemp::setSortNode( aDataPlan->baseMTR,
                                                    sBaseNode )
                          != IDE_SUCCESS );
                IDE_TEST( qmcSortTemp::sort( aDataPlan->baseMTR )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( aCodePlan->priorNode != NULL )
        {
            IDE_TEST( qmnCMTR::setPriorNode( aTemplate,
                                             aCodePlan->plan.left,
                                             aCodePlan->priorNode )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* 6. Key Range�� �ִٸ� Sort Temp�� �����Ѵ� */
        if ( aCodePlan->connectByKeyRange != NULL )
        {
            aDataPlan->sortTuple = &aTemplate->tmplate.rows[aCodePlan->sortRowID];

            aDataPlan->sortMTR = (qmcdSortTemp *)( aTemplate->tmplate.data +
                                                   aCodePlan->sortMTROffset );
            IDE_TEST( initSortMtrNode( aTemplate,
                                       aCodePlan,
                                       aCodePlan->sortNode,
                                       aDataPlan->mtrNode )
                      != IDE_SUCCESS );

            IDE_TEST( makeSortTemp( aTemplate,
                                    aDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        IDE_TEST( initForTable( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );
    }

    *aDataPlan->flag &= ~QMND_CNBY_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_CNBY_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::initForTable( qcTemplate * aTemplate,
                              qmncCNBY   * aCodePlan,
                              qmndCNBY   * aDataPlan )

{
    iduMemory * sMemory = NULL;

    sMemory = aTemplate->stmt->qmxMem;

    if ( aCodePlan->mFixKeyRange != NULL )
    {
        IDE_TEST_RAISE( aCodePlan->mIndex == NULL, ERR_UNEXPECTED_INDEX );

        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 aCodePlan->mFixKeyRange,
                                                 &aDataPlan->mFixKeyRangeSize )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( aDataPlan->mFixKeyRangeSize <= 0, ERR_UNEXPECTED_WRONG_SIZE );

        IDU_FIT_POINT( "qmnCNBY::initForTable::cralloc::aDataPlan->mFixKeyRangeArea",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc( aDataPlan->mFixKeyRangeSize,
                                                    (void **)&aDataPlan->mFixKeyRangeArea )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->mFixKeyRangeArea == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        aDataPlan->mFixKeyRangeSize = 0;
        aDataPlan->mFixKeyRange     = NULL;
        aDataPlan->mFixKeyRangeArea = NULL;
    }

    if ( ( aCodePlan->mFixKeyRange != NULL ) &&
         ( aCodePlan->mFixKeyFilter != NULL ) )
    {
        IDE_TEST_RAISE( aCodePlan->mIndex == NULL, ERR_UNEXPECTED_INDEX );

        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 aCodePlan->mFixKeyFilter,
                                                 &aDataPlan->mFixKeyFilterSize )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( aDataPlan->mFixKeyFilterSize <= 0, ERR_UNEXPECTED_WRONG_SIZE );

        IDU_FIT_POINT( "qmnCNBY::initForTable::cralloc::aDataPlan->mFixKeyFilterArea",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc( aDataPlan->mFixKeyFilterSize,
                                                    (void **)&aDataPlan->mFixKeyFilterArea )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->mFixKeyFilterArea == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        aDataPlan->mFixKeyFilterSize = 0;
        aDataPlan->mFixKeyFilter     = NULL;
        aDataPlan->mFixKeyFilterArea = NULL;
    }

    if ( aCodePlan->mVarKeyRange != NULL )
    {
        IDE_TEST_RAISE( aCodePlan->mIndex == NULL, ERR_UNEXPECTED_INDEX );

        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 aCodePlan->mVarKeyRange,
                                                 &aDataPlan->mVarKeyRangeSize )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( aDataPlan->mVarKeyRangeSize <= 0, ERR_UNEXPECTED_WRONG_SIZE );

        IDU_FIT_POINT( "qmnCNBY::initForTable::cralloc::aDataPlan->mVarKeyRangeArea",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc( aDataPlan->mVarKeyRangeSize,
                                                    (void **)&aDataPlan->mVarKeyRangeArea )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->mVarKeyRangeArea == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        aDataPlan->mVarKeyRangeSize = 0;
        aDataPlan->mVarKeyRange     = NULL;
        aDataPlan->mVarKeyRangeArea = NULL;
    }

    if ( ( aCodePlan->mVarKeyFilter != NULL ) &&
         ( ( aCodePlan->mVarKeyRange != NULL ) ||
           ( aCodePlan->mFixKeyRange != NULL ) ) )
    {
        IDE_TEST_RAISE( aCodePlan->mIndex == NULL, ERR_UNEXPECTED_INDEX );

        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 aCodePlan->mVarKeyFilter,
                                                 &aDataPlan->mVarKeyFilterSize )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( aDataPlan->mVarKeyFilterSize <= 0, ERR_UNEXPECTED_WRONG_SIZE );

        IDU_FIT_POINT( "qmnCNBY::initForTable::cralloc::aDataPlan->mVarKeyFilterArea",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc( aDataPlan->mVarKeyFilterSize,
                                                    (void **)&aDataPlan->mVarKeyFilterArea )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->mVarKeyFilterArea == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        aDataPlan->mVarKeyFilterSize = 0;
        aDataPlan->mVarKeyFilter     = NULL;
        aDataPlan->mVarKeyFilterArea = NULL;
    }

    if ( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
           == QMN_PLAN_STORAGE_DISK )
    {
        IDU_FIT_POINT( "qmnCNBY::initForTable::alloc::aDataPlan->lastStack->items[0].rowPtr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( sMemory->alloc( aDataPlan->priorTuple->rowOffset,
                                  (void**)&aDataPlan->lastStack->items[0].rowPtr )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( aDataPlan->lastStack->items[0].rowPtr == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( aCodePlan->connectByKeyRange != NULL ) &&
         ( aCodePlan->mIndex == NULL ) &&
         ( ( aCodePlan->flag & QMNC_CNBY_FROM_DUAL_MASK )
           == QMNC_CNBY_FROM_DUAL_FALSE ) )
    {
        aDataPlan->sortTuple = &aTemplate->tmplate.rows[aCodePlan->sortRowID];

        aDataPlan->sortMTR = (qmcdSortTemp *)( aTemplate->tmplate.data +
                                               aCodePlan->sortMTROffset );
        IDE_TEST( initSortMtrNode( aTemplate,
                                   aCodePlan,
                                   aCodePlan->sortNode,
                                   aDataPlan->mtrNode )
                  != IDE_SUCCESS );

        IDE_TEST( makeSortTempTable( aTemplate,
                                     aCodePlan,
                                     aDataPlan )
                  != IDE_SUCCESS );

        *aDataPlan->flag &= ~QMND_CNBY_USE_SORT_TEMP_MASK;
        *aDataPlan->flag |= QMND_CNBY_USE_SORT_TEMP_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "aCodePlan->mIndex is",
                                  "NULL" ));
    }
    IDE_EXCEPTION( ERR_UNEXPECTED_WRONG_SIZE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "KeyRangeSize",
                                  "0" ));
    }
    IDE_EXCEPTION( ERR_MEM_ALLOC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 *  exec Child
 */
IDE_RC qmnCNBY::execChild( qcTemplate * aTemplate,
                           qmncCNBY   * aCodePlan,
                           qmndCNBY   * aDataPlan )
{
    qmndCMTR * sMtr = NULL;

    IDE_TEST( aCodePlan->plan.left->init( aTemplate,
                                          aCodePlan->plan.left )
              != IDE_SUCCESS);

    if ( ( aCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
         == QMNC_CNBY_CHILD_VIEW_TRUE )
    {
        sMtr = ( qmndCMTR * )( aTemplate->tmplate.data +
                               aCodePlan->plan.left->offset );
        aDataPlan->baseMTR = sMtr->sortMgr;
    }
    else
    {
        aDataPlan->baseMTR = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 *  Get Null Row
 */
IDE_RC qmnCNBY::getNullRow( qcTemplate * aTemplate,
                            qmncCNBY   * aCodePlan,
                            qmndCNBY   * aDataPlan )
{
    iduMemory * sMemory = NULL;
    UInt        sNullRowSize = 0;

    sMemory = aTemplate->stmt->qmxMem;

    if ( ( aCodePlan->flag & QMNC_CNBY_JOIN_MASK )
         == QMNC_CNBY_JOIN_FALSE )
    {
        if ( ( aCodePlan->flag & QMNC_CNBY_CHILD_VIEW_MASK )
             == QMNC_CNBY_CHILD_VIEW_TRUE )
        {
            /* Row Size ȹ�� */
            IDE_TEST( qmnCMTR::getNullRowSize( aTemplate,
                                               aCodePlan->plan.left,
                                               &sNullRowSize )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sNullRowSize <= 0, ERR_WRONG_ROW_SIZE );

            // Null Row�� ���� ���� �Ҵ�
            IDU_LIMITPOINT("qmnCNBY::getNullRow::malloc");
            IDE_TEST( sMemory->cralloc( sNullRowSize,
                                        &aDataPlan->nullRow )
                      != IDE_SUCCESS);

            IDE_TEST( qmnCMTR::getNullRow( aTemplate,
                                           aCodePlan->plan.left,
                                           aDataPlan->nullRow )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
                 == QMN_PLAN_STORAGE_DISK )
            {
                IDE_TEST_RAISE( aDataPlan->priorTuple->rowOffset <= 0, ERR_WRONG_ROW_SIZE );

                IDU_FIT_POINT( "qmnCNBY::getNullRow::cralloc::aDataPlan->nullRow",
                               idERR_ABORT_InsufficientMemory );
                IDE_TEST( sMemory->cralloc( aDataPlan->priorTuple->rowOffset,
                                            &aDataPlan->nullRow )
                          != IDE_SUCCESS);
                IDE_TEST( qmn::makeNullRow( aDataPlan->priorTuple,
                                            aDataPlan->nullRow )
                          != IDE_SUCCESS );
                SMI_MAKE_VIRTUAL_NULL_GRID( aDataPlan->mNullRID );

                IDU_FIT_POINT( "qmnCNBY::getNullRow::cralloc::aDataPlan->prevPriorRow",
                               idERR_ABORT_InsufficientMemory );
                IDE_TEST( sMemory->cralloc( aDataPlan->priorTuple->rowOffset,
                                            &aDataPlan->prevPriorRow )
                          != IDE_SUCCESS);
                SMI_MAKE_VIRTUAL_NULL_GRID( aDataPlan->mPrevPriorRID );
            }
            else
            {
                IDE_TEST( smiGetTableNullRow( aCodePlan->mTableHandle,
                                              (void **) &aDataPlan->nullRow,
                                              &aDataPlan->mNullRID )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        IDE_TEST( qmcSortTemp::getNullRow( aDataPlan->baseMTR,
                                           &aDataPlan->nullRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_WRONG_ROW_SIZE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnCNBY::getNullRow",
                                  "sRowSize <= 0" ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

/**
 * Plan print
 */
IDE_RC qmnCNBY::printPlan( qcTemplate   * aTemplate,
                           qmnPlan      * aPlan,
                           ULong          aDepth,
                           iduVarString * aString,
                           qmnDisplay     aMode )
{
    qmncCNBY  * sCodePlan = NULL;
    qmndCNBY  * sDataPlan = NULL;
    ULong        i         = 0;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)(aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    iduVarStringAppend( aString,
                        "CONNECT BY ( " );

    if ( sCodePlan->mIndex != NULL )
    {
        iduVarStringAppend( aString, "INDEX: " );

        iduVarStringAppend( aString, sCodePlan->mIndex->userName );
        iduVarStringAppend( aString, "." );
        iduVarStringAppend( aString, sCodePlan->mIndex->name );

        if ( ( *sDataPlan->flag & QMND_CNBY_INIT_DONE_MASK )
             == QMND_CNBY_INIT_DONE_TRUE )
        {
            if ( sDataPlan->mKeyRange == smiGetDefaultKeyRange() )
            {
                (void) iduVarStringAppend( aString, ", FULL SCAN " );
            }
            else
            {
                (void) iduVarStringAppend( aString, ", RANGE SCAN " );
            }
        }
        else
        {
            if ( ( sCodePlan->mFixKeyRange == NULL ) &&
                 ( sCodePlan->mVarKeyRange == NULL ) )
            {
                iduVarStringAppend( aString, ", FULL SCAN " );
            }
            else
            {
                iduVarStringAppend( aString, ", RANGE SCAN " );
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* Access ������ ��� */
    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( (*sDataPlan->flag & QMND_CNBY_INIT_DONE_MASK)
             == QMND_CNBY_INIT_DONE_TRUE )
        {
            if ( sCodePlan->connectByKeyRange != NULL )
            {
                iduVarStringAppendFormat( aString,
                                          "ACCESS: %"ID_UINT32_FMT,
                                          sDataPlan->plan.myTuple->modify );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "ACCESS: %"ID_UINT32_FMT,
                                          sDataPlan->plan.myTuple->modify );
            }
        }
        else
        {
            if ( sCodePlan->connectByKeyRange != NULL )
            {
                iduVarStringAppendFormat( aString,
                                          "ACCESS: 0" );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "ACCESS: 0" );
            }
        }

    }
    else
    {
        if ( sCodePlan->connectByKeyRange != NULL )
        {
            iduVarStringAppendFormat( aString,
                                      "ACCESS: ??" );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "ACCESS: ??" );
        }
    }

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    if ( ( QCU_TRCLOG_DETAIL_MTRNODE == 1 ) &&
         ( sCodePlan->connectByKeyRange != NULL ) )
    {
        if ( sCodePlan->sortNode != NULL )
        {
            qmn::printMTRinfo( aString,
                               aDepth,
                               sCodePlan->sortNode,
                               "sortNode",
                               sCodePlan->myRowID,
                               sCodePlan->sortRowID,
                               ID_USHORT_MAX );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( QCU_TRCLOG_DETAIL_MTRNODE == 1 ) &&
         ( ( sCodePlan->flag & QMNC_CNBY_JOIN_MASK )
           == QMNC_CNBY_JOIN_TRUE ) )
    {
        qmn::printMTRinfo( aString,
                           aDepth,
                           sCodePlan->mBaseMtrNode,
                           "mBaseMtrNode",
                           sCodePlan->myRowID,
                           sCodePlan->sortRowID,
                           ID_USHORT_MAX );
    }
    else
    {
        /* Nothing to do */
    }

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        if ( sCodePlan->startWithConstant != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ START WITH CONSTANT FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->startWithConstant )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->startWithFilter != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ START WITH FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->startWithFilter )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->startWithSubquery != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ START WITH SUBQUERY FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->startWithSubquery )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->startWithNNF != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ START WITH NOT-NORMAL-FORM FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->startWithNNF )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->connectByConstant != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ CONNECT BY CONSTANT FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->connectByConstant )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->connectByKeyRange != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ CONNECT BY KEY RANGE ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->connectByKeyRange )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->connectByFilter != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ CONNECT BY FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->connectByFilter )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->mFixKeyRange4Print != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ FIXED KEY ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->mFixKeyRange4Print)
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->mVarKeyRange != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }

            iduVarStringAppend( aString,
                                " [ VARIABLE KEY ]\n" );

            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->mVarKeyRange)
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->mFixKeyFilter4Print != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ FIXED KEY FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->mFixKeyFilter4Print)
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCodePlan->mVarKeyFilter != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }

            iduVarStringAppend( aString,
                                " [ VARIABLE KEY FILTER ]\n" );

            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->mVarKeyFilter)
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }
        if ( sCodePlan->levelFilter != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ LEVEL FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->levelFilter )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }

        /* BUG-39434 The connect by need rownum pseudo column. */
        if ( sCodePlan->rownumFilter != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ ROWNUM FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->rownumFilter )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( sCodePlan->startWithSubquery != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->startWithSubquery,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39041 need subquery for connect by clause */
    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        /* BUG-39041 need subquery for connect by clause */
        if ( sCodePlan->connectBySubquery != NULL )
        {
            for ( i = 0; i < aDepth; i++ )
            {
                iduVarStringAppend( aString,
                                    " " );
            }
            iduVarStringAppend( aString,
                                " [ CONNECT BY SUBQUERY FILTER ]\n" );
            IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                              aString,
                                              aDepth + 1,
                                              sCodePlan->connectBySubquery )
                     != IDE_SUCCESS);
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39041 need subquery for connect by clause */
    if ( sCodePlan->connectBySubquery != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->connectBySubquery,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //----------------------------
    // Operator�� ��� ���� ���
    //----------------------------
    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    sCodePlan->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* Child Plan�� ��� */
    IDE_TEST( aPlan->left->printPlan( aTemplate,
                                      aPlan->left,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

// baseMTR tuple ����
IDE_RC qmnCNBY::setBaseTupleSet( qcTemplate * aTemplate,
                                 qmndCNBY   * aDataPlan,
                                 const void * aRow )
{
    qmdMtrNode * sNode;

    for ( sNode = aDataPlan->baseMTR->recordNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        IDE_TEST( sNode->func.setTuple( aTemplate,
                                        sNode,
                                        (void *)aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

// baseMTR tuple�� ���� plan.myTuple ����
IDE_RC qmnCNBY::copyTupleSet( qcTemplate * aTemplate,
                              qmncCNBY   * aCodePlan,
                              mtcTuple   * aDstTuple )
{
    mtcTuple   * sCMTRTuple = NULL;
    mtcColumn  * sMyColumn;
    mtcColumn  * sCMTRColumn;
    UInt         i;

    if ( ( aCodePlan->flag & QMNC_CNBY_JOIN_MASK )
         == QMNC_CNBY_JOIN_FALSE )
    {
        IDE_TEST( qmnCMTR::getTuple( aTemplate,
                                     aCodePlan->plan.left,
                                     & sCMTRTuple )
                  != IDE_SUCCESS );

        sMyColumn = aDstTuple->columns;
        sCMTRColumn = sCMTRTuple->columns;
        for ( i = 0; i < sCMTRTuple->columnCount; i++, sMyColumn++, sCMTRColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sMyColumn->column.flag ) == ID_TRUE )
            {
                IDE_DASSERT( sCMTRColumn->module->actualSize(
                                 sCMTRColumn,
                                 sCMTRColumn->column.value )
                             <= sCMTRColumn->column.size );

                idlOS::memcpy( (SChar*)sMyColumn->column.value,
                               (SChar*)sCMTRColumn->column.value,
                               sCMTRColumn->module->actualSize(
                                   sCMTRColumn,
                                   sCMTRColumn->column.value ) );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItFirstDual( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
    idBool      sJudge     = ID_FALSE;
    idBool      sCheck     = ID_FALSE;
    qmncCNBY  * sCodePlan  = (qmncCNBY *)aPlan;
    qmndCNBY  * sDataPlan  = (qmndCNBY *)( aTemplate->tmplate.data +
                                           aPlan->offset );

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_NONE;
    /* BUG-47820 start with ������ level pseudo column�� ���� ��� �������. */
    sDataPlan->levelValue = 0;
    *sDataPlan->levelPtr = 0;

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    sDataPlan->levelValue = 1;
    *sDataPlan->levelPtr  = 1;
    sDataPlan->priorTuple->row = sDataPlan->nullRow;

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_NONE )
    {
        sDataPlan->lastStack->items[0].rowPtr = sDataPlan->plan.myTuple->row;
        sDataPlan->lastStack->items[0].level = 1;
        sDataPlan->lastStack->nextPos = 1;
        sDataPlan->plan.myTuple->modify++;

        if ( sCodePlan->connectByConstant != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->connectByConstant,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        if ( sJudge == ID_TRUE )
        {
            /* 1. Test Level Filter */
            if ( sCodePlan->levelFilter != NULL )
            {
                *sDataPlan->levelPtr = ( sDataPlan->levelValue + 1 );
                IDE_TEST( qtc::judge( &sCheck,
                                      sCodePlan->levelFilter,
                                      aTemplate )
                          != IDE_SUCCESS );

                *sDataPlan->levelPtr = sDataPlan->levelValue;
                if ( sCheck == ID_FALSE )
                {
                    *sDataPlan->isLeafPtr = 1;
                    IDE_CONT( NORMAL_EXIT );
                }
                else
                {
                    *sDataPlan->isLeafPtr = 0;
                }
            }
            else
            {
                /* Nothing to do */
            }

            if ( sCodePlan->rownumFilter != NULL )
            {
                sDataPlan->rownumValue = *(sDataPlan->rownumPtr);
                sDataPlan->rownumValue++;
                *sDataPlan->rownumPtr = sDataPlan->rownumValue;

                if ( qtc::judge( &sCheck, sCodePlan->rownumFilter, aTemplate )
                     != IDE_SUCCESS )
                {
                    sDataPlan->rownumValue--;
                    *sDataPlan->rownumPtr = sDataPlan->rownumValue;
                    IDE_TEST( 1 );
                }
                else
                {
                    /* Nothing to do */
                }

                sDataPlan->rownumValue--;
                *sDataPlan->rownumPtr = sDataPlan->rownumValue;

                if ( sCheck == ID_FALSE )
                {
                    *sDataPlan->isLeafPtr = 1;
                    IDE_CONT( NORMAL_EXIT );
                }
                else
                {
                    *sDataPlan->isLeafPtr = 0;
                }
            }
            else
            {
                /* Nothing to do */
            }

            sJudge = ID_FALSE;
            if ( sCodePlan->connectByFilter != NULL )
            {
                sDataPlan->priorTuple->row = sDataPlan->plan.myTuple->row;
                IDE_TEST( qtc::judge( &sJudge, sCodePlan->connectByFilter, aTemplate )
                          != IDE_SUCCESS );

                if ( sJudge == ID_FALSE )
                {
                    *sDataPlan->isLeafPtr = 1;
                }
                else
                {
                    IDE_TEST_RAISE( ( ( sCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
                                      == QMNC_CNBY_IGNORE_LOOP_FALSE ), err_loop_detected );
                }
            }
            else
            {
                sJudge = ID_TRUE;
            }

            /* BUG-39401 need subquery for connect by clause */
            if ( ( sCodePlan->connectBySubquery != NULL ) &&
                 ( sJudge == ID_TRUE ) )
            {
                IDE_TEST( qtc::judge( &sJudge, sCodePlan->connectBySubquery, aTemplate )
                          != IDE_SUCCESS );
                if ( sJudge == ID_FALSE )
                {
                    *sDataPlan->isLeafPtr = 1;
                }
                else
                {
                    /* Nothing td do */
                }
            }
            else
            {
                /* Nothing to do */
            }

            if ( sJudge == ID_TRUE )
            {
                sDataPlan->doIt = doItNextDual;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            *sDataPlan->isLeafPtr = 1;
            sDataPlan->doIt = doItFirstDual;
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_loop_detected )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMN_HIER_LOOP ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItNextDual( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
    qmncCNBY      * sCodePlan = NULL;
    qmndCNBY      * sDataPlan = NULL;
    idBool          sCheck    = ID_FALSE;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)( aTemplate->tmplate.data + aPlan->offset );

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    /* 1. Set qmcRowFlag */
    *aFlag = QMC_ROW_DATA_EXIST;

    *sDataPlan->isLeafPtr = 0;

    sDataPlan->levelValue++;

    /* 1. Test Level Filter */
    if ( sCodePlan->levelFilter != NULL )
    {
        *sDataPlan->levelPtr = ( sDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              sCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );

        *sDataPlan->levelPtr = ( sDataPlan->levelValue );

        if ( sCheck == ID_FALSE )
        {
            *sDataPlan->isLeafPtr = 1;
            sDataPlan->doIt = doItFirstDual;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        *sDataPlan->levelPtr = ( sDataPlan->levelValue );
    }

    if ( sCodePlan->rownumFilter != NULL )
    {
        sDataPlan->rownumValue = *(sDataPlan->rownumPtr);
        sDataPlan->rownumValue++;
        *sDataPlan->rownumPtr = sDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, sCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            sDataPlan->rownumValue--;
            *sDataPlan->rownumPtr = sDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        sDataPlan->rownumValue--;
        *sDataPlan->rownumPtr = sDataPlan->rownumValue;

        if ( sCheck == ID_FALSE )
        {
            *sDataPlan->isLeafPtr = 1;
            sDataPlan->doIt = doItFirstDual;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    sDataPlan->plan.myTuple->modify++;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItFirstTable( qcTemplate * aTemplate,
                                qmnPlan    * aPlan,
                                qmcRowFlag * aFlag )
{
    idBool      sJudge     = ID_FALSE;
    idBool      sExist     = ID_FALSE;
    qmncCNBY  * sCodePlan  = (qmncCNBY *)aPlan;
    qmndCNBY  * sDataPlan  = (qmndCNBY *)( aTemplate->tmplate.data +
                                           aPlan->offset );

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_NONE;

    sDataPlan->priorTuple->row = sDataPlan->nullRow;
    /* BUG-47820 start with ������ level pseudo column�� ���� ��� �������. */
    sDataPlan->levelValue = 0;
    *sDataPlan->levelPtr = 0;

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    sDataPlan->levelValue = 1;
    *sDataPlan->levelPtr = 1;

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_NONE )
    {
        sDataPlan->lastStack->items[0].rowPtr = sDataPlan->plan.myTuple->row;
        sDataPlan->priorTuple->row = sDataPlan->plan.myTuple->row;

        sDataPlan->lastStack->items[0].level = 1;
        sDataPlan->lastStack->nextPos = 1;

        if ( sCodePlan->connectByConstant != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->connectByConstant,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        if ( sJudge == ID_TRUE )
        {
            if ( ( *sDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
                 == QMND_CNBY_USE_SORT_TEMP_TRUE )
            {
                IDE_TEST( searchNextLevelDataSortTemp( aTemplate,
                                                       sCodePlan,
                                                       sDataPlan,
                                                       &sExist )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( searchNextLevelDataTable( aTemplate,
                                                    sCodePlan,
                                                    sDataPlan,
                                                    &sExist )
                          != IDE_SUCCESS );
            }

            if ( sExist == ID_TRUE )
            {
                *sDataPlan->isLeafPtr = 0;
                IDE_TEST( setCurrentRowTable( sDataPlan, 2 )
                          != IDE_SUCCESS );
                sDataPlan->doIt = qmnCNBY::doItNextTable;
            }
            else
            {
                *sDataPlan->isLeafPtr = 1;
                IDE_TEST( setCurrentRowTable( sDataPlan, 1 )
                          != IDE_SUCCESS );
                sDataPlan->doIt = qmnCNBY::doItFirstTable;
            }
        }
        else
        {
            *sDataPlan->isLeafPtr = 1;
            sDataPlan->doIt = qmnCNBY::doItFirstTable;
        }
        sDataPlan->plan.myTuple->modify++;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItNextTable( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
    qmncCNBY      * sCodePlan   = NULL;
    qmndCNBY      * sDataPlan   = NULL;
    qmnCNBYStack  * sStack      = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sBreak      = ID_FALSE;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)( aTemplate->tmplate.data + aPlan->offset );
    sStack    = sDataPlan->lastStack;

    /* 1. Set qmcRowFlag */
    *aFlag = QMC_ROW_DATA_EXIST;

    /* 2. Set PriorTupe Row Previous */
    sDataPlan->priorTuple->row = sDataPlan->prevPriorRow;

    /* 3. Search Next Level Data */
    if ( *sDataPlan->isLeafPtr == 0 )
    {
        if ( ( *sDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
             == QMND_CNBY_USE_SORT_TEMP_TRUE )
        {
            IDE_TEST( searchNextLevelDataSortTemp( aTemplate,
                                                   sCodePlan,
                                                   sDataPlan,
                                                   &sIsRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( searchNextLevelDataTable( aTemplate,
                                                sCodePlan,
                                                sDataPlan,
                                                &sIsRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsRow == ID_FALSE )
    {
        /* 4. Set isLeafPtr and Set CurrentRow Previous */
        if ( *sDataPlan->isLeafPtr == 0 )
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTable( sDataPlan, 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 5. Search Sibling Data */
            while ( sDataPlan->levelValue > 1 )
            {
                sBreak = ID_FALSE;
                IDE_TEST( searchSiblingDataTable( aTemplate,
                                                  sCodePlan,
                                                  sDataPlan,
                                                  &sBreak )
                          != IDE_SUCCESS );
                if ( sBreak == ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        if ( sDataPlan->levelValue <= 1 )
        {
            /* 6. finished hierarchy data new start */
            for ( sStack = sDataPlan->firstStack;
                  sStack != NULL;
                  sStack = sStack->next )
            {
                sStack->nextPos = 0;
            }
            IDE_TEST( doItFirstTable( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
        }
        else
        {
            sDataPlan->plan.myTuple->modify++;
        }
    }
    else
    {
        IDE_TEST( setCurrentRowTable( sDataPlan, 2 )
                  != IDE_SUCCESS );
        sDataPlan->plan.myTuple->modify++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���ÿ��� ���� Plan�� �÷��� item ���� */
IDE_RC qmnCNBY::setCurrentRowTable( qmndCNBY   * aDataPlan,
                                    UInt         aPrev )
{
    qmnCNBYStack * sStack  = NULL;
    mtcColumn    * sColumn = NULL;
    UInt           sIndex  = 0;
    UInt           i       = 0;

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos >= aPrev )
    {
        sIndex = sStack->nextPos - aPrev;
    }
    else
    {
        sIndex = aPrev - sStack->nextPos;
        IDE_DASSERT( sIndex <= QMND_CNBY_BLOCKSIZE );
        sIndex = QMND_CNBY_BLOCKSIZE - sIndex;
        sStack = sStack->prev;
    }

    IDE_TEST_RAISE( sStack == NULL, ERR_INTERNAL )

    aDataPlan->plan.myTuple->row = sStack->items[sIndex].rowPtr;
    aDataPlan->prevPriorRow      = aDataPlan->priorTuple->row;

    *aDataPlan->levelPtr    = sStack->items[sIndex].level;
    aDataPlan->firstStack->currentLevel = *aDataPlan->levelPtr;

    if ( sIndex > 0 )
    {
        aDataPlan->priorTuple->row = sStack->items[sIndex - 1].rowPtr;
    }
    else
    {
        if ( aDataPlan->firstStack->currentLevel >= QMND_CNBY_BLOCKSIZE )
        {
            sStack = sStack->prev;
            aDataPlan->priorTuple->row =
                    sStack->items[QMND_CNBY_BLOCKSIZE - 1].rowPtr;
        }
        else
        {
            aDataPlan->priorTuple->row = aDataPlan->nullRow;

            // PROJ-2362 memory temp ���� ȿ���� ����
            sColumn = aDataPlan->priorTuple->columns;
            for ( i = 0; i < aDataPlan->priorTuple->columnCount; i++, sColumn++ )
            {
                if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
                {
                    sColumn->module->null( sColumn,
                                           sColumn->column.value );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnCNBY::setCurrentRowTable",
                                  "The stack pointer is null" ));
    }

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchSiblingDataTable( qcTemplate * aTemplate,
                                        qmncCNBY   * aCodePlan,
                                        qmndCNBY   * aDataPlan,
                                        idBool     * aBreak )
{
    qmnCNBYStack  * sStack      = NULL;
    qmnCNBYStack  * sPriorStack = NULL;
    qmnCNBYItem   * sItem       = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sCheck      = ID_FALSE;
    idBool          sIsExist    = ID_FALSE;
    UInt            sPriorPos   = 0;

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos == 0 )
    {
        sStack = sStack->prev;
        aDataPlan->lastStack = sStack;
    }
    else
    {
        /* Nothing to do */
    }

    /* 1. nextPos�� �׻� ���� Stack�� ����Ű�Ƿ� �̸� ���� ���ѳ��´�. */
    --sStack->nextPos;

    if ( sStack->nextPos <= 0 )
    {
        sPriorStack = sStack->prev;
    }
    else
    {
        sPriorStack = sStack;
    }

    --aDataPlan->levelValue;

    /* 2. Set Prior Row  */
    sPriorPos = sPriorStack->nextPos - 1;

    aDataPlan->priorTuple->row = sPriorStack->items[sPriorPos].rowPtr;

    /* 3. Set Current CNBY Info */
    sItem = &sStack->items[sStack->nextPos];

    if ( ( *aDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
         == QMND_CNBY_USE_SORT_TEMP_TRUE )
    {
        IDE_TEST( searchKeyRangeRowTable( aTemplate,
                                          aCodePlan,
                                          aDataPlan,
                                          sItem,
                                          ID_FALSE,
                                          &sIsExist )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                         aCodePlan,
                                         aDataPlan ) != IDE_SUCCESS );

        /* 4. Search Siblings Data */
        IDE_TEST( searchSequnceRowTable( aTemplate,
                                         aCodePlan,
                                         aDataPlan,
                                         sItem,
                                         &sIsExist )
                  != IDE_SUCCESS );
    }

    /* 5. Push Siblings Data and Search Next level */
    if ( sIsExist == ID_TRUE )
    {

        /* BUG-39434 The connect by need rownum pseudo column.
         * Siblings �� ã�� ���� ���ÿ� �ֱ� ���� rownum�� �˻��ؾ��Ѵ�.
         */
        if ( aCodePlan->rownumFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sCheck,
                                  aCodePlan->rownumFilter,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sCheck = ID_TRUE;
        }

        if ( sCheck == ID_TRUE )
        {
            IDE_TEST( pushStackTable( aTemplate, aDataPlan, sItem )
                      != IDE_SUCCESS );

            if ( ( *aDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
                 == QMND_CNBY_USE_SORT_TEMP_TRUE )
            {
                IDE_TEST( searchNextLevelDataSortTemp( aTemplate,
                                                       aCodePlan,
                                                       aDataPlan,
                                                       &sIsRow )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( searchNextLevelDataTable( aTemplate,
                                                    aCodePlan,
                                                    aDataPlan,
                                                    &sIsRow )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sIsRow == ID_TRUE )
        {
            *aDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRowTable( aDataPlan, 2 )
                      != IDE_SUCCESS );
        }
        else
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTable( aDataPlan, 1 )
                      != IDE_SUCCESS );
        }
        *aBreak = ID_TRUE;
    }
    else
    {
        /* 6. isLeafPtr set 1 */
        if ( *aDataPlan->isLeafPtr == 0 )
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTable( aDataPlan, 0 )
                      != IDE_SUCCESS );
            *aBreak = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���� ������ �ڷ� Search */
IDE_RC qmnCNBY::searchNextLevelDataTable( qcTemplate  * aTemplate,
                                          qmncCNBY    * aCodePlan,
                                          qmndCNBY    * aDataPlan,
                                          idBool      * aExist )
{
    idBool               sCheck           = ID_FALSE;
    idBool               sExist           = ID_FALSE;
    idBool               sIsMutexLock     = ID_FALSE;
    iduMemory          * sMemory          = NULL;
    void               * sIndexHandle     = NULL;
    qcStatement        * sStmt            = aTemplate->stmt;
    qmnCNBYStack       * sStack           = NULL;
    qmnCNBYItem        * sItem            = NULL;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                     aCodePlan,
                                     aDataPlan ) != IDE_SUCCESS );

    sStack = aDataPlan->lastStack;
    sItem  = &sStack->items[sStack->nextPos];

    if ( sItem->mCursor == NULL )
    {
        sMemory = aTemplate->stmt->qmxMem;

        if ( aCodePlan->mIndex != NULL )
        {
            sIndexHandle = aCodePlan->mIndex->indexHandle;
            aDataPlan->mCursorProperty.mIndexTypeID = (UChar)aCodePlan->mIndex->indexTypeId;
        }
        else
        {
            sIndexHandle = NULL;
            aDataPlan->mCursorProperty.mIndexTypeID = SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID;
        }
        IDU_FIT_POINT( "qmnCNBY::searchNextLevelDataTable::alloc::sItem->mCursor",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( sMemory->alloc( ID_SIZEOF( smiTableCursor ),
                    (void **)&sItem->mCursor )
                != IDE_SUCCESS );
        IDE_TEST_RAISE( sItem->mCursor == NULL, ERR_MEM_ALLOC );

        sItem->mCursor->initialize();
        sItem->mCursor->setDumpObject( NULL );

        IDE_TEST( sStmt->mCursorMutex.lock(NULL) != IDE_SUCCESS );
        sIsMutexLock = ID_TRUE;

        IDE_TEST( sItem->mCursor->open( QC_SMI_STMT( aTemplate->stmt ),
                                       aCodePlan->mTableHandle,
                                       sIndexHandle,
                                       aCodePlan->mTableSCN,
                                       NULL,
                                       aDataPlan->mKeyRange,
                                       aDataPlan->mKeyFilter,
                                       &aDataPlan->mCallBack,
                                       SMI_LOCK_READ | SMI_TRAVERSE_FORWARD |
                                       SMI_PREVIOUS_DISABLE | SMI_TRANS_ISOLATION_IGNORE,
                                       SMI_SELECT_CURSOR,
                                       &aDataPlan->mCursorProperty )
                  != IDE_SUCCESS );

        IDE_TEST( aTemplate->cursorMgr->addOpenedCursor( sStmt->qmxMem,
                                                         aCodePlan->myRowID,
                                                         sItem->mCursor )
                  != IDE_SUCCESS );

        sIsMutexLock = ID_FALSE;
        IDE_TEST( sStmt->mCursorMutex.unlock() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sItem->mCursor->restart( aDataPlan->mKeyRange,
                                           aDataPlan->mKeyFilter,
                                           &aDataPlan->mCallBack )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sItem->mCursor->beforeFirst() != IDE_SUCCESS );

    IDE_TEST( searchSequnceRowTable( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     sItem,
                                     &sExist )
              != IDE_SUCCESS );

    /* 3. Push Stack */
    if ( sExist == ID_TRUE )
    {
        IDE_TEST( pushStackTable( aTemplate, aDataPlan, sItem )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsMutexLock == ID_TRUE )
    {
        (void)sStmt->mCursorMutex.unlock();
        sIsMutexLock = ID_FALSE;
    }
    else
    {
        /* Nothing to do */
    }
    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchSequnceRowTable( qcTemplate  * aTemplate,
                                       qmncCNBY    * aCodePlan,
                                       qmndCNBY    * aDataPlan,
                                       qmnCNBYItem * aItem,
                                       idBool      * aIsExist )
{
    void      * sOrgRow    = NULL;
    void      * sSearchRow = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sIsExist   = ID_FALSE;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    idBool      sIsAllSame = ID_TRUE;

    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                       &aDataPlan->plan.myTuple->rid,
                                       SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while ( sSearchRow != NULL )
    {
        aDataPlan->plan.myTuple->row = sSearchRow;

        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        if ( ( sSearchRow == aDataPlan->priorTuple->row ) &&
             ( aCodePlan->connectByConstant != NULL ) )
        {
            IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                               &aDataPlan->plan.myTuple->rid,
                                               SMI_FIND_NEXT )
                      != IDE_SUCCESS );

            if ( sSearchRow != NULL )
            {
                aDataPlan->plan.myTuple->row = sSearchRow;
            }
            else
            {
                aDataPlan->plan.myTuple->row = sOrgRow;
                break;
            }
        }
        else
        {
            /* Nothing to do */
        }

        /* 3. Judge ConnectBy Filter */
        sJudge = ID_FALSE;
        if ( aCodePlan->connectByFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectByFilter, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        /* BUG-39401 need subquery for connect by clause */
        if ( ( aCodePlan->connectBySubquery != NULL ) &&
             ( sJudge == ID_TRUE ) )
        {
            aDataPlan->plan.myTuple->modify++;
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectBySubquery, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            for ( sNode = aCodePlan->priorNode;
                  sNode != NULL;
                  sNode = (qtcNode *)sNode->node.next )
            {
                sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );
                if ( sRow1Size == sRow2Size )
                {
                    if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                         == 0 )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsAllSame = ID_FALSE;
                    break;
                }
            }
            if ( sIsAllSame == ID_TRUE )
            {
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sJudge == ID_TRUE )
        {
            /* 4. Check Hier Loop */
            IDE_TEST( checkLoop( aCodePlan,
                                 aDataPlan,
                                 aDataPlan->plan.myTuple->row,
                                 &sLoop )
                      != IDE_SUCCESS );

            if ( sLoop == ID_FALSE )
            {
                sIsExist = ID_TRUE;
                aItem->rowPtr = sSearchRow;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                           &aDataPlan->plan.myTuple->rid,
                                           SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    *aIsExist = sIsExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                       qmncCNBY   * aCodePlan,
                                       qmndCNBY   * aDataPlan )
{
    qmnCursorPredicate  sPredicateInfo;

    //-------------------------------------
    // Predicate ������ ����
    //-------------------------------------

    sPredicateInfo.index      = aCodePlan->mIndex;
    sPredicateInfo.tupleRowID = aCodePlan->myRowID;

    // Fixed Key Range ������ ����
    sPredicateInfo.fixKeyRangeArea = aDataPlan->mFixKeyRangeArea;
    sPredicateInfo.fixKeyRange     = aDataPlan->mFixKeyRange;
    sPredicateInfo.fixKeyRangeOrg  = aCodePlan->mFixKeyRange;
    sPredicateInfo.fixKeyRangeSize = aDataPlan->mFixKeyRangeSize;

    // Variable Key Range ������ ����
    sPredicateInfo.varKeyRangeArea = aDataPlan->mVarKeyRangeArea;
    sPredicateInfo.varKeyRange     = aDataPlan->mVarKeyRange;
    sPredicateInfo.varKeyRangeOrg  = aCodePlan->mVarKeyRange;
    sPredicateInfo.varKeyRange4FilterOrg = aCodePlan->mVarKeyRange4Filter;
    sPredicateInfo.varKeyRangeSize = aDataPlan->mVarKeyRangeSize;

    // Fixed Key Filter ������ ����
    sPredicateInfo.fixKeyFilterArea = aDataPlan->mFixKeyFilterArea;
    sPredicateInfo.fixKeyFilter     = aDataPlan->mFixKeyFilter;
    sPredicateInfo.fixKeyFilterOrg  = aCodePlan->mFixKeyFilter;
    sPredicateInfo.fixKeyFilterSize = aDataPlan->mFixKeyFilterSize;

    // Variable Key Filter ������ ����
    sPredicateInfo.varKeyFilterArea = aDataPlan->mVarKeyFilterArea;
    sPredicateInfo.varKeyFilter     = aDataPlan->mVarKeyFilter;
    sPredicateInfo.varKeyFilterOrg  = aCodePlan->mVarKeyFilter;
    sPredicateInfo.varKeyFilter4FilterOrg = aCodePlan->mVarKeyFilter4Filter;
    sPredicateInfo.varKeyFilterSize = aDataPlan->mVarKeyFilterSize;

    // Not Null Key Range ������ ����
    sPredicateInfo.notNullKeyRange = NULL;

    // Filter ������ ����
    sPredicateInfo.filter = aCodePlan->connectByFilter;

    sPredicateInfo.filterCallBack  = &aDataPlan->mCallBack;
    sPredicateInfo.callBackDataAnd = &aDataPlan->mCallBackDataAnd;
    sPredicateInfo.callBackData    = aDataPlan->mCallBackData;

    /* PROJ-2632 */
    sPredicateInfo.mSerialFilterInfo  = NULL;
    sPredicateInfo.mSerialExecuteData = NULL;

    //-------------------------------------
    // Key Range, Key Filter, Filter�� ����
    //-------------------------------------

    IDE_TEST( qmn::makeKeyRangeAndFilter( aTemplate,
                                          &sPredicateInfo )
              != IDE_SUCCESS );

    aDataPlan->mKeyRange = sPredicateInfo.keyRange;
    aDataPlan->mKeyFilter = sPredicateInfo.keyFilter;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::pushStackTable( qcTemplate  * aTemplate,
                                qmndCNBY    * aDataPlan,
                                qmnCNBYItem * aItem )
{
    qmnCNBYStack * sStack = NULL;

    sStack = aDataPlan->lastStack;

    aItem->level = ++aDataPlan->levelValue;

    aDataPlan->priorTuple->row = aItem->rowPtr;

    ++sStack->nextPos;
    if ( sStack->nextPos >= QMND_CNBY_BLOCKSIZE )
    {
        if ( sStack->next == NULL )
        {
            IDE_TEST( allocStackBlock( aTemplate, aDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->lastStack          = sStack->next;
            aDataPlan->lastStack->nextPos = 0;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::makeSortTempTable( qcTemplate * aTemplate,
                                   qmncCNBY   * aCodePlan,
                                   qmndCNBY   * aDataPlan )
{
    smiFetchColumnList * sFetchColumnList = NULL;
    void               * sSearchRow       = NULL;
    qmdMtrNode         * sNode            = NULL;
    qmnCNBYItem        * sItem            = NULL;
    idBool               sIsMutexLock     = ID_FALSE;
    qcStatement        * sStmt            = aTemplate->stmt;

    IDE_TEST( qmcSortTemp::init( aDataPlan->sortMTR,
                                 aTemplate,
                                 ID_UINT_MAX,
                                 aDataPlan->mtrNode,
                                 aDataPlan->mtrNode->next,
                                 0,
                                 QMCD_SORT_TMP_STORAGE_MEMORY )
              != IDE_SUCCESS );

    aDataPlan->mCursorProperty.mIndexTypeID = SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID;

    if ( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
           == QMN_PLAN_STORAGE_DISK )
    {
        IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                    aTemplate,
                    aCodePlan->priorRowID,
                    ID_FALSE,
                    NULL,
                    ID_TRUE,
                    &sFetchColumnList ) != IDE_SUCCESS );

        aDataPlan->mCursorProperty.mFetchColumnList = sFetchColumnList;
    }
    else
    {
        /* Nothing to do */
    }
    sItem = &aDataPlan->firstStack->items[0];

    IDU_FIT_POINT( "qmnCNBY::makeSortTempTable::alloc::sItem->mCursor",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( sStmt->qmxMem->alloc( ID_SIZEOF( smiTableCursor ),
                                    (void **)&sItem->mCursor )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sItem->mCursor == NULL, ERR_MEM_ALLOC );

    sItem->mCursor->initialize();
    sItem->mCursor->setDumpObject( NULL );

    IDE_TEST( sStmt->mCursorMutex.lock(NULL) != IDE_SUCCESS );
    sIsMutexLock = ID_TRUE;

    IDE_TEST( sItem->mCursor->open( QC_SMI_STMT( aTemplate->stmt ),
                                    aCodePlan->mTableHandle,
                                    NULL,
                                    aCodePlan->mTableSCN,
                                    NULL,
                                    smiGetDefaultKeyRange(),
                                    smiGetDefaultKeyRange(),
                                    smiGetDefaultFilter(),
                                    SMI_LOCK_READ | SMI_TRAVERSE_FORWARD |
                                    SMI_PREVIOUS_DISABLE | SMI_TRANS_ISOLATION_IGNORE,
                                    SMI_SELECT_CURSOR,
                                    &aDataPlan->mCursorProperty )
              != IDE_SUCCESS );

    IDE_TEST( aTemplate->cursorMgr->addOpenedCursor( sStmt->qmxMem,
                                                     aCodePlan->myRowID,
                                                     sItem->mCursor )
              != IDE_SUCCESS );

    sIsMutexLock = ID_FALSE;
    IDE_TEST( sStmt->mCursorMutex.unlock() != IDE_SUCCESS );

    IDE_TEST( sItem->mCursor->beforeFirst() != IDE_SUCCESS );

    sSearchRow = aDataPlan->plan.myTuple->row;

    IDE_TEST( sItem->mCursor->readRow( (const void **)&sSearchRow,
                                       &aDataPlan->plan.myTuple->rid,
                                       SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while ( sSearchRow != NULL )
    {
        aDataPlan->plan.myTuple->row = sSearchRow;

        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::alloc( aDataPlan->sortMTR,
                                      &aDataPlan->sortTuple->row )
                  != IDE_SUCCESS);

        for ( sNode = aDataPlan->mtrNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                        sNode,
                        aDataPlan->sortTuple->row )
                    != IDE_SUCCESS );
        }

        IDE_TEST( qmcSortTemp::addRow( aDataPlan->sortMTR,
                                       aDataPlan->sortTuple->row )
                  != IDE_SUCCESS );

        IDE_TEST( sItem->mCursor->readRow( (const void **)&sSearchRow,
                                           &aDataPlan->plan.myTuple->rid,
                                           SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmcSortTemp::sort( aDataPlan->sortMTR )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsMutexLock == ID_TRUE )
    {
        (void)sStmt->mCursorMutex.unlock();
        sIsMutexLock = ID_FALSE;
    }
    else
    {
        /* Nothing to do */
    }
    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchNextLevelDataSortTemp( qcTemplate  * aTemplate,
                                             qmncCNBY    * aCodePlan,
                                             qmndCNBY    * aDataPlan,
                                             idBool      * aExist )
{
    idBool               sCheck           = ID_FALSE;
    idBool               sExist           = ID_FALSE;
    qmnCNBYStack       * sStack           = NULL;
    qmnCNBYItem        * sItem            = NULL;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    sStack = aDataPlan->lastStack;
    sItem = &sStack->items[sStack->nextPos];

    IDE_TEST( searchKeyRangeRowTable( aTemplate,
                                      aCodePlan,
                                      aDataPlan,
                                      sItem,
                                      ID_TRUE,
                                      &sExist )
              != IDE_SUCCESS );

    if ( sExist == ID_TRUE )
    {
        IDE_TEST( pushStackTable( aTemplate, aDataPlan, sItem )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchKeyRangeRowTable( qcTemplate  * aTemplate,
                                        qmncCNBY    * aCodePlan,
                                        qmndCNBY    * aDataPlan,
                                        qmnCNBYItem * aItem,
                                        idBool        aIsNextLevel,
                                        idBool      * aIsExist )
{
    void      * sSearchRow = NULL;
    void      * sPrevRow   = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sNextRow   = ID_FALSE;
    idBool      sIsExist   = ID_FALSE;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    idBool      sIsAllSame = ID_TRUE;

    if ( aIsNextLevel == ID_TRUE )
    {
        /* 1. Get First Range Row */
        IDE_TEST( qmcSortTemp::getFirstRange( aDataPlan->sortMTR,
                                              aCodePlan->connectByKeyRange,
                                              &sSearchRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. Restore Position and Get Next Row */
        IDE_TEST( qmcSortTemp::setLastKey( aDataPlan->sortMTR,
                                           aItem->lastKey )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMTR,
                                              &aItem->pos )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                             &sSearchRow )
                  != IDE_SUCCESS );
    }

    while ( sSearchRow != NULL )
    {
        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        sPrevRow = aDataPlan->plan.myTuple->row;

        IDE_TEST( restoreTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   sSearchRow )
                  != IDE_SUCCESS );

        sJudge = ID_FALSE;
        IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectByFilter, aTemplate )
                  != IDE_SUCCESS );

        if ( sJudge == ID_FALSE )
        {
            sNextRow = ID_TRUE;
        }
        else
        {
            sNextRow = ID_FALSE;
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            for ( sNode = aCodePlan->priorNode;
                  sNode != NULL;
                  sNode = (qtcNode *)sNode->node.next )
            {
                sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );

                if ( sRow1Size == sRow2Size )
                {
                    if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                         == 0 )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsAllSame = ID_FALSE;
                    break;
                }
            }
            if ( sIsAllSame == ID_TRUE )
            {
                sNextRow = ID_TRUE;
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sJudge == ID_TRUE )
        {
            /* 4. Check Hier Loop */
            IDE_TEST( checkLoop( aCodePlan,
                                 aDataPlan,
                                 aDataPlan->plan.myTuple->row,
                                 &sLoop )
                      != IDE_SUCCESS );

            /* 7. Store Position and lastKey and Row Pointer */
            if ( sLoop == ID_FALSE )
            {
                sIsExist = ID_TRUE;
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMTR,
                                                    &aItem->pos )
                          != IDE_SUCCESS );
                IDE_TEST( qmcSortTemp::getLastKey( aDataPlan->sortMTR,
                                                   &aItem->lastKey )
                          != IDE_SUCCESS );

                aItem->rowPtr = aDataPlan->plan.myTuple->row;
            }
            else
            {
                sNextRow = ID_TRUE;
            }
        }
        else
        {
            aDataPlan->plan.myTuple->row = sPrevRow;
        }

        /* 8. Search Next Row Pointer */
        if ( sNextRow == ID_TRUE )
        {
            IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                                 &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sSearchRow = NULL;
        }

    }

    *aIsExist = sIsExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItFirstTableDisk( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
    idBool      sJudge     = ID_FALSE;
    idBool      sExist     = ID_FALSE;
    qmncCNBY  * sCodePlan  = (qmncCNBY *)aPlan;
    qmndCNBY  * sDataPlan  = (qmndCNBY *)( aTemplate->tmplate.data +
                                           aPlan->offset );

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_NONE;
    /* BUG-47820 start with ������ level pseudo column�� ���� ��� �������. */
    sDataPlan->levelValue = 0;
    *sDataPlan->levelPtr = 0;

    idlOS::memcpy( sDataPlan->priorTuple->row,
                   sDataPlan->nullRow, sDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( sDataPlan->mNullRID, sDataPlan->priorTuple->rid );

    IDE_TEST( aPlan->left->doIt( aTemplate, aPlan->left, aFlag )
              != IDE_SUCCESS );

    sDataPlan->levelValue = 1;
    *sDataPlan->levelPtr = 1;

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) != QMC_ROW_DATA_NONE )
    {
        idlOS::memcpy( sDataPlan->lastStack->items[0].rowPtr,
                       sDataPlan->plan.myTuple->row, sDataPlan->priorTuple->rowOffset );
        SC_COPY_GRID( sDataPlan->plan.myTuple->rid, sDataPlan->lastStack->items[0].mRid );

        idlOS::memcpy( sDataPlan->priorTuple->row,
                       sDataPlan->plan.myTuple->row, sDataPlan->priorTuple->rowOffset );
        SC_COPY_GRID( sDataPlan->plan.myTuple->rid, sDataPlan->priorTuple->rid );

        sDataPlan->lastStack->items[0].level = 1;
        sDataPlan->lastStack->nextPos = 1;

        if ( sCodePlan->connectByConstant != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->connectByConstant,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        if ( sJudge == ID_TRUE )
        {
            if ( ( *sDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
                 == QMND_CNBY_USE_SORT_TEMP_TRUE )
            {
                IDE_TEST( searchNextLevelDataSortTempDisk( aTemplate,
                                                           sCodePlan,
                                                           sDataPlan,
                                                           &sExist )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( searchNextLevelDataTableDisk( aTemplate,
                                                        sCodePlan,
                                                        sDataPlan,
                                                        &sExist )
                          != IDE_SUCCESS );
            }

            if ( sExist == ID_TRUE )
            {
                *sDataPlan->isLeafPtr = 0;
                IDE_TEST( setCurrentRowTableDisk( sDataPlan, 2 )
                          != IDE_SUCCESS );
                sDataPlan->doIt = qmnCNBY::doItNextTableDisk;
            }
            else
            {
                *sDataPlan->isLeafPtr = 1;
                IDE_TEST( setCurrentRowTableDisk( sDataPlan, 1 )
                          != IDE_SUCCESS );
                sDataPlan->doIt = qmnCNBY::doItFirstTableDisk;
            }
        }
        else
        {
            *sDataPlan->isLeafPtr = 1;
            sDataPlan->doIt = qmnCNBY::doItFirstTableDisk;
        }
        sDataPlan->plan.myTuple->modify++;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmnCNBY::doItNextTableDisk( qcTemplate * aTemplate,
                                   qmnPlan    * aPlan,
                                   qmcRowFlag * aFlag )
{
    qmncCNBY      * sCodePlan   = NULL;
    qmndCNBY      * sDataPlan   = NULL;
    qmnCNBYStack  * sStack      = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sBreak      = ID_FALSE;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)( aTemplate->tmplate.data + aPlan->offset );
    sStack    = sDataPlan->lastStack;

    /* 1. Set qmcRowFlag */
    *aFlag = QMC_ROW_DATA_EXIST;

    /* 2. Set PriorTupe Row Previous */
    idlOS::memcpy( sDataPlan->priorTuple->row,
                   sDataPlan->prevPriorRow, sDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( sDataPlan->mPrevPriorRID, sDataPlan->priorTuple->rid );

    /* 3. Search Next Level Data */
    if ( *sDataPlan->isLeafPtr == 0 )
    {
        if ( ( *sDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
             == QMND_CNBY_USE_SORT_TEMP_TRUE )
        {
            IDE_TEST( searchNextLevelDataSortTempDisk( aTemplate,
                                                       sCodePlan,
                                                       sDataPlan,
                                                       &sIsRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( searchNextLevelDataTableDisk( aTemplate,
                                                    sCodePlan,
                                                    sDataPlan,
                                                    &sIsRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsRow == ID_FALSE )
    {
        /* 4. Set isLeafPtr and Set CurrentRow Previous */
        if ( *sDataPlan->isLeafPtr == 0 )
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTableDisk( sDataPlan, 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 5. Search Sibling Data */
            while ( sDataPlan->levelValue > 1 )
            {
                sBreak = ID_FALSE;
                IDE_TEST( searchSiblingDataTableDisk( aTemplate,
                                                      sCodePlan,
                                                      sDataPlan,
                                                      &sBreak )
                          != IDE_SUCCESS );
                if ( sBreak == ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        if ( sDataPlan->levelValue <= 1 )
        {
            /* 6. finished hierarchy data new start */
            for ( sStack = sDataPlan->firstStack;
                  sStack != NULL;
                  sStack = sStack->next )
            {
                sStack->nextPos = 0;
            }
            IDE_TEST( doItFirstTableDisk( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
        }
        else
        {
            sDataPlan->plan.myTuple->modify++;
        }
    }
    else
    {
        IDE_TEST( setCurrentRowTableDisk( sDataPlan, 2 )
                  != IDE_SUCCESS );
        sDataPlan->plan.myTuple->modify++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmnCNBY::setCurrentRowTableDisk( qmndCNBY   * aDataPlan,
                                        UInt         aPrev )
{
    qmnCNBYStack * sStack = NULL;
    UInt           sIndex = 0;

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos >= aPrev )
    {
        sIndex = sStack->nextPos - aPrev;
    }
    else
    {
        sIndex = aPrev - sStack->nextPos;
        IDE_DASSERT( sIndex <= QMND_CNBY_BLOCKSIZE );
        sIndex = QMND_CNBY_BLOCKSIZE - sIndex;
        sStack = sStack->prev;
    }

    IDE_TEST_RAISE( sStack == NULL, ERR_INTERNAL )

    idlOS::memcpy( aDataPlan->plan.myTuple->row,
                   sStack->items[sIndex].rowPtr, aDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( sStack->items[sIndex].mRid, aDataPlan->plan.myTuple->rid );

    idlOS::memcpy( aDataPlan->prevPriorRow,
                   aDataPlan->priorTuple->row, aDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( aDataPlan->priorTuple->rid, aDataPlan->mPrevPriorRID );

    *aDataPlan->levelPtr    = sStack->items[sIndex].level;
    aDataPlan->firstStack->currentLevel = *aDataPlan->levelPtr;

    if ( sIndex > 0 )
    {
        idlOS::memcpy( aDataPlan->priorTuple->row,
                       sStack->items[sIndex - 1].rowPtr, aDataPlan->priorTuple->rowOffset );
        SC_COPY_GRID( sStack->items[sIndex - 1].mRid, aDataPlan->priorTuple->rid );
    }
    else
    {
        if ( aDataPlan->firstStack->currentLevel >= QMND_CNBY_BLOCKSIZE )
        {
            sStack = sStack->prev;
            idlOS::memcpy( aDataPlan->priorTuple->row,
                           sStack->items[QMND_CNBY_BLOCKSIZE - 1].rowPtr, aDataPlan->priorTuple->rowOffset );
            SC_COPY_GRID( sStack->items[QMND_CNBY_BLOCKSIZE - 1].mRid, aDataPlan->priorTuple->rid );
        }
        else
        {
            idlOS::memcpy( aDataPlan->priorTuple->row,
                           aDataPlan->nullRow, aDataPlan->priorTuple->rowOffset );
            SC_COPY_GRID( aDataPlan->mNullRID, aDataPlan->priorTuple->rid );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnCNBY::setCurrentRowTableDisk",
                                  "The stack pointer is null" ));
    }

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchSequnceRowTableDisk( qcTemplate  * aTemplate,
                                           qmncCNBY    * aCodePlan,
                                           qmndCNBY    * aDataPlan,
                                           qmnCNBYItem * aItem,
                                           idBool      * aIsExist )
{
    void      * sOrgRow    = NULL;
    void      * sSearchRow = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sIsExist   = ID_FALSE;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    idBool      sIsAllSame = ID_TRUE;

    sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
    IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                       &aDataPlan->plan.myTuple->rid,
                                       SMI_FIND_NEXT )
              != IDE_SUCCESS );

    while ( sSearchRow != NULL )
    {
        aDataPlan->plan.myTuple->row = sSearchRow;

        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        if ( ( SC_GRID_IS_EQUAL( aDataPlan->plan.myTuple->rid,
                                 aDataPlan->priorTuple->rid ) == ID_TRUE ) &&
             ( aCodePlan->connectByConstant != NULL ) )
        {
            IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                               &aDataPlan->plan.myTuple->rid,
                                              SMI_FIND_NEXT )
                      != IDE_SUCCESS );

            if ( sSearchRow != NULL )
            {
                aDataPlan->plan.myTuple->row = sSearchRow;
            }
            else
            {
                aDataPlan->plan.myTuple->row = sOrgRow;
                break;
            }
        }
        else
        {
            /* Nothing to do */
        }

        /* 3. Judge ConnectBy Filter */
        sJudge = ID_FALSE;
        if ( aCodePlan->connectByFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectByFilter, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sJudge = ID_TRUE;
        }

        /* BUG-39401 need subquery for connect by clause */
        if ( ( aCodePlan->connectBySubquery != NULL ) &&
             ( sJudge == ID_TRUE ) )
        {
            aDataPlan->plan.myTuple->modify++;
            IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectBySubquery, aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            for ( sNode = aCodePlan->priorNode;
                  sNode != NULL;
                  sNode = (qtcNode *)sNode->node.next )
            {
                sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );
                if ( sRow1Size == sRow2Size )
                {
                    if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                         == 0 )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsAllSame = ID_FALSE;
                    break;
                }
            }
            if ( sIsAllSame == ID_TRUE )
            {
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sJudge == ID_TRUE )
        {
            IDE_TEST( checkLoopDisk( aCodePlan,
                                     aDataPlan,
                                     &sLoop )
                      != IDE_SUCCESS );

            if ( sLoop == ID_FALSE )
            {
                sIsExist = ID_TRUE;
                idlOS::memcpy( aItem->rowPtr, sSearchRow, aDataPlan->priorTuple->rowOffset );
                SC_COPY_GRID( aDataPlan->plan.myTuple->rid, aItem->mRid );
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( aItem->mCursor->readRow( (const void **)&sSearchRow,
                                           &aDataPlan->plan.myTuple->rid,
                                           SMI_FIND_NEXT )
                  != IDE_SUCCESS );
    }

    *aIsExist = sIsExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmnCNBY::searchSiblingDataTableDisk( qcTemplate * aTemplate,
                                            qmncCNBY   * aCodePlan,
                                            qmndCNBY   * aDataPlan,
                                            idBool     * aBreak )
{
    qmnCNBYStack  * sStack      = NULL;
    qmnCNBYStack  * sPriorStack = NULL;
    qmnCNBYItem   * sItem       = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sCheck      = ID_FALSE;
    idBool          sIsExist    = ID_FALSE;
    UInt            sPriorPos   = 0;

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos == 0 )
    {
        sStack = sStack->prev;
        aDataPlan->lastStack = sStack;
    }
    else
    {
        /* Nothing to do */
    }

    /* 1. nextPos�� �׻� ���� Stack�� ����Ű�Ƿ� �̸� ���� ���ѳ��´�. */
    --sStack->nextPos;

    if ( sStack->nextPos <= 0 )
    {
        sPriorStack = sStack->prev;
    }
    else
    {
        sPriorStack = sStack;
    }

    --aDataPlan->levelValue;

    /* 2. Set Prior Row  */
    sPriorPos = sPriorStack->nextPos - 1;

    idlOS::memcpy( aDataPlan->priorTuple->row,
                   sPriorStack->items[sPriorPos].rowPtr, aDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( sPriorStack->items[sPriorPos].mRid, aDataPlan->priorTuple->rid );

    /* 3. Set Current CNBY Info */
    sItem = &sStack->items[sStack->nextPos];

    if ( ( *aDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
         == QMND_CNBY_USE_SORT_TEMP_TRUE )
    {
        IDE_TEST( searchKeyRangeRowTableDisk( aTemplate,
                                              aCodePlan,
                                              aDataPlan,
                                              sItem,
                                              ID_FALSE,
                                              &sIsExist )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                         aCodePlan,
                                         aDataPlan ) != IDE_SUCCESS );

        /* 4. Search Siblings Data */
        IDE_TEST( searchSequnceRowTableDisk( aTemplate,
                                             aCodePlan,
                                             aDataPlan,
                                             sItem,
                                             &sIsExist )
                  != IDE_SUCCESS );
    }

    /* 5. Push Siblings Data and Search Next level */
    if ( sIsExist == ID_TRUE )
    {

        /* BUG-39434 The connect by need rownum pseudo column.
         * Siblings �� ã�� ���� ���ÿ� �ֱ� ���� rownum�� �˻��ؾ��Ѵ�.
         */
        if ( aCodePlan->rownumFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sCheck,
                                  aCodePlan->rownumFilter,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sCheck = ID_TRUE;
        }

        if ( sCheck == ID_TRUE )
        {
            IDE_TEST( pushStackTableDisk( aTemplate, aDataPlan, sItem )
                      != IDE_SUCCESS );

            if ( ( *aDataPlan->flag & QMND_CNBY_USE_SORT_TEMP_MASK )
                 == QMND_CNBY_USE_SORT_TEMP_TRUE )
            {
                IDE_TEST( searchNextLevelDataSortTempDisk( aTemplate,
                                                           aCodePlan,
                                                           aDataPlan,
                                                           &sIsRow )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( searchNextLevelDataTableDisk( aTemplate,
                                                        aCodePlan,
                                                        aDataPlan,
                                                        &sIsRow )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sIsRow == ID_TRUE )
        {
            *aDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRowTableDisk( aDataPlan, 2 )
                      != IDE_SUCCESS );
        }
        else
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTableDisk( aDataPlan, 1 )
                      != IDE_SUCCESS );
        }
        *aBreak = ID_TRUE;
    }
    else
    {
        /* 6. isLeafPtr set 1 */
        if ( *aDataPlan->isLeafPtr == 0 )
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRowTableDisk( aDataPlan, 0 )
                      != IDE_SUCCESS );
            *aBreak = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchKeyRangeRowTableDisk( qcTemplate  * aTemplate,
                                            qmncCNBY    * aCodePlan,
                                            qmndCNBY    * aDataPlan,
                                            qmnCNBYItem * aItem,
                                            idBool        aIsNextLevel,
                                            idBool      * aIsExist )
{
    void      * sSearchRow = NULL;
    void      * sPrevRow   = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sNextRow   = ID_FALSE;
    idBool      sIsExist   = ID_FALSE;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    idBool      sIsAllSame = ID_TRUE;

    if ( aIsNextLevel == ID_TRUE )
    {
        /* 1. Get First Range Row */
        IDE_TEST( qmcSortTemp::getFirstRange( aDataPlan->sortMTR,
                                              aCodePlan->connectByKeyRange,
                                              &sSearchRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. Restore Position and Get Next Row */
        IDE_TEST( qmcSortTemp::setLastKey( aDataPlan->sortMTR,
                                           aItem->lastKey )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->sortMTR,
                                              &aItem->pos )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                             &sSearchRow )
                  != IDE_SUCCESS );
    }

    while ( sSearchRow != NULL )
    {
        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        sPrevRow = aDataPlan->plan.myTuple->row;

        IDE_TEST( restoreTupleSet( aTemplate,
                                   aDataPlan->mtrNode,
                                   sSearchRow )
                  != IDE_SUCCESS );

        sJudge = ID_FALSE;
        IDE_TEST( qtc::judge( &sJudge, aCodePlan->connectByFilter, aTemplate )
                  != IDE_SUCCESS );

        if ( sJudge == ID_FALSE )
        {
            sNextRow = ID_TRUE;
        }
        else
        {
            sNextRow = ID_FALSE;
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            for ( sNode = aCodePlan->priorNode;
                  sNode != NULL;
                  sNode = (qtcNode *)sNode->node.next )
            {
                sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );

                if ( sRow1Size == sRow2Size )
                {
                    if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                         == 0 )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsAllSame = ID_FALSE;
                    break;
                }
            }
            if ( sIsAllSame == ID_TRUE )
            {
                sNextRow = ID_TRUE;
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sJudge == ID_TRUE )
        {
            IDE_TEST( checkLoopDisk( aCodePlan,
                                     aDataPlan,
                                     &sLoop )
                      != IDE_SUCCESS );

            /* 7. Store Position and lastKey and Row Pointer */
            if ( sLoop == ID_FALSE )
            {
                sIsExist = ID_TRUE;
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->sortMTR,
                                                    &aItem->pos )
                          != IDE_SUCCESS );
                IDE_TEST( qmcSortTemp::getLastKey( aDataPlan->sortMTR,
                                                   &aItem->lastKey )
                          != IDE_SUCCESS );

                idlOS::memcpy( aItem->rowPtr,
                               aDataPlan->plan.myTuple->row,
                               aDataPlan->priorTuple->rowOffset );

                SC_COPY_GRID( aDataPlan->plan.myTuple->rid, aItem->mRid );
            }
            else
            {
                sNextRow = ID_TRUE;
            }
        }
        else
        {
            aDataPlan->plan.myTuple->row = sPrevRow;
        }

        /* 8. Search Next Row Pointer */
        if ( sNextRow == ID_TRUE )
        {
            IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->sortMTR,
                                                 &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sSearchRow = NULL;
        }
    }

    *aIsExist = sIsExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmnCNBY::searchNextLevelDataTableDisk( qcTemplate  * aTemplate,
                                              qmncCNBY    * aCodePlan,
                                              qmndCNBY    * aDataPlan,
                                              idBool      * aExist )
{
    idBool               sCheck           = ID_FALSE;
    idBool               sExist           = ID_FALSE;
    idBool               sIsMutexLock     = ID_FALSE;
    iduMemory          * sMemory          = NULL;
    void               * sIndexHandle     = NULL;
    smiFetchColumnList * sFetchColumnList = NULL;
    qcStatement        * sStmt            = aTemplate->stmt;
    qmnCNBYStack       * sStack           = NULL;
    qmnCNBYItem        * sItem            = NULL;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                     aCodePlan,
                                     aDataPlan ) != IDE_SUCCESS );

    sStack = aDataPlan->lastStack;
    sItem = &sStack->items[sStack->nextPos];

    if ( sItem->mCursor == NULL )
    {
        sMemory = aTemplate->stmt->qmxMem;

        if ( aCodePlan->mIndex != NULL )
        {
            sIndexHandle = aCodePlan->mIndex->indexHandle;
            aDataPlan->mCursorProperty.mIndexTypeID = (UChar)aCodePlan->mIndex->indexTypeId;
        }
        else
        {
            sIndexHandle = NULL;
            aDataPlan->mCursorProperty.mIndexTypeID = SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID;
        }
        IDU_FIT_POINT( "qmnCNBY::searchNextLevelDataTableDisk::alloc::sItem->mCursor",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( sMemory->alloc( ID_SIZEOF( smiTableCursor ),
                    (void **)&sItem->mCursor )
                != IDE_SUCCESS );
        IDE_TEST_RAISE( sItem->mCursor == NULL, ERR_MEM_ALLOC );

        IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                    aTemplate,
                    aCodePlan->priorRowID,
                    ID_FALSE,
                    ( sIndexHandle != NULL ) ? aCodePlan->mIndex: NULL,
                    ID_TRUE,
                    &sFetchColumnList ) != IDE_SUCCESS );

        aDataPlan->mCursorProperty.mFetchColumnList = sFetchColumnList;

        IDU_FIT_POINT( "qmnCNBY::searchNextLevelDataTableDisk::alloc::sItem->rowPtr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( sMemory->alloc( aDataPlan->priorTuple->rowOffset,
                                  (void**)&sItem->rowPtr )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sItem->rowPtr == NULL, ERR_MEM_ALLOC );

        sItem->mCursor->initialize();
        sItem->mCursor->setDumpObject( NULL );

        IDE_TEST( sStmt->mCursorMutex.lock(NULL) != IDE_SUCCESS );
        sIsMutexLock = ID_TRUE;

        IDE_TEST( sItem->mCursor->open( QC_SMI_STMT( aTemplate->stmt ),
                                       aCodePlan->mTableHandle,
                                       sIndexHandle,
                                       aCodePlan->mTableSCN,
                                       NULL,
                                       aDataPlan->mKeyRange,
                                       aDataPlan->mKeyFilter,
                                       &aDataPlan->mCallBack,
                                       SMI_LOCK_READ | SMI_TRAVERSE_FORWARD |
                                       SMI_PREVIOUS_DISABLE | SMI_TRANS_ISOLATION_IGNORE,
                                       SMI_SELECT_CURSOR,
                                       &aDataPlan->mCursorProperty )
                  != IDE_SUCCESS );

        IDE_TEST( aTemplate->cursorMgr->addOpenedCursor( sStmt->qmxMem,
                                                         aCodePlan->myRowID,
                                                         sItem->mCursor )
                  != IDE_SUCCESS );

        sIsMutexLock = ID_FALSE;
        IDE_TEST( sStmt->mCursorMutex.unlock() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sItem->mCursor->restart( aDataPlan->mKeyRange,
                                           aDataPlan->mKeyFilter,
                                           &aDataPlan->mCallBack )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sItem->mCursor->beforeFirst() != IDE_SUCCESS );

    IDE_TEST( searchSequnceRowTableDisk( aTemplate,
                                         aCodePlan,
                                         aDataPlan,
                                         sItem,
                                         &sExist )
              != IDE_SUCCESS );

    /* 3. Push Stack */
    if ( sExist == ID_TRUE )
    {
        IDE_TEST( pushStackTableDisk( aTemplate, aDataPlan, sItem )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsMutexLock == ID_TRUE )
    {
        (void)sStmt->mCursorMutex.unlock();
        sIsMutexLock = ID_FALSE;
    }
    else
    {
        /* Nothing to do */
    }
    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchNextLevelDataSortTempDisk( qcTemplate  * aTemplate,
                                                 qmncCNBY    * aCodePlan,
                                                 qmndCNBY    * aDataPlan,
                                                 idBool      * aExist )
{
    idBool               sCheck           = ID_FALSE;
    idBool               sExist           = ID_FALSE;
    qmnCNBYStack       * sStack           = NULL;
    qmnCNBYItem        * sItem            = NULL;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    sStack = aDataPlan->lastStack;
    sItem = &sStack->items[sStack->nextPos];

    if ( sItem->rowPtr == NULL )
    {
        IDU_FIT_POINT( "qmnCNBY::searchNextLevelDataSortTempDisk::alloc::sItem->rowPtr",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( aDataPlan->priorTuple->rowOffset,
                                                  (void**)&sItem->rowPtr )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sItem->rowPtr == NULL, ERR_MEM_ALLOC );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( searchKeyRangeRowTableDisk( aTemplate,
                                          aCodePlan,
                                          aDataPlan,
                                          sItem,
                                          ID_TRUE,
                                          &sExist )
              != IDE_SUCCESS );

    if ( sExist == ID_TRUE )
    {
        IDE_TEST( pushStackTableDisk( aTemplate, aDataPlan, sItem )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MEMORY_ALLOCATION ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::pushStackTableDisk( qcTemplate  * aTemplate,
                                    qmndCNBY    * aDataPlan,
                                    qmnCNBYItem * aItem )
{
    qmnCNBYStack * sStack = NULL;

    sStack = aDataPlan->lastStack;

    aItem->level = ++aDataPlan->levelValue;

    idlOS::memcpy( aDataPlan->priorTuple->row,
                   aItem->rowPtr, aDataPlan->priorTuple->rowOffset );
    SC_COPY_GRID( aItem->mRid, aDataPlan->priorTuple->rid );

    ++sStack->nextPos;
    if ( sStack->nextPos >= QMND_CNBY_BLOCKSIZE )
    {
        if ( sStack->next == NULL )
        {
            IDE_TEST( allocStackBlock( aTemplate, aDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->lastStack          = sStack->next;
            aDataPlan->lastStack->nextPos = 0;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::firstInitForJoin( qcTemplate * aTemplate,
                                  qmncCNBY   * aCodePlan,
                                  qmndCNBY   * aDataPlan )
{
    iduMemory  * sMemory   = NULL;
    qmcMtrNode * sTmp      = NULL;
    qmdMtrNode * sNode     = NULL;
    qmdMtrNode * sSortNode = NULL;
    qmdMtrNode * sBaseNode = NULL;
    qmdMtrNode * sPrev     = NULL;
    idBool       sFind     = ID_FALSE;
    mtcTuple   * sTuple    = NULL;
    qmcRowFlag   sRowFlag  = QMC_ROW_INITIALIZE;
    void       * sRow      = NULL;
    SInt         i         = 0;

    sMemory = aTemplate->stmt->qmxMem;

    aDataPlan->plan.myTuple = &aTemplate->tmplate.rows[aCodePlan->myRowID];
    aDataPlan->childTuple   = &aTemplate->tmplate.rows[aCodePlan->baseRowID];
    aDataPlan->priorTuple   = &aTemplate->tmplate.rows[aCodePlan->priorRowID];

    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_LEVEL_MASK )
         == QMNC_CNBY_LEVEL_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->levelRowID];
        aDataPlan->levelPtr     = (SLong *)sTuple->row;
        aDataPlan->levelValue   = *(aDataPlan->levelPtr);
    }
    else
    {
        aDataPlan->levelDummy = 0;
        aDataPlan->levelPtr   = &aDataPlan->levelDummy;
        aDataPlan->levelValue = 0;
    }

    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_ISLEAF_MASK )
         == QMNC_CNBY_ISLEAF_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->isLeafRowID];
        aDataPlan->isLeafPtr    = (SLong *)sTuple->row;
    }
    else
    {
        aDataPlan->isLeafDummy = 0;
        aDataPlan->isLeafPtr = &aDataPlan->isLeafDummy;
    }

    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_FUNCTION_MASK )
         == QMNC_CNBY_FUNCTION_TRUE )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->stackRowID];
        aDataPlan->stackPtr     = (SLong *)sTuple->row;
    }

    if ( aCodePlan->rownumFilter != NULL )
    {
        sTuple                  = &aTemplate->tmplate.rows[aCodePlan->rownumRowID];
        aDataPlan->rownumPtr    = (SLong *)sTuple->row;
        aDataPlan->rownumValue  = *(aDataPlan->rownumPtr);
    }
    else
    {
        /* Nothing to do */
    }
    aDataPlan->startWithPos = 0;
    aDataPlan->mKeyRange     = NULL;
    aDataPlan->mKeyFilter    = NULL;

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->myRowID )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->priorRowID )
              != IDE_SUCCESS );

    /* 4. CNBY Stack alloc */
    IDU_FIT_POINT( "qmnCNBY::firstInitForJoin::cralloc::aDataPlan->firstStack",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( sMemory->cralloc( ID_SIZEOF( qmnCNBYStack),
                                (void **)&(aDataPlan->firstStack ) )
              != IDE_SUCCESS );

    aDataPlan->firstStack->nextPos = 0;
    aDataPlan->firstStack->myRowID   = aCodePlan->myRowID;
    aDataPlan->firstStack->baseRowID = aCodePlan->baseRowID;
    aDataPlan->firstStack->baseMTR   = aDataPlan->baseMTR;
    aDataPlan->lastStack = aDataPlan->firstStack;

    SMI_CURSOR_PROP_INIT( &(aDataPlan->mCursorProperty), NULL, NULL );
    aDataPlan->mCursorProperty.mParallelReadProperties.mThreadCnt = 1;
    aDataPlan->mCursorProperty.mParallelReadProperties.mThreadID  = 1;
    aDataPlan->mCursorProperty.mStatistics = aTemplate->stmt->mStatistics;

    aDataPlan->mBaseMtrNode = (qmdMtrNode *)( aTemplate->tmplate.data +
                                              aCodePlan->mtrNodeOffset );
    /* BUG-48300 */
    if ( ( aCodePlan->flag & QMNC_CNBY_FUNCTION_MASK )
         == QMNC_CNBY_FUNCTION_TRUE )
    {
        /* 4.1 Set Connect By Stack Address */
        *aDataPlan->stackPtr = (SLong)(aDataPlan->firstStack);
    }

    aDataPlan->baseMTR = (qmcdSortTemp *)( aTemplate->tmplate.data +
                                           aCodePlan->mBaseSortMTROffset );
    IDE_TEST( qmc::linkMtrNode( aCodePlan->mBaseMtrNode, aDataPlan->mBaseMtrNode )
              != IDE_SUCCESS );

    IDE_TEST( qmc::initMtrNode( aTemplate,
                                aDataPlan->mBaseMtrNode,
                                0 )
              != IDE_SUCCESS );

    IDE_TEST( qmc::refineOffsets( aDataPlan->mBaseMtrNode, (UInt)QMC_MEMSORT_TEMPHEADER_SIZE )
              != IDE_SUCCESS );

    if ( aCodePlan->baseSortNode != NULL )
    {
        sBaseNode = ( qmdMtrNode * )( aTemplate->tmplate.data +
                                      aCodePlan->baseSortOffset );
        sSortNode = sBaseNode;
        for ( sTmp = aCodePlan->baseSortNode;
              sTmp != NULL;
              sTmp = sTmp->next )
        {
            for ( sNode = aDataPlan->mBaseMtrNode;
                  sNode != NULL;
                  sNode = sNode->next )
            {
                if ( ( sTmp->srcNode == sNode->srcNode ) &&
                     ( sTmp->dstNode == sNode->dstNode ) )
                {
                    *sSortNode = *sNode;
                    sSortNode->next = NULL;
                    sSortNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;

                    if ( (sTmp->flag & QMC_MTR_SORT_ORDER_MASK)
                         == QMC_MTR_SORT_ASCENDING)
                    {
                        sSortNode->flag |= QMC_MTR_SORT_ASCENDING;
                    }
                    else
                    {
                        sSortNode->flag |= QMC_MTR_SORT_DESCENDING;
                    }

                    IDE_TEST(qmc::setCompareFunction( sSortNode )
                             != IDE_SUCCESS );
                    sFind = ID_TRUE;

                    sPrev = sSortNode;

                    if ( sTmp->next != NULL )
                    {
                        sSortNode++;
                        sPrev->next = sSortNode;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    for ( i = 0; i < aTemplate->tmplate.rows[aCodePlan->priorRowID].columnCount; i++ )
    {
        aTemplate->tmplate.rows[aCodePlan->priorRowID].columns[i].column.offset =
            aTemplate->tmplate.rows[aCodePlan->myRowID].columns[i].column.offset;
    }

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               &aTemplate->tmplate,
                               aDataPlan->mBaseMtrNode->dstNode->node.table )
              != IDE_SUCCESS );

    /* Temp Table�� �ʱ�ȭ */
    IDE_TEST( qmcSortTemp::init( aDataPlan->baseMTR,
                                 aTemplate,
                                 ID_UINT_MAX,
                                 aDataPlan->mBaseMtrNode,
                                 sBaseNode,
                                 0,
                                 QMCD_SORT_TMP_STORAGE_MEMORY )
              != IDE_SUCCESS );

    IDE_TEST( getNullRow( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    IDE_TEST( aCodePlan->plan.left->init( aTemplate,
                                          aCodePlan->plan.left )
              != IDE_SUCCESS);

    // doIt left child
    IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                          aCodePlan->plan.left,
                                          &sRowFlag )
              != IDE_SUCCESS );

    while ( ( sRowFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sRow = aDataPlan->mBaseMtrNode->dstTuple->row;
        IDE_TEST( qmcSortTemp::alloc( aDataPlan->baseMTR,
                                      &sRow )
                  != IDE_SUCCESS );

        for ( sNode = aDataPlan->mBaseMtrNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setMtr( aTemplate,
                                          sNode,
                                          sRow )
                      != IDE_SUCCESS );
        }
        // Temp Table�� ����
        IDE_TEST( qmcSortTemp::addRow( aDataPlan->baseMTR,
                                       sRow )
                  != IDE_SUCCESS );

        // Left Child�� ����
        IDE_TEST( aCodePlan->plan.left->doIt( aTemplate,
                                              aCodePlan->plan.left,
                                              &sRowFlag )
                  != IDE_SUCCESS );
    }

    if ( sFind == ID_TRUE )
    {
        IDE_TEST( qmcSortTemp::sort( aDataPlan->baseMTR )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( aCodePlan->connectByKeyRange != NULL ) &&
         ( ( aCodePlan->flag & QMNC_CNBY_SIBLINGS_MASK )
           == QMNC_CNBY_SIBLINGS_TRUE ) )
    {
        aDataPlan->sortTuple = &aTemplate->tmplate.rows[aCodePlan->sortRowID];

        aDataPlan->mtrNode = (qmdMtrNode *)( aTemplate->tmplate.data +
                                             aCodePlan->mSortNodeOffset );
        aDataPlan->sortMTR = (qmcdSortTemp *)( aTemplate->tmplate.data +
                                               aCodePlan->sortMTROffset );

        IDE_TEST( initSortMtrNode( aTemplate,
                                   aCodePlan,
                                   aCodePlan->sortNode,
                                   aDataPlan->mtrNode )
                  != IDE_SUCCESS );

        IDE_TEST( makeSortTemp( aTemplate,
                                aDataPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    *aDataPlan->flag &= ~QMND_CNBY_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_CNBY_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItFirstJoin( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
    SInt        sCount     = 0;
    idBool      sJudge     = ID_FALSE;
    idBool      sExist     = ID_FALSE;
    void      * sSearchRow = NULL;
    qmncCNBY  * sCodePlan  = (qmncCNBY *)aPlan;
    qmndCNBY  * sDataPlan  = (qmndCNBY *)( aTemplate->tmplate.data +
                                           aPlan->offset );
    qmnCNBYItem sReturnItem;
    qtcNode   * sFilters[START_WITH_MAX];
    SInt        i = 0;

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );
    IDE_TEST( initCNBYItem( &sReturnItem) != IDE_SUCCESS );

    *aFlag = QMC_ROW_DATA_NONE;
    /* BUG-47820 start with ������ level pseudo column�� ���� ��� �������. */
    sDataPlan->levelValue = 0;
    *sDataPlan->levelPtr = 0;

    sFilters[START_WITH_FILTER]   = sCodePlan->startWithFilter;
    sFilters[START_WITH_SUBQUERY] = sCodePlan->startWithSubquery;
    sFilters[START_WITH_NNF]      = sCodePlan->startWithNNF;

    do
    {
        sJudge  = ID_FALSE;

        if ( sDataPlan->startWithPos == 0 )
        {
            IDE_TEST( qmcSortTemp::getFirstSequence( sDataPlan->baseMTR,
                                                     &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( sCount == 0 )
            {
                IDE_TEST( qmcSortTemp::restoreCursor( sDataPlan->baseMTR,
                                                      &sDataPlan->lastStack->items[0].pos )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( qmcSortTemp::getNextSequence( sDataPlan->baseMTR,
                                                    &sSearchRow )
                      != IDE_SUCCESS );
        }

        IDE_TEST_CONT( sSearchRow == NULL, NORMAL_EXIT );

        sDataPlan->plan.myTuple->row = sSearchRow;
        sDataPlan->priorTuple->row = sSearchRow;
        sDataPlan->startWithPos++;
        sCount++;

        IDE_TEST( setBaseTupleSet( aTemplate,
                                   sDataPlan,
                                   sDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        for ( i = 0 ; i < START_WITH_MAX; i++ )
        {
            sJudge = ID_TRUE;
            if ( sFilters[i] != NULL )
            {
                if ( i == START_WITH_SUBQUERY )
                {
                    sDataPlan->plan.myTuple->modify++;
                }
                else
                {
                    /* Nothing to do */
                }
                IDE_TEST( qtc::judge( &sJudge,
                                      sFilters[i],
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            if ( sJudge == ID_FALSE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
    } while ( sJudge == ID_FALSE );

    *aFlag = QMC_ROW_DATA_EXIST;
    sDataPlan->levelValue = 1;
    *sDataPlan->levelPtr = 1;

    /* 4. STORE STACK */
    IDE_TEST( qmcSortTemp::storeCursor( sDataPlan->baseMTR,
                                        &sDataPlan->lastStack->items[0].pos)
              != IDE_SUCCESS );

    IDE_TEST( initCNBYItem( &sDataPlan->lastStack->items[0])
              != IDE_SUCCESS );
    sDataPlan->lastStack->items[0].rowPtr = sSearchRow;
    sDataPlan->lastStack->nextPos = 1;

    /* 5. Check CONNECT BY CONSTANT FILTER */
    if ( sCodePlan->connectByConstant != NULL )
    {
        IDE_TEST( qtc::judge( &sJudge,
                              sCodePlan->connectByConstant,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    /* 6. Search Next Level Data */
    if ( sJudge == ID_TRUE )
    {
        IDE_TEST( searchNextLevelDataForJoin( aTemplate,
                                              sCodePlan,
                                              sDataPlan,
                                              &sReturnItem,
                                              &sExist )
                  != IDE_SUCCESS );
        if ( sExist == ID_TRUE )
        {
            *sDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     2 )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnCNBY::doItNextJoin;
        }
        else
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     1 )
                      != IDE_SUCCESS );
            sDataPlan->doIt = qmnCNBY::doItFirstJoin;
        }
    }
    else
    {
        *sDataPlan->isLeafPtr = 1;
        sDataPlan->doIt = qmnCNBY::doItFirstJoin;
    }
    sDataPlan->plan.myTuple->modify++;

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::doItNextJoin( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
    qmncCNBY      * sCodePlan   = NULL;
    qmndCNBY      * sDataPlan   = NULL;
    qmnCNBYStack  * sStack      = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sBreak      = ID_FALSE;
    qmnCNBYItem     sNewItem;

    sCodePlan = (qmncCNBY *)aPlan;
    sDataPlan = (qmndCNBY *)( aTemplate->tmplate.data + aPlan->offset );
    sStack    = sDataPlan->lastStack;

    /* 1. Set qmcRowFlag */
    *aFlag = QMC_ROW_DATA_EXIST;
    IDE_TEST( initCNBYItem( &sNewItem) != IDE_SUCCESS );

    /* 2. Set PriorTupe Row Previous */
    sDataPlan->priorTuple->row = sDataPlan->prevPriorRow;

    IDE_TEST( setBaseTupleSet( aTemplate,
                               sDataPlan,
                               sDataPlan->priorTuple->row )
              != IDE_SUCCESS );

    /* 3. Search Next Level Data */
    if ( *sDataPlan->isLeafPtr == 0 )
    {
        IDE_TEST( searchNextLevelDataForJoin( aTemplate,
                                              sCodePlan,
                                              sDataPlan,
                                              &sNewItem,
                                              &sIsRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsRow == ID_FALSE )
    {
        /* 4. Set isLeafPtr and Set CurrentRow Previous */
        if ( *sDataPlan->isLeafPtr == 0 )
        {
            *sDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     sCodePlan,
                                     sDataPlan,
                                     1 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 5. Search Sibling Data */
            while ( sDataPlan->levelValue > 1 )
            {
                sBreak = ID_FALSE;
                IDE_TEST( searchSiblingDataForJoin( aTemplate,
                                                    sCodePlan,
                                                    sDataPlan,
                                                    &sBreak )
                          != IDE_SUCCESS );
                if ( sBreak == ID_TRUE )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        if ( sDataPlan->levelValue <= 1 )
        {
            /* 6. finished hierarchy data new start */
            for ( sStack = sDataPlan->firstStack;
                  sStack != NULL;
                  sStack = sStack->next )
            {
                sStack->nextPos = 0;
            }
            IDE_TEST( doItFirstJoin( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );
        }
        else
        {
            sDataPlan->plan.myTuple->modify++;
        }
    }
    else
    {
        /* 7. Set Current Row */
        IDE_TEST( setCurrentRow( aTemplate,
                                 sCodePlan,
                                 sDataPlan,
                                 2 )
                  != IDE_SUCCESS );
        sDataPlan->plan.myTuple->modify++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchNextLevelDataForJoin( qcTemplate  * aTemplate,
                                            qmncCNBY    * aCodePlan,
                                            qmndCNBY    * aDataPlan,
                                            qmnCNBYItem * aItem,
                                            idBool      * aExist )
{
    idBool sCheck = ID_FALSE;
    idBool sExist = ID_FALSE;

    /* 1. Test Level Filter */
    if ( aCodePlan->levelFilter != NULL )
    {
        *aDataPlan->levelPtr = ( aDataPlan->levelValue + 1 );
        IDE_TEST( qtc::judge( &sCheck,
                              aCodePlan->levelFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * Next Level�� ã�� ������ Rownum�� �ϳ� �������� �˻��ؾ��Ѵ�.
     * �ֳ��ϸ� �׻� isLeaf �˻�� ���� ���� �� �ִ��� �˻��ϱ� �����̴�
     * �׷��� rownum �� �ϳ� �������Ѽ� �˻��ؾ��Ѵ�.
     */
    if ( aCodePlan->rownumFilter != NULL )
    {
        aDataPlan->rownumValue = *(aDataPlan->rownumPtr);
        aDataPlan->rownumValue++;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        if ( qtc::judge( &sCheck, aCodePlan->rownumFilter, aTemplate )
             != IDE_SUCCESS )
        {
            aDataPlan->rownumValue--;
            *aDataPlan->rownumPtr = aDataPlan->rownumValue;
            IDE_TEST( 1 );
        }
        else
        {
            /* Nothing to do */
        }

        aDataPlan->rownumValue--;
        *aDataPlan->rownumPtr = aDataPlan->rownumValue;

        IDE_TEST_CONT( sCheck == ID_FALSE, NORMAL_EXIT );
    }
    else
    {
        sCheck = ID_TRUE;
    }

    /* 2. Search Key Range Row */
    IDE_TEST( searchKeyRangeRowForJoin( aTemplate,
                                        aCodePlan,
                                        aDataPlan,
                                        NULL,
                                        aItem )
              != IDE_SUCCESS );

    /* 3. Push Stack */
    if ( aItem->rowPtr != NULL )
    {
        aItem->level = ++aDataPlan->levelValue;
        IDE_TEST( pushStack( aTemplate,
                             aCodePlan,
                             aDataPlan,
                             aItem )
                  != IDE_SUCCESS );
        sExist = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT )

    *aExist = sExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchSiblingDataForJoin( qcTemplate * aTemplate,
                                          qmncCNBY   * aCodePlan,
                                          qmndCNBY   * aDataPlan,
                                          idBool     * aBreak )
{
    qmnCNBYStack  * sStack      = NULL;
    qmnCNBYStack  * sPriorStack = NULL;
    qmnCNBYItem   * sItem       = NULL;
    idBool          sIsRow      = ID_FALSE;
    idBool          sCheck      = ID_FALSE;
    UInt            sPriorPos   = 0;
    qmnCNBYItem     sNewItem;

    IDE_TEST( initCNBYItem( &sNewItem) != IDE_SUCCESS );

    sStack = aDataPlan->lastStack;

    if ( sStack->nextPos == 0 )
    {
        sStack = sStack->prev;
        aDataPlan->lastStack = sStack;
    }
    else
    {
        /* Nothing to do */
    }

    /* 1. nextPos�� �׻� ���� Stack�� ����Ű�Ƿ� �̸� ���� ���ѳ��´�. */
    --sStack->nextPos;

    if ( sStack->nextPos <= 0 )
    {
        sPriorStack = sStack->prev;
    }
    else
    {
        sPriorStack = sStack;
    }

    --aDataPlan->levelValue;

    /* 2. Set Prior Row  */
    sPriorPos = sPriorStack->nextPos - 1;
    aDataPlan->priorTuple->row = sPriorStack->items[sPriorPos].rowPtr;

    IDE_TEST( setBaseTupleSet( aTemplate,
                               aDataPlan,
                               aDataPlan->priorTuple->row )
              != IDE_SUCCESS );

    /* 3. Set Current CNBY Info */
    sItem = &sStack->items[sStack->nextPos];

    IDE_TEST( searchKeyRangeRowForJoin( aTemplate,
                                        aCodePlan,
                                        aDataPlan,
                                        sItem,
                                        &sNewItem )
              != IDE_SUCCESS );

    /* 5. Push Siblings Data and Search Next level */
    if ( sNewItem.rowPtr != NULL )
    {
        /* BUG-39434 The connect by need rownum pseudo column.
         * Siblings �� ã�� ���� ���ÿ� �ֱ� ���� rownum�� �˻��ؾ��Ѵ�.
         */
        if ( aCodePlan->rownumFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sCheck,
                                  aCodePlan->rownumFilter,
                                  aTemplate )
                      != IDE_SUCCESS );
        }
        else
        {
            sCheck = ID_TRUE;
        }

        if ( sCheck == ID_TRUE )
        {
            sNewItem.level = ++aDataPlan->levelValue;
            IDE_TEST( pushStack( aTemplate,
                                 aCodePlan,
                                 aDataPlan,
                                 &sNewItem )
                      != IDE_SUCCESS );
            IDE_TEST( searchNextLevelDataForJoin( aTemplate,
                                                  aCodePlan,
                                                  aDataPlan,
                                                  &sNewItem,
                                                  &sIsRow )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        if ( sIsRow == ID_TRUE )
        {
            *aDataPlan->isLeafPtr = 0;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     2 )
                      != IDE_SUCCESS );
        }
        else
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     1 )
                      != IDE_SUCCESS );
        }
        *aBreak = ID_TRUE;
    }
    else
    {
        /* 6. isLeafPtr set 1 */
        if ( *aDataPlan->isLeafPtr == 0 )
        {
            *aDataPlan->isLeafPtr = 1;
            IDE_TEST( setCurrentRow( aTemplate,
                                     aCodePlan,
                                     aDataPlan,
                                     0 )
                      != IDE_SUCCESS );
            *aBreak = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmnCNBY::searchKeyRangeRowForJoin( qcTemplate  * aTemplate,
                                          qmncCNBY    * aCodePlan,
                                          qmndCNBY    * aDataPlan,
                                          qmnCNBYItem * aOldItem,
                                          qmnCNBYItem * aNewItem )
{
    void      * sSearchRow = NULL;
    void      * sPrevRow   = NULL;
    idBool      sJudge     = ID_FALSE;
    idBool      sLoop      = ID_FALSE;
    idBool      sNextRow   = ID_FALSE;
    idBool      sIsAllSame = ID_TRUE;
    void      * sRow1      = NULL;
    void      * sRow2      = NULL;
    UInt        sRow1Size  = 0;
    UInt        sRow2Size  = 0;
    qtcNode   * sNode      = NULL;
    mtcColumn * sColumn    = NULL;

    aNewItem->rowPtr = NULL;

    if ( aOldItem == NULL )
    {
        /* 1. Get First Range Row */
        IDE_TEST( qmcSortTemp::getFirstRange( aDataPlan->baseMTR,
                                              aCodePlan->connectByKeyRange,
                                              &sSearchRow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* 2. Restore Position and Get Next Row */
        IDE_TEST( qmcSortTemp::setLastKey( aDataPlan->baseMTR,
                                           aOldItem->lastKey )
                  != IDE_SUCCESS );
        IDE_TEST( qmcSortTemp::restoreCursor( aDataPlan->baseMTR,
                                              &aOldItem->pos )
                  != IDE_SUCCESS );

        IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->baseMTR,
                                             &sSearchRow )
                  != IDE_SUCCESS );
    }

    while ( sSearchRow != NULL )
    {
        /* ������ ���� �˻� */
        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        sPrevRow = aDataPlan->plan.myTuple->row;

        aDataPlan->plan.myTuple->row = sSearchRow;

        IDE_TEST( setBaseTupleSet( aTemplate,
                                   aDataPlan,
                                   aDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );

        /* 4. Judge Connect By Filter */
        IDE_TEST( qtc::judge( &sJudge,
                              aCodePlan->connectByFilter,
                              aTemplate )
                  != IDE_SUCCESS );
        if ( sJudge == ID_FALSE )
        {
            sNextRow = ID_TRUE;
        }
        else
        {
            sNextRow = ID_FALSE;
        }

        if ( ( sJudge == ID_TRUE ) &&
             ( ( aCodePlan->flag & QMNC_CNBY_IGNORE_LOOP_MASK )
               == QMNC_CNBY_IGNORE_LOOP_TRUE ) )
        {
            sIsAllSame = ID_TRUE;
            for ( sNode = aCodePlan->priorNode;
                  sNode != NULL;
                  sNode = (qtcNode *)sNode->node.next )
            {
                sColumn = &aTemplate->tmplate.rows[sNode->node.table].columns[sNode->node.column];

                sRow1 = (void *)mtc::value( sColumn, aDataPlan->priorTuple->row, MTD_OFFSET_USE );
                sRow2 = (void *)mtc::value( sColumn, aDataPlan->plan.myTuple->row, MTD_OFFSET_USE );

                sRow1Size = sColumn->module->actualSize( sColumn, sRow1 );
                sRow2Size = sColumn->module->actualSize( sColumn, sRow2 );
                if ( sRow1Size == sRow2Size )
                {
                    if ( idlOS::memcmp( sRow1, sRow2, sRow1Size )
                         == 0 )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsAllSame = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsAllSame = ID_FALSE;
                    break;
                }
            }

            if ( sIsAllSame == ID_TRUE )
            {
                sNextRow = ID_TRUE;
                sJudge = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if ( sJudge == ID_TRUE )
        {
            /* 6. Check HIER LOOP */
            IDE_TEST( checkLoop( aCodePlan,
                                 aDataPlan,
                                 aDataPlan->plan.myTuple->row,
                                 &sLoop )
                      != IDE_SUCCESS );

            /* 7. Store Position and lastKey and Row Pointer */
            if ( sLoop == ID_FALSE )
            {
                IDE_TEST( qmcSortTemp::storeCursor( aDataPlan->baseMTR,
                                                    &aNewItem->pos )
                          != IDE_SUCCESS );
                IDE_TEST( qmcSortTemp::getLastKey( aDataPlan->baseMTR,
                                                   &aNewItem->lastKey )
                          != IDE_SUCCESS );
                aNewItem->rowPtr = aDataPlan->plan.myTuple->row;
            }
            else
            {
                sNextRow = ID_TRUE;
            }
        }
        else
        {
            aDataPlan->plan.myTuple->row = sPrevRow;

            IDE_TEST( setBaseTupleSet( aTemplate,
                                       aDataPlan,
                                       aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        /* 8. Search Next Row Pointer */
        if ( sNextRow == ID_TRUE )
        {
            IDE_TEST( qmcSortTemp::getNextRange( aDataPlan->baseMTR,
                                                 &sSearchRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sSearchRow = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

