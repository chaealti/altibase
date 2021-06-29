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
#include <smiMain.h>
#include <smiTrans.h>
#include <smiMisc.h>
#include <smiStatement.h>
#include <smuProperty.h>
#include <smiTempTable.h>
#include <smiHashTempTable.h>

iduMemPool smiHashTempTable::mTempCursorPool;

#define SDT_CHECK_HASHSTATS( aHeader, aOpr )            \
    if( ( ++((aHeader)->mCheckCnt) % SMI_TT_STATS_INTERVAL ) == 0 ) \
    {                                                               \
        generateHashStats( (aHeader), (aOpr) );                     \
    }                                                               \
    else                                                            \
    {                                                               \
        (aHeader)->mStatsPtr->mTTLastOpr = (aOpr);                  \
    };

/*********************************************************
 * Description :
 *   ���� ������ TempTable�� ���� �ֿ� �޸𸮸� �ʱ�ȭ��
 ***********************************************************/
IDE_RC smiHashTempTable::initializeStatic()
{
    return mTempCursorPool.initialize(
                  IDU_MEM_SM_SMI,
                  (SChar*)"SMI_HASH_CURSOR_POOL",
                  smuProperty::getIteratorMemoryParallelFactor(),
                  ID_SIZEOF( smiHashTempCursor ),
                  64, /* ElemCount */
                  IDU_AUTOFREE_CHUNK_LIMIT,        /* ChunkLimit */
                  ID_TRUE,                         /* UseMutex */
                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE, /* AlignByte */
                  ID_FALSE,                        /* ForcePooling */
                  ID_TRUE,                         /* GarbageCollection */
                  ID_TRUE,                         /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_LEGACY );
}

/*********************************************************
 * Description :
 *   ���� ����� TempTable�� ���� �ֿ� �޴����� ����.
 ***********************************************************/
IDE_RC smiHashTempTable::destroyStatic()
{
    return mTempCursorPool.destroy();
}

/**************************************************************************
 * Description : Column�� ������ Ȯ���մϴ�.
 *
 * aTempHeader - [IN] ��� Temp Table Header
 * aColumnList - [IN] Column�� List
 ***************************************************************************/
