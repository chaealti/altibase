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
 * $Id: mtcfTo_char.c 84839 2019-01-30 09:26:24Z ahra.cho $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcf.h>
#include <mtddlLexer.h>

#define IDL_MIN(a,b)  ( (a) < (b) ? (a) : (b) )

extern mtfModule mtfTo_char;

extern mtdModule mtcdDate;
extern mtdModule mtcdNumeric;
extern mtdModule mtcdFloat;
extern mtdModule mtcdChar;

static ACI_RC compXXXXandRN( acp_uint8_t  * aNumFmt,
                             acp_uint32_t     aNumFmtLen,
                             acp_uint8_t  * aResultValue,
                             acp_uint16_t * aResultLen,
                             acp_sint32_t     aIntNum );

static ACI_RC convertToRoman( acp_sint32_t     aIntNum,
                              acp_uint16_t * aRNCharCnt,
                              acp_char_t  * aTemp );

static ACI_RC convertToString( acp_sint32_t    aLength,
                               acp_sint32_t    aSignExp,
                               acp_uint8_t * aMantissa,
                               acp_char_t * aTemp,
                               acp_sint32_t  * aTempLen );

static ACI_RC applyFormat( acp_char_t       * aString,
                           acp_sint32_t          aStringLen,
                           acp_uint8_t       * aFormat,
                           acp_uint32_t          aFormatLen,
                           acp_uint8_t       * aResult,
                           acp_uint32_t        * aResultLen,
                           acp_uint8_t       * aToken,
                           mtlCurrency * aCurrency,
                           acp_bool_t        aIsMinus );

// EEEE format�� ���� ���, format ����
static ACI_RC applyNonEEEEFormat( acp_char_t       * aString,
                                  acp_sint32_t          aStringLen,
                                  acp_uint8_t       * aResult,
                                  acp_uint32_t        * aResultLen,
                                  acp_uint8_t       * aToken,
                                  mtlCurrency * aCurrency,
                                  acp_bool_t        aIsMinus );

// EEEE format�� ���� ���, format ����
static ACI_RC applyEEEEFormat( acp_char_t       * aString,
                               acp_sint32_t          aStringLen,
                               acp_uint8_t       * aResult,
                               acp_uint32_t        * aResultLen,
                               acp_uint8_t       * aToken,
                               mtlCurrency * aCurrency,
                               acp_bool_t        aIsMinus );

static ACI_RC removeLeadingBlank( acp_char_t  * aResult,
                                  acp_uint32_t     aResultLen,
                                  acp_uint32_t   * aBlankCnt );

ACP_INLINE acp_uint32_t getIntegerLength( acp_uint32_t aInteger )
{
    if ( aInteger < 10 )
    {
        return 1;
    }
    else if ( aInteger < 100 )
    {
        return 2;
    }
    else if ( aInteger < 1000 )
    {
        return 3;
    }
    else if ( aInteger < 10000 )
    {
        return 4;
    }
    else if ( aInteger < 100000 )
    {
        return 5;
    }
    else if ( aInteger < 1000000 )
    {
        return 6;
    }
    else if ( aInteger < 10000000 )
    {
        return 7;
    }
    else if ( aInteger < 100000000 )
    {
        return 8;
    }
    else if ( aInteger < 1000000000 )
    {
        return 9;
    }
    else
    {
        return 10;
    }
}

ACP_INLINE acp_uint32_t mtfFastUInt2Str( acp_char_t * aBuffer,
                                            acp_uint32_t    aValue,
                                            acp_uint32_t    aLength )
{
    acp_char_t *    sBuffer = aBuffer;
    acp_uint32_t    sHighValue;
    acp_uint32_t    sLowValue;

    switch ( aLength )
    {
    case 1:
        *sBuffer++ = '0' + ((aValue = aValue * 268435456) >> 28);
        break;

    case 2:
        *sBuffer++ = '0' + ((aValue = aValue * 26843546) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        break;

    case 3:
        *sBuffer++ = '0' + ((aValue = aValue * 2684355) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        break;

    case 4:
        *sBuffer++ = '0' + ((aValue = aValue * 268436) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        *sBuffer++ = '0' + ((aValue = (aValue & 0x0FFFFFFF) * 10) >> 28);
        break;

    case 5:
        sHighValue = aValue / 10000;
        sLowValue  = aValue % 10000;
        *sBuffer++ = '0' + ((sHighValue = sHighValue * 268435456) >> 28);
        *sBuffer++ = '0' + ((sLowValue = sLowValue * 268436) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        break;

    case 6:
        sHighValue = aValue / 10000;
        sLowValue  = aValue % 10000;
        *sBuffer++ = '0' + ((sHighValue = sHighValue * 26843546) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = sLowValue * 268436) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        break;

    case 7:
        sHighValue = aValue / 10000;
        sLowValue  = aValue % 10000;
        *sBuffer++ = '0' + ((sHighValue = sHighValue * 2684355) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = sLowValue * 268436) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        break;

    case 8:
        sHighValue = aValue / 10000;
        sLowValue  = aValue % 10000;
        *sBuffer++ = '0' + ((sHighValue = sHighValue * 268436) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sHighValue = ((sHighValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = sLowValue * 268436) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        *sBuffer++ = '0' + ((sLowValue = ((sLowValue & 0x0FFFFFFF)) * 10) >> 28);
        break;

    default:
        acpSnprintf( sBuffer, aLength, "%"ACI_INT32_FMT, aValue );
        aLength = acpCStrLen( sBuffer, ACP_UINT32_MAX );
        //aLength = idlOS::snprintf( sBuffer, aLength, "%"ID_UINT32_FMT, aValue );
    }

    return aLength;
}

static
    const acp_char_t* gMONTHName[12] = {
        "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
    };
static
    const acp_char_t* gMonthName[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
static
    const acp_char_t* gmonthName[12] = {
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"
    };

static
    const acp_char_t* gMONName[12] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
static
    const acp_char_t* gMonName[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
static
    const acp_char_t* gmonName[12] = {
        "jan", "feb", "mar", "apr", "may", "jun",
        "jul", "aug", "sep", "oct", "nov", "dec"
    };

static
    const acp_char_t* gDAYName[7] = {
        "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
        "THURSDAY", "FRIDAY", "SATURDAY"
    };
static
    const acp_char_t* gDayName[7] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
static
    const acp_char_t* gdayName[7] = {
        "sunday", "monday", "tuesday", "wednesday",
        "thursday", "friday", "saturday"
    };

static
    const acp_char_t* gDYName[7] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };
static
    const acp_char_t* gDyName[7] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
static
    const acp_char_t* gdyName[7] = {
        "sun", "mon", "tue", "wed", "thu", "fri", "sat"
    };

static
    const acp_char_t* gRMMonth[12] = {
        "I", "II", "III", "IV", "V", "VI",
        "VII", "VIII", "IX", "X", "XI", "XII"
    };
static
    const acp_char_t* grmMonth[12] = {
        "i", "ii", "iii", "iv", "v", "vi",
        "vii", "viii", "ix", "x", "xi", "xii"
    };

static ACI_RC applyNONEFormat( mtdDateType*  aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    acp_sint32_t sLen;
    acp_sint32_t sBufferLen;
    sLen = acpCStrLen((acp_char_t*)aString, ACP_UINT32_MAX);

    ACP_UNUSED(aDate);
    ACP_UNUSED(aIsFillMode);

    if ( sLen < (*aBufferFence)-(*aBufferCur) )
    {
        sBufferLen = sLen;
    } 
    else
    {
        sBufferLen = (*aBufferFence)-(*aBufferCur);
    }

    acpCStrCpy( aBuffer+(*aBufferCur), 
                sBufferLen,
                aString,
                sLen );
    /* BUG-34447 c porting for isql
    idlOS::strncpy( aBuffer+(*aBufferCur), 
                    aString,
                    sBufferLen );
                    */

    (*aBufferCur) += sBufferLen;

    return ACI_SUCCESS;
}

static ACI_RC applyAM_UFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    acp_sint32_t sHour   = mtdDateInterfaceHour( aDate );

    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'A';
        aBuffer[(*aBufferCur)++] = 'M';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'P';
        aBuffer[(*aBufferCur)++] = 'M';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyAM_ULFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    acp_sint32_t sHour = mtdDateInterfaceHour( aDate );

    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'A';
        aBuffer[(*aBufferCur)++] = 'm';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'P';
        aBuffer[(*aBufferCur)++] = 'm';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyAM_LFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    acp_sint32_t sHour = mtdDateInterfaceHour( aDate );

    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'a';
        aBuffer[(*aBufferCur)++] = 'm';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'p';
        aBuffer[(*aBufferCur)++] = 'm';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-36296 SCC Format ���� */
static ACI_RC applySCCFormat( mtdDateType  * aDate,
                              acp_char_t   * aBuffer,
                              acp_sint32_t * aBufferCur,
                              acp_sint32_t * aBufferFence,
                              acp_char_t   * aString,
                              acp_bool_t     aIsFillMode )
{
    acp_sint16_t sYear     = mtdDateInterfaceYear( aDate );
    acp_uint32_t sValue    = 0;
    acp_uint32_t sValueLen = 0;

    ACP_UNUSED(aString);

    ACI_TEST_RAISE( (*aBufferFence) - (*aBufferCur) < 1,
                    ERR_BUFFER_NOT_ENOUGH );

    if ( aIsFillMode == ACP_FALSE )
    {
        if ( sYear <= 0 )
        {
            sValue = ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100; // ceil

            aBuffer[(*aBufferCur)++] = '-';
        }
        else
        {
            sValue = ( ( sYear + 99 ) / 100 ) % 100; // ceil

            aBuffer[(*aBufferCur)++] = ' ';
        }

        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          2 );
    }
    else
    {
        if ( sYear <= 0 )
        {
            sValue = ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100; // ceil

            aBuffer[(*aBufferCur)++] = '-';
        }
        else
        {
            sValue = ( ( sYear + 99 ) / 100 ) % 100; // ceil
        }

        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (*aBufferFence) - (*aBufferCur) < (acp_sint32_t)sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_BUFFER_NOT_ENOUGH )
    {
        aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                         "applySCCFormat",
                         "Buffer not enough" );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyCCFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint16_t sYear  = mtdDateInterfaceYear( aDate );
    acp_uint32_t sValue = 0;
    acp_uint32_t sValueLen;

    if ( sYear <= 0 )
    {
        /* Year 0�� BC -1���̴�. ���밪�� ���ϱ� ���� �����Ѵ�.
         * ������ ��, ��ȣ�� �����Ѵ�. (Oracle)
         */
        sValue = ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100; // ceil
    }
    else
    {
        sValue = ( ( sYear + 99 ) / 100 ) % 100; // ceil
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyDAY_UFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gDAYName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/* BUG-34447 c porting for isql
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gDAYName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */

    return ACI_SUCCESS;
}

static ACI_RC applyDAY_ULFormat( mtdDateType* aDate,
                                 acp_char_t*       aBuffer,
                                 acp_sint32_t*        aBufferCur,
                                 acp_sint32_t*        aBufferFence,
                                 acp_char_t*       aString,
                                 acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gDayName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gDayName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */

    return ACI_SUCCESS;
}
static ACI_RC applyDAY_LFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gdayName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gdayName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyDDDFormat( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint16_t       sYear      = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth     = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay       = mtdDateInterfaceDay( aDate );

    acp_sint32_t         sDayOfYear = mtcDayOfYear( sYear, sMonth, sDay );

    acp_uint32_t         sValue     = sDayOfYear;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 3 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 3 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyDDFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint32_t sDay    = mtdDateInterfaceDay( aDate );

    acp_uint32_t sValue  = sDay;
    acp_uint32_t sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );
        
        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyDY_UFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gDYName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gDYName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyDY_ULFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gDyName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gDyName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyDY_LFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gdyName[mtcDayOfWeek( sYear, sMonth, sDay )] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gdyName[mtc::dayOfWeek( sYear, sMonth, sDay )] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyDFormat( mtdDateType* aDate,
                            acp_char_t*       aBuffer,
                            acp_sint32_t*        aBufferCur,
                            acp_sint32_t*        aBufferFence,
                            acp_char_t*       aString,
                            acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t         sDay = mtdDateInterfaceDay( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 mtcDayOfWeek( sYear, sMonth, sDay ) + 1 );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%1"ID_INT32_FMT,
                                      mtc::dayOfWeek( sYear, sMonth, sDay ) + 1 );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyFFFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 6 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 6 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF1Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro / 100000;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 1 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 1 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );
        
        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF2Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro / 10000;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF3Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro / 1000;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 3 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 3 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF4Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro / 100;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 4 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 4 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF5Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro / 10;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 5 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 5 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyFF6Format( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );
    acp_uint32_t         sValue = sMicro;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 6 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 6 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyHHFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint32_t         sHour = mtdDateInterfaceHour( aDate );

    acp_uint32_t         sValue = sHour;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2);
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyHH12Format( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint32_t         sHour = mtdDateInterfaceHour( aDate );
    acp_uint32_t         sValue = sHour % 12;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        if ( ( sHour == 0 ) || ( sHour == 12 ) )
        {
            ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 12, 2 );
        }
        else
        {
            ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValue, 
                                              2 );
        }
    }
    else
    {
        if ( ( sHour == 0 ) || ( sHour == 12 ) )
        {
            ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 12, 2 );
        }
        else
        {
            sValueLen      = getIntegerLength( sValue );

            ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValue, 
                                              sValueLen );
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyHH24Format( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint32_t         sHour = mtdDateInterfaceHour( aDate );
    acp_uint32_t         sValue = sHour;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyMIFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_sint32_t         sMin = mtdDateInterfaceMinute( aDate );

    acp_uint32_t         sValue = sMin;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyMMFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acp_uint32_t         sValue = sMonth;
    acp_uint32_t         sValueLen;

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyMON_UFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gMONName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/* BUG-34447 c porting for isql
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gMONName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyMON_ULFormat( mtdDateType* aDate,
                                 acp_char_t*       aBuffer,
                                 acp_sint32_t*        aBufferCur,
                                 acp_sint32_t*        aBufferFence,
                                 acp_char_t*       aString,
                                 acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gMonName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gMonName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyMON_LFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gmonName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gmonName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyMONTH_UFormat( mtdDateType* aDate,
                                  acp_char_t*       aBuffer,
                                  acp_sint32_t*        aBufferCur,
                                  acp_sint32_t*        aBufferFence,
                                  acp_char_t*       aString,
                                  acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gMONTHName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gMONTHName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyMONTH_ULFormat( mtdDateType* aDate,
                                   acp_char_t*       aBuffer,
                                   acp_sint32_t*        aBufferCur,
                                   acp_sint32_t*        aBufferFence,
                                   acp_char_t*       aString,
                                   acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gMonthName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gMonthName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyMONTH_LFormat( mtdDateType* aDate,
                                  acp_char_t*       aBuffer,
                                  acp_sint32_t*        aBufferCur,
                                  acp_sint32_t*        aBufferFence,
                                  acp_char_t*       aString,
                                  acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gmonthName[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gmonthName[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyPM_UFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint32_t        sHour = mtdDateInterfaceHour( aDate );

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'A';
        aBuffer[(*aBufferCur)++] = 'M';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'P';
        aBuffer[(*aBufferCur)++] = 'M';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyPM_ULFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint32_t        sHour = mtdDateInterfaceHour( aDate );

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'A';
        aBuffer[(*aBufferCur)++] = 'm';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'P';
        aBuffer[(*aBufferCur)++] = 'm';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyPM_LFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_sint32_t        sHour = mtdDateInterfaceHour( aDate );

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

    if ( sHour < 12 )
    {
        aBuffer[(*aBufferCur)++] = 'a';
        aBuffer[(*aBufferCur)++] = 'm';
    }
    else
    {
        aBuffer[(*aBufferCur)++] = 'p';
        aBuffer[(*aBufferCur)++] = 'm';
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyQFormat( mtdDateType* aDate,
                            acp_char_t*       aBuffer,
                            acp_sint32_t*        aBufferCur,
                            acp_sint32_t*        aBufferFence,
                            acp_char_t*       aString,
                            acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t sMonth = mtdDateInterfaceMonth( aDate );
    acp_uint32_t  sValue = ( sMonth + 2 ) / 3; //ceil( (SDouble) sMonth / 3 );

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 1 );

    (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 1 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyRM_UFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 gRMMonth[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      gRMMonth[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

static ACI_RC applyRM_LFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );

    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 grmMonth[sMonth-1] );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      grmMonth[sMonth-1] );
                                      */
    return ACI_SUCCESS;
}

/* BUG-36296 SYYYY Format ���� */
static ACI_RC applySYYYYFormat( mtdDateType  * aDate,
                                acp_char_t   * aBuffer,
                                acp_sint32_t * aBufferCur,
                                acp_sint32_t * aBufferFence,
                                acp_char_t   * aString,
                                acp_bool_t     aIsFillMode )
{
    acp_sint16_t sYear     = mtdDateInterfaceYear( aDate );
    acp_uint32_t sValue    = 0;
    acp_uint32_t sValueLen = 0;

    ACP_UNUSED(aString);

    ACI_TEST_RAISE( (*aBufferFence) - (*aBufferCur) < 1,
                    ERR_BUFFER_NOT_ENOUGH );

    if ( aIsFillMode == ACP_FALSE )
    {
        if ( sYear < 0 )
        {
            sValue = abs( sYear ) % 10000;

            aBuffer[(*aBufferCur)++] = '-';
        }
        else
        {
            sValue = sYear % 10000;

            aBuffer[(*aBufferCur)++] = ' ';
        }

        ACI_TEST_RAISE( (*aBufferFence) - (*aBufferCur) < 4,
                        ERR_BUFFER_NOT_ENOUGH );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 4 );
    }
    else
    {
        if ( sYear < 0 )
        {
            sValue = abs( sYear ) % 10000;

            aBuffer[(*aBufferCur)++] = '-';
        }
        else
        {
            sValue = sYear % 10000;
        }

        sValueLen      = getIntegerLength( sValue );

        ACI_TEST_RAISE( (*aBufferFence) - (*aBufferCur) < (acp_sint32_t)sValueLen,
                        ERR_BUFFER_NOT_ENOUGH );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_BUFFER_NOT_ENOUGH )
    {
        aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                         "applySYYYYFormat",
                         "Buffer not enough" );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyYYYYFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint32_t       sValue = 0;
    acp_uint32_t       sValueLen;

    ACP_UNUSED(aString);

    if ( sYear < 0 )
    {
        /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
        sValue = abs( sYear ) % 10000;
    }
    else
    {
        sValue = sYear % 10000;
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 4 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 4 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyYYYFormat( mtdDateType* aDate,
                              acp_char_t*       aBuffer,
                              acp_sint32_t*        aBufferCur,
                              acp_sint32_t*        aBufferFence,
                              acp_char_t*       aString,
                              acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint32_t       sValue = 0;
    acp_uint32_t       sValueLen;

    ACP_UNUSED(aString);

    if ( sYear < 0 )
    {
        /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
        sValue = abs( sYear ) % 1000;
    }
    else
    {
        sValue = sYear % 1000;
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 3 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 3 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyYYFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint32_t       sValue = 0;
    acp_uint32_t       sValueLen;

    ACP_UNUSED(aString);

    if ( sYear < 0 )
    {
        /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
        sValue = abs( sYear ) % 100;
    }
    else
    {
        sValue = sYear % 100;
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyYFormat( mtdDateType* aDate,
                            acp_char_t*       aBuffer,
                            acp_sint32_t*        aBufferCur,
                            acp_sint32_t*        aBufferFence,
                            acp_char_t*       aString,
                            acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint32_t       sValue = 0;
    acp_uint32_t       sValueLen;

    ACP_UNUSED(aString);

    if ( sYear < 0 )
    {
        /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
        sValue = abs( sYear ) % 10;
    }
    else
    {
        sValue = sYear % 10;
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 1 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 1 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyRRRRFormat( mtdDateType* aDate,
                               acp_char_t*       aBuffer,
                               acp_sint32_t*        aBufferCur,
                               acp_sint32_t*        aBufferFence,
                               acp_char_t*       aString,
                               acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    return applyYYYYFormat( aDate,
                            aBuffer,
                            aBufferCur,
                            aBufferFence,
                            NULL,
                            aIsFillMode );
}

static ACI_RC applyRRFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aString);

    return applyYYFormat( aDate,
                          aBuffer,
                          aBufferCur,
                          aBufferFence,
                          NULL,
                          aIsFillMode );

    return ACI_SUCCESS;
}

static ACI_RC applySSSSSSSSFormat( mtdDateType* aDate,
                                   acp_char_t*       aBuffer,
                                   acp_sint32_t*        aBufferCur,
                                   acp_sint32_t*        aBufferFence,
                                   acp_char_t*       aString,
                                   acp_bool_t       aIsFillMode )
{
    acp_sint32_t        sSec = mtdDateInterfaceSecond( aDate );
    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );

    acp_uint32_t         sValueSec = sSec;
    acp_uint32_t         sValueMicro = sMicro;
    acp_uint32_t         sValueLen;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < ( 2 + 6 ) );
        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValueSec, 2 );
        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValueMicro, 6 );
    }
    else
    {
        if ( sSec != 0 )
        {
            sValueLen      = getIntegerLength( sValueSec );

            ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < ( sValueLen + 6 ) );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValueSec, 
                                              sValueLen );
            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValueMicro, 
                                              6 );
        }
        else
        {
            sValueLen      = getIntegerLength( sValueMicro );

            ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValueMicro, 
                                              sValueLen );
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applySSSSSSFormat( mtdDateType* aDate,
                                 acp_char_t*       aBuffer,
                                 acp_sint32_t*        aBufferCur,
                                 acp_sint32_t*        aBufferFence,
                                 acp_char_t*       aString,
                                 acp_bool_t       aIsFillMode )
{
    acp_uint32_t         sMicro = mtdDateInterfaceMicroSecond( aDate );

    acp_uint32_t         sValue = sMicro;
    acp_uint32_t         sValueLen;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 6 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 6 );

    }
    else
    {
        sValueLen      = getIntegerLength( sValue );
        
        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applySSSSSFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    acp_sint32_t        sHour = mtdDateInterfaceHour( aDate );
    acp_sint32_t        sMin = mtdDateInterfaceMinute( aDate );
    acp_sint32_t        sSec = mtdDateInterfaceSecond( aDate );

    acp_uint32_t         sValue = ( sHour * 60 * 60 ) + ( sMin * 60 ) + sSec;
    acp_uint32_t         sValueLen;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 5 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 5 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applySSFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    acp_sint32_t        sSec = mtdDateInterfaceSecond( aDate );

    acp_uint32_t         sValue = sSec;
    acp_uint32_t         sValueLen;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyWWFormat( mtdDateType* aDate,
                             acp_char_t*       aBuffer,
                             acp_sint32_t*        aBufferCur,
                             acp_sint32_t*        aBufferFence,
                             acp_char_t*       aString,
                             acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t        sDay = mtdDateInterfaceDay( aDate );

    acp_uint32_t         sValue = mtcWeekOfYear( sYear, sMonth, sDay);
    acp_uint32_t         sValueLen;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen      = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen);

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                          sValue, 
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-42926 TO_CHAR()�� IW �߰� */
static ACI_RC applyIWFormat( mtdDateType  * aDate,
                             acp_char_t   * aBuffer,
                             acp_sint32_t * aBufferCur,
                             acp_sint32_t * aBufferFence,
                             acp_char_t   * aString,
                             acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcWeekOfYearForStandard( sYear, sMonth, sDay );
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-42941 TO_CHAR()�� WW2(Oracle Version WW) �߰� */
static ACI_RC applyWW2Format( mtdDateType  * aDate,
                              acp_char_t   * aBuffer,
                              acp_sint32_t * aBufferCur,
                              acp_sint32_t * aBufferFence,
                              acp_char_t   * aString,
                              acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcWeekOfYearForOracle( sYear, sMonth, sDay );
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyWFormat( mtdDateType*     aDate,
                            acp_char_t*      aBuffer,
                            acp_sint32_t*    aBufferCur,
                            acp_sint32_t*    aBufferFence,
                            acp_char_t*      aString,
                            acp_bool_t       aIsFillMode )
{
    acp_uint32_t       sWeekOfMonth;
    acp_sint16_t       sYear = mtdDateInterfaceYear( aDate );
    acp_uint8_t        sMonth = mtdDateInterfaceMonth( aDate );
    acp_sint32_t       sDay = mtdDateInterfaceDay( aDate );
    acp_double_t       sCeilResult;

    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    ACI_TEST( (*aBufferFence) - (*aBufferCur) < 1 );

    acpMathCeil( ( sDay + mtcDayOfWeek( sYear, sMonth, 1 ) ) / 7,
                 &sCeilResult);
    sWeekOfMonth = (acp_uint32_t) sCeilResult;
    /*
    sWeekOfMonth = (acp_uint32_t) idlOS::ceil( (SDouble) ( sDay +
                                                   mtcDayOfWeek( sYear, sMonth, 1 )
                                                   ) / 7 );
                                                   */

    (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sWeekOfMonth, 1 );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC applyYCYYYFormat( mtdDateType* aDate,
                                acp_char_t*       aBuffer,
                                acp_sint32_t*        aBufferCur,
                                acp_sint32_t*        aBufferFence,
                                acp_char_t*       aString,
                                acp_bool_t       aIsFillMode )
{
    acp_sint16_t       sYear   = mtdDateInterfaceYear( aDate );
    acp_uint32_t       sValue1 = 0;
    acp_uint32_t       sValue2 = 0;
    acp_uint32_t       sValueLen;

    ACP_UNUSED(aString);

    if ( sYear < 0 )
    {
        /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
        sValue1 = ( abs( sYear ) / 1000 ) % 10;
        sValue2 = abs( sYear ) % 1000;
    }
    else
    {
        sValue1 = ( sYear / 1000 ) % 10;
        sValue2 = sYear % 1000;
    }

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < ( 1 + 1 + 3 ) );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue1, 1 );
        aBuffer[(*aBufferCur)++] = ',';
        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue2, 3 );
    }
    else
    {
        if ( sValue1 != 0 )
        {
            sValueLen      = getIntegerLength( sValue1 );

            ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < ( sValueLen + 1 + 3 ) );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValue1, 
                                              sValueLen );

            aBuffer[(*aBufferCur)++] = ',';

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValue2, 
                                              3 );
        }
        else
        {
            sValueLen      = getIntegerLength( sValue2 );

            ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

            (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), 
                                              sValue2, 
                                              sValueLen );
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


