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

/*******************************************************************************
 * $Id: rpdXLogfileMgr.h
 ******************************************************************************/

#ifndef _O_RPD_XLOGFILEMGR_H_
#define _O_RPD_XLOGFILEMGR_H_ 1

#include <idtBaseThread.h>
#include <rpdXLogfileFlusher.h>
#include <rpdXLogfileCreater.h>
#include <sdi.h>
#include <idp.h>
#include <idu.h>
#include <smi.h>
#include <qci.h>
#include <rpDef.h>

typedef struct rpdXLogfile
{
    iduFile         mFile;
    UInt            mFileNo;
    void *          mBase;
    rpdXLogfile *   mNextXLogfile;
    rpdXLogfile *   mPrevXLogfile;
    SInt            mFileRefCnt;
    iduMutex        mRefCntMutex; //�ش� mutex�� refCnt�� �����Ҷ� ��´�.
    idBool          mIsWriteEnd;
} rpdXLogfile;

typedef struct rpdXLogfileHeader
{
    UInt    mFileWriteOffset;
    UInt    mFileFirstXLogOffset;
    smSN    mFileFirstXSN;
} rpdXLogfileHeader;

class rpdXLogfileMgr
{
public:
    /* �ܺο��� ȣ�Ⱑ���� �������̽� */
    IDE_RC  initialize( SChar *     aReplName, 
                        rpXLogLSN   aXLogLSN, 
                        UInt        aExceedTimeout,
                        idBool *    aExitFlag,
                        void *      aReceiverPtr,
                        rpXLogLSN    aLastXLogLSN );
    IDE_RC  finalize();

    /* read */
    IDE_RC  checkRemainXLog();
    IDE_RC  getRemainSize( UInt *   aRemainSize );
    IDE_RC  getNextFileRemainSize( UInt *   aRemainSize );
    idBool  isLastXLog();
    IDE_RC  readXLog( void * aDest, UInt aLength, idBool aIsConvert = ID_TRUE );
    IDE_RC  readXLogDirect( void ** aMMapPtr, UInt aLength );

    /* write */
    IDE_RC  writeXLog( void * aSrc,
                       UInt   aLength,
                       smSN	  aXSN );

    /* xlogfile open */
    IDE_RC  openXLogfileForWrite();
    IDE_RC  openXLogfileForRead( UInt aXLogfileNo );
    IDE_RC  openFirstXLogfileForRead();
    IDE_RC  openAndMappingFile( UInt aXLogfileNo, rpdXLogfile ** aNewFile );
    static IDE_RC  openAndReadHeader( SChar * aRepName, SChar * aPath,
                                      UInt    aXLogfileNo, rpdXLogfileHeader * aOutHeader );
    void    setFileMeta( rpdXLogfile *  aFile, UInt aFileNo );

    /* for recovery */
    IDE_RC  findLastWrittenFileAndSetWriteInfo();

    void    getReadInfo( rpXLogLSN * aXLogLSN );

    SChar   mRepName[QCI_MAX_NAME_LEN + 1];
    SChar * mXLogDirPath;

public:
    /* ���� ������� xlogfile�� ���� */
    rpXLogLSN   getReadXLogLSNWithLock();
    UInt        getReadFileNoWithLock();
    void        setReadInfoWithLock( rpXLogLSN aLSN, UInt  aFileNo );
    rpXLogLSN   mWriteXLogLSN;
    UInt        mWriteFileNo;
    iduMutex    mWriteInfoMutex;
    void *      mWriteBase;

    rpXLogLSN   getWriteXLogLSNWithLock();
    UInt        getWriteFileNoWithLock();
    void        setWriteInfoWithLock( rpXLogLSN aLSN, UInt  aFileNo );
    rpXLogLSN   mReadXLogLSN;
    UInt        mReadFileNo;
    iduMutex    mReadInfoMutex;
    void *      mReadBase;
    void        setWaitWrittenXLog( idBool aIsWait );
    idBool      getWaitWrittenXLog();

    /* ���� �����ִ� xlogfile */
    rpdXLogfile *   mCurWriteXLogfile;
    rpdXLogfile *   mCurReadXLogfile;

    void    addNewFileToFileList( rpdXLogfile * aBeforeFile, rpdXLogfile * aNewFile );
    void    addToFileListinLast( rpdXLogfile * aNewFile );

    rpdXLogfile *   mXLogfileListHeader;
    rpdXLogfile *   mXLogfileListTail;
    iduMutex        mXLogfileListMutex;

    ULong           mFileMaxSize;
    UInt            mExceedTimeout;

    /* file�� reference count ���� */
    UInt    getFileRefCnt( rpdXLogfile * aFile );
    void    incFileRefCnt( rpdXLogfile * aFile );
    void    decFileRefCnt( rpdXLogfile * aFile );

