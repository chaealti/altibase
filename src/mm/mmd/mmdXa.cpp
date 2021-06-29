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

#include <mmErrorCode.h>
#include <mmcSession.h>
#include <mmdDef.h>
#include <mmdManager.h>
#include <mmdXa.h>
#include <mmdXid.h>
#include <mmuProperty.h>
#include <mmtSessionManager.h>
#include <dki.h>

#define FLAG_IS_SET(F, S)              (((F) & (S)) == (S))

const SChar *MMDXASTATE[] =
{
    "XA_STATE_IDLE",
    "XA_STATE_ACTIVE",
    "XA_STATE_PREPARED",
    "XA_STATE_HEURISTICALLY_COMMITTED",
    "XA_STATE_HEURISTICALLY_ROLLBACKED",
    "XA_STATE_NON_EXIST_TRANSACTION",
    "XA_STATE_ROLLBACK_ONLY",
    "NONE"
};

/**
 * �ӽ÷� ������ �� ���,
 * ���Ǹ� ���ؼ� mmdXa::fix ��� mmdXaSafeFind�� �̿��Ѵ�.
 *
 * mmdXaSafeFind()�� fix count�� ������Ű�� �����Ƿ�
 * ���߿� ���ܰ� �߻��ϴ��� fix count�� ���õ� ������ ó���� �� �ʿ䰡 ����.
 *
 * mmdXaSafeFind()�� ���� mmdXid�� �����͸� �̿��ؼ�
 * ������ ���� ��, Ȯ���ϴ� �� �ܿ� �ٸ� ������ �ؼ��� �ȵǸ�,
 * ������ mmdXa::fix()�� mmdXa::unFix() ���̿��� ȣ���� ���� ���Ѵ�.
 *
 * @param aXidObjPtr
 *        xid ��ü�� �����͸� ���� ������ ������
 * @param aXIDPtr
 *        mmdXidManager���� �˻��� xid ��
 * @param aXaLogFlag
 *        xid ��ü�� ã�Ҵٴ� ���� �α׿� ������ ����.
 *        mmdXidManager::find() �� ��, ����° ���ڷ� �ѱ� ��.
 *
 * @return mmdXidManager::find()�� ������ ���, IDE_FAILURE.
 *         mmdXidManager::find() ���д� smuHash::findNodeLatch()�� ������� ��
 *         lock ȹ��, ������ �����ϸ� �߻��Ѵ�.
 *         �� ���� ��쿡�� IDE_ASSERT()�� �˻��ϹǷ� �����ϸ� ������ �ٿ� �� ��.
 */
IDE_RC mmdXaSafeFind(mmdXid **aXidObjPtr, ID_XID *aXIDPtr, mmdXaLogFlag aXaLogFlag)
{
    IDE_DASSERT((aXidObjPtr != NULL) && (aXIDPtr != NULL));

    UInt sStage = 0;
    UInt sBucket = mmdXidManager::getBucketPos(aXIDPtr);
/* fix BUG-35374 To improve scalability about XA, latch granularity of XID hash should be more better than now
   that is to say , chanage the granularity from global to bucket level.
*/               
    IDE_ASSERT(mmdXidManager::lockWrite(sBucket) == IDE_SUCCESS);
    sStage = 1;
    IDE_TEST(mmdXidManager::find(aXidObjPtr, aXIDPtr,sBucket,aXaLogFlag) != IDE_SUCCESS);
    sStage = 0;
    IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        if (sStage == 1)
        {
            IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);
        }
    }

    return IDE_FAILURE;
}

/**
 * ID_XID�� �ش��ϴ� mmdXid�� ã��
 * fix count�� 1 ������Ų��.
 *
 * @param aXidObjPtr
 *        xid ��ü�� �����͸� ���� ������ ������
 * @param aXIDPtr
 *        mmdXidManager���� �˻��� xid ��
 * @param aXaLogFlag
 *        xid ��ü�� ã�Ҵٴ� ���� �α׿� ������ ����.
 *        mmdXidManager::find() �� ��, ����° ���ڷ� �ѱ� ��.
 *
 * @return mmdXidManager::find()�� ������ ���, IDE_FAILURE.
 */
IDE_RC mmdXa::fix(mmdXid **aXidObjPtr, ID_XID *aXIDPtr, mmdXaLogFlag aXaLogFlag)
{
    IDE_DASSERT((aXidObjPtr != NULL) && (aXIDPtr != NULL));

    mmdXid     *sXidObj  = NULL;
    UInt        sStage    = 0;
    UInt        sBucketPos = mmdXidManager::getBucketPos(aXIDPtr);
/* fix BUG-35374 To improve scalability about XA, latch granularity of XID hash should be more better than now
  that is to say , chanage the granularity from global to bucket level.
 */               
    
    /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
     fix(list-s-latch) , fixWithAdd(list-x-latch)�� �и����� fix ������ �л��ŵ�ϴ�. */
    IDE_ASSERT(mmdXidManager::lockRead(sBucketPos) == IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(mmdXidManager::find(&sXidObj, aXIDPtr,sBucketPos, aXaLogFlag) != IDE_SUCCESS);

    if (sXidObj != NULL)
    {
        /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
           list-s-latch�� ��ұ⶧����  xid fix-count���ռ��� ���Ͽ�
          xid latch�� ��� fix count�� �����ϴ� fixWithLatch�Լ��� ����Ѵ�.*/
        sXidObj->fixWithLatch();
    }
    sStage = 0;
    IDE_ASSERT(mmdXidManager::unlock(sBucketPos) == IDE_SUCCESS);
    
    *aXidObjPtr = sXidObj;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch (sStage)
        {
            case 1:
                IDE_ASSERT(mmdXidManager::unlock(sBucketPos) == IDE_SUCCESS);
            default:
                break;
        }
    }

    return IDE_FAILURE;
}


/**
 * Caller�� �̸� �����ص� xid object��  xid list�� �߰��õ��� �Ѵ�.
 * �̹� list�� �ִ� ��쿡�� ���ϵ� xid object�� Ȱ���ϵ��� �Ѵ�.
 *
 * @param aXidObj2Add
 *         Caller�� latch duration�� ���̱� ���Ͽ� �̸� �����ص� xid object
 * @param aFixedXidObjPtr
 *        xid list���� aXidPtr value�� �˻��Ͽ� ������ aXidObj2Add pointer�� �ǰ�,
 *        �˻��Ͽ� ������  �˻��� xid object pointer�� �ȴ�.                               
 * @param aXaLogFlag
 *        xid ��ü�� ã�Ҵٴ� ���� �α׿� ������ ����.
 *        mmdXidManager::find() �� ��, ����° ���ڷ� �ѱ� ��.
 *
 * @return mmdXidManager::find()�� ������ ���, IDE_FAILURE.
 */

IDE_RC mmdXa::fixWithAdd(mmdXaContext */*aXaContext*/,
                         mmdXid       *aXidObj2Add,
                         mmdXid      **aFixedXidObjPtr,
                         ID_XID       *aXIDPtr,
                         mmdXaLogFlag  aXaLogFlag)
{
    IDE_DASSERT(aXidObj2Add != NULL);
    IDE_DASSERT(aXIDPtr != NULL);
    IDE_DASSERT(aFixedXidObjPtr != NULL);

    mmdXid     *sXidObj  = NULL;
    UInt        sStage    = 0;
    /* fix BUG-35374 To improve scalability about XA, latch granularity of XID hash should be more better than now
       that is to say , chanage the granularity from global to bucket level. */           
    UInt sBucket = mmdXidManager::getBucketPos(aXIDPtr);

    /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
     fix(list-s-latch) , fixWithAdd(list-x-latch)�� �и����� fix ������ �л��ŵ�ϴ�. */
    IDE_ASSERT(mmdXidManager::lockWrite(sBucket) == IDE_SUCCESS);
    sStage = 1;

    IDE_TEST(mmdXidManager::find(&sXidObj, aXIDPtr,sBucket,aXaLogFlag) != IDE_SUCCESS);
    if (sXidObj == NULL)
    {
        IDE_TEST(mmdXidManager::add(aXidObj2Add,sBucket) != IDE_SUCCESS);
        sXidObj = aXidObj2Add;
    }
    else
    {
        //nothing to do
    }

    sXidObj->fix();
    sStage = 0;
    IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);

    *aFixedXidObjPtr = sXidObj;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch (sStage)
        {
            case 1:
                IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);
            default:
                break;
        }
    }

    return IDE_FAILURE;
}

/**
 * ID_XID�� �ش��ϴ� mmdXid�� ã��
 * fix count�� 1 ���ҽ�Ų��.
 *
 * ����, fix count�� 0�� ������
 * �޸𸮸� ��ȯ�Ѵ�.
 *
 * aXidObj�� NULL�̸� �ƹ��͵� ���Ѵ�.
 *
 * @param aXidObj
 *        xid ��ü�� ������
 * @param aXIDPtr
 *        aXidObj�� �ѱ�� ��ü�� ID_XID ��
 * @param aOptFlag
 *        unfix �� ��, �߰��� ������ ���� ����.
 *        MMD_XA_REMOVE_FROM_XIDLIST�� ���Ǹ� �� ���� �ɼ��� ���õȴ�.
 *        �߰��� ������ ������ ���� ��쿡�� ��������� MMD_XA_NONE�� �ѱ� ���� ����.
 *
 * @return mmdXidManager::find()�� ������ ���, IDE_FAILURE.
 *         mmdXidManager::find() ���д� smuHash::findNodeLatch()�� ������� ��
 *         lock ȹ��, ������ �����ϸ� �߻��ϸ�,
 *         aXidObj�� NULL�� �ƴ� ��쿡�� �����Ѵ�.
 *         MMD_XA_REMOVE_FROM_XIDLIST �ɼ��� ����� ��쿡�� �ش��ϸ�,
 *         �� ���� ��쿡�� IDE_ASSERT()�� �˻��ϹǷ� �����ϸ� ������ �ٿ� �� ��.
 */
