/***********************************************************************
 * Copyright 1999-2013 ALTIBASE Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduFitManager.cpp 87881 2020-06-29 08:04:01Z kclee $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <idp.h>
#include <iduLatch.h>
#include <idErrorCode.h>
#include <iduFitManager.h>

#if defined(ALTIBASE_FIT_CHECK)

idBool iduFitManager::mIsInitialized = ID_FALSE;
idBool iduFitManager::mIsPluginLoaded = ID_FALSE;
iduFitStatus iduFitManager::mIsFitEnabled = IDU_FIT_NONE;
UInt iduFitManager::mInitFailureCount = 0;

/* ServerID �� sleepNode ���� ����Ʈ */
SChar iduFitManager::mSID[IDU_FIT_SID_MAX_LEN + 1];
iduSleepList iduFitManager::mSleepList;

void *iduFitManager::mShmPtr = NULL;
volatile SInt *iduFitManager::mShmBuffer = NULL;

/* FIT ������ ��ſ� �Լ� �� ���� ���̺귯�� �ڵ� */
fitTransmission iduFitManager::mTransmission = NULL;
PDL_SHLIB_HANDLE iduFitManager::mFitHandle = PDL_SHLIB_INVALID_HANDLE;

/* iduFitManager �ʱ�ȭ �Լ� */
IDE_RC iduFitManager::initialize()
{
    /* FIT_ENABLE = 1 �̸� loading �ϰ�, 0 �̸� ���� �ʴ´�. */
    IDE_TEST( iduFitManager::getFitEnable() == IDU_FIT_FALSE );

    /* iduFitManager �ʱ�ȭ ���¸� Ȯ�� */
    if ( mIsInitialized == ID_TRUE )
    {
        IDE_TEST( iduFitManager::finalize() != IDE_SUCCESS );
    }
    else
    {
        /* DO NOTHING */
    }

    /* ���� �ĺ��� �Ľ��ϱ� */
    IDE_TEST_RAISE( iduFitManager::parseSID() != IDE_SUCCESS, ERR_FIT_INITIALIZATION );

    /* �޸� �Ҵ� */
    IDE_TEST_RAISE( initSleepList(&iduFitManager::mSleepList) != IDE_SUCCESS, ERR_FIT_INITIALIZATION );

    /* ���� ���̺귯�� fitPlugin �ε� */
    IDE_TEST_RAISE( loadFitPlugin() != IDE_SUCCESS, ERR_FIT_INITIALIZATION );

    /* Shared Memory �� ������ ���� �����̹Ƿ� Failure �� �߻��ϴ��� �����Ѵ� */
    (void)attachSharedMemory();

    /* fitPluging �ε� ���� ���� ���� */
    mIsPluginLoaded = ID_TRUE;

    /* iduFitManager ���� ���� ���� */
    mIsInitialized = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FIT_INITIALIZATION )
    {
        /* ���� �ϴ��� log �� ����� ��� �����Ѵ�. */
        IDU_FIT_LOG( "FIT Initialization Failure." );

        mInitFailureCount++;

        /* Failure �� IDU_FIT_INIT_RETRY_MAX �̻����� �ݺ��� ���FIT_ENABLE �� 0 ���� �����Ѵ�. */
        if ( mInitFailureCount > IDU_FIT_INIT_RETRY_MAX ) 
        {
            mIsFitEnabled = IDU_FIT_FALSE;
            IDU_FIT_LOG( "Set value of FIT_ENABLE to 0" );
        }
        else 
        {
            /* Do nothing */
        }

    }

    IDE_EXCEPTION_END;
    
    return IDE_SUCCESS; 
}

/* iduFitManager �Ҹ� �Լ� */
IDE_RC iduFitManager::finalize()
{
    UInt sState = 1;

    /* fitPluging �ε��� ���� */
    IDE_TEST_RAISE( iduFitManager::unloadFitPlugin() != IDE_SUCCESS, ERR_FIT_FINALIZATION );

    /* sleepList�� �����ϱ� */
    IDE_TEST_RAISE( finalSleepList(&iduFitManager::mSleepList) != IDE_SUCCESS, ERR_FIT_FINALIZATION );

    if ( ( mShmPtr != NULL ) && ( mShmPtr != (void *)-1 ) )
    {
        (void)idlOS::shmdt( mShmPtr );
        mShmBuffer = NULL;
        mShmPtr = NULL;
    }
    else
    {
        /* Do Nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FIT_FINALIZATION )
    {
        /* ���� �ϴ��� log �� ����� ��� �����Ѵ�. */
        IDU_FIT_LOG( "FIT Finalization Failure." );
    }

    IDE_EXCEPTION_END;
    
    switch ( sState )
    {
        case 1:
            (void)finalSleepList(&iduFitManager::mSleepList);
            /* fall through */
        default:
        {
            break;
        }
    }

    return IDE_SUCCESS;
}

