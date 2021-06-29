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
 * $Id: stfWKB.cpp 18883 2006-11-14 01:48:40Z sabbra $
 *
 * Description:
 * WKB(Well Known Binary)�κ��� Geometry ��ü �����ϴ� �Լ�
 * �� ������ stdParsing.cpp �� �ִ�.
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <mtdTypes.h>
#include <ste.h>
#include <stdTypes.h>
#include <stdParsing.h>
#include <stfWKB.h>
#include <stdUtils.h>

extern mtdModule mtdInteger;

/***********************************************************************
 * Description:
 * WKB�κ��� Geometry Type�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * UInt*    aType(Out): ���� WKB Type
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/

IDE_RC stfWKB::typeFromWKB( UChar *aWKB, UInt *aType )
{
    idBool sIsEquiEndian = ID_FALSE;
    
    IDE_TEST_RAISE( aWKB == NULL, ERR_ABORT_INVALID_WKB );
       
    // BUG-24357 WKB Endian
    IDE_TEST( stdUtils::compareEndian( *aWKB, &sIsEquiEndian )
              != IDE_SUCCESS );

    idlOS::memcpy( aType, aWKB+1, WKB_INT32_SIZE );

    if ( sIsEquiEndian == ID_FALSE )
    {
        mtdInteger.endian(aType);
    }
    else
    {
        // nothing to do
    }
 
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_INVALID_WKB )
    {    
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNEXPECTED_ERROR,
                                  "stfWKB::typeFromWKB",
                                  "invalid WKB" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * BUG-32531 Consider for GIS EMPTY
 * WKB�κ��� EMPTY�� CHECK �ϱ� ���� Multipoint�� POINT ������
 * ���Ѵ�.
 *
 * void* aWKB(In): �о� ���� ����
 * UInt* aType(Out): ���� WKB Count Number
 * IDE_RC* aResult(Out): Error code
 **********************************************************************/
 	
IDE_RC stfWKB::cntNumFromWKB( UChar *aWKB, UInt *aVal )
{
 	IDE_DASSERT( aWKB != NULL );
 	
 	idlOS::memcpy( aVal, ((WKBMultiPoint*)aWKB)->mNumWKBPoints, WKB_INT32_SIZE );
 	
 	return IDE_SUCCESS;
} 

/***********************************************************************
 * Description:
 * WKB�κ��� Geometry ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::geomFromWKB( iduMemory*   aQmxMem,
                            void*        aWKB,
                            void*        aBuf,
                            void*        aFence,
                            IDE_RC*      aResult,
                            UInt         aValidateOption )
{
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
}

/***********************************************************************
 * Description:
 * WKB�κ��� ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::pointFromWKB( iduMemory*   aQmxMem,
                             void*        aWKB,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption ) 
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0; 
    
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;

    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    /* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
	{
        IDE_TEST_RAISE(sWkbType != WKB_POINT_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE(sWkbType != WKB_POINT_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	} 
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� ���� ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::lineFromWKB( iduMemory*   aQmxMem,
                            void*        aWKB,
                            void*        aBuf,
                            void*        aFence,
                            IDE_RC*      aResult,
                            UInt         aValidateOption )
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0;
    
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    /* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
 	{
        IDE_TEST_RAISE(sWkbType != WKB_LINESTRING_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE(sWkbType != WKB_LINESTRING_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	} 
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� ������ ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::polyFromWKB( iduMemory*   aQmxMem,
                            void*        aWKB,
                            void*        aBuf,
                            void*        aFence,
                            IDE_RC*      aResult,
                            UInt         aValidateOption )
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0;  
    
    // Fix BUG-15834
    UChar* sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    /* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
 	{
        IDE_TEST_RAISE(sWkbType != WKB_POLYGON_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE(sWkbType != WKB_POLYGON_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	} 
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  WKB�κ��� RECTANGLE ��ü�� �о� ���δ�.
 *
 *  void   * aWKB(In)     : �о� ���� ����
 *  void   * aBuf(Out)    : ����� ����
 *  void   * aFence(In)   : ����� ���� �潺
 *  IDE_RC * aResult(Out) : Error code
 **********************************************************************/
IDE_RC stfWKB::rectFromWKB( iduMemory * aQmxMem,
                            void      * aWKB,
                            void      * aBuf,
                            void      * aFence,
                            IDE_RC    * aResult,
                            UInt        aValidateOption )
{
    UInt       sWkbType   = WKB_RECTANGLE_TYPE;

    // Fix BUG-15834
    UChar    * sWKB       = (UChar *)((mtdBinaryType *)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType *)aWKB)->mLength;

    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sWkbType != WKB_RECTANGLE_TYPE, ERR_OBJECT_TYPE );

    return stdParsing::stdBinValue( aQmxMem,
                                    sWKB,
                                    sWKBLength,
                                    aBuf,
                                    aFence,
                                    aResult,
                                    aValidateOption );

    IDE_EXCEPTION( ERR_OBJECT_TYPE );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� ��Ƽ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::mpointFromWKB( iduMemory*   aQmxMem,
                              void*        aWKB,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption ) 
{
    UInt       sWkbType;
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );
    
    IDE_TEST_RAISE(sWkbType != WKB_MULTIPOINT_TYPE, err_object_type);
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� ��Ƽ���� ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::mlineFromWKB( iduMemory*   aQmxMem,
                             void*        aWKB,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption )
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0;   
    
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    /* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
 	{
        IDE_TEST_RAISE(sWkbType != WKB_MULTILINESTRING_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE(sWkbType != WKB_MULTILINESTRING_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	} 
   
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� ��Ƽ������ ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::mpolyFromWKB( iduMemory*   aQmxMem,
                             void*        aWKB,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption )
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0; 
    
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

 	/* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
	{
        IDE_TEST_RAISE(sWkbType != WKB_MULTIPOLYGON_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE(sWkbType != WKB_MULTIPOLYGON_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	}  
    
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKB�κ��� �ݷ��� ��ü�� �о� ���δ�.
 *
 * void*    aWKB(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKB::geoCollFromWKB( iduMemory*   aQmxMem,
                               void*        aWKB,
                               void*        aBuf,
                               void*        aFence,
                               IDE_RC*      aResult,
                               UInt         aValidateOption )
{
    UInt       sWkbType;
    UInt       sWkbCntNum = 0; 
    
    // Fix BUG-15834
    UChar    * sWKB = (UChar*)((mtdBinaryType*)aWKB)->mValue;
    UInt       sWKBLength = ((mtdBinaryType*)aWKB)->mLength;
    
    IDE_TEST( typeFromWKB( sWKB, &sWkbType ) != IDE_SUCCESS );

    /* BUG-32531 Consider for GIS EMPTY */
 	if ( sWkbType != WKB_MULTIPOINT_TYPE )
 	{
        IDE_TEST_RAISE( sWkbType != WKB_COLLECTION_TYPE, err_object_type);
 	}
 	else
 	{
        cntNumFromWKB( sWKB, &sWkbCntNum);
 	
        if ( sWkbCntNum != 0 )
        {
            IDE_TEST_RAISE( sWkbType != WKB_COLLECTION_TYPE, err_object_type);
        }
        else
        {
            // nothing to do
        }
 	} 
    
    return stdParsing::stdBinValue(aQmxMem,
                                   sWKB,
                                   sWKBLength,
                                   aBuf,
                                   aFence,
                                   aResult,
                                   aValidateOption);
    
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


