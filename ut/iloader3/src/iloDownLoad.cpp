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
 * $Id: iloDownLoad.cpp 89537 2020-12-16 05:50:38Z chkim $
 **********************************************************************/

#include <ilo.h>
#include <idtBaseThread.h>

iloDownLoad::iloDownLoad( ALTIBASE_ILOADER_HANDLE aHandle )
: m_DataFile(aHandle)
{
    m_pProgOption = NULL;
    m_pISPApi = NULL;
}

SInt iloDownLoad::GetTableTree(ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    sHandle->mParser.mDateForm[0] = 0;

    IDE_TEST_RAISE( m_FormCompiler.FormFileParse( sHandle, m_pProgOption->m_FormFile)
                    == SQL_FALSE, err_open );

    m_TableTree.SetTreeRoot(sHandle->mParser.mTableNodeParser);

    if ( idlOS::getenv(ENV_ILO_DATEFORM) != NULL )
    {
        idlOS::strcpy( sHandle->mParser.mDateForm, idlOS::getenv(ENV_ILO_DATEFORM) );
    }

    // BUG-24355: -silent �ɼ��� ���� ��쿡�� ���
    if (( sHandle->mProgOption->m_bExist_Silent != SQL_TRUE ) && 
            ( sHandle->mUseApi != SQL_TRUE ))
    {
        if ( sHandle->mParser.mDateForm[0] != 0 )
        {
            idlOS::printf("DATE FORMAT : %s\n", sHandle->mParser.mDateForm);
        }

        //==========================================================
        // proj1778 nchar: just for information
        idlOS::printf("DATA_NLS_USE: %s\n", sHandle->mProgOption->m_DataNLS);
        if ( sHandle->mProgOption->mNCharColExist == ILO_TRUE )
        {
            if ( sHandle->mProgOption->mNCharUTF16 == ILO_TRUE)
            {
                idlOS::printf("NCHAR_UTF16 : %s\n", "YES");
            }
            else
            {
                idlOS::printf("NCHAR_UTF16 : %s\n", "NO");
            }
        }
        idlOS::fflush(stdout);
    }

#ifdef _ILOADER_DEBUG
    m_TableTree.PrintTree();
#endif

    return SQL_TRUE;

    IDE_EXCEPTION( err_open );
    {
    }
    IDE_EXCEPTION_END;

    if (uteGetErrorCODE(sHandle->mErrorMgr) != 0x00000)
    {
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }

    if ( m_pProgOption->m_bExist_log )
    {
        /* BUG-47652 Set file permission */
        if ( m_LogFile.OpenFile(m_pProgOption->m_LogFile, 
                    m_pProgOption->IsExistFilePerm()) == SQL_TRUE )
        {
            // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
            // ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�
            m_LogFile.PrintLogErr(sHandle->mErrorMgr);
            m_LogFile.CloseFile();
        }
    }

    return SQL_FALSE;
}