IDE_RC mmdXa::unFix(mmdXid *aXidObj, ID_XID * /*aXIDPtr*/, UInt aOptFlag)
{
    mmdXid     *sXidObj  = NULL;
    UInt        sFixCount;
    UInt        sStage   = 0;

    ID_XID     *sXIDPtr  = NULL;
    UInt        sBucket;
    
    if (aXidObj != NULL)
    {
        /* bug-35619: valgrind warning: invalid read of size 1
           aXIDPtr ���ڸ� �� �� �Ѱ� ����ϴ� ���� ���� ����
           mmdXid ������ XID�� �����.
           �Լ�ȣ���ϴ� ���� �����Ƿ� ���ڴ� �״�� ���� */
        sXIDPtr = aXidObj->getXid();

        /* fix BUG-35374 To improve scalability about XA, latch
           granularity of XID hash should be more better than now.
           that is to say , chanage the granularity
           from global to bucket level. */
        sBucket = mmdXidManager::getBucketPos(sXIDPtr);

        IDE_ASSERT(mmdXidManager::lockWrite(sBucket) == IDE_SUCCESS);
        sStage = 1;

        aXidObj->unfix(&sFixCount);


        if (FLAG_IS_SET(aOptFlag, MMD_XA_REMOVE_FROM_XIDLIST))
        {
            IDE_TEST(mmdXidManager::find(&sXidObj, sXIDPtr,sBucket) != IDE_SUCCESS);

            if (sXidObj != NULL)
            {
                IDE_ASSERT(mmdXidManager::remove(sXidObj,&sFixCount) == IDE_SUCCESS);
            }
        }
        sStage = 0;
        IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);
       /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
        XA Unfix �ÿ� latch duaration�� ���̱����Ͽ� xid fix-Count�� xid list latch release�ϰ� ���� 
        aXid Object�� ���� �Ѵ�.*/
        if(sFixCount == 0)
        {    
            IDE_ASSERT(mmdXidManager::free(aXidObj, ID_TRUE)
                    == IDE_SUCCESS);
            aXidObj = NULL;
        }
    }
    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        if (sStage == 1)
        {
            IDE_ASSERT(mmdXidManager::unlock(sBucket) == IDE_SUCCESS);
        }
    }

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginOpen(mmdXaContext *aXaContext)
{
    aXaContext->mReturnValue = XA_OK;

    return IDE_SUCCESS;
}

IDE_RC mmdXa::beginClose(mmdXaContext *aXaContext)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);

    aXaContext->mReturnValue = XA_OK;

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginStart(mmdXaContext     *aXaContext,
                         mmdXid           *aXid,
                         mmdXaAssocState  aXaAssocState,
                         UInt             *aState)
{
    PDL_Time_Value sSleepTime;
    PDL_Time_Value sMaxSleepTime;
    PDL_Time_Value sCurSleepTime;
    /* BUG-22935  ����  XID�� ���Ͽ� ,  xa_start with TMJOIN�� xa_commit,xa_rollback�� ���ÿ�
       ����ɶ� ������ ����*/
    mmdXid        *sXid = NULL;
    ID_XID         sXidValue;

    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    IDE_TEST_RAISE(aXaAssocState   == MMD_XA_ASSOC_STATE_ASSOCIATED,
                   NotAssociated);
    IDE_TEST_RAISE( (aXaContext->mFlag & TMRESUME) &&
                    (aXaAssocState  !=  MMD_XA_ASSOC_STATE_SUSPENDED),
                    InvalidXaState);

    if (aXid == NULL)
    {
        IDE_TEST_RAISE(MMD_XA_XID_RESTART_FLAG(aXaContext->mFlag), XidNotExist);
    }
    else
    {
        if (aXaContext->mFlag & TMNOWAIT)
        {
            IDE_TEST_RAISE(aXid->getState() == MMD_XA_STATE_ACTIVE, XidInUse);
        }
        else
        {
            if(aXid->getState() == MMD_XA_STATE_ACTIVE)
            {
                sSleepTime.set(0, MMD_XA_RETRY_SPIN_TIMEOUT);
                sCurSleepTime.set(0,0);
                sMaxSleepTime.set(mmuProperty::getQueryTimeout(),0);
                /* BUG-22935  ����  XID�� ���Ͽ� ,  xa_start with TMJOIN�� xa_commit,xa_rollback�� ���ÿ�
                   ����ɶ� ������ ����*/
                idlOS::memcpy(&sXidValue,aXid->getXid(),ID_SIZEOF(sXidValue));
                *aState =1;
                aXid->unlock();
              retry:
                IDE_TEST(mmdXaSafeFind(&sXid,&sXidValue,MMD_XA_DO_NOT_LOG) != IDE_SUCCESS);
                IDE_TEST_RAISE(((sXid == NULL) ||( sXid != aXid)),XidNotExist);
                //fix BUG-27101, Xa BeginStart�ÿ� lock���������� state�� �����ϰ� �ֽ��ϴ�.
                //�׸��� Code-Sonar Null reference waring�� �����ϱ� ���Ͽ�
                // sXid��� aXid�� ����մϴ�( sXid �� aXid�� ���⶧���Դϴ�).
                aXid->lock();
                *aState =2;
                if ( (aXid->getState() == MMD_XA_STATE_ACTIVE ) &&  (sCurSleepTime < sMaxSleepTime  ) )
                {
                    *aState = 1;
                    //Code-Sonar null reference waring�� �����ϱ� ���Ͽ�
                    //sXid��� aXid�� ����մϴ�( sXid �� aXid�� ���⶧���Դϴ�).
                    aXid->unlock();
                    idlOS::sleep(sSleepTime);

                    sCurSleepTime += sSleepTime;
                    goto retry;
                }//if
            }//else
            else
            {
                //nothig to do
            }

        }//else

        /* BUG-22935  ����  XID�� ���Ͽ� ,  xa_start with TMJOIN�� xa_commit,xa_rollback�� ���ÿ�
         ����ɶ� ������ ����*/
        IDE_TEST_RAISE((aXid->getState() != MMD_XA_STATE_IDLE), InvalidXaState);

        IDE_TEST_RAISE(!MMD_XA_XID_RESTART_FLAG(aXaContext->mFlag), XidAlreadyExist);
    }

    aXaContext->mReturnValue = XA_OK;

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(NotAssociated);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_ASSOCIATED_BRANCH ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        // bug-27071: xa: beginStart xid null ref
        if (aXid == NULL)
        {
            IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE,
                                     "UNKNOWN XA_STATE: XID NULL"));
        }
        else
        {
            IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE,
                                     MMDXASTATE[aXid->getState()] ));
        }
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidInUse);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_XID_INUSE ));
        aXaContext->mReturnValue = XA_RETRY;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(XidAlreadyExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_ALREADY_EXIST_XID ));
        aXaContext->mReturnValue = XAER_DUPID;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mmdXa::beginEnd(mmdXaContext *aXaContext, mmdXid *aXid, mmdXid *aCurrentXid,
                       mmdXaAssocState  aXaAssocState)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    // bug-26973: codesonar: xid null ref
    // xid null �˻��ϴ� �κ��� switch ���Ŀ��� �������� �ű�.
    IDE_TEST_RAISE(aXid == NULL, XidNotExist);

    switch (aXaContext->mSession->getXaAssocState())
    {
        case MMD_XA_ASSOC_STATE_NOTASSOCIATED:
            IDE_RAISE(NotAssociated);
            break;

        case MMD_XA_ASSOC_STATE_ASSOCIATED:
            break;

        case MMD_XA_ASSOC_STATE_SUSPENDED:
            IDE_TEST_RAISE(aXaContext->mFlag & TMSUSPEND, InvalidXaState);
            break;

        default:
            IDE_RAISE(InvalidXaState);
            break;
    }

    IDE_TEST_RAISE(aXid != aCurrentXid, InvalidXid);
    if(aXid->getState() != MMD_XA_STATE_ACTIVE)
    {

        //fix BUG-22561
        IDE_TEST_RAISE(aXaAssocState != MMD_XA_ASSOC_STATE_SUSPENDED,
                       NotAssociated);
    }
    else
    {
        //nothing to do
    }

    aXaContext->mReturnValue = XA_OK;

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(NotAssociated);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_ASSOCIATED_BRANCH ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[aXid->getState()] ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(InvalidXid);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XID ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginPrepare(mmdXaContext *aXaContext, mmdXid *aXid)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    IDE_TEST_RAISE(aXaContext->mSession->getXaAssocState()
                   != MMD_XA_ASSOC_STATE_NOTASSOCIATED, InvalidXaSession);

    IDE_TEST_RAISE(aXid == NULL, XidNotExist);

    switch (aXid->getState())
    {
        case MMD_XA_STATE_IDLE:
            aXaContext->mReturnValue = XA_OK;
            break;

        case MMD_XA_STATE_ACTIVE:
        case MMD_XA_STATE_PREPARED:
        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
        case MMD_XA_STATE_NO_TX:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            IDE_RAISE(InvalidXaState);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[aXid->getState()] ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginCommit(mmdXaContext *aXaContext, mmdXid *aXid)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    //IDE_TEST_RAISE(aXaContext->mSession->isXaAssociated() == ID_TRUE, NotAssociated);
    IDE_TEST_RAISE(aXaContext->mSession->getXaAssocState()
                   != MMD_XA_ASSOC_STATE_NOTASSOCIATED, InvalidXaSession);

    IDE_TEST_RAISE(aXid == NULL, XidNotExist);

    switch (aXid->getState())
    {
        case MMD_XA_STATE_IDLE:
            IDE_TEST_RAISE(!(aXaContext->mFlag & TMONEPHASE), InvalidXaState);

        case MMD_XA_STATE_PREPARED:
            aXaContext->mReturnValue = XA_OK;
            break;

        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
            aXaContext->mReturnValue = XA_HEURCOM;
            IDE_RAISE(HeuristicState); /* BUG-20728 */
            break;

        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
            aXaContext->mReturnValue = XA_HEURRB;
            IDE_RAISE(HeuristicState); /* BUG-20728 */
            break;

        case MMD_XA_STATE_ACTIVE:
        case MMD_XA_STATE_NO_TX:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            IDE_RAISE(InvalidXaState);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[aXid->getState()] ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(HeuristicState);
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginRollback(mmdXaContext *aXaContext, mmdXid *aXid, mmdXaWaitMode  *aWaitMode)
{
    //fix BUG-22033
    *aWaitMode = MMD_XA_NO_WAIT;

    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    IDE_TEST_RAISE(aXaContext->mSession->getXaAssocState()
                   == MMD_XA_ASSOC_STATE_ASSOCIATED, NotAssociated);

    // bug-27071: xa: xid null ref
    IDE_TEST_RAISE(aXid == NULL, XidNotExist);

    switch (aXid->getState())
    {
        case MMD_XA_STATE_IDLE:
        case MMD_XA_STATE_PREPARED:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            aXaContext->mReturnValue = XA_OK;
            break;

        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
            aXaContext->mReturnValue = XA_HEURCOM;
            IDE_RAISE(HeuristicState); /* BUG-20728 */
            break;

        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
            aXaContext->mReturnValue = XA_HEURRB;
            IDE_RAISE(HeuristicState); /* BUG-20728 */
            break;

        case MMD_XA_STATE_ACTIVE:
            *aWaitMode = MMD_XA_WAIT;
            break;
        case MMD_XA_STATE_NO_TX:
            IDE_RAISE(InvalidXaState);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(NotAssociated);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_ASSOCIATED_BRANCH ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    // bug-27071: xa: xid null ref
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[aXid->getState()] ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(HeuristicState);
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginForget(mmdXaContext *aXaContext, mmdXid *aXid)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    //IDE_TEST_RAISE(aXaContext->mSession->isXaAssociated() == ID_TRUE, NotAssociated);
    IDE_TEST_RAISE(aXaContext->mSession->getXaAssocState()
                   == MMD_XA_ASSOC_STATE_ASSOCIATED, NotAssociated);

    IDE_TEST_RAISE(aXid == NULL, XidNotExist);

    switch (aXid->getState())
    {
        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
            aXaContext->mReturnValue = XA_OK;
            break;

        case MMD_XA_STATE_IDLE:
        case MMD_XA_STATE_ACTIVE:
        case MMD_XA_STATE_PREPARED:
        case MMD_XA_STATE_NO_TX:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            IDE_RAISE(InvalidXaState);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(NotAssociated);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_ASSOCIATED_BRANCH ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[aXid->getState()] ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmdXa::beginRecover(mmdXaContext *aXaContext)
{
    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);
    IDE_TEST_RAISE(aXaContext->mSession->getXaAssocState()
                   != MMD_XA_ASSOC_STATE_NOTASSOCIATED, NotAssociated);

    aXaContext->mReturnValue = XA_OK;

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidXaSession);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_SESSION ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(NotAssociated);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_ASSOCIATED_BRANCH ));
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mmdXa::open(mmdXaContext *aXaContext)
{
    // bug-24840 divide xa log: IDE_SERVER -> IDE_XA
    ideLog::logLine(IDE_XA_1, "OPEN (%d)", aXaContext->mSession->getSessionID());

    IDE_TEST(beginOpen(aXaContext) != IDE_SUCCESS);

    /* BUG-45844 (Server-Side) (Autocommit Mode) Multi-Transaction�� �����ؾ� �մϴ�. */
    IDE_TEST( aXaContext->mSession->setGlobalTransactionLevel( 2 ) != IDE_SUCCESS );

    aXaContext->mSession->setXaSession(ID_TRUE);
    aXaContext->mSession->setXaAssocState(MMD_XA_ASSOC_STATE_NOTASSOCIATED);
    /* BUG-20850 xa_open ���ǵ� local transaction ���� ���񽺰� ������ */
    aXaContext->mSession->setSessionState(MMC_SESSION_STATE_SERVICE);

    return;

    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }
    }

    ideLog::logLine(IDE_XA_0, "OPEN ERROR -> %d", aXaContext->mReturnValue);

}