//static ACI_RC applyYYCYYFormat( mtdDateType* aDate,
//                                acp_char_t*       aBuffer,
//                                acp_sint16_t*        aBufferCur,
//                                acp_sint16_t*        aBufferFence,
//                                acp_char_t*       /* aString */,
//                                acp_bool_t       aIsFillMode )
//{
//    acp_sint16_t       sYear = mtdDateInterface::year( aDate );
//
//    if( aIsFillMode == ACP_FALSE )
//    {
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%02"ID_INT32_FMT,
//                                             sYear / 100 );
//
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%s",
//                                             "," );
//
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%02"ID_INT32_FMT,
//                                             sYear % 100 );
//    }
//    else
//    {
//        if( (acp_sint16_t)(sYear / 100) != 0 )
//        {
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%"ID_INT32_FMT,
//                                                 sYear / 100 );
//
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%s",
//                                                 "," );
//
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%02"ID_INT32_FMT,
//                                                 sYear % 100 );
//        }
//        else
//        {
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%"ID_INT32_FMT,
//                                                 sYear % 100 );
//        }
//    }
//
//    return ACI_SUCCESS;
//}
//
//static ACI_RC applyYYYCYFormat( mtdDateType* aDate,
//                                acp_char_t*       aBuffer,
//                                acp_sint16_t*        aBufferCur,
//                                acp_sint16_t*        aBufferFence,
//                                acp_char_t*       /* aString */,
//                                acp_bool_t       aIsFillMode )
//{
//    acp_sint16_t       sYear = mtdDateInterface::year( aDate );
//
//    if( aIsFillMode == ACP_FALSE )
//    {
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%03"ID_INT32_FMT,
//                                             sYear / 10 );
//
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%s",
//                                             "," );
//
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%01"ID_INT32_FMT,
//                                             sYear % 10 );
//    }
//    else
//    {
//        if( (acp_sint16_t)(sYear / 10) != 0 )
//        {
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%"ID_INT32_FMT,
//                                                 sYear / 10 );
//
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%s",
//                                                 "," );
//
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%01"ID_INT32_FMT,
//                                                 sYear % 10 );
//        }
//        else
//        {
//            (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                                 (*aBufferFence)-(*aBufferCur),
//                                                 "%"ID_INT32_FMT,
//                                                 sYear % 10 );
//        }
//    }
//
//    return ACI_SUCCESS;
//}
//
//static ACI_RC applyYYYYCFormat( mtdDateType* aDate,
//                                acp_char_t*       aBuffer,
//                                acp_sint16_t*        aBufferCur,
//                                acp_sint16_t*        aBufferFence,
//                                acp_char_t*       /* aString */,
//                                acp_bool_t       aIsFillMode )
//{
//    acp_sint16_t       sYear = mtdDateInterface::year( aDate );
//
//    if( aIsFillMode == ACP_FALSE )
//    {
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%04"ID_INT32_FMT,
//                                             sYear );
//    }
//    else
//    {
//        (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                             (*aBufferFence)-(*aBufferCur),
//                                             "%"ID_INT32_FMT,
//                                             sYear );
//    }
//
//    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
//                                         (*aBufferFence)-(*aBufferCur),
//                                         "%s",
//                                         "," );
//
//    return ACI_SUCCESS;
//}

static ACI_RC applyDOUBLE_QUOTE_STRINGFormat( mtdDateType*     aDate,
                                              acp_char_t*      aBuffer,
                                              acp_sint32_t*    aBufferCur,
                                              acp_sint32_t*    aBufferFence,
                                              acp_char_t*      aString,
                                              acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aDate);
    ACP_UNUSED(aIsFillMode);

    acpSnprintf( aBuffer+(*aBufferCur),
                 (*aBufferFence)-(*aBufferCur),
                 "%s",
                 aString );
    (*aBufferCur) += acpCStrLen( aBuffer+(*aBufferCur), ACP_UINT32_MAX );
/*
    (*aBufferCur) += idlOS::snprintf( aBuffer+(*aBufferCur),
                                      (*aBufferFence)-(*aBufferCur),
                                      "%s",
                                      aString );
                                      */
    return ACI_SUCCESS;
}

// To fix BUG-17693
static ACI_RC applyFMFormat( mtdDateType*     aDate,
                             acp_char_t*      aBuffer,
                             acp_sint32_t*    aBufferCur,
                             acp_sint32_t*    aBufferFence,
                             acp_char_t*      aString,
                             acp_bool_t       aIsFillMode )
{
    ACP_UNUSED(aDate);
    ACP_UNUSED(aBuffer);
    ACP_UNUSED(aBufferCur);
    ACP_UNUSED(aBufferFence);
    ACP_UNUSED(aString);
    ACP_UNUSED(aIsFillMode);

    // Nothing to do

    return ACI_SUCCESS;
}

