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
 * $Id: sdptbBit.cpp 27228 2008-07-23 17:36:52Z newdaily $
 **********************************************************************/

#include <sdptb.h>

//255�� �ȵ���.
static UChar gZeroBitIdx[256] =
{
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,5 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,6 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,5 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,7 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,5 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,6 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,5 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,4 ,
    0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,8
};

//0�� �ȵ���.
static UChar gBitIdx[256] =
{
    8 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    5 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    6 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    5 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    7 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    5 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    6 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    5 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,
    4 ,0 ,1 ,0 ,2 ,0 ,1 ,0 ,3 ,0 ,1 ,0 ,2 ,0 ,1 ,0
};

static UChar zeroBitCntInOneByte[256] =
{
    8 ,7 ,7 ,6 ,7 ,6 ,6 ,5 ,7 ,6 ,6 ,5 ,6 ,5 ,5 ,4 ,
    7 ,6 ,6 ,5 ,6 ,5 ,5 ,4 ,6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,
    7 ,6 ,6 ,5 ,6 ,5 ,5 ,4 ,6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    7 ,6 ,6 ,5 ,6 ,5 ,5 ,4 ,6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,4 ,3 ,3 ,2 ,3 ,2 ,2 ,1 ,
    7 ,6 ,6 ,5 ,6 ,5 ,5 ,4 ,6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,4 ,3 ,3 ,2 ,3 ,2 ,2 ,1 ,
    6 ,5 ,5 ,4 ,5 ,4 ,4 ,3 ,5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,
    5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,4 ,3 ,3 ,2 ,3 ,2 ,2 ,1 ,
    5 ,4 ,4 ,3 ,4 ,3 ,3 ,2 ,4 ,3 ,3 ,2 ,3 ,2 ,2 ,1 ,
    4 ,3 ,3 ,2 ,3 ,2 ,2 ,1 ,3 ,2 ,2 ,1 ,2 ,1 ,1 ,0
};

/*
 * 0�� bit�� ã������ count���� �˻����� �ʴ´�.
 */
#define SDPTB_RESULT_BIT_VAL_gZeroBitIdx( result, count )            (result)

/*
 * 1�� bit�� ã������ count���� �˻��Ѵ�.
 * ������ 1�� ã�� ��ƾ�� �׸� ���� �ʱ� ������ count�� �˻��ص� ������ ����.
 */
#define SDPTB_RESULT_BIT_VAL_gBitIdx( result, count )                         \
               ( (result >= count) ?  SDPTB_BIT_NOT_FOUND : result )

/*
 * aAddr�ּҰ����κ��� ����Ʈ ������ �˻��� �����Ͽ� compare_val�� Ʋ�� ����
 * ������ array�� ����ؼ� �� ��Ʈ�� �ε������� ����.
 * ����: �ѹ���Ʈ������ �˻��� �ϴ°��� �ƴ϶� ��������Ʈ���� ��Ӱ˻��Ѵ�.
 */
#define SDPTB_BIT_FIND_PER_BYTES( aAddr,compare_val ,aBitIdx,                  \
                                  sIdx ,array ,aCount, aMask)                  \
   {                                                                           \
       if (  *(UChar*)aAddr != compare_val )                                   \
       {                                                                       \
           *aBitIdx =                                                          \
            SDPTB_RESULT_BIT_VAL_##array( sIdx + array[*(UChar*)aAddr & aMask],\
                                          aCount);                             \
            IDE_RAISE( return_anyway );                                        \
       }                                                                       \
       else                                                                    \
       {                                                                       \
           (*(UChar**)&aAddr)++;                                               \
           sIdx += SDPTB_BITS_PER_BYTE;                                        \
           if( sIdx >= aCount )                                                \
           {                                                                   \
                *aBitIdx = SDPTB_BIT_NOT_FOUND;                                \
                IDE_RAISE( return_anyway );                                    \
           }                                                                   \
       }                                                                       \
   }