void mmdXa::close(mmdXaContext *aXaContext)
{
    mmcSession *sSession = aXaContext->mSession;
    //fix BUG-21527
    ID_XID     *sXidValue;
    mmdXid     *sXid;
    mmdXaState  sXaState;
    //fix BUG-22349 XA need to handle state.
    UInt        sState = 0;
    //fix BUG-21794
    iduList         *sXidList;
    iduListNode     *sIterator;
    iduListNode     *sNodeNext;
    mmdIdXidNode    *sXidNode;
    //fix BUG-XA close�� record-lock dead-lock�� �߻��Ҽ� ����.
    mmcSessID       sAssocSessionID;
    mmcSessID       sSessionID;

    ideLog::logLine(IDE_XA_1, "CLOSE (%d)", aXaContext->mSession->getSessionID());

    IDE_TEST(beginClose(aXaContext) != IDE_SUCCESS);
    if (aXaContext->mSession->getXaAssocState() == MMD_XA_ASSOC_STATE_ASSOCIATED)
    {
        sXidValue = aXaContext->mSession->getLastXid();
        end(aXaContext,sXidValue);
        IDE_TEST(aXaContext->mReturnValue != XA_OK);
    }

    /* BUG-20734 */
    //fix BUG-21527
    sXidList   = aXaContext->mSession->getXidList();

    //fix BUG-XA close�� record-lock dead-lock�� �߻��Ҽ� ����.
    sSessionID = aXaContext->mSession->getSessionID();

    IDU_LIST_ITERATE_SAFE(sXidList,sIterator,sNodeNext)
    {
        sXidNode = (mmdIdXidNode*) sIterator->mObj;
        sXidValue = &(sXidNode->mXID);
        IDE_TEST(mmdXa::fix(&sXid, sXidValue, MMD_XA_DO_LOG) != IDE_SUCCESS);
        //fix BUG-22349 XA need to handle state.
        sState = 1;

        if( sXid != NULL)
        {
            sXaState = sXid->getState();
            //fix BUG-XA close�� record-lock dead-lock�� �߻��Ҽ� ����.
            sAssocSessionID = sXid->getAssocSessionID();
            //fix BUG-22349 XA need to handle state.
            sState = 0;
            IDE_ASSERT(mmdXa::unFix(sXid, sXidValue, MMD_XA_NONE) == IDE_SUCCESS);
            switch (sXaState)
            {
                case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
                case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
                case MMD_XA_STATE_PREPARED:
                case MMD_XA_STATE_NO_TX:
                    //fix BUG-21794
                    IDU_LIST_REMOVE(&(sXidNode->mLstNode));
                    IDE_ASSERT(mmdXidManager::free(sXidNode) == IDE_SUCCESS);
                    break;

                case MMD_XA_STATE_IDLE:
                case MMD_XA_STATE_ACTIVE:
                case MMD_XA_STATE_ROLLBACK_ONLY:  /* BUG-44976 */
                     //fix BUG-XA close�� record-lock dead-lock�� �߻��Ҽ� ����.
                    if( sSessionID  == sAssocSessionID)
                    {
                        rollback(aXaContext,sXidValue);
                        IDE_TEST(aXaContext->mReturnValue != XA_OK);
                    }
                    else
                    {
                        //fix BUG-XA close�� record-lock dead-lock�� �߻��Ҽ� ����.
                        IDU_LIST_REMOVE(&(sXidNode->mLstNode));
                        IDE_ASSERT(mmdXidManager::free(sXidNode) == IDE_SUCCESS);
                    }
                    break;
                default:
                    IDE_ASSERT(0);
                    break;
            }
        }
        else
        {
            IDU_LIST_REMOVE(&(sXidNode->mLstNode));
            IDE_ASSERT(mmdXidManager::free(sXidNode) == IDE_SUCCESS);
            //fix BUG-22349 XA need to handle state.
            sState = 0;
        }

    }// IDU_LIST_ITERATE_SAFE
    sSession->setSessionState(MMC_SESSION_STATE_SERVICE);
    sSession->setXaSession(ID_FALSE);

    return;
    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }
        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, sXidValue, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }
    }

    ideLog::logLine(IDE_XA_0, "CLOSE ERROR -> %d", aXaContext->mReturnValue);
}

/* BUG-18981 */
void mmdXa::start(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmcSession       *sSession   = aXaContext->mSession;
    mmdXid           *sXid       = NULL;
    /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
     XaFixWithAdd latch duration�� ���̱� ���Ͽ� xid object�� �̸� �Ҵ��ϰ�
     Begin�մϴ�. */
    mmdXid           *sXid2Add   = NULL;
    mmdXaAssocState   sXaAssocState;
    //fix BUG-22349 XA need to handle state.
    UInt              sState = 0;
    //fix BUG-21795
    idBool            sNeed2RestoreLocalTx = ID_FALSE;

    //BUG-25050 Xid ���
    UChar             sXidString[XID_DATA_MAX_LEN];
    /* PROJ-2436 ADO.NET MSDTC */
    mmcTransEscalation sTransEscalation = sSession->getTransEscalation();
    mmcTransObj       *sTransToEscalate = NULL;

    if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
    {
        /* nothing to do */
    }
    else
    {
        sTransToEscalate = sSession->getTransPtr();
    }

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_1, "START (%d)(XID:%s)",
                aXaContext->mSession->getSessionID(),
                sXidString);    //BUG-25050 Xid ���

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_LOG) != IDE_SUCCESS);
    sState = 1;

