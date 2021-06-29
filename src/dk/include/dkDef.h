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

#ifndef _O_DK_DEF_H_
#define _O_DK_DEF_H_ 1

#include <idu.h>
#include <qci.h>
#include <dkErrorCode.h>


/* DB module enable/disable from 'DBLINK_ENABLE' altibase property */
#define DK_DISABLE                      (0)
#define DK_ENABLE                       (1)

#define DK_UNUSED(aVar) (void)(aVar)

/* ���� ���ڿ��� ���� ����. DK_NAME_LEN�� QP���� ���� ���� ����Ѵ�. */
#define DK_NAME_LEN                     QCI_MAX_NAME_LEN    /* 2012.10.05 ���� 40 */
#define DK_PATH_LEN                     (1024)
#define DK_DRIVER_CLASS_NAME_LEN        (1024)
#define DK_MSG_LEN                      (2048)
#define DK_URL_LEN                      DK_PATH_LEN
#define DK_USER_ID_LEN                  DK_NAME_LEN
#define DK_USER_PW_LEN                  DK_NAME_LEN
#define DK_TIME_STR_LEN                 (128)

/* Altibase �������� AltiLinker ���μ����� �ѹ��� �����ϴ� packet �� �ִ�ũ�� */
#define DK_MAX_PACKET_LEN               (32)*(1024) /* BUG-36837 */

/* Altibase �������� AltiLinker ���μ����� ���ް����� SQL text �� �ִ���� */
#define DK_MAX_SQLTEXT_LEN              (32000)

#define DK_RC_SUCCESS                   (0)
#define DK_RC_FAILURE                   (1)
#define DK_RC_END_OF_FETCH              (2)

#define DK_MAX_TARGET_COUNT             (128)
#define DK_MAX_TARGET_SUB_ITEM_COUNT    (8)

/* ����� Column ������ 1024 ������ ���ѵǾ� �ִ�. 
   ���� �� ��������� �ٲ�� �Ǹ� �׿� ���� �� ������� �������־�� �Ѵ�. */
#define DK_MAX_COL_CNT                  (1024)

/* Trace log directory name */ 
#define DK_TRC_DIR                      "trc"

/* Trace log */
#define DK_TRC_LOG_FORCE          IDE_DK_0
#define DK_TRC_LOG_ERROR          IDE_DK_1

/* ���� IP ���ڿ� */
#define DK_LOCAL_IP                     ((SChar *)"127.0.0.1")

#define DK_INVALID_USER_ID              (ID_UINT_MAX)
#define DK_INVALID_LINK_ID              (ID_UINT_MAX)
#define DK_INVALID_STMT_ID              (ID_SLONG_MAX)
#define DK_INVALID_STMT_TYPE            (ID_UINT_MAX)
#define DK_INVALID_REMOTE_SESSION_ID    (ID_USHORT_MAX)

#define DK_FETCH_END                    (DK_MAX_COL_CNT + 1)

#define DK_MAX_NUMERIC_DISPLAY_LEN      (136)

#define DK_TX_RESULT_STR_SIZE           (10)

#define DK_INIT_GTX_ID                  (ID_UINT_MAX)
#define DK_INIT_RTX_ID                  (ID_UINT_MAX)
#define DK_INIT_LTX_ID                  (ID_UINT_MAX)
#define DK_INIT_REMOTE_SESSION_ID       (ID_USHORT_MAX)

#endif /* _O_DK_DEF_H_ */
