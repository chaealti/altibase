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
 * $Id $
 **********************************************************************/

#include <idl.h>
#include <iduMemMgr.h>
#include <ute.h>
#include <utpDef.h>
#include <utpCSVWriter.h>
#include <utpStatistics.h>

extern uteErrorMgr     gErrorMgr;

utpCommandType   utpStatistics::mCmdType    = UTP_CMD_NONE;
idBool           utpStatistics::mCsvOut     = ID_FALSE;
idBool           utpStatistics::mTextOut    = ID_FALSE;

altiProfHandle   utpStatistics::mHandle;
UInt             utpStatistics::mPrevProgressRate;

FILE*            utpStatistics::mCsvFp      = NULL;
FILE*            utpStatistics::mTextFp     = NULL;

utpHashmap*      utpStatistics::mMainMap    = NULL;

/* Description:
 *
 * 1. ���ڷ� ���� ���� ������ ���� 5�� ����
 * 2. �� ���� �о IDV_PROF_TYPE_STMT�� ��쿡��
 *    statement ���� ����
 * 3. ��� ���
 */
IDE_RC utpStatistics::run(SInt             aArgc,
                          SChar**          aArgv,
                          utpCommandType   aCmdType,
                          utpStatOutFormat aOutFormat)
{
    SInt            i;
    idvProfHeader  *sHeader;
    void           *sBody;

    IDE_TEST(initialize(aArgc, aArgv, aCmdType, aOutFormat)
             != IDE_SUCCESS);

    /* 1. ���ڷ� ���� ���� ������ ���� 5�� ���� */
    for (i = 0; i < aArgc; i++)
    {
        /* ���� �޽��� ��� */
        idlOS::fprintf(stderr, FMT_PROCESSING, aArgv[i]);
        loadBar(0, 100, 20);

        IDE_TEST(utpProfile::open(aArgv[i], &mHandle)
                 != IDE_SUCCESS);

        /* progress display variable */
        mPrevProgressRate = 0;

        while (utpProfile::next(&mHandle) == IDE_SUCCESS)
        {
            utpProfile::getHeader(&mHandle, &sHeader);
            utpProfile::getBody(&mHandle, &sBody);

            /* 2. �� ���� �о IDV_PROF_TYPE_STMT�� ��쿡��
             *    statement ���� ���� 
             */
            if (sHeader->mType == IDV_PROF_TYPE_STMT)
            {
                getStmtInfo(sBody);
            }
            else
            {
                /* do nothing */
            }
        }

        utpProfile::close(&mHandle);
        loadBar(100, 100, 20);
    }

    /* 3. ��� ���: */
    IDE_TEST(writeResult() != IDE_SUCCESS);

    finalize();

    idlOS::fprintf(stderr, MSG_FINISHED);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    finalize();

    return IDE_FAILURE;
}

/* Description:
 *
 * 1. ���� ���� ���� Ȯ��
 * 2. hash ����
 * 3. profile ���� �б�� ���� ����
 */
IDE_RC utpStatistics::initialize(SInt             aArgc,
                                 SChar**          aArgv,
                                 utpCommandType   aCmdType,
                                 utpStatOutFormat aOutFormat)
{
    SInt i;

    mCmdType = aCmdType;

    if ((aOutFormat == UTP_STAT_BOTH) || (aOutFormat == UTP_STAT_CSV))
    {
        mCsvOut = ID_TRUE;
    }
    if ((aOutFormat == UTP_STAT_BOTH) || (aOutFormat == UTP_STAT_TEXT))
    {
        mTextOut = ID_TRUE;
    }

    /* 1. ���� ���� ���� Ȯ�� */
    for (i = 0; i < aArgc; i++)
    {
        IDE_TEST_RAISE(idlOS::access(aArgv[i], F_OK) != 0,
                       err_file_not_found);
    }

    /* 2. hash ���� */
    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        IDE_TEST(utpHash::create(&mMainMap,
                                 UTP_KEY_TYPE_INT,
                                 UTP_SESSIONHASHMAP_INIT_SIZE)
                 != IDE_SUCCESS);
    }
    else // if (mCmdType == UTP_CMD_STAT_QUERY)
    {
        IDE_TEST(utpHash::create(&mMainMap,
                                 UTP_KEY_TYPE_CHAR,
                                 UTP_QUERYHASHMAP_INIT_SIZE)
                 != IDE_SUCCESS);
    }
    
    /* 3. profile ���� �б�� ���� ���� */
    IDE_TEST(utpProfile::initialize(&mHandle) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_file_not_found);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_FILE_OPEN, aArgv[i]);
        utePrintfErrorCode(stderr, &gErrorMgr);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Description:
 *
 * 1. profile ���� �б�� ���� ����
 * 2. hashmap�� put�� ���� �޸� ����
 * 3. hash ����
 */
