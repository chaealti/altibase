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
 * $Id: iloLoad.cpp 90308 2021-03-24 08:32:25Z donlet $
 **********************************************************************/

#include <ilo.h>
#include <iloLoadInsert.h>

iloLoad::iloLoad( ALTIBASE_ILOADER_HANDLE aHandle )
:m_DataFile(aHandle)
{
    m_pProgOption   = NULL;
    m_pISPApi       = NULL;
    m_pISPApiUp     = NULL;
    m_pTableInfo    = NULL;
    mErrorCount     = 0;
}

SInt iloLoad::GetTableTree( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    sHandle->mParser.mDateForm[0] = 0;

    IDE_TEST_RAISE( m_FormCompiler.FormFileParse( sHandle, m_pProgOption->m_FormFile)
                    != SQL_TRUE, err_open );

    m_TableTree.SetTreeRoot(sHandle->mParser.mTableNodeParser);
    if ( idlOS::getenv(ENV_ILO_DATEFORM) != NULL )
    {
        idlOS::strcpy( sHandle->mParser.mDateForm, idlOS::getenv(ENV_ILO_DATEFORM) );
    }

    // BUG-24355: -silent �ɼ��� ���� ��쿡�� ���
    if (( sHandle->mProgOption->m_bExist_Silent != SQL_TRUE) &&
            ( sHandle->mUseApi != SQL_TRUE ))
    {
        if ( sHandle->mParser.mDateForm[0] != 0 )
        {
            idlOS::printf("DATE FORMAT : %s\n", sHandle->mParser.mDateForm);
        }

        //==========================================================
        // proj1778 nchar: just for information
        idlOS::printf("DATA_NLS_USE: %s\n", sHandle->mProgOption->m_DataNLS);
        if (sHandle->mProgOption->mNCharColExist == ILO_TRUE)
        {
            if (sHandle->mProgOption->mNCharUTF16 == ILO_TRUE)
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

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    if ( m_pProgOption->m_bExist_log )
    {
        /* BUG-47652 Set file permission */
        if ( m_LogFile.OpenFile(m_pProgOption->m_LogFile,
                    m_pProgOption->IsExistFilePerm()) == SQL_TRUE )
        {
            // BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
            // ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�
            m_LogFile.PrintLogErr( sHandle->mErrorMgr);
            m_LogFile.CloseFile();
        }
    }

    return SQL_FALSE;
}

SInt iloLoad::GetTableInfo( ALTIBASE_ILOADER_HANDLE  aHandle)
{
    SInt           i = 0;
    SInt           sExistBadLog = SQL_FALSE;
    iloTableInfo  *sTableInfo = NULL;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    sExistBadLog = m_pProgOption->m_bExist_bad || m_pProgOption->m_bExist_log;
    
    if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
    {
        sExistBadLog = SQL_TRUE;
    }

    if( m_pProgOption->m_bExist_array != SQL_TRUE )
    {
        m_pProgOption->m_ArrayCount = 1;
    }
    
    //PROJ-1714
    if( m_pProgOption->m_bExist_parallel != SQL_TRUE )
    {
        m_pProgOption->m_ParallelCount = 1;
    }

    //BUG-24294 
    //-Direct �ɼ� ���� -array �� �������� �ʾ��� ��� �ִ밪���� ����
    if( (m_pProgOption->m_bExist_direct == SQL_TRUE) &&
        (m_pProgOption->m_bExist_array  == SQL_FALSE) )
    {
        // ulnSetStmtAttr.cpp���� SQL_ATTR_PARAMSET_SIZE�� MAX�� 
        // ID_USHORT_MAX�� �����ϰ� ����
        m_pProgOption->m_ArrayCount = ID_USHORT_MAX - 1;    
    }        
    
    /* PROJ-1714
     * TABLE ��ü ����
     * Parallel Uploading�� ���Ͽ� Upload�� �ʿ��� TableInfo ��ü�� 
     * Parallel �ɼǿ� ���� ����ŭ �����Ѵ�.
     */
    m_pTableInfo = new iloTableInfo[m_pProgOption->m_ParallelCount];

    //�ּ��� �ϳ��� TableInfo ��ü�� �����ǹǷ�, ù��° ��ü�� ����Ѵ�.
    for ( i = 0; i < m_pProgOption->m_ParallelCount; i++ )
    {
        sTableInfo = &m_pTableInfo[i];
        IDE_TEST(sTableInfo->GetTableInfo( sHandle,
                                           m_TableTree.GetTreeRoot() )
                 != SQL_TRUE);

        /* LOB �÷��� ���� ���ڵ��� INSERT ������ ������ ��
         * LOB �÷����� ���Ϸκ��� ������ �����ϴ� ���� ������ �߻��ߴٰ� ����.
         * �� �� m_ArrayCount �Ǵ� m_CommitUnit�� 1�� �ƴ� ���
         * ������ �߻��� ���ڵ常 �ѹ��ϴ� ���� �Ұ����ϴ�.
         * ����, LOB �÷��� ������ ���
         * m_ArrayCount�� m_CommitUnit�� ������ 1�� �Ѵ�. */
        // PROJ-1518 Atomic Array Insert
        // ���� ������ ������ LOB �÷��� ���� ���ڵ� �� ��� Atomic �� ����Ҽ� ����.
        // PROJ-1714 Parallel iLoader ���� ������.
        // PROJ-1760 Direct-Path Upload �� LOB Column�� �������� �ʴ´�.
        if ( IsLOBColExist( sHandle, sTableInfo ) == ILO_TRUE )
        {
            m_pProgOption->m_ArrayCount      = 1;
            m_pProgOption->m_CommitUnit      = 1;
            m_pProgOption->m_bExist_atomic   = SQL_FALSE;
            m_pProgOption->m_ParallelCount   = 1;           // PROJ-1714
            m_pProgOption->m_bExist_direct   = SQL_FALSE;   // PROJ-1760
            m_pProgOption->m_ioParallelCount = 0;     
        }

        // BUG-24358 iloader Geometry Support.
        IDE_TEST(reallocFileToken( sHandle, sTableInfo ) != IDE_SUCCESS);

        IDE_TEST_RAISE(sTableInfo->AllocTableAttr( sHandle ,
                    m_pProgOption->m_ArrayCount,
                    sExistBadLog ) 
                != SQL_TRUE, AllocTableAttrError);

#ifdef _ILOADER_DEBUG
        sTableInfo->PrintTableInfo();
#endif

    }

    return SQL_TRUE;

    IDE_EXCEPTION(AllocTableAttrError);
    {
        for ( ; i >= 0; i--)
        {
            m_pTableInfo[i].Reset();
        }
    }
    IDE_EXCEPTION_END;

    if (m_pTableInfo != NULL)
    {
        delete[] m_pTableInfo;
        m_pTableInfo = NULL;
    }
    return SQL_FALSE;
}

SInt iloLoad::LoadwithPrepare( ALTIBASE_ILOADER_HANDLE aHandle )
{
    uttTime        s_qcuTimeCheck;
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    enum
    {
        NoDone = 0, GetTableTreeDone, GetTableInfoDone,
        FileOpenDone, ExecDone, VarDone, ConnSQLApi
    } sState = NoDone;

    IDE_TEST(GetTableTree(sHandle) != SQL_TRUE);  // �������� ���ϴ� �κ�.
    sState = GetTableTreeDone;

    IDE_TEST(GetTableInfo( sHandle ) != SQL_TRUE);
    sState = GetTableInfoDone;
    
    IDE_TEST(InitFiles(sHandle) != IDE_SUCCESS);
    sState = FileOpenDone;

    (void)Init4Api(sHandle);

    IDE_TEST(InitTable(sHandle) != IDE_SUCCESS);
    sState = ExecDone;

    IDE_TEST(MakePrepareSQLStatement( sHandle, &m_pTableInfo[0]) != SQL_TRUE);

    if ( sHandle->mUseApi != SQL_TRUE )
    {
        s_qcuTimeCheck.setName("UPLOAD");
        s_qcuTimeCheck.start();
    }
    
    (void)InitVariables(sHandle);
    sState = VarDone;

    IDE_TEST(InitStmts(sHandle) != IDE_SUCCESS);
    sState = ConnSQLApi;
    
    IDE_TEST(RunThread(sHandle) != IDE_SUCCESS);
    
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        idlOS::printf("\n     ");
        s_qcuTimeCheck.finish();
    }
    
    (void)PrintMessages(sHandle, &s_qcuTimeCheck);

    (void)Fini4Api(sHandle);

    (void)FiniStmts(sHandle);
    (void)FiniVariables(sHandle);
    (void)FiniTables(sHandle);
    (void)FiniFiles(sHandle);
    (void)FiniTableInfo(sHandle);
    m_TableTree.FreeTree();

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    switch (sState)
    {
    case ConnSQLApi:
        (void)FiniStmts(sHandle);
    case VarDone:
        (void)FiniVariables(sHandle);
    case ExecDone:
        (void)m_pISPApi->StmtInit();
        (void)m_pISPApi->EndTran(ILO_FALSE);
    case FileOpenDone:
        (void)FiniFiles(sHandle);
    case GetTableInfoDone:
        (void)FiniTableInfo(sHandle);
    case GetTableTreeDone:
        m_TableTree.FreeTree();
        break;
    default:
        break;
    }

    return SQL_FALSE;
}

SInt iloLoad::MakePrepareSQLStatement( ALTIBASE_ILOADER_HANDLE  aHandle,
                                       iloTableInfo            *aTableInfo)
{
    SInt  i = 0;
    SInt  index;
    // BUG-39878 iloader temp buffer size problem
    SChar tmpBuffer[384];

    UInt  secVal;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    aTableInfo->CopyStruct(sHandle);
    IDE_TEST( aTableInfo->seqColChk(sHandle) == SQL_FALSE ||
              aTableInfo->seqDupChk(sHandle) == SQL_FALSE );

    m_pISPApi->clearQueryString();
     
    if (aTableInfo->mIsQueue == 0)
    {
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

        /* 
         * PROJ-1760
         * -direct �ɼ��� ���� ���, DPath Upload HINT�� ���� Query�� �����Ѵ�.
         * -ioParallel �ɼ��� ���� ���, Parallel DPath Upload HINT�� ���� Query�� �����Ѵ�.
         */
        
        if ( m_pProgOption->m_bExist_direct == SQL_TRUE)
        {
            if ( m_pProgOption->m_bExist_ioParallel == SQL_TRUE )
            {
                idlOS::sprintf(tmpBuffer, "INSERT /*+APPEND PARALLEL(%s, %d)*/ INTO %s(",
                               aTableInfo->GetTableName(), 
                               m_pProgOption->m_ioParallelCount, 
                               
                               aTableInfo->GetTableName());
            }
            else
            {
                idlOS::sprintf(tmpBuffer, "INSERT /*+APPEND*/ INTO %s(",
                               aTableInfo->GetTableName());
            }
        }
        else
        {
               /* BUG-30467 */
            if( sHandle->mProgOption->mPartition == ILO_TRUE )
            {
                idlOS::sprintf(tmpBuffer, "INSERT INTO %s PARTITION (%s) (",
                               aTableInfo->GetTableName(), 
                               sHandle->mParser.mPartitionName);                        
            }
            else
            {
                idlOS::sprintf(tmpBuffer, "INSERT INTO %s(",
                               aTableInfo->GetTableName());
            }
        }
    }
    else
    {
        // make enqueue statement
        idlOS::sprintf(tmpBuffer, "ENQUEUE INTO %s(",
                       aTableInfo->GetTableName());
    }
    
    IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);

    for ( i=0; i<aTableInfo->GetAttrCount(); i++)
    {
        if( aTableInfo->mSkipFlag[i] == ILO_TRUE &&
            aTableInfo->GetAttrType(i) != ISP_ATTR_TIMESTAMP )
        {
            continue;
        }
        if ( aTableInfo->GetAttrType(i) == ISP_ATTR_TIMESTAMP &&
             sHandle->mParser.mTimestampType == ILO_TIMESTAMP_DEFAULT )
        {
            continue;
        }
        idlOS::sprintf(tmpBuffer, "%s,", aTableInfo->GetAttrName(i));
        IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);

    }
    m_pISPApi->replaceTerminalChar( ')' );

    IDE_TEST(m_pISPApi->appendQueryString( (SChar*)" VALUES (") == SQL_FALSE);
    
    for ( i=0; i<aTableInfo->GetAttrCount(); i++)
    {
        if ( aTableInfo->GetAttrType(i) == ISP_ATTR_TIMESTAMP )
        {
            if ( sHandle->mParser.mTimestampType == ILO_TIMESTAMP_DEFAULT )
            {
                continue;
            }
            else if ( sHandle->mParser.mTimestampType == ILO_TIMESTAMP_NULL )
            {
                IDE_TEST(m_pISPApi->appendQueryString( (SChar*)"NULL,") == SQL_FALSE);
            }
            else if ( sHandle->mParser.mTimestampType == ILO_TIMESTAMP_VALUE )
            {
                struct tm tval;
                SChar  tmp[5];
                idlOS::memset( &tval, 0, sizeof(struct tm));
                idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal, 4);
                tmp[4] = 0;
                tval.tm_year = idlOS::atoi(tmp) - 1900;
                // add length(YYYY, 4)
                idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal+4, 2);
                tmp[2] = 0;
                tval.tm_mon = idlOS::atoi(tmp) - 1;
                // add length(YYYY:MM, 6)
                idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal+6, 2);
                tmp[2] = 0;
                tval.tm_mday = idlOS::atoi(tmp);
                if ( idlOS::strlen(sHandle->mParser.mTimestampVal) > 8 )
                {
                    // add length(YYYY:MM:DD, 8)
                    idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal+8, 2);
                    tmp[2] = 0;
                    tval.tm_hour = idlOS::atoi(tmp);
                    // add length(YYYY:MM:DD:HH, 10)
                    idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal+10, 2);
                    tmp[2] = 0;
                    tval.tm_min = idlOS::atoi(tmp);
                    // add length(YYYY:MM:DD:HH:MI, 12)
                    idlOS::strncpy(tmp, sHandle->mParser.mTimestampVal+12, 2);
                    tmp[2] = 0;
                    tval.tm_sec = idlOS::atoi(tmp);
                }
                secVal = (UInt)idlOS::mktime(&tval);

                idlOS::sprintf(tmpBuffer,
                        "BYTE\'%08"ID_XINT32_FMT"\',", secVal);
                IDE_TEST(m_pISPApi->appendQueryString( tmpBuffer )
                         == SQL_FALSE);
                
            }
            else if ( sHandle->mParser.mTimestampType == ILO_TIMESTAMP_DAT )
            {
                if( aTableInfo->mSkipFlag[i] != ILO_TRUE )
                {
                    IDE_TEST(m_pISPApi->appendQueryString( (SChar*)"?,") == SQL_FALSE);
                }
            }
            continue;
        }
        if( aTableInfo->mSkipFlag[i] == ILO_TRUE )
        {
            continue;
        }
        if ((index = aTableInfo->seqEqualChk( sHandle, i )) >= 0)
        {
            idlOS::sprintf(tmpBuffer,
                           "%s.%s,",
                           aTableInfo->localSeqArray[index].seqName,
                           aTableInfo->localSeqArray[index].seqVal);
            IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);
            continue;
        }
        else if( aTableInfo->GetAttrType(i) == ISP_ATTR_DATE )
        {
            if( aTableInfo->mAttrDateFormat[i] != NULL )
            {
                idlOS::sprintf(tmpBuffer, "to_date(?,'%s'),",
                               aTableInfo->mAttrDateFormat[i]);
                IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);
            
            }
            else if( idlOS::strlen(sHandle->mParser.mDateForm) >= 1 )
            {
                idlOS::sprintf(tmpBuffer, "to_date(?,'%s'),", sHandle->mParser.mDateForm);
                IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);
            }
            else
            {
                IDE_TEST(m_pISPApi->appendQueryString( (SChar*)"?,") == SQL_FALSE);
            }
        }
        // BUG-24358 iloader geometry type support
        else if ( aTableInfo->GetAttrType(i) == ISP_ATTR_GEOMETRY)
        {
            /* 
             * BUG-48357 WKB compatibility option
             * geomfromewkb function can read WKB, though not written in manual. 
             * So, "-geom WKB" option with in mode is not necessary.
             */
            /* BUG-47821 SRID support: geomfromwkb -> geomfromewkb */
            idlOS::sprintf( tmpBuffer, "geomfromewkb(?)," );
            IDE_TEST(m_pISPApi->appendQueryString(tmpBuffer) == SQL_FALSE);
        }        
        else
        {
            // BUG-26485 iloader �� trim ����� �߰��ؾ� �մϴ�.
            // mAttrDateFormat �� ����ڰ� �Է��� �Լ����� ���ɴϴ�.
            if( aTableInfo->mAttrDateFormat[i] != NULL )
            {
                idlOS::sprintf(tmpBuffer, "%s,", aTableInfo->mAttrDateFormat[i]);
                IDE_TEST(m_pISPApi->appendQueryString( tmpBuffer ) == SQL_FALSE);
            }
            else
            {
                IDE_TEST(m_pISPApi->appendQueryString( (SChar*)"?,") == SQL_FALSE);
            }
        }
    }
    m_pISPApi->replaceTerminalChar( ')');

    // BUG-25010 iloader -mode TRUNCATE ����
    // ����Ǵ� ������ ���� �ְ� �Ѵ�.
    if (m_pProgOption->m_bDisplayQuery == ILO_TRUE)
    {
        idlOS::printf("UpLoad QueryStr[%s]\n", m_pISPApi->getSqlStatement());
    }

    aTableInfo->FreeDateFormat();

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

