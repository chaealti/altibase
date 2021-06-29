/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemory.cpp 86179 2019-09-16 10:34:38Z jykim $
 **********************************************************************/

/***********************************************************************
 *
 * NAME
 *   iduMemory.h
 *
 * DESCRIPTION
 *   Dynamic memory allocator.
 *
 * PUBLIC FUNCTION(S)
 *   iduMemory( ULong BufferSize )
 *      BufferSize�� �޸� �Ҵ��� ���� �߰� ������ ũ�� �Ҵ�޴�
 *      �޸��� ũ��� BufferSize�� �ʰ��� �� �����ϴ�.
 *
 *   void* alloc( size_t Size )
 *      Size��ŭ�� �޸𸮸� �Ҵ��� �ݴϴ�.
 *
 *   void clear( )
 *      �Ҵ���� ��� �޸𸮸� ���� �մϴ�.
 *
 * NOTES
 *
 * MODIFIED    (MM/DD/YYYY)
 *    assam     01/12/2000 - Created
 *
 **********************************************************************/

#include <idl.h>
#include <idu.h>
#include <idp.h>
#include <idErrorCode.h>
#include <ideErrorMgr.h>
#include <iduProperty.h>
#include <iduMemory.h>
#include <idtContainer.h>

idBool iduMemory::mUsePrivate = ID_FALSE;

IDE_RC iduMemory::initializeStatic( void )
{
#define IDE_FN "iduMemory::initializeStatic"
    iduMemory::mUsePrivate = iduMemMgr::isPrivateAvailable();
    return IDE_SUCCESS;
    /*
     * Adding this code to use in future

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
     */
#undef IDE_FN
}

IDE_RC iduMemory::destroyStatic( void )
{
#define IDE_FN "iduMemory::destroyStatic"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mUsePrivate = ID_FALSE;
    return IDE_SUCCESS;
#undef IDE_FN
}