IDE_RC smiHashTempTable::getColumnCount( smiTempTableHeader  * aTempHeader,
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

        if ( (sColumn->offset + sColumn->size) > aTempHeader->mRowSize )
        {
            aTempHeader->mRowSize = sColumn->offset + sColumn->size;
        }

        sColumnCount++;
        sColumns = sColumns->next;

        /* Key Column List�ʹ� �ٸ��� �ߺ� skip ���� �����Ƿ�,
         * Column Max Count�� ������ �ʴ��� ��Ȯ�ϰ� Ȯ���ؾ� �Ѵ�.
         * List�� �߸��Ǿ��� ��쿡 Hang�� �߻��ϴ� ���� ���� ����
         * BUG-40079�� �߰��� ������ BUG-46265���� while ������ �ű��. */
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
IDE_RC smiHashTempTable::buildColumnInfo( smiTempTableHeader  * aTempHeader,
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
            if ( sKeyColumns->column->id == sColumn->id )
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
IDE_RC smiHashTempTable::buildKeyColumns( smiTempTableHeader  * aTempHeader,
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
    aTempHeader->mKeyEndOffset  = sKeyColumns->column->offset + sKeyColumns->column->size;

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

        if ( aTempHeader->mKeyEndOffset < ( sKeyColumns->column->offset +
                                            sKeyColumns->column->size ))
        {
            aTempHeader->mKeyEndOffset = ( sKeyColumns->column->offset +
                                           sKeyColumns->column->size );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***************************************************************************
 * Description : TempTable�� �����մϴ�.
 *
 * aStatistics   - [IN] �������
 * aSpaceID      - [IN] TablespaceID
 * aWorkAreaSize - [IN] ������ TempTable�� WA ũ��. (0�̸� �ڵ� ����)
 * aStatement    - [IN] Statement
 * aFlag         - [IN] TempTable�� Ÿ���� ������
 * aColumnList   - [IN] Column ���
 * aKeyColumns   - [IN] KeyColumn(������) Column ���
 * aTable        - [OUT] ������ TempTable
 ***************************************************************************/
IDE_RC smiHashTempTable::create( idvSQL              * aStatistics,
                                 scSpaceID             aSpaceID,
                                 ULong                 aInitWorkAreaSize,
                                 smiStatement        * aStatement,
                                 UInt                  aFlag,
                                 const smiColumnList * aColumnList,
                                 const smiColumnList * aKeyColumns,
                                 const void         ** aTable )
{
    smiTempTableHeader   * sHeader;
    UInt                   sState = 0;

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

    IDU_FIT_POINT( "smiHashTempTable::create::alloc" );
    IDE_TEST( smiTempTable::allocTempTableHdr( &sHeader ) != IDE_SUCCESS );
    sState = 1;
    idlOS::memset( sHeader, 0, ID_SIZEOF( smiTempTableHeader ) );

    /************ TempTable ������� �ʱ�ȭ *************/
    smiTempTable::initStatsPtr( sHeader,
                                aStatistics,
                                aStatement );
    
    generateHashStats( sHeader,
                       SMI_TTOPR_CREATE );
    
    sHeader->mRowCount       = 0;
    sHeader->mWASegment      = NULL;
    sHeader->mTTState        = SMI_TTSTATE_INIT;
    sHeader->mTTFlag         = aFlag | SMI_TTFLAG_TYPE_HASH ;
    sHeader->mSpaceID        = aSpaceID;
    sHeader->mHitSequence    = 1;
    sHeader->mTempCursorList = NULL;
    sHeader->mStatistics     = aStatistics;
    sHeader->mCheckCnt       = 0;

    sHeader->mSortGroupID  = 0;
    sHeader->mRowBuffer4Compare    = NULL;
    sHeader->mRowBuffer4CompareSub = NULL;

    IDE_TEST( getColumnCount( sHeader,
                              aColumnList ) != IDE_SUCCESS );

    /* smiTempTable_create_malloc_Columns.tc */
    IDU_FIT_POINT("smiHashTempTable::create::malloc::Columns");
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

    if ( aKeyColumns != NULL )
    {
        IDE_TEST( buildKeyColumns( sHeader,
                                   aKeyColumns ) != IDE_SUCCESS );
    }
    else
    {
        sHeader->mKeyEndOffset = sHeader->mRowSize;
    }

    /* smiTempTable_create_malloc_RowBuffer4Fetch.tc */
    IDU_FIT_POINT("smiHashTempTable::create::malloc::RowBuffer4Fetch");
    if ( SM_IS_FLAG_ON( sHeader->mTTFlag,  SMI_TTFLAG_UNIQUE ) )
    {
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMI,
                                     sHeader->mRowSize,
                                     (void**)&sHeader->mRowBuffer4Fetch )
                  != IDE_SUCCESS );
        sHeader->mStatsPtr->mRuntimeMemSize += sHeader->mRowSize;
    }
    sState = 3;

    IDE_TEST( sdtHashModule::createHashSegment( sHeader,
                                                aInitWorkAreaSize )
              != IDE_SUCCESS );
    sState = 4;

    sdtHashModule::initHashTempHeader( sHeader );

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
    case 4:
        (void)sdtHashModule::dropHashSegment( (sdtHashSegHdr*)sHeader->mWASegment );
    case 3:
        if ( sHeader->mRowBuffer4Fetch != NULL )
        {
            (void)iduMemMgr::free( sHeader->mRowBuffer4Fetch );
            sHeader->mRowBuffer4Fetch = NULL;
        }
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
 * Description : TempTable�� �����մϴ�.
 *
 * aTable   - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiHashTempTable::drop( void    * aTable )
{
    UInt                 sState  = 5;
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    /* Drop�� �ݵ�� ���� �Ͽ��� �ϱ� ������ Session check�� ���� �ʴ´�.*/
    sHeader->mStatsPtr->mDropTV = smiGetCurrTime();
    generateHashStats( sHeader,
                       SMI_TTOPR_DROP );

    sdtHashModule::estimateHashSize( sHeader );

    IDE_TEST( closeAllCursor( sHeader ) != IDE_SUCCESS );

    sState = 4;
    IDE_TEST( sdtHashModule::dropHashSegment( (sdtHashSegHdr*)sHeader->mWASegment ) != IDE_SUCCESS );

    /* ������� ������Ŵ */
    smiTempTable::accumulateStats( sHeader );

    sState = 3;
    if ( sHeader->mNullRow != NULL )
    {
        IDE_TEST( iduMemMgr::free( sHeader->mNullRow )
                  != IDE_SUCCESS );
        sHeader->mNullRow = NULL;
    }

    sState = 2;
    if ( sHeader->mRowBuffer4Fetch != NULL )
    {
        IDE_TEST( iduMemMgr::free( sHeader->mRowBuffer4Fetch )
                  != IDE_SUCCESS );
        sHeader->mRowBuffer4Fetch = NULL;
    }
    sState = 1;
    IDE_TEST( iduMemMgr::free( sHeader->mColumns )
              != IDE_SUCCESS );
    sHeader->mColumns = NULL;

    sState = 0;
    IDE_TEST( smiTempTable::freeTempTableHdr( sHeader ) != IDE_SUCCESS );
    sHeader = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 5:
        (void)sdtHashModule::dropHashSegment( (sdtHashSegHdr*)sHeader->mWASegment );
    case 4:
        if ( sHeader->mNullRow != NULL )
        {
            (void)iduMemMgr::free( sHeader->mNullRow );
        }
    case 3:
        if ( sHeader->mRowBuffer4Fetch != NULL )
        {
            (void)iduMemMgr::free( sHeader->mRowBuffer4Fetch );
        }
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
 * Description : truncateTable�ϰ� Ŀ���� �ݽ��ϴ�.
 *
 * aTable   - [IN] ��� Table
 ***************************************************************************/
IDE_RC smiHashTempTable::clear( void   * aTable )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;
    sdtHashSegHdr      * sWASeg  = (sdtHashSegHdr*)sHeader->mWASegment;
    
    //BUG-48477: 
    static UInt sTRCChkCnt = 0; 

    if ( ( sHeader->mTTState != SMI_TTSTATE_INIT ) ||
         ( sHeader->mRowCount != 0 ) ||
         ( sWASeg->mUsedWCBPtr != SDT_USED_PAGE_PTR_TERMINATED ) ||
         ( sWASeg->mOpenCursorType > SMI_HASH_CURSOR_INIT ) ||
         ( sWASeg->mHashSlotAllocPageCount != 0 ) ||
         ( sWASeg->mAllocWAPageCount != 0 ) ) 
    {
        if ( sHeader->mTTState == SMI_TTSTATE_INIT )
        {
            if ( sTRCChkCnt < 10 )
            {   
                ideLog::log(IDE_ERR_0,  "RowCount               = %"ID_INT64_FMT"\n"
                                        "mUsedWCBPtr            = %"ID_xPOINTER_FMT"\n"
                                        "OpenCursorType         = %"ID_UINT32_FMT"\n"
                                        "HashSlotAllocPageCount = %"ID_UINT64_FMT"\n"
                                        "AllocWAPageCount       = %"ID_UINT64_FMT"\n", 
                                        sHeader->mRowCount, 
                                        sWASeg->mUsedWCBPtr, 
                                        sWASeg->mOpenCursorType, 
                                        sWASeg->mHashSlotAllocPageCount,
                                        sWASeg->mAllocWAPageCount );
                sTRCChkCnt++;
            }
        }

        generateHashStats( sHeader,
                           SMI_TTOPR_CLEAR );

        sdtHashModule::estimateHashSize( sHeader );

        IDE_TEST( closeAllCursor( sHeader ) != IDE_SUCCESS );

        sHeader->mRowCount       = 0;
        sHeader->mTTState        = SMI_TTSTATE_INIT;
        sHeader->mHitSequence    = 1;

        IDE_TEST( sdtHashModule::truncateHashSegment( sWASeg )
                  != IDE_SUCCESS );

        sdtHashModule::initHashTempHeader( sHeader );
    } 
    else
    {
        if ( sHeader->mHitSequence != 1 ) 
        {
            sHeader->mHitSequence = 1;
        }
    }

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
 * aTable    - [IN] ��� Table
 ***************************************************************************/
void smiHashTempTable::clearHitFlag( void   * aTable )
{
    IDE_DASSERT( ((smiTempTableHeader*)aTable)->mHitSequence < ID_UINT_MAX );
    ((smiTempTableHeader*)aTable)->mHitSequence++;
}

/**************************************************************************
 * Description : Hash�� �����͸� �����մϴ�.
 *
 * aTable       - [IN] ��� Table
 * aValue       - [IN] ������ Value
 * aHashValue   - [IN] ������ HashValue ( HashTemp�� ��ȿ )
 * aGRID        - [OUT] ������ ��ġ
 * aResult      - [OUT] ������ �����Ͽ��°�? ( UniqueViolation Check�� )
 ***************************************************************************/
IDE_RC smiHashTempTable::insert( void     * aTable,
                                 smiValue * aValue,
                                 UInt       aHashValue,
                                 scGRID   * aGRID,
                                 idBool   * aResult )
{
    SDT_CHECK_HASHSTATS( (smiTempTableHeader*)aTable,
                         SMI_TTOPR_INSERT );

    return sdtHashModule::insert( (smiTempTableHeader*)aTable,
                                  aValue,
                                  aHashValue,
                                  aGRID,
                                  aResult );
}

/**************************************************************************
 * Description : Ư�� row�� �����մϴ�.
 *
 * aCursor  - [IN] ������ Row�� ����Ű�� Ŀ��
 * aValue   - [IN] ������ Value
 ***************************************************************************/
IDE_RC smiHashTempTable::update(smiHashTempCursor * aCursor,
                                smiValue          * aValue )
{
    SDT_CHECK_HASHSTATS( (smiTempTableHeader*)aCursor->mTTHeader,
                         SMI_TTOPR_UPDATE );

    return sdtHashModule::update( aCursor, aValue ) ;
}

/**************************************************************************
 * Description : HitFlag�� �����մϴ�.
 *
 * aCursor   - [IN] ������ Row�� ����Ű�� Ŀ��
 ***************************************************************************/
void smiHashTempTable::setHitFlag( smiHashTempCursor * aCursor )
{
    SDT_CHECK_HASHSTATS( (smiTempTableHeader*)aCursor->mTTHeader,
                         SMI_TTOPR_SETHITFLAG );

    sdtHashModule::setHitFlag( aCursor );

    return ;
}

/**************************************************************************
 * Description : HitFlag ���θ� �����մϴ�.
 *
 * aCursor   - [IN] ���� Row�� ����Ű�� Ŀ��
 ***************************************************************************/
idBool smiHashTempTable::isHitFlagged( smiHashTempCursor * aCursor )
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
    return sdtHashModule::isHitFlagged( aCursor );
}


/**************************************************************************
 * Description : GRID�� Row�� �����ɴϴ�.
 *
 * aTable       - [IN] ��� Table
 * aGRID        - [IN] ��� Row
 * aDestRowBuf  - [OUT] ������ Row�� ������ ����
 ***************************************************************************/
IDE_RC smiHashTempTable::fetchFromGRID( void     * aTable,
                                        scGRID     aGRID,
                                        void     * aDestRowBuf )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;

    SDT_CHECK_HASHSTATS( sHeader,
                         SMI_TTOPR_FETCH_FROMGRID );

    IDE_DASSERT( ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WAMAP ) &&
                 ( SC_MAKE_SPACE( aGRID ) != SDT_SPACEID_WORKAREA ) );

    return sdtHashModule::fetchFromGRID( sHeader,
                                         aGRID,
                                         aDestRowBuf );
}


/***************************************************************************
 * Description : Ŀ���� �Ҵ��Ѵ�.
 *
 * aTable      - [IN] ��� Table
 * aColumns    - [IN] ������ Column ����
 * aCursorType - [IN] Cursor�� Type�� �����Ѵ�.
 * aCursor     - [OUT] �Ҵ��� Cursor�� ��ȯ
 ***************************************************************************/
IDE_RC smiHashTempTable::allocHashCursor( void                * aTable,
                                          const smiColumnList * aColumns,
                                          UInt                  aCursorType,
                                          smiHashTempCursor  ** aCursor )
{
    smiTempTableHeader * sHeader = (smiTempTableHeader*)aTable;
    smiHashTempCursor  * sCursor;
    const smiColumn    * sColumn;
    UInt                 sState = 0;

    IDU_FIT_POINT_RAISE( "smiHashTempTable::openCursor::alloc", memery_allocate_failed );

    /* �ٷ� �տ� �Ҵ��� cursor �������� Ȯ���Ѵ�.
     * hash cursor�� update� ���ȴ�.
     * mWCBPtr�� ���� �� �� ���� writePage �ϴµ�
     * ���� mWcb�� �����ְ�, dirty���
     * �� cursor�� �ű�� ���� flush ���ش�.*/
    sCursor = (smiHashTempCursor*)sHeader->mTempCursorList;

    IDE_TEST( mTempCursorPool.alloc( (void**)&sCursor ) != IDE_SUCCESS );
    sState = 1;

    sCursor->mTTHeader      = sHeader;
    sCursor->mUpdateColumns = (smiColumnList*)aColumns;
    sCursor->mUpdateEndOffset = 0;
    sCursor->mWASegment     = sHeader->mWASegment;
    sCursor->mWCBPtr        = NULL;

    while ( aColumns != NULL )
    {
        sColumn = aColumns->column;
        if (( sColumn->offset + sColumn->size ) > sCursor->mUpdateEndOffset )
        {
            sCursor->mUpdateEndOffset = sColumn->offset + sColumn->size;
        }
        aColumns = aColumns->next;
    }

    IDE_TEST( sdtHashModule::initHashCursor( sCursor,
                                             aCursorType ) != IDE_SUCCESS );
    sState = 2;

    *aCursor = sCursor;

    /* ����ó���� �Ͼ�� �ʴ� ��찡 �Ǿ�� Link�� ������ */
    sCursor->mNext = (smiHashTempCursor*)sHeader->mTempCursorList;
    sHeader->mTempCursorList = (void*)sCursor;

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( memery_allocate_failed );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
#endif
    IDE_EXCEPTION_END;

    switch( sState )
    {
    case 2:
        // nothing
    case 1:
        (void) mTempCursorPool.memfree( sCursor );
        break;
    default:
        break;
    }

    smiTempTable::checkAndDump( sHeader );

    return IDE_FAILURE;
}


/***************************************************************************
 * Description : Hash Scan�� Ŀ���� ����. HashValue�� ���� �� �� ���� �ٽ� ����.
 * �ش� Hash Row�� �ִ��� Hash Value�� ������� �ؼ�,Hash Slot�� ���� 1�� �Ǵ��Ͽ�
 * ������ FALSE��, �������� �ִٰ� �ǴܵǸ� TRUE�� �����Ѵ�.
 * (True - �������� �ְ� �������� ����, False - Ȯ���ϰ� ����)
 * True return�ÿ� Hash value�� Hash slot�� pointer�� Cursor�� �����Ѵ�.
 * Hash Temp Table���� ���� ���� ȣ��Ǵ� �Լ� �� �ϳ��� ������ ����� �߿��ϴ�.
 *
 * aCursor    - [IN/OUT] ��� Cursor
 * aHashValue - [IN] Ž�� �� Row�� Hash Value
 ***************************************************************************/
idBool smiHashTempTable::checkHashSlot( smiHashTempCursor* aHashCursor,
                                        UInt               aHashValue )
{
    IDU_FIT_POINT( "1.BUG-48313@smiHashTempTable::checkHashSlot" );
    
    return sdtHashModule::checkHashSlot( aHashCursor,
                                         aHashValue );
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;
    return ID_FALSE;
#endif
}

/***************************************************************************
 * Description : Update�� Scan�� Ŀ���� �ٽ� ����.
 *
 * aCursor    - [IN/OUT] ��� Cursor
 * aHashValue - [IN] Ž�� �� Row�� Hash Value
 ***************************************************************************/
idBool smiHashTempTable::checkUpdateHashSlot( smiHashTempCursor* aHashCursor,
                                              UInt               aHashValue )
{
    return sdtHashModule::checkUpdateHashSlot( aHashCursor,
                                               aHashValue );
}

/***************************************************************************
 * Description : ����(HashValue, KeyFilter)�� �´� ù��° Row�� ��ȯ�Ѵ�.
 *               WorkArea�� Ž�� �����ϵ��� �籸�� �ϴ� ���ҵ� �Ѵ�.
 *               openHashCursor�� �ִ��� ������ �����ϵ��� �ϱ� ���Ͽ�
 *               �Ϻ� cursor open�� ����� �ŰܿԴ�.
 *
 * aCursor     - [IN] ��� Cursor
 * aFlag       - [IN] TempCursor�� Ÿ��/�뵵/���� ���� ��Ÿ��
 * aRowFilter  - [IN] RowFilter
 * aRow        - [OUT] ã�� Row
 * aRowGRID    - [OUT] ã�� row�� GRID
 ***************************************************************************/
IDE_RC smiHashTempTable::openHashCursor( smiHashTempCursor   * aCursor,
                                         UInt                  aFlag,
                                         const smiCallBack   * aRowFilter,
                                         UChar              ** aRow,
                                         scGRID              * aRowGRID )
{
    /* Insert�� ���� ���ٸ� cursor�� open ���� �ʴ´�. */
    IDE_DASSERT( aCursor->mTTHeader->mRowCount != 0 );

    SDT_CHECK_HASHSTATS( aCursor->mTTHeader,
                         SMI_TTOPR_OPENCURSOR_HASH );

    SM_SET_FLAG_OFF( aFlag, SMI_TCFLAG_FILTER_MASK );

    if ( aRowFilter != smiGetDefaultFilter() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_ROW );
    }

    aCursor->mRowFilter = aRowFilter;
    aCursor->mTCFlag    = aFlag;

    return sdtHashModule::openHashCursor( aCursor,
                                          aRow,
                                          aRowGRID );
}