/* BUG-46727 TO_CHAR()�� IYYY �߰� */
static ACI_RC applyIYYYFormat( mtdDateType  * aDate,
                               acp_char_t   * aBuffer,
                               acp_sint32_t * aBufferCur,
                               acp_sint32_t * aBufferFence,
                               acp_char_t   * aString,
                               acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcYearForStandard( sYear, sMonth, sDay );
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 4 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 4 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-46727 TO_CHAR()�� IYY �߰� */
static ACI_RC applyIYYFormat( mtdDateType  * aDate,
                              acp_char_t   * aBuffer,
                              acp_sint32_t * aBufferCur,
                              acp_sint32_t * aBufferFence,
                              acp_char_t   * aString,
                              acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcYearForStandard( sYear, sMonth, sDay ) % 1000;
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 3 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 3 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-46727 TO_CHAR()�� IY �߰� */
static ACI_RC applyIYFormat( mtdDateType  * aDate,
                             acp_char_t   * aBuffer,
                             acp_sint32_t * aBufferCur,
                             acp_sint32_t * aBufferFence,
                             acp_char_t   * aString,
                             acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcYearForStandard( sYear, sMonth, sDay ) % 100;
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 2 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 2 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* BUG-46727 TO_CHAR()�� I �߰� */
static ACI_RC applyIFormat( mtdDateType  * aDate,
                            acp_char_t   * aBuffer,
                            acp_sint32_t * aBufferCur,
                            acp_sint32_t * aBufferFence,
                            acp_char_t   * aString,
                            acp_bool_t     aIsFillMode )
{
    acp_sint16_t    sYear     = mtdDateInterfaceYear( aDate );
    acp_uint8_t     sMonth    = mtdDateInterfaceMonth( aDate );
    acp_sint32_t    sDay      = mtdDateInterfaceDay( aDate );

    acp_uint32_t    sValue    = mtcYearForStandard( sYear, sMonth, sDay ) % 10;
    acp_uint32_t    sValueLen = 0;

    ACP_UNUSED(aString);

    if ( aIsFillMode == ACP_FALSE )
    {
        ACI_TEST( (*aBufferFence) - (*aBufferCur) < 1 );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur), sValue, 1 );
    }
    else
    {
        sValueLen = getIntegerLength( sValue );

        ACI_TEST( (acp_uint32_t)(*aBufferFence) - (*aBufferCur) < sValueLen );

        (*aBufferCur) += mtfFastUInt2Str( aBuffer + (*aBufferCur),
                                          sValue,
                                          sValueLen );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/*
static mtfFormatModuleFunc applySEPARATORFormat = &applyNONEFormat;
static mtfFormatModuleFunc applyWHITESPACEFormat = &applyNONEFormat;
*/

static mtfFormatModuleFunc gFormatFuncSet[MTC_TO_CHAR_MAX_PRECISION] = { NULL,
    &applyNONEFormat,         &applyAM_UFormat,         &applyAM_ULFormat,
    &applyAM_LFormat,         &applyCCFormat,           &applyDAY_UFormat,
    &applyDAY_ULFormat,       &applyDAY_LFormat,        &applyDDDFormat,
    &applyDDFormat,           &applyDY_UFormat,         &applyDY_ULFormat,
    &applyDY_LFormat,         &applyDFormat,            &applyFFFormat,
    &applyFF1Format,          &applyFF2Format,          &applyFF3Format,
    &applyFF4Format,          &applyFF5Format,          &applyFF6Format,
    &applyHHFormat,           &applyHH12Format,         &applyHH24Format,
    &applyMIFormat,           &applyMMFormat,           &applyMON_UFormat,
    &applyMON_ULFormat,       &applyMON_LFormat,        &applyMONTH_UFormat,
    &applyMONTH_ULFormat,     &applyMONTH_LFormat,      &applyPM_UFormat,
    &applyPM_ULFormat,        &applyPM_LFormat,         &applyQFormat,
    &applyRM_UFormat,         &applyRM_LFormat,         &applyRRRRFormat,
    &applyRRFormat,           &applySSSSSSSSFormat,     &applySSSSSSFormat,
    &applySSSSSFormat,        &applySSFormat,           &applyWWFormat,
    &applyWFormat,            &applyYCYYYFormat,        &applyYYYYFormat,
    &applyYYYFormat,          &applyYYFormat,           &applyYFormat,
    &applyDOUBLE_QUOTE_STRINGFormat, &applyFMFormat,    &applyNONEFormat,//applySEPARATORFormat,
    &applyIWFormat,           &applyWW2Format,          &applySYYYYFormat,
    &applySCCFormat,          &applyIYYYFormat,         &applyIYYFormat,
    &applyIYFormat,           &applyIFormat,            &applyNONEFormat,//applyWHITESPACEFormat,
};

ACI_RC convertToRoman( acp_sint32_t   aIntNum,
                       acp_uint16_t * aRNCharCnt,
                       acp_char_t   * aTemp )
{
/***********************************************************************
 *
 * Description : �θ� ���ڷ� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_char_t* sRNChar = aTemp;

    sRNChar[0] = '\0';
    *aRNCharCnt = 0;

    while ( aIntNum > 0 )
    {
        if ( aIntNum > 999 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "M" );
            (*aRNCharCnt) += 1;
            aIntNum -= 1000;
        }
        else if ( aIntNum > 899 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "CM" );
            (*aRNCharCnt) += 2;
            aIntNum -= 900;
        }
        else if ( aIntNum > 499 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "D" );
            (*aRNCharCnt) += 1;
            aIntNum -= 500;
        }
        else if ( aIntNum > 399 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "CD" );
            (*aRNCharCnt) += 2;
            aIntNum -= 400;
        }
        else if ( aIntNum > 99 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "C" );
            (*aRNCharCnt) += 1;
            aIntNum -= 100;
        }
        else if ( aIntNum > 89 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "XC" );
            (*aRNCharCnt) += 2;
            aIntNum -= 90;
        }
        else if ( aIntNum > 49 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "L" );
            (*aRNCharCnt) += 1;
            aIntNum -= 50;
        }
        else if ( aIntNum > 39 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "XL" );
            (*aRNCharCnt) += 2;
            aIntNum -= 40;
        }
        else if ( aIntNum > 9 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "X" );
            (*aRNCharCnt) += 1;
            aIntNum -= 10;
        }
        else if ( aIntNum == 9 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "IX" );
            (*aRNCharCnt) += 2;
            aIntNum = 0;
        }
        else if ( aIntNum > 4 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "V" );
            (*aRNCharCnt) += 1;
            aIntNum -= 5;
        }
        else if ( aIntNum == 4 )
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "IV" );
            (*aRNCharCnt) += 2;
            aIntNum = 0;
        }
        else /*if ( aIntNum > 0 )*/
        {
            aciVaAppendFormat( sRNChar, 16, "%s", "I" );
            (*aRNCharCnt) += 1;
            aIntNum -= 1;
        }
    }

    return ACI_SUCCESS;
}

ACI_RC convertToString( acp_sint32_t  aLength,
                        acp_sint32_t  aSignExp,
                        acp_uint8_t*  aMantissa,
                        acp_char_t*   aTemp,
                        acp_sint32_t* aTempLen )
{
/***********************************************************************
 *
 * Description : numeric type�� ���ڸ� to_char(number, number_format)
 *               ���� ��ȯ�� ������ string���� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_char_t*  sTemp = aTemp;
    acp_sint32_t sBufferCur = 0;
    // Numeric�� ���ڿ��� ��ȯ�� �� �ʿ��� �ִ� ���� ũ��
    // Scale'�� ������ -84 ~ 128 �̹Ƿ�, '+.' �Ǵ� '-.'�� Scale 128 ���� ���
    acp_sint32_t sBufferFence = 2 + MTD_NUMERIC_SCALE_MAXIMUM + 1;

    acp_bool_t   sIsMinus = ACP_FALSE;
    acp_bool_t   sIsFloat = ACP_FALSE;
    acp_bool_t   sIsPoint = ACP_FALSE;

    acp_sint32_t    sNumber;
    acp_sint32_t    sZeroCount;
    acp_sint32_t    sIterator;
    acp_sint32_t    sIterator2 = 0;
    acp_sint32_t    sCharCnt = 0;

    sTemp[0] = '\0';

    if ( aSignExp > 128 )
    {
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "+" );
    }
    // 0�� ��쿡�� +. ���� ��ȯ�Ѵ�.
    else if ( aSignExp == 128 )
    {
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "+" );
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "." );
        *aTempLen = IDL_MIN( sBufferCur, sBufferFence - 1 );

        return ACI_SUCCESS;
    }
    else
    {
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "-" );
    }

    // ������ ��� ����� ��ȯ
    if ( aSignExp < 128 )
    {
        aSignExp = 128 - aSignExp;
        aSignExp += 128;
        sIsMinus = ACP_TRUE;
    }

    // �Ҽ��� �Ʒ� �κ��� ���� ���� 0 �ٴ� ��ġ�� �ٸ�
    if ( aSignExp - 193 < aLength - 2 )
    {
        sIsFloat = ACP_TRUE;
    }

    for ( sIterator = 0; sIterator < aLength - 1; sIterator++ )
    {
        sNumber = *( aMantissa + sIterator );

        // �����̸�
        if ( sIsMinus == ACP_TRUE )
        {
            sNumber = 99 - sNumber;
        }

        // �Ҽ� �κ��� ���� ���, ���� �ڸ��� ��ġ�� ����
        if ( ( sIsPoint == ACP_FALSE ) && ( sIsFloat == ACP_TRUE ) &&
             ( sIterator > ( aSignExp - 193 ) ) )
        {
            sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "." );
            sIsPoint = ACP_TRUE;

            // �Ҽ��� ��� .������ �ٴ� 0�� ���� ���
            if ( ( ( sIsMinus == ACP_TRUE ) &&
                 ( ( 99 - *aMantissa ) >= 10 ) ) ||
                 ( ( sIsMinus == ACP_FALSE ) &&
                   ( *aMantissa >= 10 ) ) )
            {
                sZeroCount = ( 193 - aSignExp ) * 2 - 2;
            }
            else
            {
                sZeroCount = ( 193 - aSignExp ) * 2 - 1;
            }

            for ( sIterator2 = 0; sIterator2 < sZeroCount; sIterator2++ )
            {
                sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "0" );
            }
        }

        if ( sNumber >= 10 )
        {
             sBufferCur = aciVaAppendFormat( sTemp,
                                 sBufferFence,
                                 "%d",
                                 sNumber / 10 );
            sCharCnt++;

            if ( sNumber % 10 != 0 )
            {
                 sBufferCur = aciVaAppendFormat( sTemp,
                                     sBufferFence,
                                     "%d",
                                     sNumber % 10 );
                sCharCnt++;
            }
            else
            {
                // �Ҽ� �κ��� ���� ���,
                // �� ������ mantissa�� 10�� ����� �� �ǳ��� 0�� ������.
                if ( ( sIterator == ( aLength - 2 ) ) && ( sIsFloat == ACP_TRUE ) )
                {
                    // nothing to do
                }
                else
                {
                     sBufferCur = aciVaAppendFormat( sTemp,
                                         sBufferFence,
                                         "%s",
                                         "0" );
                    sCharCnt++;
                }
            }
        }
        else if ( ( sNumber >= 0 ) && ( sNumber < 10 ) )
        {
            // ù��° mantissa ���� ���ڸ����̸�
            if ( sIterator == 0 )
            {
                sBufferCur = aciVaAppendFormat( sTemp,
                                     sBufferFence,
                                     "%d",
                                     sNumber );
                sCharCnt++;
            }
            // �߰��̳� �� ���� mantissa ���� ���ڸ����̸�
            else
            {
                 sBufferCur = aciVaAppendFormat( sTemp,
                                      sBufferFence,
                                      "%s",
                                      "0" );
                 sBufferCur = aciVaAppendFormat( sTemp,
                                  sBufferFence,
                                  "%d",
                                  sNumber );
                sCharCnt += 2;
            }
        }
        else
        {
            break;
        }
    }

    // ������ ��� �Էµ� �� �̿ܿ� 0�� �߰��� �ٴ� ��� 0�� ���� ���
    if ( sIsFloat == ACP_TRUE )
    {
        sZeroCount = 0;
    }
    else if ( ( aSignExp >= 193 ) && ( sIsFloat == ACP_FALSE ) &&
         ( ( sCharCnt - aLength ) >= ( aSignExp - 193 ) ) )
    {
        sZeroCount = 0;
    }
    else
    {
        sZeroCount = ( ( aSignExp - 193 ) -
                       ( aLength - 2 ) ) * 2;
    }

    for ( sIterator = 0; sIterator < sZeroCount; sIterator++ )
    {
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "0" );
    }

    if ( sIsFloat == ACP_FALSE )
    {
        sBufferCur = aciVaAppendFormat( sTemp, sBufferFence, "%s", "." );
    }
    else
    {
        // nothing to do
    }

    *aTempLen = IDL_MIN( sBufferCur, sBufferFence - 1 );

    return ACI_SUCCESS;
}

ACI_RC mtfToCharInterface_checkFormat( acp_uint8_t* aFmt,
                                       acp_uint32_t aLength,
                                       acp_uint8_t* aToken )
{
/***********************************************************************
 *
 * Description : number format�� üũ�Ѵ�.
 *              �ٸ� �Լ����� ���� number format token�� aToken�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint16_t sCommaCnt  = 0;
    acp_uint16_t sPeriodCnt = 0;
    acp_uint16_t sDollarCnt = 0;
    acp_uint16_t sZeroCnt   = 0;
    acp_uint16_t sNineCnt   = 0;
    acp_uint16_t sBCnt      = 0;
    acp_uint16_t sEEEECnt   = 0;
    acp_uint16_t sMICnt     = 0;
    acp_uint16_t sPRCnt     = 0;
    acp_uint16_t sRNCnt     = 0;
    acp_uint16_t sSCnt      = 0;
    acp_uint16_t sVCnt      = 0;
    acp_uint16_t sXXXXCnt   = 0;
    acp_uint16_t sFMCnt     = 0;
    acp_uint16_t sCCnt      = 0;
    acp_uint16_t sLCnt      = 0;
    acp_uint16_t sGCnt      = 0;
    acp_uint16_t sDCnt      = 0;

    acp_sint32_t  sIntNineCnt   = 0;
    acp_sint32_t  sIntZeroCnt   = 0;
    acp_sint32_t  sFloatNineCnt = 0;
    acp_sint32_t  sFloatZeroCnt = 0;
    acp_sint32_t  sAfterVNineZeroCnt = 0;

    acp_bool_t sIsFirstS      = ACP_FALSE;
    acp_bool_t sIsFirstPeriod = ACP_FALSE;

    acp_uint16_t   sFormatIndex   = 0;
    acp_uint8_t*   sFormat        = aFmt;
    acp_uint32_t   sFormatLeftLen = aLength;  // ó���ϰ� ���� format string�� length
    acp_uint32_t   sFormatLen     = aLength;   // format string�� length

    // To fix BUG-17693,28199
    // 'FM' format�� format string���� ã�´�.
    // (��� ���ڿ��� ���� ���������� �ǹ��̰� �� �տ� �ѹ��ۿ� ���� �� ����.)
    if ( mtcStrCaselessMatch( sFormat, 2, "FM", 2 ) == 0 )
    {
        sFMCnt++;

        // 'FM' format�� ���� sFormat�� �����Ѵ�.
        sFormat += 2;
        sFormatLeftLen -= 2;
        sFormatLen -= 2;
        // sFormatIndex = 0;
    }
    else
    {
        // Nothing to do.
    }
    
    while ( sFormatIndex < sFormatLen )
    {
        if ( sFormatLeftLen >= 4 )
        {
            if ( mtcStrCaselessMatch( sFormat, 4, "EEEE", 4 ) == 0 )
            {
                // eeee format�� ���� ��� 9�� 0�� ������ ���� .�� ������ �ȵ�.
                ACI_TEST_RAISE( ( sEEEECnt != 0 ) || ( sCommaCnt != 0) ||
                                ( sGCnt != 0 )    ||
                                ( sIsFirstPeriod == ACP_TRUE ),
                                ERR_INVALID_LITERAL );

                if ( sFormatLeftLen >= 6 )
                {
                    if ( (mtcStrCaselessMatch( sFormat+4, 2, "MI", 2 ) == 0) ||
                         (mtcStrCaselessMatch( sFormat+4, 2, "PR", 2 ) == 0) )
                    {
                        ACI_TEST_RAISE( sFormatIndex != sFormatLen - 6,
                                        ERR_INVALID_LITERAL );
                    }
                    else
                    {
                        ACI_RAISE( ERR_INVALID_LITERAL );
                    }
                }
                else if ( sFormatLeftLen >= 5 )
                {
                    if ( mtcStrCaselessMatch( sFormat+4, 1, "S", 1 ) == 0 )
                    {
                        ACI_TEST_RAISE( sFormatIndex != sFormatLen - 5,
                                        ERR_INVALID_LITERAL );
                    }
                    else
                    {
                        ACI_RAISE( ERR_INVALID_LITERAL );
                    }
                }
                else
                {
                    ACI_TEST_RAISE( ( sFormatIndex != sFormatLen - 4 ) ||
                                    ( sFormatLen   == 4 ),
                                    ERR_INVALID_LITERAL );
                }

                sEEEECnt++;
                sFormat += 4;
                sFormatLeftLen -= 4;
                sFormatIndex += 3;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 4, "XXXX", 4 ) == 0 )
            {
                ACI_TEST_RAISE( sFormatLen != 4, ERR_INVALID_LITERAL );

                sXXXXCnt++;
                sFormat += 4;
                sFormatLeftLen -= 4;
                sFormatIndex += 3;
                goto break_out;
            }
        }
        if ( sFormatLeftLen >= 2 )
        {
            if ( mtcStrCaselessMatch( sFormat, 2, "MI", 2 ) == 0 )
            {
                ACI_TEST_RAISE( ( sFormatIndex != sFormatLen -2 ) ||
                                ( sSCnt != 0 ) || ( sPRCnt != 0 ) ||
                                ( sMICnt != 0 ),
                                ERR_INVALID_LITERAL );

                sMICnt++;
                sFormat += 2;
                sFormatLeftLen -= 2;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 2, "PR", 2 ) == 0 )
            {
                ACI_TEST_RAISE( ( sFormatIndex != sFormatLen -2 ) ||
                                ( sSCnt != 0 ) || ( sPRCnt != 0 ) ||
                                ( sMICnt != 0 ),
                                ERR_INVALID_LITERAL );
                sPRCnt++;
                sFormat += 2;
                sFormatLeftLen -= 2;
                sFormatIndex++;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 2, "RN", 2 ) == 0 )
            {
                ACI_TEST_RAISE( sFormatLen != 2, ERR_INVALID_LITERAL );

                sRNCnt++;
                sFormat += 2;
                sFormatLeftLen -= 2;
                sFormatIndex++;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 2, "FM", 2 ) == 0 )
            {
                // 'FM' format�� format string �߰��� ���� �� ����.
                ACI_RAISE( ERR_INVALID_LITERAL );
            }
        }
        if ( sFormatLeftLen >= 1 )
        {
            if ( mtcStrMatch( sFormat, 1, ",", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sDCnt != 0 )              ||
                                ( sPeriodCnt != 0 )         ||
                                ( sEEEECnt   != 0 )         ||
                                ( sVCnt      != 0 )         ||
                                ( sGCnt      != 0 )         ||
                                (( sNineCnt + sZeroCnt ) == 0 ),
                                ERR_INVALID_LITERAL );
                sCommaCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrMatch( sFormat, 1, ".", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sDCnt      != 0 )   ||
                                ( sPeriodCnt != 0 )   ||
                                ( sEEEECnt   != 0 )   ||
                                ( sGCnt      != 0 )   ||
                                ( sVCnt      != 0 ),
                                ERR_INVALID_LITERAL );

                if ( (sFormatIndex == sFormatLen - 1) ||
                     (( sNineCnt + sZeroCnt ) == 0) )
                {
                    sIsFirstPeriod = ACP_TRUE;
                }
                sPeriodCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrMatch( sFormat, 1, "$", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sLCnt != 0 )      ||
                                ( sCCnt != 0 )      ||
                                ( sDollarCnt != 0 ),
                                ERR_INVALID_LITERAL );
                sDollarCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrMatch( sFormat, 1, "0", 1 ) == 0 )
            {
                if ( (sPeriodCnt == 0) && (sVCnt == 0) && (sDCnt == 0))
                {
                    sIntZeroCnt++;
                }

                if ( sVCnt == 1 )
                {
                    sAfterVNineZeroCnt++;
                    sFloatZeroCnt++;
                }

                if ( ( sPeriodCnt == 1 ) || ( sDCnt == 1 ) )
                {
                    sFloatZeroCnt++;
                }

                sZeroCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrMatch( sFormat, 1, "9", 1 ) == 0 )
            {
                if ( (sPeriodCnt == 0) && (sVCnt == 0) && (sDCnt == 0) )
                {
                    sIntNineCnt++;
                }

                if ( sVCnt == 1 )
                {
                    sAfterVNineZeroCnt++;
                    sFloatNineCnt++;
                }

                if ( ( sPeriodCnt == 1 ) || ( sDCnt == 1 ) )
                {
                    sFloatNineCnt++;
                }

                sNineCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "B", 1 ) == 0 )
            {
                ACI_TEST_RAISE( sBCnt != 0, ERR_INVALID_LITERAL );

                sBCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "S", 1 ) == 0 )
            {
                if ( sFormatLeftLen == sFormatLen )
                {
                    sIsFirstS = ACP_TRUE;
                }

                ACI_TEST_RAISE( ( sFormatIndex != 0 ) &&
                                ( sFormatIndex != sFormatLen -1 ),
                                ERR_INVALID_LITERAL );
                ACI_TEST_RAISE( ( sSCnt != 0 ) || ( sPRCnt != 0 ) ||
                                ( sMICnt != 0 ),
                                ERR_INVALID_LITERAL );

                sSCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "V", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sPeriodCnt != 0 ) ||
                                ( sDCnt      != 0 ) ||
                                ( sVCnt      != 0 ),
                                ERR_INVALID_LITERAL );

                sVCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "C", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sLCnt != 0 )      ||
                                ( sCCnt != 0 )      ||
                                ( sDollarCnt != 0 ),
                                ERR_INVALID_LITERAL );

                if ( ( sFormatIndex > 0 ) &&
                     ( sFormatIndex < sFormatLen - 1 ))
                {
                    if ( sIsFirstS == ACP_FALSE )
                    {
                        if ( sFormatLeftLen > 3 )
                        {
                            ACI_RAISE( ERR_INVALID_LITERAL );
                        }
                        else if ( sFormatLeftLen == 3 )
                        {
                            if ( (mtcStrCaselessMatch( sFormat + 1, 2,
                                                           "PR", 2 ) != 0 ) &&
                                 ( mtcStrCaselessMatch( sFormat + 1, 2,
                                                           "MI", 2 ) != 0 ) )
                            {
                                ACI_RAISE( ERR_INVALID_LITERAL );
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        } else if ( sFormatLeftLen == 2 )
                        {
                            if ( mtcStrCaselessMatch( sFormat + 1, 1,
                                                          "S", 1 ) != 0 )
                            {
                                ACI_RAISE( ERR_INVALID_LITERAL );
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        }
                        else
                        {
                            ACI_RAISE( ERR_INVALID_LITERAL );
                        }
                    }
                    else
                    {
                        ACI_TEST_RAISE( sFormatIndex != 1, ERR_INVALID_LITERAL );
                    }
                }

                sCCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "L", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sLCnt != 0 )      ||
                                ( sCCnt != 0 )      ||
                                ( sDollarCnt != 0 ),
                                ERR_INVALID_LITERAL );

                if ( ( sFormatIndex > 0 ) &&
                     ( sFormatIndex < sFormatLen - 1 ))
                {
                    if ( sIsFirstS == ACP_FALSE )
                    {
                        if ( sFormatLeftLen > 3 )
                        {
                            ACI_RAISE( ERR_INVALID_LITERAL );
                        }
                        else if ( sFormatLeftLen == 3 )
                        {
                            if ( (mtcStrCaselessMatch( sFormat + 1, 2,
                                                           "PR", 2 ) != 0 ) &&
                                 ( mtcStrCaselessMatch( sFormat + 1, 2,
                                                           "MI", 2 ) != 0 ) )
                            {
                                ACI_RAISE( ERR_INVALID_LITERAL );
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        } else if ( sFormatLeftLen == 2 )
                        {
                            if ( mtcStrCaselessMatch( sFormat + 1, 1,
                                                          "S", 1 ) != 0 )
                            {
                                ACI_RAISE( ERR_INVALID_LITERAL );
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        }
                        else
                        {
                            ACI_RAISE( ERR_INVALID_LITERAL );
                        }
                    }
                    else
                    {
                        ACI_TEST_RAISE( sFormatIndex != 1, ERR_INVALID_LITERAL );
                    }
                }
                sLCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "G", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sDCnt != 0 )              ||
                                ( sPeriodCnt != 0 )         ||
                                ( sEEEECnt   != 0 )         ||
                                ( sVCnt      != 0 )         ||
                                ( sCommaCnt  != 0 )         ||
                                ( sPeriodCnt != 0 )         ||
                                (( sNineCnt + sZeroCnt ) == 0 ),
                                ERR_INVALID_LITERAL );

                sGCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
            if ( mtcStrCaselessMatch( sFormat, 1, "D", 1 ) == 0 )
            {
                ACI_TEST_RAISE( ( sDCnt      != 0 )   ||
                                ( sPeriodCnt != 0 )   ||
                                ( sEEEECnt   != 0 )   ||
                                ( sCommaCnt  != 0 )   ||
                                ( sPeriodCnt != 0 )   ||
                                ( sVCnt      != 0 ),
                                ERR_INVALID_LITERAL );
                if ( (sFormatIndex == sFormatLen - 1) ||
                     (( sNineCnt + sZeroCnt ) == 0) )
                {
                    sIsFirstPeriod = ACP_TRUE;
                }
                sDCnt++;
                sFormat++;
                sFormatLeftLen--;
                goto break_out;
            }
        }
break_out:
        sFormatIndex++;
    }

    // �������� �ʴ� format�� �ִ� ���, ERROR
    ACI_TEST_RAISE( sFormatLeftLen != 0, ERR_INVALID_LITERAL )

    aToken[MTD_NUMBER_FORMAT_FM]     = sFMCnt;
    aToken[MTD_NUMBER_FORMAT_COMMA]  = sCommaCnt;
    aToken[MTD_NUMBER_FORMAT_PERIOD] = sPeriodCnt;
    aToken[MTD_NUMBER_FORMAT_DOLLAR] = sDollarCnt;
    aToken[MTD_NUMBER_FORMAT_ZERO]   = sZeroCnt;
    aToken[MTD_NUMBER_FORMAT_NINE]   = sNineCnt;
    aToken[MTD_NUMBER_FORMAT_B]      = sBCnt;
    aToken[MTD_NUMBER_FORMAT_EEEE]   = sEEEECnt;
    aToken[MTD_NUMBER_FORMAT_MI]     = sMICnt;
    aToken[MTD_NUMBER_FORMAT_PR]     = sPRCnt;
    aToken[MTD_NUMBER_FORMAT_RN]     = sRNCnt;
    aToken[MTD_NUMBER_FORMAT_S]      = sSCnt;
    aToken[MTD_NUMBER_FORMAT_V]      = sVCnt;
    aToken[MTD_NUMBER_FORMAT_XXXX]   = sXXXXCnt;
    aToken[MTD_NUMBER_FORMAT_C]      = sCCnt;
    aToken[MTD_NUMBER_FORMAT_L]      = sLCnt;
    aToken[MTD_NUMBER_FORMAT_G]      = sGCnt;
    aToken[MTD_NUMBER_FORMAT_D]      = sDCnt;
    aToken[MTD_COUNT_NINE]           = sIntNineCnt;
    aToken[MTD_COUNT_ZERO]           = sIntZeroCnt;
    aToken[MTD_COUNT_FLOAT_NINE]     = sFloatNineCnt;
    aToken[MTD_COUNT_FLOAT_ZERO]     = sFloatZeroCnt;
    aToken[MTD_COUNT_VNINE_ZERO]     = sAfterVNineZeroCnt;
    aToken[MTD_COUNT_FIRST_S]        = sIsFirstS;
    aToken[MTD_COUNT_FIRST_PERIOD]   = sIsFirstPeriod;

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL )
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_LITERAL);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC applyFormat( acp_char_t        * aString,
                    acp_sint32_t        aStringLen,
                    acp_uint8_t       * aFormat,
                    acp_uint32_t        aFormatLen,
                    acp_uint8_t       * aResult,
                    acp_uint32_t      * aResultLen,
                    acp_uint8_t       * aToken,
                    mtlCurrency       * aCurrency,
                    acp_bool_t          aIsMinus )
{
/***********************************************************************
 *
 * Description : number format�� ���·� string�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint16_t sDollarCnt         = aToken[MTD_NUMBER_FORMAT_DOLLAR];
    acp_uint16_t sEEEECnt           = aToken[MTD_NUMBER_FORMAT_EEEE];
    acp_uint16_t sMICnt             = aToken[MTD_NUMBER_FORMAT_MI];
    acp_uint16_t sPRCnt             = aToken[MTD_NUMBER_FORMAT_PR];
    acp_uint16_t sSCnt              = aToken[MTD_NUMBER_FORMAT_S];

    acp_sint32_t  sIntNineCnt        = aToken[MTD_COUNT_NINE];
    acp_sint32_t  sIntZeroCnt        = aToken[MTD_COUNT_ZERO];
    acp_sint32_t  sFloatNineCnt      = aToken[MTD_COUNT_FLOAT_NINE];
    acp_sint32_t  sFloatZeroCnt      = aToken[MTD_COUNT_FLOAT_ZERO];

    acp_bool_t sIsPeriod          = ACP_FALSE;
    acp_bool_t sIsFirstS          = (acp_bool_t) aToken[MTD_COUNT_FIRST_S];
    acp_bool_t sIsFloatZero       = ACP_FALSE;

    acp_sint32_t sIntNumCnt           = 0;
    acp_sint32_t sFloatNumCnt         = 0;
    acp_sint32_t sFloatInvalidNumCnt  = 0;

    acp_uint16_t sIterator          = 0;
    acp_uint8_t  sTemp[MTD_NUMBER_FORMAT_BUFFER_LEN];
    acp_uint8_t* sResult            = aResult;
    acp_sint32_t  sResultLen        = 0;
    acp_char_t* sString             = NULL;
    acp_sint32_t  sStringLen        = 0;

    acp_uint32_t   sResultIndex       = 0;
    acp_uint32_t   sFormatIndex       = 0;
    acp_uint32_t   sStringIndex       = 0;

    sString                   = aString;
    sStringLen                = aStringLen;

    acp_uint8_t*   sFormat            = NULL;
    acp_uint32_t   sFormatLen         = 0;

    sFormat                   = aFormat;
    sFormatLen                = aFormatLen;
    sResultLen                = sFormatLen;

    // string���� ���� �κ��� ���� ������ ����.
    for ( sIterator = 0; sIterator < aStringLen; sIterator++ )
    {
        if ( ( *( sString + sIterator ) >= '0' ) &&
             ( *( sString + sIterator ) <= '9' ) )
        {
            if ( sIsPeriod == ACP_FALSE )
            {
                sIntNumCnt++;
            }
            else
            {
                sFloatNumCnt++;
                if ( *( sString + sIterator ) != '0' )
                {
                    sIsFloatZero = ACP_FALSE;
                }
                else
                {
                    if ( sIsFloatZero == ACP_TRUE )
                    {
                        // �����κ��� ���� ��� ��ȿ���ڰ� ������ ���� 0�� ����
                        // 0.00234���� ���, 2��
                        sFloatInvalidNumCnt++;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
        }

        if ( *( sString + sIterator ) == '.' )
        {
            sIsPeriod = ACP_TRUE;
            sIsFloatZero = ACP_TRUE;
        }
    }

    // format string�� result string�� �°� ������Ų��.
    if ( sEEEECnt == 0 )
    {
        // fmt�� ��ȿ����(�Ҽ��� ��)�� �� ������ #���� ä���� return
        if ( sIntNumCnt > ( sIntNineCnt + sIntZeroCnt ) )
        {
            acpMemSet( sResult,
                           '#',
                           aFormatLen + 1 );
            *aResultLen = aFormatLen + 1;

            return ACI_SUCCESS;
        }
    }
    else
    {
        acpMemSet( sResult,
                       ' ',
                       1 );
        sResultIndex++;
        sResultLen++;
    }

    aToken[MTD_COUNT_INTEGER]       = sIntNumCnt;
    aToken[MTD_COUNT_FLOAT]         = sFloatNumCnt;
    aToken[MTD_COUNT_INVALID_FLOAT] = sFloatInvalidNumCnt;

    // format string�� result string�� �°� ������Ų��.
    // ��ȣ ��ȣ�� �ϳ��� ���� ��� �� �տ� �ӽ÷� ���ڸ� �ϳ�
    // �־����.
    if ( ( sSCnt + sMICnt + sPRCnt) == 0 )
    {
        acpMemSet( sResult + sResultIndex,
                       '@',
                       1 );
        sResultIndex++;
        sResultLen++;
    }
    else
    {
        if ( ( sSCnt == 1 ) && ( sIsFirstS == ACP_FALSE ) )
        {
            acpMemSet( sResult + sResultIndex,
                           'O',
                           1 );
            sResultIndex++;
            sResultLen++;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sPRCnt == 1 )
        {
            acpMemCpy( sResult + sResultIndex,
                           sFormat + sFormatLen - 2,
                           1 );
            sResultIndex++;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sMICnt == 1 )
        {
            acpMemCpy( sResult + sResultIndex,
                           sFormat + sFormatLen - 2,
                           1 );
            sResultIndex++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // '$'�� ��ȣ ������ �߰�
    // S�� �Ǿտ� ������ format ���´� ���߿� $��ȣ�� ó���Ѵ�.
    if ( sDollarCnt == 1 )
    {
        if ( sIsFirstS == ACP_FALSE )
        {
            acpMemSet( sResult + sResultIndex,
                           '$',
                           1 );
            sResultIndex++;
        }
        else
        {
            // nothing to do
        }
    }

    for ( ; sFormatIndex < sFormatLen; sFormatIndex++ )
    {
        if ( *( sFormat + sFormatIndex ) == '$' )
        {
            if ( sIsFirstS == ACP_FALSE )
            {
                // Nothing to do
            }
            else
            {
                if ( sFormatIndex == 1 )
                {
                    // S�� ���� ǥ�� ������ �� ���̳� �� �ڿ��� �� �� �ִ�.
                    // $�� ���� �տ��� �� �� �ִ�.
                    // ����, S$�� �����ϴ� ���� �����̴�.
                    acpMemCpy( sResult + sResultIndex,
                                   sFormat + sFormatIndex,
                                   1 );
                    sResultIndex++;
                }
                else
                {
                    // S�� $ ������ format�� $ �ڷ� ������, S$�� �����ϰ� �����.

                    if ( sEEEECnt == 0 )
                    {
                        ACE_ASSERT( sFormatIndex >= 2 );
                        /*
                        IDE_ASSERT_MSG( sFormatIndex >= 2,
                                        "sFormatIndex : %"ACI_UINT32_FMT"\n",
                                        sFormatIndex );
                                        */

                        // ���� ���, 'S999$'�� 'S$999'�� ��ȯ�Ѵ�.
                        acpMemCpy( sTemp, sResult, sFormatIndex );
                        acpMemCpy( sResult, sTemp, 1 );
                        acpMemSet( sResult + 1, '$', 1 );
                        acpMemCpy( sResult + 2, sTemp + 1, sFormatIndex - 1 );
                    }
                    else
                    {
                        ACE_ASSERT( sResultIndex >= 3 );
                        /*
                        IDE_ASSERT_MSG( sResultIndex >= 3,
                                        "sResultIndex : %"ACI_UINT32_FMT"\n",
                                        sResultIndex );
                                        */

                        // EEEE format�� ���� ���, �� ���� ������.
                        // ���� ���, ' S999$'�� ' S$999'�� ��ȯ�Ѵ�.
                        acpMemCpy( sTemp, sResult, sResultIndex );
                        acpMemCpy( sResult, sTemp, 2 );
                        acpMemSet( sResult + 2, '$', 1 );
                        acpMemCpy( sResult + 3, sTemp + 2, sResultIndex - 2 );
                    }
                    sResultIndex++;
                }
            }
        }
        else if ( ( mtcStrCaselessMatch( sFormat + sFormatIndex, 1,
                                             "O", 1 ) == 0 ) ||
                  ( mtcStrCaselessMatch( sFormat + sFormatIndex, 1,
                                             "P", 1 ) == 0 ) ||
                  ( mtcStrCaselessMatch( sFormat + sFormatIndex, 1,
                                             "M", 1 ) == 0 ) )
        {
            // Nothing to do
        }
        else if ( mtcStrCaselessMatch( sFormat + sFormatIndex, 1,
                                           "B", 1 ) == 0 )
        {
            sResultLen--;
        }
        else
        {
            acpMemCpy( sResult + sResultIndex,
                           sFormat + sFormatIndex,
                           1 );
            sResultIndex++;
        }
    }
    sResultIndex = 0;

    if ( sIntNumCnt == 0 )
    {
        if ( ( sFloatNineCnt + sFloatZeroCnt ) == 0 )
        {
            sIntNumCnt = 1;
        }
    }

    aToken[MTD_INDEX_STRING]     = sStringIndex;
    aToken[MTD_INDEX_STRING_LEN] = sStringLen;
    aToken[MTD_INDEX_FORMAT]     = sFormatIndex;
    aToken[MTD_INDEX_FORMAT_LEN] = sFormatLen;
    aToken[MTD_INDEX_RESULT]     = sResultIndex;
    aToken[MTD_INDEX_RESULT_LEN] = sResultLen;

    if ( sEEEECnt == 0 )
    {
        // EEEE format�� ���� ���, format ����
        ACI_TEST( applyNonEEEEFormat( aString,
                                      aStringLen,
                                      aResult,
                                      aResultLen,
                                      aToken,
                                      aCurrency,
                                      aIsMinus )
                  != ACI_SUCCESS );
    }
    else
    {
        // EEEE format�� ���� ���, format ����
        ACI_TEST( applyEEEEFormat( aString,
                                   aStringLen,
                                   aResult,
                                   aResultLen,
                                   aToken,
                                   aCurrency,
                                   aIsMinus )
                  != ACI_SUCCESS );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC applyNonEEEEFormat( acp_char_t       * aString,
                           acp_sint32_t       aStringLen,
                           acp_uint8_t      * aResult,
                           acp_uint32_t     * aResultLen,
                           acp_uint8_t      * aToken,
                           mtlCurrency      * aCurrency,
                           acp_bool_t         aIsMinus )
{
/***********************************************************************
 *
 * Description : EEEE format�� ���� ��� aString�� aFormat�� �°�
                 ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint16_t  sBCnt              = aToken[MTD_NUMBER_FORMAT_B];
    acp_uint16_t  sDCnt              = aToken[MTD_NUMBER_FORMAT_D];

    acp_sint32_t   sIntNineCntTemp    = aToken[MTD_COUNT_NINE];
    acp_sint32_t   sIntZeroCnt        = aToken[MTD_COUNT_ZERO];
    acp_sint32_t   sIntZeroCntTemp    = aToken[MTD_COUNT_ZERO];

    acp_bool_t  sIsStart           = ACP_FALSE;
    acp_bool_t  sIsStartV          = ACP_FALSE;

    acp_sint32_t  sIntNumCnt         = aToken[MTD_COUNT_INTEGER];
    acp_sint32_t  sFloatCnt          = 0;
    acp_uint16_t  sIterator          = 0;
    acp_uint8_t   sTemp[MTD_NUMBER_FORMAT_BUFFER_LEN];
    acp_uint8_t * sResult            = aResult;
    acp_sint32_t  sResultLen         = 0;
    acp_char_t *  sString            = NULL;
    acp_sint32_t  sStringLen         = 0;
    acp_uint8_t * sChar              = NULL;

    acp_sint32_t  sResultIndex       = 0;
    acp_sint32_t  sStringIndex       = 0;
    acp_sint32_t  sTempIndex         = 0;
    acp_sint32_t  sTempLen           = 0;
    acp_sint32_t  sCount             = 0;

    sString                   = aString;
    sStringLen                = aStringLen;

    sStringIndex              = aToken[MTD_INDEX_STRING];
    sStringLen                = aToken[MTD_INDEX_STRING_LEN];
    sResultIndex              = aToken[MTD_INDEX_RESULT];
    sResultLen                = aToken[MTD_INDEX_RESULT_LEN];
    
    sFloatCnt     = aToken[MTD_COUNT_FLOAT_NINE] + aToken[MTD_COUNT_FLOAT_ZERO];

    // str1�� 0�� ���, 0�� ��Ÿ���� ���ؼ� sIntNumCnt�� 1�� �Ѵ�.
    if ( ( sBCnt == 0 ) && ( sStringLen == 2 ) && ( sFloatCnt == 0 ) )
    {
        sIntNumCnt = 1;
    }

    // ���� �κ��� ���� ������ ���� ��, 0�� ������
    // �������� 9�� ,�� �� ������ ������.
    sCount = sIntNineCntTemp + sIntZeroCnt;
    sChar = sResult + sResultIndex;

    while ( sCount >= sIntNumCnt )
    {
        if ( sCount > sIntNumCnt )
        {
            if ( ( *sChar == '0' ) || ( *sChar == '.' ) ||
                 ( mtcStrCaselessMatch( sChar, 1, "D", 1 ) == 0 ) )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( ( *sChar != ',' ) &&
                 ( mtcStrCaselessMatch( sChar, 1, "G", 1 ) != 0 ) )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( ( sIntNineCntTemp == 0 ) &&
             ( *sChar != ',' ) &&
             ( mtcStrCaselessMatch( sChar, 1, "G", 1 ) != 0 ) )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( *sChar == '9' ) || ( *sChar == ',' ) ||
             ( mtcStrCaselessMatch( sChar, 1, "G", 1 ) == 0 ))

        {
            if ( *sChar == '9' )
            {
                sIntNineCntTemp--;
                sCount = sIntNineCntTemp + sIntZeroCnt;
            }
            acpMemCpy( sTemp,
                           sResult,
                           sResultLen );

            acpMemCpy( sResult,
                           sResult + sResultIndex,
                           1 );

            acpMemCpy( sResult + 1,
                           sTemp,
                           sResultIndex );
        }

        if ( sResultIndex < sResultLen - 1 )
        {
            sResultIndex++;
            sChar = sResult + sResultIndex;
        }
        else
        {
            break;
        }
    }

    sResultIndex = 0;
    sStringIndex++;

    while ( sResultIndex < sResultLen )
    {
        if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "@", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "S", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "O", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "P", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "M", 1 ) == 0 ) )
        {
            sIsStart = ACP_TRUE;
        }

        if ( ( sStringLen == 0 ) &&
             ( ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                          "@", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "S", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "O", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "P", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "M", 1 ) == 0 ) ) )
        {
            sIsStart = ACP_TRUE;
        }

        if ( sIsStart == ACP_FALSE )
        {
            acpMemSet( sResult + sResultIndex,
                           ' ',
                           1 );
        }
        else
        {
            if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                            "@", 1 ) == 0 ) )
            {
                if ( ( sBCnt == 0 ) || ( sStringLen != 2 ) )
                {
                    if ( *aString == '+' )
                    {
                        if ( aIsMinus == ACP_TRUE )
                        {
                            acpMemSet( sResult + sResultIndex,
                                           '-',
                                           1 );
                        }
                        else
                        {
                            acpMemSet( sResult + sResultIndex,
                                           ' ',
                                           1 );
                        }
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '-',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "S", 1 ) == 0 )
            {
                if ( ( sBCnt == 0 ) || ( sStringLen != 2 ) )
                {
                    if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '+',
                                       1 );
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '-',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "P", 1 ) == 0 )
            {
                if ( ( sBCnt == 0 ) || ( sStringLen != 2 ) )
                {
                    if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '<',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "R", 1 ) == 0 )
            {
                if ( ( sBCnt == 0 ) || ( sStringLen != 2 ) )
                {
                    if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '>',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "I", 1 ) == 0 )
            {
                if ( ( sBCnt == 0 ) || ( sStringLen != 2 ) )
                {
                    if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '-',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "O", 1 ) == 0 ) ||
                      ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "M", 1 ) == 0 ) )
            {
                for ( sIterator = 0;
                      sIterator < ( sResultLen - sResultIndex - 1 );
                      sIterator++ )
                {
                    acpMemCpy( sResult + sResultIndex + sIterator,
                                   sResult + sResultIndex + sIterator + 1,
                                   1 );
                }
                sResultLen--;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "V", 1 ) == 0 ) )
            {
                sIsStartV = ACP_TRUE;

                for ( sIterator = 0;
                      sIterator < ( sResultLen - sResultIndex - 1 );
                      sIterator++ )
                {
                    acpMemCpy( sResult + sResultIndex + sIterator,
                                   sResult + sResultIndex + sIterator + 1,
                                   1 );
                }
                sResultLen--;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "C", 1 ) == 0 ) )
            {
                acpMemCpy( sTemp, sResult, sResultLen );
                acpMemCpy( sResult + sResultIndex,
                               aCurrency->C,
                               3 );
                sTempIndex = sResultIndex + 1;
                sResultIndex += 3;

                if ( sResultLen > sTempIndex )
                {
                    acpMemCpy( sResult + sResultIndex,
                                   sTemp + sTempIndex,
                                   sResultLen - sTempIndex );
                }
                else
                {
                    /* Nothing to do */
                }
                sResultLen += 2 ;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "L", 1 ) == 0 ) )
            {
                sTempLen = acpCStrLen( aCurrency->L, ACP_UINT32_MAX );
                acpMemCpy( sTemp, sResult, sResultLen );
                acpMemCpy( sResult + sResultIndex,
                               aCurrency->L,
                               sTempLen );
                sTempIndex = sResultIndex + 1;
                sResultIndex += sTempLen;

                if ( sResultLen > sTempIndex )
                {
                    acpMemCpy( sResult + sResultIndex,
                                   sTemp + sTempIndex,
                                   sResultLen - sTempIndex );
                }
                else
                {
                    /* Nothing to do */
                }
                sResultLen += sTempLen - 1 ;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "G", 1 ) == 0 ) )
            {
                acpMemCpy( sResult + sResultIndex,
                               &aCurrency->G,
                               1 );
            }
            else if ( ( sIsStartV == ACP_TRUE ) &&
                      ( ( *( sResult + sResultIndex ) == '9' ) ||
                        ( *( sResult + sResultIndex ) == '0' ) ) )
            {
                if ( ( sBCnt == 1 ) && ( sStringLen == 2 ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
                else
                {
                    if ( *( sString + sStringIndex ) == '.' )
                    {
                        sStringIndex++;
                        continue;
                    }

                    if ( sStringIndex >= aStringLen )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '0',
                                       1 );
                    }
                    else
                    {
                        acpMemCpy( sResult + sResultIndex,
                                       sString + sStringIndex,
                                       1 );
                        sStringIndex++;
                    }
                }
            }
            else if ( ( *( sResult + sResultIndex ) == '9' ) ||
                      ( *( sResult + sResultIndex ) == '0' ) ||
                      ( *( sResult + sResultIndex ) == '.' ) ||
                       ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                  "D", 1 ) == 0 ) )
            {
                // B format�� �ְ�, ���ڰ� 0�� ���
                if ( ( sBCnt == 1 ) && ( sStringLen == 2 ) )
                {
                    if ((((*( sString + sStringIndex ) == '.' ) &&
                          (*( sResult + sResultIndex ) == '.' ))) ||
                         ((*( sString + sStringIndex ) == '.') &&
                           ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                   "D", 1 ) == 0 ) ))
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                        sStringIndex++;
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                    }
                }
                else if ( ( sBCnt == 0 ) && ( sStringLen == 2 ) )
                {
                    if ((((*( sString + sStringIndex ) == '.' )&&
                          (*( sResult + sResultIndex ) == '.' ))) ||
                         ((*( sString + sStringIndex ) == '.') &&
                           ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                   "D", 1 ) == 0 ) ))
                    {
                        if ( sDCnt == 0 )
                        {
                            acpMemSet( sResult + sResultIndex, '.', 1 );
                        }
                        else
                        {
                            acpMemCpy( sResult + sResultIndex,
                                           &aCurrency->D, 1 );
                        }
                        sStringIndex++;
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '0',
                                       1 );
                    }
                }
                else if ( sStringIndex >= aStringLen )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '0',
                                   1 );
                }
                else if ( ( (*( sResult + sResultIndex ) == '0') ||
                            (*( sResult + sResultIndex ) == '9' )) &&
                          ( ( sIntZeroCntTemp + sIntNineCntTemp ) > sIntNumCnt ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '0',
                                   1 );
                    sIntZeroCntTemp--;
                }
                else
                {
                    if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                   "D", 1 ) == 0 )
                    {
                        acpMemCpy( sResult + sResultIndex,
                                       &aCurrency->D,
                                       1 );
                    }
                    else
                    {
                        acpMemCpy( sResult + sResultIndex,
                                       sString + sStringIndex,
                                       1 );
                    }
                    sStringIndex++;
                }
            }
            else
            {
                // nothing to do
            }
        }
        sResultIndex++;
    }
    *aResultLen = sResultLen;

    return ACI_SUCCESS;
}

