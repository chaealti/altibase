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
 * $Id: smrLogFileMgr.h 89697 2021-01-05 10:29:13Z et16 $
 **********************************************************************/

#ifndef _O_SMR_LOG_FILE_MGR_H_
#define _O_SMR_LOG_FILE_MGR_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smrLogFile.h>
#include <smrLogMultiplexThread.h>

class smrLFThread;


/* �������� �α������� �ϳ��� ���ӵ� �α׷� �����ϴ� Ŭ����
 *
 * �α����� �ϳ��� �� ����ؼ� ���� �α� ���Ϸ� switch�� ����� �����
 * �ּ�ȭ �ϱ� ���ؼ� ������ ����� �α����ϵ��� �̸� �غ��� ����
 * log file prepare �������� ���ҵ� �����Ѵ�.
 * 
 */
class smrLogFileMgr : public idtBaseThread
{
//For Operation
public:
    /* log file prepare �������� run�Լ�.
     *
     * �α����� �ϳ��� �� ����ؼ� ���� �α� ���Ϸ� switch�� ����� �����
     * �ּ�ȭ �ϱ� ���ؼ� ������ ����� �α����ϵ��� �̸� �غ��� ���´�.
     *
     * �α������� �غ��ϴ� ���� ������ ���� �ΰ��� ��쿡 �ǽ��Ѵ�.
     *   1. Ư�� �ð����� ��ٷ����� �ֱ������� �ǽ�
     *   2. preCreateLogFile �Լ� ȣ�⿡ ���ؼ�
     */
    virtual void run();

    /* �α������� �����Ͽ� �ش� �α�������
     * ���µ� �α����� ����Ʈ�� ������ �����Ѵ�.
     */
    
    IDE_RC open(UInt                aFileNo,
                idBool              aWrite,
                smrLogFile**        aLogFile);
    
    /* �α������� �����ϴ� �����尡 �� �̻� ���� ���,
     * �ش� �α������� close�ϰ� �α����� list���� close�� �α������� �����Ѵ�.
     */ 

    IDE_RC close(smrLogFile*  aLogFile);

    /* �α������� �����Ͽ� ��ũ���� �о� �´� */ 
    IDE_RC openLstLogFile( UInt          aFileNo,
                           UInt          aOffset,
                           smrLogFile ** aLogFile );
    
    IDE_RC closeAllLogFile();

    /* smuProperty::getLogFilePrepareCount()�� �����ϵ��� �α�������
     * �̸� �����Ѵ�.
     */
    IDE_RC addEmptyLogFile();

    /* log file prepare �����带 ������ �α������� �̸� ������ �д�.
     *
     * aWait => ID_TRUE �̸� �ش� �α������� ������ ������ ��ٸ���.
     *          ID_FALSE �̸� �α������� ������ ��û�� �ϰ� �ٷ� �����Ѵ�.
     */
    IDE_RC preCreateLogFile( idBool aWait );
    
    /* ������ prepare �� �ξ��� �α����ϵ��� ��� open�Ѵ�.
     */
    IDE_RC preOpenLogFile();

    /* ���� ������� �α������� ���� �α�������
     * ���� ����� �α����Ϸ� �����Ѵ�.
     * switch�� �α������� �������� ���� ���, �α������� ���� �����Ѵ�.
     */
    IDE_RC switchLogFile( smrLogFile**  sCurLogFile );
    
    /* �α����� prepare �����带 �����Ѵ�.
     */
    IDE_RC shutdown();

    /* �α����� ����Ʈ���� �α������� ã�´�.
     */
    idBool findLogFile( UInt           aLogFileNo, 
                        smrLogFile**   aLogFile);


    /* �α����� ��ü�� �α����� ����Ʈ�� �߰��Ѵ�.
     */
    IDE_RC addLogFile( smrLogFile *aNewLogFile );

    /* �ý��ۿ��� �αװ����� �ʱ�ȭ�ÿ� ȣ��Ǵ� �Լ��ν�
     * ����ϴ� �α����� �غ��ϰ�, �α����� ������ thread��
     * �����Ѵ�.
     */
    IDE_RC startAndWait( smLSN       * aEndLSN, 
                         UInt          aLstCreateLog,
                         idBool        aRecovery,
                         smrLogFile ** aLogFile );

    /* �α������� �α����� ����Ʈ���� �����Ѵ�.
     * ����! thread-safe���� ���� �Լ���.
     */
    inline void removeLogFileFromList( smrLogFile *aLogFile);

    /* CREATE DB ����ÿ� ȣ��Ǹ�, 0��° �α������� �����Ѵ�.
     *
     * aLogPath - [IN] �α����ϵ��� ������ ���
     */
    static IDE_RC create( const SChar * aLogPath );


