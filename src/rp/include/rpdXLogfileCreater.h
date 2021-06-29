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
 * $Id: rpdXLogfileCreater.h
 ******************************************************************************/

#ifndef _O_RPD_XLOGFILECREATER_H_
#define _O_RPD_XLOGFILECREATER_H_ 1

#include <idtBaseThread.h>
#include <rpDef.h>
#include <qci.h>

/* xlogfile�� ���� �� ������ ����ϴ� thread */
class rpdXLogfileCreater : public idtBaseThread
{
    public:
        /* thread �⺻ */
        rpdXLogfileCreater() : idtBaseThread() {};
        virtual ~rpdXLogfileCreater() {};
        IDE_RC  initializeThread() { return IDE_SUCCESS; };
        void    finalizeThread() {};
        void    run();

        /* xlogfileMgr */
        class rpdXLogfileMgr *    mXLogfileMgr;

        /* �ʱ�ȭ �� ���� */
        IDE_RC  initialize( SChar *             aReplName,
                            rpXLogLSN           aXLogLSN,
                            rpdXLogfileMgr *    aXLogfileMgr,
                            void *              aReceiverPtr );
        void    finalize();
        void    destroy();

        /* create �۾� ���� */
        IDE_RC  createXLogfiles();
        IDE_RC  createXLogfile( UInt    aFileNo );

        /* ���� ���� ���� */
        IDE_RC  checkAndRemoveOldXLogfiles();
        IDE_RC  removeOldXLogfiles( UInt    aUsingXlogfileNo );
        IDE_RC  removeXLogfile( SChar *     aFilePath );
    private:
        IDE_RC  getOldestXLogfileNo( UInt   aFileNo );
    public:
        UInt    mOldestExistXLogfileNo;
        UInt    mFileCreateCount;

        /* ���� ������ ���� receiver�� ���� */
        void *   mReceiverPtr;

        /* �帧 ���� ���� */
        void    wakeupCreater();
        void    sleepCreater();
        idBool  isCreaterAlive();

        idBool      mExitFlag;
        idBool      mEndFlag;
        idBool      mIsThreadSleep;
        iduMutex    mThreadWaitMutex;
        iduCond     mThreadWaitCV;

        /* prePareFileCnt ���� */
        void    getPrepareFileCnt( UInt *   aResult );
        void    incPrepareFileCnt();
        void    decPrepareFileCnt();

        UInt        mPrepareXLogfileCnt;
        iduMutex    mFileCntMutex;

        /* �� xlogfile�� ������ �ʿ��� ���� */
        SChar *     mXLogDirPath;
        SChar       mRepName[QCI_MAX_NAME_LEN + 1];
        UInt        mLastCreatedFileNo;

        /* ��Ÿ */
        static idBool   isFileExist( SChar * aFilePath );
};

inline idBool rpdXLogfileCreater::isCreaterAlive()
{
    if( mEndFlag == ID_TRUE )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}

#endif // _O_RPD_XLOGFILECREATER_H_
