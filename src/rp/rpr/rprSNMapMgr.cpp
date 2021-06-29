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
 * $Id: rprSNMapMgr.cpp $
 **********************************************************************/
#include <idu.h>
#include <rprSNMapMgr.h>

IDE_RC rprSNMapMgr::initialize( const SChar * aRepName,
                                idBool        aNeedLock )
{
    SChar          sName[IDU_MUTEX_NAME_LEN];

    mMaxReplicatedSN       = SM_SN_NULL;
    mMaxMasterCommitSN = SM_SN_NULL;

    idlOS::memset(mRepName, 0, QC_MAX_OBJECT_NAME_LEN + 1);
    idlOS::strncpy(mRepName, aRepName, QC_MAX_OBJECT_NAME_LEN);

    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_SN_MAP_MGR_MUTEX",
                    aRepName);

    IDE_TEST_RAISE(mSNMapMgrMutex.initialize(sName,
                                             IDU_MUTEX_KIND_NATIVE,
                                             IDV_WAIT_INDEX_NULL)
                   != IDE_SUCCESS, ERR_MUTEX_INIT);

    IDU_LIST_INIT(&mSNMap);

    /* pool �ʱ�ȭ
     * ������ �����߿� �޸� �Ҵ��� receiver�� �����ϸ�, ������ receiver�� �Ѵ�.
     * ������ �����ϰų�, ������ ������ executor�� �Ҵ� ������ �Ѵ�.
     * �� �޸� �Ҵ� ������ Ư�� ������ �ϳ��� �����尡 ����ϹǷ�,
     * mutex�� ����� �ʿ䰡 ����.
     */
    idlOS::snprintf(sName, IDU_MUTEX_NAME_LEN, "RP_%s_SN_MAP_POOL", aRepName);

    IDE_TEST( mEntryPool.initialize(IDU_MEM_RP_RPR,
                                    sName,
                                    1,
                                    ID_SIZEOF(rprSNMapEntry),
                                    (1024 * 4),
                                    IDU_AUTOFREE_CHUNK_LIMIT, //chunk max(default)
                                    aNeedLock,                //use mutex(no use)
                                    8,                        //align byte(default)
                                    ID_FALSE,				  //ForcePooling
                                    ID_TRUE,				  //GarbageCollection
                                    ID_TRUE,                  /* HWCacheLine */
                                    IDU_MEMPOOL_TYPE_LEGACY   /* mempool type*/) 
              != IDE_SUCCESS );			

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_MUTEX_INIT);
    {
        IDE_ERRLOG(IDE_RP_0);
        IDE_SET(ideSetErrorCode(rpERR_FATAL_ThrMutexInit));
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Mutex initialization error");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/* replicated Tx�� begin SN ������� ���ĵ�,
 * insertNewTx�� ȣ��� �� ������ master begin SN�� ������� ������
 * �ʱ� ������ master begin SN�� ���ĵ��� ����
 */
IDE_RC rprSNMapMgr::insertNewTx(smSN aMasterBeginSN, rprSNMapEntry **aNewEntry)
{
    rprSNMapEntry *sNewEntry;

    IDE_TEST(mEntryPool.alloc((void**)&sNewEntry) != IDE_SUCCESS);

    sNewEntry->mMasterBeginSN      = aMasterBeginSN;
    sNewEntry->mMasterCommitSN     = SM_SN_NULL;
    sNewEntry->mReplicatedBeginSN  = SM_SN_NULL;
    sNewEntry->mReplicatedCommitSN = SM_SN_NULL;

    IDU_LIST_INIT_OBJ(&(sNewEntry->mNode), sNewEntry);

    IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);

    IDU_LIST_ADD_LAST(&mSNMap, &(sNewEntry->mNode));

    IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

    *aNewEntry = sNewEntry;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*call by rpcExecutor::loadRecoveryInfos*/
IDE_RC rprSNMapMgr::insertEntry(rpdRecoveryInfo * aRecoveryInfo)
{
    rprSNMapEntry *sNewEntry;
    IDE_TEST( insertNewTx( aRecoveryInfo->mMasterBeginSN, &sNewEntry ) != IDE_SUCCESS );
    setSNs( sNewEntry,
            aRecoveryInfo->mMasterCommitSN,
            aRecoveryInfo->mReplicatedBeginSN,
            aRecoveryInfo->mReplicatedCommitSN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*call by rpxReceiverApply::applyTrCommit for recovery support normal receiver*/
void rprSNMapMgr::deleteTxByEntry(rprSNMapEntry* aEntry)
{
    IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);

    IDU_LIST_REMOVE(&(aEntry->mNode));

    IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

    (void)mEntryPool.memfree(aEntry);

    return;
}

/*call by rpxSender::sendTrCommit for recovery sender*/
void rprSNMapMgr::deleteTxByReplicatedCommitSN(smSN    aReplicatedCommitSN,
                                               idBool* aIsExist)
{
    iduListNode   *  sNode;
    iduListNode   *  sDummy;
    rprSNMapEntry *  sEntry;

    *aIsExist = ID_FALSE;

    IDU_LIST_ITERATE_SAFE(&mSNMap, sNode, sDummy)
    {
        sEntry = (rprSNMapEntry*)sNode->mObj;
        if(sEntry->mReplicatedCommitSN == aReplicatedCommitSN)
        {
            IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);
            IDU_LIST_REMOVE(sNode);
            IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);
            (void)mEntryPool.memfree(sEntry);
            *aIsExist = ID_TRUE;
        }
    }

    return;
}

