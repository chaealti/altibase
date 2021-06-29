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
 * $Id: mtcdDate.cpp 38198 2010-02-11 06:15:55Z sungminee $
 **********************************************************************/

#include <mtce.h>
#include <mtcc.h>
#include <mtcd.h>
#include <mtcl.h>
#include <mtcdTypes.h>
#include <mtcddlLexer.h>

#define MTD_DATE_ALIGN (sizeof(acp_uint32_t))
#define MTD_DATE_SIZE  (sizeof(mtdDateType))

mtdDateType mtcdDateNull = { -32768, 0, 0 };

static acp_uint8_t MONTHS[12][4] = { "JAN","FEB","MAR","APR","MAY","JUN",
                                     "JUL","AUG","SEP","OCT","NOV","DEC" };

static
const char* gMONTHName[12] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};
static
const char* gMonthName[12] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static
const char* gmonthName[12] = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"
};

static
const char* gMONName[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};
static
const char* gMonName[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static
const char* gmonName[12] = {
    "jan", "feb", "mar", "apr", "may", "jun",
    "jul", "aug", "sep", "oct", "nov", "dec"
};

static
const char* gDAYName[7] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY"
};
static
const char* gDayName[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static
const char* gdayName[7] = {
    "sunday", "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"
};

static
const char* gDYName[7] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};
static
const char* gDyName[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static
const char* gdyName[7] = {
    "sun", "mon", "tue", "wed", "thu", "fri", "sat"
};

static
const char* gRMMonth[12] = {
    "I", "II", "III", "IV", "V", "VI",
    "VII", "VIII", "IX", "X", "XI", "XII"
};
static
const char* grmMonth[12] = {
    "i", "ii", "iii", "iv", "v", "vi",
    "vii", "viii", "ix", "x", "xi", "xii"
};

#define gNONE   (acp_uint8_t)0
#define gDIGIT  (acp_uint8_t)1
#define gALPHA  (acp_uint8_t)2
#define gSEPAR  (acp_uint8_t)3
#define gWHSP   (acp_uint8_t)4

const acp_uint8_t gInputST[256] = {
    gNONE , gNONE , gNONE , gNONE , /* 00-03 */
    gNONE , gNONE , gNONE , gNONE , /* 04-07 */
    gNONE , gWHSP , gWHSP , gNONE , /* 08-0B | BS,HT,LF,VT */
    gNONE , gWHSP , gNONE , gNONE , /* 0C-0F | FF,CR,SO,SI */
    gNONE , gNONE , gNONE , gNONE , /* 10-13 */
    gNONE , gNONE , gNONE , gNONE , /* 14-17 */
    gNONE , gNONE , gNONE , gNONE , /* 18-1B */
    gNONE , gNONE , gNONE , gNONE , /* 1C-1F */
    gWHSP , gSEPAR, gNONE , gSEPAR, /* 20-23 |  !"# */
    gSEPAR, gSEPAR, gNONE , gSEPAR, /* 24-27 | $%&' */
    gSEPAR, gSEPAR, gSEPAR, gSEPAR, /* 28-2B | ()*+ */
    gSEPAR, gSEPAR, gSEPAR, gSEPAR, /* 2C-2F | ,-./ */
    gDIGIT, gDIGIT, gDIGIT, gDIGIT, /* 30-33 | 0123 */
    gDIGIT, gDIGIT, gDIGIT, gDIGIT, /* 34-37 | 4567 */
    gDIGIT, gDIGIT, gSEPAR, gSEPAR, /* 38-3B | 89:; */
    gSEPAR, gSEPAR, gSEPAR, gSEPAR, /* 3C-3F | <=>? */
    gSEPAR, gALPHA, gALPHA, gALPHA, /* 40-43 | @ABC */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 44-47 | DEFG */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 48-4B | HIJK */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 4C-4F | LMNO */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 50-53 | PQRS */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 54-57 | TUVW */
    gALPHA, gALPHA, gALPHA, gSEPAR, /* 58-5B | XYZ[ */
    gSEPAR, gSEPAR, gSEPAR, gSEPAR, /* 5C-5F | \]^_ */
    gSEPAR, gALPHA, gALPHA, gALPHA, /* 60-63 | `abc */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 64-67 | defg */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 68-6B | hijk */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 6C-6F | lmno */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 70-73 | pqrs */
    gALPHA, gALPHA, gALPHA, gALPHA, /* 74-77 | tuvw */
    gALPHA, gALPHA, gALPHA, gSEPAR, /* 78-7B | xyz{ */
    gSEPAR, gSEPAR, gSEPAR, gSEPAR, /* 7C-7F | |}~  */
    gNONE , gNONE , gNONE , gNONE , /* 80-83 */
    gNONE , gNONE , gNONE , gNONE , /* 84-87 */
    gNONE , gNONE , gNONE , gNONE , /* 88-8B */
    gNONE , gNONE , gNONE , gNONE , /* 8C-8F */
    gNONE , gNONE , gNONE , gNONE , /* 90-93 */
    gNONE , gNONE , gNONE , gNONE , /* 94-97 */
    gNONE , gNONE , gNONE , gNONE , /* 98-9B */
    gNONE , gNONE , gNONE , gNONE , /* 9C-9F */
    gNONE , gNONE , gNONE , gNONE , /* A0-A3 */
    gNONE , gNONE , gNONE , gNONE , /* A4-A7 */
    gNONE , gNONE , gNONE , gNONE , /* A8-AB */
    gNONE , gNONE , gNONE , gNONE , /* AC-AF */
    gNONE , gNONE , gNONE , gNONE , /* B0-B3 */
    gNONE , gNONE , gNONE , gNONE , /* B4-B7 */
    gNONE , gNONE , gNONE , gNONE , /* B8-BB */
    gNONE , gNONE , gNONE , gNONE , /* BC-BF */
    gNONE , gNONE , gNONE , gNONE , /* C0-C3 */
    gNONE , gNONE , gNONE , gNONE , /* C4-C7 */
    gNONE , gNONE , gNONE , gNONE , /* C8-CB */
    gNONE , gNONE , gNONE , gNONE , /* CC-CF */
    gNONE , gNONE , gNONE , gNONE , /* D0-D3 */
    gNONE , gNONE , gNONE , gNONE , /* D4-D7 */
    gNONE , gNONE , gNONE , gNONE , /* D8-DB */
    gNONE , gNONE , gNONE , gNONE , /* DC-DF */
    gNONE , gNONE , gNONE , gNONE , /* E0-E3 */
    gNONE , gNONE , gNONE , gNONE , /* E4-E7 */
    gNONE , gNONE , gNONE , gNONE , /* E8-EB */
    gNONE , gNONE , gNONE , gNONE , /* EC-EF */
    gNONE , gNONE , gNONE , gNONE , /* F0-F3 */
    gNONE , gNONE , gNONE , gNONE , /* F4-F7 */
    gNONE , gNONE , gNONE , gNONE , /* F8-FB */
    gNONE , gNONE , gNONE , gNONE   /* FC-FF */
};

const acp_uint8_t gDaysOfMonth[2][13] = {
    { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } ,  // 
    { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }    // ����
};

const acp_uint32_t gAccDaysOfMonth[2][13] = {
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 }, // 
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }  // ����
};

acp_uint32_t gHashMonth[12];

static ACI_RC mtdInitializeDate( acp_uint32_t aNo );

static ACI_RC mtdEstimate( acp_uint32_t* aColumnSize,
                           acp_uint32_t* aArguments,
                           acp_sint32_t* aPrecision,
                           acp_sint32_t* aScale );

static ACI_RC mtdValue( mtcTemplate*  aTemplate,
                        mtcColumn*    aColumn,
                        void*         aValue,
                        acp_uint32_t* aValueOffset,
                        acp_uint32_t  aValueSize,
                        const void*   aToken,
                        acp_uint32_t  aTokenLength,
                        ACI_RC*       aResult );

static acp_uint32_t mtdActualSize( const mtcColumn* aColumn,
                                   const void*      aRow,
                                   acp_uint32_t     aFlag );

static void mtdNull( const mtcColumn* aColumn,
                     void*            aRow,
                     acp_uint32_t     aFlag );

static acp_uint32_t mtdHash( acp_uint32_t     aHash,
                             const mtcColumn* aColumn,
                             const void*      aRow,
                             acp_uint32_t     aFlag );

static acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                             const void*      aRow,
                             acp_uint32_t     aFlag );

static acp_sint32_t mtdDateLogicalComp( mtdValueInfo* aValueInfo1,
                                        mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                             mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                              mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                                                mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                                                 mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                                                   mtdValueInfo* aValueInfo2 );

static acp_sint32_t mtdDateStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                                    mtdValueInfo* aValueInfo2 );

static void mtdEndian( void* aValue );

static ACI_RC mtdValidate( mtcColumn*   aColumn,
                           void*        aValue,
                           acp_uint32_t aValueSize);

static acp_double_t mtdSelectivityDate( void* aColumnMax,
                                        void* aColumnMin,
                                        void* aValueMax,
                                        void* aValueMin );

static ACI_RC mtdEncode( mtcColumn*    aColumn,
                         void*         aValue,
                         acp_uint32_t  aValueSize,
                         acp_uint8_t*  aCompileFmt,
                         acp_uint32_t  aCompileFmtLen,
                         acp_uint8_t*  aText,
                         acp_uint32_t* aTextLen,
                         ACI_RC*       aRet );

static ACI_RC mtdDecode( mtcTemplate*  aTemplate,
                         mtcColumn*    aColumn,
                         void*         aValue,
                         acp_uint32_t* aValueSize,
                         acp_uint8_t*  aCompileFmt,
                         acp_uint32_t  aCompileFmtLen,
                         acp_uint8_t*  aText,
                         acp_uint32_t  aTextLen,
                         ACI_RC*       aRet );

static ACI_RC mtdCompileFmt( mtcColumn*    aColumn,
                             acp_uint8_t*  aCompiledFmt,
                             acp_uint32_t* aCompiledFmtLen,
                             acp_uint8_t*  aFormatString,
                             acp_uint32_t  aFormatStringLength,
                             ACI_RC*       aRet );

static ACI_RC mtdValueFromOracle( mtcColumn*    aColumn,
                                  void*         aValue,
                                  acp_uint32_t* aValueOffset,
                                  acp_uint32_t  aValueSize,
                                  const void*   aOracleValue,
                                  acp_sint32_t  aOracleLength,
                                  ACI_RC*       aResult );

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue );

static acp_uint32_t mtdNullValueSize();

static mtcName mtdTypeName[1] = {
    { NULL, 4, (void*)"DATE" }
};

static mtcColumn mtdColumn;

mtdModule mtcdDate = {
    mtdTypeName,
    &mtdColumn,
    MTD_DATE_ID,
    0,
    { /*SMI_BUILTIN_B_TREE_INDEXTYPE_ID*/0,
      /*SMI_BUILTIN_B_TREE2_INDEXTYPE_ID*/0,
      0, 0, 0, 0, 0 },
    MTD_DATE_ALIGN,
    MTD_GROUP_DATE|
    MTD_CANON_NEEDLESS|
    MTD_CREATE_ENABLE|
    MTD_COLUMN_TYPE_FIXED|
    MTD_SELECTIVITY_ENABLE|
    MTD_SEARCHABLE_PRED_BASIC|
    MTD_SQL_DATETIME_SUB_TRUE|
    MTD_LITERAL_TRUE|
    MTD_VARIABLE_LENGTH_TYPE_FALSE| // PROJ-1705
    MTD_DATA_STORE_DIVISIBLE_FALSE| // PROJ-1705
    MTD_DATA_STORE_MTDVALUE_FALSE,  // PROJ-1705
    19,
    0,
    0,
    (void*)&mtcdDateNull,
    mtdInitializeDate,
    mtdEstimate,
    mtdValue,
    mtdActualSize,
    mtdGetPrecisionNA,
    mtdNull,
    mtdHash,
    mtdIsNull,
    mtdIsTrueNA,
    mtdDateLogicalComp,    // Logical Comparison
    {
        // Key Comparison
        {
            // mt value�� ���� compare 
            mtdDateMtdMtdKeyAscComp, // Ascending Key Comparison
            mtdDateMtdMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // mt value�� stored value���� compare 
            mtdDateStoredMtdKeyAscComp, // Ascending Key Comparison
            mtdDateStoredMtdKeyDescComp // Descending Key Comparison
        }
        ,
        {
            // stored value�� ���� compare 
            mtdDateStoredStoredKeyAscComp, // Ascending Key Comparison
            mtdDateStoredStoredKeyDescComp // Descending Key Comparison
        }
    },
    mtdCanonizeDefault,
    mtdEndian,
    mtdValidate,
    mtdSelectivityDate,
    mtdEncode,
    mtdDecode,
    mtdCompileFmt,
    mtdValueFromOracle,
    mtdMakeColumnInfoDefault,

    // PROJ-1705
    mtdStoredValue2MtdValue,
    mtdNullValueSize,
    mtdHeaderSizeDefault
};

ACI_RC mtdDateInterfaceConvertDate2Interval( mtdDateType     * aDate,
                                             mtdIntervalType * aInterval );
ACI_RC mtdDateInterfaceConvertInterval2Date( mtdIntervalType * aInterval,
                                             mtdDateType     * aDate );

ACI_RC mtdInitializeDate( acp_uint32_t aNo )
{
    ACI_TEST( mtdInitializeModule( &mtcdDate, aNo ) != ACI_SUCCESS );

    // mtdColumn�� �ʱ�ȭ
    ACI_TEST( mtcInitializeColumn( & mtdColumn,
                                   & mtcdDate,
                                   0,   // arguments
                                   0,   // precision
                                   0 )  // scale
              != ACI_SUCCESS );
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtdEstimate( acp_uint32_t* aColumnSize,
                    acp_uint32_t* aArguments,
                    acp_sint32_t* aPrecision,
                    acp_sint32_t* aScale )
{
    ACP_UNUSED(aPrecision);
    ACP_UNUSED(aScale);
    
    ACI_TEST_RAISE( *aArguments != 0, ERR_INVALID_LENGTH );

    *aColumnSize = MTD_DATE_SIZE;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
    
}

ACI_RC mtdValue( mtcTemplate*  aTemplate,
                 mtcColumn*    aColumn,
                 void*         aValue,
                 acp_uint32_t* aValueOffset,
                 acp_uint32_t  aValueSize,
                 const void*   aToken,
                 acp_uint32_t  aTokenLength,
                 ACI_RC*       aResult )
{
    acp_uint32_t sValueOffset;
    mtdDateType* sValue;

    ACE_ASSERT( aTemplate != NULL );
    ACE_ASSERT( aTemplate->dateFormat != NULL );
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_DATE_ALIGN );

    if( sValueOffset + MTD_DATE_SIZE <= aValueSize )
    {
        sValue = (mtdDateType*)( (acp_uint8_t*)aValue + sValueOffset );
        if( aTokenLength == 0 )
        {
            *sValue = mtcdDateNull;
        }
        else
        {
            acpMemSet((void*)sValue, 0x00, sizeof(mtdDateType));

            ACI_TEST( mtdDateInterfaceToDate(
                          sValue,
                          (acp_uint8_t*)aToken,
                          (acp_uint16_t)aTokenLength,
                          (acp_uint8_t*)aTemplate->dateFormat,
                          acpCStrLen( aTemplate->dateFormat, ACP_UINT32_MAX ) )
                      != ACI_SUCCESS );

            // PROJ-1436
            // dateFormat�� ���������� ǥ���Ѵ�.
            aTemplate->dateFormatRef = ACP_TRUE;
        }
        
        aColumn->column.offset  = sValueOffset;
        *aValueOffset            =  sValueOffset + MTD_DATE_SIZE;
        
        *aResult = ACI_SUCCESS;
    }
    else
    {
        *aResult = ACI_FAILURE;
    }
    
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

acp_uint32_t mtdActualSize( const mtcColumn* aTemp,
                            const void*      aTemp2,
                            acp_uint32_t     aTemp3)
{
    ACP_UNUSED(aTemp);
    ACP_UNUSED(aTemp2);
    ACP_UNUSED(aTemp3);
    
    return MTD_DATE_SIZE;

}

void mtdNull( const mtcColumn* aColumn,
              void*            aRow,
              acp_uint32_t     aFlag )
{
    
    mtdDateType* sValue;

    ACP_UNUSED( aColumn);
    
    sValue = (mtdDateType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           NULL );

    if( sValue != NULL )
    {
        *sValue = mtcdDateNull;
    }

}

acp_uint32_t mtdHash( acp_uint32_t     aHash,
                      const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    mtdDateType* sDate;
    acp_sint16_t sYear;
    acp_uint8_t  sMonth;
    acp_uint8_t  sDay;
    acp_uint8_t  sHour;
    acp_uint8_t  sMinute;
    acp_uint8_t  sSecond;
    acp_uint32_t sMicroSecond;

    ACP_UNUSED( aColumn);

    sDate = (mtdDateType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdDate.staticNull );

    sYear        = mtdDateInterfaceYear(sDate);
    sMonth       = mtdDateInterfaceMonth(sDate);
    sDay         = mtdDateInterfaceDay(sDate);
    sHour        = mtdDateInterfaceHour(sDate);
    sMinute      = mtdDateInterfaceMinute(sDate);
    sSecond      = mtdDateInterfaceSecond(sDate);
    sMicroSecond = mtdDateInterfaceMicroSecond(sDate);

    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sYear,
                     sizeof(sYear) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sMonth,
                     sizeof(sMonth) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sDay,
                     sizeof(sDay) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sHour,
                     sizeof(sHour) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sMinute,
                     sizeof(sMinute) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sSecond,
                     sizeof(sSecond) );
    aHash = mtcHash( aHash,
                     (const acp_uint8_t*)&sMicroSecond,
                     sizeof(sMicroSecond) );
    return aHash;
}

acp_bool_t mtdIsNull( const mtcColumn* aColumn,
                      const void*      aRow,
                      acp_uint32_t     aFlag )
{
    const mtdDateType* sValue;
    acp_sint32_t       sResult;

    ACP_UNUSED( aColumn);
 
    sValue = (const mtdDateType*)
        mtdValueForModule( NULL,
                           aRow,
                           aFlag,
                           mtcdDate.staticNull );

    sResult = MTD_DATE_IS_NULL( sValue );
    
    return ( sResult == 1 ) ? ACP_TRUE : ACP_FALSE;
}

acp_sint32_t
mtdDateLogicalComp( mtdValueInfo* aValueInfo1,
                    mtdValueInfo* aValueInfo2 )
{
    return mtdDateMtdMtdKeyAscComp( aValueInfo1,
                                    aValueInfo2 );
}