IDE_RC iduMemory::init( iduMemoryClientIndex aIndex, ULong aBufferSize)
{
#define IDE_FN "iduMemory::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mCurHead    = mHead = NULL;
    mDefaultChunkSize  = aBufferSize;
    mIndex      = aIndex;
    mChunkCount = 0;
    mTotalChunkSize = 0;
    
#if defined(ALTIBASE_MEMORY_CHECK)
    mUsePrivate = ID_FALSE;
    mSize       = 0;
#endif
    if (mUsePrivate == ID_TRUE)
    {
        IDE_TEST(iduMemMgr::createAllocator(&mPrivateAlloc) != IDE_SUCCESS);
    }
    else
    {
        (void)mPrivateAlloc.initialize(NULL);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    mUsePrivate = ID_FALSE;
    return IDE_FAILURE;
#undef IDE_FN
}

void iduMemory::destroy( void )
{
#define IDE_FN "iduMemory::destroy"
    freeAll(0);
    if (mUsePrivate == ID_TRUE)
    {
        (void)iduMemMgr::freeAllocator(&mPrivateAlloc);
    }
    else
    {
        /* Do nothing */
    }
#undef IDE_FN
}

IDE_RC iduMemory::malloc(size_t aSize, void** aPointer)
{
#define IDE_FN "iduMemory::malloc"
    IDE_TEST( iduMemMgr::malloc( mIndex,
                                 aSize,
                                 aPointer,
                                 IDU_MEM_IMMEDIATE,
                                 &mPrivateAlloc) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#undef IDE_FN
}

IDE_RC iduMemory::free(void* aPointer)
{
#define IDE_FN "iduMemory::free"
    IDE_TEST( iduMemMgr::free( aPointer, &mPrivateAlloc ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
#undef IDE_FN
}

IDE_RC iduMemory::header()
{
#define IDE_FN "iduMemory::header"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

#if defined(ALTIBASE_MEMORY_CHECK)

    iduMemoryHeader *sHeader;

    IDE_TEST(malloc(sizeof(iduMemoryHeader),
                    (void**)&sHeader)
             != IDE_SUCCESS);

    IDE_ASSERT( sHeader != NULL);

    sHeader->mNext      = NULL;
    sHeader->mCursor    = 0;
    sHeader->mChunkSize = 0;
    sHeader->mBuffer    = NULL;
    mChunkCount++;

    if (mHead == NULL)
    {
        mHead = mCurHead = sHeader;
    }
    else
    {
        IDE_ASSERT(mCurHead != NULL);
        mCurHead->mNext = sHeader;
        mCurHead        = sHeader;
    }
#else
    if( mHead == NULL )
    {
        if( mDefaultChunkSize < ID_SIZEOF(iduMemoryHeader) )
        {
            // BUG-32293
            mDefaultChunkSize = idlOS::align8((UInt)IDL_MAX(sizeof(iduMemoryHeader),
                                                     iduProperty::getQpMemChunkSize()));
        }

        IDE_TEST(malloc(mDefaultChunkSize, (void**)&mHead) != IDE_SUCCESS);
        mTotalChunkSize += mDefaultChunkSize;

        IDE_ASSERT(mHead != NULL);
        mCurHead = mHead;

        IDE_ASSERT(mCurHead != NULL);
        if( mCurHead != NULL )
        {
            mCurHead->mNext   = NULL;
            mCurHead->mCursor = idlOS::align8((UInt)sizeof(iduMemoryHeader));
            mCurHead->mChunkSize = mDefaultChunkSize;
            mCurHead->mBuffer = ((char*)mCurHead);
            mChunkCount++;
        }
    }
    IDE_ASSERT(mCurHead != NULL);

#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC iduMemory::release( iduMemoryHeader* aHeader )
{

#define IDE_FN "iduMemory::release"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryHeader* sHeader;
    iduMemoryHeader* next;
#if defined(ALTIBASE_MEMORY_CHECK)
    for( sHeader = aHeader; sHeader != NULL; sHeader = next )
    {
        next = sHeader->mNext;

        if(sHeader->mBuffer != NULL)
        {
            IDE_TEST(free(sHeader->mBuffer) != IDE_SUCCESS);
        }
        else
        {
            /* fall through */
        }

        mSize -= sHeader->mChunkSize;

        IDE_TEST(free(sHeader) != IDE_SUCCESS);
        mChunkCount--;
    }
#else
    for( sHeader = aHeader; sHeader != NULL; sHeader = next )
    {
        next = sHeader->mNext;
	mTotalChunkSize -= sHeader->mChunkSize;
        IDE_TEST(free(sHeader) != IDE_SUCCESS);

        mChunkCount--;
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduMemory::cralloc( size_t aSize, void** aMemPtr )
{
#define IDE_FN "iduMemory::cralloc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void *sResult;

    IDE_TEST(alloc(aSize, &sResult) != IDE_SUCCESS);

    idlOS::memset(sResult, 0, aSize);

    *aMemPtr = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduMemory::extend( ULong aRequestedSize)
{

#define IDE_FN "iduMemory::extend"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    /* current [mCurHead] is not sufficient size */

    iduMemoryHeader *sBefore;
    ULong             sSize;

    IDE_ASSERT(mCurHead != NULL);

    sBefore = mCurHead;

    for (mCurHead  = mCurHead->mNext; /* skip tested header  */
         mCurHead != NULL;
         mCurHead  = mCurHead->mNext)
    {
        IDE_ASSERT(mCurHead->mCursor ==
                   idlOS::align8((UInt)sizeof(iduMemoryHeader)));

        /* find proper memory which has enough size,
           it is pointed by mCurHead */
        if ( aRequestedSize <= (mCurHead->mChunkSize - mCurHead->mCursor))
        {
            return IDE_SUCCESS;
        }

        sBefore = mCurHead;
    }
    /* can't find proper memory, make another header */
    IDE_ASSERT(sBefore != NULL && mCurHead == NULL);

    sSize = aRequestedSize + idlOS::align8((UInt)sizeof(iduMemoryHeader));

    if (sSize < mDefaultChunkSize)
    {
        sSize = mDefaultChunkSize;
    }

    IDE_TEST( checkMemoryMaximumLimit(sSize) != IDE_SUCCESS );

    IDE_TEST(malloc(sSize,
                    (void**)&mCurHead)
             != IDE_SUCCESS);
    mTotalChunkSize += sSize;

    mCurHead->mNext      = NULL;
    mCurHead->mCursor    = idlOS::align8((UInt)sizeof(iduMemoryHeader));
    mCurHead->mChunkSize = sSize;
    mCurHead->mBuffer    = ((char*)mCurHead);
    mChunkCount++;

    sBefore->mNext       = mCurHead;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    mCurHead = sBefore;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC iduMemory::alloc( size_t aSize, void** aMemPtr )
{

#define IDE_FN "iduMemory::alloc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    void* sResult;

    IDE_TEST_RAISE(aSize == 0, err_invalid_argument);

    IDE_TEST(header() != IDE_SUCCESS);

#if defined(ALTIBASE_MEMORY_CHECK)
    IDE_TEST(checkMemoryMaximumLimit(aSize) != IDE_SUCCESS);
    IDE_TEST(malloc(aSize,
                    (void**)&(mCurHead->mBuffer))
             != IDE_SUCCESS);

    sResult = mCurHead->mBuffer;
    IDE_ASSERT(sResult != NULL);
    mCurHead->mChunkSize = aSize;
    mCurHead->mCursor    = aSize;
    mSize               += aSize;
#else
    aSize = idlOS::align8((ULong)aSize);

    IDE_ASSERT(mCurHead != NULL);

    if( aSize > ( mCurHead->mChunkSize - mCurHead->mCursor ) )
    {
        IDE_TEST(extend(aSize) != IDE_SUCCESS);
    }

    sResult            = mCurHead->mBuffer + mCurHead->mCursor;
    mCurHead->mCursor += aSize;
#endif

    *aMemPtr = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_argument);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idaInvalidParameter));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

SInt  iduMemory::getStatus( iduMemoryStatus* aStatus )
{
#define IDE_FN "iduMemory::getStatus"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if( mHead == NULL && mCurHead == NULL )
    {
        aStatus->mSavedCurr = NULL;
        aStatus->mSavedCursor  = 0;
    }
    else
    {
        IDE_TEST_RAISE( mHead == NULL || mCurHead == NULL,
                        ERR_INVALID_STATUS );
        aStatus->mSavedCurr    = mCurHead;
        aStatus->mSavedCursor  = mCurHead->mCursor;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_STATUS );
    IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_STATUS));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

SInt  iduMemory::setStatus( iduMemoryStatus* aStatus )
{
#define IDE_FN "iduMemory::setStatus"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryHeader* sHeader;

    if( mHead == NULL && mCurHead == NULL )
    {
        IDE_TEST_RAISE( aStatus->mSavedCurr != NULL ||
                        aStatus->mSavedCursor != 0,
                        ERR_NOT_SAME_BUFFER_POSITION );
    }
    else
    {
        IDE_TEST_RAISE( mHead == NULL || mCurHead == NULL,
                        ERR_NOT_SAME_HEADER );

        if( aStatus->mSavedCurr == NULL && aStatus->mSavedCursor == 0 )
        {
            aStatus->mSavedCurr   = mHead;
#if defined(ALTIBASE_MEMORY_CHECK)
            aStatus->mSavedCursor = 0;
#else
            aStatus->mSavedCursor = idlOS::align8((UInt)sizeof(iduMemoryHeader));
#endif
        }

#if defined(DEBUG)
        // aStatus->mSavedCurr�� ����Ʈ �� �����ϴ��� Ȯ���Ѵ�.
        // BUG-22287, ��쿡 ���� �ð��� ���� �ɸ��� �۾����� ��ü
        // loop ���� �и� �Ͽ� DEBUG �󿡼��� Ȯ�� �ϵ��� �Ѵ�.

        for( sHeader = mHead ; sHeader != NULL ; sHeader = sHeader->mNext )
        {
            if( sHeader == aStatus->mSavedCurr )
            {
                break;
            }
        }
        IDE_DASSERT( ( sHeader != NULL ) && ( sHeader == aStatus->mSavedCurr ) );
#else
        sHeader = aStatus->mSavedCurr;
     
        IDE_ASSERT( sHeader != NULL );
#endif

#if !defined(ALTIBASE_MEMORY_CHECK)
        IDE_TEST_RAISE( aStatus->mSavedCursor !=
                        idlOS::align8((UInt)aStatus->mSavedCursor),
                        ERR_INCORRECT_BUFFER_ALIGN );

        IDE_TEST_RAISE( aStatus->mSavedCursor <
                        idlOS::align8((UInt)sizeof(iduMemoryHeader)),
                        ERR_LESS_THAN_BUFFER );
#endif
        IDE_TEST_RAISE( aStatus->mSavedCursor > sHeader->mCursor,
                        ERR_NOT_SMAE_OBJECT_BUFFER_POSITIION );

#if defined(ALTIBASE_MEMORY_CHECK)
        /* BUG-18098 [valgrind] 12,672 bytes in 72 blocks are indirectly
         * lost - by smnfInit()
         *
         * ALTIBASE_MEMORY_CHECK�� ��� iduMemory�� alloc�� ���� �޸𸮸�
         * �Ҵ��Ѵ�. �Ҵ�� �޸𸮳��� ��ũ�� ����Ʈ�� �����ϰ� �߰���
         * �޸𸮴� ��ũ�� ����Ʈ�� ���� �߰��մϴ�. �׸��� ���� setstatus��
         * ȣ��Ǹ� iduMemoryStatus�� ����Ű�� ��ũ���� mCurHead�� ����
         * �ٲߴϴ�. �׷��� �׻� mCurHead�� ������ ��ġ�� ����Ű�� ������
         * setstatus�� ȣ����� �ٽ� alloc�� ȣ��� ��� mCurHead�� ����Ű��
         * �������� ������ ��ư� �ɼ� �ֽ��ϴ�.
         *
         * a -> b -> c -> d�� ��� mCurHead = d�� �˴ϴ�. ���⼭ setstatus
         * �� ȣ��Ǿ� ���� mCurHead�� b�� �Ǿ��ٰ� �ϰ� �ٽ� alloc�� �Ǿ�
         * e�� �߰��Ǹ�
         * a -> b -> e�� �˴ϴ�. ��������� c, d�� ã�� ���� ���� õ����
         * ��ư� �˴ϴ�. ���� c, d�� free�����ְ� b�� next�� null��
         * �ٲپ�� �մϴ�. */
        if( sHeader->mNext != NULL )
        {
            IDE_ASSERT( release( sHeader->mNext ) == IDE_SUCCESS );
            sHeader->mNext = NULL;
        }
#else
        for( sHeader =  aStatus->mSavedCurr->mNext ;
             sHeader != NULL ;
             sHeader =  sHeader->mNext )
        {
            /* initialize after the restored position */
            sHeader->mCursor = idlOS::align8((UInt)sizeof(iduMemoryHeader));
        }
#endif
        mCurHead          = aStatus->mSavedCurr;
        mCurHead->mCursor = aStatus->mSavedCursor;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SAME_BUFFER_POSITION )
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_SET_STATUS,
                                "not same buffer position"));
    } 
    IDE_EXCEPTION( ERR_NOT_SAME_HEADER )
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_SET_STATUS,
                                "not same Header"));
    } 
    IDE_EXCEPTION( ERR_INCORRECT_BUFFER_ALIGN )
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_SET_STATUS,
                                "incorrect buffer align"));
    } 
    IDE_EXCEPTION( ERR_LESS_THAN_BUFFER )
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_SET_STATUS,
                                "iduMemoryHeader less than buffer"));
    } 
    IDE_EXCEPTION( ERR_NOT_SMAE_OBJECT_BUFFER_POSITIION )
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_IDU_MEMORY_INVALID_SET_STATUS,
                                "not same object buffer position"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;


