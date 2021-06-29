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
 
/*
 * -----------------------------------------------------------------------------
 *  ALA_GetODBCCValue() �Լ���
 *      ALA_Value �� �Է����� �޾Ƽ� ����ڰ� aODBCCTypeID �� ������ Ÿ������
 *      Ÿ���� ��ȯ�� �� �� ��� �����͸�
 *      aOutODBCCValueBuffer �� ����Ű�� ���ۿ� ��� �ִ� �Լ��̴�.
 *
 *      �� ��, Ÿ���� ��ȯ��
 *
 *      mt --> cmt --> ulnColumn --> odbc
 *
 *      �� ������ �̷������.
 *
 *      �� �� ul ���� ��ȯ�� uln �� �Լ����� �̿��ؼ� �� �� ������
 *      PROJ-1000 Client C Porting ��� �������� ����� C �� �������� �ʾƼ�
 *      mt --> cmt �� ��ȯ�� ���� mmcSession �� �̿��� �� ���� ��Ȳ�̾���.
 *
 *      �� ���� (ulaConv.c) �� mmcSession �� �����ϴ� mt --> cmt ��
 *      ��ȯ �ڵ带 �׷��� �����ͼ� C �� ������ �ڵ��̴�.
 *
 *      mmcConvFmMT.cpp ���� ����.
 * -----------------------------------------------------------------------------
 */

#include <ulaConvNumeric.h>

ACI_RC ulaConvConvertFromMTToCMT(cmtAny           *aTarget,
                                 void             *aSource,
                                 acp_uint32_t      aSourceType,
                                 acp_uint32_t      aLobSize,
                                 ulaConvByteOrder  aByteOrder);