acp_sint32_t
mtdDateMtdMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                         mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDateType* sValue1;
    mtdDateType* sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;

    //---------
    // value1
    //---------        
    sValue1 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdDate.staticNull );

    sNull1 = MTD_DATE_IS_NULL( sValue1 );   

    //---------
    // value2
    //---------        
    sValue2 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDate.staticNull );
    sNull2 = MTD_DATE_IS_NULL( sValue2 );   

    //---------
    // compare 
    //---------
    
    if( !sNull1 && !sNull2 )
    {
        // Enhancement-13065 Date �� ���� ���� 
        // mtdDateInterface�� compare �Լ��� �����ؼ�
        // �� ������ å���� mtdDateInterface�� ������.
        //
        // null�� ���� �� ��å�� mtdDateModule�� ������ �ִ´�.
        // mtdDateInterfaceCompare�� �ܼ��� ���� ũ�⸦ ���� ��,
        // null�� ���� �� ��å�� ������ ���� �ʱ� ������
        // null üũ�� mtcdDate���� ���ش�.

        return mtdDateInterfaceCompare( sValue1, sValue2 );
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDateMtdMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                          mtdValueInfo* aValueInfo2 )
{
    /***********************************************************************
     *
     * Description : Mtd Ÿ���� Key�� ���� ascending compare
     *
     * Implementation :
     *
     ***********************************************************************/

    mtdDateType* sValue1;
    mtdDateType* sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;

    //---------
    // value1
    //---------        
    sValue1 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo1->value,
                           aValueInfo1->flag,
                           mtcdDate.staticNull );

    sNull1 = MTD_DATE_IS_NULL( sValue1 );   

    //---------
    // value2
    //---------        
    sValue2 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDate.staticNull );
    
    sNull2 = MTD_DATE_IS_NULL( sValue2 );

    //---------
    // compare
    //---------        

    if( !sNull1 && !sNull2 )
    {
        return mtdDateInterfaceCompare(sValue2, sValue1);
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDateStoredMtdKeyAscComp( mtdValueInfo* aValueInfo1,
                            mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/
    
    mtdDateType  sValue1;
    mtdDateType* sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;

    //-------
    // value1
    //-------

    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.year, aValueInfo1->value );

    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo1->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t ) 
    MTC_INT_BYTE_ASSIGN( &sValue1.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo1->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );
    
    sNull1 = MTD_DATE_IS_NULL( &sValue1 );

    //-------
    // value2
    //-------
    sValue2 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDate.staticNull );

    sNull2 = MTD_DATE_IS_NULL( sValue2 );

    //-------
    // Compare
    //-------    

    if( !sNull1 && !sNull2 )
    {
        // Enhancement-13065 Date �� ���� ���� 
        // mtdDateInterface�� compare �Լ��� �����ؼ�
        // �� ������ å���� mtdDateInterface�� ������.
        //
        // null�� ���� �� ��å�� mtdDateModule�� ������ �ִ´�.
        // mtdDateInterfaceCompare�� �ܼ��� ���� ũ�⸦ ���� ��,
        // null�� ���� �� ��å�� ������ ���� �ʱ� ������
        // null üũ�� mtcdDate���� ���ش�.

        return mtdDateInterfaceCompare( &sValue1, sValue2 );
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDateStoredMtdKeyDescComp( mtdValueInfo* aValueInfo1,
                             mtdValueInfo* aValueInfo2 )
{
    /***********************************************************************
     *
     * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
     *
     * Implementation :
     *
     ***********************************************************************/
    
    mtdDateType  sValue1;
    mtdDateType* sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;

    //-------
    // value1
    //-------

    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.year, aValueInfo1->value );

    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo1->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t )
    MTC_INT_BYTE_ASSIGN( &sValue1.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo1->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );

    sNull1 = MTD_DATE_IS_NULL( &sValue1 );

    //-------
    // value2
    //-------
    sValue2 = (mtdDateType*)
        mtdValueForModule( NULL,
                           aValueInfo2->value,
                           aValueInfo2->flag,
                           mtcdDate.staticNull );

    sNull2 = MTD_DATE_IS_NULL( sValue2 );

    //-------
    // Compare
    //-------

    if( !sNull1 && !sNull2 )
    {
        return mtdDateInterfaceCompare(sValue2, &sValue1);
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDateStoredStoredKeyAscComp( mtdValueInfo* aValueInfo1,
                               mtdValueInfo* aValueInfo2 )
{
/***********************************************************************
 *
 * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDateType  sValue1;
    mtdDateType  sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;
    
    //-------
    // value1
    //-------

    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.year, aValueInfo1->value );
    
    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo1->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t )
    MTC_INT_BYTE_ASSIGN( &sValue1.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo1->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );

    sNull1 = MTD_DATE_IS_NULL( &sValue1 );
    
    //-------
    // value2
    //-------

    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue2.year, aValueInfo2->value );

    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue2.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo2->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t )
    MTC_INT_BYTE_ASSIGN( &sValue2.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo2->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );

    sNull2 = MTD_DATE_IS_NULL( &sValue2 );

    //-------
    // Compare
    //-------

    if( !sNull1 && !sNull2 )
    {
        // Enhancement-13065 Date �� ���� ����
        // mtdDateInterface�� compare �Լ��� �����ؼ�
        // �� ������ å���� mtdDateInterface�� ������.
        //
        // null�� ���� �� ��å�� mtdDateModule�� ������ �ִ´�.
        // mtdDateInterfaceCompare�� �ܼ��� ���� ũ�⸦ ���� ��,
        // null�� ���� �� ��å�� ������ ���� �ʱ� ������
        // null üũ�� mtcdDate���� ���ش�.

        return mtdDateInterfaceCompare( &sValue1, &sValue2 );
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}

acp_sint32_t
mtdDateStoredStoredKeyDescComp( mtdValueInfo* aValueInfo1,
                                mtdValueInfo* aValueInfo2 )
{
    /***********************************************************************
     *
     * Description : Mtd Ÿ���� Key�� Stored Key ���� ascending compare
     *
     * Implementation :
     *
     ***********************************************************************/

    mtdDateType  sValue1;
    mtdDateType  sValue2;
    acp_sint32_t sNull1;
    acp_sint32_t sNull2;
    
    //-------
    // value1
    //-------
    
    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.year, aValueInfo1->value );

    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue1.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo1->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t ) 
    MTC_INT_BYTE_ASSIGN( &sValue1.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo1->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );

    sNull1 = MTD_DATE_IS_NULL( &sValue1 );
    
    //-------
    // value2
    //-------

    // year ( acp_sint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue2.year, aValueInfo2->value );

    // mon_day_hour ( acp_uint16_t )
    MTC_SHORT_BYTE_ASSIGN( &sValue2.mon_day_hour,
                           ((acp_uint8_t*)aValueInfo2->value) +
                           sizeof(acp_sint16_t) );

    // min_sec_min ( acp_uint32_t ) 
    MTC_INT_BYTE_ASSIGN( &sValue2.min_sec_mic,
                         ((acp_uint8_t*)aValueInfo2->value) +
                         sizeof(acp_sint16_t) +
                         sizeof(acp_sint16_t) );

    sNull2 = MTD_DATE_IS_NULL( &sValue2 );

    //-------
    // Compare
    //-------
    
    if( !sNull1 && !sNull2 )
    {
        return mtdDateInterfaceCompare(&sValue2, &sValue1);
    }
    if( sNull1 && !sNull2 )
    {
        return 1;
    }
    if( !sNull1 /*&& sNull2*/ )
    {
        return -1;
    }
    return 0;
}


void mtdEndian( void* aValue )
{
    
    acp_uint8_t* sValue;
    acp_uint8_t  sIntermediate;
    
    sValue = (acp_uint8_t*)&(((mtdDateType*)aValue)->year);
    
    sIntermediate = sValue[0];
    sValue[0]     = sValue[1];
    sValue[1]     = sIntermediate;

    sValue = (acp_uint8_t*)&(((mtdDateType*)aValue)->mon_day_hour);
    
    sIntermediate = sValue[0];
    sValue[0]     = sValue[1];
    sValue[1]     = sIntermediate;
    
    sValue = (acp_uint8_t*)&(((mtdDateType*)aValue)->min_sec_mic);

    sIntermediate = sValue[0];
    sValue[0]     = sValue[3];
    sValue[3]     = sIntermediate;
    sIntermediate = sValue[1];
    sValue[1]     = sValue[2];
    sValue[2]     = sIntermediate;

}


ACI_RC mtdValidate( mtcColumn*   aColumn,
                    void*        aValue,
                    acp_uint32_t aValueSize)
{
/***********************************************************************
 *
 * Description : value�� semantic �˻� �� mtcColum �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    mtdDateType * sVal = (mtdDateType*)aValue;

    ACI_TEST_RAISE( sVal == NULL, ERR_INVALID_NULL );

    ACI_TEST_RAISE( aValueSize != sizeof(mtdDateType), ERR_INVALID_LENGTH );

    // �ʱ�ȭ�� aColumn�� cannonize() �ÿ� ���
    // �̶�, data type module�� precision �������� ����ϹǷ�,
    // language ���� ������ �ʿ����
    ACI_TEST( mtcInitializeColumn( aColumn,
                                   & mtcdDate,
                                   0,   // arguments
                                   0,   // precision
                                   0 )  // scale
              != ACI_SUCCESS );

    if( mtdIsNull( aColumn, aValue, MTD_OFFSET_USELESS ) != ACP_TRUE )
    {
        ACI_TEST_RAISE( mtdDateInterfaceMonth(sVal)       <  1  ||
                        mtdDateInterfaceMonth(sVal)       > 12  ||
                        mtdDateInterfaceDay(sVal)         <  1  ||
                        mtdDateInterfaceDay(sVal)         > 31  ||
                        mtdDateInterfaceHour(sVal)        > 23  ||
                        mtdDateInterfaceMinute(sVal)      > 59  ||
                        mtdDateInterfaceMicroSecond(sVal) > 999999,
                        ERR_INVALID_VALUE);

        if( mtdDateInterfaceMonth(sVal) ==  4 ||
            mtdDateInterfaceMonth(sVal) ==  6 ||
            mtdDateInterfaceMonth(sVal) ==  9 ||
            mtdDateInterfaceMonth(sVal) == 11 )
        {
            ACI_TEST_RAISE( mtdDateInterfaceDay(sVal) == 31,
                            ERR_INVALID_VALUE );
        }
        else if( mtdDateInterfaceMonth(sVal) == 2 )
        {
            if ( mtdDateInterfaceIsLeapYear( mtdDateInterfaceYear( sVal ) ) == ACP_TRUE )
            {
                ACI_TEST_RAISE( mtdDateInterfaceDay(sVal) > 29,
                                ERR_INVALID_VALUE );
            }
            else
            {
                ACI_TEST_RAISE( mtdDateInterfaceDay(sVal) > 28,
                                ERR_INVALID_VALUE );
            }
        }

        /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
        ACI_TEST_RAISE( ( mtdDateInterfaceYear( sVal ) == 1582 ) &&
                        ( mtdDateInterfaceMonth( sVal ) == 10 ) &&
                        ( 4 < mtdDateInterfaceDay( sVal ) ) && ( mtdDateInterfaceDay( sVal ) < 15 ),
                        ERR_INVALID_VALUE );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_NULL);
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE);
    }
    ACI_EXCEPTION( ERR_INVALID_LENGTH );
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_LENGTH);
    }
    ACI_EXCEPTION( ERR_INVALID_VALUE );
    {
        aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_double_t mtdSelectivityDate( void* aColumnMax,
                                 void* aColumnMin,
                                 void* aValueMax,
                                 void* aValueMin )
{
/***********************************************************************
 *
 * Description :
 *    DATE �� Selectivity ���� �Լ�
 *
 * Implementation :
 *
 *    Selectivity = (aValueMax - aValueMin) / (aColumnMax - aColumnMin)
 *    0 < Selectivity <= 1 �� ���� ������
 *
 ***********************************************************************/
    

    mtdDateType*    sColumnMax;
    mtdDateType*    sColumnMin;
    mtdDateType*    sValueMax;
    mtdDateType*    sValueMin;
    acp_double_t    sSelectivity;
    acp_double_t    sDenominator;  // �и�
    acp_double_t    sNumerator;    // ���ڰ�
    mtdIntervalType sInterval1;
    mtdIntervalType sInterval2;
    mtdIntervalType sInterval3;
    mtdIntervalType sInterval4;
    mtdValueInfo    sValueInfo1;
    mtdValueInfo    sValueInfo2;
    mtdValueInfo    sValueInfo3;
    mtdValueInfo    sValueInfo4;

    
    sColumnMax = (mtdDateType*) aColumnMax;
    sColumnMin = (mtdDateType*) aColumnMin;
    sValueMax  = (mtdDateType*) aValueMax;
    sValueMin  = (mtdDateType*) aValueMin;
    
    //------------------------------------------------------
    // Data�� ��ȿ�� �˻�
    //------------------------------------------------------

    if ( ( mtdIsNull( NULL, aColumnMax, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aColumnMin, MTD_OFFSET_USELESS ) == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aValueMax, MTD_OFFSET_USELESS )  == ACP_TRUE ) ||
         ( mtdIsNull( NULL, aValueMin, MTD_OFFSET_USELESS )  == ACP_TRUE ) )
    {
        // Data�� NULL �� ���� ���
        // �ε�ȣ�� Default Selectivity�� 1/3�� Setting��
        sSelectivity = MTD_DEFAULT_SELECTIVITY;
    }
    else
    {
        //------------------------------------------------------
        // ��ȿ�� �˻�
        // ������ ���� ������ �߸��� ��� �����̰ų� �Է� ������.
        // Column�� Min������ Value�� Max���� ���� ���
        // Column�� Max������ Value�� Min���� ū ���
        //------------------------------------------------------

        sValueInfo1.column = NULL;
        sValueInfo1.value  = sColumnMin;
        sValueInfo1.flag   = MTD_OFFSET_USELESS;

        sValueInfo2.column = NULL;
        sValueInfo2.value  = sValueMax;
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sValueInfo3.column = NULL;
        sValueInfo3.value  = sColumnMax;
        sValueInfo3.flag   = MTD_OFFSET_USELESS;

        sValueInfo4.column = NULL;
        sValueInfo4.value  = sValueMin;
        sValueInfo4.flag   = MTD_OFFSET_USELESS;
        

        if ( ( mtdDateLogicalComp( &sValueInfo1,
                                   &sValueInfo2 ) > 0 )
             ||
             ( mtdDateLogicalComp( &sValueInfo3,
                                   &sValueInfo4 ) < 0 ) )
        {
            // To Fix PR-11858
            sSelectivity = 0;
        }
        else
        {
            //------------------------------------------------------
            // Value�� ����
            // Value�� Min���� Column�� Min������ �۴ٸ� ����
            // Value�� Max���� Column�� Max������ ũ�ٸ� ����
            //------------------------------------------------------

            sValueInfo1.column = NULL;
            sValueInfo1.value  = sValueMin;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = NULL;
            sValueInfo2.value  = sColumnMin;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            if ( mtdDateLogicalComp( &sValueInfo1,
                                     &sValueInfo2 ) < 0 )
            {
                sValueMin = sColumnMin;
            }
            else
            {
                // Nothing To Do
            }

            
            sValueInfo1.column = NULL;
            sValueInfo1.value  = sValueMax;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            sValueInfo2.column = NULL;
            sValueInfo2.value  = sColumnMax;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            if ( mtdDateLogicalComp( &sValueInfo1,
                                     &sValueInfo2 ) > 0 )
            {
                sValueMax = sColumnMax;
            }
            else
            {
                // Nothing To Do
            }

            //------------------------------------------------------
            // �и� (aColumnMax - aColumnMin) �� ȹ��
            //------------------------------------------------------

            // Date(aColumnMax) -> interval1 �� ��ȯ
            // Date(aColumnMin) -> interval2 �� ��ȯ
            // Date(aValueMax) -> interval1 �� ��ȯ
            // Date(aValueMin) -> interval2 �� ��ȯ
            if ( ( mtdDateInterfaceConvertDate2Interval( sColumnMax,
                                                         &sInterval1 )
                   == ACI_SUCCESS ) &&
                 ( mtdDateInterfaceConvertDate2Interval( sColumnMin,
                                                         &sInterval2 )
                   == ACI_SUCCESS ) &&
                 ( mtdDateInterfaceConvertDate2Interval( sValueMax,
                                                         &sInterval3 )
                   == ACI_SUCCESS ) &&
                 ( mtdDateInterfaceConvertDate2Interval( sValueMin,
                                                         &sInterval4 )
                   == ACI_SUCCESS ) )
            {
                // �и��� Interval ���
                sInterval1.second      -= sInterval2.second;
                sInterval1.microsecond -= sInterval2.microsecond;
        
                if( sInterval1.microsecond < 0 )
                {
                    sInterval1.second--;
                    sInterval1.microsecond += 1000000;
                }
                if( sInterval1.microsecond >= 1000000 )
                {
                    sInterval1.second++;
                    sInterval1.microsecond -= 1000000;
                }

                // Double Type���� ��ȯ
                sDenominator = (acp_double_t) sInterval1.second/864e2 +
                    (acp_double_t) sInterval1.microsecond/864e8;
        
                if ( sDenominator <= 0.0 )
                {
                    // �߸��� ��� ������ ����� ���
                    sSelectivity = MTD_DEFAULT_SELECTIVITY;
                }
                else
                {
                    //------------------------------------------------------
                    // ���ڰ� (aValueMax - aValueMin) �� ȹ��
                    //------------------------------------------------------

                    // ������ Interval ���
                    sInterval3.second      -= sInterval4.second;
                    sInterval3.microsecond -= sInterval4.microsecond;
            
                    if( sInterval3.microsecond < 0 )
                    {
                        sInterval3.second--;
                        sInterval3.microsecond += 1000000;
                    }
                    if( sInterval3.microsecond >= 1000000 )
                    {
                        sInterval3.second++;
                        sInterval3.microsecond -= 1000000;
                    }
            
                    // Double Type���� ��ȯ
                    sNumerator =
                        (acp_double_t) sInterval3.second/864e2 +
                        (acp_double_t) sInterval3.microsecond/864e8;
            
                    if ( sNumerator <= 0.0 )
                    {
                        // To Fix PR-11858
                        sSelectivity = 0;
                    }
                    else
                    {
                        //-----------------------------------------------------
                        // Selectivity ���
                        //-----------------------------------------------------
               
                        sSelectivity = sNumerator / sDenominator;
                        if ( sSelectivity > 1.0 )
                        {
                            sSelectivity = 1.0;
                        }
                    }
                }
            }
            // Date -> Interval�� ������ ���ٸ�
            else
            {
                sSelectivity = 0.0;
            }

        }
    }
    
    return sSelectivity;

}
    
ACI_RC mtdEncode( mtcColumn*    aColumn,
                  void*         aValue,
                  acp_uint32_t  aValueSize,
                  acp_uint8_t*  aCompileFmt,
                  acp_uint32_t  aCompileFmtLen,
                  acp_uint8_t*  aText,
                  acp_uint32_t* aTextLen,
                  ACI_RC*       aRet )
{
    acp_uint32_t          sStringLen;

    ACP_UNUSED(aColumn);
    ACP_UNUSED(aValueSize);
    
    //----------------------------------
    // Parameter Validation
    //----------------------------------

    ACE_ASSERT( aValue != NULL );
    ACE_ASSERT( aCompileFmt != NULL );
    ACE_ASSERT( aCompileFmtLen > 0 );
    ACE_ASSERT( aText != NULL );
    ACE_ASSERT( *aTextLen > 0 );
    ACE_ASSERT( aRet != NULL );
    
    //----------------------------------
    // Initialization
    //----------------------------------

    aText[0] = '\0';
    sStringLen = 0;
    
    //----------------------------------
    // Set String
    //----------------------------------

    // To Fix BUG-16801
    if ( mtdIsNull( NULL, aValue, MTD_OFFSET_USELESS ) == ACP_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        // PROJ-1618
        ACI_TEST( mtdDateInterfaceToChar( (mtdDateType*) aValue,
                                          aText,
                                          & sStringLen,
                                          *aTextLen,
                                          aCompileFmt,
                                          aCompileFmtLen )
                  != ACI_SUCCESS );
    }
    
    //----------------------------------
    // Finalization
    //----------------------------------

    aText[sStringLen] = '\0';
    *aTextLen = sStringLen;
    
    *aRet = ACI_SUCCESS;
    
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;
    
    *aRet = ACI_FAILURE;
    
    return ACI_FAILURE;
}


