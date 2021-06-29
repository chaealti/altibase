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
 * $Id: mtc.h 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#ifndef _O_MTC_H_
# define _O_MTC_H_ 1

# include <mtcDef.h>
# include <mtdTypes.h>
# include <idn.h>

#define MTC_MAXIMUM_EXTERNAL_GROUP_CNT          (128)
#define MTC_MAXIMUM_CHAR_SIZE_FOR_NUMBER_GROUP  (1024)

/* PROJ-2209 DBTIMEZONE */
#define MTC_TIMEZONE_NAME_LEN  (40)
#define MTC_TIMEZONE_VALUE_LEN (6)

// PROJ-4155 DBMS PIPE
#define MTC_MSG_VARBYTE_TYPE        (MTD_VARBYTE_ID)
#define MTC_MSG_MAX_BUFFER_SIZE     (8192) // 8k

// ����.
// permission 0644(rw-r-r), 0600(rw-----)���� user1���� ������
// msq queue�� user2���� snd,rcv �Ҽ� ����. 
// �������� 0600���� ������ msg queue��  user2 ���� ipcs -a ��ȸ��
// msg queue ���� ��ȸ�� �ʵ�
#define MTC_MSG_PUBLIC_PERMISSION   (0666)
#define MTC_MSG_PRIVATE_PERMISSION  (0644)

typedef struct mtcMsgBuffer
{
    SLong  mType;
    UChar  mMessage[MTC_MSG_MAX_BUFFER_SIZE];
} mtcMsgBuffer;

