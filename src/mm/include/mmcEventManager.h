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

#ifndef _O_MMC_EVENT_MANAGER_H_
#define _O_MMC_EVENT_MANAGER_H_ 1

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <cm.h>
#include <smiTrans.h>
#include <qci.h>
#include <mmcConvNumeric.h>
#include <mmcDef.h>
#include <mmcLob.h>
#include <mmcTrans.h>
#include <mmcStatement.h>
#include <mmcStatementManager.h>
#include <mmqQueueInfo.h>
#include <mmtSessionManager.h>

typedef struct mmcEventInfo
{
    SChar * mName;
    SChar * mMessage;
} mmcEventInfo;

class mmcEventManager
{
public:
    mmcEventManager() {};
    ~mmcEventManager() {};

    // �ʱ�ȭ �۾��� �����Ѵ�.
    IDE_RC         initialize( mmcSession * aSession );

    // ������ �۾��� �����Ѵ�.
    IDE_RC         finalize();

    // �̺�Ʈ ��� �ؽ��� �̺�Ʈ�� ����Ѵ�.
    IDE_RC         regist( const SChar * aName,
                           UShort        aNameSize );

    // �̺�Ʈ ��� �ؽ��� �־��� �̸��� ��ϵǾ� �ִ��� Ȯ���Ѵ�.
    idBool         isRegistered( const SChar * aName,
                                 UShort        aNameSize );

    // �̺�Ʈ ��� �ؽ����� �־��� �̸��� �̺�Ʈ�� �����Ѵ�.
    IDE_RC         remove(  const SChar * aName,
                            UShort        aNameSize );

    // �̺�Ʈ ��� �ؽ��� �ʱ�ȭ �Ѵ�.
    IDE_RC         removeall();

    // polling interval�� ���� �����Ѵ�.
    IDE_RC         setDefaults( SInt aPollingInterval );

    // �̺�Ʈ pending ����Ʈ�� �̺�Ʈ�� �߰��Ѵ�.
    IDE_RC         signal( const SChar * aName,
                           UShort        aNameSize,
                           const SChar * aMessage,
                           UShort        aMessageSize );

    mmcEventInfo * isSignaled( const SChar * aName,
                               UShort        aNameSize );

    // ������ �̺�Ʈ�� ��ٸ���.
    IDE_RC         waitany( idvSQL   * aStatistics,
                            SChar    * aName,
                            UShort   * aNameSize,
                            SChar    * aMessage,
                            UShort   * aMessageSize,
                            SInt     * aStatus,
                            const SInt aTimeout );

    // �־��� �̸��� �̺�Ʈ�� ��ٸ���.
    IDE_RC         waitone( idvSQL       * aStatistics,
                            const SChar  * aName,
                            const UShort * aNameSize,
                            SChar        * aMessage,
                            UShort       * aMessageSize,
                            SInt         * aStatus,
                            const SInt     aTimeout );

    IDE_RC         commit();

    IDE_RC         rollback( SChar * aSvpName );

    IDE_RC         savepoint( SChar * aSvpName );

    IDE_RC         apply( mmcSession * aSession );

    inline iduList * getPendingList();

    inline IDE_RC  lock();

    inline IDE_RC  unlock();

    inline IDE_RC  lockForCV();

    inline IDE_RC  unlockForCV();

private:
    // �̺�Ʈ pending ����Ʈ
    iduList        mPendingList;

    // �̺�Ʈ ��� ����Ʈ
    // BUGBUG 
    // hash�� ����ϵ��� ���� �ؾ���.
    iduList        mList;

    // �̺�Ʈ ť
    iduList        mQueue;

    // Polling Interval
    SInt           mPollingInterval;

    // alloc/free�� �����ο� �޸� ������
    iduVarMemList  mMemory;

    // ���� ����
    mmcSession   * mSession;

    // cond_timedwait ȣ��ÿ� ���
    iduCond        mCV;

    PDL_Time_Value mTV;

    iduMutex       mMutex;

    // ���� �ڷᱸ���� ���� ���ü� ����ÿ� ���
    iduMutex       mSync;

    // ��ϵ� �̺�Ʈ�� ����
    SInt           mSize;

    // pending�� �̺�Ʈ�� ����
    SInt           mPendingSize;

private:
    // pending ����Ʈ���� savepoint ������ �����Ѵ�.
    IDE_RC removeSvp( SChar * aSvpName );

    IDE_RC freeNode( iduListNode * aNode );

    void   dump();
};

inline iduList * mmcEventManager::getPendingList()
{
    return &mPendingList;
}

inline IDE_RC mmcEventManager::lock()
{
    return mSync.lock( NULL );
}

inline IDE_RC mmcEventManager::unlock()
{
    return mSync.unlock();
}

inline IDE_RC mmcEventManager::lockForCV()
{
    return mMutex.lock( NULL );
}

inline IDE_RC mmcEventManager::unlockForCV()
{
    return mMutex.unlock();
}

#endif
