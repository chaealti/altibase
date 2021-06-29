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
 * $Id: mtc.cpp 36850 2009-11-19 04:57:38Z raysiasd $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>
#include <mtccDef.h>
#include <acp.h>
#include <acl.h>
#if defined (CYGWIN32)
#include <pthread.h>
#include <mtcdTypes.h>
#endif

extern mtdModule mtdDouble;

extern mtlModule mtlAscii;
extern mtlModule mtlUTF16;


// PROJ-1579 NCHAR BUGBUG

mtcGetDBCharSet         getDBCharSet;
mtcGetNationalCharSet   getNationalCharSet;

static acp_thr_once_t   gMtcOnceControl = ACP_THR_ONCE_INITIALIZER;
static acp_sint32_t     gMtcInitCount   = 0;

static ACI_RC           gMtcResult      = ACI_SUCCESS;
static acp_thr_mutex_t  gMtcMutex       = ACP_THR_MUTEX_INITIALIZER;

acp_uint32_t         mtcExtTypeModuleGroupCnt;
mtdModule ** mtcExtTypeModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

acp_uint32_t         mtcExtCvtModuleGroupCnt;
mtvModule ** mtcExtCvtModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

acp_uint32_t         mtcExtFuncModuleGroupCnt;
mtfModule ** mtcExtFuncModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

const acp_uint8_t mtcHashPermut[256] = {
    1, 87, 49, 12,176,178,102,166,121,193,  6, 84,249,230, 44,163,
    14,197,213,181,161, 85,218, 80, 64,239, 24,226,236,142, 38,200,
    110,177,104,103,141,253,255, 50, 77,101, 81, 18, 45, 96, 31,222,
    25,107,190, 70, 86,237,240, 34, 72,242, 20,214,244,227,149,235,
    97,234, 57, 22, 60,250, 82,175,208,  5,127,199,111, 62,135,248,
    174,169,211, 58, 66,154,106,195,245,171, 17,187,182,179,  0,243,
    132, 56,148, 75,128,133,158,100,130,126, 91, 13,153,246,216,219,
    119, 68,223, 78, 83, 88,201, 99,122, 11, 92, 32,136,114, 52, 10,
    138, 30, 48,183,156, 35, 61, 26,143, 74,251, 94,129,162, 63,152,
    170,  7,115,167,241,206,  3,150, 55, 59,151,220, 90, 53, 23,131,
    125,173, 15,238, 79, 95, 89, 16,105,137,225,224,217,160, 37,123,
    118, 73,  2,157, 46,116,  9,145,134,228,207,212,202,215, 69,229,
    27,188, 67,124,168,252, 42,  4, 29,108, 21,247, 19,205, 39,203,
    233, 40,186,147,198,192,155, 33,164,191, 98,204,165,180,117, 76,
    140, 36,210,172, 41, 54,159,  8,185,232,113,196,231, 47,146,120,
    51, 65, 28,144,254,221, 93,189,194,139,112, 43, 71,109,184,209
};

const acp_uint32_t mtcHashInitialValue = 0x01020304;

void mtccInitializeMutex( void )
{
    if ( ACP_RC_NOT_SUCCESS(acpThrMutexCreate(&gMtcMutex, ACP_THR_MUTEX_DEFAULT)))
    {
        aciSetErrorCode(mtERR_FATAL_THR_MUTEXINIT);
        gMtcResult = ACI_FAILURE;
    }
}

ACI_RC mtcInitializeForClient( acp_char_t* aDefaultNls )
{
    acpThrOnce(&gMtcOnceControl, mtccInitializeMutex ) ;

    ACI_TEST( gMtcResult != ACI_SUCCESS );

    ACI_TEST_RAISE( ACP_RC_NOT_SUCCESS(acpThrMutexLock(&gMtcMutex)) ,
                    ERR_MUTEX_LOCK );

    /* mtl.initialize()���� iduMemMgr.malloc()�ÿ� ������ �߻�. iduMemMgr �ʱ�ȭ �ؾ���. */

    if( gMtcInitCount == 0 )
    {
        ACI_TEST( mtlInitialize( aDefaultNls, ACP_TRUE ) != ACI_SUCCESS );
        ACI_TEST( mtdInitialize( NULL, 0 ) != ACI_SUCCESS );
    }

    gMtcInitCount++;

    ACI_TEST_RAISE( ACP_RC_NOT_SUCCESS(acpThrMutexUnlock(&gMtcMutex )) ,
                    ERR_MUTEX_UNLOCK );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_MUTEX_LOCK );
    aciSetErrorCode(mtERR_FATAL_THR_MUTEXLOCK);

    ACI_EXCEPTION( ERR_MUTEX_UNLOCK );
    aciSetErrorCode(mtERR_FATAL_THR_MUTEXUNLOCK);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtcFinalizeForClient( void )
{
    acpThrOnce(&gMtcOnceControl, mtccInitializeMutex ) ;

    ACI_TEST( gMtcResult != ACI_SUCCESS );

    ACI_TEST_RAISE( ACP_RC_NOT_SUCCESS(acpThrMutexLock(&gMtcMutex )) ,
                    ERR_MUTEX_LOCK );

    if( gMtcInitCount > 0 )
    {
        gMtcInitCount--;

        if( gMtcInitCount == 0 )
        {
            ACI_TEST( mtdFinalize() != ACI_SUCCESS );
            ACI_TEST( mtlFinalize() != ACI_SUCCESS );
        }
    }

    ACI_TEST_RAISE( ACP_RC_NOT_SUCCESS(acpThrMutexUnlock(&gMtcMutex )) ,
                    ERR_MUTEX_UNLOCK );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_MUTEX_LOCK );
    aciSetErrorCode(mtERR_FATAL_THR_MUTEXLOCK);
    
    ACI_EXCEPTION( ERR_MUTEX_UNLOCK );
    aciSetErrorCode(mtERR_FATAL_THR_MUTEXUNLOCK);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

// BUG-39147
void mtcDestroyForClient()
{
#if defined(VC_WIN32)
    if (gMtcMutex.mInitFlag == 2)
    {
        acpThrMutexDestroy(&gMtcMutex);
    }
#endif
}

acp_uint32_t mtcIsSameType( const mtcColumn* aColumn1,
                            const mtcColumn* aColumn2 )
{
    if( aColumn1->type.dataTypeId == aColumn2->type.dataTypeId  &&
        aColumn1->type.languageId == aColumn2->type.languageId  &&
        ( aColumn1->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK ) ==
        ( aColumn2->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK ) &&
        aColumn1->precision     == aColumn2->precision      &&
        aColumn1->scale         == aColumn2->scale           )
    {
        return 1;
    }

    return 0;
}

/********************************************************************
 * Description
 *    �� �Լ��� ���� ���̾ ���� �Լ��̴�.
 *    Variable �÷��� ���� NULL�� ��ȯ�Ǹ�
 *    �� �Լ��� mtdModule�� NULL ������ ������ �� ��ȯ�Ѵ�.
 *    �׸��� SM���� callback���� �� �Լ��� ȣ��Ǽ��� �ȵȴ�.
 *    �ֳ��ϸ�, aColumn�� module�� �����ϹǷ� smiColumn��
 *    ���ڷ� �ѱ�� �ȵȴ�.
 *
 ********************************************************************/

#if defined(ACP_CFG_BIG_ENDIAN)
acp_uint32_t mtcHash( acp_uint32_t       aHash,
                      const acp_uint8_t* aValue,
                      acp_uint32_t       aLength )
{
    acp_uint8_t        sHash[4];
    const acp_uint8_t* sFence;

    sFence = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;

    for( ; aValue < sFence; aValue++ )
    {
        sHash[0] = mtcHashPermut[sHash[0]^*aValue];
        sHash[1] = mtcHashPermut[sHash[1]^*aValue];
        sHash[2] = mtcHashPermut[sHash[2]^*aValue];
        sHash[3] = mtcHashPermut[sHash[3]^*aValue];
    }

    return sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3];
}
#else
acp_uint32_t mtcHash(acp_uint32_t       aHash,
                     const acp_uint8_t* aValue,
                     acp_uint32_t       aLength )
{
    acp_uint8_t        sHash[4];
    const acp_uint8_t* sFence;

    sFence = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;
    sFence--;
    // little endian�̱� ������ value�� �������� �Ѵ�.
    for(; sFence >= aValue; sFence--)
    {
        sHash[0] = mtcHashPermut[sHash[0]^*sFence];
        sHash[1] = mtcHashPermut[sHash[1]^*sFence];
        sHash[2] = mtcHashPermut[sHash[2]^*sFence];
        sHash[3] = mtcHashPermut[sHash[3]^*sFence];
    }
    return sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3];
}
#endif

// fix BUG-9496
// float, numeric data type�� ���� hash �Լ�
// (mtc::hash �Լ��� endian�� ���� ����ó��)
// mtc::hashWithExponent�� endian�� ���� ���� ó���� ���� �ʴ´�.
// float, numeric data type�� mtdNumericType ����ü�� ����ϹǷ�
// endian�� ���� ���� ó���� ���� �ʾƵ� ��.
acp_uint32_t mtcHashWithExponent( acp_uint32_t       aHash,
                                  const acp_uint8_t* aValue,
                                  acp_uint32_t       aLength )
{
    acp_uint8_t        sHash[4];
    const acp_uint8_t* sFence;

    sFence = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;

    for( ; aValue < sFence; aValue++ )
    {
        sHash[0] = mtcHashPermut[sHash[0]^*aValue];
        sHash[1] = mtcHashPermut[sHash[1]^*aValue];
        sHash[2] = mtcHashPermut[sHash[2]^*aValue];
        sHash[3] = mtcHashPermut[sHash[3]^*aValue];
    }

    return sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3];
}

acp_uint32_t mtcHashSkip( acp_uint32_t       aHash,
                          const acp_uint8_t* aValue,
                          acp_uint32_t       aLength )
{
    acp_uint8_t        sHash[4];
    acp_uint32_t       sSkip;
    const acp_uint8_t* sFence;
    const acp_uint8_t* sSubFence;

    sSkip   = 2;
    sFence  = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;

    while( aValue < sFence )
    {
        sSubFence = aValue + 2;
        if( sSubFence > sFence )
        {
            sSubFence = sFence;
        }
        for( ; aValue < sSubFence; aValue ++ )
        {
            sHash[0] = mtcHashPermut[sHash[0]^*aValue];
            sHash[1] = mtcHashPermut[sHash[1]^*aValue];
            sHash[2] = mtcHashPermut[sHash[2]^*aValue];
            sHash[3] = mtcHashPermut[sHash[3]^*aValue];
        }
        aValue += sSkip;
        sSkip <<= 1;
    }

    return (sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3]);
}

acp_uint32_t mtcHashWithoutSpace( acp_uint32_t       aHash,
                                  const acp_uint8_t* aValue,
                                  acp_uint32_t       aLength )
{
    // To Fix PR-11941
    // ������ String�� ��� ���� ���ϰ� �ſ� �ɰ���.
    // ����, Space�� ������ ��� String�� ���ϵ��� ��.

    acp_uint8_t        sHash[4];
    const acp_uint8_t* sFence;

    sFence  = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;

    for( ; aValue < sFence; aValue ++ )
    {
        if ( ( *aValue != ' ' ) && ( *aValue != 0x00 ) )
        {
            sHash[0] = mtcHashPermut[sHash[0]^*aValue];
            sHash[1] = mtcHashPermut[sHash[1]^*aValue];
            sHash[2] = mtcHashPermut[sHash[2]^*aValue];
            sHash[3] = mtcHashPermut[sHash[3]^*aValue];
        }
    }

    // To Fix PR-11941
    /*
      acp_uint8_t        sHash[4];
      acp_uint32_t         sSkip;
      const acp_uint8_t* sFence;
      const acp_uint8_t* sSubFence;

      sSkip   = 2;
      sFence  = aValue + aLength;

      sHash[3] = aHash;
      aHash >>= 8;
      sHash[2] = aHash;
      aHash >>= 8;
      sHash[1] = aHash;
      aHash >>= 8;
      sHash[0] = aHash;

      while( aValue < sFence )
      {
      sSubFence = aValue + 2;
      if( sSubFence > sFence )
      {
      sSubFence = sFence;
      }
      for( ; aValue < sSubFence; aValue ++ )
      {
      if( *aValue != ' ' )
      {
      sHash[0] = mtcHashPermut[sHash[0]^*aValue];
      sHash[1] = mtcHashPermut[sHash[1]^*aValue];
      sHash[2] = mtcHashPermut[sHash[2]^*aValue];
      sHash[3] = mtcHashPermut[sHash[3]^*aValue];
      }
      }
      aValue += sSkip;
      sSkip <<= 1;
      }
    */

    return (sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3]);
}

