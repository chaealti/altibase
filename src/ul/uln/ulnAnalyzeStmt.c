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
#include <ulnAnalyzeStmt.h>
#include <ulnConv.h>

/* Position Array�� Element �⺻ ���� */
#define BASE_ARR_CNT           (16)

/*
 * ' ', " " ���� ���ڴ� ��� Skip.
 */
#define SKIP_QUOTER(aCurPtr, aFencePtr, aChar) \
do                                             \
{                                              \
    while ((aCurPtr) < (aFencePtr))            \
    {                                          \
        if (*(aCurPtr) != aChar)               \
        {                                      \
            (aCurPtr)++;                       \
        }                                      \
        else                                   \
        {                                      \
            break;                             \
        }                                      \
    }                                          \
} while (0)                                    \

/*
 * BUG-35204 distinguish between the query and the comment
 *
 * COMMENT�� Skip.
 *
 * A line comment     : //, --
 * Multi-line comment : Slash* *Slash
 */
#define SKIP_COMMENT(aCurPtr, aFencePtr, aPrevChar)               \
do                                                                \
{                                                                 \
    if ((aCurPtr) < (aFencePtr))                                  \
    {                                                             \
        if ((aPrevChar) == '/' && *(aCurPtr) == '*')              \
        {                                                         \
            (aCurPtr)++;                                          \
                                                                  \
            while ((aFencePtr) - (aCurPtr) > 1)                   \
            {                                                     \
                if (*(aCurPtr) == '*' && *((aCurPtr) + 1) == '/') \
                {                                                 \
                    (aCurPtr)++;                                  \
                    break;                                        \
                }                                                 \
                else                                              \
                {                                                 \
                    (aCurPtr)++;                                  \
                }                                                 \
            }                                                     \
        }                                                         \
        else if (((aPrevChar) == '/' && *(aCurPtr) == '/') ||     \
                 ((aPrevChar) == '-' && *(aCurPtr) == '-'))       \
        {                                                         \
            (aCurPtr)++;                                          \
                                                                  \
            while ((aCurPtr) < (aFencePtr))                       \
            {                                                     \
                if (*(aCurPtr) != '\n')                           \
                {                                                 \
                    (aCurPtr)++;                                  \
                }                                                 \
                else                                              \
                {                                                 \
                    break;                                        \
                }                                                 \
            }                                                     \
        }                                                         \
        else                                                      \
        {                                                         \
        }                                                         \
    }                                                             \
} while (0)                                                       \

/*
 * IDENTIFIER�� Skip.
 *
 * [a-zA-Z_][a-zA-Z0-9_]*
 */
#define SKIP_IDENTIFIER(aCurPtr, aFencePtr)          \
do                                                   \
{                                                    \
    if ((aCurPtr) < (aFencePtr))                     \
    {                                                \
        if (acpCharIsDigit(*(aCurPtr)) == ACP_TRUE)  \
        {                                            \
            break;                                   \
        }                                            \
    }                                                \
                                                     \
    while ((aCurPtr) < (aFencePtr))                  \
    {                                                \
        if (*(aCurPtr) == '_' ||                     \
            acpCharIsAlnum(*(aCurPtr)) == ACP_TRUE)  \
        {                                            \
            (aCurPtr)++;                             \
        }                                            \
        else                                         \
        {                                            \
            break;                                   \
        }                                            \
    }                                                \
} while (0)                                          \


/**
 *  ulnAnalyzeStmt
 */
struct ulnAnalyzeStmt
{
    acl_mem_area_t          mMemArea;
    acl_mem_area_snapshot_t mMemAreaSnapShot;

    acp_uint16_t            mNameCnt;   /* :name�� ���� */
    acp_uint16_t            mMarkerCnt; /* ?�� ����     */

    acp_list_t              mTokensList;
};

/**
 *  ulnTokenListObj
 */
typedef struct ulnTokenListObj {
    acp_char_t    *mToken;
    acp_sint32_t   mTokenLen;
    acp_uint16_t  *mPosArr;
    acp_uint16_t   mPosCnt;
} ulnTokenListObj;


/**
 *  Declare Inside Function
 */
static ACI_RC ulnAnalyzeStmtSetPosArr(ulnAnalyzeStmt  *aAnalyzeStmt,
                                      acp_char_t      *aToken,
                                      acp_sint32_t     aTokenLen,
                                      acp_uint16_t     aPos);

