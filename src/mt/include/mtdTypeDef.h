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
 * $Id: mtdTypeDef.h 19015 2006-11-19 23:43:45Z sungminee $
 *
 * ul���� �����ϹǷ� ����� �����Ͽ��� ��.
 *
 **********************************************************************/

#ifndef _O_MTD_TYPE_DEF_H_
#define _O_MTD_TYPE_DEF_H_  1

// PROJ-1364
// mtdModule.id
#define MTD_MAX_COUNT       36 // ��ü type�� ����

// BUG-37903
// data type �߰� �� qcp/qcpll.l�� TL_TYPED_LETERAL��
// ������Ÿ�Ը��� �߰��ؾ� �Ѵ�.

// Type id �� mtdModuleById �迭�� ���� ��,
// 0x3F �� ����ŷ �� ���� �迭�� index �� ����Ѵ�.
// ���� type id �� ���� �߰��Ϸ���
// ���� id �� 0x3F �� ����ŷ �� ���� ��ġ�� �ʵ��� �ؾ� �Ѵ�.
// (id �� ����ŷ�� ���� ���� �ٸ� ��� �ּ��� []�� ���� ǥ���Ͽ���.)
                                                        // [ID & 0x3F]
#define MTD_BIGINT_ID       (UInt)-5                    // [59] 1��
#define MTD_BINARY_ID       (UInt)-2                    // [62]
#define MTD_BLOB_ID         30                          //
#define MTD_BLOB_LOCATOR_ID 31    // PROJ-1362          //
#define MTD_CLOB_ID         40                          //      5��
#define MTD_CLOB_LOCATOR_ID 41    // PROJ-1362          //
#define MTD_BOOLEAN_ID      16                          //
#define MTD_BIT_ID          (UInt)-7                    // [57]
#define MTD_VARBIT_ID       (UInt)-100  // VARBIT�� ODBC�� ���ǵǾ� ���� �ʴ�. [28]
#define MTD_CHAR_ID         1                           //      10��
#define MTD_DATE_ID         9                           //
#define MTD_DOUBLE_ID       8                           //
#define MTD_FLOAT_ID        6                           //
#define MTD_BYTE_ID         20001                       // [33]
#define MTD_NIBBLE_ID       20002                       // [34] 15��
#define MTD_VARBYTE_ID      20003                       // [35]
#define MTD_INTEGER_ID      4                           //
#define MTD_INTERVAL_ID     10                          //
#define MTD_LIST_ID         10001                       // [17]
#define MTD_NULL_ID         0                           //
#define MTD_NUMBER_ID       10002                       // [42] 20��
#define MTD_NUMERIC_ID      2                           //
#define MTD_REAL_ID         7                           //
#define MTD_SMALLINT_ID     5                           //
#define MTD_VARCHAR_ID      12                          //
#define MTD_NONE_ID         (UInt)-999                  // [25] 25��
#define MTS_FILETYPE_ID     30001                       // [49]
#define MTS_CONNECT_TYPE_ID 30002
#define MTD_GEOMETRY_ID     10003                       // [19]
#define MTD_NCHAR_ID        (UInt)-8   // PROJ-1579     // [56]
#define MTD_NVARCHAR_ID     (UInt)-9   // PROJ-1579     // [55]
#define MTD_ECHAR_ID        60         // PROJ-2002     //      30��
#define MTD_EVARCHAR_ID     61         // PROJ-2002     //
#define MTD_UNDEF_ID        90         // PROJ-2163     // [26]

// PROJ-1075 ����� ���� Ÿ�� �Ǵ� rowtype id
#define MTD_UDT_ID_MIN           (1000001) // type�� udt���� �񱳿�
#define MTD_ROWTYPE_ID           (1000001)
#define MTD_RECORDTYPE_ID        (1000002)
#define MTD_ASSOCIATIVE_ARRAY_ID (1000003)
#define MTD_REF_CURSOR_ID        (1000004)
#define MTD_UDT_ID_MAX           (1000004) // type�� udt���� �񱳿�
// PROJ-1075
// User-defined type�� mtdModule�� no �� ����.
// ���� pre-defined�� ���� ����Ѵ�.
#define MTD_UDT_MODULE_NO     (999)

#endif /* _O_MTD_TYPE_DEF_H_ */
 
