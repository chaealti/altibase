/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <uln.h>
#include <ulnPrivate.h>
#include <ulnLob.h>
#include <ulnConvChar.h>
#include <ulnConvWChar.h>

/*
 * ========================================
 *
 * ULN LOB BUFFER
 *
 * ========================================
 */

/*
 * -------------------------
 * ulnLobBuffer : Initialize
 * -------------------------
 */

static ACI_RC ulnLobBufferInitializeFILE(ulnLobBuffer *aLobBuffer,
                                         acp_uint8_t  *aFileNamePtr,
                                         acp_uint32_t  aFileOption)
{
    aLobBuffer->mObject.mFile.mFileName           = (acp_char_t *)aFileNamePtr;
    aLobBuffer->mObject.mFile.mFileOption         = aFileOption;
    aLobBuffer->mObject.mFile.mFileHandle.mHandle = ULN_INVALID_HANDLE;
    aLobBuffer->mObject.mFile.mFileSize           = ACP_SINT64_LITERAL(0);
    aLobBuffer->mObject.mFile.mTempBuffer         = NULL;

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferInitializeMEMORY(ulnLobBuffer *aLobBuffer,
                                           acp_uint8_t  *aBufferPtr,
                                           acp_uint32_t  aBufferSize)
{
    aLobBuffer->mObject.mMemory.mBuffer        = aBufferPtr;
    aLobBuffer->mObject.mMemory.mBufferSize    = aBufferSize;
    aLobBuffer->mObject.mMemory.mCurrentOffset = 0;

    return ACI_SUCCESS;
}

/*
 * ----------------------
 * ulnLobBuffer : Prepare
 * ----------------------
 */

static ACI_RC ulnLobBufferPrepareMEMORY(ulnFnContext *aFnContext,
                                        ulnLobBuffer *aLobBuffer)
{
    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aLobBuffer);

    /*
     * �޸� ���۴� �ݵ�� ������� ���ε�޸��̱� ������ �ƹ��͵� �ؼ��� �ȵȴ�.
     */

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferPrepareFILE(ulnFnContext *aFnContext, ulnLobBuffer *aLobBuffer)
{
    ULN_FLAG(sNeedCloseFile);

    acp_offset_t  sFileSize   = 0;
    acp_file_t   *sFileHandle = &aLobBuffer->mObject.mFile.mFileHandle;
    acp_stat_t    sStat;

    /*
     * ���� ����
     */
    switch (aLobBuffer->mObject.mFile.mFileOption)
    {
        case SQL_FILE_READ:
            ACI_TEST_RAISE(acpFileOpen(sFileHandle,
                                       aLobBuffer->mObject.mFile.mFileName,
                                       ACP_O_RDONLY,
                                       0) != ACP_RC_SUCCESS,
                           LABEL_FILE_OPEN_ERR);

            ULN_FLAG_UP(sNeedCloseFile);

            /*
             * ���� ������ ���
             */
            ACI_TEST_RAISE(acpFileStat(sFileHandle, &sStat) != ACP_RC_SUCCESS,
                           LABEL_GET_FILESIZE_ERR);

            sFileSize = sStat.mSize;

            /* BUG-47416 LOB �ִ������� 4,294,967,295 bytes (4GB-1byte), 32bit ȯ���� ���� ĳ���� ����. */
            ACI_TEST_RAISE((acp_uint64_t)sFileSize > (acp_uint64_t)ACP_UINT32_MAX, LABEL_FILESIZE_TOO_BIG);
            aLobBuffer->mObject.mFile.mFileSize = sFileSize;

            /*
             * ���Ͽ������� lob �����͸� �о ������ ������ �ӽ÷� �� �޸𸮰� �ʿ��ϴ�.
             * ���Ͽ� ������ lob �����͸� �� ��쿡�� �ӽ� �޸𸮰� �ʿ� ����.
             */
            ACI_TEST_RAISE(aLobBuffer->mObject.mFile.mTempBuffer != NULL, LABEL_MEM_MAN_ERR);

            break;

        case SQL_FILE_CREATE:
            /*
             * Access Mode : 0600 :
             */
            ACI_TEST_RAISE(
                acpFileOpen(sFileHandle,
                            aLobBuffer->mObject.mFile.mFileName,
                            ACP_O_WRONLY | ACP_O_CREAT | ACP_O_EXCL,
                            ACP_S_IRUSR | ACP_S_IWUSR) != ACP_RC_SUCCESS,
                LABEL_FILE_OPEN_ERR_EXIST);

            ULN_FLAG_UP(sNeedCloseFile);

            break;

        case SQL_FILE_OVERWRITE:
            /*
             * BUGBUG : O_CREAT �� O_EXCL �� �Բ� ��õǾ� ���� ����� open�� ������ �����ϴ���
             *          �׽�Ʈ�� �ϰ� ������ �����ϴ� �ΰ��� ������ atomic �ϰ� ó���ϰ� �ȴ�.
             *          �׷��� ������ �� �� �ɼ��� �Բ� �����Ǿ� ������ ������ ������ ���
             *          ������ ���ϵȴ�.
             *
             *          �׷���, �Ʒ��� �Լ����� O_EXCL �� �߰����ڴ�, �����ϴ� ������ truncate
             *          ��Ű�� ���� ���� �� �ȵǰ�(������ ��), O_EXCL �� ���ڴ� atomic ��
             *          ������ ���� �ʾƼ� �����̴�.
             *
             *          �ϴ��� ���� ���翩�� �׽�Ʈ���� �������� �ʴ°��� �Ǵ��ϰ� �� ���̿�
             *          �ٸ� ���ΰԽ��� ������ ���� �̸����� ����� ������ ��Ȳ�� �߻���
             *          ���ɼ��� 0 �� �����Ѵٰ� ���� �׳� O_EXCL ���� �������ȴ�.
             */
            ACI_TEST_RAISE(
                acpFileOpen(sFileHandle,
                            aLobBuffer->mObject.mFile.mFileName,
                            ACP_O_WRONLY | ACP_O_TRUNC | ACP_O_CREAT,
                            ACP_S_IRUSR | ACP_S_IWUSR) != ACP_RC_SUCCESS,
                LABEL_FILE_OPEN_ERR);

            ULN_FLAG_UP(sNeedCloseFile);

            break;

        case SQL_FILE_APPEND:
            ACI_TEST_RAISE(acpFileOpen(sFileHandle,
                                       aLobBuffer->mObject.mFile.mFileName,
                                       ACP_O_WRONLY | ACP_O_APPEND,
                                       0) != ACP_RC_SUCCESS,
                           LABEL_FILE_OPEN_ERR);

            ULN_FLAG_UP(sNeedCloseFile);

            break;

        default:
            ACI_RAISE(LABEL_UNKNOWN_FILE_OPTION);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FILESIZE_TOO_BIG)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_ABORT_FILE_SIZE_TOO_BIG,
                         aLobBuffer->mObject.mFile.mFileName);
    }

    ACI_EXCEPTION(LABEL_UNKNOWN_FILE_OPTION)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_ABORT_UNKNOWN_FILE_OPTION,
                         aLobBuffer->mObject.mFile.mFileOption);
    }

    ACI_EXCEPTION(LABEL_FILE_OPEN_ERR)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_ABORT_FILE_OPEN,
                         aLobBuffer->mObject.mFile.mFileName);
    }

    ACI_EXCEPTION(LABEL_FILE_OPEN_ERR_EXIST)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_ABORT_FILE_EXIST,
                         aLobBuffer->mObject.mFile.mFileName);
    }

    ACI_EXCEPTION(LABEL_GET_FILESIZE_ERR)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_ABORT_GET_FILE_SIZE,
                         aLobBuffer->mObject.mFile.mFileName);
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnErrorExtended(aFnContext,
                         SQL_ROW_NUMBER_UNKNOWN,
                         SQL_COLUMN_NUMBER_UNKNOWN,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulnLobBufferPrepareFILE");
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedCloseFile)
    {
        /*
         * ���� �ݱ�
         */
        if (acpFileClose(sFileHandle) != ACP_RC_SUCCESS)
        {
            ulnErrorExtended(aFnContext,
                             SQL_ROW_NUMBER_UNKNOWN,
                             SQL_COLUMN_NUMBER_UNKNOWN,
                             ulERR_ABORT_FILE_CLOSE,
                             aLobBuffer->mObject.mFile.mFileName);
        }
    }
    return ACI_FAILURE;
}

