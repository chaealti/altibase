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
 

/***********************************************************************
 * $Id: ulaTransTbl.c 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <acp.h>
#include <acl.h>
#include <ace.h>

#include <ulaTransTbl.h>

static acp_uint32_t ulaTransTblGetTransSlotID(ulaTransTbl *aTbl, ulaTID aTID)
{
    return (aTID % aTbl->mTblSize);
}


static ulaTransTblNode *ulaTransTblGetTrNode(ulaTransTbl *aTbl, ulaTID aTID)
{
    acp_uint32_t sTransSlotID = ulaTransTblGetTransSlotID(aTbl, aTID);

    if (aTbl->mTransTbl != NULL)
    {
        return &(aTbl->mTransTbl[sTransSlotID]);
    }
    else
    {
        return NULL;
    }
}

ulaTransTblNode *ulaTransTblGetTrNodeDirect(ulaTransTbl  *aTbl,
                                            acp_uint32_t  aIndex)
{
    ACE_DASSERT(aIndex < aTbl->mTblSize);
    return (aTbl->mTransTbl != NULL) ? &(aTbl->mTransTbl[aIndex]) : NULL;
}

acp_uint32_t ulaTransTblGetTableSize(ulaTransTbl *aTbl)
{
    return aTbl->mTblSize;
}

ulaXLogLinkedList *ulaTransTblGetCollectionList(ulaTransTbl *aTbl, ulaTID aTID)
{
    acp_uint32_t sTransSlotID = ulaTransTblGetTransSlotID(aTbl, aTID);

    if (aTbl->mTransTbl != NULL)
    {
        return &(aTbl->mTransTbl[sTransSlotID].mCollectionList);
    }
    else
    {
        return NULL;
    }
}

acp_bool_t ulaTransTblIsATransNode(ulaTransTblNode *aNode)
{
    ACE_ASSERT(aNode != NULL);
    return (aNode->mBeginSN != ULA_SN_NULL) ? ACP_TRUE : ACP_FALSE;
}

acp_bool_t ulaTransTblIsATrans(ulaTransTbl *aTbl, ulaTID aTID)
{
    ACE_DASSERT(aTbl->mTransTbl != NULL);
    return ulaTransTblIsATransNode(ulaTransTblGetTrNode(aTbl, aTID));
}

/***********************************************************************
 * Description : ulaTransTblNode �ʱ�ȭ
 *
 * aTransNode  - [IN] �ʱ�ȭ�� ������ ulaTransTblNode
 *
 **********************************************************************/
static void ulaTransTblInitTransNode(ulaTransTblNode *aTransNode)
{
    aTransNode->mTID           = ACP_UINT32_MAX;
    aTransNode->mBeginSN       = ULA_SN_NULL;
    aTransNode->mLocHead.mNext = NULL;
}

/***********************************************************************
 * Description : Transaction Table �ʱ�ȭ
 *
 * aTblSize - [IN] Log Anallyzer ���� Transaction Table ũ��
 *
 **********************************************************************/
ACI_RC ulaTransTblInitialize(ulaTransTbl  *aTbl,
                             acp_uint32_t  aTblSize,
                             acp_bool_t    aUseCollectionList,
                             ulaErrorMgr  *aOutErrorMgr)
{
    acp_rc_t     sRc;
    acp_sint32_t sIndex = 0;
    acp_uint32_t sStage = 0;

    aTbl->mATransCnt         = 0;
    aTbl->mTblSize           = aTblSize;
    aTbl->mTransTbl          = NULL;
    aTbl->mUseCollectionList = aUseCollectionList;

    sStage = 1;
    if (aTbl->mTblSize != 0)
    {
        sRc = acpMemAlloc((void **)&aTbl->mTransTbl,
                          aTbl->mTblSize * ACI_SIZEOF(ulaTransTblNode));
        ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ALA_ERR_MEM_ALLOC);
    }

    sStage = 2;
    for (sIndex = 0; sIndex < (acp_sint32_t)aTbl->mTblSize; sIndex++)
    {
        ulaTransTblInitTransNode(aTbl->mTransTbl + sIndex);

        /* XLog Linked List�� �ʱ�ȭ�Ѵ�. (Transaction ���� �ÿ��� ���) */
        sRc = ulaXLogLinkedListInitialize
                                    (&aTbl->mTransTbl[sIndex].mCollectionList,
                                     ACP_FALSE,
                                     aOutErrorMgr);
    }

    sStage = 3;
    sRc = acpThrMutexCreate(&aTbl->mTransTblNodeMutex, ACP_THR_MUTEX_DEFAULT);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_INIT);

    sStage = 4;
    sRc = acpThrMutexCreate(&aTbl->mCollectionListMutex, ACP_THR_MUTEX_DEFAULT);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_INIT);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_INIT)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_INITIALIZE,
                        "Transaction Table Mutex");
    }
    ACI_EXCEPTION(ALA_ERR_MEM_ALLOC)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_MEMORY_ALLOC);
    }
    ACI_EXCEPTION_END;

    switch (sStage)
    {
        case 4 :
            (void)acpThrMutexDestroy(&aTbl->mTransTblNodeMutex);

        case 3 :
        case 2 :
            for (sIndex--; sIndex >= 0; sIndex--)
            {
                (void)ulaXLogLinkedListDestroy
                            (&aTbl->mTransTbl[sIndex].mCollectionList, NULL);
            }

        case 1 :
            if (aTbl->mTransTbl != NULL)
            {
                acpMemFree(aTbl->mTransTbl);
                aTbl->mTransTbl = NULL;
            }
            break;

        default :
            break;
    }

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : Transaction Table ����
 *
 **********************************************************************/
