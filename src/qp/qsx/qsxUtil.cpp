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
 * $Id: qsxUtil.cpp 89096 2020-10-29 23:09:57Z bethy $
 **********************************************************************/

/*
   NAME
    qsxUtil.cpp

   DESCRIPTION

   PUBLIC FUNCTION(S)

   PRIVATE FUNCTION(S)

   NOTES

   MODIFIED   (MM/DD/YY)
*/

#include <idl.h>
#include <idu.h>

#include <smi.h>

#include <qcuProperty.h>
#include <qc.h>
#include <qtc.h>
#include <qmn.h>
#include <qcuError.h>
#include <qsxUtil.h>
#include <qsxArray.h>
#include <qcsModule.h>
#include <qcuSessionPkg.h>
#include <qsvEnv.h>

IDE_RC qsxUtil::assignNull (
    qtcNode      * aNode,
    qcTemplate   * aTemplate )
{
    #define IDE_FN "IDE_RC qsxUtil::assignNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcColumn * sColumn;
    void      * sValueTemp;

    sColumn = QTC_TMPL_COLUMN( aTemplate, aNode );

    sValueTemp = (void*)mtc::value( sColumn,
                                    QTC_TMPL_TUPLE( aTemplate, aNode )->row,
                                    MTD_OFFSET_USE );

    sColumn->module->null( sColumn,
                           sValueTemp );

    return IDE_SUCCESS;

    #undef IDE_FN
}