#if defined(WRS_VXWORKS)
static SInt sDays[2][14] = {
    { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};
#endif

/* PROJ-2446 ONE SOURCE */
typedef void * mmlDciTableColumn;

typedef IDE_RC (*dciCharConverFn)(
                     void            * ,
                     const mtlModule * ,
                     const mtlModule * ,
                     void            * ,
                     SInt              ,
                     void            * ,
                     SInt            * ,
                     SInt      );

#define DCI_CONV_DATA_IN       (0x01)
#define DCI_CONV_DATA_OUT      (0x02)
#define DCI_CONV_CALC_TOTSIZE  (0x10) /* ���ڿ� ��ü ���� ��� */

class mtc
{
private:
    static const UChar hashPermut[256];

    static IDE_RC cloneTuple( iduMemory   * aMemory,
                              mtcTemplate * aSrcTemplate,
                              UShort        aSrcTupleID,
                              mtcTemplate * aDstTemplate,
                              UShort        aDstTupleID );

    static IDE_RC cloneTuple( iduVarMemList * aMemory,
                              mtcTemplate   * aSrcTemplate,
                              UShort          aSrcTupleID,
                              mtcTemplate   * aDstTemplate,
                              UShort          aDstTupleID );

    /* PROJ-1071 Parallel Query */
    static IDE_RC cloneTuple4Parallel( iduMemory   * aMemory,
                                       mtcTemplate * aSrcTemplate,
                                       UShort        aSrcTupleID,
                                       mtcTemplate * aDstTemplate,
                                       UShort        aDstTupleID );

public:
    static inline SInt dayOfYear( SInt aYear,
                                  SInt aMonth,
                                  SInt aDay )
    {
#if !defined(WRS_VXWORKS)
        static SInt sDays[2][14] = {
            { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
            { 0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
        };
#endif

        // BUG-22710
        if ( mtdDateInterface::isLeapYear( aYear ) == ID_TRUE )
        {
            return sDays[1][aMonth] + aDay;
        }
        else
        {
            return sDays[0][aMonth] + aDay;
        }
    }

    static inline SInt dayOfCommonEra( SInt aYear,
                                       SInt aMonth,
                                       SInt aDay )
    {
        SInt sDays;

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
                      + dayOfYear( aYear, aMonth, aDay )
                      - 366; /* BC 1��(aYear == 0)�� �����̴�. */
            }
            /* BUG-36296 �׷������� ���� ��Ģ�� 1583����� �����Ѵ�. 1582�� �������� 4�⸶�� �����̴�. */
            else
            {
                sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                      + dayOfYear( aYear, aMonth, aDay );
            }
        }
        else if ( aYear == 1582 )
        {
            /* BUG-36296 �׷������� 1582�� 10�� 15�Ϻ��� �����Ѵ�. */
            if ( ( aMonth < 10 ) ||
                 ( ( aMonth == 10 ) && ( aDay < 15 ) ) )
            {
                sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                      + dayOfYear( aYear, aMonth, aDay );
            }
            else
            {
                sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                      + dayOfYear( aYear, aMonth, aDay )
                      - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
            }
        }
        else
        {
            /* BUG-36296 1600�� ������ �����콺�°� �׷������� ������ ����. */
            if ( aYear <= 1600 )
            {
                sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                      + dayOfYear( aYear, aMonth, aDay )
                      - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
            }
            else
            {
                sDays = ( ( aYear - 1 ) * 365 ) + ( ( aYear - 1 ) / 4 )
                      - ( ( aYear - 1 - 1600 ) / 100 ) + ( ( aYear - 1 - 1600 ) / 400 )
                      + dayOfYear( aYear, aMonth, aDay )
                      - 10; /* 1582�� 10�� 4��(��)���� 10�� 15��(��)���� �ٷ� �ǳʶڴ�. */
            }
        }

        /* AD 0001�� 1�� 1���� Day 0 �̴�. �׸���, BC 0001�� 12�� 31���� Day -1 �̴�. */
        sDays--;

        return sDays;
    }

    /* �Ͽ��� : 0, ����� : 6 */
    static inline SInt dayOfWeek( SInt aYear,
                                  SInt aMonth,
                                  SInt aDay )
    {
        /* ������� ��쿡�� ������ �����Ƿ�, %7�� �����ϰ� �������� ���Ѵ�.
         * AD 0001�� 01�� 01���� �����(sDays = 0)�̴�. +6�� �Ͽ� �����Ѵ�.
         */
        return ( ( dayOfCommonEra( aYear, aMonth, aDay ) % 7 ) + 6 ) % 7;
    }

    /* �޷°� ��ġ�ϴ� ���� : �ְ� �Ͽ��Ϻ��� �����Ѵ�. */
    static inline SInt weekOfYear( SInt aYear,
                                   SInt aMonth,
                                   SInt aDay )
    {
        SInt sWeek;

        sWeek = (SInt) idlOS::ceil( (SDouble)( dayOfWeek ( aYear, 1, 1 ) +
                                               dayOfYear ( aYear, aMonth, aDay ) ) / 7 );
        return sWeek;
    }

    /* BUG-42941 TO_CHAR()�� WW2(Oracle Version WW) �߰� */
    static inline SInt weekOfYearForOracle( SInt aYear,
                                            SInt aMonth,
                                            SInt aDay )
    {
        SInt sWeek = 0;

        sWeek = ( dayOfYear( aYear, aMonth, aDay ) + 6 ) / 7;

        return sWeek;
    }

    /* BUG-42926 TO_CHAR()�� IW �߰� */
    static SInt weekOfYearForStandard( SInt aYear,
                                       SInt aMonth,
                                       SInt aDay );

    /* BUG-46727 TO_CHAR()�� IYYY �߰� */
    static SInt yearForStandard( SInt aYear,
                                 SInt aMonth,
                                 SInt aDay );

    static const UInt  hashInitialValue;

    // mtd::valueForModule���� ����Ѵ�.
    static mtcGetColumnFunc            getColumn;
    static mtcOpenLobCursorWithRow     openLobCursorWithRow;
    static mtcOpenLobCursorWithGRID    openLobCursorWithGRID;
    static mtcReadLob                  readLob;
    static mtcGetLobLengthWithLocator  getLobLengthWithLocator;
    static mtcIsNullLobColumnWithRow   isNullLobColumnWithRow;
    static mtcGetCompressionColumnFunc getCompressionColumn;
    /* PROJ-2446 ONE SOURCE */
    static mtcGetStatistics            getStatistics;

    // PROJ-2047 Strengthening LOB
    static IDE_RC getLobLengthLocator( smLobLocator   aLobLocator,
                                       idBool       * aIsNull,
                                       UInt         * aLobLength,
                                       idvSQL       * aStatistics );
    static IDE_RC isNullLobRow( const void      * aRow,
                                const smiColumn * aColumn,
                                idBool          * aIsNull );
    
    // PROJ-1579 NCHAR
    static mtcGetDBCharSet            getDBCharSet;
    static mtcGetNationalCharSet      getNationalCharSet;

    static const void* getColumnDefault( const void*,
                                         const smiColumn*,
                                         UInt* );

    static UInt         mExtTypeModuleGroupCnt;
    static mtdModule ** mExtTypeModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

    static UInt         mExtCvtModuleGroupCnt;
    static mtvModule ** mExtCvtModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

    static UInt         mExtFuncModuleGroupCnt;
    static mtfModule ** mExtFuncModuleGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

    static UInt              mExtRangeFuncGroupCnt;
    static smiCallBackFunc * mExtRangeFuncGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

    static UInt             mExtCompareFuncCnt;
    static mtdCompareFunc * mExtCompareFuncGroup[MTC_MAXIMUM_EXTERNAL_GROUP_CNT];

    static IDE_RC addExtTypeModule( mtdModule **  aExtTypeModules );
    static IDE_RC addExtCvtModule( mtvModule **  aExtCvtModules );
    static IDE_RC addExtFuncModule( mtfModule **  aExtFuncModules );
    static IDE_RC addExtRangeFunc( smiCallBackFunc * aExtRangeFuncs );
    static IDE_RC addExtCompareFunc( mtdCompareFunc * aExtCompareFuncs );

    static IDE_RC initialize( mtcExtCallback    * aCallBacks );

    static IDE_RC finalize( idvSQL * aStatistics );

    static UInt isSameType( const mtcColumn* aColumn1,
                            const mtcColumn* aColumn2 );

    static void copyColumn( mtcColumn*       aDestination,
                            const mtcColumn* aSource );

    static const void* value( const mtcColumn* aColumn,
                              const void*      aRow,
                              UInt             aFlag );

    static UInt hash( UInt         aHash,
                      const UChar* aValue,
                      UInt         aLength );

    // fix BUG-9496
    static UInt hashWithExponent( UInt         aHash,
                                  const UChar* aValue,
                                  UInt         aLength );

    static UInt hashSkip( UInt         aHash,
                          const UChar* aValue,
                          UInt         aLength );

    // BUG-41937
    static UInt hashSkipWithoutZero( UInt         aHash,
                                     const UChar* aValue,
                                     UInt         aLength );

    static UInt hashWithoutSpace( UInt         aHash,
                                  const UChar* aValue,
                                  UInt         aLength );

    static IDE_RC makeNumeric( mtdNumericType* aNumeric,
                               UInt            aMaximumMantissa,
                               const UChar*    aString,
                               UInt            aLength );

    /* PROJ-1517 */
    static void makeNumeric( mtdNumericType *aNumeric,
                             const SLong     aNumber );

    // BUG-41194
    static IDE_RC makeNumeric( mtdNumericType *aNumeric,
                               const SDouble   aNumber );
    
    static IDE_RC numericCanonize( mtdNumericType *aValue,
                                   mtdNumericType *aCanonizedValue,
                                   SInt            aCanonPrecision,
                                   SInt            aCanonScale,
                                   idBool         *aCanonized );

    static IDE_RC floatCanonize( mtdNumericType *aValue,
                                 mtdNumericType *aCanonizedValue,
                                 SInt            aCanonPrecision,
                                 idBool         *aCanonized );
        
    static IDE_RC makeSmallint( mtdSmallintType* aSmallint,
                                const UChar*     aString,
                                UInt             aLength );
    
    static IDE_RC makeInteger( mtdIntegerType* aInteger,
                               const UChar*    aString,
                               UInt            aLength );
    
    static IDE_RC makeBigint( mtdBigintType* aBigint,
                              const UChar*   aString,
                              UInt           aLength );
    
    static IDE_RC makeReal( mtdRealType* aReal,
                            const UChar* aString,
                            UInt         aLength );
    
    static IDE_RC makeDouble( mtdDoubleType* aDouble,
                              const UChar*   aString,
                              UInt           aLength );
    
    static IDE_RC makeInterval( mtdIntervalType* aInterval,
                                const UChar*     aString,
                                UInt             aLength );
    
    static IDE_RC makeBinary( mtdBinaryType* aBinary,
                              const UChar*   aString,
                              UInt           aLength );
    
    static IDE_RC makeBinary( UChar*       aBinaryValue,
                              const UChar* aString,
                              UInt         aLength,
                              UInt*        aBinaryLength,
                              idBool*      aOddSizeFlag );
    
    static IDE_RC makeByte( mtdByteType* aByte,
                            const UChar* aString,
                            UInt         aLength );

    static IDE_RC makeByte( UChar*       aByteValue,
                            const UChar* aString,
                            UInt         aLength,
                            UInt       * aByteLength,
                            idBool     * aOddSizeFlag);
    
    static IDE_RC makeNibble( mtdNibbleType* aNibble,
                              const UChar*   aString,
                              UInt           aLength );
    
    static IDE_RC makeNibble( UChar*       aNibbleValue,
                              UChar        aOffsetInByte,
                              const UChar* aString,
                              UInt         aLength,
                              UInt*        aNibbleLength );
    
    static IDE_RC makeBit( mtdBitType*  aBit,
                           const UChar* aString,
                           UInt         aLength );
    
    static IDE_RC makeBit( UChar*       aBitValue,
                           UChar        aOffsetInBit,
                           const UChar* aString,
                           UInt         aLength,
                           UInt*        aBitLength );
    
    static IDE_RC findCompare( const smiColumn* aColumn,
                               UInt             aFlag,
                               smiCompareFunc*  aCompare );

    // PROJ-1618
    static IDE_RC findKey2String( const smiColumn   * aColumn,
                                  UInt                aFlag,
                                  smiKey2StringFunc * aKey2String );

    // PROJ-1629
    static IDE_RC findNull( const smiColumn   * aColumn,
                            UInt                aFlag,
                            smiNullFunc       * aNull );
    
    static IDE_RC findIsNull( const smiColumn * aColumn,
                              UInt             aFlag,
                              smiIsNullFunc *  aIsNull );

    static IDE_RC getAlignValue( const smiColumn * aColumn,
                                 UInt *            aAlignValue );

    // Column�� �´� Hash Key ���� �Լ� �˻�
    static IDE_RC findHashKeyFunc( const smiColumn * aColumn,
                                   smiHashKeyFunc  * aHashKeyFunc );

    // PROJ-1705
    // QP����ó�������ѷ��ڵ忡�� �ε������ڵ屸����
    // ���ڵ�κ��� �÷�����Ÿ�� size�� value ptr�� ��´�.
    static IDE_RC getValueLengthFromFetchBuffer( idvSQL          * aStatistic,
                                                 const smiColumn * aColumn,
                                                 const void      * aRow,
                                                 UInt            * aColumnSize,
                                                 idBool          * aIsNullValue );

    // PROJ-1705
    // ��ũ���̺��÷��� ����Ÿ�� �����ϴ� �Լ� �˻�
    static IDE_RC findStoredValue2MtdValue(
        const smiColumn            * aColumn,
        smiCopyDiskColumnValueFunc * aStoredValue2MtdValueFunc );

    // PROJ-2429 Dictionary based data compress for on-disk DB 
    // ��ũ���̺��÷��� ����Ÿ Ÿ�Կ� �´� ������ �����Լ� �˻�
    static IDE_RC findStoredValue2MtdValue4DataType(
        const smiColumn            * aColumn,
        smiCopyDiskColumnValueFunc * aStoredValue2MtdValueFunc );

    static IDE_RC findActualSize( const smiColumn   * aColumn,                            
                                  smiActualSizeFunc * aActualSizeFunc );

    // PROJ-1705
    // ��ũ���̺��÷��� ����Ÿ��
    // qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
    static void storedValue2MtdValue( const smiColumn * aColumn,
                                      void            * aDestValue,
                                      UInt              aDestValueOffset,
                                      UInt              aLength,
                                      const void      * aValue );    
                                       

    static IDE_RC cloneTemplate( iduMemory    * aMemory,
                                 mtcTemplate  * aSource,
                                 mtcTemplate  * aDestination );

    static IDE_RC cloneTemplate( iduVarMemList * aMemory,
                                 mtcTemplate   * aSource,
                                 mtcTemplate   * aDestination );

    // PROJ-2527 WITHIN GROUP AGGR
    static void finiTemplate( mtcTemplate * aTemplate );

    static IDE_RC addFloat( mtdNumericType *aValue,
                            UInt            aPrecisionMaximum,
                            mtdNumericType *aArgument1,
                            mtdNumericType *aArgument2 );

    static IDE_RC subtractFloat( mtdNumericType *aValue,
                                 UInt            aPrecisionMaximum,
                                 mtdNumericType *aArgument1,
                                 mtdNumericType *aArgument2 );

    static IDE_RC multiplyFloat( mtdNumericType *aValue,
                                 UInt            aPrecisionMaximum,
                                 mtdNumericType *aArgument1,
                                 mtdNumericType *aArgument2 );

    static IDE_RC divideFloat( mtdNumericType *aValue,
                               UInt            aPrecisionMaximum,
                               mtdNumericType *aArgument1,
                               mtdNumericType *aArgument2 );

    /* PROJ-1517 */
    static IDE_RC modFloat( mtdNumericType *aValue,
                            UInt            aPrecisionMaximum,
                            mtdNumericType *aArgument1,
                            mtdNumericType *aArgument2 );

    static IDE_RC signFloat( mtdNumericType *aValue,
                             mtdNumericType *aArgument1 );

    static IDE_RC absFloat( mtdNumericType *aValue,
                            mtdNumericType *aArgument1 );

    static IDE_RC ceilFloat( mtdNumericType *aValue,
                             mtdNumericType *aArgument1 );

    static IDE_RC floorFloat( mtdNumericType *aValue,
                              mtdNumericType *aArgument1 );

    static IDE_RC numeric2Slong( SLong          *aValue,
                                 mtdNumericType *aArgument1 );

    static void numeric2Double( mtdDoubleType  * aValue,
                                mtdNumericType * aArgument1 );

    // BUG-41194
    static IDE_RC double2Numeric( mtdNumericType *aValue,
                                  mtdDoubleType   aArgument1 );

    static IDE_RC roundFloat( mtdNumericType *aValue,
                              mtdNumericType *aArgument1,
                              mtdNumericType *aArgument2 );

    static IDE_RC truncFloat( mtdNumericType *aValue,
                              mtdNumericType *aArgument1,
                              mtdNumericType *aArgument2 );

    // To fix BUG-12944 ��Ȯ�� precision ������.
    static IDE_RC getPrecisionScaleFloat( const mtdNumericType * aValue,
                                          SInt                 * aPrecision,
                                          SInt                 * aScale );

    // BUG-16531 ����ȭ Conflict �˻縦 ���� Image �� �Լ�
    static IDE_RC isSamePhysicalImage( const mtcColumn * aColumn,
                                       const void      * aRow1,
                                       UInt              aFlag1,
                                       const void      * aRow2,
                                       UInt              aFlag2,
                                       idBool          * aOutIsSame );

    //----------------------
    // mtcColumn�� �ʱ�ȭ
    //----------------------

    // data module �� language module�� ������ ���
    static IDE_RC initializeColumn( mtcColumn       * aColumn,
                                    const mtdModule * aModule,
                                    UInt              aArguments,
                                    SInt              aPrecision,
                                    SInt              aScale );

    // �ش� data module �� language module�� ã�Ƽ� �����ؾ� �ϴ� ���
    static IDE_RC initializeColumn( mtcColumn       * aColumn,
                                    UInt              aDataTypeId,
                                    UInt              aArguments,
                                    SInt              aPrecision,
                                    SInt              aScale );

    // BUG-23012
    // src column���� dest column�� �ʱ�ȭ�ϴ� ���
    static void initializeColumn( mtcColumn  * aDestColumn,
                                  mtcColumn  * aSrcColumn );
    
    // PROJ-2002 Column Security
    // data module �� language module�� ������ ���
    static IDE_RC initializeEncryptColumn( mtcColumn    * aColumn,
                                           const SChar  * aPolicy,
                                           UInt           aEncryptedSize,
                                           UInt           aECCSize );

    static idBool compareOneChar( UChar * aValue1,
                                  UChar   aValue1Size,
                                  UChar * aValue2,
                                  UChar   aValue2Size );

    // PROJ-1597
    static IDE_RC getNonStoringSize( const smiColumn * acolumn, UInt * aOutSize );


    // PROJ-2002 Column Security
    // echar/evarchar type�� like key size�� ���
    static IDE_RC getLikeEcharKeySize( mtcTemplate * aTemplate,
                                       UInt        * aECCSize,
                                       UInt        * aKeySize );
	/* PROJ-2209 DBTIMEZONE */
    static SLong  getSystemTimezoneSecond( void );
    static SChar *getSystemTimezoneString( SChar * aTimezoneString );

    /* PROJ-1517 */
    static void makeFloatConversionModule( mtcStack         *aStack,
                                           const mtdModule **aModule );

    // BUG-37484
    static idBool needMinMaxStatistics( const smiColumn* aColumn );

    /* PROJ-1071 Parallel query */
    static IDE_RC cloneTemplate4Parallel( iduMemory    * aMemory,
                                          mtcTemplate  * aSource,
                                          mtcTemplate  * aDestination );

    /* PROJ-2399 */
    static IDE_RC getColumnStoreLen( const smiColumn * aColumn,
                                     UInt            * aActualColLen );
};

#endif /* _O_MTC_H_ */
