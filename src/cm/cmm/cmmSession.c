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

#include <cmAllClient.h>


/*
 * cmmSession ��ü�� �����ϴ� ����
 *
 * Slot      : cmmSession ��ü �����Ϳ� Next Free SessionID�� ������ ����ü
 * Page      : SessionSlot�� CMM_SESSION_ID_SLOT_MAX����ŭ ������ �޸� ��
 * PageTable : SessionPage�� CMM_SESSION_ID_PAGE_MAX����ŭ ������ �޸� ��
 *
 *   +------+      +------+------+----
 *   | Page | ---> | Slot | Slot |
 *   +------+      +------+------+----
 *   | Page |
 *   +------+
 *   |      |
 *
 *   PageTable
 */

#define CMM_SESSION_ID_SIZE_BIT  16
#define CMM_SESSION_ID_PAGE_BIT  7
#define CMM_SESSION_ID_SLOT_BIT  (CMM_SESSION_ID_SIZE_BIT - CMM_SESSION_ID_PAGE_BIT)

#define CMM_SESSION_ID_PAGE_MAX  (1 << CMM_SESSION_ID_PAGE_BIT)
#define CMM_SESSION_ID_SLOT_MAX  (1 << CMM_SESSION_ID_SLOT_BIT)

#define CMM_SESSION_ID_PAGE_MASK ((CMM_SESSION_ID_PAGE_MAX - 1) << CMM_SESSION_ID_SLOT_BIT)
#define CMM_SESSION_ID_SLOT_MASK (CMM_SESSION_ID_SLOT_MAX - 1)

#define CMM_SESSION_ID_PAGE(aSessionID) (((aSessionID) >> CMM_SESSION_ID_SLOT_BIT) & (CMM_SESSION_ID_PAGE_MAX - 1))
#define CMM_SESSION_ID_SLOT(aSessionID) ((aSessionID) & CMM_SESSION_ID_SLOT_MASK)

#define CMM_SESSION_ID(aPage, aSlot)    (((aPage) << CMM_SESSION_ID_SLOT_BIT) | ((aSlot) & CMM_SESSION_ID_SLOT_MASK))


typedef struct cmmSessionSlot
{
    acp_uint16_t    mSlotID;
    acp_uint16_t    mNextFreeSlotID;
    cmmSession     *mSession;
} cmmSessionSlot;

typedef struct cmmSessionPage
{
    acp_uint16_t    mPageID;
    acp_uint16_t    mSlotUseCount;
    acp_uint16_t    mFirstFreeSlotID;
    acp_uint16_t    mNextFreePageID;
    cmmSessionSlot *mSlot;
} cmmSessionPage;

typedef struct cmmSessionPageTable
{
    acp_uint16_t    mFirstFreePageID;
    acp_thr_mutex_t mMutex;
    cmmSessionPage  mPage[CMM_SESSION_ID_PAGE_MAX];
} cmmSessionPageTable;


static cmmSessionPageTable gPageTable;