/* Local���� ��ũ�� flush�� ������ SN�� �����Ǵ� Remote SN�� ����
 * �Լ� ������ �ϴ� �۾��� �� ������
 * 1. Local���� ��ũ�� flush�� ������ SN�� �����Ǵ� Remote SN�� ��ȯ 
 * 2. SN Map�� �ּ� SN�� ���Ͽ� ���� ��
 * 3. local�� remote���� ��� ��ũ�� flush�� ��Ʈ���� ���� ��
 *                         sn map structure
 *                  master                 replicated
 *  tx1|tx1'  begin sn | commit sn | begin sn | commit sn
 *  tx2|tx2'  begin sn | commit sn | begin sn | commit sn
 */
void rprSNMapMgr::getLocalFlushedRemoteSN(smSN  aLocalFlushSN, 
                                          smSN  aRemoteFlushSN,
                                          smSN  aRestartSN,
                                          smSN *aLocalFlushedRemoteSN)
{
    smSN             sLFRSN     = SM_SN_NULL; //local flushed remote sn
    iduListNode   *  sNode      = NULL;
    iduListNode   *  sDummy     = NULL;
    rprSNMapEntry *  sEntry     = NULL;

    if(aLocalFlushSN != SM_SN_NULL)
    {
        if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
        {
            IDU_LIST_ITERATE_SAFE(&mSNMap, sNode, sDummy)
            {
                sEntry = (rprSNMapEntry*)sNode->mObj;

                /* 1.ó�� ��Ʈ�� ���� Flush�� SN�� �������
                 *   �ƹ��͵� Flush���� �ʾ����Ƿ� sLFRSN�� SM_SN_NULL�� ��ȯ
                 * 2.ó�� ��Ʈ���� �ƴ� ���
                 *   ���� commit sn�� ���� begin sn���̿� sLFRSN�� �����Ƿ�
                 *   ���� commit sn�� sLFRSN���� ��ȯ
                 */
                if(aLocalFlushSN < sEntry->mReplicatedBeginSN)
                {
                    break;
                }

                //replicated begin���ٴ� ũ�ų� ����, commit���ٴ� ����. master�� begin sn���� ����
                if(aLocalFlushSN < sEntry->mReplicatedCommitSN)
                {
                    sLFRSN = sEntry->mMasterBeginSN;
                    break;
                }

                //replicated commit���� ũ�ų� ����. master�� commit sn���� ���� ��, ���� ��Ʈ�� Ȯ��
                //local������ flush�Ǿ���
                sLFRSN = sEntry->mMasterCommitSN;
                //���� aLocalFlushSN���� ū ���� ������ �ʾ����Ƿ�, ���� ��带 Ȯ���ؾ� ��
                //remote���� flush�Ǿ����Ƿ� ���� (��� flush�Ǿ���)
                if(aRemoteFlushSN != SM_SN_NULL)
                {
                    if ( ( aRemoteFlushSN >= sEntry->mMasterCommitSN ) &&
                         ( aLocalFlushSN >= sEntry->mReplicatedCommitSN ) )
                    {
                        IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);
                        IDU_LIST_REMOVE(sNode);
                        IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

                        (void)mEntryPool.memfree(sEntry);
                    }
                    else
                    {
                        /* do nothing */
                    }
                }
            }
        }
        else
        {
            /*
             * BUG-41331
             * SN Mapping table�� ����ִٴ� ����
             * 1. Replication Log�� �� ó���Ǿ��ٴ� �Ͱ�
             * 2. Active�� Standby�� Replication �����Ͱ� ���ٴ�
             * �� ���ϹǷ� Aciver server�� restartSN�� LocalFlushRemoteSN�� �����Ѵ�.
             */
            sLFRSN = aRestartSN;
        }
    }

    *aLocalFlushedRemoteSN = sLFRSN;

    return;
}

