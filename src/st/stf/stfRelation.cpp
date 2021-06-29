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
 * $Id: stfRelation.cpp 18883 2006-11-14 01:48:40Z sabbra $
 *
 * Description:
 * Geometry ��ü�� Geometry ��ü���� ���� �Լ� ����
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <mtc.h>
#include <mtd.h>
#include <mtdTypes.h>

#include <qc.h>

#include <ste.h>
#include <stdTypes.h>
#include <stdUtils.h>
#include <stfRelation.h>
#include <stdPolyClip.h>

extern mtdModule stdGeometry;

/* Calculate MBR ******************************************************/

SInt stfRelation::isMBRIntersects( mtdValueInfo * aValueInfo1, // from index.
                                   mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if((sMBR1->mMinX > sMBR2->mMaxX) || (sMBR1->mMaxX < sMBR2->mMinX) ||
       (sMBR1->mMinY > sMBR2->mMaxY) || (sMBR1->mMaxY < sMBR2->mMinY) )
    {
        return 0;
    }
    else
    {
        return 1; // TRUE
    }
}

SInt stfRelation::isMBRContains( mtdValueInfo * aValueInfo1, // from index.
                                 mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if((sMBR1->mMinX <= sMBR2->mMinX) && (sMBR1->mMaxX >= sMBR2->mMaxX) &&
       (sMBR1->mMinY <= sMBR2->mMinY) && (sMBR1->mMaxY >= sMBR2->mMaxY)  )
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}

SInt stfRelation::isMBRWithin( mtdValueInfo * aValueInfo1, // from index.
                               mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if((sMBR1->mMinX >= sMBR2->mMinX) && (sMBR1->mMaxX <= sMBR2->mMaxX) &&
       (sMBR1->mMinY >= sMBR2->mMinY) && (sMBR1->mMaxY <= sMBR2->mMaxY) )
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}

SInt stfRelation::isMBREquals( mtdValueInfo * aValueInfo1, // from index.
                               mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if((sMBR1->mMinX == sMBR2->mMinX) && (sMBR1->mMaxX == sMBR2->mMaxX) &&
       (sMBR1->mMinY == sMBR2->mMinY) && (sMBR1->mMaxY == sMBR2->mMaxY) )
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}

// TASK-3171 B-Tree Spatial
SInt stfRelation::compareMBR( mtdValueInfo * aValueInfo1, // from column 
                              mtdValueInfo * aValueInfo2 )// from row
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR2 = (stdMBR*)aValueInfo2->value;
    sMBR1 = &((const stdGeometryHeader*)
              mtd::valueForModule( (smiColumn*)aValueInfo1->column,
                                   aValueInfo1->value,
                                   aValueInfo1->flag,
                                   stdGeometry.staticNull ))->mMbr;

    if (sMBR1->mMinX > sMBR2 ->mMinX )
    {
        return 1;
    }
    else if (sMBR1->mMinX < sMBR2->mMinX)
    {
        return -1;
    }
    else { // sMBR1->minX == mbr2->minx

        if (sMBR1->mMinY > sMBR2->mMinY)
        {
            return 1;
        }
        else if (sMBR1->mMinY <sMBR2->mMinY)
        {
            return -1;
        }
        else { // sMBR1->minY == mbr2->minY
            if (sMBR1->mMaxX > sMBR2->mMaxX)
            {
                return 1;
            }
            else if (sMBR1->mMaxX < sMBR2->mMaxX)
            {
                return -1;
            }
            else // maxx == maxx
            {
                if (sMBR1->mMaxY > sMBR2->mMaxY)
                {
                    return 1;
                }
                else if (sMBR1->mMaxY < sMBR2->mMaxY)
                {
                    return -1;
                }
            }
        }
    }
    return 0; // exactly same two mbr!.
}

SInt stfRelation::compareMBRKey( mtdValueInfo * aValueInfo1, // from index 
                                 mtdValueInfo * aValueInfo2 )// from index 
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;
    if (sMBR1->mMinX > sMBR2 ->mMinX )
    {
        return 1;
    }
    else if (sMBR1->mMinX < sMBR2->mMinX)
    {
        return -1;
    }
    else { // sMBR1->minX == mbr2->minx

        if (sMBR1->mMinY > sMBR2->mMinY)
        {
            return 1;
        }
        else if (sMBR1->mMinY <sMBR2->mMinY)
        {
            return -1;
        }
        else { // sMBR1->minY == mbr2->minY
            if (sMBR1->mMaxX > sMBR2->mMaxX)
            {
                return 1;
            }
            else if (sMBR1->mMaxX < sMBR2->mMaxX)
            {
                return -1;
            }
            else // maxx == maxx
            {
                if (sMBR1->mMaxY > sMBR2->mMaxY)
                {
                    return 1;
                }
                else if (sMBR1->mMaxY < sMBR2->mMaxY)
                {
                    return -1;
                }
            }
        }
    }
    return 0; // exactly same two mbr!.
}

SInt stfRelation::stfMinXLTEMinX( mtdValueInfo * aValueInfo1, // from index.
                                  mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if (sMBR1->mMinX <= sMBR2->mMinX)
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}


SInt stfRelation::stfMinXLTEMaxX( mtdValueInfo * aValueInfo1, // from index.
                                  mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sMBR1;
    const stdMBR* sMBR2;

    sMBR1 = (stdMBR*)aValueInfo1->value;
    sMBR2 = (stdMBR*)aValueInfo2->value;

    if (sMBR1->mMinX <= sMBR2->mMaxX)
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}

SInt stfRelation::stfFilterIntersects( mtdValueInfo * aValueInfo1, // from index.
                                       mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sIdxMBR;
    const stdMBR* sColMBR;

    sIdxMBR = (stdMBR*)aValueInfo1->value;
    sColMBR = (stdMBR*)aValueInfo2->value;

    if ((sIdxMBR->mMinY <= sColMBR->mMaxY) &&
        (sIdxMBR->mMaxX >= sColMBR->mMinX) &&
        (sIdxMBR->mMaxY >= sColMBR->mMinY))
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}

SInt stfRelation::stfFilterContains( mtdValueInfo * aValueInfo1, // from index.
                                     mtdValueInfo * aValueInfo2 )// from key range.
{
    const stdMBR* sIdxMBR;
    const stdMBR* sColMBR;

    sIdxMBR = (stdMBR*)aValueInfo1->value;
    sColMBR = (stdMBR*)aValueInfo2->value;

    if ((sIdxMBR->mMinY <= sColMBR->mMinY) &&
        (sIdxMBR->mMaxX >= sColMBR->mMaxX) &&
        (sIdxMBR->mMaxY >= sColMBR->mMaxY))
    {
        return 1; // TRUE
    }
    else
    {
        return 0;
    }
}



