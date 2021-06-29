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
 * $Id: sdbDef.h 86110 2019-09-02 04:52:04Z et16 $
 *
 * Description :
 *
 * - Dirty Page Buffer �ڷᱸ��
 *
 **********************************************************************/

#ifndef _O_SDB_DEF_H_
#define _O_SDB_DEF_H_ 1

#include <smDef.h>
#include <iduLatch.h>
#include <smuList.h>
#include <smuUtility.h>
#include <iduMutex.h>

/* BCB�� �ʱ� Page Type Value */
#define SDB_NULL_PAGE_TYPE    (0)

#define SDB_SMALL_BUFFER_SIZE (1024)

/* --------------------------------------------------------------------
 * PROJ-1665 : Corrupted Page Processing Policy ����
 *     - FATAL : Page ���°� corrupted �̸� fatal error �߻�
 *     - ABORT : Page ���°� corrupted �̸� abort error �߻�
 *
 * Redo �ÿ� corrupted page���� ������ �̸� corrupted page list�� �����Ѵ�.
 * Undo�� ������ corrupted page list�� �� page ���� extent�� ���¸�
 * �˻��Ͽ� �ش� extent�� free �����̸� corrupted page list���� �����Ѵ�.
 * page�� ���� extent�� free �̸� page�鵵 ��� free �̹Ƿ�
 * corrupted page ���� �ϴ��� ������� �����̴�.
 *
 * PROJ_1665 ������ Redo �ÿ� corrupted page�� ������ ������ fatal��
 * ó���Ǿ���. �׷��� Direct-Path INSERT�� commit ���� ��� page����
 * flush �ϴµ� ���� Partial Write�Ǿ��ٸ� �̴� commit ���� ������ ���̴�.
 * commit ���� ���� transaction�� Undo �� ���̰� �̶� Direct-Path INSERT��
 * ���� �Ҵ���� extent���� ��� tablespace�� ��ȯ�Ǿ������.
 * tablespace ��ȯ�� extent���� ��� free �����̰� �̿� ���� page�鵵
 * ��� free ���°� �ǹǷ� corrupted page��� �ؼ� ������ fatal ��
 * ó���ؼ� �ȵȴ�.
 * ----------------------------------------------------------------- */

typedef enum
{
    SDB_CORRUPTED_PAGE_READ_FATAL = 0,
    SDB_CORRUPTED_PAGE_READ_ABORT,
    SDB_CORRUPTED_PAGE_READ_PASS
} sdbCorruptPageReadPolicy;

// PROJ-1665
typedef struct sdbReadPageInfo
{
    idBool                   aNoAbort;
    sdbCorruptPageReadPolicy aCorruptPagePolicy;
}sdbReadPageInfo;

/* --------------------------------------------------------------------
 * Buffering policy ���� (BUG-17727)
 * ----------------------------------------------------------------- */

typedef enum
{
    // least recently used
    SDB_BUFFERING_LRU = 0,
    // most recently used
    SDB_BUFFERING_MRU
} sdbBufferingPolicy;


/* --------------------------------------------------------------------
 * Buffer Frame�� I/O �۾��� ���� ����
 * ----------------------------------------------------------------- */

typedef enum
{
    // ������ �ƴ�
    SDB_IO_NOTHING = 0,

    // ��ũ�κ��� �������� �д� ��
    // latch func vector���� ���ǹǷ� 1�� �Ǿ�� �Ѵ�.
    SDB_IO_READ = 1,

    // ��ũ�� �������� ���� ��
    SDB_IO_WRITE

} sdbIOState;

#if 0 //not used.
/* --------------------------------------------------------------------
 * ���� Ʈ������� ��û Event
 * ----------------------------------------------------------------- */

typedef enum
{
    SDB_EVENT_LATCH_FREE = 0,
    SDB_EVENT_LATCH_WAIT_S,
    SDB_EVENT_LATCH_WAIT_X,
    SDB_EVENT_LATCH_WAIT_RIO,
    SDB_EVENT_LATCH_WAIT_WIO,
    SDB_EVENT_LATCH_WAITALL_RIO
} sdbWaitEventType;
#endif

#if 0 //not used.
/* --------------------------------------------------------------------
 * Flush ����� Type
 * ----------------------------------------------------------------- */

typedef enum
{

    // LRU ����Ʈ flush
    SDB_FLUSH_LRU = 0,

    // Flush ����Ʈ flush
    SDB_FLUSH_FLUSH,

    SDB_FLUSH_NOTHING

} sdbFlushType;
#endif

