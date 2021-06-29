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
 * $Id:$
 **********************************************************************/

#ifndef _O_SDB_BCB_MGR_H_
#define _O_SDB_BCB_MGR_H_ 1

#include <smDef.h>
#include <sdbDef.h>

#include <iduLatch.h>
#include <iduMutex.h>
#include <idv.h>
#include <idl.h>

class sdbBCB;
class sdsBCB;

/* --------------------------------------------------------------------
 * Ư�� BCB�� ��� ������ �����ϴ��� ���θ�
 * �˻��Ҷ� ���
 * ----------------------------------------------------------------- */
typedef idBool (*sdbFiltFunc)( void *aBCB, void *aFiltAgr );

typedef enum
{
    SDB_BCB_NO_LIST = 0,
    SDB_BCB_LRU_HOT,
    SDB_BCB_LRU_COLD,
    SDB_BCB_PREPARE_LIST,
    SDB_BCB_FLUSH_LIST,
    SDB_BCB_DELAYED_FLUSH_LIST      /* PROJ-2669 */
} sdbBCBListType;

#define SDB_BCB_LIST_NONE      ID_UINT_MAX
#define SDB_CP_LIST_NONE       ID_UINT_MAX

/* --------------------------------------------------------------------
 * BCB�� ���� ����
 * ----------------------------------------------------------------- */
typedef enum
{
    // ���� ������ �ʴ� ����. hash���� ���ŵǾ� �ִ�.
    // mPageID�� mSpaceID�� �ŷ��� �� ����.
    SDB_BCB_FREE = 0,

    // ���� hash���� ������ �� �ִ� ����. ������ ������ ������� �ʾұ�
    // ������, ��ũ IO���� �׳� replace�� �����ϴ�.
    SDB_BCB_CLEAN,

    // ���� hash���� ������ �� ������, ������ ����� ����.
    // replace �� ���ؼ� ��ũ�� flush�� �ʿ�
    SDB_BCB_DIRTY,

    // flusher�� flush�� ���� �ڽ��� ���� ����(IOB)�� �� BCB
    // ������ ������ ����. ����� replace�Ǿ�� �ȵȴ�.
    SDB_BCB_INIOB,

    // SDB_BCB_INIOB ���¿��� � Ʈ������� ������ ������ ����.
    SDB_BCB_REDIRTY
} sdbBCBState;

//BUG-48042: Page, BCB, BCB ����Ʈ ��� ��Ʈ
typedef struct
{
    UChar       *mTmpFrame;
    sdbBCB      *mTmpBCB;
    smuList     *mTmpNode;
    void        *mTmpFrameMemHandle;
} sdbBuffAreaPtrSet;

//BCB-48042: Parallel �Ҵ��� ���� Job
typedef struct
{
    UInt              mStartBCBID;
    UInt              mJobCnt;
    sdbBuffAreaPtrSet *mPtrSet;
} sdbBuffAreaJobInfo; 

class sdbBCB
{
public:
    // PROJ-2102 ���� �κ� 
    SD_BCB_PARAMETERS

    // BCB ���� ID
    UInt            mID;

    // BCB�� ����
    sdbBCBState     mState;
    sdbBCBState     mPrevState;

    // �ϳ��� Buffer Frame�� �ּҸ� ����Ų��. ���� frame�� �ּҴ� ������
    // size�� align�Ǿ��ִ�.
    UChar          *mFrame;

    // mFrame�� ���� MemoryHandle�μ� Free�ÿ� ����Ѵ�. (����:iduMemPool2)
    void           *mFrameMemHandle;

    // To fix BUG-13462
    // BCB frame�� ������ Ÿ��
    // �� ���ǵ� ���� ���̾��� ������ Ÿ���� �𸣱� ������ UInt�� ���
    UInt            mPageType;

    // sdbLRUList �Ǵ� sdbFlushList �Ǵ� sdbPrepareList�� �ϳ��� ���� �� �ִ�.
    // List�� ���� �ڷᱸ��
    smuList         mBCBListItem;
    sdbBCBListType  mBCBListType;
    
    // ����ȭ�� ����Ʈ�߿��� �ڽ��� ���� ����Ʈ�� �ĺ���
    UInt            mBCBListNo;

    // BCB�� ������ Ƚ��
    UInt            mTouchCnt;

    // mState�� �����ϱ� ���ؼ�, �Ǵ� mFixCount�� mTouchCnt�� �����ϱ� ���ؼ�
    // ��ƾ� �ϴ� mutex. ���� BCBMutex��� �ϸ� �̰��� ���Ѵ�.
    iduMutex        mMutex;
    
