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
 * $Id: stfAnalysis.cpp 18883 2006-11-14 01:48:40Z sabbra $
 *
 * Description:
 * Geometry ��ü �м� �Լ� ����
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>

#include <mtc.h>
#include <mtd.h>
#include <mtdTypes.h>

#include <qc.h>

#include <stuProperty.h>
#include <ste.h>
#include <stdUtils.h>
#include <stcDef.h>
#include <stdTypes.h>
#include <stdPrimitive.h>
#include <stfRelation.h>
#include <stfAnalysis.h>
#include <stfBasic.h>

#include <stdPolyClip.h>

extern mtdModule stdGeometry;
extern mtdModule mtdDouble;

//#define STD_BUFFER_PRECISION        36 // BUG-16717
#define STD_BUFFER_PRECISION        12

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� �Ÿ� ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::distance(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* /* aTemplate */ )
{
    stdGeometryHeader*  sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*  sValue2 = (stdGeometryHeader *)aStack[2].value;

    SDouble          sDist;
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        // To Fix PR-15270
        // NULL ����� �����ؾ� ��.
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue1 ) != IDE_SUCCESS );
        IDE_TEST( stdUtils::checkValid( sValue2 ) != IDE_SUCCESS );

        IDE_TEST( getDistanceTwoObject( (stdGeometryType*)sValue1, 
                                        (stdGeometryType*)sValue2, 
                                        &sDist ) != IDE_SUCCESS );
        
        *(SDouble*) aStack[0].value = sDist;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� buffering ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::buffer(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sRet   = (stdGeometryHeader*)aStack[0].value;
    SDouble*                sDist  = (SDouble*)aStack[2].value;
    UInt                    aFence = aStack[0].column->precision;
    
    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;
    
    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue )==ID_TRUE) ||
        (mtdDouble.isNull( NULL, sDist )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576 
        IDE_TEST( stdUtils::checkValid( sValue ) != IDE_SUCCESS );

        // BUGBUG
        // To Fix BUG-16469
        /* BUG-33904 : ���� �Ÿ��� ������ ��쿡�� ������ �߻��ϵ��� �����մϴ�. */
        IDE_TEST_RAISE( *sDist < 0, ERR_INVALID_DISTANCE );
        
        stdGeometry.null(NULL, sRet);  // Fix BUG-15504

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
        
        IDE_TEST( getBuffer( sQmxMem, (stdGeometryType*)sValue, *sDist, sRet, aFence )
             != IDE_SUCCESS );
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DISTANCE );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_BUFFER_DISTANCE));
    }

    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� intersection ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::intersection(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    stdGeometryHeader*      sRet = (stdGeometryHeader*)aStack[0].value;
    UInt                    aFence = aStack[0].column->precision;     

    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue1 ) != IDE_SUCCESS );
        IDE_TEST( stdUtils::checkValid( sValue2 ) != IDE_SUCCESS );

        stdGeometry.null(NULL, sRet);  // Fix BUG-15504

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
        
        IDE_TEST( getIntersection( sQmxMem,
                                   (stdGeometryType*)sValue1, 
                                   (stdGeometryType*)sValue2, 
                                   sRet,
                                   aFence )
                  != IDE_SUCCESS );
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� difference ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::difference(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    stdGeometryHeader*      sRet = (stdGeometryHeader*)aStack[0].value;
    UInt                    aFence = aStack[0].column->precision; 
    
    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue1 ) != IDE_SUCCESS );
        IDE_TEST( stdUtils::checkValid( sValue2 ) != IDE_SUCCESS );

        stdGeometry.null(NULL, sRet);  // Fix BUG-15504

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
        
        IDE_TEST( getDifference( sQmxMem,
                                 (stdGeometryType*)sValue1, 
                                 (stdGeometryType*)sValue2, 
                                 sRet,
                                 aFence )
                  != IDE_SUCCESS );
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}




/***********************************************************************
 * Description:
 * �� Geometry ��ü�� union ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::unions(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    stdGeometryHeader*      sRet = (stdGeometryHeader*)aStack[0].value;
    UInt                    aFence = aStack[0].column->precision;   
 
    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue1 ) != IDE_SUCCESS );
        IDE_TEST( stdUtils::checkValid( sValue2 ) != IDE_SUCCESS );

        stdGeometry.null(NULL, sRet);  // Fix BUG-15504

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
        
        IDE_TEST( getUnion( sQmxMem,
                            (stdGeometryType*)sValue1, 
                            (stdGeometryType*)sValue2, 
                            sRet,
                            aFence )
                  != IDE_SUCCESS );
            
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� Symmetric Difference ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::symDifference(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    stdGeometryHeader*      sRet = (stdGeometryHeader*)aStack[0].value;
    UInt                    aFence = aStack[0].column->precision;
    
    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue1 ) != IDE_SUCCESS );
        IDE_TEST( stdUtils::checkValid( sValue2 ) != IDE_SUCCESS );

        stdGeometry.null(NULL, sRet);  // Fix BUG-15504
    
        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
    
        IDE_TEST( getSymDifference( sQmxMem,
                                    (stdGeometryType*)sValue1, 
                                    (stdGeometryType*)sValue2, 
                                    sRet,
                                    aFence )
                  != IDE_SUCCESS );
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� ConvexHull ��ü ����
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfAnalysis::convexHull(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sRet = (stdGeometryHeader*)aStack[0].value;
    UInt                    aFence = aStack[0].column->precision; 
    
    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( stdGeometry.isNull( NULL, sValue )==ID_TRUE )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        // BUG-33576
        IDE_TEST( stdUtils::checkValid( sValue ) != IDE_SUCCESS );

        stdGeometry.null(NULL, sRet);  // Fix BUG-15504
    
        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;
    
        IDE_TEST( getConvexHull( sQmxMem, (stdGeometryType*)sValue, sRet, aFence ) 
                    != IDE_SUCCESS );
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// distance
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� �Ÿ� ����
 *
 * stdGeometryType*    aObj1(In): ��ü1
 * stdGeometryType*    aObj2(In): ��ü2
 * SDouble*            aResult(Out):
 **********************************************************************/