SInt iloLoad::ExecuteDeleteStmt( ALTIBASE_ILOADER_HANDLE  aHandle,        
                                 iloTableInfo            *aTableInfo )
{
    // BUG-39878 iloader temp buffer size problem  
    SChar sTmpQuery[256];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    idlOS::sprintf(sTmpQuery , "DELETE FROM %s ", aTableInfo->GetTableName());
    m_pISPApi->clearQueryString();
    
    IDE_TEST(m_pISPApi->appendQueryString(sTmpQuery) == SQL_FALSE);

    // BUG-25010 iloader -mode TRUNCATE ����
    // ����Ǵ� ������ ���� �ְ� �Ѵ�.
    if (m_pProgOption->m_bDisplayQuery == ILO_TRUE)
    {
        idlOS::printf("DELETE QueryStr[%s]\n", sTmpQuery);
    }

    IDE_TEST(m_pISPApi->ExecuteDirect() != SQL_TRUE);

    //BUG-26118 iLoader Hang
    IDE_TEST(m_pISPApi->EndTran(ILO_TRUE) != IDE_SUCCESS);

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return SQL_FALSE;
}
// BUG-25010 iloader -mode TRUNCATE ����
SInt iloLoad::ExecuteTruncateStmt( ALTIBASE_ILOADER_HANDLE aHandle,
                                   iloTableInfo           *aTableInfo )
{
    // BUG-39878 iloader temp buffer size problem  
    SChar sTmpQuery[256];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    idlOS::sprintf(sTmpQuery , "TRUNCATE TABLE %s ", aTableInfo->GetTableName());
    m_pISPApi->clearQueryString();
    
    IDE_TEST(m_pISPApi->appendQueryString(sTmpQuery) == SQL_FALSE);

    if (m_pProgOption->m_bDisplayQuery == ILO_TRUE)
    {
        idlOS::printf("TRUNCATE QueryStr[%s]\n", sTmpQuery);
    }

    IDE_TEST(m_pISPApi->ExecuteDirect() != SQL_TRUE);

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return SQL_FALSE;
}

