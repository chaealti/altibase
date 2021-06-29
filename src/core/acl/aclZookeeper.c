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
 * $Id$
 **********************************************************************/

#include <aclZookeeper.h>
#include <acp.h>

ZKCResult aclZKC_result( acp_sint32_t aResultCode )
{
    ZKCResult sResult;

    switch( aResultCode )
    {
        case ZOK:
            sResult = ZKC_SUCCESS;
            break;
        case ZCONNECTIONLOSS:
            sResult = ZKC_CONNECTION_MISSING;
            break;
        case ZNONODE:
            sResult = ZKC_NO_NODE;
            break;
        case ZNODEEXISTS:
            sResult = ZKC_NODE_EXISTS;
            break;
        case ZOPERATIONTIMEOUT:
            sResult = ZKC_WAITING_TIMEOUT;
            break;
        case ZKC_DATA_TOO_LARGE:
            sResult = ZKC_DATA_TOO_LARGE;
            break;
        default:
            sResult = aResultCode;
            break;
    }

    return sResult;
}

/* ����� ZKS�� ������ ZKS�� �����ϴ� �Լ� */
ZKCResult aclZKC_connect( aclZK_Handler ** aZKHandler,   /* zookeeper handler */
                          acp_char_t     * aServerInfo,  /* ��Ű�� ������ ���� */
                          acp_uint32_t     aTimeout )    /* timeout ���� �ð� */
{
    aclZK_Handler  * zh;
    ZKCResult        sReturn;

    /* connection test */
    acp_sint32_t     sResult;
    acp_char_t       sTestPath[] = "/altibase_shard";
    acp_char_t       sTestBuffer[128];
    acp_sint32_t     sTestBufferLen = 128;
    acp_sint32_t     sRetryCnt = 0;

    /* ���ʿ��� zookeeper trace log�� �����Ѵ�. */
    zoo_set_debug_level( ZOO_LOG_LEVEL_ERROR );

    /* ���������� ���������� �������ڸ��� connection lost�Ǵ� ��찡 �߰ߵǾ���. *
     * �̸� ���� ���� connection lost�� ��� �ִ� 10�� ��õ� �ϵ��� �Ѵ�.       */
    while( sRetryCnt < ZKC_CONNECT_RETRY_MAX_CNT )
    {
        /* 1. connection info�� ���������� üũ�Ѵ�. */
        if( aServerInfo == NULL )
        {
            sReturn = ZKC_CONNECTION_INFO_MISSING;

            break;
        }

        /* 2. ZKS�� �����Ѵ�. */
        zh = zookeeper_init( aServerInfo,   /* server ip/port */
                             NULL,          /* watch */
                             aTimeout,      /* timeout check */
                             0,             /* client id */
                             0,             /* context */
                             0 );           /* flags */

        if( zh == NULL )
        {
            sReturn = ZKC_CONNECTION_FAIL;

            break;
        }
        else
        {
            /* ���� �۵� ���� ZKS�� 2�� ������ ���¿��� ZKS�� ������ ��� 
             * connection ��ü���� �����ϳ� ZKS�� �̿��� ������ �� �� ����.
             * �̷� ��츦 ����� ���ӿ� �����ߴ��� �׽�Ʈ �غ���. */
            sResult = zoo_get( zh,
                               sTestPath,
                               NULL,
                               sTestBuffer,
                               &sTestBufferLen,
                               NULL );

            /* ���������� ���ٽ� ZOK�� �����ϰ� zookeeper client�� ������� ��� NONODE�� �����Ѵ�. *
             * ������̵� ���� ��ü�� ������ ���̹Ƿ� SUCCESS ó���Ѵ�.                             */
            if( ( sResult == ZOK ) || ( sResult == ZNONODE ) )
            {
                *aZKHandler = zh;

                sReturn = ZKC_SUCCESS;

                break;
            }
            else
            {
                aclZKC_disconnect( zh );

                sReturn =  ZKC_CONNECTION_FAIL;

                sRetryCnt++;
            }
        }
    }

    return sReturn;
}

/* ZKS���� ������ ���� �Լ� */
void aclZKC_disconnect( aclZK_Handler * aZKHandler /* zookeeper handler */ )
{
    if( aZKHandler != NULL )
    {
        zookeeper_close( aZKHandler );
    }
}