ACI_RC mtdDecode( mtcTemplate*  aTemplate,
                  mtcColumn*    aColumn,
                  void*         aValue,
                  acp_uint32_t* aValueSize,
                  acp_uint8_t*  aCompileFmt,
                  acp_uint32_t  aCompileFmtLen,
                  acp_uint8_t*  aText,
                  acp_uint32_t  aTextLen,
                  ACI_RC*       aRet )
{

    acp_uint32_t i;
    acp_uint16_t sLen;
    acp_uint8_t* sCPtr    = aCompileFmt;
    acp_uint8_t* sCFence  = aCompileFmt + aCompileFmtLen;
    acp_uint8_t* sTPtr    = aText;
    acp_uint8_t* sTFence  = aText + aTextLen;
    mtdDateType* sDateVal = (mtdDateType*)aValue;
    acp_sint32_t sSign    = 1;
    acp_sint64_t sResult;

    ACP_UNUSED(aTemplate);
    ACP_UNUSED(aColumn);

    ACI_TEST_RAISE((sDateVal == NULL) || (*aValueSize < sizeof(mtdDateType)),
                   ERR_BUFFER_OVERFLOW);

    acpMemSet((void*)sDateVal, 0x00, sizeof(mtdDateType));

    while(sCPtr < sCFence)
    {
        if(*sCPtr == '%')
        {
            if(sCPtr[1] == '%')
            {
                ACI_TEST_RAISE(sCPtr + 1 >= sCFence, ERR_INVALID_FMT);
                ACI_TEST_RAISE(*sTPtr == '%', ERR_DATA_FMT_MISMATCH);
                sCPtr += 2;
                sTPtr++;
            }
            else if(sCPtr[1] >= '0' && sCPtr[1] <= '9')
            {
                sLen = sCPtr[1] - '0';
                ACI_TEST_RAISE(sCPtr + 2 >= sCFence, ERR_INVALID_FMT);
                if( sCPtr[2] == 'Y' )
                {
                    if(sLen == 2)
                    {
                        ACI_TEST_RAISE(sTPtr + 2 > sTFence,
                                       ERR_DATA_FMT_MISMATCH);

                        acpCStrToInt64((acp_char_t*)sTPtr,
                                       acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                       &sSign,
                                       (acp_uint64_t*)&sResult,
                                       10,
                                       (acp_char_t**)&sTPtr);
                        
                        sResult *= sSign;

                        ACI_TEST(
                            mtdDateInterfaceSetYear( sDateVal,
                                                     1900 + sResult)
                            != ACI_SUCCESS );
                        
/*                      ACI_TEST(
                        mtdDateInterfaceSetYear( sDateVal,
                        1900 + idlOS::strtol(
                        (acp_char_t*)sTPtr,
                        (acp_char_t**)&sTPtr,
                        10) )
                        != ACI_SUCCESS );*/
                        
                        ACI_TEST_RAISE(mtdDateInterfaceYear(sDateVal) > 2000,
                                       ERR_DATA_FMT_MISMATCH);
                    }
                    else if(sLen == 4)
                    {
                        ACI_TEST_RAISE(sTPtr + 4 > sTFence, ERR_INVALID_FMT);
                        acpCStrToInt64((acp_char_t*)sTPtr,
                                       acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                       &sSign,
                                       (acp_uint64_t*)&sResult,
                                       10,
                                       (acp_char_t**)&sTPtr);
                        
                        sResult *= sSign;

                        ACI_TEST(
                            mtdDateInterfaceSetYear( sDateVal,
                                                     sResult)
                            != ACI_SUCCESS );
                        /*   ACI_TEST(
                             mtdDateInterfaceSetYear( sDateVal,
                             idlOS::strtol(
                             (acp_char_t*)sTPtr,
                             (acp_char_t**)&sTPtr,
                             10) )
                             != ACI_SUCCESS );*/
                    }
                    else
                    {
                        ACI_RAISE(ERR_INVALID_FMT);
                    }
                }
                else if( sCPtr[2] == 'M' )
                {
                    if(sLen == 2)
                    {
                        ACI_TEST_RAISE(sTPtr + 2 > sTFence, ERR_INVALID_FMT);
                        acpCStrToInt64((acp_char_t*)sTPtr,
                                       acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                       &sSign,
                                       (acp_uint64_t*)&sResult,
                                       10,
                                       (acp_char_t**)&sTPtr);
                        
                        sResult *= sSign;

                        ACI_TEST(
                            mtdDateInterfaceSetMonth( sDateVal,
                                                      sResult)
                            != ACI_SUCCESS );
/*                        ACI_TEST(
                          mtdDateInterfaceSetMonth( sDateVal,
                          idlOS::strtol(
                          (acp_char_t*)sTPtr,
                          (acp_char_t**)&sTPtr,
                          10) )
                          != ACI_SUCCESS );*/
                    }
                    else if(sLen == 3)
                    {
                        ACI_TEST_RAISE(sTPtr + 3 > sTFence, ERR_INVALID_FMT);
                        for(i = 0; i < 12; i++)
                        {
                            if(acpCStrCmp((acp_char_t*)MONTHS[i],
                                          (acp_char_t*)sTPtr,
                                          3) == 0)
                            {
                                break;
                            }
                        }
                        ACI_TEST_RAISE(i == 12, ERR_INVALID_FMT);
                        ACI_TEST(
                            mtdDateInterfaceSetMonth(sDateVal, i + 1)
                            != ACI_SUCCESS );
                        sTPtr += 3; 
                    }
                    else
                    {
                        ACI_RAISE(ERR_INVALID_FMT);
                    }
                }
                else if( sCPtr[2] == 'D' )
                {
                    ACI_TEST_RAISE(sLen != 2, ERR_INVALID_FMT);
                    ACI_TEST_RAISE(sTPtr + 2 > sTFence,
                                   ERR_INVALID_FMT);
                    acpCStrToInt64((acp_char_t*)sTPtr,
                                   acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                   &sSign,
                                   (acp_uint64_t*)&sResult,
                                   10,
                                   (acp_char_t**)&sTPtr);
                        
                    sResult *= sSign;

                    ACI_TEST(
                        mtdDateInterfaceSetDay( sDateVal,
                                                sResult)
                        != ACI_SUCCESS );
/*                    ACI_TEST(
                      mtdDateInterfaceSetDay( sDateVal,
                      idlOS::strtol(
                      (acp_char_t*)sTPtr,
                      (acp_char_t**)&sTPtr,
                      10) )
                      != ACI_SUCCESS );*/
                }
                else if( sCPtr[2] == 'H' )
                {
                    ACI_TEST_RAISE(sLen != 2, ERR_INVALID_FMT);
                    ACI_TEST_RAISE(sTPtr + 2 > sTFence,
                                   ERR_INVALID_FMT);
                    acpCStrToInt64((acp_char_t*)sTPtr,
                                   acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                   &sSign,
                                   (acp_uint64_t*)&sResult,
                                   10,
                                   (acp_char_t**)&sTPtr);
                        
                    sResult *= sSign;

                    ACI_TEST(
                        mtdDateInterfaceSetHour( sDateVal,
                                                 sResult)
                        != ACI_SUCCESS );
/*                    ACI_TEST(
                      mtdDateInterfaceSetHour( sDateVal,
                      idlOS::strtol(
                      (acp_char_t*)sTPtr,
                      (acp_char_t**)&sTPtr,
                      10) )
                      != ACI_SUCCESS );*/
                }
                else if( sCPtr[2] == 'm' )
                {
                    ACI_TEST_RAISE(sLen != 2, ERR_INVALID_FMT);
                    ACI_TEST_RAISE(sTPtr + 2 > sTFence,
                                   ERR_INVALID_FMT);
                    acpCStrToInt64((acp_char_t*)sTPtr,
                                   acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                   &sSign,
                                   (acp_uint64_t*)&sResult,
                                   10,
                                   (acp_char_t**)&sTPtr);
                        
                    sResult *= sSign;

                    ACI_TEST(
                        mtdDateInterfaceSetMinute( sDateVal,
                                                   sResult)
                        != ACI_SUCCESS );
/*                    ACI_TEST(
                      mtdDateInterfaceSetMinute( sDateVal,
                      idlOS::strtol(
                      (acp_char_t*)sTPtr,
                      (acp_char_t**)&sTPtr,
                      10) )
                      != ACI_SUCCESS );*/
                }
                else if( sCPtr[2] == 'S' )
                {
                    ACI_TEST_RAISE(sLen != 2, ERR_INVALID_FMT);
                    ACI_TEST_RAISE(sTPtr + 2 > sTFence,
                                   ERR_INVALID_FMT);
                    acpCStrToInt64((acp_char_t*)sTPtr,
                                   acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                   &sSign,
                                   (acp_uint64_t*)&sResult,
                                   10,
                                   (acp_char_t**)&sTPtr);
                        
                    sResult *= sSign;

                    ACI_TEST(
                        mtdDateInterfaceSetSecond( sDateVal,
                                                   sResult)
                        != ACI_SUCCESS );
/*                    ACI_TEST(
                      mtdDateInterfaceSetSecond( sDateVal,
                      idlOS::strtol(
                      (acp_char_t*)sTPtr,
                      (acp_char_t**)&sTPtr,
                      10) )
                      != ACI_SUCCESS );*/
                }
                else if( sCPtr[2] == 'U' )
                {
                    ACI_TEST_RAISE(sLen != 6, ERR_INVALID_FMT);
                    ACI_TEST_RAISE(sTPtr + 6 > sTFence, ERR_INVALID_FMT);
                    acpCStrToInt64((acp_char_t*)sTPtr,
                                   acpCStrLen((acp_char_t*)sTPtr, ACP_UINT32_MAX),
                                   &sSign,
                                   (acp_uint64_t*)&sResult,
                                   10,
                                   (acp_char_t**)&sTPtr);
                        
                    sResult *= sSign;

                    ACI_TEST(
                        mtdDateInterfaceSetMicroSecond( sDateVal,
                                                        sResult)
                        != ACI_SUCCESS );
/*                    ACI_TEST(
                      mtdDateInterfaceSetMicroSecond( sDateVal,
                      idlOS::strtol(
                      (acp_char_t*)sTPtr,
                      (acp_char_t**)&sTPtr,
                      10) )
                      != ACI_SUCCESS );*/
                }
                else
                {
                    ACI_RAISE(ERR_INVALID_FMT);
                }
                sCPtr += 3;
            }
            else
            {
                ACI_RAISE(ERR_INVALID_FMT);
            }
        }
        else
        {
            ACI_TEST_RAISE(*sCPtr != *sTPtr, ERR_DATA_FMT_MISMATCH);
            sCPtr++;
            sTPtr++;
        }
    }

    *aValueSize = sizeof(mtdDateType);
    *aRet = ACI_SUCCESS;
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION(ERR_BUFFER_OVERFLOW);
    {
        *aRet = ACI_FAILURE;
        return ACI_SUCCESS;
    }
    ACI_EXCEPTION(ERR_INVALID_FMT);
    {
        aciSetErrorCode(mtERR_ABORT_CODING_INVALID_FMT);
    }
    ACI_EXCEPTION(ERR_DATA_FMT_MISMATCH);
    {
        aciSetErrorCode(mtERR_ABORT_CODING_DATA_FMT_MISMATCH);
    }
    ACI_EXCEPTION_END;

    *aRet = ACI_FAILURE;
    *aValueSize = 0;

    return ACI_FAILURE;

}


ACI_RC mtdCompileFmt( mtcColumn*    aColumn ,
                      acp_uint8_t*  aCompiledFmt,
                      acp_uint32_t* aCompiledFmtLen,
                      acp_uint8_t*  aFormatString,
                      acp_uint32_t  aFormatStringLength,
                      ACI_RC     *  aRet )
{

    acp_uint32_t sLen;
    acp_uint8_t* sFPtr   = aFormatString;
    acp_uint8_t* sFFence = aFormatString + aFormatStringLength;
    acp_uint8_t* sCPtr   = aCompiledFmt;
    acp_uint8_t* sCFence = aCompiledFmt + MTD_COMPILEDFMT_MAXLEN;

    ACP_UNUSED(aColumn);
    
    for(;sFPtr < sFFence;)
    {
        sLen = 1;
        switch(acpCharToUpper(*sFPtr))
        {
            case 'Y' :
            {
                while(&sFPtr[sLen] < sFFence)
                {
                    if(acpCharToUpper(sFPtr[sLen]) != 'Y' ||
                       sLen == 4 )
                    {
                        break;
                    }
                    sLen++;
                }
                if( sLen == 2 || sLen == 3 )
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 3,"%%2Y");
                    sLen = 2;
                }
                else if( sLen >= 4 )
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 5,"%%4Y");
                    sLen = 4;
                }
                else
                {
                    ACI_TEST( sCPtr + 2 >= sCFence );
                    *sCPtr = *sFPtr;
                    sCPtr[1] = '\0';
                }
                break;
            }
            case 'M' :
            {
                if( &sFPtr[sLen] < sFFence )
                {
                    if(acpCharToUpper(sFPtr[sLen]) == 'M')
                    {
                        ACI_TEST( sCPtr + 4 >= sCFence );
                        acpSnprintf((acp_char_t*)sCPtr, 3,"%%2M");

                        sLen = 2;
                    }
                    else if(acpCharToUpper(sFPtr[sLen]) == 'O' &&
                            &sFPtr[sLen + 1] < sFFence &&
                            acpCharToUpper(sFPtr[sLen + 1]) == 'N')
                    {
                        ACI_TEST( sCPtr + 4 >= sCFence );
                        acpSnprintf((acp_char_t*)sCPtr, 4,"%%3M");
                        sLen = 3;
                    }
                    else if(acpCharToUpper(sFPtr[sLen]) == 'I')
                    {
                        ACI_TEST( sCPtr + 4 >= sCFence );
                        acpSnprintf((acp_char_t*)sCPtr, 3,"%%2m");
                        sLen = 2;
                    }
                    else
                    {
                        ACI_TEST( sCPtr + 2 >= sCFence );
                        *sCPtr = *sFPtr;
                        sCPtr[1] = '\0';
                    }
                }
                else
                {
                    *sCPtr = *sFPtr;
                    sCPtr[1] = '\0';
                }
                break;
            }
            case 'D' :
            {
                if( &sFPtr[sLen] < sFFence  &&
                    acpCharToUpper(sFPtr[sLen]) == 'D')
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 3,"%%2D");
                    sLen = 2;
                }
                else
                {
                    ACI_TEST( sCPtr + 2 >= sCFence );
                    *sCPtr = *sFPtr;
                    sCPtr[1] = '\0';
                }
                break;
            }
            case 'H' :
            {
                if( &sFPtr[sLen] < sFFence  &&
                    acpCharToUpper(sFPtr[sLen]) == 'H')
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 3,"%%2H");
                    sLen = 2;
                }
                else
                {
                    ACI_TEST( sCPtr + 2 >= sCFence );
                    *sCPtr = *sFPtr;
                    sCPtr[1] = '\0';
                }
                break;
            }
            case 'S' :
            {
                while(&sFPtr[sLen] < sFFence)
                {
                    if(acpCharToUpper(sFPtr[sLen]) != 'S' ||
                       sLen == 6 )
                    {
                        break;
                    }
                    sLen++;
                }
                if( sLen >= 2 && sLen < 6 )
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 3,"%%2S");
                    sLen = 2;
                }
                else if( sLen >= 6 )
                {
                    ACI_TEST( sCPtr + 4 >= sCFence );
                    acpSnprintf((acp_char_t*)sCPtr, 7,"%%6U");
                    sLen = 6;
                }
                else
                {
                    ACI_TEST( sCPtr + 2 >= sCFence );
                    *sCPtr = *sFPtr;
                    sCPtr[1] = '\0';
                }
                break;
            }
            default :
            {
                ACI_TEST( sCPtr + 2 >= sCFence );
                *sCPtr = *sFPtr;
                sCPtr[1] = '\0';
                break;
            }
        } // switch
        sFPtr += sLen;
        sCPtr += acpCStrLen((acp_char_t*)sCPtr, ACP_UINT32_MAX);
    } // for

    *aCompiledFmtLen = sCPtr - aCompiledFmt;
    *aRet = ACI_SUCCESS;
    
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    *aCompiledFmtLen = 0;
    *aRet = ACI_FAILURE;

    return ACI_SUCCESS;
    
}