#undef IDE_FN
}

void iduMemory::clear( void )
{
#define IDE_FN "iduMemory::clear"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

#if defined(ALTIBASE_MEMORY_CHECK)
    // do nothing
#else
    iduMemoryHeader *sHeader;

    if( mHead != NULL )
    {
        mCurHead = mHead;
    }

    for( sHeader = mHead; sHeader != NULL; sHeader=sHeader->mNext )
    {
        /* initialize after the restored position */
        sHeader->mCursor = idlOS::align8((UInt)sizeof(iduMemoryHeader));
    }
#endif
#undef IDE_FN
}

void iduMemory::freeAll( UInt aMinChunkNo )
{
#define IDE_FN "iduMemory::freeAll"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    iduMemoryHeader * cursor;
    UInt              cursor_no;

    if( mHead != NULL )
    {
        if( aMinChunkNo > 0 )
        {
            clear();
            for( cursor = mHead, cursor_no = 1;
                 (cursor != NULL) && (cursor_no < aMinChunkNo);
                 cursor = cursor->mNext, cursor_no++ )
            {
            }

            if( cursor != NULL && cursor_no == aMinChunkNo )
            {
                release( cursor->mNext );
                cursor->mNext = NULL;
                mCurHead = cursor;
            }
        }
        else
        {
            release( mHead );
            mCurHead = mHead = NULL;
        }
    }

#undef IDE_FN
}