IDE_RC stfAnalysis::getDistanceTwoObject(
                        stdGeometryType*    aObj1,
                        stdGeometryType*    aObj2,
                        SDouble*            aResult )
{
    SDouble             sDist, sMinDist;
    stdGeometryType*    sTraceObj1, *sTraceObj2;
    SInt                sNumObjects1, sNumObjects2;
    SInt                i, j;

    if ( (aObj1->header.mType == STD_EMPTY_TYPE) ||
         (aObj2->header.mType == STD_EMPTY_TYPE) )
    {
        // To Fix BUG-16440
        sDist = 0;
    }
    else
    {
        // BUG-16102
        if ( ( ( aObj1->header.mType != STD_GEOCOLLECTION_2D_EXT_TYPE ) &&
               ( aObj1->header.mType != STD_GEOCOLLECTION_2D_TYPE ) )
             &&
             ( ( aObj2->header.mType == STD_GEOCOLLECTION_2D_EXT_TYPE ) ||
               ( aObj2->header.mType == STD_GEOCOLLECTION_2D_TYPE ) ) )
        {
            // swap( sObj1, sObj2 );
            sTraceObj1 = aObj1;
            aObj1      = aObj2;
            aObj2      = sTraceObj1;
        }
        
        switch(aObj1->header.mType)
        {
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToSp2D( &aObj1->point2D,
                                                     &aObj2->point2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSpToSl2D( &aObj1->point2D,
                                                     &aObj2->linestring2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSpToSa2D( &aObj1->point2D,
                                                     &aObj2->polygon2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceSpToMp2D( &aObj1->point2D,
                                                     &aObj2->mpoint2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceSpToMl2D( &aObj1->point2D,
                                                     &aObj2->mlinestring2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceSpToMa2D( &aObj1->point2D,
                                                     &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToSl2D(
                            &aObj2->point2D, &aObj1->linestring2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSlToSl2D(
                            &aObj1->linestring2D, &aObj2->linestring2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSlToSa2D(
                            &aObj1->linestring2D, &aObj2->polygon2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceSlToMp2D(
                            &aObj1->linestring2D, &aObj2->mpoint2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceSlToMl2D(
                            &aObj1->linestring2D, &aObj2->mlinestring2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceSlToMa2D(
                            &aObj1->linestring2D, &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToSa2D(
                            &aObj2->point2D, &aObj1->polygon2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSlToSa2D(
                            &aObj2->linestring2D, &aObj1->polygon2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSaToSa2D(
                            &aObj1->polygon2D, &aObj2->polygon2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceSaToMp2D(
                            &aObj1->polygon2D, &aObj2->mpoint2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceSaToMl2D(
                            &aObj1->polygon2D, &aObj2->mlinestring2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceSaToMa2D(
                            &aObj1->polygon2D, &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToMp2D(
                            &aObj2->point2D, &aObj1->mpoint2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSlToMp2D(
                            &aObj2->linestring2D, &aObj1->mpoint2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSaToMp2D(
                            &aObj2->polygon2D, &aObj1->mpoint2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceMpToMp2D(
                            &aObj1->mpoint2D, &aObj2->mpoint2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceMpToMl2D(
                            &aObj1->mpoint2D, &aObj2->mlinestring2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceMpToMa2D(
                            &aObj1->mpoint2D, &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToMl2D(
                            &aObj2->point2D, &aObj1->mlinestring2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSlToMl2D(
                            &aObj2->linestring2D, &aObj1->mlinestring2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSaToMl2D(
                            &aObj2->polygon2D, &aObj1->mlinestring2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceMpToMl2D(
                            &aObj2->mpoint2D,&aObj1->mlinestring2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceMlToMl2D(
                            &aObj1->mlinestring2D, &aObj2->mlinestring2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceMlToMa2D(
                            &aObj1->mlinestring2D, &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                        sDist = getDistanceSpToMa2D(
                            &aObj2->point2D, &aObj1->mpolygon2D);
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                        sDist = getDistanceSlToMa2D(
                            &aObj2->linestring2D, &aObj1->mpolygon2D);
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                        sDist = getDistanceSaToMa2D(
                            &aObj2->polygon2D, &aObj1->mpolygon2D);
                        break;
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        sDist = getDistanceMpToMa2D(
                            &aObj2->mpoint2D, &aObj1->mpolygon2D);
                        break;
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        sDist = getDistanceMlToMa2D(
                            &aObj2->mlinestring2D, &aObj1->mpolygon2D);
                        break;
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        sDist = getDistanceMaToMa2D(
                            &aObj1->mpolygon2D, &aObj2->mpolygon2D);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            // BUG-16102
            case STD_GEOCOLLECTION_2D_EXT_TYPE:
            case STD_GEOCOLLECTION_2D_TYPE:
                sNumObjects1 = STD_N_GEOMS( (stdGeoCollection2DType*)aObj1 );
                sTraceObj1   = STD_FIRST_COLL2D( aObj1 );
                sMinDist     = -1;
                
                if ( ( aObj2->header.mType == STD_GEOCOLLECTION_2D_EXT_TYPE ) ||
                     ( aObj2->header.mType == STD_GEOCOLLECTION_2D_TYPE ) )
                {
                    sNumObjects2 = STD_N_GEOMS( (stdGeoCollection2DType*)aObj2 );
                    sMinDist     = -1;
                    for( i=0; i<sNumObjects1; i++ )
                    {
                        sTraceObj2   = STD_FIRST_COLL2D( aObj2 );
                        for( j=0; j<sNumObjects2; j++ )
                        {
                            IDE_TEST( getDistanceTwoObject( sTraceObj1, sTraceObj2, &sDist )
                                  != IDE_SUCCESS );
                            if( sMinDist<0 || sMinDist > sDist )
                            {
                                sMinDist = sDist;
                            }
                            sTraceObj2 = STD_NEXT_GEOM( sTraceObj2 );
                        }
                        
                        sTraceObj1 = STD_NEXT_GEOM( sTraceObj1 );
                    }
                }
                else
                {
                    for( i=0; i<sNumObjects1; i++ )
                    {
                        IDE_TEST( getDistanceTwoObject( sTraceObj1, aObj2, &sDist )
                                  != IDE_SUCCESS );
                        if( sMinDist<0 || sMinDist > sDist )
                        {
                            sMinDist = sDist;
                        }
                        sTraceObj1 = STD_NEXT_GEOM( sTraceObj1 );
                    }
                }
                sDist = sMinDist;
                break;
            // BUG-16102
            default:
                IDE_RAISE( err_incompatible_type );
        }
    }

    *aResult = sDist;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        // To fix BUG-15300 
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;
        
        stdUtils::getTypeText( aObj1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aObj2->header.mType, sType2, &sLen2 );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                "(\"%s\" : \"%s\")",
                sType1, sType2 );
                
        IDE_SET(ideSetErrorCode(stERR_ABORT_CONVERSION_MODULE_NOT_FOUND,
            sBuffer));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * ��1�� ��2 ������ �Ÿ� ����
 *
 * const stdPoint2D* aP1(In): ��1 
 * const stdPoint2D* aP2(In): ��2
 **********************************************************************/
SDouble stfAnalysis::primDistancePToP2D(
                                const stdPoint2D* aP1,
                                const stdPoint2D* aP2 )
{
    SDouble sDx = aP2->mX - aP1->mX;
    SDouble sDy = aP2->mY - aP1->mY;

    return idlOS::sqrt((sDx * sDx) + (sDy * sDy));
}

/***********************************************************************
 * Description:
 * �� aP�� ���� ������ �Ÿ� ����
 *
 * const stdPoint2D* aP(In): ��
 * const stdPoint2D* aQ1(In): ����
 * const stdPoint2D* aQ2(In): ����
 **********************************************************************/
SDouble stfAnalysis::primDistancePToL2D(
                                const stdPoint2D* aP,
                                const stdPoint2D* aQ1,
                                const stdPoint2D* aQ2 )
{
    SDouble segment_mag = (aQ2->mX - aQ1->mX)*(aQ2->mX - aQ1->mX) +
                        (aQ2->mY - aQ1->mY)*(aQ2->mY - aQ1->mY);

    if( segment_mag != 0 ) //Q1 != Q2;
    {
        SDouble u;
        SDouble xp;
        SDouble yp;
        u = ((aP->mX - aQ1->mX)*(aQ2->mX - aQ1->mX)+
            (aP->mY - aQ1->mY)*(aQ2->mY - aQ1->mY))/segment_mag ;

        // Fix BUG-15433
        if(u < 0)
        {
            return idlOS::sqrt((aP->mX - aQ1->mX)*(aP->mX - aQ1->mX)+
                (aP->mY - aQ1->mY)*(aP->mY - aQ1->mY));
        }
        else if(u > 1)
        {
            return idlOS::sqrt((aP->mX - aQ2->mX)*(aP->mX - aQ2->mX)+
                (aP->mY - aQ2->mY)*(aP->mY - aQ2->mY));
        }
        else
        {
            xp = aQ1->mX+ u * (aQ2->mX-aQ1->mX);
            yp = aQ1->mY+ u * (aQ2->mY-aQ1->mY);
            return idlOS::sqrt((xp - aP->mX)*(xp - aP->mX)+
                (yp - aP->mY)*(yp - aP->mY));
        }
    }
    else
    {    
        return idlOS::sqrt((aP->mX - aQ1->mX)*(aP->mX - aQ1->mX)+
            (aP->mY - aQ1->mY)*(aP->mY - aQ1->mY));
    }
}


/***********************************************************************
 * Description:
 * ����1�� ����2 ������ �Ÿ� ����
 *
 * const stdPoint2D* aP1(In): ����1
 * const stdPoint2D* aP2(In): ����1
 * const stdPoint2D* aQ1(In): ����2
 * const stdPoint2D* aQ2(In): ����2
 **********************************************************************/
SDouble stfAnalysis::primDistanceLToL2D(
                                const stdPoint2D* aP1,
                                const stdPoint2D* aP2,
                                const stdPoint2D* aQ1,
                                const stdPoint2D* aQ2 )
{
    SDouble sTemp;
    SDouble sDist = 0;

    if( stdUtils::intersect2D(aP1, aP2, aQ1, aQ2) == ID_FALSE )
    {
        sDist = primDistancePToL2D(aP1, aQ1, aQ2);
        sTemp = primDistancePToL2D(aP2, aQ1, aQ2);
        if(sTemp < sDist)
        {
            sDist = sTemp;
        }
        sTemp = primDistancePToL2D(aQ1, aP1, aP2);
        if(sTemp < sDist)
        {
            sDist = sTemp;
        }
        sTemp = primDistancePToL2D(aQ2, aP1, aP2);
        if(sTemp < sDist)
        {
            sDist = sTemp;
        }
    }
    else
    {
        /* �� ������ �����ϴ� ���, �� ���а� �Ÿ��� 0 */
    }

    return sDist;
}

/***********************************************************************
 * Description:
 * ���� �������� �Ÿ� ����
 *
 * const stdPoint2D*       aPt(In): ��
 * const stdLinearRing2D*  aRing(In): ��
 **********************************************************************/
SDouble stfAnalysis::primDistancePToR2D(
                                const stdPoint2D*       aPt,
                                const stdLinearRing2D*  aRing )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;
    
    stdPoint2D * sFirstPt = STD_FIRST_PT2D(aRing);
    
    for( i = 1; i < STD_N_POINTS(aRing); i++ )
    {
        sTemp = primDistancePToL2D(
            aPt, 
            STD_NEXTN_PT2D(sFirstPt,i-1), 
            STD_NEXTN_PT2D(sFirstPt,i));
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
    }
    return sDist;
}

/***********************************************************************
 * Description:
 * ���а� �������� �Ÿ� ����
 *
 * const stdPoint2D*       aP1(In): ����
 * const stdPoint2D*       aP2(In): ����
 * const stdLinearRing2D*  aRing(In): ��
 **********************************************************************/
SDouble stfAnalysis::primDistanceLToR2D(
                                const stdPoint2D*       aP1,
                                const stdPoint2D*       aP2,
                                const stdLinearRing2D*  aRing )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt i;
    
    stdPoint2D * sFirstPt = STD_FIRST_PT2D(aRing);
    
    for( i = 1; i < STD_N_POINTS(aRing); i++ )
    {
        sTemp = primDistanceLToL2D(
            aP1,
            aP2, 
            STD_NEXTN_PT2D(sFirstPt,i-1), 
            STD_NEXTN_PT2D(sFirstPt,i));
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
    }
    return sDist;
}

/***********************************************************************
 * Description:
 * ��1�� ��2������ �Ÿ� ����
 *
 * const stdLinearRing2D*  aRing1(In): ��1
 * const stdLinearRing2D*  aRing2(In): ��2
 **********************************************************************/
SDouble stfAnalysis::primDistanceRToR2D(
                                const stdLinearRing2D*  aRing1,
                                const stdLinearRing2D*  aRing2 )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt i;
    
    stdPoint2D * sFirstPt = STD_FIRST_PT2D(aRing1);
    
    for( i = 1; i < STD_N_POINTS(aRing1); i++ )
    {
        sTemp = primDistanceLToR2D(
            STD_NEXTN_PT2D(sFirstPt,i-1), 
            STD_NEXTN_PT2D(sFirstPt,i), 
            aRing2);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
    }
    return sDist;
}


// -----------------------------------------------------------------------------
// Point With Other ------------------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdPoint2DType*    aDstPt(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToSp2D(
                                const stdPoint2DType*    aPt,
                                const stdPoint2DType*    aDstPt )
{
    return primDistancePToP2D(&aPt->mPoint, &aDstPt->mPoint);
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdLineString2DType*   aDstLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToSl2D(
                                const stdPoint2DType*        aPt,
                                const stdLineString2DType*   aDstLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt i;
    
    stdPoint2D * sFirstPt = STD_FIRST_PT2D(aDstLine);
    
    for( i = 1; i < STD_N_POINTS(aDstLine); i++ )
    {
        sTemp = primDistancePToL2D(
            &aPt->mPoint, 
            STD_NEXTN_PT2D(sFirstPt,i-1), 
            STD_NEXTN_PT2D(sFirstPt,i));
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
    }
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdPolygon2DType*     aDstPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToSa2D(
                                const stdPoint2DType*       aPt,
                                const stdPolygon2DType*     aDstPoly )
{
    SDouble             sTemp;
    SDouble             sDist = 0;
    UInt                sInit = 1;
    stdLinearRing2D*    sDstRing;
    UInt                i;

    // Fix BUG-15433
    if( stfRelation::checkMatch('T', 
        stfRelation::spiTosai(&aPt->mPoint, aDstPoly)) == MTD_BOOLEAN_TRUE )
    {
        return 0.0;
    }

    sDstRing = STD_FIRST_RN2D(aDstPoly);
    
    for( i = 0; i < STD_N_RINGS(aDstPoly); i++ )
    {
        sTemp = primDistancePToR2D(&aPt->mPoint, sDstRing);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstRing = STD_NEXT_RN2D(sDstRing);
    }
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdMultiPoint2DType*   aDstMPt(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToMp2D(
                                const stdPoint2DType*        aPt,
                                const stdMultiPoint2DType*   aDstMPt )
{
    stdPoint2DType*     sDstPt;
    SDouble             sTemp;
    SDouble             sDist = 0;
    UInt                sInit = 1;
    UInt                i;
    
    sDstPt = STD_FIRST_POINT2D(aDstMPt);
    for( i = 0; i < STD_N_OBJECTS(aDstMPt); i++ )
    {
        sTemp = primDistancePToP2D(&aPt->mPoint, &sDstPt->mPoint);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPt = STD_NEXT_POINT2D(sDstPt);
    }
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdMultiLineString2DType* aDstMLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToMl2D(
                                const stdPoint2DType*           aPt,
                                const stdMultiLineString2DType* aDstMLine )
{
    stdLineString2DType*    sDstLine;
    SDouble                 sTemp;
    SDouble                 sDist = 0;
    UInt                    sInit = 1;
    UInt                    i;

    sDstLine = STD_FIRST_LINE2D(aDstMLine);
    for( i = 0; i < STD_N_OBJECTS(aDstMLine); i++ )
    {
        sTemp = getDistanceSpToSl2D(aPt, sDstLine);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstLine = STD_NEXT_LINE2D(sDstLine);
    }             
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPoint2DType*    aPt(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSpToMa2D(
                                const stdPoint2DType*           aPt,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    stdPolygon2DType*   sDstPoly;
    SDouble             sTemp;
    SDouble             sDist = 0;
    UInt                sInit = 1;
    UInt                i;

    sDstPoly = STD_FIRST_POLY2D(aDstMPoly);
    for( i = 0; i < STD_N_OBJECTS(aDstMPoly); i++ )
    {
        sTemp = getDistanceSpToSa2D(aPt, sDstPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPoly = STD_NEXT_POLY2D(sDstPoly);
    }
    return sDist;
}

// -----------------------------------------------------------------------------
// Line With Other -------------------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdLineString2DType*    aLine(In):
 * const stdLineString2DType*    aDstLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSlToSl2D(
                                const stdLineString2DType*    aLine,
                                const stdLineString2DType*    aDstLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i,j;

    stdPoint2D * sPt = STD_FIRST_PT2D(aLine);
    stdPoint2D * sDstPt = STD_FIRST_PT2D(aDstLine);
    
    for( i = 1; i < STD_N_POINTS(aLine); i++ )
    {
        for( j = 1; j < STD_N_POINTS(aDstLine); j++ )
        {
            sTemp = primDistanceLToL2D(
                STD_NEXTN_PT2D(sPt,i-1), 
                STD_NEXTN_PT2D(sPt,i), 
                STD_NEXTN_PT2D(sDstPt,j-1), 
                STD_NEXTN_PT2D(sDstPt,j));        
            if( (sInit == 1) || (sTemp < sDist) )
            {
                sInit = 0;
                sDist = sTemp;
            }
        }
    }
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdLineString2DType*      aLine(In):
 * const stdPolygon2DType*         aDstPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSlToSa2D(
                                const stdLineString2DType*      aLine,
                                const stdPolygon2DType*         aDstPoly )
{
    SDouble             sTemp;
    SDouble             sDist = 0;
    UInt                sInit = 1;
    stdLinearRing2D*    sDstRing;
    UInt                i,j;

    stdPoint2D * sPt = STD_FIRST_PT2D(aLine);    

    for( i = 1; i < STD_N_POINTS(aLine); i++ )
    {
        sDstRing = STD_FIRST_RN2D(aDstPoly);
        for( j = 0; j < STD_N_RINGS(aDstPoly); j++ )
        {
            sTemp = primDistanceLToR2D(
                STD_NEXTN_PT2D(sPt,i-1), 
                STD_NEXTN_PT2D(sPt,i), 
                sDstRing);
            if( (sInit == 1) || (sTemp < sDist) )
            {
                sInit = 0;
                sDist = sTemp;
            }
            sDstRing = STD_NEXT_RN2D(sDstRing);
        }
    }
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdLineString2DType*      aLine(In):
 * const stdMultiPoint2DType*      aDstMPt(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSlToMp2D(
                                const stdLineString2DType*      aLine,
                                const stdMultiPoint2DType*      aDstMPt )

{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPoint2DType * sDstPt = STD_FIRST_POINT2D(aDstMPt);
    for( i = 0; i < STD_N_OBJECTS(aDstMPt); i++ )
    {
        sTemp = getDistanceSpToSl2D(sDstPt, aLine);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPt = STD_NEXT_POINT2D(sDstPt);
    }             
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdLineString2DType*      aLine(In):
 * const stdMultiLineString2DType* aDstMLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSlToMl2D(
                                const stdLineString2DType*      aLine,
                                const stdMultiLineString2DType* aDstMLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdLineString2DType* sDstLine = STD_FIRST_LINE2D(aDstMLine);
    for( i = 0; i < STD_N_OBJECTS(aDstMLine); i++ )
    {
        sTemp = getDistanceSlToSl2D(aLine, sDstLine);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstLine = STD_NEXT_LINE2D(sDstLine);
    }             
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdLineString2DType*      aLine(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSlToMa2D(
                                const stdLineString2DType*      aLine,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPolygon2DType* sDstPoly = STD_FIRST_POLY2D(aDstMPoly);
    for( i = 0; i < STD_N_OBJECTS(aDstMPoly); i++ )
    {
        sTemp = getDistanceSlToSa2D(aLine, sDstPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPoly = STD_NEXT_POLY2D(sDstPoly);
    }             
    return sDist;
}

// -----------------------------------------------------------------------------
// Polygon With Other ----------------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPolygon2DType*     aPoly(In):
 * const stdPolygon2DType*     aDstPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSaToSa2D(
                                const stdPolygon2DType*     aPoly,
                                const stdPolygon2DType*     aDstPoly )
{
    SDouble             sTemp;
    SDouble             sDist = 0;
    UInt                sInit = 1;
    stdLinearRing2D*    sDstRing;
    UInt                i,j;
    
    stdLinearRing2D* sRing = STD_FIRST_RN2D(aPoly);
    for( i = 0; i < STD_N_RINGS(aPoly); i++ )
    {
        sDstRing = STD_FIRST_RN2D(aDstPoly);
        for( j = 0; j < STD_N_RINGS(aDstPoly); j++ )
        {
            sTemp = primDistanceRToR2D(sRing, sDstRing);
            if( (sInit == 1) || (sTemp < sDist) )
            {
                sInit = 0;
                sDist = sTemp;
            }
            sDstRing = STD_NEXT_RN2D(sDstRing);
        }        
        sRing = STD_NEXT_RN2D(sRing);
    }
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPolygon2DType*     aPoly(In):
 * const stdMultiPoint2DType*  aDstMPt(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSaToMp2D(
                                const stdPolygon2DType*     aPoly,
                                const stdMultiPoint2DType*  aDstMPt )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPoint2DType * sDstPt = STD_FIRST_POINT2D(aDstMPt);
    for( i = 0; i < STD_N_OBJECTS(aDstMPt); i++ )
    {
        sTemp = getDistanceSpToSa2D(sDstPt, aPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPt = STD_NEXT_POINT2D(sDstPt);
    }             
    return sDist;
}

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPolygon2DType*     aPoly(In):
 * const stdMultiLineString2DType* aDstMLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSaToMl2D(
                                const stdPolygon2DType*         aPoly,
                                const stdMultiLineString2DType* aDstMLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdLineString2DType* sDstLine = STD_FIRST_LINE2D(aDstMLine);
    for( i = 0; i < STD_N_OBJECTS(aDstMLine); i++ )
    {
        sTemp = getDistanceSlToSa2D(sDstLine, aPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstLine = STD_NEXT_LINE2D(sDstLine);
    }             
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdPolygon2DType*     aPoly(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceSaToMa2D(
                                const stdPolygon2DType*         aPoly,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPolygon2DType* sDstPoly = STD_FIRST_POLY2D(aDstMPoly);
    for( i = 0; i < STD_N_OBJECTS(aDstMPoly); i++ )
    {
        sTemp = getDistanceSaToSa2D(aPoly, sDstPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sDstPoly = STD_NEXT_POLY2D(sDstPoly);
    }             
    return sDist;
}

// -----------------------------------------------------------------------------
// Multi Point With Other ------------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiPoint2DType*      aMPt(In):
 * const stdMultiPoint2DType*      aDstMPt(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMpToMp2D(
                                const stdMultiPoint2DType*      aMPt,
                                const stdMultiPoint2DType*      aDstMPt )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPoint2DType * sPt = STD_FIRST_POINT2D(aMPt);
    for( i = 0; i < STD_N_OBJECTS(aMPt); i++ )
    {
        sTemp = getDistanceSpToMp2D(sPt, aDstMPt);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sPt = STD_NEXT_POINT2D(sPt);
    }             
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiPoint2DType*      aMPt(In):
 * const stdMultiLineString2DType* aDstMLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMpToMl2D(
                                const stdMultiPoint2DType*      aMPt,
                                const stdMultiLineString2DType* aDstMLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPoint2DType * sPt = STD_FIRST_POINT2D(aMPt);
    for( i = 0; i < STD_N_OBJECTS(aMPt); i++ )
    {
        sTemp = getDistanceSpToMl2D(sPt, aDstMLine);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sPt = STD_NEXT_POINT2D(sPt);
    }             
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiPoint2DType*      aMPt(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMpToMa2D(
                                const stdMultiPoint2DType*      aMPt,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPoint2DType * sPt = STD_FIRST_POINT2D(aMPt);
    for( i = 0; i < STD_N_OBJECTS(aMPt); i++ )
    {
        sTemp = getDistanceSpToMa2D(sPt, aDstMPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sPt = STD_NEXT_POINT2D(sPt);
    }             
    return sDist;
}


// -----------------------------------------------------------------------------
// Multi Line With Other -------------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiLineString2DType* aMLine(In):
 * const stdMultiLineString2DType* aDstMLine(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMlToMl2D(
                                const stdMultiLineString2DType* aMLine,
                                const stdMultiLineString2DType* aDstMLine )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdLineString2DType * sLine = STD_FIRST_LINE2D(aMLine);
    for( i = 0; i < STD_N_OBJECTS(aMLine); i++ )
    {
        sTemp = getDistanceSlToMl2D(sLine, aDstMLine);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }             
    return sDist;
}


/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiLineString2DType* aMLine(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMlToMa2D(
                                const stdMultiLineString2DType* aMLine,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdLineString2DType * sLine = STD_FIRST_LINE2D(aMLine);
    for( i = 0; i < STD_N_OBJECTS(aMLine); i++ )
    {
        sTemp = getDistanceSlToMa2D(sLine, aDstMPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return sDist;
}


// -----------------------------------------------------------------------------
// Multi Polygon With Other ----------------------------------------------------
// -----------------------------------------------------------------------------

/***********************************************************************
 * Description:
 * ��ü�� ��ü ������ �Ÿ�
 *
 * const stdMultiPolygon2DType*    aMPoly(In):
 * const stdMultiPolygon2DType*    aDstMPoly(In):
 **********************************************************************/
SDouble stfAnalysis::getDistanceMaToMa2D(
                                const stdMultiPolygon2DType*    aMPoly,
                                const stdMultiPolygon2DType*    aDstMPoly )
{
    SDouble sTemp;
    SDouble sDist = 0;
    UInt    sInit = 1;
    UInt    i;

    stdPolygon2DType * sPoly = STD_FIRST_POLY2D(aMPoly);
    for( i = 0; i < STD_N_OBJECTS(aMPoly); i++ )
    {
        sTemp = getDistanceSaToMa2D(sPoly, aDstMPoly);
        if( (sInit == 1) || (sTemp < sDist) )
        {
            sInit = 0;
            sDist = sTemp;
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    return sDist;
}


////////////////////////////////////////////////////////////////////////////////
// buffer
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� buffering ��ü ����
 *
 * stdGeometryType*    aObj(In): Buffering �� ��ü
 * SDouble             aDist(In): Buffering �� �Ÿ�
 * stdGeometryHeader*  aRet(Out): Buffering �� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getBuffer(
                    iduMemory*          aQmxMem,
                    stdGeometryType*    aObj,
                    SDouble             aDist,
                    stdGeometryHeader*  aRet,
                    UInt                aFence ) // Fix Bug - 25110
{
    /* BUG-33904 : ���� Tolerance�� 10�� ���� �� ��쿡�� �Է¹��� 
     * ������Ʈ���� �����Ͽ� �����մϴ�. */ 
    if ( aDist <= STU_CLIP_TOLERANCE * ST_BUFFER_MULTIPLIER )
    {
        switch(aObj->header.mType)
        {
            case STD_EMPTY_TYPE:
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj);
                break;
            default:
                IDE_RAISE( err_invalid_object_mType );
        }
    }
    else
    {
        switch(aObj->header.mType)
        {
        case STD_EMPTY_TYPE: // Fix BUG-16440
            stdUtils::makeEmpty(aRet);
            break;
        case STD_POINT_2D_EXT_TYPE:
        case STD_POINT_2D_TYPE:
            IDE_TEST( getPointBufferObject2D( &aObj->point2D.mPoint, aDist, 
                                              STD_BUFFER_PRECISION, aRet, aFence )
                     != IDE_SUCCESS );
            break;
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
            IDE_TEST( getLinestringBufferObject2D( aQmxMem, &aObj->linestring2D, aDist, 
                                                   STD_BUFFER_PRECISION, aRet, aFence )
                     != IDE_SUCCESS );
            break;
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
            IDE_TEST( getPolygonBufferObject2D( aQmxMem,
                                                &aObj->polygon2D,
                                                aDist, 
                                                STD_BUFFER_PRECISION,
                                                aRet,
                                                aFence )
                     != IDE_SUCCESS );
            break;
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
            IDE_TEST( getMultiPointBufferObject2D( aQmxMem, &aObj->mpoint2D, aDist, 
                                                   STD_BUFFER_PRECISION, aRet, aFence )
                     != IDE_SUCCESS );
            break;
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
            IDE_TEST( getMultiLinestringBufferObject2D( aQmxMem, &aObj->mlinestring2D, aDist, 
                                                        STD_BUFFER_PRECISION, aRet, aFence )
                     != IDE_SUCCESS );
            break;
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
            IDE_TEST( getMultiPolygonBufferObject2D( aQmxMem,
                                                     &aObj->mpolygon2D,
                                                     aDist, 
                                                     STD_BUFFER_PRECISION,
                                                     aRet,
                                                     aFence )
                     != IDE_SUCCESS );
            break;

        default:
            IDE_RAISE( err_invalid_object_mType );
        }

        /* PROJ-2422 srid */
        if ( stdUtils::isExtendedType( aObj->header.mType ) == ID_TRUE )
        {
            IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. */
        }
    }
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * stdPoint2D ��ü�� buffering ��ü ����
 *
 * const stdPoint2D*       aPt(In):         Buffering �� ��ü
 * const SDouble           aDistance(In):   Buffering �� �Ÿ�
 * const SInt              aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*      aRet(Out):       Buffering �� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getPointBufferObject2D( 
                    const stdPoint2D*       aPt,
                    const SDouble           aDistance,
                    const SInt              aPrecision,
                    stdGeometryHeader*      aRet,
                    UInt                    aFence ) //Fix Bug - 25110
{
    
    IDE_TEST( primPointBuffer2D(aPt, aDistance, aPrecision, aRet, aFence)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * stdLineString2DType ��ü�� buffering ��ü ����
 *
 * const stdLineString2DType*   aPt(In):         Buffering �� ��ü
 * const SDouble                aDistance(In):   Buffering �� �Ÿ�
 * const SInt                   aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*           aRet(Out):       Buffering �� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::getLinestringBufferObject2D(
    iduMemory*                  aQmxMem,
    const stdLineString2DType*  aLine,
    const SDouble               aDistance,
    const SInt                  aPrecision,
    stdGeometryHeader*          aRet,
    UInt                        aFence )  //Fix Bug - 25110
{
    stdGeometryHeader** sGeoms;
    
    stdPoint2D*         sPt;
    UInt                i, sIdx = 0, sMax;
    UInt                sCnt;
    UInt                sSize;
    
    sGeoms = NULL;
    
    // BUG-33123
    IDE_TEST_RAISE( STD_N_POINTS(aLine) == 0, ERR_INVALID_GEOMETRY );

    sCnt = (STD_N_POINTS(aLine))*2-1;  // x*2-1 = (x-1)*2+1
    
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*sCnt,
                                (void**) & sGeoms )
              != IDE_SUCCESS );
    
    sPt = STD_FIRST_PT2D(aLine);
    sMax = STD_N_POINTS(aLine)-1;
    for( i = 0; i < sMax; i++ ) 
    {
        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*(aPrecision+1);
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
        
        IDE_TEST( primPointBuffer2D( STD_NEXTN_PT2D(sPt,i), 
                                     aDistance,
                                     aPrecision,
                                     sGeoms[sIdx],
                                     sSize )
                  != IDE_SUCCESS );
        sIdx++;

        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
        
        IDE_TEST( primLineBuffer2D( STD_NEXTN_PT2D(sPt,i),
                                    STD_NEXTN_PT2D(sPt,i+1), 
                                    aDistance,
                                    aPrecision,
                                    sGeoms[sIdx],
                                    sSize )
                  != IDE_SUCCESS );
        sIdx++;
    }

    /* BUG-33904 : ���ν�Ʈ���� �������� �� ���� ���� ���, �� ���� ���� ���� �������� 
     * �������� �ʵ��� �����մϴ�. */
    if ( stdUtils::isSamePoints2D4Func( sPt, STD_NEXTN_PT2D(sPt,i) ) == ID_FALSE )
    {
        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*(aPrecision+1);
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );

        IDE_TEST( primPointBuffer2D( STD_NEXTN_PT2D(sPt,i), 
                                     aDistance,
                                     aPrecision,
                                     sGeoms[sIdx],
                                     sSize )
                  != IDE_SUCCESS );
        
        sIdx++;
    }
    else
    {
        sCnt--;
    }

    IDE_TEST( primUnionObjects2D( aQmxMem, sGeoms, sCnt, aRet, aFence )
              != IDE_SUCCESS );
    
    // BUG-16171    
    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * stdPolygon2DType ��ü�� buffering ��ü ����
 *
 * const stdPolygon2DType*   aPt(In):         Buffering �� ��ü
 * const SDouble             aDistance(In):   Buffering �� �Ÿ�
 * const SInt                aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*        aRet(Out):       Buffering �� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getPolygonBufferObject2D(
                    iduMemory*                  aQmxMem,
                    const stdPolygon2DType*     aPoly,
                    const SDouble               aDistance,
                    const SInt                  aPrecision,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )  //Fix Bug -25110
{
    stdLinearRing2D*        sRing1 = STD_FIRST_RN2D(aPoly);
    
    stdPolygon2DType*       sPolyRet;
    stdLinearRing2D*        sRingRet;
    UInt                    sRingCnt, sSize, sTotalSize;

    IDE_TEST( primRingBuffer2D( aQmxMem, aPoly, aDistance, aPrecision, aRet, aFence )
              != IDE_SUCCESS );

    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    sPolyRet = (stdPolygon2DType*)aRet;
    sPolyRet->mNumRings = sRingCnt = STD_N_RINGS(aPoly);
    sRingRet = STD_FIRST_RN2D(sPolyRet);
    
    sTotalSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + 
                 STD_PT2D_SIZE*STD_N_POINTS(sRingRet);
    
    if( sRingCnt > 1 )  // ���θ��� �����ϸ� ���θ� ����
    {
        sSize = STD_GEOM_SIZE(aPoly) - (STD_POLY2D_SIZE + STD_RN2D_SIZE + 
                                        STD_PT2D_SIZE*STD_N_POINTS(sRing1));
        sTotalSize += sSize;
        IDE_TEST_RAISE(sTotalSize > aFence, size_error);
        sRing1 = STD_NEXT_RN2D(sRing1);
        sRingRet = STD_NEXT_RN2D(sRingRet);

        idlOS::memcpy(sRingRet, sRing1, sSize);
    }
    
    sPolyRet->mSize = sTotalSize;

    // invalid ��ü�� ���� �� �ִ�.
    if ( stdPrimitive::validatePolygon2D( aQmxMem,
                                          sPolyRet,
                                          sTotalSize )
         == IDE_SUCCESS )
    {
        sPolyRet->mIsValid = ST_VALID;
    }
    else
    {
        sPolyRet->mIsValid = ST_INVALID;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * stdMultiPoint2DType ��ü�� buffering ��ü ����
 *
 * const stdMultiPoint2DType*   aPt(In):         Buffering �� ��ü
 * const SDouble                aDistance(In):   Buffering �� �Ÿ�
 * const SInt                   aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*           aRet(Out):       Buffering �� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getMultiPointBufferObject2D(
                    iduMemory*                  aQmxMem,
                    const stdMultiPoint2DType*  aMPoint,
                    const SDouble               aDistance,
                    const SInt                  aPrecision,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence) //Fix Bug - 25110
{
    stdGeometryHeader** sGeoms;
    
    stdPoint2DType*     sPoint;
    UInt                i, sIdx = 0;
    UInt                sMax = STD_N_OBJECTS(aMPoint);

    sGeoms = NULL;
    
    // BUG-33123
    IDE_TEST_RAISE( sMax == 0, ERR_INVALID_GEOMETRY );
    
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*sMax,
                                (void**) & sGeoms )
              != IDE_SUCCESS );
    
    sPoint = STD_FIRST_POINT2D(aMPoint);
    for( i = 0; i < sMax; i++ ) 
    {
        IDE_TEST( aQmxMem->alloc( STD_POLY2D_SIZE + STD_RN2D_SIZE
                                  + STD_PT2D_SIZE*(aPrecision+1),
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
        
        IDE_TEST( primPointBuffer2D( &sPoint->mPoint,
                                     aDistance,
                                     aPrecision,
                                     sGeoms[sIdx],
                                     aFence )
                  != IDE_SUCCESS );
        
        sIdx++;
        
        sPoint = STD_NEXT_POINT2D(sPoint);
    }

    IDE_TEST( primUnionObjects2D( aQmxMem, sGeoms, sMax, aRet, aFence )
              != IDE_SUCCESS );

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * stdMultiLineString2DType ��ü�� buffering ��ü ����
 *
 * const stdMultiLineString2DType*  aPt(In):         Buffering �� ��ü
 * const SDouble                    aDistance(In):   Buffering �� �Ÿ�
 * const SInt                       aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*               aRet(Out):       Buffering �� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::getMultiLinestringBufferObject2D(
    iduMemory*                      aQmxMem,
    const stdMultiLineString2DType* aMLine,
    const SDouble                   aDistance,
    const SInt                      aPrecision,
    stdGeometryHeader*              aRet,
    UInt                            aFence)  //Fix Bug - 25110
{
    stdGeometryHeader** sGeoms;
    
    stdLineString2DType*    sLine;
    stdPoint2D*             sPt;
    UInt                    i, j, sIdx = 0, sMax = 0, sMaxO;
    UInt                    sCnt = 0;
    UInt                    sSize;
    
    sLine = STD_FIRST_LINE2D(aMLine);
    sMaxO = STD_N_OBJECTS(aMLine);
    for( i = 0; i < sMaxO; i++ )
    {
        sCnt += (STD_N_POINTS(sLine))*2-1;   // x*2-1 = (x-1)*2+1
        sLine = STD_NEXT_LINE2D(sLine);
    }

    sGeoms = NULL;
    
    // BUG-33123
    IDE_TEST_RAISE( sCnt == 0, ERR_INVALID_GEOMETRY );
    
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*sCnt,
                                (void**) & sGeoms )
              != IDE_SUCCESS );
    
    sLine = STD_FIRST_LINE2D(aMLine);
    for( i = 0; i < sMaxO; i++ )
    {
        sPt = STD_FIRST_PT2D(sLine);
        sMax = STD_N_POINTS(sLine)-1;
        for( j = 0; j < sMax; j++ ) 
        {
            sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*(aPrecision+1);
            IDE_TEST( aQmxMem->alloc( sSize,
                                      (void**) & sGeoms[sIdx] )
                      != IDE_SUCCESS );
            stdUtils::nullify( sGeoms[sIdx] );
        
            IDE_TEST( primPointBuffer2D( STD_NEXTN_PT2D(sPt,j), 
                                         aDistance,
                                         aPrecision,
                                         sGeoms[sIdx],
                                         sSize )
                      != IDE_SUCCESS );
            sIdx++;

            sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
            IDE_TEST( aQmxMem->alloc( sSize,
                                      (void**) & sGeoms[sIdx] )
                      != IDE_SUCCESS );
            stdUtils::nullify( sGeoms[sIdx] );
            
            IDE_TEST( primLineBuffer2D( STD_NEXTN_PT2D(sPt,j),
                                        STD_NEXTN_PT2D(sPt,j+1), 
                                        aDistance,
                                        aPrecision,
                                        sGeoms[sIdx] ,
                                        sSize )
                      != IDE_SUCCESS );
            sIdx++;
        }

        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*(aPrecision+1);
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
            
        IDE_TEST( primPointBuffer2D( STD_NEXTN_PT2D(sPt,j), 
                                     aDistance,
                                     aPrecision,
                                     sGeoms[sIdx],
                                     sSize )
                  != IDE_SUCCESS );
        sIdx++;
        
        sLine = STD_NEXT_LINE2D(sLine);
    }

    IDE_TEST( primUnionObjects2D( aQmxMem, sGeoms, sCnt, aRet, aFence )
              != IDE_SUCCESS );
    
    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;   
}

/***********************************************************************
 * Description:
 * stdMultiPolygon2DType ��ü�� buffering ��ü ����
 *
 * const stdMultiPolygon2DType*   aPt(In):         Buffering �� ��ü
 * const SDouble                  aDistance(In):   Buffering �� �Ÿ�
 * const SInt                     aPrecision(In):  ���̳� ȣ�� ��Ȯ�� 
 * stdGeometryHeader*             aRet(Out):       Buffering �� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::getMultiPolygonBufferObject2D(
    iduMemory*                      aQmxMem,
    const stdMultiPolygon2DType*    aMPoly,
    const SDouble                   aDistance,
    const SInt                      aPrecision,
    stdGeometryHeader*              aRet,
    UInt                            aFence ) //Fix Bug - 25110
{
    stdGeometryHeader**     sGeoms;
    stdPolygon2DType*       sPoly;
    UInt                    i;
    UInt                    sMax;
    UInt                    sIdx = 0;
    
    sPoly = STD_FIRST_POLY2D(aMPoly);
    sMax = STD_N_OBJECTS(aMPoly);

    sGeoms = NULL;
    
    // BUG-33123
    IDE_TEST_RAISE( sMax == 0, ERR_INVALID_GEOMETRY );
    
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*sMax,
                                (void**) & sGeoms )
              != IDE_SUCCESS );
    
    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( getPolygonBufferObject2D( aQmxMem,
                                            sPoly,
                                            aDistance,
                                            aPrecision,
                                            aRet,
                                            aFence )
                  != IDE_SUCCESS );

        IDE_TEST( aQmxMem->alloc( STD_GEOM_SIZE( aRet ),
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        idlOS::memcpy( sGeoms[sIdx], aRet, STD_GEOM_SIZE( aRet ) );

        sIdx++;
        sPoly = STD_NEXT_POLY2D(sPoly);
    }

    IDE_TEST( primUnionObjects2D( aQmxMem, sGeoms, sMax, aRet, aFence )
              != IDE_SUCCESS );

    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;   
}


////////////////////////////////////////////////////////////////////////////////
// Intersection
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� intersection ��ü ����
 *
 * stdGeometryType*     aObj1(In): ��ü1
 * stdGeometryType*     aObj2(In): ��ü2
 * stdGeometryHeader*   aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersection(
                    iduMemory*                  aQmxMem,
                    stdGeometryType*            aObj1,
                    stdGeometryType*            aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) //Fix Bug - 25110
    
{
    if( (stdUtils::isEmpty( (stdGeometryHeader*) aObj1 ) == ID_TRUE) ||
        (stdUtils::isEmpty( (stdGeometryHeader*) aObj2 ) == ID_TRUE) )
    {
        // Fix BUG-16440
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        switch(aObj1->header.mType)
        {
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST(getIntersectionPointPoint2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST(getIntersectionPointLine2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS ) ;
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST(getIntersectionPointArea2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST (getIntersectionPointLine2D(aQmxMem, aObj2, aObj1, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST( getIntersectionLineLine2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST(getIntersectionLineArea2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST(getIntersectionPointArea2D(aQmxMem, aObj2, aObj1, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST(getIntersectionLineArea2D(aQmxMem, aObj2, aObj1, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST(getIntersectionAreaArea2D(aQmxMem,
                                                           aObj1,
                                                           aObj2,
                                                           aRet,
                                                           aFence)
                                 != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            default:
                IDE_RAISE( err_incompatible_type );
        }

        /* PROJ-2422 srid */
        /* ���ʿ� ���߰ų� �����ʿ� �����. */
        if ( stdUtils::isExtendedType( aObj1->header.mType ) == ID_TRUE )
        {
            IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj1 )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( stdUtils::isExtendedType( aObj2->header.mType ) == ID_TRUE )
            {
                IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj2 )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 0���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionPointPoint2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence)   //Fix Bug - 25110
{
    stdPoint2DType          *sPoint1, *sPoint2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;

    idBool                  sInserted;

    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POINT_2D_GROUP ,
        err_incompatible_type );

    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
        
        sPoint2 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoint2 == NULL, err_invalid_object_mType );
            
            if( stdUtils::isSamePoints2D(&sPoint1->mPoint, &sPoint2->mPoint)
                == ID_TRUE )
            {
                IDE_TEST( insertPointLink2D( aQmxMem,
                                             &sPtHeader,
                                             &sPoint1->mPoint,
                                             &sInserted )
                          != IDE_SUCCESS );
                if( sInserted == ID_TRUE )   
                {
                    sCnt++;
                }
            }
            
            sPoint2 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoint2);
        }

        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST(makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS );
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 1���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionPointLine2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )  //Fix Bug - 25110
{
    stdPoint2DType          *sPoint1;
    stdLineString2DType     *sLine2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;

    idBool                  sInserted;

    sPtHeader   = NULL;
    sCnt = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
        
        sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
            
            if( stfRelation::spiTosli(&sPoint1->mPoint, sLine2) == '0' ||
                stfRelation::spiToslb(&sPoint1->mPoint, sLine2) == '0' )
            {
                IDE_TEST( insertPointLink2D( aQmxMem,
                                             &sPtHeader,
                                             &sPoint1->mPoint,
                                             &sInserted )
                          != IDE_SUCCESS );
                if( sInserted == ID_TRUE )  
                {
                    sCnt++;
                }
            }
            sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sLine2);
        }        
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST(makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 2���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionPointArea2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence)  //Fix Bug - 25110
{
    stdPoint2DType          *sPoint1;
    stdPolygon2DType        *sPoly2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;

    idBool                  sInserted;

    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );
    
    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
        
        sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
            
            if( stfRelation::spiTosai(&sPoint1->mPoint, sPoly2) == '0' ||
                stfRelation::spiTosab(&sPoint1->mPoint, sPoly2) == '0' )
            {               
                IDE_TEST( insertPointLink2D( aQmxMem,
                                             &sPtHeader,
                                             &sPoint1->mPoint,
                                             &sInserted )
                          != IDE_SUCCESS );
                if ( sInserted == ID_TRUE )   
                {
                    sCnt++;
                }
            }
            sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoly2);
        }        
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS );
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 1���� ��ü�� 1���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::getIntersectionLineLine2D( iduMemory             * aQmxMem,
                                        const stdGeometryType * aObj1,
                                        const stdGeometryType * aObj2,
                                        stdGeometryHeader     * aRet,
                                        UInt                    aFence )  // Fix Bug - 25110
{
    stdLineString2DType     *sLine1;
    stdLineString2DType     *sLine2;
    stdPoint2D              *sPt1;
    stdPoint2D              *sPt2;
    UInt                    i, j, k, l, sPtCnt = 0, sLineCnt = 0;
    UInt                    sMax1, sMax2, sMaxPt1, sMaxPt2;

    pointLink2D*            sPtHeader = NULL;
    lineLink2D*             sLineHeader = NULL;
    stdPoint2D              *spRet1, *spRet2;

    idBool                  sInserted;
    
    spRet1 = NULL;
    spRet2 = NULL;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_LINE_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sLine1 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);    
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sLine1 == NULL, err_invalid_object_mType );
        
        sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
        
            sMaxPt1 = STD_N_POINTS(sLine1)-1;
            sMaxPt2 = STD_N_POINTS(sLine2)-1;
            sPt1 = STD_FIRST_PT2D(sLine1);
            for( k = 0; k < sMaxPt1; k++ )
            {
                sPt2 = STD_FIRST_PT2D(sLine2);
                for( l = 0; l < sMaxPt2; l++ )
                {
                    spRet1 = NULL;
                    spRet2 = NULL;

                    IDE_TEST( primIntersectionLineLine2D( aQmxMem,
                                                          sPt1,
                                                          STD_NEXT_PT2D(sPt1), 
                                                          sPt2,
                                                          STD_NEXT_PT2D(sPt2),
                                                          &spRet1,
                                                          &spRet2 )
                              != IDE_SUCCESS );

                    if( (spRet1 != NULL) && (spRet2 != NULL) )
                    {                        
                        IDE_TEST( insertLineLink2D( aQmxMem,
                                                    &sLineHeader,
                                                    spRet1,
                                                    spRet2 )
                                  != IDE_SUCCESS );
                        
                        spRet1 = NULL;
                        spRet2 = NULL;
                        
                        sLineCnt++;
                    }
                    else if(spRet1 != NULL)
                    {                        
                        IDE_TEST( insertPointLink2D( aQmxMem,
                                                     &sPtHeader,
                                                     spRet1,
                                                     &sInserted)
                                  != IDE_SUCCESS );
                        if( sInserted == ID_TRUE )
                        {
                            sPtCnt++;
                        }
                        spRet1 = NULL;
                    }

                    sPt2 = STD_NEXT_PT2D(sPt2);
                }
                sPt1 = STD_NEXT_PT2D(sPt1);
            }

            sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sLine2);
        }
        sLine1 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine1);
    }
    
    if( (sLineCnt > 0) && (sPtCnt > 0) )
    {
        IDE_TEST( makeGeomFromPointLineLink2D( aQmxMem,
                                               &sPtHeader,
                                               (SInt)sPtCnt,
                                               sLineHeader,
                                               aRet,
                                               aFence)
                  != IDE_SUCCESS );
    }
    else if(sPtCnt > 0)
    {
        IDE_TEST(makeMPointFromPointLink2D( sPtHeader, aRet, aFence) != IDE_SUCCESS);
    }
    else if(sLineCnt > 0)
    {
        IDE_TEST( makeMLineFromLineLink2D( aQmxMem, sLineHeader, aRet, aFence )
                  != IDE_SUCCESS );
    }

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 1���� ��ü�� 2���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionLineArea2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )   // Fix BUG - 25110
{
    stdLineString2DType * sLine;
    stdPolygon2DType    * sPoly;
    lineLink2D          * sLineHeader = NULL;
    pointLink2D         * sPtHeader   = NULL;
    UInt                  i;
    UInt                  j;
    UInt                  sMax1;
    UInt                  sMax2;
    UInt                  sPtCnt   = 0;
    UInt                  sLineCnt = 0;

    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_LINE_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sLine = (stdLineString2DType*)stdUtils::getFirstGeometry(
        (stdGeometryHeader*)aObj1);
    sPoly = (stdPolygon2DType*)stdUtils::getFirstGeometry(
        (stdGeometryHeader*)aObj2);

    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sLine == NULL, err_invalid_object_mType );

        sPoly = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoly == NULL, err_invalid_object_mType );

            IDE_TEST( stfAnalysis::primIntersectionLineArea2D( aQmxMem,
                                                               sLine,
                                                               sPoly,
                                                               &sPtHeader,
                                                               &sLineHeader,
                                                               &sPtCnt,
                                                               &sLineCnt,
                                                               aFence)
                      != IDE_SUCCESS);

            sPoly = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoly);
        }
        sLine = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine);
    }

    stdUtils::makeEmpty(aRet);

    if( (sLineCnt > 0) && (sPtCnt > 0) )
    {
        IDE_TEST( makeGeomFromPointLineLink2D(aQmxMem,
                                              &sPtHeader,
                                              (SInt)sPtCnt,
                                              sLineHeader,
                                              aRet,
                                              aFence)
                  != IDE_SUCCESS );
    }
    else if(sPtCnt > 0)
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader,
                                             aRet,
                                             aFence )
                  != IDE_SUCCESS);
    }
    else if(sLineCnt > 0)
    {
        IDE_TEST( makeMLineFromLineLink2D( aQmxMem,
                                           sLineHeader,
                                           aRet,
                                           aFence )
                  != IDE_SUCCESS );
    }

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionAreaArea2D( iduMemory*                  aQmxMem ,
                                               const stdGeometryType*      aObj1,
                                               const stdGeometryType*      aObj2,
                                               stdGeometryHeader*          aRet,
                                               UInt                        aFence )
{
    if (stuProperty::useGpcLib() == ID_TRUE )
    {
        IDE_TEST( getIntersectionAreaArea2D4Gpc( aQmxMem,
                                                 aObj1,
                                                 aObj2,
                                                 aRet,
                                                 aFence )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getIntersectionAreaArea2D4Clip( aQmxMem,
                                                  aObj1,
                                                  aObj2,
                                                  aRet,
                                                  aFence )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionAreaArea2D4Clip( iduMemory*                  aQmxMem ,
                                                    const stdGeometryType*      aObj1,
                                                    const stdGeometryType*      aObj2,
                                                    stdGeometryHeader*          aRet,
                                                    UInt                        aFence )
{
    // BUG-33436
    // ������, ��Ƽ�������� �������� �ʾƵ� �Ǳ� ������ 
    // ��ü�� �״�� �����Ѵ�.

    IDE_TEST(getPolygonClipAreaArea2D( aQmxMem,
                                       (stdGeometryHeader*)aObj1,
                                       (stdGeometryHeader*)aObj2,
                                       aRet,
                                       ST_CLIP_INTERSECT,
                                       aFence )
             != IDE_SUCCESS );    
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Intersection ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getIntersectionAreaArea2D4Gpc( iduMemory*                  aQmxMem ,
                                                   const stdGeometryType*      aObj1,
                                                   const stdGeometryType*      aObj2,
                                                   stdGeometryHeader*          aRet,
                                                   UInt                        aFence )
{
    stdPolygon2DType        *sPoly1;
    stdPolygon2DType        *sPoly2;
    objMainLink2D            sLnkHeader1, sLnkHeader2;
    stdGpcPolygon            sGpcPolyA, sGpcPolyB, sGpcPolyC;
    UInt                     i, sMax1, sMax2;

    sLnkHeader1.pItem = NULL;
    sLnkHeader1.pNext = NULL;
    sLnkHeader2.pItem = NULL;
    sLnkHeader2.pNext = NULL;

    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POLYGON_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );

        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader1, sPoly1 ) != IDE_SUCCESS );
        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }

    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader2, sPoly2 ) != IDE_SUCCESS );
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }
    
    initGpcPolygon( &sGpcPolyA );
    initGpcPolygon( &sGpcPolyB );
    initGpcPolygon( &sGpcPolyC );

    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyA, &sLnkHeader1 )
              != IDE_SUCCESS );
    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyB, &sLnkHeader2 )
              != IDE_SUCCESS );

    IDE_TEST( stdGpc::polygonClip( aQmxMem,
                                   STD_GPC_INT,
                                   &sGpcPolyA,
                                   &sGpcPolyB,
                                   &sGpcPolyC )
              != IDE_SUCCESS );
    
    IDE_TEST( getPolygonFromGpc2D( aQmxMem, (stdMultiPolygon2DType*)aRet, &sGpcPolyC, aFence ) != IDE_SUCCESS);

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// Difference
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� difference ��ü ����
 *
 * stdGeometryType*     aObj1(In): ��ü1
 * stdGeometryType*     aObj2(In): ��ü2
 * stdGeometryHeader*   aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifference(
                    iduMemory*                  aQmxMem,
                    stdGeometryType*            aObj1,
                    stdGeometryType*            aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )  // Fix Bug - 25110
{
    if ( aObj1->header.mType == STD_EMPTY_TYPE )
    {
        stdUtils::makeEmpty(aRet);
    }
    else if ( aObj2->header.mType == STD_EMPTY_TYPE )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj1);
    }
    else
    {
        switch(aObj1->header.mType)
        {
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST(getDifferencePointPoint2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST(getDifferenceLineLine2D(aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST(getDifferenceAreaArea2D(aQmxMem,
                                                         aObj1,
                                                         aObj2,
                                                         aRet,
                                                         aFence)
                                 != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;        
            default:
                IDE_RAISE( err_incompatible_type );
        }

        /* PROJ-2422 srid */
        /* ���ʿ� ���߰ų� �����ʿ� �����. */
        if ( stdUtils::isExtendedType(aObj1->header.mType) == ID_TRUE )
        {
            IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj1 )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( stdUtils::isExtendedType(aObj2->header.mType) == ID_TRUE )
            {
                IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj2 )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 0���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferencePointPoint2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )   // Fix Bug - 25110
{
    stdPoint2DType          *sPoint1, *sPoint2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;
    idBool                  sFound;

    idBool                  sInserted;
    
    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POINT_2D_GROUP ,
        err_incompatible_type );

    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
        
        sFound = ID_FALSE;
                                            
        sPoint2 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoint2 == NULL, err_invalid_object_mType );
            
            if( stdUtils::isSamePoints2D(&sPoint1->mPoint, &sPoint2->mPoint)
                == ID_TRUE )
            {
                sFound = ID_TRUE;
                break;
            }
            sPoint2 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoint2);
        }
        
        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D(aQmxMem,
                                        &sPtHeader,
                                        &sPoint1->mPoint,
                                        &sInserted )
                      != IDE_SUCCESS );
            if ( sInserted == ID_TRUE )
            {
                sCnt++;
            }
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS );
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 1���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferencePointLine2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) // Fix BUG - 25110
{
    stdPoint2DType          *sPoint1;
    stdLineString2DType     *sLine2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;
    idBool                  sFound;

    idBool                  sInserted;
    
    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );
    
    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
    
        sFound = ID_FALSE;

        sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
            
            if( stfRelation::spiTosli(&sPoint1->mPoint, sLine2) == '0' ||
                stfRelation::spiToslb(&sPoint1->mPoint, sLine2) == '0' )
            {
                sFound = ID_TRUE;
                break;
            }
            sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sLine2);
        }
        
        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D( aQmxMem,
                                         &sPtHeader,
                                         &sPoint1->mPoint,
                                         &sInserted )
                      != IDE_SUCCESS );
            if( sInserted == ID_TRUE )
            {
                sCnt++;
            }
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST(makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 2���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferencePointArea2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) // Fix BUG - 25110
{
    stdPoint2DType          *sPoint1;
    stdPolygon2DType        *sPoly2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;
    idBool                  sFound;

    idBool                  sInserted;
    
    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );
    
    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
    
        sFound = ID_FALSE;

        sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
    
            if( stfRelation::spiTosai(&sPoint1->mPoint, sPoly2) == '0' ||
                stfRelation::spiTosab(&sPoint1->mPoint, sPoly2) == '0' )
            {
                sFound = ID_TRUE;
                break;
            }
            sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoly2);
        }        

        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D( aQmxMem,
                                         &sPtHeader,
                                         &sPoint1->mPoint,
                                         &sInserted )
                      != IDE_SUCCESS );
            if ( sInserted == ID_TRUE )
            {
                sCnt++;
            }
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 1���� ��ü�� 1���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferenceLineLine2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )   // Fix BUG - 2511
{
    stdLineString2DType*    sLine1;
    stdLineString2DType*    sLine2;
    lineLink2D*               sLineHeader1;
    lineLink2D*               sLineHeader2;
    UInt                    i, j, sMax1, sMax2;

    sLineHeader1 = NULL;
    sLineHeader2 = NULL;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_LINE_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );
        
    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sLine1 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sLine1 == NULL, err_invalid_object_mType );
    
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine1, &sLineHeader1)
                  != IDE_SUCCESS );
        sLine1 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine1);
    }
        
    sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( j = 0; j < sMax2; j++ )
    {
        IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
    
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine2, &sLineHeader2)
                  != IDE_SUCCESS );
        sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine2);
    }
    
    IDE_TEST( makeDifferenceLineLink2D(aQmxMem, sLineHeader1, sLineHeader2)
              != IDE_SUCCESS );
    
    IDE_TEST( makeMLineFromLineLink2D( aQmxMem, sLineHeader1, aRet, aFence ) != IDE_SUCCESS );
    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferenceAreaArea2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) // Fix BUG - 25110
{
    if ( stuProperty::useGpcLib() == ID_TRUE )
    {
        IDE_TEST( getDifferenceAreaArea2D4Gpc( aQmxMem,
                                               aObj1,
                                               aObj2,
                                               aRet,
                                               aFence )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getDifferenceAreaArea2D4Clip( aQmxMem,
                                                aObj1,
                                                aObj2,
                                                aRet,
                                                aFence )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferenceAreaArea2D4Clip(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) // Fix BUG - 25110
{
    // BUG-33436
    // ������, ��Ƽ�������� �������� �ʾƵ� �Ǳ� ������ 
    // ��ü�� �״�� �����Ѵ�.   

    IDE_TEST(getPolygonClipAreaArea2D( aQmxMem,
                                       (stdGeometryHeader*)aObj1,
                                       (stdGeometryHeader*)aObj2,
                                       aRet,
                                       ST_CLIP_DIFFERENCE,
                                       aFence )
             != IDE_SUCCESS );    
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getDifferenceAreaArea2D4Gpc(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence ) // Fix BUG - 25110
{
    stdPolygon2DType        *sPoly1;
    stdPolygon2DType        *sPoly2;
    objMainLink2D             sLnkHeader1, sLnkHeader2;
    stdGpcPolygon             sGpcPolyA, sGpcPolyB, sGpcPolyC;
    UInt                    i, sMax1, sMax2;

    sLnkHeader1.pItem = NULL;
    sLnkHeader1.pNext = NULL;
    sLnkHeader2.pItem = NULL;
    sLnkHeader2.pNext = NULL;

    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POLYGON_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );
    
    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader1, sPoly1 ) != IDE_SUCCESS );
        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }
    
    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader2, sPoly2 ) != IDE_SUCCESS );
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }
    
    initGpcPolygon( &sGpcPolyA );
    initGpcPolygon( &sGpcPolyB );
    initGpcPolygon( &sGpcPolyC );

    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyA, &sLnkHeader1 )
              != IDE_SUCCESS );
    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyB, &sLnkHeader2 )
              != IDE_SUCCESS );

    IDE_TEST( stdGpc::polygonClip( aQmxMem,
                                   STD_GPC_DIFF,
                                   &sGpcPolyA,
                                   &sGpcPolyB,
                                   &sGpcPolyC )
              != IDE_SUCCESS );
    
    IDE_TEST( getPolygonFromGpc2D( aQmxMem, (stdMultiPolygon2DType*)aRet, &sGpcPolyC, aFence ) != IDE_SUCCESS);
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// Union
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� union ��ü ����
 *
 * stdGeometryType*     aObj1(In): ��ü1
 * stdGeometryType*     aObj2(In): ��ü2
 * stdGeometryHeader*   aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnion(
                    iduMemory*          aQmxMem,
                    stdGeometryType*    aObj1,
                    stdGeometryType*    aObj2,
                    stdGeometryHeader*  aRet,
                    UInt                aFence )
{
    // Fix BUG-16440
    if( stdUtils::isEmpty( (stdGeometryHeader*) aObj1 ) == ID_TRUE )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj2);
    }
    else if( stdUtils::isEmpty( (stdGeometryHeader*) aObj2 ) == ID_TRUE )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj1);
    }
    else
    {
        switch(aObj1->header.mType)
        {
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST_RAISE( getUnionPointPoint2D( aQmxMem,
                                                              aObj1,
                                                              aObj2,
                                                              aRet,
                                                              aFence ) != IDE_SUCCESS, err_pass );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST_RAISE( getUnionLineLine2D( aObj1,
                                                            aObj2,
                                                            aRet,
                                                            aFence ) != IDE_SUCCESS, err_pass );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST_RAISE( getUnionAreaArea2D( aQmxMem,
                                                            aObj1,
                                                            aObj2,
                                                            aRet,
                                                            aFence )
                                        != IDE_SUCCESS, err_pass );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            default:
                IDE_RAISE( err_incompatible_type );
        }

        /* PROJ-2422 srid */
        /* ���ʿ� ���߰ų� �����ʿ� �����. */
        if ( stdUtils::isExtendedType( aObj1->header.mType ) == ID_TRUE )
        {
            IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj1 )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( stdUtils::isExtendedType( aObj2->header.mType ) == ID_TRUE )
            {
                IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj2 )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_pass);
    {
        //error pass
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 0���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnionPointPoint2D(
                    iduMemory*              aQmxMem,
                    const stdGeometryType*  aObj1,
                    const stdGeometryType*  aObj2,
                    stdGeometryHeader*      aRet,
                    UInt                    aFence)  // Fix BUG - 25110
{
    stdPoint2DType          *sPoint1, *sPoint2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;

    idBool                  sInserted;

    sPtHeader   = NULL;
    sCnt        = 0;

    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POINT_2D_GROUP ,
        err_incompatible_type );

    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );

        IDE_TEST( insertPointLink2D( aQmxMem,
                                     &sPtHeader,
                                     &sPoint1->mPoint,
                                     &sInserted )
                  != IDE_SUCCESS );
        if ( sInserted == ID_TRUE )
        {
            sCnt++;
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }

    sPoint2 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( j = 0; j < sMax2; j++ )
    {
        IDE_TEST_RAISE( sPoint2 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertPointLink2D( aQmxMem,
                                     &sPtHeader,
                                     &sPoint2->mPoint,
                                     &sInserted )
                  != IDE_SUCCESS );
        if ( sInserted == ID_TRUE )
        {            
            sCnt++;
        }
        sPoint2 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint2);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
        
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 1���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnionPointLine2D(
                    iduMemory*              aQmxMem,
                    const stdGeometryType*  aObj1,
                    const stdGeometryType*  aObj2,
                    stdGeometryHeader*      aRet,
                    UInt                    aFence)  // Fix BUG - 25110
{
    stdPoint2DType          *sPoint1;
    stdLineString2DType     *sLine2;
    stdPoint2D              *sPt2;
    UInt                    sMax1, sMax2, sMaxPt2;
    UInt                    i, j, k, sPtCnt, sLineCnt;
    pointLink2D*            sPtHeader;
    lineLink2D*             sLineHeader;

    idBool                  sInserted;

    sPtHeader   = NULL;
    sLineHeader = NULL;
    sPtCnt      = 0;
    sLineCnt    = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );

    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
    
        IDE_TEST( insertPointLink2D( aQmxMem,
                                     &sPtHeader,
                                     &sPoint1->mPoint,
                                     &sInserted )
                  != IDE_SUCCESS );
        if ( sInserted == ID_TRUE )
        {    
            sPtCnt++;  
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }

    sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( j = 0; j < sMax2; j++ )
    {
        IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
    
        sMaxPt2 = STD_N_POINTS(sLine2)-1;
        sPt2 = STD_FIRST_PT2D(sLine2);
        for( k = 0; k < sMaxPt2; k++ )
        {
            IDE_TEST( insertLineLink2D( aQmxMem,
                                        &sLineHeader,
                                        sPt2,
                                        STD_NEXT_PT2D(sPt2))
                      != IDE_SUCCESS );
                
            sLineCnt++;
            sPt2 = STD_NEXT_PT2D(sPt2);
        }
        sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine2);
    }
    
    if( (sLineCnt > 0) && (sPtCnt > 0) )
    {
        IDE_TEST( makeGeomFromPointLineLink2D(aQmxMem, &sPtHeader, (SInt)sPtCnt, sLineHeader, aRet, aFence) != IDE_SUCCESS );
    }
    else if(sPtCnt > 0)
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
    }
    else if(sLineCnt > 0)
    {
        IDE_TEST( makeMLineFromLineLink2D( aQmxMem, sLineHeader, aRet, aFence )
                  != IDE_SUCCESS );

    }

    // Fix BUG-28695
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 2���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnionPointArea2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence)  // Fix BUG - 25110
{
    stdPoint2DType          *sPoint1;
    stdPolygon2DType        *sPoly2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;
    idBool                  sFound;

    idBool                  sInserted;
    
    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );
    
    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
    
        sFound = ID_FALSE;

        sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
    
            if( stfRelation::spiTosai(&sPoint1->mPoint, sPoly2) == '0' ||
                stfRelation::spiTosab(&sPoint1->mPoint, sPoly2) == '0' )
            {
                sFound = ID_TRUE;
                break;
            }
            sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoly2);
        }        

        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D(aQmxMem,
                                        &sPtHeader,
                                        &sPoint1->mPoint,
                                        &sInserted )
                      != IDE_SUCCESS );
            if( sInserted == ID_TRUE )
            {
                sCnt++;
            }
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*)aObj2);
    }
    else
    {
        stdGeoCollection2DType*     sColl = (stdGeoCollection2DType*)aRet;
        stdGeometryHeader*          sSubObj;
        
        
        stdUtils::makeEmpty(aRet);
        stdUtils::setType(aRet, STD_GEOCOLLECTION_2D_TYPE);
        sColl->mSize = STD_COLL2D_SIZE;
        sColl->mNumGeometries = 2;

        sSubObj = (stdGeometryHeader*)STD_FIRST_COLL2D(sColl);
        IDE_TEST(makeMPointFromPointLink2D( sPtHeader, sSubObj, aFence ) != IDE_SUCCESS);
        stdUtils::shiftMultiObjToSingleObj(sSubObj);
        sColl->mSize += STD_GEOM_SIZE(sSubObj);
        IDE_TEST( stdUtils::mergeMBRFromHeader(aRet,sSubObj) != IDE_SUCCESS );

        sSubObj = (stdGeometryHeader*)STD_NEXT_GEOM(sSubObj);
        stdUtils::copyGeometry(sSubObj, (stdGeometryHeader*)aObj2);
        sColl->mSize += STD_GEOM_SIZE(sSubObj);
        IDE_TEST( stdUtils::mergeMBRFromHeader(aRet,sSubObj) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 1���� ��ü�� 1���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnionLineLine2D(
                    const stdGeometryType*  aObj1,
                    const stdGeometryType*  aObj2,
                    stdGeometryHeader*      aRet,
                    UInt                    aFence )
{
    stdMultiLineString2DType*   sResMLine = (stdMultiLineString2DType*)aRet;
    stdLineString2DType*        sResLine  = STD_FIRST_LINE2D(sResMLine);
    
    const stdGeometryType*      sCurrObj;
    stdMultiLineString2DType*   sMLine;
    stdLineString2DType*        sLine;
    SInt                        i, j, sMax;
    
    stdUtils::makeEmpty(aRet);
    stdUtils::setType(aRet, STD_MULTILINESTRING_2D_TYPE);
    aRet->mSize = STD_MLINE2D_SIZE;
    sResMLine->mNumObjects = 0;
    
    for( i = 0; i < 2; i++ )
    {
        if( i == 0 )
        {
            sCurrObj = aObj1;
        }
        else if( i == 1 )
        {
            sCurrObj = aObj2;
        }        
        
        if(sCurrObj->header.mType == STD_LINESTRING_2D_TYPE)
        {
            sLine = (stdLineString2DType*)sCurrObj;

            IDE_TEST( stdUtils::mergeMBRFromHeader(aRet, (stdGeometryHeader*)sCurrObj)
                       != IDE_SUCCESS );

            sResMLine->mNumObjects++;
            aRet->mSize += STD_GEOM_SIZE(sLine);

            IDE_TEST_RAISE( aRet->mSize > aFence, err_object_buffer_overflow);

            idlOS::memcpy( sResLine, sLine, STD_GEOM_SIZE(sLine));
            
            sResLine = STD_NEXT_LINE2D(sResLine);
        }
        else if(sCurrObj->header.mType == STD_MULTILINESTRING_2D_TYPE)
        {
            sMLine = (stdMultiLineString2DType*)sCurrObj;
            sLine = STD_FIRST_LINE2D(sMLine);

            IDE_TEST( stdUtils::mergeMBRFromHeader(aRet, (stdGeometryHeader*)sCurrObj)
                       != IDE_SUCCESS );
            
            sMax = STD_N_OBJECTS(sMLine);
            for( j = 0; j < sMax; j++ )
            {
                if( (aRet->mSize + STD_GEOM_SIZE(sLine)) >= aFence )
                {
                    IDE_RAISE( err_object_buffer_overflow );
                }

                sResMLine->mNumObjects++;
                aRet->mSize += STD_GEOM_SIZE(sLine);                
                idlOS::memcpy( sResLine, sLine, STD_GEOM_SIZE(sLine));
                
                sLine = STD_NEXT_LINE2D(sLine);
                sResLine = STD_NEXT_LINE2D(sResLine);
            }
        }
        else
        {
            IDE_RAISE( err_incompatible_type );
        }
    }

    // Fix BUG-28695
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_object_buffer_overflow);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC
stfAnalysis::getUnionAreaArea2D( iduMemory*              aQmxMem,
                                 const stdGeometryType*  aObj1,
                                 const stdGeometryType*  aObj2,
                                 stdGeometryHeader*      aRet,
                                 UInt                    aFence )
{
    if ( stuProperty::useGpcLib() == ID_TRUE )
    {
        IDE_TEST( getUnionAreaArea2D4Gpc( aQmxMem,
                                          aObj1,
                                          aObj2,
                                          aRet,
                                          aFence )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getUnionAreaArea2D4Clip( aQmxMem,
                                           aObj1,
                                           aObj2,
                                           aRet,
                                           aFence )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC
stfAnalysis::getUnionAreaArea2D4Clip( iduMemory*              aQmxMem,
                                      const stdGeometryType*  aObj1,
                                      const stdGeometryType*  aObj2,
                                      stdGeometryHeader*      aRet,
                                      UInt                    aFence )
{
    // BUG-33436
    // ������, ��Ƽ�������� �������� �ʾƵ� �Ǳ� ������ 
    // ��ü�� �״�� �����Ѵ�.      

    IDE_TEST(getPolygonClipAreaArea2D( aQmxMem,
                                       (stdGeometryHeader*)aObj1,
                                       (stdGeometryHeader*)aObj2,
                                       aRet,
                                       ST_CLIP_UNION,
                                       aFence )
             != IDE_SUCCESS );    
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE; 
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC
stfAnalysis::getUnionAreaArea2D4Gpc( iduMemory*              aQmxMem,
                                     const stdGeometryType*  aObj1,
                                     const stdGeometryType*  aObj2,
                                     stdGeometryHeader*      aRet,
                                     UInt                    aFence )
{
    stdGeometryHeader** sGeoms;
    
    sGeoms = NULL;
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*2,
                                (void**) & sGeoms )
              != IDE_SUCCESS );

    sGeoms[0] = (stdGeometryHeader*)aObj1;
    sGeoms[1] = (stdGeometryHeader*)aObj2;

    IDE_TEST( primUnionObjectsPolygon2D( aQmxMem, sGeoms, 2, aRet, aFence)
              != IDE_SUCCESS );

    // Fix BUG-28695
    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE; 
}

                    
/***********************************************************************
 * Description:
 * 2D ��ǥ�迡 �ִ� �ΰ��� ���� �ٸ� ���� ��ü�� union ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getUnionCollection2D(
                    const stdGeometryType*  aObj1,
                    const stdGeometryType*  aObj2,
                    stdGeometryHeader*      aRet,
                    UInt                    aFence)  // Fix BUG - 25110
{
    stdGeoCollection2DType*     sColl = (stdGeoCollection2DType*)aRet;
    stdGeometryType*            sSubObj  = STD_FIRST_COLL2D(sColl);
     
    const stdGeometryType*      sCurrObj;
    stdGeometryHeader*          sCurrObjMirror;
    SInt                        i;
    
    stdUtils::makeEmpty(aRet);
    stdUtils::setType(aRet, STD_GEOCOLLECTION_2D_TYPE);
    aRet->mSize = STD_COLL2D_SIZE;
    sColl->mNumGeometries = 2;
    
    for( i = 0; i < 2; i++ )
    {
        if( i == 0 )
        {
            sCurrObj = aObj1;
        }
        else if( i == 1 )
        {
            sCurrObj = aObj2;
        }        

        if(sCurrObj->header.mType == STD_GEOCOLLECTION_2D_TYPE)
        {
            IDE_RAISE( err_incompatible_type );
        }
        
        sCurrObjMirror = (stdGeometryHeader*)sCurrObj;
        
        IDE_TEST( stdUtils::mergeMBRFromHeader(aRet, sCurrObjMirror) != IDE_SUCCESS );

        aRet->mSize += STD_GEOM_SIZE(sCurrObjMirror);

        IDE_TEST_RAISE(aRet->mSize > aFence, size_error);
        idlOS::memcpy( sSubObj, sCurrObjMirror, STD_GEOM_SIZE(sCurrObjMirror));
        
        sSubObj = STD_NEXT_GEOM(sSubObj);
        
    }
    // Fix BUG-28695
    stdUtils::shiftMultiObjToSingleObj(aRet);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// SymDifference
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� Symmetric Difference ��ü ����
 *
 * stdGeometryType*     aObj1(In): ��ü1
 * stdGeometryType*     aObj2(In): ��ü2
 * stdGeometryHeader*   aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifference(
                    iduMemory*                  aQmxMem,
                    stdGeometryType*            aObj1,
                    stdGeometryType*            aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )  // Fix BUG - 25110
{
    // Fix BUG-16440
    if ( stdUtils::isEmpty( (stdGeometryHeader*) aObj1 ) == ID_TRUE )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj2);
    }
    else if( stdUtils::isEmpty( (stdGeometryHeader*) aObj2 ) == ID_TRUE )
    {
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*) aObj2);
    }
    else
    {
        switch(aObj1->header.mType)
        {
            case STD_POINT_2D_EXT_TYPE:
            case STD_POINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTIPOINT_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POINT_2D_EXT_TYPE:
                    case STD_POINT_2D_TYPE:
                    case STD_MULTIPOINT_2D_EXT_TYPE:
                    case STD_MULTIPOINT_2D_TYPE:
                        IDE_TEST( getSymDifferencePointPoint2D( aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS);
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_LINESTRING_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_LINESTRING_2D_EXT_TYPE:
                    case STD_LINESTRING_2D_TYPE:
                    case STD_MULTILINESTRING_2D_EXT_TYPE:
                    case STD_MULTILINESTRING_2D_TYPE:
                        IDE_TEST( getSymDifferenceLineLine2D( aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            case STD_POLYGON_2D_EXT_TYPE:
            case STD_POLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
            case STD_MULTIPOLYGON_2D_TYPE:
                switch(aObj2->header.mType)
                {
                    case STD_POLYGON_2D_EXT_TYPE:
                    case STD_POLYGON_2D_TYPE:
                    case STD_MULTIPOLYGON_2D_EXT_TYPE:
                    case STD_MULTIPOLYGON_2D_TYPE:
                        IDE_TEST( getSymDifferenceAreaArea2D( aQmxMem, aObj1, aObj2, aRet, aFence ) != IDE_SUCCESS );
                        break;
                    default:
                        IDE_RAISE( err_incompatible_type );
                }
                break;
            default:
                IDE_RAISE( err_incompatible_type );
        }

        /* PROJ-2422 srid */
        /* ���ʿ� ���߰ų� �����ʿ� �����. */
        if ( stdUtils::isExtendedType( aObj1->header.mType ) == ID_TRUE )
        {
            IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj1 )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( stdUtils::isExtendedType( aObj2->header.mType ) == ID_TRUE )
            {
                IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj2 )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 0���� ��ü�� 0���� ��ü�� Symmetric Difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifferencePointPoint2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence )  //Fix BUG - 25110
{
    stdPoint2DType          *sPoint1, *sPoint2;
    UInt                    sMax1, sMax2;
    UInt                    i, j, sCnt;
    pointLink2D*            sPtHeader;
    idBool                  sFound;

    idBool                  sInserted;
    
    sPtHeader   = NULL;
    sCnt        = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POINT_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POINT_2D_GROUP ,
        err_incompatible_type );

    sMax1       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2       = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);
    
    sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );

        sFound = ID_FALSE;
                                            
        sPoint2 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            IDE_TEST_RAISE( sPoint2 == NULL, err_invalid_object_mType );
        
            if( stdUtils::isSamePoints2D(&sPoint1->mPoint, &sPoint2->mPoint)
                == ID_TRUE )
            {
                sFound = ID_TRUE;
                break;
            }
            sPoint2 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoint2);
        }
        
        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D( aQmxMem,
                                         &sPtHeader,
                                         &sPoint1->mPoint,
                                         &sInserted )
                      != IDE_SUCCESS );
            if ( sInserted == ID_TRUE )
            {              
                sCnt++;
            }
        }
        sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint1);
    }
    
    sPoint2 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ )
    {
        IDE_TEST_RAISE( sPoint2 == NULL, err_invalid_object_mType );

        sFound = ID_FALSE;
                                            
        sPoint1 = (stdPoint2DType*)stdUtils::getFirstGeometry(
                                            (stdGeometryHeader*)aObj1);
        for( j = 0; j < sMax1; j++ )
        {
            IDE_TEST_RAISE( sPoint1 == NULL, err_invalid_object_mType );
            
            if( stdUtils::isSamePoints2D(&sPoint1->mPoint, &sPoint2->mPoint)
                == ID_TRUE )
            {
                sFound = ID_TRUE;
                break;
            }
            sPoint1 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                                (stdGeometryHeader*)sPoint1);
        }
        
        if( sFound == ID_FALSE )
        {
            IDE_TEST( insertPointLink2D( aQmxMem,
                                         &sPtHeader,
                                         &sPoint2->mPoint,
                                         &sInserted )
                      != IDE_SUCCESS );
            if( sInserted == ID_TRUE )
            {
                sCnt++;
            }
        }
        sPoint2 = (stdPoint2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoint2);
    }
    
    if( sCnt == 0 )
    {
        stdUtils::makeEmpty(aRet);
    }
    else
    {
        IDE_TEST( makeMPointFromPointLink2D( sPtHeader, aRet, aFence ) != IDE_SUCCESS);
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 1���� ��ü�� 1���� ��ü�� Symmetric Difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifferenceLineLine2D(
                    iduMemory*                  aQmxMem,
                    const stdGeometryType*      aObj1,
                    const stdGeometryType*      aObj2,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence)  // Fix BUG - 25110
{
    stdLineString2DType*    sLine1;
    stdLineString2DType*    sLine2;
    lineLink2D*             sLineHeader1;
    lineLink2D*             sLineHeader2;
    lineLink2D*             sLineHeaderUnion;
    lineLink2D*             sLineHeaderInter;
    UInt                    i, j, sMax1, sMax2;

    sLineHeader1 = NULL;
    sLineHeader2 = NULL;
    sLineHeaderUnion = NULL;
    sLineHeaderInter = NULL;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_LINE_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_LINE_2D_GROUP ,
        err_incompatible_type );
        
    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sLine1 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        IDE_TEST_RAISE( sLine1 == NULL, err_invalid_object_mType );
        
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine1, &sLineHeader1)
                  != IDE_SUCCESS );
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine1, &sLineHeaderUnion)
                  != IDE_SUCCESS );
        
        sLine1 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine1);
    }
        
    sLine2 = (stdLineString2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( j = 0; j < sMax2; j++ )
    {
        IDE_TEST_RAISE( sLine2 == NULL, err_invalid_object_mType );
        
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine2, &sLineHeader2)
                  != IDE_SUCCESS );
        IDE_TEST( makeLineLinkFromLine2D(aQmxMem, sLine2, &sLineHeaderUnion)
                  != IDE_SUCCESS );

        sLine2 = (stdLineString2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sLine2);
    }
    
    IDE_TEST( makeIntersectionLineLink2D( aQmxMem,
                                          sLineHeader1,
                                          sLineHeader2,
                                          &sLineHeaderInter )
              != IDE_SUCCESS );
    
    IDE_TEST( makeDifferenceLineLink2D(aQmxMem, sLineHeaderUnion, sLineHeaderInter)
              != IDE_SUCCESS );
    
    IDE_TEST( makeMLineFromLineLink2D( aQmxMem, sLineHeaderUnion, aRet, aFence )
              != IDE_SUCCESS );
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Symmetric Difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifferenceAreaArea2D( iduMemory*                  aQmxMem,
                                                const stdGeometryType*      aObj1,
                                                const stdGeometryType*      aObj2,
                                                stdGeometryHeader*          aRet,
                                                UInt                        aFence ) //Fix BUG - 25110
{
    if ( stuProperty::useGpcLib() == ID_TRUE )
    {
        IDE_TEST( getSymDifferenceAreaArea2D4Gpc( aQmxMem,
                                                  aObj1,
                                                  aObj2,
                                                  aRet,
                                                  aFence )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getSymDifferenceAreaArea2D4Clip( aQmxMem,
                                                   aObj1,
                                                   aObj2,
                                                   aRet,
                                                   aFence )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Symmetric Difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifferenceAreaArea2D4Clip( iduMemory*                  aQmxMem,
                                                     const stdGeometryType*      aObj1,
                                                     const stdGeometryType*      aObj2,
                                                     stdGeometryHeader*          aRet,
                                                     UInt                        aFence ) //Fix BUG - 25110
{
    // BUG-33436
    // ������, ��Ƽ�������� �������� �ʾƵ� �Ǳ� ������ 
    // ��ü�� �״�� �����Ѵ�.      
    
    IDE_TEST(getPolygonClipAreaArea2D( aQmxMem,
                                       (stdGeometryHeader*)aObj1,
                                       (stdGeometryHeader*)aObj2,
                                       aRet,
                                       ST_CLIP_SYMDIFFERENCE,
                                       aFence )
             != IDE_SUCCESS );    
    
    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * 2D ���� ��ǥ�迡 �ִ� 2���� ��ü�� 2���� ��ü�� Symmetric Difference ��ü ����
 *
 * const stdGeometryType*   aObj1(In): ��ü1
 * const stdGeometryType*   aObj2(In): ��ü2
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getSymDifferenceAreaArea2D4Gpc( iduMemory*                  aQmxMem,
                                                    const stdGeometryType*      aObj1,
                                                    const stdGeometryType*      aObj2,
                                                    stdGeometryHeader*          aRet,
                                                    UInt                        aFence ) //Fix BUG - 25110
{
    stdPolygon2DType        *sPoly1;
    stdPolygon2DType        *sPoly2;
    objMainLink2D             sLnkHeader1, sLnkHeader2;
    stdGpcPolygon             sGpcPolyA, sGpcPolyB, sGpcPolyC;
    UInt                    i, sMax1, sMax2;

    sLnkHeader1.pItem = NULL;
    sLnkHeader1.pNext = NULL;
    sLnkHeader2.pItem = NULL;
    sLnkHeader2.pNext = NULL;

    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POLYGON_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );
    
    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);
    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader1, sPoly1 ) != IDE_SUCCESS );
        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }

    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );
        
        IDE_TEST( insertObjLink2D( aQmxMem, &sLnkHeader2, sPoly2 ) != IDE_SUCCESS );
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }
    
    initGpcPolygon( &sGpcPolyA );
    initGpcPolygon( &sGpcPolyB );
    initGpcPolygon( &sGpcPolyC );

    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyA, &sLnkHeader1 )
              != IDE_SUCCESS );
    IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, &sGpcPolyB, &sLnkHeader2 )
              != IDE_SUCCESS );

    IDE_TEST( stdGpc::polygonClip( aQmxMem,
                                   STD_GPC_XOR,
                                   &sGpcPolyA,
                                   &sGpcPolyB,
                                   &sGpcPolyC )
              != IDE_SUCCESS );
    
    IDE_TEST( getPolygonFromGpc2D( aQmxMem, (stdMultiPolygon2DType*)aRet, &sGpcPolyC, aFence ) != IDE_SUCCESS);

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// ConvexHull
////////////////////////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� ConvexHull ��ü ����
 *
 * stdGeometryHeader*  aObj(In): ��ü
 * stdGeometryHeader*  aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getConvexHull(
                    iduMemory*          aQmxMem,
                    stdGeometryType*    aObj,
                    stdGeometryHeader*  aRet,
                    UInt                aFence )  //Fix BUG - 25110
{
    switch(aObj->header.mType)
    {
    case STD_EMPTY_TYPE: // Fix BUG-16440
        stdUtils::makeEmpty(aRet);
        break;
    case STD_POLYGON_2D_EXT_TYPE:
    case STD_POLYGON_2D_TYPE:
        IDE_TEST ( getPolygonConvexHull2D(aQmxMem, &aObj->polygon2D, aRet, aFence) != IDE_SUCCESS );
        break;
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
    case STD_MULTIPOLYGON_2D_TYPE:
        IDE_TEST ( getMultiPolygonConvexHull2D(aQmxMem, &aObj->mpolygon2D, aRet, aFence) != IDE_SUCCESS );
        break;
    case STD_POINT_2D_EXT_TYPE:
    case STD_POINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
    case STD_MULTIPOINT_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
    case STD_LINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
    case STD_MULTILINESTRING_2D_TYPE:
        stdUtils::copyGeometry(aRet, (stdGeometryHeader*)aObj);
        break;
    default:
        IDE_RAISE( err_invalid_object_mType );
    }

    /* PROJ-2422 srid */
    if ( stdUtils::isExtendedType( aObj->header.mType ) == ID_TRUE )
    {
        IDE_TEST( stfBasic::setSRID( aRet, aFence, aObj )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * stdPolygon2DType ��ü�� ConvexHull ��ü ����
 *
 * const stdPolygon2DType*  aPoly(In): ��ü
 * stdGeometryHeader*       aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getPolygonConvexHull2D(
    iduMemory*                  aQmxMem,
    const stdPolygon2DType*     aPoly,
    stdGeometryHeader*          aRet,
    UInt                        aFence )  //Fix BUG - 25110
{
    stdPolygon2DType*       sPolyRet;
    stdLinearRing2D*        sRingRet;
    UInt                    sTotalSize;
    UInt                    sPtCnt;
    
    sTotalSize = STD_POLY2D_SIZE + STD_RN2D_SIZE +
                 STD_PT2D_SIZE * STD_N_POINTS(STD_FIRST_RN2D(aPoly));

    IDE_TEST_RAISE(sTotalSize > aFence, size_error);
    
    idlOS::memcpy(aRet, aPoly, sTotalSize);

    sPolyRet = (stdPolygon2DType*)aRet;
    sRingRet = STD_FIRST_RN2D(sPolyRet);

    sPolyRet->mNumRings = 1;  // Fix BUG - 28696 ConvexHull�� �ܺθ��� ���� �Ѵ�.

    sPtCnt = primMakeConvexHull2D(sRingRet) + 1; // Fix Bug - 28696 ���� ������ �մϴ�.

    sTotalSize = STD_POLY2D_SIZE + STD_RN2D_SIZE +
                 STD_PT2D_SIZE * sPtCnt;

    IDE_TEST_RAISE( sTotalSize > aFence, size_error);

    sPolyRet->mSize = sTotalSize;

    // invalid ��ü�� ���� �� �ִ�.
    if ( stdPrimitive::validatePolygon2D( aQmxMem,
                                          sPolyRet,
                                          sTotalSize )
         == IDE_SUCCESS )
    {
        sPolyRet->mIsValid = ST_VALID;
    }
    else
    {
        sPolyRet->mIsValid = ST_INVALID;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * stdMultiPolygon2DType ��ü�� ConvexHull ��ü ����
 *
 * const stdMultiPolygon2DType*     aMPoly(In): ��ü
 * stdGeometryHeader*               aRet(Out): ��� ��ü
 **********************************************************************/
IDE_RC stfAnalysis::getMultiPolygonConvexHull2D(
                    iduMemory*                      aQmxMem,
                    const stdMultiPolygon2DType*    aMPoly,
                    stdGeometryHeader*              aRet,
                    UInt                            aFence ) //Fix BUG - 25110
{
    stdPolygon2DType*       sPoly;
    
    stdMultiPolygon2DType*  sMPolyRet = (stdMultiPolygon2DType*)aRet;
    stdPolygon2DType*       sPolyRet;
    UInt                    i, sMax;
    UInt                    sTotalSize;
    
    sTotalSize = STD_MPOLY2D_SIZE;
    IDE_TEST_RAISE(sTotalSize > aFence, size_error);

    idlOS::memcpy(sMPolyRet, aMPoly, STD_MPOLY2D_SIZE);
    
    sPoly = STD_FIRST_POLY2D(aMPoly);
    sPolyRet = STD_FIRST_POLY2D(aRet);
    sMax = STD_N_OBJECTS(aMPoly);
    for( i = 0; i < sMax; i++)
    {
        IDE_TEST(getPolygonConvexHull2D( aQmxMem, sPoly, (stdGeometryHeader*)sPolyRet, aFence ) != IDE_SUCCESS);
        sTotalSize += STD_GEOM_SIZE(sPolyRet);
    
        sPoly = STD_NEXT_POLY2D(sPoly);
        sPolyRet = STD_NEXT_POLY2D(sPolyRet);
    }
    
    sMPolyRet->mSize = sTotalSize;

    // invalid ��ü�� ���� �� �ִ�.
    if ( stdPrimitive::validateMultiPolygon2D( aQmxMem,
                                               sMPolyRet,
                                               sTotalSize )
         == IDE_SUCCESS )
    {
        sMPolyRet->mIsValid = ST_VALID;
    }
    else
    {
        sMPolyRet->mIsValid = ST_INVALID;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

static SInt cmpl2D(const void *a, const void *b)
{
    SDouble     v;
    
    v = ((stdPoint2D*) a)->mX - ((stdPoint2D*)b)->mX;
    if(v > 0.)
    {
        return 1;
    }
    if(v < 0.)
    {
        return -1;
    }
    v = ((stdPoint2D*) b)->mY - ((stdPoint2D*)a)->mY;
    if(v > 0.)
    {
        return 1;
    }
    if(v < 0.)
    {
        return -1;
    }
    return 0;
}

static SInt cmph2D(const void *a, const void *b)
{
    return  cmpl2D(b, a);
}

static idBool is3PointCCW2D(stdPoint2D* aPt, UInt i, UInt j, UInt k)
{
    SDouble     a,b,c,d;
    
    a = aPt[i].mX - aPt[j].mX;
    b = aPt[i].mY - aPt[j].mY;
    c = aPt[k].mX - aPt[j].mX;
    d = aPt[k].mY - aPt[j].mY;
    return ((a*d - b*c <= 0) ? ID_TRUE : ID_FALSE);
}

UInt stfAnalysis::makeChain2D( 
                    stdPoint2D*                 aPt, 
                    UInt                        aMax, 
                    SInt (*cmpPoint) (const void*, const void*) )
{
    UInt        i, j, s = 1;
    stdPoint2D    t;

    idlOS::qsort( aPt, aMax, STD_PT2D_SIZE, cmpPoint );
    
    for( i = 2; i < aMax; i++ )
    {
        j = s;
        while((j >= 1) && (is3PointCCW2D(aPt, i, j, j-1) == ID_TRUE))
        {
            j--;
        }
        s = j + 1;
        t = aPt[s];
        aPt[s] = aPt[i];
        aPt[i] = t;
    }
    return s;
}

UInt stfAnalysis::primMakeConvexHull2D(
                    stdLinearRing2D*            aRingRet )
{
    stdPoint2D*     sPt = STD_FIRST_PT2D(aRingRet);
    UInt            sMax = STD_N_POINTS(aRingRet);
    UInt            sResCnt, u;

    u = makeChain2D( sPt, sMax, cmpl2D );
    sPt[sMax] = sPt[0];
    sResCnt = u + makeChain2D( sPt+u, sMax-u+1, cmph2D );
    
    aRingRet->mNumPoints = sResCnt+1;
    
    return sResCnt;
}

////////////////////////////////////////////////////////////////////////////////
// Primitive Analysis Functions ------------------------------------------------
////////////////////////////////////////////////////////////////////////////////

IDE_RC
stfAnalysis::primPointBuffer2D( const stdPoint2D*           aPt,
                                const SDouble               aDistance,
                                const SInt                  /*aPrecision*/,
                                stdGeometryHeader*          aResult,
                                UInt                        aFence)  //Fix BUG - 25110
{
    stdPolygon2DType*   sPoly = (stdPolygon2DType*)aResult;
    stdLinearRing2D*    sRing = STD_FIRST_RN2D(sPoly);
    stdPoint2D*         sPt   = STD_FIRST_PT2D(sRing);
    SInt                i     = 0;

    static SDouble      sSin[ STD_BUFFER_PRECISION ] = {
        (SDouble)0.0,                   // sin(0)
        (SDouble)0.5,                   // sin(30)
        (SDouble)0.86602540378443865,   // sin(60)
        (SDouble)1.0,                   // sin(90)
        (SDouble)0.86602540378443865,   // sin(120)
        (SDouble)0.5,                   // sin(150)
        (SDouble)0.0,                   // sin(180)
        (SDouble)-0.5,                  // sin(210)
        (SDouble)-0.86602540378443865,  // sin(240)
        (SDouble)-1,                    // sin(270)
        (SDouble)-0.86602540378443865,  // sin(300)
        (SDouble)-0.5                   // sin(330)
    };

    static SDouble      sCos[ STD_BUFFER_PRECISION ] = {
        (SDouble)1.0,                   // cos(0)
        (SDouble)0.86602540378443865,   // cos(30)
        (SDouble)0.5,                   // cos(60)
        (SDouble)0.0,                   // cos(90)
        (SDouble)-0.5,                  // cos(120)
        (SDouble)-0.86602540378443865,  // cos(150)
        (SDouble)-1.0,                  // cos(180)
        (SDouble)-0.86602540378443865,  // cos(210)
        (SDouble)-0.5,                  // cos(240)
        (SDouble)0,                     // cos(270)
        (SDouble)0.5,                   // cos(300)
        (SDouble)0.86602540378443865    // cos(330)
    };

    IDE_TEST_RAISE( STD_POLY2D_SIZE + STD_RN2D_SIZE +
                   STD_PT2D_SIZE * (STD_BUFFER_PRECISION + 1 ) >
                   aFence, size_error );

    // BUG-27941
    // sin, cos ���� ����� �����Ѵ�.
    sPt->mX = aPt->mX + aDistance * sCos[0];
    sPt->mY = aPt->mY + aDistance * sSin[0];

    aResult->mMbr.mMinX = aResult->mMbr.mMaxX =
        STD_NEXTN_PT2D(sPt,STD_BUFFER_PRECISION)->mX = sPt->mX;
    aResult->mMbr.mMinY = aResult->mMbr.mMaxY =
        STD_NEXTN_PT2D(sPt,STD_BUFFER_PRECISION)->mY = sPt->mY;

    for(i = 1; i < STD_BUFFER_PRECISION; i++ )
    {
        // BUG-27941
        // sin, cos ���� ����� �����Ѵ�.
        STD_NEXTN_PT2D(sPt,i)->mX = aPt->mX + aDistance * sCos[i];
        STD_NEXTN_PT2D(sPt,i)->mY = aPt->mY + aDistance * sSin[i];
        IDE_TEST( stdUtils::mergeMBRFromPoint2D(aResult, sPt+i)
                  != IDE_SUCCESS );
    }
    sRing->mNumPoints = STD_BUFFER_PRECISION+1;
    stdUtils::setType((stdGeometryHeader*)sPoly,STD_POLYGON_2D_TYPE);
    sPoly->mSize = STD_POLY2D_SIZE + STD_RN2D_SIZE +
                   STD_PT2D_SIZE*STD_N_POINTS(sRing);
    sPoly->mNumRings = 1;

    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::primLineBuffer2D( const stdPoint2D*           aPt1,
                               const stdPoint2D*           aPt2,
                               const SDouble               aDistance,
                               const SInt                  /* aPrecision */,
                               stdGeometryHeader*          aResult,
                               UInt                        aFence)  //Fix BUG - 25110
{
    stdPolygon2DType*   sPolyDst = (stdPolygon2DType*)aResult;
    stdLinearRing2D*    sRingDst = STD_FIRST_RN2D( sPolyDst );
    stdPoint2D*         sCurrPt  = STD_FIRST_PT2D( sRingDst );
    SDouble             sTemp;
    SDouble             sSqrtTemp;
    SDouble             sGradient;

    IDE_TEST_RAISE( STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5 > aFence , size_error );

    if ( stdUtils::isSamePoints2D4Func( aPt1, aPt2 ) == ID_TRUE )
    {
        // BUG-33902
        // same point�� ��� line�� �ƴϹǷ� ������ ��ȯ�Ѵ�.
        IDE_RAISE( ERR_SAME_POINT );
    }
    else if ( aPt1->mX == aPt2->mX )
    {
        sCurrPt->mX = aPt1->mX - aDistance;
        sCurrPt->mY = aPt1->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt1->mX + aDistance;
        sCurrPt->mY = aPt1->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt2->mX + aDistance;
        sCurrPt->mY = aPt2->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt2->mX - aDistance;
        sCurrPt->mY = aPt2->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mX;
        sCurrPt->mY = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );

        sRingDst->mNumPoints = 5;
        stdUtils::setType((stdGeometryHeader*)sPolyDst,STD_POLYGON_2D_TYPE );
        sPolyDst->mSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
        sPolyDst->mNumRings = 1;
    }
    else if ( aPt1->mY == aPt2->mY )
    {
        sCurrPt->mX = aPt1->mX;
        sCurrPt->mY = aPt1->mY - aDistance;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt1->mX;
        sCurrPt->mY = aPt1->mY + aDistance;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt2->mX;
        sCurrPt->mY = aPt2->mY + aDistance;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = aPt2->mX;
        sCurrPt->mY = aPt2->mY - aDistance;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mX;
        sCurrPt->mY = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );

        sRingDst->mNumPoints = 5;
        stdUtils::setType( (stdGeometryHeader*)sPolyDst,STD_POLYGON_2D_TYPE );
        sPolyDst->mSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
        sPolyDst->mNumRings = 1;
    }
    else
    {
        sTemp =
            (( aPt2->mX - aPt1->mX ) * ( aPt2->mX - aPt1->mX ) +
            ( aPt1->mY - aPt2->mY ) * ( aPt1->mY - aPt2->mY )) /
            ( aPt1->mY - aPt2->mY ) / ( aPt1->mY - aPt2->mY ) ;

        sSqrtTemp = idlOS::sqrt( sTemp );
        sGradient = ( aPt2->mX - aPt1->mX ) / ( aPt1->mY - aPt2->mY );

        sCurrPt->mX =
            ( sTemp * aPt1->mX - aDistance * sSqrtTemp ) / sTemp;
        sCurrPt->mY =
            ( sCurrPt->mX - aPt1->mX ) * sGradient + aPt1->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX =
            ( sTemp * aPt1->mX + aDistance * sSqrtTemp) / sTemp;
        sCurrPt->mY =
            ( sCurrPt->mX - aPt1->mX ) * sGradient + aPt1->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX =
            ( sTemp * aPt2->mX + aDistance * sSqrtTemp ) / sTemp;
        sCurrPt->mY =
            ( sCurrPt->mX - aPt2->mX ) * sGradient + aPt2->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX =
            ( sTemp * aPt2->mX - aDistance * sSqrtTemp ) / sTemp;
        sCurrPt->mY =
            ( sCurrPt->mX - aPt2->mX ) * sGradient + aPt2->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );
        sCurrPt = STD_NEXT_PT2D( sCurrPt );
        IDE_TEST_RAISE( sCurrPt == NULL, size_error );

        sCurrPt->mX = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mX;
        sCurrPt->mY = ((stdPoint2D*)STD_FIRST_PT2D( sRingDst ))->mY;
        IDE_TEST( stdUtils::setMBRFromPoint2D( &sPolyDst->mMbr, sCurrPt ) != IDE_SUCCESS );

        sRingDst->mNumPoints = 5;
        stdUtils::setType( (stdGeometryHeader*)sPolyDst,STD_POLYGON_2D_TYPE );
        sPolyDst->mSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
        sPolyDst->mNumRings = 1;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION( ERR_SAME_POINT );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_POINTS));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::primRingBuffer2D( iduMemory*                  aQmxMem,
                               const stdPolygon2DType*     aPoly,
                               const SDouble               aDistance,
                               const SInt                  aPrecision,
                               stdGeometryHeader*          aResult,
                               UInt                        aFence )  //Fix BUG - 25110
{
    stdGeometryHeader** sGeoms;
    
    stdLinearRing2D*    aRing = STD_FIRST_RN2D(aPoly);
    stdPoint2D*         sPt;
    UInt                i, sIdx = 0, sMax;
    UInt                sCnt;
    UInt                sSize;

    sGeoms = NULL;

    // BUG-33123
    IDE_TEST_RAISE( STD_N_POINTS(aRing) == 0, ERR_INVALID_GEOMETRY );

    sCnt = (STD_N_POINTS(aRing)-1)*2+1;
    
    IDE_TEST( aQmxMem->cralloc( STD_PGEOHEAD_SIZE*sCnt,
                                (void**) & sGeoms )
              != IDE_SUCCESS );

    sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*STD_N_POINTS(aRing);
    IDE_TEST( aQmxMem->alloc( sSize,
                              (void**) & sGeoms[sIdx] )
              != IDE_SUCCESS );
    
    IDE_TEST_RAISE(sSize > aFence, size_error);
    
    idlOS::memcpy(sGeoms[sIdx], aPoly, sSize );
    
    ((stdPolygon2DType*)sGeoms[sIdx])->mNumRings = 1;
    ((stdPolygon2DType*)sGeoms[sIdx])->mSize = sSize;
    sIdx++;
    
    sPt = STD_FIRST_PT2D(aRing);
    sMax = STD_N_POINTS(aRing)-1;
    for( i = 0; i < sMax; i++ ) 
    {
        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*(aPrecision+1);
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
        
        IDE_TEST( primPointBuffer2D( STD_NEXTN_PT2D(sPt,i), 
                                     aDistance,
                                     aPrecision,
                                     sGeoms[sIdx],
                                     sSize )
                  != IDE_SUCCESS );
        
        sIdx++;

        sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*5;
        IDE_TEST( aQmxMem->alloc( sSize,
                                  (void**) & sGeoms[sIdx] )
                  != IDE_SUCCESS );
        stdUtils::nullify( sGeoms[sIdx] );
        
        IDE_TEST( primLineBuffer2D( STD_NEXTN_PT2D(sPt,i),
                                    STD_NEXTN_PT2D(sPt,i+1), 
                                    aDistance,
                                    aPrecision,
                                    sGeoms[sIdx],
                                    sSize )
                  != IDE_SUCCESS );
        sIdx++;
    }

    IDE_TEST( primUnionObjects2D( aQmxMem, sGeoms, sCnt, aResult, aFence )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::primUnionObjects2D( iduMemory*          aQmxMem,
                                 stdGeometryHeader** aGeoms,
                                 SInt                aCnt,
                                 stdGeometryHeader*  aResult,
                                 UInt                aFence )  //Fix BUG - 25110
{
    stdGeometryHeader** sGeomsDst;
    stdGeometryHeader*  sSubGeom;
    SInt                sGroupType;
    SInt                sTotal, sCnt;
    SInt                i, j;

    sGeomsDst = NULL;
    
    sGroupType = stdUtils::getGroup( aGeoms[0] );
    if( sGroupType == STD_NULL_GROUP )
    {
        IDE_RAISE(STF_PRIM_UNION_2D);
    }
    
    for( i = 1; i < aCnt; i++ )
    {
        if( sGroupType != stdUtils::getGroup( aGeoms[i] ) )
        {
            IDE_RAISE(STF_PRIM_UNION_2D);
        }
    }
    
    if( sGroupType == STD_POLYGON_2D_GROUP )
    {
        if ( stuProperty::useGpcLib() == ID_TRUE )
        {
            IDE_TEST( primUnionObjectsPolygon2D( aQmxMem, aGeoms, aCnt, aResult, aFence )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( stdPolyClip::unionPolygons2D( aQmxMem,
                                                    aGeoms,
                                                    aCnt,
                                                    aResult,
                                                    aFence )
                      != IDE_SUCCESS );
        }
        IDE_RAISE(STF_PRIM_UNION_2D);
    }
    else if( (sGroupType == STD_LINE_2D_GROUP) ||
             (sGroupType == STD_POINT_2D_GROUP) ) 
    {
        sTotal = 0;
        for( i = 0; i < aCnt; i++ ) 
        {
            sTotal += stdUtils::getGeometryNum( aGeoms[i] );
        }

        // BUG-33123
        IDE_TEST_RAISE( sTotal == 0, ERR_INVALID_GEOMETRY );

        IDE_TEST( aQmxMem->alloc( STD_PGEOHEAD_SIZE*sTotal,
                                  (void**) & sGeomsDst )
                  != IDE_SUCCESS );

        sTotal = 0;
        for( i = 0; i < aCnt; i++ ) 
        {
            sCnt = stdUtils::getGeometryNum( aGeoms[i] );
            sSubGeom = stdUtils::getFirstGeometry( aGeoms[i] );
            for( j = 0; j < sCnt; j++ )
            {
                sGeomsDst[sTotal++] = sSubGeom;
                sSubGeom = stdUtils::getNextGeometry( sSubGeom );
            }
        }

        IDE_TEST( createMultiObject2D( aQmxMem, sGeomsDst, sTotal, aResult, aFence )
                  != IDE_SUCCESS );
        
        sGeomsDst = NULL;
    }

    IDE_EXCEPTION_CONT(STF_PRIM_UNION_2D);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 * ������ ��ü��(stdPolygon2DType, stdMultiPolygon2DType)�� Union �����Ѵ�.
 *
 * stdGeometryHeader** aGeoms(In): ������ ��ü���� ������ �迭
 * SInt                aCnt(In): ������ ��ü ����
 * stdGeometryHeader*  aResult(Out): ��� ��ü
 **********************************************************************/
IDE_RC
stfAnalysis::primUnionObjectsPolygon2D( iduMemory*          aQmxMem,
                                        stdGeometryHeader** aGeoms,
                                        SInt                aCnt,
                                        stdGeometryHeader*  aResult,
                                        UInt                aFence)  //Fix BUG - 25110
{
    stdGeometryHeader*      sSubGeom;
    SInt                    sCnt;
    SInt                    i, j;
   
    objMainLink2D             sLnkHeader;
    objMainLink2D*            sLnkMain;

    stdMultiPolygon2DType   * sMPolyA;
    stdGpcPolygon             sGpcPolyA, sGpcPolyB, sGpcPolyC;
    stdGpcPolygon             *sPtrA, *sPtrB, *sPtrC, *sPtrT;
    
    sLnkHeader.pItem = NULL;
    sLnkHeader.pNext = NULL;
    
    sMPolyA = NULL;

    // ��� �������� �м��Ͽ� �������� �ʴ� �������� �׷��� ���� ��ũ�� ��
    for( i = 0; i < aCnt; i++ )
    {
        sCnt = stdUtils::getGeometryNum( aGeoms[i] );
        sSubGeom = stdUtils::getFirstGeometry( aGeoms[i] );

        IDE_TEST_RAISE(sSubGeom == NULL, ERR_INVALID_TYPE);

        for( j = 0; j < sCnt; j++ ) 
        {
            IDE_TEST( insertObjLink2D( aQmxMem,
                                       &sLnkHeader,
                                       (stdPolygon2DType*)sSubGeom )
                      != IDE_SUCCESS );
            sSubGeom = stdUtils::getNextGeometry( sSubGeom );
        }
    }
    
    sCnt = 0;
    sLnkMain = sLnkHeader.pNext;    
    if( sLnkMain != NULL )
    {
        sPtrA = &sGpcPolyA;
        sPtrB = &sGpcPolyB;
        sPtrC = &sGpcPolyC;
        
        initGpcPolygon( &sGpcPolyA );
        initGpcPolygon( &sGpcPolyB );
        initGpcPolygon( &sGpcPolyC );
    
        IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, sPtrA, &sLnkHeader )
                  != IDE_SUCCESS );
        
        while( sLnkMain != NULL )
        {
            IDE_TEST( setGpcPolygonObjLnk2D( aQmxMem, sPtrB, sLnkMain )
                      != IDE_SUCCESS );
            
            IDE_TEST( stdGpc::polygonClip( aQmxMem,
                                           STD_GPC_UNION,
                                           sPtrA,
                                           sPtrB,
                                           sPtrC )
                      != IDE_SUCCESS );
            
            initGpcPolygon( sPtrA );
            initGpcPolygon( sPtrB );
            
            sPtrT = sPtrA; sPtrA = sPtrC; sPtrC = sPtrT;
            sLnkMain = sLnkMain->pNext;
            sCnt++;
        }
        
        IDE_TEST(getPolygonFromGpc2D( aQmxMem, (stdMultiPolygon2DType*)aResult, sPtrA , aFence ) != IDE_SUCCESS);
    } 
    else
    {
        IDE_TEST( allocMPolyFromObjLink2D(aQmxMem, &sMPolyA, &sLnkHeader, aFence )
                  != IDE_SUCCESS );
        
        idlOS::memcpy( aResult, sMPolyA, STD_GEOM_SIZE(sMPolyA) );

        sMPolyA = NULL;
    }
 
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_TYPE );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_NOT_SUPPORTED_OBJECT_TYPE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::createMultiObject2D( iduMemory*          aQmxMem,
                                  stdGeometryHeader** aGeoms,
                                  SInt                aCnt,
                                  stdGeometryHeader*  aResult,
                                  UInt                aFence )  //Fix BUG - 25110
{
    stdGeometryHeader** sGeomsDst;
    stdGeometryHeader*  sSubGeom;
    stdGeometryType*    sObj;
    SInt                i, j;
    SInt                sTotal, sCnt;
    SInt                sType;
    UInt                sSize;

    sGeomsDst = NULL;
    sTotal = 0;
    for( i = 0; i < aCnt; i++ )
    {
        if( aGeoms[i]==NULL )
        {
            continue;
        }
        sTotal += stdUtils::getGeometryNum( aGeoms[i] );
    }

    IDE_TEST_RAISE( (sTotal < 1) || (aCnt < 1), err_invalid_geometry); 

    IDE_TEST( aQmxMem->alloc( STD_PGEOHEAD_SIZE*sTotal,
                              (void**) & sGeomsDst )
              != IDE_SUCCESS );

    if( sTotal != aCnt )
    {
        sTotal = 0;
        for( i = 0; i < aCnt; i++ )
        {
            if( aGeoms[i] == NULL )
            {
                continue;
            }
                
            sCnt = stdUtils::getGeometryNum( aGeoms[i] );
            sSubGeom = stdUtils::getFirstGeometry( aGeoms[i] );
            
            for( j = 0; j < sCnt; j++ ) 
            {
                sGeomsDst[sTotal++] = sSubGeom;
                sSubGeom = stdUtils::getNextGeometry( sSubGeom );
            }    
        }
    }
    else
    {        
        for( i = 0; i < aCnt; i++ ) 
        {
            sGeomsDst[i] = aGeoms[i];
        }
    }
    
    IDE_TEST_RAISE( sGeomsDst == NULL, err_invalid_geometry );
    
    IDE_TEST_RAISE( sGeomsDst[0] == NULL, err_invalid_geometry );

    sType = sGeomsDst[0]->mType;
    sSize = STD_GEOHEAD_SIZE;

    for( i = 0; i < sTotal; i++ ) 
    {
        sSize += STD_GEOM_SIZE(sGeomsDst[i]);

        if( sType != sGeomsDst[i]->mType )
        {
            sType = STD_GEOCOLLECTION_2D_TYPE;
        }
    }
    IDE_TEST_RAISE(sSize > aFence, size_error);

    sObj = (stdGeometryType*)aResult;
    switch( sType ) 
    {
    case STD_POINT_2D_EXT_TYPE :
    case STD_POINT_2D_TYPE :
        stdUtils::setType((stdGeometryHeader*)aResult,
                            STD_MULTIPOINT_2D_TYPE);
        sObj->mpoint2D.mNumObjects = sTotal;
        break;
    case STD_LINESTRING_2D_EXT_TYPE:
    case STD_LINESTRING_2D_TYPE:
        stdUtils::setType((stdGeometryHeader*)aResult,
                            STD_MULTILINESTRING_2D_TYPE);
        sObj->mlinestring2D.mNumObjects = sTotal;
        break;
    case STD_POLYGON_2D_EXT_TYPE:
    case STD_POLYGON_2D_TYPE:
        stdUtils::setType((stdGeometryHeader*)aResult,
                            STD_MULTIPOLYGON_2D_TYPE);
        sObj->mpolygon2D.mNumObjects = sTotal;
        break;
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
    case STD_GEOCOLLECTION_2D_TYPE:
        stdUtils::setType((stdGeometryHeader*)aResult,
                            STD_GEOCOLLECTION_2D_TYPE);
        sObj->collection2D.mNumGeometries = sTotal;
        break;
    }
    
    sSubGeom = (stdGeometryHeader*)(aResult + STD_GEOHEAD_SIZE);
    for( i = 0; i < sTotal; i++ ) 
    {
        sSize = STD_GEOM_SIZE(sGeomsDst[i]);        
        idlOS::memcpy( sSubGeom, sGeomsDst[i], sSize );
        sSubGeom = (stdGeometryHeader*)((UChar*)sSubGeom+ sSize);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION(err_invalid_geometry);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void stfAnalysis::initGpcPolygon( stdGpcPolygon* aGpcPoly )
{
    aGpcPoly->mNumContours  = 0;
    aGpcPoly->mContour       = NULL;
    aGpcPoly->mHole          = NULL;
}

IDE_RC
stfAnalysis::setGpcPolygonObjLnk2D( iduMemory     * aQmxMem,
                                    stdGpcPolygon * aGpcPoly,
                                    objMainLink2D * aMLink )

{
    objLink2D *sLink = aMLink->pItem;
    
    while( sLink != NULL )
    {
        IDE_TEST( setGpcPolygon2D( aQmxMem, aGpcPoly, sLink->pObj ) != IDE_SUCCESS );
        sLink = sLink->pNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::setGpcPolygon2D( iduMemory*          aQmxMem,
                              stdGpcPolygon*      aGpcPoly,
                              stdPolygon2DType*   aPoly )
{
    stdLinearRing2D*    sRing;
    stdGpcVertexList    sContour;
    
    UInt                i, j;
    UInt                sMaxR;
    
    stdPoint2D*         sRingPt;
    UInt                sRingPtCnt;

    sContour.mVertex = NULL;
    
    sRing = STD_FIRST_RN2D(aPoly);
    sMaxR = STD_N_RINGS(aPoly);
    
    for( i = 0; i < sMaxR; i++ ) 
    {
        sRingPt = STD_FIRST_PT2D(sRing);
        sRingPtCnt = STD_N_POINTS(sRing);

        sContour.mVertex = NULL;
        
        // BUG-33123
        IDE_TEST_RAISE( sRingPtCnt == 0, ERR_INVALID_GEOMETRY );
        
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdGpcVertex) * sRingPtCnt,
                                  (void**) & sContour.mVertex )
                  != IDE_SUCCESS );
        for( j = 0; j < sRingPtCnt; j++ )
        {
            sContour.mVertex[j].mX = sRingPt[j].mX;
            sContour.mVertex[j].mY = sRingPt[j].mY;
        }
        sContour.mNumVertices = STD_N_POINTS(sRing);
        
        if( i == 0 )
        {
            IDE_TEST( stdGpc::addContour( aQmxMem, aGpcPoly, &sContour, 0 )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( stdGpc::addContour( aQmxMem, aGpcPoly, &sContour, 1 )
                      != IDE_SUCCESS );
        }

        sContour.mVertex = NULL;
        
        sRing = STD_NEXT_RN2D(sRing);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_GEOMETRY );
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


// BUG-16717
IDE_RC
stfAnalysis::getPolygonFromGpc2D( iduMemory             * aQmxMem,
                                  stdMultiPolygon2DType * aMPoly,
                                  stdGpcPolygon         * aGpcPoly,
                                  UInt                    aFence)  //Fix BUG - 25110
{
    stdPolygon2DType*   sPoly;
    stdLinearRing2D*    sRing;
    stdPoint2D*         sPt;
    stdMBR              sMBR;

    ringMainLink2D*       sHeader = NULL;
    ringMainLink2D*       sMainLnk, *sNMainLnk;
    ringLink2D*           sInterior, *sNInterior;

    UInt                sMaxR;
    UInt                i, j, sMax, sSize, sNumPoints;


    sPoly = NULL;
    sMainLnk = NULL;
    sRing = NULL;
    sInterior = NULL;
    
    
    if( aGpcPoly == NULL )
    {
        stdUtils::makeEmpty((stdGeometryHeader*)aMPoly);
        IDE_RAISE(STF_GPC2POLY_2D);
    }

    sMaxR = aGpcPoly->mNumContours;
    
    if( (sMaxR == 0) || (sMaxR >= 90000) )
    {
        stdUtils::makeEmpty((stdGeometryHeader*)aMPoly);
        IDE_RAISE(STF_GPC2POLY_2D);
    }

    stdUtils::setType((stdGeometryHeader*)aMPoly,STD_MULTIPOLYGON_2D_TYPE);
    aMPoly->mSize = STD_MPOLY2D_SIZE;
    aMPoly->mNumObjects = 0;
    
    // �ܺθ� ���� ��ũ�� �����.
    for( i = 0; i < sMaxR; i++ )
    {
        if( aGpcPoly->mHole[i] == 0 )
        {
            sMax = aGpcPoly->mContour[i].mNumVertices+1;
            
            sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*sMax;
            aMPoly->mNumObjects++;
            
            sPoly = NULL;
            IDE_TEST( aQmxMem->alloc( sSize,
                                      (void**) &sPoly )
                      != IDE_SUCCESS );
            
            stdUtils::makeEmpty((stdGeometryHeader*)sPoly);
            stdUtils::setType((stdGeometryHeader*)sPoly,STD_POLYGON_2D_TYPE);
            sPoly->mNumRings = 1;
            
            sRing = STD_FIRST_RN2D(sPoly);
            sRing->mNumPoints = sMax;
            sPt = STD_FIRST_PT2D(sRing);
            sNumPoints = 0;
            for( j = 0; j < sMax-1; j++ )
            {
                if( ( j != 0 ) &&
                    ( aGpcPoly->mContour[i].mVertex[j-1].mX
                      == aGpcPoly->mContour[i].mVertex[j].mX ) &&
                    ( aGpcPoly->mContour[i].mVertex[j-1].mY
                      == aGpcPoly->mContour[i].mVertex[j].mY) )
                {
                    continue;
                }
                sPt->mX = aGpcPoly->mContour[i].mVertex[j].mX;
                sPt->mY = aGpcPoly->mContour[i].mVertex[j].mY;
                IDE_TEST( stdUtils::mergeMBRFromPoint2D( (stdGeometryHeader*)aMPoly,sPt)
                          != IDE_SUCCESS );
                IDE_TEST( stdUtils::mergeMBRFromPoint2D( (stdGeometryHeader*)sPoly,sPt)
                          != IDE_SUCCESS );
                sPt = STD_NEXT_PT2D(sPt);
                sNumPoints++;
            }
            sNumPoints++;
            sRing->mNumPoints = sNumPoints;
            sSize = STD_POLY2D_SIZE + STD_RN2D_SIZE + STD_PT2D_SIZE*sNumPoints;
            sPoly->mSize = sSize;
            aMPoly->mSize += sSize;
            IDE_TEST_RAISE(aMPoly->mSize > aFence, size_error);

            // To fix BUG-33472 CodeSonar warining

            if ( sPt != STD_FIRST_PT2D(sRing))
            {                               
                idlOS::memcpy( sPt, STD_FIRST_PT2D(sRing), STD_PT2D_SIZE);
            }
            else
            {
                // nothing to do
            }
            
            sPt = STD_NEXT_PT2D(sPt);

            sMainLnk = NULL;
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(ringMainLink2D),
                                      (void**) & sMainLnk )
                      != IDE_SUCCESS );

            sMainLnk->pInterior = NULL;
            sMainLnk->pPoly = sPoly;            
        
            if( sHeader == NULL )
            {
                sHeader = sMainLnk;
                sHeader->pNext = NULL;
            }
            else
            {
                sMainLnk->pNext = sHeader->pNext;
                sHeader->pNext = sMainLnk;
            }
        }
    }
    
    // ���θ��� ��ũ�� �����Ѵ�.
    for( i = 0; i < sMaxR; i++ )
    {
        if( aGpcPoly->mHole[i] != 0 )
        {
            sMax = aGpcPoly->mContour[i].mNumVertices+1;
            
            sSize = STD_RN2D_SIZE + STD_PT2D_SIZE*sMax;
            
            stdUtils::initMBR( &sMBR );

            sRing = NULL;
            IDE_TEST( aQmxMem->alloc( sSize,
                                      (void**) &sRing )
                      != IDE_SUCCESS );
            
            sPt = STD_FIRST_PT2D(sRing);
            sNumPoints = 0;
            for( j = 0; j < sMax-1; j++ )
            {
                if( ( j != 0 ) &&
                    ( aGpcPoly->mContour[i].mVertex[j-1].mX
                      == aGpcPoly->mContour[i].mVertex[j].mX ) &&
                    ( aGpcPoly->mContour[i].mVertex[j-1].mY
                      == aGpcPoly->mContour[i].mVertex[j].mY ) )
                {
                    continue;
                }
                sPt->mX = aGpcPoly->mContour[i].mVertex[j].mX;
                sPt->mY = aGpcPoly->mContour[i].mVertex[j].mY;
                
                IDE_TEST( stdUtils::setMBRFromPoint2D( &sMBR, sPt ) != IDE_SUCCESS );

                sPt = STD_NEXT_PT2D(sPt);
                sNumPoints++;
            }
            sNumPoints++;
            sSize = STD_RN2D_SIZE + STD_PT2D_SIZE*sNumPoints;
            sRing->mNumPoints = sNumPoints;
            aMPoly->mSize += sSize;
            IDE_TEST_RAISE( aMPoly->mSize > aFence, size_error);
            if ( sPt != STD_FIRST_PT2D(sRing) )
            {
                idlOS::memcpy( sPt, STD_FIRST_PT2D(sRing), STD_PT2D_SIZE);
            }
            sPt = STD_NEXT_PT2D(sPt);
            
            sMainLnk = sHeader;
            while(sMainLnk)
            {
                sNMainLnk = sMainLnk->pNext;
                
                if( stdUtils::isRingContainsRing2D( 
                    STD_FIRST_RN2D(sMainLnk->pPoly), &sMainLnk->pPoly->mMbr,
                    sRing, &sMBR ) )
                {
                    break;
                }
                
                sMainLnk = sNMainLnk;
            }
            
            if( sMainLnk != NULL )
            {
                sInterior = NULL;
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(ringLink2D),
                                          (void**) &sInterior )
                          != IDE_SUCCESS );
                sInterior->pRing = sRing;
                sInterior->pNext = sMainLnk->pInterior;
                sMainLnk->pInterior = sInterior;

                sPoly = sMainLnk->pPoly;
                sPoly->mSize += sSize;
                sPoly->mNumRings++;                
            }
            else
            {
                sRing = NULL;
            }
        }
    }

    sPoly = STD_FIRST_POLY2D(aMPoly);
    sMainLnk = sHeader;
    while(sMainLnk != NULL)
    {
        sNMainLnk = sMainLnk->pNext;
        idlOS::memcpy( sPoly,
                       sMainLnk->pPoly,
                       STD_POLY2D_SIZE + STD_RN2D_SIZE
                       +  STD_PT2D_SIZE
                       * STD_N_POINTS(STD_FIRST_RN2D(sMainLnk->pPoly)) );

        sRing = STD_FIRST_RN2D(sPoly);
        sRing = STD_NEXT_RN2D(sRing);
        sInterior = sMainLnk->pInterior;            
        while( sInterior != NULL ) 
        {
            sNInterior = sInterior->pNext;
            idlOS::memcpy( sRing, sInterior->pRing, STD_RN2D_SIZE +
                    STD_PT2D_SIZE*STD_N_POINTS(sInterior->pRing) );
            sRing = STD_NEXT_RN2D(sRing);
            sInterior->pRing = NULL;
            sInterior = sNInterior;
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
        sMainLnk->pPoly = NULL;
        sMainLnk = sNMainLnk;
    }

    IDE_EXCEPTION_CONT(STF_GPC2POLY_2D);

    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC
stfAnalysis::allocMPolyFromObjLink2D( iduMemory*              aQmxMem,
                                      stdMultiPolygon2DType** aMPoly,
                                      objMainLink2D*          aMLink,
                                      UInt                    aFence )  //Fix BUG - 25110
{
    objLink2D*            sObjLnk = aMLink->pItem;
    stdPolygon2DType*   sPoly;
    UInt                sTotalSize = STD_MPOLY2D_SIZE;
    SInt                sPolyCnt = 0;
    while( sObjLnk )
    {
        sTotalSize += STD_GEOM_SIZE(sObjLnk->pObj);

        IDE_TEST_RAISE(sTotalSize > aFence, size_error);
        sPolyCnt++;
        
        sObjLnk = sObjLnk->pNext;
    }
    
    IDE_TEST( aQmxMem->alloc( sTotalSize,
                              (void**) aMPoly )
              != IDE_SUCCESS );

    stdUtils::makeEmpty((stdGeometryHeader*)(*aMPoly));
    stdUtils::setType((stdGeometryHeader*)(*aMPoly),STD_MULTIPOLYGON_2D_TYPE);
    IDE_TEST_RAISE(sTotalSize > aFence, size_error);
    (*aMPoly)->mSize = sTotalSize;
    (*aMPoly)->mNumObjects = sPolyCnt;

    sObjLnk = aMLink->pItem;
    sPoly = STD_FIRST_POLY2D((*aMPoly));
    while( sObjLnk ) 
    {
        idlOS::memcpy( sPoly, sObjLnk->pObj, STD_GEOM_SIZE(sObjLnk->pObj) );
        
        IDE_TEST( stdUtils::mergeMBRFromHeader((stdGeometryHeader*)(*aMPoly), 
                                               (stdGeometryHeader*)sPoly)
                   != IDE_SUCCESS );
        
        sObjLnk = sObjLnk->pNext;
        sPoly = STD_NEXT_POLY2D(sPoly);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
   
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �ԷµǴ� �������� �������� �ʴ� ��ũ�� ã�� �߰��Ѵ�.
 *
 * objMainLink2D*      aHeader(Out):
 * stdPolygon2DType*   aPoly(In):
 **********************************************************************/

IDE_RC
stfAnalysis::insertObjLink2D( iduMemory        * aQmxMem,
                              objMainLink2D    * aHeader,
                              stdPolygon2DType * aPoly )
{
    objLink2D*            sObjLnk;
    objLink2D*            sTmpLnk;
    objMainLink2D*        sMainLnk = aHeader;
    stdLinearRing2D*    sRing;
    stdLinearRing2D*    sRingCmp;
    stdMBR              sMBR;
    stdMBR              sMBRCmp;

    if( aHeader->pItem == NULL ) 
    {
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(objLink2D),
                                  (void**) & sObjLnk )
                  != IDE_SUCCESS );
                                     
        sObjLnk->pObj = aPoly;
        sObjLnk->pNext = NULL;
        
        aHeader->pItem = sObjLnk;
        IDE_RAISE(STF_INSERT_OBJLINK_2D);
    }
    
    sRing = STD_FIRST_RN2D(aPoly);
    stdUtils::initMBR( &sMBR );
    IDE_TEST( stdUtils::setMBRFromRing2D(&sMBR, sRing) != IDE_SUCCESS );
    while( sMainLnk ) 
    {
        sTmpLnk = sMainLnk->pItem;
        while( sTmpLnk ) 
        {
            sRingCmp = STD_FIRST_RN2D(sTmpLnk->pObj);            
            stdUtils::initMBR( &sMBRCmp );
            IDE_TEST( stdUtils::setMBRFromRing2D(&sMBRCmp, sRingCmp) != IDE_SUCCESS );
            
            if( stdUtils::isRingNotDisjoint2D( sRing,
                                               & sMBR,
                                               sRingCmp,
                                               & sMBRCmp ) 
                == ID_TRUE )
            {
                break;
            }
            sTmpLnk = sTmpLnk->pNext;
        }
        if( sTmpLnk == NULL ) 
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(objLink2D),
                                      (void**) & sObjLnk )
                      != IDE_SUCCESS );
            
            sObjLnk->pObj = aPoly;
            sObjLnk->pNext = sMainLnk->pItem;
            
            sMainLnk->pItem = sObjLnk;
            IDE_RAISE(STF_INSERT_OBJLINK_2D);
        }
        sMainLnk = sMainLnk->pNext;
    }

    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(objLink2D),
                              (void**) & sObjLnk )
              != IDE_SUCCESS );
    
    sObjLnk->pObj = aPoly;
    sObjLnk->pNext = NULL;

    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(objMainLink2D),
                              (void**) & sMainLnk )
                      != IDE_SUCCESS );
    
    sMainLnk->pNext = aHeader->pNext;
    sMainLnk->pItem = sObjLnk;
    
    aHeader->pNext = sMainLnk;

    IDE_EXCEPTION_CONT(STF_INSERT_OBJLINK_2D);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////
// Point Link, Line Link
/////////////////////////////////////////////////////////////////////////////

IDE_RC
stfAnalysis::primIntersectionLineLine2D( iduMemory        * aQmxMem,
                                         const stdPoint2D * aPt11,
                                         const stdPoint2D * aPt12,
                                         const stdPoint2D * aPt21,
                                         const stdPoint2D * aPt22,
                                         stdPoint2D      ** aRet1,
                                         stdPoint2D      ** aRet2 )
{
    if( stdUtils::quadLinear2D( aPt11, aPt12, aPt21, aPt22 ) == ID_TRUE )
    {
        if(stdUtils::between2D( aPt11, aPt12, aPt21 )==ID_TRUE)
        {
            if( (*aRet1) == NULL )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                
                (*aRet1)->mX = aPt21->mX;
                (*aRet1)->mY = aPt21->mY;
            }
        }
        
        if(stdUtils::between2D( aPt11, aPt12, aPt22 )==ID_TRUE)
        {
            if( (*aRet1) == NULL )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                
                (*aRet1)->mX = aPt22->mX;
                (*aRet1)->mY = aPt22->mY;
            }
            else if( ((*aRet2) == NULL) &&
                     (stdUtils::isSamePoints2D(*aRet1, aPt22) == ID_FALSE) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                
                (*aRet2)->mX = aPt22->mX;
                (*aRet2)->mY = aPt22->mY;
            }
        }
        
        if(stdUtils::between2D( aPt21, aPt22, aPt11 )==ID_TRUE)
        {
            if( (*aRet1) == NULL )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt11->mX;
                (*aRet1)->mY = aPt11->mY;
            }
            else if( ((*aRet2) == NULL) &&
                     (stdUtils::isSamePoints2D(*aRet1, aPt11) == ID_FALSE) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                (*aRet2)->mX = aPt11->mX;
                (*aRet2)->mY = aPt11->mY;
            }
        }
        
        if(stdUtils::between2D( aPt21, aPt22, aPt12 )==ID_TRUE)
        {
            if( (*aRet1) == NULL )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt12->mX;
                (*aRet1)->mY = aPt12->mY;
            }
            else if( ((*aRet2) == NULL) &&
                (stdUtils::isSamePoints2D(*aRet1, aPt12) == ID_FALSE) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                (*aRet2)->mX = aPt12->mX;
                (*aRet2)->mY = aPt12->mY;
            }
        }
    }
    else if(stdUtils::intersect2D( aPt11, aPt12, aPt21, aPt22 )==ID_TRUE)
    {
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                  (void**) aRet1 )
                  != IDE_SUCCESS );
        if( stdUtils::getIntersection2D(aPt11, aPt12, aPt21, aPt22, (*aRet1))
            == ID_FALSE )
        {
            (*aRet1) = NULL;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRet1 = NULL;
    *aRet2 = NULL;
    
    return IDE_FAILURE;
}

