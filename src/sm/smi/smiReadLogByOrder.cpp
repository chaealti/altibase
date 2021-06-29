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
 * $Id: smiReadLogByOrder.cpp 85722 2019-06-25 06:18:05Z minku.kang $
 **********************************************************************/

#include <smDef.h>
#include <smr.h>
#include <smi.h>

/*
  �� Class�� Parallel Loggingȯ�濡�� Replication Sender�� �α׸� �о
  ���� ��� Log Header�� Sequence Number������ �α׸� ������ ���ؼ� �ۼ���
  Class�̴�. smiReadInfo�� �����Ͽ� ������� �� �α���
  ������, ���� ��ġ������ �����ϰ� Read�� ȣ��ɶ� ����
  smiReadInfo�� �м��Ͽ� ��������� �α׸� �����Ѵ�. �����ϴ� ������
  ������ ����.

      - ������ �α׸� ������ �ȴ�. �ֳ��ϸ� �������� ��ġ�� Log Header�� Sequence
        Number�� ������ �����ϱ� �����̴�.
*/

/***********************************************************************
 * Description : aInitLSN���� ũ�ų� ���� LSN���� ���� �α���
 *               ���� ���� LSN���� ���� log�� ��ġ�� ã�´�. ���������
 *               endLSN���� reading position�� setting�Ѵ�.
 *               
 *
 * aInitSN         - [IN] ù��° �о���� �α��� SN
 * aPreOpenFileCnt - [IN] PreRead�� �α������� ����
 * aNotDecomp      - [IN] ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ���� ���� (����� ���·� ��ȯ)
 */
