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
 * $id$
 **********************************************************************/

#include <dkdRemoteTableMgr.h>
#include <dkdMisc.h>
#include <dktDef.h>

/***********************************************************************
 * Description: Remote table manager �� �ʱ�ȭ�Ѵ�.
 *
 *  aBufBlockCnt    - [IN] �� record buffer manager �� �Ҵ���� buffer
 *                         �� �����ϴ� buffer block �� ����
 *  aAllocator      - [IN] record buffer �� �Ҵ���� �� ����ϱ� ���� 
 *                         TLSF memory allocator �� ����Ű�� ������
 *
 *  aBuffSize  - [OUT] REMOTE_TABLE �� record buffer size
 *
 **********************************************************************/
IDE_RC  dkdRemoteTableMgr::initialize( UInt * aBuffSize )
{
    mIsEndOfFetch           = ID_FALSE;
    mIsFetchBufReadAll      = ID_FALSE;
    mIsFetchBufFull         = ID_FALSE;
    mRowSize                = 0;
    mFetchRowBufSize        = DKU_DBLINK_REMOTE_TABLE_BUFFER_SIZE * DKD_BUFFER_BLOCK_SIZE_UNIT_MB;
    mConvertedRowBufSize    = 0;
    mFetchRowCnt            = 0;
    mInsertRowCnt           = 0;
    mRecordCnt              = 0;
    mFetchRowBuffer         = NULL;
    mConvertedRowBuffer     = NULL;
    mInsertPos              = NULL;
    mTypeConverter          = NULL;
    mCurRow                 = NULL;
    *aBuffSize              = mFetchRowBufSize;

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : ���ݼ����κ��� ������ ���ڵ带 �Ͻ������� ������ ���� 
 *               �� �ʿ��� �ڿ��� �Ҵ��Ѵ�.
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::activate()
{
    IDE_ASSERT( mTypeConverter != NULL );

    mConvertedRowBuffer = NULL;
    
    /* mRowSize */
    IDE_TEST( dkdTypeConverterGetRecordLength( mTypeConverter, &mRowSize )
              != IDE_SUCCESS );

    /* mFetchRowBufSize / mFetchRowCnt / mFetchRowBuffer */
    IDE_TEST( makeFetchRowBuffer() != IDE_SUCCESS );

    /* mConvertedRowBufSize / mConvertedRowBuffer */
    if ( mRowSize > DKP_ADLP_PACKET_DATA_MAX_LEN ) 
    {
        mConvertedRowBufSize = mRowSize;
    }
    else
    {
        mConvertedRowBufSize = DK_MAX_PACKET_LEN;
    }

    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_DK,
                                       1,
                                       mConvertedRowBufSize,
                                       (void **)&mConvertedRowBuffer,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;
    
    if ( mConvertedRowBuffer != NULL )
    {
        (void)iduMemMgr::free( mConvertedRowBuffer );
        mConvertedRowBuffer = NULL;
    }
    else
    {
        /* do nothing */
    }
    
    return IDE_FAILURE;
}

/************************************************************************
 * Description : ���ݼ����κ��� fetch �ؿ� ���ڵ带 insert �ϱ� ���� 
 *               �Ͻ������� ������ ���۸� �Ҵ��Ѵ�.
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::makeFetchRowBuffer()
{
    idBool              sIsAllocated    = ID_FALSE;

    if ( mFetchRowBuffer == NULL )
    {
        if ( mRowSize > 0 )
        {
            IDE_TEST( dkdMisc::getFetchRowCnt( mRowSize,
                                               mFetchRowBufSize,
                                               &mFetchRowCnt,
                                               DKT_STMT_TYPE_REMOTE_TABLE )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                               mFetchRowBufSize,
                                               (void **)&mFetchRowBuffer,
                                               IDU_MEM_IMMEDIATE )
                            != IDE_SUCCESS, ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );

            sIsAllocated = ID_TRUE;

            cleanFetchRowBuffer();
        }
        else
        {
            /* row size is '0' */
            mFetchRowBufSize = 0;
        }
    }
    else
    {
        /* already allocated */
        cleanFetchRowBuffer();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAllocated == ID_TRUE )
    {
        (void)iduMemMgr::free( mFetchRowBuffer );
        mFetchRowBuffer = NULL;
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : fetch row buffer �� �޸𸮿��� �����Ѵ�. 
 *
 ************************************************************************/
void    dkdRemoteTableMgr::freeFetchRowBuffer()
{
    if ( mFetchRowBuffer != NULL )
    {
        (void)iduMemMgr::free( mFetchRowBuffer );

        mFetchRowBuffer = NULL;
        mInsertPos      = NULL;
        mCurRow         = NULL;
    }
    else
    {
        /* do nothing */
    }
}

/************************************************************************
 * Description : converted row buffer �� �޸𸮿��� �����Ѵ�. 
 *
 ************************************************************************/
void    dkdRemoteTableMgr::freeConvertedRowBuffer()
{
    if ( mConvertedRowBuffer != NULL )
    {
        (void)iduMemMgr::free( mConvertedRowBuffer );

        mConvertedRowBuffer  = NULL;
        mConvertedRowBufSize = 0;
    }
    else
    {
        /* do nothing */
    }
}
/************************************************************************
 * Description : ���ݼ����κ��� fetch �ؿ� ���ڵ带 insert �ϱ� ���� 
 *               �Ͻ������� ������ ���۸� �Ҵ��Ѵ�.
 *
 ************************************************************************/
void    dkdRemoteTableMgr::cleanFetchRowBuffer()
{
    if ( mFetchRowBuffer != NULL )
    {
        idlOS::memset( mFetchRowBuffer, 0, mFetchRowBufSize );
    
        mInsertRowCnt      = 0;
        mIsFetchBufFull    = ID_FALSE;
        mIsFetchBufReadAll = ID_FALSE;
        mInsertPos         = mFetchRowBuffer;
    }
    else
    {
        /* do nothing */
    }
}

/************************************************************************
 * Description : Remote table manager �� �����Ѵ�.
 *
 ************************************************************************/
void dkdRemoteTableMgr::finalize()
{
    freeFetchRowBuffer();
    freeConvertedRowBuffer();

    if ( mTypeConverter != NULL )
    {
        (void)destroyTypeConverter();
    }
    else
    {
        /* do nothing */
    }
}

/************************************************************************
 * Description : Type converter �� �����Ѵ�. Type converter �� result
 *               set meta ������ ���� �ִ�. 
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::createTypeConverter( dkpColumn   *aColMetaArr, 
                                                UInt         aColCount )
{
    IDE_TEST( dkdTypeConverterCreate( aColMetaArr, 
                                      aColCount, 
                                      &mTypeConverter )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Type converter �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::destroyTypeConverter()
{
    return dkdTypeConverterDestroy( mTypeConverter );
}

/************************************************************************
 * Description : Type converter �� altibase type ���� ��ȯ�� ���� �ִ� 
 *               meta ������ ��û�Ѵ�.
 *
 *  aMeta       - [IN] ��û�� meta ������ ��� ����ü ������
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::getConvertedMeta( mtcColumn **aMeta )
{
    IDE_TEST( dkdTypeConverterGetConvertedMeta( mTypeConverter, aMeta )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Fetch row buffer �κ��� record �ϳ��� fetch �ؿ´�. 
 *
 *  aRow        - [OUT] fetch �ؿ� row �� ����Ű�� ������
 *
 ************************************************************************/
void    dkdRemoteTableMgr::fetchRow( void  **aRow )
{
    /* fetch row */
    if ( mCurRow != NULL )
    {
        idlOS::memcpy( *aRow, (void *)mCurRow, mRowSize );

        if ( mInsertRowCnt > 0 )
        {
            mInsertRowCnt--;
            mCurRow += mRowSize;
        }
        else
        {
            /* insert row count is 0 */
            if ( mIsEndOfFetch != ID_TRUE )
            {
                /* remote data remained */
            }
            else
            {
                /* no more record */
                mCurRow = NULL;
                *aRow   = NULL;
            }

            mIsFetchBufReadAll = ID_TRUE;
        }
    }
    else
    {
        /* no more rows exists, success */
        *aRow = NULL;
    }
}

/************************************************************************
 * Description : REMOTE_TABLE �� fetch row buffer �� record �ϳ��� 
 *               insert �Ѵ�. �� ��, �Է¹޴� row �� cm block ���κ��� 
 *               ���� raw data �� mt type ���� ��ȯ�� �� insert �Ѵ�.
 *
 *  aRow        - [IN] insert �� record �� ����Ű�� ������
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::insertRow( void *aRow )
{ 
    IDE_DASSERT ( mInsertRowCnt < mFetchRowCnt );   

    if ( aRow != NULL )
    {
        IDE_TEST( dkdTypeConverterConvertRecord( mTypeConverter, 
                                                 aRow, 
                                                 (void *)mInsertPos ) 
                  != IDE_SUCCESS );

        mInsertRowCnt++;
        mRecordCnt++;

        if ( mInsertRowCnt != mFetchRowCnt )
        {
            mInsertPos += mRowSize;
        }
        else
        {
            mIsFetchBufFull = ID_TRUE;
            mInsertPos      = mFetchRowBuffer;
        }
    }
    else
    {
        /* NULL insertion represents the end of fetch */
        mIsEndOfFetch = ID_TRUE;    
        mInsertPos    = mFetchRowBuffer;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Record buffer �� iteration �� restart ��Ų��.
 *
 ************************************************************************/
IDE_RC  dkdRemoteTableMgr::restart()
{    
    IDE_TEST( makeFetchRowBuffer() != IDE_SUCCESS );

    mIsEndOfFetch = ID_FALSE;
    mInsertRowCnt = 0;
    mRecordCnt    = 0;
    mCurRow       = mFetchRowBuffer;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