/***************************************************************************
 * Description : Update�� Cursor�� ���ؼ� ù�� ° row�� �޾ƿ´�.
 *               Update Cursor�� Unique Hash�̹Ƿ� FetchNext�� ����.
 *
 * aCursor     - [IN] ��� Cursor
 * aFlag       - [IN] TempCursor�� Ÿ��/�뵵/���� ���� ��Ÿ��
 * aRowFilter  - [IN] RowFilter
 * aRow        - [OUT] ã�� Row
 * aRowGRID    - [OUT] ã�� row�� GRID
 ***************************************************************************/
IDE_RC smiHashTempTable::openUpdateHashCursor( smiHashTempCursor   * aCursor,
                                               UInt                  aFlag,
                                               const smiCallBack   * aRowFilter,
                                               UChar              ** aRow,
                                               scGRID              * aRowGRID )
{
    /* Insert�� ���� ���ٸ� open�� ȣ����� �ʴ´�. */
    IDE_DASSERT( aCursor->mTTHeader->mRowCount != 0 );

    SDT_CHECK_HASHSTATS( aCursor->mTTHeader,
                         SMI_TTOPR_OPENCURSOR_UPDATE );

    SM_SET_FLAG_OFF( aFlag, SMI_TCFLAG_FILTER_MASK );

    if ( aRowFilter != smiGetDefaultFilter() )
    {
        SM_SET_FLAG_ON( aFlag, SMI_TCFLAG_FILTER_ROW );
    }

    aCursor->mRowFilter = aRowFilter;
    aCursor->mTCFlag    = aFlag;

    return sdtHashModule::openUpdateHashCursor( aCursor,
                                                aRow,
                                                aRowGRID );
}