//BUG-28693
IDE_RC
stfAnalysis::primIntersectionLineArea2D( iduMemory                 * aQmxMem,
                                         const stdLineString2DType * aObj1,
                                         const stdPolygon2DType    * aObj2,
                                         pointLink2D              ** aPtHeader,
                                         lineLink2D               ** aLineHeader,
                                         UInt                      * aPtCnt,
                                         UInt                      * aLineCnt,
                                         UInt                        aFence )
{
    stdLineString2DType * sLine;
    stdLineString2DType * sTmpLine = NULL;
    stdPolygon2DType    * sPoly;
    stdPoint2D          * sPt1;
    stdPoint2D          * sPt2;
    stdPoint2D          * spRet1;
    stdPoint2D          * spRet2;
    stdPoint2D          * sPrePoint;
    stdLinearRing2D     * sRing;
    stdPoint2DType        sPoint;
    UInt                  i;
    UInt                  j;
    UInt                  k;
    UInt                  sMaxLine;
    UInt                  sMaxRing;
    UInt                  sMaxPoint;
    UInt                  sPtCount  = 0;
    idBool                sInserted;
    idBool                sXOrder   = ID_TRUE;
    pointLink2D         * sPtHeader = NULL;
    pointLink2D         * sLnk;
    SChar                 sPattern[10];
    mtdBooleanType        sReturn;

    idlOS::strcpy( (SChar*)sPattern, "FF*******");

    sLine = (stdLineString2DType*)stdUtils::getFirstGeometry(
        (stdGeometryHeader*)aObj1);

    IDE_TEST_RAISE( sLine == NULL, err_invalid_object_mType );

    stdUtils::makeEmpty((stdGeometryHeader*)&sPoint);
    stdUtils::setType((stdGeometryHeader*)&sPoint,STD_POINT_2D_TYPE);

    sMaxLine = STD_N_POINTS(sLine)-1;
    sPt1 = STD_FIRST_PT2D(sLine);

    IDE_TEST (aQmxMem->alloc( STD_LINE2D_SIZE + STD_POINT2D_SIZE+ STD_POINT2D_SIZE,
                              (void**) &sTmpLine)
              != IDE_SUCCESS );

    sPoint.mPoint.mX = sPt1->mX;
    sPoint.mPoint.mY = sPt1->mY;

    sPoly = (stdPolygon2DType*)stdUtils::getFirstGeometry(
        (stdGeometryHeader*)aObj2);

    IDE_TEST( stfRelation::relate(aQmxMem,
                                  (stdGeometryType*)&sPoint,
                                  (stdGeometryType*)sPoly,
                                  sPattern,
                                  &sReturn)
              != IDE_SUCCESS );

    if ( sReturn == MTD_BOOLEAN_FALSE )
    {
        IDE_TEST( stfAnalysis::insertSortPointLink2D ( aQmxMem,
                                                       &sPtHeader,
                                                       sPt1,
                                                       sXOrder,
                                                       &sInserted)
                  != IDE_SUCCESS );

        if ( sInserted == ID_TRUE )
        {
            sPtCount ++;
        }
    }

    for ( i = 0; i < sMaxLine ; i++)  // Line
    {
        sPoly = (stdPolygon2DType*)stdUtils::getFirstGeometry(
            (stdGeometryHeader*)aObj2);

        IDE_TEST_RAISE( sPoly == NULL, err_invalid_object_mType );

        sMaxRing = STD_N_RINGS(sPoly);
        sRing = STD_FIRST_RN2D(sPoly);

        if ( sPt1->mX != STD_NEXT_PT2D(sPt1)->mX )
        {
            sXOrder = ID_TRUE;
        }
        else
        {
            sXOrder = ID_FALSE;
        }

        for ( j = 0 ; j < sMaxRing ; j++)  //Ring
        {
            sMaxPoint = STD_N_POINTS(sRing)-1;
            sPt2 = STD_FIRST_PT2D(sRing);

            for ( k = 0 ; k < sMaxPoint ; k ++) //Ring-Line
            {
                spRet1 = NULL;
                spRet2 = NULL;

                IDE_TEST( stfAnalysis::primIntersectionLineLine2D ( aQmxMem,
                                                                    sPt1,
                                                                    STD_NEXT_PT2D(sPt1),
                                                                    sPt2,
                                                                    STD_NEXT_PT2D(sPt2),
                                                                    &spRet1,
                                                                    &spRet2 )
                          != IDE_SUCCESS );

                if ( spRet1 != NULL )
                {
                    IDE_TEST( stfAnalysis::insertSortPointLink2D ( aQmxMem,
                                                                   &sPtHeader,
                                                                   spRet1,
                                                                   sXOrder,
                                                                   &sInserted)
                             != IDE_SUCCESS );
                    if (sInserted == ID_TRUE )
                    {
                        sPtCount ++;
                    }
                }

                if ( spRet2 != NULL )
                {
                    IDE_TEST( stfAnalysis::insertSortPointLink2D ( aQmxMem,
                                                                   &sPtHeader,
                                                                   spRet2,
                                                                   sXOrder,
                                                                   &sInserted)
                             != IDE_SUCCESS );
                    if (sInserted == ID_TRUE )
                    {
                        sPtCount ++;
                    }
                }

                sPt2 = STD_NEXT_PT2D(sPt2);
            }
            sRing = STD_NEXT_RN2D(sRing);
        }

        //calc

        sPt1 = STD_NEXT_PT2D(sPt1);

        sPoint.mPoint.mX = sPt1->mX;
        sPoint.mPoint.mY = sPt1->mY;

        IDE_TEST( stfRelation::relate(aQmxMem,
                                      (stdGeometryType*)&sPoint,
                                      (stdGeometryType*)sPoly,
                                      sPattern,
                                      &sReturn)
                  != IDE_SUCCESS );
        
        if ( sReturn == MTD_BOOLEAN_FALSE )
        {
            IDE_TEST( stfAnalysis::insertSortPointLink2D ( aQmxMem,
                                                           &sPtHeader,
                                                           sPt1,
                                                           sXOrder,
                                                           &sInserted)
                      != IDE_SUCCESS );
            if ( sInserted == ID_TRUE )
            {
                sPtCount ++;
            }
        }
    }

    //calc

    sLnk = sPtHeader;

    if ( sPtCount == 1)
    {
        IDE_TEST( stfAnalysis::insertPointLink2D ( aQmxMem,
                                                   aPtHeader,
                                                   &(sLnk->Point),
                                                   &sInserted)
                  != IDE_SUCCESS );
        (*aPtCnt)++;
    }
    else if ( sPtCount > 1)
    {
        sPrePoint = &(sLnk->Point);
        sLnk = sLnk->pNext;

        IDE_TEST( stfAnalysis::insertPointLink2D ( aQmxMem,
                                                   aPtHeader,
                                                   sPrePoint,
                                                   &sInserted)
                  != IDE_SUCCESS );

        while ( sLnk != NULL )
        {
            IDE_TEST( stfAnalysis::makeLineFrom2PointLink( sPrePoint,
                                                           &(sLnk->Point),
                                                           (stdGeometryHeader*)sTmpLine,
                                                           aFence )
                      != IDE_SUCCESS);

            if ( (stfRelation::sliTosai(sTmpLine, sPoly) !='F'))
            {
                IDE_TEST( insertLineLink2D( aQmxMem,
                                            aLineHeader,
                                            sPrePoint,
                                            &sLnk->Point)
                          != IDE_SUCCESS );
                (*aLineCnt)++;
            }
            else
            {
                IDE_TEST( stfAnalysis::insertPointLink2D ( aQmxMem,
                                                           aPtHeader,
                                                           &(sLnk->Point),
                                                           &sInserted)
                          != IDE_SUCCESS );

                if ( sInserted == ID_TRUE )
                {
                    (*aPtCnt)++;
                }
            }

            sPrePoint = &sLnk->Point;
            sLnk = sLnk->pNext;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::primUnionLineLine2D( iduMemory        * aQmxMem,
                                  const stdPoint2D * aPt11,
                                  const stdPoint2D * aPt12,
                                  const stdPoint2D * aPt21,
                                  const stdPoint2D * aPt22,
                                  stdPoint2D      ** aRet1,
                                  stdPoint2D      ** aRet2 )
{
    stdPoint2D  sAB;
    stdPoint2D  sAC;
    stdPoint2D  sAD;
    SDouble     sDotC, sDotD;
    SDouble     sLenAB, sLenAC, sLenAD;

    if( stdUtils::quadLinear2D( aPt11, aPt12, aPt21, aPt22 ) == ID_TRUE )
    {
        stdUtils::subVec2D( aPt11, aPt12, &sAB );
        stdUtils::subVec2D( aPt11, aPt21, &sAC );
        stdUtils::subVec2D( aPt11, aPt22, &sAD );
        sLenAB = stdUtils::length2D(&sAB);
        sLenAC = stdUtils::length2D(&sAC);
        sLenAD = stdUtils::length2D(&sAD);
        
        sDotC = stdUtils::dot2D( &sAB, &sAC );
        sDotD = stdUtils::dot2D( &sAB, &sAD );
        
        if( (sDotC >= 0) && (sDotD >= 0) )    // AC,AD�� AB�� ���� ����
        {
            if( (sLenAB >= sLenAC) && (sLenAB >= sLenAD) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                
                (*aRet1)->mX = aPt11->mX;
                (*aRet1)->mY = aPt11->mY;

                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                
                (*aRet2)->mX = aPt12->mX;
                (*aRet2)->mY = aPt12->mY;
            }
            else if( (sLenAB >= sLenAC) && (sLenAB < sLenAD) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                
                (*aRet1)->mX = aPt11->mX;
                (*aRet1)->mY = aPt11->mY;
                
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                
                (*aRet2)->mX = aPt22->mX;
                (*aRet2)->mY = aPt22->mY;
            }
            else if( (sLenAB < sLenAC) && (sLenAB >= sLenAD) )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt11->mX;
                (*aRet1)->mY = aPt11->mY;
                
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet2 )
                          != IDE_SUCCESS );
                (*aRet2)->mX = aPt21->mX;
                (*aRet2)->mY = aPt21->mY;
            }
        }
        else if( (sDotC >= 0) && (sDotD < 0) )    // AC�� ���� ����
        {
            if(sLenAB >= sLenAC)
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt12->mX;
                (*aRet1)->mY = aPt12->mY;
            }
            else
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt21->mX;
                (*aRet1)->mY = aPt21->mY;
            }
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                      (void**) aRet2 )
                      != IDE_SUCCESS );
            (*aRet2)->mX = aPt22->mX;
            (*aRet2)->mY = aPt22->mY;
        }
        else if( (sDotC < 0) && (sDotD >= 0) )    // AD�� ���� ����
        {
            if(sLenAB >= sLenAD)
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt12->mX;
                (*aRet1)->mY = aPt12->mY;
            }
            else
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                          (void**) aRet1 )
                          != IDE_SUCCESS );
                (*aRet1)->mX = aPt22->mX;
                (*aRet1)->mY = aPt22->mY;
            }
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(stdPoint2D),
                                      (void**) aRet2 )
                      != IDE_SUCCESS );
            (*aRet2)->mX = aPt21->mX;
            (*aRet2)->mY = aPt21->mY;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRet1 = NULL;
    *aRet2 = NULL;
    
    return IDE_FAILURE;
}

