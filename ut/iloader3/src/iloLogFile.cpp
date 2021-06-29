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
 * $Id: iloLogFile.cpp 88494 2020-09-04 04:29:31Z chkim $
 **********************************************************************/

#include <ilo.h>

iloLogFile::iloLogFile()
{
    m_LogFp = NULL;
    m_UseLogFile = SQL_FALSE;
    mIsCSV  = ID_FALSE;
    idlOS::strcpy(m_FieldTerm, "^");
    idlOS::strcpy(m_RowTerm, "\n");
}

void iloLogFile::SetTerminator(SChar *szFiledTerm, SChar *szRowTerm)
{
    idlOS::strcpy(m_FieldTerm, szFiledTerm);
    idlOS::strcpy(m_RowTerm, szRowTerm);
}

SInt iloLogFile::OpenFile( SChar *szFileName, idBool aIsExistFilePerm )
{
    // bug-20391
    if( idlOS::strncmp(szFileName, "stderr", 6) == 0)
    {
        m_LogFp = stderr;
    }
    else if(idlOS::strncmp(szFileName, "stdout", 6) == 0)
    {
        m_LogFp = stdout;
    }
    else
    {
        //BUG-24511 ��� Fopen�� binary type���� �����ؾ���
        /* BUG-47652 Set file permission */
        m_LogFp = ilo_fopen( szFileName, "wb", aIsExistFilePerm );
    }
    IDE_TEST( m_LogFp == NULL );

    m_UseLogFile = SQL_TRUE;
    return SQL_TRUE;

    IDE_EXCEPTION_END;

    idlOS::printf("Log File [%s] open fail\n", szFileName);

    m_UseLogFile = SQL_FALSE;
    return SQL_FALSE;
}