ACI_RC mtdValueFromOracle( mtcColumn*    aColumn,
                           void*         aValue,
                           acp_uint32_t* aValueOffset,
                           acp_uint32_t  aValueSize,
                           const void*   aOracleValue,
                           acp_sint32_t  aOracleLength,
                           ACI_RC*       aResult )
{
    acp_uint32_t       sValueOffset;
    mtdDateType*       sValue;
    const acp_uint8_t* sOracleValue;
    
    sValueOffset = ACP_ALIGN_ALL( *aValueOffset, MTD_DATE_ALIGN );
    
    if( sValueOffset + MTD_DATE_SIZE <= aValueSize )
    {
        sValue = (mtdDateType*)((acp_uint8_t*)aValue+sValueOffset);
        aColumn->column.offset = sValueOffset;
            
        if( aOracleLength >= 0 )
        {
            ACI_TEST_RAISE( aOracleLength != 7, ERR_INVALID_LENGTH );
            
            sOracleValue = (const acp_uint8_t*)aOracleValue;

            ACI_TEST( mtdDateInterfaceMakeDate(sValue,
                                               ( (acp_uint16_t)sOracleValue[0] - 100 ) * 100
                                               + (acp_uint16_t)sOracleValue[1] - 100,
                                               sOracleValue[2],
                                               sOracleValue[3],
                                               sOracleValue[4] - 1,
                                               sOracleValue[5] - 1,
                                               sOracleValue[6] - 1,
                                               0 )
                      != ACI_SUCCESS );
        }
        else
        {
            *sValue = mtcdDateNull;
        }
        
        ACI_TEST( mtdValidate( aColumn, sValue, MTD_DATE_SIZE )
                  != ACI_SUCCESS );
    
        *aValueOffset = sValueOffset + aColumn->column.size;
        
        *aResult = ACI_SUCCESS;
    }
    else
    {
        *aResult = ACI_FAILURE;
    }
    
    return ACI_SUCCESS;
    
    ACI_EXCEPTION(ERR_INVALID_LENGTH);
    aciSetErrorCode(mtERR_ABORT_INVALID_LENGTH);
    
    ACI_EXCEPTION_END;

    *aResult = ACI_FAILURE;
    
    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetYear(mtdDateType* aDate, acp_sint16_t aYear)
{
    ACE_ASSERT( aDate != NULL );

    aDate->year = aYear;

    return ACI_SUCCESS;
}

ACI_RC mtdDateInterfaceSetMonth(mtdDateType* aDate, acp_uint8_t aMonth)
{
    acp_uint16_t sMonth = (acp_uint16_t)aMonth;

    ACE_ASSERT( aDate != NULL );

    ACI_TEST_RAISE( ( sMonth < 1 ) || ( sMonth > 12 ),
                    ERR_INVALID_MONTH );

    aDate->mon_day_hour &= ~MTD_DATE_MON_MASK;
    aDate->mon_day_hour |= sMonth << MTD_DATE_MON_SHIFT;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_MONTH);
    aciSetErrorCode(mtERR_ABORT_INVALID_MONTH);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetDay(mtdDateType* aDate, acp_uint8_t aDay)
{
    acp_uint16_t sDay = (acp_uint16_t)aDay;

    ACE_ASSERT( aDate != NULL );

    ACI_TEST_RAISE( ( sDay < 1 ) || ( sDay > 31 ),
                    ERR_INVALID_DAY );

    aDate->mon_day_hour &= ~MTD_DATE_DAY_MASK;
    aDate->mon_day_hour |= sDay << MTD_DATE_DAY_SHIFT;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_DAY);
    aciSetErrorCode(mtERR_ABORT_INVALID_DAY);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetHour(mtdDateType* aDate, acp_uint8_t aHour)
{
    acp_uint16_t sHour = (acp_uint16_t)aHour;

    ACE_ASSERT( aDate != NULL );

    // acp_uint8_t�̹Ƿ� lower bound�� �˻����� ����
    ACI_TEST_RAISE( sHour > 23,
                    ERR_INVALID_HOUR24 );

    aDate->mon_day_hour &= ~MTD_DATE_HOUR_MASK;
    aDate->mon_day_hour |= sHour;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_HOUR24);
    aciSetErrorCode(mtERR_ABORT_DATE_INVALID_HOUR24);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetMinute(mtdDateType* aDate, acp_uint8_t aMinute)
{
    acp_uint32_t sMinute = (acp_uint32_t)aMinute;

    ACE_ASSERT( aDate != NULL );

    // acp_uint8_t�̹Ƿ� lower bound�� �˻����� ����
    ACI_TEST_RAISE( sMinute > 59,
                    ERR_INVALID_MINUTE );

    aDate->min_sec_mic &= ~MTD_DATE_MIN_MASK;
    aDate->min_sec_mic |= sMinute << MTD_DATE_MIN_SHIFT;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_MINUTE);
    aciSetErrorCode(mtERR_ABORT_INVALID_MINUTE);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetSecond(mtdDateType* aDate, acp_uint8_t aSec)
{
    acp_uint32_t sSec = (acp_uint32_t)aSec;

    ACE_ASSERT( aDate != NULL );

    // acp_uint8_t�̹Ƿ� lower bound�� �˻����� ����
    ACI_TEST_RAISE( sSec > 59,
                    ERR_INVALID_SECOND );

    aDate->min_sec_mic &= ~MTD_DATE_SEC_MASK;
    aDate->min_sec_mic |= sSec << MTD_DATE_SEC_SHIFT;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_SECOND);
    aciSetErrorCode(mtERR_ABORT_INVALID_SECOND);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceSetMicroSecond(mtdDateType* aDate, acp_uint32_t aMicroSec)
{
    ACE_ASSERT( aDate != NULL );

    // acp_uint32_t�̹Ƿ� lower bound�� �˻����� ����
    ACI_TEST_RAISE( aMicroSec > 999999,
                    ERR_INVALID_MICROSECOND );

    aDate->min_sec_mic &= ~MTD_DATE_MSEC_MASK;
    aDate->min_sec_mic |= aMicroSec;

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_MICROSECOND);
    aciSetErrorCode(mtERR_ABORT_INVALID_MICROSECOND);
    
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceAddMonth( mtdDateType* aResult, mtdDateType* aDate, acp_sint64_t aNumber )
{

    acp_uint8_t   sStartLastDays[13] = { 0, 31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31 };

    acp_uint8_t   sEndLastDays[13] = { 0, 31, 28, 31, 30, 31, 30,
                                       31, 31, 30, 31, 30, 31 };

    acp_sint64_t  sStartYear;
    acp_sint64_t  sStartMonth;
    acp_sint64_t  sStartDay;
    acp_sint64_t  sStartHour;
    acp_sint64_t  sStartMinute;
    acp_sint64_t  sStartSecond;
    acp_sint64_t  sStartMicroSecond;

    acp_sint64_t  sEndYear;
    acp_sint64_t  sEndMonth;
    acp_sint64_t  sEndDay;
    acp_sint64_t  sEndHour;
    acp_sint64_t  sEndMinute;
    acp_sint64_t  sEndSecond;
    acp_sint64_t  sEndMicroSecond;

    sStartYear = mtdDateInterfaceYear(aDate);
    sStartMonth = mtdDateInterfaceMonth(aDate);
    sStartDay = mtdDateInterfaceDay(aDate);
    sStartHour = mtdDateInterfaceHour(aDate);
    sStartMinute = mtdDateInterfaceMinute(aDate);
    sStartSecond = mtdDateInterfaceSecond(aDate);
    sStartMicroSecond = mtdDateInterfaceMicroSecond(aDate);

    sEndYear = sStartYear;
    sEndMonth = sStartMonth;
    sEndDay = sStartDay;
    sEndHour = sStartHour;
    sEndMinute = sStartMinute;
    sEndSecond = sStartSecond;
    sEndMicroSecond = sStartMicroSecond;

    if ( mtdDateInterfaceIsLeapYear( sStartYear ) == ACP_TRUE )
    {
        sStartLastDays[2] = 29;
    }
    else
    {
        /* Nothing to do */
    }

    sEndYear = sStartYear + ( aNumber / 12 );
    sEndMonth = sStartMonth + ( aNumber % 12 );

    if ( sEndMonth < 1 )
    {
        sEndYear--;
        sEndMonth += 12;
    }
    else if ( sEndMonth > 12 )
    {
        sEndYear++;
        sEndMonth -= 12;
    }
    else
    {
        // nothing to do
    }

    if ( mtdDateInterfaceIsLeapYear( sEndYear ) == ACP_TRUE )
    {
        sEndLastDays[2] = 29;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sStartDay == sStartLastDays[sStartMonth] ||
         sStartDay >= sEndLastDays[sEndMonth] )
    {
        sEndDay = sEndLastDays[sEndMonth];
    }
    else
    {
        /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
        if ( ( sEndYear == 1582 ) &&
             ( sEndMonth == 10 ) &&
             ( 4 < sStartDay ) && ( sStartDay < 15 ) )
        {
            sEndDay = 15;
        }
        else
        {
            sEndDay = sStartDay;
        }
    }

    if ( sEndYear < -9999 || sEndYear > 9999 )
    {
        ACI_RAISE ( ERR_INVALID_YEAR );
    }

    mtdDateInterfaceMakeDate(aResult,
                             sEndYear,
                             sEndMonth,
                             sEndDay,
                             sEndHour,
                             sEndMinute,
                             sEndSecond,
                             sEndMicroSecond );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC mtdDateInterfaceAddDay( mtdDateType* aResult, mtdDateType* aDate, acp_sint64_t aNumber )
{
    mtdIntervalType sInterval = { ACP_SINT64_LITERAL(0), ACP_SINT64_LITERAL(0) };

    /* BUG-36296 Date�� Interval�� ��ȯ�Ͽ� �۾��ϰ�, �ٽ� Date�� ��ȯ�Ѵ�. */
    ACI_TEST( mtdDateInterfaceConvertDate2Interval( aDate,
                                                    &sInterval )
              != ACI_SUCCESS );

    sInterval.second      += (aNumber * ACP_SINT64_LITERAL(86400));

    sInterval.microsecond += (sInterval.second * 1000000);

    sInterval.second      = sInterval.microsecond / 1000000;
    sInterval.microsecond = sInterval.microsecond % 1000000;

    ACI_TEST_RAISE( ( sInterval.second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                    ( sInterval.second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                    ( ( sInterval.second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                      ( sInterval.microsecond < 0 ) ),
                    ERR_INVALID_YEAR );

    ACI_TEST( mtdDateInterfaceConvertInterval2Date( &sInterval,
                                                    aResult )
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC mtdDateInterfaceAddSecond( mtdDateType* aResult,
                                  mtdDateType* aDate,
                                  acp_sint64_t aSecond,
                                  acp_sint64_t aMicroSecond )
{
    mtdIntervalType sInterval = { ACP_SINT64_LITERAL(0), ACP_SINT64_LITERAL(0) };

    /* BUG-36296 Date�� Interval�� ��ȯ�Ͽ� �۾��ϰ�, �ٽ� Date�� ��ȯ�Ѵ�. */
    ACI_TEST( mtdDateInterfaceConvertDate2Interval( aDate,
                                                    &sInterval )
              != ACI_SUCCESS );

    sInterval.second      += aSecond;
    sInterval.microsecond += aMicroSecond;

    sInterval.microsecond += (sInterval.second * 1000000);

    sInterval.second      = sInterval.microsecond / 1000000;
    sInterval.microsecond = sInterval.microsecond % 1000000;

    ACI_TEST_RAISE( ( sInterval.second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                    ( sInterval.second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                    ( ( sInterval.second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                      ( sInterval.microsecond < 0 ) ),
                    ERR_INVALID_YEAR );

    ACI_TEST( mtdDateInterfaceConvertInterval2Date( &sInterval,
                                                    aResult )
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC mtdDateInterfaceAddMicroSecond( mtdDateType* aResult, mtdDateType* aDate, acp_sint64_t aNumber )
{
    mtdIntervalType sInterval = { ACP_SINT64_LITERAL(0), ACP_SINT64_LITERAL(0) };

    /* BUG-36296 Date�� Interval�� ��ȯ�Ͽ� �۾��ϰ�, �ٽ� Date�� ��ȯ�Ѵ�. */
    ACI_TEST( mtdDateInterfaceConvertDate2Interval( aDate,
                                                    &sInterval )
              != ACI_SUCCESS );

    sInterval.microsecond += aNumber;

    sInterval.microsecond += (sInterval.second * 1000000);

    sInterval.second      = sInterval.microsecond / 1000000;
    sInterval.microsecond = sInterval.microsecond % 1000000;

    ACI_TEST_RAISE( ( sInterval.second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                    ( sInterval.second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                    ( ( sInterval.second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                      ( sInterval.microsecond < 0 ) ),
                    ERR_INVALID_YEAR );

    ACI_TEST( mtdDateInterfaceConvertInterval2Date( &sInterval,
                                                    aResult )
              != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC
mtdDateInterfaceMakeDate( mtdDateType* aDate,
                          acp_sint16_t aYear,
                          acp_uint8_t  aMonth,
                          acp_uint8_t  aDay,
                          acp_uint8_t  aHour,
                          acp_uint8_t  aMinute,
                          acp_uint8_t  aSec,
                          acp_uint32_t aMicroSec )
{
    // Bug-10600 UMR error
    // aDate�� �� �ɹ��� �ʱ�ȭ �ؾ� �Ѵ�.
    // by kumdory, 2005-03-04
    aDate->mon_day_hour = 0;
    aDate->min_sec_mic = 0;

    // BUG-31389
    // ��¥�� ��ȿ���� üũ �ϱ� ���ؼ� ��/��/���� �ʿ� �ϴ�.
    // �̿� ���� ��/��/�ʵ��� �ܵ����� �˻� �����ϴ�.
    ACI_TEST( mtdDateInterfaceCheckYearMonthDayAndSetDateValue( aDate,
                                                                aYear,
                                                                aMonth,
                                                                aDay )
              != ACI_SUCCESS );

    ACI_TEST( mtdDateInterfaceSetHour( aDate, aHour ) != ACI_SUCCESS );
    ACI_TEST( mtdDateInterfaceSetMinute( aDate, aMinute ) != ACI_SUCCESS );
    ACI_TEST( mtdDateInterfaceSetSecond( aDate, aSec ) != ACI_SUCCESS );
    ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, aMicroSec ) != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC mtdDateInterfaceConvertDate2Interval( mtdDateType*     aDate,
                                             mtdIntervalType* aInterval )
/***********************************************************************
 *
 * Description :
 *    Date ������ ���� Date -> Interval�� ��ȯ�ϴ� �Լ�
 *
 * Implementation :
 *    1) �ش� �� �������� �� �ϼ��� ���Ͽ� �Ϸ�� �ʼ�(86400��)�� ���Ͽ� ���Ѵ�.
 *    2) �ð������� ���� ������ �ʼ��� ���Ͽ� ���Ѵ�.
 *    3) ������ ���Ѱ͵��� ���� �����ش�.
 *    4) �׷����¸� ����Ѵٴ� �����Ͽ� ����Ѵ�.
 ***********************************************************************/
{
    ACI_TEST_RAISE( MTD_DATE_IS_NULL(aDate) == ACP_TRUE, ERR_INVALID_NULL );

    aInterval->second =
        (acp_sint64_t)mtcDayOfCommonEra( mtdDateInterfaceYear(aDate),
                                         mtdDateInterfaceMonth(aDate),
                                         mtdDateInterfaceDay(aDate) ) * ACP_SINT64_LITERAL(86400);
    aInterval->second +=
        mtdDateInterfaceHour(aDate)   * 3600
        + mtdDateInterfaceMinute(aDate) * 60
        + mtdDateInterfaceSecond(aDate);
    aInterval->microsecond = mtdDateInterfaceMicroSecond(aDate);

    /* DATE�� MicroSecond�� �׻� 0 �̻��̴�. Second�� �����̸�, MicroSecond�� ������ �����. */
    if ( ( aInterval->second < 0 ) && ( aInterval->microsecond > 0 ) )
    {
        aInterval->second++;
        aInterval->microsecond -= 1000000;
    }
    else
    {
        /* Nothing to do */
    }

    ACI_TEST_RAISE( ( aInterval->second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                    ( aInterval->second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                    ( ( aInterval->second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                      ( aInterval->microsecond < 0 ) ),
                    ERR_INVALID_YEAR );

    return ACI_SUCCESS;
    
    ACI_EXCEPTION( ERR_INVALID_NULL );
    aciSetErrorCode(mtERR_ABORT_VALIDATE_INVALID_VALUE);

    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC mtdDateInterfaceConvertInterval2Date( mtdIntervalType* aInterval,
                                             mtdDateType*     aDate )
/***********************************************************************
 *
 * Description :
 *    Date ���� �� Date�� �ٽ� ��ȯ�� ���� Interval -> Date�� ��ȯ�ϴ� �Լ�
 *
 * Implementation :
 *    1) BC���� AD���� �����Ѵ�.
 *      - Interval�� �����̸�, BC�̴�.
 *      - Interval�� 0�̸�, AD 0001�� 1�� 1�� 0�� 0���̴�.
 *
 *    2) 1582�� 10�� 4�� ������ �����콺���� �����ϰ�, 1582�� 10�� 15�� ���Ĵ� �׷������� �����Ѵ�.
 *      - 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�.
 ***********************************************************************/
{
#define DAY_PER_SECOND      86400
#define YEAR400_PER_DAY     146097
#define YEAR100_PER_DAY     36524
#define YEAR4_PER_DAY       1461

/* '1582-10-15' - '0001-01-01' */
#define FIRST_GREGORIAN_DAY ( 577737 )

    acp_sint16_t sYear          = 0;
    acp_uint8_t  sMonth         = 0;
    acp_uint8_t  sDay           = 0;
    acp_uint8_t  sHour          = 0;
    acp_uint8_t  sMinute        = 0;
    acp_uint8_t  sSecond        = 0;
    acp_sint32_t sMicroSecond   = 0;

    acp_sint32_t sDays          = 0;
    acp_sint32_t sSeconds       = 0;
    acp_sint32_t sLeap          = 0;
    acp_sint32_t s400Years      = 0;
    acp_sint32_t s100Years      = 0;
    acp_sint32_t s4Years        = 0;
    acp_sint32_t s1Years        = 0;
    acp_sint32_t sStartDayOfMonth[2][13] =
        {
            {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
            {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
        };

    ACI_TEST_RAISE( ( aInterval->second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                    ( aInterval->second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                    ( ( aInterval->second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                      ( aInterval->microsecond < 0 ) ),
                    ERR_INVALID_YEAR );

    // �ϴ����� �ʴ����� �����ش�.
    sDays    = aInterval->second / DAY_PER_SECOND;
    sSeconds = aInterval->second % DAY_PER_SECOND;

    sMicroSecond = (acp_sint32_t)aInterval->microsecond;

    /* BC by Abel.Chun */
    if ( ( sDays < 0 ) || ( sSeconds < 0 ) || ( sMicroSecond < 0 ) )
    {
        ACE_DASSERT( sDays <= 0 );
        ACE_DASSERT( sSeconds <= 0 );
        ACE_DASSERT( sMicroSecond <= 0 );

        /* DATE���� ������ ������ �����ϴ�. Second�� Microsecond�� ����� �����Ѵ�. */
        if ( sMicroSecond < 0 )
        {
            sSeconds--;
            sMicroSecond += 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sSeconds < 0 )
        {
            sDays--;
            sSeconds += DAY_PER_SECOND;
        }
        else
        {
            /* Nothing to do */
        }

        /* BUG-36296 ��������� 4�⸶�� �����̶�� �����Ѵ�.
         *  - ��� �θ��� ��ġ�� �����콺 ī�̻縣�� ����� 45����� �����콺���� �����Ͽ���.
         *  - �ʱ��� �����콺��(����� 45�� ~ ����� 8��)������ ������ 3�⿡ �� �� �ǽ��Ͽ���. (Oracle 11g ������)
         *  - BC 4713����� �����콺���� ����Ѵ�. õ�����ڵ��� �����콺���� ����Ѵ�. 4�⸶�� �����̴�.
         *
         *  AD 0���� �������� �ʴ´�. DATE�� ������ 0�̸�, BC 1���̴�. ��, BC 1���� ������ AD 1���̴�.
         *  BC 0001�� 12�� 31���� Day -1 �̴�. �׸���, BC 1��(aYear == 0)�� �����̴�.
         */
        s4Years = sDays / YEAR4_PER_DAY;
        sDays   = sDays % YEAR4_PER_DAY;
        if ( ( s4Years < 0 ) && ( sDays == 0 ) )
        {
            s4Years++;
            sDays = -YEAR4_PER_DAY;
        }
        else
        {
            /* Nothing to do */
        }

        if ( sDays < -1096 )
        {
            s1Years = -3;

            /* DATE���� ������ ������ �����ϴ�. Days�� ����� �����Ѵ�. */
            sDays = sDays + 1461;
        }
        else if ( sDays < -731 ) // && sDays >= -1096
        {
            s1Years = -2;

            /* DATE���� ������ ������ �����ϴ�. Days�� ����� �����Ѵ�. */
            sDays = sDays + 1096;
        }
        else if ( sDays < -366 ) // && sDays >= -731
        {
            s1Years = -1;

            /* DATE���� ������ ������ �����ϴ�. Days�� ����� �����Ѵ�. */
            sDays = sDays + 731;
        }
        else                     // sDays >= -366
        {
            s1Years = 0;

            /* DATE���� ������ ������ �����ϴ�. Days�� ����� �����Ѵ�. */
            sDays = sDays + 366;
        }

        sYear = s4Years * 4
              + s1Years;
    }
    else
    {
        ACE_DASSERT( sDays >= 0 );
        ACE_DASSERT( sSeconds >= 0 );
        ACE_DASSERT( sMicroSecond >= 0 );

        if ( sDays < FIRST_GREGORIAN_DAY )
        {
            /* BUG-36296 �׷������� ���� ��Ģ�� 1583����� �����Ѵ�. 1582�� �������� 4�⸶�� �����̴�. */
            s4Years = sDays / YEAR4_PER_DAY;
            sDays   = sDays % YEAR4_PER_DAY;
        }
        /* BUG-36296 �׷������� 1582�� 10�� 15�Ϻ��� �����Ѵ�. */
        else
        {
            /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
            sDays += 10;

            /* BUG-36296 1600�� ������ �����콺�°� �׷������� ������ ����. */
            if ( sDays <= ( 400 * YEAR4_PER_DAY ) )
            {
                s4Years = sDays / YEAR4_PER_DAY;
                sDays   = sDays % YEAR4_PER_DAY;
            }
            else
            {
                /* BUG-36296 �׷������� ���� ��Ģ�� 1583����� �����Ѵ�. 1582�� �������� 4�⸶�� �����̴�. */
                s4Years = 400;
                sDays  -= ( 400 * YEAR4_PER_DAY );

                s400Years = sDays / YEAR400_PER_DAY;
                sDays     = sDays % YEAR400_PER_DAY;

                s100Years = sDays / YEAR100_PER_DAY;
                sDays     = sDays % YEAR100_PER_DAY;
                if ( s100Years == 4 )
                {
                    /* 400�� �ֱ� > (100�� �ֱ� * 4)�̹Ƿ�, 100�� �ֱ� Ƚ���� �����Ѵ�. */
                    s100Years = 3;
                    sDays    += YEAR100_PER_DAY;
                }
                else
                {
                    /* Nothing to do */
                }

                s4Years += sDays / YEAR4_PER_DAY;
                sDays    = sDays % YEAR4_PER_DAY;
            }
        }

        if ( sDays >= 1095 )
        {
            s1Years = 3;
            sDays  -= 1095;
        }
        else if ( sDays >= 730 ) // && sDays < 1095
        {
            s1Years = 2;
            sDays  -= 730;
        }
        else if ( sDays >= 365 ) // && sDays < 730
        {
            s1Years = 1;
            sDays  -= 365;
        }
        else                    // sDays < 365
        {
            s1Years = 0;
        }

        sYear = s400Years * 400
              + s100Years * 100
              + s4Years   * 4
              + s1Years
              + 1; /* AD 0001����� �����Ѵ�. */
    }

    sDays++;    /* 0���� �������� �����Ƿ�, +1�� �����Ѵ�. */

    // ��, �� ���ϱ�
    if ( mtdDateInterfaceIsLeapYear( sYear ) == ACP_TRUE )
    {
        sLeap = 1;
    }
    else
    {
        sLeap = 0;
    }

    for ( sMonth = 1; sMonth < 13; sMonth++)
    {
        if ( sStartDayOfMonth[sLeap][(acp_uint32_t)sMonth] >= sDays )
        {
            break;
        }
    }

    sDay = sDays - sStartDayOfMonth[sLeap][sMonth - 1];
    
    // �ð� ���ϱ�
    sHour = sSeconds / 3600;
    sSeconds %= 3600;
    sMinute = sSeconds / 60;
    sSecond = sSeconds % 60;

    mtdDateInterfaceMakeDate(aDate,
                             sYear,
                             sMonth,
                             sDay,
                             sHour,
                             sMinute,
                             sSecond,
                             (acp_uint32_t)sMicroSecond );
    
    return ACI_SUCCESS;

    
    ACI_EXCEPTION( ERR_INVALID_YEAR );
    aciSetErrorCode(mtERR_ABORT_INVALID_YEAR);
    
    ACI_EXCEPTION_END;
    
    return ACI_FAILURE;
}

ACI_RC
mtdDateInterfaceToDateGetInteger( acp_uint8_t** aString,
                                  acp_uint32_t* aLength,
                                  acp_uint32_t  aMax,
                                  acp_uint32_t* aValue )
{ 
/***********************************************************************
 *
 * Description :
 *    string->number�� ��ȯ(format length���� ª�Ƶ� ���)
 *    YYYY, RRRR, DD, MM �� �������� ����ϴ� format�� ���� ���
 *
 * Implementation :
 *    1) ���ڷ� �� ���ڸ� ������ acp_uint32_t value�� ��ȯ.
 *       ���ڷ� ��ȯ�� �κ� ��ŭ ���ڿ� ������ �̵� �� ���� ����
 *    2) ���ڰ� �ƴ� ���ڸ� ������ break
 *    3) ���� �� ���ڵ� ��ȯ�� ���� �ʾҴٸ� error
 ***********************************************************************/
    acp_uint8_t* sInitStr;

    ACE_ASSERT( aString != NULL );
    ACE_ASSERT( aLength != NULL );
    ACE_ASSERT( aValue  != NULL );

    ACI_TEST_RAISE( *aLength < 1,
                    ERR_NOT_ENOUGH_INPUT );

    sInitStr = *aString;
    
    // ����ڰ� ������ �ִ� ���� ���� ���ڿ��� ���̿� ���Ͽ� ����
    // ���� ������
    aMax = ( *aLength < aMax ) ? *aLength : aMax;
    
    // ���ڷ� �� ���ڸ� ������ acp_uint32_t value�� ��ȯ.
    for( *aValue = 0;
         aMax    > 0;
         aMax--, (*aString)++, (*aLength)-- )
    {
        if( gInputST[**aString] == gDIGIT  )
        {
            *aValue = *aValue * 10 + **aString - '0';
        }
        else
        {
            // ���ڰ� �ƴ� ���ڸ� ������ break
            break;
        }
    }

    // ���� �� ���ڵ� ��ȯ�� ���� �ʾҴٸ� error
    ACI_TEST_RAISE( *aString == sInitStr,
                    ERR_NON_NUMERIC_INPUT );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_ENOUGH_INPUT );
    aciSetErrorCode(mtERR_ABORT_DATE_NOT_ENOUGH_INPUT);

    ACI_EXCEPTION( ERR_NON_NUMERIC_INPUT );
    aciSetErrorCode(mtERR_ABORT_DATE_NON_NUMERIC_INPUT);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC
mtdDateInterfaceToDateGetMonth( acp_uint8_t** aString,
                                acp_uint32_t* aLength,
                                acp_uint32_t* aMonth )
{ 
/***********************************************************************
 * Description : '��'�� �ش��ϴ� ���ڿ��� �а�, aMonth�� ������ ���� ����(1~12)
 *                �ǹ� �ִ� '��'�� �ǹ��ϴ� ���ڿ��� ��� �� ���� ��ŭ
 *                aString�� aLength�� ����
 * Implementation : '��'�� �ǹ��ϴ� �� ���ڿ��� ���� ���� 3�ڸ� �̸� �ؽ��Ͽ�
 *                  �� ���� �̸� �迭�� �����Ͽ� �ؽð��� ���� ��, ���ڿ� ��
 *                  ���� ���̸� ���� �ؽ� ���̺��� �������� �ʰ�, ��� ��츦 ��
 ***********************************************************************/
    acp_sint32_t       sIdx;
    acp_sint32_t       sMax;
    acp_uint32_t       sHashVal;
    acp_uint8_t        sString[10] = {0};
    acp_sint32_t       sLength;
    acp_uint32_t       sMonthLength[12] = { 7, 8, 5, 5, 3, 4,   // �ش� '��'�� �ǹ��ϴ�
                                            4, 6, 9, 7, 8, 8 }; // ���ڿ��� ����

    static acp_bool_t sHashInit = ACP_FALSE;
    
    ACE_ASSERT( aString != NULL );
    ACE_ASSERT( aLength != NULL );
    ACE_ASSERT( aMonth  != NULL );

    // ���� �ǹ��ϴ� ���ڿ��� �ּ� 3�� �̻��̾�� ��
    ACI_TEST_RAISE( *aLength < 3,
                    ERR_NOT_ENOUGH_INPUT );


    // ���� �ǹ��ϴ� ���ڿ� �� ���� �� ���� 9�� = september
    // ���� ���ڿ��� ���̿� ���Ͽ� ���� ���� ������
    sMax = ACP_MIN(*aLength, 9);
        
    // ���ڿ��� �ִ� ���̸� ���
    for( sLength = 0;
         sLength < sMax;
         sLength++)
    {
        if( gInputST[(*aString)[sLength]] != gALPHA  )
        {
            break;
        }
    }

    // ���ڿ��� �����ϰ�, ��� �빮�ڷ� ��ȯ
    acpMemCpy( sString, *aString, sLength );

    acpCStrToUpper( (acp_char_t*)sString, sLength );

    // ���ڸ� 3�ڸ����� �̿��Ͽ� �ؽ� �� ����
    sHashVal = mtcHash(mtcHashInitialValue,
                       sString,
                       3);


    if ( sHashInit == ACP_FALSE )
    {
        for ( sIdx = 0; sIdx < 12 ; sIdx ++)
        {
            gHashMonth[sIdx] =  mtcHash(mtcHashInitialValue,
                                        (const acp_uint8_t*)gMONName[sIdx],
                                        3);
        }
        sHashInit = ACP_FALSE;
    }

    for( sIdx=0; sIdx<12; sIdx++ )
    {
        if( sHashVal == gHashMonth[sIdx] )
        {
            if( acpCStrCmp( (acp_char_t*)sString,
                            gMONName[sIdx],
                            3 ) == 0 )
            {
                // �ϴ� �� 3�ڰ� ��ġ�ϹǷ� �ش� �޷� �ν��� �� ����
                // ������, ���ڰ� �ƴ� '��'�̸����� �Է��� �� �����Ƿ�
                // �����ϸ�, ��� �Է� ���ڸ� �Һ��ؾ� ��
                if( sLength >= (acp_sint32_t) sMonthLength[sIdx] )
                {
                    if( acpCStrCmp( (acp_char_t*)sString,
                                    gMONTHName[sIdx],
                                    sMonthLength[sIdx] ) == 0 )
                    {
                        // ������ ���� ���� �̸����� ��Ī�� ������ ���
                        sLength = sMonthLength[sIdx];
                    }
                    else
                    {
                        // �ٿ��� �̸����� ��Ī�� ������ ���
                        sLength = 3;
                    }
                }
                else
                {
                    // '��'���� ���� �����̰ų� 3�̾�� �Ѵ�.
                    // ���� ��� to_date( 'SEPMON','MONDAY' )�� ����
                    // �Է��� ���� �� ����
                    sLength = 3;
                }

                // ��Ī�� ���������Ƿ� ���� �����ϰ� �ݺ��� ����
                *aMonth = sIdx + 1;
                *aString += sLength;
                *aLength -= sLength;
                break;
            }
            else
            {
                // ���� '��'�� ���� �� ��
            }
        }
    }

    // ��� ��츦 �˻��ϵ��� ��Ī�Ǵ� ���ڿ��� ã�� ���ϸ� ����
    ACI_TEST_RAISE( sIdx == 12, ERR_INVALID_DATE );                    

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_ENOUGH_INPUT );
    aciSetErrorCode(mtERR_ABORT_DATE_NOT_ENOUGH_INPUT);

    ACI_EXCEPTION(ERR_INVALID_DATE);
    aciSetErrorCode(mtERR_ABORT_INVALID_DATE);
    
    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC
mtdDateInterfaceToDateGetRMMonth( acp_uint8_t** aString,
                                  acp_uint32_t* aLength,
                                  acp_uint32_t* aMonth )
{ 
/***********************************************************************
 * Description : '��'�� �ش��ϴ� �θ��� ���ڿ��� �а�, aMonth�� ������
 *                ���� ����(1~12) �ǹ� �ִ� '��'�� �ǹ��ϴ� ���ڿ���
 *                ��� �� ���� ��ŭ aString�� aLength�� ����
 * Implementation : ���� �� �ִ� ������ ��찡 3���̹Ƿ� �ݺ������� ���
 ***********************************************************************/

    acp_sint32_t   sIdx       = 0;
    acp_sint32_t   sMax;
    acp_uint8_t    sString[5] = {0};
    acp_sint32_t   sLength;

    acp_bool_t     sIsMinus   = ACP_FALSE;
    acp_bool_t     sIsEnd     = ACP_FALSE;
    acp_sint16_t   sMonth     = 0;
    acp_sint16_t   sICnt      = 0;
    acp_sint16_t   sVCnt      = 0;
    acp_sint16_t   sXCnt      = 0;

    ACE_ASSERT( aString != NULL );
    ACE_ASSERT( aLength != NULL );
    ACE_ASSERT( aMonth  != NULL );

    // ���� �ǹ��ϴ� ���ڿ��� �ּ� 1�� �̻��̾�� ��
    ACI_TEST_RAISE( *aLength < 1,
                    ERR_NOT_ENOUGH_INPUT );


    // ���� �ǹ��ϴ� ���ڿ� �� ���� �� ���� 4�� = VIII
    // ���� ���ڿ��� ���̿� ���Ͽ� ���� ���� ������
    sMax = ACP_MIN(*aLength, 4);
        
    // ���ڿ��� �ִ� ���̸� ���
    for( sLength = 0;
         sLength < sMax;
         sLength++)
    {
        if( gInputST[(*aString)[sLength]] != gALPHA  )
        {
            break;
        }
    }

    // ���ڿ��� �����ϰ�, ��� �빮�ڷ� ��ȯ
    acpMemCpy( sString, *aString, sLength );
    acpCStrToUpper( (acp_char_t*)sString, sLength );
    
    while ( ( ( sString[sIdx] == 'I' ) ||
              ( sString[sIdx] == 'V' ) ||
              ( sString[sIdx] == 'X' ) ) &&
            ( ((acp_sint32_t)sLength - ( sICnt + sVCnt + sXCnt )) > 0 ) )
    {
        if ( sIsEnd == ACP_TRUE )
        {
            ACI_RAISE( ERR_INVALID_LITERAL );
        }
                    
        if ( sString[sIdx] == 'I' )
        {
            if ( sICnt >= 3 )
            {
                ACI_RAISE( ERR_INVALID_LITERAL );
            }
            else
            {
                sMonth++;
                sICnt++;
                sIsMinus = ACP_TRUE;
            }
        }
        else if ( sString[sIdx] == 'V' )
        {
            if ( sICnt > 1 || sVCnt == 1 )
            {
                ACI_RAISE( ERR_INVALID_LITERAL );
            }

            if ( sIsMinus == ACP_TRUE )
            {
                sMonth += 3; 
                sIsMinus = ACP_FALSE;
                sIsEnd = ACP_TRUE;
            } 
            else
            {
                sMonth += 5;
            }
            sVCnt++;
        }
        else if ( sString[sIdx] == 'X' )
        {
            if ( sICnt > 1 || sXCnt == 1 )
            {
                ACI_RAISE( ERR_INVALID_LITERAL );
            }

            if ( sIsMinus == ACP_TRUE ) 
            {
                sMonth += 8; 
                sIsMinus = ACP_FALSE;
                sIsEnd = ACP_TRUE;
            }
            else
            {
                sMonth += 10;
            }
            sXCnt++;
        }
        sIdx++;
    }

    *aMonth   = sMonth;
    *aString += ( sICnt + sVCnt + sXCnt );
    *aLength -= ( sICnt + sVCnt + sXCnt );

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_LITERAL );
    aciSetErrorCode(mtERR_ABORT_INVALID_DATE);
    
    ACI_EXCEPTION( ERR_NOT_ENOUGH_INPUT );
    aciSetErrorCode(mtERR_ABORT_DATE_NOT_ENOUGH_INPUT);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}

ACI_RC
mtdDateInterfaceToDate( mtdDateType* aDate,
                        acp_uint8_t* aString,
                        acp_uint32_t aStringLen,
                        acp_uint8_t* aFormat,
                        acp_uint32_t aFormatLen )
{

    // Variables for mtldl lexer (scanner)
    acp_uint32_t     sToken;
    yyscan_t         sScanner;
    acp_bool_t       sInitScanner = ACP_FALSE;

    // Local Variable
    acp_sint32_t     sIdx;
    acp_sint32_t     sLeap         = 0;         // 0:�Ϲ�, 1:����
    acp_uint32_t     sNumber;
    acp_uint32_t     sFormatLen;
    acp_bool_t       sIsAM         = ACP_TRUE;   // �⺻�� AM
    acp_bool_t       sPrecededWHSP = ACP_FALSE;  // �Է� ���ڿ���
    // ����,��,�ٹٲ���
    // �־����� �˻�
    acp_uint32_t     sOldLen;                   // ���ڿ� ���� �� �˻� ���� ����
    acp_uint8_t*     sString;
    acp_uint32_t     sStringLen;
    acp_uint8_t*     sFormat;                   // ���� ó���ϰ� �ִ� ������ ��ġ�� ����Ŵ
    acp_uint8_t      sErrorFormat[MTC_TO_CHAR_MAX_PRECISION+1];

    // ���� ���� ���� �ߺ��� �Է��� �ִ� ��츦 ����Ͽ�
    // aDate�� ���� ���� �������� �ʰ�, �����ϱ� ���� ����
    // ������ ���˵�: 'SSSSS', 'DDD', 'DAY'
    acp_uint32_t     sHour         = 0;
    acp_uint32_t     sMinute       = 0;
    acp_uint32_t     sSecond       = 0;
    acp_uint8_t      sMonth        = 0;
    acp_uint8_t      sDay          = 0;
    acp_uint32_t     sDayOfYear    = 0;         // Day of year (1-366)

    // Date format element�� �ߺ����� �����ϴ� ���� ���� ���� ����
    // To fix BUG-14516
    acp_bool_t       sSetYY        = ACP_FALSE;
    acp_bool_t       sSetMM        = ACP_FALSE;
    acp_bool_t       sSetDD        = ACP_FALSE;
    acp_bool_t       sSetAMPM      = ACP_FALSE;
    acp_bool_t       sSetHH12      = ACP_FALSE;
    acp_bool_t       sSetHH24      = ACP_FALSE;
    acp_bool_t       sSetMI        = ACP_FALSE;
    acp_bool_t       sSetSS        = ACP_FALSE;
    acp_bool_t       sSetMS        = ACP_FALSE;  // microsecond
    acp_bool_t       sSetSID       = ACP_FALSE;  // seconds in day
//  acp_bool_t       sSetDOW       = ACP_FALSE;  // day of week (��������)
    acp_bool_t       sSetDDD       = ACP_FALSE;  // day of year

    ACE_ASSERT( aDate   != NULL );
    ACE_ASSERT( aString != NULL );
    ACE_ASSERT( aFormat != NULL );

    // aString�� aFormat�� ���� ������ �����Ѵ�.
    // To fix BUG-15087
    // ���� ������ while loop�ȿ��� ó���Ѵ�.    
    while( aStringLen > 0 )
    {
        if( gInputST[aString[aStringLen-1]] == gWHSP )
        {
            aStringLen--;
        }
        else
        {
            break;
        }
    }
    
    while( aFormatLen > 0 )
    {
        if( gInputST[aFormat[aFormatLen-1]] == gWHSP )
        {
            aFormatLen--;
        }
        else
        {
            break;
        }
    }

    sFormat       = aFormat;
    sString       = aString;
    sStringLen    = aStringLen;

    ACI_TEST_RAISE( mtddllex_init ( &sScanner ) != 0, ERR_LEX_INIT_FAILED );
    sInitScanner = ACP_TRUE;

    mtddl_scan_bytes( (yyconst char*)aFormat, aFormatLen, sScanner );

    // get first token
    sToken = mtddllex( sScanner );
    while( sToken != 0 )
    {

        // ����, ��, �ٹٲ޵��� �ִٸ� ����
        sPrecededWHSP = ACP_FALSE;
        // BUG-18788 sStringLen�� �˻��ؾ� ��
        while( sStringLen > 0 )
        {
            if ( gInputST[*sString] == gWHSP )
            {
                sString++;
                sStringLen--;
                sPrecededWHSP = ACP_TRUE;
            }
            else
            {
                break;
            }
        }

        switch( sToken )
        {
            /***********************************
             * ���� �Է��� �޴� ���
             ***********************************/
            // Year: YCYYY, RRRR, RR, SYYYY, YYYY, YYY, YY, Y
            case MTD_DATE_FORMAT_YCYYY:
            {
                // YCYYY�϶� : YYYY�� �����ϳ�
                //            ������ �ڸ��� ��ǥ(,)�� ������ ������ �Է� ����
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;

                sOldLen = sStringLen;
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            4,
                                                            &sNumber )
                          != ACI_SUCCESS );

                if( ( ( sOldLen - sStringLen ) == 1 ) && ( sStringLen > 0 ) )
                {
                    // �� �ڸ� ���ڸ� ���� ��� �Ϲ������� ','�� ����ȴ�
                    if( *sString == ',' )
                    {
                        // ���ڰ� ������ ��ǥ(,)�� �ִٸ� ������ 4�ڸ�
                        // ���� �Է����� �����Ѵ�. ���� �ٸ� ������
                        // ���� �������� �� ������, YCYYY�� �Բ� ��ǥ��
                        // �����ڷ� ����ϸ鼭 �� �Ҿ� ���ڸ��� ������
                        // �Է��ϴ� �������� �Է� ���� ������ ������
                        // ��) to_date( '9,9,9', 'YCYYY,MM,DD' )

                        // õ�� �ڸ� �Է�
                        ACI_TEST( mtdDateInterfaceSetYear( aDate, sNumber * 1000 )
                                  != ACI_SUCCESS );

                        // ','�� ������ ������ 3�ڸ�(����) �о �ϼ�
                        sOldLen = sStringLen;
                        ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                                    &sStringLen,
                                                                    3,
                                                                    &sNumber )
                                  != ACI_SUCCESS );

                        ACI_TEST_RAISE(
                            ( sOldLen - sStringLen ) != 3,
                            ERR_NOT_ENOUGH_INPUT );

                        // ������ �� �ڸ� �߰�
                        ACI_TEST( mtdDateInterfaceSetYear( aDate, mtdDateInterfaceYear(aDate) + sNumber )
                                  != ACI_SUCCESS );

                    }
                    else
                    {
                        // ������ �״�� ���� (1�ڸ� ���� ������ �ǹ�)
                        ACI_TEST( mtdDateInterfaceSetYear( aDate, sNumber )
                                  != ACI_SUCCESS );                    
                    }
                }
                else
                {
                    // �� ���� ��� ����ڰ� �Է��� �״�� ����
                    ACI_TEST( mtdDateInterfaceSetYear( aDate, sNumber )
                              != ACI_SUCCESS );                    
                }

                break;
            }

            case MTD_DATE_FORMAT_RRRR:
            {
                // RRRR�϶� : 4�ڸ� �� ���� �׸� �״��
                //           4�ڸ��� �ƴ� ��� �Ʒ��� ��Ģ�� ����
                //            RRRR < 50  �̸� 2000�� ���Ѵ�. 
                //            RRRR < 100 �̸� 1900�� ���Ѵ�. 
                //            RRRR >=100 �̸� �׸� �״��
                //     '2005' : 2005��
                //     '0005' : 5��
                //     '005'  : 2005��
                //     '05'   : 2005��
                //     '5'    : 2005��
                //     '100'  : 0100��
                //     '99'   : 1999��
                //     '50'   : 1950��
                //     '49'   : 2049��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;

                sOldLen = sStringLen;
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,                                            &sStringLen,
                                                            4,
                                                            &sNumber )
                          != ACI_SUCCESS );

                if( sOldLen - sStringLen < 4 )
                {
                    // 4�ڸ��� �ƴ� ��� �Ʒ��� ����
                    if( sNumber < 50 )
                    {
                        sNumber += 2000;
                    }
                    else if( sNumber < 100 )
                    {
                        sNumber += 1900;
                    }
                }
                else
                {
                    // 4�ڸ� �� ���� �Է°� �״��
                    // do nothing
                }

                ACI_TEST( mtdDateInterfaceSetYear( aDate, sNumber )
                          != ACI_SUCCESS );

                break;
            }

            case MTD_DATE_FORMAT_RR:
            {
                // RR�϶�   : RR < 50  �̸� 2000�� ���Ѵ�.  
                //           RR < 100 �̸� 1900�� ���Ѵ�. 
                //     '2005' : ����
                //     '005'  : ����
                //     '05'   : 2005��
                //     '5'    : 2005��
                //     '100'  : ����
                //     '99'   : 1999��
                //     '50    : 1950��
                //     '49'   : 2049��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                if( sNumber < 50 )
                {
                    ACI_TEST( mtdDateInterfaceSetYear( aDate, (acp_sint16_t)(2000 + sNumber) )
                              != ACI_SUCCESS );
                }
                else
                {
                    ACI_TEST( mtdDateInterfaceSetYear( aDate, (acp_sint16_t)(1900 + sNumber) )
                              != ACI_SUCCESS );
                }

                break;
            }

            case MTD_DATE_FORMAT_SYYYY :    /* BUG-36296 SYYYY Format ���� */
            {
                // SYYYY�� �� : '-'�� ���� ���� �����̰�, '+'/''�̸� ����̴�.
                //     '-2005' : -2006��
                //     '-0005' : -6��
                //     '-005'  : -6��
                //     '-05'   : -6��
                //     '-5'    : -6��
                //     '2005'  : 2005��
                //     '0005'  : 5��
                //     '005'   : 5��
                //     '05'    : 5��
                //     '5'     : 5��
                //     '1970'  : 1970��
                //     '70'    : 70��
                //     '170'   : 170��
                //     '7'     : 7��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;

                if ( *sString == '-' )
                {
                    // ��ȣ ���̸�ŭ �Է� ���ڿ� ����
                    sString++;
                    sStringLen--;

                    ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                                &sStringLen,
                                                                4,
                                                                &sNumber )
                              != ACI_SUCCESS );

                    ACI_TEST( mtdDateInterfaceSetYear( aDate,
                                                       -((acp_sint16_t)sNumber) )
                              != ACI_SUCCESS );
                }
                else
                {
                    if ( *sString == '+' )
                    {
                        // ��ȣ ���̸�ŭ �Է� ���ڿ� ����
                        sString++;
                        sStringLen--;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                                &sStringLen,
                                                                4,
                                                                &sNumber )
                              != ACI_SUCCESS );

                    ACI_TEST( mtdDateInterfaceSetYear( aDate,
                                                       sNumber )
                              != ACI_SUCCESS );
                }

                break;
            }

            case MTD_DATE_FORMAT_YYYY:
            {
                // YYYY�϶� : �Է°��� �״�� ���ڷ� �ٲ۴�.
                //     '2005' : 2005��
                //     '0005' : 5��
                //     '005'  : 5��
                //     '05'   : 5��
                //     '5'    : 5��
                //     '1970' : 1970��
                //     '70'   : 70��
                //     '170'  : 170��
                //     '7'    : 7��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            4,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetYear( aDate, sNumber )
                          != ACI_SUCCESS );

                break;
            }

            case MTD_DATE_FORMAT_YYY:
            {
                // YYY�϶�   : ���� 2000�� ���Ѵ�.
                //     '005'  : 2005��
                //     '05'   : 2005��
                //     '5'    : 2005��
                //     '70'   : 2070��
                //     '170'  : 2170��
                //     '7'    : 2007��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            3,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetYear( aDate, 2000 + sNumber )
                          != ACI_SUCCESS );

                break;
            }            

            case MTD_DATE_FORMAT_YY:
            {
                // YY�϶�   : ���� 2000�� ���Ѵ�.
                //     '05'   : 2005��
                //     '5'    : 2005��
                //     '99'   : 2099��
                //     '50    : 2050��
                //     '49'   : 2049��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetYear( aDate, (acp_sint16_t)(2000 + sNumber) )
                          != ACI_SUCCESS );

                break;
            }

            case MTD_DATE_FORMAT_Y:
            {
                // Y�϶�   : ���� 2000�� ���Ѵ�.
                //     '5'    : 2005��
                ACI_TEST_RAISE( sSetYY != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetYY = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            1,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetYear( aDate, (acp_sint16_t)(2000 + sNumber) )
                          != ACI_SUCCESS );

                break;
            }
            
            // Month: MM (������ �Է��ϴ� '��'�� �ǹ��ϴ� format)
            case MTD_DATE_FORMAT_MM:
            {
                ACI_TEST_RAISE( sSetMM != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMM = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetMonth( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // Day: DD    
            case MTD_DATE_FORMAT_DD:
            {
                ACI_TEST_RAISE( sSetDD != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetDD = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );
                
                ACI_TEST( mtdDateInterfaceSetDay( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // Day of year
            // note that this format element set not only month but day
            case MTD_DATE_FORMAT_DDD:
            {
                ACI_TEST_RAISE( sSetDDD != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetDDD = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            3,
                                                            &sDayOfYear )
                          != ACI_SUCCESS );
                
                // �ٸ� �Է� ��(��,��,��)�� ���� �ٸ� ������ �˻��ؾ�
                // �ϹǷ�, ��� �Է��� ����� �� ���� ���� �Է�

                break;
            }
            
            // Hour: HH, HH12, HH24
            case MTD_DATE_FORMAT_HH12:
            {
                // 12�ð� ����
                ACI_TEST_RAISE( ( sSetHH12 != ACP_FALSE ) ||
                                ( sSetHH24 != ACP_FALSE ),
                                ERR_CONFLICT_FORMAT );
                sSetHH12 = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                // aDate�� �ԷµǴ� ������ 1~24 ������,
                // ���� PM�� ��õ� ��� �Լ� ���� ���� +12
                ACI_TEST_RAISE( sNumber > 12 || sNumber < 1,
                                ERR_INVALID_HOUR );

                ACI_TEST( mtdDateInterfaceSetHour( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            case MTD_DATE_FORMAT_HH:
            case MTD_DATE_FORMAT_HH24:
            {
                // 24�ð� ����
                ACI_TEST_RAISE( ( sSetHH24 != ACP_FALSE ) ||
                                ( sSetHH12 != ACP_FALSE ) ||
                                ( sSetAMPM != ACP_FALSE ),
                                ERR_CONFLICT_FORMAT );
                sSetHH24 = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetHour( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // Minute    
            case MTD_DATE_FORMAT_MI:
            {
                ACI_TEST_RAISE( sSetMI != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMI = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetMinute( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // Second    
            case MTD_DATE_FORMAT_SS:
            {
                ACI_TEST_RAISE( sSetSS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetSS = ACP_TRUE;
                
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetSecond( aDate, (acp_uint8_t)sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // Microsecond
            case MTD_DATE_FORMAT_SSSSSS:
            {
                // microseconds
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            6,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber ) );
                break;
            }

            // millisecond 1-digit
            case MTD_DATE_FORMAT_FF1:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            1,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 1,
                                ERR_NOT_ENOUGH_INPUT );

                // microsecond�� �����ϴ� ���� millisecond����
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber * 100000 )
                          != ACI_SUCCESS );
                
                break;
            }

            // millisecond 2-digit
            case MTD_DATE_FORMAT_FF2:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            2,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 2,
                                ERR_NOT_ENOUGH_INPUT );

                // microsecond�� �����ϴ� ���� millisecond����
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber * 10000 )
                          != ACI_SUCCESS );
                
                break;
            }

            // millisecond 3-digit
            case MTD_DATE_FORMAT_FF3:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            3,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 3,
                                ERR_NOT_ENOUGH_INPUT );

                // microsecond�� �����ϴ� ���� millisecond����
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber * 1000 )
                          != ACI_SUCCESS );
                
                break;
            }

            // millisecond 3-digit, microsecond 1-digit
            case MTD_DATE_FORMAT_FF4:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            4,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 4,
                                ERR_NOT_ENOUGH_INPUT );

                // microsecond�� �����ϴ� ���� millisecond����
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber * 100 )
                          != ACI_SUCCESS );
                
                break;
            }

            // millisecond 3-digit, microsecond 2-digit
            case MTD_DATE_FORMAT_FF5:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            5,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 5,
                                ERR_NOT_ENOUGH_INPUT );

                // microsecond�� �����ϴ� ���� millisecond����
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber * 10 )
                          != ACI_SUCCESS );
                
                break;
            }

            // microsecond 6-digit
            case MTD_DATE_FORMAT_FF:
            case MTD_DATE_FORMAT_FF6:
            {
                // second and microsecond
                ACI_TEST_RAISE( sSetMS != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMS = ACP_TRUE;
                
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            6,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( (sOldLen - sStringLen) != 6,
                                ERR_NOT_ENOUGH_INPUT );

                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber )
                          != ACI_SUCCESS );
                
                break;
            }

            // both second and microsecond
            case MTD_DATE_FORMAT_SSSSSSSS:
            {
                // second and microsecond
                ACI_TEST_RAISE( ( sSetSS != ACP_FALSE ) ||
                                ( sSetMS != ACP_FALSE ),
                                ERR_CONFLICT_FORMAT );
                sSetSS = ACP_TRUE;
                sSetMS = ACP_TRUE;
                
                // fix for BUG-14067
                sOldLen = sStringLen;

                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            8,
                                                            &sNumber )
                          != ACI_SUCCESS );

                // fix for BUG-14067
                ACI_TEST_RAISE( (sOldLen - sStringLen) != 8,
                                ERR_NOT_ENOUGH_INPUT );
                
                ACI_TEST( mtdDateInterfaceSetSecond( aDate, sNumber / 1000000 )
                          != ACI_SUCCESS );
                ACI_TEST( mtdDateInterfaceSetMicroSecond( aDate, sNumber % 1000000 )
                          != ACI_SUCCESS );
                
                break;
            }
            
            // Seconds in day
            case MTD_DATE_FORMAT_SSSSS:
            {
                ACI_TEST_RAISE( sSetSID != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetSID = ACP_TRUE;

                // Seconds past midnight (0-86399)
                ACI_TEST( mtdDateInterfaceToDateGetInteger( &sString,
                                                            &sStringLen,
                                                            5,
                                                            &sNumber )
                          != ACI_SUCCESS );

                ACI_TEST_RAISE( sNumber>86399 ,
                                ERR_INVALID_SEC_IN_DAY );
                
                sHour = sNumber / 3600;
                sMinute = ( sNumber - sHour * 3600 ) / 60;
                sSecond = sNumber % 60;

                // switch���� ����� ���Ŀ�
                // ������ ������ �ð�/��/�ʰ� �ִٸ� �浹�ϴ��� �˻�
                
                break;
            }

            /***********************************
             * ����/���ĸ� ��Ÿ���� FORMAT
             ***********************************/
            case MTD_DATE_FORMAT_AM_U:
            case MTD_DATE_FORMAT_AM_UL:
            case MTD_DATE_FORMAT_AM_L:
            case MTD_DATE_FORMAT_PM_U:
            case MTD_DATE_FORMAT_PM_UL:
            case MTD_DATE_FORMAT_PM_L:
            {
                // AM or PM
                ACI_TEST_RAISE( ( sSetAMPM != ACP_FALSE ) ||
                                ( sSetHH24 != ACP_FALSE ),
                                ERR_CONFLICT_FORMAT );
                sSetAMPM = ACP_TRUE;

                if( acpCStrCaseCmp( (acp_char_t*)sString,"AM",2) == 0 )
                {
                    sIsAM = ACP_TRUE;
                }
                else if( acpCStrCaseCmp( (acp_char_t*)sString,"PM",2) == 0 )
                {
                    sIsAM = ACP_FALSE;
                }
                else
                {
                    ACI_RAISE( ERR_LITERAL_MISMATCH );
                }

                sString    += 2;
                sStringLen -= 2;
                
                // ���� aDate�� �ݿ��� loop ���� ��
                break;
            }
            
            /***********************************
             * �������� �Էµ� '��'�� ��Ÿ���� ���ڿ�
             ***********************************/
            case MTD_DATE_FORMAT_MON_U:
            case MTD_DATE_FORMAT_MON_UL:
            case MTD_DATE_FORMAT_MON_L:
            case MTD_DATE_FORMAT_MONTH_U:
            case MTD_DATE_FORMAT_MONTH_UL:
            case MTD_DATE_FORMAT_MONTH_L:
            {
                // Month
                ACI_TEST_RAISE( sSetMM != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMM = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetMonth( &sString,
                                                          &sStringLen,
                                                          &sNumber )
                          != ACI_SUCCESS );
                
                ACI_TEST( mtdDateInterfaceSetMonth( aDate, sNumber )
                          != ACI_SUCCESS );

                break;
            }
            
            // �θ��ڷ� �� ��
            case MTD_DATE_FORMAT_RM_U:
            case MTD_DATE_FORMAT_RM_L:
            {
                ACI_TEST_RAISE( sSetMM != ACP_FALSE,
                                ERR_CONFLICT_FORMAT );
                sSetMM = ACP_TRUE;

                ACI_TEST( mtdDateInterfaceToDateGetRMMonth( &sString,
                                                            &sStringLen,
                                                            &sNumber )
                          != ACI_SUCCESS );
                
                ACI_TEST( mtdDateInterfaceSetMonth( aDate,sNumber )
                          != ACI_SUCCESS );
                
                break;
            }
            
            /***********************************
             * ""�� ���� ���ڿ�
             ***********************************/
            case MTD_DATE_FORMAT_DOUBLE_QUOTE_STRING:
            {
                // ���� �� ����ǥ�� �����ϰ� ���ڿ��� ���ؾ� �Ѵ�
                // ���� ���̴� -2��ŭ ���̰�
                // ��ū�� ������ +1��ŭ �ǳʶپ� �����ϸ� �ȴ�

                // double quote string�� ���� ���
                sFormatLen = (acp_uint32_t) mtddlget_leng( sScanner ) - 2;

                ACI_TEST_RAISE( sStringLen < sFormatLen,
                                ERR_NOT_ENOUGH_INPUT );
                
                ACI_TEST_RAISE(
                    acpCStrCmp( (acp_char_t*)sString,
                                mtddlget_text(sScanner)+1,
                                sFormatLen ),
                    ERR_LITERAL_MISMATCH );

                // ���ڿ��� ���̸�ŭ �Է� ���ڿ� ����
                sString    += sFormatLen;
                sStringLen -= sFormatLen;
                
                break;
            }
            
            /***********************************
             * ������ -/,.:;'
             ***********************************/
            case MTD_DATE_FORMAT_SEPARATOR:
            {
                // FORMAT�� separator�� �����ϸ�,
                // input string�� seperator�� white space�� �� ���;� ��
                // BUG-18788 sStringLen�� �˻��ؾ� ��
                if ( sStringLen > 0 )
                {
                    ACI_TEST_RAISE( ( gInputST[*sString] != gSEPAR )
                                    && ( sPrecededWHSP == ACP_FALSE ) ,
                                    ERR_LITERAL_MISMATCH );
                }
                else
                {
                    ACI_RAISE( ERR_LITERAL_MISMATCH );
                }
                

                // white space�� �����ڷ� ���� ���� �̹� switch�� ������ ���ŵǾ��� ���̹Ƿ�
                // �������� ��쿡�� �Է� ���ڿ� ��ġ�� �����ϰ�, ���̸� ������
                // �����ڰ� ���� ���ڷ� ������ ���, �ش� ���ڿ� ���� ��ŭ ����
                sFormatLen = (acp_uint32_t) mtddlget_leng( sScanner );
                // BUG-18788 sStringLen�� �˻��ؾ� ��
                for( sIdx=0;
                     ( sIdx < (acp_sint32_t)sFormatLen ) && ( sStringLen > 0 );
                     sIdx++ )
                {
                    if( gInputST[*sString] == gSEPAR )
                    {
                        sString++;
                        sStringLen--;
                    }
                }

                break;
            }
            
            /***********************************
             * �̵� ���� �ƴϸ� �ν��� �� ���� ����
             ***********************************/
            default:
            {
                sFormatLen = ACP_MIN( MTC_TO_CHAR_MAX_PRECISION,
                                      aFormatLen - ( sFormat - aFormat ) );
                acpMemCpy( sErrorFormat,
                           sFormat,
                           sFormatLen );
                sErrorFormat[sFormatLen] = '\0';
                
                ACI_RAISE( ERR_NOT_RECONGNIZED_FORMAT );
            }
            
        }

        // ���� ������ ������ġ�� ����Ŵ
        sFormat += mtddlget_leng( sScanner );
        
        // get next token
        sToken = mtddllex( sScanner );
    }
    
    mtddllex_destroy ( sScanner );
    sInitScanner = ACP_FALSE;


    // ��ĵ�� ��� ���´µ�, string�� ���� �ִٸ� ����
    ACI_TEST_RAISE( sStringLen != 0,
                    ERR_NOT_ENOUGH_FORMAT );

    //-----------------------------------------------------------
    // ��/��/�� ���� ����
    //-----------------------------------------------------------

    if( mtdDateInterfaceIsLeapYear( mtdDateInterfaceYear(aDate) ) == ACP_TRUE )
    {
        sLeap = 1;
    }
    else
    {
        sLeap = 0;
    }

    // Day of year�� �ԷµǾ� �ִ� ��� Ȯ���Ͽ� �����ϰ� �Է���
    // DDD�� �Ʒ��� ���� ��쿡 ������ ����.
    // TO_DATE( '366-2004', 'DDD-YYYY' ) => �ùٸ� ����� 2004�� 12�� 31��
    // ������ ���� ��¥�� 2005�⵵�̱� ������ 366�� ���� 12��
    // 31���� �ѱ� ������ INVALID_DAY ���� �߻���.  ������ ������ date
    // format model�� �տ������� ������� �б� ������.
    // => ���� ��� �Է��� ó���ѵ�, ���������� ���ռ��� �˻��Ͽ� ���� �Է�
    if( sSetDDD == ACP_TRUE )
    {

        ACI_TEST_RAISE( (sDayOfYear < 1 ) || ( sDayOfYear > (acp_uint32_t)(365+sLeap) ),
                        ERR_INVALID_DAY_OF_YEAR );

        // '��'�� ���
        // day of year�� 30���� ���� ���� �׻� �ش� '��'�̰ų�
        // �ش� '��'���� 1���� '��'�� �����Ƿ� �Ʒ��� ���� ��� ����
        sMonth = (acp_uint8_t)(sDayOfYear/30);
        if( sDayOfYear > gAccDaysOfMonth[sLeap][sMonth] )
        {
            sMonth++;
        }
        else
        {
            // do nothing
        }

        ACI_TEST_RAISE( sMonth == 0,
                        ERR_INVALID_DAY_OF_YEAR );

        // '��'�� ���
        sDay = sDayOfYear - gAccDaysOfMonth[sLeap][sMonth-1];
        
        // �̹� '��'�� �ԷµǾ��ٸ�, day of year�� ������ '��'�� ��ġ�Ͽ��� ��
        ACI_TEST_RAISE( ( sSetMM == ACP_TRUE ) &&
                        ( sMonth != mtdDateInterfaceMonth(aDate) ),
                        ERR_DDD_CONFLICT_MM );
        
        // �̹� '��'�� �ԷµǾ��ٸ�, day of year�� ������ '��'�� ��ġ�Ͽ��� �� 
        ACI_TEST_RAISE( ( sSetDD == ACP_TRUE ) &&
                        ( sDay != mtdDateInterfaceDay(aDate) ),
                        ERR_DDD_CONFLICT_DD );

        // ���� ���� �˻縦 ��� ����ϸ� ���� �Է�
        ACI_TEST( mtdDateInterfaceSetMonth( aDate, sMonth ) != ACI_SUCCESS );
        ACI_TEST( mtdDateInterfaceSetDay( aDate, sDay ) != ACI_SUCCESS );

        ACE_DASSERT( ( sDayOfYear > gAccDaysOfMonth[sLeap][sMonth-1] ) &&
                     ( sDayOfYear <=  gAccDaysOfMonth[sLeap][sMonth] ) );
    }
    else
    {
        // 'DDD'������ �̿��Ͽ� day of year�� �Էµ��� ���� ���
        // do nothing
    }

    /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
    if ( ( mtdDateInterfaceYear( aDate ) == 1582 ) &&
         ( mtdDateInterfaceMonth( aDate ) == 10 ) &&
         ( 4 < mtdDateInterfaceDay( aDate ) ) && ( mtdDateInterfaceDay( aDate ) < 15 ) )
    {
        ACI_TEST( mtdDateInterfaceSetDay( aDate, 15 ) != ACI_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // fix BUG-18787
    // year �Ǵ� month�� ������ �ȵǾ����� ��쿡��
    // ���߿� üũ�Ѵ�.
    if ( ( mtdDateInterfaceYear( aDate ) != ACP_SINT16_MAX ) &&
         ( mtdDateInterfaceMonth( aDate ) != 0 ) )
    {
        // '��'�� ����� (������� ����Ͽ�) �����Ǿ����� �˻�
        // lower bound�� mtdDateInterfaceSetDay���� �˻������Ƿ� upper bound�� �˻�
        ACI_TEST_RAISE( mtdDateInterfaceDay(aDate) > gDaysOfMonth[sLeap][mtdDateInterfaceMonth(aDate)],
                        ERR_INVALID_DAY );
    }
    else
    {
        // do nothing
    }
    
    //-----------------------------------------------------------
    // ��/��/�� ���� ����
    //-----------------------------------------------------------    
    // To fix BUG-14516
    // am/pm format�� ����� ��� HH�� HH24�� ����� �� ����.
    // -> sSetXXXX �� �̿��Ͽ� ��� ���� ����� �ߺ��� �˻��ϹǷ�
    // �� �ܰ迡�� ���� �˻縦 ������ �ʿ䰡 ����.
    if( ( sSetHH12 == ACP_TRUE ) ||
        ( sSetAMPM == ACP_TRUE ) )
    {
        if( sIsAM == ACP_TRUE )    
        {
            if( mtdDateInterfaceHour(aDate) == 12 )
            {
                // BUG-15640
                // 12AM�� 00AM�̴�.
                mtdDateInterfaceSetHour( aDate, 0 );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if( mtdDateInterfaceHour(aDate) == 12 )
            {
                // BUG-15640
                // 12PM�� 12PM�̴�.
                // Nothing to do.
            }
            else
            {
                mtdDateInterfaceSetHour( aDate, mtdDateInterfaceHour(aDate) + 12 );
            }
        }
    }
    else
    {
        // 24�ð� ������ �Է��� ���
        // do nothing
    }    
    
    // 'SSSSS' ������ ���� ���� �Էµ� ���, HH[12|24]?, MI, SS��
    // �Էµ� ���� �ִٸ� ��ġ�ؾ� ��
    if( sSetSID == ACP_TRUE )
    {
        // �̹� '��'�� �Է� �Ǿ��ٸ�, seconds in day�� �Էµ� ���� ��ġ�Ͽ��� ��
        ACI_TEST_RAISE( ( ( sSetHH12 == ACP_TRUE ) || ( sSetHH24 == ACP_TRUE ) ) &&
                        ( sHour != mtdDateInterfaceHour(aDate) ),
                        ERR_SSSSS_CONFLICT_HH );
        
        // �̹� '��'�� �Է� �Ǿ��ٸ�, seconds in day�� �Էµ� ���� ��ġ�Ͽ��� ��
        ACI_TEST_RAISE( ( sSetMI == ACP_TRUE ) &&
                        ( sMinute != mtdDateInterfaceMinute(aDate) ),
                        ERR_SSSSS_CONFLICT_MI );

        // �̹� '��'�� �Է� �Ǿ��ٸ�, seconds in day�� �Էµ� ���� ��ġ�Ͽ��� ��
        ACI_TEST_RAISE( ( sSetSS == ACP_TRUE ) &&
                        ( sSecond != mtdDateInterfaceSecond(aDate) ),
                        ERR_SSSSS_CONFLICT_SS );

        // ���� ���� �˻縦 ��� ����ϸ� ���� �Է�
        ACI_TEST( mtdDateInterfaceSetHour( aDate, sHour ) != ACI_SUCCESS );        
        ACI_TEST( mtdDateInterfaceSetMinute( aDate, sMinute ) != ACI_SUCCESS );
        ACI_TEST( mtdDateInterfaceSetSecond( aDate, sSecond ) != ACI_SUCCESS );
    }
    else
    {
        // 'SSSSS'������ �̿��Ͽ� seconds in day�� �Էµ��� ���� ���
        // do nothing        
    }
    
    
//  // dat of week�� day of year�� ���� ������ ���⼭ �ԷµǾ�� ��
//  // day of week
//  if( sSetDOW == ACP_TRUE )
//  {
//  }


    // format �̳� input�� ���� ��� �˻�
    
    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_CONFLICT_FORMAT );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_CONFLICT_FORMAT);
    }
    ACI_EXCEPTION( ERR_INVALID_DAY_OF_YEAR );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_INVALID_DAY_OF_YEAR);
    }
    ACI_EXCEPTION( ERR_INVALID_SEC_IN_DAY );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_INVALID_SEC_IN_DAY);
    }
    ACI_EXCEPTION( ERR_INVALID_HOUR );
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_HOUR);
    }
    ACI_EXCEPTION( ERR_NOT_ENOUGH_INPUT );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_NOT_ENOUGH_INPUT);
    }
    ACI_EXCEPTION( ERR_NOT_ENOUGH_FORMAT );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_NOT_ENOUGH_FORMAT);
    }
    ACI_EXCEPTION( ERR_LITERAL_MISMATCH );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_LITERAL_MISMATCH);
    }
    ACI_EXCEPTION( ERR_NOT_RECONGNIZED_FORMAT );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_NOT_RECOGNIZED_FORMAT,
                        sErrorFormat );
    }
    ACI_EXCEPTION( ERR_SSSSS_CONFLICT_SS );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_SSSSS_CONFLICT_SS);
    }
    ACI_EXCEPTION( ERR_SSSSS_CONFLICT_MI );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_SSSSS_CONFLICT_MI);
    }
    ACI_EXCEPTION( ERR_SSSSS_CONFLICT_HH );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_SSSSS_CONFLICT_HH);
    }
    ACI_EXCEPTION( ERR_DDD_CONFLICT_DD );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_DDD_CONFLICT_DD);
    }
    ACI_EXCEPTION( ERR_DDD_CONFLICT_MM );
    {
        aciSetErrorCode(mtERR_ABORT_DATE_DDD_CONFLICT_MM);
    }
    ACI_EXCEPTION( ERR_INVALID_DAY );
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_DAY);
    }
    ACI_EXCEPTION( ERR_LEX_INIT_FAILED )
    {
        aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                         "mtdDateInterfaceToDate",
                         "Lex init failed" );
    }
    ACI_EXCEPTION_END;

    if ( sInitScanner == ACP_TRUE )
    {
        mtddllex_destroy ( sScanner );
    }
    
    return ACI_FAILURE;
    
}