reCheck:

    if (sXid != NULL)
    {
        sXid->lock();
        //fix BUG-22349 XA need to handle state.
        sState = 2;
    }

    sXaAssocState = sSession->getXaAssocState();
    IDE_TEST(beginStart(aXaContext, sXid, sXaAssocState,&sState) != IDE_SUCCESS);
    
    /* BUG-20850 global transation �������� local transaction �� ���¸� �����Ѵ�. */
    if (sNeed2RestoreLocalTx != ID_TRUE)
    {
        if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
        {
            IDE_TEST_RAISE(cleanupLocalTrans(sSession) != IDE_SUCCESS, LocalTransError);
            sNeed2RestoreLocalTx = ID_TRUE;
        }
        else
        {
            prepareLocalTrans(sSession);
        }
    }

    if (sXid == NULL)
    {
        /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
           XaFixWithAdd latch duration�� ���̱� ���Ͽ� xid object�� �̸� �Ҵ��ϰ�
           Begin�մϴ�. */
        IDE_TEST(mmdXidManager::alloc(&sXid2Add, aXid, sTransToEscalate)
                != IDE_SUCCESS);

        if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
        {
            sXid2Add->beginTrans(aXaContext->mSession);
        }
        else
        {
            /* already began */
        }

        sXid2Add->lock();
        
        IDE_TEST_RAISE(mmdXa::fixWithAdd(aXaContext,
                                         sXid2Add,
                                         &sXid,
                                         aXid,
                                         MMD_XA_DO_LOG) != IDE_SUCCESS,
                errorFixWithAdd);

        if (sXid2Add != sXid)
        {
            /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
              �̹� ��ϵ� ��쿡��, �̸��Ҵ�� xid object�� �����Ѵ�  */
            sXid2Add->unlock();

            if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
            {
                sXid2Add->rollbackTrans(NULL);

                IDE_ASSERT(mmdXidManager::free(sXid2Add, ID_TRUE)
                        == IDE_SUCCESS);
            }
            else
            {
                /* not needed rollback */

                IDE_ASSERT(mmdXidManager::free(sXid2Add, ID_FALSE)
                        == IDE_SUCCESS);
            }

            sXid2Add = NULL;

            goto reCheck;
        }
        else
        {
            sState = 2;        //BUG-28994 [CodeSonar]Null Pointer Dereference

            if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
            {
                sSession->setTrans(sXid->getTransPtr());
                sSession->setSessionBegin( ID_TRUE );
            }
            else
            {
                /* nothing to do */
            }
        }
    }
    else
    {
        sSession->setTrans(sXid->getTransPtr());
        sSession->setSessionBegin( ID_TRUE );
    }

    sXid->setState(MMD_XA_STATE_ACTIVE);
    //fix BUG-22669 XID list�� ���� performance view �ʿ�.
    //fix BUG-23656 session,xid ,transaction�� ������ performance view�� �����ϰ�,
    //�׵鰣�� ���踦 ��Ȯ�� �����ؾ� ��.
    // XID�� session�� associate��Ŵ.
    sXid->associate( aXaContext->mSession);
    
    /*
     * BUGBUG: IsolationLevel�� ���⼭ �����ص� �ǳ�?
     */

    // Session Info �߿��� transaction ���� �� �ʿ��� ���� ����

    sSession->setSessionInfoFlagForTx(sSession->getIsolationLevel(), /* BUG-39817 */
                                      SMI_TRANSACTION_REPL_DEFAULT,
                                      SMI_TRANSACTION_NORMAL,
                                      (idBool)mmuProperty::getCommitWriteWaitMode());

    sSession->setSessionState(MMC_SESSION_STATE_SERVICE);
    /* BUG-20850 global transaction �� NONAUTOCOMMIT ��忩�� �� */
    sSession->setGlobalCommitMode(MMC_COMMITMODE_NONAUTOCOMMIT);
    sSession->setXaAssocState(MMD_XA_ASSOC_STATE_ASSOCIATED);
    sSession->setTransEscalation(MMC_TRANS_DO_NOT_ESCALATE);
    //fix BUG-21891,40772
    IDE_TEST( sSession->addXid(aXid) != IDE_SUCCESS );
    //fix BUG-22349 XA need to handle state.
    sState = 1;
    sXid->unlock();

    //fix BUG-22349 XA need to handle state.
    sState = 0;
    IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);

    return;

    IDE_EXCEPTION(LocalTransError);
    {
        aXaContext->mReturnValue = XAER_OUTSIDE;
    }
    IDE_EXCEPTION(errorFixWithAdd);
    {
        /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
           �������߻��� ��쿡��,�̸��Ҵ�� xid object�� �����Ѵ� */
        sState =1;
        sXid2Add->unlock();

        if (sTransEscalation == MMC_TRANS_DO_NOT_ESCALATE)
        {
            sXid2Add->rollbackTrans(NULL);

            IDE_ASSERT(mmdXidManager::free(sXid2Add, ID_TRUE) == IDE_SUCCESS);
        }
        else
        {
            IDE_ASSERT(mmdXidManager::free(sXid2Add, ID_FALSE) == IDE_SUCCESS);
        }
    }
    
    IDE_EXCEPTION_END;
    {
        /* BUG-20850 xa_start ���н�, global transaction ���� ����
           local transaction ���·� �����Ѵ�. */
        //fix BUG-21795
        if(sNeed2RestoreLocalTx == ID_TRUE)
        {
            restoreLocalTrans(sSession);
        }

        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }

        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch


    }

    ideLog::logLine(IDE_XA_0, "START ERROR -> %d(XID:%s)",
                aXaContext->mReturnValue,
                sXidString);        //BUG-25050 Xid ���
}

/* BUG-18981 */
void mmdXa::end(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmcSession *sSession   = aXaContext->mSession;
    //fix BUG-21527
    ID_XID          *sXidValue;
    mmdXid          *sXid        = NULL;
    mmdXid          *sSessionXid = NULL;
    mmdXaAssocState sXaAssocState;
    UInt            sState = 0;


    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_1, "END (%d)(XID:%s)",
                sSession->getSessionID(),
                sXidString);            //BUG-25050 Xid ���

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_LOG) != IDE_SUCCESS);
    sState = 1;

    //fix BUG-21527
    sXidValue = sSession->getLastXid();
    //fix BUG-22561 xa_end�ÿ� Transaction Branch Assoc���� state�� suspend�϶� �������̰�
    //�߸���.
    IDE_TEST_RAISE(sXidValue == NULL, PROTO_ERROR);
    IDE_TEST(mmdXaSafeFind(&sSessionXid, sXidValue, MMD_XA_DO_LOG) != IDE_SUCCESS);
    sXaAssocState = sSession->getXaAssocState();
    if (sXid != NULL)
    {
        sXid->lock();
        sState = 2;
    }
    if(sSessionXid != NULL)
    {
        //fix BUG-22561 xa_end�ÿ� Transaction Branch Assoc���� state�� suspend�϶� �������̰�
        //�߸���.
        IDE_TEST(beginEnd(aXaContext,sXid, sSessionXid,sXaAssocState) != IDE_SUCCESS);
    }
    else
    {
        /* BUG-29078
         * NOTA Error
         */
        IDE_TEST_RAISE(sSessionXid == NULL, XidNotExist)
    }
    //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
    //statement end�� �ڵ�����ó���ؾ� �մϴ�.
    /* PROJ-1381 FAC : End �������� Non-Holdable Fetch�� �����Ѵ�.
     * Holdable Fetch�� Commit, Rollback ������ ��� ó������ �����ȴ�. */
    IDE_ASSERT(sSession->closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) == IDE_SUCCESS);
    IDE_TEST(sXid->addAssocFetchListItem(sSession) != IDE_SUCCESS);

    IDU_FIT_POINT_RAISE( "mmdXa::end::lock::isAllStmtEndExceptHold",  
                         StmtRemainError);
    IDE_TEST_RAISE(sSession->isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);

    //fix BUG-22561 xa_end�ÿ� Transaction Branch Assoc���� state�� suspend�϶�
    //�������̰� �߸���.
    if(sState == 2)
    {
        if (aXaContext->mFlag & TMFAIL )
        {
            sXid->setState(MMD_XA_STATE_ROLLBACK_ONLY);
            aXaContext->mReturnValue = XA_RBROLLBACK;
        }
        else
        {
            sXid->setState(MMD_XA_STATE_IDLE);
        }

        sXid->setHeuristicXaEnd(ID_FALSE);        //BUG-29351
        
        sXid->unlock();
        sState = 1;
    }

    IDE_DASSERT( sState == 1);
    sState = 0;

    IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);

    sSession->setSessionState(MMC_SESSION_STATE_READY);


    switch (sXaAssocState)
    {
        case MMD_XA_ASSOC_STATE_ASSOCIATED:
            if (aXaContext->mFlag & TMSUSPEND )
            {
                sSession->setXaAssocState(MMD_XA_ASSOC_STATE_SUSPENDED);
            }
            else
            {
                sSession->setXaAssocState(MMD_XA_ASSOC_STATE_NOTASSOCIATED);
            }
            break;

        case MMD_XA_ASSOC_STATE_SUSPENDED:
            //fix BUG-22561 xa_end�ÿ� Transaction Branch Assoc���� state�� suspend�϶�
            //�������̰� �߸���.
            sSession->setXaAssocState(MMD_XA_ASSOC_STATE_NOTASSOCIATED);
            break;

        case MMD_XA_ASSOC_STATE_NOTASSOCIATED:
        default:
            break;
    }

    /* BUG-20850 global transaction ���� ��, global transaction ���� ����
       local transaction ���·� �����Ѵ�. */
    if(sXaAssocState != MMD_XA_ASSOC_STATE_SUSPENDED)
    {

        restoreLocalTrans(sSession);
    }
    else
    {
        // nothing to do.
    }

    return;

    IDE_EXCEPTION(PROTO_ERROR);
    {
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        aXaContext->mReturnValue = XAER_RMERR;
        ideLog::logLine(IDE_XA_0, "XA-WARNING !! XA END ERROR(%d) Statement Remain !!!! -> %d(XID:%s)",
                    sSession->getSessionID(),
                    aXaContext->mReturnValue,
                    sXidString);        //BUG-25050 Xid ���
    }
    IDE_EXCEPTION(XidNotExist);
    {
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION_END;
    {
        switch(sState )
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default:
                break;

        }
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }
    }

    ideLog::logLine(IDE_XA_0, "END ERROR(%d) -> %d(XID:%s)",
                sSession->getSessionID(),
                aXaContext->mReturnValue,
                sXidString);        //BUG-25050 Xid ���

}

/* BUG-29078
 * XA_ROLLBACK�� XA_END���� �������� ��쿡 �������� Statement�� ������ ��쿡, 
 * �ش� XID�� Heuristic�ϰ� XA_END�� �����Ѵ�.
 */