/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 1�� ��Ʈ�� ã�Ƽ�
 *  aBitIdx�� �� �ε�����ȣ�� �Ѱ��ش�.  aHint�� �־��� ��Ʈ �ε�����ȣ��
 *  ������������ aAddr�κ��� �ش��Ʈ�� �˻��Ѵ�.
 *
 * aAddr            - [IN] �˻��� ������ �ּҰ�
 * aCount           - [IN] �˻��� ��Ʈ����
 * aHint            - [IN] �˻������� ��Ʈ(�˻��� ������ "��Ʈ�� index��ȣ")
 * aBitIdx          - [OUT] �˻��� ��Ʈ�� index��ȣ
 *
 ***********************************************************************/
void sdptbBit::findBitFromHint( void *  aAddr,
                                UInt    aCount,
                                UInt    aHint,
                                UInt *  aBitIdx )
{
    UInt    sIdx=0;
    UInt    sLoop;
    UInt    sTemp;
    UChar   sMask = 0; //��Ʈ������ ���� ������� �뵵�� ���ȴ�.
    UInt    sHintInBytes; //����Ʈ������ ��Ʈ���� �����Ѵ�.

    IDE_ASSERT( aHint < aCount );

    IDL_MEM_BARRIER;  // BUG-47666

    sHintInBytes = aHint / SDPTB_BITS_PER_BYTE;

    if( aHint >= SDPTB_BITS_PER_BYTE )
    {
        sIdx = sHintInBytes * SDPTB_BITS_PER_BYTE;
        *(UChar**)&aAddr += sHintInBytes;
    }

    sMask = 0xFF << (aHint % SDPTB_BITS_PER_BYTE);

    /* 
     * BUG-22182  bitmap tbs���� ���Ͽ� free������ �ִµ�
     *            ã�� ���ϴ� ��찡 �ֽ��ϴ�.
     */
    // best case
    if( ((*(UChar*)aAddr & sMask) != 0x00)   ||
        (((aHint % SDPTB_BITS_PER_BYTE) + (aCount - aHint)) 
                                  <= SDPTB_BITS_PER_BYTE) )
    {
        if( (sIdx >> 3) == sHintInBytes) //��Ʈ�� ���Ե� ����Ʈ���
        {
            *aBitIdx = SDPTB_RESULT_BIT_VAL_gBitIdx( 
                                    sIdx + gBitIdx[(*(UChar*)aAddr) & sMask],
                                    aCount );
        }
        else
        {
            *aBitIdx = SDPTB_RESULT_BIT_VAL_gBitIdx( 
                                    sIdx + gBitIdx[(*(UChar*)aAddr) & 0xFF],
                                    aCount );
        }
        IDE_CONT( return_anyway );
    }

    //best case�� �ƴ϶�� ���� �����ִ� ����Ʈ�� �ǳʶپ�߸� �Ѵ�.
    (*(UChar**)&aAddr)++;
    sIdx += SDPTB_BITS_PER_BYTE;

    /*
     * 8����Ʈ�� ���ĵ��� �ʾҴٸ� ���ĵɶ����� ����Ʈ ������ �д´�.
     */
    sTemp = (UInt)((vULong)aAddr % SDPTB_BITS_PER_BYTE);
    if( sTemp != 0 )
    {
        sLoop = SDPTB_BITS_PER_BYTE - sTemp;

        while( sLoop-- ) 
        {
           /* 
            * BUG-22182 bitmap tbs���� ���Ͽ� free������ �ִµ� ã�� ���ϴ�
            *           ��찡 �ֽ��ϴ�.
            */
            if( (sIdx >> 3) == sHintInBytes ) //��Ʈ�� ���Ե� ����Ʈ���
            {                                 
                SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                          0x00,
                                          aBitIdx,
                                          sIdx,
                                          gBitIdx, 
                                          aCount,
                                          sMask );
            }
            else
            {
                SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                          0x00,
                                          aBitIdx,
                                          sIdx,
                                          gBitIdx, 
                                          aCount,
                                          0xFF );
            }
        }

    }

    while ( *(ULong*)aAddr == 0 )
    {
       (*(ULong**)&aAddr)++;
       sIdx +=  SDPTB_BITS_PER_ULONG;
    }

    while( 1 ) 
    {
           /* 
            * BUG-22182 bitmap tbs���� ���Ͽ� free������ �ִµ� ã�� ���ϴ�
            *           ��찡 �ֽ��ϴ�.
            */
            if( (sIdx >> 3) == sHintInBytes ) //��Ʈ�� ���Ե� ����Ʈ���
            {
                SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                          0x00,
                                          aBitIdx,
                                          sIdx,
                                          gBitIdx, 
                                          aCount,
                                          sMask );
            }
            else
            {
                SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                          0x00,
                                          aBitIdx,
                                          sIdx,
                                          gBitIdx, 
                                          aCount,
                                          0xFF );
            }
    }
    
    IDE_EXCEPTION_CONT( return_anyway );

    /*
     * BUG-22363   IDE_ASSERT(sIdx <= aCache->mMaxGGID)[sdptbExtent.cpp:432], 
     *             errno=[16] �� ���� ����������
     *
     * ������ üũ�� �ؼ� ���������� �ٽ��ѹ� Ȯ���ϰ� ����üũ�Ѵ�.
     */
    if( (*aBitIdx != SDPTB_BIT_NOT_FOUND) &&
		(*aBitIdx >= aCount) )
    {
       *aBitIdx = SDPTB_BIT_NOT_FOUND;

    }
    return ; 
}


