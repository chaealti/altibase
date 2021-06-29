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
 * $Id: mtcc.h 36231 2009-10-22 04:07:06Z kumdory $
 **********************************************************************/

#ifndef _O_MTCC_H_
#define _O_MTCC_H_ 1

#include <mtccDef.h>
#include <mtcdTypes.h>
#include <mtca.h>

#define MTC_MAXIMUM_EXTERNAL_GROUP_CNT          (128)
#define MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP  (1024)

/* PROJ-2209 DBTIMEZONE */
#define MTC_TIMEZONE_NAME_LEN  (40)
#define MTC_TIMEZONE_VALUE_LEN (6)

ACP_EXTERN_C_BEGIN

extern const acp_uint8_t mtcHashPermut[256];

ACP_INLINE acp_sint32_t mtcDayOfYear( acp_sint32_t aYear,
                                      acp_sint32_t aMonth,
                                      acp_sint32_t aDay )
{
    acp_sint32_t sDays[2][14] = {
        { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
        { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
    };

    // BUG-22710
    if ( mtdDateInterfaceIsLeapYear( aYear ) == ACP_TRUE )
    {
        return sDays[1][aMonth] + aDay;
    }
    else
    {
        return sDays[0][aMonth] + aDay;
    }
}

ACP_INLINE acp_sint32_t mtcDayOfCommonEra( acp_sint32_t aYear,
                                           acp_sint32_t aMonth,
                                           acp_sint32_t aDay )
{
    acp_sint32_t sDays = 0;

    if ( aYear < 1582 )
    {
        /* BUG-36296 ��������� 4�⸶�� �����̶�� �����Ѵ�.
         *  - ��� �θ��� ��ġ�� �����콺 ī�̻縣�� ����� 45����� �����콺���� �����Ͽ���.
         *  - �ʱ��� �����콺��(����� 45�� ~ ����� 8��)������ ������ 3�⿡ �� �� �ǽ��Ͽ���. (Oracle 11g ������)
         *  - BC 4713����� �����콺���� ����Ѵ�. õ�����ڵ��� �����콺���� ����Ѵ�. 4�⸶�� �����̴�.
         *
         *  AD 0���� �������� �ʴ´�. aYear�� 0�̸�, BC 1���̴�. ��, BC 1���� ������ AD 1���̴�.
         */
        if ( aYear <= 0 )
        {
            sDays = ( aYear * 365 ) + ( aYear / 4 )
                  + mtcDayOfYear( aYear, aMonth, aDay )
                  - 366; /* BC 1��(aYear == 0)�� �����̴�. */
        }
        /* BUG-36296 �׷������� ���� ��Ģ�� 1583����� �����Ѵ�. 1582�� �������� 4�⸶�� �����̴�. */
        else
        {
            sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                  + mtcDayOfYear( aYear, aMonth, aDay );
        }
    }
    else if ( aYear == 1582 )
    {
        /* BUG-36296 �׷������� 1582�� 10�� 15�Ϻ��� �����Ѵ�. */
        if ( ( aMonth < 10 ) ||
             ( ( aMonth == 10 ) && ( aDay < 15 ) ) )
        {
            sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                  + mtcDayOfYear( aYear, aMonth, aDay );
        }
        else
        {
            sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                  + mtcDayOfYear( aYear, aMonth, aDay )
                  - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
        }
    }
    else
    {
        /* BUG-36296 1600�� ������ �����콺�°� �׷������� ������ ����. */
        if ( aYear <= 1600 )
        {
            sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                  + mtcDayOfYear( aYear, aMonth, aDay )
                  - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
        }
        else
        {
            sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                  - ( ( aYear - 1 - 1600 ) / 100 ) + ( ( aYear - 1 - 1600 ) / 400 )
                  + mtcDayOfYear( aYear, aMonth, aDay )
                  - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
        }
    }

    /* AD 0001�� 1�� 1���� Day 0 �̴�. �׸���, BC 0001�� 12�� 31���� Day -1 �̴�. */
    sDays--;

    return sDays;
}

/* �Ͽ��� : 0, ����� : 6 */
ACP_INLINE acp_sint32_t mtcDayOfWeek( acp_sint32_t aYear,
                                      acp_sint32_t aMonth,
                                      acp_sint32_t aDay )
{
    /* ������� ��쿡�� ������ �����Ƿ�, %7�� �����ϰ� �������� ���Ѵ�.
     * AD 0001�� 01�� 01���� �����(sDays = 0)�̴�. +6�� �Ͽ� �����Ѵ�.
     */
    return ( ( mtcDayOfCommonEra( aYear, aMonth, aDay ) % 7 ) + 6 ) % 7;
}

/* �޷°� ��ġ�ϴ� ���� : �ְ� �Ͽ��Ϻ��� �����Ѵ�. */
ACP_INLINE acp_sint32_t mtcWeekOfYear( acp_sint32_t aYear,
                                       acp_sint32_t aMonth,
                                       acp_sint32_t aDay )
{
    acp_double_t sWeek;
    
    // Always success
    (void)acpMathCeil( (acp_double_t)( mtcDayOfWeek ( aYear, 1, 1 ) +
                                       mtcDayOfYear ( aYear, aMonth, aDay ) ) / 7,
                       &sWeek);
    
    return (acp_sint32_t)sWeek;
}

/* BUG-42941 TO_CHAR()�� WW2(Oracle Version WW) �߰� */
ACP_INLINE acp_sint32_t mtcWeekOfYearForOracle( acp_sint32_t aYear,
                                                acp_sint32_t aMonth,
                                                acp_sint32_t aDay )
{
    acp_sint32_t sWeek = 0;

    sWeek = ( mtcDayOfYear( aYear, aMonth, aDay ) + 6 ) / 7;

    return sWeek;
}

/* BUG-42926 TO_CHAR()�� IW �߰� */
acp_sint32_t mtcWeekOfYearForStandard( acp_sint32_t aYear,
                                       acp_sint32_t aMonth,
                                       acp_sint32_t aDay );

/* BUG-46727 TO_CHAR()�� IYYY �߰� */
acp_sint32_t mtcYearForStandard( acp_sint32_t aYear,
                                 acp_sint32_t aMonth,
                                 acp_sint32_t aDay );

extern const acp_uint32_t mtcHashInitialValue;

// mtd::valueForModule���� ����Ѵ�.
// PROJ-1579 NCHAR
extern mtcGetDBCharSet            getDBCharSet;
extern mtcGetNationalCharSet      getNationalCharSet;

extern acp_uint32_t         mtcExtTypeModuleGroupCnt;
extern mtdModule ** mtcExtTypeModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

extern acp_uint32_t         mtcExtCvtModuleGroupCnt;
extern mtvModule ** mtcExtCvtModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

extern acp_uint32_t         mtcExtFuncModuleGroupCnt;
extern mtfModule ** mtcExtFuncModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

ACI_RC mtcAddExtTypeModule( mtdModule ** aExtTypeModules );
ACI_RC mtcAddExtCvtModule( mtvModule **  aExtCvtModules );
ACI_RC mtcAddExtFuncModule( mtfModule ** aExtFuncModules );

ACI_RC mtcInitializeForClient( acp_char_t * aDefaultNls );

ACI_RC mtcFinalizeForClient( void );

// BUG-39147
void mtcDestroyForClient();

acp_uint32_t mtcIsSameType( const mtcColumn* aColumn1,
                            const mtcColumn* aColumn2 );

void mtcCopyColumn( mtcColumn      * aDestination,
                    const mtcColumn* aSource );

const void* mtcValue( const mtcColumn* aColumn,
                      const void     * aRow,
                      acp_uint32_t     aFlag );

acp_uint32_t mtcHash( acp_uint32_t       aHash,
                      const acp_uint8_t* aValue,
                      acp_uint32_t       aLength );

// fix BUG-9496
acp_uint32_t mtcHashWithExponent( acp_uint32_t       aHash,
                                  const acp_uint8_t* aValue,
                                  acp_uint32_t       aLength );

acp_uint32_t mtcHashSkip( acp_uint32_t       aHash,
                          const acp_uint8_t* aValue,
                          acp_uint32_t       aLength );

acp_uint32_t mtcHashWithoutSpace( acp_uint32_t       aHash,
                                  const acp_uint8_t* aValue,
                                  acp_uint32_t       aLength );

ACI_RC mtcMakeNumeric( mtdNumericType    * aNumeric,
                       acp_uint32_t        aMaximumMantissa,
                       const acp_uint8_t * aString,
                       acp_uint32_t        aLength );

void mtcMakeNumeric2( mtdNumericType       * aNumeric,
                      const mtaTNumericType* aNumber );

ACI_RC mtcNumericCanonize( mtdNumericType * aValue,
                           mtdNumericType * aCanonizedValue,
                           acp_sint32_t     aCanonPrecision,
                           acp_sint32_t     aCanonScale,
                           acp_bool_t     * aCanonized );

ACI_RC mtcMakeSmallint( mtdSmallintType   * aSmallint,
                        const acp_uint8_t * aString,
                        acp_uint32_t        aLength );

ACI_RC mtcMakeInteger( mtdIntegerType    * aInteger,
                       const acp_uint8_t * aString,
                       acp_uint32_t        aLength );

ACI_RC mtcMakeBigint( mtdBigintType     * aBigint,
                      const acp_uint8_t * aString,
                      acp_uint32_t        aLength );

ACI_RC mtcMakeReal( mtdRealType       * aReal,
                    const acp_uint8_t * aString,
                    acp_uint32_t        aLength );

ACI_RC mtcMakeDouble( mtdDoubleType     * aDouble,
                      const acp_uint8_t * aString,
                      acp_uint32_t        aLength );

ACI_RC mtcMakeInterval( mtdIntervalType   * aInterval,
                        const acp_uint8_t * aString,
                        acp_uint32_t        aLength );

ACI_RC mtcMakeBinary( mtdBinaryType     * aBinary,
                      const acp_uint8_t * aString,
                      acp_uint32_t        aLength );

ACI_RC mtcMakeByte( mtdByteType       * aByte,
                    const acp_uint8_t * aString,
                    acp_uint32_t        aLength );

ACI_RC mtcMakeByte2( acp_uint8_t       * aByteValue,
                     const acp_uint8_t * aString,
                     acp_uint32_t        aLength,
                     acp_uint32_t      * aByteLength,
                     acp_bool_t*        aOddSizeFlag);

ACI_RC mtcMakeNibble( mtdNibbleType     * aNibble,
                      const acp_uint8_t * aString,
                      acp_uint32_t        aLength );

ACI_RC mtcMakeNibble2( acp_uint8_t       * aNibbleValue,
                       acp_uint8_t         aOffsetInByte,
                       const acp_uint8_t * aString,
                       acp_uint32_t        aLength,
                       acp_uint32_t      * aNibbleLength );

ACI_RC mtcMakeBit( mtdBitType        * aBit,
                   const acp_uint8_t * aString,
                   acp_uint32_t        aLength );

ACI_RC mtcMakeBit2( acp_uint8_t      * aBitValue,
                    acp_uint8_t        aOffsetInBit,
                    const acp_uint8_t* aString,
                    acp_uint32_t       aLength,
                    acp_uint32_t     * aBitLength );

ACI_RC mtcAddFloat( mtdNumericType *aValue,
                    acp_uint32_t    aPrecisionMaximum,
                    mtdNumericType *aArgument1,
                    mtdNumericType *aArgument2 );

ACI_RC mtcSubtractFloat( mtdNumericType *aValue,
                         acp_uint32_t    aPrecisionMaximum,
                         mtdNumericType *aArgument1,
                         mtdNumericType *aArgument2 );

ACI_RC mtcMultiplyFloat( mtdNumericType *aValue,
                         acp_uint32_t    aPrecisionMaximum,
                         mtdNumericType *aArgument1,
                         mtdNumericType *aArgument2 );

ACI_RC mtcDivideFloat( mtdNumericType *aValue,
                       acp_uint32_t    aPrecisionMaximum,
                       mtdNumericType *aArgument1,
                       mtdNumericType *aArgument2 );

ACI_RC mtcModFloat( mtdNumericType *aValue,
                    acp_uint32_t    aPrecisionMaximum,
                    mtdNumericType *aArgument1,
                    mtdNumericType *aArgument2 );

ACI_RC mtcRoundFloat( mtdNumericType *aValue,
                      mtdNumericType *aArgument1,
                      mtdNumericType *aArgument2 );

ACI_RC mtcNumeric2Slong( acp_sint64_t          *aValue,
                         mtdNumericType *aArgument1 );

// To fix BUG-12944 ��Ȯ�� precision ������.
ACI_RC mtcGetPrecisionScaleFloat( const mtdNumericType * aValue,
                                  acp_sint32_t         * aPrecision,
                                  acp_sint32_t         * aScale );

// BUG-30257 Clinet C ���� ���� Client�� Image �� �Լ�
acp_bool_t mtcIsSamePhysicalImageByModule( const mtdModule* aModule,
                                           const void     * aValue1,
                                           const void     * aValue2);

//----------------------
// mtcColumn�� �ʱ�ȭ
//----------------------

// data module �� language module�� ������ ���
ACI_RC mtcInitializeColumn( mtcColumn       * aColumn,
                            const mtdModule * aModule,
                            acp_uint32_t      aArguments,
                            acp_sint32_t      aPrecision,
                            acp_sint32_t      aScale );

acp_bool_t mtcCompareOneChar( acp_uint8_t * aValue1,
                              acp_uint8_t   aValue1Size,
                              acp_uint8_t * aValue2,
                              acp_uint8_t   aValue2Size );

/* PROJ-2209 DBTIMEZONE */
acp_sint64_t getSystemTimezoneSecond( void );
acp_char_t  *getSystemTimezoneString( acp_char_t * aTimezoneString );

ACP_EXTERN_C_END

#endif /* _O_MTCC_H_ */