/*
 * --------------------------------------------------
 * ulnLobBuffer : Data In to local buffer from server
 * --------------------------------------------------
 */

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
static ACI_RC ulnLobBufferDataInDumpAsCHAR(cmtVariable  *aCmVariable,
                                           acp_uint32_t  aOffset,
                                           acp_uint32_t  aReceivedDataSize,
                                           acp_uint8_t  *aReceivedData,
                                           void         *aContext)
{
    /*
     * �� �Լ��� ulncBLOB_CHAR ���� ȣ��ȴ�.
     * ��ȯ�� �Ѵ�. �ʿ��� ������ ũ��� ������ ������ũ�� * 2 + 1 �̴�.
     */

    ulnFnContext *sFnContext = (ulnFnContext *)aContext;
    ulnLob       *sLob       = (ulnLob *)sFnContext->mArgs;
    ulnLobBuffer *sLobBuffer = sLob->mBuffer;

    acp_uint32_t          sLengthConverted;

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    /*
     * Note : ����!!!!! mBuffer �� NULL �� ���� �ִ�.
     */

    sLengthConverted
        = ulnConvDumpAsChar(sLobBuffer->mObject.mMemory.mBuffer + (sLob->mSizeRetrieved * 2),
                            sLobBuffer->mObject.mMemory.mBufferSize - (sLob->mSizeRetrieved * 2),
                            aReceivedData,
                            aReceivedDataSize);

    /*
     * ���� �Լ� ulnConvDumpAsChar �� �����ϴ� ���� ������ ¦���̸�,
     * �׻� null termination �� ���� ������ ���� �� ���̸� �����Ѵ�.
     *
     * ��, 
     *
     *      *(destination buffer + sLengthConverted) = 0;
     *
     *  �� ���� �� �͹̳��̼��� �����ϰ� �� �� �ִ�.
     *
     * �׸���, sLob �� mSizeRetrieved �� ������ ������ �������� ���ŵ� �������� ����Ʈ���� ����.
     */

    sLob->mSizeRetrieved += sLengthConverted / 2;

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferDataInDumpAsWCHAR(cmtVariable  *aCmVariable,
                                            acp_uint32_t  aOffset,
                                            acp_uint32_t  aReceivedDataSize,
                                            acp_uint8_t  *aReceivedData,
                                            void         *aContext)
{
    /*
     * �� �Լ��� ulncBLOB_WCHAR ���� ȣ��ȴ�.
     * ��ȯ�� �Ѵ�. �ʿ��� ������ ũ��� ������ ������ũ�� * 4 + 1 �̴�.
     */

    ulnFnContext *sFnContext = (ulnFnContext *)aContext;
    ulnLob       *sLob       = (ulnLob *)sFnContext->mArgs;
    ulnLobBuffer *sLobBuffer = sLob->mBuffer;

    acp_uint32_t          sLengthConverted;

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    /*
     * Note : ����!!!!! mBuffer �� NULL �� ���� �ִ�.
     */

    sLengthConverted
        = ulnConvDumpAsWChar(sLobBuffer->mObject.mMemory.mBuffer + (sLob->mSizeRetrieved * 2),
                             sLobBuffer->mObject.mMemory.mBufferSize - (sLob->mSizeRetrieved * 2),
                             aReceivedData,
                             aReceivedDataSize);

    /*
     * ���� �Լ� ulnConvDumpAsChar �� �����ϴ� ���� ������ ¦���̸�,
     * �׻� null termination �� ���� ������ ���� �� ���̸� �����Ѵ�.
     *
     * ��,
     *
     *      *(destination buffer + sLengthConverted) = 0;
     *
     *  �� ���� �� �͹̳��̼��� �����ϰ� �� �� �ִ�.
     *
     * �׸���, sLob �� mSizeRetrieved �� ������ ������ �������� ���ŵ� �������� ����Ʈ���� ����.
     */

    sLob->mSizeRetrieved += (sLengthConverted / ACI_SIZEOF(ulWChar)) / 2;

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferDataInBINARY(cmtVariable  *aCmVariable,
                                       acp_uint32_t  aOffset,
                                       acp_uint32_t  aReceivedDataSize,
                                       acp_uint8_t  *aReceivedData,
                                       void         *aContext)
{
    /*
     * �� �Լ��� ulncBLOB_BINARY, ulnc_CLOB_BINARY, ulnc_CLOB_CHAR ���� ȣ��ȴ�.
     * ��ȯ ���� �״�� �����Ѵ�.
     */

    ulnFnContext *sFnContext = (ulnFnContext *)aContext;
    ulnLob       *sLob       = (ulnLob *)sFnContext->mArgs;
    ulnLobBuffer *sLobBuffer = sLob->mBuffer;

    acp_uint32_t  sBufferSize;
    acp_uint8_t  *sTarget;
    acp_uint32_t  sSizeToCopy;

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    sBufferSize = sLobBuffer->mOp->mGetSize(sLobBuffer);

    if (sLob->mSizeRetrieved + aReceivedDataSize > sBufferSize)
    {
        /*
         * ���ŵ� �����Ͱ� ����� ���� ��踦 ������.
         *
         * 01004 �� conversion �Լ��� ȣ���� �Լ�����
         * length needed, length written ������ ������ Ÿ���� ���� �����Ѵ�.
         */
        sSizeToCopy = sBufferSize - sLob->mSizeRetrieved;
    }
    else
    {
        /*
         * ���ŵ� �����Ͱ� ����� ���� ��� ���ʿ� �� �� �ִ� ũ���̴�.
         */
        sSizeToCopy = aReceivedDataSize;
    }

    if (sSizeToCopy > 0)
    {
        /*
         * Note : ����!!!!! mBuffer �� NULL �� ���� �ִ�.
         */
        sTarget = sLob->mBuffer->mObject.mMemory.mBuffer + sLob->mSizeRetrieved;

        acpMemCpy(sTarget, aReceivedData, sSizeToCopy);

        sLob->mSizeRetrieved += sSizeToCopy;
    }

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferDataInFILE(cmtVariable * aCmVariable,
                                     acp_uint32_t  aOffset,
                                     acp_uint32_t  aReceivedDataSize,
                                     acp_uint8_t  *aReceivedData,
                                     void         *aContext)
{
    acp_size_t    sWrittenSize = 0;
    ulnFnContext *sFnContext   = (ulnFnContext *)aContext;
    ulnLob       *sLob         = (ulnLob *)sFnContext->mArgs;

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    ACI_TEST_RAISE(sLob->mBuffer->mObject.mFile.mFileHandle.mHandle == 
                   ULN_INVALID_HANDLE,
                   LABEL_MEM_MAN_ERR);

    ACI_TEST_RAISE(acpFileWrite(&sLob->mBuffer->mObject.mFile.mFileHandle,
                                aReceivedData,
                                aReceivedDataSize,
                                &sWrittenSize) != ACP_RC_SUCCESS,
                   LABEL_FILE_WRITE_ERR);

    ACI_TEST_RAISE(sWrittenSize != (acp_size_t)aReceivedDataSize, LABEL_FILE_WRITE_ERR);

    sLob->mSizeRetrieved += aReceivedDataSize;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FILE_WRITE_ERR)
    {
        ulnError(sFnContext, ulERR_ABORT_LOB_FILE_WRITE_ERR);
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnLobBufferFileFinalize");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
static ACI_RC ulnLobBufferDataInCHAR(cmtVariable  *aCmVariable,
                                     acp_uint32_t  aOffset,
                                     acp_uint32_t  aReceivedDataSize,
                                     acp_uint8_t  *aReceivedData,
                                     void         *aContext)
{
    ulnFnContext *sFnContext = (ulnFnContext *)aContext;
    ulnLob       *sLob       = (ulnLob *)sFnContext->mArgs;
    ulnLobBuffer *sLobBuffer = sLob->mBuffer;

    ulnDbc       *sDbc = NULL;
    acp_uint32_t *sAppBufferOffset = &sLobBuffer->mObject.mMemory.mCurrentOffset;
    acp_uint8_t  *sAppBuffer       = sLobBuffer->mObject.mMemory.mBuffer +
                                     sLobBuffer->mObject.mMemory.mCurrentOffset;
    acp_uint8_t  *sAppBufferCurPtr     = sAppBuffer;
    acp_uint32_t  sAppBufferRemainSize = sLobBuffer->mObject.mMemory.mBufferSize -
                                         sLobBuffer->mObject.mMemory.mCurrentOffset;

    acp_uint8_t   sBuffer[ULN_MAX_CHARSIZE] = {0, };
    acp_uint8_t   sBufferOffset = 0;

    acp_uint32_t  sSrcMaxPrecision  = 0;
    acp_uint32_t  sDestMaxPrecision = 0;
    acp_sint32_t  sRemainSize       = 0;

    mtlModule          *sSrcCharSet  = NULL;
    mtlModule          *sDestCharSet = NULL;
    aciConvCharSetList  sSrcCharSetIndex;
    aciConvCharSetList  sDestCharSetIndex;
    ACI_RC              sRC = ACI_FAILURE;

    ulnCharSet   *sCharSet = &sLobBuffer->mCharSet;

    /* ������ ���ŵ� ������ ���κ� + ���� ���ŵ� �������� ���� ����Ʈ */
    acp_uint8_t  *sMergeDataPrePtr = NULL;
    acp_uint8_t  *sMergeDataPtr    = aReceivedData - sCharSet->mRemainSrcLen;
    acp_uint8_t  *sMergeDataCurPtr = aReceivedData - sCharSet->mRemainSrcLen;
    acp_uint32_t  sMergeDataRemainSize = aReceivedDataSize +
                                         sCharSet->mRemainSrcLen;
    acp_uint8_t  *sMergeDataFencePtr  = sMergeDataCurPtr + sMergeDataRemainSize;

    sLob->mGetSize -= aReceivedDataSize;

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    sSrcCharSet       = sDbc->mCharsetLangModule;
    sDestCharSet      = sDbc->mClientCharsetLangModule;
    sSrcCharSetIndex  = mtlGetIdnCharSet(sSrcCharSet);
    sDestCharSetIndex = mtlGetIdnCharSet(sDestCharSet);
    sSrcMaxPrecision  = sSrcCharSet->maxPrecision(1);
    sDestMaxPrecision = sDestCharSet->maxPrecision(1);

    /* 
     * Partial Converting
     *
     * ������ ���ŵ� ������ ���κа� ���� ���ŵ� �����͸� ���ļ� ���ڵ� �Ѵ�.
     * ���� ������ ���κ��� ���� ���ŵ� �������� ��� ������ ����Ǹ�
     * �̹� ����� �������� ������ �Ѽ��� �Ǿ ������ ����.
     *
     * ����� ������ ���ŵ� ������ ���κ��� 4byte �����̰� ��� ������� 16byte�̴�.
     */
    if (sCharSet->mRemainSrcLen > 0)
    {
        acpMemCpy(sMergeDataCurPtr, 
                  sCharSet->mRemainSrc,
                  sCharSet->mRemainSrcLen);
        sCharSet->mRemainSrcLen = 0;
    }
    else
    {
        /* Nothing */
    }

    ACI_TEST_RAISE(sAppBufferRemainSize == 0, NO_NEED_WORK);
    ACI_TEST_RAISE(sAppBufferRemainSize < sDestMaxPrecision,
                   LABEL_MAY_NOT_ENOUGH_APP_BUFFER)

    while (sMergeDataCurPtr < sMergeDataFencePtr &&
           sAppBufferRemainSize > 0)
    {
        sRemainSize = sDestMaxPrecision;

        ACI_TEST_RAISE(aciConvConvertCharSet(sSrcCharSetIndex,
                                             sDestCharSetIndex,
                                             sMergeDataCurPtr,
                                             sMergeDataRemainSize,
                                             sAppBufferCurPtr,
                                             &sRemainSize,
                                             -1)
                       != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);

        sMergeDataPrePtr = sMergeDataCurPtr;
        (void)sSrcCharSet->nextCharPtr(&sMergeDataCurPtr, sMergeDataFencePtr);
        sMergeDataRemainSize -= (sMergeDataCurPtr - sMergeDataPrePtr);

        sAppBufferCurPtr     += (sDestMaxPrecision - sRemainSize);
        sAppBufferRemainSize -= (sDestMaxPrecision - sRemainSize);

        /* Partial�� ��� */
        if (sLob->mGetSize != 0 && sMergeDataRemainSize < sSrcMaxPrecision)
        {
            sCharSet->mRemainSrcLen = sMergeDataRemainSize;
            acpMemCpy(sCharSet->mRemainSrc,
                      sMergeDataCurPtr,
                      sMergeDataRemainSize);
            break;
        }
        else if (sAppBufferRemainSize < sDestMaxPrecision)
        {
            ACI_RAISE(LABEL_MAY_NOT_ENOUGH_APP_BUFFER);
        }
        else
        {
            /* Nothing */
        }
    } /* while (sMergeDataCurPtr < sMergeDataFencePtr... */

    sLob->mSizeRetrieved    += (sMergeDataCurPtr - sMergeDataPtr);
    sCharSet->mConvedSrcLen += (sMergeDataCurPtr - sMergeDataPtr);

    sCharSet->mCopiedDesLen += (sAppBufferCurPtr - sAppBuffer);
    sCharSet->mDestLen      += (sAppBufferCurPtr - sAppBuffer);
    *(sAppBufferOffset)     += (sAppBufferCurPtr - sAppBuffer);

    ACI_EXCEPTION_CONT(NO_NEED_WORK);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(sFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_MAY_NOT_ENOUGH_APP_BUFFER)
    {
        /* 
         * mDestLen���� �߸� ������ ��ü ��������� �����ؾ� �Ѵ�.
         * aLength->mNeeded�� �� ���� ����ȴ�.
         */
        sCharSet->mDestLen += (sAppBufferCurPtr - sAppBuffer);

        while (sMergeDataCurPtr < sMergeDataFencePtr)
        {
            sRemainSize = sDestMaxPrecision;

            ACI_TEST_RAISE(aciConvConvertCharSet(sSrcCharSetIndex,
                                                 sDestCharSetIndex,
                                                 sMergeDataCurPtr,
                                                 sMergeDataRemainSize,
                                                 sBuffer + sBufferOffset,
                                                 &sRemainSize,
                                                 -1)
                           != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED_SBUFFER);

            sMergeDataPrePtr = sMergeDataCurPtr;
            (void)sSrcCharSet->nextCharPtr(&sMergeDataCurPtr, sMergeDataFencePtr);
            sMergeDataRemainSize -= (sMergeDataCurPtr - sMergeDataPrePtr);

            sBufferOffset += (sDestMaxPrecision - sRemainSize);

            if (sBufferOffset > sAppBufferRemainSize)
            {
                break;
            }
            else
            {
                /* Nothing */
            }
        }

        /* 
         * "��" KSC5601(B6 F8), UTF-8(EB 9E 8D)
         *
         * App Buffer�� 2����Ʈ ���� ��� EB 9E�� App Buffer�� ����ǰ�
         * 8D�� mRemainTextLen�� ���� �Ǿ� ������ ����ڰ� LOB �����͸�
         * ��û�� �� ������ �ش�. Binding�� �ϰų� SQLGetData()�� ����
         * ��� ���ڰ� �߷��� �����͸� ���ۿ� �� ä����� �Ѵ�.
         *
         * GDPosition�� "��" ���� ���ڸ� ����Ų��. 
         */
        if (sBufferOffset > sAppBufferRemainSize)
        {
            sCharSet->mRemainTextLen = sBufferOffset -
                                       sAppBufferRemainSize;
            acpMemCpy(sCharSet->mRemainText,
                      sBuffer + sAppBufferRemainSize,
                      sCharSet->mRemainTextLen);

            acpMemCpy(sAppBufferCurPtr,
                      sBuffer,
                      sAppBufferRemainSize);

            sAppBufferCurPtr += sAppBufferRemainSize;
        }
        else
        {
            acpMemCpy(sAppBufferCurPtr,
                      sBuffer,
                      sBufferOffset);

            sAppBufferCurPtr += sBufferOffset;
        }

        sCharSet->mDestLen      += sBufferOffset;
        sLob->mSizeRetrieved    += (sMergeDataCurPtr - sMergeDataPtr);
        sCharSet->mConvedSrcLen += (sMergeDataCurPtr - sMergeDataPtr);

        sCharSet->mCopiedDesLen += (sAppBufferCurPtr - sAppBuffer);
        *(sAppBufferOffset)     += (sAppBufferCurPtr - sAppBuffer);

        sRC = ACI_SUCCESS;
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED)
    {
        ulnError(sFnContext,
                 ulERR_ABORT_CONVERT_CHARSET_FAILED,
                 sSrcCharSet,
                 sDestCharSet,
                 sMergeDataFencePtr - sMergeDataCurPtr,
                 sAppBufferCurPtr - sLobBuffer->mObject.mMemory.mBuffer);
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED_SBUFFER)
    {
        ulnError(sFnContext,
                 ulERR_ABORT_CONVERT_CHARSET_FAILED,
                 sSrcCharSet,
                 sDestCharSet,
                 sMergeDataFencePtr - sMergeDataCurPtr,
                 -1);
    }

    ACI_EXCEPTION_END;

    return sRC;
}

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
static ACI_RC ulnLobBufferDataInWCHAR(cmtVariable  *aCmVariable,
                                      acp_uint32_t  aOffset,
                                      acp_uint32_t  aReceivedDataSize,
                                      acp_uint8_t  *aReceivedData,
                                      void         *aContext)
{
    ulnFnContext *sFnContext = (ulnFnContext *)aContext;
    ulnLob       *sLob       = (ulnLob *)sFnContext->mArgs;
    ulnLobBuffer *sLobBuffer = sLob->mBuffer;

    ulnDbc       *sDbc = NULL;
    acp_uint32_t *sAppBufferOffset = &sLobBuffer->mObject.mMemory.mCurrentOffset;
    acp_uint8_t  *sAppBuffer       = sLobBuffer->mObject.mMemory.mBuffer + 
                                     sLobBuffer->mObject.mMemory.mCurrentOffset;
    acp_uint8_t  *sAppBufferCurPtr = sAppBuffer;
    acp_uint32_t  sAppBufferRemainSize = sLobBuffer->mObject.mMemory.mBufferSize -
                                         sLobBuffer->mObject.mMemory.mCurrentOffset;

    acp_uint32_t  sSrcMaxPrecision  = 0;
    acp_uint32_t  sDestMaxPrecision = 0;
    acp_sint32_t  sRemainSize       = 0;
    acp_uint8_t   sTempChar         = '\0';

    mtlModule          *sSrcCharSet  = NULL;
    mtlModule          *sDestCharSet = NULL;
    aciConvCharSetList  sSrcCharSetIndex;
    aciConvCharSetList  sDestCharSetIndex;
    ACI_RC              sRC = ACI_FAILURE;

    ulnCharSet   *sCharSet = &sLobBuffer->mCharSet;

    /* ������ ���ŵ� ������ ���κ� + ���� ���ŵ� �������� ���� ����Ʈ */
    acp_uint8_t  *sMergeDataPrePtr = NULL;
    acp_uint8_t  *sMergeDataPtr    = aReceivedData - sCharSet->mRemainSrcLen;
    acp_uint8_t  *sMergeDataCurPtr = aReceivedData - sCharSet->mRemainSrcLen;
    acp_uint32_t  sMergeDataRemainSize = aReceivedDataSize +
                                         sCharSet->mRemainSrcLen;
    acp_uint8_t  *sMergeDataFencePtr  = sMergeDataCurPtr + sMergeDataRemainSize;

    sLob->mGetSize -= aReceivedDataSize;

    ULN_FNCONTEXT_GET_DBC(sFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACP_UNUSED(aCmVariable);
    ACP_UNUSED(aOffset);

    sSrcCharSet       = sDbc->mCharsetLangModule;
    sDestCharSet      = sDbc->mWcharCharsetLangModule;
    sSrcCharSetIndex  = mtlGetIdnCharSet(sSrcCharSet);
    sDestCharSetIndex = mtlGetIdnCharSet(sDestCharSet);
    sSrcMaxPrecision  = sSrcCharSet->maxPrecision(1);
    sDestMaxPrecision = sDestCharSet->maxPrecision(1);

    /* 
     * Partial Converting
     *
     * ������ ���ŵ� ������ ���κа� ���� ���ŵ� �����͸� ���ļ� ���ڵ� �Ѵ�.
     * ���� ������ ���κ��� ���� ���ŵ� �������� ��� ������ ����Ǹ�
     * �̹� ����� �������� ������ �Ѽ��� �Ǿ ������ ����.
     *
     * ����� ������ ���ŵ� ������ ���κ��� 4byte �����̰� ��� ������� 16byte�̴�.
     */
    if (sCharSet->mRemainSrcLen > 0)
    {
        acpMemCpy(sMergeDataCurPtr, 
                  sCharSet->mRemainSrc,
                  sCharSet->mRemainSrcLen);
        sCharSet->mRemainSrcLen = 0;
    }
    else
    {
        /* Nothing */
    }

    ACI_TEST_RAISE(sAppBufferRemainSize == 0, NO_NEED_WORK);

    while (sMergeDataCurPtr < sMergeDataFencePtr &&
           sAppBufferRemainSize > 0)
    {
        sRemainSize = sDestMaxPrecision;

        sRC = aciConvConvertCharSet(sSrcCharSetIndex,
                                    sDestCharSetIndex,
                                    sMergeDataCurPtr,
                                    sMergeDataRemainSize,
                                    sAppBufferCurPtr,
                                    &sRemainSize,
                                    -1);

        ACI_TEST_RAISE(sRC != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);

#ifndef ENDIAN_IS_BIG_ENDIAN
        sTempChar = sAppBufferCurPtr[0];
        sAppBufferCurPtr[0] = sAppBufferCurPtr[1];
        sAppBufferCurPtr[1] = sTempChar;
#endif

        sMergeDataPrePtr = sMergeDataCurPtr;
        (void)sSrcCharSet->nextCharPtr(&sMergeDataCurPtr, sMergeDataFencePtr);

        sMergeDataRemainSize -= (sMergeDataCurPtr - sMergeDataPrePtr);
        sAppBufferCurPtr     += (sDestMaxPrecision - sRemainSize);
        sAppBufferRemainSize -= (sDestMaxPrecision - sRemainSize);

        /* Partial�� ��� */
        if (sLob->mGetSize != 0 && sMergeDataRemainSize < sSrcMaxPrecision)
        {
            sCharSet->mRemainSrcLen = sMergeDataRemainSize;
            acpMemCpy(sCharSet->mRemainSrc,
                      sMergeDataCurPtr,
                      sMergeDataRemainSize);

            break;
        }
        else
        {
            /* Nothing */
        }
    }

    sCharSet->mCopiedDesLen += (sAppBufferCurPtr - sAppBuffer);
    sCharSet->mDestLen      += (sAppBufferCurPtr - sAppBuffer);
    (*sAppBufferOffset)     += (sAppBufferCurPtr - sAppBuffer);

    sLob->mSizeRetrieved    += (sMergeDataCurPtr - sMergeDataPtr);
    sCharSet->mConvedSrcLen += (sMergeDataCurPtr - sMergeDataPtr);

    ACI_EXCEPTION_CONT(NO_NEED_WORK);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(sFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED)
    {
        ulnError(sFnContext,
                 ulERR_ABORT_CONVERT_CHARSET_FAILED,
                 sSrcCharSet,
                 sDestCharSet,
                 sMergeDataFencePtr - sMergeDataCurPtr,
                 sAppBufferCurPtr - sLobBuffer->mObject.mMemory.mBuffer);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/*
 * ---------------------------------------------------
 * ulnLobBuffer : Data Out from local buffer to server
 * ---------------------------------------------------
 */

/*
 * Note : ���ǻ���!! ���ǻ���!!!
 *
 *                     |--- cmpArgDBLobPut::mOffset = A + B
 *                     |           : DB �� ����� LOB �� ������ 0 ���� ��������
 *                     |             offset. ��, ���� offset
 *                     |
 *                     |  +-- cmpArgDBLobPut::mData
 *                     |  |
 *           |<-- B -->|<-+-->|  DATA : �ѹ��� cmpArgDBLobPut ���� ������ Data
 *           |         |      |
 *           +---------+------+-------------------+
 *           |  �� ��  | DATA |  ���� ������ ��ü | ������Ʈ �ϰ��� �ϴ� Data
 *           +---------+------+-------------------+
 * |<-- A -->|                             ______/
 * |         |                       _____/
 * |         |               _______/
 * | start   |        ______/
 * | offset->|       /
 * +---------+------+------------------------
 * | DB �� ����Ǿ� �ִ� LOB ������
 * +---------+------+------------------------
 *           |      |
 *           |<---->|
 *              Update �Ϸ��� �ϴ� ����
 */

/* PROJ-2047 Strengthening LOB - Removed aOffset */
static ACI_RC ulnLobAppendCore(ulnFnContext *aFnContext,
                               ulnPtContext *aPtContext,
                               ulnLob       *aLob,
                               acp_uint8_t  *aBuffer,
                               acp_uint32_t  aSize)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx     = &(aPtContext->mCmiPtContext);
    acp_uint64_t         sLobLocatorVal;
    acp_uint16_t         sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t          sState          = 0;

    ACE_ASSERT(aSize <= ULN_LOB_PIECE_SIZE);

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    sPacket.mOpID = CMP_OP_DB_LobPut;

    CMI_WRITE_CHECK(sCtx, 13 + aSize);
    sState = 1;

    CMI_WOP(sCtx, CMP_OP_DB_LobPut);
    CMI_WR8(sCtx, &sLobLocatorVal);
    CMI_WR4(sCtx, &aSize);          // size
    CMI_WCP(sCtx, aBuffer, aSize);

    ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket) != ACI_SUCCESS);

    /*
     * ulnLob �� ����� �߰���Ų ��ŭ ������Ų��.
     */
    aLob->mSize += aSize;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "LobAppend");
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

static ACI_RC ulnLobBufferDataOutBINARY(ulnFnContext *aFnContext,
                                        ulnPtContext *aPtContext,
                                        ulnLob       *aLob)
{
    acp_uint32_t sSourceSize          = aLob->mBuffer->mOp->mGetSize(aLob->mBuffer);
    acp_uint32_t sSourceOffsetToWrite = 0;
    acp_uint32_t sRemainingSize       = 0;
    acp_uint32_t sSizeToSend          = 0;

    while (sSourceOffsetToWrite < aLob->mBuffer->mObject.mMemory.mBufferSize)
    {
        sRemainingSize = sSourceSize - sSourceOffsetToWrite;
        sSizeToSend    = ACP_MIN(sRemainingSize, ULN_LOB_PIECE_SIZE);

        ACI_TEST(ulnLobAppendCore(aFnContext,
                                  aPtContext,
                                  aLob,
                                  aLob->mBuffer->mObject.mMemory.mBuffer + sSourceOffsetToWrite,
                                  sSizeToSend) != ACI_SUCCESS);

        sSourceOffsetToWrite += sSizeToSend;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * PROJ-2047 Strengthening LOB - Partial Converting
 *
 * ������ ������ CM ��� ���ۿ� ���� ������ ����
 */
static ACI_RC ulnLobBufferDataOutFILE(ulnFnContext *aFnContext,
                                      ulnPtContext *aPtContext,
                                      ulnLob       *aLob)
{
    cmiProtocol          sPacket;
    cmiProtocolContext  *sCtx     = &(aPtContext->mCmiPtContext);
    acp_uint64_t         sLobLocatorVal;

    acp_size_t   sFileSizeRetrieved = 0;
    acp_uint32_t sDataSize          = 0;
    acp_uint16_t sCursor            = 0;
    acp_uint16_t sOrgWriteCursor    = CMI_GET_CURSOR(sCtx);
    acp_uint8_t  sState             = 0;
    acp_rc_t     sRet;

    acp_uint8_t *sSizePtr = NULL;

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));

    do
    {
        sDataSize = ULN_LOB_PIECE_SIZE;

        sPacket.mOpID = CMP_OP_DB_LobPut;

        CMI_WRITE_CHECK(sCtx, 13 + sDataSize);
        sState = 1;
        sCursor = sCtx->mWriteBlock->mCursor;

        CMI_WOP(sCtx, CMP_OP_DB_LobPut);
        CMI_WR8(sCtx, &sLobLocatorVal);

        /* ������ ũ�� ��ġ�� ����� ���� */
        sSizePtr = sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor;
        CMI_WR4(sCtx, &sDataSize); // size

        sRet = acpFileRead(&aLob->mBuffer->mObject.mFile.mFileHandle,
                           sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor,
                           ULN_LOB_PIECE_SIZE,
                           &sFileSizeRetrieved);

        if (ACP_RC_NOT_SUCCESS(sRet))
        {
            ACI_TEST_RAISE(ACP_RC_NOT_EOF(sRet), LABEL_FILE_READ_ERR);
        }

        if (sFileSizeRetrieved != 0)
        {
            /* ���� �о�� ũ��� Overwrite */
            sDataSize = sFileSizeRetrieved;
            CM_ENDIAN_ASSIGN4(sSizePtr, &sDataSize);
            sCtx->mWriteBlock->mCursor += sFileSizeRetrieved;

            ACI_TEST(ulnWriteProtocol(aFnContext, aPtContext, &sPacket)
                     != ACI_SUCCESS);

            /*
             * ulnLob �� ����� �߰���Ų ��ŭ ������Ų��.
             */
            aLob->mSize += sFileSizeRetrieved;
        }
        else
        {
            /* ���Ͽ��� ���� �����Ͱ� 0�̸� mCursor�� ��ġ�� �ǵ����� */
            sCtx->mWriteBlock->mCursor = sCursor;
        }
    } while (sFileSizeRetrieved != 0 &&
             aLob->mSize < aLob->mBuffer->mObject.mFile.mFileSize );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FILE_READ_ERR)
    {
        sCtx->mWriteBlock->mCursor = sCursor;

        ulnError(aFnContext, ulERR_ABORT_LOB_FILE_READ_ERR);
    }
    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "ulnLobBufferDataOutFILE");
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
static ACI_RC ulnLobBufferDataOutCHAR(ulnFnContext *aFnContext,
                                      ulnPtContext *aPtContext,
                                      ulnLob       *aLob)
{
    cmiProtocolContext  *sCtx     = &(aPtContext->mCmiPtContext);
    acp_uint64_t         sLobLocatorVal;

    ulnDbc       *sDbc = NULL;

    acp_uint8_t  *sSrcPrePtr     = NULL;
    acp_uint8_t  *sSrcCurPtr     = aLob->mBuffer->mObject.mMemory.mBuffer;
    acp_uint32_t  sSrcSize       = aLob->mBuffer->mOp->mGetSize(aLob->mBuffer);
    acp_uint8_t  *sSrcFencePtr   = sSrcCurPtr + sSrcSize;
    acp_uint32_t  sSrcRemainSize = sSrcSize;

    acp_uint8_t  *sDestPtr          = NULL;
    acp_uint32_t  sDestOffset       = 0;
    acp_uint32_t  sDestRemainSize   = 0;
    acp_uint32_t  sDestMaxPrecision = 0;
    acp_sint32_t  sRemainSize       = 0;

    mtlModule          *sSrcCharSet  = NULL;
    mtlModule          *sDestCharSet = NULL;
    aciConvCharSetList  sSrcCharSetIndex;
    aciConvCharSetList  sDestCharSetIndex;
    acp_uint8_t        *sSizePtr        = NULL;
    acp_uint16_t        sOrgWriteCursor = CMI_GET_CURSOR(sCtx);
    acp_uint8_t         sState          = 0;
    ACI_RC              sRC             = ACI_FAILURE;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));

    sSrcCharSet       = sDbc->mClientCharsetLangModule;
    sDestCharSet      = sDbc->mCharsetLangModule;
    sSrcCharSetIndex  = mtlGetIdnCharSet(sSrcCharSet);
    sDestCharSetIndex = mtlGetIdnCharSet(sDestCharSet);
    sDestMaxPrecision = sDestCharSet->maxPrecision(1);

    while (sSrcRemainSize > 0)
    {
        sDestOffset = 0;
        sDestRemainSize = ULN_LOB_PIECE_SIZE;

        CMI_WRITE_CHECK(sCtx, 13 + sDestRemainSize);
        sState = 1;

        CMI_WOP(sCtx, CMP_OP_DB_LobPut);
        CMI_WR8(sCtx, &sLobLocatorVal);

        /* ������ ũ�� ��ġ�� ����� ���� */
        sSizePtr = sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor;
        CMI_WR4(sCtx, &sDestRemainSize); // size

        sDestPtr = sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor;

        while (sDestMaxPrecision < sDestRemainSize &&
               sSrcCurPtr < sSrcFencePtr)
        {
            sRemainSize = sDestMaxPrecision;

            sRC = aciConvConvertCharSet(sSrcCharSetIndex,
                                        sDestCharSetIndex,
                                        sSrcCurPtr,
                                        sSrcRemainSize,
                                        sDestPtr + sDestOffset,
                                        &sRemainSize,
                                        -1);

            ACI_TEST_RAISE(sRC != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);

            sSrcPrePtr = sSrcCurPtr;
            (void)sSrcCharSet->nextCharPtr(&sSrcCurPtr, sSrcFencePtr);
            sSrcRemainSize -= (sSrcCurPtr - sSrcPrePtr);

            sDestOffset     += (sDestMaxPrecision - sRemainSize);
            sDestRemainSize -= (sDestMaxPrecision - sRemainSize);
        }

        /* ���� �о�� ũ��� Overwrite */
        CM_ENDIAN_ASSIGN4(sSizePtr, &sDestOffset);
        sCtx->mWriteBlock->mCursor += sDestOffset;

        aLob->mSize += sDestOffset;
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "ulnLobBufferDataOutCHAR");
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED)
    {
        ulnError(aFnContext,
                 ulERR_ABORT_CONVERT_CHARSET_FAILED,
                 sSrcCharSet,
                 sDestCharSet,
                 sSrcFencePtr - sSrcCurPtr,
                 sDestOffset);
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
static ACI_RC ulnLobBufferDataOutWCHAR(ulnFnContext *aFnContext,
                                       ulnPtContext *aPtContext,
                                       ulnLob       *aLob)
{
    cmiProtocolContext  *sCtx     = &(aPtContext->mCmiPtContext);
    acp_uint64_t         sLobLocatorVal;

    ulnDbc       *sDbc = NULL;

    acp_uint8_t  *sSrcPrePtr      = NULL;
    acp_uint8_t  *sSrcCurPtr      = aLob->mBuffer->mObject.mMemory.mBuffer;
    acp_uint32_t  sSrcSize        = aLob->mBuffer->mOp->mGetSize(aLob->mBuffer);
    acp_uint8_t  *sSrcFencePtr    = sSrcCurPtr + sSrcSize;
    acp_uint32_t  sSrcRemainSize  = sSrcSize;
    acp_uint16_t  sOrgWriteCursor = CMI_GET_CURSOR(sCtx);

#ifndef ENDIAN_IS_BIG_ENDIAN
    acp_uint8_t   sSrcWCharBuf[2] = {0, };
#endif

    acp_uint8_t  *sDestPtr          = NULL;
    acp_uint32_t  sDestOffset       = 0;
    acp_uint32_t  sDestRemainSize   = 0;
    acp_uint32_t  sDestMaxPrecision = 0;
    acp_sint32_t  sRemainSize       = 0;

    mtlModule          *sSrcCharSet  = NULL;
    mtlModule          *sDestCharSet = NULL;
    aciConvCharSetList  sSrcCharSetIndex;
    aciConvCharSetList  sDestCharSetIndex;
    acp_uint8_t        *sSizePtr = NULL;
    acp_uint8_t         sState   = 0;
    ACI_RC              sRC = ACI_FAILURE;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));

    sSrcCharSet       = sDbc->mWcharCharsetLangModule;
    sDestCharSet      = sDbc->mCharsetLangModule;
    sSrcCharSetIndex  = mtlGetIdnCharSet(sSrcCharSet);
    sDestCharSetIndex = mtlGetIdnCharSet(sDestCharSet);
    sDestMaxPrecision = sDestCharSet->maxPrecision(1);

    while (sSrcRemainSize > 0)
    {
        sDestOffset = 0;
        sDestRemainSize = ULN_LOB_PIECE_SIZE;

        CMI_WRITE_CHECK(sCtx, 13 + sDestRemainSize);
        sState = 1;

        CMI_WOP(sCtx, CMP_OP_DB_LobPut);
        CMI_WR8(sCtx, &sLobLocatorVal);

        /* ������ ũ�� ��ġ�� ����� ���� */
        sSizePtr = sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor;
        CMI_WR4(sCtx, &sDestRemainSize); // size

        sDestPtr = sCtx->mWriteBlock->mData + sCtx->mWriteBlock->mCursor;

        while (sDestMaxPrecision < sDestRemainSize &&
               sSrcCurPtr < sSrcFencePtr)
        {
            sRemainSize = sDestMaxPrecision;

#ifndef ENDIAN_IS_BIG_ENDIAN
            /* UTF16 Little Endian�̸� Big Endian���� ��ȯ���� */
            if (sSrcRemainSize >= 2)
            {
                sSrcWCharBuf[0] = sSrcCurPtr[1];
                sSrcWCharBuf[1] = sSrcCurPtr[0];
            }
            else
            {
                /* Nothing */
            }

            sRC = aciConvConvertCharSet(sSrcCharSetIndex,
                                        sDestCharSetIndex,
                                        sSrcWCharBuf,
                                        sSrcRemainSize,
                                        sDestPtr + sDestOffset,
                                        &sRemainSize,
                                        -1);
#else
            sRC = aciConvConvertCharSet(sSrcCharSetIndex,
                                        sDestCharSetIndex,
                                        sSrcCurPtr,
                                        sSrcRemainSize,
                                        sDestPtr + sDestOffset,
                                        &sRemainSize,
                                        -1);
#endif

            ACI_TEST_RAISE(sRC != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);

            sSrcPrePtr = sSrcCurPtr;
            (void)sSrcCharSet->nextCharPtr(&sSrcCurPtr, sSrcFencePtr);
            sSrcRemainSize -= (sSrcCurPtr - sSrcPrePtr);

            sDestOffset     += (sDestMaxPrecision - sRemainSize);
            sDestRemainSize -= (sDestMaxPrecision - sRemainSize);
        }

        /* ���� �о�� ũ��� Overwrite */
        CM_ENDIAN_ASSIGN4(sSizePtr, &sDestOffset);
        sCtx->mWriteBlock->mCursor += sDestOffset;

        aLob->mSize += sDestOffset;
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "ulnLobBufferDataOutWCHAR");
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED)
    {
        ulnError(aFnContext,
                 ulERR_ABORT_CONVERT_CHARSET_FAILED,
                 sSrcCharSet,
                 sDestCharSet,
                 sSrcFencePtr - sSrcCurPtr,
                 sDestOffset);
    }
    ACI_EXCEPTION_END;

    if( (sState == 0) && (cmiGetLinkImpl(sCtx) == CMN_LINK_IMPL_IPCDA) )
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_MESSAGE_TOO_LONG, CMB_BLOCK_DEFAULT_SIZE);
    }

    CMI_SET_CURSOR(sCtx, sOrgWriteCursor);

    return ACI_FAILURE;
}

