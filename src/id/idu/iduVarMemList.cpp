/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id: iduVarMemList.cpp 17293 2006-07-25 03:04:24Z sbjang $
 **********************************************************************/

#include <iduVarMemList.h>
#include <ideLog.h>
#include <idp.h>
#include <idtContainer.h>

idBool        iduVarMemList::mUsePrivate = ID_FALSE;

IDE_RC iduVarMemList::initializeStatic( void )
{
#define IDE_FN "initializeStatic()"
    iduVarMemList::mUsePrivate = iduMemMgr::isPrivateAvailable();
    return IDE_SUCCESS;

    /*
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
    */
#undef IDE_FN
}

IDE_RC iduVarMemList::destroyStatic( void )
{
#define IDE_FN "destroyStatic()"
    iduVarMemList::mUsePrivate = ID_FALSE;
    return IDE_SUCCESS;
#undef IDE_FN
}

/*
 * �޸� �޴����� �ʱ�ȭ �۾��� �����Ѵ�.
 * ST �Լ� �������� �̹� �ʱ�ȭ�� �޸� �Ŵ����� ���� �����͸� �� ����ϱ� ������ �Ű澵 �ʿ䰡 ����. QP������ ���� aIndex�� ������ IDU_MEM_QMT�� ����ϰ� �ִ�.
 */
IDE_RC iduVarMemList::init( iduMemoryClientIndex aIndex,
                            ULong                aMaxSize )
{
#define IDE_FN "init( iduMemoryClientIndex aIndex )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mSize    = 0;
    mIndex   = aIndex;
    mMaxSize = aMaxSize;
    mCount   = 0;

#if defined(ALTIBASE_MEMORY_CHECK)
    mUsePrivate = ID_FALSE;
#endif
    /*
     * TASK-4930
     */
    if (mUsePrivate == ID_TRUE)
    {
        IDE_TEST(iduMemMgr::createAllocator(&mPrivateAlloc) != IDE_SUCCESS);
    }
    else
    {
        (void)mPrivateAlloc.initialize(NULL);
    }
    
    IDU_LIST_INIT( &mNodeList );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-33688
       Even though the init function fails, the other member functions can be called.
       Therefore we resets these values to prevent the problem of the functions. */
    mMaxSize = 0;

    (void)mPrivateAlloc.initialize(NULL);

    IDU_LIST_INIT( &mNodeList );

    return IDE_FAILURE;
#undef IDE_FN
}

/*
 * �޸� �Ŵ����� ���� �۾��� �����Ѵ�.
 * alloc�̳� realloc �Լ��� ����ؼ� �̹� �Ҵ� �Ǿ����� ���� �������� ���� �޸𸮿� ���� ���� �۾� �� ���������� ���Ǵ� �޸� Ǯ�� ���� ���� �۾��� �����Ѵ�.
 * ST �Լ� �������� destroy �Լ��� ����� ȣ���ؼ��� �ȵȴ�.
 */
