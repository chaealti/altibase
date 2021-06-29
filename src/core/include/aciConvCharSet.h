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
 * $Id: aciConvCharSet.h 11319 2010-06-23 02:39:42Z djin $
 **********************************************************************/

#ifndef _O_ACICONVCHARSET_H_
#define  _O_ACICONVCHARSET_H_  1

#include <acpTypes.h>

ACP_EXTERN_C_BEGIN

#define ACICONV_MAX_CHAR_SET_LEN ((acp_uint32_t)40)

/* ASCII �������� üũ */
#define ACICONV_IS_ASCII(c)       ( (((c) & ~0x7F) == 0) ? ACP_TRUE : ACP_FALSE )

#define ACICONV_IS_UTF16_ASCII_PTR(s) ( ( ( *(acp_uint8_t*)s == 0 ) && ( (*((acp_uint8_t*)s+1) & ~0x7F) == 0 ) ) ? ACP_TRUE : ACP_FALSE )

/* ĳ���� �� ��ȯ ��, ��ȯ�� ���ڰ� ���� ��� ��Ÿ���� ����
 ASCII�� ��� '?'�̴�.
 WE8ISO8859P1�� ��� �Ųٷ� ����ǥ(0xBF)�� default_replace_character�̴�.
 (���� WE8ISO8859P1�� �������� �����Ƿ� ������� �ʴ´�.) */
#define ACICONV_ASCII_DEFAULT_REPLACE_CHARACTER ((acp_uint8_t)(0x3F))

/*  space character ' ' */
#define ACICONV_ASCII_SPACE ((acp_uint8_t)(0x20))

#define ACICONV_IS_UTF16_ALNUM_PTR(s)                                   \
(                                                                   \
  (                                                                 \
    ( *(acp_uint8_t*)s == 0 )                                             \
     &&                                                             \
    (                                                               \
        (((*((acp_uint8_t*)s+1)) >= 'a') && ((*((acp_uint8_t*)s+1)) <= 'z')) || \
        (((*((acp_uint8_t*)s+1)) >= 'A') && ((*((acp_uint8_t*)s+1)) <= 'Z')) || \
        (((*((acp_uint8_t*)s+1)) >= '0') && ((*((acp_uint8_t*)s+1)) <= '9'))    \
    )                                                               \
  ) ? ACP_TRUE : ACP_FALSE                                            \
)


/* PROJ-1579 NCHAR */
/* �����ϴ� ĳ���ͼ��� id�� �����Ѵ�.  */
/* conversion matrix�� ������ �ֱ� ������ 0���� ������� ��ȣ�� �Űܾ� �� */
typedef enum aciConvCharSetList
{
    ACICONV_ASCII_ID = 0,
    ACICONV_KSC5601_ID,
    ACICONV_MS949_ID,
    ACICONV_EUCJP_ID,
    ACICONV_SHIFTJIS_ID,
    ACICONV_MS932_ID, /* PROJ-2590 [��ɼ�] CP932 database character set ���� */
    ACICONV_BIG5_ID,
    ACICONV_GB231280_ID,
    /* PORJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
    ACICONV_MS936_ID,
    ACICONV_UTF8_ID,
    ACICONV_UTF16_ID,
    ACICONV_MAX_CHARSET_ID
} aciConvCharSetList;

typedef enum aciConvCharFeature
{
    ACICONV_CF_UNKNOWN,
    ACICONV_CF_NEG_TRAIL,
    ACICONV_CF_POS_TRAIL,
    ACICONV_CF_SJIS
}aciConvCharFeature;

typedef struct aciConvSummary16 
{
  acp_uint16_t indx; /* index into big table */
  acp_uint16_t used; /* bitmask of used entries */
} aciConvSummary16;

ACP_EXTERN_C_END

#endif /* _O_ACICONVCHARSET_H_ */
 