static ACI_RC cmmSessionAllocPage(cmmSessionPage *aPage)
{
    /*
     * Page�� Slot Array�� �Ҵ�Ǿ� ���� ������ �Ҵ��ϰ� �ʱ�ȭ
     */

    if (aPage->mSlot == NULL)
    {
        acp_uint16_t sSlotID;

        /*
         * Slot Array �Ҵ�
         */

        ACI_TEST(acpMemAlloc((void **)&(aPage->mSlot),
                             ACI_SIZEOF(cmmSessionSlot) * CMM_SESSION_ID_SLOT_MAX)
                 != ACP_RC_SUCCESS);

        /*
         * �� Slot �ʱ�ȭ
         */

        for (sSlotID = 0; sSlotID < CMM_SESSION_ID_SLOT_MAX; sSlotID++)
        {
            aPage->mSlot[sSlotID].mSlotID         = sSlotID;
            aPage->mSlot[sSlotID].mNextFreeSlotID = sSlotID + 1;
            aPage->mSlot[sSlotID].mSession        = NULL;
        }

        /*
         * Page�� Slot �Ӽ��� �ʱ�ȭ
         */

        aPage->mSlotUseCount    = 0;
        aPage->mFirstFreeSlotID = 0;
    }

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

static ACI_RC cmmSessionFreePage(cmmSessionPage *aPage)
{
    /*
     * Page�� Slot�� �ϳ��� ���ǰ� ���� ������ Slot Array�� ����
     */

    if ((aPage->mPageID != 0) && (aPage->mSlot != NULL) && (aPage->mSlotUseCount == 0))
    {
        acpMemFree(aPage->mSlot);

        aPage->mFirstFreeSlotID = 0;
        aPage->mSlot            = NULL;
    }

    return ACI_SUCCESS;
}

static ACI_RC cmmSessionFindSlot(cmmSessionPage **aPage, cmmSessionSlot **aSlot, acp_uint16_t aSessionID)
{
    acp_uint16_t sPageID = CMM_SESSION_ID_PAGE(aSessionID);
    acp_uint16_t sSlotID = CMM_SESSION_ID_SLOT(aSessionID);

    /*
     * Page �˻�
     */

    *aPage = &(gPageTable.mPage[sPageID]);

    /*
     * Page�� Slot Array�� �Ҵ�Ǿ� �ִ��� �˻�
     */

    ACI_TEST((*aPage)->mSlot == NULL);

    /*
     * Slot �˻�
     */

    *aSlot = &((*aPage)->mSlot[sSlotID]);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}


ACI_RC cmmSessionInitializeStatic()
{
    cmmSession   sSession;
    acp_uint16_t sPageID;

    /*
     * cmmSession�� mSessionID ��� ũ�Ⱑ CMM_SESSION_ID_SIZE_BIT�� ��ġ�ϴ��� �˻�
     */
    ACE_ASSERT((ACI_SIZEOF(sSession.mSessionID) * 8) == CMM_SESSION_ID_SIZE_BIT);

    /*
     * PageTable �ʱ�ȭ
     */

    gPageTable.mFirstFreePageID = 0;

    ACI_TEST(acpThrMutexCreate(&gPageTable.mMutex, ACP_THR_MUTEX_DEFAULT)
             != ACP_RC_SUCCESS);

    /*
     * �� Page �ʱ�ȭ
     */

    for (sPageID = 0; sPageID < CMM_SESSION_ID_PAGE_MAX; sPageID++)
    {
        gPageTable.mPage[sPageID].mPageID          = sPageID;
        gPageTable.mPage[sPageID].mSlotUseCount    = 0;
        gPageTable.mPage[sPageID].mFirstFreeSlotID = 0;
        gPageTable.mPage[sPageID].mNextFreePageID  = sPageID + 1;
        gPageTable.mPage[sPageID].mSlot            = NULL;
    }

    /*
     * ù��° Page �̸� �Ҵ�
     */

    ACI_TEST(cmmSessionAllocPage(&(gPageTable.mPage[0])) != ACI_SUCCESS);

    /*
     * SessionID = 0 ���� Session�� �Ҵ��� �� ����
     */

    gPageTable.mPage[0].mSlotUseCount    = 1;
    gPageTable.mPage[0].mFirstFreeSlotID = 1;

#ifdef CMM_SESSION_VERIFY
    cmmSessionVerify();
#endif

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmmSessionFinalizeStatic()
{
    acp_uint16_t sPageID;

    /*
     * Page���� Slot Array �޸𸮸� ����
     */

    for (sPageID = 0; sPageID < CMM_SESSION_ID_PAGE_MAX; sPageID++)
    {
        if (gPageTable.mPage[sPageID].mSlot != NULL)
        {
            acpMemFree(gPageTable.mPage[sPageID].mSlot);

            gPageTable.mPage[sPageID].mSlotUseCount    = 0;
            gPageTable.mPage[sPageID].mFirstFreeSlotID = 0;
            gPageTable.mPage[sPageID].mNextFreePageID  = 0;
            gPageTable.mPage[sPageID].mSlot            = NULL;
        }
    }

    /*
     * PageTable ����
     */

    gPageTable.mFirstFreePageID = 0;

    ACI_TEST(acpThrMutexDestroy(&gPageTable.mMutex) != ACP_RC_SUCCESS);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmmSessionAdd(cmmSession *aSession)
{
    acp_uint16_t    sPageID;
    acp_uint16_t    sSlotID;
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    ACE_ASSERT(acpThrMutexLock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    /*
     * �Ҵ簡���� Page�� ������ ����
     */

    ACI_TEST_RAISE(gPageTable.mFirstFreePageID == CMM_SESSION_ID_PAGE_MAX, SessionLimitReach);

    /*
     * �Ҵ簡���� Page �˻�
     */

    sPageID = gPageTable.mFirstFreePageID;
    sPage   = &(gPageTable.mPage[sPageID]);

    /*
     * Page �޸� �Ҵ�
     */

    ACI_TEST(cmmSessionAllocPage(sPage) != ACI_SUCCESS);

    /*
     * Page���� �� Slot �˻�
     */

    sSlotID = sPage->mFirstFreeSlotID;
    sSlot   = &(sPage->mSlot[sSlotID]);

    /*
     * Slot�� ����ִ��� Ȯ��
     */

    ACE_ASSERT(sSlot->mSession == NULL);

    /*
     * Session �߰�
     */

    sSlot->mSession      = aSession;
    aSession->mSessionID = CMM_SESSION_ID(sPageID, sSlotID);

    /*
     * UsedSlotCount ����
     */

    sPage->mSlotUseCount++;

    /*
     * FreeSlot List ����
     */

    sPage->mFirstFreeSlotID = sSlot->mNextFreeSlotID;
    sSlot->mNextFreeSlotID  = CMM_SESSION_ID_SLOT_MAX;

    /*
     * Page�� full�̸� FreePage List ����
     */

    if (sPage->mFirstFreeSlotID == CMM_SESSION_ID_SLOT_MAX)
    {
        gPageTable.mFirstFreePageID = sPage->mNextFreePageID;
        sPage->mNextFreePageID      = CMM_SESSION_ID_PAGE_MAX;
    }

#ifdef CMM_SESSION_VERIFY
    cmmSessionVerify();
#endif

    ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(SessionLimitReach);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SESSION_LIMIT_REACH));
    }
    ACI_EXCEPTION_END;
    {
#ifdef CMM_SESSION_VERIFY
        cmmSessionVerify();
#endif
    }

    ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    return ACI_FAILURE;
}

ACI_RC cmmSessionRemove(cmmSession *aSession)
{
    acp_uint16_t    sPageID;
    acp_uint16_t    sSlotID;
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    ACE_ASSERT(acpThrMutexLock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    /*
     * Page �� Slot �˻�
     */

    ACI_TEST_RAISE(cmmSessionFindSlot(&sPage, &sSlot, aSession->mSessionID) != ACI_SUCCESS, SessionNotFound);

    sPageID = sPage->mPageID;
    sSlotID = sSlot->mSlotID;

    /*
     * Slot�� ����� Session�� ��ġ�ϴ��� �˻�
     */

    ACI_TEST_RAISE(sSlot->mSession != aSession, InvalidSession);

    /*
     * Session ����
     */

    sSlot->mSession      = NULL;
    aSession->mSessionID = 0;

    /*
     * UsedSlotCount ����
     */

    sPage->mSlotUseCount--;

    /*
     * FreeSlot List ����
     */

    sSlot->mNextFreeSlotID  = sPage->mFirstFreeSlotID;
    sPage->mFirstFreeSlotID = sSlotID;

    /*
     * Page�� full�̾����� FreePage List ����
     */

    if (sPage->mSlotUseCount == (CMM_SESSION_ID_SLOT_MAX - 1))
    {
        sPage->mNextFreePageID      = gPageTable.mFirstFreePageID;
        gPageTable.mFirstFreePageID = sPageID;

        /*
         * Next FreePage�� empty�̸� Page �޸� ����
         */

        if (gPageTable.mPage[sPage->mNextFreePageID].mSlotUseCount == 0)
        {
            ACI_TEST(cmmSessionFreePage(&gPageTable.mPage[sPage->mNextFreePageID]) != ACI_SUCCESS);
        }
    }

    /*
     * Page�� empty�̰� First FreePage�� �ƴϸ� Page �޸� ����
     */

    if ((sPageID != 0) && (sPage->mSlotUseCount == 0) && (gPageTable.mFirstFreePageID != sPageID))
    {
        ACI_TEST(cmmSessionFreePage(sPage) != ACI_SUCCESS);
    }

#ifdef CMM_SESSION_VERIFY
    cmmSessionVerify();
#endif

    ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(SessionNotFound);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SESSION_DOES_NOT_EXIST));
    }
    ACI_EXCEPTION(InvalidSession);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INVALID_SESSION));
    }
    ACI_EXCEPTION_END;
    {
#ifdef CMM_SESSION_VERIFY
        cmmSessionVerify();
#endif

        ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}

ACI_RC cmmSessionFind(cmmSession **aSession, acp_uint16_t aSessionID)
{
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    ACE_ASSERT(acpThrMutexLock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    /*
     * Page �� Slot �˻�
     */

    ACI_TEST(cmmSessionFindSlot(&sPage, &sSlot, aSessionID) != ACI_SUCCESS);

    /*
     * Slot�� Session�� �����ϴ��� �˻�
     */

    ACI_TEST(sSlot->mSession == NULL);

    /*
     * Slot�� Session�� ������
     */

    *aSession = sSlot->mSession;

    ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_SESSION_DOES_NOT_EXIST));
        ACE_ASSERT(acpThrMutexUnlock(&gPageTable.mMutex) == ACP_RC_SUCCESS);
    }

    return ACI_FAILURE;
}