/* ZKS�� �����͸� ������ �Լ� */
ZKCResult aclZKC_setInfo( aclZK_Handler * aZKHandler, /* zookeeper handler */
                          acp_char_t    * aPath,      /* path */
                          acp_char_t    * aData,      /* ������ ������ */
                          acp_sint32_t    aDataSize ) /* ������ �������� ���� */
{
    acp_sint32_t sResult;

    sResult = zoo_set( aZKHandler,  /* zookeeper handler */
                       aPath,       /* path */
                       aData,       /* ������ ������ */
                       aDataSize,   /* ������ �������� ���� */
                       -1 );        /* version */

    return aclZKC_result( sResult );
}

/* ZKS�� ���丮(���)�� �߰��ϴ� �Լ� */
ZKCResult aclZKC_insertNode( aclZK_Handler * aZKHandler,    /* zookeeper handler */
                             acp_char_t    * aPath,         /* path */
                             acp_char_t    * aData,         /* ������ ������ */
                             acp_sint32_t    aDataSize,     /* ������ �������� ���� */
                             acp_bool_t      aIsEphemeral ) /* �ӽ� ��� ���� */
{
    acp_sint32_t sResult;
    acp_sint32_t sNodeflag;

    if( aIsEphemeral == ACP_TRUE )
    {
        sNodeflag = ZOO_EPHEMERAL;
    }
    else
    {
        sNodeflag = ZOO_PERSISTENT;
    }

    sResult = zoo_create( aZKHandler,            /* zookeeper handler */ 
                          aPath,                 /* �߰��� path */
                          aData,                 /* ������ ������ */
                          aDataSize,             /* ������ �������� ���� */
                          &ZOO_OPEN_ACL_UNSAFE,  /* ACL */
                          sNodeflag,             /* flags */
                          NULL,                  /* path_buffer */
                          0 );                   /* path_buffer_len */

    return aclZKC_result( sResult );
}

/* ZKS�� �����͸� �������� �Լ� */ 
ZKCResult aclZKC_getInfo( aclZK_Handler * aZKHandler,   /* zookeeper handler */
                          acp_char_t    * aPath,        /* �����͸� ������ path */
                          acp_char_t    * aBuffer,      /* ������ �����͸� ������ ���� */
                          acp_sint32_t  * aDataLen )    /* ������ �������� ���� */

{
    acp_sint32_t sResult;
    acp_sint32_t sBufferSize = *aDataLen;

    acpMemSet( aBuffer, 0, *aDataLen );

    /* ������ : zoo_get�� DataLen ������ �ϳ��� ������ 2���� ���� �Ѵ�.
     *          1. �����͸� ������ ������ ����(IN)
     *          2. ������ ������ �������� ����(OUT)
     *          �׷��Ƿ� �ѹ� get �Լ��� ����� �� DataLen ������ �״�� �ٽ� ����� ��� 
     *          �ι�° get�� ù get���� ������ ���̷� ���ѵǴ� ������ �߻��Ѵ�.  */

    sResult = zoo_get( aZKHandler,   /* zookeeper handler */
                       aPath,        /* �����͸� ������ path */
                       NULL,         /* watch */
                       aBuffer,      /* ������ �����͸� ������ ���� */
                       aDataLen,     /* ������ �������� ���� */
                       NULL );       /* ��� */

    /* ���������� �޾ƿ����� �������� ���̰� ������ ũ��� ���ٸ�            *
     * �������� ���̰� �� ��� zookeeper server�� ���� �߷��� ���ɼ��� �ִ�. */
    if( ( sResult == ZOK ) && ( sBufferSize == *aDataLen ) )
    {
        sResult = ZKC_DATA_TOO_LARGE;
    }

    return aclZKC_result( sResult );
}

/* ZKS�� Ư�� ����� �ڽ��� �������� �Լ� */
ZKCResult aclZKC_getChildren( aclZK_Handler * aZKHandler, /* zookeeper handler */
                              acp_char_t    * aPath,      /* �ڽ� ��带 ������ ����� path */
                              String_vector * aBuffer )   /* ������ �ڽ��� ������ ���� */
{
    acp_sint32_t sResult;

    sResult = zoo_get_children( aZKHandler, /* zookeeper handler */
                                aPath,      /* �ڽ� ��带 ������ ����� path */
                                NULL,       /* watch */
                                aBuffer );  /* ������ �ڽ��� ������ ����(string vector) */ 

    return aclZKC_result( sResult );
}