IDE_RC iduVarMemList::destroy()
{
#define IDE_FN "destroy()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( freeAll() != IDE_SUCCESS );

    if (mUsePrivate == ID_TRUE)
    {
        (void)iduMemMgr::freeAllocator(&mPrivateAlloc);
    }
    else
    {
        /* Do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/*
 * aSize�� ������ ũ�� ��ŭ�� �޸� ������ �Ҵ��Ѵ�.
 */
IDE_RC iduVarMemList::alloc( ULong aSize, void **aBuffer )
{
#define IDE_FN "alloc( ULong aSize, void **aBuffer )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    ULong        sSize   = 0;
    SChar       *sBuffer = NULL;
    iduListNode *sNode   = NULL;

    IDE_TEST_RAISE( aSize == 0, err_invalid_argument );

    IDE_TEST_RAISE( (mSize + aSize) > mMaxSize, err_max_allocation );

    sSize = idlOS::align8( ID_SIZEOF(iduListNode) ) + ID_SIZEOF(ULong) + aSize;

    IDE_TEST( iduMemMgr::malloc(mIndex,
                                sSize,
                                (void **)&sBuffer,
                                IDU_MEM_IMMEDIATE,
                                &mPrivateAlloc) != IDE_SUCCESS );

    sNode = (iduListNode *)sBuffer;

    // At the begin of the buffer iduListNode and buffer size are stored.
    (void)setSize( sBuffer + idlOS::align8( ID_SIZEOF(iduListNode) ), aSize );

    *aBuffer = sBuffer + idlOS::align8( ID_SIZEOF(iduListNode) ) + ID_SIZEOF(ULong);

    sNode->mObj = sBuffer + idlOS::align8( ID_SIZEOF(iduListNode) );

    IDU_LIST_ADD_LAST( &mNodeList, sNode );

    mSize += aSize;
    mCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_argument );
    {
        IDE_SET( ideSetErrorCode(idERR_ABORT_idaInvalidParameter) );
    }
    IDE_EXCEPTION( err_max_allocation );
    {
        IDE_SET( ideSetErrorCode(idERR_ABORT_MAX_MEM_SIZE_EXCEED,
                                 iduMemMgr::mClientInfo[mIndex].mName,
                                 (mSize + aSize),
                                 mMaxSize ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduVarMemList::cralloc( ULong aSize, void **aBuffer )
{
#define IDE_FN "alloc( ULong aSize, void **aBuffer )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( alloc( aSize, aBuffer ) != IDE_SUCCESS );

    idlOS::memset( *aBuffer,
                   0,
                   aSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/*
 * �Ҵ�� �޸� ������ �����Ѵ�.
 * alloc �Լ��� ���ؼ� �Ҵ���� ���� �����͸� �ѱ�� ��� ASSERT ó�� �Ѵ�.
 */
IDE_RC iduVarMemList::free( void *aBuffer )
{
#define IDE_FN "free( void *aBuffer )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar       *sBuffer   = (SChar*)aBuffer;
    iduListNode *sNode     = NULL;

    // BUG-37543
    // Slow memory free function, the query optimizer will take a long time.
    sNode = (iduListNode*)( sBuffer -
                            idlOS::align8( ID_SIZEOF(iduListNode) ) -
                            ID_SIZEOF( ULong ) );

    IDE_ASSERT( sNode != NULL );

    IDU_LIST_REMOVE( sNode );

    mSize -= getSize( sNode->mObj );
    mCount--;

    IDE_TEST( iduMemMgr::free( sNode, &mPrivateAlloc ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/*
 * �Ҵ�� ��� �޸𸮿� ���� ���� �۾��� �����Ѵ�.
 * ������ ���������� ���Ǵ� �޸� Ǯ�� ���� ���� �۾��� ������� �ʴ´�.
 */
IDE_RC iduVarMemList::freeAll()
{
#define IDE_FN "freeAll()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduListNode *sIterator = NULL;
    iduListNode *sNodeNext = NULL;

    IDU_LIST_ITERATE_SAFE( &mNodeList, sIterator, sNodeNext )
    {
        mSize -= getSize( sIterator->mObj );
        mCount--;

        IDE_TEST( iduMemMgr::free( sIterator, &mPrivateAlloc ) != IDE_SUCCESS );
    }

    IDU_LIST_INIT( &mNodeList );

    IDE_ASSERT( mSize == 0 );
    IDE_ASSERT( mCount == 0 );    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

ULong iduVarMemList::getSize( void *aBuffer )
{
    return *((ULong *)aBuffer);
}

void iduVarMemList::setSize( void *aBuffer, ULong aSize )
{
    *((ULong *)aBuffer) = aSize;
}

void iduVarMemList::dump()
{
#define IDE_FN "dump()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduListNode *sIterator = NULL;

    idlOS::printf( "--------------------------------------------\n" );
    IDU_LIST_ITERATE( &mNodeList, sIterator )
    {
        idlOS::printf( "[%"ID_UINT64_FMT"][%s]\n",
                        getSize( sIterator->mObj ),
                        (SChar *)sIterator->mObj + ID_SIZEOF(ULong) );
    }
    idlOS::printf( "--------------------------------------------\n\n" );

#undef IDE_FN
}

IDE_RC iduVarMemList::getStatus( iduVarMemListStatus *aStatus )
{
#define IDE_FN "getStatus( iduVarMemListStatus *aStatus )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( alloc( 1, (void **)&aStatus->mCursor ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    IDE_ASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduVarMemList::setStatus( iduVarMemListStatus *aStatus )
{
#define IDE_FN "setStatus( iduVarMemListStatus *aStatus )"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar       *sBuffer   = NULL;
    iduListNode *sIterator = NULL;
    iduListNode *sNodeNext = NULL;
    iduList     *sNodeList = NULL;

    if( aStatus->mCursor != NULL )
    {
        IDU_LIST_ITERATE( &mNodeList, sIterator )
        {
            sBuffer = (SChar *)sIterator->mObj;

            if( aStatus->mCursor == (sBuffer + ID_SIZEOF(ULong)) )
            {
                sNodeList = sIterator;
                break;
            }
        }

        IDE_ASSERT( sNodeList != NULL );

        if( sNodeList != NULL )
        {
            IDU_LIST_ITERATE_SAFE2( &mNodeList, 
                                    sNodeList, 
                                    sIterator, 
                                    sNodeNext )
            {
                // �ڱ� �ڽſ� ���ؼ��� free ���� �ʾƾ� �Ѵ�.
                if( sIterator == sNodeList )
                {
                    continue;
                }

                IDU_LIST_REMOVE( sIterator );

                mSize -= getSize( sIterator->mObj );
                mCount--;

                IDE_TEST( iduMemMgr::free( sIterator, &mPrivateAlloc ) != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduVarMemList::freeRange( iduVarMemListStatus *aBegin,
                          iduVarMemListStatus *aEnd )
{
    SChar       *sBuffer   = NULL;
    iduListNode *sIterator = NULL;
    iduListNode *sNodeNext = NULL;
    iduList     *sNodeList = NULL;
    idBool       sEnd      = ID_FALSE;

    if( aBegin->mCursor != NULL )
    {
        IDU_LIST_ITERATE( &mNodeList, sIterator )
        {
            sBuffer = (SChar *)sIterator->mObj;

            if( aBegin->mCursor == (sBuffer + ID_SIZEOF(ULong)) )
            {
                sNodeList = sIterator;
                break;
            }
        }

        IDE_ASSERT( sNodeList != NULL );

        IDU_LIST_ITERATE_SAFE2( &mNodeList, 
                                sNodeList, 
                                sIterator, 
                                sNodeNext )
        {
            sBuffer = (SChar *)sIterator->mObj;
        
            if( (aEnd->mCursor) == (sBuffer + ID_SIZEOF(ULong)) )
            {
                sEnd = ID_TRUE;
            }

            IDU_LIST_REMOVE( sIterator );

            mSize -= getSize( sIterator->mObj );
            mCount--;

            IDE_TEST( iduMemMgr::free( sIterator, &mPrivateAlloc ) != IDE_SUCCESS );
        
            if( sEnd == ID_TRUE )
            {
                break;
            }
        }
     
        IDE_ASSERT( sEnd == ID_TRUE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

ULong iduVarMemList::getAllocSize()
{
    return mSize;
}

UInt iduVarMemList::getAllocCount()
{
    return mCount;
}

iduList * iduVarMemList::getNodeList()
{
    return & mNodeList;
}

IDE_RC iduVarMemList::clone( iduVarMemList *aMem )
{
    iduListNode *sIterator = NULL;
    void        *sBuffer   = NULL;
    ULong        sSize;
    ULong        sTotalSize = 0;
    UInt         sCount = 0;

    IDE_TEST( aMem->freeAll() != IDE_SUCCESS );
    
    IDU_LIST_ITERATE( &mNodeList, sIterator )
    {
        sSize = getSize( sIterator->mObj );
        sTotalSize += sSize;
        sCount++;
        
        IDE_TEST( aMem->alloc( sSize, & sBuffer ) != IDE_SUCCESS );
        
        idlOS::memcpy( sBuffer,
                       (SChar*) sIterator->mObj + ID_SIZEOF(ULong),
                       sSize );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

SInt iduVarMemList::compare( iduVarMemList *aMem )
{
    iduList     *sNodeList;
    iduListNode *sIterator1 = NULL;
    iduListNode *sIterator2 = NULL;
    ULong        sSize1;
    ULong        sSize2;
    UInt         sCount1;
    UInt         sCount2;
    SInt         sCompare = 0;
    UInt         sCount = 0;
    SChar       *sBuf1;
    SChar       *sBuf2;

    // 1. count diff
    sCount1 = mCount;
    sCount2 = aMem->getAllocCount();

    if ( sCount1 != sCount2 )
    {
        ideLog::log( IDE_SERVER_0, "[Notify : SQL Plan Cache] Plan Memory Check : "
                     "Count Diff (%d,%d)",
                     sCount1, sCount2 );        
        sCompare = -1;
    }
    else
    {
        // Nothing to do.
    }

    // 2. size diff
    sSize1 = mSize;
    sSize2 = aMem->getAllocSize();
    
    if ( sSize1 != sSize2 )
    {
        ideLog::log( IDE_SERVER_0, "[Notify : SQL Plan Cache] Plan Memory Check : "
                     "Size Diff (%d,%d)",
                     (UInt)sSize1, (UInt)sSize2 );        
        sCompare = -1;
    }
    else
    {
        // Nothing to do.
    }

    // 3. node diff
    sNodeList = aMem->getNodeList();
    
    for ( sIterator1 = mNodeList.mNext, sIterator2 = sNodeList->mNext;
          (sIterator1 != &mNodeList) && (sIterator2 != sNodeList);
          sIterator1 = sIterator1->mNext, sIterator2 = sIterator2->mNext )
    {
        sSize1 = getSize( sIterator1->mObj );
        sSize2 = getSize( sIterator2->mObj );

        if ( sSize1 != sSize2 )
        {
            ideLog::log( IDE_SERVER_0, "[Notify : SQL Plan Cache] Plan Memory Check : "
                         "Node[%d].Size Diff (%d,%d)",
                         sCount, (UInt)sSize1, (UInt)sSize2 );
            sCompare = -1;
        }
        else
        {
            sBuf1 = (SChar*) sIterator1->mObj + ID_SIZEOF(ULong);
            sBuf2 = (SChar*) sIterator2->mObj + ID_SIZEOF(ULong);
            
            if ( idlOS::memcmp( sBuf1, sBuf2, sSize1 ) != 0 )
            {
                ideLog::log( IDE_SERVER_0, "[Notify : SQL Plan Cache] Plan Memory Check : "
                             "Node[%d].Buffer Modified, Size=%d",
                             sCount, (UInt)sSize1 );
                sCompare = -1;
            }
        }
        
        sCount++;
    }
    
    if ( (sIterator1 != &mNodeList) || (sIterator2 != sNodeList) )
    {
        ideLog::log( IDE_SERVER_0, "[Notify : SQL Plan Cache] Plan Memory Check : "
                     "Count Diff (%d)",
                     sCount );
        sCompare = -1;
    }

    return sCompare;
}
