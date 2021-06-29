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
 * $Id: iloLoadPrepare.cpp 90270 2021-03-21 23:20:18Z bethy $
 **********************************************************************/

#include <ilo.h>
#include <idtBaseThread.h>


/***********************************************************************
 * FILE DESCRIPTION :
 *
 * This file includes the functions that break off from
 * the iloLoad::LoadwithPrepare function.
 **********************************************************************/

IDE_RC iloLoad::InitFiles(iloaderHandle *sHandle)
{
    iloBool       sLOBColExist;
    SChar         szMsg[4096];
    SChar         sTableName[MAX_OBJNAME_LEN];

    sLOBColExist = IsLOBColExist( sHandle, &m_pTableInfo[0] );

    m_DataFile.SetTerminator(m_pProgOption->m_FieldTerm,
                             m_pProgOption->m_RowTerm);
    m_DataFile.SetEnclosing(m_pProgOption->m_bExist_e,
                            m_pProgOption->m_EnclosingChar);
                            
    if (sLOBColExist == ILO_TRUE)
    {
        /* �ε� �� lob_file_size, use_separate_files �ɼ���
         * ����� �Է°��� ����. 
         * ���� ������ BUG-24583���� �������� �ʵ��� ��!!!
         */
        
        m_DataFile.SetLOBOptions(m_pProgOption->mUseLOBFile,
                                 ID_ULONG(0),
                                 m_pProgOption->mUseSeparateFiles, //BUG-24583 
                                 m_pProgOption->mLOBIndicator);
        // -lob 'use_separate_files=yes �� ��쿡 FileInfo ������ ���� �Ҵ�                         
        if ( m_pProgOption->mUseSeparateFiles == ILO_TRUE )
        {
            IDE_TEST(m_DataFile.LOBFileInfoAlloc( sHandle,
                                                  m_pTableInfo->mLOBColumnCount )
                                                  != IDE_SUCCESS);
        }
    }

    IDE_TEST_RAISE(m_DataFile.OpenFileForUp( sHandle,
                                       m_pProgOption->GetDataFileName(ILO_TRUE),
                                       -1,
                                       ILO_FALSE,
                                       sLOBColExist ) != SQL_TRUE,
                   ErrDataFileOpen);

    if (m_pProgOption->m_bExist_log)
    {
        /* BUG-47652 Set file permission */
        IDE_TEST_RAISE( m_LogFile.OpenFile( m_pProgOption->m_LogFile, 
                                            m_pProgOption->IsExistFilePerm() )
                       != SQL_TRUE, ErrLogFileOpen );

        /* TASK-2657 */
        if(  m_pProgOption->mRule == csv )
        {
            (void)m_LogFile.setIsCSV ( ILO_TRUE );
        }

        m_pTableInfo[0].GetTransTableName(sTableName, (UInt)MAX_OBJNAME_LEN);
          
        /* BUG-32114 aexport must support the import/export of partition tables.
         * ILOADER IN/OUT TABLE NAME�� PARTITION �ϰ�� PARTITION NAME���� ���� */
        if( sHandle->mProgOption->mPartition == ILO_TRUE )
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            idlOS::sprintf(szMsg, "<DataLoad>\nTableName : %s / %s\n",
                    sTableName,
                    sHandle->mParser.mPartitionName );
        }
        else
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            idlOS::sprintf(szMsg, "<DataLoad>\nTableName : %s\n",
                    sTableName);
        }

        m_LogFile.PrintLogMsg(szMsg);
        m_LogFile.PrintTime("Start Time");
        m_LogFile.SetTerminator( m_pProgOption->m_FieldTerm,
                                 m_pProgOption->m_RowTerm);
    }

    if (m_pProgOption->m_bExist_bad)
    {
        /* TASK-2657 */
        if( m_pProgOption->mRule == csv )
        {
            (void)m_BadFile.setIsCSV ( ILO_TRUE );
        }
        /* BUG-47652 Set file permission */
        IDE_TEST_RAISE( m_BadFile.OpenFile( m_pProgOption->m_BadFile,
                                            m_pProgOption->IsExistFilePerm() ) 
                        != SQL_TRUE, ErrBadFileOpen );

        m_BadFile.SetTerminator( m_pProgOption->m_FieldTerm,
                                 m_pProgOption->m_RowTerm );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ErrDataFileOpen);
    {
    }
    IDE_EXCEPTION(ErrLogFileOpen);
    {
        (void)m_DataFile.CloseFileForUp(sHandle);
    }
    IDE_EXCEPTION(ErrBadFileOpen);
    {
        (void)m_DataFile.CloseFileForUp(sHandle);
        if (m_pProgOption->m_bExist_log)
        {
            (void)m_LogFile.CloseFile();
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloLoad::Init4Api(iloaderHandle *sHandle)
{
    SChar         sTableName[MAX_OBJNAME_LEN];

    if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
    {
        idlOS::memcpy( sHandle->mLog.tableName,
                       m_pTableInfo[0].GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN),
                       ID_SIZEOF(sHandle->mLog.tableName));

    }
    return IDE_SUCCESS;
}

IDE_RC iloLoad::InitTable(iloaderHandle *sHandle)
{
    SChar         sTableName[MAX_OBJNAME_LEN];

    if (m_pProgOption->m_LoadMode == ILO_REPLACE)
    {
        //Delete SQL�� �ѹ��� �����ϸ��...
        IDE_TEST_RAISE(ExecuteDeleteStmt( sHandle,
                                          &m_pTableInfo[0]) 
                                          != SQL_TRUE, DeleteError);
    }
    // BUG-25010 iloader -mode TRUNCATE ����
    else if (m_pProgOption->m_LoadMode == ILO_TRUNCATE)
    {
        IDE_TEST_RAISE(ExecuteTruncateStmt( sHandle,
                                            &m_pTableInfo[0])
                                            != SQL_TRUE, DeleteError);
    }

    //PROJ-1760
    //No-logging Mode�� ���� ó��
    if ( (m_pProgOption->m_bExist_direct == SQL_TRUE) &&
         (m_pProgOption->m_directLogging == SQL_FALSE) )
    {
        IDE_TEST_RAISE(GetLoggingMode(&m_pTableInfo[0]) != SQL_TRUE, GetError);
        if( m_TableLogStatus == SQL_TRUE )  //Table�� ���� Logging Mode�� ���, No-Logging Mode�� ����
        {
            IDE_TEST_RAISE(ExecuteAlterStmt( sHandle,
                                             &m_pTableInfo[0],
                                             SQL_TRUE )
                                             != SQL_TRUE, AlterError);
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(DeleteError);
    {
        (void)m_pISPApi->EndTran(ILO_FALSE);
        (void)idlOS::printf("Delete Record from Table(%s) Fail\n",
                            m_pTableInfo[0].GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN)
                           );
        if (m_pProgOption->m_bExist_log)
        {
            m_LogFile.PrintLogMsg("Delete Record from Table Fail\n");
        }
    }
    IDE_EXCEPTION(GetError);
    {
        (void)m_pISPApi->EndTran(ILO_FALSE);
        // BUGBUG!
        // GetLoggingMode() ������ ó��...
    
    }
    IDE_EXCEPTION(AlterError);
    {
        (void)m_pISPApi->EndTran(ILO_FALSE);
        (void)idlOS::printf("Alter Table (%s) Logging/NoLogging\n",
                            m_pTableInfo[0].GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN)
                           );
        if (m_pProgOption->m_bExist_log)
        {
            m_LogFile.PrintLogMsg("Alter Table Logging/NoLogging Fail\n");
        }    
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloLoad::InitVariables(iloaderHandle *sHandle)
{
    SChar         sTableName[MAX_OBJNAME_LEN];

    /* PROJ-1714
     * Upload Thread ���� �ʱ�ȭ
     * Connection ��ü �迭 ���� �� Prepare, Bind  ... Etc
     */
     
    idlOS::thread_mutex_init( &(sHandle->mParallel.mLoadInsMutex) );
    idlOS::thread_mutex_init( &(sHandle->mParallel.mLoadFIOMutex) );
    idlOS::thread_mutex_init( &(sHandle->mParallel.mLoadLOBMutex) );
    
    mConnIndex      = 0;
    mLoadCount      = 0;
    mTotalCount     = 0;
    mErrorCount     = 0;
    mReadRecCount   = 0;

    /* BUG-30413 */
    mSetCallbackLoadCnt = 0;
    mCBFrequencyCnt     =  sHandle->mProgOption->mSetRowFrequency;

    /* BUG-30413 
     * ALTIBASE_ILOADER_STATISTIC_LOG�� table Name setting
     * */
    idlOS::memcpy( sHandle->mStatisticLog.tableName,
                   m_pTableInfo[0].GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN),
                   ID_SIZEOF(sHandle->mStatisticLog.tableName) );


    //BUG-24211 �ʱⰪ ����
    // BUG-24816 iloader Hang
    // �����ϱ� ���� ���� �̸� �������� �����尡 �����Ҷ� ���� -1 �Ѵ�.
    sHandle->mParallel.mLoadThrNum = m_pProgOption->m_ParallelCount;
    
    /* Circular Buffer ���� */
    m_DataFile.InitializeCBuff(sHandle);   
    
    return IDE_SUCCESS;
}

IDE_RC iloLoad::InitStmts(iloaderHandle *sHandle)
{
    SInt          i;

    /* Uploading Connection ��ü ���� */
    m_pISPApiUp = new iloSQLApi[m_pProgOption->m_ParallelCount];

    for( i = 0; i < m_pProgOption->m_ParallelCount; i++ )
    {    
        IDE_TEST(m_pISPApiUp[i].InitOption(sHandle) != IDE_SUCCESS);
        
        //BUG-22429 iLoader �ҽ��� CopyContructor�� �����ؾ� ��..
        m_pISPApiUp[i].CopyConstructure(*m_pISPApi);
        
        IDE_TEST(ConnectDBForUpload( sHandle,
                                     &m_pISPApiUp[i],
                                     sHandle->mProgOption->GetServerName(),
                                     sHandle->mProgOption->GetDBName(),
                                     sHandle->mProgOption->GetLoginID(),
                                     sHandle->mProgOption->GetPassword(),
                                     sHandle->mProgOption->GetDataNLS(),
                                     sHandle->mProgOption->GetPortNum(),
                                     GetConnType(),
                                     sHandle->mProgOption->IsPreferIPv6(), /* BUG-29915 */
                                     sHandle->mProgOption->GetSslCa(),
                                     sHandle->mProgOption->GetSslCapath(),
                                     sHandle->mProgOption->GetSslCert(),
                                     sHandle->mProgOption->GetSslKey(),
                                     sHandle->mProgOption->GetSslVerify(),
                                     sHandle->mProgOption->GetSslCipher())
                                     != IDE_SUCCESS);
        
        /* PROJ-1760
         * Parallel Direct-Path Upload �� ��쿡
         * Upload Session���� Parallel DML ������ ����� ��..
         */
        if (m_pProgOption->m_bExist_ioParallel == SQL_TRUE)
        {
            //���ο� Connection��ü�� CopyConstructure�� �ִµ�.. �̰��� ������ ��� ������~ 
            IDE_TEST_RAISE(ExecuteParallelStmt( sHandle,
                                                &m_pISPApiUp[i])
                                                != SQL_TRUE, ParallelDmlError);
        }
        m_pISPApiUp[i].CopySQLStatement(*m_pISPApi);
        
        IDE_TEST_RAISE(m_pISPApiUp[i].Prepare() != SQL_TRUE, PrepareError);
        IDE_TEST(BindParameter( sHandle,
                                &m_pISPApiUp[i],
                                &m_pTableInfo[i]) != SQL_TRUE);  
        IDE_TEST_RAISE(m_pISPApiUp[i].AutoCommit(ILO_FALSE) != IDE_SUCCESS,
                       SetAutoCommitError); 
        IDE_TEST_RAISE(m_pISPApiUp[i].setQueryTimeOut( 0 ) != SQL_TRUE,
                       err_set_timeout);

        /* TASK-7307 */
        if (m_pProgOption->mExistTxLevel == SQL_TRUE)
        {
            IDE_TEST_RAISE(m_pISPApiUp[i].SetGlobalTransactionLevel(
                               m_pProgOption->mTxLevel) != IDE_SUCCESS,
                           SetAutoCommitError); 
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ParallelDmlError);
    {
        for( i = 0; i < m_pProgOption->m_ParallelCount; i++ )
        {
            (void)DisconnectDBForUpload(&m_pISPApiUp[i]);
        }        
        (void)idlOS::printf("ALTER SESSION SET PARALLEL_DML_MODE Fail\n");
        if (m_pProgOption->m_bExist_log)
        {
            m_LogFile.PrintLogMsg("ALTER SESSION SET PARALLEL_DML_MODE Fail\n");
        }
    }
    IDE_EXCEPTION(PrepareError);
    {
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
        
        if (m_pProgOption->m_bExist_log)
        {
            // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
            // ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�
            m_LogFile.PrintLogErr(sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION(SetAutoCommitError);
    {
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
        
        for( i = 0; i < m_pProgOption->m_ParallelCount; i++ )
        {
            (void)DisconnectDBForUpload(&m_pISPApiUp[i]);
        }        
    }
    IDE_EXCEPTION( err_set_timeout );
    {
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloLoad::RunThread(iloaderHandle *sHandle)
{
    SInt           i;

    //PROJ-1714
    idtThreadRunner sReadFileThread_id;                      //BUG-22436
    idtThreadRunner sInsertThread_id[MAX_PARALLEL_COUNT];    //BUG-22436
    IDE_RC          sReadFileThread_status;
    IDE_RC          sInsertThread_status[MAX_PARALLEL_COUNT];

    /* Thread ����. */
      
    //BUG-22436 - ID �Լ��� ����..
    sReadFileThread_status = sReadFileThread_id.launch(
        iloLoad::ReadFileToBuff_ThreadRun, sHandle);

    for(i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        //BUG-22436 - ID �Լ��� ����..
        sInsertThread_status[i] = sInsertThread_id[i].launch(
            iloLoad::InsertFromBuff_ThreadRun, sHandle);
    }
    
    IDE_TEST( sReadFileThread_status != IDE_SUCCESS );
    
    for(i = 0; i <  m_pProgOption->m_ParallelCount; i++)
    {
        if ( sInsertThread_status[i] != IDE_SUCCESS )
        {
            sHandle->mThreadErrExist = ILO_TRUE;
        }
    }

    //BUG-22436 - ID �Լ��� ����..
    sReadFileThread_status  = sReadFileThread_id.join();
    
    for(i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        if ( sInsertThread_status[i] == IDE_SUCCESS )
        {
            sInsertThread_status[i] = sInsertThread_id[i].join();
        }
    }

    IDE_TEST( sReadFileThread_status != IDE_SUCCESS );
    
    for(i = 0; i <  m_pProgOption->m_ParallelCount; i++)
    {
        IDE_TEST( sInsertThread_status[i] != IDE_SUCCESS );
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloLoad::PrintMessages(iloaderHandle *sHandle,
                              uttTime       *a_qcuTimeCheck)
{
    SChar         szMsg[128];
    SChar         sTableName[MAX_OBJNAME_LEN];

    if (( !m_pProgOption->m_bExist_NST ) &&
            ( sHandle->mUseApi != SQL_TRUE ))
    {
        // BUG-24096 : iloader ��� �ð� ǥ��
        a_qcuTimeCheck->showAutoScale4Wall();
    }
    
    // bug-17865
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        m_pTableInfo[0].GetTransTableName(sTableName, (UInt)MAX_OBJNAME_LEN);

        /* BUG-32114 aexport must support the import/export of partition tables.
        * ILOADER IN/OUT TABLE NAME�� PARTITION �ϰ�� PARTITION NAME���� ���� */
        if( sHandle->mProgOption->mPartition == ILO_TRUE )
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            (void)idlOS::printf("\n     Load Count  : %d(%s / %s)", 
                    mTotalCount - mErrorCount,
                    sTableName,
                    sHandle->mParser.mPartitionName );
        }
        else
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            (void)idlOS::printf("\n     Load Count  : %d(%s)", 
                    mTotalCount - mErrorCount,
                    sTableName);
        }

        if (mErrorCount > 0)
        {
            (void)idlOS::printf("\n     Error Count : %d\n", mErrorCount);
        }
        (void)idlOS::printf("\n\n");
    }

    if (m_pProgOption->m_bExist_log)
    {
        m_LogFile.PrintTime("End Time");
        (void)idlOS::sprintf(szMsg, "Total Row Count : %d\n"
                                    "Load Row Count  : %d\n"
                                    "Error Row Count : %d\n",
                             mTotalCount, mTotalCount - mErrorCount, mErrorCount);
        m_LogFile.PrintLogMsg(szMsg);
    }

    return IDE_SUCCESS;
}

IDE_RC iloLoad::Fini4Api(iloaderHandle *sHandle)
{
    if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
    {
        /* BUG-30413 */
        if ( sHandle->mProgOption->mStopIloader != ILO_TRUE )
        {
            sHandle->mLog.totalCount = mTotalCount;    
            sHandle->mLog.loadCount  = mTotalCount - mErrorCount;
            sHandle->mLog.errorCount = mErrorCount;       
        }
        else
        {
            sHandle->mLog.totalCount  = sHandle->mStatisticLog.totalCount;
            sHandle->mLog.loadCount   = sHandle->mStatisticLog.loadCount; 
            sHandle->mLog.errorCount  = sHandle->mStatisticLog.errorCount;
        }
         
        sHandle->mLogCallback( ILO_LOG, &sHandle->mLog ); 
    }
    return IDE_SUCCESS;
}
    
IDE_RC iloLoad::FiniStmts(iloaderHandle * /* sHandle */)
{
    SInt          i;

    //Connection ����
    for ( i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        /* BUG-46048 Codesonar warning */
        (void) m_pISPApiUp[i].StmtInit();
        (void) DisconnectDBForUpload(&m_pISPApiUp[i]);
    }
    delete[] m_pISPApiUp;
    m_pISPApiUp = NULL;

    return IDE_SUCCESS;
}

IDE_RC iloLoad::FiniVariables(iloaderHandle *sHandle)
{
    //���� ���� ����
    m_DataFile.FinalizeCBuff(sHandle);

    idlOS::thread_mutex_destroy( &(sHandle->mParallel.mLoadInsMutex) );
    idlOS::thread_mutex_destroy( &(sHandle->mParallel.mLoadFIOMutex) );
    idlOS::thread_mutex_destroy( &(sHandle->mParallel.mLoadLOBMutex) );

    return IDE_SUCCESS;
}

IDE_RC iloLoad::FiniTables(iloaderHandle *sHandle)
{
    SChar         sTableName[MAX_OBJNAME_LEN];

    //PROJ-1760
    //Table�� Logging Mode�� ���� ���·� �����Ѵ�.
    if ( (m_pProgOption->m_bExist_direct == SQL_TRUE) &&
         (m_pProgOption->m_directLogging == SQL_FALSE) &&
         (m_TableLogStatus == SQL_TRUE) )
    {
        IDE_TEST_RAISE(ExecuteAlterStmt( sHandle,
                                         &m_pTableInfo[0],
                                         SQL_FALSE )
                                         != SQL_TRUE, AlterError);
    }

    (void)m_pISPApi->StmtInit();

    return IDE_SUCCESS;

    IDE_EXCEPTION(AlterError);
    {
        (void)m_pISPApi->EndTran(ILO_FALSE);
        (void)idlOS::printf("Alter Table (%s) Logging/NoLogging\n",
                            m_pTableInfo[0].GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN)
                           );
        if (m_pProgOption->m_bExist_log)
        {
            m_LogFile.PrintLogMsg("Alter Table Logging/NoLogging Fail\n");
        }    
    }    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloLoad::FiniFiles(iloaderHandle *sHandle)
{
    if (m_pProgOption->m_bExist_bad)
    {
        (void)m_BadFile.CloseFile();
    }

    if (m_pProgOption->m_bExist_log)
    {
        (void)m_LogFile.CloseFile();
    }

    (void)m_DataFile.CloseFileForUp(sHandle);

    return IDE_SUCCESS;
}

IDE_RC iloLoad::FiniTableInfo(iloaderHandle * /* sHandle */)
{
    SInt          i;

    for ( i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        m_pTableInfo[i].Reset();
    }

    delete[] m_pTableInfo;
    m_pTableInfo = NULL;

    return IDE_SUCCESS;
}