/*
 * -----------------------
 * ulnLobBuffer : Finalize
 * -----------------------
 */

static ACI_RC ulnLobBufferFinalizeMEMORY(ulnFnContext *aFnContext,
                                         ulnLobBuffer *aLobBuffer)
{
    ACP_UNUSED(aFnContext);

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    ulnCharSetFinalize(&aLobBuffer->mCharSet);

    return ACI_SUCCESS;
}

static ACI_RC ulnLobBufferFinalizeFILE(ulnFnContext *aFnContext, ulnLobBuffer *aLobBuffer)
{
    /*
     * �ӽ÷� �Ҵ��ߴ� �޸� ����
     */

    if (aLobBuffer->mObject.mFile.mTempBuffer != NULL)
    {
        acpMemFree(aLobBuffer->mObject.mFile.mTempBuffer);
        aLobBuffer->mObject.mFile.mTempBuffer = NULL;
    }

    /*
     * ���� ����
     */

    aLobBuffer->mObject.mFile.mFileSize = ACP_SINT64_LITERAL(0);

    ACI_TEST_RAISE(aLobBuffer->mObject.mFile.mFileHandle.mHandle == 
                   ULN_INVALID_HANDLE, LABEL_MEM_MAN_ERR);
    ACI_TEST_RAISE(
        acpFileClose(&aLobBuffer->mObject.mFile.mFileHandle) != ACP_RC_SUCCESS,
        LABEL_FILE_CLOSE_ERR);

    aLobBuffer->mObject.mFile.mFileHandle.mHandle = ULN_INVALID_HANDLE;

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    ulnCharSetFinalize(&aLobBuffer->mCharSet);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FILE_CLOSE_ERR)
    {
        ulnError(aFnContext, ulERR_ABORT_FILE_CLOSE, aLobBuffer->mObject.mFile.mFileName);
    }

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(aFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "ulnLobBufferFileFinalize");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static acp_uint32_t ulnLobBufferGetSizeMEMORY(ulnLobBuffer *aBuffer)
{
    return aBuffer->mObject.mMemory.mBufferSize;
}

