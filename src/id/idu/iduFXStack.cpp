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
 * $Id:$
 **********************************************************************/

#include <idl.h>
#include <iduFile.h>
#include <iduFXStack.h>

/***********************************************************************
 * Description : Stack�� �ʱ�ȭ �Ѵ�. ���ÿ� �ʿ��� Memory�� �̹� �Ҵ��
 *               �Ǿ� �ִٰ� �����ϰ� ����Ѵ�. ���⼭�� Mutex�� ��Ÿ
 *               �ٸ� Member�� �ʱ�ȭ �Ѵ�. �̷��� �ϴ� ������ ������
 *               malloc�� ȣ������ �ʱ� ���ؼ� �̴�.
 *
 * aStackName   - [IN] Stack �̸�, ���� ������ �������� ����Ѵ�.
 * aStackInfo   - [IN] iduFXStackInfo�μ� ���� �ڷᱸ��
 * aMaxItemCnt  - [IN] ������ ������ �ִ� �ִ� Item����
 * aItemSize    - [IN] ������ Item ũ��.
 **********************************************************************/
IDE_RC iduFXStack::initialize( SChar          *aMtxName,
                               iduFXStackInfo *aStackInfo,
                               UInt            aMaxItemCnt,
                               UInt            aItemSize )
{
    IDE_TEST( aStackInfo->mMutex.initialize(
                  aMtxName,
                  IDU_MUTEX_KIND_POSIX,
                  IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST_RAISE(aStackInfo->mCV.initialize() != IDE_SUCCESS,
                   err_cond_init);

    aStackInfo->mMaxItemCnt = aMaxItemCnt;
    aStackInfo->mCurItemCnt = 0;
    aStackInfo->mItemSize   = aItemSize;
    aStackInfo->mWaiterCnt  = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_init );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_ThrCondInit ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Stack�� Mutex�� Condition Variable�� �����Ѵ�.
 *
 * aStackInfo - [IN] Stack �ڷᱸ��.
 **********************************************************************/
IDE_RC iduFXStack::destroy( iduFXStackInfo *aStackInfo )
{
    IDE_ASSERT( iduFXStack::isEmpty( aStackInfo ) == ID_TRUE );

    IDE_TEST_RAISE( aStackInfo->mCV.destroy() != IDE_SUCCESS,
                    err_cond_dest);

    IDE_TEST( aStackInfo->mMutex.destroy()
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_dest );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_ThrCondDestroy ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Stack�� ���ο� Item�� Push�Ѵ�. Max�� �̻� ������
 *               ASSERT�� �װ� �Ѵ�. ���ο� Item�� Push�ϰ� Ȥ��
 *               ���ο� Item�� Push�Ǳ⸦ ��ٸ��� Transa
 *
 * aStackInfo - [IN] Stack �ڷ� ����
 * aItem      - [IN] Push�� Stack�� Item
 **********************************************************************/
IDE_RC iduFXStack::push( idvSQL         *aStatSQL,
                         iduFXStackInfo *aStackInfo,
                         void           *aItem )
{
    UInt   sCurItemCnt;
    UInt   sItemSize;
    SChar *sPushPos;
    SInt   sState = 0;

    IDE_ASSERT( aStackInfo->mMutex.lock( aStatSQL )
                == IDE_SUCCESS );
    sState = 1;

    sCurItemCnt = aStackInfo->mCurItemCnt;
    sItemSize   = aStackInfo->mItemSize;

    /* Max�� �̻� ������ �״´�. */
    IDE_ASSERT( sCurItemCnt < aStackInfo->mMaxItemCnt );

    sPushPos = aStackInfo->mArray + sCurItemCnt * sItemSize;

    /* �������� ũ�Ⱑ vULongũ��� memcpy�� ȣ������ �ʰ� Assign�� �Ѵ�. ����
       �� ���� aItem�� �����ּҰ� 8Byte Align�Ǿ� �־�� �Ѵ�. */
    if( sItemSize == ID_SIZEOF(UInt) )
    {
        *((UInt*)sPushPos) = *((UInt*)aItem);
    }
    else
    {
        if( sItemSize == ID_SIZEOF(ULong) )
        {
            *((ULong*)sPushPos) = *((ULong*)aItem);
        }
        else
        {
            idlOS::memcpy( aStackInfo->mArray + sCurItemCnt * sItemSize,
                           aItem,
                           sItemSize );
        }
    }

    /* ���ο� Item�� Push�Ǿ����Ƿ� ���� Item�� ������ 1���� ��Ų��. */
    aStackInfo->mCurItemCnt++;

    /* ���ο� Item�� Insert�Ǳ⸦ ����ϴ� Thread���� �����ش�. */
    IDE_TEST( getupWaiters( aStackInfo ) != IDE_SUCCESS );

    sState = 0;
    IDE_ASSERT( aStackInfo->mMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState !=  0 )
    {
        IDE_ASSERT( aStackInfo->mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� Item�� Stack�� Push�Ѵ�.
 *
 * aStackInfo - [IN] Stack �ڷ� ����.
 * aWaitMode  - [IN] When stack is empty,
 *                   if aWaitMode = IDU_FXSTACK_POP_WAIT �̸�,
 *                     ���ο� Item�� Push�ɶ����� ���,
 *                   else
 *                     Stack�� ����ٰ� return�Ѵ�.
 *
 * aPopItem   - [IN] Pop�� Item�� ����� �޸� ����
 * aIsEmpty   - [IN] Stack�� ������� ID_TRUE, �ƴϸ� ID_FALSE
 **********************************************************************/
IDE_RC iduFXStack::pop( idvSQL             *aStatSQL,
                        iduFXStackInfo     *aStackInfo,
                        iduFXStackWaitMode  aWaitMode,
                        void               *aPopItem,
                        idBool             *aIsEmpty )
{
    UInt   sCurItemCnt;
    UInt   sItemSize;
    SChar *sPopPos;
    SInt   sState = 0;

    *aIsEmpty   = ID_TRUE;

    IDE_ASSERT( aStackInfo->mMutex.lock( aStatSQL )
                == IDE_SUCCESS );
    sState = 1;

    while(1)
    {
        sCurItemCnt = aStackInfo->mCurItemCnt;

        if( ( sCurItemCnt == 0 ) &&
            ( aWaitMode   == IDU_FXSTACK_POP_WAIT ) )
        {
            /* ���ο� Push�� �߻��Ҷ����� ��ٸ���. */
            IDE_TEST( waitForPush( aStackInfo ) != IDE_SUCCESS );
        }
        else
        {
            break;
        }
    }

    /* ���� Stack�� ����ٰ� Return�Ѵ�. �̶� aWaitMode��
       IDU_FXSTACK_POP_NOWAIT �̾�� �Ѵ�. */
    IDE_TEST_CONT( sCurItemCnt == 0, STACK_EMPTY );

    sItemSize = aStackInfo->mItemSize;

    sCurItemCnt--;
    sPopPos = aStackInfo->mArray + ( sCurItemCnt ) * sItemSize;

    /* Itemũ�Ⱑ vULong ũ���̸� memcpy�� �θ��� �ʰ� assign��Ų��. */
    if( sItemSize == ID_SIZEOF(UInt) )
    {
        *((UInt*)aPopItem) = *((UInt*)sPopPos);
    }
    else
    {
        if( sItemSize == ID_SIZEOF(ULong) )
        {
            *((ULong*)aPopItem) = *((ULong*)sPopPos);
        }
        else
        {
            idlOS::memcpy( aPopItem,
                           sPopPos,
                           sItemSize );
        }
    }

    /* Stack���� Item�� ���������Ƿ� 1 ���� ��Ų��. */
    aStackInfo->mCurItemCnt = sCurItemCnt;

    *aIsEmpty = ID_FALSE;

    IDE_EXCEPTION_CONT( STACK_EMPTY );

    sState = 0;
    IDE_ASSERT( aStackInfo->mMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( aStackInfo->mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}