ACI_RC mtcMakeNumeric( mtdNumericType*    aNumeric,
                       acp_uint32_t       aMaximumMantissa,
                       const acp_uint8_t* aString,
                       acp_uint32_t       aLength )
{
    const acp_uint8_t* sMantissaStart;
    const acp_uint8_t* sMantissaEnd;
    const acp_uint8_t* sPointPosition;
    acp_uint32_t       sOffset;
    acp_sint32_t       sSign;
    acp_sint32_t       sExponentSign;
    acp_sint32_t       sExponent;
    acp_sint32_t       sMantissaIterator;

    if( (aString == NULL) || (aLength == 0) )
    {
        aNumeric->length = 0;
    }
    else
    {
        sSign  = 1;
        for(sOffset = 0; sOffset < aLength; sOffset++ )
        {
            if( aString[sOffset] != ' ' && aString[sOffset] != '\t' )
            {
                break;
            }
        }
        ACI_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
        switch( aString[sOffset] )
        {
            case '-':
                sSign = -1;
            case '+':
                sOffset++;
                ACI_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
        }
        if( aString[sOffset] == '.' )
        {
            sMantissaStart = &(aString[sOffset]);
            sPointPosition = &(aString[sOffset]);
            sOffset++;
            ACI_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
            ACI_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
                            ERR_INVALID_LITERAL );
            sOffset++;
            for( ; sOffset < aLength; sOffset++ )
            {
                if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                {
                    break;
                }
            }
            sMantissaEnd = &(aString[sOffset - 1]);
        }
        else
        {
            ACI_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
                            ERR_INVALID_LITERAL );
            sMantissaStart = &(aString[sOffset]);
            sPointPosition = NULL;
            sOffset++;
            for( ; sOffset < aLength; sOffset++ )
            {
                if( aString[sOffset] == '.' )
                {
                    sPointPosition = &(aString[sOffset]);
                    sOffset++;
                    break;
                }
                if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                {
                    break;
                }
            }
            for( ; sOffset < aLength; sOffset++ )
            {
                if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                {
                    break;
                }
            }
            sMantissaEnd = &(aString[sOffset - 1]);
        }
        sExponent = 0;
        if( sOffset < aLength )
        {
            if( aString[sOffset] == 'E' || aString[sOffset] == 'e' )
            {
                sExponentSign = 1;
                sOffset++;
                ACI_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
                switch( aString[sOffset] )
                {
                    case '-':
                        sExponentSign = -1;
                    case '+':
                        sOffset++;
                        ACI_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
                }
                if( sExponentSign > 0 )
                {
                    ACI_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
                                    ERR_INVALID_LITERAL );
                    for( ; sOffset < aLength; sOffset++ )
                    {
                        if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                        {
                            break;
                        }
                        sExponent = sExponent * 10 + aString[sOffset] - '0';
                        ACI_TEST_RAISE( sExponent > 100000000,
                                        ERR_VALUE_OVERFLOW );
                    }
                    sExponent *= sExponentSign;
                }
                else
                {
                    ACI_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
                                    ERR_INVALID_LITERAL );
                    for( ; sOffset < aLength; sOffset++ )
                    {
                        if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                        {
                            break;
                        }
                        if( sExponent < 10000000 )
                        {
                            sExponent = sExponent * 10 + aString[sOffset] - '0';
                        }
                    }
                    sExponent *= sExponentSign;
                }
            }
        }
        for( ; sOffset < aLength; sOffset++ )
        {
            if( aString[sOffset] != ' ' && aString[sOffset] != '\t' )
            {
                break;
            }
        }
        ACI_TEST_RAISE( sOffset < aLength, ERR_INVALID_LITERAL );
        if( sPointPosition == NULL )
        {
            sPointPosition = sMantissaEnd + 1;
        }
        for( ; sMantissaStart <= sMantissaEnd; sMantissaStart++ )
        {
            if( *sMantissaStart != '0' && *sMantissaStart != '.' )
            {
                break;
            }
        }
        for( ; sMantissaEnd >= sMantissaStart; sMantissaEnd-- )
        {
            if( *sMantissaEnd != '0' && *sMantissaEnd != '.' )
            {
                break;
            }
        }
        if( sMantissaStart > sMantissaEnd )
        {
            /* Zero */
            aNumeric->length       = 1;
            aNumeric->signExponent = 0x80;
        }
        else
        {
            if( sMantissaStart < sPointPosition )
            {
                sExponent += sPointPosition - sMantissaStart;
            }
            else
            {
                sExponent -= sMantissaStart - sPointPosition - 1;
            }
            sMantissaIterator = 0;
            if( sSign > 0 )
            {
                aNumeric->length = 1;
                if( sExponent & 0x00000001 )
                {
                    aNumeric->mantissa[sMantissaIterator] =
                        *sMantissaStart - '0';
                    sMantissaStart++;
                    sMantissaIterator++;
                }
                sExponent = ( ( sExponent + 200000001 ) >> 1 ) - 100000000;
                for( ;
                     sMantissaIterator < (acp_sint32_t)aMaximumMantissa;
                     sMantissaIterator++ )
                {
                    if( sMantissaEnd < sMantissaStart )
                    {
                        break;
                    }
                    if( *sMantissaStart == '.' )
                    {
                        sMantissaStart++;
                    }
                    if( sMantissaEnd - sMantissaStart < 1 )
                    {
                        break;
                    }
                    if( sMantissaStart[1] == '.' )
                    {
                        if( sMantissaEnd - sMantissaStart < 2 )
                        {
                            break;
                        }
                        aNumeric->mantissa[sMantissaIterator] =
                            ( sMantissaStart[0] - '0' ) * 10
                            + ( sMantissaStart[2] - '0' );
                        sMantissaStart += 3;
                    }
                    else
                    {
                        aNumeric->mantissa[sMantissaIterator] =
                            ( sMantissaStart[0] - '0' ) * 10
                            + ( sMantissaStart[1] - '0' );
                        sMantissaStart += 2;
                    }
                }
                if( sMantissaStart <= sMantissaEnd )
                {
                    if( *sMantissaStart == '.' )
                    {
                        sMantissaStart++;
                    }
                }
                if( sMantissaIterator < (acp_sint32_t)aMaximumMantissa &&
                    sMantissaStart   == sMantissaEnd )
                {
                    aNumeric->mantissa[sMantissaIterator] =
                        ( sMantissaStart[0] - '0' ) * 10;
                    sMantissaStart++;
                    sMantissaIterator++;
                }
                if( sMantissaStart <= sMantissaEnd )
                {
                    if( sMantissaStart[0] >= '5' )
                    {
                        for( sMantissaIterator--;
                             sMantissaIterator >= 0;
                             sMantissaIterator-- )
                        {
                            aNumeric->mantissa[sMantissaIterator]++;
                            if( aNumeric->mantissa[sMantissaIterator] < 100 )
                            {
                                break;
                            }
                            aNumeric->mantissa[sMantissaIterator] = 0;
                        }
                        if( sMantissaIterator < 0 )
                        {
                            aNumeric->mantissa[0] = 1;
                            sExponent++;
                            sMantissaIterator++;
                        }
                        sMantissaIterator++;
                    }
                }

                ACI_TEST_RAISE( sExponent > 63, ERR_VALUE_OVERFLOW );
                if( sExponent >= -63 )
                {
                    aNumeric->length       = sMantissaIterator + 1;
                    aNumeric->signExponent = 0x80 | ( sExponent + 64 );
                }
                else
                {
                    aNumeric->length       = 1;
                    aNumeric->signExponent = 0x80;
                }
            }
            else
            {
                aNumeric->length = 1;
                if( sExponent & 0x00000001 )
                {
                    aNumeric->mantissa[sMantissaIterator] =
                        99 - ( *sMantissaStart - '0' );
                    sMantissaStart++;
                    sMantissaIterator++;
                }
                sExponent = ( ( sExponent + 200000001 ) >> 1 ) - 100000000;
                for( ;
                     sMantissaIterator < (acp_sint32_t)aMaximumMantissa;
                     sMantissaIterator++ )
                {
                    if( sMantissaEnd < sMantissaStart )
                    {
                        break;
                    }
                    if( *sMantissaStart == '.' )
                    {
                        sMantissaStart++;
                    }
                    if( sMantissaEnd - sMantissaStart < 1 )
                    {
                        break;
                    }
                    if( sMantissaStart[1] == '.' )
                    {
                        if( sMantissaEnd - sMantissaStart < 2 )
                        {
                            break;
                        }
                        aNumeric->mantissa[sMantissaIterator] =
                            99 - ( ( sMantissaStart[0] - '0' ) * 10
                                   + ( sMantissaStart[2] - '0' ) );
                        sMantissaStart += 3;
                    }
                    else
                    {
                        aNumeric->mantissa[sMantissaIterator] =
                            99 - ( ( sMantissaStart[0] - '0' ) * 10
                                   + ( sMantissaStart[1] - '0' ) );
                        sMantissaStart += 2;
                    }
                }
                if( sMantissaStart <= sMantissaEnd )
                {
                    if( *sMantissaStart == '.' )
                    {
                        sMantissaStart++;
                    }
                }
                if( sMantissaIterator < (acp_sint32_t)aMaximumMantissa &&
                    sMantissaStart == sMantissaEnd )
                {
                    aNumeric->mantissa[sMantissaIterator] =
                        99 - ( ( sMantissaStart[0] - '0' ) * 10 );
                    sMantissaStart++;
                    sMantissaIterator++;
                }
                if( sMantissaStart <= sMantissaEnd )
                {
                    if( sMantissaStart[0] >= '5' )
                    {
                        for( sMantissaIterator--;
                             sMantissaIterator >= 0;
                             sMantissaIterator-- )
                        {
                            if( aNumeric->mantissa[sMantissaIterator] == 0 )
                            {
                                aNumeric->mantissa[sMantissaIterator] = 99;
                            }
                            else
                            {
                                aNumeric->mantissa[sMantissaIterator]--;
                                break;
                            }
                        }
                        if( sMantissaIterator < 0 )
                        {
                            aNumeric->mantissa[0] = 98;
                            sExponent++;
                            sMantissaIterator++;
                        }
                        sMantissaIterator++;
                    }
                }

                ACI_TEST_RAISE( sExponent > 63, ERR_VALUE_OVERFLOW );
                if( sExponent >= -63 )
                {
                    aNumeric->length       = sMantissaIterator + 1;
                    aNumeric->signExponent = 64 - sExponent;
                }
                else
                {
                    aNumeric->length       = 1;
                    aNumeric->signExponent = 0x80;
                }
            }
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);

    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtcNumericCanonize( mtdNumericType *aValue,
                           mtdNumericType *aCanonizedValue,
                           acp_sint32_t    aCanonPrecision,
                           acp_sint32_t    aCanonScale,
                           acp_bool_t     *aCanonized )
{
    acp_sint32_t sMaximumExponent;
    acp_sint32_t sMinimumExponent;
    acp_sint32_t sExponent;
    acp_sint32_t sExponentEnd;
    acp_sint32_t sCount;

    *aCanonized = ACP_TRUE;

    if( aValue->length > 1 )
    {
        sMaximumExponent =
            ( ( ( aCanonPrecision - aCanonScale ) + 2001 ) >> 1 ) - 1000;
        sMinimumExponent =
            ( ( 2002 - aCanonScale ) >> 1 ) - 1000;
        if( aValue->signExponent & 0x80 )
        {
            sExponent = ( aValue->signExponent & 0x7F ) - 64;
            ACI_TEST_RAISE( sExponent > sMaximumExponent,
                            ERR_VALUE_OVERFLOW );
            if( sExponent == sMaximumExponent )
            {
                if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                {
                    ACI_TEST_RAISE( aValue->mantissa[0] >= 10,
                                    ERR_VALUE_OVERFLOW );
                }
            }
            sExponentEnd = sExponent - aValue->length + 2;
            if( sExponent >= sMinimumExponent )
            {
                if( ( sExponentEnd < sMinimumExponent )   ||
                    ( sExponentEnd == sMinimumExponent  &&
                      ( aCanonScale & 0x01 ) == 1     &&
                      aValue->mantissa[sExponent-sExponentEnd] % 10 != 0 ) )
                {
                    if( aCanonScale & 0x01 )
                    {
                        sCount = sExponent - sMinimumExponent;
                        aCanonizedValue->signExponent = aValue->signExponent;
                        aCanonizedValue->length       = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 >= 5 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 10;
                            sCount--;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10;
                            sCount--;
                            if( aCanonizedValue->mantissa[sCount+1] == 0 )
                            {
                                aCanonizedValue->length--;
                                for( ; sCount >= 0; sCount-- )
                                {
                                    aCanonizedValue->mantissa[sCount] =
                                        aValue->mantissa[sCount];
                                    if( aCanonizedValue->mantissa[sCount] != 0 )
                                    {
                                        break;
                                    }
                                    aCanonizedValue->length--;
                                }
                            }
                        }
                        if( aCanonizedValue->mantissa[sCount+1] > 99 )
                        {
                            aCanonizedValue->length--;
                            for( ; sCount >= 0; sCount-- )
                            {
                                aCanonizedValue->mantissa[sCount] =
                                    aValue->mantissa[sCount] + 1;
                                if( aCanonizedValue->mantissa[sCount] < 100 )
                                {
                                    sCount--;
                                    break;
                                }
                                aCanonizedValue->length--;
                            }
                        }
                    }
                    else
                    {
                        sCount = sExponent - sMinimumExponent;
                        aCanonizedValue->signExponent = aValue->signExponent;
                        aCanonizedValue->length       = sCount + 2;
                        if( aValue->mantissa[sCount+1] >= 50 )
                        {
                            for( ; sCount >= 0; sCount-- )
                            {
                                aCanonizedValue->mantissa[sCount] =
                                    aValue->mantissa[sCount] + 1;
                                if( aCanonizedValue->mantissa[sCount] < 100 )
                                {
                                    sCount--;
                                    break;
                                }
                                aCanonizedValue->length--;
                            }
                        }
                    }
                    for( ; sCount >= 0; sCount-- )
                    {
                        aCanonizedValue->mantissa[sCount] =
                            aValue->mantissa[sCount];
                    }

                    //------------------------------------------------
                    // To Fix BUG-13225
                    // ex) Numeric : 1  byte( length ) +
                    //               1  byte( sign & exponent ) +
                    //               20 byte( mantissa )
                    // 
                    //     Numeric( 6 )�� ��, 100000.01111�� ������ ����
                    //     �����Ǿ�� ��
                    //     100000.0111 => length : 3
                    //                    sign & exponent : 197
                    //                    mantissa : 1
                    //     �׷��� length�� 5�� �����Ǵ� ������ �־���
                    //     ���� : mantissa���� 0�� ���, length��
                    //            �ٿ��� ��
                    //------------------------------------------------
                        
                    for( sCount=aCanonizedValue->length-2; sCount>0; sCount-- )
                    {
                        if( aCanonizedValue->mantissa[sCount] == 0 )
                        {
                            aCanonizedValue->length--;
                        }
                        else
                        {
                            break;
                        }                    
                    }          
                        
                    if( aCanonizedValue->mantissa[0] == 0 )
                    {
                        aCanonizedValue->length       = 1;
                        aCanonizedValue->signExponent = 0x80;
                    }
                    else
                    {
                        if( aCanonizedValue->mantissa[0] > 99 )
                        {
                            ACI_TEST_RAISE( aCanonizedValue->signExponent
                                            == 0xFF,
                                            ERR_VALUE_OVERFLOW );
                            aCanonizedValue->signExponent++;
                            aCanonizedValue->length      = 2;
                            aCanonizedValue->mantissa[0] = 1;
                        }
                        sExponent =
                            ( aCanonizedValue->signExponent & 0x7F ) - 64;
                        ACI_TEST_RAISE( sExponent > sMaximumExponent,
                                        ERR_VALUE_OVERFLOW );
                        if( sExponent == sMaximumExponent )
                        {
                            if( ( aCanonPrecision - aCanonScale )
                                & 0x01 )
                            {
                                ACI_TEST_RAISE( aCanonizedValue->mantissa[0]
                                                >= 10,
                                                ERR_VALUE_OVERFLOW );
                            }
                        }
                    }
                }
                else
                {
                    *aCanonized = ACP_FALSE;
                }
            }
            else if ( sExponent == sMinimumExponent - 1 &&
                      ( aCanonScale & 0x01 ) == 0 )
            {
                if( aValue->mantissa[0] >= 50 )
                {
                    ACI_TEST_RAISE( aValue->signExponent == 0xFF,
                                    ERR_VALUE_OVERFLOW );
                    aCanonizedValue->length       = 2;
                    aCanonizedValue->signExponent = aValue->signExponent + 1;
                    aCanonizedValue->mantissa[0]  = 1;
                }
                else
                {
                    aCanonizedValue->length       = 1;
                    aCanonizedValue->signExponent = 0x80;
                }
                sExponent = ( aCanonizedValue->signExponent & 0x7F ) - 64;
                ACI_TEST_RAISE( sExponent > sMaximumExponent,
                                ERR_VALUE_OVERFLOW );
                if( sExponent == sMaximumExponent )
                {
                    if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                    {
                        ACI_TEST_RAISE( aCanonizedValue->mantissa[0] >= 10,
                                        ERR_VALUE_OVERFLOW );
                    }
                }
            }
            else
            {
                aCanonizedValue->length       = 1;
                aCanonizedValue->signExponent = 0x80;
            }
        }
        else
        {
            sExponent = 64 - ( aValue->signExponent & 0x7F );
            ACI_TEST_RAISE( sExponent > sMaximumExponent,
                            ERR_VALUE_OVERFLOW );
            if( sExponent == sMaximumExponent )
            {
                if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                {
                    ACI_TEST_RAISE( aValue->mantissa[0] <= 89,
                                    ERR_VALUE_OVERFLOW );
                }
            }
            sExponentEnd = sExponent - aValue->length + 2;
            if( sExponent >= sMinimumExponent )
            {
                if( sExponentEnd < sMinimumExponent ||
                    ( sExponentEnd == sMinimumExponent &&
                      ( aCanonScale & 0x01 ) == 1   &&
                      aValue->mantissa[sExponent-sExponentEnd] % 10 != 9 ) )
                {
                    if( aCanonScale & 0x01 )
                    {
                        sCount = sExponent - sMinimumExponent;
                        aCanonizedValue->signExponent = aValue->signExponent;
                        aCanonizedValue->length       = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 <= 4 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 - 1;
                            sCount--;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 9;
                            sCount--;
                            if( aCanonizedValue->mantissa[sCount+1] == 99 )
                            {
                                aCanonizedValue->length--;
                                for( ; sCount >= 0; sCount-- )
                                {
                                    aCanonizedValue->mantissa[sCount] =
                                        aValue->mantissa[sCount];
                                    if( aCanonizedValue->mantissa[sCount]
                                        != 99 )
                                    {
                                        break;
                                    }
                                    aCanonizedValue->length--;
                                }
                            }
                        }
                        if( aCanonizedValue->mantissa[sCount+1] > 99 )
                        {
                            aCanonizedValue->length--;
                            for( ; sCount >= 0; sCount-- )
                            {
                                aCanonizedValue->mantissa[sCount] =
                                    aValue->mantissa[sCount] - 1;
                                if( aCanonizedValue->mantissa[sCount] < 100 )
                                {
                                    sCount--;
                                    break;
                                }
                                aCanonizedValue->length--;
                            }
                        }
                    }
                    else
                    {
                        sCount = sExponent - sMinimumExponent;
                        aCanonizedValue->signExponent = aValue->signExponent;
                        aCanonizedValue->length       = sCount + 2;
                        if( aValue->mantissa[sCount+1] <= 49 )
                        {
                            for( ; sCount >= 0; sCount-- )
                            {
                                aCanonizedValue->mantissa[sCount] =
                                    aValue->mantissa[sCount] - 1;
                                if( aCanonizedValue->mantissa[sCount] <= 99 )
                                {
                                    sCount--;
                                    break;
                                }
                                aCanonizedValue->length--;
                            }
                        }
                    }
                    for( ; sCount >= 0; sCount-- )
                    {
                        aCanonizedValue->mantissa[sCount] =
                            aValue->mantissa[sCount];
                    }

                    // To Fix BUG-13225
                    for( sCount=aCanonizedValue->length-2; sCount>0; sCount-- )
                    {
                        if( aCanonizedValue->mantissa[sCount] == 99 )
                        {
                            aCanonizedValue->length--;
                        }
                        else
                        {
                            break;
                        }
                    }

                    if( aCanonizedValue->mantissa[0] == 99 )
                    {
                        aCanonizedValue->length       = 1;
                        aCanonizedValue->signExponent = 0x80;
                    }
                    else
                    {
                        if( aCanonizedValue->mantissa[0] > 99 )
                        {
                            ACI_TEST_RAISE( aCanonizedValue->signExponent
                                            == 0x01,
                                            ERR_VALUE_OVERFLOW );
                            aCanonizedValue->signExponent--;
                            aCanonizedValue->length      = 2;
                            aCanonizedValue->mantissa[0] = 98;
                        }
                        sExponent =
                            64 - ( aCanonizedValue->signExponent & 0x7F );
                        ACI_TEST_RAISE( sExponent > sMaximumExponent,
                                        ERR_VALUE_OVERFLOW );
                        if( sExponent == sMaximumExponent )
                        {
                            if( ( aCanonPrecision - aCanonScale )
                                & 0x01 )
                            {
                                ACI_TEST_RAISE( aCanonizedValue->mantissa[0]
                                                <= 89,
                                                ERR_VALUE_OVERFLOW );
                            }
                        }
                    }
                }
                else
                {
                    *aCanonized = ACP_FALSE;
                }
            }
            else if ( sExponent == sMinimumExponent - 1 &&
                      ( aCanonScale & 0x01 ) == 0 )
            {
                if( aValue->mantissa[0] <= 49 )
                {
                    ACI_TEST_RAISE( aValue->signExponent == 0x01,
                                    ERR_VALUE_OVERFLOW );
                    aCanonizedValue->length       = 2;
                    aCanonizedValue->signExponent = aValue->signExponent - 1;
                    aCanonizedValue->mantissa[0]  = 98;
                }
                else
                {
                    aCanonizedValue->length       = 1;
                    aCanonizedValue->signExponent = 0x80;
                }
                sExponent = 64 - aCanonizedValue->signExponent;
                ACI_TEST_RAISE( sExponent > sMaximumExponent,
                                ERR_VALUE_OVERFLOW );
                if( sExponent == sMaximumExponent )
                {
                    if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                    {
                        ACI_TEST_RAISE( aCanonizedValue->mantissa[0] <= 89,
                                        ERR_VALUE_OVERFLOW );
                    }
                }
            }
            else
            {
                aCanonizedValue->length       = 1;
                aCanonizedValue->signExponent = 0x80;
            }
        }
    }
    else
    {
        *aCanonized = ACP_FALSE;
    }

    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
    
}


