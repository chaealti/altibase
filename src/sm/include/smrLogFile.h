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
 * $Id: smrLogFile.h 89697 2021-01-05 10:29:13Z et16 $
 **********************************************************************/

#ifndef _O_SMR_LOG_FILE_H_
#define _O_SMR_LOG_FILE_H_ 1

#include <idu.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smrDef.h>
#include <smrLogHeadI.h>

/*
 * �ϳ��� �α����Ͽ� ���� operation���� ������ ��ü
 *
 */
class smrLogFile
{
    friend class smrBackupMgr;
    friend class smrLogMgr;
    friend class smrLogFileMgr;
    friend class smrRemoteLogMgr; //PROJ-1915

//For Operation
public:
    static IDE_RC initializeStatic( UInt aLogFileSize );
    static IDE_RC destroyStatic();

    IDE_RC initialize();
    IDE_RC destroy();

    IDE_RC create( SChar  * aStrFilename,
                   SChar  * aInitBuffer,
                   UInt     aSize );

    IDE_RC prepare( SChar   * aStrFilename,
                    SChar   * aTempFileName,
                    SChar   * aInitBuffer,
                    SInt      aSize );

    IDE_RC backup(SChar   * aBackupFullFilePath);

    /* �α������� DISK���� �����Ѵ�.
     * checkpoint���� �α����� ������ �����ϸ� ������Ȳ���� ó���ϰ�,
     * restart recovery�ÿ� �α����� ������ �����ϸ� �����Ȳ���� ó���Ѵ�.
     */
    static IDE_RC remove( SChar   * aStrFileName,
                          idBool    aIsCheckpoint );


    inline void append( SChar   * aStrData,
                        UInt      aSize );

    /* BUG-45711 */
    inline void scribble( SChar  * aStrData,
                           UInt     aSize,
                           UInt     aOffset );

    void   read( UInt     aOffset,
                 SChar  * aStrBuffer,
                 UInt     aSize );

    void   read( UInt       aOffset,
                 SChar   ** aLogPtr );

    IDE_RC readFromDisk( UInt     aOffset,
                         SChar  * aStrBuffer,
                         UInt     aSize );

    void   clear( UInt aBegin );
    inline IDE_RC sync() { return mFile.sync(); }

    // �α� ��Ͻ� Align�� Page�� ũ�� ���� 
    UInt getLogPageSize();

    // �α׸� Ư�� Offset���� Sync�Ѵ�.
    IDE_RC syncLog( idBool  aSyncLastPage,
                    UInt    aOffsetToSync );

    inline void setPos( UInt    aFileNo,
                        UInt    aOffset )
    {
        mFileNo         = aFileNo;
        mOffset         = aOffset;
        mFreeSize       = mSize - aOffset;
        mPreSyncOffset  = aOffset;
        mSyncOffset     = aOffset;
    }

    inline IDE_RC lock();
    inline IDE_RC unlock();

    IDE_RC write( SInt     aWhere,
                  void   * aBuffer,
                  SInt     aSize );

    static void   getMemoryStatus() { mLogBufferPool.status(); }

    inline UInt getFileNo() { return mFileNo; }

#if defined(IA64_HP_HPUX)
   SInt touchMMapArea();
#else
   void touchMMapArea();
#endif

    // Log Buffer�� aStartOffset���� aEndOffset���� Disk�� ������.
    IDE_RC syncToDisk( UInt aStartOffset, UInt aEndOffset );

    // �ϳ��� �α� ���ڵ尡 Valid���� ���θ� �Ǻ��Ѵ�.
    static idBool isValidLog( smLSN       * aLSN,
                              smrLogHead  * aLogHeadPtr,
                              SChar       * aLogPtr,
                              UInt          aLogSizeAtDisk);

    // �α����Ϲ�ȣ�� �α׷��ڵ� ���������� �����ѹ��� �����Ѵ�.
    static inline smMagic makeMagicNumber( UInt     aFileNo,
                                           UInt     aOffset );

    // BUG-37018 log�� magic nunber�� ��ȿ���� �˻��Ѵ�.
    static inline idBool isValidMagicNumber( smLSN       * aLSN,
                                             smrLogHead  * aLogHeadPtr );


    //* �α������� File Begin Log�� �������� üũ�Ѵ�.
    IDE_RC checkFileBeginLog( UInt aFileNo );
    idBool isOpened() { return mIsOpened; };

    SChar* getFileName() { return mFile.getFileName(); }

    // Direct I/O�� ����� ��� Direct I/O Pageũ���
    // Align�� �ּҸ� mBase�� ����
    IDE_RC allocAndAlignLogBuffer();

    // allocAndAlignLogBuffer �� �Ҵ��� �α׹��۸� Free�Ѵ�.
    IDE_RC freeLogBuffer();

    inline void   setEndLogFlush( idBool aFinished );
    inline idBool isAllLogSynced(); /* BUG-35392 */

