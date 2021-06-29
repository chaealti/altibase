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
 * $Id: aciConvGbk.c 571 2014-07-17 04:54:19Z myungsub.shin $
 **********************************************************************/

#include <aciErrorMgr.h>
#include <aciConvGbk.h>
#include <aciConvGb2312.h>
#include <aciConvCp936ext.h>
#include <aciConvGbkext1.h>
#include <aciConvGbkext2.h>
#include <aciConvGbkextinv.h>

ACP_EXPORT acp_sint32_t aciConvConvertMbToWc4Gbk( void         * aSrc,
                                                  acp_sint32_t   aSrcRemain,
                                                  acp_sint32_t * aSrcAdvance,
                                                  void         * aDest,
                                                  acp_sint32_t   aDestRemain )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
 *     GBK ==> UTF16BE
 *
 * Implementation :
 *     1) ������� GB2312 �� ������ ���ڸ� ���� ��ȯ.
 *     2) �׿��� ���ڴ� ������� GB2312 �� ����.
 *     3) User_Defined_Area �� ��� CP936EXT �� ����.
 *     4) ��ȯ�� �����ϸ�, GBK �� ��ȯ����� ����.
 *
 ***********************************************************************/

    acp_uint8_t  * sSrcCharPtr = (acp_uint8_t *)aSrc;
    acp_sint32_t   sRet;
    acp_uint16_t   sWc;

    aDestRemain = 0;

    if ( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] < 0xff ) )
    {
        if ( aSrcRemain < 2 )
        {
            *aSrcAdvance = aSrcRemain;
            sRet = ACICONV_RET_TOOFEW;
        }
        else
        {
            if ( ( sSrcCharPtr[0] >= 0xa1 ) && ( sSrcCharPtr[0] <= 0xf7 ) )
            {
                /* 1) ������� GB2312 �� ������ ���ڸ� ���� ��ȯ
                 *
                 * GB2312 �� GBK ���� ������ ���ڿ� ������ �����ڵ� ���� ���ϴ�
                 * ���ڰ� ������, �׹��ڸ� ó���ϱ� ���� �б⹮�̴�. �Ʒ��� �ش�
                 * �ϴ� ���� ���� �����ڵ� ���� ���Ͽ���.
                 *
                 * Code      GB2312    GBK
                 * 0xA1A4    U+30FB    U+00B7    MIDDLE DOT
                 * 0xA1AA    U+2015    U+2014    EM DASH
                 *
                 * ����, �Ʒ��� ����ó���� ���� ���ڸ� GB2312 ���� �����ϱ� ��
                 * ��, GBK �� �����ڵ� ������ ��ȯ�ϴ� �۾��̴�.
                 */
                if ( sSrcCharPtr[0] == 0xa1 )
                {
                    if ( sSrcCharPtr[1] == 0xa4 )
                    {
                        /* �� �����ڵ�� UTF16BE �� ������, ���Ŀ� UTF16LE �� ��
                         * ���Ǹ� ���� �����ؾ� �Ѵ�.
                         */
                        sWc = 0x00b7;
                        ACICONV_WC_TO_UTF16BE( aDest, sWc );

                        *aSrcAdvance = 2;
                        sRet = 2;
                    }
                    else if ( sSrcCharPtr[1] == 0xaa )
                    {
                        /* ���Ŀ� UTF16LE �� �߰��Ǹ� ���� �����ؾ� �Ѵ�. */
                        sWc = 0x2014;
                        ACICONV_WC_TO_UTF16BE( aDest, sWc );

                        *aSrcAdvance = 2;
                        sRet = 2;
                    }
                    else
                    {
                        /* ���� ó�� ������ ���Ե��� �ʴ� �����̴�. GB2312 �� ó
                         * ���ؾ� �Ѵ�. ����, ��ȯ���� ���� ���� �˸��� ���� sRe
                         * t ���� �����Ѵ�.
                         */
                        *aSrcAdvance = 2;
                        sRet = ACICONV_RET_ILSEQ;
                    }
                }
                else
                {
                    /* ��ȯ���� ���� ���� �˸��� ���� sRet ���� �����Ѵ�. */
                    *aSrcAdvance = 2;
                    sRet = ACICONV_RET_ILSEQ;
                }

                if ( sRet != ACICONV_RET_ILSEQ )
                {
                    ACI_RAISE( NORMAL_EXIT );
                }
                else
                {
                    /* Nothing to do */
                }

                /* 2) �׿��� ���ڴ� ������� GB2312 �� ���� */
                if ( ( sSrcCharPtr[1] >= 0xa1 ) &&
                     ( sSrcCharPtr[1] < 0xff ) )
                {
                    sRet = aciConvConvertMbToWc4Gb2312( aSrc,
                                                        aSrcRemain,
                                                        aSrcAdvance,
                                                        aDest,
                                                        aDestRemain );

                    if ( sRet != ACICONV_RET_ILSEQ )
                    {
                        ACI_RAISE( NORMAL_EXIT );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    /* 3) CP936EXT �� ����
                     *
                     * GB2312 ���� ��ȯ���� �ʴ� User_Defined_Area�� ���ڰ� ����
                     * �Ѵ�. �̷��� ���ڴ� GB2312 ������ ������, ��ȯ �ڵ�������
                     * ( �ڵ尪 �迭 )�� ����. ����, �ڵ��������� ���ǵ� CP936
                     * EXT �� �����Ѵ�.
                     */
                    sRet = aciConvConvertMbToWc4Cp936ext( aSrc,
                                                          aSrcRemain,
                                                          aSrcAdvance,
                                                          aDest,
                                                          aDestRemain );
                }
                else
                {
                    *aSrcAdvance = 2;
                    sRet = ACICONV_RET_ILSEQ;
                }
            }
            else
            {
                /* 4) GBK �� ��ȯ����� ���� */
                if ( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] <= 0xa0 ) )
                {
                    sRet = aciConvConvertMbToWc4Gbkext1( aSrc,
                                                         aSrcRemain,
                                                         aSrcAdvance,
                                                         aDest,
                                                         aDestRemain );
                }
                else
                {
                    *aSrcAdvance = 2;
                    sRet = ACICONV_RET_ILSEQ;
                }
            }

            if ( sRet != ACICONV_RET_ILSEQ )
            {
                ACI_RAISE( NORMAL_EXIT );
            }
            else
            {
                /* Nothing to do */
            }

            /* 4) GBK �� ��ȯ����� ���� */
            if ( ( sSrcCharPtr[0] >= 0xa8 ) &&
                 ( sSrcCharPtr[0] <= 0xfe ) )
            {
                sRet = aciConvConvertMbToWc4Gbkext2( aSrc,
                                                     aSrcRemain,
                                                     aSrcAdvance,
                                                     aDest,
                                                     aDestRemain );
            }
            else
            {
                if ( sSrcCharPtr[0] == 0xa2 )
                {
                    if ( ( sSrcCharPtr[1] >= 0xa1 ) &&
                         ( sSrcCharPtr[1] <= 0xaa ) )
                    {
                        sWc = 0x2170 + ( sSrcCharPtr[1] - 0xa1 );
                        ACICONV_WC_TO_UTF16BE( aDest, sWc );

                        *aSrcAdvance = 2;
                        sRet = 2;
                    }
                    else
                    {
                        *aSrcAdvance = 2;
                        sRet = ACICONV_RET_ILSEQ;
                    }
                }
                else
                {
                    *aSrcAdvance = 2;
                    sRet = ACICONV_RET_ILSEQ;
                }
            }
        }
    }
    else
    {
        *aSrcAdvance = 1;
        sRet = ACICONV_RET_ILSEQ;
    }

    ACI_EXCEPTION_CONT( NORMAL_EXIT );

    return sRet;
}

