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
 * $Id: idnGbk.cpp 66009 2014-07-16 06:55:19Z myungsub.shin $
 **********************************************************************/

#include <ide.h>
#include <idnGbk.h>
#include <idnGb2312.h>
#include <idnGbkext1.h>
#include <idnGbkext2.h>
#include <idnGbkextinv.h>
#include <idnCp936ext.h>

SInt convertMbToWc4Gbk( void    * aSrc,
                        SInt      aSrcRemain,
                        void    * aDest,
                        SInt      aDestRemain )
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

    UChar   * sSrcCharPtr;
    UShort    sWc;
    SInt      sRet;

    sSrcCharPtr = (UChar *)aSrc;

    if ( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] < 0xff ) )
    {
        if ( aSrcRemain < 2 )
        {
            sRet = RET_TOOFEW;
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
                        WC_TO_UTF16BE( aDest, sWc );

                        sRet = 2;
                    }
                    else if ( sSrcCharPtr[1] == 0xaa )
                    {
                        /* ���Ŀ� UTF16LE �� �߰��Ǹ� ���� �����ؾ� �Ѵ�. */
                        sWc = 0x2014;
                        WC_TO_UTF16BE( aDest, sWc );

                        sRet = 2;
                    }
                    else
                    {
                        /* ���� ó�� ������ ���Ե��� �ʴ� �����̴�. GB2312 �� ó
                         * ���ؾ� �Ѵ�. ����, ��ȯ���� ���� ���� �˸��� ���� sRe
                         * t ���� �����Ѵ�.
                         */
                        sRet = RET_ILSEQ;
                    }
                }
                else
                {
                    /* ��ȯ���� ���� ���� �˸��� ���� sRet ���� �����Ѵ�. */
                    sRet = RET_ILSEQ;
                }

                if ( sRet != RET_ILSEQ )
                {
                    IDE_CONT( NORMAL_EXIT );
                }
                else
                {
                    /* Nothing to do. */
                }

                /* 2) �׿��� ���ڴ� ������� GB2312 �� ���� */
                if ( ( sSrcCharPtr[1] >= 0xa1 ) &&
                     ( sSrcCharPtr[1] < 0xff ) )
                {
                    sRet = convertMbToWc4Gb2312( aSrc,
                                                 aSrcRemain,
                                                 aDest,
                                                 aDestRemain );

                    if ( sRet != RET_ILSEQ )
                    {
                        IDE_CONT( NORMAL_EXIT );
                    }
                    else
                    {
                        /* Nothing to do. */
                    }

                    /* 3) CP936EXT �� ����
                     *
                     * GB2312 ���� ��ȯ���� �ʴ� User_Defined_Area�� ���ڰ� ����
                     * �Ѵ�. �̷��� ���ڴ� GB2312 ������ ������, ��ȯ �ڵ�������
                     * ( �ڵ尪 �迭 )�� ����. ����, �ڵ��������� ���ǵ� CP936
                     * EXT �� �����Ѵ�.
                     */
                    sRet = convertMbToWc4Cp936ext( aSrc,
                                                   aSrcRemain,
                                                   aDest,
                                                   aDestRemain );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* 4) GBK �� ��ȯ����� ���� */
                if ( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] <= 0xa0 ) )
                {
                    sRet = convertMbToWc4Gbkext1( aSrc,
                                                  aSrcRemain,
                                                  aDest,
                                                  aDestRemain );
                }
                else
                {
                    sRet = RET_ILSEQ;
                }
            }

            if ( sRet != RET_ILSEQ )
            {
                IDE_CONT( NORMAL_EXIT );
            }
            else
            {
                /* Nothing to do. */
            }

            /* 4) GBK �� ��ȯ����� ���� */
            if ( ( sSrcCharPtr[0] >= 0xa8 ) &&
                 ( sSrcCharPtr[0] <= 0xfe ) )
            {
                sRet = convertMbToWc4Gbkext2( aSrc,
                                              aSrcRemain,
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
                        WC_TO_UTF16BE( aDest, sWc );

                        sRet = 2;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }
    else
    {
        sRet = RET_ILSEQ;
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return sRet;
}

SInt convertWcToMb4Gbk( void    * aSrc,
                        SInt      aSrcRemain,
                        void    * aDest,
                        SInt      aDestRemain )
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

    UChar             * sDestCharPtr;
    SInt                sRet;
    UShort              sWc;

    sDestCharPtr = (UChar *)aDest;

    UTF16BE_TO_WC( sWc, aSrc );

    if ( aDestRemain < 2 )
    {
        sRet = RET_TOOSMALL;
    }
    else
    {
        /* 1) GB2312 �� ���� */
        if ( ( sWc != 0x30fb ) && ( sWc != 0x2015 ) )
        {
            sRet = convertWcToMb4Gb2312( aSrc, aSrcRemain, aDest, aDestRemain );
        }
        else
        {
            /* ��ȯ���� ���� ���� �˸��� ���� sRet ���� �����Ѵ�. */
            sRet = RET_ILUNI;
        }

        if ( sRet != RET_ILUNI )
        {
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do. */
        }

        /* 2) GBK �� ��ȯ����� ���� */
        sRet = convertWcToMb4Gbkextinv( aSrc,
                                        aSrcRemain,
                                        aDest,
                                        aDestRemain );

        if ( sRet != RET_ILUNI )
        {
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do. */
        }

        if ( ( sWc >= 0x2170 ) && ( sWc <= 0x2179 ) )
        {
            sDestCharPtr[0] = 0xa2;
            sDestCharPtr[1] = 0xa1 + ( sWc - 0x2170 );

            sRet = 2;
        }
        else
        {
            sRet = convertWcToMb4Cp936ext( aSrc,
                                           aSrcRemain,
                                           aDest,
                                           aDestRemain );

            if ( sRet != RET_ILUNI )
            {
                IDE_CONT( NORMAL_EXIT );
            }
            else
            {
                /* Nothing to do. */
            }

            /* ������� GB2312 �� ������ ������ ����ó���̴�. */
            if ( sWc == 0x00b7 )
            {
                sDestCharPtr[0] = 0xa1;
                sDestCharPtr[1] = 0xa4;

                sRet = 2;
            }
            else
            {
                if ( sWc == 0x2014 )
                {
                    sDestCharPtr[0] = 0xa1;
                    sDestCharPtr[1] = 0xaa;

                    sRet = 2;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return sRet;

}
/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰�
 *
 *  - GB18030 �� �߰��Ǿ��� ���� ����� �� �ִ� ���� �Լ��̴�.
 *  - ���翡 ������� �����Ƿ� �ּ�ó���� �Ѵ�. ( 2014-07-15 )
 *
 * SInt copyGbk( void    * aSrc,
 *               SInt      aSrcRemain,
 *              void    * aDest,
 *              SInt      aDestRemain )
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
 **********************************************************************/
/*
 *     UChar * sSrcCharPtr;
 *     UChar * sDestCharPtr;
 *     SInt    sRet;
 *
 *     sSrcCharPtr = (UChar *)aSrc;
 *     sDestCharPtr = (UChar *)aDest;
 *
 *     if ( sSrcCharPtr[0] < 0x80)
 *     {
 *         *sDestCharPtr = *sSrcCharPtr;
 *         sRet = 1;
 *     }
 *     else
 *     {
 *        if( ( sSrcCharPtr[0] >= 0x81 ) && ( sSrcCharPtr[0] < 0xff ) )
 *         {
 *             if ( aSrcRemain < 2 )
 *             {
 *                 sRet = RET_TOOFEW;
 *             }
 *             else
 *             {
 *                 if ( aDestRemain < 2 )
 *                 {
 *                     sRet = RET_TOOSMALL;
 *                 }
 *                 else
 *                 {
 *                     sDestCharPtr[0] = sSrcCharPtr[0];
 *                     sDestCharPtr[1] = sSrcCharPtr[1];
 *
 *                     sRet = 2;
 *                 }
 *             }
 *         }
 *         else
 *         {
 *             sRet = RET_ILSEQ;
 *         }
 *     }
 *
 *     return sRet;
 * }
 */