/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 1�� ��Ʈ�� ã�Ƽ�
 *  aBitIdx�� �� �ε�����ȣ�� �Ѱ��ش�.
 *
 * aAddr            - [IN] �˻��� ������ �ּҰ�
 * aCount           - [IN] �˻��� ��Ʈ����
 * aBitIdx          - [OUT] �˻��� ��Ʈ�� index��ȣ
 *
 ***********************************************************************/
void sdptbBit::findBit( void * aAddr,
                        UInt   aCount,
                        UInt * aBitIdx )
{
    findBitFromHint( aAddr,
                     aCount,
                     0,   // ���������� hint,�� ù��Ʈ���Ͱ˻��Ѵ�.
                     aBitIdx);
}


/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 0�� ��Ʈ�� ã�Ƽ�
 *  aBitIdx�� �� �ε�����ȣ�� �Ѱ��ش�.  aHint�� �־��� ��Ʈ �ε�����ȣ��
 *  ������������ aAddr�κ��� �ش��Ʈ�� �˻��Ѵ�.
 *
 * aAddr            - [IN] �˻��� ������ �ּҰ�
 * aCount           - [IN] �˻��� ��Ʈ����
 * aHint            - [IN] �˻������� ��Ʈ(�˻��� ������ "��Ʈ�� index��ȣ")
 * aBitIdx          - [OUT] �˻��� ��Ʈ�� index��ȣ
 ***********************************************************************/
void sdptbBit::findZeroBitFromHint( void *  aAddr,
                                    UInt    aCount,
                                    UInt    aHint,
                                    UInt *  aBitIdx )
{
    UInt    sIdx=0;
    UInt    sLoop;
    UInt    sTemp;
    UInt    sNBytes;
    
    IDE_ASSERT( aAddr != NULL );
    IDE_ASSERT( aBitIdx != NULL );
    IDE_ASSERT( aHint < aCount );
    
    if( aHint >= SDPTB_BITS_PER_BYTE )
    {
        sNBytes = aHint / SDPTB_BITS_PER_BYTE;
        sIdx = sNBytes*SDPTB_BITS_PER_BYTE;

        *(UChar**)&aAddr += sNBytes;
    }

    // best case
    if( *(UChar*)aAddr != 0xFF )
    {
        *aBitIdx = 
          SDPTB_RESULT_BIT_VAL_gZeroBitIdx( sIdx + gZeroBitIdx[*(UChar*)aAddr],
                                            aCount);
        IDE_CONT( return_anyway );
    }

    /*
     * 8����Ʈ�� ���ĵ��� �ʾҴٸ� ���ĵɶ����� ����Ʈ ������ �д´�.
     */
    sTemp = (UInt)((vULong)aAddr % SDPTB_BITS_PER_BYTE);
    if( sTemp != 0 )
    {
        sLoop = SDPTB_BITS_PER_BYTE - sTemp;

        while( sLoop-- ) 
        {
            SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                      0xFF,
                                      aBitIdx,
                                      sIdx,
                                      gZeroBitIdx, 
                                      aCount,
                                      0xFF);   //space holder 
        }

    }
    IDE_ASSERT( ((vULong)aAddr % SDPTB_BITS_PER_BYTE) == 0 );

    while( *(ULong*)aAddr == ID_ULONG_MAX ) 
    {
       (*(ULong**)&aAddr)++;
       sIdx +=  SDPTB_BITS_PER_ULONG;
    }

    while( 1 ) 
    {
       SDPTB_BIT_FIND_PER_BYTES( aAddr,
                                 0xFF,
                                 aBitIdx,
                                 sIdx,
                                 gZeroBitIdx, 
                                 aCount,
                                 0xFF); //space holder
    }
    
    IDE_EXCEPTION_CONT( return_anyway );

    return ; 
}

