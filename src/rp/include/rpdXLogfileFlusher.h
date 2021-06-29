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
 * $Id: rpdXLogfileFlusher.h
 ******************************************************************************/

#ifndef _O_RPD_XLOGFILEFLUSHER_H_
#define _O_RPD_XLOGFILEFLUSHER_H_ 1

#include <idtBaseThread.h>
#include <rpDef.h>

/* flush�� �����ϴ� thread */
class rpdXLogfileFlusher : public idtBaseThread
{
    public:
        /* thread �⺻ */
        rpdXLogfileFlusher() : idtBaseThread() {};
        virtual ~rpdXLogfileFlusher() {};
        IDE_RC  initializeThread() { return IDE_SUCCESS; };
        void    finalizeThread() {};
        void    run();

        /* xlogfileMgr */
        class rpdXLogfileMgr *  mXLogfileMgr;

        /* �ʱ�ȭ �� ���� */
        IDE_RC  initialize( SChar *             aReplName,
                            rpdXLogfileMgr *    mXLogfileMgr );
        void    finalize();
        void    destroy();

        /* flush �۾� ���� */
        IDE_RC      flushXLogfiles();

        /* �帧 ���� ���� */
        void    wakeupFlusher();
        void    sleepFlusher();
        idBool  isFlusherAlive();

        idBool      mIsThreadSleep;
        iduMutex    mThreadWaitMutex;
        iduCond     mThreadWaitCV;
        idBool      mExitFlag;
        idBool      mEndFlag;

        /* flush list ���� */
        iduListNode *   getFileFromFlushList();
        IDE_RC          addFileToFlushList( void * aXLogfile );
        idBool          checkListEmpty();

        iduList     mFlushList;
        iduMutex    mFlushMutex;
};

inline idBool rpdXLogfileFlusher::isFlusherAlive()
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

#endif // _O_RPD_XLOGFILEFLUSHER_H_