SInt iloDownLoad::GetTableInfo( ALTIBASE_ILOADER_HANDLE aHandle )
{

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    IDE_TEST(m_TableInfo.GetTableInfo( sHandle, m_TableTree.GetTreeRoot()) 
                                        != SQL_TRUE);
    
    m_TableTree.FreeTree();
#ifdef _ILOADER_DEBUG
    m_TableInfo.PrintTableInfo();
#endif

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

IDE_RC iloDownLoad::InitFiles(iloaderHandle *sHandle,
                              SChar         *sTableName)
{
    iloBool sLOBColExist;
    SChar   sMsg[4096];

    sLOBColExist = IsLOBColExist();

    m_DataFile.SetTerminator(m_pProgOption->m_FieldTerm,
                             m_pProgOption->m_RowTerm);
    m_DataFile.SetEnclosing(m_pProgOption->m_bExist_e,
                            m_pProgOption->m_EnclosingChar);
                            
    if (sLOBColExist == ILO_TRUE)
    {
        m_DataFile.SetLOBOptions(m_pProgOption->mUseLOBFile,
                                 m_pProgOption->mLOBFileSize,
                                 m_pProgOption->mUseSeparateFiles,
                                 m_pProgOption->mLOBIndicator);
    }
    
    m_DataFileCnt    = 0;      //File��ȣ �ʱ�ȭ
    m_LoadCount      = 0;
    mTotalCount      = 0;
    
    /* BUG-30413 */
    m_CBFrequencyCnt = sHandle->mProgOption->mSetRowFrequency; 
    idlOS::memcpy( sHandle->mStatisticLog.tableName,
                   sTableName,
                   ID_SIZEOF(sHandle->mStatisticLog.tableName));

    if (m_pProgOption->m_bExist_log)
    {
        /* BUG-47652 Set file permission */
        IDE_TEST( m_LogFile.OpenFile( m_pProgOption->m_LogFile,
                                      m_pProgOption->IsExistFilePerm()) != SQL_TRUE );

        /* BUG-32114 aexport must support the import/export of partition tables.
         * ILOADER IN/OUT TABLE NAME�� PARTITION �ϰ�� PARTITION NAME���� ���� */
        if( sHandle->mProgOption->mPartition == ILO_TRUE )
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            idlOS::sprintf(sMsg, "<Data DownLoad>\nTableName : %s / %s\n",
                    sTableName,
                    sHandle->mParser.mPartitionName);
        }
        else
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            idlOS::sprintf(sMsg, "<Data DownLoad>\nTableName : %s\n",
                    sTableName);
        }

        m_LogFile.PrintLogMsg(sMsg);
        m_LogFile.PrintTime("Start Time");
        m_LogFile.SetTerminator(m_pProgOption->m_FieldTerm,
                                m_pProgOption->m_RowTerm);
    }
    else
    {
        if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
        {
            idlOS::memcpy( sHandle->mLog.tableName,
                           sTableName,
                           ID_SIZEOF(sHandle->mLog.tableName));

        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloDownLoad::RunThread(iloaderHandle *sHandle)
{
    SInt   i;

    //PROJ-1714
    idtThreadRunner sDownloadThread_id[MAX_PARALLEL_COUNT];       //BUG-22436
    IDE_RC          sDownloadThread_status[MAX_PARALLEL_COUNT];

    //
    // Download Thread ����
    //
    
    idlOS::thread_mutex_init( &(sHandle->mParallel.mDownLoadMutex) );
    
    for(i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        //BUG-22436 - ID �Լ��� ����..
        sDownloadThread_status[i] = sDownloadThread_id[i].launch(
                iloDownLoad::DownloadToFile_ThreadRun, sHandle);
    }

    for(i = 0; i <  m_pProgOption->m_ParallelCount; i++)
    {
        if ( sDownloadThread_status[i] != IDE_SUCCESS )
        {
            sHandle->mThreadErrExist = ILO_TRUE;
        }
    }
    
    for(i = 0; i < m_pProgOption->m_ParallelCount; i++)
    {
        if ( sDownloadThread_status[i] == IDE_SUCCESS )
        {
            //BUG-22436 - ID �Լ��� ����..
            sDownloadThread_status[i]  = sDownloadThread_id[i].join();
        }
    }
    
    for(i = 0; i <  m_pProgOption->m_ParallelCount; i++)
    {
        IDE_TEST( sDownloadThread_status[i] != IDE_SUCCESS );
    }        

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iloDownLoad::PrintMessages(iloaderHandle *sHandle,
                                  SChar         *sTableName)
{
    SChar  sMsg[4096];

    // bug-17865
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        /* BUG-32114 aexport must support the import/export of partition tables.
         * ILOADER IN/OUT TABLE NAME�� PARTITION �ϰ�� PARTITION NAME���� ���� */
        if( sHandle->mProgOption->mPartition == ILO_TRUE )
        {
            idlOS::printf("\n     Total %d record download(%s / %s)\n", 
                    mTotalCount,
                    sTableName,
                    sHandle->mParser.mPartitionName );
        }
        else
        {
            /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
            idlOS::printf("\n     Total %d record download(%s)\n", 
                    mTotalCount,
                    sTableName);
        }
    }

    if (m_pProgOption->m_bExist_log)
    {
        m_LogFile.PrintTime("End Time");
        idlOS::sprintf(sMsg, "Total Row Count : %d\n"
                             "Load Row Count  : %d\n"
                             "Error Row Count : %d\n", mTotalCount, mTotalCount, 0);
        m_LogFile.PrintLogMsg(sMsg);
    }

    if ( sHandle->mUseApi != SQL_TRUE )
    {
        idlOS::printf("\n");
    }

    return IDE_SUCCESS;
}

SInt iloDownLoad::DownLoad( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SChar  sTableName[MAX_OBJNAME_LEN];
   
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    enum
    {
        NoDone = 0, GetTableTreeDone, GetTableInfoDone, ExecDone,
        LogFileOpenDone, DataFileCloseForReopenDone
    } sDone = NoDone;

    // parse formout file
    IDE_TEST(GetTableTree(sHandle) != SQL_TRUE);
    sDone = GetTableTreeDone;

    //======================================================
    /* proj1778 nchar
       after parsing fomrout file,
       we can know data_nls_use
       download�ÿ��� ��Ȯ�� ó���� ���Ͽ�
       formout ���� ������ ����� DATA_NLS_USE ����
       download�ÿ� ���� ������ ALTIBASE_NLS_USE���� Ʋ����
       ����ó���ϵ��� �Ѵ�
       */

    IDE_TEST_RAISE(idlOS::strcasecmp( sHandle->mProgOption->m_NLS,
                sHandle->mProgOption->m_DataNLS) != 0, NlsUseError);

    IDE_TEST(GetTableInfo(sHandle) != SQL_TRUE);
    sDone = GetTableInfoDone;

    /* BUG-43277 -prefetch_rows option
     *  SQLSetStmtAttr function must be called with default value 0
     *  even if -prefetch_rows option is not specified.
     *  This is because the PrefetchRows value is still applied 
     *  once it is set.
     */
    IDE_TEST_RAISE(
            m_pISPApi->SetPrefetchRows(sHandle->mProgOption->m_PrefetchRows)
        != IDE_SUCCESS, SetStmtAttrError);

    /* BUG-44187 Support Asynchronous Prefetch */
    IDE_TEST_RAISE(
            m_pISPApi->SetAsyncPrefetch(sHandle->mProgOption->m_AsyncPrefetchType)
        != IDE_SUCCESS, SetStmtAttrError);

    IDE_TEST(GetQueryString(sHandle) != SQL_TRUE);
    IDE_TEST_RAISE(ExecuteQuery(sHandle) != SQL_TRUE, ExecError);
    sDone = ExecDone;
    
    // compare formout file column count and real column count
    IDE_TEST(CompareAttrType() != SQL_TRUE);
    
    m_TableInfo.GetTransTableName(sTableName, (UInt)MAX_OBJNAME_LEN);

    IDE_TEST(InitFiles(sHandle, sTableName) != IDE_SUCCESS);
    sDone = LogFileOpenDone;
    
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        idlOS::printf("     ");
    }
    
    IDE_TEST(RunThread(sHandle) != IDE_SUCCESS);
    
    (void)PrintMessages(sHandle, sTableName);

    if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
    {
        sHandle->mLog.totalCount = mTotalCount;
        sHandle->mLog.loadCount  = mTotalCount;
        sHandle->mLog.errorCount = 0;

        sHandle->mLogCallback( ILO_LOG, &sHandle->mLog );
    }

    m_TableInfo.Reset();
    (void)m_pISPApi->StmtInit();
    (void)m_pISPApi->EndTran(ILO_TRUE);
    (void)m_pISPApi->m_Column.AllFree();
    idlOS::thread_mutex_destroy( &(sHandle->mParallel.mDownLoadMutex) );
    if (m_pProgOption->m_bExist_log)
    {
        (void)m_LogFile.CloseFile();
    }

    return SQL_TRUE;

    IDE_EXCEPTION(ExecError);
    {
        (void)m_pISPApi->EndTran(ILO_FALSE);
    }
    IDE_EXCEPTION(NlsUseError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Nls_Use_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION(SetStmtAttrError);
    {
        if (uteGetErrorCODE( sHandle->mErrorMgr) != 0x00000)
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    if (sDone == GetTableTreeDone)
    {
        m_TableTree.FreeTree();
    }

    if (sDone >= GetTableInfoDone)
    {
        m_TableInfo.Reset();

        if (sDone >= ExecDone)
        {
            (void)m_pISPApi->StmtInit();
            (void)m_pISPApi->EndTran(ILO_FALSE);
            (void)m_pISPApi->m_Column.AllFree();

            if (sDone >= LogFileOpenDone)
            {
                idlOS::thread_mutex_destroy( &(sHandle->mParallel.mDownLoadMutex) );
                if (m_pProgOption->m_bExist_log)
                {
                    (void)m_LogFile.CloseFile();
                }
            }
        }
    }
    

    return SQL_FALSE;
}


/* PROJ-1714
 * ReadFileToBuff()�� Thread�� ����ϱ� ���ؼ� ȣ��Ǵ� �Լ�
 */

void* iloDownLoad::DownloadToFile_ThreadRun(void *arg)
{
    iloaderHandle *sHandle = (iloaderHandle *) arg;
    
    sHandle->mDownLoad->DownloadToFile(sHandle);
    return 0;
}

void iloDownLoad::DownloadToFile( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SInt          sLoad;
    SInt          sCount;
    SInt          sArrayNum;
    iloBool       sLOBColExist; 
    FILE         *sWriteFp = NULL;
    iloColumns    spColumn;
    SQLRETURN     sSqlRC;
    //BUG-24583
    SInt          sI;
    SChar         sTmpFilePath[MAX_FILEPATH_LEN];
    SChar         sTableName[MAX_OBJNAME_LEN];
    SChar         sColName[MAX_OBJNAME_LEN];
    
    SInt           sDownLoadCount = 0;
    PDL_Time_Value sSleepTime;
    SInt           sCBDownLoadCount = 0;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    iloMutexLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );

    sLOBColExist = IsLOBColExist();  
      
    /* BUG-24583
     * -lob 'use_separate_files=yes'�� ���, 
     * �ش� ��θ� �����ϰ�, LOB �����͸� ������ Directory�� �����Ѵ�.
     */
    if (sLOBColExist == ILO_TRUE)
    {
        if ( m_pProgOption->mUseSeparateFiles == ILO_TRUE )
        {
            /* BUGBUG
             * mkdir�� false�� ������ �� �ִ� ���� 
             * ���丮�� �������� ������ ��, �����Ϸ��� ���丮�� �̹� ������������� �̴�. 
             * �� �ΰ����� �������� ���ϱ� ������ ���� ���� ó������ �ʴ´�.
             */
             
            /* Directory���� : Download�� ����θ��� ���� 
             *  [TableName]/[ColumnName]/
             */
             
            //Table Directory ����
            (void)m_TableInfo.GetTransTableName(sTableName, (UInt)MAX_OBJNAME_LEN);
            (void)idlOS::mkdir(sTableName, 0755);
            
            for( sI = 0; sI < m_TableInfo.GetAttrCount(); sI++ )
            {
                if( (m_TableInfo.GetAttrType(sI) == ISP_ATTR_CLOB) ||
                    (m_TableInfo.GetAttrType(sI) == ISP_ATTR_BLOB) )
                {
                    //Column �� Directory ����
                    (void)idlOS::sprintf(sTmpFilePath,
                                         "%s/%s",
                                         sTableName,
                                         m_TableInfo.GetTransAttrName(sI,sColName,(UInt)MAX_OBJNAME_LEN)
                                        );
                    (void)idlOS::mkdir(sTmpFilePath, 0755);
                }
            }
        }
    }

    /* PROJ-1714
     * Parallel �ɼ��� ����� Download�� ���, 
     * Parallel �ɼǿ� ������ ����ŭ File�� ���� �ٸ��̸����� �����ȴ�.    
     */
    if ( m_pProgOption->m_bExist_split == ILO_TRUE || 
         ( m_pProgOption->m_ParallelCount > 1 &&  sLOBColExist != ILO_TRUE ) )
    {
        sWriteFp = m_DataFile.OpenFileForDown( sHandle, 
                                               m_pProgOption->GetDataFileName(ILO_FALSE), 
                                               m_DataFileCnt++,
                                               ILO_TRUE,
                                               sLOBColExist);
    }
    else
    {
        sWriteFp = m_DataFile.OpenFileForDown( sHandle,
                                               m_pProgOption->GetDataFileName(ILO_FALSE), 
                                               -1,
                                               ILO_TRUE,
                                               sLOBColExist);
    }
    
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );

    IDE_TEST(sWriteFp == NULL); // BUG-43432

    spColumn.m_ArrayCount = m_pProgOption->m_ArrayCount;
    m_pISPApi->DescribeColumns(&spColumn);
    
    sLoad = 1;              //BIND������ ���θ� �Ǵ��ϱ� ���� �����..
    
    while(1)
    {
        iloMutexLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
        
        /* PROJ-1714
         * Parallel�� ������� ���� ���, Bind�� �ѹ��� ���ָ� �ȴ�. (Array Fetch �� ��쵵 ����������)
         * Parallel�� ����� ���, �� Thread���� Fetch�ϱ� �� Bind�� iloColumn��ü�� ������� �Ѵ�.
         * �̰��� -Array�� ������� �ʴ� -Parallel �� ��쿡 ���� ���ϸ� ������ �� �ִ�. 
         * ������, -Array�� ����� ��쿡�� -Parallel�� ���� ����� �������� �ȴ�.
         * ����, -Parallel �� ����� ��쿡�� -Array �� ����ϵ��� �����ؾ� �Ѵ�.
         */
        if ( (sLoad == 1) || (m_pProgOption->m_ParallelCount > 1) )
        {
        /* BUG-24358 iloader Geometry Type support */
            IDE_TEST( m_pISPApi->BindColumns( sHandle,
                                              &spColumn,
                                              &m_TableInfo) != ILO_TRUE );
        }

        /* BUG-30413 */
        if (( sHandle->mUseApi == SQL_TRUE ) &&
                ( sHandle->mLogCallback != NULL ))
        {
            if ( sHandle->mProgOption->mStopIloader == ILO_TRUE )
            {
                iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
                break;
            }
        }

        sSqlRC = m_pISPApi->Fetch(&spColumn);
        if ( sSqlRC == ILO_FALSE )
        {
            iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
            break;
        }

        /* BUG-30413 */
        if (( sHandle->mUseApi == SQL_TRUE ) &&
                ( sHandle->mLogCallback != NULL ))
        {
            /* dataout ������ ���Ѵ� */
            if( m_pProgOption->m_bExist_array == SQL_TRUE )
            {
                sCBDownLoadCount = (m_LoadCount += spColumn.m_ArrayCount);
            }
            else
            {
                sCBDownLoadCount = (m_LoadCount += 1);
            }

            if (( sHandle->mProgOption->mSetRowFrequency != 0 ) &&
                    ( m_CBFrequencyCnt <= sCBDownLoadCount ))
            {
                m_CBFrequencyCnt += sHandle->mProgOption->mSetRowFrequency;

                sHandle->mStatisticLog.loadCount  = sCBDownLoadCount; 

                if ( sHandle->mLogCallback( ILO_STATISTIC_LOG, &sHandle->mStatisticLog )
                        != ALTIBASE_ILO_SUCCESS )
                { 
                    sHandle->mProgOption->mStopIloader = ILO_TRUE;
                }
            }
        }
    
        iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
     
        if ( spColumn.m_ArrayCount > 1)
        {
            sCount = (SInt)spColumn.m_RowsFetched;
        }
        else
        {
            /* 
             * PROJ-1714
             * LOB�� ��쿡�� ArrayFetch�� ������� �ʴ´�.
             * ����, m_RowsFetched ���� ����� �� ����.
             */
            sCount = 1;
        }
        for (sArrayNum = 0; sArrayNum < sCount; sArrayNum++)
        {
            //LOB Column�� ������ ���, Parallel�� 1�� �ȴ�. ���� Split ���Ǹ� �˻�.
            if (m_pProgOption->m_bExist_split == ILO_TRUE)
            {
                // PrintOneRecord()�� ���ڷ� �ִ� �� ��ȣ��
                // ���� �����ִ� ������ ���� �������� �� ��ȣ�̴�.
                // ���� ��ȣ�� ������ ������ ���� �� ��ȣ�� 1���� �ٽ� ���۵ȴ�.
                // PrintOneRecord()�� ���ڷ� ���� �� ��ȣ�� �ñ�������
                // use_separate_files=yes�� ���� LOB ���ϸ� ���ȴ�. 
                IDE_TEST(m_DataFile.PrintOneRecord( sHandle,
                                                    (sLoad - 1) % m_pProgOption->m_SplitRowCount + 1,
                                                     &spColumn, 
                                                     &m_TableInfo, 
                                                     sWriteFp, 
                                                     sArrayNum) != SQL_TRUE);
            }
            else
            {
                IDE_TEST(m_DataFile.PrintOneRecord( sHandle,
                                                    sLoad, 
                                                    &spColumn,
                                                    &m_TableInfo,
                                                    sWriteFp,
                                                    sArrayNum) != SQL_TRUE);
                
            }
            if ( (m_pProgOption->m_bExist_split == ILO_TRUE) &&
                 (sLoad % m_pProgOption->m_SplitRowCount == 0) )
            {
                (void)m_DataFile.CloseFileForDown( sHandle, sWriteFp );
                // BUG-23118 -split, -parallel�� �Բ� ����� ��� �ٿ�ε尡 ������
                iloMutexLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
                sWriteFp = m_DataFile.OpenFileForDown( sHandle,
                                                      m_pProgOption->GetDataFileName(ILO_FALSE), 
                                                      m_DataFileCnt++,
                                                      ILO_TRUE, 
                                                      sLOBColExist);
                iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );

                IDE_TEST(sWriteFp == NULL); // BUG-43432
            }
            
            sLoad++;  

            //Progress ó��
            if ( sHandle->mUseApi != SQL_TRUE )
            {
                sDownLoadCount = (m_LoadCount += 1);
            
                if ((sDownLoadCount % 100) == 0)
                {        
                    /* BUG-32114 aexport must support the import/export of partition tables.*/
                    PrintProgress( sHandle, sDownLoadCount);
                }
            }

            // bug-18707     
            if ((m_pProgOption->mExistWaitTime == SQL_TRUE) &&
                (m_pProgOption->mExistWaitCycle == SQL_TRUE))
            {
                if ((sDownLoadCount % m_pProgOption->mWaitCycle) == 0)
                {
                    sSleepTime.initialize(0, m_pProgOption->mWaitTime * 1000);
                    idlOS::sleep( sSleepTime );
                }
            }
        } // End of Array-For-Loop
    }
    
    sLoad -= 1;
    
    iloMutexLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
    mTotalCount += sLoad;
    iloMutexUnLock( sHandle, &(sHandle->mParallel.mDownLoadMutex) );
    
    spColumn.AllFree();
    IDE_TEST(m_pISPApi->IsFetchError() == ILO_TRUE);
    (void)m_DataFile.CloseFileForDown( sHandle, sWriteFp );
    sWriteFp = NULL;

    return ;

    IDE_EXCEPTION_END;

    /* BUG-42895 Print error msg when encoutering an error during downloading data. */
    PRINT_ERROR_CODE( sHandle->mErrorMgr );

    if ( m_pProgOption->m_bExist_log )
    {
        m_LogFile.PrintLogErr(sHandle->mErrorMgr);
    }

    spColumn.AllFree();

    if(sWriteFp != NULL)
    {
        (void)m_DataFile.CloseFileForDown( sHandle, sWriteFp ); 
    }

    return ;
}