    /*   aPrvLogFile�� �ٷ� �ڿ� aNewLogFile�� �����ִ´�.
     */
    IDE_RC AddLogFileToList( smrLogFile *aPrvLogFile,
                             smrLogFile *aNewLogFile );

    /* �α����ϵ��� DISK���� �����Ѵ�.
     * checkpoint���� �α����� ������ �����ϸ� ������Ȳ���� ó���ϰ�,
     * restart recovery�ÿ� �α����� ������ �����ϸ� �����Ȳ���� ó���Ѵ�.
     */
    void removeLogFile( UInt   aFstFileNo, 
                        UInt   aLstFileNo, 
                        idBool aIsCheckpoint );

    inline void setFileNo( UInt  aFileNo, 
                           UInt  aTargetFileNo,
                           UInt  aLstFileNo)
    {
        mCurFileNo    = aFileNo;
        mTargetFileNo = aTargetFileNo;
        mLstFileNo    = aLstFileNo;
    }

    inline void getLstLogFileNo( UInt *aLstFileNo )
    {
        *aLstFileNo = mLstFileNo;
    }

    inline void getCurLogFileNo( UInt *aCurFileNo )
    {
        *aCurFileNo = mCurFileNo;
    }

    /* �α����� ��ȣ�� �ش��ϴ� �α� �����Ͱ� �ִ��� üũ�Ѵ�.
     */
    IDE_RC checkLogFile( UInt aFileNo );

    // �α����� �����ڸ� �ʱ�ȭ �Ѵ�.
    IDE_RC initialize( const SChar     * aLogPath, 
                       const SChar     * aArchivePath,
                       smrLFThread     * aLFThread );
#if 0 // not used
    // aFileNo�� �ش��ϴ� Logfile�� �����ϴ��� Check�Ѵ�.
    IDE_RC isLogFileExist(UInt aFileNo, idBool & aIsExist);
#endif
    // �α����� �����ڸ� �����Ѵ�.
    IDE_RC destroy();

    /*
      aFileNo�� �ش��ϴ� Logfile�� Open�Ǿ����� check�ϰ�
      Open�Ǿ��ִٸ� LogFile�� Reference Count�� ������Ų��.
    */
    IDE_RC checkLogFileOpenAndIncRefCnt(UInt         aFileNo,
                                        idBool     * aAlreadyOpen,
                                        smrLogFile** aLogFile);

    static void removeLogFiles( UInt          aFstFileNo, 
                                UInt          aLstFileNo, 
                                idBool        aIsCheckpoint,
                                idBool        aIsMultiplexLogFile,
                                const SChar * aLogPath );

    smrLogFileMgr();
    virtual ~smrLogFileMgr();

    inline UInt getLFOpenCnt() {return mLFOpenCnt;}
    
    inline UInt getLFPrepareWaitCnt() {return mLFPrepareWaitCnt;}

    inline UInt getLFPrepareCnt() {return smuProperty::getLogFilePrepareCount();}
    
    inline UInt getLstPrepareLFNo() {return mLstFileNo;}
    
    inline UInt getCurWriteLFNo() {return mCurFileNo;}

    inline const SChar * getSrcLogPath() { return mSrcLogPath;}

private:
    inline IDE_RC lockListMtx() { return mMtxList.lock( NULL ); }
    inline IDE_RC unlockListMtx() { return mMtxList.unlock(); }

    inline IDE_RC lock() { return mMutex.lock( NULL ); }
    inline IDE_RC unlock() { return mMutex.unlock(); }

//For Log Mgr
private:
    
    // �� �α����� �����ڿ� ���� �α����ϵ��� Flush���� �α����� Flush ������
    smrLFThread     * mLFThread;
    
    // log file prepare thread�� �����ؾ� �ϴ� ��Ȳ���� ����
    // mFinish == ID_TRUE �� ���ǿ��� thread�� ����Ǿ�� �Ѵ�.
    idBool       mFinish;
    
    // log file prepare thread�� log file �����۾������� ����
    // mResume == ID_FALSE �̸� thread�� sleep���¿� �ִ� ���̴�.
    idBool       mResume;

    // log file prepare thread�� �ٸ� thread���� ���ü���� ����
    // Condition value.
    // mMutex�� �Բ� ���ȴ�.
    iduCond      mCV;
    
    // ���� �α׸� ����ϰ� �ִ� �α����Ϲ�ȣ
    UInt         mCurFileNo;
    
    // ���������� prepare�� Log file��ȣ
    UInt         mLstFileNo;
    
    // ù��° prepared�� Log file��ȣ
    // �׻� mCurFileNo + 1�� ���� ���Ѵ�.
    UInt         mTargetFileNo;

    // log file prepare thread�� �ٸ� thread���� ���ü���� ���� Mutex
    iduMutex     mMutex;