#if 0 //not used.
/* ------------------------------------------------------------------------
 * ûũ�� Type : ( UNDO : For Undo Tablespace , NORMAL : Other Tablespaces )
 * ---------------------------------------------------------------------- */

typedef enum
{
    SDB_CHUNK_NORMAL = 0,

    SDB_CHUNK_UNDO

} sdbChunkType;
#endif

#if 0 //not used.
// flush ����� type ����
// flush ��� type�� �߰��� ��, �̰͵� �����ȴ�.
#define SDB_FLUSH_TYPE_COUNT    (SDB_FLUSH_FLUSH+1)
#endif
/* --------------------------------------------------------------------
 * BCB�� ������ ���¸� ��Ÿ����.
 * �������� �����Ǿ�����, BCB�� ������ ������ LSN�� ���Եȴ�.
 * ----------------------------------------------------------------- */
#if 0 //not used.
// BCB�� ������ ���� ����.
#define SDB_NOT_MODIFIED   (0)
#endif
/* --------------------------------------------------------------------
 * �������� ������� ��� ���
 * ----------------------------------------------------------------- */

typedef enum
{
    // ��� ���� �������� ��´�. �������� ���� ���ϸ� ������ return
    SDB_WAIT_NO = 0,

    // ������ latch�� ��� ���� ��, ���� �����ϸ� ���,
    // �׷��� ������ ����Ѵ�.
    SDB_WAIT_NORMAL

} sdbWaitMode;


/* --------------------------------------------------------------------
 * latch ���
 * ----------------------------------------------------------------- */
typedef enum
{

    SDB_X_LATCH = 0,
    SDB_S_LATCH,
    SDB_NO_LATCH

} sdbLatchMode;

#if 0 //not used. 
/* --------------------------------------------------------------------
 * release ���
 * mtx���� savepoint���� release�ϴ� ��쿡��
 * ��� �������� �������� �ʾ����� �����Ѵ�.
 * �̶��� flush list�� ������� �ʴ´�.
 * ----------------------------------------------------------------- */
typedef enum
{
    // ��ġ release�� �Ѵ�.
    SDB_JUST_RELEASE = 0,
    // �����Ǿ��� ��� �̸� release�ÿ� �ݿ��Ѵ�.
    SDB_REFLECT_MODIFICATION
} sdbReleaseMode;
#endif
typedef enum
{
    SDB_SINGLE_PAGE_READ = 0,
    SDB_MULTI_PAGE_READ
} sdbPageReadMode;

typedef struct sdbFrameHdr
{
    // üũ��
    UInt     mCheckSum;

    // ������ ������ LSN
    // flush�� write�ȴ�.
    smLSN    mPageLSN;

    // ���̺����̽� ���̵�
    UShort   mSpaceID;

    // ���̴���
    SChar    mAlign[2];

    // �ε����� smo ���࿩�θ� Ȯ���� �� �ִ�
    // SMO ��ȣ . ó���� 0�̴�.
    ULong    mIndexSMONo;

    //BCB ������. ���� Frame�� �����ִ� BCB Pointer
    void*    mBCBPtr;

#ifndef COMPILE_64BIT
    SChar    mAlign[4];
#endif
} sdbFrameHdr;


#if 0 //not used.
/* ------------------------------------------------
 * double write header ������ �����ϴ� ������.
 * space 1���� 0�� �������� space ������ �Բ�
 * ����ȴ�.
 * ----------------------------------------------*/
#define SDB_DOUBLEWRITE_HDR_SPACE    SMI_ID_TABLESPACE_SYSTEM_DATA

#define SDB_DOUBLEWRITE_HDR_PAGE     SD_CREATE_PID(0,0)
#endif

// To implement PRJ-1539 : Buffer Manager Renewal
#if 0 //not used.
// �ϳ��� ûũ�� ������ �ִ� �ּ� ������ ����
// �ش� ������ �׻� Ŀ�� �Ѵ�.
#define MIN_FRAME_COUNT_PER_CHUNK    (128)
#endif

#if 0 //not used.
// �����ɼ� �ִ� ��� ������ �ִ� ũ�⸦ �����Ѵ�.
// ��ü Undo ���� �������� CREATE_UNDO_BUFFER_LIMIT �̻���
// CreatePage�� �����ϱ� ���ؼ� ���ȴ�.
#define CREATE_UNDO_BUFFER_LIMIT     (0.95)

