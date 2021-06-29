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

#include <cmAll.h>


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
    UShort          mSlotID;
    UShort          mNextFreeSlotID;
    cmmSession     *mSession;
} cmmSessionSlot;

typedef struct cmmSessionPage
{
    UShort          mPageID;
    UShort          mSlotUseCount;
    UShort          mFirstFreeSlotID;
    UShort          mNextFreePageID;
    cmmSessionSlot *mSlot;
} cmmSessionPage;

typedef struct cmmSessionPageTable
{
    UShort          mFirstFreePageID;
    iduMutex        mMutex;
    cmmSessionPage  mPage[CMM_SESSION_ID_PAGE_MAX];
} cmmSessionPageTable;


static cmmSessionPageTable gPageTable;


static IDE_RC cmmSessionAllocPage(cmmSessionPage *aPage)
{
    /*
     * Page�� Slot Array�� �Ҵ�Ǿ� ���� ������ �Ҵ��ϰ� �ʱ�ȭ
     */

    if (aPage->mSlot == NULL)
    {
        UShort sSlotID;

        IDU_FIT_POINT_RAISE( "cmmSession::cmmSessionAllocPage::malloc::Slot",
                              InsufficientMemory );
        /*
         * Slot Array �Ҵ�
         */

        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_CMM,
                                         ID_SIZEOF(cmmSessionSlot) * CMM_SESSION_ID_SLOT_MAX,
                                         (void **)&(aPage->mSlot),
                                         IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );

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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }    
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

