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
 * $Id: qtc.cpp 90923 2021-05-31 01:27:54Z donovan.seo $
 *
 * Description :
 *     QP layer�� MT layer�� �߰��� ��ġ�ϴ� layer��
 *     ����� MT interface layer ������ �Ѵ�.
 *
 *     ���⿡�� QP ��ü�� ���� ���������� ���Ǵ� �Լ��� ���ǵǾ� �ִ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <mte.h>
#include <mtd.h>
#include <mtc.h>
#include <qcg.h>
#include <qtc.h>
#include <qmn.h>
#include <qsv.h>
#include <qcuSqlSourceInfo.h>
#include <mtdTypes.h>
#include <qsf.h>
#include <qcgPlan.h>
#include <qcsModule.h>
#include <qmoOuterJoinOper.h>
#include <qcpManager.h>
#include <mtuProperty.h>
#include <qmsPreservedTable.h>
#include <qcmUser.h>
#include <qtcDef.h>
#include <qsxArray.h>
#include <qmv.h>
#include <qmvQTC.h>
#include <qsvProcStmts.h>

extern mtdModule mtdBoolean;
extern mtdModule mtdUndef;
extern mtdModule mtdBit;
extern mtdModule mtdVarbit;
extern mtdModule mtdByte;
extern mtdModule mtdVarbyte;
extern mtdModule mtdChar;
extern mtdModule mtdVarchar;
extern mtdModule mtdEchar;
extern mtdModule mtdEvarchar;
extern mtdModule mtdNchar;
extern mtdModule mtdNvarchar;
extern mtdModule mtdNumeric;
extern mtdModule mtdFloat;

extern mtfModule mtfGetBlobValue;
extern mtfModule mtfGetClobValue;
extern mtfModule mtfList;
extern mtfModule mtfCast;
extern mtfModule mtfIsNull;
extern mtfModule mtfIsNotNull;
extern mtfModule mtfCase2;
extern mtfModule mtfDecode;
extern mtfModule mtfDigest;
extern mtfModule mtfDump;
extern mtfModule mtfNvl;
extern mtfModule mtfNvl2;
extern mtfModule mtfEqualAny;
extern mtfModule mtfEqual;
extern mtfModule mtfNotEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfLike;
extern mtfModule mtfNotLike;
extern mtfModule mtfLnnvl;
extern mtfModule mtfExists;
extern mtfModule mtfNotExists;
extern mtfModule mtfListagg;
extern mtfModule mtfPercentileCont;
extern mtfModule mtfPercentileDisc;
extern mtfModule mtfRank;
extern mtfModule mtfRankWithinGroup;    // BUG-41631
extern mtfModule mtfPercentRankWithinGroup;    // BUG-41771
extern mtfModule mtfCumeDistWithinGroup;       // BUG-41800
extern mtfModule qsfMCountModule;    // PROJ-2533
extern mtfModule qsfMFirstModule;
extern mtfModule qsfMLastModule;
extern mtfModule qsfMNextModule;
extern mtfModule mtfRatioToReport;
extern mtfModule mtfMin;
extern mtfModule mtfMinKeep;
extern mtfModule mtfMax;
extern mtfModule mtfMaxKeep;
extern mtfModule mtfSum;
extern mtfModule mtfSumKeep;
extern mtfModule mtfAvg;
extern mtfModule mtfAvgKeep;
extern mtfModule mtfCount;
extern mtfModule mtfCountKeep;
extern mtfModule mtfVariance;
extern mtfModule mtfVarianceKeep;
extern mtfModule mtfStddev;
extern mtfModule mtfStddevKeep;

qcDepInfo qtc::zeroDependencies;

smiCallBackFunc qtc::rangeFuncs[] = {
    qtc::rangeMinimumCallBack4Mtd,
    qtc::rangeMinimumCallBack4GEMtd,
    qtc::rangeMinimumCallBack4GTMtd,
    qtc::rangeMinimumCallBack4Stored,
    qtc::rangeMaximumCallBack4Mtd,
    qtc::rangeMaximumCallBack4LEMtd,
    qtc::rangeMaximumCallBack4LTMtd,
    qtc::rangeMaximumCallBack4Stored,
    qtc::rangeMinimumCallBack4IndexKey,
    qtc::rangeMaximumCallBack4IndexKey,
    NULL
};

mtdCompareFunc qtc::compareFuncs[] = {
    qtc::compareMinimumLimit,
    qtc::compareMaximumLimit4Mtd,
    qtc::compareMaximumLimit4Stored,
    NULL
};

/***********************************************************************
 * [qtc::templateRowFlags]
 *
 * Tuple Set�� �� Tuple�� ũ�� ������ ���� �� ������ ������ �� �ִ�.
 *    - MTC_TUPLE_TYPE_CONSTANT     : ��� Tuple
 *    - MTC_TUPLE_TYPE_VARIABLE     : ���� Tuple(Host������ ����)
 *    - MTC_TUPLE_TYPE_INTERMEDIATE : �߰� ��� Tuple
 *    - MTC_TUPLE_TYPE_TABLE        : Table Tuple
 *
 * Stored Procedure�� ������ ���Ͽ� Tuple Set�� ���� Clone �۾��� �ʿ���
 * ���, ���� ���� �پ��� Tuple�� ���Ͽ� �پ��� ó���� �����ϴ�.
 * �ش� Const Structure���� �� Tuple�� ���� ó�� ����� ���� �����
 * �� flag�� ���ǵǾ� �ִ�.
 **********************************************************************/

/* BUG-44490 mtcTuple flag Ȯ�� �ؾ� �մϴ�. */
/* 32bit flag ���� ��� �Ҹ��� 64bit�� ���� */
/* type�� UInt���� ULong���� ���� */
const ULong qtc::templateRowFlags[MTC_TUPLE_TYPE_MAXIMUM] = {
    /* MTC_TUPLE_TYPE_CONSTANT                                       */
    // ��� Tuple�� ���,
    // Column ����, Execute ����, Row ������ ������� �ʴ´�.
    MTC_TUPLE_TYPE_CONSTANT|
    MTC_TUPLE_COLUMN_SET_TRUE|        /* COLUMN  : set pointer       */
    MTC_TUPLE_COLUMN_ALLOCATE_FALSE|  /* COLUMN  : no allocation     */
    MTC_TUPLE_COLUMN_COPY_FALSE|      /* COLUMN  : no copy           */
    MTC_TUPLE_EXECUTE_SET_TRUE|       /* EXECUTE : set pointer       */
    MTC_TUPLE_EXECUTE_ALLOCATE_FALSE| /* EXECUTE : no allocation     */
    MTC_TUPLE_EXECUTE_COPY_FALSE|     /* EXECUTE : no copy           */
    MTC_TUPLE_ROW_SET_TRUE|           /* ROW     : set pointer       */
    MTC_TUPLE_ROW_ALLOCATE_FALSE|     /* ROW     : no allocation     */
    MTC_TUPLE_ROW_COPY_FALSE,         /* ROW     : no copy           */

    /* MTC_TUPLE_TYPE_VARIABLE                                       */
    // Host ������ ������ Tuple�� ���,
    // Column ����, Execute ����, Row ������ Host Binding�� �̷������
    // ������ ��� ������ �����Ǿ� ���� �ʴ�.
    MTC_TUPLE_TYPE_VARIABLE|
    MTC_TUPLE_COLUMN_SET_TRUE|        /* COLUMN  : set pointer       */
    MTC_TUPLE_COLUMN_ALLOCATE_FALSE|  /* COLUMN  : no allocation     */
    MTC_TUPLE_COLUMN_COPY_FALSE|      /* COLUMN  : no copy           */
    MTC_TUPLE_EXECUTE_SET_FALSE|      /* EXECUTE : no set            */
    MTC_TUPLE_EXECUTE_ALLOCATE_TRUE|  /* EXECUTE : allocation        */
    MTC_TUPLE_EXECUTE_COPY_TRUE|      /* EXECUTE : copy              */
    MTC_TUPLE_ROW_SET_FALSE|          /* ROW     : no set            */
    MTC_TUPLE_ROW_ALLOCATE_FALSE|     /* ROW     : no allocation     */
    MTC_TUPLE_ROW_COPY_FALSE,         /* ROW     : no copy           */

    /* MTC_TUPLE_TYPE_INTERMEDIATE                                   */
    // �߰� ����� �ǹ��ϴ� Tuple�� ���,
    // Column ����, Execute ����, Row ������ ������ ����Ǳ� ��������
    // �� ������ ��Ȯ���� ������ �� ����.
    MTC_TUPLE_TYPE_INTERMEDIATE|
    MTC_TUPLE_COLUMN_SET_TRUE|        /* COLUMN  : set pointer       */
    MTC_TUPLE_COLUMN_ALLOCATE_FALSE|  /* COLUMN  : no allocation     */
    MTC_TUPLE_COLUMN_COPY_FALSE|      /* COLUMN  : no copy           */
    MTC_TUPLE_EXECUTE_SET_FALSE|      /* EXECUTE : no set            */
    MTC_TUPLE_EXECUTE_ALLOCATE_TRUE|  /* EXECUTE : allocation        */
    MTC_TUPLE_EXECUTE_COPY_TRUE|      /* EXECUTE : copy              */
    MTC_TUPLE_ROW_SET_FALSE|          /* ROW     : no set            */
    MTC_TUPLE_ROW_ALLOCATE_TRUE|      /* ROW     : allocation        */
    MTC_TUPLE_ROW_COPY_FALSE,         /* ROW     : no copy           */

    /* MTC_TUPLE_TYPE_TABLE                                          */
    // Table �� �ǹ��ϴ� Tuple�� ���,
    // Column ���� �� Execute ������ ������ �ʴ´�.
    // �׷���, Disk Variable Column�� ó���ϱ� ���ؼ���
    // Column ������ �����Ͽ��� �Ѵ�.
    MTC_TUPLE_TYPE_TABLE|
    MTC_TUPLE_COLUMN_SET_TRUE|        /* COLUMN  : set pointer       */
    MTC_TUPLE_COLUMN_ALLOCATE_FALSE|  /* COLUMN  : no allocation     */
    MTC_TUPLE_COLUMN_COPY_FALSE|      /* COLUMN  : no copy           */
    MTC_TUPLE_EXECUTE_SET_TRUE|       /* EXECUTE : set pointer       */
    MTC_TUPLE_EXECUTE_ALLOCATE_FALSE| /* EXECUTE : no allocation     */
    MTC_TUPLE_EXECUTE_COPY_FALSE|     /* EXECUTE : no copy           */
    MTC_TUPLE_ROW_SET_FALSE|          /* ROW     : no set            */
    MTC_TUPLE_ROW_ALLOCATE_FALSE|     /* ROW     : no allocation     */
    MTC_TUPLE_ROW_COPY_FALSE          /* ROW     : no copy           */
};

/* BUG-44382 clone tuple ���ɰ��� */
void qtc::setTupleColumnFlag( mtcTuple * aTuple,
                              idBool     aCopyColumn,
                              idBool     aMemsetRow )
{
    // column ���簡 �ʿ��� ��� (column�� value buffer, offset���� ����ϴ� ���)
    if ( aCopyColumn == ID_TRUE )
    {
        aTuple->lflag &= ~MTC_TUPLE_COLUMN_SET_MASK;
        aTuple->lflag |= MTC_TUPLE_COLUMN_SET_FALSE;
        aTuple->lflag &= ~MTC_TUPLE_COLUMN_ALLOCATE_MASK;
        aTuple->lflag |= MTC_TUPLE_COLUMN_ALLOCATE_TRUE;
        aTuple->lflag &= ~MTC_TUPLE_COLUMN_COPY_MASK;
        aTuple->lflag |= MTC_TUPLE_COLUMN_COPY_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    // row�Ҵ�� memset�� �ʿ��� ��� (�ʱ�ȭ�� �ʿ��� ���)
    if ( aMemsetRow == ID_TRUE )
    {
        aTuple->lflag &= ~MTC_TUPLE_ROW_MEMSET_MASK;
        aTuple->lflag |= MTC_TUPLE_ROW_MEMSET_TRUE;
    }
    else
    {
        // Nothing to do.
    }
}

IDE_RC qtc::rangeMinimumCallBack4Mtd( idBool     * aResult,
                                      const void * aRow,
                                      void       *, /* aDirectKey */
                                      UInt        , /* aDirectKeyPartialSize */
                                      const scGRID,
                                      void       * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Minimum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = aRow;
        sValueInfo1.length = 0; // do not use
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Min Value������ �����ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value
    *aResult = ( sOrder >= 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::rangeMinimumCallBack4GEMtd( idBool       * aResult,
                                        const void   * aRow,
                                        void         * aDirectKey,
                                        UInt           aDirectKeyPartialSize,
                                        const scGRID   aRid,
                                        void         * aData )
{
    return rangeMinimumCallBack4Mtd( aResult, 
                                     aRow, 
                                     aDirectKey, 
                                     aDirectKeyPartialSize, 
                                     aRid, 
                                     aData );
}

IDE_RC qtc::rangeMinimumCallBack4GTMtd( idBool       * aResult,
                                        const void   * aRow,
                                        void         *, /* aDirectKey */
                                        UInt          , /* aDirectKeyPartialSize */
                                        const scGRID   /* aRid */,
                                        void         * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Minimum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = aRow;
        sValueInfo1.length = 0; // do not use
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Min Value������ �����ʿ� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value
    *aResult = ( sOrder > 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::rangeMinimumCallBack4Stored( idBool     * aResult,
                                         const void * aColVal,
                                         void       *, /* aDirectKey */
                                         UInt        , /* aDirectKeyPartialSize */
                                         const scGRID,
                                         void       * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Minimum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    const smiValue     * sColVal;
    SInt                 aOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;
    
    sColVal = (const smiValue*)aColVal;

    for( aOrder  = 0,   sData  = (qtcMetaRangeColumn*)aData;
         aOrder == 0 && sData != NULL;
         sData   = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = sColVal[ sData->columnIdx ].value;
        sValueInfo1.length = sColVal[ sData->columnIdx ].length;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        aOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Min Value������ �����ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value
    *aResult = ( aOrder >= 0 ) ? ID_TRUE : ID_FALSE ;

    return IDE_SUCCESS;
}

SInt qtc::compareMinimumLimit( mtdValueInfo * /* aValueInfo1 */,
                               mtdValueInfo * /* aValueInfo2 */ )
{
    return 1;
}

SInt qtc::compareMaximumLimit4Mtd( mtdValueInfo * aValueInfo1,
                                   mtdValueInfo * /* aValueInfo2 */ )
{
    const void* sValue = mtc::value( aValueInfo1->column,
                                     aValueInfo1->value,
                                     aValueInfo1->flag );

    return (aValueInfo1->column->module->isNull( aValueInfo1->column,
                                                 sValue )
            != ID_TRUE ) ? -1 : 0 ;
}

SInt qtc::compareMaximumLimit4Stored( mtdValueInfo * aValueInfo1,
                                      mtdValueInfo * /* aValueInfo2 */ )
{
    if ( ( aValueInfo1->column->module->flag & MTD_VARIABLE_LENGTH_TYPE_MASK )
         == MTD_VARIABLE_LENGTH_TYPE_TRUE )
    {
        return ( aValueInfo1->length != 0 ) ? -1 : 0;
    }
    else
    {
        return ( idlOS::memcmp( aValueInfo1->value,
                                aValueInfo1->column->module->staticNull,
                                aValueInfo1->column->column.size )
                 != 0 ) ? -1 : 0;
    }
}

IDE_RC qtc::rangeMaximumCallBack4Mtd( idBool     * aResult,
                                      const void * aRow,
                                      void       *, /* aDirectKey */
                                      UInt        , /* aDirectKeyPartialSize */
                                      const scGRID /* aRid */,
                                      void       * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Maximum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = aRow;
        sValueInfo1.length = 0; // do not use
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Max Value������ ���ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value

    *aResult = ( sOrder <= 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::rangeMaximumCallBack4LEMtd( idBool     * aResult,
                                        const void * aRow,
                                        void       * aDirectKey,
                                        UInt         aDirectKeyPartialSize,
                                        const scGRID aRid,
                                        void       * aData )
{
    return rangeMaximumCallBack4Mtd( aResult, 
                                     aRow, 
                                     aDirectKey,
                                     aDirectKeyPartialSize,
                                     aRid, 
                                     aData );
}

IDE_RC qtc::rangeMaximumCallBack4LTMtd( idBool     * aResult,
                                        const void * aRow,
                                        void       *, /* aDirectKey */
                                        UInt        , /* aDirectKeyPartialSize */
                                        const scGRID /* aRid */,
                                        void       * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Maximum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = aRow;
        sValueInfo1.length = 0; // do not use
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        sOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Max Value������ ���ʿ� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value

    *aResult = ( sOrder < 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::rangeMaximumCallBack4Stored( idBool     * aResult,
                                         const void * aColVal,
                                         void       *, /* aDirectKey */
                                         UInt        , /* aDirectKeyPartialSize */
                                         const scGRID /* aRid */,
                                         void       * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta ������ ���� Maximum Key Range CallBack���θ� ���ȴ�.
 *    ����, Disk Variable Column�� ���� ����� �ʿ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    const smiValue     * sColVal;
    SInt                 aOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;

    sColVal = (const smiValue*)aColVal;

    for( aOrder  = 0,   sData  = (qtcMetaRangeColumn*)aData;
         aOrder == 0 && sData != NULL;
         sData   = sData->next )
    {
        sValueInfo1.column = &(sData->columnDesc);
        sValueInfo1.value  = sColVal[ sData->columnIdx ].value;
        sValueInfo1.length = sColVal[ sData->columnIdx ].length;
        sValueInfo1.flag   = MTD_OFFSET_USE;

        sValueInfo2.column = &(sData->valueDesc);
        sValueInfo2.value  = sData->value;
        sValueInfo2.length = 0; // do not use 
        sValueInfo2.flag   = MTD_OFFSET_USELESS;

        aOrder = sData->compare( &sValueInfo1,
                                 &sValueInfo2 );
    }

    // Column ���� Max Value������ ���ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value

    *aResult = ( aOrder <= 0 ) ? ID_TRUE : ID_FALSE ;

    return IDE_SUCCESS;
}

/*
 * PROJ-2433
 * Direct key Index�� direct key�� mtd�� ���ϴ� range callback �Լ�
 * - index�� ù��° �÷��� direct key�� ����
 * - partial direct key�� ó���ϴºκ� �߰�
 */
IDE_RC qtc::rangeMinimumCallBack4IndexKey( idBool     * aResult,
                                           const void * aRow,
                                           void       * aDirectKey,
                                           UInt         aDirectKeyPartialSize,
                                           const scGRID,
                                           void       * aData )
{
    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;
    mtcColumn            sDummyColumn;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        if ( aDirectKey != NULL )
        {
            /*
             * PROJ-2433 Direct Key Index
             * direct key�� NULL �� �ƴѰ��, ù��° �÷��� direct key�� ���Ѵ�.
             *
             * - aDirectKeyPartialSize�� 0 �� �ƴѰ��,
             *   partial direct key �̹Ƿ� MTD_PARTIAL_KEY_ON �� flag�� �����Ѵ�.
             *
             * - partial direct key�� ���,
             *   �� ����� ��Ȯ�� ���� �ƴϹǷ� �ι�° �÷� �񱳾��� �ٷ� �����Ѵ�.
             */
            sDummyColumn.column.offset = 0;
            sDummyColumn.module = sData->valueDesc.module; /* BUG-40530 & valgirnd */

            sValueInfo1.column = &sDummyColumn;
            sValueInfo1.value  = aDirectKey;
            sValueInfo1.length = aDirectKeyPartialSize;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            if ( aDirectKeyPartialSize != 0 )
            {
                sValueInfo1.flag &= ~MTD_PARTIAL_KEY_MASK;
                sValueInfo1.flag |= MTD_PARTIAL_KEY_ON;
            }
            else
            {
                sValueInfo1.flag &= ~MTD_PARTIAL_KEY_MASK;
                sValueInfo1.flag |= MTD_PARTIAL_KEY_OFF;
            }

            sValueInfo2.column = &(sData->valueDesc);
            sValueInfo2.value  = sData->value;
            sValueInfo2.length = 0;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            sOrder = sData->compare( &sValueInfo1,
                                     &sValueInfo2 );

            if ( aDirectKeyPartialSize != 0 )
            {
                /* partial key �̸�,
                 * ���� �÷��� �񱳴��ǹ̾���. �ٷγ����� */
                break;
            }
            else
            {
                aDirectKey = NULL; /* direct key�� �ѹ��� ���� */
            }
        }
        else
        {
            sValueInfo1.column = &(sData->columnDesc);
            sValueInfo1.value  = aRow;
            sValueInfo1.length = 0;
            sValueInfo1.flag   = MTD_OFFSET_USE;

            sValueInfo2.column = &(sData->valueDesc);
            sValueInfo2.value  = sData->value;
            sValueInfo2.length = 0;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            sOrder = sData->compare( &sValueInfo1,
                                     &sValueInfo2 );
        }
    }

    // Column ���� Min Value������ �����ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value
    *aResult = ( sOrder >= 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::rangeMaximumCallBack4IndexKey( idBool     * aResult,
                                           const void * aRow,
                                           void       * aDirectKey,
                                           UInt         aDirectKeyPartialSize,
                                           const scGRID /* aRid */,
                                           void       * aData )
{
    SInt                 sOrder;
    qtcMetaRangeColumn * sData;
    mtdValueInfo         sValueInfo1;
    mtdValueInfo         sValueInfo2;
    mtcColumn            sDummyColumn;

    sOrder = 0;

    for ( sData = (qtcMetaRangeColumn*)aData ;
          ( sOrder == 0 ) && ( sData != NULL ) ;
          sData = sData->next )
    {
        if ( aDirectKey != NULL )
        {
            /*
             * PROJ-2433 Direct Key Index
             * direct key�� NULL �� �ƴѰ��, ù��° �÷��� direct key�� ���Ѵ�.
             *
             * - aDirectKeyPartialSize�� 0 �� �ƴѰ��,
             *   partial direct key �̹Ƿ� MTD_PARTIAL_KEY_ON �� flag�� �����Ѵ�.
             *
             * - partial direct key�� ���,
             *   �� ����� ��Ȯ�� ���� �ƴϹǷ� �ι�° �÷� �񱳾��� �ٷ� �����Ѵ�.
             */
            sDummyColumn.column.offset = 0;
            sDummyColumn.module = sData->valueDesc.module; /* BUG-40530 & valgirnd */

            sValueInfo1.column = &sDummyColumn;
            sValueInfo1.value  = aDirectKey;
            sValueInfo1.length = aDirectKeyPartialSize;
            sValueInfo1.flag   = MTD_OFFSET_USELESS;

            if ( aDirectKeyPartialSize != 0 )
            {
                sValueInfo1.flag &= ~MTD_PARTIAL_KEY_MASK;
                sValueInfo1.flag |= MTD_PARTIAL_KEY_ON;
            }
            else
            {
                sValueInfo1.flag &= ~MTD_PARTIAL_KEY_MASK;
                sValueInfo1.flag |= MTD_PARTIAL_KEY_OFF;
            }

            sValueInfo2.column = &(sData->valueDesc);
            sValueInfo2.value  = sData->value;
            sValueInfo2.length = 0;
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            sOrder = sData->compare( &sValueInfo1,
                                     &sValueInfo2 );

            if ( aDirectKeyPartialSize != 0 )
            {
                /* partial key �̸�,
                 * ���� �÷��� �񱳴��ǹ̾���. �ٷγ����� */
                break;
            }
            else
            {
                aDirectKey = NULL; /* direct key�� �ѹ��� ���� */
            }
        }
        else
        {
            sValueInfo1.column = &(sData->columnDesc);
            sValueInfo1.value  = aRow;
            sValueInfo1.length = 0; // do not use
            sValueInfo1.flag   = MTD_OFFSET_USE;

            sValueInfo2.column = &(sData->valueDesc);
            sValueInfo2.value  = sData->value;
            sValueInfo2.length = 0; // do not use
            sValueInfo2.flag   = MTD_OFFSET_USELESS;

            sOrder = sData->compare( &sValueInfo1,
                                     &sValueInfo2 );
        }
    }

    // Column ���� Max Value������ ���ʿ� �ְų� ���� ���
    // TRUE ���� Setting�Ѵ�.
    //     ------------- Column -------------
    //     |                                |
    //     V                                V
    //  Min Value                        Max Value

    *aResult = ( sOrder <= 0 ) ? ID_TRUE : ID_FALSE;

    return IDE_SUCCESS;
}

IDE_RC qtc::initConversionNodeIntermediate( mtcNode** aConversionNode,
                                            mtcNode*  aNode,
                                            void*     aInfo )
{
/***********************************************************************
 *
 * Description :
 *
 *    Conversion Node�� Intermediate Tuple�� �����ϰ� �ʱ�ȭ��
 *    mtcCallBack.initConversionNode �� �Լ� �����Ϳ� ���õ�.
 *    HOST ������ ���� ����� Conversion�� ���� ����Ѵ�.
 *    Prepare ����(P-V-O)���� ����
 *
 *    Ex) int1 = double1
 *
 * Implementation :
 *
 *    Conversion Node�� ���� ������ �Ҵ�ް�, ��� Node�� ������.
 *    Tuple Set���� INTERMEDIATE TUPLE�� ������ �����.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::initConversionNodeIntermediate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcCallBackInfo* sInfo;

    sInfo = (qtcCallBackInfo*)aInfo;

    IDU_LIMITPOINT("qtc::initConversionNodeIntermediate::malloc");
    IDE_TEST(sInfo->memory->alloc( idlOS::align8((UInt)ID_SIZEOF(qtcNode)),
                                   (void**)aConversionNode )
             != IDE_SUCCESS);

    *(qtcNode*)*aConversionNode = *(qtcNode*)aNode;

    // PROJ-1362
    (*aConversionNode)->baseTable = aNode->baseTable;
    (*aConversionNode)->baseColumn = aNode->baseColumn;

    IDE_TEST( qtc::nextColumn( sInfo->memory,
                               (qtcNode*)*aConversionNode,
                               sInfo->tmplate->stmt,
                               sInfo->tmplate,
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               1 )
              != IDE_SUCCESS );

    /* BUG-44526 INTERMEDIATE Tuple Row�� �ʱ�ȭ���� �ʾƼ�, valgrind warning �߻� */
    // �ʱ�ȭ�� �ʿ���
    setTupleColumnFlag( &(sInfo->tmplate->tmplate.rows[((qtcNode *)*aConversionNode)->node.table]),
                        ID_FALSE,
                        ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::estimateNode( qtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          mtcCallBack* aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Node�� estimate �� ������
 *
 * Implementation :
 *    �ش� Node�� estimate�� �����ϰ�
 *    PRIOR Column�� ���� Validation�� ������.
 *
 ***********************************************************************/
#define IDE_FN "IDE_RC qtc::estimateNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcuSqlSourceInfo      sqlInfo;
    qtcCallBackInfo*      sCallBackInfo = (qtcCallBackInfo*)(aCallBack->info);
    qcStatement*          sStatement = sCallBackInfo->statement;
    qmsSFWGH*             sSFWGHOfCallBack = sCallBackInfo->SFWGH;
    qmsQuerySet         * sQuerySetCallBack = sCallBackInfo->querySet;
    UInt                  sSqlCode;

    IDE_TEST_RAISE( (SInt)(aNode->node.lflag&MTC_NODE_ARGUMENT_COUNT_MASK) >=
                    aRemain,
                    ERR_STACK_OVERFLOW );

    // �ش� Node�� estimate �� ������.
    if ( aNode->node.module->estimate( (mtcNode*)aNode,
                                       aTemplate,
                                       aStack,
                                       aRemain,
                                       aCallBack )
         != IDE_SUCCESS )
    {
        // subquery�� ��� QP���ؿ��� ������ ó�������Ƿ� skip�Ѵ�.
        IDE_TEST( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                  == MTC_NODE_OPERATOR_SUBQUERY );

        sqlInfo.setSourceInfo( ((qcTemplate*)aTemplate)->stmt,
                               & aNode->position );
        IDE_RAISE( ERR_PASS );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-39683
    // ���ڰ� undef type�̸� ���ϵ� undef type�̾�� �Ѵ�.
    if ( ( ( aNode->node.lflag & MTC_NODE_UNDEF_TYPE_MASK )
           == MTC_NODE_UNDEF_TYPE_EXIST ) &&
         ( ( ( aNode->node.module->lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_FUNCTION ) ||
           ( ( aNode->node.module->lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_AGGREGATION ) ) &&
         ( aStack->column->module->id != MTD_LIST_ID ) )
    {
        IDE_TEST( mtc::initializeColumn( aStack->column,
                                         & mtdUndef,
                                         0,
                                         0,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------------------------------------
    // Hierarchy ������ ���� ���ǿ��� PRIOR keyword�� ����� �� ����.
    // �̿� ���� Validation�� ������
    //----------------------------------------------------------------
    if (sSFWGHOfCallBack != NULL)
    {
        if ( ( ( aNode->lflag & QTC_NODE_PRIOR_MASK )
               == QTC_NODE_PRIOR_EXIST )
             && ( sSFWGHOfCallBack->hierarchy == NULL ) )
        {
            sqlInfo.setSourceInfo( sStatement,
                                   & aNode->position );
            IDE_RAISE( ERR_PRIOR_WITHOUT_CONNECTBY );
        }
    }
  
    // PROJ-2462 Reuslt Cache
    if ( sQuerySetCallBack != NULL )
    {
        // PROJ-2462 Result Cache
        if ( ( ( aNode->node.lflag & MTC_NODE_VARIABLE_MASK )
               == MTC_NODE_VARIABLE_TRUE ) ||
             ( ( aNode->lflag & QTC_NODE_LOB_COLUMN_MASK )
               == QTC_NODE_LOB_COLUMN_EXIST ) ||
             ( ( aNode->lflag & QTC_NODE_COLUMN_RID_MASK )
               == QTC_NODE_COLUMN_RID_EXIST ) )
        {
            sQuerySetCallBack->lflag &= ~QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
            sQuerySetCallBack->lflag |= QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE;
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

    // PROJ-2204 Join Update, Delete
    if ( sSFWGHOfCallBack != NULL )
    {
        switch ( sSFWGHOfCallBack->validatePhase )
        {
            case QMS_VALIDATE_FROM:
                IDE_TEST( qmsPreservedTable::addOnCondPredicate( sStatement,
                                                                 sSFWGHOfCallBack,
                                                                 sCallBackInfo->from,
                                                                 aNode )
                          != IDE_SUCCESS );
                break;
 
            case QMS_VALIDATE_WHERE:
                IDE_TEST( qmsPreservedTable::addPredicate( sStatement,
                                                           sSFWGHOfCallBack,
                                                           aNode )
                          != IDE_SUCCESS );
                break;
 
            default:
                break;
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION( ERR_PRIOR_WITHOUT_CONNECTBY );
    {
        (void)sqlInfo.init(sCallBackInfo->memory);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_PRIOR_WITHOUT_CONNECTBY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_PASS );
    {
        // sqlSourceInfo�� ���� error���.
        if ( ideHasErrorPosition() == ID_FALSE )
        {
            sSqlCode = ideGetErrorCode();

            (void)sqlInfo.initWithBeforeMessage(sCallBackInfo->memory);

            IDE_SET(
                ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                                sqlInfo.getBeforeErrMessage(),
                                sqlInfo.getErrMessage()));
            (void)sqlInfo.fini();

            // overwrite wrapped errorcode to original error code.
            ideGetErrorMgr()->Stack.LastError = sSqlCode;
        }
        else
        {
            // Nothing to do.
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// To Fix BUG-11921(A3) 11920(A4)
//
// estimateInternal ���� arguments�� ����
// recursive�ϰ� estimateInternal �� �ϳ��� ȣ�����ֱ� ����,
// ���� ����ڰ� �������� ���� �⺻ ���ڸ� aNode->arguments ��
// ���ٿ� �־�� �ϴ��� �Ǵ��ϰ�, �ʿ��ϴٸ� ����Ʈ ���� ����Ű��
// qtcNode���� ���� aNode->arguments�� �� ���� �Ŵ޵��� �Ѵ�.

IDE_RC qtc::appendDefaultArguments( qtcNode*     aNode,
                                    mtcTemplate* aTemplate,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    mtcCallBack* aCallBack )
{
    qtcNode  * sNode;
    mtcStack * sStack;
    SInt       sRemain;

    if ( aNode->node.module == & qtc::spFunctionCallModule )
    {
        // BUG-41262 PSM overloading
        // PSM ������ ��� �̸� estimate �� �ؾ� overloading �� �����ϴ�.
        // ��� ��쿡 �̸� ó���Ҽ� �����Ƿ� column �� ó���Ѵ�.

        // BUG-44518 Stack �� ����� �����ؾ� �Ѵ�.
        for( sNode  = (qtcNode*)aNode->node.arguments,
             sStack = aStack + 1, sRemain = aRemain - 1;
             sNode != NULL;
             sNode  = (qtcNode*)sNode->node.next, sStack++, sRemain-- )
        {
            if ( sNode->node.module == &qtc::columnModule )
            {
                // PROJ-2533
                // array�� index�� primitive type�� ������ ���°��� �ƴϱ� ������,
                // arguments�� ���� estimate�� �ʿ��ϴ�.
                // e.g. func1( arrayVar[ func2() ] )
                IDE_TEST( estimateInternal( sNode,
                                            aTemplate,
                                            sStack,
                                            sRemain,
                                            aCallBack )
                          != IDE_SUCCESS );
            }
            else
            {
                // nothing to do.
            }
        }

        IDE_TEST( qsv::createExecParseTreeOnCallSpecNode( aNode, aCallBack )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::addLobValueFuncForSP( qtcNode      * aNode,
                                  mtcTemplate  * aTemplate,
                                  mtcStack     * aStack,
                                  SInt        /* aRemain */,
                                  mtcCallBack  * aCallBack )
{
    qcTemplate  * sTemplate = (qcTemplate*) aTemplate;
    qtcNode     * sNewNode[2];
    qtcNode     * sNode;
    qtcNode     * sPrevNode = NULL;
    mtfModule   * sGetLobModule = NULL;
    mtcColumn   * sColumn;
    mtcStack    * sStack;
    mtcStack      sInternalStack[2];

    if ( aNode->node.module == & qtc::spFunctionCallModule )
    {
        for( sNode  = (qtcNode*)aNode->node.arguments, sStack = aStack + 1;
             sNode != NULL;
             sNode  = (qtcNode*)sNode->node.next, sStack++ )
        {
            sColumn = QTC_TMPL_COLUMN( sTemplate, (qtcNode*) sNode );
            
            if ( QTC_TEMPLATE_IS_COLUMN( sTemplate, (qtcNode*) sNode ) == ID_TRUE )
            {
                if ( sColumn->module->id == MTD_BLOB_ID )
                {
                    sGetLobModule = & mtfGetBlobValue;
                }
                else if ( sColumn->module->id == MTD_CLOB_ID )
                {
                    sGetLobModule = & mtfGetClobValue;
                }
                else
                {
                    sGetLobModule = NULL;
                }

                if ( sGetLobModule != NULL )
                {
                    // get_lob_value �Լ��� �����.
                    IDE_TEST( makeNode( sTemplate->stmt,
                                        sNewNode,
                                        & sNode->columnName,
                                        sGetLobModule )
                              != IDE_SUCCESS );
                    
                    // �Լ��� �����Ѵ�.
                    sNewNode[0]->node.arguments = (mtcNode*)sNode;
                    sNewNode[0]->node.next = sNode->node.next;
                    sNewNode[0]->node.arguments->next = NULL;

                    sNewNode[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
                    sNewNode[0]->node.lflag |= 1;
                    
                    qtc::dependencySetWithDep( & sNewNode[0]->depInfo,
                                               & sNode->depInfo );

                    // input argument
                    sInternalStack[1] = *sStack;
                    
                    // get_lob_value�� stack�� 1���� ����ؾ��Ѵ�.
                    IDE_TEST( estimateNode( sNewNode[0],
                                            aTemplate,
                                            sInternalStack,
                                            2,
                                            aCallBack )
                              != IDE_SUCCESS );

                    // result
                    *sStack = sInternalStack[0];

                    if ( sPrevNode != NULL )
                    {
                        sPrevNode->node.next = (mtcNode*)sNewNode[0];
                    }
                    else
                    {
                        aNode->node.arguments = (mtcNode*)sNewNode[0];
                    }

                    sPrevNode = sNewNode[0];
                    sNode = sNewNode[0];
                }
                else
                {
                    sPrevNode = sNode;
                }
            }
            else
            {
                sPrevNode = sNode;
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::estimateInternal( qtcNode*     aNode,
                              mtcTemplate* aTemplate,
                              mtcStack*    aStack,
                              SInt         aRemain,
                              mtcCallBack* aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Node Tree�� estimate�Ѵ�.
 *
 * Implementation :
 *    Node Tree�� Traverse�ϸ� Node Tree��ü�� estimate �Ѵ�.
 *    Aggregation �����ڿ� ���� Ư���� �����ڴ� �� Ư¡�� �°� �з��Ѵ�.
 *    (1) Argument Node�鿡 ���� estimate ����
 *    (2) over clause ���� ���� estimate ����
 *    (3) ���� Node�� ���� estimate ����
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::estimateInternal"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode           * sNode;
    mtcStack          * sStack;
    SInt                sRemain;
    qtcCallBackInfo   * sInfo;
    ULong               sLflag;
    idBool              sIsAggrNode;
    idBool              sIsAnalyticFuncNode;
    UInt                sSqlCode;
    qcuSqlSourceInfo    sSqlInfo;
    qtcNode           * sPassNode;
    mtcNode           * sNextNodePtr;
    mtcNode           * sPassArgNode;
    qtcOverColumn     * sOverColumn;

    // PROJ-2533
    // array(columnModule/member function) ����
    // proc/func(spFunctionCallModule)������ estimate���� ���� �ؾ��Ѵ�.
    IDE_TEST( qmvQTC::changeModuleToArray ( aNode,
                                            aCallBack ) 
              != IDE_SUCCESS);

    // Node�� dependencies �ʱ�ȭ
    dependencyClear( & aNode->depInfo );

    // sLflag �ʱ�ȭ
    sLflag = MTC_NODE_BIND_TYPE_TRUE;

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_AGGREGATION  )
    {
        sInfo = (qtcCallBackInfo*)aCallBack->info;
        sIsAggrNode = ID_TRUE;
    }
    else
    {
        sInfo = NULL;
        sIsAggrNode = ID_FALSE;
    }

    // BUG-41243 
    // spFunctionCallModule�� �ƴ� ���, Name-based Argument�� �����ϸ� �� �ȴ�.
    // (e.g. replace(P1=>'P', 'a')�� �ϸ� P1, 'P', 'a' 3���� argument�� �ν�)
    IDE_TEST_RAISE( ( aNode->node.module != & qtc::spFunctionCallModule ) &&
                    ( qtc::hasNameArgumentForPSM( aNode ) == ID_TRUE ),
                    NAME_NOTATION_NOT_ALLOWED );

    // To Fix BUG-11921(A3) 11920(A4)
    // Argument Node�鿡 ���� estimateInternal ���� ���� Default Argument�� �����Ѵ�.
    IDE_TEST( appendDefaultArguments(aNode, aTemplate, aStack,
                                     aRemain, aCallBack   )
              != IDE_SUCCESS );


    // Analytic Function�� ���ϴ� Į������ ���� ���� ����
    if ( aNode->overClause != NULL )
    {
        sIsAnalyticFuncNode = ID_TRUE;
        aNode->lflag &= ~QTC_NODE_ANAL_FUNC_COLUMN_MASK;
        aNode->lflag |= QTC_NODE_ANAL_FUNC_COLUMN_TRUE;
    }
    else
    {
        // analytic function�� ���ϴ� Į���� ���
        if ( ( aNode->lflag & QTC_NODE_ANAL_FUNC_COLUMN_MASK )
             == QTC_NODE_ANAL_FUNC_COLUMN_TRUE )
        {
            sIsAnalyticFuncNode = ID_TRUE;
        }
        else
        {   
            sIsAnalyticFuncNode = ID_FALSE;
        }
    }
   
    //-----------------------------------------
    // Argument Node�鿡 ���� estimateInternal ����
    //-----------------------------------------

    for( sNode  = (qtcNode*)aNode->node.arguments,
             sStack = aStack + 1, sRemain = aRemain - 1;
         sNode != NULL;
         sNode  = (qtcNode*)sNode->node.next, sStack++, sRemain-- )
    {       
        // BUG-44367
        // prevent thread stack overflow
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );

        /* PROJ-2197 PSM Renewal */
        if( (aNode->lflag & QTC_NODE_COLUMN_CONVERT_MASK)
            == QTC_NODE_COLUMN_CONVERT_TRUE )
        {
            sNode->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
        }
        else
        {
            /* nothing to do */
        }

        if( sIsAnalyticFuncNode == ID_TRUE )
        {
            sNode->lflag &= ~QTC_NODE_ANAL_FUNC_COLUMN_MASK;
            sNode->lflag |= QTC_NODE_ANAL_FUNC_COLUMN_TRUE;
        }
        else
        {
            // nothing to do 
        }

        //-----------------------------------------------------
        // BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻�
        //  ��, <SIMPLE CASE>�� expr�� subquery ���� �����߻�.
        // 
        // �Ʒ� �ڵ尡 �̻��� ���̰�����,
        // branch���� ��ġ ȣȯ�� ������
        // �Ʒ� ������ ����ó���� �� ���� �ε��� �����ؾ� �ϴ� �����
        // �ļ�ó�� ���̴� �ڵ尡 �߰���.
        // 
        // ��) case ( select count(*) from dual )
        //     when 0 then '000'
        //     when 1 then '111'
        //     else 'asdf'
        //     �� ������ �Ľ̰������� ��庹�縦 ���� ������ ���� ��尡 ���������.
        //     ������, ���� �������� subQuery�� ��庹�縦 �� �� ����.
        // 
        //      [ CASE ] <--MTC_NODE_CASE_TYPE_SIMPLE �÷��װ� ������.
        //          |
        //          |                     / pass node�� �޾ƾ� �Ѵٴ� �÷��װ� ������.
        //          |                    / (QTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_TRUE)
        //          |                   \/
        //         [=]  ---  ['000'] - [=] --- ['111'] -  ['asdf']
        //          |                   |       
        //          |                   |
        //        [subQ] - [0]        [subQ] - [1]
        //
        //     estimate��������
        //     CASE����� ù��° = (��, [subQ]=[0])������ subQuery�� estimate�� �����ϰ�,
        //     ������ = (��, [subQ]=[1])�� ù��° subQuery�� PASS node�� �����ϰ�,
        //     subQuery�� �ߺ� estimate���� �ʵ��� �Ѵ�.
        //     ����, subQuery�� �������� �ʰ� ó���� �� �ִ�.
        //
        //      [ CASE ]
        //          |
        //         [=]  ---  ['000'] - [=] --- ['111'] -  ['asdf']
        //          |                   |
        //          |                [PASS] - [1]
        //          |-------------------|
        //        [subQ] - [0]      
        //
        //-----------------------------------------------------
        if( ( aNode->node.lflag & MTC_NODE_CASE_TYPE_MASK ) == MTC_NODE_CASE_TYPE_SIMPLE )
        {
            if( ( ( ((qtcNode*)(aNode->node.arguments))->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
                &&
                ( ( sNode->node.lflag & MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_MASK )
                  == MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_TRUE ) ) 
            {
                sNextNodePtr = sNode->node.arguments->next;

                sInfo = (qtcCallBackInfo*)aCallBack->info;

                IDE_TEST( qtc::makePassNode(
                              sInfo->statement,
                              NULL,
                              (qtcNode*)(aNode->node.arguments->arguments),
                              &sPassNode ) != IDE_SUCCESS );

                sPassNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
                sPassNode->node.lflag |= MTC_NODE_INDIRECT_FALSE;

                // BUG-28446 [valgrind], BUG-38133
                // qtcCalculate_Pass(mtcNode*, mtcStack*, int, void*, mtcTemplate*)
                // (qtcPass.cpp:333)
                // SIMPLE CASEó���� ���� �ʿ信 ���� ������ PASS NODE���� ǥ��
                // skipModule�̸� size�� 0�̴�.
                sPassNode->node.lflag &= ~MTC_NODE_CASE_EXPRESSION_PASSNODE_MASK;
                sPassNode->node.lflag |= MTC_NODE_CASE_EXPRESSION_PASSNODE_TRUE;
                
                sPassNode->node.next = sNextNodePtr;
                sNode->node.arguments = (mtcNode*)sPassNode;

                // BUG-44518 order by ������ ESTIMATE �ߺ� �����ϸ� �ȵ˴ϴ�.
                // Alias �� ������ fatal �� �߻��Ҽ� �ֽ��ϴ�.
                // select i1 as i2 , i2 as i1 from t1 order by func1(i1,1);
                // SELECT  i1+i1 AS i1 FROM t1 order by func1(i1,1);
                if ( (sNode->lflag & QTC_NODE_ORDER_BY_ESTIMATE_MASK)
                    == QTC_NODE_ORDER_BY_ESTIMATE_FALSE )
                {
                    IDE_TEST( estimateInternal( sNode,
                                                aTemplate,
                                                sStack,
                                                sRemain,
                                                aCallBack )
                            != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do.
                }
            }
            else
            {
                // BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻�
                // <SIMPLE CASE>�� expr�� subquery�� �ƴ� ���,
                // PASS NODE�� ������ �÷��׸� �����Ѵ�.
                sNode->node.lflag &= ~MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_MASK;
                sNode->node.lflag |= MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_FALSE;

                // BUG-44518 order by ������ ESTIMATE �ߺ� �����ϸ� �ȵ˴ϴ�.
                // Alias �� ������ fatal �� �߻��Ҽ� �ֽ��ϴ�.
                // select i1 as i2 , i2 as i1 from t1 order by func1(i1,1);
                // SELECT  i1+i1 AS i1 FROM t1 order by func1(i1,1);
                if ( (sNode->lflag & QTC_NODE_ORDER_BY_ESTIMATE_MASK)
                    == QTC_NODE_ORDER_BY_ESTIMATE_FALSE )
                {
                    IDE_TEST( estimateInternal( sNode,
                                                aTemplate,
                                                sStack,
                                                sRemain,
                                                aCallBack )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do.
                }
            }
        }
        else
        {
            if( ( ( aNode->node.lflag & MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_MASK )
                == MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_TRUE ) &&
                ( (qtcNode*)(aNode->node.arguments) == sNode ) )
            {
                sPassArgNode =
                    qtc::getCaseSubExpNode( (mtcNode*)(sNode->node.arguments) );

                sStack->column = aTemplate->rows[sPassArgNode->table].columns +
                    sPassArgNode->column;
            }
            else
            {
                /* PSM Array�� Index Node�� bind ������ �������� �ʴ´�.
                 * �ֳ��ϸ� Array�״�� binding �ϱ� �����̴�.
                 * ex) SELECT I1 INTO V1 FROM T1 WHERE I1 = ARR1[INDEX1];
                 *                                          ^^   ^^
                 *  -> SELECT I1         FROM T1 WHERE I1 = ?           ;
                 *     ?�� ARR1[INDEX1] */
                if( ( ( aNode->lflag & QTC_NODE_SP_ARRAY_INDEX_VAR_MASK )
                      == QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST ) &&
                    ( ( sNode->lflag & QTC_NODE_COLUMN_CONVERT_MASK )
                      == QTC_NODE_COLUMN_CONVERT_TRUE ) )
                {
                    sNode->lflag &= ~QTC_NODE_COLUMN_CONVERT_MASK;
                    sNode->lflag |= QTC_NODE_COLUMN_CONVERT_FALSE;
                }

                // BUG-44518 order by ������ ESTIMATE �ߺ� �����ϸ� �ȵ˴ϴ�.
                // Alias �� ������ fatal �� �߻��Ҽ� �ֽ��ϴ�.
                // select i1 as i2 , i2 as i1 from t1 order by func1(i1,1);
                // SELECT  i1+i1 AS i1 FROM t1 order by func1(i1,1);
                if ( (sNode->lflag & QTC_NODE_ORDER_BY_ESTIMATE_MASK)
                    == QTC_NODE_ORDER_BY_ESTIMATE_FALSE )
                {
                    IDE_TEST( estimateInternal( sNode,
                                                aTemplate,
                                                sStack,
                                                sRemain,
                                                aCallBack )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do.
                }
            }
        }

        //------------------------------------------------------
        // Argument�� ���� �� �ʿ��� ������ ��� �����Ѵ�.
        //    [Index ��� ���� ����]
        //     aNode->module->mask : ���� Node�� column�� ���� ���,
        //     ���� ����� flag�� index�� ����� �� ������ Setting�Ǿ� ����.
        //     �� ��, ������ ����� Ư���� �ǹ��ϴ� mask�� �̿��� flag��
        //     ����������μ� index�� Ż �� ������ ǥ���� �� �ִ�.
        //------------------------------------------------------

        aNode->node.lflag |=
            sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

        //------------------------------------------------------
        // PROJ-1492
        // Argument�� bind���� lflag�� ��� ������.
        //    1. BIND_TYPE_FALSE�� �ϳ��� ������ BIND_TYPE_FALSE�� �ȴ�.
        //------------------------------------------------------

        if( ( sNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST )
        {
            if( ( sNode->node.lflag & MTC_NODE_BIND_TYPE_MASK ) ==
                MTC_NODE_BIND_TYPE_FALSE )
            {
                sLflag &= ~MTC_NODE_BIND_TYPE_MASK;
                sLflag |= MTC_NODE_BIND_TYPE_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        // Argument�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( dependencyOr( & aNode->depInfo,
                                & sNode->depInfo,
                                & aNode->depInfo ) != IDE_SUCCESS );
    }

    if( ( aNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST )
    {
        if( QTC_IS_TERMINAL( aNode ) == ID_FALSE )
        {
            //------------------------------------------------------
            // PROJ-1492
            // ���� ��尡 �ִ� �ش� Node�� bind���� lflag�� �����Ѵ�.
            //    1. Argument�� BIND_TYPE_FALSE�� �ϳ��� ������ BIND_TYPE_FALSE�� �ȴ�.
            //       (��, �ش� Node�� CAST�Լ� ����� ��� �׻� BIND_TYPE_TRUE�� �ȴ�.)
            //------------------------------------------------------
            aNode->node.lflag |= sLflag & MTC_NODE_BIND_TYPE_MASK;

            if( aNode->node.module == & mtfCast )
            {
                aNode->node.lflag |= MTC_NODE_BIND_TYPE_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sInfo = (qtcCallBackInfo*)aCallBack->info;
            if ( (sInfo->statement != NULL) &&
                 (sInfo->statement->spvEnv->createProc != NULL) )
            {
                IDE_TEST( qsvProcStmts::makeUsingParam( NULL,
                                                        aNode,
                                                        aCallBack )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------
    // func1(c1) ó�� lob column�� ���ڷ� �޴� function���� ó���ϱ� ���Ͽ�
    // func1(get_clob_value(c1)) �������� get_clob_value �Լ��� �ٿ��ִ´�.
    //-----------------------------------------
    
    IDE_TEST( addLobValueFuncForSP( aNode,
                                    aTemplate,
                                    aStack,
                                    aRemain,
                                    aCallBack )
              != IDE_SUCCESS );
    
    //-----------------------------------------
    // ���� Node�� ���� estimate ����
    //-----------------------------------------

    IDE_TEST( estimateNode( aNode,
                            aTemplate,
                            aStack,
                            aRemain,
                            aCallBack )
              != IDE_SUCCESS );

    //----------------------------------------------------
    // over ���� ������ analytic function ��
    // validation �� partition by column�� ���� estimate
    //----------------------------------------------------

    if( aNode->overClause != NULL )
    {
        IDE_TEST( estimate4OverClause( aNode,
                                       aTemplate,
                                       aStack,
                                       aRemain,
                                       aCallBack )
                  != IDE_SUCCESS );

        // BUG-27457
        // analytic function�� ������ ����
        aNode->lflag &= ~QTC_NODE_ANAL_FUNC_MASK;
        aNode->lflag |= QTC_NODE_ANAL_FUNC_EXIST;

        // BUG-41013
        // over �� �ȿ� ���� _prowid �� flag ������ �ʿ��ϴ�.
        for ( sOverColumn = aNode->overClause->overColumn;
              sOverColumn != NULL;
              sOverColumn = sOverColumn->next )
        {
            if ( (sOverColumn->node->lflag & QTC_NODE_COLUMN_RID_MASK)
                 == QTC_NODE_COLUMN_RID_EXIST )
            {
                aNode->lflag &= ~QTC_NODE_COLUMN_RID_MASK;
                aNode->lflag |= QTC_NODE_COLUMN_RID_EXIST;

                break;
            }
        }
    }
    else
    {
        if ( ( aNode->node.lflag & MTC_NODE_FUNCTION_ANALYTIC_MASK )
             == MTC_NODE_FUNCTION_ANALYTIC_TRUE )
        {
            sSqlInfo.setSourceInfo( sInfo->statement,
                                    & aNode->position );
            IDE_RAISE( ERR_MISSING_OVER_IN_WINDOW_FUNCTION );
        }
        else
        {
            // Nothing to do.
        }
    }

    // PROJ-1789 PROWID
    // Aggregation �Լ��� PROWID ��� �Ұ�
    // BUG-41013 sum(_prowid) + 1 �� ��쿡 ����� ���� ���ϰ� �ִ�.
    // �˻��ϴ� ��ġ�� ������
    if ((aNode->node.lflag & MTC_NODE_OPERATOR_MASK)
        == MTC_NODE_OPERATOR_AGGREGATION)
    {
        if ((aNode->lflag & QTC_NODE_COLUMN_RID_MASK)
            == QTC_NODE_COLUMN_RID_EXIST)
        {
            IDE_RAISE(ERR_PROWID_NOT_SUPPORTED);
        }
    }

    //------------------------------------------------------
    // BUG-16000
    // Column�̳� Function�� Type�� Lob or Binary Type�̸� flag����
    //------------------------------------------------------

    aNode->lflag &= ~QTC_NODE_BINARY_MASK;

    if ( isEquiValidType( aNode, aTemplate ) == ID_FALSE )
    {
        aNode->lflag |= QTC_NODE_BINARY_EXIST;
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------------------
    // PROJ-1404
    // variable built-in function�� ����� ��� �����Ѵ�.
    //------------------------------------------------------

    if ( ( aNode->node.lflag & MTC_NODE_VARIABLE_MASK )
         == MTC_NODE_VARIABLE_TRUE )
    {
        aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
        aNode->lflag |= QTC_NODE_VAR_FUNCTION_EXIST;
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------
    // PROJ-1653 Outer Join Operator (+)
    // terminal node �߿��� �÷���常�� (+)�� ����� �� �ִ�.
    //-----------------------------------------
    if ( ( ( aNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
           == QTC_NODE_JOIN_OPERATOR_EXIST )
         &&
         ( ( aNode->lflag & QTC_NODE_PRIOR_MASK)  // BUG-34370 prior column�� skip
           == QTC_NODE_PRIOR_ABSENT )
         &&
         ( QTC_IS_TERMINAL(aNode) == ID_TRUE ) )
    {
        if ( QTC_TEMPLATE_IS_COLUMN((qcTemplate*)aTemplate,aNode) == ID_TRUE )
        {
            dependencyAndJoinOperSet( aNode->node.table, & aNode->depInfo );
        }
        else
        {
            sInfo = (qtcCallBackInfo*)aCallBack->info;                

            if ( sInfo != NULL )
            {
                sSqlInfo.setSourceInfo( sInfo->statement,
                                        & aNode->position );
                IDE_RAISE(ERR_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN);
            }
            else
            {
                IDE_RAISE(ERR_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN2);
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //-----------------------------------------
    // Ư���� �����ڿ� ó��
    //-----------------------------------------
    
    if( ((aNode->node.lflag & MTC_NODE_OPERATOR_MASK) ==
         MTC_NODE_OPERATOR_AGGREGATION) &&
        ((aNode->lflag & QTC_NODE_AGGR_ESTIMATE_MASK) ==
         QTC_NODE_AGGR_ESTIMATE_FALSE) &&
        (sIsAggrNode == ID_TRUE) )
    {
        //----------------------------------------------------
        // [ �ش� Node�� Aggregation �������� ��� ]
        //
        // �Ϲ� Aggregation�� Nested Aggregation�� �з��Ѵ�.
        // ����, ���� ��ø Aggregation�� Validation Error�� �ɷ�����.
        //----------------------------------------------------

        IDE_TEST( estimateAggregation( aNode, aCallBack )
                  != IDE_SUCCESS );

    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------------
    // [ �Ϲ� �������� ��� ]
    // Constant Expression�� ���� ��ó���� �õ��Ѵ�.
    //------------------------------------------------------

    sInfo = (qtcCallBackInfo*)aCallBack->info;
    if ( sInfo->statement != NULL )
    {
        // �������� Validation ������ ���
        IDE_TEST( preProcessConstExpr( sInfo->statement,
                                       aNode,
                                       aTemplate,
                                       aStack,
                                       aRemain,
                                       aCallBack )
                  != IDE_SUCCESS );

        if( ( aNode->node.lflag & MTC_NODE_REESTIMATE_MASK )
            == MTC_NODE_REESTIMATE_TRUE )
        {
            if ( aNode->node.module->estimate( (mtcNode*)aNode,
                                               aTemplate,
                                               aStack,
                                               aRemain,
                                               aCallBack )
                 != IDE_SUCCESS )
            {
                sSqlInfo.setSourceInfo( sInfo->statement,
                                        & aNode->position );
                IDE_RAISE( ERR_PASS );
            }
            else
            {
                // Nothing To Do
            }

            // PROJ-1413
            // re-estimate�� ���������Ƿ� ���� estimate�� ����
            // re-estimate�� ���д�.
            aNode->node.lflag &= ~MTC_NODE_REESTIMATE_MASK;
            aNode->node.lflag |= MTC_NODE_REESTIMATE_FALSE;
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_PASS );
    {
        // sqlSourceInfo�� ���� error���.
        if ( ideHasErrorPosition() == ID_FALSE )
        {
            sSqlCode = ideGetErrorCode();

            (void)sSqlInfo.initWithBeforeMessage(sInfo->memory);
            IDE_SET(
                ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                                sSqlInfo.getBeforeErrMessage(),
                                sSqlInfo.getErrMessage()));
            (void)sSqlInfo.fini();

            // overwrite wrapped errorcode to original error code.
            ideGetErrorMgr()->Stack.LastError = sSqlCode;
        }
        else
        {
            // Nothing to do.
        }
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN );
    {
        (void)sSqlInfo.init(sInfo->memory);

        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN,
                                 sSqlInfo.getErrMessage() ));
        (void)sSqlInfo.fini();
    }    
    IDE_EXCEPTION( ERR_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN2 );
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOW_JOIN_OPER_WITH_NON_COLUMN,
                                 "" ));
    }
    IDE_EXCEPTION( ERR_MISSING_OVER_IN_WINDOW_FUNCTION );
    {
        (void)sSqlInfo.init(sInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_MISSING_OVER_IN_WINDOW_FUNCTION,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION(ERR_PROWID_NOT_SUPPORTED)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_PROWID_NOT_SUPPORTED));
    }
    IDE_EXCEPTION( NAME_NOTATION_NOT_ALLOWED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_PARAM_NAME_NOTATION_NOW_ALLOWED ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

idBool
qtc::isEquiValidType( qtcNode     * aNode,
                      mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description :
 *     BUG-16000
 *     Equal������ ������ Ÿ������ �˻��Ѵ�.
 *     Column�̳� Function�� Type�� Lob or Binary Type�̸� ID_FALSE�� ��ȯ
 *     (BUG: MT function�� ����, PSM function�� Type�� �˻����� ����)
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qtc::isEquiValidType"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool              sReturn = ID_TRUE;
    mtcNode           * sNode;
    const mtdModule   * sModule;

    if ( (aNode->lflag & QTC_NODE_BINARY_MASK)
         == QTC_NODE_BINARY_EXIST )
    {
        sReturn = ID_FALSE;
    }
    else
    {
        switch ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        {
            case MTC_NODE_OPERATOR_SUBQUERY:
                for ( sNode = aNode->node.arguments;
                      sNode != NULL;
                      sNode = sNode->next )
                {
                    sReturn = isEquiValidType( (qtcNode*) sNode,
                                               aTemplate );

                    if ( sReturn == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                break;

            case MTC_NODE_OPERATOR_FUNCTION:
                sModule = aTemplate->rows[aNode->node.table].
                    columns[aNode->node.column].module;

                sReturn = mtf::isEquiValidType( sModule );
                break;

            case MTC_NODE_OPERATOR_MISC:
                if ( (aNode->node.module == & qtc::columnModule) ||
                     (aNode->node.module == & qtc::valueModule) )
                {
                    sModule = aTemplate->rows[aNode->node.table].
                        columns[aNode->node.column].module;

                    sReturn = mtf::isEquiValidType( sModule );
                }
                else
                {
                    // Nothing to do.
                }
                break;

            default:
                break;
        }
    }

    return sReturn;

#undef IDE_FN
}

IDE_RC
qtc::registerTupleVariable( qcStatement    * aStatement,
                            qcNamePosition * aPosition )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *               tuple variable�� ����Ѵ�.
 *
 * Implementation :
 *     $$�� �����ϴ� tuple variable�� ����Ѵ�.
 *
 **********************************************************************/
#define IDE_FN "qtc::registerTupleVariable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcTupleVarList    * sTupleVariable;

    IDE_DASSERT( QC_IS_NULL_NAME( (*aPosition) ) == ID_FALSE );

    if ( ( aPosition->size > QC_TUPLE_VAR_HEADER_SIZE ) &&
         ( idlOS::strMatch( aPosition->stmtText + aPosition->offset,
                            QC_TUPLE_VAR_HEADER_SIZE,
                            QC_TUPLE_VAR_HEADER,
                            QC_TUPLE_VAR_HEADER_SIZE ) == 0 ) )
    {
        IDU_LIMITPOINT("qtc::registerTupleVariable::malloc");
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qcTupleVarList, & sTupleVariable )
                  != IDE_SUCCESS );

        SET_POSITION( sTupleVariable->name, (*aPosition) );
        sTupleVariable->next = QC_SHARED_TMPLATE(aStatement)->tupleVarList;

        QC_SHARED_TMPLATE(aStatement)->tupleVarList = sTupleVariable;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qtc::estimateAggregation( qtcNode     * aNode,
                          mtcCallBack * aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Aggregation Expression�� ���� estimate ��
 *    �����Ѵ�.
 *
 * Implementation :
 *     [�ش� Node�� Aggregation �������� ���]
 *     - Aggregation ����
 *       (1) �Ϲ� Aggregation
 *       (2) Nested Aggregation
 *       (3) Analytic Function 
 *
 ***********************************************************************/

#define IDE_FN "estimateAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode             * sNode;
    qtcCallBackInfo     * sInfo;
    qmsSFWGH            * sSFWGH;
    qmsAggNode          * sAggNode;
    qmsAnalyticFunc     * sAnalyticFunc;
    qcDepInfo             sResDependencies;
    qcuSqlSourceInfo      sqlInfo;
    idBool                sOuterHavingCase = ID_FALSE;

    // ���� Node�� Nested Aggregation�̸� ���� ��ø�̸�
    // �̴� Validation Error
    IDE_TEST_RAISE( ( aNode->lflag & QTC_NODE_AGGREGATE2_MASK )
                    == QTC_NODE_AGGREGATE2_EXIST,
                    ERR_INVALID_AGGREGATION );

    // BUG-16000
    // Aggregation ������ Lob or Binary Type�� ���ڷ� ���� �� ����.
    // ��, distinct�� ���� count������ ����
    if ( ( (aNode->node.lflag & MTC_NODE_DISTINCT_MASK) ==
           MTC_NODE_DISTINCT_FALSE ) &&
         ( ( aNode->node.module == & mtfCount ) ||
           ( aNode->node.module == & mtfCountKeep ) ) )
    {
        /* PROJ-2528 KeepAggregaion
         * Count�� lob�� ����ϴ°Ͱ� ����
         * CountKeep�� ù ��° Argument�� lob�� ��������� �� ��°
         * �� keep�� order by�� ���� lob�� ����� �� ����.
         * lob�� �񱳴���� �ƴϱ� �����̴�.
         */
        if ( aNode->node.module == &mtfCountKeep )
        {
            /* BUG-49029 */
            if ( aNode->node.arguments != NULL )
            {
                for ( sNode  = (qtcNode*)aNode->node.arguments->next;
                      sNode != NULL;
                      sNode  = (qtcNode*)sNode->node.next )
                {
                    IDE_TEST_RAISE( (sNode->lflag & QTC_NODE_BINARY_MASK) ==
                                    QTC_NODE_BINARY_EXIST,
                                    ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT );
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        for( sNode  = (qtcNode*)aNode->node.arguments;
             sNode != NULL;
             sNode  = (qtcNode*)sNode->node.next )
        {
            IDE_TEST_RAISE(
                (sNode->lflag & QTC_NODE_BINARY_MASK) ==
                QTC_NODE_BINARY_EXIST,
                ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT );
        }
    }

    sInfo = (qtcCallBackInfo*)aCallBack->info;
    sSFWGH = sInfo->SFWGH;

    //---------------------------------------------------
    // Argument�� dependencies�� �̿��� �ش� SFWGH�� ã�´�.
    //---------------------------------------------------

    while (sSFWGH != NULL)
    {
        // check dependency
        qtc::dependencyClear( & sResDependencies );
        if( aNode->node.arguments != NULL )
        {
            qtc::dependencyAnd(
                & ((qtcNode *)(aNode->node.arguments))->depInfo,
                & sSFWGH->depInfo,
                & sResDependencies );
        }
        if ( qtc::dependencyEqual( & sResDependencies,
                                   & qtc::zeroDependencies ) == ID_TRUE )
        {
            sSFWGH = sSFWGH->outerQuery;
            continue;
        }
        else
        {
            break;
        }
    }

    if (sSFWGH == NULL)
    {
        // ������ ���� ��찡 �̿� �ش��Ѵ�.
        // SUM( 1 ), COUNT(*)
        // ��, �������� ���� �� �� �ִ�.
        sSFWGH = sInfo->SFWGH;
    }

    if (sSFWGH == NULL)
    {
        // order by�� Į���� ���� estimate �� ���
        if ( aNode->overClause == NULL )
        {
            sqlInfo.setSourceInfo( sInfo->statement,
                                   & aNode->position );
            IDE_RAISE(ERR_NOT_ALLOWED_AGGREGATION);
        }
        else
        {
            // BUG-21807
            // analytic function�� aggregate function�� �ƴϸ�,
            // order by ������ �� �� ����
        }
    }
    else
    {
        // outer HAVING, local (TARGET/WHERE)
        if( sSFWGH != sInfo->SFWGH )
        {
            if( sSFWGH->validatePhase == QMS_VALIDATE_HAVING )
            {
                sOuterHavingCase = ID_TRUE;
            }
        }
    }

    //---------------------------------------------------
    // TODO - ���� ������ ���� �ڵ� �ݿ��ؾ� ��.
    // PR-6353�� ������ BUG�� ������.
    //---------------------------------------------------

    if( sOuterHavingCase == ID_TRUE )
    {
        //--------------------------
        // special case for OUTER/HAVING
        //--------------------------
        IDU_LIMITPOINT("qtc::estimateAggregation::malloc1");
        IDE_TEST(STRUCT_ALLOC(sInfo->memory, qmsAggNode, (void**)&sAggNode)
                 != IDE_SUCCESS);

        sAggNode->aggr = aNode;
        
        if( ( aNode->lflag & QTC_NODE_AGGREGATE_MASK )
            == QTC_NODE_AGGREGATE_EXIST )
        {
            sNode = (qtcNode*)aNode->node.arguments;

            if ( sNode != NULL )
            {
                if ( ( sNode->lflag & QTC_NODE_ANAL_FUNC_MASK )
                     == QTC_NODE_ANAL_FUNC_ABSENT )
                {
                    aNode->lflag |= QTC_NODE_AGGREGATE2_EXIST;

                    sAggNode->next           = sInfo->SFWGH->aggsDepth1;
                    sInfo->SFWGH->aggsDepth1 = sAggNode;
                }
                else
                {
                    // BUG-21808
                    // aggregation�� argument�� analytic function��
                    // �� �� ����
                    // ( aggregation ���� �Ŀ� analytic function��
                    // ����Ǳ� ����)
                    sqlInfo.setSourceInfo( sInfo->statement,
                            &sNode->position );
                    IDE_RAISE( ERR_INVALID_WINDOW_FUNCTION );
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            aNode->lflag |= QTC_NODE_AGGREGATE_EXIST;

            sAggNode->next     = sSFWGH->aggsDepth1;
            sSFWGH->aggsDepth1 = sAggNode;
            sInfo->SFWGH->outerHavingCase = ID_TRUE;
        }
    }
    else
    {
        if ( aNode->overClause != NULL )
        {
            //--------------------------
            // Analytic Function�� ��� ( PROJ-1762 )
            //--------------------------
            aNode->lflag |= QTC_NODE_AGGREGATE_EXIST;
            
            IDU_LIMITPOINT("qtc::estimateAggregation::malloc2");
            IDE_TEST(STRUCT_ALLOC(sInfo->memory,
                                  qmsAnalyticFunc,
                                  (void**)&sAnalyticFunc)
                     != IDE_SUCCESS);
            
            sAnalyticFunc->analyticFuncNode = aNode;
            sAnalyticFunc->next = sInfo->querySet->analyticFuncList;
            sInfo->querySet->analyticFuncList = sAnalyticFunc;
        }
        else
        {
            //--------------------------
            // normal case
            //--------------------------
            
            IDU_LIMITPOINT("qtc::estimateAggregation::malloc3");
            IDE_TEST(STRUCT_ALLOC(sInfo->memory, qmsAggNode, (void**)&sAggNode)
                     != IDE_SUCCESS);
            
            sAggNode->aggr = aNode;
            
            if( (aNode->lflag & QTC_NODE_AGGREGATE_MASK )
                == QTC_NODE_AGGREGATE_EXIST )
            {
                sNode = (qtcNode*)aNode->node.arguments;
                
                if ( sNode != NULL )
                {
                    if ( ( sNode->lflag & QTC_NODE_ANAL_FUNC_MASK )
                         == QTC_NODE_ANAL_FUNC_ABSENT )
                    {
                        // ���� Node�� Aggregation�� �����ϰ�,
                        // analytic function�� aggregation�� �ƴ� ���,
                        // Nested Aggregation���� ������.
                        aNode->lflag |= QTC_NODE_AGGREGATE2_EXIST;
                    
                        // set all child aggregation with the indexArgument value of 1.
                        IDE_TEST( setSubAggregation(
                                      (qtcNode*)(sAggNode->aggr->node.arguments) )
                                  != IDE_SUCCESS );
                    
                        sAggNode->next           = sInfo->SFWGH->aggsDepth2;
                        sInfo->SFWGH->aggsDepth2 = sAggNode;
                    }
                    else
                    {
                        // BUG-21808
                        // aggregation�� argument�� analytic function��
                        // �� �� ����
                        sqlInfo.setSourceInfo( sInfo->statement,
                                               & sNode->position );
                        IDE_RAISE( ERR_INVALID_WINDOW_FUNCTION );
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                //------------------------------------------------------
                // ���� Node�� Aggregation�� ���� ���,
                // �Ϲ� Aggregation���� ������.
                // TODO -
                // ������ ���� GROUP BY Column�� ���� Aggregation�� ���,
                // Nested Aggregation���� �����Ͽ��� �Ѵ�.
                //
                // Ex)  SELECT SUM(I1), MAX(SUM(I2)) FROM T1 GROUP BY I1;
                //             ^^^^^^^
                //------------------------------------------------------
                aNode->lflag |= QTC_NODE_AGGREGATE_EXIST;
                
                if( sInfo->SFWGH->validatePhase == QMS_VALIDATE_HAVING )
                {
                    aNode->indexArgument = 1;
                }
                
                sAggNode->next           = sInfo->SFWGH->aggsDepth1;
                sInfo->SFWGH->aggsDepth1 = sAggNode;
            }
        }
    }

    /* BUG-35674 */
    aNode->lflag &= ~QTC_NODE_AGGR_ESTIMATE_MASK;
    aNode->lflag |=  QTC_NODE_AGGR_ESTIMATE_TRUE;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_WINDOW_FUNCTION );
    {
        (void)sqlInfo.init(sInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_WINDOW_FUNCTION,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_AGGREGATION );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_AGGREGATION));
    }
    IDE_EXCEPTION( ERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_USE_INCOMPARABLE_DATA_TYPE_WITH_DISTINCT));
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_AGGREGATION)
    {
        (void)sqlInfo.init(sInfo->memory);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QCV_NOT_ALLOWED_AGGREGATION,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::estimate4OverClause( qtcNode     * aNode,
                          mtcTemplate * aTemplate,
                          mtcStack    * aStack,
                          SInt          aRemain,
                          mtcCallBack * aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Analytic Function Expression�� ���� estimate ��
 *    �����Ѵ�.
 *
 * Implementation :
 *    (1) Aggregation ���� ��, sum ���� �� ����
 *    (2) Partition By Column�� analytic function�� �� �� ����
 *
 ***********************************************************************/

#define IDE_FN "estimate4OverClause"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcCallBackInfo  * sCallBackInfo;
    qcuSqlSourceInfo   sSqlInfo;
    qtcNode          * sCurArgument;
    qtcOverColumn    * sCurOverColumn;
    mtcStack         * sStack;
    SInt               sRemain;

    sCallBackInfo = (qtcCallBackInfo*)(aCallBack->info);

    // BUG-27457
    IDE_TEST_RAISE( sCallBackInfo->querySet == NULL,
                    ERR_INVALID_CALLBACK );
    
    // Aggregation�� ����
    if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         != MTC_NODE_OPERATOR_AGGREGATION )
    {
        sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                & aNode->position );
        
        IDE_RAISE( ERR_NOT_SUPPORTED_FUNCTION );
    }
    else
    {
        // nothing to do 
    }

    // BUG-33663 Ranking Function
    // ranking function�� order by expression�� �ݵ�� �־�� ��
    if ( ( ( aNode->node.lflag & MTC_NODE_FUNCTION_RANKING_MASK )
           == MTC_NODE_FUNCTION_RANKING_TRUE )
         &&
         ( aNode->overClause->orderByColumn == NULL ) )
    {
        sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                & aNode->position );
        
        IDE_RAISE( ERR_MISSING_ORDER_IN_WINDOW_FUNCTION );
    }
    else
    {
        // nothing to do
    }

    /* BUG-43087 support ratio_to_report
     */
    if ( aNode->overClause->orderByColumn != NULL )
    {
        if ( ( aNode->node.module == &mtfRatioToReport ) ||
             ( aNode->node.module == &mtfMinKeep ) ||
             ( aNode->node.module == &mtfMaxKeep ) ||
             ( aNode->node.module == &mtfSumKeep ) ||
             ( aNode->node.module == &mtfAvgKeep ) ||
             ( aNode->node.module == &mtfCountKeep ) ||
             ( aNode->node.module == &mtfVarianceKeep ) ||
             ( aNode->node.module == &mtfStddevKeep ) )
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                    &aNode->overClause->orderByColumn->node->position );
            IDE_RAISE( ERR_CANNOT_USE_ORDER_BY_CALUSE );
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

    // Analytic Function�� Target�� Order By ������ �� �� ����
    if ( ( sCallBackInfo->querySet->processPhase != QMS_VALIDATE_TARGET ) &&
         ( sCallBackInfo->querySet->processPhase != QMS_VALIDATE_ORDERBY ) )
    {
        sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                & aNode->position );
        
        IDE_RAISE( ERR_INVALID_WINDOW_FUNCTION );
    }
    else
    {
        // nothing to do 
    }

    // Argument Column�鿡 analytic function�� �ü� ����
    for ( sCurArgument = (qtcNode*)aNode->node.arguments;
          sCurArgument != NULL;
          sCurArgument = (qtcNode*)sCurArgument->node.next )
    {
        if ( ( sCurArgument->lflag & QTC_NODE_ANAL_FUNC_MASK )
             == QTC_NODE_ANAL_FUNC_EXIST )
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                    & sCurArgument->position );
            IDE_RAISE( ERR_INVALID_WINDOW_FUNCTION );
        }
        else
        {
            // Nothing to do.
        }
    }

    // Partition By column �鿡 ���� estimate
    for ( sCurOverColumn = aNode->overClause->overColumn, 
            sStack = aStack + 1, sRemain = aRemain - 1;
          sCurOverColumn != NULL;
          sCurOverColumn = sCurOverColumn->next )
    {
        sCurOverColumn->node->lflag &= ~QTC_NODE_ANAL_FUNC_COLUMN_MASK;
        sCurOverColumn->node->lflag |= QTC_NODE_ANAL_FUNC_COLUMN_TRUE;

        /* BUG-39678
           over���� ����� psm ������ bind ������ ġȯ */
        sCurOverColumn->node->lflag &= ~QTC_NODE_PROC_VAR_MASK;
        sCurOverColumn->node->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;

        IDE_TEST( estimateInternal( sCurOverColumn->node,
                                    aTemplate,
                                    sStack,
                                    sRemain,
                                    aCallBack )
                  != IDE_SUCCESS );

        /* TASK-7219 Shard Transformer Refactoring */
        aNode->lflag |= sCurOverColumn->node->lflag & QTC_NODE_MASK;

        // BUG-32358 ����Ʈ Ÿ���� ��� ���� Ȯ��
        if ( (sCurOverColumn->node->node.lflag & MTC_NODE_OPERATOR_MASK) ==
             MTC_NODE_OPERATOR_LIST )
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                   &sCurOverColumn->node->position );
            IDE_RAISE(ERR_INVALID_WINDOW_FUNCTION);
        }
        else
        {
            // Nothing to do.
        }

        // ���������� ��� �Ǿ����� Ÿ�� �÷��� �ΰ��̻����� Ȯ��
        if ( ( sCurOverColumn->node->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_SUBQUERY )
        {
            if ( ( sCurOverColumn->node->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 1 )
            {
                sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                        & sCurOverColumn->node->position );
                IDE_RAISE(ERR_INVALID_WINDOW_FUNCTION);
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }

        // BUG-35670 over ���� lob, geometry type ��� �Ұ�
        if ( (sCurOverColumn->node->lflag & QTC_NODE_BINARY_MASK) ==
             QTC_NODE_BINARY_EXIST)
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                    &sCurOverColumn->node->position );
            IDE_RAISE(ERR_INVALID_WINDOW_FUNCTION);
        }
        else
        {
            // Nothing to do.
        }

        // Partition By Column�� analytic function�� �� �� ����
        if ( ( sCurOverColumn->node->lflag & QTC_NODE_ANAL_FUNC_MASK )
             == QTC_NODE_ANAL_FUNC_EXIST )
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                    & sCurOverColumn->node->position );
            IDE_RAISE( ERR_INVALID_WINDOW_FUNCTION );
        }
        else
        {
            // Nothing to do.
        }

        // partition by column�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( dependencyOr( & aNode->depInfo,
                                & sCurOverColumn->node->depInfo,
                                & aNode->depInfo )
                  != IDE_SUCCESS );
    }
 
    /* PROJ-1805 Windowing Clause Support
     * window Start Point and End Point estimate
     */
    if ( aNode->overClause->window != NULL )
    {
        if ( ( aNode->node.lflag & MTC_NODE_FUNCTION_WINDOWING_MASK )
             == MTC_NODE_FUNCTION_WINDOWING_FALSE )
        {
            sSqlInfo.setSourceInfo( sCallBackInfo->statement,
                                    &aNode->position );
            IDE_RAISE( ERR_INVALID_WINDOW_CLAUSE_FUNCTION );
        }
        else
        {
            /* Nothing to do */
        }
        if ( aNode->overClause->window->rowsOrRange == QTC_OVER_WINODW_ROWS )
        {
            if ( aNode->overClause->window->start != NULL )
            {
                if ( aNode->overClause->window->start->value != NULL )
                {
                    IDE_TEST_RAISE( aNode->overClause->window->start->value->type
                                    != QTC_OVER_WINDOW_VALUE_TYPE_NUMBER,
                                    ERR_WINDOW_MISMATCHED_VALUE_TYPE );
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
            if ( aNode->overClause->window->end != NULL )
            {
                if ( aNode->overClause->window->end->value != NULL )
                {
                    IDE_TEST_RAISE( aNode->overClause->window->end->value->type
                                    != QTC_OVER_WINDOW_VALUE_TYPE_NUMBER,
                                    ERR_WINDOW_MISMATCHED_VALUE_TYPE );
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
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORTED_FUNCTION );
    {
        (void)sSqlInfo.init(sCallBackInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_UNSUPPORTED_FUNCTION,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_WINDOW_FUNCTION );
    {
        (void)sSqlInfo.init(sCallBackInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_WINDOW_FUNCTION,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_MISSING_ORDER_IN_WINDOW_FUNCTION );
    {
        (void)sSqlInfo.init(sCallBackInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_MISSING_ORDER_IN_WINDOW_FUNCTION,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_CALLBACK )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::estimate4OverClause",
                                  "Invalid callback" ));
    }
    IDE_EXCEPTION( ERR_INVALID_WINDOW_CLAUSE_FUNCTION )
    {
        (void)sSqlInfo.init(sCallBackInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_WINDOW_CLAUSE_FUNC,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }

    IDE_EXCEPTION( ERR_WINDOW_MISMATCHED_VALUE_TYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_MISMATCHED_VALUE_TYPE ) )
    }
    IDE_EXCEPTION( ERR_CANNOT_USE_ORDER_BY_CALUSE );
    {
        (void)sSqlInfo.init(sCallBackInfo->memory);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_CANNOT_USE_ORDER_BY_CLAUSE,
                                sSqlInfo.getErrMessage()));
        (void)sSqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeConversionNode( qtcNode*         aNode,
                                qcStatement*     aStatement,
                                qcTemplate*      aTemplate,
                                const mtdModule* aModule )
{
/***********************************************************************
 *
 * Description :
 *    DML��� Destine Column�� ������ �µ��� Value�� Column��
 *    Conversion �Ѵ�.
 *    ������ ���� DML���� Conversion Node�� ����� ���Ͽ� ����Ѵ�.
 *        - INSERT INTO T1(double_1) VALUES ( 1 );
 *                                            ^^
 * Implementation :
 *    ������ Conversion Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeConversionNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcNode*        sNode;
    mtcNode*        sConversionNode;
    const mtvTable* sTable;
    ULong           sCost;
    mtcStack        sStack[2];

    qtcCallBackInfo sCallBackInfo = {
        aTemplate,               // Template
        QC_QMP_MEM(aStatement),  // Memory ������
        NULL,                // Statement
        NULL,                // Query Set
        NULL,                // SFWGH
        NULL                 // From ����
    };

    mtcCallBack sCallBack = {
        &sCallBackInfo,                 // CallBack ����
        MTC_ESTIMATE_ARGUMENTS_DISABLE, // Child�� ���� Estimation Disable
        qtc::alloc,                     // Memory �Ҵ� �Լ�
        initConversionNodeIntermediate  // Conversion Node ���� �Լ�
    };

    for( sNode = &aNode->node;
         ( sNode->lflag&MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
         sNode = sNode->arguments ) ;

    //---------------------------------------------------------
    // �ش� Node�� Column ���̿� Conversion�� �ʿ������� �˻��ϰ�,
    // �ʿ��� ���, Conversion Node�� �����Ѵ�.
    // INSERT INTO T1(double_1) VALUES (3);
    //                ^^^^^^^^          ^^
    //                |                 |
    //               Column ����       ���� Node
    //
    // PROJ-1492
    // ȣ��Ʈ ������ �����ϴ��� �� Ÿ���� �� �� �ִ� ���
    // Conversion Node�� �����Ѵ�.
    //---------------------------------------------------------

    sCost = 0;

    sStack[0].column = QTC_TMPL_COLUMN( aTemplate, (qtcNode*)sNode );

    if ( ( ( sStack[0].column->module->id == MTD_BLOB_LOCATOR_ID ) &&
           ( aModule->id == MTD_BLOB_ID ) )
         ||
         ( ( sStack[0].column->module->id == MTD_CLOB_LOCATOR_ID ) &&
           ( aModule->id == MTD_CLOB_ID ) ) )
    {
        /* DML���� lob_locator�� ����ó���ϹǷ�
         * lob_locator�� lob type���� conversion�� �ʿ䰡 ����.
         *
         * Nothing to do.
         */
    }
    else
    {
        IDE_TEST( mtv::tableByNo( &sTable,
                                  aModule->no,
                                  sStack[0].column->module->no )
                  != IDE_SUCCESS );

        sCost += sTable->cost;

        IDE_TEST_RAISE( sCost >= MTV_COST_INFINITE, ERR_CONVERT );

        //----------------------------------------------
        // Conversion ���� �Լ��� ȣ���Ѵ�.
        // ������ ������ CallBack ������ �̿��� Conversion�� �����Ѵ�.
        //----------------------------------------------

        IDE_TEST( mtf::makeConversionNode( &sConversionNode,
                                           sNode,
                                           & aTemplate->tmplate,
                                           sStack,
                                           &sCallBack,
                                           sTable )
                  != IDE_SUCCESS );

        //----------------------------------------------
        // Conversion�� �����Ѵ�.
        //----------------------------------------------

        if( sNode->conversion == NULL )
        {
            if( sConversionNode != NULL )
            {
                sNode->lflag &= ~(MTC_NODE_INDEX_MASK);
                sNode->lflag |= MTC_NODE_INDEX_UNUSABLE;
            }
            sNode->conversion = sConversionNode;
            sNode->cost += sCost;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

SInt qtc::getCountBitSet( qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * Description :
 *    �ش� dependencies���� ���Ե� dependency�� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "SInt qtc::getCountBitSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return aOperand1->depCount;

#undef IDE_FN
}


SInt qtc::getCountJoinOperator( qcDepInfo  * aOperand )
{
/***********************************************************************
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *    �ش� dependencies���� ���Ե� Outer join Operator �� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt   i;
    SInt   sJoinOperCount = 0;

    for ( i = 0 ; i < aOperand->depCount ; i++ )
    {
        if ( QMO_JOIN_OPER_EXISTS( aOperand->depJoinOper[i] ) == ID_TRUE )
        {
            sJoinOperCount++;
        }
        else
        {
            // Nothing to do.
        }
    }

    return sJoinOperCount;
}

void
qtc::dependencyAndJoinOperSet( UShort      aTupleID,
                               qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 *    Internal Tuple ID �� dependencies�� Setting
 *    Set If Outer Join Operator Exists
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    aOperand1->depCount = 1;
    aOperand1->depend[0] = aTupleID;

    aOperand1->depJoinOper[0] = QMO_JOIN_OPER_TRUE;
}


void qtc::dependencyJoinOperAnd( qcDepInfo * aOperand1,
                                 qcDepInfo * aOperand2,
                                 qcDepInfo * aResult )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 *    AND of (Dependencies & Join Oper)�� ����
 *    depend ������ outer join operator ������ ��� ���� �Ϳ� ���ؼ��� return.
 *    outer join operator ���� QMO_JOIN_OPER_TRUE �� QMO_JOIN_OPER_BOTH ��
 *    ���� ������ �����Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    qcDepInfo sResult;

    UInt sLeft;
    UInt sRight;

    sResult.depCount = 0;

    for ( sLeft = 0, sRight = 0;
          ( (sLeft < aOperand1->depCount) && (sRight < aOperand2->depCount) );
        )
    {
        if ( ( aOperand1->depend[sLeft] == aOperand2->depend[sRight] )
          && ( QMO_JOIN_OPER_EQUALS( aOperand1->depJoinOper[sLeft],
                                    aOperand2->depJoinOper[sRight] ) ) )
        {
            sResult.depend[sResult.depCount] = aOperand1->depend[sLeft];
            sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[sLeft];
            sResult.depCount++;

            sLeft++;
            sRight++;
        }
        else
        {
            if ( aOperand1->depend[sLeft] > aOperand2->depend[sRight] )
            {
                sRight++;
            }
            else
            {
                sLeft++;
            }
        }
    }

    qtc::dependencySetWithDep( aResult, & sResult );
}


idBool
qtc::dependencyJoinOperEqual( qcDepInfo * aOperand1,
                              qcDepInfo * aOperand2 )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 *    Dependencies & Outer Join Operator Status �� ��� ��Ȯ��
 *    ������ ���� �Ǵ�
 *
 * Implementation :
 *    ������ ���� outer join operator ���� ��Ȯ�� ���ƾ��Ѵٴ� ���̴�.
 *    QMO_JOIN_OPER_TRUE != QMO_JOIN_OPER_BOTH �̴�.
 *    ����, QMO_JOIN_OPER_TRUE �� QMO_JOIN_OPER_BOTH �� ���� ������ �����Ϸ���
 *    ������ �Լ��� ����ų� �� �Լ����� argument �� �߰��Ͽ� �����Ѵ�.
 *
 *
 ***********************************************************************/

    UInt   i;
    idBool sRet = ID_TRUE;

    if ( aOperand1->depCount != aOperand2->depCount )
    {
        sRet = ID_FALSE;
    }
    else
    {
        for ( i = 0; i < aOperand1->depCount; i++ )
        {
            if ( ( aOperand1->depend[i] != aOperand2->depend[i] )
              || ( aOperand1->depJoinOper[i] != aOperand2->depJoinOper[i] ) )
            {
                sRet = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do. 
            }
        }
    }

    return sRet;
}


void qtc::getJoinOperCounter( qcDepInfo  * aOperand1,
                              qcDepInfo  * aOperand2 )
{
/***********************************************************************
 *
 * Description :
 *
 *    dep ������ Outer join operator �� ������ �ݴ��� dep �� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    aOperand1->depCount = 2;

    aOperand1->depend[0] = aOperand2->depend[0];
    aOperand1->depend[1] = aOperand2->depend[1];
    aOperand1->depJoinOper[0] = aOperand2->depJoinOper[1]; // �ݴ�
    aOperand1->depJoinOper[1] = aOperand2->depJoinOper[0]; // �ݴ�
}


idBool
qtc::isOneTableOuterEachOther( qcDepInfo   * aDepInfo )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ϳ��� predicate ���� �ϳ��� dependency table �� ����
 *    outer join �Ǵ��� �˻�
 *    (t1.i1(+) = t1.i2)
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt     i;
    idBool   sRet;

    sRet = ID_FALSE;

    for ( i = 0 ;
          i < aDepInfo->depCount ;
          i++ )
    {
        if ( ( aDepInfo->depJoinOper[i] & QMO_JOIN_OPER_MASK )
                == QMO_JOIN_OPER_BOTH )
        {
            sRet = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    return sRet;
}


UInt
qtc::getDependTable( qcDepInfo  * aDepInfo,
                     UInt         aIdx )
{
    return aDepInfo->depend[aIdx];
}


UChar
qtc::getDependJoinOper( qcDepInfo  * aDepInfo,
                        UInt         aIdx )
{
    return aDepInfo->depJoinOper[aIdx];
}


SInt qtc::getPosFirstBitSet( qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependencies�� �����ϴ� ���� Dependency ���� �����Ѵ�.
 *
 * Implementation :
 *
 *    ���� Dependency ���� Return�ϰ�,
 *    Dependency�� �������� ���� ���,
 *    QTC_DEPENDENCIES_END(-1) �� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "SInt qtc::getPosFirstBitSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( aOperand1->depCount == 0 )
    {
        return QTC_DEPENDENCIES_END;
    }
    else
    {
        return aOperand1->depend[0];
    }

#undef IDE_FN
}

SInt qtc::getPosNextBitSet( qcDepInfo * aOperand1,
                            UInt   aPos )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependencies���� ���� ��ġ�� �����ϴ� Dependency ���� �����Ѵ�.
 *
 * Implementation :
 *
 *    ���� ��ġ(aPos)���κ��� ������ �����ϴ� Dependency ���� Return�ϰ�,
 *    Dependency�� �������� ���� ���,
 *    QTC_DEPENDENCIES_END(-1) �� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "SInt qtc::getPosNextBitSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt i;

    for ( i = 0; i < aOperand1->depCount; i++ )
    {
        if ( (UInt) aOperand1->depend[i] == aPos )
        {
            break;
        }
        else
        {
            // Go Go
        }
    }

    if ( (i + 1) < aOperand1->depCount )
    {
        return aOperand1->depend[i+1];
    }
    else
    {
        return QTC_DEPENDENCIES_END;
    }

#undef IDE_FN
}

IDE_RC qtc::alloc( void* aInfo,
                   UInt  aSize,
                   void** aMemPtr)
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Memory �����ڸ� �̿��Ͽ� Memory�� �Ҵ��Ѵ�.
 *
 * Implementation :
 *
 *    CallBack information���κ��� memory �����ڸ� ȹ���ϰ�,
 *    �̷κ��� Memory�� �Ҵ��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "void* qtc::alloc"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return ((qtcCallBackInfo*)aInfo)->memory->alloc( aSize, aMemPtr);

#undef IDE_FN
}

IDE_RC qtc::getOpenedCursor( mtcTemplate*     aTemplate,
                             UShort           aTableID,
                             smiTableCursor** aCursor,
                             UShort         * aOrgTableID,
                             idBool*          aFound )
{
/***********************************************************************
 *
 * Description :
 *
 *    tableID�� Ŀ�������� �����´�.
 *
 * Implementation :
 *
 *    qmcCursor�� ����� Ŀ�������� tableID�� ȹ���ϰ�,
 *    �̷κ��� lob-locator�� open�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "void* qtc::getOpenedCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcTemplate* sTemplate;

    sTemplate = (qcTemplate*) aTemplate;

    if( ( aTemplate->rows[aTableID].lflag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
        == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
    {
        *aOrgTableID = aTemplate->rows[aTableID].partitionTupleID;
    }
    else
    {
        *aOrgTableID = aTableID;
    }

    return ( sTemplate->cursorMgr->getOpenedCursor( *aOrgTableID, aCursor, aFound ) );

#undef IDE_FN
}

IDE_RC qtc::addOpenedLobCursor( mtcTemplate  * aTemplate,
                                smLobLocator   aLocator )
{
/***********************************************************************
 *
 * Description : BUG-40427
 *
 * Implementation :
 *
 *    qmcCursor�� open�� lob locator 1���� ����Ѵ�.
 *    ���ʷ� open�� lob locator 1���� �ʿ��ϹǷ�,
 *    �̹� ��ϵ� ��� qmcCursor������ �ƹ� �ϵ� ���� �ʴ´�.
 *
 ***********************************************************************/

#define IDE_FN "void* qtc::addOpenedLobCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcTemplate* sTemplate;

    sTemplate = (qcTemplate*) aTemplate;

    /* BUG-38290
     * addOpenedLobCursor �� lob locator �� ���� �� ����ǹǷ�
     * parallel query �� ����� �ƴϾ ���ü� ������ �߻����� �ʴ´�.
     */
    return ( sTemplate->cursorMgr->addOpenedLobCursor( aLocator ) );

#undef IDE_FN
}

idBool qtc::isBaseTable( mtcTemplate * aTemplate,
                         UShort        aTable )
{
    qmsFrom  * sFrom;
    idBool     sIsBaseTable = ID_FALSE;

    if ( aTable < aTemplate->rowCount )
    {
        sFrom = ((qcTemplate*)aTemplate)->tableMap[aTable].from;

        /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
        if ( sFrom != NULL )
        {
            // BUG-38943 view�� base table�� �ƴϴ�.
            if ( sFrom->tableRef->view == NULL )
            {
                sIsBaseTable = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return sIsBaseTable;
}

IDE_RC qtc::closeLobLocator( smLobLocator  aLocator )
{
/***********************************************************************
 *
 * Description :
 *
 *    locator�� �ݴ´�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "void* qtc::closeLobLocator"

    return qmx::closeLobLocator( NULL, /* idvSQL* */
                                 aLocator );

#undef IDE_FN
}

IDE_RC qtc::nextRow( iduVarMemList * aMemory,
                     qcStatement   * aStatement,
                     qcTemplate    * aTemplate,
                     ULong           aFlag )
{
/***********************************************************************
 *
 * Description :
 *
 *    Tuple Set���� �־��� Tuple ������ ���� Tuple�� �Ҵ�޴´�.
 *
 * Implementation :
 *
 *    aFlag ���ڸ� �̿��Ͽ� ������ ���� Tuple ������ �Ǵ��ϰ�,
 *    �Ʒ��� ���� ������ Tuple�� ���� ���ο� Tuple�� �Ҵ�޴´�.
 *        - MTC_TUPLE_TYPE_CONSTANT
 *        - MTC_TUPLE_TYPE_VARIABLE
 *        - MTC_TUPLE_TYPE_INTERMEDIATE
 *    MTC_TUPLE_TYPE_TABLE�� ���, �ش� �Լ��� ȣ����� �ʴ´�.
 *
 ***********************************************************************/

    idBool    sFirst;
    UShort    sCurRowID = ID_USHORT_MAX;

    aFlag &= MTC_TUPLE_TYPE_MASK;

    IDE_TEST_RAISE( aTemplate->tmplate.rowCount >= MTC_TUPLE_ROW_MAX_CNT,
                    ERR_TUPLE_SHORTAGE );

    // PROJ-1358 Tuple Set�� �ڵ����� Ȯ���Ѵ�.
    if ( aTemplate->tmplate.rowCount >= aTemplate->tmplate.rowArrayCount )
    {
        // �Ҵ�� �������� �� ū ������ �ʿ��� ���
        // ���� rowArrayCount ��ŭ �� Ȯ���Ѵ�. (2��� Ȯ��)
        IDE_TEST( increaseInternalTuple( aStatement,
                                         aTemplate->tmplate.rowArrayCount )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // ���ʷ� �ش� Tuple�� �Ҵ�Ǵ� ���� �Ǵ��ϰ�,
    // �ش� Tuple������ ���� ����ϰ� �ִ� ��ġ�� �����Ѵ�.

    sFirst = aTemplate->tmplate.currentRow[aFlag] == ID_USHORT_MAX? ID_TRUE : ID_FALSE ;

    aTemplate->tmplate.currentRow[aFlag] = aTemplate->tmplate.rowCount;

    sCurRowID = aTemplate->tmplate.currentRow[aFlag];

    // �ش� Tuple�� ������ �ʱ�ȭ�Ѵ�.
    /* BUGBUG: columnMaximum�� ����ġ�� ũ�� å���Ǿ� �ֽ��ϴ�. */
    aTemplate->tmplate.rows[sCurRowID].lflag
        = templateRowFlags[aFlag];
    aTemplate->tmplate.rows[sCurRowID].columnCount   = 0;
    // PROJ-1362  SMI_COLUMN_ID_MAXIMUM�� 256���� �پ,
    // 256�� �̻��� �÷��� ���� View������ �����Ҽ� �־
    // SMI_COLUMN_ID_MAXIMUM ��� 1024���� ���� ������ define�� ���.
    aTemplate->tmplate.rows[sCurRowID].columnMaximum = MTC_TUPLE_COLUMN_ID_MAXIMUM;

    // PROJ-1502 PARTITIONED DISK TABLE
    aTemplate->tmplate.rows[sCurRowID].partitionTupleID = sCurRowID;

    // memset for BUG-4953
    switch( aFlag )
    {
        //-------------------------------------------------
        // CONSTANT TUPLE�� ���� Tuple �ʱ�ȭ
        //-------------------------------------------------
        case MTC_TUPLE_TYPE_CONSTANT:
            {
                aTemplate->tmplate.rows[sCurRowID].rowOffset  = 0;
                if( sFirst == ID_TRUE )
                {
                    // ���� CONSTANT ó���ÿ��� 4096 Bytes ������ �Ҵ���.
                    aTemplate->tmplate.rows[sCurRowID].rowMaximum
                        = QC_CONSTANT_FIRST_ROW_SIZE;
                }
                else
                {
                    // ������ CONSTANT ó���ÿ��� 65536 Bytes ������ �Ҵ���.
                    aTemplate->tmplate.rows[sCurRowID].rowMaximum
                        = QC_CONSTANT_ROW_SIZE;
                }

                // Column ������ �Ҵ�
                // To fix PR-14793 column �޸𸮴� �ʱ�ȭ �Ǿ�� ��.
                IDU_LIMITPOINT("qtc::nextRow::malloc1");
                IDE_TEST( aMemory->cralloc(
                              idlOS::align8((UInt)ID_SIZEOF(mtcColumn))
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**)&(aTemplate->tmplate.rows[sCurRowID].columns) )
                          != IDE_SUCCESS);

                // PROJ-1473
                IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                              aMemory,
                              &(aTemplate->tmplate),
                              sCurRowID )
                          != IDE_SUCCESS );

                // A3������ Execute�� ���� ���� �Ҵ��� Parsing�Ŀ� �̷����
                //     qci2::fixAfterParsing()
                // �׷���, A4������ Constant Expression�� ó���� ����
                // �̸� �Ҵ����
                IDU_LIMITPOINT("qtc::nextRow::malloc2");
                IDE_TEST( aMemory->alloc(
                              ID_SIZEOF(mtcExecute)
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**)&(aTemplate->tmplate.rows[sCurRowID].execute) )
                          != IDE_SUCCESS);

                // Row ������ �Ҵ�
                IDU_LIMITPOINT("qtc::nextRow::malloc3");
                IDE_TEST(aMemory->alloc(
                             aTemplate->tmplate.rows[sCurRowID].rowMaximum,
                             (void**)&(aTemplate->tmplate.rows[sCurRowID].row) )
                         != IDE_SUCCESS);

                break;
            }
            //-------------------------------------------------
            // VARIABLE TUPLE�� ���� Tuple �ʱ�ȭ
            //-------------------------------------------------
        case MTC_TUPLE_TYPE_VARIABLE:
            {
                if( sFirst == ID_TRUE  && aTemplate->tmplate.variableRow == ID_USHORT_MAX )
                {
                    // ���� Binding�� �ʿ��� Tuple�� ��ġ�� ������.
                    aTemplate->tmplate.variableRow = aTemplate->tmplate.rowCount;
                }
                aTemplate->tmplate.rows[sCurRowID].rowOffset  = 0;
                aTemplate->tmplate.rows[sCurRowID].rowMaximum = 0;

                // Column ������ �Ҵ�
                // To fix PR-14793 column �޸𸮴� �ʱ�ȭ �Ǿ�� ��.
                IDU_LIMITPOINT("qtc::nextRow::malloc4");
                IDE_TEST( aMemory->cralloc(
                              idlOS::align8((UInt)ID_SIZEOF(mtcColumn))
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**)&(aTemplate->tmplate.rows[sCurRowID].columns) )
                          != IDE_SUCCESS);

                // PROJ-1473
                IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                              aMemory,
                              &(aTemplate->tmplate),
                              sCurRowID )
                    != IDE_SUCCESS );

                // Execute ������ �Ҵ�
                IDU_LIMITPOINT("qtc::nextRow::malloc5");
                IDE_TEST( aMemory->alloc(
                              ID_SIZEOF(mtcExecute)
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**)&(aTemplate->tmplate.rows[sCurRowID].execute) )
                          != IDE_SUCCESS);

                // Row������ Binding�Ŀ� ó���ȴ�.

                break;
            }
            //-------------------------------------------------
            // INTERMEDIATE TUPLE�� ���� Tuple �ʱ�ȭ
            //-------------------------------------------------
        case MTC_TUPLE_TYPE_INTERMEDIATE:
            {
                aTemplate->tmplate.rows[sCurRowID].rowOffset  = 0;
                aTemplate->tmplate.rows[sCurRowID].rowMaximum = 0;

                // Column ������ �Ҵ�
                // To fix PR-14793 column �޸𸮴� �ʱ�ȭ �Ǿ�� ��.
                IDU_LIMITPOINT("qtc::nextRow::malloc6");
                IDE_TEST( aMemory->cralloc(
                              idlOS::align8((UInt)ID_SIZEOF(mtcColumn))
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**)&(aTemplate->tmplate.rows[sCurRowID].columns) )
                          != IDE_SUCCESS);

                // PROJ-1473
                IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                              aMemory,
                              &(aTemplate->tmplate),
                              sCurRowID )
                          != IDE_SUCCESS );

                // Execute ������ �Ҵ�
                IDU_LIMITPOINT("qtc::nextRow::malloc7");
                IDE_TEST( aMemory->alloc(
                              ID_SIZEOF(mtcExecute)
                              * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                              (void**) & (aTemplate->tmplate.rows[sCurRowID].execute))
                          != IDE_SUCCESS);

                // Row ������ Validation�Ŀ� ó����
                // qtc::fixAfterValidation()

                break;
            }
        default:
            {
                IDE_RAISE( ERR_NOT_APPLICABLE );
                break;
            }
    } // end of switch

    aTemplate->tmplate.rows[sCurRowID].ridExecute = &gQtcRidExecute;
    aTemplate->tmplate.rowCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TUPLE_SHORTAGE );
    IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TUPLE_SHORTAGE));

    IDE_EXCEPTION( ERR_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::nextLargeConstColumn( iduVarMemList * aMemory,
                                  qtcNode       * aNode,
                                  qcStatement   * aStatement,
                                  qcTemplate    * aTemplate,
                                  UInt            aSize )
{
#define IDE_FN "IDE_RC qtc::nextLargeConstColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort sCurRowID = ID_USHORT_MAX;

    ULong sFlag = MTC_TUPLE_TYPE_CONSTANT;

    IDE_TEST_RAISE( aTemplate->tmplate.rowCount >= MTC_TUPLE_ROW_MAX_CNT,
                    ERR_TUPLE_SHORTAGE );

    // PROJ-1358 Tuple Set�� �ڵ����� Ȯ���Ѵ�.
    if ( aTemplate->tmplate.rowCount >= aTemplate->tmplate.rowArrayCount )
    {
        // �Ҵ�� �������� �� ū ������ �ʿ��� ���
        // ���� rowArrayCount ��ŭ �� Ȯ���Ѵ�. (2��� Ȯ��)
        IDE_TEST( increaseInternalTuple( aStatement,
                                         aTemplate->tmplate.rowArrayCount )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sCurRowID = aTemplate->tmplate.rowCount;

    // �ش� Tuple�� ������ �ʱ�ȭ�Ѵ�.
    aTemplate->tmplate.rows[sCurRowID].lflag = templateRowFlags[sFlag];
    aTemplate->tmplate.rows[sCurRowID].columnCount   = 0;
    aTemplate->tmplate.rows[sCurRowID].columnMaximum = 1;

    // PROJ-1502 PARTITIONED DISK TABLE
    aTemplate->tmplate.rows[sCurRowID].partitionTupleID = sCurRowID;

    //-------------------------------------------------
    // CONSTANT TUPLE�� ���� Tuple �ʱ�ȭ
    //-------------------------------------------------
    aTemplate->tmplate.rows[sCurRowID].rowOffset  = 0;

    aTemplate->tmplate.rows[sCurRowID].rowMaximum = aSize;

    // Column ������ �Ҵ�
    // To fix PR-14793 column �޸𸮴� �ʱ�ȭ �Ǿ�� ��.
    IDU_LIMITPOINT("qtc::nextLargeConstColumn::malloc1");
    IDE_TEST( aMemory->cralloc(
                  idlOS::align8((UInt)ID_SIZEOF(mtcColumn))
                  * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                  (void**)&(aTemplate->tmplate.rows[sCurRowID].columns) )
              != IDE_SUCCESS);

    // PROJ-1473
    IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                  aMemory,
                  &(aTemplate->tmplate),
                  sCurRowID )
              != IDE_SUCCESS );
    
    // A3������ Execute�� ���� ���� �Ҵ��� Parsing�Ŀ� �̷����
    //     qci2::fixAfterParsing()
    // �׷���, A4������ Constant Expression�� ó���� ����
    // �̸� �Ҵ����
    IDU_LIMITPOINT("qtc::nextLargeConstColumn::malloc2");
    IDE_TEST( aMemory->cralloc(
                  ID_SIZEOF(mtcExecute)
                  * aTemplate->tmplate.rows[sCurRowID].columnMaximum,
                  (void**)&(aTemplate->tmplate.rows[sCurRowID].execute) )
              != IDE_SUCCESS);

    // Row ������ �Ҵ�
    IDU_LIMITPOINT("qtc::nextLargeConstColumn::malloc3");
    IDE_TEST(aMemory->cralloc(
                 aTemplate->tmplate.rows[sCurRowID].rowMaximum,
                 (void**)&(aTemplate->tmplate.rows[sCurRowID].row) )
             != IDE_SUCCESS);

    // ���ο� Column ������ �Ҵ�޴´�.
    aNode->node.table  = sCurRowID;
    aNode->node.column = aTemplate->tmplate.rows[sCurRowID].columnCount;

    aTemplate->tmplate.rows[sCurRowID].columnCount += 1;

    aTemplate->tmplate.rows[aNode->node.table].lflag &=
        ~(MTC_TUPLE_ROW_SKIP_MASK);
    aTemplate->tmplate.rows[aNode->node.table].lflag |=
        MTC_TUPLE_ROW_SKIP_FALSE;

    aTemplate->tmplate.rowCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TUPLE_SHORTAGE );
    IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TUPLE_SHORTAGE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::nextColumn( iduVarMemList * aMemory,
                        qtcNode       * aNode,
                        qcStatement   * aStatement,
                        qcTemplate    * aTemplate,
                        ULong           aFlag,
                        UInt            aColumns)
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Tuple���� ���� Column�� �Ҵ�޴´�.
 *
 * Implementation :
 *
 *    aFlag ���ڸ� �̿��Ͽ� ������ ���� Tuple ������ �Ǵ��ϰ�,
 *    �Ʒ��� ���� ������ Tuple�� ���� Column�� �Ҵ�޴´�.
 *        - MTC_TUPLE_TYPE_CONSTANT
 *        - MTC_TUPLE_TYPE_VARIABLE
 *        - MTC_TUPLE_TYPE_INTERMEDIATE
 *    MTC_TUPLE_TYPE_TABLE�� ���, �ش� �Լ��� ȣ����� �ʴ´�.
 *
 *    �ش� Node(aNode)�� Table ID�� Column ID�� �ο������μ�,
 *    Column ������ �Ҵ�޴´�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::nextColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort  sCurrRowID;

    aFlag &= MTC_TUPLE_TYPE_MASK;

    // ���� �ش� Tuple Row�� ���� ��� ���ο� Row�� �Ҵ�޴´�.
    if( aTemplate->tmplate.currentRow[aFlag] == ID_USHORT_MAX )
    {
        IDE_TEST( qtc::nextRow( aMemory, aStatement, aTemplate, aFlag )
                  != IDE_SUCCESS );
    }

    sCurrRowID = aTemplate->tmplate.currentRow[aFlag];

    // Column ������ ������ ���, ���ο� Row�� �Ҵ�޴´�.
    if( aTemplate->tmplate.rows[sCurrRowID].columnCount + aColumns
        > aTemplate->tmplate.rows[sCurrRowID].columnMaximum )
    {
        IDE_TEST_RAISE( (aFlag == MTC_TUPLE_TYPE_VARIABLE) &&
                        (aTemplate->tmplate.variableRow ==
                         aTemplate->tmplate.currentRow[aFlag]),
                        ERR_HOST_VAR_LIMIT );
          
        IDE_TEST( qtc::nextRow( aMemory, aStatement, aTemplate, aFlag )
                  != IDE_SUCCESS );

        sCurrRowID = aTemplate->tmplate.currentRow[aFlag];

        IDE_TEST_RAISE( aTemplate->tmplate.rows[sCurrRowID].columnCount
                        + aColumns >
                        aTemplate->tmplate.rows[sCurrRowID].columnMaximum,
                        ERR_TUPLE_SHORTAGE );
    }

    // ���ο� Column ������ �Ҵ�޴´�.
    aNode->node.table  = aTemplate->tmplate.currentRow[aFlag];
    aNode->node.column = aTemplate->tmplate.rows[sCurrRowID].columnCount;

    aTemplate->tmplate.rows[sCurrRowID].columnCount += aColumns;

    aTemplate->tmplate.rows[aNode->node.table].lflag &=
        ~(MTC_TUPLE_ROW_SKIP_MASK);
    aTemplate->tmplate.rows[aNode->node.table].lflag |=
        MTC_TUPLE_ROW_SKIP_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TUPLE_SHORTAGE );
    {    
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TUPLE_SHORTAGE));
    }
    IDE_EXCEPTION( ERR_HOST_VAR_LIMIT );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_HOST_VAR_LIMIT_EXCEED,
                                (SInt)aTemplate->tmplate.rows[sCurrRowID].columnMaximum));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qtc::nextTable( UShort          *aRow,
                       qcStatement     *aStatement,
                       qcmTableInfo    *aTableInfo,
                       idBool           aIsDiskTable,
                       UInt             aNullableFlag ) // PR-13597
{
/***********************************************************************
 *
 * Description :
 *
 *    Table�� ���� Tuple ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 *    �Ϲ� Table�� ���� ������ ���(aTableInfo != NULL),
 *        Meta Cache�� Column ������ �̿��Ͽ� Tuple�� Column ������ ����
 *        - Disk Table�� ���� ����� �Ǿ�� ��.
 *             : Column ������ �����ؾ� ��.
 *             : Tuple�� Disk/Memory������ ������ �����ؾ� ��.
 *    �Ϲ� Table�� �ƴ� ���(aTableInfo == NULL),
 *        Column ������ Execution ������ �����ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::nextTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort         sCurRowID = ID_USHORT_MAX;
    qcTemplate*    sTemplate = QC_SHARED_TMPLATE(aStatement);
    UShort         i;


    IDE_TEST_RAISE( sTemplate->tmplate.rowCount >= MTC_TUPLE_ROW_MAX_CNT,
                    ERR_TUPLE_SHORTAGE );

    // PROJ-1358 Tuple Set�� �ڵ����� Ȯ���Ѵ�.
    if ( sTemplate->tmplate.rowCount >= sTemplate->tmplate.rowArrayCount )
    {
        // �Ҵ�� �������� �� ū ������ �ʿ��� ���
        // ���� rowArrayCount ��ŭ �� Ȯ���Ѵ�. (2��� Ȯ��)
        IDE_TEST( increaseInternalTuple( aStatement,
                                         sTemplate->tmplate.rowArrayCount )
                  != IDE_SUCCESS );
    }

    // Tuple ID�� �Ҵ��Ѵ�.
    sTemplate->tmplate.currentRow[MTC_TUPLE_TYPE_TABLE]
        = sCurRowID
        = sTemplate->tmplate.rowCount;

    // PROJ-1502 PARTITIONED DISK TABLE
    sTemplate->tmplate.rows[sCurRowID].partitionTupleID = sCurRowID;

    // PROJ-2205 rownum in DML
    sTemplate->tmplate.rows[sCurRowID].cursorInfo = NULL;

    if( aTableInfo != NULL )
    {
        //--------------------------------------------------------
        // �Ϲ� Table�� ���,
        // Column ������ �����ϰ�, Execute�� ���� ������ �Ҵ��Ѵ�.
        //--------------------------------------------------------

        sTemplate->tmplate.rows[sCurRowID].lflag
            = templateRowFlags[MTC_TUPLE_TYPE_TABLE];
        sTemplate->tmplate.rows[sCurRowID].columnCount
            = sTemplate->tmplate.rows[sCurRowID].columnMaximum
            = aTableInfo->columnCount;

        //------------------------------------------------------------
        // Column ������ ����
        // Disk Variable Column�� ó���ϱ� ���ؼ��� Column ������ �����ؾ� ��.
        // ����) Memory Table�� ���, ������ ������ �ʿ䰡 ������
        //       Stored Procedure�� ���� Tuple Set ���縦 ����
        //       Tuple Set������ ����ġ�� ����ȭ�ϴ� ������ �߻��ϰ� �ȴ�.
        //       ( mtc::cloneTuple(), qtc::templateRowFlags )
        //------------------------------------------------------------

        IDU_LIMITPOINT("qtc::nextTable::malloc1");
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(mtcColumn)
                                                * sTemplate->tmplate.rows[sCurRowID].columnCount,
                                                (void**) & (sTemplate->tmplate.rows[sCurRowID].columns) )
                 != IDE_SUCCESS );

        for( i = 0; i < sTemplate->tmplate.rows[sCurRowID].columnCount; i++ )
        {
            idlOS::memcpy( (void *) &(sTemplate->tmplate.rows[sCurRowID].columns[i]),
                           (void *) (aTableInfo->columns[i].basicInfo),
                           ID_SIZEOF(mtcColumn) );

            // PR-13597
            if( aNullableFlag == MTC_COLUMN_NOTNULL_FALSE )
            {
                sTemplate->tmplate.rows[sCurRowID].columns[i].flag &=
                    ~(MTC_COLUMN_NOTNULL_MASK);

                sTemplate->tmplate.rows[sCurRowID].columns[i].flag |=
                    (MTC_COLUMN_NOTNULL_FALSE);
            }
            else
            {
                // �׳� ������ ������ ��.
            }

            /* PROJ-2160 */
            if ( (aTableInfo->columns[i].basicInfo)->type.dataTypeId == MTD_GEOMETRY_ID )
            {
                sTemplate->tmplate.rows[sCurRowID].lflag &= ~(MTC_TUPLE_ROW_GEOMETRY_MASK);
                sTemplate->tmplate.rows[sCurRowID].lflag |= (MTC_TUPLE_ROW_GEOMETRY_TRUE);

                /* BUG-44382 clone tuple ���ɰ��� */
                /* geometry tuple���� column�� �����Ѵ�. */
                if ( ((aTableInfo->columns[i].basicInfo)->column.flag & SMI_COLUMN_TYPE_MASK)
                     == SMI_COLUMN_TYPE_VARIABLE_LARGE )
                {
                    // ���簡 �ʿ���
                    setTupleColumnFlag( &(sTemplate->tmplate.rows[sCurRowID]),
                                        ID_TRUE,
                                        ID_FALSE );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // nothing todo
            }
        }

        // PROJ-1473
        IDE_TEST( qtc::allocAndInitColumnLocateInTuple(
                      QC_QMP_MEM(aStatement),
                      &(sTemplate->tmplate),
                      sCurRowID )
            != IDE_SUCCESS );

        // Execute ������ Setting�� ���� �޸� �Ҵ�
        IDU_LIMITPOINT("qtc::nextTable::malloc2");
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(mtcExecute)
                                                * sTemplate->tmplate.rows[sCurRowID].columnCount,
                                                (void**)&(sTemplate->tmplate.rows[sCurRowID].execute))
                 != IDE_SUCCESS);


        //----------------------------------------------
        // [Cursor Flag �� ����]
        // �����ϴ� ���̺��� ������ ����, Ager�� ������ �ٸ��� �ȴ�.
        // �̸� ���� Memory, Disk, Memory�� Disk�� �����ϴ� ����
        // ��Ȯ�ϰ� �Ǵ��Ͽ��� �Ѵ�.
        // ���ǹ��� �������� �ٸ� ���̺��� ������ �� �����Ƿ�,
        // �ش� Cursor Flag�� ORing�Ͽ� �� ������ �����ǰ� �Ѵ�.
        //----------------------------------------------

        // Disk/Memory Table ���θ� ����
        if(ID_TRUE == aIsDiskTable)
        {
            // Disk Table�� ���
            sTemplate->tmplate.rows[sCurRowID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
            sTemplate->tmplate.rows[sCurRowID].lflag |= MTC_TUPLE_STORAGE_DISK;

            // Cursor Flag�� ����
            sTemplate->smiStatementFlag |= SMI_STATEMENT_DISK_CURSOR;
        }
        else
        {
            // Memory Table�� ���
            sTemplate->tmplate.rows[sCurRowID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
            sTemplate->tmplate.rows[sCurRowID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

            // Cursor Flag�� ����
            sTemplate->smiStatementFlag |= SMI_STATEMENT_MEMORY_CURSOR;
        }
    }
    else
    {
        //--------------------------------------------------------
        // �ӽ� Table ��������,
        // Execution ������ ��� ������ �����ȴ�.
        //--------------------------------------------------------
        sTemplate->tmplate.rows[sCurRowID].lflag = 0;
        sTemplate->tmplate.rows[sCurRowID].lflag &= ~MTC_TUPLE_TYPE_MASK;
        sTemplate->tmplate.rows[sCurRowID].lflag |= MTC_TUPLE_TYPE_TABLE;
        sTemplate->tmplate.rows[sCurRowID].lflag &= ~MTC_TUPLE_ROW_SKIP_MASK;
        sTemplate->tmplate.rows[sCurRowID].lflag |= MTC_TUPLE_ROW_SKIP_TRUE;

        sTemplate->tmplate.rows[sCurRowID].columnCount
            = sTemplate->tmplate.rows[sCurRowID].columnMaximum
            = 0;
        /* BUG-46023 fix */
        sTemplate->tmplate.rows[sCurRowID].columns = NULL;
        sTemplate->tmplate.rows[sCurRowID].columnLocate = NULL;
        sTemplate->tmplate.rows[sCurRowID].execute = NULL;
    }

    // PROJ-1789 PROWID
    sTemplate->tmplate.rows[sCurRowID].ridExecute = &gQtcRidExecute;

    // set out param
    *aRow = sTemplate->tmplate.rowCount;
    // rowCount ������Ŵ
    sTemplate->tmplate.rowCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TUPLE_SHORTAGE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TUPLE_SHORTAGE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::increaseInternalTuple( qcStatement* aStatement,
                            UShort       aIncreaseCount )
{
/***********************************************************************
 *
 * Description :
 *
 *     PROJ-1358
 *     Internal Tuple Set�� Ȯ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qtc::increaseInternalTuple"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt       sNewArrayCnt;
    UInt       sOldArrayCnt;
    mtcTuple * sNewTuple;
    mtcTuple * sOldTuple;

    qcTableMap * sNewTableMap;
    qcTableMap * sOldTableMap;
    qcTemplate * sTemplate = NULL;
    
    //---------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------

    // BUG-45666 DDL ���� ���� partition table�� ���� fk constraint ���� ��
    // execution �ܰ迡���� tuple set�� �ʿ�.
    if ( QC_SHARED_TMPLATE(aStatement) == NULL )
    {
        if ( ( aStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_MASK )
             == QCI_STMT_MASK_DDL )
        {
            sTemplate = QC_PRIVATE_TMPLATE(aStatement);
        }
        else
        {
            // BUG-21627
            // execution�ÿ� tuple Ȯ���� plan�� �����Ű�Ƿ� ������� �ʴ´�.
            IDE_RAISE( ERR_TUPLE_SHORTAGE );
        }
    }
    else
    {
        sTemplate = QC_SHARED_TMPLATE(aStatement);
    }
    
    IDE_DASSERT( sTemplate->tmplate.rowCount <=
                 sTemplate->tmplate.rowArrayCount );

    //---------------------------------------------
    // ���ο� Internal Tuple�� ���� Ȯ��
    //---------------------------------------------

    // To Fix PR-12659
    // Internal Tuple Set�� allocStatement() ������ ������ �Ҵ���� �ʰ�,
    // �ʿ信 ���Ͽ� �Ҵ�޵��� �Ѵ�.

    if ( aIncreaseCount == 0 )
    {
        // ���� �Ҵ� �޴� ���
        sNewArrayCnt = MTC_TUPLE_ROW_INIT_CNT;
    }
    else
    {
        // BUG-21627
        // aIncreaseCount��ŭ Ȯ����
        sNewArrayCnt = (UInt)sTemplate->tmplate.rowArrayCount + (UInt)aIncreaseCount;
    }

    /* BUG-48299 */
    if ( sNewArrayCnt == MTC_TUPLE_ROW_MAX_CNT + 1 )
    {
        sNewArrayCnt = MTC_TUPLE_ROW_MAX_CNT;
    }

    IDE_TEST_RAISE( sNewArrayCnt > MTC_TUPLE_ROW_MAX_CNT, ERR_TUPLE_SHORTAGE );

    IDU_LIMITPOINT("qtc::increaseInternalTuple::malloc1");
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(mtcTuple) * sNewArrayCnt,
                                             (void**) & sNewTuple )
              != IDE_SUCCESS );

    //---------------------------------------------
    // Internal Tuple�� ���� ����
    // ������ ������ �������� ����.
    //---------------------------------------------

    sOldArrayCnt = sTemplate->tmplate.rowArrayCount;

    if ( sOldArrayCnt == 0 )
    {
        // To Fix PR-12659
        // ���� �Ҵ�޴� ���� ���� Internal Tuple Set�� �������� ����.
    }
    else
    {
        sOldTuple = sTemplate->tmplate.rows;
        
        // ���� Internal Tuple ������ ����
        idlOS::memcpy( sNewTuple,
                       sOldTuple,
                       ID_SIZEOF(mtcTuple) * sOldArrayCnt );
        
        // ���� tuple�� �����Ѵ�.
        IDE_TEST( QC_QMP_MEM(aStatement)->free( (void*) sOldTuple )
                  != IDE_SUCCESS );
    }

    sTemplate->tmplate.rowArrayCount = sNewArrayCnt;
    sTemplate->tmplate.rows = sNewTuple;

    //---------------------------------------------
    // ���ο� Table Map�� ���� Ȯ��
    //---------------------------------------------

    IDU_LIMITPOINT("qtc::increaseInternalTuple::malloc2");
    IDE_TEST( QC_QMP_MEM(aStatement)->cralloc(
                  ID_SIZEOF(qcTableMap) * sNewArrayCnt,
                  (void**) & sNewTableMap )
              != IDE_SUCCESS );

    if ( sOldArrayCnt == 0 )
    {
        // To Fix PR-12659
        // ���� �Ҵ�޴� ���� ���� Internal Tuple Set�� �������� ����.
    }
    else
    {
        sOldTableMap = sTemplate->tableMap;

        // ���� Table Map ������ ����
        idlOS::memcpy( sNewTableMap,
                       sOldTableMap,
                       ID_SIZEOF(qcTableMap) * sOldArrayCnt );

        // ���� tableMap�� �����Ѵ�.
        IDE_TEST( QC_QMP_MEM(aStatement)->free( (void*) sOldTableMap )
                  != IDE_SUCCESS );
    }

    sTemplate->tableMap = sNewTableMap;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TUPLE_SHORTAGE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TUPLE_SHORTAGE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::smiCallBack( idBool       * aResult,
                         const void   * aRow,
                         void         *, /* aDirectKey */
                         UInt          , /* aDirectKeyPartialSize */
                         const scGRID   aRid,  /* BUG-16318 */
                         void         * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Storage Manager�� ���� ȣ��Ǵ� Filter�� ���� CallBack �Լ�
 *    CallBack Filter�� �⺻ �����̸�, smiCallBackAnd�� �Բ�
 *    ���� ���� CallBack�� ���� �����ϴ�.
 *    Filter�� �ϳ��� ���� ���, ȣ��ȴ�.
 *
 *    BUG-16318
 *    Lob���� �Լ��� ��� Locator�� �������� rid�� �ʿ�� �ϹǷ�
 *    aRid�� �߰��Ǿ���.
 *
 * Implementation :
 *
 *    Filter�� ���� ����� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::smiCallBack"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcSmiCallBackData* sData = (qtcSmiCallBackData*)aData;
    void*               sValueTemp;

    /* BUGBUG: const void* to void* */
    sData->table->row = (void*)aRow;
    sData->table->rid = aRid;
    sData->table->modify++;

    // BUG-33674
    IDE_TEST_RAISE( sData->tmplate->stackRemain < 1, ERR_STACK_OVERFLOW );
    
    IDE_TEST( sData->calculate( sData->node,
                                sData->tmplate->stack,
                                sData->tmplate->stackRemain,
                                sData->calculateInfo,
                                sData->tmplate )
              != IDE_SUCCESS );

    /* PROJ-2180
       qp ������ mtc::value �� ����Ѵ�.
       �ٸ� ���⿡���� ������ ���ؼ� valueForModule �� ����Ѵ�. */
    sValueTemp = (void*)mtd::valueForModule( (smiColumn*)sData->column,
                                             sData->tuple->row,
                                             MTD_OFFSET_USE,
                                             sData->column->module->staticNull );

    // BUG-34321 return value optimization
    return sData->isTrue( aResult,
                          sData->column,
                          sValueTemp );

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::smiCallBackAnd( idBool       * aResult,
                            const void   * aRow,
                            void         *, /* aDirectKey */
                            UInt          , /* aDirectKeyPartialSize */
                            const scGRID   aRid,  /* BUG-16318 */
                            void         * aData )
{
/***********************************************************************
 *
 * Description :
 *
 *    Storage Manager�� ���� ȣ��Ǵ� Filter�� ���� CallBack �Լ�
 *    2�� �̻��� Filter�� ���� ���, ���ȴ�.
 *
 *    BUG-16318
 *    Lob���� �Լ��� ��� Locator�� �������� rid�� �ʿ�� �ϹǷ�
 *    aRid�� �߰��Ǿ���.
 *
 * Implementation :
 *
 *    ���� Filter�� ���� ����� TRUE�� ���,
 *    ���� Filter�� �����Ѵ�.
 *    A4������ 3���� Filter�� ���յ� �� �ִ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::smiCallBack"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcSmiCallBackDataAnd* sData = (qtcSmiCallBackDataAnd*)aData;

    // ù ��° Filter ����
    IDE_TEST( smiCallBack( aResult,
                           aRow,
                           NULL,
                           0,
                           aRid,
                           sData->argu1 )
              != IDE_SUCCESS );

    if (*aResult == ID_TRUE && sData->argu2 != NULL )
    {
        // �� ��° Filter ����
        IDE_TEST( smiCallBack( aResult,
                               aRow,
                               NULL,
                               0,
                               aRid,
                               sData->argu2 )
                  != IDE_SUCCESS );

        if ( *aResult == ID_TRUE && sData->argu3 != NULL )
        {
            // �� ��° Filter ����
            IDE_TEST( smiCallBack( aResult,
                                   aRow,
                                   NULL,
                                   0,
                                   aRid,
                                   sData->argu3 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

void qtc::setSmiCallBack( qtcSmiCallBackData* aData,
                          qtcNode*            aNode,
                          qcTemplate*         aTemplate,
                          UInt                aTable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Storage Manager���� ����� �� �ֵ��� CallBack Data�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::setSmiCallBack"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aData->node          = (mtcNode*)aNode;
    aData->tmplate       = &aTemplate->tmplate;
    aData->table         = aData->tmplate->rows + aTable;
    aData->tuple         = aData->tmplate->rows + aNode->node.table;
    aData->calculate     = aData->tuple->execute[aNode->node.column].calculate;
    aData->calculateInfo =
        aData->tuple->execute[aNode->node.column].calculateInfo;
    aData->column        = aData->tuple->columns + aNode->node.column;
    aData->isTrue        = aData->column->module->isTrue;

#undef IDE_FN
}

void qtc::setSmiCallBackAnd( qtcSmiCallBackDataAnd* aData,
                             qtcSmiCallBackData*    aArgu1,
                             qtcSmiCallBackData*    aArgu2,
                             qtcSmiCallBackData*    aArgu3 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Storage Manager���� ����� �� �ֵ���
 *    ���� ���� CallBack Data�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::setSmiCallBack"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aData->argu1 = aArgu1;
    aData->argu2 = aArgu2;
    aData->argu3 = aArgu3;

#undef IDE_FN
}

void qtc::setCharValue( mtdCharType* aValue,
                        mtcColumn*   aColumn,
                        SChar*       aString )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� String���κ��� CHAR type value�� ������.
 *
 * Implementation :
 *    Data�� ���� �κ��� ' '�� canonize�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "void qtc::setCharValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aValue->length = aColumn->precision;
    idlOS::memset( aValue->value, 0x20, aValue->length );
    idlOS::memcpy( aValue->value, aString, idlOS::strlen( (char*)aString ) );

#undef IDE_FN
}

void qtc::setVarcharValue( mtdCharType* aValue,
                           mtcColumn*,
                           SChar*       aString,
                           SInt         aLength )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� String���κ��� VARCHAR type value�� ������.
 *
 * Implementation :
 *
 *    ���� ���̸�ŭ�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "void qtc::setCharValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( aLength > 0 )
    {
        aValue->length = aLength;
    }
    else
    {
        aValue->length = idlOS::strlen( (char*)aString );
    }

    idlOS::memcpy( aValue->value, aString, aValue->length );

#undef IDE_FN
}

void qtc::initializeMetaRange( smiRange * aRange,
                               UInt       aCompareType )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta Key Range�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    aRange->prev = aRange->next = NULL;

    if ( aCompareType == MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL ||
         aCompareType == MTD_COMPARE_MTDVAL_MTDVAL )
    {
        aRange->minimum.callback = rangeMinimumCallBack4Mtd;
        aRange->maximum.callback = rangeMaximumCallBack4Mtd;
    }
    else 
    {
        if ( ( aCompareType == MTD_COMPARE_STOREDVAL_MTDVAL ) ||
             ( aCompareType == MTD_COMPARE_STOREDVAL_STOREDVAL ) )
        {
            aRange->minimum.callback = rangeMinimumCallBack4Stored;
            aRange->maximum.callback = rangeMaximumCallBack4Stored;
        }
        else
        {
            /* PROJ-2433 */
            aRange->minimum.callback = rangeMinimumCallBack4IndexKey;
            aRange->maximum.callback = rangeMaximumCallBack4IndexKey;
        }
    }

    aRange->minimum.data        = NULL;
    aRange->maximum.data        = NULL;
}

void qtc::setMetaRangeIsNullColumn(qtcMetaRangeColumn* aRangeColumn,
                                   const mtcColumn*    aColumnDesc,
                                   UInt                aColumnIdx )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta�� ����Is null Key Range ������ ����
 *
 * Implementation :
 *    Column ������ Value ������ �̿��Ͽ� Key Range ������ ����
 *    ��, �ϳ��� Column�� ���� key range ������ �����ȴ�.
 *
 ***********************************************************************/
    
    aRangeColumn->next                    = NULL;

    if ( ( aColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        aRangeColumn->compare = qtc::compareMaximumLimit4Stored;
    }
    else
    {
        aRangeColumn->compare = qtc::compareMaximumLimit4Mtd;
    }
    
    aRangeColumn->columnIdx               = aColumnIdx;
    aRangeColumn->columnDesc              = *aColumnDesc;
//    aRangeColumn->valueDesc               = NULL;
    aRangeColumn->value                   = NULL;

}

void qtc::setMetaRangeColumn( qtcMetaRangeColumn* aRangeColumn,
                              const mtcColumn*    aColumnDesc,
                              const void*         aValue,
                              UInt                aOrder,
                              UInt                aColumnIdx )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta�� ���� Key Range ������ ����
 *
 * Implementation :
 *    Column ������ Value ������ �̿��Ͽ� Key Range ������ ����
 *    ��, �ϳ��� Column�� ���� key range ������ �����ȴ�.
 *
 ***********************************************************************/
    UInt    sCompareType;

    // PROJ-1872
    // index�� �ִ� Į���� meta range�� ���� �Ǹ�
    // disk index column�� compare�� stored type�� mt type ���� ���̴�.
    if( ( aColumnDesc->column.flag & SMI_COLUMN_STORAGE_MASK )
        == SMI_COLUMN_STORAGE_DISK )
    {
        sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        if( (aColumnDesc->column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            sCompareType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
        }
        else
        {
            IDE_DASSERT( ( (aColumnDesc->column.flag & SMI_COLUMN_TYPE_MASK)
                           == SMI_COLUMN_TYPE_VARIABLE ) ||
                         ( (aColumnDesc->column.flag & SMI_COLUMN_TYPE_MASK)
                           == SMI_COLUMN_TYPE_VARIABLE_LARGE ) );

            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
        }
    }

    aRangeColumn->next                    = NULL;
    aRangeColumn->compare                 =
        (aOrder == SMI_COLUMN_ORDER_ASCENDING) ?
        aColumnDesc->module->keyCompare[sCompareType]
                                       [MTD_COMPARE_ASCENDING] :
        aColumnDesc->module->keyCompare[sCompareType]
                                       [MTD_COMPARE_DESCENDING];

    aRangeColumn->columnIdx               = aColumnIdx;
    MTC_COPY_COLUMN_DESC( &(aRangeColumn->columnDesc), aColumnDesc );
    MTC_COPY_COLUMN_DESC( &(aRangeColumn->valueDesc), aColumnDesc );
    aRangeColumn->valueDesc.column.offset = 0;
    aRangeColumn->valueDesc.column.flag &= ~SMI_COLUMN_TYPE_MASK;
    aRangeColumn->valueDesc.column.flag |= SMI_COLUMN_TYPE_FIXED;
    aRangeColumn->value                   = aValue;
}

void qtc::changeMetaRangeColumn( qtcMetaRangeColumn* aRangeColumn,
                                 const mtcColumn*    aColumnDesc,
                                 UInt                aColumnIdx )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *
 *    Meta�� ���� Key Range ������ ����
 *    Key Range�� column������ ������ ��Ƽ�ǿ� �´� �÷����� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    aRangeColumn->columnIdx               = aColumnIdx;
    MTC_COPY_COLUMN_DESC( &(aRangeColumn->columnDesc), aColumnDesc );
    MTC_COPY_COLUMN_DESC( &(aRangeColumn->valueDesc), aColumnDesc );
    aRangeColumn->valueDesc.column.offset = 0;
    aRangeColumn->valueDesc.column.flag &= ~SMI_COLUMN_TYPE_MASK;
    aRangeColumn->valueDesc.column.flag |= SMI_COLUMN_TYPE_FIXED;
}


void qtc::addMetaRangeColumn( smiRange*           aRange,
                              qtcMetaRangeColumn* aRangeColumn )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta�� ���� �ϳ��� Key Range������ smiRange�� �����
 *
 * Implementation :
 *
 *    Meta�� ���� KeyRange�� '='�� ���� Range���� �����ϸ�,
 *    Key Range������ ������ �����ϴ� Multiple Key Range�� ������� �ʴ´�.
 *    ����, Minimum Range�� Maximum Range�� �����ϴٴ� �����Ͽ���,
 *    Range�� �����ϰ� �ȴ�.
 *
 *    ���� Column�� ���� Range�� minimum/maximum range��,
 *    ���� Column�� ���� Range�� �ڷ� �����Ѵ�.
 *
 *    �� ���� Range�� ����� ����� ������ ������ ����
 *    ex) A[user_id = 'abc'] and B[table_name = 't1']
 *
 *        smiRange->min-------          ->max
 *                           |             |
 *                           V             V
 *                        [A Range] --> [B Range]
 *
 *        max range�� ���, ���� Range�� �߰��� ���� �����Ǹ�,
 *        Range�� �߰��� �� �̻� ���� ���, qtc::fixMetaRange()�� ȣ����
 *        ����, min�� ������ ��ġ�� ����Ű�� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "void qtc::addMetaRangeColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if( aRange->minimum.data == NULL )
    {

        aRange->minimum.data =
            aRange->maximum.data = aRangeColumn;
    }
    else
    {
        ((qtcMetaRangeColumn*)aRange->maximum.data)->next = aRangeColumn;
        aRange->maximum.data                              = aRangeColumn;
    }

#undef IDE_FN
}

void qtc::fixMetaRange( smiRange* aRange )
{
/***********************************************************************
 *
 * Description :
 *
 *    Meta�� ���� smiRange�� Key Range ������ �Ϸ���.
 *
 * Implementation :
 *
 *    qtc::addMetaRangeColumn()���� ���� ���ҵ���, maximum ������
 *    Range�� �߰��� ���� ���� ������ Range�� �����ϰ� �ִ�.
 *    �̸� ���� ��ġ�� ���� Key Range ������ �Ϸ��ϰ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "void qtc::fixMetaRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aRange->maximum.data = aRange->minimum.data;

#undef IDE_FN
}

IDE_RC qtc::fixAfterParsing( qcTemplate    * aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing�� �Ϸ�� ��, Tuple Set�� ���� ó���� ��
 *
 * Implementation :
 *
 *    CONSTANT TUPLE�� Execute�� ���� ������ �Ҵ�޴´�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::fixAfterParsing"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aTemplate->tmplate.currentRow[MTC_TUPLE_TYPE_VARIABLE] = ID_USHORT_MAX;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qtc::createColumn( qcStatement*    aStatement,
                          qcNamePosition* aPosition,
                          mtcColumn**     aColumn,
                          UInt            aArguments,
                          qcNamePosition* aPrecision,
                          qcNamePosition* aScale,
                          SInt            aScaleSign,
                          idBool          aIsForSP )
{
/***********************************************************************
 *
 * Description :
 *
 *    CREATE TABLE ���� ��� ���� COLUMN TYPE�� ���Ǹ� �̿��Ͽ�
 *    Column ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Column ���Ǹ� ���� ������ �Ҵ�ް�,
 *    Column Type�� data type ����� ã��, precision �� scale�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::createColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    idBool            sIsTimeStamp = ID_FALSE;
    SInt              sPrecision;
    SInt              sScale;
    const mtdModule * sModule;

    // BUG-27034 Code Sonar
    IDE_TEST_RAISE( (aPosition->offset < 0) || (aPosition->size < 0),
                    ERR_EMPTY_DATATYPE_POSITION );

    if ( aArguments == 0 )
    {
        if ( idlOS::strMatch( aPosition->stmtText + aPosition->offset,
                              aPosition->size,
                              "TIMESTAMP",
                              9) == 0 )
        {
            sIsTimeStamp = ID_TRUE;
        }
    }

    if ( sIsTimeStamp == ID_FALSE )
    {
        // BUG-16233
        IDU_LIMITPOINT("qtc::createColumn::malloc1");
        IDE_TEST(STRUCT_CRALLOC(QC_QMP_MEM(aStatement), mtcColumn, aColumn)
                 != IDE_SUCCESS);

        // initialize : mtc::initializeColumn()���� arguements�� �ʱ�ȭ��
        (*aColumn)->flag = 0;

        // find mtdModule
        // mtdModule�� mtdNumber Type�� ���,
        // arguement�� ���� mtdFloat �Ǵ� mtdNummeric���� �ʱ�ȭ �ؾ���
        IDE_TEST(
            mtd::moduleByName( & sModule,
                               (void*)(aPosition->stmtText+aPosition->offset),
                               aPosition->size )
            != IDE_SUCCESS );

        if ( sModule->id == MTD_NUMBER_ID )
        {
            if ( aArguments == 0 )
            {
                IDE_TEST( mtd::moduleByName( & sModule, (void*)"FLOAT", 5 )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( mtd::moduleByName( & sModule, (void*)"NUMERIC", 7 )
                          != IDE_SUCCESS );
            }
        }
        else if ( (sModule->id == MTD_BLOB_ID) ||
                  (sModule->id == MTD_CLOB_ID) )
        {
            // PROJ-1362
            // clob�� arguments�� �׻� 0�̾�� �Ѵ�.
            // �׷��� estimate�ÿ��� 1�� ����ϱ� ������
            // ���Ƿ� ������ ���� ���� 2�� �ٲ۴�.
            if ( aArguments != 0 )
            {
                aArguments = 2;
            }
            else
            {
                if ( aIsForSP != ID_TRUE )
                {
                    /* BUG-36429 LOB Column�� �����ϴ� ���, Precision�� 0���� �����Ѵ�. */
                    aArguments = 1;
                }
                else
                {
                    /* BUG-36429 LOB Value�� �����ϴ� ���, Precision�� ������Ƽ ������ �����Ѵ�. */
                }
            }
        }
        else if ( (sModule->id == MTD_ECHAR_ID) ||
                  (sModule->id == MTD_EVARCHAR_ID) )
        {
            // PROJ-2002 Column Security
            // create�� echar, evarchar�� ����� �� ����.
            IDE_RAISE( ERR_INVALID_ENCRYPTION_DATATYPE );
        }
        else
        {
            // nothing to do
        }

        if ( aPrecision != NULL )
        {
            IDE_TEST_RAISE( (aPrecision->offset < 0) || (aPrecision->size < 0),
                            ERR_EMPTY_PRECISION_POSITION );
            
            sPrecision = (SInt)idlOS::strToUInt(
                (UChar*)(aPrecision->stmtText+aPrecision->offset),
                aPrecision->size );
        }
        else
        {
            sPrecision = 0;
        }

        if ( aScale != NULL )
        {
            IDE_TEST_RAISE( (aScale->offset < 0) || (aScale->size < 0),
                            ERR_EMPTY_SCALE_POSITION );
            
            sScale = (SInt)idlOS::strToUInt(
                (UChar*)(aScale->stmtText+aScale->offset),
                aScale->size ) * aScaleSign;
        }
        else
        {
            sScale = 0;
        }

        IDE_TEST( mtc::initializeColumn(
                      *aColumn,
                      sModule,
                      aArguments,
                      sPrecision,
                      sScale )
                  != IDE_SUCCESS );
    }
    else
    {
        // in case of TIMESTAMP
        IDE_TEST( qtc::createColumn4TimeStamp( aStatement, aColumn )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ENCRYPTION_DATATYPE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_ENCRYPTION_DATATYPE));
    }
    IDE_EXCEPTION( ERR_EMPTY_DATATYPE_POSITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::createColumn",
                                  "Nameposition of data type is empty" ));
    }
    IDE_EXCEPTION( ERR_EMPTY_PRECISION_POSITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::createColumn",
                                  "Nameposition of precision is empty" ));
    }
    IDE_EXCEPTION( ERR_EMPTY_SCALE_POSITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::createColumn",
                                  "Nameposition of scale is empty" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::createColumn( qcStatement*    aStatement,
                          mtdModule  *    aModule,
                          mtcColumn**     aColumn,
                          UInt            aArguments,
                          qcNamePosition* aPrecision,
                          qcNamePosition* aScale,
                          SInt            aScaleSign )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Column�� data type module�� �̿��Ͽ�
 *    Column ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Column ���Ǹ� ���� ������ �Ҵ�ް�,
 *    �־��� module ������ �����ϰ�, precision �� scale�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::createColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SInt sPrecision;
    SInt sScale;

    // BUG-16233
    IDU_LIMITPOINT("qtc::createColumn::malloc2");
    IDE_TEST(STRUCT_CRALLOC(QC_QMP_MEM(aStatement), mtcColumn, aColumn)
             != IDE_SUCCESS);

    // initialize : mtc::initializeColumn()���� arguements�� �ʱ�ȭ��
    (*aColumn)->flag = 0;

    // PROJ-1362
    // clob�� arguments�� �׻� 0�̾�� �Ѵ�.
    // �׷��� estimate�ÿ��� 1�� ����ϱ� ������
    // ���Ƿ� ������ ���� ���� 2�� �ٲ۴�.
    if ( (aModule->id == MTD_BLOB_ID) ||
         (aModule->id == MTD_CLOB_ID) )
    {
        if ( aArguments != 0 )
        {
            aArguments = 2;
        }
        else
        {
            // Nothing to do.
        }
    }
    else if ( (aModule->id == MTD_ECHAR_ID) ||
              (aModule->id == MTD_EVARCHAR_ID) )
    {
        // PROJ-2002 Column Security
        // create�� echar, evarchar�� ����� �� ����.
        IDE_RAISE( ERR_INVALID_ENCRYPTION_DATATYPE );
    }
    else
    {
        // Nothing to do.
    }

    sPrecision = ( aPrecision != NULL ) ?
        (SInt)idlOS::strToUInt( (UChar*)(aPrecision->stmtText+aPrecision->offset),
                                aPrecision->size ) : 0 ;

    sScale     = ( aScale     != NULL ) ?
        (SInt)idlOS::strToUInt( (UChar*)(aScale->stmtText+aScale->offset),
                                aScale->size ) * aScaleSign : 0 ;

    IDE_TEST( mtc::initializeColumn(
                  *aColumn,
                  aModule,
                  aArguments,
                  sPrecision,
                  sScale )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ENCRYPTION_DATATYPE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_ENCRYPTION_DATATYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::createColumn4TimeStamp( qcStatement*    aStatement,
                                    mtcColumn**     aColumn )
{
#define IDE_FN "IDE_RC qtc::createColumn4TimeStamp"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // make BYTE ( 8 ) for TIMESTAMP

    SInt sPrecision;
    SInt sScale;

    // BUG-16233
    IDU_LIMITPOINT("qtc::createColumn4TimeStamp::malloc");
    IDE_TEST(STRUCT_CRALLOC(QC_QMP_MEM(aStatement), mtcColumn, aColumn)
             != IDE_SUCCESS);

    // initialize : mtc::initializeColum()���� arguements�� �ʱ�ȭ��
    //(*aColumn)->flag = 0;

    // precision, scale ����
    sPrecision = QC_BYTE_PRECISION_FOR_TIMESTAMP;
    sScale     = 0;

    // aColumn�� �ʱ�ȭ
    // : dataType�� byte, language��  session�� language�� ����
    IDE_TEST( mtc::initializeColumn(
                  *aColumn,
                  MTD_BYTE_ID,
                  1,
                  sPrecision,
                  sScale )
              != IDE_SUCCESS );

    (*aColumn)->flag &= ~MTC_COLUMN_TIMESTAMP_MASK;
    (*aColumn)->flag |= MTC_COLUMN_TIMESTAMP_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::changeColumn4SRID( qcStatement     * /*aStatement*/,
                               qdExtColumnAttr * aColumnAttr,
                               mtcColumn       * aColumn )
{
    IDE_ASSERT( aColumn     != NULL );
    IDE_ASSERT( aColumnAttr != NULL );

    // srid�� geometry�� ������
    IDE_TEST_RAISE( aColumn->module->id != MTD_GEOMETRY_ID,
                    ERR_INVALID_SRID_DATATYPE );

    // srid�� ����
    aColumn->mColumnAttr.mSridAttr.mSrid = aColumnAttr->mSrid;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_SRID_DATATYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_SRID_DATATYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeColumn4Encrypt( qcStatement     * aStatement,
                                  qdExtColumnAttr * aColumnAttr,
                                  mtcColumn       * aColumn )
{
    const mtdModule * sModule;
    SChar             sPolicy[MTC_POLICY_NAME_SIZE + 1] = { 0, };
    idBool            sIsExist;
    idBool            sIsSalt;
    idBool            sIsEncodeECC;
    UInt              sEncryptedSize;
    UInt              sECCSize;
    qcuSqlSourceInfo  sqlInfo;

    IDE_ASSERT( aColumn     != NULL );
    IDE_ASSERT( aColumnAttr != NULL );

    // encryption type�� char, varchar�� ������
    IDE_TEST_RAISE( ( aColumn->module->id != MTD_CHAR_ID ) &&
                    ( aColumn->module->id != MTD_VARCHAR_ID ),
                    ERR_INVALID_ENCRYPTION_DATATYPE );

    // policy name ���� �˻�
    if ( aColumnAttr->mPolicyPosition.size > MTC_POLICY_NAME_SIZE )
    {
        sqlInfo.setSourceInfo( aStatement, & aColumnAttr->mPolicyPosition );
        IDE_RAISE( ERR_TOO_LONG_POLICY_NAME );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( aColumnAttr->mPolicyPosition.size == 0 )
    {
        IDE_RAISE( ERR_NOT_EXIST_POLICY );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( aColumn->module->id == MTD_CHAR_ID )
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_ECHAR_ID )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_EVARCHAR_ID )
                  != IDE_SUCCESS );
    }

    // policy �˻�
    QC_STR_COPY( sPolicy, aColumnAttr->mPolicyPosition );

    IDE_TEST( qcsModule::getPolicyInfo( sPolicy,
                                        & sIsExist,
                                        & sIsSalt,
                                        & sIsEncodeECC )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsExist == ID_FALSE,
                    ERR_NOT_EXIST_POLICY );

    IDE_TEST_RAISE( sIsEncodeECC == ID_TRUE,
                    ERR_INVALID_POLICY );

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn(
                  aColumn,
                  sModule,
                  1,
                  aColumn->precision,  // 0
                  0 )
              != IDE_SUCCESS );
    
    // encryted size �˻�
    IDE_TEST( qcsModule::getEncryptedSize( sPolicy,
                                           aColumn->precision,  // 1
                                           & sEncryptedSize )
              != IDE_SUCCESS );

    IDE_TEST( qcsModule::getECCSize( aColumn->precision,  // 1
                                     & sECCSize )
              != IDE_SUCCESS );

    // aColumn�� �ι�° �ʱ�ȭ
    IDE_TEST( mtc::initializeEncryptColumn(
                  aColumn,
                  sPolicy,
                  sEncryptedSize,
                  sECCSize )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ENCRYPTION_DATATYPE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_ENCRYPTION_DATATYPE));
    }
    IDE_EXCEPTION( ERR_TOO_LONG_POLICY_NAME );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_TOO_LONG_POLICY_NAME,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_POLICY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_NOT_EXIST_POLICY, sPolicy));
    }
    IDE_EXCEPTION( ERR_INVALID_POLICY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_INVALID_POLICY, sPolicy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeColumn4Decrypt( mtcColumn    * aColumn )
{
#define IDE_FN "IDE_RC qtc::changeColumn4Decrypt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtdModule   * sModule;
    qcuSqlSourceInfo    sqlInfo;

    IDE_ASSERT( aColumn != NULL );

    // encryption type�� char, varchar�� ������
    IDE_TEST_RAISE( ( aColumn->module->id != MTD_ECHAR_ID ) &&
                    ( aColumn->module->id != MTD_EVARCHAR_ID ),
                    ERR_INVALID_ENCRYPTION_DATATYPE );

    if ( aColumn->module->id == MTD_ECHAR_ID )
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_CHAR_ID )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_VARCHAR_ID )
                  != IDE_SUCCESS );
    }

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn(
                  aColumn,
                  sModule,
                  1,
                  aColumn->precision,
                  0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ENCRYPTION_DATATYPE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_ENCRYPTION_DATATYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::changeColumn4Decrypt( mtcColumn    * aSrcColumn,
                                  mtcColumn    * aDestColumn )
{
/***********************************************************************
 *
 * Description : PROJ-1584 DML Return Clause
 *               DestColumn : Plain Column
 *               SrcColumn  : ��ȣȭ Column
 *
 *               ��ȣȭ Column�� Decrypt �Ͽ� initializeColumn ����.
 * Implementation :
 *
 **********************************************************************/
    const mtdModule   * sModule;

    IDE_ASSERT( aSrcColumn != NULL );

    // encryption type�� char, varchar�� ������
    IDE_TEST_RAISE( ( aSrcColumn->module->id != MTD_ECHAR_ID ) &&
                    ( aSrcColumn->module->id != MTD_EVARCHAR_ID ),
                    ERR_INVALID_ENCRYPTION_DATATYPE );

    if ( aSrcColumn->module->id == MTD_ECHAR_ID )
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_CHAR_ID )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mtd::moduleById( & sModule, MTD_VARCHAR_ID )
                  != IDE_SUCCESS );
    }

    // aColumn�� �ʱ�ȭ
    IDE_TEST( mtc::initializeColumn(
                  aDestColumn,
                  sModule,
                  1,
                  aSrcColumn->precision,
                  0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ENCRYPTION_DATATYPE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QTC_INVALID_ENCRYPTION_DATATYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::addOrArgument( qcStatement* aStatement,
                           qtcNode**    aNode,
                           qtcNode**    aArgument1,
                           qtcNode**    aArgument2 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing �ܰ迡�� OR keyword�� ������ ��, �ش� Node�� �����Ѵ�.
 *
 * Implementation :
 *
 *    �� ���� argument�� OR Node�� argument�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::addOrArgument"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    extern mtfModule mtfOr;

    if( ( aArgument1[0]->node.lflag &
          ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK )
            ) == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        // OR�� ��ø�� ���, ������ OR Node�� ����Ѵ�.
        // ������ ���� ���� ������ �����ȴ�.
        //
        //       aNode[0]                            aNode[1]
        //         |                                    |
        //         V                                    |
        //       [OR Node]                              |
        //         |                                    |
        //         V                                    V
        //       [Argument]---->[Argument]-- ... -->[Argument2]
        //
        // ���� ���� aNode[1]�� ���� argument�� �߰��Ͽ�
        // ��ø OR�� ó���Ѵ�.

        aNode[0] = aArgument1[0];
        aNode[1] = aArgument1[1];
        IDE_TEST( qtc::addArgument( aNode, aArgument2 ) != IDE_SUCCESS );
    }
    else
    {
        // ���ο� OR Node�� �����Ѵ�.
        IDU_LIMITPOINT("qtc::addOrArgument::malloc");
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
                 != IDE_SUCCESS);
        // OR Node �ʱ�ȭ
        QTC_NODE_INIT( aNode[0] );        

        aNode[1] = NULL;

        IDE_TEST_RAISE( aNode[0] == NULL, ERR_MEMORY_SHORTAGE );

        // OR Node ���� ����
        aNode[0]->node.lflag          = mtfOr.lflag
                                      & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->position.stmtText   = aArgument1[0]->position.stmtText;
        aNode[0]->position.offset     = aArgument1[0]->position.offset;
        aNode[0]->position.size       = aArgument2[0]->position.offset
                                        + aArgument2[0]->position.size
                                        - aArgument1[0]->position.offset;

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   mtfOr.lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        aNode[0]->node.module = &mtfOr;

        // Argument�� �����Ѵ�.
        IDE_TEST( qtc::addArgument( aNode, aArgument1 ) != IDE_SUCCESS );
        IDE_TEST( qtc::addArgument( aNode, aArgument2 ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_SHORTAGE );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_MEMORY_SHORTAGE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::addAndArgument( qcStatement* aStatement,
                            qtcNode**    aNode,
                            qtcNode**    aArgument1,
                            qtcNode**    aArgument2 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing �ܰ迡�� AND keyword�� ������ ��, �ش� Node�� �����Ѵ�.
 *
 * Implementation :
 *
 *    �� ���� argument�� OR Node�� argument�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::addAndArgument"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    extern mtfModule mtfAnd;

    if( ( aArgument1[0]->node.lflag &
          ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK )
            ) == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        // AND�� ��ø�� ���, ������ AND Node�� ����Ѵ�.
        // ������ ���� ���� ������ �����ȴ�.
        //
        //       aNode[0]                            aNode[1]
        //         |                                    |
        //         V                                    |
        //       [AND Node]                              |
        //         |                                    |
        //         V                                    V
        //       [Argument]---->[Argument]-- ... -->[Argument2]
        //
        // ���� ���� aNode[1]�� ���� argument�� �߰��Ͽ�
        // ��ø AND�� ó���Ѵ�.

        aNode[0] = aArgument1[0];
        aNode[1] = aArgument1[1];
        IDE_TEST( qtc::addArgument( aNode, aArgument2 ) != IDE_SUCCESS );
    }
    else
    {
        // ���ο� AND Node�� �����Ѵ�.
        IDU_LIMITPOINT("qtc::addAndArgument::malloc");
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
                 != IDE_SUCCESS);

        // AND Node �ʱ�ȭ
        QTC_NODE_INIT( aNode[0] );
        aNode[1] = NULL;

        IDE_TEST_RAISE( aNode[0] == NULL, ERR_MEMORY_SHORTAGE );

        // AND Node ���� ����
        aNode[0]->node.lflag          = mtfAnd.lflag
                                      & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->position.stmtText   = aArgument1[0]->position.stmtText;
        aNode[0]->position.offset     = aArgument1[0]->position.offset;
        aNode[0]->position.size       = aArgument2[0]->position.offset
                                        + aArgument2[0]->position.size
                                        - aArgument1[0]->position.offset;

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   mtfAnd.lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        aNode[0]->node.module = &mtfAnd;

        // Argument�� �����Ѵ�.
        IDE_TEST( qtc::addArgument( aNode, aArgument1 ) != IDE_SUCCESS );
        IDE_TEST( qtc::addArgument( aNode, aArgument2 ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_SHORTAGE );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_MEMORY_SHORTAGE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::lnnvlNode( qcStatement  * aStatement,
                       qtcNode      * aNode )
{
/***********************************************************************
 *
 * Description :
 *     BUG-36125
 *     LNNVL�Լ��� �־��� predicate�� �����Ų��.
 *
 * Implementation :
 *     �� �����ڴ� notNode������ �����ϰ� counter operator�� ��ȯ�ϰ�,
 *     �� �� �� predicatea�鿡�� LNNVL�� �����.
 *     ex) i1 < 0 AND i1 > 1 => LNNVL(i1 <0) OR LNNVL(i1 > 1)
 *
 *     BUG-41294
 *     ROWNUM Predicate�� Copy�� �� qtcNodeTree�� ���еǾ�� �ϹǷ�
 *     �� �����ڶ� ���� �� counter operator�� �����ؾ� �Ѵ�.
 *
 *     [ ���� ]
 *     �� �Լ��� caller�� 2���� �ִµ�, ���� estimate�� �����Ѵ�.
 *     - qtc::notNode() => Parsing Phase���� estimate
 *     - qmoDnfMgr::makeDnfNotNormal=> lnnvlNode() ���� estimate
 *
 ***********************************************************************/

    qtcNode*            sNode;
    qcuSqlSourceInfo    sqlInfo;

    if( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
          == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        // ��������
        if( aNode->node.module->counter != NULL )
        {
            IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                       aNode,
                                       aStatement,
                                       QC_SHARED_TMPLATE(aStatement),
                                       MTC_TUPLE_TYPE_INTERMEDIATE,
                                       1 )
                      != IDE_SUCCESS );

            aNode->node.module = aNode->node.module->counter;

            aNode->node.lflag &= ~MTC_NODE_OPERATOR_MASK;
            aNode->node.lflag |= aNode->node.module->lflag & MTC_NODE_OPERATOR_MASK;
            aNode->node.lflag |= aNode->node.module->lflag & MTC_NODE_PRINT_FMT_MASK;

            aNode->node.lflag &= ~MTC_NODE_GROUP_MASK;
            aNode->node.lflag |= aNode->node.module->lflag & MTC_NODE_GROUP_MASK;

            for( sNode  = (qtcNode*)aNode->node.arguments;
                 sNode != NULL;
                 sNode  = (qtcNode*)sNode->node.next )
            {
                IDE_TEST( lnnvlNode( aStatement, sNode ) != IDE_SUCCESS );
            }
        }
        else
        {
            sqlInfo.setSourceInfo( aStatement, &aNode->position );
            IDE_RAISE( ERR_INVALID_ARGUMENT_TYPE );
        }
    }
    else
    {
        // �� ������
        // sNode�� �Ҵ��Ͽ� aNode�� �����ϰ�, aNode���� LNNVL�� �����Ѵ�.

        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
                 != IDE_SUCCESS);

        idlOS::memcpy( sNode, aNode, ID_SIZEOF( qtcNode ) );

        QTC_NODE_INIT( aNode );

        // Node ���� ����
        aNode->node.module = &mtfLnnvl;
        aNode->node.lflag  = mtfLnnvl.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode,
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   mtfLnnvl.lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        // aNode�� ������ LNNVL�� argument�� sNode�� �����Ѵ�.
        aNode->node.arguments  = (mtcNode *)sNode;

        aNode->node.next = (mtcNode *)sNode->node.next;
        sNode->node.next = NULL;

        IDE_TEST( estimateNodeWithArgument( aStatement,
                                            aNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARGUMENT_TYPE );
    (void)sqlInfo.init(aStatement->qmeMem);
    IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ARGUMENT_TYPE,
                              sqlInfo.getErrMessage() ) );
    (void)sqlInfo.fini();

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::notNode( qcStatement* aStatement,
                     qtcNode**    aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing �ܰ迡�� NOT keyword�� ������ ���� ó���� �Ѵ�.
 *
 * Implementation :
 *
 *    ������ NOT Node�� �������� �ʰ� Node Tree�� Traverse�ϸ�,
 *    �� ������ �� �� �����ڿ� ���Ͽ�
 *    NOT�� �ش��ϴ� Counter �����ڷ� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::notNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode*            sNode;
    qcuSqlSourceInfo    sqlInfo;

    if( aNode[0]->node.module->counter != NULL )
    {
        aNode[0]->node.module = aNode[0]->node.module->counter;

        aNode[0]->node.lflag &= ~MTC_NODE_OPERATOR_MASK;
        aNode[0]->node.lflag |=
                          aNode[0]->node.module->lflag & MTC_NODE_OPERATOR_MASK;
        // BUG-19180
        aNode[0]->node.lflag |=
                          aNode[0]->node.module->lflag & MTC_NODE_PRINT_FMT_MASK;

        // PROJ-1718 Subquery unnesting
        aNode[0]->node.lflag &= ~MTC_NODE_GROUP_MASK;
        aNode[0]->node.lflag |=
                          aNode[0]->node.module->lflag & MTC_NODE_GROUP_MASK;

        if ( ( aNode[0]->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            for( sNode  = (qtcNode*)aNode[0]->node.arguments;
                 sNode != NULL;
                 sNode  = (qtcNode*)sNode->node.next )
            {
                IDE_TEST( notNode( aStatement, &sNode ) != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing To Do
            // A3������ Subquery�� Predicate�� ������ �����Ͽ�����,
            // A4������ �������� �ʴ´�.
        }
    }
    else
    {
        // BUG-36125 NOT LNNVL(condition)�� LNNVL(LNNVL(condition))�� ��ȯ�Ѵ�.
        if( aNode[0]->node.module == &mtfLnnvl )
        {
            IDE_TEST( lnnvlNode( aStatement, aNode[0] ) != IDE_SUCCESS );
        }
        else
        {
            sqlInfo.setSourceInfo( aStatement, &aNode[0]->position );
            IDE_RAISE( ERR_INVALID_ARGUMENT_TYPE );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARGUMENT_TYPE );
    (void)sqlInfo.init(aStatement->qmeMem);
    IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_ARGUMENT_TYPE,
                              sqlInfo.getErrMessage() ) );
    (void)sqlInfo.fini();

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::priorNode( qcStatement* aStatement,
                       qtcNode**    aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing �ܰ迡�� PRIOR keyword�� ������ ���� ó���� �Ѵ�.
 *
 * Implementation :
 *
 *       ex) PRIOR (i1 + 1) : �ڽ� �� �ƴ϶�, argument���� prior��
 *           �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::priorNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode* sNode;

    aNode[0]->lflag |= QTC_NODE_PRIOR_EXIST;

    // Prior Column�� Column�ӿ��� �ұ��ϰ� �� Ư�� ��
    // ����� ����� Ư���� ���´�.  ��, Indexable Predicate�� �з� ���
    // Key Range������ ���� �Ǵ��� �� �� ����� ��޵Ǿ�� �Ѵ�.
    //     Ex) WHERE i1 = prior i2
    //         : prior i2�� index�� ����� �� �ִ� column��
    //           �ƴϳ�, �̷��� ������ �������� �Ѵ�.
    // �̷��� ó���� �����ϰ� �ϱ� ����, ������ ����
    // Dependency �� Indexable�� ���� ó���� �Ѵ�.

    // Dependencies �� ����
    qtc::dependencyClear( & aNode[0]->depInfo );

    // �ε����� ����� �� ���� Column���� ǥ��.
    aNode[0]->node.lflag &= ~MTC_NODE_INDEX_MASK;
    aNode[0]->node.lflag |= MTC_NODE_INDEX_UNUSABLE;

    for( sNode  = (qtcNode*)aNode[0]->node.arguments;
         sNode != NULL;
         sNode  = (qtcNode*)sNode->node.next )
    {
        IDE_TEST( priorNode( aStatement, &sNode ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::priorNodeSetWithNewTuple( qcStatement* aStatement,
                                      qtcNode**    aNode,
                                      UShort       aOriginalTable,
                                      UShort       aTable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Optimization �ܰ迡�� ó���Ǹ�,
 *    PRIOR Node�� ���ο� Tuple�� �������� �籸���Ѵ�.
 *
 *
 * Implementation :
 *
 *   �ش� Node�� Traverse�ϸ鼭,
 *   PRIOR Node�� Table ID�� ���ο� Table ID(aTable)�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::priorNodeSetWithNewTable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode* sNode;

    if( (aNode[0]->lflag & QTC_NODE_PRIOR_MASK) ==
        QTC_NODE_PRIOR_EXIST &&
        (aNode[0]->node.table == aOriginalTable ) )
    {
        aNode[0]->node.table = aTable;

        // set execute
        IDE_TEST( qtc::estimateNode( aStatement, aNode[0] )
                  != IDE_SUCCESS );
    }

    for( sNode  = (qtcNode*)aNode[0]->node.arguments;
         sNode != NULL;
         sNode  = (qtcNode*)sNode->node.next )
    {
        IDE_TEST( priorNodeSetWithNewTuple( aStatement,
                                            &sNode,
                                            aOriginalTable,
                                            aTable ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeNode( qcStatement*    aStatement,
                      qtcNode**       aNode,
                      qcNamePosition* aPosition,
                      const UChar*    aOperator,
                      UInt            aOperatorLength )
{
/***********************************************************************
 *
 * Description :
 *
 *    �����ڸ� ���� Node�� ������.
 *
 * Implementation :
 *
 *    �־��� String(aOperator)���κ��� �ش� function module�� ȹ���ϰ�,
 *    �̸� �̿��Ͽ� Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtfModule* sModule;
    idBool           sExist;

    // Node ����
    IDU_LIMITPOINT("qtc::makeNode::malloc1");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    // if you change this code, you should change qmvQTC::setColumnID
    if( aOperatorLength != 0 )
    {
        IDE_TEST( mtf::moduleByName( &sModule,
                                     &sExist,
                                     (void*)aOperator,
                                     aOperatorLength ) != IDE_SUCCESS );
        if ( sExist == ID_FALSE )
        {
            // if no module is found, use stored function module.
            // only in unified_invocation in qcply.y
            sModule = & qtc::spFunctionCallModule;
        }

        aNode[0]->node.lflag  = sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->node.module = sModule;
        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
        
        // set base table and column ID
        aNode[0]->node.baseTable = aNode[0]->node.table;
        aNode[0]->node.baseColumn = aNode[0]->node.column;
    }
    else
    {
        aNode[0]->node.lflag  = 0;
        aNode[0]->node.module = NULL;
    }

    if( &qtc::spFunctionCallModule == aNode[0]->node.module )
    {
        // fix BUG-14257
        aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
        aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
    }

    aNode[0]->position            = *aPosition;
    aNode[0]->columnName          = *aPosition;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeNodeForMemberFunc( qcStatement     * aStatement,
                                   qtcNode        ** aNode,
                                   qcNamePosition  * aPositionStart,
                                   qcNamePosition  * aPositionEnd,
                                   qcNamePosition  * aUserNamePos,
                                   qcNamePosition  * aTableNamePos,
                                   qcNamePosition  * aColumnNamePos,
                                   qcNamePosition  * aPkgNamePos )
{
/***********************************************************************
 *
 * Description : PROJ-1075
 *       array type�� member function�� ����Ͽ� node�� �����Ѵ�.
 *       �Ϲ� function�� �˻����� �ʴ´�.
 *
 * Implementation :
 *       �� �Լ��� �� �� �ִ� ����� �Լ� ����
 *           (1) arrvar_name.memberfunc_name()            - aUserNamePos(x)
 *           (2) user_name.spfunc_name()                  - aUserNamePos(x)
 *           (3) label_name.arrvar_name.memberfunc_name() - ��� ����
 *           (4) user_name.package_name.spfunc_name()     - ��� ����
 *            // BUG-38243 support array method at package
 *           (5) user_name.package_name.array_name.memberfunc_name
 *                -> (5)�� �׻� memberber function�� �� �ۿ� ����
 ***********************************************************************/

    const mtfModule* sModule;
    idBool           sExist;
    SChar            sBuffer[1024];
    SChar            sNameBuffer[256];
    UInt             sLength;

    // Node ����
    IDU_LIMITPOINT("qtc::makeNodeForMemberFunc::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // ���ռ� �˻�. ��� tableName, columnName�� �����ؾ� ��.
    IDE_DASSERT( QC_IS_NULL_NAME( (*aTableNamePos) ) == ID_FALSE );
    IDE_DASSERT( QC_IS_NULL_NAME( (*aColumnNamePos) ) == ID_FALSE );

    if ( QC_IS_NULL_NAME( (*aPkgNamePos) ) == ID_TRUE )
    {
        // Node ���� ����
        IDE_TEST( qsf::moduleByName( &sModule,
                                     &sExist,
                                     (void*)(aColumnNamePos->stmtText +
                                             aColumnNamePos->offset),
                                     aColumnNamePos->size )
                  != IDE_SUCCESS );
        
        if ( sExist == ID_FALSE )
        {
            sModule = & qtc::spFunctionCallModule;
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        IDE_TEST( qsf::moduleByName( &sModule,
                                     &sExist,
                                     (void*)(aPkgNamePos->stmtText +
                                             aPkgNamePos->offset),
                                     aPkgNamePos->size )
                  != IDE_SUCCESS );

        if ( sExist == ID_FALSE )
        {
            sLength = aPkgNamePos->size;
            
            if( sLength >= ID_SIZEOF(sNameBuffer) )
            {
                sLength = ID_SIZEOF(sNameBuffer) - 1;
            }
            else
            {
                /* Nothing to do. */
            }
            
            idlOS::memcpy( sNameBuffer,
                           (SChar*) aPkgNamePos->stmtText + aPkgNamePos->offset,
                           sLength );
            sNameBuffer[sLength] = '\0';
            idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                             "(Name=\"%s\")", sNameBuffer );
        
            IDE_RAISE( ERR_NOT_FOUND );
        }
        else
        {
            /* Nothing to do. */
        }
    }

    aNode[0]->node.lflag  = sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->node.module = sModule;

    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode[0],
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
              != IDE_SUCCESS );

    // set base table and column ID
    aNode[0]->node.baseTable = aNode[0]->node.table;
    aNode[0]->node.baseColumn = aNode[0]->node.column;
    
    if( &qtc::spFunctionCallModule == aNode[0]->node.module )
    {
        // fix BUG-14257
        aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
        aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
    }
    
    aNode[0]->userName   = *aUserNamePos;
    aNode[0]->tableName  = *aTableNamePos;
    aNode[0]->columnName = *aColumnNamePos;
    aNode[0]->pkgName    = *aPkgNamePos;

    aNode[0]->position.stmtText = aPositionStart->stmtText;
    aNode[0]->position.offset   = aPositionStart->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size
                                - aPositionStart->offset;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_FUNCTION_MODULE_NOT_FOUND,
                                  sBuffer ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC qtc::makeNode( qcStatement*    aStatement,
                      qtcNode**       aNode,
                      qcNamePosition* aPosition,
                      mtfModule*      aModule )
{
/***********************************************************************
 *
 * Description :
 *
 *    Module ����(aModule)�κ��� Node�� ������.
 *
 * Implementation :
 *
 *    �־��� Module ����(aModule)�� �̿��Ͽ� Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Node ����
    IDU_LIMITPOINT("qtc::makeNode::malloc2");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag  = aModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->node.module = aModule;

    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode[0],
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               aModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
              != IDE_SUCCESS );
    
    // set base table and column ID
    aNode[0]->node.baseTable = aNode[0]->node.table;
    aNode[0]->node.baseColumn = aNode[0]->node.column;
    
    aNode[0]->position            = *aPosition;
    aNode[0]->columnName          = *aPosition;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qtc::addArgument( qtcNode** aNode,
                         qtcNode** aArgument )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Node(aNode)�� Argument�� �߰���.
 *
 * Implementation :
 *
 *    �ش� Node�� Argument�� �߰��Ѵ�.
 *    Argument�� ���� ���� ���� Argument�� �̹� �����ϴ� ��츦
 *    �����Ͽ� ó���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::addArgument"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcNamePosition sPosition;

    IDE_TEST_RAISE( ( aNode[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK )
                    == MTC_NODE_ARGUMENT_COUNT_MAXIMUM,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    if( aNode[1] == NULL )
    {
        aNode[0]->node.arguments = (mtcNode*)aArgument[0];
    }
    else
    {
        aNode[1]->node.next = (mtcNode*)aArgument[0];
    }
    aNode[0]->node.lflag++;
    aNode[1] = aArgument[0];

    if( aNode[0]->position.offset < aNode[1]->position.offset )
    {
        sPosition.stmtText = aNode[0]->position.stmtText;
        sPosition.offset   = aNode[0]->position.offset;
        sPosition.size     = aNode[1]->position.offset + aNode[1]->position.size
                           - sPosition.offset;
    }
    else
    {
        sPosition.stmtText = aNode[1]->position.stmtText;
        sPosition.offset   = aNode[1]->position.offset;
        sPosition.size     = aNode[0]->position.offset + aNode[0]->position.size
                           - sPosition.offset;
    }
    aNode[0]->position = sPosition;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::addWithinArguments( qcStatement  * aStatement,
                                qtcNode     ** aNode,
                                qtcNode     ** aArguments )
{
/***********************************************************************
 *
 * Description : PROJ-2527 WITHIN GROUP AGGR
 *
 *    �ش� Node(aNode)�� Function Arguments�� �߰���.
 *
 *    BUG-41771 �ʵ� ����
 *      ����(2015.06.15) PERCENT_RANK �Լ���
 *                      Aggregation (WITHIN GROUP) �Լ��� �ְ�,
 *                      Analytic (OVER) �Լ��� ����.
 *
 *      PERCENT_RANK() OVER(..) �Լ��� �߰��Ѵٸ�,
 *         PERCENT_RANK() OVER(..)           --> �Լ��� : "PERCENT_RANK"
 *                                               ���� : mtfPercentRank
 *         PERCENT_RANK(..) WITHIN GROUP(..) --> �Լ��� : "PERCENT_RANK_WITHIN_GROUP"
 *                                               ���� : mtfPercentRankWithinGroup
 *        �̷��� �۾��ϰ�, �Ʒ��� �ڵ� ��,
 *
 *        ( aNode[0]->node.module != &mtfPercentRankWithinGroup ), // BUG-41771
 *        -->
 *        ( aNode[0]->node.module != &mtfPercentRank ),
 *
 *        �� �����Ѵ�.
 *
 *      ( qtc::changeWithinGroupNode �ڵ嵵 �ʵ� )
 *
 *    BUG-41800
 *      ����(2015.06.15) CUME_DIST �Լ���
 *                      Aggregation (WITHIN GROUP) �Լ��� �ְ�,
 *                      Analytic (OVER) �Լ��� ����.
 *
 *      CUME_DIST() OVER(..) �Լ��� �߰��Ѵٸ�,
 *         CUME_DIST() OVER(..)           --> �Լ��� : "CUME_DIST"
 *                                            ���� : mtfCumeDist
 *         CUME_DIST(..) WITHIN GROUP(..) --> �Լ��� : "CUME_DIST_WITHIN_GROUP"
 *                                            ���� : mtfCumeDistWithinGroup
 *        �̷��� �۾��ϰ�, �Ʒ��� �ڵ� ��,
 *
 *        ( aNode[0]->node.module != &mtfCumeDistWithinGroup ), // BUG-417800
 *        -->
 *        ( aNode[0]->node.module != &mtfCumeDist ),
 *
 *        �� �����Ѵ�.
 *
 *      ( qtc::changeWithinGroupNode �ڵ嵵 �ʵ� )
 *
 * Implementation :
 *
 *    aNode�� within group ���ڸ� funcArguments�� �߰��ϰ�
 *    aNode->node.arguments�� ���� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sCopyNode;
    qtcNode   * sNode;
    UInt        sCount = 0;

    IDE_TEST_RAISE( ( aNode[0]->node.module != &mtfListagg ) &&
                    ( aNode[0]->node.module != &mtfPercentileCont ) &&
                    ( aNode[0]->node.module != &mtfPercentileDisc ) &&
                    ( aNode[0]->node.module != &mtfRank ) &&                   // BUG-41631
                    ( aNode[0]->node.module != &mtfPercentRankWithinGroup ) && // BUG-41771
                    ( aNode[0]->node.module != &mtfCumeDistWithinGroup ),      // BUG-41800
                    ERR_NOT_ALLOWED_WITHIN_GROUP );
    
    if ( aArguments[0]->node.module != NULL )
    {
        // one expression
        aNode[0]->node.funcArguments = &(aArguments[0]->node);
        sCount = 1;
    }
    else
    {
        // list expression
        aNode[0]->node.funcArguments = aArguments[0]->node.arguments;
        sCount = ( aArguments[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );
    }
    
    // �ִ� ������ ���� �� ����.
    IDE_TEST_RAISE( ( aNode[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) + sCount >
                    MTC_NODE_ARGUMENT_COUNT_MAXIMUM,
                    ERR_INVALID_FUNCTION_ARGUMENT );
    
    // funcArguments�� node.lflag�� �����ϱ� ���� �����Ѵ�.
    IDE_TEST( copyNodeTree( aStatement,
                            (qtcNode*) aNode[0]->node.funcArguments,
                            & sCopyNode,
                            ID_TRUE,
                            ID_FALSE )
              != IDE_SUCCESS );

    // arguments�� �ٿ��ִ´�.
    if ( aNode[0]->node.arguments == NULL )
    {
        aNode[0]->node.arguments = (mtcNode*) sCopyNode;
    }
    else
    {
        for ( sNode = (qtcNode*) aNode[0]->node.arguments;
              sNode->node.next != NULL;
              sNode = (qtcNode*) sNode->node.next );

        sNode->node.next = (mtcNode*) sCopyNode;
    }

    //argument count�� ����
    aNode[0]->node.lflag += sCount;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOWED_WITHIN_GROUP );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_NOT_ALLOWED_WITHIN_GROUP ) );

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_FUNCTION_ARGUMENT ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::modifyQuantifiedExpression( qcStatement* aStatement,
                                        qtcNode**    aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Parsing �ܰ迡��
 *    Quantified Expression ó���� ���� ������ �׿� Node�� �����Ѵ�.
 *
 * Implementation :
 *
 *    Parsing�� ���� ������ �׿� Node�� ������ ���, �̸� �����Ѵ�.
 *
 *    ex) i1 in ( select a1 ... )
 *
 *          [IN]                          [IN]
 *           |                             |
 *           V                             V
 *          [i1] --> [�׿� Node]    ==>   [i1] --> [subquery]
 *                       |
 *                       V
 *                    [subquery]
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::modifyQuantifiedExpression"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode*  sArgument1;
    qtcNode*  sArgument2;
    qtcNode*  sArgument;

    sArgument1 = (qtcNode*)aNode[0]->node.arguments;
    sArgument2 = (qtcNode*)sArgument1->node.next;

    if( ( sArgument2->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 1 &&
        sArgument2->node.module == &mtfList )
    {
        sArgument = (qtcNode*)sArgument2->node.arguments;
        if( sArgument->node.module              == &subqueryModule ||
            ( sArgument->node.module              == &mtfList      &&
              ( sArgument1->node.module           != &mtfList      ||
                sArgument->node.arguments->module == &mtfList ) ) )
        {
            sArgument1->node.next   = (mtcNode*)&sArgument->node;
            aNode[1]                = sArgument;

            IDE_TEST( mtc::initializeColumn(
                          QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sArgument2->node.table].columns +
                          sArgument2->node.column,
                          & qtc::skipModule,
                          0,
                          0,
                          0 )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeValue( qcStatement*    aStatement,
                       qtcNode**       aNode,
                       const UChar*    aTypeName,
                       UInt            aTypeLength,
                       qcNamePosition* aPosition,
                       const UChar*    aValue,
                       UInt            aValueLength,
                       mtcLiteralType  aLiteralType )
{
/***********************************************************************
 *
 * Description :
 *
 *    Value�� ���� Node�� ������.
 *
 * Implementation :
 *
 *   Data Type ����(aTypeName)�� Value ����(aValue)�� �̿��Ͽ�
 *   Value Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtdModule* sModule;
    IDE_RC           sResult;
    mtcTemplate    * sMtcTemplate;
    mtcColumn      * sColumn;
    UInt             sMtcColumnFlag = 0;

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    // Node ����
    IDU_LIMITPOINT("qtc::makeValue::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag          = valueModule.lflag
                                  & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->position            = *aPosition;
    aNode[0]->columnName          = *aPosition;
    aNode[0]->node.module         = &valueModule;

    IDE_TEST( mtd::moduleByName( &sModule, (void*)aTypeName, aTypeLength )
              != IDE_SUCCESS );

    sResult = qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode[0],
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_CONSTANT,
                               valueModule.lflag & MTC_NODE_COLUMN_COUNT_MASK );

    // PROJ-1579 NCHAR
    // ���ͷ��� ������ ���� �Ʒ��� ���� �з��Ѵ�.
    // NCHAR ���ͷ�    : N'��'
    // �����ڵ� ���ͷ� : U'\C548'
    // �Ϲ� ���ͷ�     : 'ABC' or CHAR'ABC'
    if( aLiteralType == MTC_COLUMN_NCHAR_LITERAL )
    {
        sMtcColumnFlag |= MTC_COLUMN_LITERAL_TYPE_NCHAR;
    }
    else if( aLiteralType == MTC_COLUMN_UNICODE_LITERAL )
    {
        sMtcColumnFlag |= MTC_COLUMN_LITERAL_TYPE_UNICODE;
    }
    else
    {
        // mtcColumn.flag = 0���� �ʱ�ȭ
        sMtcColumnFlag = MTC_COLUMN_LITERAL_TYPE_NORMAL;
    }

    if( sResult == IDE_SUCCESS )
    {
        // To Fix BUG-12607
        sColumn = sMtcTemplate->rows[aNode[0]->node.table].columns
            + sMtcTemplate->rows[aNode[0]->node.table].columnCount - 1;


        IDE_TEST( mtc::initializeColumn(
                      sColumn,
                      sModule,
                      0,
                      0,
                      0 )
                  != IDE_SUCCESS );

        // PROJ-1579 NCHAR
        // �Ľ� �ܰ迡�� LITERAL_TYPE_NCHAR�� LITERAL_TYPE_UNICODE��
        // ������ �� �ִ�.
        sColumn->flag |= sMtcColumnFlag;

        // To Fix BUG-12925
        IDE_TEST(
            sColumn->module->value(
                sMtcTemplate,
                sMtcTemplate->rows[aNode[0]->node.table].columns
                + sMtcTemplate->rows[aNode[0]->node.table].columnCount - 1,
                sMtcTemplate->rows[aNode[0]->node.table].row,
                & sMtcTemplate->rows[aNode[0]->node.table].rowOffset,
                sMtcTemplate->rows[aNode[0]->node.table].rowMaximum,
                (const void*) aValue,
                aValueLength,
                & sResult )
            != IDE_SUCCESS );
    }

    //---------------------------------------------------------
    // sResult != IDE_SUCCESS�� ���� �� ��찡 �ִ�.
    // qtc::nextColumn()���� Next Column�� �Ҵ���� ���� ���,
    // sModule->value() ȣ�� �� ���� row ������ ������ ���
    //---------------------------------------------------------
    if( sResult != IDE_SUCCESS )
    {
        IDE_TEST( qtc::nextRow( QC_QMP_MEM(aStatement),
                                aStatement,
                                QC_SHARED_TMPLATE(aStatement),
                                MTC_TUPLE_TYPE_CONSTANT )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_CONSTANT,
                                   valueModule.lflag &
                                   MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        // To Fix BUG-12607
        sColumn = sMtcTemplate->rows[aNode[0]->node.table].columns
            + sMtcTemplate->rows[aNode[0]->node.table].columnCount - 1;

        IDE_TEST( mtc::initializeColumn(
                      sColumn,
                      sModule,
                      0,
                      0,
                      0 )
                  != IDE_SUCCESS );

        // PROJ-1579 NCHAR
        // �Ľ� �ܰ迡�� LITERAL_TYPE_NCHAR�� LITERAL_TYPE_UNICODE��
        // ������ �� �ִ�.
        sColumn->flag |= sMtcColumnFlag;

        // To Fix BUG-12925
        IDE_TEST(
            sColumn->module->value(
                sMtcTemplate,
                sMtcTemplate->rows[aNode[0]->node.table].columns
                + sMtcTemplate->rows[aNode[0]->node.table].columnCount - 1,
                sMtcTemplate->rows[aNode[0]->node.table].row,
                & sMtcTemplate->rows[aNode[0]->node.table].rowOffset,
                sMtcTemplate->rows[aNode[0]->node.table].rowMaximum,
                (const void*) aValue,
                aValueLength,
                & sResult )
            != IDE_SUCCESS );

        IDE_TEST_RAISE( sResult != IDE_SUCCESS, ERR_INVALID_LITERAL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LITERAL );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LITERAL));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeNullValue( qcStatement     * aStatement,
                           qtcNode        ** aNode,
                           qcNamePosition  * aPosition )
{
/***********************************************************************
 *
 * Description : BUG-38952 type null
 *    NULL Value�� ���� Node�� ������.
 *
 * Implementation :
 *    TYPE_NULL property�� ���� null type�� NULL�� �����ϰų�
 *    varchar type�� NULL�� �����Ѵ�.
 *
 ***********************************************************************/

    if ( QCU_TYPE_NULL == 0 )
    {
        IDE_TEST( makeValue( aStatement,
                             aNode,
                             (const UChar*)"NULL",
                             4,
                             aPosition,
                             (UChar*)( aPosition->stmtText + aPosition->offset ),
                             aPosition->size,
                             MTC_COLUMN_NORMAL_LITERAL )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( makeValue( aStatement,
                             aNode,
                             (const UChar*)"VARCHAR",
                             7,
                             aPosition,
                             (UChar*)( aPosition->stmtText + aPosition->offset ),
                             0,
                             MTC_COLUMN_NORMAL_LITERAL )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::makeProcVariable( qcStatement*     aStatement,
                              qtcNode**        aNode,
                              qcNamePosition*  aPosition,
                              mtcColumn *      aColumn,
                              UInt             aProcVarOp )
{
/***********************************************************************
 *
 * Description :
 *
 *    Procedure Variable�� ���� Node�� ������.
 *
 * Implementation :
 *
 *    Column ����(aColumn)�� �̿��Ͽ� Procedure Variable�� ����
 *    Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeProcVariable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcTemplate       * sQcTemplate;
    mtcTemplate      * sMtcTemplate;
    mtcTuple         * sMtcTuple;
    UShort             sCurrRowID;
    UShort             sColumnIndex;
    UInt               sOffset;

    // Node ����
    IDU_LIMITPOINT("qtc::makeProcVariable::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag          = columnModule.lflag
                                  & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->position            = *aPosition;
    aNode[0]->columnName          = *aPosition;
    aNode[0]->node.module         = &columnModule;
    aNode[0]->node.table          = ID_USHORT_MAX;
    aNode[0]->node.column         = ID_USHORT_MAX;

    sQcTemplate = QC_SHARED_TMPLATE(aStatement);
    sMtcTemplate = & sQcTemplate->tmplate;

    if ( aProcVarOp & QTC_PROC_VAR_OP_NEXT_COLUMN )
    {
        /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ����
         * Intermediate Tuple Row�� �ְ� ��� ���� ���� ���¿���,
         * Intermediate Tuple Row�� Lob Column�� �Ҵ��� ��,
         * (Old Offset + New Size) > Property �̸�,
         * ���ο� Intermediate Tuple Row�� �Ҵ��Ѵ�.
         */
        if ( ( sQcTemplate->tmplate.currentRow[MTC_TUPLE_TYPE_INTERMEDIATE] != ID_USHORT_MAX ) &&
             ( aColumn != NULL ) )
        {
            sCurrRowID = sQcTemplate->tmplate.currentRow[MTC_TUPLE_TYPE_INTERMEDIATE];

            sMtcTuple = &(sQcTemplate->tmplate.rows[sCurrRowID]);
            if ( sMtcTuple->columnCount != 0 )
            {
                if ( ( aColumn->type.dataTypeId == MTD_BLOB_ID ) ||
                     ( aColumn->type.dataTypeId == MTD_CLOB_ID ) )
                {
                    for( sColumnIndex = 0, sOffset = 0;
                         sColumnIndex < sMtcTuple->columnCount;
                         sColumnIndex++ )
                    {
                        if ( sMtcTuple->columns[sColumnIndex].module != NULL )
                        {
                            sOffset = idlOS::align( sOffset,
                                                    sMtcTuple->columns[sColumnIndex].module->align );
                            sOffset += sMtcTuple->columns[sColumnIndex].column.size;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }

                    if ( (sOffset + (UInt)aColumn->column.size ) > QCU_INTERMEDIATE_TUPLE_LOB_OBJECT_LIMIT )
                    {
                        IDE_TEST( qtc::nextRow( QC_QMP_MEM(aStatement),
                                                aStatement,
                                                sQcTemplate,
                                                MTC_TUPLE_TYPE_INTERMEDIATE )
                                  != IDE_SUCCESS );
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
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   sQcTemplate,
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   valueModule.lflag &
                                   MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        // for rule_data_type in qcply.y
        if ( aColumn != NULL )
        {
            sMtcTemplate->rows[aNode[0]->node.table]
                .columns[ aNode[0]->node.column ] = *aColumn ;
        }

        if ( aProcVarOp & QTC_PROC_VAR_OP_SKIP_MODULE )
        {
            IDE_TEST( mtc::initializeColumn(
                          sMtcTemplate->rows[aNode[0]->node.table].columns
                          + aNode[0]->node.column,
                          & qtc::skipModule,
                          0,
                          0,
                          0 )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // no column is
        IDE_DASSERT( aColumn == NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeInternalProcVariable( qcStatement*    aStatement,
                                      qtcNode**       aNode,
                                      qcNamePosition* aPosition,
                                      mtcColumn *     aColumn,
                                      UInt            aProcVarOp
    )
{
/***********************************************************************
 *
 * Description :
 *    For PR-11391
 *    Internal Procedure Variable�� ���� Node�� ������.
 *    Internal Procedure Variable�� table�� column���� procedure variable����
 *    �˻��ϸ� �ȵ�.
 * Implementation :
 *
 *    Column ����(aColumn)�� �̿��Ͽ� Procedure Variable�� ����
 *    Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeInternalProcVariable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcTemplate * sMtcTemplate;

    // Node ����
    IDU_LIMITPOINT("qtc::makeInternalProcVariable::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag          = columnModule.lflag
                                  & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->lflag               = QTC_NODE_INTERNAL_PROC_VAR_EXIST;

    if( aPosition == NULL )
    {
        SET_EMPTY_POSITION(aNode[0]->position);
        SET_EMPTY_POSITION(aNode[0]->columnName);
    }
    else
    {
        aNode[0]->position            = *aPosition;
        aNode[0]->columnName          = *aPosition;
    }

    aNode[0]->node.module         = &columnModule;

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    if ( aProcVarOp & QTC_PROC_VAR_OP_NEXT_COLUMN )
    {
        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   valueModule.lflag &
                                   MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );

        // for rule_data_type in qcply.y
        if ( aColumn != NULL )
        {
            sMtcTemplate->rows[aNode[0]->node.table]
                .columns[ aNode[0]->node.column ] = *aColumn ;
        }

        if ( aProcVarOp & QTC_PROC_VAR_OP_SKIP_MODULE )
        {
            IDE_TEST( mtc::initializeColumn(
                          sMtcTemplate->rows[aNode[0]->node.table].columns
                          + aNode[0]->node.column,
                          & qtc::skipModule,
                          0,
                          0,
                          0 )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // no column is
        IDE_DASSERT( aColumn == NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeVariable( qcStatement*    aStatement,
                          qtcNode**       aNode,
                          qcNamePosition* aPosition )
{
/***********************************************************************
 *
 * Description :
 *
 *    Host Variable�� ���� Node�� ������.
 *
 * Implementation :
 *
 *    Host ������ �������� Setting�ϰ�,
 *    Value Module�� ������ Module�� Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeVariable"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcStmtListMgr     * sStmtListMgr;
    qcNamedVarNode    * sNamedVarNode;
    qcNamedVarNode    * sTailVarNode;
    idBool              sIsFound = ID_FALSE;
    idBool              sIsSameVar = ID_FALSE;
    UInt                sHostVarCnt;    
    UInt                sHostVarIdx;
    qcBindNode        * sBindNode;
    qtcNode           * sNode;

    IDE_ASSERT( aStatement->myPlan->stmtListMgr != NULL );

    sStmtListMgr = aStatement->myPlan->stmtListMgr;

    // Node ����
    IDU_LIMITPOINT("qtc::makeVariable::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag          = ( valueModule.lflag
                                  | MTC_NODE_BIND_EXIST )
                                  & ~MTC_NODE_COLUMN_COUNT_MASK;
    aNode[0]->position            = *aPosition;
    aNode[0]->columnName          = *aPosition;
    aNode[0]->node.module         = &valueModule;

    sHostVarCnt = qcg::getBindCount( aStatement );
    
    // PROJ-2415 Grouping Sets Clause
    for ( sHostVarIdx = sStmtListMgr->mHostVarOffset; /* TASK-7219 */
          sHostVarIdx < sHostVarCnt;
          sHostVarIdx++ )
    {
        if ( sStmtListMgr->hostVarOffset[ sHostVarIdx ] == aPosition->offset )
        {
            // reparsing �Ҷ� �̹� �Ҵ�� column�� �ٽ� ��� �Ѵ�.
            aNode[0]->node.table  = QC_SHARED_TMPLATE(aStatement)->tmplate.variableRow;
            aNode[0]->node.column = sHostVarIdx;

            sIsSameVar = ID_TRUE;

            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sIsSameVar == ID_FALSE )
    {
        // ���� parsing �Ҷ�
        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_VARIABLE,
                                   valueModule.lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
        
        aStatement->myPlan->stmtListMgr->hostVarOffset[ aNode[0]->node.column ] =
            aPosition->offset;
        aStatement->myPlan->stmtListMgr->mHostVarNode[ aNode[0]->node.column ] =
            aNode[0]; /* TASK-7219 */

        IDE_DASSERT( aNode[0]->node.column == sHostVarCnt );
    }
    else
    {
        aStatement->myPlan->stmtListMgr->mHostVarNode[ aNode[0]->node.column ] =
            aNode[0]; /* TASK-7219 */
    }

    // BUG-36986
    // PSM �� ���� parsing �������� variable �� ���� qtcNode ������
    // �ߺ�ȸ�Ǹ� ���� table, column position �� ������ ������ �����Ѵ�.
    // BUGBUG : �Ϲ������� CLI, JDBC, ODBC ���
    //          name base biding �� �����ϰ� �Ǹ� �����ؾ� ��

    // reparsing �� �� column position �� �������̹Ƿ�
    // makeVariable �� ���� �� QCP_SET_HOST_VAR_OFFSET �� ���ڷ� ���� ����
    // ������ column position �� baseColumn �� copy �Ѵ�.
    aNode[0]->node.baseColumn = aNode[0]->node.column;

    if( aStatement->calledByPSMFlag == ID_TRUE )
    {
        // BUG-41248 DBMS_SQL package
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qcBindNode,
                                &sBindNode )
                  != IDE_SUCCESS );
 
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qtcNode,
                                &sNode )
                  != IDE_SUCCESS);

        *sNode = *aNode[0];

        sBindNode->mVarNode = sNode;

        sBindNode->mNext = aStatement->myPlan->bindNode;

        aStatement->myPlan->bindNode = sBindNode;
 
        if( aStatement->namedVarNode == NULL )
        {
            // ���ʻ���
            IDE_TEST( STRUCT_ALLOC( QC_QME_MEM( aStatement ),
                                    qcNamedVarNode,
                                    &aStatement->namedVarNode )
                      != IDE_SUCCESS );

            aStatement->namedVarNode->varNode = aNode[0];
            aStatement->namedVarNode->next    = NULL;
        }
        else
        {
            // ������ ��ϵ� variable �˻�
            for( sNamedVarNode = aStatement->namedVarNode;
                 sNamedVarNode != NULL;
                 sNamedVarNode = sNamedVarNode->next )
            {
                sTailVarNode = sNamedVarNode;

                if( ( idlOS::strMatch( aNode[0]->columnName.stmtText + aNode[0]->columnName.offset,
                                       aNode[0]->columnName.size,
                                       "?", 1 ) != 0 )
                    &&
                    ( idlOS::strMatch( aNode[0]->columnName.stmtText + aNode[0]->columnName.offset,
                                       aNode[0]->columnName.size,
                                       sNamedVarNode->varNode->columnName.stmtText
                                         + sNamedVarNode->varNode->columnName.offset,
                                       sNamedVarNode->varNode->columnName.size ) == 0 ) )

                {
                    // Same host variable found.
                    sIsFound = ID_TRUE;
                    break;
                }
            }

            if( sIsFound == ID_TRUE )
            {
                // ������ host variable �� table, column position set
                aNode[0]->node.table  = sNamedVarNode->varNode->node.table;
                aNode[0]->node.column = sNamedVarNode->varNode->node.column;
            }
            else
            {
                // namedVarNode �� �߰�
                IDE_TEST( STRUCT_ALLOC( QC_QME_MEM( aStatement ),
                                        qcNamedVarNode,
                                        &sNamedVarNode )
                          != IDE_SUCCESS );

                sNamedVarNode->varNode = aNode[0];
                sNamedVarNode->next    = NULL;
                sTailVarNode->next     = sNamedVarNode;
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC qtc::makeColumn( qcStatement*    aStatement,
                        qtcNode**       aNode,
                        qcNamePosition* aUserPosition,
                        qcNamePosition* aTablePosition,
                        qcNamePosition* aColumnPosition,
                        qcNamePosition* aPkgPosition )
{
/***********************************************************************
 *
 * Description :
 *
 *    Column�� ���� Node�� ������.
 *
 * Implementation :
 *
 *    User Name, Table Name�� ���� ������ ���� ������ �˸���
 *    ������ �����ϰ�, Column Module�� ������ ����� setting�Ѵ�.
 *
 ***********************************************************************/

    // Node ����
    IDU_LIMITPOINT("qtc::makeColumn::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
             != IDE_SUCCESS);

    // Node �ʱ�ȭ
    QTC_NODE_INIT( aNode[0] );
    aNode[1] = NULL;

    // Node ���� ����
    aNode[0]->node.lflag          = columnModule.lflag
                                  & ~MTC_NODE_COLUMN_COUNT_MASK;

    // PROJ-1073 Package
    if( ( aUserPosition != NULL ) &&
        ( aTablePosition != NULL ) &&
        ( aColumnPosition != NULL ) &&
        ( aPkgPosition != NULL ) )
    {
        // ----------------------------------
        // | USER | TABLE | COLUMN | PACKAGE
        // ----------------------------------
        // |  O   |   O   |   O    |   O  
        // ----------------------------------
        aNode[0]->position.stmtText = aUserPosition->stmtText;
        aNode[0]->position.offset   = aUserPosition->offset;
        aNode[0]->position.size     = aPkgPosition->offset
                                    + aPkgPosition->size
                                    - aUserPosition->offset;
        aNode[0]->userName          = *aUserPosition;
        aNode[0]->tableName         = *aTablePosition;
        aNode[0]->columnName        = *aColumnPosition;
        aNode[0]->pkgName           = *aPkgPosition;
    }
    // fix BUG-19185
    else if( (aUserPosition != NULL) &&
             (aTablePosition != NULL) &&
             (aColumnPosition != NULL) )
    {
        // ----------------------------------
        // | USER | TABLE | COLUMN | PACKAGE
        // ----------------------------------
        // |  O   |   O   |   O    |    X
        // ----------------------------------
        aNode[0]->position.stmtText = aUserPosition->stmtText;
        aNode[0]->position.offset   = aUserPosition->offset;
        aNode[0]->position.size     = aColumnPosition->offset
                                    + aColumnPosition->size
                                    - aUserPosition->offset;
        aNode[0]->userName          = *aUserPosition;
        aNode[0]->tableName         = *aTablePosition;
        aNode[0]->columnName        = *aColumnPosition;
    }
    else if( (aTablePosition != NULL) && (aColumnPosition != NULL)  )
    {
        // ----------------------------------
        // | USER | TABLE | COLUMN | PACKAGE 
        // ----------------------------------
        // |  X   |   O   |   O    |   X
        // ----------------------------------
        aNode[0]->position.stmtText = aTablePosition->stmtText;
        aNode[0]->position.offset   = aTablePosition->offset;
        aNode[0]->position.size     = aColumnPosition->offset
                                    + aColumnPosition->size
                                    - aTablePosition->offset;
        aNode[0]->tableName         = *aTablePosition;
        aNode[0]->columnName        = *aColumnPosition;
    }
    else if ( aColumnPosition != NULL )
    {
        // ----------------------------------
        // | USER | TABLE | COLUMN | PACKAGE
        // ----------------------------------
        // |  X   |   X   |   O    |   X
        // ----------------------------------
        aNode[0]->position          = *aColumnPosition;
        aNode[0]->columnName        = *aColumnPosition;
    }
    else
    {
        // nothing to do 
    }

    aNode[0]->node.module = &columnModule;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::makeAssign( qcStatement * aStatement,
                        qtcNode     * aNode,
                        qtcNode     * aArgument )
{
/***********************************************************************
 *
 * Description :
 *    Assign�� ���� Node�� ������.
 *    Indirect�� �޸� aNode�� �ʱ�ȭ���� �ʰ� �״�� �̿��Ѵ�.
 *
 * Implementation :
 *    TODO - �ܺο��� Argument�� ó������ �ʵ��� �����ؾ� ��.
 *    ��, makeIndirect�� ���� argument�� ���� ������ ����ؾ� �ϸ�,
 *    Host ���� binding���� ����Ͽ�,
 *    argument�� node.flag �������� estimateInternal()�� ����
 *    �°��� �� �ֵ��� �ؾ� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeAssign"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // Node ���� ����
    aNode->node.module    = &assignModule;
    aNode->node.arguments = &aArgument->node;
    aNode->node.lflag     = ( assignModule.lflag
                              & ~MTC_NODE_COLUMN_COUNT_MASK ) | 1;
    
    aNode->overClause     = NULL;

    // To Fix BUG-10887
    qtc::dependencySetWithDep( & aNode->depInfo, & aArgument->depInfo );

    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode,
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               1 )
              != IDE_SUCCESS );

    IDE_TEST( qtc::estimateNode( aStatement,
                                 aNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeIndirect( qcStatement* aStatement,
                          qtcNode*     aNode,
                          qtcNode*     aArgument )
{
/***********************************************************************
 *
 * Description :
 *
 *    Indirection�� ���� Node�� ������.
 *    (����, qtcIndirect.cpp)
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeIndirect"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    QTC_NODE_INIT( aNode );

    aNode->node.module    = &indirectModule;
    aNode->node.arguments = &aArgument->node;
    aNode->node.lflag     = ( indirectModule.lflag
                              & ~MTC_NODE_COLUMN_COUNT_MASK ) | 1;

    // To Fix BUG-10887
    qtc::dependencySetWithDep( & aNode->depInfo, & aArgument->depInfo );

    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode,
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               1 )
              != IDE_SUCCESS );

    IDE_TEST( qtc::estimateNode( aStatement,
                                 aNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::makePassNode( qcStatement* aStatement,
                   qtcNode *    aCurrentNode,
                   qtcNode *    aArgumentNode,
                   qtcNode **   aPassNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Pass Node�� ������.
 *    (����, qtcPass.cpp)
 *
 * Implementation :
 *
 *    Current Node�� ������ ���, �� ������ Pass Node�� ��ü��.
 *    Current Node�� ���� ���, ���ο� Pass Node�� ������.
 *
 *    Current Node�� �����ϴ� ���
 *        - SELECT i1 + 1 FROM T1 GROUP BY i1 + 1 HAVING i1 + 1 > ?;
 *                 ^^^^^^                                ^^^^^^
 *        - �ش� i1 + 1�� Pass Node�� ��ü�ϰ� Pass Node�� argument��
 *          GROUP BY i1 + 1�� (i1 + 1)�� ���ϰ� �ȴ�.
 *        - Pass Node�� Conversion�� ������ �� ������, indirection ����
 *          �ʵ��� �ؾ� �Ѵ�.
 *    Current Node �� ���� ���
 *        - SELECT i1 + 1 FROM T1 ORDER BY 1;
 *        - Sorting�� ���� ���� ������ (i1 + 1)�� Argument�� �Ͽ� Pass
 *          Node �� �����ϰ� �ȴ�.
 *        - Pass Node�� Conversion�� �������� ������,
 *          indirection�� �ǵ��� �Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qtc::makePassNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sPassNode;

    if ( aCurrentNode != NULL )
    {
        // Pass Node �⺻ ���� Setting
        aCurrentNode->node.module = &passModule;

        // flag ���� ����
        // ���� ������ �״�� �����ϰ�, pass node�� ������ �߰�
        aCurrentNode->node.lflag &= ~MTC_NODE_COLUMN_COUNT_MASK;
        aCurrentNode->node.lflag |= 1;

        aCurrentNode->node.lflag &= ~MTC_NODE_OPERATOR_MASK;
        aCurrentNode->node.lflag |= MTC_NODE_OPERATOR_MISC;

        // Indirection�� �ƴ��� Setting
        aCurrentNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
        aCurrentNode->node.lflag |= MTC_NODE_INDIRECT_FALSE;

        // PROJ-1473
        aCurrentNode->node.lflag &= ~MTC_NODE_REMOVE_ARGUMENTS_MASK;
        aCurrentNode->node.lflag |= MTC_NODE_REMOVE_ARGUMENTS_TRUE;

        sPassNode = aCurrentNode;
    }
    else
    {
        // ���ο� Pass Node ����
        IDU_LIMITPOINT("qtc::makePassNode::malloc");
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, & sPassNode)
                 != IDE_SUCCESS);

        // Argument Node�� �״�� ����
        idlOS::memcpy( sPassNode, aArgumentNode, ID_SIZEOF(qtcNode) );

        // Pass Node �⺻ ���� Setting
        sPassNode->node.module    = &passModule;

        // flag ���� ����
        // Argument ����� ���� ������ �״�� �����ϰ�,
        // Pass node�� ������ �߰�
        sPassNode->node.lflag &= ~MTC_NODE_COLUMN_COUNT_MASK;
        sPassNode->node.lflag |= 1;

        sPassNode->node.lflag &= ~MTC_NODE_OPERATOR_MASK;
        sPassNode->node.lflag |= MTC_NODE_OPERATOR_MISC;

        // Indirection���� Setting
        sPassNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
        sPassNode->node.lflag |= MTC_NODE_INDIRECT_TRUE;

        sPassNode->node.conversion     = NULL;
        sPassNode->node.leftConversion = NULL;
        sPassNode->node.funcArguments  = NULL;
        sPassNode->node.orgNode        = NULL;
        sPassNode->node.next           = NULL;
        sPassNode->node.cost           = 0;
        sPassNode->subquery            = NULL;
        sPassNode->indexArgument       = 0;
    }

    // ���ο� ID Setting
    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               sPassNode,
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               1 )
              != IDE_SUCCESS );

    sPassNode->node.arguments = (mtcNode*) aArgumentNode;

    // set base table and column ID
    sPassNode->node.baseTable = aArgumentNode->node.baseTable;
    sPassNode->node.baseColumn = aArgumentNode->node.baseColumn;

    sPassNode->overClause = NULL;

    IDE_TEST( qtc::estimateNode( aStatement,
                                 sPassNode ) != IDE_SUCCESS );

    *aPassNode = sPassNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::preProcessConstExpr( qcStatement  * aStatement,
                          qtcNode      * aNode,
                          mtcTemplate  * aTemplate,
                          mtcStack     * aStack,
                          SInt           aRemain,
                          mtcCallBack  * aCallBack )
{
/***********************************************************************
 *
 * Description :
 *    Validation �ܰ迡�� �׻� ������ ���� ���� �Ǵ�
 *    Constant Expression�� ���Ͽ� �̸� �����Ѵ�.
 *
 *    �̷��� ó���� ������ ���� ������ ��� ���ؼ��̴�.
 *        1.  Expression�� �ݺ� ���� ����
 *            ex) i1 = 1 + 1
 *                ^^^^^^^^^^
 *          ���� ����� �����Կ��� �ұ��ϰ� ��� �ݺ� ����Ǵ� ����
 *          �����ϱ� �����̴�.
 *        2.  Selectivity ����
 *            ex) double1 > 3
 *                         ^^
 *           ���� ���� Column Type�� double�̰� Value Type�� integer��
 *           ��� 3���� double������ ��ȯ�Ǿ�� selectivity�� ����� ��
 *           �ִ�.  �̷��� predicate�� selectivity�� �����ϱ� ���� Value
 *           ������ ���� ���� �̸� ��������μ� �� ȿ���� ���� �� �ִ�.
 *        3.  Fixed Key Range ��� ȿ���� ����
 *            ex) double1 = 3 + 4
 *                          ^^^^^
 *            ���� ���� ���������� Value������ ������ �ʿ��ϰ�,
 *            ���� conversion�Ǿ�� �ϱ� ������
 *            Key Range�� �̸� ������ �� ����.  �׷���, �̴� ������ �����
 *            Conversion ����� �׻� �����ϱ� ������ Fixed Key Range��
 *            ������ �� �ְ� �ȴ�.
 *
 *    �̷��� ó���� �����ϱ� ���ؼ��� ������ ���� ������ �ذ��Ͽ��� �Ѵ�.
 *
 *        - �Ϲ� ����� Tuple Set�� [Constant ����]�� ����ȴ�.  �� ������
 *          ���� ������ �ʴ� �����̱� ������ ���� �����ϱ� ���� �޸� ������
 *          �Ҵ�Ǿ� �ִ�.
 *          �׷���, ���� �Ǵ�  Conversion�� �� ����� �������̸� Data Type
 *          ���� �������� �� �����Ƿ� [Intermediate ����]���� �����Ǹ�,
 *          ���� ���� �޸𸮸� Execution ������ �Ҵ�ް� �ȴ�.
 *          ��, �׻� ���� ����� �����ϴ� �����̶� �� ���� ���� ������
 *          ������ ���� ������ �̸� ������ �� ���� �ȴ�.
 *        - ����, Constant Expression�� ���� ��ó���� ���ؼ���
 *          Intermediate �������� �����Ǵ� ������ Constant ��������
 *          �̵����Ѿ� �ϸ�, ������ ����� Node���� ���� ������ ǥ��,
 *          ������ �Ӽ� ����(�Ϲ� �����)�� ����Ͽ��� �Ѵ�.
 *
 * Implementation :
 *     Constant Expression�� ��ó���� ������ ����
 *        - ȣ��Ʈ ������ ����� �Ѵ�.
 *            * i1 = 1 + ?
 *                  ^^^^^^
 *              ���� ���� Host ������ �ִ� ��� �� ���� ����� ������ �� ����.
 *            * i1 + ? = 1 + 1
 *                       ^^^^^
 *              ���� ����� ����������, ������ ȣ��Ʈ ������ ���� ������
 *              Ÿ���� ���� ���� �ִ�.
 *        - ���� �� Conversion�� �߻��ؾ� �Ѵ�.
 *            * i1 = 1
 *                   ^^
 *              �̹� [Constant ����]�� �����ϸ� ó���� ������ ����.
 *        - Aggregation�� ����� �Ѵ�.
 *            * i1 = 1 + SUM(1)
 *                   ^^^^^^^^^^
 *              ��� ���·� ���̳�, Record�� ������ ���� ����� �޶�����.
 *        - Subquery�� ����� �Ѵ�.
 *            * i1 = 1 + (select sum(a1) from t2 );
 *                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *              Dependencies�� �������� �ʾ� ���ó�� �Ǵܵ� �� �ִ�.
 *        - ����θ� �����Ǿ�� �Ѵ�.
 *            * i1 = 1 + 1
 *        - PRIOR Column�� ����� �Ѵ�.
 *            * i1 = prior i2 + 1
 *                   ^^^^^^^^^^^^
 *              Dependencies�� ���� ���ó�� ���̳�, Record�� ��ȭ�� ����
 *              ����� �ٲ��.
 *
 *    ó�� ����
 *
 *        - ���� ��尡 �ƴ� Argument�� ���ؼ��� ó���Ѵ�.
 *            i1 + ? = 1 + 1
 *                     ^^^^^
 *            ��, (1 + 1)�� ���� ó���� (+) ��尡 �ƴ� (=) ��忡��
 *            ó���Ǿ�� �Ѵ�.
 *        - ó�� ���� ���θ� �Ǵ��Ѵ�.
 *            - ���� ��带 �������� ������ �Ǵ��Ѵ�.
 *                - Host ������ �������� �ʾƾ� �Ѵ�.
 *                - Argument�� �־�� �Ѵ�.
 *                - subquery�� �ƴϾ�� �Ѵ�.
 *                - List�� �ƴϾ�� �Ѵ�.
 *            - ���� ���� Argument�� �������� ������ �Ǵ��Ѵ�.
 *                - dependecies�� zero�̾�� �Ѵ�.
 *                - argument�� conversion�� �־�� �Ѵ�.
 *                - prior�� ����� �Ѵ�.
 *                - aggregation�� ����� �Ѵ�.
 *                - subquery�� ����� �Ѵ�.
 *                - list�� ���� ó���� ����ؾ� �Ѵ�.
 *        - �� ������ ������ ��� �ش� argument ��带 ����
 *          Constant ������ �Ҵ��Ѵ�.
 *            - Argument�� �ִٸ� �ڽ��� ���� ���� �Ҵ�
 *            - Conversion�� �ִٸ� ��� Conversion�� ���� ���� �Ҵ�
 *              double1 = 1 + 1
 *                        ^^^^^
 *              [+]--conv-->[bigint=>double]--conv-->[int=>bigint]
 *               |
 *               V
 *              [1]-->[1]
 *
 *              ���� ���� ��� �ڽ��� ���� ���� 1, Conversion�� ���� ���� 2��,
 *              �� 3���� ������ �Ҵ�޴´�.
 *
 *        - Argument ����� ������ �����Ѵ�.
 *             ������ ���������μ� Constant ������ ����� ��ϵȴ�.
 *        - Node�� ���� ���踦 ����
 *
 *             [=]
 *              |
 *              V            |-conv-->[B=>D]--conv-->[I=>B]
 *             [double1]--->[+]
 *                           |
 *                           V
 *                          [1]-->[1]
 *
 *                               ||
 *                              \  /
 *                               \/
 *
 *             [=]
 *              |
 *              V
 *             [double1]-->[B=>D]
 *
 *         - Value ���� ��ȯ
 *             ���� ���迡 ���� ���õ� Node�� Value Module�� ��ȯ�Ѵ�.
 *
 *   ���� ���� ó�� ������ ::estimateInternal()�� ����
 *   ���� ���� �ö󰡸鼭 ó���ǰ� �ȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qtc::preProcessConstExpr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sCurNode;
    qtcNode * sPrevNode;
    qtcNode * sListCurNode;
    qtcNode * sListPrevNode;
    qtcNode * sResultNode;
    qtcNode * sOrgNode;

    mtcStack * sStack;
    SInt       sRemain;
    mtcStack * sListStack;
    SInt       sListRemain;

    idBool     sAbleProcess;

    //-------------------------------------------
    // ���� ��带 �̿��� ���ռ� �˻�
    //-------------------------------------------

    if ( (aNode->subquery == NULL) &&
         (aNode->node.arguments != NULL) &&
         (aNode->node.module != &mtfList) )
    {
        //-------------------------------------------
        // �� Argument�� ���� Pre-Processing ó��
        //-------------------------------------------

        for ( sCurNode = (qtcNode*) aNode->node.arguments, sPrevNode = NULL,
                  sStack = aStack + 1, sRemain = aRemain -1;
              sCurNode != NULL;
              sCurNode = (qtcNode*) sCurNode->node.next, sStack++, sRemain-- )
        {
            // BUG-40892 host ������ ������ runConstExpr �� �������� ����
            // host ������ üũ�ϴ� ��ġ�� �����Ͽ� ������ runConstExpr�� ������
            if( MTC_NODE_IS_DEFINED_VALUE( (mtcNode*)sCurNode ) == ID_TRUE )
            {
                // Argument�� ���ռ� �˻�
                IDE_TEST( isConstExpr( QC_SHARED_TMPLATE(aStatement),
                                       sCurNode,
                                       &sAbleProcess )
                          != IDE_SUCCESS );
            }
            else
            {
                sAbleProcess = ID_FALSE;
            }

            if ( sAbleProcess == ID_TRUE )
            {
                if ( sCurNode->node.module == & mtfList )
                {
                    // List�� ���
                    // List�� argument�� ���Ͽ� ��ó���� ������.
                    for ( sListCurNode = (qtcNode*) sCurNode->node.arguments,
                              sListPrevNode = NULL,
                              sListStack = sStack + 1,
                              sListRemain = sRemain - 1;
                          sListCurNode != NULL;
                          sListCurNode = (qtcNode*) sListCurNode->node.next,
                              sListStack++, sListRemain-- )
                    {
                        // ���ȭ�ϱ� �� ���� ��带 ��� �Ѵ�.
                        IDU_LIMITPOINT("qtc::preProcessConstExpr::malloc1");
                        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qtcNode, & sOrgNode )
                                  != IDE_SUCCESS);

                        idlOS::memcpy( sOrgNode, sListCurNode, ID_SIZEOF(qtcNode) );

                        // ��� Expression�� ������
                        IDE_TEST( runConstExpr( aStatement,
                                                QC_SHARED_TMPLATE(aStatement),
                                                sListCurNode,
                                                aTemplate,
                                                sListStack,
                                                sListRemain,
                                                aCallBack,
                                                & sResultNode )
                                  != IDE_SUCCESS );

                        if (sListPrevNode != NULL )
                        {
                            sListPrevNode->node.next = (mtcNode*) sResultNode;
                        }
                        else
                        {
                            sCurNode->node.arguments = (mtcNode*) sResultNode;
                        }
                        sResultNode->node.next = sListCurNode->node.next;
                        sResultNode->node.conversion = NULL;

                        sResultNode->node.arguments =
                            sListCurNode->node.arguments;
                        sResultNode->node.leftConversion =
                            sListCurNode->node.leftConversion;
                        sResultNode->node.orgNode =
                            (mtcNode*) sOrgNode;
                        sResultNode->node.funcArguments =
                            sListCurNode->node.funcArguments;
                        sResultNode->node.baseTable =
                            sListCurNode->node.baseTable;
                        sResultNode->node.baseColumn =
                            sListCurNode->node.baseColumn;

                        sListPrevNode = sResultNode;
                    }

                    // PROJ-1436
                    // LIST ��ü�� ���ȭ������ �ʴ´�. LIST�� ���ȭ�ϸ�
                    // LIST�� ���� stack�� ��� tuple�� ����ϰ� �ǹǷ� ���
                    // tuple�� row�� �����ȴ�.
                    sPrevNode = sCurNode;
                }
                else
                {
                    // ���ȭ�ϱ� �� ���� ��带 ��� �Ѵ�.
                    IDU_LIMITPOINT("qtc::preProcessConstExpr::malloc2");
                    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qtcNode, & sOrgNode )
                              != IDE_SUCCESS);
                    
                    idlOS::memcpy( sOrgNode, sCurNode, ID_SIZEOF(qtcNode) );
                    
                    // ��� Expression�� ������
                    IDE_TEST( runConstExpr( aStatement,
                                            QC_SHARED_TMPLATE(aStatement),
                                            sCurNode,
                                            aTemplate,
                                            sStack,
                                            sRemain,
                                            aCallBack,
                                            & sResultNode ) != IDE_SUCCESS );
                    
                    // ������� ����
                    if ( sPrevNode != NULL )
                    {
                        sPrevNode->node.next = (mtcNode *) sResultNode;
                    }
                    else
                    {
                        aNode->node.arguments = (mtcNode *) sResultNode;
                    }
                    sResultNode->node.next = sCurNode->node.next;
                    sResultNode->node.conversion = NULL;
                    
                    // List���� ó���� ���Ͽ� Argument������ ���� �д�.
                    sResultNode->node.arguments =
                        sCurNode->node.arguments;
                    sResultNode->node.leftConversion =
                        sCurNode->node.leftConversion;
                    sResultNode->node.orgNode =
                        (mtcNode*) sOrgNode;
                    sResultNode->node.funcArguments =
                        sCurNode->node.funcArguments;
                    sResultNode->node.baseTable =
                        sCurNode->node.baseTable;
                    sResultNode->node.baseColumn =
                        sCurNode->node.baseColumn;

                    // �̹� ġȯ�� Node�� �̿��Ͽ��� �Ѵ�.
                    sPrevNode = sResultNode;
                }
            }
            else
            {
                // ���� ó���� �� ����.
                sPrevNode = sCurNode;
                continue;
            }
        } // end of for
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::setColumnExecutionPosition( mtcTemplate * aTemplate,
                                 qtcNode     * aNode,
                                 qmsSFWGH    * aColumnSFWGH,
                                 qmsSFWGH    * aCurrentSFWGH )
{
/***********************************************************************
 *
 * Description : column ���� ��ġ ����
 *
 * Implementation :
 *     ex) SELECT i1 FROM t1 ORDER BY i1;
 *         i1 column���� target�� order by���� ����ȴٴ� ������ ������
 *
 *      aColumnSFWGH : ���� column�� ���� �ִ� SFWGH
 *     aCurrentSFWGH : ���� ó������ SFWGH
 ***********************************************************************/

    mtcColumn       * sMtcColumn;

    if( (aColumnSFWGH != NULL) &&
        (aCurrentSFWGH != NULL) )
    {
        sMtcColumn = QTC_TUPLE_COLUMN(&aTemplate->rows[aNode->node.table],
                                      aNode);

        // 1) ���ǿ� ���� �÷������� ������ �����Ѵ�.
        sMtcColumn->flag |= MTC_COLUMN_USE_COLUMN_TRUE;

        /*
         * PROJ-1789 PROWID
         * ����� _PROWID�� target�� �ִ��� where���� �ִ��� ���� X
         */
        if( aNode->node.column == MTC_RID_COLUMN_ID )
        {
            aTemplate->rows[aNode->node.table].lflag
                &= ~MTC_TUPLE_TARGET_RID_MASK;
            aTemplate->rows[aNode->node.table].lflag
                |= MTC_TUPLE_TARGET_RID_EXIST;
        }

        // 2) �÷��� ��ġ ���� ����
        if( aColumnSFWGH->thisQuerySet != NULL )
        {
            // 2) �÷��� ��ġ�� target�����������������Ѵ�.
            if( aColumnSFWGH->thisQuerySet->processPhase
                == QMS_VALIDATE_TARGET )
            {
                // BUG-37841
                // window function���� �����ϴ� �÷��� target�� �̿ܿ����� ����Ѵٰ�
                // �����Ͽ� view push projection���� ���ŵ��� �ʵ��� �Ѵ�.
                if ( ( aNode->lflag & QTC_NODE_ANAL_FUNC_COLUMN_MASK )
                     == QTC_NODE_ANAL_FUNC_COLUMN_FALSE )
                {
                    sMtcColumn->flag
                        |= MTC_COLUMN_USE_TARGET_TRUE;
                }
                else
                {
                    sMtcColumn->flag
                        |= MTC_COLUMN_USE_NOT_TARGET_TRUE;
                }
            }
            else
            {
                sMtcColumn->flag
                    |= MTC_COLUMN_USE_NOT_TARGET_TRUE;
            }

            if( aColumnSFWGH->thisQuerySet->processPhase
                == QMS_VALIDATE_WHERE )
            {
                sMtcColumn->flag
                    |= MTC_COLUMN_USE_WHERE_TRUE;
            }
            else
            {
                // Nothing to do.
            }
  
            if( aColumnSFWGH->thisQuerySet->processPhase
                == QMS_VALIDATE_SET )
            {
                sMtcColumn->flag
                    |= MTC_COLUMN_USE_SET_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            // BUG-25470
            // OUTER COLUMN REFERENCE�� �ִ� ��� flag�����Ѵ�.
            if( aColumnSFWGH != aCurrentSFWGH )
            {
                sMtcColumn->flag
                    |= MTC_COLUMN_OUTER_REFERENCE_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // SELECT ���� ���� ���� Į���� ���
            // ex) DELETE ������ where �� column
            // nothing to do
        }

        //-----------------------------------
        // LOB, GEOMETRY TYPE ��
        // binary �÷��� ���Ե� ���ǹ���
        // rid ���������� ó���Ѵ�.
        //-----------------------------------
        if( qtc::isEquiValidType(aNode, aTemplate)
            == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            aTemplate->rows[aNode->node.table].lflag
                &= ~MTC_TUPLE_BINARY_COLUMN_MASK;
            aTemplate->rows[aNode->node.table].lflag
                |= MTC_TUPLE_BINARY_COLUMN_EXIST;
        }

        // materialize ����� Push Projection �� ���
        if( ( ( aTemplate->rows[aNode->node.table].lflag &
                MTC_TUPLE_STORAGE_MASK )
              == MTC_TUPLE_STORAGE_DISK )
            ||
            ( ( aTemplate->rows[aNode->node.table].lflag &
                MTC_TUPLE_VIEW_MASK )
              == MTC_TUPLE_VIEW_TRUE ) )
        {
            if( aColumnSFWGH->hints->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
            {
                // tuple set����
                // ���ڵ����������� ó���Ǿ�� ��
                // tuple���� �����Ѵ�.
                // ����, rid �Ǵ� record �������� ó����
                // hint �Ǵ� memory table ���п� ���� ��������
                // ���̱� ���ؼ�.
                if (((aTemplate->rows[aNode->node.table].lflag &
                      MTC_TUPLE_BINARY_COLUMN_MASK) ==
                     MTC_TUPLE_BINARY_COLUMN_ABSENT) &&
                    ((aTemplate->rows[aNode->node.table].lflag &
                      MTC_TUPLE_TARGET_RID_MASK) ==
                     MTC_TUPLE_TARGET_RID_ABSENT))
                {
                    aTemplate->rows[aNode->node.table].lflag
                        &= ~MTC_TUPLE_MATERIALIZE_MASK;
                    aTemplate->rows[aNode->node.table].lflag
                        |= MTC_TUPLE_MATERIALIZE_VALUE;
                }
                else
                {
                    // BUG-35585
                    // tuple�Ӹ��ƴ϶� SFWGH, querySet�� rid�� ó���ϰ� �Ѵ�.
                    aColumnSFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_RID;

                    aTemplate->rows[aNode->node.table].lflag
                        &= ~MTC_TUPLE_MATERIALIZE_MASK;
                    aTemplate->rows[aNode->node.table].lflag
                        |= MTC_TUPLE_MATERIALIZE_RID;
                }
            }
            else
            {
                aTemplate->rows[aNode->node.table].lflag
                    &= ~MTC_TUPLE_MATERIALIZE_MASK;
                aTemplate->rows[aNode->node.table].lflag
                    |= MTC_TUPLE_MATERIALIZE_RID;
            }
        }
        else
        {
            // DISK�� VIEW�� �ƴ� ���
            // Nothing To Do
        }
    }
    else
    {
        // SFWGH�� NULL �� ���
        // Nothing To Do 
    }

    return IDE_SUCCESS;
}


void qtc::checkLobAndEncryptColumn( mtcTemplate * aTemplate,
                                    mtcNode     * aNode )
{
    qtcNode     * sNode      = NULL;
    qcStatement * sStatement = NULL;
    mtdModule   * sMtdModule = NULL;

    sNode      = ( qtcNode * )aNode;
    sStatement = ( ( qcTemplate * )aTemplate )->stmt;

    /* PROJ-2462 ResultCache */
    if ( ( sNode->node.column != ID_USHORT_MAX ) &&
         ( sNode->node.table  != ID_USHORT_MAX ) )
    {
        sMtdModule = (mtdModule *)aTemplate->rows[sNode->node.table].columns[sNode->node.column].module;
        if ( sMtdModule != NULL )
        {
            // PROJ-2462 Result Cache
            if ( ( sMtdModule->flag & MTD_ENCRYPT_TYPE_MASK )
                 == MTD_ENCRYPT_TYPE_TRUE )
            {
                /* BUG-45626 Encrypt Column View Compile Fatal */
                if ( QC_SHARED_TMPLATE(sStatement) != NULL )
                {
                    QC_SHARED_TMPLATE(sStatement)->resultCache.flag &= ~QC_RESULT_CACHE_DISABLE_MASK;
                    QC_SHARED_TMPLATE(sStatement)->resultCache.flag |= QC_RESULT_CACHE_DISABLE_TRUE;
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

            // PROJ-2462 Result Cache
            if ( ( sMtdModule->flag & MTD_COLUMN_TYPE_MASK )
                 == MTD_COLUMN_TYPE_LOB )
            {
                sNode->lflag &= ~QTC_NODE_LOB_COLUMN_MASK;
                sNode->lflag |= QTC_NODE_LOB_COLUMN_EXIST;
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
    else
    {
        /* Nothing to do */
    }
}

idBool qtc::isConstValue( qcTemplate  * aTemplate,
                          qtcNode     * aNode )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                constant value(���� ���� ǥ����)���� �ƴ��� �˻�.
 *
 *  Implementation :
 *            (1) conversion�� ����� �Ѵ�.
 *               - ������ �̹� conversion�ٿ��� calculate�� ����.
 *            (2) valueModule�̾�� �Ѵ�.
 *               - ���� ���� �ϳ��� Ư�� ������ �����.
 *            (3) constant tuple ������ �־�� �Ѵ�.
 *               - constant expressionó���� �Ǿ��ٸ�
 *                 constant tuple������ ���� �����.
 *
 ***********************************************************************/

    if ( ( aNode->node.conversion == NULL ) &&
         ( aNode->node.module == & qtc::valueModule ) &&
         ( (aTemplate->tmplate.rows[aNode->node.table].lflag &
            MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_CONSTANT ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

idBool qtc::isHostVariable( qcTemplate  * aTemplate,
                            qtcNode     * aNode )
{
/***********************************************************************
 *
 *  Description : host variable node���� �˻��Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    if ( ( aNode->node.module == & qtc::valueModule ) &&
         ( (aTemplate->tmplate.rows[aNode->node.table].lflag &
            MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_VARIABLE ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

IDE_RC
qtc::isConstExpr( qcTemplate  * aTemplate,
                  qtcNode     * aNode,
                  idBool      * aResult )
{
/***********************************************************************
 *
 * Description :
 *
 *    Constant Expression������ �Ǵ�
 *
 * Implementation :
 *
 *     [Argument�� ���ռ� �˻�]
 *          - argument�� conversion�� �־�� �Ѵ�.
 *          - dependecies�� zero�̾�� �Ѵ�.
 *          - prior�� ����� �Ѵ�.
 *          - aggregation�� ����� �Ѵ�.
 *          - subquery�� ����� �Ѵ�.
 *          - �̹� conversion�� �߻��� ��尡 �ƴϾ�� �Ѵ�.
 *          - column�� �ƴϾ�� �Ѵ�.
 *            : PROJ-1075 array type�� column node�� argument�� �� �� ����.
 *
 *     [���� ������ ����]
 *          - Argument�� �ִٸ�, ��� argument�� ����̾�� �Ѵ�.
 *          - Argument�� ���� Conversion�� �ִٸ�,
 *          - ���� Node�� ����̾�� �Ѵ�.
 *
 ***********************************************************************/
#define IDE_FN "qtc::isConstExpr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sCurNode;
    qtcNode * sArgNode;

    idBool sAbleProcess;

    sAbleProcess = ID_TRUE;
    sCurNode = aNode;

    // BUG-15995
    // random, sendmsg ���� �Լ����� constant��
    // ó���Ǿ�� �ȵ�.
    if ( (sCurNode->node.module->lflag & MTC_NODE_VARIABLE_MASK)
         == MTC_NODE_VARIABLE_FALSE )
    {
        if ( sCurNode->node.arguments != NULL )
        {
            // To Fix PR-8724
            // SUM(4) �� ���� Aggregation�� ��쿡��
            // ��ó���ؼ��� �ȵ�.
            // PROJ-1075
            // column node�� ��� argument�� �ִ� �ϴ���
            // constant expression�� �� �� ����.
            if ( ( sCurNode->subquery == NULL )
                 &&
                 ( (aNode->lflag & QTC_NODE_AGGREGATE_MASK)
                   == QTC_NODE_AGGREGATE_ABSENT )
                 &&
                 ( (aNode->lflag & QTC_NODE_AGGREGATE2_MASK)
                   == QTC_NODE_AGGREGATE2_ABSENT )
                 &&
                 ( (aNode->lflag & QTC_NODE_CONVERSION_MASK)
                   != QTC_NODE_CONVERSION_TRUE )
                 &&
                 ( aNode->node.module != &qtc::columnModule )
                 &&
                 ( (aNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK)
                   == 1 )
                 )
            {
                // Argument�� �ִ� ���
                for ( sArgNode = (qtcNode*) sCurNode->node.arguments;
                      sArgNode != NULL;
                      sArgNode = (qtcNode*) sArgNode->node.next )
                {
                    if ( ( sArgNode->node.module == & qtc::valueModule ) &&
                         ( (aTemplate->tmplate.rows[sArgNode->node.table].lflag &
                            MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_CONSTANT ) )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sAbleProcess = ID_FALSE;
                        break;
                    }
                }
            }
            else
            {
                sAbleProcess = ID_FALSE;
            }
        }
        else
        {
            // Argument�� ���� ���
            if ( ( sCurNode->node.conversion != NULL ) &&
                 ( sCurNode->node.module == & qtc::valueModule ) &&
                 ( (aTemplate->tmplate.rows[sCurNode->node.table].lflag &
                    MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_CONSTANT ) )
            {
                // Nothing To Do
            }
            else
            {
                sAbleProcess = ID_FALSE;
            }
        }
    }
    else
    {
        // �׻� Variable�� ó���Ǿ�� �ϴ� ���
        sAbleProcess = ID_FALSE;
    }

    *aResult = sAbleProcess;

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qtc::runConstExpr( qcStatement * aStatement,
                   qcTemplate  * aTemplate,
                   qtcNode     * aNode,
                   mtcTemplate * aMtcTemplate,
                   mtcStack    * aStack,
                   SInt          aRemain,
                   mtcCallBack * aCallBack,
                   qtcNode    ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Constant Expression�� �����ϰ� ��ȯ�� ��� ��带
 *    �����Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "qtc::runConstExpr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sCurNode;
    qtcNode * sConvertNode;
    mtcStack * sStack;
    SInt       sRemain;

    qtcCallBackInfo * sCallBackInfo = (qtcCallBackInfo*)(aCallBack->info);
    qcuSqlSourceInfo  sqlInfo;
    UInt              sSqlCode;

    sCurNode = aNode;

    sStack = aTemplate->tmplate.stack;
    sRemain = aTemplate->tmplate.stackRemain;

    // ���� ��带 ���� Constant ���� �Ҵ�
    if ( sCurNode->node.arguments != NULL )
    {
        IDE_TEST( getConstColumn( aStatement,
                                  aTemplate,
                                  sCurNode ) != IDE_SUCCESS );
    }

    // Conversion�� ���� Constant ���� �Ҵ�
    for ( sConvertNode = (qtcNode*) sCurNode->node.conversion;
          sConvertNode != NULL;
          sConvertNode = (qtcNode*) sConvertNode->node.conversion )
    {
        IDE_TEST( getConstColumn( aStatement,
                                  aTemplate,
                                  sConvertNode ) != IDE_SUCCESS );
    }

    // PROJ-1346
    // list conversion�� ���� ����� ���ȭ�� �����Ͽ� �ּ�ó���Ͽ���.
    // runConstExpr �����Ŀ� ���� ����� leftConversion�� �����Ѵ�.
    
    // To Fix PR-12938
    // Left Conversion�� ������ ��� �̿� ���� Constant ������ �Ҵ���.
    // Left Conversion�� ���ӵ� conversion�� node.conversion��.
    //for ( sConvertNode = (qtcNode*) sCurNode->node.leftConversion;
    //      sConvertNode != NULL;
    //      sConvertNode = (qtcNode*) sConvertNode->node.conversion )
    //{
    //    IDE_TEST( getConstColumn( aStatement,
    //                              aTemplate,
    //                              sConvertNode ) != IDE_SUCCESS );
    //}

    // Constant Expression�� ���� ������ ����
    // Value������ ���� ����ȴ�.
    aTemplate->tmplate.stack = aStack;
    aTemplate->tmplate.stackRemain = aRemain;

    if ( qtc::calculate( sCurNode,
                         aTemplate )
         != IDE_SUCCESS )
    {
        sqlInfo.setSourceInfo( aStatement,
                               & sCurNode->position );
        IDE_RAISE( ERR_PASS );
    }
    else
    {
        // Nothing to do.
    }

    // ���� ������ ����� �ش��ϴ� ��� ȹ��
    sConvertNode = (qtcNode *)
        mtf::convertedNode( (mtcNode *) sCurNode,
                            & aTemplate->tmplate );

    // Value Module�� ġȯ
    if ( sConvertNode->node.module != &valueModule )
    {
        sConvertNode->node.module = &valueModule;

        // ��庯ȯ�� �߻����� �÷��׷� ����
        sConvertNode->lflag &= ~QTC_NODE_CONVERSION_MASK;
        sConvertNode->lflag |= QTC_NODE_CONVERSION_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    // PROJ-1413
    // Value Name�� ����
    // �̹� ������ conversion ���鿡 �־��� position�� �ƴ϶�
    // ���� ��尡 ���� position ������ �����ؾ� �Ѵ�.
    if ( sCurNode != sConvertNode )
    {
        SET_POSITION( sConvertNode->position, sCurNode->position );
        SET_POSITION( sConvertNode->userName, sCurNode->userName );
        SET_POSITION( sConvertNode->tableName, sCurNode->tableName );
        SET_POSITION( sConvertNode->columnName, sCurNode->columnName );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST(
        qtc::estimateNode( sConvertNode,
                           aMtcTemplate,
                           aStack,
                           aRemain,
                           aCallBack )
        != IDE_SUCCESS );

    *aResultNode = sConvertNode;

    aTemplate->tmplate.stack = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_PASS );
    {
        // sqlSourceInfo�� ���� error���.
        if ( ideHasErrorPosition() == ID_FALSE )
        {
            sSqlCode = ideGetErrorCode();

            (void)sqlInfo.initWithBeforeMessage(sCallBackInfo->memory);
            IDE_SET(
                ideSetErrorCode(qpERR_ABORT_QSX_SQLTEXT_WRAPPER,
                                sqlInfo.getBeforeErrMessage(),
                                sqlInfo.getErrMessage()));
            (void)sqlInfo.fini();

            // overwrite wrapped errorcode to original error code.
            ideGetErrorMgr()->Stack.LastError = sSqlCode;
        }
        else
        {
            // Nothing to do.
        }
    }
    IDE_EXCEPTION_END;

    aTemplate->tmplate.stack = sStack;
    aTemplate->tmplate.stackRemain = sRemain;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::getConstColumn( qcStatement * aStatement,
                     qcTemplate  * aTemplate,
                     qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Constant Expression�� ���� ó���� ����
 *    Constant Column������ ȸ��
 *
 * Implementation :
 *
 *
 ***********************************************************************/
#define IDE_FN "qtc::getConstColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_RC sResult;

    UShort sOrgTupleID;
    UShort sOrgColumnID;
    UShort sNewTupleID;
    UShort sNewColumnID;
    UInt   sColumnCnt;

    mtcTemplate * sMtcTemplate;

    sMtcTemplate = & aTemplate->tmplate;

    // ���� ������ ����
    sOrgTupleID = aNode->node.table;
    sOrgColumnID = aNode->node.column;
    sColumnCnt = aNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK;

    // ���ռ� �˻�
    IDE_DASSERT( sColumnCnt == 1 );

    // ���ο� Constant ������ �Ҵ�
    sResult = qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode,
                               aStatement,
                               aTemplate,
                               MTC_TUPLE_TYPE_CONSTANT,
                               sColumnCnt );

    // PROJ-1358
    // Internal Tuple�� ������ ������ Tuple Pointer�� ����� �� �ִ�.
    sNewTupleID = aNode->node.table;

    sMtcTemplate->rows[sNewTupleID].rowOffset = idlOS::align(
        sMtcTemplate->rows[sNewTupleID].rowOffset,
        sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module->align );

    if ( (sResult == IDE_SUCCESS) &&
         ( sMtcTemplate->rows[sNewTupleID].rowOffset
           +  sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].column.size
           >  sMtcTemplate->rows[sNewTupleID].rowMaximum ) )
    {
        // Constant Tuple�� ���� ũ�� �˻�
        sResult = IDE_FAILURE;
    }

    // ������ ������ ��� ������ Constant Tuple �Ҵ� �� ó��
    if( sResult != IDE_SUCCESS )
    {
        // PROJ-1583 large geometry
        /* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
        if( (sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module->id == MTD_GEOMETRY_ID) ||
            (sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module->id == MTD_BINARY_ID) ||
            (sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module->id == MTD_BLOB_ID) ||
            (sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module->id == MTD_CLOB_ID) )
        {
            IDE_TEST( qtc::nextLargeConstColumn( QC_QMP_MEM(aStatement),
                                                 aNode,
                                                 aStatement,
                                                 aTemplate,
                                                 sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].column.size )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qtc::nextRow( QC_QMP_MEM(aStatement),
                                    aStatement,
                                    aTemplate,
                                    MTC_TUPLE_TYPE_CONSTANT )
                      != IDE_SUCCESS );

            IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                       aNode,
                                       aStatement,
                                       aTemplate,
                                       MTC_TUPLE_TYPE_CONSTANT,
                                       sColumnCnt )
                      != IDE_SUCCESS );
        }

        // PROJ-1358
        // Internal Tuple�� ������ ������ Tuple Pointer�� ����� �� �ִ�.
        sNewTupleID = aNode->node.table;

        sMtcTemplate->rows[sNewTupleID].rowOffset = idlOS::align(
            sMtcTemplate->rows[sNewTupleID].rowOffset,
            sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].module
            ->align );

        // ���ռ� �˻�
        IDE_DASSERT(
            sMtcTemplate->rows[sNewTupleID].rowOffset
            + sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID].column.size
            <=  sMtcTemplate->rows[sNewTupleID].rowMaximum );
    }

    // ������ ������ �� �ֵ���
    // ���� ������ ���� �Ҵ� ���� Contant ������ ����
    sNewColumnID = aNode->node.column;

    idlOS::memcpy( & sMtcTemplate->rows[sNewTupleID].columns[sNewColumnID],
                   &  sMtcTemplate->rows[sOrgTupleID].columns[sOrgColumnID],
                   ID_SIZEOF(mtcColumn) * sColumnCnt );
    idlOS::memcpy( &  sMtcTemplate->rows[sNewTupleID].execute[sNewColumnID],
                   &  sMtcTemplate->rows[sOrgTupleID].execute[sOrgColumnID],
                   ID_SIZEOF(mtcExecute) * sColumnCnt );

    // Column�� offset�� Tuple�� offset ������
    sMtcTemplate->rows[sNewTupleID].columns[sNewColumnID].column.offset
        =  sMtcTemplate->rows[sNewTupleID].rowOffset;
    sMtcTemplate->rows[sNewTupleID].rowOffset
        +=  sMtcTemplate->rows[sNewTupleID].columns[sNewColumnID].column.size;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qtc::makeConstantWrapper( qcStatement * aStatement,
                          qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Host Constant Wrapper Node�� ������.
 *    (����, qtcConstantWrapper.cpp)
 *
 * Implementation :
 *
 *    ���� Node�� �����ϰ�,
 *    ���� Node�� Constant Wrapper Node�� ��ü��.
 *    �̷��� �����μ� �ܺο��� ������ ��� ���� ó���� ������.
 *
 *    [aNode]      =>      [Wrapper]
 *                             |
 *                             V
 *                         ['aNode'] : ����� Node
 *
 ***********************************************************************/

#define IDE_FN "qtc::makeConstantWrapper"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode     * sNode;
    mtcTemplate * sTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;
    UShort        sVariableRow;

    //---------------------------------------
    // �Էµ� Node�� ����
    //---------------------------------------

    if( aNode->node.module == & hostConstantWrapperModule )
    {
        return IDE_SUCCESS;
    }

    IDU_LIMITPOINT("qtc::makeConstantWrapper::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, & sNode )
             != IDE_SUCCESS);

    idlOS::memcpy( sNode, aNode, ID_SIZEOF(qtcNode) );

    //---------------------------------------
    // ���� Node�� Wrapper Node�� ��ü��.
    //---------------------------------------

    aNode->node.module = & hostConstantWrapperModule;
    aNode->node.conversion     = NULL;
    aNode->node.leftConversion = NULL;
    aNode->node.funcArguments  = NULL;
    aNode->node.orgNode        = NULL;
    aNode->node.arguments      = & sNode->node; // ������ ��带 ����
    aNode->node.cost           = 0;
    aNode->subquery            = NULL;
    aNode->indexArgument       = 0;

    // fix BUG-11545
    // ����� ���(wrapper node�� arguments)�� next�� ������ ���´�.
    // constantWrapper����� next�� �� next�� ���������� ����.
    sNode->node.next           = NULL;

    aNode->node.lflag  = hostConstantWrapperModule.lflag;
    aNode->node.lflag |=
        sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
    aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

    // PROJ-1492
    // bind���� lflag�� �����Ѵ�.
    aNode->node.lflag |= sNode->node.lflag & MTC_NODE_BIND_TYPE_MASK;

    //---------------------------------------
    // Node ID�� ���� �Ҵ����
    //---------------------------------------

    // fix BUG-18868
    // BUG-17506�� �������� (sysdate, PSM����, Bind����)��
    // execution �߿� constant�� ó���� �� �ְ� �Ǿ���.
    // �׷��� ȣ��Ʈ ������ ������� �ʴ� ������ ��� �Ľ̰�������
    // ȣ��Ʈ ������ ���� ��带 ������ �ʴ´�.
    // ������ constant wrapper ��嵵 variable tuple type�� ����ϱ� ������
    // nextColumn() ȣ�� �Ŀ� variableRow�� ���� ȣ��Ʈ ������ �ִ� ��ó��
    // ����ǰ� �ȴ�.
    // �� ���� nextColumn() ȣ�� �Ŀ� �������ش�.

    sVariableRow = sTemplate->variableRow;

    IDE_TEST( qtc::nextColumn(
                  QC_QMP_MEM(aStatement),
                  aNode,
                  aStatement,
                  QC_SHARED_TMPLATE(aStatement),
                  MTC_TUPLE_TYPE_VARIABLE,
                  aNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK )
              != IDE_SUCCESS );

    // fix BUG-18868
    sTemplate->variableRow = sVariableRow;

    IDE_TEST( qtc::estimateNode( aStatement,
                                 aNode )
              != IDE_SUCCESS );

    //---------------------------------------
    // Execute ���θ� ǥ���� ���� ������ ��ġ�� ������.
    //---------------------------------------

    aNode->node.info = sTemplate->execInfoCnt;
    sTemplate->execInfoCnt++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::optimizeHostConstExpression( qcStatement * aStatement,
                                  qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Node Tree ������ Host Constant Expression�� ã�Ƴ���,
 *    �̿� ���Ͽ� Constant Wrapper Node�� ����.
 *    (����, qtcConstantWrapper.cpp)
 *
 * Implementation :
 *
 *    ���� ��츦 Host Constant Expression���� �Ǵ��Ѵ�.
 *        - Dependencies�� 0�̾�� ��.
 *        - Argument�� �ְų� Conversion�� �־�� ��.
 *        - Ex)
 *            - 5 + ?         [+] : Host Constant Expression
 *                             |
 *                             V
 *                            [5]---->[?]
 *
 *            - double = ?    [=]
 *                             |              t-->[conv]
 *                             V              |
 *                            [double]-------[?]  : Host Constant Expression
 *
 *    ������ ���� ���� �� �̻� �������� �ʴ´�.
 *        - HOST ������ ���� ���
 *        - Indirection�� �ִ� ���
 *        - List���� ���
 *        - �̴� Host Constant Expression�̶� �ϴ���,
 *          ����� �ϳ� �̻��� �� �ֱ� �����̴�.
 *
 *
 ***********************************************************************/

#define IDE_FN ""
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sNode;

    if ( ( (aNode->node.lflag & MTC_NODE_INDIRECT_MASK)
           == MTC_NODE_INDIRECT_TRUE )                // Indirection�� ���
         ||
         ( aNode->node.module == & mtfList )          // List �� ���
         ||
         ( aNode->node.module == & subqueryModule )   // Subquery�� ���
         ||
         ( ( (aNode->lflag & QTC_NODE_AGGREGATE_MASK) // Aggregation�� ����
             == QTC_NODE_AGGREGATE_EXIST ) ||
           ( (aNode->lflag & QTC_NODE_AGGREGATE2_MASK)
             == QTC_NODE_AGGREGATE2_EXIST ) ) )
    {
        // Nothing To Do
        // �� �̻� �������� �ʴ´�.
    }
    else
    {
        // BUG-17506
        // Execution�߿��� ����� ����� �� �ִ� ��带 Dynamic Constant���
        // �θ���. (sysdate, PSM����, Bind����)
        if ( QTC_IS_DYNAMIC_CONSTANT( aNode ) == ID_TRUE )
        {
            if ( ( qtc::dependencyEqual( & aNode->depInfo,
                                         & qtc::zeroDependencies )
                   == ID_TRUE ) &&
                 ( aNode->node.arguments != NULL ||
                   aNode->node.conversion != NULL )
                 )
            {
                // Constant Expression�� ���� Wrapper Node�� �����ϰ�
                // �� �̻� �������� �ʴ´�.
                IDE_TEST( qtc::makeConstantWrapper( aStatement, aNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // ���� Node ������ ���� Traverse�� �����Ѵ�.
                for ( sNode = (qtcNode*) aNode->node.arguments;
                      sNode != NULL;
                      sNode = (qtcNode*) sNode->node.next )
                {
                    IDE_TEST( qtc::optimizeHostConstExpression( aStatement, sNode )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // Nothing To Do
            // �� �̻� �������� �ʴ´�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::makeSubqueryWrapper( qcStatement * aStatement,
                          qtcNode     * aSubqueryNode,
                          qtcNode    ** aWrapperNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Subquery Wrapper Node�� ������.
 *    (����, qtcSubqueryWrapper.cpp)
 *
 * Implementation :
 *    �ݵ�� Subquery Node���� ���ڷ� �޴´�.
 *
 *                           [Wrapper]
 *                             |
 *                             V
 *    [aSubqueryNode]   =>   [aSubqueryNode]
 *
 ***********************************************************************/

#define IDE_FN "qtc::makeSubqueryWrapper"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sWrapperNode;
    mtcTemplate * sTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    // �ݵ�� Subquery Node�� ���ڷ� �޴´�.
    IDE_DASSERT( aSubqueryNode->subquery != NULL );

    //---------------------------------------
    // �Էµ� Subquery Node�� ����
    // dependencies�� flag ������ �����ϱ� ����.
    //---------------------------------------

    IDU_LIMITPOINT("qtc::makeSubqueryWrapper::malloc");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, & sWrapperNode )
             != IDE_SUCCESS);

    idlOS::memcpy( sWrapperNode, aSubqueryNode, ID_SIZEOF(qtcNode) );

    //---------------------------------------
    // Wrapper Node�� ������ ����
    //---------------------------------------

    sWrapperNode->node.module = & subqueryWrapperModule;
    sWrapperNode->node.conversion     = NULL;
    sWrapperNode->node.leftConversion = NULL;
    sWrapperNode->node.funcArguments  = NULL;
    sWrapperNode->node.orgNode        = NULL;
    sWrapperNode->node.arguments      = (mtcNode*) aSubqueryNode;
    sWrapperNode->node.next           = NULL;
    sWrapperNode->node.cost           = 0;
    sWrapperNode->subquery            = NULL;
    sWrapperNode->indexArgument       = 0;

    // flag ���� ����
    // ���� ������ �״�� �����ϰ�, pass node�� ������ �߰�
    sWrapperNode->node.lflag &= ~MTC_NODE_COLUMN_COUNT_MASK;
    sWrapperNode->node.lflag |= 1;

    sWrapperNode->node.lflag &= ~MTC_NODE_OPERATOR_MASK;
    sWrapperNode->node.lflag |= MTC_NODE_OPERATOR_MISC;

    // Indirection ���� Setting
    sWrapperNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
    sWrapperNode->node.lflag |= MTC_NODE_INDIRECT_TRUE;

    //---------------------------------------
    // Node ID�� ���� �Ҵ����
    //---------------------------------------

    IDE_TEST(
        qtc::nextColumn(
            QC_QMP_MEM(aStatement),
            sWrapperNode,
            aStatement,
            QC_SHARED_TMPLATE(aStatement),
            MTC_TUPLE_TYPE_INTERMEDIATE,
            sWrapperNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK )
        != IDE_SUCCESS );

    IDE_TEST( qtc::estimateNode( aStatement,
                                 sWrapperNode )
              != IDE_SUCCESS );

    //---------------------------------------
    // Execute ���θ� ǥ���� ���� ������ ��ġ�� ������.
    //---------------------------------------

    sWrapperNode->node.info = sTemplate->execInfoCnt;
    sTemplate->execInfoCnt++;

    *aWrapperNode = sWrapperNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aWrapperNode = NULL;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeTargetColumn( qtcNode* aNode,
                              UShort   aTable,
                              UShort   aColumn )
{
/***********************************************************************
 *
 * Description :
 *
 *    Asterisk Target Column�� ���� Node�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeTargetColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    QTC_NODE_INIT( aNode );
    
    aNode->node.module         = &columnModule;
    aNode->node.table          = aTable;
    aNode->node.column         = aColumn;
    aNode->node.baseTable      = aTable;
    aNode->node.baseColumn     = aColumn;
    aNode->node.lflag          = columnModule.lflag
                               & ~MTC_NODE_COLUMN_COUNT_MASK;
    
    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qtc::makeInternalColumn( qcStatement* aStatement,
                                UShort       aTable,
                                UShort       aColumn,
                                qtcNode**    aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    SET���� ǥ���� ���� �߰� Column�� ������.
 *
 *    ex) SELECT i1 FROM T1 INTERSECT SELECT a1 FROM T2;
 *    SET�� Target�� ���� ������ Column ������ �����Ͽ��� �Ѵ�.
 *
 * Implementation :
 *
 *    ������ ���� Column Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::makeInternalColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode         * sNode[2] = {NULL,NULL};
    qcNamePosition    sColumnPos;

    SET_EMPTY_POSITION( sColumnPos );

    IDE_TEST( qtc::makeColumn( aStatement, sNode, NULL, NULL, &sColumnPos, NULL )
              != IDE_SUCCESS);

    sNode[0]->node.table  = aTable;
    sNode[0]->node.column = aColumn;

    *aNode = sNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aNode = NULL;

    return IDE_FAILURE;

#undef IDE_FN
}

void qtc::resetTupleOffset( mtcTemplate* aTemplate, UShort aTupleID )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Tuple�� �ִ� offset�� �������Ѵ�.
 *
 * Implementation :
 *
 *    Alignment�� ����Ͽ� �ش� Tuple�� �ִ� Offset�� �������Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::resetTupleOffset"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt          sColumn;
    UInt          sOffset;

    for( sColumn = 0, sOffset = 0;
         sColumn < aTemplate->rows[aTupleID].columnCount;
         sColumn++ )
    {
        sOffset = idlOS::align(
            sOffset,
            aTemplate->rows[aTupleID].columns[sColumn].module->align );
        aTemplate->rows[aTupleID].columns[sColumn].column.offset = sOffset;
        sOffset +=  aTemplate->rows[aTupleID].columns[sColumn].column.size;

        // To Fix PR-8528
        // ���Ƿ� �����Ǵ� Tuple�� ��� Column ID�� ���Ƿ� �����Ѵ�.
        // ���� ������ �� ���� Table�� ID(0)�� �������� �����Ѵ�.
        aTemplate->rows[aTupleID].columns[sColumn].column.id = sColumn;
    }

    aTemplate->rows[aTupleID].rowOffset =
        aTemplate->rows[aTupleID].rowMaximum = sOffset;

#undef IDE_FN
}

IDE_RC qtc::allocIntermediateTuple( qcStatement* aStatement,
                                    mtcTemplate* aTemplate,
                                    UShort       aTupleID,
                                    SInt         aColCount)
{
/***********************************************************************
 *
 * Description :
 *
 *    Intermediate Tuple�� ���� ������ �Ҵ��Ѵ�.
 *
 * Implementation :
 *
 *    Intermediate Tuple�� ���� Column�� Execute������ Ȯ���Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::allocIntermediateTuple"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if (aColCount > 0)
    {
        aTemplate->rows[aTupleID].lflag
            = qtc::templateRowFlags[MTC_TUPLE_TYPE_INTERMEDIATE];
        aTemplate->rows[aTupleID].columnCount   = aColCount;
        aTemplate->rows[aTupleID].columnMaximum = aColCount;

        /* BUG-44382 clone tuple ���ɰ��� */
        // ���簡 �ʿ���
        setTupleColumnFlag( &(aTemplate->rows[aTupleID]),
                            ID_TRUE,
                            ID_FALSE );

        IDU_LIMITPOINT("qtc::allocIntermediateTuple::malloc1");
        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc(
                idlOS::align8((UInt)ID_SIZEOF(mtcColumn)) * aColCount,
                (void**)&(aTemplate->rows[aTupleID].columns))
            != IDE_SUCCESS);

        // PROJ-1473
        IDE_TEST(
            qtc::allocAndInitColumnLocateInTuple( QC_QMP_MEM(aStatement),
                                                  aTemplate,
                                                  aTupleID )
            != IDE_SUCCESS );

        IDU_LIMITPOINT("qtc::allocIntermediateTuple::malloc2");
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                     ID_SIZEOF(mtcExecute) * aColCount,
                     (void**) & (aTemplate->rows[aTupleID].execute))
                 != IDE_SUCCESS);

        aTemplate->rows[aTupleID].rowOffset  = 0;
        aTemplate->rows[aTupleID].rowMaximum = 0;
        aTemplate->rows[aTupleID].row        = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::changeNode( qcStatement*    aStatement,
                        qtcNode**       aNode,
                        qcNamePosition* aPosition,
                        qcNamePosition* aPositionEnd )
{
/***********************************************************************
 *
 * Description :
 *
 *    List Expression�� ������ �������Ѵ�.
 *
 * Implementation :
 *
 *    List Expression�� �ǹ̸� ������ ������ Setting�ϸ�,
 *    �ش� String�� ��ġ�� �������Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::changeNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtfModule* sModule;
    mtcNode*         sNode;
    idBool           sExist;

    IDE_TEST( mtf::moduleByName( &sModule,
                                 &sExist,
                                 (void*)(aPosition->stmtText+aPosition->offset),
                                 aPosition->size )
              != IDE_SUCCESS );
    
    if ( sExist == ID_FALSE )
    {
        sModule = & qtc::spFunctionCallModule;
    }

    // sModule�� mtfList�� �ƴ� ���� �Լ��������� ���� �����.
    // �� ��� node change�� �Ͼ.
    // ex) sum(i1) or to_date(i1, 'yy-mm-dd')
    if( (sModule != &mtfList) || (aNode[0]->node.module == NULL) )
    {
        // node�� module�� null�� �ƴ� ���� ���� expression�� ���.
        // ex) sum(i1)
        //
        // i1��ü�� �޷��ֱ� ������ ������ ���� ���� node�� ����.
        //
        // i1   =>   ( )
        //            |
        //            i1
        if( aNode[0]->node.module != NULL )
        {
            sNode    = &aNode[0]->node;

            // Node ����
            IDU_LIMITPOINT("qtc::changeNode::malloc");
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
                     != IDE_SUCCESS);

            // Node �ʱ�ȭ
            QTC_NODE_INIT( aNode[0] );
            aNode[1] = NULL;

            aNode[0]->node.arguments      = sNode;
            aNode[0]->node.lflag          = 1;
        }

        // node�� module�� null�� ���� list�������� expression�� �Ŵ޷� �ִ� ���.
        // node�� module�� null�� �ƴ� ��� �̹� ������ �� node�� ����� ������.
        // ex) to_date( i1, 'yy-mm-dd' )
        //
        // list������ ��� �ֻ����� �� node�� �޷������Ƿ�
        // ������ ���� �� node�� change
        //
        //  ( )                    to_date
        //   |                =>     |
        //   i1 - 'yy-mm-dd'        i1 - 'yy-mm-dd'
        aNode[0]->columnName       = *aPosition;

        aNode[0]->node.lflag  |= sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->node.module  = sModule;

        if( &qtc::spFunctionCallModule == aNode[0]->node.module )
        {
            // fix BUG-14257
            aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
            aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
        }

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
    }
    else
    {
        // BUG-38935 display name�� ���� �����Ѵ�.
        aNode[0]->node.lflag &= ~MTC_NODE_REMOVE_ARGUMENTS_MASK;
        aNode[0]->node.lflag |= MTC_NODE_REMOVE_ARGUMENTS_TRUE;
    }
    
    aNode[0]->position.stmtText = aPosition->stmtText;
    aNode[0]->position.offset   = aPosition->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size
                                - aPosition->offset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::changeNodeByModule( qcStatement*    aStatement,
                                qtcNode**       aNode,
                                mtfModule*      aModule,
                                qcNamePosition* aPosition,
                                qcNamePosition* aPositionEnd )
{
/***********************************************************************
 *
 * Description : BUG-41310
 *
 *    List Expression�� ������ �������Ѵ�.
 *
 * Implementation :
 *
 *    List Expression�� �ǹ̸� ������ ������ Setting�ϸ�,
 *    �ش� String�� ��ġ�� �������Ѵ�.
 *
 ***********************************************************************/
    const mtfModule* sModule;
    mtcNode*         sNode;

    IDE_DASSERT( aModule != NULL );

    sModule = aModule;

    // sModule�� mtfList�� �ƴ� ���� �Լ��������� ���� �����.
    // �� ��� node change�� �Ͼ.
    // ex) sum(i1) or to_date(i1, 'yy-mm-dd')
    if ( ( sModule != &mtfList ) || ( aNode[0]->node.module == NULL ) )
    {
        // node�� module�� null�� �ƴ� ���� ���� expression�� ���.
        // ex) sum(i1)
        //
        // i1��ü�� �޷��ֱ� ������ ������ ���� ���� node�� ����.
        //
        // i1   =>   ( )
        //            |
        //            i1
        if ( aNode[0]->node.module != NULL )
        {
            sNode    = &aNode[0]->node;

            // Node ����
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ), qtcNode, &( aNode[0] ) )
                     != IDE_SUCCESS );

            // Node �ʱ�ȭ
            QTC_NODE_INIT( aNode[0] );
            aNode[1] = NULL;

            aNode[0]->node.arguments      = sNode;
            aNode[0]->node.lflag          = 1;
        }

        // node�� module�� null�� ���� list�������� expression�� �Ŵ޷� �ִ� ���.
        // node�� module�� null�� �ƴ� ��� �̹� ������ �� node�� ����� ������.
        // ex) to_date( i1, 'yy-mm-dd' )
        //
        // list������ ��� �ֻ����� �� node�� �޷������Ƿ�
        // ������ ���� �� node�� change
        //
        //  ( )                    to_date
        //   |                =>     |
        //   i1 - 'yy-mm-dd'        i1 - 'yy-mm-dd'
        aNode[0]->columnName       = *aPosition;

        aNode[0]->node.lflag  |= sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->node.module  = sModule;

        if( &qtc::spFunctionCallModule == aNode[0]->node.module )
        {
            // fix BUG-14257
            aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
            aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
        }

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM( aStatement ),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE( aStatement ),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
    }
    else
    {
        // BUG-38935 display name�� ���� �����Ѵ�.
        aNode[0]->node.lflag &= ~MTC_NODE_REMOVE_ARGUMENTS_MASK;
        aNode[0]->node.lflag |= MTC_NODE_REMOVE_ARGUMENTS_TRUE;
    }
    
    aNode[0]->position.stmtText = aPosition->stmtText;
    aNode[0]->position.offset   = aPosition->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size
                                - aPosition->offset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeNodeForMemberFunc( qcStatement    * aStatement,
                                     qtcNode       ** aNode,
                                     qcNamePosition * aPositionStart,
                                     qcNamePosition * aPositionEnd,
                                     qcNamePosition * aUserNamePos,
                                     qcNamePosition * aTableNamePos,
                                     qcNamePosition * aColumnNamePos,
                                     qcNamePosition * aPkgNamePos,
                                     idBool           aIsBracket )
{
/**********************************************************************************
 *
 * Description : PROJ-1075
 *    List Expression�� ������ �������Ѵ�.
 *    �Ϲ� function�� ã�� �ʰ� �׻� member function�� ���� �˻��Ѵ�.
 *
 * Implementation :
 *    List Expression�� �ǹ̸� ������ ������ Setting�ϸ�,
 *    �ش� String�� ��ġ�� �������Ѵ�.
 *    �� �Լ��� �� �� �ִ� ����� �Լ� ����
 *        (1) arrvar_name.memberfunc_name( list_expr )  - aUserNamePos(X)
 *        (2) user_name.spfunc_name( list_expr )        - aUserNamePos(X)
 *        (3) label_name.arrvar_name.memberfunc_name( list_expr ) - ��� ����
 *        (4) user_name.package_name.spfunc_name( list_exor ) - ��� ����
 *        // BUG-38243 support array method at package
 *        (5) user_name.package_name.arrvar_name.memberfunc_name( list_expr )
 *             -> (5)�� �׻� member func�� �� �ۿ� ����
 *********************************************************************************/

    const mtfModule* sModule;
    mtcNode*         sNode;
    idBool           sExist;
    SChar            sBuffer[1024];
    SChar            sNameBuffer[256];
    UInt             sLength;

    if ( aIsBracket == ID_FALSE )
    {
        // ���ռ� �˻�. ��� tableName, columnName�� �����ؾ� ��.
        IDE_DASSERT( QC_IS_NULL_NAME( (*aTableNamePos) ) == ID_FALSE );
        IDE_DASSERT( QC_IS_NULL_NAME( (*aColumnNamePos) ) == ID_FALSE );

        if ( QC_IS_NULL_NAME((*aPkgNamePos)) == ID_TRUE )
        {
            // spFunctionCall�� �� ���� ����.
            IDE_TEST( qsf::moduleByName( &sModule,
                                         &sExist,
                                         (void*)(aColumnNamePos->stmtText +
                                                 aColumnNamePos->offset),
                                         aColumnNamePos->size )
                      != IDE_SUCCESS );

            if ( ( sExist == ID_FALSE ) ||
                 ( sModule == &qsfMCountModule ) ||
                 ( sModule == &qsfMFirstModule ) ||
                 ( sModule == &qsfMLastModule ) ||
                 ( sModule == &qsfMNextModule ) )
            {
                sModule = & qtc::spFunctionCallModule;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            // (5)�� ��� ������ member function�̾�߸� ��
            IDE_TEST( qsf::moduleByName( &sModule,
                                         &sExist,
                                         (void*)(aPkgNamePos->stmtText +
                                                 aPkgNamePos->offset),
                                         aPkgNamePos->size )
                      != IDE_SUCCESS );

            if ( sExist == ID_FALSE )
            {
                sLength = aPkgNamePos->size;

                if ( sLength >= ID_SIZEOF(sNameBuffer) )
                {
                    sLength = ID_SIZEOF(sNameBuffer) - 1;
                }
                else
                {
                    /* Nothing to do. */
                }

                idlOS::memcpy( sNameBuffer,
                               (SChar*) aPkgNamePos->stmtText + aPkgNamePos->offset,
                               sLength );
                sNameBuffer[sLength] = '\0';
                idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                                 "(Name=\"%s\")", sNameBuffer );

                IDE_RAISE( ERR_NOT_FOUND );
            }
            else
            {
                /* Nothing to do. */
            }
        }

        // node�� module�� null�� �ƴ� ���� ���� expression�� ���.
        // ex) sum(i1)
        //
        // i1��ü�� �޷��ֱ� ������ ������ ���� ���� node�� ����.
        //
        // i1   =>   ( )
        //            |
        //            i1
        if ( aNode[0]->node.module != NULL )
        {
            sNode    = &aNode[0]->node;

            // Node ����
            IDU_LIMITPOINT("qtc::changeNodeForMemberFunc::malloc");
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &(aNode[0]))
                     != IDE_SUCCESS);

            // Node �ʱ�ȭ
            QTC_NODE_INIT( aNode[0] );
            aNode[1] = NULL;

            // Node ���� ����
            aNode[0]->node.arguments      = sNode;
            aNode[0]->node.lflag          = 1;
        }
        else
        {
            // Nothing to do.
        }

        // node�� module�� null�� ���� list�������� expression�� �Ŵ޷� �ִ� ���.
        // node�� module�� null�� �ƴ� ��� �̹� ������ �� node�� ����� ������.
        // ex) to_date( i1, 'yy-mm-dd' )
        //
        // list������ ��� �ֻ����� �� node�� �޷������Ƿ�
        // ������ ���� �� node�� change
        //
        //  ( )                    to_date
        //   |                =>     |
        //   i1 - 'yy-mm-dd'        i1 - 'yy-mm-dd'
        aNode[0]->node.lflag  |= sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
        aNode[0]->node.module  = sModule;

        if ( &qtc::spFunctionCallModule == aNode[0]->node.module )
        {
            // fix BUG-14257
            aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
            aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
    } // aIsBracket == ID_FALSE
    else // aIsBracket == ID_TRUE
    {
        aNode[0]->node.module = &qtc::columnModule;
        aNode[0]->node.lflag |= qtc::columnModule.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
    }

    aNode[0]->userName   = *aUserNamePos;
    aNode[0]->tableName  = *aTableNamePos;
    aNode[0]->columnName = *aColumnNamePos;
    aNode[0]->pkgName    = *aPkgNamePos;

    aNode[0]->position.stmtText = aPositionStart->stmtText;
    aNode[0]->position.offset   = aPositionStart->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size - aPositionStart->offset;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_FUNCTION_MODULE_NOT_FOUND,
                                  sBuffer ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeIgnoreNullsNode( qtcNode     * aNode,
                                   idBool      * aChanged )
{
/***********************************************************************
 *
 * Description :
 *
 *    analytic function�� ignore nulls �Լ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::changeIgnoreNullsNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar            sModuleName[QC_MAX_OBJECT_NAME_LEN + 1];
    const mtfModule* sModule;
    idBool           sExist;

    idlOS::snprintf( sModuleName,
                     QC_MAX_OBJECT_NAME_LEN + 1,
                     "%s_IGNORE_NULLS",
                     (SChar*) aNode->node.module->names->string );

    IDE_TEST( mtf::moduleByName( &sModule,
                                 &sExist,
                                 (void*)sModuleName,
                                 idlOS::strlen(sModuleName) )
              != IDE_SUCCESS );

    if ( sExist == ID_TRUE )
    {
        aNode->node.module = sModule;
        
        *aChanged = ID_TRUE;
    }
    else
    {
        *aChanged = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::changeWithinGroupNode( qcStatement * aStatement,
                                   qtcNode     * aNode,
                                   qtcOver     * aOver )
{
/*************************************************************************
 *
 * Description :
 *    BUG-41631
 *    ���� �̸��� Within Group Aggregation �Լ��� �����Ѵ�.
 *
 *    ------------------------------------------------------------------------
 *    BUG-41771 PERCENT_RANK WITHIN GROUP
 *      RANK �� ���� ����,
 *        RANK(..) WITHIN GROUP --> �Լ��� : "RANK_WITHIN_GROUP"
 *                                  ���� : mtfRankWithinGroup
 *        RANK() OVER()         --> �Լ��� : "RANK"
 *                                  ���� : mtfRank
 *
 *       RANK�� WITHIIN GROUP ���� ��� ��, �װ��� ����� �̰�����,
 *       mtfRank -> mtfRankWithinGroup
 *       �̷��� ��ü�ȴ�.
 *
 *       �׷���, PERCENT_RANK ��
 *       RANK ó�� Over ���� ����ϴ�, Analytic ����� ����, �ٷ� mtfPercentRankWithinGroup ��
 *       �޸��� �Ǿ� �̰����� ����� ��ü�� �ʿ䰡 ����.(�̰͹ۿ� ������ �Ҽ��� ����.)
 *       �Լ����� 'PERCENT_RANK_WITHIN_GROUP' �� �ƴ� 'PERCENT_RANK' �� �� ������,
 *       �ļ��� sql ������ �Լ����� �򵵷� �ϱ� �����̴�.
 *
 *       PERCENT_RANK() OVER(..)  �Լ��� �߰��ȴٸ�,
 *         PERCENT_RANK() OVER(..)           --> �Լ��� : "PERCENT_RANK"
 *                                               ���� : mtfPercentRank
 *         PERCENT_RANK(..) WITHIN GROUP(..) --> �Լ��� : "PERCENT_RANK_WITHIN_GROUP"
 *                                               ���� : mtfPercentRankWithinGroup
 *       �̷� ���·� �۾��Ѵ�.
 *
 *       ���Ŀ���, PERCENT_RANK �� ���� �⺻ �����,
 *       WITHIN GROUP ��/OVER �� ���о��� mtfPercentRank �� �޸��� �Ǹ�,
 *       ����, WITHIN GROUP ���� ���Ǹ� �̰����� mtfPercentRankWithinGroup ��
 *       ��ü���ش�.
 *
 *       PERCENT_RANK() OVER(..)
 *             |
 *             |
 *          mtfPercentRank
 *
 *       PERCENT_RANK(..) WITHIN GROUP(..)
 *             |
 *             |
 *          mtfPercentRank --> mtfPercentRankWithinGroup
 *
 *       ex)
 *       if ( aNode->node.module == &mtfRank )
 *       {
 *           sModule = &mtfRankWithinGroup;
             sChanged = ID_TRUE;
 *       }
 *       else if ( aNode->node.module == &mtfPercentRank )
 *       {
 *           sModule = &mtfPercentRankWithinGroup;
 *           sChanged = ID_TRUE;
 *       }
 *       else
 *       {
 *           // Nothing to do
 *       }
 *
 *    ------------------------------------------------------------------------
 *    BUG-41800 CUME_DIST WITHIN GROUP
 *      PERCENT_RANK �� ����������,
 *        CUME_DIST() OVER(..)  �Լ��� �߰��ȴٸ�,
 *          CUME_DIST() OVER(..)           --> �Լ��� : "CUME_DIST"
 *                                             ���� : mtfCumeDist
 *          CUME_DIST(..) WITHIN GROUP(..) --> �Լ��� : "CUME_DIST_WITHIN_GROUP"
 *                                             ���� : mtfCumeDistWithinGroup
 *       �̷� ���·� �۾��Ѵ�.
 *
 *       ���Ŀ���, CUME_DIST �� ���� �⺻ �����,
 *       WITHIN GROUP ��/OVER �� ���о��� mtfCumeDist �� �޸��� �Ǹ�,
 *       ����, WITHIN GROUP ���� ���Ǹ� �̰����� mtfCumeDistWithinGroup ��
 *       ��ü���ش�.
 *
 *       CUME_DIST() OVER(..)
 *             |
 *             |
 *          mtfCumeDist
 *
 *       CUME_DIST(..) WITHIN GROUP(..)
 *             |
 *             |
 *          mtfCumeDist --> mtfCumeDistWithinGroup
 *
 *       ex)
 *       if ( aNode->node.module == &mtfRank )
 *       {
 *           sModule = &mtfRankWithinGroup;
             sChanged = ID_TRUE;
 *       }
 *       else if ( aNode->node.module == &mtfPercentRank )
 *       {
 *           sModule = &mtfPercentRankWithinGroup;
 *           sChanged = ID_TRUE;
 *       }
 *       else if ( aNode->node.module == &mtfCumeDist )
 *       {
 *           sModule = &mtfCumeDistWithinGroup;
 *           sChanged = ID_TRUE;
 *       }
 *
 * Implementation :
 *
 **************************************************************************/

    const mtfModule *   sModule;
    idBool              sChanged;
    ULong               sNodeArgCnt;
    qcuSqlSourceInfo    sqlInfo;

    sChanged  = ID_FALSE;

    if ( aNode->node.module == &mtfRank )
    {
        sModule = &mtfRankWithinGroup;
        sChanged = ID_TRUE;
    }
    else
    {
        // Nothing to do
    }

    if ( sChanged == ID_TRUE )
    {
        // ���� ���� ���
        sNodeArgCnt = aNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

        // ���� module Flag ����
        aNode->node.lflag &= ~(aNode->node.module->lflag);

        // �� module flag ��ġ
        aNode->node.lflag |= sModule->lflag & ~MTC_NODE_ARGUMENT_COUNT_MASK;

        // ����� ���� ���� ����
        aNode->node.lflag |= sNodeArgCnt;

        aNode->node.module = sModule;
    }
    else
    {
        // Nothing to do
    }

    if ( aOver != NULL )
    {
        if ( qtc::canHasOverClause( aNode ) == ID_FALSE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & aNode->position );
            IDE_RAISE( ERR_NOT_ALLOWED_OVER_CLAUSE );
        }
        else
        {
            // Nothing to do
        }
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOWED_OVER_CLAUSE );
    {
        (void)sqlInfo.init( QC_QMP_MEM( aStatement ) );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_OVER_CLAUSE_NOT_ALLOWED,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qtc::canHasOverClause( qtcNode * aNode )
{
/***********************************************************************
 *
 * Description :
 *
 *    Over (..) ������ ����� �� �ִ� Within Group �Լ����� Ȯ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool  sAllowed = ID_TRUE;

    if ( (aNode->node.module == &mtfRankWithinGroup) ||
         (aNode->node.module == &mtfPercentRankWithinGroup) ||
         (aNode->node.module == &mtfCumeDistWithinGroup) )
    {
        sAllowed = ID_FALSE;
    }
    else
    {
        // Nothing to do
    }

    return sAllowed;
}

IDE_RC qtc::getBigint( SChar*          aStmtText,
                       SLong*          aValue,
                       qcNamePosition* aPosition )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� String�� �̿��� BIGINT Value�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::getBigint"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    extern mtdModule mtdBigint;

    mtcColumn sColumn;
    UInt      sOffset;
    IDE_RC    sResult;

    sOffset = 0;

    // To Fix BUG-12607
    IDE_TEST( mtc::initializeColumn( & sColumn,
                                     & mtdBigint,
                                     0,           // arguments
                                     0,           // precision
                                     0 )          // scale
              != IDE_SUCCESS );

    IDE_TEST( mtdBigint.value( NULL,
                               &sColumn,
                               (void*)aValue,
                               &sOffset,
                               ID_SIZEOF(SLong),
                               (void*)(aStmtText+aPosition->offset),
                               aPosition->size,
                               &sResult )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResult != IDE_SUCCESS, ERR_NOT_APPLICABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_FATAL_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::fixAfterValidation( iduVarMemList * aMemory,
                                qcTemplate    * aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Validation �Ϸ� ���� ó��
 *
 * Implementation :
 *
 *    Intermediate Tuple�� ���Ͽ� row ������ �Ҵ�޴´�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::fixAfterValidation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UShort        sRow     = 0;
    UShort        sColumn  = 0;
    UInt          sOffset  = 0;
    ULong         sRowSize = ID_ULONG(0);

    void        * sRowPtr;
    mtdDateType * sValue;
    mtcColumn   * sSysdateColumn;

    for( sRow = 0; sRow < aTemplate->tmplate.rowCount; sRow++ )
    {
        if( ( aTemplate->tmplate.rows[sRow].lflag &
              ( MTC_TUPLE_ROW_SKIP_MASK | MTC_TUPLE_TYPE_MASK ) ) ==
            ( MTC_TUPLE_ROW_SKIP_FALSE | MTC_TUPLE_TYPE_INTERMEDIATE ) )
        {
            aTemplate->tmplate.rows[sRow].lflag |= MTC_TUPLE_ROW_SKIP_TRUE;

            for( sColumn = 0, sOffset = 0;
                 sColumn < aTemplate->tmplate.rows[sRow].columnCount;
                 sColumn++ )
            {
                if( aTemplate->tmplate.rows[sRow].columns[sColumn].module != NULL )
                {
                    sOffset = idlOS::align( sOffset,
                                            aTemplate->tmplate.rows[sRow].
                                            columns[sColumn].module->align );
                }
                else
                {
                    // ���ν����� 0�� tmplate���� �ʱ�ȭ ���� ���� �÷���
                    // ������ �� �ִ�.
                    // �÷� �Ҵ�ÿ� cralloc�� ����ϱ� ������ module��
                    // NULL�� ���� ���� �ȴ�.

                    // ��)
                    // create or replace procedure proc1 as
                    // v1 integer;
                    // v2 integer;
                    // begin
                    // select * from t1 where a = v1 and b = v2;
                    // end;
                    // /
                    continue;
                }
                aTemplate->tmplate.rows[sRow].columns[sColumn].column.offset =
                        sOffset;

                sRowSize = (ULong)sOffset + (ULong)aTemplate->tmplate.rows[sRow].columns[sColumn].column.size;
                IDE_TEST_RAISE( sRowSize > (ULong)ID_UINT_MAX, ERR_EXCEED_TUPLE_ROW_MAX_SIZE );

                sOffset = (UInt)sRowSize;

                // To Fix PR-8528
                // ���Ƿ� �����Ǵ� Tuple�� ��� Column ID�� ���Ƿ� �����Ѵ�.
                // ���� ������ �� ���� Table�� ID(0)�� �������� �����Ѵ�.
                aTemplate->tmplate.rows[sRow].columns[sColumn].column.id =
                        sColumn;
            }

            aTemplate->tmplate.rows[sRow].rowOffset  =
            aTemplate->tmplate.rows[sRow].rowMaximum = sOffset;

            if( sOffset != 0 )
            {
                // BUG-15548
                IDU_LIMITPOINT("qtc::fixAfterValidation::malloc2");
                IDE_TEST(
                    aMemory->cralloc(
                        sOffset,
                        (void**)&(aTemplate->tmplate.rows[sRow].row))
                    != IDE_SUCCESS);
            }
            else
            {
                aTemplate->tmplate.rows[sRow].row = NULL;
            }
        }
    }

    aTemplate->tmplate.rowCountBeforeBinding = aTemplate->tmplate.rowCount;

    if( aTemplate->sysdate != NULL )
    {
        // fix BUG-10719 UMR
        sRowPtr = aTemplate->tmplate.rows[aTemplate->sysdate->node.table].row;
        sSysdateColumn = &(aTemplate->tmplate.
                           rows[aTemplate->sysdate->node.table].
                           columns[aTemplate->sysdate->node.column]);
        sValue = (mtdDateType*)mtc::value(
            sSysdateColumn, sRowPtr, MTD_OFFSET_USE );
        sValue->year         = 0;
        sValue->mon_day_hour = 0;
        sValue->min_sec_mic  = 0;
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EXCEED_TUPLE_ROW_MAX_SIZE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::fixAfterValidation",
                                  "tuple row size is larger than 4GB" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

void qtc::fixAfterValidationForCreateInvalidView( qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Invalid View�� ���� ó��
 *
 * Implementation :
 *
 *    ���ʿ��� Memory�� �Ҵ���� �ʵ��� Intermediate Tuple �� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    UShort  sRow;

    for( sRow = 0; sRow < aTemplate->tmplate.rowCount; sRow++ )
    {
        if( ( aTemplate->tmplate.rows[sRow].lflag &
              ( MTC_TUPLE_ROW_SKIP_MASK | MTC_TUPLE_TYPE_MASK ) ) ==
            ( MTC_TUPLE_ROW_SKIP_FALSE | MTC_TUPLE_TYPE_INTERMEDIATE ) )
        {
            /* BUG-40858 */
            if (aTemplate->sysdate != NULL)
            {
                if (aTemplate->sysdate->node.table != sRow)
                {
                    aTemplate->tmplate.rows[sRow].columnCount = 0;
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                aTemplate->tmplate.rows[sRow].columnCount = 0;
            }
        }
    }
}

IDE_RC qtc::setVariableTupleRowSize( qcTemplate    * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-2163
 *
 *    Type Binding �Ϸ� ���� ó��
 *
 * Implementation :
 *
 *    Variable Tuple �� row �� offset �� max size �� �����Ѵ�.
 *
 ***********************************************************************/

    UShort  sRow;
    UShort  sColumn;
    UInt    sOffset;

#ifdef DEBUG
    UShort  sCount;

    for( sRow = 0, sCount = 0; sRow < aTemplate->tmplate.rowCount; sRow++ )
    {
        if( ( aTemplate->tmplate.rows[sRow].lflag & MTC_TUPLE_TYPE_MASK ) ==
                  MTC_TUPLE_TYPE_VARIABLE )
        {
            sCount++;
        }
    }

    // Variable Tuple row count �� �׻� 1 �̴�.
    IDE_DASSERT( sCount == 1 );
#endif

    sRow = aTemplate->tmplate.variableRow;

    for( sColumn = 0, sOffset = 0;
         sColumn < aTemplate->tmplate.rows[sRow].columnCount;
         sColumn++ )
    {
        IDE_TEST_RAISE( aTemplate->tmplate.rows[sRow].
                        columns[sColumn].module == NULL,
                        ERR_UNINITIALIZED_MODULE );

        sOffset = idlOS::align(
            sOffset,
            aTemplate->tmplate.rows[sRow].columns[sColumn].module->align );
        aTemplate->tmplate.rows[sRow].columns[sColumn].column.offset =
            sOffset;
        sOffset +=
            aTemplate->tmplate.rows[sRow].columns[sColumn].column.size;
    }

    aTemplate->tmplate.rows[sRow].rowOffset =
        aTemplate->tmplate.rows[sRow].rowMaximum = sOffset;

    aTemplate->tmplate.rows[sRow].row = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNINITIALIZED_MODULE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::setVariableTupleRowSize",
                                  "uninitialied module" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::estimateNode( qcStatement* aStatement,
                          qtcNode*     aNode )
{
/***********************************************************************
 *
 * Description :
 *    ����� �ʿ信 ���� �߰��Ǵ� Node�鿡 ���� Estimation�� �����Ѵ�.
 *    ���� ���, Indirect Node, Prior Node ���� �̿� �ش��Ѵ�.
 *
 * Implementation :
 *    TODO - ����� Private Function���� ������ �Լ��̴�.
 *    �ܺο��� ������� �ʵ��� �ؾ� ��.
 *    ��ü �Լ��� ���� �� �Լ� �� �ϳ��� �����Ͽ� ����ϵ��� �ؾ� ��.
 *        - qtc::estimateNodeWithoutArgument()
 *        - qtc::estimateNodeWithArgument()
 *
 ***********************************************************************/

#define IDE_FN "qtc::estimateNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qtc::estimateNode"));

    qtcCallBackInfo sCallBackInfo = {
        QC_SHARED_TMPLATE(aStatement),
        QC_QMP_MEM(aStatement),
        NULL,
        NULL,
        NULL,
        NULL
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,
        MTC_ESTIMATE_ARGUMENTS_ENABLE, // TODO - DISABLE�� ó���ؾ� �ҵ�.
        qtc::alloc,
        NULL
    };

    IDE_TEST( qtc::estimateNode( aNode,
                                 & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stack,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount,
                                 &sCallBack )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::estimateNodeWithoutArgument( qcStatement* aStatement,
                                         qtcNode*     aNode )
{
/***********************************************************************
 *
 * Description :
 *    Argument�� ������ ���� ���� Estimate �� �����Ѵ�.
 *    AND, OR��� ���� Argument�� ������ ū �ǹ̸� ���� �ʴ�
 *    Node���� ������ ��� �̸� ȣ���Ѵ�.
 *
 * Implementation :
 *
 *    TODO - ���� ���� �ʿ�
 *    Argument�� �����ϱ� �ʱ� ������ Conversion���� �߻��� �� ����.
 *
 ***********************************************************************/

#define IDE_FN "qtc::estimateNodeWithoutArgument"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qtc::estimateNodeWithoutArgument"));

    qtcCallBackInfo sCallBackInfo = {
        QC_SHARED_TMPLATE(aStatement),  // Template
        QC_QMP_MEM(aStatement),   // Memory ������
        NULL,                 // Statement
        NULL,                 // Query Set
        NULL,                 // SFWGH
        NULL                  // From ����
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,                  // CallBack ����
        MTC_ESTIMATE_ARGUMENTS_DISABLE,  // Child�� ���� Estimation Disable
        qtc::alloc,                      // Memory �Ҵ� �Լ�
        initConversionNodeIntermediate   // Conversion Node ���� �Լ�
        // TODO - Conversion�� �߻��� �� ����.
        // ����, Conversion Node ���� �Լ����� ��� NULL�� Setting�Ǿ��
        // �� ������ ��Ȯ�ϴ�.
    };

    qtcNode * sNode;
    UInt      sArgCnt;
    ULong     sLflag;

    // Argument�� �̿��� Dependencies �� Flag ����
    qtc::dependencyClear( & aNode->depInfo );

    // sLflag �ʱ�ȭ
    sLflag = MTC_NODE_BIND_TYPE_TRUE;

    for ( sNode = (qtcNode *) aNode->node.arguments, sArgCnt = 0;
          sNode != NULL;
          sNode = (qtcNode *) sNode->node.next, sArgCnt++ )
    {
        aNode->node.lflag |=
            sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

        // PROJ-1404
        // variable built-in function�� ����� ��� �����Ѵ�.
        if ( ( sNode->node.lflag & MTC_NODE_VARIABLE_MASK )
             == MTC_NODE_VARIABLE_TRUE )
        {
            aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
            aNode->lflag |= QTC_NODE_VAR_FUNCTION_EXIST;
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1492
        // Argument�� bind���� lflag�� ��� ������.
        if( ( sNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST )
        {
            if( ( sNode->node.lflag & MTC_NODE_BIND_TYPE_MASK ) ==
                MTC_NODE_BIND_TYPE_FALSE )
            {
                sLflag &= ~MTC_NODE_BIND_TYPE_MASK;
                sLflag |= MTC_NODE_BIND_TYPE_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        // Argument�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( dependencyOr( & aNode->depInfo,
                                & sNode->depInfo,
                                & aNode->depInfo )
                  != IDE_SUCCESS );
    }

    if( ( ( aNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) &&
        ( QTC_IS_TERMINAL( aNode ) == ID_FALSE ) )
    {
        // PROJ-1492
        // ���� ��尡 �ִ� �ش� Node�� bind���� lflag�� �����Ѵ�.
        aNode->node.lflag |= sLflag & MTC_NODE_BIND_TYPE_MASK;

        if( aNode->node.module == & mtfCast )
        {
            aNode->node.lflag |= MTC_NODE_BIND_TYPE_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    aNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    if ( (aNode->node.module->lflag & MTC_NODE_LOGICAL_CONDITION_MASK)
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        aNode->node.lflag |= 1;
    }
    else
    {
        aNode->node.lflag |= sArgCnt;
    }

    IDE_TEST( qtc::estimateNode(aNode,
                                & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                QC_SHARED_TMPLATE(aStatement)->tmplate.stack,
                                QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount,
                                &sCallBack )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::estimateNodeWithArgument( qcStatement* aStatement,
                                      qtcNode*     aNode )
{
/***********************************************************************
 *
 * Description :
 *    �̹� estimate �� Argument�� ������ �̿��Ͽ� Estimate �Ѵ�.
 *    Argument�� ���Ͽ� �߰����� Conversion�� �ʿ��� ��� ����Ѵ�.
 *
 * Implementation :
 *
 *    �̹� estimate�� ������ �̿��Ͽ� ó���ϱ� ������,
 *    Argument�� ���Ͽ� traverse�ϸ鼭 ó������ �ʴ´�.
 *    TODO - ���� Binding�� ���� ����� �ʿ��ϴ�.
 *
 ***********************************************************************/

#define IDE_FN "qtc::estimateNodeWithArgument"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qtc::estimateNodeWithArgument"));

    qtcCallBackInfo sCallBackInfo = {
        QC_SHARED_TMPLATE(aStatement),  // Template
        QC_QMP_MEM(aStatement),   // Memory ������
        NULL,                 // Statement
        NULL,                 // Query Set
        NULL,                 // SFWGH
        NULL                  // From ����
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,                  // CallBack ����
        MTC_ESTIMATE_ARGUMENTS_ENABLE,   // Child�� ���� Estimation�� �ʿ���
        qtc::alloc,                      // Memory �Ҵ� �Լ�
        initConversionNodeIntermediate   // Conversion Node ���� �Լ�
    };

    qmsParseTree      * sParseTree;
    qmsSFWGH          * sSFWGH;
    qtcNode           * sNode;
    qtcNode           * sOrgNode;
    mtcStack          * sStack;
    mtcStack          * sOrgStack;
    SInt                sRemain;
    SInt                sOrgRemain;
    UInt                sArgCnt;
    ULong               sLflag;

    // estimate arguments
    for( sNode  = (qtcNode*)aNode->node.arguments,
             sStack = (QC_SHARED_TMPLATE(aStatement)->tmplate.stack) + 1,
             sRemain = (QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount) - 1;
         sNode != NULL;
         sNode  = (qtcNode*)sNode->node.next, sStack++, sRemain-- )
    {
        // BUG-33674
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );
        
        // INDIRECT �� �ִ� ��� ���� Node�� column ������ ����ؾ� ��
        // TODO - Argument�� �̹� Conversion�� ���� ��쿡 ���� ����� ����
        for( sOrgNode = sNode;
             ( sOrgNode->node.lflag & MTC_NODE_INDIRECT_MASK )
                 == MTC_NODE_INDIRECT_TRUE;
             sOrgNode = (qtcNode *) sOrgNode->node.arguments ) ;

        // PROJ-1718 Subquery unnesting
        // List type�� argument�� ��� �ݵ�� argument�� estimate�ؾ� �Ѵ�.
        if( QTC_IS_LIST( sOrgNode ) == ID_TRUE )
        {
            sOrgStack  = QC_SHARED_TMPLATE(aStatement)->tmplate.stack;
            sOrgRemain = QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount;

            QC_SHARED_TMPLATE(aStatement)->tmplate.stack      = sStack;
            QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount = sRemain;

            if( sOrgNode->node.module == &mtfList )
            {
                // List
                IDE_TEST( estimateNodeWithArgument( aStatement,
                                                    sOrgNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // BUG-38415 Subquery Argument
                // �� ���, SELECT �����̾�� �Ѵ�.
                IDE_ERROR( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT );

                sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
                sSFWGH     = sParseTree->querySet->SFWGH;

                IDE_TEST( estimateNodeWithSFWGH( aStatement,
                                                 sSFWGH,
                                                 sOrgNode )
                          != IDE_SUCCESS );
            }

            QC_SHARED_TMPLATE(aStatement)->tmplate.stack      = sOrgStack;
            QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount = sOrgRemain;
        }
        else
        {
            // Nothing to do.
        }

        sStack->column =
            QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sOrgNode->node.table].columns
            + sOrgNode->node.column;
    }

    // Argument�� �̿��� Dependencies �� Flag ����
    qtc::dependencyClear( & aNode->depInfo );

    // sLflag �ʱ�ȭ
    sLflag = MTC_NODE_BIND_TYPE_TRUE;

    for ( sNode = (qtcNode *) aNode->node.arguments, sArgCnt = 0;
          sNode != NULL;
          sNode = (qtcNode *) sNode->node.next, sArgCnt++ )
    {
        aNode->node.lflag |=
            sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

        // PROJ-1404
        // variable built-in function�� ����� ��� �����Ѵ�.
        if ( ( sNode->node.lflag & MTC_NODE_VARIABLE_MASK )
             == MTC_NODE_VARIABLE_TRUE )
        {
            aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
            aNode->lflag |= QTC_NODE_VAR_FUNCTION_EXIST;
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1492
        // Argument�� bind���� lflag�� ��� ������.
        if( ( sNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST )
        {
            if( ( sNode->node.lflag & MTC_NODE_BIND_TYPE_MASK ) ==
                MTC_NODE_BIND_TYPE_FALSE )
            {
                sLflag &= ~MTC_NODE_BIND_TYPE_MASK;
                sLflag |= MTC_NODE_BIND_TYPE_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        // Argument�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( dependencyOr( & aNode->depInfo,
                                & sNode->depInfo,
                                & aNode->depInfo )
                  != IDE_SUCCESS );
    }

    if( ( ( aNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) &&
        ( QTC_IS_TERMINAL( aNode ) == ID_FALSE ) )
    {
        // PROJ-1492
        // ���� ��尡 �ִ� �ش� Node�� bind���� lflag�� �����Ѵ�.
        aNode->node.lflag |= sLflag & MTC_NODE_BIND_TYPE_MASK;

        if( aNode->node.module == & mtfCast )
        {
            aNode->node.lflag |= MTC_NODE_BIND_TYPE_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    aNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;

    if ( (aNode->node.module->lflag & MTC_NODE_LOGICAL_CONDITION_MASK)
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        aNode->node.lflag |= 1;
    }
    else
    {
        aNode->node.lflag |= sArgCnt;
    }

    // estimate root node
    IDE_TEST( qtc::estimateNode( aNode,
                                 & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stack,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount,
                                 &sCallBack )
              != IDE_SUCCESS);

    // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
    // estimateNode �� ����� ���� �� �������־�� �Ѵ�.
    // Ex : LENGTH( lobColumn )
    aNode->lflag &= ~QTC_NODE_BINARY_MASK;

    if ( isEquiValidType( aNode, & QC_SHARED_TMPLATE(aStatement)->tmplate ) == ID_FALSE )
    {
        aNode->lflag |= QTC_NODE_BINARY_EXIST;
    }
    else
    {
        aNode->lflag |= QTC_NODE_BINARY_ABSENT;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::estimateWindowFunction( qcStatement * aStatement,
                                    qmsSFWGH    * aSFWGH,
                                    qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     Transformation �������� ���� ������ window function��
 *     estimation�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcCallBackInfo sCallBackInfo = {
        QC_SHARED_TMPLATE(aStatement),  // Template
        QC_QMP_MEM(aStatement),   // Memory ������
        NULL,                 // Statement
        aSFWGH->thisQuerySet, // Query Set
        NULL,                 // SFWGH
        NULL                  // From ����
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,                  // CallBack ����
        MTC_ESTIMATE_ARGUMENTS_ENABLE,   // Child�� ���� Estimation�� �ʿ���
        qtc::alloc,                      // Memory �Ҵ� �Լ�
        initConversionNodeIntermediate   // Conversion Node ���� �Լ�
    };

    qtcNode         * sNode;
    qtcNode         * sOrgNode;
    mtcStack        * sStack;
    qmsProcessPhase   sProcessPhase;
    SInt              sRemain;
    UInt              sArgCnt;
    ULong             sLflag;

    IDE_ERROR( aNode->overClause != NULL );
    IDE_ERROR( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                    == MTC_NODE_OPERATOR_AGGREGATION );

    for( sNode  = (qtcNode*)aNode->node.arguments,
             sStack = (QC_SHARED_TMPLATE(aStatement)->tmplate.stack) + 1,
             sRemain = (QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount) - 1;
         sNode != NULL;
         sNode  = (qtcNode*)sNode->node.next, sStack++, sRemain-- )
    {
        // BUG-33674
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );
        
        // INDIRECT �� �ִ� ��� ���� Node�� column ������ ����ؾ� ��
        // TODO - Argument�� �̹� Conversion�� ���� ��쿡 ���� ����� ����
        for( sOrgNode = sNode;
             ( sOrgNode->node.lflag & MTC_NODE_INDIRECT_MASK )
                 == MTC_NODE_INDIRECT_TRUE;
             sOrgNode = (qtcNode *) sOrgNode->node.arguments ) ;

        sStack->column =
            QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sOrgNode->node.table].columns
            + sOrgNode->node.column;
    }

    // Argument�� �̿��� Dependencies �� Flag ����
    qtc::dependencyClear( & aNode->depInfo );

    // sLflag �ʱ�ȭ
    sLflag = MTC_NODE_BIND_TYPE_TRUE;

    for ( sNode = (qtcNode *) aNode->node.arguments, sArgCnt = 0;
          sNode != NULL;
          sNode = (qtcNode *) sNode->node.next, sArgCnt++ )
    {
        aNode->node.lflag |=
            sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

        // PROJ-1404
        // variable built-in function�� ����� ��� �����Ѵ�.
        if ( ( sNode->node.lflag & MTC_NODE_VARIABLE_MASK )
             == MTC_NODE_VARIABLE_TRUE )
        {
            aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
            aNode->lflag |= QTC_NODE_VAR_FUNCTION_EXIST;
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1492
        // Argument�� bind���� lflag�� ��� ������.
        if( ( sNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST )
        {
            if( ( sNode->node.lflag & MTC_NODE_BIND_TYPE_MASK ) ==
                MTC_NODE_BIND_TYPE_FALSE )
            {
                sLflag &= ~MTC_NODE_BIND_TYPE_MASK;
                sLflag |= MTC_NODE_BIND_TYPE_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }

        // Argument�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( dependencyOr( & aNode->depInfo,
                                & sNode->depInfo,
                                & aNode->depInfo )
                  != IDE_SUCCESS );
    }

    if( ( ( aNode->node.lflag & MTC_NODE_BIND_MASK ) == MTC_NODE_BIND_EXIST ) &&
        ( QTC_IS_TERMINAL( aNode ) == ID_FALSE ) )
    {
        // PROJ-1492
        // ���� ��尡 �ִ� �ش� Node�� bind���� lflag�� �����Ѵ�.
        aNode->node.lflag |= sLflag & MTC_NODE_BIND_TYPE_MASK;

        if( aNode->node.module == & mtfCast )
        {
            aNode->node.lflag |= MTC_NODE_BIND_TYPE_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    aNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    aNode->node.lflag |= sArgCnt;

    // estimate root node
    IDE_TEST( qtc::estimateNode( aNode,
                                 & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stack,
                                 QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount,
                                 &sCallBack )
              != IDE_SUCCESS);

    // Window function�� SELECT/ORDER BY�������� ��� �����ϹǷ�
    // �Ͻ������� ������ validation �ܰ踦 SELECT���� ��ȯ�Ѵ�.
    sProcessPhase = aSFWGH->thisQuerySet->processPhase;
    aSFWGH->thisQuerySet->processPhase = QMS_VALIDATE_TARGET;

    IDE_TEST( estimate4OverClause( aNode,
                                   & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                   QC_SHARED_TMPLATE(aStatement)->tmplate.stack,
                                   QC_SHARED_TMPLATE(aStatement)->tmplate.stackCount,
                                   &sCallBack )
              != IDE_SUCCESS );

    aNode->lflag &= ~QTC_NODE_ANAL_FUNC_MASK;
    aNode->lflag |= QTC_NODE_ANAL_FUNC_EXIST;

    IDE_TEST( estimateAggregation( aNode, &sCallBack )
              != IDE_SUCCESS );

    aSFWGH->thisQuerySet->processPhase = sProcessPhase;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::estimateConstExpr( qtcNode     * aNode,
                               qcTemplate  * aTemplate,
                               qcStatement * aStatement )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                �ֻ��� ��忡 ���� constant expressionó��
 *
 *  Implementation :
 *                �ֻ��� ��尡 aggregation�� �ƴ� ���
 *                constant expressionó���� �����ϸ� ó��
 *
 ***********************************************************************/

    qtcNode           * sResultNode;
    qtcNode           * sOrgNode;
    idBool              sAbleProcess;

    qtcCallBackInfo sCallBackInfo = {
        aTemplate,
        QC_QMP_MEM(aTemplate->stmt),
        aStatement,
        NULL,
        NULL,
        NULL
    };

    mtcCallBack sCallBack = {
        &sCallBackInfo,
        MTC_ESTIMATE_ARGUMENTS_ENABLE,
        alloc,
        initConversionNodeIntermediate
    };

    if ( ( MTC_NODE_IS_DEFINED_VALUE( & aNode->node ) == ID_TRUE )
         &&
         (aNode->subquery == NULL) // : A4 only
         // ( (aNode->flag & QTC_NODE_SUBQUERY_MASK )
         //   != QTC_NODE_SUBQUERY_EXIST ) // : A3 only
         // PSM function
         &&
         (aNode->node.module != &mtfList) )
    {
        // Argument�� ���ռ� �˻�
        IDE_TEST( isConstExpr( aTemplate, aNode, &sAbleProcess )
                  != IDE_SUCCESS );

        if ( sAbleProcess == ID_TRUE )
        {
            // ���ȭ�ϱ� �� ���� ��带 ��� �Ѵ�.
            IDU_LIMITPOINT("qtc::estimateConstExpr::malloc");
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qtcNode, & sOrgNode )
                      != IDE_SUCCESS);

            idlOS::memcpy( sOrgNode, aNode, ID_SIZEOF(qtcNode) );

            // ��� Expression�� ������
            IDE_TEST( runConstExpr( aStatement,
                                    aTemplate,
                                    aNode,
                                    &aTemplate->tmplate,
                                    aTemplate->tmplate.stack,
                                    aTemplate->tmplate.stackRemain,
                                    &sCallBack,
                                    & sResultNode ) != IDE_SUCCESS );

            sResultNode->node.next = aNode->node.next;
            sResultNode->node.conversion = NULL;

//          sResultNode->node.arguments = aNode->node.arguments;
            sResultNode->node.arguments = NULL;
            sResultNode->node.leftConversion =
                aNode->node.leftConversion;
            sResultNode->node.orgNode =
                (mtcNode*) sOrgNode;
            sResultNode->node.funcArguments =
                aNode->node.funcArguments;
            sResultNode->node.baseTable =
                aNode->node.baseTable;
            sResultNode->node.baseColumn =
                aNode->node.baseColumn;

            //fix BUG-18119
            if( aNode != sResultNode )
            {
                idlOS::memcpy(aNode, sResultNode, ID_SIZEOF(qtcNode));
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



IDE_RC qtc::estimate( qtcNode*     aNode,
                      qcTemplate*  aTemplate,
                      qcStatement* aStatement,
                      qmsQuerySet* aQuerySet,
                      qmsSFWGH*    aSFWGH,
                      qmsFrom*     aFrom )
{
/***********************************************************************
 *
 * Description :
 *
 *    Node Tree�� ���� Estimation�� ������.
 *
 * Implementation :
 *
 *    Node Tree�� ��� Traverse�ϸ鼭 Estimate �� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::estimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcCallBackInfo sCallBackInfo = {
        aTemplate,
        QC_QMP_MEM(aTemplate->stmt),
        aStatement,
        aQuerySet,
        aSFWGH,
        aFrom
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,
        MTC_ESTIMATE_ARGUMENTS_ENABLE,
        alloc,
        initConversionNodeIntermediate
    };

    IDE_TEST( estimateInternal( aNode,
                                & aTemplate->tmplate,
                                aTemplate->tmplate.stack,
                                aTemplate->tmplate.stackRemain,
                                &sCallBack )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::getSortColumnPosition( qmsSortColumns* aSortColumn,
                                   qcTemplate*     aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    ORDER BY indicator�� ���� ������ ������.
 *
 * Implementation :
 *
 *    ORDER BY Expression �� value integer�� ���,
 *    �̸� Indicator�� �ؼ��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::getSortColumnPosition"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    extern mtdModule mtdSmallint;

    qtcNode*         sNode;
    const mtcColumn* sColumn;

    aSortColumn->targetPosition = -1;
    sNode                       = aSortColumn->sortColumn;

    if ( isConstValue( aTemplate, sNode ) == ID_TRUE )
    {
        sColumn = QTC_TMPL_COLUMN( aTemplate, sNode );
        
        if( sColumn->module == &mtdSmallint )
        {
            // To Fix PR-8015
            aSortColumn->targetPosition =
                *(SShort*)QTC_TMPL_FIXEDDATA( aTemplate, sNode );
        }
        else 
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

#undef IDE_FN
}


IDE_RC qtc::initialize( qtcNode*    aNode,
                        qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Aggregation Node�� �ʱ�ȭ�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::initialize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // BUG-33674
    IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

    // BUG-34321 return value optimization
    return aTemplate->tmplate.rows[aNode->node.table].
        execute[aNode->node.column].
        initialize( (mtcNode*)aNode,
                    aTemplate->tmplate.stack,
                    aTemplate->tmplate.stackRemain,
                    aTemplate->tmplate.rows[aNode->node.table].
                        execute[aNode->node.column].
                        calculateInfo,
                    &aTemplate->tmplate );

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::aggregate( qtcNode*    aNode,
                       qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Aggregation Node�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::aggregate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // BUG-33674
    IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

    // BUG-34321 return value optimization
    return aTemplate->tmplate.rows[aNode->node.table].
        execute[aNode->node.column].
        aggregate( (mtcNode*)aNode,
                   aTemplate->tmplate.stack,
                   aTemplate->tmplate.stackRemain,
                   aTemplate->tmplate.rows[aNode->node.table].
                       execute[aNode->node.column].
                       calculateInfo,
                   &aTemplate->tmplate );

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::aggregateWithInfo( qtcNode*    aNode,
                               void*       aInfo,
                               qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Aggregation Node�� ���� ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::aggregate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // BUG-33674
    IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

    // BUG-34321 return value optimization
    return aTemplate->tmplate.rows[aNode->node.table].
        execute[aNode->node.column].
        aggregate( (mtcNode*)aNode,
                   aTemplate->tmplate.stack,
                   aTemplate->tmplate.stackRemain,
                   aInfo,
                   &aTemplate->tmplate );

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::finalize( qtcNode*    aNode,
                      qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Aggregation Node�� ������ �۾��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::finalize"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // BUG-33674
    IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

    // BUG-34321 return value optimization
    return aTemplate->tmplate.rows[aNode->node.table].
        execute[aNode->node.column].
        finalize( (mtcNode*)aNode,
                  aTemplate->tmplate.stack,
                  aTemplate->tmplate.stackRemain,
                  aTemplate->tmplate.rows[aNode->node.table].
                      execute[aNode->node.column].
                      calculateInfo,
                  &aTemplate->tmplate );

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::calculate(qtcNode* aNode, qcTemplate* aTemplate)
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Node�� ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    �ش� Node�� ������ �����ϰ�,
 *    Conversion�� ���� ���, Conversion ó���� �����Ѵ�.
 *
 ***********************************************************************/

    /*
      ���ν��� ����ÿ� ���̺��� �÷� ������ �پ�� ��� ���� ó���Ѵ�.
      Assign ���� ��� �������� ����ȯ�� �̷�� ���� ������ ȣȯ���� �ʴ�
      Ÿ������ ����Ǵ� ��� �����߿� ������ �߻��Ѵ�.

      ��)
      drop table t2;
      create table t2 ( a integer, b integer );

      create or replace procedure proc1
      as
      v1 t2%rowtype;
      begin

      v1.a := 1;
      v1.b := 999;

      println( v1.a );
      println( v1.b );

      end;
      /

      drop table t2;
      create table t2 ( a integer );

      exec proc1;
    */

    // BUG-33674
    IDE_TEST_RAISE( aTemplate->tmplate.stackRemain < 1, ERR_STACK_OVERFLOW );

    if( aNode->node.column != MTC_RID_COLUMN_ID )
    {
        IDE_TEST_RAISE(aNode->node.column >=
                       aTemplate->tmplate.rows[aNode->node.table].columnCount,
                       ERR_MODIFIED);

        IDE_TEST(aTemplate->tmplate.rows[aNode->node.table].
                 execute[aNode->node.column].calculate(
                     (mtcNode*)aNode,
                     aTemplate->tmplate.stack,
                     aTemplate->tmplate.stackRemain,
                     aTemplate->tmplate.rows[aNode->node.table].
                         execute[aNode->node.column].
                         calculateInfo,
                     &aTemplate->tmplate)
                 != IDE_SUCCESS);
    }
    else
    {
        /* rid pseudo column */
        IDE_TEST(aTemplate->tmplate.rows[aNode->node.table].
                 ridExecute->calculate((mtcNode*)aNode,
                                       aTemplate->tmplate.stack,
                                       aTemplate->tmplate.stackRemain,
                                       NULL,
                                       &aTemplate->tmplate)
                 != IDE_SUCCESS);
    }

    if (aNode->node.conversion != NULL )
    {
        IDE_TEST( mtf::convertCalculate( (mtcNode*)aNode,
                                         aTemplate->tmplate.stack,
                                         aTemplate->tmplate.stackRemain,
                                         NULL,
                                         &aTemplate->tmplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MODIFIED );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_NOT_EXISTS_COLUMN ) );
    }
    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::judge( idBool*     aJudgement,
                   qtcNode*    aNode,
                   qcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    �ش� Node�� ������ �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 *    �ش� Node�� ������ �����ϰ�,
 *    ������ TRUE������ �Ǵ��Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::judge"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST( calculate( aNode, aTemplate ) != IDE_SUCCESS );

    // BUG-34321 return value optimization
    return aTemplate->tmplate.stack->column->module->isTrue(
                        aJudgement,
                        aTemplate->tmplate.stack->column,
                        aTemplate->tmplate.stack->value );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/* PROJ-2209 DBTIMEZONE */
/***********************************************************************
 *
 * Description :
 *
 *    ���� ������ System Time�� ȹ��.
 *
 * Implementation :
 *
 *    gettimeofday()�� �̿��� System Time�� ȹ���ϰ�,
 *    �̸� Data Type�� ������ ��ȯ��Ŵ
 *
 *    1. unixdate    : gmtime_r() , makeDate()
 *    2. currentdate : gmtime_r() , makeDate() + addSecond( session tz offset )
 *    3. sysdate     : gmtime_r() , makeDate() + addSecond( os tz offset )
 *
 ***********************************************************************/
IDE_RC qtc::setDatePseudoColumn( qcTemplate * aTemplate )
{
    PDL_Time_Value sTimevalue;
    struct tm      sGlobaltime;
    time_t         sTime;
    mtcNode*       sNode;
    mtdDateType*   sDate = NULL;
    mtdDateType    sTmpdate;

    if ( ( aTemplate->unixdate != NULL ) ||
         ( aTemplate->sysdate  != NULL ) ||
         ( aTemplate->currentdate != NULL ) )
    {
        sTimevalue = idlOS::gettimeofday();

        sTime = (time_t)sTimevalue.sec();
        idlOS::gmtime_r( &sTime, &sGlobaltime );

        IDE_TEST( mtdDateInterface::makeDate( &sTmpdate,
                                              (SShort)sGlobaltime.tm_year + 1900,
                                              sGlobaltime.tm_mon + 1,
                                              sGlobaltime.tm_mday,
                                              sGlobaltime.tm_hour,
                                              sGlobaltime.tm_min,
                                              sGlobaltime.tm_sec,
                                              sTimevalue.usec() )
                  != IDE_SUCCESS );

        // set UNIX_DATE
        if ( aTemplate->unixdate != NULL )
        {
            sNode = &aTemplate->unixdate->node;
            sDate = (mtdDateType*)
                        ( (UChar*) aTemplate->tmplate.rows[sNode->table].row
                          + aTemplate->tmplate.rows[sNode->table].columns[sNode->column].column.offset );

            *sDate = sTmpdate;
        }
        else
        {
            /* nothing to do. */
        }

        // set CURRENT_DATE
        if ( aTemplate->currentdate != NULL )
        {
            sNode = &aTemplate->currentdate->node;
            sDate = (mtdDateType*)
                        ( (UChar*) aTemplate->tmplate.rows[sNode->table].row
                          + aTemplate->tmplate.rows[sNode->table].columns[sNode->column].column.offset );

            IDE_TEST( mtdDateInterface::addSecond( sDate,
                                                   &sTmpdate,
                                                   QCG_GET_SESSION_TIMEZONE_SECOND( aTemplate->stmt ),
                                                   0 )
            != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }

        // set SYSDATE
        if ( aTemplate->sysdate != NULL )
        {
            sNode = &aTemplate->sysdate->node;
            sDate = (mtdDateType*)
                        ( (UChar*) aTemplate->tmplate.rows[sNode->table].row
                          + aTemplate->tmplate.rows[sNode->table].columns[sNode->column].column.offset );

            /* PROJ-2207 Password policy support
               sysdate_for_natc�� �����ϸ� sysdate�� ��ȯ�Ѵ�. */
            if ( QCU_SYSDATE_FOR_NATC[0] != '\0' )
            {
                sDate->year         = 0;
                sDate->mon_day_hour = 0;
                sDate->min_sec_mic  = 0;

                IDE_TEST_CONT( mtdDateInterface::toDate( sDate,
                                                         (UChar*)QCU_SYSDATE_FOR_NATC,
                                                         idlOS::strlen(QCU_SYSDATE_FOR_NATC),
                                                         (UChar*)aTemplate->tmplate.dateFormat,
                                                         idlOS::strlen(aTemplate->tmplate.dateFormat) )
                               == IDE_SUCCESS, NORMAL_EXIT );
            }
            else
            {
                /* nothing to do. */
            }

            IDE_TEST( mtdDateInterface::addSecond( sDate,
                                                   &sTmpdate,
                                                   MTU_OS_TIMEZONE_SECOND,
                                                   0 )
                      != IDE_SUCCESS );
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

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qtc::sysdate( mtdDateType* aDate )
{
/***********************************************************************
 *
 * Description :
 *
 *    ���� ������ System Time�� ȹ����.
 *
 * Implementation :
 *
 *    gettimeofday()�� �̿��� System Time�� ȹ���ϰ�,
 *    �̸� Data Type�� ������ ��ȯ��Ŵ
 *
 ***********************************************************************/

#define IDE_FN "void qtc::sysdate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    PDL_Time_Value sTimevalue;
    struct tm      sLocaltime;
    time_t         sTime;
    mtdDateType    sTmpdate;

    sTimevalue = idlOS::gettimeofday();
    sTime      = (time_t)sTimevalue.sec();
    idlOS::gmtime_r( &sTime, &sLocaltime );

    IDE_TEST( mtdDateInterface::makeDate( &sTmpdate,
                                          (SShort)sLocaltime.tm_year + 1900,
                                          sLocaltime.tm_mon + 1,
                                          sLocaltime.tm_mday,
                                          sLocaltime.tm_hour,
                                          sLocaltime.tm_min,
                                          sLocaltime.tm_sec,
                                          sTimevalue.usec() )
              != IDE_SUCCESS );

    IDE_TEST( mtdDateInterface::addSecond( aDate,
                                           &sTmpdate,
                                           MTU_OS_TIMEZONE_SECOND,
                                           0 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC qtc::initSubquery( mtcNode*     aNode,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Subquery Node�� Plan �ʱ�ȭ
 *
 * Implementation :
 *
 *
 ***********************************************************************/
#define IDE_FN "IDE_RC qtc::initSubquery"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmnPlan* sPlan;

    sPlan = ((qtcNode*)aNode)->subquery->myPlan->plan;

    return sPlan->init( (qcTemplate*)aTemplate, sPlan );

#undef IDE_FN
}

IDE_RC qtc::fetchSubquery( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           idBool*      aRowExist )
{
/***********************************************************************
 *
 * Description :
 *
 *    Subquery Node�� Plan ����
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::fetchSubquery"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmnPlan*   sPlan;
    qmcRowFlag sFlag = QMC_ROW_INITIALIZE;

    sPlan = ((qtcNode*)aNode)->subquery->myPlan->plan;

    IDE_TEST( sPlan->doIt( (qcTemplate*)aTemplate, sPlan, &sFlag )
              != IDE_SUCCESS );

    if( ( sFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        *aRowExist = ID_TRUE;
    }
    else
    {
        *aRowExist = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::finiSubquery( mtcNode*     /* aNode */,
                          mtcTemplate* /* aTemplate */)
{
/***********************************************************************
 *
 * Description :
 *
 *    Subquery Node�� Plan ����
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::finiSubquery"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qtc::setCalcSubquery( mtcNode     * aNode,
                             mtcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-2448 Subquery caching
 *
 *    Subquery Node�� calculate �Լ� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_ERROR_RAISE( aNode->arguments != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode->arguments->module == &qtc::subqueryModule, ERR_UNEXPECTED_CACHE_ERROR );

    if ( aNode->module == &mtfExists )
    {
        aTemplate->rows[aNode->arguments->table].execute[aNode->arguments->column].calculate
            = qtcSubqueryCalculateExists;
    }
    else if ( aNode->module == &mtfNotExists )
    {
        aTemplate->rows[aNode->arguments->table].execute[aNode->arguments->column].calculate
            = qtcSubqueryCalculateNotExists;
    }
    else
    {
        IDE_ERROR_RAISE( 0, ERR_UNEXPECTED_CACHE_ERROR );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::setCalcSubquery",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::cloneQTCNodeTree4Partition(
    iduVarMemList * aMemory,
    qtcNode*        aSource,
    qtcNode**       aDestination,
    UShort          aSourceTable,
    UShort          aDestTable,
    idBool          aContainRootsNext )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *    source(partitioned table)�� ��带 destination(partition) ����
 *    ��ȯ�Ͽ� ������
 *
 *    - root�� next�� �������� ����
 *    - conversion�� clear
 *
 * Implementation :
 *
 *    Source Node Tree�� Traverse�ϸ鼭 Node�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_LIMITPOINT("qtc::cloneQTCNodeTree4Partition::malloc");
    IDE_TEST(STRUCT_ALLOC(aMemory, qtcNode, &sNode)
             != IDE_SUCCESS);

    idlOS::memcpy( sNode, aSource, ID_SIZEOF(qtcNode) );

    sNode->node.conversion = NULL;
    sNode->node.leftConversion = NULL;

    if ((sNode->node.module == &qtc::columnModule) ||
        (sNode->node.module == &gQtcRidModule))
    {
        // source�� dest�� �ٸ� ������ ����.
        if( (sNode->node.table == aSourceTable) &&
            (aSourceTable != aDestTable) )
        {
            sNode->node.table = aDestTable;
        }
        else
        {
            // Nothing to do.
        }

        // BUG-27291
        // �̹� estimate�����Ƿ� estimate�� �ʿ����.
        sNode->lflag &= ~QTC_NODE_COLUMN_ESTIMATE_MASK;
        sNode->lflag |= QTC_NODE_COLUMN_ESTIMATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    if( aSource->node.arguments != NULL )
    {
        if( ( aSource->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_SUBQUERY )
        {
            // Subquery����� ��쿣 arguments�� �������� �ʴ´�.
        }
        else
        {
            IDE_TEST( cloneQTCNodeTree4Partition(
                          aMemory,
                          (qtcNode *)aSource->node.arguments,
                          (qtcNode **)(&(sNode->node.arguments)),
                          aSourceTable,
                          aDestTable,
                          ID_TRUE )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    // To fix BUG-21243
    // pass node�� �������� �ʰ� �����Ѵ�.
    // view target column�� �����ϴ� ��� passnode���� ����Ǿ
    // view push selection�� �ϴ� ��� �߸��� ����� ����.
    if( aSource->node.module == &qtc::passModule )
    {
        idlOS::memcpy( sNode,
                       sNode->node.arguments,
                       ID_SIZEOF(qtcNode) );
    }
    else
    {
        // Nothing to do.
    }
    
    if( aSource->node.next != NULL )
    {
        if( aContainRootsNext == ID_TRUE )
        {
            IDE_TEST( cloneQTCNodeTree4Partition(
                          aMemory,
                          (qtcNode *)aSource->node.next,
                          (qtcNode **)(&(sNode->node.next)),
                          aSourceTable,
                          aDestTable,
                          ID_TRUE )
                      != IDE_SUCCESS );
        }
        else
        {
            sNode->node.next = NULL;
        }
    }
    else
    {
        // Nothing to do.
    }
    
    *aDestination = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::cloneQTCNodeTree( iduVarMemList * aMemory,
                              qtcNode       * aSource,
                              qtcNode      ** aDestination,
                              idBool          aContainRootsNext,
                              idBool          aConversionClear,
                              idBool          aConstCopy,
                              idBool          aConstRevoke )
{
/***********************************************************************
 *
 * Description :
 *
 *    Source Node�� Desination���� ������.
 *
 *    - aContainRootsNext: ID_TRUE�̸� aDestination�� next�� ��� ����
 *    - aConversionClear: ID_TRUE�̸� conversion�� ��� NULL�� ����
 *                        ( push selection���� �����)
 *    - aConstCopy       : ID_TRUE�̸� constant node�� �����Ѵ�.
 *    - aConstRevoke     : ID_TRUE�̸� runConstExpr�� constantó���Ǳ�
 *                         ���� ���� ���� �����Ͽ� �����Ѵ�.
 *
 * Implementation :
 *
 *    Source Node Tree�� Traverse�ϸ鼭 Node�� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::cloneQTCNodeTree"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sNode;
    qtcNode * sSrcNode;

    IDE_ASSERT( aSource != NULL );
    
    //--------------------------------------------------
    // clone ��� source node
    //--------------------------------------------------
    if ( ( ( aSource->lflag & QTC_NODE_CONVERSION_MASK )
           == QTC_NODE_CONVERSION_TRUE ) &&
         ( aConstRevoke == ID_TRUE ) )
    {
        //--------------------------------------------------
        // PROJ-1404
        // �̹� constantó���� ���� �����Ͽ� �����Ѵ�.
        //--------------------------------------------------
        IDE_DASSERT( aConstCopy == ID_TRUE );
        IDE_DASSERT( aSource->node.orgNode != NULL );

        sSrcNode = (qtcNode*)aSource->node.orgNode;
    }
    else
    {
        sSrcNode = aSource;
    }
    
    //--------------------------------------------------
    // source node ����
    //--------------------------------------------------
    IDU_LIMITPOINT("qtc::cloneQTCNodeTree::malloc");
    IDE_TEST(STRUCT_ALLOC(aMemory, qtcNode, &sNode) != IDE_SUCCESS);

    idlOS::memcpy( sNode, sSrcNode, ID_SIZEOF(qtcNode) );

    if( aConversionClear == ID_TRUE )
    {
        sNode->node.conversion = NULL;
        sNode->node.leftConversion = NULL;
    }
    else
    {
        // Nothing to do...
    }

    // PROJ-1404
    // column�� ���� �� �� estimate���� �ʴ´�.
    if((sSrcNode->node.module == & qtc::columnModule) ||
       (sSrcNode->node.module == &gQtcRidModule))
    {
        sNode->lflag &= ~QTC_NODE_COLUMN_ESTIMATE_MASK;
        sNode->lflag |= QTC_NODE_COLUMN_ESTIMATE_TRUE;
    }
    else
    {
        // Nothing to do.
    }    

    //--------------------------------------------------
    // source node�� argument�� ������ �̸� ����
    //--------------------------------------------------

    if( sSrcNode->node.arguments != NULL )
    {
        if( ( sSrcNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_SUBQUERY )
        {
            // Subquery����� ��쿣 arguments�� �������� �ʴ´�.
        }
        else
        {
            // BUG-22045
            // aSource->node.arguments->next���� ����ϱ� ���ؼ� aSource��
            // dependency�� �˻��Ѵ�.
            if( ( qtc::dependencyEqual( & sSrcNode->depInfo,
                                        & qtc::zeroDependencies )
                  == ID_FALSE )
                ||
                ( aConstCopy == ID_TRUE ) )
            {
                IDE_TEST( cloneQTCNodeTree( aMemory,
                                            (qtcNode *)sSrcNode->node.arguments,
                                            (qtcNode **)(&(sNode->node.arguments)),
                                            ID_TRUE,
                                            aConversionClear,
                                            aConstCopy,
                                            aConstRevoke )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do...
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // To fix BUG-21243
    // pass node�� �������� �ʰ� �����Ѵ�.
    // view target column�� �����ϴ� ��� passnode���� ����Ǿ
    // view push selection�� �ϴ� ��� �߸��� ����� ����.
    // ������ ���� conversion�� ���� ��츸 passnode�� ����.
    if( (aConversionClear == ID_TRUE) &&
        (sSrcNode->node.module == &qtc::passModule) )
    {
        idlOS::memcpy( sNode, sNode->node.arguments, ID_SIZEOF(qtcNode) );
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------------------
    // source�� next�� ������ �̸� ����
    // 
    // �̶�, constantó���� ��嵵 orgNode�� �ƴ�
    // ���� source�� next�� ���󰡾��Ѵ�.
    // �̴� aSource->node.orgNode�� next�� �ƴ϶�
    // ���� source�� link�� ����Ű �����̴�. ( BUG-22927 )
    //--------------------------------------------------
        
    if( aSource->node.next != NULL )
    {
        if( aContainRootsNext == ID_TRUE )
        {
            if( ( qtc::dependencyEqual(
                      &((qtcNode*)aSource->node.next)->depInfo,
                      & qtc::zeroDependencies )
                  == ID_FALSE )
                ||
                ( aConstCopy == ID_TRUE ) )
            {
                IDE_TEST( cloneQTCNodeTree( aMemory,
                                            (qtcNode *)aSource->node.next,
                                            (qtcNode **)(&(sNode->node.next)),
                                            ID_TRUE,
                                            aConversionClear,
                                            aConstCopy,
                                            aConstRevoke )
                          != IDE_SUCCESS );
            }
            else
            {
                // nothing to do 
            }
        }
        else
        {
            sNode->node.next = NULL;
        }
    }
    else
    {
        sNode->node.next = NULL;
    }

    *aDestination = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::copyNodeTree( qcStatement*    aStatement,
                          qtcNode*        aSource,
                          qtcNode**       aDestination,
                          idBool          aContainRootsNext,
                          idBool          aSubqueryCopy )
{
/***********************************************************************
 *
 * Description : parsing�������� �ʿ��� nodetree�� �����Ѵ�.
 *
 *
 * Implementation : subquery�� ���� ���� ����.
 *
 ***********************************************************************/

    qtcNode          * sNode;
    qtcNode          * sSrcNode;

    IDE_ASSERT( aSource != NULL );
    
    //--------------------------------------------------
    // clone ��� source node
    //--------------------------------------------------
    
    sSrcNode = aSource;

    //--------------------------------------------------
    // source node ����
    //--------------------------------------------------
    
    if ( ( ( sSrcNode->node.lflag & MTC_NODE_OPERATOR_MASK )
           == MTC_NODE_OPERATOR_SUBQUERY )
         &&
         ( aSubqueryCopy == ID_TRUE ) )
    {
        // subquery node ����
        IDE_TEST( copySubqueryNodeTree( aStatement,
                                        sSrcNode,
                                        & sNode )
                  != IDE_SUCCESS );
    }
    else
    {
        IDU_LIMITPOINT("qtc::copyNodeTree::malloc");
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                                qtcNode,
                                &sNode )
                  != IDE_SUCCESS );
        
        idlOS::memcpy( sNode, sSrcNode, ID_SIZEOF(qtcNode) );
    }

    //--------------------------------------------------
    // source node�� argument�� ������ �̸� ����
    //--------------------------------------------------

    if( sSrcNode->node.arguments != NULL )
    {
        IDE_TEST( copyNodeTree( aStatement,
                                (qtcNode *)sSrcNode->node.arguments,
                                (qtcNode **)(&(sNode->node.arguments)),
                                ID_TRUE,
                                aSubqueryCopy )
                  != IDE_SUCCESS );
    }
    else
    {
        sNode->node.arguments = NULL;
    }
    
    //--------------------------------------------------
    // source�� next�� ������ �̸� ����
    //--------------------------------------------------
        
    if( sSrcNode->node.next != NULL )
    {
        if( aContainRootsNext == ID_TRUE )
        {
            IDE_TEST( copyNodeTree( aStatement,
                                    (qtcNode *)sSrcNode->node.next,
                                    (qtcNode **)(&(sNode->node.next)),
                                    ID_TRUE,
                                    aSubqueryCopy )
                      != IDE_SUCCESS );
        }
        else
        {
            sNode->node.next = NULL;
        }
    }
    else
    {
        sNode->node.next = NULL;
    }

    *aDestination = sNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::copySubqueryNodeTree( qcStatement  * aStatement,
                                  qtcNode      * aSource,
                                  qtcNode     ** aDestination )
{
/***********************************************************************
 *
 * Description : parsing�������� �ʿ��� subquery nodetree�� �����Ѵ�.
 *
 * Implementation : 
 *
 ***********************************************************************/

    qdDefaultParseTree  * sDefaultParseTree;
    qcStatement           sStatement;

    IDE_ASSERT( aSource != NULL );
    IDE_ASSERT( ( aSource->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_SUBQUERY );
    
    //--------------------------------------------------
    // parsing �غ�
    //--------------------------------------------------

    // set meber of qcStatement
    QC_SET_STATEMENT( (&sStatement), aStatement, NULL );

    //--------------------------------------------------
    // parsing subquery
    //--------------------------------------------------
    sStatement.myPlan->stmtText = aSource->position.stmtText;
    sStatement.myPlan->stmtTextLen = idlOS::strlen( aSource->position.stmtText );
    
    // parsing subquery
    IDE_TEST( qcpManager::parsePartialForSubquery( &sStatement,
                                                   aSource->position.stmtText,
                                                   aSource->position.offset,
                                                   aSource->position.size )
              != IDE_SUCCESS );

    //--------------------------------------------------
    // assign subquery node
    //--------------------------------------------------
    
    sDefaultParseTree = (qdDefaultParseTree*) sStatement.myPlan->parseTree;
    *aDestination = sDefaultParseTree->defaultValue;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qtc::isConstNode4OrderBy( qtcNode * aNode )
{
/***********************************************************************
 *
 * Description :
 *    ��� ������� �˻��Ѵ�.
 *    BUG-25528 PSM������ host������ ����� �ƴ� ������ �Ǵ��ϵ��� ����
 *    Constant Node�� ���� ������ ���Ƕ�� ���� �����Ƿ�
 *    qmvOrderBy::disconnectConstantNode() �Ǵ��� ���ؼ��� ���
 *
 * Implementation :
 *    ����� �ƴ� ����
 *    - column
 *    - SUM(4), COUNT(*)
 *    - subquery
 *    - user-defined function (variable function�̶�� �����Ѵ�.)
 *    - sequence
 *    - prior
 *    - level, rownum
 *    - variable built-in function (random, sendmsg)
 *    - PSM ����
 *    - host ����
 *
 *    ��� ����
 *    - value
 *    - sysdate
 *
 ***********************************************************************/

    idBool  sIsConstNode = ID_FALSE;

    if( qtc::dependencyEqual( &aNode->depInfo,
                              &qtc::zeroDependencies ) == ID_TRUE )
    {
        if ( ( ( aNode->lflag & QTC_NODE_AGGREGATE_MASK )
               == QTC_NODE_AGGREGATE_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_AGGREGATE2_MASK )
               == QTC_NODE_AGGREGATE2_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK )
               == QTC_NODE_SUBQUERY_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_PROC_FUNCTION_MASK )
               == QTC_NODE_PROC_FUNCTION_FALSE )
             &&
             ( ( aNode->lflag & QTC_NODE_SEQUENCE_MASK )
               == QTC_NODE_SEQUENCE_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_PRIOR_MASK )
               == QTC_NODE_PRIOR_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_LEVEL_MASK )
               == QTC_NODE_LEVEL_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_ROWNUM_MASK )
               == QTC_NODE_ROWNUM_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_VAR_FUNCTION_MASK )
               == QTC_NODE_VAR_FUNCTION_ABSENT )
            &&
            ( ( aNode->lflag & QTC_NODE_PROC_VAR_MASK )
              == QTC_NODE_PROC_VAR_ABSENT )
            &&
            ( ( (aNode->node).lflag & MTC_NODE_BIND_MASK )
              == MTC_NODE_BIND_ABSENT )
             )
        {
            sIsConstNode = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return sIsConstNode;
}

idBool qtc::isConstNode4LikePattern( qcStatement * aStatement,
                                     qtcNode     * aNode,
                                     qcDepInfo   * aOuterDependencies )
{
/***********************************************************************
 *
 * Description :
 *    BUG-25594
 *    like pattern string�� ��� ������� �˻��Ѵ�.
 *
 * Implementation :
 *    ����� �ƴ� ����
 *    - column
 *    - SUM(4), COUNT(*)
 *    - subquery
 *    - user-defined function (variable function�̶�� �����Ѵ�.)
 *    - sequence
 *    - prior
 *    - level, rownum
 *    - variable built-in function (random, sendmsg)
 *
 *    ��� ����
 *    - PSM ����
 *    - host ����
 *    - value
 *    - sysdate
 *    - outer column ( BUG-43495 )
 *
 ***********************************************************************/

    idBool  sIsConstNode = ID_FALSE;

    if( qtc::dependencyEqual( &aNode->depInfo,
                              &qtc::zeroDependencies ) == ID_TRUE )
    {
        if ( ( ( aNode->lflag & QTC_NODE_AGGREGATE_MASK )
               == QTC_NODE_AGGREGATE_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_AGGREGATE2_MASK )
               == QTC_NODE_AGGREGATE2_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK )
               == QTC_NODE_SUBQUERY_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_PROC_FUNCTION_MASK )
               == QTC_NODE_PROC_FUNCTION_FALSE )
             &&
             ( ( aNode->lflag & QTC_NODE_SEQUENCE_MASK )
               == QTC_NODE_SEQUENCE_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_PRIOR_MASK )
               == QTC_NODE_PRIOR_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_LEVEL_MASK )
               == QTC_NODE_LEVEL_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_ROWNUM_MASK )
               == QTC_NODE_ROWNUM_ABSENT )
             &&
             ( ( aNode->lflag & QTC_NODE_VAR_FUNCTION_MASK )
               == QTC_NODE_VAR_FUNCTION_ABSENT )
             )
        {
            sIsConstNode = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        /* BUG-43495
           outer column�̸� ����� �Ǵ� */
        if ( (qtc::dependencyContains( aOuterDependencies,
                                       &aNode->depInfo )
              == ID_TRUE) &&
             (QCU_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE == 0) )
        {
            sIsConstNode = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE );
    }

    return sIsConstNode;
}

IDE_RC qtc::copyAndForDnfFilter( qcStatement* aStatement,
                                 qtcNode*     aSource,
                                 qtcNode**    aDestination,
                                 qtcNode**    aLast )
{
/***********************************************************************
 *
 * Description :
 *
 *    DNF Filter�� ���� �ʿ��� Node�� ������.
 *
 * Implementation :
 *
 *    DNF Filter�� �����ϱ� ���Ͽ� �ֻ��� AND Node(aSource)�� �����ϰ�,
 *    ������ DNF_NOT Node�� �����Ͽ� �̸� �����Ѵ�.
 *
 *        [AND]       : aSource
 *          |
 *          V
 *        [DNF_NOT]
 *
 *    TODO - A4������ ���� ��Ȯ�ϰ� ���ȭ�Ǿ� ó���Ǿ� ��.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::copyAndForDnfFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode * sNode;
    qtcNode * sFirstNode;
    qtcNode * sTrvsNode;
    qtcNode * sPrevNode;

    IDU_LIMITPOINT("qtc::copyAndForDnfFilter::malloc1");
    IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
             != IDE_SUCCESS);

    idlOS::memcpy( sNode, aSource, ID_SIZEOF(qtcNode) );
    sFirstNode = sNode;

    sPrevNode = NULL;
    sTrvsNode = (qtcNode*)(aSource->node.arguments);
    while( sTrvsNode != NULL )
    {
        IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                         sTrvsNode,
                                         &sNode,
                                         ID_FALSE,
                                         ID_FALSE,
                                         ID_FALSE,
                                         ID_FALSE )
                                         != IDE_SUCCESS );

        sNode->node.next = NULL;

        if( sPrevNode != NULL )
        {
            sPrevNode->node.next = (mtcNode *)sNode;
        }
        else
        {
            sFirstNode->node.arguments = (mtcNode *)sNode;
        }

        sPrevNode = sNode;
        sTrvsNode = (qtcNode*)(sTrvsNode->node.next);
    }

    *aDestination = sFirstNode;
    *aLast        = sPrevNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qtc::makeSameDataType4TwoNode( qcStatement * aStatement,
                               qtcNode     * aNode1,
                               qtcNode     * aNode2 )
{
/***********************************************************************
 *
 * Description :
 *     �� Node�� ���� ���� ���� ������ Data Type�� �����
 *     ���� �� �ֵ��� �����Ѵ�.
 *
 * Implementation :
 *
 *     ������ ���� ������ �̿��� �� SELECT ������ Target��
 *     Conversion���� ���� �ٸ� Type �� Target�̶� �� ����
 *     SET ���� ������ ����� �� �ֵ��� ��.
 *
 *     �Ʒ� �׸��� ���� ������ (=) �����ڸ� �����Ͽ�,
 *     Target�� �� Column�� ������ ��,
 *     (=)�� ���ؼ� estimate �����μ� ���� Column�� ������
 *     Data Type �� �ǵ��� Type Conversion��Ų��.
 *             ( = )
 *               |
 *               V
 *             (indierct) --------> (indirect)
 *               |                      |
 *               V                      V
 *       SELECT (column)      SELECT (column)
 *
 *
 ***********************************************************************/

#define IDE_FN "qtc::makeSameDataType4TwoNode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qtc::makeSameDataType4TwoNode"));

    // TYPE�� �ٸ� Target�� ���� Conversion�� ���Ͽ�
    // �ʿ��� ���� ������.
    qtcNode         * sEqualNode[2];
    qcNamePosition    sNullPosition;
    qtcNode         * sLeftIndirect;
    qtcNode         * sRightIndirect;

    SET_EMPTY_POSITION( sNullPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sEqualNode,
                             & sNullPosition,
                             (const UChar *) "=",
                             1 ) != IDE_SUCCESS );

    //fix BUG-17610
    sEqualNode[0]->node.lflag |= MTC_NODE_EQUI_VALID_SKIP_TRUE;

    // 1. Left�� SELECT Target�� ������
    IDU_LIMITPOINT("qtc::makeSameDataType4TwoNode::malloc1");
    IDE_TEST ( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                             qtcNode,
                             & sLeftIndirect )!= IDE_SUCCESS);
    IDE_TEST( qtc::makeIndirect( aStatement,
                                 sLeftIndirect,
                                 aNode1 )
              != IDE_SUCCESS );

    // 2. Right�� SELECT Target�� ������
    IDU_LIMITPOINT("qtc::makeSameDataType4TwoNode::malloc2");
    IDE_TEST ( STRUCT_ALLOC( QC_QMP_MEM(aStatement),
                             qtcNode,
                             & sRightIndirect )!= IDE_SUCCESS);
    IDE_TEST( qtc::makeIndirect( aStatement,
                                 sRightIndirect,
                                 aNode2 )
              != IDE_SUCCESS );

    // 3. (=) �����ڿ� Left�� Right Target�� Indirect Node ��
    //    �߰��� �ξ� ������.
    sEqualNode[0]->node.arguments = (mtcNode *) sLeftIndirect;
    sEqualNode[0]->node.arguments->next = (mtcNode *) sRightIndirect;

    // 4. estimate ȣ���� �Ͽ�, Left Target�� Right Target��
    //    Data Type �� ���Ͻ�Ŵ
    IDE_TEST(qtc::estimateNodeWithArgument( aStatement,
                                            sEqualNode[0] )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::makeLeftDataType4TwoNode( qcStatement * aStatement,
                                      qtcNode     * aNode1,
                                      qtcNode     * aNode2 )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *     �� Node�� ���� ���� ���� ���ʰ� ������ Data Type�� �����
 *     ���� �� �ֵ��� �����Ѵ�.
 *
 * Implementation :
 *
 *     �Ʒ� �׸��� ���� cast �����ڸ� �����Ͽ�, �������� target column��
 *     ���ʰ� ������ type�� �ǵ��� conversion node�� �����Ѵ�.
 *
 *                                 CAST( .. as column1_type)
 *                                       |
 *                                       V
 *                                   (indierct)
 *                                       |
 *                                       V
 *       SELECT (column1)      SELECT (column2)
 *
 ***********************************************************************/

    // TYPE�� �ٸ� Target�� ���� Conversion�� ���Ͽ�
    // �ʿ��� ���� ������.
    qtcNode         * sCastNode[2];
    qcNamePosition    sNullPosition;
    qtcNode           sIndirect;

    SET_EMPTY_POSITION( sNullPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sCastNode,
                             & sNullPosition,
                             (const UChar *) "CAST",
                             4 ) != IDE_SUCCESS );

    // SELECT Target�� ������
    IDE_TEST( qtc::makeIndirect( aStatement,
                                 & sIndirect,
                                 aNode2 )
              != IDE_SUCCESS );

    // cast �����ڿ� ������
    sCastNode[0]->node.arguments = (mtcNode *) & sIndirect;
    sCastNode[0]->node.funcArguments = (mtcNode *) aNode1;

    // estimate ȣ���� �Ͽ�, Data Type �� ���Ͻ�Ŵ
    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sCastNode[0] )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::makeRecursiveTargetInfo( qcStatement * aStatement,
                                     qtcNode     * aWithNode,
                                     qtcNode     * aViewNode,
                                     qcmColumn   * aColumnInfo )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *
 * Implementation :
 *     �߻��ϴ� target�� �����ϴ� target�� �����Ͽ� target column��
 *     precision�� �����Ѵ�.
 *
 ***********************************************************************/

    mtcColumn       * sWithColumn = NULL;
    mtcColumn       * sViewColumn = NULL;
    const mtdModule * sModule = NULL;

    sWithColumn = QTC_STMT_COLUMN( aStatement, aWithNode );
    sViewColumn = QTC_STMT_COLUMN( aStatement, aViewNode  );

    //---------------------------------------------
    // type�� �̹� ����.
    //---------------------------------------------

    IDE_DASSERT( sViewColumn->module->id == sWithColumn->module->id );

    //---------------------------------------------
    // columnInfo�� type,precision�� �����Ѵ�.
    //---------------------------------------------
    
    if ( sWithColumn->column.size < sViewColumn->column.size )
    {
        // �߻��ϴ� ��� ��� type�� ���� �����Ѵ�.
        // bit->varbit, byte->varbyte, char->varchar, echar->evarchar,
        // nchar->nvarchar, numeric->float
        sModule = sViewColumn->module;

        if ( sModule == &mtdBit )
        {
            sModule = &mtdVarbit;
        }
        else if ( sModule == &mtdByte )
        {
            sModule = &mtdVarbyte;
        }
        else if ( sModule == &mtdChar )
        {
            sModule = &mtdVarchar;
        }
        else if ( sModule == &mtdEchar )
        {
            sModule = &mtdEvarchar;
        }
        else if ( sModule == &mtdNchar )
        {
            sModule = &mtdNvarchar;
        }
        else if ( sModule == &mtdNumeric )
        {
            sModule = &mtdFloat;
        }
        else
        {
            // Nothing to do.
        }
        
        IDE_DASSERT( ( sModule->flag & MTD_CREATE_PARAM_MASK )
                     != MTD_CREATE_PARAM_NONE );
        
        // �ش� type�� ���� ū precision���� �����Ѵ�.
        IDE_TEST( mtc::initializeColumn( aColumnInfo->basicInfo,
                                         sModule,
                                         1,
                                         sModule->maxStorePrecision,
                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // �����ϴ� ���, ū ������ �����Ѵ�.
        mtc::copyColumn( aColumnInfo->basicInfo, sWithColumn );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//-------------------------------------------------
// PROJ-1358
// Bit Dependency ������ �ٲ� �ڷ� ������ ǥ����.
//-------------------------------------------------

void
qtc::dependencySet( UShort      aTupleID,
                    qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Internal Tuple ID �� dependencies�� Setting
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "qtc::dependencySet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aOperand1->depCount = 1;
    aOperand1->depend[0] = aTupleID;

    // PROJ-1653 Outer Join Operator (+)
    aOperand1->depJoinOper[0] = QMO_JOIN_OPER_FALSE;

#undef IDE_FN
}


void
qtc::dependencyChange( UShort      aSourceTupleID,
                       UShort      aDestTupleID,
                       qcDepInfo * aOperand1,
                       qcDepInfo * aResult )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *
 *    partitioned table�� tuple id��
 *    partition�� tuple id�� �����Ѵ�.
 *    partition�׷��� ���� selection graph�� from��
 *    dependency�� ��������, partitioned table�� dependency�� ������
 *    �ʰ� partition�� dependency�� �������� ������ �־�� �Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    qcDepInfo sResult;
    idBool    sSourceExist = ID_FALSE;
    idBool    sDestExist = ID_FALSE;
    UInt      i;

    if ( aSourceTupleID != aDestTupleID )
    {
        // source tuple id�� dest tuple id�� �ٸ���.
        sResult.depCount = 0;

        for ( i = 0; i < aOperand1->depCount; i++ )
        {
            if ( aOperand1->depend[i] == aSourceTupleID )
            {
                sSourceExist = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            if ( aOperand1->depend[i] == aDestTupleID )
            {
                sDestExist = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sSourceExist == ID_TRUE )
        {
            // source tuple id�� �ִ�.

            if ( sDestExist == ID_TRUE )
            {
                // dest tuple id�� �̹� �ִ�. source tuple id�� �ִٸ� �����Ѵ�.
                for ( i = 0; i < aOperand1->depCount; i++ )
                {
                    if ( aOperand1->depend[i] != aSourceTupleID )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                // dest tuple id�� ����. source tuple id�� �ٲ۴�.
                // (��, ���� ������ �����ؾ� �Ѵ�.)

                if ( aSourceTupleID < aDestTupleID )
                {
                    for ( i = 0;
                          ( i < aOperand1->depCount ) &&
                              ( aOperand1->depend[i] < aSourceTupleID );
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }

                    // source tuple id�� �����Ѵ�.
                    i++;

                    for ( ;
                          ( i < aOperand1->depCount ) &&
                              ( aOperand1->depend[i] < aDestTupleID );
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }

                    // dest tuple id�� �߰��Ѵ�.
                    sResult.depend[sResult.depCount] = aDestTupleID;
                    sResult.depCount++;

                    for ( ;
                          i < aOperand1->depCount;
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }
                }
                else
                {
                    for ( i = 0;
                          ( i < aOperand1->depCount ) &&
                              ( aOperand1->depend[i] < aDestTupleID );
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }

                    // dest tuple id�� �߰��Ѵ�.
                    sResult.depend[sResult.depCount] = aDestTupleID;
                    sResult.depCount++;

                    for ( ;
                          ( i < aOperand1->depCount ) &&
                              ( aOperand1->depend[i] < aSourceTupleID );
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }

                    // source tuple id�� �����Ѵ�.
                    i++;

                    for ( ;
                          i < aOperand1->depCount;
                          i++ )
                    {
                        sResult.depend[sResult.depCount] = aOperand1->depend[i];
                        sResult.depCount++;
                    }
                }
            }

            qtc::dependencySetWithDep( aResult, & sResult );
        }
        else
        {
            // source tuple id�� ����.
            qtc::dependencySetWithDep( aResult, aOperand1 );
        }
    }
    else
    {
        // source tuple id�� dest tuple id�� ����.
        qtc::dependencySetWithDep( aResult, aOperand1 );
    }
}

void qtc::dependencySetWithDep( qcDepInfo * aOperand1,
                                qcDepInfo * aOperand2 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependencies�� dependencies�� Setting
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencySetWithDep"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_ASSERT( aOperand1 != NULL );
    IDE_ASSERT( aOperand2 != NULL );
    
    if ( aOperand1 != aOperand2 )
    {
        aOperand1->depCount = aOperand2->depCount;

        if ( aOperand2->depCount > 0 )
        {
            idlOS::memcpy( aOperand1->depend,
                           aOperand2->depend,
                           ID_SIZEOF(UShort) * aOperand2->depCount );

            // PROJ-1653 Outer Join Operator (+)
            idlOS::memcpy( aOperand1->depJoinOper,
                           aOperand2->depJoinOper,
                           ID_SIZEOF(UChar) * aOperand2->depCount );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

#undef IDE_FN
}

void qtc::dependencyAnd( qcDepInfo * aOperand1,
                         qcDepInfo * aOperand2,
                         qcDepInfo * aResult )
{
/***********************************************************************
 *
 * Description :
 *
 *    AND Dependencies�� ����
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencyAnd"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcDepInfo sResult;

    UInt sLeft;
    UInt sRight;

    sResult.depCount = 0;

    for ( sLeft = 0, sRight = 0;
          ( (sLeft < aOperand1->depCount) && (sRight < aOperand2->depCount) );
        )
    {
        if ( aOperand1->depend[sLeft]
             == aOperand2->depend[sRight] )
        {
            sResult.depend[sResult.depCount] = aOperand1->depend[sLeft];

            // PROJ-1653 Outer Join Operator (+)
            sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[sLeft] & aOperand2->depJoinOper[sRight];

            sResult.depCount++;

            sLeft++;
            sRight++;
        }
        else
        {
            if ( aOperand1->depend[sLeft] > aOperand2->depend[sRight] )
            {
                sRight++;
            }
            else
            {
                sLeft++;
            }
        }
    }

    qtc::dependencySetWithDep( aResult, & sResult );

#undef IDE_FN
}

IDE_RC
qtc::dependencyOr( qcDepInfo * aOperand1,
                   qcDepInfo * aOperand2,
                   qcDepInfo * aResult )
{
/***********************************************************************
 *
 * Description :
 *
 *    OR Dependencies�� ����
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "qtc::dependencyOr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcDepInfo sResult;

    UInt sLeft;
    UInt sRight;

    IDE_ASSERT( aOperand1 != NULL );
    IDE_ASSERT( aOperand2 != NULL );
    IDE_ASSERT( aOperand1->depCount <= QC_MAX_REF_TABLE_CNT );
    IDE_ASSERT( aOperand2->depCount <= QC_MAX_REF_TABLE_CNT );
    
    sResult.depCount = 0;

    for ( sLeft = 0, sRight = 0;
          (sLeft < aOperand1->depCount) && (sRight < aOperand2->depCount);
        )
    {
        IDE_TEST_RAISE( sResult.depCount >= QC_MAX_REF_TABLE_CNT,
                        err_too_many_table_ref );

        if ( aOperand1->depend[sLeft] == aOperand2->depend[sRight] )
        {
            sResult.depend[sResult.depCount] = aOperand1->depend[sLeft];

            // PROJ-1653 Outer Join Operator (+)
            sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[sLeft] | aOperand2->depJoinOper[sRight];

            sLeft++;
            sRight++;

        }
        else
        {
            if ( aOperand1->depend[sLeft] > aOperand2->depend[sRight] )
            {
                sResult.depend[sResult.depCount] = aOperand2->depend[sRight];
                // PROJ-1653 Outer Join Operator (+)
                sResult.depJoinOper[sResult.depCount] = aOperand2->depJoinOper[sRight];

                sRight++;
            }
            else
            {
                sResult.depend[sResult.depCount] = aOperand1->depend[sLeft];
                // PROJ-1653 Outer Join Operator (+)
                sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[sLeft];

                sLeft++;
            }
        }

        sResult.depCount++;
    }

    // Left Operand�� ���� �ִ� �� Or-ing
    for ( ; sLeft < aOperand1->depCount; sLeft++ )
    {
        IDE_TEST_RAISE( sResult.depCount >= QC_MAX_REF_TABLE_CNT,
                        err_too_many_table_ref );

        sResult.depend[sResult.depCount] = aOperand1->depend[sLeft];
        // PROJ-1653 Outer Join Operator (+)
        sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[sLeft];
        sResult.depCount++;
    }

    // Right Operand�� ���� �ִ� �� Or-ing
    for ( ; sRight < aOperand2->depCount; sRight++ )
    {
        // To Fix PR-12758
        // �˻縦 ���� �����Ͽ��� ABW�� �߻����� ����.
        IDE_TEST_RAISE( sResult.depCount >= QC_MAX_REF_TABLE_CNT,
                        err_too_many_table_ref );

        sResult.depend[sResult.depCount] = aOperand2->depend[sRight];
        // PROJ-1653 Outer Join Operator (+)
        sResult.depJoinOper[sResult.depCount] = aOperand2->depJoinOper[sRight];
        sResult.depCount++;
    }

    qtc::dependencySetWithDep( aResult, & sResult );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_too_many_table_ref);
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QTC_TOO_MANY_TABLES ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

void qtc::dependencyRemove( UShort      aTupleID,
                            qcDepInfo * aOperand1,
                            qcDepInfo * aResult )
{
/***********************************************************************
 *
 * Description :
 *     aOperand1���� aTupleID�� dependency�� ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencyRemove"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcDepInfo sResult;
    UInt      i;

    sResult.depCount = 0;

    for ( i = 0; i < aOperand1->depCount; i++ )
    {
        if ( aOperand1->depend[i] != aTupleID )
        {
            sResult.depend[sResult.depCount] = aOperand1->depend[i];
            // PROJ-1653 Outer Join Operator (+)
            sResult.depJoinOper[sResult.depCount] = aOperand1->depJoinOper[i];
            sResult.depCount++;
        }
        else
        {
            // Nothing to do.
        }
    }

    qtc::dependencySetWithDep( aResult, & sResult );

#undef IDE_FN
}

void qtc::dependencyClear( qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependencies�� �ʱ�ȭ��.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencyClear"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    aOperand1->depCount = 0;

#undef IDE_FN
}

idBool qtc::dependencyEqual( qcDepInfo * aOperand1,
                             qcDepInfo * aOperand2 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependencies�� ������ ���� �Ǵ�
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencyEqual"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("IDE_RC qtc::dependencyEqual"));

    UInt i;

    if ( aOperand1->depCount != aOperand2->depCount )
    {
        return ID_FALSE;
    }
    else
    {
        for ( i = 0; i < aOperand1->depCount; i++ )
        {
            if ( aOperand1->depend[i] != aOperand2->depend[i] )
            {
                return ID_FALSE;
            }
        }
    }

    return ID_TRUE;

#undef IDE_FN
}

idBool qtc::haveDependencies( qcDepInfo * aOperand1 )
{
/***********************************************************************
 *
 * Description :
 *
 *    Dependency �� �����ϴ����� �Ǵ�
 *
 * Implementation :
 *
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::haveDependencies"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if ( aOperand1->depCount == 0 )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }

#undef IDE_FN
}

idBool qtc::dependencyContains( qcDepInfo * aSubject, qcDepInfo * aTarget )
{
/***********************************************************************
 *
 * Description :
 *
 *     aTarget�� Dependency�� aSubject�� Dependency�� ���ԵǴ��� �Ǵ�
 *
 * Implementation :
 *
 *
 **********************************************************************/

#define IDE_FN "IDE_RC qtc::dependencyContains"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt sLeft;
    UInt sRight;

    sRight = 0;

    if ( aSubject->depCount >= aTarget->depCount )
    {
        for ( sLeft = 0, sRight = 0;
              (sRight < aTarget->depCount) && (sLeft < aSubject->depCount) ; )
        {
            if ( aSubject->depend[sLeft] == aTarget->depend[sRight] )
            {
                sLeft++;
                sRight++;
            }
            else
            {
                if ( aSubject->depend[sLeft] > aTarget->depend[sRight] )
                {
                    break;
                }
                else
                {
                    sLeft++;
                }
            }
        }
    }
    else
    {
        // ���԰��踦 ������ �� ����.
    }

    if ( sRight == aTarget->depCount )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }

#undef IDE_FN
}

void qtc::dependencyMinus( qcDepInfo * aOperand1,
                           qcDepInfo * aOperand2,
                           qcDepInfo * aResult )
{
/***********************************************************************
 *
 * Description :
 *
 *     ( aOperand1 - aOperand2 ) = aOperand1;
 * 
 *     aOperand1���� aOperand2�� ��ġ�� dependency�� ����
 *
 * Implementation :
 *
 **********************************************************************/

    qcDepInfo sResultDepInfo;
    UInt      i;

    qtc::dependencyClear( & sResultDepInfo );
    qtc::dependencySetWithDep( & sResultDepInfo, aOperand1 );

    for ( i = 0; i < aOperand2->depCount; i++ )
    {
        dependencyRemove( aOperand2->depend[i],
                          & sResultDepInfo, 
                          & sResultDepInfo );
    }

    qtc::dependencySetWithDep( aResult, & sResultDepInfo );
}

IDE_RC qtc::isEquivalentExpression(
    qcStatement     * aStatement,
    qtcNode         * aNode1,
    qtcNode         * aNode2,
    idBool          * aIsTrue )
{
#define IDE_FN "qtc::isEquivalentExpression"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qtc::isEquivalentExpression"));

    qsExecParseTree * sExecPlanTree1;
    qsExecParseTree * sExecPlanTree2;
    qtcNode         * sArgu1;
    qtcNode         * sArgu2;
    mtcColumn       * sColumn1;
    mtcColumn       * sColumn2;
    mtcNode         * sNode1;
    mtcNode         * sNode2;
    qtcOverColumn   * sCurOverColumn1;
    qtcOverColumn   * sCurOverColumn2;
    SInt              sResult;
    idBool            sIsTrue;
    mtdValueInfo      sValueInfo1;
    mtdValueInfo      sValueInfo2;
    UInt              sFlag1;
    UInt              sFlag2;

    if ( (aNode1 != NULL) && (aNode2 != NULL) )
    {   
        // PROJ-2242 : constant filter �񱳸� ���� ���� �߰�
        // random �Լ����� ����� ���� ����

        // PROJ-2415 Grouping Sets Clause
        // ���� Null Value�� Empty Group���� ���� �� Null Value�� �񱳵Ǵ� �� �� ��������
        // QTC_NODE_EMPTY_GROUP_TRUE �� �˻��Ѵ�.
        if ( ( aNode1->node.arguments != NULL ) &&
             ( aNode2->node.arguments != NULL ) &&
             ( ( aNode1->lflag & QTC_NODE_CONVERSION_MASK )
                == QTC_NODE_CONVERSION_FALSE ) &&
             ( ( aNode2->lflag & QTC_NODE_CONVERSION_MASK )
                == QTC_NODE_CONVERSION_FALSE ) &&
             ( ( ( aNode1->lflag & QTC_NODE_EMPTY_GROUP_MASK )
                 != QTC_NODE_EMPTY_GROUP_TRUE ) &&
               ( ( aNode2->lflag & QTC_NODE_EMPTY_GROUP_MASK )
                 != QTC_NODE_EMPTY_GROUP_TRUE ) ) ) 
        {
            // non-terminal node

            if ( isSameModule(aNode1,aNode2) == ID_TRUE )
            {
                sIsTrue = ID_TRUE;

                //-------------------------------
                // arguments �˻�
                //-------------------------------

                sArgu1 = (qtcNode *)(aNode1->node.arguments);
                sArgu2 = (qtcNode *)(aNode2->node.arguments);

                while( (sArgu1 != NULL) && (sArgu2 != NULL) )
                {
                    IDE_TEST(isEquivalentExpression(
                                 aStatement, sArgu1, sArgu2, &sIsTrue)
                             != IDE_SUCCESS);

                    if (sIsTrue == ID_TRUE)
                    {
                        sArgu1 = (qtcNode *)(sArgu1->node.next);
                        sArgu2 = (qtcNode *)(sArgu2->node.next);
                    }
                    else
                    {
                        break; // exit while loop
                    }
                }

                if ( (sArgu1 != NULL) || (sArgu2 != NULL) )
                {
                    // arguments->next�� ���� ���� �ʴ� ���
                    sIsTrue = ID_FALSE;
                }
                else
                {
                    // Nothing to do.
                }

                if( sIsTrue == ID_FALSE )
                {
                    // BUG-31777
                    // ��ȯ��Ģ ������ +, * �������� ��� �� �� ��� expression��
                    // argument ������ �ٲ㰡�� �ٽ� ���غ���.
                    if( ( aNode1->node.module->lflag & MTC_NODE_COMMUTATIVE_MASK ) ==
                            MTC_NODE_COMMUTATIVE_TRUE )
                    {
                        sArgu1 = (qtcNode *)(aNode1->node.arguments);
                        sArgu2 = (qtcNode *)(aNode2->node.arguments->next);

                        IDE_TEST( isEquivalentExpression(
                                      aStatement, sArgu1, sArgu2, &sIsTrue )
                                != IDE_SUCCESS);

                        if( sIsTrue == ID_TRUE )
                        {
                            sArgu1 = (qtcNode *)(aNode1->node.arguments->next);
                            sArgu2 = (qtcNode *)(aNode2->node.arguments);

                            IDE_TEST( isEquivalentExpression(
                                          aStatement, sArgu1, sArgu2, &sIsTrue )
                                    != IDE_SUCCESS);
                        }
                    }
                }

                if ( sIsTrue == ID_TRUE )
                {
                    // PROJ-2179
                    // Aggregate function���� DISTINCT ���� ���� ���ΰ� ���ƾ� �Ѵ�.
                    if( ( aNode1->node.lflag & MTC_NODE_DISTINCT_MASK ) !=
                        ( aNode2->node.lflag & MTC_NODE_DISTINCT_MASK ) )
                    {
                        sIsTrue = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                    /* BUG-46805 */
                    if ( ( aNode1->node.lflag & MTC_NODE_OPERATOR_MASK )
                        == MTC_NODE_OPERATOR_SUBQUERY ) // subquery
                    {
                        // BUG-48381 copy�� subquery�� ������ ���������̴�.
                        if ( ( aNode1->node.table != aNode2->node.table ) ||
                             ( aNode1->node.column != aNode2->node.column ) )
                        {
                            if ( ( aNode1->subquery->myPlan->graph != NULL ) &&
                                 ( aNode2->subquery->myPlan->graph != NULL ) )
                            {
                                // BUG-48381 key predicate �߿� set op�� ���Ե� �������� ������Ŷ�� �ְ�
                                //           key predicate�� DNF�� Ǯ�� ��� ��>��������
                                if ( ( aNode1->subquery->myPlan->graph->myQuerySet->SFWGH != NULL ) &&
                                     ( aNode2->subquery->myPlan->graph->myQuerySet->SFWGH != NULL ) )
                                {
                                    if ( ( aNode1->subquery->myPlan->graph->myQuerySet->SFWGH->where != NULL ) ||
                                         ( aNode2->subquery->myPlan->graph->myQuerySet->SFWGH->where != NULL ) )
                                    {
                                        if ( ( aNode1->subquery->myPlan->graph->myQuerySet->SFWGH->where != NULL ) &&
                                             ( aNode2->subquery->myPlan->graph->myQuerySet->SFWGH->where != NULL ) )
                                        {
                                            sArgu1 = (qtcNode *)aNode1->subquery->myPlan->graph->myQuerySet->SFWGH->where;
                                            sArgu2 = (qtcNode *)aNode2->subquery->myPlan->graph->myQuerySet->SFWGH->where;

                                            while ( ( sArgu1 != NULL ) && ( sArgu2 != NULL ) )
                                            {
                                                IDE_TEST(isEquivalentExpression( aStatement, sArgu1, sArgu2, &sIsTrue)
                                                         != IDE_SUCCESS);

                                                if ( sIsTrue == ID_TRUE)
                                                {
                                                    if ( ( sArgu1->node.lflag & MTC_NODE_OPERATOR_MASK )
                                                         == MTC_NODE_OPERATOR_SUBQUERY ) // subquery
                                                    {
                                                        sIsTrue = ID_FALSE;
                                                        break;
                                                    }
                                                    else
                                                    {
                                                        sArgu1 = (qtcNode *)(sArgu1->node.next);
                                                        sArgu2 = (qtcNode *)(sArgu2->node.next);
                                                    }
                                                }
                                                else
                                                {
                                                    break; // exit while loop
                                                }
                                            }

                                            if ( (sArgu1 != NULL) || (sArgu2 != NULL) )
                                            {
                                                // arguments->next�� ���� ���� �ʴ� ���
                                                sIsTrue = ID_FALSE;
                                            }
                                            else
                                            {
                                                // Nothing to do.
                                            }
                                        }
                                        else
                                        {
                                            sIsTrue = ID_FALSE;
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
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    // Nothing to do.
                }

                *aIsTrue = sIsTrue;
            }
            else
            {
                *aIsTrue = ID_FALSE;
            }
        }
        // PROJ-2415 Grouping Sets Clause
        // ���� Null Value�� Empty Group���� ���� �� Null Value�� �񱳵Ǵ� �� �� ��������
        // QTC_NODE_EMPTY_GROUP_TRUE �� �˻��Ѵ�.

        // PROJ-2242 : constant filter �񱳸� ���� ���� �߰�
        // random �Լ����� ����� ���� ����
        else if ( ( ( (aNode1->node.arguments == NULL) &&
                      (aNode2->node.arguments == NULL) )
                    ||
                    ( ( ( aNode1->lflag & QTC_NODE_CONVERSION_MASK )
                        == QTC_NODE_CONVERSION_TRUE ) &&
                      ( ( aNode2->lflag & QTC_NODE_CONVERSION_MASK)
                        == QTC_NODE_CONVERSION_TRUE ) ) ) &&
                  ( ( ( aNode1->lflag & QTC_NODE_EMPTY_GROUP_MASK )
                      != QTC_NODE_EMPTY_GROUP_TRUE ) &&
                    ( ( aNode2->lflag & QTC_NODE_EMPTY_GROUP_MASK )
                      != QTC_NODE_EMPTY_GROUP_TRUE ) ) )
        {
            // terminal node

            if ( isSameModule(aNode1,aNode2) == ID_TRUE )
            {
                sIsTrue = ID_TRUE;
                
                //-------------------------------
                // terminal nodes �˻�
                //-------------------------------
                
                // terminal nodes : subquery,
                //                  host variable,
                //                  value,
                //                  column
                if ( ( aNode1->node.lflag & MTC_NODE_OPERATOR_MASK )
                    == MTC_NODE_OPERATOR_SUBQUERY ) // subquery
                {
                    sIsTrue = ID_FALSE;
                }
                else
                {
                    sColumn1 =
                        & ( QC_SHARED_TMPLATE(aStatement)->tmplate.
                            rows[aNode1->node.table].
                            columns[aNode1->node.column]);
                    sColumn2 =
                        & ( QC_SHARED_TMPLATE(aStatement)->tmplate.
                            rows[aNode2->node.table].
                            columns[aNode2->node.column]);

                    if (aNode1->node.module == &(qtc::valueModule))
                    {
                        if ( ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.
                                 rows[aNode1->node.table].lflag
                                 & MTC_TUPLE_TYPE_MASK )
                               == MTC_TUPLE_TYPE_VARIABLE )            ||
                             ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.
                                 rows[aNode2->node.table].lflag
                                 & MTC_TUPLE_TYPE_MASK )
                               == MTC_TUPLE_TYPE_VARIABLE ) )
                        {
                            // BUG-22045
                            // variable tuple�̶� �ϴ��� bind tuple�� ���� �˻簡���ϴ�.
                            if ( ( aNode1->node.table ==
                                   QC_SHARED_TMPLATE(aStatement)->tmplate.variableRow ) &&
                                 ( aNode1->node.table == aNode2->node.table ) &&
                                 ( aNode1->node.column == aNode2->node.column ) )
                            {
                                // Nothing to do.
                            }
                            else
                            {
                                sIsTrue = ID_FALSE;
                            }
                        }
                        else
                        {
                            /* PROJ-1361 : data module�� language module �и�
                               if (sColumn1->type.type == sColumn2->type.type &&
                               sColumn1->type.language ==
                               sColumn2->type.language)
                            */
                            if ( sColumn1->type.dataTypeId == MTD_GEOMETRY_ID ||
                                 sColumn2->type.dataTypeId == MTD_GEOMETRY_ID )
                            {
                                /* PROJ-2242 : CSE
                                   ex) WHERE OVERLAPS(F2, GEOMETRY'MULTIPOINT(0 0)')
                                          OR OVERLAPS(F2, GEOMETRY'MULTIPOINT(1 1, 2 2)')
                                                          |
                                                   �Ʒ� ���ǿ��� �� ��带 ���ٰ� �Ǵ�
                                */
                                sIsTrue = ID_FALSE;
                            }
                            else if ( sColumn1->type.dataTypeId == sColumn2->type.dataTypeId )
                            {
                                // compare value
                                sValueInfo1.column = sColumn1;
                                sValueInfo1.value  = QC_SHARED_TMPLATE(aStatement)->tmplate.
                                                     rows[aNode1->node.table].row;
                                sValueInfo1.flag   = MTD_OFFSET_USE;

                                sValueInfo2.column = sColumn2;
                                sValueInfo2.value  = QC_SHARED_TMPLATE(aStatement)->tmplate.
                                                     rows[aNode2->node.table].row;
                                sValueInfo2.flag   = MTD_OFFSET_USE;

                                sResult = sColumn1->module->
                                    keyCompare[MTD_COMPARE_MTDVAL_MTDVAL][MTD_COMPARE_ASCENDING]( &sValueInfo1,
                                                                                                  &sValueInfo2 );

                                if (sResult == 0)
                                {
                                    // Nothing to do.
                                }
                                else
                                {
                                    sIsTrue = ID_FALSE;
                                }
                            }
                            else
                            {
                                sIsTrue = ID_FALSE;
                            }
                        }
                    }
                    else if (aNode1->node.module == &(qtc::columnModule))
                    {
                        // column
                        if ( (aNode1->node.table == aNode2->node.table) &&
                             (aNode1->node.column == aNode2->node.column) )
                        {
                            // PROJ-2179
                            // PRIOR ���� ���� ���ΰ� ���ƾ� �Ѵ�.
                            if( ( aNode1->lflag & QTC_NODE_PRIOR_MASK ) !=
                                ( aNode2->lflag & QTC_NODE_PRIOR_MASK ) )
                            {
                                sIsTrue = ID_FALSE;
                            }
                            else
                            {
                                // Nothing to do.
                            }

                            // PROJ-2527 WITHIN GROUP AGGR
                            // WITHIN GROUP �� OREDER BY direction�� ���� ������ ����..
                            if( ( aNode1->node.lflag & MTC_NODE_WITHIN_GROUP_ORDER_MASK ) !=
                                ( aNode2->node.lflag & MTC_NODE_WITHIN_GROUP_ORDER_MASK ) )
                            {
                                sIsTrue = ID_FALSE;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        }
                        else
                        {
                            // BUG-31961
                            // Procedure ������ procedure template�� column�� ���Ѵ�.
                            if ( ( ( aNode1->lflag & QTC_NODE_PROC_VAR_MASK )
                                     == QTC_NODE_PROC_VAR_EXIST ) &&
                                 ( ( aNode2->lflag & QTC_NODE_PROC_VAR_MASK )
                                     == QTC_NODE_PROC_VAR_EXIST ) )
                            {
                                sNode1 = (mtcNode *)QC_SHARED_TMPLATE(aStatement)->tmplate.
                                    rows[aNode1->node.table].execute[aNode1->node.column].calculateInfo;
                                sNode2 = (mtcNode *)QC_SHARED_TMPLATE(aStatement)->tmplate.
                                    rows[aNode2->node.table].execute[aNode2->node.column].calculateInfo;

                                if( ( sNode1 != NULL ) && ( sNode2 != NULL ) )
                                {
                                    if( (sNode1->table  != sNode2->table) ||
                                        (sNode1->column != sNode2->column) )
                                    {
                                        sIsTrue = ID_FALSE;
                                    }
                                    else
                                    {
                                        // Nothing to do.
                                    }
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                            }
                            else
                            {
                                sIsTrue = ID_FALSE;
                            }
                        }
                    }
                    else if (aNode1->node.module == &(qtc::spFunctionCallModule))
                    {
                        // BUG-21065
                        // PSM�� ��� ���� expression���� �˻��Ѵ�.
                        sExecPlanTree1 = (qsExecParseTree*)
                            aNode1->subquery->myPlan->parseTree;
                        sExecPlanTree2 = (qsExecParseTree*)
                            aNode2->subquery->myPlan->parseTree;

                        if ( (sExecPlanTree1->procOID == sExecPlanTree2->procOID) &&
                             (sExecPlanTree1->subprogramID == sExecPlanTree2->subprogramID) )
                        {
                            if ( (aStatement->spvEnv->createProc == NULL) &&
                                 (aStatement->spvEnv->createPkg == NULL) &&
                                 (sExecPlanTree1->procOID == QS_EMPTY_OID) )
                            {
                                sIsTrue = ID_FALSE;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        }
                        else
                        {
                            sIsTrue = ID_FALSE;
                        }
                    }
                    else if (aNode1->node.module == &mtfCount )
                    {
                        // PROJ-2179 COUNT(*)�� ���� ���ڰ� ���� aggregate functino
                        // Nothing to do.
                    }
                    else
                    {
                        // BUG-34382
                        // ������ ����� table, column ���� ���Ѵ�.
                        if ( (aNode1->node.table == aNode2->node.table) &&
                             (aNode1->node.column == aNode2->node.column) )
                        {
                            sIsTrue = ID_TRUE;
                        }
                        else
                        {
                            sIsTrue = ID_FALSE;
                        }
                    }
                }

                *aIsTrue = sIsTrue;
            }
            else
            {
                *aIsTrue = ID_FALSE;
            }
        }
        else
        {
            *aIsTrue = ID_FALSE;
        }
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    //-------------------------------
    // over�� �˻�
    //-------------------------------    
    if( *aIsTrue == ID_TRUE )
    {
        if ( ( aNode1->overClause != NULL ) &&
             ( aNode2->overClause != NULL ) )
        {
            // �� ��� ��� over���� �ִ� ���
            
            sCurOverColumn1 = aNode1->overClause->overColumn;
            sCurOverColumn2 = aNode2->overClause->overColumn;
    
            while( (sCurOverColumn1 != NULL) && (sCurOverColumn2 != NULL) )
            {
                IDE_TEST( isEquivalentExpression(
                              aStatement,
                              sCurOverColumn1->node,
                              sCurOverColumn2->node,
                              &sIsTrue )
                          != IDE_SUCCESS);

                // PROJ-2179
                // OVER���� ORDER BY�� ������ Ȯ���Ѵ�.

                sFlag1 = ( sCurOverColumn1->flag & QTC_OVER_COLUMN_MASK );
                sFlag2 = ( sCurOverColumn2->flag & QTC_OVER_COLUMN_MASK );

                if( sFlag1 != sFlag2 )
                {
                    sIsTrue = ID_FALSE;
                }
                else
                {
                    if( sFlag1 == QTC_OVER_COLUMN_ORDER_BY )
                    {
                        sFlag1 = ( sCurOverColumn1->flag & QTC_OVER_COLUMN_ORDER_MASK );
                        sFlag2 = ( sCurOverColumn2->flag & QTC_OVER_COLUMN_ORDER_MASK );

                        if( sFlag1 != sFlag2 )
                        {
                            sIsTrue = ID_FALSE;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if (sIsTrue == ID_TRUE)
                {
                    sCurOverColumn1 = sCurOverColumn1->next;
                    sCurOverColumn2 = sCurOverColumn2->next;
                }
                else
                {
                    break; // exit while loop
                }
            }

            if ( (sCurOverColumn1 != NULL) || (sCurOverColumn2 != NULL) )
            {
                // overColumn->next�� ���� ���� �ʴ� ���
                sIsTrue = ID_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else if ( ( aNode1->overClause == NULL ) &&
                  ( aNode2->overClause == NULL ) )
        {
            // �� ��� ��� over���� ���� ���
            
            // Nothing to do.
        }
        else
        {
            // ��� ���ʿ��� over���� �ִ� ���
            
            sIsTrue = ID_FALSE;
        }

        *aIsTrue = sIsTrue;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::isEquivalentPredicate( qcStatement * aStatement,
                                   qtcNode     * aPredicate1,
                                   qtcNode     * aPredicate2,
                                   idBool      * aResult )
{
    qtcNode   * sArg1;
    qtcNode   * sArg2;
    mtfModule * sOperator;

    IDE_DASSERT( aPredicate1 != NULL );
    IDE_DASSERT( aPredicate2 != NULL );
    IDE_DASSERT( ( aPredicate1->node.lflag & MTC_NODE_COMPARISON_MASK )
                    == MTC_NODE_COMPARISON_TRUE );
    IDE_DASSERT( ( aPredicate2->node.lflag & MTC_NODE_COMPARISON_MASK )
                    == MTC_NODE_COMPARISON_TRUE );

    if( isSameModule( aPredicate1, aPredicate2 ) == ID_TRUE )
    {
        sArg1 = (qtcNode *)aPredicate1->node.arguments;
        sArg2 = (qtcNode *)aPredicate2->node.arguments;

        while( ( sArg1 != NULL ) && ( sArg2 != NULL ) )
        {
            IDE_TEST( isEquivalentExpression( aStatement,
                                              sArg1,
                                              sArg2,
                                              aResult )
                      != IDE_SUCCESS );

            if( *aResult == ID_FALSE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
            sArg1 = (qtcNode *)sArg1->node.next;
            sArg2 = (qtcNode *)sArg2->node.next;
        }

        if( *aResult == ID_TRUE )
        {
            if( ( sArg1 == NULL ) && ( sArg2 == NULL ) )
            {
                // Argument���� ������ ������ ���
            }
            else
            {
                // Argument���� ������ �ٸ� ���
                *aResult = ID_FALSE;
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        *aResult = ID_FALSE;
    }

    if( *aResult == ID_FALSE )
    {
        /******************************************************
         * ������ ���� ������ �����̹Ƿ� Ȯ���Ѵ�.
         * | A = B  | B = A  |
         * | A <> B | B <> A |
         * | A < B  | B > A  |
         * | A <= B | B >= A |
         * | A > B  | B < A  |
         * | A >= B | B <= A |
         *****************************************************/
        if( aPredicate1->node.module == &mtfEqual )
        {
            sOperator = &mtfEqual;
        }
        else if( aPredicate1->node.module == &mtfNotEqual )
        {
            sOperator = &mtfNotEqual;
        }
        else if( aPredicate1->node.module == &mtfLessThan )
        {
            sOperator = &mtfGreaterThan;
        }
        else if( aPredicate1->node.module == &mtfLessEqual )
        {
            sOperator = &mtfGreaterEqual;
        }
        else if( aPredicate1->node.module == &mtfGreaterThan )
        {
            sOperator = &mtfLessThan;
        }
        else if( aPredicate1->node.module == &mtfGreaterEqual )
        {
            sOperator = &mtfLessEqual;
        }
        else
        {
            sOperator = NULL;
        }

        if( aPredicate2->node.module == sOperator )
        {
            sArg1 = (qtcNode *)aPredicate1->node.arguments;
            sArg2 = (qtcNode *)aPredicate2->node.arguments->next;

            IDE_TEST( isEquivalentExpression( aStatement,
                                              sArg1,
                                              sArg2,
                                              aResult )
                      != IDE_SUCCESS );

            if( *aResult == ID_TRUE )
            {
                sArg1 = (qtcNode *)aPredicate1->node.arguments->next;
                sArg2 = (qtcNode *)aPredicate2->node.arguments;

                IDE_TEST( isEquivalentExpression( aStatement,
                                                  sArg1,
                                                  sArg2,
                                                  aResult )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : (parsing�� ������) estimate�ϱ� ���� expression�� ���� equivalent �˻�
 *
 ***********************************************************************/
IDE_RC qtc::isEquivalentExpressionByName( qtcNode * aNode1,
                                          qtcNode * aNode2,
                                          idBool  * aIsEquivalent )
{
    qtcNode         * sArgu1;
    qtcNode         * sArgu2;
    qtcOverColumn   * sCurOverColumn1;
    qtcOverColumn   * sCurOverColumn2;
    UInt              sFlag1;
    UInt              sFlag2;
    idBool            sIsEquivalent;

    if ( ( aNode1 != NULL ) && ( aNode2 != NULL ) )
    {
        /* PROJ-2415 Grouping Sets Clause
         * ���� Null Value�� Empty Group���� ���� �� Null Value�� �񱳵Ǵ� �� �� ��������
         * QTC_NODE_EMPTY_GROUP_TRUE �� �˻��Ѵ�.
         */
        if ( ( aNode1->node.arguments != NULL ) &&
             ( aNode2->node.arguments != NULL ) &&
             ( ( aNode1->lflag & QTC_NODE_EMPTY_GROUP_MASK ) != QTC_NODE_EMPTY_GROUP_TRUE ) &&
             ( ( aNode2->lflag & QTC_NODE_EMPTY_GROUP_MASK ) != QTC_NODE_EMPTY_GROUP_TRUE ) )
        {
            /* Non-terminal Node */

            if ( isSameModuleByName( aNode1,aNode2 ) == ID_TRUE )
            {
                sIsEquivalent = ID_TRUE;

                //-------------------------------
                // arguments �˻�
                //-------------------------------

                sArgu1 = (qtcNode *)(aNode1->node.arguments);
                sArgu2 = (qtcNode *)(aNode2->node.arguments);

                while( ( sArgu1 != NULL ) && ( sArgu2 != NULL ) )
                {
                    IDE_TEST( isEquivalentExpressionByName( sArgu1, sArgu2, &sIsEquivalent )
                              != IDE_SUCCESS );

                    if ( sIsEquivalent == ID_TRUE )
                    {
                        sArgu1 = (qtcNode *)(sArgu1->node.next);
                        sArgu2 = (qtcNode *)(sArgu2->node.next);
                    }
                    else
                    {
                        break;
                    }
                }
                if ( ( sArgu1 != NULL ) || ( sArgu2 != NULL ) )
                {
                    sIsEquivalent = ID_FALSE;
                }
                else
                {
                    /* Nothing to do */
                }

                if ( sIsEquivalent == ID_FALSE )
                {
                    /* BUG-31777
                     * ��ȯ��Ģ ������ +, * �������� ��� �� �� ��� expression��
                     * argument ������ �ٲ㰡�� �ٽ� ���غ���.
                     */
                    if ( ( aNode1->node.module->lflag & MTC_NODE_COMMUTATIVE_MASK ) ==
                                                        MTC_NODE_COMMUTATIVE_TRUE )
                    {
                        sArgu1 = (qtcNode *)(aNode1->node.arguments);
                        sArgu2 = (qtcNode *)(aNode2->node.arguments->next);

                        IDE_TEST( isEquivalentExpressionByName( sArgu1, sArgu2, &sIsEquivalent )
                                  != IDE_SUCCESS );

                        if ( sIsEquivalent == ID_TRUE )
                        {
                            sArgu1 = (qtcNode *)(aNode1->node.arguments->next);
                            sArgu2 = (qtcNode *)(aNode2->node.arguments);

                            IDE_TEST( isEquivalentExpressionByName( sArgu1, sArgu2, &sIsEquivalent )
                                      != IDE_SUCCESS );
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
                else
                {
                    /* Nothing to do */
                }

                if ( sIsEquivalent == ID_TRUE )
                {
                    /* PROJ-2179
                     * Aggregate function���� DISTINCT ���� ���� ���ΰ� ���ƾ� �Ѵ�.
                     */
                    if ( ( aNode1->node.lflag & MTC_NODE_DISTINCT_MASK ) !=
                         ( aNode2->node.lflag & MTC_NODE_DISTINCT_MASK ) )
                    {
                        sIsEquivalent = ID_FALSE;
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

                *aIsEquivalent = sIsEquivalent;
            }
            else
            {
                *aIsEquivalent = ID_FALSE;
            }
        }
        /* PROJ-2415 Grouping Sets Clause
         * ���� Null Value�� Empty Group���� ���� �� Null Value�� �񱳵Ǵ� �� �� ��������
         * QTC_NODE_EMPTY_GROUP_TRUE �� �˻��Ѵ�.
         */
        else if ( ( aNode1->node.arguments == NULL ) &&
                  ( aNode2->node.arguments == NULL ) &&
                  ( ( aNode1->lflag & QTC_NODE_EMPTY_GROUP_MASK ) != QTC_NODE_EMPTY_GROUP_TRUE ) &&
                  ( ( aNode2->lflag & QTC_NODE_EMPTY_GROUP_MASK ) != QTC_NODE_EMPTY_GROUP_TRUE ) )
        {
            /* Terminal Node */

            if ( isSameModuleByName( aNode1, aNode2 ) == ID_TRUE )
            {
                sIsEquivalent = ID_TRUE;

                if ( ( aNode1->node.lflag & MTC_NODE_OPERATOR_MASK )
                                         == MTC_NODE_OPERATOR_SUBQUERY ) /* subquery */
                {
                    sIsEquivalent = ID_FALSE;
                }
                else
                {
                    if (aNode1->node.module == &(qtc::valueModule))
                    {
                        if ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->columnName, aNode2->columnName ) != ID_TRUE )
                        {
                            sIsEquivalent = ID_FALSE;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                    else if ( aNode1->node.module == &(qtc::columnModule) )
                    {
                        /* PROJ-2179
                         * PRIOR ���� ���� ���ΰ� ���ƾ� �Ѵ�.
                         */
                        if( ( aNode1->lflag & QTC_NODE_PRIOR_MASK ) !=
                            ( aNode2->lflag & QTC_NODE_PRIOR_MASK ) )
                        {
                            sIsEquivalent = ID_FALSE;
                        }
                        else
                        {
                            if ( ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->userName, aNode2->userName ) != ID_TRUE ) ||
                                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->tableName, aNode2->tableName ) != ID_TRUE ) ||
                                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->columnName, aNode2->columnName ) != ID_TRUE ) ||
                                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->pkgName, aNode2->pkgName ) != ID_TRUE ) )
                            {
                                sIsEquivalent = ID_FALSE;
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        }
                    }
                    else if ( aNode1->node.module == &(qtc::spFunctionCallModule) )
                    {
                        if ( ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->userName, aNode2->userName ) != ID_TRUE ) ||
                             ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->tableName, aNode2->tableName ) != ID_TRUE ) ||
                             ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->columnName, aNode2->columnName ) != ID_TRUE ) ||
                             ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->pkgName, aNode2->pkgName ) != ID_TRUE ) )
                        {
                            sIsEquivalent = ID_FALSE;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                    else if ( aNode1->node.module == &mtfCount )
                    {
                        /* PROJ-2179 COUNT(*)�� ���� ���ڰ� ���� aggregate function */
                        /* Nothing to do */
                    }
                    else
                    {
                        if ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->position, aNode2->position ) != ID_TRUE )
                        {
                            sIsEquivalent = ID_FALSE;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                }

                *aIsEquivalent = sIsEquivalent;
            }
            else
            {
                *aIsEquivalent = ID_FALSE;
            }
        }
        else
        {
            *aIsEquivalent = ID_FALSE;
        }
    }
    else
    {
        *aIsEquivalent = ID_FALSE;
    }

    //-------------------------------
    // over�� �˻�
    //-------------------------------
    if ( *aIsEquivalent == ID_TRUE )
    {
        if ( ( aNode1->overClause != NULL ) &&
             ( aNode2->overClause != NULL ) )
        {
            /* �� ��� ��� over���� �ִ� ��� */

            sCurOverColumn1 = aNode1->overClause->overColumn;
            sCurOverColumn2 = aNode2->overClause->overColumn;

            while ( ( sCurOverColumn1 != NULL ) && ( sCurOverColumn2 != NULL ) )
            {
                IDE_TEST( isEquivalentExpressionByName(
                              sCurOverColumn1->node,
                              sCurOverColumn2->node,
                              &sIsEquivalent )
                          != IDE_SUCCESS );

                /* PROJ-2179
                 * OVER���� ORDER BY�� ������ Ȯ���Ѵ�.
                 */

                sFlag1 = sCurOverColumn1->flag & QTC_OVER_COLUMN_MASK;
                sFlag2 = sCurOverColumn2->flag & QTC_OVER_COLUMN_MASK;

                if ( sFlag1 != sFlag2 )
                {
                    sIsEquivalent = ID_FALSE;
                }
                else
                {
                    if ( sFlag1 == QTC_OVER_COLUMN_ORDER_BY )
                    {
                        sFlag1 = sCurOverColumn1->flag & QTC_OVER_COLUMN_ORDER_MASK;
                        sFlag2 = sCurOverColumn2->flag & QTC_OVER_COLUMN_ORDER_MASK;

                        if ( sFlag1 != sFlag2 )
                        {
                            sIsEquivalent = ID_FALSE;
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

                if ( sIsEquivalent == ID_TRUE )
                {
                    sCurOverColumn1 = sCurOverColumn1->next;
                    sCurOverColumn2 = sCurOverColumn2->next;
                }
                else
                {
                    break; /* exit while loop */
                }
            }

            if ( ( sCurOverColumn1 != NULL ) || ( sCurOverColumn2 != NULL ) )
            {
                /* overColumn->next�� ���� ���� �ʴ� ��� */
                sIsEquivalent = ID_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else if ( ( aNode1->overClause == NULL ) &&
                  ( aNode2->overClause == NULL ) )
        {
            /* �� ��� ��� over���� ���� ��� */
            /* Nothing to do */
        }
        else
        {
            /* ��� ���ʿ��� over���� �ִ� ��� */
            sIsEquivalent = ID_FALSE;
        }

        *aIsEquivalent = sIsEquivalent;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : module�� ������ ��(isEquivalentExpressionByName���� ���)
 *
 ***********************************************************************/
idBool qtc::isSameModuleByName( qtcNode * aNode1,
                                qtcNode * aNode2 )
{
    idBool sIsSame;

    if ( aNode1->node.module == aNode2->node.module )
    {
        if ( aNode1->node.module == &(qtc::spFunctionCallModule) )
        {
            if ( ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->userName, aNode2->userName ) != ID_TRUE ) ||
                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->tableName, aNode2->tableName ) != ID_TRUE ) ||
                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->columnName, aNode2->columnName ) != ID_TRUE ) ||
                 ( QC_IS_NAME_MATCHED_OR_EMPTY( aNode1->pkgName, aNode2->pkgName ) != ID_TRUE ) )
            {
                sIsSame = ID_FALSE;
            }
            else
            {
                sIsSame = ID_TRUE;
            }
        }
        else
        {
            sIsSame = ID_TRUE;
        }
    }
    else
    {
        sIsSame = ID_FALSE;
    }

    return sIsSame;
}

IDE_RC qtc::changeNodeFromColumnToSP( qcStatement * aStatement,
                                      qtcNode     * aNode,
                                      mtcTemplate * aTemplate,
                                      mtcStack    * aStack,
                                      SInt          aRemain,
                                      mtcCallBack * aCallBack,
                                      idBool      * aFindColumn )
{
/***********************************************************************
 *
 * Description :
 *    column node�� stored function node�� �����Ѵ�.
 *
 * Implementation :
 *    1. change module
 *    2. estimate node
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtc::changeNodeFromColumnToSP"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt                  sErrCode;

    // update module pointer
    aNode->node.module = & qtc::spFunctionCallModule;
    aNode->node.lflag  = qtc::spFunctionCallModule.lflag &
                                    ~MTC_NODE_COLUMN_COUNT_MASK;

    aNode->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
    aNode->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;


    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                               aNode,
                               aStatement,
                               QC_SHARED_TMPLATE(aStatement),
                               MTC_TUPLE_TYPE_INTERMEDIATE,
                               qtc::spFunctionCallModule.lflag &
                                   MTC_NODE_COLUMN_COUNT_MASK )
        != IDE_SUCCESS );

    if (qtc::estimateInternal( aNode,
                               aTemplate,
                               aStack,
                               aRemain,
                               aCallBack ) == IDE_SUCCESS )
    {
        *aFindColumn = ID_TRUE;
    }
    else
    {
        sErrCode = ideGetErrorCode();

        // PROJ-1083 Package
        // sErrCode �߰�
        if ( sErrCode == qpERR_ABORT_QSV_NOT_EXIST_PROC_SQLTEXT ||
             sErrCode == qpERR_ABORT_QCM_NOT_EXIST_USER ||
             sErrCode == qpERR_ABORT_QSV_INVALID_IDENTIFIER ||
             sErrCode == qpERR_ABORT_QSV_NOT_EXIST_PKG )
        {
            IDE_CLEAR();
        }
        else
        {
            IDE_RAISE( ERR_PROC_VALIDATE );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_PROC_VALIDATE)
    {
        // Do not set error code
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::setSubAggregation( qtcNode * aNode )
{
#define IDE_FN "IDE_RC qtc::setSubAggregation"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if( ( aNode->lflag & QTC_NODE_AGGREGATE_MASK )
        == QTC_NODE_AGGREGATE_EXIST )
    {
        aNode->indexArgument = 1;
    }
    if( aNode->node.next != NULL )
    {
        IDE_TEST( setSubAggregation( (qtcNode*)(aNode->node.next) ) != IDE_SUCCESS );
    }
    if( aNode->node.arguments != NULL )
    {
        IDE_TEST( setSubAggregation( (qtcNode*)(aNode->node.arguments) ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtc::getDataOffset( qcStatement * aStatement,
                           UInt          aSize,
                           UInt        * aOffsetPtr )
{
/***********************************************************************
 *
 * Description : aOffsetPtr�� template�� data ��ġ�� Ȯ���Ѵ�.
 *               template�� dataSize�� consistency�� �����ϱ� ����
 *               �� �Լ��� ���ؼ� offset ��ġ�� ���� �Ѵ�.
 *
 * Implementation : datasize�� �׻� 8����Ʈ align�Ǿ� �ֵ��� �Ѵ�.
 *
 **********************************************************************/

#define IDE_FN "IDE_RC qtc::getDataOffset"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( aStatement != NULL );

    *aOffsetPtr = QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize;
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize += aSize;

    // align ����
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize =
        idlOS::align8( QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize );

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC qtc::addSDF( qcStatement * aStatement, qmoScanDecisionFactor * aSDF )
{
    qmoScanDecisionFactor * sLast;
    qcStatement           * sTopStatement;

    IDE_DASSERT( aStatement != NULL );

    // Optimize �������� view���� ���� statement�� ���������.
    // ���� statement�� sdf�� �߰��ϸ� �ȵǰ�,
    // �׻� �� ���� statement���� sdf�� �����ؾ� �ϹǷ�
    // ���� statement�� �����´�.

    sTopStatement = QC_SHARED_TMPLATE(aStatement)->stmt;

    if( sTopStatement->myPlan->scanDecisionFactors == NULL )
    {
        sTopStatement->myPlan->scanDecisionFactors = aSDF;
    }
    else
    {
        for( sLast = sTopStatement->myPlan->scanDecisionFactors;
             sLast->next != NULL;
             sLast = sLast->next ) ;

        sLast->next = aSDF;
    }

    return IDE_SUCCESS;
}

IDE_RC qtc::removeSDF( qcStatement * aStatement, qmoScanDecisionFactor * aSDF )
{
    qmoScanDecisionFactor * sPrev;
    qcStatement           * sTopStatement;

    IDE_DASSERT( aStatement != NULL );

    // Optimize �������� view���� ���� statement�� ���������.
    // ���� statement�� sdf�� �߰��ϸ� �ȵǰ�,
    // �׻� �� ���� statement���� sdf�� �����ؾ� �ϹǷ�
    // ���� statement�� �����´�.
    sTopStatement = QC_SHARED_TMPLATE(aStatement)->stmt;

    IDE_ASSERT( sTopStatement->myPlan->scanDecisionFactors != NULL );

    if( sTopStatement->myPlan->scanDecisionFactors == aSDF )
    {
        sTopStatement->myPlan->scanDecisionFactors = aSDF->next;
    }
    else
    {
        for( sPrev = sTopStatement->myPlan->scanDecisionFactors;
             sPrev->next != aSDF;
             sPrev = sPrev->next )
        {
            IDE_DASSERT( sPrev != NULL );
        }

        sPrev->next = aSDF->next;
    }

    return IDE_SUCCESS;
}

idBool qtc::getConstPrimitiveNumberValue( qcTemplate  * aTemplate,
                                          qtcNode     * aNode,
                                          SLong       * aNumberValue )
{
/***********************************************************************
 *
 * Description : PROJ-1405 ROWNUM
 *               ������ constant value�� ���� �����´�.
 *
 * Implementation :
 *
 **********************************************************************/

    const mtcColumn  * sColumn;
    const void       * sValue;
    SLong              sNumberValue;
    idBool             sIsNumber;

    sNumberValue = 0;
    sIsNumber = ID_FALSE;

    if ( isConstValue( aTemplate, aNode ) == ID_TRUE )
    {
        sColumn = QTC_TMPL_COLUMN( aTemplate, aNode );
        sValue  = QTC_TMPL_FIXEDDATA( aTemplate, aNode );

        if ( sColumn->module->isNull( sColumn, sValue ) == ID_TRUE )
        {
            // Nothing to do.
        }
        else
        {
            // BUG-17791
            if ( sColumn->module->id == MTD_SMALLINT_ID )
            {
                sIsNumber = ID_TRUE;
                sNumberValue = (SLong) (*(mtdSmallintType*)sValue);
            }
            else if ( sColumn->module->id == MTD_INTEGER_ID )
            {
                sIsNumber = ID_TRUE;
                sNumberValue = (SLong) (*(mtdIntegerType*)sValue);
            }
            else if ( sColumn->module->id == MTD_BIGINT_ID )
            {
                sIsNumber = ID_TRUE;
                sNumberValue = (SLong) (*(mtdBigintType*)sValue);
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

    *aNumberValue = sNumberValue;

    return sIsNumber;
}

//PROJ-1583 large geometry
UInt
qtc::getSTObjBufSize( mtcCallBack * aCallBack )
{
    qtcCallBackInfo * sCallBackInfo    = (qtcCallBackInfo*)(aCallBack->info);

    // BUG-27619
    qcStatement*      sStatement       = sCallBackInfo->tmplate->stmt;
    qmsSFWGH*         sSFWGHOfCallBack = sCallBackInfo->SFWGH;
    qmsHints        * sHints           = NULL;
    qcSharedPlan    * sMyPlan          = NULL;
    UInt              sObjBufSize      = QMS_NOT_DEFINED_ST_OBJECT_BUFFER_SIZE;

    IDE_DASSERT( sStatement != NULL );
    sMyPlan = sStatement->myPlan;

    if( sSFWGHOfCallBack != NULL )
    {
        sHints = sSFWGHOfCallBack->hints;
    }

    if( sHints != NULL )
    {
        if( sHints->STObjBufSize != QMS_NOT_DEFINED_ST_OBJECT_BUFFER_SIZE )
        {
            sObjBufSize = sHints->STObjBufSize;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    // To Fix BUG-32072

    if ( sObjBufSize == QMS_NOT_DEFINED_ST_OBJECT_BUFFER_SIZE )
    {
        if ( sMyPlan != NULL )
        {
            if ( sMyPlan->planEnv != NULL )
            {
                sObjBufSize = sMyPlan->planEnv->planProperty.STObjBufSize;
            }
            else
            {
                // Nothing to do
            }
            
        }
        else
        {
            // Nothing to do 
        }
    }
    else
    {
        //Nothing to do
    }

    if ( sObjBufSize == QMS_NOT_DEFINED_ST_OBJECT_BUFFER_SIZE )
    {
        // BUG-27619
        if (sStatement != NULL)
        {
            
            sObjBufSize = QCG_GET_SESSION_ST_OBJECT_SIZE( sStatement );

            // environment�� ���
            qcgPlan::registerPlanProperty( sStatement,
                                           PLAN_PROPERTY_ST_OBJECT_SIZE );
        }
        else
        {
            sObjBufSize = QCU_ST_OBJECT_BUFFER_SIZE;
        }
    }
    else
    {
        // Nothing to do.
    }

    return sObjBufSize;
}

IDE_RC
qtc::allocAndInitColumnLocateInTuple( iduVarMemList * aMemory,
                                      mtcTemplate   * aTemplate,
                                      UShort          aRowID )
{
    UShort   i;

    IDU_LIMITPOINT("qtc::allocAndInitColumnLocateInTuple::malloc");
    IDE_TEST(
        aMemory->cralloc(
            idlOS::align8((UInt)ID_SIZEOF(mtcColumnLocate))
            * aTemplate->rows[aRowID].columnMaximum,
            (void**)
            &(aTemplate->rows[aRowID].columnLocate ) )
        != IDE_SUCCESS );

    for( i = 0;
         i < aTemplate->rows[aRowID].columnMaximum;
         i++ )
    {
        aTemplate->rows[aRowID].columnLocate[i].table
            = aRowID;
        aTemplate->rows[aRowID].columnLocate[i].column
            = i;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::setBindParamInfoByNode( qcStatement * aStatement,
                                    qtcNode     * aColumnNode,
                                    qtcNode     * aBindNode )
{
/***********************************************************************
 * 
 * Description : Bind Param Info ������ ������
 *
 * Implementation :
 *    aColumnNode�� ���� Column Info�� ȹ���Ͽ�
 *    aBindNode�� �����Ǵ� BindParam ������ �����Ѵ�.
 ***********************************************************************/

    qciBindParam * sBindParam;
    mtcColumn    * sColumnInfo = NULL;

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aColumnNode != NULL );
    IDE_DASSERT( aBindNode != NULL );
    
    //-------------------
    // Column Info ȹ��
    //-------------------
    
    sColumnInfo = QTC_STMT_COLUMN( aStatement, aColumnNode );
    
    //-------------------
    // Column Info�� ���� Bind Parameter ���� ����
    //-------------------
    sBindParam = &aStatement->myPlan->sBindParam[aBindNode->node.column].param;

    // PROJ-2002 Column Security
    // ��ȣ ����Ÿ Ÿ���� ���� ����Ÿ Ÿ������ Bind Parameter ���� ����
    if( sColumnInfo->type.dataTypeId == MTD_ECHAR_ID )
    {
        sBindParam->type      = MTD_CHAR_ID;
        sBindParam->language  = sColumnInfo->type.languageId;
        sBindParam->arguments = 1;
        sBindParam->precision = sColumnInfo->precision;
        sBindParam->scale     = 0;
    }
    else if( sColumnInfo->type.dataTypeId == MTD_EVARCHAR_ID )
    {
        sBindParam->type      = MTD_VARCHAR_ID;
        sBindParam->language  = sColumnInfo->type.languageId;
        sBindParam->arguments = 1;
        sBindParam->precision = sColumnInfo->precision;
        sBindParam->scale     = 0;
    }
    /* BUG-43294 The server raises an assertion becauseof bindings to NULL columns */
    else if( sColumnInfo->type.dataTypeId == MTD_NULL_ID )
    {
        sBindParam->type      = MTD_VARCHAR_ID;
        sBindParam->language  = sColumnInfo->type.languageId;
        sBindParam->arguments = 1;
        sBindParam->precision = 1;
        sBindParam->scale     = 0;
    }
    else
    {
        sBindParam->type      = sColumnInfo->type.dataTypeId;
        sBindParam->language  = sColumnInfo->type.languageId;
        sBindParam->arguments = sColumnInfo->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;
        sBindParam->precision = sColumnInfo->precision;
        sBindParam->scale     = sColumnInfo->scale;
    }
    
    return IDE_SUCCESS;
}

IDE_RC qtc::setDescribeParamInfo4Where( qcStatement     * aStatement,
                                        qtcNode         * aNode )
{
/***********************************************************************
 * 
 * Description : WHERE ������ DescribeParamInfo ���� ������ ���,
 *               �̸� ã�� ����
 *
 * Implementation :
 *    WHERE ������ DescribeParamInfo ���� ������ ���,
 *    - ���� node�� bind node�� ����
 *    - �� ��������  ( =, >, <, >=, <= )
 *    - (Column)(�񱳿�����)(Bind) or (Bind)(�񱳿�����)(Column) ������
 *
 *    ���� node�� next�� ���� �����ϸ鼭 �Ʒ��� ���� �۾� ������
 *    (1) ���� node�� bind node�� �ִ��� �˻�
 *    (2) �� ���������� �˻�
 *        - �� ������ �̸� (3) ���� ����
 *        - �� �����ڰ� �ƴϸ�, ���� node�� argument�� ���Ͽ�
 *          recursive�ϰ� (1)�� ������
 *    (3) (Column)(�񱳿�����)(Bind) or (Bind)(�񱳿�����)(Column) ����
 *         �̸� Column������ ȹ���Ͽ� Bind Param ���� ����
 *
 ***********************************************************************/

    qtcNode      * sCurNode;
    qtcNode      * sLeftOperand;
    qtcNode      * sRightOperand;
    qtcNode      * sColumnNode;
    qtcNode      * sBindNode;

    for ( sCurNode = aNode;
          sCurNode != NULL;
          sCurNode = (qtcNode*)sCurNode->node.next )
    {
        if ( ( sCurNode->node.lflag & MTC_NODE_BIND_MASK )
             == MTC_NODE_BIND_EXIST )
        {
            //----------------------------
            // bind node�� �ִ� ���,
            //----------------------------

            // BUG-34327
            // >ALL�� MTC_NODE_OPERATOR_GREATER�̹Ƿ� flag�� ������ �ʰ�
            // mtfModule�� ���� ���Ѵ�.
            if ( ( sCurNode->node.module == & mtfEqual )       ||
                 ( sCurNode->node.module == & mtfNotEqual )    ||
                 ( sCurNode->node.module == & mtfLessThan )    ||
                 ( sCurNode->node.module == & mtfLessEqual )   ||
                 ( sCurNode->node.module == & mtfGreaterThan ) ||
                 ( sCurNode->node.module == & mtfGreaterEqual ) )
            {
                //----------------------------
                // ���� node�� �� �������� ��� ( =, !=, >, <, >=, <= )
                //----------------------------

                sLeftOperand = (qtcNode*)sCurNode->node.arguments;
                sRightOperand = (qtcNode*)sCurNode->node.arguments->next;

                //----------------------------
                // (Column)(�񱳿�����)(Bind) or
                // (Bind)(�񱳿�����)(Column) ���¸� ã��
                //----------------------------
                sColumnNode = NULL;
                sBindNode   = NULL;
                
                if ( ( QTC_IS_TERMINAL( sLeftOperand ) == ID_TRUE ) &&
                     ( QTC_IS_TERMINAL( sRightOperand ) == ID_TRUE ) )
                {
                    if ( ( sLeftOperand->node.lflag & MTC_NODE_BIND_MASK )
                         == MTC_NODE_BIND_EXIST )
                    {
                        // Left operand�� bind node�� ���,
                        // Right operand�� column �̾�� ��
                        if ( QTC_IS_COLUMN( aStatement, sRightOperand )
                             == ID_TRUE )
                        {
                            sColumnNode = sRightOperand;
                            sBindNode   = sLeftOperand;
                        }
                        else
                        {
                            // constant ������, bind node �����
                            // �����
                        }
                    }
                    else
                    {
                        // Right operand�� bind node�� ���,
                        // Left operand�� column �̾�� ��
                        if ( QTC_IS_COLUMN( aStatement, sLeftOperand )
                             == ID_TRUE )
                        {
                            sColumnNode = sLeftOperand;
                            sBindNode   = sRightOperand;
                        }
                        else
                        {
                            // constant ������, bind node �����
                            // �����
                        }
                    }
                }
                else
                {    
                    // left operand �Ǵ� right operand��
                    // terminal�� �ƴ� ���,
                    // next node�� ����
                }

                //-------------------
                // Bind Param ���� ����
                //-------------------

                if ( ( sColumnNode != NULL ) && ( sBindNode != NULL ) )
                {
                    // (Column)(�񱳿�����)(Bind) or
                    // (Bind)(�񱳿�����)(Column) ���¸� ã�� ���,
                    // BindParamInfo ����
                    IDE_TEST( setBindParamInfoByNode( aStatement,
                                                      sColumnNode,
                                                      sBindNode )
                              != IDE_SUCCESS );
                    
                    sColumnNode = NULL;
                    sBindNode = NULL;
                }
                else
                {
                    // nothing to do 
                    // (ex. 1 = ?, ? = ? etc... )
                }
            }
            else
            {
                // nothing to do 
            }
            
            if ( ( sCurNode->node.module == & mtfLike ) ||
                 ( sCurNode->node.module == & mtfNotLike ) )
            {
                //----------------------------
                // ���� node�� like �������� ��� (like, not like)
                //----------------------------
                    
                sLeftOperand = (qtcNode*)sCurNode->node.arguments;
                sRightOperand = (qtcNode*)sCurNode->node.arguments->next;

                //----------------------------
                // (Column)(�񱳿�����)(Bind) ���¸� ã��
                //----------------------------
                sColumnNode = NULL;
                sBindNode   = NULL;
                
                if ( ( QTC_IS_TERMINAL( sLeftOperand ) == ID_TRUE ) &&
                     ( QTC_IS_TERMINAL( sRightOperand ) == ID_TRUE ) )
                {
                    if ( ( sRightOperand->node.lflag & MTC_NODE_BIND_MASK )
                         == MTC_NODE_BIND_EXIST )
                    {
                        // Right operand�� bind node�� ���,
                        // Left operand�� column �̾�� ��
                        if ( QTC_IS_COLUMN( aStatement, sLeftOperand )
                             == ID_TRUE )
                        {
                            sColumnNode = sLeftOperand;
                            sBindNode   = sRightOperand;
                        }
                        else
                        {
                            // constant ������, bind node �����
                            // �����
                        }
                    }
                    else
                    {
                        // Left operand�� bind node�� ���
                    }
                }
                else
                {    
                    // left operand �Ǵ� right operand��
                    // terminal�� �ƴ� ���,
                    // next node�� ����
                }
                    
                //-------------------
                // Bind Param ���� ����
                //-------------------

                if ( ( sColumnNode != NULL ) && ( sBindNode != NULL ) )
                {
                    // (Column)(�񱳿�����)(Bind) or
                    // (Bind)(�񱳿�����)(Column) ���¸� ã�� ���,
                    // BindParamInfo ����
                    IDE_TEST( setBindParamInfoByNode( aStatement,
                                                      sColumnNode,
                                                      sBindNode )
                              != IDE_SUCCESS );
                        
                    sColumnNode = NULL;
                    sBindNode = NULL;
                }
                else
                {
                    // nothing to do 
                }
            }
            else
            {
                // nothing to do
            }
            
            if ( sCurNode->node.module == & mtfCast )
            {
                //----------------------------
                // ���� node�� cast �������� ���
                //----------------------------
                    
                sLeftOperand = (qtcNode*)sCurNode->node.arguments;

                //----------------------------
                // cast((Bind) as (Column)) ���¸� ã��
                //----------------------------
                sColumnNode = NULL;
                sBindNode   = NULL;
                        
                if ( ( QTC_IS_TERMINAL( sLeftOperand ) == ID_TRUE )
                     &&
                     ( (sLeftOperand->node.lflag & MTC_NODE_BIND_MASK)
                       == MTC_NODE_BIND_EXIST ) )
                {
                    // Left operand�� bind node�� ���,
                    sColumnNode = (qtcNode*)sCurNode->node.funcArguments;
                    sBindNode   = sLeftOperand;
                }
                else
                {
                    // nothing to do
                }
                        
                //-------------------
                // Bind Param ���� ����
                //-------------------

                if ( ( sColumnNode != NULL ) && ( sBindNode != NULL ) )
                {
                    // (Column)(�񱳿�����)(Bind) or
                    // (Bind)(�񱳿�����)(Column) ���¸� ã�� ���,
                    // BindParamInfo ����
                    IDE_TEST( setBindParamInfoByNode( aStatement,
                                                      sColumnNode,
                                                      sBindNode )
                              != IDE_SUCCESS );
                        
                    sColumnNode = NULL;
                    sBindNode = NULL;
                }
                else
                {
                    // nothing to do 
                }
            }
            else
            {
                // nothing to do 
            }
            
            // �� �����ڰ� �ƴ� ���,
            IDE_TEST(
                setDescribeParamInfo4Where(
                    aStatement,
                    (qtcNode*)sCurNode->node.arguments )
                != IDE_SUCCESS );
        }
        else
        {
            // bind node�� ���� ���,
            // nothing to do 
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::fixAfterOptimization( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *
 *    Optimization �Ϸ� ���� ó��
 *
 * Implementation :
 *
 *    Tuple�� �Ҵ�� ���ʿ��� ������ �����Ͽ� ����ȭ�Ѵ�.
 *
 ***********************************************************************/

    qcTemplate  * sTemplate = QC_SHARED_TMPLATE(aStatement);
    mtcTuple    * sTuple;
    mtcColumn   * sNewColumns;
    mtcColumn   * sOldColumns;
    mtcExecute  * sNewExecute;
    mtcExecute  * sOldExecute;
    UShort        sRow;

    // BUG-34350
    if( QCU_OPTIMIZER_REFINE_PREPARE_MEMORY == 1 )
    {
        for ( sRow = 0; sRow < sTemplate->tmplate.rowCount; sRow++ )
        {
            sTuple = & sTemplate->tmplate.rows[sRow];
 
            IDE_DASSERT( sTuple->columnCount <= sTuple->columnMaximum );
 
            //------------------------------------------------
            // ������� ���� column, execute�� ����
            //------------------------------------------------
 
            if ( sTuple->columnCount < sTuple->columnMaximum )
            {
                sOldColumns = sTuple->columns;
                sOldExecute = sTuple->execute;
 
                if ( sTuple->columnCount == 0 )
                {
                    // mtcColumn
                    IDE_TEST( QC_QMP_MEM(aStatement)->free( sOldColumns )
                              != IDE_SUCCESS );
                    sNewColumns = NULL;
 
                    // mtcEecute
                    IDE_TEST( QC_QMP_MEM(aStatement)->free( sOldExecute )
                              != IDE_SUCCESS );
                    sNewExecute = NULL;
                }
                else
                {
                    // mtcColumn
                    IDU_LIMITPOINT("qtc::fixAfterOptimization::malloc1");
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(mtcColumn) * sTuple->columnCount,
                                                             (void**) & sNewColumns )
                              != IDE_SUCCESS );
 
                    idlOS::memcpy( (void*) sNewColumns,
                                   (void*) sOldColumns,
                                   ID_SIZEOF(mtcColumn) * sTuple->columnCount );
 
                    IDE_TEST( QC_QMP_MEM(aStatement)->free( sOldColumns )
                              != IDE_SUCCESS );
 
                    // mtcEecute
                    IDU_LIMITPOINT("qtc::fixAfterOptimization::malloc2");
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(mtcExecute) * sTuple->columnCount,
                                                             (void**) & sNewExecute )
                              != IDE_SUCCESS );
 
                    idlOS::memcpy( (void*) sNewExecute,
                                   (void*) sOldExecute,
                                   ID_SIZEOF(mtcExecute) * sTuple->columnCount );
 
                    IDE_TEST( QC_QMP_MEM(aStatement)->free( sOldExecute )
                              != IDE_SUCCESS );
                }
 
                sTuple->columnMaximum = sTuple->columnCount;
                sTuple->columns = sNewColumns;
                sTuple->execute = sNewExecute;  
            }
            else
            {
                // Nothing to do.
            }
 
            //------------------------------------------------
            // mtcColumnLocate�� ����
            //------------------------------------------------
 
            if ( sTuple->columnLocate != NULL )
            {
                // mtcColumnLocate�� optimization���Ŀ���
                // ���̻� ������ �����Ƿ� �����Ѵ�.
                IDE_TEST( QC_QMP_MEM(aStatement)->free( sTuple->columnLocate )
                          != IDE_SUCCESS );
 
                sTuple->columnLocate = NULL;
            }
            else
            {
                // Nothing to do.
            }

            /* BUG-42639 Fixed Table + Disk Temp ����FATAL
             * Fiexed Table�� �ִ� ������ ��� Transaction ���� �����ϵ���
             * �ߴµ� Disk Temp �����ÿ��� Transaction�� �ʿ��ϴ�
             */
            if ( ( sTuple->lflag & MTC_TUPLE_STORAGE_MASK )
                 == MTC_TUPLE_STORAGE_DISK )
            {
                QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
                QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        // Skip refining prepare memory
        // It reduces prepare time but causes wasting prepare memory.

        /* BUG-42639 Fixed Table + Disk Temp ����FATAL
         * Fiexed Table�� �ִ� ������ ��� Transaction ���� �����ϵ���
         * �ߴµ� Disk Temp �����ÿ��� Transaction�� �ʿ��ϴ�
         */
        for ( sRow = 0; sRow < sTemplate->tmplate.rowCount; sRow++ )
        {
            sTuple = & sTemplate->tmplate.rows[sRow];

            IDE_DASSERT( sTuple->columnCount <= sTuple->columnMaximum );

            if ( ( sTuple->lflag & MTC_TUPLE_STORAGE_MASK )
                 == MTC_TUPLE_STORAGE_DISK )
            {
                QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
                QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qtc::isSameModule( qtcNode* aNode1,
                   qtcNode* aNode2 )
{
/***********************************************************************
 *
 * Description : module�� ������ ��(isEquivalentExpression���� ���)
 *               To fix BUG-21441
 *
 * Implementation :
 *    (1) module�� �ٸ��� false
 *    (2) module�� ������, sp�ΰ�� oid�� �ٸ��� false
 *    (3) module�� ����, ���� sp�� ��� oid�� ������ true
 *
 ***********************************************************************/

    idBool sIsSame;

    if( aNode1->node.module == aNode2->node.module )
    {
        if( aNode1->node.module ==
            &(qtc::spFunctionCallModule) )
        {
            IDE_ASSERT( (aNode1->subquery != NULL) &&
                        (aNode2->subquery != NULL) );

            if( ((qsExecParseTree*)
                 (aNode1->subquery->myPlan->parseTree))->procOID ==
                ((qsExecParseTree*)
                 (aNode2->subquery->myPlan->parseTree))->procOID )
            {
                sIsSame = ID_TRUE;
            }
            else
            {
                sIsSame = ID_FALSE;
            }
        }
        else
        {
            sIsSame = ID_TRUE;
        }
    }
    else
    {
        sIsSame = ID_FALSE;
    }
    
    return sIsSame;
}

mtcNode*
qtc::getCaseSubExpNode( mtcNode* aNode )
{
/***********************************************************************
 *
 * Description : case expression node��ȯ
 *
 * BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻�
 *
 * Implementation :
 *
 ***********************************************************************/  

    mtcNode* sNode;
    mtcNode* sConversion;
    
    for( sNode = aNode;
         ( sNode->lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_TRUE;
         sNode = sNode->arguments ) ;
    
    sConversion = sNode->conversion;
    
    if ( sConversion != NULL )
    {
        if( sConversion->info != ID_UINT_MAX )
        {
            sConversion = sNode;            
        }
        if( sConversion != NULL )
        {
            sNode = sConversion;
        }
    }
    
    return sNode;
}

idBool qtc::isQuotedName( qcNamePosition * aPosition )
{
    idBool  sIsQuotedName = ID_FALSE;
    
    if ( aPosition != NULL )
    {
        if ( (aPosition->stmtText != NULL)
             &&
             (aPosition->offset > 0)
             &&
             (aPosition->size > 0) )
        {
            if ( aPosition->stmtText[aPosition->offset - 1] == '"' )
            {
                // stmtText�� NTS�� 1byte�� �� �о ������.
                if ( aPosition->stmtText[aPosition->offset + aPosition->size] == '"' )
                {
                    sIsQuotedName = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return sIsQuotedName;
}

/**
 * PROJ-2208 getNLSCurrencyCallback
 *
 *  MT �ʿ� ��ϵǴ� callback �Լ��� mtcTemplate�� ���ڷ� �޾� ������ ã�� �� ���ǿ�
 *  �ش��ϴ� Currency�� ã�Ƽ� �����Ѵ�. ���� ������ �������� ������
 *  Property ���� �����Ѵ�.
 *
 */
IDE_RC qtc::getNLSCurrencyCallback( mtcTemplate * aTemplate,
                                    mtlCurrency * aCurrency )
{
    qcTemplate * sTemplate = ( qcTemplate * )aTemplate;
    SChar      * sTemp = NULL;
    SInt         sLen  = 0;

    sTemp = QCG_GET_SESSION_ISO_CURRENCY( sTemplate->stmt );

    idlOS::memcpy( aCurrency->C, sTemp, MTL_TERRITORY_ISO_LEN );
    aCurrency->C[MTL_TERRITORY_ISO_LEN] = 0;

    sTemp = QCG_GET_SESSION_CURRENCY( sTemplate->stmt );
    sLen  = idlOS::strlen( sTemp );

    if ( sLen <= MTL_TERRITORY_CURRENCY_LEN )
    {
        idlOS::memcpy( aCurrency->L, sTemp, sLen );
        aCurrency->L[sLen] = 0;
        aCurrency->len = sLen;
    }
    else
    {
        idlOS::memcpy( aCurrency->L, sTemp, MTL_TERRITORY_CURRENCY_LEN );
        aCurrency->L[MTL_TERRITORY_CURRENCY_LEN] = 0;
        aCurrency->len = MTL_TERRITORY_CURRENCY_LEN;
    }

    sTemp = QCG_GET_SESSION_NUMERIC_CHAR( sTemplate->stmt );
    aCurrency->D = *( sTemp );
    aCurrency->G = *( sTemp + 1 );

    return IDE_SUCCESS;
}

/**
 * PROJ-1353
 *
 *   GROUPING, GROUPING_ID �� ���� Ư���ϰ� 2�� estimate�� �ʿ��Ѱ�� �ٷ� ��带
 *   estimate�ϱ� ���� �ʿ��ϴ�. estimateInternal �� 2�� ����Ǹ� ���̰� �Ǵ� ����
 *   �����ϱ� ���ؼ���.
 */
IDE_RC qtc::estimateNodeWithSFWGH( qcStatement * aStatement,
                                   qmsSFWGH    * aSFWGH,
                                   qtcNode     * aNode )
{
    qtcCallBackInfo sCallBackInfo = {
        QC_SHARED_TMPLATE(aStatement),
        QC_QMP_MEM(aStatement),
        aStatement,
        NULL,
        aSFWGH,
        NULL
    };
    mtcCallBack sCallBack = {
        &sCallBackInfo,
        MTC_ESTIMATE_ARGUMENTS_ENABLE,
        alloc,
        initConversionNodeIntermediate
    };

    qcTemplate * sTemplate = QC_SHARED_TMPLATE(aStatement);


    IDE_TEST( estimateNode( aNode,
                            (mtcTemplate *)sTemplate,
                            sTemplate->tmplate.stack,
                            sTemplate->tmplate.stackRemain,
                            &sCallBack )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::getLoopCount( qcTemplate  * aTemplate,
                          qtcNode     * aLoopNode,
                          SLong       * aCount )
{
/***********************************************************************
 *
 * Description :
 *    LOOP clause�� expression�� �����Ͽ� ���� ��ȯ�Ѵ�.
 * 
 * Implementation :
 *
 ***********************************************************************/
    
    const mtcColumn  * sColumn;
    const void       * sValue;

    IDE_TEST( qtc::calculate( aLoopNode, aTemplate )
              != IDE_SUCCESS );
    
    sColumn = aTemplate->tmplate.stack->column;
    sValue  = aTemplate->tmplate.stack->value;

    if ( sColumn->module->id == MTD_BIGINT_ID )
    {
        if ( sColumn->module->isNull( sColumn, sValue ) == ID_TRUE )
        {
            *aCount = 0;
        }
        else
        {
            *aCount = (SLong)(*(mtdBigintType*)sValue);
        }
    }
    else if ( sColumn->module->id == MTD_LIST_ID )
    {
        *aCount = sColumn->precision;
    }
    else if ( sColumn->module->id == MTD_ASSOCIATIVE_ARRAY_ID )
    {
        // PROJ-1904 Extend UDT
        IDE_TEST_RAISE( *(qsxArrayInfo**)sValue == NULL, ERR_INVALID_ARRAY );

        *aCount = qsxArray::getElementsCount( *(qsxArrayInfo**)sValue );
    }
    else if ( ( sColumn->module->id == MTD_RECORDTYPE_ID ) ||
              ( sColumn->module->id == MTD_ROWTYPE_ID ) )
    {
        *aCount = 1;
    }
    else
    {
        // �̹� validation���� �˻������Ƿ�
        // ����� ���� ���� ����.
        IDE_RAISE( ERR_LOOP_TYPE );
    }

    IDE_TEST_RAISE( *aCount < 0, ERR_LOOP_VALUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LOOP_TYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::getLoopCount",
                                  "invalid type" ));
    }
    IDE_EXCEPTION( ERR_LOOP_VALUE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_LOOP_VALUE ) );
    }
    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qtc::hasNameArgumentForPSM( qtcNode * aNode )
{
    qtcNode * sCurrArg = NULL;
    idBool    sExist   = ID_FALSE;

    for ( sCurrArg = (qtcNode *)(aNode->node.arguments);
          sCurrArg != NULL;
          sCurrArg = (qtcNode *)(sCurrArg->node.next) )
    {
        if ( ( sCurrArg->lflag & QTC_NODE_SP_PARAM_NAME_MASK )
               == QTC_NODE_SP_PARAM_NAME_TRUE )
        {
            sExist = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    return sExist;
}

IDE_RC qtc::changeNodeForArray( qcStatement*    aStatement,
                                qtcNode**       aNode,
                                qcNamePosition* aPosition,
                                qcNamePosition* aPositionEnd,
                                idBool          aIsBracket )
{
/***********************************************************************
 *
 * Description : PROJ-2533
 *    List Expression�� ������ �������Ѵ�.
 *    name( arguments ) ������ function/array�� ���� ������ �������Ѵ�.
 *    name[ index ] ������ array�� ���� ������ �������Ѵ�.
 *
 * Implementation :
 *    List Expression�� �ǹ̸� ������ ������ Setting�ϸ�,
 *    �ش� String�� ��ġ�� �������Ѵ�.
 *    �� �Լ��� �� �� �ִ� ����� ����
 *        (1) func_name ()
 *            proc_name ()
 *            func_name ( list_expr )
 *            proc_name ( list_expr )
 *             * user-defined function, window function, built-in function
 *        (2) arrvar_name ( list_expr ) -> spFunctionCallModule
 *            arrvar_name [ list_expr ] -> columnModule
 *        (3) winddow function
 *            w_func()  over cluse
 *            w_func( list_expr ) within_clause  ignore_nulls  over_clause
 ***********************************************************************/
    const mtfModule * sModule = NULL;
    idBool            sExist = ID_FALSE;

    if ( aIsBracket == ID_FALSE )
    {
        IDE_TEST( mtf::moduleByName( &sModule,
                                     &sExist,
                                     (void*)(aPosition->stmtText +aPosition->offset),
                                     aPosition->size )
                  != IDE_SUCCESS );

        if ( sExist == ID_FALSE )
        {
            sModule = & qtc::spFunctionCallModule;
        }
        else
        {
            // Nothing to do.
        }

        // sModule�� mtfList�� �ƴ� ���� �Լ��������� ���� �����.
        // �� ��� node change�� �Ͼ.
        // ex) sum(i1) or to_date(i1, 'yy-mm-dd')
        if ( (sModule != &mtfList) || (aNode[0]->node.module == NULL) )
        {
            IDE_DASSERT( aNode[0]->node.module == NULL );

            // node�� module�� null�� ���� list��������
            // expression�� �Ŵ޷� �ִ� ���.
            // node�� module�� null�� �ƴ� ���,
            //  �̹� ������ �� node�� ����� ������.
            // ex) to_date( i1, 'yy-mm-dd' )
            //
            // list������ ��� �ֻ����� �� node�� �޷������Ƿ�
            // ������ ���� �� node�� change
            //
            //  ( )                    to_date
            //   |                =>     |
            //   i1 - 'yy-mm-dd'        i1 - 'yy-mm-dd'
            aNode[0]->node.lflag  |= sModule->lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
            aNode[0]->node.module  = sModule;

            if ( &qtc::spFunctionCallModule == aNode[0]->node.module )
            {
                // fix BUG-14257
                aNode[0]->lflag &= ~QTC_NODE_PROC_FUNCTION_MASK;
                aNode[0]->lflag |= QTC_NODE_PROC_FUNCTION_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                       aNode[0],
                                       aStatement,
                                       QC_SHARED_TMPLATE(aStatement),
                                       MTC_TUPLE_TYPE_INTERMEDIATE,
                                       (sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK) )
                      != IDE_SUCCESS );
        }
        else
        {
            // BUG-38935 display name�� ���� �����Ѵ�.
            aNode[0]->node.lflag &= ~MTC_NODE_REMOVE_ARGUMENTS_MASK;
            aNode[0]->node.lflag |= MTC_NODE_REMOVE_ARGUMENTS_TRUE;
        }
    }
    else
    {
        aNode[0]->node.module = &qtc::columnModule;
        aNode[0]->node.lflag |= qtc::columnModule.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;
    }

    aNode[0]->columnName        = *aPosition;
    aNode[0]->position.stmtText = aPosition->stmtText;
    aNode[0]->position.offset   = aPosition->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size - aPosition->offset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeColumn( qtcNode        ** aNode,
                          qcNamePosition  * aPositionStart,
                          qcNamePosition  * aPositionEnd,
                          qcNamePosition  * aUserNamePos,
                          qcNamePosition  * aTableNamePos,
                          qcNamePosition  * aColumnNamePos,
                          qcNamePosition  * aPkgNamePos)
{
/*******************************************************************
 * Description :
 *     PROJ-2533 array ������ ����� changeNode
 *     column�� ���� ������ �������Ѵ�.
 *
 * Implementation :
 *
 *******************************************************************/
    IDE_DASSERT( aNode[0]->node.module == NULL );
 
    aNode[0]->node.module = &qtc::columnModule;
    aNode[0]->node.lflag |= qtc::columnModule.lflag & ~MTC_NODE_COLUMN_COUNT_MASK;

    aNode[0]->userName   = *aUserNamePos;
    aNode[0]->tableName  = *aTableNamePos;
    aNode[0]->columnName = *aColumnNamePos;
    aNode[0]->pkgName    = *aPkgNamePos;

    aNode[0]->position.stmtText = aPositionStart->stmtText;
    aNode[0]->position.offset   = aPositionStart->offset;
    aNode[0]->position.size     = aPositionEnd->offset + aPositionEnd->size - aPositionStart->offset;

    return IDE_SUCCESS;
}

IDE_RC qtc::addKeepArguments( qcStatement  * aStatement,
                              qtcNode     ** aNode,
                              qtcKeepAggr  * aKeep )
{
    qtcNode         * sCopyNode = NULL;
    qtcNode         * sNode     = NULL;
    UInt              sCount    = 0;
    qcuSqlSourceInfo  sSqlInfo;
    qcNamePosition    sPosition;
    qtcNode         * sNewNode[2];

    if ( ( aNode[0]->node.module != &mtfMin ) &&
         ( aNode[0]->node.module != &mtfMax ) &&
         ( aNode[0]->node.module != &mtfSum ) &&
         ( aNode[0]->node.module != &mtfAvg ) &&
         ( aNode[0]->node.module != &mtfCount ) &&
         ( aNode[0]->node.module != &mtfVariance ) &&
         ( aNode[0]->node.module != &mtfStddev ) )
    {
        sSqlInfo.setSourceInfo( aStatement, &aKeep->mKeepPosition );
        IDE_RAISE( ERR_SYNTAX );
    }
    else
    {
        /* Nothing to do */
    }

    SET_EMPTY_POSITION( sPosition );
    IDE_TEST( qtc::makeValue( aStatement,
                              sNewNode,
                              ( const UChar * )"CHAR",
                              4,
                              &sPosition,
                              ( const UChar * )&aKeep->mOption,
                              1,
                              MTC_COLUMN_NORMAL_LITERAL )
              != IDE_SUCCESS );

    aNode[0]->node.funcArguments = &sNewNode[0]->node;
    sNewNode[0]->node.arguments = NULL;
    sNewNode[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;

    if ( aKeep->mExpression[0]->node.module != NULL )
    {
        sCount = 2;
        sNewNode[0]->node.next = (mtcNode *)&aKeep->mExpression[0]->node;
        sNewNode[0]->node.lflag += 1;
    }
    else
    {
        sCount = ( aKeep->mExpression[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) + 1;
        IDE_TEST_RAISE( ( aNode[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) + sCount >
                        MTC_NODE_ARGUMENT_COUNT_MAXIMUM,
                        ERR_INVALID_FUNCTION_ARGUMENT );
        sNewNode[0]->node.next = aKeep->mExpression[0]->node.arguments;
        sNewNode[0]->node.lflag += ( sCount - 1 );
    }

    // funcArguments�� node.lflag�� �����ϱ� ���� �����Ѵ�.
    IDE_TEST( copyNodeTree( aStatement,
                            (qtcNode*) aNode[0]->node.funcArguments,
                            & sCopyNode,
                            ID_TRUE,
                            ID_FALSE )
              != IDE_SUCCESS );

    // arguments�� �ٿ��ִ´�.
    if ( aNode[0]->node.arguments == NULL )
    {
        aNode[0]->node.arguments = (mtcNode*) sCopyNode;
    }
    else
    {
        for ( sNode = (qtcNode*) aNode[0]->node.arguments;
              sNode->node.next != NULL;
              sNode = (qtcNode*) sNode->node.next );

        sNode->node.next = (mtcNode*) sCopyNode;
    }

    //argument count�� ����
    aNode[0]->node.lflag += sCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SYNTAX )
    {
        ( void )sSqlInfo.init( aStatement->qmeMem );
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCP_SYNTAX, sSqlInfo.getErrMessage() ));
        ( void )sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_FUNCTION_ARGUMENT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::changeKeepNode( qcStatement  * aStatement,
                            qtcNode     ** aNode )
{
    const mtfModule *   sModule     = NULL;
    ULong               sNodeArgCnt = 0;
    UShort              sCurrRowID  = 0;
    qcTemplate        * sTemplate   = NULL;

    if ( aNode[0]->node.module == &mtfMin )
    {
        sModule = &mtfMinKeep;
    }
    else if ( aNode[0]->node.module == &mtfMax )
    {
        sModule = &mtfMaxKeep;
    }
    else if ( aNode[0]->node.module == &mtfSum )
    {
        sModule = &mtfSumKeep;
    }
    else if ( aNode[0]->node.module == &mtfAvg )
    {
        sModule = &mtfAvgKeep;
    }
    else if ( aNode[0]->node.module == &mtfCount )
    {
        sModule = &mtfCountKeep;
    }
    else if ( aNode[0]->node.module == &mtfVariance )
    {
        sModule = &mtfVarianceKeep;
    }
    else if ( aNode[0]->node.module == &mtfStddev )
    {
        sModule = &mtfStddevKeep;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_FUNC );
    }

    // ���� ���� ���
    sNodeArgCnt = aNode[0]->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // ���� module Flag ����
    aNode[0]->node.lflag &= ~(aNode[0]->node.module->lflag);

    // �� module flag ��ġ
    aNode[0]->node.lflag |= sModule->lflag & ~MTC_NODE_ARGUMENT_COUNT_MASK;

    // ����� ���� ���� ����
    aNode[0]->node.lflag |= sNodeArgCnt;

    aNode[0]->node.module = sModule;

    sTemplate = QC_SHARED_TMPLATE( aStatement );
    sCurrRowID = sTemplate->tmplate.currentRow[MTC_TUPLE_TYPE_INTERMEDIATE];
    if ( sTemplate->tmplate.rows[sCurrRowID].columnCount + 1 >
         sTemplate->tmplate.rows[sCurrRowID].columnMaximum )
    {
        IDE_TEST( qtc::nextColumn( QC_QMP_MEM(aStatement),
                                   aNode[0],
                                   aStatement,
                                   QC_SHARED_TMPLATE(aStatement),
                                   MTC_TUPLE_TYPE_INTERMEDIATE,
                                   sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK )
                  != IDE_SUCCESS );
    }
    else
    {
        sTemplate->tmplate.rows[sCurrRowID].columnCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtc::changeKeepNode",
                                  "Invalid Function" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-45745
idBool qtc::isSameType( mtcColumn * aColumn1,
                        mtcColumn * aColumn2 )
{
    idBool  sIsSameType = ID_FALSE;
    
    if ( mtc::isSameType( aColumn1, aColumn2 ) )
    {
        sIsSameType = ID_TRUE;
    }
    else
    {
        sIsSameType = ID_FALSE;
    }
    
    return sIsSameType;
}

IDE_RC qtc::estimateSerializeFilter( qcStatement * aStatement,
                                     mtcNode     * aNode,
                                     UInt        * aCount )
{
    qcTemplate           * sTemplate  = QC_SHARED_TMPLATE( aStatement );
    mtcNode              * sNode      = NULL;
    mtcNode              * sArgument  = NULL;
    mtxSerialExecuteFunc   sExecute   = mtx::calculateNA;
    UChar                  sEntryType = QTC_ENTRY_TYPE_NONE;
    UInt                   sArgCount  = 0;
    UInt                   sCount     = 0;

    IDE_TEST( checkSerializeFilterType( sTemplate,
                                        aNode,
                                        &( sArgument ),
                                        &( sExecute ),
                                        &( sEntryType ) )
              != IDE_SUCCESS );

    for ( sNode  = sArgument;
          sNode != NULL;
          sNode  = sNode->next )
    {
        IDE_TEST( estimateSerializeFilter( aStatement,
                                           sNode,
                                           aCount )
                  != IDE_SUCCESS );

        sArgCount += 1;
    }

    if ( ( sEntryType == QTC_ENTRY_TYPE_AND ) ||
         ( sEntryType == QTC_ENTRY_TYPE_OR  ) )
    {
        sCount += sArgCount;
    }
    else
    {
        IDE_TEST( sArgCount > 2 );

        sCount += 1;
    }

    for ( sNode  = aNode->conversion;
          sNode != NULL;
          sNode  = sNode->conversion )
    {
        sExecute = sTemplate->tmplate.rows[sNode->table].execute[sNode->column].mSerialExecute;

        IDE_TEST( sExecute == mtx::calculateNA );

        sCount += 1;
    }

    *aCount += sCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::allocateSerializeFilter( qcStatement          * aStatement,
                                     UInt                   aCount,
                                     mtxSerialFilterInfo ** aInfo )
{
    IDU_FIT_POINT( "qtc::allocateSerializeFilter::alloc",
                   idERR_ABORT_InsufficientMemory );
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( mtxSerialFilterInfo ) * aCount,
                                               (void **) aInfo )
              != IDE_SUCCESS );

    QTC_SET_SERIAL_FILTER_INFO( (*aInfo),
                                NULL,
                                mtx::calculateNA,
                                0,
                                NULL,
                                NULL );

    QTC_SET_ENTRY_HEADER( (*aInfo)->mHeader,
                          0,
                          0,
                          0,
                          0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::recursiveSerializeFilter( qcStatement          * aStatement,
                                      mtcNode              * aNode,
                                      mtxSerialFilterInfo  * aFense,
                                      mtxSerialFilterInfo ** aInfo )
{
    qcTemplate           * sTemplate    = QC_SHARED_TMPLATE( aStatement );
    mtcNode              * sNode        = NULL;
    mtcNode              * sArgument    = NULL;
    UInt                   sIndex       = 0;
    mtxSerialFilterInfo  * sOffset[2]   = { NULL, NULL };
    UChar                  sEntryType   = QTC_ENTRY_TYPE_NONE;
    mtxSerialExecuteFunc   sExecute     = mtx::calculateNA;
    mtxSerialFilterInfo  * sPrev        = NULL;

    IDE_TEST( checkSerializeFilterType( sTemplate,
                                        aNode,
                                        &( sArgument ),
                                        &( sExecute ),
                                        &( sEntryType ) )
              != IDE_SUCCESS );

    /* Host Constant Wrapper
     *  \
     *   [CHECK] [?] [?] [?]
     *     \       \   \   \
     *      sPrev    Unknown
     */
    if ( sEntryType == QTC_ENTRY_TYPE_CHECK )
    {
        IDE_TEST( setSerializeFilter( QTC_ENTRY_TYPE_CHECK,
                                      QTC_ENTRY_COUNT_ONE,
                                      aNode,
                                      sExecute,
                                      NULL,
                                      NULL,
                                      aFense,
                                      aInfo )
                  != IDE_SUCCESS );

        sPrev = (*aInfo);
    }
    else
    {
        sPrev = NULL;
    }

    for ( sNode  = sArgument;
          sNode != NULL;
          sNode  = sNode->next )
    {
        IDE_TEST( recursiveSerializeFilter( aStatement ,
                                            sNode,
                                            aFense,
                                            aInfo )
                  != IDE_SUCCESS );

        if ( ( sEntryType == QTC_ENTRY_TYPE_AND ) ||
             ( sEntryType == QTC_ENTRY_TYPE_OR ) )
        {
            IDE_TEST( setSerializeFilter( sEntryType,
                                          QTC_ENTRY_COUNT_TWO,
                                          aNode,
                                          sExecute,
                                          (*aInfo),
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            /* Linking AND / OR
             *  \
             *   [OP1] [AND'] [OP2] [AND''] [OP 3] [AND''']
             *             ^         /   ^         /
             *              \ mRight      \ mRight
             */
            if ( sPrev == NULL )
            {
                sPrev = (*aInfo);
            }
            else
            {
                (*aInfo)->mRight = sPrev;
                sPrev            = (*aInfo);
            }
        }
        else
        {
            IDE_TEST( sIndex >= 2 );

            sOffset[sIndex++] = (*aInfo);
        }
    }

    IDE_TEST( setParentSerializeFilter( sEntryType,
                                        sIndex,
                                        aNode,
                                        sExecute,
                                        sOffset[0],
                                        sOffset[1],
                                        aFense,
                                        aInfo )
                      != IDE_SUCCESS );

    IDE_TEST( adjustSerializeFilter( sEntryType,
                                     sPrev,
                                     aInfo )
                   != IDE_SUCCESS );

    if ( aNode->conversion != NULL )
    {
        IDE_TEST( recursiveConversionSerializeFilter( aStatement ,
                                                      aNode->conversion,
                                                      aFense,
                                                      aInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::recursiveConversionSerializeFilter( qcStatement          * aStatement,
                                                mtcNode              * aNode,
                                                mtxSerialFilterInfo  * aFense,
                                                mtxSerialFilterInfo ** aInfo )
{
    qcTemplate         * sTemplate  = QC_SHARED_TMPLATE( aStatement );
    UChar                sEntryType = QTC_ENTRY_TYPE_NONE;
    mtxSerialExecuteFunc sExecute   = mtx::calculateNA;

    if ( aNode->conversion != NULL )
    {
        IDE_TEST( recursiveConversionSerializeFilter( aStatement ,
                                                      aNode->conversion,
                                                      aFense,
                                                      aInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( checkConversionSerializeFilterType( sTemplate,
                                                  aNode,
                                                  (*aInfo)->mNode,
                                                  &( sExecute ),
                                                  &( sEntryType ) )
              != IDE_SUCCESS );

    IDE_TEST( setConversionSerializeFilter( sEntryType,
                                            aNode,
                                            sExecute,
                                            aFense,
                                            aInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::checkSerializeFilterType( qcTemplate           * aTemplate,
                                      mtcNode              * aNode,
                                      mtcNode             ** aArgument,
                                      mtxSerialExecuteFunc * aExecute,
                                      UChar                * aEntryType )
{
    mtcNode            * sArgument  = aNode->arguments;
    UChar                sEntryType = QTC_ENTRY_TYPE_NONE;
    mtxSerialExecuteFunc sExecute   = mtx::calculateNA;

    if ( ( aNode->module->lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_AND )
    {
        IDE_TEST( sArgument == NULL );

        if ( sArgument->next != NULL )
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_AND;
        }
        else
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_AND_SINGLE;
        }
    }
    else if ( ( aNode->module->lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_OR )
    {
        IDE_TEST( sArgument == NULL );

        if ( sArgument->next != NULL )
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_OR;
        }
        else
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_OR_SINGLE;
        }
    }
    else if ( ( aNode->module->lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_NOT )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_NOT;
    }
    else
    {
        IDE_TEST( checkNodeModuleForNormal( aTemplate,
                                            aNode,
                                            &( sArgument ),
                                            &( sExecute ),
                                            &( sEntryType ) )
                  != IDE_SUCCESS );
    }

    *aExecute   = sExecute;
    *aArgument  = sArgument;
    *aEntryType = sEntryType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::checkNodeModuleForNormal( qcTemplate           * aTemplate,
                                      mtcNode              * aNode,
                                      mtcNode             ** aArgument,
                                      mtxSerialExecuteFunc * aExecute,
                                      UChar                * aEntryType )
{
    mtcNode            * sArgument  = aNode->arguments;
    UChar                sEntryType = QTC_ENTRY_TYPE_NONE;
    mtxSerialExecuteFunc sExecute   = mtx::calculateNA;

    if ( aNode->module == &gQtcRidModule )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].ridExecute->mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_RID;

        IDE_TEST( sExecute == mtx::calculateNA );
    }
    else if ( aNode->module == &qtc::columnModule )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_COLUMN;

        IDE_TEST( sExecute == mtx::calculateNA );
    }
    else if ( aNode->module == &qtc::valueModule )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_VALUE;

        if ( ( aNode->orgNode != NULL ) &&
             ( sArgument      != NULL ) )
        {
            sArgument = NULL;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        IDE_TEST( checkNodeModuleForEtc( aTemplate,
                                         aNode,
                                         &( sArgument ),
                                         &( sExecute ),
                                         &( sEntryType ) )
                  != IDE_SUCCESS );
    }

    *aExecute   = sExecute;
    *aArgument  = sArgument;
    *aEntryType = sEntryType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::checkNodeModuleForEtc( qcTemplate           * aTemplate,
                                   mtcNode              * aNode,
                                   mtcNode             ** aArgument,
                                   mtxSerialExecuteFunc * aExecute,
                                   UChar                * aEntryType )
{
    mtcNode            * sArgument  = aNode->arguments;
    UChar                sEntryType = QTC_ENTRY_TYPE_NONE;
    mtxSerialExecuteFunc sExecute   = mtx::calculateNA;

    if ( aNode->module == &qtc::hostConstantWrapperModule )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_CHECK;
    }
    else if ( aNode->module == &mtfLnnvl )
    {
        sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
        sEntryType = QTC_ENTRY_TYPE_LNNVL;
    }
    else
    {
        IDE_TEST( sArgument == NULL );

        if ( sArgument->next != NULL )
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_FUNCTION;
        }
        else
        {
            sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;
            sEntryType = QTC_ENTRY_TYPE_SINGLE;
        }

        IDE_TEST( sExecute == mtx::calculateNA );
    }

    *aExecute   = sExecute;
    *aArgument  = sArgument;
    *aEntryType = sEntryType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::checkConversionSerializeFilterType( qcTemplate           * aTemplate,
                                                mtcNode              * aNode,
                                                mtcNode              * aArgument,
                                                mtxSerialExecuteFunc * aExecute,
                                                UChar                * aEntryType )
{
    UChar                sEntryType = QTC_ENTRY_TYPE_NONE;
    mtcColumn          * sColumn    = aTemplate->tmplate.rows[aNode->table].columns + aNode->column;
    mtcColumn          * sArgument  = aTemplate->tmplate.rows[aArgument->table].columns + aArgument->column;
    mtxSerialExecuteFunc sExecute   = aTemplate->tmplate.rows[aNode->table].execute[aNode->column].mSerialExecute;

    IDE_DASSERT( ( sColumn   != NULL ) &&
                 ( sArgument != NULL ) );

    IDE_TEST( sExecute == mtx::calculateNA );

    switch( sColumn->module->id )
    {
        case MTD_NCHAR_ID:
        case MTD_NVARCHAR_ID:
            switch( sArgument->module->id )
            {
                case MTD_CHAR_ID:
                case MTD_VARCHAR_ID:
                    sEntryType = QTC_ENTRY_TYPE_CONVERT_CHAR;

                    break;

                default:
                    sEntryType = QTC_ENTRY_TYPE_CONVERT;

                    break;
            }

            break;

        case MTD_CHAR_ID:
        case MTD_VARCHAR_ID:
            switch( sArgument->module->id )
            {
                case MTD_NCHAR_ID:
                case MTD_NVARCHAR_ID:
                    sEntryType = QTC_ENTRY_TYPE_CONVERT_CHAR;

                    break;

                case MTD_DATE_ID:
                    sEntryType = QTC_ENTRY_TYPE_CONVERT_DATE;

                    break;

                default:
                    sEntryType = QTC_ENTRY_TYPE_CONVERT;

                    break;
            }

            break;

        case MTD_DATE_ID:
            sEntryType = QTC_ENTRY_TYPE_CONVERT_DATE;

            break;

        default:
            sEntryType = QTC_ENTRY_TYPE_CONVERT;

            break;
    }

    *aExecute   = sExecute;
    *aEntryType = sEntryType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::adjustSerializeFilter( UChar                  aType,
                                   mtxSerialFilterInfo  * aPrev,
                                   mtxSerialFilterInfo ** aInfo )
{
    mtxSerialFilterInfo * sAnd  = (*aInfo)->mRight;
    mtxSerialFilterInfo * sPrev = NULL;
    mtxSerialFilterInfo * sLast = (*aInfo);

    switch( aType )
    {
        case QTC_ENTRY_TYPE_CHECK:
            IDE_TEST_RAISE( ( ( aPrev    == NULL ) ||
                              ( (*aInfo) == NULL ) ),
                            INVALID_ACCESS_FILTER );

            /* Host Constant Wrapper
             *  \
             *   [CHECK] [OP1] [OP2] [OP3]
             *     \                     \
             *      sPrev                 aInfo = sLast
             *
             * After Adjust
             *  \
             *   [CHECK] [OP1] [OP2] [OP3]
             *     \                  ^
             *      mLeft -----------/
             */
            aPrev->mLeft = sLast;

            break;

        case QTC_ENTRY_TYPE_AND:
        case QTC_ENTRY_TYPE_OR:
            IDE_TEST_RAISE( (*aInfo) == NULL,
                            INVALID_ACCESS_FILTER );

            /* AND / OR Optimize
             *  \
             *   [OP1] [AND'] [OP2] [AND''] [OP 3] [AND''']
             *             ^         /   ^         /      \
             *              \ mRight      \ mRight         aInfo = sLast
             *
             * After Adjust
             *  \
             *   [OP1] [AND'] [OP2] [AND''] [OP 3] [AND''']
             *          \            \              \     ^
             *           mRight ----- mRight ------- mRight = sLast
             */
            while ( sAnd != NULL )
            {
                sPrev        = sAnd->mRight;
                sAnd->mRight = sLast;
                sAnd         = sPrev;
            }

            sLast->mRight = sLast;

            break;

        case QTC_ENTRY_TYPE_RID:
        case QTC_ENTRY_TYPE_COLUMN:
        case QTC_ENTRY_TYPE_VALUE:
        case QTC_ENTRY_TYPE_FUNCTION:
        case QTC_ENTRY_TYPE_SINGLE:
        case QTC_ENTRY_TYPE_AND_SINGLE:
        case QTC_ENTRY_TYPE_OR_SINGLE:
        case QTC_ENTRY_TYPE_NOT:
        case QTC_ENTRY_TYPE_LNNVL:

            break;

        default:
            IDE_TEST( 1 );

            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ACCESS_FILTER )
    {
        ideLog::log( IDE_QP_0,
                     "Serial Filter Failure: %s: %s",
                     "qtc::adjustSerializeFilter",
                     "invalid access filter" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::setConversionSerializeFilter( UChar                  aType,
                                          mtcNode              * aNode,
                                          mtxSerialExecuteFunc   aExecute,
                                          mtxSerialFilterInfo  * aFense,
                                          mtxSerialFilterInfo ** aInfo )
{
    switch( aType )
    {
        case QTC_ENTRY_TYPE_CONVERT:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_ONE,
                                          aNode,
                                          aExecute,
                                          (*aInfo),
                                          NULL,
                                          aFense,
                                          aInfo )
                  != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_CONVERT_CHAR:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_TWO,
                                          aNode,
                                          aExecute,
                                          (*aInfo),
                                          NULL,
                                          aFense,
                                          aInfo )
                  != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_CONVERT_DATE:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_THREE,
                                          aNode,
                                          aExecute,
                                          (*aInfo),
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_RID:
        case QTC_ENTRY_TYPE_COLUMN:
        case QTC_ENTRY_TYPE_VALUE:
        case QTC_ENTRY_TYPE_FUNCTION:
        case QTC_ENTRY_TYPE_SINGLE:
        case QTC_ENTRY_TYPE_AND_SINGLE:
        case QTC_ENTRY_TYPE_OR_SINGLE:
        case QTC_ENTRY_TYPE_NOT:
        case QTC_ENTRY_TYPE_LNNVL:
        case QTC_ENTRY_TYPE_CHECK:
        case QTC_ENTRY_TYPE_AND:
        case QTC_ENTRY_TYPE_OR:
        default:

            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::setParentSerializeFilter( UChar                  aType,
                                      UChar                  aCount,
                                      mtcNode              * aNode,
                                      mtxSerialExecuteFunc   aExecute,
                                      mtxSerialFilterInfo  * aLeft,
                                      mtxSerialFilterInfo  * aRight,
                                      mtxSerialFilterInfo  * aFense,
                                      mtxSerialFilterInfo ** aInfo  )
{
    switch( aType )
    {
        case QTC_ENTRY_TYPE_VALUE:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_ZERO,
                                          aNode,
                                          aExecute,
                                          NULL,
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_RID:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_ONE,
                                          aNode,
                                          aExecute,
                                          NULL,
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_COLUMN:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_TWO,
                                          aNode,
                                          aExecute,
                                          NULL,
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_FUNCTION:
            IDE_TEST( setSerializeFilter( aType,
                                          aCount,
                                          aNode,
                                          aExecute,
                                          aLeft,
                                          aRight,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_AND_SINGLE:
        case QTC_ENTRY_TYPE_OR_SINGLE:
        case QTC_ENTRY_TYPE_NOT:
        case QTC_ENTRY_TYPE_LNNVL:
        case QTC_ENTRY_TYPE_SINGLE:
            IDE_TEST( setSerializeFilter( aType,
                                          QTC_ENTRY_COUNT_ONE,
                                          aNode,
                                          aExecute,
                                          (*aInfo),
                                          NULL,
                                          aFense,
                                          aInfo )
                      != IDE_SUCCESS );

            break;

        case QTC_ENTRY_TYPE_CHECK:
        case QTC_ENTRY_TYPE_AND:
        case QTC_ENTRY_TYPE_OR:
        default:

            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtc::setSerializeFilter( UChar                  aType,
                                UChar                  aCount,
                                mtcNode              * aNode,
                                mtxSerialExecuteFunc   aExecute,
                                mtxSerialFilterInfo  * aLeft,
                                mtxSerialFilterInfo  * aRight,
                                mtxSerialFilterInfo  * aFense,
                                mtxSerialFilterInfo ** aInfo )
{
    mtxSerialFilterInfo * sInfo   = (*aInfo);
    mtxSerialFilterInfo * sLeft   = aLeft;
    mtxSerialFilterInfo * sRight  = aRight;
    UInt                  sNumber = 1;
    UChar                 sSize   = aCount;
    UInt                  sOffset = 0;

    switch( aType )
    {
        case QTC_ENTRY_TYPE_CHECK:
            sSize += QTC_ENTRY_SIZE_ONE;
            break;

        case QTC_ENTRY_TYPE_VALUE:
        case QTC_ENTRY_TYPE_RID:
        case QTC_ENTRY_TYPE_COLUMN:
        case QTC_ENTRY_TYPE_AND:
        case QTC_ENTRY_TYPE_OR:
        case QTC_ENTRY_TYPE_AND_SINGLE:
        case QTC_ENTRY_TYPE_OR_SINGLE:
        case QTC_ENTRY_TYPE_NOT:
        case QTC_ENTRY_TYPE_LNNVL:
            sSize += QTC_ENTRY_SIZE_TWO;
            break;

        case QTC_ENTRY_TYPE_FUNCTION:
        case QTC_ENTRY_TYPE_SINGLE:
        case QTC_ENTRY_TYPE_CONVERT:
        case QTC_ENTRY_TYPE_CONVERT_CHAR:
        case QTC_ENTRY_TYPE_CONVERT_DATE:
            sSize += QTC_ENTRY_SIZE_THREE;
            break;

        default:
            sSize += QTC_ENTRY_SIZE_ZERO;
            break;
    }

    if ( sInfo->mHeader.mId == 0 )
    {
        QTC_SET_SERIAL_FILTER_INFO( sInfo,
                                    aNode,
                                    aExecute,
                                    sOffset,
                                    sLeft,
                                    sRight );

        QTC_SET_ENTRY_HEADER( sInfo->mHeader,
                              sNumber,
                              aType,
                              sSize,
                              aCount );
    }
    else
    {
        sNumber = sInfo->mHeader.mId + 1;
        sOffset = sInfo->mOffset + sInfo->mHeader.mSize;

        IDE_TEST_RAISE( sNumber >= ID_USHORT_MAX,
                        TOO_MANY_FILTER );

        *aInfo = *aInfo + 1;

        IDE_TEST_RAISE( (*aInfo) > aFense,
                        INVALID_ACCESS_FILTER );

        sInfo = (*aInfo);

        QTC_SET_SERIAL_FILTER_INFO( sInfo,
                                    aNode,
                                    aExecute,
                                    sOffset,
                                    sLeft,
                                    sRight );

        QTC_SET_ENTRY_HEADER( sInfo->mHeader,
                              sNumber,
                              aType,
                              sSize,
                              aCount );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( TOO_MANY_FILTER )
    {
        ideLog::log( IDE_QP_0,
                     "Serial Filter Failure: %s: %s",
                     "qtc::setSerializeFilter",
                     "too many filter node" );
    }
    IDE_EXCEPTION( INVALID_ACCESS_FILTER )
    {
        ideLog::log( IDE_QP_0,
                     "Serial Filter Failure: %s: %s",
                     "qtc::setSerializeFilter",
                     "invalid access filter" );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