ACI_RC applyEEEEFormat( acp_char_t       * aString,
                        acp_sint32_t       aStringLen,
                        acp_uint8_t      * aResult,
                        acp_uint32_t     * aResultLen,
                        acp_uint8_t      * aToken,
                        mtlCurrency      * aCurrency,
                        acp_bool_t         aIsMinus )
{
/***********************************************************************
 *
 * Description : EEEE format�� �ִ� ��� aString�� aFormat�� �°�
                 ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_uint16_t sBCnt = aToken[MTD_NUMBER_FORMAT_B];

    acp_sint32_t  sIntNineCnt        = aToken[MTD_COUNT_NINE];
    acp_sint32_t  sIntZeroCnt        = aToken[MTD_COUNT_ZERO];
    acp_sint32_t  sAfterVNineZeroCnt = aToken[MTD_COUNT_VNINE_ZERO];

    acp_bool_t sIsStart = ACP_FALSE;
    acp_bool_t sIsStartV = ACP_FALSE;

    acp_sint32_t  sIntNumCnt          = aToken[MTD_COUNT_INTEGER];
    acp_sint32_t  sFloatInvalidNumCnt = aToken[MTD_COUNT_INVALID_FLOAT];

    acp_sint32_t  sExp = 0;
    acp_uint16_t sIterator = 0;
    acp_uint16_t sZeroIterator = 0;
    acp_uint8_t  sTemp[MTD_NUMBER_FORMAT_BUFFER_LEN];
    acp_uint8_t* sResult = aResult;
    acp_sint32_t  sResultLen = 0;
    acp_char_t* sString = NULL;
    acp_sint32_t  sStringLen;

    acp_sint32_t  sResultIndex = 0;
    acp_sint32_t  sStringIndex = 0;
    acp_sint32_t  sTempIndex   = 0;
    acp_sint32_t  sTempLen     = 0;

    sString = aString;
    sStringLen = aStringLen;

    sStringIndex = aToken[MTD_INDEX_STRING];
    sStringLen   = aToken[MTD_INDEX_STRING_LEN];
    sResultIndex = aToken[MTD_INDEX_RESULT];
    sResultLen   = aToken[MTD_INDEX_RESULT_LEN];

    sZeroIterator = sAfterVNineZeroCnt;

    // str1�� 0�� ���, 0�� ��Ÿ���� ���ؼ� sIntNumCnt�� 1�� �Ѵ�.
    if ( sStringLen == 2 )
    {
        sIntNumCnt = 1;
    }

    // ���� ǥ��� ��Ÿ�� ��, ������ ����Ѵ�.
    // 0�� �ƴϰ�, 0~1 ������ �Ҽ��� ��
    if ( ( sStringLen != 2 ) && ( *( sString + 1 ) == '.' ) )
    {
        sExp = sFloatInvalidNumCnt * (-1) - 1;
    }
    else
    {
        sExp = sIntNumCnt - 1;
    }

    // EEEE format�� ���� ���, V���� 9 �Ǵ� 0�� �������� ���
    // �ϳ��� ���ܵΰ� �����Ѵ�.
    acpMemCpy( sTemp,
                   sResult,
                   sResultLen );

    acpMemCpy( sResult,
                   sTemp,
                   3 );

    acpMemCpy( sResult + 3,
                   sTemp + sIntNineCnt + sIntZeroCnt + 2,
                   sResultLen - sIntNineCnt - sIntZeroCnt - 2 );

    sResultLen = sResultLen - sIntNineCnt - sIntZeroCnt + 1;

    sResultIndex = 0;
    sStringIndex++;

    while ( ( sStringIndex <= sStringLen ) &&
            ( ( *( sString + sStringIndex ) < '1' ) ||
              ( *( sString + sStringIndex ) > '9' ) ) )
    {
        sStringIndex++;
    }

    // B�� �ְ�, str1�� 0�� ���� sResultLen ��ŭ�� ������ ����Ѵ�.
    // loop���� ����.
    while ( ( sResultIndex < sResultLen ) &&
            ( ( sBCnt == 0 ) || ( sStringLen != 2 ) ) )
    {
        if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "@", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "S", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "O", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "P", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                        "M", 1 ) == 0 ) )
        {
            sIsStart = ACP_TRUE;
        }

        if ( ( sStringLen == 0 ) &&
             ( ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                          "@", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "S", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "O", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "P", 1 ) == 0 ) ||
             ( mtcStrCaselessMatch( sResult + sResultIndex + 1, 1,
                                        "M", 1 ) == 0 ) ) )
        {
            sIsStart = ACP_TRUE;
        }

        if ( sIsStart == ACP_FALSE )
        {
            acpMemSet( sResult + sResultIndex,
                           ' ',
                           1 );
        }
        else
        {
            if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                            "@", 1 ) == 0 ) )
            {
                if ( *aString == '+' )
                {
                    if ( aIsMinus == ACP_TRUE )
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '-',
                                       1 );
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       ' ',
                                       1 );
                    }
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   '-',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "S", 1 ) == 0 )
            {
                if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '+',
                                   1 );
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   '-',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "P", 1 ) == 0 )
            {
                if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   '<',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "R", 1 ) == 0 )
            {
                if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
                else
                {

                    acpMemSet( sResult + sResultIndex,
                                   '>',
                                   1 );
                }
            }
            else if ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "I", 1 ) == 0 )
            {
                if ( ( *aString == '+' ) && ( aIsMinus == ACP_FALSE ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   ' ',
                                   1 );
                }
                else
                {
                    acpMemSet( sResult + sResultIndex,
                                   '-',
                                   1 );
                }
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "O", 1 ) == 0 ) ||
                      ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                               "M", 1 ) == 0 ) )
            {
                // L, M ����
                for ( sIterator = 0;
                      sIterator < ( sResultLen - sResultIndex - 1 );
                      sIterator++ )
                {
                    acpMemCpy( sResult + sResultIndex + sIterator,
                                   sResult + sResultIndex + sIterator + 1,
                                   1 );
                }
                sResultLen--;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "V", 1 ) == 0 ) )
            {
                sIsStartV = ACP_TRUE;

                // V����
                for ( sIterator = 0;
                      sIterator < ( sResultLen - sResultIndex - 1 );
                      sIterator++ )
                {
                    acpMemCpy( sResult + sResultIndex + sIterator,
                                   sResult + sResultIndex + sIterator + 1,
                                   1 );
                }
                sResultLen--;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "C", 1 ) == 0 ) )
            {
                acpMemCpy( sTemp, sResult, sResultLen );
                acpMemCpy( sResult + sResultIndex,
                               aCurrency->C,
                               3 );
                sTempIndex = sResultIndex + 1;
                sResultIndex += 3;
                if ( sResultLen > sTempIndex )
                {
                    acpMemCpy( sResult + sResultIndex,
                                   sTemp + sTempIndex,
                                   sResultLen - sTempIndex );
                }
                else
                {
                    /* Nothing to do */
                }
                sResultLen += 2 ;
                continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "L", 1 ) == 0 ) )
            {
                 sTempLen = acpCStrLen( aCurrency->L, ACP_UINT32_MAX );
                 acpMemCpy( sTemp, sResult, sResultLen );
                 acpMemCpy( sResult + sResultIndex,
                                aCurrency->L,
                                sTempLen );
                 sTempIndex = sResultIndex + 1;
                 sResultIndex += sTempLen;

                 if ( sResultLen > sTempIndex )
                 {
                     acpMemCpy( sResult + sResultIndex,
                                    sTemp + sTempIndex,
                                    sResultLen - sTempIndex );
                 }
                 else
                 {
                     /* Nothing to do */
                 }
                 sResultLen += sTempLen - 1 ;
                 continue;
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                 "D", 1 ) == 0 ) )
            {
                acpMemCpy( sResult + sResultIndex,
                               &aCurrency->D, 1 );
            }
            else if ( ( sIsStartV == ACP_TRUE ) &&
                      ( ( *( sResult + sResultIndex ) == '9' ) ||
                        ( *( sResult + sResultIndex ) == '0' ) ) )
            {
                if ( sStringLen - sStringIndex >= 1 )
                {
                    if ( ( *( sString + sStringIndex) == '.' ) ||
                         ( mtcStrCaselessMatch( sResult + sResultIndex, 1,
                                                    "D", 1 ) == 0 ) )
                    {
                        sStringIndex++;
                    }
                }

                // ��Ÿ�� �� �ִ� ��ȿ������ ������ �ʰ��� ���
                // �ʰ��� V�� ������ŭ 0�� �߰��Ѵ�.
                // �ʰ����� ���� �������� string�� ���ڸ� �״�� �����´�.
                if ( ( sIntNumCnt != 0 ) &&
                     ( sZeroIterator <= ( sAfterVNineZeroCnt - ( aStringLen - 2 ) + 1 ) ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '0',
                                   1 );
                    sZeroIterator--;
                    sStringIndex++;
                }
                else if ( sIntNumCnt == 0 )
                {
                    if ( ( sZeroIterator > 0 ) &&
                         ( sStringIndex < aStringLen ) )

                    {
                        acpMemCpy( sResult + sResultIndex,
                                       sString + sStringIndex,
                                       1 );
                        sZeroIterator--;
                        sStringIndex++;
                    }
                    else
                    {
                        acpMemSet( sResult + sResultIndex,
                                       '0',
                                       1 );
                        sZeroIterator--;
                        sStringIndex++;
                    }
                }
                else
                {
                    acpMemCpy( sResult + sResultIndex,
                                   sString + sStringIndex,
                                   1 );
                    sZeroIterator--;
                    sStringIndex++;
                }
            }
            else if ( ( mtcStrCaselessMatch( sResult + sResultIndex, 4,
                                                 "EEEE", 4 ) == 0 ) )
            {
                acpMemSet( sResult + sResultIndex,
                               'E',
                               1 );

                if ( sExp >= 0 )
                {
                    acpMemSet( sResult + sResultIndex + 1,
                                   '+',
                                   1 );
                }
                else
                {
                    acpMemSet( sResult + sResultIndex + 1,
                                   '-',
                                   1 );
                    sExp = sExp * (-1);
                }

                // exponent�� �� �ڸ��� ���
                // result string�� ũ�⸦ �ϳ� �÷��� �Ѵ�.
                if ( sExp > 99 )
                {
                    sResultIndex += 2;

                    acpMemCpy( sTemp,
                                   sResult,
                                   sResultLen );

                    acpMemCpy( sResult,
                                   sTemp,
                                   sResultIndex );

                    acpMemSet( sResult + sResultIndex,
                                   'E',
                                   1 );

                    acpMemCpy( sResult + sResultIndex + 1,
                                   sTemp + sResultIndex,
                                   sResultLen - sResultIndex );

                    sResultLen++;

                    acpMemSet( sResult + sResultIndex,
                                   ( sExp / 100 ) + '0',
                                   1 );

                    acpMemSet( sResult + sResultIndex + 1,
                                   ( sExp / 10 ) - ( ( sExp / 100 ) * 10 ) + '0',
                                   1 );

                    acpMemSet( sResult + sResultIndex + 2,
                                   ( sExp % 10 ) + '0',
                                   1 );
                    sResultIndex += 2;
                }
                else
                {
                    acpMemSet( sResult + sResultIndex + 2,
                                   sExp / 10 + '0',
                                   1 );

                    acpMemSet( sResult + sResultIndex + 3,
                                   sExp % 10 + '0',
                                   1 );
                    sResultIndex += 3;
                }
            }
            else if ( ( *( sResult + sResultIndex ) == '9' ) ||
                      ( *( sResult + sResultIndex ) == '0' ) )
            {
                if ( ( sBCnt == 0 ) && ( sStringLen == 2 ) )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '0',
                                   1 );
                }
                else if ( sStringIndex >= aStringLen )
                {
                    acpMemSet( sResult + sResultIndex,
                                   '0',
                                   1 );
                }
                else
                {

                    if ( ( *( sString + sStringIndex) == '.' ) ||
                         ( mtcStrCaselessMatch( sString + sStringIndex, 1,
                                                    "D", 1 ) == 0 ) )
                    {
                        if ( sStringIndex != aStringLen )
                        {
                            sStringIndex++;
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

                    acpMemCpy( sResult + sResultIndex,
                                   sString + sStringIndex,
                                   1 );
                    sStringIndex++;
                }
            }
            else
            {
                // nothing to do
            }
        }
        sResultIndex++;
    }

    if ( ( sBCnt == 1 ) && ( sStringLen == 2 ) )
    {
        acpMemSet( sResult,
                       ' ',
                       sResultLen );
    }
    *aResultLen = sResultLen;

    return ACI_SUCCESS;
}

