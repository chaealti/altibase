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
 * $Id: smrFT.h 32652 2009-05-13 02:59:22Z bskim $
 *
 * Description :
 *
 * Backup ���� Dump
 *
 * X$ARCHIVE
 * X$STABLE_MEM_DATAFILES
 * X$LFG
 * X$LOG
 *
 **********************************************************************/

#ifndef _O_SMR_FT_H_
#define _O_SMR_FT_H_ 1

#include <idu.h>
#include <idp.h>
#include <smrDef.h>
#include <sctTableSpaceMgr.h>

// fixed table related def.
typedef struct smrArchiveInfo
{
    smiArchiveMode mArchiveMode;
    idBool         mArchThrRunning;
    const SChar*   mArchDest;
    UInt           mNextLogFile2Archive;
    /* Archive Directory���� ���� ���� Logfile No */
    UInt           mOldestActiveLogFile;
    /* ���� ������� Logfile No */
    UInt           mCurrentLogFile;
} smrArchiveInfo;

//added for performance view
typedef struct smrStableMemDataFile
{
    UInt           mSpaceID;
    const SChar*   mDataFileName;
    smuList        mDataFileNameLst;
}smrStableMemDataFile;

//added for LFG Fixed Table
typedef struct smrLFGInfo
{
    // ���� log�� ����ϱ� ���� ����ϴ� logfile No
    UInt          mCurWriteLFNo;

    // ���� logfile���� ���� log�� ��ϵ� ��ġ
    UInt          mCurOffset;

    // ���� Open�� LogFile�� ����
    UInt          mLFOpenCnt;

    // Log Prepare Count
    UInt          mLFPrepareCnt;

    // log switch �߻��� wait event�� �߻��� Ƚ��
    UInt          mLFPrepareWaitCnt;

    // ���������� prepare�� logfile No
    UInt          mLstPrepareLFNo;

    smLSN         mEndLSN;
    UInt          mFstDeleteFileNo;
    UInt          mLstDeleteFileNo;
    smLSN         mResetLSN;

    // Update Transaction�� ��
    UInt          mUpdateTxCount;

    // Group Commit ���ġ
    // Commit�Ϸ��� Ʈ����ǵ��� Log�� Flush�Ǳ⸦ ��ٸ� Ƚ��
    UInt          mGCWaitCount;

    // Commit�Ϸ��� Ʈ����ǵ��� Flush�Ϸ��� Log�� ��ġ��
    // �̹� Log�� Flush�� ������ �Ǹ�Ǿ� �������� Ƚ��
    UInt          mGCAlreadySyncCount;

    // Commit�Ϸ��� Ʈ����ǵ��� ������ Log �� Flush�� Ƚ��
    UInt          mGCRealSyncCount;
} smrLFGInfo;

class smrFT
{
public:
    // X$ARCHIVE
    static IDE_RC buildRecordForArchiveInfo(idvSQL              * /*aStatistics*/,
                                            void        *aHeader,
                                            void        *aDumpObj,
                                            iduFixedTableMemory *aMemory);

    // X$STABLE_MEM_DATAFILES
    static IDE_RC buildRecordForStableMemDataFiles(idvSQL              * /*aStatistics*/,
                                                   void        *aHeader,
                                                   void        *aDumpObj,
                                                   iduFixedTableMemory *aMemory);

    // X$LFG
    static IDE_RC buildRecordOfLFGForFixedTable(idvSQL              * /*aStatistics*/,
                                                void*  aHeader,
                                                void*  aDumpObj,
                                                iduFixedTableMemory *aMemory);

    // X$LOG
    static IDE_RC buildRecordForLogAnchor(idvSQL              * /*aStatistics*/,
                                          void*  aHeader,
                                          void*  aDumpObj,
                                          iduFixedTableMemory *aMemory);

    // X$RECOVERY_FAIL_OBJ
    static IDE_RC buildRecordForRecvFailObj(idvSQL              * /*aStatistics*/,
                                            void*  aHeader,
                                            void*  /* aDumpObj */,
                                            iduFixedTableMemory *aMemory);
};



#endif /* _O_SMR_FT_H_ */

