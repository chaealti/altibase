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
 * $Id: qmcHashTempTable.cpp 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *     Hash Temp Table�� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 *
 **********************************************************************/
#include <idl.h> // for win32 porting
#include <qmcHashTempTable.h>
#include <qmxResultCache.h>

IDE_RC
qmcHashTemp::init( qmcdHashTemp * aTempTable,
                   qcTemplate   * aTemplate,
                   UInt           aMemoryIdx,
                   qmdMtrNode   * aRecordNode,
                   qmdMtrNode   * aHashNode,
                   qmdMtrNode   * aAggrNode,
                   UInt           aBucketCnt,
                   UInt           aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Hash Temp Table�� �ʱ�ȭ�Ѵ�.
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

    idBool       sIsDistinct;
    idBool       sIsPrimary;
    mtcColumn  * sColumn;
    UInt         i;

    // ���ռ� �˻�
    IDE_DASSERT( aTempTable != NULL );
    IDE_DASSERT( aRecordNode != NULL );

    aTempTable->flag = aFlag;
    aTempTable->mTemplate = aTemplate;
    aTempTable->recordNode = aRecordNode;
    aTempTable->hashNode = aHashNode;
    aTempTable->aggrNode = aAggrNode;

    aTempTable->bucketCnt = aBucketCnt;

    IDE_DASSERT( aTempTable->recordNode->dstTuple->rowOffset > 0 );
    aTempTable->mtrRowSize = aTempTable->recordNode->dstTuple->rowOffset;
    aTempTable->nullRowSize = aTempTable->mtrRowSize;

    aTempTable->memoryMgr = NULL;
    //  BUG-10511
    aTempTable->nullRow = NULL;
    aTempTable->hashKey = 0;

    aTempTable->memoryTemp = NULL;
    aTempTable->memoryPartTemp = NULL;
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
    // Distinct ���� ����
    if ( (aTempTable->flag & QMCD_HASH_TMP_DISTINCT_MASK)
         == QMCD_HASH_TMP_DISTINCT_TRUE )
    {
        sIsDistinct = ID_TRUE;
    }
    else
    {
        sIsDistinct = ID_FALSE;
    }

    // Primary ���� ����
    if ( (aTempTable->flag & QMCD_HASH_TMP_PRIMARY_MASK)
         == QMCD_HASH_TMP_PRIMARY_TRUE )
    {
        sIsPrimary = ID_TRUE;
    }
    else
    {
        sIsPrimary = ID_FALSE;
    }

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //    1. Memory Hash Temp Table �ʱ�ȭ
        //    2. Null Row ����
        //    3. Record�� Memory ������ �ʱ�ȭ
        //-----------------------------------------

        // Memory Temp Table�� ���� Null Row�� �����Ѵ�.

        if ( aTempTable->hashNode != NULL )
        {
            // Memory Hash Temp Table ��ü�� ���� �� �ʱ�ȭ
            if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                 == QMCD_HASH_TMP_HASHING_PARTITIONED )
            {
                IDU_LIMITPOINT("qmcHashTemp::init::malloc1");
                IDE_TEST( aTempTable->memory->alloc( ID_SIZEOF( qmcdMemPartHashTemp ),
                                                     (void**)& aTempTable->memoryPartTemp )
                          != IDE_SUCCESS );

                IDE_TEST( qmcMemPartHash::init( aTempTable->memoryPartTemp,
                                                aTempTable->mTemplate,
                                                aTempTable->memory,
                                                aTempTable->recordNode,
                                                aTempTable->hashNode,
                                                aTempTable->bucketCnt ) != IDE_SUCCESS );
            }
            else
            {
                IDU_FIT_POINT( "qmcHashTemp::init::alloc::memoryTemp",
                                idERR_ABORT_InsufficientMemory );

                IDE_TEST( aTempTable->memory->alloc( ID_SIZEOF(qmcdMemHashTemp),
                                                     (void**)& aTempTable->memoryTemp )
                          != IDE_SUCCESS);

                IDE_TEST( qmcMemHash::init( aTempTable->memoryTemp,
                                            aTempTable->mTemplate,
                                            aTempTable->memory,
                                            aTempTable->recordNode,
                                            aTempTable->hashNode,
                                            aTempTable->bucketCnt,
                                            sIsDistinct ) != IDE_SUCCESS );
            }
        }
        else
        {
            // To Fix PR-7960
            // GROUP BY�� ���� ��� �ݵ�� Memory Temp Table�� ����ϰ� �ǳ�,
            // ������ Memory Hash Temp Table�� �ʿ�� ������ �ʴ´�.
        }

        if ( sIsPrimary == ID_TRUE )
        {
            //----------------------------------
            // Primary Hash Temp Table�� ���
            //----------------------------------

            // Record ���� �Ҵ��� ���� �޸� ������ �ʱ�ȭ
            IDU_FIT_POINT( "qmcHashTemp::init::alloc::primaryMemoryMgr",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( aTempTable->memory->alloc( ID_SIZEOF(qmcMemory),
                                                (void**) & aTempTable->memoryMgr )
                      != IDE_SUCCESS);
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

            // BUG-10512
            // primary hash temp table������ null row ����
            IDE_TEST( makeMemNullRow( aTempTable ) != IDE_SUCCESS );
        }
        else
        {
            //----------------------------------
            // Secondary Hash Temp Table�� ���
            //----------------------------------

            // Record ���� �Ҵ��� ���� �޸� ������ �ʱ�ȭ
            IDU_FIT_POINT( "qmcHashTemp::init::alloc::secondaryMemoryMgr",
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
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Temp Table�� ���� Null Row ������ Disk Temp Table���� �ϸ�,
        // Null Row�� ȹ���� Disk Temp Table�κ��� ���´�.

        // Disk Hash Temp Table ��ü�� ���� �� �ʱ�ȭ
        IDU_LIMITPOINT("qmcHashTemp::init::malloc3");
        IDE_TEST( aTempTable->mTemplate->stmt->qmxMem->alloc(
                ID_SIZEOF(qmcdDiskHashTemp),
                (void**)& aTempTable->diskTemp ) != IDE_SUCCESS);

        IDE_TEST( qmcDiskHash::init( aTempTable->diskTemp,
                                     aTempTable->mTemplate,
                                     aTempTable->recordNode,
                                     aTempTable->hashNode,
                                     aTempTable->aggrNode,
                                     aTempTable->bucketCnt,
                                     aTempTable->mtrRowSize,
                                     sIsDistinct )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::clear( qmcdHashTemp * aTempTable )
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

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //     1. Record�� ���� �Ҵ�� ���� ����
        //     2. Memory Hash Temp Table �� clear
        //-----------------------------------------

        // Record�� ���� �Ҵ�� ���� ����
        if ( (aTempTable->flag & QMCD_HASH_TMP_PRIMARY_MASK)
             == QMCD_HASH_TMP_PRIMARY_TRUE )
        {
            // Primary �� ���
            // To fix BUG-17591
            // �Ҵ�� ������ ���� �ϹǷ�
            // qmcMemory::clearForReuse�� ȣ���Ѵ�.
            aTempTable->memoryMgr->clearForReuse();
        }
        else
        {
            // Secondary�� ��� Memory �����ڰ� �������� �ʴ´�.
        }

        if ( aTempTable->hashNode != NULL )
        {
            // Memory Hash Temp Table�� clear
            if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                 == QMCD_HASH_TMP_HASHING_PARTITIONED )
            {
                IDE_TEST( qmcMemPartHash::clear( aTempTable->memoryPartTemp )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( qmcMemHash::clear( aTempTable->memoryTemp )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // To Fix PR-7960
            // GROUP BY�� ���� ��� �ݵ�� Memory Temp Table�� ����ϰ� �ǳ�,
            // ������ Memory Hash Temp Table�� �ʿ�� ������ �ʴ´�.

            // Nothing To Do
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
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Hash Temp Table�� clear
        IDE_TEST( qmcDiskHash::clear( aTempTable->diskTemp )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::clearHitFlag( qmcdHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *     ��� Record�� Hit Flag�� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::clearHitFlag( aTempTable->memoryPartTemp )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcMemHash::clearHitFlag( aTempTable->memoryTemp )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::clearHitFlag( aTempTable->diskTemp )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::alloc( qmcdHashTemp * aTempTable,
                    void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    Record�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            *aRow = aTempTable->insertRow;
        }
        else
        {
            IDU_FIT_POINT( "qmcHashTemp::alloc::cralloc::aRow",
                            idERR_ABORT_InsufficientMemory );

            /* BUG-38290 */
            IDE_TEST( aTempTable->memoryMgr->cralloc(
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
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk Temp Table�� ����ϴ� ���
        // ������ Memory ������ �Ҵ���� �ʰ� ó���� �Ҵ���
        // �޸� ������ �ݺ������� ����Ѵ�.

        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::addRow( qmcdHashTemp * aTempTable,
                     void         * aRow )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� Record�� �����Ѵ�.
 *    Non-Distinction Insert�� ���ȴ�.  ����, ������ ������ �� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;
    UInt                sHashKey;
    void              * sResultRow = NULL;
    void              * sRow       = NULL;
    idBool              sResult;

    // ���ռ� �˻�
    IDE_DASSERT( aRow != NULL );


    // Hash Key ȹ��
    IDE_TEST( getHashKey( aTempTable, aRow, & sHashKey )
              != IDE_SUCCESS );

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
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
        // Memory Hash Temp Table�� ����ϴ� ���
        //    1. Hit Flag �� ������踦 �ʱ�ȭ�Ѵ�.
        //    2. Hash Key ȹ��
        //    3. Memory Temp Table�� �����Ѵ�.
        //-----------------------------------------

        // Hit Flag�� �ʱ�ȭ�Ѵ�.
        sElement = (qmcMemHashElement*) sRow;

        sElement->flag  = QMC_ROW_INITIALIZE;
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;
        sElement->next  = NULL;

        // Memory Temp Table�� �����Ѵ�.
        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::insertAny( aTempTable->memoryPartTemp,
                                                 sHashKey,
                                                 sRow,
                                                 & sResultRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( aTempTable->memoryTemp->insert( aTempTable->memoryTemp,
                                                      sHashKey,
                                                      sRow,
                                                      & sResultRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // PROJ-1597 Temp record size ���� ����
        // hash key�� SM���� ������� �ʰ� QP�� �����ֵ��� �Ѵ�.
        IDE_TEST( qmcDiskHash::insert( aTempTable->diskTemp,
                                       sHashKey,
                                       & sResult )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::addDistRow( qmcdHashTemp  * aTempTable,
                         void         ** aRow,
                         idBool        * aResult )
{
/***********************************************************************
 *
 * Description :
 *    Temp Table�� Record�� �����Ѵ�.
 *    Distinction Insert�� ���ȴ�.  ������ ������ �� �ִ�.
 *
 * Implementation :
 *    Data ������ �õ��ϰ�, ���� ���п� ���� ����� ����
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;
    UInt                sHashKey;
    void              * sResultRow = NULL;
    void              * sRow       = NULL;

    // Hash Key ȹ��
    IDE_TEST( getHashKey( aTempTable, *aRow, & sHashKey )
              != IDE_SUCCESS );

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            IDE_TEST( makeTempTypeRow( aTempTable,
                                       *aRow,
                                       & sRow )
                      != IDE_SUCCESS );
            
            // ���� ������ row�� ��ü�Ѵ�.
            *aRow = sRow;
        }
        else
        {
            sRow = *aRow;
        }
        
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //    1. Hit Flag �� ������踦 �ʱ�ȭ�Ѵ�.
        //    2. Hash Key ȹ��
        //    3. Memory Temp Table�� �����Ѵ�.
        //-----------------------------------------

        // Hit Flag�� �ʱ�ȭ�Ѵ�.
        sElement = (qmcMemHashElement*) sRow;

        sElement->flag  = QMC_ROW_INITIALIZE;
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;
        sElement->next  = NULL;

        // Memory Temp Table�� �����Ѵ�.
        // PROJ-2553 Distinct Hashing������ 
        // Partitioned Array Hashing�� ����ؼ��� �� �ȴ�.
        IDE_DASSERT( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                        != QMCD_HASH_TMP_HASHING_PARTITIONED );

        IDE_TEST( aTempTable->memoryTemp->insert( aTempTable->memoryTemp,
                                                  sHashKey,
                                                  sRow,
                                                  & sResultRow )
                  != IDE_SUCCESS );

        if ( sResultRow == NULL )
        {
            // ���� ����
            *aResult = ID_TRUE;
        }
        else
        {
            // ���� ����
            *aResult = ID_FALSE;
        }
    }
    else
    {
        // PROJ-1597 Temp record size ���� ����
        // hash key�� SM���� ������� �ʰ� QP�� �����ֵ��� �Ѵ�.
        IDE_TEST( qmcDiskHash::insert( aTempTable->diskTemp,
                                       sHashKey,
                                       aResult )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::makeTempTypeRow( qmcdHashTemp  * aTempTable,
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
            aTempTable->memory,
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
}

IDE_RC
qmcHashTemp::updateAggr( qmcdHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� Update�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // �ش� Plan Node������ Record�� ���� Aggregation ��ü��
        // �̹� Temp Table�� �ݿ��Ǿ� ������ �۾��� �ʿ����.
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk�� ������ �ݿ��Ͽ��� �Ѵ�.
        IDE_TEST( qmcDiskHash::updateAggr( aTempTable->diskTemp )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::updateFiniAggr( qmcdHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Column�� ���� ���� Update�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // �ش� Plan Node������ Record�� ���� Aggregation ��ü��
        // �̹� Temp Table�� �ݿ��Ǿ� ������ �۾��� �ʿ����.
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Disk�� ������ �ݿ��Ͽ��� �Ѵ�.
        IDE_TEST( qmcDiskHash::updateFiniAggr( aTempTable->diskTemp )
                  != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::addNewGroup( qmcdHashTemp * aTempTable,
                          void         * aRow )
{
/***********************************************************************
 *
 * Description :
 *    To Fix PR-8213
 *       - Group Aggregation�� ���ؼ��� ���Ǹ�, ���ο� Group��
 *         Temp Table�� ����Ѵ�.
 *       - �̹� ������ Group�� ������ �ǴܵǾ��� ������ �ݵ��
 *         �����Ͽ��� �Ѵ�.
 *
 * Implementation :
 *
 *    - Memory Temp Table�� ���
 *      : ���� Group�� �˻��� ��, ���� Group�� ���� ��� ���ԵǾ� ����
 *      : ���� �ƹ��� �۾��� �������� �ʴ´�.
 *
 *    - Disk Temp Table�� ���
 *      : ���ο� Group�� �����Ѵ�.
 *      : �̹� ���ο� Group���� �Ǻ��Ǿ����Ƿ� �ݵ�� �����Ͽ��� ��.
 *
 ***********************************************************************/

    idBool sResult;
    UInt   sHashKey;

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // �ش� Plan Node������ Record�� ���� Aggregation ��ü��
        // �̹� Temp Table�� �ݿ��Ǿ� ������ �۾��� �ʿ����.
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // Hash Key ȹ��
        IDE_TEST( getHashKey( aTempTable, aRow, & sHashKey )
                  != IDE_SUCCESS );

        // Disk�� ���ο� Row�� �����Ѵ�.
        IDE_TEST( qmcDiskHash::insert( aTempTable->diskTemp,
                                       sHashKey,
                                       & sResult )
                  != IDE_SUCCESS );

        // �ݵ�� ������ �����ؾ� ��.
        IDE_DASSERT( sResult == ID_TRUE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::getFirstGroup( qmcdHashTemp * aTempTable,
                            void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Group ���� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // PROJ-2553 Grouping Hashing������ 
        // Partitioned Array Hashing�� ����ؼ��� �� �ȴ�.
        IDE_DASSERT( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                         != QMCD_HASH_TMP_HASHING_PARTITIONED );

        IDE_TEST( qmcMemHash::getFirstSequence( aTempTable->memoryTemp,
                                                aRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getFirstGroup( aTempTable->diskTemp,
                                              aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::getFirstSequence( qmcdHashTemp * aTempTable,
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

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::getFirstSequence( aTempTable->memoryPartTemp,
                                                        aRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcMemHash::getFirstSequence( aTempTable->memoryTemp,
                                                    aRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getFirstSequence( aTempTable->diskTemp,
                                                 aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::getFirstHit( qmcdHashTemp * aTempTable,
                          void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *     ù��° Hit �� Row�˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::getFirstHit( aTempTable->memoryPartTemp, aRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcMemHash::getFirstHit( aTempTable->memoryTemp, aRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getFirstHit( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::getFirstNonHit( qmcdHashTemp * aTempTable,
                             void        ** aRow )
{
/***********************************************************************
 *
 * Description :
 *    ù��° Non-Hit Row�˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::getFirstNonHit( aTempTable->memoryPartTemp, aRow )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcMemHash::getFirstNonHit( aTempTable->memoryTemp, aRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getFirstNonHit( aTempTable->diskTemp,
                                               aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::getSameRowAndNonHit( qmcdHashTemp * aTempTable,
                                  void         * aRow,
                                  void        ** aResultRow )
{
/***********************************************************************
 *
 * Description :
 *     �־��� Row�� ������ Row�̸鼭 Hit Flag�� Setting�� ���� ����
 *     Record�� �˻���.
 *     Set Intersection���� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt sHashKey;

    //-----------------------------------------
    // �Է� Row�� Hash Key�� ȹ��
    //-----------------------------------------

    IDE_TEST( getHashKey( aTempTable, aRow, & sHashKey )
              != IDE_SUCCESS );

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        // PROJ-2553 Set Operation������ Partitioned Array Hashing��
        // ����ؼ��� �� �ȴ�.
        IDE_DASSERT( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                         != QMCD_HASH_TMP_HASHING_PARTITIONED );

        IDE_TEST( qmcMemHash::getSameRowAndNonHit( aTempTable->memoryTemp,
                                                   sHashKey,
                                                   aRow,
                                                   aResultRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getSameRowAndNonHit( aTempTable->diskTemp,
                                                    sHashKey,
                                                    aRow,
                                                    aResultRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmcHashTemp::getNullRow( qmcdHashTemp * aTempTable,
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
    
    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
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
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getNullRow( aTempTable->diskTemp, aRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::getSameGroup( qmcdHashTemp  * aTempTable,
                           void         ** aRow,
                           void         ** aResultRow )
{
/***********************************************************************
 *
 * Description :
 *
 *    To Fix PR-8213
 *    �־��� Row�� ������ Group�� Row�� �˻��Ѵ�.
 *    Group Aggregation������ ����Ѵ�.
 *
 * Implementation :
 *
 *    Memory Temp Table�� ��� �˻��� ���ÿ� ������ �õ���.
 *       - ������ Group �� �ִ� ��� �˻���.
 *       - ������ Group �� ���� ��� ���Ե�.
 *
 *    Disk Temp Table�� ��� �˻��� ������.
 *
 ***********************************************************************/

    qmcMemHashElement * sElement;
    UInt                sHashKey;
    void              * sRow;

    // Hash Key ȹ��
    // Disk Temp Table�� ��쵵 Select�� ���� Hash Key�� �ʿ��ϴ�.
    IDE_TEST( getHashKey( aTempTable, *aRow, & sHashKey )
              != IDE_SUCCESS );

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        // PROJ-2362 memory temp ���� ȿ���� ����
        if ( aTempTable->existTempType == ID_TRUE )
        {
            IDE_TEST( makeTempTypeRow( aTempTable,
                                       *aRow,
                                       & sRow )
                      != IDE_SUCCESS );
            
            // ���� ������ row�� ��ü�Ѵ�.
            *aRow = sRow;
        }
        else
        {
            sRow = *aRow;
        }
        
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //    1. Hit Flag �� ������踦 �ʱ�ȭ�Ѵ�.
        //    2. Memory Temp Table�� �����Ѵ�.
        //-----------------------------------------

        // Hit Flag�� �ʱ�ȭ�Ѵ�.
        sElement = (qmcMemHashElement*) sRow;

        sElement->flag  = QMC_ROW_INITIALIZE;
        sElement->flag &= ~QMC_ROW_HIT_MASK;
        sElement->flag |= QMC_ROW_HIT_FALSE;
        sElement->next  = NULL;

        // Memory Temp Table�� �����Ѵ�.
        // ���� Group�� �ִ� ��� ������ �����ϰ� aResultRow�� ����
        // ������.
        // ���� ���, ������ �����ϰ� aResultRow�� NULL�� ��.

        // PROJ-2553 Group Aggregation������ 
        // Partitioned Array Hashing�� ����ؼ��� �� �ȴ�.
        IDE_DASSERT( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
                         != QMCD_HASH_TMP_HASHING_PARTITIONED );

        IDE_TEST( aTempTable->memoryTemp->insert( aTempTable->memoryTemp,
                                                  sHashKey,
                                                  sRow,
                                                  aResultRow )
                  != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getSameGroup( aTempTable->diskTemp,
                                             sHashKey,
                                             *aRow,
                                             aResultRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::getDisplayInfo( qmcdHashTemp * aTempTable,
                             ULong        * aDiskPage,
                             SLong        * aRecordCnt,
                             UInt         * aBucketCnt )
{
/***********************************************************************
 *
 * Description :
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    if ( (aTempTable->flag & QMCD_HASH_TMP_STORAGE_TYPE)
         == QMCD_HASH_TMP_STORAGE_MEMORY )
    {
        //-----------------------------------------
        // Memory Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        *aDiskPage = 0;

        if ( ( aTempTable->flag & QMCD_HASH_TMP_HASHING_TYPE )
             == QMCD_HASH_TMP_HASHING_PARTITIONED )
        {
            IDE_TEST( qmcMemPartHash::getDisplayInfo( aTempTable->memoryPartTemp,
                                                      aRecordCnt,
                                                      aBucketCnt )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmcMemHash::getDisplayInfo( aTempTable->memoryTemp,
                                                  aRecordCnt,
                                                  aBucketCnt )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------
        // Disk Hash Temp Table�� ����ϴ� ���
        //-----------------------------------------

        IDE_TEST( qmcDiskHash::getDisplayInfo( aTempTable->diskTemp,
                                               aDiskPage,
                                               aRecordCnt )
                  != IDE_SUCCESS );
        *aBucketCnt = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmcHashTemp::makeMemNullRow( qmcdHashTemp * aTempTable )
{
/***********************************************************************
 *
 * Description :
 *    Memory Hash Temp Table�� ���� Null Row�� �����Ѵ�.
 *
 * Implementation :
 *    ���� �����ϴ� Column�� ���ؼ��� Null Value�� �����ϰ�,
 *    Pointer/RID���� �����ϴ� Column�� ���ؼ��� Null Value�� ��������
 *    �ʴ´�.
 *
 ***********************************************************************/

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
        IDU_LIMITPOINT("qmcSortTemp::makeMemNullRow::malloc");
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
        IDU_FIT_POINT( "qmcHashTemp::makeMemNullRow::cralloc::nullRow",
                        idERR_ABORT_InsufficientMemory );

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
}


IDE_RC
qmcHashTemp::getHashKey( qmcdHashTemp * aTempTable,
                         void         * aRow,
                         UInt         * aHashKey )
{
/***********************************************************************
 *
 * Description :
 *    �־��� Row�κ��� Hash Key�� ȹ���Ѵ�.
 * Implementation :
 *
 ***********************************************************************/

    UInt sValue = mtc::hashInitialValue;
    qmdMtrNode * sNode;

    for ( sNode = aTempTable->hashNode; sNode != NULL; sNode = sNode->next )
    {
        sValue = sNode->func.getHash( sValue,
                                      sNode,
                                      aRow );
    }

    *aHashKey = sValue;

    return IDE_SUCCESS;
}
