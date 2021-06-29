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
 * $Id: qtcColumn.cpp 84991 2019-03-11 09:21:00Z andrew.shin $
 *
 * Description :
 *
 *     Column�� �ǹ��ϴ� Node
 *     Ex) T1.i1
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <mte.h>
#include <qtc.h>
#include <smi.h>
#include <qmvQTC.h>
#include <qsxArray.h>
#include <qsvEnv.h>
#include <qcuSessionPkg.h>

extern mtdModule mtdList;

extern mtxModule mtxColumn; /* PROJ-2632 */

//-----------------------------------------
// Column �������� �̸��� ���� ����
//-----------------------------------------

static mtcName qtcNames[1] = {
    { NULL, 6, (void*)"COLUMN" }
};

//-----------------------------------------
// Column �������� Module �� ���� ����
//-----------------------------------------

static IDE_RC qtcColumnEstimate( mtcNode*     aNode,
                                 mtcTemplate* aTemplate,
                                 mtcStack*    aStack,
                                 SInt         aRemain,
                                 mtcCallBack* aCallBack );

mtfModule qtc::columnModule = {
    1|                      // �ϳ��� Column ����
    MTC_NODE_INDEX_USABLE|  // Index�� ����� �� ����
    MTC_NODE_OPERATOR_MISC, // ��Ÿ ������
    ~0,                     // Indexable Mask : �ǹ� ����
    1.0,                    // default selectivity (�� ������ �ƴ�)
    qtcNames,               // �̸� ����
    NULL,                   // Counter ������ ����
    mtf::initializeDefault, // ���� ������ �ʱ�ȭ �Լ�, ����
    mtf::finalizeDefault,   // ���� ����� ���� �Լ�, ����
    qtcColumnEstimate       // Estimate �� �Լ�
};

//-----------------------------------------
// Column �������� ���� �Լ��� ����
//-----------------------------------------

IDE_RC qtcCalculate_Column( mtcNode*  aNode,
                            mtcStack* aStack,
                            SInt      aRemain,
                            void*     aInfo,
                            mtcTemplate* aTemplate );

IDE_RC qtcCalculate_ArrayColumn( mtcNode*  aNode,
                                 mtcStack* aStack,
                                 SInt      aRemain,
                                 void*     aInfo,
                                 mtcTemplate* aTemplate );

//fix PROJ-1596
IDE_RC qtcCalculate_IndirectArrayColumn( mtcNode*  aNode,
                                         mtcStack* aStack,
                                         SInt      aRemain,
                                         void*     aInfo,
                                         mtcTemplate* aTemplate );

IDE_RC qtcCalculate_IndirectColumn( mtcNode*  aNode,
                                    mtcStack* aStack,
                                    SInt      aRemain,
                                    void*     aInfo,
                                    mtcTemplate* aTemplate );

static const mtcExecute qtcExecute = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_Column,  // COLUMN ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtxColumn.mCommon,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

static const mtcExecute qtcExecuteArrayColumn = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_ArrayColumn,  // COLUMN ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

static const mtcExecute qtcExecuteIndirectArrayColumn = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_IndirectArrayColumn,  // COLUMN ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

static const mtcExecute qtcExecuteIndirectColumn = {
    mtf::calculateNA,     // Aggregation �ʱ�ȭ �Լ�, ����
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    mtf::calculateNA,
    mtf::calculateNA,     // Aggregation ���� �Լ�, ����
    qtcCalculate_IndirectColumn,  // COLUMN ���� �Լ�
    NULL,                 // ������ ���� �ΰ� ����, ����
    mtx::calculateNA,
    mtk::estimateRangeNA, // Key Range ũ�� ���� �Լ�, ����
    mtk::extractRangeNA   // Key Range ���� �Լ�, ����
};

