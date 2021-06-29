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
 * $Id: qsfRaiseAppErr.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *     PROJ-1335 PSM ���� (RAISE_APPLICATION_ERROR)
 *     ����� ���� ������ �߻���Ű�� �Լ�.
 *
 * Syntax :
 *     RAISE_APP_ERR( errCode INTEGER, errMsg VARCHAR(2047) );
 *     RETURN BOOLEAN
 *
 **********************************************************************/

#include <qsf.h>
#include <qc.h>

extern mtdModule mtdVarchar;
extern mtdModule mtdInteger;
extern mtdModule mtdBoolean;

static mtcName qsfFunctionName[1] = {
    { NULL, 13, (void*)"RAISE_APP_ERR" }
};

static IDE_RC qsfEstimate( mtcNode*     aNode,
                           mtcTemplate* aTemplate,
                           mtcStack*    aStack,
                           SInt         aRemain,
                           mtcCallBack* aCallBack );

mtfModule qsfRaiseAppErrModule = {
    1|MTC_NODE_OPERATOR_MISC|MTC_NODE_VARIABLE_TRUE,
    ~0,
    1.0,                    // default selectivity (�� ������ �ƴ�)
    qsfFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    qsfEstimate
};


IDE_RC qsfCalculate_RaiseAppErr( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate );

static const mtcExecute qsfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    qsfCalculate_RaiseAppErr,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};

IDE_RC qsfEstimate( mtcNode*     aNode,
                    mtcTemplate* aTemplate,
                    mtcStack*    aStack,
                    SInt      /* aRemain */,
                    mtcCallBack* aCallBack )
{
#define IDE_FN "IDE_RC qsfEstimate"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    const mtdModule* sModules[2] =
    {
        &mtdInteger,
        &mtdVarchar
    };

    const mtdModule* sModule = &mtdBoolean;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) ==
                    MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 2,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = qsfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_NOT_AGGREGATION));

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qsfCalculate_RaiseAppErr( mtcNode*     aNode,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 void*        aInfo,
                                 mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *     qsfCalculate_RaiseAppErr
 *
 * Implementation :
 *     1. �����ڵ� ���� �˻�. ���������� ����
 *     2. �����޽��� ���� �˻�. ���̰� 2047�� ������ ����
 *     3. ����� ������ �����Ͽ� ������ ������.
 *
 ***********************************************************************/
    
#define IDE_FN "IDE_RC qsfCalculate_RaiseAppErr"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qcStatement    * sStatement;
    mtdCharType    * sErrorMsg;
    mtdIntegerType   sErrorCode;
    mtdBooleanType * sReturnValue;
    UInt             sServerErrorCode;
    SChar*           sServerErrorMsg;
    
    sStatement    = ((qcTemplate*)aTemplate)->stmt;
    
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );
    
    if( (aStack[1].column->module->isNull( aStack[1].column,
                                           aStack[1].value ) == ID_TRUE) ||
        (aStack[2].column->module->isNull( aStack[2].column,
                                           aStack[2].value ) == ID_TRUE) )
    {
        // Value Error
        IDE_RAISE(ERR_ARGUMENT_NOT_APPLICABLE);
    }
    else
    {
        // Nothing to do
    }

    sReturnValue = (mtdBooleanType*)aStack[0].value;
    sErrorCode = *(mtdIntegerType*)aStack[1].value;
    sErrorMsg = (mtdCharType*)aStack[2].value;
    *sReturnValue = MTD_BOOLEAN_TRUE;

    // �����ڵ��� ������ ����� ������ ������ �˻�.
    IDE_TEST_RAISE( E_CHECK_UDE(sErrorCode) == ID_FALSE,
                    ERR_ERRORCODE_INVALID_RANGE );
    
    
    IDE_TEST_RAISE( sErrorMsg->length > MAX_ERROR_MSG_LEN,
                    ERR_INVALID_LENGTH );

    IDU_FIT_POINT( "qsfRaiseAppErr::qsfCalculate_RaiseAppErr::alloc::ServerErrorMsg" );
    IDE_TEST( sStatement->qmxMem->alloc(
                              sErrorMsg->length + 1,
                              (void**)&sServerErrorMsg )
                  != IDE_SUCCESS );

    sServerErrorCode = sErrorCode;

    idlOS::strncpy( sServerErrorMsg,
                    (SChar*)sErrorMsg->value,
                    sErrorMsg->length );
    *( sServerErrorMsg + sErrorMsg->length ) = '\0';
        
    IDE_RAISE( ERR_USER_DEFINED );
    
    //  ����� ���� �����޽����� raise. ������ failure��.
    IDE_DASSERT(0);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ERRORCODE_INVALID_RANGE );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSF_INVALID_ERROR_RANGE,
                                sErrorCode));
    }
    IDE_EXCEPTION( ERR_INVALID_LENGTH );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_LENGTH));
    }
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_USER_DEFINED );
    {
        IDE_SET(ideSetErrorCodeAndMsg( E_SERVER_ERROR(sServerErrorCode),
                                       sServerErrorMsg ) );
    }
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

 
