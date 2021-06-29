/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idp.cpp 89621 2020-12-24 05:40:35Z emlee $
 * Description:
 *
 * �� Ŭ������ ��Ƽ���̽��� ������Ƽ�� �����ϴ� �� Ŭ�����̴�.
 *
 * �Ϲ������� ����ڴ� ::read(), ::update()���� ����Ѵ�.
 * ==============================================================
 *
 * idp ����� ��ü���� ������
 * ������Ƽ�� �����ϰ�, ����ڿ��� UI�� �����ϴ� class idp��
 * �� ������Ƽ�� �߻��� ���� API�� �����ϰ�, �����ϴ� idpBase ���� Ŭ����,
 * �׸���, idpBase �� ���� �� ����Ÿ Ÿ�Կ� ���ؼ� ������ ���� Ŭ������
 * �����ȴ�.
 * ���� ����� ����Ÿ Ÿ���� SInt, UInt, SLong, ULong, String ��
 * ������,
 * �� ������Ƽ�� �Ӽ��� ������.
 * ��, �� ������Ƽ�� �ܺ�/���� �Ӽ�, �б�����/���� �Ӽ�, ���ϰ�/�ټ��� �Ӽ�,
 *     ������ �˻����/�˻�ź� �Ӽ�, ����Ÿ Ÿ�� �Ӽ� ���� �ִ�.
 *
 * �� �Ӽ��� ������Ƽ�� ����� �� �־�����, �����ϰ� ����Ǿ�� �Ѵ�.
 *
 * ��Ϲ�)
 * idpDescResource.cpp�� �� ������Ƽ�� ���� ��� ����Ʈ�� �����Ѵ�.
 * ���⿡ ����� ������Ƽ�� �̸�/�Ӽ�/Ÿ�� ���� �����ָ� �ȴ�.
 *
 *
 * idpBase --> idpEachType(���)
 *
 *
 * ����ó��)
 * ������Ƽ �ε� �ܰ迡���� �����ڵ� �� ���� ����Ÿ�� �ε��Ǳ� �����̴�.
 * ����, ������Ƽ �ε��������� �߻��� ������, �� ������ IDE_FAILURE�� ���
 * idp::getErrorBuf()�� ȣ���Ͽ�, ����� ���� �޽����� ����ϸ� �ȴ�.
 *
 *
 *   ���� ���۷� ������ �����Ǵ� �ܰ��� �Լ�
 *     idp::initialize()
 *     idp::regist();
 *     idp::insert()
 *     idp::readConf()
 *
 *   �ý��� ���� ���۷� �����Ǵ� �ܰ� �Լ� (���� �޽��� �ε��Ŀ�  ȣ��Ǵ� �͵�)
 *     idp::read()
 *     idp::update()
 *     idp::setupBeforeUpdateCallback()
 *     idp::setupAfterUpdateCallback
 *
 *   BUGBUG - Logging Level ���� �� DDL�� ���� �� �־�� ��.
 *
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <idp.h>
#include <idErrorCode.h>

SChar    idp::mErrorBuf[IDP_ERROR_BUF_SIZE];
SChar   *idp::mHomeDir;
SChar   *idp::mConfName;
PDL_thread_mutex_t idp::mMutex;
UInt     idp::mCount;
iduList  idp::mArrBaseList[IDP_MAX_PROP_COUNT];
SChar   *idp::mSID = NULL;

extern IDE_RC registProperties();

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::initialize()
 *
 * Description :
 * idp ��ü �ʱ�ȭ�ϰ�, Descriptor�� ����Ѵ�.
 *
 * ---------------------------------------------------------------------------*/

IDE_RC idp::initialize(SChar *aHomeDir, SChar *aConfName)
{
    SChar *sHomeDir;
    UInt   sHomeDirLen;
    UInt   i;

    // ���� Ŭ�������� �߻��� ������ idp Ŭ������ ���۷� �޾ƿ��� ����.
    idpBase::initializeStatic(mErrorBuf);

    /* ---------------------
     *  [1] Home Dir ����
     * -------------------*/
    if (aHomeDir == NULL) // ����Ʈ ȯ�溯������ ������Ƽ �ε�
    {
        sHomeDir = idlOS::getenv(IDP_HOME_ENV);
    }
    else // HomeDir�� NULL�� �ƴ�
    {
        // BUG-29649 Initialize Global Variables of shmutil
        if(idlOS::strlen(aHomeDir) == 0)
        {
            sHomeDir = idlOS::getenv(IDP_HOME_ENV);
        }
        else
        {
            sHomeDir = aHomeDir;
        }
    }

    // HomeDir�� �����Ǿ��ִ��� �˻��Ѵ�.
    IDE_TEST_RAISE(sHomeDir == NULL, home_env_error);
    sHomeDirLen = idlOS::strlen(sHomeDir);
    IDE_TEST_RAISE(sHomeDirLen == 0, home_env_error);

    /* BUG-15311:
     * HomeDir �� ���� ���͸� �����ڰ� �پ�������
     * (��: /home/altibase/altibase_home4/)
     * ��Ÿ�� ����� ������ ���� ��θ���
     * ���͸� �����ڰ� �ݺ��Ǿ� ��Ÿ���ϴ�.
     * (��: /home/altibase/altibase_home4//dbs/user_tbs.dbf)
     * �̷��� ������ ������ ��θ����� �����Ϸ� �� ���,
     * ��Ÿ�� ����Ǿ��ִ´�� ���͸� �����ڸ� �ݺ��ؼ� �������� ������
     * ������ ������ ã�� �� ���ٴ� ������ ���ϴ�.
     * �� ������ �ذ��ϱ� ����,
     * HomeDir �� ���� ���͸� �����ڰ� �پ�������
     * �̸� ������ ���ڿ��� HomeDir�� ����ϵ��� �����մϴ�. */

    // HomeDir�� ���͸� �����ڷ� ������ �ʴ� ���
    if ( (sHomeDir[sHomeDirLen - 1] != '/') &&
         (sHomeDir[sHomeDirLen - 1] != '\\') )
    {
        // ȯ�溯�� �Ǵ� ���ڷ� ���� HomeDir�� �״�� mHomeDir�� �����Ѵ�.
        mHomeDir = sHomeDir;
    }
    // HomeDir�� ���͸� �����ڷ� ������ ���
    else
    {
        // �� ���� ���͸� �����ڸ� ������ �κ��� ���̸� ���Ѵ�.
        while (sHomeDirLen > 0)
        {
            if ( (sHomeDir[sHomeDirLen - 1] != '/') &&
                 (sHomeDir[sHomeDirLen - 1] != '\\') )
            {
                break;
            }
            sHomeDirLen--;
        }

        // to remove false alarms from codesonar test
        IDE_TEST_RAISE( sHomeDirLen >= ID_UINT_MAX -1, too_long_home_env_error);

        // �� ���� ���͸� �����ڸ� ������ HomeDir�� mHomeDir�� �����Ѵ�.
        mHomeDir = (SChar *)iduMemMgr::mallocRaw(sHomeDirLen + 1);
        IDE_ASSERT(mHomeDir != NULL);
        idlOS::strncpy(mHomeDir, sHomeDir, sHomeDirLen);
        mHomeDir[sHomeDirLen] = '\0';
    }

    /* ---------------------
     *  [2] Conf File  ����
     * -------------------*/

    if (aConfName == NULL)
    {
        mConfName = IDP_DEFAULT_CONFFILE;
    }
    else
    {
        if (idlOS::strlen(aConfName) == 0)
        {
            mConfName = IDP_DEFAULT_CONFFILE;
        }
        else
        {
            mConfName = aConfName;
        }
    }

    IDE_TEST_RAISE(idlOS::thread_mutex_init(&mMutex) != 0,
                   mutex_error);

    //mArrBaseList�� registProperties ���� �ʱ�ȭ �Ǿ�� ��.
    mCount = 0; // counter for mArrBaseList
    for(i = 0; i < IDP_MAX_PROP_COUNT; i++)
    {
        IDU_LIST_INIT(&mArrBaseList[i]);
    }

    // ������Ƽ Descriptor�� ����Ѵ�.
    IDE_TEST(registProperties() != IDE_SUCCESS);

    IDE_TEST(readPFile() != IDE_SUCCESS);

    /*Pfile�� Env�� ���� ������ SID�� Local Instance�� Property�� �ݿ��Ѵ�.*/
    IDE_TEST(setLocalSID() != IDE_SUCCESS);

    IDE_TEST(readSPFile() != IDE_SUCCESS);

    /*Source���� ���� ���� �켱������ ������ ���� ������Ƽ�� Memory Value�� �����Ͽ� �����Ѵ�.*/
    IDE_TEST(insertMemoryValueByPriority() != IDE_SUCCESS);

    IDE_TEST(verifyMemoryValues() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(too_long_home_env_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp initialize() Error : Too Long ALTIBASE_HOME Environment.(>=%"ID_UINT32_FMT")\n",
                        ID_UINT_MAX - 1);
    }

    IDE_EXCEPTION(home_env_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp initialize() Error : No [%s] Environment Exist.\n", IDP_HOME_ENV);
    }

    IDE_EXCEPTION(mutex_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp initialize() Error : Mutex Initialization Failed.\n");
    }

    IDE_EXCEPTION_END;

    ideSetErrorCodeAndMsg(idERR_ABORT_idp_Initialize_Error,
                          idp::getErrorBuf());
    return IDE_FAILURE;
}