static ACI_RC ulnAnalyzeStmtStrip(ulnAnalyzeStmt *aAnalyzeStmt,
                                  acp_char_t     *aStmtStr,
                                  acp_sint32_t    aStmtStrLen);

/**
 *  ulnAnalyzeStmtCreate
 */
ACI_RC ulnAnalyzeStmtCreate(ulnAnalyzeStmt **aAnalyzeStmt,
                            acp_char_t      *aStmtStr,
                            acp_sint32_t     aStmtStrLen)
{
    acp_rc_t sRC;
    ulnAnalyzeStmt *sAnalyzeStmt = NULL;

    ULN_FLAG(sNeedFreeAnalyzeStmt);
    ULN_FLAG(sNeedDestroyMemArea);

    ACI_TEST(aAnalyzeStmt == NULL);

    *aAnalyzeStmt = NULL;

    ACI_TEST_RAISE(aStmtStr == NULL || aStmtStrLen <= 0, NO_NEED_WORK);

    sRC = acpMemAlloc((void**)&sAnalyzeStmt, ACI_SIZEOF(ulnAnalyzeStmt));
    ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));
    ULN_FLAG_UP(sNeedFreeAnalyzeStmt);

    /*
     * aclMemAreaCreate()������ ���� Chunk�� ����� �����ϰ�
     * ���� �Ҵ��� aclMemAreaAlloc()���� �Ѵ�.
     */
    aclMemAreaCreate(&sAnalyzeStmt->mMemArea, 512);
    aclMemAreaGetSnapshot(&sAnalyzeStmt->mMemArea,
                          &sAnalyzeStmt->mMemAreaSnapShot);
    ULN_FLAG_UP(sNeedDestroyMemArea);

    sAnalyzeStmt->mNameCnt   = 0;
    sAnalyzeStmt->mMarkerCnt = 0;

    acpListInit(&sAnalyzeStmt->mTokensList);

    ACI_TEST(ulnAnalyzeStmtStrip(sAnalyzeStmt, aStmtStr, aStmtStrLen));

    *aAnalyzeStmt = sAnalyzeStmt;

    ACI_EXCEPTION_CONT(NO_NEED_WORK);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedDestroyMemArea)
    {
        aclMemAreaDestroy(&sAnalyzeStmt->mMemArea);
    }
    ULN_IS_FLAG_UP(sNeedFreeAnalyzeStmt)
    {
        acpMemFree(sAnalyzeStmt);
        sAnalyzeStmt = NULL;
    }

    return ACI_FAILURE;
}

/**
 *  ulnAnalyzeStmtReInit
 */
ACI_RC ulnAnalyzeStmtReInit(ulnAnalyzeStmt **aAnalyzeStmt,
                            acp_char_t      *aStmtStr,
                            acp_sint32_t     aStmtStrLen)
{
    ulnAnalyzeStmt *sAnalyzeStmt = NULL;

    ACI_TEST_RAISE(aAnalyzeStmt  == NULL, NO_NEED_WORK);
    ACI_TEST_RAISE(*aAnalyzeStmt == NULL, NO_NEED_WORK);
    ACI_TEST_RAISE(aStmtStr == NULL || aStmtStrLen <= 0, NO_NEED_WORK);

    sAnalyzeStmt = *aAnalyzeStmt;

    /* MemArea�� ó�� ��ġ�� ������ */
    aclMemAreaFreeToSnapshot(&sAnalyzeStmt->mMemArea,
                             &sAnalyzeStmt->mMemAreaSnapShot);

    sAnalyzeStmt->mNameCnt   = 0;
    sAnalyzeStmt->mMarkerCnt = 0;

    acpListInit(&sAnalyzeStmt->mTokensList);

    ACI_TEST(ulnAnalyzeStmtStrip(sAnalyzeStmt, aStmtStr, aStmtStrLen));

    ACI_EXCEPTION_CONT(NO_NEED_WORK);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    aclMemAreaDestroy(&sAnalyzeStmt->mMemArea);
    acpMemFree(sAnalyzeStmt);
    *aAnalyzeStmt = NULL;

    return ACI_FAILURE;
}

/**
 *  ulnAnalyzeStmtDestroy
 */