void iduMemory::freeUnused( void )
{
#define IDE_FN "iduMemory::freeUnused"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if( mCurHead != NULL )
    {
        release( mCurHead->mNext );
        mCurHead->mNext = NULL;
    }

#undef IDE_FN
}

ULong iduMemory::getSize( void )
{
#define IDE_FN "iduMemory::getSize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("getSize"));

    return mTotalChunkSize;

#undef IDE_FN
}


void iduMemory::dump()
{
#define IDE_FN "iduMemory::dump"
    iduMemoryHeader* sHeader;
    UInt pos = 0;

    idlOS::printf( "!!!!! DUMP !!!!!\n" );
    for( sHeader = mHead; sHeader != NULL; sHeader=sHeader->mNext )
    {
        idlOS::printf( "[%d] %d\n", pos, sHeader->mChunkSize );
        pos++;
    }
    idlOS::printf( "!!!!! END  !!!!!\n\n" );
#undef IDE_FN
}

IDE_RC iduMemory::checkMemoryMaximumLimit(ULong aSize)
{
/***********************************************************************
 *
 * Description : prepare, execute memory üũ (BUG-
 *
 * Implementation :
 *    extend�ÿ� �Ҹ���, ���� chunkcount +1�� chunksize�� ���� ����
 *    property�� ������ ũ�� ����.
 *    query prepare, query execute �޸𸮿� ����.
 *
 ***********************************************************************/
#define IDE_FN "iduMemory::checkMemoryMaximum"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

#if defined(ALTIBASE_MEMORY_CHECK)
    switch( mIndex )
    {
        case IDU_MEM_QMP:
            IDE_TEST_RAISE(
                (mSize + aSize) > iduProperty::getPrepareMemoryMax(),
                err_prep_mem_alloc );
            break;
        case IDU_MEM_QMX:
            IDE_TEST_RAISE(
                (mSize + aSize) > iduProperty::getExecuteMemoryMax(),
                err_exec_mem_alloc );
            break;

        default:
            break;
    }
#else
    switch( mIndex )
    {
        case IDU_MEM_QMP:
            IDE_TEST_RAISE(
                (getSize() + aSize) >
                iduProperty::getPrepareMemoryMax(),
                err_prep_mem_alloc );
            break;
        case IDU_MEM_QMX:
            IDE_TEST_RAISE(
                (getSize() + aSize) >                  /*BUG-46948*/
                iduProperty::getExecuteMemoryMax(),
                err_exec_mem_alloc );
            break;

        default:
            break;
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_prep_mem_alloc );
    {
        IDE_SET( ideSetErrorCode(idERR_ABORT_MAX_MEM_SIZE_EXCEED,
                                 iduMemMgr::mClientInfo[mIndex].mName,
                                 aSize,
                                 iduProperty::getPrepareMemoryMax() ) );
    }
    IDE_EXCEPTION( err_exec_mem_alloc );
    {
        IDE_SET( ideSetErrorCode(idERR_ABORT_MAX_MEM_SIZE_EXCEED,
                                 iduMemMgr::mClientInfo[mIndex].mName,
                                 aSize,
                                 iduProperty::getExecuteMemoryMax() ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}