ACI_RC compXXXXandRN( acp_uint8_t*   aNumFmt,
                      acp_uint32_t   aNumFmtLen,
                      acp_uint8_t*   aResultValue,
                      acp_uint16_t*  aResultLen,
                      acp_sint32_t   aIntNum )
{
/***********************************************************************
 *
 * Description : compXXXXandRN
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_uint16_t         sRNCharCnt = 0;
    acp_char_t           sRnTemp[MTD_NUMBER_FORMAT_BUFFER_LEN];
    acp_sint32_t         sIterator;
    acp_uint8_t*         sNumFmt = aNumFmt;
    acp_uint32_t         sNumFmtLen = aNumFmtLen;
    acp_sint32_t         sIntNum = aIntNum;
    acp_sint32_t         sIndex = 0;
    acp_uint8_t          sChr = 0;
    acp_uint8_t          sIsEmpty = 1;

    if ( sNumFmtLen >= 4 )
    {
        if ( mtcStrMatch( sNumFmt, 4, "XXXX", 4 ) == 0 )
        {
            for ( sIterator = 0 ; sIterator < 8 ; sIterator++ )
            {
                sChr = ( ( sIntNum << (sIterator*4)) >> 28 ) & 0xF;

                if ( ( sChr != 0 ) || ( sIsEmpty == 0 ) )
                {
                    sIsEmpty = 0;
                    if ( sChr < 10 )
                    {
                        aResultValue[sIndex] = sChr + '0';
                    }
                    else
                    {
                        aResultValue[sIndex] = sChr + 'A' - 10;
                    }
                    sIndex++;
                }
            }
            *aResultLen = sIndex;
        }
        else if ( mtcStrCaselessMatch( sNumFmt, 4, "XXXX", 4 ) == 0 )
        {
            for ( sIterator = 0 ; sIterator < 8 ; sIterator++ )
            {
                sChr = ( ( sIntNum << (sIterator*4)) >> 28 ) & 0xF;

                if ( ( sChr != 0 ) || ( sIsEmpty == 0 ) )
                {
                    sIsEmpty = 0;
                    if ( sChr < 10 )
                    {
                        aResultValue[sIndex] = sChr + '0';
                    }
                    else
                    {
                        aResultValue[sIndex] = sChr + 'a' - 10;
                    }
                    sIndex++;
                }
            }
            *aResultLen = sIndex;
        }
    }
    else if ( sNumFmtLen >= 2 )
    {
        if ( mtcStrMatch( sNumFmt, 2, "RN", 2 ) == 0 )
        {
            if ( ( sIntNum > 3999 ) || ( sIntNum <= 0 ) )
            {
                ACI_RAISE ( ERR_INVALID_LENGTH );
            }

            ACI_TEST( convertToRoman( sIntNum, &sRNCharCnt, sRnTemp )
                      != ACI_SUCCESS );

            acpMemCpy( aResultValue,
                           sRnTemp,
                           sRNCharCnt );

            *aResultLen = sRNCharCnt;
        }
        else if ( mtcStrCaselessMatch( sNumFmt, 2, "RN", 2 ) == 0 )
        {
            if ( ( sIntNum > 3999 ) || ( sIntNum <= 0 ) )
            {
                ACI_RAISE ( ERR_INVALID_LENGTH );
            }

            ACI_TEST( convertToRoman( sIntNum, &sRNCharCnt, sRnTemp )
                      != ACI_SUCCESS );

            for ( sIterator = 0; sIterator < sRNCharCnt; sIterator++ )
            {
                if ( ( *( sRnTemp + sIterator ) >= 'A' ) &&
                     ( *( sRnTemp + sIterator ) <= 'Z' ) )
                {
                    // �빮���� ���, �ҹ��ڷ� ��ȯ�Ͽ� ����� ����
                    *( sRnTemp + sIterator ) = *( sRnTemp + sIterator ) + 0x20;
                }
                else
                {
                    *( sRnTemp + sIterator ) = *( sRnTemp + sIterator );
                }
            }

            acpMemCpy( aResultValue,
                           sRnTemp,
                           sRNCharCnt );

            *aResultLen = sRNCharCnt;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtfToCharInterfaceMakeFormatInfo( mtcNode*     aNode,
                                         mtcTemplate* aTemplate,
                                         acp_uint8_t* aFormat,
                                         acp_uint32_t aFormatLen,
                                         mtcCallBack* aCallBack )
{
    mtcExecute*         sExecute = NULL;
    acp_sint32_t        sToken = -1;
    acp_char_t*         sString = NULL;
    acp_uint32_t        sStringLen;
    mtdFormatInfo*      sFormatInfo = NULL;
    yyscan_t            sScanner;
    acp_bool_t          sInitScanner = ACP_FALSE;
    acp_bool_t          sIsFillMode = ACP_FALSE;
    acp_uint8_t        *sFormat = NULL;  //  ���� ����� ���� ���� ������ ��ġ�� ������
    acp_uint32_t        sFormatLen;
    acp_uint8_t        *sErrorFormat = NULL;

    sExecute = &(aTemplate->rows[aNode->table].execute[aNode->column]);

    // calculateInfo�� format������ (sFormatInfo) ������ ������ �Ҵ�
    ACI_TEST(aCallBack->alloc( aCallBack->info,
                               sizeof(mtdFormatInfo),
                               (void**)&( sExecute->calculateInfo ) )
                               != ACI_SUCCESS );

    sFormatInfo = (mtdFormatInfo*)(sExecute->calculateInfo);
    sFormatInfo->count = 0;

    // calculateInfo�� �ʿ��� format�� ������ŭ ������ �Ҵ�
    // format�� ������ �ִ� aFormatLen�̴�.
    ACI_TEST(aCallBack->alloc( aCallBack->info,
                               sizeof(mtfTo_charCalcInfo) * aFormatLen,
                               (void**)&( sExecute->calculateInfo ) )
                               != ACI_SUCCESS );

    sFormatInfo->format = (mtfTo_charCalcInfo*) (sExecute->calculateInfo);

    ACI_TEST_RAISE( mtddllex_init ( &sScanner ) != 0, ERR_LEX_INIT_FAILED );
    sInitScanner = ACP_TRUE;

    sFormat = aFormat;
    mtddl_scan_bytes ( (const acp_char_t*)aFormat, aFormatLen, sScanner );

    // get first token
    sToken = mtddllex( sScanner );
    while ( sToken != 0 )
    {
        sFormatInfo->format[sFormatInfo->count].applyDateFormat =
            gFormatFuncSet[sToken];

        // To fix BUG-17693
        sFormatInfo->format[sFormatInfo->count].isFillMode = ACP_FALSE;

        if ( sToken == MTD_DATE_FORMAT_SEPARATOR )
        {
            // Get string length
            sStringLen = mtddlget_leng( sScanner );
            
            // calculateInfo�� format������ (sFormatInfo) ������ ������ �Ҵ�
            // SEPARATOR Ȥ�� ������ ũ�� ��ŭ �Ҵ�
            ACI_TEST(aCallBack->alloc( aCallBack->info,
                                       sStringLen + 1,
                                       (void**)&( sExecute->calculateInfo ) )
                                       != ACI_SUCCESS );

            sFormatInfo->format[sFormatInfo->count].string =
                (acp_char_t*) sExecute->calculateInfo;
            sString = sFormatInfo->format[sFormatInfo->count].string;

            // MAX_PRECISION�� �ʰ����� �ʴ� �������� ���ڿ� ����
            sStringLen = IDL_MIN(MTC_TO_CHAR_MAX_PRECISION-1, sStringLen);
            acpMemCpy(sString,
                          mtddlget_text(sScanner),
                          sStringLen);
            sString[sStringLen] = 0;
        }
        else if ( sToken == MTD_DATE_FORMAT_DOUBLE_QUOTE_STRING )
        {
            // Get string length
            // "***"�� ��� ���ʿ� double quote�� ������ ���� ������ ����
            sStringLen = mtddlget_leng( sScanner ) - 2;
            
            // calculateInfo�� format������ (sFormatInfo) ������ ������ �Ҵ�
            ACI_TEST(aCallBack->alloc( aCallBack->info,
                                       sStringLen + 1,
                                       (void**)&( sExecute->calculateInfo ) )
                                       != ACI_SUCCESS );

            sFormatInfo->format[sFormatInfo->count].string =
                (acp_char_t*) sExecute->calculateInfo;
            sString = sFormatInfo->format[sFormatInfo->count].string;

            // "***"�� ��� ���ʿ� double quote�� �����Ѵ�.
            // MAX_PRECISION�� �ʰ����� �ʴ� �������� ���ڿ� ����
            sStringLen = IDL_MIN(MTC_TO_CHAR_MAX_PRECISION-1, sStringLen);
            acpMemCpy(sString,
                          mtddlget_text(sScanner)+1,
                          sStringLen);
            sString[sStringLen] = 0;
        }
        else if ( sToken == MTD_DATE_FORMAT_NONE )
        {
            // BUG-19753
            // separator Ȥ�� quoted string�� �ƴ� �νĵ��� ���� ������ ����

            sFormatLen = aFormatLen - ( sFormat - aFormat );
            ACI_TEST(aCallBack->alloc( aCallBack->info,
                                       sFormatLen + 1,
                                       (void**)&sErrorFormat )
                                       != ACI_SUCCESS );            
            
            acpMemCpy( sErrorFormat,
                           sFormat,
                           sFormatLen );
            sErrorFormat[sFormatLen] = '\0';
            
            ACI_RAISE( ERR_DATE_NOT_RECOGNIZED_FORMAT );
        }
        else
        {
            // NONE, DOUBLE_QUOTE ������ �ƴҶ��� string�� �ʿ������
            // �޸� �Ҵ��� ������ ������ �Լ��� ȣ���� ��, UMR �߻�
            ACI_TEST(aCallBack->alloc( aCallBack->info,
                                       1,
                                       (void**)&( sExecute->calculateInfo ) )
                                       != ACI_SUCCESS );

            sFormatInfo->format[sFormatInfo->count].string =
                (acp_char_t*) sExecute->calculateInfo;
            sFormatInfo->format[sFormatInfo->count].string[0] = 0;

            sFormatInfo->format[sFormatInfo->count].isFillMode = sIsFillMode;

            // To fix BUG-17693
            if ( sToken == MTD_DATE_FORMAT_FM )
            {
                sIsFillMode = ACP_TRUE;
            }
            else
            {
                // Nothing to do
            }
        }
        
        sFormatInfo->count++;

        // ���� ������ �������� ����Ű���� ������ �̵�
        sFormat += mtddlget_leng( sScanner );
        
        // get next token
        sToken = mtddllex( sScanner );
    }

    mtddllex_destroy ( sScanner );
    sInitScanner = ACP_FALSE;

    // calculateInfo���� mtdFormatInfo�� ó�� �ּҸ� �ٽ� ��������� �Ѵ�.
    (sExecute->calculateInfo) = sFormatInfo;

    if ( sToken == -1 )
    {
        ACI_RAISE( ERR_INVALID_LITERAL );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL )
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_DATE);
    }
    ACI_EXCEPTION( ERR_DATE_NOT_RECOGNIZED_FORMAT )
    {
        aciSetErrorCode(mtERR_ABORT_DATE_NOT_RECOGNIZED_FORMAT,
                                sErrorFormat);
    }
    ACI_EXCEPTION( ERR_LEX_INIT_FAILED )
    {
        aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                         "mtfToCharInterfaceMakeFormatInfo",
                         "Lex init failed" );
    }
    ACI_EXCEPTION_END;

    if ( sInitScanner == ACP_TRUE )
    {
        mtddllex_destroy ( sScanner );
    }

    return ACI_FAILURE;
}


ACI_RC removeLeadingBlank( acp_char_t     * aResult,
                           acp_uint32_t     aResultLen,
                           acp_uint32_t   * aBlankCnt )
{
/***********************************************************************
 *
 * Description :
 *    BUG-17693
 *
 * Implementation :
 *
 *    TO_CHAR( number_type, 'number_format_model' )
 *
 *    'FM' format�� ���� ��� ���ʿ� ���� ������ ��� �����Ѵ�.
 *
 *
 ***********************************************************************/

    acp_uint32_t    sIndex;
    acp_uint32_t    i;

    for ( sIndex = 0; sIndex < aResultLen; sIndex++ )
    {
        if ( aResult[sIndex] == ' ' )
        {
            (*aBlankCnt)++;
        }
        else
        {
            break;
        }
    }

    if (*aBlankCnt != 0)
    {
        for ( sIndex = (*aBlankCnt), i = 0;
             sIndex < aResultLen;
             sIndex++, i++ )
        {
            aResult[i] = aResult[sIndex];
        }
    }
    else
    {
        // Nothing to do.
    }

    return ACI_SUCCESS;
}
 