    inline idBool getEndLogFlush();

    smrLogFile();
    virtual ~smrLogFile();

    // �α������� open�Ѵ�.
    IDE_RC open( UInt     aFileNo,
                 SChar  * aStrFilename,
                 idBool   aIsMultiplexLogFile,
                 UInt     aSize,
                 idBool   aWrite      = ID_FALSE );

    IDE_RC close();

private:
    IDE_RC mmap( UInt   aSize,
                 idBool aWrite );
    IDE_RC unmap( void );
    UInt   getLastValidOffset(); /* BUG-35392 */
    
//For Member
public:
    //�α������� ������ ���̻� ��ũ�� ����� �ʿ䰡 ���ٸ�
    //ID_TRUE, else ID_FALSE
    idBool       mEndLogFlush;

    //Open�Ǿ����� ID_TRUE, else ID_FALSE
    idBool       mIsOpened;
    
    // ��ũ���� �α�����
    iduFile      mFile;

    /* Logfile�� FileNo */
    UInt         mFileNo;
    /* Logfile�� �αװ� ��ϵ� ��ġ */
    UInt         mOffset;
    
    // ������� sync�� �Ϸ�� ũ��
    // ��, �������� sync�� �����ؾ��� offset
    UInt         mSyncOffset;

    // Sync��û�� �������� ������ �������� sync���� �� �� ���
    // mSyncOffset�� mPreSyncOffset�� ����صд�.
    // syncLog�Լ������� mPreSyncOffset�� ����,
    // ������ ������ �������� sync���� ��쿡��
    // ������ �������� sync�� �ϵ��� �Ѵ�.
    UInt         mPreSyncOffset;
    UInt         mFreeSize;
    UInt         mSize;

    // �ϳ��� �α������� ������ ���ϴ� ����
    //
    // Durability Level 5���� 
    // Direct I/O�� �Ѵٸ� Log Buffer�� ���� �ּ� ����
    // Direct I/O Pageũ�⿡ �°� Align�� ���־�� �Ѵ�.
    //  - mBaseAlloced : mLogBufferPool���� �Ҵ���� Align���� ���� �α׹���
    //  - mBase        : mBaseAlloced�� Direct I/O Pageũ�� �� Align�� �α׹���
    void        *mBaseAlloced;
    void        *mBase;

    // �� �α������� reference count
    UInt         mRef;

    // �� �α����Ͽ� ���� ���ü� ��� ���� Mutex
    iduMutex     mMutex;
    // �� �α������� �� �Ἥ �ٸ� �α����Ϸ� switch�Ǿ����� ����
    idBool       mSwitch;
    // �� �α����Ͽ� ���Ⱑ �������� ����
    idBool       mWrite;
    // �� �α������� mmap���� �������� ����
    idBool       mMemmap;
    // ��ũ�� �α������� mFile�� �������� ���� 
    idBool       mOnDisk;

    // smrLogFleMgr���� open�� �α����� ����Ʈ�� ������ �� ���
    smrLogFile  *mPrvLogFile;
    smrLogFile  *mNxtLogFile;

    // smrLFThread���� flush�� �α����� ����Ʈ�� ������ �� ���
    smrLogFile  *mSyncPrvLogFile;
    smrLogFile  *mSyncNxtLogFile;

    //PROJ-2232 log multiplex
    //����ȭ �α׹��ۿ� SMR_LT_FILE_END���� ����Ǿ��� ����ȭ �α�������
    //���� �α����Ϸ� switch�Ǿ����� Ȯ��
    volatile idBool   mMultiplexSwitch[ SM_LOG_MULTIPLEX_PATH_MAX_CNT ]; 

    // �α����� ���۸� �Ҵ���� �޸� Ǯ
    static iduMemPool mLogBufferPool;

//For Test
    UInt         mLstSyncPos;

    idBool       mIsMultiplexLogFile;
};

inline IDE_RC smrLogFile::lock()
{
    return mMutex.lock( NULL );
}

inline IDE_RC smrLogFile::unlock()
{

    return mMutex.unlock();

}

/*
 * �α����� ��ȣ�� �α׷��ڵ� �������� �̿��Ͽ�
 * �α����Ͽ� ����� �����ѹ��� �����Ѵ�.
 *
 * [ �����ѹ��� ���� ]
 * �����ѹ��� 32��Ʈ ������ ��� �α��� ����� ��ϵȴ�.
 * +--------------------+-------------------------+
 * | ������Ʈ           |  ������Ʈ               |
 * | 9 ��Ʈ ( 0~ 511 )  |  23 ��Ʈ ( 0 ~ 8M - 1 ) |
 * | �α����Ϲ�ȣ       |  �α׷��ڵ� offset      |
 * +--------------------+-------------------------+
 *
 * SYNC_CREATE_ = 0 ���� �־� �α������� 0���� memset���� �ʵ��� ������ ���
 * Invalid�� Log�� Valid�� �α׷� �Ǻ��� Ȯ���� ���߾��ش�.
 */