ACI_RC
mtdDateInterfaceToChar( mtdDateType*  aDate,
                        acp_uint8_t*  aString,
                        acp_uint32_t* aStringLen,    // ���� ��ϵ� ���� ����
                        acp_sint32_t  aStringMaxLen,
                        acp_uint8_t*  aFormat,
                        acp_uint32_t  aFormatLen )
{

    acp_uint32_t    sToken;

    // temp variables
    acp_sint16_t sYear         = mtdDateInterfaceYear( aDate );
    acp_uint8_t  sMonth        = mtdDateInterfaceMonth( aDate );
    acp_sint32_t sDay          = mtdDateInterfaceDay( aDate );
    acp_sint32_t sHour         = mtdDateInterfaceHour( aDate );
    acp_sint32_t sMin          = mtdDateInterfaceMinute( aDate );
    acp_sint32_t sSec          = mtdDateInterfaceSecond( aDate );
    acp_uint32_t sMicro        = mtdDateInterfaceMicroSecond( aDate );
    acp_sint32_t sDayOfYear;
    acp_uint16_t sWeekOfMonth;
    acp_sint16_t sAbsYear      = 0;
    yyscan_t     sScanner;
    acp_bool_t   sInitScanner  = ACP_FALSE;
    acp_bool_t   sIsFillMode   = ACP_FALSE;
    acp_uint8_t* sString       = aString;
    acp_sint32_t sStringMaxLen = aStringMaxLen;
    acp_uint32_t sLength       = 0;

    // for error msg
    acp_uint8_t* sFormat;     // ���� ó���ϰ� �ִ� ������ ��ġ�� ����Ŵ
    acp_uint8_t  sErrorFormat[MTC_TO_CHAR_MAX_PRECISION+1];
    acp_uint32_t sFormatLen;
    acp_double_t sCeilResult;
    
    if ( sYear < 0 )
    {
        sAbsYear = -sYear;
    }
    else
    {
        sAbsYear = sYear;
    }

    // terminate character�� ����Ͽ� ������ ���ڸ� ������ �� �ִ� �ִ�
    // ���̴� �ϳ� ����
    sStringMaxLen--;
    
    ACI_TEST_RAISE( mtddllex_init ( &sScanner ) != 0, ERR_LEX_INIT_FAILED );
    sInitScanner = ACP_TRUE;
    
    sFormat = aFormat;
    mtddl_scan_bytes( (yyconst char*)aFormat, aFormatLen, sScanner );

    // get first token
    sToken = mtddllex( sScanner );
    while( sToken != 0 )
    {
        /* BUG-36296 FM�� ������ �������� �ʴ´�. */
        sLength = 0;

        switch ( sToken )
        {
            // �ݺ������� appendFORMAT�� ȣ���Կ� ����
            // '���þ� ����Ʈ�� �˰���'�� ������ �߻���
            // ���� appendFORMAT�� snprintf�� �ٲٰ�,
            // ��µǴ� ���̿����� ���ڿ� �����͸� ����
            case MTD_DATE_FORMAT_SEPARATOR :
                sLength = ACP_MIN( sStringMaxLen, mtddlget_leng(sScanner) );
                acpMemCpy( sString,
                           mtddlget_text(sScanner),
                           sLength );
                break;

            case MTD_DATE_FORMAT_AM_U :
            case MTD_DATE_FORMAT_PM_U :
                if ( sHour < 12 )
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "AM", sLength );
                }
                else
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "PM", sLength );
                }
                break;

            case MTD_DATE_FORMAT_AM_UL :
            case MTD_DATE_FORMAT_PM_UL :
                if ( sHour < 12 )
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "Am", sLength );
                }
                else
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "Pm", sLength );
                }
                break;

            case MTD_DATE_FORMAT_AM_L :
            case MTD_DATE_FORMAT_PM_L :
                if ( sHour < 12 )
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "am", sLength );
                }
                else
                {
                    sLength = ACP_MIN( sStringMaxLen, 2 );
                    acpMemCpy( sString, "pm", sLength );
                }
                break;

            case MTD_DATE_FORMAT_SCC :
                if ( sYear <= 0 )
                {
                    /* Year 0�� BC -1���̴�. ���밪�� ���ϱ� ���� �����Ѵ�. */
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "-%02"ACI_INT32_FMT,
                                     ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "-%"ACI_INT32_FMT,
                                     ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100 );
                    }
                }
                else
                {
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     " %02"ACI_INT32_FMT,
                                     ( ( sYear + 99 ) / 100 ) % 100 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%"ACI_INT32_FMT,
                                     ( ( sYear + 99 ) / 100 ) % 100 );
                    }
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_CC :
                if ( sYear <= 0 )
                {
                    /* Year 0�� BC -1���̴�. ���밪�� ���ϱ� ���� �����Ѵ�.
                     * ������ ��, ��ȣ�� �����Ѵ�. (Oracle)
                     */
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%02"ACI_INT32_FMT,
                                     ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%"ACI_INT32_FMT,
                                     ( ( abs( sYear - 1 ) + 99 ) / 100 ) % 100 );
                    }
                }
                else
                {
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%02"ACI_INT32_FMT,
                                     ( ( sYear + 99 ) / 100 ) % 100 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%"ACI_INT32_FMT,
                                     ( ( sYear + 99 ) / 100 ) % 100 );
                    }
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );

                break;

            case MTD_DATE_FORMAT_DAY_U :

                acpSnprintf((acp_char_t*) sString,
                            sStringMaxLen,
                            "%s",
                            gDAYName[mtcDayOfWeek( sYear, sMonth, sDay )] );
                    
                    
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gDAYName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/
                break;

            case MTD_DATE_FORMAT_DAY_UL :

                acpSnprintf((acp_char_t*) sString,
                            sStringMaxLen,
                            "%s",
                            gDayName[mtcDayOfWeek( sYear, sMonth, sDay )] );
                
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gDayName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/
                break;

            case MTD_DATE_FORMAT_DAY_L :
             
                acpSnprintf((acp_char_t*) sString,
                            sStringMaxLen,
                            "%s",
                            gdayName[mtcDayOfWeek( sYear, sMonth, sDay )] );
                
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gdayName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/
                break;

            case MTD_DATE_FORMAT_DDD :
                sDayOfYear = mtcDayOfYear( sYear, sMonth, sDay );

                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%03"ACI_INT32_FMT,
                                 sDayOfYear );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString, sStringMaxLen,
                      "%03"ACI_SINT32_FMT, sDayOfYear );*/
                }
                else
                {
                    
                    acpSnprintf((acp_char_t*) sString, sStringMaxLen,
                                "%"ACI_INT32_FMT, sDayOfYear );
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString, sStringMaxLen,
                      "%"ACI_SINT32_FMT, sDayOfYear );*/
                }
                break;

            case MTD_DATE_FORMAT_DD :

                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT, sDay );

                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_SINT32_FMT, sDay );*/
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sDay );
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_SINT32_FMT, sDay );*/
                }
                break;

            case MTD_DATE_FORMAT_DY_U :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gDYName[mtcDayOfWeek( sYear, sMonth, sDay )] );

                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gDYName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/
                break;
            case MTD_DATE_FORMAT_DY_UL :
                   
                acpSnprintf((acp_char_t*) sString,
                            sStringMaxLen,
                            "%s",
                            gDyName[mtcDayOfWeek( sYear, sMonth, sDay )] );
                
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gDyName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/

                break;

            case MTD_DATE_FORMAT_DY_L :
                                    
                acpSnprintf(  (acp_char_t*) sString,
                              sStringMaxLen,
                              "%s",
                              gdyName[mtcDayOfWeek( sYear, sMonth, sDay )] );
                    
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString,
                  sStringMaxLen,
                  "%s",
                  gdyName[mtcDayOfWeek( sYear, sMonth, sDay )] );*/
                break;

            case MTD_DATE_FORMAT_D :

                acpSnprintf((acp_char_t*) sString, sStringMaxLen, "%1"ACI_INT32_FMT,
                            mtcDayOfWeek( sYear, sMonth, sDay ) + 1 );
                    
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                sLength =
                  idlOS::snprintf( (acp_char_t*) sString, sStringMaxLen, "%1"ACI_SINT32_FMT,
                  mtcDayOfWeek( sYear, sMonth, sDay ) + 1 );*/
                break;

            case MTD_DATE_FORMAT_FF1 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%01"ACI_INT32_FMT,
                                sMicro / 100000);
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%01"ACI_SINT32_FMT,
                      sMicro / 100000);*/
                }
                else
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%"ACI_INT32_FMT,
                                sMicro / 100000);
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX); 
                 
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%"ACI_SINT32_FMT,
                      sMicro / 100000);*/
                }
                break;

            case MTD_DATE_FORMAT_FF2 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%02"ACI_INT32_FMT,
                                sMicro / 10000);
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    /*   sLength =
                         idlOS::snprintf( (acp_char_t*) sString,
                         sStringMaxLen,
                         "%02"ACI_INT32_FMT,
                         sMicro / 10000);*/
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 sMicro / 10000);

                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    /*   sLength =
                         idlOS::snprintf( (acp_char_t*) sString,
                         sStringMaxLen,
                         "%"ACI_SINT32_FMT,
                         sMicro / 10000);*/
                }
                break;

            case MTD_DATE_FORMAT_FF3 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%03"ACI_INT32_FMT,
                                sMicro / 1000 );
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%03"ACI_SINT32_FMT,
                      sMicro / 1000 );*/
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 sMicro / 1000 );
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%"ACI_SINT32_FMT,
                      sMicro / 1000 );*/
                }
                break;

            case MTD_DATE_FORMAT_FF4 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%04"ACI_INT32_FMT,
                                sMicro / 100);
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%04"ACI_SINT32_FMT,
                      sMicro / 100);*/
                }
                else
                {
                    acpSnprintf((acp_char_t*) sString,
                                sStringMaxLen,
                                "%"ACI_INT32_FMT,
                                sMicro / 100);
                    
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
/*                    sLength =
                      idlOS::snprintf( (acp_char_t*) sString,
                      sStringMaxLen,
                      "%"ACI_INT32_FMT,
                      sMicro / 100);*/
                }
                break;

            case MTD_DATE_FORMAT_FF5 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%05"ACI_INT32_FMT,
                                 sMicro / 10 );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 sMicro / 10 );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_FF :
            case MTD_DATE_FORMAT_FF6 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%06"ACI_INT32_FMT,
                                 sMicro );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 sMicro );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_FM :
                // To fix BUG-17693
                sIsFillMode = ACP_TRUE;
                break;

            case MTD_DATE_FORMAT_HH :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, 
                                 sStringMaxLen, 
                                 "%02"ACI_INT32_FMT, 
                                 sHour );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, 
                                 sStringMaxLen, 
                                 "%"ACI_INT32_FMT, 
                                 sHour );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX); 
                }
                break;

            case MTD_DATE_FORMAT_HH12 :
                if( sIsFillMode == ACP_FALSE )
                {
                    if ( sHour == 0 || sHour == 12 )
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                     "%02"ACI_INT32_FMT, 12 );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                     "%02"ACI_INT32_FMT, sHour % 12 );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                }
                else
                {
                    if ( sHour == 0 || sHour == 12 )
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                     "%"ACI_INT32_FMT, 12 );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                     "%"ACI_INT32_FMT, sHour % 12 );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                }
                break;

            case MTD_DATE_FORMAT_HH24 :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT, sHour );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sHour );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_MI :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT, sMin );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sMin );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }

                break;

            case MTD_DATE_FORMAT_MM :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT, sMonth );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sMonth );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }

                break;

            case MTD_DATE_FORMAT_MON_U :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gMONName[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_MON_UL :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gMonName[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;
            case MTD_DATE_FORMAT_MON_L :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gmonName[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_MONTH_U :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gMONTHName[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_MONTH_UL :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gMonthName[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;
            case MTD_DATE_FORMAT_MONTH_L :
                acpSnprintf( (acp_char_t*) sString,
                             sStringMaxLen,
                             "%s",
                             gmonthName[sMonth-1] );

                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                break;

            case MTD_DATE_FORMAT_Q :

                acpMathCeil( (acp_double_t) sMonth / 3, &sCeilResult);

                acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                             "%1"ACI_INT32_FMT, 
                             (acp_uint32_t) sCeilResult );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_RM_U :

                acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                             "%s", gRMMonth[sMonth-1] );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_RM_L :

                acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                             "%s", grmMonth[sMonth-1] );

                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);

                break;

            case MTD_DATE_FORMAT_SSSSSSSS :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                 "%02"ACI_INT32_FMT"%06"ACI_INT32_FMT,
                                 sSec,
                                 sMicro );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    if( sSec != 0 )
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                                     "%"ACI_INT32_FMT"%06"ACI_INT32_FMT,
                                     sSec,
                                     sMicro );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sMicro );
                        sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                    }
                }
                break;

            case MTD_DATE_FORMAT_SSSSSS :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%06"ACI_INT32_FMT, sMicro );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sMicro );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_SSSSS :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%05"ACI_INT32_FMT,
                                 ( sHour*60*60 ) +
                                 ( sMin*60 ) + sSec );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 ( sHour*60*60 ) +
                                 ( sMin*60 ) + sSec );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_SS :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT, sSec );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT, sSec );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_WW :
                if( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%02"ACI_INT32_FMT,
                                 mtcWeekOfYear( sYear, sMonth, sDay ) );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                else
                {
                    acpSnprintf( (acp_char_t*) sString, sStringMaxLen, "%"ACI_INT32_FMT,
                                 mtcWeekOfYear( sYear, sMonth, sDay ) );
                    sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                }
                break;

            case MTD_DATE_FORMAT_IW :   /* BUG-42926 TO_CHAR()�� IW �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%02"ACI_INT32_FMT,
                                 mtcWeekOfYearForStandard( sYear, sMonth, sDay ) );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcWeekOfYearForStandard( sYear, sMonth, sDay ) );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_WW2 :  /* BUG-42941 TO_CHAR()�� WW2(Oracle Version WW) �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%02"ACI_INT32_FMT,
                                 mtcWeekOfYearForOracle( sYear, sMonth, sDay ) );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcWeekOfYearForOracle( sYear, sMonth, sDay ) );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_W :
                acpMathCeil( (acp_double_t) ( sDay + mtcDayOfWeek( sYear, sMonth, 1 ))  / 7, &sCeilResult);
                sWeekOfMonth = (acp_uint16_t) sCeilResult;
                acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                             "%1"ACI_INT32_FMT, sWeekOfMonth );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                break;

            case MTD_DATE_FORMAT_YCYYY :
                /* ������ ��, ��ȣ�� �����Ѵ�. (Oracle) */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *)sString,
                                 sStringMaxLen,
                                 "%01"ACI_INT32_FMT",%03"ACI_INT32_FMT,
                                 ( sAbsYear / 1000 ) % 10,
                                 sAbsYear % 1000 );
                }
                else
                {
                    if ( sAbsYear < 1000 )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%"ACI_INT32_FMT,
                                     sAbsYear % 1000 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                     sStringMaxLen,
                                     "%"ACI_INT32_FMT",%03"ACI_INT32_FMT,
                                     ( sAbsYear / 1000 ) % 10,
                                     sAbsYear % 1000 );
                    }
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_SYYYY :    /* BUG-36296 SYYYY Format ���� */
                if ( sYear < 0 )
                {
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                                   sStringMaxLen,
                                                   "-%04"ACI_INT32_FMT,
                                                   sAbsYear % 10000 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                                   sStringMaxLen,
                                                   "-%"ACI_INT32_FMT,
                                                   sAbsYear % 10000 );
                    }
                }
                else
                {
                    if ( sIsFillMode == ACP_FALSE )
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                                   sStringMaxLen,
                                                   " %04"ACI_INT32_FMT,
                                                   sAbsYear % 10000 );
                    }
                    else
                    {
                        acpSnprintf( (acp_char_t *)sString,
                                                   sStringMaxLen,
                                                   "%"ACI_INT32_FMT,
                                                   sAbsYear % 10000 );
                    }
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_YYYY :
            case MTD_DATE_FORMAT_RRRR :
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%04"ACI_INT32_FMT,
                                               sAbsYear % 10000 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%"ACI_INT32_FMT,
                                               sAbsYear % 10000 );
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_YYY :
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%03"ACI_INT32_FMT,
                                               sAbsYear % 1000 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%"ACI_INT32_FMT,
                                               sAbsYear % 1000 );
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;


            case MTD_DATE_FORMAT_YY :
            case MTD_DATE_FORMAT_RR :
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%02"ACI_INT32_FMT,
                                               sAbsYear % 100 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%"ACI_INT32_FMT,
                                               sAbsYear % 100 );
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_Y :
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%01"ACI_INT32_FMT,
                                               sAbsYear % 10 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *)sString,
                                               sStringMaxLen,
                                               "%"ACI_INT32_FMT,
                                               sAbsYear % 10 );
                }

                sLength = acpCStrLen( (acp_char_t *)sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_DOUBLE_QUOTE_STRING :
                acpSnprintf( (acp_char_t*) sString, sStringMaxLen,
                             "%s", mtddlget_text(sScanner) + 1 );
                sLength = acpCStrLen( (acp_char_t*)sString, ACP_UINT32_MAX);
                // BUG-27290
                // �� double-quoted string�̶��ϴ��� ��� '"'��
                // ����ǹǷ� sLength�� 1���� ���� �� ����.
                sLength--;
                break;

            case MTD_DATE_FORMAT_IYYY :   /* BUG-46727 TO_CHAR()�� IYYY �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%04"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_IYY :   /* BUG-46727 TO_CHAR()�� IYY �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%03"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 1000 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 1000 );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_IY :   /* BUG-46727 TO_CHAR()�� IY �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%02"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 100 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 100 );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            case MTD_DATE_FORMAT_I :   /* BUG-46727 TO_CHAR()�� I �߰� */
                if ( sIsFillMode == ACP_FALSE )
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%01"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 10 );
                }
                else
                {
                    acpSnprintf( (acp_char_t *) sString,
                                 sStringMaxLen,
                                 "%"ACI_INT32_FMT,
                                 mtcYearForStandard( sYear, sMonth, sDay ) % 10 );
                }
                sLength = acpCStrLen( (acp_char_t *) sString, ACP_UINT32_MAX );
                break;

            default:
                sFormatLen = ACP_MIN( MTC_TO_CHAR_MAX_PRECISION,
                                      aFormatLen - ( sFormat - aFormat ) );
                
                acpMemCpy( sErrorFormat,
                           sFormat,
                           sFormatLen );
                sErrorFormat[sFormatLen] = '\0';
                
                ACI_RAISE( ERR_NOT_RECONGNIZED_FORMAT );
        }
        
        sString       += sLength;
        sStringMaxLen -= sLength;
        
        // ���� ������ ������ġ�� ����Ŵ
        sFormat += mtddlget_leng( sScanner );

        // get next token
        sToken = mtddllex( sScanner );
    }

    // snprintf���� '\0' termination�� �� �� �� �����Ƿ�
    sString[0] = '\0';

    mtddllex_destroy ( sScanner );
    sInitScanner = ACP_FALSE;

    *aStringLen = sString - aString;

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_NOT_RECONGNIZED_FORMAT )
    {
        aciSetErrorCode(mtERR_ABORT_DATE_NOT_RECOGNIZED_FORMAT,
                        sErrorFormat );
    }
    ACI_EXCEPTION( ERR_LEX_INIT_FAILED )
    {
        aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                         "mtdDateInterfaceToChar",
                         "Lex init failed" );
    }
    ACI_EXCEPTION_END;

    if ( sInitScanner == ACP_TRUE )
    {
        mtddllex_destroy ( sScanner );
    }

    return ACI_FAILURE;
}