/*
 * PROJ-1760
 * Parallel Direct-Path Insert Query�� �����ϱ� ���� Query ����
 * Upload�� �����ϴ� Session���� �����ؾ߸� �Ѵ�.
 * -Parallel Option�� ����� ��쿡�� ������ ����ŭ Session�� �����ǹǷ�
 * �� Session���� �����ϵ��� �ؾ� ��
 * SQL> Alter Session Set PARALLEL_DML_MODE = 1;
 */
SInt iloLoad::ExecuteParallelStmt( ALTIBASE_ILOADER_HANDLE  aHandle,
                                   iloSQLApi               *aISPApi )
{
    SChar sTmpQuery[128];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    idlOS::sprintf(sTmpQuery, "ALTER SESSION SET PARALLEL_DML_MODE=1");
    aISPApi->clearQueryString();
    
    IDE_TEST(aISPApi->appendQueryString(sTmpQuery) == SQL_FALSE);

    IDE_TEST(aISPApi->ExecuteDirect() != SQL_TRUE);

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return SQL_FALSE;
}

/* PROJ-1760 Parallel Direct-Path Upload
 * Upload�� �����Ϸ��� �ϴ� Table�� Logging Mode�� ���´�.
 * m_TableLogStatus�� ���� �Ʒ��� ����.
 * Logging Mode�� ��쿡�� SQL_TRUE
 * NoLogging Mode�� ��쿡�� SQL_FALSE
 * NoLogging Mode�� ��쿡�� Upload�� ���� �Ŀ� 
 * 'Alter table XXX logging' Query�� ������ �ʿ䰡 ����.
 */