/* ZKS�� ��带 �����ϴ� �Լ� */
ZKCResult aclZKC_deleteNode( aclZK_Handler * aZKHandler, /* zookeeper handler */
                             acp_char_t    * aPath )     /* ������ ����� path */
{
    acp_sint32_t sResult;

    sResult = zoo_delete( aZKHandler, /* zookeeper handler */
                          aPath,      /* ������ ����� path */
                          -1 );       /* version */

    return aclZKC_result( sResult );
}

/* ZKS�� �����͸� ������ �Լ�
   ZKS���� ������� ������ ���Ű� �����Ƿ� �� �����ͷ� ���� ��ü�Ͽ� �����͸� ���� �Ͱ� ������ ȿ���� ����.  */
ZKCResult aclZKC_deleteInfo( aclZK_Handler * aZKHandler,  /* zookeeper handler */
                             acp_char_t    * aPath )      /* �����͸� ������ ����� path */
{
    acp_sint32_t sResult;

    sResult = zoo_set( aZKHandler,  /* zookeeper handler */
                       aPath,       /* path */
                       NULL,        /* ������ ������ */
                       0,           /* ������ �������� ���� */
                       -1 );        /* version */

    return aclZKC_result( sResult );
}

/* ZKS�� Ư�� ��尡 �����ϴ��� üũ�ϴ� �Լ� */
ZKCResult aclZKC_existNode( aclZK_Handler * aZKHandler, /* zookeeper handler */
                            acp_char_t    * aPath )      /* üũ�� ����� path */
{
    acp_sint32_t sResult;

    sResult = zoo_exists( aZKHandler, /* zookeeper handler */
                          aPath,      /* üũ�� ����� path */
                          0,          /* watch */
                          NULL );     /* ������� */

    return aclZKC_result( sResult );    
}

/* multi operation���� �۾��� �����ϴ� �Լ� */
ZKCResult aclZKC_multiOp( aclZK_Handler   * aZKHandler,    /* zookeeper handler */
                          acp_sint32_t      aCount,        /* multi op�� ������ count */
                          zoo_op_t        * aOps,          /* multi operation�� set  */
                          zoo_op_result_t * aOpResult )    /* �� op�� ����� ���� result set */
{
    acp_sint32_t sResult;

    sResult = zoo_multi( aZKHandler, 
                         aCount, 
                         aOps, 
                         aOpResult );

    return aclZKC_result( sResult );
}

/* Ư�� ����� �����Ϳ� watch�� �Ŵ� �Լ� 
 * ����� �����Ͱ� ����ɶ� �˶��� �߻��Ѵ�.
 * �����͸� ����� ������ ������ �����ص� �˶��� �߻��Ѵ�. */
ZKCResult aclZKC_Watch_Data( aclZK_Handler   * aZKHandler,  /* zookeeper handler */
                             watcher_fn        aWatchFunc,  /* �˶� �߻��� ȣ��Ǵ� �Լ� */
                             acp_char_t      * aPath )      /* ������ ������ Ȯ���� ����� path */
{
    acp_sint32_t sResult;
    acp_char_t   sBuffer[128];
    acp_sint32_t sBufferLen = 128;
    
    sResult = zoo_wget( aZKHandler,     /* zookeeper handler */
                        aPath,          /* path */
                        aWatchFunc,     /* watcher func */
                        NULL,           /* watcherCtx */
                        sBuffer,        /* buffer */
                        &sBufferLen,    /* buffer_len */
                        NULL );         /* ������� */

    return aclZKC_result( sResult );
} 

/* Ư�� ����� �߰�/������ watch�� �Ŵ� �Լ�
 * �ش� ��尡 �߰�/���� �� �� ���� �˶��� �߻��Ѵ�. */
ZKCResult aclZKC_Watch_Node( aclZK_Handler   * aZKHandler,  /* zookeeper_handler */
                             watcher_fn        aWatchFunc,  /* �˶� �߻��� ȣ��Ǵ� �Լ� */
                             acp_char_t      * aPath )      /* �߰�/������ Ȯ���� ����� path */
{
    acp_sint32_t          sResult;
     
    sResult = zoo_wexists( aZKHandler,  /* zookeeper_handler */
                           aPath,       /* path */
                           aWatchFunc,  /* watcher func */
                           NULL,        /* watcherCtx */
                           NULL );      /* ������� */
    
    return aclZKC_result( sResult );
}
