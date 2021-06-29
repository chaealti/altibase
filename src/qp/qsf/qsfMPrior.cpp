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
 * $Id: qsfMPrior.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *     PROJ-1075 array type������ member function FIRST
 *
 * Syntax :
 *     arr_var.PRIOR( index );
 *     RETURN INTEGER/VARCHAR  <= �ش� index�� ���� index�� ��ȯ.
 *                                index�� INTEGER/VARCHAR ���� �ϳ���
 *
 * Implementation :
 *     1. �ش� index�� element�� ���ٸ� NULL.
 *     2. �ش� index���� ���� ù��° index�� ��ȯ�Ѵ�.
 *
 **********************************************************************/

#include <qsf.h>
#include <qc.h>
#include <qsvEnv.h>
#include <qcuSqlSourceInfo.h>
#include <qsParseTree.h>
#include <qsvProcVar.h>
#include <qsxArray.h>
#include <qcuSessionPkg.h>

static mtcName qsfMPriorFunctionName[1] = {
    { NULL, 5, (void*)"PRIOR" }
};


static IDE_RC qsfMPriorEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 mtcCallBack* aCallBack );

IDE_RC qsfMPriorCalculate(mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate );

mtfModule qsfMPriorModule = {
    1|MTC_NODE_OPERATOR_FUNCTION|MTC_NODE_VARIABLE_TRUE,
    ~(MTC_NODE_INDEX_MASK),
    1.0,  // default selectivity (�� �����ڰ� �ƴ�)
    qsfMPriorFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    qsfMPriorEstimate
};

