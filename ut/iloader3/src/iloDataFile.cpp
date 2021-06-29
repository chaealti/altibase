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
 * $Id: iloDataFile.cpp 88494 2020-09-04 04:29:31Z chkim $
 **********************************************************************/

#include <ilo.h>

#if (defined(VC_WIN32) || defined(VC_WIN64)) && _MSC_VER < 1400
extern "C" int _fseeki64(FILE *, __int64, int);
#endif

// BUG-24902: FilePath�� ���� ������� Ȯ��
#if defined(VC_WIN32)
#define IS_ABSOLUTE_PATH(P) ((P[0] == IDL_FILE_SEPARATOR) || ((strlen(P) > 3) && (P[1] == ':') && (P[2] == IDL_FILE_SEPARATOR)))
#else
#define IS_ABSOLUTE_PATH(P)  (P[0] == IDL_FILE_SEPARATOR)
#endif

/* BUG-46442
   * ������ �߻��� ���� �÷� ��ȣ ���� */
#define SET_READ_ERROR          \
    sRC = READ_ERROR;           \
    if (sReadErrCol == -1)      \
    {                           \
        sReadErrCol = i;        \
    }

iloLOB::iloLOB()
{
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
    mLOBLoc = 0;
# else
    mLOBLoc.hiword = mLOBLoc.loword = 0;
#endif
    *(SShort *)&mLOBLocCType = 0;
    *(SShort *)&mValCType = 0;
    mLOBLen = 0;
    mPos = 0;
    mIsFetchError = ILO_FALSE;
    mLOBAccessMode = LOBAccessMode_RDONLY;
    /* BUG-21064 : CLOB type CSV up/download error */
    mIsBeginCLOBAppend  = ILO_FALSE;
    mSaveBeginCLOBEnc   = ILO_FALSE;
    mIsCSVCLOBAppend    = ILO_FALSE;
}

iloLOB::~iloLOB()
{

}

/**
 * Open.
 *
 * ������ LOB�� ���� ������ �����Ѵ�.
 * LOB �б��� ��� �����κ��� LOB ���̸� ��� �ϵ� �����Ѵ�.
 *
 * @param[in] aLOBLocCType
 *  LOB locator�� C ��������. ���� �� �� �ϳ��̴�.
 *  SQL_C_CLOB_LOCATOR, SQL_C_BLOB_LOCATOR.
 * @param[in] aLOBLoc
 *  LOB locator
 * @param[in] aValCType
 *  ���� C ��������. ���� �� �� �ϳ��̴�.
 *  SQL_C_CHAR, SQL_C_BINARY.
 *  ��, SQL_C_BINARY�� aLOBLocCType��
 *  SQL_C_BLOB_LOCATOR�� ��츸 �����ϴ�.
 * @param[in] aLOBAccessMode
 *  LOB �б� �Ǵ� ���⸦ �����Ѵ�. ���� �� �� �ϳ��̴�.
 *  iloLOB::LOBAccessMode_RDONLY,
 *  iloLOB::LOBAccessMode_WRONLY.
 */