void mmdXa::heuristicEnd(mmcSession* aSession, ID_XID *aXid)
{
    mmcSession *sSession   = aSession;
    //fix BUG-21527
    ID_XID          *sXidValue;
    mmdXid          *sXid        = NULL;
    mmdXid          *sSessionXid = NULL;
    UInt            sState = 0;

    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    IDE_TEST( sSession == NULL );

    IDE_TEST( mmdXa::fix(&sXid, aXid, MMD_XA_DO_NOT_LOG) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sXid == NULL );
    
    IDE_TEST( sXid->getHeuristicXaEnd() != ID_TRUE );
    
    ideLog::logLine(IDE_XA_1, "Heuristic END (%d)(XID:%s)",
                sSession->getSessionID(),
                sXidString);            //BUG-25050 Xid ���

    //fix BUG-21527
    sXidValue = sSession->getLastXid();

    IDE_TEST_RAISE(sXidValue == NULL, occurErrorWithLog);
    IDE_TEST_RAISE(mmdXaSafeFind(&sSessionXid, sXidValue, MMD_XA_DO_LOG) != IDE_SUCCESS, occurErrorWithLog);
    
    if (sXid != NULL)
    {
        sXid->lock();
        sState = 2;
    }
    
    //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
    //statement end�� �ڵ�����ó���ؾ� �մϴ�.
    /* PROJ-1381 FAC : Rollback ó���� �ϹǷ� Commit �ȵ� Fetch�� ��� �ݴ´�. */
    IDE_TEST(sSession->closeAllCursor(ID_TRUE, MMC_CLOSEMODE_NON_COMMITED) != IDE_SUCCESS);

    IDU_FIT_POINT_RAISE( "mmdXa::heuristicEnd::lock::isAllStmtEndExceptHold", 
                         occurErrorWithLog);
    IDE_TEST_RAISE( sSession->isAllStmtEndExceptHold() != ID_TRUE, occurErrorWithLog );

    /* BUG-29078
     * If Heuristic XA_END is done, State is not to be SUSPEND. 
     * Because XA_END flag is must a TMSUCCESS.
     */
    if(sState == 2)
    {
        sXid->setState(MMD_XA_STATE_IDLE);
        sXid->setHeuristicXaEnd(ID_FALSE);
        sXid->unlock();
        sState = 1;
    }

    IDE_DASSERT(sState == 1);
    sState = 0;

    IDE_ASSERT( mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS );

    sSession->setSessionState(MMC_SESSION_STATE_READY);

    sSession->setXaAssocState(MMD_XA_ASSOC_STATE_NOTASSOCIATED);

    /* BUG-20850 global transaction ���� ��, global transaction ���� ����
       local transaction ���·� �����Ѵ�. */
    restoreLocalTrans(sSession);

    return;
    
    IDE_EXCEPTION(occurErrorWithLog);
    {
        ideLog::logLine(IDE_XA_0, "Heuristic END ERROR(%d) -> (XID:%s)",
                        sSession->getSessionID(),
                        sXidString);        //BUG-25050 Xid ���
    }
    IDE_EXCEPTION_END;
    {
        switch(sState )
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default:
                break;

        }
    }

}

/* BUG-18981 */
void mmdXa::prepare(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmdXid *sXid = NULL;
    //fix BUG-30343 Committing a xid can be failed in replication environment.
    SChar         *sErrorMsg;
    //fix BUG-22349 XA need to handle state.
    UInt    sState = 0;
    idBool  sReadOnly;
    UInt    sUnfixOpt = MMD_XA_NONE;

    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_1, "PREPARE (%d)(XID:%s)",
                aXaContext->mSession->getSessionID(),
                sXidString);            //BUG-25050 Xid ���

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_LOG) != IDE_SUCCESS);
    //fix BUG-22349 XA need to handle state.
    sState = 1;

    if (sXid != NULL)
    {
        sXid->lock();
        //fix BUG-22349 XA need to handle state.
        sState = 2;
    }

    IDE_TEST(beginPrepare(aXaContext, sXid) != IDE_SUCCESS);

    IDE_ASSERT(mmcTrans::isReadOnly(sXid->getTransPtr(), &sReadOnly) == IDE_SUCCESS);

    if (sReadOnly == ID_TRUE)
    {
        //dblink�� shard�� ���� commit�� �ʿ��� �� �ִ�.
        sReadOnly = dkiIsReadOnly( aXaContext->mSession->getDatabaseLinkSession() );
    }

    if (sReadOnly == ID_TRUE)
    {
        ideLog::logLine(IDE_XA_1, "PREPARE READ ONLY (%d) -> 0x%x(XID:%s)",
                    aXaContext->mSession->getSessionID(),
                    sXid,
                    sXidString);        //BUG-25050 Xid ���


        IDU_FIT_POINT("mmdXa::prepare::lock::prepareTransReadOnly" );
        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
        IDE_TEST(sXid->prepareTrans(aXaContext->mSession) != IDE_SUCCESS);
        //fix BUG-22349 XA need to handle state.
        sState = 1;
        sXid->unlock();
        //fix BUG-22349 XA need to handle state.
        sState = 0;
        IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);

        IDE_TEST(mmdXa::fix(&sXid, aXid,MMD_XA_DO_LOG) != IDE_SUCCESS);
        //fix BUG-22349 XA need to handle state.
        sState = 1;

        if (sXid != NULL)
        {
            sXid->lock();
            //fix BUG-22349 XA need to handle state.
            sState = 2;
            //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
            //fix BUG-30343 Committing a xid can be failed in replication environment.
            IDE_TEST_RAISE(sXid->commitTrans(aXaContext->mSession) != IDE_SUCCESS, commitError);


            aXaContext->mReturnValue = XA_RDONLY;
            //fix BUG-22349 XA need to handle state.
            sState = 1;
            sXid->unlock();

            sUnfixOpt = (MMD_XA_REMOVE_FROM_XIDLIST | MMD_XA_REMOVE_FROM_SESSION);
        }
        else
        {
            ideLog::logLine(IDE_XA_0, "PREPARE READ ONLY (%d) -> 0x%x(XID:%s)NOT FOUND",
                        aXaContext->mSession->getSessionID(),
                        sXid,
                        sXidString);        //BUG-25050 Xid ���
        }
    }
    else
    {
        IDU_FIT_POINT("mmdXa::prepare::lock::prepareTrans" );

        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
        IDE_TEST(sXid->prepareTrans(aXaContext->mSession) != IDE_SUCCESS);
        sState = 1;
        sXid->unlock();
    }
    //fix BUG-22349 XA need to handle state.
    sState = 0;
    IDE_TEST(mmdXa::unFix(sXid, aXid, sUnfixOpt) != IDE_SUCCESS);
    if (FLAG_IS_SET(sUnfixOpt, MMD_XA_REMOVE_FROM_SESSION))
    {
        aXaContext->mSession->removeXid(aXid);
    }

    return;
    
    IDE_EXCEPTION(commitError)
    {
        //fix BUG-30343 Committing a xid can be failed in replication environment.
        sErrorMsg =  ideGetErrorMsg(ideGetErrorCode());
        ideLog::logLine(IDE_XA_0, "Xid  Commit Error [%s], reason [%s]", sXidString,sErrorMsg);

    }

    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }

        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch
    }

    ideLog::logLine(IDE_XA_0, "PREPARE ERROR -> %d(XID:%s)",
                aXaContext->mReturnValue,
                sXidString);            //BUG-25050 Xid ���

}

/* BUG-18981 */
void mmdXa::commit(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmdXid         *sXid = NULL;
    mmcSession     *sSession = aXaContext->mSession;
    //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
    //statement end�� �ڵ�����ó���ؾ� �մϴ�.
    UInt           sErrorCode;
    SChar         *sErrorMsg;
    //fix BUG-22349 XA need to handle state.
    UInt            sState = 0;

    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_1, "COMMIT (%d)(XID:%s)",
                aXaContext->mSession->getSessionID(),
                sXidString);            //BUG-25050 Xid ���

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_LOG) != IDE_SUCCESS);
    //fix BUG-22349 XA need to handle state.
    sState = 1;

    if (sXid != NULL)
    {
        sXid->lock();
        //fix BUG-22349 XA need to handle state.
        sState = 2;
    }

    IDE_TEST(beginCommit(aXaContext, sXid) != IDE_SUCCESS);

    IDU_FIT_POINT_RAISE( "mmdXa::commit::lock::commitTrans", 
                         CommitError);
    //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
    //statement end�� �ڵ�����ó���ؾ� �մϴ�.
    //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
    IDE_TEST_RAISE(sXid->commitTrans(aXaContext->mSession) != IDE_SUCCESS,CommitError);

    //fix BUG-22349 XA need to handle state.
    sState = 1;
    sXid->unlock();

    //fix BUG-22349 XA need to handle state.
    sState = 0;
    IDE_TEST(mmdXa::unFix(sXid, aXid, MMD_XA_REMOVE_FROM_XIDLIST) != IDE_SUCCESS);
    sSession->removeXid(aXid);

    return;

    IDE_EXCEPTION(CommitError);
    {
        //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
        //statement end�� �ڵ�����ó���ؾ� �մϴ�.
        aXaContext->mReturnValue = XAER_RMERR;
        sErrorCode = ideGetErrorCode();
        sErrorMsg =  ideGetErrorMsg(sErrorCode);
        if(sErrorMsg != NULL)
        {
            ideLog::logLine(IDE_XA_0, "XA-WARNING !! XA COMMIT  ERROR(%d) : Reason %s (XID:%s)",
                        aXaContext->mSession->getSessionID(),
                        sErrorMsg,
                        sXidString);        //BUG-25050 Xid ���
        }
    }

    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }

        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch
    }

    ideLog::logLine(IDE_XA_0, "COMMIT ERROR -> %d(XID:%s)",
                aXaContext->mReturnValue,
                sXidString);        //BUG-25050 Xid ���
}

