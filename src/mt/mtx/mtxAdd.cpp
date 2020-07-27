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
 * $Id: mtxAdd.cpp 85045 2019-03-20 01:40:10Z andrew.shin $
 **********************************************************************/
#include <mtx.h>

extern mtxModule mtxAdd;

static mtxSerialExecuteFunc mtxAddGetSerialExecute( UInt aMtd1Type,
                                                    UInt aMtd2Type );

static IDE_RC mtxAddInteger( mtxEntry ** aEntry );
static IDE_RC mtxAddBigint( mtxEntry ** aEntry );
static IDE_RC mtxAddReal( mtxEntry ** aEntry );
static IDE_RC mtxAddDouble( mtxEntry ** aEntry );
static IDE_RC mtxAddFloat( mtxEntry ** aEntry );
static IDE_RC mtxAddDateInterval( mtxEntry ** aEntry );
static IDE_RC mtxAddIntervalDate( mtxEntry ** aEntry );
static IDE_RC mtxAddInterval( mtxEntry ** aEntry );

mtxModule mtxAdd = {
    MTX_EXECUTOR_ID_ADD,
    mtx::calculateNA,
    mtxAddGetSerialExecute
};

static mtxSerialExecuteFunc mtxAddGetSerialExecute( UInt aMtd1Type,
                                                    UInt aMtd2Type )
{
    mtxSerialExecuteFunc sFunc = mtx::calculateNA;

    switch ( aMtd1Type )
    {
        case MTD_INTEGER_ID:
            sFunc = mtxAddInteger;

            break;

        case MTD_NUMERIC_ID:
        case MTD_FLOAT_ID:
            sFunc = mtxAddFloat;

            break;

        case MTD_REAL_ID:
            sFunc = mtxAddReal;

            break;

        case MTD_DOUBLE_ID:
            sFunc = mtxAddDouble;

            break;

        case MTD_DATE_ID:
            switch ( aMtd2Type )
            {
                case MTD_INTERVAL_ID:
                    sFunc = mtxAddDateInterval;

                    break;

                default:
                    sFunc = mtx::calculateNA;

                    break;
            }

            break;

        case MTD_INTERVAL_ID:
            switch ( aMtd2Type )
            {
                case MTD_DATE_ID:
                    sFunc = mtxAddIntervalDate;

                    break;

                case MTD_INTERVAL_ID:
                    sFunc = mtxAddInterval;

                    break;

                default:
                    sFunc = mtx::calculateNA;

                    break;
            }

            break;

        case MTD_BIGINT_ID:
            sFunc = mtxAddBigint;

            break;

        default:
            sFunc = mtx::calculateNA;

            break;
    }

    return sFunc;
}