ACP_EXPORT acp_sint32_t aciConvConvertWcToMb4Gbk( void         * aSrc,
                                                  acp_sint32_t   aSrcRemain,
                                                  acp_sint32_t * aSrcAdvance,
                                                  void         * aDest,
                                                  acp_sint32_t   aDestRemain )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
 *     UTF16BE ==> GBK
 *
 * Implementation :
 *     1) GB2312 �� ����.
 *     2) GBK �� ��ȯ����� ����.
 *
 ***********************************************************************/

    acp_uint8_t  * sDestCharPtr = (acp_uint8_t *)aDest;
    acp_sint32_t   sRet;
    acp_uint16_t   sWc;

    aSrcRemain = 0;

    ACICONV_UTF16BE_TO_WC( sWc, aSrc );

    if ( aDestRemain < 2 )
    {
        *aSrcAdvance = 2;
        sRet = ACICONV_RET_TOOSMALL;
    }
    else
    {
        /* 1) GB2312 �� ���� */
        if ( ( sWc != 0x30fb ) && ( sWc != 0x2015 ) )
        {
            sRet = aciConvConvertWcToMb4Gb2312( aSrc,
                                                aSrcRemain,
                                                aSrcAdvance,
                                                aDest,
                                                aDestRemain );
        }
        else
        {
            /* ��ȯ���� ���� ���� �˸��� ���� sRet ���� �����Ѵ�. */
            *aSrcAdvance = 2;
            sRet = ACICONV_RET_ILUNI;
        }

        if ( sRet != ACICONV_RET_ILUNI )
        {
            ACI_RAISE( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }

        /* 2) GBK �� ��ȯ����� ���� */
        sRet = aciConvConvertWcToMb4Gbkextinv(aSrc,
                                              aSrcRemain,
                                              aSrcAdvance,
                                              aDest,
                                              aDestRemain );

        if ( sRet != ACICONV_RET_ILUNI )
        {
            ACI_RAISE( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sWc >= 0x2170 ) && ( sWc <= 0x2179 ) )
        {
            sDestCharPtr[0] = 0xa2;
            sDestCharPtr[1] = 0xa1 + ( sWc - 0x2170 );

            *aSrcAdvance = 2;
            sRet = 2;
        }
        else
        {
            sRet = aciConvConvertWcToMb4Cp936ext( aSrc,
                                                  aSrcRemain,
                                                  aSrcAdvance,
                                                  aDest,
                                                  aDestRemain );

            if ( sRet != ACICONV_RET_ILUNI )
            {
                ACI_RAISE( NORMAL_EXIT );
            }
            else
            {
                /* Nothing to do */
            }

            /* ������� GB2312 �� ������ ������ ����ó���̴�. */
            if ( sWc == 0x00b7 )
            {
                sDestCharPtr[0] = 0xa1;
                sDestCharPtr[1] = 0xa4;

                *aSrcAdvance = 2;
                sRet = 2;
            }
            else
            {
                if ( sWc == 0x2014 )
                {
                    sDestCharPtr[0] = 0xa1;
                    sDestCharPtr[1] = 0xaa;

                    *aSrcAdvance = 2;
                    sRet = 2;
                }
                else
                {
                    *aSrcAdvance = 2;
                    sRet = ACICONV_RET_ILUNI;
                }
            }
        }
    }

    ACI_EXCEPTION_CONT( NORMAL_EXIT );

    return sRet;
}
/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
 *
 *  - GB18030 �� �߰��Ǿ��� ���� ����� �� �ִ� ���� �Լ��̴�.
 *  - ���翡 ������� �����Ƿ� �ּ�ó���� �Ѵ�. ( 2014-07-15 )
 *
 * ACP_EXPORT acp_sint32_t aciConvCopyGbk( void         * aSrc,
 *                                         acp_sint32_t   aSrcRemain,
 *                                         acp_sint32_t * aSrcAdvance,
 *                                         void         * aDest,
 *                                         acp_sint32_t   aDestRemain )
 * {
 */
/***********************************************************************
 *
 * Description :
 *     PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
 *     ex) GBK ==> CP936
 *             ==> GB18030
 *
 * Implementation :
 *     ������ ĳ���� �� �����Լ��� ���� �� ������ �����ϴ�.
 *
 ***********************************************************************/
/*
 *     acp_uint8_t  * sSrcCharPtr;
 *     acp_uint8_t  * sDestCharPtr;
 *     acp_sint32_t   sRet;
 * 
 *     sSrcCharPtr = (acp_uint8_t *)aSrc;
 *     sDestCharPtr = (acp_uint8_t *)aDest;
 * 
 *     if ( sSrcCharPtr[0] < 0x80 )
 *     {
 *         *sDestCharPtr = *sSrcCharPtr;
 * 
 *         *aSrcAdvance = 1;
 *         sRet = 1;
 *     }
 *     else
 *     {
 *         if( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] < 0xff ) )
 *         {
 *             if ( aSrcRemain < 2 )
 *             {
 *                 *aSrcAdvance = aSrcRemain;
 *                 sRet = ACICONV_RET_TOOFEW;
 *             }
 *             else
 *             {
 *                 if ( aDestRemain < 2 )
 *                 {
 *                     *aSrcAdvance = 2;
 *                     sRet = ACICONV_RET_TOOSMALL;
 *                 }
 *                 else
 *                 {
 *                     sDestCharPtr[0] = sSrcCharPtr[0];
 *                     sDestCharPtr[1] = sSrcCharPtr[1];
 * 
 *                     *aSrcAdvance = 2;
 *                     sRet = 2;
 *                 }
 *             }
 *         }
 *         else
 *         {
 *             *aSrcAdvance = 1;
 *             sRet = ACICONV_RET_ILSEQ;
 *         }
 *     }
 * 
 *     return sRet;
 * }
 */
