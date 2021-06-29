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
 * $Id: mtc.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtf.h>
#include <mtl.h>
#include <mtv.h>
#include <mtk.h>
#include <mtz.h>
#if defined (CYGWIN32)
# include <pthread.h>
#endif

#include <idErrorCode.h>
#include <mtuProperty.h>
#include <iduCheckLicense.h>

extern mtdModule mtdDouble;
extern mtdModule mtdNumeric;
extern mtdModule mtdFloat;
extern mtlModule mtlAscii;
extern mtlModule mtlUTF16;

/* PROJ-1517 */
SDouble gDoublePow[64];

/********************************************************
 * BUG-41194 double to numeric ��ȯ ���ɰ���
 * 2������ 10������ ����� ����� ��ȯ���̺��� �����Ѵ�.
 *
 * 2^-1075 = 0.247033 E-323
 * 2^-1074 = 0.494066 E-323
 * ...
 * 2^-1    = 0.500000 E0
 * 2^0     = 0.100000 E1
 * 2^1     = 0.200000 E1
 * 2^2     = 0.400000 E1
 * ...
 * 2^1074  = 0.202402 E324
 * 2^1075  = 0.404805 E324
 *           ~~~~~~~~  ~~~
 *           �Ǽ���   ������
 *
 ********************************************************/
SDouble   gConvMantissaBuf[1075+1+1075];  // �Ǽ���
SInt      gConvExponentBuf[1075+1+1075];  // ������
SDouble * gConvMantissa;
SInt    * gConvExponent;
idBool    gIEEE754Double;

const UChar mtc::hashPermut[256] = {
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

const UInt mtc::hashInitialValue = 0x01020304;

mtcGetColumnFunc            mtc::getColumn;
mtcOpenLobCursorWithRow     mtc::openLobCursorWithRow;
mtcOpenLobCursorWithGRID    mtc::openLobCursorWithGRID;
mtcReadLob                  mtc::readLob;
mtcGetLobLengthWithLocator  mtc::getLobLengthWithLocator;
mtcIsNullLobColumnWithRow   mtc::isNullLobColumnWithRow;
mtcGetCompressionColumnFunc mtc::getCompressionColumn;
/* PROJ-2446 ONE SOURCE */
mtcGetStatistics            mtc::getStatistics;

// PROJ-1579 NCHAR
mtcGetDBCharSet            mtc::getDBCharSet;
mtcGetNationalCharSet      mtc::getNationalCharSet;

static IDE_RC             gMtcResult      = IDE_SUCCESS;
static PDL_thread_mutex_t gMtcMutex;

UInt mtc::mExtTypeModuleGroupCnt = 0;
UInt mtc::mExtCvtModuleGroupCnt = 0;
UInt mtc::mExtFuncModuleGroupCnt = 0;
UInt mtc::mExtRangeFuncGroupCnt = 0;
UInt mtc::mExtCompareFuncCnt = 0;

mtdModule ** mtc::mExtTypeModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];
mtvModule ** mtc::mExtCvtModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];
mtfModule ** mtc::mExtFuncModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];
smiCallBackFunc * mtc::mExtRangeFuncGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];
mtdCompareFunc  * mtc::mExtCompareFuncGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

/* BUG-39237 add sys_guid() function */
ULong     gInitialValue;
SChar     gHostID[MTF_SYS_HOSTID_LENGTH + 1];
UInt      gTimeSec;

extern "C" void mtcInitializeMutex( void )
{
    if( idlOS::thread_mutex_init( &gMtcMutex ) != 0 )
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_ThrMutexInit));
        gMtcResult = IDE_FAILURE;
    }
}

const void* mtc::getColumnDefault( const void*,
                                   const smiColumn*,
                                   UInt* )
{
    return NULL;
}

IDE_RC
mtc::addExtTypeModule( mtdModule **  aExtTypeModules )
{
    IDE_ASSERT( aExtTypeModules != NULL );
    IDE_ASSERT( mExtTypeModuleGroupCnt < MTC_MAXIMUM_EXTERNAL_GROUP_CNT );

    mExtTypeModuleGroup[mExtTypeModuleGroupCnt] = aExtTypeModules;
    mExtTypeModuleGroupCnt++;

    return IDE_SUCCESS;
}

IDE_RC
mtc::addExtCvtModule( mtvModule **  aExtCvtModules )
{
    IDE_ASSERT( aExtCvtModules != NULL );
    IDE_ASSERT( mExtCvtModuleGroupCnt < MTC_MAXIMUM_EXTERNAL_GROUP_CNT );

    mExtCvtModuleGroup[mExtCvtModuleGroupCnt] = aExtCvtModules;
    mExtCvtModuleGroupCnt++;

    return IDE_SUCCESS;
}

IDE_RC
mtc::addExtFuncModule( mtfModule **  aExtFuncModules )
{
    IDE_ASSERT( aExtFuncModules != NULL );
    IDE_ASSERT( mExtFuncModuleGroupCnt < MTC_MAXIMUM_EXTERNAL_GROUP_CNT );

    mExtFuncModuleGroup[mExtFuncModuleGroupCnt] = aExtFuncModules;
    mExtFuncModuleGroupCnt++;

    return IDE_SUCCESS;
}

IDE_RC
mtc::addExtRangeFunc( smiCallBackFunc  * aExtRangeFuncs )
{
    IDE_ASSERT( aExtRangeFuncs != NULL );
    IDE_ASSERT( mExtRangeFuncGroupCnt < MTC_MAXIMUM_EXTERNAL_GROUP_CNT );

    mExtRangeFuncGroup[mExtRangeFuncGroupCnt] = aExtRangeFuncs;
    mExtRangeFuncGroupCnt++;

    return IDE_SUCCESS;
}

IDE_RC
mtc::addExtCompareFunc( mtdCompareFunc * aExtCompareFuncs )
{
    IDE_ASSERT( aExtCompareFuncs != NULL );
    IDE_ASSERT( mExtCompareFuncCnt < MTC_MAXIMUM_EXTERNAL_GROUP_CNT );

    mExtCompareFuncGroup[mExtCompareFuncCnt] = aExtCompareFuncs;
    mExtCompareFuncCnt++;

    return IDE_SUCCESS;
}

IDE_RC mtc::initialize( mtcExtCallback  * aCallBacks )
{
    SDouble   sDoubleValue = 1.0;
    SDouble   sDoubleProbe = -100.0;
    UChar   * sDoubleProbes = (UChar*) &sDoubleProbe;
    SChar     sHostString[IDU_SYSTEM_INFO_LENGTH];
    UInt      i;
    SInt      j;

    /* BUG-40387 trace log�� ���� ���̱� ����
     * mt�ʱ�ȭ�� �߻��ϴ� ������ ������� �ʴ´�. */
    
    if( aCallBacks == NULL )
    {
        getColumn               = getColumnDefault;
        openLobCursorWithRow    = NULL;
        openLobCursorWithGRID   = NULL;
        readLob                 = NULL;
        getLobLengthWithLocator = NULL;
        isNullLobColumnWithRow  = NULL;

        getDBCharSet            = NULL;
        getNationalCharSet      = NULL;

        getCompressionColumn    = NULL;
        /* PROJ-2336 ONE SOURCE */
        getStatistics           = NULL;
    }
    else
    {
        getColumn               = aCallBacks->getColumn;
        openLobCursorWithRow    = aCallBacks->openLobCursorWithRow;
        openLobCursorWithGRID   = aCallBacks->openLobCursorWithGRID;
        readLob                 = aCallBacks->readLob;
        getLobLengthWithLocator = aCallBacks->getLobLengthWithLocator;
        isNullLobColumnWithRow  = aCallBacks->isNullLobColumnWithRow;

        getDBCharSet            = aCallBacks->getDBCharSet;
        getNationalCharSet      = aCallBacks->getNationalCharSet;

        getCompressionColumn    = aCallBacks->getCompressionColumn;
        /* PROJ-2336 ONE SOURCE */
        getStatistics           = aCallBacks->getStatistics;
    }

    IDE_TEST( mtuProperty::initProperty( NULL ) != IDE_SUCCESS );

    /* PROJ-1517 */
    for ( i = 0; i < ( ID_SIZEOF(gDoublePow) / ID_SIZEOF(gDoublePow[0]) ); i++)
    {
        gDoublePow[i] = sDoubleValue;
        sDoubleValue  = sDoubleValue * 100.0;
    }

    // BUG-41194
    // IEEE754���� 64bit double�� -100�� ������ ���� ǥ���Ѵ�.
    // 11000000 01011001 00000000 00000000 00000000 00000000 00000000 00000000
    // C0       59       00       00       00       00       00       00
#if defined(ENDIAN_IS_BIG_ENDIAN)
    if ( ( sDoubleProbes[0] == 0xC0 ) && ( sDoubleProbes[1] == 0x59 ) &&
         ( sDoubleProbes[2] == 0x00 ) && ( sDoubleProbes[3] == 0x00 ) &&
         ( sDoubleProbes[4] == 0x00 ) && ( sDoubleProbes[5] == 0x00 ) &&
         ( sDoubleProbes[6] == 0x00 ) && ( sDoubleProbes[7] == 0x00 ) )
#else
    if ( ( sDoubleProbes[7] == 0xC0 ) && ( sDoubleProbes[6] == 0x59 ) &&
         ( sDoubleProbes[5] == 0x00 ) && ( sDoubleProbes[4] == 0x00 ) &&
         ( sDoubleProbes[3] == 0x00 ) && ( sDoubleProbes[2] == 0x00 ) &&
         ( sDoubleProbes[1] == 0x00 ) && ( sDoubleProbes[0] == 0x00 ) )
#endif
    {
        // IEEE754������ double�̴�.
        gIEEE754Double = ID_TRUE;
        
        // 2���� 10���� ��ȯ���̺��� �����Ѵ�.
        gConvMantissa = gConvMantissaBuf + 1075;
        gConvExponent = gConvExponentBuf + 1075;
    
        gConvMantissa[0] = 0.1;
        gConvExponent[0] = 1;
        for ( j = 1; j <= 1075; j++ )
        {
            gConvMantissa[j] = gConvMantissa[j - 1] * 2.0;
            gConvExponent[j] = gConvExponent[j - 1];
            if ( gConvMantissa[j] > 1 )
            {
                gConvMantissa[j] /= 10.0;
                gConvExponent[j]++;
            }
            else
            {
                // Nothing to do.
            }

            gConvMantissa[-j] = gConvMantissa[-j + 1] / 2.0;
            gConvExponent[-j] = gConvExponent[-j + 1];
            if ( gConvMantissa[-j] < 0.1 )
            {
                gConvMantissa[-j] *= 10.0;
                gConvExponent[-j]--;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // IEEE754������ double�� �ƴϴ�.
        gIEEE754Double = ID_FALSE;
    }
  
    /* BUG-39237 adding sys_guid() function that returning byte(16) type. */ 
    idlOS::srand(idlOS::time());

    gInitialValue = ( (ULong) idlOS::rand() << 32 ) + (ULong) idlOS::rand() ;
    gTimeSec      = (UInt) idlOS::gettimeofday().sec();

    // MT ��� �ʱ�ȭ �ÿ��� ������Ƽ�� �ʱ�ȭ ������
    // ���� Ŭ���̾�Ʈ ���� �ÿ��� �ش� Ŭ���̾�Ʈ�� ALTIBASE_NLS_USE��
    // �ٽ� defModule�� �����ϰ� �ȴ�.
    IDE_TEST( mtl::initialize( (SChar*)"US7ASCII", ID_FALSE )
              != IDE_SUCCESS );
    IDE_TEST( mtd::initialize( mExtTypeModuleGroup, mExtTypeModuleGroupCnt )
              != IDE_SUCCESS );
    IDE_TEST( mtv::initialize( mExtCvtModuleGroup, mExtCvtModuleGroupCnt )
              != IDE_SUCCESS );
    IDE_TEST( mtf::initialize( mExtFuncModuleGroup, mExtFuncModuleGroupCnt )
              != IDE_SUCCESS );
    IDE_TEST( mtk::initialize( mExtRangeFuncGroup, mExtRangeFuncGroupCnt,
                               mExtCompareFuncGroup, mExtCompareFuncCnt )
              != IDE_SUCCESS );
    IDE_TEST( mtz::initialize() != IDE_SUCCESS );

    IDE_TEST( iduCheckLicense::getHostUniqueString( sHostString, ID_SIZEOF(sHostString))
              != IDE_SUCCESS );

    if ( idlOS::strlen( sHostString ) < MTF_SYS_HOSTID_LENGTH )
    {
        idlOS::snprintf( gHostID, MTF_SYS_HOSTID_LENGTH + 1,
                         MTF_SYS_HOSTID_FORMAT""ID_XINT32_FMT,
                         idlOS::rand() );
    }
    else
    {
        idlOS::strncpy( gHostID, sHostString, MTF_SYS_HOSTID_LENGTH);
        idlOS::strUpper( gHostID, MTF_SYS_HOSTID_LENGTH );
        gHostID[MTF_SYS_HOSTID_LENGTH] = '\0';
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC mtc::finalize( idvSQL * aStatistics )
{
    IDE_TEST( mtf::finalize() != IDE_SUCCESS );
    IDE_TEST( mtv::finalize() != IDE_SUCCESS );
    IDE_TEST( mtd::finalize() != IDE_SUCCESS );
    IDE_TEST( mtl::finalize() != IDE_SUCCESS );
    IDE_TEST( mtz::finalize() != IDE_SUCCESS );
    IDE_TEST( mtuProperty::finalProperty( aStatistics )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt mtc::isSameType( const mtcColumn* aColumn1,
                      const mtcColumn* aColumn2 )
{
    UInt sRes;
    if( aColumn1->type.dataTypeId == aColumn2->type.dataTypeId  &&
        aColumn1->type.languageId == aColumn2->type.languageId  &&
        ( aColumn1->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK ) ==
        ( aColumn2->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK ) &&
        aColumn1->precision     == aColumn2->precision      &&
        aColumn1->scale         == aColumn2->scale           )
    {
        sRes = 1;
    }
    else
    {
        sRes = 0;
    }

    return sRes;
}

void mtc::copyColumn( mtcColumn*       aDestination,
                      const mtcColumn* aSource )
{
    *aDestination = *aSource;

    aDestination->column.flag &= ~SMI_COLUMN_TYPE_MASK;
    aDestination->column.flag |= SMI_COLUMN_TYPE_FIXED;

    // BUG-38494 Compressed Column
    aDestination->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
    aDestination->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;
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
const void* mtc::value( const mtcColumn* aColumn,
                        const void*      aRow,
                        UInt             aFlag )
{
    // Proj-1391 Removing variable NULL Project
    const void* sValue =
        mtd::valueForModule( (smiColumn*)aColumn, aRow, aFlag,
                             aColumn->module->staticNull );

    return sValue;
}

#ifdef ENDIAN_IS_BIG_ENDIAN
UInt mtc::hash( UInt         aHash,
                const UChar* aValue,
                UInt         aLength )
{
    UChar        sHash[4];
    const UChar* sFence;

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
        sHash[0] = hashPermut[sHash[0]^*aValue];
        sHash[1] = hashPermut[sHash[1]^*aValue];
        sHash[2] = hashPermut[sHash[2]^*aValue];
        sHash[3] = hashPermut[sHash[3]^*aValue];
    }

    return sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3];
}
#else
UInt mtc::hash(UInt         aHash,
               const UChar* aValue,
               UInt         aLength )
{
    UChar        sHash[4];
    const UChar* sFence;

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
        sHash[0] = hashPermut[sHash[0]^*sFence];
        sHash[1] = hashPermut[sHash[1]^*sFence];
        sHash[2] = hashPermut[sHash[2]^*sFence];
        sHash[3] = hashPermut[sHash[3]^*sFence];
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
UInt mtc::hashWithExponent( UInt         aHash,
                            const UChar* aValue,
                            UInt         aLength )
{
    UChar        sHash[4];
    const UChar* sFence;

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
        sHash[0] = hashPermut[sHash[0]^*aValue];
        sHash[1] = hashPermut[sHash[1]^*aValue];
        sHash[2] = hashPermut[sHash[2]^*aValue];
        sHash[3] = hashPermut[sHash[3]^*aValue];
    }

    return sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3];
}

UInt mtc::hashSkip( UInt         aHash,
                    const UChar* aValue,
                    UInt         aLength )
{
    UChar        sHash[4];
    UInt         sSkip;
    const UChar* sFence;
    const UChar* sSubFence;

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
            sHash[0] = hashPermut[sHash[0]^*aValue];
            sHash[1] = hashPermut[sHash[1]^*aValue];
            sHash[2] = hashPermut[sHash[2]^*aValue];
            sHash[3] = hashPermut[sHash[3]^*aValue];
        }
        aValue += sSkip;
        sSkip <<= 1;
    }

    return (sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3]);
}

UInt mtc::hashSkipWithoutZero( UInt         aHash,
                               const UChar* aValue,
                               UInt         aLength )
{
    UChar        sHash[4];
    UInt         sSkip;
    const UChar* sFence;
    const UChar* sSubFence;

    sSkip   = 2;
    sFence  = aValue + aLength;

    sHash[3] = aHash;
    aHash >>= 8;
    sHash[2] = aHash;
    aHash >>= 8;
    sHash[1] = aHash;
    aHash >>= 8;
    sHash[0] = aHash;

    while ( aValue < sFence )
    {
        sSubFence = aValue + 2;
        if ( sSubFence > sFence )
        {
            sSubFence = sFence;
        }
        else
        {
            // Nothing to do.
        }
        for ( ; aValue < sSubFence; aValue++ )
        {
            if ( *aValue != 0x00 )
            {
                sHash[0] = hashPermut[sHash[0]^*aValue];
                sHash[1] = hashPermut[sHash[1]^*aValue];
                sHash[2] = hashPermut[sHash[2]^*aValue];
                sHash[3] = hashPermut[sHash[3]^*aValue];
            }
            else
            {
                // Nothing to do.
            }
        }
        aValue += sSkip;
        sSkip <<= 1;
    }

    return (sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3]);
}

UInt mtc::hashWithoutSpace( UInt         aHash,
                            const UChar* aValue,
                            UInt         aLength )
{
    // To Fix PR-11941
    // ������ String�� ��� ���� ���ϰ� �ſ� �ɰ���.
    // ����, Space�� ������ ��� String�� ���ϵ��� ��.

    UChar        sHash[4];
    const UChar* sFence;

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
            sHash[0] = hashPermut[sHash[0]^*aValue];
            sHash[1] = hashPermut[sHash[1]^*aValue];
            sHash[2] = hashPermut[sHash[2]^*aValue];
            sHash[3] = hashPermut[sHash[3]^*aValue];
        }
    }

    // To Fix PR-11941
    /*
      UChar        sHash[4];
      UInt         sSkip;
      const UChar* sFence;
      const UChar* sSubFence;

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
      sHash[0] = hashPermut[sHash[0]^*aValue];
      sHash[1] = hashPermut[sHash[1]^*aValue];
      sHash[2] = hashPermut[sHash[2]^*aValue];
      sHash[3] = hashPermut[sHash[3]^*aValue];
      }
      }
      aValue += sSkip;
      sSkip <<= 1;
      }
    */

    return (sHash[0]<<24|sHash[1]<<16|sHash[2]<<8|sHash[3]);
}