    // mFstLogFile�� mOpenLogFileCount �� ���� ���ü� ��� ���� Mutex
    iduMutex     mMtxList;

    // ���� �����ִ� �α����� ����
    UInt         mOpenLogFileCount;
    
    // ���� �����ִ� �α������� linked list
    // �� ��ũ�� ����Ʈ�� ���۹����
    // smrLFThread��  mSyncLogFileList �� ���� �ּ��� �ڼ��� ����Ǿ� �ִ�.
    smrLogFile   mFstLogFile;

    // smrLogFile ��ü�� ����� �޸� ������ ���ϴ� memory pool
    // iduMemPool��ü�� thread safe�ϹǷ�, ������ ���ü� ��� �� �ʿ䰡 ����.
    iduMemPool   mMemPool;

    // �α������� �ʱ�ȭ�� �����͸� ���ϴ� ����
    // OS PAGE ũ�⸸ŭ align�Ǿ� �ִ�.
    SChar*       mLogFileInitBuffer;
    
    // mLogFileInitBuffer �� ������ �Ҵ�� align���� ���� �ּ�.
    SChar*       mLogFileInitBufferPtr;

    // ���� Open�� LogFile�� ����
    UInt         mLFOpenCnt;

    // log switch �߻��� wait event�� �߻��� Ƚ��
    UInt         mLFPrepareWaitCnt;

    const SChar*  mSrcLogPath;
    const SChar*  mArchivePath;

    SChar        mTempLogFileName[SM_MAX_FILE_NAME]; /* BUG-49409 */

public:
    static smrLogMultiplexThread * mSyncThread;
    static smrLogMultiplexThread * mCreateThread;
    static smrLogMultiplexThread * mDeleteThread;
    static smrLogMultiplexThread * mAppendThread;
};


/* �α������� �α����� ����Ʈ���� �����Ѵ�.
 * ����! thread-safe���� ���� �Լ���.
 */
inline void smrLogFileMgr :: removeLogFileFromList( smrLogFile*  aLogFile )
{
    aLogFile->mPrvLogFile->mNxtLogFile = aLogFile->mNxtLogFile;
    aLogFile->mNxtLogFile->mPrvLogFile = aLogFile->mPrvLogFile;

    mLFOpenCnt--;
}

inline void smrLogFileMgr::removeLogFiles( UInt          aFstFileNo, 
                                           UInt          aLstFileNo, 
                                           idBool        aIsCheckPoint,
                                           idBool        aIsMultiplexLogFile,
                                           const SChar * aLogPath )
{
    UInt    i               = 0;
    UInt    sStartFileNum   = 0;
    SChar   sLogFileName[ SM_MAX_FILE_NAME ];

    IDE_ASSERT( aLogPath != NULL );

    sStartFileNum   = aFstFileNo;

    /* BUG-42589: �α� ���� ���� �� �� üũ����Ʈ ��Ȳ�� �ƴϾ(restart recovery)
     * �α� ���� ���� ������ Ʈ���̽� �α׷� �����. */
    if ( aIsCheckPoint == ID_FALSE )
    {
        ideLog::log( IDE_SM_0, "Remove Log File : File[<%"ID_UINT32_FMT"> ~ <%"ID_UINT32_FMT">]\n", aFstFileNo, aLstFileNo-1 );
    }
    else
    {
        // do nothing
    }

#if defined(VC_WINCE)
    //fix ERROR_SHARING_VIOLATION error

    if( (aFstFileNo - smuProperty::getChkptIntervalInLog()) > 0 )
    {
        sStartFileNum   = aFstFileNo - smuProperty::getChkptIntervalInLog();
    }
    else
    {
        sStartFileNum   = 0;
    }

    for( i = sStartFileNum ; i < aLstFileNo ; i++ )
#else
    for( i = sStartFileNum ; i < aLstFileNo ; i++ )
#endif
    {
        idlOS::snprintf(sLogFileName,
                        SM_MAX_FILE_NAME,
                        "%s%c%s%"ID_UINT32_FMT, 
                        aLogPath, 
                        IDL_FILE_SEPARATOR, 
                        SMR_LOG_FILE_NAME, 
                        i);

        if( (aIsMultiplexLogFile == ID_TRUE) && 
            (idf::access( sLogFileName, F_OK) != 0) )
        {
            continue;
        }

        if(smrLogFile::remove(sLogFileName, aIsCheckPoint) != IDE_SUCCESS)
        {
            ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                        SM_TRC_MRECOVERY_LOGFILEMGR_CANNOT_REMOVE_LOGFILE,
                        sLogFileName,
                        (UInt)errno);
        }
    }
}

#endif /* _O_SMR_LOG_FILE_MGR_H_ */