#if 0 //not used
/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 0�� ��Ʈ�� ã�Ƽ�
 *  aBitIdx�� �� �ε�����ȣ�� �Ѱ��ش�.
 *
 * aAddr            - [IN] �˻��� ������ �ּҰ�
 * aCount           - [IN] �˻��� ��Ʈ����
 * aBitIdx          - [OUT] �˻��� ��Ʈ�� index��ȣ
 *
 ***********************************************************************/
void sdptbBit::findZeroBit( void * aAddr,
                            UInt   aCount,
                            UInt * aBitIdx )
{
    findZeroBitFromHint( aAddr,
                         aCount,
                         0,   // ���������� hint,�� ù��Ʈ���Ͱ˻��Ѵ�.
                         aBitIdx );
}
#endif

/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 1�� ��Ʈ�� ã�Ƽ�
 *  aBitIdx�� �� �ε�����ȣ�� �Ѱ��ش�.  aHint�� �־��� ��Ʈ �ε�����ȣ��
 *  ������������ aAddr�κ��� �ش��Ʈ�� �˻��Ѵ�.
 *  aCount�� ��Ʈ���� �˻��ߴµ� ã�� ���޴ٸ� �ٽ� ó������ �˻��Ѵ�.(rotate)
 *
 * aAddr            - [IN] �˻��� ������ �ּҰ�
 * aCount           - [IN] �˻��� ��Ʈ����
 * aHint            - [IN] �˻������� ��Ʈ(�˻��� ������ "��Ʈ�� index��ȣ")
 * aBitIdx          - [OUT] �˻��� ��Ʈ�� index��ȣ
 *
 ***********************************************************************/
void sdptbBit::findBitFromHintRotate( void *  aAddr,
                                      UInt    aCount,
                                      UInt    aHint,
                                      UInt *  aBitIdx )
{
    void * sAddr = aAddr;
    findBitFromHint( aAddr,
                     aCount, 
                     aHint,
                     aBitIdx );

    if ( (*aBitIdx == SDPTB_BIT_NOT_FOUND) && (aHint > 0) )
    {
        aAddr = sAddr;
        findBitFromHint( aAddr,
                         aHint,
                         0,           //ù��Ʈ���� �˻�
                         aBitIdx );
    }
    else
    {
        /* nothing  to do */
    }
}

/***********************************************************************
 * Description :
 *  aAddr�ּҰ����κ��� aCount���� ��Ʈ�� ������� 1�� ��Ʈ������ ���� ���Ѵ�.
 *
 * aAddr            - [IN] ���� �ּҰ�
 * aCount           - [IN] ����̵Ǵ� ��Ʈ����
 * aRet             - [OUT] 1�� ��Ʈ�� ������ ��
 *
 ***********************************************************************/
void sdptbBit::sumOfZeroBit( void * aAddr,
                             UInt   aCount,
                             UInt * aRet )
{
    UInt    sSum = 0 ;
    UChar * sP = (UChar *)aAddr;
    UChar   sTemp = 0x01;
    UInt    sLoop = aCount / SDPTB_BITS_PER_BYTE;
    UInt    sRest = aCount % SDPTB_BITS_PER_BYTE;

    while( sLoop-- )
    {
        sSum += zeroBitCntInOneByte[ *sP++ ];
    }

    while( sRest-- )
    {
        if( ( *sP & sTemp ) == 0 ) // 0�� ��Ʈ�� ��Ʈ �߰�!
        {
            sSum++;
        }

        sTemp <<= 1;
    }

    *aRet = sSum;
}