SInt iloLoad::GetLoggingMode(iloTableInfo * /*aTableInfo*/)
{
    //BUG-BUG~
    // ���� Table�� Logging Mode�� ���� �� �ִ� ����� �����Ƿ� �ش� ����� �Ϸ��� �Ŀ�
    // ����� �ؾ� �Ѵ�.
    m_TableLogStatus = SQL_TRUE;
    return SQL_TRUE;
}

/* PROJ-1760
 * Direct-Path Upload�ÿ� No Logging Mode�� ������ ��쿡
 * 'Alter table XXX nologging' Query�� ������ ��� �Ѵ�.
 * aIsLog�� SQL_TRUE�� ��쿡�� nologging Mode�� ����
 * aIsLog�� SQL_FALSE�� ��쿡�� logging Mode�� ����
 */
SInt iloLoad::ExecuteAlterStmt( ALTIBASE_ILOADER_HANDLE  aHandle,
                                iloTableInfo            *aTableInfo, 
                                SInt                     aIsLog )
{
    // BUG-39878 iloader temp buffer size problem  
    SChar sTmpQuery[256];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if( aIsLog == SQL_TRUE )    //No-Logging Mode�� �����ؾ� �� ���
    {
        idlOS::sprintf(sTmpQuery, "ALTER TABLE %s NOLOGGING", aTableInfo->GetTableName());
    }
    else                        //Logging Mode�� �����ؾ��� ���
    {
        idlOS::sprintf(sTmpQuery, "ALTER TABLE %s LOGGING", aTableInfo->GetTableName());
    }
    
    m_pISPApi->clearQueryString();
    
    IDE_TEST(m_pISPApi->appendQueryString(sTmpQuery) == SQL_FALSE);

    IDE_TEST(m_pISPApi->ExecuteDirect() != SQL_TRUE);

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return SQL_FALSE;    
}