static acp_uint32_t ulnLobBufferGetSizeFILE(ulnLobBuffer *aBuffer)
{
    return aBuffer->mObject.mFile.mFileSize;
}

/*
 * ========================================
 *
 * �ܺη� export �Ǵ� �Լ���
 *
 *      : SQLGetLob
 *      : SQLPutLob
 *      : SQLGetLobLength
 *
 * ���� �Լ����� ���ȴ�.
 *
 * ========================================
 */

ACI_RC ulnLobGetSize(ulnFnContext *aFnContext,
                     ulnPtContext *aPtContext,
                     acp_uint64_t  aLocatorID,
                     acp_uint32_t *aSize,
                     acp_uint16_t *aIsNull)
{
    ulnStmt      *sStmt  = NULL;
    ulnCache     *sCache = NULL;
    ulnLob       *sLob;
    ulnColumn    *sColumn;
    acp_uint32_t  i, j;
    void         *sTempArg;
    ulnDbc       *sDbc;
    ulnRow       *sRow;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLocatorVal;

    acp_uint32_t  sOffset;
    acp_uint8_t  *sSrc;
    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    acp_uint64_t  sSize;

    /* PROJ-2728 Sharding LOB */
    acp_bool_t    sIsNullLob      = ACP_FALSE;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);


    /* BUG-44125 [mm-cli] IPCDA ��� �׽�Ʈ �� hang - iloader CLOB */
    ACI_TEST_RAISE(cmiGetLinkImpl(&aPtContext->mCmiPtContext) == CMI_LINK_IMPL_IPCDA,
                   IPCDANotSupport);

    ACI_TEST_RAISE( aFnContext->mObjType != ULN_OBJ_TYPE_STMT,
                    LABEL_CACHE_MISS );

    sStmt  = aFnContext->mHandle.mStmt;
    sCache = ulnStmtGetCache(sStmt);

    /*
     * Seaching in CACHE
     */
    ACI_TEST_RAISE( ulnCacheHasLob( sCache ) == ACP_FALSE, LABEL_CACHE_MISS );

    /*
     * 0. In Current Position
     */
    for ( i = 1; i <= ulnStmtGetColumnCount(sStmt); i++ )
    {
        sColumn = ulnCacheGetColumn(sCache, i);

        if ( (sColumn->mMtype == ULN_MTYPE_BLOB) ||
             (sColumn->mMtype == ULN_MTYPE_CLOB) )
        {
            sLob = (ulnLob*)sColumn->mBuffer;
            ACI_TEST_RAISE(sLob == NULL, LABEL_FUNCTION_SEQUENCE_ERR);  /* BUG-46889 */

            ULN_GET_LOB_LOCATOR_VALUE(&(sLocatorVal),&(sLob->mLocatorID));
            if ( sLocatorVal == aLocatorID )
            {
                *aSize = sLob->mSize;
                sIsNullLob = sLob->mIsNull;
                ACI_RAISE( LABEL_CACHE_HIT );
            }
        }
    }

    /*
     * PROJ-2047 Strengthening LOB - LOBCACHE
     * 
     * 1. In LOB Cache
     */
    ACI_TEST_RAISE(ulnLobCacheGetLobLength(sStmt->mLobCache,
                                           aLocatorID,
                                           aSize,
                                           &sIsNullLob)
                   == ACI_SUCCESS, LABEL_CACHE_HIT);

    /*
     * 2. In whole CACHE
     */

    // To Fix BUG-20480
    // ���� Cursor Position�� �̿��Ͽ� Cache ���� �����ϴ� ���� �˻�
    // Review : ulnCacheGetRow() �Լ��� �̿��� ������ Position ��뵵 ������.
    for ( i = 0; i < (acp_uint32_t)ulnCacheGetRowCount(sCache); i++ )
    {
        sRow = ulnCacheGetRow(sCache, i);
        sOffset = 0;

        for ( j = 1; j <= ulnStmtGetColumnCount(sStmt); j++ )
        {
            sColumn = ulnCacheGetColumn(sCache, j);
            if ( (sColumn->mMtype == ULN_MTYPE_BLOB) ||
                 (sColumn->mMtype == ULN_MTYPE_CLOB) )
            {
                sSrc = sRow->mRow + sOffset;
                CM_ENDIAN_ASSIGN8(&sLocatorVal, sSrc);

                if (sLocatorVal == aLocatorID )
                {
                    sSrc = sRow->mRow + sOffset + 8;

                    /* PROJ-2047 Strengthening LOB - LOBCACHE */
                    CM_ENDIAN_ASSIGN8(&sSize, sSrc);

                    /* PROJ-2728 Sharding LOB */
                    if ( sSize == ACP_ULONG_MAX )
                    {
                        sIsNullLob = ACP_TRUE;
                        *aSize = 0;
                    }
                    else
                    {
                        sIsNullLob = ACP_FALSE;
                        *aSize = sSize;
                    }

                    ACI_RAISE( LABEL_CACHE_HIT );
                }
            }
            // ������ ���� �����Ϳ��� ���� column ��ġ�� ���Ѵ�.
            ulnDataGetNextColumnOffset(sColumn, sRow->mRow+sOffset, &sOffset);
        }
    }

    ACI_EXCEPTION_CONT( LABEL_CACHE_MISS );

    /*
     * 3. Get LobSize from Server
     */
    ACI_TEST(ulnWriteLobGetSizeREQ(aFnContext,
                                   aPtContext,
                                   aLocatorID) != ACI_SUCCESS);

    ACI_TEST(ulnFlushProtocol(aFnContext,
                              aPtContext) != ACI_SUCCESS);

    sTempArg          = aFnContext->mArgs;
    aFnContext->mArgs = (void *)&sSize;

    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    /* PROJ-2728 Sharding LOB
     *   ULONG_MAX ���� NULL�� ����Ѵ� */
    if ( sSize == ACP_ULONG_MAX )
    {
        sIsNullLob = ACP_TRUE;
        sSize = 0;
    }
    else
    {
        sIsNullLob = ACP_FALSE;
    }
    *aSize = sSize;

    aFnContext->mArgs = sTempArg;

    ACI_EXCEPTION_CONT( LABEL_CACHE_HIT );

    /* PROJ-2728 Sharding LOB */
    if ( aIsNull != NULL )
    {
        *aIsNull = sIsNullLob;
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(IPCDANotSupport)
    {
        ulnError(aFnContext, ulERR_ABORT_IPCDA_UNSUPPORTED_FUNCTION);
    }
    ACI_EXCEPTION(LABEL_FUNCTION_SEQUENCE_ERR)
    {
        ulnError(aFnContext, ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobGetData(ulnFnContext *aFnContext,
                            ulnPtContext *aPtContext,
                            ulnLob       *aLob,
                            ulnLobBuffer *aLobBuffer,
                            acp_uint32_t  aStartingOffset,
                            acp_uint32_t  aSizeToGet)
{
    void   *sTempArg;
    ulnDbc *sDbc;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    acp_uint64_t  sLobLocatorValCache;

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    ulnStmt      *sStmt  = NULL;
    ulnCache     *sCache = NULL;
    ulnLob       *sLob;
    ulnColumn    *sColumn;
    acp_uint32_t  i;

    /*
     * ���
     */
    sTempArg      = aFnContext->mArgs;
    aLob->mBuffer = aLobBuffer;

    /*
     * send LOB GET REQ
     *
     * aStartingOffset :
     *      ������ ����� lob ������ �߿���
     *      �������� ���� �������� ���� ��ġ
     */

    aLob->mSizeRetrieved = 0;

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    aLob->mGetSize       = aSizeToGet;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    // fix BUG-18938
    // LOB �������� ���̰� 0�̸� �����͸� ��û�� �ʿ䰡 ����.
    ACI_TEST_RAISE(aSizeToGet == 0, NO_NEED_REQUEST_TO_SERVER);

    ACI_TEST_RAISE(aLobBuffer->mType != ULN_LOB_BUFFER_TYPE_FILE &&
                   aLobBuffer->mOp->mGetSize(aLobBuffer) == 0,
                   NO_NEED_REQUEST_TO_SERVER);

    aFnContext->mArgs = (void *)aLob;

    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal, &(aLob->mLocatorID));                      

    /* PROJ-2728 Sharding LOB
     *   handle�� DBC�̸� ������ �䱸�ؾ� �Ѵ� */
    ACI_TEST_RAISE( aFnContext->mObjType != ULN_OBJ_TYPE_STMT,
                    REQUEST_TO_SERVER );

    sStmt  = aFnContext->mHandle.mStmt;
    sCache = ulnStmtGetCache(sStmt);

    /*
     * Seaching in CACHE
     */
    ACI_TEST_RAISE(ulnCacheHasLob(sCache) == ACP_FALSE, REQUEST_TO_SERVER);

    /*
     * PROJ-2047 Strengthening LOB - LOBCACHE
     */
    for ( i = 1; i <= ulnStmtGetColumnCount(sStmt); i++ )
    {
        sColumn = ulnCacheGetColumn(sCache, i);

        if ( (sColumn->mMtype == ULN_MTYPE_BLOB) ||
             (sColumn->mMtype == ULN_MTYPE_CLOB) )
        {
            sLob = (ulnLob *)sColumn->mBuffer;
            ACI_TEST_RAISE(sLob == NULL, LABEL_FUNCTION_SEQUENCE_ERR);  /* BUG-46889 */

            ULN_GET_LOB_LOCATOR_VALUE(&(sLobLocatorValCache),
                                      &(sLob->mLocatorID));
            if (sLobLocatorValCache == sLobLocatorVal)
            {
                ACI_TEST_RAISE(sLob->mData == NULL, REQUEST_TO_SERVER);

                ACI_TEST_RAISE((acp_uint64_t)aStartingOffset +
                               (acp_uint64_t)aSizeToGet >
                               (acp_uint64_t)sLob->mSize,
                               LABEL_INVALID_LOB_RANGE);

                ACI_TEST_RAISE(
                    aLobBuffer->mOp->mDataIn(NULL,
                                             0,
                                             aSizeToGet,
                                             sLob->mData + aStartingOffset,
                                             aFnContext) != ACI_SUCCESS,
                    REQUEST_TO_SERVER);

                ACI_RAISE(NO_NEED_REQUEST_TO_SERVER);
            }
            else
            {
                /* Nothing */
            }
        }
        else
        {
            /* Nothing */
        }
    }

    /*
     * PROJ-2047 Strengthening LOB - LOBCACHE
     *
     * ������ ��û�ϱ� ���� LOB CACHE�� Ȯ������.
     */
    ACI_TEST_RAISE(ulnLobCacheGetLob(sStmt->mLobCache,
                                     sLobLocatorVal,
                                     aStartingOffset,
                                     aSizeToGet,
                                     aFnContext)
                   == ACI_SUCCESS, NO_NEED_REQUEST_TO_SERVER);

    ACI_TEST_RAISE(ulnLobCacheGetErr(sStmt->mLobCache)
                   == LOB_CACHE_INVALID_RANGE, LABEL_INVALID_LOB_RANGE);

    ACI_EXCEPTION_CONT(REQUEST_TO_SERVER);

    /* ������ ��û���� */
    ACI_TEST(ulnWriteLobGetREQ(aFnContext,
                               aPtContext,
                               sLobLocatorVal,
                               aStartingOffset,
                               aSizeToGet) != ACI_SUCCESS);
    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    /*
     * receive LOB GET RES : ulnCallbackLobGetResult() �Լ����� ��ӵ�.
     */
    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    ACI_EXCEPTION_CONT(NO_NEED_REQUEST_TO_SERVER);

    /*
     * ����
     */
    aFnContext->mArgs = sTempArg;
    aLob->mBuffer     = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_INVALID_LOB_RANGE)
    {
        ulnError(aFnContext, ulERR_ABORT_INVALID_LOB_RANGE);
    }
    ACI_EXCEPTION(LABEL_FUNCTION_SEQUENCE_ERR)
    {
        ulnError(aFnContext, ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
    }
    ACI_EXCEPTION_END;

    /*
     * ����
     */
    aFnContext->mArgs = sTempArg;
    aLob->mBuffer     = NULL;

    return ACI_FAILURE;
}

ACI_RC ulnLobFreeLocator(ulnFnContext *aFnContext, ulnPtContext *aPtContext, acp_uint64_t aLocator)
{
    ulnDbc  *sDbc;
    ulnStmt *sStmt = NULL;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * send LOB FREE REQ
     */
    ACI_TEST(ulnWriteLobFreeREQ(aFnContext, aPtContext, aLocator) != ACI_SUCCESS);

    if (sDbc->mConnType == ULN_CONNTYPE_IPC)
    {
        /*
         * ��Ŷ ����
         *
         * BUGBUG : �̰� Ʃ���� ���߿� ����. free �ٱ����� flush �� �ָ� �� ������.
         */
        ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

        /*
         * receive LOB FREE RES
         */
        ACI_TEST(ulnReadProtocol(aFnContext,
                                 aPtContext,
                                 sDbc->mConnTimeoutValue) != ACI_SUCCESS);
    }

    /* PROJ-2728 Sharding LOB */
    if ( aFnContext->mObjType == ULN_OBJ_TYPE_STMT )
    {
        sStmt = aFnContext->mHandle.mStmt;

        /* PROJ-2047 Strengthening LOB - LOBCACHE */
        ACI_TEST(ulnLobCacheRemove(sStmt->mLobCache, aLocator) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ========================================
 *
 * ULN LOB
 *
 * ========================================
 */

/*
 * ------------------
 * ulnLob : Get State
 * ------------------
 */

static ulnLobState ulnLobGetState(ulnLob *aLob)
{
    return aLob->mState;
}

/*
 * --------------------
 * ulnLob : Set Locator
 * --------------------
 */

static ACI_RC ulnLobSetLocator(ulnFnContext *aFnContext, ulnLob *aLob, acp_uint64_t aLocatorID)
{
    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_INITIALIZED, LABEL_INVALID_STATE);
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_SET_LOB_LOCATOR(&(aLob->mLocatorID),aLocatorID);    
    aLob->mState     = ULN_LOB_ST_LOCATOR;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_STATE)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_INVALID_STATE, "ulnLobSetLocator");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * -------------
 * ulnLob : Open
 * -------------
 */

static ACI_RC ulnLobOpen(ulnFnContext *aFnContext,
                         ulnPtContext *aPtContext,
                         ulnLob       *aLob)
{
    ACP_UNUSED(aPtContext);

    /*
     * BUGBUG : �̺κп��� ULN_LOB_ST_OPENED �� ���ֵ� �Ǵ� �������� �����ؾ� �Ѵ�.
     *          �ϴ�, getdata ���� ��¿ �� ���� ������ open �� ȣ��Ǵ� ������ �����
     *          ������ �׳� ����ϵ��� �ߴ�.
     */

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_LOCATOR &&
                   aLob->mState != ULN_LOB_ST_OPENED,
                   LABEL_INVALID_STATE);

    aLob->mState = ULN_LOB_ST_OPENED;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_STATE)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_INVALID_STATE, "ulnLobOpen");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * --------------
 * ulnLob : Close
 * --------------
 */

static ACI_RC ulnLobClose(ulnFnContext *aFnContext, ulnPtContext *aPtContext, ulnLob *aLob)
{

    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    ACI_TEST_RAISE((aLob->mState != ULN_LOB_ST_LOCATOR &&
                    aLob->mState != ULN_LOB_ST_OPENED),
                   LABEL_INVALID_STATE);

    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    ACI_TEST(ulnLobFreeLocator(aFnContext, aPtContext, sLobLocatorVal) != ACI_SUCCESS);

    aLob->mState         = ULN_LOB_ST_INITIALIZED;

    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_SET_LOB_LOCATOR(&(aLob->mLocatorID), ACP_SINT64_LITERAL(0));
    aLob->mSizeRetrieved = 0;
    aLob->mSize          = 0;

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    aLob->mGetSize       = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_STATE)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_INVALID_STATE, "ulnLobClose");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ---------------
 * ulnLob : Append
 * ---------------
 */

