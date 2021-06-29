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
 * $Id: smrUTransQueue.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_SMR_UTRANS_QUEUE_H_
#define _O_SMR_UTRANS_QUEUE_H_ 1

#include <smDef.h>
#include <iduPriorityQueue.h>
#include <smrLogFile.h>

typedef struct smrUndoTransInfo
{
    void        *mTrans;
    /* mTrans�� mLstUndoNxtLSN�� ����Ű�� Log�� LogHead */
    smrLogHead   mLogHead;
    /* mTrans�� mLstUndoNxtLSN�� ����Ű�� Log�� LogBuffer Ptr */
    SChar*       mLogPtr;
    /* mTrans�� mLstUndoNxtLSN�� ����Ű�� Log�� �ִ� Logfile Ptr */
    smrLogFile*  mLogFilePtr;
    /* mTrans�� mLstUndoNxtLSN�� ����Ű�� Log�� Valid�ϸ� ID_TRUE, �ƴϸ� ID_FALSE */
    idBool       mIsValid;
         
    /* �α����Ϸ� ���� ���� �α��� ũ��
       ����� �α��� ��� �α��� ũ��� �α����ϻ��� �α�ũ�Ⱑ �ٸ���
     */
    UInt         mLogSizeAtDisk;
} smrUndoTransInfo;

class smrUTransQueue
{
public:

    IDE_RC initialize(SInt aTransCount);
    IDE_RC destroy();

    IDE_RC insert(smrUndoTransInfo*);
    smrUndoTransInfo* remove();

    static SInt compare(const void *arg1,const  void *arg2);
    IDE_RC insertActiveTrans(void* aTrans);
    
        
    SInt getItemCount() {return mPQueueTransInfo.getDataCnt();}

    smrUTransQueue();
    virtual ~smrUTransQueue();

private:
    /*���� Undo�� Transactino�� ���� */
    UInt              mCurUndoTransCount;
    /*Undo Transaction�� �ִ� ���� */
    UInt              mMaxTransCount;

    /*smrUndoTransInfo Array */
    smrUndoTransInfo *mArrUndoTransInfo;

    /*smrUndoTransInfo�� mLoghead�� mSN�� ū ������
      Sorting�ؼ� �����ϴ� Sort Array */
    iduPriorityQueue      mPQueueTransInfo;
};

#endif /* _O_SMR_UTRANS_QUEUE_H_ */