SInt iloLoad::BindParameter( ALTIBASE_ILOADER_HANDLE  aHandle,
                             iloSQLApi               *aISPApi,
                             iloTableInfo            *aTableInfo)
{
    SInt         i;
    SInt         j = 0;
    SInt         sBufLen;
    SQLSMALLINT  sInOutType = SQL_PARAM_INPUT;
    SQLSMALLINT  sSQLType;
    SQLSMALLINT  sValType = SQL_C_CHAR;
    SQLSMALLINT  sParamCnt = 0;
    SShort       sDecimalDigits = 0;
    UInt         sColSize = MAX_VARCHAR_SIZE;
    void        *sParamValPtr;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    mArrayCount = m_pProgOption->m_ArrayCount;
    aTableInfo->CopyStruct(sHandle);

    IDE_TEST( aISPApi->NumParams(&sParamCnt)
              != IDE_SUCCESS );

    IDE_TEST(aISPApi->SetColwiseParamBind( sHandle, 
                                           (UInt)mArrayCount,
                                           aTableInfo->mStatusPtr)
                                           != IDE_SUCCESS);

    for (i = 0; i < aTableInfo->GetAttrCount(); i++)
    {
        if (aTableInfo->seqEqualChk( sHandle, i ) >= 0 ||
            aTableInfo->mSkipFlag[i] == ILO_TRUE)
        {
            continue;
        }
        if (aTableInfo->GetAttrType(i) == ISP_ATTR_TIMESTAMP &&
            sHandle->mParser.mAddFlag == ILO_TRUE)
        {
            continue;
        }

        j++;

        switch (aTableInfo->GetAttrType(i))
        {
        case ISP_ATTR_CHAR:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_CHAR;
            /* BUG - 18804 */
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_VARCHAR:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_VARCHAR;
            /* BUG - 18804 */
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        //===========================================================
        // proj1778 nchar
        case ISP_ATTR_NCHAR:
            sInOutType = SQL_PARAM_INPUT;
            if (sHandle->mProgOption->mNCharUTF16 == ILO_TRUE)
            {
                sValType = SQL_C_WCHAR;
            }
            else
            {
                sValType = SQL_C_CHAR;
            }
            sSQLType = SQL_WCHAR;
            /* BUG - 18804 */
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_NVARCHAR:
            sInOutType = SQL_PARAM_INPUT;
            if (sHandle->mProgOption->mNCharUTF16 == ILO_TRUE)
            {
                sValType = SQL_C_WCHAR;
            }
            else
            {
                sValType = SQL_C_CHAR;
            }
            sSQLType = SQL_WVARCHAR;
            /* BUG - 18804 */
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        //===========================================================
        case ISP_ATTR_CLOB:
            sInOutType = SQL_PARAM_OUTPUT;
            sValType = SQL_C_CLOB_LOCATOR;
            sSQLType = SQL_CLOB_LOCATOR;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrVal(i);
            sBufLen = (SInt)aTableInfo->mAttrValEltLen[i];
            break;
        case ISP_ATTR_NIBBLE:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_NIBBLE;
            /* BUG - 18804 */
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_BYTES:
        case ISP_ATTR_VARBYTE:
        case ISP_ATTR_TIMESTAMP:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_BYTE;
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_BLOB:
            sInOutType = SQL_PARAM_OUTPUT;
            sValType = SQL_C_BLOB_LOCATOR;
            sSQLType = SQL_BLOB_LOCATOR;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrVal(i);
            sBufLen = (SInt)aTableInfo->mAttrValEltLen[i];
            break;
        case ISP_ATTR_SMALLINT:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_SMALLINT;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_INTEGER:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_INTEGER;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_BIGINT:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_BIGINT;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_REAL:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_REAL;
            sColSize = 0;
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_FLOAT:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_FLOAT;
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_DOUBLE:
            /* BUG-46485 SQL_C_DOUBLE -> SQL_C_CHAR */
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_DOUBLE;
            sColSize = 0; /* Ignored */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_NUMERIC_LONG:
        case ISP_ATTR_NUMERIC_DOUBLE:
        case ISP_ATTR_DECIMAL:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_NUMERIC;
            /* BUG - 18804 */
            sColSize = MAX_NUMBER_SIZE;
            sDecimalDigits = (SShort)aTableInfo->mScale[i];
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        case ISP_ATTR_BIT:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_BINARY;
            sSQLType = SQL_BIT;
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrVal(i);
            sBufLen = (SInt)aTableInfo->mAttrValEltLen[i];
            break;
        case ISP_ATTR_VARBIT:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_BINARY;
            sSQLType = SQL_VARBIT;
            sColSize = (UInt)aTableInfo->mPrecision[i];
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrVal(i);
            sBufLen = (SInt)aTableInfo->mAttrValEltLen[i];
            break;
        case ISP_ATTR_DATE:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_CHAR;
            /* BUG - 18804 */
            sColSize = MAX_NUMBER_SIZE;
            sDecimalDigits = 0;
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
            // BUG-24358 iloader geometry type support
        case ISP_ATTR_GEOMETRY:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_BINARY;
            sSQLType = SQL_BINARY;
            sColSize = (UInt)aTableInfo->mAttrCValEltLen[i];/* BUG-31404 */
            sDecimalDigits = 0; /* Ignored */
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
            break;
        default:
            sInOutType = SQL_PARAM_INPUT;
            sValType = SQL_C_CHAR;
            sSQLType = SQL_CHAR;
            sColSize = MAX_VARCHAR_SIZE;
            sDecimalDigits = 0;
            sParamValPtr = aTableInfo->GetAttrCVal(i);
            sBufLen = (SInt)aTableInfo->mAttrCValEltLen[i];
        }

        IDE_TEST(aISPApi->BindParameter( (UShort)j,
                                         sInOutType,
                                         sValType,
                                         sSQLType,
                                         sColSize,
                                         sDecimalDigits,
                                         sParamValPtr,
                                         sBufLen,
                                         &aTableInfo->mAttrInd[i][0])
                                         != IDE_SUCCESS);
    }

    /* Added when fixing BUG-40363, but this is for BUG-34432 */
    IDE_TEST_RAISE( j != sParamCnt, param_mismatch );

    return SQL_TRUE;

    IDE_EXCEPTION( param_mismatch )
    {
        uteSetErrorCode( sHandle->mErrorMgr,
                         utERR_ABORT_Param_Count_Mismatch,
                         j,
                         m_pISPApi->getSqlStatement() );
    }
    IDE_EXCEPTION_END;

    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return SQL_FALSE;
}