/*
  ACI_RC
  mtdDateInterface::getMaxCharLength( acp_uint8_t*       aFormat,
  acp_uint32_t         aFormatLen,
  acp_uint32_t*        aLength)
  {
  acp_uint32_t    sToken;

  // temp variables
  acp_sint32_t         sBufferCur = 0;
  yyscan_t     sScanner;
  acp_bool_t       sInitScanner = ACP_FALSE;

  ACI_TEST_RAISE( mtddllex_init ( &sScanner ) != 0, ERR_LEX_INIT_FAILED );
  sInitScanner = ACP_TRUE;

  mtddl_scan_bytes( (yyconst char*)aFormat, aFormatLen, sScanner );

  // get first token
  sToken = mtddllex( sScanner );
  while( sToken != 0 )
  {
  switch ( sToken )
  {
  case MTD_DATE_FORMAT_NONE :
  sBufferCur += acpStrLen( mtddlget_text(sScanner), ACP_UINT32_MAX );
  break;

  case MTD_DATE_FORMAT_AM_U :
  case MTD_DATE_FORMAT_PM_U :
  case MTD_DATE_FORMAT_AM_UL :
  case MTD_DATE_FORMAT_PM_UL :
  case MTD_DATE_FORMAT_AM_L :
  case MTD_DATE_FORMAT_PM_L :
  sBufferCur += 2; //(AM/PM, am,pm.. Am..)
  break;

  case MTD_DATE_FORMAT_CC :
  sBufferCur += 2;
  break;

  case MTD_DATE_FORMAT_DAY_U :
  case MTD_DATE_FORMAT_DAY_L :
  case MTD_DATE_FORMAT_DAY_UL :
  sBufferCur += 9; // "WEDNESDAY"
  break;
  case MTD_DATE_FORMAT_DDD :
  sBufferCur += 3; // "365"
  break;

  case MTD_DATE_FORMAT_DD :
  sBufferCur += 2; // "31"
  break;

  case MTD_DATE_FORMAT_DY_U :
  case MTD_DATE_FORMAT_DY_UL :
  case MTD_DATE_FORMAT_DY_L :
  sBufferCur +=3; // "MON"
  break;

  case MTD_DATE_FORMAT_D :
  sBufferCur +=1;
  break;

  case MTD_DATE_FORMAT_FF1 :
  sBufferCur += 1;
  break;

  case MTD_DATE_FORMAT_FF2 :
  sBufferCur += 2;
  break;

  case MTD_DATE_FORMAT_FF3 :
  sBufferCur += 3;
  break;

  case MTD_DATE_FORMAT_FF4 :
  sBufferCur += 4;
  break;

  case MTD_DATE_FORMAT_FF5 :
  sBufferCur += 5;
  break;

  case MTD_DATE_FORMAT_FF :
  case MTD_DATE_FORMAT_FF6 :
  sBufferCur += 6;
  break;

  case MTD_DATE_FORMAT_HH :
  case MTD_DATE_FORMAT_HH12 :
  case MTD_DATE_FORMAT_HH24 :
  case MTD_DATE_FORMAT_MI :
  case MTD_DATE_FORMAT_MM :
  sBufferCur +=2;
  break;

  case MTD_DATE_FORMAT_MON_U :
  case MTD_DATE_FORMAT_MON_UL :
  case MTD_DATE_FORMAT_MON_L :
  sBufferCur += 3; //'JAN'
  break;
  case MTD_DATE_FORMAT_MONTH_U :
  case MTD_DATE_FORMAT_MONTH_UL :
  case MTD_DATE_FORMAT_MONTH_L :
  sBufferCur += 9; // "september"
  break;

  case MTD_DATE_FORMAT_Q :
  sBufferCur += 1;
  break;

  case MTD_DATE_FORMAT_RM_U :
  case MTD_DATE_FORMAT_RM_L :
  sBufferCur += 4; //'VIII'
  break;

  case MTD_DATE_FORMAT_SSSSSSSS :
  sBufferCur += 8; //'sec:microsec'
  break;

  case MTD_DATE_FORMAT_SSSSSS :
  sBufferCur += 6; //'sec:microsec'
  break;

  case MTD_DATE_FORMAT_SSSSS :
  sBufferCur += 5; 
  break;

  case MTD_DATE_FORMAT_SS :
  sBufferCur += 2; 
  break;

  case MTD_DATE_FORMAT_WW :
  sBufferCur +=2; // week of year
  break;

  case MTD_DATE_FORMAT_W :
  sBufferCur +=1; // week of month
  break;

  case MTD_DATE_FORMAT_YCYYY : 
  sBufferCur += 5; //20,03
  break;

  case MTD_DATE_FORMAT_YYYY :
  case MTD_DATE_FORMAT_RRRR :
  sBufferCur += 4;
  break;

  case MTD_DATE_FORMAT_YY :
  case MTD_DATE_FORMAT_RR :
  sBufferCur += 2;
  break;

  case MTD_DATE_FORMAT_DOUBLE_QUOTE_STRING :
  sBufferCur += acpCStrLen( mtddlget_text(sScanner), ACP_UINT32_MAX ) + 1;
  break;

  default:
  ACI_RAISE( ERR_INVALID_LITERAL );
  }

  // get next token
  sToken = mtddllex( sScanner);
  }

  mtddllex_destroy ( sScanner );
  sInitScanner = ACP_FALSE;

  if (sBufferCur != 0)
  {
  sBufferCur ++; // trailing \0
  }
  *aLength = sBufferCur;
    
  return ACI_SUCCESS;

  ACI_EXCEPTION( ERR_INVALID_LITERAL );
  {
  aciSetErrorCode(mtERR_ABORT_INVALID_DATE);
  }
  ACI_EXCEPTION( ERR_LEX_INIT_FAILED );
  {
  aciSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
  "mtdDateInterfaceGetMaxCharLength",
  "Lex init failed" );
  }
  ACI_EXCEPTION_END;

  if ( sInitScanner == ACP_TRUE )
  {
  mtddllex_destroy ( sScanner );
  }

  return ACI_FAILURE;

  }
*/