IDE_RC qsxUtil::assignNull( mtcColumn * aColumn,
                            void      * aValue,
                            idBool      aCopyRef )
{
    qsTypes       * sType = NULL;
    qsxArrayInfo ** sArrayInfo = NULL;
    mtcColumn     * sColumn = NULL;
    qcmColumn     * sQcmColumn = NULL;
    void          * sRow = NULL;
    UInt            sSize = 0;

    sType = ((qtcModule*)aColumn->module)->typeInfo;

    if ( ( (sType->flag & QTC_UD_TYPE_HAS_ARRAY_MASK) ==
           QTC_UD_TYPE_HAS_ARRAY_TRUE ) &&
         ( aCopyRef == ID_TRUE ) )
    {
        if ( aColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
        {
            sArrayInfo = (qsxArrayInfo**)aValue;

            IDE_TEST( qsxArray::finalizeArray( sArrayInfo )
                      != IDE_SUCCESS );
        }
        else // MTD_RECORDTYPE_ID or MTD_ROWTYPE_ID
        {
            sQcmColumn = sType->columns;

            while ( sQcmColumn != NULL )
            {
                sColumn = sQcmColumn->basicInfo;

                sSize = idlOS::align( sSize, sColumn->module->align ); 
                sRow = (void*)((UChar*)aValue + sSize);

                if ( sColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
                {
                    sArrayInfo = (qsxArrayInfo**)sRow;

                    IDE_TEST( qsxArray::finalizeArray( sArrayInfo )
                              != IDE_SUCCESS );
                }
                else if ( (sColumn->type.dataTypeId == MTD_ROWTYPE_ID) ||
                          (sColumn->type.dataTypeId == MTD_RECORDTYPE_ID) )
                {
                    IDE_TEST( assignNull( sColumn,
                                          sRow,
                                          aCopyRef )
                              != IDE_SUCCESS );
                }
                else
                {
                    sColumn->module->null( sColumn, sRow );
                }

                sSize += sColumn->column.size;

                sQcmColumn = sQcmColumn->next;
            }
        }
    }
    else
    {
        aColumn->module->null( aColumn,
                               aValue );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxUtil::assignValue (
    iduMemory    * aMemory,
    mtcStack     * aSourceStack,
    SInt           aSourceStackRemain,
    qtcNode      * aDestNode,
    qcTemplate   * aDestTemplate,
    idBool         aCopyRef )
{
    mtcColumn          * sDestColumn = NULL;

    // BUG-33674
    IDE_TEST_RAISE( aSourceStackRemain < 1, ERR_STACK_OVERFLOW );

    sDestColumn = QTC_TMPL_COLUMN( aDestTemplate, aDestNode );

    /* PROJ-2586 PSM Parameters and return without precision
       aDestNode�� parameter�� �� mtcColumn ���� ���� */
    if ( (aDestNode->lflag & QTC_NODE_SP_PARAM_OR_RETURN_MASK) == QTC_NODE_SP_PARAM_OR_RETURN_TRUE )
    {
        IDE_TEST( qsxUtil::adjustParamAndReturnColumnInfo( aMemory,
                                                           aSourceStack->column,
                                                           sDestColumn,
                                                           &aDestTemplate->tmplate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( assignValue( aMemory,
                           aSourceStack->column,
                           aSourceStack->value,
                           sDestColumn,
                           (void*)
                           ( (SChar*) aDestTemplate->tmplate.rows[aDestNode->node.table].row +
                             sDestColumn->column.offset ),
                           & aDestTemplate->tmplate,
                           aCopyRef )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_STACK_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-11192 date_format session property
// mtv::executeConvert()�� mtcTemplate�� �ʿ���
// aDestTemplate ���� �߰�.
// by kumdory, 2005-04-08
IDE_RC qsxUtil::assignValue (
    iduMemory    * aMemory,
    qtcNode      * aSourceNode,
    qcTemplate   * aSourceTemplate,
    mtcStack     * aDestStack,
    SInt           aDestStackRemain,
    qcTemplate   * aDestTemplate,
    idBool         aParamOrReturn,
    idBool         aCopyRef )
{
    mtcColumn          * sSourceColumn ;

    IDE_TEST_RAISE( aDestStackRemain < 1, ERR_STACK_OVERFLOW );

    sSourceColumn = QTC_TMPL_COLUMN( aSourceTemplate, aSourceNode );

    /* PROJ-2586 PSM Parameters and return without precision
       aDestNode�� parameter�� �� mtcColumn ���� ���� */
    if ( aParamOrReturn == ID_TRUE )
    {
        IDE_TEST( qsxUtil::adjustParamAndReturnColumnInfo( aMemory,
                                                           sSourceColumn,
                                                           aDestStack->column,
                                                           &aDestTemplate->tmplate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( assignValue( aMemory,
                           sSourceColumn,
                           (void*)
                           ( (SChar*) aSourceTemplate->tmplate.rows[aSourceNode->node.table].row
                             + sSourceColumn->column.offset ),
                           aDestStack->column,
                           aDestStack->value,
                           & aDestTemplate->tmplate,
                           aCopyRef )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_STACK_OVERFLOW);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxUtil::assignValue (
    iduMemory    * aMemory,
    mtcColumn    * aSourceColumn,
    void         * aSourceValue,
    mtcColumn    * aDestColumn,
    void         * aDestValue,
    mtcTemplate  * aDestTemplate,
    idBool         aCopyRef )
{
/***********************************************************************
 *
 * Description : PROJ-1075 assign.
 *               row/record/array type�� ���� �з��Ͽ� assign�Ѵ�.
 *
 * Implementation : assign�� ������ ������ ���� �����Ѵ�.
 * (1) �Ѵ� primitive �� �ƴѰ��
 *     : assignNonPrimitiveValue
 * (2) �Ѵ� primitive �� ���
 *     : assignPrimitiveValue
 * (3) �ϳ��� primitive, �ϳ��� udt�� ���
 *     : conversion�Ұ�. ����
 *
 ***********************************************************************/

    qsxArrayBindingHeader* sHeader            = NULL;
    qsxArrayInfo         * sSrcArrayInfo      = NULL;
    qsxAvlTree           * sSrcTree           = NULL;
    qsxArrayInfo         * sDstArrayInfo      = NULL;
    qsxAvlTree           * sDstTree           = NULL;
    void                 * sDstRowPtr         = NULL;
    mtcColumn            * sKeyCol            = NULL;
    void                 * sKeyPtr            = NULL;
    mtcColumn            * sDataCol           = NULL;
    void                 * sDataPtr           = NULL;
    void                 * sRowPtr            = NULL;
    UInt                   sActualSize        = 0;
    SInt                   sKey               = -1;
    SInt                   sPrevKey           = -1;
    UInt                   sSize              = 0;
    UInt                   sCount             = 0;
    SChar                * sCursor            = NULL; 
    SChar                * sFence             = NULL;       
    idBool                 sFound             = ID_FALSE;
    UInt                   sHasNull           = 0;
    qcTemplate           * sTemplate          = (qcTemplate *)aDestTemplate;
#if defined(ENDIAN_IS_BIG_ENDIAN)
    idBool                 sIsBigEndianServer = ID_TRUE;
#else
    idBool                 sIsBigEndianServer = ID_FALSE;
#endif
    idBool                 sIsBigEndianClient = ID_FALSE;
    UInt                   i;
    SInt                   j;

    // right node�� NULL�� ���
    if ( aSourceColumn->type.dataTypeId == MTD_NULL_ID )
    {
        if ( (aDestColumn->type.dataTypeId >= MTD_UDT_ID_MIN) && 
             (aDestColumn->type.dataTypeId <= MTD_UDT_ID_MAX) )
        {
            IDE_TEST( assignNull( aDestColumn,
                                  aDestValue,
                                  aCopyRef )
                      != IDE_SUCCESS );
        }
        else
        {
            aDestColumn->module->null( aDestColumn, aDestValue );
        }
    }
    // �Ѵ� primitive�� �ƴ� ���
    else if( ( aDestColumn->type.dataTypeId >= MTD_UDT_ID_MIN ) &&
             ( aDestColumn->type.dataTypeId <= MTD_UDT_ID_MAX ) &&
             ( aSourceColumn->type.dataTypeId >= MTD_UDT_ID_MIN ) &&
             ( aSourceColumn->type.dataTypeId <= MTD_UDT_ID_MAX ) )
    {
        IDE_TEST( assignNonPrimitiveValue( aMemory,
                                           aSourceColumn,
                                           aSourceValue,
                                           aDestColumn,
                                           aDestValue,
                                           aDestTemplate,
                                           aCopyRef )
                  != IDE_SUCCESS );
    }
    // �Ѵ� primitive�� ���
    else if ( ( ( aDestColumn->type.dataTypeId < MTD_UDT_ID_MIN ) ||
                ( aDestColumn->type.dataTypeId > MTD_UDT_ID_MAX ) ) &&
              ( ( aSourceColumn->type.dataTypeId < MTD_UDT_ID_MIN ) ||
                ( aSourceColumn->type.dataTypeId > MTD_UDT_ID_MAX ) ) )
    {
        // �Ϲ����� primitive value�� assign�Լ��� ȣ���Ѵ�.
        IDE_TEST( assignPrimitiveValue( aMemory,
                                        aSourceColumn,
                                        aSourceValue,
                                        aDestColumn,
                                        aDestValue,
                                        aDestTemplate )
                  != IDE_SUCCESS );
    }
    // BUG-45701
    // source binary, dest array
    else if ( (aSourceColumn->type.dataTypeId == MTD_BINARY_ID) &&
              (aDestColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID) )
    {
        IDE_ERROR_RAISE( sTemplate->stmt != NULL, ERR_UNEXPECTED_ERROR );
        IDE_ERROR_RAISE( sTemplate->stmt->session != NULL, ERR_UNEXPECTED_ERROR );
        IDE_ERROR_RAISE( sTemplate->stmt->session->mMmSession != NULL, ERR_UNEXPECTED_ERROR );

        sIsBigEndianClient = qci::mSessionCallback.mIsBigEndianClient( sTemplate->stmt->session->mMmSession );

        IDE_TEST_RAISE( (UInt)aSourceColumn->precision < ID_SIZEOF(qsxArrayBindingHeader), ERR_CONVERSION_NOT_APPLICABLE );

        sHeader = (qsxArrayBindingHeader *)(((mtdBinaryType *)aSourceValue)->mValue);    
        sDstArrayInfo = *(qsxArrayInfo**)aDestValue;

        sFence = (SChar *)(((mtdBinaryType *)aSourceValue)->mValue) + aSourceColumn->precision;       

        if ( sIsBigEndianServer != sIsBigEndianClient )
        {
            sHeader->version = qsxUtil::reverseEndian( sHeader->version );
            sHeader->sqlType = qsxUtil::reverseEndian( sHeader->sqlType );
            sHeader->elemCount = qsxUtil::reverseEndian( sHeader->elemCount );
            sHeader->returnElemCount = qsxUtil::reverseEndian( sHeader->returnElemCount );
            sHeader->hasNull = qsxUtil::reverseEndian( sHeader->hasNull );
        }
        else
        {
            // Nothing to do.
        } 

        IDE_TEST_RAISE( sHeader->version != QSX_ARRAY_BINDING_VERSION, ERR_INVALID_ARRAY_BINDING_PROTOCOL );
 
        sDstTree = &sDstArrayInfo->avlTree;
 
        IDE_TEST( qsxArray::truncateArray( sDstArrayInfo )
                  != IDE_SUCCESS );
 
        sKeyCol  = sDstTree->keyCol;
        sDataCol = sDstTree->dataCol;
 
        IDE_DASSERT( sKeyCol  != NULL );
        IDE_DASSERT( sDataCol != NULL );

        switch( sDataCol->type.dataTypeId )
        {
            case MTD_SMALLINT_ID:
                sSize = 2;
                break;
            case MTD_INTEGER_ID:
                sSize = 4;
                break;
            case MTD_BIGINT_ID:
                sSize = 8;
                break;
            case MTD_REAL_ID:
                sSize = 4;
                break;
            case MTD_DOUBLE_ID:
                sSize = 8;
                break;
            default:
               IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE ); 
        }

        IDE_TEST_RAISE( sKeyCol->type.dataTypeId != MTD_INTEGER_ID, ERR_CONVERSION_NOT_APPLICABLE );
       
        IDE_TEST_RAISE( sHeader->sqlType != (SInt)sDataCol->type.dataTypeId, ERR_CONVERSION_NOT_APPLICABLE );
 
        for( i = 0; i < sHeader->elemCount; i++ ) 
        {
            sKey = i + 1;
            sKeyPtr  = (void *)&sKey;
            sDataPtr = (SChar*)(sHeader->data + i * sSize);

            IDE_TEST_RAISE( (SChar *)sDataPtr + sSize > sFence, ERR_ARRAY_INDEX_OUT_OF_RANGE );

            if ( sIsBigEndianServer != sIsBigEndianClient )
            {
                if ( sSize == 2 )
                {
                    *(UShort *)sDataPtr = qsxUtil::reverseEndian( *(UShort *)sDataPtr );
                }
                else if ( sSize == 4 )
                {
                    *(UInt *)sDataPtr = qsxUtil::reverseEndian( *(UInt *)sDataPtr );
                }
                else if ( sSize == 8 )
                {
                    *(ULong *)sDataPtr = qsxUtil::reverseEndian( *(ULong *)sDataPtr );
                }
            }
            else
            {
                // Nothing to do.
            }
 
            IDE_TEST( qsxAvl::insert( sDstTree,
                                      sKeyCol,
                                      sKeyPtr,
                                      &sDstRowPtr )
                      != IDE_SUCCESS );
 
            sDstRowPtr = (SChar*)sDstRowPtr + sDstTree->dataOffset;
 
            sActualSize = sDataCol->module->actualSize( sDataCol,
                                                        sDataPtr );
            idlOS::memcpy( sDstRowPtr, sDataPtr, sActualSize );
        }
    }
    // BUG-45701
    // source array, dest binary 
    else if ( (aDestColumn->type.dataTypeId == MTD_BINARY_ID) &&
              (aSourceColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID) )
    {
        IDE_ERROR_RAISE( sTemplate->stmt != NULL, ERR_UNEXPECTED_ERROR );
        IDE_ERROR_RAISE( sTemplate->stmt->session != NULL, ERR_UNEXPECTED_ERROR );
        IDE_ERROR_RAISE( sTemplate->stmt->session->mMmSession != NULL, ERR_UNEXPECTED_ERROR );

        sIsBigEndianClient = qci::mSessionCallback.mIsBigEndianClient( sTemplate->stmt->session->mMmSession );

        IDE_TEST_RAISE( (UInt)aDestColumn->precision < ID_SIZEOF(qsxArrayBindingHeader), ERR_CONVERSION_NOT_APPLICABLE );
        
        sHeader = (qsxArrayBindingHeader *)(((mtdBinaryType *)aDestValue)->mValue);    
        sSrcArrayInfo = *(qsxArrayInfo**)aSourceValue;

        sFence = (SChar *)(((mtdBinaryType *)aDestValue)->mValue) + aDestColumn->precision;       

        sSrcTree = &sSrcArrayInfo->avlTree;
 
        sKeyCol  = sSrcTree->keyCol;
        sDataCol = sSrcTree->dataCol;
 
        IDE_DASSERT( sKeyCol  != NULL );
        IDE_DASSERT( sDataCol != NULL );

        switch( sDataCol->type.dataTypeId )
        {
            case MTD_SMALLINT_ID:
                sSize = 2;
                break;
            case MTD_INTEGER_ID:
                sSize = 4;
                break;
            case MTD_BIGINT_ID:
                sSize = 8;
                break;
            case MTD_REAL_ID:
                sSize = 4;
                break;
            case MTD_DOUBLE_ID:
                sSize = 8;
                break;
            default:
               IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE ); 
        }

        IDE_TEST_RAISE( sKeyCol->type.dataTypeId != MTD_INTEGER_ID, ERR_CONVERSION_NOT_APPLICABLE );
        
        IDE_TEST( qsxAvl::searchMinMax( &sSrcArrayInfo->avlTree,
                                        AVL_LEFT,
                                        &sRowPtr,
                                        &sFound )
                  != IDE_SUCCESS );

        while( sFound != ID_FALSE )
        {
            sKeyPtr  = sRowPtr;
            sDataPtr = ((SChar*)sRowPtr + sSrcTree->dataOffset);

            IDE_TEST_RAISE( *(SInt *)sKeyPtr <= 0, ERR_ARRAY_INDEX_OUT_OF_RANGE );

            if ( sDataCol->module->isNull( sDataCol, sDataPtr ) == ID_TRUE )
            {
                sHasNull = 1;
            }
 
            sKey = *(SInt *)sKeyPtr;
            sKey = sKey - 1;

            // null padding
            for ( j = sPrevKey + 1; j < sKey; j++ )
            {
                sCursor = sHeader->data + j * sSize;
        
                IDE_TEST_RAISE( sCursor + sSize > sFence, ERR_ARRAY_INDEX_OUT_OF_RANGE );

                sDataCol->module->null( sDataCol, sCursor );

                if ( sIsBigEndianServer != sIsBigEndianClient )
                {
                    if ( sSize == 2 )
                    {
                        *(UShort *)sCursor = qsxUtil::reverseEndian( *(UShort *)sCursor );
                    }
                    else if ( sSize == 4 )
                    {
                        *(UInt *)sCursor = qsxUtil::reverseEndian( *(UInt *)sCursor );
                    }
                    else if ( sSize == 8 )
                    {
                        *(ULong *)sCursor= qsxUtil::reverseEndian( *(ULong *)sCursor );
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }

            sCursor = sHeader->data + sKey * sSize;
            
            IDE_TEST_RAISE( sCursor + sSize > sFence, ERR_ARRAY_INDEX_OUT_OF_RANGE );

            idlOS::memcpy( sCursor, sDataPtr, sSize ); 

            if ( sIsBigEndianServer != sIsBigEndianClient )
            {
                if ( sSize == 2 )
                {
                    *(UShort *)sCursor = qsxUtil::reverseEndian( *(UShort *)sCursor );
                }
                else if ( sSize == 4 )
                {
                    *(UInt *)sCursor = qsxUtil::reverseEndian( *(UInt *)sCursor );
                }
                else if ( sSize == 8 )
                {
                    *(ULong *)sCursor= qsxUtil::reverseEndian( *(ULong *)sCursor );
                }
            }
            else
            {
                // Nothing to do.
            }
 
            IDE_TEST( qsxAvl::searchNext( &sSrcArrayInfo->avlTree,
                                          sKeyCol,
                                          sKeyPtr,
                                          AVL_RIGHT,
                                          AVL_NEXT,
                                          &sRowPtr,
                                          &sFound )
                      != IDE_SUCCESS );
            sCount++;  

            sPrevKey = sKey;
        }
     
        if ( sCount != (sKey + 1) )
        {
            sHasNull = 1;
        }

        sHeader->version = QSX_ARRAY_BINDING_VERSION;
        sHeader->sqlType = sDataCol->type.dataTypeId;
        sHeader->elemCount = 0;
        sHeader->returnElemCount = sKey + 1;
        sHeader->hasNull = sHasNull; 

        ((mtdBinaryType *)aDestValue)->mLength = sSize * sHeader->returnElemCount 
                                                    + offsetof(qsxArrayBindingHeader, data);  

        if ( sIsBigEndianServer != sIsBigEndianClient )
        {
            sHeader->version = qsxUtil::reverseEndian( sHeader->version );
            sHeader->sqlType = qsxUtil::reverseEndian( sHeader->sqlType );
            sHeader->elemCount = qsxUtil::reverseEndian( sHeader->elemCount );
            sHeader->returnElemCount = qsxUtil::reverseEndian( sHeader->returnElemCount );
            sHeader->hasNull = qsxUtil::reverseEndian( sHeader->hasNull );
        }
        else
        {
            // Nothing to do.
        } 
    }
    else
    {
        // conversion not applicable.
        IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_ARRAY_INDEX_OUT_OF_RANGE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_ARRAY_INDEX_OUT_OF_RANGE));
    }
    IDE_EXCEPTION( ERR_INVALID_ARRAY_BINDING_PROTOCOL );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_INVALID_ARRAY_BINDING_PROTOCOL));
    }
    IDE_EXCEPTION( ERR_UNEXPECTED_ERROR );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR, "qsxUtil::assignValue", "Unexpected error"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsxUtil::assignNonPrimitiveValue (
    iduMemory    * aMemory,
    mtcColumn    * aSourceColumn,
    void         * aSourceValue,
    mtcColumn    * aDestColumn,
    void         * aDestValue,
    mtcTemplate  * aDestTemplate,
    idBool         aCopyRef )
{
/***********************************************************************
 *
 * Description : PROJ-1075 assign.
 *               �Ѵ� primitive �� �ƴѰ�쿡 ���� �з��Ͽ� assign�Ѵ�.
 *
 * Implementation : assign�� ������ ������ ���� �����Ѵ�.
 * 1) record := record (typeid����)
 *  - typeInfo�� ���� : assignPrimitiveValue
 *  - typeInfo�� �ٸ� : assignRowValue
 * 2) record := row  OR  row := record
 *  - assignRowValue
 * 3) row := row (typeid ����)
 *  - assignRowValue   BUGBUG. ���� execution�� tableInfo��
 *  - ��������? tableInfo��� �̸� ������ ����rowtype����
 *    Ȯ���� �� �� �ִ�. �̶��� �׳� assignPrimitiveValue��
 *    ȣ���ص� �ɰŰ�����..
 * 4) array := array (typeid ����)
 *  - assignArrayValue
 *  - BUGBUG. ���� nested table, varrayȮ��� ���� �ٲ��� ��.
 * 5) ��������� : ����
 *
 ***********************************************************************/
#define IDE_FN "qsxUtil::assignNonPrimitiveValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qtcModule* sSrcModule;
    qtcModule* sDestModule;

    // typeID�� ������ ���
    if( aDestColumn->type.dataTypeId == aSourceColumn->type.dataTypeId )
    {
        // �Ѵ� rowtype�� ��� assignRow
        if( aDestColumn->type.dataTypeId == MTD_ROWTYPE_ID )
        {
            IDE_TEST( assignRowValue( aMemory,
                                      aSourceColumn,
                                      aSourceValue,
                                      aDestColumn,
                                      aDestValue,
                                      aDestTemplate,
                                      aCopyRef )
                      != IDE_SUCCESS );
        }
        // �Ѵ� recordtype�� ���
        else if ( aDestColumn->type.dataTypeId == MTD_RECORDTYPE_ID )
        {
            sSrcModule = (qtcModule*)aSourceColumn->module;
            sDestModule = (qtcModule*)aDestColumn->module;

            // recordtype�� �ݵ�� ����Ÿ���̾�߸� ��.
            if( sSrcModule->typeInfo == sDestModule->typeInfo )
            {
                // ���ռ� �˻�.
                // ���� type�̹Ƿ� typesize�� �����ϴ�.
                IDE_DASSERT( sSrcModule->typeInfo->typeSize ==
                             sDestModule->typeInfo->typeSize );

                // ���ռ� �˻�.
                // ���� type�̹Ƿ� columnCount�� �����ϴ�.
                IDE_DASSERT( sSrcModule->typeInfo->columnCount ==
                             sDestModule->typeInfo->columnCount );

                // PROJ-1904 Extend UDT
                if ( (sSrcModule->typeInfo->flag & QTC_UD_TYPE_HAS_ARRAY_MASK) ==
                     QTC_UD_TYPE_HAS_ARRAY_TRUE )
                {
                    IDE_TEST( assignRowValue( aMemory,
                                              aSourceColumn,
                                              aSourceValue,
                                              aDestColumn,
                                              aDestValue,
                                              aDestTemplate,
                                              aCopyRef )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( assignPrimitiveValue( aMemory,
                                                    aSourceColumn,
                                                    aSourceValue,
                                                    aDestColumn,
                                                    aDestValue,
                                                    aDestTemplate )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // conversion not applicable
                IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
            }
        }
        // �Ѵ� array�� ���
        else if ( aDestColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
        {
            sSrcModule = (qtcModule*)aSourceColumn->module;
            sDestModule = (qtcModule*)aDestColumn->module;

            // arraytype�� �ݵ�� ����Ÿ���̾�߸� ��.
            if( sSrcModule->typeInfo == sDestModule->typeInfo )
            {
                if( aCopyRef == ID_TRUE )
                {
                    IDE_TEST( qsxArray::finalizeArray( (qsxArrayInfo**)aDestValue )
                              != IDE_SUCCESS );

                    IDE_TEST_RAISE( *((qsxArrayInfo**)aSourceValue) == NULL, ERR_INVALID_ARRAY );

                    // reference copy�� ���
                    IDE_TEST( assignPrimitiveValue( aMemory,
                                                    aSourceColumn,
                                                    aSourceValue,
                                                    aDestColumn,
                                                    aDestValue,
                                                    aDestTemplate )
                              != IDE_SUCCESS );

                    (*((qsxArrayInfo**)aSourceValue))->avlTree.refCount++;
                }
                else
                {
                    // reference copy�� �ƴ� ���
                    // array�� data�� ���� copy
                    IDE_TEST( assignArrayValue( aSourceValue,
                                                aDestValue,
                                                aDestTemplate )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // conversion not applicable.
                IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
            }
        }
        else if ( aDestColumn->type.dataTypeId == MTD_REF_CURSOR_ID )
        {
            sSrcModule = (qtcModule*)aSourceColumn->module;
            sDestModule = (qtcModule*)aDestColumn->module;

            // ref cursor type�� �ݵ�� ����Ÿ���̾�߸� ��.
            if( sSrcModule->typeInfo == sDestModule->typeInfo )
            {
                // reference copy�� ���
                IDE_TEST( assignPrimitiveValue( aMemory,
                                                aSourceColumn,
                                                aSourceValue,
                                                aDestColumn,
                                                aDestValue,
                                                aDestTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // conversion not applicable.
                IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
            }

        }
        else
        {
            // conversion not applicable
            IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
        }
    }
    // type�� ���� �ٸ� ���
    else
    {
        // ���� �ϳ��� array type�� ������ ���� ȣȯ�Ұ�.
        // record-row, row-record�� ȣȯ����.
        if( ( aDestColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID ) ||
            ( aSourceColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID ) )
        {
            // conversion not applicable.
            IDE_RAISE( ERR_CONVERSION_NOT_APPLICABLE );
        }
        else
        {
            IDE_TEST( assignRowValue( aMemory,
                                      aSourceColumn,
                                      aSourceValue,
                                      aDestColumn,
                                      aDestValue,
                                      aDestTemplate,
                                      aCopyRef )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxUtil::assignRowValue (
    iduMemory    * aMemory,
    mtcColumn    * aSourceColumn,
    void         * aSourceValue,
    mtcColumn    * aDestColumn,
    void         * aDestValue,
    mtcTemplate  * aDestTemplate,
    idBool         aCopyRef )
{
/***********************************************************************
 *
 * Description : PROJ-1075
 *               rowtype�� ���� assign�Ѵ�.
 *
 * Implementation :
 *    (1) source, dest column�� module�� �����Ͽ�
 *        columnCount�� ��ġ�ϴ��� �˻��Ѵ�.
          ��ġ���� ������ conversion not applicable.
 *    (2) columnCount�� ������Ű�鼭 Column�� �ּҸ� ������Ų��.
 *        ->row/record type�� ���� template�� ���� �÷��� ������� �ڸ����
 *          �ִ�.
 *    (3) ���� column�� �̿��Ͽ� mtc::value�Լ��� ȣ���ؼ� ���ο�
 *        value��ġ�� source, dest ��� ����.
 *    (4) ���Ӱ� ���� column, value�� ������ assignPrimitiveValueȣ��.
 *    (5) column ������ŭ (2) ~ (4) �ݺ�.
 *
 ***********************************************************************/
#define IDE_FN "qsxUtil::assignRowValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qtcModule* sSrcModule;
    qtcModule* sDestModule;
    qcmColumn* sSrcRowColumn;
    qcmColumn* sDestRowColumn;

    UInt       sSrcColumnCount;
    UInt       sDestColumnCount;

    void*      sSrcColumnValue;
    void*      sDestColumnValue;

    // row/record type�� ����� ���� ���(qtcModule)�� Ȯ��.
    sSrcModule = (qtcModule*)aSourceColumn->module;
    sDestModule = (qtcModule*)aDestColumn->module;

    // module->typeInfo->columnCount�� ����.
    sSrcColumnCount = sSrcModule->typeInfo->columnCount;
    sDestColumnCount = sDestModule->typeInfo->columnCount;

    // column count�� ���� �ٸ��ٸ� conversion �Ұ���. ����.
    IDE_TEST_RAISE( sSrcColumnCount != sDestColumnCount,
                    ERR_CONVERSION_NOT_APPLICABLE );

    for( sSrcRowColumn = sSrcModule->typeInfo->columns,
             sDestRowColumn = sDestModule->typeInfo->columns;
         sSrcRowColumn != NULL &&
             sDestRowColumn != NULL;
         sSrcRowColumn = sSrcRowColumn->next,
             sDestRowColumn = sDestRowColumn->next)
    {
        // row/record type������ ������ ���� �÷���
        // ���� ��.
        sSrcColumnValue = (void*)mtc::value( sSrcRowColumn->basicInfo,
                                             aSourceValue,
                                             MTD_OFFSET_USE );
        sDestColumnValue = (void*)mtc::value( sDestRowColumn->basicInfo,
                                              aDestValue,
                                              MTD_OFFSET_USE );

        if ( (sSrcRowColumn->basicInfo->type.dataTypeId == MTD_RECORDTYPE_ID) ||
             (sSrcRowColumn->basicInfo->type.dataTypeId == MTD_ROWTYPE_ID) ||
             (sSrcRowColumn->basicInfo->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID) )
        {
            IDE_TEST( assignNonPrimitiveValue( aMemory,
                                               sSrcRowColumn->basicInfo,
                                               sSrcColumnValue,
                                               sDestRowColumn->basicInfo,
                                               sDestColumnValue,
                                               aDestTemplate,
                                               aCopyRef )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( assignPrimitiveValue( aMemory,
                                            sSrcRowColumn->basicInfo,
                                            sSrcColumnValue,
                                            sDestRowColumn->basicInfo,
                                            sDestColumnValue,
                                            aDestTemplate )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsxUtil::assignRowValueFromStack(
    iduMemory    * aMemory,
    mtcStack     * aSourceStack,
    SInt           aSourceStackRemain,
    qtcNode      * aDestNode,
    qcTemplate   * aDestTemplate,
    UInt           aTargetCount )
{
    mtcColumn* sDestColumn;

    mtcStack*  sStack;
    SInt       sRemain;

    mtcStack*  sDestStack;

    qtcModule* sDestModule;
    qcmColumn* sDestRowColumn;

    void*      sDestColumnValue;
    void*      sDestValue;

    sDestColumn = QTC_TMPL_COLUMN( aDestTemplate, aDestNode );
    sDestModule = (qtcModule*)sDestColumn->module;
    // row/record type�� ����� ���� ���(qtcModule)�� Ȯ��.

    sDestStack  = aDestTemplate->tmplate.stack;
    sDestValue  = sDestStack->value;

    IDE_TEST_RAISE( aTargetCount != sDestModule->typeInfo->columnCount,
                    ERR_CONVERSION_NOT_APPLICABLE );

    IDE_TEST_RAISE( aSourceStack == NULL , ERR_UNEXPECTED );

    for( sStack = aSourceStack, sRemain = aSourceStackRemain,
             sDestRowColumn = sDestModule->typeInfo->columns;
             sDestRowColumn != NULL;
         sStack++, sRemain--,
             sDestRowColumn = sDestRowColumn->next)
    {
        IDE_TEST_RAISE( sRemain < 1, ERR_STACK_OVERFLOW );

        // row/record type������ ������ ���� �÷��� ���� ��.
        sDestColumnValue = (void*)mtc::value( sDestRowColumn->basicInfo,
                                              sDestValue,
                                              MTD_OFFSET_USE );

        // ������ ���� �÷����� primitive type��.
        IDE_TEST( assignPrimitiveValue( aMemory,
                                        sStack->column,
                                        sStack->value,
                                        sDestRowColumn->basicInfo,
                                        sDestColumnValue,
                                        &aDestTemplate->tmplate )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_STACK_OVERFLOW));
    }
    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_UNEXPECTED );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qsxUtil::assignRowValueFromStack",
                                  "Column Value is null" ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qsxUtil::assignArrayValue (
    void         * aSourceValue,
    void         * aDestValue,
    mtcTemplate  * aDestTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1075
 *               arraytype�� ���� assign�Ѵ�.
 *
 * Implementation :
 *    (1) qsxArray::assign�Լ� ȣ��.
 *
 ***********************************************************************/
#define IDE_FN "qsxUtil::assignArrayValue"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    IDE_TEST_RAISE( *(qsxArrayInfo**)aDestValue == NULL,
                    ERR_INVALID_ARRAY );

    IDE_TEST_RAISE( *(qsxArrayInfo**)aSourceValue == NULL,
                    ERR_INVALID_ARRAY );

    IDE_TEST( qsxArray::assign( aDestTemplate,
                                *(qsxArrayInfo**)aDestValue,
                                *(qsxArrayInfo**)aSourceValue )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


// BUG-11192 date_format session property
// mtv::executeConvert()�� mtcTemplate�� �ʿ���
// aDestTemplate ���� �߰�.
// by kumdory, 2005-04-08

// PROJ-1075 assignPrimitiveValue�� ����.
// �� �Լ��� primitive value�Ǵ� ���� recordtype value
// assign�ÿ��� ȣ���.
IDE_RC qsxUtil::assignPrimitiveValue (
    iduMemory    * aMemory,
    mtcColumn    * aSourceColumn,
    void         * aSourceValue,
    mtcColumn    * aDestColumn,
    void         * aDestValue,
    mtcTemplate  * aDestTemplate )
{
    mtcColumn          * sDestColumn;
    void               * sDestValue;
    mtvConvert         * sConvert;
    UInt                 sSourceArgCount;
    UInt                 sDestActualSize;
    void               * sDestCanonizedValue;
    iduMemoryStatus      sMemStatus;
    UInt                 sStage = 0;

    // PROJ-2002 Column Security
    // PSM������ ��ȣ �÷� Ÿ���� ����� �� ����.
    IDE_DASSERT( (aSourceColumn->type.dataTypeId != MTD_ECHAR_ID) &&
                 (aSourceColumn->type.dataTypeId != MTD_EVARCHAR_ID) );

    IDE_DASSERT( (aDestColumn->type.dataTypeId != MTD_ECHAR_ID) &&
                 (aDestColumn->type.dataTypeId != MTD_EVARCHAR_ID) );

    IDE_TEST( aMemory->getStatus( &sMemStatus ) != IDE_SUCCESS );
    sStage = 1;

    // check conversion
    /* PROJ-1361 : data module�� language module �и����� 
    if (aDestColumn->type.type     == aSourceColumn->type.type &&
        aDestColumn->type.language == aSourceColumn->type.language)
    */
    if ( aDestColumn->type.dataTypeId == aSourceColumn->type.dataTypeId )
    {
        // same type
        sDestColumn = aSourceColumn;
        sDestValue  = aSourceValue;
    }
    else
    {
        // convert
        sSourceArgCount = aSourceColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;

        IDE_TEST(mtv::estimateConvert4Server(
                    aMemory,
                    &sConvert,
                    aDestColumn->type,        // aDestType
                    aSourceColumn->type,      // aSourceType
                    sSourceArgCount,          // aSourceArgument
                    aSourceColumn->precision, // aSourcePrecision
                    aSourceColumn->scale,     // aSourceScale 
                    aDestTemplate)            // mtcTemplate* : for passing session dateFormat
                != IDE_SUCCESS);

        // source value pointer
        sConvert->stack[sConvert->count].value = aSourceValue;

        // Dest value pointer
        sDestColumn = sConvert->stack[0].column;
        sDestValue  = sConvert->stack[0].value;

        IDE_TEST(mtv::executeConvert( sConvert, aDestTemplate )
                 != IDE_SUCCESS);
    }

    // canonize
    if ( ( aDestColumn->module->flag & MTD_CANON_MASK )
         == MTD_CANON_NEED )
    {
        sDestCanonizedValue = sDestValue;

        IDE_TEST( aDestColumn->module->canonize(
                      aDestColumn,
                      &sDestCanonizedValue,           // canonized value
                      NULL,
                      sDestColumn,
                      sDestValue,                     // original value
                      NULL,
                      aDestTemplate )
                  != IDE_SUCCESS );

        sDestValue = sDestCanonizedValue;
    }
    else if ( ( aDestColumn->module->flag & MTD_CANON_MASK )
              == MTD_CANON_NEED_WITH_ALLOCATION )
    {
        IDE_TEST(aMemory->alloc(aDestColumn->column.size,
                                (void**)&sDestCanonizedValue)
                 != IDE_SUCCESS);

        IDE_TEST( aDestColumn->module->canonize(
                      aDestColumn,
                      & sDestCanonizedValue,           // canonized value
                      NULL,
                      sDestColumn,
                      sDestValue,                      // original value
                      NULL,
                      aDestTemplate )
                  != IDE_SUCCESS );

        sDestValue = sDestCanonizedValue;
    }

    sDestActualSize = aDestColumn->module->actualSize(
                          aDestColumn,
                          sDestValue );

    //fix BUG-16274
    if( aDestValue != sDestValue )
    {
        idlOS::memcpy( aDestValue,
                       sDestValue,
                       sDestActualSize );
    }

    sStage = 0;

    IDE_TEST( aMemory-> setStatus( &sMemStatus ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sStage )
    {
        case 1 :
            if ( aMemory-> setStatus( &sMemStatus ) != IDE_SUCCESS )
            {
                IDE_ERRLOG(IDE_QP_1);
            }
    }

    return IDE_FAILURE;
}

IDE_RC qsxUtil::calculateBoolean (
    qtcNode      * aNode,
    qcTemplate   * aTemplate,
    idBool       * aIsTrue )
{
    #define IDE_FN "IDE_RC qsxUtil::calculateBoolean"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcColumn   * sColumn;
    void        * sValue;

    IDE_TEST( qtc::calculate( aNode,
                              aTemplate ) != IDE_SUCCESS );

    sColumn = aTemplate->tmplate.stack[0].column ;
    sValue  = aTemplate->tmplate.stack[0].value ;

    IDE_TEST( sColumn->module->isTrue( aIsTrue,
                                       sColumn,
                                       sValue )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

    #undef IDE_FN
}


IDE_RC qsxUtil::calculateAndAssign (
    iduMemory    * aMemory,
    qtcNode      * aSourceNode,
    qcTemplate   * aSourceTemplate,
    qtcNode      * aDestNode,
    qcTemplate   * aDestTemplate,
    idBool         aCopyRef )
{
    IDE_TEST( qtc::calculate( aSourceNode,
                              aSourceTemplate )
              != IDE_SUCCESS );

    IDE_TEST( assignValue ( aMemory,
                            aSourceTemplate->tmplate.stack,
                            aSourceTemplate->tmplate.stackRemain,
                            aDestNode,
                            aDestTemplate,
                            aCopyRef )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1335 RAISE ����
// RAISE system_defined_exception;�� ��� ������ �����ڵ尡 �����Ǿ�� �Ѵ�.
typedef struct qsxIdNameMap
{
    SInt  id;
    SChar name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt  errorCode;
} qsxIdNameMap;

// PROJ-1335 RAISE ����
// ������ system_defined_exception�� �˸��� ��ǥ �����ڵ带 ���Խ�Ŵ.
qsxIdNameMap excpIdNameMap[] =
{
    { 
        QSX_CURSOR_ALREADY_OPEN_NO, "CURSOR_ALREADY_OPEN", qpERR_ABORT_QSX_CURSOR_ALREADY_OPEN
    },
    {
        QSX_DUP_VAL_ON_INDEX_NO, "DUP_VAL_ON_INDEX", qpERR_ABORT_QSX_DUP_VAL_ON_INDEX
    },
    {
        QSX_INVALID_CURSOR_NO, "INVALID_CURSOR", qpERR_ABORT_QSX_INVALID_CURSOR
    },
    {
        QSX_INVALID_NUMBER_NO, "INVALID_NUMBER", qpERR_ABORT_QSX_INVALID_NUMBER
    },
    {
        QSX_NO_DATA_FOUND_NO, "NO_DATA_FOUND", qpERR_ABORT_QSX_NO_DATA_FOUND
    },
    {
        QSX_PROGRAM_ERROR_NO, "PROGRAM_ERROR", qpERR_ABORT_QSX_PROGRAM_ERROR
    },
    {
        QSX_STORAGE_ERROR_NO, "STORAGE_ERROR", qpERR_ABORT_QSX_STORAGE_ERROR
    },
    {
        QSX_TIMEOUT_ON_RESOURCE_NO, "TIMEOUT_ON_RESOURCE", qpERR_ABORT_QSX_TIMEOUT_ON_RESOURCE
    },
    {
        QSX_TOO_MANY_ROWS_NO, "TOO_MANY_ROWS", qpERR_ABORT_QSX_TOO_MANY_ROWS
    },
    {
        QSX_VALUE_ERROR_NO, "VALUE_ERROR", qpERR_ABORT_QSX_VALUE_ERROR
    },
    {
        QSX_ZERO_DIVIDE_NO, "ZERO_DIVIDE", qpERR_ABORT_QSX_ZERO_DIVIDE
    },
    {
        QSX_INVALID_PATH_NO, "INVALID_PATH", qpERR_ABORT_QSX_FILE_INVALID_PATH
    },
    {
        QSX_INVALID_MODE_NO, "INVALID_MODE", qpERR_ABORT_QSX_INVALID_FILEOPEN_MODE
    },
    {
        QSX_INVALID_FILEHANDLE_NO, "INVALID_FILEHANDLE", qpERR_ABORT_QSX_FILE_INVALID_FILEHANDLE
    },
    {
        QSX_INVALID_OPERATION_NO, "INVALID_OPERATION", qpERR_ABORT_QSX_FILE_INVALID_OPERATION
    },
    {
        QSX_READ_ERROR_NO, "READ_ERROR", qpERR_ABORT_QSX_FILE_READ_ERROR
    },
    {
        QSX_WRITE_ERROR_NO, "WRITE_ERROR", qpERR_ABORT_QSX_FILE_WRITE_ERROR
    },
    {
        QSX_ACCESS_DENIED_NO, "ACCESS_DENIED", qpERR_ABORT_QSX_DIRECTORY_ACCESS_DENIED
    },
    {
        QSX_DELETE_FAILED_NO, "DELETE_FAILED", qpERR_ABORT_QSX_FILE_DELETE_FAILED
    },
    {
        QSX_RENAME_FAILED_NO, "RENAME_FAILED", qpERR_ABORT_QSX_FILE_RENAME_FAILED
    },
    {
        QSX_SHARD_MULTIPLE_ERRORS_NO, "SHARD_MULTIPLE_ERRORS", sdERR_ABORT_SHARD_MULTIPLE_ERRORS
    },
    {
        0, "", 0
    }
};



IDE_RC qsxUtil::getSystemDefinedException (
SChar           * aStmtText,
qcNamePosition  * aName,
SInt            * aId,
UInt            * aErrorCode )
{
    #define IDE_FN "qsxUtil::qsxGetSystemDefinedExceptionId"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SInt i;
    for (i=0; excpIdNameMap[i].id < 0; i++)
    {
        if (idlOS::strMatch( 
            aStmtText + aName->offset,
            aName->size, 
            excpIdNameMap[i].name,
            idlOS::strlen(excpIdNameMap[i].name) ) == 0 )
        {
            *aId = excpIdNameMap[i].id;
            *aErrorCode = excpIdNameMap[i].errorCode;
            return IDE_SUCCESS;
        }
    }
    *aId = 0;
    *aErrorCode = 0;

    IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_INTERNAL_SERVER_ERROR_ARG,
                            "unknown system exception"));

    return IDE_FAILURE;


    #undef IDE_FN

}


IDE_RC qsxUtil::getSystemDefinedExceptionName (
SInt    aId,     
SChar **aName)
{
    #define IDE_FN "qsxUtil::qsxGetSystemDefinedExceptionName"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SInt i;
    for (i=0; excpIdNameMap[i].id < 0; i++)
    {
        if ( aId == excpIdNameMap[i].id )
        {
            (*aName) = excpIdNameMap[i].name;
            return IDE_SUCCESS;
        }
    }

    return IDE_FAILURE;

    #undef IDE_FN
}

IDE_RC
qsxUtil::arrayReturnToInto( qcTemplate         * aTemplate,
                            qcTemplate         * aDestTemplate,
                            qmmReturnValue     * aReturnValue,
                            qmmReturnIntoValue * aReturnIntoValue,
                            vSLong               aRowCnt )
{
/***********************************************************************
 *
 * Description : PROJ-1584 DML Return Clause 
 *  PSM Array Type RETURN Clause (Column Value) -> INTO Clause (Column Value)��
 *  ���� �Ͽ� �ش�.
 * 
 * Implementation :
 *      (1) Return Clause -> INTO Clause value copy.
 *
 * BUG-45157 fix
 *   - ������ qmmReturnInto�� qmmReturnValue, qmmReturnIntoValue�� ����.
 *    - aReturnValue�� aTemplate�� �ѽ��̰�
 *    - aReturnIntoValue�� aDestTemplate�� �ѽ��̴�.
 *
 ***********************************************************************/

    void               * sSrcValue;
    mtcColumn          * sSrcColumn;
    qmmReturnValue     * sReturnValue;
    qmmReturnIntoValue * sReturnIntoValue;
    qtcNode            * sIndexNode;
    mtcColumn          * sIndexColumn;
    void               * sIndexValue;

    sReturnValue     = aReturnValue;
    sReturnIntoValue = aReturnIntoValue;

    // DML�� �������� �� return into�� ó���ϹǷ� aRowCnt�� 0�� �� ����.
    IDE_DASSERT( aRowCnt != 0 );

    // index overflow
    IDE_TEST_RAISE( aRowCnt >= (vSLong)MTD_INTEGER_MAXIMUM,
                    err_index_overflow );

    for( ;
         sReturnValue     != NULL;
         sReturnValue     = sReturnValue->next,
         sReturnIntoValue = sReturnIntoValue->next )
    {
        IDE_TEST( qtc::calculate( sReturnValue->returnExpr,
                                  aTemplate )
                  != IDE_SUCCESS );

        sSrcColumn = aTemplate->tmplate.stack->column;
        sSrcValue  = aTemplate->tmplate.stack->value;

        // decrypt�Լ��� �ٿ����Ƿ� ��ȣ�÷��� ���� �� ����.
        IDE_DASSERT( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                     == MTD_ENCRYPT_TYPE_FALSE );

        // Associative Array�� index node�� node�� arguments�̱� ������
        // �ݵ�� node�� arguments�� �־�� �Ѵ�.
        // ARRAY_NODE[1]
        // ^ NODE     ^ INDEX NODE(ARRAY_NODE�� arguments)
        IDE_DASSERT( sReturnIntoValue->returningInto->node.arguments != NULL );

        // BUG-42715
        // Array�� ó�� return value�� ���� ���� array�� �ʱ�ȭ �Ѵ�.
        // package ������ ���� ����ϸ� aTemplate�� aDestTemplate�� �����ϸ�,
        // �� ��쿡�� array�� truncate �Ѵ�.
        if ( ( aRowCnt == 1 ) && ( aTemplate == aDestTemplate ) )
        {
            IDE_TEST( truncateArray( aTemplate->stmt,
                                     sReturnIntoValue->returningInto )
                      != IDE_SUCCESS );
        }

        sIndexNode   = (qtcNode*)sReturnIntoValue->returningInto->node.arguments;
        sIndexColumn = QTC_TMPL_COLUMN( aDestTemplate, sIndexNode );
        sIndexValue  = QTC_TMPL_FIXEDDATA( aDestTemplate, sIndexNode );

        IDE_ASSERT( sIndexColumn->module->id == MTD_INTEGER_ID );

        *(mtdIntegerType*)sIndexValue = (mtdIntegerType)(aRowCnt);

        IDE_TEST( qtc::calculate( sReturnIntoValue->returningInto,
                                  aDestTemplate )
                  != IDE_SUCCESS );

        IDE_TEST( qsxUtil::assignValue( QC_QMX_MEM(aTemplate->stmt),
                                        sSrcColumn,
                                        sSrcValue,
                                        aDestTemplate->tmplate.stack->column,
                                        aDestTemplate->tmplate.stack->value,
                                        &aDestTemplate->tmplate,
                                        ID_FALSE )
                 != IDE_SUCCESS );
    } // end for

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_index_overflow);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_ARRAY_INDEX_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qsxUtil::recordReturnToInto( qcTemplate         * aTemplate,
                             qcTemplate         * aDestTemplate,
                             qmmReturnValue     * aReturnValue,
                             qmmReturnIntoValue * aReturnIntoValue,
                             vSLong               aRowCnt,
                             idBool               aIsBulk )
{
/***********************************************************************
 *
 * Description : PROJ-1584 DML Return Clause 
 *  PSM record/row Type RETURN Clause (Column Value) -> INTO Clause (Column Value)��
 *  ���� �Ͽ� �ش�.
 * 
 * Implementation :
 *      (1) Return Clause -> INTO Clause value copy.
 *
 * BUG-45157 fix
 *   - ������ qmmReturnInto�� qmmReturnValue, qmmReturnIntoValue�� ����.
 *    - aReturnValue�� aTemplate�� �ѽ��̰�
 *    - aReturnIntoValue�� aDestTemplate�� �ѽ��̴�.
 *
 ***********************************************************************/

    mtcColumn      * sSrcColumn;
    qmmReturnValue * sReturnValue;
    qtcNode        * sReturnIntoNode;
    qtcNode        * sIndexNode;
    mtcColumn      * sIndexColumn;
    void           * sIndexValue;
    mtcStack       * sOriStack;
    SInt             sOriRemain;
    UInt             sTargetCount = 0;

    sOriStack  = aTemplate->tmplate.stack;
    sOriRemain = aTemplate->tmplate.stackRemain;

    IDE_DASSERT( aReturnValue != NULL );
    IDE_DASSERT( aReturnIntoValue != NULL );

    // DML�� �������� �� return into�� ó���ϹǷ� aRowCnt�� 0�� �� ����.
    IDE_DASSERT( aRowCnt != 0 );

    // index overflow
    IDE_TEST_RAISE( aRowCnt >= (vSLong)MTD_INTEGER_MAXIMUM,
                    err_invalid_cursor );

    /* RETURN Clause */
    for( sReturnValue = aReturnValue;
         sReturnValue != NULL;
         sReturnValue = sReturnValue->next )
    {
        IDE_TEST( qtc::calculate( sReturnValue->returnExpr,
                                  aTemplate )
                  != IDE_SUCCESS );

        sSrcColumn = aTemplate->tmplate.stack->column;

        // decrypt�Լ��� �ٿ����Ƿ� ��ȣ�÷��� ���� �� ����.
        IDE_DASSERT( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                     == MTD_ENCRYPT_TYPE_FALSE );

        aTemplate->tmplate.stack++;
        aTemplate->tmplate.stackRemain--;

        sTargetCount++;
    }

    /* INTO Clause */
    sReturnIntoNode = aReturnIntoValue->returningInto;

    if( aIsBulk == ID_TRUE )
    {
        IDE_DASSERT( sReturnIntoNode->node.arguments != NULL );
        IDE_DASSERT( sReturnIntoNode->node.next      == NULL );

        sIndexNode   = (qtcNode*)sReturnIntoNode->node.arguments;
        sIndexColumn = QTC_TMPL_COLUMN( aDestTemplate, sIndexNode );
        sIndexValue  = QTC_TMPL_FIXEDDATA( aDestTemplate, sIndexNode );

        IDE_ASSERT( sIndexColumn->module->id == MTD_INTEGER_ID );

        *(mtdIntegerType*)sIndexValue = (mtdIntegerType)(aRowCnt);

        // BUG-42715
        // Array�� ó�� return value�� ���� ���� array�� �ʱ�ȭ �Ѵ�.
        // package ������ ���� ����ϸ� aTemplate�� aDestTemplate�� �����ϸ�,
        // �� ��쿡�� array�� truncate �Ѵ�.
        if ( ( aRowCnt == 1 ) && ( aTemplate == aDestTemplate ) )
        {
            IDE_TEST( truncateArray( aTemplate->stmt,
                                     sReturnIntoNode )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    /* RETURNING i1, i2 INTO r1;
     * i1, i2��   aTemplate->tmplate.stack ���� ����ϰ�,
     * r1�� aDestTemplate->tmplate.stack ���� ����Ѵ�. */
    IDE_TEST( qtc::calculate( sReturnIntoNode,
                              aDestTemplate )
              != IDE_SUCCESS );

    // BUG-42715
    // return value�� original stack�� �����Ƿ�
    // assignRowValueFromStack�� ȣ���� �� original stack�� �ѱ��.
    // aTemplate�� stack�� assign�� ���� �Ŀ� �����Ѵ�.
    IDE_TEST( qsxUtil::assignRowValueFromStack( QC_QMX_MEM(aDestTemplate->stmt),
                                                sOriStack,
                                                sOriRemain,
                                                sReturnIntoNode,
                                                aDestTemplate,
                                                sTargetCount )
              != IDE_SUCCESS );

    aTemplate->tmplate.stack       = sOriStack;
    aTemplate->tmplate.stackRemain = sOriRemain;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalid_cursor);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_INVALID_CURSOR));
    }
    IDE_EXCEPTION_END;

    aTemplate->tmplate.stack       = sOriStack;
    aTemplate->tmplate.stackRemain = sOriRemain;

    return IDE_FAILURE;
}

IDE_RC
qsxUtil::primitiveReturnToInto( qcTemplate         * aTemplate,
                                qcTemplate         * aDestTemplate,
                                qmmReturnValue     * aReturnValue,
                                qmmReturnIntoValue * aReturnIntoValue )
{
/***********************************************************************
 *
 * Description : BUG-36131
 *  PSM primitive Type RETURN Clause (Column Value) -> INTO Clause (Column Value)��
 *  ���� �Ͽ� �ش�.
 * 
 * Implementation :
 *      (1) Return Clause -> INTO Clause value copy.
 *
 * BUG-45157 fix
 *   - ������ qmmReturnInto�� qmmReturnValue, qmmReturnIntoValue�� ����.
 *    - aReturnValue�� aTemplate�� �ѽ��̰�
 *    - aReturnIntoValue�� aDestTemplate�� �ѽ��̴�.
 *
 ***********************************************************************/

    void               * sSrcValue;
    mtcColumn          * sSrcColumn;
    qmmReturnValue     * sReturnValue;
    qmmReturnIntoValue * sReturnIntoValue;

    sReturnValue     = aReturnValue;
    sReturnIntoValue = aReturnIntoValue;

    for( ;
         sReturnValue     != NULL;
         sReturnValue     = sReturnValue->next,
         sReturnIntoValue = sReturnIntoValue->next )
    {
        IDE_TEST( qtc::calculate( sReturnValue->returnExpr,
                                  aTemplate )
                  != IDE_SUCCESS );

        sSrcColumn = aTemplate->tmplate.stack->column;
        sSrcValue  = aTemplate->tmplate.stack->value;

        // decrypt�Լ��� �ٿ����Ƿ� ��ȣ�÷��� ���� �� ����.
        IDE_DASSERT( (sSrcColumn->module->flag & MTD_ENCRYPT_TYPE_MASK)
                     == MTD_ENCRYPT_TYPE_FALSE );

        IDE_TEST( qtc::calculate( sReturnIntoValue->returningInto,
                                  aDestTemplate )
                  != IDE_SUCCESS );

        IDE_TEST( qsxUtil::assignValue( QC_QMX_MEM(aTemplate->stmt),
                                        sSrcColumn,
                                        sSrcValue,
                                        aDestTemplate->tmplate.stack->column,
                                        aDestTemplate->tmplate.stack->value,
                                        &aDestTemplate->tmplate,
                                        ID_FALSE )
                 != IDE_SUCCESS );
    } // end for

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

// BUG-37011
IDE_RC 
qsxUtil::truncateArray ( qcStatement * aQcStmt,
                         qtcNode     * aQtcNode )
{
    qtcNode       * sQtcNode;
    qtcColumnInfo * sColumnInfo;
    qsxArrayInfo  * sArrayInfo;
    qtcNode         sNode;
    // BUG-38292
    void          * sInfo;
    mtcNode       * sRealNode;
    qcTemplate    * sTemplate;
    qsxPkgInfo    * sPkgInfo;

    for( sQtcNode = aQtcNode;
         sQtcNode != NULL;
         sQtcNode = (qtcNode*)sQtcNode->node.next )
    {
        IDE_DASSERT( sQtcNode->node.arguments != NULL );

        /* BUG-38292
           array type variable�� package spec�� ����� ���,
           indirect calculate ������� �����ȴ�.
           indirect caclulate ����� �� calculateInfo��
           array type variable�� ���� ������ ���õǾ� �ִ�. 
           ����, sQtcNode->node.objectID�� ���� �Ǵ��ؼ�
           array type variable�� ������ �����;� �Ѵ�.*/
        sInfo = QTC_TMPL_EXECUTE(QC_PRIVATE_TMPLATE(aQcStmt),
                                 sQtcNode)->calculateInfo;

        if ( sQtcNode->node.objectID == QS_EMPTY_OID )
        {
            sColumnInfo = (qtcColumnInfo*)sInfo;

            sNode.node.table    = sColumnInfo->table;
            sNode.node.column   = sColumnInfo->column;
            sNode.node.objectID = sColumnInfo->objectId;

            sTemplate   = QC_PRIVATE_TMPLATE(aQcStmt);
        }
        else
        {
            sRealNode = (mtcNode *)sInfo;

            IDE_TEST( qsxPkg::getPkgInfo( sRealNode->objectID,
                                          &sPkgInfo )
                      != IDE_SUCCESS );

            IDE_TEST( qcuSessionPkg::searchPkgInfoFromSession(
                          aQcStmt,
                          sPkgInfo,
                          QC_PRIVATE_TMPLATE(aQcStmt)->tmplate.stack,
                          QC_PRIVATE_TMPLATE(aQcStmt)->tmplate.stackRemain,
                          &sTemplate ) != IDE_SUCCESS );

            sNode.node.table    = sRealNode[1].table;
            sNode.node.column   = sRealNode[1].column;
            sNode.node.objectID = sRealNode[1].objectID;
        }

        // array������ column, value�� ���´�.
        sArrayInfo = *((qsxArrayInfo **)QTC_TMPL_FIXEDDATA( sTemplate, &sNode ));

        IDE_TEST_RAISE( sArrayInfo == NULL, ERR_INVALID_ARRAY );

        IDE_TEST( qsxArray::truncateArray( sArrayInfo ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               qsxArrayInfo�� �����Ѵ�.
 *
 * Implementation : 
 *   * aQcStmt->session->mQPSpecific->mArrMemMgr��
 *     qsxArrayInfo�� �����ϱ� ���� memory manager�̴�.
 *   * ������ qsxArrayInfo�� list�� ù ��°�� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC qsxUtil::allocArrayInfo( qcStatement   * aQcStmt,
                                qsxArrayInfo ** aArrayInfo )
{
    qcSessionSpecific * sQPSpecific = &(aQcStmt->session->mQPSpecific);
    qsxArrayInfo      * sArrayInfo  = NULL;

    IDE_DASSERT( aArrayInfo != NULL );

    IDU_FIT_POINT("qsxUtil::allocArrayInfo::malloc::iduMemory",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST( sQPSpecific->mArrMemMgr->mMemMgr.alloc( (void**)&sArrayInfo )
              != IDE_SUCCESS );

    sArrayInfo->qpSpecific     = sQPSpecific;
    sArrayInfo->avlTree.memory = NULL;

    sArrayInfo->next      = sQPSpecific->mArrInfo;
    sQPSpecific->mArrInfo = sArrayInfo;

    *aArrayInfo = sArrayInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sArrayInfo != NULL )
    {
        (void)sQPSpecific->mArrMemMgr->mMemMgr.memfree( sArrayInfo );
    }

    *aArrayInfo = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               aNodeSize�� �´� qsxArrayMemMgr�� �����ϰų�
 *               ã�Ƽ� qsxArrayInfo�� �����Ѵ�.
 *
 * Implementation : 
 *
 ***********************************************************************/
IDE_RC qsxUtil::allocArrayMemMgr( qcStatement  * aQcStmt,
                                  qsxArrayInfo * aArrayInfo )
{
    qcSessionSpecific * sSessionInfo = &(aQcStmt->session->mQPSpecific);
    qsxArrayMemMgr    * sCurrMemMgr = NULL;
    qsxArrayMemMgr    * sPrevMemMgr = NULL;
    qsxArrayMemMgr    * sNewMemMgr = NULL;

    SInt                sNodeSize = 0;
    idBool              sIsFound = ID_FALSE;
    UInt                sStage = 0;

    sNodeSize = aArrayInfo->avlTree.rowSize + ID_SIZEOF(qsxAvlNode);

    IDE_DASSERT( sSessionInfo->mArrMemMgr != NULL );

    sCurrMemMgr = sSessionInfo->mArrMemMgr->mNext;
    sPrevMemMgr = sSessionInfo->mArrMemMgr;

    while ( sCurrMemMgr != NULL )
    {
        if ( sCurrMemMgr->mNodeSize == sNodeSize )
        {
            sIsFound = ID_TRUE;
            break;
        }
        else if ( sCurrMemMgr->mNodeSize > sNodeSize )
        {
            sIsFound = ID_FALSE;
            break;
        }
        else
        {
            sPrevMemMgr = sCurrMemMgr;
            sCurrMemMgr = sCurrMemMgr->mNext;
        }
    }

    if ( sIsFound == ID_FALSE )
    {
        IDU_FIT_POINT("qsxUtil::allocArrayMemMgr::malloc::iduMemory",
                      idERR_ABORT_InsufficientMemory);
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_QSN,
                                     ID_SIZEOF(qsxArrayMemMgr),
                                     (void**)&sNewMemMgr )
                  != IDE_SUCCESS );
        sStage = 1;

        aArrayInfo->avlTree.memory = (iduMemListOld*)sNewMemMgr;

        sPrevMemMgr->mNext = sNewMemMgr;

        sNewMemMgr->mNodeSize = sNodeSize;
        sNewMemMgr->mRefCount = 1;
        sNewMemMgr->mNext     = sCurrMemMgr;

        sStage = 2;

        IDE_TEST( ((iduMemListOld*)sNewMemMgr)->initialize( IDU_MEM_QSN,
                                                            0,
                                                            (SChar*)"PSM_ARRAY_VARIABLE",
                                                            sNodeSize,
                                                            QSX_AVL_EXTEND_COUNT,
                                                            QSX_AVL_FREE_COUNT,
                                                            ID_FALSE )
                  != IDE_SUCCESS );
        sStage = 3;
    }
    else
    {
        aArrayInfo->avlTree.memory = (iduMemListOld*)sCurrMemMgr;
        sCurrMemMgr->mRefCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 3:
            {
                (void)((iduMemListOld*)sNewMemMgr)->destroy( ID_FALSE );
            }
            /* fall through */
        case 2:
            {
                if ( sIsFound == ID_FALSE )
                {
                    sPrevMemMgr->mNext = sCurrMemMgr;
                }
                else
                {
                    IDE_DASSERT(0);
                    // Nothing to do.
                }
            }
            /* fall through */
        case 1:
            {
                (void)iduMemMgr::free( sNewMemMgr );
            }
            break;
        default:
            {
                break;
            }
    }

    aArrayInfo->avlTree.memory = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               ����� �Ϸ��� qsxArrayInfo�� �����Ѵ�.
 *
 * Implementation : 
 *
 ***********************************************************************/
IDE_RC qsxUtil::freeArrayInfo( qsxArrayInfo * aArrayInfo )
{
    qcSessionSpecific * sQPSpecific    = aArrayInfo->qpSpecific;
    qsxArrayInfo      * sCurrArrayInfo = NULL;
    qsxArrayInfo      * sPrevArrayInfo = NULL;
    qsxArrayMemMgr    * sMemMgr        = NULL;

    sCurrArrayInfo = sQPSpecific->mArrInfo;

    while ( sCurrArrayInfo != NULL )
    {
        if ( sCurrArrayInfo == aArrayInfo )
        {
            if ( sPrevArrayInfo != NULL )
            {
                sPrevArrayInfo->next = sCurrArrayInfo->next;
            }
            else
            {
                sQPSpecific->mArrInfo = sCurrArrayInfo->next;
            }

            break;
        }
        else
        {
            // Nothing to do.
        }

        sPrevArrayInfo = sCurrArrayInfo;
        sCurrArrayInfo = sCurrArrayInfo->next;
    }

    // PROJ-1904 Extend UDT
    // �ݵ�� ã�ƾ� �Ѵ�.
    IDE_DASSERT ( sCurrArrayInfo != NULL );

    IDE_DASSERT( aArrayInfo->avlTree.refCount == 0 );

    sMemMgr = (qsxArrayMemMgr *)aArrayInfo->avlTree.memory;

    aArrayInfo->avlTree.memory = NULL;
    aArrayInfo->next = NULL;

    if ( sMemMgr != NULL )
    {
        sMemMgr->mRefCount--;
        IDE_DASSERT( sMemMgr->mRefCount >= 0 );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( sQPSpecific->mArrMemMgr->mMemMgr.memfree( aArrayInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               ����� �Ϸ��� qsxArrayMemMgr�� ��� �����Ѵ�.
 *               Session ���� ������ ȣ���Ѵ�.
 *
 * Implementation : 
 *   (1) loop�� ���鼭 session�� ������ memory manager�� ��� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC qsxUtil::destroyArrayMemMgr( qcSessionSpecific * aQPSpecific )
{
    qsxArrayMemMgr    * sCurrMemMgr = NULL;
    qsxArrayMemMgr    * sNextMemMgr = NULL;

    sCurrMemMgr = aQPSpecific->mArrMemMgr;

    while ( sCurrMemMgr != NULL )
    {
        sNextMemMgr = sCurrMemMgr->mNext;

        IDE_TEST( sCurrMemMgr->mMemMgr.destroy( ID_FALSE )
                  != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free( sCurrMemMgr )
                  != IDE_SUCCESS );

        sCurrMemMgr = sNextMemMgr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               Record type ������ �ʱ�ȭ �Ѵ�.
 *
 * Implementation : 
 *
 ***********************************************************************/
IDE_RC qsxUtil::initRecordVar( qcStatement * aQcStmt,
                               qcTemplate  * aTemplate,
                               qtcNode     * aNode,
                               idBool        aCopyRef )
{
    mtcColumn * sColumn    = NULL;
    qsTypes   * sType      = NULL;
    void      * sValueTemp = NULL;

    sColumn = QTC_TMPL_COLUMN( aTemplate, aNode );
    sType   = ((qtcModule*)sColumn->module)->typeInfo;

    sValueTemp = (void*)mtc::value( sColumn,
                                    QTC_TMPL_TUPLE( aTemplate, aNode )->row,
                                    MTD_OFFSET_USE );

    if ( (sType->flag & QTC_UD_TYPE_HAS_ARRAY_MASK) ==
         QTC_UD_TYPE_HAS_ARRAY_TRUE )
    {
        IDE_TEST( qsxUtil::initRecordVar( aQcStmt,
                                          sColumn,
                                          sValueTemp,
                                          aCopyRef )
                  != IDE_SUCCESS );
    }
    else
    {
        sColumn->module->null( sColumn, sValueTemp );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : PROJ-1094 Extend UDT
 *               Record type ������ �ʱ�ȭ �Ѵ�.
 *
 * Implementation : 
 *   * Record type ������ �� column�� type�� �°� �����Ѵ�.
 *     (1) AA type        : array �ʱ�ȭ
 *     (2) Record type    : qsxUtil::initRecordVar(���ȣ��)
 *     (3) primitive type : mtdSetNull
 *
 ***********************************************************************/
IDE_RC qsxUtil::initRecordVar( qcStatement     * aQcStmt,
                               const mtcColumn * aColumn,
                               void            * aRow,
                               idBool            aCopyRef )
{
    mtcColumn * sColumn = NULL;
    qcmColumn * sQcmColumn = NULL;
    qcmColumn * sTmpQcmColumn = NULL;
    void      * sRow = NULL;
    UInt        sSize = 0;

    IDE_DASSERT( aQcStmt != NULL );

    if ( aRow != NULL )
    {
        sQcmColumn = (qcmColumn*)(((qtcModule*)aColumn->module)->typeInfo->columns);

        while ( sQcmColumn != NULL )
        {
            sColumn = sQcmColumn->basicInfo;

            sSize = idlOS::align( sSize, sColumn->module->align ); 
            sRow = (void*)((UChar*)aRow + sSize);

            if ( sColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
            {
                *((qsxArrayInfo**)sRow) = NULL;

                if ( (aQcStmt->spvEnv->createProc == NULL) &&
                     (aQcStmt->spvEnv->createPkg  == NULL) &&
                     (aCopyRef == ID_FALSE) )
                {
                    sTmpQcmColumn = (qcmColumn*)(((qtcModule*)sColumn->module)->typeInfo->columns);

                    IDE_TEST( qsxArray::initArray( (qcStatement*)aQcStmt,
                                                   (qsxArrayInfo**)sRow,
                                                   sTmpQcmColumn,
                                                   aQcStmt->mStatistics )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else if ( (sColumn->type.dataTypeId == MTD_ROWTYPE_ID) ||
                      (sColumn->type.dataTypeId == MTD_RECORDTYPE_ID) )
            {
                IDE_TEST( qsxUtil::initRecordVar( aQcStmt,
                                                  sColumn,
                                                  sRow,
                                                  aCopyRef )
                          != IDE_SUCCESS );
            }
            else
            {
                sColumn->module->null( sColumn, sRow );
            }

            sSize += sColumn->column.size;

            sQcmColumn = sQcmColumn->next;
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
 * Description : PROJ-1094 Extend UDT
 *               Record type ������ ���� �Ѵ�.
 *
 * Implementation : 
 *
 ***********************************************************************/
IDE_RC qsxUtil::finalizeRecordVar( qcTemplate * aTemplate,
                                   qtcNode    * aNode )
{
    mtcColumn * sColumn = NULL;
    qsTypes   * sType = NULL;
    void      * sValueTemp = NULL;

    sColumn = QTC_TMPL_COLUMN( aTemplate, aNode );
    sType   = ((qtcModule*)sColumn->module)->typeInfo;

    if ( (sType->flag & QTC_UD_TYPE_HAS_ARRAY_MASK) ==
         QTC_UD_TYPE_HAS_ARRAY_TRUE )
    {
        sValueTemp = (void*)mtc::value( sColumn,
                                        QTC_TMPL_TUPLE( aTemplate, aNode )->row,
                                        MTD_OFFSET_USE );

        IDE_TEST( qsxUtil::finalizeRecordVar( sColumn,
                                              sValueTemp )
                  != IDE_SUCCESS );
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
 * Description : PROJ-1094 Extend UDT
 *               Record type ������ ���� �Ѵ�.
 *
 * Implementation : 
 *   * Record type ������ �� column�� type�� �°� �����Ѵ�.
 *     (1) AA type        : qsxArray::finalizeArray
 *     (2) Record type    : qsxUtil::finalizeRecordVar(���ȣ��)
 *     (3) primitive type : Nothing to do.
 *
 ***********************************************************************/
IDE_RC qsxUtil::finalizeRecordVar( const mtcColumn * aColumn,
                                   void            * aRow )
{
    mtcColumn * sColumn = NULL;
    qcmColumn * sQcmColumn = NULL;
    qsTypes   * sType = NULL;
    void      * sRow = aRow;
    UInt        sSize = 0;

    sType = ((qtcModule*)aColumn->module)->typeInfo;

    if ( (aRow != NULL) &&
         ((sType->flag & QTC_UD_TYPE_HAS_ARRAY_MASK) ==
          QTC_UD_TYPE_HAS_ARRAY_TRUE) )
    {
        sQcmColumn = (qcmColumn*)(((qtcModule*)aColumn->module)->typeInfo->columns);

        while ( sQcmColumn != NULL )
        {
            sColumn = sQcmColumn->basicInfo;

            sSize = idlOS::align( sSize, sColumn->module->align ); 
            sRow = (void*)((UChar*)aRow + sSize);

            if ( sColumn->type.dataTypeId == MTD_ASSOCIATIVE_ARRAY_ID )
            {
                IDE_TEST( qsxArray::finalizeArray( (qsxArrayInfo**)sRow )
                          != IDE_SUCCESS );
            }
            else if ( (sColumn->type.dataTypeId == MTD_ROWTYPE_ID) ||
                      (sColumn->type.dataTypeId == MTD_RECORDTYPE_ID) )
            {
                IDE_TEST( qsxUtil::finalizeRecordVar( sColumn,
                                                      sRow )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            sSize += sColumn->column.size;

            sQcmColumn = sQcmColumn->next;
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

IDE_RC qsxUtil::adjustParamAndReturnColumnInfo( iduMemory   * aQmxMem,
                                                mtcColumn   * aSourceColumn,
                                                mtcColumn   * aDestColumn,
                                                mtcTemplate * aDestTemplate )
{
/***********************************************************************************************
 * Description :
 *     PROJ-2586 PSM Parameters and return without precision
 *
 *     aDestColumn�� �Ʒ��� datatype�� �ش�� ��, mtcColumn ������ aSourceColumn�� ���� ����
 *         - CHAR( M )
 *         - VARCHAR( M )
 *         - NCHAR( M )
 *         - NVARCHAR( M )
 *         - BIT( M )
 *         - VARBIT( M )
 *         - BYTE( M )
 *         - VARBYTE( M )
 *         - NIBBLE( M )
 *         - FLOAT[ ( P ) ]
 *         - NUMBER[ ( P [ , S ] ) ]
 *         - NUMERIC[ ( P [ , S ] ) ]
 *         - DECIMAL[ ( P [ , S ] ) ]
 *             ��NUMERIC�� ����
 **********************************************************************************************/

    mtvConvert         * sConvert        = NULL;
    UInt                 sSourceArgCount = 0;
    iduMemoryStatus      sMemStatus;
    UInt                 sStage          = 0;

    IDE_DASSERT( aQmxMem != NULL );
    IDE_DASSERT( aSourceColumn != NULL );
    IDE_DASSERT( aDestColumn != NULL );
    IDE_DASSERT( aDestTemplate != NULL );

    // PSM������ ��ȣ �÷� Ÿ���� ����� �� ����.
    IDE_DASSERT( (aSourceColumn->type.dataTypeId != MTD_ECHAR_ID) &&
                 (aSourceColumn->type.dataTypeId != MTD_EVARCHAR_ID) );

    IDE_DASSERT( (aDestColumn->type.dataTypeId != MTD_ECHAR_ID) &&
                 (aDestColumn->type.dataTypeId != MTD_EVARCHAR_ID) );

    IDE_TEST_CONT( (aDestColumn->flag & MTC_COLUMN_SP_SET_PRECISION_MASK)
                   == MTC_COLUMN_SP_SET_PRECISION_FALSE,
                   SKIP_ADJUSTMENT );
    IDE_TEST_CONT( (aDestColumn->flag & MTC_COLUMN_SP_ADJUST_PRECISION_MASK)
                   == MTC_COLUMN_SP_ADJUST_PRECISION_TRUE,
                   SKIP_ADJUSTMENT );

    // aDestColumn�� datatype�� ���� ������.
    switch ( aDestColumn->type.dataTypeId )
    {
        case MTD_CHAR_ID :
        case MTD_VARCHAR_ID :
        case MTD_NCHAR_ID :
        case MTD_NVARCHAR_ID :
        case MTD_BIT_ID :
        case MTD_VARBIT_ID :
        case MTD_BYTE_ID :
        case MTD_VARBYTE_ID :
        case MTD_NIBBLE_ID :
        case MTD_FLOAT_ID :
        case MTD_NUMBER_ID :
        case MTD_NUMERIC_ID :
            break;
        default :
            IDE_CONT( SKIP_ADJUSTMENT );
            break;
    }

    IDE_TEST( aQmxMem->getStatus( &sMemStatus ) != IDE_SUCCESS );
    sStage = 1;

    // aSourceColumn�� datatype�� aDestColumn�� datatype�� �����ϸ� �ٷ� ����
    // �ƴϸ�, convertion�ؼ� ����
    if ( aSourceColumn->type.dataTypeId == aDestColumn->type.dataTypeId )
    {
        IDE_TEST_RAISE( aDestColumn->precision < aSourceColumn->precision, ERR_INVALID_LENGTH );

        // aSourceColumn�� precision, scale, size ������ aDestColumn�� �����Ѵ�.
        aDestColumn->precision   = aSourceColumn->precision;
        aDestColumn->scale       = aSourceColumn->scale;
        aDestColumn->column.size = aSourceColumn->column.size;
    }
    else
    {
        sSourceArgCount = aSourceColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;

        IDE_TEST( mtv::estimateConvert4Server( aQmxMem,
                                               &sConvert,
                                               aDestColumn->type,       //aDestType
                                               aSourceColumn->type,     //aSourceType
                                               sSourceArgCount,         //aSourceArgument
                                               aSourceColumn->precision,//aSourcePrecision
                                               aSourceColumn->scale,    //aSourceScale
                                               aDestTemplate )          //mtcTmplate for passing session dateFormat
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( aDestColumn->precision < sConvert->stack[0].column->precision, ERR_INVALID_LENGTH );

        // convertion�� aSourceColumn�� precision, scale, size ������ aDestColumn�� �����Ѵ�.
        aDestColumn->precision   = sConvert->stack[0].column->precision;
        aDestColumn->scale       = sConvert->stack[0].column->scale;
        aDestColumn->column.size = sConvert->stack[0].column->column.size;
    }

    // ���� �Ϸ��ߴٰ� ǥ���Ѵ�.
    aDestColumn->flag &= ~MTC_COLUMN_SP_ADJUST_PRECISION_MASK;
    aDestColumn->flag |= MTC_COLUMN_SP_ADJUST_PRECISION_TRUE;

    sStage = 0;
    IDE_TEST( aQmxMem->setStatus( &sMemStatus ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_ADJUSTMENT )

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    }
    IDE_EXCEPTION_END;

    if( sStage == 1 )
    {
        if ( aQmxMem->setStatus( &sMemStatus ) != IDE_SUCCESS )
        {
            IDE_ERRLOG(IDE_QP_1);
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

    return IDE_FAILURE;
}

IDE_RC qsxUtil::finalizeParamAndReturnColumnInfo( mtcColumn   * aColumn )
{
/***********************************************************************************************
 * Description :
 *     PROJ-2586 PSM Parameters and return without precision
 *
 *     parameter �� return�� mtcColumn ������ ���󺹱ͽ�Ų��.
 *         - ���� Procedure �� Function ���� ������Ű�� �ʾƵ� �ǳ�( �׳� ������Ų�� ),
 *         - Package�� ���, session ������ template�� �����Ǳ� ������ ����������� �Ѵ�.
 **********************************************************************************************/

    if ( (aColumn->flag & MTC_COLUMN_SP_ADJUST_PRECISION_MASK) == MTC_COLUMN_SP_ADJUST_PRECISION_TRUE )
    {
        aColumn->scale = 0;

        switch ( aColumn->module->id )
        {
            case MTD_CHAR_ID :
                aColumn->precision   = QCU_PSM_CHAR_DEFAULT_PRECISION; 
                aColumn->column.size = ID_SIZEOF(UShort) + aColumn->precision; 
                break;
            case MTD_VARCHAR_ID :
                aColumn->precision   = QCU_PSM_VARCHAR_DEFAULT_PRECISION; 
                aColumn->column.size = ID_SIZEOF(UShort) + aColumn->precision; 
                break;
            case MTD_NCHAR_ID :
                if ( mtl::mNationalCharSet->id == MTL_UTF16_ID )
                {
                    aColumn->precision = QCU_PSM_NCHAR_UTF16_DEFAULT_PRECISION; 
                    aColumn->column.size = ID_SIZEOF(UShort) + ( aColumn->precision * MTL_UTF16_PRECISION ); 
                }
                else /* UTF8 */
                {
                    /* ������ �ڵ�  
                       mtdEstimate���� UTF16 �Ǵ� UTF8�� �ƴϸ� ���� �߻���. */
                    IDE_DASSERT( mtl::mNationalCharSet->id == MTL_UTF8_ID );

                    aColumn->precision = QCU_PSM_NCHAR_UTF8_DEFAULT_PRECISION; 
                    aColumn->column.size = ID_SIZEOF(UShort) + ( aColumn->precision * MTL_UTF8_PRECISION ); 
                }
                break;
            case MTD_NVARCHAR_ID :
                if ( mtl::mNationalCharSet->id == MTL_UTF16_ID )
                {
                    aColumn->precision = QCU_PSM_NVARCHAR_UTF16_DEFAULT_PRECISION; 
                    aColumn->column.size = ID_SIZEOF(UShort) + ( aColumn->precision * MTL_UTF16_PRECISION ); 
                }
                else /* UTF8 */
                {
                    /* ������ �ڵ�  
                       mtdEstimate���� UTF16 �Ǵ� UTF8�� �ƴϸ� ���� �߻���. */
                    IDE_DASSERT( mtl::mNationalCharSet->id == MTL_UTF8_ID );

                    aColumn->precision = QCU_PSM_NVARCHAR_UTF8_DEFAULT_PRECISION; 
                    aColumn->column.size = ID_SIZEOF(UShort) + ( aColumn->precision * MTL_UTF8_PRECISION ); 
                }
                break;
            case MTD_BIT_ID :
                aColumn->precision = QS_BIT_PRECISION_DEFAULT; 
                aColumn->column.size = ID_SIZEOF(UInt) + ( BIT_TO_BYTE( aColumn->precision ) );
                break;
            case MTD_VARBIT_ID :
                aColumn->precision = QS_VARBIT_PRECISION_DEFAULT; 
                aColumn->column.size = ID_SIZEOF(UInt) + ( BIT_TO_BYTE( aColumn->precision ) );
                break;
            case MTD_BYTE_ID :
                aColumn->precision = QS_BYTE_PRECISION_DEFAULT; 
                aColumn->column.size = ID_SIZEOF(UShort) + aColumn->precision; 
                break;
            case MTD_VARBYTE_ID :
                aColumn->precision = QS_VARBYTE_PRECISION_DEFAULT; 
                aColumn->column.size = ID_SIZEOF(UShort) + aColumn->precision; 
                break;
            case MTD_NIBBLE_ID :
                aColumn->precision = MTD_NIBBLE_PRECISION_MAXIMUM; 
                aColumn->column.size = ID_SIZEOF(UChar) + ( ( aColumn->precision + 1 ) >> 1);
                break;
            case MTD_FLOAT_ID :
                aColumn->precision = MTD_FLOAT_PRECISION_MAXIMUM; 
                aColumn->column.size = MTD_FLOAT_SIZE( aColumn->precision );
                break;
            case MTD_NUMBER_ID :
            case MTD_NUMERIC_ID :
                // DECIMAL type�� NUMERIC type�� ����
                aColumn->precision = MTD_NUMERIC_PRECISION_MAXIMUM; 
                aColumn->column.size = MTD_NUMERIC_SIZE( aColumn->precision, aColumn->scale);
                break;
            default :
                IDE_DASSERT( 0 );
        }

        aColumn->flag &= ~MTC_COLUMN_SP_ADJUST_PRECISION_MASK;
        aColumn->flag |= MTC_COLUMN_SP_ADJUST_PRECISION_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC 
qsxUtil::preCalculateArray ( qcStatement * aQcStmt,
                             qtcNode     * aQtcNode )
{
    qtcNode         * sQtcNode;
    void            * sInfo;
    qcTemplate      * sTemplate;
    qtcColumnInfo   * sColumnInfo;
    mtcColumn       * sDestColumn = NULL;

    sTemplate = QC_PRIVATE_TMPLATE(aQcStmt);

    for( sQtcNode = aQtcNode;
         sQtcNode != NULL;
         sQtcNode = (qtcNode*)sQtcNode->node.next )
    {
        if ( (sQtcNode->lflag & QTC_NODE_SP_NESTED_ARRAY_MASK)
             == QTC_NODE_SP_NESTED_ARRAY_TRUE)
        {
            IDE_DASSERT( sQtcNode->node.orgNode != NULL );

            //Right
            if( qtc::calculate( (qtcNode *)(sQtcNode->node.orgNode),
                                sTemplate )
                != IDE_SUCCESS)
            {
                IDE_RAISE( ERR_INVALID_ARRAY );
            }

            IDE_TEST_RAISE( *((qsxArrayInfo **)(sTemplate->tmplate.stack[0].value)) == NULL, ERR_INVALID_ARRAY );

            // left
            sInfo = QTC_TMPL_EXECUTE(QC_PRIVATE_TMPLATE(aQcStmt),
                                     sQtcNode)->calculateInfo;
            sColumnInfo = (qtcColumnInfo*)sInfo;
            sDestColumn = sTemplate->tmplate.rows[sColumnInfo->table].columns + sColumnInfo->column;

            IDE_TEST( assignValue( QC_QMX_MEM(aQcStmt),
                                   sTemplate->tmplate.stack[0].column,
                                   sTemplate->tmplate.stack[0].value,
                                   sDestColumn,
                                   (void*)
                                   ( (SChar*)sTemplate->tmplate.rows[sColumnInfo->table].row +
                                     sDestColumn->column.offset ),
                                   & sTemplate->tmplate,
                                   ID_TRUE )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
