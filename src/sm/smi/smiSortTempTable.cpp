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
 * $Id $
 **********************************************************************/
#include <sdt.h>
#include <smiTrans.h>
#include <smiMisc.h>
#include <smiMain.h>
#include <smiTempTable.h>
#include <smiStatement.h>
#include <smuProperty.h>
#include <smiSortTempTable.h>
#include <sdtTempRow.h>
#include <sdtWASortMap.h>
#include <sgmManager.h>

iduMemPool smiSortTempTable::mTempCursorPool;
iduMemPool smiSortTempTable::mTempPositionPool;

#define SDT_CHECK_SORTSTATS_AND_SESSIONEND( aHeader, aOpr )            \
    if( ( ++((aHeader)->mCheckCnt) % SMI_TT_STATS_INTERVAL ) == 0 ) \
    {                                                                          \
        generateSortStats( (aHeader), (aOpr) );                                \
        IDE_TEST( iduCheckSessionEvent( (aHeader)->mStatistics ) != IDE_SUCCESS); \
        (aHeader)->mCheckCnt = 0;                                               \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        (aHeader)->mStatsPtr->mTTLastOpr = (aOpr);                              \
    };

#define SDT_CHECK_SORTSTATS( aHeader, aOpr )            \
    if( ( ++((aHeader)->mCheckCnt) % SMI_TT_STATS_INTERVAL ) == 0 ) \
    {                                                               \
        generateSortStats( (aHeader), (aOpr) );                     \
    }                                                               \
    else                                                            \
    {                                                               \
        (aHeader)->mStatsPtr->mTTLastOpr = (aOpr);                  \
    };


/*********************************************************
 * Description : ���� ������ TempTable�� ���� �ֿ� �޸𸮸� �ʱ�ȭ��
 ***********************************************************/