/* Recoery Sender�� �α׸� ���� �� �ش� Ʈ������� Recovery�ؾ��ϴ��� Ȯ�� ��
 * SN Map�� replicated tx�� begin sn���� ���ĵǾ������Ƿ�, aBeginLogSN
 * ���� ū ���� ������ scan�� �ߴ� �ϰ� recovery�� ���ʿ��� Ʈ��������� �Ǵ� ��
 */
idBool rprSNMapMgr::needRecoveryTx(smSN aBeginLogSN)
{
    iduListNode   *  sNode      = NULL;
    rprSNMapEntry *  sEntry     = NULL;

    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        IDU_LIST_ITERATE(&mSNMap, sNode)
        {
            sEntry = (rprSNMapEntry*)sNode->mObj;
            if(sEntry->mReplicatedBeginSN == aBeginLogSN)
            { //Recovery �ؾ� ��
                return ID_TRUE;
            }
            if(sEntry->mReplicatedBeginSN > aBeginLogSN)
            {
                break;
            }
        }
    }
    //�˻� ����
    return ID_FALSE;
}

/*call by rpcExecutor::checkAndGiveupReplication for checkpoint thread*/
smSN rprSNMapMgr::getMinReplicatedSN()
{
    smSN sMinSN = SM_SN_NULL;
    iduListNode   *  sNode      = NULL;
    rprSNMapEntry *  sEntry     = NULL;

    IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);

    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        IDU_LIST_ITERATE(&mSNMap, sNode)
        {
            sEntry = (rprSNMapEntry*)sNode->mObj;
            sMinSN = sEntry->mReplicatedBeginSN;
            break;
        }
    }

    IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

    return sMinSN;
}

/*call by rpcExecutor::saveAllRecoveryInfos*/
void rprSNMapMgr::getFirstEntrySNsNDelete(rpdRecoveryInfo * aRecoveryInfos)
{
    iduListNode   * sNode;
    rprSNMapEntry * sEntry;

    aRecoveryInfos->mMasterBeginSN      = SM_SN_NULL;
    aRecoveryInfos->mMasterCommitSN     = SM_SN_NULL;
    aRecoveryInfos->mReplicatedBeginSN  = SM_SN_NULL;
    aRecoveryInfos->mReplicatedCommitSN = SM_SN_NULL;

    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        sNode = IDU_LIST_GET_FIRST(&mSNMap);
        sEntry = (rprSNMapEntry*)sNode->mObj;

        IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);
        IDU_LIST_REMOVE(sNode);
        IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

        aRecoveryInfos->mMasterBeginSN      = sEntry->mMasterBeginSN;
        aRecoveryInfos->mMasterCommitSN     = sEntry->mMasterCommitSN;
        aRecoveryInfos->mReplicatedBeginSN  = sEntry->mReplicatedBeginSN;
        aRecoveryInfos->mReplicatedCommitSN = sEntry->mReplicatedCommitSN;

        (void)mEntryPool.memfree(sEntry);
    }

    return;

}

void rprSNMapMgr::getFirstEntrySN(rpdRecoveryInfo * aRecoveryInfos)
{
    iduListNode   * sNode;
    rprSNMapEntry * sEntry;

    aRecoveryInfos->mMasterBeginSN      = SM_SN_NULL;
    aRecoveryInfos->mMasterCommitSN     = SM_SN_NULL;
    aRecoveryInfos->mReplicatedBeginSN  = SM_SN_NULL;
    aRecoveryInfos->mReplicatedCommitSN = SM_SN_NULL;

    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        sNode = IDU_LIST_GET_FIRST(&mSNMap);

        sEntry = (rprSNMapEntry*)sNode->mObj;
        
        aRecoveryInfos->mMasterBeginSN      = sEntry->mMasterBeginSN;
        aRecoveryInfos->mMasterCommitSN     = sEntry->mMasterCommitSN;
        aRecoveryInfos->mReplicatedBeginSN  = sEntry->mReplicatedBeginSN;
        aRecoveryInfos->mReplicatedCommitSN = sEntry->mReplicatedCommitSN;
    }
    return;
}


