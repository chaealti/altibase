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
 * $Id:
 **********************************************************************/

#ifndef _O_SMI_READ_LOG_BYORDER_H_
#define _O_SMI_READ_LOG_BYORDER_H_ 1

#include <smDef.h>
#include <smrLogFile.h>
#include <smrPreReadLFileThread.h>
#include <iduReusedMemoryHandle.h>
#include <smrRemoteLogMgr.h>

/* Parallel Logging : Log�� ����
   ������� ������ �ϴ� Replicaiton�� ���� �����Ǿ���.
   ���� ������ �ϴ� Log�� ��ġ�� ������
   �ְ�, ������ ���� ����Ű�� �α��� LSN�� ���� ���� Log��
   �о ������.
   .*/
typedef struct smiReadInfo
{
    /* Read�� Log LSN */
    smLSN       mReadLSN;
    /* mReadLSN�� ����Ű�� Log�� LogHead */
    smrLogHead  mLogHead;
    /* mReadLSN�� ����Ű�� Log�� LogBuffer Ptr - ����� �α�*/
    SChar     * mLogPtr;
    /* mReadLSN�� ����Ű�� Log�� �ִ� logfile Ptr */
    smrLogFile* mLogFilePtr;
    /* mReadLSN�� ����Ű�� Log�� Valid�ϸ� ID_TRUE, �ƴϸ� ID_FALSE */
    idBool      mIsValid;

    /* ���������� Read�� Valid �α��� LSN���� Return�Ѵ�.*/
    smLSN        mLstLogLSN;

    /* Log Switch*/
    idBool      mIsLogSwitch;

    /* �α��� �������� ���� �ڵ�
     * 
     * ����� �α��� ���, �� �ڵ��� ���ϴ� �޸𸮿� ������ �����ϰ�
     * �α׷��ڵ��� �����Ͱ� �� �޸𸮸� ���� ����Ű�� �ȴ�.
     * ����, smiReadInfo�ϳ��� ���� ���� ���۸� ����� �Ѵ�. */
    iduReusedMemoryHandle mDecompBufferHandle;

    /* ���� �α��� ũ��
     * 
     * ����� �α��� ��� �α��� ũ���
     * ���ϻ��� ũ�Ⱑ �ٸ� �� �ִ�.
     * ���� ���� �α��� ��ġ�� ����Ҷ� �� ���� ����Ͽ��� �Ѵ�. */
    UInt mLogSizeAtDisk;
} smiReadInfo;

class smiReadLogByOrder
{
public:
    /* �ʱ�ȭ ���� */
    IDE_RC initialize( smSN       aInitSN,
                       UInt       aPreOpenFileCnt,
                       idBool     aIsRemoteLog,
                       ULong      aLogFileSize, 
                       SChar   ** aLogDirPath,
                       idBool     aNotDecomp );  /*����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)*/

    /* �Ҵ�� Resource�� �����Ѵ�.*/
    IDE_RC destroy();

    /* mSortRedoInfo�� ����ִ� smrRedoInfo�߿���
     * ���� ���� mLSN���� ���� Log�� �о���δ�. */
    IDE_RC readLog( smSN       * aInitSN,
                    smLSN      * aLSN,
                    void      ** aLogHeadPtr,
                    void      ** aLogPtr,
                    idBool     * aIsValid );

    idBool isCompressedLog( SChar * aRawLog );

    smOID  getTableOID( SChar * aRawLog );

    IDE_RC decompressLog( SChar      * aCompLog,
                          smLSN      * aReadLSN,
                          smiLogHdr  * aLogHead,
                          SChar     ** aLogPtr );

    /* ��� LogFile�� �����ؼ� aMinLSN���� ���� LSN��
       ������ �α׸� ù��°�� ������ LogFile No�� ���ؼ� ù��°�� �����α׷�
       Setting�Ѵ�.*/
    IDE_RC setFirstReadLogFile( smLSN aInitLSN );
    /* setFirstReadLogFile���� ã�� ���Ͽ� ���ؼ� ������ ù��°�� �о����
       �α��� ��ġ�� ã�´�. �� aInitLSN���� ũ�ų� ���� LSN���� ���� �α���
       ���� ���� LSN���� ���� log�� ��ġ�� ã�´�.*/
    IDE_RC setFirstReadLogPos( smLSN aInitLSN );

    /* valid�� �α׸� �о���δ�.*/
    IDE_RC searchValidLog( idBool *aIsValid );
    
    /* PROJ-1915 
      * �������� �α׿��� ������ ���� ��ϵ� LSN�� ��´�. 
      */
    IDE_RC getRemoteLastUsedGSN( smSN * aSN );

    /*
      ���� �а� �ִ� ���������� ��� �αװ�
      Sync�� �Ǿ����� Check�Ѵ�.
    */
    IDE_RC isAllReadLogSynced( idBool *aIsSynced );
  
    IDE_RC stop();
    IDE_RC startByLSN(smLSN aLstReadLSN);
    /*
      Pre Fetch�� �α����� ������ �������Ѵ� 
    */ 
    void    setPreReadFileCnt(UInt aFileCnt)   { mPreReadFileCnt = aFileCnt; }

    /*
      Read ������ �����Ѵ�.
    */
    void   getReadLSN( smLSN *aReadLSN );

    smiReadLogByOrder(){};
    virtual ~smiReadLogByOrder(){};

    iduMemoryHandle * getDecompBufferHandle() { return &( mReadInfo.mDecompBufferHandle ); }
    UInt              getDecompOffset() { return mReadInfo.mReadLSN.mOffset; }
    UInt              getFileNo() { return mReadInfo.mReadLSN.mFileNo; } 

private:
    static SInt   compare( const void *arg1,  const void *arg2 );

    // Read Info�� �ʱ�ȭ �Ѵ�.
    IDE_RC initializeReadInfo( smiReadInfo * aReadInfo );

    // Read Info�� �ı��Ѵ�.
    IDE_RC destroyReadInfo( smiReadInfo * aReadInfo );
    
    
private:
    /* Read������ ������ �ִ�. */
    smiReadInfo  mReadInfo;
    /* smrRedoInfo�� mLogHead�� mLSN�� �������� smrRedoInfo��
       Sorting�ؼ� ������ �ִ�.*/
    iduPriorityQueue mPQueueRedoInfo;
    /* ���� Read�� �������� smiReadInfo */
    smiReadInfo* mCurReadInfoPtr;
    /* �α������� �̸� �д� Thread�̴�. */
    smrPreReadLFileThread mPreReadLFThread;
    /* �α������� �̸� ���� �����̴�. */
    UInt         mPreReadFileCnt;
    /* ���������� �о��� �α��� Sequence Number */
    smLSN        mLstReadLogLSN;

    /* PROJ-1915 off-line �α� ���� Local �α����� */
    idBool       mIsRemoteLog;
    /* PROJ-1915 ����Ʈ �α� �޴��� */

    smrRemoteLogMgr mRemoteLogMgr;
    /* BUG-46944 ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)*/
    idBool       mNotDecomp;
};

#endif /* _O_SMR_READ_LOG_BYORDER_H_ */