ACI_RC ulaTransTblDestroy(ulaTransTbl *aTbl, ulaErrorMgr *aOutErrorMgr)
{
    acp_rc_t     sRc;
    acp_uint32_t sIndex;
    acp_uint32_t sStage = 0;

    for (sIndex = 0; sIndex < aTbl->mTblSize; sIndex++)
    {
        /* Active Transaction�� �����Ѵ�. */
        if (ulaTransTblIsATransNode(aTbl->mTransTbl + sIndex) == ACP_TRUE)
        {
            sStage = 1;
            ACI_TEST(ulaTransTblRemoveTrans(aTbl,
                                            aTbl->mTransTbl[sIndex].mTID,
                                            aOutErrorMgr)
                     != ACI_SUCCESS);
        }

        /* XLog Linked List�� �����Ѵ�. (Transaction ���� �ÿ��� ���) */
        sStage = 2;
        ACI_TEST(ulaXLogLinkedListDestroy
                                (&aTbl->mTransTbl[sIndex].mCollectionList,
                                 aOutErrorMgr)
                 != ACI_SUCCESS);
    }
    sStage = 3;

    ACE_DASSERT(aTbl->mATransCnt == 0);

    /* Transaction Table�� �Ҵ�� ��� Memory�� �����Ѵ�. */
    if (aTbl->mTransTbl != NULL)
    {
        acpMemFree(aTbl->mTransTbl);
        aTbl->mTransTbl = NULL;
    }

    sRc = acpThrMutexDestroy(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_DESTROY);
    sStage = 4;

    sRc = acpThrMutexDestroy(&aTbl->mCollectionListMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_DESTROY);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_DESTROY)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_DESTROY,
                        "Transaction Table Mutex");
    }
    ACI_EXCEPTION_END;

    switch (sStage)
    {
        case 1 :
            if (aTbl->mTransTbl != NULL)
            {
                (void)ulaXLogLinkedListDestroy
                            (&aTbl->mTransTbl[sIndex].mCollectionList, NULL);
            }

        case 2 :
            for (sIndex++; sIndex < aTbl->mTblSize; sIndex++)
            {
                if (ulaTransTblIsATransNode(aTbl->mTransTbl + sIndex)
                    == ACP_TRUE)
                {
                    sStage = 1;
                    (void)ulaTransTblRemoveTrans(aTbl,
                                                 aTbl->mTransTbl[sIndex].mTID,
                                                 NULL);
                }

                (void)ulaXLogLinkedListDestroy
                            (&aTbl->mTransTbl[sIndex].mCollectionList, NULL);
            }

            if (aTbl->mTransTbl != NULL)
            {
                acpMemFree(aTbl->mTransTbl);
                aTbl->mTransTbl = NULL;
            }

            (void)acpThrMutexDestroy(&aTbl->mTransTblNodeMutex);

        case 3 :
            (void)acpThrMutexDestroy(&aTbl->mCollectionListMutex);
            break;

        default :
            break;
    }

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : aTID�� �ش��ϴ� Transaction Slot�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 *
 * aTID       - [IN] Transaction ID
 * aBeginSN   - [IN] Transaction �� Begin SN
 *
 **********************************************************************/