void rprSNMapMgr::destroy()
{
    mMaxReplicatedSN   = SM_SN_NULL;
    mMaxMasterCommitSN = SM_SN_NULL;

    IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);
    IDU_LIST_INIT(&mSNMap);
    IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);

    if(mEntryPool.destroy(ID_FALSE) != IDE_SUCCESS)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if(mSNMapMgrMutex.destroy() != 0)
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    return;
}

void rprSNMapMgr::setSNs(rprSNMapEntry* aEntry,
                         smSN           aMasterCommitSN,
                         smSN           aReplicatedBeginSN,
                         smSN           aReplicatedCommitSN)
{
    aEntry->mMasterCommitSN     = aMasterCommitSN;
    aEntry->mReplicatedBeginSN  = aReplicatedBeginSN;
    aEntry->mReplicatedCommitSN = aReplicatedCommitSN;
}

/*delete Master Commit SN <= Active's RP Recovery SN Entry for recovery sender*/
UInt rprSNMapMgr::refineSNMap(smSN aActiveRPRecoverySN)
{
    iduListNode   *  sNode;
    iduListNode   *  sDummy;
    rprSNMapEntry *  sEntry;
    UInt             sCount = 0;

    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        IDU_LIST_ITERATE_SAFE(&mSNMap, sNode, sDummy)
        {
            sEntry = (rprSNMapEntry*)sNode->mObj;
            sCount ++;

            if((sEntry->mMasterCommitSN == SM_SN_NULL) ||
               (sEntry->mMasterCommitSN <= aActiveRPRecoverySN))
            {
                IDE_ASSERT(mSNMapMgrMutex.lock(NULL) == IDE_SUCCESS);
                IDU_LIST_REMOVE(sNode);
                IDE_ASSERT(mSNMapMgrMutex.unlock() == IDE_SUCCESS);
                (void)mEntryPool.memfree(sEntry);
                sCount --;
            }
        }
    }

    return sCount;
}


/*call by Receiver finalize*/
void rprSNMapMgr::setMaxSNs()
{
    iduListNode   *  sNode      = NULL;
    rprSNMapEntry *  sEntry     = NULL;
    smSN     sMaxMasterCommitSN = 0;
    smSN     sMaxReplicatedSN   = 0;
    
    if(IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE)
    {
        IDU_LIST_ITERATE(&mSNMap, sNode)
        {
            sEntry = (rprSNMapEntry*)sNode->mObj;
            
            //max master commit sn
            if(sEntry->mMasterCommitSN != SM_SN_NULL)
            {
                if(sMaxMasterCommitSN < sEntry->mMasterCommitSN)
                {
                    sMaxMasterCommitSN = sEntry->mMasterCommitSN;
                }
            } //max master commit sn

            //max replicated sn
            if((sEntry->mReplicatedBeginSN != SM_SN_NULL) && 
               (sEntry->mReplicatedCommitSN != SM_SN_NULL))
            {
                if(sMaxReplicatedSN < sEntry->mReplicatedCommitSN)
                {
                    sMaxReplicatedSN = sEntry->mReplicatedCommitSN;
                }
            } //max replicated sn
        } // list iterate
    } //is empty
    
    if(sMaxMasterCommitSN != 0)
    {
        mMaxMasterCommitSN = sMaxMasterCommitSN;
    }
    else
    {
        mMaxMasterCommitSN = SM_SN_NULL;
    }
    
    if(sMaxReplicatedSN != 0)
    {
        mMaxReplicatedSN = sMaxReplicatedSN;
    }
    else
    {
        mMaxReplicatedSN = SM_SN_NULL;
    }
}

smSN rprSNMapMgr::getMaxMasterCommitSN()
{
    if((mMaxMasterCommitSN == SM_SN_NULL) && 
       (IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE))
    {
        setMaxSNs();
    }
    return mMaxMasterCommitSN;
}

smSN rprSNMapMgr::getMaxReplicatedSN()
{ 
    if((mMaxReplicatedSN == SM_SN_NULL) && 
       (IDU_LIST_IS_EMPTY(&mSNMap) != ID_TRUE))
    {
        setMaxSNs();
    }
    return mMaxMasterCommitSN;
}