IDE_RC
stfAnalysis::makeGeomFromPointLineLink2D( iduMemory         * aQmxMem,
                                          pointLink2D      ** aPtHeader,
                                          SInt                aPtCnt,
                                          lineLink2D        * aLineHeader,
                                          stdGeometryHeader * aRet,
                                          UInt                aFence )  //Fix BUG - 25110
{
    pointLink2D     *sPtLnk, *sPtLnkNext, *sPtLnkPrev;
    lineLink2D      *sLineLnk;
    idBool          sFound;

    sPtLnk = (*aPtHeader);    
    sPtLnkPrev = NULL;
    while( sPtLnk )
    {
        sPtLnkNext = sPtLnk->pNext;

        sFound = ID_FALSE;
        
        sLineLnk = aLineHeader;
        while( sLineLnk )
        {
            if( stdUtils::between2D( &sLineLnk->Point1, &sLineLnk->Point2, 
                &sPtLnk->Point ) == ID_TRUE )
            {
                sFound = ID_TRUE;
                break;
            }
            sLineLnk = sLineLnk->pNext;
        }
        
        if( sFound == ID_TRUE )
        {
            if( sPtLnkPrev != NULL )
            {
                sPtLnkPrev->pNext = sPtLnkNext;
            }

            if( sPtLnk == (*aPtHeader) )
            {
                (*aPtHeader) = sPtLnkNext;
            }
            
            sPtLnk = NULL;
            
            aPtCnt--;
        }
        else
        {
            sPtLnkPrev = sPtLnk;
        }
        
        sPtLnk = sPtLnkNext;
    }
    
    if( aPtCnt <= 0 )
    {
        IDE_TEST( makeMLineFromLineLink2D( aQmxMem, aLineHeader, aRet, aFence )
                  != IDE_SUCCESS );
        
        stdUtils::shiftMultiObjToSingleObj(aRet);
    }
    else
    {
        stdGeoCollection2DType*     sColl = (stdGeoCollection2DType*)aRet;
        stdGeometryHeader*          sSubObj;
    
        stdUtils::makeEmpty(aRet);
        stdUtils::setType(aRet, STD_GEOCOLLECTION_2D_TYPE);
        sColl->mSize = STD_COLL2D_SIZE;
        sColl->mNumGeometries = 2;
    
        sSubObj = (stdGeometryHeader*)STD_FIRST_COLL2D(sColl);
        IDE_TEST( makeMPointFromPointLink2D(*aPtHeader, sSubObj, aFence) != IDE_SUCCESS);
        IDE_TEST_RAISE( sColl->mSize + STD_GEOM_SIZE(sSubObj) > aFence , size_error);
        stdUtils::shiftMultiObjToSingleObj(sSubObj);
        sColl->mSize += STD_GEOM_SIZE(sSubObj);
        IDE_TEST( stdUtils::mergeMBRFromHeader(aRet,sSubObj) != IDE_SUCCESS );
    
        sSubObj = (stdGeometryHeader*)STD_NEXT_GEOM(sSubObj);
        IDE_TEST( makeMLineFromLineLink2D(aQmxMem, aLineHeader, sSubObj, aFence)
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sColl->mSize + STD_GEOM_SIZE(sSubObj) > aFence , size_error);
        stdUtils::shiftMultiObjToSingleObj(sSubObj);
        sColl->mSize += STD_GEOM_SIZE(sSubObj);
        IDE_TEST( stdUtils::mergeMBRFromHeader(aRet,sSubObj) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

      IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC stfAnalysis::makeMPointFromPointLink2D(
                    pointLink2D*                aPtHeader,
                    stdGeometryHeader*          aRet,
                    UInt                        aFence)  //Fix BUG - 25110
{
    stdMultiPoint2DType*    sMPoint = (stdMultiPoint2DType*)aRet;
    stdPoint2DType*         sPoint, *sTemp;
    pointLink2D*            sLnk = aPtHeader;
    idBool                  sFound;
    
    stdUtils::makeEmpty(aRet);
    stdUtils::setType(aRet, STD_MULTIPOINT_2D_TYPE);
    sMPoint->mSize = STD_MPOINT2D_SIZE;
    sMPoint->mNumObjects = 0;
    
    sPoint = STD_FIRST_POINT2D(sMPoint);
    while(sLnk)
    {
        sFound = ID_FALSE;
    
        sTemp = STD_FIRST_POINT2D(sMPoint);
        while(sTemp < sPoint)
        {
            if( (sTemp->mPoint.mX == sLnk->Point.mX) && 
                (sTemp->mPoint.mY == sLnk->Point.mY) )
            {
                sFound = ID_TRUE;
                break;
            }
            sTemp = STD_NEXT_POINT2D(sTemp);
        }
    
        if( sFound == ID_TRUE )
        {
            sLnk = sLnk->pNext;
            continue;
        }
    
        sMPoint->mSize += STD_POINT2D_SIZE;
        IDE_TEST_RAISE ( sMPoint->mSize > aFence , size_error );
        sMPoint->mNumObjects++;
                
        stdUtils::makeEmpty((stdGeometryHeader*)sPoint);
        stdUtils::setType((stdGeometryHeader*)sPoint, STD_POINT_2D_TYPE);
        sPoint->mSize = STD_POINT2D_SIZE;

        IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, &sLnk->Point)
                  != IDE_SUCCESS );
        IDE_TEST( stdUtils::mergeMBRFromPoint2D((stdGeometryHeader*)sPoint, &sLnk->Point)
                  != IDE_SUCCESS );
        
        sPoint->mPoint.mX = sLnk->Point.mX;
        sPoint->mPoint.mY = sLnk->Point.mY;
        
        sPoint = STD_NEXT_POINT2D(sPoint);
        sLnk = sLnk->pNext;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }

    IDE_EXCEPTION_END;



    return IDE_FAILURE;

}


//BUG-28693
IDE_RC
stfAnalysis::makeLineFrom2PointLink( stdPoint2D        * aFirst,
                                     stdPoint2D        * aSecond,
                                     stdGeometryHeader * aRet,
                                     UInt                aFence)
{
    stdPoint2D          * sPt;
    stdLineString2DType * sLine;
    UInt                  sTwoPtLineSize;

    sTwoPtLineSize = STD_LINE2D_SIZE + STD_PT2D_SIZE + STD_PT2D_SIZE ;

    IDE_TEST_RAISE( sTwoPtLineSize > aFence , size_error );

    stdUtils::makeEmpty(aRet);
    stdUtils::setType(aRet, STD_LINESTRING_2D_TYPE);

    sLine = (stdLineString2DType*)aRet;
    sLine->mSize = sTwoPtLineSize;
    sLine->mNumPoints = 2;

    sPt = STD_FIRST_PT2D(sLine);
    sPt->mX = aFirst->mX;
    sPt->mY = aFirst->mY;

    IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt) != IDE_SUCCESS );

    sPt = STD_NEXT_PT2D(sPt);

    sPt->mX = aSecond->mX;
    sPt->mY = aSecond->mY;

    IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt) != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
