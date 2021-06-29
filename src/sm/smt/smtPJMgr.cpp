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
 * $Id: smtPJMgr.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idtBaseThread.h>
#include <smErrorCode.h>
#include <smtPJChild.h>
#include <smtPJMgr.h>
#include <smDef.h>

smtPJMgr::smtPJMgr() : idtBaseThread()
{


}

IDE_RC smtPJMgr::initialize(SInt         aChildCount,
                            smtPJChild **aChildArray,
                            idBool      *aSuccess,
                            idBool       aIgnoreError)
{
    SInt i;
    
    IDE_ASSERT(aSuccess != NULL);
    
    mLocalSuccess  = ID_TRUE;
    mGlobalSuccess = aSuccess;
    
    mChildCount   = aChildCount;
    mChildArray   = aChildArray;
    mIgnoreError  = aIgnoreError;
    mErrorNo      = 0;
    
    IDE_TEST(mMutex.initialize((SChar*)"PJ_MGR_MUTEX",
                               IDU_MUTEX_KIND_POSIX,
                               IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);

    for (i = 0; i < aChildCount; i++) // client �ʱ�ȭ
    {
        IDE_TEST(aChildArray[i]->initialize(this,
                                            i) != IDE_SUCCESS);
    }
    
    mLocalSuccess = ID_TRUE;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    mErrorNo = ideGetErrorCode();
    
    mLocalSuccess = ID_FALSE;
    *mGlobalSuccess = ID_FALSE;
    
    return IDE_FAILURE;

}

IDE_RC smtPJMgr::destroy()
{
        
    IDE_TEST(mMutex.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

void smtPJMgr::run()
{
    SInt   i = 0;
    SInt   sDeadSum;
    idBool sChildError = ID_FALSE;
    SInt   sChildCount = mChildCount;
    
    IDE_TEST( lock() != IDE_SUCCESS);

    for (i = 0; i <  mChildCount; i++)
    {
        /* BUG-40933 thread �Ѱ��Ȳ���� FATAL���� ���� �ʵ��� ����
         * smtPJMgr�� ��쿡�� mChildCount�� ���� ������ smtPJChild::run���� �����Ƿ�
         * ������ thread������ �����ϴ� ���� �����ϴ�. */
        if( mChildArray[i]->start() != IDE_SUCCESS )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_THREAD,
                         "smtPJMgr started less child threads %"ID_INT32_FMT" than requested %"ID_INT32_FMT"",
                         i,
                         mChildCount );

            sChildCount = i;
            break;
        }
    }
    
    IDE_TEST(unlock() != IDE_SUCCESS);

    /* BUG-40933 thread �Ѱ��Ȳ���� FATAL���� ���� �ʵ��� ����
     * thread�� �ϳ��� �������� ���� ��쿡�� ABORT�ϵ��� �Ѵ�. */
    IDE_TEST(sChildCount == 0);

    
    /* ------------------------------------------------
     * ��� �۾��� ������ ���� ���
     * ----------------------------------------------*/
        
    while(ID_TRUE)
    {
        sDeadSum  = 0;
        for (i = 0; i < sChildCount; i++)
        {
            if ((mChildArray[i]->getStatus() & SMT_PJ_SIGNAL_EXIT)
                == SMT_PJ_SIGNAL_EXIT)
            {
                sDeadSum++;
            }
        }
        if (sDeadSum == sChildCount) // ��ΰ� ������ ������
        {
            break;
        }
        PDL_Time_Value sPDL_Time_Value;
        sPDL_Time_Value.initialize(1);
        idlOS::sleep(sPDL_Time_Value);
    }

    mLocalSuccess = ID_TRUE;

    sChildError = ID_TRUE;
    
    for (i = 0; i < sChildCount; i++)
    {
        IDE_TEST((mChildArray[i]->getStatus() & SMT_PJ_SIGNAL_ERROR)
                 == SMT_PJ_SIGNAL_ERROR);
    }
    
    sChildError = ID_FALSE;
    return;

    IDE_EXCEPTION_END;
    {
        if(mIgnoreError == ID_FALSE)
        {
            /* BUG-40933 thread �Ѱ��Ȳ���� FATAL���� ���� �ʵ��� ����
             * thread�� �ϳ��� �������� ���� ��쿡�� FATAL�� �ƴ� ABORT������ ������ �Ѵ�. */
            if(sChildCount != 0)
            {
                IDE_CALLBACK_FATAL("error");
            }
        }
    }

    if(sChildError == ID_TRUE)
    {
        mErrorNo = mChildArray[i]->getErrorNo();
    }

    if(mErrorNo == 0)
    {
        mErrorNo = ideGetErrorCode();
    }

    mLocalSuccess = ID_FALSE;
    *mGlobalSuccess = ID_FALSE;

}