/***************************************************************************
 * Description : Full Scan���� ù��° Row�� �����´�.
 *
 * aCursor     - [IN] ��� Cursor
 * aFlag       - [IN] TempCursor�� Ÿ��/�뵵/���� ���� ��Ÿ��
 * aRow        - [OUT] ã�� Row
 * aRowGRID    - [OUT] ã�� row�� GRID
 ***************************************************************************/
IDE_RC smiHashTempTable::openFullScanCursor( smiHashTempCursor   * aCursor,
                                             UInt                  aFlag,
                                             UChar              ** aRow,
                                             scGRID              * aRowGRID )
{
    if ( aCursor->mTTHeader->mRowCount != 0 )
    {
        SDT_CHECK_HASHSTATS( aCursor->mTTHeader,
                             SMI_TTOPR_OPENCURSOR_FULL );

        SM_SET_FLAG_OFF( aFlag, SMI_TCFLAG_FILTER_MASK );

        aCursor->mTCFlag = aFlag;

        IDE_DASSERT( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) );
        IDE_DASSERT( SM_IS_FLAG_OFF( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) );

        return sdtHashModule::openFullScanCursor( aCursor,
                                                  aRow,
                                                  aRowGRID );
    }
    else
    {
        SC_MAKE_NULL_GRID(*aRowGRID);
        *aRow = NULL;
        aCursor->mWAPagePtr = NULL; // fetch full next�� ȣ�� �� ���� �����
        return IDE_SUCCESS;
    }
}

