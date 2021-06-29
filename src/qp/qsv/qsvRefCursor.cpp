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
 * $Id: qsvRefCursor.cpp 22178 2007-06-15 01:26:33Z orc $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuSqlSourceInfo.h>
#include <qsParseTree.h>
#include <qsvRefCursor.h>
#include <qsv.h>
#include <qsvProcVar.h>
#include <qsvProcStmts.h>
#include <qcuSessionPkg.h>
#include <qmv.h>

extern mtdModule    mtdVarchar;
extern mtdModule    mtdChar;

IDE_RC
qsvRefCursor::validateOpenFor( qcStatement       * aStatement,
                               qsProcStmtOpenFor * aProcStmtOpenFor )
{
/***********************************************************************
 *
 *  Description : PROJ-1386 Dynamic SQL
 *               
 *  Implementation :
 *            (1) query string�� char�� varchar���� �˻�
 *            (2) USING���� ���� validation
 *
 ***********************************************************************/

    mtcColumn         * sMtcColumn;
    qcuSqlSourceInfo    sqlInfo;    

    // fix BUG-37495
    if( aProcStmtOpenFor->sqlStringNode->node.module == &qtc::subqueryModule )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &aProcStmtOpenFor->sqlStringNode->position );
        IDE_RAISE(ERR_INSUFFICIENT_ARGUEMNT);
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qtc::estimate( aProcStmtOpenFor->sqlStringNode,
                             QC_SHARED_TMPLATE(aStatement),
                             aStatement,
                             NULL,
                             NULL,
                             NULL )
              != IDE_SUCCESS );

    sMtcColumn = &(QC_SHARED_TMPLATE(aStatement)->tmplate.
                   rows[aProcStmtOpenFor->sqlStringNode->node.table].
                   columns[aProcStmtOpenFor->sqlStringNode->node.column]);

    if( ( sMtcColumn->module != &mtdVarchar ) &&
        ( sMtcColumn->module != &mtdChar ) )
    {
        sqlInfo.setSourceInfo( aStatement,
                               &aProcStmtOpenFor->sqlStringNode->position );
        IDE_RAISE(ERR_INSUFFICIENT_ARGUEMNT);
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( validateUsingParamClause(
                  aStatement,
                  aProcStmtOpenFor->usingParams,
                  &aProcStmtOpenFor->usingParamCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INSUFFICIENT_ARGUEMNT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode( qpERR_ABORT_QSV_INSUFFICIENT_ARGUEMNT_SQLTEXT,
                             sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qsvRefCursor::validateUsingParamClause(
    qcStatement  * aStatement,
    qsUsingParam * aUsingParams,
    UInt         * aUsingParamCount )
{
/***********************************************************************
 *
 *  Description : PROJ-1385 Dynamic-SQL
 *                USING ... ���� ���� validation
 *  Implementation : parameter�� inouttype���� ����
 *                  OUT, IN OUT �� ���
 *                  (1)�ݵ�� procedure variable�̾�� ��
 *                  (2) read�� ������ variable�� ��� ������
 *
 ***********************************************************************/
    
    idBool              sFindVar;
    qsVariables       * sArrayVariable;
    qcuSqlSourceInfo    sqlInfo;
    qsUsingParam      * sCurrParam;
    qtcNode           * sCurrParamNode;

    *aUsingParamCount = 0;
    
    if( aUsingParams != NULL )
    {
        for( sCurrParam = aUsingParams;
             sCurrParam != NULL;
             sCurrParam = sCurrParam->next )
        {
            sCurrParamNode = sCurrParam->paramNode;

            if( sCurrParam->inOutType == QS_IN )
            {
                // inŸ���� ��� Ư���� �˻縦 ���� ����.
                IDE_TEST( qtc::estimate( sCurrParamNode,
                                         QC_SHARED_TMPLATE(aStatement), 
                                         aStatement,
                                         NULL,
                                         NULL,
                                         NULL )
                      != IDE_SUCCESS );
            }
            else
            {
                // (1) out, in out�� ��� �ݵ�� procedure�������� ��.
                // invalid out argument ������ ��.
                IDE_TEST(qsvProcVar::searchVarAndPara(
                         aStatement,
                         sCurrParamNode,
                         ID_TRUE, // for OUTPUT
                         &sFindVar,
                         &sArrayVariable)
                     != IDE_SUCCESS);

                if( sFindVar == ID_FALSE )
                {
                    IDE_TEST( qsvProcVar::searchVariableFromPkg(
                            aStatement,
                            sCurrParamNode,
                            &sFindVar,
                            &sArrayVariable )
                        != IDE_SUCCESS );
                }

                if (sFindVar == ID_FALSE)
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sCurrParamNode->position );
                    IDE_RAISE(ERR_INVALID_OUT_ARGUMENT);
                }
                else
                {
                    // Nothing to do.
                }

                // lvalue�� psm������ �����ϹǷ� lvalue flag�� ����.
                // out�� ��쿡�� �ش��. array_index_variable�� ���
                // ������ ������ �ϱ� ����.
                if( sCurrParam->inOutType == QS_OUT )
                {
                    sCurrParamNode->lflag |= QTC_NODE_LVALUE_ENABLE;
                }
                else
                {
                    // Nothing to do.
                }

                // (2) out, in out�� ��� outbinding_disable�̸� ����
                IDE_TEST( qtc::estimate( sCurrParamNode,
                                         QC_SHARED_TMPLATE(aStatement), 
                                         aStatement,
                                         NULL,
                                         NULL,
                                         NULL )
                          != IDE_SUCCESS );
            
                if ( ( sCurrParamNode->lflag & QTC_NODE_OUTBINDING_MASK )
                     == QTC_NODE_OUTBINDING_DISABLE )
                {
                    sqlInfo.setSourceInfo(
                        aStatement,
                        & sCurrParamNode->position );
                    IDE_RAISE(ERR_INVALID_OUT_ARGUMENT);
                }
                else
                {
                    // Nothing to do.
                }
            }

            (*aUsingParamCount)++;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_OUT_ARGUMENT);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_INVALID_OUT_ARGUEMNT_SQLTEXT,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qsvRefCursor::searchRefCursorVar(
    qcStatement  * aStatement,
    qtcNode      * aVariableNode,
    qsVariables ** aVariables,
    idBool       * aFindVar )
{
/***********************************************************************
 *
 *  Description : PROJ-1386 Dynamic-SQL
 *
 *  Implementation : (1) variable search
 *                   (2) variable ã������ ref cursor type���� �˻�
 *                   (3) variable�� ��ã�Ұų�, ref cursor type�� �ƴϸ�
 *                       not found
 *
 ***********************************************************************/
    
    mtcColumn         * sMtcColumn;
    // PROJ-1073 Package
    qcTemplate        * sTemplate;
    qsxPkgInfo        * sPkgInfo;
    
    IDE_TEST( qsvProcVar::searchVarAndPara(
                  aStatement,
                  aVariableNode,
                  ID_FALSE,
                  aFindVar,
                  aVariables )
              != IDE_SUCCESS );

    if( *aFindVar == ID_FALSE )
    {
        IDE_TEST( qsvProcVar::searchVariableFromPkg(
                      aStatement,
                      aVariableNode,
                      aFindVar,
                      aVariables )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nohting to do.
    }

    if( *aFindVar == ID_TRUE )
    {
        if( aVariableNode->node.objectID != 0 )
        {
            IDE_TEST( qsxPkg::getPkgInfo( aVariableNode->node.objectID,
                                          &sPkgInfo )
                      != IDE_SUCCESS );

            // objectID�� template�� �����´�.
            sTemplate = sPkgInfo->tmplate;
        }
        else
        {
            sTemplate = QC_SHARED_TMPLATE(aStatement);
        }

        sMtcColumn = &(sTemplate->tmplate.
                       rows[aVariableNode->node.table].
                       columns[aVariableNode->node.column]);

        // �ݵ�� ref cursor type module�̾�� ��.
        if( sMtcColumn->module->id != MTD_REF_CURSOR_ID )
        {
            *aFindVar = ID_FALSE;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qsvRefCursor::validateFetch( qcStatement     * aStatement,
                             qsProcStmts     * aProcStmts,
                             qsProcStmts     * /*aParentStmt*/,
                             qsValidatePurpose /*aPurpose*/ )
{
/***********************************************************************
 *
 *  Description : PROJ-1386 Dynamic-SQL
 *                BUG-41707 chunking bulk collections of reference cursor    
 *
 *  Implementation :
 *            (1) into���� ���� validation
 *            (2) limit���� ���� validation
 *
 ***********************************************************************/

    qsProcStmtFetch * sProcStmtFetch = (qsProcStmtFetch*)aProcStmts;

    /* into���� ���� validation */
    IDE_TEST( qsvProcStmts::validateIntoClauseForRefCursor( 
                   aStatement,
                   aProcStmts,
                   sProcStmtFetch->intoVariableNodes )
              != IDE_SUCCESS );

    /* limit���� ���� validation */
    // limit���� bulk collect into�� ���� ��� �����ϴ�.
    if ( ( sProcStmtFetch->intoVariableNodes->bulkCollect == ID_TRUE ) && 
         ( sProcStmtFetch->limit != NULL ) )
    {
        IDE_TEST( qmv::validateLimit( aStatement,
                                      sProcStmtFetch->limit )
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