IDE_RC utpStatistics::finalize()
{
    /* 1. profile ���� �б�� ���� ���� */
    utpProfile::finalize(&mHandle);

    IDE_TEST_CONT(mMainMap == NULL, empty_hash);

    /* 2. hashmap�� put�� ���� �޸� ���� */
    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        utpHash::traverse(mMainMap, freeSessHashNode, NULL);
    }
    else
    {
        utpHash::traverse(mMainMap, freeStatHashNode, NULL);
    }

    /* 3. hash ���� */
    utpHash::destroy(mMainMap);

    mMainMap = NULL;

    IDE_EXCEPTION_CONT(empty_hash);

    return IDE_SUCCESS;
}

/* Description:
 *
 * hash�� �߰��ϱ� ���� �Ҵ��� �޸𸮸� �����Ѵ�.
 */
IDE_RC utpStatistics::freeSessHashNode(void* /* aItem */,
                                       void* aNodeData)
{
    utpHashmap *sQueryMap = (utpHashmap*)aNodeData;

    utpHash::traverse(sQueryMap, freeStatHashNode, NULL);

    utpHash::destroy(sQueryMap);

    return IDE_SUCCESS;
}

/* Description:
 *
 * hash�� �߰��ϱ� ���� �Ҵ��� �޸𸮸� �����Ѵ�.
 */
IDE_RC utpStatistics::freeStatHashNode(void* /* aItem */,
                                       void* aNodeData)
{
    profStat *sStat = (profStat*)aNodeData;

    idlOS::free(sStat->mQuery);
    idlOS::free(sStat);

    return IDE_SUCCESS;
}

/* Description:
 *
 * statement ���� �߿��� ���� �ð�, ���� ����, �������� �����Ѵ�.
 * hash���� �ش� �������� ã�Ƽ� ������ ������Ʈ�ϰ�,
 * ������ hash�� �߰��Ѵ�.
 */
void utpStatistics::getStmtInfo(void *aBody)
{
    UInt             sQueryLen;
    UChar           *sPtr = (UChar *)aBody;
    SChar           *sQueryBuffer = NULL;
    idvProfStmtInfo  sStmtInfo;

    /* for displaying progress bar */
    PDL_stat         sProgressBuf;
    
    idlOS::memcpy(&sStmtInfo, sPtr, ID_SIZEOF(idvProfStmtInfo));

    sPtr += ID_SIZEOF(idvProfStmtInfo);
    sPtr += ID_SIZEOF(idvSQL);
    
    idlOS::memcpy(&sQueryLen, sPtr, ID_SIZEOF(UInt));

    sPtr += ID_SIZEOF(UInt);

    sQueryBuffer = (SChar *)idlOS::malloc(sQueryLen + 1);
    IDE_TEST_RAISE(sQueryBuffer == NULL, err_memory);

    /* ����� ǥ�� */
    idlOS::fstat(mHandle.mFP, &sProgressBuf);
    loadBar(mHandle.mOffset, sProgressBuf.st_size, 20);

    idlOS::memcpy(sQueryBuffer, sPtr, sQueryLen);
    sQueryBuffer[sQueryLen] = 0;

    IDE_TEST(updateHashData(&sStmtInfo, sQueryBuffer) != IDE_SUCCESS);

    return;

    IDE_EXCEPTION(err_memory);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        exit(0);
    }
    IDE_EXCEPTION_END;

    return;
}