const mtcExecute qsfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    qsfMPriorCalculate,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC qsfMPriorEstimate( mtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt      /* aRemain */,
                          mtcCallBack* aCallBack )
{
/***********************************************************************
 *
 * Description : PROJ-1075 prior�Լ��� estimate
 *
 * Implementation :
 *            �⺻���� routine�� �Ϲ� qsf~�Լ���� ������,
 *            host variable binding�� ������� �ʰ�
 *            psm���ο����� ����� �����ϴ�.
 *
 *            ������ ���� �������� ���� �� �ִ�.
 *            (1) var_name.prior(index)
 *            (2) label_name.var_name.prior(index)
 *            var_name�� qtcNode�� tableName�� �ش�ǹǷ� ������ �����ؾ� �Ѵ�.
 *            qtcNode->userName, tableName�� �̿��Ͽ� array type variable�� �˻�.
 *            execute->calculateInfo�� ã�� ������ ������ �����Ͽ� �ش�.
 *            index, return value�� ������ �ش� ������ key column type�� �����ϰ� ����.
 *
 ***********************************************************************/

    qcStatement     * sStatement;
    qtcNode         * sNode;
    qsVariables     * sArrayVariable;
    idBool            sIsFound;
    qcuSqlSourceInfo  sqlInfo;
    qtcColumnInfo   * sColumnInfo;
    mtcColumn       * sKeyColumn;
    UInt              sArgumentCount;
    const mtdModule * sModule;

    sStatement      = ((qcTemplate*)aTemplate)->stmt;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    sNode            = (qtcNode*)aNode;

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    // ���ռ� �˻�. tableName�� �ݵ�� �����ؾ� ��.
    IDE_DASSERT( QC_IS_NULL_NAME(sNode->tableName) == ID_FALSE );

    // array type ������ �˻�.
    IDE_TEST( qsvProcVar::searchArrayVar( sStatement,
                                          sNode,
                                          &sIsFound,
                                          &sArrayVariable )
              != IDE_SUCCESS );

    if( sIsFound == ID_FALSE )
    {
        sqlInfo.setSourceInfo( sStatement,
                               &sNode->tableName );
        IDE_RAISE(ERR_NOT_FOUND_VAR);
    }
    else
    {
        aTemplate->rows[aNode->table].execute[aNode->column] = qsfExecute;

        // ������ table, column������ execute->calculateInfo�� �����Ѵ�.
        IDU_FIT_POINT( "qsfMPrior::qsfMPriorEstimate::alloc::ColumnInfo" );
        IDE_TEST( aCallBack->alloc( aCallBack->info,
                                    ID_SIZEOF( qtcColumnInfo ),
                                    (void**)&sColumnInfo )
                  != IDE_SUCCESS );

        sColumnInfo->table    = sArrayVariable->common.table;
        sColumnInfo->column   = sArrayVariable->common.column;
        sColumnInfo->objectId = sArrayVariable->common.objectID;

        aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo = (void*)sColumnInfo;

        // typeInfo�� ù��° �÷��� index column��.
        sModule = sArrayVariable->typeInfo->columns->basicInfo->module;

        IDE_TEST( mtf::makeConversionNodes( aNode,
                                            aNode->arguments,
                                            aTemplate,
                                            aStack + 1,
                                            aCallBack,
                                            &sModule )
                  != IDE_SUCCESS );

        sKeyColumn = sArrayVariable->typeInfo->columns->basicInfo;

        sArgumentCount = sKeyColumn->flag & MTC_COLUMN_ARGUMENT_COUNT_MASK;

        // return���� keyColumn�� ����.
        IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                         sKeyColumn->module,
                                         sArgumentCount,
                                         sKeyColumn->precision,
                                         sKeyColumn->scale )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION(ERR_NOT_FOUND_VAR);
    {
        (void)sqlInfo.init(sStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_NOT_EXIST_VARIABLE_NAME_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qsfMPriorCalculate(mtcNode*     aNode,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          void*        aInfo,
                          mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1075 prior�Լ��� calculate
 *
 * Implementation :
 *          aInfo���� qsxArrayInfo�� �����ͼ�
 *          searchPrior�Լ��� ȣ���Ѵ�.
 *          ��, prior�� ���̻� ���� ��� �� index�� null�� ��쵵
 *          ������ ���� �ʰ� ��å�� NULL.
 *
 ***********************************************************************/    

    qsxArrayInfo   * sArrayInfo;
    mtcColumn      * sArrayColumn;
    qtcColumnInfo  * sColumnInfo;
    void           * sPriorKey;
    idBool           sFound;
    UInt             sActualSize;
    mtcTemplate    * sTemplateForArrayVar;

    sColumnInfo = (qtcColumnInfo*)aInfo;

    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    /* BUG-38243
       array method ��� ��, �ش� array�� �ش� aTemplate�� �ƴ�
       �ٸ� template�� ������ ���� �� �ִ�. */
    if ( sColumnInfo->objectId == QS_EMPTY_OID )
    {
        sTemplateForArrayVar = aTemplate;
    }
    else
    {
        IDE_TEST( qcuSessionPkg::getTmplate( ((qcTemplate *)aTemplate)->stmt,
                                             sColumnInfo->objectId,
                                             aStack,
                                             aRemain,
                                             &sTemplateForArrayVar )
                  != IDE_SUCCESS );

        IDE_DASSERT( sTemplateForArrayVar != NULL );
    }

    sArrayColumn = sTemplateForArrayVar->rows[sColumnInfo->table].columns + sColumnInfo->column;

    // PROJ-1904 Extend UDT
    sArrayInfo = *((qsxArrayInfo ** )( (UChar*) sTemplateForArrayVar->rows[sColumnInfo->table].row
                                    + sArrayColumn->column.offset ));

    // ���ռ� �˻�.
    IDE_TEST_RAISE( sArrayInfo == NULL, ERR_INVALID_ARRAY );

    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        // index�� null�� ��쵵 error���� �ʰ� NULL
        aStack[0].column->module->null( aStack[0].column,
                                        aStack[0].value );
    }
    else
    {
        IDE_TEST( qsxArray::searchPrior( sArrayInfo,
                                         aStack[1].column,
                                         aStack[1].value,
                                         &sPriorKey,
                                         &sFound )
                  != IDE_SUCCESS );

        if( sFound == ID_TRUE )
        {
            sActualSize = aStack[0].column->module->actualSize(
                aStack[0].column,
                sPriorKey );
            // module, precision, scale��� estimate�� �����ϰ� ����������Ƿ�
            // actualSize�� ���� �� aStack[0].column->module�� ����Ѵ�.
            idlOS::memcpy( aStack[0].value,
                           sPriorKey,
                           sActualSize );
        }
        else
        {
            aStack[0].column->module->null( aStack[0].column,
                                            aStack[0].value );
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
