/***********************************************************************
 * Copyright 1999-2006, ALTIBase Corporation or its subsididiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id$
 *
 * Description:
 * EWKT(Extended Well Known Text)�κ��� Geometry ��ü �����ϴ� �Լ�
 * �� ������ stdParsing.cpp �� �ִ�.
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <ideMsgLog.h>
#include <mtdTypes.h>
#include <ste.h>
#include <stdTypes.h>
#include <stdParsing.h>
#include <stfEWKT.h>

// BUG-47966
extern mtdModule stdGeometry;

/***********************************************************************
 * Description: PROJ-2422 SRID
 * EWKT�κ��� Geometry ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::geomFromEWKT( iduMemory*   aQmxMem,
                              void*        aEWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption )
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
}

/***********************************************************************
 * Description: PROJ-2422 SRID
 * EWKT�κ��� ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::pointFromEWKT( iduMemory*   aQmxMem,
                               void*        aEWKT,
                               void*        aBuf,
                               void*        aFence,
                               IDE_RC*      aResult,
                               UInt         aValidateOption )               
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sEWKTFence )
    {
        if ( sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if (idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0)
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID( &sPtr, sEWKTFence ) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS );
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

        IDE_TEST_RAISE( sPtr + STD_POINT_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_POINT_NAME,
                                            STD_POINT_NAME_LEN ) != 0,
                        err_object_type);
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
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
 * Description: PROJ-2422 SRID
 * EWKT�κ��� ���� ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::lineFromEWKT( iduMemory*   aQmxMem,
                              void*        aEWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption )     
                  
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sEWKTFence )
    {
        if ( sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if ( idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0 )
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID(&sPtr, sEWKTFence) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace(&sPtr, sEWKTFence) != IDE_SUCCESS );
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

        IDE_TEST_RAISE( sPtr + STD_LINESTRING_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_LINESTRING_NAME, 
                                            STD_LINESTRING_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
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
 * Description: PROJ-2422 SRID
 * WKT��EWKT�κ��� ������ ��ü�� �о� ���δ�.
 * BUG-47966 WKT �Ǵ� EWKT�� ���� �Ľ��� �Ŀ� ��ü�� Ÿ���� Ȯ���ϵ��� ����
 *
 * void*    aEWKT(In): �о� ���� ����( WKT, EWKT ��� �� �� �ִ�.)
 * void*    aBuf(Out): ����� ���� (GEOMETRY)
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code - stdParsing::stdValue ���
 * SInt     aSRID(In): ������ SRID��
 **********************************************************************/