ACI_RC
mtdDateInterfaceCheckYearMonthDayAndSetDateValue(
    mtdDateType* aDate,
    acp_sint16_t aYear,
    acp_uint8_t  aMonth,
    acp_uint8_t  aDay)
{
    ACI_TEST( mtdDateInterfaceSetYear( aDate, aYear ) != ACI_SUCCESS );
    ACI_TEST( mtdDateInterfaceSetMonth( aDate, aMonth ) != ACI_SUCCESS );

    /* BUG-36296 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
    if ( ( aYear == 1582 ) &&
         ( aMonth == 10 ) &&
         ( 4 < aDay ) && ( aDay < 15 ) )
    {
        ACI_TEST( mtdDateInterfaceSetDay( aDate, 15 ) != ACI_SUCCESS );
    }
    else
    {
        ACI_TEST( mtdDateInterfaceSetDay( aDate, aDay ) != ACI_SUCCESS );

        if ( mtdDateInterfaceIsLeapYear( aYear ) == ACP_TRUE )
        {
            ACI_TEST_RAISE( aDay > gDaysOfMonth[1][aMonth], ERR_INVALID_DAY );
        }
        else
        {
            ACI_TEST_RAISE( aDay > gDaysOfMonth[0][aMonth], ERR_INVALID_DAY );
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION( ERR_INVALID_DAY );
    aciSetErrorCode(mtERR_ABORT_INVALID_DAY);
    
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC mtdStoredValue2MtdValue( acp_uint32_t aColumnSize,
                                       void*        aDestValue,
                                       acp_uint32_t aDestValueOffset,
                                       acp_uint32_t aLength,
                                       const void*  aValue )
{
/******************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *******************************************************************/

    mtdDateType  * sDateValue;

    // �������� ����Ÿ Ÿ���� ���
    // �ϳ��� �÷� ����Ÿ�� ������������ ������ ����Ǵ� ���� ����.

    ACP_UNUSED(aDestValueOffset);
    
    sDateValue = (mtdDateType*)aDestValue;
    
    if( aLength == 0 )
    {
        // NULL ����Ÿ
        *sDateValue = mtcdDateNull;
    }
    else
    {
        ACI_TEST_RAISE( aLength != aColumnSize, ERR_INVALID_STORED_VALUE );        

        acpMemCpy( sDateValue, aValue, aLength );
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(ERR_INVALID_STORED_VALUE);
    {
        aciSetErrorCode(mtERR_ABORT_INVALID_STORED_DATA_LENGTH);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;    
}


acp_uint32_t mtdNullValueSize()
{
/*******************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ    
 *******************************************************************/

    return mtdActualSize( NULL,
                          & mtcdDateNull,
                          MTD_OFFSET_USELESS );   
}

