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
 * $Id: idnCharSet.h 80575 2017-07-21 07:06:35Z yoonhee.kim $
 **********************************************************************/

#ifndef _O_IDNCHARSET_H_
#define  _O_IDNCHARSET_H_  1

#define IDN_MAX_CHAR_SET_LEN ((UInt)40)

// ASCII �������� üũ
#define IDN_IS_ASCII(c)       ( (((c) & ~0x7F) == 0) ? ID_TRUE : ID_FALSE )

#define IDN_IS_UTF16_ASCII_PTR(s) ( ( ( *(UChar*)s == 0 ) && ( (*((UChar*)s+1) & ~0x7F) == 0 ) ) ? ID_TRUE : ID_FALSE )

// ĳ���� �� ��ȯ ��, ��ȯ�� ���ڰ� ���� ��� ��Ÿ���� ����
// ASCII�� ��� '?'�̴�.
// WE8ISO8859P1�� ��� �Ųٷ� ����ǥ(0xBF)�� default_replace_character�̴�.
// (���� WE8ISO8859P1�� �������� �����Ƿ� ������� �ʴ´�.)
#define IDN_ASCII_DEFAULT_REPLACE_CHARACTER ((UChar)(0x3F))

// space character ' '
#define IDN_ASCII_SPACE ((UChar)(0x20))

#define IDN_IS_UTF16_ALNUM_PTR(s)                                   \
(                                                                   \
  (                                                                 \
    ( *(UChar*)s == 0 )                                             \
     &&                                                             \
    (                                                               \
        (((*((UChar*)s+1)) >= 'a') && ((*((UChar*)s+1)) <= 'z')) || \
        (((*((UChar*)s+1)) >= 'A') && ((*((UChar*)s+1)) <= 'Z')) || \
        (((*((UChar*)s+1)) >= '0') && ((*((UChar*)s+1)) <= '9'))    \
    )                                                               \
  ) ? ID_TRUE : ID_FALSE                                            \
)


// PROJ-1579 NCHAR
// �����ϴ� ĳ���ͼ��� id�� �����Ѵ�. 
// conversion matrix�� ������ �ֱ� ������ 0���� ������� ��ȣ�� �Űܾ� ��
typedef enum idnCharSetList
{
    IDN_ASCII_ID = 0,
    IDN_KSC5601_ID,
    IDN_MS949_ID,
    IDN_EUCJP_ID,
    IDN_SHIFTJIS_ID,
    IDN_MS932_ID,   /* PROJ-2590 [��ɼ�] CP932 database character set ���� */
    IDN_BIG5_ID,
    IDN_GB231280_ID,
    /* PORJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
    IDN_MS936_ID,
    IDN_UTF8_ID,
    IDN_UTF16_ID,
    IDN_MAX_CHARSET_ID
} idnCharSetList;

typedef enum idnCharFeature
{
    IDN_CF_UNKNOWN,
    IDN_CF_NEG_TRAIL,
    IDN_CF_POS_TRAIL,
    IDN_CF_SJIS
}idnCharFeature;

typedef struct Summary16 
{
  UShort indx; // index into big table
  UShort used; // bitmask of used entries
} Summary16;

#endif /* _O_IDNCHARSET_H_ */
 
