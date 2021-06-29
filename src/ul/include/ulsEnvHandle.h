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
 *
 * Spatio-Temporal Environment Handle
 *
 * TODO - Error ���� ü�踦 �����ؾ� ��.
 *
 ***********************************************************************/

#ifndef _O_ULS_ENV_HANDLE_H_
#define _O_ULS_ENV_HANDLE_H_    (1)

#include <acp.h>
#include <aciTypes.h>
#include <aciErrorMgr.h>

#include <ulsTypes.h>
#include <ulsError.h>

#define ULS_HANDLE_INITIALIZED    (20061130)
#define ULS_HANDLE_INVALID        (0)

/* Environment Handle*/
typedef struct ulsHandle
{
    acp_uint32_t       mInit;      /* Init Value*/
    ulsErrorMgr        mErrorMgr;  /* Error Manager*/
} ulsHandle;

/*----------------------------------------------------------------*
 *  External Interfaces
 *----------------------------------------------------------------*/

/* Env Handle ���� �Ҵ� �� �ʱ�ȭ */
ACSRETURN ulsAllocEnv( ulsHandle ** aHandle );

/* Env Handle ���� ����*/
ACSRETURN ulsFreeEnv( ulsHandle * aHandle );

/* Error ���� ȹ�� */
ACSRETURN ulsGetError( ulsHandle    * aHandle,
                       acp_uint32_t * aErrorCode,
                       acp_char_t  ** aErrorMessage,
                       acp_sint16_t * aErrorMessageLength );

/*----------------------------------------------------------------*
 *  Internal Interfaces
 *----------------------------------------------------------------*/

/* Env Handle �ʱ�ȭ*/
ACI_RC  ulsInitEnv( ulsHandle * aHandle );

/* Env Handle �˻� */
ACI_RC  ulsCheckEnv( ulsHandle * aHandle );

/* Error Code ����*/
void ulsSetErrorCode( ulsHandle * aHandle, acp_uint32_t aErrorCode, ...);

#endif /* _O_ULS_ENV_HANDLE_H_ */


