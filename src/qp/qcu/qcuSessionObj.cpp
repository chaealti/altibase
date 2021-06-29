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
 * $Id: qcuSessionObj.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description : PROJ-1371 PSM File Handling
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <qcuProperty.h>
#include <qcuSessionObj.h>
#include <iduFileStream.h>
#include <qsf.h>

// BUG-40854
#include <idCore.h>
extern ULong    gInitialValue;

/* BUG-41307 User Lock ���� */
#define USER_LOCK_RESULT_SUCCESS            (0)
#define USER_LOCK_RESULT_TIMEOUT            (1)
#define USER_LOCK_RESULT_PARAMETER_ERROR    (3)
#define USER_LOCK_RESULT_OWN_ERROR          (4)

/* BUG-41307 User Lock ���� */
acl_mem_pool_t     gUserLockMemPool;
acl_hash_table_t   gUserLockHashTable;

IDE_RC qcuSessionObj::initOpenedFileList( qcSessionObjInfo* aSessionObjInfo )
{
/***********************************************************************
 *
 * Description :
 *  filelist�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "qcuSessionObj::initOpenedFileList"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    IDE_DASSERT( aSessionObjInfo != NULL );
        
    aSessionObjInfo->filelist.count = 0;
    aSessionObjInfo->filelist.fileNode = NULL;
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qcuSessionObj::closeAllOpenedFile(
                           qcSessionObjInfo* aSessionObjInfo )
{
/***********************************************************************
 *
 * Description :
 *  �����ִ� ��� file�� close�ϰ� node�޸� ����
 *
 * Implementation :
 *  1. node�� next�� ���󰡸鼭 closeFile�� �ϰ� �޸� free
 *  2. filelist�ʱ�ȭ(filenode�� null�� �ʱ�ȭ)
 *
 ***********************************************************************/
    
#define IDE_FN "qcuSessionObj::closeAllOpenedFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qcFileNode* sNode;
    qcFileNode* sNext;
    
    IDE_DASSERT( aSessionObjInfo != NULL );

    for (sNode = aSessionObjInfo->filelist.fileNode;
         sNode != NULL;
         sNode = sNext)
    {
        sNext = sNode->next;

        (void)( iduFileStream::closeFile( sNode->fp ) );

        (void)( iduMemMgr::free(sNode) );
        sNode = NULL;
        
        aSessionObjInfo->filelist.count--;
    }

    aSessionObjInfo->filelist.fileNode = NULL;

    IDE_DASSERT( aSessionObjInfo->filelist.count == 0 );
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qcuSessionObj::addOpenedFile( qcSessionObjInfo* aSessionObjInfo,
                                     FILE* aFp )
{
/***********************************************************************
 *
 * Description :
 *  fopen���� open�� file�� filelist�� �߰�
 *
 * Implementation :
 *
 ***********************************************************************/
    
#define IDE_FN "qcuSessionObj::addOpenedFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qcFileNode* sFileNodeNew;
    UInt        sState = 0;

    IDE_DASSERT( aSessionObjInfo != NULL );

    // BUG-13068
    // limit�� �ʰ��ϸ� ����
    IDE_TEST_RAISE( aSessionObjInfo->filelist.count >=
                    QCU_PSM_FILE_OPEN_LIMIT, ERR_OPEN_LIMIT );
    
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QCI,
                                 ID_SIZEOF(qcFileNode),
                                 (void**)&sFileNodeNew )
              != IDE_SUCCESS );
    sState = 1;
    sFileNodeNew->fp = aFp;

    sFileNodeNew->next = aSessionObjInfo->filelist.fileNode;
    aSessionObjInfo->filelist.fileNode = sFileNodeNew;

    aSessionObjInfo->filelist.count++;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_OPEN_LIMIT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCU_PSM_FILE_OPEN_LIMIT_EXCEED));
    }
    
    IDE_EXCEPTION_END;

    // �������� ������ file�� �ݾƾ� ��.
    (void)iduFileStream::closeFile(aFp);
    
    switch( sState )
    {
        case 1:
            iduMemMgr::free( sFileNodeNew );
            break;
        default:
            break;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qcuSessionObj::delOpenedFile( qcSessionObjInfo * aSessionObjInfo,
                                     FILE             * aFp,
                                     idBool           * aExist )
{
/***********************************************************************
 *
 * Description :
 *  fclose�� close�� file�� filelist���� ����
 *
 * Implementation :
 *  1. previous node�� �����ϱ� ���� headnode�� ����� next�� ó�� ��ġ��
 *     ����
 *  2. previous, current�� next�� �ű�鼭 fp�� ���� ��尡 �ִ��� �˻�
 *  3. ���� ã�� ��尡 ���ٸ� �ƹ��ϵ� ���� ����
 *     => �߰��� fclose_all�� ������ �Ŀ� fclose�� �������� �����
 *  4. ��带 ã�� ��� previous�� next�� ã�� ����� next�� �����ϰ�
 *     ��带 ������.
 *  5. previous�� next�� �� ó�� ����� �� �����Ƿ� filelist�� node��
 *     headnode�� next�� ����Ű�� ��
 *
 ***********************************************************************/
    
#define IDE_FN "qcuSessionObj::delOpenedFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qcFileNode* sFileNodePrev;
    qcFileNode* sFileNodeCurrent;
    qcFileNode  sFileNodeHead;

    IDE_DASSERT( aSessionObjInfo != NULL );

    *aExist = ID_TRUE;

    sFileNodeHead.next = aSessionObjInfo->filelist.fileNode;
    
    sFileNodePrev = &sFileNodeHead;
    
    sFileNodeCurrent = sFileNodePrev->next;
    
    while( sFileNodeCurrent != NULL )
    {
        if( sFileNodeCurrent->fp == aFp )
        {
            break;
        }
        else
        {
            sFileNodePrev = sFileNodeCurrent;
            sFileNodeCurrent = sFileNodeCurrent->next;
        }
    }

    if( sFileNodeCurrent == NULL )
    {
        *aExist = ID_FALSE;
    }
    else
    {
        sFileNodePrev->next = sFileNodeCurrent->next;
  
        (void)( iduMemMgr::free( sFileNodeCurrent ) );
        sFileNodeCurrent = NULL;

        aSessionObjInfo->filelist.fileNode = sFileNodeHead.next;

        IDE_DASSERT( aSessionObjInfo->filelist.count > 0 );

        aSessionObjInfo->filelist.count--;
    }
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qcuSessionObj::initSendSocket( qcSessionObjInfo * aSessionObjInfo )
{
    IDE_DASSERT( aSessionObjInfo != NULL );

    aSessionObjInfo->sendSocket = PDL_INVALID_SOCKET;

    return IDE_SUCCESS;
}

IDE_RC qcuSessionObj::closeSendSocket( qcSessionObjInfo * aSessionObjInfo )
{
    IDE_DASSERT( aSessionObjInfo != NULL );

    if( aSessionObjInfo->sendSocket != PDL_INVALID_SOCKET )
    {
        idlOS::closesocket(aSessionObjInfo->sendSocket);
        aSessionObjInfo->sendSocket = PDL_INVALID_SOCKET;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC qcuSessionObj::getSendSocket( PDL_SOCKET       * aSendSocket,
                                     qcSessionObjInfo * aSessionObjInfo,
                                     SInt               aInetFamily )
{
    IDE_DASSERT( aSessionObjInfo != NULL );

    if( aSessionObjInfo->sendSocket == PDL_INVALID_SOCKET )
    {
        aSessionObjInfo->sendSocket = idlOS::socket( aInetFamily, SOCK_DGRAM, 0 );
    }
    else
    {
        // Nothing to do.
    }

    *aSendSocket = aSessionObjInfo->sendSocket;

    return IDE_SUCCESS;
}

// BUG-40854
IDE_RC qcuSessionObj::initConnection( qcSessionObjInfo * aSessionObjInfo )
{
    IDE_DASSERT( aSessionObjInfo != NULL );

    aSessionObjInfo->connectionList.count = 0;
    aSessionObjInfo->connectionList.connectionNode = NULL;

    return IDE_SUCCESS;
}


IDE_RC qcuSessionObj::addConnection( qcSessionObjInfo * aSessionObjInfo,
                                     PDL_SOCKET         aSocket,
                                     SLong            * aConnectionNodeKey )
{
    qcConnectionNode * sConnectNode = NULL;

    IDE_DASSERT( aSessionObjInfo != NULL );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_QCI,
                                 ID_SIZEOF( qcConnectionNode ),
                                 (void**)&sConnectNode )
              != IDE_SUCCESS );

    sConnectNode->socket = aSocket;
    sConnectNode->connectNodeKey = idCore::acpAtomicInc64( &gInitialValue );
    sConnectNode->connectState = QC_CONNECTION_STATE_CONNECTED;

    sConnectNode->next = aSessionObjInfo->connectionList.connectionNode;
    aSessionObjInfo->connectionList.connectionNode = sConnectNode;
    aSessionObjInfo->connectionList.count++;
    
    *aConnectionNodeKey = sConnectNode->connectNodeKey;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcuSessionObj::closeAllConnection( qcSessionObjInfo * aSessionObjInfo )
{
    qcConnectionNode * sNode = NULL;
    qcConnectionNode * sNext;

    IDE_DASSERT( aSessionObjInfo != NULL );

    for ( sNode =  aSessionObjInfo->connectionList.connectionNode;
          sNode != NULL;
          sNode =  sNext )
    {
        sNext = sNode->next;
        
        idlOS::closesocket( sNode->socket );
        
        (void)iduMemMgr::free( sNode );
        
        sNode = NULL;

        aSessionObjInfo->connectionList.count --;
    }

    aSessionObjInfo->connectionList.connectionNode = NULL;

    IDE_ASSERT( aSessionObjInfo->connectionList.count == 0 );

    return IDE_SUCCESS;
}

IDE_RC qcuSessionObj::delConnection( qcSessionObjInfo * aSessionObjInfo,
                                     SLong              aConnectionNodeKey )
{
    qcConnectionNode * sNodePrev;
    qcConnectionNode * sNodeCurrent;
    qcConnectionNode   sNodeHead;

    IDE_DASSERT( aSessionObjInfo != NULL );

    sNodeHead.next = aSessionObjInfo->connectionList.connectionNode;

    sNodePrev = &sNodeHead;

    sNodeCurrent = sNodePrev->next;

    while ( sNodeCurrent != NULL )
    {
        if ( sNodeCurrent->connectNodeKey == aConnectionNodeKey )
        {
            break;
        }
        else
        {
            sNodePrev = sNodeCurrent;
            sNodeCurrent = sNodeCurrent->next;
        }
    }

    if ( sNodeCurrent == NULL )
    {
        // already free ( closeAll )
    }
    else
    {
        sNodePrev->next = sNodeCurrent->next;

        IDE_ASSERT( sNodeCurrent->socket != PDL_INVALID_SOCKET );
        idlOS::closesocket( sNodeCurrent->socket );

        (void)iduMemMgr::free( sNodeCurrent );

        sNodeCurrent = NULL;

        aSessionObjInfo->connectionList.connectionNode = sNodeHead.next;
        IDE_ASSERT( aSessionObjInfo->connectionList.count > 0 );
        aSessionObjInfo->connectionList.count--;
    }

    return IDE_SUCCESS;
}

IDE_RC qcuSessionObj::getConnectionSocket( qcSessionObjInfo * aSessionObjInfo,
                                           SLong              aConnectionNodeKey,
                                           PDL_SOCKET       * aSocket )
{
    qcConnectionNode * sNodePrev;
    qcConnectionNode * sNodeCurrent;
    qcConnectionNode   sNodeHead;
    
    IDE_DASSERT( aSessionObjInfo != NULL );
    
    sNodeHead.next = aSessionObjInfo->connectionList.connectionNode;
    
    sNodePrev = &sNodeHead;
    
    sNodeCurrent = sNodePrev->next;
    
    while ( sNodeCurrent != NULL )
    {
        if ( sNodeCurrent->connectNodeKey == aConnectionNodeKey )
        {
            break;
        }
        else
        {
            sNodePrev = sNodeCurrent;
            sNodeCurrent = sNodeCurrent->next;
        }
    }
    
    if ( sNodeCurrent == NULL )
    {
        *aSocket = PDL_INVALID_SOCKET;
    }
    else
    {
        /* PROJ-2657 UTL_SMTP ���� */
        if ( sNodeCurrent->connectState == QC_CONNECTION_STATE_NOCONNECT )
        {
            *aSocket = PDL_INVALID_SOCKET;
        }
        else
        {
            *aSocket = sNodeCurrent->socket;
        }
    }
    
    return IDE_SUCCESS;
}

/* PROJ-2657 UTL_SMTP ���� */
void qcuSessionObj::setConnectionState( qcSessionObjInfo * aSessionObjInfo,
                                        SLong              aConnectionNodeKey,
                                        SInt               aState )
{
    qcConnectionNode * sNodePrev;
    qcConnectionNode * sNodeCurrent;
    qcConnectionNode   sNodeHead;
    
    IDE_DASSERT( aSessionObjInfo != NULL );
    
    sNodeHead.next = aSessionObjInfo->connectionList.connectionNode;
    
    sNodePrev = &sNodeHead;
    
    sNodeCurrent = sNodePrev->next;
    
    while ( sNodeCurrent != NULL )
    {
        if ( sNodeCurrent->connectNodeKey == aConnectionNodeKey )
        {
            break;
        }
        else
        {
            sNodePrev = sNodeCurrent;
            sNodeCurrent = sNodeCurrent->next;
        }
    }
    
    if ( sNodeCurrent == NULL )
    {
        /* Nothing to do */
    }
    else
    {
        sNodeCurrent->connectState = aState;
    }
}

/* PROJ-2657 UTL_SMTP ���� */
void qcuSessionObj::getConnectionState( qcSessionObjInfo * aSessionObjInfo,
                                        SLong              aConnectionNodeKey,
                                        SInt             * aState )
{
    qcConnectionNode * sNodePrev;
    qcConnectionNode * sNodeCurrent;
    qcConnectionNode   sNodeHead;

    IDE_DASSERT( aSessionObjInfo != NULL );

    sNodeHead.next = aSessionObjInfo->connectionList.connectionNode;

    sNodePrev = &sNodeHead;

    sNodeCurrent = sNodePrev->next;

    while ( sNodeCurrent != NULL )
    {
        if ( sNodeCurrent->connectNodeKey == aConnectionNodeKey )
        {
            break;
        }
        else
        {
            sNodePrev = sNodeCurrent;
            sNodeCurrent = sNodeCurrent->next;
        }
    }

    if ( sNodeCurrent == NULL )
    {
        *aState = QC_CONNECTION_STATE_NOCONNECT;
    }
    else
    {
        *aState = sNodeCurrent->connectState;
    }
}

IDE_RC qcuSessionObj::initializeStatic()
{
    UInt    sBucketCount;
    idBool  sIsMemPoolAlloced = ID_FALSE;

    IDE_TEST_RAISE( ACP_RC_NOT_SUCCESS( aclMemPoolCreate( &gUserLockMemPool,
                                                          ID_SIZEOF(qcUserLockNode),
                                                          QCU_USER_LOCK_POOL_INIT_SIZE,
                                                          1 /* ParallelFactor */ ) ),
                    ERR_ALLOC_MEM_POOL );
    sIsMemPoolAlloced = ID_TRUE;

    sBucketCount = QCU_USER_LOCK_POOL_INIT_SIZE * 2 + 1;
    IDE_TEST_RAISE( ACP_RC_NOT_SUCCESS( aclHashCreate( &gUserLockHashTable,
                                                       sBucketCount,
                                                       ID_SIZEOF(SInt),
                                                       aclHashHashInt32,
                                                       aclHashCompInt32,
                                                       ACP_TRUE ) ),
                    ERR_ALLOC_HASH_TABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALLOC_MEM_POOL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_MEM_POOL_ALLOC_FAILED ) );
    }
    IDE_EXCEPTION( ERR_ALLOC_HASH_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_HASH_TABLE_ALLOC_FAILED ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsMemPoolAlloced == ID_TRUE )
    {
        aclMemPoolDestroy( &gUserLockMemPool );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

void qcuSessionObj::finalizeStatic()
{
    qcUserLockNode      * sValue;
    acl_hash_traverse_t   sHashTraverse;
    acp_rc_t              sRC;

    /* Server�� �����ϸ� ��� Session�� �������� ������, Hash Table�� �ִ� ��� User Lock�� Release ���°� �ȴ�.
     * Hash Table�� ��� User Lock�� �����Ѵ�.
     */
    sRC = aclHashTraverseOpen( &sHashTraverse,
                               &gUserLockHashTable,
                               ACP_TRUE );
    if ( ACP_RC_IS_SUCCESS( sRC ) )
    {
        while ( ACP_RC_NOT_EOF( sRC ) )
        {
            /* ACP_RC_SUCCESS, ACP_RC_EOF �� �� �ϳ��̴�. */
            sRC = aclHashTraverseNext( &sHashTraverse, (void **)&sValue );

            if ( ACP_RC_IS_SUCCESS( sRC ) )
            {
                /* Server�� �����ϴ� ��Ȳ�̹Ƿ�, �����ص� ���� Server�� ������ �����Ű�� �ʴ´�. */
                (void)sValue->mutex.destroy();
                aclMemPoolFree( &gUserLockMemPool, (void *)sValue );
            }
            else
            {
                IDE_DASSERT( sRC == ACP_RC_EOF );
            }
        }

        aclHashTraverseClose( &sHashTraverse );

        aclHashDestroy( &gUserLockHashTable );

        /* Hash Table�� ������ �Ŀ� Memory Pool�� �����ؾ� �Ѵ�. */
        aclMemPoolDestroy( &gUserLockMemPool );
    }
    else
    {
        /* Server�� ������ ������ �� �����Ƿ�, �ƹ��͵� ���� �ʴ´�. */
    }

    return;
}

void qcuSessionObj::initializeUserLockList( qcSessionObjInfo * aSessionObjInfo )
{
    IDE_DASSERT( aSessionObjInfo != NULL );

    aSessionObjInfo->userLockList.count = 0;
    aSessionObjInfo->userLockList.userLockNode = NULL;

    return;
}

void qcuSessionObj::finalizeUserLockList( qcSessionObjInfo * aSessionObjInfo )
{
    qcUserLockNode * sNode;
    qcUserLockNode * sNext;

    IDE_DASSERT( aSessionObjInfo != NULL );

    for ( sNode = aSessionObjInfo->userLockList.userLockNode;
          sNode != NULL;
          sNode = sNext )
    {
        sNext = sNode->next;
        sNode->next = NULL;

        /* �����ص� ó���� ����� ����. �����ϰ� ��� �����Ѵ�. */
        (void)sNode->mutex.unlock();

        aSessionObjInfo->userLockList.count--;
    }

    aSessionObjInfo->userLockList.userLockNode = NULL;

    IDE_DASSERT( aSessionObjInfo->userLockList.count == 0 );

    return;
}

IDE_RC qcuSessionObj::requestUserLock( qcSessionObjInfo * aSessionObjInfo,
                                       SInt               aUserLockID,
                                       SInt             * aResult )
{
    qcUserLockNode * sUserLockNode;
    idBool           sIsTimeout;

    IDE_DASSERT( aSessionObjInfo != NULL );

    if ( ( aUserLockID < 0 ) || ( aUserLockID > 1073741823 ) )
    {
        *aResult = USER_LOCK_RESULT_PARAMETER_ERROR;
    }
    else
    {
        /* User Lock�� �̹� ������� Ȯ���Ѵ�. */
        findUserLockFromSession( aSessionObjInfo,
                                 aUserLockID,
                                 &sUserLockNode );

        if ( sUserLockNode != NULL )
        {
            *aResult = USER_LOCK_RESULT_OWN_ERROR;
        }
        else
        {
            /* Session���� Request ������ �Ѱ迡 �����ߴ��� �˻��Ѵ�. */
            IDE_TEST( checkUserLockRequestLimit( aSessionObjInfo ) != IDE_SUCCESS );

            /* Hash Table�� �ִ� ���� �������, ���� ������ �˻��Ѵ�. */
            IDE_TEST( findUserLockFromHashTable( aUserLockID, &sUserLockNode ) != IDE_SUCCESS );

            if ( sUserLockNode != NULL )
            {
                /* Nothing to do */
            }
            else
            {
                IDE_TEST( makeAndAddUserLockToHashTable( aUserLockID,
                                                         &sUserLockNode )
                          != IDE_SUCCESS );

                /* ���� Session�� ���� �������� ���� ������ User Lock�� ���ÿ� Request�ϸ�,
                 * �� Session�� User Lock�� Hash Table�� �ְ�, ������ Session�� �����Ѵ�.
                 * �� ���, ������ Session�� Hash Table���� User Lock�� �ٽ� ��´�.
                 */
                if ( sUserLockNode == NULL )
                {
                    IDE_TEST( findUserLockFromHashTable( aUserLockID, &sUserLockNode ) != IDE_SUCCESS );

                    /* Server ���� ������ Hash Table�� �߰��� �� �� �����Ƿ�, User Lock�� �ݵ�� ��´�. */
                    IDE_ASSERT( sUserLockNode != NULL );
                }
                else
                {
                    /* Nothing to do */
                }
            }

            IDE_TEST( waitForUserLock( sUserLockNode, &sIsTimeout ) != IDE_SUCCESS );

            if ( sIsTimeout == ID_TRUE )
            {
                *aResult = USER_LOCK_RESULT_TIMEOUT;
            }
            else
            {
                addUserLockToSession( aSessionObjInfo, sUserLockNode );

                *aResult = USER_LOCK_RESULT_SUCCESS;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcuSessionObj::releaseUserLock( qcSessionObjInfo * aSessionObjInfo,
                                       SInt               aUserLockID,
                                       SInt             * aResult )
{
    qcUserLockNode * sUserLockNode;

    IDE_DASSERT( aSessionObjInfo != NULL );

    if ( ( aUserLockID < 0 ) || ( aUserLockID > 1073741823 ) )
    {
        *aResult = USER_LOCK_RESULT_PARAMETER_ERROR;
    }
    else
    {
        /* User Lock�� ������ �ִ��� Ȯ���Ѵ�. */
        findUserLockFromSession( aSessionObjInfo,
                                 aUserLockID,
                                 &sUserLockNode );

        if ( sUserLockNode != NULL )
        {
            removeUserLockFromSession( aSessionObjInfo,
                                       aUserLockID );

            IDE_TEST( sUserLockNode->mutex.unlock() != IDE_SUCCESS );

            *aResult = USER_LOCK_RESULT_SUCCESS;
        }
        else
        {
            *aResult = USER_LOCK_RESULT_OWN_ERROR;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcuSessionObj::findUserLockFromSession( qcSessionObjInfo  * aSessionObjInfo,
                                             SInt                aUserLockID,
                                             qcUserLockNode   ** aUserLockNode )
{
    qcUserLockNode * sNode;

    /* �ߺ��� ���ر� ����, Request�� ��� Node�� Ȯ���ؾ� �Ѵ�. ����, �˻� ������ �߿����� �ʴ�.
     * �Ϲ������� Request�� Release�� ������ �ݴ��̴�. Release�� ���� �˻��� ���� �������� �߰��� Node�� ����� Ȯ���� ����.
     */
    for ( sNode = aSessionObjInfo->userLockList.userLockNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        if ( sNode->id == aUserLockID )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aUserLockNode = sNode;

    return;
}

IDE_RC qcuSessionObj::checkUserLockRequestLimit( qcSessionObjInfo * aSessionObjInfo )
{
    IDE_TEST_RAISE( aSessionObjInfo->userLockList.count >= QCU_USER_LOCK_REQUEST_LIMIT,
                    ERR_USER_LOCK_REQUEST_LIMIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_USER_LOCK_REQUEST_LIMIT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_USER_LOCK_REQUEST_LIMIT_EXCEED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcuSessionObj::addUserLockToSession( qcSessionObjInfo * aSessionObjInfo,
                                          qcUserLockNode   * aUserLockNode )
{
    /* �Ϲ������� Request�� Release�� ������ �ݴ��̴�. Release�� ������ �ϱ� ����, �� �տ� Node�� �߰��Ѵ�. */
    aUserLockNode->next = aSessionObjInfo->userLockList.userLockNode;
    aSessionObjInfo->userLockList.userLockNode = aUserLockNode;

    aSessionObjInfo->userLockList.count++;

    return;
}

void qcuSessionObj::removeUserLockFromSession( qcSessionObjInfo * aSessionObjInfo,
                                               SInt               aUserLockID )
{
    qcUserLockNode * sNode;
    qcUserLockNode * sPrev = NULL;

    /* �Ϲ������� Request�� Release�� ������ �ݴ��̴�. Release�� ���� �˻��� ���� �������� �߰��� Node�� ����� Ȯ���� ����. */
    for ( sNode = aSessionObjInfo->userLockList.userLockNode;
          sNode != NULL;
          sNode = sNode->next )
    {
        if ( sNode->id == aUserLockID )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        sPrev = sNode;
    }

    if ( sNode != NULL )    // ��� Node�� ã���� ��
    {
        if ( sPrev == NULL )    // ��� Node�� ù ��°�� ��
        {
            aSessionObjInfo->userLockList.userLockNode = sNode->next;
        }
        else
        {
            sPrev->next = sNode->next;
        }
        sNode->next = NULL;

        aSessionObjInfo->userLockList.count--;
    }
    else
    {
        /* Nothing to do */
    }

    return;
}

IDE_RC qcuSessionObj::findUserLockFromHashTable( SInt              aUserLockID,
                                                 qcUserLockNode ** aUserLockNode )
{
    acp_rc_t    sRC;

    /* It returns ACP_RC_ENOENT if not found */
    sRC = aclHashFind( &gUserLockHashTable,
                       (void *)&aUserLockID,
                       (void **)aUserLockNode );

    switch ( sRC )
    {
        case ACP_RC_SUCCESS :
            /* Nothing to do */
            break;

        case ACP_RC_ENOENT :
            *aUserLockNode = NULL;
            break;

        default :
            IDE_RAISE( ERR_FIND_HASH_TABLE );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FIND_HASH_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_HASH_TABLE_FIND_FAILED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcuSessionObj::addUserLockToHashTable( qcUserLockNode * aUserLockNode,
                                              idBool         * aAlreadyExist )
{
    acp_rc_t    sRC;

    /* It returns ACP_RC_EEXIST if it already exist */
    sRC = aclHashAdd( &gUserLockHashTable,
                      (void *)&(aUserLockNode->id),
                      (void *)aUserLockNode );

    switch ( sRC )
    {
        case ACP_RC_SUCCESS :
            *aAlreadyExist = ID_FALSE;
            break;

        case ACP_RC_EEXIST :
            *aAlreadyExist = ID_TRUE;
            break;

        default :
            IDE_RAISE( ERR_ADD_HASH_TABLE );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ADD_HASH_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_HASH_TABLE_ADD_FAILED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcuSessionObj::makeAndAddUserLockToHashTable( SInt              aUserLockID,
                                                     qcUserLockNode ** aUserLockNode )
{
    qcUserLockNode * sUserLockNode;
    SChar            sName[IDU_MUTEX_NAME_LEN];
    idBool           sIsAlloced = ID_FALSE;
    idBool           sIsInited  = ID_FALSE;
    idBool           sAlreadyExist;

    IDE_TEST_RAISE( ACP_RC_NOT_SUCCESS( aclMemPoolAlloc( &gUserLockMemPool,
                                                         (void **)&sUserLockNode ) ),
                    ERR_ALLOC_MEM_POOL );
    sIsAlloced = ID_TRUE;

    /* ���⿡�� �Ҵ��� User Lock ID�� ���Ŀ� �ٲ��� �ʴ´�. */
    sUserLockNode->id   = aUserLockID;
    sUserLockNode->next = NULL;

    (void)idlOS::snprintf( sName, IDU_MUTEX_NAME_LEN, "USER_LOCK_%"ID_INT32_FMT, aUserLockID );
    IDE_TEST( sUserLockNode->mutex.initialize( sName,
                                               IDU_MUTEX_KIND_POSIX,
                                               IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sIsInited = ID_TRUE;

    IDE_TEST( addUserLockToHashTable( sUserLockNode, &sAlreadyExist ) != IDE_SUCCESS );

    if ( sAlreadyExist != ID_TRUE )
    {
        *aUserLockNode = sUserLockNode;
    }
    else
    {
        /* Hash Table�� �̹� User Lock�� �ִٸ�, �ٸ� Session�� ���� �߰��� ���̴�. */
        sIsInited = ID_FALSE;
        IDE_TEST( sUserLockNode->mutex.destroy() != IDE_SUCCESS );

        sIsAlloced = ID_FALSE;
        aclMemPoolFree( &gUserLockMemPool, (void *)sUserLockNode );

        *aUserLockNode = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALLOC_MEM_POOL )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCU_MEM_POOL_ALLOC_FAILED ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsInited == ID_TRUE )
    {
        (void)sUserLockNode->mutex.destroy();
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsAlloced == ID_TRUE )
    {
        aclMemPoolFree( &gUserLockMemPool, (void *)sUserLockNode );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC qcuSessionObj::waitForUserLock( qcUserLockNode * aUserLockNode,
                                       idBool         * aIsTimeout )
{
    UInt           sElapsedSeconds      = 0;
    UInt           sElapsedMicroSeconds = 0;
    PDL_Time_Value sSleepTime;
    idBool         sIsLocked;

    /* USER_LOCK_REQUEST_CHECK_INTERVAL : 10 ~ 999999 microseconds */
    sSleepTime.set( 0, QCU_USER_LOCK_REQUEST_CHECK_INTERVAL );

    /* USER_LOCK_REQUEST_TIMEOUT�� 0�� ������ �� ���� �õ��Ѵ�. */
    IDE_TEST( aUserLockNode->mutex.trylock( sIsLocked ) != IDE_SUCCESS );

    while ( ( sIsLocked != ID_TRUE ) &&
            ( sElapsedSeconds < QCU_USER_LOCK_REQUEST_TIMEOUT ) )
    {
        /* sleep ������ ��� �ð��� �̸� ����Ѵ�. */
        sElapsedMicroSeconds += QCU_USER_LOCK_REQUEST_CHECK_INTERVAL;
        if ( sElapsedMicroSeconds >= 1000000 )
        {
            sElapsedSeconds++;
            sElapsedMicroSeconds -= 1000000;

            /* ������ sleep�� �����Ѵ�. */
            if ( sElapsedSeconds >= QCU_USER_LOCK_REQUEST_TIMEOUT )
            {
                sSleepTime.set( 0, ( QCU_USER_LOCK_REQUEST_CHECK_INTERVAL - sElapsedMicroSeconds ) );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        (void)idlOS::sleep( sSleepTime );

        IDE_TEST( aUserLockNode->mutex.trylock( sIsLocked ) != IDE_SUCCESS );
    }

    if ( sIsLocked == ID_TRUE )
    {
        *aIsTimeout = ID_FALSE;
    }
    else
    {
        *aIsTimeout = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