    // �������� ���� ��ġ. ������ ��ü�� ���� ���ü� ����
    iduLatch     mPageLatch;
    
    // ������ ��ġ�� ���� �ʴ� fixPage������ ���� mutex.
    iduMutex        mReadIOMutex;
    
    // �� BCB�� ���� �������� mTouchCnt�� ������ �ð�
    idvTime         mLastTouchedTime;

    /* PROJ-2669 ���� ������ BCB Touch �ð� */
    idvTime         mLastTouchCheckTime;
    
    // �� BCB�� ���ؼ� fix�ϰ� �ִ� ������ ����
    UInt            mFixCnt;

    // �������� �о �Ǵ��� ����
    idBool          mReadyToRead;
    
    /* �������� ��ũ���� �о�Դµ�, �̰��� ������ ��쿡 ���⿡
     * ID_TRUE �� �����Ѵ�. �̰��� �ʿ��� ������ no latch�� ��������
     * �����ϴ� �����尡 ���� �������� ������ �� ���������� �˾ƾ� �ϱ�
     * �����̴�.*/
    idBool          mPageReadError;

    // ��������� ���� �ڷ�
    idvTime         mCreateOrReadTime;   // ������ createPage �Ǵ� loadPage�� �ð�
    UInt            mWriteCount;
    // PROJ-2102 : ���� �Ǵ� Secondary Buffer BCB
    sdsBCB        * mSBCB;

public:
    static ULong mTouchUSecInterval;

public:
    /* ����!!
     * �����ִ� �޼ҵ���� ��Ѱ͵� ���ü��� ���������� �ʴ´�.
     * �ܺο��� ���ü��� ��Ʈ�� �ؾ� �Ѵ�.
     * */
    IDE_RC initialize(void *aFrameMemHandle, UChar *aFrame, UInt aBCBID);

    IDE_RC destroy();

    //dirty���¸� clean���·� �����ϰ�, ���õ� ������ �ʱ�ȭ �Ѵ�.
    void clearDirty();

    inline idBool isFree();
    inline void setToFree();
    inline void makeFreeExceptListItem();

    inline void clearTouchCnt();

    inline void updateTouchCnt();

    inline void lockPageXLatch(idvSQL *aStatistics);
    inline void lockPageSLatch(idvSQL *aStatistics);
    inline void tryLockPageXLatch( idBool *aSuccess);
    inline void tryLockPageSLatch( idBool *aSuccess );

    inline void unlockPageLatch(void);

    inline void lockBCBMutex(idvSQL *aStatistics);
    inline void unlockBCBMutex();

    inline void lockReadIOMutex(idvSQL *aStatistics);
    inline void unlockReadIOMutex(void);

    inline void incFixCnt();
    inline void decFixCnt();

    static  idBool isSamePageID(void *aLhs,
                                void *aRhs);

    void dump();
    static inline IDE_RC dump(void *aBCB);

    static inline void setBCBPtrOnFrame( sdbFrameHdr   *aFrame,
                                         sdbBCB        *aBCBPtr);

    static inline void setSpaceIDOnFrame( sdbFrameHdr   *aFrame,
                                          scSpaceID      aSpaceID);

    static inline void setPageLSN( sdbFrameHdr    *aFrame,
                                   smLSN           aLSN );
    
    inline void lockBufferAreaX(idvSQL   *aStatistics);
    inline void lockBufferAreaS(idvSQL   *aStatistics);

};

idBool sdbBCB::isFree()
{
    if( (mState == SDB_BCB_FREE ) &&
        (mFixCnt == 0))
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

void sdbBCB::makeFreeExceptListItem()
{
    IDE_ASSERT( mFixCnt == 0 );

    mState    = SDB_BCB_FREE;

    SM_LSN_MAX( mRecoveryLSN );
    mTouchCnt = 0;

    IDV_TIME_INIT( &mLastTouchedTime );
    IDV_TIME_INIT( &mLastTouchCheckTime );
    IDV_TIME_INIT( &mCreateOrReadTime );

    mWriteCount    = 0;
    mReadyToRead   = ID_FALSE;
    mPageReadError = ID_FALSE;
}

// flag�� �����ϱ��, ��������   clean���� �ɶ� �ʱ�ȭ
void sdbBCB::setToFree()
{
    makeFreeExceptListItem();

    SDB_INIT_BCB_LIST( this );

    if (( mCPListItem.mNext != NULL ) ||
        ( mCPListItem.mPrev != NULL ) ||
        ( mCPListNo != SDB_CP_LIST_NONE ))
    {
        /* BUG-47945 cp list ����� ���� ���� */
        dump();
        IDE_ASSERT(0);
    }

    SDB_INIT_CP_LIST( this );
}

void sdbBCB::lockPageXLatch(idvSQL  * aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs, 
                    IDV_WAIT_INDEX_LATCH_BUFFER_BUSY_WAITS,
                    mSpaceID, mPageID, mPageType );

    IDE_ASSERT( mPageLatch.lockWrite( aStatistics,
                                      &sWeArgs )
                == IDE_SUCCESS );
}

