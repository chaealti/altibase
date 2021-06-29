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
 * $Id: qmcSortTempTable.cpp 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Description :
 *     Sort Temp Table�� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/

#include <qmoKeyRange.h>
#include <qmcSortTempTable.h>
#include <qmxResultCache.h>

IDE_RC
qmcSortTemp::init( qmcdSortTemp * aTempTable,
                   qcTemplate   * aTemplate,
                   UInt           aMemoryIdx,
                   qmdMtrNode   * aRecordNode,
                   qmdMtrNode   * aSortNode,
                   SDouble        aStoreRowCount,
                   UInt           aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Sort Temp Table�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *    Memory Temp Table�� Disk Temp Table�� ����� �����Ͽ�
 *    �׿� �´� �ʱ�ȭ�� �����Ѵ�.
 *
 *    BUG-38290
 *    Temp table �� ���ο� qcTemplate �� qmcMemory �� ������
 *    temp table ������ ����Ѵ�.
 *    �� �ΰ����� temp table �� init �� ���� template �� �� template ��
 *    ����� QMX memory �̴�.
 *    ���� temp table init ������ temp table build ������ ���� �ٸ�
 *    template �� ����ؾ� �Ѵٸ� �� ������ ����Ǿ�� �Ѵ�.
 *    Parallel query ����� �����ϸ鼭 temp table build �� parallel ��
 *    ����� ��� �� ������ ����ؾ� �Ѵ�.
 *
 *    Temp table build �� temp table �� �����ϴ� plan node ��
 *    init ������ ����ȴ�.
 *    ������ ���� parallel query �� partitioned table �̳� HASH, SORT,
 *    GRAG  ��忡�� PRLQ ��带 ����, parallel �����ϹǷ� 
 *    temp table �� �����ϴ� plan node �� ���� init  ���� �ʴ´�.
 *
 *    �ٸ� subqeury filter ������ temp table �� ����� ���� ���ܰ� ���� ��
 *    ������, subquery filter �� plan node �� ����� �� �ʱ�ȭ �ǹǷ�
 *    ���ÿ� temp table  �� �����Ǵ� ���� ����.
 *    (qmcTempTableMgr::addTempTable �� ���� �ּ� ����)
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::init"));

    mtcColumn  * sColumn;
    UInt         i;

    // ���ռ� �˻�
    IDE_DASSERT( aRecordNode != NULL );

    aTempTable->flag       = aFlag;
    aTempTable->mTemplate  = aTemplate;
    aTempTable->recordNode = aRecordNode;
    aTempTable->sortNode   = aSortNode;
    
    IDE_DASSERT( aTempTable->recordNode->dstTuple->rowOffset > 0 );
    aTempTable->mtrRowSize = aTempTable->recordNode->dstTuple->rowOffset;
    aTempTable->nullRowSize = aTempTable->mtrRowSize;

    aTempTable->recordCnt = 0;

    aTempTable->memoryMgr = NULL;
    aTempTable->nullRow = NULL;
    aTempTable->range = NULL;
    aTempTable->rangeArea = NULL;
    aTempTable->rangeAreaSize = 0;
    aTempTable->memoryTemp = NULL;
    aTempTable->diskTemp = NULL;

    aTempTable->existTempType = ID_FALSE;
    aTempTable->insertRow = NULL;

    aTempTable->memoryIdx = aMemoryIdx;
    if ( aMemoryIdx == ID_UINT_MAX )
    {
        aTempTable->memory = aTemplate->stmt->qmxMem;
    }
    else
    {
        aTempTable->memory = aTemplate->resultCache.memArray[aMemoryIdx];
    }

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //    1. Null Row ����
        //    2. Memory Sort Temp Table �ʱ�ȭ
        //    3. Record�� Memory ������ �ʱ�ȭ
        //-----------------------------------------

        // Memory Sort Temp Table ��ü�� ���� �� �ʱ�ȭ
        IDU_FIT_POINT( "qmcSortTemp::init::alloc::memoryTemp",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->memory->alloc( ID_SIZEOF(qmcdMemSortTemp),
                                             (void**)& aTempTable->memoryTemp )
                  != IDE_SUCCESS );

        IDE_TEST( qmcMemSort::init( aTempTable->memoryTemp,
                                    aTempTable->mTemplate,
                                    aTempTable->memory,
                                    aTempTable->sortNode ) != IDE_SUCCESS );

        // Record ���� �Ҵ��� ���� �޸� ������ �ʱ�ȭ
        IDU_FIT_POINT( "qmcSortTemp::init::alloc::memoryMgr",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->memory->alloc(
                  ID_SIZEOF(qmcMemory),
                  (void**) & aTempTable->memoryMgr ) != IDE_SUCCESS);

        aTempTable->memoryMgr = new (aTempTable->memoryMgr)qmcMemory();

        /* BUG-38290 */
        aTempTable->memoryMgr->init( aTempTable->mtrRowSize );

        // PROJ-2362 memory temp ���� ȿ���� ����
        for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
              i < aTempTable->recordNode->dstTuple->columnCount;
              i++, sColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
            {
                aTempTable->existTempType = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
            
        // �̸� ���۸� �Ҵ��Ѵ�.
        if ( aTempTable->existTempType == ID_TRUE )
        {
            /* BUG-38290 */
            IDE_TEST( aTempTable->memoryMgr->cralloc(
                    aTempTable->memory,
                    aTempTable->mtrRowSize,
                    &(aTempTable->insertRow) )
                != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }

        // Memory Temp Table�� ���� Null Row�� �����Ѵ�.
        IDE_TEST( makeMemNullRow( aTempTable ) != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Temp Table�� ���� Null Row ������
        // Disk Temp Table���� �ϸ�,
        // Null Row�� ȹ���� Disk Temp Table�κ��� ���´�.

        // Disk Sort Temp Table ��ü�� ���� �� �ʱ�ȭ
        IDU_FIT_POINT( "qmcSortTemp::init::alloc::diskTemp",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                  ID_SIZEOF(qmcdDiskSortTemp),
                  (void**)& aTempTable->diskTemp ) != IDE_SUCCESS);

        IDE_TEST( qmcDiskSort::init( aTempTable->diskTemp,
                                     aTempTable->mTemplate,
                                     aTempTable->recordNode,
                                     aTempTable->sortNode,
                                     aStoreRowCount,
                                     aTempTable->mtrRowSize,
                                     aTempTable->flag )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::clear( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     Temp Table���� ��� �����͸� �����ϰ�, �ʱ�ȭ�Ѵ�.
 *     Dependency ���濡 ���Ͽ� �߰� ����� �籸���� �ʿ䰡 ���� ��,
 *     ���ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::clear"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::clear"));

    aTempTable->recordCnt = 0;

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //     1. Record�� ���� �Ҵ�� ���� ����
        //     2. Memory Sort Temp Table �� clear
        //-----------------------------------------

        // Memory Sort Temp Table�� clear
        IDE_TEST( qmcMemSort::clear( aTempTable->memoryTemp )
                  != IDE_SUCCESS );
            
        // �̸� ���۸� �Ҵ��Ѵ�.
        if ( aTempTable->existTempType == ID_TRUE )
        {
            /* BUG-38290 */
            IDE_TEST( aTempTable->memoryMgr->cralloc(
                    aTempTable->memory,
                    aTempTable->mtrRowSize,
                    &(aTempTable->insertRow) )
                != IDE_SUCCESS);
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Sort Temp Table�� clear
        IDE_TEST( qmcDiskSort::clear( aTempTable->diskTemp )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::clearHitFlag( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     ��� Record�� Hit Flag�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::clearHitFlag"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::clearHitFlag"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::clearHitFlag( aTempTable->memoryTemp )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::clearHitFlag( aTempTable->diskTemp )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::alloc( qmcdSortTemp  * aTempTable,
                    void         ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Record�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::alloc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::alloc"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            *aRow = aTempTable->insertRow;
        }
        else
        {
            IDU_FIT_POINT( "qmcSortTemp::alloc::alloc::aRow",
                            idERR_ABORT_InsufficientMemory );

            /* BUG-38290 */
            IDE_TEST( aTempTable->memoryMgr->alloc(
                    aTempTable->memory,
                    aTempTable->mtrRowSize,
                    aRow )
                != IDE_SUCCESS);
           
            // PROJ-2462 ResultCache
            // ResultCache�� ���Ǹ� CacheMemory �� qmxMemory�� �հԷ�
            // ExecuteMemoryMax�� üũ�Ѵ�.
            if ( ( aTempTable->mTemplate->resultCache.count > 0 ) &&
                 ( ( aTempTable->mTemplate->resultCache.flag & QC_RESULT_CACHE_MAX_EXCEED_MASK )
                   == QC_RESULT_CACHE_MAX_EXCEED_FALSE ) &&
                 ( ( aTempTable->mTemplate->resultCache.flag & QC_RESULT_CACHE_DATA_ALLOC_MASK )
                   == QC_RESULT_CACHE_DATA_ALLOC_TRUE ) )
            {
                IDE_TEST( qmxResultCache::checkExecuteMemoryMax( aTempTable->mTemplate,
                                                                 aTempTable->memoryIdx )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Temp Table�� ����ϴ� ���
        // ������ Memory ������ �Ҵ���� �ʰ� ó���� �Ҵ���
        // �޸� ������ �ݺ������� ����Ѵ�.

        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::addRow( qmcdSortTemp * aTempTable,
                     void         * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� Record�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::addRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::addRow"));

    qmcMemSortElement * sElement;
    void              * sRow;

    IDE_DASSERT( aRow != NULL );

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            IDE_TEST( makeTempTypeRow( aTempTable,
                                       aRow,
                                       & sRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sRow = aRow;
        }
        
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //    1. Hit Flag�� �ʱ�ȭ�Ѵ�.
        //    2. Memory Temp Table�� �����Ѵ�.
        //-----------------------------------------

        // Hit Flag�� �ʱ�ȭ�Ѵ�.
        sElement = (qmcMemSortElement*) sRow;

        sElement->flag  = QMC_ROW_INITIALIZE;
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;

        // Memory Temp Table�� �����Ѵ�.
        IDE_TEST( qmcMemSort::attach( aTempTable->memoryTemp, sRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::insert( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    aTempTable->recordCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::makeTempTypeRow( qmcdSortTemp  * aTempTable,
                              void          * aRow,
                              void         ** aExtRow )
{
/***********************************************************************
 *
 * Description :
 *    �Ϲ� memory row�� memory Ȯ�� row ���·� �����.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::makeTempTypeRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::makeTempTypeRow"));

    mtcColumn  * sColumn;
    UInt         sRowSize;
    UInt         sActualSize;
    void       * sExtRow;
    idBool       sIsTempType;
    UInt         i;

    sRowSize = aTempTable->mtrRowSize;
    
    for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
          i < aTempTable->recordNode->dstTuple->columnCount;
          i++, sColumn++ )
    {
        if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
        {
            sActualSize = sColumn->module->actualSize( sColumn,
                                                       sColumn->column.value );
            
            sRowSize = idlOS::align( sRowSize, sColumn->module->align );
            sRowSize += sActualSize;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    sRowSize = idlOS::align8( sRowSize );

    IDE_DASSERT( aTempTable->mtrRowSize < sRowSize );

    /* BUG-38290 */
    IDE_TEST( aTempTable->memoryMgr->alloc(
            aTempTable->mTemplate->stmt->qmxMem,
            sRowSize,
            & sExtRow )
        != IDE_SUCCESS);

    // fixed row ����
    idlOS::memcpy( (SChar*)sExtRow, (SChar*)aRow,
                   aTempTable->mtrRowSize );

    sRowSize = aTempTable->mtrRowSize;
    
    for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
          i < aTempTable->recordNode->dstTuple->columnCount;
          i++, sColumn++ )
    {
        // offset ����
        if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
             == SMI_COLUMN_TYPE_TEMP_1B )
        {
            sIsTempType = ID_TRUE;
                
            sRowSize = idlOS::align( sRowSize, sColumn->module->align );
            IDE_DASSERT( sRowSize < 256 );
                
            *(UChar*)((UChar*)sExtRow + sColumn->column.offset) = (UChar)sRowSize;
        }
        else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_2B )
        {
            sIsTempType = ID_TRUE;
                
            sRowSize = idlOS::align( sRowSize, sColumn->module->align );
            IDE_DASSERT( sRowSize < 65536 );

            *(UShort*)((UChar*)sExtRow + sColumn->column.offset) = (UShort)sRowSize;
        }
        else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                  == SMI_COLUMN_TYPE_TEMP_4B )
        {
            sIsTempType = ID_TRUE;
                
            sRowSize = idlOS::align( sRowSize, sColumn->module->align );
            
            *(UInt*)((UChar*)sExtRow + sColumn->column.offset) = sRowSize;
        }
        else
        {
            sIsTempType = ID_FALSE;
        }

        if ( sIsTempType == ID_TRUE )
        {
            sActualSize = sColumn->module->actualSize( sColumn,
                                                       sColumn->column.value );

            idlOS::memcpy( (SChar*)sExtRow + sRowSize,
                           (SChar*)sColumn->column.value,
                           sActualSize );
            
            sRowSize += sActualSize;
        }
        else
        {
            // Nothing to do.
        }
    }

    *aExtRow = sExtRow;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::sort( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     ����� Row���� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::sort"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------
        IDE_TEST( qmcMemSort::sort( aTempTable->memoryTemp )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------
        // PROJ-1431 : bottom-up index build�� ���� row�� ��� ä�� ��
        // index(sparse cluster b-tree)�� build��
        if( aTempTable->diskTemp->sortNode != NULL )
        {
            IDE_TEST( qmcDiskSort::sort( aTempTable->diskTemp )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::shiftAndAppend( qmcdSortTemp * aTempTable,
                             void         * aRow,
                             void        ** aReturnRow )
{
/***********************************************************************
 *
 * Description :
 *     Limit Sorting�� �����Ѵ�.
 *
 * Implementation :
 *     Memory Temp Table�� ��츸 ��ȿ��
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::shiftAndAppend"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void   * sRow;
    
    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            IDE_TEST( makeTempTypeRow( aTempTable,
                                       aRow,
                                       & sRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sRow = aRow;
        }
        
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::shiftAndAppend( aTempTable->memoryTemp,
                                              sRow,
                                              aReturnRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Memory Temp Table������ ��� ������.
        IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::changeMinMax( qmcdSortTemp * aTempTable,
                           void         * aRow,
                           void        ** aReturnRow )
{
/***********************************************************************
 *
 * Description :
 *     Min-Max ������ ���� Limit Sorting�� �����Ѵ�.
 *
 * Implementation :
 *     Memory Temp Table�� ��츸 ��ȿ��
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::changeMinMax"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void * sRow;
    
    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            IDE_TEST( makeTempTypeRow( aTempTable,
                                       aRow,
                                       & sRow )
                      != IDE_SUCCESS );
        }
        else
        {
            sRow = aRow;
        }
        
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::changeMinMax( aTempTable->memoryTemp,
                                            sRow,
                                            aReturnRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Memory Temp Table������ ��� ������.
        IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::getFirstSequence( qmcdSortTemp * aTempTable,
                               void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getFirstSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getFirstSequence( aTempTable->memoryTemp,
                                                aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getFirstSequence( aTempTable->diskTemp,
                                                 aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcSortTemp::getNextSequence( qmcdSortTemp * aTempTable,
                              void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getNextSequence"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::getNextSequence"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getNextSequence( aTempTable->memoryTemp,
                                               aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getNextSequence( aTempTable->diskTemp,
                                                aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcSortTemp::getFirstRange( qmcdSortTemp * aTempTable,
                            qtcNode      * aRangePredicate,
                            void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Range �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getFirstRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::getFirstRange"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Key Range�� ����
        IDE_TEST( makeMemKeyRange( aTempTable, aRangePredicate )
                  != IDE_SUCCESS );

        // ������ Key Range�� �̿��� �˻�
        IDE_TEST( qmcMemSort::getFirstRange( aTempTable->memoryTemp,
                                             aTempTable->range,
                                             aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Key Range�� ������ ���ؼ��� Index �������� �ʿ��ϴ�.
        // ����, Disk Temp Table������ Key Range�� �����Ͽ�
        // ó���Ѵ�.
        IDE_TEST( qmcDiskSort::getFirstRange( aTempTable->diskTemp,
                                              aRangePredicate,
                                              aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmcSortTemp::getLastKey( qmcdSortTemp * aTempTable,
                                SLong        * aLastKey )
{
    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        *aLastKey = aTempTable->memoryTemp->mLastKey;
    }
    else
    {
        IDE_RAISE( ERR_INTERNAL );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmcSortTemp::getLastKey",
                                  "unsupported feature" ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC qmcSortTemp::setLastKey( qmcdSortTemp * aTempTable,
                                SLong          aLastKey )
{
    if ( ( aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE )
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        aTempTable->memoryTemp->mLastKey = aLastKey;
    }
    else
    {
        IDE_RAISE( ERR_INTERNAL );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmcSortTemp::setLastKey",
                                  "unsupported feature" ));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC
qmcSortTemp::getNextRange( qmcdSortTemp * aTempTable,
                           void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ���� Range �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getNextRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::getNextRange"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // �̹� ������ Key Range�� �̿��� �˻�
        IDE_TEST( qmcMemSort::getNextRange( aTempTable->memoryTemp, aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getNextRange( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qmcSortTemp::getFirstHit( qmcdSortTemp * aTempTable,
                                 void        ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *     ù��° Hit �� Row�˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getFirstHit( aTempTable->memoryTemp, aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getFirstHit( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmcSortTemp::getNextHit( qmcdSortTemp * aTempTable,
                                void        ** aRow )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *     ���� Hit �� Row�˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getNextHit( aTempTable->memoryTemp, aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getNextHit( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcSortTemp::getFirstNonHit( qmcdSortTemp * aTempTable,
                             void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Hit���� ���� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getFirstNonHit( aTempTable->memoryTemp, aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getFirstNonHit( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcSortTemp::getNextNonHit( qmcdSortTemp * aTempTable,
                            void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Hit���� ���� Record�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcMemSort::getNextNonHit( aTempTable->memoryTemp, aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getNextNonHit( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcSortTemp::getNullRow( qmcdSortTemp * aTempTable,
                         void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    NULL Row�� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    qmdMtrNode * sNode;
    
    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_DASSERT( aTempTable->nullRow != NULL );
        
        *aRow = aTempTable->nullRow;
        
        // PROJ-2362 memory temp ���� ȿ���� ����
        for ( sNode = aTempTable->recordNode;
              sNode != NULL;
              sNode = sNode->next )
        {
            sNode->func.makeNull( sNode,
                                  aTempTable->nullRow );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getNullRow( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcSortTemp::setHitFlag( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    ���� �о Record�� Hit Flag�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    qmcMemSortElement * sElement;

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //    1. ���� �а� �ִ� Record�� ã�´�.
        //    2. �ش� Record�� Hit Flag�� setting�Ѵ�.
        //-----------------------------------------

        // ���� �а� �ִ� Record �˻�
        sElement = (qmcMemSortElement*) aTempTable->recordNode->dstTuple->row;

        // �ش� Record�� Hit Flag Setting
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_TRUE;
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::setHitFlag( aTempTable->diskTemp )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmcSortTemp::isHitFlagged( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description : PROJ-2385
 *    ���� �о Record�� Hit Flag�� �ִ��� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcMemSortElement * sElement;
    idBool              sIsHitFlagged = ID_FALSE;

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //----------------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //    1. ���� �а� �ִ� Record�� ã�´�.
        //    2. �ش� Record�� Hit Flag�� �ִ��� �Ǵ��Ѵ�.
        //----------------------------------------------

        // ���� �а� �ִ� Record �˻�
        sElement = (qmcMemSortElement*) aTempTable->recordNode->dstTuple->row;

        // �ش� Record�� Hit Flag�� �ִ��� �Ǵ�
        if ( ( sElement->flag & QMC_ROW_HIT_MASK ) == QMC_ROW_HIT_TRUE )
        {
            sIsHitFlagged = ID_TRUE;
        }
        else
        {
            sIsHitFlagged = ID_FALSE;
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        sIsHitFlagged = qmcDiskSort::isHitFlagged( aTempTable->diskTemp );
    }

    return sIsHitFlagged;
}

IDE_RC
qmcSortTemp::storeCursor( qmcdSortTemp     * aTempTable,
                          smiCursorPosInfo * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *    Merge Join�� ���� ������� ���� Cursor�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::storeCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::storeCursor"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Cursor Index ����
        IDE_TEST( qmcMemSort::getCurrPosition(
                      aTempTable->memoryTemp,
                      & aCursorInfo->mCursor.mTRPos.mIndexPos )
                  != IDE_SUCCESS );

        // Row Pointer ����
        aCursorInfo->mCursor.mTRPos.mRowPtr =
            aTempTable->recordNode->dstTuple->row;
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getCurrPosition( aTempTable->diskTemp,
                                                aCursorInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcSortTemp::restoreCursor( qmcdSortTemp     * aTempTable,
                            smiCursorPosInfo * aCursorInfo )
{
/***********************************************************************
 *
 * Description :
 *    Merge Join�� ���� ������� ������ Cursor�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::restoreCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::restoreCursor"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Cursor Index ����
        IDE_TEST( qmcMemSort::setCurrPosition(
                      aTempTable->memoryTemp,
                      aCursorInfo->mCursor.mTRPos.mIndexPos )
                  != IDE_SUCCESS );

        // Row Pointer ����
        aTempTable->recordNode->dstTuple->row =
            aCursorInfo->mCursor.mTRPos.mRowPtr;
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::setCurrPosition( aTempTable->diskTemp,
                                                aCursorInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmcSortTemp::getCursorInfo( qmcdSortTemp     * aTempTable,
                            void            ** aTableHandle,
                            void            ** aIndexHandle,
                            qmcdMemSortTemp ** aMemSortTemp,
                            qmdMtrNode      ** aMemSortRecord )
{
/***********************************************************************
 *
 * Description :
 *    View SCAN��� �����ϱ� ���� ���� ����
 *
 * Implementation :
 *    Memory Temp Table�� ���, Memory Temp Table ��ü�� ����
 *    Disk Temp Table�� ���, Table Handle�� Index Handle�� ������.
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getCursorInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::getCursorInfo"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Memory Temp Table ��ü�� �ش�.
        *aTableHandle = NULL;
        *aIndexHandle = NULL;
        *aMemSortTemp = aTempTable->memoryTemp;

        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            *aMemSortRecord = aTempTable->recordNode;
        }
        else
        {
            *aMemSortRecord = NULL;
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Temp Table�� Handle�� �ش�.
        IDE_TEST( qmcDiskSort::getCursorInfo( aTempTable->diskTemp,
                                              aTableHandle,
                                              aIndexHandle )
                  != IDE_SUCCESS );

        *aMemSortTemp   = NULL;
        *aMemSortRecord = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::getDisplayInfo( qmcdSortTemp * aTempTable,
                             ULong        * aDiskPageCount,
                             SLong        * aRecordCount )
{
/***********************************************************************
 *
 * Description :
 *    Plan Display�� ���� ������ ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::getDisplayInfo"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::getDisplayInfo"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        *aDiskPageCount = 0;
        *aRecordCount = aTempTable->recordCnt;
    }
    else
    {
        //-----------------------------------------
        // Disk Sort Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskSort::getDisplayInfo( aTempTable->diskTemp,
                                               aDiskPageCount,
                                               aRecordCount )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::setSortNode( qmcdSortTemp     * aTempTable,
                          const qmdMtrNode * aSortNode )
/***********************************************************************
 *
 * Description :
 *    �ʱ�ȭ�ǰ�, �����Ͱ� �Էµ� Temp Table�� ����Ű�� �����Ѵ�.
 *    (Window Sort���� ���)
 *
 * Implementation :
 *    ���� Disk Sort Temp�� ��� �߰��� ����Ű ������ �Ұ��ϹǷ�
 *    Memory Sort�� ��츸 ������
 *
 ***********************************************************************/
{
#define IDE_FN "qmcSortTemp::setSortNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::setSortNode"));


    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_MEMORY )
    {
        IDE_TEST( qmcMemSort::setSortNode( aTempTable->memoryTemp,
                                           aSortNode )
                  != IDE_SUCCESS );
    }
    else
    {

        /********************************************************************
         * PROJ-2201 Innovation in sorting and hashing(temp)
         ********************************************************************/
        IDE_ERROR ((aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
                    == QMCD_SORT_TMP_STORAGE_DISK );

        IDE_TEST( qmcDiskSort::setSortNode( aTempTable->diskTemp,
                                            aSortNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::setUpdateColumnList( qmcdSortTemp     * aTempTable,
                                  const qmdMtrNode * aUpdateColumns )
/***********************************************************************
 *
 * Description :
 *    update�� ������ column list�� ����
 *    ����: Disk Sort Temp Table�� ���, hitFlag�� �Բ� ��� �Ұ�
 *
 * Implementation :
 *
 ***********************************************************************/
{
#define IDE_FN "qmcSortTemp::setUpdateColumnList"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::setUpdateColumnList"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_DISK )
    {
        IDE_TEST( qmcDiskSort::setUpdateColumnList( aTempTable->diskTemp,
                                                    aUpdateColumns )
                  != IDE_SUCCESS );
        
    }
    else
    {
        // Memory Sort Temp Table�� ���, �ǹ̰� ����
        // do nothing
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    

    
#undef IDE_FN
}

IDE_RC
qmcSortTemp::updateRow( qmcdSortTemp * aTempTable )
/***********************************************************************
 *
 * Description :
 *    ���� �˻� ���� ��ġ�� row�� update
 *    (Disk�� ��츸 �ǹ̰� ����)
 *
 * Implementation :
 *
 ***********************************************************************/
{
#define IDE_FN "qmcSortTemp::updateRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::updateRow"));

    if ( (aTempTable->flag & QMCD_SORT_TMP_STORAGE_TYPE)
         == QMCD_SORT_TMP_STORAGE_DISK )
    {
        IDE_TEST( qmcDiskSort::updateRow( aTempTable->diskTemp )
                  != IDE_SUCCESS );
        
    }
    else
    {
        // Memory Sort Temp Table�� ��� �ǹ̰� ����
        // do nothing
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    

#undef IDE_FN
}

IDE_RC
qmcSortTemp::makeMemNullRow( qmcdSortTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Memory Sort Temp Table�� ���� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    ���� �����ϴ� Column�� ���ؼ��� Null Value�� �����ϰ�,
 *    Pointer/RID���� �����ϴ� Column�� ���ؼ��� Null Value�� ��������
 *    �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::makeMemNullRow"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::makeMemNullRow"));

    qmdMtrNode * sNode;
    mtcColumn  * sColumn;
    UInt         sRowSize;
    UInt         sActualSize;
    idBool       sIsTempType;
    UInt         i;

    // PROJ-2362 memory temp ���� ȿ���� ����
    if ( aTempTable->existTempType == ID_TRUE )
    {
        sRowSize = aTempTable->mtrRowSize;
        
        for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
              i < aTempTable->recordNode->dstTuple->columnCount;
              i++, sColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
            {
                sActualSize = sColumn->module->actualSize( sColumn,
                                                           sColumn->module->staticNull );
            
                sRowSize = idlOS::align( sRowSize, sColumn->module->align );
                sRowSize += sActualSize;
            }
            else
            {
                // Nothing to do.
            }
        }

        aTempTable->nullRowSize = idlOS::align8( sRowSize );

        // Null Row�� ���� ���� �Ҵ�
        IDU_FIT_POINT( "qmcSortTemp::makeMemNullRow::cralloc::nullRow",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->memory->cralloc( aTempTable->nullRowSize,
                                               (void**) & aTempTable->nullRow )
                  != IDE_SUCCESS);

        sRowSize = aTempTable->mtrRowSize;
        
        for ( i = 0, sColumn = aTempTable->recordNode->dstTuple->columns;
              i < aTempTable->recordNode->dstTuple->columnCount;
              i++, sColumn++ )
        {
            // offset ����
            if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                 == SMI_COLUMN_TYPE_TEMP_1B )
            {
                sIsTempType = ID_TRUE;
                
                sRowSize = idlOS::align( sRowSize, sColumn->module->align );
                IDE_DASSERT( sRowSize < 256 );
                
                *(UChar*)((UChar*)aTempTable->nullRow + sColumn->column.offset) = (UChar)sRowSize;
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_2B )
            {
                sIsTempType = ID_TRUE;
                
                sRowSize = idlOS::align( sRowSize, sColumn->module->align );
                IDE_DASSERT( sRowSize < 65536 );

                *(UShort*)((UChar*)aTempTable->nullRow + sColumn->column.offset) = (UShort)sRowSize;
            }
            else if ( ( sColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_TEMP_4B )
            {
                sIsTempType = ID_TRUE;
                
                sRowSize = idlOS::align( sRowSize, sColumn->module->align );
            
                *(UInt*)((UChar*)aTempTable->nullRow + sColumn->column.offset) = sRowSize;
            }
            else
            {
                sIsTempType = ID_FALSE;
            }

            if ( sIsTempType == ID_TRUE )
            {
                sActualSize = sColumn->module->actualSize( sColumn,
                                                           sColumn->module->staticNull );

                idlOS::memcpy( (SChar*)aTempTable->nullRow + sRowSize,
                               (SChar*)sColumn->module->staticNull,
                               sActualSize );
            
                sRowSize += sActualSize;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Null Row�� ���� ���� �Ҵ�
        IDU_LIMITPOINT("qmcSortTemp::makeMemNullRow::malloc");
        IDE_TEST( aTempTable->memory->cralloc( aTempTable->nullRowSize,
                                               (void**) & aTempTable->nullRow )
                  != IDE_SUCCESS);
    }

    for ( sNode = aTempTable->recordNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        //-----------------------------------------------
        // ���� ���� �����ϴ� Column�� ���ؼ���
        // NULL Value�� �����Ѵ�.
        //-----------------------------------------------

        sNode->func.makeNull( sNode,
                              aTempTable->nullRow );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmcSortTemp::makeMemKeyRange( qmcdSortTemp * aTempTable,
                              qtcNode      * aRangePredicate )
{
/***********************************************************************
 *
 * Description :
 *    Memory Temp Table�� ���� Key Range�� �����Ѵ�.
 *
 * Implementation :
 *    ������ ���� ������ Key Range�� �����Ѵ�.
 *    - Key Range�� ũ�� ���
 *    - Key Range�� ���� ���� Ȯ��
 *    - Key Range ����
 ***********************************************************************/

#define IDE_FN "qmcSortTemp::makeMemKeyRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qmcSortTemp::makeMemKeyRange"));

    qtcNode * sFilter;
    UInt sKeyColsFlag;
    UInt sCompareType;

    // Key Range�� ũ�� ��� �� ���� Ȯ��
    if ( aTempTable->rangeArea == NULL )
    {
        // Sort Temp Table�� Key Range�� �� Column�� ���ؼ��� ������

        // Key Range�� Size���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTempTable->mTemplate,
                                           aRangePredicate,
                                           &aTempTable->rangeAreaSize )
            != IDE_SUCCESS);

        // Key Range�� ���� ���� �Ҵ�
        IDU_FIT_POINT( "qmcSortTemp::makeMemKeyRange::alloc::rangeArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( aTempTable->memory->cralloc( aTempTable->rangeAreaSize,
                                               (void**)& aTempTable->rangeArea )
                  != IDE_SUCCESS);
    }
    else
    {
        // �̹� �Ҵ�Ǿ� ����
        // Nothing To Do
    }

    // Key Range ����

    if ( (aTempTable->sortNode->myNode->flag & QMC_MTR_SORT_ORDER_MASK)
         == QMC_MTR_SORT_ASCENDING )
    {
        sKeyColsFlag = SMI_COLUMN_ORDER_ASCENDING;
    }
    else
    {
        sKeyColsFlag = SMI_COLUMN_ORDER_DESCENDING;
    }

    // Memory Temp Table�� key range�� MT Ÿ�԰��� compare
    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;

    // To Fix PR-8109
    // Key Range ������ ���ؼ��� �� ����� �Ǵ�
    // Column�� �Է� ���ڷ� ����Ͽ��� �Ѵ�.
    // Key Range ����
    IDE_TEST(
        qmoKeyRange::makeKeyRange( aTempTable->mTemplate,
                                   aRangePredicate,
                                   1,
                                   aTempTable->sortNode->func.compareColumn,
                                   & sKeyColsFlag,
                                   sCompareType,
                                   aTempTable->rangeArea,
                                   aTempTable->rangeAreaSize,
                                   & (aTempTable->range),
                                   & sFilter ) != IDE_SUCCESS );

    // ���ռ� �˻�
    // �ݵ�� Range ������ �����ؾ� ��
    IDE_DASSERT( sFilter == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
