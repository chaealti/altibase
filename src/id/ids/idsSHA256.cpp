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
 
#include <ide.h>
#include <idsSHA256.h>

#define ASCII2HEX(c) (((c) < 10) ? ('0' + (c)) : ('A' + ((c) - 10)))

/* Rotate Right ( circular right shift ) */
#define ROTR(bits,word) \
    ((((word) >> (bits)) & 0xFFFFFFFF ) | \
      ((word) << (32-(bits))))

/* SHA-256 Functions described in FIPS 180-4 Ch.4.1.2 */
#define CH(x,y,z) \
    (((x) & (y)) ^ ( ~(x) & (z) ))

#define MAJ(x,y,z) \
    (((x) & (y)) ^ ((x) & (z)) ^ ((y) &(z)))

#define USIGMA0(x) \
    (ROTR(2,(x)) ^ ROTR(13,(x)) ^ ROTR(22,(x)))

#define USIGMA1(x) \
    (ROTR(6,(x)) ^ ROTR(11,(x)) ^ ROTR(25,(x)))

#define LSIGMA0(x) \
    (ROTR(7,(x)) ^ ROTR(18,(x)) ^ ((x) >> 3))

#define LSIGMA1(x) \
    (ROTR(17,(x)) ^ ROTR(19,(x)) ^ ((x) >> 10))

void idsSHA256::digestToHexCode( UChar * aByte, 
                                 UChar * aMessage,
                                 UInt    aMessageLen )
{
    UInt        sRemainsLen = aMessageLen;
    UInt sDigest[8]  = {
        /* SHA-256 Initial hash value described in FIPS 180-4 Ch.5.3.3 */
        0x6A09E667,
        0xBB67AE85,
        0x3C6EF372,
        0xA54FF53A,
        0x510E527F,
        0x9B05688C,
        0x1F83D9AB,
        0x5BE0CD19
    };

    while( sRemainsLen >= SHA256_BLK_SIZE )
    {
        processBlock( sDigest, aMessage );
        aMessage    += SHA256_BLK_SIZE;
        sRemainsLen -= SHA256_BLK_SIZE;
    }
    processRemains( sDigest, aMessage, aMessageLen, sRemainsLen );

    makeHexCode( aByte, sDigest );
}

void idsSHA256::digest( UChar * aResult, UChar * aMessage, UInt aMessageLen )
{
    UChar sByte[32];

    digestToHexCode( sByte, aMessage, aMessageLen );

    convertHexToString( aResult, sByte );
}

void idsSHA256::processRemains( UInt  * aDigest,
                                UChar * aMessage,
                                UInt    aMessageLen,
                                UInt    aRemainsLen )
{
    UInt  i;
    UChar sBlock[ SHA256_BLK_SIZE ] = { 0 , };
    UInt  sMessageLenBitHigh        = 0;
    UInt  sMessageLenBitLow         = 0;

    for( i = 0 ; i < aRemainsLen ; i++ )
    {
        sBlock[i] = aMessage[i];
    }
    /* Append the bit '1' to the end of the message */
    sBlock[aRemainsLen] = 0x80;

    if ( aRemainsLen >= 56 )
    {
        processBlock( aDigest, sBlock );
        idlOS::memset( &sBlock, 0, 56 );
    }
    else
    {
        // Nothing to do 
    }

    sMessageLenBitHigh = ( aMessageLen & 0xE0000000 ) >> 24;
    sMessageLenBitLow  = aMessageLen << 3;
    
    sBlock[56] = ( sMessageLenBitHigh >> 24 ) & 0xFF;
    sBlock[57] = ( sMessageLenBitHigh >> 16 ) & 0xFF;
    sBlock[58] = ( sMessageLenBitHigh >> 8 )  & 0xFF;
    sBlock[59] =   sMessageLenBitHigh         & 0xFF;

    sBlock[60] = ( sMessageLenBitLow >> 24 ) & 0xFF;
    sBlock[61] = ( sMessageLenBitLow >> 16 ) & 0xFF;
    sBlock[62] = ( sMessageLenBitLow >>  8 ) & 0xFF;
    sBlock[63] =   sMessageLenBitLow         & 0xFF;

    processBlock( aDigest, sBlock );
}

