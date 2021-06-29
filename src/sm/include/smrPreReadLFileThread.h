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
 * $Id: smrPreReadLFileThread.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/
#ifndef _O_SMR_PREREADLFILE_THREAD_H_
#define _O_SMR_PREREADLFILE_THREAD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <smrDef.h>
#include <smrLogFile.h>
#include <iduMemListOld.h>

/* PreReadInfo�� mFlag�� */
#define SMR_PRE_READ_FILE_MASK  (0x00000003)
/* �ʱⰪ */
#define SMR_PRE_READ_FILE_NON   (0x00000000)
/* File�� open�� �Ϸ�Ǿ��� �� */
#define SMR_PRE_READ_FILE_OPEN  (0x00000001)
/* File�� Close�� ��û�Ǿ��� �� */
#define SMR_PRE_READ_FILE_CLOSE (0x00000002)

/*
  Pre Read Thread�� �ڽſ��� ������ Open Request
  �� �����ϱ� ���ؼ� Request�� ���ö� ���� �ϳ���
  ���������.
*/
typedef struct smrPreReadLFInfo
{
    // �б⸦ ��û�� FileNo
    UInt        mFileNo;
    // Open�� LogFile
    smrLogFile *mLogFilePtr;
    
    // ���� File�� Open�Ǿ����� SMR_PRE_READ_FILE_OPEN,
    // �ƴϸ� SMR_PRE_READ_FILE_CLOSE
    UInt        mFlag;

    struct smrPreReadLFInfo *mNext;
    struct smrPreReadLFInfo *mPrev;
} smrPreReadLFInfo;

/*
  smrPreReadLFileThread�� Replication �� Sender��
  ���ؼ� ������� ������ Sender�� �о�� �� ���Ͽ� ���ؼ�
  �̸� Read�� �����Ͽ� Sender�� Disk/IO������ waiting��
  �߻��ϴ°��� �����ϱ����� ���������.
*/
class smrPreReadLFileThread : public idtBaseThread
{
//Member Function
public:
    /* �ʱ�ȭ�� �����Ѵ�.*/
    IDE_RC initialize();
    /* open�� Logfile�� close�ϰ� �Ҵ�� Resource�� ��ȯ�Ѵ�.*/
    IDE_RC destroy();

    /* PreReadThread���� aFileNo�� �ش��ϴ� ���Ͽ�
       ���ؼ� open�� ��û�Ѵ�.*/
    IDE_RC addOpenLFRequest( UInt aFileNo );
    /* aFileNo�� �ش��ϴ� File�� Close�� ��û�Ѵ�.*/
    IDE_RC closeLogFile( UInt aFileNo );

    /* Thread�� �����Ų��.*/
    IDE_RC shutdown();
    
    virtual void run();

    /* PreRead Thread�� Sleep���̸� �����.*/
    IDE_RC resume();

    smrPreReadLFileThread();
    virtual ~smrPreReadLFileThread();
    
//Member Function
private:
    /* Request List, Open Log File List���뼭 ���ٽ� mMutex.lock����*/
    inline IDE_RC lock();
    /* mMutex.unlock����*/
    inline IDE_RC unlock();
    
    inline IDE_RC lockCond() { return mCondMutex.lock( NULL /* idvSQL* */ ); }
    inline IDE_RC unlockCond() { return mCondMutex.unlock(); }
    
    /* aInfo�� Request List�� �߰�*/
    inline void addToLFRequestList(smrPreReadLFInfo *aInfo);
    /* aInfo�� Request List���� ����*/
    inline void removeFromLFRequestList(smrPreReadLFInfo *aInfo);
    /* aInfo�� Open Log File List�� �߰�*/
    inline void addToLFList(smrPreReadLFInfo *aInfo);
    /* aInfo�� Open Log File List���� ����*/
    inline void removeFromLFList(smrPreReadLFInfo *aInfo);
    /* Request List�� ����ִ��� check */
    inline idBool isEmptyOpenLFRequestList() 
        { return mOpenLFRequestList.mNext == &mOpenLFRequestList ? ID_TRUE : ID_FALSE; }
    /* smrPreReadLFInfo�� �ʱ�ȭ*/
    inline void initPreReadInfo(smrPreReadLFInfo *aInfo);

    /* aFileNo�� �ش��ϴ� PreRequestInfo�� Request List���� ã�´�.*/
    smrPreReadLFInfo* findInOpenLFRequestList( UInt aFileNo );
    /* aFileNo�� �ش��ϴ� PreRequestInfo�� Open Logfile List���� ã�´�.*/
    smrPreReadLFInfo* findInOpenLFList( UInt aFileNo );
    /* logfile�� Open�� ������ smrPreReadInfo�� ã�´�.*/
    IDE_RC getJobOfPreReadInfo(smrPreReadLFInfo **aPreReadInfo);

//Member Variable    
private:
    /* Open LogFile Request List*/
    smrPreReadLFInfo mOpenLFRequestList;
    /* Open LogFile List*/
    smrPreReadLFInfo mOpenLFList;
    /* List���� Mutex*/
    iduMutex         mMutex;
    /* Pre Read Info Memory Pool */
    iduMemListOld    mPreReadLFInfoPool;
    /* Thread ���� Check */
    idBool           mFinish;

    /* Condition Variable */
    iduCond          mCV;
    /* Time Value */
    PDL_Time_Value   mTV;

    /* Waiting���� Mutex�μ� Thread�� sleep�� ���
     �� Mutex�� Ǯ�� waiting�Ѵ�.*/
    iduMutex         mCondMutex;

    /* Thread�� Wake up�� ��� �̰��� ID_TRUE�� �Ѵ�. */
    idBool           mResume;
};

inline IDE_RC smrPreReadLFileThread::lock()
{
    return mMutex.lock( NULL /* idvSQL* */ );
}

inline IDE_RC smrPreReadLFileThread::unlock()
{
    return mMutex.unlock();
}

inline void smrPreReadLFileThread::addToLFRequestList(smrPreReadLFInfo *aInfo)
{
    aInfo->mNext = &mOpenLFRequestList;
    aInfo->mPrev = mOpenLFRequestList.mPrev;
    mOpenLFRequestList.mPrev->mNext = aInfo;
    mOpenLFRequestList.mPrev = aInfo;
}

inline void smrPreReadLFileThread::removeFromLFRequestList(smrPreReadLFInfo *aInfo)
{
    aInfo->mNext->mPrev = aInfo->mPrev;
    aInfo->mPrev->mNext = aInfo->mNext;
}

inline void smrPreReadLFileThread::addToLFList(smrPreReadLFInfo *aInfo)
{
    aInfo->mNext = &mOpenLFList;
    aInfo->mPrev = mOpenLFList.mPrev;
    mOpenLFList.mPrev->mNext = aInfo;
    mOpenLFList.mPrev = aInfo;
}

inline void smrPreReadLFileThread::removeFromLFList(smrPreReadLFInfo *aInfo)
{
    aInfo->mNext->mPrev = aInfo->mPrev;
    aInfo->mPrev->mNext = aInfo->mNext;
}

inline void smrPreReadLFileThread::initPreReadInfo(smrPreReadLFInfo *aInfo)
{
    aInfo->mNext   = NULL;
    aInfo->mPrev   = NULL;
    aInfo->mFileNo = ID_UINT_MAX;
    aInfo->mLogFilePtr = NULL;
    aInfo->mFlag   = SMR_PRE_READ_FILE_NON;
}

#endif /* _O_SMR_PREREADLFILE_THREAD_H_ */