static IDE_RC mtxAddInteger( mtxEntry ** aEntry )
{
    mtdIntegerType * sReturn   = (mtdIntegerType *)aEntry[0]->mAddress;
    UChar            sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdIntegerType   sArg1Val  = *(mtdIntegerType *)( aEntry[1] + 1 )->mAddress;
    UChar            sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdIntegerType   sArg2Val  = *(mtdIntegerType *)( aEntry[2] + 1 )->mAddress;
    mtdBigintType    sBigint;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTX_IS_NULL_INTEGER( sArg1Val ) || MTX_IS_NULL_INTEGER( sArg2Val ) )
    {
        MTX_SET_NULL_INTEGER( sReturn );
    }
    else
    {
        sBigint  = sArg1Val;
        sBigint += sArg2Val;

        IDE_TEST_RAISE( sBigint > MTD_INTEGER_MAXIMUM,
                        ERR_OVERFLOW );

        IDE_TEST_RAISE( sBigint < MTD_INTEGER_MINIMUM,
                        ERR_OVERFLOW );

        *sReturn = sBigint;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddInteger",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION( ERR_OVERFLOW )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddBigint( mtxEntry ** aEntry )
{
    mtdBigintType * sReturn   = (mtdBigintType *)aEntry[0]->mAddress;
    UChar           sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdBigintType   sArg1Val  = *(mtdBigintType *)( aEntry[1] + 1 )->mAddress;
    UChar           sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdBigintType   sArg2Val  = *(mtdBigintType *)( aEntry[2] + 1 )->mAddress;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTX_IS_NULL_BIGINT( sArg1Val ) || MTX_IS_NULL_BIGINT( sArg2Val ) )
    {
        MTX_SET_NULL_BIGINT( sReturn );
    }
    else
    {
        if ( ( sArg1Val > 0 ) && ( sArg2Val > 0 ) )
        {
            IDE_TEST_RAISE( ( MTD_BIGINT_MAXIMUM - sArg1Val ) < sArg2Val,
                            ERR_OVERFLOW );
        }
        else
        {
            if ( ( sArg1Val < 0 ) && ( sArg2Val < 0 ) )
            {
                IDE_TEST_RAISE( ( MTD_BIGINT_MINIMUM - sArg1Val ) > sArg2Val,
                                ERR_OVERFLOW );
            }
            else
            {
                /* Nothing to do */
            }
        }

        *sReturn = sArg1Val + sArg2Val;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddBigint",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION( ERR_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_VALUE_OVERFLOW ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddReal( mtxEntry ** aEntry )
{
    mtdRealType * sReturn   = (mtdRealType *)aEntry[0]->mAddress;
    UChar         sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdRealType * sArg1Val  = (mtdRealType *)( aEntry[1] + 1 )->mAddress;
    UChar         sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdRealType * sArg2Val  = (mtdRealType *)( aEntry[2] + 1 )->mAddress;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTX_IS_NULL_REAL( sArg1Val ) || MTX_IS_NULL_REAL( sArg2Val ) )
    {
        MTX_SET_NULL_REAL( sReturn );
    }
    else
    {
        *sReturn = *sArg1Val + *sArg2Val;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddReal",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddDouble( mtxEntry ** aEntry )
{
    mtdDoubleType * sReturn   = (mtdDoubleType *)aEntry[0]->mAddress;
    UChar           sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdDoubleType * sArg1Val  = (mtdDoubleType *)( aEntry[1] + 1 )->mAddress;
    UChar           sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdDoubleType * sArg2Val  = (mtdDoubleType *)( aEntry[2] + 1 )->mAddress;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTX_IS_NULL_DOUBLE( sArg1Val ) || MTX_IS_NULL_DOUBLE( sArg2Val ) )
    {
        MTX_SET_NULL_DOUBLE( sReturn );
    }
    else
    {
        *sReturn = *sArg1Val + *sArg2Val;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddDouble",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddFloat( mtxEntry ** aEntry )
{
    mtdNumericType * sReturn   = (mtdNumericType *)aEntry[0]->mAddress;
    UChar            sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdNumericType * sArg1Val  = (mtdNumericType *)( aEntry[1] + 1 )->mAddress;
    UChar            sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdNumericType * sArg2Val  = (mtdNumericType *)( aEntry[2] + 1 )->mAddress;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTX_IS_NULL_FLOAT( sArg1Val ) || MTX_IS_NULL_FLOAT( sArg2Val ) )
    {
        MTX_SET_NULL_FLOAT( sReturn );
    }
    else
    {
        IDE_TEST( mtc::addFloat( sReturn,
                                 MTD_FLOAT_PRECISION_MAXIMUM,
                                 sArg1Val,
                                 sArg2Val )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddFloat",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddDateInterval( mtxEntry ** aEntry )
{
    mtdDateType     * sReturn   = (mtdDateType *)aEntry[0]->mAddress;
    UChar             sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdDateType     * sArg1Val  = (mtdDateType *)( aEntry[1] + 1 )->mAddress;
    UChar             sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdIntervalType * sArg2Val  = (mtdIntervalType *)( aEntry[2] + 1 )->mAddress;
    mtdIntervalType   sInterval;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTD_DATE_IS_NULL( sArg1Val ) || MTD_INTERVAL_IS_NULL( sArg2Val ) )
    {
        MTX_SET_NULL_DATE( sReturn );
    }
    else
    {
        IDE_TEST( mtdDateInterface::convertDate2Interval( sArg1Val,
                                                          & sInterval )
                  != IDE_SUCCESS );

        sInterval.second      += sArg2Val->second;
        sInterval.microsecond += sArg2Val->microsecond;

        sInterval.second      += ( sInterval.microsecond / 1000000 );
        sInterval.microsecond %= 1000000;

        if ( ( sInterval.microsecond < 0 ) && ( sInterval.second > 0 ) )
        {
            sInterval.second--;
            sInterval.microsecond += 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sInterval.microsecond > 0 ) && ( sInterval.second < 0 ) )
        {
            sInterval.second++;
            sInterval.microsecond -= 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST_RAISE( ( sInterval.second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                        ( sInterval.second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                        ( ( sInterval.second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                          ( sInterval.microsecond < 0 ) ),
                        ERR_INVALID_YEAR );

        IDE_TEST( mtdDateInterface::convertInterval2Date( & sInterval,
                                                          sReturn )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddDateInterval",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_YEAR )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_YEAR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddIntervalDate( mtxEntry ** aEntry )
{
    mtdDateType     * sReturn   = (mtdDateType *)aEntry[0]->mAddress;
    UChar             sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdIntervalType * sArg1Val  = (mtdIntervalType *)( aEntry[1] + 1 )->mAddress;
    UChar             sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdDateType     * sArg2Val  = (mtdDateType *)( aEntry[2] + 1 )->mAddress;
    mtdIntervalType   sInterval;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTD_INTERVAL_IS_NULL( sArg1Val ) || MTD_DATE_IS_NULL( sArg2Val ) )
    {
        MTX_SET_NULL_DATE( sReturn );
    }
    else
    {
        IDE_TEST( mtdDateInterface::convertDate2Interval( sArg2Val,
                                                          & sInterval )
                  != IDE_SUCCESS );

        sInterval.second      += sArg1Val->second;
        sInterval.microsecond += sArg1Val->microsecond;

        sInterval.second      += ( sInterval.microsecond / 1000000 );
        sInterval.microsecond %= 1000000;

        if ( ( sInterval.microsecond < 0 ) && ( sInterval.second > 0 ) )
        {
            sInterval.second--;
            sInterval.microsecond += 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sInterval.microsecond > 0 ) && ( sInterval.second < 0 ) )
        {
            sInterval.second++;
            sInterval.microsecond -= 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST_RAISE( ( sInterval.second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                        ( sInterval.second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                        ( ( sInterval.second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                          ( sInterval.microsecond < 0 ) ),
                        ERR_INVALID_YEAR );

        IDE_TEST( mtdDateInterface::convertInterval2Date( & sInterval,
                                                          sReturn )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddDateInterval",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_YEAR )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_YEAR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

static IDE_RC mtxAddInterval( mtxEntry ** aEntry )
{
    mtdIntervalType * sReturn   = (mtdIntervalType *)aEntry[0]->mAddress;
    UChar             sArg1Flag = aEntry[1]->mHeader.mFlag;
    mtdIntervalType * sArg1Val  = (mtdIntervalType *)( aEntry[1] + 1 )->mAddress;
    UChar             sArg2Flag = aEntry[2]->mHeader.mFlag;
    mtdIntervalType * sArg2Val  = (mtdIntervalType *)( aEntry[2] + 1 )->mAddress;

    IDE_TEST_RAISE( MTX_IS_NOT_CALCULATED( sArg1Flag ) || MTX_IS_NOT_CALCULATED( sArg2Flag ),
                    INVALID_ENTRY_ORDER );

    if ( MTD_INTERVAL_IS_NULL( sArg1Val ) || MTD_INTERVAL_IS_NULL( sArg2Val ) )
    {
        MTX_SET_NULL_INTERVAL( sReturn );
    }
    else
    {
        sReturn->second      = sArg1Val->second + sArg2Val->second;
        sReturn->microsecond = sArg1Val->microsecond + sArg2Val->microsecond;

        sReturn->second      += ( sReturn->microsecond / 1000000 );
        sReturn->microsecond %= 1000000;

        if ( ( sReturn->microsecond < 0 ) && ( sReturn->second > 0 ) )
        {
            sReturn->second--;
            sReturn->microsecond += 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sReturn->microsecond > 0 ) && ( sReturn->second < 0 ) )
        {
            sReturn->second++;
            sReturn->microsecond -= 1000000;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST_RAISE( ( sReturn->second > MTD_DATE_MAX_YEAR_PER_SECOND ) ||
                        ( sReturn->second < MTD_DATE_MIN_YEAR_PER_SECOND ) ||
                        ( ( sReturn->second == MTD_DATE_MIN_YEAR_PER_SECOND ) &&
                          ( sReturn->microsecond < 0 ) ),
                        ERR_INVALID_YEAR );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_ENTRY_ORDER )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_UNEXPECTED_ERROR,
                                  "mtxAddInterval",
                                  "invalid entry order" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_YEAR )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_YEAR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