void idsSHA256::processBlock( UInt  * aDigest,
                              UChar * aBlock )
{
    UInt         i;
    UInt         W[64];
    UInt         A, B, C, D, E, F, G, H, T1, T2; // 8 working variables and 2 temporary words
    static UInt  K[] =
    {
        /* SHA-256 Constants described in FIPS 180-4 Ch.4.2.2 */
        0x428A2F98,
        0x71374491,
        0xB5C0FBCF,
        0xE9B5DBA5,
        0x3956C25B,
        0x59F111F1,
        0x923F82A4,
        0xAB1C5ED5,
        0xD807AA98,
        0x12835B01,
        0x243185BE,
        0x550C7DC3,
        0x72BE5D74,
        0x80DEB1FE,
        0x9BDC06A7,
        0xC19BF174,
        0xE49B69C1,
        0xEFBE4786,
        0x0FC19DC6,
        0x240CA1CC,
        0x2DE92C6F,
        0x4A7484AA,
        0x5CB0A9DC,
        0x76F988DA,
        0x983E5152,
        0xA831C66D,
        0xB00327C8,
        0xBF597FC7,
        0xC6E00BF3,
        0xD5A79147,
        0x06CA6351,
        0x14292967,
        0x27B70A85,
        0x2E1B2138,
        0x4D2C6DFC,
        0x53380D13,
        0x650A7354,
        0x766A0ABB,
        0x81C2C92E,
        0x92722C85,
        0xA2BFE8A1,
        0xA81A664B,
        0xC24B8B70,
        0xC76C51A3,
        0xD192E819,
        0xD6990624,
        0xF40E3585,
        0x106AA070,
        0x19A4C116,
        0x1E376C08,
        0x2748774C,
        0x34B0BCB5,
        0x391C0CB3,
        0x4ED8AA4A,
        0x5B9CCA4F,
        0x682E6FF3,
        0x748F82EE,
        0x78A5636F,
        0x84C87814,
        0x8CC70208,
        0x90BEFFFA,
        0xA4506CEB,
        0xBEF9A3F7,
        0xC67178F2
    };
   
    /* Following steps are described in FIPS 180-4 Ch.6.4 */
    /* 1. Prepare the message schedule */
    for ( i = 0 ; i < 16 ; i++ )
    {
        W[i]  = ( (UInt) aBlock[i * 4    ] ) << 24;
        W[i] |= ( (UInt) aBlock[i * 4 + 1] ) << 16;
        W[i] |= ( (UInt) aBlock[i * 4 + 2] ) <<  8;
        W[i] |= ( (UInt) aBlock[i * 4 + 3] );
    }

    for ( i = 16 ; i < 64 ; i++ )
    {
        W[i] = LSIGMA1(W[i-2]) + W[i-7] + LSIGMA0(W[i-15]) + W[i-16];
    }

    /* 2. Initialize 8 working variables( a,b,c,d,e,f,g,h ) with the previous hash value */
    A = aDigest[0];
    B = aDigest[1];
    C = aDigest[2];
    D = aDigest[3];
    E = aDigest[4];
    F = aDigest[5];
    G = aDigest[6];
    H = aDigest[7];

    /* 3. Compute */
    for ( i = 0 ; i < 64 ; i++ )
    {
        T1 = H + USIGMA1(E) + CH(E,F,G) + K[i] + W[i];
        T2 = USIGMA0(A) + MAJ(A,B,C);
        H = G;
        G = F;
        F = E;
        E = D + T1;
        D = C;
        C = B;
        B = A;
        A = T1 + T2;
    }

    /* 4. Compute hash value */
    aDigest[0] += A;
    aDigest[1] += B;
    aDigest[2] += C;
    aDigest[3] += D;
    aDigest[4] += E;
    aDigest[5] += F;
    aDigest[6] += G;
    aDigest[7] += H;
}