// BUG-24358 iloader geometry type support
// get maximum size of geometry column, resize data token length.
IDE_RC iloLoad::reallocFileToken( ALTIBASE_ILOADER_HANDLE  aHandle,
                                  iloTableInfo            *aTableInfo )
{
    SInt sI;
    SInt sMaxGeoColSize = MAX_TOKEN_VALUE_LEN;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    for (sI = aTableInfo->GetAttrCount() - 1; sI >= 0; sI--)
    {
        if (aTableInfo->seqEqualChk( sHandle, sI ) >= 0 ||
            aTableInfo->mSkipFlag[sI] == ILO_TRUE)
        {
            continue;
        }
        if( aTableInfo->GetAttrType(sI) == ISP_ATTR_GEOMETRY)
        {
            if ((aTableInfo->mPrecision[sI] + MAX_SEPERATOR_LEN) > sMaxGeoColSize)
            {
                sMaxGeoColSize = aTableInfo->mPrecision[sI];
            }
        }
    }
    return m_DataFile.ResizeTokenLen( sHandle, sMaxGeoColSize );
}

/**
 * IsLOBColExist.
 *
 * �ε�� �÷� �� LOB �÷��� �����ϴ��� �˻��Ѵ�.
 */
iloBool iloLoad::IsLOBColExist( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                iloTableInfo            *aTableInfo)
{
    EispAttrType sDataType;
    SInt         sI;
    iloBool       sLOBColExist = ILO_FALSE;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    //BUG-24583
    aTableInfo->mLOBColumnCount = 0;    //Initialize

    for (sI = aTableInfo->GetAttrCount() - 1; sI >= 0; sI--)
    {
        if (aTableInfo->seqEqualChk( sHandle, sI ) >= 0 ||
            aTableInfo->mSkipFlag[sI] == ILO_TRUE)
        {
            continue;
        }
        sDataType = aTableInfo->GetAttrType(sI);
        if (sDataType == ISP_ATTR_BLOB || sDataType == ISP_ATTR_CLOB)
        {
            sLOBColExist = ILO_TRUE;
            aTableInfo->mLOBColumnCount++;      //BUG-24583       
        }
    }

    return sLOBColExist;
}


/* PROJ-1714
 * ReadFileToBuff()�� Thread�� ����ϱ� ���ؼ� ȣ��Ǵ� �Լ�
 */