IDE_RC smiReadLogByOrder::initialize( smSN     aInitSN,
                                      UInt     aPreOpenFileCnt,
                                      idBool   aIsRemoteLog,
                                      ULong    aLogFileSize,
                                      SChar ** aLogDirPath,
                                      idBool   aNotDecomp )
{
    UInt   j;
    UInt   sFileNo;
    idBool sIsRemoteLogMgrInit = ID_FALSE;
    smLSN  sInitLSN;

    mNotDecomp = aNotDecomp;

    IDE_DASSERT( (aInitSN != SM_SN_NULL) && (aInitSN < SM_SN_MAX) );

    SM_MAKE_LSN( sInitLSN, aInitSN );

    /* PROJ-1915 : ID_FALSE :local �α�, ID_TRUE : Remote Log */
    mIsRemoteLog    = aIsRemoteLog;

    if ( mIsRemoteLog == ID_TRUE )
    {
        /* PROJ-1915 : off-line �α� ó���� ���� smrRemoteLogMgr�ʱ�ȭ */
        IDE_TEST( mRemoteLogMgr.initialize( aLogFileSize, 
                                            aLogDirPath,
                                            mNotDecomp )
                  != IDE_SUCCESS );
        sIsRemoteLogMgrInit = ID_TRUE;
    }

    mCurReadInfoPtr = NULL;
    mPreReadFileCnt = aPreOpenFileCnt;
    SM_LSN_MAX( mLstReadLogLSN );    

    IDE_TEST( initializeReadInfo( & mReadInfo ) != IDE_SUCCESS );
    
    IDE_TEST( mPQueueRedoInfo.initialize( IDU_MEM_SM_SMR,
                                          1,                       /* aDataMaxCnt*/
                                          ID_SIZEOF(smiReadInfo*), /* aDataSize */
                                          compare )
             != IDE_SUCCESS );

    /* ù��° �о�� �� �α������� ��ġ�� ã�´�.*/
    IDE_TEST( setFirstReadLogFile( sInitLSN ) != IDE_SUCCESS );

    /* ù��° �о�� �� �α׷��ڵ��� ��ġ�� ã�´�.*/
    IDE_TEST( setFirstReadLogPos( sInitLSN ) != IDE_SUCCESS );

    /* mPreReadFileCnt > 0�϶��� PreRead Thread�� �ٿ��.*/
    if ( mPreReadFileCnt != 0 )
    {
        IDE_TEST( mPreReadLFThread.initialize() != IDE_SUCCESS );

        sFileNo = mReadInfo.mReadLSN.mFileNo + 1;
        /* mReadInfo[i].mReadLSN.mFileNo�� ����Ű�� ������
           �̹� Open�Ǿ��ִ�.*/
        for ( j = 0 ; j < mPreReadFileCnt ; j++ )
        {
            IDE_TEST( mPreReadLFThread.addOpenLFRequest( sFileNo )
                      != IDE_SUCCESS );
            sFileNo++;
        }

        /* PreRead LogFile Thread�� Start��Ų��. */
        IDE_TEST( mPreReadLFThread.start() != IDE_SUCCESS );
        IDE_TEST( mPreReadLFThread.waitToStart(0) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsRemoteLogMgrInit == ID_TRUE )
    {
        IDE_PUSH();
        (void)mRemoteLogMgr.destroy();
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smiReadLogByOrder�� �����Ѵ�.
 *
 * �Ҵ�� Resource�� �����Ѵ�.
 */
IDE_RC smiReadLogByOrder::destroy()
{
    
    IDE_TEST( mPQueueRedoInfo.destroy()  != IDE_SUCCESS );

    IDE_TEST( destroyReadInfo( & mReadInfo ) != IDE_SUCCESS );

    if ( mPreReadFileCnt != 0 )
    {
        IDE_TEST( mPreReadLFThread.shutdown() != IDE_SUCCESS );
        IDE_TEST( mPreReadLFThread.destroy() != IDE_SUCCESS );
    }

    /* PROJ-1915 */
    if ( mIsRemoteLog == ID_TRUE )
    {
        IDE_TEST( mRemoteLogMgr.destroy() != IDE_SUCCESS );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Read Info�� �ʱ�ȭ �Ѵ�.

    [IN] aReadInfo - �ʱ�ȭ�� Read Info
 */
IDE_RC smiReadLogByOrder::initializeReadInfo( smiReadInfo * aReadInfo )
{
    IDE_DASSERT( aReadInfo != NULL );
    
    idlOS::memset( aReadInfo, 0, ID_SIZEOF( *aReadInfo ) );
    
    aReadInfo->mLogFilePtr = NULL;

    // �α� �������� ���� �ڵ��� �ʱ�ȭ
    IDE_TEST( aReadInfo->mDecompBufferHandle.initialize( IDU_MEM_SM_SMR )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Read Info�� �ı��Ѵ�.
    
    [IN] aReadInfo - �ı��� Read Info
 */
IDE_RC smiReadLogByOrder::destroyReadInfo( smiReadInfo * aReadInfo )
{
    IDE_DASSERT( aReadInfo != NULL );

    // �α� �������� ���� �ڵ��� �ı�
    IDE_TEST( aReadInfo->mDecompBufferHandle.destroy()
              != IDE_SUCCESS );
    
    if ( aReadInfo->mLogFilePtr != NULL )
    {
        /* PROJ-1915 */
        if ( mIsRemoteLog == ID_FALSE )
        {
            IDE_TEST( smrLogMgr::closeLogFile(aReadInfo->mLogFilePtr)
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( mRemoteLogMgr.closeLogFile(aReadInfo->mLogFilePtr)
                      != IDE_SUCCESS );
        }
        aReadInfo->mLogFilePtr = NULL;        
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : iduPriorityQueue���� Item���� Compare�� �� ����ϴ�
 *               Callback Function��. iduPriorityQueue�� initialize�Ҷ� �Ѱ���
 *
 * arg1  - [IN] compare�� smiReadInfo 1
 * arg2  - [IN] compare�� smiReadInfo 2
*/
SInt smiReadLogByOrder::compare(const void *arg1,const void *arg2)
{
    smLSN sLSN1;
    smLSN sLSN2;

    IDE_DASSERT( arg1 != NULL );
    IDE_DASSERT( arg2 != NULL );
        
    SM_GET_LSN( sLSN1, ((*(smiReadInfo**)arg1))->mReadLSN );
    SM_GET_LSN( sLSN2, ((*(smiReadInfo**)arg2))->mReadLSN );

    if ( smrCompareLSN::isGT( &sLSN1, &sLSN2 ) )
    {
        return 1;
    }
    else
    {
        if ( smrCompareLSN::isLT( &sLSN1, &sLSN2 ) )
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}

/***********************************************************************
 * Description : ���������� ���� �α� ������ ��ϵ� Log�� �о �ش�.

  mSortRedoInfo�� ����ִ� smiReadInfo�߿��� ���� ���� mLSN����
  ���� Log�� �о���δ�.
  
  aSN           - [OUT] Log�� SN
  aLSN          - [OUT] Log�� LSN
  aLogHeadPtr   - [OUT] aLSN�� ����Ű�� log�� Log Header Ptr
  aLogPtr       - [OUT] aLSN�� ����Ű�� log�� Log Buffr Ptr
  aIsValid  - [OUT] aLSN�� ����Ű�� log�� Valid�ϸ� ID_TRUE�ƴϸ� ID_FALSE
 **********************************************************************/
IDE_RC smiReadLogByOrder::readLog( smSN    * aSN,
                                   smLSN   * aLSN,
                                   void   ** aLogHeadPtr,
                                   void   ** aLogPtr,
                                   idBool  * aIsValid )
{
    idBool    sIsExistLogTail = ID_FALSE;
    idBool    sUnderflow      = ID_FALSE;
#ifdef DEBUG 
    smLSN     sReadLSN;
#endif

    IDE_DASSERT( aLSN             != NULL );
    IDE_DASSERT( aLogPtr          != NULL );
    IDE_DASSERT( aIsValid         != NULL );
    
    *aIsValid = ID_FALSE;

    if ( mCurReadInfoPtr != NULL )
    {
        if ( mCurReadInfoPtr->mIsValid == ID_TRUE )
        {

            if ( ( ( mCurReadInfoPtr->mLogHead.mFlag ) & SMR_LOG_COMPRESSED_MASK )
                 == SMR_LOG_COMPRESSED_OK )
            {
                // Comp �α׸� FILE END�� ����.
                mCurReadInfoPtr->mReadLSN.mOffset += mCurReadInfoPtr->mLogSizeAtDisk;
            } 
            else
            {
                /* ���������� Redo������ �� smiReadInfo�� mReadLSN��
                   smrRecoveryMgr�� Redo���� update�� �ȴ�. ���� �ٽ�
                   mReadLSN�� ����Ű�� �α׸� �о smiReadInfo�� �����ϰ�
                   �ٽ� mSortArrRedoInfo�� �־�� �Ѵ�. */
                if ( smrLogHeadI::getType( &mCurReadInfoPtr->mLogHead )
                     == SMR_LT_FILE_END )
                {
                    mCurReadInfoPtr->mReadLSN.mFileNo++;
                    mCurReadInfoPtr->mReadLSN.mOffset = 0;
                    mCurReadInfoPtr->mIsLogSwitch = ID_TRUE;
                }
                else
                {
                    mCurReadInfoPtr->mReadLSN.mOffset += mCurReadInfoPtr->mLogSizeAtDisk;
                }
            }
            mCurReadInfoPtr->mIsValid = ID_FALSE;
        }
    }

    /* �α׸� �о Valid�� �α׸� ã�´� */
    IDE_TEST( searchValidLog( &sIsExistLogTail ) != IDE_SUCCESS );

    mPQueueRedoInfo.dequeue( (void*)&mCurReadInfoPtr, &sUnderflow );

    if ( sUnderflow == ID_FALSE )
    {
        /* BUG-26717 CodeSonar Null Pointer Dereference
         * sUnderflow�� ID_FALSE�� ��� mCurReadInfoPtr�� Null�� �ƴ� */
        IDE_ASSERT( mCurReadInfoPtr != NULL );

        *aIsValid = mCurReadInfoPtr->mIsValid;

        /* �о���� �αװ� ���°�� */
        if ( sIsExistLogTail == ID_TRUE )
        {
            IDE_DASSERT( mCurReadInfoPtr->mIsValid == ID_TRUE );
            
            /* Read���� ù��° read�� sIsExistLogTail�� True����
               ����. ���� mLstReadLogLSN�� SM_SN_NULL�� �ɼ� ����. */
            IDE_ASSERT( !SM_IS_LSN_MAX( mLstReadLogLSN ) );           

            /* 
               LSN��ȣ������ ������ ���� ����Ǳ� �Ǳ� ������ ���������� ����
               �α��� LSN���� ���� �α��� LSN���� ���ؼ� ������
               ���� �αװ� �ִ��� �����Ѵ�.�̸� ���� ���������� ���� �α���
               LSN���� ���� �α��� LSN���� ������ �����Ͽ�
               �̹� ���� LSN���� ���� LSN�� ������ �αװ� �߰ߵǸ� �߰��� ���� LSN���� ������
               �α׸� ����ϴ� Transaction�� �ִ� ������ �Ǵ��Ѵ�.
               �߰ߵ� �߰��� ���� LSN�� queue�� �ٽ� ���� �ʾ����Ƿ� replication ���� ���ܵɵ�. 
               ������ �÷��� Sender�� �ٽ� retry ���� �Ǵ��ϰ� �Ѵ�.  
            */
#ifdef DEBUG
            sReadLSN = smrLogHeadI::getLSN( &mCurReadInfoPtr->mLogHead ); 
            IDE_DASSERT( smrCompareLSN::isEQ( &mCurReadInfoPtr->mReadLSN, &sReadLSN ) );
#endif
            //[TASK-6757]LFG,SN ����
            if ( smrCompareLSN::isLT( &mCurReadInfoPtr->mReadLSN, &mLstReadLogLSN ) )
            {
                ideLog::log( IDE_ERR_0,
                             "Read Invalid Log during Read Log By Order. \n"
                             "CurRedo LSN = %"ID_UINT32_FMT",%"ID_UINT32_FMT"\n"
                             "Last Apply  LSN = %"ID_UINT32_FMT",%"ID_UINT32_FMT"\n",
                             mCurReadInfoPtr->mReadLSN.mFileNo,
                             mCurReadInfoPtr->mReadLSN.mOffset,
                             mLstReadLogLSN.mFileNo,
                             mLstReadLogLSN.mOffset );

                *aIsValid = ID_FALSE;
                mCurReadInfoPtr = NULL;

                IDE_ERROR_RAISE(0, error_read_invalid_log );
            }
            else
            {
                /* nothing to do */
            }
        }
        
        if ( *aIsValid == ID_TRUE )
        {
            /* ���������� ���� Log�� LSN���� �����Ѵ�.*/
#ifdef DEBUG
            sReadLSN = smrLogHeadI::getLSN( &mCurReadInfoPtr->mLogHead ); 
            IDE_DASSERT( smrCompareLSN::isEQ( &mCurReadInfoPtr->mReadLSN, &sReadLSN ) );
#endif
            mLstReadLogLSN = mCurReadInfoPtr->mReadLSN;
            *aSN           = SM_MAKE_SN( mCurReadInfoPtr->mReadLSN );

            *aLogPtr       = mCurReadInfoPtr->mLogPtr;
            *aLogHeadPtr   = &(mCurReadInfoPtr->mLogHead);

            *aLSN = mCurReadInfoPtr->mReadLSN;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_read_invalid_log )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG,
                                  "[Repl] read log by order" ));
    }
    IDE_EXCEPTION_END;

    *aLogPtr          = NULL;
    *aLogHeadPtr      = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aInitLSN���� ū SN�� ���� ���� SN����
 *               ���� �α������� ã�´�.
 * [TASK-6757]LFG,SN ����  BUGBUG : LSN�� �˰� �ִ�.-> ������ ã���� �ִ�. 
 *
 * aInitLSN    - [IN] ù��° �о�� �� �α��� LSN��
 ***********************************************************************/
IDE_RC smiReadLogByOrder::setFirstReadLogFile( smLSN aInitLSN )
{
    smLSN       sEndLSN;
    UInt        sFstChkFileNo  = 0;
    UInt        sEndChkFileNo  = 0;
    UInt        sFstReadFileNo = 0;
    UInt        sState      = 0;
    idBool      sIsFound    = ID_FALSE;
    idBool      sIsValid    = ID_FALSE;
    SChar       sErrMsg[512] = { 0, };
#ifdef DEBUG
    smLSN       sReadLSN;
#endif
    /* LSN�� �����Ͽ� Replication�� ���� �� ��� 
     * ex) ALTER REPLICATION ALA1 START AT SN(0)
     * LSN �� INIT ��  ������ �־ ASSERT �� �˻��ϸ� �ȵȴ�. */
    IDE_DASSERT( !SM_IS_LSN_MAX( aInitLSN ) );
 
    // BUG-27725
    SM_LSN_INIT( sEndLSN );
 
    /* File�� Check�ϴ� ���� Check����Ʈ�� �߻��Ͽ� Log File�� ������
       �� �ִ�. �̸� �����ϱ� ���ؼ� Lock�� �����Ѵ�.*/
    if ( mIsRemoteLog == ID_FALSE )
    {
        IDE_TEST( smrRecoveryMgr::lockDeleteLogFileMtx()
                  != IDE_SUCCESS );
        sState = 1;

        smrRecoveryMgr::getLstDeleteLogFileNo( &sFstChkFileNo );
    }
    else
    {
        mRemoteLogMgr.getFirstFileNo( &sFstChkFileNo );
    }
    
    if ( mIsRemoteLog == ID_FALSE )
    {
        smrLogMgr::getLstLSN( &sEndLSN );
    }
    else
    {
        mRemoteLogMgr.getLstLSN( &sEndLSN );
    }
    
    sEndChkFileNo = sEndLSN.mFileNo;

    if ( mIsRemoteLog == ID_FALSE )
    {
        (void)smrLogMgr::getFirstNeedLFN( aInitLSN,
                                          sFstChkFileNo,
                                          sEndChkFileNo,
                                          &sFstReadFileNo );
    }
    else
    {
        (void)mRemoteLogMgr.getFirstNeedLFN( aInitLSN,
                                             sFstChkFileNo,
                                             sEndChkFileNo,
                                             &sFstReadFileNo );
    }

    if ( mIsRemoteLog == ID_FALSE )
    {
        sState = 0;
        IDE_TEST( smrRecoveryMgr::unlockDeleteLogFileMtx()
                  != IDE_SUCCESS );
    }

    /*
       sENDLSN�� mFileNo���� ���� �α� ���ϵ鿡
       ���ؼ� aInitLSN���� ū LSN�� ���� ���� LSN�� ���� �α������� ã�´�.
    */
    SM_SET_LSN( mReadInfo.mReadLSN,
                sFstReadFileNo,
                0 );
   
    mReadInfo.mLogFilePtr  = NULL;
    mReadInfo.mIsValid     = ID_FALSE;
    SM_LSN_INIT( mReadInfo.mLstLogLSN );
    mReadInfo.mIsLogSwitch = ID_FALSE;

    /* mReadLSN�� ����Ű�� Log�� �о mReadInfo�� �����Ѵ�. */
    if ( mIsRemoteLog == ID_FALSE )
    {
        IDE_TEST( smrLogMgr::readFirstLogHead( &(mReadInfo.mReadLSN),
                                               &(mReadInfo.mLogHead) )
                  != IDE_SUCCESS );
        /* MagicNumber�� �ּ��� valid  �˻縦 �Ѵ�. */
        sIsValid = smrLogFile::isValidMagicNumber( &(mReadInfo.mReadLSN), 
                                                   &(mReadInfo.mLogHead) );
    }
    else
    {
        /*
         *  Offline replicator ���� ���Ǹ� Log ������ ��ȿ���� Ȯ���Ѵ�.
         */
        IDE_TEST( mRemoteLogMgr.readFirstLogHead( &(mReadInfo.mReadLSN),
                                                  &(mReadInfo.mLogHead),
                                                  &sIsValid )
                  !=IDE_SUCCESS );
    }

    if ( sIsValid == ID_TRUE )
    {
#ifdef DEBUG
        sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
        IDE_DASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif

        /* MagicNumber�� �ּ��� valid �˻縸 �ؼ� �ѱ��. 
         * offset ������ �� valid �˻縦 �Ѵ�.  */
    
        mReadInfo.mIsValid = ID_TRUE;
        SM_GET_LSN( mReadInfo.mLstLogLSN, mReadInfo.mReadLSN );

        // BUG-29115
        // ��� �ϳ��� aInitLSN�� �����ؾ� �Ѵ�.
        if ( smrCompareLSN::isLTE( &mReadInfo.mLstLogLSN, &aInitLSN ) )
        {
            sIsFound = ID_TRUE;
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        /* do nothing */
    }

    IDU_FIT_POINT_RAISE( "smiReadLogByOrder::setFirstReadLogFile:sIsFound",
                         ERR_NOT_FOUND_LOG );
    IDE_TEST_RAISE( sIsFound == ID_FALSE, ERR_NOT_FOUND_LOG );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_LOG);
    {
        idlOS::snprintf( sErrMsg, 512,
                         "sFstChkFileNo : %"ID_UINT32_FMT
                         ", sEndChkFileNo : %"ID_UINT32_FMT
                         ", sFstReadFileNo : %"ID_UINT32_FMT,
                         sFstChkFileNo,
                         sEndChkFileNo,
                         sFstReadFileNo );
        IDE_SET(ideSetErrorCode( smERR_ABORT_NotFoundLog, 
                                 aInitLSN.mFileNo,
                                 aInitLSN.mOffset,
                                 sErrMsg ));
    }
    IDE_EXCEPTION_END;
        
    if ( sState != 0 )
    {
        IDE_PUSH();
        (void)smrRecoveryMgr::unlockDeleteLogFileMtx();
        IDE_POP();
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aInitLSN���� ū SN�� ���� ���� SN����
 *               ���� �α����Ͽ��� ���� �α��� ��ġ�� ã�´�.
 * [TASK-6757]LFG,SN ����  BUGBUG : LSN�� �˰� �ִ�.-> ������ ã���� �ִ�. 
 *
 * [BUG-44571] aInitLSN�� ������ ������ �α�(SMR_LT_FILE_END)�̰ų�
 *             �׺��� ū OFFSET�� ���� ��� �α׸� ã�����Ѵ�.
 *             �̰��, ���������� ù�α�(offset:0)�� �����ϵ��� �Ѵ�. 
 *
 * aInitLSN    - [IN] ù��° �о�� �� �α��� LSN��
 **********************************************************************/
IDE_RC smiReadLogByOrder::setFirstReadLogPos( smLSN aInitLSN )
{
    void      * sQueueData;
    idBool      sOverflow   = ID_FALSE;
    smLSN       sLstWriteLSN;
    SChar       sErrMsg[512] = { 0, };
    smLSN       sEndLSN;
#ifdef DEBUG
    smLSN       sReadLSN;
#endif

    if ( mIsRemoteLog == ID_FALSE )
    {
        smrLogMgr::getLstLSN( &sEndLSN );
    }
    else
    {
        mRemoteLogMgr.getLstLSN( &sEndLSN );
    }

    /* LSN�� �����Ͽ� Replication�� ���� �� ��� 
     * ex) ALTER REPLICATION ALA1 START AT SN(0)
     * LSN �� INIT�� ������ �־ ASSERT �� �˻��ϸ� �ȵȴ�. */
    IDE_DASSERT( !SM_IS_LSN_MAX( aInitLSN ) );
 
    /* mReadInfo�� ������ �α����ϳ�����
     * aInitLSN���� ũ�ų� ���� LSN�� ���� �α��߿��� ���� ����
     * LSN�� ���� �α��� ��ġ�� ã�´�. */
    while ( mReadInfo.mIsValid == ID_TRUE )
    {
        /* ���� �д� �α��� LSN���� aInitLSN������ ù��°�� ũ�ų� ���ٸ�
         * �̷αװ� aInitLSN�� ���� ����� ���̴�.�ֳ��ϸ� �α״� �������
         * ��ϵǱ⶧���̴�. */

#ifdef DEBUG
        sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
        IDE_DASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif
        if ( smrCompareLSN::isGTE( &mReadInfo.mReadLSN, &aInitLSN ) ) 
        {
            /* BUG-21726 */
            if ( mReadInfo.mLogPtr == NULL )
            {
                /* mReadLSN�� ����Ű�� Log�� �о mReadInfo�� �����Ѵ�.*/
                if ( mIsRemoteLog == ID_FALSE )
                {
                    if ( mNotDecomp == ID_FALSE )
                    {
                        /* ����� �α������� ��� ������ Ǯ� ���ڵ带 �о�´�. */
                        IDE_TEST( smrLogMgr::readLog(
                                                &(mReadInfo.mDecompBufferHandle ),
                                                &(mReadInfo.mReadLSN),
                                                ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                                &(mReadInfo.mLogFilePtr),
                                                &(mReadInfo.mLogHead),
                                                &(mReadInfo.mLogPtr),
                                                &(mReadInfo.mIsValid),
                                                &(mReadInfo.mLogSizeAtDisk) )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ�Ѵ�. */
                        IDE_TEST( smrLogMgr::readLog4RP(
                                                &(mReadInfo.mReadLSN),
                                                ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                                &(mReadInfo.mLogFilePtr),
                                                &(mReadInfo.mLogHead),
                                                &(mReadInfo.mLogPtr),
                                                &(mReadInfo.mIsValid),
                                                &(mReadInfo.mLogSizeAtDisk) )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    IDE_TEST( mRemoteLogMgr.readLogAndValid(
                                            &(mReadInfo.mDecompBufferHandle ),
                                            &(mReadInfo.mReadLSN),
                                            ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                            &(mReadInfo.mLogFilePtr),
                                            &(mReadInfo.mLogHead),
                                            &(mReadInfo.mLogPtr),
                                            &(mReadInfo.mIsValid),
                                            &(mReadInfo.mLogSizeAtDisk) )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                /* nothing to do ... */
            }

            sQueueData = (void *)&(mReadInfo);
            mPQueueRedoInfo.enqueue((void*)&sQueueData, &sOverflow);
            IDE_ASSERT( sOverflow == ID_FALSE );

            break;
        }
        else
        {
            /* nothing to do ... */
        }

        IDU_FIT_POINT_RAISE( "smiReadLogByOrder::setFirstReadLogPos:smrLogHeadI::getType::mReadLSN.mFileNo",
                             ERR_NOT_FOUND_LOG );

        if ( ( smrLogHeadI::getFlag( &mReadInfo.mLogHead ) & SMR_LOG_COMPRESSED_MASK )
               == SMR_LOG_COMPRESSED_OK )
        {
            /* ���� �α׸� �б� ���� */
            mReadInfo.mReadLSN.mOffset += mReadInfo.mLogSizeAtDisk;
        }
        else
        {
            if ( smrLogHeadI::getType( &mReadInfo.mLogHead ) == SMR_LT_FILE_END )
            {
                if ( mReadInfo.mReadLSN.mFileNo < sEndLSN.mFileNo )
                {
                    /* BUG-44571
                     * �߰ߵ� �αװ� ������ ������ �α�(SMR_LT_FILE_END)�ΰ�� 
                     * ���������� ù�α�(offset:0)�� �д´�. */
                    SM_SET_LSN( mReadInfo.mReadLSN,
                                mReadInfo.mReadLSN.mFileNo + 1, /* next file no. */
                                0 );
                }
                else
                {
                    /* ���� �α������� ����. */
                    IDE_RAISE( ERR_NOT_FOUND_LOG );
                }
            }
            else
            {
                /* ���� �α׸� �б� ���� */
                mReadInfo.mReadLSN.mOffset += mReadInfo.mLogSizeAtDisk;
            }
        }
        
        /* mReadLSN�� ����Ű�� Log�� �о mReadInfo�� �����Ѵ�.*/
        if ( mIsRemoteLog == ID_FALSE )
        {
            if ( mNotDecomp == ID_FALSE )
            {
                /* ����� �α������� ��� ������ Ǯ� ���ڵ带 �о�´�. */
                IDE_TEST( smrLogMgr::readLog( &(mReadInfo.mDecompBufferHandle ),
                                              &(mReadInfo.mReadLSN),
                                              ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                              &(mReadInfo.mLogFilePtr),
                                              &(mReadInfo.mLogHead),
                                              &(mReadInfo.mLogPtr),
                                              &(mReadInfo.mIsValid),
                                              &(mReadInfo.mLogSizeAtDisk) )
                          != IDE_SUCCESS );
            }
            else
            {
                /* ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ�Ѵ�. */
                IDE_TEST( smrLogMgr::readLog4RP( &(mReadInfo.mReadLSN),
                                                 ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                                 &(mReadInfo.mLogFilePtr),
                                                 &(mReadInfo.mLogHead),
                                                 &(mReadInfo.mLogPtr),
                                                 &(mReadInfo.mIsValid),
                                                 &(mReadInfo.mLogSizeAtDisk) )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( mRemoteLogMgr.readLogAndValid(
                                        &(mReadInfo.mDecompBufferHandle ),
                                        &(mReadInfo.mReadLSN),
                                        ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                        &(mReadInfo.mLogFilePtr),
                                        &(mReadInfo.mLogHead),
                                        &(mReadInfo.mLogPtr),
                                        &(mReadInfo.mIsValid),
                                        &(mReadInfo.mLogSizeAtDisk) )
                    != IDE_SUCCESS );

            /* BUG-26768 */
            /* initLSN���� ū SN���� ��ϵ� �αװ� ���� ���
             * mIsValid�� ID_TRUE�� �ϰ� ���´�. */
            mRemoteLogMgr.getLstLSN( &sLstWriteLSN );
            if ( ( mReadInfo.mIsValid == ID_FALSE ) &&
                 ( smrCompareLSN::isLT( &sLstWriteLSN, &aInitLSN ) ) )           
            {
                mReadInfo.mIsValid = ID_TRUE;
                break;
            }
            else
            {
                /* noghint to do ... */
            }
        }
    } // end of while
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_FOUND_LOG);
    {
        idlOS::snprintf( sErrMsg, 512,
                         "Last read LogLSN : %"ID_UINT32_FMT",%"ID_UINT32_FMT,
                         mReadInfo.mLstLogLSN.mFileNo, mReadInfo.mLstLogLSN.mOffset );
        IDE_SET(ideSetErrorCode( smERR_ABORT_NotFoundLog, 
                                 aInitLSN.mFileNo,
                                 aInitLSN.mOffset,
                                 sErrMsg ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� ���� �αװ�
 *               ��ϵǾ����� üũ�ϰ� �ִٸ� �о���δ�.
 *
 * aIsExistLogTail - [OUT] mReadInfo�� ����Ű��
 *                        �αװ� Invalid�� �α׶��
 *                        ID_TRUE, �ƴϸ� ID_FALSE.
 *
 */
IDE_RC smiReadLogByOrder::searchValidLog( idBool *aIsExistLogTail )
{
    UInt    sPreReadFileCnt = mPreReadFileCnt;
    void   *sQueueData;
    idBool  sOverflow  = ID_FALSE;
    smLSN   sLastLogLSN;
    smLSN   sReadLSN;

    IDE_DASSERT( aIsExistLogTail != NULL );

    *aIsExistLogTail = ID_TRUE;

    //���� �αװ� �����ٸ� �ٽ� ���ο� �αװ� ��ϵǾ�����
    //üũ�ؾ��Ѵ�.
    if ( mReadInfo.mIsValid == ID_FALSE )
    {
        if ( mIsRemoteLog == ID_FALSE )
        {
            /* BUG-42739
             * getlstWriteSN �� �ƴ� smiGetValidLSN�� �̿��ؼ�
             * Dummy�� �������� �ʴ� last Used LSN �� �޾ƿ;� �Ѵ�. */
            IDE_TEST( smiGetLastValidLSN( &sLastLogLSN ) != IDE_SUCCESS );

            if ( !(smrCompareLSN::isEQ( &sLastLogLSN, &mReadInfo.mLstLogLSN ) ) )
            {
                if ( mNotDecomp == ID_FALSE )
                {
                    /*mReadLSN�� ����Ű�� Log�� �о mReadInfo�� �����Ѵ�.*/
                    IDE_TEST( smrLogMgr::readLog(
                                             &(mReadInfo.mDecompBufferHandle ),
                                             &(mReadInfo.mReadLSN),
                                             ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                             &(mReadInfo.mLogFilePtr),
                                             &(mReadInfo.mLogHead),
                                             &(mReadInfo.mLogPtr),
                                             &(mReadInfo.mIsValid),
                                             &(mReadInfo.mLogSizeAtDisk) )
                             != IDE_SUCCESS );
                }
                else
                {
                    /* mReadLSN�� ����Ű�� Log�� �о mReadInfo�� �����Ѵ�
                       ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ�Ѵ�. */
                    IDE_TEST( smrLogMgr::readLog4RP(
                                             &(mReadInfo.mReadLSN),
                                             ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                             &(mReadInfo.mLogFilePtr),
                                             &(mReadInfo.mLogHead),
                                             &(mReadInfo.mLogPtr),
                                             &(mReadInfo.mIsValid),
                                             &(mReadInfo.mLogSizeAtDisk) )
                             != IDE_SUCCESS );
                }

            /* BUG-37931 
             ------------------------------------------------     /  --------------------
             LSN    | 100 | 101 | 102   | 103 | 104   | 105 | .. /_ .. | 110 | 111  | 112
             Status | ok  | ok  | dummy | ok  | dummy | ok  |     /    | ok  | dummy| ok
             ------------ A ------------------------- B ----     /   - C ---------- D ---
                                                                server restart
             case 1) service ����
                     dummy �α׸� ������ �ִ� LSN (B)
                     ���������� valid �� �α׸� ������ ���� (A)
                     101( A ) ������ �о�� �ϴµ� 102�� �о��ٸ� retry

             case 2) server restart
                     restart �� dummy �α׸� ������ �ִ� LSN (D)
                     restart �� valid �� �α׸� ������ ���� (C)

                     dummy�� ����� ���� �༮�� �ִٸ�..
                     getLstWriteLSN�� (C) ���� ������������
                     rp���� ó���Ѵ�. */

#ifdef DEBUG 
                sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
                IDE_DASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif
                if ( smrCompareLSN::isLT( &sLastLogLSN, &mReadInfo.mReadLSN ) == ID_TRUE )
                {
                    mReadInfo.mIsValid     = ID_FALSE;
                    mReadInfo.mIsLogSwitch = ID_FALSE;
                }
                else
                {
                    /* nothing to do ... */
                }

                if ( ( mReadInfo.mIsLogSwitch == ID_TRUE ) &&
                     ( sPreReadFileCnt != 0 ) )
                {
                    IDE_TEST( mPreReadLFThread.closeLogFile( mReadInfo.mReadLSN.mFileNo )
                              != IDE_SUCCESS );

                    /* Prefetch Thread�� ������ �̸� �α������� �о���δ�.*/
                    IDE_TEST( mPreReadLFThread.addOpenLFRequest(
                                                         mReadInfo.mReadLSN.mFileNo +
                                                         sPreReadFileCnt )
                              != IDE_SUCCESS );
                    mReadInfo.mIsLogSwitch = ID_FALSE;
                }
                else
                {
                    /* nothing to do ... */
                }

                if ( mReadInfo.mIsValid == ID_TRUE )
                {
                    IDE_ASSERT( sOverflow == ID_FALSE );

                    sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
#ifdef DEBUG 
                    IDE_ASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif
                    SM_GET_LSN( mReadInfo.mLstLogLSN, sReadLSN );
                    sQueueData = (void*)(&mReadInfo); //BUG-21726

                    mPQueueRedoInfo.enqueue( (void*)&sQueueData, &sOverflow );
                    *aIsExistLogTail = ID_FALSE;
                }
                else
                {
                    /* nothing to do ... */
                }
            }
            else
            {

                /* ���ο� �αװ� ���� */ 
                /* nothing to do ... */
                 

            }
        }
        else //remote log�� ���
        {
            mRemoteLogMgr.getLstLSN( &sLastLogLSN ); 
            if ( !(smrCompareLSN::isEQ( &sLastLogLSN, &mReadInfo.mLstLogLSN ) ) )
            {
                /*mReadLSN�� ����Ű�� Log�� �о mArrReadInfo�� �����Ѵ�.*/
                IDE_TEST( mRemoteLogMgr.readLogAndValid(
                                         &(mReadInfo.mDecompBufferHandle ),
                                         &(mReadInfo.mReadLSN),
                                         ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                         &(mReadInfo.mLogFilePtr),
                                         &(mReadInfo.mLogHead),
                                         &(mReadInfo.mLogPtr),
                                         &(mReadInfo.mIsValid),
                                         &(mReadInfo.mLogSizeAtDisk) )
                          != IDE_SUCCESS );
            
                if ( ( mReadInfo.mIsLogSwitch == ID_TRUE ) &&
                     ( sPreReadFileCnt != 0 ) )
                {
                    IDE_TEST( mPreReadLFThread.closeLogFile( mReadInfo.mReadLSN.mFileNo )
                              != IDE_SUCCESS );

                    /* Prefetch Thread�� ������ �̸� �α������� �о���δ�.*/
                    IDE_TEST( mPreReadLFThread.addOpenLFRequest(
                                                     mReadInfo.mReadLSN.mFileNo +
                                                     sPreReadFileCnt)
                              != IDE_SUCCESS );
                    mReadInfo.mIsLogSwitch = ID_FALSE;
                }                    
                else
                {
                    /* nothing to do ... */
                }
                
                if ( mReadInfo.mIsValid == ID_TRUE )
                {
                    IDE_ASSERT( sOverflow == ID_FALSE );

#ifdef DEBUG
                    sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
                    IDE_DASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif
                    SM_GET_LSN( mReadInfo.mLstLogLSN, mReadInfo.mReadLSN );
                    sQueueData = (void*)(&mReadInfo); //BUG-21726
            
                    mPQueueRedoInfo.enqueue( (void*)&sQueueData, &sOverflow );
                    *aIsExistLogTail = ID_FALSE;
                }
                else
                {
                    /* nothing to do ... */
                }
            }
            else
            {
                /* nothing to do ... */
            }
        }
    }
    else
    {
        *aIsExistLogTail = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���������� ������������ �о��� �αװ� ��� sync�Ǿ�����
 *               Check �Ѵ�.
 *
 * aIsSynced - [OUT] ��� Sync�� �Ǹ� ID_TRUE, �ƴϸ� ID_FALSE
 ***********************************************************************/
IDE_RC smiReadLogByOrder::isAllReadLogSynced( idBool *aIsSynced )
{
    smLSN sSyncedLSN;
    smLSN sSyncLSN;
    
    IDE_DASSERT( aIsSynced != NULL );

    *aIsSynced = ID_TRUE;

    IDE_TEST( smrLogMgr::getLFThread().getSyncedLSN( &sSyncedLSN )
              != IDE_SUCCESS );

    sSyncLSN = mReadInfo.mReadLSN;

    if ( mCurReadInfoPtr == &mReadInfo )
    {
        if ( mCurReadInfoPtr->mIsValid == ID_TRUE )
        {
            /* BUGBUG: Sender�� ���� �α��̱⶧���� */
            sSyncLSN.mOffset +=  mCurReadInfoPtr->mLogSizeAtDisk;
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        /* nothing to do ... */
    }

    if ( smrCompareLSN::isLT( &(sSyncedLSN),
                              &(sSyncLSN) )
         == ID_TRUE )
    {
        *aIsSynced = ID_FALSE;
    }
    else
    {
        /* nothing to do ... */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : ���������� ��������LSN�� �޾Ƽ� ���� �α׺���
 *               ���� �� �ֵ��� �ʱ�ȭ �Ѵ�.
 *               PROJ-1670 replication log buffer���� �α׸� ���� ��
 *               �� Ŭ������ �ƴ� rpdLogBufferMgr Ŭ������ ����
 *               sender�� �α׸� ���� �� �ְԵǾ���.
 *              �׷���, rpdLogBufferMgr���� ��� �αװ� �������� �ʱ� ������
 *               bufferMgr�� ���� �α��� ��� �� Ŭ������ ���� �о�� �Ѵ�.
 *               �׷��� ���������� ���� LSN�� ���� ���� �� �ִ�
 *               �Լ��� �߰��ϰ� �Ǿ���.
 * aLstReadLSN - [IN] ���������� ���� LSN
 ***********************************************************************/
IDE_RC smiReadLogByOrder::startByLSN( smLSN aLstReadLSN )
{
    UInt  sFileNo    = 0;
    UInt  i          = 0;
#ifdef DEBUG
    smLSN sReadLSN;
#endif

    SM_GET_LSN( mReadInfo.mReadLSN, aLstReadLSN );
    mReadInfo.mLogFilePtr  = NULL;
    mReadInfo.mIsValid     = ID_TRUE;
    SM_LSN_MAX( mReadInfo.mLstLogLSN );
    mReadInfo.mIsLogSwitch = ID_FALSE;

    if ( mNotDecomp == ID_FALSE )
    {
        /* ���� ���������� ���� �α��� LSN(mReadLSN)��
         * �ش��ϴ� ������ ���� �α׸� �д´�.
         * �о�� �ϴ� �α״� ���� �α��̴�.*/
        IDE_TEST( smrLogMgr::readLog( &(mReadInfo.mDecompBufferHandle ),
                                      &(mReadInfo.mReadLSN),
                                      ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                      &(mReadInfo.mLogFilePtr),
                                      &(mReadInfo.mLogHead),
                                      &(mReadInfo.mLogPtr),
                                      &(mReadInfo.mIsValid),
                                      &(mReadInfo.mLogSizeAtDisk) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* ���� ���������� ���� �α��� LSN(mReadLSN)��
         * �ش��ϴ� ������ ���� �α׸� �д´�.
         * �о�� �ϴ� �α״� ���� �α��̴�.
         * ����� �α������� ��� ������ Ǯ���ʰ� ��ȯ�Ѵ�. */

        IDE_TEST( smrLogMgr::readLog4RP( &(mReadInfo.mReadLSN),
                                         ID_TRUE, /* Close Log File When aLogFile doesn't include aLSN */
                                         &(mReadInfo.mLogFilePtr),
                                         &(mReadInfo.mLogHead),
                                         &(mReadInfo.mLogPtr),
                                         &(mReadInfo.mIsValid),
                                         &(mReadInfo.mLogSizeAtDisk) )
                  != IDE_SUCCESS );
    }

#ifdef DEBUG
    sReadLSN = smrLogHeadI::getLSN( &mReadInfo.mLogHead ); 
    IDE_DASSERT( smrCompareLSN::isEQ( &mReadInfo.mReadLSN, &sReadLSN ) );
#endif
    SM_GET_LSN( mReadInfo.mLstLogLSN, mReadInfo.mReadLSN );    
    SM_GET_LSN( mLstReadLogLSN, mReadInfo.mReadLSN );
    mCurReadInfoPtr = &mReadInfo;

    sFileNo = mReadInfo.mReadLSN.mFileNo + 1;

    if ( mPreReadFileCnt != 0 )
    {
        for ( i = 0 ; i < mPreReadFileCnt ; i++ )
        {
            IDE_TEST( mPreReadLFThread.addOpenLFRequest( sFileNo )
                      != IDE_SUCCESS );
            sFileNo++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : ��� ���� �α������� close �Ѵ�.
 *               sender�� �α����� �д� ���� �ߴ��� �� ����Ѵ�.
 *               PROJ-1670 replication log buffer-- startByLSN����
 ***********************************************************************/
IDE_RC smiReadLogByOrder::stop()
{
    UInt    i           = 0;
    UInt    sFileNo;
    void  * sQueueData;
    idBool  sUnderflow  = ID_FALSE;

    /* PROJ-1670 replication log buffer�� ����� ���� ȣ��Ǿ�� �Ѵ�. */
    mPQueueRedoInfo.dequeue( (void*)&sQueueData, &sUnderflow );

    if ( mReadInfo.mLogFilePtr != NULL )
    {
        IDE_TEST( smrLogMgr::closeLogFile( mReadInfo.mLogFilePtr )
                  != IDE_SUCCESS );
        mReadInfo.mLogFilePtr = NULL;
    }
    else
    {
        /* nothing to do ... */
    }

    if ( mPreReadFileCnt != 0 )
    {
        sFileNo = mReadInfo.mReadLSN.mFileNo + 1;

        for ( i = 0 ; i < mPreReadFileCnt ; i++ )
        {
            IDE_TEST( mPreReadLFThread.closeLogFile( sFileNo )
                      != IDE_SUCCESS );
            sFileNo++;
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Read ������ �����Ѵ�.
 *
 * aReadLSN    - [OUT] Read LSN�� ������ �޸�
 */
void smiReadLogByOrder::getReadLSN( smLSN     * aReadLSN )
{
    IDE_DASSERT( aReadLSN != NULL );
    SM_GET_LSN( *aReadLSN, mReadInfo.mReadLSN );
    return;
}

/**********************************************************************
 * PROJ-1915 
 * Description : �������� �α׿��� ���������� Log�� ����ϱ� ���� ���� SN���� �����Ѵ�.
 *
 * aSN  - [OUT] output parameter
 **********************************************************************/
IDE_RC smiReadLogByOrder::getRemoteLastUsedGSN( smSN * aSN )
{
    smSN  sRetSN = 0;
    smLSN sTmpLSN;

    IDE_ASSERT( mIsRemoteLog == ID_TRUE ); //remote log���� �Ѵ�.

    mRemoteLogMgr.getLstLSN( &sTmpLSN );
    if ( !SM_IS_LSN_INIT( sTmpLSN ) )
    {
        sRetSN = SM_MAKE_SN( sTmpLSN );
    }
    else
    {
        /* nothing to do ... */
    }
    *aSN = sRetSN;

    return IDE_SUCCESS;
}

idBool smiReadLogByOrder::isCompressedLog( SChar * aRawLog )
{
    return smrLogComp::isCompressedLog( aRawLog );
}

smOID smiReadLogByOrder::getTableOID( SChar * aRawLog )
{
    return smrLogComp::getTableOID( aRawLog );
}

IDE_RC smiReadLogByOrder::decompressLog( SChar      * aCompLog,
                                         smLSN      * aReadLSN,
                                         smiLogHdr  * aLogHead,
                                         SChar     ** aLogPtr )
{
    UInt sDummySize = 0;

    IDE_TEST( smrLogComp::decompressCompLog( &( mReadInfo.mDecompBufferHandle ),
                                             aReadLSN->mOffset,
                                             aCompLog,
                                             smrLogFile::makeMagicNumber( aReadLSN->mFileNo,
                                                                          aReadLSN->mOffset ),
                                             aLogHead,
                                             aLogPtr,
                                             &sDummySize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