IDE_RC stfEWKT::polyFromEWKT( iduMemory*   aQmxMem,
                              void*        aEWKT,
                              void*        aBuf,
                              void*        aFence,
                              IDE_RC*      aResult,
                              UInt         aValidateOption,
                              SInt         aSRID )     
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;

    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;

    IDE_TEST( stdParsing::stdValue( aQmxMem,
                                    sEWKT,
                                    sEWKTLength,
                                    aBuf,
                                    aFence,
                                    aResult,
                                    aValidateOption,
                                    ID_TRUE,
                                    aSRID )
              != IDE_SUCCESS );

    if ( *aResult == IDE_SUCCESS )
    { 
        if ( ((stdGeometryHeader *)aBuf)->mType == STD_EMPTY_TYPE )
        {
            IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                            err_parsing );

            if ( sPtr + STD_SRID_NAME_LEN <= sEWKTFence )
            {
                if ( idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0 )
                {
                    sPtr += STD_SRID_NAME_LEN;
                    IDE_TEST( stdParsing::skipSRID( &sPtr, sEWKTFence ) != IDE_SUCCESS );
                    IDE_TEST( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS );
                }
            }

            if ( sPtr + STD_POLYGON_NAME_LEN <= sEWKTFence )
            {
                if ( idlOS::strncasecmp( (SChar*)sPtr,
                                         STD_POLYGON_NAME, 
                                         STD_POLYGON_NAME_LEN ) != 0 )
                {
                    // retrun NULL
                    stdGeometry.null(stdGeometry.column, aBuf);
                }
                else
                {
                    // polygon empty �� ��� 
                    // Nothing to do.
                }
            }
            else
            {
                // retrun NULL
                stdGeometry.null(stdGeometry.column, aBuf);
            }
        }
        else
        {
            if ( ( ((stdGeometryHeader *)aBuf)->mType != STD_POLYGON_2D_TYPE ) &&
                 ( ((stdGeometryHeader *)aBuf)->mType != STD_POLYGON_2D_EXT_TYPE ) )    
            {
                // retrun NULL
                stdGeometry.null(stdGeometry.column, aBuf);
            }
            else
            {
                // polygon_2d �Ǵ� polygon_2d_ext type �� ��� 
                // Nothing to do.
            }
        }
    }
    else
    {
        // �߻��� �� �ִ� ���� : retry_memory( ������ �޸� ũ�⸦ �Ѿ�� ���)
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_parsing );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_INVALID_WKT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: PROJ-2422 SRID
 * EWKT�κ��� ��Ƽ����Ʈ ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::mpointFromEWKT( iduMemory*   aQmxMem,
                                void*        aEWKT,
                                void*        aBuf,
                                void*        aFence,
                                IDE_RC*      aResult,
                                UInt         aValidateOption )     
                               
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sEWKTFence )
    {
        if ( sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if ( idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0 )
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID( &sPtr, sEWKTFence ) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS );
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

        IDE_TEST_RAISE( sPtr + STD_MULTIPOINT_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTIPOINT_NAME, 
                                            STD_MULTIPOINT_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
        
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
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
 * Description: PROJ-2422 SRID
 * EWKT�κ��� ��Ƽ���� ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::mlineFromEWKT( iduMemory*   aQmxMem,
                               void*        aEWKT,
                               void*        aBuf,
                               void*        aFence,
                               IDE_RC*      aResult,
                               UInt         aValidateOption )     
                              
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sEWKTFence )
    {
        if ( sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if ( idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0 )
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID(&sPtr, sEWKTFence) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace(&sPtr, sEWKTFence) != IDE_SUCCESS );
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

        IDE_TEST_RAISE( sPtr + STD_MULTILINESTRING_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTILINESTRING_NAME,
                                            STD_MULTILINESTRING_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
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
 * Description: PROJ-2422 SRID
 * EWKT�κ��� ��Ƽ������ ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::mpolyFromEWKT( iduMemory*   aQmxMem,
                               void*        aEWKT,
                               void*        aBuf,
                               void*        aFence,
                               IDE_RC*      aResult,
                               UInt         aValidateOption )     
                       
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );
    
    if ( sPtr != sEWKTFence )
    {
        if ( sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if ( idlOS::strncasecmp( (SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN ) == 0 )
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID( &sPtr, sEWKTFence ) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS );
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

        IDE_TEST_RAISE( sPtr + STD_MULTIPOLYGON_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_MULTIPOLYGON_NAME, 
                                            STD_MULTIPOLYGON_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
    IDE_EXCEPTION( err_parsing );
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_INVALID_WKT ) );
    }
    IDE_EXCEPTION( err_object_type);
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ) );
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description: PROJ-2422 SRID
 * EWKT�κ��� �ݷ��� ��ü�� �о� ���δ�.
 *
 * void*    aEWKT(In): �о� ���� ����
 * void*    aBuf(Out): ����� ����
 * void*    aFence(In): ����� ���� �潺
 * IDE_RC*  aResult(Out): Error code
 **********************************************************************/
IDE_RC stfEWKT::geoCollFromEWKT( iduMemory*   aQmxMem,
                                 void*        aEWKT,
                                 void*        aBuf,
                                 void*        aFence,
                                 IDE_RC*      aResult,
                                 UInt         aValidateOption )     
                  
{
    UChar*  sEWKT = ((mtdCharType*)aEWKT)->value;
    UInt    sEWKTLength = ((mtdCharType*)aEWKT)->length;
   
    UChar*  sPtr = sEWKT;
    UChar*  sEWKTFence = sEWKT + sEWKTLength;
    
    IDE_TEST_RAISE( stdParsing::skipSpace( &sPtr, sEWKTFence ) != IDE_SUCCESS,
                    err_parsing );

    if ( sPtr != sEWKTFence )
    {
        if (sPtr + STD_SRID_NAME_LEN < sEWKTFence )
        {
            if (idlOS::strncasecmp((SChar*)sPtr, STD_SRID_NAME, STD_SRID_NAME_LEN) == 0)
            {
                sPtr += STD_SRID_NAME_LEN;
                IDE_TEST( stdParsing::skipSRID(&sPtr, sEWKTFence) != IDE_SUCCESS );
                IDE_TEST( stdParsing::skipSpace(&sPtr, sEWKTFence) != IDE_SUCCESS );
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
        
        IDE_TEST_RAISE( sPtr + STD_GEOCOLLECTION_NAME_LEN >= sEWKTFence, err_object_type );
        IDE_TEST_RAISE( idlOS::strncasecmp( (SChar*)sPtr, STD_GEOCOLLECTION_NAME,
                                            STD_GEOCOLLECTION_NAME_LEN ) != 0,
                        err_object_type );
    }
    else
    {
        // null object�� ���
    }
   
    return stdParsing::stdValue( aQmxMem,
                                 sEWKT,
                                 sEWKTLength,
                                 aBuf,
                                 aFence,
                                 aResult,
                                 aValidateOption,
                                 ID_TRUE,  // enable srid
                                 ST_SRID_INIT );
    
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
