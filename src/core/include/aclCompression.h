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
 
/*******************************************************************************
 * $Id: aclCompression.h 3773 2008-11-28 09:05:43Z djin $
 ******************************************************************************/

#if !defined(_O_ACL_COMPRESSION_H_)
#define _O_ACL_COMPRESSION_H_

/**
 * @file
 * @ingroup CoreCompression
 *
 *  RealTime Compresssion and Decompression API using LZO algorithm
 *
 *
 *  ################## CAUTION #######################
 *   ��Ƽ���̽��� iduCompression���� ȣȯ���� ���� �ҽ��� �������� �ʾ���.
 *   iduCompression.h iduCompression.cpp info
 *   URL: svn://svn.altibase.local/opt/svnrepos/altidev4/trunk/src/id/include
 *   URL: svn://svn.altibase.local/opt/svnrepos/altidev4/trunk/src/id/idu
 *    r26440 | jdlee | 2008-06-10 13:02:48 +0900 (ȭ, 10  6�� 2008) | 1 line
 *   
 */


ACP_EXTERN_C_BEGIN

/*
**  Primitive C Datatypes 
*/

#include <acpTypes.h>

#define UChar  acp_uint8_t
#define UInt   acp_uint32_t
#define IDE_RC acp_rc_t 

/*
**  Altibase Macros
*/

/***********************************************************************
 * ���⳪�� ��ũ�� �Լ��� ��� �ؽ����̺� ���õ� ������ �ϴµ� ���δ�.
 *
 * IDU_COMPRESSION_D_BITS : �ؽ����̺��� Ű�� ��Ʈ���� ��Ÿ����.�� ���Ʈ�� Ű�� ������
 *                          �������� ��Ÿ��. �� ��Ʈ�� ���������� �ؼ� �ؽ� ���̺��� 
 *                          ũ�⵵ ���� �� �� �ִ�. ��, �޸𸮻���� ���� ����, �ø�
 *                          ���� �ִ�. compress�Լ��� aWorkMem�� ũ�⿡ ������ �ش�.
 *
 * IDU_COMPRESSION_D_SIZE : �ؽ����̺� Ű�� �ִ� ũ�⸦ ��Ÿ��. 
 *
 * IDU_COMPRESSION_D_MASK : IDU_COMPRESSION_D_BITS ���� ��ŭ�� ���� ��Ʈ�� ��� 1�̴�. 
 *                          �̰Ͱ� and������ �ϸ� �ؽ�Ű�� ��ȿ �����ȿ� ��� �� �� �ִ�.
 *
 * IDU_COMPRESSION_D_HIGH : IDU_COMPRESSION_D_MASK�� �� ������ �� bit�� set�� ��
 *
 **********************************************************************/
#define IDU_COMPRESSION_D_BITS          12
#define IDU_COMPRESSION_D_SIZE          ((acp_uint32_t)1UL << (IDU_COMPRESSION_D_BITS))
#define IDU_COMPRESSION_D_MASK          (IDU_COMPRESSION_D_SIZE - 1)
#define IDU_COMPRESSION_D_HIGH          ((IDU_COMPRESSION_D_MASK >> 1) + 1)


/***********************************************************************
 * IDU_COMPRESSION_WORK_SIZE : compess�Լ��� ���ڷ� ���� aWorkMem�� ũ��� �ݵ��
 *                             �� ũ��� ������ �Ѵ�. �̰��� ũ��� IDU_COMPRESSION_D_BITS��
 *                             ������ �޴´�.  �ֳ��ϸ� �ؽ� Ű�� ������ ���� �ؽ� ���̺���
 *                             ũ�Ⱑ �޶����� �����̰�, aWorkMem�� �ٷ� �ؽ����̺��̱�
 *                             �����̴�. �� �����ϴµ� �־ �����ϴ� �ؽ� ���̺��� �����Ҷ�
 *                             ���� ���� �־�� �Ѵ�. 
 *
 * IDU_COMPRESSION_MAX_OUTSIZE : compress�Լ��� aSrcBuf�� ������ ũ��� �� �� ������,
 *                               aDestBuf�� ũ��� �ݵ�� aSrcBuf size�� ���ڷ� �־� ��
 *                               ��ũ�� �Լ��� �����Ͽ� ũ�⸦ �����ؾ� �Ѵ�.
 *                               �� ũ�⿡ ���� ������ iduCompression.cpp�� '������ ũ�� ��������'
 *                               �κ��� ���� �ϱ� �ٶ���.
 *
 **********************************************************************/
#define IDU_COMPRESSION_WORK_SIZE         ((acp_uint32_t) (IDU_COMPRESSION_D_SIZE * sizeof(acp_char_t *)))
#define IDU_COMPRESSION_MAX_OUTSIZE(size) ( (size) + (((size) / 22)+1) + 3)

/**
 * Work Memory Size for compression.
 * Allocate the size of this memory to compress something.
 */
#define ACL_COMPRESSION_WORK_SIZE         IDU_COMPRESSION_WORK_SIZE

/**
 *  Get the required uncompressed memory size
 *  @param size  size of the original compressed area
 */
#define ACL_COMPRESSION_MAX_OUTSIZE(size) IDU_COMPRESSION_MAX_OUTSIZE(size)