/*-----------------------------------------------------------------------------
 * Name :
 *     idp::destroy()
 *
 * Description :
 * idp ��ü �����Ѵ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::destroy()
{
    UInt   i;
    UInt   j;

    /* remove property array */
    for (i = 0; i < mCount; i++)
    {
        iduListNode *sIterator = NULL;
        iduListNode *sNodeNext = NULL;

        IDU_LIST_ITERATE_SAFE( &mArrBaseList[i], sIterator, sNodeNext )
        {
            idpBase *sObj = (idpBase *)sIterator->mObj;
            IDU_LIST_REMOVE(sIterator);

            // freeing memory that store value and
            // freeing descriptor should be separated
            
            for (j = 0; j < sObj->mMemVal.mCount; j++)
            {
                iduMemMgr::freeRaw(sObj->mMemVal.mVal[j]);
            }

            iduMemMgr::freeRaw(sObj);
            iduMemMgr::freeRaw(sIterator);
        }
    }
    mCount = 0;

    for(i = 0; i < IDP_MAX_PROP_COUNT; i++)
    {
        IDE_TEST_RAISE(IDU_LIST_IS_EMPTY(&mArrBaseList[i]) != ID_TRUE,
                       prop_error);
    }

    IDE_TEST_RAISE(idlOS::thread_mutex_destroy(&mMutex) != 0,
                   mutex_error);
    idpBase::destroyStatic();

    return IDE_SUCCESS;

    IDE_EXCEPTION(prop_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp destroy() Error : Removing Property failed.\n");
    }

    IDE_EXCEPTION(mutex_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp destroy() Error : Mutex Destroy Failed.\n");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::regist(����� Descriptr ��ü ������)
 *
 * Description :
 * idp Descriptor�� �ϳ��� ����Ѵ�.
 * ���� �Էµ� ���� ���߿� insert()�� ���� ��ϵǸ�, �� �ܰ迡����
 * default ���� ��ϵȴ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::regist(idpBase *aBase) /* called by each prop*/
{

    idpBase     *sBase;
    iduListNode *sNode;

    IDE_TEST_RAISE(mCount >= IDP_MAX_PROP_COUNT, overflow_error);

    sBase = findBase(aBase->getName());

    IDE_TEST_RAISE(sBase != NULL, already_exist_error);

    sNode = (iduListNode*)iduMemMgr::mallocRaw(ID_SIZEOF(iduListNode));

    IDE_TEST_RAISE(sNode == NULL, memory_alloc_error);

    IDU_LIST_INIT_OBJ(sNode, aBase);
    IDU_LIST_ADD_LAST(&mArrBaseList[mCount], sNode);
    mCount++;

    aBase->registCallback(); // registration callback

    return IDE_SUCCESS;
    IDE_EXCEPTION(overflow_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp regist() Error : "
                        "Overflow of Max Property Count (%"ID_UINT32_FMT").\n",
                        (UInt)IDP_MAX_PROP_COUNT);
    }
    IDE_EXCEPTION(already_exist_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp regist() Error : Property %s is duplicated!!\n",
                        aBase->getName());
    }
    IDE_EXCEPTION(memory_alloc_error)
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp regist() Error : memory allocation error\n");
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
*
* Description :
*  ������Ƽ�� ������ Memory Value�� �������ؼ� Cluster Instance�� �Ͱ�
*  �׷��� ���� Nomal Instance�� �����Ͽ� �ٸ� Verify �Լ��� ȣ���Ѵ�.
*
**********************************************************************/
IDE_RC idp::verifyMemoryValues()
{
    UInt sClusterDatabase = 0;
    UInt sShardEnable = 0;
    UInt sShardAllowAutoCommit = 0;
    UInt sAutoCommit = 0;
    UInt sGlobalTransactionLevel = 0;

    IDE_TEST(read("CLUSTER_DATABASE", &sClusterDatabase ,0) != IDE_SUCCESS);

    if(sClusterDatabase == 0)
    {
        /*���� ���� �˻� �� ���� �� �˻� NORMAL INSTANCE*/
        IDE_TEST(verifyMemoryValue4Normal() != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST_RAISE(sClusterDatabase != 1, err_cluster_database_val_range);

        /*���� ���� �˻� �� ���� �� �˻�,����, ���ϰ�,���ϰ��˻�  CLUSTER INSTANCE*/
        IDE_TEST(verifyMemoryValue4Cluster() != IDE_SUCCESS);
    }

    /* BUG-48247 */
    IDE_TEST( read("SHARD_ENABLE", &sShardEnable, 0) != IDE_SUCCESS );
    IDE_TEST( read("AUTO_COMMIT", &sAutoCommit, 0) != IDE_SUCCESS );
    IDE_TEST( read("__SHARD_ALLOW_AUTO_COMMIT", &sShardAllowAutoCommit, 0) != IDE_SUCCESS );

    if ( sShardEnable == 1 )
    {
        IDE_TEST_RAISE( (sAutoCommit == 1) && (sShardAllowAutoCommit == 0), err_shard_not_applicable );
    }

    /* BUG-48352 */
    if ( sShardEnable == 0 )
    {
        IDE_TEST( read("GLOBAL_TRANSACTION_LEVEL", &sGlobalTransactionLevel, 0) != IDE_SUCCESS );
        
        IDE_TEST_RAISE( sGlobalTransactionLevel == 3, err_Cannot_set_GlobalTransactionLevel ); 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cluster_database_val_range)
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp verifyMemoryValues() Error : "
                        "Property [CLUSTER_DATABASE] "
                        "Overflowed the Value Range.(%d~%d)",
                        0, 1);
    }
    IDE_EXCEPTION( err_shard_not_applicable )
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE,     
                        "idp verifyMemoryValues() Error : "
                        "Cannot set AUTO_COMMIT =1 when SHARD_ENABLE = 1.");
    }
    IDE_EXCEPTION( err_Cannot_set_GlobalTransactionLevel )
    {
        idlOS::snprintf(mErrorBuf, 
                        IDP_ERROR_BUF_SIZE,     
                        "idp verifyMemoryValues() Error : "
                        "Cannot set GLOBAL_TRANSACTION_LEVEL = 3 when SHARD_ENABLE = 0.");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
*
* Description :
*  Normal Instance�� ���� ���ؼ� �ڽ��� ������Ƽ���� ���� �������� �ִ��� �˻��Ѵ�.
*
**********************************************************************/
IDE_RC idp::verifyMemoryValue4Normal()
{
    UInt         i;
    iduListNode* sNode;
    iduList*     sBaseList;
    idpBase*     sBase;

    /*��� ������Ƽ�� ���� ����Ʈ*/
    for (i = 0; i < mCount; i++)
    {
        sBaseList = &mArrBaseList[i];
        /*�ϳ��� ������Ƽ ����Ʈ�� �� �׸� ���ؼ�*/
        IDU_LIST_ITERATE(sBaseList, sNode)
        {
            sBase = (idpBase*)sNode->mObj;
            IDE_TEST(sBase->verifyInsertedValues() != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
*
* Description :
*  cluster Instance�� ���� ���ؼ� ������Ƽ���� ���� �Ӽ�, ����/���ϰ� �Ӽ���
*  �����ϴ��� �˻��ϰ�, ������ ���� ������ �˻��Ѵ�.
**********************************************************************/
IDE_RC idp::verifyMemoryValue4Cluster()
{
    UInt         i;
    iduListNode* sFstNode;
    iduList*     sBaseList;
    idpBase*     sFstBase;
    idpBase*     sBase;
    iduListNode* sNode;

    /*��� ������Ƽ�� ���� ����Ʈ*/
    for(i = 0; i < mCount; i++)
    {
        sBaseList = &mArrBaseList[i];
        sFstNode  = IDU_LIST_GET_FIRST(sBaseList);
        sFstBase  = (idpBase*)sFstNode->mObj;

        /*���� �Ӽ� üũ */
        if(sFstBase->isMustShare() == ID_TRUE)
        {

            IDE_TEST_RAISE((sFstBase->existSPFileValBySID() != ID_TRUE) &&
                           (sFstBase->existSPFileValByAsterisk() != ID_TRUE),
                           err_not_shared);
        }

        /*Identical/Unique �Ӽ� üũ*/
        if((sFstBase->getAttr() & IDP_ATTR_IU_MASK) != IDP_ATTR_IU_ANY)
        {
            /*Identical*/
            if((sFstBase->getAttr() & IDP_ATTR_IU_MASK) ==
                IDP_ATTR_IU_IDENTICAL)
            {
                IDE_TEST(verifyIdenticalAttr(sBaseList) != IDE_SUCCESS);
            }
            else /*Unique*/
            {
                IDE_TEST(verifyUniqueAttr(sBaseList) != IDE_SUCCESS);
            }
        }

        /*�ϳ��� ������Ƽ ����Ʈ�� �� �׸� ���ؼ�*/
        IDU_LIST_ITERATE(sBaseList, sNode)
        {
            sBase = (idpBase*)sNode->mObj;
            IDE_TEST(sBase->verifyInsertedValues() != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_shared);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "verify value error : "
                        "Property [%s] must be shared in SPFILE!\n",
                        sFstBase->getName());
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
*
* Description :
*   ����Ʈ ���� ������Ƽ���� Memory ���� ��� �������� Ȯ���Ѵ�.
*   aBaseList      - [IN]  Ư�� ������Ƽ�� ���� �ټ��� SID�� ���� ������Ƽ���� ����Ʈ
*****************************************************************************/
IDE_RC idp::verifyIdenticalAttr(iduList* aBaseList)
{
    iduListNode* sFstNode;
    idpBase*     sFstBase;
    iduListNode* sCmpNode;
    idpBase*     sCmpBase;
    UInt         i;
    void*        sVal1;
    void*        sVal2;

    IDE_ASSERT(IDU_LIST_IS_EMPTY(aBaseList) == ID_FALSE);

    sFstNode = IDU_LIST_GET_FIRST(aBaseList);
    sFstBase = (idpBase*)sFstNode->mObj;

    IDU_LIST_ITERATE_AFTER2LAST(aBaseList, sFstNode, sCmpNode)
    {
        sCmpBase = (idpBase*)sCmpNode->mObj;

        IDE_TEST_RAISE(sFstBase->getValCnt() != sCmpBase->getValCnt(),
                       err_not_identical);

        for(i = 0; i < sFstBase->getValCnt(); i++)
        {
            IDE_TEST(sFstBase->readPtr4Internal(i, &sVal1) != IDE_SUCCESS);
            IDE_TEST(sCmpBase->readPtr4Internal(i, &sVal2) != IDE_SUCCESS);
            IDE_TEST_RAISE(sFstBase->compare(sVal1, sVal2) != 0,
                           err_not_identical);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_identical);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "check value error : "
                        "Property [%s] must be identical on all instances!!\n",
                        sFstBase->getName());
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
*
* Description :
*   ����Ʈ ���� ������Ƽ���� Memory ���� ��� ���� �ٸ��� Ȯ���Ѵ�.
*   aBaseList      - [IN]  Ư�� ������Ƽ�� ���� �ټ��� SID�� ���� ������Ƽ���� ����Ʈ
*****************************************************************************/
IDE_RC idp::verifyUniqueAttr(iduList* aBaseList)
{
    idpBase*     sCmpBase1;
    idpBase*     sCmpBase2;
    iduListNode* sCmpNode1;
    iduListNode* sCmpNode2;
    void*        sVal1;
    void*        sVal2;
    UInt         i, j;

    IDE_ASSERT(IDU_LIST_IS_EMPTY(aBaseList) == ID_FALSE);

    IDU_LIST_ITERATE(aBaseList, sCmpNode1)
    {
        sCmpBase1 = (idpBase*)sCmpNode1->mObj;

        IDU_LIST_ITERATE_AFTER2LAST(aBaseList, sCmpNode1, sCmpNode2)
        {
            sCmpBase2 = (idpBase*)sCmpNode2->mObj;

            for(i = 0; i < sCmpBase1->getValCnt(); i++)
            {
                IDE_TEST(sCmpBase1->readPtr4Internal(i, &sVal1)
                        != IDE_SUCCESS);

                for(j = 0; j < sCmpBase2->getValCnt(); j++)
                {
                    IDE_TEST(sCmpBase2->readPtr4Internal(j, &sVal2)
                            != IDE_SUCCESS);
                    IDE_TEST_RAISE(sCmpBase1->compare(sVal1, sVal2) == 0,
                                   err_not_unique);
                }
            }
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_unique);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "verify value error : "
                        "Property [%s] must be unique on all instances!\n",
                        sCmpBase1->getName());
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ڽ���(Local Instance) ������Ƽ�� ã�´�.
 * aName -[IN] ã�����ϴ� ������Ƽ �̸�
 *
 *****************************************************************************/
idpBase *idp::findBase(const SChar *aName)
{
    UInt         i;
    iduListNode* sNode;
    idpBase*     sBase;

    for(i = 0; i < mCount; i++)
    {
        if(IDU_LIST_IS_EMPTY(&mArrBaseList[i]) != ID_TRUE)
        {
            sNode = IDU_LIST_GET_FIRST(&mArrBaseList[i]);
            sBase = (idpBase*)sNode->mObj;

            if(idlOS::strcasecmp(sBase->getName(), aName) == 0)
            {
                return (idpBase*)sBase;
            }
        }
    }

    return NULL;
}

/****************************************************************************
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� ������Ƽ ����Ʈ�� ã�Ƽ� ��ȯ�Ѵ�.
 * aName -[IN] ã�����ϴ� ������Ƽ ����Ʈ�� ������Ƽ �̸�
 *
 *****************************************************************************/
iduList *idp::findBaseList(const SChar *aName)
{
    UInt         i;
    iduListNode* sNode;
    idpBase*     sBase;

    for (i = 0; i < mCount; i++)
    {
        if(IDU_LIST_IS_EMPTY(&mArrBaseList[i]) != ID_TRUE)
        {
            sNode = IDU_LIST_GET_FIRST(&mArrBaseList[i]);
            sBase = (idpBase*)sNode->mObj;

            if (idlOS::strcasecmp(sBase->getName(), aName) == 0)
            {
                return &mArrBaseList[i];
            }
        }
    }

    return NULL;
}

/****************************************************************************
 *
 * Description :
 *  aBaseList�� ������Ƽ ����Ʈ���� SID�� �̿��Ͽ� SID�� ���� ������Ƽ�� ã�´�.
 * aBaseList -[IN] ����� �Ǵ� ������Ƽ ����Ʈ
 * aSID      -[IN] ã�����ϴ� ������Ƽ ����Ʈ�� ������Ƽ �̸�
 *
 *****************************************************************************/
idpBase* idp::findBaseBySID(iduList* aBaseList, const SChar* aSID)
{
    idpBase*        sBase;
    iduListNode*    sNode;

    if(aBaseList != NULL)
    {
        IDU_LIST_ITERATE(aBaseList, sNode)
        {
            sBase = (idpBase*)sNode->mObj;

            if (idlOS::strcasecmp(sBase->getSID(), aSID) == 0)
            {
                return sBase;
            }
        }
    }

    return NULL;
}

/****************************************************************************
 *
 * Description :
 *   Local Instance Property(��� ������Ƽ ����Ʈ�� ù ��° �׸�)�� �ڽ��� SID�� �����Ѵ�.
 *
 *****************************************************************************/
IDE_RC idp::setLocalSID()
{
    UInt         i;
    iduListNode* sNode;
    idpBase*     sBase;
    SChar*       sSID = NULL;
    idBool       sIsFound = ID_FALSE;

    readPtrBySrc("SID",
                 0, /*n��° ��*/
                 IDP_VALUE_FROM_ENV,
                 (void**)&sSID,
                 &sIsFound);

    if(sIsFound == ID_FALSE)
    {
        readPtrBySrc("SID",
                     0, /*n��° ��*/
                     IDP_VALUE_FROM_PFILE,
                     (void**)&sSID,
                     &sIsFound);
    }

    if(sIsFound == ID_FALSE)
    {
        readPtrBySrc("SID",
                     0, /*n��° ��*/
                     IDP_VALUE_FROM_DEFAULT,
                     (void**)&sSID,
                     &sIsFound);
    }

    IDE_TEST_RAISE(sIsFound == ID_FALSE, error_not_exist_sid);
    IDE_ASSERT(sSID != NULL);

    mSID = sSID;

    for (i = 0; i < mCount; i++)
    {
        sNode = IDU_LIST_GET_FIRST(&mArrBaseList[i]);
        sBase = (idpBase*)sNode->mObj;
        sBase->setSID(mSID);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_not_exist_sid);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "idp setLocalSID() Error : Local SID is not found!!\n");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  aName�� �ش��ϴ� Local ������Ƽ�� aValue�� ���� ��Ʈ�� ������ ���� aSrc�� ������ ��ġ�� �����Ѵ�.
*
*   const SChar     *aName,    - [IN]  Local ������Ƽ �̸�
*   SChar           *aValue,   - [IN]  ��Ʈ�� ������ ��
*   idpValueSource   aSrc,     - [IN]  ������ Source�� ��ġ
*                                      (default/env/pfile/spfile by asterisk, spfile by sid)
*   idBool          *aFindFlag - [OUT] ������Ƽ�� ���������� �˻� �Ǿ����� ����
*
*******************************************************************************************/
IDE_RC idp::insertBySrc(const SChar     *aName,
                        SChar           *aValue,
                        idpValueSource   aSrc,
                        idBool          *aFindFlag)
{
    idpBase *sBase;

    *aFindFlag = ID_FALSE;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    *aFindFlag = ID_TRUE;

    IDE_TEST(sBase->insertBySrc(aValue, aSrc) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp insert() Error : Property [%s] was not found!!\n",
                        aName);
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*  aName�� �ش��ϴ�������Ƽ ����Ʈ�� ��� Base ��ü�� ��Ʈ�� ������ ���� aSrc�� ������ ��ġ�� �����Ѵ�.
*
*   iduList         *aBaseList, - [IN]  ������Ƽ ����Ʈ
*   SChar           *aValue,    - [IN]  ��Ʈ�� ������ ��
*   idpValueSource   aSrc,      - [IN]  ������ Source�� ��ġ
*                                      (default/env/pfile/spfile by asterisk, spfile by sid)
*
*******************************************************************************************/
IDE_RC idp::insertAll(iduList          *aBaseList,
                      SChar            *aValue,
                      idpValueSource    aSrc)
{
    idpBase     *sBase;
    iduListNode *sNode;

    IDU_LIST_ITERATE(aBaseList, sNode)
    {
        sBase = (idpBase*)sNode->mObj;
        IDE_TEST(sBase->insertBySrc(aValue, aSrc) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*-----------------------------------------------------------------------------
 * Name :
 *     idp::read(�̸�, ��, ��ȣ)
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ش� ������Ƽ�� ���� ��´�.
 * �� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
 * ����ؾ� �Ѵ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::read(const SChar *aName, void *aOutParam, UInt aNum)
{
    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
    IDE_TEST(sBase->read(aOutParam, aNum) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idp::readFromEnv(const SChar *aName, void *aOutParam, UInt aNum)
{
    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
    IDE_TEST(sBase->readBySrc(aOutParam, IDP_VALUE_FROM_ENV, aNum) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* ������Ƽ�� �̸��� SID�� �̿��Ͽ� �ش� ������Ƽ�� �����͸� ��´�.
* �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
* ����ؾ� �Ѵ�.
* ��� ������Ƽ�� �б������̰�, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
*
* const SChar   *aSID       - [IN]  ã�����ϴ� ������Ƽ�� SID
* const SChar   *aName,     - [IN]  ã�����ϴ� ������Ƽ �̸�
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* void          *aOutParam, - [OUT] ��� ��
*******************************************************************************************/
IDE_RC idp::readBySID(const SChar *aSID,
                      const SChar *aName,
                      UInt         aNum,
                      void        *aOutParam)
{
    idpBase *sBase;
    iduList *sBaseList;

    sBaseList = findBaseList(aName);

    IDE_TEST_RAISE(sBaseList == NULL, not_found_error);

    sBase = findBaseBySID(sBaseList, aSID);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST(sBase->read(aOutParam, aNum) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_Property_NotFound, aSID, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* ������Ƽ�� �̸��� SID�� �̿��Ͽ� �ش� ������Ƽ�� �����͸� ��´�.
* �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
* ����ؾ� �Ѵ�.
* ��� ������Ƽ�� �б������̰�, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
* ã���� �ϴ� ���� ������ aIsFound�� ID_FALSE�� setting�ϰ�, ������
* ID_TRUE�� setting�Ѵ�.
*
* const SChar   *aSID       - [IN]  ã�����ϴ� ������Ƽ�� SID
* const SChar   *aName,     - [IN]  ã�����ϴ� ������Ƽ �̸�
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* void         **aOutParam, - [OUT] ��� ���� ������
* idBool        *aIsFound)  - [OUT] �˻� ���� ����
*******************************************************************************************/
IDE_RC idp::readPtrBySID(const SChar *aSID,
                         const SChar *aName,
                         UInt         aNum,
                         void       **aOutParam,
                         idBool      *aIsFound)
{
    idpBase *sBase;
    iduList* sBaseList;

    *aIsFound = ID_FALSE;

    sBaseList = findBaseList(aName);

    sBase = findBaseBySID(sBaseList, aSID);

    if(sBase != NULL)
    {
        IDE_TEST(sBase->readPtr(aOutParam, aNum) != IDE_SUCCESS);

        *aIsFound = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::readPtr(�̸�, ������ ** , ��ȣ, aIsFound)
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ش� ������Ƽ�� �����͸� ��´�.
 * �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
 * ����ؾ� �Ѵ�.
 * ��� ������Ƽ�� �б������̰�, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
 * ã���� �ϴ� ���� ������ aIsFound�� ID_FALSE�� setting�ϰ�, ������
 * ID_TRUE�� setting�Ѵ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::readPtr(const SChar *aName,
                    void       **aOutParam,
                    UInt         aNum,
                    idBool      *aIsFound)
{
    idpBase *sBase;

    *aIsFound = ID_FALSE;

    sBase = findBase(aName);

    if(sBase != NULL)
    {
        *aIsFound = ID_TRUE;

        // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
        //                  client ������ �ȵ�
        //
        // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
        IDE_TEST(sBase->readPtr(aOutParam, aNum) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* ������Ƽ�� �̸��� �̿��Ͽ� �ش��ϴ� Local Instance ������Ƽ�� ã��
* ã�� ������Ƽ���� source�� ��ġ�� ����� ���� �����͸� ��´�.
* �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
* ����ؾ� �Ѵ�.
* ��� ������Ƽ��, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
* ã���� �ϴ� ���� ������ aIsFound�� ID_FALSE�� setting�ϰ�, ������
* ID_TRUE�� setting�Ѵ�.
* const SChar   *aName,     - [IN]  ã�����ϴ� ������Ƽ �̸�
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* idpValueSource aSrc,      - [IN]  � ���� ����� ���ؼ� ������ �������� ��Ÿ���� Value Source
* void         **aOutParam, - [OUT] ��� ���� ������
* idBool        *aIsFound)  - [OUT] �˻� ���� ����
*******************************************************************************************/
void idp::readPtrBySrc(const SChar   *aName,
                       UInt           aNum,
                       idpValueSource aSrc,
                       void         **aOutParam,
                       idBool        *aIsFound)
{
    idpBase *sBase;

    *aIsFound  = ID_FALSE;
    *aOutParam = NULL;

    sBase = findBase(aName);

    if(sBase != NULL)
    {
        if(sBase->readPtrBySrc(aNum, aSrc, aOutParam) == IDE_SUCCESS)
        {
            IDE_ASSERT(*aOutParam != NULL);

            *aIsFound = ID_TRUE;
        }
        else
        {
            *aIsFound = ID_FALSE;
        }
    }

    return;
}

/******************************************************************************************
*
* Description :
* ������Ƽ�� �̸��� �̿��Ͽ� �ش��ϴ� Local Instance ������Ƽ�� ã��
* ã�� ������Ƽ���� source�� ��ġ�� ����� ���� ���� ������ �޸𸮸� �Ҵ��Ͽ� �Ҵ�� ��ġ�� ���� ������ ��,
* ����� �޸��� �����͸� ��´�. �׷��Ƿ�, �� �Լ��� ȣ���� �ʿ��� �޸𸮸� �����ؾ��Ѵ�.
* �� ���� String Ÿ���� ������Ƽ�� ���ؼ��� ?�� $ALTIBASE_HOME���� ������
* ���� ��ȯ�Ѵ�.
* ã���� �ϴ� ���� ������ aIsFound�� ID_FALSE�� setting�ϰ�, ������
* ID_TRUE�� setting�Ѵ�.
* const SChar   *aName,     - [IN]  ã�����ϴ� ������Ƽ �̸�
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* idpValueSource aSrc,      - [IN]  � ���� ����� ���ؼ� ������ �������� ��Ÿ���� Value Source
* void         **aOutParam, - [OUT] �޸� ������ ����� ������Ƽ ���� ������
* idBool        *aIsFound)  - [OUT] �˻� ���� ����
*******************************************************************************************/
void idp::readClonedPtrBySrc(const SChar   *aName,
                             UInt           aNum,
                             idpValueSource aSrc,
                             void         **aOutParam,
                             idBool        *aIsFound)
{
    idpBase *sBase;
    void    *sSrcValue;

    *aIsFound  = ID_FALSE;
    *aOutParam = NULL;

    sBase = findBase(aName);

    if(sBase != NULL)
    {
        if(sBase->readPtrBySrc(aNum, aSrc, &sSrcValue) == IDE_SUCCESS)
        {
            IDE_ASSERT(sSrcValue != NULL);

            sBase->cloneValue(sBase, sSrcValue, aOutParam);

            *aIsFound = ID_TRUE;
        }
        else
        {
            *aIsFound = ID_FALSE;
        }
    }

    return;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::readPtr(�̸�, ������ ** , ��ȣ)
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ش� ������Ƽ�� �����͸� ��´�.
 * �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
 * ����ؾ� �Ѵ�.
 * ��� ������Ƽ�� �б������̰�, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::readPtr(const SChar *aName, void **aOutParam, UInt aNum)
{
    idBool sIsFound;

    IDE_TEST(readPtr(aName, aOutParam, aNum, &sIsFound)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(sIsFound == ID_FALSE, not_found_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* ������Ƽ�� �̸��� SID�� �̿��Ͽ� �ش� ������Ƽ�� �����͸� ��´�.
* �� ���� ���� ������ �����̸�, ȣ���ڰ� ������ ������ Ÿ������ �����ؼ�
* ����ؾ� �Ѵ�.
* ��� ������Ƽ�� �б������̰�, String Ÿ���� ��쿡�� ��ȿ�ϴ�.
*
* const SChar   *aSID       - [IN]  ã�����ϴ� ������Ƽ�� SID
* const SChar   *aName,     - [IN]  ã�����ϴ� ������Ƽ �̸�
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* void         **aOutParam, - [OUT] ��� ���� ������
*******************************************************************************************/
IDE_RC idp::readPtrBySID(const SChar *aSID,
                         const SChar *aName,
                         UInt         aNum,
                         void       **aOutParam)
{
    idBool sIsFound;

    IDE_TEST(readPtrBySID(aSID, aName, aNum, aOutParam, &sIsFound)
             != IDE_SUCCESS);

    IDE_TEST_RAISE(sIsFound == ID_FALSE, not_found_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_Property_NotFound,
                                aSID,
                                aName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::update(�̸�, Native��, ��ȣ)
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ش� ������Ƽ�� ���� �����Ѵ�.
 * �� ���� ������ �����̸�, ����Ű�� ���� �ش� ����Ÿ Ÿ���̾�� �Ѵ�.
 * ��Ʈ�� ������ ���, ��Ȯ�� ������ ������ �� ����.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::update(idvSQL      * aStatistics,
                   const SChar * aName,
                   UInt          aInParam,
                   UInt          aNum,
                   void        * aArg)
{
    SChar sCharVal[16];

    idlOS::snprintf(sCharVal, ID_SIZEOF(sCharVal), "%"ID_UINT32_FMT, aInParam);

    return idp::update(aStatistics, aName, sCharVal, aNum, aArg);
}

IDE_RC idp::update(idvSQL      * aStatistics,
                   const SChar * aName,
                   ULong         aInParam,
                   UInt          aNum,
                   void        * aArg)
{
    SChar sCharVal[32];

    idlOS::snprintf(sCharVal, ID_SIZEOF(sCharVal), "%"ID_UINT64_FMT, aInParam);

    return idp::update(aStatistics, aName, sCharVal, aNum, aArg);
}

// BUG-43533 OPTIMIZER_FEATURE_ENABLE
IDE_RC idp::update4Startup(idvSQL      * aStatistics,
                           const SChar * aName,
                           SChar       * aInParam,
                           UInt          aNum,
                           void        * aArg)
{

    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
    IDE_TEST(sBase->update4Startup(aStatistics, aInParam, aNum, aArg) != IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idp::update(idvSQL      * aStatistics,
                   const SChar * aName,
                   SChar       * aInParam,
                   UInt          aNum,
                   void        * aArg)
{

    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
    IDE_TEST(sBase->update(aStatistics, aInParam, aNum, aArg) != IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::updateForce(�̸�, Native��, ��ȣ)
 *
 * Description :
 * ������Ƽ�� �̸��� �̿��Ͽ� �ش� ������Ƽ�� ���� "������" �����Ѵ�.
 * �� ���� ������ �����̸�, ����Ű�� ���� �ش� ����Ÿ Ÿ���̾�� �Ѵ�.
 * ��Ʈ�� ������ ���, ��Ȯ�� ������ ������ �� ����.
 * READ-ONLY ������Ƽ�� ������ �� Ȱ���ϸ� �����׽�Ʈ �������� ����Ѵ�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::updateForce(const SChar *aName, UInt aInParam,  UInt aNum, void *aArg)
{
    SChar sCharVal[16];

    idlOS::snprintf(sCharVal, ID_SIZEOF(sCharVal), "%"ID_UINT32_FMT, aInParam);

    return idp::updateForce(aName, sCharVal, aNum, aArg);
}

IDE_RC idp::updateForce(const SChar *aName, ULong aInParam,  UInt aNum, void *aArg)
{
    SChar sCharVal[32];

    idlOS::snprintf(sCharVal, ID_SIZEOF(sCharVal), "%"ID_UINT64_FMT, aInParam);

    return idp::updateForce(aName, sCharVal, aNum, aArg);
}


IDE_RC idp::updateForce(const SChar *aName, SChar *aInParam,  UInt aNum, void *aArg)
{
    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    // To Fix BUG-18324 alter system set LOGICAL_AGER_COUNT���� �ϴµ���
    //                  client ������ �ȵ�
    //
    // �� �ȿ��� �ش� Property�� Mutex�� ��´�.
    IDE_TEST(sBase->updateForce(aInParam, aNum, aArg) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC idp::validate( const SChar * aName,
                      SChar       * aInParam,
                      idBool        aIsSystem )
{
    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST(sBase->validate( aInParam, aIsSystem )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// PROJ-2727
IDE_RC idp::getPropertyAttribute( const SChar * aName,
                                  UInt        * aOutParam )
{
    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST(sBase->getPropertyAttribute( aOutParam )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::setupBeforeUpdateCallback(�̸�, ȣ��� �ݹ� �Լ� )
 *
 * Description :
 * ����� ȣ��� �ݹ��� ����Ѵ�.
 * � Name�� ������Ƽ�� ����� ��, �� �Լ��� ȣ��Ǵµ�,
 * �̶� �������� ���İ��� �Բ� ���ƿ´�.
 *
 * �̷� �ݹ� ��Ŀ������ �ʿ��� ������, � ������Ƽ�� ��� �����۾��ÿ�
 * Ư���� ���� ��ġ�� �ʿ��ϱ� �����̴�.
 *
 * ---------------------------------------------------------------------------*/

IDE_RC idp::setupBeforeUpdateCallback(const SChar *aName,
                                      idpChangeCallback mCallback)
{
    idBool sLocked = ID_FALSE;

    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST_RAISE(idlOS::thread_mutex_lock(&mMutex) != 0,
                   lock_error);
    sLocked = ID_TRUE;
    sBase->setupBeforeUpdateCallback(mCallback);
    sLocked = ID_FALSE;


    IDE_TEST_RAISE(idlOS::thread_mutex_unlock(&mMutex) != 0,
                   unlock_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }
    IDE_EXCEPTION(lock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "idp setupBeforeUpdateCallback() Error : Mutex Lock Failed.\n");
    }

    IDE_EXCEPTION(unlock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "idp setupBeforeUpdateCallback() Error : Mutex Unlock Failed.\n");
    }

    IDE_EXCEPTION_END;
    if (sLocked == ID_TRUE)
    {
        IDE_ASSERT(idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     idp::setupAfterUpdateCallback(�̸�, ȣ��� �ݹ� �Լ� )
 *
 * Description :
 * ����� ȣ��� �ݹ��� ����Ѵ�.
 * � Name�� ������Ƽ�� ����� ��, �� �Լ��� ȣ��Ǵµ�,
 * �̶� �������� ���İ��� �Բ� ���ƿ´�.
 *
 * �̷� �ݹ� ��Ŀ������ �ʿ��� ������, � ������Ƽ�� ��� �����۾��ÿ�
 * Ư���� ���� ��ġ�� �ʿ��ϱ� �����̴�.
 *
 * ---------------------------------------------------------------------------*/

IDE_RC idp::setupAfterUpdateCallback(const SChar *aName,
                                     idpChangeCallback mCallback)
{
    idBool sLocked = ID_FALSE;

    idpBase *sBase;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST_RAISE(idlOS::thread_mutex_lock(&mMutex) != 0,
                   lock_error);
    sLocked = ID_TRUE;
    sBase->setupAfterUpdateCallback(mCallback);
    sLocked = ID_FALSE;
    IDE_TEST_RAISE(idlOS::thread_mutex_unlock(&mMutex) != 0,
                   unlock_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }
    IDE_EXCEPTION(lock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "idp setupAfterUpdateCallback() Error : Mutex Lock Failed.\n");
    }

    IDE_EXCEPTION(unlock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE,
                        "idp setupAfterUpdateCallback() Error : Mutex Unlock Failed.\n");
    }

    IDE_EXCEPTION_END;
    if (sLocked == ID_TRUE)
    {
        IDE_ASSERT(idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}


/*-----------------------------------------------------------------------------
 * Name :
 *     idp::readPFile()
 *
 * Description :
 * configuration ȭ���� �Ľ��ϰ�, ������ �Է��Ѵ�.(insert())
 *
 * ---------------------------------------------------------------------------*/

IDE_RC idp::readPFile()
{
    SChar       sConfFile[512];
    SChar       sLineBuf[IDP_MAX_PROP_LINE_SIZE];
    PDL_HANDLE  sFD = PDL_INVALID_HANDLE;
    SChar       *sName;
    SChar       *sValue;
    idBool sFindFlag;
    idBool sOpened = ID_FALSE;

    idlOS::memset(sConfFile, 0, ID_SIZEOF(sConfFile));

#if !defined(VC_WIN32)
    if ( mConfName[0] == IDL_FILE_SEPARATOR )
#else
    if ( mConfName[1] == ':'
      && mConfName[2] == IDL_FILE_SEPARATOR )
#endif /* VC_WIN32 */
    {   // Absolute Path
        idlOS::snprintf(sConfFile, ID_SIZEOF(sConfFile), "%s", mConfName);
    }
    else
    {
        idlOS::snprintf(sConfFile, ID_SIZEOF(sConfFile), "%s%c%s", mHomeDir, IDL_FILE_SEPARATOR, mConfName);
    }

    sFD = idf::open(sConfFile, O_RDONLY);

    IDE_TEST_RAISE(sFD == PDL_INVALID_HANDLE, open_error);
    // fix BUG-25544 [CodeSonar::DoubleClose]
    sOpened = ID_TRUE;

    while(!idf::fdeof(sFD))
    {
        idlOS::memset(sLineBuf, 0, IDP_MAX_PROP_LINE_SIZE);
        if (idf::fdgets(sLineBuf, IDP_MAX_PROP_LINE_SIZE, sFD) == NULL)
        {
            // ȭ���� ������ ����
            break;
        }
        // sLineBuf�� ������ ������ ����

        sName  = NULL;
        sValue = NULL;

        IDE_TEST(parseBuffer(sLineBuf, &sName, &sValue) != IDE_SUCCESS);

        if ((sName != NULL) && (sValue != NULL)) // ������Ƽ ���ο� ������ ����(�̸�+�� ����)
        {
            sFindFlag = ID_FALSE;

            if (insertBySrc(sName, sValue, IDP_VALUE_FROM_PFILE, &sFindFlag) != IDE_SUCCESS)
            {
                IDE_TEST_RAISE(sFindFlag != ID_FALSE, err_insert);
                /* �ܼ��� Name�� ��ã�� ��쿡�� �����ϵ��� ��*/
                ideLog::log(IDE_SERVER_0, "%s\n", idp::getErrorBuf());
            }
        }
    }

    // fix BUG-25544 [CodeSonar::DoubleClose]
    sOpened = ID_FALSE;
    IDE_TEST_RAISE(idf::close(sFD) != 0, close_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(open_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp readConf() Error : Open File [%s] Error.\n", sConfFile);
    }
    IDE_EXCEPTION(close_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp readConf() Error : Close File [%s] Error.\n", sConfFile);
    }
    IDE_EXCEPTION(err_insert);
    {
        /*insertBySrc���� ���� �޼����� �����Ǿ���.*/
    }

    IDE_EXCEPTION_END;

    if (sFD != PDL_INVALID_HANDLE)
    {
        // fix BUG-25544 [CodeSonar::DoubleClose]
        if (sOpened == ID_TRUE)
        {
            (void)idf::close(sFD);
        }
    }


    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*           SPFile�� �Ľ��ϰ�, ������ �Է��Ѵ�.
*******************************************************************************************/
IDE_RC idp::readSPFile()
{
    SChar       *sSPFileName = NULL;
    SChar        sLineBuf[IDP_MAX_PROP_LINE_SIZE];
    FILE        *sFP = NULL;
    SChar       *sSID;
    SChar       *sName;
    SChar       *sValue;
    iduList     *sBaseList;
    idpBase     *sBase;
    idpBase     *sCloneBase;
    iduListNode *sNode;
    idBool       sIsFound = ID_FALSE;
    idBool       sOpened  = ID_FALSE;

    /* ?�� $ALTIBASE_HOME���� ��ȯ�Ǿ ��ȯ�Ǿ�� �ϹǷ�, readClonedPtrBySrc��
     * ���ؼ� ������Ƽ ���� �����;��Ѵ�.*/
    readClonedPtrBySrc("SPFILE",
                       0, /*n��° ��*/
                       IDP_VALUE_FROM_ENV,
                       (void**)&sSPFileName,
                       &sIsFound);

    if(sIsFound != ID_TRUE)
    {
        IDE_ASSERT(sSPFileName == NULL);
        readClonedPtrBySrc("SPFILE",
                           0, /*n��° ��*/
                           IDP_VALUE_FROM_PFILE,
                           (void**)&sSPFileName,
                           &sIsFound);
    }
    else
    {
        /*SPFILE ������Ƽ�� default�� ���� ����. �׷��Ƿ�, default ���� �˻����� �ʴ´�.*/
    }

    /* SPFILE�� ������ ENV/PFILE�� ���� ��� ������Ƽ�� �о���̰� ������ ������ �� �����Ƿ�
     * ���� �м��� �ǳʶٰ� ���� ������ �����Ѵ�.*/
    IDE_TEST_RAISE(sIsFound == ID_FALSE, cont_next_step);

    IDE_TEST_RAISE(sSPFileName == NULL, null_spfile_name);
    sFP = idlOS::fopen(sSPFileName, "r");

    IDE_TEST_RAISE(sFP == NULL, open_error);
    sOpened = ID_TRUE;

    while(!feof(sFP))
    {
        idlOS::memset(sLineBuf, 0, IDP_MAX_PROP_LINE_SIZE);
        if (idlOS::fgets(sLineBuf, IDP_MAX_PROP_LINE_SIZE, sFP) == NULL)
        {
            // ȭ���� ������ ����
            break;
        }

        // sLineBuf�� ������ ������ ����
        sSID   = NULL;
        sName  = NULL;
        sValue = NULL;

        IDE_TEST(parseSPFileLine(sLineBuf, &sSID, &sName, &sValue) != IDE_SUCCESS);

        if((sSID != NULL) && (sName != NULL) && (sValue != NULL))
        {
            sBaseList = findBaseList(sName);

            if(sBaseList == NULL)
            {
                idlOS::snprintf(mErrorBuf,
                                IDP_ERROR_BUF_SIZE,
                                "idp readSPFile() Error : "
                                "Property [%s] Not Registered.\n",
                                sName );
                /* �ܼ��� Name�� ��ã�� ��쿡�� �����ϵ��� ��*/
                ideLog::log(IDE_SERVER_0, "%s\n", idp::getErrorBuf());
                continue;
            }

            /*SID�� *�� ǥ��� ���*/
            if(idlOS::strcmp(sSID, "*") == 0)
            {
                IDE_TEST(insertAll(sBaseList,
                         sValue,
                         IDP_VALUE_FROM_SPFILE_BY_ASTERISK)
                        != IDE_SUCCESS);
            }
            else
            {
                sBase = findBaseBySID(sBaseList, sSID);

                if (sBase != NULL)
                {
                    /*sSID�� ���� ������Ƽ�� sValue���� ����*/
                    IDE_TEST(sBase->insertBySrc(sValue, IDP_VALUE_FROM_SPFILE_BY_SID) != IDE_SUCCESS);
                }
                else
                {
                    /*Local Instance�� ������Ƽ�� ������ Ÿ���� ������Ƽ�� �����Ͽ� ����Ʈ�� ����*/
                    sNode = IDU_LIST_GET_FIRST(sBaseList);
                    sBase = (idpBase*)sNode->mObj;

                    IDE_TEST(sBase->clone(sSID, &sCloneBase) != IDE_SUCCESS);

                    IDE_TEST(sCloneBase->insertBySrc(sValue, IDP_VALUE_FROM_SPFILE_BY_SID) != IDE_SUCCESS);

                    sNode = (iduListNode*)iduMemMgr::mallocRaw(ID_SIZEOF(iduListNode));

                    IDE_TEST_RAISE(sNode == NULL, memory_alloc_error);

                    IDU_LIST_INIT_OBJ(sNode, sCloneBase);
                    IDU_LIST_ADD_LAST(sBaseList, sNode);
                }
            }
        }
    }

    sOpened = ID_FALSE;
    IDE_TEST_RAISE(idlOS::fclose(sFP) != 0, close_error);

    if(sSPFileName != NULL)
    {
        iduMemMgr::freeRaw(sSPFileName);
        sSPFileName = NULL;
    }

    IDE_EXCEPTION_CONT(cont_next_step);

    return IDE_SUCCESS;

    IDE_EXCEPTION(open_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp readSPFile() Error : Open File [%s] Error.\n",
                        sSPFileName);
    }
    IDE_EXCEPTION(close_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp readSPFile() Error : Close File [%s] Error.\n",
                        sSPFileName);
    }
    IDE_EXCEPTION(memory_alloc_error)
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp regist() Error : memory allocation error\n");
    }
    IDE_EXCEPTION(null_spfile_name)
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp readSPFile() Error : NULL file name\n");
    }

    IDE_EXCEPTION_END;

    if (sFP != NULL)
    {
        if (sOpened == ID_TRUE)
        {
            (void)idlOS::fclose(sFP);
        }
    }

    if(sSPFileName != NULL)
    {
        iduMemMgr::freeRaw(sSPFileName);
        sSPFileName = NULL;
    }

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
*           SPFile���� ���� ������ �м��Ͽ� SID, Name, Value�� �и��� �ش�.
*
* SChar  *aLineBuf, -[IN] SPFile���� ���� ���� ����
* SChar **aSID,     -[OUT] SID
* SChar **aName,    -[OUT] Name
* SChar **aValue    -[OUT] Value
*******************************************************************************************/
IDE_RC idp::parseSPFileLine(SChar  *aLineBuf,
                            SChar **aSID,
                            SChar **aName,
                            SChar **aValue)
{
    SChar *sNameWithSID = NULL;

    *aSID   = NULL;
    *aName  = NULL;
    *aValue = NULL;

    IDE_TEST(parseBuffer(aLineBuf, &sNameWithSID, aValue) != IDE_SUCCESS);

    if(sNameWithSID != NULL)
    {
        parseSID(sNameWithSID, aSID, aName);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ��Ʈ������ ù���ڸ� ���� */
void idp::eraseCharacter(SChar *aBuf)
{
    SInt i;

    for (i = 0; aBuf[i]; i++)
    {
        aBuf[i] = aBuf[i + 1];
    }
}

/* ��Ʈ���� �����ϴ� ��� WHITE-SPACE�� ���� */
void idp::eraseWhiteSpace(SChar *aLineBuf)
{
    SInt   i;
    SInt   sLen;
    idBool sIsValue;
    SChar  sQuoteChar;

    sLen       = idlOS::strlen(aLineBuf);
    sIsValue   = ID_FALSE;
    sQuoteChar = 0;

    for (i = 0; i < sLen && aLineBuf[i]; i++)
    {
        if (aLineBuf[i] == '=')
        {
            sIsValue = ID_TRUE;
        }
        if (sIsValue == ID_TRUE)
        {
            if (sQuoteChar != 0)
            {
                if (sQuoteChar == aLineBuf[i])
                {
                    sQuoteChar = 0;
                    eraseCharacter(&aLineBuf[i]);
                    i--;
                }
            }
            else if (aLineBuf[i] == '"' || aLineBuf[i] == '\'')
            {
                sQuoteChar = aLineBuf[i];
                eraseCharacter(&aLineBuf[i]);
                i--;
            }
        }
        if (sQuoteChar == 0)
        {
            if (aLineBuf[i] == '#')
            {
                aLineBuf[i]= 0;
                return;
            }
            if (isspace(aLineBuf[i]) != 0) // �����̽� ��
            {
                eraseCharacter(&aLineBuf[i]);
                i--;
            }
        }
    }
}

IDE_RC idp::parseBuffer(SChar  *aLineBuf,
                        SChar **aName,
                        SChar **aValue)
{
    SInt i;
    SInt sLen;

    // 1. White Space ����
    eraseWhiteSpace(aLineBuf);

    // 2. ������ ���ų� �ּ��̸� ����
    sLen = idlOS::strlen(aLineBuf);

    if (sLen > 0 && aLineBuf[0] != '#')
    {
        *aName = aLineBuf;

        // 3. value �������� �˻�
        for (i = 0; i < sLen; i++)
        {
            if (aLineBuf[i] == '=')
            {
                // �����ڰ� �����ϸ�,
                aLineBuf[i] = 0;

                if (aLineBuf[i + 1] != 0) // Value�� ������.
                {
                    *aValue = &aLineBuf[i + 1];

                    IDE_TEST_RAISE(idlOS::strlen(&aLineBuf[i + 1]) > IDP_MAX_VALUE_LEN,
                                   too_long_value);
                }
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(too_long_value);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp parseBuffer() Error : [%s] Value is too long. "
                        "(not over %"ID_UINT32_FMT")\n", *aName, (UInt)IDP_MAX_VALUE_LEN);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC idp::parseSID(SChar  *aBuf,
                     SChar **aSID,
                     SChar **aName)
{
    SInt i;
    SInt sLen;

    IDE_ASSERT(aBuf != NULL);

    *aSID  = NULL;
    *aName = NULL;

    sLen = idlOS::strlen(aBuf);

    if(sLen > 0)
    {
        *aSID = aBuf;

        for (i = 0; i < sLen; i++)
        {
            if (aBuf[i] == '.')
            {
                // �����ڰ� �����ϸ�,
                aBuf[i] = 0;

                if (aBuf[i + 1] != 0) // Property Name�� ������.
                {
                    *aName = &aBuf[i + 1];

                    IDE_TEST_RAISE(idlOS::strlen(&aBuf[i + 1]) > IDP_MAX_VALUE_LEN,
                                   err_too_long_propertyName);
                }

                break;
            }
        }

        IDE_TEST_RAISE(idlOS::strlen(*aSID) > IDP_MAX_SID_LEN,
                       err_too_long_propertySID);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_too_long_propertyName);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp parseSID() Error : [%s] Property name is too long. "
                        "(not over %"ID_UINT32_FMT")\n",
                        *aName,
                        (UInt)IDP_MAX_VALUE_LEN);
    }

    IDE_EXCEPTION(err_too_long_propertySID);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp parseSID() Error : [%s] Property SID is too long. "
                        "(not over %"ID_UINT32_FMT")\n",
                        *aSID,
                        (UInt)IDP_MAX_VALUE_LEN);
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*-----------------------------------------------------------------------------
 * Name :
 *     getPropertyCount(�̸�)
 *
 * Description :
 * property�� � ���� �Ǿ��°�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::getMemValueCount(const SChar *aName, UInt  *aPropertyCount)
{
    idpBase *sBase;
    idBool sLocked = ID_FALSE;

    sBase = findBase(aName);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST_RAISE(idlOS::thread_mutex_lock(&mMutex) != 0,
                   lock_error);

    sLocked = ID_TRUE;
    *aPropertyCount = sBase->getMemValueCount();
    sLocked = ID_FALSE;

    IDE_TEST_RAISE(idlOS::thread_mutex_unlock(&mMutex) != 0,
                   unlock_error);

    return IDE_SUCCESS;
    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION(lock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp read() Error : Mutex Lock Failed.\n");
    }

    IDE_EXCEPTION(unlock_error);
    {
        idlOS::snprintf(mErrorBuf, IDP_ERROR_BUF_SIZE, "idp read() Error : Mutex Unlock Failed.\n");
    }

    IDE_EXCEPTION_END;
    if (sLocked == ID_TRUE)
    {
        IDE_ASSERT(idlOS::thread_mutex_unlock(&mMutex) == 0);
    }


    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *     getStoredCountBySID(SID,�̸�)
 *
 * Description :
 * sid�� name�� �̿��Ͽ� �˻��� property�� ���� � ���� �Ǿ��°�.
 *
 * ---------------------------------------------------------------------------*/
IDE_RC idp::getStoredCountBySID(const SChar* aSID,
                                const SChar *aName,
                                UInt        *aPropertyCount)
{
    idBool sLocked = ID_FALSE;

    idpBase *sBase;
    iduList *sBaseList;

    sBaseList = findBaseList(aName);

    sBase = findBaseBySID(sBaseList, aSID);

    IDE_TEST_RAISE(sBase == NULL, not_found_error);

    IDE_TEST_RAISE(idlOS::thread_mutex_lock(&mMutex) != 0,
                   lock_error);
    sLocked = ID_TRUE;

    *aPropertyCount = sBase->getMemValueCount();

    sLocked = ID_FALSE;
    IDE_TEST_RAISE(idlOS::thread_mutex_unlock(&mMutex) != 0,
                   unlock_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(not_found_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_idp_NameNotFound, aName));
    }

    IDE_EXCEPTION(lock_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp read() Error : Mutex Lock Failed.\n");
    }

    IDE_EXCEPTION(unlock_error);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp read() Error : Mutex Unlock Failed.\n");
    }

    IDE_EXCEPTION_END;

    if (sLocked == ID_TRUE)
    {
        IDE_ASSERT(idlOS::thread_mutex_unlock(&mMutex) == 0);
    }

    return IDE_FAILURE;
}

/******************************************************************************************
*
* Description :
* aName�� ���� ������Ƽ ����Ʈ�� �˻��Ͽ�, aName�� ������ �ν��Ͻ��� SID�� ���
* ã�Ƽ�, aSIDArray�� �־ �����ָ�, �˻��� SID���� ������ return ���� aCount�� ��ȯ�Ѵ�.
*
* UInt           aNum,      - [IN]  ������Ƽ�� ����� n ��° ���� �ǹ��ϴ� �ε���
* void         **aSIDArray, - [OUT] SID���� ������ �� �ִ� ������ �迭
* UInt          *aCount     - [OUT] �˻��� SID ���� (return ���� ����)
 *******************************************************************************************/
void idp::getAllSIDByName(const SChar *aName, SChar** aSIDArray, UInt* aCount)
{
    UInt            sCount = 0;
    idpBase        *sBase;
    iduListNode    *sNode;
    iduList        *sBaseList;

    sBaseList = findBaseList(aName);

    if(sBaseList != NULL)
    {
        IDU_LIST_ITERATE(sBaseList, sNode)
        {
            sBase = (idpBase*)sNode->mObj;
            aSIDArray[sCount] = sBase->getSID();
            sCount++;
        }
    }

    *aCount = sCount;

    return;
}

/******************************************************************************************
*
* Description :
* Source �켱������ ���� ������Ƽ�� Memory Value�� �����ϰ�, Memory Value�� ������ ������ �Ҵ��� ��,
* ������ ���� �����Ͽ�, ������Ƽ ��ü(idpBase)�� �����Ѵ�.
 *******************************************************************************************/
IDE_RC idp::insertMemoryValueByPriority()
{
    UInt            i, j;
    iduListNode    *sNode;
    iduList        *sBaseList;
    idpBase        *sBase;
    UInt            sCount;
    idpValueSource  sValSrc;
    void           *sVal;

    /*��� ������Ƽ�� ���� ����Ʈ*/
    for (i = 0; i < mCount; i++)
    {
        sBaseList = &mArrBaseList[i];
        /*�ϳ��� ������Ƽ ����Ʈ�� �� �׸� ���ؼ�*/
        IDU_LIST_ITERATE(sBaseList, sNode)
        {
            sBase = (idpBase*)sNode->mObj;

            /* SID�� �̿��Ͽ� SPFILE�� ������ ���� �ִ��� Ȯ��*/
            if(sBase->mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_SID].mCount > 0)
            {
                sCount = sBase->mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_SID].mCount;
                sValSrc = IDP_VALUE_FROM_SPFILE_BY_SID;
            }
            /* "*"�� �̿��Ͽ� SPFILE�� ������ ���� �ִ��� Ȯ��*/
            else if(sBase->mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_ASTERISK].mCount > 0)
            {
                sCount = sBase->mSrcValArr[IDP_VALUE_FROM_SPFILE_BY_ASTERISK].mCount;
                sValSrc = IDP_VALUE_FROM_SPFILE_BY_ASTERISK;
            }
            /* ENV�� �̿��Ͽ� ������ ���� �ִ��� Ȯ��*/
            else if(sBase->mSrcValArr[IDP_VALUE_FROM_ENV].mCount > 0)
            {
                sCount = sBase->mSrcValArr[IDP_VALUE_FROM_ENV].mCount;
                sValSrc = IDP_VALUE_FROM_ENV;
            }
            /* PFILE�� �̿��Ͽ� ������ ���� �ִ��� Ȯ��*/
            else if(sBase->mSrcValArr[IDP_VALUE_FROM_PFILE].mCount > 0)
            {
                sCount = sBase->mSrcValArr[IDP_VALUE_FROM_PFILE].mCount;
                sValSrc = IDP_VALUE_FROM_PFILE;
            }
            /* default ������ ������ �� �ִ°� Ȯ��*/
            else if(sBase->allowDefault() == ID_TRUE)//default �ƹ� ���� �������� �ʾ���
            {
                sCount = sBase->mSrcValArr[IDP_VALUE_FROM_DEFAULT].mCount;
                sValSrc = IDP_VALUE_FROM_DEFAULT;
            }
            else
            {
                /* property�� � ���� �������� �ʾҰ�, Default�� ����� ���� ������ ����
                 * �ٸ� ����� ������Ƽ�̸� SPFILE�� ����� ���� �����Ƿ�
                 * �� �κ����� ���� �� ����.*/
                IDE_RAISE(default_value_not_allowed);
            }

            for(j = 0; j < sCount; j++) //��� ���� values�� ����
            {
                sVal = sBase->mSrcValArr[sValSrc].mVal[j];
                IDE_TEST(sBase->insertMemoryRawValue(sVal) != IDE_SUCCESS);
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(default_value_not_allowed);
    {
        idlOS::snprintf(mErrorBuf,
                        IDP_ERROR_BUF_SIZE,
                        "idp insertMemoryValueByPriority() Error : "
                        "Property [%s] should be specified by configuration.",
                        sBase->getName());
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