stfAnalysis::makeMLineFromLineLink2D( iduMemory         * aQmxMem,
                                      lineLink2D        * aLineHeader,
                                      stdGeometryHeader * aRet,
                                      UInt                aFence )  //Fix BUG - 25110
{
    stdMultiLineString2DType*   sMLine;
    stdLineString2DType*        sLine;
    stdPoint2D*                 sPt;
    lineLink2D*                 sLnk;
    
    lineMainLink2D              sMainLnkHeader;
    lineMainLink2D*             sMainLnk;
    lineLink2D*                 sSubLnk;
    lineLink2D*                 sNewItem;
    idBool                      sBFound;
    UInt                        sLineCnt = 0;
    
    sMainLnkHeader.pItemFirst = NULL;
    sMainLnkHeader.pItemLast = NULL;
    sMainLnkHeader.pNext = NULL;

    sLnk = aLineHeader;
    while(sLnk != NULL)
    {
        if( stdUtils::isSamePoints2D(&sLnk->Point1, &sLnk->Point2) == ID_TRUE )
        {
            sLnk = sLnk->pNext;
            continue;
        }

        if(sMainLnkHeader.pItemFirst == NULL)
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                      (void**) & sNewItem )
                      != IDE_SUCCESS );
                      
            sNewItem->Point1.mX = sLnk->Point1.mX;
            sNewItem->Point1.mY = sLnk->Point1.mY;
            sNewItem->Point2.mX = sLnk->Point2.mX;
            sNewItem->Point2.mY = sLnk->Point2.mY;
            sNewItem->pNext = NULL;
            sMainLnkHeader.pItemFirst = sNewItem;
            sMainLnkHeader.pItemLast = sNewItem;
            sLineCnt = 1;
            
            sLnk = sLnk->pNext;
            continue;
        }
        
        sBFound = ID_FALSE;
        sMainLnk = &sMainLnkHeader;
        while(sMainLnk)
        {
            if( stdUtils::isSamePoints2D(
                    &sLnk->Point1, 
                    &sMainLnk->pItemFirst->Point1) == ID_TRUE )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                          (void**) & sNewItem )
                          != IDE_SUCCESS );
                          
                sNewItem->Point1.mX = sLnk->Point2.mX;
                sNewItem->Point1.mY = sLnk->Point2.mY;
                sNewItem->Point2.mX = sLnk->Point1.mX;
                sNewItem->Point2.mY = sLnk->Point1.mY;
                sNewItem->pNext = sMainLnk->pItemFirst;
                sMainLnk->pItemFirst = sNewItem;

                sBFound = ID_TRUE;
                break;                
            }
            else if( stdUtils::isSamePoints2D(
                    &sLnk->Point2, 
                    &sMainLnk->pItemFirst->Point1) == ID_TRUE )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                          (void**) & sNewItem )
                          != IDE_SUCCESS );
                          
                sNewItem->Point1.mX = sLnk->Point1.mX;
                sNewItem->Point1.mY = sLnk->Point1.mY;
                sNewItem->Point2.mX = sLnk->Point2.mX;
                sNewItem->Point2.mY = sLnk->Point2.mY;
                sNewItem->pNext = sMainLnk->pItemFirst;
                sMainLnk->pItemFirst = sNewItem;

                sBFound = ID_TRUE;
                break;                
            }            
            else if( stdUtils::isSamePoints2D(
                    &sLnk->Point1, 
                    &sMainLnk->pItemLast->Point2) == ID_TRUE )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                          (void**) & sNewItem )
                          != IDE_SUCCESS );
                
                sNewItem->Point1.mX = sLnk->Point1.mX;
                sNewItem->Point1.mY = sLnk->Point1.mY;
                sNewItem->Point2.mX = sLnk->Point2.mX;
                sNewItem->Point2.mY = sLnk->Point2.mY;
                sNewItem->pNext = NULL;
                sMainLnk->pItemLast->pNext = sNewItem;
                sMainLnk->pItemLast = sNewItem;

                sBFound = ID_TRUE;
                break;                
            }
            else if( stdUtils::isSamePoints2D(
                    &sLnk->Point2,
                    &sMainLnk->pItemLast->Point2) == ID_TRUE )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                          (void**) & sNewItem )
                          != IDE_SUCCESS );
                          
                sNewItem->Point1.mX = sLnk->Point2.mX;
                sNewItem->Point1.mY = sLnk->Point2.mY;
                sNewItem->Point2.mX = sLnk->Point1.mX;
                sNewItem->Point2.mY = sLnk->Point1.mY;
                sNewItem->pNext = NULL;
                sMainLnk->pItemLast->pNext = sNewItem;
                sMainLnk->pItemLast = sNewItem;

                sBFound = ID_TRUE;
                break;                
            }
            
            sMainLnk = sMainLnk->pNext;
        }
        
        if(sBFound == ID_FALSE)
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                      (void**) & sNewItem )
                      != IDE_SUCCESS );
            
            sNewItem->Point1.mX = sLnk->Point1.mX;
            sNewItem->Point1.mY = sLnk->Point1.mY;
            sNewItem->Point2.mX = sLnk->Point2.mX;
            sNewItem->Point2.mY = sLnk->Point2.mY;
            sNewItem->pNext = NULL;

            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineMainLink2D),
                                      (void**) & sMainLnk )
                      != IDE_SUCCESS );

            sMainLnk->pItemFirst = sNewItem;
            sMainLnk->pItemLast = sNewItem;
            
            sMainLnk->pNext = sMainLnkHeader.pNext;
            sMainLnkHeader.pNext = sMainLnk;
            sLineCnt++;
        }        
        sLnk = sLnk->pNext;
    }
    
    stdUtils::makeEmpty(aRet);
    if( sLineCnt > 1)
    {
        sMLine = (stdMultiLineString2DType*)aRet;
        sLine = STD_FIRST_LINE2D(sMLine);

        stdUtils::setType(aRet, STD_MULTILINESTRING_2D_TYPE);
        sMLine->mSize = STD_MLINE2D_SIZE;
        sMLine->mNumObjects = 0;

        sMainLnk = &sMainLnkHeader;
        while(sMainLnk)
        {
            sMLine->mSize += STD_LINE2D_SIZE;
            sMLine->mNumObjects++;
            IDE_TEST_RAISE( sMLine->mSize > aFence , size_error );
            stdUtils::makeEmpty((stdGeometryHeader*)sLine);
            stdUtils::setType((stdGeometryHeader*)sLine, STD_LINESTRING_2D_TYPE);
            sLine->mSize = STD_LINE2D_SIZE;
            sLine->mNumPoints = 0;

            sPt = STD_FIRST_PT2D(sLine);
            sSubLnk = sMainLnk->pItemFirst;
            while(sSubLnk)
            {
                sPt->mX = sSubLnk->Point1.mX;
                sPt->mY = sSubLnk->Point1.mY;

                sMLine->mSize += STD_PT2D_SIZE;
                sLine->mSize += STD_PT2D_SIZE;
                sLine->mNumPoints++;

                IDE_TEST_RAISE( sMLine->mSize > aFence , size_error );
                
                IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt)
                          != IDE_SUCCESS );
                IDE_TEST( stdUtils::mergeMBRFromPoint2D((stdGeometryHeader*)sLine, sPt)
                          != IDE_SUCCESS );
            
                sPt = STD_NEXT_PT2D(sPt);
                
                if(sSubLnk->pNext == NULL)
                {
                    sPt->mX = sSubLnk->Point2.mX;
                    sPt->mY = sSubLnk->Point2.mY;

                    sMLine->mSize += STD_PT2D_SIZE;
                    sLine->mSize += STD_PT2D_SIZE;
                    sLine->mNumPoints++;
                    
                    IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt)
                              != IDE_SUCCESS );
                    IDE_TEST( stdUtils::mergeMBRFromPoint2D((stdGeometryHeader*)sLine, sPt)
                              != IDE_SUCCESS );
                
                    sPt = STD_NEXT_PT2D(sPt);
                }
                
                sSubLnk = sSubLnk->pNext;
            }
            
            sLine = (stdLineString2DType*)sPt;
            
            sMainLnk = sMainLnk->pNext;
        }
    }
    else if( sLineCnt == 1)
    {
        sLine = (stdLineString2DType*)aRet;
    
        sMainLnk = &sMainLnkHeader;
        
        stdUtils::setType(aRet, STD_LINESTRING_2D_TYPE);
        sLine->mSize = STD_LINE2D_SIZE;
        sLine->mNumPoints = 0;
        IDE_TEST_RAISE( sLine->mSize > aFence , size_error );
        sPt = STD_FIRST_PT2D(sLine);
        sSubLnk = sMainLnk->pItemFirst;
        while(sSubLnk)
        {
            sPt->mX = sSubLnk->Point1.mX;
            sPt->mY = sSubLnk->Point1.mY;

            sLine->mSize += STD_PT2D_SIZE;
            sLine->mNumPoints++;
            IDE_TEST_RAISE( sLine->mSize > aFence , size_error );            
            IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt) != IDE_SUCCESS );
        
            sPt = STD_NEXT_PT2D(sPt);

            if(sSubLnk->pNext == NULL)
            {
                sPt->mX = sSubLnk->Point2.mX;
                sPt->mY = sSubLnk->Point2.mY;

                sLine->mSize += STD_PT2D_SIZE;
                sLine->mNumPoints++;
                
                IDE_TEST( stdUtils::mergeMBRFromPoint2D(aRet, sPt) != IDE_SUCCESS );
            
                sPt = STD_NEXT_PT2D(sPt);
            }

            sSubLnk = sSubLnk->pNext;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(size_error);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_BUFFER_OVERFLOW ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
