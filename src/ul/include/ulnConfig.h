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

#ifndef _O_ULN_CONFIG_H_
#define _O_ULN_CONFIG_H_ 1

/**
 * ULN_USE_GETDATA_EXTENSIONS.
 *
 * �� ����� GetData �� ���ؼ� �������� ���� �� ������ �� ����ڰ� ���ε����� ����
 * �÷��� �����͸� IRD record �� ĳ���ϴ� �κа� ���õ� �ڵ带 Ȱ��ȭ��Ű������ ���θ�
 * �����ϴ� ����̴�.
 *
 * �׷���, SQLGetData() �� ODBC ���忡�� ����� ��������� �״�� �α�� �����Ǿ���,
 * �׿� ���� fetch protocol �� �ٲ������, �Ʒ��� �ڵ尡 �ʿ� ���� �Ǿ���.
 *
 * ������, SQLGetInfo() ���� SQL_GETDATA_EXTENSIONS �� ���� ������ ����ڰ� ����� ��
 * ������ ��� �ɼ��� �����ϱ� ���ؼ��� ��� ����� ĳ�� ��ƾ�� �ʿ��ϴ�.
 * �׶��� ����ؼ� �ڵ带 ���ܵд�.
 *
 * ����(2006. 09. 26��)�� �Ʒ��� ULN_USE_GETDATA_EXTENSIONS  �� 0 ���� ������ �����ν�
 * caching �� ���õ� �ڵ尡 �ϳ��� �����ϵ��� �ʵ��� ��ġ�� �ξ���.
 *
 * ���� ����(2006.09.26) ���� �ڵ���� out ��Ű�� ������ �ڵ���� ����������,
 * �ϼ������� ���� �ڵ���̴�. ���丸 �ľ��ؼ� ���� �ϵ��� �Ѵ�.
 *
 * �ʿ䰡 ���� �� ������ Ǯ�� �ڵ� ������ ������� �ϸ� �ǰڴ�.
 */
#define ULN_USE_GETDATA_EXTENSIONS 0

/*
 * DiagHeader �� ������ ûũ Ǯ�� ���� ũ�� �� SP ����
 */
#define ULN_MAX_DIAG_MSG_LEN                ACI_MAX_ERROR_MSG_LEN
#define ULN_SIZE_OF_CHUNK_IN_DIAGHEADER     (ACI_SIZEOF(ulnDiagRec) + ULN_MAX_DIAG_MSG_LEN + 1024)
#define ULN_NUMBER_OF_SP_IN_DIAGHEADER      (10)

/*
 * ENV�� ûũ Ǯ�� ���� ũ�� �� SP �� ����
 */
#define ULN_SIZE_OF_CHUNK_IN_ENV            (4 * 1024)
#define ULN_NUMBER_OF_SP_IN_ENV             20

/*
 * DBC�� ûũ Ǯ�� ���� ũ�� �� SP ����
 */
#define ULN_SIZE_OF_CHUNK_IN_DBC            (32 * 1024)
#define ULN_NUMBER_OF_SP_IN_DBC             20

/*
 * Cache �� ûũǮ�� ����ũ�� �� SP ����
 */
#define ULN_SIZE_OF_CHUNK_IN_CACHE          (64 * 1024)
#define ULN_NUMBER_OF_SP_IN_CACHE           4

#endif /* _O_ULN_CONFIG_H_ */