IDE_RC smiSortTempTable::initializeStatic()
{
    IDE_TEST( mTempCursorPool.initialize(
                  IDU_MEM_SM_SMI,
                  (SChar*)"SMI_SORT_CURSOR_POOL",
                  smuProperty::getIteratorMemoryParallelFactor(),
                  ID_SIZEOF( smiSortTempCursor),
                  64, /* ElemCount */
                  IDU_AUTOFREE_CHUNK_LIMIT,               /* ChunkLimit */
                  ID_TRUE,                                /* UseMutex */
                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,        /* AlignByte */
                  ID_FALSE,                               /* ForcePooling */
                  ID_TRUE,                                /* GarbageCollection */
                  ID_TRUE,                                /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_LEGACY )
              != IDE_SUCCESS );

    IDE_TEST( mTempPositionPool.initialize(
                  IDU_MEM_SM_SMI,
                  (SChar*)"SMI_SORT_POSITION_POOL",
                  smuProperty::getIteratorMemoryParallelFactor(),
                  ID_SIZEOF( smiTempPosition ),
                  64, /* ElemCount */
                  IDU_AUTOFREE_CHUNK_LIMIT,               /* ChunkLimit */
                  ID_TRUE,                                /* UseMutex */
                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,        /* AlignByte */
                  ID_FALSE,                               /* ForcePooling */
                  ID_TRUE,                                /* GarbageCollection */
                  ID_TRUE ,                               /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_LEGACY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
 * Description : ���� ����� TempTable�� ���� �ֿ� �޴����� ����.
 ***********************************************************/
IDE_RC smiSortTempTable::destroyStatic()
{
    IDE_TEST( mTempCursorPool.destroy()   != IDE_SUCCESS );
    IDE_TEST( mTempPositionPool.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Column�� ������ Ȯ���մϴ�.
 *
 * aTempHeader - [IN] ��� Temp Table Header
 * aColumnList - [IN] Column�� List
 ***************************************************************************/
IDE_RC smiSortTempTable::getColumnCount( smiTempTableHeader  * aTempHeader,
                                         const smiColumnList * aColumnList )
{
    smiColumnList * sColumns = (smiColumnList *)aColumnList;
    smiColumn     * sColumn;
    UInt            sColumnCount = 0;

    aTempHeader->mRowSize  = 0;

    while( sColumns != NULL)
    {
        sColumn  = (smiColumn*)sColumns->column;
        IDE_ERROR_MSG( (sColumn->flag & SMI_COLUMN_TYPE_MASK)
                       == SMI_COLUMN_TYPE_FIXED,
                       "Error occurred while temp table create "
                       "(Tablespace ID : %"ID_UINT32_FMT")",
                       aTempHeader->mSpaceID );

        aTempHeader->mRowSize = IDL_MAX( sColumn->offset + sColumn->size,
                                         aTempHeader->mRowSize );

        sColumnCount++;
        sColumns = sColumns->next;

        // Key Column List�ʹ� �ٸ��� �ߺ� skip ���� �����Ƿ�,
        // Column Max Count�� ������ �ʴ��� ��Ȯ�ϰ� Ȯ���ؾ� �Ѵ�.
        // List�� �߸��Ǿ��� ��쿡 Hang�� �߻��ϴ� ���� ���� ����
        // BUG-40079�� �߰��� ������ BUG-46265���� while ������ �ű��.
        IDE_TEST_RAISE( sColumnCount > SMI_COLUMN_ID_MAXIMUM,
                        maximum_column_count_error );
    }

    aTempHeader->mColumnCount = sColumnCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( maximum_column_count_error );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_Maximum_Column_count_in_temptable ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description : TempTable�� Column Info�� �籸�� �մϴ�.
 *
 * aTempHeader - [IN] ��� Temp Table Header
 * aColumnList - [IN] Column�� List
 * aKeyColumns - [IN] Key Column�� List
 ***************************************************************************/
IDE_RC smiSortTempTable::buildColumnInfo( smiTempTableHeader  * aTempHeader,
                                          const smiColumnList * aColumnList,
                                          const smiColumnList * aKeyColumns )
{
    smiColumnList * sColumns = (smiColumnList *)aColumnList;
    smiColumn     * sColumn;
    smiTempColumn * sTempColumn;
    smiColumnList * sKeyColumns;
    UInt            sColumnIdx = 0;

    while( sColumns != NULL )
    {
        IDE_ERROR( sColumnIdx < aTempHeader->mColumnCount );

        sColumn     = (smiColumn*)sColumns->column;
        sTempColumn = &aTempHeader->mColumns[ sColumnIdx ];

        /*********************** KEY COLUMN ***************************/
        /* Ű Į������ ã�� ��, Ű Į���̸� �׿� �ش��ϴ� Flag�� ���� */
        sKeyColumns = (smiColumnList*)aKeyColumns;
        while( sKeyColumns != NULL )
        {
            if( sKeyColumns->column->id == sColumn->id )
            {
                /* KeyColumn�� Flag�� �������� */
                sColumn->flag = sKeyColumns->column->flag;
                sColumn->flag &= ~SMI_COLUMN_USAGE_MASK;
                sColumn->flag |= SMI_COLUMN_USAGE_INDEX;
                break;
            }
            else
            {
                sKeyColumns = sKeyColumns->next;
            }
        }

        /*********************** STORE COLUMN *********************/
        idlOS::memcpy( &sTempColumn->mColumn,
                       sColumn,
                       ID_SIZEOF( smiColumn ) );

        IDE_TEST( gSmiGlobalCallBackList.findCopyDiskColumnValue(
                                            sColumn,
                                            &sTempColumn->mConvertToCalcForm )
                  != IDE_SUCCESS);

        IDE_TEST( gSmiGlobalCallBackList.findNull( sColumn,
                                                   sColumn->flag,
                                                   &sTempColumn->mNull )
                  != IDE_SUCCESS );

        /* PROJ-2435 order by nulls first/last */
        IDE_TEST( gSmiGlobalCallBackList.findIsNull( sColumn,
                                                     sColumn->flag,
                                                     &sTempColumn->mIsNull )
                  != IDE_SUCCESS );

        IDE_TEST( gSmiGlobalCallBackList.getNonStoringSize(
                      sColumn,
                      &sTempColumn->mStoringSize )
                  != IDE_SUCCESS);

        IDE_TEST( gSmiGlobalCallBackList.findCompare(
                      sColumn,
                      (sColumn->flag & SMI_COLUMN_ORDER_MASK) | SMI_COLUMN_COMPARE_NORMAL,
                      &sTempColumn->mCompare )
                  != IDE_SUCCESS );

        sTempColumn->mIdx = sColumnIdx;
        sTempColumn->mNextKeyColumn = NULL;
        sColumnIdx++;
        sColumns = sColumns->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : TempTable�� Key Column Info�� �籸�� �մϴ�.
 *
 * aTempHeader - [IN] ��� Temp Table Header
 * aKeyColumns - [IN] Key Column�� List
 ***************************************************************************/
IDE_RC smiSortTempTable::buildKeyColumns( smiTempTableHeader  * aTempHeader,
                                          const smiColumnList * aKeyColumns )
{
    smiTempColumn * sTempColumn;
    smiColumnList * sKeyColumns;
    smiTempColumn * sPrevKeyColumn;
    UInt            sColumnIdx;
    UInt            sColumnCount;

    /* KeyColumnList�� ������ */
    sKeyColumns    = (smiColumnList*)aKeyColumns;
    sColumnIdx     = sKeyColumns->column->id & SMI_COLUMN_ID_MASK;
    sPrevKeyColumn = &aTempHeader->mColumns[ sColumnIdx ];
    sColumnCount   = 0;
    aTempHeader->mKeyColumnList = sPrevKeyColumn;

    while( sKeyColumns->next != NULL )
    {
        // �ߺ��Ǿ skip�ϸ� �ǹǷ� ��Ȯ�ϰ� Column Max Count ��������
        // Ȯ�� �� �ʿ�� ����. Hang�� �߻����� ���� ������ ������ Ȯ���Ѵ�.
        IDE_ERROR( ++sColumnCount <= ( SMI_COLUMN_ID_MAXIMUM * 2 ) );

        sKeyColumns = sKeyColumns->next;
        sColumnIdx  = sKeyColumns->column->id & SMI_COLUMN_ID_MASK;

        IDE_ERROR( sColumnIdx < aTempHeader->mColumnCount );

        sTempColumn = &aTempHeader->mColumns[ sColumnIdx ];

        IDE_ERROR( sKeyColumns->column->id == sTempColumn->mColumn.id );

        if ( sTempColumn->mNextKeyColumn == NULL )
        {
            sPrevKeyColumn->mNextKeyColumn = sTempColumn;
            sPrevKeyColumn = sTempColumn;
        }
        else
        {
            // BUG-46265 Create Disk Temp Table ���� Key column list��
            // ��ȯ �� ���� �ִ��� �����մϴ�.
            // NULL�� �ƴ� ���� �̹� �ռ� Key Column list �� ���� �� ���̴�.
            // �̸� skip���� ������ hang�̳� �߸��� order�� ��µǹǷ� skip�Ѵ�.
            // Order by I0, I1, I0 ���� ��� ù��° I0���� �̹� I0�� ���� �ǹǷ�
            // �ι�° I0�� �ǹ̰� ����, skip �ص� �ȴ�.
            // QX���� �ߺ� ���� �ϹǷ� �ߺ��ؼ� �������� ���� ������,
            // ������ ��츦 ���ؼ� �����Ѵ�.

            IDE_DASSERT( 0 );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : TempTable�� �����մϴ�.
 *
 * Sort/LimitSort/MinMaxSort ���� �����Ͽ� �����մϴ�.
 * segAttr, SegStoAttr�� TempTable�� Ư���� �ʿ���⿡ �����մϴ�.
 *
 * aStatistics     - [IN] �������
 * aSpaceID        - [IN] TablespaceID
 * aWorkAreaSize   - [IN] ������ TempTable�� WA ũ��. (0�̸� �ڵ� ����)
 * aStatement      - [IN] Statement
 * aFlag           - [IN] TempTable�� Ÿ���� ������
 * aColumnList     - [IN] Column ���
 * aKeyColumns     - [IN] KeyColumn(������) Column ���
 * aWorkGroupRatio - [IN] Sort������ ũ��. 0�̸� �˾Ƽ� ��
 * aTable          - [OUT] ������ TempTable
 ***************************************************************************/
IDE_RC smiSortTempTable::create( idvSQL              * aStatistics,
                                 scSpaceID             aSpaceID,
                                 ULong                 aInitWorkAreaSize,
                                 smiStatement        * aStatement,
                                 UInt                  aFlag,
                                 const smiColumnList * aColumnList,
                                 const smiColumnList * aKeyColumns,
                                 UInt                  aWorkGroupRatio,
                                 const void         ** aTable )
{
    smiTempTableHeader * sHeader;
    smiTempColumn      * sTempColumn;
    UInt                 sState = 0;
    SChar              * sValue;
    UInt                 sLength;
    UInt                 sColumnIdx;

    IDE_TEST_RAISE( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) != ID_TRUE,
                    tablespaceID_error );

    /* TempTablespace��  Lock�� ��´�. */
    IDE_TEST( sctTableSpaceMgr::lockAndValidateTBS(
                                        aStatement->getTrans()->getTrans(),
                                        aSpaceID,
                                        SCT_VAL_DDL_DML,
                                        ID_TRUE,   /* intent lock  ���� */
                                        ID_TRUE,  /* exclusive lock */
                                        sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aStatement->getTrans()->getTrans()) ) 
              != IDE_SUCCESS );

    IDU_FIT_POINT( "smiSortTempTable::create::alloc" );
    IDE_TEST( smiTempTable::allocTempTableHdr( &sHeader ) != IDE_SUCCESS );
    sState = 1;

    idlOS::memset( sHeader, 0, ID_SIZEOF( smiTempTableHeader ) );

    /************ TempTable ������� �ʱ�ȭ *************/
    smiTempTable::initStatsPtr( sHeader,
                                aStatistics,
                                aStatement );

    generateSortStats( sHeader,
                       SMI_TTOPR_CREATE );

    sHeader->mRowCount       = 0;
    sHeader->mWASegment      = NULL;
    sHeader->mTTState        = SMI_TTSTATE_INIT;
    sHeader->mTTFlag         = aFlag | SMI_TTFLAG_TYPE_SORT;
    sHeader->mSpaceID        = aSpaceID;
    sHeader->mHitSequence    = 1;
    sHeader->mTempCursorList = NULL;
    sHeader->mFetchGroupID   = SDT_WAGROUPID_INIT;
    sHeader->mStatistics     = aStatistics;
    sHeader->mOpenCursor     = NULL;
    sHeader->mCheckCnt       = 0;

    IDE_TEST( getColumnCount( sHeader,
                              aColumnList ) != IDE_SUCCESS );

    /* smiTempTable_create_malloc_Columns.tc */
    IDU_FIT_POINT("smiSortTempTable::create::malloc::Columns");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 (ULong)ID_SIZEOF(smiTempColumn) * sHeader->mColumnCount,
                                 (void**)&sHeader->mColumns )
        != IDE_SUCCESS );
    sState = 2;
    sHeader->mStatsPtr->mRuntimeMemSize += ID_SIZEOF(smiTempColumn) * sHeader->mColumnCount;

    /***************************************************************
     * Column���� ����
     ****************************************************************/
    IDE_TEST( buildColumnInfo( sHeader,
                               aColumnList,
                               aKeyColumns ) != IDE_SUCCESS );

    if( aKeyColumns != NULL )
    {
        IDE_TEST( buildKeyColumns( sHeader,
                                   aKeyColumns ) != IDE_SUCCESS );
    }

    /* smiTempTable_create_malloc_RowBuffer4Fetch.tc */
    IDU_FIT_POINT("smiSortTempTable::create::malloc::RowBuffer4Fetch");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 sHeader->mRowSize,
                                 (void**)&sHeader->mRowBuffer4Fetch )
        != IDE_SUCCESS );
    sState = 3;
    sHeader->mStatsPtr->mRuntimeMemSize += sHeader->mRowSize;
    /* smiTempTable_create_malloc_RowBuffer4Compare.tc */
    IDU_FIT_POINT("smiSortTempTable::create::malloc::RowBuffer4Compare");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 sHeader->mRowSize,
                                 (void**)&sHeader->mRowBuffer4Compare )
        != IDE_SUCCESS );
    sState = 4;
    sHeader->mStatsPtr->mRuntimeMemSize += sHeader->mRowSize;
    /* smiTempTable_create_malloc_RowBuffer4CompareSub.tc */
    IDU_FIT_POINT("smiSortTempTable::create::malloc::RowBuffer4CompareSub");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 sHeader->mRowSize,
                                 (void**)&sHeader->mRowBuffer4CompareSub )
        != IDE_SUCCESS );
    sState = 5;
    sHeader->mStatsPtr->mRuntimeMemSize += sHeader->mRowSize;

    /* smiTempTable_create_malloc_NullRow.tc */
    IDU_FIT_POINT("smiSortTempTable::create::malloc::NullRow");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                 sHeader->mRowSize,
                                 (void**)&sHeader->mNullRow )
        != IDE_SUCCESS );
    sState = 6;
    sHeader->mStatsPtr->mRuntimeMemSize += sHeader->mRowSize;

    /* NullColumn ���� */
    for( sColumnIdx = 0 ; sColumnIdx < sHeader->mColumnCount ; sColumnIdx++ )
    {
        sTempColumn = &sHeader->mColumns[ sColumnIdx ];

        if( (sTempColumn->mColumn.flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED )
        {
            sValue = (SChar*)sHeader->mNullRow + sTempColumn->mColumn.offset;
        }
        else
        {
            sValue = sgmManager::getVarColumn((SChar*)sHeader->mNullRow, &(sTempColumn->mColumn), &sLength);
        }

        sTempColumn->mNull( &sTempColumn->mColumn,
                            sValue );
    }

    sHeader->mSortGroupID = 0;
    IDE_TEST( sHeader->mRunQueue.initialize( IDU_MEM_SM_SMI,
                                             ID_SIZEOF( scPageID ) )
              != IDE_SUCCESS);
    sState = 7;
    sHeader->mWorkGroupRatio = aWorkGroupRatio == 0 ? smuProperty::getTempSortGroupRatio() : aWorkGroupRatio;

    IDE_TEST( sdtSortSegment::createSortSegment( sHeader,
                                                 aInitWorkAreaSize )
              != IDE_SUCCESS );
    sState = 8;

    IDE_TEST( sdtSortModule::init( sHeader ) != IDE_SUCCESS );

    *aTable = (const void*)sHeader;

    return IDE_SUCCESS;

    IDE_EXCEPTION( tablespaceID_error );
    {
        ideLog::log( IDE_SM_0, "[FAILURE] Fatal error during create disk temp table "
                               "(Tablespace ID : %"ID_UINT32_FMT")",
                               aSpaceID );
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 8:
            (void)sdtSortSegment::dropSortSegment( (sdtSortSegHdr*)sHeader->mWASegment );
        case 7:
            (void)sHeader->mRunQueue.destroy();
        case 6:
            (void)iduMemMgr::free( sHeader->mNullRow );
            sHeader->mNullRow = NULL;
        case 5:
            (void)iduMemMgr::free( sHeader->mRowBuffer4CompareSub );
            sHeader->mRowBuffer4CompareSub = NULL;
        case 4:
            (void)iduMemMgr::free( sHeader->mRowBuffer4Compare );
            sHeader->mRowBuffer4Compare = NULL;
        case 3:
            (void)iduMemMgr::free( sHeader->mRowBuffer4Fetch );
            sHeader->mRowBuffer4Fetch = NULL;
        case 2:
            (void)iduMemMgr::free( sHeader->mColumns );
            sHeader->mColumns = NULL;
        case 1:
            (void)smiTempTable::freeTempTableHdr( sHeader );
            sHeader = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Ű Į���� �缳���Ѵ�. WindowSort���� ���ؼ��̴�.
 *
 * aTable       - [IN] ��� Table
 * aKeyColumns  - [IN] ������ KeyColumn��
 ***************************************************************************/
IDE_RC smiSortTempTable::resetKeyColumn( void                * aTable,
                                         const smiColumnList * aKeyColumns )
{
    smiTempTableHeader   * sHeader = (smiTempTableHeader*)aTable;
    smiTempColumn        * sTempColumn;
    smiTempColumn        * sPrevKeyColumn = NULL;
    smiColumnList        * sKeyColumns;
    UInt                   sColumnIdx;

    SDT_CHECK_SORTSTATS_AND_SESSIONEND( sHeader,
                                        SMI_TTOPR_RESET_KEY_COLUMN ) ;

    IDE_ERROR( SM_IS_FLAG_ON( sHeader->mTTFlag, SMI_TTFLAG_RANGESCAN ) );

    /* ���� Column���Լ�, Index Flag�� ��� ������ */
    for( sColumnIdx = 0 ; sColumnIdx < sHeader->mColumnCount ; sColumnIdx++ )
    {
        sTempColumn = &sHeader->mColumns[ sColumnIdx ];
        sTempColumn->mColumn.flag &= ~SMI_COLUMN_USAGE_INDEX;
    }

    sHeader->mKeyColumnList  = NULL;
    sPrevKeyColumn           = NULL;
    sKeyColumns = (smiColumnList*)aKeyColumns;

    while( sKeyColumns != NULL )
    {
        IDE_TEST_RAISE( sKeyColumns->column->id > SMI_COLUMN_ID_MAXIMUM,
                        maximum_column_count_error );
        sColumnIdx = sKeyColumns->column->id & SMI_COLUMN_ID_MASK;
        IDE_ERROR( sColumnIdx < sHeader->mColumnCount );

        sTempColumn = &sHeader->mColumns[ sColumnIdx ];

        IDE_ERROR( sKeyColumns->column->id == sTempColumn->mColumn.id );

        if( sPrevKeyColumn == NULL )
        {
            sHeader->mKeyColumnList = sTempColumn;
        }
        else
        {
            sPrevKeyColumn->mNextKeyColumn = sTempColumn;
        }

        sTempColumn->mColumn.flag = sKeyColumns->column->flag;
        sTempColumn->mColumn.flag &= ~SMI_COLUMN_USAGE_MASK;
        sTempColumn->mColumn.flag |= SMI_COLUMN_USAGE_INDEX;

        /* Ž�� ����(ASC,DESC)�� ����� �� �ֱ⿡, Compare�Լ���
         * �ٽ� Ž���ؾ� �� */
        IDE_TEST( gSmiGlobalCallBackList.findCompare(
                      sKeyColumns->column,
                      (sTempColumn->mColumn.flag & SMI_COLUMN_ORDER_MASK) | SMI_COLUMN_COMPARE_NORMAL,
                      &sTempColumn->mCompare )
                  != IDE_SUCCESS );

        /* PROJ-2435 order by nulls first/last */
        IDE_TEST( gSmiGlobalCallBackList.findIsNull( sKeyColumns->column,
                                                     sKeyColumns->column->flag,
                                                     &sTempColumn->mIsNull )
                  != IDE_SUCCESS );

        sPrevKeyColumn = sTempColumn;
        sKeyColumns = sKeyColumns->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( maximum_column_count_error );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_Maximum_Column_count_in_temptable));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : TempTable�� �����մϴ�.
 *
 * aTable - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiSortTempTable::drop( void  * aTable )
{
    UInt                 sState = 9;
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    /* Drop�� �ݵ�� ���� �Ͽ��� �ϱ� ������ Session check�� ���� �ʴ´�.*/
    sHeader->mStatsPtr->mDropTV = smiGetCurrTime();
    generateSortStats( sHeader,
                       SMI_TTOPR_DROP);

    IDE_TEST( closeAllCursor( aTable ) != IDE_SUCCESS );

    sState = 8;
    IDE_TEST( sdtSortModule::destroy( sHeader ) != IDE_SUCCESS );

    sState = 7;
    IDE_TEST( sdtSortSegment::dropSortSegment( (sdtSortSegHdr*)sHeader->mWASegment ) != IDE_SUCCESS );

    /* ������� ������Ŵ */
    smiTempTable::accumulateStats( sHeader );

    sState = 6;
    IDE_TEST( sHeader->mRunQueue.destroy() != IDE_SUCCESS );

    sState = 5;
    IDE_TEST( iduMemMgr::free( sHeader->mNullRow )
              != IDE_SUCCESS );
    sHeader->mNullRow = NULL;

    sState = 4;
    IDE_TEST( iduMemMgr::free( sHeader->mRowBuffer4CompareSub )
              != IDE_SUCCESS );
    sHeader->mRowBuffer4CompareSub = NULL;

    sState = 3;
    IDE_TEST( iduMemMgr::free( sHeader->mRowBuffer4Compare )
              != IDE_SUCCESS );
    sHeader->mRowBuffer4Compare = NULL;

    sState = 2;
    IDE_TEST( iduMemMgr::free( sHeader->mRowBuffer4Fetch )
              != IDE_SUCCESS );
    sHeader->mRowBuffer4Fetch = NULL;

    sState = 1;
    IDE_TEST( iduMemMgr::free( sHeader->mColumns )
              != IDE_SUCCESS );
    sHeader->mColumns = NULL;

    sState = 0;
    IDE_TEST( smiTempTable::freeTempTableHdr( sHeader )
              != IDE_SUCCESS );
    sHeader = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 9:
    case 8:
        (void) sdtSortSegment::dropSortSegment( (sdtSortSegHdr*)sHeader->mWASegment );
    case 7:
        (void)sHeader->mRunQueue.destroy();
    case 6:
        (void)iduMemMgr::free( sHeader->mNullRow );
    case 5:
        (void)iduMemMgr::free( sHeader->mRowBuffer4CompareSub );
    case 4:
        (void)iduMemMgr::free( sHeader->mRowBuffer4Compare );
    case 3:
        (void)iduMemMgr::free( sHeader->mRowBuffer4Fetch );
    case 2:
        (void)iduMemMgr::free( sHeader->mColumns );
    case 1:
        (void)smiTempTable::freeTempTableHdr( sHeader );
        break;
    default:
        break;
    }
    return IDE_FAILURE;
}

/**************************************************************************
 * Description : truncateTable�ϰ� ��� Ŀ���� �ݽ��ϴ�.
 *
 * aTable    - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiSortTempTable::clear( void   * aTable )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    generateSortStats( sHeader,
                       SMI_TTOPR_CLEAR );

    IDE_TEST( closeAllCursor( aTable ) != IDE_SUCCESS );

    /******************** Destroy ****************************/
    IDE_TEST( sdtSortModule::destroy( sHeader ) != IDE_SUCCESS );

    sHeader->mRowCount       = 0;
    sHeader->mTTState        = SMI_TTSTATE_INIT;
    sHeader->mHitSequence    = 1;
    sHeader->mFetchGroupID   = SDT_WAGROUPID_INIT;

    IDE_TEST( sdtSortSegment::truncateSortSegment( (sdtSortSegHdr*)sHeader->mWASegment ) != IDE_SUCCESS );

    IDE_TEST( sdtSortModule::init( sHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( sHeader );

    return IDE_FAILURE;
}


/**************************************************************************
 * Description : HitFlag�� �ʱ�ȭ�մϴ�.
 * �����δ� Sequence���� �ʱ�ȭ �Ͽ�, Row�� HitSequence�� ���� �ٸ��� �ϰ�,
 * �� ���� �ٸ��� Hit���� �ʾҴٰ� �Ǵ��մϴ�.
 *
 * aTable  - [IN] ��� Table
 ***************************************************************************/
void smiSortTempTable::clearHitFlag( void   * aTable )
{
    ((smiTempTableHeader*)aTable)->mHitSequence++;
}

/**************************************************************************
 * Description : �����͸� �����մϴ�.
 *
 * aTable - [IN] ��� Table
 * aValue - [IN] ������ Value
 ***************************************************************************/
IDE_RC smiSortTempTable::insert( void     * aTable,
                                 smiValue * aValue )
{
    SDT_CHECK_SORTSTATS( (smiTempTableHeader*)aTable,
                         SMI_TTOPR_INSERT );

    return sdtSortModule::insert( (smiTempTableHeader*)aTable,
                                  aValue );
}

/**************************************************************************
 * Description : WindowSort�� �� Ư�� row�� �����մϴ�.
 *
 * aCursor   - [IN] ������ Row�� ����Ű�� Ŀ��
 * aValue    - [IN] ������ Value
 ***************************************************************************/
IDE_RC smiSortTempTable::update( smiSortTempCursor* aCursor,
                                 smiValue         * aValue )
{
    SDT_CHECK_SORTSTATS( aCursor->mTTHeader,
                         SMI_TTOPR_UPDATE );

    return sdtTempRow::update( aCursor,
                               aValue );
}

/**************************************************************************
 * Description : HitFlag�� �����մϴ�.
 *
 * aCursor      - [IN] ������ Row�� ����Ű�� Ŀ��
 ***************************************************************************/
void smiSortTempTable::setHitFlag(smiSortTempCursor* aCursor)
{
    SDT_CHECK_SORTSTATS( aCursor->mTTHeader,
                         SMI_TTOPR_SETHITFLAG );

    sdtTempRow::setHitFlag( aCursor,
                            aCursor->mTTHeader->mHitSequence );
    return;
}

/**************************************************************************
 * Description : HitFlag ���θ� Ȯ���մϴ�.
 *
 * aCursor      - [IN] ���� Row�� ����Ű�� Ŀ��
 ***************************************************************************/
idBool smiSortTempTable::isHitFlagged( smiSortTempCursor* aCursor )
{
    /* PROJ-2339
     * Hit���δ� �Ʒ��� ���� Ȯ���Ѵ�.
     * ( (Temp Table�� HitSequence) == (���� Row�� HitSequence) ) ? T : F;
     *
     * setHitFlag() :
     * ���� Row�� HitSequence�� 1 ������Ų��.
     *
     * clearHitFlag() :
     * Temp Table�� HitSequence�� 1 ������Ų��.
     * �̷��� ��� Row�� Non-Hit�ǹǷ� HitFlag�� ��� ����� �Ͱ� ���� ȿ����
     * I/O ��� ���� ó���� �� �ִ�.
     *
     * (HitFlag ������ PROJ-2201���� �����Ǿ���.)
     */

    // TTHeader�� HitSequence�� �Է�����, HitFlag�� �ִ��� ��ȯ�Ѵ�.
    return sdtTempRow::isHitFlagged( aCursor,
                                     aCursor->mTTHeader->mHitSequence );
}

/**************************************************************************
 * Description : �����մϴ�.
 *
 * aTable  - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiSortTempTable::sort( void * aTable )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    generateSortStats( sHeader,
                       SMI_TTOPR_SORT );

    return sdtSortModule::sort( sHeader );
}

/**************************************************************************
 * Description : GRID�� Row�� �����ɴϴ�.
 * InMemoryScan�� ���, Disk���� Page�� ������ ���� �ʱ� ������,
 * WAMap�� �������� �����ɴϴ�.
 *
 * aTable       - [IN] ��� Table
 * aGRID        - [IN] ��� Row
 * aDestRowBuf  - [OUT] ������ Row�� ������ ����
 ***************************************************************************/
IDE_RC smiSortTempTable::fetchFromGRID( void     * aTable,
                                        scGRID     aGRID,
                                        void     * aDestRowBuf )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)sHeader->mWASegment;
    sdtSortScanInfo      sTRPInfo;
    sdtWCB             * sWCBPtr;
    idBool               sIsFixedPage = ID_FALSE;

    SDT_CHECK_SORTSTATS_AND_SESSIONEND( sHeader,
                                        SMI_TTOPR_FETCH_FROMGRID );

    sTRPInfo.mFetchEndOffset = sHeader->mRowSize;

    if( sHeader->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN )
    {
        /*InMemoryScan�� ���, GRID��� WAMap�� �̿��� ���� Pointing*/
        IDE_ERROR( sHeader->mFetchGroupID == SDT_WAGROUPID_NONE );
        IDE_ERROR( SC_MAKE_SPACE( aGRID ) == SDT_SPACEID_WAMAP );

        IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                           SC_MAKE_PID( aGRID ),
                                           (vULong*)&sTRPInfo.mTRPHeader )
                  != IDE_SUCCESS );

        IDE_TEST( sdtTempRow::fetch( sWASeg,
                                     sHeader->mFetchGroupID,
                                     sHeader->mRowBuffer4Fetch,
                                     &sTRPInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ERROR( sHeader->mFetchGroupID != SDT_WAGROUPID_INIT );

        IDE_DASSERT( ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WAMAP ) &&
                     ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WORKAREA ) );

        IDE_TEST( sdtSortSegment::getPagePtrByGRID( sWASeg,
                                                    sHeader->mFetchGroupID,
                                                    aGRID,
                                                    (UChar**)&sTRPInfo.mTRPHeader )
                  != IDE_SUCCESS );
        IDE_DASSERT( SM_IS_FLAG_ON( sTRPInfo.mTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

        sWCBPtr = sdtSortSegment::findWCB( sWASeg, SC_MAKE_PID( aGRID ) );

        sdtSortSegment::fixWAPage( sWCBPtr );
        sIsFixedPage = ID_TRUE;

        IDE_TEST( sdtTempRow::fetch( sWASeg,
                                     sHeader->mFetchGroupID,
                                     sHeader->mRowBuffer4Fetch,
                                     &sTRPInfo )
                  != IDE_SUCCESS );

        sIsFixedPage = ID_FALSE;
        sdtSortSegment::unfixWAPage( sWCBPtr );

        IDE_DASSERT(( sWASeg->mGroup[1].mHintWCBPtr != sWCBPtr ) ||
                    ( sWCBPtr->mFix > 0 ));
    }

    idlOS::memcpy( aDestRowBuf,
                   sTRPInfo.mValuePtr,
                   sTRPInfo.mValueLength );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( sHeader );

    if( sIsFixedPage == ID_TRUE )
    {
        sdtSortSegment::unfixWAPage( sWCBPtr );

        IDE_DASSERT(( sWASeg->mGroup[1].mHintWCBPtr != sWCBPtr ) ||
                    ( sWCBPtr->mFix > 0 ));
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : Ŀ���� ���ϴ�.
 * Range,Filter,� ���� BeforeFirst ���ҵ� �մϴ�.
 *
 * aTable     - [IN] ��� Table
 * aFlag      - [IN] TempCursor�� Ÿ���� ������
 * aColumns   - [IN] ������ Column ����
 * aKeyRange  - [IN] SortTemp�� �����, Range
 * aKeyFilter - [IN] KeyFilter
 * aRowFilter - [IN] RowFilter
 * aCursor    - [OUT] ��ȯ��
 ***************************************************************************/
IDE_RC smiSortTempTable::openCursor( void                * aTable,
                                     UInt                  aFlag,
                                     const smiColumnList * aColumns,
                                     const smiRange      * aKeyRange,
                                     const smiRange      * aKeyFilter,
                                     const smiCallBack   * aRowFilter,
                                     smiSortTempCursor     ** aCursor )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;
    smiSortTempCursor  * sCursor;
    const smiColumn    * sColumn;
    UInt                 sState = 0;

    IDE_DASSERT((sHeader->mTTFlag & SMI_TTFLAG_TYPE_MASK ) == SMI_TTFLAG_TYPE_SORT );

    if( sHeader->mOpenCursor == NULL )
    {
        /* QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY �� ���� ���
         * QP���� Sort���� ó���ϴ� ��찡 �߻��� */
        IDE_TEST( sort( aTable ) != IDE_SUCCESS );
    }
    SDT_CHECK_SORTSTATS_AND_SESSIONEND( sHeader,
                                        SMI_TTOPR_OPENCURSOR );

    IDU_FIT_POINT_RAISE( "smiSortTempTable::openCursor::alloc", memery_allocate_failed );
    IDE_TEST( mTempCursorPool.alloc( (void**)&sCursor ) != IDE_SUCCESS );
    sState = 1;

    /* ������ Success�� ȣ���ϴ� Filter�� �ƿ� �����ϵ��� Flag�� ������*/
    SM_SET_FLAG_OFF( aFlag, SMI_TCFLAG_FILTER_MASK );
    if( aKeyRange  != smiGetDefaultKeyRange() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_RANGE );
    }
    if( aKeyFilter != smiGetDefaultKeyRange() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_KEY );
    }
    if( aRowFilter != smiGetDefaultFilter() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_ROW );
    }
    sCursor->mRange         = aKeyRange;
    sCursor->mKeyFilter     = aKeyFilter;
    sCursor->mRowFilter     = aRowFilter;
    sCursor->mTTHeader      = sHeader;
    sCursor->mTCFlag        = aFlag;
    sCursor->mUpdateColumns = aColumns;
    sCursor->mPositionList  = NULL;
    sCursor->mMergePosition = NULL;

    sCursor->mWPID          = SC_NULL_PID;
    sCursor->mWAPagePtr     = NULL;
    sCursor->mWASegment     = sHeader->mWASegment;

    sCursor->mUpdateEndOffset = 0;

    while ( aColumns != NULL )
    {
        sColumn = aColumns->column;
        if (( sColumn->offset + sColumn->size ) > sCursor->mUpdateEndOffset )
        {
            sCursor->mUpdateEndOffset = sColumn->offset + sColumn->size;
        }
        aColumns = aColumns->next;
    }

    IDE_ERROR( sHeader->mOpenCursor != NULL );
    IDE_TEST( sHeader->mOpenCursor( sHeader, sCursor )
              != IDE_SUCCESS);

    *aCursor = sCursor;

    /* Fetch �Լ��� �����Ǿ���� �� */
    IDE_ERROR( sCursor->mFetch != NULL );

    /* ����ó���� �Ͼ�� �ʴ� ��찡 �Ǿ�� Link�� ������ */
    sCursor->mNext = (smiSortTempCursor*)sHeader->mTempCursorList;
    sHeader->mTempCursorList = (void*)sCursor;

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( memery_allocate_failed );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
#endif
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        (void) mTempCursorPool.memfree( sCursor );
    }

    smiTempTable::checkAndDump( sHeader );

    return IDE_FAILURE;
}