IDE_RC iloLOB::OpenForUp( ALTIBASE_ILOADER_HANDLE  /* aHandle */,
                          SQLSMALLINT              aLOBLocCType,
                          SQLUBIGINT               aLOBLoc,
                          SQLSMALLINT              aValCType,
                          LOBAccessMode            aLOBAccessMode,
                          iloSQLApi               *aISPApi )
{
    UInt sLOBLen;

    /* 1.Check validity of arguments. */
    IDE_TEST(aLOBLocCType != SQL_C_BLOB_LOCATOR &&
             aLOBLocCType != SQL_C_CLOB_LOCATOR);
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
    IDE_TEST((ULong)aLOBLoc == ID_ULONG(0));
# else
    IDE_TEST(aLOBLoc.hiword == 0 && aLOBLoc.loword == 0);
#endif
    if (aLOBLocCType == SQL_C_BLOB_LOCATOR)
    {
        IDE_TEST((aValCType != SQL_C_BINARY) && (aValCType != SQL_C_CHAR));
    }
    else /* (aLOBLocCType == SQL_C_CLOB_LOCATOR) */
    {
        IDE_TEST(aValCType != SQL_C_CHAR);
    }

    /* 2.Get LOB length. */
    if (aLOBAccessMode == LOBAccessMode_RDONLY)
    {
        IDE_TEST_RAISE(aISPApi->GetLOBLength(aLOBLoc, aLOBLocCType, &sLOBLen)
                       != IDE_SUCCESS, GetLOBLengthError);
            mLOBLen = sLOBLen;
    }
    else
    {
        mLOBLen = 0;
    }

    /* 3.Initialize class member variables. */
    mLOBLocCType = aLOBLocCType;
    mLOBLoc = aLOBLoc;
    mValCType = aValCType;
    mLOBAccessMode = aLOBAccessMode;
    mPos = 0;
    mIsFetchError = ILO_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(GetLOBLengthError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC iloLOB::OpenForDown( ALTIBASE_ILOADER_HANDLE aHandle,
                            SQLSMALLINT             aLOBLocCType, 
                            SQLUBIGINT              aLOBLoc,
                            SQLSMALLINT             aValCType, 
                            LOBAccessMode           aLOBAccessMode)
{
    UInt sLOBLen;
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
        
    /* 1.Check validity of arguments. */
    IDE_TEST(aLOBLocCType != SQL_C_BLOB_LOCATOR &&
             aLOBLocCType != SQL_C_CLOB_LOCATOR);
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
    IDE_TEST((ULong)aLOBLoc == ID_ULONG(0));
# else
    IDE_TEST(aLOBLoc.hiword == 0 && aLOBLoc.loword == 0);
#endif
    if (aLOBLocCType == SQL_C_BLOB_LOCATOR)
    {
        IDE_TEST((aValCType != SQL_C_BINARY) && (aValCType != SQL_C_CHAR));
    }
    else /* (aLOBLocCType == SQL_C_CLOB_LOCATOR) */
    {
        IDE_TEST(aValCType != SQL_C_CHAR);
    }

    /* 2.Get LOB length. */
    if (aLOBAccessMode == LOBAccessMode_RDONLY)
    {
        IDE_TEST_RAISE(sHandle->mSQLApi->GetLOBLength(aLOBLoc, aLOBLocCType, &sLOBLen)
                       != IDE_SUCCESS, GetLOBLengthError);
            mLOBLen = sLOBLen;
    }
    else
    {
        mLOBLen = 0;
    }

    /* 3.Initialize class member variables. */
    mLOBLocCType = aLOBLocCType;
    mLOBLoc = aLOBLoc;
    mValCType = aValCType;
    mLOBAccessMode = aLOBAccessMode;
    mPos = 0;
    mIsFetchError = ILO_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(GetLOBLengthError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/**
 * Close.
 *
 * �����ϴ� LOB ���� �ڿ��� �����Ѵ�.
 */
IDE_RC iloLOB::CloseForUp( ALTIBASE_ILOADER_HANDLE /* aHandle */, iloSQLApi *aISPApi )
{
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
    if ((ULong)mLOBLoc != ID_ULONG(0))
#else
    if ((mLOBLoc.hiword != 0 || mLOBLoc.loword != 0))
#endif
    {
        /* 1.Free resources (for example, locks) related to LOB locator. */
        IDE_TEST_RAISE(aISPApi->FreeLOB(mLOBLoc) != IDE_SUCCESS,
                       FreeLOBError);

        /* 2.Reset LOB locator member variable. */
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
        *(ULong *)&mLOBLoc = ID_ULONG(0);
# else
        mLOBLoc.hiword = mLOBLoc.loword = 0;
#endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(FreeLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC iloLOB::CloseForDown( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
    if ((ULong)mLOBLoc != ID_ULONG(0))
#else
    if ((mLOBLoc.hiword != 0 || mLOBLoc.loword != 0))
#endif
    {
        /* 1.Free resources (for example, locks) related to LOB locator. */
        IDE_TEST_RAISE( sHandle->mSQLApi->FreeLOB(mLOBLoc) != IDE_SUCCESS,
                       FreeLOBError);

        /* 2.Reset LOB locator member variable. */
#if (SIZEOF_LONG == 8) || defined(HAVE_LONG_LONG) || defined(VC_WIN32)
        *(ULong *)&mLOBLoc = ID_ULONG(0);
# else
        mLOBLoc.hiword = mLOBLoc.loword = 0;
#endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(FreeLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Fetch.
 *
 * LOB �б� �� ȣ��Ǹ�,
 * �����κ��� ���� ������ LOB �����͸� �о�� �����Ѵ�.
 * ���������� ������� ���� ��ġ�� ����ϰ� �ִٰ�
 * ���� Fetch() ȣ�� �� �̾����� �κ��� �о �����Ѵ�.
 *
 * @param[out] aVal
 *  �����κ��� �о�� LOB �����Ͱ� ����Ǿ��ִ� ����.
 *  �����ʹ� NULL�� ������� �ʴ´�.
 * @param[out] aStrLen
 *  ���ۿ� ����Ǿ��ִ� LOB �������� ����.
 */
IDE_RC iloLOB::Fetch( ALTIBASE_ILOADER_HANDLE   aHandle,
                      void                    **aVal,
                      UInt                     *aStrLen)
{
    UInt         sForLength;
    UInt         sValueLength;
    SQLSMALLINT  sTargetCType;
    void        *sValue;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    /* 1.Check whether we have already fetched all. */
    IDE_TEST_RAISE(mLOBLen <= mPos, AllFetched);

    /* 2.Call SQLGetLob(). */
    // BUG-21837 LOB ���� ����� �����Ѵ�.
    if ((mLOBLen - mPos) > ILO_LOB_PIECE_SIZE)
    {
        sForLength = ILO_LOB_PIECE_SIZE;
    }
    else
    {
        sForLength = mLOBLen - mPos;
    }
    if (mLOBLocCType == SQL_C_BLOB_LOCATOR)
    {
        sTargetCType = SQL_C_BINARY;
        sValue = mBinBuf;
    }
    else /* (mLOBLocCType == SQL_C_CLOB_LOCATOR) */
    {
        sTargetCType = SQL_C_CHAR;
        /* BUG-21064 : CLOB type CSV up/download error */
        if( ( sHandle->mProgOption->mRule == csv ) &&
            ( sHandle->mProgOption->mUseLOBFile != ILO_TRUE ) )
        {
            sValue = mBinBuf;
        }
        else
        {
            sValue = mChBuf;
        }
    }
    IDE_TEST_RAISE( sHandle->mSQLApi->GetLOB( mLOBLocCType,
                                              mLOBLoc, 
                                              mPos,
                                              sForLength,
                                              sTargetCType,
                                              sValue,
                                              ILO_LOB_PIECE_SIZE,
                                              &sValueLength)
                                              != IDE_SUCCESS, GetLOBError);



    /* 3.Update current position in LOB. */
    mPos += sValueLength;

    /* 4.If column's type is BLOB and user buffer's type is char,
     * convert binary data to hexadecimal character string. */
    if (mLOBLocCType == SQL_C_BLOB_LOCATOR && mValCType == SQL_C_CHAR)
    {
        ConvertBinaryToChar(sValueLength);
        sValueLength *= 2;
    }
    /* BUG-21064 : CLOB type CSV up/download error */
    else
    {
        if ( CLOB4CSV && ( sHandle->mProgOption->mUseLOBFile != ILO_TRUE ) )
        {
            // ���� CLOB data�� CSV���·� ��ȯ�Ѵ�.
             iloConvertCharToCSV( sHandle, &sValueLength);
        }
    }

    /* 5.Make return values. */
    if (mValCType == SQL_C_BINARY)
    {
        *(UChar **)aVal = mBinBuf;
    }
    else /* (mValCType == SQL_C_CHAR) */
    {
        *(SChar **)aVal = mChBuf;
    }
    *aStrLen = (UInt)sValueLength;

    mIsFetchError = ILO_FALSE;
    
   
    return IDE_SUCCESS;

    IDE_EXCEPTION(AllFetched);
    {
        IDE_TEST_RAISE( sHandle->mSQLApi->FreeLOB(mLOBLoc) != IDE_SUCCESS,
                        FreeLOBError);
 
        mIsFetchError = ILO_FALSE;
    }
    IDE_EXCEPTION(GetLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/

        mIsFetchError = ILO_TRUE;
    }
    IDE_EXCEPTION(FreeLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * GetBuffer.
 *
 * LOB ���� �� ȣ��Ǹ�,
 * ������ ������ LOB �����͸� ������ ���۸� ��´�.
 *
 * @param[out] aVal
 *  ������ ������ LOB �����͸� ������ ����.
 * @param[out] aBufLen
 *  ���� ����.
 */
IDE_RC iloLOB::GetBuffer(void **aVal, UInt *aBufLen)
{
    if (mLOBLocCType == SQL_C_BLOB_LOCATOR)
    {
        if (mValCType == SQL_C_BINARY)
        {
            *(UChar **)aVal = mBinBuf;
            // BUG-21837 LOB ���� ����� �����Ѵ�.
            *aBufLen = ILO_LOB_PIECE_SIZE;
        }
        else /* (mValCType == SQL_C_CHAR) */
        {
            *(SChar **)aVal = mChBuf;
            *aBufLen = ILO_LOB_PIECE_SIZE * 2;
        }
    }
    else /* (mLOBLocCType == SQL_C_CLOB_LOCATOR) */
    {
        *(SChar **)aVal = mChBuf;
        *aBufLen = ILO_LOB_PIECE_SIZE;
    }

    return IDE_SUCCESS;
}

/**
 * Append.
 *
 * LOB ���� �� ȣ��Ǹ�,
 * GetBuffer()�� ���� ���ۿ� ����� �����͸� ������ �����Ͽ�
 * �ش� LOB�� �޺κп� ���ٿ� ����.
 *
 * @param[in] aStrLen
 *  ���ۿ� ����Ǿ��ִ� �������� ����.
 *  ������ �����ʹ� NULL�� ����� �ʿ䰡 ������,
 *  �� �� ���� NULL ���� ���ڸ� ������ �����̴�.
 */
IDE_RC iloLOB::Append( ALTIBASE_ILOADER_HANDLE  aHandle,
                       UInt                     aStrLen,
                       iloSQLApi               *aISPApi)
{
    SQLSMALLINT  sSourceCType;
    void        *sValue;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    /* 1.If column's type is BLOB and user buffer's type is char,
     * convert hexadecimal character string to binary data. */
    if (mLOBLocCType == SQL_C_BLOB_LOCATOR && mValCType == SQL_C_CHAR)
    {
        IDE_TEST(ConvertCharToBinary( sHandle, aStrLen) != IDE_SUCCESS);
        aStrLen /= 2;
    }
    /* BUG-21064 : CLOB type CSV up/download error */
    else
    {
        if ( CLOB4CSV && ( sHandle->mProgOption->mUseLOBFile != ILO_TRUE ) )
        {
            // ù CLOB data block�� CSV �������� �ƴ��� �Ǻ�.
            if( (mIsBeginCLOBAppend == ILO_TRUE) &&
                (mChBuf[0] == sHandle->mProgOption->mCSVEnclosing) )
            {
                mIsCSVCLOBAppend   = ILO_TRUE;
            }

            if( mIsCSVCLOBAppend == ILO_TRUE )
            {
                IDE_TEST(iloConvertCSVToChar( sHandle, &aStrLen) != IDE_SUCCESS);
            }
            else
            {
                // CSV ������ �ƴ� ��� ���� ó������� ����.
            }
        }
    }

    /* 2.Call SQLPutLob(). */
    if (mLOBLocCType == SQL_C_BLOB_LOCATOR)
    {
        sSourceCType = SQL_C_BINARY;
        sValue = mBinBuf;
    }
    else /* (mLOBLocCType == SQL_C_CLOB_LOCATOR) */
    {
        sSourceCType = SQL_C_CHAR;
        /* BUG-21064 : CLOB type CSV up/download error */
        if( mIsCSVCLOBAppend == ILO_TRUE )
        {
            sValue = mBinBuf;
        }
        else
        {
            sValue = mChBuf;
        }
    }
    IDE_TEST_RAISE(aISPApi->PutLOB(mLOBLocCType, mLOBLoc, (SInt)mPos, 0,
                                  sSourceCType, sValue, (SInt)aStrLen)
                   != IDE_SUCCESS, PutLOBError);

    /* 3.Update current position in LOB. */
    mPos += aStrLen;

    return IDE_SUCCESS;

    IDE_EXCEPTION(PutLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {    
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * ConvertBinaryToChar.
 *
 * LOB �б� �� Fetch()���� ȣ��Ǹ�,
 * BLOB�� SQL_C_CHAR�� ���ۿ� ������ ���
 * �����κ��� ���� raw ���� �����͸� 16���� ��Ʈ������ ��ȯ�Ѵ�.
 *
 * @param[in] aBinLen
 *  ���� ������ ���ۿ� ����Ǿ��ִ� raw ���� �������� ����.
 */
void iloLOB::ConvertBinaryToChar(UInt aBinLen)
{
    UInt sBinIdx;
    UInt sChIdx;
    UInt sNibble;

    for (sBinIdx = sChIdx = 0; sBinIdx < aBinLen; sBinIdx++)
    {
        sNibble = mBinBuf[sBinIdx] / 16;
        if (sNibble < 10)
        {
            mChBuf[sChIdx++] = (SChar)(sNibble + '0');
        }
        else
        {
            mChBuf[sChIdx++] = (SChar)(sNibble - 10 + 'A');
        }

        sNibble = mBinBuf[sBinIdx] % 16;
        if (sNibble < 10)
        {
            mChBuf[sChIdx++] = (SChar)(sNibble + '0');
        }
        else
        {
            mChBuf[sChIdx++] = (SChar)(sNibble - 10 + 'A');
        }
    }
}

/**
 * ConvertCharToBinary.
 *
 * LOB ���� �� Append()���� ȣ��Ǹ�,
 * SQL_C_CHAR�� ������ �����͸� BLOB�� ������ ���
 * 16���� ��Ʈ�� ������ �����͸� raw ���� �����ͷ� ��ȯ�Ѵ�.
 *
 * @param[in] aChLen
 *  Char ���ۿ� ����Ǿ��ִ� 16���� ��Ʈ���� ����.
 */
IDE_RC iloLOB::ConvertCharToBinary( ALTIBASE_ILOADER_HANDLE aHandle, UInt aChLen)
{
    SChar sCh;
    UInt  sBinIdx;
    UInt  sChIdx;
    UInt  sHiNibble;
    UInt  sLoNibble;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_TEST_RAISE(aChLen % 2 == 1, IllegalChLenError);

    for (sChIdx = sBinIdx = 0; sChIdx < aChLen; sChIdx += 2)
    {
        sCh = mChBuf[sChIdx];
        if ('0' <= sCh && sCh <= '9')
        {
            sHiNibble = (UInt)(sCh - '0');
        }
        else if ('A' <= sCh && sCh <= 'F')
        {
            sHiNibble = (UInt)(sCh - 'A' + 10);
        }
        else
        {
            IDE_RAISE(IllegalHexCharError);
        }

        sCh = mChBuf[sChIdx + 1];
        if ('0' <= sCh && sCh <= '9')
        {
            sLoNibble = (UInt)(sCh - '0');
        }
        else if ('A' <= sCh && sCh <= 'F')
        {
            sLoNibble = (UInt)(sCh - 'A' + 10);
        }
        else
        {
            IDE_RAISE(IllegalHexCharError);
        }

        mBinBuf[sBinIdx++] = (UChar)(sHiNibble * 16 + sLoNibble);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(IllegalChLenError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Invalid_Value_Len);

        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION(IllegalHexCharError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Invalid_Value);

        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* BUG-21064 : CLOB type CSV up/download error */
/**
 * iloConvertCharToCSV.
 *
 * CLOB �б� �� Fetch()���� ȣ��Ǹ�,
 * CLOB�� ���ۿ� ������ ���
 * �����κ��� char �����͸� CSV ���·� ��ȯ�Ѵ�.
 *
 * @param[in] aValueLength
 *  ���� CLOB �������� ����.
 */
void iloLOB::iloConvertCharToCSV( ALTIBASE_ILOADER_HANDLE  aHandle,
                                  UInt                    *aValueLen )
{
    SChar sEnclosing;
    UInt  i;
    UInt  j;
    UInt  sIndex;
    UInt  sLen;
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    sEnclosing = sHandle->mProgOption->mCSVEnclosing;
    i = j = 0;
    sIndex = 0;
    sLen = *aValueLen;

    while ( i < sLen )
    {
        if( *(mBinBuf + i) == sEnclosing )
        {
            if( i == 0 )
            {
                mChBuf[sIndex++] = sEnclosing;
                // escape ���ڰ� �߰��Ǿ� ��ȯ�� ���ڿ����̰� 1����.
                (*aValueLen) ++;
            }
            else
            {
                idlOS::strncpy( mChBuf + sIndex, (const SChar *)mBinBuf + j, i - j );
                sIndex += i - j;
                mChBuf[sIndex++] = sEnclosing;
                // escape ���ڰ� �߰��Ǿ� ��ȯ�� ���ڿ����̰� 1����.
                (*aValueLen) ++;
            }
            j = i;
        }
        if( i == (sLen - 1) )
        {
            idlOS::strncpy( mChBuf + sIndex, (const SChar *)mBinBuf + j, i - j + 1 );
        }
        i++;
    }
}

/* BUG-21064 : CLOB type CSV up/download error */
/**
 * iloConvertCSVToChar.
 *
 * CLOB upload �� Append()���� ȣ��Ǹ�,
 * datafile �� ����� CSV������ CLOB data��
 * ���� data���·� ��ȯ�Ѵ�.
 *
 * @param[in] aStrLen
 *  CSV CLOB �������� ����.
 */
IDE_RC iloLOB::iloConvertCSVToChar( ALTIBASE_ILOADER_HANDLE  aHandle,
                                    UInt                    *aStrLen )
{
    SChar sCh;
    UInt  sBinIdx;
    UInt  sChIdx;
    UInt  sLen;

    sBinIdx = 0;
    sChIdx  = 0;
    sLen    = *aStrLen;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    // CLOB �÷��� �׻� "�� enclosing�Ǿ� �־�, ù���� "�� �����ؾ��Ѵ�.
    if( mIsBeginCLOBAppend  == ILO_TRUE )
    {
        sChIdx++;
        mIsBeginCLOBAppend = ILO_FALSE;
    }
    // ù��° ���� ������ ���ڰ� "�ϰ�� ���� �� ó���� "�� ���� ������ �ʰ� �����ؾ��Ѵ�.
    else if( mSaveBeginCLOBEnc == ILO_TRUE )
    {
        IDE_TEST( mChBuf[ sChIdx++ ] != sHandle->mProgOption->mCSVEnclosing );
        mBinBuf[sBinIdx++] = sHandle->mProgOption->mCSVEnclosing;
        mSaveBeginCLOBEnc = ILO_FALSE;
    }
    else
    {
        // do nothing
    }

    for( ; sChIdx < sLen ; sChIdx++ )
    {
        sCh = mChBuf[sChIdx];
        if ( sCh == sHandle->mProgOption->mCSVEnclosing )
        {
            if( sChIdx == (ILO_LOB_PIECE_SIZE - 1) )
            {
                if( mChBuf[sChIdx-1] != sHandle->mProgOption->mCSVEnclosing )
                {
                    mSaveBeginCLOBEnc = ILO_TRUE;
                }
            }
            else
            {
                if( sChIdx == (sLen - 1) )
                {
                    // do nothing
                }
                else
                {
                    IDE_TEST( mChBuf[ ++sChIdx ] != sHandle->mProgOption->mCSVEnclosing );
                    IDE_TEST( sBinIdx >= ILO_LOB_PIECE_SIZE );
                    mBinBuf[ sBinIdx++ ] = mChBuf[ sChIdx ];
                }
            }
        }
        else
        {
            IDE_TEST( sBinIdx >= ILO_LOB_PIECE_SIZE );
            mBinBuf[ sBinIdx++ ] = mChBuf[ sChIdx ];
        }
    }

    *aStrLen = sBinIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


iloDataFile::iloDataFile( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    idlOS::memset(mDataFilePath, 0, ID_SIZEOF(mDataFilePath));
    m_DataFp = NULL;
    mDataFileNo = -1;
    idlOS::strcpy(m_FieldTerm, "^");
    m_nFTLen = (SInt)idlOS::strlen(m_FieldTerm);
    mFTPhyLen = GetStrPhyLen(m_FieldTerm);
    mFTLexStateTransTbl = NULL;
    idlOS::strcpy(m_RowTerm, "\n");
    m_nRTLen = (SInt)idlOS::strlen(m_RowTerm);;
    mRTPhyLen = GetStrPhyLen(m_RowTerm);;
    mRTLexStateTransTbl = NULL;
    m_SetEnclosing = SQL_FALSE;
    idlOS::strcpy(m_Enclosing, "");
    mEnLexStateTransTbl = NULL;
    /* TASK-2657 */
    // BUG-27633: mErrorToken�� �÷��� �ִ� ũ��� �Ҵ�
    mErrorToken = (SChar*) idlOS::malloc(MAX_TOKEN_VALUE_LEN + MAX_SEPERATOR_LEN);
    IDE_TEST_RAISE(mErrorToken == NULL, MAllocError);
    mErrorToken[0] = '\0';
    mErrorTokenMaxSize = (MAX_TOKEN_VALUE_LEN + MAX_SEPERATOR_LEN);
    m_SetNextToken = SQL_FALSE;
    mLOBColExist = ILO_FALSE;
    mLOB = NULL;
    mLOBFP = NULL;
    mLOBFileNo = 0;
    mLOBFilePos = ID_ULONG(0);
    mAccumLOBFilePos = ID_ULONG(0);
    mFileNameBody[0] = '\0';
    mDataFileNameExt[0] = '\0';
    // BUG-18803 readsize �ɼ� �߰�
    // mDoubleBuff�� readsize ���� ���߾ ���� �Ҵ��� �Ѵ�.
    mDoubleBuffPos = -1;              //BUG-22434
    mDoubleBuffSize = 0;  //BUG-22434
    mDoubleBuff     = NULL;
    /* BUG-24358 iloader Geometry Data */    
    m_TokenValue = (SChar*) idlOS::malloc(MAX_TOKEN_VALUE_LEN + MAX_SEPERATOR_LEN);
    IDE_TEST_RAISE(m_TokenValue == NULL, MAllocError);
    m_TokenValue[0] = '\0';
    mTokenMaxSize = (MAX_TOKEN_VALUE_LEN + MAX_SEPERATOR_LEN);
    mLOBFile = NULL;            //BUG-24583
    mLOBFileRowCount = 0;
    mLOBFileColumnNum = 0;      //BUG-24583
    mOutFileFP = NULL;          //PROJ-2030, CT_CASE-3020 CHAR outfile ����

    return;

    IDE_EXCEPTION(MAllocError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    if (mErrorToken != NULL)
    {
        idlOS::free(mErrorToken);
        mErrorToken = NULL;
    }

    if ( sHandle->mUseApi != SQL_TRUE )
    {
        // ��ü�� �ʱ�ȭ�� �� �޸� �Ҵ翡 �����ؼ��� �ȵȴ�.
        exit(1);
    }
}

iloDataFile::~iloDataFile()
{
    FreeAllLexStateTransTbl();
    if (mLOB != NULL)
    {
        delete mLOB;
    }
    /* BUG-24358 iloader Geometry Data */    
    if (m_TokenValue != NULL)
    {
        idlOS::free(m_TokenValue);
        mTokenMaxSize = 0;
    }
    // BUG-27633: mErrorToken�� �÷��� �ִ� ũ��� �Ҵ�
    if (mErrorToken != NULL)
    {
        idlOS::free(mErrorToken);
        mErrorToken = NULL;
        mErrorTokenMaxSize = 0;
    }
    
    //BUG-24583
    if ( mLOBFile != NULL )
    {
        LOBFileInfoFree();
    }

    // BUG-18803 readsize �ɼ� �߰�
    if(mDoubleBuff != NULL)
    {
        idlOS::free(mDoubleBuff);
    }
}

/**
 * ����Ÿ ������ ��ü ��ηκ��� ��θ� �����ؼ� �����صд�.
 *
 * @param [IN] aDataFileName
 *             ����Ÿ ������ ��ü ���
 */
void iloDataFile::InitDataFilePath(SChar *aDataFileName)
{
    SChar *sPtr;
    SInt   sPos;

    sPtr = idlOS::strrchr(aDataFileName, IDL_FILE_SEPARATOR);
    if (sPtr != NULL)
    {
        sPos = (sPtr - aDataFileName) + 1;
        (void)idlOS::strncpy(mDataFilePath, aDataFileName, sPos);
        mDataFilePath[sPos] = '\0';
    }
}

SInt iloDataFile::OpenFileForUp( ALTIBASE_ILOADER_HANDLE  aHandle,
                                 SChar                   *aDataFileName,
                                 SInt                     aDataFileNo,
                                 iloBool                   aIsWr, 
                                 iloBool                   aLOBColExist )
{
    SChar  sFileName[MAX_FILEPATH_LEN];
    SChar  sMode[3];

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    /* ������ ���ϸ����κ��� ���ϸ� ��ü, ������ ���� Ȯ����,
     * ������ ���� ��ȣ�� ��´�. */
    AnalDataFileName(aDataFileName, aIsWr);
    /* ������ ���Ͽ� ������ ��� ����ڰ� ���ڷ� �� ������ ���� ��ȣ�� ��� */
    if (aIsWr == ILO_TRUE)
    {
        mDataFileNo = aDataFileNo;
    }
    mLOBColExist = aLOBColExist;

    if (mDataFileNo < 0)
    {
        (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName), "%s%s",
                              mFileNameBody, mDataFileNameExt);
    }
    else
    {
        (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                              "%s%s%"ID_INT32_FMT, mFileNameBody,
                              mDataFileNameExt, mDataFileNo);
    }

    if (aIsWr == ILO_TRUE)
    {
        //BUG-24511 ��� Fopen�� binary type���� �����ؾ���
        (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "wb");
    }
    else
    {
        /* To Fix BUG-14940 �ѱ��� ���� �����Ϳ� ����
         *                  iloader���� import�� �߰��� �����. */
        (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "rb");
    }
    m_DataFp = iloFileOpen( sHandle, sFileName, O_CREAT | O_RDWR, sMode, LOCK_WR);
    IDE_TEST_RAISE(m_DataFp == NULL, OpenError);
    mDataFilePos = ID_ULONG(0);
    // BUG-24873 clob ������ �ε��Ҷ� [ERR-91044 : Error occurred during data file IO.] �߻�
    mDataFileRead = ID_ULONG(0);

    if (aIsWr == ILO_FALSE)
    {
        m_SetNextToken = SQL_FALSE;
        IDE_TEST_RAISE(MakeAllLexStateTransTbl( sHandle ) != IDE_SUCCESS,
                       LexStateTransTblMakeFail);
    }

    if (mLOBColExist == ILO_TRUE)
    {
        IDE_TEST_RAISE( InitLOBProc(sHandle, aIsWr) != IDE_SUCCESS, LOBProcInitError);
    }

    // BUG-18803 readsize �ɼ� �߰�
    // mDoubleBuff�� readsize ���� ���߾ ���� �Ҵ��� �Ѵ�.
    if(mDoubleBuff != NULL)
    {
        idlOS::free(mDoubleBuff);
    }
    mDoubleBuffPos = -1;
    mDoubleBuffSize = sHandle->mProgOption->mReadSzie;

    mDoubleBuff = (SChar*)idlOS::malloc(mDoubleBuffSize +1);
    IDE_TEST_RAISE(mDoubleBuff == NULL, NO_MEMORY);

    idlOS::memset(mDoubleBuff, 0, mDoubleBuffSize +1);

    InitDataFilePath(sFileName);

    return SQL_TRUE;

    IDE_EXCEPTION(OpenError);
    {
        if (uteGetErrorCODE(sHandle->mErrorMgr) != 0x91100)
        {
            uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_openFileError,
                    sFileName);

            if ( sHandle->mUseApi != SQL_TRUE )
            {
                utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            }
        }
        else
        {
            uteSetErrorCode( sHandle->mErrorMgr,utERR_ABORT_File_Lock_Error,
                    sFileName);

            if ( sHandle->mUseApi != SQL_TRUE )
            {
                utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            }
        }
    }
    IDE_EXCEPTION(LexStateTransTblMakeFail)
    {
        if (idlOS::fclose(m_DataFp) == 0)
        {
            m_DataFp = NULL;
        }
    }
    IDE_EXCEPTION(LOBProcInitError);
    {
        if (idlOS::fclose(m_DataFp) == 0)
        {
            m_DataFp = NULL;
        }
        FreeAllLexStateTransTbl();
    }
    IDE_EXCEPTION(NO_MEMORY);
    {
        mDoubleBuffSize = 0;
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stderr, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

FILE* iloDataFile::OpenFileForDown( ALTIBASE_ILOADER_HANDLE  aHandle,
                                    SChar                   *aDataFileName,
                                    SInt                     aDataFileNo, 
                                    iloBool                   aIsWr, 
                                    iloBool                   aLOBColExist )
{
    FILE   *spFile;
    SChar  sFileName[MAX_FILEPATH_LEN];
    SChar  sMode[3];
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    /* ������ ���ϸ����κ��� ���ϸ� ��ü, ������ ���� Ȯ����,
     * ������ ���� ��ȣ�� ��´�. */
    AnalDataFileName(aDataFileName, aIsWr);
    /* ������ ���Ͽ� ������ ��� ����ڰ� ���ڷ� �� ������ ���� ��ȣ�� ��� */
    if (aIsWr == ILO_TRUE)
    {
        mDataFileNo = aDataFileNo;
    }
    mLOBColExist = aLOBColExist;

    if (mDataFileNo < 0)
    {
        (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName), "%s%s",
                              mFileNameBody, mDataFileNameExt);
    }
    else
    {
        (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                              "%s%s%"ID_INT32_FMT, mFileNameBody,
                              mDataFileNameExt, mDataFileNo);
    }

    if (aIsWr == ILO_TRUE)
    {
        //BUG-24511 ��� Fopen�� binary type���� �����ؾ���
        (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "wb");
    }
    else
    {
        /* To Fix BUG-14940 �ѱ��� ���� �����Ϳ� ����
         *                  iloader���� import�� �߰��� �����. */
        (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "rb");
    }
    spFile = iloFileOpen( sHandle, sFileName, O_CREAT | O_RDWR, sMode, LOCK_WR);
    IDE_TEST_RAISE(spFile == NULL, OpenError);
    
    //PROJ-1714
    //LOB Column�� ���� ��쿡��, LOB File�� ó���ϴ� ������ ����� �� �ֵ��� �Ѵ�.
    if(aLOBColExist == ILO_TRUE)
    {
        m_DataFp = spFile;
    }
    
    mDataFilePos = ID_ULONG(0);
    
    if (aIsWr == ILO_FALSE)
    {
        m_SetNextToken = SQL_FALSE;
        IDE_TEST_RAISE(MakeAllLexStateTransTbl( sHandle ) != IDE_SUCCESS,
                       LexStateTransTblMakeFail);
    }

    if (mLOBColExist == ILO_TRUE)
    {
        IDE_TEST_RAISE(InitLOBProc(sHandle, aIsWr) != IDE_SUCCESS, LOBProcInitError);
    }

    InitDataFilePath(sFileName);

    return spFile;

    IDE_EXCEPTION(OpenError);
    {
        /* BUG-43432 Error Message is printed out in the function which calls this function.
        if (uteGetErrorCODE( sHandle->mErrorMgr) != 0x91100) //utERR_ABORT_File_Lock_Error
        {
            if ( sHandle->mUseApi != SQL_TRUE )
            {
                (void)idlOS::printf("Data File [%s] open fail\n", sFileName);
            }
        }
        else
        {
            if ( sHandle->mUseApi != SQL_TRUE )
            {
                utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            }
        }
        */
    }
    IDE_EXCEPTION(LexStateTransTblMakeFail)
    {
        if (idlOS::fclose(spFile) == 0)
        {
            spFile = NULL;
        }
    }
    IDE_EXCEPTION(LOBProcInitError);
    {
        if (idlOS::fclose(spFile) == 0)
        {
            spFile = NULL;
        }
        FreeAllLexStateTransTbl();
    }
    IDE_EXCEPTION_END;
    
    return NULL;
}

SInt iloDataFile::CloseFileForUp( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    FreeAllLexStateTransTbl();
    if (mLOBColExist == ILO_TRUE)
    {
        IDE_TEST_RAISE(FinalLOBProc(sHandle) != IDE_SUCCESS, LOBProcFinalError);
    }
    IDE_TEST_RAISE(idlOS::fclose(m_DataFp) != 0, CloseError);
    m_DataFp = NULL;

    return SQL_TRUE;

    IDE_EXCEPTION(LOBProcFinalError);
    {
        if (idlOS::fclose(m_DataFp) == 0)
        {
            m_DataFp = NULL;
        }
    }
    IDE_EXCEPTION(CloseError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        (void)idlOS::printf("Data file close fail\n");
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

SInt iloDataFile::CloseFileForDown( ALTIBASE_ILOADER_HANDLE aHandle, FILE *fp)
{
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (mLOBColExist == ILO_TRUE)
    {
        IDE_TEST_RAISE(FinalLOBProc(sHandle) != IDE_SUCCESS, LOBProcFinalError);
    }
    IDE_TEST_RAISE(idlOS::fclose(fp) != 0, CloseError);
    fp = NULL;

    return SQL_TRUE;

    IDE_EXCEPTION(LOBProcFinalError);
    {
        if (idlOS::fclose(fp) == 0)
        {
            fp = NULL;
        }
    }
    IDE_EXCEPTION(CloseError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("Data file close fail\n");
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

void iloDataFile::SetTerminator(SChar *szFiledTerm, SChar *szRowTerm)
{
    idlOS::strcpy(m_FieldTerm, szFiledTerm);
    m_nFTLen = (SInt)idlOS::strlen(m_FieldTerm);
    mFTPhyLen = GetStrPhyLen(m_FieldTerm);
    idlOS::strcpy(m_RowTerm, szRowTerm);
    m_nRTLen = (SInt)idlOS::strlen(m_RowTerm);
    mRTPhyLen = GetStrPhyLen(m_RowTerm);
}

void iloDataFile::SetEnclosing(SInt bSetEnclosing, SChar *szEnclosing)
{
    m_SetEnclosing = bSetEnclosing;
    if (m_SetEnclosing)
    {
        idlOS::strcpy(m_Enclosing, szEnclosing);
        m_nEnLen = (SInt)idlOS::strlen(m_Enclosing);
        mEnPhyLen = GetStrPhyLen(m_Enclosing);
    }
}



SInt iloDataFile::PrintOneRecord( ALTIBASE_ILOADER_HANDLE  aHandle,
                                  SInt                     aRowNo, 
                                  iloColumns              *pCols,
                                  iloTableInfo            *pTableInfo, 
                                  FILE                    *aWriteFp, 
                                  SInt                     aArrayNum ) //PROJ-1714
{
    SChar  mMsSqlBuf[128];
    SChar *pos;
    SChar *microSecond;
    SChar  miliSecond[128];
    SInt   i;
    UInt   sWrittenLen;
    EispAttrType sAttrType;
    SChar *sValuePtr = NULL;
    UInt   sValueLen = 0;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    for (i=0; i < pCols->GetSize(); i++)
    {    
        sAttrType = pTableInfo->GetAttrType(i);
        if (m_SetEnclosing)
        {
            IDE_TEST_RAISE( idlOS::fwrite(m_Enclosing, m_nEnLen, 1, aWriteFp)
                            != (UInt)1, WriteError );
        }

        // switch�� ������ �������� ���Ǵ� buffer pointer �ʱ�ȭ
        sValuePtr = (SChar*)(pCols->m_Value[i])
                  + (aArrayNum * (SInt)pCols->mBufLen[i]);
        // ���⼭ ������ ���̰��� �ٽ� ���� �� �����Ƿ� �ؿ��� ������ �ٽ� �����ؾ� �Ѵ�
        sValueLen = (UInt)*(pCols->m_Len[i] + aArrayNum);
        
        switch (pCols->GetType(i))
        {
            case SQL_NUMERIC : /* SQLBindCol���� SQL_C_CHAR�� ������ */
            case SQL_DECIMAL :
            case SQL_FLOAT   :
                if (*(pCols->m_Len[i] + aArrayNum)  != SQL_NULL_DATA)
                {
                    // BUG-25827 iloader�� Default �� noexp �ɼ��� �پ����� ���ڽ��ϴ�.
                    /*
                    if (gProgOption.mNoExp != SQL_TRUE &&
                        pTableInfo->mNoExpFlag[i] != ID_TRUE)
                    {
                        ConvNumericNormToExp(pCols, i, aArrayNum);
                    }
                    */
                    // �������� ���̰��� ���Ҽ� �����Ƿ� ���⼭ ���̰� ������ ����
                    sValueLen = (UInt)*(pCols->m_Len[i] + aArrayNum);
                    IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen, 1, aWriteFp)
                                    != (UInt)1, WriteError );
                }
                break;
            case SQL_CHAR :
            case SQL_WCHAR :
            case SQL_VARCHAR :
            case SQL_WVARCHAR :
            case SQL_SMALLINT:
            case SQL_INTEGER :
            case SQL_BIGINT  :
            case SQL_REAL   :
            case SQL_DOUBLE  :
            case SQL_NIBBLE :
            case SQL_BYTES :
            case SQL_VARBYTE :
            case SQL_DATE :
            case SQL_TIMESTAMP :
            case SQL_TYPE_DATE :
            case SQL_TYPE_TIMESTAMP:
                if (*(pCols->m_Len[i] + aArrayNum) == SQL_NULL_DATA)
                {
                    break;
                }

                // TruncTrailingSpaces()  �ȿ��� length���� ���� �� �����Ƿ�
                // ���⼭ ���� ���̰��� ����
                sValueLen = (UInt)*(pCols->m_Len[i] + aArrayNum);
                /* fix CASE-4061 */
                if ( (sAttrType == ISP_ATTR_DATE) &&
                    ( sHandle->mProgOption->mMsSql == SQL_TRUE) )
                {
                    (void)idlOS::snprintf(mMsSqlBuf, ID_SIZEOF(mMsSqlBuf), "%s",
                                        sValuePtr);
                    pos = idlOS::strrchr(mMsSqlBuf, ':');
                    if ( pos != NULL )
                    {
                        *pos          = '\0';
                        microSecond   = pos+1;
                        if ( idlOS::strlen(microSecond) > 3 )
                        {
                            idlOS::strncpy(miliSecond, microSecond, 3);
                            miliSecond[3] ='\0';
                            sWrittenLen = (UInt)idlOS::fprintf(aWriteFp, "%s.%s",
                                                               mMsSqlBuf,
                                                               miliSecond);
                            IDE_TEST_RAISE(sWrittenLen
                                           < (UInt)idlOS::strlen(mMsSqlBuf) + 4,
                                           WriteError);
                        }
                        else
                        {
                            IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen, 1, aWriteFp)
                                            != (UInt)1, WriteError );
                        }
                    }
                    else
                    {
                        IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen, 1, aWriteFp)
                                        != (UInt)1, WriteError );
                    }
                }
                else if( (sAttrType == ISP_ATTR_DATE) ||
                         (sAttrType == ISP_ATTR_CHAR) ||
                         (sAttrType == ISP_ATTR_VARCHAR) ||
                         (sAttrType == ISP_ATTR_NCHAR) ||
                         (sAttrType == ISP_ATTR_NVARCHAR) )

                {
                    
                    // cpu�� little-endian�̰� nchar/nvarchar �÷��̰�, nchar_utf16=yes�̸�
                    // big-endian���� ��ȯ��Ų�� (datafile���� big-endian���θ� ����)
#ifndef ENDIAN_IS_BIG_ENDIAN
                    if (((sAttrType == ISP_ATTR_NCHAR) ||
                         (sAttrType == ISP_ATTR_NVARCHAR)) &&
                        ( sHandle->mProgOption->mNCharUTF16 == ILO_TRUE))
                    {
                        pTableInfo->ConvertShortEndian(sValuePtr, sValueLen);
                    }
#endif
                    /* TASK-2657 */
                    if ( sHandle->mProgOption->mRule == csv )
                    {
                        /* the date type might have ',' in the value string, so " " is forced */
                        if ( sAttrType == ISP_ATTR_DATE )
                        {
                            IDE_TEST_RAISE( idlOS::fwrite( &(sHandle->mProgOption->mCSVEnclosing), 1, 1, aWriteFp ) 
                                            != (UInt)1, WriteError);
                            IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen,
                                            1, aWriteFp ) != (UInt)1, WriteError);
                            IDE_TEST_RAISE( idlOS::fwrite( &(sHandle->mProgOption->mCSVEnclosing),
                                            1, 1, aWriteFp ) 
                                            != (UInt)1, WriteError);
                        }
                        else
                        {
                            IDE_TEST_RAISE( csvWrite ( sHandle, 
                                                       sValuePtr,
                                                       sValueLen,
                                                       aWriteFp )
                                                       != SQL_TRUE, WriteError);
                        }
                    }
                    else
                    {
                        IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen, 1, aWriteFp)
                                        != (UInt)1, WriteError );
                    }
                }
                else
                {
                    IDE_TEST_RAISE( idlOS::fwrite(sValuePtr, sValueLen, 1, aWriteFp)
                                    != (UInt)1, WriteError );
                }
                break;
            case SQL_BINARY :
            case SQL_BLOB :
            case SQL_BLOB_LOCATOR :
            case SQL_CLOB :
            case SQL_CLOB_LOCATOR :
                 /* BUG-24358 iloader Geometry Data */
                if (sAttrType == ISP_ATTR_GEOMETRY)
                {
                    // if null, skip.
                    if (*(pCols->m_Len[i] + aArrayNum) == SQL_NULL_DATA)
                    {
                        break;
                    }
                    sValueLen = (UInt)*(pCols->m_Len[i] + aArrayNum);

                    if ( sHandle->mProgOption->mRule == csv )
                    {
                        IDE_TEST_RAISE(
                             csvWrite ( sHandle, sValuePtr, sValueLen, aWriteFp )
                            != SQL_TRUE, WriteError);
                    }
                    else /* rule == csv */
                    {
                        IDE_TEST_RAISE(
                            idlOS::fwrite(sValuePtr, sValueLen,
                                          1, aWriteFp )
                            != (UInt)1, WriteError);
                    }
                }
                else // if sAttrType != ISP_ATTR_GEOMETRY
                {
                    /* LOB */
                    IDE_TEST(PrintOneLOBCol(sHandle,
                                            aRowNo, 
                                            pCols, 
                                            i, 
                                            aWriteFp, 
                                            pTableInfo)     //BUG-24583
                             != IDE_SUCCESS);   
                }
                break;
            default :
                IDE_RAISE(UnknownDataTypeError);
        }

        if (m_SetEnclosing)
        {
            IDE_TEST_RAISE( idlOS::fwrite(m_Enclosing, m_nEnLen, 1, aWriteFp)
                            != (UInt)1, WriteError );
        }
        if ( i == pCols->GetSize()-1 )
        {
            if ( sHandle->mProgOption->mInformix == SQL_TRUE )
            {
                /* TASK-2657 */
                if ( sHandle->mProgOption->mRule == csv )
                {
                    IDE_TEST_RAISE( idlOS::fwrite( &(sHandle->mProgOption->mCSVFieldTerm), 1, 1, aWriteFp ) 
                                    != 1, WriteError);
                }
                else
                {
                    IDE_TEST_RAISE( idlOS::fwrite(m_FieldTerm, m_nFTLen, 1, aWriteFp)
                                    != (UInt)1, WriteError );
                }
            }
            /* BUG-29779: csv�� rowterm�� \r\n���� �����ϴ� ��� */
            IDE_TEST_RAISE( idlOS::fwrite(m_RowTerm, m_nRTLen, 1, aWriteFp)
                            != (UInt)1, WriteError );
        }
        else
        {
            /* TASK-2657 */
            if ( sHandle->mProgOption->mRule == csv )
            {
                IDE_TEST_RAISE( idlOS::fwrite( &(sHandle->mProgOption->mCSVFieldTerm), 1, 1, aWriteFp ) 
                                != 1, WriteError);
            }
            else
            {
                IDE_TEST_RAISE( idlOS::fwrite(m_FieldTerm, m_nFTLen, 1, aWriteFp)
                                != (UInt)1, WriteError );
            }
        }
    } //end of Column For Loop
    
    return SQL_TRUE;

    IDE_EXCEPTION(WriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("Data file write fail\n");
        }
    }
    IDE_EXCEPTION(UnknownDataTypeError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Unkown_Datatype_Error, "");
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return SQL_FALSE;
}

/* TASK-2657 */
SInt iloDataFile::csvWrite ( ALTIBASE_ILOADER_HANDLE  aHandle,
                             SChar                   *aValue,
                             UInt                     aValueLen,
                             FILE                    *aWriteFp )
{
    SChar sEnclosing;
    UInt  i;
    UInt  j;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_TEST_RAISE((aValue == NULL) || (aWriteFp == NULL), WriteError);

    sEnclosing = sHandle->mProgOption->mCSVEnclosing;

    i = j = 0;


    /* if the type is char or varchar, " " is forced. */
    while ( i < aValueLen )
    {
       if( *(aValue + i) == sEnclosing )
       {
           if( i == 0 )
           {
               IDE_TEST_RAISE(idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp ) 
                              != (UInt)1, WriteError);
           }
           else
           {
               if ( j == 0 )
               {
                   /* write heading quote, except for the i==0 case. */
                   IDE_TEST_RAISE(idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp )
                                  != (UInt)1, WriteError);
               }
               IDE_TEST_RAISE( idlOS::fwrite( aValue + j, i - j, 1, aWriteFp )
                               != (UInt)1, WriteError);
               IDE_TEST_RAISE( idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp )
                               != (UInt)1, WriteError);
           }
           j = i;
       }
       if( i == aValueLen - 1 )
       {
           if ( j == 0 )
           {
               IDE_TEST_RAISE( idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp )
                               != (UInt)1, WriteError);
               IDE_TEST_RAISE( idlOS::fwrite( aValue, aValueLen, 1, aWriteFp )
                               != (UInt)1, WriteError);
               IDE_TEST_RAISE( idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp )
                               != (UInt)1, WriteError);
           }
           else
           {
               IDE_TEST_RAISE( idlOS::fwrite( aValue + j, i - j + 1, 1, aWriteFp )
                               != (UInt)1, WriteError);
               IDE_TEST_RAISE( idlOS::fwrite( &sEnclosing, 1, 1, aWriteFp )
                               != (UInt)1, WriteError);
           }
       }
       i++;
    }
    return SQL_TRUE;

    IDE_EXCEPTION(WriteError);
    {
        // �� �Լ��� ȣ���ϴ� ������ ������ ó���ϹǷ� ���⼭�� ����
    }
    IDE_EXCEPTION_END;
    return SQL_FALSE;
}

/**
 * ConvNumericNormToExp.
 *
 * ���ڿ� ���·� ����� NUMERIC �÷� ����
 * ��������([-]ddd.ddd)���� ������([-]d.dddE+dd)���� ��ȯ�Ѵ�.
 *
 * @param[in,out] aCols
 *  �÷� �� �� �÷� ���� ���̰� ����Ǿ��ִ� ����ü.
 * @param[in] aColIdx
 *  aCols ����ü ������ �� ��° �÷������� �����ϴ� �÷� �ε���.
 */
void iloDataFile::ConvNumericNormToExp(iloColumns *aCols, SInt aColIdx, SInt aArrayNum)
{
    SChar *sPeriodPtr;
    SChar *sStr;
    UInt   sDigitIdx;
    UInt   sExp;
    UInt   sRDigitIdx;
    UInt   sStrLen;

    /* �̹� �������̸� ��ȯ �ʿ� ����. */
    
    if (idlOS::strchr((SChar*)(aCols->m_Value[aColIdx]) + (aArrayNum * (SInt)aCols->mBufLen[aColIdx]), 'E') != NULL)
    {
        return;
    }

    /* aCols->m_Value[aColIdx]���� ��ȣ�� ������ ���ڿ��� sStr�� ����. */
    if (((SChar*)(aCols->m_Value[aColIdx]) + (aArrayNum * (SInt)aCols->mBufLen[aColIdx]))[0] != '-')
    {
        sStr = (SChar*)(aCols->m_Value[aColIdx]) + (aArrayNum * (SInt)aCols->mBufLen[aColIdx]);
        sStrLen = (UInt)*(aCols->m_Len[aColIdx] + aArrayNum);
    }
    else
    {
        sStr = (SChar*)(aCols->m_Value[aColIdx]) + (aArrayNum * (SInt)aCols->mBufLen[aColIdx]) + 1;
        sStrLen = (UInt)*(aCols->m_Len[aColIdx] + aArrayNum) - 1;
    }

    /* �����ΰ� 1�ڸ��̰� �Ҽ��ΰ� ���� ���, ��ȯ �ʿ� ����. */
    if (sStr[1] == '\0')
    {
        return;
    }

    /* 0.���� �������� �ʴ� ������ ��� */
    if (sStr[0] != '0')
    {
        /* �����ΰ� 1�ڸ��̰� �Ҽ��ΰ� �ִ� ���, ��ȯ �ʿ� ����. */
        if (sStr[1] == '.')
        {
            return;
        }

        sPeriodPtr = idlOS::strchr(sStr, '.');

        /* �Ҽ����� ���� ������ ��� */
        if (sPeriodPtr == NULL)
        {
            sExp = (UInt)(sStrLen - 1);

            for (sRDigitIdx = sStrLen - 1; sStr[sRDigitIdx] == '0';
                 sRDigitIdx--);

            /* �������� ���� ū �ڸ����� ������ ��� �ڸ����� 0�� ��� */
            if (sRDigitIdx == 0)
            {
                sStrLen = 1;
            }
            /* �����ο� 0�� �ƴ� �ڸ����� �� �� �̻��� ��� */
            else
            {
                /* �Ҽ��� ���. */
                (void)idlOS::memmove(sStr + 2, sStr + 1, sRDigitIdx);

                /* �Ҽ��� ���. */
                sStr[1] = '.';

                sStrLen = 2 + sRDigitIdx;
            }
        }
        /* �Ҽ����� �ִ� ������ ��� */
        else /* (sPeriodPtr != NULL) */
        {
            sExp = (UInt)((UInt)(sPeriodPtr - sStr) - 1);

            /* �Ҽ��� ���. */
            (void)idlOS::memmove(sStr + 2, sStr + 1, sExp);

            /* �Ҽ��� ���. */
            sStr[1] = '.';
        }

        /* ������ ���. */
        sStrLen += (UInt)idlOS::sprintf(sStr + sStrLen, "E+%"ID_UINT32_FMT,
                                        sExp);
    }
    /* 0.���� �����ϴ� ������ ��� */
    else /* (sStr[0] == '0') */
    {
        sStr[1] = '0';

        // bug-20800
        sDigitIdx = 0;
        while(sStr[sDigitIdx] == '0')
        {
            ++sDigitIdx;
        }

        sExp = (UInt)(sDigitIdx - 1);

        /* ������ ���. */
        sStr[0] = sStr[sDigitIdx];

        /* 0�� �ƴ� ���ڰ� 1�ڸ����� ��� */
        if (sStr[sDigitIdx + 1] == '\0')
        {
            sStrLen = 1;
        }
        else
        {
            /* �Ҽ��� ���. */
            sStr[1] = '.';

            /* �Ҽ��� ���. */
            (void)idlOS::memmove(sStr + 2, sStr + sDigitIdx + 1,
                                 sStrLen - sDigitIdx - 1);

            sStrLen = (UInt)(sStrLen - sDigitIdx + 1);
        }

        /* ������ ���. */
        sStrLen += (UInt)idlOS::sprintf(sStr + sStrLen, "E-%"ID_UINT32_FMT,
                                        sExp);
    }

    /* aCols->m_Len[aColIdx]�� ��ȯ�� ���ڿ��� ���̷� �缳��. */
    if (((SChar *)aCols->m_Value[aColIdx])[0] != '-')
    {
        *(aCols->m_Len[aColIdx] + aArrayNum) = (SQLLEN)sStrLen;
    }
    else
    {
        *(aCols->m_Len[aColIdx] + aArrayNum) = (SQLLEN)(sStrLen + 1);
    }
  
}

/**
 * GetToken.
 *
 * ������ ���Ϸκ��� �� ���� ��ū�� �б� ���� �Լ��̴�.
 * ��ū�� �������� TEOF, TFIELD_TERM, TROW_TERM, TVALUE�� ������,
 * �� �Լ��� ���ϰ��� �� ���� �� �ϳ��� �ȴ�.
 * ���ϰ��� TVALUE�� ��� m_TokenValue, mTokenLen�� ����
 * ��ū ���ڿ��� ���ڿ��� ���̰� �����ȴ�.
 */
EDataToken iloDataFile::GetTokenFromCBuff( ALTIBASE_ILOADER_HANDLE aHandle )
{
    SChar sChr;
    SInt  sRet = 0;
    UInt  sEnCnt = 0;
    UInt  sEnMatchLen = 0;
    UInt  sFTMatchLen = 0;
    UInt  sRTMatchLen = 0;
    SInt  sTmp = 0;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    mTokenLen = 0;

    IDE_TEST_RAISE(m_SetNextToken == SQL_TRUE, NextTokenExist);

    /* m_TokenValue ���ۿ��� ���� �����ڰ� ���ÿ� ����� �� �����Ƿ�,
     * m_TokenValue ���� ũ��� ���� �ִ� ũ�� + ������ �ִ� ũ�� ��
     * NULL ���Ṯ�ڸ� ���� �������� �����ȴ�. */
    while (mTokenLen < (mTokenMaxSize - 1) )
    {
        sRet = ReadDataFromCBuff( sHandle, &sChr );    //BUG-22434
        IDE_TEST_RAISE(sRet == -1, EOFReached);
        //BUG-22513
        sTmp = (UChar)sChr;
        
        mDataFileRead++;         //LOB ������ ��� ���Ǹ�, single Threadó���̹Ƿ� Mutex lock�� �ʿ����.. 

        m_TokenValue[mTokenLen++] = (SChar)sChr;

        if (sEnCnt == 0)
        {
            sFTMatchLen = mFTLexStateTransTbl[sFTMatchLen][sTmp];
            if (sFTMatchLen == (UInt)m_nFTLen)
            {
                if (mTokenLen == (UInt)m_nFTLen &&
                    m_SetEnclosing)
                {
                    return TFIELD_TERM;
                }
                else
                {
                    m_SetNextToken = SQL_TRUE;
                    mNextToken = TFIELD_TERM;
                    mTokenLen -= (UInt)m_nFTLen;
                    m_TokenValue[mTokenLen] = '\0';
                    return TVALUE;
                }
            }

            sRTMatchLen = mRTLexStateTransTbl[sRTMatchLen][sTmp];
            if (sRTMatchLen == (UInt)m_nRTLen)
            {
                if (mTokenLen == (UInt)m_nRTLen &&
                    (m_SetEnclosing || sHandle->mProgOption->mInformix))
                {
                    return TROW_TERM;
                }
                else
                {
                    m_SetNextToken = SQL_TRUE;
                    mNextToken = TROW_TERM;
                    mTokenLen -= (UInt)m_nRTLen;
                    m_TokenValue[mTokenLen] = '\0';
                    return TVALUE;
                }
            }
        }

        if (m_SetEnclosing && (sEnCnt == 1 || mTokenLen == sEnMatchLen + 1))
        {
            sEnMatchLen = mEnLexStateTransTbl[sEnMatchLen][sTmp];
            if (sEnMatchLen == (UInt)m_nEnLen)
            {
                if (sEnCnt == 0)
                {
                    sEnCnt++;
                    sEnMatchLen = 0;
                    mTokenLen = 0;
                }
                else
                {
                    mTokenLen -= (UInt)m_nEnLen;
                    m_TokenValue[mTokenLen] = '\0';
                    return TVALUE;
                }
            }
        }
    }

    IDE_RAISE(BufferOverflow);

    IDE_EXCEPTION(NextTokenExist);
    {
        m_SetNextToken = SQL_FALSE;
        return mNextToken;
    }
    IDE_EXCEPTION(EOFReached);
    {
        if (mTokenLen > 0)
        {
            m_SetNextToken = SQL_TRUE;
            mNextToken = TEOF;
            m_TokenValue[mTokenLen] = '\0';
            return TVALUE;
        }
        else
        {
            return TEOF;
        }
    }
    IDE_EXCEPTION(BufferOverflow);
    {
        m_TokenValue[mTokenLen] = '\0';
        return TVALUE;
    }
    IDE_EXCEPTION_END;

    return TEOF;
}


/* TASK-2657 ******************************************************
 * 
 * Description  : parse csv format data, and store the token value
 * Return value : the type of a token 
 *
 ******************************************************************/
EDataToken iloDataFile::GetCSVTokenFromCBuff( ALTIBASE_ILOADER_HANDLE  aHandle,
                                              SChar                   *aColName)
{

    SChar  sEnclosing;
    SChar  sFieldTerm;
    SChar  sChr;
    /* BUG-29779: csv�� rowterm�� \r\n���� �����ϴ� ��� */
    SChar *sRowTerm;
    SChar  sRowTermBK[MAX_SEPERATOR_LEN];   //temparary space for ensuring rowterm.
    SInt   sRowTermInd;                     //sRowTermBK�� index
    SInt   sReadresult = 0;
    UInt   sMaxTokenSize;
    UInt   sMaxErrorTokenSize;
    SInt   sInQuotes;
    SInt   sBadorLog;
    SInt   sRowTermLen;
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    idBool sSkipReadData = ID_FALSE;
    eReadState sState;
    eReadState sPreState;                   // ������������

    sState    = stStart;
    sPreState = stError;
    /* BUG-24358 iloader Geometry Data */
    sMaxTokenSize = mTokenMaxSize;
    sMaxErrorTokenSize = mErrorTokenMaxSize;
    mTokenLen = 0;
    mErrorTokenLen = 0;
    sInQuotes = ILO_FALSE;

    sEnclosing = sHandle->mProgOption->mCSVEnclosing;
    sRowTerm   = sHandle->mProgOption->m_RowTerm;
    sFieldTerm = sHandle->mProgOption->mCSVFieldTerm;
    sBadorLog  = sHandle->mProgOption->m_bExist_bad || sHandle->mProgOption->m_bExist_log || ( sHandle->mLogCallback != NULL );

    sRowTermInd = 0;
    sRowTermLen = idlOS::strlen(sRowTerm);

    if ( m_SetNextToken == SQL_TRUE )
    {
        m_SetNextToken = SQL_FALSE;
        return mNextToken;
    }

    while ( sMaxTokenSize && sMaxErrorTokenSize )
    {
        /* BUG-29779: csv�� rowterm�� \r\n���� �����ϴ� ��� */
        if( sSkipReadData == ID_TRUE )
        {
            sSkipReadData = ID_FALSE;
        }
        else
        {
            if ( (sReadresult = ReadDataFromCBuff(sHandle,&sChr)) < 0 )
            {   // case of EOF
                break;
            }

            //PROJ-1714
            //Buffer���� ���� Byte��.. LOB Column�� Upload�� ��쿡 ����.
            mDataFileRead++;
            /* if bad or log option is set, save an entire token value */
            if ( sBadorLog )
            {
                mErrorToken[ mErrorTokenLen++ ] = (SChar)sChr;
                sMaxErrorTokenSize--;
            }
        }

        switch ( sState )
        {
            /* stStart   : state of starting up reading a field  */
            case stStart :
                if ( sChr != '\n' && isspace(sChr) )
                {
                    break;
                }
                else if ( sChr == sEnclosing )
                {
                    sState = stCollect;
                    sInQuotes = ILO_TRUE;
                    break;
                }
                sState = stCollect;
                sSkipReadData = ID_TRUE;
                break;
            /* BUG-29779: csv�� rowterm�� \r\n���� �����ϴ� ��� */
            // Rowterm�� string���� �ٲ�� rowterm�� �´����� ó���ϱ� ���� ���¸� �߰���.
            case stRowTerm:
                if ( sRowTermInd < sRowTermLen && sChr == sRowTerm[sRowTermInd] )
                {
                    sRowTermBK[sRowTermInd++] = sChr;
                    if( sRowTermInd == sRowTermLen )
                    {   // match up with rowterm
                        switch( sPreState )
                        {
                            case stCollect:
                                while ( mTokenLen && m_TokenValue[ mTokenLen - 1] == ' ' )
                                {
                                    mTokenLen--;
                                }
                                m_SetNextToken = SQL_TRUE;
                                mNextToken = TROW_TERM;
                                m_TokenValue[ mTokenLen ] = '\0';
                                if ( sBadorLog )
                                {
                                    mErrorToken[ --mErrorTokenLen ] = '\0';
                                }
                                return TVALUE;
                                break;
                            case stTailSpace:
                            case stEndQuote:
                                m_SetNextToken = SQL_TRUE;
                                mNextToken = TROW_TERM;
                                m_TokenValue[ mTokenLen ] = '\0';
                                if ( sBadorLog )
                                {
                                    mErrorToken[ --mErrorTokenLen ] = '\0';
                                }
                                return TVALUE;
                                break;
                            case stError:
                                if ( sBadorLog )
                                {
                                    mErrorToken[ --mErrorTokenLen ] = '\0';
                                }
                                m_SetNextToken = SQL_TRUE;
                                mNextToken = TROW_TERM;
                                IDE_RAISE( wrong_CSVformat_error );
                                break;
                            default:
                                break;
                        }
                        // ��� �Ʒ� �ζ����� Ż�� ����.
                        sState      = sPreState;
                        sRowTermInd = 0;
                    }
                }
                else
                {
                    IDE_TEST_RAISE( (sMaxTokenSize - sRowTermInd) <= 0,
                                    buffer_overflow_error);
                    idlOS::memcpy( (void*)(m_TokenValue + mTokenLen),
                                   (void*)sRowTermBK,
                                   sRowTermInd );
                    mTokenLen    += sRowTermInd;
                    sSkipReadData = ID_TRUE;
                    sState        = sPreState;
                    sRowTermInd   = 0;
                }
                break;
            /* state of in the middle of reading a field  */    
            case stCollect :
                if ( sInQuotes == ILO_TRUE )
                {
                    if ( sChr == sEnclosing )
                    {
                        sState = stEndQuote;
                        break;
                    }
                }
                else if ( sChr == sFieldTerm )
                {
                    while ( mTokenLen && m_TokenValue[ mTokenLen - 1] == ' ' )
                    {
                        mTokenLen--;
                    }
                    m_SetNextToken = SQL_TRUE;
                    mNextToken = TFIELD_TERM;
                    m_TokenValue[ mTokenLen ] = '\0';
                    if ( sBadorLog )
                    {
                        mErrorToken[ --mErrorTokenLen ] = '\0';
                    }
                    return TVALUE;
                }
                else if ( sChr == sRowTerm[sRowTermInd] )
                {
                    sPreState = sState;
                    sState    = stRowTerm;
                    sSkipReadData = ID_TRUE;
                    break;
                }
                else if ( sChr == sEnclosing )
                {
                    /* CSV format is wrong, so state must be changed to stError  */
                    sState = stError;
                    /* if buffer is sufficient, save the error character.
                     * this is just for showing error character on stdout. */
                    if( sMaxTokenSize > 1 )
                    {
                        m_TokenValue[ mTokenLen++] = sChr;
                    }
                    m_TokenValue[ mTokenLen ] = '\0';
                    break;
                }
                /* collect good(csv format) characters */
                m_TokenValue[ mTokenLen++ ] = sChr;
                sMaxTokenSize--;
                break;
            /* at tailing spaces out of quotes */    
            case stTailSpace :
            /* at tailing quote */
            case stEndQuote :
                /* In case of reading an escaped quote. */
                if ( sChr == sEnclosing && sState != stTailSpace )
                {
                    m_TokenValue[ mTokenLen++ ] = sChr;
                    sMaxTokenSize--;
                    sState = stCollect;
                    break;
                }
                else if ( sChr == sFieldTerm )
                {
                    m_SetNextToken = SQL_TRUE;
                    mNextToken = TFIELD_TERM;
                    m_TokenValue[ mTokenLen ] = '\0';
                    if ( sBadorLog )
                    {
                        mErrorToken[ --mErrorTokenLen ] = '\0';
                    }
                    return TVALUE;
                }
                else if ( sChr == sRowTerm[sRowTermInd] )
                {
                    sPreState = sState;
                    sState    = stRowTerm;
                    sSkipReadData = ID_TRUE;
                    break;
                }
                else if ( isspace(sChr) )
                {
                    sState = stTailSpace;
                    break;
                }

                if( sMaxTokenSize > 1 )
                {
                    m_TokenValue[ mTokenLen++] = sChr;
                }
                m_TokenValue[ mTokenLen ] = '\0';
                sState = stError;
                break;
            /* state of a wrong csv format is read */    
            case stError :
                if ( sChr == sFieldTerm )
                {
                    if ( sBadorLog )
                    {
                        mErrorToken[ --mErrorTokenLen ] = '\0';
                    }
                    m_SetNextToken = SQL_TRUE;
                    mNextToken = TFIELD_TERM;
                    IDE_RAISE( wrong_CSVformat_error );
                }
                else if ( sChr == sRowTerm[sRowTermInd] )
                {
                    sPreState = sState;
                    sState    = stRowTerm;
                    sSkipReadData = ID_TRUE;
                    break;
                }
                break;
            default:
                break;
        }
    }

    if ( (sMaxTokenSize == 0) || (sMaxErrorTokenSize == 0) )
    {
        IDE_RAISE( buffer_overflow_error );
    }

    if( sReadresult != 1 )
    {
        if( mTokenLen != 0 )
        {
            m_SetNextToken = SQL_TRUE;
            mNextToken = TEOF;
            m_TokenValue[ mTokenLen ] = '\0';
            if ( sBadorLog )
            {
                mErrorToken[ mErrorTokenLen ] = '\0';
            }
            return TVALUE;
        }
        else
        {
            return TEOF;
        }
    }

    IDE_RAISE( wrong_CSVformat_error );

    IDE_EXCEPTION( wrong_CSVformat_error );
    {
        // BUG-24898 iloader �Ľ̿��� ��ȭ
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Invalid_CSV_File_Format_Error,
                        aColName,
                        m_TokenValue);
    }
    IDE_EXCEPTION( buffer_overflow_error );
    {
        m_TokenValue[mTokenLen] = '\0';
        if ( sBadorLog )
        {
            // fix BUG-25539 [CodeSonar::TypeUnderrun]
            if (mErrorTokenLen > 0)
            {
                mErrorToken[ mErrorTokenLen - 1 ] = '\0';
                mErrorTokenLen--;
            }
        }
        return TVALUE;
    }
    IDE_EXCEPTION_END;

    return TERROR;
}


/**
 * GetLOBToken.
 *
 * ������ ���Ϸκ��� LOB �����͸� �б� ���� �Լ��̴�.
 * LOB�� ũ�Ⱑ ũ�Ƿ�
 * ������ ������ LOB �����͸� �޸� ���۷� �о���̴� ���� �ƴ϶�
 * LOB �������� ���� ��ġ, ���̸� ���Ͽ� �����Ѵ�.
 * �˰��� ��ü�� GetToken()�� �����ϴ�.
 * ���ϰ��� TVALUE�� ���� ��� ���ڵ��� �����ȴ�.
 *
 * @param[out] aLOBPhyOffs
 *  ������ ���ϳ����� LOB �������� ���� ��ġ.
 * @param[out] aLOBPhyLen
 *  ������ ���ϳ����� LOB �������� ����.
 *  "������"�� �ǹ̴� Windows �÷����� ���
 *  "\n"�� ���Ͽ��� "\r\n"���� ����Ǿ� 2����Ʈ�� ī��Ʈ���� �ǹ��Ѵ�.
 * @param[out] aLOBLen
 *  LOB �������� ����(������ ���� �ƴ�).
 *  "\n"�� 1����Ʈ�� ī��Ʈ�ȴ�.
 */
EDataToken iloDataFile::GetLOBToken( ALTIBASE_ILOADER_HANDLE   aHandle,
                                     ULong                    *aLOBPhyOffs,
                                     ULong                    *aLOBPhyLen,
                                     ULong                    *aLOBLen )
{
    SChar sChr;
    SInt  i;
    SInt  sRet;
    UInt  sEnCnt      = 0;
    UInt  sEnMatchLen = 0;
    UInt  sFTMatchLen = 0;
    UInt  sRTMatchLen = 0;
    SInt  sTmp;
    /* BUG-21064 : CLOB type CSV up/download error */
    /* BUG-30409 : CLOB type data�� upload�� csv ���亯ȯ�� �ǹٷ� ���� ����. */
    iloBool        sCSVnoLOBFile;
    // �ٷ����� "���ڰ� �Ծ������� �˷��ִ� ����.
    iloBool        sPreSCVEnclose;
    // ���� enclosing("...")������ �������� �˷��ִ� ����.
    iloBool        sCSVEnclose;
    iloaderHandle *sHandle;
    sHandle        = (iloaderHandle *) aHandle;
    sCSVEnclose    = ILO_FALSE;
    sPreSCVEnclose = ILO_FALSE;
    sCSVnoLOBFile  = (iloBool)(( sHandle->mProgOption->mRule == csv ) &&
                               ( sHandle->mProgOption->mUseLOBFile != ILO_TRUE ));

    (*aLOBPhyOffs) = mDataFileRead; //mDataFilePos;
    (*aLOBPhyLen)  = ID_ULONG(0);
    (*aLOBLen)     = ID_ULONG(0);

    IDE_TEST_RAISE(m_SetNextToken == SQL_TRUE, NextTokenExist);

    for(i=0 ; ; i++)
    {
        sRet = ReadDataFromCBuff( sHandle, &sChr);    //BUG-22434
        IDE_TEST_RAISE(sRet == -1, EOFReached);

        (*aLOBPhyLen)++;
        (*aLOBLen)++;

        /* BUG-30409 : CLOB type data�� upload�� csv ���亯ȯ�� �ǹٷ� ���� ����. */
        // CSV �÷��� ���۰� ���� �˾Ƴ����Ѵ�.
        // 1. ó���� " �� ���� �����̴�.
        // 2. "���� \n�� �÷�data�� ��, row term�� �ƴϴ�.
        // 3. "�ȿ� "�� ���� ������ � ���ڰ� �����Ŀ� ���� �÷��� �������� �����ȴ�.
        //    - "�ȿ� "�� �µ� �ٽ� "������ �÷� data�� ���̴�.
        //    - "�ȿ� "�� �µ� "�� ������ �ٸ����ڰ����� �÷��� ���̴�.
        if( sCSVnoLOBFile == ILO_TRUE )
        {
            if( sChr == sHandle->mProgOption->mCSVEnclosing )
            {
                if( i == 0 )
                {   // ó���� " �� ���� �÷��� �����̴�.
                    sCSVEnclose = ILO_TRUE;
                    continue;
                }
                else
                {
                    // enclosing�Ǿ����� ������ "�� �Դ�.
                    IDE_TEST_RAISE( sCSVEnclose != ILO_TRUE, WrongCSVFormat);

                    if( sPreSCVEnclose == ILO_TRUE )
                    {
                        // "�ȿ� "�� �µ� �ٽ� "������ �÷� data�� ���̴�.
                        sPreSCVEnclose = ILO_FALSE;
                        continue;
                    }
                    else
                    {
                        // "�ȿ� "�� �Դ�
                        sPreSCVEnclose = ILO_TRUE;
                        continue;
                    }
                }
            }
            else
            {
                if( sPreSCVEnclose == ILO_TRUE )
                {
                    // "�ȿ� "�� �µ� "�� ������ �ٸ����ڰ����� �÷��� ���̴�.
                    sCSVEnclose = ILO_FALSE;
                }
                else
                {
                    sPreSCVEnclose = ILO_FALSE;
                }
            }
        }

        //BUG-22513
        sTmp = (UChar)sChr;

        if (sEnCnt == 0)
        {
            if ( sCSVEnclose == ILO_FALSE )
            {
                sFTMatchLen = mFTLexStateTransTbl[sFTMatchLen][sTmp];
                if (sFTMatchLen == (UInt)m_nFTLen )
                {
                    mDataFileRead += (*aLOBPhyLen);
                    if ((*aLOBLen) == (ULong)m_nFTLen &&
                        m_SetEnclosing)
                    {
                        return TFIELD_TERM;
                    }
                    else
                    {
                        m_SetNextToken = SQL_TRUE;
                        mNextToken = TFIELD_TERM;
                        (*aLOBPhyLen) -= (ULong)mFTPhyLen;
                        (*aLOBLen) -= (ULong)m_nFTLen;
                        return TVALUE;
                    }
                }

                sRTMatchLen = mRTLexStateTransTbl[sRTMatchLen][sTmp];
                if (sRTMatchLen == (UInt)m_nRTLen)
                {
                    mDataFileRead += (*aLOBPhyLen);
                    if ((*aLOBLen) == (ULong)m_nRTLen &&
                          (m_SetEnclosing || sHandle->mProgOption->mInformix))
                    {
                        return TROW_TERM;
                    }
                    else
                    {
                        m_SetNextToken = SQL_TRUE;
                        mNextToken = TROW_TERM;
                        (*aLOBPhyLen) -= (ULong)mRTPhyLen;
                        (*aLOBLen) -= (ULong)m_nRTLen;
                        return TVALUE;
                    }
                }
            }
        }

        // csv �����ϰ�� �Ʒ� �κ��� ���õȴ�.
        if (m_SetEnclosing &&
            (sEnCnt == 1 || (*aLOBLen) == (ULong)sEnMatchLen + 1))
        {
            sEnMatchLen = mEnLexStateTransTbl[sEnMatchLen][sTmp];
            if (sEnMatchLen == (UInt)m_nEnLen)
            {
                mDataFileRead += (*aLOBPhyLen);
                if (sEnCnt == 0)
                {
                    sEnCnt++;
                    sEnMatchLen = 0;
                    (*aLOBPhyOffs) = mDataFileRead;
                    (*aLOBPhyLen) = ID_ULONG(0);
                    (*aLOBLen) = ID_ULONG(0);
                }
                else
                {
                    (*aLOBPhyLen) -= (ULong)mEnPhyLen;
                    (*aLOBLen) -= (ULong)m_nEnLen;
                    return TVALUE;
                }
            }
        }
    }

    return TVALUE;

    IDE_EXCEPTION(NextTokenExist);
    {
        m_SetNextToken = SQL_FALSE;
        return mNextToken;
    }
    IDE_EXCEPTION(EOFReached);
    {
        if ((*aLOBPhyLen) > ID_ULONG(0))
        {
            mDataFileRead += (*aLOBPhyLen);
            m_SetNextToken = SQL_TRUE;
            mNextToken = TEOF;
            return TVALUE;
        }
        else
        {
            return TEOF;
        }
    }
    IDE_EXCEPTION(WrongCSVFormat);
    {
        // csv format�� �ƴϴ�.
        // �ǹٷ� data�� �Էµ��� ���� �� �ִ�.
        return TERROR;
    }
    IDE_EXCEPTION_END;

    return TEOF;
}

void iloDataFile::rtrim()
{
    SInt   cnt = 0;
    SInt   len = 0;
    SChar *ptr;
    len = idlOS::strlen(m_TokenValue);
    ptr =  m_TokenValue + len;
    while (cnt < len)
    {
        if (*ptr != ' ') break;
        ptr--;
        cnt++;
    }
    if (cnt < len)
    {
        *ptr = '\0';
    }
}

/*
 *   Return Value
 *   READ_ERROR         0
 *   READ_SUCCESS       1
 *   END_OF_FILE        2
 */
 //PROJ-1714
SInt iloDataFile::ReadOneRecordFromCBuff( ALTIBASE_ILOADER_HANDLE  aHandle,
                                          iloTableInfo            *aTableInfo,
                                          SInt                     aArrayCount )
{

    EDataToken eToken;
    SInt sReadErrCol = -1; // BUG-46442
    SInt i = 0;
    SInt j = 0;
    SInt sMsSql;
    SInt sRC = READ_SUCCESS;
    IDE_RC sRC_loadFromOutFile;   //PROJ-2030, CT_CASE-3020 CHAR outfile ����
    IDE_RC sRC_SetAttrValue;
    SChar sColName[MAX_OBJNAME_LEN];
    
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    sMsSql             = sHandle->mProgOption->mMsSql;
    mLOBFileColumnNum  = 0;           //BUG-24583 Init 
    
    if ( i < aTableInfo->GetAttrCount() &&
         aTableInfo->GetAttrType(i) == ISP_ATTR_TIMESTAMP &&
         sHandle->mParser.mAddFlag == ILO_TRUE )
    {
        i++;
    }
    
    /* BUG - 18804 */
    if( sHandle->mProgOption->m_bExist_bad || sHandle->mProgOption->m_bExist_log || ( sHandle->mLogCallback != NULL))
    {
        for (; j < aTableInfo->GetAttrCount(); j++)
        {
            aTableInfo->mAttrFail[ j ][ 0 ] = '\0';
            aTableInfo->mAttrFailLen[j] = 0;
        }
    }
    
    if (i < aTableInfo->GetAttrCount())
    {
        /* �÷� �� ����. */
        if ( ((aTableInfo->GetAttrType(i) != ISP_ATTR_CLOB) &&
              (aTableInfo->GetAttrType(i) != ISP_ATTR_BLOB))
            ||
             (mUseLOBFileOpt == ILO_TRUE) )
        {
            /* TASK-2657 */
            if( sHandle->mProgOption->mRule == csv )
            {
                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
            }
            else
            {
                eToken = GetTokenFromCBuff(sHandle);
            }
        }
        else
        {
            eToken = GetLOBToken( sHandle,
                                  &aTableInfo->mLOBPhyOffs[i][aArrayCount],
                                  &aTableInfo->mLOBPhyLen[i][aArrayCount],
                                  &aTableInfo->mLOBLen[i][aArrayCount] );
        }
    }
    /* �÷� skip������ �� �̻� �о���� �÷��� ���� ��� */
    else /* (i == pTableInfo->GetAttrCount()) */
    {
        /* ������ ������ �̹� �࿡�� ���� �����͸� �� �����ڰ� ���� ������ ���� */
        do
        {
            /* TASK-2657 */
            if( sHandle->mProgOption->mRule == csv )
            {
                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
            }
            else
            {
                eToken = GetTokenFromCBuff(sHandle);
            }
        } 
        while (eToken != TEOF && eToken != TROW_TERM);

        if (eToken == TEOF)
        {
            aTableInfo->SetReadCount(i, aArrayCount);

            sRC = END_OF_FILE;
            IDE_CONT(RETURN_CODE);
        }
    }
    
    
    for ( ; i < aTableInfo->GetAttrCount(); i++)
    {
        if (eToken == TEOF)
        {
            aTableInfo->SetReadCount(i, aArrayCount);
            if (i == 0 ||
                (i == 1 && sHandle->mParser.mAddFlag == ILO_TRUE &&
                aTableInfo->GetAttrType(0) == ISP_ATTR_TIMESTAMP))
            {
                sRC = END_OF_FILE;
                IDE_CONT(RETURN_CODE);
            }
            else
            {
                SET_READ_ERROR;
                IDE_CONT(RETURN_CODE);
            }
        }
        else if (eToken == TVALUE )
        {
            if ( (aTableInfo->GetAttrType(i) != ISP_ATTR_CLOB) &&
                 (aTableInfo->GetAttrType(i) != ISP_ATTR_BLOB) )
            {
                
                //PROJ-2030, CT_CASE-3020 CHAR outfile ����
                //loadFromOutFile() �� �����ϴ� ��쿡�� SetAttrValue() �� �����Ѵ�.
                //loadFromOutFile() �Ǵ� SetAttrValue�� �����ϴ� ���� ����ī��Ʈ�ϰ�, 
                //errȭ�Ͽ� ����Ѵ�.
                sRC_loadFromOutFile = IDE_SUCCESS;
                sRC_SetAttrValue = IDE_SUCCESS;

                if ( (mTokenLen > 0) && (aTableInfo->mOutFileFlag[i] == ILO_TRUE) )
                {
                    sRC_loadFromOutFile = loadFromOutFile(sHandle);
                }
                
                if (sRC_loadFromOutFile == IDE_SUCCESS)
                {
                    /* �� �����ص�. */
                    sRC_SetAttrValue = aTableInfo->SetAttrValue( sHandle,
                                                                 i,
                                                                 aArrayCount,
                                                                 m_TokenValue,
                                                                 mTokenLen,
                                                                 sMsSql,
                                                                 sHandle->mProgOption->mNCharUTF16);
                }
            
                if ( (sRC_loadFromOutFile != IDE_SUCCESS) || (sRC_SetAttrValue != IDE_SUCCESS) )
                {
                    /* BUG - 18804 */
                    /* bad or log option�� ��� ��������� fail�� �ʵ尪�� mAttrFail[ i ]�� ������ */
                    if( sHandle->mProgOption->m_bExist_bad || 
                            sHandle->mProgOption->m_bExist_log || (sHandle->mLogCallback != NULL))
                    {
                        if( sHandle->mProgOption->mRule == csv )
                        {
                            // TASK-2657
                            IDE_TEST(aTableInfo->SetAttrFail( sHandle,
                                                              i,
                                                              mErrorToken,
                                                              mErrorTokenLen)
                                                              != IDE_SUCCESS);
                        }
                        // proj1778 nchar
                        // utf16 nchar dataó���� ���ؼ�
                        // no csv������, sprintf�� �����ϰ� memcpy�ϵ��� ����
                        // ref) no csv�� ��� mErrorToken �� ������� ����
                        else
                        {
                            IDE_TEST(aTableInfo->SetAttrFail( sHandle,
                                                              i,
                                                              m_TokenValue,
                                                              mTokenLen)
                                                              != IDE_SUCCESS);
                        }
                    }
                    SET_READ_ERROR;
                }
            }

            else if (mUseLOBFileOpt == ILO_TRUE)
            {
                /* LOB indicator�� �м��Ͽ� �����°� ���̸� ����. */
                if (AnalLOBIndicator(&aTableInfo->mLOBPhyOffs[i][aArrayCount],
                                     &aTableInfo->mLOBPhyLen[i][aArrayCount])
                    != IDE_SUCCESS)
                {
                    SET_READ_ERROR;
                }
            }
            else
            {
                /* �÷� �ִ� ���� �Ѿ���� �˻�. */
                if (aTableInfo->GetAttrType(i) == ISP_ATTR_CLOB)
                {
                    if (aTableInfo->mLOBLen[i][aArrayCount]
                        > ID_ULONG(0xFFFFFFFF))
                    {
                        // BUG-24823 iloader ���� ���϶����� �����޽����� ����ϰ� �־ diff �� �߻��մϴ�.
                        // ���ϸ�� ���μ��� ����ϴ� �κ��� �����մϴ�.
                        // BUG-24898 iloader �Ľ̿��� ��ȭ
                        uteSetErrorCode( sHandle->mErrorMgr,
                                        utERR_ABORT_Token_Value_Range_Error,
                                        0,
                                        aTableInfo->GetTransAttrName(i,sColName,(UInt)MAX_OBJNAME_LEN),
                                        "[CLOB]");
                        aTableInfo->SetReadCount(i, aArrayCount);
                        sRC = READ_ERROR;
                    }
                }
                else /* (pTableInfo->GetAttrType(i) == ISP_ATTR_BLOB) */
                {
                    if (aTableInfo->mLOBLen[i][aArrayCount]
                        > ID_ULONG(0x1FFFFFFFE))
                    {
                        // BUG-24823 iloader ���� ���϶����� �����޽����� ����ϰ� �־ diff �� �߻��մϴ�.
                        // ���ϸ�� ���μ��� ����ϴ� �κ��� �����մϴ�.
                        // BUG-24898 iloader �Ľ̿��� ��ȭ
                        uteSetErrorCode( sHandle->mErrorMgr,
                                        utERR_ABORT_Token_Value_Range_Error,
                                        0,
                                        aTableInfo->GetTransAttrName(i,sColName,(UInt)MAX_OBJNAME_LEN),
                                        "[BLOB]");
                        aTableInfo->SetReadCount(i, aArrayCount);
                        sRC = READ_ERROR;
                    }
                }
            }

            /* �÷� skip */
            if ( i+1 < aTableInfo->GetAttrCount() &&
                 aTableInfo->GetAttrType(i+1) == ISP_ATTR_TIMESTAMP &&
                 sHandle->mParser.mAddFlag == ILO_TRUE )
            {
                i++;
            }
            /* TASK-2657 */
            if( sHandle->mProgOption->mRule == csv )
            {
                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
            }
            else
            {
                /* ���� �̾����� �ʵ� ������, �� �����ڸ� ����. */
                eToken = GetTokenFromCBuff(sHandle);
            }

        }    
        /* TASK-2657 */
        /* this part for sticking the CSV module on previous messy code :( */
        else if ( eToken == TERROR )
        {
            if( sHandle->mProgOption->m_bExist_bad ||
                    sHandle->mProgOption->m_bExist_log || (sHandle->mLogCallback != NULL) )
            {
                IDE_TEST(aTableInfo->SetAttrFail( sHandle,
                                                  i, 
                                                  mErrorToken, 
                                                  mErrorTokenLen)
                                                  != IDE_SUCCESS);
            }
            SET_READ_ERROR;
            if( sHandle->mProgOption->mRule == csv )
            {
                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
                if( eToken == TEOF )
                {
                    aTableInfo->SetReadCount(i+1, aArrayCount);

                    SET_READ_ERROR;
                    IDE_CONT(RETURN_CODE);
                }
            }
            else
            {
                eToken = GetTokenFromCBuff(sHandle);
            }
        }
        else /* !TEOF && !TVALUE */
        {
            aTableInfo->SetReadCount(i, aArrayCount);

            SET_READ_ERROR;
            IDE_CONT(RETURN_CODE);
        }

        if ((i == aTableInfo->GetAttrCount()-1) && (eToken == TEOF))
        {
            aTableInfo->SetReadCount(i + 1, aArrayCount);

            IDE_CONT(RETURN_CODE);
        }

        if (sHandle->mProgOption->mInformix == SQL_TRUE &&
            (i == aTableInfo->GetAttrCount()-1) && (eToken == TFIELD_TERM))
        {
            /* TASK-2657 */
            if( sHandle->mProgOption->mRule == csv )
            {
                /* BUG-47663 Parse error CSV file with -informix option 
                   Consume TFIELD_TERM token that is useless field_term after last column */
                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));

                eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
            }
            else
            {
                eToken = GetTokenFromCBuff(sHandle);
            }
            if (eToken == TEOF)
            {
                aTableInfo->SetReadCount(i + 1, aArrayCount);

                IDE_CONT(RETURN_CODE);
            }
            else if (eToken != TROW_TERM)
            {
                aTableInfo->SetReadCount(i, aArrayCount);

                SET_READ_ERROR;
                IDE_CONT(RETURN_CODE);
            }
        }
        else if ((i == aTableInfo->GetAttrCount()-1) && (eToken != TROW_TERM))
        {
            /* TASK-2657 */
            /* if we read columns more than what we expected */
            if( sHandle->mProgOption->mRule == csv && eToken == TFIELD_TERM )
            {
                aTableInfo->SetReadCount(i + 1, aArrayCount);
                do
                {
                    eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
                }
                while ( eToken != TROW_TERM && eToken != TEOF);
                if ( eToken == TEOF )
                {
                    sRC = END_OF_FILE;
                    IDE_CONT(RETURN_CODE);
                }
            }
            else
            {
                aTableInfo->SetReadCount(i, aArrayCount);
            }

            SET_READ_ERROR;
            IDE_CONT(RETURN_CODE);
        }
        else if ( (i < aTableInfo->GetAttrCount()-1) &&
                  (eToken != TFIELD_TERM) )
        {
            //BUG-28584
            aTableInfo->SetReadCount( i + 1, aArrayCount);

            SET_READ_ERROR;
            IDE_CONT(RETURN_CODE);
        }

        if (i < aTableInfo->GetAttrCount()-1)
        {
            /* �÷� �� ����. */
            if ((aTableInfo->GetAttrType(i + 1) != ISP_ATTR_CLOB &&
                 aTableInfo->GetAttrType(i + 1) != ISP_ATTR_BLOB)
                ||
                mUseLOBFileOpt == ILO_TRUE)
            {
                /* TASK-2657 */
                if( sHandle->mProgOption->mRule == csv )
                {
                    eToken = GetCSVTokenFromCBuff( sHandle, aTableInfo->GetAttrName(i));
                }
                else
                {
                    eToken = GetTokenFromCBuff(sHandle);
                }
            }
            else
            {
                eToken = GetLOBToken( sHandle,
                                      &aTableInfo->mLOBPhyOffs[i + 1][aArrayCount],
                                      &aTableInfo->mLOBPhyLen[i + 1][aArrayCount],
                                      &aTableInfo->mLOBLen[i + 1][aArrayCount] );
            }
        }
    }

    aTableInfo->SetReadCount(i, aArrayCount);
    mLOBFileColumnNum = 0;          //BUG-24583 : �������� ���� LOB File�� Open �ϱ����� �ʱ�ȭ �Ѵ�.

    // BUG-24898 iloader �Ľ̿��� ��ȭ
    // ��� return �� ����� ������.
    // �����ڵ尡 �����Ǿ� ���� ������ ����Ÿ �Ľ̿����� ����Ѵ�.
    IDE_EXCEPTION_CONT(RETURN_CODE);

    if(uteGetErrorCODE(sHandle->mErrorMgr) == 0)
    {
        if(sRC == READ_ERROR)
        {
            if(sReadErrCol >= 0 && sReadErrCol < aTableInfo->GetAttrCount())
            {
                uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Parsing_Error,
                                aTableInfo->GetTransAttrName(sReadErrCol,sColName,(UInt)MAX_OBJNAME_LEN)
                               );
            }
        }
        else if ( sHandle->mThreadErrExist == ILO_TRUE )
        {
            uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_THREAD_Error);
        }
    }

    return sRC;

    IDE_EXCEPTION_END;

    return SYS_ERROR;
}