ACI_RC ulaTransTblInsertTrans(ulaTransTbl *aTbl,
                              ulaTID       aTID,
                              ulaSN        aBeginSN,
                              ulaErrorMgr *aOutErrorMgr)
{
    acp_rc_t     sRc;
    acp_uint32_t sIndex;
    acp_bool_t   sMutexLock = ACP_FALSE;

    ACE_DASSERT(aBeginSN != ULA_SN_NULL);

    /* aTID�� ����� Transaction Slot�� ã�´�. */
    sIndex = ulaTransTblGetTransSlotID(aTbl, aTID);

    sRc = acpThrMutexLock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);
    sMutexLock = ACP_TRUE;

    /* �̹� ��� ������ �˻��Ѵ�. */
    ACI_TEST_RAISE(ulaTransTblIsATransNode(&(aTbl->mTransTbl[sIndex]))
                   == ACP_TRUE,
                   ERR_ALREADY_BEGIN);

    /* TID�� Begin SN�� �����Ѵ�. */
    aTbl->mTransTbl[sIndex].mTID     = aTID;
    aTbl->mTransTbl[sIndex].mBeginSN = aBeginSN;

    /* Active Trans Count�� ������Ų��. */
    aTbl->mATransCnt++;

    sMutexLock = ACP_FALSE;
    sRc = acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_ALREADY_BEGIN)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_TX_ALREADY_BEGIN,
                        aTbl->mTransTbl[sIndex].mTID, aTID);
    }
    ACI_EXCEPTION_END;

    if (sMutexLock == ACP_TRUE)
    {
        (void)acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    }

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : aTID�� �ش��ϴ� Transaction Slot�� ��ȯ�ϰ� ������
 *               member�� ���ؼ� �Ҹ��ڸ� ȣ���Ѵ�.
 *
 * aTID - [IN] ������ Transaction�� ID
 *
 **********************************************************************/
ACI_RC ulaTransTblRemoveTrans(ulaTransTbl *aTbl,
                              ulaTID       aTID,
                              ulaErrorMgr *aOutErrorMgr)
{
    acp_rc_t       sRc;
    acp_uint32_t   sIndex;
    acp_bool_t     sMutexLock = ACP_FALSE;

    /* aTID�� ����� Transaction Slot�� ã�´�. */
    sIndex = ulaTransTblGetTransSlotID(aTbl, aTID);

    sRc = acpThrMutexLock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);
    sMutexLock = ACP_TRUE;

    /* ��� ������ �˻��Ѵ�. */
    ACI_TEST_RAISE(ulaTransTblIsATransNode(&(aTbl->mTransTbl[sIndex]))
                   != ACP_TRUE,
                   ERR_YET_NOT_BEGIN);

    /* Transaction Slot�� �Ҵ�� �޸𸮸� �����Ѵ�. */
    (void)ulaTransTblRemoveAllLocator(aTbl, aTID, aOutErrorMgr);

    /* Transaction Slot�� �ʱ�ȭ�Ѵ�. */
    ulaTransTblInitTransNode(&(aTbl->mTransTbl[sIndex]));

    /* Active Trans Count�� ���ҽ�Ų��. */
    aTbl->mATransCnt--;

    sMutexLock = ACP_FALSE;
    sRc = acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_YET_NOT_BEGIN)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_IGNORE_NOT_ACTIVE_TX, aTID);
    }
    ACI_EXCEPTION_END;

    if (sMutexLock == ACP_TRUE)
    {
        (void)acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    }

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : aTID Ʈ����� Entry�� Remote LOB Locator��
 *               Local LOB Locator�� pair�� �����Ѵ�.
 *
 * aTID      - [IN] Ʈ����� ID
 * aRemoteLL - [IN] ������ Remote LOB Locator
 * aLocalLL  - [IN] ������ Local LOB Locator
 *
 **********************************************************************/
