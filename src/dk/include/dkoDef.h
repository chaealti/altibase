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
 * $id$
 **********************************************************************/

#ifndef _O_DKO_DEF_H_
#define _O_DKO_DEF_H_ 1


#include <idTypes.h>
#include <idkAtomic.h>

#include <dkDef.h>

#define DKO_LINK_USER_MODE_PUBLIC           (0)
#define DKO_LINK_USER_MODE_PRIVATE          (1)

#define DKO_GET_USER_SESSION_ID( aSession ) ((aSession)->mUserId)

/* Define DK module link object */
typedef struct dkoLink
{
    UInt            mId;        /* �� �����ͺ��̽� ��ũ�� �ĺ���        */
    UInt            mUserId;    /* ��ũ�� ������ user �� id             */
    SInt            mUserMode;  /* ��ũ ��ü�� public/private ��ü����  */
    SInt            mLinkType;  /* heterogeneous/homogeneous link ����  */
    smOID           mLinkOID;   /* SM ���� �ο��ϴ� object id           */
    SChar           mLinkName[DK_NAME_LEN + 1];     /* ��ũ ��ü�� �̸� */
    SChar           mTargetName[DK_NAME_LEN + 1];   /* target ���ݼ���  */
    SChar           mRemoteUserId[DK_NAME_LEN + 1]; /* ���ݼ��� ���� id */
    SChar           mRemoteUserPasswd[DK_NAME_LEN + 1];/* ���� password */
} dkoLink;

#endif /* _O_DKO_DEF_H_ */

