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
 * $Id: stfWKT.cpp 18883 2006-11-14 01:48:40Z sabbra $
 *
 * Description:
 * WKT(Well Known Text)�κ��� Geometry ��ü �����ϴ� �Լ�
 * �� ������ stdParsing.cpp �� �ִ�.
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <mtdTypes.h>
#include <ste.h>
#include <stdTypes.h>
#include <stdParsing.h>
#include <stfWKT.h>

/***********************************************************************
 * Description:
 * WKT�κ��� Geometry ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::geomFromText( iduMemory*   aQmxMem,
                             void*        aWKT,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption,
                             idBool       aSRIDOption,
                             SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
}

/***********************************************************************
 * Description:
 * WKT�κ��� ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::pointFromText( iduMemory*   aQmxMem,
                              void*        aWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption,
                              idBool       aSRIDOption,
                              SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_POINT_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_POINT_NAME, 
                                            STD_POINT_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� ���� ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::lineFromText( iduMemory*   aQmxMem,
                             void*        aWKT,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption,
                             idBool       aSRIDOption,
                             SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_LINESTRING_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_LINESTRING_NAME, 
                                            STD_LINESTRING_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� ������ ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::polyFromText( iduMemory*   aQmxMem,
                             void*        aWKT,
                             void*        aBuf,
                             void*        aFence,
                             IDE_RC*      aResult,
                             UInt         aValidateOption,
                             idBool       aSRIDOption,
                             SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE(stdParsing::skipSpace(&sPtr, sWKTFence) != IDE_SUCCESS,
                   err_parsing);
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_POLYGON_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_POLYGON_NAME, 
                                            STD_POLYGON_NAME_LEN ) != 0,
                       err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  WKT�κ��� RECTANGLE ��ü�� �о� ���δ�.
 *
 *  void   * aWKT(In)     : �о� ���� ����
 *  void   * aBuf(Out)    : ����� ����
 *  void   * aFence(In)   : ����� ���� �潺
 *  IDE_RC * aResult(Out) : Error code
 **********************************************************************/
IDE_RC stfWKT::rectFromText( iduMemory  * aQmxMem,
                             void       * aWKT,
                             void       * aBuf,
                             void       * aFence,
                             IDE_RC     * aResult,
                             UInt         aValidateOption,
                             idBool       aSRIDOption,
                             SInt         aSRID )
{
    UChar * sWKT       = ((mtdCharType *)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType *)aWKT)->length;

    UChar * sPtr       = sWKT;
    UChar * sWKTFence  = sWKT + sWKTLength;

    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sWKTFence ) != IDE_SUCCESS,
                    ERR_PARSING );
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_RECTANGLE_NAME_LEN >= sWKTFence, ERR_OBJECT_TYPE );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_RECTANGLE_NAME, 
                                            STD_RECTANGLE_NAME_LEN ) != 0,
                        ERR_OBJECT_TYPE );
    }
    else
    {
        // null object�� ���
    }

    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );

    IDE_EXCEPTION( ERR_PARSING );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_INVALID_WKT ) );
    }
    IDE_EXCEPTION( ERR_OBJECT_TYPE );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� ��Ƽ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::mpointFromText( iduMemory*   aQmxMem,
                               void*        aWKT,
                               void*        aBuf,
                               void*        aFence,
                               IDE_RC*      aResult,
                               UInt         aValidateOption,
                               idBool       aSRIDOption,
                               SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE(stdParsing::skipSpace(&sPtr, sWKTFence) != IDE_SUCCESS,
                   err_parsing);
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_MULTIPOINT_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTIPOINT_NAME, 
                                            STD_MULTIPOINT_NAME_LEN ) != 0,
                       err_object_type );
    }
    else
    {
        // null object�� ���
    }
        
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION( err_parsing );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_INVALID_WKT ) );
    }
    IDE_EXCEPTION( err_object_type );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ) );
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� ��Ƽ���� ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::mlineFromText( iduMemory*   aQmxMem,
                              void*        aWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption,
                              idBool       aSRIDOption,
                              SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE(stdParsing::skipSpace(&sPtr, sWKTFence) != IDE_SUCCESS,
                   err_parsing);
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_MULTILINESTRING_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTILINESTRING_NAME,
                                            STD_MULTILINESTRING_NAME_LEN ) != 0,
                       err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� ��Ƽ������ ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::mpolyFromText( iduMemory*   aQmxMem,
                              void*        aWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption,
                              idBool       aSRIDOption,
                              SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE(stdParsing::skipSpace(&sPtr, sWKTFence) != IDE_SUCCESS,
                   err_parsing);
    
    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_MULTIPOLYGON_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTIPOLYGON_NAME, 
                                            STD_MULTIPOLYGON_NAME_LEN ) != 0,
                       err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * WKT�κ��� �ݷ��� ��ü�� �о� ���δ�.
 *
 * void*    aWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfWKT::geoCollFromText( iduMemory*   aQmxMem,
                                void*        aWKT,
                                void*        aBuf,
                                void*        aFence,
                                IDE_RC*      aResult,
                                UInt         aValidateOption,
                                idBool       aSRIDOption,
                                SInt         aSRID )
{
    UChar*  sWKT = ((mtdCharType*)aWKT)->value;
    UInt    sWKTLength = ((mtdCharType*)aWKT)->length;
   
    UChar*  sPtr = sWKT;
    UChar*  sWKTFence = sWKT + sWKTLength;
    
    IDE_TEST_RAISE(stdParsing::skipSpace(&sPtr, sWKTFence) != IDE_SUCCESS,
                   err_parsing);

    if ( sPtr != sWKTFence )
    {
        IDE_TEST_RAISE( sPtr + STD_GEOCOLLECTION_NAME_LEN >= sWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_GEOCOLLECTION_NAME,
                                            STD_GEOCOLLECTION_NAME_LEN ) != 0,
                       err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sWKT,
                                 sWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 aSRIDOption,
                                 aSRID );
    
    IDE_EXCEPTION(err_parsing);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_WKT));
    }
    IDE_EXCEPTION(err_object_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


