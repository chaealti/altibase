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
 * PROJ-1915 : Off-line Replicator
 * Off-line ������ ���� LFGMgr ���� �α� �б� ��� ���� ���� �Ѵ�.
 *
 **********************************************************************/

#ifndef _O_SMR_REMOTE_LFG_MGR_H_
#define _O_SMR_REMOTE_LFG_MGR_H_ 1

#include <idu.h>
#include <iduMemoryHandle.h>
#include <smDef.h>
#include <smrDef.h>

typedef struct smrRemoteLogMgrInfo{
    /* ù��° �α� ���� ��ȣ */
    UInt    mFstFileNo;
    /* ������ �α� ���� ��ȣ */
    UInt    mEndFileNo;
    /* ������ ��ϵ� �α� LstLSN*/
    smLSN    mLstLSN;
}smrRemoteLogMgrInfo;

class smrRemoteLogMgr
{
public :
    smrRemoteLogMgr();
    ~smrRemoteLogMgr();

    /*
     * aLogFileSize - off-line Log File Size
     * aLogDirPath  - LogDirPath array
     * aNotDecomp   - ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)
     */
    IDE_RC initialize( ULong    aLogFileSize,
                       SChar ** aLogDirPath,
                       idBool   aNotDecomp );

    /* �α� �׷� ������ ���� */
    IDE_RC destroy();

    /* ������ LSN���� ���Ѵ�. */
    void getLstLSN( smLSN * aLstLSN );

    /* Ư�� �α������� ù��° �α׷��ڵ��� Head�� File�κ��� ���� �д´� */
    IDE_RC readFirstLogHeadFromDisk( smLSN      * aLSN,
                                     smrLogHead * aLogHead,
                                     idBool     * aIsValid );

    /* aLogFile�� Close�Ѵ�. */
    IDE_RC closeLogFile(smrLogFile * aLogFile);

    /* aLSN�� ����Ű�� �α������� ù��° Log �� Head�� �д´� */
    IDE_RC readFirstLogHead( smLSN      * aLSN,
                             smrLogHead * aLogHead,
                             idBool     * aIsValid );

    /*
      aFirstFileNo���� aEndFileNo������
      ��� LogFile�� �����ؼ� aMinSN���� ���� SN�� ������ �α׸�
      ù��°�� ������ LogFile No�� ���ؼ� aNeedFirstFileNo�� �־��ش�.
    */
    IDE_RC getFirstNeedLFN( smLSN        aMinLSN,
                            const UInt   aFirstFileNo,
                            const UInt   aEndFileNo,
                            UInt       * aNeedFirstFileNo );

    /* Ư�� LSN�� log record�� �ش� log record�� ���� �α� ������ �����Ѵ�. */
    IDE_RC readLog( iduMemoryHandle * aDecompBufferHandle,
                    smLSN           * aLSN,
                    idBool            aIsCloseLogFile,
                    smrLogFile     ** aLogFile,
                    smrLogHead      * aLogHead,
                    SChar          ** aLogPtr,
                    UInt            * aLogSizeAtDisk );

    /* readLog�� Valid �˻� ���� �Ѵ�.  */
    IDE_RC readLogAndValid( iduMemoryHandle * aDecompBufferHandle,
                            smLSN           * aLSN,
                            idBool            aIsCloseLogFile,
                            smrLogFile     ** aLogFile,
                            smrLogHead      * aLogHeadPtr,
                            SChar          ** aLogPtr,
                            idBool          * aIsValid,
                            UInt            * aLogSizeAtDisk );

    /*
       aFileNo�� �ش��ϴ� Logfile�� open����
       aLogFilePtr�� open�� logfile�� pointer�� �Ѱ��ش�..
     */
    IDE_RC openLogFile( UInt          aFileNo,
                        idBool        aIsWrite,
                        smrLogFile ** aLogFilePtr );

    /* Check LogDir Exist */
    IDE_RC checkLogDirExist(void);

    /* aIndex�� �ش��ϴ� �α� ��θ� ���� �Ѵ�. */
    SChar* getLogDirPath();

    /* aIndex�� �α� ��θ� ���� �Ѵ�. */
    void   setLogDirPath(SChar * aDirPath);

    /* �α� ���� ����� ���� �Ѵ�. */
    ULong  getLogFileSize(void);

    /* �α� ���� ����� ���� �Ѵ�. */
    void   setLogFileSize(ULong aLogFileSize);

    /* �α� ���� ���� ���� �˻� */
    IDE_RC isLogFileExist(UInt aFileNo, idBool * aIsExist);

    /* mRemoteLogMgrs ������ ä���. */
    IDE_RC setRemoteLogMgrsInfo();

    /* �ּ� ���� ��ȣ, �ִ� ���� ��ȣ */
    IDE_RC setFstFileNoAndEndFileNo(UInt * aFstFileNo,
                                    UInt * aEndFileNo);

    /* ��� �α� ���� ��ȣ���� ���� ���� ��ȣ�� ���� �Ѵ�. */
    void   getFirstFileNo(UInt * aFileNo);

    /* logfileXXX���� XXX�� ��ȣ�� ��ȯ �Ѵ�. */
    UInt   chkLogFileAndGetFileNo(SChar  * aFileName,
                                  idBool * aIsLogFile);

private :

    /* smrLogFile ��ü �Ҵ��� ���� memory pool */
    iduMemPool mMemPool;

    /* �α� ���� ������ */
    ULong      mLogFileSize;

    /* �α� ���� ��� */
    SChar    * mLogDirPath;

    /* �α����ϸŴ��� */
    smrRemoteLogMgrInfo mRemoteLogMgrs;

    /* BUG-46944 ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)*/
    idBool mNotDecomp;
};
#endif /* _O_SMR_REMOTE_LFG_MGR_H_ */

