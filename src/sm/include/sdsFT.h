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
 * $Id$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer
 **********************************************************************/

#ifndef _O_SDS_FT_H_
#define _O_SDS_FT_H_ 1

#include <smDef.h>

/***********************************************
 *  BCB�� ���� fixed table ����
 ***********************************************/
typedef struct sdsBCBStat
{
    UInt  mSBCBID;
    UInt  mSpaceID;
    UInt  mPageID;
    UInt  mHashBucketNo;
    SInt  mCPListNo;
    UInt  mRecvLSNFileNo;
    UInt  mRecvLSNOffset;
    UInt  mState;
    UInt  mBCBMutexLocked;
    UInt  mPageLSNFileNo;
    UInt  mPageLSNOffset;
} sdsBCBStat;

typedef struct sdsBCBStatArg
{
    void                *mHeader;
    iduFixedTableMemory *mMemory;
} sdsBCBStatArg;

/***********************************************************************
 *  Flush Mgr ���� fixed table ����
 ***********************************************************************/
typedef struct sdsFlushMgrStat
{
    /* flusher�� ����*/
    UInt    mFluserCount;
    /* checkpoint list count */
    UInt    mCPList;
    /* ���� ��û�� job ���� */
    UInt    mReqJobCount;
    /* flush ����� ������ �� */
    UInt    mReplacementFlushPages;
    /* checkpoint flush ��� pages �� */
    ULong   mCheckpointFlushPages;
    /* checkpoint recoveryLSN�� ���� ���� SBCB.BCBID */
    UInt    mMinBCBID;
    /* checkpoint recoveryLSN�� ���� ���� SBCB.SpaceID */
    UInt    mMinBCBSpaceID;
    /* checkpoint recoveryLSN�� ���� ���� SBCB.PageID */
    UInt    mMinBCBPageID;
} sdsFlushMgrStat;

/***********************************************************************
 *  File node ���� fixed table ����
 ***********************************************************************/
typedef struct sdsFileNodeStat
{
    SChar*    mName;

    smLSN     mRedoLSN;
    smLSN     mCreateLSN;

    UInt      mSmVersion;

    ULong     mFilePages;
    idBool    mIsOpened;
    UInt      mState;
} sdsFileNodeStat;

class sdsFT
{
public:

private:

public:

private:

};

#endif //_O_SDS_FT_H_