IDE_RC mtc::makeNumeric( mtdNumericType* aNumeric,
                         UInt            aMaximumMantissa,
                         const UChar*    aString,
                         UInt            aLength )
{
    const UChar* sMantissaStart;
    const UChar* sMantissaEnd;
    const UChar* sPointPosition;
    UInt         sOffset;
    SInt         sSign;
    SInt         sExponentSign;
    SInt         sExponent;
    SInt         sMantissaIterator;

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
        IDE_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
        switch( aString[sOffset] )
        {
            case '-':
                sSign = -1;
            case '+':
                sOffset++;
                IDE_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
        }
        if( aString[sOffset] == '.' )
        {
            sMantissaStart = &(aString[sOffset]);
            sPointPosition = &(aString[sOffset]);
            sOffset++;
            IDE_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
            IDE_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
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
            IDE_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
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
                IDE_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
                switch( aString[sOffset] )
                {
                    case '-':
                        sExponentSign = -1;
                    case '+':
                        sOffset++;
                        IDE_TEST_RAISE( sOffset >= aLength, ERR_INVALID_LITERAL );
                }
                if( sExponentSign > 0 )
                {
                    IDE_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
                                    ERR_INVALID_LITERAL );
                    for( ; sOffset < aLength; sOffset++ )
                    {
                        if( aString[sOffset] < '0' || aString[sOffset] > '9' )
                        {
                            break;
                        }
                        sExponent = sExponent * 10 + aString[sOffset] - '0';
                        IDE_TEST_RAISE( sExponent > 100000000,
                                        ERR_VALUE_OVERFLOW );
                    }
                    sExponent *= sExponentSign;
                }
                else
                {
                    IDE_TEST_RAISE( aString[sOffset] < '0' || aString[sOffset] > '9',
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
        IDE_TEST_RAISE( sOffset < aLength, ERR_INVALID_LITERAL );
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
                     sMantissaIterator < (SInt)aMaximumMantissa;
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
                if( sMantissaIterator < (SInt)aMaximumMantissa &&
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

                IDE_TEST_RAISE( sExponent > 63, ERR_VALUE_OVERFLOW );
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
                     sMantissaIterator < (SInt)aMaximumMantissa;
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
                if( sMantissaIterator < (SInt)aMaximumMantissa &&
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

                IDE_TEST_RAISE( sExponent > 63, ERR_VALUE_OVERFLOW );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::numericCanonize( mtdNumericType *aValue,
                             mtdNumericType *aCanonizedValue,
                             SInt            aCanonPrecision,
                             SInt            aCanonScale,
                             idBool         *aCanonized )
{
    SInt sMaximumExponent;
    SInt sMinimumExponent;
    SInt sExponent;
    SInt sExponentEnd;
    SInt sCount;

    *aCanonized = ID_TRUE;

    if( aValue->length > 1 )
    {
        sMaximumExponent =
            ( ( ( aCanonPrecision - aCanonScale ) + 2001 ) >> 1 ) - 1000;
        sMinimumExponent =
            ( ( 2002 - aCanonScale ) >> 1 ) - 1000;
        if( aValue->signExponent & 0x80 )
        {
            sExponent = ( aValue->signExponent & 0x7F ) - 64;
            IDE_TEST_RAISE( sExponent > sMaximumExponent,
                            ERR_VALUE_OVERFLOW );
            if( sExponent == sMaximumExponent )
            {
                if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                {
                    IDE_TEST_RAISE( aValue->mantissa[0] >= 10,
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
                            IDE_TEST_RAISE( aCanonizedValue->signExponent
                                            == 0xFF,
                                            ERR_VALUE_OVERFLOW );
                            aCanonizedValue->signExponent++;
                            aCanonizedValue->length      = 2;
                            aCanonizedValue->mantissa[0] = 1;
                        }
                        sExponent =
                            ( aCanonizedValue->signExponent & 0x7F ) - 64;
                        IDE_TEST_RAISE( sExponent > sMaximumExponent,
                                        ERR_VALUE_OVERFLOW );
                        if( sExponent == sMaximumExponent )
                        {
                            if( ( aCanonPrecision - aCanonScale )
                                & 0x01 )
                            {
                                IDE_TEST_RAISE( aCanonizedValue->mantissa[0]
                                                >= 10,
                                                ERR_VALUE_OVERFLOW );
                            }
                        }
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
            else if ( sExponent == sMinimumExponent - 1 &&
                      ( aCanonScale & 0x01 ) == 0 )
            {
                if( aValue->mantissa[0] >= 50 )
                {
                    IDE_TEST_RAISE( aValue->signExponent == 0xFF,
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
                IDE_TEST_RAISE( sExponent > sMaximumExponent,
                                ERR_VALUE_OVERFLOW );
                if( sExponent == sMaximumExponent )
                {
                    if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                    {
                        IDE_TEST_RAISE( aCanonizedValue->mantissa[0] >= 10,
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
            IDE_TEST_RAISE( sExponent > sMaximumExponent,
                            ERR_VALUE_OVERFLOW );
            if( sExponent == sMaximumExponent )
            {
                if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                {
                    IDE_TEST_RAISE( aValue->mantissa[0] <= 89,
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
                            IDE_TEST_RAISE( aCanonizedValue->signExponent
                                            == 0x01,
                                            ERR_VALUE_OVERFLOW );
                            aCanonizedValue->signExponent--;
                            aCanonizedValue->length      = 2;
                            aCanonizedValue->mantissa[0] = 98;
                        }
                        sExponent =
                            64 - ( aCanonizedValue->signExponent & 0x7F );
                        IDE_TEST_RAISE( sExponent > sMaximumExponent,
                                        ERR_VALUE_OVERFLOW );
                        if( sExponent == sMaximumExponent )
                        {
                            if( ( aCanonPrecision - aCanonScale )
                                & 0x01 )
                            {
                                IDE_TEST_RAISE( aCanonizedValue->mantissa[0]
                                                <= 89,
                                                ERR_VALUE_OVERFLOW );
                            }
                        }
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
            else if ( sExponent == sMinimumExponent - 1 &&
                      ( aCanonScale & 0x01 ) == 0 )
            {
                if( aValue->mantissa[0] <= 49 )
                {
                    IDE_TEST_RAISE( aValue->signExponent == 0x01,
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
                IDE_TEST_RAISE( sExponent > sMaximumExponent,
                                ERR_VALUE_OVERFLOW );
                if( sExponent == sMaximumExponent )
                {
                    if( ( aCanonPrecision - aCanonScale ) & 0x01 )
                    {
                        IDE_TEST_RAISE( aCanonizedValue->mantissa[0] <= 89,
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
        *aCanonized = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC mtc::floatCanonize( mtdNumericType *aValue,
                           mtdNumericType *aCanonizedValue,
                           SInt            aCanonPrecision,
                           idBool         *aCanonized )
{
    SInt sPrecision;
    SInt sCount;
    SInt sFence;

    *aCanonized = ID_TRUE;

    if( aValue->length > 1 )
    {
        sFence = aValue->length - 2;
        sPrecision = ( aValue->length - 1 ) << 1;
        if( aValue->signExponent & 0x80 )
        {
            if( aValue->mantissa[0] < 10 )
            {
                sPrecision--;
                if( aValue->mantissa[sFence] % 10 == 0 )
                {
                    sPrecision--;
                }
                if( sPrecision > aCanonPrecision )
                {
                    aCanonizedValue->signExponent = aValue->signExponent;
                    if( aCanonPrecision & 0x01 )
                    {
                        sCount = aCanonPrecision >> 1;
                        aCanonizedValue->length = sCount + 2;
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
                    else
                    {
                        sCount = aCanonPrecision >> 1;
                        aCanonizedValue->length = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 >= 5 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 10;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10;
                        }
                        sCount--;
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
                    for( ; sCount >= 0; sCount-- )
                    {
                        aCanonizedValue->mantissa[sCount] =
                            aValue->mantissa[sCount];
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
            else
            {
                if( aValue->mantissa[sFence] % 10 == 0 )
                {
                    sPrecision--;
                }
                if( sPrecision > aCanonPrecision )
                {
                    aCanonizedValue->signExponent = aValue->signExponent;
                    if( aCanonPrecision & 0x01 )
                    {
                        sCount = ( aCanonPrecision - 1 ) >> 1;
                        aCanonizedValue->length = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 >= 5 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 10;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10;
                        }
                        sCount--;
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
                        sCount = ( aCanonPrecision - 1 ) >> 1;
                        aCanonizedValue->length = sCount + 2;
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
                    if( aCanonizedValue->mantissa[0] > 99 )
                    {
                        IDE_TEST_RAISE( aCanonizedValue->signExponent == 0xFF,
                                        ERR_VALUE_OVERFLOW );
                        aCanonizedValue->signExponent++;
                        aCanonizedValue->length      = 2;
                        aCanonizedValue->mantissa[0] = 1;
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
        }
        else
        {
            if( aValue->mantissa[0] > 89 )
            {
                sPrecision--;
                if( aValue->mantissa[sFence] % 10 == 9 )
                {
                    sPrecision--;
                }
                if( sPrecision > aCanonPrecision )
                {
                    aCanonizedValue->signExponent = aValue->signExponent;
                    if( aCanonPrecision & 0x01 )
                    {
                        sCount = aCanonPrecision >> 1;
                        aCanonizedValue->length = sCount + 2;
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
                    else
                    {
                        sCount = aCanonPrecision >> 1;
                        aCanonizedValue->length = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 <= 4 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 - 1;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 9;
                        }
                        sCount--;
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
                    for( ; sCount >= 0; sCount-- )
                    {
                        aCanonizedValue->mantissa[sCount] =
                            aValue->mantissa[sCount];
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
            else
            {
                if( aValue->mantissa[sFence] % 10 == 9 )
                {
                    sPrecision--;
                }
                if( sPrecision > aCanonPrecision )
                {
                    aCanonizedValue->signExponent = aValue->signExponent;
                    if( aCanonPrecision & 0x01 )
                    {
                        sCount = ( aCanonPrecision - 1 ) >> 1;
                        aCanonizedValue->length = sCount + 2;
                        if( aValue->mantissa[sCount] % 10 <= 4 )
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 - 1;
                        }
                        else
                        {
                            aCanonizedValue->mantissa[sCount] =
                                aValue->mantissa[sCount] - aValue->mantissa[sCount] % 10 + 9;
                        }
                        sCount--;
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
                        sCount = ( aCanonPrecision - 1 ) >> 1;
                        aCanonizedValue->length = sCount + 2;
                        if( aValue->mantissa[sCount+1] <= 49 )
                        {
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
                    for( ; sCount >= 0; sCount-- )
                    {
                        aCanonizedValue->mantissa[sCount] =
                            aValue->mantissa[sCount];
                    }
                    if( aCanonizedValue->mantissa[0] > 99 )
                    {
                        IDE_TEST_RAISE( aCanonizedValue->signExponent == 0x01,
                                        ERR_VALUE_OVERFLOW );
                        aCanonizedValue->signExponent--;
                        aCanonizedValue->length      = 2;
                        // �����̹Ƿ� carry�߻��� ���� -1 ��, 98�� �Ǿ�� �Ѵ�.
                        aCanonizedValue->mantissa[0] = 98;
                    }
                }
                else
                {
                    *aCanonized = ID_FALSE;
                }
            }
        }
    }
    else
    {
        *aCanonized = ID_FALSE;
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * converts SLong to mtdNumericType
 */
void mtc::makeNumeric( mtdNumericType *aNumeric,
                       const SLong     aNumber )
{
    UInt   sCount;
    ULong  sULongValue;

    SShort i;
    SShort sExponent;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM];

    if ( aNumber == 0 )
    {
        aNumeric->length       = 1;
        aNumeric->signExponent = 0x80;
    }
    /* ��� */
    else if ( aNumber > 0 )
    {
        sULongValue = aNumber;

        for ( i = 9; i >= 0; i-- )
        {
            sMantissa[i] = (UChar)(sULongValue % 100);
            sULongValue /= 100;
        }

        for ( sExponent = 0; sExponent < 10; sExponent++ )
        {
            if ( sMantissa[sExponent] != 0 )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        for ( i = 0; i + sExponent < 10; i++ )
        {
            sMantissa[i] = sMantissa[i + sExponent];
        }

        for ( ; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
        {
            sMantissa[i] = 0;
        }
        sExponent = 10 - sExponent;

        aNumeric->signExponent = sExponent + 192;

        /* length ���� */
        for ( sCount = 0, aNumeric->length = 1;
             sCount < ID_SIZEOF(sMantissa) - 1;
             sCount++ )
        {
            if ( sMantissa[sCount] != 0 )
            {
                aNumeric->length = sCount + 2;
            }
            else
            {
                /* Nothing to do */
            }
        }

        for ( sCount = 0;
             sCount < (UInt)( aNumeric->length - 1 );
             sCount++ )
        {
            aNumeric->mantissa[sCount] = sMantissa[sCount];
        }

    }
    /* ���� */
    else
    {
        if ( aNumber == -ID_LONG(9223372036854775807) - ID_LONG(1) )
        {
            sULongValue = (ULong)(ID_ULONG(9223372036854775807) + ID_ULONG(1));
        }
        else
        {
            sULongValue = (ULong)(-aNumber);
        }

        for ( i = 9; i >= 0; i-- )
        {
            sMantissa[i] = (UChar)(sULongValue % 100);
            sULongValue /= 100;
        }

        for ( sExponent = 0; sExponent < 10; sExponent++ )
        {
            if ( sMantissa[sExponent] != 0 )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        for ( i = 0; i + sExponent < 10; i++ )
        {
            sMantissa[i] = sMantissa[i + sExponent];
        }

        for ( ; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
        {
            sMantissa[i] = 0;
        }
        sExponent = 10 - sExponent;

        aNumeric->signExponent = 64 - sExponent;

        /* length ���� */
        for ( sCount = 0, aNumeric->length = 1;
             sCount < ID_SIZEOF(sMantissa) - 1;
             sCount++ )
        {
            if ( sMantissa[sCount] != 0 )
            {
                aNumeric->length = sCount + 2;
            }
            else
            {
                /* Nothing to do */
            }
        }

        /* ���� ���� ��ȯ */
        for( sCount = 0;
             sCount < (UInt)( aNumeric->length - 1 );
             sCount++ )
        {
            aNumeric->mantissa[sCount] = 99 - sMantissa[sCount];
        }
    }
}

IDE_RC mtc::makeNumeric( mtdNumericType *aNumeric,
                         const SDouble   aNumber )
{
    SChar   sBuffer[32];

    if ( ( MTU_DOUBLE_TO_NUMERIC_FAST_CONV == 0 ) ||
         ( gIEEE754Double == ID_FALSE ) )
    {
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "%"ID_DOUBLE_G_FMT"",
                         aNumber );
    
        IDE_TEST( makeNumeric( aNumeric,
                               MTD_FLOAT_MANTISSA_MAXIMUM,
                               (const UChar*)sBuffer,
                               idlOS::strlen(sBuffer) )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( double2Numeric( aNumeric, aNumber )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC mtc::findCompare( const smiColumn* aColumn,
                         UInt             aFlag,
                         smiCompareFunc*  aCompare )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;
    UInt             sCompareType;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    sCompareType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;

    if ( (aColumn->flag & SMI_COLUMN_COMPRESSION_MASK) != SMI_COLUMN_COMPRESSION_TRUE )
    {
        if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_NORMAL )
        {
            if ( (aColumn->flag & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_FIXED )
            {
                sCompareType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
            }
            else
            {
                sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
            }
        }
        else if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_KEY_AND_VROW )
        {
            sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
        }
        else if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_KEY_AND_KEY )
        {
            sCompareType = MTD_COMPARE_STOREDVAL_STOREDVAL;
        }
        else
        {
            /* PROJ-2433
             * Direct Key�� compare �Լ� ������� type ���� */
            if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_DIRECT_KEY )
            {
                if ( (aColumn->flag & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_FIXED )
                {
                    sCompareType = MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL;
                }
                else
                {
                    sCompareType = MTD_COMPARE_INDEX_KEY_MTDVAL;
                }
            }
            else
            {
                ideLog::log( IDE_ERR_0,
                             "aFlag : %"ID_UINT32_FMT"\n"
                             "sColumn->type.dataTypeId : %"ID_UINT32_FMT"\n",
                             aFlag,
                             sColumn->type.dataTypeId );

                IDE_ASSERT( 0 );
            }
        }
    }
    else
    {
        /* PROJ-2433
         * Direct Key Index�� compressed column�� ���������ʴ´� */
        IDE_DASSERT( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) !=
                     SMI_COLUMN_COMPARE_DIRECT_KEY );

        if ((aColumn->flag & SMI_COLUMN_STORAGE_MASK) == SMI_COLUMN_STORAGE_MEMORY) 
        {
            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
        }
        else if ((aColumn->flag & SMI_COLUMN_STORAGE_MASK) == SMI_COLUMN_STORAGE_DISK)
        {
            if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_NORMAL )
            {
                if ( (aColumn->flag & SMI_COLUMN_TYPE_MASK ) == SMI_COLUMN_TYPE_FIXED )
                {
                    sCompareType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
                }
                else
                {
                    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
                }
            }
            else if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_KEY_AND_VROW )
            {
                sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
            }
            else if ( ( aFlag & SMI_COLUMN_COMPARE_TYPE_MASK ) == SMI_COLUMN_COMPARE_KEY_AND_KEY )
            {
                sCompareType = MTD_COMPARE_STOREDVAL_STOREDVAL;
            }
            else
            {
                ideLog::log( IDE_ERR_0,
                             "aFlag : %"ID_UINT32_FMT"\n"
                             "sColumn->type.dataTypeId : %"ID_UINT32_FMT"\n",
                             aFlag,
                             sColumn->type.dataTypeId );
 
                IDE_ASSERT( 0 );
            }
        }
        else
        {
            IDE_ASSERT( 0 );
        }
    }

    if( ( aFlag & SMI_COLUMN_ORDER_MASK ) == SMI_COLUMN_ORDER_ASCENDING )
    {
        *aCompare = (smiCompareFunc)
            sModule->keyCompare[sCompareType][MTD_COMPARE_ASCENDING];
    }
    else
    {
        *aCompare = (smiCompareFunc)
            sModule->keyCompare[sCompareType][MTD_COMPARE_DESCENDING];
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::findKey2String( const smiColumn   * aColumn,
                            UInt                /* aFlag */,
                            smiKey2StringFunc * aKey2String )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aKey2String = (smiKey2StringFunc) sModule->encode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::findNull( const smiColumn   * aColumn,
                      UInt                /* aFlag */,
                      smiNullFunc       * aNull )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aNull = (smiNullFunc)sModule->null;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::findIsNull( const smiColumn * aColumn,
                        UInt             /* aFlag */,
                        smiIsNullFunc   * aIsNull )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aIsNull = (smiIsNullFunc)sModule->isNull;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
mtc::findHashKeyFunc( const smiColumn * aColumn,
                      smiHashKeyFunc * aHashKeyFunc )
{
/***********************************************************************
 *
 * Description :
 *    Column�� Data Type�� �����ϴ� Hash Key ���� �Լ��� ��´�.
 *    Data Type�� �� �� ���� ���� layer(SM)���� hash key ���� �Լ���
 *    ��� ���� ����Ѵ�.
 *
 * Implementation :
 *
 *    Column�� ������ Type�� �ش��ϴ� ����� ȹ���ϰ�,
 *    �̿� ���� Hash Key ���� �Լ��� Return�Ѵ�.
 *
 ***********************************************************************/
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aHashKeyFunc = (smiHashKeyFunc) sModule->hash;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
mtc::findStoredValue2MtdValue( const smiColumn             * aColumn,
                               smiCopyDiskColumnValueFunc  * aStoredValue2MtdValueFunc )
{
/***********************************************************************
 *
 * Description :
 *    Column�� Data Type�� �����ϴ� column value�� �����ϴ� �Լ��� ��´�.
 *    Data Type�� �� �� ���� ���� layer(SM)���� colum value�� �����ϴ�
 *    �Լ��� ��� ���� ����Ѵ�.
 *
 *    PROJ-2429 Dictionary based data compress for on-disk DB
 *    dictionary compression colunm�� ���
 *    mtd::mtdStoredValue2MtdValue4CompressColumn�Լ��� ��ȯ�ϰ�
 *    �׷��� ������� ������Ÿ�Կ� �´� ��ũ���̺��÷���
 *    ����Ÿ�� �����ϴ� �Լ��� ��ȯ �Ѵ�.
 *
 * Implementation :
 *
 *    Column�� ������ Type�� �ش��ϴ� ����� ȹ���ϰ�,
 *    �̿� ���� column value�� �����ϴ� �Լ��� �����Ѵ�.
 ***********************************************************************/

    const mtcColumn* sColumn;
    const mtdModule* sModule;
    UInt             sFunctionIdx;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    if ( (aColumn->flag & SMI_COLUMN_COMPRESSION_MASK) 
        != SMI_COLUMN_COMPRESSION_TRUE )
    {
        sFunctionIdx = MTD_COLUMN_COPY_FUNC_NORMAL;
    }
    else
    {
        sFunctionIdx = MTD_COLUMN_COPY_FUNC_COMPRESSION;
    }

    *aStoredValue2MtdValueFunc =
        (smiCopyDiskColumnValueFunc)sModule->storedValue2MtdValue[sFunctionIdx];

    IDE_ASSERT( *aStoredValue2MtdValueFunc != NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
mtc::findStoredValue2MtdValue4DataType( const smiColumn             * aColumn,
                                        smiCopyDiskColumnValueFunc  * aStoredValue2MtdValueFunc )
{
/***********************************************************************
 *
 * Description :
 *    Column�� Data Type�� �����ϴ� column value�� �����ϴ� �Լ��� ��´�.
 *    Data Type�� �� �� ���� ���� layer(SM)���� colum value�� �����ϴ�
 *    �Լ��� ��� ���� ����Ѵ�.
 *
 *    PROJ-2429 Dictionary based data compress for on-disk DB 
 *    dictionary compression colunm�� �������
 *    ������Ÿ�Կ� �´� ��ũ���̺��÷���  ����Ÿ�� �����ϴ�
 *    �Լ��� ��ȯ �Ѵ�.
 *
 *
 * Implementation :
 *
 *    Column�� ������ Type�� �ش��ϴ� ����� ȹ���ϰ�,
 *    �̿� ���� column value�� �����ϴ� �Լ��� �����Ѵ�.
 ***********************************************************************/

    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aStoredValue2MtdValueFunc =
        (smiCopyDiskColumnValueFunc)sModule->storedValue2MtdValue[MTD_COLUMN_COPY_FUNC_NORMAL];

    IDE_ASSERT( *aStoredValue2MtdValueFunc != NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC mtc::findActualSize( const smiColumn   * aColumn,
                            smiActualSizeFunc * aActualSizeFunc )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aActualSizeFunc = (smiActualSizeFunc)sModule->actualSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// Added For compacting Disk Index Slots by xcom73
IDE_RC mtc::getAlignValue( const smiColumn * aColumn,
                           UInt *            aAlignValue )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
              != IDE_SUCCESS );

    *aAlignValue = sModule->align;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//--------------------------------------------------
// PROJ-1705
// sm���� ���̺��ڵ�κ��� �ε������ڵ带 �����ϱ� ����
// �ش� �÷�����Ÿ�� size�� value ptr�� ���Ѵ�.
// SM���� �ε������ڵ带 �����ϱ� ���ؼ� �Ʒ��� ������ ����Ǹ�,
//  (1) SM ��ũ���ڵ带 �о ->
//  (2) QP ���� ó�� ������ ���ڵ� ����  ->
//  (3) SM �ε������ڵ� ����
// �� �Լ��� (2)->(3)�� ����� ȣ��ȴ�.
//
// RP������ �� �Լ��� ȣ��Ǵµ�,
// log�� �о� mtdType�� value�� �����ϰ�,
// �� value�� mtdType�� length�� ���Ҷ� ȣ��ȴ�.
//--------------------------------------------------
IDE_RC mtc::getValueLengthFromFetchBuffer( idvSQL          * /*aStatistic*/,
                                           const smiColumn * aColumn,
                                           const void      * aRow,
                                           UInt            * aColumnSize,
                                           idBool          * aIsNullValue )
{
    const mtcColumn * sColumn;

    sColumn = (const mtcColumn*)aColumn;

    *aColumnSize = sColumn->module->actualSize( sColumn, aRow );

    if( aIsNullValue != NULL )
    {
        *aIsNullValue = sColumn->module->isNull( sColumn, aRow );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

//    IDE_EXCEPTION_END;

//    return IDE_FAILURE;
}

//--------------------------------------------------
// PROJ-1705
// ��ũ���̺��÷��� ����Ÿ��
// qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
//--------------------------------------------------
void mtc::storedValue2MtdValue( const smiColumn* aColumn,
                                void           * aDestValue,
                                UInt             aDestValueOffset,
                                UInt             aLength,
                                const void     * aValue )
{
    const mtcColumn * sColumn;
    UInt              sFunctionIdx;

    sColumn = (const mtcColumn*)aColumn;

    // PROJ-2429 Dictionary based data compress for on-disk DB
    // Dictionary compression column���� Ȯ���ϰ� dictionary column copy�Լ���
    // �����Ѵ�. Dictionary compression column�� �ƴѰ�� data type�� �´� �Լ���
    // ���� �Ѵ�.
    if ( (aColumn->flag & SMI_COLUMN_COMPRESSION_MASK) 
        != SMI_COLUMN_COMPRESSION_TRUE )
    {
        sFunctionIdx = MTD_COLUMN_COPY_FUNC_NORMAL;
    }
    else
    {
        sFunctionIdx = MTD_COLUMN_COPY_FUNC_COMPRESSION;
    }

    sColumn->module->storedValue2MtdValue[sFunctionIdx]( sColumn->column.size,
                                                         aDestValue,
                                                         aDestValueOffset,
                                                         aLength,
                                                         aValue );
}


IDE_RC mtc::cloneTuple( iduMemory   * aMemory,
                        mtcTemplate * aSrcTemplate,
                        UShort        aSrcTupleID,
                        mtcTemplate * aDstTemplate,
                        UShort        aDstTupleID )
{
    ULong sFlag;
    SInt  i;

    aDstTemplate->rows[aDstTupleID].modify
        = aSrcTemplate->rows[aSrcTupleID].modify ;
    aDstTemplate->rows[aDstTupleID].lflag
        = aSrcTemplate->rows[aSrcTupleID].lflag ;
    aDstTemplate->rows[aDstTupleID].columnCount
        = aSrcTemplate->rows[aSrcTupleID].columnCount ;
    // ���� �÷� ������ �����ؾ� �Ѵ�.
    aDstTemplate->rows[aDstTupleID].columnMaximum
        = aSrcTemplate->rows[aSrcTupleID].columnCount;
    aDstTemplate->rows[aDstTupleID].rowOffset
        = aSrcTemplate->rows[aSrcTupleID].rowOffset ;
    aDstTemplate->rows[aDstTupleID].rowMaximum
        = aSrcTemplate->rows[aSrcTupleID].rowMaximum ;
    aDstTemplate->rows[aDstTupleID].columns = NULL ;
    aDstTemplate->rows[aDstTupleID].execute = NULL ;
    aDstTemplate->rows[aDstTupleID].row = NULL ;

    // PROJ-1502 PARTITIONED DISK TABLE
    aDstTemplate->rows[aDstTupleID].partitionTupleID = aDstTupleID;

    // PROJ-1502 PARTITIONED DISK TABLE
    aDstTemplate->rows[aDstTupleID].tableHandle = aSrcTemplate->rows[aSrcTupleID].tableHandle;

    // PROJ-2205 rownum in DML
    aDstTemplate->rows[aDstTupleID].cursorInfo = NULL;

    sFlag = aSrcTemplate->rows[aSrcTupleID].lflag;


    /* clone mtcTuple.columns */
    if ( sFlag & MTC_TUPLE_COLUMN_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].columns
            = aSrcTemplate->rows[aSrcTupleID].columns;
    }

    if ( sFlag & MTC_TUPLE_COLUMN_ALLOCATE_TRUE )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF(mtcColumn) *
                                 aDstTemplate->rows[aDstTupleID].columnMaximum,
                                 (void**)
                                 &(aDstTemplate->rows[aDstTupleID].columns))
                 != IDE_SUCCESS);
    }

    if ( sFlag & MTC_TUPLE_COLUMN_COPY_TRUE )
    {
        for( i = 0 ; i < aDstTemplate->rows[aDstTupleID].columnMaximum; i++ )
        {
            /* BUG-44382 clone tuple ���ɰ��� */
            if ( ( sFlag & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_TABLE )
            {
                aDstTemplate->rows[aDstTupleID].columns[i] =
                    aSrcTemplate->rows[aSrcTupleID].columns[i];
            }
            else
            {
                // intermediate tuple
                MTC_COPY_COLUMN_DESC( &(aDstTemplate->rows[aDstTupleID].columns[i]),
                                      &(aSrcTemplate->rows[aSrcTupleID].columns[i]) );
            }
        }
        /*
          idlOS::memcpy( aDstTemplate->rows[aDstTupleID].columns,
          aSrcTemplate->rows[aSrcTupleID].columns,
          ID_SIZEOF(mtcColumn)
          * aDstTemplate->rows[aDstTupleID].columnMaximum );
          */
    }

    /* clone mtcTuple.execute */
    if ( sFlag & MTC_TUPLE_EXECUTE_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].execute
            = aSrcTemplate->rows[aSrcTupleID].execute;
    }

    if ( sFlag & MTC_TUPLE_EXECUTE_ALLOCATE_TRUE )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF(mtcExecute) *
                                 aDstTemplate->rows[aDstTupleID].columnMaximum,
                                 (void**)
                                 &(aDstTemplate->rows[aDstTupleID].execute))
                 != IDE_SUCCESS);
    }

    if ( sFlag & MTC_TUPLE_EXECUTE_COPY_TRUE )
    {
        if (aSrcTemplate->rows[aSrcTupleID].execute != NULL)
        {
            for( i = 0; i < aDstTemplate->rows[aDstTupleID].columnMaximum; i++ )
            {
                aDstTemplate->rows[aDstTupleID].execute[i] =
                    aSrcTemplate->rows[aSrcTupleID].execute[i];
            }
            /*
              idlOS::memcpy( aDstTemplate->rows[aDstTupleID].execute,
              aSrcTemplate->rows[aSrcTupleID].execute,
              ID_SIZEOF(mtcExecute)
              * aDstTemplate->rows[aDstTupleID].columnMaximum );
              */
        }
    }

    /* PROJ-1789 PROWID */
    aDstTemplate->rows[aDstTupleID].ridExecute =
        aSrcTemplate->rows[aSrcTupleID].ridExecute;

    /* clone mtcTuple.row */
    if ( sFlag & MTC_TUPLE_ROW_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].row
            = aSrcTemplate->rows[aSrcTupleID].row;
    }

    if ( sFlag & MTC_TUPLE_ROW_ALLOCATE_TRUE )
    {
        if ( aDstTemplate->rows[aDstTupleID].rowMaximum > 0 )
        {
            /* BUG-44382 clone tuple ���ɰ��� */
            if ( sFlag & MTC_TUPLE_ROW_MEMSET_TRUE )
            {
                // BUG-15548
                IDE_TEST( aMemory->cralloc(
                              ID_SIZEOF(SChar) * aDstTemplate->rows[aDstTupleID].rowMaximum,
                              (void**)&(aDstTemplate->rows[aDstTupleID].row))
                          != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST( aMemory->alloc(
                              ID_SIZEOF(SChar) * aDstTemplate->rows[aDstTupleID].rowMaximum,
                              (void**)&(aDstTemplate->rows[aDstTupleID].row))
                          != IDE_SUCCESS);
            }
        }
        else
        {
            aDstTemplate->rows[aDstTupleID].row = NULL;
        }
    }

    if ( sFlag & MTC_TUPLE_ROW_COPY_TRUE )
    {
        idlOS::memcpy( aDstTemplate->rows[aDstTupleID].row,
                       aSrcTemplate->rows[aSrcTupleID].row,
                       ID_SIZEOF(SChar)
                       * aDstTemplate->rows[aDstTupleID].rowMaximum );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::cloneTuple( iduVarMemList * aMemory,
                        mtcTemplate   * aSrcTemplate,
                        UShort          aSrcTupleID,
                        mtcTemplate   * aDstTemplate,
                        UShort          aDstTupleID )
{
    ULong sFlag;
    SInt  i;

    aDstTemplate->rows[aDstTupleID].modify
        = aSrcTemplate->rows[aSrcTupleID].modify ;
    aDstTemplate->rows[aDstTupleID].lflag
        = aSrcTemplate->rows[aSrcTupleID].lflag ;
    aDstTemplate->rows[aDstTupleID].columnCount
        = aSrcTemplate->rows[aSrcTupleID].columnCount ;
    // ���� �÷� ������ �����ؾ� �Ѵ�.
    aDstTemplate->rows[aDstTupleID].columnMaximum
        = aSrcTemplate->rows[aSrcTupleID].columnCount;
    aDstTemplate->rows[aDstTupleID].rowOffset
        = aSrcTemplate->rows[aSrcTupleID].rowOffset ;
    aDstTemplate->rows[aDstTupleID].rowMaximum
        = aSrcTemplate->rows[aSrcTupleID].rowMaximum ;
    aDstTemplate->rows[aDstTupleID].columns = NULL ;
    aDstTemplate->rows[aDstTupleID].execute = NULL ;
    aDstTemplate->rows[aDstTupleID].row = NULL ;

    // PROJ-1502 PARTITIONED DISK TABLE
    aDstTemplate->rows[aDstTupleID].partitionTupleID = aDstTupleID;

    // PROJ-1502 PARTITIONED DISK TABLE
    aDstTemplate->rows[aDstTupleID].tableHandle = aSrcTemplate->rows[aSrcTupleID].tableHandle;

    // PROJ-2205 rownum in DML
    aDstTemplate->rows[aDstTupleID].cursorInfo = NULL;

    sFlag = aSrcTemplate->rows[aSrcTupleID].lflag;


    /* clone mtcTuple.columns */
    if ( sFlag & MTC_TUPLE_COLUMN_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].columns
            = aSrcTemplate->rows[aSrcTupleID].columns;
    }

    if ( sFlag & MTC_TUPLE_COLUMN_ALLOCATE_TRUE )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF(mtcColumn) *
                                 aDstTemplate->rows[aDstTupleID].columnMaximum,
                                 (void**)
                                 &(aDstTemplate->rows[aDstTupleID].columns))
                 != IDE_SUCCESS);
    }

    if ( sFlag & MTC_TUPLE_COLUMN_COPY_TRUE )
    {
        for( i = 0 ; i < aDstTemplate->rows[aDstTupleID].columnMaximum; i++ )
        {
            /* BUG-44382 clone tuple ���ɰ��� */
            if ( ( sFlag & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_TABLE )
            {
                aDstTemplate->rows[aDstTupleID].columns[i] =
                    aSrcTemplate->rows[aSrcTupleID].columns[i];
            }
            else
            {
                // intermediate tuple
                MTC_COPY_COLUMN_DESC( &(aDstTemplate->rows[aDstTupleID].columns[i]),
                                      &(aSrcTemplate->rows[aSrcTupleID].columns[i]) );
            }
        }
        /*
          idlOS::memcpy( aDstTemplate->rows[aDstTupleID].columns,
          aSrcTemplate->rows[aSrcTupleID].columns,
          ID_SIZEOF(mtcColumn)
          * aDstTemplate->rows[aDstTupleID].columnMaximum );
          */
    }

    /* clone mtcTuple.execute */
    if ( sFlag & MTC_TUPLE_EXECUTE_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].execute
            = aSrcTemplate->rows[aSrcTupleID].execute;
    }

    if ( sFlag & MTC_TUPLE_EXECUTE_ALLOCATE_TRUE )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF(mtcExecute) *
                                 aDstTemplate->rows[aDstTupleID].columnMaximum,
                                 (void**)
                                 &(aDstTemplate->rows[aDstTupleID].execute))
                 != IDE_SUCCESS);
    }

    if ( sFlag & MTC_TUPLE_EXECUTE_COPY_TRUE )
    {
        if (aSrcTemplate->rows[aSrcTupleID].execute != NULL)
        {
            for( i = 0; i < aDstTemplate->rows[aDstTupleID].columnMaximum; i++ )
            {
                aDstTemplate->rows[aDstTupleID].execute[i] =
                    aSrcTemplate->rows[aSrcTupleID].execute[i];
            }
            /*
              idlOS::memcpy( aDstTemplate->rows[aDstTupleID].execute,
              aSrcTemplate->rows[aSrcTupleID].execute,
              ID_SIZEOF(mtcExecute)
              * aDstTemplate->rows[aDstTupleID].columnMaximum );
              */
        }
    }

    /* PROJ-1789 PROWID */
    aDstTemplate->rows[aDstTupleID].ridExecute=
        aSrcTemplate->rows[aSrcTupleID].ridExecute;

    /* clone mtcTuple.row */
    if ( sFlag & MTC_TUPLE_ROW_SET_TRUE )
    {
        aDstTemplate->rows[aDstTupleID].row
            = aSrcTemplate->rows[aSrcTupleID].row;
    }

    if ( sFlag & MTC_TUPLE_ROW_ALLOCATE_TRUE )
    {
        if ( aDstTemplate->rows[aDstTupleID].rowMaximum > 0 )
        {
            /* BUG-44382 clone tuple ���ɰ��� */
            if ( sFlag & MTC_TUPLE_ROW_MEMSET_TRUE )
            {
                // BUG-15548
                IDE_TEST( aMemory->cralloc(
                              ID_SIZEOF(SChar) * aDstTemplate->rows[aDstTupleID].rowMaximum,
                              (void**)&(aDstTemplate->rows[aDstTupleID].row))
                          != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST( aMemory->alloc(
                              ID_SIZEOF(SChar) * aDstTemplate->rows[aDstTupleID].rowMaximum,
                              (void**)&(aDstTemplate->rows[aDstTupleID].row))
                          != IDE_SUCCESS);
            }
        }
        else
        {
            aDstTemplate->rows[aDstTupleID].row = NULL;
        }
    }

    if ( sFlag & MTC_TUPLE_ROW_COPY_TRUE )
    {
        idlOS::memcpy( aDstTemplate->rows[aDstTupleID].row,
                       aSrcTemplate->rows[aSrcTupleID].row,
                       ID_SIZEOF(SChar)
                       * aDstTemplate->rows[aDstTupleID].rowMaximum );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 *
 * ���� �����߿� dateFormat�� �߰��Ǿ��µ�
 * �� ������ cloneTemplate�� source, target�� �����ϰ� �Ǿ� �ִ�.
 * ���� �� �Լ��� ���� ���� �������� ���Ǿ�� �Ѵ�.
 * Ȥ�� �ٸ� ������ template�� �����ϴ� ��쿡��
 * �ݵ�� dateFormat�� ���� assign �� �־�� �Ѵ�.
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 * By kumdory, 2005-04-08
 *
 ******************************************************************/
IDE_RC mtc::cloneTemplate( iduMemory    * aMemory,
                           mtcTemplate  * aSource,
                           mtcTemplate  * aDestination )
{
    UShort sTupleIndex;

    aDestination->stackCount      = aSource->stackCount;
    aDestination->dataSize        = aSource->dataSize;

    // To Fix PR-9600
    // To Fix PR-9600
    // mtcTemplate::execInfo, mtcTemplate::execInfoCnt ����� �߰���
    // �ش� ������ ���� ���縦 ���־�� ��.
    aDestination->execInfoCnt     = aSource->execInfoCnt;

    aDestination->initSubquery    = aSource->initSubquery;
    aDestination->fetchSubquery   = aSource->fetchSubquery;
    aDestination->finiSubquery    = aSource->finiSubquery;
    aDestination->setCalcSubquery = aSource->setCalcSubquery;
    aDestination->getOpenedCursor = aSource->getOpenedCursor;
    aDestination->addOpenedLobCursor = aSource->addOpenedLobCursor;
    aDestination->isBaseTable     = aSource->isBaseTable;
    aDestination->closeLobLocator = aSource->closeLobLocator;
    aDestination->getSTObjBufSize = aSource->getSTObjBufSize;
    aDestination->rowCount        = aSource->rowCount;
    aDestination->rowCountBeforeBinding  = aSource->rowCountBeforeBinding;
    aDestination->variableRow     = aSource->variableRow;

    // PROJ-2002 Column Security
    aDestination->encrypt          = aSource->encrypt;
    aDestination->decrypt          = aSource->decrypt;
    aDestination->encodeECC        = aSource->encodeECC;
    aDestination->getDecryptInfo   = aSource->getDecryptInfo;
    aDestination->getECCInfo       = aSource->getECCInfo;
    aDestination->getECCSize       = aSource->getECCSize;

    // BUG-11192����
    // template�� dateFormat�� �߰��Ǹ鼭 cloneTemplate����
    // ���縦 ���־�� �Ѵ�.
    // cloneTemplate�� ���� ���ǳ������� ���ǹǷ�
    // �����͸� assign�ϸ� �ȴ�. (malloc�ؼ� ī���� �ʿ�� ����)
    aDestination->dateFormat      = aSource->dateFormat;
    aDestination->dateFormatRef   = aSource->dateFormatRef;

    /* PROJ-2208 Multi Currency */
    aDestination->nlsCurrency     = aSource->nlsCurrency;
    aDestination->nlsCurrencyRef  = aSource->nlsCurrencyRef;

    // BUG-37247
    aDestination->groupConcatPrecisionRef = aSource->groupConcatPrecisionRef;

    // BUG-41944
    aDestination->arithmeticOpMode    = aSource->arithmeticOpMode;
    aDestination->arithmeticOpModeRef = aSource->arithmeticOpModeRef;

    idlOS::memcpy(aDestination->currentRow,
                  aSource->currentRow,
                  MTC_TUPLE_TYPE_MAXIMUM * ID_SIZEOF(UShort) );

    // stack buffer is set on execution phase. PR2475
    aDestination->stackCount  = 0;
    aDestination->stackBuffer = NULL;
    aDestination->stackRemain = 0;
    aDestination->stack       = NULL;

    if ( aDestination->dataSize > 0 )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF (UChar) *
                                 aDestination->dataSize,
                                 (void**)&(aDestination->data))
                 != IDE_SUCCESS);
    }
    else
    {
        aDestination->data = NULL ;
    }

    // To Fix PR-9600
    // mtcTemplate::execInfo, mtcTemplate::execInfoCnt ����� �߰���
    // �ش� ������ ���� ���縦 ���־�� ��.
    if ( aDestination->execInfoCnt > 0 )
    {
        IDE_TEST(aMemory->alloc( aDestination->execInfoCnt *
                                 ID_SIZEOF(UInt),
                                 (void**)&(aDestination->execInfo))
                 != IDE_SUCCESS);
    }
    else
    {
        aDestination->execInfo = NULL;
    }

    // PROJ-2527 WITHIN GROUP AGGR
    aDestination->funcDataCnt = aSource->funcDataCnt;
        
    if ( aDestination->funcDataCnt > 0 )
    {
        IDU_FIT_POINT( "mtc::cloneTemplate::alloc::funcData",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aMemory->alloc( aDestination->funcDataCnt *
                                  ID_SIZEOF(mtfFuncDataBasicInfo*),
                                  (void**)&(aDestination->funcData) )
                  != IDE_SUCCESS);
    }
    else
    {
        aDestination->funcData = NULL;
    }
    
    for ( sTupleIndex = 0                                   ;
          sTupleIndex < aDestination->rowCountBeforeBinding ;
          sTupleIndex ++ )
    {
        IDE_TEST( mtc::cloneTuple( aMemory,
                                   aSource,
                                   sTupleIndex,
                                   aDestination,
                                   sTupleIndex )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::cloneTemplate( iduVarMemList * aMemory,
                           mtcTemplate   * aSource,
                           mtcTemplate   * aDestination )
{
    UShort sTupleIndex;

    aDestination->stackCount      = aSource->stackCount;
    aDestination->dataSize        = aSource->dataSize;

    // To Fix PR-9600
    // To Fix PR-9600
    // mtcTemplate::execInfo, mtcTemplate::execInfoCnt ����� �߰���
    // �ش� ������ ���� ���縦 ���־�� ��.
    aDestination->execInfoCnt     = aSource->execInfoCnt;

    aDestination->initSubquery    = aSource->initSubquery;
    aDestination->fetchSubquery   = aSource->fetchSubquery;
    aDestination->finiSubquery    = aSource->finiSubquery;
    aDestination->setCalcSubquery = aSource->setCalcSubquery;
    aDestination->getOpenedCursor = aSource->getOpenedCursor;
    aDestination->addOpenedLobCursor = aSource->addOpenedLobCursor;
    aDestination->isBaseTable     = aSource->isBaseTable;
    aDestination->closeLobLocator = aSource->closeLobLocator;
    aDestination->getSTObjBufSize = aSource->getSTObjBufSize;
    aDestination->rowCount        = aSource->rowCount;
    aDestination->rowCountBeforeBinding  = aSource->rowCountBeforeBinding;
    aDestination->variableRow     = aSource->variableRow;

    // PROJ-2002 Column Security
    aDestination->encrypt          = aSource->encrypt;
    aDestination->decrypt          = aSource->decrypt;
    aDestination->encodeECC        = aSource->encodeECC;
    aDestination->getDecryptInfo   = aSource->getDecryptInfo;
    aDestination->getECCInfo       = aSource->getECCInfo;
    aDestination->getECCSize       = aSource->getECCSize;

    // BUG-11192����
    // template�� dateFormat�� �߰��Ǹ鼭 cloneTemplate����
    // ���縦 ���־�� �Ѵ�.
    // cloneTemplate�� ���� ���ǳ������� ���ǹǷ�
    // �����͸� assign�ϸ� �ȴ�. (malloc�ؼ� ī���� �ʿ�� ����)
    aDestination->dateFormat      = aSource->dateFormat;
    aDestination->dateFormatRef   = aSource->dateFormatRef;

    /* PROJ-2208 Multi Currency */
    aDestination->nlsCurrency     = aSource->nlsCurrency;
    aDestination->nlsCurrencyRef  = aSource->nlsCurrencyRef;

    // BUG-37247
    aDestination->groupConcatPrecisionRef = aSource->groupConcatPrecisionRef;

    // BUG-41944
    aDestination->arithmeticOpMode    = aSource->arithmeticOpMode;
    aDestination->arithmeticOpModeRef = aSource->arithmeticOpModeRef;
    
    idlOS::memcpy(aDestination->currentRow,
                  aSource->currentRow,
                  MTC_TUPLE_TYPE_MAXIMUM * ID_SIZEOF(UShort) );

    // stack buffer is set on execution phase. PR2475
    aDestination->stackCount  = 0;
    aDestination->stackBuffer = NULL;
    aDestination->stackRemain = 0;
    aDestination->stack       = NULL;

    if ( aDestination->dataSize > 0 )
    {
        IDE_TEST(aMemory->alloc( ID_SIZEOF (UChar) *
                                 aDestination->dataSize,
                                 (void**)&(aDestination->data))
                 != IDE_SUCCESS);
    }
    else
    {
        aDestination->data = NULL ;
    }

    // To Fix PR-9600
    // mtcTemplate::execInfo, mtcTemplate::execInfoCnt ����� �߰���
    // �ش� ������ ���� ���縦 ���־�� ��.
    if ( aDestination->execInfoCnt > 0 )
    {
        IDE_TEST(aMemory->alloc( aDestination->execInfoCnt *
                                 ID_SIZEOF(UInt),
                                 (void**)&(aDestination->execInfo))
                 != IDE_SUCCESS);
    }
    else
    {
        aDestination->execInfo = NULL;
    }

    // PROJ-2527 WITHIN GROUP AGGR
    aDestination->funcDataCnt = aSource->funcDataCnt;
        
    if ( aDestination->funcDataCnt > 0 )
    {
        IDU_FIT_POINT( "mtc::cloneTemplate::alloc::funcData",
                       idERR_ABORT_InsufficientMemory );
        IDE_TEST( aMemory->alloc( aDestination->funcDataCnt *
                                  ID_SIZEOF(mtfFuncDataBasicInfo*),
                                  (void**)&(aDestination->funcData) )
                  != IDE_SUCCESS);
    }
    else
    {
        aDestination->funcData = NULL;
    }
    
    for ( sTupleIndex = 0                                   ;
          sTupleIndex < aDestination->rowCountBeforeBinding ;
          sTupleIndex ++ )
    {
        IDE_TEST( mtc::cloneTuple( aMemory,
                                   aSource,
                                   sTupleIndex,
                                   aDestination,
                                   sTupleIndex )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 *
 * PROJ-2527 WITHIN GROUP AGGR
 *
 * cloneTemplate�ÿ� ������ ��ü�鿡 ���� finalize�Ѵ�.
 *
 ******************************************************************/
void mtc::finiTemplate( mtcTemplate  * aTemplate )
{
    /* BUG-46892 */
    idvSQL * sStatistics = NULL;
    ULong    sTotalSize  = (ULong)0;
    UInt     i;

    sStatistics = mtc::getStatistics( aTemplate );

    if ( aTemplate->funcData != NULL )
    {
        for ( i = 0; i < aTemplate->funcDataCnt; i++ )
        {
            if ( aTemplate->funcData[i] != NULL )
            {
                sTotalSize += aTemplate->funcData[i]->memoryMgr->getSize(); /* BUG-46892 */

                mtf::freeFuncDataMemory( aTemplate->funcData[i]->memoryMgr );

                aTemplate->funcData[i] = NULL;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-46892 */
    if ( sStatistics != NULL )
    {
        IDV_SQL_SET( sStatistics, mMathTempMem, sTotalSize );
    }
    else
    {
        /* Nothing to do */
    }
}

/*-----------------------------------------------------------------------------
 * Name :
 *    IDE_RC mtc::addFloat()
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


IDE_RC mtc::addFloat( mtdNumericType *aValue,
                      UInt            aPrecisionMaximum,
                      mtdNumericType *aArgument1,
                      mtdNumericType *aArgument2 )
{
    SInt sCarry;
    SInt sExponent1;
    SInt sExponent2;
    SInt mantissaIndex;
    SInt tempLength;
    SInt longLength;
    SInt shortLength;

    SInt lIndex;
    SInt sIndex;
    SInt checkIndex;

    SInt sLongExponent;
    SInt sShortExponent;

    mtdNumericType *longArg;
    mtdNumericType *shortArg;

//    idlOS::memset( aValue->mantissa , 0 , ID_SIZEOF(aValue->mantissa) );

    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
    }
    else if( aArgument1->length == 1 )
    {
        aValue->length = aArgument2->length;
        aValue->signExponent = aArgument2->signExponent;
        idlOS::memcpy( aValue->mantissa , aArgument2->mantissa , aArgument2->length - 1);
    }
    else if( aArgument2->length == 1 )
    {
        aValue->length = aArgument1->length;
        aValue->signExponent = aArgument1->signExponent;
        idlOS::memcpy( aValue->mantissa , aArgument1->mantissa , aArgument1->length - 1);
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
        if( longLength - longArg->length > (SInt)(aPrecisionMaximum / 2 + 2) )
        {
            aValue->length = shortArg->length;
            aValue->signExponent = shortArg->signExponent;
            idlOS::memcpy( aValue->mantissa ,
                           shortArg->mantissa ,
                           shortArg->length);
            return IDE_SUCCESS;
        }

        if( longLength > (SInt) (aPrecisionMaximum / 2 + 2) )
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

        IDE_TEST_RAISE( sExponent1 > 63 , ERR_OVERFLOW);

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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    IDE_RC mtc::subtractFloat()
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

IDE_RC mtc::subtractFloat( mtdNumericType *aValue,
                           UInt            aPrecisionMaximum,
                           mtdNumericType *aArgument1,
                           mtdNumericType *aArgument2 )
{
    SInt sBorrow;
    SInt sExponent1;
    SInt sExponent2;

    SInt mantissaIndex;
    SInt maxLength;
    SInt sArg1Length;
    SInt sArg2Length;
    SInt sDiffExp;

    SInt sArg1Index;
    SInt sArg2Index;
    SInt lIndex;
    SInt sIndex;

    SInt isZero;

    SInt sArgExponent1;
    SInt sArgExponent2;

    mtdNumericType* longArg;
    mtdNumericType* shortArg;
    SInt longLength;
    SInt shortLength;
    SInt tempLength;

    SInt i;

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
        idlOS::memcpy( aValue->mantissa ,
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
        if( longLength - longArg->length > (SInt)(aPrecisionMaximum / 2 + 2) )
        {
            if( shortArg == aArgument1 )
            {
                aValue->length = shortArg->length;
                aValue->signExponent = shortArg->signExponent;
                idlOS::memcpy( aValue->mantissa ,
                               shortArg->mantissa ,
                               shortArg->length);
                return IDE_SUCCESS;
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

        if( longLength > (SInt) (aPrecisionMaximum / 2 + 2) )
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

        IDE_TEST_RAISE( sExponent1 > 63 , ERR_OVERFLOW);

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
                    SShort checkIndex = 0;
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
                    return IDE_SUCCESS;
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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    IDE_SET(ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    IDE_RC mtc::multiplyFloat()
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
IDE_RC mtc::multiplyFloat( mtdNumericType *aValue,
                           UInt            /* aPrecisionMaximum */,
                           mtdNumericType *aArgument1,
                           mtdNumericType *aArgument2 )
{
    SInt sResultMantissa[40];   // ����� �ִ� 80�ڸ����� ���� �� �ִ�.
    SInt i;                     // for multiplication
    SInt j;                     // for multiplication
    SInt sCarry;
    SInt sResultFence;
    SShort sArgExponent1;
    SShort sArgExponent2;
    SShort sResultExponent;
    SShort sMantissaStart;

    // �ϳ��� null�� ��� nulló��
    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
        aValue->signExponent = 0;
        return IDE_SUCCESS;
    }
    // �ϳ��� 0�� ��� 0����
    else if( aArgument1->length == 1 || aArgument2->length == 1 )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
        return IDE_SUCCESS;
    }

    // result mantissa�ִ� ���� ����
    sResultFence = aArgument1->length + aArgument2->length - 2;
    // result mantissa�� 0���� �ʱ�ȭ
    idlOS::memset( sResultMantissa, 0, ID_SIZEOF(UInt) * sResultFence );


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
                    (SInt)aArgument1->mantissa[i] *
                    (SInt)aArgument2->mantissa[j];
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
                    (SInt)aArgument1->mantissa[i] *
                    (SInt)( 99 - aArgument2->mantissa[j] );
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
                    (SInt)( 99 - aArgument1->mantissa[i] ) *
                    (SInt)aArgument2->mantissa[j];
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
                    (SInt)( 99 - aArgument1->mantissa[i] ) *
                    (SInt)( 99 - aArgument2->mantissa[j] );
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
    IDE_TEST_RAISE( sResultExponent > 63, ERR_VALUE_OVERFLOW );

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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*-----------------------------------------------------------------------------
 * Name :
 *    IDE_RC mtc::divideFloat()
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
IDE_RC mtc::divideFloat( mtdNumericType *aValue,
                         UInt            /* aPrecisionMaximum*/,
                         mtdNumericType *aArgument1,
                         mtdNumericType *aArgument2 )

{
    SInt  sMantissa1[42];        // ������ �� �� ����.
    UChar sMantissa2[20] = {0,}; // ���� ����.(�������)
    SInt  sMantissa3[21];        // ���� * �κи�. �������� ū ��� ���.(�ִ� 1byte)

    SInt sNumerator;           // ������ ���ð�.
    SInt sDenominator;         // ���� ���ð�.
    SInt sPartialQ;            // �κи�.

    SInt i;                    // for Iterator
    SInt j;                    // for Iterator
    SInt k;                    // for Iterator

    SInt sResultFence;

    SInt sStep;
    SInt sLastStep;

    SShort sArgExponent1;
    SShort sArgExponent2;
    SShort sResultExponent;

    UChar * sMantissa2Ptr;
    SInt    sCarry;
    SInt    sMantissaStart;

    // �ϳ��� null�̸� null
    if( aArgument1->length == 0 || aArgument2->length == 0 )
    {
        aValue->length = 0;
        return IDE_SUCCESS;
    }
    // 0���� ���� ���
    else if ( aArgument2->length == 1 )
    {
        IDE_RAISE(ERR_DIVIDE_BY_ZERO);
    }
    // 0�� ���� ���
    else if ( aArgument1->length == 1 )
    {
        aValue->length = 1;
        aValue->signExponent = 0x80;
        return IDE_SUCCESS;
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
            sMantissa1[i+1] = (SInt)aArgument1->mantissa[i];
        }
        idlOS::memset( &sMantissa1[i+1], 0, sizeof(SInt) * (42 - i - 1) );
    }
    else
    {
        // �������� ��ĭ���� ����(0��° ĭ���� ���� ����ɰ���)
        for( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa1[i+1] = 99 - (SInt)aArgument1->mantissa[i];
        }
        idlOS::memset( &sMantissa1[i+1], 0, sizeof(SInt) * (42 - i - 1) );
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
            sMantissa2[i] = 99 - (SInt)aArgument2->mantissa[i];
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
        sDenominator = (SInt)sMantissa2Ptr[0] * 10000 +
            (SInt)sMantissa2Ptr[1] * 100 +
            (SInt)sMantissa2Ptr[2];
    }
    else if ( aArgument2->length > 2 )
    {
        sDenominator = (SInt)sMantissa2Ptr[0] * 10000 +
            (SInt)sMantissa2Ptr[1] * 100;
    }
    else
    {
        sDenominator = (SInt)sMantissa2Ptr[0] * 10000;
    }
    IDE_TEST_RAISE(sDenominator == 0, ERR_DIVIDE_BY_ZERO);

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
                sMantissa3[k+1] = (SInt)sMantissa2Ptr[k] * sPartialQ;
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
    IDE_TEST_RAISE( sResultExponent > 63, ERR_VALUE_OVERFLOW );

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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DIVIDE_BY_ZERO );
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_DIVIDE_BY_ZERO ));
    }
    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * gets the modula of aArgument1 divided by aArgument2, stores the result in aValue
 */
IDE_RC mtc::modFloat( mtdNumericType *aValue,
                      UInt            /* aPrecisionMaximum*/,
                      mtdNumericType *aArgument1,
                      mtdNumericType *aArgument2 )
{
    SShort  sExponent1;
    SShort  sExponent2;
    UChar   sMantissa1[MTD_FLOAT_MANTISSA_MAXIMUM + 1] = { 0, };
    UChar   sMantissa2[MTD_FLOAT_MANTISSA_MAXIMUM + 1] = { 0, };
    UChar   sMantissa3[MTD_FLOAT_MANTISSA_MAXIMUM + 1] = { 0, };
    UChar   sMantissa4[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    UShort  sMininum;
    UShort  sBorrow;
    SInt    i;                    // for Iterator
    SInt    j;                    // for Iterator
    SShort  sArgExponent1;
    SShort  sArgExponent2;
    SInt    sMantissaStart;

    /* �ϳ��� null�̸� null */
    if ( ( aArgument1->length == 0 ) || ( aArgument2->length == 0 ) )
    {
        aValue->length = 0;
        IDE_CONT( NORMAL_EXIT );
    }
    /* 0���� ���� ��� */
    else if ( aArgument2->length == 1 )
    {
        IDE_RAISE(ERR_DIVIDE_BY_ZERO);
    }
    /* 0�� ���� ��� */
    else if ( aArgument1->length == 1 )
    {
        aValue->length = 1;
        aValue->signExponent = 0x80;
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* ������� �������� �κи� ���� */
    sArgExponent1 = aArgument1->signExponent & 0x80;
    sArgExponent2 = aArgument2->signExponent & 0x80;

    /* exponent ���ϱ� */
    if ( sArgExponent1 == 0x80 )
    {
        sExponent1 = (aArgument1->signExponent & 0x7F) - 64;
    }
    else
    {
        sExponent1 = 64 - (aArgument1->signExponent & 0x7F);
    }

    if ( sArgExponent2 == 0x80 )
    {
        sExponent2 = (aArgument2->signExponent & 0x7F) - 64;
    }
    else
    {
        sExponent2 = 64 - (aArgument2->signExponent & 0x7F);
    }

    /* ������ ����. ������ ��� ���·� �ٲپ� mantissa���� */
    sMantissa1[0] = sMantissa2[0] = 0;

    if ( sArgExponent1 == 0x80 )
    {
        for ( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa1[i + 1] = aArgument1->mantissa[i];
        }
    }
    else
    {
        for ( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa1[i + 1] = 99 - aArgument1->mantissa[i];
        }
    }

    /* ���� ����. */
    if ( sArgExponent2 == 0x80 )
    {
        for ( i = 0; i < aArgument2->length - 1; i++ )
        {
            sMantissa2[i + 1] = aArgument2->mantissa[i];
        }
    }
    else
    {
        for ( i = 0; i < aArgument2->length - 1; i++ )
        {
            sMantissa2[i + 1] = 99 - aArgument2->mantissa[i];
        }
    }

    while ( sExponent1 >= sExponent2 )
    {
        while ( ( sMantissa1[0] != 0 ) || ( sMantissa1[1] >= sMantissa2[1] ) )
        {
            sMininum = ( sMantissa1[0] * 100 + sMantissa1[1] ) / ( sMantissa2[1] + 1 );
            if ( sMininum )
            {
                sMantissa3[0] = 0;
                for ( j = 1; j < (SInt)ID_SIZEOF(sMantissa3); j++ )
                {
                    sMantissa3[j - 1] += ((UShort)sMantissa2[j]) * sMininum / 100;
                    sMantissa3[j]      = ((UShort)sMantissa2[j]) * sMininum % 100;
                }

                for ( j = ID_SIZEOF(sMantissa3) - 1; j > 0; j-- )
                {
                    if ( sMantissa3[j] >= 100 )
                    {
                        sMantissa3[j]     -= 100;
                        sMantissa3[j - 1] += 1;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }

                sBorrow = 0;
                for ( j = ID_SIZEOF(sMantissa3) - 1; j >= 0; j-- )
                {
                    sMantissa3[j] += sBorrow;
                    sBorrow = 0;
                    if ( sMantissa1[j] < sMantissa3[j] )
                    {
                        sMantissa1[j] += 100;
                        sBorrow = 1;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                    sMantissa1[j] -= sMantissa3[j];
                }
            }
            else
            {
                if ( idlOS::memcmp( sMantissa1, sMantissa2, ID_SIZEOF(sMantissa2) ) >= 0 )
                {
                    sBorrow = 0;
                    for ( j = ID_SIZEOF(sMantissa3) - 1; j >= 0; j-- )
                    {
                        sMantissa3[j] = sMantissa2[j] + sBorrow;
                        sBorrow = 0;
                        if ( sMantissa1[j] < sMantissa3[j] )
                        {
                            sMantissa1[j] += 100;
                            sBorrow = 1;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                        sMantissa1[j] -= sMantissa3[j];
                    }
                }
                else
                {
                    /* Nothing to do */
                }
                break;
            }
        }

        if ( sExponent1 <= sExponent2 )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        for ( j = 0; j < (SInt)ID_SIZEOF(sMantissa1) - 1; j++ )
        {
            sMantissa1[j] = sMantissa1[j + 1];
        }

        sMantissa1[j] = 0;
        sExponent1--;
    }

    sMantissaStart = ID_SIZEOF(sMantissa1);
    for ( i = 1; i < (SInt)ID_SIZEOF(sMantissa1); i++ )
    {
        if ( sMantissa1[i] != 0 )
        {
            sMantissaStart = i;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sExponent1 -= sMantissaStart - 1;

    /* exponent�� 63�� ������ ����(�ִ� 126�̹Ƿ�) */
    IDE_TEST_RAISE( sExponent1 > 63, ERR_VALUE_OVERFLOW );

    /* exponent�� -63(-126)���� ������ 0�� �� */
    if ( ( sExponent1 < -63 ) || ( sMantissaStart == ID_SIZEOF(sMantissa1) ) )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
    }
    else
    {
        for ( i = 0, j = sMantissaStart; j < (SInt)ID_SIZEOF(sMantissa1); i++, j++ )
        {
            sMantissa4[i] = sMantissa1[j];
        }
        /* ���� makenumeric ȣȯ�� */
        for ( i = 0, aValue->length = 1; i < (SInt)ID_SIZEOF(sMantissa4) - 1; i++ )
        {
            if ( sMantissa4[i] != 0 )
            {
                aValue->length = i + 2;
            }
            else
            {
                /* Nothing to do */
            }
        }

        /* �������� ��ȣ�� ������. */
        if ( sArgExponent1 == 0x80 )
        {
            aValue->signExponent = sExponent1 + 192;
            for ( i = 0; i < aValue->length - 1; i++ )
            {
                aValue->mantissa[i] = sMantissa4[i];
            }
        }
        else
        {
            aValue->signExponent = 64 - sExponent1;
            for ( i = 0; i < aValue->length - 1; i++ )
            {
                aValue->mantissa[i] = 99 - sMantissa4[i];
            }
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DIVIDE_BY_ZERO );
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_DIVIDE_BY_ZERO ));
    }
    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * gets sign of aArgument1
 */
IDE_RC mtc::signFloat( mtdNumericType *aValue,
                       mtdNumericType *aArgument1 )
{
    SShort sArgExponent1;

    /* null �̸� null */
    if ( aArgument1->length == 0)
    {
        aValue->length = 0;
    }
    /* zero �̸� zero */
    else if ( aArgument1->length == 1 )
    {
        aValue->length       = 1;
        aValue->signExponent = 0x80;
    }
    else
    {
        sArgExponent1 = aArgument1->signExponent & 0x80;

        /* ��� */
        if ( sArgExponent1 == 0x80 )
        {
            aValue->signExponent = 193;
            aValue->length = 2;
            aValue->mantissa[0] = 1;
        }
        /* ���� */
        else
        {
            aValue->signExponent = 63;
            aValue->length = 2;
            aValue->mantissa[0] = 98;
        }
    }

    return IDE_SUCCESS;
}

/*
 * PROJ-1517
 * gets absolute value of aArgument1 and store it on aValue
 */
IDE_RC mtc::absFloat( mtdNumericType *aValue,
                      mtdNumericType *aArgument1 )
{
    SShort sArgExponent1;
    SInt   i;

    /* null �̸� null */
    if ( aArgument1->length == 0)
    {
        aValue->length = 0;
    }
    else
    {
        sArgExponent1 = aArgument1->signExponent & 0x80;

        /* ��� */
        if ( sArgExponent1 == 0x80 )
        {
            aValue->length = aArgument1->length;
            aValue->signExponent = aArgument1->signExponent;
            for ( i = 0; i < aArgument1->length - 1; i++ )
            {
                aValue->mantissa[i] = aArgument1->mantissa[i];
            }
        }
        /* ���� */
        else
        {
            aValue->length = aArgument1->length;
            aValue->signExponent = (64 - (aArgument1->signExponent & 0x7F)) + 192;
            for ( i = 0; i < aArgument1->length - 1; i++ )
            {
                aValue->mantissa[i] = 99 - aArgument1->mantissa[i];
            }
        }
    }

    return IDE_SUCCESS;
}
/*
 * PROJ-1517
 * gets the smallest integral value not less than aArgument1 and store it on aValue
 */
IDE_RC mtc::ceilFloat( mtdNumericType *aValue,
                       mtdNumericType *aArgument1 )
{
    SShort sExponent;
    UShort sCarry;
    idBool sHasFraction;
    SInt   i;
    SShort sArgExponent1;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };

    /* null �̸� null */
    if ( aArgument1->length == 0 )
    {
        aValue->length = 0;
    }
    else if ( aArgument1->length == 1 )
    {
        aValue->length = 1;
        aValue->mantissa[0] = 0;
        aValue->signExponent = 0x80;
    }
    else
    {
        aValue->signExponent = aArgument1->signExponent;
        aValue->length = aArgument1->length;
        for ( i = 0; i < aArgument1->length - 1; i++ )
        {
            sMantissa[i] = aArgument1->mantissa[i];
            aValue->mantissa[i] = aArgument1->mantissa[i];
        }

        sArgExponent1 = aArgument1->signExponent & 0x80;

        /* ��� */
        if ( sArgExponent1 == 0x80 )
        {
            sExponent = (aArgument1->signExponent & 0x7F) - 64;

            if ( sExponent < 0)
            {
                sExponent = 1;
                aValue->mantissa[0] = 1;
                aValue->length = 2;
                aValue->signExponent = sExponent + 192;
            }
            else
            {
                if ( sExponent < MTD_FLOAT_MANTISSA_MAXIMUM )
                {
                    sHasFraction = ID_FALSE;

                    for ( i = sExponent; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
                    {
                        if ( sMantissa[i] > 0 )
                        {
                            sHasFraction = ID_TRUE;
                            break;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }

                    if ( sHasFraction == ID_TRUE )
                    {
                        sCarry = 1;
                        for ( i = sExponent - 1; i >= 0; i-- )
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
                            for ( i = sExponent; i > 0; i-- )
                            {
                                sMantissa[i] = sMantissa[i - 1];
                            }
                            sMantissa[0] = 1;
                            sExponent++;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                    else
                    {
                        /* No Fraction */
                    }

                    if ( sExponent < aValue->length )
                    {
                        for ( i = sExponent; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
                        {
                            sMantissa[i] = 0;
                        }

                        aValue->length = sExponent + 1;
                        aValue->signExponent = sExponent + 192;
                        for ( i = 0; i < aValue->length - 1; i++ )
                        {
                            aValue->mantissa[i] = sMantissa[i];
                        }
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
        /* ���� */
        else
        {
            sExponent = 64 - (aArgument1->signExponent & 0x7F);

            if ( sExponent <= 0 )
            {
                aValue->length       = 1;
                aValue->signExponent = 0x80;
            }
            else
            {
                if ( sExponent < aValue->length )
                {
                    for ( i = sExponent; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
                    {
                        sMantissa[i] = 99;
                    }

                    aValue->length = sExponent + 1;
                    aValue->signExponent = 64 - sExponent;
                    for ( i = 0; i < aValue->length - 1; i++ )
                    {
                        aValue->mantissa[i] = sMantissa[i];
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }

    return IDE_SUCCESS;
}

/*
 * PROJ-1517
 * gets the largest integral value not greater than aArgument1 and store it on aValue
 */
IDE_RC mtc::floorFloat( mtdNumericType *aValue,
                        mtdNumericType *aArgument1 )
{
    SShort sExponent;
    UShort sCarry;
    SInt   i;
    SShort sArgExponent1;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };

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
        aValue->signExponent = aArgument1->signExponent;
        aValue->length = aArgument1->length;
        sArgExponent1 = aArgument1->signExponent & 0x80;
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

        /* ��� */
        if ( sArgExponent1 == 0x80 )
        {
            sExponent = (aArgument1->signExponent & 0x7F) - 64;

            if ( sExponent <= 0 )
            {
                aValue->length       = 1;
                aValue->signExponent = 0x80;
            }
            else
            {
                /* BUG-42098 in floor function, zero trim of numeric type is needed */
                if ( sExponent < aValue->length )
                {
                    for ( i = sExponent; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
                    {
                        sMantissa[i] = 0;
                    }
                    aValue->length = sExponent + 1;
                    aValue->signExponent = sExponent + 192;
                    for ( i = 0; i < aValue->length - 1; ++i )
                    {
                        aValue->mantissa[i] = sMantissa[i];
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
        /* ���� */
        else
        {
            sExponent = 64 - (aArgument1->signExponent & 0x7F);

            if ( sExponent < 0)
            {
                sExponent = 1;
                aValue->mantissa[0] = 98;
                aValue->length = 2;
                aValue->signExponent = 64 - sExponent;
            }
            else
            {
                if ( sExponent < MTD_FLOAT_MANTISSA_MAXIMUM )
                {
                    if ( sMantissa[sExponent] > 0 )
                    {
                        sCarry = 1;
                        for ( i = sExponent - 1; i >= 0; i--)
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
                                /* nothing to do. */
                            }
                        }

                        if ( sCarry != 0 )
                        {
                            for ( i = sExponent; i > 0; i-- )
                            {
                                sMantissa[i] = sMantissa[i - 1];
                            }
                            sMantissa[0] = 1;
                            sExponent++;
                        }
                        else
                        {
                            /* nothing to do. */
                        }
                    }
                    else
                    {
                        /* nothing to do. */
                    }

                    /* BUG-42098 in floor function, zero trim of numeric type is needed */
                    if ( sExponent < aValue->length )
                    {
                        for ( i = sExponent; i < MTD_FLOAT_MANTISSA_MAXIMUM; i++ )
                        {
                            sMantissa[i] = 0;
                        }

                        aValue->length = sExponent + 1;
                        aValue->signExponent = 64 - sExponent;
                        for ( i = 0; i < aValue->length - 1; i++ )
                        {
                            aValue->mantissa[i] = 99 - sMantissa[i];
                        }
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
    }/* else */

    return IDE_SUCCESS;
}

/*
 * PROJ-1517
 * converts mtdNumericType to SLong
 */
IDE_RC mtc::numeric2Slong( SLong          *aValue,
                           mtdNumericType *aArgument1 )
{
    ULong  sULongValue = 0;
    SShort sArgExponent1;
    SShort sExponent;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    SInt   i;

    static const ULong sPower[] = { ID_ULONG(1),
                                    ID_ULONG(100),
                                    ID_ULONG(10000),
                                    ID_ULONG(1000000),
                                    ID_ULONG(100000000),
                                    ID_ULONG(10000000000),
                                    ID_ULONG(1000000000000),
                                    ID_ULONG(100000000000000),
                                    ID_ULONG(10000000000000000),
                                    ID_ULONG(1000000000000000000) };
    /* null �̸� ���� */
    IDE_TEST_RAISE( aArgument1->length == 0, ERR_NULL_VALUE );

    if ( aArgument1->length == 1 )
    {
        *aValue = 0;
        IDE_CONT( NORMAL_EXIT );
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
        IDE_TEST_RAISE( (UShort)sExponent
                        > ( ID_SIZEOF(sPower) / ID_SIZEOF(sPower[0]) ),
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
        IDE_TEST_RAISE( sULongValue > ID_ULONG(9223372036854775807),
                        ERR_OVERFLOW );
        *aValue = sULongValue;
    }
    //����
    else
    {
        IDE_TEST_RAISE( sULongValue > ID_ULONG(9223372036854775808),
                        ERR_OVERFLOW );

        if ( sULongValue == ID_ULONG(9223372036854775808) )
        {
            *aValue = ID_LONG(-9223372036854775807) - ID_LONG(1);
        }
        else
        {
            *aValue = -(SLong)sULongValue;
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ) );
    }

    IDE_EXCEPTION( ERR_NULL_VALUE );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_NULL_VALUE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * converts mtdNumericType to SDouble (mtdDoubleType)
 */
void mtc::numeric2Double( mtdDoubleType  * aValue,
                          mtdNumericType * aArgument1 )
{
    UChar     sMantissaLen;
    SShort    sScale;
    SShort    sExponent;
    UChar     i;
    SShort    sArgExponent1;
    UChar   * sMantissa;
    SDouble   sDoubleValue = 0.0;

    sMantissaLen = aArgument1->length - 1;

    sArgExponent1        = ( aArgument1->signExponent & 0x80 );
    sScale               = ( aArgument1->signExponent & 0x7F );

    if ( (sScale != 0) && (sMantissaLen > 0) )
    {
        if ( sArgExponent1 == 0x80 )
        {
            for ( i = 0, sMantissa = aArgument1->mantissa;
                  i < sMantissaLen;
                  i++, sMantissa++ )
            {
                sDoubleValue = sDoubleValue * 100.0 + *sMantissa;
            }

            sExponent = sScale - 64 - sMantissaLen;
        }
        else
        {
            for ( i = 0, sMantissa = aArgument1->mantissa;
                  i < sMantissaLen;
                  i++, sMantissa++ )
            {
                sDoubleValue = sDoubleValue * 100.0 + (99 - *sMantissa);
            }
            sExponent = 64 - sScale - sMantissaLen;
        }

        if (sExponent >= 0)
        {
            sDoubleValue = sDoubleValue * gDoublePow[sExponent];
        }
        else
        {
            if (sExponent < -63)
            {
                sDoubleValue = 0.0;
            }
            else
            {
                sDoubleValue = sDoubleValue / gDoublePow[-sExponent];
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    if (sArgExponent1 == 0x80)
    {
        *aValue = sDoubleValue;
    }
    else
    {
        *aValue = -sDoubleValue;
    }
}

// BUG-41194 double to numeric ��ȯ ���ɰ���
IDE_RC mtc::double2Numeric( mtdNumericType *aValue,
                            mtdDoubleType   aArgument1 )
{
    SLong    sLongValue;
    SShort   sExponent2;  // 2^e
    SChar    sSign;
    UChar  * sDoubles   = (UChar*) &aArgument1;
    UChar  * sExponents = (UChar*) &sExponent2;
    UChar  * sFractions = (UChar*) &sLongValue;
    UChar    sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM];
    SShort   sExponent10;   // 10^e
    SShort   sExponent100;  // 100^e
    SShort   sExponent;
    SShort   sLength;
    SShort   i;

    IDE_TEST_RAISE( gIEEE754Double == ID_FALSE,
                    ERR_NOT_APPLICABLE );
    
    //------------------------------------------
    // decompose double
    //------------------------------------------
    
    // IEEE 754-1985 ǥ�ؿ� ���� double type�� �и��Ѵ�.
    // 64 bit = 1 bit sign + 11 bit exponent + 52 bit fraction
    
#if defined(ENDIAN_IS_BIG_ENDIAN)
    
    // sign
    sSign = sDoubles[0] & 0x80;
    sSign = (sSign >> 7) & 0x01;  // 0 Ȥ�� 1

    // exponent
    sExponents[0] = sDoubles[0] & 0x7f;
    sExponents[1] = sDoubles[1] & 0xf0;
    sExponent2 = (sExponent2 >> 4) & 0x0fff;  // 0 ~ 2047

    // fraction
    sFractions[0] = 0;
    sFractions[1] = (sDoubles[1] & 0x0f) | 0x10;  // �տ� 1�߰�
    sFractions[2] = sDoubles[2];
    sFractions[3] = sDoubles[3];
    sFractions[4] = sDoubles[4];
    sFractions[5] = sDoubles[5];
    sFractions[6] = sDoubles[6];
    sFractions[7] = sDoubles[7];

#else

    // sign
    sSign = sDoubles[7] & 0x80;
    sSign = (sSign >> 7) & 0x01;  // 0 Ȥ�� 1
    
    // exponent
    sExponents[0] = sDoubles[6] & 0xf0;
    sExponents[1] = sDoubles[7] & 0x7f;
    sExponent2 = (sExponent2 >> 4) & 0x0fff;  // 0 ~ 2047

    // fraction
    sFractions[0] = sDoubles[0];
    sFractions[1] = sDoubles[1];
    sFractions[2] = sDoubles[2];
    sFractions[3] = sDoubles[3];
    sFractions[4] = sDoubles[4];
    sFractions[5] = sDoubles[5];
    sFractions[6] = (sDoubles[6] & 0x0f) | 0x10;  // �տ� 1�߰�
    sFractions[7] = 0;

#endif

    //------------------------------------------
    // check zero, infinity, NaN (Not a Number)
    //------------------------------------------

    if ( sExponent2 == 0x00 )
    {
        // zero
        aValue->length = 1;
        aValue->signExponent = 0x80;
        
        IDE_CONT( normal_exit );
    }
    else
    {
        // Nothing to do.
    }

    if ( sExponent2 == 0x7ff )
    {
        if ( sLongValue == 0x10000000000000 )
        {
            // -infinity, +infinity
            IDE_RAISE( ERR_VALUE_OVERFLOW );
        }
        else
        {
            // NaN
            IDE_RAISE( ERR_INVALID_LITERAL );
        }
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // binary to decimal
    //------------------------------------------

    // double_value
    // = sign * (1*2^52 + (f0)*2^51 + (f1)*2^50 + ... + (f51)*2^0) * 2^(e-1023-52)
    // = sign * (long_value) * 2^(e-1023-52)
    // = sign * (long_value * x) * 10^y
    sLongValue = (SLong)(sLongValue * gConvMantissa[sExponent2 - 1023 - 52]);
    sExponent10 = gConvExponent[sExponent2 - 1023 - 52];

    //------------------------------------------
    // make numeric
    //------------------------------------------

    // Ȧ�� ���� ����
    if ( sExponent10 % 2 != 0 )
    {
        sLongValue *= 10;
        sExponent10--;
    }
    else
    {
        // Nothing to do.
    }
        
    for ( i = 9; i >= 0; i-- )
    {
        sMantissa[i] = (UChar)(sLongValue % 100);
        sLongValue /= 100;
    }

    for ( sExponent100 = 0; sExponent100 < 10; sExponent100++ )
    {
        if ( sMantissa[sExponent100] != 0 )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
        
    for ( i = 0; i + sExponent100 < 10; i++ )
    {
        sMantissa[i] = sMantissa[i + sExponent100];
    }
    for ( ; i < 10; i++)
    {
        sMantissa[i] = 0;
    }
        
    sLength = 1;
    for ( i = 0; i < 10; i++ )
    {
        if ( sMantissa[i] != 0 )
        {
            sLength = i + 2;
        }
        else
        {
            // Nothing to do.
        }
    }

    sExponent = 10 - sExponent100 + (sExponent10 / 2);
    
    IDE_TEST_RAISE( sExponent > 63, ERR_VALUE_OVERFLOW );

    if ( sExponent < -63 )
    {
        // zero
        aValue->length = 1;
        aValue->signExponent = 0x80;
    }
    else
    {
        if ( sSign == 0 )
        {
            aValue->length = sLength;
            aValue->signExponent = 128 + 64 + sExponent;
        
            for ( i = 0; i < sLength - 1; i++ )
            {
                aValue->mantissa[i] = sMantissa[i];
            }
        }
        else
        {
            aValue->length = sLength;
            aValue->signExponent = 64 - sExponent;

            // ���� ���� ��ȯ
            for ( i = 0; i < sLength - 1; i++ )
            {
                aValue->mantissa[i] = 99 - sMantissa[i];
            }
        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * rounds up aArgument1 and store it on aValue
 */
IDE_RC mtc::roundFloat( mtdNumericType *aValue,
                        mtdNumericType *aArgument1,
                        mtdNumericType *aArgument2 )
{
    SShort sExponent;
    SLong  sRound;
    UShort sCarry;
    SShort sArgExponent1;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    SInt   i;

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
        IDE_TEST( numeric2Slong( &sRound, aArgument2 ) != IDE_SUCCESS );

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
                IDE_TEST_RAISE( sExponent > 63, ERR_OVERFLOW );
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
            switch ( (SInt)( sRound % 2 ) )
#else
            switch ( sRound % 2 )
#endif
            {
                case 1:
                    if ( ( sExponent - ( sRound / 2 ) - 1 )
                        < (SShort)ID_SIZEOF(sMantissa) )
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
                              i < (SInt)ID_SIZEOF(sMantissa); i++ )
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
                    if ( ( sExponent - ( sRound / 2 ) ) < (SShort)ID_SIZEOF(sMantissa) )
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
                              i < (SInt)ID_SIZEOF(sMantissa); i++ )
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
                    if ( ( sExponent - ( sRound / 2 ) ) < (SShort)ID_SIZEOF(sMantissa) )
                    {
                        if ( sMantissa[sExponent - ( sRound / 2 )] >= 50 )
                        {
                            sMantissa[sExponent - ( sRound / 2 ) - 1]++;
                        }
                        else
                        {
                            /* Nothing to do */
                        }

                        for ( i = sExponent - ( sRound / 2 ); i < (SInt)ID_SIZEOF(sMantissa); i++ )
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
            for ( i = ID_SIZEOF(sMantissa) - 1; i >= 0; i-- )
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
                for ( i = ID_SIZEOF(sMantissa) - 1; i > 0; i-- )
                {
                    sMantissa[i] = sMantissa[i - 1];
                }
                sMantissa[0] = 1;
                sExponent++;
                IDE_TEST_RAISE( sExponent > 63, ERR_OVERFLOW );
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
            for ( i = 0, aValue->length = 1; i < (SInt)ID_SIZEOF(sMantissa) - 1; i++ )
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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OVERFLOW );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1517
 * gets the integral part of aArgument1 and store it on aValue
 */
IDE_RC mtc::truncFloat( mtdNumericType *aValue,
                        mtdNumericType *aArgument1,
                        mtdNumericType *aArgument2 )
{
    SShort sExponent;
    SLong  sTrunc;
    SShort sArgExponent1;
    UChar  sMantissa[MTD_FLOAT_MANTISSA_MAXIMUM] = { 0, };
    SInt   i;

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
        IDE_TEST( numeric2Slong( &sTrunc, aArgument2 ) != IDE_SUCCESS );

        sTrunc = -sTrunc;

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

        if ( ( sExponent * 2 ) <= sTrunc )
        {
            /* zero */
            aValue->length = 1;
            aValue->mantissa[0]= 0;
            aValue->signExponent = 0x80;
        }
        else
        {
#if defined(HP_HPUX) || defined(IA64_HP_HPUX)
            switch ( (SInt)(sTrunc % 2) )
#else
            switch ( sTrunc % 2 )
#endif
            {
                case 1:
                    if ( ( sExponent - ( sTrunc / 2 ) - 1 ) < (SShort)ID_SIZEOF(sMantissa) )
                    {

                        sMantissa[sExponent - ( sTrunc / 2 ) - 1]
                                  -= sMantissa[sExponent - ( sTrunc / 2 ) - 1] % 10;

                        for ( i = sExponent - ( sTrunc / 2 ); i < (SInt)ID_SIZEOF(sMantissa); i++ )
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
                    if ( ( sExponent - ( sTrunc / 2 ) ) < (SShort)ID_SIZEOF(sMantissa) )
                    {
                        sMantissa[sExponent - ( sTrunc / 2 )]
                                  -= sMantissa[sExponent - ( sTrunc / 2 )] % 10;

                        for ( i = sExponent - ( sTrunc / 2 ) + 1; i < (SInt)ID_SIZEOF(sMantissa); i++ )
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
                    if ( ( sExponent - ( sTrunc / 2 ) ) < (SShort)ID_SIZEOF(sMantissa) )
                    {
                        for ( i = sExponent - ( sTrunc / 2 ); i < (SInt)ID_SIZEOF(sMantissa); i++ )
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

            //length ����
            for ( i = 0, aValue->length = 1; i < (SInt)ID_SIZEOF(sMantissa) - 1; i++ )
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

            //����
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
                /* zero */
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::getPrecisionScaleFloat( const mtdNumericType * aValue,
                                    SInt                 * aPrecision,
                                    SInt                 * aScale )
{
// To fix BUG-12944
// precision�� scale�� ��Ȯ�� �����ִ� �Լ�.
// ���� maximum precision�� �Ѵ� ���� �ִ� 39, 40�̹Ƿ�,
// precision, scale�� ������ 38�� �Ѱ��ش�.(�ִ� mantissa�迭 ũ�⸦ ���� �����Ƿ� ������)
// ������ ������. return IDE_SUCCESS;
    SInt sExponent;

    if ( aValue->length > 1 )
    {
        sExponent  = (aValue->signExponent&0x7F) - 64;
        if ( (aValue->signExponent & 0x80) == 0x00 )
        {
            sExponent *= -1;
        }
        else
        {
            /* Nothing to do */
        }

        *aPrecision = (aValue->length-1)*2;
        *aScale = *aPrecision - (sExponent << 1);

        if ( (aValue->signExponent & 0x80) == 0x00 )
        {
            if ( (99 - *(aValue->mantissa)) / 10 == 0 )
            {
                (*aPrecision)--;
            }
            else
            {
                // Nothing to do.
            }

            if ( (99 - *(aValue->mantissa + aValue->length - 2)) % 10 == 0 )
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

            if ( *(aValue->mantissa + aValue->length - 2) % 10 == 0 )
            {
                (*aPrecision)--;
                (*aScale)--;
            }
            else
            {
                // Nothing to do.
            }
        }


        if ( (*aPrecision) > MTD_NUMERIC_PRECISION_MAXIMUM )
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

    return IDE_SUCCESS;
}

IDE_RC mtc::isSamePhysicalImage( const mtcColumn * aColumn,
                                 const void      * aRow1,
                                 UInt              aFlag1,
                                 const void      * aRow2,
                                 UInt              aFlag2,
                                 idBool          * aOutIsSame )
{
/***********************************************************************
 *
 * Description :
 *
 *    BUG-16531
 *    Column�� ������ Data�� ������ Image���� �˻�
 *
 * Implementation :
 *
 *    NULL�� ��� Garbage Data�� ������ �� �����Ƿ� ��� NULL���� �Ǵ�
 *    Data�� ���̰� �������� �˻�
 *    Data ���̰� ������ ��� memcmp �� �˻�
 *
 ***********************************************************************/

    UInt              sLength1;
    UInt              sLength2;

    const void *      sValue1;
    const void *      sValue2;

    const mtdModule * sTypeModule;

    //------------------------------------------
    // Parameter Validation
    //------------------------------------------

    IDE_ASSERT( aColumn != NULL );
    IDE_ASSERT( aRow1 != NULL );
    IDE_ASSERT( aRow2 != NULL );
    IDE_ASSERT( aOutIsSame != NULL );

    //------------------------------------------
    // Initialize
    //------------------------------------------

    *aOutIsSame = ID_FALSE;

    /* PROJ-2118 */
    /* Data Type ��� ȹ�� */
    IDE_TEST( mtd::moduleById( &sTypeModule,
                               aColumn->type.dataTypeId )
              != IDE_SUCCESS );

    //------------------------------------------
    // Image Comparison
    //------------------------------------------

    sValue1 = mtd::valueForModule( (smiColumn*) aColumn,
                                   aRow1,
                                   aFlag1,
                                   aColumn->module->staticNull );
    sValue2 = mtd::valueForModule( (smiColumn*) aColumn,
                                   aRow2,
                                   aFlag2,
                                   aColumn->module->staticNull );

    // check both data is NULL
    if ( ( sTypeModule->isNull( aColumn, sValue1 ) == ID_TRUE ) &&
         ( sTypeModule->isNull( aColumn, sValue2 ) == ID_TRUE ) )
    {
        // both are NULL value
        *aOutIsSame = ID_TRUE;
    }
    else
    {
        // Data Length
        sLength1 = sTypeModule->actualSize( aColumn, sValue1 );
        sLength2 = sTypeModule->actualSize( aColumn, sValue2 );

        if ( sLength1 == sLength2 )
        {
            if ( idlOS::memcmp( sValue1,
                                sValue2,
                                sLength1 ) == 0 )
            {
                // same image
                *aOutIsSame = ID_TRUE;
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::initializeColumn( mtcColumn       * aColumn,
                              const mtdModule * aModule,
                              UInt              aArguments,
                              SInt              aPrecision,
                              SInt              aScale )
{
/***********************************************************************
 *
 * Description : mtcColumn�� �ʱ�ȭ
 *              ( mtdModule �� mtlModule�� �������� ��� )
 *
 * Implementation :
 *
 ***********************************************************************/
    const mtdModule * sModule;

    // smiColumn ���� ����
    aColumn->column.flag = SMI_COLUMN_TYPE_FIXED;

    if ( aModule->id == MTD_NUMBER_ID )
    {
        if( aArguments == 0 )
        {
            IDE_TEST( mtd::moduleById( & sModule, MTD_FLOAT_ID )
                      != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST( mtd::moduleById( & sModule, MTD_NUMERIC_ID )
                      != IDE_SUCCESS);
        }
    }
    else
    {
        sModule = aModule;
    }

    // data type module ���� ����
    aColumn->type.dataTypeId = sModule->id;
    aColumn->module = sModule;

    aColumn->flag = aArguments;
    aColumn->precision = aPrecision;
    aColumn->scale = aScale;

    // �߰����� �ʱ�ȭ
    idlOS::memset( &(aColumn->mColumnAttr),
                   0x00,
                   ID_SIZEOF( mtcColumnAttr ) );

    // data type module�� semantic �˻�
    IDE_TEST( sModule->estimate( & aColumn->column.size,
                                 & aColumn->flag,
                                 & aColumn->precision,
                                 & aColumn->scale )
              != IDE_SUCCESS );

    if( (mtl::mDBCharSet != NULL) && (mtl::mNationalCharSet != NULL) )
    {
        // create database�� ��� validation ��������
        // mtl::mDBCharSet, mtl::mNationalCharSet�� �����ϰ� �ȴ�.

        // Nothing to do
        if( (sModule->id == MTD_NCHAR_ID) ||
            (sModule->id == MTD_NVARCHAR_ID) )
        {
            aColumn->type.languageId = mtl::mNationalCharSet->id;
            aColumn->language = mtl::mNationalCharSet;
        }
        else
        {
            aColumn->type.languageId = mtl::mDBCharSet->id;
            aColumn->language = mtl::mDBCharSet;
        }
    }
    else
    {
        // create database�� �ƴ� ���
        // �ӽ÷� ���� ��, META �ܰ迡�� �缳���Ѵ�.
        // fixed table, PV�� ASCII�� �����ص� �������.
        // (ASCII�̿��� ���� ���� ����)
        mtl::mDBCharSet = & mtlAscii;
        mtl::mNationalCharSet = & mtlUTF16;

        aColumn->type.languageId = mtl::mDBCharSet->id;
        aColumn->language = mtl::mDBCharSet;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::initializeColumn( mtcColumn       * aColumn,
                              UInt              aDataTypeId,
                              UInt              aArguments,
                              SInt              aPrecision,
                              SInt              aScale )
{
/***********************************************************************
 *
 * Description : mtcColumn�� �ʱ�ȭ
 *               ( mtdModule �� mtlModule�� ã�ƾ� �ϴ� ��� )
 *
 * Implementation :
 *
 ***********************************************************************/
    const mtdModule * sModule;
    const mtdModule * sRealModule;

    // smiColumn ���� ����
    aColumn->column.flag = SMI_COLUMN_TYPE_FIXED;

    // �ش� mtdModule ã�Ƽ� mtcColumn::module�� ����
    IDE_TEST( mtd::moduleById( & sModule, aDataTypeId ) != IDE_SUCCESS);

    if ( sModule->id == MTD_NUMBER_ID )
    {
        if( aArguments == 0 )
        {
            IDE_TEST( mtd::moduleById( & sRealModule, MTD_FLOAT_ID )
                      != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST( mtd::moduleById( & sRealModule, MTD_NUMERIC_ID )
                      != IDE_SUCCESS);
        }
    }
    else
    {
        sRealModule = sModule;
    }

    aColumn->type.dataTypeId = sRealModule->id;
    aColumn->module = sRealModule;

    // flag, precision, scale ���� ����
    aColumn->flag = aArguments;
    aColumn->precision = aPrecision;
    aColumn->scale = aScale;

    // �߰����� �ʱ�ȭ
    idlOS::memset( &(aColumn->mColumnAttr),
                   0x00,
                   ID_SIZEOF( mtcColumnAttr ) );

    // data type module�� semantic �˻�
    IDE_TEST( sRealModule->estimate( & aColumn->column.size,
                                     & aColumn->flag,
                                     & aColumn->precision,
                                     & aColumn->scale )
              != IDE_SUCCESS );

    if( (mtl::mDBCharSet != NULL) && (mtl::mNationalCharSet != NULL) )
    {
        // create database�� ��� validation ��������
        // mtl::mDBCharSet, mtl::mNationalCharSet�� �����ϰ� �ȴ�.

        // Nothing to do
        if( (sModule->id == MTD_NCHAR_ID) ||
            (sModule->id == MTD_NVARCHAR_ID) )
        {
            aColumn->type.languageId = mtl::mNationalCharSet->id;
            aColumn->language = mtl::mNationalCharSet;
        }
        else
        {
            aColumn->type.languageId = mtl::mDBCharSet->id;
            aColumn->language = mtl::mDBCharSet;
        }
    }
    else
    {
        // create database�� �ƴ� ���
        // �ӽ÷� ���� ��, META �ܰ迡�� �缳���Ѵ�.
        // fixed table, PV�� ASCII�� �����ص� �������.
        // (ASCII�̿��� ���� ���� ����)
        mtl::mDBCharSet = & mtlAscii;
        mtl::mNationalCharSet = & mtlUTF16;

        aColumn->type.languageId = mtl::mDBCharSet->id;
        aColumn->language = mtl::mDBCharSet;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void mtc::initializeColumn( mtcColumn  * aDestColumn,
                            mtcColumn  * aSrcColumn )
{
/***********************************************************************
 *
 * Description : mtcColumn�� �ʱ�ȭ
 *               ( src column���� dest column�� �ʱ�ȭ�ϴ� ���)
 *
 * Implementation :
 *
 ***********************************************************************/
    // type, module ����
    aDestColumn->type.dataTypeId = aSrcColumn->type.dataTypeId;
    aDestColumn->module          = aSrcColumn->module;

    // language, module ����
    aDestColumn->type.languageId = aSrcColumn->type.languageId;
    aDestColumn->language        = aSrcColumn->language;

    // smiColumn size, flag ���� ����
    aDestColumn->column.size = aSrcColumn->column.size;
    aDestColumn->column.flag = aSrcColumn->column.flag;

    aDestColumn->column.flag &= ~SMI_COLUMN_TYPE_MASK;
    aDestColumn->column.flag |= SMI_COLUMN_TYPE_FIXED;

    // BUG-38785
    aDestColumn->column.flag &= ~SMI_COLUMN_COMPRESSION_MASK;
    aDestColumn->column.flag |= SMI_COLUMN_COMPRESSION_FALSE;

    // mtcColumn flag, precision, scale ���� ����
    aDestColumn->flag      = aSrcColumn->flag;
    // BUG-36836
    // not null �Ӽ��� �������� �ʴ´�.
    aDestColumn->flag      &= ~MTC_COLUMN_NOTNULL_MASK;
    aDestColumn->precision = aSrcColumn->precision;
    aDestColumn->scale     = aSrcColumn->scale;

    // echar, evarchar
    if (( aDestColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                 == MTD_ENCRYPT_TYPE_TRUE )
    {
        // ���� ���� ����
        aDestColumn->mColumnAttr.mEncAttr.mEncPrecision = aSrcColumn->mColumnAttr.mEncAttr.mEncPrecision;
    }
    else
    {
        // ���� ���� ����
        aDestColumn->mColumnAttr.mEncAttr.mEncPrecision = 0;
    }
    
    idlOS::memcpy( (void*) aDestColumn->mColumnAttr.mEncAttr.mPolicy,
                   (void*) aSrcColumn->mColumnAttr.mEncAttr.mPolicy,
                   MTC_POLICY_NAME_SIZE + 1 );
}

IDE_RC mtc::initializeEncryptColumn( mtcColumn    * aColumn,
                                     const SChar  * aPolicy,
                                     UInt           aEncryptedSize,
                                     UInt           aECCSize )
{
/***********************************************************************
 * PROJ-2002 Column Security
 *
 * Description : �÷��� ��ȣȭ�� ���� mtcColumn�� �ʱ�ȭ
 *              ( mtdModule �� mtlModule�� �������� ��� )
 *
 * Implementation :
 *
 ***********************************************************************/
    IDE_ASSERT( aColumn->module != NULL );

    // echar, evarchar�� ������
    IDE_ASSERT( (aColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                 == MTD_ENCRYPT_TYPE_TRUE );

    // ���� ���� ����
    aColumn->mColumnAttr.mEncAttr.mEncPrecision = (SInt) ( aEncryptedSize + aECCSize );
    idlOS::snprintf( aColumn->mColumnAttr.mEncAttr.mPolicy,
                     MTC_POLICY_NAME_SIZE + 1,
                     "%s",
                     aPolicy );

    // data type module�� semantic �˻�
    IDE_TEST( aColumn->module->estimate( & aColumn->column.size,
                                         & aColumn->flag,
                                         & aColumn->mColumnAttr.mEncAttr.mEncPrecision,
                                         & aColumn->scale )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeSmallint( mtdSmallintType* aSmallint,
                          const UChar*     aString,
                          UInt             aLength )
{
    SLong  sLong;
    UChar *sEndPtr = NULL;

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    // BUG-29170
    errno = 0;
    sLong = idlOS::strtol( (const char*)aString, (char**)&sEndPtr, 10 );

    IDE_TEST_RAISE( errno == ERANGE, ERR_VALUE_OVERFLOW );

    if ( sEndPtr != (aString + aLength) )
    {
        sLong = (SLong)idlOS::strtod( (const char*)aString, (char**)&sEndPtr );

        IDE_TEST_RAISE( errno == ERANGE, ERR_VALUE_OVERFLOW );
        IDE_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    }

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sLong == 0 )
    {
        sLong = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    IDE_TEST_RAISE( sLong < MTD_SMALLINT_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    IDE_TEST_RAISE( sLong > MTD_SMALLINT_MAXIMUM,
                    ERR_VALUE_OVERFLOW );

    *aSmallint = (mtdSmallintType)sLong;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeInteger( mtdIntegerType* aInteger,
                         const UChar*    aString,
                         UInt            aLength )
{
    SLong  sLong;
    UChar *sEndPtr = NULL;

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    // BUG-29170
    errno = 0;
    sLong = idlOS::strtol( (const char*)aString, (char**)&sEndPtr, 10 );

    IDE_TEST_RAISE( errno == ERANGE, ERR_VALUE_OVERFLOW );

    if ( sEndPtr != (aString + aLength) )
    {
        sLong = (SLong)idlOS::strtod( (const char*)aString, (char**)&sEndPtr );

        IDE_TEST_RAISE( errno == ERANGE, ERR_VALUE_OVERFLOW );
        IDE_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );
    }

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sLong == 0 )
    {
        sLong = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    IDE_TEST_RAISE( sLong < MTD_INTEGER_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    IDE_TEST_RAISE( sLong > MTD_INTEGER_MAXIMUM,
                    ERR_VALUE_OVERFLOW );

    *aInteger = (mtdIntegerType)sLong;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeBigint( mtdBigintType* aBigint,
                        const UChar*   aString,
                        UInt           aLength )
{
    SLong           sLong;
    mtdNumericType* sNumeric;
    UChar           sNumericBuffer[MTD_NUMERIC_SIZE_MAXIMUM];

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    sNumeric = (mtdNumericType*)sNumericBuffer;

    IDE_TEST( makeNumeric( sNumeric,
                           MTD_NUMERIC_MANTISSA_MAXIMUM,
                           aString,
                           aLength ) != IDE_SUCCESS );

    if( sNumeric->length > 1 )
    {
        IDE_TEST( numeric2Slong( &sLong,
                                 sNumeric )
                  != IDE_SUCCESS );
    }
    else
    {
        sLong = 0;
    }

    IDE_TEST_RAISE( sLong < MTD_BIGINT_MINIMUM,
                    ERR_VALUE_OVERFLOW );
    IDE_TEST_RAISE( sLong > MTD_BIGINT_MAXIMUM,
                    ERR_VALUE_OVERFLOW );

    *aBigint = (mtdBigintType)sLong;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeReal( mtdRealType* aReal,
                      const UChar* aString,
                      UInt         aLength )
{
    UChar *sEndPtr = NULL;

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    errno = 0;

    *aReal = idlOS::strtod( (const char*)aString, (char**)&sEndPtr );

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( *aReal == 0 )
    {
        *aReal = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    IDE_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );

    IDE_TEST_RAISE( (*aReal == +HUGE_VAL) || (*aReal == -HUGE_VAL),
                    ERR_VALUE_OVERFLOW );

    if( ( idlOS::fabs(*aReal) < MTD_REAL_MINIMUM ) &&
        ( *aReal != 0 ) )
    {
        *aReal = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeDouble( mtdDoubleType* aDouble,
                        const UChar*   aString,
                        UInt           aLength )
{
    UChar *sEndPtr = NULL;

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    errno = 0;

    *aDouble = idlOS::strtod( (const char*)aString, (char**)&sEndPtr );

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( *aDouble == 0 )
    {
        *aDouble = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    IDE_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );

    IDE_TEST_RAISE( (*aDouble == +HUGE_VAL) || (*aDouble == -HUGE_VAL),
                    ERR_VALUE_OVERFLOW );

    if( ( idlOS::fabs(*aDouble) < MTD_DOUBLE_MINIMUM ) &&
        ( *aDouble != 0 ) )
    {
        *aDouble = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_VALUE_OVERFLOW );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_VALUE_OVERFLOW));

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeInterval( mtdIntervalType* aInterval,
                          const UChar*     aString,
                          UInt             aLength )
{
    SDouble  sDouble;
    UChar   *sEndPtr = NULL;
    SDouble  sIntegerPart;
    SDouble  sFractionalPart;

    IDE_TEST_RAISE( aLength > MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP,
                    ERR_INVALID_LITERAL );

    errno = 0;

    sDouble = idlOS::strtod( (const char*)aString, (char**)&sEndPtr );

    /* PATCH(BEGIN): GNU MATH LIBRARY - REMOVE MINUS ZERO */
    if( sDouble == 0 )
    {
        sDouble = 0;
    }
    /* PATCH(END): GNU MATH LIBRARY - REMOVE MINUS ZERO */

    IDE_TEST_RAISE( sEndPtr != (aString + aLength), ERR_INVALID_LITERAL );

    sDouble *= 864e2;

    sFractionalPart        = modf( sDouble, &sIntegerPart );
    aInterval->second      = (SLong)sIntegerPart;
    aInterval->microsecond = (SLong)( sFractionalPart * 1E6 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1597 Temp record size ��������
// bit, varbit, nibble Ÿ�Ե���
// varchar, char ���� Ÿ�԰��� �ٸ��� DISK DB������ header�� �����ؾ� �Ѵ�.
// ������ getMtdHeaderSize()�� �̵� Ÿ���� ������� �ʰ� header ���̸�
// ��ȯ�ϱ� ������ column ���� actual size - header size ��ŭ ������
// Ȯ���ϸ� ������ �ȴ�.
// �̸� �ذ��ϱ� ���� bit, varbit, nibble ���� Ÿ���� �����
// headerSize�� ��ȯ�ϴ� �Լ��� ������ �������.
// �� �Լ��� SM�� qdbCommon���� ���ȴ�.
// �� �Լ��� �ǹ̴� �÷��� DISK DB�� ����� ��,
// ������� �ʴ� ������ ���̸� ���Ѵ�.
// bit ���� Ÿ���� �÷� ���� ��ΰ� ����Ǿ�� �ϱ� ������
// �� ���� 0�̴�.
IDE_RC mtc::getNonStoringSize( const smiColumn *aColumn, UInt * aOutSize )
{
    const mtcColumn* sColumn;
    const mtdModule* sModule;

    sColumn = (const mtcColumn*)aColumn;

    /* PROJ-2118 */
    IDE_TEST( mtd::moduleById( &sModule,
                               sColumn->type.dataTypeId )
              != IDE_SUCCESS);

    if ( (sModule->flag & MTD_DATA_STORE_MTDVALUE_MASK)
        == MTD_DATA_STORE_MTDVALUE_TRUE )
    {
        *aOutSize = 0;
    }
    else
    {
        *aOutSize = sModule->headerSize();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeBinary( mtdBinaryType* aBinary,
                        const UChar*   aString,
                        UInt           aLength )
{
    UInt sBinaryLength = 0;

    IDE_TEST( mtc::makeBinary( aBinary->mValue,
                               aString,
                               aLength,
                               &sBinaryLength,
                               NULL)
              != IDE_SUCCESS );

    aBinary->mLength = sBinaryLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeBinary( UChar*       aBinaryValue,
                        const UChar* aString,
                        UInt         aLength,
                        UInt*        aBinaryLength,
                        idBool*      aOddSizeFlag )
{
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    UChar        sBinary;
    UInt         sBinaryLength = *aBinaryLength;

    const UChar* sSrcText = aString;
    UInt         sSrcLen = aLength;

    static const UChar sHexByte[256] = {
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

    sIterator = aBinaryValue;

    /* bug-31397: large sql_byte params make buffer overflow.
     * If data is divided into several pieces due to the CM block limit,
     * the last odd byte of the 1st piece is combined with
     * the 1st byte of the 2nd piece.
     *  ex) 1st piece: 1 2 3 4 5     2nd piece: 6 7 8
     *  convert the 1st piece: 0x12 0x34 0x50 (set aOddSizeFlag to 1)
     *  convert the 2nd piece: 0x12 0x34 0x56 0x78 */
    if ((aOddSizeFlag != NULL) && (*aOddSizeFlag == ID_TRUE) &&
        (*aBinaryLength > 0) && (sSrcLen > 0))
    {
        /* get the last byte of the target buffer */
        sBinary = *(sIterator - 1);
        IDE_TEST_RAISE( sHexByte[sSrcText[0]] > 15, ERR_INVALID_LITERAL );
        sBinary |= sHexByte[sSrcText[0]];
        *(sIterator - 1) = sBinary;

        sSrcText++;
        sSrcLen--;
        *aOddSizeFlag = ID_FALSE;
    }

    for( sToken = sSrcText, sTokenFence = sToken + sSrcLen;
         sToken < sTokenFence;
         sIterator++, sToken += 2 )
    {
        IDE_TEST_RAISE( sHexByte[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sBinary = sHexByte[sToken[0]] << 4;
        if( sToken + 1 < sTokenFence )
        {
            IDE_TEST_RAISE( sHexByte[sToken[1]] > 15,
                            ERR_INVALID_LITERAL );
            sBinary |= sHexByte[sToken[1]];
        }
        /* bug-31397: large sql_byte params make buffer overflow.
         * If the size is odd, set aOddSizeFlag to 1 */
        else if ((sToken + 1 == sTokenFence) && (aOddSizeFlag != NULL))
        {
            *aOddSizeFlag = ID_TRUE;
        }
        sBinaryLength++;
        *sIterator = sBinary;
    }
    *aBinaryLength = sBinaryLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeByte( mtdByteType* aByte,
                      const UChar* aString,
                      UInt         aLength )
{
    UInt sByteLength = 0;

    IDE_TEST( mtc::makeByte( aByte->value,
                             aString,
                             aLength,
                             &sByteLength,
                             NULL)
              != IDE_SUCCESS );

    aByte->length = sByteLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeByte( UChar*       aByteValue,
                      const UChar* aString,
                      UInt         aLength,
                      UInt*        aByteLength,
                      idBool*      aOddSizeFlag)
{
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    UChar        sByte;
    UInt         sByteLength = *aByteLength;

    const UChar* sSrcText = aString;
    UInt         sSrcLen = aLength;

    static const UChar sHexByte[256] = {
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

    /* bug-31397: large sql_byte params make buffer overflow.
     * If data is divided into several pieces due to the CM block limit,
     * the last odd byte of the 1st piece is combined with
     * the 1st byte of the 2nd piece.
     *  ex) 1st piece: 1 2 3 4 5     2nd piece: 6 7 8
     *  convert the 1st piece: 0x12 0x34 0x50 (set aOddSizeFlag to 1)
     *  convert the 2nd piece: 0x12 0x34 0x56 0x78 */
    if ((aOddSizeFlag != NULL) && (*aOddSizeFlag == ID_TRUE) &&
        (*aByteLength > 0) && (sSrcLen > 0))
    {
        /* get the last byte of the target buffer */
        sByte = *(sIterator - 1);
        IDE_TEST_RAISE( sHexByte[sSrcText[0]] > 15, ERR_INVALID_LITERAL );
        sByte |= sHexByte[sSrcText[0]];
        *(sIterator - 1) = sByte;

        sSrcText++;
        sSrcLen--;
        *aOddSizeFlag = ID_FALSE;
    }

    for( sToken = sSrcText, sTokenFence = sToken + sSrcLen;
         sToken < sTokenFence;
         sIterator++, sToken += 2 )
    {
        IDE_TEST_RAISE( sHexByte[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sByte = sHexByte[sToken[0]] << 4;
        if( sToken + 1 < sTokenFence )
        {
            IDE_TEST_RAISE( sHexByte[sToken[1]] > 15,
                            ERR_INVALID_LITERAL );
            sByte |= sHexByte[sToken[1]];
        }
        /* bug-31397: large sql_byte params make buffer overflow.
         * If the size is odd, set aOddSizeFlag to 1 */
        else if ((sToken + 1 == sTokenFence) && (aOddSizeFlag != NULL))
        {
            *aOddSizeFlag = ID_TRUE;
        }
        sByteLength++;
        IDE_TEST_RAISE( sByteLength > MTD_BYTE_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );
        *sIterator = sByte;
    }
    *aByteLength = sByteLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeNibble( mtdNibbleType* aNibble,
                        const UChar*   aString,
                        UInt           aLength )
{
    UInt sNibbleLength = 0;

    IDE_TEST( mtc::makeNibble( aNibble->value,
                               0,
                               aString,
                               aLength,
                               &sNibbleLength )
              != IDE_SUCCESS );

    aNibble->length = sNibbleLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeNibble( UChar*       aNibbleValue,
                        UChar        aOffsetInByte,
                        const UChar* aString,
                        UInt         aLength,
                        UInt*        aNibbleLength )
{
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    UChar        sNibble;
    UInt         sNibbleLength = *aNibbleLength;

    static const UChar sHexNibble[256] = {
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

    sToken = (const UChar*)aString;
    sTokenFence = sToken + aLength;

    if( (aOffsetInByte == 1) && (sToken < sTokenFence) )
    {
        IDE_TEST_RAISE( sHexNibble[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sNibble = (*sIterator) | sHexNibble[sToken[0]];
        sNibbleLength++;
        IDE_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );

        *sIterator = sNibble;
        sIterator++;
        sToken += 1;
    }

    for( ;sToken < sTokenFence; sIterator++, sToken += 2 )
    {
        IDE_TEST_RAISE( sHexNibble[sToken[0]] > 15, ERR_INVALID_LITERAL );
        sNibble = sHexNibble[sToken[0]] << 4;
        sNibbleLength++;
        IDE_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );

        if( sToken + 1 < sTokenFence )
        {
            IDE_TEST_RAISE( sHexNibble[sToken[1]] > 15, ERR_INVALID_LITERAL );
            sNibble |= sHexNibble[sToken[1]];
            sNibbleLength++;
            IDE_TEST_RAISE( sNibbleLength > MTD_NIBBLE_PRECISION_MAXIMUM,
                            ERR_INVALID_LITERAL );
        }

        *sIterator = sNibble;
    }

    *aNibbleLength = sNibbleLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeBit( mtdBitType*  aBit,
                     const UChar* aString,
                     UInt         aLength )
{
    UInt sBitLength = 0;

    IDE_TEST( mtc::makeBit( aBit->value,
                            0,
                            aString,
                            aLength,
                            &sBitLength )
              != IDE_SUCCESS );

    aBit->length = sBitLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::makeBit( UChar*       aBitValue,
                     UChar        aOffsetInBit,
                     const UChar* aString,
                     UInt         aLength,
                     UInt*        aBitLength )
{
    const UChar* sToken;
    const UChar* sTokenFence;
    UChar*       sIterator;
    UChar        sBit;
    UInt         sSubIndex;
    UInt         sBitLength = *aBitLength;
    UInt         sTokenOffset = 0;

    static const UChar sHexBit[256] = {
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
    sToken = (const UChar*)aString;
    sTokenFence = sToken + aLength;

    if( aOffsetInBit > 0 )
    {
        sBit = *sIterator;

        IDE_TEST_RAISE( sHexBit[sToken[0]] > 1, ERR_INVALID_LITERAL );

        sSubIndex    = aOffsetInBit;

        while( (sToken + sTokenOffset < sTokenFence) && (sSubIndex < 8) )
        {
            IDE_TEST_RAISE( sHexBit[sToken[sTokenOffset]] > 1, ERR_INVALID_LITERAL );
            sBit |= ( sHexBit[sToken[sTokenOffset]] << ( 7 - sSubIndex ) );
            sBitLength++;
            IDE_TEST_RAISE( sBitLength > MTD_BIT_PRECISION_MAXIMUM,
                            ERR_INVALID_LITERAL );
            sTokenOffset++;
            sSubIndex++;
        }

        sToken += sTokenOffset;
        *sIterator = sBit;

        sIterator++;
    }

    for( ; sToken < sTokenFence; sIterator++, sToken += 8 )
    {
        IDE_TEST_RAISE( sHexBit[sToken[0]] > 1, ERR_INVALID_LITERAL );

        // �� �ֱ� ���� 0���� �ʱ�ȭ
        idlOS::memset( sIterator,
                       0x00,
                       1 );

        sBit = sHexBit[sToken[0]] << 7;
        sBitLength++;
        IDE_TEST_RAISE( sBitLength > MTD_BIT_PRECISION_MAXIMUM,
                        ERR_INVALID_LITERAL );

        sSubIndex = 1;
        while( sToken + sSubIndex < sTokenFence && sSubIndex < 8 )
        {
            IDE_TEST_RAISE( sHexBit[sToken[sSubIndex]] > 1, ERR_INVALID_LITERAL );
            sBit |= ( sHexBit[sToken[sSubIndex]] << ( 7 - sSubIndex ) );
            sBitLength++;
            IDE_TEST_RAISE( sBitLength > MTD_BIT_PRECISION_MAXIMUM,
                            ERR_INVALID_LITERAL );
            sSubIndex++;
        }

        *sIterator = sBit;
    }

    *aBitLength = sBitLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool mtc::compareOneChar( UChar * aValue1,
                            UChar   aValue1Size,
                            UChar * aValue2,
                            UChar   aValue2Size )
{
    idBool sIsSame;

    if( aValue1Size != aValue2Size )
    {
        sIsSame = ID_FALSE;
    }
    else
    {
        sIsSame = ( idlOS::memcmp( aValue1, aValue2,
                                   aValue1Size ) == 0
                    ? ID_TRUE : ID_FALSE );
    }

    return sIsSame;
}

IDE_RC mtc::getLikeEcharKeySize( mtcTemplate * aTemplate,
                                 UInt        * aECCSize,
                                 UInt        * aKeySize )
{
    UInt  sECCSize;

    IDE_TEST( aTemplate->getECCSize( MTC_LIKE_KEY_PRECISION,
                                     & sECCSize )
              != IDE_SUCCESS );

    *aKeySize = idlOS::align( ID_SIZEOF(UShort)
                              + ID_SIZEOF(UShort)
                              + MTC_LIKE_KEY_PRECISION + sECCSize,
                              8 );

    if ( aECCSize != NULL )
    {
        *aECCSize = sECCSize;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::getLobLengthLocator( smLobLocator   aLobLocator,
                                 idBool       * aIsNull,
                                 UInt         * aLobLength,
                                 idvSQL       * aStatistics )
{
    SLong  sLobLength;
    idBool sIsNulLob;

    // BUG-41525
    if ( aLobLocator == MTD_LOCATOR_NULL )
    {
        *aIsNull    = ID_TRUE;
        *aLobLength = 0;
    }
    else
    {
        IDE_TEST( mtc::getLobLengthWithLocator( aStatistics,
                                                aLobLocator,
                                                & sLobLength,
                                                & sIsNulLob )
                  != IDE_SUCCESS );
    
        if ( sIsNulLob == ID_TRUE )
        {
            *aIsNull    = ID_TRUE;
            *aLobLength = 0;
        }
        else
        {
            IDE_TEST_RAISE( (sLobLength < 0) || (sLobLength > ID_UINT_MAX),
                            ERR_LOB_SIZE );
    
            *aIsNull    = ID_FALSE;
            *aLobLength = (UInt)sLobLength;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LOB_SIZE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mtc::isNullLobRow( const void      * aRow,
                          const smiColumn * aColumn,
                          idBool          * aIsNull )
{
    if ( mtc::isNullLobColumnWithRow(aRow, aColumn) == ID_TRUE )
    {
        *aIsNull = ID_TRUE;
    }
    else
    {
        *aIsNull = ID_FALSE;
    }

    return IDE_SUCCESS;
}

/* PROJ-2209 DBTIMEZONE */
SLong mtc::getSystemTimezoneSecond( void )
{
    PDL_Time_Value sTimevalue;
    time_t         sTime;
    struct tm      sLocaltime;
    struct tm      sGlobaltime;
    SLong          sLocalSecond;
    SLong          sGlobalSecond;
    SLong          sHour;
    SLong          sMin;
    SInt           sDayOffset;

    sTimevalue = idlOS::gettimeofday();
    sTime = (time_t)sTimevalue.sec();

    idlOS::localtime_r( &sTime, &sLocaltime );
    idlOS::gmtime_r( &sTime, &sGlobaltime );

    sHour = (SLong)sLocaltime.tm_hour;
    sMin  = (SLong)sLocaltime.tm_min;
    sLocalSecond  = ( sHour * 60 * 60 ) + ( sMin * 60 );

    sHour = (SLong)sGlobaltime.tm_hour;
    sMin  = (SLong)sGlobaltime.tm_min;
    sGlobalSecond = ( sHour * 60 * 60 ) + ( sMin * 60 );

    if ( sLocaltime.tm_mday != sGlobaltime.tm_mday )
    {
        sDayOffset = sLocaltime.tm_mday - sGlobaltime.tm_mday;
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

SChar * mtc::getSystemTimezoneString( SChar * aTimezoneString )
{
    SLong sTimezoneSecond;
    SLong sAbsTimezoneSecond;

    sTimezoneSecond = getSystemTimezoneSecond();

    if ( sTimezoneSecond < 0 )
    {
        sAbsTimezoneSecond = sTimezoneSecond * -1;
        idlOS::snprintf( aTimezoneString,
                         MTC_TIMEZONE_VALUE_LEN + 1,
                         "-%02"ID_INT64_FMT":%02"ID_INT64_FMT,
                         sAbsTimezoneSecond / 3600, ( sAbsTimezoneSecond % 3600 ) / 60 );
    }
    else
    {
        idlOS::snprintf( aTimezoneString,
                         MTC_TIMEZONE_VALUE_LEN + 1,
                         "+%02"ID_INT64_FMT":%02"ID_INT64_FMT,
                         sTimezoneSecond / 3600, ( sTimezoneSecond % 3600 ) / 60 );
    }

    return aTimezoneString;
}

/* PROJ-1517 */
void mtc::makeFloatConversionModule( mtcStack         *aStack,
                                     const mtdModule **aModule )
{
    if ( aStack->column->module->id == MTD_NUMERIC_ID )
    {
        *aModule = &mtdNumeric;
    }
    else
    {
        *aModule = &mtdFloat;
    }
}

// BUG-37484
idBool mtc::needMinMaxStatistics( const smiColumn* aColumn )
{
    const mtcColumn * sColumn;
    idBool            sRc;

    sColumn = (const mtcColumn*)aColumn;

    if( sColumn->type.dataTypeId == MTD_GEOMETRY_ID )
    {
        sRc = ID_FALSE;
    }
    else
    {
        sRc = ID_TRUE;
    }

    return sRc;
}

/* PROJ-1071 Parallel query */
IDE_RC mtc::cloneTemplate4Parallel( iduMemory    * aMemory,
                                    mtcTemplate  * aSource,
                                    mtcTemplate  * aDestination )
{
    UShort sTupleIndex;

    aDestination->stackCount            = aSource->stackCount;
    aDestination->dataSize              = aSource->dataSize;
    aDestination->execInfoCnt           = aSource->execInfoCnt;
    aDestination->initSubquery          = aSource->initSubquery;
    aDestination->fetchSubquery         = aSource->fetchSubquery;
    aDestination->finiSubquery          = aSource->finiSubquery;
    aDestination->setCalcSubquery       = aSource->setCalcSubquery;
    aDestination->getOpenedCursor       = aSource->getOpenedCursor;
    aDestination->addOpenedLobCursor    = aSource->addOpenedLobCursor;
    aDestination->isBaseTable           = aSource->isBaseTable;
    aDestination->closeLobLocator       = aSource->closeLobLocator;
    aDestination->getSTObjBufSize       = aSource->getSTObjBufSize;
    aDestination->rowCount              = aSource->rowCount;
    aDestination->rowCountBeforeBinding = aSource->rowCountBeforeBinding;
    aDestination->variableRow           = aSource->variableRow;
    aDestination->encrypt               = aSource->encrypt;
    aDestination->decrypt               = aSource->decrypt;
    aDestination->encodeECC             = aSource->encodeECC;
    aDestination->getDecryptInfo        = aSource->getDecryptInfo;
    aDestination->getECCInfo            = aSource->getECCInfo;
    aDestination->getECCSize            = aSource->getECCSize;
    aDestination->dateFormat            = aSource->dateFormat;
    aDestination->dateFormatRef         = aSource->dateFormatRef;
    aDestination->nlsCurrency           = aSource->nlsCurrency;
    aDestination->nlsCurrencyRef        = aSource->nlsCurrencyRef;
    aDestination->groupConcatPrecisionRef = aSource->groupConcatPrecisionRef;
    aDestination->arithmeticOpMode      = aSource->arithmeticOpMode;
    aDestination->arithmeticOpModeRef   = aSource->arithmeticOpModeRef;
    aDestination->funcDataCnt           = aSource->funcDataCnt;

    idlOS::memcpy(aDestination->currentRow,
                  aSource->currentRow,
                  MTC_TUPLE_TYPE_MAXIMUM * ID_SIZEOF(UShort) );

    // stack buffer is set on allocation

    // data �� ���߿� �����Ѵ�.

    // execInfo �� ���߿� �����Ѵ�.

    for ( sTupleIndex = 0;
          sTupleIndex < aDestination->rowCount;
          sTupleIndex++ )
    {
        IDE_TEST( cloneTuple4Parallel( aMemory,
                                       aSource,
                                       sTupleIndex,
                                       aDestination,
                                       sTupleIndex )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-1071 Parallel query */
IDE_RC mtc::cloneTuple4Parallel( iduMemory   * /*aMemory*/,
                                 mtcTemplate * aSrcTemplate,
                                 UShort        aSrcTupleID,
                                 mtcTemplate * aDstTemplate,
                                 UShort        aDstTupleID )
{
    ULong sFlag;

    sFlag = aSrcTemplate->rows[aSrcTupleID].lflag;

    aDstTemplate->rows[aDstTupleID].modify
        = aSrcTemplate->rows[aSrcTupleID].modify;
    aDstTemplate->rows[aDstTupleID].lflag
        = aSrcTemplate->rows[aSrcTupleID].lflag;
    aDstTemplate->rows[aDstTupleID].columnCount
        = aSrcTemplate->rows[aSrcTupleID].columnCount;
    aDstTemplate->rows[aDstTupleID].columnMaximum
        = aSrcTemplate->rows[aSrcTupleID].columnMaximum;
    aDstTemplate->rows[aDstTupleID].rowOffset
        = aSrcTemplate->rows[aSrcTupleID].rowOffset;
    aDstTemplate->rows[aDstTupleID].rowMaximum
        = aSrcTemplate->rows[aSrcTupleID].rowMaximum;

    // columns �� execute �� ���� ���ø��� ���� �����ؼ� ����Ѵ�.
    aDstTemplate->rows[aDstTupleID].columns
        = aSrcTemplate->rows[aSrcTupleID].columns;
    aDstTemplate->rows[aDstTupleID].execute
        = aSrcTemplate->rows[aSrcTupleID].execute;

    // constant, variable, table tuple �� row �� �����ؾ� �Ѵ�.
    // Intermediate tuple �� ������ �������� ROW ALLOC �� FALSE �̴�.
    if ( (sFlag & MTC_TUPLE_ROW_ALLOCATE_MASK)
         == MTC_TUPLE_ROW_ALLOCATE_FALSE )
    {
        // ���� row �� ����� MTC_TUPLE_ROW_SET_MASK �� ���� �����ȴ�.
        // �� mask �� prepare ���� ������ �������� ������ mask �̴�.
        // ������ parallel �� ���� template clone ��
        // execution ���߿� �Ͼ�� ������
        // ���� ���� ���� template �� �״�� ����ϱ� ���� row �� ���� �Ҵ��ϴ�
        // intermediate tuple �� ������ ������ tuple ���� row �� �����ؾ� �Ѵ�.
        aDstTemplate->rows[aDstTupleID].row
            = aSrcTemplate->rows[aSrcTupleID].row;
    }
    else
    {
        /* Nothing to do. */
    }

    aDstTemplate->rows[aDstTupleID].partitionTupleID
        = aSrcTemplate->rows[aSrcTupleID].partitionTupleID;
    aDstTemplate->rows[aDstTupleID].cursorInfo
        = aSrcTemplate->rows[aSrcTupleID].cursorInfo;
    aDstTemplate->rows[aDstTupleID].ridExecute =
        aSrcTemplate->rows[aSrcTupleID].ridExecute;

    return IDE_SUCCESS;
}

/* PROJ-2399 */
IDE_RC mtc::getColumnStoreLen( const smiColumn * aColumn,
                               UInt            * aActualColLen )
{
    mtcColumn       * sColumn;
    const mtdModule * sModule;

    sColumn = (mtcColumn*)aColumn;

    if ( (sColumn->flag & MTC_COLUMN_NOTNULL_MASK ) == MTC_COLUMN_NOTNULL_FALSE )
    {
        /* not null�� �ƴ� column�� variable column���� ���� �Ѵ�. */
        *aActualColLen = ID_UINT_MAX;
    }
    else
    {
        if ( (sColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK) 
             != SMI_COLUMN_COMPRESSION_TRUE )
        {
            /* sm�� ������ ����� column length�� ���Ѵ�.
             * variable column�� ��� ID_UINT_MAX�� ��ȯ�ȴ�. */
            IDE_TEST( mtd::moduleById( &sModule, sColumn->type.dataTypeId )
                      != IDE_SUCCESS );
 
            *aActualColLen = sModule->storeSize( aColumn );
        }
        else
        {
            // PROJ-2429 Dictionary based data compress for on-disk DB
            // Dictionary column�� ũ��� ID_SIZEOF(smOID)�̴�.
            *aActualColLen = ID_SIZEOF(smOID);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
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
SInt mtc::weekOfYearForStandard( SInt aYear,
                                 SInt aMonth,
                                 SInt aDay )
{
    SInt    sWeekOfYear  = 0;
    SInt    sDayOfWeek   = 0;
    SInt    sDayOfYear   = 0;
    SInt    sFirstSunday = 0;
    idBool  sIsPrevYear  = ID_FALSE;

    /* Step 1. ������ ���մϴ�. */
    if ( aMonth == 1 )
    {
        switch ( aDay )
        {
            case 1 :
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
                if ( ( sDayOfWeek >= 5 ) || ( sDayOfWeek == 0 ) )   // ������
                {
                    sIsPrevYear = ID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 2 :
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek >= 5 )                              // ����
                {
                    sIsPrevYear = ID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
                break;

            case 3 :
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
                if ( sDayOfWeek == 5 )                              // ��
                {
                    sIsPrevYear = ID_TRUE;
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
        if ( sIsPrevYear == ID_TRUE )
        {
            sDayOfWeek = dayOfWeek( aYear - 1, 1, 1 );
            sDayOfYear = dayOfYear( aYear - 1, 12, 31 );
        }
        else
        {
            sDayOfWeek = dayOfWeek( aYear, 1, 1 );
            sDayOfYear = dayOfYear( aYear, aMonth, aDay );
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
SInt mtc::yearForStandard( SInt aYear,
                           SInt aMonth,
                           SInt aDay )
{
    SInt    sYear  = 0;
    SInt    sDayOfWeek   = 0;

    /* Step 1. ������ ���մϴ�. */
    sYear = aYear;

    if ( aMonth == 1 )
    {
        switch ( aDay )
        {
            case 1 :
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
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
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
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
                sDayOfWeek = dayOfWeek( aYear, 1, 1 );
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
                sDayOfWeek = dayOfWeek( aYear, 12, 31 );
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