/* BUG-18981 */
void mmdXa::rollback(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmdXid        *sXid = NULL;
    mmcSession    *sSession = aXaContext->mSession;
    UInt           sCurSleepTime = 0;
    //fix BUG-23776, XA ROLLBACK�� XID�� ACTIVE�϶� ���ð���
    //QueryTime Out�� �ƴ϶�,Property�� �����ؾ� ��.
    UInt           sMaxWaitTime =   mmuProperty::getXaRollbackTimeOut() ;

    mmdXaWaitMode  sWaitMode;
    mmdXaLogFlag   sXaLogFlag = MMD_XA_DO_LOG;
    //fix BUG-22349 XA need to handle state.
    UInt            sState = 0;
    UInt            sUnfixOpt = MMD_XA_NONE;

    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];
    // BUG-25020
    mmcSession    *sAssocSession;
    idBool         sIsSetTimeout = ID_FALSE;

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_1, "ROLLBACK (%d)(XID:%s)",
                aXaContext->mSession->getSessionID(),
                sXidString);        //BUG-25050 Xid ���

  retry:
    IDE_TEST(mmdXa::fix(&sXid, aXid, sXaLogFlag) != IDE_SUCCESS);
    //fix BUG-22349 XA need to handle state.
    sState = 1;

    if (sXid != NULL)
    {
        sXid->lock();
        //fix BUG-22349 XA need to handle state.
        sState = 2;

        IDU_FIT_POINT("mmdXa::rollback::lock::beginRollback");
        IDE_TEST(beginRollback(aXaContext, sXid,&sWaitMode) != IDE_SUCCESS);
        if(sWaitMode == MMD_XA_WAIT)
        {
            if ( sIsSetTimeout == ID_FALSE )
            {
                // Fix BUG-25020 XA�� Active �����ε�, rollback�� ����
                // ��ٷ� Query Timeout���� ó���Ͽ� �ش� Transaction�� Rollback �ǵ��� ��.
                sAssocSession = sXid->getAssocSession();

                //BUG-25323 Trace Code...
                if (sAssocSession == NULL)
                {
                    ideLog::logLine(IDE_XA_0, "AssocSession is NULL(%d) -> (XID:%s)",
                    aXaContext->mSession->getSessionID(),
                    sXidString);
                }
                else
                {
                    mmtSessionManager::setQueryTimeoutEvent(sAssocSession,
                                                            sAssocSession->getInfo()->mCurrStmtID);
                    sIsSetTimeout = ID_TRUE;
                    /* BUG-29078
                     * ��� Statement�� ����� �����̸�, XA_ROLLBACK_TIMEOUT�ñ��� ����ϰ�,
                     * Statement�� �����ִ� ����̸�, �ش� Statement�� ����� �Ŀ� Heuristic XA_END�� �����Ѵ�.
                     */
                    /* PROJ-1381 FAC : Holdable Fetch�� transaction�� ������ ���� �ʴ´�. */
                    if (sAssocSession->isAllStmtEndExceptHold() != ID_TRUE)
                    {
                        sXid->setHeuristicXaEnd(ID_TRUE);
                    }
                }
            }

            if( sXid->getHeuristicXaEnd() != ID_TRUE )
            {
                /* BUG-29078
                 * Statement�� ��� ����� ���, XA_ROLLBACK_TIMEOUT Property �ð���ŭ
                 * XA_END�� ���ŵ� ������ ����Ѵ�.
                 */
                if(sCurSleepTime <  sMaxWaitTime)
                {
                    //fix BUG-22349 XA need to handle state.
                    sState = 1;
                    sXid->unlock();
                    sState = 0;
                    IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
                    idlOS::sleep(1);
                    sCurSleepTime += 1;
                    sXaLogFlag = MMD_XA_DO_NOT_LOG;
                    goto retry;
                }
                else
                {
                    //fix BUG-22306 XA ROLLBACK�� �ش� XID�� ACTIVE�϶�
                    //��� �Ϸ��� �̿� ���� rollbackó�� ��å�� �߸���.
                    // ���� XID�� DML�������̴�.
                    aXaContext->mReturnValue =  XAER_RMERR;
                }
            }
            else        // if getIsRollback is true..
            {
                /* BUG-29078 
                 * Statement�� �����ִ� ��쿡, Heuristic XA_END�� �ɶ����� ����Ѵ�.
                 */
                sState = 1;
                sXid->unlock();
                sState = 0;
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
                idlOS::sleep(1);
                sXaLogFlag = MMD_XA_DO_NOT_LOG;
                goto retry;
            }
        }
        else
        {
            //fix BUG-22033
            //nothing to do
        }

        //fix BUG-22306 XA ROLLBACK�� �ش� XID�� ACTIVE�϶�
        //��� �Ϸ��� �̿� ���� rollbackó�� ��å�� �߸���.
        // ���� XID�� DML�������̴�.
        IDE_TEST(sWaitMode == MMD_XA_WAIT);

        //BUG-25020
        if ( sIsSetTimeout == ID_TRUE )
        {
            ideLog::logLine(IDE_XA_0, "ROLLBACK By RM Session (%d)(XID:%s)",
                            aXaContext->mSession->getSessionID(),
                            sXidString);
        }

        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
        sXid->rollbackTrans(aXaContext->mSession);

        sState = 1;
        sXid->unlock();

        sUnfixOpt = (MMD_XA_REMOVE_FROM_XIDLIST | MMD_XA_REMOVE_FROM_SESSION);
    }
    else
    {
        //fix BUG-22033
        aXaContext->mReturnValue = XA_OK;
    }
    sState = 0;
    IDE_TEST(mmdXa::unFix(sXid, aXid, sUnfixOpt) != IDE_SUCCESS);
    if (FLAG_IS_SET(sUnfixOpt, MMD_XA_REMOVE_FROM_SESSION))
    {
        sSession->removeXid(aXid);
    }


    return;

    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }

        switch(sState )
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default:
                break;

        }//switch.
    }

    ideLog::logLine(IDE_XA_0, "ROLLBACK ERROR(%d) -> %d(XID:%s)",
                aXaContext->mSession->getSessionID(),
                aXaContext->mReturnValue,
                sXidString);            //BUG-25050 Xid ���

}

/* BUG-18981 */
void mmdXa::forget(mmdXaContext *aXaContext, ID_XID *aXid)
{
    //fix BUG-22349 XA need to handle state.
    UInt    sState = 0;
    mmdXid *sXid = NULL;

    //BUG-25050 Xid ���
    UChar          sXidString[XID_DATA_MAX_LEN];

    /* PROJ-2446 */ 
    mmcSession    *sSession = aXaContext->mSession;

    (void)idaXaConvertXIDToString(NULL, aXid, sXidString, XID_DATA_MAX_LEN);

    ideLog::logLine(IDE_XA_0, "FORGET (%d)(XID:%s)",
                aXaContext->mSession->getSessionID(),
                sXidString);        //BUG-25050 Xid ���

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_LOG) != IDE_SUCCESS);
    sState = 1;

    if (sXid != NULL)
    {
        sXid->lock();
        sState = 2;
    }

    IDU_FIT_POINT("mmdXa::forget::lock::beginForget");
    IDE_TEST(beginForget(aXaContext, sXid) != IDE_SUCCESS);
    sXid->setState(MMD_XA_STATE_NO_TX);

    IDE_TEST( mmdManager::removeHeuristicTrans( sSession->getStatSQL(), /* PROJ-2446 */
                                                aXid ) 
              != IDE_SUCCESS );

    sState = 1;
    sXid->unlock();

    sState = 0;
    IDE_TEST(mmdXa::unFix(sXid, aXid, MMD_XA_REMOVE_FROM_XIDLIST) != IDE_SUCCESS);

    return;

    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }

        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 2:
                sXid->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch

    }

    ideLog::logLine(IDE_XA_0, "FORGET ERROR -> %d(XID:%s)",
                aXaContext->mReturnValue,
                sXidString);            //BUG-25050 Xid ���
}

/* BUG-18981 */
void mmdXa::recover(mmdXaContext  *aXaContext,
                    ID_XID       **aPreparedXids,
                    SInt          *aPreparedXidsCnt,
                    ID_XID       **aHeuristicXids,
                    SInt          *aHeuristicXidsCnt)
{
    ID_XID         sXid;
    timeval        sTime;
    smiCommitState sTxState;
    SInt           sSlotID = -1;
    SInt           i;
    //fix BUG-22383  [XA] xa_recover���� xid ����Ʈ ������� �Ҵ繮��.
    SInt           sPreparedMax = smiGetTransTblSize();

    ID_XID        *sXidList = NULL;
    
    /* PROJ-2446 */ 
    mmcSession    *sSession = aXaContext->mSession;

    ideLog::logLine(IDE_XA_0, "RECOVER (%d)", aXaContext->mSession->getSessionID());

    IDE_TEST(beginRecover(aXaContext) != IDE_SUCCESS);

    // BUG-20668
    // select from SYSTEM_.SYS_XA_HEURISTIC_TRANS_
     //fix BUG-27218 XA Load Heurisitc Transaction�Լ� ������ ��Ȯ�� �ؾ� �Ѵ�.
    IDE_TEST( mmdManager::loadHeuristicTrans( sSession->getStatSQL(),   /* PROJ-2446 */
                                              MMD_LOAD_HEURISTIC_XIDS_AT_XA_RECOVER, 
                                              aHeuristicXids, 
                                              aHeuristicXidsCnt ) 
              != IDE_SUCCESS );
             
    IDU_FIT_POINT_RAISE( "mmdXa::recover::malloc::XidList", 
                          InsufficientMemory );
            
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_MMT,
                                     ID_SIZEOF(ID_XID) * sPreparedMax,
                                     (void **)&sXidList,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );
    *aPreparedXids = sXidList;

    for (i=0; i <sPreparedMax; i++ )
    {
        IDE_TEST(smiXaRecover(&sSlotID, &sXid, &sTime, &sTxState) != IDE_SUCCESS);

        if (sSlotID < 0)
        {
            break;
        }

        idlOS::memcpy(&(sXidList[i]), &sXid, ID_SIZEOF(ID_XID));
    }

    if ( i  > 0)
    {
        *aPreparedXidsCnt = i;
    }
    aXaContext->mReturnValue = *aPreparedXidsCnt + *aHeuristicXidsCnt;

    return;

    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;
    {
        if (aXaContext->mReturnValue == XA_OK)
        {
            aXaContext->mReturnValue = XAER_RMERR;
        }
        *aPreparedXidsCnt = 0;
        *aHeuristicXidsCnt = 0;
    }

    ideLog::logLine(IDE_XA_0, "RECOVER ERROR -> %d", aXaContext->mReturnValue);
}

/* PROJ-2436 ADO.NET MSDTC */
void mmdXa::heuristicCompleted(mmdXaContext *aXaContext, ID_XID *aXid)
{
    mmdXid     *sXid     = NULL;
    UInt        sState   = 0;

    IDE_ASSERT(aXaContext != NULL);
    IDE_ASSERT(aXid != NULL);

    IDE_TEST_RAISE(aXaContext->mSession->isXaSession() != ID_TRUE, InvalidXaSession);

    IDE_TEST(mmdXa::fix(&sXid, aXid, MMD_XA_DO_NOT_LOG) != IDE_SUCCESS);
    sState = 1;

    IDE_TEST_RAISE(sXid == NULL, XidNotExist);
    
    sXid->lock();

    switch (sXid->getState())
    {
        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
            aXaContext->mReturnValue = XA_HEURCOM;
            break;

        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
            aXaContext->mReturnValue = XA_HEURRB;
            break;

        case MMD_XA_STATE_IDLE:
        case MMD_XA_STATE_PREPARED:
        case MMD_XA_STATE_ACTIVE:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            aXaContext->mReturnValue = XA_RETRY;
            break;

        case MMD_XA_STATE_NO_TX:
            aXaContext->mReturnValue = XAER_NOTA;
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    sXid->unlock();

    IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
    sState = 0;

    return;
    
    IDE_EXCEPTION(InvalidXaSession);
    {
        aXaContext->mReturnValue = XAER_PROTO;
    }
    IDE_EXCEPTION(XidNotExist);
    {
        aXaContext->mReturnValue = XAER_NOTA;
    }
    IDE_EXCEPTION_END;
    {
        switch (sState)
        {
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXid, aXid, MMD_XA_NONE) == IDE_SUCCESS);
            default:
                break;
        }
    }
}