// Free�� Undo ������ �ּ� ũ�⸦ �����Ѵ�.
// ��ü Undo ���� �������� FREE_UNDO_BUFFER_LIMIT ��ŭ��
// Free ������ �����ϱ� ���ؼ� ���ȴ�.
#define FREE_UNDO_BUFFER_LIMIT       (0.05)
#endif
typedef enum sdbDPathBCBState
{
    /* �ʱ⿡ �������� �����Ǿ���, ��ũ�� �������� �ȵȴ�.*/
    SDB_DPB_APPEND = 0,
    /* �� �������� ���� ��� Modify������ ������. ��ũ��
     * ������ȴ�.*/
    SDB_DPB_DIRTY,
    /* ���� �� �������� Disk�� ���� ������� ���õǾ���. */
    SDB_DPB_FLUSH
} sdbDPathBCBState;

struct sdbDPathBCB;
struct sdbDPathBuffInfo;
class  sdbDPathBFThread;

typedef struct sdbDPathBCB
{
    /* BCB�鳢���� ��ũ�� ����Ʈ�� ����
     * ���� */
    smuList           mNode;

    /* mPage�� SpaceID, PageID */
    scSpaceID         mSpaceID;
    scPageID          mPageID;
 
    /* BCB�� ���¸� ǥ�� */
    sdbDPathBCBState  mState;
    UChar*            mPage;
    sdbDPathBuffInfo* mDPathBuffInfo;

    /* �� BCB�� ���ؼ� �α����� ����. ���� ID_TRUE��
     * mPage�� ��ũ�� ���������� ���� Page Image�α׸�
     * ���� ����Ѵ�.
     * Logging ���δ� table �Ӽ��̹Ƿ� DPath Buffer �Ӽ��� �ƴϹǷ�
     * Buffer ������ �����ϴ� sdbDPathBuffInfo���� ���� �ʴ°��� �´�. */
    idBool            mIsLogging;
} sdbDPathBCB;

typedef struct sdbDPathBuffInfo
{
    /* Direct Path Insert�� ������� Page�� ���� */
    ULong             mTotalPgCnt;

    /* Flush Thread */
    sdbDPathBFThread *mFlushThread;

    /* Append Page List */
    smuList           mAPgList;

    /* Flush Request List */
    smuList           mFReqPgList;

    /* Flush Request List Mutex */
    iduMutex          mMutex;

    scSpaceID         mLstSpaceID;

    // ��� ó���� ���� �� ����
    ULong             mAllocBuffPageTryCnt;
    ULong             mAllocBuffPageFailCnt;
    ULong             mBulkIOCnt;
} sdbDPathBuffInfo;

typedef struct sdbDPathBulkIOInfo
{
    /* ���� Buffer�� �����ּ� */
    UChar            *mRDIOBuffPtr;
    /* Direct IO�� ���ؼ� iduProperty::getDirectIOPageSize��
       Align�� Buffer�� �����ּ� */
    UChar            *mADIOBuffPtr;

    /* Buffer�� ũ�� */
    UInt              mDBufferSize;

    /* IO�� �����ؾ� �� Direct Path Buffer��
     * BCB���� ��ũ�� ����Ʈ�̴�.*/
    smuList           mBaseNode;

    /* ���� mBaseNode�� ����Ǿ� �ִ� BCB�� ����
     * �߰��ɶ����� 1�� �����ϰ� Bulk IO����� IO��
     * �Ϸ�Ǹ� 0���� �ȴ�.
     * */
    UInt              mIORequestCnt;

    /* ���������� mBaseNode�� �߰��� Page��
     * SpaceID�� PageID */
    scSpaceID         mLstSpaceID;
    scPageID          mLstPageID;
} sdbDPathBulkIOInfo;

/* TASK-4990 changing the method of collecting index statistics
 * MPR�� ParallelScan����� Sampling���� �̿��ϱ� ����, ���� Extent���� 
 * �ƴ����� �Ǵ��ϴ� CallbackFilter�� �д�. */
typedef idBool (*sdbMPRFilterFunc)( ULong   aExtSeq,
                                    void  * aFilterData );

typedef struct sdbMPRFilter4PScan
{
    ULong mThreadCnt;
    ULong mThreadID;
} sdbMPRFilter4PScan;

