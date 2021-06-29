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
 * $Id: smrPreReadLFileThread.cpp 82426 2018-03-09 05:12:27Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smDef.h>
#include <smr.h>

#define SMR_PREREADLF_INFO_POOL_ELEMENT_COUNT (32)

smrPreReadLFileThread::smrPreReadLFileThread() : idtBaseThread()
{
}

smrPreReadLFileThread::~smrPreReadLFileThread()
{
}

/***********************************************************************
 * Description : �ʱ�ȭ�� �����Ѵ�.
 *
 */
IDE_RC smrPreReadLFileThread::initialize()
{
    mFinish = ID_FALSE;
    mResume = ID_FALSE;
    
    IDE_TEST_RAISE( mCV.initialize((SChar *)"PreReadLFileThread Cond")
                    != IDE_SUCCESS, err_cond_var_init);
    
    IDE_TEST( mMutex.initialize( (SChar*) "PreReadLFileThread Mutex",
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( mCondMutex.initialize( (SChar*) "PreReadLFileThread Cond Mutex",
                                     IDU_MUTEX_KIND_POSIX,
                                     IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    
    /* Open LogFile Request List�� �ʱ�ȭ�Ѵ�.*/
    mOpenLFRequestList.mPrev = &mOpenLFRequestList;
    mOpenLFRequestList.mNext = &mOpenLFRequestList;
    /* Open LogFile List�� �ʱ�ȭ�Ѵ�.*/
    mOpenLFList.mPrev = &mOpenLFList;
    mOpenLFList.mNext = &mOpenLFList;

    IDE_TEST( mPreReadLFInfoPool.initialize( IDU_MEM_SM_SMR,
                                             0,
                                             (SChar*) "SMR_PREREADLF_INFO_POOL",
                                             ID_SIZEOF(smrPreReadLFInfo),
                                             SMR_PREREADLF_INFO_POOL_ELEMENT_COUNT,
                                             10, /* 10���̻�� Free */
                                             ID_TRUE )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_init);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : open�� Logfile�� close�ϰ� �Ҵ�� Resource�� ��ȯ�Ѵ�.
 *
 */
IDE_RC smrPreReadLFileThread::destroy()
{
    smrPreReadLFInfo *sCurPreReadInfo;
    smrPreReadLFInfo *sNxtPreReadInfo;

    /* open logfile request list�� ��� request�� ����ϰ�
       �����Ѵ�.*/
    sCurPreReadInfo = mOpenLFRequestList.mNext;
    
    while(sCurPreReadInfo != &mOpenLFRequestList)
    {
        sNxtPreReadInfo = sCurPreReadInfo->mNext;
        removeFromLFRequestList(sCurPreReadInfo);
        
        IDE_TEST( mPreReadLFInfoPool.memfree( (void*)sCurPreReadInfo )
                  != IDE_SUCCESS );
        
        sCurPreReadInfo = sNxtPreReadInfo;
        
    }

    /* open Log file list���� file�� open�Ǿ� ������ close�ϰ�
       �Ҵ�� �޸𸮸� �ݳ��Ѵ�.*/
    sCurPreReadInfo = mOpenLFList.mNext;

    // ���� �ִ� ��� LogFile�� Close�Ѵ�.
    while ( sCurPreReadInfo != &mOpenLFList )
    {
        sNxtPreReadInfo = sCurPreReadInfo->mNext;
        removeFromLFList(sCurPreReadInfo);
        if ( ( sCurPreReadInfo->mFlag & SMR_PRE_READ_FILE_MASK ) 
             == SMR_PRE_READ_FILE_OPEN )
        {
            /* mFlag���� SMR_PRE_READ_FILE_OPEN�� �Ǿ� ������ logfile��
               Open�Ǿ� �ִ°��̴�.*/
            IDE_TEST( smrLogMgr::closeLogFile(sCurPreReadInfo->mLogFilePtr)
                      != IDE_SUCCESS );
        }

        IDE_TEST( mPreReadLFInfoPool.memfree((void*)sCurPreReadInfo)
                  != IDE_SUCCESS );
        
        sCurPreReadInfo = sNxtPreReadInfo;
    }
    
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );
    IDE_TEST( mCondMutex.destroy() != IDE_SUCCESS );

    IDE_TEST( mPreReadLFInfoPool.destroy() != IDE_SUCCESS );

    IDE_TEST_RAISE(mCV.destroy() != IDE_SUCCESS, err_cond_destroy);

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_destroy ); 
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : open logfile request�� ��� ���� �ִٰ� request�� ������
 *               �ϴ� request�� �ڽ� open logfile list�� �ű�� open�� �����Ѵ�.
 *               �̶� logfile open�� �޸𸮿� ��� logfile�� �о���̴� ���̴�.
 *               �׸��� logfile�� open�� �Ϸ�Ǹ� request info�� mFlag��
 *               file�� open�� �Ϸ�Ǿ�ٴ� ���� ǥ���ϱ� ����
 *               SMR_PRE_READ_FILE_OPEN�� Setting�Ѵ�. �׷��� mFlag����
 *               SMR_PRE_READ_FILE_CLOSE�� �Ǿ� �ִٸ� �̴� open�� request���ʿ���
 *               PreReadThread�� Open�ϴ� ���̿� close�� ��û�� ���̴�. �̶� �ٽ�
 *               logfile�� close�ϰ� �Ҵ�� resoruce�� �ݳ��Ѵ�.
 *
 *****************************************************************************/
void smrPreReadLFileThread::run()
{
    SInt                sState      = 0;
    smrPreReadLFInfo  * sCurPreReadInfo;
    time_t              sUntilTime;
    SInt                rc;

    while(mFinish == ID_FALSE)
    {
        if(sState == 0)
        {
            IDE_TEST( lockCond() != IDE_SUCCESS );
            sState = 1;
        }
        
        sUntilTime = idlOS::time(NULL) + 1;
        mTV.set(sUntilTime);

        sState = 0;
        rc = mCV.timedwait(&mCondMutex, &mTV);
        sState = 1;
        
        if ( rc != IDE_SUCCESS )
        {
            // �Ϲ� UNIX SYSTEM CALL RETURN �Ծ�: err == ETIME
            // PDL CALL RETURN �Ծ�: err == -1 && errno == ETIME 
            IDE_TEST_RAISE(mCV.isTimedOut() != ID_TRUE, err_cond_wait);
            mResume = ID_TRUE;
        }
        else
        {
            if ( mResume == ID_FALSE ) 
            {
                // clear checkpoint interval�� ����̹Ƿ� time�� reset �Ѵ�.
                continue;
            }
        }

        if ( smuProperty::isRunLogPreReadThread() == SMU_THREAD_OFF )
        {
            // To Fix PR-14783
            // System Thread�� �۾��� �������� �ʵ��� �Ѵ�.
            continue;
        }
        else
        {
            // Go Go 
        }
        
        sState = 0;
        IDE_TEST( unlockCond() != IDE_SUCCESS );
        
        sCurPreReadInfo = NULL;
            
        /* logfile open request list���� reqest�� �����ͼ�
           open logfile list�� �߰��Ѵ�. */
        IDE_TEST( getJobOfPreReadInfo(&sCurPreReadInfo)
                  != IDE_SUCCESS );
        
        if(sCurPreReadInfo != NULL)
        {
            rc = smrLogMgr::openLogFile( sCurPreReadInfo->mFileNo,
                                         ID_FALSE,
                                         &(sCurPreReadInfo->mLogFilePtr) );

            IDE_TEST( lock() != IDE_SUCCESS );
            sState = 2;

            if ( rc == IDE_FAILURE )
            {
                if ( ( errno == ENOENT ) ||
                     ( ideGetErrorCode() == smERR_ABORT_WaitLogFileOpen ) )
                {
                    sCurPreReadInfo->mLogFilePtr = NULL;
                    removeFromLFList(sCurPreReadInfo);
                    
                    IDE_TEST( mPreReadLFInfoPool.memfree( (void*)sCurPreReadInfo ) 
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_RAISE(error_file_open);
                }
            }
            else
            {
                
                if ( ( ( sCurPreReadInfo->mFlag & SMR_PRE_READ_FILE_MASK )
                       == SMR_PRE_READ_FILE_CLOSE ) || 
                     ( rc != IDE_SUCCESS ) )
                {
                    /* PreReadThread�� logfile�� open�ϰ� �ִ»��̿� �ٸ��ʿ���
                       close�� ��û�ߴ�.*/
                    IDE_TEST( smrLogMgr::closeLogFile(sCurPreReadInfo->mLogFilePtr )
                              != IDE_SUCCESS);
                    
                    sCurPreReadInfo->mLogFilePtr = NULL;
                    removeFromLFList(sCurPreReadInfo);
                    
                    IDE_TEST( mPreReadLFInfoPool.memfree((void*)sCurPreReadInfo)
                             != IDE_SUCCESS );
                }
                else
                {
                    /* open logfile list�� �ִ� PreReadInfo�� mFlag����
                       SMR_PRE_READ_FILE_OPEN�� setting�Ѵ�.*/
                    sCurPreReadInfo->mFlag &= ~SMR_PRE_READ_FILE_MASK;
                    sCurPreReadInfo->mFlag |= SMR_PRE_READ_FILE_OPEN;
                }
            }
            
            sState = 0;
            IDE_TEST( unlock() != IDE_SUCCESS );
            
        }
    } /* While */

    if( sState == 1 )
    {
        sState = 0;
        IDE_TEST( unlockCond() != IDE_SUCCESS );
    }
    
    return;

    IDE_EXCEPTION(error_file_open);
    {
    }
    IDE_EXCEPTION(err_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch(sState)
    {
        case 2:
            IDE_ASSERT( unlock() == IDE_SUCCESS );
            break;
            
        case 1:
            IDE_ASSERT( unlockCond() == IDE_SUCCESS );
            break;

        default:
            break;
    }
    IDE_POP();

    IDE_CALLBACK_FATAL( "Pre Read Logfile Thread Fatal" );
    return;
}

/***********************************************************************
 * Description : PreReadThread���� aFileNo�� �ش��ϴ� ���Ͽ�
 *               ���ؼ� open�� ��û�Ѵ�.
 *
 * aFileNo  - [IN] LogFile No
 */
IDE_RC smrPreReadLFileThread::addOpenLFRequest( UInt aFileNo )
{
    SInt              sState = 0;
    smrPreReadLFInfo *sCurPreReadInfo;

    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;
    
    IDE_TEST( mPreReadLFInfoPool.alloc((void**)&sCurPreReadInfo)
             != IDE_SUCCESS );

    initPreReadInfo(sCurPreReadInfo);
    
    sCurPreReadInfo->mFileNo = aFileNo;

    /* LogFile Open Request�� PreReadInfo�� �߰��Ѵ�.*/
    addToLFRequestList(sCurPreReadInfo);

    IDE_DASSERT(findInOpenLFRequestList( aFileNo ) ==
                sCurPreReadInfo);
    
    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    IDE_TEST( resume() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        (void)unlock();
        IDE_POP();
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : <aFileNo>�� �ش��ϴ� Logfile�� ���ؼ� Close�� ��û�Ѵ�.
 *
 * aFileNo - [IN] LogFile No
 */
IDE_RC smrPreReadLFileThread::closeLogFile( UInt aFileNo )
{
    SInt              sState = 0;
    smrPreReadLFInfo *sCurPreReadInfo;
    
    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /* Open LogFile List�� �ִ��� Check�Ѵ�.*/
    sCurPreReadInfo = findInOpenLFList( aFileNo );

    if(sCurPreReadInfo == NULL)
    {
        sCurPreReadInfo = findInOpenLFRequestList( aFileNo );

        if(sCurPreReadInfo != NULL)
        {
            // ���� Request List���� �ְ� ������ Open���� �ʾұ� ������
            // List���� ���Ÿ� �Ѵ�.
            removeFromLFRequestList(sCurPreReadInfo);
            IDE_TEST( mPreReadLFInfoPool.memfree((void*)sCurPreReadInfo)
                      != IDE_SUCCESS );
        }
    }
    else
    {
        if ( ( sCurPreReadInfo->mFlag & SMR_PRE_READ_FILE_MASK )
               == SMR_PRE_READ_FILE_OPEN )
        {
            /* File�� Open�Ǿ� �ִ�.*/
            IDE_TEST( smrLogMgr::closeLogFile( sCurPreReadInfo->mLogFilePtr )
                      != IDE_SUCCESS );

            sCurPreReadInfo->mLogFilePtr = NULL;

            removeFromLFList(sCurPreReadInfo);
            
            IDE_TEST( mPreReadLFInfoPool.memfree( (void*)sCurPreReadInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // ���� PreRead Thread�� �� ������ open�ϰ� �ִ�. ���߿� open��
            // ������ ���� sCurPreReadInfo->mFlag�� ���� Check�Ͽ�
            // SMR_PRE_READ_FILE_CLOSE�̸� File�� Close�ϰ� �����Ѵ�.
            IDE_ASSERT( (sCurPreReadInfo->mFlag & SMR_PRE_READ_FILE_MASK)
                         == SMR_PRE_READ_FILE_NON );
            sCurPreReadInfo->mFlag &= ~SMR_PRE_READ_FILE_MASK;
            sCurPreReadInfo->mFlag |= SMR_PRE_READ_FILE_CLOSE;
        }
    }

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 1:
            IDE_ASSERT( unlock() == IDE_SUCCESS);
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : open logfile Request list���� aFileNo�� ã�´�.
 *
 * aFileNo - [IN] LogFile No
 */
smrPreReadLFInfo* smrPreReadLFileThread::findInOpenLFRequestList( UInt aFileNo )
{
    smrPreReadLFInfo *sCurPreReadInfo;
        
    sCurPreReadInfo = mOpenLFRequestList.mNext;

    while(sCurPreReadInfo != &mOpenLFRequestList)
    {
        if ( sCurPreReadInfo->mFileNo == aFileNo )
        {
            break;
        }
        else
        {
            /* nothing to do ... */
        }

        sCurPreReadInfo = sCurPreReadInfo->mNext;
    }

    return sCurPreReadInfo == &mOpenLFRequestList ? NULL : sCurPreReadInfo;
}

/***********************************************************************
 * Description : open logfile list���� aFileNo�� ã�´�.
 *
 * aFileNo - [IN] LogFile No
 */
smrPreReadLFInfo* smrPreReadLFileThread::findInOpenLFList( UInt aFileNo )
{
    smrPreReadLFInfo *sCurPreReadInfo;

    sCurPreReadInfo = mOpenLFList.mNext;
    
    while(sCurPreReadInfo != &mOpenLFList)
    {
        if ( sCurPreReadInfo->mFileNo == aFileNo )
        {
            break;
        }
        else
        {
            /* nothing to do ... */
        }

        sCurPreReadInfo = sCurPreReadInfo->mNext;
    }

    return sCurPreReadInfo == &mOpenLFList ? NULL : sCurPreReadInfo;
}

/***********************************************************************
 * Description : open logfile Request list�� request�� �ִٸ� �̰��� lst log
 *               file no���� �۰ų� ������ �̸� request list���� open list��
 *               �̵��ϰ� �̿����� prereadinfo�� aPreReadInfo�� �־��ش�.
 *               
 * aPreReadInfo - [OUT] Open�ؾߵ� PreReadInfo
 *
 */
IDE_RC smrPreReadLFileThread::getJobOfPreReadInfo(smrPreReadLFInfo **aPreReadInfo)
{
    SInt      sState = 0;
    smLSN     sLstLSN;
    smrPreReadLFInfo *sCurPreReadInfo;

    IDE_DASSERT( aPreReadInfo != NULL );

    *aPreReadInfo = NULL;
    SM_LSN_MAX( sLstLSN );

    if ( isEmptyOpenLFRequestList() == ID_FALSE )
    {
        smrLogMgr::getLstLSN( &sLstLSN ) ;
            
        IDE_TEST( lock() != IDE_SUCCESS );
        sState = 1;

        if ( isEmptyOpenLFRequestList() == ID_FALSE )
        {
            sCurPreReadInfo = mOpenLFRequestList.mNext;

            while ( sCurPreReadInfo != &mOpenLFRequestList )
            {
                // ������ �αװ� ��ϵ� �α��������� Check�Ѵ�.
                if ( sCurPreReadInfo->mFileNo <= sLstLSN.mFileNo )
                {
                    removeFromLFRequestList( sCurPreReadInfo );
                    addToLFList( sCurPreReadInfo );

                    IDE_ASSERT( findInOpenLFList( sCurPreReadInfo->mFileNo )
                                == sCurPreReadInfo );

                    *aPreReadInfo = sCurPreReadInfo;
                    break;
                }
                    
                sCurPreReadInfo = sCurPreReadInfo->mNext;
            }
        }
        else
        {
            /* nothing do to */
        }

        sState = 0;
        IDE_TEST( unlock() != IDE_SUCCESS );
    }
    else
    {
        /* nothing do to */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        (void)unlock();
        IDE_POP();
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : PreReadThread�� �����Ѵ�.
 *
 */
IDE_RC smrPreReadLFileThread::shutdown()
{
    mFinish = ID_TRUE;
    
    IDL_MEM_BARRIER;

    // �����尡 ������ ����� ������ ��ٸ���.
    IDE_TEST_RAISE( join() != IDE_SUCCESS, err_thr_join );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));   
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : PreReadThread�� �����. �̹� �۾����̸� ���õȴ�.
 *
 */
IDE_RC smrPreReadLFileThread::resume()
{
    SInt sState = 0;
    
    IDE_TEST( lockCond() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST_RAISE(mCV.signal() != IDE_SUCCESS, err_cond_signal);

    sState = 0;
    IDE_TEST( unlockCond() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(unlockCond() == IDE_SUCCESS);
        IDE_POP();
    }
    
    return IDE_FAILURE;
    
}