IDE_RC mmdXa::cleanupLocalTrans(mmcSession *aSession)
{
    idBool       sReadOnly;
    mmcTransObj *sTrans = NULL;

    /* BUG-20850 local transaction �� readOnly �����̸� commit �� �� �� */
    if ( (aSession->getCommitMode() == MMC_COMMITMODE_NONAUTOCOMMIT) &&
         (aSession->isActivated() == ID_TRUE) )
    {
        sTrans = aSession->getTransPtr();
        if (sTrans != NULL)
        {
            IDE_ASSERT(mmcTrans::isReadOnly(sTrans, &sReadOnly) == IDE_SUCCESS);
            if (sReadOnly == ID_TRUE)
            {
                /* PROJ-1381 FAC : Commit �ϹǷ� Holdable Fetch�� �����Ѵ�. */
                IDE_ASSERT(aSession->closeAllCursor(ID_TRUE, MMC_CLOSEMODE_REMAIN_HOLD) == IDE_SUCCESS);
                aSession->lockForFetchList();
                IDU_LIST_JOIN_LIST(aSession->getCommitedFetchList(), aSession->getFetchList());
                aSession->unlockForFetchList();
                IDE_TEST_RAISE(aSession->isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);

                IDE_TEST(mmcTrans::commit(sTrans, aSession) != IDE_SUCCESS);
                aSession->setNeedLocalTxBegin(ID_TRUE);
                aSession->setActivated(ID_FALSE);
            }
        }
    }
    /* BUG-20850 local transaction �� active �����̸� ���� �߻� */
    /* PROJ-1381 FAC */
    IDE_TEST_RAISE(aSession->isAllStmtEndExceptHold() != ID_TRUE, StmtRemainError);
    IDE_TEST_RAISE(aSession->isActivated() != ID_FALSE, AlreadyActiveError);

    /* BUG-20850 global transaction �������� local transaction �� ���¸� ������ �д�. */
    aSession->saveLocalCommitMode();
    aSession->saveLocalTrans();

    return IDE_SUCCESS;

    IDE_EXCEPTION(AlreadyActiveError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_MMC_TRANSACTION_ALREADY_ACTIVE));
    }
    IDE_EXCEPTION(StmtRemainError);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_OTHER_STATEMENT_REMAINS));
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* PROJ-2436 ADO.NET MSDTC */
void mmdXa::prepareLocalTrans(mmcSession *aSession)
{
    aSession->saveLocalCommitMode();
    aSession->allocLocalTrans();
    aSession->setNeedLocalTxBegin(ID_TRUE);
}

void mmdXa::restoreLocalTrans(mmcSession *aSession)
{
    idBool      sIsDummyBegin = ID_FALSE;

    /* BUG-20850 global transaction ���� ��, global transaction ���� ����
       local transaction ���·� �����Ѵ�. */
    aSession->setSessionState(MMC_SESSION_STATE_SERVICE);
    aSession->restoreLocalTrans();
    aSession->restoreLocalCommitMode();

    if(aSession->getNeedLocalTxBegin() == ID_TRUE)
    {
        mmcTrans::begin( aSession->getTransPtr(),
                         aSession->getStatSQL(),
                         aSession->getSessionInfoFlagForTx(),
                         aSession,
                         &sIsDummyBegin );
        aSession->setNeedLocalTxBegin(ID_FALSE);
    }
    aSession->setActivated(ID_FALSE);

}

void mmdXa::terminateSession(mmcSession *aSession)
{
    //fix BUG-21527
    ID_XID          *sXidValue;
    mmdXid          *sXid;
    //fix BUG-21794
    iduList         *sXidList;
    iduListNode     *sIterator;
    iduListNode     *sNodeNext;
    mmdIdXidNode    *sXidNode;
    UInt             sUnfixOpt = MMD_XA_NONE;

    //fix BUG-21862
    ideLog::logLine(IDE_XA_1, "terminate session (%d)", aSession->getSessionID());
    sXidList = aSession->getXidList();
    IDU_LIST_ITERATE_SAFE(sXidList,sIterator,sNodeNext)
    {
        sXidNode = (mmdIdXidNode*) sIterator->mObj;
        sXidValue = &(sXidNode->mXID);
        /* ------------------------------------------------
         * BUG-20662
         *
         * mmcSession:finalize �κ��� XaSession �� ��쿡�� ȣ���.
         * client ������ ����� �Ϸ���� ����(xa_end,xa_prepare ���� ����)
         * transaction �� rollback ó���ؾ� �Ѵ�.
         * ----------------------------------------------*/
        //fix BUG-21527
        IDE_ASSERT(mmdXa::fix(&sXid, sXidValue,MMD_XA_DO_LOG) == IDE_SUCCESS);
        //fix BUG-21527
        if(sXid != NULL)
        {
            // fix BUG-23306 XA ���� ����� �ڽ��� ������ XID�� rollback ó�� �ؾ��մϴ�.
            if (sXid->getAssocSessionID() == aSession->getSessionID())
            {
                sXid->lock();

                /* BUG-20662 xa_end �� ���� �ʾ��� ��� */
                if (aSession->getXaAssocState() == MMD_XA_ASSOC_STATE_ASSOCIATED)
                {
                    if (sXid->getState() == MMD_XA_STATE_ACTIVE)
                    {
                        sXid->setState(MMD_XA_STATE_IDLE);
                    }
                }

                /*
                * fix BUG-23656 session,xid ,transaction�� ������ performance view�� �����ϰ�,
                * �׵鰣�� ���踦 ��Ȯ�� �����ؾ� ��.
                * XID�� session�� dis-associate ��Ŵ.
                *
                * fix BUG-26164 Heuristic Commit/Rollback ���Ŀ� ���� ����������.
                * Xid_state�� ���� Transaction ó���� �޶�����.
                */
                sXid->disAssociate(sXid->getState());

                switch (sXid->getState())
                {
                    case MMD_XA_STATE_IDLE:
                    case MMD_XA_STATE_ACTIVE:
                    case MMD_XA_STATE_ROLLBACK_ONLY:
                        /* BUG-20662 xa_prepare �� �ϱ� ���� �����̸� */
                        //fix BUG-22479 xa_end�ÿ� fetch�߿� �ִ� statement�� ���Ͽ�
                        //statement end�� �ڵ�����ó���ؾ� �մϴ�.
                        /* PROJ-1381 FAC : Commit ���� ���� Fetch�� ��� �ݴ´�. */
                        IDE_ASSERT(aSession->closeAllCursor(ID_TRUE, MMC_CLOSEMODE_NON_COMMITED) == IDE_SUCCESS);
                        IDE_ASSERT(aSession->isAllStmtEndExceptHold() == ID_TRUE);

                        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
                        sXid->rollbackTrans(aSession);

                        sXid->unlock();

                        sUnfixOpt = MMD_XA_REMOVE_FROM_XIDLIST;
                        break;

                    case MMD_XA_STATE_PREPARED:
                    case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
                    case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
                    case MMD_XA_STATE_NO_TX:
                        sXid->unlock();
                        break;

                    default:
                        IDE_ASSERT(0);
                        break;
                }//switch

            }
        }//sXid
        IDE_ASSERT(mmdXa::unFix(sXid, sXidValue, sUnfixOpt) == IDE_SUCCESS);

        IDU_LIST_REMOVE(&(sXidNode->mLstNode));
        IDE_ASSERT(mmdXidManager::free(sXidNode) == IDE_SUCCESS) ;
    }//IDU_LIST

    if (aSession->getXaAssocState() == MMD_XA_ASSOC_STATE_ASSOCIATED)
    {
        //fix BUG-21862
        ideLog::logLine(IDE_XA_1, "terminate session  DO restoreLocalTrans(%d)", aSession->getSessionID());
        restoreLocalTrans(aSession);
    }
    else
    {
        //fix BUG-21862
        ideLog::logLine(IDE_XA_1, "terminate session  DONT'T  restoreLocalTrans(%d)", aSession->getSessionID());
    }
    //fix BUG-21948
    if(aSession->getSessionState() != MMC_SESSION_STATE_INIT)
    {
        aSession->setSessionState(MMC_SESSION_STATE_SERVICE);
    }
    else
    {
        //nothing to do
        // �̹� disconect protocol�� ���Ͽ� endSession�� �Ȱ���̴�.
    }
    aSession->setXaAssocState(MMD_XA_ASSOC_STATE_NOTASSOCIATED);
}