void idsSHA256::makeHexCode( UChar * aByte, UInt * aDigest )
{
    UInt i;

    for ( i = 0; i < 8; i++ )
    {
        *aByte++ = ( *aDigest >> 24 ) & 0xFF;
        *aByte++ = ( *aDigest >> 16 ) & 0xFF;
        *aByte++ = ( *aDigest >>  8 ) & 0xFF;
        *aByte++ =   *aDigest         & 0xFF;

        aDigest++;
    }
}

void idsSHA256::convertHexToString( UChar * aString, UChar * aByte )
{
    UInt i;
    for( i = 0 ; i < 32 ; i++ )
    {
        *aString++ = ASCII2HEX( ( *aByte & 0xF0 ) >> 4 );
        *aString++ = ASCII2HEX( *aByte & 0x0F );

        aByte++;
    }
}

/* BUG-39303
 *
 * void idsSHA256::digestToByte       : Byte �� ��ȯ�ϴ� �ؽ�
 *
 * void idsSHA256::initializeForGroup : Aggregation �ؽ��� �ʱ�ȭ �Լ�
 * void idsSHA256::digestForGroup     : Aggregation �ؽ� �Լ�
 * void idsSHA256::finalizeForGroup   : Aggregation �ؽ��� ���� �Լ�
 *
 */
void idsSHA256::digestToByte( UChar * aResult,
                              UChar * aMessage,
                              UInt    aMessageLen )
{
/***********************************************************************
 *
 * Description : digestToByte
 *
 * Implementation :
 *
 *  �ؽ��� ȣ���� �� ����Ʈ�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    digestToHexCode( aResult, aMessage, aMessageLen );
}
void idsSHA256::initializeForGroup( idsSHA256Context * aContext )
{
/***********************************************************************
 *
 * Description : initForGroup
 *
 * Implementation :
 *
 *  �ؽ� ���ؽ�Ʈ�� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    /* �ؽ� ���ؽ�Ʈ�� �ʱ�ȭ �Ѵ�. */
    aContext->mTotalSize  = 0;
    aContext->mBufferSize = 0;
    /* SHA-256 Initial hash value described in FIPS 180-4 Ch.5.3.3 */
    aContext->mDigest[0]  = 0x6A09E667;
    aContext->mDigest[1]  = 0xBB67AE85;
    aContext->mDigest[2]  = 0x3C6EF372;
    aContext->mDigest[3]  = 0xA54FF53A;
    aContext->mDigest[4]  = 0x510E527F;
    aContext->mDigest[5]  = 0x9B05688C;
    aContext->mDigest[6]  = 0x1F83D9AB;
    aContext->mDigest[7]  = 0x5BE0CD19;
}