void* iloLoad::ReadFileToBuff_ThreadRun(void *arg)
{
    iloaderHandle *sHandle = (iloaderHandle *) arg;
    
    sHandle->mLoad->ReadFileToBuff(sHandle);
    return 0;
}


/* PROJ-1714
 * InsertFromBuff()�� Thread�� ����ϱ� ���ؼ� ȣ��Ǵ� �Լ�
 */

void* iloLoad::InsertFromBuff_ThreadRun(void *arg)
{
    iloaderHandle *sHandle = (iloaderHandle *) arg;
    
    sHandle->mLoad->InsertFromBuff(sHandle);
    return 0;
}


/* PROJ-1714
 * File���� ���� ������ ���� ���ۿ� �Է��ϵ��� �Ѵ�
 */

void iloLoad::ReadFileToBuff( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SInt    sReadResult = 0;
    SChar  *sReadTmp    = NULL;
    iloBool  sLOBColExist;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    // BUG-18803 readsize �ɼ� �߰�
    sReadTmp = (SChar*)idlOS::malloc((m_pProgOption->mReadSzie) * 2 );
    IDE_TEST_RAISE((sReadTmp == NULL), NO_MEMORY);
    sLOBColExist = IsLOBColExist( sHandle, &m_pTableInfo[0] );

    while(1)
    {
        /* PROJ-1714
         * LOB Column�� ������ ���, LOB Data�� Load�ϴ� �߿� Datafile�� ������ �ȵ�..
         */
        if( sLOBColExist == ILO_TRUE )
        {    
            iloMutexLock( sHandle, &(sHandle->mParallel.mLoadLOBMutex) );
        }
        // BUG-18803 readsize �ɼ� �߰�
        sReadResult = m_DataFile.ReadFromFile((m_pProgOption->mReadSzie), sReadTmp);
        
        if( sLOBColExist == ILO_TRUE )
        {    
             iloMutexUnLock( sHandle, &(sHandle->mParallel.mLoadLOBMutex) );
        }   
             
        if( sReadResult == 0 ) //EOF �� ���...
        {
            m_DataFile.SetEOF( sHandle, ILO_TRUE);
            break;
        }        
        
        (void)m_DataFile.WriteDataToCBuff( sHandle, sReadTmp, sReadResult);
        
        /* BUG-24211
         * -L �ɼ����� ������ �Է��� ��������� ������ ���, FileRead Thread�� ��Ӵ�� ���� �ʵ��� 
         * Load�ϴ� Thread�� ������ 0�� �� ��� �����ϵ��� �Ѵ�.
         */
        if( sHandle->mParallel.mLoadThrNum == 0)
        {
            // BUG-24816 iloader Hang
            // �����ϰ� EOF �� ������ ������.
            m_DataFile.SetEOF( sHandle, ILO_TRUE );
            break;
        }
    }
    // BUG-18803 readsize �ɼ� �߰�
    idlOS::free(sReadTmp);
    sReadTmp = NULL;
    return;

    IDE_EXCEPTION(NO_MEMORY); 
    {
        // insert �����尡 �����Ҽ� �ְ� EOF �� �����Ѵ�.
        m_DataFile.SetEOF( sHandle, ILO_TRUE );
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
        PRINT_ERROR_CODE(sHandle->mErrorMgr);
    }
    IDE_EXCEPTION_END;

    return;
}

/* BUG-46486 Improve lock wait
 * ���ڵ� ���� ��ŷ���� array ���� ��ŷ���� ����
 */
/* PROJ-1714
 * ���� ���۸� �а�, Parsing�Ŀ� Table�� INSERT �Ѵ�.
 */
