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
 * $Id: smlDef.h 89120 2020-11-02 06:06:28Z donlet $
 **********************************************************************/

#ifndef _O_SML_DEF_H_
#define _O_SML_DEF_H_ 1

#include <idl.h>
#include <iduList.h>
#include <smDef.h>
#include <smu.h>

#define SML_NUMLOCKTYPES      (6)
#define SML_LOCK_POOL_SIZE    (1024)
// For Table Lock Count for Prepare Log
# define SML_MAX_LOCK_INFO ((SM_PAGE_SIZE - (SMR_LOGREC_SIZE(smrXaPrepareLog)+ID_SIZEOF(smrLogTail))) / ID_SIZEOF(smlTableLockInfo))

#define SML_LOCKMODE_STRLEN  (32)

class  smlLockPool;

struct smlLockNode;
struct smlLockItem;

typedef enum 
{
    SML_NLOCK   = 0x00000000,// �ζ�
    SML_SLOCK   = 0x00000001,// ������
    SML_XLOCK   = 0x00000002,// ������
    SML_ISLOCK  = 0x00000003,// �ǵ� ������
    SML_IXLOCK  = 0x00000004,// �ǵ� ������
    SML_SIXLOCK = 0x00000005 // ���� �ǵ� ������
} smlLockMode;

struct smlLockItem
{
    iduMutex            mMutex;

    smiLockItemType     mLockItemType;  // ����� ȹ���� Item�� Ÿ��
    scSpaceID           mSpaceID;       // ����� ȹ���� ���̺����̽� ID
    ULong               mItemID;        // ���̺�Ÿ���̶�� Table OID
                                        // ��ũ ����Ÿ������ ��� File ID
    smlLockMode         mGrantLockMode;
    SInt                mGrantCnt;
    SInt                mRequestCnt;
    SInt                mFlag;         // table ��ǥ��
    SInt                mArrLockCount[SML_NUMLOCKTYPES];

    smlLockNode*        mFstLockGrant; 
    smlLockNode*        mFstLockRequest; 
    smlLockNode*        mLstLockGrant; 
    smlLockNode*        mLstLockRequest;
};

// lock node������ ���̱� ���Ͽ� ���Ե� ����.
// ���̺� Ư�� transaction�� grant�� lock��常
// �����ϰ�, �� �� transaction��  �ٸ�  lock conflict
// mode�� ��û�Ҷ� request list�� �� lock node��
// �������� �ʰ�,
// grant�� lock����� lock mode�� �����ϰ�, lockSlot
// �� �����Ѵ�.
typedef struct smlLockSlot
{
    smlLockNode     *mLockNode;
    smlLockSlot     *mPrvLockSlot;
    smlLockSlot     *mNxtLockSlot;

    /* BUG-15906: non-autocommit��忡�� Select�Ϸ��� IS_LOCK�� �����Ǹ�
     * ���ڽ��ϴ�.
     * Partial Rollback�̳� Select Stmt End�Ҷ� Lock�� ������ Ǯ���
     * ������ �����ϱ� ���� Transaction�� Lock Slot�� Sequence Number��
     * ������ �д�. */
    ULong            mLockSequence;
    
    SInt             mMask;
    smlLockMode      mOldMode;
    smlLockMode      mNewMode;
} smlLockSlot;