void ulnAnalyzeStmtDestroy(ulnAnalyzeStmt **aAnalyzeStmt)
{
    ACI_TEST(aAnalyzeStmt == NULL);
    ACI_TEST(*aAnalyzeStmt == NULL);

    /* Destroy MemArea */
    aclMemAreaFreeAll(&(*aAnalyzeStmt)->mMemArea);
    aclMemAreaDestroy(&(*aAnalyzeStmt)->mMemArea);

    /* Free AnalyzeStmt */
    acpMemFree(*aAnalyzeStmt);
    *aAnalyzeStmt = NULL;

    ACI_EXCEPTION_END;

    return;
}

/**
 *  ulnAnalyzeStmtStrip
 *
 *  �� �Լ��� ulnPrepare() ���� ȣ��Ǳ� ������
 *  SQL ���� ���� ��Ȳ�� ���� ���� ����� �ʿ�� ����.
 *
 *  = Statement���� :name�� ��ġ�� ����Ʈ�� �����Ѵ�.
 */
ACI_RC ulnAnalyzeStmtStrip(ulnAnalyzeStmt *aAnalyzeStmt,
                           acp_char_t     *aStmtStr,
                           acp_sint32_t    aStmtStrLen)
{
    acp_uint16_t  sNum = 0;
    acp_char_t   *sCurPtr   = NULL;
    acp_char_t   *sFencePtr = NULL;
    acp_char_t   *sTokenPtr = NULL;

    ACI_TEST(aAnalyzeStmt == NULL);

    sCurPtr   = aStmtStr;
    sFencePtr = aStmtStr + aStmtStrLen;

    /* ��ū�� �м��ϰ� TokenList�� �ϳ��� �ִ´�. */
    for ( ; sCurPtr < sFencePtr; sCurPtr++)
    {
        switch (*sCurPtr)
        {
            case '\'' :
                sCurPtr += 1;
                SKIP_QUOTER(sCurPtr, sFencePtr, '\'');
                break;

            case '\"' :
                sCurPtr += 1;
                SKIP_QUOTER(sCurPtr, sFencePtr, '\"');
                break;

            /* BUG-35204 distinguish between the query and the comment */
            case '-' :
                sCurPtr += 1;
                SKIP_COMMENT(sCurPtr, sFencePtr, '-');
                break;

            case '/' :
                sCurPtr += 1;
                SKIP_COMMENT(sCurPtr, sFencePtr, '/');
                break;

            case ':' :
                sCurPtr += 1;
                sTokenPtr = sCurPtr;

                SKIP_IDENTIFIER(sCurPtr, sFencePtr);

                if (sCurPtr - sTokenPtr > 0)
                {
                    sNum += 1;
                    ACI_TEST(ulnAnalyzeStmtSetPosArr(aAnalyzeStmt,
                                                     sTokenPtr,
                                                     sCurPtr - sTokenPtr,
                                                     sNum)
                            );
                }
                else
                {
                    /* Nothing */
                }
                break;

            case '?' :
                sNum += 1;
                break;

            default:
                break;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 *  ulnAnalyzeStmtSetPosArr
 *
 *  ���ο� ��ū�̳� ���� ��ū�� Position�� �����Ѵ�.
 */
static ACI_RC ulnAnalyzeStmtSetPosArr(ulnAnalyzeStmt  *aAnalyzeStmt,
                                      acp_char_t      *aToken,
                                      acp_sint32_t     aTokenLen,
                                      acp_uint16_t     aPos)
{
    acp_rc_t sRC;
    acp_bool_t sIsNewToken = ACP_TRUE;

    acp_list_t       *sNode   = NULL;
    ulnTokenListObj  *sObj    = NULL;
    ulnTokenListObj  *sNewObj = NULL;
    acp_uint16_t     *sPosArr = NULL;
    acp_uint16_t      sSize   = 0;

    ACP_LIST_ITERATE(&aAnalyzeStmt->mTokensList, sNode)
    {
        sObj = sNode->mObj;

        if (sObj->mTokenLen != aTokenLen)
        {
            continue;
        }
        else if (acpCStrCmp(sObj->mToken, aToken, aTokenLen) == 0)
        {
            sIsNewToken = ACP_FALSE;
            break;
        }
        else
        {
            continue;
        }
    }

    /* ���ο� ��ū�̸� List�� �߰��Ѵ� */
    if (sIsNewToken == ACP_TRUE)
    {
        sRC = aclMemAreaAlloc(&aAnalyzeStmt->mMemArea,
                              (void **)&sNewObj,
                              ACI_SIZEOF(ulnTokenListObj));
        ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

        sRC = aclMemAreaAlloc(&aAnalyzeStmt->mMemArea,
                              (void **)&sNewObj->mToken,
                              aTokenLen + 1);
        ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

        acpMemCpy(sNewObj->mToken, aToken, aTokenLen);
        sNewObj->mToken[aTokenLen] = '\0';
        sNewObj->mTokenLen = aTokenLen;

        sSize  = ACI_SIZEOF(*sNewObj->mPosArr);
        sSize *= BASE_ARR_CNT;

        sRC = aclMemAreaAlloc(&aAnalyzeStmt->mMemArea,
                              (void **)&sNewObj->mPosArr,
                              sSize);
        ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

        sNewObj->mPosArr[0] = aPos;
        sNewObj->mPosCnt    = 1;

        sRC = aclMemAreaAlloc(&aAnalyzeStmt->mMemArea,
                              (void **)&sNode,
                              ACI_SIZEOF(acp_list_t));
        ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

        acpListInitObj(sNode, sNewObj);
        acpListAppendNode(&aAnalyzeStmt->mTokensList, sNode);
    }
    else
    {
        /* Position�� ������ ������ �ִ��� Ȯ������ */
        if (sObj->mPosCnt % BASE_ARR_CNT == 0)
        {
            sPosArr = sObj->mPosArr;

            sSize  = ACI_SIZEOF(*sObj->mPosArr);
            sSize *= (sObj->mPosCnt + BASE_ARR_CNT);
            sRC    = aclMemAreaAlloc(&aAnalyzeStmt->mMemArea,
                                     (void **)&sObj->mPosArr,
                                     sSize);
            ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

            sSize  = ACI_SIZEOF(*sObj->mPosArr);
            sSize *= sObj->mPosCnt;
            acpMemCpy(sObj->mPosArr, sPosArr, sSize);
        }
        else
        {
            /* Nothing */
        }

        sObj->mPosArr[sObj->mPosCnt] = aPos;
        sObj->mPosCnt += 1;
    }

    if (aToken[0] == '?')
    {
        aAnalyzeStmt->mMarkerCnt += 1;
    }
    else
    {
        aAnalyzeStmt->mNameCnt += 1;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 *  ulnAnalyzeStmtGetPosArr
 *
 *  aToken�� PosArr�� ������ �� �迭�� �������� ��ȯ�� �ش�.
 */
ACI_RC ulnAnalyzeStmtGetPosArr(ulnAnalyzeStmt  *aAnalyzeStmt,
                               acp_char_t      *aToken,
                               acp_uint16_t   **aPosArr,
                               acp_uint16_t    *aPosCnt)
{
    acp_list_t      *sNode = NULL;
    ulnTokenListObj *sObj  = NULL;
    acp_sint32_t sTokenLen = 0;

    ACI_TEST(aAnalyzeStmt == NULL ||
             aToken       == NULL ||
             aPosArr      == NULL ||
             aPosCnt      == NULL);

    *aPosArr = NULL;
    *aPosCnt = 0;

    /* ?, :name�� statement�� ȥ��� ��� ��� Position ���ε� ���� */
    ACI_TEST(aAnalyzeStmt->mMarkerCnt == 0 && aAnalyzeStmt->mNameCnt == 0);
    ACI_TEST(aAnalyzeStmt->mMarkerCnt > 0 && aAnalyzeStmt->mNameCnt > 0);
    ACI_TEST(aAnalyzeStmt->mMarkerCnt > 0 && aAnalyzeStmt->mNameCnt == 0);

    sTokenLen = acpCStrLen(aToken, ACP_SINT32_MAX);

    ACP_LIST_ITERATE(&aAnalyzeStmt->mTokensList, sNode)
    {
        sObj = sNode->mObj;

        if (sObj->mTokenLen != sTokenLen)
        {
            continue;
        }
        else if (acpCStrCmp(sObj->mToken, aToken, sTokenLen) == 0)
        {
            *aPosArr = sObj->mPosArr;
            *aPosCnt = sObj->mPosCnt;
            break;
        }
        else
        {
            continue;
        }
    }

    ACI_TEST(*aPosArr == NULL);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