IDE_RC utpStatistics::updateHashData(idvProfStmtInfo *aStmtInfo,
                                     SChar           *aQuery)
{
    UInt        sSessID;
    ULong       sTotalTime;
    utpHashmap *sQueryMap = NULL;
    profStat   *sStatValue = NULL;

    sTotalTime = aStmtInfo->mTotalTime;
    sSessID = aStmtInfo->mSID;

    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        IDE_TEST(utpHash::get(mMainMap,
                              sSessID,
                              (void**)(&sQueryMap)) != IDE_SUCCESS);
    }
    else
    {
        sQueryMap = mMainMap;
    }

    if (sQueryMap == NULL)
    {
        IDE_TEST(utpHash::create(&sQueryMap,
                                 UTP_KEY_TYPE_CHAR,
                                 UTP_QUERYHASHMAP_INIT_SIZE)
                 != IDE_SUCCESS);

        IDE_TEST(utpHash::put(mMainMap,
                              sSessID,
                              sQueryMap) != IDE_SUCCESS);

        IDE_TEST(insertQueryData(sQueryMap,
                                 sSessID,
                                 sTotalTime,
                                 aQuery)
                 != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST(utpHash::get(sQueryMap,
                              aQuery,
                              (void**)(&sStatValue)) != IDE_SUCCESS);
        if (sStatValue != NULL)
        {
            updateQueryData(aStmtInfo->mExecutionFlag,
                            sStatValue,
                            sTotalTime);

            /* �̹� hashmap�� ������, ������ ���� �Ҵ��� �޸𸮴�
             * ���̻� �ʿ� ���� ������ �����Ѵ�
             */
            idlOS::free(aQuery);
        }
        else
        {
            IDE_TEST(insertQueryData(sQueryMap,
                                     sSessID,
                                     sTotalTime,
                                     aQuery)
                     != IDE_SUCCESS);
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Description:
 *
 * hash�� ������ �������� �ִ� ���, �Ʒ� ���� ������Ʈ�Ѵ�.
 * ���� Ƚ��(mCount)
 * �� ���� �ð�(mTotalTime)
 * �ּ� ���� �ð�(mMinTime)
 * �ִ� ���� �ð�(mMaxTime)
 * ���� Ƚ��(mSucc)
 * ���� Ƚ��(mFail)
 */
void utpStatistics::updateQueryData(UInt      aExecFlag,
                                    profStat *aStatValue,
                                    ULong     aTotalTime)
{
    if ( aExecFlag == 0 )
    {
        aStatValue->mFail++;
    }
    else
    {
        aStatValue->mSucc++;
    }

    aStatValue->mCount++;
    aStatValue->mTotalTime += aTotalTime;
    if ( aStatValue->mMinTime > aTotalTime )
    {
        aStatValue->mMinTime = aTotalTime;
    }
    if ( aStatValue->mMaxTime < aTotalTime )
    {
        aStatValue->mMaxTime = aTotalTime;
    }
}

/* Description:
 *
 * hash�� ������ �������� ���� ���, ���ο� ���� hash�� �߰��Ѵ�.
 */
IDE_RC utpStatistics::insertQueryData(utpHashmap *aQueryMap,
                                      UInt        aSessID,
                                      ULong       aTotalTime,
                                      SChar      *aQuery)
{
    profStat *sNewStat;
    
    sNewStat = (profStat*)idlOS::malloc(ID_SIZEOF(profStat));
    IDE_TEST_RAISE(sNewStat == NULL, err_memory);

    sNewStat->mSessID    = aSessID;
    sNewStat->mCount     = 1;
    sNewStat->mMinTime   = aTotalTime;
    sNewStat->mMaxTime   = aTotalTime;
    sNewStat->mTotalTime = aTotalTime;
    sNewStat->mSucc      = 1;
    sNewStat->mFail      = 0;
    sNewStat->mQuery     = aQuery;

    IDE_TEST(utpHash::put(aQueryMap, aQuery, sNewStat) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_memory);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        exit(0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Description:
 *
 * hash �� ������ ��谪�� ����Ѵ�.
 */
IDE_RC utpStatistics::writeResult()
{
    IDE_TEST(setOutfile() != IDE_SUCCESS);

    writeTitle();
    writeStat();

    closeOutfile();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void utpStatistics::writeTitle()
{
    if (mTextOut == ID_TRUE)
    {
        if (mCmdType == UTP_CMD_STAT_SESSION)
        {
            idlOS::fprintf(mTextFp, "SESSION  ");
        }
        idlOS::fprintf(mTextFp, TEXT_TITLE"\n");
        idlOS::fprintf(mTextFp, HORI_LINE"\n");
    }

    if (mCsvOut == ID_TRUE)
    {
        if (mCmdType == UTP_CMD_STAT_SESSION)
        {
            utpCSVWriter::writeTitle(mCsvFp, (const SChar *)CSV_TITLE_SESS);
        }
        else
        {
            utpCSVWriter::writeTitle(mCsvFp, (const SChar *)CSV_TITLE);
        }
    }
}

void utpStatistics::writeStat()
{
    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        utpHash::traverse(mMainMap, sortSessHashNode, NULL);
    }
    else
    {
        sortAndWriteHash(mMainMap);
    }
}

/* Description:
 *
 * Session hash�� �� ��忡�� total time �������� sorting.
 */
IDE_RC utpStatistics::sortSessHashNode(void* /* aItem */,
                                       void* aNodeData)
{
    utpHashmap *sQueryMap = (utpHashmap*)aNodeData;

    sortAndWriteHash(sQueryMap);

    return IDE_SUCCESS;
}

/* Description:
 *
 * ��� ������ �̸��� ���ϰ�, ������ ����.
 */
IDE_RC utpStatistics::setOutfile()
{
    UInt  sClock;
    SChar sFileName[50];
    SChar sFilePrefix[50];

    idlOS::umask(0);
    sClock = idlOS::time(NULL); // second

    /* ���� �̸� ���� */
    idlOS::snprintf(sFilePrefix, ID_SIZEOF(sFilePrefix),
            "alti-prof-stat-%"ID_UINT32_FMT,
            sClock);

    idlOS::fprintf(stderr, "\n");

    if (mCsvOut == ID_TRUE)
    {
        idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                "%s.csv", sFilePrefix);

        mCsvFp = idlOS::fopen(sFileName, "wb");
        IDE_TEST_RAISE(mCsvFp == NULL, err_fopen);

        idlOS::fprintf(stderr, FMT_WRITING_CSV, sFilePrefix);
    }
    if (mTextOut == ID_TRUE)
    {
        idlOS::snprintf(sFileName, ID_SIZEOF(sFileName),
                "%s.txt", sFilePrefix);

        mTextFp = idlOS::fopen(sFileName, "w");
        IDE_TEST_RAISE(mTextFp == NULL, err_fopen);

        idlOS::fprintf(stderr, FMT_WRITING_TEXT, sFilePrefix);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_fopen);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_FILE_OPEN, sFileName);
        utePrintfErrorCode(stderr, &gErrorMgr);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void utpStatistics::closeOutfile()
{
    if (mCsvFp != NULL)
    {
        idlOS::fclose(mCsvFp);
    }
    if (mTextFp != NULL)
    {
        idlOS::fclose(mTextFp);
    }
}

void utpStatistics::writeNodeInCsv(void* aNodeData)
{
    double    sAvg;
    double    sTot;
    double    sMin;
    double    sMax; 

    profStat *sStat  = (profStat*)aNodeData;

    sAvg = (sStat->mTotalTime / 1000000.) /
           (double)(sStat->mCount - sStat->mFail);
    sTot = sStat->mTotalTime / 1000000.;
    sMin = sStat->mMinTime / 1000000.;
    sMax = sStat->mMaxTime / 1000000.;

    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        utpCSVWriter::writeInt(mCsvFp, sStat->mSessID);
    }
    utpCSVWriter::writeInt(mCsvFp, sStat->mCount);
    utpCSVWriter::writeDouble(mCsvFp, sAvg);
    utpCSVWriter::writeDouble(mCsvFp, sTot);
    utpCSVWriter::writeDouble(mCsvFp, sMin);
    utpCSVWriter::writeDouble(mCsvFp, sMax);
    utpCSVWriter::writeInt(mCsvFp, sStat->mSucc);
    utpCSVWriter::writeInt(mCsvFp, sStat->mFail);
    utpCSVWriter::writeString(mCsvFp, sStat->mQuery);
}

void utpStatistics::writeNodeInText(void* aItem, void* aNodeData)
{
    UInt      i;
    UInt      sLen;
    double    sAvg;
    double    sTot;
    double    sMin;
    double    sMax; 

    profStat *sStat     = (profStat*)aNodeData;
    SChar    *sQuery    = sStat->mQuery;
    profSum  *sSumValue = (profSum *)aItem;

    sAvg = (sStat->mTotalTime / 1000000.) /
           (double)(sStat->mCount - sStat->mFail);

    /* ���� ��Ʈ���� �� ���ο� ��µǵ��� line feed ���ڸ� 
     * �������� ġȯ
     */
    sLen = idlOS::strlen((char *)sQuery);
    for (i = 0; i < sLen; i++)
    {
        if (sQuery[i] == '\n')
        {
            sQuery[i] = ' ';
        }
    }

    sTot = sStat->mTotalTime / 1000000.;
    sMin = sStat->mMinTime / 1000000.;
    sMax = sStat->mMaxTime / 1000000.;

    sSumValue->mTotalCnt += sStat->mCount;
    sSumValue->mTotalTime += sStat->mTotalTime;

    if (mCmdType == UTP_CMD_STAT_SESSION)
    {
        idlOS::fprintf(mTextFp, "%6"ID_UINT32_FMT"  ", sStat->mSessID);
    }
    idlOS::fprintf(mTextFp, FMT_STMT_INFO"\n",
            sStat->mCount, sAvg, sTot,
            sMin, sMax,
            sStat->mSucc, sStat->mFail,
            sQuery);
}

IDE_RC utpStatistics::sortAndWriteHash(utpHashmap *aHashmap)
{
    SInt             i = 0;
    SInt             sNodeCnt;
    profStat**       sHashValues = NULL;
    profSum          sTotValue;

    sTotValue.mTotalCnt = 0;
    sTotValue.mTotalTime = 0;

    sNodeCnt = utpHash::size(aHashmap);

    sHashValues = (profStat **)idlOS::malloc(
                    sNodeCnt * ID_SIZEOF(profStat*));

    IDE_TEST_RAISE(sHashValues == NULL, err_memory);

    IDE_TEST(utpHash::values(aHashmap, (void**)sHashValues)
             != IDE_SUCCESS);

    idlOS::qsort(sHashValues,
                 sNodeCnt,
                 ID_SIZEOF(profStat*),
                 compareHashNode);

    for (i = 0; i < sNodeCnt; i++)
    {
        if (mCsvOut == ID_TRUE)
        {
            writeNodeInCsv(sHashValues[i]);
        }
        if (mTextOut == ID_TRUE)
        {
            writeNodeInText(&sTotValue, sHashValues[i]);
        }
    }

    /* ���� ��ü�� ����, ���� �ð� ���, ���� �ð� �հ� ��� */
    if (mTextOut == ID_TRUE)
    {
        idlOS::fprintf(mTextFp, HORI_LINE"\n");
        if (mCmdType == UTP_CMD_STAT_SESSION)
        {
            idlOS::fprintf(mTextFp, "[SUB-TOTAL] ");
        }
        else
        {
            idlOS::fprintf(mTextFp, "[TOTAL] ");
        }
        /* BUG-46048 Codesonar warning */
        idlOS::fprintf(mTextFp, FMT_SUMMARY_INFO,
            sTotValue.mTotalCnt,
            (sTotValue.mTotalCnt == 0)?
                0 : sTotValue.mTotalTime / 1000000. / sTotValue.mTotalCnt,
            sTotValue.mTotalTime / 1000000.);
    }

    idlOS::free(sHashValues);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_memory);
    {
        uteSetErrorCode(&gErrorMgr, utERR_ABORT_memory_error,
                        __FILE__, (UInt)__LINE__);
        utePrintfErrorCode(stderr, &gErrorMgr);
        exit(0);
    }
    IDE_EXCEPTION_END;

    if (sHashValues != NULL)
    {
        idlOS::free(sHashValues);
    }

    return IDE_FAILURE;
}

/* 
 * TOTAL desc, COUNT desc �� ����
 */
SInt utpStatistics::compareHashNode(const void* aLeft,
                                    const void* aRight)
{
    UInt      sSubValue;
    profStat *sLeftStat  = *(profStat **)aLeft;
    profStat *sRightStat = *(profStat **)aRight;

    sSubValue = sRightStat->mTotalTime - sLeftStat->mTotalTime;

    if (sSubValue == 0)
    {
        return sRightStat->mCount - sLeftStat->mCount;
    }
    else
    {
        return sSubValue;
    }
}