static IDE_RC cmmSessionFreePage(cmmSessionPage *aPage)
{
    /*
     * Page�� Slot�� �ϳ��� ���ǰ� ���� ������ Slot Array�� ����
     */

    if ((aPage->mPageID != 0) && (aPage->mSlot != NULL) && (aPage->mSlotUseCount == 0))
    {
        IDE_TEST(iduMemMgr::free(aPage->mSlot) != IDE_SUCCESS);

        aPage->mFirstFreeSlotID = 0;
        aPage->mSlot            = NULL;
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

static IDE_RC cmmSessionFindSlot(cmmSessionPage **aPage, cmmSessionSlot **aSlot, UShort aSessionID)
{
    UShort sPageID = CMM_SESSION_ID_PAGE(aSessionID);
    UShort sSlotID = CMM_SESSION_ID_SLOT(aSessionID);

    /*
     * Page �˻�
     */

    *aPage = &(gPageTable.mPage[sPageID]);

    /*
     * Page�� Slot Array�� �Ҵ�Ǿ� �ִ��� �˻�
     */

    IDE_TEST((*aPage)->mSlot == NULL);

    /*
     * Slot �˻�
     */

    *aSlot = &((*aPage)->mSlot[sSlotID]);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC cmmSessionInitializeStatic()
{
    cmmSession sSession;
    UShort     sPageID;

    /*
     * cmmSession�� mSessionID ��� ũ�Ⱑ CMM_SESSION_ID_SIZE_BIT�� ��ġ�ϴ��� �˻�
     */
    IDE_ASSERT((ID_SIZEOF(sSession.mSessionID) * 8) == CMM_SESSION_ID_SIZE_BIT);

    /*
     * PageTable �ʱ�ȭ
     */

    gPageTable.mFirstFreePageID = 0;

    IDE_TEST(gPageTable.mMutex.initialize((SChar *)"CMM_SESSION_MUTEX",
                                          IDU_MUTEX_KIND_NATIVE,
                                          IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS);

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

    IDE_TEST(cmmSessionAllocPage(&(gPageTable.mPage[0])) != IDE_SUCCESS);

    /*
     * SessionID = 0 ���� Session�� �Ҵ��� �� ����
     */

    gPageTable.mPage[0].mSlotUseCount    = 1;
    gPageTable.mPage[0].mFirstFreeSlotID = 1;

#ifdef CMM_SESSION_VERIFY
    cmmSessionVerify();
#endif

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmmSessionFinalizeStatic()
{
    UShort sPageID;

    /*
     * Page���� Slot Array �޸𸮸� ����
     */

    for (sPageID = 0; sPageID < CMM_SESSION_ID_PAGE_MAX; sPageID++)
    {
        if (gPageTable.mPage[sPageID].mSlot != NULL)
        {
            IDE_TEST(iduMemMgr::free(gPageTable.mPage[sPageID].mSlot)!=IDE_SUCCESS);

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

    IDE_TEST(gPageTable.mMutex.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmmSessionAdd(cmmSession *aSession)
{
    UShort          sPageID;
    UShort          sSlotID;
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    IDE_ASSERT( gPageTable.mMutex.lock(NULL /* idvSQL* */)
                == IDE_SUCCESS );

    /*
     * �Ҵ簡���� Page�� ������ ����
     */

    IDE_TEST_RAISE(gPageTable.mFirstFreePageID == CMM_SESSION_ID_PAGE_MAX, SessionLimitReach);

    /*
     * �Ҵ簡���� Page �˻�
     */

    sPageID = gPageTable.mFirstFreePageID;
    sPage   = &(gPageTable.mPage[sPageID]);

    /*
     * Page �޸� �Ҵ�
     */

    IDE_TEST(cmmSessionAllocPage(sPage) != IDE_SUCCESS);

    /*
     * Page���� �� Slot �˻�
     */

    sSlotID = sPage->mFirstFreeSlotID;
    sSlot   = &(sPage->mSlot[sSlotID]);

    /*
     * Slot�� ����ִ��� Ȯ��
     */

    IDE_ASSERT(sSlot->mSession == NULL);

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

    IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SessionLimitReach);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SESSION_LIMIT_REACH));
    }
    IDE_EXCEPTION_END;
    {
#ifdef CMM_SESSION_VERIFY
        cmmSessionVerify();
#endif
    }

    IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);

    return IDE_FAILURE;
}

IDE_RC cmmSessionRemove(cmmSession *aSession)
{
    UShort          sPageID;
    UShort          sSlotID;
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    IDE_ASSERT(gPageTable.mMutex.lock(NULL /* idvSQL* */)
               == IDE_SUCCESS);

    /*
     * Page �� Slot �˻�
     */

    IDE_TEST_RAISE(cmmSessionFindSlot(&sPage, &sSlot, aSession->mSessionID) != IDE_SUCCESS, SessionNotFound);

    sPageID = sPage->mPageID;
    sSlotID = sSlot->mSlotID;

    /*
     * Slot�� ����� Session�� ��ġ�ϴ��� �˻�
     */

    IDE_TEST_RAISE(sSlot->mSession != aSession, InvalidSession);

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
            IDE_TEST(cmmSessionFreePage(&gPageTable.mPage[sPage->mNextFreePageID]) != IDE_SUCCESS);
        }
    }

    /*
     * Page�� empty�̰� First FreePage�� �ƴϸ� Page �޸� ����
     */

    if ((sPageID != 0) && (sPage->mSlotUseCount == 0) && (gPageTable.mFirstFreePageID != sPageID))
    {
        IDE_TEST(cmmSessionFreePage(sPage) != IDE_SUCCESS);
    }

#ifdef CMM_SESSION_VERIFY
    cmmSessionVerify();
#endif

    IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(SessionNotFound);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SESSION_DOES_NOT_EXIST));
    }
    IDE_EXCEPTION(InvalidSession);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INVALID_SESSION));
    }
    IDE_EXCEPTION_END;
    {
#ifdef CMM_SESSION_VERIFY
        cmmSessionVerify();
#endif

        IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

IDE_RC cmmSessionFind(cmmSession **aSession, UShort aSessionID)
{
    cmmSessionPage *sPage;
    cmmSessionSlot *sSlot;

    IDE_ASSERT(gPageTable.mMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);

    /*
     * Page �� Slot �˻�
     */

    IDE_TEST(cmmSessionFindSlot(&sPage, &sSlot, aSessionID) != IDE_SUCCESS);

    /*
     * Slot�� Session�� �����ϴ��� �˻�
     */

    IDE_TEST(sSlot->mSession == NULL);

    /*
     * Slot�� Session�� ������
     */

    *aSession = sSlot->mSession;

    IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_SESSION_DOES_NOT_EXIST));

        IDE_ASSERT(gPageTable.mMutex.unlock() == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}