ACI_RC ulaTransTblInsertLocator(ulaTransTbl    *aTbl,
                                ulaTID          aTID,
                                ulaLobLocator   aRemoteLL,
                                ulaLobLocator   aLocalLL,
                                ulaErrorMgr    *aOutErrorMgr)
{
    acp_rc_t         sRc;
    ulaTransTblNode *sTrNode;
    ulaLOBLocEntry  *sLocEntry = NULL;
    acp_bool_t       sMutexLock = ACP_FALSE;

    sTrNode = ulaTransTblGetTrNode(aTbl, aTID);
    ACI_TEST_RAISE(sTrNode == NULL, ERR_NOT_INIT_TX_TBL);

    sRc = acpThrMutexLock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);
    sMutexLock = ACP_TRUE;

    ACI_TEST_RAISE(ulaTransTblIsATransNode(sTrNode) != ACP_TRUE,
                   ERR_NOT_ACTIVE);

    sRc = acpMemAlloc((void **)&sLocEntry, ACI_SIZEOF(ulaLOBLocEntry));
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ALA_ERR_MEM_ALLOC);

    sLocEntry->mRemoteLL    = aRemoteLL;
    sLocEntry->mLocalLL     = aLocalLL;
    sLocEntry->mNext        = sTrNode->mLocHead.mNext;
    sTrNode->mLocHead.mNext = sLocEntry;

    sMutexLock = ACP_FALSE;
    sRc = acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_NOT_INIT_TX_TBL)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_NOT_INIT_TXTABLE);
    }
    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_NOT_ACTIVE)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_IGNORE_NOT_ACTIVE_TX, aTID);
    }
    ACI_EXCEPTION(ALA_ERR_MEM_ALLOC)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_MEMORY_ALLOC);
    }
    ACI_EXCEPTION_END;

    if (sMutexLock == ACP_TRUE)
    {
        (void)acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    }
	
	// BUG-40316, codesonar
	if (sLocEntry != NULL )
	{
        acpMemFree(sLocEntry);
		sLocEntry = NULL;
	}

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : Remote LOB Locator�� Key�� Ʈ����� Entry����
 *               Locator entry�� �����Ѵ�.
 *
 * aTID      - [IN] Ʈ����� ID
 * aRemoteLL - [IN] ������ Remote LOB Locator
 *
 **********************************************************************/
ACI_RC ulaTransTblRemoveLocator(ulaTransTbl   *aTbl,
                                ulaTID         aTID,
                                ulaLobLocator  aRemoteLL,
                                ulaErrorMgr   *aOutErrorMgr)
{
    acp_rc_t         sRc;
    ulaTransTblNode *sTrNode;
    ulaLOBLocEntry  *sPrevEntry;
    ulaLOBLocEntry  *sCurEntry;
    acp_bool_t       sMutexLock = ACP_FALSE;

    sTrNode = ulaTransTblGetTrNode(aTbl, aTID);
    ACI_TEST_RAISE(sTrNode == NULL, ERR_NOT_INIT_TX_TBL);

    sRc = acpThrMutexLock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);
    sMutexLock = ACP_TRUE;

    ACI_TEST_RAISE(ulaTransTblIsATransNode(sTrNode) != ACP_TRUE,
                   ERR_NOT_ACTIVE);

    sPrevEntry = &sTrNode->mLocHead;
    sCurEntry  = sPrevEntry->mNext;

    while (sCurEntry != NULL)
    {
        if (sCurEntry->mRemoteLL == aRemoteLL)
        {
            sPrevEntry->mNext = sCurEntry->mNext;
            acpMemFree(sCurEntry);
            break;
        }
        sPrevEntry = sCurEntry;
        sCurEntry  = sCurEntry->mNext;
    }

    sMutexLock = ACP_FALSE;
    sRc = acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_NOT_INIT_TX_TBL)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_NOT_INIT_TXTABLE);
    }
    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_NOT_ACTIVE)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_IGNORE_NOT_ACTIVE_TX, aTID);
    }
    ACI_EXCEPTION_END;

    if (sMutexLock == ACP_TRUE)
    {
        (void)acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    }

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : Ʈ����� Entry���� ��� Locator entry�� �����Ѵ�.
 *
 * aTID      - [IN] Ʈ����� ID
 *
 **********************************************************************/