SInt iloDownLoad::GetQueryString(ALTIBASE_ILOADER_HANDLE aHandle)
{
    SInt   i;
    SChar  sBuffer[1024];
    m_pISPApi->clearQueryString();
   
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    /* BUG-47608 stmt_prefix */
    if ( m_pProgOption->m_bExist_StmtPrefix == ID_TRUE )
    {
        IDE_TEST(m_pISPApi->appendQueryString( (SChar*) m_pProgOption->m_StmtPrefix ) == SQL_FALSE);
        IDE_TEST(m_pISPApi->appendQueryString( (SChar*) " ") == SQL_FALSE);
    }
    else
    {
        // Do nothing
    }

    idlOS::sprintf(sBuffer, "SELECT /*+%s*/ ", m_TableInfo.m_HintString);
    IDE_TEST(m_pISPApi->appendQueryString(sBuffer) == SQL_FALSE);

    for (i=0; i < m_TableInfo.GetAttrCount(); i++)
    {
        if( m_TableInfo.GetAttrType(i) == ISP_ATTR_DATE )
        {
            if ( m_TableInfo.mAttrDateFormat[i] != NULL )
            {
                idlOS::sprintf(sBuffer, "to_char(%s,'%s')",
                               m_TableInfo.GetAttrName(i),
                               m_TableInfo.mAttrDateFormat[i]);
                IDE_TEST(m_pISPApi->appendQueryString(sBuffer) == SQL_FALSE);
            }
            else if ( idlOS::strlen(sHandle->mParser.mDateForm) >= 1 )
            {
                idlOS::sprintf(sBuffer, "to_char(%s,'%s')",
                               m_TableInfo.GetAttrName(i), sHandle->mParser.mDateForm);
                IDE_TEST(m_pISPApi->appendQueryString(sBuffer) == SQL_FALSE);
            }
            else
            {
                IDE_TEST(m_pISPApi->appendQueryString(
                             m_TableInfo.GetAttrName(i)) == SQL_FALSE);
            }
        }
        else if( m_TableInfo.GetAttrType(i) == ISP_ATTR_BIT ||
                 m_TableInfo.GetAttrType(i) == ISP_ATTR_VARBIT )
        {
            idlOS::sprintf(sBuffer, "to_char(%s)", m_TableInfo.GetAttrName(i));
            IDE_TEST(m_pISPApi->appendQueryString(sBuffer)
                     == SQL_FALSE);
        }
        /* BUG-24358 iloader Geometry Type support */
        else if (m_TableInfo.GetAttrType(i) == ISP_ATTR_GEOMETRY)
        {
            /* BUG-48357 WKB compatibility option */
            if (m_pProgOption->m_bExist_geom == ID_TRUE &&
                    m_pProgOption->m_bIsGeomWKB == ID_TRUE)
            {
                idlOS::sprintf(sBuffer, "asbinary(%s)", m_TableInfo.GetAttrName(i));
            }
            else
            {
                /* BUG-47821 SRID support: asbinary -> asewkb */
                idlOS::sprintf(sBuffer, "asewkb(%s)", m_TableInfo.GetAttrName(i));
            }
            IDE_TEST(m_pISPApi->appendQueryString(sBuffer)
                    == SQL_FALSE);
        }
        else
        {
            IDE_TEST(m_pISPApi->appendQueryString(m_TableInfo.GetAttrName(i))
                     == SQL_FALSE);
        }
        IDE_TEST(m_pISPApi->appendQueryString((SChar*)",") == SQL_FALSE);

    }
    m_pISPApi->replaceTerminalChar(' ');

    IDE_TEST(m_pISPApi->appendQueryString((SChar*)" from ") == SQL_FALSE);    
    IDE_TEST(m_pISPApi->appendQueryString(
                 m_TableInfo.GetTableName()) == SQL_FALSE);
    
    /* BUG-30467 */
    if( sHandle->mProgOption->mPartition == ILO_TRUE )
    {
        idlOS::sprintf(sBuffer, " PARTITION (%s)",sHandle->mParser.mPartitionName);
        IDE_TEST(m_pISPApi->appendQueryString(sBuffer) == SQL_FALSE);
    }

    if (m_TableInfo.ExistDownCond() == SQL_TRUE)
    {
        IDE_TEST(m_pISPApi->appendQueryString((SChar*)" ") == SQL_FALSE);
        IDE_TEST(m_pISPApi->appendQueryString(m_TableInfo.GetQueryString()) == SQL_FALSE);
    }
    if (m_TableInfo.mIsQueue != 0)
    {
        // add order by clause
        IDE_TEST(m_pISPApi->appendQueryString((SChar*)" Order by msgid")
                == SQL_FALSE);
    }
    m_TableInfo.FreeDateFormat();

    if (m_pProgOption->m_bDisplayQuery == ILO_TRUE)
    {
        idlOS::printf("DownLoad QueryStr[%s]\n", m_pISPApi->getSqlStatement());
    }
   
    return SQL_TRUE;
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

SInt iloDownLoad::ExecuteQuery( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    IDE_TEST(m_pISPApi->SelectExecute(&m_TableInfo) != SQL_TRUE);
    
    /* PROJ-1714
     * m_Column���� LOB Column�� ������ Ȯ���ϱ� ���� �������� �ִ´�.
     * ���� Bind �� ��ü�� �ƴϴ�. 
     */
     
    IDE_TEST(m_pISPApi->DescribeColumns(&m_pISPApi->m_Column) != SQL_TRUE);
    
    //PROJ-1714
    if( m_pProgOption->m_bExist_parallel != SQL_TRUE )
    {
        m_pProgOption->m_ParallelCount = 1;
    }
    
    if( m_pProgOption->m_bExist_array != SQL_TRUE )
    {
        m_pProgOption->m_ArrayCount = 1;
    }
    
    /* PROJ-1714
     *  LOB column�� ������ ���, Parallel�� ���� 1�� �����Ѵ�.
     *  �̴� Open�Ǿ��ִ� LOB Cursor�� Re-Open ���� �����ϱ� �����̴�.
     *  ��, LOB Column�� ������ ���, Array �� Parallel �� ��� �������� �ʴ´�.
     */ 
    if (IsLOBColExist())
    {
        m_pProgOption->m_ArrayCount = 1;
        m_pProgOption->m_ParallelCount = 1;
    }
    

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    if (uteGetErrorCODE( sHandle->mErrorMgr) != 0x00000)
    {
        utePrintfErrorCode(stdout, sHandle->mErrorMgr);
    }

    return SQL_FALSE;
}

SInt iloDownLoad::CompareAttrType()
{
    IDE_TEST( m_TableInfo.GetAttrCount() != m_pISPApi->m_Column.GetSize() );

    /* �̰��� Form ���Ͽ� ����� ������ Ÿ�԰� ����ڰ� �Է���
     * ������ Ÿ���� ���ϴ� �ڵ带 �����Ѵ�.
     */

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    idlOS::printf("Result Attribute Count is wrong\n");

    return SQL_FALSE;
}

/**
 * IsLOBColExist.
 *
 * �ٿ�ε�� �÷� �� LOB �÷��� �����ϴ��� �˻��Ѵ�.
 */
iloBool iloDownLoad::IsLOBColExist()
{
    SInt        sI;
    SQLSMALLINT sDataType;
    iloBool      sLOBColExist = ILO_FALSE;

    for (sI = m_pISPApi->m_Column.GetSize() - 1; sI >= 0; sI--)
    {
        sDataType = m_pISPApi->m_Column.GetType(sI);
        if (sDataType == SQL_BINARY || sDataType == SQL_BLOB ||
            sDataType == SQL_CLOB || sDataType == SQL_BLOB_LOCATOR ||
            sDataType == SQL_CLOB_LOCATOR)
        {
            /* BUG-24358 iloader Geometry Support */
            if (m_TableInfo.GetAttrType(sI) == ISP_ATTR_GEOMETRY)
            {
                continue;
            }
            else
            {
                sLOBColExist = ILO_TRUE;
                break;
            }
        }
    }

    return sLOBColExist;
}

void iloDownLoad::PrintProgress( ALTIBASE_ILOADER_HANDLE aHandle,
                                 SInt aDownLoadCount )
{
    SChar sTableName[MAX_OBJNAME_LEN];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if ((aDownLoadCount % 5000) == 0)
    {
        m_TableInfo.GetTransTableName(sTableName, (UInt)MAX_OBJNAME_LEN);

        /* BUG-32114 aexport must support the import/export of partition tables.
         * ILOADER IN/OUT TABLE NAME�� PARTITION �ϰ�� PARTITION NAME���� ���� */
        if( sHandle->mProgOption->mPartition == ILO_TRUE )
        {
            idlOS::printf("\n%d record download(%s / %s)\n\n",
                    aDownLoadCount,
                    sTableName,
                    sHandle->mParser.mPartitionName );
        }
        else
        {
            idlOS::printf("\n%d record download(%s)\n\n",
                    aDownLoadCount,
                    sTableName);
        }
    }
    else
    {
        putchar('.');
    }
    idlOS::fflush(stdout);
}