typedef struct sdbMPRFilter4SamplingAndPScan
{
    /* for Parallel Scan */
    ULong mThreadCnt;
    ULong mThreadID;

    /* for Sampling */
    SFloat mPercentage;
} sdbMPRFilter4SamplingAndPScan;


/****************************************************************************
 * PROJ-2102 Fast Secondaty Buffer  
 ****************************************************************************/
typedef enum
{
    SD_LAYER_BUFFER_POOL,      /* Primary  Buffer���� ȣ��� */
    SD_LAYER_SECONDARY_BUFFER  /* Secondary Buffer ���� ȣ��� */
}sdLayerState;

/*******************************************************************************
 * BCB �������� ���Ǵ� �κ��� ������
 ******************************************************************************/
#define  SD_BCB_PARAMETERS                                                    \
    /* Page�� ����� Space */                                                  \
    scSpaceID       mSpaceID;                                                  \
                                                                               \
    /* Space�ȿ��� ������ Number */                                            \
    scPageID        mPageID;                                                   \
                                                                               \
    /* sdbBCBHash�� ���� �ڷᱸ�� */                                           \
    smuList         mHashItem;                                                 \
                                                                               \
    /* ��� �ؽÿ� ���� �ִ��� �ĺ��� */                                       \
    UInt            mHashBucketNo;                                             \
                                                                               \
    /* ���� ����Ʈ�ʹ� ������ SDB_BCB_DIRTY�Ǵ� SDB_BCB_REDIRTY�� ��쿡��     \
     * check point list���� ���ϰ� �ȴ�. checkpoint list�� ���� �ڷᱸ��  */   \
    smuList         mCPListItem;                                               \
                                                                               \
    /* ����ȭ�� check point ����Ʈ �߿��� �ڽ��� ���� ����Ʈ�� �ĺ��� */       \
    UInt            mCPListNo;                                                 \
                                                                               \
    /* Page�� ���ʿ� Dirty�� ��ϵɶ�, System�� Last LSN */                    \
    smLSN           mRecoveryLSN;                                              \

typedef struct sdBCB
{
    SD_BCB_PARAMETERS
}sdBCB;


#define SDB_INIT_BCB_LIST( aBCB ) {                                            \
        SMU_LIST_INIT_NODE(&( ((sdbBCB*)aBCB)->mBCBListItem) );                \
        ((sdbBCB*)aBCB)->mBCBListType = SDB_BCB_NO_LIST;                       \
        ((sdbBCB*)aBCB)->mBCBListNo   = SDB_BCB_LIST_NONE;                     \
    }

#define SDB_INIT_CP_LIST( aBCB ) {                                             \
        SMU_LIST_INIT_NODE( &( ((sdBCB*)aBCB)->mCPListItem) );                 \
        ((sdBCB*)aBCB)->mCPListNo    = SDB_CP_LIST_NONE;                       \
    }

#define SDB_INIT_HASH_CHAIN_LIST( aBCB ) {                                     \
        SMU_LIST_INIT_NODE( &((sdBCB*)aBCB)->mHashItem );                      \
    }

#define SD_GET_CPLIST_NO( aBCB )                                               \
    (((sdBCB*)aBCB)->mCPListNo)

#define SD_GET_BCB_RECOVERYLSN( aBCB )                                         \
    (((sdBCB*)aBCB)->mRecoveryLSN)                       

#define SD_GET_BCB_SPACEID( aBCB )                                             \
    (((sdBCB*)aBCB)->mSpaceID)

#define SD_GET_BCB_PAGEID( aBCB )                                              \
    (((sdBCB*)aBCB)->mPageID)

#define SD_GET_BCB_CPLISTNO( aBCB )                                            \
    (((sdBCB*)aBCB)->mCPListNo)

#define SD_GET_BCB_CPLISTITEM( aBCB )                                          \
    (&( ((sdBCB*)aBCB)->mCPListItem ))

#define SD_GET_BCB_HASHITEM( aBCB )                                            \
    (&( ((sdBCB*)aBCB)->mHashItem ))

#define SD_GET_BCB_HASHBUCKETNO( aBCB )                                        \
    (((sdBCB*)aBCB)->mHashBucketNo)



// Flush�� Page�� File�� Sync �ϱ� ���� ������ ��´�.
typedef struct sdbSyncFileInfo
{
    scSpaceID mSpaceID;
    sdFileID  mFileID;
} sdbSyncFileInfo;

#endif  // _O_SDB_DEF_H_