void iloLoad::InsertFromBuff( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SInt    sRet = READ_SUCCESS;
    SInt    sExecFlag;
    SLong   sExecCount = 0;
    iloBool sDryrun;
        
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    iloLoadInsertContext  sData;

    idlOS::memset(&sData, 0, ID_SIZEOF(iloLoadInsertContext));
    sData.mHandle = sHandle;

    sDryrun = sHandle->mProgOption->mDryrun;
    
    /* BUG-48016 Fix skipped commit */
    sData.mUncommitInsertCount = 0;

    IDE_TEST(InitContextData(&sData) != IDE_SUCCESS);

    while(sRet != END_OF_FILE)
    {
        if ( sHandle->mProgOption->mGetTotalRowCount != ILO_TRUE )
        {
            // BUG-24879 errors �ɼ� ����
            // 1���� �����尡 ���ǿ� �ɷ����� ������ �����嵵 �����Ų��.
            if(m_pProgOption->m_ErrorCount > 0)
            {
                // not use (m_pProgOption->m_bExist_errors == SQL_TRUE) here.
                // even if -errors not used, the default value will be 50.
                // if -errors 0 then it will ignore this function
                if((m_pProgOption->m_ErrorCount <= sData.mErrorCount) ||
                   (m_pProgOption->m_ErrorCount <= mErrorCount))
                {
                    m_LogFile.PrintLogMsg("Error Count Over. Stop Load");
                    break;
                }
            }
        }
        
        sRet = ReadRecord( &sData ); 

        IDE_TEST_CONT( sHandle->mProgOption->mGetTotalRowCount == ILO_TRUE,
                       TOTAL_COUNT_CODE );

        BREAK_OR_CONTINUE;

        if (sRet != END_OF_FILE)
        {
            // bug-18707     
            Sleep(&sData);
        }

        sExecFlag = (sData.mRealCount == sData.mArrayCount) ||
                    (sRet == END_OF_FILE)                   ||
                    (m_pProgOption->m_bExist_L              &&
                     (sData.mRecordNumber[sData.mRealCount - 1] == m_pProgOption->m_LastRow));

        IDE_TEST_CONT(!sExecFlag, COMMIT_CODE);

        /* BUG-43388 in -dry-run */
        if ( IDL_LIKELY_TRUE( sDryrun == ILO_FALSE ) )
        {
            IDE_TEST_CONT(sData.mRealCount == sData.mArrayCount, EXECUTE_CODE);

            (void)SetParamInfo(&sData);

            BREAK_OR_CONTINUE;

            IDE_EXCEPTION_CONT(EXECUTE_CODE);

            sExecCount++;

            //INSERT SQL EXECUTE
            (void)ExecuteInsertSQL(&sData);

            BREAK_OR_CONTINUE;

            /* Execute()�� ����Ǹ�,
             * Lob �÷��� outbind�� ���� lOB locator�� ����� �����̰�,
             * ���� �����Ͱ� �ε�� ���´� �ƴϴ�. */
            (void)ExecuteInsertLOB(&sData);

            BREAK_OR_CONTINUE;
        }
        else /* if -dry-run */
        {
            // do nothing...
        }

        if (sRet == END_OF_FILE)
        {
            continue;
        }

        IDE_EXCEPTION_CONT(COMMIT_CODE);

        //Commit ó��
        /* BUG-48016 Fix skipped commit */
        if ((m_pProgOption->m_CommitUnit >= 1) &&
            (sExecCount >= m_pProgOption->m_CommitUnit))
        {
            /* BUG-43388 in -dry-run */
            if ( IDL_LIKELY_TRUE( sDryrun == ILO_FALSE ) )
            {
                (void)Commit(&sData);
            }
            else /* if -dry-run */
            {
                (void)Commit4Dryrun(&sData);
            }
            sExecCount = 0;
            
            if (sHandle->mProgOption->m_bExist_ShowCommit == ID_TRUE)
            {
                idlOS::fprintf( stderr, "%s%s\n",
                        DEV_SHOW_COMMIT_PREFIX,
                        "Commit executed");
            }
        }

        IDE_EXCEPTION_CONT(TOTAL_COUNT_CODE);

    }//End of while

    /* BUG-43388 in -dry-run */
    if ( IDL_LIKELY_TRUE( sDryrun == ILO_FALSE ) )
    {
        // BUG-29024
        (void)Commit(&sData);
    }
    else /* if -dry-run */
    {
        (void)Commit4Dryrun(&sData);
    }
    
    /* BUG-48016 Fix skipped commit */
    if (sHandle->mProgOption->m_bExist_ShowCommit == ID_TRUE)
    {
        idlOS::fprintf( stderr, "%s%s\n", 
                DEV_SHOW_COMMIT_PREFIX,
                "Last commit executed");
    }

    //Thread ������ ó��... 
    (void)FiniContextData(&sData);

    return;

    IDE_EXCEPTION_END;

    // BUG-29024
    if (m_pISPApiUp[sData.mConnIndex].EndTran(ILO_TRUE) != IDE_SUCCESS)
    {
        iloMutexLock( sHandle, &(sHandle->mParallel.mLoadFIOMutex) );
        m_LogFile.PrintLogErr(sHandle->mErrorMgr);
        (void)m_pISPApiUp[sData.mConnIndex].EndTran(ILO_FALSE);
        iloMutexUnLock( sHandle,&(sHandle->mParallel.mLoadFIOMutex) );
    }

    (void)FiniContextData(&sData);

    return;
}

IDE_RC  iloLoad::ConnectDBForUpload( ALTIBASE_ILOADER_HANDLE aHandle,
                                     iloSQLApi * aISPApi,
                                     SChar     * aHost,
                                     SChar     * aDB,
                                     SChar     * aUser,
                                     SChar     * aPasswd,
                                     SChar     * aNLS,
                                     SInt        aPortNo,
                                     SInt        aConnType,
                                     iloBool     aPreferIPv6, /* BUG-29915 */
                                     SChar     * aSslCa, /* BUG-41406 SSL */
                                     SChar     * aSslCapath,
                                     SChar     * aSslCert,
                                     SChar     * aSslKey,
                                     SChar     * aSslVerify,
                                     SChar     * aSslCipher)
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    IDE_TEST(aISPApi->Open(aHost, aDB, aUser, aPasswd, aNLS, aPortNo,
                           aConnType, aPreferIPv6, /* BUG-29915 */
                           aSslCa,
                           aSslCapath,
                           aSslCert,
                           aSslKey,
                           aSslVerify,
                           aSslCipher)
             != IDE_SUCCESS);

    /* BUG-47891 */
    IDE_TEST( aISPApi->GetShardEnable() != IDE_SUCCESS );

    // BUG-34082 The replication option of the iloader doesn't work properly.
    IDE_TEST( aISPApi->alterReplication( sHandle->mProgOption->mReplication )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    PRINT_ERROR_CODE(sHandle->mErrorMgr);

    return IDE_FAILURE;  
}


IDE_RC iloLoad::DisconnectDBForUpload(iloSQLApi *aISPApi)
{
    aISPApi->Close();

    return IDE_SUCCESS;
}