/**************************************************************************
 * Description : Cursor�� ���� ���� Row�� �����ɴϴ�(FullScan)
 *
 * aCursor   - [IN] ��� Cursor
 * aRow      - [OUT] ��� Row
 * aGRID     - [OUT] ������ Row�� GRID
 ***************************************************************************/
IDE_RC smiHashTempTable::fetchFullNext( smiHashTempCursor  * aCursor,
                                        UChar             ** aRow,
                                        scGRID             * aRowGRID )
{
    if ( aCursor->mTTHeader->mRowCount != 0 )
    {
        SDT_CHECK_HASHSTATS( aCursor->mTTHeader,
                             SMI_TTOPR_FETCH_FULL );

        return sdtHashModule::fetchFullNext( aCursor,
                                             aRow,
                                             aRowGRID);
    }
    else
    {
        SC_MAKE_NULL_GRID(*aRowGRID);
        *aRow = NULL;
        aCursor->mWAPagePtr = NULL; // fetch full next�� ȣ�� �� ���� �����

        return IDE_SUCCESS;
    }
}

/**************************************************************************
 * Description : Cursor�� ���� ���� Row�� �����ɴϴ�( Hash Scan )
 *
 * aCursor   - [IN] ��� Cursor
 * aRow      - [OUT] ��� Row
 * aGRID     - [OUT] ������ Row�� GRID
 ***************************************************************************/