/***************************************************************************
 * Description : Ŀ���� �ٽ� ���ϴ�. Ŀ�� ��Ȱ��� ���˴ϴ�.
 * Ŀ���� ���ο� Range/Filter�� �شٰ� �ص� �½��ϴ�.
 *
 * aCursor        - [IN] ��� Cursor
 * aFlag          - [IN] TempCursor�� Ÿ���� ������
 * aKeyRange      - [IN] SortTemp�� �����, Range
 * aKeyFilter     - [IN] KeyFilter
 * aRowFilter     - [IN] RowFilter
 ***************************************************************************/
IDE_RC smiSortTempTable::restartCursor( smiSortTempCursor   * aCursor,
                                        UInt                  aFlag,
                                        const smiRange      * aKeyRange,
                                        const smiRange      * aKeyFilter,
                                        const smiCallBack   * aRowFilter )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;

    SDT_CHECK_SORTSTATS_AND_SESSIONEND( sHeader,
                                        SMI_TTOPR_RESTARTCURSOR );

    IDE_DASSERT( sHeader->mOpenCursor != NULL );

    SM_SET_FLAG_OFF( aFlag, SMI_TCFLAG_FILTER_MASK );
    if( aKeyRange  != smiGetDefaultKeyRange() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_RANGE );
    }
    if( aKeyFilter != smiGetDefaultKeyRange() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_KEY );
    }
    if( aRowFilter != smiGetDefaultFilter() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_ROW );
    }
    aCursor->mRange         = aKeyRange;
    aCursor->mKeyFilter     = aKeyFilter;
    aCursor->mRowFilter     = aRowFilter;
    aCursor->mTCFlag        = aFlag;

    IDE_TEST( sHeader->mOpenCursor( sHeader, aCursor )
              != IDE_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( sHeader );

    return IDE_FAILURE;
}