void mtcMakeNumeric2( mtdNumericType*        aNumeric,
                      const mtaTNumericType* aNumber )
{
    acp_uint32_t sCount;

    if( MTA_TNUMERIC_IS_NULL( aNumber ) )
    {
        aNumeric->length = 0;
    }
    else if( MTA_TNUMERIC_IS_ZERO( aNumber ) )
    {
        aNumeric->length       = 1;
        aNumeric->signExponent = 0x80;
    }
    else if( MTA_TNUMERIC_IS_POSITIVE( aNumber ) )
    {
        aNumeric->signExponent = aNumber->sebyte;
        for( sCount = 0, aNumeric->length = 1;
             sCount < sizeof(aNumber->mantissa) - 1;
             sCount++ )
        {
            if( aNumber->mantissa[sCount] != 0 )
            {
                aNumeric->length = sCount + 2;
            }
        }
        for( sCount = 0;
             sCount < (acp_uint32_t)( aNumeric->length - 1 );
             sCount++ )
        {
            aNumeric->mantissa[sCount] = aNumber->mantissa[sCount];
        }
    }
    else
    {
        aNumeric->signExponent = ( aNumber->sebyte & 0x80 )
            | ( 0x80 - ( aNumber->sebyte & 0x7F ) );
        for( sCount = 0, aNumeric->length = 1;
             sCount < sizeof(aNumber->mantissa) - 1;
             sCount++ )
        {
            if( aNumber->mantissa[sCount] != 0 )
            {
                aNumeric->length = sCount + 2;
            }
        }
        for( sCount = 0;
             sCount < (acp_uint32_t)( aNumeric->length - 1 );
             sCount++ )
        {
            aNumeric->mantissa[sCount] = 99 - aNumber->mantissa[sCount];
        }
    }
}

/*-----------------------------------------------------------------------------
 * Name :
 *    ACI_RC mtc::addFloat()
 *
 * Argument :
 *    aValue - point of value , ��� �Ǿ����� ��
 *    aArgument1
 *    aArgument2 - �Է°� aValue = aArgument1 + aArgument2
 *    aPrecisionMaximum - �ڸ���
 *
 * Description :
 *    1. exponent���
 *    2. �ڸ������ ���� ; carry ����
 *    3. �Ǿ��� ĳ���� �ڸ���+1 ���ش�
 * ---------------------------------------------------------------------------*/


ACI_RC mtcAddFloat( mtdNumericType *aValue,
                    acp_uint32_t    aPrecisionMaximum,
                    mtdNumericType *aArgument1,
                    mtdNumericType *aArgument2 )
{
    acp_sint32_t sCarry;
    acp_sint32_t sExponent1;
    acp_sint32_t sExponent2;
    acp_sint32_t mantissaIndex;
    acp_sint32_t tempLength;
    acp_sint32_t longLength;
    acp_sint32_t shortLength;

    acp_sint32_t lIndex;
    acp_sint32_t sIndex;
    acp_sint32_t checkIndex;

    acp_sint32_t sLongExponent;
    acp_sint32_t sShortExponent;

    mtdNumericType *longArg;
    mtdNumericType *shortArg;

    acpMemSet( aValue->mantissa, 0, sizeof(aValue->mantissa) );

    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
    }
    else if( aArgument1->length == 1 )
    {
        aValue->length = aArgument2->length;
        aValue->signExponent = aArgument2->signExponent;
        acpMemCpy( aValue->mantissa , aArgument2->mantissa , aArgument2->length - 1);
    }
    else if( aArgument2->length == 1 )
    {
        aValue->length = aArgument1->length;
        aValue->signExponent = aArgument1->signExponent;
        acpMemCpy( aValue->mantissa , aArgument1->mantissa , aArgument1->length - 1);
    }
    else
    {
        //exponent
        if( (aArgument1->signExponent & 0x80) == 0x80 )
        {
            sExponent1 = (aArgument1->signExponent & 0x7F) - 64;
        }
        else
        {
            sExponent1 = 64 - (aArgument1->signExponent & 0x7F);
        }

        if( (aArgument2->signExponent & 0x80) == 0x80 )
        {
            sExponent2 = (aArgument2->signExponent & 0x7F) - 64;
        }
        else
        {
            sExponent2 = 64 - (aArgument2->signExponent & 0x7F);
        }

        //maxLength
        if( sExponent1 < sExponent2 )
        {
            tempLength = aArgument1->length + (sExponent2 - sExponent1);
            sExponent1 = sExponent2;

            if( tempLength > aArgument2->length )
            {
                longLength = tempLength;
                shortLength = aArgument2->length;
                longArg = aArgument1;
                shortArg = aArgument2;
            }
            else
            {
                longLength = aArgument2->length;
                shortLength = tempLength;
                longArg = aArgument2;
                shortArg = aArgument1;
            }
        }
        else
        {
            tempLength = aArgument2->length + (sExponent1 - sExponent2);

            if( tempLength > aArgument1->length )
            {
                longLength = tempLength;
                shortLength = aArgument1->length;
                longArg = aArgument2;
                shortArg = aArgument1;
            }
            else
            {
                longLength = aArgument1->length;
                shortLength = tempLength;
                longArg = aArgument1;
                shortArg = aArgument2;
            }
        }

        //�Ű����� 0���� ä�������� 21���� ������
        if( longLength - longArg->length > (acp_sint32_t)(aPrecisionMaximum / 2 + 2) )
        {
            aValue->length = shortArg->length;
            aValue->signExponent = shortArg->signExponent;

            acpMemCpy( aValue->mantissa ,
                       shortArg->mantissa ,
                       shortArg->length);
            return ACI_SUCCESS;
        }

        if( longLength > (acp_sint32_t) (aPrecisionMaximum / 2 + 2) )
        {
            // BUG-10557 fix
            // maxLength�� 21�� ���� ��, 21�� ���̴µ�
            // �̶�, argument1 �Ǵ� argument2�� ���꿡 ������ ������
            // maxLength�� �پ�� ��ŭ ����� �Ѵ�.
            //
            // ���� ��� 158/10000*17/365 + 1�� ����� ��,
            // 158/10000*17/365�� 0.00073589... (length=20) �ε�,
            // maxLength�� 22���� 21�� ���϶�,
            // 0.00073589...�� ���̸� 19�� �ٿ� ������ ���ڸ��� �����ؾ� �Ѵ�.
            // subtractFloat������ ���������� �̷� ó���� ���־�� �Ѵ�.
            //
            lIndex = longArg->length - 2 - (longLength - 21);
            longLength = 21;
        }
        else
        {
            lIndex = longArg->length - 2;
        }

        sIndex = shortArg->length - 2;

        sCarry = 0;

        sLongExponent = longArg->signExponent & 0x80;
        sShortExponent = shortArg->signExponent & 0x80;

        //     1  : 2 :   3
        //   xxxxxxxxx________
        //+  ______xxxxxxxxxxx
        //�׸��� ��ġ�� �ʴ� ����
        //������� POSITIVE : 0
        //         NEGATIVE : 99
        for( mantissaIndex = longLength - 2 ; mantissaIndex >= 0 ; mantissaIndex-- )
        {
            // 3����?
            if( mantissaIndex > (shortLength -2) && lIndex >= 0)
            {
                if( sShortExponent == 0x80 )
                {
                    aValue->mantissa[mantissaIndex] =
                        longArg->mantissa[lIndex] + sCarry;
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        longArg->mantissa[lIndex] + 99 + sCarry;
                }
                lIndex--;
            }
            //�����
            else if( mantissaIndex > (shortLength -2) && lIndex < 0)
            {
                //ĳ�� ����
                aValue->mantissa[mantissaIndex] = 0;
                if( sShortExponent == 0x00 )
                {
                    aValue->mantissa[mantissaIndex] = 99;
                }
                if( sLongExponent == 0x00 )
                {
                    aValue->mantissa[mantissaIndex] += 99;
                }
                aValue->mantissa[mantissaIndex] += sCarry;
            }
            //2����
            else if( mantissaIndex <= (shortLength - 2) && lIndex >=0 && sIndex >=0)
            {
                aValue->mantissa[mantissaIndex] =
                    longArg->mantissa[lIndex] +
                    shortArg->mantissa[sIndex] + sCarry;
                lIndex--;
                sIndex--;
            }
            //1����
            else if( lIndex < 0 )
            {
                if( sLongExponent == 0x80 )
                {
                    aValue->mantissa[mantissaIndex] =
                        shortArg->mantissa[sIndex] + sCarry;
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        shortArg->mantissa[sIndex] + 99 + sCarry;
                }
                sIndex--;
            }
            else if( sIndex < 0 )
            {

                if( sShortExponent == 0x80 )
                {
                    aValue->mantissa[mantissaIndex] =
                        longArg->mantissa[lIndex] + sCarry;
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        longArg->mantissa[lIndex] + 99 + sCarry;
                }
                lIndex--;
            }

            //ĳ�� ����
            if( aValue->mantissa[mantissaIndex] >= 100 )
            {
                aValue->mantissa[mantissaIndex] -= 100;
                sCarry = 1;
            }
            else
            {
                sCarry = 0;
            }
        }

        //������ ���� ĳ���� �ڸ��� �̵��� ���� �׸��� +1�� ���ش�
        //����� �ڸ� �̵�
        if( sLongExponent != sShortExponent )
        {
            if( sCarry )
            {
                for( mantissaIndex = longLength - 2 ; mantissaIndex >= 0 ; mantissaIndex-- )
                {
                    aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex] + sCarry;
                    if( aValue->mantissa[mantissaIndex] >= 100 )
                    {
                        sCarry = 1;
                        aValue->mantissa[mantissaIndex] -= 100;
                    }
                    else
                    {
                        break;
                    }
                }
                sCarry = 1;
            }
        }
        //�Ѵ� ����
        //������1 �����ش�
        else if( sLongExponent == 0x00 && sShortExponent == 0x00 )
        {
            if( sCarry == 0 )
            {
                for( mantissaIndex = longLength - 1 ; mantissaIndex > 0 ; mantissaIndex-- )
                {
                    aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex - 1];
                }
                aValue->mantissa[0] = 98;
                sExponent1++;
                longLength++;
            }

            sCarry = 1;
            for( mantissaIndex = longLength - 2 ; mantissaIndex >= 0 ; mantissaIndex-- )
            {
                aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex] + sCarry;
                if( aValue->mantissa[mantissaIndex] >= 100 )
                {
                    sCarry = 1;
                    aValue->mantissa[mantissaIndex] -= 100;
                }
                else
                {
                    sCarry = 0;
                    break;
                }
            }

        }
        else
        {
            if( sCarry )
            {
                for( mantissaIndex = longLength - 1 ; mantissaIndex > 0 ; mantissaIndex-- )
                {
                    aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex - 1];
                }
                aValue->mantissa[0] = 1;
                sExponent1++;
                longLength++;
            }
        }

        ACI_TEST_RAISE( sExponent1 > 63 , ERR_OVERFLOW);

        checkIndex = 0;

        if( (sShortExponent == 0x80 && sLongExponent == 0x80) ||
            (sShortExponent != sLongExponent &&
             sCarry == 1 ) )
        {

            //���� 0�� �����ش�
            for( mantissaIndex = 0 ; mantissaIndex < longLength - 1 ; mantissaIndex++ )
            {
                if( aValue->mantissa[mantissaIndex] == 0 )
                {
                    checkIndex++;
                }
                else
                {
                    break;
                }
            }
            for( mantissaIndex = 0 ; mantissaIndex < longLength - 1 - checkIndex ; mantissaIndex++)
            {
                aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex + checkIndex];
            }
            longLength -= checkIndex;
            sExponent1 -= checkIndex;

            //���
            //���� 0
            mantissaIndex = longLength - 2;

            //BUG-fix 4733 - UMR remove
            while( mantissaIndex >= 0 )
            {
                if( aValue->mantissa[mantissaIndex] == 0 )
                {
                    longLength--;
                    mantissaIndex--;
                }
                else
                {
                    break;
                }
            }

            aValue->signExponent = sExponent1 + 192;
        }
        else
        {
            //���� 99�� �����ش�
            for( mantissaIndex = 0 ; mantissaIndex < longLength - 1 ; mantissaIndex++ )
            {
                if( aValue->mantissa[mantissaIndex] == 99 )
                {
                    checkIndex++;
                }
                else
                {
                    break;
                }
            }
            for( mantissaIndex = 0 ; mantissaIndex < longLength - 1 - checkIndex ; mantissaIndex++)
            {
                aValue->mantissa[mantissaIndex] = aValue->mantissa[mantissaIndex + checkIndex];
            }
            longLength -= checkIndex;
            sExponent1 -= checkIndex;

            //����
            //���� 99
            mantissaIndex = longLength - 2;

            //BUG-fix 4733 UMR remove
            while( mantissaIndex > 0 )
            {
                if( aValue->mantissa[mantissaIndex] == 99 )
                {
                    mantissaIndex--;
                    longLength--;
                }
                else
                {
                    break;
                }
            }

            //-0�� �Ǹ�
            if( longLength == 1 )
            {
                aValue->signExponent = 0x80;
            }
            else
            {
                aValue->signExponent = 64 - sExponent1;
            }
        }

        // To fix BUG-12970
        // �ִ� ���̰� �Ѵ� ��� ������ ��� ��.
        if( longLength > 21 )
        {
            aValue->length = 21;
        }
        else
        {
            aValue->length = longLength;
        }

        //BUGBUG
        //precision���(��ȿ���� , length�� �����ؼ�)
        //precision�ڸ����� �ݿø����ش�.
        //aPrecisionMaximum
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_OVERFLOW );
    aciSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW );

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    ACI_RC mtc::subtractFloat()
 *
 * Argument :
 *    aValue - point of value , ��� �Ǿ����� ��
 *    aArgument1
 *    aArgument2 - �Է°� aValue = aArgument1 - aArgument2
 *    aPrecisionMaximum - �ڸ���
 *
 * Description :
 *    1. exponent���
 *    2. �ڸ������ ���� ; borrow ���� �����ϰ�� 99�� ������ ���
 *    3. �Ǿ��� ĳ���� �ڸ���+1 ���ش�
 * ---------------------------------------------------------------------------*/