/*
 * �α����Ϲ�ȣ�� �α׷��ڵ� ���������� �����ѹ��� �����Ѵ�.
 *
 * aFileNo [IN] �α����Ϲ�ȣ
 * aOffset [IN] �α����� ������
 *
 * return       �α����� ��ȣ�� �������� �̿��Ͽ� ������ �����ѹ��� �����Ѵ�.
 */
inline smMagic smrLogFile::makeMagicNumber( UInt    aFileNo,
                                            UInt    aOffset )
{
#define LOG_FILE_NO_BITS (4)
#define LOG_FILE_NO_MASK ( (1<<LOG_FILE_NO_BITS)-1 )

#define LOG_RECORD_OFFSET_BITS (12)
#define LOG_RECORD_OFFSET_MASK ( (1<<LOG_RECORD_OFFSET_BITS)-1 )
    UInt sMagic;

    IDE_DASSERT( (LOG_FILE_NO_BITS + LOG_RECORD_OFFSET_BITS) == 16 );

    sMagic = ( ( aFileNo & ( LOG_FILE_NO_MASK ) ) << LOG_RECORD_OFFSET_BITS ) |
               ( aOffset & LOG_RECORD_OFFSET_MASK );

    /* BUG-35392
     * �����ѹ��� 2����Ʈ smMagic(UShort)���θ� ����ϹǷ�
     * UInt�� �ƴ� 2����Ʈ(smMagic)�� ���� �ϵ��� �Ѵ�. */
    sMagic &= 0x0000FFFF;

    return (smMagic)sMagic;
}

void smrLogFile::setEndLogFlush( idBool aFinished )
{
    if( aFinished == ID_TRUE )
    {
        IDE_ASSERT( mSyncOffset == mOffset );
    }

    mEndLogFlush = aFinished;
}

/***********************************************************************
 * BUG-35392 
 * Description : LogFile�� ��� Sync�Ǿ������� ���θ� ��ȯ�Ѵ�.
 *               switch��� �α׿� ���ؼ��� ȣ�� �ǹǷ�
 *               mSyncOffset, mOffset �� ���ü��� �������� �ʾƵ� �ȴ�.
 ***********************************************************************/
idBool smrLogFile::isAllLogSynced()
{
    IDE_ASSERT( mSyncOffset <= mOffset );

    if( mSyncOffset == mOffset )
    {
        return  ID_TRUE;
    }

    return  ID_FALSE;
}

idBool smrLogFile::getEndLogFlush()
{
    return mEndLogFlush;
}

/***********************************************************************
 * BUG-37018
 * Description : code �ߺ��� �����ϱ� ���� �߰��� inline �Լ�
 *               dummy log�� valid�˻�� magic number�θ� ��������
 *               �ϱ⶧���� �� �Լ��� �����Ѵ�.
 *               (�Ϲ� �α��� valid�˻��Ҷ��� ���ȴ�) 
 **********************************************************************/
idBool smrLogFile::isValidMagicNumber( smLSN       * aLSN,
                                       smrLogHead  * aLogHeadPtr )
{
    idBool sIsValid;

    if ( makeMagicNumber( aLSN->mFileNo,
                          aLSN->mOffset )
         != smrLogHeadI::getMagic(aLogHeadPtr) )
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,
                    SM_TRC_MRECOVERY_LOGFILE_INVALID_MAGIC,
                    aLSN->mFileNo,
                    aLSN->mOffset,
                    smrLogHeadI::getMagic(aLogHeadPtr),
                    makeMagicNumber(aLSN->mFileNo, 
                                    aLSN->mOffset));

        sIsValid = ID_FALSE;
    }
    else
    {
        sIsValid = ID_TRUE;
    }
    
    return sIsValid;
}

inline void smrLogFile::append( SChar  * aStrData,
                                UInt     aSize )
{
    IDE_ASSERT( ( aSize != 0 ) && ( mFreeSize >= (UInt)aSize ) );

    idlOS::memcpy( (SChar*)mBase + mOffset,
                    aStrData,
                    aSize );

    /* BUG-45916 MEM_BARRIER ���Ž� AIX ��񿡼�
     * Log Multiplex Thread�� memcpy �Ϸ� ������
     * Log�� Copy �ϴ� ��찡 �߻���. */
    IDL_MEM_BARRIER;

    mFreeSize -= aSize;
    mOffset   += aSize;
}

/* BUG-45711 */
inline void smrLogFile::scribble( SChar  * aStrData,
                                  UInt     aSize,
                                  UInt     aOffset )
{
    /* �α� ���� */
    idlOS::memcpy( (SChar *)mBase + aOffset,
                    aStrData,
                    aSize );
}

#endif /* _O_SMR_LOG_FILE_H_ */
