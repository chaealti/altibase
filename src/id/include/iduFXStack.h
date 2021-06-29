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

#ifndef _O_IDU_FXSTACK_H_
#define _O_IDU_FXSTACK_H_ 1

#include <idl.h>
#include <ide.h>
#include <iduCond.h>
#include <iduMutex.h>

typedef struct iduFXStackInfo
{
    UInt       mMaxItemCnt;  // �ִ� ������ ������ �ִ� Item�� ����
    UInt       mCurItemCnt;  // ���� ������ ������ �ִ� Item�� ����
    UInt       mItemSize;    // Item�� ũ��(Byte)

    UInt       mAlign;       // Align�� ���ؼ� �߰�.

    iduMutex        mMutex;  // ���ü� ��� ����.
    iduCond         mCV;     // pop�� ����Ҷ� ���.
    UInt            mWaiterCnt;

    SChar      mArray[1];       // Item�� ������ �ִ� Stack Array
} iduFXStackInfo;

/* Stack�� ���ؼ� Pop���� ����� Stack�� ��������� ���ο� Item��
   Push�ɶ����� ���� ��⸦ ���� �ٷ� ����ٰ� ���������� ����  */
typedef enum iduFXStackWaitMode
{
    IDU_FXSTACK_POP_WAIT = 0, /* ���Ѵ�� */
    IDU_FXSTACK_POP_NOWAIT    /* �ٷθ��� */
} iduFXStackWaitMode;

class iduFXStack
{
public:
    static IDE_RC initialize( SChar          *aMtxName,
                              iduFXStackInfo *aStackInfo,
                              UInt            aMaxItemCnt,
                              UInt            aItemSize );

    static IDE_RC destroy( iduFXStackInfo *aStackInfo );

    static IDE_RC push( idvSQL         *aStatSQL,
                        iduFXStackInfo *aStackInfo,
                        void           *aItem );

    static IDE_RC pop( idvSQL             *aStatSQL,
                       iduFXStackInfo     *aStackInfo,
                       iduFXStackWaitMode  aWaitMode,
                       void               *aPopItem,
                       idBool             *aIsEmpty );

    static inline idBool isEmpty( iduFXStackInfo *aStackInfo );

    static inline IDE_RC waitForPush( iduFXStackInfo *aStackInfo );
    static inline IDE_RC getupWaiters( iduFXStackInfo *aStackInfo );

    static inline SInt getItemCnt( iduFXStackInfo *aStackInfo ) { return aStackInfo->mCurItemCnt;};
};

/* ������ ��������� ���� */
inline idBool iduFXStack::isEmpty( iduFXStackInfo *aStackInfo )
{
    return ( aStackInfo->mCurItemCnt == 0 ? ID_TRUE : ID_FALSE ) ;
}

/* ���ÿ� ���ο� Item�� Push�ɶ����� ��� */
inline IDE_RC iduFXStack::waitForPush( iduFXStackInfo *aStackInfo )
{
    IDE_RC rc;

    aStackInfo->mWaiterCnt++;

    rc = aStackInfo->mCV.wait(&(aStackInfo->mMutex));

    aStackInfo->mWaiterCnt--;

    IDE_TEST_RAISE( rc != IDE_SUCCESS, err_cond_wait );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_wait );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_ThrCondWait ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Stack�� Item�� Push�Ǳ⸦ ����ϴ� Waiter�� �����. */
inline IDE_RC iduFXStack::getupWaiters( iduFXStackInfo *aStackInfo )
{
    if( aStackInfo->mWaiterCnt > 0 )
    {
        IDE_TEST_RAISE( aStackInfo->mCV.signal() != IDE_SUCCESS,
                        err_cond_signal );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_signal );
    {
        IDE_SET( ideSetErrorCode( idERR_FATAL_ThrCondSignal ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#endif /* _O_IDU_FXSTACK_H_ */
