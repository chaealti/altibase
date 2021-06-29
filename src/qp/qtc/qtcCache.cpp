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
 * $Id$
 *
 * Description :
 *
 *      PROJ-2452 Result caching of Statement level
 *      PROJ-2448 Subquery caching
 *
 * ��� ���� :
 *
 **********************************************************************/

#include <idl.h>
#include <iduMemory.h>
#include <mtc.h>
#include <qc.h>
#include <qmgDef.h>
#include <qsParseTree.h>
#include <qsxProc.h>
#include <qsvEnv.h>
#include <qsvProcStmts.h>
#include <qcuProperty.h>
#include <qtcHash.h>
#include <qtcCache.h>
#include <qmgProjection.h>
#include <smi.h>

IDE_RC qtcCache::setIsCachableForFunction( qsVariableItems  * aParaDecls,
                                           mtdModule       ** aParamModules,
                                           mtdModule        * aReturnModule,
                                           idBool           * aIsDeterministic,
                                           idBool           * aIsCachable )
/***********************************************************************
 *
 * Description : PROJ-2452 Result caching of Statement level
 *
 *          Function or Package ���� ������ function ���� ������
 *          validation �������� input parameters, return type �� ����
 *          cache ���������� �˻��ϰ� ����� ��ȯ�Ѵ�.
 *
 *          ex) CREATE OR REPLACE FUNCTION
 *              CREATE OR REPLACE PACKAGE pkg
 *              CREATE OR REPLACE PACKAGE BODY pkg
 *
 * Implementation :
 *
 *          1. Check and set force DETERMINISTIC (for test)
 *          2. Check DETERMINISTIC
 *             Check force cache
 *          3. Check input parameters
 *           - All IN parameter
 *           - Valid data type
 *          4. Check isCachable and return type
 *           - Valid data type
 *
 *      cf) QCU_FORCE_FUNCTION_CACHE �� plan property �� �ƴ϶�
 *          ������ ������ validation �������� ����� �� ����.
 *          validation �������� ����Ϸ��� plan env �� ����ؾ� �ϴµ�
 *          ������ NATC ������ ���� �뵵�θ� �߰��Ǿ����Ƿ�
 *          plan env ����� skip �Ѵ�.
 *
 **********************************************************************/
{
    qsVariableItems  * sParamDecl = NULL;
    idBool  sIsCachable = ID_TRUE;
    UInt    i;

    IDE_ERROR_RAISE( aReturnModule != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    // Check and set force DETERMINISTIC
    if ( QCU_FORCE_FUNCTION_CACHE == QTC_CACHE_FORCE_FUNCTION_DETERMINISTIC )
    {
        *aIsDeterministic = ID_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    // Check DETERMINISTIC function or force cache
    if ( ( *aIsDeterministic == ID_TRUE ) ||
         ( QCU_FORCE_FUNCTION_CACHE == QTC_CACHE_FORCE_FUNCTION_CACHE ) )
    {
        // Check input parameter
        // if no parameter then cachable
        for ( sParamDecl = aParaDecls, i = 0;
              sParamDecl != NULL;
              sParamDecl = sParamDecl->next, i++ )
        {
            if( ( ((qsVariables *)sParamDecl)->inOutType != QS_IN ) ||
                ( QTC_IS_CACHE_TYPE( aParamModules[i] ) == ID_FALSE ) )
            {
                sIsCachable = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
                // sIsCachable = ID_TRUE;
            }
        }
    }
    else
    {
        // Basic function && Not force execution cache
        sIsCachable = ID_FALSE;
    }

    // Check return type
    if ( sIsCachable == ID_TRUE )
    {
        *aIsCachable = QTC_IS_CACHE_TYPE( aReturnModule );
    }
    else
    {
        *aIsCachable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::setIsCachableForFunction",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::validateFunctionCache( qcTemplate * aTemplate,
                                        qtcNode    * aNode )
/***********************************************************************
 *
 * Description : PROJ-2452 Result caching of Statement level
 *
 *      Function ����� validation �������� cache ���������� �˻��Ѵ�.
 *
 *          1. Check cache object count 
 *          2. CREATE SP ���� ���� function ����
 *          3. qsExecParseTree->isCachable �˻�
 *
 *      cf) QCU_QUERY_EXECUTION_CACHE_MAX_COUNT(aTemplate->cacheMaxCnt) ��
 *          plan property �� �ƴ϶� validation �������� ����� �� ����.
 *          ���� cacheObjCnt �� ID_UINT_MAX �̸������� ������Ų��.
 *
 * Implementation :
 *
 **********************************************************************/
{
    qcParseTree      * sParseTree;
    qsExecParseTree  * sExecParseTree;
    idBool             sResult = ID_FALSE;

    IDE_ERROR_RAISE( aTemplate != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    // 1. Check cache object count
    if ( aTemplate->cacheObjCnt < ID_UINT_MAX )
    {
        IDE_ERROR_RAISE( aTemplate->stmt->myPlan->parseTree != NULL, ERR_UNEXPECTED_CACHE_ERROR );
        sParseTree = aTemplate->stmt->myPlan->parseTree;

        // 2. Check SQL, PSM
        if ( sParseTree->stmtKind != QCI_STMT_CRT_SP )
        {
            IDE_ERROR_RAISE( aNode->subquery->myPlan->parseTree != NULL, ERR_UNEXPECTED_CACHE_ERROR );

            sParseTree     = aNode->subquery->myPlan->parseTree;
            sExecParseTree = (qsExecParseTree *)sParseTree;

            // 3. Check isCachable
            if ( sExecParseTree->isCachable == ID_TRUE )
            {
                sResult = ID_TRUE;
            }
            else
            {
                // isCachable false.
                // sResult = ID_FALSE;
            }
        }
        else
        {
            // SP 
            // sResult = ID_FALSE;
        }
    }
    else
    {
        // Overflow defence
        // sResult = ID_FALSE;
    }

    if ( sResult == ID_TRUE )
    {
        aNode->node.info = aTemplate->cacheObjCnt++;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::validateFunctionCache",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::validateSubqueryCache( qcTemplate * aTemplate,
                                        qtcNode    * aNode )
/***********************************************************************
 *
 * Description : PROJ-2448 Subquery caching
 *
 *      Subquery ����� validation �������� cache ���������� �˻��Ѵ�.
 *
 * Implementation : Check cachable
 *
 *          1. Check cache object count
 *             Check subquery type (SET operation)
 *          2. Check hierarchy query 
 *          3. Check data type
 *             - Target column
 *             - Outer column
 *
 **********************************************************************/
{
    qmsQuerySet  * sQuerySet = NULL;
    qmsSFWGH     * sSFWGH    = NULL;
    qmsTarget    * sTarget   = NULL;
    qmsOuterNode * sOuter    = NULL;
    mtcNode      * sNode     = NULL;
    mtcColumn    * sColumn   = NULL;

    qmsConcatElement * sGroup;
    qmsConcatElement * sSubGroup;
    idBool       sIsCachable = ID_TRUE;

    IDE_ERROR_RAISE( aTemplate       != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode           != NULL, ERR_UNEXPECTED_CACHE_ERROR ); 
    IDE_ERROR_RAISE( aNode->subquery != NULL, ERR_UNEXPECTED_CACHE_ERROR ); 

    sQuerySet = ((qmsParseTree*)(aNode->subquery->myPlan->parseTree))->querySet;

    //---------------------------------------------------
    // 1. Check cache object count and query type
    //---------------------------------------------------

    if ( ( aTemplate->cacheObjCnt < ID_UINT_MAX ) &&
         ( sQuerySet->setOp == QMS_NONE ) )
    {
        IDE_ERROR_RAISE( sQuerySet->SFWGH != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        // BUG-44226 scalar subquery�� where ������ �ܺ� ���� �÷��� �ִ� ���
        // subquery caching�� ����մϴ�.
        // target
        for ( sTarget = sQuerySet->SFWGH->target;
              sTarget != NULL;
              sTarget = sTarget->next )
        {
            if ( qtc::dependencyContains( &sQuerySet->SFWGH->depInfo,
                                          &sTarget->targetColumn->depInfo ) == ID_FALSE )
            {
                sIsCachable = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        // group by
        for ( sGroup = sQuerySet->SFWGH->group;
              (sGroup != NULL) && (sIsCachable == ID_TRUE);
              sGroup = sGroup->next )
        {
            if ( sGroup->arithmeticOrList != NULL )
            {
                if ( qtc::dependencyContains( &sQuerySet->SFWGH->depInfo,
                                              &sGroup->arithmeticOrList->depInfo ) == ID_FALSE )
                {
                    sIsCachable = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // rollup, cube, grouping set
                for ( sSubGroup = sGroup->arguments;
                      sSubGroup != NULL;
                      sSubGroup = sSubGroup->next )
                {
                    if ( qtc::dependencyContains( &sQuerySet->SFWGH->depInfo,
                                                  &sSubGroup->arithmeticOrList->depInfo ) == ID_FALSE )
                    {
                        sIsCachable = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
        }

        // having
        if ( (sIsCachable == ID_TRUE) &&
             (sQuerySet->SFWGH->having != NULL) )
        {
            if ( qtc::dependencyContains( &sQuerySet->SFWGH->depInfo,
                                          &sQuerySet->SFWGH->having->depInfo ) == ID_FALSE )
            {
                sIsCachable = ID_FALSE;
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

        //---------------------------------------------------
        // 2. Check hierarchy
        // ex) ���� ���� cache ����
        //     SELECT LEVEL, i1, i2, i3
        //       FROM ( SELECT ( SELECT max(i3) FROM t1 b WHERE i3 = a.i3 ) m, i1, i2, i3
        //                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //                FROM t1 a ) v
        //     START WITH i1 = 1 CONNECT BY PRIOR i1 = i2;
        //---------------------------------------------------

        for ( sSFWGH = sQuerySet->SFWGH;
              sSFWGH != NULL && ( sIsCachable == ID_TRUE );
              sSFWGH = sSFWGH->outerQuery )
        {
            if ( sSFWGH->hierarchy != NULL )
            {
                sIsCachable = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        //---------------------------------------------------
        // 3. Check data type
        //---------------------------------------------------

        // - target
        for ( sTarget = sQuerySet->SFWGH->target;
              ( sTarget != NULL ) && ( sIsCachable == ID_TRUE );
              sTarget = sTarget->next )
        {
            sNode = mtf::convertedNode( &sTarget->targetColumn->node,
                                        &aTemplate->tmplate );

            sColumn = &aTemplate->tmplate.rows[sNode->table].columns[sNode->column];

            sIsCachable = QTC_IS_CACHE_TYPE( sColumn->module );
        }

        // - outer column 
        for ( sOuter = sQuerySet->SFWGH->outerColumns;
              ( sOuter != NULL ) && ( sIsCachable == ID_TRUE );
              sOuter = sOuter->next )
        {
            // BUG-41905
            sColumn = &aTemplate->tmplate.rows[sOuter->column->node.table].
                                       columns[sOuter->column->node.column];

            sIsCachable = QTC_IS_CACHE_TYPE( sColumn->module );
        }
    }
    else
    {
        sIsCachable = ID_FALSE;
    }

    // BUG-41917
    if ( sIsCachable == ID_TRUE )
    {
        if ( aNode->node.info == ID_UINT_MAX )
        {
            // ���� validation
            aNode->node.info = aTemplate->cacheObjCnt++;
        }
        else
        {
            // transform ���� validation
            // Nothing to do.
        }
    }
    else
    {
        aNode->node.info = ID_UINT_MAX;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::validateSubqueryCache",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::preprocessSubqueryCache( qcStatement * aStatement,
                                          qtcNode     * aNode )
/***********************************************************************
 *
 * Description : PROJ-2448 Subquery caching
 *
 *             Subquery optimization ���� �� plan �� �ϼ��� �� ȣ��Ǿ�
 *             STORENONLY tip �� ���� cache disable �� �����ϰ�
 *             execution �� �ʿ��� outer column �� ��ġ�� Ȯ���Ѵ�.
 *
 * Implementation :
 *
 *             1. Check avoid cache and set cache disable
 *              - �󼼼��� issue 6
 *             2. Set mtrNode position of outer columns
 *              - PR-6365.sql
 *              - TC/Server/qp2disk/SQLSpec/Nist/Intermediate/yts798.sql
 *
 **********************************************************************/
{
    qmgPROJ      * sPROJ  = NULL;
    qmsQuerySet  * sQuerySet = NULL;
    qtcNode      * sNode  = NULL;
    qmsOuterNode * sOuter = NULL;
    UShort  sChangeTable  = ID_USHORT_MAX;
    UShort  sChangeColumn = ID_USHORT_MAX;

    IDE_ERROR_RAISE( aStatement != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode      != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    if ( aNode->node.info != ID_UINT_MAX )
    {
        sPROJ = (qmgPROJ *)(aNode->subquery->myPlan->graph);
        IDE_ERROR_RAISE( sPROJ != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        if ( ( sPROJ->subqueryTipFlag & QMG_PROJ_SUBQUERY_STORENSEARCH_MASK )
             == QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY )
        {
            //---------------------------------------------------
            // 1. Check avoid cache and set cache disable
            //---------------------------------------------------

            aNode->node.info = ID_UINT_MAX;
        }
        else
        {
            //---------------------------------------------------
            // 2. Set mtrNode position of outer columns
            //---------------------------------------------------

            sQuerySet = ((qmsParseTree*)aNode->subquery->myPlan->parseTree)->querySet;
            IDE_ERROR_RAISE( sQuerySet->SFWGH != NULL, ERR_UNEXPECTED_CACHE_ERROR );

            for ( sOuter = sQuerySet->SFWGH->outerColumns;
                  sOuter != NULL;
                  sOuter = sOuter->next )
            {
                // BUG-44682 subquery cache ������ �߸��� �޸𸮸� ������ �ֽ��ϴ�.
                // cacheTable, cacheColumn �� �߸��� ��ġ�� ����Ű�� �ִ�.
                // ó�� �����Ҷ��� ����Ѵ�.
                if ( (sOuter->cacheTable  == ID_USHORT_MAX) &&
                     (sOuter->cacheColumn == ID_USHORT_MAX) )
                {
                    sNode = sOuter->column;

                    if ( ( sNode->node.lflag & MTC_NODE_COLUMN_LOCATE_CHANGE_MASK )
                        == MTC_NODE_COLUMN_LOCATE_CHANGE_TRUE )
                    {
                        // BUG-44873 �����������ο� temp table �� outerColumns �� ����� ��� ����� Ʋ���ϴ�.
                        // findColumnLocate �� baseTable �� �̿��Ͽ� ã�´�.
                        // baseTable�� ã���� �������������� temp table �� �������� �ʴ´�.
                        IDE_TEST( qmg::findColumnLocate( aStatement,
                                                         ID_USHORT_MAX,
                                                         sNode->node.baseTable,
                                                         sNode->node.baseColumn,
                                                         &sChangeTable,
                                                         &sChangeColumn )
                                  != IDE_SUCCESS );

                        sOuter->cacheTable  = sChangeTable;
                        sOuter->cacheColumn = sChangeColumn;
                    }
                    else
                    {
                        sOuter->cacheTable  = sNode->node.table;
                        sOuter->cacheColumn = sNode->node.column;
                    }
                }
                else
                {
                    // nothing to do.
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::preprocessSubqueryCache",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::searchCache( qcTemplate     * aTemplate,
                              qtcNode        * aNode,
                              mtcStack       * aStack,
                              qtcCacheType     aCacheType,
                              qtcCacheObj   ** aCacheObj,
                              UInt           * aParamCnt,
                              qtcCacheState  * aState )
/***********************************************************************
 *
 * Description : ��ȿ�� cache object �� ����
 *               input parameter �� ������ cache ���� ã�´�.
 *
 * Implementation : 
 *
 *              1. Check cacheable and preparation
 *               - get parameter count
 *               - case by subquery, set the stack info as outer columm
 *              2. Search
 *               - set cacheObj info
 *               - compare currRecord 
 *               - search hashTable
 *
 *              cf) state �� �Ʒ� �ܰ� �� �ϳ��� �����ȴ�.
 *               - STATE_INVOKE_MAKE_RECORD
 *               - STATE_RETURN_CURR_RECORD
 *               - STATE_RETURN_INVOKE
 *
 * cf) Cache �˰��� ���� �������� ���̾�׷��� �Ʒ���θ� ����
 * http://nok.altibase.com/download/attachments/32923217/PROJ-2448_state_diagram.png
 *
 **********************************************************************/
{
    qtcCacheObj  * sCacheObj = NULL;
    qmsSFWGH     * sSFWGH    = NULL;
    qmsOuterNode * sOuter    = NULL;

    idBool sIsCachable = ID_TRUE;
    idBool sResult     = ID_FALSE;
    UInt sParamCnt = 0;
    UInt i = 0;

    IDE_ERROR_RAISE( aTemplate  != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode      != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aParamCnt  != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( *aCacheObj == NULL, ERR_UNEXPECTED_CACHE_ERROR );

    //---------------------------------------------------
    // 1. Check cacheable and preparation
    //---------------------------------------------------

    if ( ( aTemplate->cacheMaxCnt > 0 ) &&
         ( aTemplate->cacheMaxSize >= ID_SIZEOF(qtcHashRecord) ) &&
         ( aNode->node.info != ID_UINT_MAX ) &&
         ( aNode->node.info <= aTemplate->cacheMaxCnt ) )
    {
        // Get parameter count
        if ( aCacheType == QTC_CACHE_TYPE_DETERMINISTIC_FUNCTION )
        {
            sParamCnt = ((qsExecParseTree *)(aNode->subquery->myPlan->parseTree))->paraDeclCount;
        }
        else
        {
            if ( aTemplate->forceSubqueryCacheDisable == QTC_CACHE_FORCE_SUBQUERY_CACHE_NONE )
            {
                sSFWGH = ((qmsParseTree*)aNode->subquery->myPlan->parseTree)->querySet->SFWGH;
                IDE_ERROR_RAISE( sSFWGH != NULL, ERR_UNEXPECTED_CACHE_ERROR );

                // Set the stack info as outer columm
                for ( sOuter = sSFWGH->outerColumns, i = 0;
                      sOuter != NULL;
                      sOuter = sOuter->next, i++ )
                {
                    IDE_TEST( getColumnAndValue( aTemplate,
                                                 sOuter,
                                                 &aStack[i+1].column,
                                                 &aStack[i+1].value)
                              != IDE_SUCCESS );
                }

                sParamCnt = i;
            }
            else
            {
                sIsCachable = ID_FALSE;
            }
        }
    }
    else
    {
        sIsCachable = ID_FALSE;
    }

    //---------------------------------------------------
    // 2. Search
    //---------------------------------------------------

    if ( sIsCachable == ID_TRUE )
    {
        IDE_ERROR_RAISE( aTemplate->cacheObjects != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        sCacheObj = &(aTemplate->cacheObjects[aNode->node.info]);
        IDE_ERROR_RAISE( sCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        // Set cacheObj info
        sCacheObj->mType     = aCacheType;
        sCacheObj->mParamCnt = sParamCnt;

        if ( ( aTemplate->cacheBucketCnt == 0 ) || ( sCacheObj->mParamCnt == 0 ) )
        {
            sCacheObj->mFlag &= ~QTC_CACHE_HASH_MASK;
            sCacheObj->mFlag |=  QTC_CACHE_HASH_DISABLE;
        }

        // Compare currRecord 
        if ( sCacheObj->mCurrRecord != NULL )
        {
            IDE_TEST( compareCurrRecord( aStack + 1, // parameter stack
                                         sCacheObj,
                                         &sResult )
                      != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_RETURN_CURR_RECORD );
            }
            else
            {
                if ( sCacheObj->mHashTable != NULL )
                {
                    QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_SEARCH_HASH_TABLE );
                }
                else
                {
                    QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_INVOKE_MAKE_RECORD );
                }
            }
        }
        else
        {
            QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_INVOKE_MAKE_RECORD );
        }

        // Search hashTable
        if ( sCacheObj->mState == QTC_CACHE_STATE_SEARCH_HASH_TABLE )
        {
            IDE_TEST( searchHashTable( aStack + 1, // parameter stack
                                       sCacheObj,
                                       &sResult )
                      != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_RETURN_CURR_RECORD );
            }
            else
            {
                QTC_CACHE_SET_STATE( sCacheObj, QTC_CACHE_STATE_INVOKE_MAKE_RECORD );
            }
        }

        // BUG-47591
        switch( sCacheObj->mState )
        {
            case QTC_CACHE_STATE_RETURN_CURR_RECORD:
                sCacheObj->mHitCnt++;
                break;
            case QTC_CACHE_STATE_INVOKE_MAKE_RECORD:
                sCacheObj->mMissCnt++;
                break;
            default:
                IDE_RAISE( ERR_UNEXPECTED_CACHE_ERROR );
        }

        *aState = sCacheObj->mState;
    }
    else
    {
        *aState = QTC_CACHE_STATE_RETURN_INVOKE;
    }

    *aCacheObj = sCacheObj;
    *aParamCnt = sParamCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::searchCache",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::executeCache( iduMemory     * aMemory,
                               mtcNode       * aNode,
                               mtcStack      * aStack,
                               qtcCacheObj   * aCacheObj,
                               UInt            aBucketCnt,
                               qtcCacheState   aState,
                               const UInt      aSubqueryMode )
/***********************************************************************
 *
 * Description : Cache miss �� ��� ��������
 *               current record �� �����ϰ� hash table �� �߰��Ѵ�.
 *
 * Implementation :
 *
 *            1. Make currRecord
 *             - allocAndSetCurrRecord
 *             - resetCurrRecord
 *            2. Make hashTable
 *            3. Insert into hashTable
 *
 **********************************************************************/
{
    IDE_ERROR_RAISE( aMemory != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode   != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack  != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( ( ( aState == QTC_CACHE_STATE_INVOKE_MAKE_RECORD ) ||
                       ( aState == QTC_CACHE_STATE_RETURN_INVOKE ) ),
                       ERR_UNEXPECTED_CACHE_ERROR );

    if ( aState == QTC_CACHE_STATE_INVOKE_MAKE_RECORD )
    {
        IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        //---------------------------------------------------
        // 1. Make currRecord
        //---------------------------------------------------

        if ( ( aCacheObj->mCurrRecord != NULL ) &&
             ( ( aCacheObj->mFlag & QTC_CACHE_HASH_MASK ) == QTC_CACHE_HASH_DISABLE ) )
        {
            // Reuse record
            IDE_ERROR_RAISE( aCacheObj->mHashTable == NULL, ERR_UNEXPECTED_CACHE_ERROR );

            if ( resetCurrRecord( aMemory,
                                  aNode,
                                  aStack,
                                  aCacheObj,
                                  aSubqueryMode ) == IDE_SUCCESS )
            {
                // Reset success
                IDE_ERROR_RAISE( aCacheObj->mCurrRecord != NULL, ERR_UNEXPECTED_CACHE_ERROR );
                if ( ( aCacheObj->mFlag & QTC_CACHE_HASH_MASK ) == QTC_CACHE_HASH_ENABLE )
                {
                    if ( aCacheObj->mHashTable == NULL )
                    {
                        QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_MAKE_HASH_TABLE );
                    }
                    else
                    {
                        QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_INSERT_HASH_RECORD );
                    }
                }
            }
            else
            {
                // Reset failure
                IDE_CLEAR();
            }
        }
        else
        {
            if ( allocAndSetCurrRecord( aMemory,
                                        aNode,
                                        aStack,
                                        aCacheObj,
                                        aSubqueryMode ) == IDE_SUCCESS )
            {
                // Alloc success
                IDE_ERROR_RAISE( aCacheObj->mCurrRecord != NULL, ERR_UNEXPECTED_CACHE_ERROR );
                if ( ( aCacheObj->mFlag & QTC_CACHE_HASH_MASK ) == QTC_CACHE_HASH_ENABLE )
                {
                    if ( aCacheObj->mHashTable == NULL )
                    {
                        QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_MAKE_HASH_TABLE );
                    }
                    else
                    {
                        QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_INSERT_HASH_RECORD );
                    }
                }
            }
            else
            {
                // Alloc failure
                IDE_CLEAR();
            }
        }

        //---------------------------------------------------
        // 2. Make hashTable
        //---------------------------------------------------

        if ( aCacheObj->mState == QTC_CACHE_STATE_MAKE_HASH_TABLE )
        {
            if ( qtcHash::initialize( aMemory,
                                      aBucketCnt,
                                      aCacheObj->mParamCnt,
                                      &aCacheObj->mRemainSize,
                                      &aCacheObj->mHashTable ) == IDE_SUCCESS )
            {
                IDE_ERROR_RAISE( aCacheObj->mHashTable != NULL, ERR_UNEXPECTED_CACHE_ERROR );

                QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_INSERT_HASH_RECORD );
            }
            else
            {
                IDE_CLEAR();
                IDE_ERROR_RAISE( aCacheObj->mHashTable == NULL, ERR_UNEXPECTED_CACHE_ERROR );

                aCacheObj->mFlag &= ~QTC_CACHE_HASH_MASK;
                aCacheObj->mFlag |=  QTC_CACHE_HASH_DISABLE;
            }
        }

        //---------------------------------------------------
        // 3. Insert into hashTable
        //---------------------------------------------------

        if ( aCacheObj->mState == QTC_CACHE_STATE_INSERT_HASH_RECORD )
        {
            IDE_TEST( qtcHash::insert( aCacheObj->mHashTable,
                                       aCacheObj->mCurrRecord )
                      != IDE_SUCCESS );
        }
    }

    if ( aCacheObj != NULL )
    {
        QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_END );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::executeCache",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::getCacheValue( mtcNode     * aNode,
                                mtcStack    * aStack,
                                qtcCacheObj * aCacheObj,
                                const UInt    aSubqueryMode  )
/***********************************************************************
 *
 * Description : Cache hit �� ��� cache ���� currRecord �� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *   1. DETERMINISTIC_FUNCTION
 *    - aStack[0] �� cache ���� ����
 *   2. SCALAR_SUBQUERY
 *    - aStack[0] �� stack position �� ����
 *    - aStack[0].value �� cache value ����
 *   3. LIST_SUBQUERY
 *    - aStack[0] �� stack position �� ����
 *    - aStack[0].value �� sStack[0]~[n] �� target list �� cache value ����
 *   4. [NOT] EXISTS
 *    - aStack[0].value �� cache value ����
 *
 **********************************************************************/
{
    mtcNode  * sNode  = NULL;
    mtcStack * sStack = NULL;
    UInt sOffset = 0;
    UInt i = 0;

    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj->mCurrRecord         != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj->mCurrRecord->mValue != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    switch ( aCacheObj->mType )
    {
        case QTC_CACHE_TYPE_DETERMINISTIC_FUNCTION:

            idlOS::memcpy( aStack[0].value,
                           (SChar *)aCacheObj->mCurrRecord->mValue,
                           aStack[0].column->column.size );
            break;

        // BUG-41915
        // subquery �� ��� cache �� value ���� pointer �� �����Ѵ�.
        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_SURE:
        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_UNKNOWN:

            // stack position copy
            idlOS::memcpy( aStack,
                           (SChar *)aCacheObj->mCurrRecord->mValue,
                           ID_SIZEOF(mtcStack) );

            sOffset = ID_SIZEOF(mtcStack);

            if ( ( aSubqueryMode == 1 ) &&
                 ( aCacheObj->mType == QTC_CACHE_TYPE_SCALAR_SUBQUERY_UNKNOWN ) )
            {
                /* BUG-48776
                 * one row return unknown �迭�� scalar subquery��
                 * stack�� mtcColumn������ �߰��� cache�Ѵ�.
                 */
                sOffset += ID_SIZEOF(mtcColumn);
            }
            else
            {
                /* Nothing to do. */
            }

            aStack[0].value = (SChar *)aCacheObj->mCurrRecord->mValue + sOffset;

            break;

        case QTC_CACHE_TYPE_LIST_SUBQUERY_SURE:
        case QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE:

            // stack position copy
            // BUG-41968 align
            sOffset = idlOS::align( sOffset, aStack[0].column->module->align );
            aStack[0].value = (SChar *)aCacheObj->mCurrRecord->mValue;
            sOffset += aStack[0].column->column.size;

            sStack = (mtcStack *)aStack[0].value;

            for ( sNode = aNode->arguments, i = 0;
                  sNode != NULL;
                  sNode = sNode->next, i++ )
            {
                if ( ( aSubqueryMode == 1 ) &&
                     ( aCacheObj->mType == QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE ) )
                {
                    /* BUG-48776
                     * one row return unknown �迭�� scalar subquery��
                     * stack�� mtcColumn������ �߰��� cache�Ѵ�.
                     */
                    sOffset += ID_SIZEOF(mtcColumn);
                }
                else
                {
                    /* Nothing to do. */
                }       

                sOffset = idlOS::align( sOffset, sStack[i].column->module->align );
                sStack[i].value = (SChar *)aCacheObj->mCurrRecord->mValue + sOffset;

                sOffset += sStack[i].column->column.size;
            }
            break;

        case QTC_CACHE_TYPE_EXISTS_SUBQUERY:
        case QTC_CACHE_TYPE_NOT_EXISTS_SUBQUERY:

            aStack[0].value = (SChar *)aCacheObj->mCurrRecord->mValue;
            break;

        default:

            IDE_ERROR_RAISE( 0, ERR_UNEXPECTED_CACHE_ERROR );
            break;
    }

    QTC_CACHE_SET_STATE( aCacheObj, QTC_CACHE_STATE_END );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::getCacheValue",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::getColumnAndValue( qcTemplate    * aTemplate,
                                    qmsOuterNode  * aOuterColumn,
                                    mtcColumn    ** aColumn,
                                    void         ** aValue )
/***********************************************************************
 *
 * Description : PROJ-2448 Subquery caching
 *               Outer column �� ���� column, value �� ��ȯ�Ѵ�.
 *
 * Implementation : 
 *
 **********************************************************************/
{
    UShort       sTablePos  = 0;
    UShort       sColumnPos = 0;
    mtcColumn  * sColumn = NULL;
    void       * sValue  = NULL;

    IDE_ERROR_RAISE( aTemplate    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aOuterColumn != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aColumn      != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aValue       != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    if ( aOuterColumn->column->node.module == &qtc::passModule )
    {
        // PR-7477 error
        sTablePos  = aOuterColumn->column->node.arguments->table;
        sColumnPos = aOuterColumn->column->node.arguments->column;
    }
    else
    {
        // PR-6365 error
        // TC/Server/qp2disk/SQLSpec/Nist/Intermediate/yts798.sql error
        sTablePos = aOuterColumn->cacheTable;
        sColumnPos = aOuterColumn->cacheColumn;
    }

    IDE_ERROR_RAISE( aTemplate->tmplate.rows[sTablePos].row != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    // BUG-41462
    sColumn = &aTemplate->tmplate.rows[sTablePos].columns[sColumnPos];

    // BUG-45141
    IDE_ERROR_RAISE( sColumn->module != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sValue  = (void*)mtd::valueForModule( (smiColumn*)sColumn,
                                          aTemplate->tmplate.rows[sTablePos].row,
                                          MTD_OFFSET_USE,
                                          sColumn->module->staticNull );

    *aColumn = sColumn;
    *aValue  = sValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::getColumnAndValue",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::compareCurrRecord( mtcStack    * aStack,
                                    qtcCacheObj * aCacheObj,
                                    idBool      * aResult )
/***********************************************************************
 *
 * Description : Input parameter �� current record �� ���Ͽ�
 *               ������ TRUE, �ٸ��� FALSE �� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *               1. Calculate hash key
 *               2. Compare 
 *                - compareKeyData
 *
 **********************************************************************/
{
    qtcHashRecord * sRecord = NULL;
    idBool sResult = ID_FALSE;
    UInt   sKey = mtc::hashInitialValue;
    UInt   i = 0;

    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aResult   != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sRecord = aCacheObj->mCurrRecord;

    if ( aCacheObj->mParamCnt > 0 )
    {
        // Exists parameter
        IDE_ERROR_RAISE( sRecord->mKeyLengthArr != NULL, ERR_UNEXPECTED_CACHE_ERROR );
        IDE_ERROR_RAISE( sRecord->mKeyData      != NULL, ERR_UNEXPECTED_CACHE_ERROR );

        //---------------------------------------------------
        // 1. Calculate hash key
        //---------------------------------------------------

        for ( i = 0; i < aCacheObj->mParamCnt; i++ )
        {
            sKey = aStack[i].column->module->hash( sKey,
                                                   aStack[i].column,
                                                   aStack[i].value );
        }

        //---------------------------------------------------
        // 2. Compare 
        //---------------------------------------------------

        if ( sRecord->mKey == sKey )
        {
            IDE_TEST( qtcHash::compareKeyData( aStack,
                                               sRecord,
                                               aCacheObj->mParamCnt,
                                               &sResult )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
            // sResult = ID_FALSE;
        }
    }
    else
    {
        IDE_ERROR_RAISE( sRecord->mKey          == 0,    ERR_UNEXPECTED_CACHE_ERROR );
        IDE_ERROR_RAISE( sRecord->mKeyLengthArr == NULL, ERR_UNEXPECTED_CACHE_ERROR );
        IDE_ERROR_RAISE( sRecord->mKeyData      == NULL, ERR_UNEXPECTED_CACHE_ERROR );

        sResult = ID_TRUE;
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::compareCurrRecord",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::searchHashTable( mtcStack    * aStack,
                                  qtcCacheObj * aCacheObj,
                                  idBool      * aResult )
/***********************************************************************
 *
 * Description : Input parameter �� hash table �� record �� ���Ͽ�
 *               ������ TRUE, �ٸ��� FALSE �� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *               1. Calculate hash key
 *               2. Search bucket index
 *               3. Search in the bucket list
 *                - compareKeyData
 *
 **********************************************************************/
{
    qtcHashTable * sHashTable = NULL;
    iduList      * sHeader = NULL;
    iduList      * sCurr   = NULL;
    idBool sResult = ID_FALSE;
    UInt   sKey    = mtc::hashInitialValue;
    UInt   sIndex  = 0;
    UInt   i = 0;

    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aResult   != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sHashTable = aCacheObj->mHashTable;
    IDE_ERROR_RAISE( sHashTable != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( sHashTable->mKeyCnt    > 0, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( sHashTable->mBucketCnt > 0, ERR_UNEXPECTED_CACHE_ERROR );

    //---------------------------------------------------
    // 1. Calculate hash key
    //---------------------------------------------------

    for ( i = 0; i < sHashTable->mKeyCnt; i++ )
    {
        sKey = aStack[i].column->module->hash( sKey,
                                               aStack[i].column,
                                               aStack[i].value );
    }

    //---------------------------------------------------
    // 2. Search bucket index
    //---------------------------------------------------

    sIndex  = qtcHash::hash( sKey, sHashTable->mBucketCnt );
    sHeader = &(sHashTable->mBucketArr[sIndex].mRecordList);
    sCurr   = sHeader->mNext;

    //---------------------------------------------------
    // 3. Search in the bucket list
    //---------------------------------------------------

    while ( ( sCurr != sHeader) &&
            ( ((qtcHashRecord *)sCurr->mObj)->mKey <= sKey ) )
    {
        if ( ((qtcHashRecord *)sCurr->mObj)->mKey == sKey )
        {
            IDE_TEST( qtcHash::compareKeyData( aStack,
                                               (qtcHashRecord *)sCurr->mObj,
                                               sHashTable->mKeyCnt,
                                               &sResult )
                      != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                break;
            }
            else
            {
                sCurr = sCurr->mNext;
            }
        }
        else
        {
            sCurr = sCurr->mNext;
        }
    }

    if ( sResult == ID_TRUE )
    {
        // Change current record.
        aCacheObj->mCurrRecord = (qtcHashRecord *)sCurr->mObj;
    }
    else
    {
        // Keeping current record.
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcHash::searchHashTable",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::allocAndSetCurrRecord( iduMemory   * aMemory,
                                        mtcNode     * aNode,
                                        mtcStack    * aStack,
                                        qtcCacheObj * aCacheObj,
                                        const UInt    aSubqueryMode )
/***********************************************************************
 *
 * Description : �������� stack ������ cacheObj->mCurrRecord �� �����Ѵ�.
 *
 * Implementation :
 *
 *               1. Get key and need size
 *               2. Compare size 
 *               3. Alloc memory
 *                - alloc mCurrRecord
 *                - alloc and set mCurrRecord->mValue
 *                - alloc and set mCurrRecord->mKeyLengthArr
 *                - alloc and set mCurrRecord->mKeyData
 *                - reset remain size
 *               4. Set currRecord
 *
 **********************************************************************/
{
    qtcHashRecord * sRecord = NULL;
    UInt sKeyDataSize = 0;
    UInt sValueSize   = 0;
    UInt sNeedSize    = 0;
    UInt sKey = mtc::hashInitialValue;

    IDE_ERROR_RAISE( aMemory   != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    //---------------------------------------------------
    // 1. Get key and need size
    //---------------------------------------------------

    IDE_TEST( qtcCache::getKeyAndSize( aNode,
                                       aStack,
                                       aCacheObj,
                                       &sKey,
                                       &sKeyDataSize,
                                       &sValueSize,
                                       aSubqueryMode )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // 2. Compare size
    //---------------------------------------------------

    sNeedSize = ID_SIZEOF(qtcHashRecord) +                  // for current record
                (ID_SIZEOF(UInt) * aCacheObj->mParamCnt) +  // for key length array
                sKeyDataSize +                              // for key data
                sValueSize;                                 // for return value

    IDE_TEST( aCacheObj->mRemainSize < sNeedSize );

    //---------------------------------------------------
    // 3. Alloc memory
    //---------------------------------------------------

    IDU_FIT_POINT( "qtcCache::allocAndSetCurrRecord::alloc1",
                   idERR_ABORT_InsufficientMemory);

    // Alloc currRecord
    IDE_TEST( aMemory->alloc( ID_SIZEOF(qtcHashRecord), (void **)&sRecord )
              != IDE_SUCCESS);

    // Alloc value
    IDE_TEST( aMemory->alloc( sValueSize, (void **)&sRecord->mValue )
              != IDE_SUCCESS);

    // Alloc keyLengthArr, keyData and
    if ( aCacheObj->mParamCnt > 0 )
    {
        IDU_FIT_POINT( "qtcCache::allocAndSetCurrRecord::alloc2",
                       idERR_ABORT_InsufficientMemory);

        IDE_TEST( aMemory->alloc( ID_SIZEOF(UInt) * aCacheObj->mParamCnt,
                                  (void **)&sRecord->mKeyLengthArr )
                  != IDE_SUCCESS);

        IDE_TEST( aMemory->alloc( sKeyDataSize, (void **)&sRecord->mKeyData )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    // Reset remain size
    aCacheObj->mRemainSize -= sNeedSize;

    //---------------------------------------------------
    // 4. Set currRecord
    //---------------------------------------------------

    aCacheObj->mCurrRecord = sRecord;

    IDE_TEST( qtcCache::setKeyAndSize( aNode,
                                       aStack,
                                       aCacheObj,
                                       sKey,
                                       aSubqueryMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::allocAndSetCurrRecord",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::resetCurrRecord( iduMemory   * aMemory,
                                  mtcNode     * aNode,
                                  mtcStack    * aStack,
                                  qtcCacheObj * aCacheObj,
                                  const UInt    aSubqueryMode )
/***********************************************************************
 *
 * Description : �������� stack ������ cacheObj->mCurrRecord �� ������Ѵ�.
 *
 * Implementation :
 *
 *               1. Get key and need size
 *               2. Calculate key data size of currRecord
 *               3. Compare size & Alloc memory
 *                - alloc and set mCurrRecord->mKeyData
 *                - reset remain size
 *               4. Reset currRecord
 *
 **********************************************************************/
{
    qtcHashRecord * sRecord  = NULL;
    void          * sKeyData = NULL;
    UInt sValueSize   = 0;
    UInt sKeyDataSize = 0;
    UInt sRecordKeyDataSize = 0;
    UInt sKey = mtc::hashInitialValue;
    UInt i = 0;

    IDE_ERROR_RAISE( aMemory   != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj->mCurrRecord != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sRecord = aCacheObj->mCurrRecord;

    //---------------------------------------------------
    // 1. Get key and need size
    //---------------------------------------------------

    IDE_TEST( qtcCache::getKeyAndSize( aNode,
                                       aStack,
                                       aCacheObj,
                                       &sKey,
                                       &sKeyDataSize,
                                       &sValueSize,
                                       aSubqueryMode )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // 2. Calculate key data size of currRecord
    //---------------------------------------------------

    for ( i = 0; i < aCacheObj->mParamCnt; i++ )
    {
        sRecordKeyDataSize += sRecord->mKeyLengthArr[i];
    }

    //---------------------------------------------------
    // 3. Compare size & Alloc memory
    //---------------------------------------------------

    if ( ( aCacheObj->mParamCnt > 0 ) && ( sKeyDataSize > sRecordKeyDataSize ) )
    {
        // Disable reuse (alloc)
        IDE_TEST( aCacheObj->mRemainSize < sKeyDataSize );

        // Alloc currRecord->mKeyData
        IDU_FIT_POINT( "qtcCache::resetCurrRecord::alloc1", idERR_ABORT_InsufficientMemory );

        IDE_TEST( aMemory->alloc( sKeyDataSize, (void **)&sKeyData ) != IDE_SUCCESS );

        sRecord->mKeyData = sKeyData;

        // Reset remain size
        aCacheObj->mRemainSize -= sKeyDataSize;
    }
    else
    {
        // Enable reuse
        // Nothing to do.
    }

    //---------------------------------------------------
    // 4. Reset currRecord
    //---------------------------------------------------

    IDE_TEST( qtcCache::setKeyAndSize( aNode,
                                       aStack,
                                       aCacheObj,
                                       sKey,
                                       aSubqueryMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::resetCurrRecord",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::getKeyAndSize( mtcNode     * aNode,
                                mtcStack    * aStack,
                                qtcCacheObj * aCacheObj,
                                UInt        * aKey,
                                UInt        * aKeyDataSize,
                                UInt        * aValueSize,
                                const UInt    aSubqueryMode )
/***********************************************************************
 *
 * Description : ���ο� record �� ������ �� �ʿ���
 *               key ���� size �� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *               1. Calculate key and key data size
 *               2. Calculate value size
 *
 **********************************************************************/
{
    mtcNode * sNode = NULL;
    UInt sKey = mtc::hashInitialValue;
    UInt sKeyDataSize = 0;
    UInt sValueSize   = 0;
    UInt sParamCnt    = 0;
    UInt i = 0;

    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sParamCnt = aCacheObj->mParamCnt;

    //---------------------------------------------------
    // 1. Calculate key and key data size
    //---------------------------------------------------

    for ( i = 1; i <= sParamCnt; i++ )
    {
        sKeyDataSize +=
            aStack[i].column->module->actualSize( aStack[i].column,
                                                  aStack[i].value );
        sKey = aStack[i].column->module->hash( sKey,
                                               aStack[i].column,
                                               aStack[i].value );
    }

    //---------------------------------------------------
    // 2. Calculate value size
    //---------------------------------------------------

    switch ( aCacheObj->mType )
    {
        case QTC_CACHE_TYPE_DETERMINISTIC_FUNCTION:

            sValueSize = aStack[0].column->column.size;
            break;

        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_UNKNOWN:

            /* BUG-48776
             * one row return unknown �迭�� scalar subquery��
             * stack�� mtcColumn������ �߰��� cache�Ѵ�.
             */
            if ( aSubqueryMode == 1 )
            {
                sValueSize = ID_SIZEOF(mtcColumn);
            }
            else
            {
                /* Nothing to do. */
            }

            /* fall through */

        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_SURE:

            sValueSize += ID_SIZEOF(mtcStack) + aStack[1+sParamCnt].column->column.size;
            break;

        case QTC_CACHE_TYPE_LIST_SUBQUERY_SURE:
        case QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE:

            // BUG-41968 align
            sValueSize = idlOS::align( sValueSize, aStack[0].column->module->align );
            sValueSize += aStack[0].column->column.size;

            IDE_ERROR_RAISE( aNode->arguments != NULL, ERR_UNEXPECTED_CACHE_ERROR );

            for ( sNode = aNode->arguments, i = 1 + sParamCnt;
                  sNode != NULL;
                  sNode = sNode->next, i++ )
            {
                if ( ( aSubqueryMode == 1 ) &&
                     ( aCacheObj->mType == QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE ) )
                {
                    /* BUG-48776
                     * one row return unknown �迭�� scalar subquery��
                     * stack�� mtcColumn������ �߰��� cache�Ѵ�.
                     */
                    sValueSize += ID_SIZEOF(mtcColumn);
                }
                else
                {
                    /* Nothing to do. */
                }

                sValueSize = idlOS::align( sValueSize, aStack[i].column->module->align );
                sValueSize += aStack[i].column->column.size;
            }
            break;

        case QTC_CACHE_TYPE_EXISTS_SUBQUERY:
        case QTC_CACHE_TYPE_NOT_EXISTS_SUBQUERY:

            sValueSize = ID_SIZEOF(mtdBooleanType);
            break;

        default:

            IDE_ERROR_RAISE( 0, ERR_UNEXPECTED_CACHE_ERROR );
            break;
    }

    *aKey         = ( sParamCnt > 0 ) ? sKey: 0;
    *aKeyDataSize = sKeyDataSize;
    *aValueSize   = sValueSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::getKeyAndSize",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCache::setKeyAndSize( mtcNode     * aNode,
                                mtcStack    * aStack,
                                qtcCacheObj * aCacheObj,
                                UInt          aKey,
                                const UInt    aSubqueryMode )
/***********************************************************************
 *
 * Description : ���ο� record �� ������ �� �ʿ��� ���� �����Ѵ�.
 *
 * Implementation :
 *
 *    1. Set key, keyLengthArr, keyData (input value)
 *    2. Set return value (output value -> currRecord.mValue)
 *      2.1. DETERMINISTIC_FUNCTION : aStack[0].value �� ����
 *      2.2. SCALAR_SUBQUERY
 *       - aStack[1+ParamCnt] position �� ����
 *       - aStack[1+ParamCnt].value �� ����
 *      2.3. LIST_SUBQUERY
 *       - aStack[1+ParamCnt] position �� ����
 *       - aStack[1+ParamCnt]~[1+ParamCnt+n].value �� ����
 *      2.4. EXISTS : aStack[0].value �� ����
 *    3. Set recordListNode
 *
 **********************************************************************/
{
    qtcHashRecord * sRecord = NULL;
    mtcNode       * sNode = NULL;
    UInt sParamCnt = 0;
    UInt sOffset   = 0;
    UInt i = 0;
    UInt j = 0;

    IDE_ERROR_RAISE( aNode     != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aStack    != NULL, ERR_UNEXPECTED_CACHE_ERROR );
    IDE_ERROR_RAISE( aCacheObj != NULL, ERR_UNEXPECTED_CACHE_ERROR );

    sRecord   = aCacheObj->mCurrRecord;
    sParamCnt = aCacheObj->mParamCnt;

    //---------------------------------------------------
    // 1. Set key, keyLengthArr, keyData
    //---------------------------------------------------

    if ( sParamCnt > 0 )
    {
        // key
        sRecord->mKey = aKey;

        for ( i = 0; i < sParamCnt; i++ )
        {
            // keyLengthArr
            sRecord->mKeyLengthArr[i] = aStack[i+1].column->module->actualSize(
                                            aStack[i+1].column,
                                            aStack[i+1].value );

            // keyData
            idlOS::memcpy( (SChar *)sRecord->mKeyData + sOffset,
                           aStack[i+1].value,
                           sRecord->mKeyLengthArr[i] );

            sOffset += sRecord->mKeyLengthArr[i];
        }
    }
    else
    {
        sRecord->mKey = 0;
        sRecord->mKeyLengthArr = NULL;
        sRecord->mKeyData      = NULL;
    }

    //---------------------------------------------------
    // 2. Set return value
    //---------------------------------------------------

    switch ( aCacheObj->mType )
    {
        case QTC_CACHE_TYPE_DETERMINISTIC_FUNCTION:

            idlOS::memcpy( sRecord->mValue,
                           aStack[0].value,
                           aStack[0].column->column.size );
            break;

        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_SURE:
        case QTC_CACHE_TYPE_SCALAR_SUBQUERY_UNKNOWN:

            sOffset = ID_SIZEOF(mtcStack);

            idlOS::memcpy( (SChar *)sRecord->mValue,
                           aStack + 1 + sParamCnt,
                           sOffset );

            if ( ( aSubqueryMode == 1 ) &&
                 ( aCacheObj->mType == QTC_CACHE_TYPE_SCALAR_SUBQUERY_UNKNOWN ) )
            {
                /* BUG-48776
                 * one row return unknown �迭�� scalar subquery��
                 * stack�� mtcColumn������ �߰��� cache�Ѵ�.
                 */
                idlOS::memcpy( (SChar *)sRecord->mValue + sOffset,
                               (aStack + 1 + sParamCnt)->column,
                               ID_SIZEOF(mtcColumn) );

                ((mtcStack*)sRecord->mValue)->column = (mtcColumn*)((SChar *)sRecord->mValue + sOffset);

                sOffset += ID_SIZEOF(mtcColumn);
            }
            else
            {
                /* Nothing to do. */
            }

            idlOS::memcpy( (SChar *)sRecord->mValue + sOffset,
                           aStack[1+sParamCnt].value,
                           aStack[1+sParamCnt].column->column.size );
            break;

        case QTC_CACHE_TYPE_LIST_SUBQUERY_SURE:
        case QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE:

            // Subquery �� Target ������ ������ Stack�� ��°�� ������ ������ ����
            // BUG-41968 align
            sOffset = 0;
            sOffset = idlOS::align( sOffset, aStack[0].column->module->align );

            idlOS::memcpy( (SChar *)sRecord->mValue,
                           aStack + 1 + sParamCnt,
                           aStack[0].column->column.size );

            sOffset += aStack[0].column->column.size;

            // Subquery �� Target �� ���� value �� ����
            IDE_ERROR_RAISE( aNode->arguments != NULL, ERR_UNEXPECTED_CACHE_ERROR );

            for ( sNode = aNode->arguments, i = 1 + sParamCnt, j = 0;
                  sNode != NULL;
                  sNode = sNode->next, i++, j++ )
            {
                if ( ( aSubqueryMode == 1 ) &&
                     ( aCacheObj->mType == QTC_CACHE_TYPE_LIST_SUBQUERY_TWICE ) )
                {
                    /* BUG-48776
                     * one row return unknown �迭�� scalar subquery��
                     * stack�� mtcColumn������ �߰��� cache�Ѵ�.
                     */
                    idlOS::memcpy( (SChar *)sRecord->mValue + sOffset,
                                   aStack[i].column,
                                   ID_SIZEOF(mtcColumn) );

                    (((mtcStack*)sRecord->mValue) + j)->column = (mtcColumn*)((SChar *)sRecord->mValue + sOffset);

                    sOffset += ID_SIZEOF(mtcColumn);
                }
                else
                {
                    /* Nothing to do. */
                }

                sOffset = idlOS::align( sOffset, aStack[i].column->module->align );

                idlOS::memcpy( (SChar *)sRecord->mValue + sOffset,
                               aStack[i].value,
                               aStack[i].column->column.size );

                sOffset += aStack[i].column->column.size;
            }
            break;

        case QTC_CACHE_TYPE_EXISTS_SUBQUERY:
        case QTC_CACHE_TYPE_NOT_EXISTS_SUBQUERY:

            idlOS::memcpy( (SChar *)sRecord->mValue,
                           aStack[0].value,
                           aStack[0].column->column.size );
            break;

        default:

            IDE_ERROR_RAISE( 0, ERR_UNEXPECTED_CACHE_ERROR );
            break;
    }

    //---------------------------------------------------
    // 3. Set recordListNode
    //---------------------------------------------------

    IDU_LIST_INIT_OBJ( &sRecord->mRecordListNode, sRecord );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_CACHE_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qtcCache::setKeyAndSize",
                                  "Unexpected execution cache error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
