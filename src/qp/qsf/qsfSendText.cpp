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
 * $Id: qsfSendText.cpp 85090 2019-03-28 01:15:28Z andrew.shin $
 **********************************************************************/

#include <idl.h>
#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <qsf.h>
#include <mtk.h>
#include <mtv.h>
#include <mtdTypes.h>
#include <qtc.h>
#include <qsxUtil.h>
#include <qcuSessionObj.h>

extern mtdModule mtsConnect;
extern mtdModule mtdVarchar;
extern mtdModule mtdInteger;

static mtcName qsfFunctionName[1] = {
    { NULL, 9, (void*)"SEND_TEXT" }
};

static IDE_RC qsfEstimate( mtcNode     * aNode,
                           mtcTemplate * aTemplate,
                           mtcStack    * aStack,
                           SInt          aRemain,
                           mtcCallBack * aCallBack );

mtfModule qsfSendTextModule = {
    1 | MTC_NODE_OPERATOR_MISC | MTC_NODE_VARIABLE_TRUE,
    ~0,
    1.0, /* default selectivity (비교 연산자 아님) */
    qsfFunctionName,
    NULL,
    mtf::initializeDefault,
    mtf::finalizeDefault,
    qsfEstimate
};


IDE_RC qsfCalculate_SendText( mtcNode     * aNode,
                              mtcStack    * aStack,
                              SInt          aRemain,
                              void        * aInfo,
                              mtcTemplate * aTemplate );

static const mtcExecute qsfExecute = {
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    mtf::calculateNA,
    qsfCalculate_SendText,
    NULL,
    mtx::calculateNA,
    mtk::estimateRangeNA,
    mtk::extractRangeNA
};


IDE_RC qsfEstimate( mtcNode     * aNode,
                    mtcTemplate * aTemplate,
                    mtcStack    * aStack,
                    SInt       /* aRemain */,
                    mtcCallBack * aCallBack )
{
    const mtdModule* sModules[3] =
    {
        &mtsConnect,
        &mtdVarchar,
        &mtdInteger
    };

    const mtdModule* sModule = & mtdInteger;

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_QUANTIFIER_MASK ) == MTC_NODE_QUANTIFIER_TRUE,
                    ERR_NOT_AGGREGATION );

    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 3,
                    ERR_INVALID_FUNCTION_ARGUMENT );

    IDE_TEST( mtf::makeConversionNodes( aNode,
                                        aNode->arguments,
                                        aTemplate,
                                        aStack + 1,
                                        aCallBack,
                                        sModules )
              != IDE_SUCCESS );

    aStack[0].column = aTemplate->rows[aNode->table].columns + aNode->column;

    /* Return값은 Integer */
    IDE_TEST( mtc::initializeColumn( aStack[0].column,
                                     sModule,
                                     0,
                                     0,
                                     0 )
              != IDE_SUCCESS );

    aTemplate->rows[aNode->table].execute[aNode->column] = qsfExecute;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_AGGREGATION );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_NOT_AGGREGATION ) );
    }
    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_FUNCTION_ARGUMENT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qsfCalculate_SendText( mtcNode     * aNode,
                              mtcStack    * aStack,
                              SInt          aRemain,
                              void        * aInfo,
                              mtcTemplate * aTemplate )
{
    qcStatement     * sStatement;
    qcSession       * sSession;
    mtsConnectType  * sConnectType;
    mtdCharType     * sMessage;
    mtdIntegerType    sMessageLength;
    mtdIntegerType  * sReturnValue;
    SInt              sMsgLen;
    PDL_SOCKET        sSocket;
    
    sStatement = ((qcTemplate*)aTemplate)->stmt;
    
    sSession   = sStatement->spxEnv->mSession;

    IDU_FIT_POINT( "qsfSendText::qsfCalculate_SendText::coverage::1" );
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if ( ( aStack[1].column->module->isNull( aStack[1].column,
                                             aStack[1].value ) == ID_TRUE ) ||
         ( aStack[2].column->module->isNull( aStack[2].column,
                                             aStack[2].value ) == ID_TRUE ) ||
         ( aStack[3].column->module->isNull( aStack[3].column,
                                             aStack[3].value ) == ID_TRUE ) )
    {
        /* error. value argument is invalid. */
        IDE_RAISE( ERR_ARGUMENT_NOT_APPLICABLE );
    }
    else
    {
        /* return value */
        sReturnValue  = (mtdIntegerType *)aStack[0].value;
        *sReturnValue = -1;

        /* connect type */
        sConnectType = (mtsConnectType *)aStack[1].value;

        /* get socket */
        IDU_FIT_POINT( "qsfSendText::qsfCalculate_SendText::coverage::2" );
        IDE_TEST( qcuSessionObj::getConnectionSocket( (qcSessionObjInfo *)( sSession->mQPSpecific.mSessionObj ),
                                                      sConnectType->connectionNodeKey,
                                                      & sSocket )
                  != IDE_SUCCESS );

        if ( sSocket == PDL_INVALID_SOCKET )
        {
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }

        /* message varchar */
        sMessage = (mtdCharType *)aStack[2].value;
        
        /* message length */
        sMessageLength = *(mtdIntegerType *)aStack[3].value;

        IDU_FIT_POINT_RAISE( "qsfSendText::qsfCalculate_SendText::coverage::3", ERR_MSGLEN );
        IDE_TEST_RAISE( ( ( sMessageLength > MTD_VARCHAR_PRECISION_MAXIMUM ) ||
                          ( sMessageLength < 0 ) ),
                        ERR_MSGLEN );

        IDU_FIT_POINT_RAISE( "qsfSendText::qsfCalculate_SendText::coverage::4", ERR_MSGLEN );
        IDE_TEST_RAISE( sMessage->length < sMessageLength , ERR_MSGLEN );

        sMsgLen = idlVA::send_i( sSocket,
                                 (SChar *)sMessage->value,
                                 sMessage->length );

        /* for coverage and fit test, remove if statement */
        IDU_FIT_POINT_RAISE( "qsfSendText::qsfCalculate_SendText::coverage::5", NORMAL_PROCESS );
        IDE_TEST_RAISE( sMsgLen >= 0, NORMAL_PROCESS );

        qcuSessionObj::setConnectionState( (qcSessionObjInfo*)(sSession->mQPSpecific.mSessionObj),
                                           sConnectType->connectionNodeKey,
                                               QC_CONNECTION_STATE_NOCONNECT );

        IDE_EXCEPTION_CONT( NORMAL_PROCESS );

        *sReturnValue = sMsgLen;
    }
    
    IDE_EXCEPTION_CONT( NORMAL_EXIT );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_MSGLEN );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_VALIDATE_INVALID_LENGTH ) );
    }
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_ARGUMENT_NOT_APPLICABLE ) );
    }
    IDE_EXCEPTION_END;
   
    return IDE_FAILURE;
}