/* ErrorCode �����Լ� */
void iduFitManager::setErrorCode( const SChar *aFormat, ... )
{
    UInt sErrorCode = 0;
    va_list sArgsList;
    
    va_start(sArgsList, aFormat);

    sErrorCode = va_arg(sArgsList, UInt);

    IDE_SET( ideFitSetErrorCode(sErrorCode, sArgsList) );

    va_end(sArgsList);
}

void iduFitManager::setLogInfo( const SChar *aFileName, 
                                UInt aCodeLine,
                                iduFitAction *aFitAction )
{
    /* ���� �̸��� ���� */
    idlOS::strncpy( aFitAction->mFileName, 
                    aFileName, 
                    IDU_FIT_NAME_MAX_LEN );

    /* ������ ���� */
    aFitAction->mCodeLine = aCodeLine;
}

/* $ALTIBASE_HOME ��η� SID �����͸� �Ľ� */
IDE_RC iduFitManager::parseSID()
{
    SChar *sEnvPath = NULL;
    SChar sHomePath[IDU_FIT_PATH_MAX_LEN + 1] = { 0, };

    SChar *sStrPtr = NULL;
    SChar *sSidPtr = NULL;

    /* �ĺ��� SID �����ϱ� */
    sEnvPath = idlOS::getenv(IDP_HOME_ENV);
    IDE_TEST( sEnvPath == NULL );

    idlOS::strncpy(sHomePath, sEnvPath, IDU_FIT_PATH_MAX_LEN);
    sHomePath[IDU_FIT_PATH_MAX_LEN] = IDE_FIT_STRING_TERM;

    /* HDBHomePath�� �ڸ���. ���� ���ڸ� 
       /home/user/work/altidev4/altibase_home 
       ���ڿ��� �ڸ��� altibase_home  */
    sStrPtr = idlOS::strtok(sHomePath, IDL_FILE_SEPARATORS);
    
    /* �� �̻� ���� ���ڿ��� ���� ������ �ݺ� */
    while ( sStrPtr != NULL )
    {
        /* ������ �ڸ��� ���ؼ� ��ġ ���� */
        sSidPtr = sStrPtr;
        sStrPtr = idlOS::strtok(NULL, IDL_FILE_SEPARATORS);
    }

    /* SID ���� */
    idlOS::strncpy(iduFitManager::mSID, sSidPtr, IDU_FIT_SID_MAX_LEN);
    iduFitManager::mSID[IDU_FIT_SID_MAX_LEN] = IDE_FIT_STRING_TERM;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::requestFitAction( const SChar *aUID, iduFitAction *aFitAction )
{

    SChar sSendData[IDU_FIT_DATA_MAX_LEN + 1] = { 0, };
    SChar sRecvData[IDU_FIT_DATA_MAX_LEN + 1] = { 0, };

    /* FIT NODE �� �ƹ��͵� ������ ������� �ʴ´� */
    if ( ( mShmBuffer != NULL ) && ( mShmBuffer != (void *)-1 ) ) 
    {
        IDE_TEST( *mShmBuffer != IDU_FIT_POINT_EXIST );
    }
    else
    {
        /* Do Nothing */
    }

    IDE_TEST( aUID == NULL );
    
    if ( mIsInitialized == ID_FALSE )
    {
        IDE_TEST( iduFitManager::initialize() != IDE_SUCCESS );
    }
    else
    {
        /* DO NOTHING */
    }

    IDE_TEST( mFitHandle == PDL_SHLIB_INVALID_HANDLE );

    /* UID �� SID Append ó�� */
    idlOS::strncpy(sSendData, aUID, ID_SIZEOF(sSendData));
    sSendData[IDU_FIT_DATA_MAX_LEN] = IDE_FIT_STRING_TERM;

    idlOS::strncat( sSendData, IDU_FIT_VERTICAL_BAR, IDU_FIT_DATA_MAX_LEN );
    idlOS::strncat( sSendData, iduFitManager::mSID, IDU_FIT_DATA_MAX_LEN );

    /* FIT ������ �ۼ��� �ϱ� */
    IDE_TEST( mTransmission( (SChar *)sSendData, IDU_FIT_DATA_MAX_LEN,
                             (SChar *)sRecvData, IDU_FIT_DATA_MAX_LEN ) != 0 );

    /* ���� �޴� ����� ���� �Ľ� */
    IDE_TEST( iduFitManager::parseFitAction(sRecvData, aFitAction) != IDE_SUCCESS );

    /* �ƹ��� Action �� �ʿ����� ������� */
    IDE_TEST( aFitAction->mProtocol != IDU_FIT_DO_ACTION );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* ���� ������ �м��ϰ� FitAction�� �ʱ�ȭ */
IDE_RC iduFitManager::parseFitAction( SChar *aRecvData, iduFitAction *aFitAction )
{
    SInt sValue = 0;
    SInt sStrLen = 0;

    SLong sTimeout = 0;
    SLong sProtocol = 0;
    SLong sActionType = 0;

    SChar *sStrPtr = NULL;
    SChar *sLeftPtr = NULL;
    SChar *sRightPtr = NULL;
    SChar *sSavePtr = NULL;

    IDE_TEST( aRecvData == NULL );

    /* aRecvData ���ڿ����� '{' �����ϴ� ��ġ�� ���� */
    sLeftPtr = idlOS::strstr(aRecvData, IDE_FIT_LEFT_BRACE);
    IDE_TEST( sLeftPtr == NULL );
    
    /* aRecvData ���ڿ��� '{' �ִ��� Ȯ�� */
    IDE_TEST( idlOS::strncmp(sLeftPtr, IDE_FIT_LEFT_BRACE, 1) != 0 );
    
    /* aRecvData ���ڿ����� '}' �����ϴ� ��ġ�� ���� */
    sRightPtr = idlOS::strstr(aRecvData, IDE_FIT_RIGHT_BRACE);
    IDE_TEST( sRightPtr == NULL);
    
    /* aRecvData ���ڿ��� '}' �ִ��� Ȯ�� */
    IDE_TEST( idlOS::strncmp(sRightPtr, IDE_FIT_RIGHT_BRACE, 1) != 0 );
    
    /* ���ڿ��� �Ľ� */
    sStrPtr = idlOS::strtok_r(aRecvData, IDE_FIT_LEFT_BRACE, &sSavePtr);
    sStrPtr = idlOS::strtok_r(sStrPtr, IDE_FIT_RIGHT_BRACE, &sSavePtr);

    while ( 1 )
    {
        sStrPtr = idlOS::strtok_r(sStrPtr, IDU_FIT_VERTICAL_BAR, &sSavePtr);
        if ( NULL == sStrPtr )
        {
            break;
        }
        else
        {
            /* DO NOTHING */
        }

        switch ( sValue )
        {
            case 0:
            {
                /* Protocol �ڵ� �Ľ� */
                sStrLen = idlOS::strlen(sStrPtr);
                IDE_TEST(idlVA::strnToSLong(sStrPtr, sStrLen, &sProtocol, NULL) != 0);

                /* Protocol ���� */
                aFitAction->mProtocol = (iduFitProtocol)sProtocol;

                break;
            }
            case 1:
            {
                /* ActionType �Ľ� */
                sStrLen = idlOS::strlen(sStrPtr);
                IDE_TEST(idlVA::strnToSLong(sStrPtr, sStrLen, &sActionType, NULL) != 0);

                /* ActionType ���� */
                aFitAction->mType = (iduFitActionType)sActionType;

                break;
            }
            case 2:
            {
                /* TID �Ľ� */
                idlOS::strncpy(aFitAction->mTID, sStrPtr, ID_SIZEOF(aFitAction->mTID));
                aFitAction->mTID[IDU_FIT_TID_MAX_LEN] = IDE_FIT_STRING_TERM;

                break;
            }
            case 3:
            {
                /* Timeout �ð� �Ľ� */
                sStrLen = idlOS::strlen(sStrPtr);
                IDE_TEST(idlVA::strnToSLong(sStrPtr, sStrLen, &sTimeout, NULL) != 0);

                /* Timeout ���� */
                aFitAction->mTimeout = (UInt)sTimeout;

                break;
            }
            default:
            {
                break;
            }
        }

        sValue++;
        sStrPtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDU_FIT_DEBUG_LOG( "FIT ParseFitAction Failure." );

    return IDE_FAILURE;
}

/* �־��� fitAction�� ������ ���� ��� �����ϴ� �Լ� */
IDE_RC iduFitManager::validateFitAction( iduFitAction *aFitAction )
{

    switch ( aFitAction->mType )
    {
        case IDU_FIT_ACTION_HIT:
        {
            IDE_TEST( hitServer(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_JUMP:
        {
            IDE_TEST( abortServer(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_KILL:
        {
            IDE_TEST( killServer(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_SLEEP:
        {
            IDE_TEST( sleepServer(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_TIME_SLEEP:
        {
            IDE_TEST( timeSleepServer(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_WAKEUP:
        {
            IDE_TEST( wakeupServerBySignal(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_WAKEUP_ALL:
        {
            IDE_TEST( wakeupServerByBroadcast(aFitAction) != IDE_SUCCESS );
            break;
        }
        case IDU_FIT_ACTION_SIGSEGV:
        {
            IDE_TEST( sigsegvServer( aFitAction ) != IDE_SUCCESS );
            break;
        }
        default:
        {
           IDE_TEST( 0 );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDU_FIT_DEBUG_LOG( "FIT validateFitAction Failure." );

    return IDE_FAILURE;
}

/* ��ſ� ���̺귯���� �ε��ϴ� �Լ� */
IDE_RC iduFitManager::loadFitPlugin()
{
    SChar *sEnvPath = NULL;
    SChar sPluginPath[IDU_FIT_PATH_MAX_LEN + 1] = { 0, };

    /* $ATAF_HOME ��θ� ������ */
    sEnvPath = idlOS::getenv(IDU_FIT_ATAF_HOME_ENV);
    IDE_TEST( sEnvPath == NULL );

#if defined(VC_WIN32)
    /* coming soon */
    return IDE_FAILURE;

#else
    idlOS::snprintf( sPluginPath,
                     IDU_FIT_PATH_MAX_LEN,
                     "%s%s%s%s%s%s",
                     sEnvPath,
                     IDL_FILE_SEPARATORS,
                     "lib",
                     IDL_FILE_SEPARATORS,
                     IDU_FIT_PLUGIN_NAME,
                     ".so" );

#endif

    /* ���̺귯���� ���� */
    mFitHandle = idlOS::dlopen(sPluginPath, RTLD_LAZY | RTLD_LOCAL);
    IDE_TEST_RAISE( mFitHandle == PDL_SHLIB_INVALID_HANDLE, ERR_INVALID_HANDLE );

    IDE_TEST( loadFitFunctions() != IDE_SUCCESS );

    mIsPluginLoaded = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_HANDLE )
    {
         IDU_FIT_LOG( "FIT Plugin Load Failure." );

    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC iduFitManager::unloadFitPlugin()
{
    /* If the FitPlugin Handle avaliable */
    if ( iduFitManager::mFitHandle != PDL_SHLIB_INVALID_HANDLE )
    {
        IDE_TEST( idlOS::dlclose(iduFitManager::mFitHandle) != IDE_SUCCESS );

        /* Unload Success */
        iduFitManager::mFitHandle = PDL_SHLIB_INVALID_HANDLE;
    }
    else
    {
        /* DO NOTHING */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        iduFitManager::mFitHandle = PDL_SHLIB_INVALID_HANDLE;
    }

    return IDE_FAILURE;
}

IDE_RC iduFitManager::loadFitFunctions()
{

    mTransmission = (fitTransmission)idlOS::dlsym(mFitHandle,IDU_FIT_PLUGIN_TRANSMISSION);
    
    IDE_TEST( mTransmission == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC iduFitManager::initSleepList( iduSleepList *aSleepList )
{
    /* Setting double-linked list */
    aSleepList->mHead.mPrev = NULL;
    aSleepList->mTail.mNext = NULL;

    aSleepList->mHead.mNext = &(aSleepList->mTail);
    aSleepList->mTail.mPrev = &(aSleepList->mHead);

    /* Setting preporties */
    aSleepList->mCount = 0;

    /* Mutex �ʱ�ȭ */
    IDE_TEST_RAISE( aSleepList->mMutex.initialize((SChar *) "FIT_SLEEPLIST_MUTEX",
                                                  IDU_MUTEX_KIND_POSIX,
                                                  IDV_WAIT_INDEX_NULL)
                    != IDE_SUCCESS, ERR_MUTEX_INIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_INIT )
    {
        /* Mutex �ʱ�ȭ ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::finalSleepList( iduSleepList *aSleepList )
{
    UInt sState = 0;

    /* Clear the sleep list */
    IDE_TEST( resetSleepList(aSleepList) != IDE_SUCCESS );
    sState = 1;

    /* sleepList �� Mutex ���� */
    IDE_TEST_RAISE( aSleepList->mMutex.destroy() != IDE_SUCCESS, ERR_MUTEX_DESTROY );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_DESTROY )
    {
        /* Mutex ���� ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrMutexDestroy));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 1:
            (void)aSleepList->mMutex.destroy();
            /* fall through */
        default:
        {
            break;
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduFitManager::resetSleepList( iduSleepList *aSleepList )
{
    iduSleepNode *sDestNode = NULL;
    iduSleepNode *sNextNode = NULL;
    iduSleepNode *sTailNode = NULL;

    sTailNode = &(aSleepList->mTail);

    for ( sDestNode = aSleepList->mHead.mNext;
          sDestNode != sTailNode;
          sDestNode = sNextNode )
    {
        sNextNode = sDestNode->mNext;
        
        IDE_TEST( deleteSleepNode(aSleepList, sDestNode) != IDE_SUCCESS );

        (void)freeSleepNode(sDestNode);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC iduFitManager::createSleepNode( iduSleepNode **aSleepNode,
                                       iduFitAction *aFitAction )
{
    UInt sState = 0;
    SInt sTIDLength = 0;

    iduSleepNode *sSleepNode = NULL;

    IDE_TEST( aSleepNode == NULL );

    /* �޸� �Ҵ� */
    sSleepNode = (iduSleepNode *) iduMemMgr::mallocRaw(ID_SIZEOF(iduSleepNode));
    IDE_TEST( sSleepNode == NULL );
    sState = 1;
    
    /* TID �����ϱ� */
    idlOS::strncpy( sSleepNode->mAction.mTID, 
                    aFitAction->mTID, 
                    IDU_FIT_TID_MAX_LEN );

    sTIDLength = idlOS::strlen(aFitAction->mTID);
    sSleepNode->mAction.mTID[sTIDLength] = IDE_FIT_STRING_TERM;

    /* ActionType �����ϱ� */
    sSleepNode->mAction.mType = aFitAction->mType;

    /* Time �����ϱ� */
    sSleepNode->mAction.mTimeout = aFitAction->mTimeout;

    /* Mutex �ʱ�ȭ */
    IDE_TEST_RAISE( sSleepNode->mMutex.initialize((SChar *)"FIT_SLEEPNODE_MUTEX",
                                                  IDU_MUTEX_KIND_POSIX,
                                                  IDV_WAIT_INDEX_NULL)
                    != IDE_SUCCESS, ERR_MUTEX_INIT );
    sState = 2;

    /* Condition �ʱ�ȭ */
    IDE_TEST_RAISE( sSleepNode->mCond.initialize((SChar *)"FIT_SLEEPNODE_COND")
                    != IDE_SUCCESS, ERR_COND_INIT );

    *aSleepNode = sSleepNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_INIT )
    {
        /* Mutex �ʱ�ȭ ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION( ERR_COND_INIT )
    {
        /* Condition �ʱ�ȭ ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            (void)sSleepNode->mMutex.destroy();
            /* fall through */
        case 1:
            (void)iduMemMgr::freeRaw(sSleepNode);
            /* fall through */
        default:
        {
            break;
        }
    }
    
    return IDE_FAILURE;
}

IDE_RC iduFitManager::addSleepNode( iduSleepList *aSleepList,
                                    iduSleepNode *aSleepNode )
{
    IDE_TEST( (aSleepList == NULL) || (aSleepNode == NULL) );

    aSleepNode->mNext = NULL;
    aSleepNode->mPrev = NULL;

    IDE_TEST( aSleepList->mMutex.lock(NULL) != IDE_SUCCESS );
    
    /* Append node at list head */
    aSleepNode->mPrev = &(aSleepList->mHead);
    aSleepNode->mNext = aSleepList->mHead.mNext;

    aSleepList->mHead.mNext->mPrev = aSleepNode;
    aSleepList->mHead.mNext = aSleepNode;

    /* Increase list count */
    aSleepList->mCount++;

    IDE_TEST( aSleepList->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::deleteSleepNode( iduSleepList *aSleepList,
                                       iduSleepNode *aSleepNode )
{
    IDE_TEST( (aSleepList == NULL) || (aSleepNode == NULL) );

    IDE_TEST( aSleepList->mMutex.lock(NULL) != IDE_SUCCESS );

    IDE_TEST( (aSleepNode == &(aSleepList->mHead)) ||
              (aSleepNode == &(aSleepList->mTail)) );

    /* Remove from list */
    aSleepNode->mPrev->mNext = aSleepNode->mNext;
    aSleepNode->mNext->mPrev = aSleepNode->mPrev;

    aSleepNode->mNext = NULL;
    aSleepNode->mPrev = NULL;

    /* Decrease list count */
    aSleepList->mCount--;

    IDE_TEST( aSleepList->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::freeSleepNode( iduSleepNode *aSleepNode )
{
    IDE_TEST( aSleepNode == NULL );

    IDE_TEST_RAISE( aSleepNode->mCond.destroy() != IDE_SUCCESS,
                    ERR_COND_DESTROY );

    IDE_TEST_RAISE( aSleepNode->mMutex.destroy() != IDE_SUCCESS,
                    ERR_MUTEX_DESTROY );

    /* �޸� ���� */
    (void)iduMemMgr::freeRaw(aSleepNode);

    /* �����͸� NULL�� ���� */
    aSleepNode = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_DESTROY )
    {
        /* Condition ���� ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION( ERR_MUTEX_DESTROY )
    {
        /* Mutex ���� ���з� ���� ���� */
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrMutexDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::getSleepNode( const SChar *aUID, 
                                    iduSleepNode **aSleepNode )
{
    iduSleepNode *sDestNode = NULL;
    iduSleepNode *sTailNode = NULL;

    *aSleepNode = NULL;

    IDE_TEST( (aUID == NULL) || (aSleepNode == NULL) );

    sTailNode = &(iduFitManager::mSleepList.mTail);

    for ( sDestNode = iduFitManager::mSleepList.mHead.mNext;
          sDestNode != sTailNode;
          sDestNode = sDestNode->mNext )
    {
        /* UID �� TID �̸��� ������ Ȯ�� */
        if ( 0 == idlOS::strncmp(aUID, sDestNode->mAction.mTID, IDU_FIT_TID_MAX_LEN) )
        {
            *aSleepNode = sDestNode;
            break;
        }
        else
        {
            /* Continue */
        }
    }

    /* ������ ���� ó�� */
    IDE_TEST( *aSleepNode == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::hitServer( iduFitAction *aFitAction )
{
    /* BUG-39414 */
    ideLogEntry sLog(IDE_FIT_0);

    sLog.append( "HDB server was hitted.\n" );
    sLog.appendFormat( "[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                        aFitAction->mFileName,
                        aFitAction->mCodeLine,
                        aFitAction->mTID );
    sLog.write();

    return IDE_SUCCESS;
}

IDE_RC iduFitManager::abortServer( iduFitAction *aFitAction )
{
    /* BUG-39414 */
    ideLogEntry sLog(IDE_FIT_0);

    sLog.setTailless(ACP_TRUE);
    sLog.append( "HDB server was aborted.\n" );
    sLog.appendFormat( "[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                        aFitAction->mFileName,
                        aFitAction->mCodeLine,
                        aFitAction->mTID );
    sLog.append( "IDU_FIT_POINT_JUMP\n" );
    sLog.write();

    return IDE_SUCCESS;
}

IDE_RC iduFitManager::killServer( iduFitAction *aFitAction )
{
    /* BUG-39414 */
    ideLogEntry sLog(IDE_FIT_0);

    /* �α� ���� ���� �̵� */
    sLog.setTailless(ACP_TRUE);
    sLog.append( "HDB server was terminated.\n" );
    sLog.appendFormat( "[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                        aFitAction->mFileName,
                        aFitAction->mCodeLine,
                        aFitAction->mTID );
    sLog.append( "IDU_FIT_POINT_KILL\n" );
    sLog.write();

    /* Flush all logs */
    ideLog::flushAllModuleLogs();

    /* Need not close file descriptor */
    idlOS::exit(-1);

    return IDE_SUCCESS;
}

IDE_RC iduFitManager::sleepServer( iduFitAction *aFitAction )
{
    IDE_RC sRC = IDE_FAILURE;

    UInt sState = 0;
    iduSleepNode *sSleepNode = NULL;

    /* �޸� �Ҵ� */
    sRC = createSleepNode(&sSleepNode, aFitAction);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 1;

    /* ���� ��� ��带 �߰� */
    sRC = addSleepNode(&iduFitManager::mSleepList, sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 2;

    /* SLEEP �����ϱ� ���� ��� �� ��� */
    IDE_TEST( sSleepNode->mMutex.lock(NULL) != IDE_SUCCESS );

    /* SLEEP �α׸� ����ϱ� */
    ideLog::log ( IDE_FIT_0, 
                  "HDB server is sleeping.\n[%s:%"ID_UINT32_FMT":\"%s\":INFINATE msec]",
                  aFitAction->mFileName,
                  aFitAction->mCodeLine,
                  aFitAction->mTID );

    /* �ñ׳��� �߻��Ҷ����� ���� */
    IDE_TEST_RAISE( sSleepNode->mCond.wait(&(sSleepNode->mMutex))
                    != IDE_SUCCESS, ERR_COND_WAIT );

    /* WAKEUP �α׸� ����ϱ� */
    ideLog::log ( IDE_FIT_0, 
                  "HDB server received a wakeup signal.\n[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                  aFitAction->mFileName,
                  aFitAction->mCodeLine,
                  aFitAction->mTID );


    /* SLEEP �����ϱ� ���� ���� Ǯ�� */
    IDE_TEST( sSleepNode->mMutex.unlock() != IDE_SUCCESS );

    /* ���� ��� ��带 ���� */
    sRC = deleteSleepNode(&iduFitManager::mSleepList, sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 1;

    /* �޸� ���� */
    sRC = freeSleepNode(sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_WAIT )
    {
        /* Condition Wait ���з� ���� ���� FATAL */
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            (void)deleteSleepNode(&iduFitManager::mSleepList, sSleepNode);
            /* fall through */
        case 1:
            (void)freeSleepNode(sSleepNode);
            /* fall through */
        default:
        {
            break;
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduFitManager::timeSleepServer( iduFitAction *aFitAction )
{
    IDE_RC sRC = IDE_FAILURE;

    UInt sState = 0;
    UInt sUSecond = 0;
    iduSleepNode *sSleepNode = NULL;

    IDE_TEST( aFitAction->mTimeout == 0 );

    /* �޸� �Ҵ� */
    sRC = createSleepNode(&sSleepNode, aFitAction);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 1;

    /* ���� ��� ��带 �߰� */
    sRC = addSleepNode(&iduFitManager::mSleepList, sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 2;

    /* TIME SLEEP �����ϱ� ���� ���� ��� */
    IDE_TEST( sSleepNode->mMutex.lock(NULL) != IDE_SUCCESS );

    /* TIME SLEEP �α׸� ����ϱ� */
    ideLog::log ( IDE_FIT_0,
                  "HDB server is sleeping.\n[%s:%"ID_UINT32_FMT":\"%s\":%"ID_UINT32_FMT" msec]",
                  aFitAction->mFileName,
                  aFitAction->mCodeLine,
                  aFitAction->mTID,
                  aFitAction->mTimeout );

    /* TIME �����ϱ� */
    sUSecond = aFitAction->mTimeout;
    sSleepNode->mTV.set((idlOS::time(NULL) + (sUSecond / 1000)), 0);

    /* TIMEOUT �߻��ϰų� �ñ׳��� �߻��Ҷ����� ���� */
    IDE_TEST_RAISE( sSleepNode->mCond.timedwait(&(sSleepNode->mMutex),
                                                &(sSleepNode->mTV),
                                                ID_TRUE)
                    != IDE_SUCCESS, ERR_COND_TIME_WAIT );

    /* WAKEUP �α׸� ����ϱ� */
    ideLog::log ( IDE_FIT_0,
                  "HDB server received a wakeup signal.\n[%s:%"ID_UINT32_FMT":\"%s\"]",
                  aFitAction->mFileName,
                  aFitAction->mCodeLine,
                  aFitAction->mTID );

    /* TIME SLEEP �����ϱ� ���� ���� Ǯ�� */
    IDE_TEST( sSleepNode->mMutex.unlock() != IDE_SUCCESS );

    /* ���� ��� ��带 ���� */
    sRC = deleteSleepNode(&iduFitManager::mSleepList, sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );
    sState = 1;
    
    /* �޸� ���� */
    sRC = freeSleepNode(sSleepNode);
    IDE_TEST( sRC != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_TIME_WAIT )
    {
        /* Condition timedWait ���з� ���� ���� FATAL */
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;
    
    switch ( sState )
    {
        case 2:
            (void)deleteSleepNode(&iduFitManager::mSleepList, sSleepNode);
            /* fall through */
        case 1:
            (void)freeSleepNode(sSleepNode);
            /* fall through */
        default:
        {
            break;
        }
    }

    return IDE_FAILURE;
}

IDE_RC iduFitManager::wakeupServerBySignal( iduFitAction *aFitAction )
{
    IDE_RC sRC = IDE_FAILURE;

    /* BUG-39414 */
    ideLogEntry sLog(IDE_FIT_0);

    iduSleepNode *sDestNode = NULL;

    /* ���� ��� ��带 �˻��ϱ� */
    sRC = getSleepNode(aFitAction->mTID, &sDestNode);
    IDE_TEST( sRC != IDE_SUCCESS );

    /* WAKEUP �����ϱ� ���� ���� ��� */
    IDE_TEST( sDestNode->mMutex.lock(NULL) != IDE_SUCCESS );

    /* �ñ׳��� �߼��ϱ� */   
    IDE_TEST_RAISE( sDestNode->mCond.signal() != IDE_SUCCESS, ERR_COND_SIGNAL );

    /* WAKEUP �α׸� ����ϱ� */
    sLog.append("HDB server sent a wakeup signal.\n");
    sLog.appendFormat( "[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                        aFitAction->mFileName,
                        aFitAction->mCodeLine,
                        aFitAction->mTID );
    sLog.write();

    /* WAKEUP �Ϸ��ϱ� ���� ���� Ǯ�� */
    IDE_TEST( sDestNode->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_SIGNAL )
    {
        /* Condition Signal �߼� ���з� ���� ���� FATAL */
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC iduFitManager::wakeupServerByBroadcast( iduFitAction *aFitAction )
{
    iduSleepNode *sDestNode = NULL;
    iduSleepNode *sTailNode = NULL;

    sTailNode = &(iduFitManager::mSleepList.mTail);
   
    IDE_TEST( iduFitManager::mSleepList.mMutex.lock(NULL) != IDE_SUCCESS );

    for ( sDestNode = iduFitManager::mSleepList.mHead.mNext;
          sDestNode != sTailNode;
          sDestNode = sDestNode->mNext )
    {
        /* �ñ׳��� �߼��ϱ� */
        IDE_TEST_RAISE( sDestNode->mCond.signal() != IDE_SUCCESS, ERR_COND_SIGNAL );

        /* WAKEUP �α׸� ����ϱ� */
        ideLog::log ( IDE_FIT_0, 
                      "HDB server sent a wakeup signal.\n[%s:%"ID_UINT32_FMT":\"%s\"]",
                      aFitAction->mFileName,
                      aFitAction->mCodeLine,
                      aFitAction->mTID );

    }

    IDE_TEST( iduFitManager::mSleepList.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_SIGNAL )
    {
        /* Condition Signal �߼� ���з� ���� ���� FATAL */
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC iduFitManager::sigsegvServer( iduFitAction *aFitAction )
{

    ideLogEntry sLog(IDE_FIT_0);

    sLog.setTailless(ACP_TRUE);
    sLog.append( "Server was recevied by SIGSEGV signal\n" );
    sLog.appendFormat( "[%s:%"ID_UINT32_FMT":\"%s\"]\n",
                        aFitAction->mFileName,
                        aFitAction->mCodeLine,
                        aFitAction->mTID );
    sLog.append( "IDU_FIT_POINT_SIGNAL\n" );
    sLog.write();

    ideLog::flushAllModuleLogs();

    return IDE_SUCCESS;
}

/**************************************************************************************** 
 * FITSERVER �� FITPOINT �� ���� ��� ����� �� �ʿ䰡 ����.
 * ����, Shared Memory �� ���� FITPOINT �ִ��� Ȯ���ϰ�, ���ٸ� FITSERVER �� ����� ���� �ʴ´�. 
 ****************************************************************************************/
IDE_RC iduFitManager::attachSharedMemory()
{
    SInt sShmid = PDL_INVALID_HANDLE;
    key_t sKey;

    IDE_TEST( getSharedMemoryKey( &sKey ) != IDE_SUCCESS );

    if ( sKey != 0 )
    {

        sShmid = idlOS::shmget( (key_t)sKey, sizeof(SInt), 0666|IPC_CREAT );

        IDE_TEST_RAISE( sShmid == PDL_INVALID_HANDLE, ERR_GET_SHM );
        
        mShmPtr = idlOS::shmat( sShmid, (void *)0, 0 );

        if ( mShmPtr == (void *)-1 )
        {
            IDE_RAISE( ERR_ATTACH_SHM );
        }
        else
        {
            /* Do Nothing */
        }

        mShmBuffer = (int *)mShmPtr;

        IDU_FIT_LOG( "Shared Memory Attach Success" );
    }
    else
    {
        /* FIT_SHM_KEY = 0 */
        IDU_FIT_LOG( "Disabled Shared Memory" );
        mShmBuffer = NULL;
        mShmPtr = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GET_SHM )
    {
        IDU_FIT_DEBUG_LOG( "Failed to shmget()" );
    }

    IDE_EXCEPTION( ERR_ATTACH_SHM )
    {
        IDU_FIT_DEBUG_LOG( "Failed to shmat()" );
    }

    IDE_EXCEPTION_END;

    mShmBuffer = NULL;
    mShmPtr = NULL;

    return IDE_FAILURE;
}

/**************************************************************************************** 
 * FIT_SHM_KEY �� �ִٸ� �� ���� key �� ���
 * FIT_SHM_KEY �� 0 �̶�� shared memory ������� ����
 * FIT_SHM_KEY �� ������ ( FIT_SHM_KEY_DEFAULT * FIT_SHM_KEY_WEIGHT_NUMBER ) �� key �� ���
 * FIT_SHM_KEY_DEFAULT �� ALTIBASE_PORT_NO �� default 
*****************************************************************************************/
IDE_RC iduFitManager::getSharedMemoryKey( key_t *aKey )
{

    SChar *sFitShmKeyEnv = NULL;
    UInt sFitShmKey = 0;

    SChar *sFitAltibasePortNoEnv = NULL;
    UInt sAltibasePortNo = 0;

    SChar *sFitShmKeyDefaultEnv = NULL;
    UInt sFitShmKeyDefault = 0;

    /* FIT_SHM_KEY �� ������ FIT_SHM_KEY �� key �� ��� */
    sFitShmKeyEnv = idlOS::getenv( IDU_FIT_SHM_KEY );

    if ( sFitShmKeyEnv != NULL )
    {
        sFitShmKey = idlOS::strtol( sFitShmKeyEnv, NULL, 10 );

        *aKey = (key_t)sFitShmKey;
    }
    else
    {
        /* ALTIBASE_PORT_NO */
        sFitAltibasePortNoEnv = idlOS::getenv( IDU_FIT_ALTIBASE_PORT_NO );

        IDE_TEST_RAISE( sFitAltibasePortNoEnv == NULL, NOT_FOUND_ALTIBASE_PORT_NO );

        sAltibasePortNo = idlOS::strtol( sFitAltibasePortNoEnv, NULL, 10 );

        IDE_TEST_RAISE( sAltibasePortNo == 0, ERR_GET_ALTIBASE_PORT_NO );

        /* FIT_SHM_KEY_DEFAULT */        
        sFitShmKeyDefaultEnv = idlOS::getenv( IDU_FIT_SHM_KEY_DEFAULT );

        /* FIT_SHM_KEY_DEFAULT �� ������ ALTIBASE_PORT_NO �� ��� */
        if ( sFitShmKeyDefaultEnv == NULL ) 
        {
            *aKey = sAltibasePortNo * IDU_FIT_SHM_KEY_WEIGHT_NUMBER;
        }
        else
        {
            /* FIT_SHM_KEY_DEFAULT �� ������ FIT_SHM_KEY_DEFAULT �� ��� */
            sFitShmKeyDefault = idlOS::strtol( sFitShmKeyDefaultEnv, NULL, 10 );

            IDE_TEST_RAISE( sFitShmKeyDefault == 0, ERR_GET_FIT_SHM_KEY_DEFAULT );       
 
            *aKey = sFitShmKeyDefault * IDU_FIT_SHM_KEY_WEIGHT_NUMBER;

        }
    }

          
    return IDE_SUCCESS;

    IDE_EXCEPTION( NOT_FOUND_ALTIBASE_PORT_NO )
    {
        IDU_FIT_DEBUG_LOG( "getSharedMemoryKey : ALTIBASE_PORT_NO Not Found \n" );
    }

    IDE_EXCEPTION( ERR_GET_ALTIBASE_PORT_NO )
    {
        IDU_FIT_DEBUG_LOG( "getSharedMemoryKey : Failed to get ALTIBASE_PORT_NO \n" );
    }

    IDE_EXCEPTION( ERR_GET_FIT_SHM_KEY_DEFAULT ) 
    {
        IDU_FIT_DEBUG_LOG( "getSharedMemoryKey : Failed to get FIT_SHM_KEY_DEFAULT \n" );
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#endif /* ALTIBASE_FIT_CHECK */
