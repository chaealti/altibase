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

#ifndef _O_CMP_DEF_BASE_H_
#define _O_CMP_DEF_BASE_H_ 1


/*
 * �������� ���������� ����� �߰��� ���� ����ó�� �߰��Ѵ�.
 *
 *    enum {
 *        CM_ID_NONE = 0,
 *        ...
 *        CM_ID_MAX_VER1
 *    };  <-- �������� ����1�� ���� ����
 *
 *    enum {
 *        CM_ID_NEW = CM_ID_MAX_VER1,
 *        ...
 *        CM_ID_MAX_VER2
 *    };  <-- �������� ����2�� ���ο� ��� ����
 *
 *    #define CM_ID_MAX CM_ID_MAX_VER2  <-- ������ �������� ������ MAX�� ����
 */


/*
 * Protocol Version
 */

enum
{
    CMP_VER_BASE_NONE = 0,
    CMP_VER_BASE_V1,
    CMP_VER_BASE_MAX
};


/*
 * Operation ID
 */

enum
{
    CMP_OP_BASE_Error = 0,
    CMP_OP_BASE_Handshake,
    CMP_OP_BASE_MAX_VER1
};

#define CMP_OP_BASE_MAX CMP_OP_BASE_MAX_VER1


#endif
