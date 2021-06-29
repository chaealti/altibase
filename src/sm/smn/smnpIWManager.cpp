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
 * $Id: smnpIWManager.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <ide.h>
#include <smErrorCode.h>

#include <smc.h>
#include <smm.h>
#include <smp.h>
#include <smnDef.h>
#include <smnManager.h>
#include <smnpIWManager.h>
#include <smtPJChild.h>
#include <idu.h>
#include <smtPJMgr.h>

/* ------------------------------------------------
 *  Child Index Loader
 * ----------------------------------------------*/

class smnpIWChild : public smtPJChild
{
    smcTableHeader* mTable;
    smnIndexHeader* mIndex;
    smnIndexFile    mIndexFile;

public:
    static UInt     mIndexCompleteCount;
    static iduMutex mCountMutex;

public:
    smnpIWChild() {}
    static IDE_RC display();

    void setRebuildInfo( smcTableHeader  * aTable,
                         smnIndexHeader  * aIndex)
    {
        mTable = aTable;
        mIndex = aIndex;
    }

    IDE_RC doJob();
};

iduMutex smnpIWChild::mCountMutex;
UInt     smnpIWChild::mIndexCompleteCount;

IDE_RC smnpIWChild::display()
{

    IDE_TEST(mCountMutex.lock(NULL /* idvSQL* */)
             != IDE_SUCCESS);

    mIndexCompleteCount++;

    IDE_TEST(mCountMutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smnpIWChild::doJob()
{

    IDE_TEST(mIndexFile.initialize(mTable->mSelfOID, mIndex->mId)
             != IDE_SUCCESS);

    IDE_TEST(mIndexFile.create() != IDE_SUCCESS);
    IDE_TEST(mIndexFile.open() != IDE_SUCCESS);

    IDE_TEST(mIndex->mModule->mMakeDiskImage(mIndex, &mIndexFile)
             != IDE_SUCCESS);

    IDE_TEST(mIndexFile.close() != IDE_SUCCESS);
    IDE_TEST(mIndexFile.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* ------------------------------------------------
 *  Index Loader Manager
 * ----------------------------------------------*/

class smnpIWChildMgr : public smtPJMgr
{
    SInt              mThreadCount;
    smnpIWChild       **mChildArray;
    smcTableHeader   *mBaseTable;
    SChar            *mCurTablePtr;
    idBool            mSuccess;

    SInt             mIndexBegin;
    SInt             mIndexFence;
    SInt             mIndexCursor;

public:
    smnpIWChildMgr() {}
    IDE_RC initialize(SInt aThreadCount);
    IDE_RC assignJob(SInt    aReqChild,
                     idBool *aJobAssigned);
    IDE_RC destroy();
};

IDE_RC smnpIWChildMgr::initialize( SInt aThreadCount)
{

    SInt i;

    mSuccess = ID_TRUE;

    mThreadCount = aThreadCount;
    mBaseTable   = (smcTableHeader *)smmManager::m_catTableHeader;
    mCurTablePtr = NULL;

    mIndexBegin  = SMN_NULL_POS;
    mIndexFence  = SMN_NULL_POS;
    mIndexCursor = SMN_NULL_POS;

    smnpIWChild::mIndexCompleteCount = 0;

    IDE_TEST(smnpIWChild::mCountMutex.initialize((SChar*)"INDEX_PARALLEL_REBUILD_THR_COUNT_MUTEX",
                                                 IDU_MUTEX_KIND_NATIVE,
                                                 IDV_WAIT_INDEX_NULL)
             != IDE_SUCCESS);

    /* TC/FIT/Limit/sm/smn/smnp/smnpIWChildMgr_initialize_calloc.sql */
    IDU_FIT_POINT_RAISE( "smnpIWChildMgr::initialize::calloc",
                          insufficient_memory );

    // client �޸� �Ҵ�
    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_SM_SMN,
                               aThreadCount,
                               ID_SIZEOF(smnpIWChild*),
                               (void**)&mChildArray) != IDE_SUCCESS,
                   insufficient_memory );

    for (i = 0; i < aThreadCount; i++)
    {
        /* TC/FIT/Limit/sm/smn/smnp/smnpIWChildMgr_initialize_malloc.sql */
        IDU_FIT_POINT_RAISE( "smnpIWChildMgr::initialize::malloc",
                              insufficient_memory );

        IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMN,
                                   ID_SIZEOF(smnpIWChild),
                                   (void**)&(mChildArray[i])) != IDE_SUCCESS,
                       insufficient_memory );

        mChildArray[i] = new (mChildArray[i]) smnpIWChild();
    }

    // �Ŵ��� �ʱ�ȭ
    IDE_TEST(smtPJMgr::initialize(aThreadCount,
                                  (smtPJChild **)mChildArray,
                                  &mSuccess)
                     != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smnpIWChildMgr::destroy()
{
    SInt i;

    IDE_ASSERT(smtPJMgr::destroy() == IDE_SUCCESS);
    IDE_ASSERT(smnpIWChild::mCountMutex.destroy() == IDE_SUCCESS);

    for (i = 0; i < mThreadCount; i++)
    {
        IDE_TEST(mChildArray[i]->destroy() != IDE_SUCCESS);

        IDE_TEST(iduMemMgr::free(mChildArray[i])
                 != IDE_SUCCESS);
    }

    IDE_TEST(iduMemMgr::free(mChildArray)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smnpIWChildMgr::assignJob(SInt    aReqChild,
                               idBool *aJobAssigned)
{
    SChar          *sNextTablePtr;
    smcTableHeader *sCurTable;
    smnIndexHeader *sIndexHeader;
    smpSlotHeader  *sSlot;
    UInt            i;
    UInt            sIndexCount;
    smOID           sOID;

    *aJobAssigned = ID_FALSE;

    while (ID_TRUE)
    {
        if(mIndexCursor != SMN_NULL_POS)
        {
            mIndexCursor--;
        }

        if( (mIndexCursor != mIndexBegin) && (mIndexCursor != SMN_NULL_POS) )
        {

            sCurTable = (smcTableHeader *)(mCurTablePtr + sizeof(smpSlotHeader));
            sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex(sCurTable,mIndexCursor);


            if((sIndexHeader->mFlag & SMI_INDEX_PERSISTENT_MASK) == SMI_INDEX_PERSISTENT_DISABLE)
            {
                continue;
            }

            if((sIndexHeader->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_DISABLE)
            {
                continue;
            }

            mChildArray[aReqChild]->setRebuildInfo(sCurTable, sIndexHeader);
            *aJobAssigned = ID_TRUE;

            break;
        }

        // added for A4, fix BUG-7950
        // disk table�� ���ؼ���
        // persistent index write�� skip�ϵ���
        // �Ѵ�.
        while (1)
        {
            IDE_TEST( smcRecord::nextOIDall( mBaseTable,
                                             mCurTablePtr,
                                             &sNextTablePtr )
                      != IDE_SUCCESS);

            if( sNextTablePtr == NULL )
            {
                break;
            }

            sSlot  = (smpSlotHeader *)sNextTablePtr;
            sCurTable = (smcTableHeader *)(sSlot + 1);
            mCurTablePtr = sNextTablePtr;

            sOID = SMP_SLOT_GET_NEXT_OID( sSlot );
            IDE_ASSERT(SM_IS_NULL_OID(sOID));

            //disk table�� �ƴϸ� break.
            if( ( SMI_TABLE_TYPE_IS_DISK( sCurTable )     == ID_FALSE ) &&
                ( SMI_TABLE_TYPE_IS_VOLATILE( sCurTable ) == ID_FALSE ) )
            {
                break;
            }
        }//while (1)

        if (sNextTablePtr != NULL)
        {
            sIndexCount = smcTable::getIndexCount(sCurTable);

            if( SMP_SLOT_IS_NOT_DROP( sSlot ) )
            {
                // table header�� versioning���� �ʴ´�.
                IDE_ASSERT(SM_SCN_IS_NOT_DELETED(sSlot->mCreateSCN));

                while(1)
                {

                    // ���̺��� ������ �� NULL ROW�� �⺻������ ���Ƿ�
                    // ��� ���̺��� �ּ��� �ϳ��� ROW�� ���ϰ� �ȴ�.
                    // ��, �ε����� �����ϱ� ����
                    // ��� ���̺� ���� ROW������ ���� �ʿ䰡 ����.
                    if(sIndexCount !=0 )
                    {
                        mIndexBegin  = -1;
                        mIndexFence  = sIndexCount;
                        mIndexCursor = mIndexFence;
                        break;
                    }

                    for(i = 0; i < sIndexCount; i++)
                    {
                        IDE_CALLBACK_SEND_SYM(".");
                    }

                    mIndexBegin = SMN_NULL_POS;
                    mIndexFence = SMN_NULL_POS;
                    mIndexCursor = SMN_NULL_POS;

                    break;
                }
            }//if
            else
            {
                /* BUG-30378 ������������ Drop�Ǿ����� refine���� �ʴ�
                 * ���̺��� �����մϴ�.
                 * CASE-26385*/
                if( sIndexCount != 0 )
                {
                    ideLog::log(
                        IDE_SERVER_0,
                        "InternalError [%s:%u]\n"
                        "Invalid table header\n",
                        (SChar *)idlVA::basename(__FILE__),
                        __LINE__ );
                    smpFixedPageList::dumpSlotHeader( sSlot );
                    smcTable::dumpTableHeader( sCurTable );

#if defined(DEBUG)
#else
                    /* Release Mode�� ��� ���⿡�� �����Ű���� �մϴ�.
                     * Debug Mode�� ���, ���� ������ ����ϰ� ����
                     * ���� ��Ŵ���μ�, ���� server start �ܰ�/Refine��������
                     * ���������� ��Ű���� �մϴ�. */
                    IDE_ASSERT( 0 );
#endif
                }
                mIndexBegin = SMN_NULL_POS;
                mIndexFence = SMN_NULL_POS;
                mIndexCursor = SMN_NULL_POS;
            }//else
        }//if sNextTablePtr
        else
        {
            mIndexBegin = SMN_NULL_POS;
            mIndexFence = SMN_NULL_POS;
            mIndexCursor = SMN_NULL_POS;

            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Do Index Rebuilding Routine
 * ----------------------------------------------*/

IDE_RC smnpIWManager::doIt(SInt aLoaderCnt)
{
    UInt                sState      = 0;
    smnpIWChildMgr    * sLoadMgr    = NULL;

    /* TC/FIT/Limit/sm/smn/smnpIWManager_doIt_malloc.sql */
    IDU_FIT_POINT_RAISE( "smnpIWManager::doIt::malloc",
                          insufficient_memory );

    /* Initialize Job Pool */
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMN,
                               ID_SIZEOF(smnpIWChildMgr),
                               (void**)&sLoadMgr) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 1;

    sLoadMgr = new (sLoadMgr) smnpIWChildMgr();

    IDE_TEST(sLoadMgr->initialize(aLoaderCnt) != IDE_SUCCESS);

    IDE_TEST(sLoadMgr->start() != IDE_SUCCESS);
    IDE_TEST(sLoadMgr->waitToStart() != IDE_SUCCESS);
    IDE_TEST_RAISE(sLoadMgr->join() != IDE_SUCCESS, thr_join_error);
    IDE_TEST(sLoadMgr->destroy() != IDE_SUCCESS);
    /* BUG-40933 thread �Ѱ��Ȳ���� FATAL���� ���� �ʵ��� ����
     * thread�� �ϳ��� �������� ���Ͽ� ABORT�� ��쿡
     * smnpIWChildMgr thread join �� �� ����� Ȯ���Ͽ�
     * ABORT������ �� �� �ֵ��� �Ѵ�. */
    IDE_TEST(sLoadMgr->getResult() == ID_FALSE);

    sState = 0;
    IDE_TEST(iduMemMgr::free(sLoadMgr)
             != IDE_SUCCESS);
    sLoadMgr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION(thr_join_error);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free(sLoadMgr) == IDE_SUCCESS );
            sLoadMgr = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}