IDE_RC smiHashTempTable::fetchHashNextInternal( smiHashTempCursor  * aCursor,
                                                UChar             ** aRow,
                                                scGRID             * aRowGRID )
{
    SDT_CHECK_HASHSTATS( (smiTempTableHeader*)aCursor->mTTHeader,
                         SMI_TTOPR_FETCH_HASH );

    return sdtHashModule::fetchHashNext( aCursor,
                                         aRow,
                                         aRowGRID );
}

/**************************************************************************
 * Description : ��� Ŀ���� �ݽ��ϴ�.
 *
 * aHeader  - [IN] ��� Temp Table Header
 ***************************************************************************/
IDE_RC smiHashTempTable::closeAllCursor( smiTempTableHeader * aHeader )
{
    smiHashTempCursor  * sCursor;
    smiHashTempCursor  * sNxtCursor;

    sCursor = (smiHashTempCursor*)aHeader->mTempCursorList;
    while( sCursor != NULL )
    {
        sNxtCursor = sCursor->mNext;

        IDE_TEST( mTempCursorPool.memfree( sCursor ) != IDE_SUCCESS );

        sCursor = sNxtCursor;
    }

    aHeader->mTempCursorList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Page����, Record������ ��ȯ��. PlanGraph ��� ����
 *
 * aTable       - [IN] ��� Table
 * aPageCount   - [OUT] �� Page ����
 * aRecordCount - [OUT] �� Row ����
 ***************************************************************************/
void smiHashTempTable::getDisplayInfo( void  * aTable,
                                       ULong * aPageCount,
                                       SLong * aRecordCount )
{
    smiTempTableHeader * sHeader    = (smiTempTableHeader*)aTable;
    sdtHashSegHdr      * sWASegment = (sdtHashSegHdr*)sHeader->mWASegment;

    *aRecordCount = sHeader->mRowCount;

    // BUG-39728
    // parallel aggregation �� ����Ҷ� temp table �� �����ؼ� ������ �߻��� ���
    // getDisplayInfo �� ȣ���ϸ� FATAL �� �߻��Ѵ�.
    // �̶� sHeader �� ���������� sWASegment�� NULL �̴�.
    // �� ��Ȳ���� getDisplayInfo �� return ���� �߿�ġ �����Ƿ� FATAL �� �߻����� �ʵ��� �Ѵ�.
    if ( sWASegment != NULL )
    {
        *aPageCount = sdtHashModule::getNExtentCount( sWASegment ) * SDT_WAEXTENT_PAGECOUNT ;
    }
    else
    {
        *aPageCount = 0;
    }

    return ;
}

/**************************************************************************
 * Description : HashSlot�� �ִ� ������ �����´�.
 *
 * aTable       - [IN] ��� Table (NULL�� ��� ������)
 ***************************************************************************/
ULong smiHashTempTable::getMaxHashBucketCount( void * aTable )
{
    smiTempTableHeader * sHeader;
    sdtHashSegHdr      * sWASegment;
    ULong                sMaxBucketCount;
    ULong                sWAExtentCount;

    if ( aTable != NULL )
    {
        sHeader       = (smiTempTableHeader*)aTable;
        sWASegment    = (sdtHashSegHdr*)sHeader->mWASegment;
        sWAExtentCount = sWASegment->mMaxWAExtentCount;
    }
    else
    {
        sWAExtentCount = sdtHashModule::getMaxWAExtentCount();
    }

    sMaxBucketCount = ( sWAExtentCount - 1 ) * SDT_WAEXTENT_SIZE / ID_SIZEOF( sdtHashSlot );

    return sMaxBucketCount;
}

/**************************************************************************
 * Description : WorkArea ���� Row�� �ִ� ���� ������ �����´�.
 *
 * aTable       - [IN] ��� Table
 ***************************************************************************/
ULong smiHashTempTable::getRowAreaSize( void * aTable )
{
    smiTempTableHeader * sHeader;
    sdtHashSegHdr      * sWASegment;
    ULong                sRowPageCount;

    if ( aTable != NULL )
    {
        sHeader       = (smiTempTableHeader*)aTable;
        sWASegment    = (sdtHashSegHdr*)sHeader->mWASegment;
        sRowPageCount = ( sWASegment->mMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT )
                        - sWASegment->mHashSlotPageCount - sWASegment->mSubHashPageCount ;
    }
    else
    {
        sRowPageCount = ( sdtHashModule::getMaxWAExtentCount() * SDT_WAEXTENT_PAGECOUNT )
                        - sdtHashModule::getHashSlotPageCount()
                        - sdtHashModule::getSubHashPageCount() ;
    }

    return sRowPageCount * SD_PAGE_SIZE;
}

/**************************************************************************
 * Description : ����Ǵ� ��������� �ٽ� ������
 *
 * aHeader  - [IN] ��� Table
 * aOPr     - [IN] ������ Operation
 ***************************************************************************/
void smiHashTempTable::generateHashStats( smiTempTableHeader * aHeader,
                                          smiTempTableOpr      aOpr )
{
    smiTempTableStats * sStats ;
    sdtHashSegHdr     * sWASegment = (sdtHashSegHdr*)aHeader->mWASegment;

    if ( ( smiGetCurrTime() - aHeader->mStatsPtr->mCreateTV >=
           smuProperty::getTempStatsWatchTime() ) &&
         ( aHeader->mStatsPtr == &aHeader->mStatsBuffer ) )
    {
        /* ��ȿ�ð� �̻� ���������鼭, ���� WatchArray�� ��� �ȵž�������
         * ����Ѵ�. */
        aHeader->mStatsPtr = smiTempTable::registWatchArray( aHeader->mStatsPtr );

        if ( sWASegment != NULL )
        {
            sWASegment->mStatsPtr = aHeader->mStatsPtr;
        }
    }
    sStats = aHeader->mStatsPtr;

    sStats->mTTState   = aHeader->mTTState;
    sStats->mTTLastOpr = aOpr;

    if ( sWASegment != NULL )
    {
        sStats->mMaxWorkAreaSize =
            sdtHashModule::getMaxWAPageCount( sWASegment ) * SD_PAGE_SIZE;
        sStats->mUsedWorkAreaSize =
            sdtHashModule::getWASegmentUsedPageCount( sWASegment ) * SD_PAGE_SIZE;
        sStats->mNormalAreaSize = sdtHashModule::getNExtentCount( sWASegment )
            * SDT_WAEXTENT_PAGECOUNT
            * SD_PAGE_SIZE;
    }
    sStats->mRecordLength  = aHeader->mRowSize;
    sStats->mRecordCount   = aHeader->mRowCount;
    sStats->mMergeRunCount = aHeader->mMergeRunCount;
    sStats->mHeight        = aHeader->mHeight;
}

/**************************************************************************
 * Description : Cursor ������ ���
 *
 * aTempCursor  - [IN] ��� Cursor
 * aOutBuf      - [OUT] ������ ����� Buffer
 * aOutSize     - [OUT] Buffer�� ũ��
 ***************************************************************************/
void smiHashTempTable::dumpTempCursor( smiHashTempCursor * aTempCursor,
                                       SChar             * aOutBuf,
                                       UInt                aOutSize )
{
    while ( aTempCursor != NULL )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "DUMP HASH TEMP CURSOR:\n"
                                   "Flag        : %4"ID_UINT32_FMT"\n"
                                   "Seq         : %"ID_UINT32_FMT"\n"
                                   "InMemory    : %4"ID_UINT32_FMT"\n"
                                   "HashValue   : %"ID_XINT32_FMT"\n"
                                   "ChildPageID : %"ID_UINT32_FMT"\n"
                                   "ChildOffset : %"ID_UINT32_FMT"\n"
                                   "SubHashIdx  : %"ID_UINT32_FMT"\n"
                                   "UpdateEnd   : %"ID_UINT32_FMT"\n"
                                   "LastNPageID : %"ID_UINT32_FMT"\n",
                                   aTempCursor->mTCFlag,
                                   aTempCursor->mSeq,
                                   aTempCursor->mIsInMemory == SDT_WORKAREA_OUT_MEMORY ? "OutMemory" : "InMemory",
                                   aTempCursor->mHashValue,
                                   aTempCursor->mChildPageID,
                                   aTempCursor->mChildOffset,
                                   aTempCursor->mSubHashIdx,
                                   aTempCursor->mUpdateEndOffset,
                                   aTempCursor->mNExtLstPID );

        aTempCursor = aTempCursor->mNext;
    }

    return;
}