    /* xlogfile ���� ���� */
    void    removeFile( rpdXLogfile * aFile );

    /* sub thread ���� */
    IDE_RC  stopSubThread();
    void    destroySubThread();
    rpdXLogfileFlusher *  mXLFFlusher;
    rpdXLogfileCreater *  mXLFCreater;

    /* ��Ÿ */
    idBool      mEndFlag;
    idBool *    mExitFlag;
    idBool      mWaitWrittenXLog;

    /* read�� sleep/wake ���� */
    void        sleepReader();
    void        wakeupReader();
    iduMutex    mReadWaitMutex;
    iduCond     mReadWaitCV;
    idBool      mIsReadSleep;
    // read�� write�� ����������� read�� �۾� ��� ���� ���

    /* write�� sleep/wake ���� */
    void        sleepWriter();
    void        wakeupWriter();
    iduMutex    mWriteWaitMutex;
    iduCond     mWriteWaitCV;
    idBool      mIsWriteSleep;
    // write�� �� ������ �䱸�ϳ� �� ������ ���� �������� �ʾ����� write�� �۾� ��� ���� ���

    /* static �Լ� */
    static IDE_RC   removeAllXLogfiles( SChar * aReplName, UInt aFileNo );
    static void     rpdXLogfileEmergency( idBool aFlag );

    IDE_RC          getXLogLSNFromXSN( smSN aXSN, rpXLogLSN * aXLogLSN );
    static IDE_RC   getXLogLSNFromXSNAndReplName( smSN aXSN, SChar * aReplName, rpXLogLSN * aXLogLSN );
    static IDE_RC   getXLogfileNumberInternal( smSN aXSN,
                                               SInt aStartFileNo,
                                               SChar * aReplName,
                                               rpXLogLSN * aXLogLSN );

    static UInt checkXLogFileAndGetFileNoFromRepName( SChar * aReplName,
                                                      SChar * aFileName,
                                                      idBool * aIsXLogfile );
    static IDE_RC findFirstAndLastFileNo( SChar * aReplName,
                                          UInt * aFirstXLogFileNo,
                                          UInt * aLastXLogFileNo );

    static IDE_RC findRemainFirstXLogLSN( SChar     * aReplName,
                                          rpXLogLSN * aFirstXLogLSN );


    /* debug�� �Լ� */
    IDE_RC  byteToString( UChar   * aByte,
                          UShort    aByteSize,
                          UChar   * aString,
                          UShort  * aStringSize );
};

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::getReadInfo                *
 * ------------------------------------------------------------------*
 * ���� read info(XLogLSN)�� ���� ���� �Ѱ� replication meta�� �����Ѵ�.
 *
 * aXLogLSN    - [OUT] read ���� xlogfile�� XLogLSN
 *********************************************************************/
inline void rpdXLogfileMgr::getReadInfo( rpXLogLSN * aXLogLSN )
{
    IDE_ASSERT( aXLogLSN != NULL );

    IDE_ASSERT( mReadInfoMutex.lock( NULL ) == IDE_SUCCESS );
    *aXLogLSN    = mReadXLogLSN;
    IDE_ASSERT( mReadInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::isLastXLog                 *
 * ------------------------------------------------------------------*
 * �ش� �Լ��� recovery�� ��� gap�� ó���Ǿ����� üũ�ϱ� ���� ���ȴ�.
 *
 * aIsLastXLog - [OUT] xlogfile�� ��� xlog gap�� ó���Ǿ����� ����
*********************************************************************/
inline idBool rpdXLogfileMgr::isLastXLog()
{
    idBool  sIsEnd = ID_FALSE;

    if( mReadXLogLSN == mWriteXLogLSN )
    {
        sIsEnd = ID_TRUE;
    }
    else
    {
        sIsEnd = ID_FALSE;
    }

    return sIsEnd;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileMgr::setWaitWrittenXLog         *
 * ------------------------------------------------------------------*
 * �⺻������ read�� read�� write�� �������� ��� ���� �ð� ����Ѵ�.
 * �׷��� ���ܻ�Ȳ������ write�� �Ϻθ� ������ ���¿��� ���̻� write��
 * ������� �ʴ� ��� read�� ������ �ʴ� write�� ����ϴ� ��찡 �߻��� �� �ִ�.
 * �̸� �������� mWaitWrittenXLog��� ������ �ΰ�
 * �ش� ������ true�� ��쿡�� read�� write�� ����������� ����ϵ��� �Ѵ�.
*********************************************************************/
inline void rpdXLogfileMgr::setWaitWrittenXLog( idBool  aIsWait )
{
    mWaitWrittenXLog = aIsWait;

    return;
}

inline idBool rpdXLogfileMgr::getWaitWrittenXLog( )
{
    return mWaitWrittenXLog;
}
#endif // _O_RPD_XLOGFILEMGR_H_