/***********************************************************************
 * Description : ������ ���� �� �� ȣ���ϴ� �Լ�
 * 
 * aSrc        - [IN] : ���� ������ �����ϰ��� �ϴ� �ҽ��� ��� �ִ� ����,
 *                �̶� ������ ũ��           �� ���� �� �� �ִ�.
 *
 * aSrcLen    - [IN] : aSrc������ ����
 *
 * aDest    - [IN] : ������ ����� ����� �ԷµǴ� ����, �� ������ ũ��� �ݵ��
 *         IDU_COMPRESSION_MAX_OUTSIZE(�ҽ�������ũ��) �� �����Ͽ� �����Ѵ�.
 *
 * aDestLen    - [IN] : aDest������ ���� 
 *
 * aResultLen    - [OUT]: ����� ������ ����
 * aWorkMem - [IN] : ����� �ؽ����̺�� ����� �޸𸮸� ���� �־ �����Ų��.
 *         �̶� ���� IDU_COMPRESSION_WORK_SIZEũ��� �޸𸮸� �����Ѵ�.
 *
 *
 *
 * aWorkMem�� �ʱ�ȭ�� �ʿ� ����.
 * �ؽ� ���̺��� ��Ʈ���� srcBuf�� Ư�� ��ġ�� ����Ű�� �������̴�.
 * ���⼭ �ؽ� ���̺��� workMem�� ���Ѵ�. �� �ܺο��� ���� workMem�� ���ο��� 
 * �ؽ� ���̺�� ����Ѵ�. 
 *
 * �׷��� ������ �ؽ� ���̺��� �ִ�ũ��� 
 * "IDU_COMPRESSION_D_SIZE(�ؽ�Ű�� �ִ� ũ��) * �ּ�ũ��" �� �ȴ�.
 *
 *iduCompression.cpp�� COMPRESSION_CHECK_MPOS_NON_DET(m_pos,m_off,aSrcBuf,sSrcPtr,M4_MAX_OFFSET)
 *     �̺κ��� ������ ���� �� ������ �� �� �ִ�. 
 *
 * ����� �� ��츦 ������ ������ ���� ���� ������ �Ǵ� ��츦 ������ ����,
 * 1. srcBuf�� ó�� �ּҺ��� ���� ���� ��� ���� ���( 0����) 
 *    => ���� ��ũ���Լ����� �˻�ȴ�.
 *
 * 2. srcBuf�� ������ �ּҺ��� ū ���� ������� ��� 
 *    =>���� ��ũ���Լ��� ���� ������ ��ġ���� ū ��ġ�� ���� Ż���ϰ� �Ǿ��ִ�.
 *
 * 3. ������ ��ġ���� �۰� srcBuf���� ū ���� ������� ���( ��, ������ ���� 
 *    ������ ������ �ּ��� �� �ϰ� ���� ���) 
 *    => �̶��� ���� ��ũ�ο����� �����ϴٰ� �ǰ��� ����. ������ �Ʒ����� 
 *    ������ ������ �ּҸ� �����ͷ� �Ͽ� ���� �����͸� ã�Ƽ� �˻縦 �ϰ� �ȴ�. 
 *    �� �ƹ��� ������ ���̶�� �ص� �����ͷν� ������ ���� ���̻� �����Ⱑ
 *    �ƴ� ���� �ȴ�.  �̶��� �������� ���� ��쿣 ���� hash ���� ����
 * ���� �����ʹ� �ٸ� ��ó�� �����   ���� �Ǿ� Ż���ϰ� �ȴ�. 
 *
 **********************************************************************/


/**
 * Compress the specified Memory Area
 *
 * @param aSrc       Source Area to be compressed
 * @param aSrcLen    Source Area Memory Size
 * @param aDest      Target Area storing the compressed data
 * @param aDestLen   Target Area Memory Size
 * @param aResultLen Compressed Size
 * @param aWorkMem   Working Memory for Compression
 * @return result error code
 */
ACP_EXPORT acp_rc_t aclCompress(UChar *aSrc,
                                UInt   aSrcLen,
                                UChar *aDest,
                                UInt   aDestLen,
                                UInt*  aResultLen,
                                void*  aWorkMem );

/***********************************************************************
 * Description : ����� ������ ���� �ҽ��� Ǯ�� ����ϴ� �Լ�
 * aSrc        - [IN] : ���� ������ �ִ� ����
 * aSrcLen    - [IN] : aSrc�� ũ��
 * aDest    - [IN] : ���� ���� ������ ���� ����, �̰��� ũ��� compress������
 *             ������ ũ��� �����ϴ�. ������ ���� �İ� ũ�Ⱑ ������ ���
 * aDestLen    - [IN] : aDest�� ũ��
 * aResultLen    - [OUT]: ���� ���� ������ ũ��
 **********************************************************************/

/**
 * Decompress the specified Memory Area
 *
 * @param aSrc       Source Area to be decompressed
 * @param aSrcLen    Source Area Memory Size
 * @param aDest      Target Area Storing the Decompressed data
 * @param aDestLen   Target Area Memory Size
 * @param aResultLen Decompressed Size
 * @return result error code
 */
ACP_EXPORT acp_rc_t aclDecompress(UChar *aSrc,
                                  UInt   aSrcLen,
                                  UChar *aDest,
                                  UInt   aDestLen,
                                  UInt  *aResultLen);
ACP_EXTERN_C_END

#undef UChar
#undef UInt
#undef IDE_RC


#endif