typedef struct smlLockNode
{
    SInt             mIndex;
    smiLockItemType  mLockItemType;
    scSpaceID        mSpaceID;      // ����� ȹ���� ���̺����̽� ID
    ULong            mItemID;       // ���̺�Ÿ���̶�� Table OID
                                    // ��ũ ����Ÿ������ ��� File ID
    smTID            mTransID;
    SInt             mSlotID;      // lock�� ��û�� transaction�� slot id
    smlLockMode      mLockMode;    // transaction�� ��û�� lock mode.
    smlLockNode     *mPrvLockNode; // grant�Ǵ� request list����������
    smlLockNode     *mNxtLockNode; // grant�Ǵ� request list����������
    smlLockNode     *mCvsLockNode;
    // mCvsLockNode-> lock conflict�߻���  �ش� table�� grant
    // �� lock�� ������ Tx�� ���Ͽ�,  ���� grant�� lock node�� pointer��
    // request list�� �� lock node�� �̸� �����Ѵ�.
    // ->���߿� �ٸ� Ʈ����ǿ���  unlock table�ÿ�
    // request list�� �ִ� lock node��
    // ���ŵ� table grant mode�� ȣȯ�ɶ�, grant list�� ������
    // insert���� �ʰ� , ���� cvs lock node�� lock mode�� ���Ž���
    // list ������ ���̷��� �ǵ����� ���ԵǾ���.
    UInt             mLockCnt;
    idBool           mBeGrant;   // grant�Ǿ������� ��Ÿ���� flag, BeGranted
    smlLockItem     *mLockItem;   
    SInt             mFlag; // lock node�� ��ǥ��.
    idBool           mIsExplicitLock; // BUG-28752 implicit/explicit lock�� �����մϴ�.
    
    //For Transaction
    smlLockNode     *mPrvTransLockNode;
    smlLockNode     *mNxtTransLockNode; 
    iduListNode      mNode4LockNodeCache; /* BUG-43408 */
    idBool           mDoRemove;  
    smlLockSlot      mArrLockSlotList[SML_NUMLOCKTYPES];
} smlLockNode;

typedef struct smlLockMatrixItem
{
    UShort           mIndex;               // ��� ����.
    UShort           mNxtWaitTransItem ;
    // �ַ� table lock�ǹ̷�
    // ���ɼ� ������,record lock������
    // �ڽ��� ����ϰ� �ִ� Ʈ������� ����Ʈ�����
    // ���ȴ�.
    UShort           mNxtWaitRecTransItem;
   // record lock�̹�, �ڽſ��� record lock�� ��ٸ��� �ִ�
    // ����Ʈ�� ���ȴ�.
} smlLockMtxItem;

typedef struct smlTableLockInfo
{
    smOID       mOidTable;
    smlLockMode mLockMode;
} smlTableLockInfo;

// sml, smx �������� �����ϱ� ���Ͽ�,
// ���Ե�.
// Ʈ������� lock�� ���� lock node list(mLockNodeHeader),
// lock slit list(mLockSlotHeader)�� ����� ������ �ִ�.
// �׸��� waiting table���� Ʈ������� ���� ó�� table lock���slot id,
// record lock ��� slot id�� �����ϰ� �ִ�. 
typedef struct smlTransLockList
{
    smlLockNode       mLockNodeHeader; // Lock Node Header
    smlLockSlot       mLockSlotHeader; // Lock Slot Header
    UShort            mFstWaitTblTransItem; // ù��° Table Lock Wait Transaction
    UShort            mFstWaitRecTransItem; // ù��° Record Lock Wait Transaction
    UShort            mLstWaitRecTransItem; // ������ Record Lock Wait Transaction
    iduList           mLockNodeCache; /* BUG-43408 */

} smlTransLockList;

/* PROJ-2734 
 * �л굥��� üũ����� �Ǵ� TX�� �����ϴ� �������� ����Ѵ�.
 * �ڼ��� ������ smlLockMgr::isCycle() �ּ� ���� */
typedef enum
{
    SML_DIST_DEADLOCK_TX_NON_DIST_INFO = 0, /* �л���������TX. �л굥��� üũ����� �ƴϳ�, �л������ִ�TX ���ö����� Ž���ؾ� �Ѵ�. */
    SML_DIST_DEADLOCK_TX_FIRST_DIST_INFO,   /* �����迡�� ù��° �ִ� �л������ִ�TX. �л굥��� üũ����̴�. */
    SML_DIST_DEADLOCK_TX_UNKNOWN            /* FIRST_DIST_INFO ���Ŀ� �ִ� TX��. �л굥��� üũ����� �ƴϴ�. */
} smlDistDeadlockTxType;

typedef struct smlDistDeadlockNode
{
    smTID                 mTransID;
    smlDistDeadlockTxType mDistTxType;
} smlDistDeadlockNode;

typedef struct smlLockMode2StrTBL
{
    smlLockMode mLockMode;
    SChar    mLockModeStr[SML_LOCKMODE_STRLEN];
} smlLockModeStr;

#endif