IDE_RC qtcColumnEstimate( mtcNode*     aNode,
                          mtcTemplate* aTemplate,
                          mtcStack*    aStack,
                          SInt         aRemain,
                          mtcCallBack* aCallBack)
{
/***********************************************************************
 *
 * Description :
 *    Column �����ڿ� ���Ͽ� Estimate ������.
 *    Column Node�� ���� Column ���� �� Execute ������ Setting�Ѵ�.
 *
 * Implementation :
 *
 *    Column�� ID�� �Ҵ�ް�, dependencies �� execute ������ Setting�Ѵ�.
 *
 ***********************************************************************/

    qtcNode           * sNode         = (qtcNode *)aNode;
    qtcCallBackInfo   * sCallBackInfo = (qtcCallBackInfo*)(aCallBack->info);
    qsVariables       * sArrayVariable = NULL;
    qtcColumnInfo     * sColumnInfo;

    //fix PROJ-1596
    mtcNode           * sRealNode  = NULL;
    qcStatement       * sStatement = ((qcTemplate *)aTemplate)->stmt;
    // Indirect Calculate Flag
    idBool              sIdcFlag   = ID_FALSE;
    mtcColumn           sRealColumn;
    mtcColumn           sColumn;
    qmsSFWGH          * sSFWGH;
    mtcNode           * sConvertedNode;
    qtcNode           * sSubQNode;
    // PROJ-1073 Package
    qsxPkgInfo        * sPkgInfo;
    mtcTemplate       * sMtcTmplate;

    const mtdModule   * sModule;
    qsTypes           * sRealType;
    qsTypes           * sType;

    sSFWGH = sCallBackInfo->SFWGH;

    // ���� estimate �ÿ��� Column ID�� �Ҵ�ް� �ϰ�,
    // ������ estimate ȣ��ÿ��� Column ID�� �Ҵ���� �ʵ��� �Ѵ�.
    // ����, estimate() ������ CallBackInfo�� statement�� �����Ѵ�.
    if (sCallBackInfo->statement != NULL)
    {
        // ���� Column�� ��� Column ID�� Setting�Ѵ�.
        // Column�� �ƴ� ��쿡�� �ش� Node�� ������ Module�� ����ȴ�.
        // ���� ���, ������ ���� ���Ǹ� ���� ����.
        //     SELECT f1 FROM T1;
        // Parsing �ܰ迡���� [f1]�� Column���� �Ǵ�������,
        // �̴� Column�ϼ��� Function�� ���� �ִ�.
        // ���� Module�� ����� �����, ���ο��� estimate�� ����ȴ�.

        if( ( ( sNode->lflag & QTC_NODE_COLUMN_ESTIMATE_MASK ) ==
              QTC_NODE_COLUMN_ESTIMATE_TRUE )  ||
            ( ( sNode->lflag & QTC_NODE_PROC_VAR_ESTIMATE_MASK ) ==
              QTC_NODE_PROC_VAR_ESTIMATE_TRUE ) )
        {
            // procedure������ estimate�� �̹� �߰ų�,
            // partition column id�� �����Ͽ����Ƿ� column id�� ���� ���ϴ� ����
            // ���� �ʴ´�.
        }
        else
        {
            IDE_TEST(qmvQTC::setColumnID( sNode,
                                          aTemplate,
                                          aStack,
                                          aRemain,
                                          aCallBack,
                                          &sArrayVariable,
                                          &sIdcFlag,
                                          &sSFWGH )
                     != IDE_SUCCESS);
        }
    }

    // column module may have changed to function module
    if ( sNode->node.module == & qtc::columnModule )
    {
        if( ( sIdcFlag        == ID_FALSE ) ||
            ( aNode->objectID == 0 ) )
        {
            //---------------------------------------------------------------
            // [PR-7108]
            // LEVEL, SEQUENCE, SYSDATE ���� Column Module�̱�� �ϳ�
            // Dependency�� ���� ���õ� Column�� �ƴϴ�.
            // ����, PRIOR Column�� ���� Column�̱�� �ϳ�, �� ����
            // Dependency�� ������ ����.
            // ����, Dependencies�� �����ϴ� Column�� ���� Table Column�̰ų�
            // View�� Column���� �����Ѵ�.  �׸���, PRIOR Column�� �ƴϾ�� �Ѵ�.
            // �̷��� �ϴ� ������ Plan Node�� ��ǥ Dependency������
            // Indexable Predicate�� �з��� �����ϰ� �ϱ� �����̴�.
            // (����) Store And Search��� ���� Optimization ���� �߿� �����
            //        VIEW�� Column�� ���� Dependencies�� �������� �ȵȴ�.
            //---------------------------------------------------------------

            if ( ( (aTemplate->rows[aNode->table].lflag & MTC_TUPLE_TYPE_MASK)
                   == MTC_TUPLE_TYPE_TABLE )
                 ||
                 ( (aTemplate->rows[aNode->table].lflag & MTC_TUPLE_VIEW_MASK )
                   == MTC_TUPLE_VIEW_TRUE ) )
            {
                //-----------------------------------------------
                // PROJ-1473
                // ���ǿ� ���� �÷������� �����Ѵ�.
                // ���ڵ��������� ó���� ���,
                // ��ũ���̺�� ���� ���ǿ� ���� �÷����� ����.
                //-----------------------------------------------

                IDE_TEST( qtc::setColumnExecutionPosition( aTemplate,
                                                           sNode,
                                                           sSFWGH,
                                                           sCallBackInfo->SFWGH )
                          != IDE_SUCCESS );

                /* PROJ-2462 ResultCache */
                qtc::checkLobAndEncryptColumn( aTemplate, aNode );

                // To Fix PR-9050
                // PRIOR Column�� ���� ���δ� qtcNode->flag���� �˻��ؾ� ��.
                if ( (sNode->lflag & QTC_NODE_PRIOR_MASK)
                     == QTC_NODE_PRIOR_ABSENT )
                {
                    qtc::dependencySet( sNode->node.table, & sNode->depInfo );
                }
                else
                {
                    qtc::dependencyClear( & sNode->depInfo );
                }
            }
            else
            {
                qtc::dependencyClear( & sNode->depInfo );
            }

            //---------------------------------------------------------------
            // [ORDER BY���������� Column]
            // ORDER BY ������ ��Ÿ���� Column �� ���� Column�� �ƴ� ��찡
            // �����Ѵ�.
            // ������ ���� ���� ���� ���� ����.
            //    SELECT i1 a1, i1 + 1 a2, sum(i1) a3, count(*) a4 FROM T1
            //        ORDER BY a1, a2, a3, a4;
            // ���� ���� ORDER BY���� column�� ���,
            //     a1 �� ���� Column�̸�,
            //     a2, a3�� arguments�� �����ϹǷ� ���� Column�� �ƴϸ�,
            //     a3, a4�� aggregation �����̱� ������ ���� Column�� �ƴϴ�.
            // ����, a2, a3, a4�� ���� ���� column module�� execute��
            // �����ؼ��� �ȵȴ�.
            //---------------------------------------------------------------


            // PROJ-1075
            // array ������ �����ϴ� column�� ���
            // ex) V1[1].I1
            // V1�� table, column�� mtcExecute->info������ �����Ѵ�.

            if( ( sNode->lflag & QTC_NODE_PROC_VAR_ESTIMATE_MASK ) ==
                QTC_NODE_PROC_VAR_ESTIMATE_FALSE )
            {
                // PROJ-2533 array() �� ��� index�� �ʿ��մϴ�.
                IDE_TEST_RAISE( ( sNode->node.arguments == NULL ) &&
                                ( ( (sNode->lflag) & QTC_NODE_SP_ARRAY_INDEX_VAR_MASK ) ==
                                  QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST ),
                                ERR_INVALID_FUNCTION_ARGUMENT );

                if( ( sNode->node.arguments != NULL ) &&
                    ( sArrayVariable != NULL ) )
                {
                    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                                    ERR_INVALID_FUNCTION_ARGUMENT );

                    // typeInfo�� ù��° �÷��� index column��.
                    sModule = sArrayVariable->typeInfo->columns->basicInfo->module;

                    IDE_TEST( mtf::makeConversionNodes( aNode,
                                                        aNode->arguments,
                                                        aTemplate,
                                                        aStack + 1,
                                                        aCallBack,
                                                        &sModule )
                              != IDE_SUCCESS );

                    aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecuteArrayColumn;

                    // arrayVariable�� index column�� ������ type���� argument�� conversion node�� �޾��ش�.
                    // execute�� info�� �ش� array ������ table, column������ �Ѱ��ش�.
                    IDU_FIT_POINT( "qtcColumn::qtcColumnEstimate::alloc::ColumnInfo" );
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF( qtcColumnInfo ),
                                                (void**)&sColumnInfo )
                              != IDE_SUCCESS );

                    sColumnInfo->table = sArrayVariable->common.table;
                    sColumnInfo->column = sArrayVariable->common.column;
                    sColumnInfo->objectId = sArrayVariable->common.objectID;   // PROJ-1073 Package

                    aTemplate->rows[aNode->table].execute[aNode->column].calculateInfo = (void*)sColumnInfo;

                    sNode->lflag &= ~QTC_NODE_PROC_VAR_ESTIMATE_MASK;
                    sNode->lflag |= QTC_NODE_PROC_VAR_ESTIMATE_TRUE;
                }
                else
                {
                    if (sNode->node.arguments == NULL &&
                        QTC_IS_AGGREGATE(sNode) == ID_FALSE)
                    {
                        /* BUG-37981
                           sIdcFlag == ID_FALSE�ε�, sNode->nonde.objectID != 0�� ���� �ִ�.
                           �׷� ���� �̹� estimate�� ���·� �̴�. �׷��Ƿ�, execute���� ������ �Ǿ��ִ�.
                           ����, objectID != 0�̸�, �ƹ��͵� ���ص� �ȴ�. */
                        if( sNode->node.objectID == QS_EMPTY_OID)
                        {
                            aTemplate->rows[aNode->table].execute[aNode->column] = qtcExecute;
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
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // array ������ �����ϴ� column�� ���

            // PROJ-1073 Package
            /* Package spec�� template�� �����´�. */
            IDE_TEST( qsxPkg::getPkgInfo( aNode->objectID,
                                          &sPkgInfo )
                      != IDE_SUCCESS );

            IDE_DASSERT( sPkgInfo != NULL );

            sMtcTmplate = (mtcTemplate *)(sPkgInfo->tmplate);

            IDE_DASSERT( sMtcTmplate != NULL );

            if( ( sNode->lflag & QTC_NODE_PROC_VAR_ESTIMATE_MASK ) ==
                QTC_NODE_PROC_VAR_ESTIMATE_FALSE )
            {
                // PROJ-2533 array() �� ��� index�� �ʿ��մϴ�.
                IDE_TEST_RAISE( ( sNode->node.arguments == NULL ) &&
                                ( ( (sNode->lflag) & QTC_NODE_SP_ARRAY_INDEX_VAR_MASK ) ==
                                  QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST ),
                                ERR_INVALID_FUNCTION_ARGUMENT );

                if( sNode->node.arguments != NULL )
                {
                    IDE_TEST_RAISE( ( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) != 1,
                                    ERR_INVALID_FUNCTION_ARGUMENT );

                    // typeInfo�� ù��° �÷��� index column��.
                    sModule = sArrayVariable->typeInfo->columns->basicInfo->module;

                    IDE_TEST( mtf::makeConversionNodes( aNode,
                                                        aNode->arguments,
                                                        aTemplate,
                                                        aStack + 1,
                                                        aCallBack,
                                                        &sModule )
                              != IDE_SUCCESS );

                    IDU_FIT_POINT( "qtcColumn::qtcColumnEstimate::alloc::RealNode1" );
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF( mtcNode ) * 2,
                                                (void **)&sRealNode ) != IDE_SUCCESS );

                    sRealNode[0].table     = aNode->table;
                    sRealNode[0].column    = aNode->column;
                    sRealNode[0].objectID  = aNode->objectID;

                    sRealNode[1].table     = sArrayVariable->common.table;
                    sRealNode[1].column    = sArrayVariable->common.column;
                    sRealNode[1].objectID  = sArrayVariable->common.objectID;

                    sRealColumn  = *QTC_TMPL_COLUMN ( (qcTemplate*)sMtcTmplate,
                                                      (qtcNode*)sRealNode );

                    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(sStatement),
                                               (qtcNode *)aNode,
                                               sStatement,
                                               QC_SHARED_TMPLATE(sStatement),
                                               MTC_TUPLE_TYPE_INTERMEDIATE,
                                               1 )
                              != IDE_SUCCESS );

                    // PROJ-1073 Package
                    if( aNode->orgNode != NULL )
                    {
                        aNode->orgNode->table = aNode->table;
                        aNode->orgNode->column = aNode->column;
                        aNode->orgNode = NULL;
                    }

                    *QTC_STMT_EXECUTE( sStatement, (qtcNode*)aNode )
                                                 = qtcExecuteIndirectArrayColumn;
                    QTC_STMT_EXECUTE( sStatement, (qtcNode*)aNode )->calculateInfo
                                                 = (void*)sRealNode;
                    *QTC_STMT_COLUMN ( sStatement, (qtcNode*)aNode ) 
                                                 = sRealColumn;

                    sNode->lflag &= ~QTC_NODE_PROC_VAR_ESTIMATE_MASK;
                    sNode->lflag |= QTC_NODE_PROC_VAR_ESTIMATE_TRUE;
                }
                else
                {
                    IDU_FIT_POINT( "qtcColumn::qtcColumnEstimate::alloc::ReadNode2" );
                    IDE_TEST( aCallBack->alloc( aCallBack->info,
                                                ID_SIZEOF( mtcNode ),
                                                (void **)&sRealNode ) != IDE_SUCCESS );

                    sRealNode->table     = aNode->table;
                    sRealNode->column    = aNode->column;
                    sRealNode->objectID  = aNode->objectID;

                    sRealColumn  = *QTC_TMPL_COLUMN ( (qcTemplate*)sMtcTmplate,
                                                      (qtcNode*)sRealNode );

                    IDE_TEST( qtc::nextColumn( QC_QMP_MEM(sStatement),
                                               (qtcNode *)aNode,
                                               sStatement,
                                               QC_SHARED_TMPLATE(sStatement),
                                               MTC_TUPLE_TYPE_INTERMEDIATE,
                                               1 )
                              != IDE_SUCCESS );

                    // PROJ-1073 Package
                    if( aNode->orgNode != NULL )
                    {
                        aNode->orgNode->table = aNode->table;
                        aNode->orgNode->column = aNode->column;
                        aNode->orgNode = NULL;
                    }

                    *QTC_STMT_EXECUTE( sStatement, (qtcNode*)aNode )
                                                 = qtcExecuteIndirectColumn;
                    QTC_STMT_EXECUTE( sStatement, (qtcNode*)aNode )->calculateInfo
                                                 = (void*)sRealNode;
                    /* BUG-39340
                       UDType�� ������ ���, ������ ����� package�� QMP_MEM���� �Ҵ�޾� �����ȴ�.
                       ����, �ش纯���� ����� package�� recompile �� ���, memory�� free�ȴ�.
                       �׷��� ������ �� QMP_MEM���� �Ҵ�޾Ƽ� ������ �����ؾ��Ѵ�.  */
                    if( ( sRealColumn.module->id >= MTD_UDT_ID_MIN ) &&
                        ( sRealColumn.module->id <= MTD_UDT_ID_MAX ) )
                    {
                        sRealType = ((qtcModule *)(sRealColumn.module))->typeInfo;

                        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(sStatement),
                                                qsTypes,
                                                &sType )
                                  != IDE_SUCCESS );
                        idlOS::memcpy( sType , sRealType, ID_SIZEOF(qsTypes) );
                        sType->columns = NULL;

                        IDE_TEST( qcm::copyQcmColumns( QC_QMP_MEM(sStatement),
                                                       sRealType->columns,
                                                       &sType->columns,
                                                       sRealType->columnCount )
                                  != IDE_SUCCESS );

                        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(sStatement),
                                                qtcModule,
                                                &sType->typeModule )
                                  != IDE_SUCCESS );

                        idlOS::memcpy(sType->typeModule,
                                      sRealColumn.module,
                                      ID_SIZEOF(mtdModule) );

                        sType->typeModule->typeInfo = sType;

                        mtc::copyColumn( &sColumn, &sRealColumn );

                        sColumn.module = (mtdModule *)sType->typeModule;
                    }
                    else
                    {
                        mtc::copyColumn( &sColumn, &sRealColumn );
                    }
                    *QTC_STMT_COLUMN ( sStatement, (qtcNode*)aNode )
                                                 = sColumn;

                    sNode->lflag &= ~QTC_NODE_PROC_VAR_ESTIMATE_MASK;
                    sNode->lflag |= QTC_NODE_PROC_VAR_ESTIMATE_TRUE;
                }
            }
            else
            {
                // Nothing to do.
            }

            // PROJ-1362
            sNode->lflag &= ~QTC_NODE_BINARY_MASK;

            if ( qtc::isEquiValidType( sNode,
                                       &(QC_SHARED_TMPLATE(sStatement)->tmplate) )
                 == ID_FALSE )
            {
                sNode->lflag |= QTC_NODE_BINARY_EXIST;
            }
            else
            {
                // Nothing to do.
            }
        }

        aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
    }
    else if ( sNode->node.module == & qtc::subqueryModule )
    {
        //--------------------------------------------------------
        // BUG-25839
        // order by������ ������ target alias�� subquery�� ���
        // column������ stack�� �ö���� �ʾ� subquery�� target������
        // ���� stack�� �ø���.
        //--------------------------------------------------------

        sConvertedNode = mtf::convertedNode( sNode->node.arguments,
                                             & sCallBackInfo->tmplate->tmplate );

        aStack->column = aTemplate->rows[sConvertedNode->table].columns
            + sConvertedNode->column;

        //--------------------------------------------------------
        // target alias�� subquery�� ��� expression�� ���� conversion
        // node�� �����Ǵ� ��찡 �����Ƿ� assign node�� �����Ͽ�
        // ���������� �״�� �����ϸ鼭 conversion node�� ������ ��
        // �ְ� �Ѵ�.
        //--------------------------------------------------------

        // ���ο� subquery node ����
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(sStatement),
                                qtcNode,
                                & sSubQNode )
                  != IDE_SUCCESS);

        // sNode�� �״�� ����
        idlOS::memcpy( sSubQNode, sNode, ID_SIZEOF(qtcNode) );

        // sNode�� assign node�� �����Ѵ�.
        IDE_TEST( qtc::makeAssign( sStatement,
                                   sNode,
                                   sSubQNode )
                  != IDE_SUCCESS );
    }
    else
    {
        aStack->column = aTemplate->rows[aNode->table].columns
            + aNode->column;
    }

    // PROJ-2394
    // view�� target column���� list type�� �� �� �ִ�.
    // list type�� ��� stack�� value�� �ʿ��Ͽ� smiColumn.value�� ����صξ���.
    if ( aStack->column->module == &mtdList )
    {
        aStack->value = aStack->column->column.value;
    }
    else
    {
        // Nothing to do.
    }

    // BUG-41311
    if ( ( sNode->lflag & QTC_NODE_LOOP_VALUE_MASK ) ==
         QTC_NODE_LOOP_VALUE_EXIST )
    {
        *aStack = sSFWGH->thisQuerySet->loopStack;
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-42639 Monitoring query */
    if ( ( ( sNode->lflag & QTC_NODE_PROC_FUNCTION_MASK )
           == QTC_NODE_PROC_FUNCTION_TRUE ) ||
         ( ( sNode->lflag & QTC_NODE_SEQUENCE_MASK )
           == QTC_NODE_SEQUENCE_EXIST ) )
    {
        QC_SHARED_TMPLATE(sStatement)->flag &= ~QC_TMP_ALL_FIXED_TABLE_MASK;
        QC_SHARED_TMPLATE(sStatement)->flag |= QC_TMP_ALL_FIXED_TABLE_FALSE;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_FUNCTION_ARGUMENT );
    IDE_SET(ideSetErrorCode(mtERR_ABORT_INVALID_FUNCTION_ARGUMENT));

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qtcCalculate_Column(
    mtcNode*     aNode,
    mtcStack*    aStack,
    SInt         aRemain,
    void*,
    mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *
 *    Column�� ������ �����Ѵ�.
 *
 * Implementation :
 *
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_Column"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;

    /* PROJ-2180
       qp ������ mtc::value �� ����Ѵ�.
       �ٸ� ���⿡���� ������ ���ؼ� valueForModule �� ����Ѵ�. */
    aStack->value  = (void*)mtd::valueForModule(
                                        (smiColumn*)aStack->column,
                                        aTemplate->rows[aNode->table].row,
                                        MTD_OFFSET_USE,
                                        aStack->column->module->staticNull );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_ArrayColumn(
    mtcNode*     aNode,
    mtcStack*    aStack,
    SInt         aRemain,
    void*        aInfo,
    mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1075
 *    Array������ index������ ������ �� Column ������ �����Ѵ�.
 *
 * Implementation :
 *    search������ �����Ѵ�. ���� lvalue�̸� search-insert�� �����Ѵ�.
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_ArrayColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qsxArrayInfo  * sArrayInfo;
    mtcColumn     * sArrayColumn;
    qtcColumnInfo * sColumnInfo;
    idBool          sFound;
    qtcNode       * sNode = (qtcNode *)aNode;

    //fix PROJ-1596
    mtcTemplate   * sTmplate   = NULL;

    sColumnInfo = (qtcColumnInfo*)aInfo;

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    // argument�� ������ �����Ѵ�.
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        // Value Error
        IDE_RAISE( ERR_ARGUMENT_NOT_APPLICABLE );
    }
    else
    {
        sTmplate = aTemplate;

        // array������ column, value�� ���´�.
        sArrayColumn = sTmplate->rows[sColumnInfo->table].columns + sColumnInfo->column;

        sArrayInfo = *((qsxArrayInfo **)( (UChar*) sTmplate->rows[sColumnInfo->table].row
                                          + sArrayColumn->column.offset ));

        IDE_TEST_RAISE( sArrayInfo == NULL, ERR_INVALID_ARRAY );

        if( ( sNode->lflag & QTC_NODE_LVALUE_MASK ) == QTC_NODE_LVALUE_ENABLE )
        {
            // lvalue�̹Ƿ� search-insert�� �ϱ� ���� ID_TRUE�� �ѱ�
            IDE_TEST( qsxArray::searchKey( ((qcTemplate*)aTemplate)->stmt,
                                           sArrayInfo,
                                           aStack[1].column,
                                           aStack[1].value,
                                           ID_TRUE,
                                           &sFound )
                      != IDE_SUCCESS );
        }
        else
        {
            // rvalue�̹Ƿ� search�� �ϱ� ���� ID_FALSE�� �ѱ�
            IDE_TEST( qsxArray::searchKey( ((qcTemplate*)aTemplate)->stmt,
                                           sArrayInfo,
                                           aStack[1].column,
                                           aStack[1].value,
                                           ID_FALSE,
                                           &sFound )
                      != IDE_SUCCESS );
        }

        IDE_TEST_RAISE( sFound == ID_FALSE, ERR_NO_DATA_FOUND );

        aStack->column = aTemplate->rows[aNode->table].columns + aNode->column;
        aStack->value  = (void*)mtc::value( aStack->column,
                                            sArrayInfo->row,
                                            MTD_OFFSET_USE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DATA_FOUND );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_NO_DATA_FOUND));
    }
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_IndirectColumn(
    mtcNode*     aNode,
    mtcStack*    aStack,
    SInt         aRemain,
    void*        /* aInfo */,
    mtcTemplate* aTemplate )
{
#define IDE_FN "IDE_RC qtcCalculate_IndirectColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcTemplate * sTmplate   = NULL;
    void        * sFakeValue;
    mtcColumn   * sFakeColumn;
    qcStatement * sStatement = ((qcTemplate *)aTemplate)->stmt;
    mtcNode     * sRealNode;

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    sFakeColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sFakeValue  = (void *)mtc::value( sFakeColumn,
                                      aTemplate->rows[aNode->table].row,
                                      MTD_OFFSET_USE );

    sRealNode = (mtcNode *) QTC_TUPLE_EXECUTE( (mtcTuple *)(aTemplate->rows+aNode->table),
                                               (qtcNode *)aNode )->calculateInfo;

    // PROJ-1073 Package
    /* Package spec�� template�� �����´�. */
    IDE_TEST( qcuSessionPkg::getTmplate( sStatement,
                                         sRealNode->objectID,
                                         aStack,
                                         aRemain,
                                         &sTmplate )
              != IDE_SUCCESS );

    // package�� variable �ʱ�ȭ ��ų �� �̸� null�� �����ϴ�
    IDE_TEST( sTmplate == NULL );

    aStack->column = sTmplate->rows[sRealNode->table].columns + sRealNode->column;
    aStack->value  = (void *)mtc::value( aStack->column,
                                         sTmplate->rows[sRealNode->table].row,
                                         MTD_OFFSET_USE );

    idlOS::memcpy( sFakeValue,
                   aStack->value,
                   sFakeColumn->column.size );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_STACK_OVERFLOW )
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qtcCalculate_IndirectArrayColumn(
    mtcNode*     aNode,
    mtcStack*    aStack,
    SInt         aRemain,
    void*        aInfo,
    mtcTemplate* aTemplate )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-1075
 *    Array������ index������ ������ �� Column ������ �����Ѵ�.
 *
 * Implementation :
 *    search������ �����Ѵ�. ���� lvalue�̸� search-insert�� �����Ѵ�.
 *    Stack�� column������ Value ������ Setting�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "IDE_RC qtcCalculate_IndirectArrayColumn"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qsxArrayInfo  * sArrayInfo;
    mtcColumn     * sArrayColumn;
    idBool          sFound;
    qtcNode       * sNode      = (qtcNode *)aNode;
    mtcNode       * sRealNode  = NULL;

    mtcTemplate   * sTmplate   = NULL;
    qcStatement   * sStatement = ((qcTemplate *)aTemplate)->stmt;
    void          * sFakeValue;
    mtcColumn     * sFakeColumn;

    //sColumnInfo = (qtcColumnInfo*)aInfo;
    sRealNode = (mtcNode *)aInfo;

    IDE_TEST_RAISE( aRemain < 1, ERR_STACK_OVERFLOW );

    sFakeColumn = aTemplate->rows[aNode->table].columns + aNode->column;
    sFakeValue  = (void*)mtc::value( sFakeColumn,
                                     aTemplate->rows[aNode->table].row,
                                     MTD_OFFSET_USE );

    // argument�� ������ �����Ѵ�.
    IDE_TEST( mtf::postfixCalculate( aNode,
                                     aStack,
                                     aRemain,
                                     aInfo,
                                     aTemplate )
              != IDE_SUCCESS );

    if( aStack[1].column->module->isNull( aStack[1].column,
                                          aStack[1].value ) == ID_TRUE )
    {
        // Value Error
        IDE_RAISE( ERR_ARGUMENT_NOT_APPLICABLE );
    }
    else
    {
        // PROJ-1073 Package
        IDE_TEST( qcuSessionPkg::getTmplate( sStatement,
                                             sRealNode->objectID,
                                             aStack,
                                             aRemain,
                                             &sTmplate )
                  != IDE_SUCCESS );

        // array������ column, value�� ���´�.
        sArrayColumn = sTmplate->rows[sRealNode[1].table].columns + sRealNode[1].column;

        sArrayInfo = *((qsxArrayInfo **)( (UChar*) sTmplate->rows[sRealNode[1].table].row
                                          + sArrayColumn->column.offset ));

        IDE_TEST_RAISE( sArrayInfo == NULL, ERR_INVALID_ARRAY );

        if( ( sNode->lflag & QTC_NODE_LVALUE_MASK ) == QTC_NODE_LVALUE_ENABLE )
        {
            // lvalue�̹Ƿ� search-insert�� �ϱ� ���� ID_TRUE�� �ѱ�
            IDE_TEST( qsxArray::searchKey( ((qcTemplate*)aTemplate)->stmt,
                                           sArrayInfo,
                                           aStack[1].column,
                                           aStack[1].value,
                                           ID_TRUE,
                                           &sFound )
                      != IDE_SUCCESS );
        }
        else
        {
            // rvalue�̹Ƿ� search�� �ϱ� ���� ID_FALSE�� �ѱ�
            IDE_TEST( qsxArray::searchKey(((qcTemplate*)aTemplate)->stmt,
                                           sArrayInfo,
                                           aStack[1].column,
                                           aStack[1].value,
                                           ID_FALSE,
                                           &sFound )
                      != IDE_SUCCESS );
        }

        IDE_TEST_RAISE( sFound == ID_FALSE, ERR_NO_DATA_FOUND );

        aStack->column = sTmplate->rows[sRealNode[0].table].columns + sRealNode[0].column;
        aStack->value  = (void*)mtc::value( aStack->column,
                                            sArrayInfo->row,
                                            MTD_OFFSET_USE );

        idlOS::memcpy( sFakeValue,
                       aStack->value,
                       sFakeColumn->column.size );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DATA_FOUND );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_NO_DATA_FOUND));
    }
    IDE_EXCEPTION( ERR_ARGUMENT_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_ARGUMENT_NOT_APPLICABLE));
    }
    IDE_EXCEPTION( ERR_STACK_OVERFLOW );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_STACK_OVERFLOW));
    }
    IDE_EXCEPTION( ERR_INVALID_ARRAY );
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QSX_UNINITIALIZED_ARRAY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