ACI_RC mtfToCharInterface_mtfTo_char( mtcStack    * aStack,
                                      acp_uint8_t * aToken,
                                      mtlCurrency * aCurrency )
{
/***********************************************************************
 *
 * Description : isql���� NUMBER �������� ���� ��ȯ�� ���� �Լ�.
 *               �Էµ� ���ڸ� number format�� ���� ��ȯ�Ͽ� ���ڷ� ���.
 *
 * Implementation :
 *    SET NUMFORMAT fmt
 *    COLUMN col FORMAT fmt
 *
 * @param[out] aStack[0] : format�� �°� ��ȯ�� ���ڿ�
 * @param[in]  aStack[1] : �Է� number ������
 * @param[in]  aStack[2] : number format
 * @param[in]  aToken    : mtfToCharInterface_checkFormat �Լ����� ��ȯ�� Token ���ڿ�
 * @param[in]  aCurrency : �ش� ������ NLS_ISO_CURRENCY, NLS_CURRENCY,
 *                         NLS_NUMERIC_CHARACTERS

 *    ex) SET NUMFORMAT 9.99EEEE
 *        COLUMN col FORMAT $999,990
 *
 ***********************************************************************/

    mtdCharType*    sResult = NULL;
    mtdNumericType* sNumber = NULL;
    mtdNumericType* sNumberTemp = NULL;
    acp_uint8_t     sNumberTempBuffer[MTD_NUMERIC_SIZE_MAXIMUM];
    mtdCharType*    sVarchar = NULL;
    mtlCurrency*    sCurrency = NULL;

    acp_sint32_t        sIntNum     = 0;
    acp_sint64_t        sRnNumCheck = 0;
    acp_char_t*         sNumericString = NULL;
    acp_uint8_t*        sNumFmt = NULL;
    acp_uint32_t        sNumFmtLen  = 0;

    // Numeric�� ���ڿ��� ��ȯ�� �� �ʿ��� �ִ� ���� ũ��
    // Scale'�� ������ -84 ~ 128 �̹Ƿ�, '+.' �Ǵ� '-.'�� Scale 128 ���� ���
    acp_char_t          sTemp[2 + MTD_NUMERIC_SCALE_MAXIMUM + 1];
    acp_sint32_t        sTempLen = 0;

    acp_uint32_t        sIterator = 0;
    acp_bool_t          sIsMinus = ACP_FALSE;
    acp_uint32_t        sResultLen = 0;
    acp_uint8_t         sString[MTD_NUMBER_FORMAT_BUFFER_LEN] = {0,};     // applyFormat()�� ȣ���ϱ� ���� �ʱ�ȭ
    acp_bool_t          sIsV = ACP_FALSE;
    acp_sint32_t        sNumCnt = 0;
    acp_sint32_t        sIntNumCnt = 0;
    acp_sint32_t        sNineCnt = 0;
    acp_sint32_t        sAfterPeriodNumCnt = 0;
    acp_sint32_t        sAfterPeriodNineCnt = 0;
    acp_sint32_t        sAfterPeriodInvalidNumCnt = 0;
    acp_bool_t          sIsPeriod = ACP_FALSE;
    acp_uint8_t         sRoundPos[4];
    acp_sint32_t        sRoundNum = 0;
    acp_uint16_t        sRoundPosLen = 0;
    acp_uint8_t       * sToken = NULL;
    acp_uint32_t        sBlankCnt = 0;
    mtdNumericType    * sMtdZeroValue = NULL;
    acp_uint8_t         sMtdZeroBuff[20] = { 1, 128, 0, };
    mtdNumericType    * sArgument2 = NULL;
    acp_uint8_t         sArgument2Buff[MTD_NUMERIC_SIZE_MAXIMUM];
    acp_double_t        sPowResult;

    /* BUG-34447 for isql
     * isql�� utISPApi::ReformatRow()���� NULL���� �ƴ� ��쿡�� ȣ��ǹǷ�
     * Null �˻�� �ʿ� ����.
     * ������ Ŭ���̾�Ʈ�� mtcInitializeColumn���� module ������ ����
     * �����Ƿ�  isNull ȣ�⵵ �Ұ�.
    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value,
                                           MTD_OFFSET_USELESS) == ACP_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value,
                                           MTD_OFFSET_USELESS) == ACP_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value,
                                        MTD_OFFSET_USELESS);
    }
    else
    {
    */
        sResult = (mtdCharType*)aStack[0].value;
        sNumber = (mtdNumericType*)aStack[1].value;
        sVarchar = (mtdCharType*)aStack[2].value;

        sNumberTemp = (mtdNumericType*)sNumberTempBuffer;
        sNumberTemp->length = 0;

        sNumFmt = sVarchar->value;
        sNumFmtLen = sVarchar->length;

        /* BUG-34447 for isql
         * isql�κ��� Token�� currency�� ���޹����Ƿ� 4916~4917 �������� ��ü
        if ( aInfo != NULL )
        {
            sNumberInfo = (mtfNumberInfo*) aInfo;

            acpMemCpy( sToken,
                           sNumberInfo->sToken,
                           sizeof(acp_uint8_t) * MTD_NUMBER_MAX );
            sCurrency = &(sNumberInfo->sCurrency);
        }
        else
        {
            //fix BUG-18162
            acpMemSet( sToken,
                           0,
                           sizeof(acp_uint8_t) * MTD_NUMBER_MAX );
            
            ACI_TEST( mtfToCharInterface_checkFormat( sNumFmt, sNumFmtLen, sToken )
                      != ACI_SUCCESS );
            
            if ( ( sToken[MTD_NUMBER_FORMAT_C] > 0 ) ||
                 ( sToken[MTD_NUMBER_FORMAT_D] > 0 ) ||
                 ( sToken[MTD_NUMBER_FORMAT_G] > 0 ) ||
                 ( sToken[MTD_NUMBER_FORMAT_L] > 0 ) )
            {
                acpMemSet( sCurrency, 0x0, sizeof( mtlCurrency ));
                ACI_TEST( aTemplate->nlsCurrency( aTemplate, sCurrency )
                          != ACI_SUCCESS );
                aTemplate->nlsCurrencyRef = ACP_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        */
        sToken = aToken;
        sCurrency = aCurrency;
        
        if ( sToken[MTD_NUMBER_FORMAT_FM] == 1 )
        {
            sNumFmt += 2;
            sNumFmtLen -= 2;
        }
        else
        {
            // Nothing to do
        }

        if ( sNumber->signExponent < 128 )
        {
            sIsMinus = ACP_TRUE;
        }
        else
        {
            sIsMinus = ACP_FALSE;
        }

        if ( ( mtcStrCaselessMatch( sNumFmt, 2, "RN", 2 ) == 0 ) ||
             ( mtcStrCaselessMatch( sNumFmt, 4, "XXXX", 4 ) == 0 ) )
        {
            // �Ҽ��� �Էµ� ��� ������ �ݿø��Ѵ�.
            sMtdZeroValue = (mtdNumericType*)sMtdZeroBuff;
            ACI_TEST( mtcRoundFloat( sNumberTemp,
                                       sNumber,
                                       sMtdZeroValue )
                      != ACI_SUCCESS );

            // numeric Ÿ���� ������ ��ȯ�ϴ� ����. integer ���� ������ ��ȯ�Ѵ�.
            // *(sNumberTemp)�� numeric�� length�̴�.
            if ( sNumberTemp->length > 0 )
            {
                for ( sIterator = 0; sIterator < ((acp_uint32_t)sNumberTemp->length) - 1; sIterator++ )
                {
                    if ( sNumberTemp->signExponent > 128 )
                    {
                        acpMathPow( 100,
                                    ( sNumberTemp->signExponent - ( 193 + sIterator ) ),
                                    &sPowResult );
                        sRnNumCheck += (acp_sint64_t) ( sNumberTemp->mantissa[sIterator] *
                                                        sPowResult );
                        /* BUG-34447 c porting for isql
                        sRnNumCheck += (acp_sint64_t) ( sNumberTemp->mantissa[sIterator] *
                                                 idlOS::pow( 100, ( sNumberTemp->signExponent -
                                                                    ( 193 + sIterator ) ) ) );
                                                                    */
                    }
                    else
                    {
                        acpMathPow( 100,
                                    ( 63 - sNumberTemp->signExponent - sIterator ),
                                    &sPowResult );
                        sRnNumCheck -= (acp_sint64_t) ( ( 99 - sNumberTemp->mantissa[sIterator] ) *
                                                        sPowResult );
                        /* BUG-34447 c porting for isql
                        sRnNumCheck -= (acp_sint64_t) ( ( 99 - sNumberTemp->mantissa[sIterator] ) *
                                                 idlOS::pow( 100, ( 63 - sNumberTemp->signExponent -
                                                                    sIterator ) ) );
                        */
                    }
                    
                    // ������� Integer�̹Ƿ�, numeric�� Integer ũ�� ���Ϸ� �����ؾ� �Ѵ�.
                    if ( ( sRnNumCheck > ACP_SINT32_MAX ) || ( sRnNumCheck < ACP_SINT32_MIN ) )
                    {
                        ACI_RAISE( ERR_INVALID_LENGTH );
                    }
                    else
                    {
                        sIntNum = (acp_sint32_t) sRnNumCheck;
                    }
                }
            }
            else
            {
                // Nothing to do.
            }

            // XXXX �Ǵ� RN format�� ����Ѵ�.
            ACI_TEST( compXXXXandRN( sNumFmt,
                                     sNumFmtLen,
                                     sResult->value,
                                     &(sResult->length),
                                     sIntNum )
                      != ACI_SUCCESS );
        }
        // RN, XXXX�� �ƴ�  fmt�� ��� sNumber�� �״�� string���� ��ȯ�Ѵ�.
        else
        {
            sNineCnt = sToken[MTD_NUMBER_FORMAT_ZERO] +
                       sToken[MTD_NUMBER_FORMAT_NINE];
            sAfterPeriodNineCnt = sToken[MTD_COUNT_FLOAT_ZERO] +
                                  sToken[MTD_COUNT_FLOAT_NINE];
            if ( sToken[MTD_NUMBER_FORMAT_EEEE] <= 0 )
            {
                // format�� . �ڿ� ���� 9 �Ǵ� 0�� ������ ���缭 �ݿø�
                sRoundNum = sAfterPeriodNineCnt;

                if ( sRoundNum >= 100 )
                {
                    sRoundPos[0] = sRoundNum / 100 + '0';
                    sRoundPos[1] = ( sRoundNum / 10 ) -
                                   ( ( sRoundNum / 100 ) * 10 ) + '0';
                    sRoundPos[2] = sRoundNum % 10 + '0';
                    sRoundPosLen = 3;
                }
                else if ( sRoundNum >= 10 )
                {
                    sRoundPos[0] = sRoundNum / 10 + '0';
                    sRoundPos[1] = sRoundNum % 10 + '0';
                    sRoundPosLen = 2;
                }
                else if ( sRoundNum > 0 )
                {
                    sRoundPos[0] = sRoundNum % 10 + '0';
                    sRoundPosLen = 1;
                }
                else if ( sRoundNum == 0 )
                {
                    sRoundPos[0] = '0';
                    sRoundPosLen = 1;
                }

                sArgument2 = (mtdNumericType*)sArgument2Buff;
                ACI_TEST( mtcMakeNumeric( sArgument2,
                                            MTD_FLOAT_MANTISSA_MAXIMUM,
                                            sRoundPos,
                                            sRoundPosLen )
                          != ACI_SUCCESS );

                ACI_TEST( mtcRoundFloat( sNumberTemp,
                                           sNumber,
                                           sArgument2 )
                          != ACI_SUCCESS );

                ACI_TEST( convertToString( sNumberTemp->length,
                                           sNumberTemp->signExponent,
                                           sNumberTemp->mantissa,
                                           sTemp,
                                           &sTempLen )
                          != ACI_SUCCESS );

                sNumericString = sTemp;
            }
            else
            {
                ACI_TEST( convertToString( sNumber->length,
                                           sNumber->signExponent,
                                           sNumber->mantissa,
                                           sTemp,
                                           &sTempLen )
                          != ACI_SUCCESS );

                sNumericString = sTemp;

                for ( sIterator = 0; sIterator < (acp_uint32_t) sTempLen; sIterator++ )
                {
                    if ( *( sNumericString + sIterator ) == '.' )
                    {
                        sIsPeriod = ACP_TRUE;
                    }

                    if ( ( *( sNumericString + sIterator ) >= '0' ) &&
                         ( *( sNumericString + sIterator ) <= '9' ) )
                    {
                        sNumCnt++;

                        if ( sIsPeriod == ACP_FALSE )
                        {
                            sIntNumCnt++;
                        }
                        else
                        {
                            sAfterPeriodNumCnt++;
                        }
                    }
                }

                sIsPeriod = ACP_FALSE;

                for ( sIterator = 0; sIterator < (acp_uint32_t) sTempLen; sIterator++ )
                {
                    if ( ( *( sNumericString + sIterator ) >= '1' ) &&
                         ( *( sNumericString + sIterator ) <= '9' ) &&
                         ( sIsPeriod == ACP_TRUE ) )
                    {
                        break;
                    }

                    if ( ( sIsPeriod == ACP_TRUE ) && ( *( sNumericString + sIterator ) == '0' ) )
                    {
                        sAfterPeriodInvalidNumCnt++;
                    }

                    if ( *( sNumericString + sIterator ) == '.' )
                    {
                        sIsPeriod = ACP_TRUE;
                    }
                }

                // sString�� 0~9�� �������� format�� 9�� ������ ���� ���
                // �ݿø��Ѵ�.
                // �� ��° �ڸ����� �ݿø��� ������ ����
                if ( ( sIntNumCnt > 0 ) &&
                     ( ( sNumCnt > sNineCnt ) ||
                       ( ( sAfterPeriodNineCnt + 1 ) != sNumCnt ) ) )
                {
                    sRoundNum = ( sAfterPeriodNineCnt + 1 ) - sIntNumCnt;
                }
                else if ( sIntNumCnt == 0 )
                {
                    sRoundNum = sAfterPeriodNineCnt + 1 + sAfterPeriodInvalidNumCnt;
                }
                else
                {
                    sRoundNum = sAfterPeriodNumCnt;
                }

                if ( sIsV == ACP_TRUE )
                {
                    if ( sIntNumCnt == 0 )
                    {
                        sRoundNum = sAfterPeriodNineCnt + 1 + sAfterPeriodInvalidNumCnt;
                    }
                    else
                    {
                        sRoundNum = sAfterPeriodNineCnt - sIntNumCnt + 1;
                    }
                }

                if ( sRoundNum >= 100 )
                {
                    sRoundPos[0] = sRoundNum / 100 + '0';
                    sRoundPos[1] = ( sRoundNum / 10 ) -
                                   ( ( sRoundNum / 100 ) * 10 ) + '0';
                    sRoundPos[2] = sRoundNum % 10 + '0';
                    sRoundPosLen = 3;
                }
                else if ( sRoundNum >= 10 )
                {
                    sRoundPos[0] = sRoundNum / 10 + '0';
                    sRoundPos[1] = sRoundNum % 10 + '0';
                    sRoundPosLen = 2;
                }
                else if ( sRoundNum > 0 )
                {
                    sRoundPos[0] = sRoundNum % 10 + '0';
                    sRoundPosLen = 1;
                }
                else if ( sRoundNum == 0 )
                {
                    sRoundPos[0] = '0';
                    sRoundPosLen = 1;
                }
                else if ( ( sRoundNum < 0 ) && ( sRoundNum > -10 ) )
                {
                    sRoundPos[0] = '-';
                    sRoundPos[1] = abs( sRoundNum ) % 10 + '0';
                    sRoundPosLen = 2;
                }
                else if ( ( sRoundNum <= -10 ) && ( sRoundNum > -100 ) )
                {
                    sRoundPos[0] = '-';
                    sRoundPos[1] = abs( sRoundNum ) / 10 + '0';
                    sRoundPos[2] = abs( sRoundNum ) % 10 + '0';
                    sRoundPosLen = 3;
                }
                else if ( sRoundNum <= -100 )
                {
                    sRoundPos[0] = '-';
                    sRoundPos[1] = abs( sRoundNum ) / 100 + '0';
                    sRoundPos[2] = ( abs( sRoundNum ) / 10 ) -
                                   ( ( abs( sRoundNum ) / 100 ) * 10 ) + '0';
                    sRoundPos[3] = sRoundNum % 10 + '0';
                    sRoundPosLen = 4;
                }
                else
                {
                    // nothing to do
                }

                sArgument2 = (mtdNumericType*)sArgument2Buff;
                ACI_TEST( mtcMakeNumeric( sArgument2,
                                            MTD_FLOAT_MANTISSA_MAXIMUM,
                                            sRoundPos,
                                            sRoundPosLen )
                          != ACI_SUCCESS );

                ACI_TEST( mtcRoundFloat( sNumberTemp,
                                           sNumber,
                                           sArgument2 )
                          != ACI_SUCCESS );

                ACI_TEST( convertToString( sNumberTemp->length,
                                           sNumberTemp->signExponent,
                                           sNumberTemp->mantissa,
                                           sTemp,
                                          &sTempLen )
                          != ACI_SUCCESS );

                sNumericString = sTemp;
            }

            ACI_TEST( applyFormat( sNumericString,
                                   sTempLen,
                                   sNumFmt,
                                   sNumFmtLen,
                                   sString,
                                   &sResultLen,
                                   sToken,
                                   sCurrency,
                                   sIsMinus )
                      != ACI_SUCCESS );

            // BUG-37357
            ACI_TEST_RAISE( (acp_uint32_t)aStack[0].column->precision < sResultLen,
                            ERR_INVALID_LENGTH );
            
            acpMemCpy( sResult->value,
                           sString,
                           sResultLen );
            sResult->length = sResultLen;
        }

        // To fix BUG-17693
        if ( sToken[MTD_NUMBER_FORMAT_FM] == 1 )
        {
            // BUG-19149
            ACI_TEST( removeLeadingBlank( (acp_char_t *) sResult->value,
                                          (acp_uint32_t) sResult->length,
                                          & sBlankCnt )
                      != ACI_SUCCESS );
            
            sResult->length = (sResult->length) - sBlankCnt;
        }
        else
        {
            // Nothing to do
        }
//    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}
