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
 * $Id: smrArchThread.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMR_ARCH_THREAD_H_
#define _O_SMR_ARCH_THREAD_H_ 1

#include <idu.h>
#include <idtBaseThread.h>
#include <smrArchMultiplexThread.h>

#define SMR_ARCH_THREAD_POOL_SIZE  (50)
#define SMR_ORIGINAL_ARCH_DIR_IDX  (0)

class smrArchThread : public idtBaseThread
{
//For Operation    
public:
    // ��ī�̺��� �α����� ����Ʈ�� �α������� �ϳ� ���� �߰��Ѵ�.
    IDE_RC addArchLogFile(UInt aLogFileNo);
    
    // ��ī�̺��� �α����� ����Ʈ���� �α����� ��带 �ϳ� �����Ѵ�.
    IDE_RC removeArchLogFile(smrArchLogFile *aLogFile);

    // ��ī�̺��� �α����� ����Ʈ�� �ִ� �α����ϵ��� ��ī�̺��Ѵ�.
    // ��ī�̺� �����尡 �ֱ�������,
    // Ȥ�� ��û�� ���� ����� �����ϴ� �Լ��̴�.
    IDE_RC archLogFile();

    // ���������� ��ī�̺�� ���Ϲ�ȣ�� �����´�. 
    IDE_RC setLstArchLogFileNo(UInt    aArchLogFileNo);
    // ���������� ��ī�̺�� ���Ϲ�ȣ�� �����Ѵ�.
    IDE_RC getLstArchLogFileNo(UInt*   aArchLogFileNo);

    // �������� ��ī�̺��� �α����Ϲ�ȣ�� �����´�.
    IDE_RC getArchLFLstInfo(UInt   * aArchFstLFileNo,
                            idBool * aIsEmptyArchLFLst );
    
    // ��ī�̺��� �α����� ����Ʈ�� ��� �ʱ�ȭ �Ѵ�.
    IDE_RC clearArchList();

    virtual void run();
    // ��ī�̺� �����带 ���۽�Ű��, �����尡 ����������
    // ���۵� ������ ��ٸ���.
    IDE_RC startThread();
    
    // ��ī�̺� �����带 �����ϰ�, �����尡 ����������
    // �����Ǿ��� ������ ��ٸ���.
    IDE_RC shutdown();

    // ��ī�̺� ������ ��ü�� �ʱ�ȭ �Ѵ�.
    IDE_RC initialize( const SChar   * aArchivePath,
                       smrLogFileMgr * aLogFileMgr,
                       UInt            aLstArchFileNo);

    // ���� ��ŸƮ�� �ÿ� ��ī�̺��� �α����� ����Ʈ�� �籸���Ѵ�.
    IDE_RC recoverArchiveLogList( UInt aStartNo,
                                  UInt aEndNo );
    
    // ��ī�̺� ������ ��ü�� ���� �Ѵ�.
    IDE_RC destroy();
    
    IDE_RC lockListMtx() { return mMtxArchList.lock( NULL ); }
    IDE_RC unlockListMtx() { return mMtxArchList.unlock(); }

    IDE_RC lockThreadMtx() { return mMtxArchThread.lock( NULL ); }
    IDE_RC unlockThreadMtx() { return mMtxArchThread.unlock(); }
    
    // ��ī�̺� �����带 ������
    // ���� ��ī�̺� ����� �α����ϵ��� ��ī�̺� ��Ų��.
    IDE_RC wait4EndArchLF( UInt aToFileNo );

    smrArchThread();
    virtual ~smrArchThread();

//For Member
private:
    // ��ī�̺� �αװ� ����� ���丮
    // Log File Group�� �ϳ��� unique�� ��ī�̺� ���丮�� �ʿ��ϴ�.
    const SChar            * mArchivePath[SM_ARCH_MULTIPLEX_PATH_CNT + 1];
    // �� ��ī�̺� �����尡 ��ī�̺��� �α����ϵ��� �����ϴ� �α����� ������
    smrLogFileMgr          * mLogFileMgr;
    
    // ���� �ִ� ��ī�̺� �����带 ��������� condition value
    iduCond                  mCv;

    // smrArchLogFile�� �Ҵ�/������ ����� mempool
    iduMemPool               mMemPool;

    // ��ī�̺� �����带 �����ؾ� �� ���� ����
    idBool                   mFinish;
    // ��ī�̺� �����尡 ���������� ����
    idBool                   mResume;

    // mLstArchFileNo �� mArchFileList �� ���� ���ü� ��� ���� Mutex
    iduMutex                 mMtxArchList;
    // ��ī�̺� �������� ���ü� ��� ���� Mutex
    iduMutex                 mMtxArchThread;

    // ������ ��ī�̺�� �α����Ϲ�ȣ
    UInt                     mLstArchFileNo;
    // ��ī�̺��� �α����� ��ȣ���� ���� ����Ʈ
    smrArchLogFile           mArchFileList;

    UInt                     mArchivePathCnt;

    smrArchMultiplexThread * mArchMultiplexThreadArea;
};

#endif
