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
 * $Id: smaDef.h 88423 2020-08-26 04:06:55Z emlee $
 **********************************************************************/

#ifndef _O_SMA_DEF_H_
# define _O_SMA_DEF_H_ 1


# include <smDef.h>
# include <smx.h>
# include <smnDef.h>

# define SMA_NODE_POOL_LIST_COUNT  (4)
# define SMA_NODE_POOL_SIZE        (1024)

# define SMA_TRANSINFO_DEF_COUNT (1024)

# define SMA_NODE_POOL_MAXIMUM (100)
# define SMA_NODE_POOL_CACHE   (25)

# define SMA_GC_NAME_LEN              (128)

/* BUG-35179 Add property for parallel logical ager */
# define SMA_PARALLEL_LOGICAL_AGER_OFF (0)
# define SMA_PARALLEL_LOGICAL_AGER_ON (1)

/* BUG-47367 */
# define SMA_PARALLEL_DELETE_THREAD_OFF (0)
# define SMA_PARALLEL_DELETE_THREAD_ON  (1)


typedef struct smaOidList
{
    smSCN       mSCN;
    // row�� free��  index node free����
    // sync�� ���߱����Ͽ� ���.
    // oid�� �����ϴ�  �ε��� key slot����
    // free �Ϸ��� ������ system view scn. 
    smSCN       mKeyFreeSCN;  
    idBool      mErasable;
    idBool      mFinished;
    UInt        mCondition;
    smTID       mTransID;
    SInt        mListN;
    smaOidList* mNext;
    smxOIDNode* mHead;
    smxOIDNode* mTail;
}smaOidList;

// for fixed table .
typedef struct smaGCStatus
{
    //GC name :smaLogicalAger, smaDeleteThread.
    SChar  *mGCName;
    // ���� systemViewSCN.
    smSCN  mSystemViewSCN;
    // Active Transaction �� �ּ� memory view SCN
    smSCN  mMiMemSCNInTxs;
    // BUG-24821 V$TRANSACTION�� LOB���� MinSCN�� ��µǾ�� �մϴ�. 
    // mMiMemSCNInTxs�� ������ Ʈ����� ���̵�(���� ������ Ʈ�����)
    smTID  mOldestTx;
    // LogicalAger, DeleteThred�� Tail�� commitSCN
    smSCN  mSCNOfTail;
    // OIDList�� ��� �ִ��� boolean
    idBool mIsEmpty;
    // add�� OID ����.
    ULong  mAddOIDCnt;
    // GCó������ OID����.
    ULong  mGcOIDCnt;

    /* Transaction�� Commmit/Abort�� OID List�� Aging�� �����ؾ���
     * OID������ ��������.*/
    ULong mAgingRequestOIDCnt;

    /* Ager�� OID List�� OID�� �ϳ��� ó���ϴµ� �̶� 1������ */
    ULong mAgingProcessedOIDCnt;

    //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ �� Aging��
    //                 �и��� ������ �� ��ȭ��.
    // getMinSCN������, MinSCN������ �۾����� ���� Ƚ��
    ULong mSleepCountOnAgingCondition;

    /* ���� �������� Logical Ager Thread, Ȥ�� Delete Thread�� ���� */
    UInt  mThreadCount;
} smaGCStatus;

/*
    Instant Aging�� ���� Filter

    Instant Aging�� ������ ������ �����Ѵ�.
    
    [ ������ Filter�� ]
      1. mTBSID�� SC_NULL_SPACEID�� �ƴ� ���
         => Ư�� Tablespace�� ���� OID�鸸 Instant Aging�ǽ�
      2. mTableOID�� SM_NULL_OID�� �ƴ� ���
         => Ư�� Table�� ���� OID�鸸 Instant Aging �ǽ�
 */
typedef struct smaInstantAgingFilter
{
    scSpaceID mTBSID;    
    smOID     mTableOID;
} smaInstantAgingFilter ;


#endif /* _O_SMA_DEF_H_ */