/* Relation Functions **************************************************/

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� ���谡 Equals ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isEquals(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader* sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader* sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType     sIsEquals;
    SChar              sPattern[10];

    qcTemplate       * sQcTmplate;
    iduMemory        * sQmxMem;
    iduMemoryStatus    sQmxMemStatus;
    UInt               sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBREquals( &sValue1->mMbr, &sValue2->mMbr )==ID_TRUE )
        {
            IDE_TEST( matrixEquals(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue1,
                                               (stdGeometryType*)sValue2,
                                               sPattern,
                                               &sIsEquals )
                          != IDE_SUCCESS );
                
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsEquals = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsEquals = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsEquals;
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
 * �� Geometry ��ü�� ���谡 Disjoint ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isDisjoint(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsDisjoint;
    SChar                   sPattern[10];//"FF*FF****"; // Disjoint Metrix

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_TRUE;
    }
    else
    {
        if( stdUtils::isMBRIntersects(&sValue1->mMbr,&sValue2->mMbr)==ID_TRUE )
        {
            IDE_TEST( matrixDisjoint(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue1,
                                               (stdGeometryType*)sValue2,
                                               sPattern,
                                               &sIsDisjoint )
                          != IDE_SUCCESS );
            
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsDisjoint = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsDisjoint = MTD_BOOLEAN_TRUE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsDisjoint;

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
 * �� Geometry ��ü�� ���谡 Intersects ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isIntersects(
                        mtcNode*     aNode,
                        mtcStack*    aStack,
                        SInt         aRemain,
                        void*        aInfo,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsIntersects;

    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        IDE_TEST( isDisjoint( aNode,
                              aStack,
                              aRemain,
                              aInfo,
                              aTemplate ) != IDE_SUCCESS );

        if( *(mtdBooleanType*)aStack[0].value == MTD_BOOLEAN_TRUE )
        {
            sIsIntersects = MTD_BOOLEAN_FALSE;
        }
        else
        {
            sIsIntersects = MTD_BOOLEAN_TRUE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsIntersects;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� ���谡 Within ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isWithin(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsWithin;
    SChar                   sPattern[10];//="T*F**F***";

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBRWithin( &sValue1->mMbr, &sValue2->mMbr )==ID_TRUE )
        {
            IDE_TEST( matrixWithin(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue1,
                                               (stdGeometryType*)sValue2,
                                               sPattern,
                                               &sIsWithin )
                          != IDE_SUCCESS );
            
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsWithin = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsWithin = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsWithin;
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
 * �� Geometry ��ü�� ���谡 Contains ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isContains(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsContains;
    SChar                   sPattern[10];

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBRContains(&sValue1->mMbr,&sValue2->mMbr)==ID_TRUE )
        {
            IDE_TEST( matrixWithin(
                (stdGeometryType*)sValue2,
                (stdGeometryType*)sValue1,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue2,
                                               (stdGeometryType*)sValue1,
                                               sPattern,
                                               &sIsContains )
                          != IDE_SUCCESS );
                
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsContains = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsContains = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsContains;
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
 * �� Geometry ��ü�� ���谡 Crosses ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isCrosses(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsCrosses;
    SChar                   sPattern[10];

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBRIntersects(&sValue1->mMbr,&sValue2->mMbr)==ID_TRUE )
        {
            IDE_TEST( matrixCrosses(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue1,  // Fix BUG-15432
                                               (stdGeometryType*)sValue2,
                                               sPattern,
                                               &sIsCrosses )
                          != IDE_SUCCESS );
                          
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsCrosses = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsCrosses = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsCrosses;
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
 * �� Geometry ��ü�� ���谡 Overlaps ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isOverlaps(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsOverlaps;
    SChar                   sPattern[10];

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBRIntersects(&sValue1->mMbr,&sValue2->mMbr)==ID_TRUE )
        {
            IDE_TEST( matrixOverlaps(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                // Memory ������ ���Ͽ� ���� ��ġ ���
                IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                sStage = 1;

                IDE_TEST( stfRelation::relate( sQmxMem,
                                               (stdGeometryType*)sValue1,
                                               (stdGeometryType*)sValue2,
                                               sPattern,
                                               &sIsOverlaps )
                          != IDE_SUCCESS );
            
                // Memory ������ ���� Memory �̵�
                sStage = 0;
                IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
            }
            else
            {
                sIsOverlaps = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsOverlaps = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsOverlaps;
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
 * �� Geometry ��ü�� ���谡 Touches ���� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isTouches(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1 = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2 = (stdGeometryHeader *)aStack[2].value;
    mtdBooleanType          sIsTouches;
    UInt                    i;
    UInt                    sPatternCnt = 0;
    SChar                   sPattern[3][10]; /*={ "FT*******",
                                                  "F**T*****",
                                                  "F***T****" };*/

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
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        *(mtdBooleanType*) aStack[0].value = MTD_BOOLEAN_FALSE;
    }
    else
    {
        if( stdUtils::isMBRIntersects(&sValue1->mMbr,&sValue2->mMbr)==ID_TRUE )
        {
            IDE_TEST( matrixTouches(
                (stdGeometryType*)sValue1,
                (stdGeometryType*)sValue2,
                sPattern,
                sPatternCnt) != IDE_SUCCESS );
            if( idlOS::strlen((SChar*)sPattern) > 0 )
            {
                sIsTouches = MTD_BOOLEAN_FALSE;
                
                for( i = 0; ((i < sPatternCnt) && (sPattern[i] != 0x00)); i++ )
                {
                    // Memory ������ ���Ͽ� ���� ��ġ ���
                    IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
                    sStage = 1;

                    IDE_TEST( stfRelation::relate( sQmxMem,
                                                   (stdGeometryType*)sValue1,
                                                   (stdGeometryType*)sValue2,
                                                   sPattern[i],
                                                   &sIsTouches )
                              != IDE_SUCCESS );
            
                    // Memory ������ ���� Memory �̵�
                    sStage = 0;
                    IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
                    
                    if( sIsTouches == MTD_BOOLEAN_TRUE )
                    {
                        break;
                    }
                }
            }
            else
            {
                sIsTouches = MTD_BOOLEAN_FALSE;
            }
        }
        else
        {
            sIsTouches = MTD_BOOLEAN_FALSE;
        }

        *(mtdBooleanType*) aStack[0].value = sIsTouches;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sStage == 1)
    {
        (void)sQmxMem->setStatus(&sQmxMemStatus);
    }
    
    return IDE_FAILURE;
}

// BUG-16478
/***********************************************************************
 * Description:
 * Pattern��Ʈ���� ������ �´��� ���� 
 * '*TF012' �θ� �����Ǿ�� �Ѵ�.
 **********************************************************************/
idBool stfRelation::isValidPatternContents( const SChar *aPattern )
{
    SInt          i;
    idBool        sIsValid;
    static  SChar sValidChars[ 7 ] = "*TF012";
    
    sIsValid = ID_TRUE;
    for( i=0; i<9; i++ )
    {
        if( idlOS::strchr( sValidChars, aPattern[ i ] ) == NULL )
        {
            sIsValid = ID_FALSE;
            break;
        }
    }

    return sIsValid;
}

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� ���谡 �־����� DE-9I ��Ʈ������ �´��� �Ǻ�
 *
 * mtcStack*    aStack(InOut):
 **********************************************************************/
IDE_RC stfRelation::isRelate(
                        mtcNode*     /* aNode */,
                        mtcStack*    aStack,
                        SInt         /* aRemain */,
                        void*        /* aInfo */ ,
                        mtcTemplate* aTemplate )
{
    stdGeometryHeader*      sValue1  = (stdGeometryHeader *)aStack[1].value;
    stdGeometryHeader*      sValue2  = (stdGeometryHeader *)aStack[2].value;
    mtdCharType*            sPattern = (mtdCharType *)aStack[3].value;
    extern mtdModule        mtdChar;
    idBool                  sIsFParam2D;
    idBool                  sIsSParam2D;
    SChar*                  sPatternValue;
    mtdBooleanType          sIsRelated;
    SInt                    i;

    qcTemplate      * sQcTmplate;
    iduMemory       * sQmxMem;
    iduMemoryStatus   sQmxMemStatus;
    UInt              sStage = 0;

    sQcTmplate = (qcTemplate*) aTemplate;
    sQmxMem    = QC_QMX_MEM( sQcTmplate->stmt );

    // BUG-16478
    sPatternValue = (SChar*)sPattern->value;
    if( (sPattern->length != 9) ||
        (isValidPatternContents( sPatternValue )==ID_FALSE) )
    {
        IDE_RAISE( err_invalid_pattern );
    }
    
    // Fix BUG-15412 mtdModule.isNull ���
    if( (stdGeometry.isNull( NULL, sValue1 )==ID_TRUE) ||
        (stdGeometry.isNull( NULL, sValue2 )==ID_TRUE) ||
        (mtdChar.isNull( NULL, sPattern )==ID_TRUE) )
    {
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else if( (stdUtils::isEmpty(sValue1) == ID_TRUE) ||
             (stdUtils::isEmpty(sValue2) == ID_TRUE) )
    {
        // BUG-16451
        sIsRelated    = MTD_BOOLEAN_TRUE;
        for( i = 0; i < 9; i++ )
        {
            if( sPatternValue[i] != '*')
            {
                sIsRelated = checkMatch( sPatternValue[i], 'F' );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        *(mtdBooleanType*) aStack[0].value = sIsRelated;
   }
    else
    {
        // fix BUG-16481
        sIsFParam2D = stdUtils::is2DType(sValue1->mType);
        sIsSParam2D = stdUtils::is2DType(sValue2->mType);
        IDE_TEST_RAISE( sIsFParam2D != sIsSParam2D, err_incompatible_mType );

        // Memory ������ ���Ͽ� ���� ��ġ ���
        IDE_TEST( sQmxMem->getStatus(&sQmxMemStatus) != IDE_SUCCESS);
        sStage = 1;

        IDE_TEST( stfRelation::relate( sQmxMem,
                                       (stdGeometryType*)sValue1,
                                       (stdGeometryType*)sValue2,
                                       (SChar*)sPattern->value,
                                       &sIsRelated )
                  != IDE_SUCCESS );
            
        *(mtdBooleanType*) aStack[0].value = sIsRelated;
        
        // Memory ������ ���� Memory �̵�
        sStage = 0;
        IDE_TEST( sQmxMem->setStatus(&sQmxMemStatus) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( sValue1->mType, sType1, &sLen1 );
        stdUtils::getTypeText( sValue2->mType, sType2, &sLen2 );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                "(\"%s\" : \"%s\")",
                sType1, sType2 );

        IDE_SET(ideSetErrorCode(stERR_ABORT_CONVERSION_MODULE_NOT_FOUND,
            sBuffer));
    }
    // BUG-16478
    IDE_EXCEPTION(err_invalid_pattern);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_INVALID_RELATE_PATTERN));
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Equals ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 **********************************************************************/
IDE_RC stfRelation::matrixEquals(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar*              aPattern )
{
    SInt    sDim1, sDim2;

    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 0 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 1 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 2 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 0 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 1 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 2 )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim1 = stdUtils::getDimension(&aGeom1->header);
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            
            if( (sDim1 == 0)&&(sDim2 == 0) )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else if( (sDim1 == 1)&&(sDim2 == 1) )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else if( (sDim1 == 2)&&(sDim2 == 2) )
            {
                idlOS::strcpy( (SChar*)aPattern, "T*F**FFF*");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_match);
    {
        idlOS::strcpy( (SChar*)aPattern, "");
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Disjoint ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 **********************************************************************/
IDE_RC stfRelation::matrixDisjoint(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar*              aPattern )
{

    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F********");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*******");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F**F*****");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*FF****");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F**F*****");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*FF****");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F********");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*******");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F**F*****");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*FF****");
            break;
        default                       :
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "F**F*****");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*FF****");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "FF*FF****");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Within ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 **********************************************************************/
IDE_RC stfRelation::matrixWithin(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar*              aPattern )
{
    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern, "T*F**F***");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_match);
    {
        idlOS::strcpy( (SChar*)aPattern, "");
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Crosses ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 **********************************************************************/
IDE_RC stfRelation::matrixCrosses(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar*              aPattern )
{
    SInt    sDim1, sDim2;
    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"T*T******");
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:  // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( (sDim2 == 1) || (sDim2 == 2) )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T******");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"0********");
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"T*T******");
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:  // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 1 )
            {
                idlOS::strcpy( (SChar*)aPattern,"0********");
            }
            else if( sDim2 == 2 )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T******");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        IDE_RAISE( err_not_match );
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
        sDim1 = stdUtils::getDimension(&aGeom1->header);
        sDim2 = stdUtils::getDimension(&aGeom2->header);
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:  // Fix BUG-15518
            if( ((sDim1 == 0)&&(sDim2 == 1)) ||
                ((sDim1 == 0)&&(sDim2 == 2)) ||
                ((sDim1 == 1)&&(sDim2 == 2)) )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T******");
            }
            else if( (sDim1 == 1)&&(sDim2 == 1) )
            {
                idlOS::strcpy( (SChar*)aPattern,"0********");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_match);
    {
        idlOS::strcpy( (SChar*)aPattern, "");
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Overlaps ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 **********************************************************************/
IDE_RC stfRelation::matrixOverlaps(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar*              aPattern )
{
    SInt    sDim1, sDim2;
    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"T*T***T**");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 0 )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T***T**");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"1*T***T**");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 1 )
            {
                idlOS::strcpy( (SChar*)aPattern,"1*T***T**");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            idlOS::strcpy( (SChar*)aPattern,"T*T***T**");
            break;
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
            sDim2 = stdUtils::getDimension(&aGeom2->header);
            if( sDim2 == 2 )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T***T**");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE: // Fix BUG-15518
        sDim1 = stdUtils::getDimension(&aGeom1->header);
        sDim2 = stdUtils::getDimension(&aGeom2->header);
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:  // Fix BUG-15518
            if( ((sDim1 == 0)&&(sDim2 == 0)) ||
                ((sDim1 == 2)&&(sDim2 == 2)) )
            {
                idlOS::strcpy( (SChar*)aPattern,"T*T***T**");
            }
            else if( (sDim1 == 1)&&(sDim2 == 1) )
            {
                idlOS::strcpy( (SChar*)aPattern,"1*T***T**");
            }
            else
            {
                IDE_RAISE( err_not_match );
            }
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_match);
    {
        idlOS::strcpy( (SChar*)aPattern, "");
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
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
 * �� Geometry ��ü�� Ÿ�Կ� �´� DE-9I Touches ��Ʈ���� ���
 *
 * stdGeometryType*    aGeom1(In): ��ü1
 * stdGeometryType*    aGeom2(In): ��ü2
 * SChar*              aPattern(Out): ���� ��Ʈ������ ��µ� ����
 * UInt&               aPatternCnt: ��Ʈ���� ����
 **********************************************************************/
IDE_RC stfRelation::matrixTouches(
                    stdGeometryType*    aGeom1,
                    stdGeometryType*    aGeom2,
                    SChar               aPattern[3][10],
                    UInt&               aPatternCnt )
{
    switch(aGeom1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            IDE_RAISE( err_not_match );
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            aPatternCnt = 1;
            idlOS::strcpy( (SChar*)aPattern[0],"F0*******");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        switch(aGeom2->header.mType)
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            aPatternCnt = 1;
            idlOS::strcpy( (SChar*)aPattern[0],"F**0*****");
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            aPatternCnt = 3;
            idlOS::strcpy( (SChar*)aPattern[0],"FT*******");
            idlOS::strcpy( (SChar*)aPattern[1],"F**T*****");
            idlOS::strcpy( (SChar*)aPattern[2],"F***T****");
            break;
        default:
            IDE_RAISE( err_incompatible_mType );
        }
        break;
    default:
        IDE_RAISE( err_incompatible_mType );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_not_match);
    {
        idlOS::strcpy( (SChar*)aPattern, "");
        return IDE_SUCCESS;
    }
    IDE_EXCEPTION(err_incompatible_mType);
    {
        // To fix BUG-15300
        SChar   sType1[32];
        SChar   sType2[32];
        SChar   sBuffer[128];
        UShort  sLen1;
        UShort  sLen2;

        stdUtils::getTypeText( aGeom1->header.mType, sType1, &sLen1 );
        stdUtils::getTypeText( aGeom2->header.mType, sType2, &sLen2 );
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                "(\"%s\" : \"%s\")",
                sType1, sType2 );

        IDE_SET(ideSetErrorCode(stERR_ABORT_CONVERSION_MODULE_NOT_FOUND,
            sBuffer));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Calculate Relation *************************************************/

/***********************************************************************
 * Description:
 * �� Geometry ��ü�� aPattern ��Ʈ������ ������ ���踦 ������ �Ǻ�
 * ���谡 �����ϸ� MTD_BOOLEAN_TRUE �ƴϸ� MTD_BOOLEAN_FALSE ����
 *
 * const stdGeometryType* aObj1(In): ��ü1
 * const stdGeometryType* aObj2(In): ��ü2
 * SChar*                 aPattern(In): ���� ��Ʈ����
 **********************************************************************/
IDE_RC stfRelation::relate( iduMemory*             aQmxMem,
                            const stdGeometryType* aObj1,
                            const stdGeometryType* aObj2,
                            SChar*                 aPattern,
                            mtdBooleanType*        aReturn )
{
    SChar          sResult;
    SInt           i;
    mtdBooleanType sIsRelated = MTD_BOOLEAN_TRUE;

    if( (aObj1 == NULL) || (aObj2 == NULL) )
    {
        sIsRelated = MTD_BOOLEAN_FALSE;
        IDE_RAISE( normal_exit );
    }
    
    if( ( stdUtils::getGroup((stdGeometryHeader*)aObj1) == STD_POLYGON_2D_GROUP ) &&
        ( stdUtils::getGroup((stdGeometryHeader*)aObj2) == STD_POLYGON_2D_GROUP ) )
    {
        IDE_TEST( relateAreaArea( aQmxMem, aObj1, aObj2, aPattern, &sIsRelated ) 
                    != IDE_SUCCESS );
        IDE_RAISE( normal_exit );
    }


    switch(aObj1->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                sResult = pointTogeometry( i, aObj1, aObj2 );
                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                sResult = linestringTogeometry( i, aObj1, aObj2 );
                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                IDE_TEST( polygonTogeometry( aQmxMem, i, aObj1, aObj2, &sResult )
                          != IDE_SUCCESS );

                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                sResult = multipointTogeometry( i, aObj1, aObj2 );

                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                sResult = multilinestringTogeometry( i, aObj1, aObj2 );

                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                IDE_TEST( multipolygonTogeometry( aQmxMem, i, aObj1, aObj2, &sResult )
                          != IDE_SUCCESS );

                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        for( i = 0; i < 9; i++ )
        {
            if( aPattern[i] != '*')
            {
                IDE_TEST( collectionTogeometry( aQmxMem, i, aObj1, aObj2, &sResult )
                          != IDE_SUCCESS );
                sIsRelated = checkMatch( aPattern[i], sResult );
                if( sIsRelated == MTD_BOOLEAN_FALSE )
                {
                    break;
                }
            }
        }
    default:
        break;
    }

    IDE_EXCEPTION_CONT( normal_exit );

    *aReturn = sIsRelated;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Calculate Relation *************************************************/

/***********************************************************************
 * Description:
 * ����Ʈ ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
SChar stfRelation::pointTogeometry( SInt                   aIndex,
                                    const stdGeometryType* aObj1,
                                    const stdGeometryType* aObj2 )
{
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTospi(&aObj1->point2D.mPoint, &aObj2->point2D.mPoint);
                case 2: return spiTospe(&aObj1->point2D.mPoint, &aObj2->point2D.mPoint);
                case 6: return spiTospe(&aObj2->point2D.mPoint, &aObj1->point2D.mPoint);
                case 8: return '2';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTosli(&aObj1->point2D.mPoint, &aObj2->linestring2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return spiToslb(&aObj1->point2D.mPoint, &aObj2->linestring2D);
                    }
                case 2: return spiTosle(&aObj1->point2D.mPoint, &aObj2->linestring2D);
                case 6: return '1';
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return '0';
                    }
                case 8: return '2';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTosai(&aObj1->point2D.mPoint, &aObj2->polygon2D);
                case 1: return spiTosab(&aObj1->point2D.mPoint, &aObj2->polygon2D);
                case 2: return spiTosae(&aObj1->point2D.mPoint, &aObj2->polygon2D);
                case 6: return '2';
                case 7: return '1';
                case 8: return '2';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTompi(&aObj1->point2D.mPoint, &aObj2->mpoint2D);
                case 2: return spiTompe(&aObj1->point2D.mPoint, &aObj2->mpoint2D);
                case 6: return speTompi(&aObj1->point2D.mPoint, &aObj2->mpoint2D);
                case 8: return '2';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTomli(&aObj1->point2D.mPoint, &aObj2->mlinestring2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return spiTomlb(&aObj1->point2D.mPoint, &aObj2->mlinestring2D);
                    }
                case 2: return spiTomle(&aObj1->point2D.mPoint, &aObj2->mlinestring2D);
                case 6: return '1';
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {        
                        return '0';
                    }
                case 8: return '2';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTomai(&aObj1->point2D.mPoint, &aObj2->mpolygon2D);
                case 1: return spiTomab(&aObj1->point2D.mPoint, &aObj2->mpolygon2D);
                case 2: return spiTomae(&aObj1->point2D.mPoint, &aObj2->mpolygon2D);
                case 6: return '2';
                case 7: return '1';
                case 8: return '2';
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTogci(&aObj1->point2D.mPoint, &aObj2->collection2D);
                case 1: return spiTogcb(&aObj1->point2D.mPoint, &aObj2->collection2D);
                case 2: return spiTogce(&aObj1->point2D.mPoint, &aObj2->collection2D);
                case 6: return speTogci(&aObj1->point2D.mPoint, &aObj2->collection2D);
                case 7: return speTogcb(&aObj1->point2D.mPoint, &aObj2->collection2D);
                case 8: return '2';
            }
            break;
    }
    return 'F';
}

/***********************************************************************
 * Description:
 * ���� ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
SChar stfRelation::linestringTogeometry( SInt                   aIndex,
                                         const stdGeometryType* aObj1,
                                         const stdGeometryType* aObj2 )
{
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTosli(&aObj2->point2D.mPoint, &aObj1->linestring2D);
                case 2: return '1';
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return spiToslb(&aObj2->point2D.mPoint, &aObj1->linestring2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return '0';
                    }
                case 6: return spiTosle(&aObj2->point2D.mPoint, &aObj1->linestring2D);
                case 8: return '2';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTosli(&aObj1->linestring2D, &aObj2->linestring2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return sliToslb(&aObj1->linestring2D, &aObj2->linestring2D);
                    }
                case 2: return sliTosle(&aObj1->linestring2D, &aObj2->linestring2D);
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return sliToslb(&aObj2->linestring2D, &aObj1->linestring2D);
                    }
                case 4: 
                    // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) ||
                        stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbToslb(&aObj1->linestring2D, &aObj2->linestring2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTosle(&aObj1->linestring2D, &aObj2->linestring2D);
                    }
                case 6: return sliTosle(&aObj2->linestring2D, &aObj1->linestring2D);
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTosle(&aObj2->linestring2D, &aObj1->linestring2D);
                    }
                case 8: return '2';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTosai(&aObj1->linestring2D, &aObj2->polygon2D);
                case 1: return sliTosab(&aObj1->linestring2D, &aObj2->polygon2D);
                case 2: return sliTosae(&aObj1->linestring2D, &aObj2->polygon2D);
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTosai(&aObj1->linestring2D, &aObj2->polygon2D);
                    }
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTosab(&aObj1->linestring2D, &aObj2->polygon2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTosae(&aObj1->linestring2D, &aObj2->polygon2D);
                    }
                case 6: return '2';
                case 7: return sleTosab(&aObj1->linestring2D, &aObj2->polygon2D);
                case 8: return '2';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTompi(&aObj1->linestring2D, &aObj2->mpoint2D);
                case 2: return '1';
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTompi(&aObj1->linestring2D, &aObj2->mpoint2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTompe(&aObj1->linestring2D, &aObj2->mpoint2D);
                    }
                case 6: return sleTompi(&aObj1->linestring2D, &aObj2->mpoint2D);
                case 8: return '2';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTomli(&aObj1->linestring2D, &aObj2->mlinestring2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return sliTomlb(&aObj1->linestring2D, &aObj2->mlinestring2D);
                    }
                case 2: return sliTomle(&aObj1->linestring2D, &aObj2->mlinestring2D);
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomli(&aObj1->linestring2D, &aObj2->mlinestring2D);
                    }
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) ||
                        stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomlb(&aObj1->linestring2D, &aObj2->mlinestring2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomle(&aObj1->linestring2D, &aObj2->mlinestring2D);
                    }
                case 6: return sleTomli(&aObj1->linestring2D, &aObj2->mlinestring2D);
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return sleTomlb(&aObj1->linestring2D, &aObj2->mlinestring2D);
                    }
                case 8: return '2';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTomai(&aObj1->linestring2D, &aObj2->mpolygon2D);
                case 1: return sliTomab(&aObj1->linestring2D, &aObj2->mpolygon2D);
                case 2: return sliTomae(&aObj1->linestring2D, &aObj2->mpolygon2D);
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomai(&aObj1->linestring2D, &aObj2->mpolygon2D);
                    }
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomab(&aObj1->linestring2D, &aObj2->mpolygon2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTomae(&aObj1->linestring2D, &aObj2->mpolygon2D);
                    }
                case 6: return '2';
                case 7: return sleTomab(&aObj1->linestring2D, &aObj2->mpolygon2D);
                case 8: return '2';
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTogci(&aObj1->linestring2D, &aObj2->collection2D);
                case 1: return sliTogcb(&aObj1->linestring2D, &aObj2->collection2D);
                case 2: return sliTogce(&aObj1->linestring2D, &aObj2->collection2D);
                case 3: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTogci(&aObj1->linestring2D, &aObj2->collection2D);
                    }
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTogcb(&aObj1->linestring2D, &aObj2->collection2D);
                    }
                case 5: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTogce(&aObj1->linestring2D, &aObj2->collection2D);
                    }
                case 6: return sleTogci(&aObj1->linestring2D, &aObj2->collection2D);
                case 7: return sleTogcb(&aObj1->linestring2D, &aObj2->collection2D);
                case 8: return '2';
            }
            break;
    }
    return 'F';
}

/***********************************************************************
 * Description:
 * ������ ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
IDE_RC stfRelation::polygonTogeometry( iduMemory*             aQmxMem,
                                       SInt                   aIndex,
                                       const stdGeometryType* aObj1,
                                       const stdGeometryType* aObj2,
                                       SChar*                 aResult )
{
    SChar sResult;
    
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = spiTosai(&aObj2->point2D.mPoint, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = spiTosab(&aObj2->point2D.mPoint, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = '1';
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = spiTosae(&aObj2->point2D.mPoint, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = sliTosai(&aObj2->linestring2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTosai(&aObj2->linestring2D, &aObj1->polygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sliTosab(&aObj2->linestring2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTosab(&aObj2->linestring2D, &aObj1->polygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = sleTosab(&aObj2->linestring2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = sliTosae(&aObj2->linestring2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTosae(&aObj2->linestring2D, &aObj1->polygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( saiTosai(aQmxMem, &aObj1->polygon2D, &aObj2->polygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = saiTosab(&aObj1->polygon2D, &aObj2->polygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( saiTosae(aQmxMem, &aObj1->polygon2D, &aObj2->polygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = saiTosab(&aObj2->polygon2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = sabTosab(&aObj1->polygon2D, &aObj2->polygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = sabTosae(&aObj1->polygon2D, &aObj2->polygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( saiTosae(aQmxMem, &aObj2->polygon2D, &aObj1->polygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = sabTosae(&aObj2->polygon2D, &aObj1->polygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = saiTompi(&aObj1->polygon2D, &aObj2->mpoint2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sabTompi(&aObj1->polygon2D, &aObj2->mpoint2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = '1';
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = saeTompi(&aObj1->polygon2D, &aObj2->mpoint2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = saiTomli(&aObj1->polygon2D, &aObj2->mlinestring2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = saiTomlb(&aObj1->polygon2D, &aObj2->mlinestring2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sabTomli(&aObj1->polygon2D, &aObj2->mlinestring2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = sabTomlb(&aObj1->polygon2D, &aObj2->mlinestring2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = sabTomle(&aObj1->polygon2D, &aObj2->mlinestring2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = saeTomli(&aObj1->polygon2D, &aObj2->mlinestring2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = saeTomlb(&aObj1->polygon2D, &aObj2->mlinestring2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( saiTomai(aQmxMem, &aObj1->polygon2D, &aObj2->mpolygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = saiTomab(&aObj1->polygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( saiTomae(aQmxMem, &aObj1->polygon2D, &aObj2->mpolygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sabTomai(&aObj1->polygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = sabTomab(&aObj1->polygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = sabTomae(&aObj1->polygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( saeTomai(aQmxMem, &aObj1->polygon2D, &aObj2->mpolygon2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = saeTomab(&aObj1->polygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( saiTogci(aQmxMem, &aObj1->polygon2D, &aObj2->collection2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = saiTogcb(&aObj1->polygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( saiTogce(aQmxMem, &aObj1->polygon2D, &aObj2->collection2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sabTogci(&aObj1->polygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = sabTogcb(&aObj1->polygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = sabTogce(&aObj1->polygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( saeTogci(aQmxMem, &aObj1->polygon2D, &aObj2->collection2D, &sResult )
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = saeTogcb(&aObj1->polygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
    }

    sResult = 'F';

    IDE_EXCEPTION_CONT( normal_exit );

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * ��Ƽ����Ʈ ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
SChar stfRelation::multipointTogeometry( SInt                   aIndex,
                                         const stdGeometryType* aObj1,
                                         const stdGeometryType* aObj2 )
{
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return spiTompi(&aObj2->point2D.mPoint, &aObj1->mpoint2D);
                case 2: return speTompi(&aObj2->point2D.mPoint, &aObj1->mpoint2D);
                case 6: return spiTompe(&aObj2->point2D.mPoint, &aObj1->mpoint2D);
                case 8: return '2';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return sliTompi(&aObj2->linestring2D, &aObj1->mpoint2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTompi(&aObj2->linestring2D, &aObj1->mpoint2D);
                    }
                case 2: return sleTompi(&aObj2->linestring2D, &aObj1->mpoint2D);
                case 6: return '1';
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return slbTompe(&aObj2->linestring2D, &aObj1->mpoint2D);
                    }
                case 8: return '2';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return saiTompi(&aObj2->polygon2D, &aObj1->mpoint2D);
                case 1: return sabTompi(&aObj2->polygon2D, &aObj1->mpoint2D);
                case 2: return saeTompi(&aObj2->polygon2D, &aObj1->mpoint2D);
                case 6: return '2';
                case 7: return '1';
                case 8: return '2';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return mpiTompi(&aObj1->mpoint2D, &aObj2->mpoint2D);
                case 2: return mpiTompe(&aObj1->mpoint2D, &aObj2->mpoint2D);
                case 6: return mpiTompe(&aObj2->mpoint2D, &aObj1->mpoint2D);
                case 8: return '2';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return mpiTomli(&aObj1->mpoint2D, &aObj2->mlinestring2D);
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return mpiTomlb(&aObj1->mpoint2D, &aObj2->mlinestring2D);
                    }
                case 2: return mpiTomle(&aObj1->mpoint2D, &aObj2->mlinestring2D);
                case 6: return '1';
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        return mpeTomlb(&aObj1->mpoint2D, &aObj2->mlinestring2D);
                    }
                case 8: return '2';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return mpiTomai(&aObj1->mpoint2D, &aObj2->mpolygon2D);
                case 1: return mpiTomab(&aObj1->mpoint2D, &aObj2->mpolygon2D);
                case 2: return mpiTomae(&aObj1->mpoint2D, &aObj2->mpolygon2D);
                case 6: return '2';
                case 7: return '1';
                case 8: return '2';
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0: return mpiTogci(&aObj1->mpoint2D, &aObj2->collection2D);
                case 1: return mpiTogcb(&aObj1->mpoint2D, &aObj2->collection2D);
                case 2: return mpiTogce(&aObj1->mpoint2D, &aObj2->collection2D);
                case 6: return mpeTogci(&aObj1->mpoint2D, &aObj2->collection2D);
                case 7: return mpeTogcb(&aObj1->mpoint2D, &aObj2->collection2D);
                case 8: return '2';
            }
            break;
    }
    return 'F';
}

/***********************************************************************
 * Description:
 * ��Ƽ ���� ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
SChar stfRelation::multilinestringTogeometry( SInt                   aIndex,
                                              const stdGeometryType* aObj1,
                                              const stdGeometryType* aObj2 )
{
    switch(aObj2->header.mType)
    {
    case STD_POINT_2D_TYPE:
    case STD_POINT_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return spiTomli(&aObj2->point2D.mPoint, &aObj1->mlinestring2D);
        case 2: return '1';
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return spiTomlb(&aObj2->point2D.mPoint, &aObj1->mlinestring2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return '0';
            }
        case 6: return spiTomle(&aObj2->point2D.mPoint, &aObj1->mlinestring2D);
        case 8: return '2';
        }
        break;
    case STD_LINESTRING_2D_TYPE:
    case STD_LINESTRING_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return sliTomli(&aObj2->linestring2D, &aObj1->mlinestring2D);
        case 1: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return slbTomli(&aObj2->linestring2D, &aObj1->mlinestring2D);
            }
        case 2: return sleTomli(&aObj2->linestring2D, &aObj1->mlinestring2D);
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return sliTomlb(&aObj2->linestring2D, &aObj1->mlinestring2D);
            }
        case 4: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) ||
                stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return slbTomlb(&aObj2->linestring2D, &aObj1->mlinestring2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return sleTomlb(&aObj2->linestring2D, &aObj1->mlinestring2D);
            }
        case 6: return sliTomle(&aObj2->linestring2D, &aObj1->mlinestring2D);
        case 7: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return slbTomle(&aObj2->linestring2D, &aObj1->mlinestring2D);
            }
        case 8: return '2';
        }
        break;
    case STD_POLYGON_2D_TYPE:
    case STD_POLYGON_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return saiTomli(&aObj2->polygon2D, &aObj1->mlinestring2D);
        case 1: return sabTomli(&aObj2->polygon2D, &aObj1->mlinestring2D);
        case 2: return saeTomli(&aObj2->polygon2D, &aObj1->mlinestring2D);
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return saiTomlb(&aObj2->polygon2D, &aObj1->mlinestring2D);
            }
        case 4: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return sabTomlb(&aObj2->polygon2D, &aObj1->mlinestring2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return saeTomlb(&aObj2->polygon2D, &aObj1->mlinestring2D);
            }
        case 6: return '2';
        case 7: return sabTomle(&aObj2->polygon2D, &aObj1->mlinestring2D);
        case 8: return '2';
        }
        break;
    case STD_MULTIPOINT_2D_TYPE:
    case STD_MULTIPOINT_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return mpiTomli(&aObj2->mpoint2D, &aObj1->mlinestring2D);
        case 2: return '1';
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mpiTomlb(&aObj2->mpoint2D, &aObj1->mlinestring2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mpeTomlb(&aObj2->mpoint2D, &aObj1->mlinestring2D);
            }
        case 6: return mpiTomle(&aObj2->mpoint2D, &aObj1->mlinestring2D);
        case 8: return '2';
        }
        break;
    case STD_MULTILINESTRING_2D_TYPE:
    case STD_MULTILINESTRING_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return mliTomli(&aObj1->mlinestring2D, &aObj2->mlinestring2D);
        case 1: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return mliTomlb(&aObj1->mlinestring2D, &aObj2->mlinestring2D);
            }
        case 2: return mliTomle(&aObj1->mlinestring2D, &aObj2->mlinestring2D);
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mliTomlb(&aObj2->mlinestring2D, &aObj1->mlinestring2D);
            }
        case 4: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) ||
                stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return mlbTomlb(&aObj1->mlinestring2D, &aObj2->mlinestring2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTomle(&aObj1->mlinestring2D, &aObj2->mlinestring2D);
            }
        case 6: return mliTomle(&aObj2->mlinestring2D, &aObj1->mlinestring2D);
        case 7: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
            {
                break;
            }
            else
            {
                return mlbTomle(&aObj2->mlinestring2D, &aObj1->mlinestring2D);
            }
        case 8: return '2';
        }
        break;
    case STD_MULTIPOLYGON_2D_TYPE:
    case STD_MULTIPOLYGON_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return mliTomai(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
        case 1: return mliTomab(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
        case 2: return mliTomae(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTomai(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
            }
        case 4: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTomab(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTomae(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
            }
        case 6: return '2';
        case 7: return mleTomab(&aObj1->mlinestring2D, &aObj2->mpolygon2D);
        case 8: return '2';
        }
        break;
    case STD_GEOCOLLECTION_2D_TYPE:
    case STD_GEOCOLLECTION_2D_EXT_TYPE:
        switch(aIndex)
        {
        case 0: return mliTogci(&aObj1->mlinestring2D, &aObj2->collection2D);
        case 1: return mliTogcb(&aObj1->mlinestring2D, &aObj2->collection2D);
        case 2: return mliTogce(&aObj1->mlinestring2D, &aObj2->collection2D);
        case 3: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTogci(&aObj1->mlinestring2D, &aObj2->collection2D);
            }
        case 4: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTogcb(&aObj1->mlinestring2D, &aObj2->collection2D);
            }
        case 5: // Fix BUG-15923
            if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj1 ) )
            {
                break;
            }
            else
            {
                return mlbTogce(&aObj1->mlinestring2D, &aObj2->collection2D);
            }
        case 6: return mleTogci(&aObj1->mlinestring2D, &aObj2->collection2D);
        case 7: return mleTogcb(&aObj1->mlinestring2D, &aObj2->collection2D);
        case 8: return '2';
        }
        break;
    }

    return 'F';
}

/***********************************************************************
 * Description:
 * ��Ƽ������ ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
IDE_RC stfRelation::multipolygonTogeometry( iduMemory*             aQmxMem,
                                            SInt                   aIndex,
                                            const stdGeometryType* aObj1,
                                            const stdGeometryType* aObj2,
                                            SChar*                 aResult )
{
    SChar sResult;
    
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = spiTomai(&aObj2->point2D.mPoint, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = spiTomab(&aObj2->point2D.mPoint, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = '1';
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = spiTomae(&aObj2->point2D.mPoint, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = sliTomai(&aObj2->linestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTomai(&aObj2->linestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sliTomab(&aObj2->linestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTomab(&aObj2->linestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = sleTomab(&aObj2->linestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = sliTomae(&aObj2->linestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTomae(&aObj2->linestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( saiTomai(aQmxMem, &aObj2->polygon2D, &aObj1->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = sabTomai(&aObj2->polygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( saeTomai(aQmxMem, &aObj2->polygon2D, &aObj1->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = saiTomab(&aObj2->polygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = sabTomab(&aObj2->polygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = saeTomab(&aObj2->polygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( saiTomae(aQmxMem, &aObj2->polygon2D, &aObj1->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = sabTomae(&aObj2->polygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = mpiTomai(&aObj2->mpoint2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = mpiTomab(&aObj2->mpoint2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = '1';
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = mpiTomae(&aObj2->mpoint2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = mliTomai(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTomai(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = mliTomab(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTomab(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = mleTomab(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = mliTomae(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTomae(&aObj2->mlinestring2D, &aObj1->mpolygon2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( maiTomai(aQmxMem, &aObj1->mpolygon2D, &aObj2->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = maiTomab(&aObj1->mpolygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( maiTomae(aQmxMem, &aObj1->mpolygon2D, &aObj2->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = maiTomab(&aObj2->mpolygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = mabTomab(&aObj1->mpolygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = mabTomae(&aObj1->mpolygon2D, &aObj2->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( maiTomae(aQmxMem, &aObj2->mpolygon2D, &aObj1->mpolygon2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = mabTomae(&aObj2->mpolygon2D, &aObj1->mpolygon2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( maiTogci(aQmxMem, &aObj1->mpolygon2D, &aObj2->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = maiTogcb(&aObj1->mpolygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( maiTogce(aQmxMem, &aObj1->mpolygon2D, &aObj2->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = mabTogci(&aObj1->mpolygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = mabTogcb(&aObj1->mpolygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = mabTogce(&aObj1->mpolygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( maeTogci(aQmxMem, &aObj1->mpolygon2D, &aObj2->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = maeTogcb(&aObj1->mpolygon2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
    }

    sResult = 'F';
    
    IDE_EXCEPTION_CONT( normal_exit );

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * �ݷ��� ��ü�� �ٸ� ��ü�� ��Ʈ���� �ε����� �ش��ϴ� ���踦 ����
 * ��ġ�� ������ ������ �����Ѵ�.
 * '0' ��
 * '1' ��
 * '2' ��
 * 'F' �������� �ʴ´�.
 *
 * SInt                  aIndex(In): ��Ʈ���� �ε��� ��ȣ
 * const stdGeometryType* aObj1(In): ��ü
 * const stdGeometryType* aObj2(In): �񱳵� ��ü
 **********************************************************************/
IDE_RC stfRelation::collectionTogeometry( iduMemory*             aQmxMem,
                                          SInt                   aIndex,
                                          const stdGeometryType* aObj1,
                                          const stdGeometryType* aObj2,
                                          SChar*                 aResult )
{
    SChar sResult;
    
    switch(aObj2->header.mType)
    {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = spiTogci(&aObj2->point2D.mPoint, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = speTogci(&aObj2->point2D.mPoint, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = spiTogcb(&aObj2->point2D.mPoint, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = speTogcb(&aObj2->point2D.mPoint, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = spiTogce(&aObj2->point2D.mPoint, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = sliTogci(&aObj2->linestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTogci(&aObj2->linestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = sleTogci(&aObj2->linestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = sliTogcb(&aObj2->linestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTogcb(&aObj2->linestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = sleTogcb(&aObj2->linestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = sliTogce(&aObj2->linestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = slbTogce(&aObj2->linestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( saiTogci(aQmxMem, &aObj2->polygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = sabTogci(&aObj2->polygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( saeTogci(aQmxMem, &aObj2->polygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = saiTogcb(&aObj2->polygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = sabTogcb(&aObj2->polygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = saeTogcb(&aObj2->polygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( saiTogce(aQmxMem, &aObj2->polygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = sabTogce(&aObj2->polygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = mpiTogci(&aObj2->mpoint2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    sResult = mpeTogci(&aObj2->mpoint2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = mpiTogcb(&aObj2->mpoint2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = mpeTogcb(&aObj2->mpoint2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = mpiTogce(&aObj2->mpoint2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    sResult = mliTogci(&aObj2->mlinestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 1: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTogci(&aObj2->mlinestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 2:
                    sResult = mleTogci(&aObj2->mlinestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = mliTogcb(&aObj2->mlinestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 4: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTogcb(&aObj2->mlinestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 5:
                    sResult = mleTogcb(&aObj2->mlinestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    sResult = mliTogce(&aObj2->mlinestring2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 7: // Fix BUG-15923
                    if( stdUtils::isClosed2D( (stdGeometryHeader*)aObj2 ) )
                    {
                        break;
                    }
                    else
                    {
                        sResult = mlbTogce(&aObj2->mlinestring2D, &aObj1->collection2D);
                        IDE_RAISE( normal_exit );
                    }
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( maiTogci(aQmxMem, &aObj2->mpolygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = mabTogci(&aObj2->mpolygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( maeTogci(aQmxMem, &aObj2->mpolygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = maiTogcb(&aObj2->mpolygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = mabTogcb(&aObj2->mpolygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = maeTogcb(&aObj2->mpolygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( maiTogce(aQmxMem, &aObj2->mpolygon2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = mabTogce(&aObj2->mpolygon2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
        case STD_GEOCOLLECTION_2D_TYPE:
        case STD_GEOCOLLECTION_2D_EXT_TYPE:
            switch(aIndex)
            {
                case 0:
                    IDE_TEST( gciTogci(aQmxMem, &aObj1->collection2D, &aObj2->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 1:
                    sResult = gciTogcb(&aObj1->collection2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 2:
                    IDE_TEST( gciTogce(aQmxMem, &aObj1->collection2D, &aObj2->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 3:
                    sResult = gciTogcb(&aObj2->collection2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 4:
                    sResult = gcbTogcb(&aObj1->collection2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 5:
                    sResult = gcbTogce(&aObj1->collection2D, &aObj2->collection2D);
                    IDE_RAISE( normal_exit );
                case 6:
                    IDE_TEST( gciTogce(aQmxMem, &aObj2->collection2D, &aObj1->collection2D, &sResult)
                              != IDE_SUCCESS );
                    IDE_RAISE( normal_exit );
                case 7:
                    sResult = gcbTogce(&aObj2->collection2D, &aObj1->collection2D);
                    IDE_RAISE( normal_exit );
                case 8:
                    sResult = '2';
                    IDE_RAISE( normal_exit );
            }
            break;
    }
    
    sResult = 'F';
    
    IDE_EXCEPTION_CONT( normal_exit );

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 * �� ��Ʈ������ ���谡 ��ġ�ϴ��� �Ǻ�
 * '0' '1' '2'�� ���� ���� 'T'�� ���� ��ġ�Ѵ�.
 * ���谡 ��ġ�ϸ� MTD_BOOLEAN_TRUE �ƴϸ� MTD_BOOLEAN_FALSE ����
 *
 * SChar aPM(In): ��Ʈ����1
 * SChar aResult(In): ��Ʈ����2
 **********************************************************************/
mtdBooleanType stfRelation::checkMatch( SChar aPM, SChar aResult )
{
    // aResult�� 0,1,2,F�� �����Ѵ�.
    switch( aPM )
    {
        case '0':
            switch( aResult )
            {
                case '0':
                case 'T': return MTD_BOOLEAN_TRUE;
            }
            break;
        case '1':
            switch( aResult )
            {
                case '1':
                case 'T': return MTD_BOOLEAN_TRUE;
            }
            break;
        case '2':
            switch( aResult )
            {
                case '2':
                case 'T': return MTD_BOOLEAN_TRUE;
            }
            break;
        case 'T':
            switch( aResult )
            {
                case '0':
                case '1':
                case '2':
                case 'T': return MTD_BOOLEAN_TRUE;
            }
            break;
        case 'F':
            switch( aResult )
            {
                case 'F': return MTD_BOOLEAN_TRUE;
            }
            break;
    }
    return MTD_BOOLEAN_FALSE;
}

/***********************************************************************
 * Description:
 *
 *    ����(Line Segment)���� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *    ������ ���� ���а� ����Ǵ� ������ ������ ����� DE-9IM ���踦 ���Ѵ�.
 *       - ���� ���а� ������ ����
 *       - ���� ���� ������ ����
 *       - ������ ���� ������������ ����
 *
 *    ���а� ������ ���� ������ ���� ���� ����� ������ ����.
 *
 *           S-----------S : Segment 1
 *           T-----------T : Segment 2
 *
 *    �� 4���� ���а� ������ ���
 *
 *         ST=======ST
 *
 *         ; ������ ���� ('1')
 *
 *    �� 3���� ���а� ������ ���
 *
 *         ST=======S-------T
 *
 *         ; ������ ���� ('1')
 *
 *    �� 2���� ���а� ������ ���
 *
 *         S-----T=====T-----S
 *
 *         ; ������ ������ ��찡 ���� ('1')
 *
 *         S-----T=====S----T
 *
 *         ; ������ ������ ��찡 ���� ('1')
 *
 *         S
 *         |
 *         |
 *         ST-------T
 *
 *         ; ������ ����, ������('F')���� ������('0')���� �Ǵ��ؾ� ��.
 *
 *    �� 1���� ���а� ������ ���
 *
 *               S
 *               |
 *               |
 *         T-----S-----T
 *
 *         ; ������('F')���� ������('0')���� �Ǵ��ؾ� ��.
 *
 *    �� 0���� ���а� ������ ���
 *
 *         S-----S     T-----T
 *
 *
 *               S
 *               |
 *               |
 *          T----+-----T
 *               |
 *               |
 *               S 
 *
 **********************************************************************/

SChar
stfRelation::compLineSegment( stdPoint2D * aSeg1Pt1,         // LineSeg 1
                              stdPoint2D * aSeg1Pt2,         // LineSeg 1
                              idBool       aIsTermSeg1Pt1,   // is terminal
                              idBool       aIsTermSeg1Pt2,   // is terminal
                              stdPoint2D * aSeg2Pt1,         // LineSeg 2
                              stdPoint2D * aSeg2Pt2,         // LineSeg 2 
                              idBool       aIsTermSeg2Pt1,   // is terminal
                              idBool       aIsTermSeg2Pt2 )  // is terminal
{
    SInt    sMeetCnt;     // ���а� ������ ���� ����
    SInt    sOnPointCnt;  // ���� ���� ��ġ�ϴ� ����
    SInt    sTermMeetCnt; // ���а� ������ ���� �������� ����� ����
    SChar   sResult;

    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aSeg1Pt1 != NULL );
    IDE_DASSERT( aSeg1Pt2 != NULL );
    IDE_DASSERT( aSeg2Pt1 != NULL );
    IDE_DASSERT( aSeg2Pt2 != NULL );
    
    //----------------------------------------
    // Initialization
    //----------------------------------------

    sMeetCnt = 0;
    sOnPointCnt = 0;
    sTermMeetCnt = 0;

    //----------------------------------------
    // ����� ������ ���踦 �ľ���.
    //----------------------------------------

    // Segment1 �� Point1 �� �˻�
    if( stdUtils::between2D( aSeg2Pt1, aSeg2Pt2, aSeg1Pt1 ) == ID_TRUE )
    {
        // ���а� ���� ����
        sMeetCnt++;

        // ��� �������� �˻�
        if( (stdUtils::isSamePoints2D(aSeg2Pt1, aSeg1Pt1)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aSeg2Pt2, aSeg1Pt1)==ID_TRUE) )
        {
            // ���� ���� ����
            sOnPointCnt++;
        }
        else
        {
            // ���г����� ���� ����
        }

        // ���� ���� �˻�
        if ( aIsTermSeg1Pt1 == ID_TRUE )
        {
            // �������� ���а� ������ ����
            sTermMeetCnt++;
        }
        else
        {
            // �������� ���а� ������ ����
        }
    }
    else
    {
        // ������ ����
    }

    // Segment1 �� Point2 �� �˻�
    if( stdUtils::between2D( aSeg2Pt1, aSeg2Pt2, aSeg1Pt2 ) == ID_TRUE )
    {
        // ���а� ���� ����
        sMeetCnt++;

        // ��� �������� �˻�
        if( (stdUtils::isSamePoints2D(aSeg2Pt1, aSeg1Pt2)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aSeg2Pt2, aSeg1Pt2)==ID_TRUE) )
        {
            // ���� ���� ����
            sOnPointCnt++;
        }
        else
        {
            // ���г����� ���� ����
        }

        // ���� ���� �˻�
        if ( aIsTermSeg1Pt2 == ID_TRUE )
        {
            // �������� ���а� ������ ����
            sTermMeetCnt++;
        }
        else
        {
            // �������� ���а� ������ ����
        }
    }
    else
    {
        // ������ ����
    }

    // Segment2 �� Point1 �� �˻�
    if( stdUtils::between2D( aSeg1Pt1, aSeg1Pt2, aSeg2Pt1 ) == ID_TRUE )
    {
        // ���а� ���� ����
        sMeetCnt++;

        // ��� �������� �˻�
        if( (stdUtils::isSamePoints2D(aSeg1Pt1, aSeg2Pt1)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aSeg1Pt2, aSeg2Pt1)==ID_TRUE) )
        {
            // ���� ���� ����
            sOnPointCnt++;
        }
        else
        {
            // ���г����� ���� ����
        }

        // ���� ���� �˻�
        if ( aIsTermSeg2Pt1 == ID_TRUE )
        {
            // �������� ���а� ������ ����
            sTermMeetCnt++;
        }
        else
        {
            // �������� ���а� ������ ����
        }
    }
    else
    {
        // ������ ����
    }

    // Segment2 �� Point2 �� �˻�
    if( stdUtils::between2D( aSeg1Pt1, aSeg1Pt2, aSeg2Pt2 ) == ID_TRUE )
    {
        // ���а� ���� ����
        sMeetCnt++;

        // ��� �������� �˻�
        if( (stdUtils::isSamePoints2D(aSeg1Pt1, aSeg2Pt2)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aSeg1Pt2, aSeg2Pt2)==ID_TRUE) )
        {
            // ���� ���� ����
            sOnPointCnt++;
        }
        else
        {
            // ���г����� ���� ����
        }

        // ���� ���� �˻�
        if ( aIsTermSeg2Pt2 == ID_TRUE )
        {
            // �������� ���а� ������ ����
            sTermMeetCnt++;
        }
        else
        {
            // �������� ���а� ������ ����
        }
    }
    else
    {
        // ������ ����
    }
    
    //----------------------------------------
    // ������ ���� ������ ���� 9IM ��� ����
    //----------------------------------------
    
    switch ( sMeetCnt )
    {
        case 0:

            if( stdUtils::intersectI2D( aSeg1Pt1,
                                        aSeg1Pt2,
                                        aSeg2Pt1,
                                        aSeg2Pt2 ) == ID_TRUE )
            {
                sResult = '0';
            }
            else
            {
                sResult = 'F';
            }
            break;
            
        case 1:
            if ( sTermMeetCnt == 0 )
            {
                // �������� ���а� ����
                sResult = '0';
            }
            else
            {
                // �������� ���а� ����
                IDE_DASSERT( sTermMeetCnt == 1 );
                sResult = 'F';
            }
            break;
            
        case 2:
            if ( sOnPointCnt == 0 )
            {
                // ������ ������ �ʰ� ������ ������ ���
                sResult = '1';
            }
            else
            {
                // ���� ���� ����
                IDE_DASSERT( sOnPointCnt == 2 );
                
                if ( sTermMeetCnt == 0 )
                {
                    // ��� �������� ������ ����
                    sResult = '0';
                }
                else
                {
                    // �������� ������ ����
                    IDE_DASSERT( sTermMeetCnt < 3 );
                    
                    sResult = 'F';
                }
            }
            break;
            
        case 3:
        case 4:
            sResult = '1';
            break;
        default:
            IDE_ASSERT(0);
    }
    
    return sResult;
}

/***********************************************************************
 * Description:
 * ����1�� ����2�� ���� ��Ʈ������ �����Ѵ�
 * aPtEnd�� ���� ���� ���꿡�� ���ܵȴ�
 *
 * stdPoint2D*     aPtEnd(In): ����1
 * stdPoint2D*     aPtEndNext(In): ����1
 * stdPoint2D*     aPtMid1(In): ����2
 * stdPoint2D*     aPtMid2(In): ����2
 **********************************************************************/
SChar stfRelation::EndLineToMidLine(
                stdPoint2D*     aPtEnd,      // End Point
                stdPoint2D*     aPtEndNext,
                stdPoint2D*     aPtMid1,
                stdPoint2D*     aPtMid2)
{
    SInt    sOnPoint = 0;
    idBool  sAffectEnd = ID_FALSE;

    if(stdUtils::between2D( aPtEnd, aPtEndNext, aPtMid1 )==ID_TRUE)
    {
        sOnPoint++;
    }
    if(stdUtils::between2D( aPtEnd, aPtEndNext, aPtMid2 )==ID_TRUE)
    {
        sOnPoint++;
    }
    if(stdUtils::between2D( aPtMid1, aPtMid2, aPtEnd )==ID_TRUE)
    {
        sOnPoint++;
        sAffectEnd = ID_TRUE;
    }
    if(stdUtils::between2D( aPtMid1, aPtMid2, aPtEndNext )==ID_TRUE)
    {
        sOnPoint++;
    }

    if( sOnPoint >= 3 )
    {
        return '1';
    }
    else if( sOnPoint == 2 )
    {
        if( (stdUtils::isSamePoints2D(aPtEnd, aPtMid1)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aPtEnd, aPtMid2)==ID_TRUE) )
        {
            return 'F';
        }
        else if( (stdUtils::isSamePoints2D(aPtEndNext, aPtMid1)==ID_TRUE) ||
                 (stdUtils::isSamePoints2D(aPtEndNext, aPtMid2)==ID_TRUE) )
        {
            return '0';
        }
        else
        {
            return '1';
        }
    }
    else if( sOnPoint == 1 )
    {
        if( sAffectEnd )
        {
            return 'F';
        }
        else
        {
            return '0';
        }
    }

    if(stdUtils::intersectI2D( aPtEnd, aPtEndNext, aPtMid1, aPtMid2 )==ID_TRUE)
    {
        return '0';
    }

    return 'F';
}

/***********************************************************************
 * Description:
 * ����1�� ����2�� ���� ��Ʈ������ �����Ѵ�
 *
 * stdPoint2D*     aPt11(In): ����1
 * stdPoint2D*     aPt12(In): ����1
 * stdPoint2D*     aPt21(In): ����2
 * stdPoint2D*     aPt22(In): ����2
 **********************************************************************/
SChar stfRelation::MidLineToMidLine(
                stdPoint2D*     aPt11,
                stdPoint2D*     aPt12,
                stdPoint2D*     aPt21,
                stdPoint2D*     aPt22 )
{
    SInt    sOnPoint = 0;

    if(stdUtils::between2D( aPt11, aPt12, aPt21 )==ID_TRUE)
    {
        sOnPoint++;
    }
    if(stdUtils::between2D( aPt11, aPt12, aPt22 )==ID_TRUE)
    {
        sOnPoint++;
    }
    if(stdUtils::between2D( aPt21, aPt22, aPt11 )==ID_TRUE)
    {
        sOnPoint++;
    }
    if(stdUtils::between2D( aPt21, aPt22, aPt12 )==ID_TRUE)
    {
        sOnPoint++;
    }

    if( sOnPoint >= 3 )
    {
        return '1';
    }
    else if( sOnPoint == 2 )
    {
        if( (stdUtils::isSamePoints2D(aPt11, aPt21)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aPt11, aPt22)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aPt12, aPt21)==ID_TRUE) ||
            (stdUtils::isSamePoints2D(aPt12, aPt22)==ID_TRUE) )
        {
            return '0';
        }
        else
        {
            return '1';
        }
    }
    else if( sOnPoint == 1 )
    {
        return '0';
    }

    if(stdUtils::intersect2D( aPt11, aPt12, aPt21, aPt22 )==ID_TRUE)
    {
        return '0';
    }

    return 'F';
}

/***********************************************************************
 * Description:
 *
 *    Line Segment�� Line String ���� �����ϴ��� �˻�
 *
 * Implementation:
 *
 *    Line Segment�� �� ����  Line String�� �����ϴ� Segment���� �����
 *    Line Segment ���̿� �����ϴ� ��� Line String�� ���� �������� �̷������ �Ǵ�
 *
 *    Segment S�� ���� ��� T�� Segment�� �������� �ʴ´ٸ� ���Ե��� ����
 *
 *                 S
 *                 |
 *        T----T---S---T---T
 *
 *    m ��° Segment�� n ��° Segment���� ������ ���ԵǾ��ٸ�,
 *
 *      ...Tm----S1----T.......Tn---S2----T....
 *
 *    S1�� S2 ���̿� �ִ� ��� �� T�� �������� ��ġ�Ѵٸ�
 *    Segment S�� Line String T ���� �����Ѵ�.
 *
 *    BUG-16941 �� ���� T �� closed line string�� ��츦 ����Ͽ��� �Ѵ�.
 *
 *      T-------------------------------------T
 *      |                                     |
 *      T...Tn--S2--T...TzTa...Tm---S1---T....T
 *                       
 *
 **********************************************************************/
idBool
stfRelation::lineInLineString( stdPoint2D                * aPt1,
                               stdPoint2D                * aPt2,
                               const stdLineString2DType * aLine )
{
    SInt i;
    idBool sResult;

    stdPoint2D * sFirstPt;

    // Line String�� Segment ����
    SInt   sLSegCnt;
    idBool sClosed;

    // ���� ���ԵǴ� Segment�� ��ġ
    SInt sPt1SegIdx;
    SInt sPt2SegIdx;

    SInt sBeginIdx;
    SInt sEndIdx;
    
    //-------------------------------------------
    // Parameter Validation
    //-------------------------------------------

    IDE_DASSERT( aPt1 != NULL );
    IDE_DASSERT( aPt2 != NULL );
    IDE_DASSERT( aLine != NULL );

    //-------------------------------------------
    // Initialization
    //-------------------------------------------

    sResult = ID_FALSE;
    sPt1SegIdx = -1;  // ���ԵǴ� ��ġ�� ����
    sPt2SegIdx = -1;  // ���ԵǴ� ��ġ�� ����

    sLSegCnt = STD_N_POINTS(aLine) - 1;
    sClosed = stdUtils::isClosed2D( (stdGeometryHeader*) aLine );

    sFirstPt = STD_FIRST_PT2D(aLine);
    
    //-------------------------------------------
    // Segment�� �� ����
    // Line String�� ��� Segment�� ���ԵǴ��� �˻�
    //-------------------------------------------
    
    for( i = 0; i < sLSegCnt; i++ )
    {
        //-------------------------------
        // ��1 �� Segment�� ���ԵǴ��� �˻�
        //-------------------------------

        if( sPt1SegIdx == -1 )
        {
            // ��ġ�� �������� ����
            if ( stdUtils::between2D( STD_NEXTN_PT2D(sFirstPt,i),
                                      STD_NEXTN_PT2D(sFirstPt,i+1),
                                      aPt1 )
                 == ID_TRUE )
            {
                // Segment ���� ���� �����ϴ� ���
                sPt1SegIdx = i;
            }
            else
            {
                // Segment ���� �������� �ʴ� ���
            }
        }
        else
        {
            // �̹� ��ġ�� ������
        }

        //-------------------------------
        // ��2 �� Segment�� ���ԵǴ��� �˻�
        //-------------------------------

        if( sPt2SegIdx == -1 )
        {
            // ��ġ�� �������� ����
            if ( stdUtils::between2D( STD_NEXTN_PT2D(sFirstPt,i),
                                      STD_NEXTN_PT2D(sFirstPt,i+1),
                                      aPt2 )
                 == ID_TRUE )
            {
                // Segment ���� ���� �����ϴ� ���
                sPt2SegIdx = i;
            }
            else
            {
                // Segment ���� �������� �ʴ� ���
            }
        }
        else
        {
            // �̹� ��ġ�� ������
        }

        if( (sPt1SegIdx != -1) && (sPt2SegIdx != -1) )
        {
            // �� ���� ��� Segment�� ���Ե� ��� 
            break;
        }
    }

    //-------------------------------------------
    // Segment�� �� ���� ��ġ�� �̿��� �˻�
    //-------------------------------------------
    
    if( (sPt1SegIdx == -1) || (sPt2SegIdx == -1) )
    {
        // Line String �� �� ���� �������� �ʴ´�
        // S----ST-----T-----T
        //sResult = ID_FALSE;
    }
    else
    {
        // �� ���� ��� Line String���� ��� Segment�� ������.
        if ( sPt1SegIdx == sPt2SegIdx )
        {
            // ������ Segment�� �� �� ��� �����ϴ� ���
            //
            // T...T---S====S----T...T
            
            sResult = ID_TRUE;
        }
        else
        {
            // ���� �ٸ� Segment�� �� ���� ����
            // T...T---S===T==T==S---T...T
            
            // ���� Segment�� ���� Segment�� ����
            if ( sPt1SegIdx < sPt2SegIdx )
            {
                sBeginIdx = sPt1SegIdx;
                sEndIdx = sPt2SegIdx;
            }
            else
            {
                sBeginIdx = sPt2SegIdx;
                sEndIdx = sPt1SegIdx;
            }

            //--------------------------------------
            // �� �� ���̿� �����ϴ� Line String�� ������
            // �������� �̷���� Tb+1 ���� Te���� �˻�
            //
            // T---Tb--S==T==..==Te==S--T--T--...
            //--------------------------------------
            
            for ( i = sBeginIdx + 1; i <= sEndIdx; i++ )
            {
                if( stdUtils::collinear2D( aPt1, 
                                           aPt2, 
                                           STD_NEXTN_PT2D(sFirstPt,i) )
                    == ID_TRUE )
                {
                    // ������ �������� �̷�� ����.
                    sResult = ID_TRUE;
                }
                else
                {
                    // ������ �������� �ƴ�
                    //
                    //          Ti--S---T
                    //         /
                    // T---S--Ti

                    sResult = ID_FALSE;
                    break;
                }
            }

            if ( (sResult == ID_FALSE) && (sClosed == ID_TRUE) )
            {
                //--------------------------------------------
                // BUG-16941
                // Closed LineString�� ��� ������ �˻簡 �ʿ���
                //
                //                   �������� ����
                //                      |
                //                      V
                // ...Te--S==T==..==T==TT0==T==..==Tb==S--T...
                //--------------------------------------------
                
                for ( i = 0; i <= sBeginIdx; i++ )
                {
                    if( stdUtils::collinear2D( aPt1, 
                                               aPt2, 
                                               STD_NEXTN_PT2D(sFirstPt,i) )
                        == ID_TRUE )
                    {
                        // ������ �������� �̷�� ����.
                        sResult = ID_TRUE;
                    }
                    else
                    {
                        sResult = ID_FALSE;
                        IDE_RAISE( LINEINLINE2D_RESULT );
                    }
                }
                
                for ( i = sEndIdx + 1; i <= sLSegCnt; i++ )
                {
                    if( stdUtils::collinear2D( aPt1, 
                                               aPt2, 
                                               STD_NEXTN_PT2D(sFirstPt,i) )
                        == ID_TRUE )
                    {
                        // ������ �������� �̷�� ����.
                        sResult = ID_TRUE;
                    }
                    else
                    {
                        sResult = ID_FALSE;
                        IDE_RAISE( LINEINLINE2D_RESULT );
                    }
                }
            }
            else
            {
                // Go Go
            }
        }
    }

    IDE_EXCEPTION_CONT( LINEINLINE2D_RESULT );
    
    return sResult;
}


/***********************************************************************
 * Description:
 * �� ������ ���� Boundary�� ���ԵǸ� ID_TRUE �ƴϸ� ID_FASLE ����
 *
 * stdPoint2D*             aPt1(In): ����
 * stdPoint2D*             aPt2(In): ����
 * stdLinearRing2D*        aRing(In): ��
 **********************************************************************/
idBool stfRelation::lineInRing(
                            stdPoint2D*             aPt1,
                            stdPoint2D*             aPt2,
                            stdLinearRing2D*        aRing )
{
    SInt            i, sMax, sIdx1, sIdx2, sFence;
    idBool          sInside;
    stdPoint2D*     sPt;

    sIdx1 = -1;
    sIdx2 = -1;
    sMax = (SInt)(STD_N_POINTS(aRing)-1);
    sPt = STD_FIRST_PT2D(aRing);
    for( i = 0; i < sMax; i++ )
    {
        if( (sIdx1 == -1) && (
            (stdUtils::isSamePoints2D(
                STD_NEXTN_PT2D(sPt,i),
                aPt1)==ID_TRUE) ||
            (stdUtils::betweenI2D( 
                STD_NEXTN_PT2D(sPt,i), 
                STD_NEXTN_PT2D(sPt,i+1), 
                aPt1 )==ID_TRUE)) )
        {
            sIdx1 = i;
        }

        if( (sIdx2 == -1) && (
            (stdUtils::isSamePoints2D(
                STD_NEXTN_PT2D(sPt,i), 
                aPt2)==ID_TRUE) ||
            (stdUtils::betweenI2D( 
                STD_NEXTN_PT2D(sPt,i), 
                STD_NEXTN_PT2D(sPt,i+1), 
                aPt2 )==ID_TRUE)) )
        {
            sIdx2 = i;
        }

        if( (sIdx1 != -1) && (sIdx2 != -1) )
        {
            break;
        }
    }

    if( (sIdx1 == -1) || (sIdx2 == -1) )
    {
        return ID_FALSE;
    }
    else if( sIdx1 == sIdx2 )
    {
        return ID_TRUE;
    }
    else
    {
        if(sIdx1 > sIdx2)
        {
            sFence = sIdx2 + sMax;
        }
        else
        {
            sFence = sIdx2;
        }

        sInside = ID_TRUE;
        for( i = sIdx1+1; i < sFence; i++ )
        {
            if(stdUtils::collinear2D( 
                aPt1, 
                aPt2, 
                STD_NEXTN_PT2D(sPt,i%sMax) ) == ID_FALSE)
            {
                sInside = ID_FALSE;
                break;
            }
        }
        if(sInside==ID_TRUE)
        {
            return ID_TRUE;
        }

        if(sIdx1 < sIdx2)
        {
            sFence = sIdx1 + sMax;
        }
        else
        {
            sFence = sIdx1;
        }

        for( i = sIdx2+1; i < sFence; i++ )
        {
            if(stdUtils::collinear2D( 
                aPt1, 
                aPt2, 
                STD_NEXTN_PT2D(sPt,i%sMax) ) == ID_FALSE)
            {
                return ID_FALSE;
            }
        }
        return ID_TRUE;
    }
}

//==============================================================================
// ��ü�� ��ü�� ���踦 �����ϴ� �Լ��� �̸��� ������ ����
//
// Single       s
// Multi        m
//
// Point        p
// Line         l
// Area         a
// Collection   c
//
// Interior     i
// Boundary     b
// Exterior     e
//
// ��� ���� ������ �����Ѵ�
// '0' ��
// '1' ��
// '2' ��
// 'F' �������� �ʴ´�.
//==============================================================================

/* Point **************************************************************/

// point to point
SChar stfRelation::spiTospi( const stdPoint2D*                  aObj1,
                             const stdPoint2D*                  aObj2 )
{
    return ((aObj1->mX == aObj2->mX ) && (aObj1->mY == aObj2->mY)) ? '0' : 'F';
}

SChar stfRelation::spiTospe( const stdPoint2D*                  aObj1,
                             const stdPoint2D*                  aObj2 )
{
    return ((aObj1->mX != aObj2->mX ) || (aObj1->mY != aObj2->mY)) ? '0' : 'F';
}

// point vs linestring
SChar stfRelation::spiTosli( const stdPoint2D*                  aObj1,
                             const stdLineString2DType*         aObj2 )
{
    UInt        i, sMax;    
    stdPoint2D* sPt;

    // Fix BUG-15923
    if(stdUtils::isClosedPoint2D(aObj1, (stdGeometryHeader*)aObj2))
    {
        return '0';
    }
    
    sPt = STD_FIRST_PT2D(aObj2);
    sMax = STD_N_POINTS(aObj2)-1;

    for( i = 0; i < sMax; i++ )
    {
        if( ( i > 0 ) && (spiTospi( sPt+i, aObj1 )  == '0' ) )
        {
            return '0';
        }

        if(stdUtils::betweenI2D( 
            STD_NEXTN_PT2D(sPt,i), 
            STD_NEXTN_PT2D(sPt,i+1), 
            aObj1 )==ID_TRUE)
        {
            return '0';
        }
    }
    return 'F';
}

SChar stfRelation::spiToslb( const stdPoint2D*                  aObj1,
                             const stdLineString2DType*         aObj2 )
{
    stdPoint2D*     sPtS = STD_FIRST_PT2D(aObj2);
    stdPoint2D*     sPtE = STD_LAST_PT2D(aObj2);
    
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    if( spiTospi( aObj1, sPtS ) == '0' )
    {
        return '0';
    }
    if( spiTospi( aObj1, sPtE ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::spiTosle( const stdPoint2D*                  aObj1,
                             const stdLineString2DType*         aObj2 )
{
    UInt        i, sMax;
    stdPoint2D* sPt;

    sPt = STD_FIRST_PT2D(aObj2);
    sMax = STD_N_POINTS(aObj2)-1;

    // Fix BUG-15544
    for( i = 0; i < sMax; i++ )  // Fix BUG-15518
    {
        if(stdUtils::between2D( 
            STD_NEXTN_PT2D(sPt,i), 
            STD_NEXTN_PT2D(sPt,i+1), 
            aObj1 )==ID_TRUE)
        {
            return 'F';
        }
    }
    return '0';
}

//==============================================================================
// TASK-2015 �� ���� �� ���ο� �����ϴ��� �Ǻ�
//==============================================================================
// point vs polygon
SChar stfRelation::spiTosai( const stdPoint2D*                  aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdLinearRing2D*    sRings;
    UInt                i, sMax;
    
    sRings = STD_FIRST_RN2D(aObj2);
    sMax = STD_N_RINGS(aObj2);
    
    // Fix BUG-15433
    if( stdUtils::insideRingI2D(aObj1, sRings)==ID_TRUE )
    {
        if( STD_N_RINGS(aObj2) > 1 )
        {
            sRings = STD_NEXT_RN2D(sRings);
            for( i = 1; i < sMax; i++ )
            {
                if( stdUtils::insideRing2D(aObj1, sRings)==ID_TRUE )
                {
                    return 'F';
                }
                sRings = STD_NEXT_RN2D(sRings);
            }
            return '0';
        }
        else
        {
            return '0';
        }
    }
    else
    {
        return 'F';
    }
}

SChar stfRelation::spiTosab( const stdPoint2D*                  aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdLinearRing2D*    sRing;
    stdPoint2D*         sPt;
    UInt                i, j , sMax, sMaxR;
    
    sRing = STD_FIRST_RN2D(aObj2);
    sMaxR = STD_N_RINGS(aObj2);
    for( i = 0; i < sMaxR; i++ )
    {
        sMax = STD_N_POINTS(sRing)-1;
        sPt = STD_FIRST_PT2D(sRing);
        for( j = 0; j < sMax; j++ )
        {
            if( stdUtils::between2D( 
                sPt, 
                STD_NEXT_PT2D(sPt), 
                aObj1)==ID_TRUE )
            {
                return '0';
            }
            sPt = STD_NEXT_PT2D(sPt);
        }
        sPt = STD_NEXT_PT2D(sPt);
        sRing = (stdLinearRing2D*)sPt;
    }
    return 'F';
}

//==============================================================================
// TASK-2015 �� ���� �� ���ο� �����ϴ��� �Ǻ�
//==============================================================================
SChar stfRelation::spiTosae( const stdPoint2D*                  aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdLinearRing2D*    sRings;
    UInt                i, sMax;
    
    sRings = STD_FIRST_RN2D(aObj2);
    sMax = STD_N_RINGS(aObj2);

    // Fix BUG-15544
    if( stdUtils::insideRing2D(aObj1, sRings)==ID_TRUE )
    {
        if( STD_N_RINGS(aObj2) > 1 )
        {
            sRings = STD_NEXT_RN2D(sRings);
            for( i = 1; i < sMax; i++ )
            {
                if( stdUtils::insideRingI2D(aObj1, sRings)==ID_TRUE )
                {
                    return '0';
                }
                sRings = STD_NEXT_RN2D(sRings);
            }
            return 'F';
        }
        else
        {
            return 'F';
        }
    }
    else
    {
        return '0';
    }
}

// point vs multipoint
SChar stfRelation::spiTompi( const stdPoint2D*                  aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTospi(aObj1, &sPoint->mPoint) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::spiTompe( const stdPoint2D*                  aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    return (spiTompi(aObj1, aObj2) == '0') ? 'F' : '0';
}

SChar stfRelation::speTompi( const stdPoint2D*                  aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    if( STD_N_OBJECTS(aObj2) < 1 )
    {
        return 'F';
    }
    if( (STD_N_OBJECTS(aObj2) == 1) && (spiTompi(aObj1, aObj2) == '0') )
    {
        return 'F';
    }
    return '0';
}

// point vs multilinestring
SChar stfRelation::spiTomli( const stdPoint2D*                  aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType* sLine;
    UInt                 i, sMax;
    
    // Fix BUG-15923
    if(stdUtils::isClosedPoint2D(aObj1, (stdGeometryHeader*)aObj2))
    {
        return '0';
    }
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if(spiTosli( aObj1, sLine ) == '0')
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::spiTomlb( const stdPoint2D*                  aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType* sLine;
    UInt                 i, sMax;
    
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if(spiToslb( aObj1, sLine ) == '0')
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::spiTomle( const stdPoint2D*                  aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType* sLine;
    UInt                 i, sMax;
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    if( sMax < 1 )
    {
        return 'F';
    }
    for( i = 0; i < sMax; i++ )
    {
        if( ( spiTosli( aObj1, sLine ) == '0' ) ||
            ( spiToslb( aObj1, sLine ) == '0' ) )
        {
            return 'F';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return '0';
}

// point vs multipolygon
SChar stfRelation::spiTomai( const stdPoint2D*                  aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if(spiTosai( aObj1, sPoly ) == '0')
        {
            return '0';
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    return 'F';
}

SChar stfRelation::spiTomab( const stdPoint2D*                  aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if(spiTosab( aObj1, sPoly ) == '0')
        {
            return '0';
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    return 'F';
}

SChar stfRelation::spiTomae( const stdPoint2D*                  aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    if( sMax < 1 )
    {
        return 'F';
    }
    for( i = 0; i < sMax; i++ )
    {
        if( ( spiTosai( aObj1, sPoly ) == '0' ) ||
            ( spiTosab( aObj1, sPoly ) == '0' ) )
        {
            return 'F';
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    return '0';
}

// point vs collection
SChar stfRelation::spiTogci( const stdPoint2D*                  aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    
    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            if(spiTospi( aObj1, &sGeom->point2D.mPoint ) == '0')
            {
                return '0';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            if(spiTosli( aObj1, &sGeom->linestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            if(spiTosai( aObj1, &sGeom->polygon2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            if(spiTompi( aObj1, &sGeom->mpoint2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            if(spiTomli( aObj1, &sGeom->mlinestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            if(spiTomai( aObj1, &sGeom->mpolygon2D ) == '0')
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }
        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::spiTogcb( const stdPoint2D*                  aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    
    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            if(spiToslb( aObj1, &sGeom->linestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            if(spiTosab( aObj1, &sGeom->polygon2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            if(spiTomlb( aObj1, &sGeom->mlinestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            if(spiTomab( aObj1, &sGeom->mpolygon2D ) == '0')
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }
        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::spiTogce( const stdPoint2D*                  aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    if( STD_N_GEOMS(aObj2) < 1 )
    {
        return 'F';
    }
    if( (spiTogci(aObj1, aObj2) == '0') ||
        (spiTogcb(aObj1, aObj2) == '0') )
    {
        return 'F';
    }
    return '0';
}

SChar stfRelation::speTogci( const stdPoint2D*                  aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTospe( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            return '2';
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = speTompi( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            return '2';
        default:
            return 'F';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {
        
            if ( sResult < sRet ) 
            {
                sResult = sRet;
            } 
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::speTogcb( const stdPoint2D*              /*aObj1*/,
                             const stdGeoCollection2DType*    aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            return '1';
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            return '1';
        default:
            return 'F';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

/***********************************************************************
 * Description:
 *
 *    SLI(Single LineString Internal)�� SLI��
 *    DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *    �� ���� Segment ������ �ɰ��� ���踦 ���ϰ� ���� �ִ밪�� ���Ѵ�.
 *
 ***********************************************************************/

SChar
stfRelation::sliTosli( const stdLineString2DType*         aObj1,
                       const stdLineString2DType*         aObj2 )
{
    UInt         i;
    UInt         j;

    SChar        sTemp;
    SChar        sResult;
    
    stdPoint2D * sPt1;
    stdPoint2D * sPt2;

    // LineString�� �����ϴ� Segment�� ����
    UInt         sLSegCnt1;
    UInt         sLSegCnt2;

    // �����ִ� LineString������ ����
    idBool       sClosed1;
    idBool       sClosed2;

    // To Fix BUG-16912
    // ���������� ������������ ����
    idBool       sIsTermSeg1Begin;
    idBool       sIsTermSeg1End;
    idBool       sIsTermSeg2Begin;
    idBool       sIsTermSeg2End;
    
    //--------------------------------
    // Parameter Validation
    //--------------------------------

    IDE_DASSERT( aObj1 != NULL );
    IDE_DASSERT( aObj2 != NULL );

    //--------------------------------
    // Inialization
    //--------------------------------

    sResult = 'F';

    sLSegCnt1 = STD_N_POINTS(aObj1) - 1;
    sLSegCnt2 = STD_N_POINTS(aObj2) - 1;
    
    sClosed1 = stdUtils::isClosed2D((stdGeometryHeader*)aObj1);
    sClosed2 = stdUtils::isClosed2D((stdGeometryHeader*)aObj2);

    //--------------------------------
    // �� LineString�� Segment ������ ���踦 �˻�
    //--------------------------------
    
    for( i = 0, sPt1 = STD_FIRST_PT2D(aObj1);
         i < sLSegCnt1;
         i++, sPt1 = STD_NEXT_PT2D(sPt1) )
    {
        //--------------------------
        // BUG-16912 Segment1�� ���� ���������� �Ǵ�
        //--------------------------

        if ( sClosed1 != ID_TRUE )
        {
            // ���� LineString�� ���
            if ( i == 0 )
            {
                sIsTermSeg1Begin = ID_TRUE;
            }
            else
            {
                sIsTermSeg1Begin = ID_FALSE;
            }

            if ( (i+1) == sLSegCnt1 )
            {
                sIsTermSeg1End = ID_TRUE;
            }
            else
            {
                sIsTermSeg1End = ID_FALSE;
            }
        }
        else
        {
            // ���� LineString�� ���
            sIsTermSeg1Begin = ID_FALSE;
            sIsTermSeg1End = ID_FALSE;
        }

        //--------------------------
        // Segment1�� LineString�� ���踦 �˻�
        //--------------------------
        
        for( j = 0, sPt2 = STD_FIRST_PT2D(aObj2);
             j < sLSegCnt2;
             j++, sPt2 = STD_NEXT_PT2D(sPt2) )
        {
            //--------------------------
            // BUG-16912 Segment2�� ���� ���������� �Ǵ�
            //--------------------------

            if ( sClosed2 != ID_TRUE )
            {
                // ���� LineString�� ���
                if ( j == 0 )
                {
                    sIsTermSeg2Begin = ID_TRUE;
                }
                else
                {
                    sIsTermSeg2Begin = ID_FALSE;
                }

                if ( (j+1) == sLSegCnt2 )
                {
                    sIsTermSeg2End = ID_TRUE;
                }
                else
                {
                    sIsTermSeg2End = ID_FALSE;
                }
            }
            else
            {
                // ���� LineString�� ���
                sIsTermSeg2Begin = ID_FALSE;
                sIsTermSeg2End = ID_FALSE;
            }

            //--------------------------
            // Line Segment���� ���� ȹ��
            //--------------------------
            
            sTemp = compLineSegment( sPt1,
                                     STD_NEXT_PT2D(sPt1),
                                     sIsTermSeg1Begin,
                                     sIsTermSeg1End,
                                     sPt2,
                                     STD_NEXT_PT2D(sPt2),
                                     sIsTermSeg2Begin,
                                     sIsTermSeg2End );

            //--------------------------
            // ���� ��� ����
            //--------------------------
            
            if ( sTemp == 'F' )
            {
                // ������ �ʿ� ����
            }
            else
            {
                IDE_DASSERT( (sTemp == '0') || (sTemp == '1') );
                
                sResult = sTemp;
                
                // ������ �����ϴ� ��찡 �ִ� ������
                IDE_TEST_RAISE( sResult == '1', SLI2D_MAX_RESULT );
            }
        }
    }

    IDE_EXCEPTION_CONT(SLI2D_MAX_RESULT);
    
    return sResult;
}

SChar stfRelation::sliToslb( const stdLineString2DType*         aObj1,
                             const stdLineString2DType*         aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj2) ;
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj2);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    if( spiTosli( sPtS, aObj1 ) == '0' )
    {
        return '0';
    }

    if( spiTosli( sPtE, aObj1 ) == '0' )
    {
        return '0';
    }

    return 'F';
}

SChar stfRelation::sliTosle( const stdLineString2DType*         aObj1,
                             const stdLineString2DType*         aObj2 )
{
    stdPoint2D*     sPt1;
    UInt            i, sMax;

    // Fix BUG-15516
    sPt1 = STD_FIRST_PT2D(aObj1);
    sMax = STD_N_POINTS(aObj1)-1;
    for( i = 0; i < sMax; i++ )
    {
        if( lineInLineString( 
            STD_NEXTN_PT2D(sPt1,i), 
            STD_NEXTN_PT2D(sPt1,i+1), 
            aObj2 ) == ID_FALSE )
        {
            return '1';
        }
    }

    return 'F';
}

SChar stfRelation::slbToslb( const stdLineString2DType*         aObj1,
                             const stdLineString2DType*         aObj2 )
{
    stdPoint2D* sPt1S = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPt2S = STD_FIRST_PT2D(aObj2);
    stdPoint2D* sPt1E = STD_LAST_PT2D(aObj1);
    stdPoint2D* sPt2E = STD_LAST_PT2D(aObj2);


    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    if( stdUtils::isSamePoints2D(sPt1S, sPt2S)==ID_TRUE )
    {
        return '0';
    }
    if( stdUtils::isSamePoints2D(sPt1S, sPt2E)==ID_TRUE )
    {
        return '0';
    }
    if( stdUtils::isSamePoints2D(sPt1E, sPt2S)==ID_TRUE )
    {
        return '0';
    }
    if( stdUtils::isSamePoints2D(sPt1E, sPt2E)==ID_TRUE )
    {
        return '0';
    }

    return 'F';
}

SChar stfRelation::slbTosle( const stdLineString2DType*         aObj1,
                             const stdLineString2DType*         aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTosle( sPtS, aObj2) == '0' )
    {
        return '0';
    }
    if( spiTosle( sPtE, aObj2) == '0' )
    {
        return '0';
    }

    return 'F';
}

/***********************************************************************
 * Description:
 *
 *    SLI(Single LineString Internal)��
 *    SAI(Single Area Internal)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   LineString�� �����ϴ� [��, ����] ��
 *   Polygon�� �����ϴ� [��, ����, ��]�� ���踦 ���� ���س���.
 *
 *   ================================
 *     LineString .vs. Polygon
 *   ================================
 *
 *   1. ���� .vs. ��
 *
 *       - �� �Լ��� �������� �ٸ� ����κ��� ����
 *
 *   2. �� .vs. �� 
 *       - ���� �ܺο� ���� : ���� �Ұ�
 *       - ���� ���ο� ���� : TRUE
 *
 *                A--------A
 *                |        |
 *                |   L    |
 *                |        |
 *                A--------A
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             L---A====L----A
 *
 *             L---A====A----L
 *
 *             A---L====L----A
 *
 *       - ������ ������ ���� : ���� ���� ����κ��� ����
 *
 *                L
 *                |
 *            A---+----A
 *                |
 *                L
 *
 *          �׷���, �����ϴ��� ������ ���� Interior Ring�� ������ ��츦
 *          ����Ͽ��� �Ѵ�.
 *
 *                L
 *                |
 *            A---I----A
 *               /|\
 *              I L I
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Lp                     Lp
 *               |                      |
 *         A-----L------A  ==>   Ap----AL----An  
 *               |                      |
 *               Ln                     Ln
 *
 *   5. ���� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Ap                    Ap
 *               |                     |
 *         L-----A-----L  ==>   Lp----LA----Ln  
 *               |                     |
 *               An                    An
 *
 *   6. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Lp
 *                |
 *         Ap----AL-----An
 *                |
 *                Ln
 *
 *   BUG-16977 : BUG-16952�� ������ ������� ���� ������ ������ ���� ������
 *      1) Line�� ���� Area�� ������ ���� ���� ==> ���� ���� ���� �˰������� ����
 *      2) Line�� ���а� Area�� ���� ���� ���� ==> ���� ���� ���� �˰������� ����
 *      3) ���� ���� ���� ���� ==> ���� ���� ������ �ǵ��� Ȯ��
 *
 *   SLI.vs.SAE �� �޸� �����Ͽ��� �� ������ �ִ�.
 *
 *   ���� ���� ���迡�� ������ ���� ���ο� �ִ� �ϴ���
 *   ������ Interia Ring ������ ���ο����� �������� ������ �� ����.
 *   ����, ������ Line�� Interior Ring���ο� ���Ե��� ������ �˻��ؾ� ��.
 *
 *      Ap  
 *       \    Area
 *        \
 *         \      P               
 *          A------An              
 *
 *      Ap       I
 *       \   I  /  P
 *        \  | /
 *         \ |/
 *          AI-------An
 *
 ***********************************************************************/

SChar
stfRelation::sliTosai( const stdLineString2DType * aLineObj,
                       const stdPolygon2DType    * aAreaObj )
{
    UInt i, j, k, m, n;
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // Line String ����
    //----------------------------
    
    stdPoint2D      * sLinePt;
    stdPoint2D      * sLinePrevPt;
    stdPoint2D      * sLineCurrPt;
    stdPoint2D      * sLineNextPt;
    UInt              sLineSegCnt;
    UInt              sLinePtCnt;
    
    //----------------------------
    // Ring ����
    //----------------------------
    
    stdLinearRing2D * sRing;
    stdPoint2D      * sRingPt;
    stdPoint2D      * sRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sRingCnt;     // Ring Count of a Polygon
    UInt              sRingSegCnt;  // Segment Count of a Ring
    idBool            sRingCCWise;  // Ring �� �ð� ������������ ����

    UInt              sCheckSegCnt;
    stdLinearRing2D * sCheckRing;
    stdPoint2D      * sCheckPt;
    stdPoint2D        sItxPt;  // Intersection Point

    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aLineObj != NULL );
    IDE_DASSERT( aAreaObj != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    sLineSegCnt = STD_N_POINTS(aLineObj) - 1;
    sLinePtCnt = STD_N_POINTS(aLineObj);

    sRingCnt = STD_N_RINGS(aAreaObj);
    
    //----------------------------------------
    // LineString�� ���� Polygon�� ���� ���� ����
    //----------------------------------------

    for ( i = 0, sLinePt = STD_FIRST_PT2D(aLineObj);
          i < sLinePtCnt;
          i++, sLinePt = STD_NEXT_PT2D(sLinePt) )
    {
        // LineString�� ������ Polygon�� ���ο� �����ϴ��� �Ǵ�
        if( spiTosai( sLinePt, aAreaObj ) == '0' )
        {
            sResult = '1';
            IDE_RAISE( SLISAI2D_MAX_RESULT );
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------------
    // LineString�� ��, ���а� Polygon�� ��, ������ ����
    //----------------------------------------

    for ( i = 0, sLinePt = STD_FIRST_PT2D(aLineObj), sLinePrevPt = NULL;
          i < sLinePtCnt;
          i++, sLinePrevPt = sLinePt, sLinePt = STD_NEXT_PT2D(sLinePt) )
    {
        // Ring�� ������ŭ �ݺ�
        for ( j = 0, sRing = STD_FIRST_RN2D(aAreaObj);
              j < sRingCnt;
              j++, sRing = STD_NEXT_RN2D(sRing) )
        {
            sRingSegCnt = STD_N_POINTS(sRing) - 1;
            sRingCCWise = stdUtils::isCCW2D(sRing);

            // Ring�� �����ϴ� Segment ������ŭ �ݺ�
            for ( k = 0, sRingPt = STD_FIRST_PT2D(sRing);
                  k < sRingSegCnt;
                  k++, sRingPt = STD_NEXT_PT2D(sRingPt) )
            {
                //----------------------------------------
                // ���� ���踦 ���� ���� ����� ����� ���� ����
                //  - LineString ���� ���а� Ring�� ���а��� ����
                //  - LineString ���� ���а� Ring�� ������ ����
                //  - Ring�� ���� ���а� LineString�� ������ ����
                //  - LineString�� ���� Ring�� ������ ����
                //----------------------------------------

                sMeetOnPoint = ID_FALSE;

                //----------------------------
                // LineString�� ������, ������, �������� ����
                //----------------------------
                
                // sLinePrevPt :  for loop�� ���� ����
                sLineCurrPt = sLinePt;
                if ( i == sLineSegCnt )
                {
                    sLineNextPt = NULL;
                }
                else
                {
                    sLineNextPt = STD_NEXT_PT2D(sLinePt);
                }
                
                //----------------------------
                // Ring�� ������, ������, �������� ����
                //----------------------------
                
                sRingPrevPt = stdUtils::findPrevPointInRing2D( sRingPt,
                                                               k,
                                                               sRingSegCnt,
                                                               NULL );
                sRingCurrPt = sRingPt;
                sRingNextPt = stdUtils::findNextPointInRing2D( sRingPt,
                                                               k,
                                                               sRingSegCnt,
                                                               NULL );

                // ������ ���ΰ� �����ϴ��� �˻�
                if( ( i < sLineSegCnt ) &&
                    ( stdUtils::intersectI2D( sLinePt,
                                              STD_NEXT_PT2D(sLinePt),
                                              sRingPt,
                                              STD_NEXT_PT2D(sRingPt) )
                      ==ID_TRUE ) )
                {
                    // ������ �����ϴ� ���
                    // ������ �����ϴ��� �ٸ� Ring�� �������� �Ǵ��Ͽ��� �Ѵ�.
                    //
                    //      L
                    //      |
                    //  A---I----A
                    //     /|+
                    //    I L I

                    if ( sRingCnt > 1 )
                    {
                        IDE_ASSERT( stdUtils::getIntersection2D(
                                        sLinePt,
                                        STD_NEXT_PT2D(sLinePt),
                                        sRingPt,
                                        STD_NEXT_PT2D(sRingPt),
                                        & sItxPt ) == ID_TRUE );
                        
                        sMeetOnPoint = ID_TRUE;
                        
                        sLinePrevPt = sLinePt;
                        sLineCurrPt = & sItxPt;
                        sLineNextPt = STD_NEXT_PT2D(sLinePt);
                        
                        sRingPrevPt = sRingPt;
                        sRingCurrPt = & sItxPt;
                        sRingNextPt = STD_NEXT_PT2D(sRingPt);
                    }
                    else
                    {
                        sResult = '1';
                        IDE_RAISE( SLISAI2D_MAX_RESULT );
                    }
                }
                
                //----------------------------
                // ���г��� Ring�� ���� �����ϴ��� �˻�
                //----------------------------
                
                if ( ( i < sLineSegCnt ) &&
                     ( stdUtils::betweenI2D( sLinePt,
                                             STD_NEXT_PT2D(sLinePt),
                                             sRingPt )==ID_TRUE ) )
                {
                    // ���� ���п��� ���� => ���� ���� ����� ����
                    //               Ap                    Ap
                    //               |                     |
                    //         L-----A-----L  ==>   Lp----LA----Ln  
                    //               |                     |
                    //               An                    An
                    sMeetOnPoint = ID_TRUE;
                    
                    sLinePrevPt = sLinePt;
                    sLineCurrPt = sRingPt;
                    sLineNextPt = STD_NEXT_PT2D(sLinePt);
                }
                
                //----------------------------
                // Ring ���г��� LineString�� ���� �����ϴ��� �˻�
                //----------------------------

                if ( stdUtils::betweenI2D( sRingPt,
                                           STD_NEXT_PT2D(sRingPt),
                                           sLinePt ) == ID_TRUE )
                {
                    // ���� ���п��� ���� => ���� ���� ����� ����
                    //               Lp                     Lp
                    //               |                      |
                    //         A-----L------A  ==>   Ap----AL----An  
                    //               |                      |
                    //               Ln                     Ln
                    sMeetOnPoint = ID_TRUE;
                        
                    sRingPrevPt = sRingPt;
                    sRingCurrPt = sLinePt;
                    sRingNextPt = stdUtils::findNextPointInRing2D( sRingPt,
                                                                   k,
                                                                   sRingSegCnt,
                                                                   NULL );
                }

                //----------------------------
                // ���� ���� �����ϴ� �� �˻�
                //----------------------------
                
                if ( stdUtils::isSamePoints2D( sLinePt, sRingPt ) == ID_TRUE )
                {
                    sMeetOnPoint = ID_TRUE;

                    // �̹� ������ ���� ���
                }

                //----------------------------
                // Ring�� Ring�� ��ġ�� ������ ���θ� �˻�
                //----------------------------
                
                if ( sMeetOnPoint == ID_TRUE )
                {
                    // Ring�� Ring�� ��ġ�� ���� ���
                    // ���ο� �����ϴ� ���� �Ǵ��� �� ����.
                    // �ٸ� ���� ���Ͽ� �Ǻ� �����ϴ�.
                    //
                    //              Pn
                    //       Ap       I
                    //        \   I  /  Pn
                    //         \  | /
                    //          \ |/
                    //           AIP-------An

                    for ( m = 0, sCheckRing = STD_FIRST_RN2D(aAreaObj);
                          m < sRingCnt;
                          m++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                    {
                        if ( j == m )
                        {
                            // �ڽ��� Ring�� �˻����� ����
                            continue;
                        }
                        else
                        {
                            sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                            
                            for ( n = 0, sCheckPt = STD_FIRST_PT2D(sCheckRing);
                                  n < sCheckSegCnt;
                                  n++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                            {
                                if ( stdUtils::between2D(
                                         sCheckPt,
                                         STD_NEXT_PT2D(sCheckPt),
                                         sLineCurrPt ) == ID_TRUE )
                                {
                                    sMeetOnPoint = ID_FALSE;
                                    break;
                                }
                            }
                            
                            if ( sMeetOnPoint != ID_TRUE )
                            {
                                break;
                            }
                            
                        }
                    }
                        
                }
                
                //----------------------------------------
                // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                //----------------------------------------
                
                if ( sMeetOnPoint == ID_TRUE )
                {
                    if ( hasRelLineSegRingSeg(
                             (j == 0 ) ? ID_TRUE : ID_FALSE, // Exterior Ring
                             sRingCCWise,
                             sRingPrevPt,
                             sRingNextPt,
                             sRingCurrPt,
                             sLinePrevPt,
                             sLineNextPt,
                             STF_INSIDE_ANGLE_POS ) == ID_TRUE )
                    {
                        sResult = '1';
                        IDE_RAISE( SLISAI2D_MAX_RESULT );
                    }
                    else
                    {
                        // ���� ���θ� �Ǵ��� �� ����
                    }
                }
                else // sMeetOnPoint == ID_FALSE
                {
                    // �˻� ����� �ƴ�
                }
                
            } // for k
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SLISAI2D_MAX_RESULT);
    
    return sResult;
}


/***********************************************************************
 * Description:
 *
 *    SLI(Single LineString Internal)��
 *    SAB(Single Area Boundary)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *    �� ���� Segment ������ �ɰ��� ���踦 ���ϰ� ���� �ִ밪�� ���Ѵ�.
 *
 ***********************************************************************/

SChar
stfRelation::sliTosab( const stdLineString2DType*         aObj1,
                       const stdPolygon2DType*            aObj2 )
{
    UInt         i;
    UInt         j;
    UInt         k;

    SChar        sTemp;
    SChar        sResult;
    
    stdPoint2D      * sPt1;
    stdPoint2D      * sPt2;
    stdLinearRing2D * sRing;
    
    UInt         sLineLSegCnt;  // Line String�� Line Segment ����
    UInt         sRingCnt;      // Polygon�� �����ϴ� Ring�� ����
    UInt         sRingLSegCnt;  // �� Ring�� �����ϴ� Line Segment ����

    // �����ִ� LineString������ ����
    idBool       sClosed;

    // To Fix BUG-16915
    // ���������� ������������ ����, Polygon�� �������� ����.
    idBool       sIsTermSegBegin;
    idBool       sIsTermSegEnd;

    //--------------------------------
    // Parameter Validation
    //--------------------------------

    IDE_DASSERT( aObj1 != NULL );
    IDE_DASSERT( aObj2 != NULL );

    //--------------------------------
    // Inialization
    //--------------------------------

    sResult = 'F';
    
    sLineLSegCnt = STD_N_POINTS(aObj1) - 1;
    sClosed = stdUtils::isClosed2D((stdGeometryHeader*)aObj1);
    
    sRingCnt = STD_N_RINGS(aObj2);

    //--------------------------------
    // LineString�� Segment ������ ���踦 �˻�
    //--------------------------------
    
    for( i = 0, sPt1 = STD_FIRST_PT2D(aObj1);
         i < sLineLSegCnt;
         i++, sPt1 = STD_NEXT_PT2D(sPt1) )
    {
        //--------------------------
        // BUG-16915 Line Segment�� ���� ���������� �Ǵ�
        //--------------------------

        if ( sClosed != ID_TRUE )
        {
            // ���� LineString�� ���
            if ( i == 0 )
            {
                sIsTermSegBegin = ID_TRUE;
            }
            else
            {
                sIsTermSegBegin = ID_FALSE;
            }

            if ( (i+1) == sLineLSegCnt )
            {
                sIsTermSegEnd = ID_TRUE;
            }
            else
            {
                sIsTermSegEnd = ID_FALSE;
            }
        }
        else
        {
            // BUG-16915 CLOSE ���θ� �Ǵ��ؾ� ��.
            // ���� LineString�� ���
            sIsTermSegBegin = ID_FALSE;
            sIsTermSegEnd = ID_FALSE;
        }
        
        //--------------------------------
        // Polygon�� �����ϴ� �� Ring ������ �˻�
        //--------------------------------

        // ���� Ring�� ���Ҷ��� ���� Ring�� ������ �����κ��� ���Ѵ�.
        for( j = 0, sRing = STD_FIRST_RN2D(aObj2);
             j < sRingCnt;
             j++, sRing = (stdLinearRing2D*) STD_NEXT_PT2D(sPt2) )
        {
            //--------------------------------
            // Ring�� �����ϴ� Line Segment ������ �˻�
            //--------------------------------

            sRingLSegCnt = STD_N_POINTS(sRing) - 1;
            
            for( k = 0, sPt2 = STD_FIRST_PT2D(sRing);
                 k < sRingLSegCnt;
                 k++, sPt2 = STD_NEXT_PT2D(sPt2) )
            {
                //------------------------------------
                // LineString�� Segment�� Ring�� Segment���� ���� ȹ��
                //------------------------------------

                // Ring�� �����ϴ� Line Segment�� �������� ����
                sTemp = compLineSegment( sPt1,
                                         STD_NEXT_PT2D(sPt1),
                                         sIsTermSegBegin,
                                         sIsTermSegEnd,
                                         sPt2,
                                         STD_NEXT_PT2D(sPt2),
                                         ID_FALSE,
                                         ID_FALSE );

                //--------------------------
                // ���� ��� ����
                //--------------------------
                
                if ( sTemp == 'F' )
                {
                    // ������ �ʿ� ����
                }
                else
                {
                    IDE_DASSERT( (sTemp == '0') || (sTemp == '1') );
                    
                    sResult = sTemp;
                    
                    // ������ �����ϴ� ��찡 �ִ� ������
                    IDE_TEST_RAISE( sResult == '1', SLISAB2D_MAX_RESULT );
                }
                
            }
        }
    }

    IDE_EXCEPTION_CONT(SLISAB2D_MAX_RESULT);

    return sResult;
}

/***********************************************************************
 * Description:
 *
 *    SLI(Single LineString Internal)��
 *    SAE(Single Area External)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   LineString�� �����ϴ� [��, ����] ��
 *   Polygon�� �����ϴ� [��, ����, ��]�� ���踦 ���� ���س���.
 *
 *   ================================
 *     LineString .vs. Polygon
 *   ================================
 *
 *   1. ���� .vs. ��
 *
 *       - �� �Լ��� �������� �ٸ� ����κ��� ����
 *
 *   2. �� .vs. �� 
 *       - ���� ���ο� ���� : ���� �Ұ�
 *       - ���� �ܺο� ���� : TRUE
 *
 *           L    A--------A
 *                |        |
 *                |        |
 *                A--------A
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             L---A====L----A
 *
 *             L---A====A----L
 *
 *             A---L====L----A
 *
 *       - ������ ������ ���� : TRUE
 *
 *                L
 *                |
 *            A---+----A
 *                |
 *                L
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Lp                     Lp
 *               |                      |
 *         A-----L------A  ==>   Ap----AL----An  
 *               |                      |
 *               Ln                     Ln
 *
 *   5. ���� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Ap                    Ap
 *               |                     |
 *         L-----A-----L  ==>   Lp----LA----Ln  
 *               |                     |
 *               An                    An
 *
 *   6. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Lp
 *                |
 *         Ap----AL-----An
 *                |
 *                Ln
 *
 *   BUG-16952 ���� ������ ������ ���� ������
 *      1) Line�� ���� Area�� ������ ���� ���� ==> ���� ���� ���� �˰������� ����
 *      2) Line�� ���а� Area�� ���� ���� ���� ==> ���� ���� ���� �˰������� ����
 *      3) ���� ���� ���� ���� ==> ���� ���� ������ �ǵ��� Ȯ��
 *
 *   ������ ���� ���п� ���õ� ������ ���⿡ �����
 *
 *   --------------------------------------------
 *   ���� ����) LineString�� ���а� Polygon ���� ����
 *   --------------------------------------------
 *
 *   LineString�� ���г��� Polygon�� ���� �����ϴ� ���
 *
 *        ----A----
 *
 *        if Area(Ap, A, Lp) * Area(A, An, Ln) < 0,
 *        ; TRUE(SLI �� SAE �� ������)
 *
 *          Ap              
 *         / \             
 *        Lp--A----Ln        
 *             \  /         
 *              An
 *
 *        ; ������ ���� ���� �����
 *
 *            A------Ap
 *            |   ** |
 *            |  Lp--A--LnA----A
 *            |      |**  |    |
 *            |      An---A    |
 *            |                |
 *            A----------------A
 *
 *        if Area(Ap, A, Lp) * Area(A, An, Ln) > 0,
 *        ; ���� ���� ���迡�� ����
 *
 *          Ap  An            
 *         / \ /  \          
 *        Lp--A----Ln
 *
 *        if Area(Ap, A, Lp) * Area(A, An, Ln) == 0,
 *        ; ���а� ������ ���п��� �����ϴ� ���迡�� ���� (BUG-16952)
 *
 *          Ap
 *         /  \
 *        Lp---A----Ln--An
 *
 *   ------------------------------------------------
 *   ���� ����) LineString�� ���� Polygon ���� ������ ����
 *   ------------------------------------------------
 *
 *   Polygon �� ���г��� LineString�� ���� �����ϴ� ���
 *
 *        ----L----
 *
 *        if L�� ���� �Ǵ� ���� �� Lp�� ������ �ܺθ� ���ϰ� �ִٸ�,
 *        ; TRUE(SLI �� SAE �� ������)
 *
 *        A         A
 *        |  Area   |
 *        |         |
 *        A----L----A
 *             |
 *             Lp
 *
 *        Area Segment�� Line String�� �� ���� ������ ���
 *        �ش����� ���� �������� Ring�� ������ ������ �ݴ��� �����ϴ� �� �˻�
 *
 *       �ܺθ��� ��� 
 *          �ð� ������ ��� ���ʿ� �����ϸ� SLI�� �ܺο� ����
 *          �ð� �ݴ������ ��� �����ʿ� �����ϸ� SLI�� �ܺο� ����
 *       ���θ��� ���
 *          �ð� ������ ��� �����ʿ� �����ϸ� SLI�� �ܺο� ����
 *          �ð� �ݴ������ ��� ���ʿ� �����ϸ� SLI�� �ܺο� ����
 *
 *       ������ ���� ������ �����س�.
 *
 *       A----->>------A                 A----------A
 *       |             |                 |          |
 *       |  A--L--<<---A                 |  I->>-I  |
 *       |  |  |                         |  |    |  |
 *       |  A--L-->>---A                 |  L----L  |
 *       |             |                 |  |    |  |
 *       A------<<-----A                 |  I-<<-I  |
 *                                       |          |
 *                                       A----------A
 *
 *
 *        if L�� ���� �Ǵ� ���� �� Lp�� ������ ���θ� ���ϰ� �ִٸ�,
 *        ; ���� ���� ����κ��� ���ߵ�
 *
 *        A       Lp   A
 *        |  Area |    |
 *        |       |    |
 *        A-------L----A
 *
 *        if L�� ���� �Ǵ� ���� �� Lp�� ������ ���� ��ġ�Ѵٸ�,
 *        ; ���а� ������ ���п��� �����ϴ� ����κ��� ���ߵ�(BUG-16952)
 *
 *        A           A
 *        |   Area    |
 *        |           |
 *        A----L-Lp---A
 *
 ***********************************************************************/

SChar
stfRelation::sliTosae( const stdLineString2DType * aLineObj,
                       const stdPolygon2DType    * aAreaObj )
{
    UInt i, j, k;
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // Line String ����
    //----------------------------
    
    stdPoint2D      * sLinePt;
    stdPoint2D      * sLinePrevPt;
    stdPoint2D      * sLineNextPt;
    UInt              sLineSegCnt;
    UInt              sLinePtCnt;
    
    //----------------------------
    // Ring ����
    //----------------------------
    
    stdLinearRing2D * sRing;
    stdPoint2D      * sRingPt;
    stdPoint2D      * sRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sRingCnt;     // Ring Count of a Polygon
    UInt              sRingSegCnt;  // Segment Count of a Ring
    idBool            sRingCCWise;  // Ring �� �ð� ������������ ����

    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aLineObj != NULL );
    IDE_DASSERT( aAreaObj != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    sLineSegCnt = STD_N_POINTS(aLineObj) - 1;
    sLinePtCnt = STD_N_POINTS(aLineObj);

    sRingCnt = STD_N_RINGS(aAreaObj);
    
    //----------------------------------------
    // LineString�� ���� Polygon�� ���� ���� ����
    //----------------------------------------

    for ( i = 0, sLinePt = STD_FIRST_PT2D(aLineObj);
          i < sLinePtCnt;
          i++, sLinePt = STD_NEXT_PT2D(sLinePt) )
    {
        // LineString�� ������ Polygon�� �ܺο� �����ϴ��� �Ǵ�
        if( spiTosae( sLinePt, aAreaObj ) == '0' )
        {
            sResult = '1';
            IDE_RAISE( SLISAE2D_MAX_RESULT );
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------------
    // LineString�� ��, ���а� Polygon�� ��, ������ ����
    //----------------------------------------

    for ( i = 0, sLinePt = STD_FIRST_PT2D(aLineObj), sLinePrevPt = NULL;
          i < sLinePtCnt;
          i++, sLinePrevPt = sLinePt, sLinePt = STD_NEXT_PT2D(sLinePt) )
    {
        // Ring�� ������ŭ �ݺ�
        for ( j = 0, sRing = STD_FIRST_RN2D(aAreaObj);
              j < sRingCnt;
              j++, sRing = STD_NEXT_RN2D(sRing) )
        {
            sRingSegCnt = STD_N_POINTS(sRing) - 1;
            sRingCCWise = stdUtils::isCCW2D(sRing);

            // Ring�� �����ϴ� Segment ������ŭ �ݺ�
            for ( k = 0, sRingPt = STD_FIRST_PT2D(sRing);
                  k < sRingSegCnt;
                  k++, sRingPt = STD_NEXT_PT2D(sRingPt) )
            {
                //----------------------------------------
                // LineString ���� ���а� Polygon ���� ������ ����
                //----------------------------------------
                
                // ������ ���ΰ� �����ϴ��� �˻�
                if( ( i < sLineSegCnt ) &&
                    ( stdUtils::intersectI2D( sLinePt,
                                              STD_NEXT_PT2D(sLinePt),
                                              sRingPt,
                                              STD_NEXT_PT2D(sRingPt) )
                      ==ID_TRUE ) )
                {
                    // ������ �����ϴ� ���
                    sResult = '1';
                    IDE_RAISE( SLISAE2D_MAX_RESULT );
                }
                else
                {
                    // ������ �������� �ʴ� ���
                    
                }

                //----------------------------------------
                // ���� ���踦 ���� ���� ����� ����� ���� ����
                //  - LineString ���� ���а� Ring�� ������ ����
                //  - Ring�� ���� ���а� LineString�� ������ ����
                //  - LineString�� ���� Ring�� ������ ����
                //----------------------------------------

                sMeetOnPoint = ID_FALSE;

                //----------------------------
                // LineString�� ������, ������, �������� ����
                //----------------------------
                
                // sLinePrevPt :  for loop�� ���� ����
                if ( i == sLineSegCnt )
                {
                    sLineNextPt = NULL;
                }
                else
                {
                    sLineNextPt = STD_NEXT_PT2D(sLinePt);
                }
                
                //----------------------------
                // Ring�� ������, ������, �������� ����
                //----------------------------
                
                sRingPrevPt = stdUtils::findPrevPointInRing2D( sRingPt,
                                                               k,
                                                               sRingSegCnt,
                                                               NULL );
                sRingCurrPt = sRingPt;
                sRingNextPt = stdUtils::findNextPointInRing2D( sRingPt,
                                                               k,
                                                               sRingSegCnt,
                                                               NULL );
                
                //----------------------------
                // ���г��� Ring�� ���� �����ϴ��� �˻�
                //----------------------------
                
                if ( ( i < sLineSegCnt ) &&
                     ( stdUtils::betweenI2D( sLinePt,
                                             STD_NEXT_PT2D(sLinePt),
                                             sRingPt )==ID_TRUE ) )
                {
                    // ���� ���п��� ���� => ���� ���� ����� ����
                    //               Ap                    Ap
                    //               |                     |
                    //         L-----A-----L  ==>   Lp----LA----Ln  
                    //               |                     |
                    //               An                    An
                    sMeetOnPoint = ID_TRUE;
                    
                    sLinePrevPt = sLinePt;
                    sLineNextPt = STD_NEXT_PT2D(sLinePt);
                }
                
                //----------------------------
                // Ring ���г��� LineString�� ���� �����ϴ��� �˻�
                //----------------------------

                if ( stdUtils::betweenI2D( sRingPt,
                                           STD_NEXT_PT2D(sRingPt),
                                           sLinePt ) == ID_TRUE )
                {
                    // ���� ���п��� ���� => ���� ���� ����� ����
                    //               Lp                     Lp
                    //               |                      |
                    //         A-----L------A  ==>   Ap----AL----An  
                    //               |                      |
                    //               Ln                     Ln
                    sMeetOnPoint = ID_TRUE;
                        
                    sRingPrevPt = sRingPt;
                    sRingCurrPt = sLinePt;
                    sRingNextPt = stdUtils::findNextPointInRing2D( sRingPt,
                                                                   k,
                                                                   sRingSegCnt,
                                                                   NULL );
                }

                //----------------------------
                // ���� ���� �����ϴ� �� �˻�
                //----------------------------
                
                if ( stdUtils::isSamePoints2D( sLinePt, sRingPt ) == ID_TRUE )
                {
                    sMeetOnPoint = ID_TRUE;

                    // �̹� ������ ���� ���
                }

                //----------------------------------------
                // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                //----------------------------------------
                
                if ( sMeetOnPoint == ID_TRUE )
                {
                    if ( hasRelLineSegRingSeg(
                             (j == 0 ) ? ID_TRUE : ID_FALSE, // Exterior Ring
                             sRingCCWise,
                             sRingPrevPt,
                             sRingNextPt,
                             sRingCurrPt,
                             sLinePrevPt,
                             sLineNextPt,
                             STF_OUTSIDE_ANGLE_POS ) == ID_TRUE )
                    {
                        sResult = '1';
                        IDE_RAISE( SLISAE2D_MAX_RESULT );
                    }
                    else
                    {
                        // ���� ���θ� �Ǵ��� �� ����
                    }
                }
                else // sMeetOnPoint == ID_FALSE
                {
                    // �˻� ����� �ƴ�
                }
                
            } // for k
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SLISAE2D_MAX_RESULT);
    
    return sResult;
}

SChar stfRelation::slbTosai( const stdLineString2DType*         aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTosai( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTosai( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }

    return 'F';
}

SChar stfRelation::slbTosab( const stdLineString2DType*         aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTosab( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTosab( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }

    return 'F';
}

SChar stfRelation::slbTosae( const stdLineString2DType*         aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTosae( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTosae( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }

    return 'F';
}

SChar stfRelation::sleTosab( const stdLineString2DType*         aObj1,
                             const stdPolygon2DType*            aObj2 )
{
    if( STD_N_RINGS(aObj2) > 1 )
    {
        return '1';
    }

    stdLinearRing2D*    sRing = STD_FIRST_RN2D(aObj2);
    stdPoint2D*         sPt2 = STD_FIRST_PT2D(sRing);
    UInt                i, sMax;

    // Fix BUG-15516
    sMax = STD_N_POINTS(sRing)-1;
    for( i = 0; i < sMax; i++ )
    {
        if( lineInLineString( 
            STD_NEXTN_PT2D(sPt2,i), 
            STD_NEXTN_PT2D(sPt2,i+1), 
            aObj1 ) == ID_FALSE )
        {
            return '1';
        }
    }

    return 'F';
}
// linestring vs multipoint
SChar stfRelation::sliTompi( const stdLineString2DType*         aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        if( spiTosli(&sPoint->mPoint, aObj1) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::slbTompi( const stdLineString2DType*         aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTompi( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTompi( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::slbTompe( const stdLineString2DType*         aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTompe( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTompe( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::sleTompi( const stdLineString2DType*         aObj1,
                             const stdMultiPoint2DType*         aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        if( spiTosle(&sPoint->mPoint, aObj1) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

// linestring vs multilinestring
SChar stfRelation::sliTomli( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosli(aObj1, sLine);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sliTomlb( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    stdPoint2D*             sPtS, *sPtE;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sPtS = STD_FIRST_PT2D(sLine);
        sPtE = STD_LAST_PT2D(sLine);

        if( spiTosli( sPtS, aObj1 ) == '0' )
        {
            return '0';
        }
        if( spiTosli( sPtE, aObj1 ) == '0' )
        {
            return '0';
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::sliTomle( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdPoint2D*             sPt1;
    stdLineString2DType*    sLine2;
    UInt                    i, j, sMax1, sMax2;
    idBool                  sFound;

    // Fix BUG-15516
    sMax1 = STD_N_POINTS(aObj1)-1;
    sMax2 = STD_N_OBJECTS(aObj2);

    sPt1 = STD_FIRST_PT2D(aObj1);
    for( i = 0; i < sMax1; i++ )
    {
        sFound = ID_FALSE;
        sLine2 = STD_FIRST_LINE2D(aObj2);
        for( j = 0; j < sMax2; j++ )
        {
            if( lineInLineString( 
                STD_NEXTN_PT2D(sPt1,i), 
                STD_NEXTN_PT2D(sPt1,i+1), 
                sLine2 ) == ID_TRUE )
            {
                sFound = ID_TRUE;
                break;
            }
            sLine2 = STD_NEXT_LINE2D(sLine2);
        }

        if( sFound == ID_FALSE )
        {
            return '1';
        }
    }

    return 'F';
}

SChar stfRelation::slbTomli( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTomli( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomli( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::slbTomlb( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    if( spiTomlb( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomlb( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::slbTomle( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTomle( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomle( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::sleTomli( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdPoint2D*             sPt2;
    stdLineString2DType*    sLine2;
    UInt                    i, j, sMaxObj, sMaxPt;

    // Fix BUG-15516
    sLine2 = STD_FIRST_LINE2D(aObj2);
    sMaxObj = STD_N_OBJECTS(aObj2);
    for( i = 0; i < sMaxObj; i++ )
    {
        sPt2 = STD_FIRST_PT2D(sLine2);
        sMaxPt = STD_N_POINTS(sLine2)-1;  // Fix BUG-15518
        for( j = 0; j < sMaxPt; j++ )   // Fix BUG-16227
        {
            if( lineInLineString( 
                STD_NEXTN_PT2D(sPt2,j), 
                STD_NEXTN_PT2D(sPt2,j+1),
                aObj1 ) == ID_FALSE )
            {
                return '1';
            }
        }
    }

    return 'F';
}

SChar stfRelation::sleTomlb( const stdLineString2DType*         aObj1,
                             const stdMultiLineString2DType*    aObj2 )
{
    stdLineString2DType*    sLine;
    stdPoint2D*             sPtS, *sPtE;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sPtS = STD_FIRST_PT2D(sLine);
        sPtE = STD_LAST_PT2D(sLine);

        if( spiTosle( sPtS, aObj1 ) == '0' )
        {
            return '0';
        }
        if( spiTosle( sPtE, aObj1 ) == '0' )
        {
            return '0';
        }

        sLine = (stdLineString2DType*)STD_NEXT_PT2D(sPtE);
    }
    return 'F';
}

// linestring vs multipolygon
SChar stfRelation::sliTomai( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);    
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosai(aObj1, sPoly);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sliTomab( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);    
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosab(aObj1, sPoly);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

/***********************************************************************
 *
 * Description :
 *
 *    ���� ���� ��ü�� ���� ������ ���� ���� ��ü�� �ܺ� �������� ���踦 ����.
 *    sli(single line internal), mae(multi area external)
 *
 * Implementation :
 *
 *    ǥ��� : Ai (��ü A�� interior ����)
 *
 *    ���� ��ü ���� ������ ���� ��ü �ܺ� �������� ����� ������ ���� ������ ǥ��
 *
 *    Si ^ ( A U B U ...U N )e
 *    <==>
 *    Si ^ ( Ae ^ Be ^ ... Ne )
 *
 ***********************************************************************/

SChar stfRelation::sliTomae( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet;
    SChar               sResult;

    //--------------------------------------
    // Initialization
    //--------------------------------------

    sResult = '1';
    
    //--------------------------------------
    // ��� �ܺ� ������ �������� �ִ��� �˻�
    //--------------------------------------
    
    sPoly = STD_FIRST_POLY2D(aObj2);    
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosae(aObj1, sPoly);

        if( sRet != '1' )
        {
            // �������� ����
            sResult = 'F';
            break;
        }
        else
        {
            // �������� ������
        }
        
        sPoly = STD_NEXT_POLY2D(sPoly);
    }

    return sResult;
}


SChar stfRelation::slbTomai( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTomai( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomai( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::slbTomab( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTomab( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomab( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::slbTomae( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2D* sPtS = STD_FIRST_PT2D(aObj1);
    stdPoint2D* sPtE = STD_LAST_PT2D(aObj1);

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    if( spiTomae( sPtS, aObj2 ) == '0' )
    {
        return '0';
    }
    if( spiTomae( sPtE, aObj2 ) == '0' )
    {
        return '0';
    }
    return 'F';
}

SChar stfRelation::sleTomab( const stdLineString2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);    
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sleTosab(aObj1, sPoly);

        if( sRet == '1' )
        {
            return '1';
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

// linestring vs geometrycollection
SChar stfRelation::sliTogci( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTosli( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTosli( aObj1, &sGeom->linestring2D );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sliTosai( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = sliTompi( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sliTomli( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sliTomai( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sliTogcb (const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;
    

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliToslb( aObj1, &sGeom->linestring2D );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sliTosab( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sliTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sliTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sliTogce( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTosle( aObj1, &sGeom->linestring2D );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sliTosae( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sliTomle( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sliTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::slbTogci( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    stdPoint2D*         sPtS;
    stdPoint2D*         sPtE;
    UInt                i, sMax;
    SChar               sRet = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sPtS = STD_FIRST_PT2D(aObj1);
    sPtE = STD_LAST_PT2D(aObj1);
    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTospi( sPtS, &sGeom->point2D.mPoint );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTospi( sPtE, &sGeom->point2D.mPoint );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = spiTosli( sPtS, &sGeom->linestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTosli( sPtE, &sGeom->linestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = spiTosai( sPtS, &sGeom->polygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTosai( sPtE, &sGeom->polygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = spiTompi( sPtS, &sGeom->mpoint2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTompi( sPtE, &sGeom->mpoint2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = spiTomli( sPtS, &sGeom->mlinestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTomli( sPtE, &sGeom->mlinestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = spiTomai( sPtS, &sGeom->mpolygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTomai( sPtE, &sGeom->mpolygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::slbTogcb( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    stdPoint2D*         sPtS;
    stdPoint2D*         sPtE;
    UInt                i, sMax;
    SChar               sRet = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sPtS = STD_FIRST_PT2D(aObj1);
    sPtE = STD_LAST_PT2D(aObj1);
    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = spiToslb( sPtS, &sGeom->linestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiToslb( sPtE, &sGeom->linestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = spiTosab( sPtS, &sGeom->polygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTosab( sPtE, &sGeom->polygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = spiTomlb( sPtS, &sGeom->mlinestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTomlb( sPtE, &sGeom->mlinestring2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = spiTomab( sPtS, &sGeom->mpolygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            sRet = spiTomab( sPtE, &sGeom->mpolygon2D );
            if( sRet == '0' )
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::slbTogce( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    stdPoint2D*         sPtS;
    stdPoint2D*         sPtE;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sPtS = STD_FIRST_PT2D(aObj1);
    sPtE = STD_LAST_PT2D(aObj1);
    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            if( (spiTospe( sPtS, &sGeom->point2D.mPoint ) == '0') ||
                (spiTospe( sPtE, &sGeom->point2D.mPoint ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            if( (spiTosle( sPtS, &sGeom->linestring2D ) == '0') ||
                (spiTosle( sPtE, &sGeom->linestring2D ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            if( (spiTosae( sPtS, &sGeom->polygon2D ) == '0') ||
                (spiTosae( sPtE, &sGeom->polygon2D ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            if( (spiTompe( sPtS, &sGeom->mpoint2D ) == '0') ||
                (spiTompe( sPtE, &sGeom->mpoint2D ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            if( (spiTomle( sPtS, &sGeom->mlinestring2D ) == '0') ||
                (spiTomle( sPtE, &sGeom->mlinestring2D ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            if( (spiTomae( sPtS, &sGeom->mpolygon2D ) == '0') ||
                (spiTomae( sPtE, &sGeom->mpolygon2D ) == '0') )
            {
                sRet = '0';
            }
            else
            {
                sRet = 'F';
            }
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sleTogci( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTosle( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTosle( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            return '2';
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = sleTompi( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sleTomli( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            return '2';
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sleTogcb( const stdLineString2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTosle( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sleTosab( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = sleTompi( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sleTomli( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sleTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

/***********************************************************************
 * Description:
 *
 *    SAI(Single Area Internal)��
 *    SAI(Single Area Internal)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   BUG-17003
 *
 *   Polygon�� �����ϴ� [��, ����, ��] ��
 *   Polygon�� �����ϴ� [��, ����, ��] �� ���踦 ���� ���س���.
 *
 *   ================================
 *     Polygon(A) .vs. Polygon(B)
 *   ================================
 *
 *   1. �� .vs. ��
 *
 *       - ��� ���� �����ϴ� ���� ���� : �ٸ� ����κ��� ����
 *       - �Ѹ�� �ٸ� ���� ������ �����ϴ� ��� : TRUE
 *
 *          A-------A
 *          |       |
 *          | B---B |
 *          | |   | |
 *          | B---B |
 *          |       |
 *          A-------A
 *
 *   1. ���� .vs. ��
 *
 *       - ���а� ����, ���� ���� ����κ��� ����
 *
 *   2. �� .vs. �� 
 *       - ���� �ٸ� Polygon�� �ܺο� ���� : ���� �Ұ�
 *       - ���� �ٸ� Polygon�� ���ο� ���� : TRUE
 *
 *                A--------A
 *                |        |
 *                |   B    |
 *                |        |
 *                A--------A
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             B---A====B----A
 *
 *             B---A====A----B
 *
 *             A---B====B----A
 *
 *       - ������ ������ ���� : TRUE
 *
 *                B
 *                |
 *            A---+----A
 *                |
 *                B
 *
 *          SLI .vs. SAI�� �޸� ������ ���� ��쿡�� TRUE��.
 *
 *                B
 *                |
 *            A---I----A
 *               /|\
 *              I B I
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Bp                     Bp
 *               |                      |
 *         A-----B------A  ==>   Ap----AB----An  
 *               |                      |
 *               Bn                     Bn
 *
 *   5. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Bp
 *                |
 *         Ap----AB-----An
 *                |
 *                Bn
 *
 *   ���� ���� ���迡�� ������ ���� ���ο� �ִ� �ϴ���
 *   ������ Interia Ring ������ ���ο����� �������� ������ �� ����.
 *   ����, ������ Line�� Interior Ring���ο� ���Ե��� ������ �˻��ؾ� ��.
 *
 *      Ap  
 *       \     B
 *        \
 *         \   
 *          A------An              
 *
 *      Ap     
 *       \   I B B    I
 *        \  |     . ^ 
 *         \ | . ^
 *          AB-------An

 ***********************************************************************/

IDE_RC stfRelation::saiTosai( iduMemory *              aQmxMem,
                              const stdPolygon2DType * a1stArea,
                              const stdPolygon2DType * a2ndArea,
                              SChar *                  aResult )
{
    UInt i, j;
    UInt m, n;
    UInt x, y;
    
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // First Area ����
    //----------------------------
    
    stdLinearRing2D * s1stRing;
    stdPoint2D      * s1stRingPt;
    stdPoint2D      * s1stRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * s1stRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * s1stRingNextPt;  // Ring Point�� ���� Point
    
    UInt              s1stRingCnt;     // Ring Count of a Polygon
    UInt              s1stRingSegCnt;  // Segment Count of a Ring
    idBool            s1stRingCCWise;  // Ring �� �ð� ������������ ����

    stdPoint2D        s1stSomePt;
    
    //----------------------------
    // Second Area ����
    //----------------------------
    
    stdLinearRing2D * s2ndRing;
    stdPoint2D      * s2ndRingPt;
    stdPoint2D      * s2ndRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * s2ndRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * s2ndRingNextPt;  // Ring Point�� ���� Point
    
    UInt              s2ndRingCnt;     // Ring Count of a Polygon
    UInt              s2ndRingSegCnt;  // Segment Count of a Ring

    stdPoint2D        s2ndSomePt;

    //----------------------------
    // Ring�� Ring�� �ߺ������� �˻�
    //----------------------------
    
    UInt              sCheckSegCnt;
    stdLinearRing2D * sCheckRing;
    stdPoint2D      * sCheckPt;
    
    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( a1stArea != NULL );
    IDE_DASSERT( a2ndArea != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    s1stRingCnt = STD_N_RINGS( a1stArea );
    s2ndRingCnt = STD_N_RINGS( a2ndArea );

    //----------------------------------------
    // ��� ���� ����κ��� ����
    //----------------------------------------

    IDE_TEST( stdUtils::getPointOnSurface2D( aQmxMem, a1stArea, & s1stSomePt )
              != IDE_SUCCESS );
    IDE_TEST( stdUtils::getPointOnSurface2D( aQmxMem, a2ndArea, & s2ndSomePt )
              != IDE_SUCCESS );
    
    if( ( spiTosai( &s1stSomePt, a2ndArea ) == '0' ) ||
        ( spiTosai( &s2ndSomePt, a1stArea ) == '0' ) )
    {
        sResult = '2';
        IDE_RAISE( SAISAI2D_MAX_RESULT );
    }
    
    //----------------------------------------
    // ���� ���� ����κ��� ����
    //----------------------------------------

    // First Area�� ��ǥ�� Second Area�� ���ο� �ִ��� �˻�
    for ( i = 0, s1stRing = STD_FIRST_RN2D(a1stArea);
          i < s1stRingCnt;
          i++, s1stRing = STD_NEXT_RN2D(s1stRing) )
    {
        s1stRingSegCnt = STD_N_POINTS(s1stRing) - 1;
        
        for ( j = 0, s1stRingPt = STD_FIRST_PT2D(s1stRing);
              j < s1stRingSegCnt;
              j++, s1stRingPt = STD_NEXT_PT2D(s1stRingPt) )
        {
            // ������ �ٸ� Polygon�� ���ο� �����ϴ��� �Ǵ�
            if( spiTosai( s1stRingPt, a2ndArea ) == '0' )
            {
                sResult = '2';
                IDE_RAISE( SAISAI2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    
    // Second Area�� ��ǥ�� First Area�� ���ο� �ִ��� �˻�
    for ( m = 0, s2ndRing = STD_FIRST_RN2D(a2ndArea);
          m < s2ndRingCnt;
          m++, s2ndRing = STD_NEXT_RN2D(s2ndRing) )
    {
        s2ndRingSegCnt = STD_N_POINTS(s2ndRing) - 1;
        
        for ( n = 0, s2ndRingPt = STD_FIRST_PT2D(s2ndRing);
              n < s2ndRingSegCnt;
              n++, s2ndRingPt = STD_NEXT_PT2D(s2ndRingPt) )
        {
            // ������ �ٸ� Polygon�� ���ο� �����ϴ��� �Ǵ�
            if( spiTosai( s2ndRingPt, a1stArea ) == '0' )
            {
                sResult = '2';
                IDE_RAISE( SAISAI2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }       
    
    //----------------------------------------
    // First Area�� Second Area �� ���� ����
    //----------------------------------------

    // First Area�� �����ϴ� Ring�� ������ŭ �ݺ�
    for ( i = 0, s1stRing = STD_FIRST_RN2D(a1stArea);
          i < s1stRingCnt;
          i++, s1stRing = STD_NEXT_RN2D(s1stRing) )
    {
        s1stRingSegCnt = STD_N_POINTS(s1stRing) - 1;
        s1stRingCCWise = stdUtils::isCCW2D(s1stRing);
        
        // First Area�� Ring�� �����ϴ� Segment������ŭ �ݺ�
        for ( j = 0, s1stRingPt = STD_FIRST_PT2D(s1stRing);
              j < s1stRingSegCnt;
              j++, s1stRingPt = STD_NEXT_PT2D(s1stRingPt) )
        {
            // Second Area�� �����ϴ� Ring�� ������ŭ �ݺ�
            for ( m = 0, s2ndRing = STD_FIRST_RN2D(a2ndArea);
                  m < s2ndRingCnt;
                  m++, s2ndRing = STD_NEXT_RN2D(s2ndRing) )
            {
                s2ndRingSegCnt = STD_N_POINTS(s2ndRing) - 1;
                
                // Second Area�� Ring�� �����ϴ� Segment������ŭ �ݺ�
                for ( n = 0, s2ndRingPt = STD_FIRST_PT2D(s2ndRing);
                      n < s2ndRingSegCnt;
                      n++, s2ndRingPt = STD_NEXT_PT2D(s2ndRingPt) )
                {
                    //----------------------------
                    // First Ring�� ������, ������, �������� ����
                    //----------------------------

                    s1stRingPrevPt =
                        stdUtils::findPrevPointInRing2D( s1stRingPt,
                                                         j,
                                                         s1stRingSegCnt,
                                                         NULL );
                    s1stRingCurrPt = s1stRingPt;
                    s1stRingNextPt =
                        stdUtils::findNextPointInRing2D( s1stRingPt,
                                                         j,
                                                         s1stRingSegCnt,
                                                         NULL );
                    
                    //----------------------------
                    // Second Ring�� ������, ������, �������� ����
                    //----------------------------
                
                    s2ndRingPrevPt =
                        stdUtils::findPrevPointInRing2D( s2ndRingPt,
                                                         n,
                                                         s2ndRingSegCnt,
                                                         NULL );
                    s2ndRingCurrPt = s2ndRingPt;
                    s2ndRingNextPt =
                        stdUtils::findNextPointInRing2D( s2ndRingPt,
                                                         n,
                                                         s2ndRingSegCnt,
                                                         NULL );

                    //------------------------------------
                    // ���а� ������ ����κ��� ����
                    //------------------------------------

                    // ������ ������ �����Ѵٸ� TRUE
                    if( ( stdUtils::intersectI2D( s1stRingPt,
                                                  STD_NEXT_PT2D(s1stRingPt),
                                                  s2ndRingPt,
                                                  STD_NEXT_PT2D(s2ndRingPt) )
                          ==ID_TRUE ) )
                    {
                        // ������ �����ϴ� ���
                        sResult = '2';
                        IDE_RAISE( SAISAI2D_MAX_RESULT );
                    }
                    else
                    {
                        // ������ �������� �ʴ� ���
                    }
                    
                    //----------------------------------------
                    // ���� ���踦 ���� ���� ����� ����� ���� ����
                    //  - Ring�� ���а� ������ ����
                    //  - Ring�� ���� ���� ����
                    //----------------------------------------

                    sMeetOnPoint = ID_FALSE;
                    
                    //----------------------------
                    // First Ring ���г��� Second Ring�� ���� �����ϴ��� �˻�
                    //----------------------------
                    
                    if ( ( stdUtils::betweenI2D( s1stRingPt,
                                                 STD_NEXT_PT2D(s1stRingPt),
                                                 s2ndRingPt )==ID_TRUE ) )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Bp                    Bp
                        //               |                     |
                        //         A-----B-----A  ==>   Ap----AB----An  
                        //               |                     |
                        //               Bn                    Bn
                        sMeetOnPoint = ID_TRUE;
                        
                        s1stRingPrevPt = s1stRingPt;
                        s1stRingCurrPt = s2ndRingPt;
                        s1stRingNextPt = STD_NEXT_PT2D(s1stRingPt);
                    }
                
                    //----------------------------
                    // Second Ring ���г��� First Ring�� ���� �����ϴ��� �˻�
                    //----------------------------

                    if ( stdUtils::betweenI2D( s2ndRingPt,
                                               STD_NEXT_PT2D(s2ndRingPt),
                                               s1stRingPt ) == ID_TRUE )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Ap                     Ap
                        //               |                      |
                        //         B-----A------B  ==>   Bp----BA----Bn  
                        //               |                      |
                        //               An                     An
                        sMeetOnPoint = ID_TRUE;
                        
                        s2ndRingPrevPt = s2ndRingPt;
                        s2ndRingCurrPt = s1stRingPt;
                        s2ndRingNextPt = STD_NEXT_PT2D(s2ndRingPt);
                    }

                    //----------------------------
                    // ���� ���� �����ϴ� �� �˻�
                    //----------------------------
                    
                    if ( stdUtils::isSamePoints2D( s1stRingPt,
                                                   s2ndRingPt ) == ID_TRUE )
                    {
                        sMeetOnPoint = ID_TRUE;
                        
                        // �̹� ������ ���� ���
                    }

                    //----------------------------
                    // Ring�� Ring�� ��ġ�� ������ ���θ� �˻�
                    //----------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        // Ring�� Ring�� ��ġ�� ���� ���
                        // ���ο� �����ϴ� ���� �Ǵ��� �� ����.
                        // �ٸ� ���� ���Ͽ� �Ǻ� �����ϴ�.
                        //
                        //              Pn
                        //       Ap       I
                        //        \   I  /  Pn
                        //         \  | /
                        //          \ |/
                        //           AIP-------An

                        // First Area�� ���� �ٸ� Ring�� ��ġ���� �˻�
                        for ( x = 0, sCheckRing = STD_FIRST_RN2D(a1stArea);
                              x < s1stRingCnt;
                              x++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                        {
                            if ( i == x )
                            {
                                // �ڽ��� Ring�� �˻����� ����
                                continue;
                            }
                            else
                            {
                                sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                                
                                for ( y=0, sCheckPt=STD_FIRST_PT2D(sCheckRing);
                                      y < sCheckSegCnt;
                                      y++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                                {
                                    if ( stdUtils::between2D(
                                             sCheckPt,
                                             STD_NEXT_PT2D(sCheckPt),
                                             s1stRingCurrPt ) == ID_TRUE )
                                    {
                                        sMeetOnPoint = ID_FALSE;
                                        break;
                                    }
                                }
                                
                                if ( sMeetOnPoint != ID_TRUE )
                                {
                                    break;
                                }
                            }
                        }

                        if ( sMeetOnPoint != ID_TRUE )
                        {
                            continue;
                        }
                        
                        // First Area�� ���� �ٸ� Ring�� ��ġ���� �˻�
                        for ( x = 0, sCheckRing = STD_FIRST_RN2D(a2ndArea);
                              x < s2ndRingCnt;
                              x++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                        {
                            if ( m == x )
                            {
                                // �ڽ��� Ring�� �˻����� ����
                                continue;
                            }
                            else
                            {
                                sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                                
                                for ( y=0, sCheckPt=STD_FIRST_PT2D(sCheckRing);
                                      y < sCheckSegCnt;
                                      y++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                                {
                                    if ( stdUtils::between2D(
                                             sCheckPt,
                                             STD_NEXT_PT2D(sCheckPt),
                                             s2ndRingCurrPt ) == ID_TRUE )
                                    {
                                        sMeetOnPoint = ID_FALSE;
                                        break;
                                    }
                                }
                                
                                if ( sMeetOnPoint != ID_TRUE )
                                {
                                    break;
                                }
                            }
                        }
                    }
                    
                    //----------------------------------------
                    // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                    //----------------------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        if ( hasRelLineSegRingSeg(
                                 (i == 0 ) ? ID_TRUE : ID_FALSE, // Exterior
                                 s1stRingCCWise,
                                 s1stRingPrevPt,
                                 s1stRingNextPt,
                                 s1stRingCurrPt,
                                 s2ndRingPrevPt,
                                 s2ndRingNextPt,
                                 STF_INSIDE_ANGLE_POS ) == ID_TRUE )
                        {
                            sResult = '2';
                            IDE_RAISE( SAISAI2D_MAX_RESULT );
                        }
                        else
                        {
                            // ���� ���θ� �Ǵ��� �� ����
                        }
                    }
                    else // sMeetOnPoint == ID_FALSE
                    {
                        // �˻� ����� �ƴ�
                    }
                } // for n
            } // for m
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SAISAI2D_MAX_RESULT);

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/***********************************************************************
 * Description:
 *
 *    SAI(Single Area Internal)��
 *    SAB(Single Area Boundary)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   BUG-17010
 *
 *   Polygon Boundary�� �����ϴ� [��, ����] ��
 *   Polygon Internal�� �����ϴ� [��, ����, ��] �� ���踦 ���� ���س���.
 *
 *   ================================
 *     Boundary .vs. Internal
 *   ================================
 *
 *   1. ���� .vs. ��
 *
 *       - ���а� ����, ���� ���� ����κ��� ����
 *
 *   2. �� .vs. �� 
 *       - ���� Polygon�� �ܺο� ���� : ���� �Ұ�
 *       - ���� Polygon�� ���ο� ���� : TRUE
 *
 *                A--------A
 *                |        |
 *                |   B    |
 *                |        |
 *                A--------A
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             B---A====B----A
 *
 *             B---A====A----B
 *
 *             A---B====B----A
 *
 *       - ������ ������ ���� : TRUE
 *
 *                B
 *                |
 *            A---+----A
 *                |
 *                B
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Bp                     Bp
 *               |                      |
 *         A-----B------A  ==>   Ap----AB----An  
 *               |                      |
 *               Bn                     Bn
 *
 *   5. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Bp
 *                |
 *         Ap----AB-----An
 *                |
 *                Bn
 *
 *   ���� ���� ���迡�� ������ ���� ���ο� �ִ� �ϴ���
 *   ������ Interia Ring ������ ���ο����� �������� ������ �� ����.
 *   ����, ������ Line�� Interior Ring���ο� ���Ե��� ������ �˻��ؾ� ��.
 *
 *      Ap  
 *       \     B
 *        \
 *         \   
 *          A------An              
 *
 *      Ap     
 *       \   I B B    I
 *        \  |     . ^ 
 *         \ | . ^
 *          AB-------An

 ***********************************************************************/

SChar
stfRelation::saiTosab( const stdPolygon2DType * aAreaInt,
                       const stdPolygon2DType * aAreaBnd )
{
    UInt i, j;
    UInt m, n;
    UInt x, y;
    
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // Area Internal ����
    //----------------------------
    
    stdLinearRing2D * sAreaRing;
    stdPoint2D      * sAreaRingPt;
    stdPoint2D      * sAreaRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sAreaRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sAreaRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sAreaRingCnt;     // Ring Count of a Polygon
    UInt              sAreaRingSegCnt;  // Segment Count of a Ring
    idBool            sAreaRingCCWise;  // Ring �� �ð� ������������ ����

    //----------------------------
    // Area Boundary ����
    //----------------------------
    
    stdLinearRing2D * sBndRing;
    stdPoint2D      * sBndRingPt;
    stdPoint2D      * sBndRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sBndRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sBndRingCnt;     // Ring Count of a Polygon
    UInt              sBndRingSegCnt;  // Segment Count of a Ring

    //----------------------------
    // Ring�� Ring�� �ߺ������� �˻�
    //----------------------------
    
    UInt              sCheckSegCnt;
    stdLinearRing2D * sCheckRing;
    stdPoint2D      * sCheckPt;
    
    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aAreaInt != NULL );
    IDE_DASSERT( aAreaBnd != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    sAreaRingCnt = STD_N_RINGS( aAreaInt );
    sBndRingCnt = STD_N_RINGS( aAreaBnd );

    //----------------------------------------
    // ���� ���� ����κ��� ����
    //----------------------------------------

    // Area Boundary�� ��ǥ�� Area Internal�� ���ԵǴ� �� �˻�
    for ( i = 0, sBndRing = STD_FIRST_RN2D(aAreaBnd);
          i < sBndRingCnt;
          i++, sBndRing = STD_NEXT_RN2D(sBndRing) )
    {
        sBndRingSegCnt = STD_N_POINTS(sBndRing) - 1;
        
        for ( j = 0, sBndRingPt = STD_FIRST_PT2D(sBndRing);
              j < sBndRingSegCnt;
              j++, sBndRingPt = STD_NEXT_PT2D(sBndRingPt) )
        {
            // ������ �ٸ� Polygon�� ���ο� �����ϴ��� �Ǵ�
            if( spiTosai( sBndRingPt, aAreaInt ) == '0' )
            {
                sResult = '1';
                IDE_RAISE( SAISAB2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    
    //----------------------------------------
    // ���� ���� ����κ��� SAI .vs. SAB �� ���� ����
    //----------------------------------------

    // Area Boundary�� �����ϴ� Ring�� ������ŭ �ݺ�
    for ( i = 0, sBndRing = STD_FIRST_RN2D(aAreaBnd);
          i < sBndRingCnt;
          i++, sBndRing = STD_NEXT_RN2D(sBndRing) )
    {
        sBndRingSegCnt = STD_N_POINTS(sBndRing) - 1;
        
        // Boundary�� Ring�� �����ϴ� Segment������ŭ �ݺ�
        for ( j = 0, sBndRingPt = STD_FIRST_PT2D(sBndRing);
              j < sBndRingSegCnt;
              j++, sBndRingPt = STD_NEXT_PT2D(sBndRingPt) )
        {
            // Area Internal�� �����ϴ� Ring�� ������ŭ �ݺ�
            for ( m = 0, sAreaRing = STD_FIRST_RN2D(aAreaInt);
                  m < sAreaRingCnt;
                  m++, sAreaRing = STD_NEXT_RN2D(sAreaRing) )
            {
                sAreaRingSegCnt = STD_N_POINTS(sAreaRing) - 1;
                sAreaRingCCWise = stdUtils::isCCW2D(sAreaRing);
                
                // Ring�� �����ϴ� Segment������ŭ �ݺ�
                for ( n = 0, sAreaRingPt = STD_FIRST_PT2D(sAreaRing);
                      n < sAreaRingSegCnt;
                      n++, sAreaRingPt = STD_NEXT_PT2D(sAreaRingPt) )
                {
                    //----------------------------
                    // Area Ring�� ������, ������, �������� ����
                    //----------------------------

                    sAreaRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sAreaRingPt,
                                                         n,
                                                         sAreaRingSegCnt,
                                                         NULL );
                    sAreaRingCurrPt = sAreaRingPt;
                    sAreaRingNextPt =
                        stdUtils::findNextPointInRing2D( sAreaRingPt,
                                                         n,
                                                         sAreaRingSegCnt,
                                                         NULL );
                    
                    //----------------------------
                    // Boundary Ring�� ������, ������, �������� ����
                    //----------------------------
                
                    sBndRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sBndRingPt,
                                                         j,
                                                         sBndRingSegCnt,
                                                         NULL );
                    
                    sBndRingNextPt =
                        stdUtils::findNextPointInRing2D( sBndRingPt,
                                                         j,
                                                         sBndRingSegCnt,
                                                         NULL );

                    //------------------------------------
                    // ���а� ������ ����κ��� ����
                    //------------------------------------

                    // ������ ������ �����Ѵٸ� TRUE
                    if( ( stdUtils::intersectI2D( sAreaRingPt,
                                                  STD_NEXT_PT2D(sAreaRingPt),
                                                  sBndRingPt,
                                                  STD_NEXT_PT2D(sBndRingPt) )
                          ==ID_TRUE ) )
                    {
                        // ������ �����ϴ� ���
                        sResult = '1';
                        IDE_RAISE( SAISAB2D_MAX_RESULT );
                    }
                    else
                    {
                        // ������ �������� �ʴ� ���
                    }
                    
                    //----------------------------------------
                    // ���� ���踦 ���� ���� ����� ����� ���� ����
                    //  - Ring�� ���а� ������ ����
                    //  - Ring�� ���� ���� ����
                    //----------------------------------------

                    sMeetOnPoint = ID_FALSE;
                    
                    //----------------------------
                    // Area Ring ���г��� Boundary Ring�� ���� �����ϴ��� �˻�
                    //----------------------------
                    
                    if ( ( stdUtils::betweenI2D( sAreaRingPt,
                                                 STD_NEXT_PT2D(sAreaRingPt),
                                                 sBndRingPt )==ID_TRUE ) )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Bp                    Bp
                        //               |                     |
                        //         A-----B-----A  ==>   Ap----AB----An  
                        //               |                     |
                        //               Bn                    Bn
                        sMeetOnPoint = ID_TRUE;
                        
                        sAreaRingPrevPt = sAreaRingPt;
                        sAreaRingCurrPt = sBndRingPt;
                        sAreaRingNextPt = STD_NEXT_PT2D(sAreaRingPt);
                    }
                
                    //----------------------------
                    // Boundary Ring ���г��� Area Ring�� ���� �����ϴ��� �˻�
                    //----------------------------

                    if ( stdUtils::betweenI2D( sBndRingPt,
                                               STD_NEXT_PT2D(sBndRingPt),
                                               sAreaRingPt ) == ID_TRUE )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Ap                     Ap
                        //               |                      |
                        //         B-----A------B  ==>   Bp----BA----Bn  
                        //               |                      |
                        //               An                     An
                        sMeetOnPoint = ID_TRUE;
                        
                        sBndRingPrevPt = sBndRingPt;
                        sBndRingNextPt = STD_NEXT_PT2D(sBndRingPt);
                    }

                    //----------------------------
                    // ���� ���� �����ϴ� �� �˻�
                    //----------------------------
                    
                    if ( stdUtils::isSamePoints2D( sAreaRingPt,
                                                   sBndRingPt ) == ID_TRUE )
                    {
                        sMeetOnPoint = ID_TRUE;
                        
                        // �̹� ������ ���� ���
                    }

                    //----------------------------
                    // Ring�� Ring�� ��ġ�� ������ ���θ� �˻�
                    //----------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        // Area�� �����ϴ� Ring���� ��ġ�� ���� ���
                        // ���ο� �����ϴ� ���� �Ǵ��� �� ����.
                        // �ٸ� ���� ���Ͽ� �Ǻ� �����ϴ�.
                        //
                        //              Bn
                        //       Ap       I
                        //        \   I  /  Bn
                        //         \  | /
                        //          \ |/
                        //           AIB-------An

                        // First Area�� ���� �ٸ� Ring�� ��ġ���� �˻�
                        for ( x = 0, sCheckRing = STD_FIRST_RN2D(aAreaInt);
                              x < sAreaRingCnt;
                              x++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                        {
                            if ( m == x )
                            {
                                // �ڽ��� Ring�� �˻����� ����
                                continue;
                            }
                            else
                            {
                                sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                                
                                for ( y=0, sCheckPt=STD_FIRST_PT2D(sCheckRing);
                                      y < sCheckSegCnt;
                                      y++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                                {
                                    if ( stdUtils::between2D(
                                             sCheckPt,
                                             STD_NEXT_PT2D(sCheckPt),
                                             sAreaRingCurrPt ) == ID_TRUE )
                                    {
                                        sMeetOnPoint = ID_FALSE;
                                        break;
                                    }
                                }
                                
                                if ( sMeetOnPoint != ID_TRUE )
                                {
                                    break;
                                }
                            }
                        }
                    }
                    
                    //----------------------------------------
                    // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                    //----------------------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        if ( hasRelLineSegRingSeg(
                                 ( m == 0 ) ? ID_TRUE : ID_FALSE, // Exterior
                                 sAreaRingCCWise,
                                 sAreaRingPrevPt,
                                 sAreaRingNextPt,
                                 sAreaRingCurrPt,
                                 sBndRingPrevPt,
                                 sBndRingNextPt,
                                 STF_INSIDE_ANGLE_POS ) == ID_TRUE )
                        {
                            sResult = '1';
                            IDE_RAISE( SAISAB2D_MAX_RESULT );
                        }
                        else
                        {
                            // ���� ���θ� �Ǵ��� �� ����
                        }
                    }
                    else // sMeetOnPoint == ID_FALSE
                    {
                        // �˻� ����� �ƴ�
                    }
                } // for n
            } // for m
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SAISAB2D_MAX_RESULT);
    
    return sResult;
}

/***********************************************************************
 * Description:
 *
 *    SAI(Single Area Internal)��
 *    SAE(Single Area External)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   BUG-17037
 *
 *   Polygon�� �����ϴ� [��, ����, ��] ��
 *   Polygon�� �����ϴ� [��, ����, ��] �� ���踦 ���� ���س���.
 *
 *   ================================
 *     Polygon Interior .vs. Polygon Exterior
 *   ================================
 *
 *   1. �� .vs. ��
 *
 *       - AreaInt ������ ������ AreaExt �ܺθ�� �����ϴ����� ���� : TRUE
 *
 *          A----------A
 *          |          |           X-----X
 *          | IX====IX |           |  X  |
 *          | ||    || |           |     |   A---A
 *          | || X  || |           X-----X   |   |
 *          | IX====IX |                     A---A
 *          |          |
 *          A----------A
 *
 *   1. ���� .vs. ��
 *
 *       - ���а� ����, ���� ���� ����κ��� ����
 *
 *   2. �� .vs. ��
 *
 *       - AreaInt�� ������ AreaExt�� �ܺο� ���� : TRUE
 *
 *                X--------X
 *            A   |        |
 *                |        |
 *                |        |
 *                X--------X
 *
 *       - AreaExt�� ���θ��� ������ AreaInt�� ���ο� ���� : TRUE
 *
 *
 *        X-------------X
 *        | A---------A |
 *        | |         | |
 *        | |   X--X  | |
 *        | |   |  |  | |
 *        | |   X--X  | |
 *        | |         | |
 *        | A---------A |
 *        X-------------X
 *
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             B---A====B----A
 *
 *             B---A====A----B
 *
 *             A---B====B----A
 *
 *       - ������ ������ ���� : TRUE
 *
 *                B
 *                |
 *            A---+----A
 *                |
 *                B
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Bp                     Bp
 *               |                      |
 *         A-----B------A  ==>   Ap----AB----An  
 *               |                      |
 *               Bn                     Bn
 *
 *   5. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Bp
 *                |
 *         Ap----AB-----An
 *                |
 *                Bn
 *
 *   ���� ���� ���迡�� ������ ���� ���ο� �ִ� �ϴ���
 *   ������ Interia Ring ������ ���ο����� �������� ������ �� ����.
 *   ����, ������ Line�� Interior Ring���ο� ���Ե��� ������ �˻��ؾ� ��.
 *
 *      Ap  
 *       \     B
 *        \
 *         \   
 *          A------An              
 *
 *      Ap     
 *       \   I B B    I
 *        \  |     . ^ 
 *         \ | . ^
 *          AB-------An
 *
 ***********************************************************************/

IDE_RC stfRelation::saiTosae( iduMemory *              aQmxMem,
                              const stdPolygon2DType * aAreaInt,
                              const stdPolygon2DType * aAreaExt,
                              SChar *                  aResult )
{
    UInt i, j;
    UInt m, n;
    UInt x, y;
    
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // Internal Area ����
    //----------------------------
    
    stdLinearRing2D * sIntRing;
    stdPoint2D      * sIntRingPt;
    stdPoint2D      * sIntRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sIntRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sIntRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sIntRingCnt;     // Ring Count of a Polygon
    UInt              sIntRingSegCnt;  // Segment Count of a Ring

    stdPoint2D        sIntSomePt;
    
    //----------------------------
    // External Area ����
    //----------------------------
    
    stdLinearRing2D * sExtRing;
    stdPoint2D      * sExtRingPt;
    stdPoint2D      * sExtRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sExtRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sExtRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sExtRingCnt;     // Ring Count of a Polygon
    UInt              sExtRingSegCnt;  // Segment Count of a Ring
    idBool            sExtRingCCWise;  // Ring �� �ð� ������������ ����

    //----------------------------
    // Ring�� Ring�� �ߺ������� �˻�
    //----------------------------
    
    UInt              sCheckSegCnt;
    stdLinearRing2D * sCheckRing;
    stdPoint2D      * sCheckPt;
    
    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aAreaInt != NULL );
    IDE_DASSERT( aAreaExt != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    sIntRingCnt = STD_N_RINGS( aAreaInt );
    sExtRingCnt = STD_N_RINGS( aAreaExt );

    //----------------------------------------
    // ��� ���� ����κ��� ����
    //----------------------------------------

    // AreaInt ������ ����� AreaExt�� �ܺο� �����ϴ� �� �˻�
    IDE_TEST( stdUtils::getPointOnSurface2D( aQmxMem, aAreaInt, & sIntSomePt )
              != IDE_SUCCESS );
    
    if( spiTosae( &sIntSomePt, aAreaExt ) == '0' )
    {
        sResult = '2';
        IDE_RAISE( SAISAE2D_MAX_RESULT );
    }
    
    //----------------------------------------
    // ���� ���� ����κ��� ����
    //----------------------------------------

    // AreaInt�� ��ǥ�� AreaExt�� �ܺο� �ִ��� �˻�
    for ( i = 0, sIntRing = STD_FIRST_RN2D(aAreaInt);
          i < sIntRingCnt;
          i++, sIntRing = STD_NEXT_RN2D(sIntRing) )
    {
        sIntRingSegCnt = STD_N_POINTS(sIntRing) - 1;
        
        for ( j = 0, sIntRingPt = STD_FIRST_PT2D(sIntRing);
              j < sIntRingSegCnt;
              j++, sIntRingPt = STD_NEXT_PT2D(sIntRingPt) )
        {
            // ������ �ٸ� Polygon�� �ܺο� �����ϴ��� �Ǵ�
            if( spiTosae( sIntRingPt, aAreaExt ) == '0' )
            {
                sResult = '2';
                IDE_RAISE( SAISAE2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    // AreaExt�� ���θ��� �� ��ǥ�� AreaInt�� ���ο� �ִ��� ����
    for ( m = 0, sExtRing = STD_FIRST_RN2D(aAreaExt);
          m < sExtRingCnt;
          m++, sExtRing = STD_NEXT_RN2D(sExtRing) )
    {
        if ( m == 0 )
        {
            // �ܺθ��� �˻����� ����
            continue;
        }
        
        sExtRingSegCnt = STD_N_POINTS(sExtRing) - 1;
        
        for ( n = 0, sExtRingPt = STD_FIRST_PT2D(sExtRing);
              n < sExtRingSegCnt;
              n++, sExtRingPt = STD_NEXT_PT2D(sExtRingPt) )
        {
            // ������ �ٸ� Polygon�� �ܺο� �����ϴ��� �Ǵ�
            if( spiTosai( sExtRingPt, aAreaInt ) == '0' )
            {
                sResult = '2';
                IDE_RAISE( SAISAE2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    
    //----------------------------------------
    // Internal Area�� External Area �� ���� ����
    //----------------------------------------

    // Internal Area�� �����ϴ� Ring�� ������ŭ �ݺ�
    for ( i = 0, sIntRing = STD_FIRST_RN2D(aAreaInt);
          i < sIntRingCnt;
          i++, sIntRing = STD_NEXT_RN2D(sIntRing) )
    {
        sIntRingSegCnt = STD_N_POINTS(sIntRing) - 1;
        
        // Ring�� �����ϴ� Segment������ŭ �ݺ�
        for ( j = 0, sIntRingPt = STD_FIRST_PT2D(sIntRing);
              j < sIntRingSegCnt;
              j++, sIntRingPt = STD_NEXT_PT2D(sIntRingPt) )
        {
            // External Area�� �����ϴ� Ring�� ������ŭ �ݺ�
            for ( m = 0, sExtRing = STD_FIRST_RN2D(aAreaExt);
                  m < sExtRingCnt;
                  m++, sExtRing = STD_NEXT_RN2D(sExtRing) )
            {
                sExtRingSegCnt = STD_N_POINTS(sExtRing) - 1;
                sExtRingCCWise = stdUtils::isCCW2D(sExtRing);
                
                // Ring�� �����ϴ� Segment������ŭ �ݺ�
                for ( n = 0, sExtRingPt = STD_FIRST_PT2D(sExtRing);
                      n < sExtRingSegCnt;
                      n++, sExtRingPt = STD_NEXT_PT2D(sExtRingPt) )
                {
                    //----------------------------
                    // Internal Area Ring�� ������, ������, �������� ����
                    //----------------------------

                    sIntRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sIntRingPt,
                                                         j,
                                                         sIntRingSegCnt,
                                                         NULL );
                    sIntRingCurrPt = sIntRingPt;
                    sIntRingNextPt =
                        stdUtils::findNextPointInRing2D( sIntRingPt,
                                                         j,
                                                         sIntRingSegCnt,
                                                         NULL );
                    
                    //----------------------------
                    // Exterior Area Ring�� ������, ������, �������� ����
                    //----------------------------
                
                    sExtRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sExtRingPt,
                                                         n,
                                                         sExtRingSegCnt,
                                                         NULL );
                    sExtRingCurrPt = sExtRingPt;
                    sExtRingNextPt =
                        stdUtils::findNextPointInRing2D( sExtRingPt,
                                                         n,
                                                         sExtRingSegCnt,
                                                         NULL );

                    //------------------------------------
                    // ���а� ������ ����κ��� ����
                    //------------------------------------

                    // ������ ������ �����Ѵٸ� TRUE
                    if( ( stdUtils::intersectI2D( sIntRingPt,
                                                  STD_NEXT_PT2D(sIntRingPt),
                                                  sExtRingPt,
                                                  STD_NEXT_PT2D(sExtRingPt) )
                          ==ID_TRUE ) )
                    {
                        // ������ �����ϴ� ���
                        sResult = '2';
                        IDE_RAISE( SAISAE2D_MAX_RESULT );
                    }
                    else
                    {
                        // ������ �������� �ʴ� ���
                    }
                    
                    //----------------------------------------
                    // ���� ���踦 ���� ���� ����� ����� ���� ����
                    //  - Ring�� ���а� ������ ����
                    //  - Ring�� ���� ���� ����
                    //----------------------------------------

                    sMeetOnPoint = ID_FALSE;
                    
                    //----------------------------
                    // Internal Area Ring ���г���
                    // External Area Ring �� ���� �����ϴ��� �˻�
                    //----------------------------
                    
                    if ( ( stdUtils::betweenI2D( sIntRingPt,
                                                 STD_NEXT_PT2D(sIntRingPt),
                                                 sExtRingPt )==ID_TRUE ) )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Bp                    Bp
                        //               |                     |
                        //         A-----B-----A  ==>   Ap----AB----An  
                        //               |                     |
                        //               Bn                    Bn
                        sMeetOnPoint = ID_TRUE;
                        
                        sIntRingPrevPt = sIntRingPt;
                        sIntRingCurrPt = sExtRingPt;
                        sIntRingNextPt = STD_NEXT_PT2D(sIntRingPt);
                    }
                
                    //----------------------------
                    // External Area Ring ���г���
                    // Internal Area Ring�� ���� �����ϴ��� �˻�
                    //----------------------------

                    if ( stdUtils::betweenI2D( sExtRingPt,
                                               STD_NEXT_PT2D(sExtRingPt),
                                               sIntRingPt ) == ID_TRUE )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Ap                     Ap
                        //               |                      |
                        //         B-----A------B  ==>   Bp----BA----Bn  
                        //               |                      |
                        //               An                     An
                        sMeetOnPoint = ID_TRUE;
                        
                        sExtRingPrevPt = sExtRingPt;
                        sExtRingCurrPt = sIntRingPt;
                        sExtRingNextPt = STD_NEXT_PT2D(sExtRingPt);
                    }

                    //----------------------------
                    // ���� ���� �����ϴ� �� �˻�
                    //----------------------------
                    
                    if ( stdUtils::isSamePoints2D( sIntRingPt,
                                                   sExtRingPt ) == ID_TRUE )
                    {
                        sMeetOnPoint = ID_TRUE;
                        
                        // �̹� ������ ���� ���
                    }

                    //----------------------------
                    // Ring�� Ring�� ��ġ�� ������ ���θ� �˻�
                    //----------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        // Ring�� Ring�� ��ġ�� ���� ���
                        // ���ο� �����ϴ� ���� �Ǵ��� �� ����.
                        // �ٸ� ���� ���Ͽ� �Ǻ� �����ϴ�.
                        //
                        //              Pn
                        //       Ap       I
                        //        \   I  /  Pn
                        //         \  | /
                        //          \ |/
                        //           AIP-------An

                        // Internal Area�� ���� �ٸ� Ring�� ��ġ���� �˻�
                        for ( x = 0, sCheckRing = STD_FIRST_RN2D(aAreaInt);
                              x < sIntRingCnt;
                              x++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                        {
                            if ( i == x )
                            {
                                // �ڽ��� Ring�� �˻����� ����
                                continue;
                            }
                            else
                            {
                                sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                                
                                for ( y=0, sCheckPt=STD_FIRST_PT2D(sCheckRing);
                                      y < sCheckSegCnt;
                                      y++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                                {
                                    if ( stdUtils::between2D(
                                             sCheckPt,
                                             STD_NEXT_PT2D(sCheckPt),
                                             sIntRingCurrPt ) == ID_TRUE )
                                    {
                                        sMeetOnPoint = ID_FALSE;
                                        break;
                                    }
                                }
                                
                                if ( sMeetOnPoint != ID_TRUE )
                                {
                                    break;
                                }
                            }
                        }

                        if ( sMeetOnPoint != ID_TRUE )
                        {
                            continue;
                        }
                        
                        // External Area�� ���� �ٸ� Ring�� ��ġ���� �˻�
                        for ( x = 0, sCheckRing = STD_FIRST_RN2D(aAreaExt);
                              x < sExtRingCnt;
                              x++, sCheckRing = STD_NEXT_RN2D(sCheckRing) )
                        {
                            if ( m == x )
                            {
                                // �ڽ��� Ring�� �˻����� ����
                                continue;
                            }
                            else
                            {
                                sCheckSegCnt = STD_N_POINTS(sCheckRing) - 1;
                                
                                for ( y=0, sCheckPt=STD_FIRST_PT2D(sCheckRing);
                                      y < sCheckSegCnt;
                                      y++, sCheckPt = STD_NEXT_PT2D(sCheckPt) )
                                {
                                    if ( stdUtils::between2D(
                                             sCheckPt,
                                             STD_NEXT_PT2D(sCheckPt),
                                             sExtRingCurrPt ) == ID_TRUE )
                                    {
                                        sMeetOnPoint = ID_FALSE;
                                        break;
                                    }
                                }
                                
                                if ( sMeetOnPoint != ID_TRUE )
                                {
                                    break;
                                }
                            }
                        }
                    }
                    
                    //----------------------------------------
                    // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                    //----------------------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        if ( hasRelLineSegRingSeg(
                                 (m == 0) ? ID_TRUE : ID_FALSE, // Exterior
                                 sExtRingCCWise,
                                 sExtRingPrevPt,
                                 sExtRingNextPt,
                                 sExtRingCurrPt,
                                 sIntRingPrevPt,
                                 sIntRingNextPt,
                                 STF_OUTSIDE_ANGLE_POS ) == ID_TRUE )
                        {
                            sResult = '2';
                            IDE_RAISE( SAISAE2D_MAX_RESULT );
                        }
                        else
                        {
                            // ���� ���θ� �Ǵ��� �� ����
                        }
                    }
                    else // sMeetOnPoint == ID_FALSE
                    {
                        // �˻� ����� �ƴ�
                    }
                } // for n
            } // for m
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SAISAE2D_MAX_RESULT);
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}


SChar stfRelation::sabTosab( const stdPolygon2DType*    aObj1,
                             const stdPolygon2DType*    aObj2 )
{
    stdPoint2D*             sPt1, *sPt2;
    stdLinearRing2D*        sRing1, *sRing2;
    UInt                    i,j,k,l, sMax1, sMax2, sMaxR1, sMaxR2;
    SChar                   sResult = 0x00;
    SChar                   sTemp;

    sRing1 = STD_FIRST_RN2D(aObj1);
    sMaxR1 = STD_N_RINGS(aObj1);
    for( i = 0; i < sMaxR1; i++ )
    {
        sPt1 = STD_FIRST_PT2D(sRing1);
        sMax1 = STD_N_POINTS(sRing1)-1;
        for( j = 0; j < sMax1; j++ )
        {
            sRing2 = STD_FIRST_RN2D(aObj2);
            sMaxR2 = STD_N_RINGS(aObj2);
            for( k = 0; k < sMaxR2; k++ )
            {
                sPt2 = STD_FIRST_PT2D(sRing2);
                sMax2 = STD_N_POINTS(sRing2)-1;
                for( l = 0; l < sMax2; l++ )
                {
                    // Fix Bug-15432
                    sTemp = MidLineToMidLine(
                        sPt1, STD_NEXT_PT2D(sPt1), 
                        sPt2, STD_NEXT_PT2D(sPt2));

                    if( sTemp == '1' )
                    {
                         return '1';    // ���� �� �ִ� �ְ� ����
                    }
                    else if( (sResult < '0') && (sTemp == '0') )
                    {
                        sResult = '0';
                    }

                    sPt2 = STD_NEXT_PT2D(sPt2);
                }
                sPt2 = STD_NEXT_PT2D(sPt2);
                sRing2 = (stdLinearRing2D*)sPt2;
            }
            sPt1 = STD_NEXT_PT2D(sPt1);
        }
        sPt1 = STD_NEXT_PT2D(sPt1);
        sRing1 = (stdLinearRing2D*)sPt1;
    }

    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    return sResult;
}

/***********************************************************************
 * Description:
 *
 *    SAB(Single Area Boundary)��
 *    SAE(Single Area External)�� DE-9IM ���踦 ���Ѵ�.
 *
 * Implementation:
 *
 *   BUG-17043
 *
 *   Polygon Boundary�� �����ϴ� [��, ����] ��
 *   Polygon External�� �����ϴ� [��, ����, ��] �� ���踦 ���� ���س���.
 *
 *   ================================
 *     Boundary .vs. External
 *   ================================
 *
 *   1. ���� .vs. ��
 *
 *       - ���а� ����, ���� ���� ����κ��� ����
 *
 *   2. �� .vs. �� 
 *       - ���� Polygon�� ���ο� ���� : ���� �Ұ�
 *       - ���� Polygon�� �ܺο� ���� : TRUE
 *
 *                A--------A
 *                |        |
 *                |        |    B
 *                |        |
 *                A--------A
 *                 
 *   3. ���� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *
 *       - ������ �������� ���� : ���� ���� ����κ��� ���� ����
 *
 *             B---A====B----A
 *
 *             B---A====A----B
 *
 *             A---B====B----A
 *
 *       - ������ ������ ���� : TRUE
 *
 *                B
 *                |
 *            A---+----A
 *                |
 *                B
 *
 *   4. �� .vs. ����
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ���� ���п��� ���� : ���� ���� ����κ��� ���� ����
 *
 *               Bp                     Bp
 *               |                      |
 *         A-----B------A  ==>   Ap----AB----An  
 *               |                      |
 *               Bn                     Bn
 *
 *   5. �� .vs. ��
 *       - �������� �ʴ� ��� : ���� �Ұ�
 *       - ������ ���� : ���� ���� ����κ��� ����
 *
 *                Bp
 *                |
 *         Ap----AB-----An
 *                |
 *                Bn
 *
 ***********************************************************************/

SChar
stfRelation::sabTosae( const stdPolygon2DType * aAreaBnd,
                       const stdPolygon2DType * aAreaExt )
{
    UInt i, j;
    UInt m, n;
    
    idBool  sMeetOnPoint;
    SChar   sResult;
    
    //----------------------------
    // Area External ����
    //----------------------------
    
    stdLinearRing2D * sExtRing;
    stdPoint2D      * sExtRingPt;
    stdPoint2D      * sExtRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sExtRingCurrPt;  // Ring Point�� ���� Point
    stdPoint2D      * sExtRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sExtRingCnt;     // Ring Count of a Polygon
    UInt              sExtRingSegCnt;  // Segment Count of a Ring
    idBool            sExtRingCCWise;  // Ring �� �ð� ������������ ����

    //----------------------------
    // Area Boundary ����
    //----------------------------
    
    stdLinearRing2D * sBndRing;
    stdPoint2D      * sBndRingPt;
    stdPoint2D      * sBndRingPrevPt;  // Ring Point�� ���� Point
    stdPoint2D      * sBndRingNextPt;  // Ring Point�� ���� Point
    
    UInt              sBndRingCnt;     // Ring Count of a Polygon
    UInt              sBndRingSegCnt;  // Segment Count of a Ring

    //----------------------------------------
    // Parameter Validation
    //----------------------------------------

    IDE_DASSERT( aAreaExt != NULL );
    IDE_DASSERT( aAreaBnd != NULL );
        
    //----------------------------------------
    // Initialization
    //----------------------------------------
    
    sResult = 'F';
    
    sExtRingCnt = STD_N_RINGS( aAreaExt );
    sBndRingCnt = STD_N_RINGS( aAreaBnd );

    //----------------------------------------
    // ���� ���� ����κ��� ����
    //----------------------------------------

    // AreaBnd�� ��ǥ�� AreaExt �ܺο� ���ԵǴ� �� �˻�
    for ( i = 0, sBndRing = STD_FIRST_RN2D(aAreaBnd);
          i < sBndRingCnt;
          i++, sBndRing = STD_NEXT_RN2D(sBndRing) )
    {
        sBndRingSegCnt = STD_N_POINTS(sBndRing) - 1;
        
        for ( j = 0, sBndRingPt = STD_FIRST_PT2D(sBndRing);
              j < sBndRingSegCnt;
              j++, sBndRingPt = STD_NEXT_PT2D(sBndRingPt) )
        {
            // ������ �ٸ� Polygon�� ���ο� �����ϴ��� �Ǵ�
            if( spiTosae( sBndRingPt, aAreaExt ) == '0' )
            {
                sResult = '1';
                IDE_RAISE( SABSAE2D_MAX_RESULT );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    
    //----------------------------------------
    // ���� ���� ����κ��� SAB .vs. SAE �� ���� ����
    //----------------------------------------

    // AreaBnd�� �����ϴ� Ring�� ������ŭ �ݺ�
    for ( i = 0, sBndRing = STD_FIRST_RN2D(aAreaBnd);
          i < sBndRingCnt;
          i++, sBndRing = STD_NEXT_RN2D(sBndRing) )
    {
        sBndRingSegCnt = STD_N_POINTS(sBndRing) - 1;
        
        // Boundary�� Ring�� �����ϴ� Segment������ŭ �ݺ�
        for ( j = 0, sBndRingPt = STD_FIRST_PT2D(sBndRing);
              j < sBndRingSegCnt;
              j++, sBndRingPt = STD_NEXT_PT2D(sBndRingPt) )
        {
            // AreaExt�� �����ϴ� Ring�� ������ŭ �ݺ�
            for ( m = 0, sExtRing = STD_FIRST_RN2D(aAreaExt);
                  m < sExtRingCnt;
                  m++, sExtRing = STD_NEXT_RN2D(sExtRing) )
            {
                sExtRingSegCnt = STD_N_POINTS(sExtRing) - 1;
                sExtRingCCWise = stdUtils::isCCW2D(sExtRing);
                
                // Ring�� �����ϴ� Segment������ŭ �ݺ�
                for ( n = 0, sExtRingPt = STD_FIRST_PT2D(sExtRing);
                      n < sExtRingSegCnt;
                      n++, sExtRingPt = STD_NEXT_PT2D(sExtRingPt) )
                {
                    //----------------------------
                    // AreaExt Ring�� ������, ������, �������� ����
                    //----------------------------

                    sExtRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sExtRingPt,
                                                         n,
                                                         sExtRingSegCnt,
                                                         NULL );
                    sExtRingCurrPt = sExtRingPt;
                    sExtRingNextPt =
                        stdUtils::findNextPointInRing2D( sExtRingPt,
                                                         n,
                                                         sExtRingSegCnt,
                                                         NULL );
                    
                    //----------------------------
                    // AreaBnd Ring�� ������, ������, �������� ����
                    //----------------------------
                
                    sBndRingPrevPt =
                        stdUtils::findPrevPointInRing2D( sBndRingPt,
                                                         j,
                                                         sBndRingSegCnt,
                                                         NULL );
                    sBndRingNextPt =
                        stdUtils::findNextPointInRing2D( sBndRingPt,
                                                         j,
                                                         sBndRingSegCnt,
                                                         NULL );

                    //------------------------------------
                    // ���а� ������ ����κ��� ����
                    //------------------------------------

                    // ������ ������ �����Ѵٸ� TRUE
                    if( ( stdUtils::intersectI2D( sExtRingPt,
                                                  STD_NEXT_PT2D(sExtRingPt),
                                                  sBndRingPt,
                                                  STD_NEXT_PT2D(sBndRingPt) )
                          ==ID_TRUE ) )
                    {
                        // ������ �����ϴ� ���
                        sResult = '1';
                        IDE_RAISE( SABSAE2D_MAX_RESULT );
                    }
                    else
                    {
                        // ������ �������� �ʴ� ���
                    }
                    
                    //----------------------------------------
                    // ���� ���踦 ���� ���� ����� ����� ���� ����
                    //  - Ring�� ���а� ������ ����
                    //  - Ring�� ���� ���� ����
                    //----------------------------------------

                    sMeetOnPoint = ID_FALSE;
                    
                    //----------------------------
                    // Area Ring ���г��� Boundary Ring�� ���� �����ϴ��� �˻�
                    //----------------------------
                    
                    if ( ( stdUtils::betweenI2D( sExtRingPt,
                                                 STD_NEXT_PT2D(sExtRingPt),
                                                 sBndRingPt )==ID_TRUE ) )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Bp                    Bp
                        //               |                     |
                        //         A-----B-----A  ==>   Ap----AB----An  
                        //               |                     |
                        //               Bn                    Bn
                        sMeetOnPoint = ID_TRUE;
                        
                        sExtRingPrevPt = sExtRingPt;
                        sExtRingCurrPt = sBndRingPt;
                        sExtRingNextPt = STD_NEXT_PT2D(sExtRingPt);
                    }
                
                    //----------------------------
                    // Boundary Ring ���г��� Area Ring�� ���� �����ϴ��� �˻�
                    //----------------------------

                    if ( stdUtils::betweenI2D( sBndRingPt,
                                               STD_NEXT_PT2D(sBndRingPt),
                                               sExtRingPt ) == ID_TRUE )
                    {
                        // ���� ���п��� ���� => ���� ���� ����� ����
                        //               Ap                     Ap
                        //               |                      |
                        //         B-----A------B  ==>   Bp----BA----Bn  
                        //               |                      |
                        //               An                     An
                        sMeetOnPoint = ID_TRUE;
                        
                        sBndRingPrevPt = sBndRingPt;
                        sBndRingNextPt = STD_NEXT_PT2D(sBndRingPt);
                    }

                    //----------------------------
                    // ���� ���� �����ϴ� �� �˻�
                    //----------------------------
                    
                    if ( stdUtils::isSamePoints2D( sExtRingPt,
                                                   sBndRingPt ) == ID_TRUE )
                    {
                        sMeetOnPoint = ID_TRUE;
                        
                        // �̹� ������ ���� ���
                    }

                    //----------------------------------------
                    // ���� ���� ����κ��� �ܺ� ������ �Ǵ�
                    //----------------------------------------
                    
                    if ( sMeetOnPoint == ID_TRUE )
                    {
                        if ( hasRelLineSegRingSeg(
                                 ( m == 0 ) ? ID_TRUE : ID_FALSE, // Exterior
                                 sExtRingCCWise,
                                 sExtRingPrevPt,
                                 sExtRingNextPt,
                                 sExtRingCurrPt,
                                 sBndRingPrevPt,
                                 sBndRingNextPt,
                                 STF_OUTSIDE_ANGLE_POS ) == ID_TRUE )
                        {
                            sResult = '1';
                            IDE_RAISE( SABSAE2D_MAX_RESULT );
                        }
                        else
                        {
                            // ���� ���θ� �Ǵ��� �� ����
                        }
                    }
                    else // sMeetOnPoint == ID_FALSE
                    {
                        // �˻� ����� �ƴ�
                    }
                } // for n
            } // for m
        } // for j
    } // for i

    IDE_EXCEPTION_CONT(SABSAE2D_MAX_RESULT);
    
    return sResult;
}


// polygon vs multipoint
SChar stfRelation::saiTompi( const stdPolygon2DType*        aObj1,
                             const stdMultiPoint2DType*     aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        if( spiTosai(&sPoint->mPoint, aObj1) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::sabTompi( const stdPolygon2DType*        aObj1,
                             const stdMultiPoint2DType*     aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTosab(&sPoint->mPoint, aObj1) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::saeTompi( const stdPolygon2DType*        aObj1,
                             const stdMultiPoint2DType*     aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;
    
    sPoint = STD_FIRST_POINT2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTosae(&sPoint->mPoint, aObj1) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

// polygon vs multilinestring
SChar stfRelation::saiTomli( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosai(sLine, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::saiTomlb( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        if( slbTosai(sLine, aObj1) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::sabTomli( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;
    
    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosab(sLine, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }


        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sabTomlb( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        if( slbTosab(sLine, aObj1) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::sabTomle( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdPoint2D*             sPt1;
    stdLinearRing2D*        sRing1;
    stdLineString2DType*    sLine2;
    UInt                    i, j, k, sMax, sMaxR, sMaxO;
    idBool                  sFound;

    sRing1 = STD_FIRST_RN2D(aObj1);
    sMaxR = STD_N_RINGS(aObj1);
    
    for( i = 0; i < sMaxR; i++ )
    {
        sMax = STD_N_POINTS(sRing1)-1;
        sPt1 = STD_FIRST_PT2D(sRing1);
        for( j = 0; j < sMax; j++ )
        {
            // Fix BUG-15516
            sFound = ID_FALSE;
            sLine2 = STD_FIRST_LINE2D(aObj2);
            sMaxO = STD_N_OBJECTS(aObj2);
            for( k = 0; k < sMaxO; k++ )
            {
                if( lineInLineString( 
                    sPt1, STD_NEXT_PT2D(sPt1), sLine2 ) == ID_TRUE )
                {
                    sFound = ID_TRUE;
                    break;
                }
                sLine2 = STD_NEXT_LINE2D(sLine2);
            }

            if( sFound == ID_FALSE )
            {
                return '1';
            }

            sPt1 = STD_NEXT_PT2D(sPt1);
        }
        sPt1 = STD_NEXT_PT2D(sPt1);
        sRing1 = (stdLinearRing2D*)sPt1;
    }

    return 'F';
}

SChar stfRelation::saeTomli( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTosae(sLine, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }


        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::saeTomlb( const stdPolygon2DType*                aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        if( slbTosae(sLine, aObj1) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

// polygon vs multipolygon
IDE_RC stfRelation::saiTomai( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdMultiPolygon2DType*       aObj2,
                              SChar*                             aResult )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( saiTosai(aQmxMem, aObj1, sPoly, &sRet)
                  != IDE_SUCCESS );

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }


        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
        
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::saiTomab( const stdPolygon2DType*            aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = saiTosab(aObj1, sPoly);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

/***********************************************************************
 *
 * Description :
 *
 *    ���� ���� ��ü�� ���� ������ ���� ���� ��ü�� �ܺ� �������� ���踦 ����.
 *    sai(single area internal), mae(multi area external)
 *
 * Implementation :
 *
 *    ǥ��� : Ai (��ü A�� interior ����)
 *
 *    BUG-16319
 *    ���� ��ü ���� ������ ���� ��ü �ܺ� �������� ����� ������ ���� ������ ǥ��
 *
 *    Si ^ ( A U B U ...U N )e
 *    <==>
 *    Si ^ ( Ae ^ Be ^ ... Ne )
 *
 ***********************************************************************/

IDE_RC stfRelation::saiTomae( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdMultiPolygon2DType*       aObj2,
                              SChar*                             aResult )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet;
    SChar               sResult;

    //---------------------------------
    // Initialization
    //---------------------------------
    
    // ���� ��ü �ܺ� ������ ��� ���������� �־�� ���������� �����Ѵ�.
    sResult = '2';

    //---------------------------------
    // ��� ��ü�� �ܺ� ������ ���������� �����ϴ� ���� �Ǵ�
    //---------------------------------
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( saiTosae(aQmxMem, aObj1, sPoly, &sRet )
                  != IDE_SUCCESS );

        if( sRet != '2' )
        {
            sResult = 'F';
            break;
        }
        else
        {
            // ���� ������ ������
        }
        
        sPoly = STD_NEXT_POLY2D(sPoly);
    }

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::sabTomai( const stdPolygon2DType*            aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = saiTosab(sPoly, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sabTomab( const stdPolygon2DType*            aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTosab(aObj1, sPoly);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

/***********************************************************************
 *
 * Description :
 *
 *    ���� ���� ��ü�� ���� ���� ���� ��ü�� �ܺ� �������� ���踦 ����.
 *    sab(single area boundary), mae(multi area external)
 *
 * Implementation :
 *
 *    ǥ��� : Ai (��ü A�� interior ����)
 *
 *    ���� ��ü ���� ���� ��ü �ܺ� �������� ����� ������ ���� ������ ǥ��
 *
 *    Sb ^ ( A U B U ...U N )e
 *    <==>
 *    Sb ^ ( Ae ^ Be ^ ... Ne )
 *
 ***********************************************************************/

SChar stfRelation::sabTomae( const stdPolygon2DType*            aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet;
    SChar               sResult;

    //---------------------------------
    // Initialization
    //---------------------------------
    
    // ���� ��ü �ܺ� ������ ��� ���������� �־�� ���������� �����Ѵ�.
    sResult = '1';

    //---------------------------------
    // ��� ��ü�� �ܺ� ������ ���������� �����ϴ� ���� �Ǵ�
    //---------------------------------
    
    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    

    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTosae(aObj1, sPoly);

        if( sRet != '1' )
        {
            sResult = 'F';
            break;
        }
        sPoly = STD_NEXT_POLY2D(sPoly);
    }

    return sResult;
}


IDE_RC stfRelation::saeTomai( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdMultiPolygon2DType*       aObj2,
                              SChar*                             aResult )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    

    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( saiTosae(aQmxMem, sPoly, aObj1, &sRet)
                  != IDE_SUCCESS );

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::saeTomab( const stdPolygon2DType*            aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPolygon2DType*   sPoly;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);    

    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTosae(sPoly, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

// polygon vs geometrycollection
IDE_RC stfRelation::saiTogci( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdGeoCollection2DType*      aObj2,
                              SChar*                             aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
                sRet = spiTosai( &sGeom->point2D.mPoint, aObj1 );
                break;
            case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTosai( &sGeom->linestring2D, aObj1 );
                break;
            case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTosai( aQmxMem, aObj1, &sGeom->polygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = saiTompi( aObj1, &sGeom->mpoint2D );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = saiTomli( aObj1, &sGeom->mlinestring2D );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTomai( aQmxMem, aObj1, &sGeom->mpolygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }
        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::saiTogcb( const stdPolygon2DType*            aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTosai( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTosab( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = saiTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = saiTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::saiTogce( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdGeoCollection2DType*      aObj2,
                              SChar*                             aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = '2';
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTosae( aQmxMem, aObj1, &sGeom->polygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = '2';
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTomae( aQmxMem, aObj1, &sGeom->mpolygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            sResult = sRet;
            break;
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::sabTogci( const stdPolygon2DType*            aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTosab( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTosab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTosab( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = sabTompi( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sabTomli( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sabTomai( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sabTogcb( const stdPolygon2DType*                aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTosab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTosab( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sabTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sabTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::sabTogce( const stdPolygon2DType*            aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sleTosab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTosae( aObj1, &sGeom->polygon2D );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = sabTomle( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = sabTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::saeTogci( iduMemory*                         aQmxMem,
                              const stdPolygon2DType*            aObj1,
                              const stdGeoCollection2DType*      aObj2,
                              SChar*                             aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
                sRet = spiTosae( &sGeom->point2D.mPoint, aObj1 );
                break;
            case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTosae( &sGeom->linestring2D, aObj1 );
                break;
            case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTosae( aQmxMem, &sGeom->polygon2D, aObj1, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = saeTompi( aObj1, &sGeom->mpoint2D );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = saeTomli( aObj1, &sGeom->mlinestring2D );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( saeTomai( aQmxMem, aObj1, &sGeom->mpolygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }
        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::saeTogcb( const stdPolygon2DType*            aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTosae( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTosae( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = saeTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = saeTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}
// multipoint vs multipoint
SChar stfRelation::mpiTompi( const stdMultiPoint2DType*        aObj1,
                             const stdMultiPoint2DType*        aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTompi(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpiTompe( const stdMultiPoint2DType*        aObj1,
                             const stdMultiPoint2DType*        aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTompe(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

// multipoint vs multilinestring
SChar stfRelation::mpiTomli( const stdMultiPoint2DType*             aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomli(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpiTomlb( const stdMultiPoint2DType*             aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomlb(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpiTomle( const stdMultiPoint2DType*             aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomle(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpeTomlb( const stdMultiPoint2DType*             aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTompe(sLine, aObj1) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

// multipoint vs multipolygon
SChar stfRelation::mpiTomai( const stdMultiPoint2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomai(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpiTomab( const stdMultiPoint2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomab(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

SChar stfRelation::mpiTomae( const stdMultiPoint2DType*         aObj1,
                             const stdMultiPolygon2DType*       aObj2 )
{
    stdPoint2DType* sPoint;
    UInt            i, sMax;

    sPoint = STD_FIRST_POINT2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( spiTomae(&sPoint->mPoint, aObj2) == '0' )
        {
            return '0';
        }
        sPoint = STD_NEXT_POINT2D(sPoint);
    }
    return 'F';
}

// multipoint vs geometrycollection
SChar stfRelation::mpiTogci( const stdMultiPoint2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            if(spiTompi( &sGeom->point2D.mPoint, aObj1 ) == '0')
            {
                return '0';
            }
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            if(sliTompi( &sGeom->linestring2D, aObj1 ) == '0')
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            if(saiTompi( &sGeom->polygon2D, aObj1 ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            if(mpiTompi( aObj1, &sGeom->mpoint2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            if(mpiTomli( aObj1, &sGeom->mlinestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            if(mpiTomai( aObj1, &sGeom->mpolygon2D ) == '0')
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }
        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::mpiTogcb( const stdMultiPoint2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            if(slbTompi( &sGeom->linestring2D, aObj1 ) == '0')
            {
                return '0';
            }
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            if(sabTompi( &sGeom->polygon2D, aObj1 ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            if(mpiTomlb( aObj1, &sGeom->mlinestring2D ) == '0')
            {
                return '0';
            }
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            if(mpiTomab( aObj1, &sGeom->mpolygon2D ) == '0')
            {
                return '0';
            }
            break;
        default:
            return 'F';
        }
        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::mpiTogce( const stdMultiPoint2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = speTompi( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sleTompi( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saeTompi( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTompe( aObj1, &sGeom->mpoint2D );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mpiTomle( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mpiTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mpeTogci( const stdMultiPoint2DType*         aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTompe( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            return '2';
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTompe( &sGeom->mpoint2D, aObj1 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            return '2';
        default:
            return 'F';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mpeTogcb( const stdMultiPoint2DType*    /*    aObj1    */,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);
    
    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            return '1';
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            return '1';
        default:
            return 'F';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}
// multilinestring vs multilinestring
SChar stfRelation::mliTomli( const stdMultiLineString2DType*        aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;
    UInt                    i, sMax;

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTomli(sLine, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mliTomlb( const stdMultiLineString2DType*        aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( sliTomlb(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::mliTomle( const stdMultiLineString2DType*        aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTomle(sLine, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mlbTomlb( const stdMultiLineString2DType*        aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }
    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj2) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTomlb(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::mlbTomle( const stdMultiLineString2DType*        aObj1,
                             const stdMultiLineString2DType*        aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTomle(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

// multilinestring vs multipolygon
SChar stfRelation::mliTomai( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTomai(sLine, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mliTomab( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTomab(sLine, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mliTomae( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sliTomae(sLine, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sLine = STD_NEXT_LINE2D(sLine);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mlbTomai( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTomai(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::mlbTomab( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTomab(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::mlbTomae( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdLineString2DType*    sLine;
    UInt                    i, sMax;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sLine = STD_FIRST_LINE2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        if( slbTomae(sLine, aObj2) == '0' )
        {
            return '0';
        }
        sLine = STD_NEXT_LINE2D(sLine);
    }
    return 'F';
}

SChar stfRelation::mleTomab( const stdMultiLineString2DType*        aObj1,
                             const stdMultiPolygon2DType*           aObj2 )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj2);
    sMax = STD_N_OBJECTS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTomle(sPoly, aObj1);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

// multilinestring vs geometrycollection
SChar stfRelation::mliTogci( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTomli( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTomli( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTomli( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTomli( &sGeom->mpoint2D, aObj1 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomli( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mliTomai( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mliTogcb( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomli( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomli( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mliTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mliTogce( const stdMultiLineString2DType*         aObj1,
                             const stdGeoCollection2DType*           aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sleTomli( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saeTomli( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomle( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mliTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mlbTogci( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTomlb( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTomlb( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTomlb( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTomlb( &sGeom->mpoint2D, aObj1 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomlb( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mlbTomai( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '0' )
        {
            return '0';
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::mlbTogcb( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomlb( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomlb( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomlb( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mlbTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '0' )
        {
            return '0';
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    return 'F';
}

SChar stfRelation::mlbTogce( const stdMultiLineString2DType*     aObj1,
                             const stdGeoCollection2DType*       aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    if( stdUtils::isClosed2D((stdGeometryHeader*)aObj1) )
    {
        return 'F';
    }

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sleTomlb( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saeTomlb( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = '0';
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomle( aObj1, &sGeom->mlinestring2D );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mlbTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mleTogci( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTomle( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTomle( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = '2';
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTomle( &sGeom->mpoint2D, aObj1 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomle( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = '2';
            break;
        default:
            return 'F';
        }

        if( sRet == '2' )
        {
            return '2';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mleTogcb( const stdMultiLineString2DType*        aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomle( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomle( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomle( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mleTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

// multipolygon vs multipolygon
IDE_RC stfRelation::maiTomai( iduMemory*                          aQmxMem,
                              const stdMultiPolygon2DType*        aObj1,
                              const stdMultiPolygon2DType*        aObj2,
                              SChar*                              aResult )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( saiTomai(aQmxMem, sPoly, aObj2, &sRet)
                  != IDE_SUCCESS );

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::maiTomab( const stdMultiPolygon2DType*        aObj1,
                             const stdMultiPolygon2DType*        aObj2 )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = saiTomab(sPoly, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::maiTomae( iduMemory*                          aQmxMem,
                              const stdMultiPolygon2DType*        aObj1,
                              const stdMultiPolygon2DType*        aObj2,
                              SChar*                              aResult )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);
    
    for( i = 0; i < sMax; i++ )
    {
        IDE_TEST( saiTomae(aQmxMem, sPoly, aObj2, &sRet)
                  != IDE_SUCCESS );

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::mabTomab( const stdMultiPolygon2DType*        aObj1,
                             const stdMultiPolygon2DType*        aObj2 )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTomab(sPoly, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mabTomae( const stdMultiPolygon2DType*        aObj1,
                             const stdMultiPolygon2DType*        aObj2 )
{
    stdPolygon2DType*       sPoly;
    UInt                    i, sMax;
    SChar                   sRet = 0x00;
    SChar                   sResult = 0x00;

    sPoly = STD_FIRST_POLY2D(aObj1);
    sMax = STD_N_OBJECTS(aObj1);
    
    for( i = 0; i < sMax; i++ )
    {
        sRet = sabTomae(sPoly, aObj2);

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sPoly = STD_NEXT_POLY2D(sPoly);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}
// multipolygon vs geometrycollection
IDE_RC stfRelation::maiTogci( iduMemory*                             aQmxMem,
                              const stdMultiPolygon2DType*           aObj1,
                              const stdGeoCollection2DType*          aObj2,
                              SChar*                                 aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
                sRet = spiTomai( &sGeom->point2D.mPoint, aObj1 );
                break;
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTomai( &sGeom->linestring2D, aObj1 );
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTomai( aQmxMem, &sGeom->polygon2D, aObj1, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = mpiTomai( &sGeom->mpoint2D, aObj1 );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = mliTomai( &sGeom->mlinestring2D, aObj1 );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( maiTomai( aQmxMem, aObj1, &sGeom->mpolygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }


        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::maiTogcb( const stdMultiPolygon2DType*       aObj1,
                             const stdGeoCollection2DType*      aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomai( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomai( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomai( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = maiTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::maiTogce( iduMemory*                          aQmxMem,
                              const stdMultiPolygon2DType*        aObj1,
                              const stdGeoCollection2DType*       aObj2,
                              SChar*                              aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = '2';
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saeTomai( aQmxMem, &sGeom->polygon2D, aObj1, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = '2';
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( maiTomae( aQmxMem, aObj1, &sGeom->mpolygon2D, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            sResult = sRet;
            break;
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::mabTogci( const stdMultiPolygon2DType*           aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTomab( &sGeom->point2D.mPoint, aObj1 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTomab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTomab( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTomab( &sGeom->mpoint2D, aObj1 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTomab( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = maiTomab( &sGeom->mpolygon2D, aObj1 );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::mabTogcb( const stdMultiPolygon2DType*           aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomab( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomab( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mabTomab( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}
SChar stfRelation::mabTogce( const stdMultiPolygon2DType*        aObj1,
                             const stdGeoCollection2DType*       aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sleTomab( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saeTomab( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = '1';
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mleTomab( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mabTomae( aObj1, &sGeom->mpolygon2D );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::maeTogci( iduMemory*                             aQmxMem,
                              const stdMultiPolygon2DType*           aObj1,
                              const stdGeoCollection2DType*          aObj2,
                              SChar*                                 aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
                sRet = spiTomae( &sGeom->point2D.mPoint, aObj1 );
                break;
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTomae( &sGeom->linestring2D, aObj1 );
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTomae( aQmxMem, &sGeom->polygon2D, aObj1, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = mpiTomae( &sGeom->mpoint2D, aObj1 );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = mliTomae( &sGeom->mlinestring2D, aObj1 );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( maiTomae( aQmxMem, &sGeom->mpolygon2D, aObj1, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::maeTogcb( const stdMultiPolygon2DType*           aObj1,
                             const stdGeoCollection2DType*          aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj2);
    sMax = STD_N_GEOMS(aObj2);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTomae( &sGeom->linestring2D, aObj1 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTomae( &sGeom->polygon2D, aObj1 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTomae( &sGeom->mlinestring2D, aObj1 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mabTomae( &sGeom->mpolygon2D, aObj1 );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

// geometrycollection vs geometrycollection
IDE_RC stfRelation::gciTogci( iduMemory*                           aQmxMem,
                              const stdGeoCollection2DType*        aObj1,
                              const stdGeoCollection2DType*        aObj2,
                              SChar*                               aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj1);
    sMax = STD_N_GEOMS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
                sRet = spiTogci( &sGeom->point2D.mPoint, aObj2 );
                break;
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTogci( &sGeom->linestring2D, aObj2 );
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTogci( aQmxMem, &sGeom->polygon2D, aObj2, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = mpiTogci( &sGeom->mpoint2D, aObj2 );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = mliTogci( &sGeom->mlinestring2D, aObj2 );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( maiTogci( aQmxMem, &sGeom->mpolygon2D, aObj2, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }

        if( sRet == '2' )
        {
            sResult = sRet;
            break;
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::gciTogcb( const stdGeoCollection2DType*        aObj1,
                             const stdGeoCollection2DType*        aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj1);
    sMax = STD_N_GEOMS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            sRet = spiTogcb( &sGeom->point2D.mPoint, aObj2 );
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = sliTogcb( &sGeom->linestring2D, aObj2 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = saiTogcb( &sGeom->polygon2D, aObj2 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            sRet = mpiTogcb( &sGeom->mpoint2D, aObj2 );
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mliTogcb( &sGeom->mlinestring2D, aObj2 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = maiTogcb( &sGeom->mpolygon2D, aObj2 );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

IDE_RC stfRelation::gciTogce( iduMemory*                          aQmxMem,
                              const stdGeoCollection2DType*       aObj1,
                              const stdGeoCollection2DType*       aObj2,
                              SChar*                              aResult )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj1);
    sMax = STD_N_GEOMS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
            case STD_POINT_2D_TYPE:
            case STD_POINT_2D_EXT_TYPE:
                sRet = spiTogce( &sGeom->point2D.mPoint, aObj2 );
                break;
            case STD_LINESTRING_2D_TYPE:
            case STD_LINESTRING_2D_EXT_TYPE:
                sRet = sliTogce( &sGeom->linestring2D, aObj2 );
                break;
            case STD_POLYGON_2D_TYPE:
            case STD_POLYGON_2D_EXT_TYPE:
                IDE_TEST( saiTogce( aQmxMem, &sGeom->polygon2D, aObj2, &sRet )
                          != IDE_SUCCESS );
                break;
            case STD_MULTIPOINT_2D_TYPE:
            case STD_MULTIPOINT_2D_EXT_TYPE:
                sRet = mpiTogce( &sGeom->mpoint2D, aObj2 );
                break;
            case STD_MULTILINESTRING_2D_TYPE:
            case STD_MULTILINESTRING_2D_EXT_TYPE:
                sRet = mliTogce( &sGeom->mlinestring2D, aObj2 );
                break;
            case STD_MULTIPOLYGON_2D_TYPE:
            case STD_MULTIPOLYGON_2D_EXT_TYPE:
                IDE_TEST( maiTogce( aQmxMem, &sGeom->mpolygon2D, aObj2, &sRet )
                          != IDE_SUCCESS );
                break;
            default:
                sResult = 'F';
                IDE_RAISE( normal_exit );
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            sResult = sRet;
            break;
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        sResult = 'F';
    }

    IDE_EXCEPTION_CONT( normal_exit );
    
    *aResult = sResult;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

SChar stfRelation::gcbTogcb( const stdGeoCollection2DType*        aObj1,
                             const stdGeoCollection2DType*        aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj1);
    sMax = STD_N_GEOMS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTogcb( &sGeom->linestring2D, aObj2 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTogcb( &sGeom->polygon2D, aObj2 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTogcb( &sGeom->mlinestring2D, aObj2 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mabTogcb( &sGeom->mpolygon2D, aObj2 );
            break;
        default:
            return 'F';
        }

        if( sRet == '1' )
        {
            return '1';
        }

        // Fix BUG-36086
        if ( sRet != 'F' )
        {

            if ( sResult < sRet )
            {
                sResult = sRet;
            }
            else
            {
                /* Nothing To Do */
            }
        }
        else
        {
            /* Nothing To Do */
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}

SChar stfRelation::gcbTogce( const stdGeoCollection2DType*       aObj1,
                             const stdGeoCollection2DType*       aObj2 )
{
    stdGeometryType*    sGeom;
    UInt                i, sMax;
    SChar               sRet = 0x00;
    SChar               sResult = 0x00;

    sGeom = STD_FIRST_COLL2D(aObj1);
    sMax = STD_N_GEOMS(aObj1);

    for( i = 0; i < sMax; i++ )
    {
        switch( sGeom->header.mType )
        {
        case STD_POINT_2D_TYPE:
        case STD_POINT_2D_EXT_TYPE:
            break;
        case STD_LINESTRING_2D_TYPE:
        case STD_LINESTRING_2D_EXT_TYPE:
            sRet = slbTogce( &sGeom->linestring2D, aObj2 );
            break;
        case STD_POLYGON_2D_TYPE:
        case STD_POLYGON_2D_EXT_TYPE:
            sRet = sabTogce( &sGeom->polygon2D, aObj2 );
            break;
        case STD_MULTIPOINT_2D_TYPE:
        case STD_MULTIPOINT_2D_EXT_TYPE:
            break;
        case STD_MULTILINESTRING_2D_TYPE:
        case STD_MULTILINESTRING_2D_EXT_TYPE:
            sRet = mlbTogce( &sGeom->mlinestring2D, aObj2 );
            break;
        case STD_MULTIPOLYGON_2D_TYPE:
        case STD_MULTIPOLYGON_2D_EXT_TYPE:
            sRet = mabTogce( &sGeom->mpolygon2D, aObj2 );
            break;
        default:
            return 'F';
        }
        if( sRet == 'F' )     // ���� �� �ִ� ���� ����
        {
            return 'F';
        }
        if( sResult == 0x00 )
        {
            sResult = sRet;
        }
        else if( sResult > sRet )
        {
            sResult = sRet;
        }

        sGeom = STD_NEXT_GEOM(sGeom);
    }
    if( sResult == 0x00 )
    {
        return 'F';
    }

    return sResult;
}
/***********************************************************************
 *
 * Description :
 *
 *    Line Segment�� Ring Segment�� (����/�ܺ�) ������ �����ϴ� �� �˻�
 *
 * Implementation :
 *
 *    Line Segment�� Ring Segment�� �� ���� �߽����� ������ ��,
 *    �� ���� ���ϴ� ���� ����(����/�ܺ�)�� �����ϴ� �� �˻���.
 *
 *               Ap
 *               |
 *               |
 *        Lp-----M-----Ln
 *               |
 *               |
 *               An
 *
 ***********************************************************************/

idBool
stfRelation::hasRelLineSegRingSeg( idBool      aIsExtRing,
                                   idBool      aIsCCWiseRing,
                                   stdPoint2D* aRingPrevPt,
                                   stdPoint2D* aRingNextPt,
                                   stdPoint2D* aMeetPoint,
                                   stdPoint2D* aLinePrevPt,
                                   stdPoint2D* aLineNextPt,
                                   stfAnglePos aWantPos )
{
    idBool sResult;

    stfAnglePos  sRevPos;
    stfAnglePos  sLinePrevAnglePos;
    stfAnglePos  sLineNextAnglePos;

    SDouble      sTriangleArea;
    
    //----------------------------
    // Parameter Validation
    //----------------------------

    IDE_DASSERT( aRingPrevPt != NULL );
    IDE_DASSERT( aRingNextPt != NULL );
    IDE_DASSERT( aMeetPoint != NULL );
    IDE_DASSERT( (aWantPos == STF_INSIDE_ANGLE_POS) ||
                 (aWantPos == STF_OUTSIDE_ANGLE_POS) );

    //----------------------------
    // Initialization
    //----------------------------

    sResult = ID_FALSE;
    sRevPos = ( aWantPos == STF_INSIDE_ANGLE_POS ) ?
        STF_OUTSIDE_ANGLE_POS : STF_INSIDE_ANGLE_POS;
    
    //----------------------------
    // �Ǵ��� ���� �ΰ� ���� ����
    //----------------------------
    
    // Area > 0 : �ð�ݴ�������� ������ ��
    // Area < 0 : �ð�������� ������ ��
    // Area = 0 : ������ ������
    
    sTriangleArea = stdUtils::area2D( aRingPrevPt,
                                      aMeetPoint,
                                      aRingNextPt );
    
    // ������ ���� �̷�� ���� ���ԵǴ� �� �˻�
    if ( aLinePrevPt != NULL )
    {
        sLinePrevAnglePos =
            stfRelation::wherePointInAngle( aRingPrevPt,
                                            aMeetPoint,
                                            aRingNextPt,
                                            aLinePrevPt );
    }
    else
    {
        sLinePrevAnglePos = STF_UNKNOWN_ANGLE_POS;
    }
    
    if ( aLineNextPt != NULL )
    {
        sLineNextAnglePos =
            stfRelation::wherePointInAngle( aRingPrevPt,
                                            aMeetPoint,
                                            aRingNextPt,
                                            aLineNextPt );
    }
    else
    {
        sLineNextAnglePos = STF_UNKNOWN_ANGLE_POS;
    }
    

    //----------------------------
    // Area �� Angle�� �̿��� ���� ���� �Ǵ�
    //----------------------------

    if ( ( (aIsExtRing == ID_TRUE) && (aIsCCWiseRing == ID_TRUE )) ||
         ( (aIsExtRing != ID_TRUE) && (aIsCCWiseRing != ID_TRUE ) ) )
    {
        if ( sTriangleArea > 0 )
        {
            //             |         Area     In-->>--
            //      Area   An                 |
            //             |                  |
            //  ->>--Ap----A         Ip-->>---I
            //                       |
            
            if ( (sLinePrevAnglePos == aWantPos)||
                 (sLineNextAnglePos == aWantPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }
        }
        else if ( sTriangleArea < 0 )
        {
            //   Area     |
            //            |
            //       A----An
            //       |     
            //  ->>--Ap

            if ( (sLinePrevAnglePos == sRevPos)||
                 (sLineNextAnglePos == sRevPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }
                            
        }
        else // sTriangleArea == 0
        {
            // ������ ���
            //
            // A--->>--Ap---Ac---An-->>--A

            if ( (sLinePrevAnglePos == aWantPos)||
                 (sLineNextAnglePos == aWantPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }
        }
    }
    else // External Not CCW, Internal CCW
    {
        if ( sTriangleArea > 0 )
        {
            //  Area       |      Area   I--------Ip--<<--
            //       A-----Ap            |  
            //       |                   |   
            //  -<<--An                  In
            //                           
                            
            if ( (sLinePrevAnglePos == sRevPos)||
                 (sLineNextAnglePos == sRevPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }
        }
        else if ( sTriangleArea < 0 )
        {
            //             |   
            //      Area   Ap  
            //             |   
            //  -<<--An----A   
            //
            if ( (sLinePrevAnglePos == aWantPos)||
                 (sLineNextAnglePos == aWantPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }
        }
        else // sTriangleArea == 0
        {
            // ������ ���  Area
            //
            // A---<<--An---Ac---Ap--<<--A

            if ( (sLinePrevAnglePos == sRevPos)||
                 (sLineNextAnglePos == sRevPos) )
            {
                sResult = ID_TRUE;
                IDE_RAISE( HAS_RELATION_MAX_RESULT );
            }
            else
            {
                // ���� ���θ� �Ǵ��� �� ����
            }

        }
    }

    IDE_EXCEPTION_CONT( HAS_RELATION_MAX_RESULT );

    return sResult;
}

/***********************************************************************
 *
 * Description :
 *
 *    Point�� Angle���� ��� ��ġ�ϴ� ���� �Ǵ�
 *
 * Implementation :
 *
 *     Angle�󿡼��� ���� ��ġ
 *
 *                  An
 *        1         |                               1
 *                  |        
 *                  4     2              Ap---3->>--Am---4---An
 *                  |
 *      Ap---->>-3--Am                              2
 *
 *                2
 *
 *   1 : INSIDE
 *   2 : OUTSIDE
 *   3 : MIN
 *   4 : MAX
 *
 ***********************************************************************/

stfAnglePos
stfRelation::wherePointInAngle( stdPoint2D * aAnglePrevPt,
                                stdPoint2D * aAngleMiddPt,
                                stdPoint2D * aAngleNextPt,
                                stdPoint2D * aTestPt )
{
    stfAnglePos sResult;
        
    // Angle(Ap, Am, An)�� �̷�� ���� ����
    SDouble sAngleArea;
    SDouble sAngleAngle;

    // Point(Ap, Am, P)�� �̷�� ���� ����
    SDouble sPointArea;
    SDouble sPointAngle;

    // ������ �� �����ϴ��� ���� ���
    SDouble sCorrArea;
    
    //--------------------------------------
    // Parameter Validation
    //--------------------------------------

    IDE_DASSERT( aAnglePrevPt != NULL );
    IDE_DASSERT( aAngleMiddPt != NULL );
    IDE_DASSERT( aAngleNextPt != NULL );
    IDE_DASSERT( aTestPt != NULL );
    
    //--------------------------------------
    // Initialization
    //--------------------------------------

    sResult = STF_UNKNOWN_ANGLE_POS;

    // ������ ���� �������� �����ϴ� �� �Ǵ�
    // ������ ���� �밢���� ��ġ�Ҷ�
    // (Ap,Am,An)�� (Ap,Am,P)�� �ٸ� Angle���� ���ü� �ִ�.
    //
    //                   An
    //                  +  
    //                 P
    //                +
    //        Ap-----Am

    // ���������̶�� Area�� �����Ǿ� 0 ���� ���´�.
    sCorrArea = stdUtils::area2D( aAngleMiddPt,
                                  aAngleNextPt,
                                  aTestPt );
        
    //--------------------------------------
    // ������ ���� ���Ѵ�.
    //--------------------------------------
    
    // Area > 0 : �ð�ݴ�������� ������ ��
    // Area < 0 : �ð�������� ������ ��
    // Area = 0 : ������ ������

    // 0 <= Angle <= 3.141592XXX
    
    sAngleArea = stdUtils::area2D( aAnglePrevPt,
                                   aAngleMiddPt,
                                   aAngleNextPt );
    sAngleAngle = stdUtils::getAngle2D( aAnglePrevPt,
                                        aAngleMiddPt,
                                        aAngleNextPt );

    sPointArea = stdUtils::area2D( aAnglePrevPt,
                                   aAngleMiddPt,
                                   aTestPt );
    
    sPointAngle = stdUtils::getAngle2D( aAnglePrevPt,
                                        aAngleMiddPt,
                                        aTestPt );

    //--------------------------------------
    // ������ ���� �̿��� ���� ���� �Ǻ�
    //--------------------------------------

    if ( sAngleArea > 0 )
    {
        //                 An
        //                 |
        //                 |
        //                 |
        //    Ap---->>-----Am
        
        if ( sPointArea > 0 )
        {
            if ( sCorrArea == 0 )
            {
                // sPointAngle == sAngleAngle �� �Ǵ�
                // Am-->P-->An �� ������
                sResult = STF_MAX_ANGLE_POS;
            }
            else
            {
                if ( sPointAngle > sAngleAngle )
                {
                    //                 An
                    //                 |       P
                    //                 |       
                    //                 |
                    //    Ap---->>-----Am
                    
                    sResult = STF_OUTSIDE_ANGLE_POS;
                }
                else // sPointAngle < sAngleAngle )
                {
                    sResult = STF_INSIDE_ANGLE_POS;
                }
            }
        }
        else if ( sPointArea < 0 )
        {
            //                 An
            //                 |    
            //                 |       
            //                 |
            //    Ap---->>-----Am
            //
            //                     P

            sResult = STF_OUTSIDE_ANGLE_POS;
        }
        else // sPointArea == 0
        {
            // 3.14���� ū ������ ���� ����
            // 0 �Ǵ� 3.141592XXXX�� ���;� ������,
            // �밢������ �����Ҷ��� 0�� ����� ���� ���� ���� ���´�.
            
            if ( sPointAngle > 3.14 )
            {
                //                 An
                //                 |    
                //                 |       
                //                 |
                //    Ap---->>-----Am        P
                
                sResult = STF_OUTSIDE_ANGLE_POS;
            }
            else
            {
                //                 An
                //                 |    
                //                 |       
                //                 |
                //    Ap---->>-P---Am
                
                sResult = STF_MIN_ANGLE_POS;
            }
        }
    }
    else if ( sAngleArea < 0 )
    {
        //     Am---->>-----An
        //     |
        //     |
        //     |
        //     Ap

        if ( sPointArea > 0 )
        {
            //
            //       Am---->>-----An
            // P     |
            //       |
            //       |
            //       Ap
            
            sResult = STF_OUTSIDE_ANGLE_POS;
        }
        else if ( sPointArea < 0 )
        {
            if ( sCorrArea == 0 )
            {
                // sPointAngle == sAngleAngle �� �Ǵ�
                // Am-->P-->An �� ������
                sResult = STF_MAX_ANGLE_POS;
            }
            else
            {
                if ( sPointAngle > sAngleAngle )
                {
                    //
                    //           P
                    //
                    //       Am---->>-----An
                    //       |
                    //       |
                    //       |
                    //       Ap
                    
                    sResult = STF_OUTSIDE_ANGLE_POS;
                }
                else
                {
                    //
                    //           
                    //
                    //       Am---->>-----An
                    //       |
                    //       |   P
                    //       |
                    //       Ap
                    
                    sResult = STF_INSIDE_ANGLE_POS;
                }
            }
        }
        else // sPointArea == 0
        {
            if ( sPointAngle > 3.14 )
            {
                //
                //       P    
                //
                //       Am---->>-----An
                //       |
                //       |
                //       |
                //       Ap
                
                sResult = STF_OUTSIDE_ANGLE_POS;
            }
            else
            {
                //
                //           
                //
                //       Am---->>-----An
                //       |
                //       P
                //       |
                //       Ap
                
                sResult = STF_MIN_ANGLE_POS;
            }
        }
    }
    else // sAngleArea == 0
    {
        //
        //     Ap-->>--Am-->>---An
        //
        
        if ( sPointArea > 0 )
        {
            //           P
            //
            //     Ap-->>--Am-->>---An
            //
            
            sResult = STF_INSIDE_ANGLE_POS;
        }
        else if ( sPointArea < 0 )
        {
            //
            //     Ap-->>--Am-->>---An
            //
            //           P
            
            sResult = STF_OUTSIDE_ANGLE_POS;
        }
        else // sPointArea == 0
        {
            if ( sPointAngle > 3.14 )
            {
                //
                //     Ap-->>---Am-->>P---An
                //
            
                sResult = STF_MAX_ANGLE_POS;
            }
            else
            {
                //
                //     Ap-->>-P--Am-->>---An
                //
                
                sResult = STF_MIN_ANGLE_POS;
            }
        }
    }

    return sResult;
}

IDE_RC stfRelation::relateAreaArea( iduMemory*             aQmxMem,
                                    const stdGeometryType* aObj1, 
                                    const stdGeometryType* aObj2,
                                    SChar*                 aPattern,
                                    mtdBooleanType*        aReturn )
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
    SChar               sResultMatrix[9] = { STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_NOT,
                                             STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_NOT,
                                             STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_NOT, STF_INTERSECTS_DIM_2 };    
    stdRepPoint*        sRepPoints;
    SInt                sPointArea;
    SDouble             sCmpMinMaxX;
    SDouble             sCmpMaxMinX;

    sMax1 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj1);
    sMax2 = stdUtils::getGeometryNum((stdGeometryHeader*)aObj2);

    sCmpMinMaxX = ( ((stdGeometryHeader*)aObj1)->mMbr.mMinX > ((stdGeometryHeader*)aObj2)->mMbr.mMinX )  ?
                    ((stdGeometryHeader*)aObj1)->mMbr.mMinX :
                    ((stdGeometryHeader*)aObj2)->mMbr.mMinX ;

    sCmpMaxMinX = ( ((stdGeometryHeader*)aObj1)->mMbr.mMaxX > ((stdGeometryHeader*)aObj2)->mMbr.mMaxX )  ?
                    ((stdGeometryHeader*)aObj2)->mMbr.mMaxX :
                    ((stdGeometryHeader*)aObj1)->mMbr.mMaxX ;

    /* �� �������� ��ǥ ���� ������ �迭 �Ҵ� */
    IDE_TEST( aQmxMem->alloc( (sMax1 + sMax2) * ID_SIZEOF(stdRepPoint),
                              (void**) & sRepPoints )
              != IDE_SUCCESS);

    sPoly1 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj1);

    for( i = 0; i < sMax1; i++ ) 
    {
        IDE_TEST_RAISE( sPoly1 == NULL, err_invalid_object_mType );

        sRing       = STD_FIRST_RN2D(sPoly1);
        sMaxR       = STD_N_RINGS(sPoly1);
        sTotalRing += sMaxR;

        sRepPoints[i].mPoint = STD_FIRST_PT2D( sRing );

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
        
        sRepPoints[i+sMax1].mPoint = STD_FIRST_PT2D( sRing );
        
        for ( j = 0; j < sMaxR; j++ )
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

        j = sRingCount;
        
        IDE_TEST( stdUtils::classfyPolygonChain( aQmxMem,
                                                 sPoly1,
                                                 i, 
                                                 sIndexSeg,
                                                 &sIndexSegTotal,
                                                 sRingSegList,
                                                 &sRingCount,
                                                 ID_FALSE )  // For validation? 
                  != IDE_SUCCESS );

        sRepPoints[i].mIsValid = ( j == sRingCount ) ? ID_FALSE : ID_TRUE;

        sPoly1 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly1);
    }

    sPoly2 = (stdPolygon2DType*)stdUtils::getFirstGeometry(
                                        (stdGeometryHeader*)aObj2);

    for( i = 0; i < sMax2; i++ ) 
    {
        IDE_TEST_RAISE( sPoly2 == NULL, err_invalid_object_mType );

        j = sRingCount;

        IDE_TEST( stdUtils::classfyPolygonChain( aQmxMem,
                                                 sPoly2,
                                                 i + sMax1, 
                                                 sIndexSeg,
                                                 &sIndexSegTotal,
                                                 sRingSegList,
                                                 &sRingCount,
                                                 ID_FALSE ) // For validation?
                  != IDE_SUCCESS );

        sRepPoints[i+sMax1].mIsValid = ( j == sRingCount ) ? ID_FALSE : ID_TRUE;
        
        sPoly2 = (stdPolygon2DType*)stdUtils::getNextGeometry(
                                            (stdGeometryHeader*)sPoly2);
    }

    /* �������� �̷���� ������� ���� ���� �񱳸� ���� */
    for ( i = 0 ; i < sMax1 ; i++ )
    {
        if ( sRepPoints[i].mIsValid == ID_FALSE )
        {
            for ( j = sMax1 ; j < sMax1 + sMax2 ; j++ )
            {
                if ( sRepPoints[j].mIsValid == ID_FALSE )
                {
                    if ( stdUtils::isSamePoints2D( sRepPoints[i].mPoint, sRepPoints[j].mPoint ) 
                           == ID_TRUE )
                    {
                        setDE9MatrixValue( sResultMatrix, STF_INTER_INTER, STF_INTERSECTS_DIM_2 );
                        setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND, STF_INTERSECTS_DIM_1 );
                    }
                }
            }
        }
    }

    if ( sIndexSegTotal == 0 )
    {
        (void)checkRelateResult( aPattern, sResultMatrix, aReturn );
        IDE_RAISE( normal_exit );      
    }

    /* BUG-33436
     * sIndexSeg�� x��ǥ ������ ���ĵǾ� �־�� �Ѵ�. */
    iduHeapSort::sort( sIndexSeg, sIndexSegTotal, 
                       ID_SIZEOF(Segment*), cmpSegment );

    /* �� ��ǥ���� �ٸ� �������� ���ο� ���ԵǴ��� �� ���� */
    for ( i = 0 ; i < sMax1; i++ )
    {
        sPointArea = stdUtils::isPointInside( sIndexSeg,
                                              sIndexSegTotal,
                                              sRepPoints[i].mPoint, 
                                              sMax1,
                                              sMax1 + sMax2 - 1 );

        switch ( sPointArea )
        {
        case ST_POINT_INSIDE:
            setDE9MatrixValue( sResultMatrix, STF_INTER_INTER,  STF_INTERSECTS_DIM_2 );
            setDE9MatrixValue( sResultMatrix, STF_BOUND_INTER,  STF_INTERSECTS_DIM_1 );
            setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER,  STF_INTERSECTS_DIM_2 );
            break;
        case ST_POINT_ONBOUND:
            setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND,  STF_INTERSECTS_DIM_0 );
            break;
        case ST_POINT_OUTSIDE:
            setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER,  STF_INTERSECTS_DIM_2 );
            break;
        default:
            IDE_DASSERT(0);
            break;
        }
    }

    for ( i = sMax1 ; i < sMax1 + sMax2; i++ )
    {
        sPointArea = stdUtils::isPointInside( sIndexSeg,
                                              sIndexSegTotal,
                                              sRepPoints[i].mPoint, 
                                              0,
                                              sMax1 - 1 );
        switch ( sPointArea )
        {
        case ST_POINT_INSIDE:
            setDE9MatrixValue( sResultMatrix, STF_INTER_INTER,  STF_INTERSECTS_DIM_2 );
            setDE9MatrixValue( sResultMatrix, STF_INTER_BOUND,  STF_INTERSECTS_DIM_1 );
            setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER,  STF_INTERSECTS_DIM_2 );
            break;
        case ST_POINT_ONBOUND:
            setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND,  STF_INTERSECTS_DIM_0 );
            break;
        case ST_POINT_OUTSIDE:
            setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER,  STF_INTERSECTS_DIM_2 );
            break;
        default:
            IDE_DASSERT(0);
            break;
        }
    }

    IDE_TEST_RAISE( checkRelateResult(aPattern, sResultMatrix, aReturn ) 
                    == ID_TRUE, normal_exit );

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
        if ( ( sIndexSeg[i]->mParent->mEnd->mEnd.mX >= sCmpMinMaxX ) && 
             ( sIndexSeg[i]->mParent->mBegin->mStart.mX <= sCmpMaxMinX ) )
        {
            sPQueue.enqueue( &(sIndexSeg[i]), &sOverflow);
            IDE_TEST_RAISE( sOverflow == ID_TRUE, ERR_ABORT_ENQUEUE_ERROR );
        }
        else
        {
            // Nothing to do 
        }
    }
    
    while(1)
    {
        sPQueue.dequeue( (void*) &sCurrSeg, &sUnderflow );

        if ( sUnderflow == ID_TRUE )
        {
            break;
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
                if ( ( sCurrNext != sCmpSeg ) && ( sCurrPrev != sCmpSeg ) && 
                     ( ( ( sCurrSeg->mParent->mPolygonNum < sMax1 ) && ( sCmpSeg->mParent->mPolygonNum  >= sMax1 ) ) ||
                       ( ( sCmpSeg->mParent->mPolygonNum  < sMax1 ) && ( sCurrSeg->mParent->mPolygonNum >= sMax1 ) ) )  )
                {
                    IDE_TEST( stdUtils::intersectCCW( sCurrSeg->mStart,
                                                      sCurrSeg->mEnd,
                                                      sCmpSeg->mStart,
                                                      sCmpSeg->mEnd,
                                                      &sIntersectStatus,
                                                      &sInterCount,
                                                      sInterResult)
                              != IDE_SUCCESS );

                    if ( sIntersectStatus != ST_NOT_INTERSECT )
                    {
                        switch( sIntersectStatus )
                        {
                            case ST_POINT_TOUCH:
                            case ST_TOUCH:
                                setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND,  STF_INTERSECTS_DIM_0 );
                                break;

                            case ST_INTERSECT:
                                setDE9MatrixValue( sResultMatrix, STF_INTER_INTER, STF_INTERSECTS_DIM_2 );
                                setDE9MatrixValue( sResultMatrix, STF_INTER_BOUND, STF_INTERSECTS_DIM_1 );
                                setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER, STF_INTERSECTS_DIM_2 );
                                setDE9MatrixValue( sResultMatrix, STF_BOUND_INTER, STF_INTERSECTS_DIM_1 );
                                setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND, STF_INTERSECTS_DIM_0 );
                                setDE9MatrixValue( sResultMatrix, STF_BOUND_EXTER, STF_INTERSECTS_DIM_1 );
                                setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER, STF_INTERSECTS_DIM_2 );
                                setDE9MatrixValue( sResultMatrix, STF_EXTER_BOUND, STF_INTERSECTS_DIM_1 );
                                break;

                            case ST_SHARE:
                                setDE9MatrixValue( sResultMatrix, STF_BOUND_BOUND,  STF_INTERSECTS_DIM_1 );
                                break;
                        }

                        IDE_TEST_RAISE( checkRelateResult(aPattern, sResultMatrix, aReturn ) 
                                        == ID_TRUE, normal_exit );
                       
                        switch( sIntersectStatus )
                        {
                        case ST_INTERSECT:
                        case ST_POINT_TOUCH:
                        case ST_TOUCH:
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
                        }
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

    IDE_TEST( stdUtils::reassign( aQmxMem, sPrimInterSeg, ID_FALSE )
              != IDE_SUCCESS);

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

    for ( i = 0 ; i < sIndexSegTotal ; i++ )
    {
        sCurrSeg = sIndexSeg[i];
        while ( sCurrSeg != NULL )
        {
            switch( sCurrSeg->mLabel )
            {

            case ST_SEG_LABEL_INSIDE:
                if( sCurrSeg->mParent->mPolygonNum < sMax1 )
                {
                    setDE9MatrixValue( sResultMatrix, STF_INTER_INTER,  STF_INTERSECTS_DIM_2 );
                    setDE9MatrixValue( sResultMatrix, STF_BOUND_INTER,  STF_INTERSECTS_DIM_1 );
                    setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER,  STF_INTERSECTS_DIM_2 );
                }
                else
                {
                    setDE9MatrixValue( sResultMatrix, STF_INTER_INTER,  STF_INTERSECTS_DIM_2 );
                    setDE9MatrixValue( sResultMatrix, STF_INTER_BOUND,  STF_INTERSECTS_DIM_1 );
                    setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER,  STF_INTERSECTS_DIM_2 );
                }
                break;
            
            case ST_SEG_LABEL_OUTSIDE:
                if( sCurrSeg->mParent->mPolygonNum < sMax1 )
                {
                    setDE9MatrixValue( sResultMatrix, STF_BOUND_EXTER,  STF_INTERSECTS_DIM_1 );
                    setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER,  STF_INTERSECTS_DIM_2 );
                }
                else
                {
                    setDE9MatrixValue( sResultMatrix, STF_EXTER_BOUND,  STF_INTERSECTS_DIM_1 );
                    setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER,  STF_INTERSECTS_DIM_2 );
                }
                break;

            case ST_SEG_LABEL_SHARED1:
                setDE9MatrixValue( sResultMatrix, STF_INTER_INTER,  STF_INTERSECTS_DIM_2 );
                setDE9MatrixValue( sResultMatrix, STF_EXTER_EXTER,  STF_INTERSECTS_DIM_2 );
                break;

            case ST_SEG_LABEL_SHARED2:
                setDE9MatrixValue( sResultMatrix, STF_EXTER_INTER,  STF_INTERSECTS_DIM_2 );
                setDE9MatrixValue( sResultMatrix, STF_INTER_EXTER,  STF_INTERSECTS_DIM_2 );
                break;

            default:
                IDE_DASSERT(0);
                break;
            }
            sCurrSeg = sCurrSeg->mNext;
        }
    }

    (void)checkRelateResult( aPattern, sResultMatrix, aReturn );

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_object_mType);
    {
        IDE_SET(ideSetErrorCode(stERR_ABORT_OBJECT_TYPE_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_ABORT_ENQUEUE_ERROR )
    {
        IDE_SET( ideSetErrorCode( stERR_ABORT_UNEXPECTED_ERROR,
                                  "stfRelation::RelateAreaArea",
                                  "enqueue overflow" ));
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


void stfRelation::setDE9MatrixValue( SChar* aMatrix, 
                                     SInt   aMatrixIndex, 
                                     SInt   aOrder )
{
    /* �̹� ������ ������ ū ��쿡�� �����Ѵ�. */
    if ( aMatrix[aMatrixIndex] < aOrder )
    {
        aMatrix[aMatrixIndex] = aOrder;
    }
    else
    {
        // Nothing to do.
    }
}

idBool stfRelation::checkRelateResult( SChar*          aPattern, 
                                       SChar*          aResultMatrix, 
                                       mtdBooleanType* aResult )
{
    SInt   i;
    idBool sFastReturn = ID_FALSE;
    idBool sReturnValue = ID_TRUE;

    for ( i = 0 ; i < 9 ; i++ )
    {
        switch( aPattern[i] )
        {
        case 'F':
            if ( aResultMatrix[i] > STF_INTERSECTS_DIM_NOT )
            {
                sFastReturn  = ID_TRUE;
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            else
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_TRUE );
            }
            break;

        case '0':
            if ( aResultMatrix[i] > STF_INTERSECTS_DIM_0 )
            {
                sFastReturn  = ID_TRUE;
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            else if ( aResultMatrix[i] == STF_INTERSECTS_DIM_0 )
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_TRUE );
            }
            else
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            break;

        case '1':
            if ( aResultMatrix[i] > STF_INTERSECTS_DIM_1 )
            {
                sFastReturn  = ID_TRUE;
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            else if ( aResultMatrix[i] == STF_INTERSECTS_DIM_1 )
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_TRUE );
            }
            else
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            break;

        case '2':
            if ( aResultMatrix[i] == STF_INTERSECTS_DIM_2 )
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_TRUE );
            }
            else
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            break;

        case 'T':
            if ( aResultMatrix[i] > STF_INTERSECTS_DIM_NOT )
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_TRUE );
            }
            else
            {
                sReturnValue = ST_LOGICAL_AND( sReturnValue, ID_FALSE );
            }
            break;

        case '*': // don't care
            break;

        default: 
            IDE_DASSERT(0);
            break;
        }
    }

    *aResult = ( sReturnValue == ID_TRUE ) ? MTD_BOOLEAN_TRUE : MTD_BOOLEAN_FALSE;
    return sFastReturn;
}