void sdbBCB::lockPageSLatch(idvSQL * aStatistics)
{
    idvWeArgs   sWeArgs;

    IDV_WEARGS_SET( &sWeArgs, 
                    IDV_WAIT_INDEX_LATCH_BUFFER_BUSY_WAITS,
                    mSpaceID, mPageID, mPageType );

    IDE_ASSERT( mPageLatch.lockRead(
                    aStatistics,
                    &sWeArgs ) == IDE_SUCCESS );
}

void sdbBCB::tryLockPageSLatch( idBool *aSuccess )
{
    IDE_ASSERT( mPageLatch.tryLockRead( aSuccess ) == IDE_SUCCESS );
}

void sdbBCB::tryLockPageXLatch( idBool *aSuccess )
{
    IDE_ASSERT( mPageLatch.tryLockWrite( aSuccess ) == IDE_SUCCESS );
}

void sdbBCB::unlockPageLatch()
{
    IDE_ASSERT( mPageLatch.unlock() == IDE_SUCCESS );
}

void sdbBCB::lockBCBMutex( idvSQL *aStatistics)
{
    IDE_ASSERT( mMutex.lock( aStatistics) == IDE_SUCCESS );
}

void sdbBCB::unlockBCBMutex(void)
{
    IDE_ASSERT( mMutex.unlock() == IDE_SUCCESS );
}

void sdbBCB::incFixCnt()
{
    mFixCnt++;
}

void sdbBCB::decFixCnt()
{
    IDE_DASSERT( mFixCnt != 0);
    mFixCnt--;
}

void sdbBCB::updateTouchCnt()
{
    idvTime sCurrentTime;
    ULong   sTime;

    // ������ touch �ȴٰ� �ؼ� �� touchCount�� �︮�� �ʰ�
    // ������ touch�ϰ�, sdbBCB::mTouchUSecInterval������ ������
    // touch count�� �ø���.
    IDV_TIME_GET( &sCurrentTime );

    if ( sdbBCB::mTouchUSecInterval > 0 )
    {
        sTime = IDV_TIME_DIFF_MICRO( &mLastTouchedTime, &sCurrentTime );

        /* PROJ-2669 ���� ������ BCB Touch �ð� */
        mLastTouchCheckTime = sCurrentTime;

        if ( sTime > mTouchUSecInterval )
        {
            mLastTouchedTime = sCurrentTime;
            mTouchCnt++;
        }
    }
    else
    {
        /* PROJ-2669 ���� ������ BCB Touch �ð� */
        mLastTouchCheckTime = sCurrentTime;

        mTouchCnt++;
    }
}

/* BUG-47945 cp list ����� ���� ���� */
IDE_RC sdbBCB::dump(void *aBCB)
{
    IDE_ERROR( aBCB != NULL );

    ((sdbBCB*)aBCB)->dump();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void sdbBCB::clearTouchCnt()
{
    mTouchCnt = 1;
}


/***********************************************************************
 * Description :
 *  aFrame      - [IN]  Page pointer
 *  aBCB        - [IN]  �ش� BCB
 ***********************************************************************/
void sdbBCB::setBCBPtrOnFrame( sdbFrameHdr   *aFrame,
                               sdbBCB        *aBCBPtr)
{
    aFrame->mBCBPtr = aBCBPtr;
}

/***********************************************************************
 * Description :
 *  aFrame      - [IN] �ش� page pointer
 *  aSpaceID    - [IN]  �ش� table space ID
 ***********************************************************************/
void sdbBCB::setSpaceIDOnFrame( sdbFrameHdr   *aFrame,
                                scSpaceID      aSpaceID)
{
    aFrame->mSpaceID = aSpaceID;
}

/***********************************************************************
 * Description :
 *  aFrame      - [IN] �ش� page pointer
 *  aLSN        - [IN] PAGE LSN
 ***********************************************************************/
void sdbBCB::setPageLSN( sdbFrameHdr    *aFrame,
                         smLSN           aLSN )
{
    aFrame->mPageLSN = aLSN;
}

#endif //_O_SDB_BCB_MGR_H_
