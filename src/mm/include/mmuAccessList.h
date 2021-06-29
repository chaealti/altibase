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

#include <idl.h>
#include <idp.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <mmuProperty.h>
#include <iduLatch.h>

#ifndef _O_MMU_ACCESS_LIST_H_
#define _O_MMU_ACCESS_LIST_H_ 1

/* PROJ-2624 [��ɼ�] MM - ������ access_list ������� ���� : 1024�� �ø� */
#define MM_IP_ACL_MAX_COUNT      (1024)
#define MM_IP_ACL_MAX_ADDR_STR   (40)
#define MM_IP_ACL_MAX_MASK_STR   (16)

class mmuAccessList
{
private:
    static iduLatch  mLatch;
    static idBool    mInitialized;

    static idBool mIPACLPermit[MM_IP_ACL_MAX_COUNT]; /* ID_TRUE: Permit, ID_FALSE: Deny */
    /* proj-1538 ipv6 */
    static struct in6_addr mIPACLAddr[MM_IP_ACL_MAX_COUNT];
    static SChar  mIPACLAddrStr[MM_IP_ACL_MAX_COUNT][MM_IP_ACL_MAX_ADDR_STR];
    static UInt   mIPACLAddrFamily[MM_IP_ACL_MAX_COUNT];
    static UInt   mIPACLMask[MM_IP_ACL_MAX_COUNT];
    static UInt   mIPACLCount;

public:
    static IDE_RC initialize();
    static IDE_RC finalize();

    static void   lock();
    static void   lockRead();
    static void   unlock();

    static void   clear();
    static IDE_RC add( idBool            aIPACLPermit,
                       struct in6_addr * aIPACLAddr,
                       SChar           * aIPACLAddrStr,
                       UInt              aIPACLAddrFamily,
                       UInt              aIPACLMask );

    static UInt   getIPACLCount();

    static IDE_RC checkIPACL( struct sockaddr_storage  * aAddr,
                              idBool                   * aAllowed,
                              SChar                   ** aIPACL );

    static IDE_RC loadAccessList();

    static IDE_RC buildAccessListRecord( idvSQL              * aStatistics,
                                         void                * aHeader,
                                         void                * aDumpObj,
                                         iduFixedTableMemory * aMemory );

    static UInt   convertToChar( void   * aBaseObj,
                                 void   * aMember,
                                 UChar  * aBuf,
                                 UInt     aBufSize );
};

inline void mmuAccessList::lock()
{
    if ( mInitialized == ID_TRUE )
    {
        IDE_ASSERT( mLatch.lockWrite( NULL,/* idvSQL* */
                                      NULL ) /* sWeArgs*/
                    == IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }
}

inline void mmuAccessList::lockRead()
{
    if ( mInitialized == ID_TRUE )
    {
        IDE_ASSERT( mLatch.lockRead( NULL,/* idvSQL* */
                                     NULL ) /* sWeArgs*/
                    == IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }
}

inline void mmuAccessList::unlock()
{
    if ( mInitialized == ID_TRUE )
    {
        IDE_ASSERT( mLatch.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* Nothing To Do */
    }
}

inline UInt mmuAccessList::getIPACLCount()
{
    return mIPACLCount;
}

typedef struct mmuAccessListInfo
{
    UInt     mNumber;       // �Է� ����; �ܼ� ������, 1������ ����
    UInt     mOp;           // 0: DENY, 1: PERMIT
    SChar    mAddress[MM_IP_ACL_MAX_ADDR_STR];  // ipv6 �ּ� �ִ� 39��
    SChar    mMask[MM_IP_ACL_MAX_MASK_STR];     // ipv4 mask �ִ� 15��
} mmuAccessListInfo;

#endif