ACI_RC mtcSubtractFloat( mtdNumericType *aValue,
                         acp_uint32_t    aPrecisionMaximum,
                         mtdNumericType *aArgument1,
                         mtdNumericType *aArgument2 )
{
    acp_sint32_t sBorrow;
    acp_sint32_t sExponent1;
    acp_sint32_t sExponent2;

    acp_sint32_t mantissaIndex;
    acp_sint32_t maxLength;
    acp_sint32_t sArg1Length;
    acp_sint32_t sArg2Length;
    acp_sint32_t sDiffExp;

    acp_sint32_t sArg1Index;
    acp_sint32_t sArg2Index;
    acp_sint32_t lIndex;
    acp_sint32_t sIndex;

    acp_sint32_t isZero;

    acp_sint32_t sArgExponent1;
    acp_sint32_t sArgExponent2;

    mtdNumericType* longArg;
    mtdNumericType* shortArg;
    acp_sint32_t longLength;
    acp_sint32_t shortLength;
    acp_sint32_t tempLength;

    acp_sint32_t i;

    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
    }
    else if( aArgument1->length == 1 )
    {
        aValue->length = aArgument2->length;

        //��ȣ �ٲٰ� ����( + -> - )
        aValue->signExponent = 256 - aArgument2->signExponent;

        for( mantissaIndex = 0 ;
             mantissaIndex < aArgument2->length - 1 ;
             mantissaIndex++ )
        {
            aValue->mantissa[mantissaIndex] =
                99 - aArgument2->mantissa[mantissaIndex];
        }

    }
    else if( aArgument2->length == 1 )
    {
        aValue->length = aArgument1->length;
        aValue->signExponent = aArgument1->signExponent;
        
        acpMemCpy( aValue->mantissa ,
                   aArgument1->mantissa ,
                   aArgument1->length - 1);
    }

    else
    {
        sArgExponent1 = aArgument1->signExponent & 0x80;
        sArgExponent2 = aArgument2->signExponent & 0x80;

        //exponent
        if( sArgExponent1 == 0x80 )
        {
            sExponent1 = (aArgument1->signExponent & 0x7F) - 64;
        }
        else
        {
            sExponent1 = 64 - (aArgument1->signExponent & 0x7F);
        }

        if( sArgExponent2 == 0x80 )
        {
            sExponent2 = (aArgument2->signExponent & 0x7F) - 64;
        }
        else
        {
            sExponent2 = 64 - (aArgument2->signExponent & 0x7F);
        }


        // To fix BUG-13569 addFloat�� ����
        // long argument�� short argument�� ����.
        //maxLength
        if( sExponent1 < sExponent2 )
        {
            sDiffExp = sExponent2 - sExponent1;
            tempLength = aArgument1->length + sDiffExp;
            sExponent1 = sExponent2;

            if( tempLength > aArgument2->length )
            {
                longLength = tempLength;
                shortLength = aArgument2->length;
                longArg = aArgument1;
                shortArg = aArgument2;
            }
            else
            {
                longLength = aArgument2->length;
                shortLength = tempLength;
                longArg = aArgument2;
                shortArg = aArgument1;
            }
        }
        else
        {
            sDiffExp = sExponent1 - sExponent2;
            tempLength = aArgument2->length + sDiffExp;

            if( tempLength > aArgument1->length )
            {
                longLength = tempLength;
                shortLength = aArgument1->length;
                longArg = aArgument2;
                shortArg = aArgument1;
            }
            else
            {
                longLength = aArgument1->length;
                shortLength = tempLength;
                longArg = aArgument1;
                shortArg = aArgument2;
            }
        }

        //�Ű����� 0���� ä�������� 21���� ������
        if( longLength - longArg->length > (acp_sint32_t)(aPrecisionMaximum / 2 + 2) )
        {
            if( shortArg == aArgument1 )
            {
                aValue->length = shortArg->length;
                aValue->signExponent = shortArg->signExponent;

                acpMemCpy( aValue->mantissa ,
                           shortArg->mantissa ,
                           shortArg->length);
                return ACI_SUCCESS;
            }
            else
            {
                aValue->length = shortArg->length;

                aValue->signExponent = 256 - shortArg->signExponent;

                for( mantissaIndex = 0 ;
                     mantissaIndex < shortArg->length - 1 ;
                     mantissaIndex++ )
                {
                    aValue->mantissa[mantissaIndex] =
                        99 - shortArg->mantissa[mantissaIndex];
                }
            }
        }

        if( longLength > (acp_sint32_t) (aPrecisionMaximum / 2 + 2) )
        {
            // BUG-10557 fix
            // maxLength�� 21�� ���� ��, 21�� ���̴µ�
            // �̶�, argument1 �Ǵ� argument2�� ���꿡 ������ ������
            // maxLength�� �پ�� ��ŭ ����� �Ѵ�.
            //
            // ���� ��� 158/10000*17/365 + 1�� ����� ��,
            // 158/10000*17/365�� 0.00073589... (length=20) �ε�,
            // maxLength�� 22���� 21�� ���϶�,
            // 0.00073589...�� ���̸� 19�� �ٿ� ������ ���ڸ��� �����ؾ� �Ѵ�.
            // subtractFloat������ ���������� �̷� ó���� ���־�� �Ѵ�.
            //
            lIndex = longArg->length - 2 - (longLength - 21);
            longLength = 21;
        }
        else
        {
            lIndex = longArg->length - 2;
        }

        maxLength = longLength;


        sIndex = shortArg->length - 2;


        if( longArg == aArgument1 )
        {
            sArg1Index = lIndex;
            sArg1Length = longLength;
            sArg2Index = sIndex;
            sArg2Length = shortLength;
        }
        else
        {
            sArg1Index = sIndex;
            sArg1Length = shortLength;
            sArg2Index = lIndex;
            sArg2Length = longLength;
        }

        sBorrow = 0;
        //����

        //     1  : 2 :   3
        //   xxxxxxxxx________
        //-  ______xxxxxxxxxxx
        //�׸��� ��ġ�� �ʴ� ����
        //������� POSITIVE : 0
        //         NEGATIVE : 99

        for( mantissaIndex = maxLength - 2 ;
             mantissaIndex >= 0 ;
             mantissaIndex-- )
        {
            //3����
            if( sArg1Length > sArg2Length && sArg1Index >= 0)
            {
                if( sArgExponent2 == 0x00 )
                {
                    if( aArgument1->mantissa[sArg1Index] - sBorrow < 99 )
                    {
                        aValue->mantissa[mantissaIndex] =
                            (100 + aArgument1->mantissa[sArg1Index]- sBorrow)
                            - 99 ;
                        sBorrow = 1;
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex] = 0;
                        sBorrow = 0;
                    }
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        aArgument1->mantissa[sArg1Index];
                }
                sArg1Index--;
                sArg1Length--;
            }
            else if( sArg1Length < sArg2Length && sArg2Index >= 0)
            {
                //�����
                if( sArgExponent1 == 0x80 )
                {
                    aValue->mantissa[mantissaIndex] =
                        100 - sBorrow - aArgument2->mantissa[sArg2Index];
                    sBorrow = 1;
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        99 - aArgument2->mantissa[sArg2Index];
                    sBorrow = 0;
                }
                sArg2Index--;
                sArg2Length--;
            }
            //��ġ�� ����(1�� ��ų� 2�� ��ų�)
            else if( (sArg1Index == (sArg2Index + sDiffExp) ||
                      sArg2Index == (sArg1Index + sDiffExp)) &&
                     sArg1Index >= 0 && sArg2Index >= 0)
            {
                if( aArgument1->mantissa[sArg1Index] <
                    aArgument2->mantissa[sArg2Index] + sBorrow )
                {
                    aValue->mantissa[mantissaIndex] =
                        (100 + aArgument1->mantissa[sArg1Index]) -
                        aArgument2->mantissa[sArg2Index] - sBorrow;
                    sBorrow = 1;
                }
                else
                {
                    aValue->mantissa[mantissaIndex] =
                        aArgument1->mantissa[sArg1Index] -
                        aArgument2->mantissa[sArg2Index] - sBorrow;
                    sBorrow = 0;
                }
                sArg1Index--;
                sArg2Index--;
            }
            //��ġ�� �ʴ� ����
            else if( (sArg2Index < 0 && mantissaIndex > sArg1Index ) ||
                     (sArg1Index < 0 && mantissaIndex > sArg2Index ) )
            {
                if(sBorrow)
                {
                    //���
                    if( sArgExponent1 == 0x80 )
                    {
                        if( sArgExponent2 == 0x80 )
                        {
                            aValue->mantissa[mantissaIndex] = 99;
                        }
                        else
                        {
                            aValue->mantissa[mantissaIndex] = 0;
                        }
                        sBorrow = 1;

                    }
                    //����
                    else
                    {
                        if( sArgExponent2 == 0x80 )
                        {
                            aValue->mantissa[mantissaIndex] = 98;
                            sBorrow = 0;
                        }
                        else
                        {
                            aValue->mantissa[mantissaIndex] = 99;
                            sBorrow = 1;
                        }
                    }
                }
                else
                {
                    //���
                    if( sArgExponent1 == 0x80 )
                    {
                        if( sArgExponent2 == 0x80 )
                        {
                            aValue->mantissa[mantissaIndex] = 0;
                            sBorrow = 0;
                        }
                        else
                        {
                            aValue->mantissa[mantissaIndex] = 1;
                            sBorrow = 1;
                        }
                    }
                    //����
                    else
                    {
                        if( sArgExponent2 == 0x80 )
                        {
                            aValue->mantissa[mantissaIndex] = 99;
                        }
                        else
                        {
                            aValue->mantissa[mantissaIndex] = 0;
                        }
                        sBorrow = 0;
                    }
                }
            }
            //1����
            else if( sArg2Index < 0 )
            {
                //����
                if( sArgExponent2 == 0x00 )
                {
                    if( aArgument1->mantissa[sArg1Index] - sBorrow < 99 )
                    {

                        aValue->mantissa[mantissaIndex] =
                            (100 + aArgument1->mantissa[sArg1Index] - sBorrow)
                            - 99;
                        sBorrow = 1;
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex] = 0;
                        sBorrow = 0;
                    }
                }
                else
                {
                    if( aArgument1->mantissa[sArg1Index] < sBorrow )
                    {
                        aValue->mantissa[mantissaIndex] =
                            100 - aArgument1->mantissa[sArg1Index] - sBorrow;
                        sBorrow = 1;
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex] =
                            aArgument1->mantissa[sArg1Index] - sBorrow;
                        sBorrow = 0;
                    }
                }
                sArg1Index--;
            }
            else if( sArg1Index < 0 )
            {
                if( sArgExponent1 == 0x00 )
                {
                    if( 99 - sBorrow < aArgument2->mantissa[sArg2Index] )
                    {
                        aValue->mantissa[mantissaIndex] =
                            100 + 99 - aArgument2->mantissa[sArg2Index]
                            - sBorrow;
                        sBorrow = 1;
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex] =
                            99 - aArgument2->mantissa[sArg2Index] - sBorrow;
                        sBorrow = 0;
                    }
                }
                else
                {
                    if( sBorrow )
                    {
                        aValue->mantissa[mantissaIndex] =
                            99 - aArgument2->mantissa[sArg2Index];
                    }
                    else
                    {
                        if( aArgument2->mantissa[sArg2Index] == 0 )
                        {
                            aValue->mantissa[mantissaIndex] = 0;

                        }
                        else
                        {
                            aValue->mantissa[mantissaIndex] =
                                100 - aArgument2->mantissa[sArg2Index];
                            sBorrow = 1;
                        }
                    }
                }
                sArg2Index--;
            }
        }

        ACI_TEST_RAISE( sExponent1 > 63 , ERR_OVERFLOW);

        if( sArgExponent1 == sArgExponent2 )
        {
            if( sBorrow )
            {
                //���
                for( mantissaIndex = maxLength - 2 ;
                     mantissaIndex >= 0 ;
                     mantissaIndex-- )
                {
                    if( aValue->mantissa[mantissaIndex] == 0 )
                    {
                        aValue->mantissa[mantissaIndex] = 99;
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex]--;
                        break;
                    }
                }


                //�Ǿ��� �ٷο� �߻��޴µ� 99�ϰ�츸 �� �����ؼ� 0�� ���ö�
                if( aValue->mantissa[0] == 99 )
                {
                    acp_sint16_t checkIndex = 0;
                    //���� 99�� �����ش�
                    for( mantissaIndex = 0 ;
                         mantissaIndex < maxLength - 1 ;
                         mantissaIndex++ )
                    {
                        if( aValue->mantissa[mantissaIndex] == 99 )
                        {
                            checkIndex++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    for( mantissaIndex = 0 ;
                         mantissaIndex < maxLength - 1 - checkIndex ;
                         mantissaIndex++)
                    {
                        aValue->mantissa[mantissaIndex] =
                            aValue->mantissa[mantissaIndex + checkIndex];
                    }
                    maxLength -= checkIndex;
                    sExponent1 -= checkIndex;

                }
                aValue->signExponent = (64 - sExponent1);

            }
            else
            {

                //������ �ȴ�.
                isZero = 0;
                mantissaIndex = 0;

                while(mantissaIndex < maxLength - 1)
                {
                    if( aValue->mantissa[mantissaIndex] == 0 )
                    {
                        mantissaIndex++;
                        isZero++;
                    }
                    else
                    {
                        break;
                    }
                }



                //0�϶�
                if( isZero == maxLength - 1)
                {
                    aValue->length = 1;
                    aValue->signExponent = 0x80;
                    return ACI_SUCCESS;
                }
                else if( isZero )
                {
                    // fix BUG-10360 valgrind error remove

                    // idlOS::memcpy(aValue->mantissa ,
                    //               aValue->mantissa + isZero ,
                    //               maxLength - isZero);
                    for( i = 0; i < ( maxLength - isZero) ; i++ )
                    {
                        *(aValue->mantissa + i)
                            = *(aValue->mantissa + isZero + i);
                    }

                    maxLength = maxLength - isZero ;
                    sExponent1 -= isZero;
                }

                aValue->signExponent = (sExponent1 + 192);

            }
        }
        else
        {
            //���

            if( sArgExponent1 == 0x80 )
            {

                for( mantissaIndex = maxLength - 2 ;
                     mantissaIndex >= 0 ;
                     mantissaIndex-- )
                {
                    if( aValue->mantissa[mantissaIndex] == 0 )
                    {
                        aValue->mantissa[mantissaIndex] = 99;
                        if( mantissaIndex == 0 )
                        {
                            sBorrow = 1;
                        }
                    }
                    else
                    {
                        aValue->mantissa[mantissaIndex]--;
                        break;
                    }
                }
                if( !sBorrow ){
                    for( mantissaIndex = maxLength - 1 ;
                         mantissaIndex > 0 ;
                         mantissaIndex-- )
                    {
                        aValue->mantissa[mantissaIndex] =
                            aValue->mantissa[mantissaIndex - 1];
                    }
                    aValue->mantissa[0] = 1;
                    sExponent1++;
                    maxLength++;
                }

                aValue->signExponent = (sExponent1 + 192);

            }
            else
            {
                if( sBorrow ){
                    //98������
                    for( mantissaIndex = maxLength - 1 ;
                         mantissaIndex > 0 ;
                         mantissaIndex-- )
                    {
                        aValue->mantissa[mantissaIndex] =
                            aValue->mantissa[mantissaIndex - 1];
                    }
                    aValue->mantissa[0] = 98;
                    sExponent1++;
                    maxLength++;
                }
                aValue->signExponent = (64 - sExponent1);

            }
        }

        // BUG-12334 fix
        // '-' ���� ��� ���ڸ��� 99�� ������ ��� nomalize�� �ʿ��ϴ�.
        // ���� ���,
        // -220 - 80 �ϸ�
        // -220: length=3, exponent=62,  mantissa=97,79
        // 80  : length=2, exponent=193, mantissa=80
        // ���: length=3, exponent=62,  mantissa=96,99
        // �ε�, -330�� length=2, exponent=62, mantissa=96�̴�.
        // ��������� �������� 99, 0�� ���ְ� length�� 1 �ٿ��� �Ѵ�.
        // ����� ��쿣 0�� ������ ��쿣 99�� ���ش�.
        // by kumdory, 2005-07-11
        if( ( aValue->signExponent & 0x80 ) > 0 )
        {
            for( mantissaIndex = maxLength - 2;
                 mantissaIndex > 0;
                 mantissaIndex-- )
            {
                if( aValue->mantissa[mantissaIndex] == 0 )
                {
                    maxLength--;
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            for( mantissaIndex = maxLength - 2;
                 mantissaIndex > 0;
                 mantissaIndex-- )
            {
                if( aValue->mantissa[mantissaIndex] == 99 )
                {
                    maxLength--;
                }
                else
                {
                    break;
                }
            }
        }

/*
  �� ��
  �ٷο� �߻� -> -1
  �ٷο� ���� -> 0

  �� ��
  �ٷο� -> -1
  �ٷο� ���� -> 0


  ����
  �ٷο� -> -1
  �ٷο� ���� -> -1


  ����
  �ٷο� -> 0 ,98 ������
  �ٷο� ���� -> 0

  //�ڿ� 0�� �����ؼ� �ð��

*/
        //BUGBUG
        //precision���(��ȿ���� , length�� �����ؼ�)
        //precision�ڸ����� �ݿø����ش�.

        // To fix BUG-12970
        // �ִ� ���̰� �Ѿ�� ��� ��������� ��.
        if( maxLength > 21 )
        {
            aValue->length = 21;
        }
        else
        {
            aValue->length = maxLength;
        }

    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_OVERFLOW );
    aciSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW );

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    ACI_RC mtc::multiplyFloat()
 *
 * Argument :
 *    aValue - point of value , ��� �Ǿ����� ��
 *    aArgument1
 *    aArgument2 - �Է°� aValue = aArgument1 * aArgument2
 *    aPrecisionMaximum - �ڸ���
 *
 * Description :
 *    ��� algorithm : grade school multiplication( �ϸ� �ʻ��)
 *
 *    1. exponent���
 *    2. �ڸ������ ���� ; �����ϰ�� 99�� ������ ���
 *    2.1 �ӵ��� ������ �ϱ� ���� ��*��, ��*��, ��*�� �� ��츦 ���� �����
 *    3. ���ڸ��� ���ؼ� �����ش�.
 *    4. ĳ�� ����� �� �������� �Ѵ�.(���Ҷ����� �ϰԵǸ� carry�� cascade�� ��� ��ȿ������)
 *    5. maximum mantissa(20)�� �Ѿ�� ����
 *    6. exponent�� -63���ϸ� 0, 63�̻��̸� overflow
 *    7. ������ 99�� ���� ����
 * ---------------------------------------------------------------------------*/
ACI_RC mtcMultiplyFloat( mtdNumericType *aValue,
                         acp_uint32_t    aPrecisionMaximum,
                         mtdNumericType *aArgument1,
                         mtdNumericType *aArgument2 )
{
    acp_sint32_t sResultMantissa[40] = { 0, };   // ����� �ִ� 80�ڸ����� ���� �� �ִ�.
    acp_sint32_t i;                     // for multiplication
    acp_sint32_t j;                     // for multiplication
    acp_sint32_t sCarry;
    acp_sint32_t sResultFence;
    acp_sint16_t sArgExponent1;
    acp_sint16_t sArgExponent2;
    acp_sint16_t sResultExponent;
    acp_sint16_t sMantissaStart;

    ACP_UNUSED(aPrecisionMaximum);

    // �ϳ��� null�� ��� nulló��
    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
        aValue->signExponent = 0;
        return ACI_SUCCESS;
    }
    // �ϳ��� 0�� ��� 0����
    else if( aArgument1->length == 1 || aArgument2->length == 1 )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
        return ACI_SUCCESS;
    }

    // result mantissa�ִ� ���� ����
    sResultFence = aArgument1->length + aArgument2->length - 2;
    // result mantissa�� 0���� �ʱ�ȭ
    acpMemSet( sResultMantissa, 0, sizeof(acp_uint32_t) * sResultFence );


    // ������� �������� �κи� ����
    sArgExponent1 = aArgument1->signExponent & 0x80;
    sArgExponent2 = aArgument2->signExponent & 0x80;

    //exponent ���ϱ�
    if( sArgExponent1 == 0x80 )
    {
        sResultExponent = (aArgument1->signExponent & 0x7F) - 64;
    }
    else
    {
        sResultExponent = 64 - (aArgument1->signExponent & 0x7F);
    }

    if( sArgExponent2 == 0x80 )
    {
        sResultExponent += (aArgument2->signExponent & 0x7F) - 64;
    }
    else
    {
        sResultExponent += 64 - (aArgument2->signExponent & 0x7F);
    }

    // if�� �񱳸� �ѹ��� �ϱ� ���� ��� ��쿡 ���� ���
    // �� * ��
    if( (sArgExponent1 != 0) && (sArgExponent2 != 0) )
    {
        for(i = aArgument1->length-2; i >= 0; i--)
        {
            for(j = aArgument2->length-2; j >= 0; j--)
            {
                sResultMantissa[i + j + 1] +=
                    (acp_sint32_t)aArgument1->mantissa[i] *
                    (acp_sint32_t)aArgument2->mantissa[j];
            }
        }
    }
    // �� * ��
    else if( (sArgExponent1 != 0) /*&& (sArgExponent2 == 0)*/ )
    {
        for(i = aArgument1->length-2; i >= 0; i--)
        {
            for(j = aArgument2->length-2; j >= 0; j--)
            {
                sResultMantissa[i + j + 1] +=
                    (acp_sint32_t)aArgument1->mantissa[i] *
                    (acp_sint32_t)( 99 - aArgument2->mantissa[j] );
            }
        }
    }
    // �� * ��
    else if( /*(sArgExponent1 == 0) &&*/ (sArgExponent2 != 0) )
    {
        for(i = aArgument1->length-2; i >= 0; i--)
        {
            for(j = aArgument2->length-2; j >= 0; j--)
            {
                sResultMantissa[i + j + 1] +=
                    (acp_sint32_t)( 99 - aArgument1->mantissa[i] ) *
                    (acp_sint32_t)aArgument2->mantissa[j];
            }
        }
    }
    // �� * ��
    else
    {
        for(i = aArgument1->length-2; i >= 0; i--)
        {
            for(j = aArgument2->length-2; j >= 0; j--)
            {
                sResultMantissa[i + j + 1] +=
                    (acp_sint32_t)( 99 - aArgument1->mantissa[i] ) *
                    (acp_sint32_t)( 99 - aArgument2->mantissa[j] );
            }
        }
    }

    // carry���
    for( i = sResultFence - 1; i > 0; i-- )
    {
        sCarry = sResultMantissa[i] / 100;
        sResultMantissa[i-1] += sCarry;
        sResultMantissa[i] -= sCarry * 100;
    }

    // �� Zero trim
    if( sResultMantissa[0] == 0 )
    {
        sMantissaStart = 1;
        sResultExponent--;
    }
    else
    {
        sMantissaStart = 0;
    }

    // �� Zero trim
    for( i = sResultFence - 1; i >= 0; i-- )
    {
        if( sResultMantissa[i] == 0 )
        {
            sResultFence--;
        }
        else
        {
            break;
        }
    }

    if( ( sResultFence - sMantissaStart ) > 20 )
    {
        sResultFence = 20 + sMantissaStart;
    }
    else
    {
        // Nothing to do.
    }

    // exponent�� 63�� ������ ����(�ִ� 126�̹Ƿ�)
    ACI_TEST_RAISE( sResultExponent > 63, ERR_VALUE_OVERFLOW );

    // exponent�� -63(-126)���� ������ 0�� ��
    if( sResultExponent < -63 )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
    }
    else
    {
        // ���� mantissa�� �� ����
        // ��ȣ�� ���� ���
        if( sArgExponent1 == sArgExponent2 )
        {
            aValue->signExponent = sResultExponent + 192;
            for( i = 0, j = sMantissaStart; j < sResultFence; i++, j++ )
            {
                aValue->mantissa[i] = sResultMantissa[j];
            }
            aValue->length = sResultFence - sMantissaStart + 1;
        }
        // ��ȣ�� �ٸ� ���
        else
        {
            aValue->signExponent = 64 - sResultExponent;
            for( i = 0, j = sMantissaStart; j < sResultFence; i++, j++ )
            {
                aValue->mantissa[i] = 99 - sResultMantissa[j];
            }
            aValue->length = sResultFence - sMantissaStart + 1;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    ACI_RC mtc::divideFloat()
 *
 * Argument :
 *    aValue - point of value , ��� �Ǿ����� ��
 *    aArgument1
 *    aArgument2 - �Է°� aValue = aArgument1 / aArgument2
 *    aPrecisionMaximum - �ڸ���
 *
 * Description :
 *    ��� algorithm : David M. Smith�� ��
 *                    'A Multiple-Precision Division Algorithm'
 *
 *    1. exponent���
 *    2. ������, ������ integer mantissa array�� ����.(��� ����� normalization)
 *    2.1 �������� ��ĭ ������ ����Ʈ�ؼ� ����.(���� ��ĭ���� ���� ����ȴ�)
 *    3. ������ ���� �����ϱ� ���� �� ����
 *    3.1 ������, �������� ���� 4�ڸ��� ���� integer���� ����
 *    3.2 �������� ����
 *    4. ������ = ������ - ���� * ������(�������� ���� �׳� ��)
 *    5. ���� maximum mantissa + 1�� �ɶ����� 3~4 �ݺ�
 *    6. ���� normalization
 *
 * ---------------------------------------------------------------------------*/
ACI_RC mtcDivideFloat( mtdNumericType *aValue,
                       acp_uint32_t    aPrecisionMaximum,
                       mtdNumericType *aArgument1,
                       mtdNumericType *aArgument2 )
    
{
    acp_sint32_t sMantissa1[42];        // ������ �� �� ����.
    acp_uint8_t  sMantissa2[20] = {0,}; // ���� ����.(�������)
    acp_sint32_t sMantissa3[21];        // ���� * �κи�. �������� ū ��� ���.(�ִ� 1byte)

    acp_sint32_t sNumerator;           // ������ ���ð�.
    acp_sint32_t sDenominator;         // ���� ���ð�.
    acp_sint32_t sPartialQ;            // �κи�.

    acp_sint32_t i;                    // for Iterator
    acp_sint32_t j;                    // for Iterator
    acp_sint32_t k;                    // for Iterator

    acp_sint32_t sResultFence;

    acp_sint32_t sStep;
    acp_sint32_t sLastStep;

    acp_sint16_t sArgExponent1;
    acp_sint16_t sArgExponent2;
    acp_sint16_t sResultExponent;

    acp_uint8_t * sMantissa2Ptr;
    acp_sint32_t  sCarry;
    acp_sint32_t  sMantissaStart;

    ACP_UNUSED(aPrecisionMaximum);
    
    // �ϳ��� null�̸� null
    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
        return ACI_SUCCESS;
    }
    // 0���� ���� ���
    else if ( aArgument2->length == 1 )
    {
        ACI_RAISE(ERR_DIVIDE_BY_ZERO);
    }
    // 0�� ���� ���
    else if ( aArgument1->length == 1 )
    {
        aValue->length = 1;
        aValue->signExponent = 0x80;
        return ACI_SUCCESS;
    }

    // ������� �������� �κи� ����
    sArgExponent1 = aArgument1->signExponent & 0x80;
    sArgExponent2 = aArgument2->signExponent & 0x80;


    // ������ ����.
    // ������ ��� ���·� �ٲپ� mantissa����
    sMantissa1[0] = 0;

    if( sArgExponent1 == 0x80 )
    {
        // �������� ��ĭ���� ����(0��° ĭ���� ���� ����ɰ���)
        for( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa1[i+1] = (acp_sint32_t)aArgument1->mantissa[i];
        }
        acpMemSet( &sMantissa1[i+1], 0, sizeof(acp_sint32_t) * (42 - i - 1) );
    }
    else
    {
        // �������� ��ĭ���� ����(0��° ĭ���� ���� ����ɰ���)
        for( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa1[i+1] = 99 - (acp_sint32_t)aArgument1->mantissa[i];
        }

        acpMemSet( &sMantissa1[i+1], 0, sizeof(acp_sint32_t) * (42 - i - 1) );
    }

    // ���� ����.
    // ������ ����϶��� �������.
    if( sArgExponent2 == 0x80 )
    {
        // mantissaptr�� ���� aArgument2->mantissa�� ����Ŵ.
        sMantissa2Ptr = aArgument2->mantissa;
    }
    else
    {
        for( i = 0; i < aArgument2->length - 1; i++ )
        {
            sMantissa2[i] = 99 - (acp_sint32_t)aArgument2->mantissa[i];
        }
        sMantissa2Ptr = sMantissa2;
    }

    //exponent ���ϱ�
    if( sArgExponent1 == 0x80 )
    {
        sResultExponent = (aArgument1->signExponent & 0x7F) - 64;
    }
    else
    {
        sResultExponent = 64 - (aArgument1->signExponent & 0x7F);
    }

    if( sArgExponent2 == 0x80 )
    {
        sResultExponent -= (aArgument2->signExponent & 0x7F) - 64 - 1;
    }
    else
    {
        sResultExponent -= 64 - (aArgument2->signExponent & 0x7F) - 1;
    }

    // ���� ���ð� ���ϱ�(�ִ� 3�ڸ����� ���ð��� ���Ͽ� ������ �ּ�ȭ �Ѵ�.)
    if( aArgument2->length > 3 )
    {
        sDenominator = (acp_sint32_t)sMantissa2Ptr[0] * 10000 +
            (acp_sint32_t)sMantissa2Ptr[1] * 100 +
            (acp_sint32_t)sMantissa2Ptr[2];
    }
    else if ( aArgument2->length > 2 )
    {
        sDenominator = (acp_sint32_t)sMantissa2Ptr[0] * 10000 +
            (acp_sint32_t)sMantissa2Ptr[1] * 100;
    }
    else
    {
        sDenominator = (acp_sint32_t)sMantissa2Ptr[0] * 10000;
    }
    ACI_TEST_RAISE(sDenominator == 0, ERR_DIVIDE_BY_ZERO);

    sLastStep = 20 + 2; // �ִ� 21�ڸ����� ���� �� �ֵ���

    for( sStep = 1; sStep < sLastStep; sStep++ )
    {
        // ������ ���ð� ���ϱ�(�������� �ڸ����� �ִ� 42�ڸ��̱� ������ abr�� �� �� ����)
        sNumerator = sMantissa1[sStep] * 1000000 +
            sMantissa1[sStep+1] * 10000 +
            sMantissa1[sStep+2] * 100 +
            sMantissa1[sStep+3];

        // �κи��� ����.
        sPartialQ = sNumerator / sDenominator;

        // �κи��� 0�� �ƴ϶�� ������ - (����*�κи�)
        if( sPartialQ != 0 )
        {
            // sPartialQ * sMantissa2�� ���� sMantissa1���� ����.
            // �ڸ����� ���缭 ���� ��. �������� ���� ���X
            // sPartialQ * sMantissa2
            sMantissa3[0] = 0;

            // ����*�κи�
            for(k = aArgument2->length-2; k >= 0; k--)
            {
                sMantissa3[k+1] = (acp_sint32_t)sMantissa2Ptr[k] * sPartialQ;
            }

            sCarry = 0;

            /** BUG-42773
             * 99.999 / 111.112 �� ������ �Ǹ� sPartialQ = 9000 �ε�
             * sMantissa3�� sCaary�� �ϸ� 100������ �ѰԵǴµ� �̿� ���õ�
             * ó���� ������.
             * �� 100 �������� carry�� �迭�� ������ ���� 0 �� carry�� �ϴµ�
             * �������� 100 ������ �Ѿ carry����  1 �� ���Ҵµ�
             * �̿� ���õ� ó���� ��� ���õǸ鼭 ����� ���̰� �ȴ�.
             * ���� ������ 0�� carry���� �ʰ� �״�� ó���Ѵ�.
             */
            // ���������� carry���
            for ( k = aArgument2->length - 1 ; k > 0; k-- )
            {
                sMantissa3[k] += sCarry;
                sCarry = sMantissa3[k] / 100;
                sMantissa3[k] -= sCarry * 100;
            }
            sMantissa3[0] += sCarry;

            // ������ - (����*�κи�)
            for( k = 0; k < aArgument2->length; k++ )
            {
                sMantissa1[sStep + k] -=  sMantissa3[k];
            }

            sMantissa1[sStep+1] +=sMantissa1[sStep]*100;
            sMantissa1[sStep] = sPartialQ;
        }
        // �κи��� 0�̶�� ���� �׳� 0
        else
        {
            sMantissa1[sStep+1] +=sMantissa1[sStep]*100;
            sMantissa1[sStep] = sPartialQ;
        }
    }

    // Final Normalization
    // carry�� �߸��� ���� normalization�Ѵ�.
    // 22�ڸ����� ����� �������Ƿ� resultFence�� 21��.
    sResultFence = 21;

    for( i = sResultFence; i > 0; i-- )
    {
        if( sMantissa1[i] >= 100 )
        {
            sCarry = sMantissa1[i] / 100;
            sMantissa1[i-1] += sCarry;
            sMantissa1[i] -= sCarry * 100;
        }
        else if ( sMantissa1[i] < 0 )
        {
            // carry�� 100������ �������� �� ó���� ����
            // +1�� �� ������ /100�� �ϰ� ���⿡ -1�� ��
            // ex) -100 => carry�� -1, -101 => carry�� -2
            //     -99  => carry�� -1
            sCarry = ((sMantissa1[i] + 1) / 100) - 1;
            sMantissa1[i-1] += sCarry;
            sMantissa1[i] -= sCarry * 100;
        }
        else
        {
            // Nothing to do.
        }
    }

    // mantissa1[0]�� 0�� ��� resultExponent�� 1 �پ��.
    // ex) �������� ù��° �ڸ����� ������ ���������� ū �����.
    //     1234 / 23 -> ù°�ڸ� ������ ���� 0��
    if( sMantissa1[0] == 0 )
    {
        sMantissaStart = 1;
        sResultExponent--;
    }
    // 0�� �ƴ� ��� ���� �����ߴ� �ڸ����� ���ڸ��� �����Ƿ� ���ڸ� ����
    else
    {
        sMantissaStart = 0;
        sResultFence--;
    }

    // �� Zero Trim
    for( i = sResultFence - 1; i > 0; i-- )
    {
        if( sMantissa1[i] == 0 )
        {
            sResultFence--;
        }
        else
        {
            break;
        }
    }

    if( ( sResultFence - sMantissaStart ) > 20 )
    {
        sResultFence = 20 + sMantissaStart;
    }
    else
    {
        // Nothing to do.
    }

    // exponent�� 63�� ������ ����(�ִ� 126�̹Ƿ�)
    ACI_TEST_RAISE( sResultExponent > 63, ERR_VALUE_OVERFLOW );

    // exponent�� -63(-126)���� ������ 0�� ��
    if( sResultExponent < -63 )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
    }
    else
    {
        // ��ȣ�� ���� ���
        if( sArgExponent1 == sArgExponent2 )
        {
            aValue->signExponent = sResultExponent + 192;
            for( i = 0, j = sMantissaStart; j < sResultFence; i++, j++ )
            {
                aValue->mantissa[i] = sMantissa1[j];
            }
            aValue->length = sResultFence - sMantissaStart + 1;
        }
        // ��ȣ�� �ٸ� ���
        else
        {
            aValue->signExponent = 64 - sResultExponent;
            for( i = 0, j = sMantissaStart; j < sResultFence; i++, j++ )
            {
                aValue->mantissa[i] = 99 - sMantissa1[j];
            }
            aValue->length = sResultFence - sMantissaStart + 1;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_DIVIDE_BY_ZERO );
    {
        aciSetErrorCode( mtERR_ABORT_DIVIDE_BY_ZERO );
    }
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtcModFloat( mtdNumericType * aTemp,
                    acp_uint32_t     aTemp2,
                    mtdNumericType * aTemp3,
                    mtdNumericType * aTemp4)
{
    ACP_UNUSED(aTemp);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    ACP_UNUSED(aTemp4);
    
    return ACI_SUCCESS;
}

ACI_RC mtcGetPrecisionScaleFloat( const mtdNumericType* aValue,
                                  acp_sint32_t        * aPrecision,
                                  acp_sint32_t        * aScale )
{
// To fix BUG-12944
// precision�� scale�� ��Ȯ�� �����ִ� �Լ�.
// ���� maximum precision�� �Ѵ� ���� �ִ� 39, 40�̹Ƿ�,
// precision, scale�� ������ 38�� �Ѱ��ش�.(�ִ� mantissa�迭 ũ�⸦ ���� �����Ƿ� ������)
// ������ ������. return IDE_SUCCESS;
    acp_sint32_t sExponent;

    if( aValue->length > 1 )
    {
        sExponent  = (aValue->signExponent&0x7F) - 64;
        if( (aValue->signExponent & 0x80) == 0x00 )
        {
            sExponent *= -1;
        }

        *aPrecision = (aValue->length-1)*2;
        *aScale = *aPrecision - (sExponent << 1);

        if( (aValue->signExponent & 0x80) == 0x00 )
        {
            if( (99 - *(aValue->mantissa)) / 10 == 0 )
            {
                (*aPrecision)--;
            }
            else
            {
                // Nothing to do.
            }

            if( (99 - *(aValue->mantissa + aValue->length - 2)) % 10 == 0 )
            {
                (*aPrecision)--;
                (*aScale)--;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if( *(aValue->mantissa) / 10 == 0 )
            {
                (*aPrecision)--;
            }
            else
            {
                // Nothing to do.
            }

            if( *(aValue->mantissa + aValue->length - 2) % 10 == 0 )
            {
                (*aPrecision)--;
                (*aScale)--;
            }
            else
            {
                // Nothing to do.
            }
        }


        if( (*aPrecision) > MTD_NUMERIC_PRECISION_MAXIMUM )
        {
            // precision�� �����ϸ鼭 scale�� ���������� �����ؾ� ��.
            (*aScale)  -= (*aPrecision) - MTD_NUMERIC_PRECISION_MAXIMUM;
            (*aPrecision) = MTD_NUMERIC_PRECISION_MAXIMUM;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        *aPrecision = 1;
        *aScale = 0;
    }

    return ACI_SUCCESS;
}

acp_bool_t
mtcIsSamePhysicalImageByModule( const mtdModule* aModule,
                                const void*      aValue1,
                                const void*      aValue2)
{

    /***********************************************************************
     *
     * Description :
     *
     *    BUG-30257
     *    client�� �Լ��ν� physical Image�� �������� ���Ѵ�.
     *
     * Implementation :
     *
     *    NULL�� ��� Garbage Data�� ������ �� �����Ƿ� ��� NULL���� �Ǵ�
     *    Data�� ���̰� �������� �˻�
     *    Data ���̰� ������ ��� memcmp �� �˻�
     *
     ***********************************************************************/

    acp_bool_t        sResult;
    acp_uint32_t      sLength1;
    acp_uint32_t      sLength2;

    //------------------------------------------
    // Parameter Validation
    //------------------------------------------

    ACE_ASSERT( aModule != NULL );
    ACE_ASSERT( aValue1 != NULL );
    ACE_ASSERT( aValue2 != NULL );

    //------------------------------------------
    // Initialize
    //------------------------------------------

    sResult = ACP_FALSE;

    //------------------------------------------
    // Image Comparison
    //------------------------------------------

    // check both data is NULL
    if ( ( aModule->isNull( NULL,
                            aValue1,
                            MTD_OFFSET_USELESS ) == ACP_TRUE ) &&
         ( aModule->isNull( NULL,
                            aValue2,
                            MTD_OFFSET_USELESS ) == ACP_TRUE ) )
    {
        // both are NULL value
        sResult = ACP_TRUE;
    }
    else
    {
        // Data Length
        sLength1 = aModule->actualSize( NULL,
                                        aValue1,
                                        MTD_OFFSET_USELESS );
        sLength2 = aModule->actualSize( NULL,
                                        aValue2,
                                        MTD_OFFSET_USELESS );


        if ( sLength1 == sLength2 )
        {
            if ( acpMemCmp( aValue1,
                            aValue2,
                            sLength1 ) == 0 )
            {
                // same image
                sResult = ACP_TRUE;
            }
            else
            {
                // different image
            }
        }
        else
        {
            // different length
        }
    }

    return sResult;
}



ACI_RC mtcInitializeColumn( mtcColumn       *aColumn,
                            const mtdModule *aModule,
                            acp_uint32_t     aArguments,
                            acp_sint32_t     aPrecision,
                            acp_sint32_t     aScale )
{
/***********************************************************************
 *
 * Description : mtcColumn�� �ʱ�ȭ
 *              ( mtdModule �� mtlModule�� �������� ��� )
 *
 * Implementation :
 *
 ***********************************************************************/

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aModule);
    ACP_UNUSED(aArguments);
    ACP_UNUSED(aPrecision);
    ACP_UNUSED(aScale);

    return ACI_SUCCESS;
}

ACI_RC mtcMakeSmallint( mtdSmallintType*   aSmallint,
                        const acp_uint8_t* aString,
                        acp_uint32_t       aLength )
{
    acp_sint64_t  sLong;
    acp_uint8_t  *sEndPtr = NULL;
    acp_double_t  sTemp;
    
    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    acpCStrToDouble((const acp_char_t*)aString,
                    acpCStrLen((acp_char_t*)aString, aLength),
                    &sTemp,
                    (acp_char_t**)&sEndPtr);

    sLong = sTemp;

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sLong == 0 )
    {
        sLong = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    ACI_TEST_RAISE( sLong < MTD_SMALLINT_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    ACI_TEST_RAISE( sLong > MTD_SMALLINT_MAXIMUM,
                    ERR_VALUE_OVERFLOW );
    
    ACI_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    
    *aSmallint = (mtdSmallintType)sLong;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeInteger( mtdIntegerType*    aInteger,
                       const acp_uint8_t* aString,
                       acp_uint32_t       aLength )
{
    acp_sint64_t  sLong;
    acp_uint8_t  *sEndPtr = NULL;
    acp_double_t  sTemp;
    
    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    acpCStrToDouble( (const acp_char_t*)aString,
                     acpCStrLen((acp_char_t*)aString, aLength),
                     &sTemp,
                     (acp_char_t**)&sEndPtr);

    sLong = sTemp;
    
    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sLong == 0 )
    {
        sLong = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    ACI_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    
    ACI_TEST_RAISE( sLong < MTD_INTEGER_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    ACI_TEST_RAISE( sLong > MTD_INTEGER_MAXIMUM,
                    ERR_VALUE_OVERFLOW );
    
    *aInteger = (mtdIntegerType)sLong;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeBigint( mtdBigintType*     aBigint,
                      const acp_uint8_t* aString,
                      acp_uint32_t       aLength )
{
    acp_sint64_t    sLong;
    mtdNumericType* sNumeric;
    mtaTNumericType sTNumeric;
    acp_uint8_t           sNumericBuffer[MTD_NUMERIC_SIZE_MAXIMUM];

    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    sNumeric = (mtdNumericType*)sNumericBuffer;
    
    ACI_TEST( mtcMakeNumeric( sNumeric,
                              MTD_NUMERIC_MANTISSA_MAXIMUM,
                              aString,
                              aLength ) != ACI_SUCCESS );

    if( sNumeric->length > 1 )
    {
        ACI_TEST( mtaTNumeric3( &sTNumeric,
                                (mtaNumericType*)&sNumeric->signExponent,
                                sNumeric->length )
                  != ACI_SUCCESS );
        ACI_TEST( mtaSint64 ( &sLong,
                              &sTNumeric )
                  != ACI_SUCCESS );
    }
    else
    {
        sLong = 0;
    }

    ACI_TEST_RAISE( sLong < MTD_BIGINT_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    ACI_TEST_RAISE( sLong > MTD_BIGINT_MAXIMUM,
                    ERR_VALUE_OVERFLOW );
    
    *aBigint = (mtdBigintType)sLong;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

#if defined(ALTI_CFG_OS_WINDOWS)
// BUG-32537
double mtcPositiveHugeVal(void)
{
    union
    {
        acp_double_t mD;
        acp_uint64_t mU;
    } sHuge;

    sHuge.mU = ACP_UINT64_LITERAL(0x7ff0000000000000);
    return  sHuge.mD;
}

double mtcNegativeHugeVal(void)
{
    union
    {
        acp_double_t mD;
        acp_uint64_t mU;
    } sHuge;

    sHuge.mU = ACP_UINT64_LITERAL(0xFff0000000000000);
    return  sHuge.mD;
}

#endif

ACI_RC mtcMakeReal( mtdRealType      * aReal,
                    const acp_uint8_t* aString,
                    acp_uint32_t       aLength )
{
    acp_uint8_t *sEndPtr = NULL;
    acp_double_t sTemp;
    
    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    acpCStrToDouble ((const acp_char_t*)aString,
                     acpCStrLen((acp_char_t*)aString, aLength),
                     &sTemp,
                     (acp_char_t**)&sEndPtr);

    *aReal = sTemp;
    
    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( *aReal == 0 )
    {
        *aReal = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    
    ACI_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    
#if defined(ALTI_CFG_OS_WINDOWS)
    // BUG-32537
    ACI_TEST_RAISE( (*aReal == mtcPositiveHugeVal()) ||
                    (*aReal == mtcNegativeHugeVal()),
                    ERR_VALUE_OVERFLOW );
#else
    ACI_TEST_RAISE( (*aReal == +HUGE_VAL) || (*aReal == -HUGE_VAL),
                    ERR_VALUE_OVERFLOW );
#endif
    
    if( ( *aReal < MTD_REAL_MINIMUM ) &&
        ( *aReal > -MTD_REAL_MINIMUM ) &&
        ( *aReal != 0 ) )
    {
        *aReal = 0;
    } 
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeDouble( mtdDoubleType*     aDouble,
                      const acp_uint8_t* aString,
                      acp_uint32_t       aLength )
{
    acp_uint8_t *sEndPtr = NULL;

    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    acpCStrToDouble( (const char*)aString,
                     acpCStrLen((acp_char_t*)aString, aLength),
                     aDouble,
                     (char**)&sEndPtr);
    
    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( *aDouble == 0 )
    {
        *aDouble = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    
    ACI_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    
#if defined(ALTI_CFG_OS_WINDOWS)
    // BUG-32537
    ACI_TEST_RAISE( (*aDouble == mtcPositiveHugeVal()) ||
                    (*aDouble == mtcNegativeHugeVal()),
                    ERR_VALUE_OVERFLOW );
#else
    ACI_TEST_RAISE( (*aDouble == +HUGE_VAL) || (*aDouble == -HUGE_VAL),
                    ERR_VALUE_OVERFLOW );
#endif
    
    if( (*aDouble < MTD_DOUBLE_MINIMUM ) &&
        (*aDouble > -MTD_DOUBLE_MINIMUM ) &&
        ( *aDouble != 0 ) )
    {
        *aDouble = 0;
    }    
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_VALUE_OVERFLOW );
    aciSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW);
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeInterval( mtdIntervalType*   aInterval,
                        const acp_uint8_t* aString,
                        acp_uint32_t       aLength )
{
    acp_double_t  sDouble;
    acp_uint8_t  *sEndPtr = NULL;
    acp_double_t  sIntegerPart;
    acp_double_t  sFractionalPart;

    ACI_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    acpCStrToDouble((const acp_char_t*)aString,
                    acpCStrLen((acp_char_t*)aString, aLength),
                    &sDouble,
                    (acp_char_t**)&sEndPtr);
    
    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sDouble == 0 )
    {
        sDouble = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    
    ACI_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    
    sDouble *= 864e2;
    
    sFractionalPart        = modf( sDouble, &sIntegerPart );
    aInterval->second      = (acp_sint64_t)sIntegerPart;
    aInterval->microsecond = (acp_sint64_t)( sFractionalPart * 1E6 );
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}


ACI_RC mtcMakeBinary( mtdBinaryType    * aBinary,
                      const acp_uint8_t* aString,
                      acp_uint32_t       aLength )
{
    acp_uint32_t sByteLength = 0;
    
    ACI_TEST( mtcMakeByte2( aBinary->mValue,
                            aString,
                            aLength,
                            &sByteLength,
                            NULL)
              != ACI_SUCCESS );
    
    aBinary->mLength = sByteLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeByte( mtdByteType      * aByte,
                    const acp_uint8_t* aString,
                    acp_uint32_t       aLength )
{
    acp_uint32_t sByteLength = 0;
    
    ACI_TEST( mtcMakeByte2( aByte->value,
                            aString,
                            aLength,
                            &sByteLength,
                            NULL)
              != ACI_SUCCESS );

    aByte->length = sByteLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeByte2( acp_uint8_t*       aByteValue,
                     const acp_uint8_t* aString,
                     acp_uint32_t       aLength,
                     acp_uint32_t*      aByteLength,
                     acp_bool_t*        aOddSizeFlag)
{
    const acp_uint8_t* sToken;
    const acp_uint8_t* sTokenFence;
    acp_uint8_t*       sIterator;
    acp_uint8_t        sByte;
    acp_uint32_t       sByteLength = *aByteLength;

    const acp_uint8_t* sSrcText = aString;
    acp_uint32_t       sSrcLen = aLength;
    
    static const acp_uint8_t sHexByte[256] = {
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };
    
    sIterator = aByteValue;


    /* bug-31397: large sql_byte params make buffer overflow. */
    if ((aOddSizeFlag != NULL) && (*aOddSizeFlag == ACP_TRUE) &&
        (*aByteLength > 0) && (sSrcLen > 0))
    {
        /* get the last byte of the target buffer */
        sByte = *(sIterator - 1);
        ACI_TEST_RAISE( sHexByte[sSrcText[0]] > 15, ERR_INVALID_LITERAL );
        sByte |= sHexByte[sSrcText[0]];
        *(sIterator - 1) = sByte;

        sSrcText++;
        sSrcLen--;
        *aOddSizeFlag = ACP_FALSE;
    }

    for( sToken = sSrcText, sTokenFence = sToken + sSrcLen;
         sToken < sTokenFence;
         sIterator++, sToken += 2 )
    {
        ACI_TEST_RAISE( sHexByte[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sByte = sHexByte[sToken[0]] << 4;
        if( sToken + 1 < sTokenFence )
        {
            ACI_TEST_RAISE( sHexByte[sToken[1]] > 15,
                            ERR_INVALID_LITERAL );
            sByte |= sHexByte[sToken[1]];
        }
        /* bug-31397: large sql_byte params make buffer overflow.
         * If the size is odd, set aOddSizeFlag to 1 */
        else if ((sToken + 1 == sTokenFence) && (aOddSizeFlag != NULL))
        {
            *aOddSizeFlag = ACP_TRUE;
        }
        sByteLength++;
        *sIterator = sByte;
    }
    *aByteLength = sByteLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeNibble( mtdNibbleType*     aNibble,
                      const acp_uint8_t* aString,
                      acp_uint32_t       aLength )
{
    acp_uint32_t sNibbleLength = 0;
    
    ACI_TEST( mtcMakeNibble2( aNibble->value,
                              0,
                              aString,
                              aLength,
                              &sNibbleLength )
              != ACI_SUCCESS );

    aNibble->length = sNibbleLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeNibble2( acp_uint8_t*       aNibbleValue,
                       acp_uint8_t        aOffsetInByte,
                       const acp_uint8_t* aString,
                       acp_uint32_t       aLength,
                       acp_uint32_t*      aNibbleLength )
{
    const acp_uint8_t* sToken;
    const acp_uint8_t* sTokenFence;
    acp_uint8_t*       sIterator;
    acp_uint8_t        sNibble;
    acp_uint32_t       sNibbleLength = *aNibbleLength;
    
    static const acp_uint8_t sHexNibble[256] = {
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };
    
    sIterator = aNibbleValue;
    
    sToken = (const acp_uint8_t*)aString;
    sTokenFence = sToken + aLength;
        
    if( (aOffsetInByte == 1) && (sToken < sTokenFence) )
    {
        ACI_TEST_RAISE( sHexNibble[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sNibble = (*sIterator) | sHexNibble[sToken[0]];
        sNibbleLength++;
        ACI_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );

        *sIterator = sNibble;
        sIterator++;
        sToken += 1;
    }
    
    for( ;sToken < sTokenFence; sIterator++, sToken += 2 )
    {
        ACI_TEST_RAISE( sHexNibble[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sNibble = sHexNibble[sToken[0]] << 4;
        sNibbleLength++;
        ACI_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );

        if( sToken + 1 < sTokenFence )
        {
            ACI_TEST_RAISE( sHexNibble[sToken[1]] > 15, ERR_INVALID_LITERAL );
            sNibble |= sHexNibble[sToken[1]];
            sNibbleLength++;
            ACI_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                            ERR_INVALID_LITERAL );
        }

        *sIterator = sNibble;
    }

    *aNibbleLength = sNibbleLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeBit( mtdBitType*        aBit,
                   const acp_uint8_t* aString,
                   acp_uint32_t       aLength )
{
    acp_uint32_t sBitLength = 0;
    
    ACI_TEST( mtcMakeBit2( aBit->value,
                           0,
                           aString,
                           aLength,
                           &sBitLength )
              != ACI_SUCCESS );
    
    aBit->length = sBitLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtcMakeBit2( acp_uint8_t*       aBitValue,
                    acp_uint8_t        aOffsetInBit,
                    const acp_uint8_t* aString,
                    acp_uint32_t       aLength,
                    acp_uint32_t*      aBitLength )
{
    const acp_uint8_t* sToken;
    const acp_uint8_t* sTokenFence;
    acp_uint8_t*       sIterator;
    acp_uint8_t        sBit;
    acp_uint32_t       sSubIndex;
    acp_uint32_t       sBitLength = *aBitLength;
    acp_uint32_t       sTokenOffset = 0;
    
    static const acp_uint8_t sHexBit[256] = {
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        0,  1, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
    };
    
    sIterator = aBitValue;
    sToken = (const acp_uint8_t*)aString;
    sTokenFence = sToken + aLength;

    if( aOffsetInBit > 0 )
    {
        sBit = *sIterator;
        
        ACI_TEST_RAISE( sHexBit[sToken[0]] > 1, ERR_INVALID_LITERAL );
        
        sSubIndex    = aOffsetInBit;
        
        while( (sToken + sTokenOffset < sTokenFence) && (sSubIndex < 8) )
        {
            ACI_TEST_RAISE( sHexBit[sToken[sTokenOffset]] > 1, ERR_INVALID_LITERAL );
            sBit |= ( sHexBit[sToken[sTokenOffset]] << ( 7 - sSubIndex ) );
            sBitLength++;
            sTokenOffset++;
            sSubIndex++;
        }

        sToken += sTokenOffset;
        *sIterator = sBit;

        sIterator++;
    }
    
    for( ; sToken < sTokenFence; sIterator++, sToken += 8 )
    {
        ACI_TEST_RAISE( sHexBit[sToken[0]] > 1, ERR_INVALID_LITERAL );

        // �� �ֱ� ���� 0���� �ʱ�ȭ
        acpMemSet( sIterator,
                   0x00,
                   1 );
        
        sBit = sHexBit[sToken[0]] << 7;
        sBitLength++;

        ACI_TEST_RAISE( sBitLength == 0, ERR_INVALID_LITERAL );

        sSubIndex = 1;
        while( sToken + sSubIndex < sTokenFence && sSubIndex < 8 )
        {
            ACI_TEST_RAISE( sHexBit[sToken[sSubIndex]] > 1, ERR_INVALID_LITERAL );
            sBit |= ( sHexBit[sToken[sSubIndex]] << ( 7 - sSubIndex ) );
            sBitLength++;
            sSubIndex++;
        }

        *sIterator = sBit;
    }

    *aBitLength = sBitLength;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

acp_bool_t mtcCompareOneChar( acp_uint8_t* aValue1,
                              acp_uint8_t  aValue1Size,
                              acp_uint8_t* aValue2,
                              acp_uint8_t  aValue2Size )
{
    acp_bool_t sIsSame;

    if( aValue1Size != aValue2Size )
    {
        sIsSame = ACP_FALSE;
    }
    else
    {
        sIsSame = ( acpMemCmp( aValue1, aValue2,
                               aValue1Size ) == 0
                    ? ACP_TRUE : ACP_FALSE );
    }

    return sIsSame;
}

acp_sint32_t mtcStrCaselessMatch(const void *aValue1,
                                 acp_size_t  aLength1,
                                 const void *aValue2,
                                 acp_size_t  aLength2)
{
    acp_char_t *sValue1;
    acp_char_t *sValue2;
    acp_size_t  sCount;

    if( aLength1 != aLength2 )
    {
        return -1;
    }

    sValue1 = (acp_char_t *)aValue1;
    sValue2 = (acp_char_t *)aValue2;

    for(sCount = 0; sCount < aLength1; sCount++)
    {
        if(acpCharToUpper(sValue1[sCount]) != acpCharToUpper(sValue2[sCount]))
        {
            return -1;
        }
    }

    return 0;
}

acp_sint32_t mtcStrMatch(const void *aValue1,
                         acp_size_t  aLength1,
                         const void *aValue2,
                         acp_size_t  aLength2)
{
    if( aLength1 < aLength2 )
    {
        return ( acpMemCmp( aValue1, aValue2, aLength1 ) >  0 ) ? 1 : -1 ;
    }
    if( aLength1 > aLength2 )
    {
        return ( acpMemCmp( aValue1, aValue2, aLength2 ) >= 0 ) ? 1 : -1 ;
    }
    return acpMemCmp( aValue1, aValue2, aLength1 );
}

/* PROJ-2209 DBTIMEZONE */
acp_sint64_t getSystemTimezoneSecond( void )
{
    acp_sint64_t   sLocalSecond;
    acp_sint64_t   sGlobalSecond;
    acp_sint64_t   sHour;
    acp_sint64_t   sMin;
    acp_sint32_t   sDayOffset;
    acp_time_exp_t sLocaltime;
    acp_time_exp_t sGlobaltime;

    acp_time_t sTime = acpTimeNow();
    acpTimeGetLocalTime( sTime, &sLocaltime );
    acpTimeGetGmTime( sTime, &sGlobaltime );

    sHour = (acp_sint64_t)sLocaltime.mHour;
    sMin  = (acp_sint64_t)sLocaltime.mMin;
    sLocalSecond = ( sHour * 60 * 60 ) + ( sMin * 60 );

    sHour = (acp_sint64_t)sGlobaltime.mHour;
    sMin  = (acp_sint64_t)sGlobaltime.mMin;
    sGlobalSecond = ( sHour * 60 * 60 ) + ( sMin * 60 );

    if ( sLocaltime.mDay != sGlobaltime.mDay )
    {
        sDayOffset = sLocaltime.mDay - sGlobaltime.mDay;
        if ( ( sDayOffset == -1 ) || ( sDayOffset > 2 ) )
        {
            /* -1 day */
            sLocalSecond -= ( 24 * 60 * 60 );
        }
        else
        {
            /* +1 day */
            sLocalSecond += ( 24 * 60 * 60 );
        }
    }
    else
    {
        /* nothing to do. */
    }

    return ( sLocalSecond - sGlobalSecond );
}

acp_char_t *getSystemTimezoneString( acp_char_t * aTimezoneString )
{
    acp_sint64_t sTimezoneSecond;
    acp_sint64_t sAbsTimezoneSecond;

    sTimezoneSecond = getSystemTimezoneSecond();

    if ( sTimezoneSecond < 0 )
    {
        sAbsTimezoneSecond = ( sTimezoneSecond * -1 );
        (void)acpSnprintf( aTimezoneString,
                           MTC_TIMEZONE_VALUE_LEN + 1,
                           "-%02"ACI_INT32_FMT":%02"ACI_INT32_FMT,
                           sAbsTimezoneSecond / 3600, ( sAbsTimezoneSecond % 3600 ) / 60 );
    }
    else
    {
        (void)acpSnprintf( aTimezoneString,
                           MTC_TIMEZONE_VALUE_LEN + 1,
                           "+%02"ACI_INT32_FMT":%02"ACI_INT32_FMT,
                           sTimezoneSecond / 3600, ( sTimezoneSecond % 3600 ) / 60 );
    }

    return aTimezoneString;
}

/*
 * PROJ-1517
 * rounds up aArgument1 and store it on aValue
 */
ACI_RC mtcRoundFloat( mtdNumericType *aValue,
                      mtdNumericType *aArgument1,
                      mtdNumericType *aArgument2 )
{
    acp_sint16_t sExponent;
    acp_sint64_t  sRound;
    acp_uint16_t sCarry;
    acp_sint16_t sArgExponent1;
    acp_uint8_t  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    acp_sint32_t   i;

    /* null �̸� null */
    if ( aArgument1->length == 0 )
    {
        aValue->length = 0;
    }
    else if ( aArgument1->length == 1 )
    {
        aValue->length = 1;
        aValue->mantissa[0]= 0;
        aValue->signExponent = 0x80;
    }
    else
    {
        ACI_TEST( mtcNumeric2Slong( &sRound, aArgument2 ) != ACI_SUCCESS );

        sRound = -sRound;

        /* exponent ���ϱ� */
        sArgExponent1 = aArgument1->signExponent & 0x80;
        if ( sArgExponent1 == 0x80 )
        {
            sExponent = (aArgument1->signExponent & 0x7F) - 64;
        }
        else
        {
            sExponent = 64 - (aArgument1->signExponent & 0x7F);
        }

        /* aArgument1 ���� */
        aValue->signExponent = aArgument1->signExponent;
        aValue->length = aArgument1->length;

        if ( sArgExponent1 == 0x80 )
        {
            for ( i = 0; i < aArgument1->length - 1; i++ )
            {
                sMantissa[i] = aArgument1->mantissa[i];
                aValue->mantissa[i] = aArgument1->mantissa[i];
            }
        }
        else
        {
            for ( i = 0; i < aArgument1->length - 1; i++ )
            {
                sMantissa[i] = 99 - aArgument1->mantissa[i];
                aValue->mantissa[i] = aArgument1->mantissa[i];
            }
        }

        if ( sExponent * 2 <= sRound )
        {
            if ( ( sExponent * 2 == sRound ) && ( sMantissa[0] >= 50 ) )
            {
                sExponent++;
                ACI_TEST_RAISE( sExponent > 63, ERR_OVERFLOW );
                aValue->length = 2;
                /* exponent ���� */
                if ( sArgExponent1 == 0x80 )
                {
                    aValue->signExponent = sExponent + 192;
                    aValue->mantissa[0] = 1;
                }
                else
                {
                    aValue->signExponent = 64 - sExponent;
                    aValue->mantissa[0] = 98;
                }
            }
            else
            {
                //zero
                aValue->length = 1;
                aValue->mantissa[0]= 0;
                aValue->signExponent = 0x80;
            }
        }
        else
        {
#if defined(HP_HPUX) || defined(IA64_HP_HPUX)
            switch ( (acp_sint32_t)( sRound % 2 ) )
#else
            switch ( sRound % 2 )
#endif
            {
                case 1:
                    if ( ( sExponent - ( sRound / 2 ) - 1 )
                        < (acp_sint16_t)sizeof(sMantissa) )
                    {
                        if ( sMantissa[sExponent - ( sRound / 2 ) - 1] % 10 >= 5 )
                        {
                            sMantissa[sExponent - ( sRound / 2 ) - 1]
                                     -= sMantissa[sExponent - ( sRound / 2 ) - 1] % 10;
                            sMantissa[sExponent - ( sRound / 2 ) - 1] += 10;
                        }
                        else
                        {
                            sMantissa[sExponent - ( sRound / 2 ) - 1]
                                     -= sMantissa[sExponent - ( sRound / 2 ) - 1] % 10;
                        }

                        for ( i = sExponent - ( sRound / 2 );
                              i < (acp_sint32_t)sizeof(sMantissa); i++ )
                        {
                            sMantissa[i] = 0;
                        }
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    break;

                case -1:
                    if ( ( sExponent - ( sRound / 2 ) ) < (acp_sint16_t)sizeof(sMantissa) )
                    {
                        if ( sMantissa[sExponent - ( sRound / 2 )] % 10 >= 5 )
                        {
                            sMantissa[sExponent - ( sRound / 2 )]
                                     -= sMantissa[sExponent - ( sRound / 2 ) ] % 10;
                            sMantissa[sExponent - ( sRound / 2 )] += 10;
                        }
                        else
                        {
                            sMantissa[sExponent - ( sRound / 2 )]
                                     -= sMantissa[sExponent - ( sRound / 2 )] % 10;
                        }

                        for ( i = sExponent - ( sRound / 2 ) + 1;
                              i < (acp_sint32_t)sizeof(sMantissa); i++ )
                        {
                            sMantissa[i] = 0;
                        }
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    break;

                default:
                    if ( ( sExponent - ( sRound / 2 ) ) < (acp_sint16_t)sizeof(sMantissa) )
                    {
                        if ( sMantissa[sExponent - ( sRound / 2 )] >= 50 )
                        {
                            sMantissa[sExponent - ( sRound / 2 ) - 1]++;
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        for ( i = sExponent - ( sRound / 2 ); i < (acp_sint32_t)sizeof(sMantissa); i++ )
                        {
                            sMantissa[i] = 0;
                        }
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    break;
            }/* switch */

            sCarry = 0;
            for ( i = sizeof(sMantissa) - 1; i >= 0; i-- )
            {
                sMantissa[i] += sCarry;
                sCarry = 0;
                if ( sMantissa[i] >= 100 )
                {
                    sMantissa[i] -= 100;
                    sCarry = 1;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            if ( sCarry != 0 )
            {
                for ( i = sizeof(sMantissa) - 1; i > 0; i-- )
                {
                    sMantissa[i] = sMantissa[i - 1];
                }
                sMantissa[0] = 1;
                sExponent++;
                ACI_TEST_RAISE( sExponent > 63, ERR_OVERFLOW );
                /* exponent ���� */
                if ( sArgExponent1 == 0x80 )
                {
                    aValue->signExponent = sExponent + 192;
                }
                else
                {
                    aValue->signExponent = 64 - sExponent;
                }
            }
            else
            {
                /* Nothing to do */
            }

            /* length ���� */
            for ( i = 0, aValue->length = 1; i < (acp_sint32_t)sizeof(sMantissa) - 1; i++ )
            {
                if ( sMantissa[i] != 0 )
                {
                    aValue->length = i + 2;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            if ( sArgExponent1 == 0x80 )
            {
                for ( i = 0; i < aValue->length - 1; i++ )
                {
                    aValue->mantissa[i] = sMantissa[i];
                }
            }
            else
            {
                for ( i = 0; i < aValue->length - 1; i++ )
                {
                    aValue->mantissa[i] = 99 - sMantissa[i];
                }
            }

            if ( sMantissa[0] == 0 )
            {
                //zero
                aValue->length = 1;
                aValue->mantissa[0]= 0;
                aValue->signExponent = 0x80;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }/* else */

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_OVERFLOW );
    {
        aciSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * PROJ-1517
 * converts mtdNumericType to SLong
 */
ACI_RC mtcNumeric2Slong( acp_sint64_t          *aValue,
                         mtdNumericType *aArgument1 )
{
    acp_uint64_t  sULongValue = 0;
    acp_sint16_t sArgExponent1;
    acp_sint16_t sExponent;
    acp_uint8_t  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    acp_sint32_t   i;

    static const acp_uint64_t sPower[] = { (acp_uint64_t)(1),
                                    (acp_uint64_t)(100),
                                    (acp_uint64_t)(10000),
                                    (acp_uint64_t)(1000000),
                                    (acp_uint64_t)(100000000),
                                    (acp_uint64_t)(10000000000),
                                    (acp_uint64_t)(1000000000000),
                                    (acp_uint64_t)(100000000000000),
                                    (acp_uint64_t)(10000000000000000),
                                    (acp_uint64_t)(1000000000000000000) };
    /* null �̸� ���� */
    ACI_TEST_RAISE( aArgument1->length == 0, ERR_NULL_VALUE );

    if ( aArgument1->length == 1 )
    {
        *aValue = 0;
        ACI_RAISE( NORMAL_EXIT );
    }
    else
    {
        /* nothing to do. */
    }

    sArgExponent1 = aArgument1->signExponent & 0x80;

    /* ����̸� �״�� �����̸� ����� ��ȯ */
    if ( sArgExponent1 == 0x80 )
    {
        sExponent = (aArgument1->signExponent & 0x7F) - 64;

        for ( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa[i] = aArgument1->mantissa[i];
        }
    }
    else
    {
        sExponent = 64 - (aArgument1->signExponent & 0x7F);

        for ( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa[i] = 99 - aArgument1->mantissa[i];
        }
    }

    if ( sExponent > 0 )
    {
        ACI_TEST_RAISE( (acp_uint16_t)sExponent
                        > ( sizeof(sPower) / sizeof(sPower[0]) ),
                        ERR_OVERFLOW );
        for ( i = 0; i < sExponent; i++ )
        {
            sULongValue += sMantissa[i] * sPower[sExponent - i - 1];
        }
    }
    else
    {
        /* nothing to do. */
    }

    //���
    if ( sArgExponent1 == 0x80 )
    {
        ACI_TEST_RAISE( sULongValue > ACP_SLONG_MAX,
                        ERR_OVERFLOW );
        *aValue = sULongValue;
    }
    //����
    else
    {
        ACI_TEST_RAISE( sULongValue > 0x8000000000000000,//(acp_uint64_t)(9223372036854775808),
                        ERR_OVERFLOW );

        if ( sULongValue == (acp_uint64_t)(0x8000000000000000) )//9223372036854775808) )
        {
            *aValue = (acp_sint64_t)ACP_SINT64_MIN;//(-9223372036854775807) - (acp_sint64_t)(1);
        }
        else
        {
            *aValue = -(acp_sint64_t)sULongValue;
        }
    }

    ACI_EXCEPTION_CONT( NORMAL_EXIT );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_OVERFLOW );
    {
        aciSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW );
    }

    ACI_EXCEPTION( ERR_NULL_VALUE );
    {
        aciSetErrorCode( mtERR_ABORT_NULL_VALUE );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    ISO 8601 ǥ�� ������ ���մϴ�.
 *      ������ �����Ϻ��� �����մϴ�.
 *      1�� 1���� ����� �����̸�, ù �ֿ� ���⵵�� ������ �ִ� ��⵵�� 1�����Դϴ�.
 *      1�� 1���� �ݿ��� �����̸�, ù �ִ� ���⵵�� ������ �����Դϴ�.
 *
 * Implementation :
 *    1. ������ ���մϴ�.
 *      1�� 1�� : 1�� 1���� �������� ���, ���⵵ 12�� 31���� ������ ���մϴ�. (Step 2)
 *      1�� 2�� : 1�� 1���� ������ ���, ���⵵ 12�� 31���� ������ ���մϴ�. (Step 2)
 *      1�� 3�� : 1�� 1���� ���� ���, ���⵵ 12�� 31���� ������ ���մϴ�. (Step 2)
 *      12�� 29�� : 12�� 31���� ���� ���, ���⵵ 1�����Դϴ�.
 *      12�� 30�� : 12�� 31���� ȭ���� ���, ���⵵ 1�����Դϴ�.
 *      12�� 31�� : 12�� 31���� ��ȭ���� ���, ���⵵ 1�����Դϴ�.
 *      ������ : ��⵵ ������ ���մϴ�. (Step 2)
 *
 *    2. ������ ���մϴ�.
 *      (1) ù �ְ� ������ ���ԵǴ��� Ȯ���մϴ�.
 *          1�� 1���� ��ȭ������ ���, ù �ְ� ������ ���Ե˴ϴ�.
 *          ù ���� ����������(�Ͽ���)�� ���մϴ�. (firstSunday)
 *
 *      (2) ������ ���� ������ ���մϴ�.
 *          ( dayOfYear - firstSunday + 6 ) / 7
 *
 *      (3) 1�� 2�� ���ؼ� ������ ���մϴ�.
 *
 ***********************************************************************/
acp_sint32_t mtcWeekOfYearForStandard( acp_sint32_t aYear,
                                       acp_sint32_t aMonth,
                                       acp_sint32_t aDay )
{
    acp_sint32_t    sWeekOfYear  = 0;
    acp_sint32_t    sDayOfWeek   = 0;
    acp_sint32_t    sDayOfYear   = 0;
    acp_sint32_t    sFirstSunday = 0;
    acp_bool_t      sIsPrevYear  = ACP_FALSE;

    /* Step 1. ������ ���մϴ�. */
    if ( aMonth == 1 )
    {
        switch ( aDay )
        {
            case 1 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( ( sDayOfWeek >= 5 ) || ( sDayOfWeek == 0 ) )   // ������
                {
                    sIsPrevYear = ACP_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 2 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek >= 5 )                              // ����
                {
                    sIsPrevYear = ACP_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 3 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek == 5 )                              // ��
                {
                    sIsPrevYear = ACP_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            default :
                break;
        }
    }
    else if ( aMonth == 12 )
    {
        switch ( aDay )
        {
            case 29 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( sDayOfWeek == 3 )                                                      // ��
                {
                    sWeekOfYear = 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 30 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( ( sDayOfWeek == 2 ) || ( sDayOfWeek == 3 ) )                           // ȭ��
                {
                    sWeekOfYear = 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 31 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( ( sDayOfWeek == 1 ) || ( sDayOfWeek == 2 ) || ( sDayOfWeek == 3 ) )    // ��ȭ��
                {
                    sWeekOfYear = 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            default :
                break;
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* Step 2. ������ ���մϴ�. */
    if ( sWeekOfYear == 0 )
    {
        if ( sIsPrevYear == ACP_TRUE )
        {
            sDayOfWeek = mtcDayOfWeek( aYear - 1, 1, 1 );
            sDayOfYear = mtcDayOfYear( aYear - 1, 12, 31 );
        }
        else
        {
            sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
            sDayOfYear = mtcDayOfYear( aYear, aMonth, aDay );
        }

        sFirstSunday = 8 - sDayOfWeek;
        if ( sFirstSunday == 8 )    // 1�� 1���� �Ͽ���
        {
            sFirstSunday = 1;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sDayOfWeek >= 1 ) && ( sDayOfWeek <= 4 ) )   // ù �ְ� ��ȭ����
        {
            sWeekOfYear = ( sDayOfYear - sFirstSunday + 6 ) / 7 + 1;
        }
        else
        {
            sWeekOfYear = ( sDayOfYear - sFirstSunday + 6 ) / 7;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return sWeekOfYear;
}

/***********************************************************************
 *
 * Description :
 *    ISO 8601 ǥ�� ������ ���մϴ�.
 *      ������ �����Ϻ��� �����մϴ�.
 *      1�� 1���� ����� �����̸�, ù �ֿ� ���⵵�� ������ �ִ� ��⵵�� 1�����Դϴ�.
 *      1�� 1���� �ݿ��� �����̸�, ù �ִ� ���⵵�� ������ �����Դϴ�.
 *
 * Implementation :
 *    1. ������ ���մϴ�.
 *      1�� 1��   : 1�� 1���� �������� ���, ���⵵�Դϴ�.
 *      1�� 2��   : 1�� 1���� ������ ���, ���⵵�Դϴ�.
 *      1�� 3��   : 1�� 1���� ���� ���, ���⵵�Դϴ�.
 *      12�� 29�� : 12�� 31���� ���� ���, ���⵵�Դϴ�.
 *      12�� 30�� : 12�� 31���� ȭ���� ���, ���⵵�Դϴ�.
 *      12�� 31�� : 12�� 31���� ��ȭ���� ���, ���⵵�Դϴ�.
 *      ������    : ��⵵�Դϴ�.
 *
 ***********************************************************************/
acp_sint32_t mtcYearForStandard( acp_sint32_t aYear,
                                 acp_sint32_t aMonth,
                                 acp_sint32_t aDay )
{
    acp_sint32_t    sYear  = 0;
    acp_sint32_t    sDayOfWeek   = 0;

    /* Step 1. ������ ���մϴ�. */
    sYear = aYear;

    if ( aMonth == 1 )
    {
        switch ( aDay )
        {
            case 1 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( ( sDayOfWeek >= 5 ) || ( sDayOfWeek == 0 ) )   // ������
                {
                    sYear = aYear - 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 2 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek >= 5 )                              // ����
                {
                    sYear = aYear - 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 3 :
                sDayOfWeek = mtcDayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek == 5 )                              // ��
                {
                    sYear = aYear - 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            default :
                break;
        }
    }
    else if ( aMonth == 12 )
    {
        switch ( aDay )
        {
            case 29 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( sDayOfWeek == 3 )                                                      // ��
                {
                    sYear = aYear + 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 30 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( ( sDayOfWeek == 2 ) || ( sDayOfWeek == 3 ) )                           // ȭ��
                {
                    sYear = aYear + 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 31 :
                sDayOfWeek = mtcDayOfWeek( aYear, 12, 31 );
                if ( ( sDayOfWeek == 1 ) || ( sDayOfWeek == 2 ) || ( sDayOfWeek == 3 ) )    // ��ȭ��
                {
                    sYear = aYear + 1;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            default :
                break;
        }
    }
    else
    {
        /* Nothing to do */
    }

    return sYear;
}