void idsSHA256::digestForGroup( idsSHA256Context * aContext )
{
/***********************************************************************
 *
 * Description : digestForGroup
 *
 * Implementation :
 *
 *  Aggregation �ؽ��� �����Ѵ�. 3 ���� ���� �����Ѵ�.
 *    1. ��� ũ�⺸�� ���� ������ ���ۿ� �����Ѵ�.
 *    2. ���۰� �� ���, ������ �ؽ��ϰ� ���� ������ ���ۿ� �����Ѵ�.
 *    3. ���ۿ� ������ �����ϴ� ��쿡, ���۸� ä���� �ؽ��ϰ�, �� ������ ������
 *       �ؽ��ϸ�, ���� ������ ���ۿ� �����Ѵ�
 *       ( �������� ���۸� ���� �ؽ��� ��, �������� 2���� ������ �����Ѵ�. )
 *
 ***********************************************************************/

    UInt    sRetrySize;
    UInt  * sDigest      = aContext->mDigest;
    UInt    sRemainsSize = aContext->mMessageSize;
    UChar * sMessage     = aContext->mMessage;
    UChar * sBuffer      = aContext->mBuffer + aContext->mBufferSize;

    /* finalForGroup �� processReamins �� ���ڸ� ���� ��� */
    aContext->mTotalSize += sRemainsSize;

    /* 1. ��� ũ�⺸�� ���� ������ ���ۿ� �����Ѵ�. */
    if ( ( aContext->mBufferSize + sRemainsSize ) < SHA256_BLK_SIZE )
    {
        idlOS::memcpy( (void*) sBuffer,
                       (void*) sMessage,
                       sRemainsSize );

         aContext->mBufferSize += sRemainsSize;
    }
    else
    {
        if ( aContext->mBufferSize == 0 )
        {
            /* 2. ���۰� �� ���, ������ �ؽ��ϰ� ���� ������ ���ۿ� �����Ѵ�. */
            while( sRemainsSize >= SHA256_BLK_SIZE )
            {
                processBlock( sDigest, sMessage );
                sMessage     += SHA256_BLK_SIZE;
                sRemainsSize -= SHA256_BLK_SIZE;
            }

            if ( sRemainsSize != 0 )
            {
                idlOS::memcpy( (void*) sBuffer,
                               (void*) sMessage,
                               sRemainsSize );

                aContext->mBufferSize = sRemainsSize;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* 3. ���ۿ� ������ �����ϴ� ��쿡, ���۸� ä���� �ؽ��ϰ�, �� ����
             *    ������ �ؽ��ϸ�, ���� ������ ���ۿ� �����Ѵ�.
             */
            if ( ( aContext->mBufferSize + sRemainsSize ) >= SHA256_BLK_SIZE )
            {
                sRetrySize = SHA256_BLK_SIZE - aContext->mBufferSize;

                idlOS::memcpy( (void*) sBuffer,
                               (void*) sMessage,
                               sRetrySize);

                processBlock( sDigest, aContext->mBuffer );
                sMessage     += sRetrySize;
                sRemainsSize -= sRetrySize;

                /* 2. ���۸� ������Ƿ�, ������ �ؽ��ϰ� ���� ������ ���ۿ� ����
                 *    �Ѵ�.
                 */
                while ( sRemainsSize >= SHA256_BLK_SIZE )
                {
                    processBlock( sDigest, sMessage );
                    sMessage     += SHA256_BLK_SIZE;
                    sRemainsSize -= SHA256_BLK_SIZE;
                }

                if ( sRemainsSize != 0 )
                {
                    idlOS::memcpy( (void*) aContext->mBuffer,
                                   (void*) sMessage,
                                   sRemainsSize );

                    aContext->mBufferSize = sRemainsSize;
                }
                else
                {
                    aContext->mBufferSize = 0;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
}

void idsSHA256::finalizeForGroup( UChar * aResult,
                                  idsSHA256Context *  aContext )
{
/***********************************************************************
 *
 * Description : finalizeForGroup
 *
 * Implementation :
 *
 *  Aggregation �ؽ��� �Ϸ��Ѵ�. 2 ���� ���� ����ȴ�.
 *    1. ���ۿ� ��� ũ�� ���� ���� ������ �����ϸ�, processRemains �� ȣ���Ѵ�.
 *    2. ���۰� �� ��쿡�� ������ �۾��� �������� �ʴ´�.
 *
 ***********************************************************************/

    if ( aContext->mBufferSize != 0 )
    {
        /* 1. ���ۿ� ��� ũ�� ���� ���� ������ �����ϸ�, processRemains �� ȣ��
         *    �Ѵ�.
         */
        processRemains( aContext->mDigest,
                        aContext->mBuffer,
                        aContext->mTotalSize,
                        aContext->mBufferSize );
    }
    else
    {
        /* 2. ���۰� �� ��쿡�� ������ �۾��� �������� �ʴ´�.
         *
         * Nothing to do
         *
         */
    }

    makeHexCode( aResult, aContext->mDigest );
}