IDE_RC mmdXa::convertStringToXid(SChar *aStr, UInt aStrLen, ID_XID *aXID)
{
    SChar *sDotPos   = NULL;
    SChar *sTmp      = NULL;
    vULong            sFormatID;
    SChar           * sGlobalTxId;
    SChar           * sBranchQualifier;
    mtdIntegerType    sInt;
    SChar             sXidString[XID_DATA_MAX_LEN];

    /* bug-36037: invalid xid
       original string��  �����ϱ� ���� ���� */
    IDE_TEST_RAISE(aStrLen >= XID_DATA_MAX_LEN, ERR_INVALID_XID);
    idlOS::memcpy(sXidString, aStr, aStrLen);
    sXidString[aStrLen] = '\0';

    sTmp = sXidString;
    sDotPos = idlOS::strchr(sTmp, '.');
    IDE_TEST_RAISE(sDotPos == NULL, ERR_INVALID_XID);
    *sDotPos = 0;
    IDE_TEST(mtc::makeInteger(&sInt, (UChar *)sTmp, sDotPos-sTmp) != IDE_SUCCESS);
    sFormatID = sInt;

    sTmp = sDotPos + 1;
    sDotPos = idlOS::strchr(sTmp, '.');
    IDE_TEST_RAISE(sDotPos == NULL, ERR_INVALID_XID);
    *sDotPos = 0;
    sGlobalTxId = sTmp;
    sBranchQualifier = sDotPos + 1;

    idaXaStringToXid(sFormatID, sGlobalTxId, sBranchQualifier, aXID);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_XID )
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XID ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// fix BUG-23815 XA session level  self record lock dead-lock�� �߻��Ѱ�쿡
// TP monitor�� ������ ����ÿ� ������ XIDó���� ����ʿ�.
// �ҽ� �ڵ� �����ϸ鼭   commitForce�� RollbackForce ������ ��Ȯ�� �ϸ鼭
// ���� if���� ����.
IDE_RC mmdXa::commitForce( idvSQL   *aStatistics,
                           SChar    *aXIDStr, 
                           UInt      aXIDStrSize )
{
    ID_XID    sXID;
    mmdXid   *sXidObj = NULL;
    //fix BUG-30343 Committing a xid can be failed in replication environment.
    SChar         *sErrorMsg;
    UInt      sStage  = 0;

    ideLog::logLine(IDE_XA_0, "COMMIT FORCE(Trans_ID:%s)", aXIDStr);

    IDE_TEST(convertStringToXid(aXIDStr, aXIDStrSize, &sXID)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(mmdXa::fix(&sXidObj, &sXID,MMD_XA_DO_LOG) != IDE_SUCCESS, not_exist_XID);
    //fix BUG-22349 XA need to handle state.
    sStage = 1;

    IDE_TEST_RAISE(sXidObj == NULL, not_exist_XID);

    sXidObj->lock();
    //fix BUG-22349 XA need to handle state.
    sStage = 2;

    if ( sXidObj->getState() == MMD_XA_STATE_PREPARED )
    {
        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
        //fix BUG-30343 Committing a xid can be failed in replication environment.
        IDE_TEST_RAISE(sXidObj->commitTrans(NULL) != IDE_SUCCESS,commitError);

        /* bug-36037: invalid xid
           invalid xid�� ��� insertHeuri ���и� ����ϹǷ�
           ������ üũ ���� */
        mmdManager::insertHeuristicTrans( aStatistics,  /* PROJ-2446 */
                                          &sXID, 
                                          QCM_XA_COMMITTED );
        sXidObj->setState(MMD_XA_STATE_HEURISTICALLY_COMMITTED);
    }
    //fix BUG-22349 XA need to handle state.
    sStage = 1;
    sXidObj->unlock();

    //fix BUG-22349 XA need to handle state.
    sStage = 0;
    IDE_ASSERT(mmdXa::unFix(sXidObj, &sXID, MMD_XA_NONE) == IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(commitError)
    {
        //fix BUG-30343 Committing a xid can be failed in replication environment.
        sErrorMsg =  ideGetErrorMsg(ideGetErrorCode());
        ideLog::logLine(IDE_XA_0, "Xid  Commit Error [%s], reason [%s]",aXIDStr ,sErrorMsg);

    }
    IDE_EXCEPTION(not_exist_XID);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
    }

    IDE_EXCEPTION_END;
    {
        //fix BUG-22349 XA need to handle state.
        switch(sStage)
        {
            case 2:
                sXidObj->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXidObj, &sXID, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch

    }
    ideLog::logLine(IDE_XA_0, "COMMIT FORCE ERROR (Trans_ID:%s)", aXIDStr);

    return IDE_FAILURE;

}
// fix BUG-23815 XA session level  self record lock dead-lock�� �߻��Ѱ�쿡
// TP monitor�� ������ ����ÿ� ������ XIDó���� ����ʿ�.
// �ҽ� �ڵ� �����ϸ鼭   commitForce�� RollbackForce ������ ��Ȯ�� �ϸ鼭
// ���� if���� ����.
/*
    ���⼭ XID�� Ʈ����� branch�� �ǹ��Ѵ�.
    XA session A���� XID 100�� start
                      update t1 set C1=C1+1 WHERE C1=1;
                      xID 100 end.

                      XID 200�� start
                      update t1 set C1=C1+1 WHERE C1=1; <--- session level self recrod dead-lock�߻�.
                      xID 200 end.
   �̷��ѹ����� �ذ��ϱ� ���Ͽ�  rollback force ��ɾ�� XID 100���� rollback���Ѿ���.

*/
IDE_RC mmdXa::rollbackForce( idvSQL     *aStatistics,
                             SChar      *aXIDStr, 
                             UInt        aXIDStrSize )
{
    ID_XID         sXID;
    mmdXaState     sXIDState;
    mmdXid        *sXidObj = NULL;
    UInt           sStage  = 0;
    UInt           sUnfixOpt = MMD_XA_NONE;

    ideLog::logLine(IDE_XA_0, "ROLLBACK FORCE(Trans_ID:%s)", aXIDStr);

    IDE_TEST(convertStringToXid(aXIDStr, aXIDStrSize, &sXID)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(mmdXa::fix(&sXidObj, &sXID, MMD_XA_DO_LOG) != IDE_SUCCESS, not_exist_XID);
    //fix BUG-22349 XA need to handle state.
    sStage = 1;

    IDE_TEST_RAISE(sXidObj == NULL, not_exist_XID);
    sXidObj->lock();
    //fix BUG-22349 XA need to handle state.
    sStage = 2;
    sXIDState = sXidObj->getState();

    // fix BUG-23815 XA session level  self record lock dead-lock�� �߻��Ѱ�쿡
    // TP monitor�� ������ ����ÿ� ������ XIDó���� ����ʿ�.
    // MMD_XA_STATE_ROLLBACK_ONLY�� rollback�����ؾ���.
    if ( (sXIDState == MMD_XA_STATE_PREPARED ) || (sXIDState == MMD_XA_STATE_ROLLBACK_ONLY))
    {
        //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
        sXidObj->rollbackTrans(NULL);
        mmdManager::insertHeuristicTrans( aStatistics,  /* PROJ-2446 */
                                          &sXID, 
                                          QCM_XA_ROLLBACKED );
        sXidObj->setState(MMD_XA_STATE_HEURISTICALLY_ROLLBACKED);
        //fix BUG-22349 XA need to handle state.
        sStage = 1;
        sXidObj->unlock();
    }
    else
    {
      // fix BUG-23815 XA session level  self record lock dead-lock�� �߻��Ѱ�쿡
      // TP monitor�� ������ ����ÿ� ������ XIDó���� ����ʿ�.
      /*
           ���⼭ XID�� Ʈ����� branch�� �ǹ��Ѵ�.
           XA session A���� XID 100�� start
                      update t1 set C1=C1+1 WHERE C1=1;
                      xID 100 end.

                      XID 200�� start
                      update t1 set C1=C1+1 WHERE C1=1; <--- session level self recrod dead-lock�߻�.
                      xID 200 end.
          �̷��ѹ����� �ذ��ϱ� ���Ͽ�  rollback force ��ɾ�� XID 100���� rollback���Ѿ���.
      */
        if( sXIDState == MMD_XA_STATE_IDLE)
        {
            sXidObj->rollbackTrans(NULL);
            sStage = 1;
            sXidObj->unlock();
            sUnfixOpt = MMD_XA_REMOVE_FROM_XIDLIST;
        }
        else
        {
            //ignore, nothig to do
            sStage = 1;
            sXidObj->unlock();
        }
    }
    //fix BUG-22349 XA need to handle state.
    sStage = 0;
    IDE_TEST(mmdXa::unFix(sXidObj, &sXID, sUnfixOpt) != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION(not_exist_XID);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
    }
    IDE_EXCEPTION_END;
    {
        //fix BUG-22349 XA need to handle state.
        switch(sStage)
        {
            case 2:
                sXidObj->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXidObj, &sXID, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch

    }

    ideLog::logLine(IDE_XA_0, "ROLLBACK FORCE ERROR(Trans_ID:%s)", aXIDStr);

    return IDE_FAILURE;
}


/* BUG-25999
   Procedure�� �̿��Ͽ� Heuristic�ϰ� ó����, ����ڰ� �Է��� XID�� ������
 */
IDE_RC mmdXa::removeHeuristicXid( idvSQL    *aStatistics,
                                  SChar     *aXIDStr, 
                                  UInt       aXIDStrSize )
{
    ID_XID    sXID;
    mmdXid   *sXidObj = NULL;
    UInt      sState  = 0;

    ideLog::logLine(IDE_XA_0, "Remove Xid that processed Heuristically(XID:%s)", aXIDStr);

    IDE_TEST(convertStringToXid(aXIDStr, aXIDStrSize, &sXID)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(mmdXa::fix(&sXidObj, &sXID, MMD_XA_DO_LOG) != IDE_SUCCESS, not_exist_XID);
    sState = 1;

    IDE_TEST_RAISE(sXidObj == NULL, not_exist_XID);

    sXidObj->lock();
    sState = 2;

    switch ( sXidObj->getState() )
    {
        case MMD_XA_STATE_HEURISTICALLY_COMMITTED:
        case MMD_XA_STATE_HEURISTICALLY_ROLLBACKED:
            break;
        case MMD_XA_STATE_IDLE:
        case MMD_XA_STATE_ACTIVE:
        case MMD_XA_STATE_PREPARED:
        case MMD_XA_STATE_NO_TX:
        case MMD_XA_STATE_ROLLBACK_ONLY:
            IDE_RAISE(InvalidXaState);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    IDU_FIT_POINT("mmdXa::removeHeuristicXid::lock::XID");
    IDE_TEST( mmdManager::removeHeuristicTrans( aStatistics, /* PROJ-2446 */
                                                &sXID ) 
              != IDE_SUCCESS );

    sState = 1;

    sXidObj->unlock();
    sState = 0;

    IDE_TEST(mmdXa::unFix(sXidObj, &sXID, MMD_XA_REMOVE_FROM_XIDLIST) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_exist_XID);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_NOT_EXIST_XID ));
    }
    IDE_EXCEPTION(InvalidXaState);
    {
        IDE_SET(ideSetErrorCode( mmERR_ABORT_INVALID_XA_STATE, MMDXASTATE[sXidObj->getState()] ));
    }
    IDE_EXCEPTION_END;
    {
        //fix BUG-22349 XA need to handle state.
        switch(sState)
        {
            case 2:
                sXidObj->unlock();
            case 1:
                IDE_ASSERT(mmdXa::unFix(sXidObj, &sXID, MMD_XA_NONE) == IDE_SUCCESS);
            default :
                break;
        }//switch

    }

    ideLog::logLine(IDE_XA_0, "Remove Xid that processed Heuristically. Error (XID:%s)", aXIDStr);

    return IDE_FAILURE;
}