stfAnalysis::makeLineLinkFromLine2D( iduMemory           * aQmxMem,
                                     stdLineString2DType * aLine,
                                     lineLink2D         ** aRetHeader )
{
    stdPoint2D*     sPt = STD_FIRST_PT2D(aLine);
    UInt            i, sMax = STD_N_POINTS(aLine)-1;
    
    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( insertLineLink2D(aQmxMem, aRetHeader, sPt, STD_NEXT_PT2D(sPt))
                  != IDE_SUCCESS );

        sPt = STD_NEXT_PT2D(sPt);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aRetHeader = NULL;
    
    return IDE_FAILURE;
}



/***********************************************************************
 * Description:
 * aLineHeaderSubj��  aLineHeaderClip�� Intersection �Ѵ�.
 * ����� aLineHeaderSubj�� ���´�.
 *
 * lineLink2D* aLineHeaderSubj(InOut): ���� ��ü
 * lineLink2D* aLineHeaderClip(In):    ���� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::makeIntersectionLineLink2D( iduMemory   * aQmxMem,
                                         lineLink2D  * aLineHeaderSubj,
                                         lineLink2D  * aLineHeaderClip,
                                         lineLink2D ** aLineHeaderRet )
{
    lineLink2D*           sLnkSubj;
    lineLink2D*           sLnkClip;
    lineLink2D*           sLnkNew;
    
    SInt                sSector1, sSector2;
    SDouble             sPos1, sPos2;
    
    sLnkSubj = aLineHeaderSubj;
    while(sLnkSubj)
    {
        sLnkClip = aLineHeaderClip;
        while(sLnkClip)
        {
            if( stdUtils::quadLinear2D( 
                &sLnkSubj->Point1,
                &sLnkSubj->Point2,
                &sLnkClip->Point1,
                &sLnkClip->Point2 ) == ID_TRUE )
            {
                sSector1 = stdUtils::sector2D(
                                    &sLnkSubj->Point1,
                                    &sLnkSubj->Point2, 
                                    &sLnkClip->Point1,
                                    &sPos1 );
                sSector2 = stdUtils::sector2D( 
                                    &sLnkSubj->Point1,
                                    &sLnkSubj->Point2, 
                                    &sLnkClip->Point2,
                                    &sPos2 );
                
                sLnkNew = NULL;
                if( (sSector1 == 0) && (sSector2 == 0) )
                {
                    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                              (void**) & sLnkNew )
                              != IDE_SUCCESS );
                    
                    sLnkNew->Point1.mX = sLnkClip->Point1.mX;
                    sLnkNew->Point1.mY = sLnkClip->Point1.mY;
                    sLnkNew->Point2.mX = sLnkClip->Point2.mX;
                    sLnkNew->Point2.mY = sLnkClip->Point2.mY;
                }
                else if( sSector1 == 0 )
                {
                    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                              (void**) & sLnkNew )
                              != IDE_SUCCESS );
                    
                    if( sSector2 < 0 )
                    {
                        sLnkNew->Point1.mX = sLnkSubj->Point1.mX;
                        sLnkNew->Point1.mY = sLnkSubj->Point1.mY;
                        sLnkNew->Point2.mX = sLnkClip->Point1.mX;
                        sLnkNew->Point2.mY = sLnkClip->Point1.mY;
                    }
                    else
                    {
                        sLnkNew->Point1.mX = sLnkClip->Point1.mX;
                        sLnkNew->Point1.mY = sLnkClip->Point1.mY;
                        sLnkNew->Point2.mX = sLnkSubj->Point2.mX;
                        sLnkNew->Point2.mY = sLnkSubj->Point2.mY;
                    }
                }
                else if( sSector2 == 0 )
                {
                    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                              (void**) & sLnkNew )
                              != IDE_SUCCESS );
                    
                    if( sSector1 < 0 )
                    {
                        sLnkNew->Point1.mX = sLnkSubj->Point1.mX;
                        sLnkNew->Point1.mY = sLnkSubj->Point1.mY;
                        sLnkNew->Point2.mX = sLnkClip->Point2.mX;
                        sLnkNew->Point2.mY = sLnkClip->Point2.mY;
                    }
                    else
                    {
                        sLnkNew->Point1.mX = sLnkClip->Point2.mX;
                        sLnkNew->Point1.mY = sLnkClip->Point2.mY;
                        sLnkNew->Point2.mX = sLnkSubj->Point2.mX;
                        sLnkNew->Point2.mY = sLnkSubj->Point2.mY;
                    }
                }
                else if( sSector1*sSector2 == -1 )
                {
                    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                              (void**) & sLnkNew )
                              != IDE_SUCCESS );
                    sLnkNew->Point1.mX = sLnkSubj->Point1.mX;
                    sLnkNew->Point1.mY = sLnkSubj->Point1.mY;
                    sLnkNew->Point2.mX = sLnkSubj->Point2.mX;
                    sLnkNew->Point2.mY = sLnkSubj->Point2.mY;
                }
                
                if( sLnkNew != NULL)
                {
                    if( (*aLineHeaderRet) == NULL )
                    {
                        sLnkNew->pNext = NULL;
                        (*aLineHeaderRet) = sLnkNew;
                    }
                    else
                    {
                        sLnkNew->pNext = (*aLineHeaderRet)->pNext;
                        (*aLineHeaderRet)->pNext = sLnkNew;
                    }
                }
            }            
            sLnkClip = sLnkClip->pNext;
        } // while(sLnkClip)
        sLnkSubj = sLnkSubj->pNext;
    } // while(sLnkSubj)

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aLineHeaderRet = NULL;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * aLineHeaderSubj��  aLineHeaderClip�� difference �Ѵ�.
 * ����� aLineHeaderSubj�� ���´�.
 *
 * lineLink2D* aLineHeaderSubj(InOut): ���� ��ü
 * lineLink2D* aLineHeaderClip(In):    ���� ��ü
 **********************************************************************/