/**************************************************************************
 * Description : Ŀ���κ��� ���� Row�� �����ɴϴ�(FetchNext)
 *
 * aCursor - [IN] ��� Cursor
 * aRow    - [OUT] ��� Row
 * aGRID   - [OUT] ������ Row�� GRID
 ***************************************************************************/
IDE_RC smiSortTempTable::fetch( smiSortTempCursor * aCursor,
                                UChar            ** aRow,
                                scGRID            * aRowGRID )
{
    SDT_CHECK_SORTSTATS_AND_SESSIONEND( aCursor->mTTHeader,
                                        SMI_TTOPR_FETCH );
    *aRowGRID = SC_NULL_GRID;

    IDE_TEST( aCursor->mFetch( aCursor,
                               aRow,
                               aRowGRID ) != IDE_SUCCESS);

    /* Fetch fail */
    if( SC_GRID_IS_NULL( *aRowGRID ) )
    {
        *aRow = NULL;
    }
    else
    {
        /* ã�Ƽ� �Ѵ� NULL�� �ƴϰų�, ��ã�Ƽ� �Ѵ� NULL�̰ų� */
        IDE_DASSERT( *aRow != NULL );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Ŀ���� ��ġ�� �����մϴ�.
 *
 * aCursor    - [IN]  ��� Cursor
 * aPosition  - [OUT] ������ ��ġ
 ***************************************************************************/
IDE_RC smiSortTempTable::storeCursor( smiSortTempCursor   * aCursor,
                                      smiTempPosition    ** aPosition )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    smiTempPosition    * sPosition;
    UInt                 sState = 0;

    SDT_CHECK_SORTSTATS_AND_SESSIONEND( sHeader,
                                        SMI_TTOPR_STORECURSOR );

    /* smiTempTable_storeCursor_alloc_Position.tc */
    IDU_FIT_POINT("smiSortTempTable::storeCursor::alloc::Position");
    IDE_TEST( mTempPositionPool.alloc( (void**)&sPosition ) != IDE_SUCCESS );
    sState = 1;

    sPosition->mNext       = aCursor->mPositionList;
    aCursor->mPositionList = sPosition;
    sPosition->mOwner      = aCursor;
    sPosition->mTTState    = sHeader->mTTState;
    sPosition->mExtraInfo  = NULL;

    IDE_ERROR( aCursor->mStoreCursor != NULL );
    IDE_TEST( aCursor->mStoreCursor( aCursor,
                                     sPosition ) != IDE_SUCCESS);

    *aPosition = sPosition;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 1:
        (void) mTempPositionPool.memfree( sPosition );
        break;
    default:
        break;

    }

    smiTempTable::checkAndDump( sHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Ŀ���� ������ ��ġ�� �ǵ����ϴ�.
 *
 * aCursor   - [IN] ��� Cursor
 * aPosition - [IN] ������ ��ġ
 * aRow      - [OUT] restore�� Cursor���� ����Ű�� Row
 * aRowGRID  - [OUT] Row�� GRID
 ***************************************************************************/
IDE_RC smiSortTempTable::restoreCursor( smiSortTempCursor  * aCursor,
                                        smiTempPosition    * aPosition,
                                        UChar             ** aRow,
                                        scGRID             * aRowGRID )
{
    SDT_CHECK_SORTSTATS_AND_SESSIONEND( aCursor->mTTHeader,
                                        SMI_TTOPR_RESTORECURSOR );

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aCursor->mRestoreCursor != NULL );
    IDE_TEST( aCursor->mRestoreCursor( aCursor,
                                       aPosition )
              != IDE_SUCCESS);

    /* restoreCursor�� ����, QP�� ���� Row/GRID�� ���� ���·�
     * �����ؾ� �Ѵ�. */
    IDE_TEST( fetchFromGRID( aCursor->mTTHeader,
                             aCursor->mGRID,
                             *aRow )
              != IDE_SUCCESS );
    *aRowGRID = aCursor->mGRID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : ��� Ŀ���� �ݽ��ϴ�.
 *
 * aCursor - [IN] ��� Sort Temp Table
 ***************************************************************************/
IDE_RC smiSortTempTable::closeAllCursor( void * aTable )
{
    smiTempTableHeader    * sHeader = (smiTempTableHeader*)aTable;
    smiSortTempCursor     * sCursor;
    smiSortTempCursor     * sNxtCursor;

    sCursor = (smiSortTempCursor*)sHeader->mTempCursorList;
    while( sCursor != NULL )
    {
        sNxtCursor = sCursor->mNext;

        IDE_TEST( closeCursor( sCursor ) != IDE_SUCCESS );

        sCursor = sNxtCursor;
    }

    sHeader->mTempCursorList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Ŀ���� �ݽ��ϴ�.
 *
 * aCursor - [IN] ��� Cursor
 ***************************************************************************/
IDE_RC smiSortTempTable::closeCursor(smiSortTempCursor *  aCursor )
{
    smiTempPosition    * sPosition;
    smiTempPosition    * sNextPosition;

    sPosition = aCursor->mPositionList;
    while( sPosition != NULL )
    {
        sNextPosition = sPosition->mNext;
        if( sPosition->mExtraInfo != NULL )
        {
            IDE_TEST( iduMemMgr::free( sPosition->mExtraInfo )
                      != IDE_SUCCESS );
        }
        IDE_TEST( mTempPositionPool.memfree( sPosition ) != IDE_SUCCESS );

        sPosition = sNextPosition;
    }
    aCursor->mPositionList  = NULL;

    if( aCursor->mMergePosition != NULL)
    {
        IDE_TEST( iduMemMgr::free( aCursor->mMergePosition )
                  != IDE_SUCCESS );
        aCursor->mMergePosition = NULL;
    }
    else
    {
        /* nothing to do */
    }

    if( aCursor->mWPID != SC_NULL_PID )
    {
        sdtSortSegment::unfixWAPage( (sdtSortSegHdr*)aCursor->mWASegment,
                                     aCursor->mWPID ) ;
    }

    IDE_TEST( mTempCursorPool.memfree( aCursor ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Page����, Record������ ��ȯ��. PlanGraph ��� ����
 *
 * aTable         - [IN] ��� Table
 * aPageCount     - [OUT] �� Page ����
 * aRecordCount   - [OUT] �� Row ����
 ***************************************************************************/
void smiSortTempTable::getDisplayInfo( void  * aTable,
                                       ULong * aPageCount,
                                       SLong * aRecordCount )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;
    sdtSortSegHdr      * sWASegment = (sdtSortSegHdr*)sHeader->mWASegment;

    *aRecordCount = sHeader->mRowCount;

    // BUG-39728
    // parallel aggregation �� ����Ҷ� temp table �� �����ؼ� ������ �߻��� ���
    // getDisplayInfo �� ȣ���ϸ� FATAL �� �߻��Ѵ�.
    // �̶� sHeader �� ���������� sWASegment�� NULL �̴�.
    // �� ��Ȳ���� getDisplayInfo �� return ���� �߿�ġ �����Ƿ� FATAL �� �߻����� �ʵ��� �Ѵ�.
    if ( sWASegment != NULL )
    {
        *aPageCount = sdtSortSegment::getNExtentCount( sWASegment )
            * SDT_WAEXTENT_PAGECOUNT ;
    }
    else
    {
        *aPageCount = 0;
    }

    return ;
}

/**************************************************************************
 * Description : ����Ǵ� ��������� �ٽ� ������
 *
 * aHeader - [IN] ��� Table
 * aOPr    - [IN] ������ Operation
 ***************************************************************************/
void smiSortTempTable::generateSortStats( smiTempTableHeader * aHeader,
                                          smiTempTableOpr      aOpr )
{
    smiTempTableStats * sStats;
    sdtSortSegHdr     * sWASegment = (sdtSortSegHdr*)aHeader->mWASegment;

    if( ( smiGetCurrTime() - aHeader->mStatsPtr->mCreateTV >=
          smuProperty::getTempStatsWatchTime() ) &&
        ( aHeader->mStatsPtr == &aHeader->mStatsBuffer ) )
    {
        /* ��ȿ�ð� �̻� ���������鼭, ���� WatchArray�� ��� �ȵž�������
         * ����Ѵ�. */
        aHeader->mStatsPtr = smiTempTable::registWatchArray( aHeader->mStatsPtr );

        if ( sWASegment != NULL )
        {
            sWASegment->mStatsPtr = aHeader->mStatsPtr ;
        }
    }
    sStats = aHeader->mStatsPtr;

    sStats->mTTState   = aHeader->mTTState;
    sStats->mTTLastOpr = aOpr;

    if( sWASegment != NULL )
    {
        sStats->mMaxWorkAreaSize   =
            sdtSortSegment::getMaxWAPageCount( sWASegment ) * SD_PAGE_SIZE;
        sStats->mUsedWorkAreaSize =
            sdtSortSegment::getWASegmentUsedPageCount( sWASegment ) * SD_PAGE_SIZE;
        sStats->mNormalAreaSize = sdtSortSegment::getNExtentCount( sWASegment )
            * SDT_WAEXTENT_PAGECOUNT
            * SD_PAGE_SIZE;
    }
    sStats->mRecordLength     = aHeader->mRowSize;
    sStats->mRecordCount      = aHeader->mRowCount;
    sStats->mMergeRunCount    = aHeader->mMergeRunCount;
    sStats->mHeight           = aHeader->mHeight;
}

/**************************************************************************
 * Description : Cursor�� ������ �����
 *
 * aTempCursor - [IN] ��� Cursor
 * aOutBuf     - [OUT] ������ ���� Buffer
 * aOutSize    - [OUT] Buffer�� ũ��
 ***************************************************************************/
void smiSortTempTable::dumpTempCursor( smiSortTempCursor * aTempCursor,
                                       SChar             * aOutBuf,
                                       UInt                aOutSize )
{
    smiTempPosition    * sPosition;

    while ( aTempCursor != NULL )
    {
        (void)idlVA::appendFormat(aOutBuf,
                                  aOutSize,
                                  "DUMP SORT TEMP CURSOR:\n"
                                  "Flag      : %4"ID_UINT32_FMT"\n"
                                  "Seq       : %4"ID_UINT32_FMT"\n"
                                  "GRID      : %4"ID_UINT32_FMT","
                                  "%4"ID_UINT32_FMT","
                                  "%4"ID_UINT32_FMT"\n"
                                  "WPID      : %4"ID_UINT32_FMT"\n"
                                  "LastSeq   : %4"ID_UINT32_FMT"\n"
                                  "SlotCount : %4"ID_UINT32_FMT"\n"
                                  "PinIdx    : %4"ID_UINT32_FMT"\n"
                                  "WAGroupID : %4"ID_UINT32_FMT"\n"
                                  "UpdateEnd : %4"ID_UINT32_FMT"\n"
                                  "\tPosition:\n"
                                  "\tSTAT SEQ  GRID GRID GRID  WPID SlotCount PinIdx\n",
                                  aTempCursor->mTCFlag,
                                  aTempCursor->mSeq,
                                  aTempCursor->mGRID.mSpaceID,
                                  aTempCursor->mGRID.mPageID,
                                  aTempCursor->mGRID.mOffset,
                                  aTempCursor->mWPID,
                                  aTempCursor->mLastSeq,
                                  aTempCursor->mSlotCount,
                                  aTempCursor->mPinIdx,
                                  aTempCursor->mWAGroupID,
                                  aTempCursor->mUpdateEndOffset );

        sPosition = aTempCursor->mPositionList;
        while( sPosition != NULL )
        {
            (void)idlVA::appendFormat(aOutBuf,
                                      aOutSize,
                                      "\t%4"ID_UINT32_FMT" "
                                      "%4"ID_UINT32_FMT" "
                                      "%4"ID_UINT32_FMT","
                                      "%4"ID_UINT32_FMT","
                                      "%4"ID_UINT32_FMT" "
                                      "%4"ID_UINT32_FMT" "
                                      "%4"ID_UINT32_FMT" "
                                      "%4"ID_UINT32_FMT" ",
                                      sPosition->mTTState,
                                      sPosition->mSeq,
                                      sPosition->mGRID.mSpaceID,
                                      sPosition->mGRID.mPageID,
                                      sPosition->mGRID.mOffset,
                                      sPosition->mWPID,
                                      sPosition->mSlotCount,
                                      sPosition->mPinIdx );

            sPosition = sPosition->mNext;
        }
        aTempCursor = aTempCursor->mNext;
    }
    return;
}
