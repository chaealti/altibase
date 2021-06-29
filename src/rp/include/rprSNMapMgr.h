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
 

#ifndef _O_RPR_SNMAPMGR_H_
#define _O_RPR_SNMAPMGR_H_

#include <smDef.h>
#include <rp.h>
#include <qci.h>
#include <rpdMeta.h>

typedef struct rprSNMapEntry
{
    smSN        mMasterBeginSN;
    smSN        mMasterCommitSN;
    smSN        mReplicatedBeginSN;
    smSN        mReplicatedCommitSN;
    iduListNode mNode;
} rprSNMapEntry;

class rprSNMapMgr;
class rpxSender;

typedef struct rprRecoveryItem
{
    rpRecoveryStatus   mStatus;
    rprSNMapMgr      * mSNMapMgr;
    rpxSender        * mRecoverySender;
} rprRecoveryItem;

class rprSNMapMgr
{
public:

    rprSNMapMgr(){};
    ~rprSNMapMgr() {};

    IDE_RC initialize( const SChar * aRepName,
                       idBool        aNeedLock );

    void   destroy();

    /* 1.Local���� ��ũ�� flush�� ������ SN�� �����Ǵ� Remote�� SN�� ���ؾ� �ϸ�,
     *   �̶�, sn map�� ��ĵ�ؾ��Ѵ�.
     * 2.���� ��ο��� ��ũ�� flush�� ��Ʈ���� �ֱ������� �����ؾ��ϸ�, �� ������ ��ĵ�� �ʿ��ϴ�.
     * ��ĵ Ƚ���� ���̱� ���� �� �� ���� �۾��� �ѹ� ��ĵ�� �Բ� �ϵ��� �Ѵ�.
     * 1�� �۾��� ����, local flush SN�� �ʿ��ϸ�, 2�� �۾��� ���� local/remote flush sn�� �ʿ��ϴ�.
     * �׷��� ���� ��ο��� ��ũ�� flush�� SN�� ���ڷ� �޴´�.
     */
    void   getLocalFlushedRemoteSN(smSN   aLocalFlushSN, 
                                   smSN   aRemoteFlushSN,
                                   smSN   aRestartSN,
                                   smSN * aLocalFlushedRemoteSN);
    /*Recoery Sender�� �α׸� ���� �� �ش� Ʈ������� Recovery�ؾ��ϴ��� Ȯ�� ��*/
    idBool needRecoveryTx(smSN aBeginLogSN);

    void   setMaxSNs();
    smSN   getMaxMasterCommitSN();
    smSN   getMinReplicatedSN();
    smSN   getMaxReplicatedSN();

    void   getFirstEntrySNsNDelete(rpdRecoveryInfo * aRecoveryInfos);
    void   getFirstEntrySN(rpdRecoveryInfo * aRecoveryInfos);
    IDE_RC insertNewTx(smSN aMasterBeginSN, rprSNMapEntry **aNewEntry);
    IDE_RC insertEntry(rpdRecoveryInfo * aRecoveryInfo);
    void   deleteTxByEntry(rprSNMapEntry * aEntry);
    void   deleteTxByReplicatedCommitSN(smSN     aReplicatedCommitSN,
                                        idBool * aIsExist);

    void   setSNs(rprSNMapEntry * aEntry,
                  smSN            aMasterCommitSN,
                  smSN            aReplicatedBeginSN,
                  smSN            aReplicatedCommitSN);

    UInt   refineSNMap(smSN aActiveRPRecoverySN);

    inline idBool isYou(const SChar * aRepName )
    {
        return ( idlOS::strcmp( mRepName, aRepName ) == 0 )
            ? ID_TRUE : ID_FALSE;
    };

    inline idBool isEmpty(){ return IDU_LIST_IS_EMPTY(&mSNMap); };

    SChar           mRepName[QC_MAX_OBJECT_NAME_LEN + 1];

    smSN            mMaxReplicatedSN;
private:

    iduMemPool      mEntryPool;
    iduList         mSNMap;
    /* SN Map Mgr Mutex�� check point thread�� receiver�� ���ÿ� ����Ʈ�� �����ϴ� ���� ���� �ϱ� ���� �����Ѵ�.
     * checkpoint thread�� checkpoint�� ����Ʈ�� ó�� ��Ʈ���� �˻��ϸ�, receiver�� ����Ʈ�� ���� �� �˻��Ѵ�. 
     * ����Ʈ�� �����ϴ� ���� receiver�� executor�� ��Ÿ������ �����ϹǷ�, 
     * receiver/executor�� ����Ʈ�� ���� �� ������ lock�� �⵵�� �Ͽ� ���ʿ��� lock�� ���� �ʵ��� �Ѵ�.
     * checkpoint thread�� �˻� �� lock�� ��Ƽ�, checkpoint thread�� �߸��� ���� ���� �ʵ��� �Ѵ�.
     */
    iduMutex        mSNMapMgrMutex;
    smSN            mMaxMasterCommitSN; //recovery�ؾ� �ϴ� ���� ���� ���θ� ������ �����ϱ� ���� ������
};

#endif //_O_RPR_SNMAPMGR_H_