IDE_RC
stfAnalysis::makeDifferenceLineLink2D( iduMemory  * aQmxMem,
                                       lineLink2D * aLineHeaderSubj,
                                       lineLink2D * aLineHeaderClip )
{
    lineLink2D*           sLnkSubj;
    lineLink2D*           sLnkClip;
    lineLink2D*           sLnkNew;
    SInt                sSector1, sSector2;
    SDouble             sPos1, sPos2;

    sLnkNew = NULL;
    
    sLnkSubj = aLineHeaderSubj;
    while(sLnkSubj)
    {
        sLnkClip = aLineHeaderClip;
        while(sLnkClip)
        {
            if( stdUtils::quadLinear2D( 
                &sLnkSubj->Point1,
                &sLnkSubj->Point2,
                &sLnkClip->Point1,
                &sLnkClip->Point2 ) == ID_TRUE )
            {
                sSector1 = stdUtils::sector2D(
                                    &sLnkSubj->Point1,
                                    &sLnkSubj->Point2, 
                                    &sLnkClip->Point1,
                                    &sPos1 );
                sSector2 = stdUtils::sector2D( 
                                    &sLnkSubj->Point1,
                                    &sLnkSubj->Point2, 
                                    &sLnkClip->Point2,
                                    &sPos2 );
                
                if( (sSector1 == 0) && (sSector2 == 0) )
                {
                    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                              (void**) & sLnkNew )
                              != IDE_SUCCESS );
                    
                    sLnkNew->pNext = sLnkSubj->pNext;
                    sLnkSubj->pNext = sLnkNew;
                    
                    if( sPos1 < sPos2 )
                    {
                        sLnkNew->Point1.mX = sLnkClip->Point2.mX;
                        sLnkNew->Point1.mY = sLnkClip->Point2.mY;
                        sLnkNew->Point2.mX = sLnkSubj->Point2.mX;
                        sLnkNew->Point2.mY = sLnkSubj->Point2.mY;
                    
                        sLnkSubj->Point2.mX = sLnkClip->Point1.mX;
                        sLnkSubj->Point2.mY = sLnkClip->Point1.mY;
                    }
                    else
                    {
                        sLnkNew->Point1.mX = sLnkClip->Point1.mX;
                        sLnkNew->Point1.mY = sLnkClip->Point1.mY;
                        sLnkNew->Point2.mX = sLnkSubj->Point2.mX;
                        sLnkNew->Point2.mY = sLnkSubj->Point2.mY;
                    
                        sLnkSubj->Point2.mX = sLnkClip->Point2.mX;
                        sLnkSubj->Point2.mY = sLnkClip->Point2.mY;
                    }
                }
                else if( sSector1 == 0 )
                {
                    if( sSector2 < 0 )
                    {
                        sLnkSubj->Point1.mX = sLnkClip->Point1.mX;
                        sLnkSubj->Point1.mY = sLnkClip->Point1.mY;
                    }
                    else
                    {
                        sLnkSubj->Point2.mX = sLnkClip->Point1.mX;
                        sLnkSubj->Point2.mY = sLnkClip->Point1.mY;
                    }
                }
                else if( sSector2 == 0 )
                {
                    if( sSector1 < 0 )
                    {
                        sLnkSubj->Point1.mX = sLnkClip->Point2.mX;
                        sLnkSubj->Point1.mY = sLnkClip->Point2.mY;
                    }
                    else
                    {
                        sLnkSubj->Point2.mX = sLnkClip->Point2.mX;
                        sLnkSubj->Point2.mY = sLnkClip->Point2.mY;
                    }
                }
            }
            
            if( stdUtils::isSamePoints2D(
                    &sLnkSubj->Point1, 
                    &sLnkSubj->Point2) )
            {
                break;
            }
            sLnkClip = sLnkClip->pNext;
        } // while(sLnkClip)
        sLnkSubj = sLnkSubj->pNext;
    } // while(sLnkSubj)

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * ����Ʈ ��ũ�� ����Ʈ�� �߰��ϴ� �Լ�
 * �ߺ��Ǵ� ����Ʈ�� ������ �Է����� �ʴ´�.
 *
 * pointLink2D** aHeader(InOut): ����Ʈ ��ũ
 * stdPoint2D*   aPt(In):        ����Ʈ
 **********************************************************************/