/* PROJ-2047 Strengthening LOB - Removed aOldSize */
static ACI_RC ulnLobAppendBegin(ulnFnContext *aFnContext,
                                ulnPtContext *aPtContext,
                                ulnLob       *aLob,
                                acp_uint32_t  aSizeToAppend)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    ACI_TEST(ulnWriteLobPutBeginREQ(aFnContext,
                                    aPtContext,
                                    sLobLocatorVal,
                                    aLob->mSize,   /* Offset to Start : LOB �� �� �� */
                                    aSizeToAppend) /* Size to append */
             != ACI_SUCCESS);

    /*
     * Note : Append �ϴ� ���̹Ƿ� LOB ������� ��ȭ�� ����.
     */

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobAppendEnd(ulnFnContext *aFnContext, ulnPtContext *aPtContext, ulnLob *aLob)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    ACI_TEST(ulnWriteLobPutEndREQ(aFnContext,
                                  aPtContext,
                                  sLobLocatorVal) != ACI_SUCCESS);

    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobAppend(ulnFnContext *aFnContext,
                           ulnPtContext *aPtContext,
                           ulnLob       *aLob,
                           ulnLobBuffer *aLobBuffer)
{
    ULN_FLAG(sNeedEndLob);
    ulnDbc *sDbc;

    aLob->mBuffer = aLobBuffer;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    /*
     * APPEND BEGIN
     */

    ACI_TEST(ulnLobAppendBegin(aFnContext,
                               aPtContext,
                               aLob,
                               aLobBuffer->mOp->mGetSize(aLobBuffer)) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedEndLob);

    /*
     * ������ ����
     */

    ACI_TEST(aLobBuffer->mOp->mDataOut(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    /*
     * APPEND END
     */

    ULN_FLAG_DOWN(sNeedEndLob);
    ACI_TEST(ulnLobAppendEnd(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    aLob->mBuffer = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "ulnLobAppend");
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedEndLob)
    {
        /*
         * Note : Flush �� ReadProtocol �� ���⼭ ���ص� finalize protocol context ����
         *        �˾Ƽ� �� �ش�. �ϴ� ������ �����Ƿ� �ؾ������ �ȴ�.
         */
        ulnLobAppendEnd(aFnContext, aPtContext, aLob);
    }

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

/*
 * ------------------
 * ulnLob : Overwrite
 * ------------------
 */

/* PROJ-2047 Strengthening LOB - Removed aOldSize */
static ACI_RC ulnLobOverWriteBegin(ulnFnContext *aFnContext,
                                   ulnPtContext *aPtContext,
                                   ulnLob       *aLob,
                                   acp_uint32_t  aSizeToOverWrite)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    ACI_TEST(ulnWriteLobPutBeginREQ(aFnContext,
                                    aPtContext,
                                    sLobLocatorVal,
                                    0,                 /* Offset to Start */
                                    aSizeToOverWrite)  /* OverwriteSize */
             != ACI_SUCCESS);

    /*
     * Note : Overwrite �ϴ� ���̹Ƿ� LOB ����� �������� 0 ���� �ʱ�ȭ���Ѿ� �Ѵ�.
     *        ulnLobAppendCore() �Լ����� �����͸� �����ϸ鼭
     *        ulnLob::mSize �� ������ �縸ŭ �������Ѽ� ����ȭ ��Ų��.
     */

    aLob->mSize = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobOverWriteEnd(ulnFnContext *aFnContext, ulnPtContext *aPtContext, ulnLob *aLob)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    ACI_TEST(ulnWriteLobPutEndREQ(aFnContext,
                                  aPtContext,
                                  sLobLocatorVal) != ACI_SUCCESS);

    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobOverWrite(ulnFnContext *aFnContext,
                              ulnPtContext *aPtContext,
                              ulnLob       *aLob,
                              ulnLobBuffer *aLobBuffer)
{
    ULN_FLAG(sNeedEndLob);
    ulnDbc *sDbc;

    aLob->mBuffer = aLobBuffer;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST_RAISE(aLob->mState != ULN_LOB_ST_OPENED, LABEL_LOB_NOT_OPENED);

    /*
     * OVERWRITE BEGIN
     */

    ACI_TEST(ulnLobOverWriteBegin(aFnContext,
                                  aPtContext,
                                  aLob,
                                  aLobBuffer->mOp->mGetSize(aLobBuffer)) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedEndLob);

    /*
     * ������ ����
     */

    ACI_TEST(aLobBuffer->mOp->mDataOut(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    /*
     * OVERWRITE END
     */

    ULN_FLAG_DOWN(sNeedEndLob);
    ACI_TEST(ulnLobOverWriteEnd(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    aLob->mBuffer = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_LOB_NOT_OPENED)
    {
        ulnError(aFnContext, ulERR_FATAL_LOB_NOT_OPENED, "ulnLobOverWrite");
    }

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedEndLob)
    {
        /*
         * Note : Flush �� ReadProtocol �� ���⼭ ���ص� finalize protocol context ����
         *        �˾Ƽ� �� �ش�. �ϴ� ������ �����Ƿ� �ؾ������ �ȴ�.
         */
        ulnLobOverWriteEnd(aFnContext, aPtContext, aLob);
    }

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

/*
 * ---------------
 * ulnLob : Update
 * ---------------
 */

/* PROJ-2047 Strengthening LOB - Removed aOldSize */
static ACI_RC ulnLobUpdateBegin(ulnFnContext *aFnContext,
                                ulnPtContext *aPtContext,
                                ulnLob       *aLob,
                                acp_uint32_t  aStartOffset,
                                acp_uint32_t  aNewSize)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));

    ACI_TEST(ulnWriteLobPutBeginREQ(aFnContext,
                                    aPtContext,
                                    sLobLocatorVal,
                                    aStartOffset,
                                    aNewSize) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnLobUpdateEnd(ulnFnContext *aFnContext, ulnPtContext *aPtContext, ulnLob *aLob)
{
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    acp_uint64_t  sLobLocatorVal;
    /* BUG-21932  The ulnLob structure should consider 4 byte alignment for 32bit-GCC */
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));
    
    ACI_TEST(ulnWriteLobPutEndREQ(aFnContext, aPtContext,sLobLocatorVal) != ACI_SUCCESS);

    ACI_TEST(ulnFlushProtocol(aFnContext, aPtContext) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* PROJ-2047 Strengthening LOB - Removed aOldSize */
static ACI_RC ulnLobUpdate(ulnFnContext *aFnContext,
                           ulnPtContext *aPtContext,
                           ulnLob       *aLob,
                           ulnLobBuffer *aLobBuffer,        /* ������Ʈ ������ ���� ä�� ������ */
                           acp_uint32_t  aStartOffset,      /* ������Ʈ ������ (���� lob) */
                           acp_uint32_t  aLengthToUpdate)   /* ������Ʈ ������ ���� */
{
    ULN_FLAG(sNeedEndLob);

    ulnDbc       *sDbc;
    acp_uint32_t  sOriginalSize;

    aLob->mBuffer = aLobBuffer;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * Note : ������Ʈ �Ǿ �ٲ�� ������ ���� ������ update begin �Լ����� ����������
     *        �������� ������Ʈ�� ������ ������ ũ��� �ϴ� ulnLob ����
     *        Append �� �ϴ� �Ͱ� ���������̴�.
     */
    sOriginalSize = aLob->mSize;
    aLob->mSize   = aStartOffset;

    /*
     * UPDATE BEGIN
     */
    ACI_TEST(ulnLobUpdateBegin(aFnContext,
                               aPtContext,
                               aLob,
                               aStartOffset,
                               aLobBuffer->mOp->mGetSize(aLobBuffer)) != ACI_SUCCESS);

    ULN_FLAG_UP(sNeedEndLob);

    /*
     * ������ ����
     */

    ACI_TEST(aLobBuffer->mOp->mDataOut(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    /*
     * UPDATE END
     */

    ULN_FLAG_UP(sNeedEndLob);
    ACI_TEST(ulnLobUpdateEnd(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    /*
     * ������Ʈ �Ŀ� LOB �� ũ�⸦ �������Ѿ� �Ѵ�.
     */

    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    /*
     * ulnLob::mSize �� ������Ʈ �� ���� ������� ��ġ��Ŵ
     */

    aLob->mSize = sOriginalSize - aLengthToUpdate + aLobBuffer->mOp->mGetSize(aLobBuffer);

    aLob->mBuffer = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedEndLob)
    {
        /*
         * Note : Flush �� ReadProtocol �� ���⼭ ���ص� finalize protocol context ����
         *        �˾Ƽ� �� �ش�. �ϴ� ������ �����Ƿ� �ؾ������ �ȴ�.
         */
        ulnLobUpdateEnd(aFnContext, aPtContext, aLob);
    }

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

/* 
 * PROJ-2047 Strengthening LOB - Added Interfaces
 */
static ACI_RC ulnLobTrim(ulnFnContext *aFnContext,
                         ulnPtContext *aPtContext,
                         ulnLob       *aLob,
                         acp_uint32_t  aStartOffset)
{
    acp_uint64_t sLobLocatorVal;
    ULN_GET_LOB_LOCATOR_VALUE(&sLobLocatorVal,&(aLob->mLocatorID));

    ACI_TEST(ulnWriteLobTrimREQ(aFnContext,
                                aPtContext,
                                sLobLocatorVal,
                                aStartOffset) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * PROJ-2728 Sharding LOB
 */
static ACI_RC ulnLobPrepare4Write(ulnFnContext *aFnContext,
                                  ulnPtContext *aPtContext,
                                  ulnLob       *aLob,
                                  acp_uint32_t  aStartOffset,      /* ������Ʈ ������ (���� lob) */
                                  acp_uint32_t  aNewSize)
{
    ulnDbc       *sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * UPDATE BEGIN
     */
    ACI_TEST(ulnLobUpdateBegin(aFnContext,
                               aPtContext,
                               aLob,
                               aStartOffset,
                               aNewSize) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

static ACI_RC ulnLobWrite(ulnFnContext *aFnContext,
                          ulnPtContext *aPtContext,
                          ulnLob       *aLob,
                          ulnLobBuffer *aLobBuffer) /* ������Ʈ ������ ���� ä�� ������ */
{
    ulnDbc       *sDbc;

    aLob->mBuffer = aLobBuffer;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * ������ ����
     */

    ACI_TEST(aLobBuffer->mOp->mDataOut(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    aLob->mBuffer = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

static ACI_RC ulnLobFinishWrite(ulnFnContext *aFnContext,
                                ulnPtContext *aPtContext,
                                ulnLob       *aLob)
{
    ulnDbc       *sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST(ulnLobUpdateEnd(aFnContext, aPtContext, aLob) != ACI_SUCCESS);

    /*
     * ������Ʈ �Ŀ� LOB �� ũ�⸦ �������Ѿ� �Ѵ�.
     */

    ACI_TEST(ulnReadProtocol(aFnContext,
                             aPtContext,
                             sDbc->mConnTimeoutValue) != ACI_SUCCESS);

    aLob->mBuffer = NULL;

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    aLob->mBuffer = NULL;

    return ACI_FAILURE;
}

/*
 * -----------------------------
 * Meaningful callback functions
 * -----------------------------
 */

ACI_RC ulnCallbackLobGetSizeResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext)
{
    ulnFnContext             *sFnContext  = (ulnFnContext *)aUserContext;

    acp_uint64_t        sLobSize64;
    acp_uint64_t        sLocatorID;
    acp_uint32_t        sSize;
    acp_bool_t          sIsNullLob;

    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD8(aProtocolContext, &sLocatorID);
    CMI_RD4(aProtocolContext, &sSize);
    CMI_RD1(aProtocolContext, sIsNullLob);

    /* PROJ-2728 Sharding LOB
     *   ULONG_MAX ���� NULL�� ����Ѵ� */
    if ( sIsNullLob == ACP_TRUE )
    {
        sLobSize64 = ACP_ULONG_MAX;
    }
    else
    {
        sLobSize64 = sSize;
    }

    /*
     * BUGBUG: lob locator ID �� üũ�ؾ� .... �ұ�?
     */

    *(acp_uint64_t *)(sFnContext->mArgs) = sLobSize64;

    return ACI_SUCCESS;
}

ACI_RC ulnCallbackLobGetResult(cmiProtocolContext *aPtContext,
                               cmiProtocol        *aProtocol,
                               void               *aServiceSession,
                               void               *aUserContext)
{
    /*
     * Note : ������ �ƹ��� ū �������� LOB �����͸� ��û�ϴ���� ������ �����͸� �߶�
     *        �����ش�. LOBGET RES �� mOffset ��
     *        ������ �ִ� ��ü LOB �����Ϳ��� ���� ���۵� ��Ŷ�� ���۵Ǵ� ���� ��ġ�̴�.
     *
     *        ��, 1Mib �����͸� �䱸�ؼ� ������ �̸��׸�, 32Kib ������ �ɰ��� LOBGET RES ��
     *        ������ �����ش�.
     *
     *        �������� ������� cmtVariable �� size �� ������ �� �� �ִ�.
     */
    ulnFnContext  *sFnContext = (ulnFnContext *)aUserContext;
    ulnLob        *sLob       = (ulnLob *)sFnContext->mArgs;

    acp_uint64_t       sLocatorID;
    acp_uint32_t       sOffset;
    acp_uint32_t       sDataSize;


    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);

    CMI_RD8(aPtContext, &sLocatorID);
    CMI_RD4(aPtContext, &sOffset);
    CMI_RD4(aPtContext, &sDataSize);

    /*
     * ȣ��Ǵ� �Լ� :
     *      ulnLobBufferDataInFILE()
     *      ulnLobBufferDataInBINARY()
     *      ulnLobBufferDataInCHAR()
     *      ulnLobBufferDataInWCHAR()
     *      ulnLobBufferDataInDumpAsCHAR()
     *      ulnLobBufferDataInDumpAsWCHAR()
     */

    ACI_TEST_RAISE(sLob->mBuffer->mOp->mDataIn(NULL,
                                sOffset,
                                sDataSize,
                                aPtContext->mReadBlock->mData +
                                aPtContext->mReadBlock->mCursor,
                                (void *)sFnContext ) != ACI_SUCCESS, LABEL_MEM_MAN_ERR);

    aPtContext->mReadBlock->mCursor += sDataSize;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnError(sFnContext, ulERR_FATAL_MEMORY_MANAGEMENT_ERROR, "CallbackLobGetResult");
    }

    ACI_EXCEPTION_END;

    aPtContext->mReadBlock->mCursor += sDataSize;

    /*
     * Note : ���⿡�� ACI_SUCCESS �����ϴ� ���� ���װ� �ƴ�. �� �Լ��� �ݹ��Լ��̱� ������.
     */

    return ACI_SUCCESS;
}

/*
 * ------------------------------
 * Meaningless callback functions
 * ------------------------------
 */

ACI_RC ulnCallbackDBLobPutBeginResult(cmiProtocolContext *aProtocolContext,
                                      cmiProtocol        *aProtocol,
                                      void               *aServiceSession,
                                      void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

ACI_RC ulnCallbackDBLobPutEndResult(cmiProtocolContext *aProtocolContext,
                                    cmiProtocol        *aProtocol,
                                    void               *aServiceSession,
                                    void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

ACI_RC ulnCallbackLobFreeResult(cmiProtocolContext *aProtocolContext,
                                cmiProtocol        *aProtocol,
                                void               *aServiceSession,
                                void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

ACI_RC ulnCallbackLobFreeAllResult(cmiProtocolContext *aProtocolContext,
                                   cmiProtocol        *aProtocol,
                                   void               *aServiceSession,
                                   void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}

/* PROJ-2047 Strengthening LOB - Added Interfaces */
ACI_RC ulnCallbackLobTrimResult(cmiProtocolContext *aProtocolContext,
                                cmiProtocol        *aProtocol,
                                void               *aServiceSession,
                                void               *aUserContext)
{
    ACP_UNUSED(aProtocolContext);
    ACP_UNUSED(aProtocol);
    ACP_UNUSED(aServiceSession);
    ACP_UNUSED(aUserContext);

    return ACI_SUCCESS;
}


/*
 * ========================================
 *
 * INITIALIZER functions
 *
 * PROJ-2047 Strengthening LOB - Added Interfaces 
 * 
 * BLOB�� CLOB�� Interface�� ������.
 *
 * ========================================
 */
static ulnLobBufferOp gUlnLobBufferOp[ULN_LOB_TYPE_MAX][ULN_LOB_BUFFER_TYPE_MAX] =
{
    /* NULL */
    {
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        }
    },

    /* BLOB */
    {
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },

        /*
         * ULN_LOB_BUFFER_TYPE_FILE
         */
        {
            ulnLobBufferInitializeFILE,
            ulnLobBufferPrepareFILE,
            ulnLobBufferDataInFILE,
            ulnLobBufferDataOutFILE,
            ulnLobBufferFinalizeFILE,

            ulnLobBufferGetSizeFILE
        },

        /*
         * ULN_LOB_BUFFER_TYPE_CHAR
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInDumpAsCHAR,
            ulnLobBufferDataOutBINARY,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        },

        /*
         * ULN_LOB_BUFFER_TYPE_WCHAR
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInDumpAsWCHAR,
            ulnLobBufferDataOutBINARY,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        },

        /*
         * ULN_LOB_BUFFER_TYPE_BINARY
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInBINARY,
            ulnLobBufferDataOutBINARY,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        }
    },

    /* CLOB */
    {
        {
            NULL, NULL, NULL, NULL, NULL, NULL
        },

        /*
         * ULN_LOB_BUFFER_TYPE_FILE
         */
        {
            ulnLobBufferInitializeFILE,
            ulnLobBufferPrepareFILE,
            ulnLobBufferDataInFILE,
            ulnLobBufferDataOutFILE,
            ulnLobBufferFinalizeFILE,

            ulnLobBufferGetSizeFILE
        },

        /*
         * ULN_LOB_BUFFER_TYPE_CHAR
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInCHAR,
            ulnLobBufferDataOutCHAR,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        },

        /*
         * ULN_LOB_BUFFER_TYPE_WCHAR
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInWCHAR,
            ulnLobBufferDataOutWCHAR,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        },

        /*
         * ULN_LOB_BUFFER_TYPE_BINARY
         */
        {
            ulnLobBufferInitializeMEMORY,
            ulnLobBufferPrepareMEMORY,
            ulnLobBufferDataInBINARY,
            ulnLobBufferDataOutBINARY,
            ulnLobBufferFinalizeMEMORY,

            ulnLobBufferGetSizeMEMORY
        }
    }
};


static ulnLobOp gUlnLobOp =
{
    ulnLobGetState,

    ulnLobSetLocator,

    ulnLobOpen,
    ulnLobClose,

    ulnLobOverWrite,
    ulnLobAppend,
    ulnLobUpdate,

    ulnLobGetData,

    /* PROJ-2047 Strengthening LOB - Added Interfaces */
    ulnLobTrim,

    /* PROJ-2728 Sharding LOB */
    ulnLobPrepare4Write,
    ulnLobWrite,
    ulnLobFinishWrite,
};

/*
 * -------------------------
 * ulnLobBuffer : Initialize
 * -------------------------
 */
ACI_RC ulnLobBufferInitialize(ulnLobBuffer *aLobBuffer,
                              ulnDbc       *aDbc,
                              ulnLobType    aLobType,
                              ulnCTypeID    aCTYPE,
                              acp_uint8_t  *aFileNameOrBufferPtr,
                              acp_uint32_t  aFileOptionOrBufferSize)
{
    ulnLobBufferType sLobBufferType;

    switch (aLobType)
    {
        case ULN_LOB_TYPE_BLOB:
        case ULN_LOB_TYPE_CLOB:
            break;

        default:
            ACI_RAISE(LABEL_INVALID_LOB_TYPE);
            break;
    }

    switch (aCTYPE)
    {
        case ULN_CTYPE_FILE:
            sLobBufferType = ULN_LOB_BUFFER_TYPE_FILE;
            break;

        case ULN_CTYPE_CHAR:
            /*
             * PROJ-2047 Strengthening LOB - Partial Converting
             *
             * ������ Ŭ���̾�Ʈĳ���ͼ��� ������ ��쿡�� BINARY�� �����Ѵ�.
             */
            if (aDbc != NULL && aLobType == ULN_LOB_TYPE_CLOB)
            {
                if (aDbc->mCharsetLangModule == aDbc->mClientCharsetLangModule)
                {
                    sLobBufferType = ULN_LOB_BUFFER_TYPE_BINARY;
                }
                else
                {
                    sLobBufferType = ULN_LOB_BUFFER_TYPE_CHAR;
                }
            }
            else
            {
                sLobBufferType = ULN_LOB_BUFFER_TYPE_CHAR;
            }

            break;

        case ULN_CTYPE_WCHAR:
            sLobBufferType = ULN_LOB_BUFFER_TYPE_WCHAR;
            break;

        case ULN_CTYPE_BINARY:
            sLobBufferType = ULN_LOB_BUFFER_TYPE_BINARY;
            break;

        default:
            ACI_RAISE(LABEL_INVALID_LOB_BUFFER_TYPE);
            break;
    }

    aLobBuffer->mType = sLobBufferType;
    aLobBuffer->mOp   = &gUlnLobBufferOp[aLobType][sLobBufferType];
    aLobBuffer->mLob  = NULL;

    aLobBuffer->mOp->mInitialize(aLobBuffer, aFileNameOrBufferPtr, aFileOptionOrBufferSize);

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    ulnCharSetInitialize(&aLobBuffer->mCharSet);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LOB_BUFFER_TYPE);

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    ACI_EXCEPTION(LABEL_INVALID_LOB_TYPE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * -------------------------
 * ulnLob : Initialize
 * -------------------------
 */

ACI_RC ulnLobInitialize(ulnLob *aLob, ulnMTypeID aMTYPE)
{
    switch (aMTYPE)
    {
        case ULN_MTYPE_BLOB:
        case ULN_MTYPE_BLOB_LOCATOR:
            aLob->mType = ULN_LOB_TYPE_BLOB;
            break;

        case ULN_MTYPE_CLOB:
        case ULN_MTYPE_CLOB_LOCATOR:
            aLob->mType = ULN_LOB_TYPE_CLOB;
            break;

        default:
            ACI_RAISE(LABEL_INVALID_LOB_TYPE);
            break;
    }

    aLob->mState          = ULN_LOB_ST_INITIALIZED;
    ULN_SET_LOB_LOCATOR(&(aLob->mLocatorID), ACP_SINT64_LITERAL(0));
    aLob->mSize           = 0;

    aLob->mSizeRetrieved  = 0;

    aLob->mBuffer         = NULL;
    aLob->mOp             = &gUlnLobOp;

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    aLob->mData           = NULL;

    /* PROJ-2047 Strengthening LOB - Partial Converting */
    aLob->mGetSize        = 0;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LOB_TYPE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