SInt iloDataFile::strtonumCheck(SChar *p)
{
    SChar c;
    while((c = *p) == ' ' || (c = *p) == '\t' )
    {
        p++;
    }

    if (c == '-' || c == '+')
    {
        p++;
    }

    while ((c = *p++), isdigit(c))
        ;

    if (c=='.')
    {
        while ((c = *p++), isdigit(c)) ;
    }
    else if (c=='\0')
    {
        return 1;
    }
    else
    {
        return 0;
    }

    if ((c == 'E') || (c == 'e'))
    {
        if ((c= *p++) == '+')
            ;
        else if (c=='-')
           ;
        else
           --p;
        while ((c = *p++), isdigit(c))
          ;
    }

    if ( c == '\0' )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * AnalDataFileName.
 *
 * ������ ���ϸ��� �м��Ͽ�, ���ϸ� ��ü, ������ ���� Ȯ����,
 * ������ ���� ��ȣ�� ��´�.
 * �м� ����� ������� mFileNameBody, mDataFileNameExt,
 * mDataFileNo�� ����ȴ�.
 *
 * @param[in] aDataFileName
 *  ������ ���ϸ�.
 * @param[in] aIsWr
 *  ������ ���� ����(ILO_TRUE)���� �б�(ILO_FALSE)���� ����.
 */
void iloDataFile::AnalDataFileName(const SChar *aDataFileName, iloBool aIsWr)
{
    SChar  sBk = 0;
    SChar *sFileNoPos;
    SChar *sPeriodPos;
    SInt   sI;

    (void)idlOS::snprintf(mFileNameBody, ID_SIZEOF(mFileNameBody), "%s",
                          aDataFileName);
    /* ���� ��ȣ�� -1�� �ʱ�ȭ�Ѵ�. */
    mDataFileNo = -1;

    /* ���ϸ� �޺κ��� ���ʿ��� ������ �����Ѵ�. */
    for (sI = idlOS::strlen(mFileNameBody) - 1; sI >= 0; sI--)
    {
        if (mFileNameBody[sI] != ' ' && mFileNameBody[sI] != '\f' &&
            mFileNameBody[sI] != '\n' && mFileNameBody[sI] != '\r' &&
            mFileNameBody[sI] != '\t' && mFileNameBody[sI] != '\v')
        {
            break;
        }
    }
    mFileNameBody[sI + 1] = '\0';

    /* ������ ���� �б��� ���,
     * ����ڰ� �Է��� ������ ���ϸ� ���� ��ȣ�� ������ �� �ִ�. */
    if (aIsWr != ILO_TRUE)
    {
        /* ���� ��ȣ�� ���� ��ġ�� ���Ѵ�. */
        for (; sI >= 0; sI--)
        {
            if (mFileNameBody[sI] < '0' || mFileNameBody[sI] > '9')
            {
                break;
            }
        }
        if (mFileNameBody[sI + 1] != '\0')
        {
            sFileNoPos = &mFileNameBody[sI + 1];
        }
        else
        {
            sFileNoPos = NULL;
        }
    }
    /* ������ ���� ������ ���,
     * ����ڰ� �Է��� ������ ���ϸ��� ���� ��ȣ�� ���� ������ ����. */
    else /* (aIsWr == ILO_TRUE) */
    {
        sFileNoPos = NULL;
    }

    /* Ȯ������ ���� ��ġ�� ���Ѵ�. */
    sPeriodPos = idlOS::strrchr(mFileNameBody, '.');
    if (sPeriodPos == mFileNameBody)
    {
        sPeriodPos = NULL;
    }

    /* Ȯ���ڰ� �����ϴ� ��� */
    if (sPeriodPos != NULL)
    {
        /* ���� ��ȣ�� ������ ������ Ȯ���ڸ� �˻��ϱ� ����,
         * �ӽ÷� ���� ��ȣ �κ��� NULL�� �����. */
        if (sFileNoPos != NULL)
        {
            sBk = *sFileNoPos;
            *sFileNoPos = '\0';
        }
        /* Ȯ���ڰ� ".dat"�� ��� */
        if (idlOS::strcasecmp(sPeriodPos, ".dat") == 0)
        {
            /* Ȯ���ڴ� ���� ��ȣ�� ������ �κ�(".dat")���� �Ѵ�. */
            (void)idlOS::snprintf(mDataFileNameExt,
                                  ID_SIZEOF(mDataFileNameExt), "%s",
                                  sPeriodPos);
            /* ���� ��ȣ�� �����ϴ� ��� */
            if (sFileNoPos != NULL)
            {
                /* ���� ��ȣ�� ��´�. */
                *sFileNoPos = sBk;
                mDataFileNo = (SInt)idlOS::strtol(sFileNoPos, (SChar **)NULL,
                                                  10);
            }
        }
        /* Ȯ���ڰ� ".dat"�� �ƴ� ��� */
        else
        {
            /* Ȯ���ڴ� ���ϸ� �޺κ��� ���� �κб��� �����ϵ��� �Ѵ�. */
            if (sFileNoPos != NULL)
            {
                *sFileNoPos = sBk;
            }
            (void)idlOS::snprintf(mDataFileNameExt,
                                  ID_SIZEOF(mDataFileNameExt), "%s",
                                  sPeriodPos);
        }
        /* ���ϸ� ��ü�� Ȯ���� �� ���� ��ȣ�� ������ �κ����� �Ѵ�. */
        *sPeriodPos = '\0';
    }
    /* Ȯ���ڰ� �������� �ʴ� ��� */
    else /* (sPeriodPos == NULL) */
    {
        mDataFileNameExt[0] = '\0';

        /* ���ϸ� ��ü�� ���ϸ� �޺κ��� ���� �κб��� �����ϰ� �ȴ�. */
    }
}

/**
 * GetStrPhyLen.
 *
 * ���ڷ� ���� ���ڿ��� �ؽ�Ʈ ���Ͽ� ����� ��� ������ �� ���̸� ���Ѵ�.
 *
 * @param[in] aStr
 *  ���̸� ���� ���ڿ�.
 *  NULL�� ����Ǿ�� �Ѵ�.
 */
UInt iloDataFile::GetStrPhyLen(const SChar *aStr)
{
    UInt         sPhyLen;
#if defined(VC_WIN32) || defined(VC_WIN64) || defined(VC_WINCE)
    const SChar *sCh;

    for (sCh = aStr, sPhyLen = 0; *sCh != '\0'; sCh++, sPhyLen++)
    {
        if (*sCh == '\n')
        {
            sPhyLen++;
        }
    }
#else
    sPhyLen = (UInt)idlOS::strlen(aStr);
#endif

    return sPhyLen;
}

/**
 * MakeAllLexStateTransTbl.
 *
 * �ʵ� ������, �� ������, �ʵ� encloser ��Ī�� ����
 * ��������ǥ�� �ۼ��Ѵ�.
 */
IDE_RC iloDataFile::MakeAllLexStateTransTbl( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    IDE_TEST(MakeLexStateTransTbl( sHandle, m_FieldTerm, &mFTLexStateTransTbl)
             != IDE_SUCCESS);
    IDE_TEST(MakeLexStateTransTbl( sHandle, m_RowTerm, &mRTLexStateTransTbl)
             != IDE_SUCCESS);
    if (m_SetEnclosing)
    {
        IDE_TEST(MakeLexStateTransTbl( sHandle, m_Enclosing, &mEnLexStateTransTbl)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * FreeAllLexStateTransTbl.
 *
 * �ʵ� ������, �� ������, �ʵ� encloser ��Ī�� ����
 * ��������ǥ�� �޸� �����Ѵ�.
 */
void iloDataFile::FreeAllLexStateTransTbl()
{
    if (mFTLexStateTransTbl != NULL)
    {
        FreeLexStateTransTbl(mFTLexStateTransTbl);
        mFTLexStateTransTbl = NULL;
    }
    if (mRTLexStateTransTbl != NULL)
    {
        FreeLexStateTransTbl(mRTLexStateTransTbl);
        mRTLexStateTransTbl = NULL;
    }
    if (mEnLexStateTransTbl != NULL)
    {
        FreeLexStateTransTbl(mEnLexStateTransTbl);
        mEnLexStateTransTbl = NULL;
    }
}

/**
 * MakeLexStateTransTbl.
 *
 * ������ ��Ī�� ��������ǥ�� �ۼ��Ѵ�.
 * ��������ǥ�� 8��Ʈ integer�� 2���� �迭�̴�.
 * �迭�� ũ��� (aStr ���ڿ� ����)x(256)�̴�.
 * (��Ȯ���� ��������ǥ �޸� ���� ���� ���Ǹ� ����
 * aStr ���ڿ� ���̺��� 1 ũ�� �Ҵ��Ѵ�.)
 * �迭�� 1�� �ε����� aStr ���ڿ��� ���ηκ���
 * �� ���� ���ڰ� ��Ī�� �����ΰ��� ��Ÿ����.
 * �迭�� 2�� �ε����� ������ �Է¹��� ���ڸ� ��Ÿ����.
 * �迭�� ���� ���� �� 1, 2�� �ε����� ������ ��
 * aStr ���ڿ��� ���ηκ��� �� ���� ���ڰ� ��Ī�� ���·� �Ǵ°��̴�.
 * ���� ���, (*aTbl)[1]['^']�� ����
 * aStr ���ڿ��� ���� 1���ڰ� �̹� ��Ī�Ǿ��ִ� ��Ȳ����
 * �̹��� ������ '^' ���ڸ� �Է¹��� ���
 * aStr ���ڿ��� ���ηκ��� �� ���� ���ڰ� ��Ī�� ���·� �Ǵ°��� ��Ÿ����.
 * ����, aStr�� "^^"���, (*aTbl)[1]['^']=2�� �ȴ�.
 *
 * @param[in] aStr
 *  ������ ��Ī�� ��������ǥ�� �ۼ��� ������.
 *  NULL�� ����Ǿ�� �Ѵ�.
 * @param[out] aTbl
 *  ������ ��Ī�� ��������ǥ.
 */
IDE_RC iloDataFile::MakeLexStateTransTbl( ALTIBASE_ILOADER_HANDLE  aHandle,
                                          SChar                  *aStr, 
                                          UChar                ***aTbl)
{
    iloBool  *sBfSubstLeftSubstMatch = NULL;
    UChar  **sTbl = NULL;
    UInt     sI, sJ, sK;
    UInt     sStrLen;

    // BUG-35099: [ux] Codesonar warning UX part -248480.2579709
    UInt     sState4Tbl   = 0;
    UInt     sState4Match = 0;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    sStrLen = (UInt)idlOS::strlen(aStr);

    /* �Լ� ���������� ����ϴ� ���� �޸� �Ҵ�. */
    if (sStrLen > 1)
    {
        sBfSubstLeftSubstMatch = (iloBool *)idlOS::malloc(
                                            (sStrLen - 1) * ID_SIZEOF(iloBool));
        IDE_TEST(sBfSubstLeftSubstMatch == NULL);
        sState4Match = 1;
    }

    /* ��������ǥ �޸� �Ҵ�. */
    sTbl = (UChar **)idlOS::malloc((sStrLen + 1) * ID_SIZEOF(UChar *));
    IDE_TEST(sTbl == NULL);
    sState4Tbl = 1;

    idlOS::memset(sTbl, 0, (sStrLen + 1) * ID_SIZEOF(UChar *));

    for (sI = 0; sI < sStrLen; sI++)
    {
        sTbl[sI] = (UChar *)idlOS::malloc(256 * ID_SIZEOF(UChar));
        IDE_TEST(sTbl[sI] == NULL);
        sState4Tbl = 2;
    }

    /* ��������ǥ �ۼ�. */
    for (sI = 0; sI < sStrLen; sI++)
    {
        for (sK = sI; sK > 0; sK--)
        {
            if (sK == 1 ||
                idlOS::strncmp(&aStr[sI + 1 - sK], aStr, sK - 1) == 0)
            {
                sBfSubstLeftSubstMatch[sK - 1] = ILO_TRUE;
            }
            else
            {
                sBfSubstLeftSubstMatch[sK - 1] = ILO_FALSE;
            }
        }

        for (sJ = 0; sJ < 256; sJ++)
        {
            if ((UInt)aStr[sI] == sJ)
            {
                sTbl[sI][sJ] = (UChar)(sI + 1);
            }
            else
            {
                for (sK = sI; sK > 0; sK--)
                {
                    if (sBfSubstLeftSubstMatch[sK - 1] == ILO_TRUE &&
                        (UInt)aStr[sK - 1] == sJ)
                    {
                        break;
                    }
                }
                sTbl[sI][sJ] = (UChar)sK;
            }
        }
    }

    if( sState4Match == 1 )
    {
        idlOS::free(sBfSubstLeftSubstMatch);
    }

    /* ����. */
    if( sState4Tbl == 2 )
    {
        FreeLexStateTransTbl(*aTbl);
    }

    (*aTbl) = sTbl;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        utePrintfErrorCode(stdout, sHandle->mErrorMgr);
    }

    switch( sState4Tbl )
    {
    case 2:
        for (sI = 0; sI < sStrLen; sI++)
        {
            if(sTbl[sI] != NULL)
            {
                (void)idlOS::free( sTbl[sI] );
            }
        }
        /* fall through */

    case 1:
        (void)idlOS::free( sTbl );
        break;

    default:
        break;
    }

    switch( sState4Match )
    {
    case 1:
        (void)idlOS::free( sBfSubstLeftSubstMatch );
    default:
        break;
    }

    return IDE_FAILURE;
}

/**
 * FreeLexStateTransTbl.
 *
 * ������ ��Ī�� ��������ǥ�� �޸� �����Ѵ�.
 *
 * @param[in] aTbl
 *  ������ ��Ī�� ��������ǥ.
 */
void iloDataFile::FreeLexStateTransTbl(UChar **aTbl)
{
    UInt     sI;

    if (aTbl != NULL)
    {
        for (sI = 0; aTbl[sI] != NULL; sI++)
        {
            idlOS::free(aTbl[sI]);
        }
        idlOS::free(aTbl);
    }
}

/**
 * SetLOBOptions.
 *
 * ����ڷκ��� �Է¹��� LOB ���� �ɼ��� iloDataFile ��ü�� �����Ѵ�.
 *
 * @param[in] aUseLOBFile
 *  use_lob_file �ɼ�.
 * @param[in] aLOBFileSize
 *  lob_file_size �ɼ�. ����Ʈ ����.
 * @param[in] aUseSeparateFiles
 *  use_separate_files �ɼ�.
 * @param[in] aLOBIndicator
 *  lob_indicator �ɼ�.
 *  %�� ���۵Ǵ� �̽������� ����(��:%n)�� '\n'�� ���� ��ȯ�� ������.
 */
void iloDataFile::SetLOBOptions(iloBool aUseLOBFile, ULong aLOBFileSize,
                                iloBool aUseSeparateFiles,
                                const SChar *aLOBIndicator)
{
    mUseLOBFileOpt = aUseLOBFile;
    mLOBFileSizeOpt = aLOBFileSize;
    mUseSeparateFilesOpt = aUseSeparateFiles;
    (void)idlOS::snprintf(mLOBIndicatorOpt, ID_SIZEOF(mLOBIndicatorOpt), "%s",
                          aLOBIndicator);
    mLOBIndicatorOptLen = (UInt)idlOS::strlen(mLOBIndicatorOpt);
}

/**
 * InitLOBProc.
 *
 * LOB ó���� �ʱ�ȭ�Ѵ�.
 * use_lob_file=yes, use_separate_files=no�� ���
 * 1�� LOB ������ ���� �۾��� �����Ѵ�.
 *
 * @param[in] aIsWr
 *  LOB ���Ͽ� ����(ILO_TRUE)���� �б�(ILO_FALSE)���� ����.
 */
IDE_RC iloDataFile::InitLOBProc( ALTIBASE_ILOADER_HANDLE aHandle, iloBool aIsWr)
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    /* LOB wrapper ��ü �޸� �Ҵ� */
    if (mLOB != NULL)
    {
        delete mLOB;
    }
    mLOB = new iloLOB;
    IDE_TEST_RAISE(mLOB == NULL, MAllocError);

    mLOBFileNo = 0;
    mLOBFilePos = ID_ULONG(0);
    mAccumLOBFilePos = ID_ULONG(0);

    if (aIsWr == ILO_TRUE)   /* Download ... */
    {
        if (mUseLOBFileOpt == ILO_TRUE && mUseSeparateFilesOpt == ILO_FALSE)
        {
            /* 1�� LOB ���� ���� */
            IDE_TEST_RAISE(OpenLOBFile(sHandle, ILO_TRUE, 1, 0, ILO_TRUE)
                           != IDE_SUCCESS, OpenLOBFileError);
        }
    }
    else /* (aIsWr == ILO_FALSE) */ /* Upload ... */
    {
        mLOBFileSizeOpt = ID_ULONG(0);

        //BUG-24583 'use_separate_files=yes' �ɼ��� ����� ��쿡�� LOBFileSize�� ���� �ʴ´�.
        if (mUseLOBFileOpt == ILO_TRUE && mUseSeparateFilesOpt == ILO_FALSE)  
        {
            /* 1�� LOB ���� ����.
             * �б��� ��� use_separate_files ���δ�
             * ���� ������ ������ �дٰ� %%separate_files�� ���;� �� �� �����Ƿ�
             * ���� ���⿡ �����ϴ��� ������ ó�������� �ʴ´�. */
            if (OpenLOBFile(sHandle,ILO_FALSE, 1, 0, ILO_FALSE) == IDE_SUCCESS)
            {
                /* lob_file_size ��� */
                IDE_TEST_RAISE(GetLOBFileSize( sHandle, &mLOBFileSizeOpt)
                                     != IDE_SUCCESS, OpenLOBFileError);
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(MAllocError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, __LINE__);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION(OpenLOBFileError);
    {
        delete mLOB;
        mLOB = NULL;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * FinalLOBProc.
 *
 * LOB ó���� �������Ѵ�.
 * �����ִ� LOB ������ �ݰ�, LOB wrapper ��ü�� �޸� �����Ѵ�.
 */
IDE_RC iloDataFile::FinalLOBProc( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if (mLOB != NULL)
    {
        delete mLOB;
        mLOB = NULL;
    }
    if (mLOBFP != NULL)
    {
        IDE_TEST(CloseLOBFile(sHandle) != IDE_SUCCESS);
        mLOBFP = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * OpenLOBFile.
 *
 * LOB ������ ����.
 * ������ ���ϸ����κ��� ���� ���ϸ� ��ü��
 * ���ڷ� �Է¹��� ���鿡 ���� LOB ���ϸ��� �����
 * ���⸦ �õ��Ѵ�.
 *
 * @param[in] aIsWr
 *  LOB ���Ͽ� ����(ILO_TRUE)���� �б�(ILO_FALSE)���� ����.
 * @param[in] aLOBFileNo_or_RowNo
 *  LOB ���� ��ȣ(use_lob_file=yes, use_separate_files=no ���)
 *  �Ǵ� �� ��ȣ(use_lob_file=yes, use_separate_files=yes ���).
 *  �� ���ڸ� ������� �ʴ� ��� 0�� �ֵ��� �Ѵ�.
 * @param[in] aColNo
 *  �� ��ȣ(use_lob_file=yes, use_separate_files=yes ���).
 *  �� ���ڸ� ������� �ʴ� ��� 0�� �ֵ��� �Ѵ�.
 * @param[in] aErrMsgPrint
 *  ���� �߻� �� ���� �޽����� ������� ����(ILO_TRUE�̸� ���).
 */
IDE_RC iloDataFile::OpenLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                 iloBool                   aIsWr,
                                 UInt                     aLOBFileNo_or_RowNo,
                                 UInt                     /*aColNo*/,
                                 iloBool                   aErrMsgPrint, 
                                 SChar                   *aFilePath)
{
    SChar sFileName[MAX_FILEPATH_LEN] = { '\0', };
    SChar sMode[3];
    SInt  sPos;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (mLOBFP != NULL)
    {
        IDE_TEST(CloseLOBFile(sHandle) != IDE_SUCCESS);
    }

    if (mUseLOBFileOpt == ILO_TRUE)
    {
        if (mUseSeparateFilesOpt == ILO_TRUE)
        {
            /* BUG-24583 
             * use_separate_files=yes�� �����Ǿ����� ���,
             */
            if (aIsWr == ILO_TRUE)   /* Download */
            {
                (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName), 
                                      "%s/%09"ID_UINT32_FMT".lob",
                                      aFilePath, aLOBFileNo_or_RowNo);
            }
            /* Upload �� ��쿡��, DataFile���� LOB File��� �� �̸��� �о fileOpen�� �����Ѵ�. */
            else
            {
                IDE_TEST_RAISE(idlOS::strlen(mLOBFile[mLOBFileColumnNum]) == 0, OpenError);

                //BUG-24902: use_separate_files=yes�� ��� DataFile�� ��� ���
                /*
                | DataFile \ -d �ɼ� |   �����    |   ������    |
                |:------------------:|:-------------:|:-------------:|
                |      ������      |    DataFile   |    DataFile   |
                |      �����      | -d + DataFile | -d + DataFile |

                * Datafile      : Datafile�� �ִ� FilePath�� ���
                * -d + Datafile : -d �ɼǿ� �ִ� FilePath�ִ� FilePath�� �ٿ��� ���
                */

                // DataFile�� �ִ� ��ΰ� �����ΰ� �ƴҰ��, -d �ɼ��� ������ -d �ɼ��� ��� ����
                if ((! IS_ABSOLUTE_PATH(mLOBFile[mLOBFileColumnNum]))
                 && ( sHandle->mProgOption->m_bExist_d == SQL_TRUE))
                {
                    (void)idlOS::strcpy(sFileName, mDataFilePath);
                }

                // DataFile�� �ִ� FilePath ���� "./"�� �ǹ� �����Ƿ� ����
                if ((idlOS::strlen(mLOBFile[mLOBFileColumnNum]) > 2)
                 && (mLOBFile[mLOBFileColumnNum][0] == '.')
                 && (mLOBFile[mLOBFileColumnNum][1] == IDL_FILE_SEPARATOR))
                {
                    sPos = 2;
                }
                else
                {
                    sPos = 0;
                }
                (void)idlOS::strcat( sFileName, mLOBFile[mLOBFileColumnNum] + sPos );

                mLOBFileColumnNum++; // BUG-24583 ���� Filename�� �б��ϱ� ���ؼ�...
            }
        }
        else /* (mUseSeparateFilesOpt == ILO_FALSE) */
        {
            if (mDataFileNo < 0)
            {
                (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                                      "%s_%09"ID_UINT32_FMT".lob",
                                      mFileNameBody, aLOBFileNo_or_RowNo);
            }
            else
            {
                (void)idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                              "%s_%09"ID_UINT32_FMT".lob%"ID_INT32_FMT,
                              mFileNameBody, aLOBFileNo_or_RowNo, mDataFileNo);
            }
        }

        /* LOB ������ ���� ���� ����. */
        if (aIsWr == ILO_TRUE)
        {
            (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "wb");
        }
        else
        {
            (void)idlOS::snprintf(sMode, ID_SIZEOF(sMode), "rb");
        }
        
        /* BUG-47652 Set file permission */
        mLOBFP = ilo_fopen( sFileName, sMode, sHandle->mProgOption->IsExistFilePerm() );
        IDE_TEST_RAISE(mLOBFP == NULL, OpenError);
        
        if (mUseSeparateFilesOpt == ILO_FALSE)
        {
            mLOBFileNo = aLOBFileNo_or_RowNo;
        }
        mLOBFilePos = ID_ULONG(0);
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(OpenError);
    {
        if (aErrMsgPrint == ILO_TRUE)
        {
            uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
            
            if ( sHandle->mUseApi != SQL_TRUE )
            {
                utePrintfErrorCode(stdout, sHandle->mErrorMgr);
                idlOS::printf("LOB file [%s] open fail\n", sFileName);
            }
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * CloseLOBFile.
 *
 * LOB ������ �ݴ´�.
 */
IDE_RC iloDataFile::CloseLOBFile( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if (mLOBFP != NULL)
    {
        IDE_TEST_RAISE(idlOS::fclose(mLOBFP) != 0, CloseError);
        mLOBFP = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(CloseError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("LOB file close fail\n");
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PrintOneLOBCol.
 *
 * �� ���� LOB �÷� ���� ������ ���� �Ǵ� LOB ���Ͽ� ����Ѵ�.
 *
 * @param[in] aRowNo
 *  ����� LOB �÷��� �Ҽӵ� ���� ��ȣ.
 * @param[in] aCols
 *  �÷� ����(LOB locator ����)���� ����ִ� �ڷᱸ��.
 * @param[in] aColIdx
 *  aCols���� �� ��° �÷��� ����� �÷������� ����.
 */
IDE_RC iloDataFile::PrintOneLOBCol( ALTIBASE_ILOADER_HANDLE  aHandle,
                                    SInt                     aRowNo, 
                                    iloColumns              *aCols, 
                                    SInt                     aColIdx, 
                                    FILE                    *aWriteFp, //PROJ-1714
                                    iloTableInfo            *pTableInfo ) //BUG-24583
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if (mUseLOBFileOpt == ILO_TRUE)
    {
        if (mUseSeparateFilesOpt == ILO_TRUE)
        {
            IDE_TEST(PrintOneLOBColToSepLOBFile( sHandle,
                                                 aRowNo, 
                                                 aCols,
                                                 aColIdx,
                                                 aWriteFp,
                                                 pTableInfo)   //BUG-24583
                                                 != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(PrintOneLOBColToNonSepLOBFile( sHandle, aCols, aColIdx, aWriteFp)
                     != IDE_SUCCESS);
        }
    }
    else
    {
        /* BUG-21064 : CLOB type CSV up/download error */
        // download �� �÷��� type�� CLOB�̶�� ������ enclosing("")�Ѵ�. 
        if( ( sHandle->mProgOption->mRule == csv ) &&
            ( aCols->GetType(aColIdx) == SQL_CLOB ) )
        {
            IDE_TEST( idlOS::fwrite( &sHandle->mProgOption->mCSVEnclosing,
                                     1, 1,
                                     aWriteFp ) != (UInt)1 );
        }
        IDE_TEST(PrintOneLOBColToDataFile( sHandle,
                                           aCols,
                                           aColIdx,
                                           aWriteFp) != IDE_SUCCESS);
        
        if( ( sHandle->mProgOption->mRule == csv ) &&
            ( aCols->GetType(aColIdx) == SQL_CLOB ) )
        {
            IDE_TEST( idlOS::fwrite( &sHandle->mProgOption->mCSVEnclosing,
                                     1, 1,
                                     aWriteFp ) != (UInt)1 );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PrintOneLOBToDataFile.
 *
 * use_lob_file=no�� ��� ȣ��Ǹ�,
 * �� ���� LOB �÷� ���� ������ ���Ͽ� ����Ѵ�.
 * ���� aCols�� ����Ǿ��ִ� LOB locator��
 * LOB wrapper ��ü(mLOB)�� ����Ͽ�
 * �����κ��� LOB �����͸� ���� ������ ���Ͽ� ����.
 *
 * @param[in] aCols
 *  �÷� ����(LOB locator ����)���� ����ִ� �ڷᱸ��.
 * @param[in] aColIdx
 *  aCols���� �� ��° �÷��� ����� �÷������� ����.
 */
IDE_RC iloDataFile::PrintOneLOBColToDataFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                              iloColumns              *aCols, 
                                              SInt                     aColIdx, 
                                              FILE                    *aWriteFp )   //PROJ-1714
{
    SQLSMALLINT  sColDataType;
    SQLSMALLINT  sLOBLocCType;
    UInt         sStrLen;
    UInt         sWrittenLen;
    void        *sBuf;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (*aCols->m_Len[aColIdx] == SQL_NULL_DATA)
    {
        return IDE_SUCCESS;
    }

    sColDataType = aCols->GetType(aColIdx);
    if (sColDataType == SQL_CLOB || sColDataType == SQL_CLOB_LOCATOR)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
    }
    else /* SQL_BINARY || SQL_BLOB || SQL_BLOB_LOCATOR */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
    }
    IDE_TEST(mLOB->OpenForDown( sHandle,
                                sLOBLocCType,
                                *(SQLUBIGINT *)aCols->m_Value[aColIdx],
                                SQL_C_CHAR,
                                iloLOB::LOBAccessMode_RDONLY)
             != IDE_SUCCESS);

    /* �����κ��� LOB �����͸� ����. */
    while (mLOB->Fetch( sHandle, &sBuf, &sStrLen) == IDE_SUCCESS)
    {
        /* LOB �����͸� ������ ���Ͽ� ��. */
        sWrittenLen = (UInt)idlOS::fwrite(sBuf, 1, sStrLen, aWriteFp);
        IDE_TEST_RAISE(sWrittenLen < sStrLen, WriteError);
    }
    /* �����κ��� LOB �����͸� ��� �������� ���� �߻��ߴ��� �˻�. */
    IDE_TEST_RAISE(mLOB->IsFetchError() == ILO_TRUE, FetchError);

    return IDE_SUCCESS;

    IDE_EXCEPTION(FetchError);
    {
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(WriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("LOB data write fail\n");
        }
        
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PrintOneLOBToNonSepLOBFile.
 *
 * use_lob_file=yes, use_separate_files=no�� ��� ȣ��Ǹ�,
 * �� ���� LOB �÷� ���� LOB ���Ͽ� ����Ѵ�.
 * ��, ������ ���Ͽ��� LOB indicator�� ����Ѵ�.
 * ���� aCols�� ����Ǿ��ִ� LOB locator��
 * LOB wrapper ��ü(mLOB)�� ����Ͽ�
 * �����κ��� LOB �����͸� ���� LOB ���Ͽ� ����.
 * LOB ���� ũ�Ⱑ �ɼ� lob_file_size�� �Ѱ� �Ǵ��� �˻��Ͽ�
 * �ڵ������� ���� ��ȣ�� LOB ������ ���� �̾ ����.
 *
 * @param[in] aCols
 *  �÷� ����(LOB locator ����)���� ����ִ� �ڷᱸ��.
 * @param[in] aColIdx
 *  aCols���� �� ��° �÷��� ����� �÷������� ����.
 */
IDE_RC iloDataFile::PrintOneLOBColToNonSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                                   iloColumns              *aCols,
                                                   SInt                     aColIdx, 
                                                   FILE                    *aWriteFp )  //PROJ-1714
{
    SChar        sLOBIndicator[52];
    SQLSMALLINT  sColDataType;
    SQLSMALLINT  sLOBLocCType;
    SQLSMALLINT  sValCType;
    UInt         sLOBLength;
    UInt         sStrLen;
    UInt         sToWriteLen;
    UInt         sWrittenLen;
    void        *sBuf;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (*aCols->m_Len[aColIdx] == SQL_NULL_DATA)
    {
        /* ������ ���Ͽ� LOB indicator ���. */
        sWrittenLen = (UInt)idlOS::fprintf(aWriteFp, "%snull",
                                           mLOBIndicatorOpt);
        IDE_TEST_RAISE(sWrittenLen < mLOBIndicatorOptLen + 4,
                       DataFileWriteError);
        return IDE_SUCCESS;
    }

    sColDataType = aCols->GetType(aColIdx);
    if (sColDataType == SQL_CLOB || sColDataType == SQL_CLOB_LOCATOR)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
        sValCType = SQL_C_CHAR;
    }
    else /* SQL_BINARY || SQL_BLOB || SQL_BLOB_LOCATOR */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
        sValCType = SQL_C_BINARY;
    }
    IDE_TEST(mLOB->OpenForDown( sHandle,
                                sLOBLocCType,
                                *(SQLUBIGINT *)aCols->m_Value[aColIdx],
                                sValCType,
                                iloLOB::LOBAccessMode_RDONLY)
             != IDE_SUCCESS);

    sLOBLength = mLOB->GetLOBLength();
    /* LOB�� NULL�� �ƴ� ��� */
    if (sLOBLength > 0)
    {
        /* LOB ���� ũ�� ������ ���� ��� */
        if (mLOBFileSizeOpt == ID_ULONG(0))
        {
            /* �����κ��� LOB �����͸� ����. */
            while (mLOB->Fetch( sHandle, &sBuf, &sStrLen) == IDE_SUCCESS)
            {
                /* LOB �����͸� LOB ���Ͽ� ��. */
                IDE_TEST_RAISE(idlOS::fwrite(sBuf, 1, sStrLen, mLOBFP) < sStrLen,
                               LOBFileWriteError);
                mLOBFilePos += (ULong)sStrLen;
                mAccumLOBFilePos += (ULong)sStrLen;
            }
            /* �����κ��� LOB �����͸� ��� �������� ���� �߻��ߴ��� �˻�. */
            IDE_TEST_RAISE(mLOB->IsFetchError() == ILO_TRUE, FetchError);
        }
        /* LOB ���� ũ�� ������ �ִ� ��� */
        else /* (mLOBFileSizeOpt > ID_ULONG(0)) */
        {
            /* �����κ��� LOB �����͸� ����. */
            while (mLOB->Fetch( sHandle, &sBuf, &sStrLen) == IDE_SUCCESS)
            {
                /* LOB ���� ũ�� �������� LOB �����͸�
                 * ���� �����ִ� LOB ���Ͽ� ��� ����� �� ���� ����
                 * ��� ���� */
                while (mLOBFileSizeOpt - mLOBFilePos < (ULong)sStrLen)
                {
                    sToWriteLen = (UInt)(mLOBFileSizeOpt - mLOBFilePos);
                    if (sToWriteLen > 0)
                    {
                        /* LOB �����͸� LOB ���Ͽ� ��. */
                        sWrittenLen = (UInt)idlOS::fwrite(sBuf, 1, sToWriteLen,
                                                          mLOBFP);
                        mLOBFilePos += (ULong)sWrittenLen;
                        mAccumLOBFilePos += (ULong)sWrittenLen;
                        IDE_TEST_RAISE(sWrittenLen < sToWriteLen,
                                       LOBFileWriteError);
                    }
                    IDE_TEST_RAISE(CloseLOBFile(sHandle) != IDE_SUCCESS,
                                   OpenOrCloseLOBFileError);
                    /* ���� ��ȣ�� LOB ������ ����. */
                    IDE_TEST_RAISE(OpenLOBFile(sHandle, ILO_TRUE, mLOBFileNo + 1, 0,
                                               ILO_TRUE)
                                   != IDE_SUCCESS, OpenOrCloseLOBFileError);
                    sBuf = (void *)((SChar *)sBuf + sToWriteLen);
                    sStrLen -= sToWriteLen;
                }
                /* LOB �����͸� LOB ���Ͽ� ��. */
                sWrittenLen = (UInt)idlOS::fwrite(sBuf, 1, sStrLen, mLOBFP);
                mLOBFilePos += (ULong)sWrittenLen;
                mAccumLOBFilePos += (ULong)sWrittenLen;
                IDE_TEST_RAISE(sWrittenLen < sStrLen, LOBFileWriteError);
            }
            /* �����κ��� LOB �����͸� ��� �������� ���� �߻��ߴ��� �˻�. */
            IDE_TEST_RAISE(mLOB->IsFetchError() == ILO_TRUE, FetchError);
        }

        /* ������ ���Ͽ� LOB indicator ���. */
        sToWriteLen = (UInt)idlOS::snprintf(sLOBIndicator,
                                         ID_SIZEOF(sLOBIndicator),
                                         "%s%"ID_UINT64_FMT":%"ID_UINT32_FMT,
                                         mLOBIndicatorOpt,
                                         mAccumLOBFilePos - (ULong)sLOBLength,
                                         sLOBLength);
        IDE_TEST_RAISE( idlOS::fwrite(sLOBIndicator, sToWriteLen, 1, aWriteFp)
                        != (UInt)1, DataFileWriteError );
    }
    /* LOB�� NULL�� ��� */
    else /* (sLOBLength == 0) */
    {
        // BUG-31004
        IDE_TEST_RAISE( sHandle->mSQLApi->FreeLOB(mLOB->GetLOBLoc()) != IDE_SUCCESS,
                        FreeLOBError);

        /* ������ ���Ͽ� LOB indicator ���. */
        sWrittenLen = (UInt)idlOS::fprintf(aWriteFp, "%snull",
                                           mLOBIndicatorOpt);
        IDE_TEST_RAISE(sWrittenLen < mLOBIndicatorOptLen + 4,
                       DataFileWriteError);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(OpenOrCloseLOBFileError);
    {
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(FetchError);
    {
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(LOBFileWriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("LOB data write fail\n");
        }
    
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(DataFileWriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("LOB indicator write fail\n");
        }
    }
    IDE_EXCEPTION(FreeLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PrintOneLOBToSepLOBFile.
 *
 * use_lob_file=yes, use_separate_files=yes�� ��� ȣ��Ǹ�,
 * �� ���� LOB �÷� ���� ������ LOB ���Ͽ� ����Ѵ�.
 * ��, ������ ���Ͽ��� LOB indicator�� ����Ѵ�.
 * ���� aCols�� ����Ǿ��ִ� LOB locator��
 * LOB wrapper ��ü(mLOB)�� ����Ͽ�
 * �����κ��� LOB �����͸� ���� LOB ���Ͽ� ����.
 *
 * @param[in] aRowNo
 *  ����� LOB �÷��� �Ҽӵ� ���� ��ȣ.
 * @param[in] aCols
 *  �÷� ����(LOB locator ����)���� ����ִ� �ڷᱸ��.
 * @param[in] aColIdx
 *  aCols���� �� ��° �÷��� ����� �÷������� ����.
 */
IDE_RC iloDataFile::PrintOneLOBColToSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                                SInt                     aRowNo, 
                                                iloColumns              *aCols,
                                                SInt                     aColIdx, 
                                                FILE                    *aWriteFp,             //PROJ-1714
                                                iloTableInfo            *pTableInfo)   //BUG-24583
{
    SQLSMALLINT  sColDataType;
    SQLSMALLINT  sLOBLocCType;
    SQLSMALLINT  sValCType;
    UInt         sStrLen;
    void        *sBuf;
    SChar        sTmpFilePath[MAX_FILEPATH_LEN];
    SChar        sTableName[MAX_OBJNAME_LEN];
    SChar        sColName[MAX_OBJNAME_LEN];
   
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (*aCols->m_Len[aColIdx] == SQL_NULL_DATA)
    {
       /* BUG-24584
        * NULL �� ���� �ƹ� ó���� ���� ����..
        */
        return IDE_SUCCESS;
    }

    sColDataType = aCols->GetType(aColIdx);
    if (sColDataType == SQL_CLOB || sColDataType == SQL_CLOB_LOCATOR)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
        sValCType = SQL_C_CHAR;
    }
    else /* SQL_BINARY || SQL_BLOB || SQL_BLOB_LOCATOR */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
        sValCType = SQL_C_BINARY;
    }
    IDE_TEST(mLOB->OpenForDown( sHandle,
                                sLOBLocCType,
                                *(SQLUBIGINT *)aCols->m_Value[aColIdx],
                                sValCType, 
                                iloLOB::LOBAccessMode_RDONLY)
             != IDE_SUCCESS);

    if (mLOB->GetLOBLength() > 0)
    {
        /* BUG-24583
         * -lob 'use_separate_files=yes'�ɼ��� ����� ��쿡�� 
         * �ش� �������� ���Ͽ� �����͸� �����Ѵ�. 
         */
        (void)sprintf(sTmpFilePath, "%s/%s",
                      pTableInfo->GetTransTableName(sTableName,(UInt)MAX_OBJNAME_LEN),
                      aCols->GetTransName(aColIdx,sColName,(UInt)MAX_OBJNAME_LEN));
        IDE_TEST_RAISE(OpenLOBFile( sHandle,
                                    ILO_TRUE,
                                    aRowNo,
                                    aColIdx + 1,
                                    ILO_TRUE, 
                                    sTmpFilePath)
                       != IDE_SUCCESS, OpenOrCloseLOBFileError);

        /* �����κ��� LOB �����͸� ����. */
        while (mLOB->Fetch( sHandle, &sBuf, &sStrLen) == IDE_SUCCESS)
        {
            /* LOB �����͸� LOB ���Ͽ� ��. */
            IDE_TEST_RAISE(idlOS::fwrite(sBuf, 1, sStrLen, mLOBFP) < sStrLen,
                        LOBFileWriteError);
        }
        /* �����κ��� LOB �����͸� ��� �������� ���� �߻��ߴ��� �˻�. */
        IDE_TEST_RAISE(mLOB->IsFetchError() == ILO_TRUE, FetchError);

        /* LOB ������ �ݴ´�. */
        IDE_TEST_RAISE(CloseLOBFile(sHandle) != IDE_SUCCESS, OpenOrCloseLOBFileError);

        /* ������ ���Ͽ� LOB indicator ���. */
        (UInt)idlOS::fprintf(aWriteFp, "%s/%09"ID_UINT32_FMT".lob",       //BUG-24583
                             sTmpFilePath, aRowNo);
    }
    else /* (mLOB->GetLOBLength() == 0) */
    {
        // BUG-31004
        IDE_TEST_RAISE( sHandle->mSQLApi->FreeLOB(mLOB->GetLOBLoc()) != IDE_SUCCESS,
                        FreeLOBError);

        /* BUG-24583 
         * LOB Data�� ���� ���, %%NULL�� ���Ͽ� ǥ������ �ʴ´�.
         * �ƹ� ó���� ���� �ʴ´�. 
         */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(OpenOrCloseLOBFileError);
    {
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(FetchError);
    {
        (void)CloseLOBFile(sHandle);
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(LOBFileWriteError);
    {
        uteSetErrorCode(sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr); 
            (void)idlOS::printf("LOB data write fail\n");
        }
        
        (void)CloseLOBFile(sHandle);
        (void)mLOB->CloseForDown(sHandle);
    }
    IDE_EXCEPTION(FreeLOBError);
    {
        /* BUG-42849 Error code and message should not be output to stdout.
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }*/
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * AnalLOBIndicator.
 *
 * ������ ���Ϸκ��� ���� LOB indicator�� �м��Ͽ�,
 * LOB indicator �����ڷ� �����ϴ��� ���θ� �˻��ϰ�,
 * LOB ���ϳ����� LOB �������� ���� ��ġ�� ���̸� ��´�.
 * �м��� LOB indicator�� m_TokenValue�� ����ִ�.
 *
 * @param[out] aLOBPhyOffs
 *  LOB ���ϳ����� LOB �������� ���� ��ġ.
 * @param[out] aLOBPhyLen
 *  LOB ���ϳ����� LOB �������� ����(������ ����).
 */
IDE_RC iloDataFile::AnalLOBIndicator(ULong *aLOBPhyOffs, ULong *aLOBPhyLen)
{

    /* BUG-24583
     * use_separate_files=yes�� ������ ��쿡�� LobIndicator�� ������� �ʰ�, 
     * FilePath+FileName �� �����ȴ�.
     */
    if (mUseSeparateFilesOpt == ILO_TRUE)
    {
        if ( idlOS::strcmp(m_TokenValue, "") != 0 )
        {
            // FilePath + FileName ����.. 
            (void)idlOS::strcpy( mLOBFile[mLOBFileColumnNum], m_TokenValue);
            (*aLOBPhyLen) = ID_ULONG(1);        
            mLOBFileColumnNum++;        //���� Filename��  �����ϱ� ���ؼ�...
        }
        else
        {
            //LOB Data�� ���� ��� (Datafile�� LOB File ��ΰ� ���� ���)
            (*aLOBPhyLen) = ID_ULONG(0);
        }
    }
    else /* (mUseSeparateFilesOpt == ILO_FALSE) */
    {
        /* LOB indicator �����ڷ� �����ϴ��� �˻� */
        IDE_TEST(idlOS::strncmp(m_TokenValue, mLOBIndicatorOpt,
                 mLOBIndicatorOptLen) != 0);    
    
        /* "%%null"�� ��� */
        if (idlOS::strcmp(&m_TokenValue[mLOBIndicatorOptLen], "null") == 0)
        {
            (*aLOBPhyLen) = ID_ULONG(0);
        }
        else if (mUseSeparateFilesOpt == ILO_FALSE)
        {
            /* "%%����:����"�κ��� ���� ��ġ�� ���̸� ��� �õ� */
            IDE_TEST (sscanf(&m_TokenValue[mLOBIndicatorOptLen],
                      "%19"ID_UINT64_FMT":%19"ID_UINT64_FMT,
                      aLOBPhyOffs, aLOBPhyLen) < 2)
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    idlOS::printf("LOB indicator \"%s\" not found in token \"%s\"\n",
                  mLOBIndicatorOpt, m_TokenValue);

    return IDE_FAILURE;
}

/**
 * LoadOneRecordLOBCols.
 *
 * �� ���ڵ��� LOB �÷��鿡 ���� �����͸� ���Ϸκ��� �о� ������ �����Ѵ�.
 *
 * @param[in] aRowNo
 *  ���ڵ��� �� ��ȣ.
 * @param[in] aTableInfo
 *  �÷����� ������ ����ִ� �ڷᱸ��.
 *  �÷� �������� ���ϳ����� LOB �������� ���� ��ġ�� ����,
 *  LOB locator�� ���Եȴ�.
 * @param[in] aArrIdx
 *  �������� ������ ���ۿ� array binding�� ����� ���
 *  aTableInfo�� �÷� �� �迭���� �� ��° ���� ����� �������� ����.
 */
IDE_RC iloDataFile::LoadOneRecordLOBCols( ALTIBASE_ILOADER_HANDLE  aHandle,
                                          SInt                     aRowNo, 
                                          iloTableInfo            *aTableInfo,
                                          SInt                     aArrIdx, 
                                          iloSQLApi               *aISPApi )
{
    iloBool sErrExist = ILO_FALSE;
    IDE_RC sRC;
    SInt   sColIdx;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    for (sColIdx = 0; sColIdx < aTableInfo->GetAttrCount(); sColIdx++)
    {
        /* ���ε���� ���� �÷� skip */
        if (aTableInfo->seqEqualChk(sHandle, sColIdx) >= 0 ||
            aTableInfo->mSkipFlag[sColIdx] == ILO_TRUE)
        {
            continue;
        }
        /* ���ε���� ���� �÷� skip */
        if (aTableInfo->GetAttrType(sColIdx) == ISP_ATTR_TIMESTAMP &&
            sHandle->mParser.mAddFlag == ILO_TRUE)
        {
            continue;
        }
        if (aTableInfo->GetAttrType(sColIdx) == ISP_ATTR_CLOB ||
            aTableInfo->GetAttrType(sColIdx) == ISP_ATTR_BLOB)
        {
            sRC = LoadOneLOBCol( sHandle, aRowNo,
                                 aTableInfo, sColIdx, aArrIdx, aISPApi);
            if (sRC != IDE_SUCCESS)
            {
                sErrExist = ILO_TRUE;
                /* �Ʒ��� ���� ������ ���
                 * �� �̻� LOB �÷��� �о������ �ʰ� ��� �Լ� ����. */
                IDE_TEST(uteGetErrorCODE( sHandle->mErrorMgr) == 0x3b032 ||
                         uteGetErrorCODE( sHandle->mErrorMgr) == 0x5003b ||
                             /* buffer full */
                         uteGetErrorCODE( sHandle->mErrorMgr) == 0x51043 ||
                             /* ��� ��� */
                         uteGetErrorCODE( sHandle->mErrorMgr) == 0x91044 ||
                             /* Data file IO error */
                         uteGetErrorCODE( sHandle->mErrorMgr) == 0x91045);
                             /* LOB file IO error */
            }
        }
    }

    IDE_TEST(sErrExist == ILO_TRUE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * LoadOneLOBCol.
 *
 * �� LOB �÷��� ���� �����͸� ���Ϸκ��� �о� ������ �����Ѵ�.
 *
 * @param[in] aRowNo
 *  ���ڵ��� �� ��ȣ.
 * @param[in] aTableInfo
 *  �÷����� ������ ����ִ� �ڷᱸ��.
 *  �÷� �������� ���ϳ����� LOB �������� ���� ��ġ�� ����,
 *  LOB locator�� ���Եȴ�.
 * @param[in] aColIdx
 *  aTableInfo������ �� ��° �÷������� ����.
 * @param[in] aArrIdx
 *  �������� ������ ���ۿ� array binding�� ����� ���
 *  aTableInfo�� �÷� �� �迭���� �� ��° ���� ����� �������� ����.
 */
IDE_RC iloDataFile::LoadOneLOBCol( ALTIBASE_ILOADER_HANDLE  aHandle,
                                   SInt                     aRowNo,
                                   iloTableInfo            *aTableInfo,
                                   SInt                     aColIdx, 
                                   SInt                     aArrIdx, 
                                   iloSQLApi               *aISPApi)
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (mUseLOBFileOpt == ILO_TRUE)
    {
        if (mUseSeparateFilesOpt == ILO_TRUE)
        {
            IDE_TEST(LoadOneLOBColFromSepLOBFile( sHandle,
                                                  aRowNo,
                                                  aTableInfo,
                                                  aColIdx,
                                                  aArrIdx,
                                                  aISPApi) != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(LoadOneLOBColFromNonSepLOBFile( sHandle, 
                                                     aTableInfo,
                                                     aColIdx,
                                                     aArrIdx, 
                                                     aISPApi) != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST(LoadOneLOBColFromDataFile( sHandle, 
                                            aTableInfo, 
                                            aColIdx,
                                            aArrIdx,
                                            aISPApi) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * LoadOneLOBColFromDataFile.
 *
 * use_lob_file=no�� ��� ȣ��Ǹ�,
 * �� LOB �÷��� ���� �����͸� ������ ���Ϸκ��� �о� ������ �����Ѵ�.
 *
 * @param[in] aTableInfo
 *  �÷����� ������ ����ִ� �ڷᱸ��.
 *  �÷� �������� ������ ���ϳ����� LOB �������� ���� ��ġ�� ����,
 *  LOB locator�� ���Եȴ�.
 * @param[in] aColIdx
 *  aTableInfo������ �� ��° �÷������� ����.
 * @param[in] aArrIdx
 *  �������� ������ ���ۿ� array binding�� ����� ���
 *  aTableInfo�� �÷� �� �迭���� �� ��° ���� ����� �������� ����.
 */
IDE_RC iloDataFile::LoadOneLOBColFromDataFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                               iloTableInfo             *aTableInfo,
                                               SInt                      aColIdx, 
                                               SInt                      aArrIdx, 
                                               iloSQLApi                *aISPApi )
{
    EispAttrType  sColDataType;
    SQLSMALLINT   sLOBLocCType;
    UInt          sBufLen;
    UInt          sReadLen;
    UInt          sStrLen;
    ULong         sAccumStrLen;
    void         *sBuf;
    void         *sAttrValPtr;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_ASSERT((mLOB != NULL) && (aTableInfo != NULL) && (aISPApi != NULL));

    sColDataType = aTableInfo->GetAttrType(aColIdx);
    if (sColDataType == ISP_ATTR_CLOB)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
        /* BUG-21064 : CLOB type CSV up/download error */
        mLOB->mIsBeginCLOBAppend = ILO_TRUE;
    }
    else /* (sColDataType == ISP_ATTR_BLOB) */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
    }

    sAttrValPtr = aTableInfo->GetAttrVal(aColIdx, aArrIdx);
    IDE_ASSERT(sAttrValPtr != NULL);

    IDE_TEST(mLOB->OpenForUp( sHandle,
                              sLOBLocCType,
                              *(SQLUBIGINT *)sAttrValPtr,
                              SQL_C_CHAR,
                              iloLOB::LOBAccessMode_WRONLY, aISPApi)
                              != IDE_SUCCESS);

    /* LOB �������� ���̰� 0���� ū ��� */
    if (aTableInfo->mLOBPhyLen[aColIdx][aArrIdx] != ID_ULONG(0))
    {
        BackupDataFilePos();
        /* ������ ���ϳ����� LOB �������� ���� ��ġ�� �̵�. */
        IDE_TEST_RAISE(DataFileSeek( sHandle,
                                     aTableInfo->mLOBPhyOffs[aColIdx][aArrIdx])
                                     != IDE_SUCCESS, SeekError);

        (void)mLOB->GetBuffer(&sBuf, &sBufLen);

        for (sAccumStrLen = ID_ULONG(0);
             sAccumStrLen < aTableInfo->mLOBLen[aColIdx][aArrIdx];
             sAccumStrLen += (ULong)sStrLen)
        {
            if (aTableInfo->mLOBLen[aColIdx][aArrIdx] - sAccumStrLen
                > (ULong)sBufLen)
            {
                sStrLen = sBufLen;
            }
            else
            {
                sStrLen = (UInt)(aTableInfo->mLOBLen[aColIdx][aArrIdx]
                                 - sAccumStrLen);
            }
            /* ������ ���Ϸκ��� ������ ����. */
            // BUG-24873 clob ������ �ε��Ҷ� [ERR-91044 : Error occurred during data file IO.] �߻�
            // ���̳ʸ� ���� �����Ƿ� ������� ���� ������ �ʿ䰡 ����.
            sReadLen = (UInt)idlOS::fread(sBuf, 1, sStrLen, m_DataFp);
            mDataFilePos += sReadLen;

            IDE_TEST_RAISE(sReadLen < sStrLen, ReadError);
            /* ������ ������ ����. */
            IDE_TEST_RAISE(mLOB->Append( sHandle, sStrLen, aISPApi)
                                         != IDE_SUCCESS, AppendError);
        }

        /* ������ ���ϳ������� ��ġ�� LOB ������ �б� ���� �� ��ġ�� ����. */
        IDE_TEST_RAISE(RestoreDataFilePos(sHandle) != IDE_SUCCESS, SeekError);
    }

    /* BUG-21064 : CLOB type CSV up/download error */
    mLOB->mIsBeginCLOBAppend = ILO_FALSE;
    mLOB->mSaveBeginCLOBEnc  = ILO_FALSE;
    mLOB->mIsCSVCLOBAppend   = ILO_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION(SeekError);
    {
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(ReadError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("LOB data read fail\n");
        }
        
        (void)RestoreDataFilePos(sHandle);
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(AppendError);
    {
        (void)RestoreDataFilePos(sHandle);
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * LoadOneLOBColFromNonSepLOBFile.
 *
 * use_lob_file=yes, use_separate_files=no�� ��� ȣ��Ǹ�,
 * �� LOB �÷��� ���� �����͸� LOB ���Ϸκ��� �о� ������ �����Ѵ�.
 * LOB �����Ͱ� ���� LOB ���Ͽ� �������� ���
 * �ڵ������� �ش� LOB ���ϵ��� ���� �����͸� �о���δ�.
 *
 * @param[in] aTableInfo
 *  �÷����� ������ ����ִ� �ڷᱸ��.
 *  �÷� �������� LOB ���ϳ����� LOB �������� ���� ��ġ�� ����,
 *  LOB locator�� ���Եȴ�.
 * @param[in] aColIdx
 *  aTableInfo������ �� ��° �÷������� ����.
 * @param[in] aArrIdx
 *  �������� ������ ���ۿ� array binding�� ����� ���
 *  aTableInfo�� �÷� �� �迭���� �� ��° ���� ����� �������� ����.
 */
IDE_RC iloDataFile::LoadOneLOBColFromNonSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                                    iloTableInfo            *aTableInfo,
                                                    SInt                      aColIdx, 
                                                    SInt                      aArrIdx,
                                                    iloSQLApi                *aISPApi )
{
    EispAttrType  sColDataType;
    SQLSMALLINT   sLOBLocCType;
    SQLSMALLINT   sValCType;
    UInt          sBufLen;
    UInt          sReadLen;
    UInt          sStrLen;
    ULong         sAccumStrLen;
    ULong         sAccumStrLenTo;
    void         *sBuf;
    void         *sAttrValPtr;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_ASSERT((mLOB != NULL) && (aTableInfo != NULL) && (aISPApi != NULL));

    sColDataType = aTableInfo->GetAttrType(aColIdx);
    if (sColDataType == ISP_ATTR_CLOB)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
        sValCType = SQL_C_CHAR;
    }
    else /* (sColDataType == ISP_ATTR_BLOB) */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
        sValCType = SQL_C_BINARY;
    }

    sAttrValPtr = aTableInfo->GetAttrVal(aColIdx, aArrIdx);
    IDE_ASSERT(sAttrValPtr != NULL);

    IDE_TEST(mLOB->OpenForUp( sHandle,
                              sLOBLocCType,
                              *(SQLUBIGINT *)sAttrValPtr,
                              sValCType, 
                              iloLOB::LOBAccessMode_WRONLY, 
                              aISPApi)
                              != IDE_SUCCESS);

    /* LOB �������� ���̰� 0���� ū ��� */
    if (aTableInfo->mLOBPhyLen[aColIdx][aArrIdx] != ID_ULONG(0))
    {
        /* LOB ���ϳ����� LOB �������� ���� ��ġ�� �̵�. */
        IDE_TEST_RAISE(LOBFileSeek( sHandle, aTableInfo->mLOBPhyOffs[aColIdx][aArrIdx])
                       != IDE_SUCCESS, SeekError);
        (void)mLOB->GetBuffer(&sBuf, &sBufLen);

        sAccumStrLen = ID_ULONG(0);
        /* ���� �����ִ� LOB ���Ͽ���
         * �ʿ��� ��� LOB �����͸� ���� �� ���� ���� ��� ���� */
        while (mLOBFilePos + aTableInfo->mLOBPhyLen[aColIdx][aArrIdx]
               - sAccumStrLen > mLOBFileSizeOpt)
        {
            sAccumStrLenTo = sAccumStrLen + mLOBFileSizeOpt - mLOBFilePos;
            for (; sAccumStrLen < sAccumStrLenTo;
                 sAccumStrLen += (ULong)sStrLen)
            {
                if (sAccumStrLenTo - sAccumStrLen > (ULong)sBufLen)
                {
                    sStrLen = sBufLen;
                }
                else
                {
                    sStrLen = (UInt)(sAccumStrLenTo - sAccumStrLen);
                }
                /* LOB ���Ϸκ��� ������ ����. */
                sReadLen = (UInt)idlOS::fread(sBuf, 1, sStrLen, mLOBFP);
                mLOBFilePos += (ULong)sReadLen;
                mAccumLOBFilePos += (ULong)sReadLen;
                IDE_TEST_RAISE(sReadLen < sStrLen, ReadError);
                /* ������ ������ ����. */
                IDE_TEST_RAISE(mLOB->Append( sHandle, sStrLen, aISPApi ) 
                                             != IDE_SUCCESS, AppendError);
            }

            IDE_TEST_RAISE(CloseLOBFile(sHandle) != IDE_SUCCESS, SeekError);
            /* ���� ��ȣ�� LOB ������ ����. */
            IDE_TEST_RAISE(OpenLOBFile( sHandle, ILO_FALSE, mLOBFileNo + 1, 0, ILO_TRUE)
                           != IDE_SUCCESS, SeekError);
        }

        sAccumStrLenTo = aTableInfo->mLOBPhyLen[aColIdx][aArrIdx];
        for (; sAccumStrLen < sAccumStrLenTo; sAccumStrLen += (ULong)sStrLen)
        {
            if (sAccumStrLenTo - sAccumStrLen > (ULong)sBufLen)
            {
                sStrLen = sBufLen;
            }
            else
            {
                sStrLen = (UInt)(sAccumStrLenTo - sAccumStrLen);
            }
            /* LOB ���Ϸκ��� ������ ����. */
            sReadLen = (UInt)idlOS::fread(sBuf, 1, sStrLen, mLOBFP);
            mLOBFilePos += (ULong)sReadLen;
            mAccumLOBFilePos += (ULong)sReadLen;
            IDE_TEST_RAISE(sReadLen < sStrLen, ReadError);
            /* ������ ������ ����. */
            IDE_TEST_RAISE(mLOB->Append( sHandle, sStrLen, aISPApi )
                                        != IDE_SUCCESS, AppendError);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(SeekError);
    {
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(ReadError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout,  sHandle->mErrorMgr);
            (void)idlOS::printf("LOB data read fail\n");
        }
    
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(AppendError);
    {
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * LoadOneLOBColFromSepLOBFile.
 *
 * use_lob_file=yes, use_separate_files=yes�� ��� ȣ��Ǹ�,
 * �� LOB �÷��� ����
 * �����͸� ������ LOB ���Ϸκ��� �о� ������ �����Ѵ�.
 *
 * @param[in] aRowNo
 *  ���ڵ��� �� ��ȣ.
 * @param[in] aTableInfo
 *  �÷����� ������ ����ִ� �ڷᱸ��.
 *  �÷� �������� null�� LOB���� ����, LOB locator�� ���Եȴ�.
 * @param[in] aColIdx
 *  aTableInfo������ �� ��° �÷������� ����.
 * @param[in] aArrIdx
 *  �������� ������ ���ۿ� array binding�� ����� ���
 *  aTableInfo�� �÷� �� �迭���� �� ��° ���� ����� �������� ����.
 */
IDE_RC iloDataFile::LoadOneLOBColFromSepLOBFile( ALTIBASE_ILOADER_HANDLE  aHandle,
                                                 SInt                     aRowNo,
                                                 iloTableInfo            *aTableInfo,
                                                 SInt                     aColIdx, 
                                                 SInt                     aArrIdx, 
                                                 iloSQLApi               *aISPApi )
{
    EispAttrType  sColDataType;
    SQLSMALLINT   sLOBLocCType;
    SQLSMALLINT   sValCType;
    UInt          sBufLen;
    UInt          sStrLen;
    void         *sBuf;
    void         *sAttrValPtr;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    IDE_ASSERT((mLOB != NULL) && (aTableInfo != NULL) && (aISPApi != NULL));

    sColDataType = aTableInfo->GetAttrType(aColIdx);
    if (sColDataType == ISP_ATTR_CLOB)
    {
        sLOBLocCType = SQL_C_CLOB_LOCATOR;
        sValCType = SQL_C_CHAR;
    }
    else /* (sColDataType == ISP_ATTR_BLOB) */
    {
        sLOBLocCType = SQL_C_BLOB_LOCATOR;
        sValCType = SQL_C_BINARY;
    }

    sAttrValPtr = aTableInfo->GetAttrVal(aColIdx, aArrIdx);
    IDE_ASSERT(sAttrValPtr != NULL);

    IDE_TEST(mLOB->OpenForUp( sHandle,
                              sLOBLocCType,
                              *(SQLUBIGINT *)sAttrValPtr,
                              sValCType, 
                              iloLOB::LOBAccessMode_WRONLY, 
                              aISPApi)
                              != IDE_SUCCESS);
    
    if (aTableInfo->mLOBPhyLen[aColIdx][aArrIdx] != ID_ULONG(0))
    {
        /* �� ��ȣ�� �� ��ȣ�� ������ ������ LOB ������ ����. */
        IDE_TEST_RAISE(OpenLOBFile(sHandle, ILO_FALSE, aRowNo, aColIdx + 1, ILO_TRUE)
                       != IDE_SUCCESS, OpenOrCloseLOBFileError);
        (void)mLOB->GetBuffer(&sBuf, &sBufLen);

        /* LOB ���Ϸκ��� ������ ����. */
        sStrLen = (UInt)idlOS::fread(sBuf, 1, sBufLen, mLOBFP);
        while (sStrLen == sBufLen)
        {
            /* ������ ������ ����. */
            IDE_TEST_RAISE(mLOB->Append( sHandle, sStrLen, aISPApi)
                                       != IDE_SUCCESS, AppendError);
            /* LOB ���Ϸκ��� ������ ����. */
            sStrLen = (UInt)idlOS::fread(sBuf, 1, sBufLen, mLOBFP);
        }
        if (sStrLen > 0)
        {
            /* ������ ������ ����. */
            IDE_TEST_RAISE(mLOB->Append( sHandle, sStrLen, aISPApi )
                                       != IDE_SUCCESS, AppendError);
        }
        /* LOB ���� �б� �������� I/O ���� �߻� ���� �˻�. */
        IDE_TEST_RAISE(ferror(mLOBFP), ReadError);

        /* LOB ������ �ݴ´�. */
        IDE_TEST_RAISE(CloseLOBFile(sHandle) != IDE_SUCCESS, OpenOrCloseLOBFileError);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(OpenOrCloseLOBFileError);
    {
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(ReadError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout,  sHandle->mErrorMgr);
            (void)idlOS::printf("LOB data read fail\n");
        }
        
        (void)CloseLOBFile(sHandle);
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION(AppendError);
    {
        (void)CloseLOBFile(sHandle);
        (void)mLOB->CloseForUp( sHandle, aISPApi );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * DataFileSeek.
 *
 * ������ ���ϳ������� ���� ��ġ�� ���� aDestPos ��ġ�� �ű��.
 *
 * @param[in] aDestPos
 *  ������ ���ϳ����� �̵��ϰ��� �ϴ� ���ο� ��ġ.
 */
IDE_RC iloDataFile::DataFileSeek( ALTIBASE_ILOADER_HANDLE aHandle,
                                  ULong                   aDestPos )
{
    ULong   sDiff;
#if defined(VC_WIN32) || defined(VC_WIN64)
    __int64 sOffs;
#else
    ULong   sI;
    ULong   sTo;
    long    sOffs;
#endif

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (aDestPos > mDataFilePos)
    {
        sDiff = aDestPos - mDataFilePos;
    }
    else
    {
        sDiff = mDataFilePos - aDestPos;
    }

#if defined(VC_WIN32) || defined(VC_WIN64)
    sOffs = (aDestPos > mDataFilePos)? (__int64)sDiff : -(__int64)sDiff;
    IDE_TEST(::_fseeki64(m_DataFp, sOffs, SEEK_CUR) < 0);
#else
    if (ID_SIZEOF(long) >= 8 || sDiff <= ID_ULONG(0x7FFFFFFF))
    {
        sOffs = (aDestPos > mDataFilePos)? (long)sDiff : -(long)sDiff;
        IDE_TEST(idlOS::fseek(m_DataFp, sOffs, SEEK_CUR) < 0);
    }
    /* long�� 4����Ʈ�̰� �̵����� long�� ������ ��� ���,
     * ���� ���� fseek()�� �ʿ��ϴ�.
     *
     * BUGBUG :
     * ��κ��� ��� fseek()����
     * ������ ��ġ�� LONG_MAX�� �Ѿ�� �� �� ������,
     * fseek()�� �̷��� �̵��� ������� ���� ä ������ ������ ���̴�. */
    else
    {
        sTo = sDiff / ID_ULONG(0x7FFFFFFF);
        sOffs = (aDestPos > mDataFilePos)? 0x7FFFFFFF : -0x7FFFFFFF;
        for (sI = ID_ULONG(0); sI < sTo; sI++)
        {
            IDE_TEST(idlOS::fseek(m_DataFp, sOffs, SEEK_CUR) < 0);
        }
        if (aDestPos > mDataFilePos)
        {
            sOffs = (long)(sDiff % ID_ULONG(0x7FFFFFFF));
        }
        else
        {
            sOffs = -(long)(sDiff % ID_ULONG(0x7FFFFFFF));
        }
        if (sOffs != 0)
        {
            IDE_TEST(idlOS::fseek(m_DataFp, sOffs, SEEK_CUR) < 0);
        }
    }
#endif /* defined(VC_WIN32) || defined(VC_WIN64) */

    mDataFilePos = aDestPos;  //PROJ-1714

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_Data_File_IO_Error);
    
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        (void)idlOS::printf("Data file seek fail\n");
    }

    return IDE_FAILURE;
}

/**
 * GetLOBFileSize.
 *
 * ���� �����ִ� LOB ������ ũ�⸦ ���Ѵ�.
 *
 * @param[out] aLOBFileSize
 *  LOB ���� ũ��.
 */
IDE_RC iloDataFile::GetLOBFileSize( ALTIBASE_ILOADER_HANDLE  aHandle, 
                                    ULong                   *aLOBFileSize)
{
#if defined(VC_WIN32) || defined(VC_WIN64)
    __int64 sOffs;
#else
    SChar  *sBuf;
    UInt    sReadLen;
    ULong   sI;
    ULong   sTo;
    long    sOffs;
#endif

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

/* 1.Windows�� ��� */
#if defined(VC_WIN32) || defined(VC_WIN64)
    /* 1.1 ���� ũ�⸦ ��´�. */
    IDE_TEST(::_fseeki64(mLOBFP, 0, SEEK_END) < 0);
    sOffs = ::_ftelli64(mLOBFP);
    IDE_TEST(sOffs < 0);
    /* 1.2 ���� Ŀ���� ����ġ�� �����Ѵ�. */
    IDE_TEST(::_fseeki64(mLOBFP, (__int64)mLOBFilePos, SEEK_SET) < 0);

    (*aLOBFileSize) = (ULong)sOffs;
    return IDE_SUCCESS;
#else /* defined(VC_WIN32) || defined(VC_WIN64) */
    /* 2.long�� 8����Ʈ �̻��� ��� */
    if (ID_SIZEOF(long) >= 8)
    {
        /* 2.1 ���� ũ�⸦ ��´�. */
        IDE_TEST(idlOS::fseek(mLOBFP, 0, SEEK_END) < 0);
        sOffs = idlOS::ftell(mLOBFP);
        IDE_TEST(sOffs < 0);
        /* 2.2 ���� Ŀ���� ����ġ�� �����Ѵ�. */
        IDE_TEST(idlOS::fseek(mLOBFP, (long)mLOBFilePos, SEEK_SET) < 0);

        (*aLOBFileSize) = (ULong)sOffs;
        return IDE_SUCCESS;
    }
    else /* (ID_SIZEOF(long) == 4) */
    {
        /* 3.long�� 4����Ʈ�̰� ���� ũ�Ⱑ LONG_MAX ������ ��� */
        /* 3.1 ���� ũ�⸦ ��´�. */
        if (idlOS::fseek(mLOBFP, 0, SEEK_END) == 0)
        {
            sOffs = idlOS::ftell(mLOBFP);
            if (sOffs >= 0)
            {
                /* 3.2 ���� Ŀ���� ����ġ�� �����Ѵ�. */
                IDE_TEST(idlOS::fseek(mLOBFP, (long)mLOBFilePos, SEEK_SET)
                         < 0);

                (*aLOBFileSize) = (ULong)sOffs;
                return IDE_SUCCESS;
            }
        }

        /* 4.long�� 4����Ʈ�̰� ���� ũ�Ⱑ LONG_MAX���� ū ���
         *
         * BUGBUG :
         * ��κ��� ��� fread()����
         * ������ ��ġ�� LONG_MAX�� �Ѿ�� �� �� ������,
         * fread()�� �� �̻��� ���� ��ġ ������ ������� ���� ä
         * ������ ������ ���̴�. */
        /* 4.1 ���� ũ�⸦ ��´�. */
        IDE_TEST(idlOS::fseek(mLOBFP, 0x7FFFFE00, SEEK_SET) < 0);
        (*aLOBFileSize) = ID_ULONG(0x7FFFFE00);
        sBuf = (SChar *)idlOS::malloc(32768);
        IDE_TEST(sBuf == NULL);

        while ((sReadLen = (UInt)idlOS::fread(sBuf, 1, 32768, mLOBFP))
               == 32768)
        {
            (*aLOBFileSize) += ID_ULONG(32768);
        }
        (*aLOBFileSize) += (ULong)sReadLen;

        idlOS::free(sBuf);
        sBuf = NULL;
        IDE_TEST(ferror(mLOBFP));

        /* 4.2 ���� Ŀ���� ����ġ�� �����Ѵ�. */
        if (mLOBFilePos <= ID_ULONG(0x7FFFFFFF))
        {
            IDE_TEST(idlOS::fseek(mLOBFP, (long)mLOBFilePos, SEEK_SET) < 0);
        }
        else
        {
            IDE_TEST(idlOS::fseek(mLOBFP, 0, SEEK_SET) < 0);
            sTo = mLOBFilePos / ID_ULONG(0x7FFFFFFF);
            for (sI = ID_ULONG(0); sI < sTo; sI++)
            {
                IDE_TEST(idlOS::fseek(mLOBFP, 0x7FFFFFFF, SEEK_CUR) < 0);
            }
            sOffs = (long)(mLOBFilePos % ID_ULONG(0x7FFFFFFF));
            if (sOffs != 0)
            {
                IDE_TEST(idlOS::fseek(m_DataFp, sOffs, SEEK_CUR) < 0);
            }
        }

        return IDE_SUCCESS;
    }
#endif /* defined(VC_WIN32) || defined(VC_WIN64) */

    IDE_EXCEPTION_END;
 
    uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
    
    if ( sHandle->mUseApi != SQL_TRUE )
    {
        utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        (void)idlOS::printf("LOB file size get fail\n");
    }
    
    return IDE_FAILURE;
}

/**
 * LOBFileSeek.
 *
 * LOB ���ϵ鿡���� ���� ��ġ�� ���� aAccumDestPos ��ġ�� �ű��.
 * ���� aAccumDestPos�� LOB ���ϵ鿡���� ���� ��ġ�̴�.
 * ���� ��ġ�� � LOB ���Ϻ��� ���� ���� ��ȣ�� ����
 * LOB ���ϵ��� ũ�Ⱑ ������ ���� ���Ѵ�.
 *
 * @param[in] aAccumDestPos
 *  LOB ���ϵ鿡�� �̵��ϰ��� �ϴ� ���ο� ��ġ.
 */
IDE_RC iloDataFile::LOBFileSeek( ALTIBASE_ILOADER_HANDLE aHandle, ULong aAccumDestPos)
{
    UInt    sDestFileNo;
    ULong   sDestPos;
    ULong   sDiff;
#if defined(VC_WIN32) || defined(VC_WIN64)
    __int64 sOffs;
#else
    ULong   sI;
    ULong   sTo;
    long    sOffs;
#endif

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    /* LOB ������ �������� �ʴ� ���:
     * InitLOBProc()�� lob_file_size ��� �κп�����
     * LOB ���� ���⿡ �����ϴ���
     * mLOBFileSizeOpt�� 0���� �� ä ���� �����ϵ��� �Ǿ� �ִ�.
     * �̷��� �� ������ InitLOBProc()������
     * use_separate_files ���θ� �� �� ���� �����̴�.
     * �׷���, �� ���α��� ���α׷��� ����� ����������
     * use_separate_files=no���� �˰� �ִ� �����̴�.
     * ����, �� ���ο��� mLOBFileSizeOpt�� 0�� ���
     * ���� ���� ���� ������ ����ؾ� �Ѵ�. */
    if (mLOBFileSizeOpt == ID_ULONG(0))
    {
        /* 1�� LOB ���� ���� �õ�.
         * ���� ����� ������ �����ϰ� �Ǿ� �ִ�.
         * ���� ���Ⱑ ������ �� �˰� ���� ���⸦ �õ��ϴ� ������
         * ���� ���⸦ �����ߴٴ� ���� �޽��� ����� ���ؼ���. */
        (void)OpenLOBFile( sHandle, ILO_FALSE, 1, 0, ILO_TRUE);
        IDE_TEST(1);
    }

    sDestFileNo = (UInt)(aAccumDestPos / mLOBFileSizeOpt) + 1;
    sDestPos = aAccumDestPos % mLOBFileSizeOpt;

    if (sDestFileNo == mLOBFileNo && sDestPos == mLOBFilePos)
    {
        IDE_TEST_RAISE(mLOBFP == NULL, LOBFileIOError);
        mAccumLOBFilePos = aAccumDestPos;
        return IDE_SUCCESS;
    }

    if (sDestFileNo == mLOBFileNo)
    {
        IDE_TEST_RAISE(mLOBFP == NULL, LOBFileIOError);
    }
    /* ���ο� ��ġ�� ���� �����ִ� LOB ������ �ƴ�
     * �ٸ� LOB ���Ͽ� ��ġ�ϴ� ��� */
    else
    {
        IDE_TEST(CloseLOBFile(sHandle) != IDE_SUCCESS);
        /* �̵��� ��ǥ������ ��ġ�ϴ� LOB ������ ����. */
        IDE_TEST(OpenLOBFile(sHandle, ILO_FALSE, sDestFileNo, 0, ILO_TRUE)
                 != IDE_SUCCESS);
    }

    if (sDestPos > mLOBFilePos)
    {
        sDiff = sDestPos - mLOBFilePos;
    }
    else
    {
        sDiff = mLOBFilePos - sDestPos;
    }

#if defined(VC_WIN32) || defined(VC_WIN64)
    sOffs = (sDestPos > mLOBFilePos)? (__int64)sDiff : -(__int64)sDiff;
    IDE_TEST_RAISE(::_fseeki64(mLOBFP, sOffs, SEEK_CUR) < 0, LOBFileIOError);
#else
    if (ID_SIZEOF(long) >= 8 || sDiff <= ID_ULONG(0x7FFFFFFF))
    {
        sOffs = (sDestPos > mLOBFilePos)? (long)sDiff : -(long)sDiff;
        IDE_TEST_RAISE(idlOS::fseek(mLOBFP, sOffs, SEEK_CUR) < 0,
                       LOBFileIOError);
    }
    /* long�� 4����Ʈ�̰� �̵����� long�� ������ ��� ���,
     * ���� ���� fseek()�� �ʿ��ϴ�.
     *
     * BUGBUG :
     * ��κ��� ��� fseek()����
     * ������ ��ġ�� LONG_MAX�� �Ѿ�� �� �� ������,
     * fseek()�� �̷��� �̵��� ������� ���� ä ������ ������ ���̴�. */
    else
    {
        sTo = sDiff / ID_ULONG(0x7FFFFFFF);
        sOffs = (sDestPos > mLOBFilePos)? 0x7FFFFFFF : -0x7FFFFFFF;
        for (sI = ID_ULONG(0); sI < sTo; sI++)
        {
            IDE_TEST_RAISE(idlOS::fseek(mLOBFP, sOffs, SEEK_CUR) < 0,
                           LOBFileIOError);
        }
        if (sDestPos > mLOBFilePos)
        {
            sOffs = (long)(sDiff % ID_ULONG(0x7FFFFFFF));
        }
        else
        {
            sOffs = -(long)(sDiff % ID_ULONG(0x7FFFFFFF));
        }
        if (sOffs != 0)
        {
            IDE_TEST_RAISE(idlOS::fseek(mLOBFP, sOffs, SEEK_CUR) < 0,
                           LOBFileIOError);
        }
    }
#endif /* defined(VC_WIN32) || defined(VC_WIN64) */

    mLOBFilePos = sDestPos;
    mAccumLOBFilePos = aAccumDestPos;

    return IDE_SUCCESS;

    IDE_EXCEPTION(LOBFileIOError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_LOB_File_IO_Error);
        utePrintfErrorCode(stdout,  sHandle->mErrorMgr);
        (void)idlOS::printf("LOB file seek fail\n");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* PROJ-1714
 * Ư�� Size��ŭ File���� ����
 */
// BUG-18803 readsize �ɼ� �߰�
// getc -> fread �Լ��� ����
SInt iloDataFile::ReadFromFile(SInt aSize, SChar* aResult)
{
    UInt    sResultSize = 0;

    sResultSize = idlOS::fread(aResult, 1, aSize, m_DataFp);
    mDataFilePos += sResultSize;
    aResult[sResultSize] = '\0';

    return sResultSize;
}

/* PROJ-1714
 * �Է� ���� �����͸� Size ��ŭ ���� ���ۿ� �����Ѵ�.
 */

SInt iloDataFile::WriteDataToCBuff( ALTIBASE_ILOADER_HANDLE aHandle,
                                    SChar* pBuf,
                                    SInt nSize)
{
    SInt sWriteResult;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    sWriteResult = mCirBuf.WriteBuf( sHandle, pBuf, nSize );
    
    return sWriteResult;
}

/* BUG-22434
 * �������ۿ� �����͸� ���� �� Double Buffer�� ���� �ִ´�.
 * ���� �������� Parsing������ Double Buffer�� �̿��ϰ�,
 * ���� ���۴� File I/O�� ó���ϰ� �ȴ�. 
 */
SInt iloDataFile::ReadDataFromCBuff( ALTIBASE_ILOADER_HANDLE aHandle, SChar* aResult )
{
    SInt  sRet       = 0;
    SInt  sReadSize  = 1;
        
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    //Double Buffer�� ����ų�, �� �о��� ���.
    if( (mDoubleBuffPos == -1) || (mDoubleBuffPos == mDoubleBuffSize) )
    {
        // BUG-18803 readsize �ɼ� �߰�
        sRet = mCirBuf.ReadBuf( sHandle, 
                                mDoubleBuff,
                                sHandle->mProgOption->mReadSzie);
        
        if ( sRet == -1 ) //EOF �� ���
        {
            return -1;
        }
        
        mDoubleBuffSize  = sRet;    //Buffer Data Size      
        mDoubleBuffPos   = 0;       //Buffer Position �ʱ�ȭ
    }
    
    idlOS::memcpy(aResult, &mDoubleBuff[mDoubleBuffPos] , sReadSize);
    mDoubleBuffPos++;
    
    return sReadSize;
}

/* BUG-24358 iloader Geometry Data */
IDE_RC iloDataFile::ResizeTokenLen( ALTIBASE_ILOADER_HANDLE aHandle, UInt aLen )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (mTokenMaxSize < aLen)
    {
        if (m_TokenValue != NULL)
        {
            idlOS::free(m_TokenValue);
        }
        m_TokenValue = (SChar*) idlOS::malloc(aLen);
        IDE_TEST_RAISE(m_TokenValue == NULL, MAllocError);
        m_TokenValue[0] = '\0';
        mTokenMaxSize = aLen;

        // BUG-27633: mErrorToken�� �÷��� �ִ� ũ��� �Ҵ�
        if (mErrorToken != NULL)
        {
            idlOS::free(mErrorToken);
        }
        mErrorToken = (SChar*) idlOS::malloc(aLen);
        IDE_TEST_RAISE(mErrorToken == NULL, MAllocError);
        mErrorToken[0] = '\0';
        mErrorTokenMaxSize = aLen;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(MAllocError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-24583 LOB FilePath+FileName ���� �Ҵ� */
IDE_RC iloDataFile::LOBFileInfoAlloc( ALTIBASE_ILOADER_HANDLE aHandle, 
                                      UInt                    aRowCount )
{
    SInt sI;

    iloaderHandle *sHandle = (iloaderHandle *) aHandle;

    if ( aRowCount != 0 )
    {
        LOBFileInfoFree();

        mLOBFile = (SChar**)idlOS::calloc(aRowCount, ID_SIZEOF(SChar*));
        IDE_TEST_RAISE(mLOBFile == NULL, MAllocError);
        mLOBFileRowCount = aRowCount;

        for (sI = 0; sI < mLOBFileRowCount; sI++)
        {
            mLOBFile[sI] = (SChar*)idlOS::malloc(MAX_FILEPATH_LEN * ID_SIZEOF(SChar));
            IDE_TEST_RAISE(mLOBFile[sI] == NULL, MAllocError);

            mLOBFile[sI][0] = '\0';
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(MAllocError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_memory_error, __FILE__, __LINE__);
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
        }
    }
    IDE_EXCEPTION_END;

    if (mLOBFile != NULL)
    {
        LOBFileInfoFree();
    }

    return IDE_FAILURE;
}

void iloDataFile::LOBFileInfoFree()
{
    SInt sI;

    if (mLOBFile != NULL)
    {
        for (sI = 0; sI < mLOBFileRowCount; sI++)
        {
            if (mLOBFile[sI] != NULL)
            {
                idlOS::free(mLOBFile[sI]);
                mLOBFile[sI] = NULL;
            }
        }

        idlOS::free(mLOBFile);
        mLOBFile = NULL;
        mLOBFileRowCount = 0;
    }
}

/**
 * loadFromOutFile.  //PROJ-2030, CT_CASE-3020 CHAR outfile ����
 *
 * Fmtȭ���� CHAR, VARCHAR, NCHAR, NVARCHAR �÷��� outfile �ɼ��� ������ ��� ȣ��ȴ�.
 * m_TokenValue�� ȭ���̸��� �ִ� ���¿���, m_TokenValue�� ȭ���� ������ ä���.
 */
IDE_RC iloDataFile::loadFromOutFile( ALTIBASE_ILOADER_HANDLE aHandle )
{
    iloaderHandle *sHandle = (iloaderHandle *) aHandle;
    
    if (mOutFileFP != NULL)
    {
        IDE_TEST_RAISE(idlOS::fclose(mOutFileFP) != 0, CloseError);
        mOutFileFP = NULL;
    }

    /* BUG-47652 Set file permission */
    mOutFileFP = ilo_fopen( m_TokenValue, "rb", sHandle->mProgOption->IsExistFilePerm() );
    IDE_TEST_RAISE(mOutFileFP == NULL, OpenError);

    mTokenLen = (UInt)idlOS::fread(m_TokenValue, 1, mTokenMaxSize, mOutFileFP);
    IDE_TEST_RAISE(ferror(mOutFileFP), ReadError);

    IDE_TEST_RAISE(idlOS::fclose(mOutFileFP) != 0, CloseError);
    mOutFileFP = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION(CloseError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_OutFile_IO_Error);
        
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            (void)idlOS::printf("Outfile close fail\n");
        }
    }
    IDE_EXCEPTION(OpenError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_OutFile_IO_Error);
    
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout, sHandle->mErrorMgr);
            idlOS::printf("Outfile [%s] open fail\n", m_TokenValue);
        }
    }
    IDE_EXCEPTION(ReadError);
    {
        uteSetErrorCode( sHandle->mErrorMgr, utERR_ABORT_OutFile_IO_Error);
    
        if ( sHandle->mUseApi != SQL_TRUE )
        {
            utePrintfErrorCode(stdout,  sHandle->mErrorMgr);
            (void)idlOS::printf("Outfile data read fail\n");
        }
        
        idlOS::fclose(mOutFileFP);
        mTokenLen = 0;
        m_TokenValue[0] = '\0';
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