ACI_RC ulaTransTblRemoveAllLocator(ulaTransTbl  *aTbl,
                                   ulaTID        aTID,
                                   ulaErrorMgr  *aOutErrorMgr)
{
    ulaTransTblNode *sTrNode;
    ulaLOBLocEntry  *sHeadEntry;
    ulaLOBLocEntry  *sCurEntry;

    sTrNode = ulaTransTblGetTrNode(aTbl, aTID);
    ACI_TEST_RAISE(sTrNode == NULL, ERR_NOT_INIT_TX_TBL);

    ACI_TEST_RAISE(ulaTransTblIsATransNode(sTrNode) != ACP_TRUE,
                   ERR_NOT_ACTIVE);

    sHeadEntry = &sTrNode->mLocHead;
    sCurEntry  = sHeadEntry->mNext;

    while (sCurEntry != NULL)
    {
        sHeadEntry->mNext = sCurEntry->mNext;
        acpMemFree(sCurEntry);

        sCurEntry = sHeadEntry->mNext;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_NOT_INIT_TX_TBL)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_NOT_INIT_TXTABLE);
    }
    ACI_EXCEPTION(ERR_NOT_ACTIVE)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_IGNORE_NOT_ACTIVE_TX, aTID);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : aTID Ʈ����� Entry���� Remote LOB Locator�� Key��
 *               Local LOB Locator�� ��ȯ�Ѵ�.
 *
 * aTID        - [IN]  Ʈ����� ID
 * aRemoteLL   - [IN]  ã�� Remote LOB Locator
 * aOutLocalLL - [OUT] ��ȯ�� Local LOB Locator ���� ����
 * aOutIsFound - [OUT] �˻� ���� ����
 *
 **********************************************************************/
ACI_RC ulaTransTblSearchLocator(ulaTransTbl   *aTbl,
                                ulaTID         aTID,
                                ulaLobLocator  aRemoteLL,
                                ulaLobLocator *aOutLocalLL,
                                acp_bool_t    *aOutIsFound,
                                ulaErrorMgr   *aOutErrorMgr)
{
    acp_rc_t         sRc;
    ulaTransTblNode *sTrNode;
    ulaLOBLocEntry  *sCurEntry;
    acp_bool_t       sMutexLock = ACP_FALSE;

    *aOutIsFound = ACP_FALSE;

    sTrNode = ulaTransTblGetTrNode(aTbl, aTID);
    ACI_TEST_RAISE(sTrNode == NULL, ERR_NOT_INIT_TX_TBL);

    sRc = acpThrMutexLock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);
    sMutexLock = ACP_TRUE;

    ACI_TEST_RAISE(ulaTransTblIsATransNode(sTrNode) != ACP_TRUE,
                   ERR_NOT_ACTIVE);

    sCurEntry = sTrNode->mLocHead.mNext;

    while (sCurEntry != NULL)
    {
        if (sCurEntry->mRemoteLL == aRemoteLL)
        {
            *aOutLocalLL = sCurEntry->mLocalLL;
            *aOutIsFound = ACP_TRUE;
            break;
        }
        sCurEntry = sCurEntry->mNext;
    }

    sMutexLock = ACP_FALSE;
    sRc = acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_NOT_INIT_TX_TBL)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_ABORT_NOT_INIT_TXTABLE);
    }
    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_TRANS_TABLE_NODE_MUTEX_NAME);
    }
    ACI_EXCEPTION(ERR_NOT_ACTIVE)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_IGNORE_NOT_ACTIVE_TX, aTID);
    }
    ACI_EXCEPTION_END;

    if (sMutexLock == ACP_TRUE)
    {
        (void)acpThrMutexUnlock(&aTbl->mTransTblNodeMutex);
    }

    return ACI_FAILURE;
}

ACI_RC ulaTransTblLockCollectionList(ulaTransTbl *aTbl,
                                     ulaErrorMgr *aOutErrorMgr)
{
    acp_rc_t sRc;

    sRc = acpThrMutexLock(&aTbl->mCollectionListMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_LOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_LOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_LOCK,
                        ULA_COLLECTION_LIST_MUTEX_NAME);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulaTransTblUnlockCollectionList(ulaTransTbl *aTbl,
                                       ulaErrorMgr *aOutErrorMgr)
{
    acp_rc_t sRc;

    sRc = acpThrMutexUnlock(&aTbl->mCollectionListMutex);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRc), ERR_MUTEX_UNLOCK);

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_MUTEX_UNLOCK)
    {
        ulaSetErrorCode(aOutErrorMgr, ulaERR_FATAL_MUTEX_UNLOCK,
                        ULA_COLLECTION_LIST_MUTEX_NAME);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
