/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id:
 **********************************************************************/

#include <idu.h>
#include <iduFatalCallback.h>

iduMutex            iduFatalCallback::mMutex;
iduCallbackFunction iduFatalCallback::mCallbackFunction[IDU_FATAL_INFO_CALLBACK_ARR_SIZE] = { NULL, };
idBool              iduFatalCallback::mInit = ID_FALSE;


/***********************************************************************
 * Description : Fatal Info �ʱ�ȭ
 *
 * Fatal �� SM ���� ���� ��� callback �ʱ�ȭ ��
 * idu callback�� doCallback ���
 *
 **********************************************************************/
IDE_RC iduFatalCallback::initializeStatic()
{
    UInt i;

    IDE_TEST( mMutex.initialize( (SChar*) "Fatal Info Callback Mutex",
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    for( i = 0 ; i < IDU_FATAL_INFO_CALLBACK_ARR_SIZE ; i++ )
    {
        mCallbackFunction[i] = NULL;
    }

    mInit = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    mInit = ID_FALSE;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Fatal Info ���� �� ID�� callback���� ����
 *
 **********************************************************************/
IDE_RC iduFatalCallback::destroyStatic()
{
    mInit = ID_FALSE;

    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Fatal Info �Լ� �߰�
 *
 *
 **********************************************************************/
void iduFatalCallback::setCallback( iduCallbackFunction aCallbackFunction )
{
    UInt i;

    if ( mInit == ID_TRUE )
    {
        lock();

        for( i = 0 ; i < IDU_FATAL_INFO_CALLBACK_ARR_SIZE  ; i++ )
        {
            if( mCallbackFunction[i] == NULL )
            {
                mCallbackFunction[i] = aCallbackFunction;
                break;
            }
        }

        unlock();

        // ���ڸ��� �߰����� ���Ѱ��
        IDE_DASSERT( i < IDU_FATAL_INFO_CALLBACK_ARR_SIZE );
    }
}

/***********************************************************************
 * Description : Fatal Info �Լ� ����
 *
 *
 **********************************************************************/
void iduFatalCallback::unsetCallback( iduCallbackFunction aCallbackFunction )
{
    UInt i;

    if ( mInit == ID_TRUE )
    {
        lock();

        for ( i = 0 ; i < IDU_FATAL_INFO_CALLBACK_ARR_SIZE ; i++ )
        {
            if ( mCallbackFunction[i] == aCallbackFunction )
            {
                mCallbackFunction[i] = NULL;
                break;
            }
        }

        unlock();
    }
}

/***********************************************************************
 * Description : ��ϵ� Fatal Info �Լ� ���� - ����� ���� ���
 *
 *
 **********************************************************************/
void iduFatalCallback::doCallback()
{
    UInt   i;
    iduCallbackFunction sCallback;

    if ( mInit == ID_TRUE )
    {
        ideLog::log( IDE_DUMP_0,
                     "\n\n"
                     "*--------------------------------------------------------*\n"
                     "*           Execute module callback functions            *\n"
                     "*--------------------------------------------------------*\n" );

        for ( i = 0 ; i < IDU_FATAL_INFO_CALLBACK_ARR_SIZE ; i++ )
        {
            sCallback = mCallbackFunction[i];
            mCallbackFunction[i] = NULL;

            if ( sCallback != NULL )
            {
                sCallback();
                ideLog::log( IDE_DUMP_0,
                             "========================================================\n");
            }
        }
    }
}