IDE_RC
stfAnalysis::insertPointLink2D( iduMemory    * aQmxMem,
                                pointLink2D ** aHeader,
                                stdPoint2D   * aPt,
                                idBool       * aInserted )
{
    pointLink2D*    sLnk;
    pointLink2D*    sNewLnk;
    idBool          sFound;

    sLnk = NULL;
    sNewLnk = NULL;

    *aInserted = ID_TRUE;
    
    if( (*aHeader) == NULL )
    {
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                  (void**) & sNewLnk )
                  != IDE_SUCCESS );
        
        sNewLnk->Point.mX = aPt->mX;
        sNewLnk->Point.mY = aPt->mY;
        sNewLnk->pNext = NULL;
        (*aHeader) = sNewLnk;
    }
    else
    {
        sFound = ID_FALSE;

        sLnk = (*aHeader);
        while( sLnk != NULL )
        {
            if( stdUtils::isSamePoints2D(&sLnk->Point, aPt) == ID_TRUE )
            {
                sLnk = NULL;
                sFound = ID_TRUE;
                break;
            }
            sLnk = sLnk->pNext;
        }

        if( sFound == ID_FALSE )
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                      (void**) & sNewLnk )
                      != IDE_SUCCESS );
            sNewLnk->Point.mX = aPt->mX;
            sNewLnk->Point.mY = aPt->mY;
            sNewLnk->pNext = (*aHeader)->pNext;
            (*aHeader)->pNext = sNewLnk;
        }
        else
        {
            *aInserted = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//BUG-28693

IDE_RC
stfAnalysis::insertSortPointLink2D( iduMemory    * aQmxMem,
                                    pointLink2D ** aHeader,
                                    stdPoint2D   * aPt,
                                    idBool         aXOrder,
                                    idBool       * aInserted)
{
    pointLink2D*    sLnk;
    pointLink2D*    sNewLnk;
    pointLink2D*    sPrevLnk;

    sLnk = *aHeader;
    sNewLnk = NULL;
    sPrevLnk = *aHeader;
    *aInserted = ID_TRUE;

    if( (*aHeader) == NULL )
    {
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                  (void**) & sNewLnk )
                  != IDE_SUCCESS );
        sNewLnk->Point.mX = aPt->mX;
        sNewLnk->Point.mY = aPt->mY;
        sNewLnk->pNext = NULL;
        *aHeader = sNewLnk;
    }
    else if( cmpPointOrder( &sLnk->Point, aPt, aXOrder ) < 0 )
    {
        if( stdUtils::isSamePoints2D(&sLnk->Point, aPt) == ID_FALSE )
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                      (void**) & sNewLnk )
                      != IDE_SUCCESS );

            sNewLnk->Point.mX = aPt->mX;
            sNewLnk->Point.mY = aPt->mY;
            sNewLnk->pNext = (*aHeader);
            *aHeader = sNewLnk;
        }
        else
        {
            *aInserted = ID_FALSE;
        }
    }
    else
    {
        sLnk = (*aHeader);
        while( sLnk != NULL )
        {
            if( cmpPointOrder( &sLnk->Point, aPt, aXOrder ) <= 0 )
            {
                break;
            }
            sPrevLnk = sLnk;
            sLnk = sLnk->pNext;
        }

        if( sLnk != NULL )
        {
            if( stdUtils::isSamePoints2D(&sLnk->Point, aPt) == ID_FALSE )
            {
                IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                          (void**) & sNewLnk )
                          != IDE_SUCCESS );
                sNewLnk->Point.mX = aPt->mX;
                sNewLnk->Point.mY = aPt->mY;
                sNewLnk->pNext = sLnk->pNext;
                sPrevLnk->pNext = sNewLnk;
            }
            else
            {
                *aInserted = ID_FALSE;
            }
        }
        else
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(pointLink2D),
                                      (void**) & sNewLnk )
                      != IDE_SUCCESS );
            sNewLnk->Point.mX = aPt->mX;
            sNewLnk->Point.mY = aPt->mY;
            sNewLnk->pNext = NULL;
            sPrevLnk->pNext = sNewLnk;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aInserted = ID_FALSE;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * ���� ��ũ�� ���� ���׸�Ʈ�� �߰��ϴ� �Լ�
 * �ԷµǴ� ���� ���׸�Ʈ�� ������ ���ο� ���Եǰų� ���յǸ� ID_FALSE ����
 *
 * lineLink2D**  aHeader,(InOut): ���� ��ũ
 * stdPoint2D*   aPt1(In): ���� ���׸�Ʈ�� ����Ʈ1
 * stdPoint2D*   aPt2(In): ���� ���׸�Ʈ�� ����Ʈ2
 **********************************************************************/

IDE_RC
stfAnalysis::insertLineLink2D( iduMemory   * aQmxMem,
                               lineLink2D ** aHeader,
                               stdPoint2D  * aPt1,
                               stdPoint2D  * aPt2 )
{
    lineLink2D*     sLnk;
    lineLink2D*     sNewLnk;
    idBool          sFound;
    stdPoint2D*     spRet1;
    stdPoint2D*     spRet2;

    sLnk = NULL;
    sNewLnk = NULL;
    spRet1 = NULL;
    spRet2 = NULL;
    
    if( (*aHeader) == NULL )
    {
        IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                  (void**) & sNewLnk )
                  != IDE_SUCCESS );
        
        sNewLnk->Point1.mX = aPt1->mX;
        sNewLnk->Point1.mY = aPt1->mY;
        sNewLnk->Point2.mX = aPt2->mX;
        sNewLnk->Point2.mY = aPt2->mY;
        sNewLnk->pNext = NULL;
        (*aHeader) = sNewLnk;
    }
    else
    {
        sFound = ID_FALSE;
    
        sLnk = (*aHeader);
        while( sLnk )
        {
            spRet1 = NULL;
            spRet2 = NULL;

            IDE_TEST( primUnionLineLine2D( aQmxMem,
                                           &sLnk->Point1,
                                           &sLnk->Point2, 
                                           aPt1,
                                           aPt2,
                                           &spRet1,
                                           &spRet2 )
                      != IDE_SUCCESS );

            if( (spRet1 != NULL) && (spRet2 != NULL) )
            {
                sLnk->Point1.mX = spRet1->mX;
                sLnk->Point1.mY = spRet1->mY;
                sLnk->Point2.mX = spRet2->mX;
                sLnk->Point2.mY = spRet2->mY;

                spRet1 = NULL;
                spRet2 = NULL;
                
                sFound = ID_TRUE;
                break;
            }
            
            sLnk = sLnk->pNext;
        }

        if ( sFound == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            IDE_TEST( aQmxMem->alloc( ID_SIZEOF(lineLink2D),
                                      (void**) & sLnk )
                      != IDE_SUCCESS );
            
            sLnk->Point1.mX = aPt1->mX;
            sLnk->Point1.mY = aPt1->mY;
            sLnk->Point2.mX = aPt2->mX;
            sLnk->Point2.mY = aPt2->mY;
            sLnk->pNext = (*aHeader)->pNext;        
            (*aHeader)->pNext = sLnk;
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC stfAnalysis::getPolygonClipAreaArea2D( iduMemory*               aQmxMem,
                                              const stdGeometryHeader* aObj1,
                                              const stdGeometryHeader* aObj2,
                                              stdGeometryHeader*       aRet,
                                              SInt                     aOpType,
                                              UInt                     aFence )
{

    stdPolygon2DType*   sPoly1;
    stdPolygon2DType*   sPoly2;
    UInt                i, j , sMax1, sMax2;
    ULong               sTotalPoint      = 0;
    ULong               sTotalRing       = 0;
    Segment**           sIndexSeg;
    Segment**           sTempIndexSeg;    
    Segment**           sReuseSeg;   
    stdLinearRing2D*    sRing;
    UInt                sMaxR;
    UInt                sMaxP;
    UInt                sIndexSegTotal   = 0;
    iduPriorityQueue    sPQueue;
    idBool              sOverflow        = ID_FALSE;
    idBool              sUnderflow       = ID_FALSE;
    Segment*            sCurrNext;
    Segment*            sCurrPrev;
    Segment*            sCurrSeg;
    Segment*            sCmpSeg;
    ULong               sReuseSegCount   = 0;
    SInt                sIntersectStatus;
    SInt                sInterCount;
    stdPoint2D          sInterResult[4];
    PrimInterSeg*       sPrimInterSeg    = NULL;
    Segment**           sRingSegList;
    UInt                sRingCount       = 0;
    ResRingList         sResRingList;
    SegmentRule         sRule[4];
    PrimInterSeg*       sResPrimInterSeg = NULL;

    /* Segment ���� ��� �迭 �ʱ�ȭ */
    sRule[ ST_CLIP_INTERSECT ]     = &stdPolyClip::segmentRuleIntersect;
    sRule[ ST_CLIP_UNION ]         = &stdPolyClip::segmentRuleUnion;
    sRule[ ST_CLIP_DIFFERENCE ]    = &stdPolyClip::segmentRuleDifference;
    sRule[ ST_CLIP_SYMDIFFERENCE ] = &stdPolyClip::segmentRuleSymDifference;

    sResRingList.mBegin   = NULL;
    sResRingList.mEnd     = NULL;
    sResRingList.mRingCnt = 0;
    
    IDE_TEST_RAISE(
        stdUtils::getGroup((stdGeometryHeader*)aObj1) != STD_POLYGON_2D_GROUP ||
        stdUtils::getGroup((stdGeometryHeader*)aObj2) != STD_POLYGON_2D_GROUP ,
        err_incompatible_type );

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);

    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );

        sRing       = STD_FIRST_RN2D(sPoly1);
        sMaxR       = STD_N_RINGS(sPoly1);
        sTotalRing += sMaxR;

        for ( j = 0; j < sMaxR; j++)
        {
            sMaxP        = STD_N_POINTS(sRing);
            sTotalPoint += sMaxP;
            /* BUG-45528 ���� ������ ��Ƽ������/�������� ���� �ÿ� �޸� �Ҵ� ������ �ֽ��ϴ�. */
            sRing        = STD_NEXT_RN2D(sRing);

        }
        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }

    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );

        sRing       = STD_FIRST_RN2D(sPoly2);
        sMaxR       = STD_N_RINGS(sPoly2);
        sTotalRing += sMaxR;
        
        for ( j = 0; j < sMaxR; j++)
        {
            sMaxP        = STD_N_POINTS(sRing);
            sTotalPoint += sMaxP;
            /* BUG-45528 ���� ������ ��Ƽ������/�������� ���� �ÿ� �޸� �Ҵ� ������ �ֽ��ϴ�. */
            sRing        = STD_NEXT_RN2D(sRing);

        }
        
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }

    /* BUG-33634 
     * ������ ����� �����Ǵ� �������� �ε����� �����ϱ� ���ؼ� 
     * sIndexSeg�� ũ�⸦ ������. 
     * sTempIndexSeg�� �Ҵ��ϴ� �κ��� �����ϰ�,
     * sRingSegList�� ũ�⸦ Ring�� ������ �°� ������. */
    IDE_TEST( aQmxMem->alloc( 3 * sTotalPoint * ID_SIZEOF(Segment*),
                              (void**) & sIndexSeg )
              != IDE_SUCCESS);

    IDE_TEST( aQmxMem->alloc( sTotalRing * ID_SIZEOF(Segment*),
                              (void**) & sRingSegList )
              != IDE_SUCCESS);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
        (stdGeometryHeader*)aObj1);
    
    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );
        IDE_TEST_RAISE( sPoly1->mIsValid == ST_INVALID, err_invalid_polygon );
        IDE_TEST_RAISE( sPoly1->mIsValid == ST_UNKNOWN, err_unknown_polygon );
        
        IDE_TEST( stdUtils::classfyPolygonChain( aQmxMem,
                                                 sPoly1,
                                                 i, 
                                                 sIndexSeg,
                                                 &sIndexSegTotal,
                                                 sRingSegList, //
                                                 &sRingCount,
                                                 ID_FALSE )  // 
                  != IDE_SUCCESS );        
        
        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }

    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);
    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );

        IDE_TEST( stdUtils::classfyPolygonChain( aQmxMem,
                                                 sPoly2,
                                                 i + sMax1, 
                                                 sIndexSeg,
                                                 &sIndexSegTotal,
                                                 sRingSegList, //
                                                 &sRingCount,
                                                 ID_FALSE ) //
                  != IDE_SUCCESS ); 
        
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }

    IDE_TEST(sPQueue.initialize(aQmxMem,
                                sIndexSegTotal,
                                ID_SIZEOF(Segment*),
                                cmpSegment )
             != IDE_SUCCESS);
    
    IDE_TEST( aQmxMem->alloc( ID_SIZEOF(Segment*) * sIndexSegTotal,
                              (void**)  &sReuseSeg )
              != IDE_SUCCESS);
    
    for ( i=0; i< sIndexSegTotal; i++ )
    {
        sPQueue.enqueue( &(sIndexSeg[i]), &sOverflow);
        IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
    }
    
    while(1)
    {
        sPQueue.dequeue( (void*) &sCurrSeg, &sUnderflow );
       
        // BUG-37504 codesonar uninitialized variable 
        if ( sUnderflow == ID_TRUE )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        sCurrNext = stdUtils::getNextSeg(sCurrSeg);
        sCurrPrev = stdUtils::getPrevSeg(sCurrSeg);

        sReuseSegCount = 0;
        sTempIndexSeg = sReuseSeg;  

        while(1)
        {      
            sPQueue.dequeue( (void*) &sCmpSeg, &sUnderflow );

            if ( sUnderflow == ID_TRUE )
            {
                break;
            }

            sReuseSeg[sReuseSegCount++] = sCmpSeg;
            
            if ( sCmpSeg->mStart.mX > sCurrSeg->mEnd.mX )
            {
                /* �� �� ���� ���� ���뿡 �־�� �Ѵ�. */
                break;                
            }
            
            do
            {
                /*
                  ���⼭ intersect�� ���⿡ ���� ������ �־� �ĳ��� �ִ�.                  
                 */
                if ( ( sCurrNext != sCmpSeg ) && ( sCurrPrev != sCmpSeg ) )
                {
                    IDE_TEST( stdUtils::intersectCCW4Func( sCurrSeg->mStart,
                                                           sCurrSeg->mEnd,
                                                           sCmpSeg->mStart,
                                                           sCmpSeg->mEnd,
                                                           &sIntersectStatus,
                                                           &sInterCount,
                                                           sInterResult)
                              != IDE_SUCCESS );

                    switch( sIntersectStatus )
                    {
                        case ST_POINT_TOUCH:
                        case ST_TOUCH:
                        case ST_INTERSECT:

                            IDE_TEST( stdUtils::addPrimInterSeg( aQmxMem,
                                                                 &sPrimInterSeg,
                                                                 sCurrSeg,
                                                                 sCmpSeg,
                                                                 sIntersectStatus,
                                                                 sInterCount,
                                                                 sInterResult)
                                      != IDE_SUCCESS);                                

                            break;

                        case ST_SHARE:

                            IDE_TEST( stdUtils::addPrimInterSeg( aQmxMem,
                                                                 &sPrimInterSeg,
                                                                 sCurrSeg,
                                                                 sCmpSeg,
                                                                 sIntersectStatus,
                                                                 1,
                                                                 &sInterResult[0])
                                      != IDE_SUCCESS);

                            IDE_TEST( stdUtils::addPrimInterSeg( aQmxMem,
                                                                 &sPrimInterSeg,
                                                                 sCurrSeg,
                                                                 sCmpSeg,
                                                                 sIntersectStatus,
                                                                 1,
                                                                 &sInterResult[1])
                                      != IDE_SUCCESS);
                          
                            break;

                        case ST_NOT_INTERSECT:
                            break;                        
                    }

                }
                else
                {
                    // Nothing to do
                }

                sCmpSeg = sCmpSeg->mNext;

                if ( sCmpSeg == NULL )
                {
                    break;                    
                }
            
                /* ������ �����Ѵ�. */

            }while( sCmpSeg->mStart.mX <= sCurrSeg->mEnd.mX );
        }
        
        /* ������ ���� �Ѵ�. */
        
        for ( i =0; i < sReuseSegCount ; i++)
        {
            sPQueue.enqueue( sTempIndexSeg++, &sOverflow);
            IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
            /* Overflow �˻� */
        }

        if ( sCurrSeg->mNext != NULL )
        {
            sPQueue.enqueue( &(sCurrSeg->mNext), &sOverflow);
            IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
        }
    }

    /* BUG-33436
     * sIndexSeg�� x��ǥ ������ ���ĵǾ� �־�� �Ѵ�. */
    iduHeapSort::sort( sIndexSeg, sIndexSegTotal, 
                       ID_SIZEOF(Segment*), cmpSegment );

    IDE_TEST( stdUtils::reassign( aQmxMem, sPrimInterSeg, ID_TRUE )
              != IDE_SUCCESS);

    /* BUG-33634
     * �� ���� ������ üũ�ؼ� �߸��� ���� �����Ѵ�. */
    stdPolyClip::adjustRingOrientation( sRingSegList,
                                        sRingCount,
                                        sIndexSeg,
                                        sIndexSegTotal,
                                        sMax1,
                                        sMax2 );

    IDE_TEST( stdPolyClip::labelingSegment( sRingSegList, 
                                            sRingCount, 
                                            sIndexSeg, 
                                            sIndexSegTotal,
                                            sMax1, 
                                            sMax2, 
                                            NULL )
              != IDE_SUCCESS );

    /* BUG-33634 
     * clip �Լ����� sIndexSeg�� ����� ������ �������� �ε����� �����Ѵ�.
     * ������ ������ �Ҿ������ ������ �����ؾ� �Ѵ�. */
    IDE_TEST( stdPolyClip::clip( aQmxMem,
                                 sRingSegList,
                                 sRingCount,
                                 &sResRingList,
                                 sIndexSeg,
                                 &sIndexSegTotal,
                                 sRule[aOpType], 
                                 sMax1,
                                 NULL ) != IDE_SUCCESS);

    /* BUG-33634 
     * clip ��� Empty�� �ƴ� ��쿡�� ������ġ�� �ִ� �� Ȯ���ϰ�,
     * ������ġ�� �ִ� ���� �и��Ѵ�. 
     * �� �� �� ������ ������ �°� �����Ѵ�. */
    if ( sResRingList.mRingCnt > 0 )
    {
        iduHeapSort::sort( sIndexSeg, sIndexSegTotal, 
                ID_SIZEOF(Segment*), cmpSegment );

        IDE_TEST( stdPolyClip::detectSelfTouch( aQmxMem, 
                                                sIndexSeg, 
                                                sIndexSegTotal,
                                                &sResPrimInterSeg) 
                  != IDE_SUCCESS );
       
        if( sResPrimInterSeg != NULL )
        {
            IDE_TEST( stdPolyClip::breakRing( aQmxMem,
                                              &sResRingList,
                                              sResPrimInterSeg ) 
                      != IDE_SUCCESS ); 
        }
        else
        {
            // Nothing to do
        }

        IDE_TEST( stdPolyClip::sortRings( &sResRingList, 
                                           sIndexSeg, 
                                           sIndexSegTotal )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do
    }


    IDE_TEST(stdPolyClip::makeGeometryFromRings( &sResRingList,
                                                  aRet, 
                                                  aFence ) 
                   != IDE_SUCCESS );

    stdUtils::shiftMultiObjToSingleObj(aRet);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_polygon);
    {
        IDE_SET(ideSetErrorCode( stERR_ABORT_INVALID_POLYGON ));
    }

    IDE_EXCEPTION(err_unknown_polygon);
    {
        IDE_SET(ideSetErrorCode( stERR_ABORT_UNKNOWN_POLYGON ));
    }

    IDE_EXCEPTION(err_incompatible_type);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }

    IDE_EXCEPTION( ERR_ABORT_ENQUEUE_ERROR )
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNEXPECTED_ERROR,
                                  "stfAnalysis::getPolygonClipAreaArea2",
                                  "enqueue overflow" ));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