SInt iloLogFile::CloseFile()
{
    IDE_TEST_CONT( m_UseLogFile == SQL_FALSE, err_ignore );

    // BUG-20525
    if((m_LogFp != stderr) && (m_LogFp != stdout))
    {
        IDE_TEST( idlOS::fclose(m_LogFp) != 0 );
    }

    m_UseLogFile = SQL_FALSE;

    IDE_EXCEPTION_CONT( err_ignore );

    return SQL_TRUE;

    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

void iloLogFile::PrintLogMsg(const SChar *szMsg)
{
    UInt sMsgLen;

    IDE_TEST(m_UseLogFile == SQL_FALSE);

    idlOS::fprintf(m_LogFp, "%s", szMsg);
    sMsgLen = idlOS::strlen(szMsg);
    if (sMsgLen > 0 && szMsg[sMsgLen - 1] != '\n')
    {
        idlOS::fprintf(m_LogFp, "\n");
    }

    IDE_EXCEPTION_END;

    return;
}

// BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
// ���� �����޽����� ������ �������� ����ϴ� �Լ��߰�
void iloLogFile::PrintLogErr(uteErrorMgr *aMgr)
{
    IDE_TEST(m_UseLogFile == SQL_FALSE);

    utePrintfErrorCode(m_LogFp, aMgr);

    IDE_EXCEPTION_END;

    return;
}

void iloLogFile::PrintTime(const SChar *szPrnStr)
{
    time_t lTime;
    SChar  *szTime;

    IDE_TEST(m_UseLogFile == SQL_FALSE);

    time(&lTime);
    szTime = ctime(&lTime);

    idlOS::fprintf(m_LogFp, "%s : %s", szPrnStr, szTime);

    IDE_EXCEPTION_END;

    return;
}
/* TASK-2657 */
void iloLogFile::setIsCSV ( SInt aIsCSV )
{
    mIsCSV = aIsCSV;
}

// BUG-21640 iloader���� �����޽����� �˾ƺ��� ���ϰ� ����ϱ�
SInt iloLogFile::PrintOneRecord( ALTIBASE_ILOADER_HANDLE  aHandle,
                                 iloTableInfo            *pTableInfo,
                                 SInt                     aReadRecCount,
                                 SInt                     aArrayCount)
{
    SInt    i;
    SChar*  sDataPtr  = NULL;
    SInt    sDataLen  = 0;
    SInt    sColCount = pTableInfo->GetReadCount(aArrayCount);
    static SChar*  sBlob     = (SChar *)"[BLOB]"; // BUG-31130
    static SChar*  sClob     = (SChar *)"[CLOB]";
    static SChar*  sGeo      = (SChar *)"[GEOMETRY]";
    static SChar*  sUnknown  = (SChar *)"unknown type";
 

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if(( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))
    {
        
        if (sHandle->mDataBufMaxColCount < sColCount)
        {
            if (sHandle->mDataBuf != NULL)
            {
                idlOS::free(sHandle->mDataBuf);
            }
            sHandle->mDataBuf = (SChar**)idlOS::calloc(sColCount,
                                                       ID_SIZEOF(SChar*));
            IDE_TEST_RAISE( sHandle->mDataBuf == NULL, mallocError );

            sHandle->mDataBufMaxColCount = sColCount;
        }

        sHandle->mLog.recordData = sHandle->mDataBuf;
        sHandle->mLog.recordColCount =  sColCount;

        sHandle->mLog.record = aReadRecCount;
    }
    
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        IDE_TEST_CONT(m_UseLogFile == SQL_FALSE, err_ignore);
    }
    else
    {
        IDE_TEST_CONT( ((m_UseLogFile == SQL_FALSE) && (sHandle->mLogCallback == NULL)), err_ignore);
    }

    if ( m_UseLogFile == SQL_TRUE )
    {
        // log ���Ͽ� rowcount �� ����Ѵ�.
        idlOS::fprintf(m_LogFp, "Record %d : ", aReadRecCount);
    }

    for (i=0; i < pTableInfo->GetReadCount(aArrayCount); i++)
    {
        if ( pTableInfo->GetAttrType(i) == ISP_ATTR_TIMESTAMP &&
             sHandle->mParser.mAddFlag == ILO_TRUE )
        {
            i++;
        }
        switch (pTableInfo->GetAttrType(i))
        {
        case ISP_ATTR_CHAR :
        case ISP_ATTR_VARCHAR :
        case ISP_ATTR_NCHAR :
        case ISP_ATTR_NVARCHAR :

            // TASK-2657
            //=======================================================
            // proj1778 nchar
            // �������� csv�̸� fwrite�� ����ϰ� �ƴϸ� fprintf(%s)�� ����ߴµ� utf16
            // nchar�� �߰��Ǹ鼭 null�����ڰ� ���ھȿ� ������ �����Ƿ� fwrite�� ���Ͻ�Ų��
            // mAttrFailLen[i] �� 0�� �ƴ϶�� read file���� failed column�� ����ִٴ� ���
            if (pTableInfo->mAttrFailLen[i] != 0)
            {
                sDataPtr = pTableInfo->GetAttrFail( i );
                sDataLen = (SInt)(pTableInfo->mAttrFailLen[i]);
            }
            else
            {
                sDataPtr = pTableInfo->GetAttrCVal(i, aArrayCount);
                sDataLen = (SInt)(pTableInfo->mAttrInd[i][aArrayCount]);
            }
    
            /* data file�� ������ Į���� �ִ� ��� */ 
            if (( sHandle->mUseApi == SQL_TRUE ) &&
                    ( sHandle->mLogCallback != NULL ))      
            {
                // BUG-40202: insure++ warning READ_OVERFLOW
                // if ( idlOS::strcmp(sDataPtr, "") == 0 )
                /* BUG-43351 Null Pointer Dereference */
                if ( sDataPtr != NULL && sDataPtr[0] == '\0' )
                {
                    sHandle->mDataBuf[i] = sDataPtr;
                }
            }
           
            if (sDataLen > 0)
            {
                if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))      
                {
                    if (sDataPtr != NULL)
                    {
                        sDataPtr[sDataLen]='\0';
                    }
        
                    sHandle->mDataBuf[i] = sDataPtr;
                }

                if ( m_UseLogFile == SQL_TRUE )
                {
                    // BUG-28069: log, bad���� csv �������� ���
                    if ((mIsCSV == ID_TRUE) && (pTableInfo->mAttrFailLen[i] == 0))
                    {
                        IDE_TEST_RAISE( iloDataFile::csvWrite( sHandle, sDataPtr, sDataLen, m_LogFp )
                                != SQL_TRUE, WriteError);
                    }
                    else
                    {
                        IDE_TEST_RAISE( idlOS::fwrite(sDataPtr, (size_t)sDataLen, 1, m_LogFp)
                                != 1, WriteError);
                    }
                }
            }
            break;
        case ISP_ATTR_INTEGER :
        case ISP_ATTR_DOUBLE :
        case ISP_ATTR_SMALLINT:
        case ISP_ATTR_BIGINT:
        case ISP_ATTR_DECIMAL:
        case ISP_ATTR_FLOAT:
        case ISP_ATTR_REAL:
        case ISP_ATTR_NIBBLE :
        case ISP_ATTR_BYTES :
        case ISP_ATTR_VARBYTE :
        case ISP_ATTR_NUMERIC_LONG :
        case ISP_ATTR_NUMERIC_DOUBLE :
        case ISP_ATTR_DATE :
        case ISP_ATTR_TIMESTAMP :
        case ISP_ATTR_BIT:
        case ISP_ATTR_VARBIT:

            // BUG - 18804
            // mAttrFail�� fail�� �ʵ尡 �ִ��� Ȯ���� �� ������ mAttrCVal���� ������.
            if (pTableInfo->mAttrFailLen[i] != 0)
            {
                sDataPtr = pTableInfo->GetAttrFail( i );
            }
            else
            {
                sDataPtr = pTableInfo->GetAttrCVal(i, aArrayCount);
            }
            
            if (( sHandle->mUseApi == SQL_TRUE ) && ( sHandle->mLogCallback != NULL ))      
            {
                sHandle->mDataBuf[i] = sDataPtr;
            }

            if ( m_UseLogFile == SQL_TRUE )
            {
                IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s", sDataPtr )
                        < 0, WriteError);
            }
            break;
        case ISP_ATTR_BLOB :
            if ( m_UseLogFile == SQL_TRUE )
            {
                IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s",sBlob )
                        < 0, WriteError);
            }
            else
            {
                 sHandle->mDataBuf[i] = sBlob;
            }
            break;
        case ISP_ATTR_CLOB :
            if ( m_UseLogFile == SQL_TRUE )
            {
                IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s",sClob )
                        < 0, WriteError);
            }
            else
            {
                 sHandle->mDataBuf[i] = sClob;
            }
            break;
        // BUG-24358 iloader geometry type support            
        case ISP_ATTR_GEOMETRY :
            if ( m_UseLogFile == SQL_TRUE )
            {
                IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s",sGeo )
                        < 0, WriteError);
            }
            else
            {
                 sHandle->mDataBuf[i] = sGeo;
            }
            break;
        default :
            if ( m_UseLogFile == SQL_TRUE )
            {
                IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s",sUnknown )
                        < 0, WriteError);
            }
            else
            {
                 sHandle->mDataBuf[i] = sUnknown;
            }
            break;
        }

        if ( m_UseLogFile == SQL_TRUE )
        {
            /* TASK-2657 */
            // BUG-24898 iloader �Ľ̿��� ��ȭ
            // log ���Ͽ� ��½� ����Ÿ ����� �ݵ�� ���Ͱ� ���� �ֵ����Ѵ�.
            // rowterm �� ����� �ʿ���� ����Ÿ�� ����� �������� ���͸� �ִ´�.
            if( mIsCSV == ID_TRUE )
            {
                if ( i != pTableInfo->GetAttrCount()-1 )
                {
                    IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%c", ',' )
                            < 0, WriteError);
                }
            }
            else
            {
                if ( i != pTableInfo->GetAttrCount()-1 )
                {
                    IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%s", m_FieldTerm )
                            < 0, WriteError);
                }
            }
        }
    }

    if ( m_UseLogFile == SQL_TRUE )
    {
        IDE_TEST_RAISE( idlOS::fprintf(m_LogFp, "%c", '\n' )
                < 0, WriteError);
    }

    IDE_EXCEPTION_CONT(err_ignore);

    return SQL_TRUE;

    IDE_EXCEPTION(mallocError)
    {   
        if (sHandle->mDataBuf != NULL)
        {
            idlOS::free(sHandle->mDataBuf);
        }
    }  
    IDE_EXCEPTION(WriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Log_File_IO_Error);
        if ( sHandle->mUseApi != SQL_TRUE)
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("Log file write fail\n");
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

