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
 * $Id: qmoTransitivity.cpp 23857 2007-10-23 02:36:53Z sungminee $
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <qmoPredicate.h>
#include <qmoTransitivity.h>
#include <qcg.h>

extern mtfModule mtfEqual;
extern mtfModule mtfNotEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;
extern mtfModule mtfLike;
extern mtfModule mtfNotLike;
extern mtfModule mtfEqualAny;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfList;

// 12���� �����ڸ� �����Ѵ�.
const qmoTransOperatorModule qmoTransMgr::operatorModule[] = {
    { & mtfEqual       , 0           }, // 0
    { & mtfNotEqual    , 1           }, // 1
    { & mtfLessThan    , 4           }, // 2
    { & mtfLessEqual   , 5           }, // 3
    { & mtfGreaterThan , 2           }, // 4
    { & mtfGreaterEqual, 3           }, // 5
    { & mtfBetween     , ID_UINT_MAX }, // 6
    { & mtfNotBetween  , ID_UINT_MAX }, // 7
    { & mtfLike        , ID_UINT_MAX }, // 8
    { & mtfNotLike     , ID_UINT_MAX }, // 9
    { & mtfEqualAny    , ID_UINT_MAX }, // 10
    { & mtfNotEqualAll , ID_UINT_MAX }, // 11
    { NULL             , ID_UINT_MAX }  // 12
};

// ID_UINT_MAX�� ����ϸ� �бⰡ ����� ��� ª�� �ܾ�� define�Ѵ�.
#define XX     ((UInt)0xFFFFFFFF)  // ID_UINT_MAX

/*
 * DOC-31095 Transitive Predicate Generation.odt p52
 * <transitive operation matrix>
 *
 * BUG-29444: not equal�� between �����ڿ� ���� �����Ǵ�
 *            transitive predicate�� ����
 *
 * a <> b AND b BETWEEN x and y �� ���
 * a NOT BETWEEN x and y �� �߰��ǵ��� �Ǿ��־��µ� �̴� �߸��� predicate ��
 *
 * �̿� ���Ҿ� a <> b �� ��� between, like, in � ��� �������� �ʴ´�
 * ���� matrix �ι�° ���� ù��° ���� ���� ��� XX �� ����
 */
const UInt qmoTransMgr::operationMatrix[QMO_OPERATION_MATRIX_SIZE][QMO_OPERATION_MATRIX_SIZE] = {
    {   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11 },
    {   1, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {   2, XX,  2,  2, XX, XX, XX, XX, XX, XX, XX, XX },
    {   3, XX,  2,  3, XX, XX, XX, XX, XX, XX, XX, XX },
    {   4, XX, XX, XX,  4,  4, XX, XX, XX, XX, XX, XX },
    {   5, XX, XX, XX,  4,  5, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX },
    {  XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX, XX }
};

#undef XX


IDE_RC
qmoTransMgr::init( qcStatement        * aStatement,
                   qmoTransPredicate ** aTransPredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qmoTransPredicate ����ü�� �����ϰ� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/
    
    qmoTransPredicate * sTransPredicate;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransPredicate != NULL );

    //------------------------------------------
    // qmoTransPredicate ���� �� �ʱ�ȭ
    //------------------------------------------
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransPredicate),
                                             (void **) & sTransPredicate )
              != IDE_SUCCESS );

    sTransPredicate->operatorList  = NULL;
    sTransPredicate->operandList = NULL;
    sTransPredicate->operandSet  = NULL;
    sTransPredicate->operandSetSize = 0;
    sTransPredicate->operandSetArray = NULL;
    sTransPredicate->genOperatorList = NULL;
    qtc::dependencyClear( &sTransPredicate->joinDepInfo );

    *aTransPredicate = sTransPredicate;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::addBasePredicate( qcStatement       * aStatement,
                               qmsQuerySet       * aQuerySet,
                               qmoTransPredicate * aTransPredicate,
                               qtcNode           * aNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qtcNode������ predicate���� operator list�� operand list��
 *     �����Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoTransMgr::addBasePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aNode != NULL );
    
    //------------------------------------------
    // operator list, operand list ����
    //------------------------------------------

    IDE_TEST( createOperatorAndOperandList( aStatement,
                                            aQuerySet,
                                            aTransPredicate,
                                            aNode,
                                            ID_FALSE )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::addBasePredicate( qcStatement       * aStatement,
                               qmsQuerySet       * aQuerySet,
                               qmoTransPredicate * aTransPredicate,
                               qmoPredicate      * aPredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qmoPredicate������ predicate���� operator list�� operand list��
 *     �����Ѵ�.
 *
 ***********************************************************************/
    
    qmoPredicate      * sPredicate;
    qtcNode           * sNode;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::addBasePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aPredicate != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sPredicate = aPredicate;
    sNode = aPredicate->node;
    
    //------------------------------------------
    // operator list�� operand list ����
    //------------------------------------------

    while ( sPredicate != NULL )
    {
        IDE_TEST( createOperatorAndOperandList( aStatement,
                                                aQuerySet,
                                                aTransPredicate,
                                                sNode,
                                                ID_TRUE )
                  != IDE_SUCCESS );

        sPredicate = sPredicate->more;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::processPredicate( qcStatement        * aStatement,
                               qmsQuerySet        * aQuerySet,
                               qmoTransPredicate  * aTransPredicate,
                               qmoTransMatrix    ** aTransMatrix,
                               idBool             * aIsApplicable )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     operator list�� operand list�� transitive predicate matrix��
 *     �����Ͽ� ����Ѵ�.
 *
 ***********************************************************************/

    qmoTransPredicate * sTransPredicate;
    qmoTransMatrix    * sTransMatrix = NULL;
    idBool              sIsApplicable;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::processPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aTransMatrix != NULL );
    IDE_DASSERT( aIsApplicable != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTransPredicate = aTransPredicate;

    //------------------------------------------
    // operand set ����
    //------------------------------------------

    IDE_TEST( createOperandSet( aStatement,
                                sTransPredicate,
                                & sIsApplicable )
              != IDE_SUCCESS );

    if ( sIsApplicable == ID_TRUE )
    {
        //------------------------------------------
        // qmoTransMatrix ���� �� �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransMatrix),
                                                 (void **) & sTransMatrix )
                  != IDE_SUCCESS );

        sTransMatrix->predicate = sTransPredicate;

        //------------------------------------------
        // transitive predicate matrix ����
        //------------------------------------------

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoTransMatrixInfo* ) * sTransPredicate->operandSetSize,
                                                   (void**)& sTransMatrix->matrix )
                  != IDE_SUCCESS);

        for (i = 0;
             i < sTransPredicate->operandSetSize;
             i++)
        {
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoTransMatrixInfo )
                                                       * sTransPredicate->operandSetSize,
                                                       (void**)&( sTransMatrix->matrix[i] ) )
                      != IDE_SUCCESS );
        }

        sTransMatrix->size = sTransPredicate->operandSetSize;

        //------------------------------------------
        // transitive predicate matrix �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( initializeTransitiveMatrix( sTransMatrix )
                  != IDE_SUCCESS );

        //------------------------------------------
        // transitive predicate matrix ���
        //------------------------------------------

        IDE_TEST( calculateTransitiveMatrix( aStatement,
                                             aQuerySet,
                                             sTransMatrix )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    *aTransMatrix = sTransMatrix;
    *aIsApplicable = sIsApplicable;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::generatePredicate( qcStatement     * aStatement,
                                qmsQuerySet     * aQuerySet,
                                qmoTransMatrix  * aTransMatrix,
                                qtcNode        ** aTransitiveNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     transitive predicate matrix�� transitive predicate�� �����Ѵ�.
 *
 ***********************************************************************/
    
    qtcNode  * sTransitiveNode;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::generatePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransMatrix != NULL );

    //------------------------------------------
    // transitive predicate ����
    //------------------------------------------

    IDE_TEST( generateTransitivePredicate( aStatement,
                                           aQuerySet,
                                           aTransMatrix,
                                           & sTransitiveNode )
              != IDE_SUCCESS );

    *aTransitiveNode = sTransitiveNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::linkPredicate( qtcNode      * aTransitiveNode,
                            qtcNode     ** aNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qtcNode������ predicate�� qtcNode���·� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode  * sNode;
    mtcNode  * sMtcNode;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::linkPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aNode != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sNode = *aNode;
    
    //------------------------------------------
    // ����
    //------------------------------------------

    if ( aTransitiveNode != NULL )
    {
        if ( sNode == NULL )
        {
            *aNode = aTransitiveNode;
        }
        else
        {
            sMtcNode = & sNode->node;
            
            while ( sMtcNode != NULL )
            {
                if ( sMtcNode->next == NULL )
                {
                    sMtcNode->next = (mtcNode*) aTransitiveNode;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
                
                sMtcNode = sMtcNode->next;
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;
}
    
IDE_RC
qmoTransMgr::linkPredicate( qcStatement   * aStatement,
                            qtcNode       * aTransitiveNode,
                            qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qtcNode������ predicate�� qmoPredicate���·� �����Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate  * sTransPredicate;
    qmoPredicate  * sNewPredicate;
    qtcNode       * sNode;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::linkPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTransPredicate = *aPredicate;
    sNode = aTransitiveNode;

    //------------------------------------------
    // ����
    //------------------------------------------

    while ( sNode != NULL )
    {
        // qmoPredicate�� �����Ѵ�.
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            & sNewPredicate )
                  != IDE_SUCCESS );

        // qmoPredicate�� �����Ѵ�.
        sNewPredicate->next = sTransPredicate;
        sTransPredicate = sNewPredicate;

        sNode = (qtcNode*) sNode->node.next;
    }

    *aPredicate = sTransPredicate;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::linkPredicate( qmoPredicate  * aTransitivePred,
                            qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qmoPredicate������ predicate�� qmoPredicate���� �����Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::linkPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sPredicate = *aPredicate;
    
    //------------------------------------------
    // ����
    //------------------------------------------

    if ( aTransitivePred != NULL )
    {
        if ( sPredicate == NULL )
        {
            *aPredicate = aTransitivePred;
        }
        else
        {
            while ( sPredicate != NULL )
            {
                if ( sPredicate->next == NULL )
                {
                    sPredicate->next = aTransitivePred;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
                
                sPredicate = sPredicate->next;
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;
}

IDE_RC
qmoTransMgr::createOperatorAndOperandList( qcStatement       * aStatement,
                                           qmsQuerySet       * aQuerySet,
                                           qmoTransPredicate * aTransPredicate,
                                           qtcNode           * aNode,
                                           idBool              aOnlyOneNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     predicate��� operator list�� operand list�� �����Ѵ�.
 *
 ***********************************************************************/
    
    qtcNode     * sNode;
    qtcNode     * sCompareNode;
    idBool        sCheckNext;
    idBool        sIsTransitiveForm;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::createOperatorAndOperandList::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransPredicate != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNode = aNode;

    //------------------------------------------
    // Operator List, Operand List ����
    //------------------------------------------
    
    while ( sNode != NULL )
    {
        if ( (sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK)
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            // CNF ������ ���
            sCompareNode = (qtcNode*) sNode->node.arguments;
            sCheckNext = ID_TRUE;
        }
        else
        {
            // DNF ������ ���
            sCompareNode = sNode;
            sCheckNext = ID_FALSE;
        }

        IDE_TEST( isTransitiveForm( aStatement,
                                    aQuerySet,
                                    sCompareNode,
                                    sCheckNext,
                                    & sIsTransitiveForm )
                  != IDE_SUCCESS );

        if ( sIsTransitiveForm == ID_TRUE )
        {
            // qmoTransOperator ����
            IDE_TEST( createOperatorAndOperand( aStatement,
                                                aTransPredicate,
                                                sCompareNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( aOnlyOneNode == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }

        sNode = (qtcNode*)sNode->node.next;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::createOperatorAndOperand( qcStatement       * aStatement,
                                       qmoTransPredicate * aTransPredicate,
                                       qtcNode           * aCompareNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     �ϳ��� predicate�� �ϳ��� operator ���� �� ���� operand ����
 *     ������ �����Ѵ�.
 *
 ***********************************************************************/

    qmoTransOperator  * sOperator;
    qmoTransOperand   * sLeft;
    qmoTransOperand   * sRight;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::createOperatorAndOperand::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT(
        ( (aCompareNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) >= 2 ) &&
        ( (aCompareNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) <= 3 ) );

    //------------------------------------------
    // qmoTransOperand ����
    //------------------------------------------

    //----------------
    // left
    //----------------
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransOperand),
                                             (void **) & sLeft )
              != IDE_SUCCESS );

    sLeft->operandFirst = (qtcNode*) aCompareNode->node.arguments;
    sLeft->operandSecond = NULL;    
    sLeft->id = ID_UINT_MAX;   // ���� �������� �ʾ���
    sLeft->next = NULL;
    
    IDE_DASSERT( sLeft->operandFirst != NULL );
    
    IDE_TEST( isIndexColumn( aStatement,
                             sLeft->operandFirst,
                             & sLeft->isIndexColumn )
              != IDE_SUCCESS );
    
    qtc::dependencySetWithDep( & sLeft->depInfo,
                               & sLeft->operandFirst->depInfo );
    
    //----------------
    // right
    //----------------
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransOperand),
                                             (void **) & sRight )
              != IDE_SUCCESS );

    if ( (aCompareNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) == 2 )
    {
        sRight->operandFirst = (qtcNode*) aCompareNode->node.arguments->next;
        sRight->operandSecond = NULL;
        sRight->id = ID_UINT_MAX;
        sRight->next = NULL;
        
        IDE_DASSERT( sRight->operandFirst != NULL );
        
        IDE_TEST( isIndexColumn( aStatement,
                                 sRight->operandFirst,
                                 & sRight->isIndexColumn )
                  != IDE_SUCCESS );
    
        qtc::dependencySetWithDep( & sRight->depInfo,
                                   & sRight->operandFirst->depInfo );
    }
    else
    {
        // like, between
        sRight->operandFirst = (qtcNode*) aCompareNode->node.arguments->next;
        sRight->operandSecond = (qtcNode*) aCompareNode->node.arguments->next->next;
        sRight->id = ID_UINT_MAX;
        sRight->isIndexColumn = ID_FALSE;
        sRight->next = NULL;

        IDE_DASSERT( sRight->operandFirst != NULL );
        IDE_DASSERT( sRight->operandSecond != NULL );
        
        IDE_TEST( qtc::dependencyOr( & sRight->operandFirst->depInfo,
                                     & sRight->operandSecond->depInfo,
                                     & sRight->depInfo )
                  != IDE_SUCCESS );
    }
    
    //------------------------------------------
    // qmoTransOperator ����
    //------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransOperator),
                                             (void **) & sOperator )
              != IDE_SUCCESS );

    sOperator->left = sLeft;
    sOperator->right = sRight;
    sOperator->id = ID_UINT_MAX;
    sOperator->next = NULL;

    for (i = 0; operatorModule[i].module != NULL; i++)
    {
        if ( aCompareNode->node.module == operatorModule[i].module )
        {
            sOperator->id = i;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_DASSERT( sOperator->id != ID_UINT_MAX );
    
    //------------------------------------------
    // qmoTransPredicate�� ����
    //------------------------------------------

    // operand list�� �����Ѵ�.
    sRight->next = aTransPredicate->operandList;
    sLeft->next = sRight;
    aTransPredicate->operandList = sLeft;
    
    // operator list�� �����Ѵ�.
    sOperator->next = aTransPredicate->operatorList;
    aTransPredicate->operatorList = sOperator;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::isIndexColumn( qcStatement * aStatement,
                            qtcNode     * aNode,
                            idBool      * aIsIndexColumn )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     ��尡 index�� ���� �÷�������� �˻��Ѵ�.
 *     (composite index�� ��� ù��° �÷��� �����ϴ�.)
 *
 ***********************************************************************/

    idBool      sIndexColumn = ID_FALSE;
    qmsFrom   * sFrom;
    qcmIndex  * sIndices;
    UInt        sIndexCnt;
    UInt        sColumnId;
    UInt        sColumnOrder;
    UInt        i;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isIndexColumn::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
        
    //------------------------------------------
    // index column ���� �˻�
    //------------------------------------------

    if ( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE )
    {
        sFrom = QC_SHARED_TMPLATE(aStatement)->tableMap[aNode->node.table].from;
        sIndices = sFrom->tableRef->tableInfo->indices;
        sIndexCnt = sFrom->tableRef->tableInfo->indexCount;

        for (i = 0; i < sIndexCnt; i++)
        {
            if ( sIndices[i].keyColCount > 0 )
            {
                sColumnId = sIndices[i].keyColumns[0].column.id;
                sColumnOrder = sColumnId & SMI_COLUMN_ID_MASK;

                if ( (UInt)aNode->node.column == sColumnOrder )
                {
                    sIndexColumn = ID_TRUE;
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

    *aIsIndexColumn = sIndexColumn;
    
    return IDE_SUCCESS;
}

IDE_RC
qmoTransMgr::isSameCharType( qcStatement * aStatement,
                             qtcNode     * aLeftNode,
                             qtcNode     * aRightNode,
                             idBool      * aIsSameType )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     ���� type�� �÷��� ���� type���� �˻��Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sLeftNode;
    qtcNode   * sRightNode;
    mtcColumn * sLeftColumn;
    mtcColumn * sRightColumn;
    idBool      sIsSameType = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isSameCharType::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftNode != NULL );
    IDE_DASSERT( aRightNode != NULL );
    IDE_DASSERT( aIsSameType != NULL );

    //------------------------------------------
    // �÷� Ȥ�� �÷� expr�� ���� type���� �˻�
    //------------------------------------------

    if ( ( qtc::dependencyEqual( & aLeftNode->depInfo,
                                 & qtc::zeroDependencies ) == ID_FALSE ) &&
         ( qtc::dependencyEqual( & aRightNode->depInfo,
                                 & qtc::zeroDependencies ) == ID_FALSE ) )
    {
        // column operator column ������ ���

        // type�� ������ ���

        if ( QTC_IS_LIST( aLeftNode ) != ID_TRUE )
        {
            // left�� column list�� �ƴ� ���

            if ( QTC_IS_LIST( aRightNode ) != ID_TRUE )
            {
                // right�� column list�� �ƴ� ���

                sLeftColumn = QTC_STMT_COLUMN( aStatement, aLeftNode );
                sRightColumn = QTC_STMT_COLUMN( aStatement, aRightNode );

                if ( ( (sLeftColumn->module->flag & MTD_GROUP_MASK)
                       == MTD_GROUP_TEXT ) &&
                     ( (sRightColumn->module->flag & MTD_GROUP_MASK)
                       == MTD_GROUP_TEXT ) )
                {
                    if ( sLeftColumn->module->id != sRightColumn->module->id )
                    {
                        sIsSameType = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // BUG-36457
                    // ���ڿ� ���� type���� transitive predicate�� �����ؼ��� �ȵȴ�.
                    if ( ( ( (sLeftColumn->module->flag & MTD_GROUP_MASK)
                             == MTD_GROUP_NUMBER ) &&
                           ( (sRightColumn->module->flag & MTD_GROUP_MASK)
                             == MTD_GROUP_TEXT ) )
                         ||
                         ( ( (sLeftColumn->module->flag & MTD_GROUP_MASK)
                             == MTD_GROUP_TEXT ) &&
                           ( (sRightColumn->module->flag & MTD_GROUP_MASK)
                             == MTD_GROUP_NUMBER ) ) )
                    {
                        sIsSameType = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                // right�� column list�� ���
                // ex) i1 in (i2,i3,1)

                sRightNode = (qtcNode*) aRightNode->node.arguments;

                while ( sRightNode != NULL )
                {
                    IDE_TEST( isSameCharType( aStatement,
                                              aLeftNode,
                                              sRightNode,
                                              & sIsSameType )
                              != IDE_SUCCESS );

                    if ( sIsSameType == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        sRightNode = (qtcNode*) sRightNode->node.next;
                    }
                }                
            }
        }
        else
        {
            // left�� column list�� ���

            if ( QTC_IS_LIST( aRightNode ) != ID_TRUE )
            {
                // right�� column list�� �ƴ� ���

                // estimate�� ���̹Ƿ� �̷� ���� ����.
                IDE_DASSERT( 0 );
            }
            else
            {
                // right�� column list�� ���

                sLeftNode = (qtcNode*) aLeftNode->node.arguments;
                sRightNode = (qtcNode*) aRightNode->node.arguments;

                if (QTC_IS_LIST(sRightNode) != ID_TRUE)
                {
                    // ex) (i1,i2) = (i3,i4)

                    while ( sLeftNode != NULL )
                    {
                        IDE_TEST( isSameCharType( aStatement,
                                                  sLeftNode,
                                                  sRightNode,
                                                  & sIsSameType )
                                  != IDE_SUCCESS );

                        if ( sIsSameType == ID_FALSE )
                        {
                            break;
                        }
                        else
                        {
                            sLeftNode = (qtcNode*) sLeftNode->node.next;
                            sRightNode = (qtcNode*) sRightNode->node.next;
                        }
                    }
                }
                else
                {
                    // ex) (i1,i2) in ((i3,i4), (i5,i6))

                    while (sRightNode != NULL)
                    {
                        IDE_TEST( isSameCharType(aStatement,
                                                 aLeftNode,
                                                 sRightNode,
                                                 &sIsSameType )
                                  != IDE_SUCCESS);

                        if (sIsSameType == ID_FALSE)
                        {
                            break;
                        }
                        else
                        {
                            sRightNode = (qtcNode*)sRightNode->node.next;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // column�� value�� �񱳿����� ������ type�� �޶� ����.

        // Nothing to do.
    }

    *aIsSameType = sIsSameType;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::isTransitiveForm( qcStatement * aStatement,
                               qmsQuerySet * aQuerySet,
                               qtcNode     * aCompareNode,
                               idBool        sCheckNext,
                               idBool      * aIsTransitiveForm )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation
 *     transitive predicate�� ������ base predicate�� ã�´�.
 *
 *     12���� �����ڸ� �����ϸ�
 *         =, !=, <, <=, >, >=, (not) between, (not) like, (not) in
 *
 *     ������ ���� format�� �����Ѵ�.
 *         1. column operator value
 *         2. column operator column
 *         3. column between value
 *         4. column like value
 *         5. column in value
 *         6. 1~5�� OR ���� AND�θ� ����Ǿ�� �Ѵ�.
 *
 * Implementation :
 *     (1) OR ���� AND�θ� ����Ǿ�� �Ѵ�.
 *     (2) constant Ȥ�� outer column expression�� �������� �ʴ´�.
 *     (3) PSM function�� �������� �ʾƾ� �Ѵ�.
 *     (4) variable build-in function�� �������� �ʾƾ� �Ѵ�.
 *     (5) subquery�� �������� �ʾƾ� �Ѵ�.
 *     (6) rownum�� �������� �ʾƾ� �Ѵ�.
 *     (7) �����ϴ� 12���� ������ �߿� �ϳ����� �Ѵ�.
 *     (8) between, like, in�� �����ʿ��� ������� �;� �Ѵ�.
 *     (9) �ǹ̾��� predicate�� �߰����� �ʴ´�.
 *         (��, i1 = i1)
 *     (10) ���� type�� �÷����� �񱳴� ���� type�� �����ϴ�.
 *         (��, 'char'i1 = 'varchar'i2 �ȵ�)
 *
 ***********************************************************************/

    qtcNode   * sArgNode0;
    qtcNode   * sArgNode1;
    qtcNode   * sArgNode2;
    qcDepInfo   sDepInfo;
    idBool      sIsTransitiveForm = ID_TRUE;
    idBool      sIsEquivalent = ID_FALSE;
    idBool      sIsSameType = ID_FALSE;
    idBool      sFound = ID_FALSE;
    UInt        sModuleId;
    UInt        i;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isTransitiveForm::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aIsTransitiveForm != NULL );

    //------------------------------------------
    // ���밡���� predicate ã��
    //------------------------------------------

    //------------------------------------------
    // 1. CNF�� ��� OR ������ �ϳ��� �����ڸ� �־�� �Ѵ�.
    //------------------------------------------
    
    if ( (sCheckNext == ID_TRUE) &&
         (aCompareNode->node.next != NULL) )
    {
        sIsTransitiveForm = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // 2. ���(value operator value)�̰ų� �ܺ� �÷� expression��
    //    �������� �ʴ´�.
    //------------------------------------------

    if ( sIsTransitiveForm == ID_TRUE )
    {
        qtc::dependencyAnd( & aCompareNode->depInfo,
                            & aQuerySet->depInfo,
                            & sDepInfo );
        
        if( qtc::dependencyEqual( & sDepInfo,
                                  & qtc::zeroDependencies ) == ID_TRUE )
        {
            sIsTransitiveForm = ID_FALSE;
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

    //------------------------------------------
    // 3. PSM function�� �������� �ʾƾ� �Ѵ�.
    //------------------------------------------
    
    if ( (sIsTransitiveForm == ID_TRUE) &&
         ((aCompareNode->lflag & QTC_NODE_PROC_FUNCTION_MASK)
          == QTC_NODE_PROC_FUNCTION_TRUE) )
    {
        sIsTransitiveForm = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // 4. variable build-in function�� �������� �ʾƾ� �Ѵ�.
    //------------------------------------------
    
    if ( (sIsTransitiveForm == ID_TRUE) &&
         ((aCompareNode->lflag & QTC_NODE_VAR_FUNCTION_MASK)
          == QTC_NODE_VAR_FUNCTION_EXIST) )
    {
        sIsTransitiveForm = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // 5. subquery�� �������� �ʾƾ� �Ѵ�.
    //------------------------------------------
    
    if ( (sIsTransitiveForm == ID_TRUE) &&
         ((aCompareNode->lflag & QTC_NODE_SUBQUERY_MASK)
          == QTC_NODE_SUBQUERY_EXIST) )
    {
        sIsTransitiveForm = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // 6. rownum�� �������� �ʾƾ� �Ѵ�.
    //------------------------------------------
    
    if ( (sIsTransitiveForm == ID_TRUE) &&
         ((aCompareNode->lflag & QTC_NODE_ROWNUM_MASK)
          == QTC_NODE_ROWNUM_EXIST) )
    {
        sIsTransitiveForm = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // 7. �����ϴ� 12���� ������ �߿� �ϳ����� �Ѵ�.
    //------------------------------------------
    
    if ( sIsTransitiveForm == ID_TRUE )
    {
        for (i = 0; operatorModule[i].module != NULL; i++)
        {
            if ( aCompareNode->node.module == operatorModule[i].module )
            {
                if ( ( ( aCompareNode->lflag & QTC_NODE_JOIN_TYPE_MASK ) == QTC_NODE_JOIN_TYPE_ANTI ) ||
                     ( ( ( aCompareNode->lflag & QTC_NODE_JOIN_TYPE_MASK ) == QTC_NODE_JOIN_TYPE_SEMI ) &&
                       ( QCU_OPTIMIZER_SEMI_JOIN_TRANSITIVITY_DISABLE == 1 ) ) )
                {
                    // ������ ��쿡 transitive predicate�� �����ϸ� �ȵȴ�.
                    // 1) PROJ-1718 Anti join�� ���
                    // 2) BUG-43421 Semi join�̰� Property __OPTIMIZER_SEMI_JOIN_TRANSITIVITY_DISABLE�� 1�� ���
                }
                else
                {
                    sModuleId = i;
                    sFound = ID_TRUE;
                    break;
                }
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sFound == ID_TRUE )
        {
            IDE_DASSERT(
                ( (aCompareNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) >= 2 ) &&
                ( (aCompareNode->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) <= 3 ) );
            
            // �񱳿������� ���� ����
            sArgNode0 = (qtcNode*) aCompareNode->node.arguments;
    
            // ������ ù��° ���ڿ� �ι�° ����
            sArgNode1 = (qtcNode*) sArgNode0->node.next;
            sArgNode2 = (qtcNode*) sArgNode1->node.next;
            
            //------------------------------------------
            // 8. between, like, in�� �����ʿ��� ������� �;� �Ѵ�.
            //------------------------------------------
            
            if ( operatorModule[sModuleId].transposeModuleId == ID_UINT_MAX )
            {
                if ( qtc::dependencyEqual( & sArgNode1->depInfo,
                                           & qtc::zeroDependencies ) == ID_TRUE )
                {
                    // Nothing to do.
                }
                else
                {
                    sIsTransitiveForm = ID_FALSE;
                    IDE_CONT(NORMAL_EXIT);
                }
                
                if ( sArgNode2 != NULL )
                {
                    if ( qtc::dependencyEqual( & sArgNode2->depInfo,
                                               & qtc::zeroDependencies ) == ID_TRUE )
                    {
                        // Nothing to do.
                    }
                    else
                    {
                        sIsTransitiveForm = ID_FALSE;
                        IDE_CONT(NORMAL_EXIT);
                    }
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

            if ( sArgNode2 == NULL )
            {
                //------------------------------------------
                // 9. �ǹ̾��� predicate�� �߰����� �ʴ´�.
                //------------------------------------------
                
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sArgNode0,
                                                       sArgNode1,
                                                       & sIsEquivalent )
                          != IDE_SUCCESS );
                
                if ( sIsEquivalent == ID_TRUE )
                {
                    sIsTransitiveForm = ID_FALSE;
                    IDE_CONT(NORMAL_EXIT);
                }
                else
                {
                    // Nothing to do.
                }
                
                //------------------------------------------
                // 10. ���� type�� �÷����� �񱳴� ���� type�� �����ϴ�.
                //------------------------------------------

                IDE_TEST( isSameCharType( aStatement,
                                          sArgNode0,
                                          sArgNode1,
                                          & sIsSameType )
                          != IDE_SUCCESS );

                if ( sIsSameType == ID_FALSE )
                {
                    sIsTransitiveForm = ID_FALSE;
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
        else
        {
            sIsTransitiveForm = ID_FALSE;
        }
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT(NORMAL_EXIT);

    *aIsTransitiveForm = sIsTransitiveForm;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
    
IDE_RC
qmoTransMgr::createOperandSet( qcStatement       * aStatement,
                               qmoTransPredicate * aTransPredicate,
                               idBool            * aIsApplicable )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation
 *     operand list�� operand set�� �����Ѵ�.
 *     (operand set�� �������� operand list�� ���̻� ������ �ʴ´�.)
 *
 * Implementation :
 *     (1) operand list�� ���� �ߺ��� �˻��ϰ� �ߺ��� ���
 *         (1.1) ���� ��带 �����ϰ� �ִ� operator list�� �����Ѵ�.
 *         (1.2) ���� ���� operand set�� �������� �ʴ´�.
 *     (2) �ߺ����� �ʴ� ���
 *         (2.1) ���� ���� operand set�� �����ϰ� id�� �Ҵ��Ѵ�.
 *     (3) ������ operand set�� id�� ���� ������ �� �ֵ���
 *         operand array�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoTransOperand   * sFirst;
    qmoTransOperand   * sSecond;
    qmoTransOperand   * sOperand;
    qmoTransOperand  ** sColSetArray;
    idBool              sIsEquivalent;
    idBool              sIsDuplicate;
    UInt                sDupCount = 0;
    UInt                sOperandId = 0;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::createOperandSet::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aIsApplicable != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sFirst = aTransPredicate->operandList;

    //------------------------------------------
    // Column Set ����
    //------------------------------------------

    while ( sFirst != NULL )
    {
        sSecond = sFirst->next;
        sIsDuplicate = ID_FALSE;

        while ( sSecond != NULL )
        {
            if ( ( sFirst->operandSecond == NULL ) &&
                 ( sSecond->operandSecond == NULL ) )
            {
                // �ι�° ���ڰ� ���� column�̳� value�� ��
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sFirst->operandFirst,
                                                       sSecond->operandFirst,
                                                       & sIsEquivalent )
                          != IDE_SUCCESS );

                if ( sIsEquivalent == ID_TRUE )
                {
                    // �����带 �����ϴ� operator�� ���ڸ� ����
                    IDE_TEST( changeOperand( aTransPredicate,
                                             sFirst,
                                             sSecond )
                              != IDE_SUCCESS );

                    sIsDuplicate = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                if ( ( sFirst->operandSecond != NULL ) &&
                     ( sSecond->operandSecond != NULL ) )
                {
                    // �ι�° ���ڰ� �ִ� value�� ��
                    // (between x and y, like w escape z)
                    IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                           sFirst->operandFirst,
                                                           sSecond->operandFirst,
                                                           & sIsEquivalent )
                              != IDE_SUCCESS );

                    if ( sIsEquivalent == ID_TRUE )
                    {
                        IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                               sFirst->operandSecond,
                                                               sSecond->operandSecond,
                                                               & sIsEquivalent )
                                  != IDE_SUCCESS );

                        if ( sIsEquivalent == ID_TRUE )
                        {
                            // �����带 �����ϴ� operator�� ���ڸ� ����
                            IDE_TEST( changeOperand( aTransPredicate,
                                                     sFirst,
                                                     sSecond )
                                      != IDE_SUCCESS );

                            sIsDuplicate = ID_TRUE;
                            break;
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
                else
                {
                    // Nothing to do.
                }
            }

            sSecond = sSecond->next;
        }

        if ( sIsDuplicate == ID_TRUE )
        {
            // operand�� �ߺ��� ���
            sDupCount++;
        }
        else
        {
            // operand�� �ߺ����� ���� ��� operand set�� ����
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransOperand),
                                                     (void **) & sOperand )
                      != IDE_SUCCESS );

            idlOS::memcpy( sOperand, sFirst, ID_SIZEOF(qmoTransOperand) );

            // ������ operand set�� �����ϵ��� operator�� ���ڸ� ����
            IDE_TEST( changeOperand( aTransPredicate,
                                     sFirst,
                                     sOperand )
                      != IDE_SUCCESS );

            // operand id �Ҵ�
            sOperand->id = sOperandId;
            sOperandId++;

            // operand set ����
            sOperand->next = aTransPredicate->operandSet;
            aTransPredicate->operandSet = sOperand;
            aTransPredicate->operandSetSize = sOperandId;
        }

        sFirst = sFirst->next;
    }

    // operand set�� id�� ���� ������ �� �ֵ��� operand array ����
    if ( sOperandId > 0 )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoTransOperand* ) * sOperandId,
                                                   (void **) & sColSetArray )
                  != IDE_SUCCESS );

        sOperand = aTransPredicate->operandSet;

        while ( sOperand != NULL )
        {
            IDE_DASSERT( sOperand->id < sOperandId );

            sColSetArray[sOperand->id] = sOperand;

            sOperand = sOperand->next;
        }

        aTransPredicate->operandSetArray = sColSetArray;
    }
    else
    {
        // Nothing to do.
    }

    if ( sDupCount > 0 )
    {
        *aIsApplicable = ID_TRUE;
    }
    else
    {
        // �ߺ��� operand�� ���ٸ� transitive predicate�� �������� �ʴ´�.
        *aIsApplicable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::changeOperand( qmoTransPredicate * aTransPredicate,
                            qmoTransOperand   * aFrom,
                            qmoTransOperand   * aTo )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     aFrom�� �����ϴ� operator list�� aTo�� �ٲ۴�.
 *
 ***********************************************************************/

    qmoTransOperator  * sOperator;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::changeOperand::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aTo != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sOperator = aTransPredicate->operatorList;

    //------------------------------------------
    // operand ����
    //------------------------------------------

    while ( sOperator != NULL )
    {
        if ( sOperator->left == aFrom )
        {
            sOperator->left = aTo;
        }
        else
        {
            // Nothing to do.
        }

        if ( sOperator->right == aFrom )
        {
            sOperator->right = aTo;
        }
        else
        {
            // Nothing to do.
        }

        sOperator = sOperator->next;
    }
    
    return IDE_SUCCESS;
}

IDE_RC
qmoTransMgr::initializeTransitiveMatrix( qmoTransMatrix * aTransMatrix )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     transitive predicate matrix�� �����ϰ� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    qmoTransOperator    *sOperator;
    qmoTransMatrixInfo **sTransMatrix;
    UInt                 sSize;
    UInt                 i;
    UInt                 j;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::initializeTransitiveMatrix::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aTransMatrix != NULL );
    IDE_DASSERT( aTransMatrix->predicate != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTransMatrix = aTransMatrix->matrix;
    sSize = aTransMatrix->size;
    
    sOperator = aTransMatrix->predicate->operatorList;

    //------------------------------------------
    // matrix �ʱ�ȭ
    //------------------------------------------
    
    for (i = 0; i < sSize; i++)
    {
        for (j = 0; j < sSize; j++)
        {
            sTransMatrix[i][j].mOperator = ID_UINT_MAX;
            sTransMatrix[i][j].mIsNewAdd = ID_FALSE;
        }
    }

    //------------------------------------------
    // predicate �ʱ�ȭ
    //------------------------------------------

    while ( sOperator != NULL )
    {
        i = sOperator->left->id;
        j = sOperator->right->id;

        IDE_DASSERT( i != j );
        
        sTransMatrix[i][j].mOperator = sOperator->id;

        // �¿캯�� �ٲپ������� ������ �߰�
        if ( operatorModule[sOperator->id].transposeModuleId != ID_UINT_MAX )
        {
            sTransMatrix[j][i].mOperator = operatorModule[sOperator->id].transposeModuleId;
        }
        else
        {
            // Nothing to do.
        }

        sOperator = sOperator->next;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoTransMgr::calculateTransitiveMatrix( qcStatement    * aStatement,
                                        qmsQuerySet    * aQuerySet,
                                        qmoTransMatrix * aTransMatrix )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     transitive predicate matrix�� ����Ѵ�.
 *
 ***********************************************************************/

    qmoTransPredicate   *sTransPredicate;
    qmoTransMatrixInfo **sTransMatrix;
    idBool               sIsBad;
    UInt                 sNewOperator;
    UInt                 sSize;
    UInt                 i;
    UInt                 j;
    UInt                 k;
    
    IDU_FIT_POINT_FATAL( "qmoTransMgr::calculateTransitiveMatrix::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransMatrix != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTransPredicate = aTransMatrix->predicate;
    sTransMatrix    = aTransMatrix->matrix;
    sSize           = aTransMatrix->size;

    //------------------------------------------
    // transitive predicate matrix ���
    //------------------------------------------

    /*
     * BUG-29646: transitive predicate �� �Ϻ��ϰ� ������ ���մϴ�
     * 
     * i -> j, j -> k ���谡 ���� ��, i -> k ���踦 �����Ѵ�
     * �̶�, �߰� �Ű��� �Ǵ� j �� loop �ֻ����� �ε��� �Ѵ� (Floyd-Warshall)
     *
     * transitive closure�� ���ϱ� ���� �߰��� bad �� �������� �ϴ� �����ϰ�
     * ���߿� bad �� �ƴѰ͵鿡 ���ؼ��� trasitive predicate�� �����Ѵ�
     */

    for (j = 0; j < sSize; j++)
    {
        for (i = 0; i < sSize; i++)
        {
            if (sTransMatrix[i][j].mOperator != ID_UINT_MAX)
            {
                for (k = 0; k < sSize; k++)
                {
                    if (sTransMatrix[j][k].mOperator != ID_UINT_MAX)
                    {
                        sNewOperator = operationMatrix[sTransMatrix[i][j].mOperator][sTransMatrix[j][k].mOperator];

                        /*
                         * matrix �� �ϴ� � ������ ä������
                         * �ٽ� ä���� �ʴ´�
                         */
                        if ((sNewOperator != ID_UINT_MAX) &&
                            (sTransMatrix[i][k].mOperator == ID_UINT_MAX))
                        {
                            sTransMatrix[i][k].mOperator = sNewOperator;

                            if (i != k)
                            {
                                IDE_TEST(isBadTransitivePredicate(aStatement,
                                                                  aQuerySet,
                                                                  sTransPredicate,
                                                                  i,
                                                                  j,
                                                                  k,
                                                                  &sIsBad)
                                         != IDE_SUCCESS);

                                if (sIsBad == ID_FALSE)
                                {
                                    sTransMatrix[i][k].mIsNewAdd = ID_TRUE;
                                }
                                else
                                {
                                    /* Nothing to do. */
                                }
                            }
                            else
                            {
                                /* i == k */
                            }
                        }
                        else
                        {
                            /* Nothing to do. */
                        }
                    }
                }
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }

    for (i = 0; i < sSize; i++)
    {
        for (j = 0; j < sSize; j++)
        {
            if (sTransMatrix[i][j].mIsNewAdd == ID_TRUE)
            {
                IDE_TEST(createNewOperator(aStatement,
                                           sTransPredicate,
                                           i,
                                           j,
                                           sTransMatrix[i][j].mOperator)
                         != IDE_SUCCESS);
            }
            else
            {
                sTransMatrix[i][j].mOperator = ID_UINT_MAX;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::isBadTransitivePredicate( qcStatement       * aStatement,
                                       qmsQuerySet       * aQuerySet,
                                       qmoTransPredicate * aTransPredicate,
                                       UInt                aOperandId1,
                                       UInt                aOperandId2,
                                       UInt                aOperandId3,
                                       idBool            * aIsBad )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation
 *     ������ transitive predicate�� bad transitive predicate����
 *     �Ǵ��Ѵ�. (bad�� �ƴ϶�� �ؼ� �׻� good�� �ƴϴ�.)
 *
 * Implementation :
 *
 *     DOC-31095 Transitive Predicate Generation.odt p54
 *     <Transitive Predicate ���� rule> (��ȣ������ �ٲ���)
 *
 *        |  D1   D2   D3  |  D1   D3  |
 *    ----+----------------+-----------+---------------
 *      1 |  0    a    0   |   0    0  | (X)
 *    ----+----------------+-----------+---------------
 *      2 |  0    a    a   |   0    a  | (depCount=1, index)
 *      3 |  0    a    b   |   0    b  | (depCount=1, no index)
 *      4 |  a    a    0   |   a    0  | (depCount=1, index)
 *      5 |  a    b    0   |   a    0  | (depCount=1, no index)
 *    ----+----------------+-----------+---------------
 *      6 |  a    0    a   |   a    a  | (X)
 *      7 |  a    a    a   |   a    a  | (X)
 *      8 |  a    b    a   |   a    a  | (depCount=1)
 *    ----+----------------+-----------+---------------
 *      9 |  a    0    b   |   a    b  | (X)
 *     10 |  a    a    b   |   a    b  | (X)
 *     11 |  a    b    b   |   a    b  | (X)
 *     12 |  a    c    b   |   a    b  | (*)
 *
 *   (*) non-joinable join pred�� joinable join pred�� ������ ���
 *
 ***********************************************************************/

    qmoTransOperand  * sOperand1;
    qmoTransOperand  * sOperand2;
    qmoTransOperand  * sOperand3;
    qcDepInfo        * sDep1;
    qcDepInfo        * sDep2;
    qcDepInfo        * sDep3;
    qcDepInfo          sAndDepInfo;
    qcDepInfo          sDepInfo;
    qcDepInfo          sDepTmp1;
    qcDepInfo          sDepTmp2;
    qcDepInfo          sDepTmp3;
    idBool             sIsBad = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isBadTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransPredicate != NULL );
    IDE_DASSERT( aIsBad != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sOperand1 = aTransPredicate->operandSetArray[aOperandId1];
    sOperand2 = aTransPredicate->operandSetArray[aOperandId2];
    sOperand3 = aTransPredicate->operandSetArray[aOperandId3];

    sDep1 = & sOperand1->depInfo;
    sDep2 = & sOperand2->depInfo;
    sDep3 = & sOperand3->depInfo;    

    IDE_TEST( qtc::dependencyOr( sDep1,
                                 sDep3,
                                 & sDepInfo )
              != IDE_SUCCESS );

    /* BUG-42134 Created transitivity predicate of join predicate must be
     * reinforced.
     * OLD Rule�� ���õǾ��ٸ� joinDepInfo�� clear�� ����ó�� �����ϵ��� �Ѵ�.
     */
    if ( QCG_GET_SESSION_OPTIMIZER_TRANSITIVITY_OLD_RULE( aStatement ) == 1 )
    {
        qtc::dependencyClear( &aTransPredicate->joinDepInfo );
    }
    else
    {
        /* Nothing to do */
    }
    //------------------------------------------
    // outer column expression�� �������� �ʴ´�.
    //------------------------------------------
    
    qtc::dependencyAnd( & sDepInfo,
                        & aQuerySet->depInfo,
                        & sAndDepInfo );
    
    if( qtc::dependencyEqual( & sAndDepInfo,
                              & qtc::zeroDependencies ) == ID_TRUE )
    {
        sIsBad = ID_TRUE;
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // bad transitive predicate �˻�
    //------------------------------------------

    if ( qtc::getCountBitSet( & sDepInfo ) == 0 )
    {
        //--------------------
        // case 1
        //--------------------
        
        sIsBad = ID_TRUE;
    }
    else
    {
        if ( (qtc::getCountBitSet( sDep1 ) == 0) ||
             (qtc::getCountBitSet( sDep3 ) == 0) )
        {
            //--------------------
            // case 2,3,4,5
            //--------------------
            
            if ( qtc::getCountBitSet( & sDepInfo ) != 1 )
            {
                sIsBad = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            // case 2,3,4,5, depCount=1
            if ( qtc::getCountBitSet( sDep1 ) == 0 )
            {
                if ( qtc::dependencyEqual( sDep2, sDep3 ) == ID_TRUE )
                {
                    // case 2
                    if ( sOperand3->isIndexColumn == ID_FALSE )
                    {
                        sIsBad = ID_TRUE;
                    }
                    else
                    {
                        // Nothing to do.

                        // case 2, depCount=1, index
                    }
                }
                else
                {
                    // case 3, depCount=1
                    if ( sOperand3->isIndexColumn == ID_TRUE )
                    {
                        /* BUG-42134
                         * Index�� ���� ��� Join�� ���õǼ� dependency��
                         * �ִٸ� bad ó���� ���� �ʴ´�
                         */
                        if ( aTransPredicate->joinDepInfo.depCount > 0 )
                        {
                            if ( ( qtc::dependencyContains( &aTransPredicate->joinDepInfo,
                                                            sDep2 ) == ID_TRUE ) ||
                                 ( qtc::dependencyContains( &aTransPredicate->joinDepInfo,
                                                            sDep3 ) == ID_TRUE ) )

                            {
                                sIsBad = ID_FALSE;
                            }
                            else
                            {
                                sIsBad = ID_TRUE;
                            }
                        }
                        else
                        {
                            sIsBad = ID_TRUE;
                        }
                    }
                    else
                    {
                        // Nothing to do.

                        // case 3, depCount=1, no index
                    }
                }
            }
            else
            {
                if ( qtc::dependencyEqual( sDep1, sDep2 ) == ID_TRUE )
                {
                    // case 4
                    if ( sOperand1->isIndexColumn == ID_FALSE )
                    {
                        sIsBad = ID_TRUE;
                    }
                    else
                    {
                        // Nothing to do.

                        // case 4, depCount=1, index
                    }
                }
                else
                {
                    // case 5, depCount=1
                    if ( sOperand1->isIndexColumn == ID_TRUE )
                    {
                        /* BUG-42134
                         * Index�� ���� ��� Join�� ���õǼ� dependency��
                         * �ִٸ� bad ó���� ���� �ʴ´�
                         */
                        if ( aTransPredicate->joinDepInfo.depCount > 0 )
                        {
                            if ( ( qtc::dependencyContains( &aTransPredicate->joinDepInfo,
                                                            sDep1 ) == ID_TRUE ) ||
                                 ( qtc::dependencyContains( &aTransPredicate->joinDepInfo,
                                                            sDep3 ) == ID_TRUE ) )
                            {
                                sIsBad = ID_FALSE;
                            }
                            else
                            {
                                sIsBad = ID_TRUE;
                            }
                        }
                        else
                        {
                            sIsBad = ID_TRUE;
                        }
                    }
                    else
                    {
                        // Nothing to do.

                        // case 5, depCount=1, no index
                    }
                }
            }
        }
        else
        {
            if ( qtc::dependencyEqual( sDep1, sDep3 ) == ID_TRUE )
            {
                //--------------------
                // case 6,7,8
                //--------------------

                if ( (qtc::getCountBitSet( sDep2 ) == 0) ||
                     (qtc::dependencyEqual( sDep1, sDep2 ) == ID_TRUE) )
                {
                    // case 6,7
                    sIsBad = ID_TRUE;
                }
                else
                {
                    // case 8
                    if ( qtc::getCountBitSet( & sDepInfo ) != 1 )
                    {
                        sIsBad = ID_TRUE;
                    }
                    else
                    {
                        // Nothing to do.

                        // case 8, depCount=1
                    }
                }
            }
            else
            {
                //--------------------
                // case 9,10,11,12
                //--------------------
                
                if ( (qtc::getCountBitSet( sDep2 ) == 0) ||
                     (qtc::dependencyEqual( sDep1, sDep2 ) == ID_TRUE) ||
                     (qtc::dependencyEqual( sDep2, sDep3 ) == ID_TRUE) )
                {
                    // case 9,10,11
                    sIsBad = ID_TRUE;
                }
                else
                {
                    // case 12
                    qtc::dependencyAnd( sDep1, sDep2, & sDepTmp1 );
                    qtc::dependencyAnd( sDep2, sDep3, & sDepTmp2 );
                    qtc::dependencyAnd( sDep1, sDep3, & sDepTmp3 );
                    
                    if ( (qtc::getCountBitSet( & sDepTmp1 ) > 0) &&
                         (qtc::getCountBitSet( & sDepTmp2 ) > 0) &&
                         (qtc::getCountBitSet( & sDepTmp3 ) == 0) )
                    {
                        // case 12
                        // {a,c}, {c,b} non-joinable join predicate
                        // {a,b} joinable join predicate
                        // non-joinable join pred�� joinable join pred�� �����Ѵ�.
                    }
                    else
                    {
                        sIsBad = ID_TRUE;
                    }
                }
            }
        }
    }

    *aIsBad = sIsBad;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::createNewOperator( qcStatement       * aStatement,
                                qmoTransPredicate * aTransPredicate,
                                UInt                aLeftId,
                                UInt                aRightId,
                                UInt                aNewOperatorId )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     qmoTransOperator�� �����ϰ� aTransPredicate�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoTransOperator * sOperator;
    qmoTransOperand  * sLeft;
    qmoTransOperand  * sRight;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::createNewOperator::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransPredicate != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sLeft = aTransPredicate->operandSetArray[aLeftId];
    sRight = aTransPredicate->operandSetArray[aRightId];
    
    //------------------------------------------
    // qmoTransOperator ����
    //------------------------------------------
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTransOperator),
                                             (void **) & sOperator )
              != IDE_SUCCESS );

    sOperator->left = sLeft;
    sOperator->right = sRight;
    sOperator->id = aNewOperatorId;

    // genOperatorList�� �����Ѵ�.
    sOperator->next = aTransPredicate->genOperatorList;
    aTransPredicate->genOperatorList = sOperator;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::generateTransitivePredicate( qcStatement     * aStatement,
                                          qmsQuerySet     * aQuerySet,
                                          qmoTransMatrix  * aTransMatrix,
                                          qtcNode        ** aTransitiveNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     transitive predicate�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoTransPredicate  * sTransPredicate;
    qmoTransOperator   * sOperator;
    qtcNode            * sTransitivePredicate;
    qtcNode            * sTransitiveNode = NULL;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::generateTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransMatrix != NULL );
    IDE_DASSERT( aTransitiveNode != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTransPredicate = aTransMatrix->predicate;
    sOperator = sTransPredicate->genOperatorList;

    //------------------------------------------
    // transitive predicate ����
    //------------------------------------------

    while ( sOperator != NULL )
    {
        if ( (sOperator->left->id < sOperator->right->id) ||
             (operatorModule[sOperator->id].transposeModuleId == ID_UINT_MAX) )
        {
            // matrix�� L�ʰ� transpose���� ���� �����ڿ� ���ؼ��� �����Ѵ�.
            IDE_TEST( createTransitivePredicate( aStatement,
                                                 aQuerySet,
                                                 sOperator,
                                                 & sTransitivePredicate )
                      != IDE_SUCCESS );
        
            // �����Ѵ�.
            sTransitivePredicate->node.next = (mtcNode*) sTransitiveNode;
            sTransitiveNode = sTransitivePredicate;
        }
        else
        {
            // �������� �ߺ��� predicate�� �������� �ʴ´�.
            
            // Nothing to do.
        }

        sOperator = sOperator->next;
    }
    
    *aTransitiveNode = sTransitiveNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::createTransitivePredicate( qcStatement        * aStatement,
                                        qmsQuerySet        * aQuerySet,
                                        qmoTransOperator   * aOperator,
                                        qtcNode           ** aTransitivePredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     �ϳ��� transitive predicate�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoTransOperand  * sLeft;
    qmoTransOperand  * sRight;
    qtcNode          * sLeftArgNode1 = NULL;
    qtcNode          * sRightArgNode1 = NULL;
    qtcNode          * sRightArgNode2 = NULL;
    qtcNode          * sCompareNode[2];
    qtcNode          * sOrNode[2];
    qcNamePosition     sNullPosition;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::createTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aOperator != NULL );
    IDE_DASSERT( aTransitivePredicate != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sLeft = aOperator->left;
    sRight = aOperator->right;
    
    SET_EMPTY_POSITION( sNullPosition );    

    //------------------------------------------
    // left operand ���� ����
    //------------------------------------------

    IDE_DASSERT( sLeft->operandSecond == NULL );
    
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     sLeft->operandFirst,
                                     & sLeftArgNode1,
                                     ID_FALSE, // root�� next�� �������� �ʴ´�.
                                     ID_TRUE,  // conversion�� ���´�.
                                     ID_TRUE,  // constant node���� �����Ѵ�.
                                     ID_TRUE ) // constant node�� �����Ѵ�.
              != IDE_SUCCESS );

    //------------------------------------------
    // right operand ���� ����
    //------------------------------------------
    
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     sRight->operandFirst,
                                     & sRightArgNode1,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_TRUE,
                                     ID_TRUE )
              != IDE_SUCCESS );

    if ( sRight->operandSecond != NULL )
    {
        IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                         sRight->operandSecond,
                                         & sRightArgNode2,
                                         ID_FALSE,
                                         ID_TRUE,
                                         ID_TRUE,
                                         ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    //------------------------------------------
    // transitive predicate ����
    //------------------------------------------

    // ���ο� operation ��带 �����Ѵ�.
    IDE_TEST( qtc::makeNode( aStatement,
                             sCompareNode,
                             & sNullPosition,
                             operatorModule[aOperator->id].module )
              != IDE_SUCCESS );

    // argument�� �����Ѵ�.
    sRightArgNode1->node.next = (mtcNode*) sRightArgNode2;
    sLeftArgNode1->node.next = (mtcNode*) sRightArgNode1;

    if ( sRightArgNode2 == NULL )
    {
        sCompareNode[0]->node.arguments = (mtcNode*) sLeftArgNode1;
        sCompareNode[0]->node.lflag |= 2;
    }
    else
    {
        sCompareNode[0]->node.arguments = (mtcNode*) sLeftArgNode1;
        sCompareNode[0]->node.lflag |= 3;
    }

    // transitive predicate flag�� �����Ѵ�.
    sCompareNode[0]->lflag &= ~QTC_NODE_TRANS_PRED_MASK;
    sCompareNode[0]->lflag |= QTC_NODE_TRANS_PRED_EXIST;

    // ���ο� OR ��带 �ϳ� �����Ѵ�.
    IDE_TEST( qtc::makeNode( aStatement,
                             sOrNode,
                             &sNullPosition,
                             (const UChar*)"OR",
                             2 )
              != IDE_SUCCESS );
    
    // sCompareNode�� �����Ѵ�.
    sOrNode[0]->node.arguments = (mtcNode*) sCompareNode[0];
    sOrNode[0]->node.lflag |= 1;

    aQuerySet->processPhase = QMS_OPTIMIZE_TRANS_PRED;

    // estimate
    IDE_TEST( qtc::estimate( sOrNode[0],
                             QC_SHARED_TMPLATE(aStatement),
                             aStatement,
                             aQuerySet,
                             aQuerySet->SFWGH,
                             NULL )
              != IDE_SUCCESS);

    *aTransitivePredicate = sOrNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmoTransMgr::getJoinDependency( qmsFrom   * aFrom,
                                       qcDepInfo * aDepInfo )
{
    if ( aFrom->joinType != QMS_NO_JOIN )
    {
        if ( aFrom->joinType == QMS_LEFT_OUTER_JOIN )
        {
            if ( aFrom->left->joinType == QMS_NO_JOIN )
            {
                IDE_TEST( qtc::dependencyOr( aDepInfo,
                                             &aFrom->left->depInfo,
                                             aDepInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( getJoinDependency( aFrom->left, aDepInfo )
                  != IDE_SUCCESS );
        IDE_TEST( getJoinDependency( aFrom->right, aDepInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Noting to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::processTransitivePredicate( qcStatement  * aStatement,
                                         qmsQuerySet  * aQuerySet,
                                         qtcNode      * aNode,
                                         idBool         aIsOnCondition,
                                         qtcNode     ** aTransitiveNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate�� ����
 *
 * Implementation :
 *    (1) Predicate Formation
 *        �����ϴ� ������ predicate�� �̾� set�� �����Ѵ�.
 *    (2) Transitive Predicate Matrix
 *        Transitive Closure�� Matrix�� �̿��Ͽ� ����Ѵ�.
 *    (3) Transitive Predicate Generation
 *        Transitive Predicate Rule�� Transitive Predicate�� �����Ѵ�.
 *
 ***********************************************************************/
    
    qmoTransPredicate * sTransitivePredicate;
    qmoTransMatrix    * sTransitiveMatrix;
    qtcNode           * sNode;
    qtcNode           * sTransitiveNode = NULL;
    idBool              sIsApplicable;
    qmsFrom           * sFrom;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::processTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aTransitiveNode != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sNode = aNode;
    
    //------------------------------------------
    // Transitive Predicate Generation
    //------------------------------------------

    if ( sNode != NULL )
    {
        IDE_TEST( init( aStatement,
                        & sTransitivePredicate )
                  != IDE_SUCCESS );

        IDE_TEST( addBasePredicate( aStatement,
                                    aQuerySet,
                                    sTransitivePredicate,
                                    sNode )
                  != IDE_SUCCESS );

        /* BUG-42134 Created transitivity predicate of join predicate must be
         * reinforced.
         */
        if ( aIsOnCondition == ID_FALSE )
        {
            for ( sFrom = aQuerySet->SFWGH->from; sFrom != NULL; sFrom = sFrom->next )
            {
                IDE_TEST( getJoinDependency( sFrom, &sTransitivePredicate->joinDepInfo )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( processPredicate( aStatement,
                                    aQuerySet,
                                    sTransitivePredicate,
                                    & sTransitiveMatrix,
                                    & sIsApplicable )
                  != IDE_SUCCESS );
        
        if ( sIsApplicable == ID_TRUE )
        {
            IDE_TEST( generatePredicate( aStatement,
                                         aQuerySet,
                                         sTransitiveMatrix,
                                         & sTransitiveNode )
                      != IDE_SUCCESS );

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

    *aTransitiveNode = sTransitiveNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::processTransitivePredicate( qcStatement   * aStatement,
                                         qmsQuerySet   * aQuerySet,
                                         qtcNode       * aNode,
                                         qmoPredicate  * aPredicate,
                                         qtcNode      ** aTransitiveNode )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate�� ����
 *
 * Implementation :
 *    (1) Predicate Formation
 *        �����ϴ� ������ predicate�� �̾� set�� �����Ѵ�.
 *    (2) Transitive Predicate Matrix
 *        Transitive Closure�� Matrix�� �̿��Ͽ� ����Ѵ�.
 *    (3) Transitive Predicate Generation
 *        Transitive Predicate Rule�� Transitive Predicate�� �����Ѵ�.
 *
 ***********************************************************************/
    
    qmoTransPredicate * sTransitivePredicate;
    qmoTransMatrix    * sTransitiveMatrix;
    qmoPredicate      * sPredicate;
    qtcNode           * sTransitiveNode = NULL;
    idBool              sIsApplicable;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::processTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTransitiveNode != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sPredicate = aPredicate;
    
    //------------------------------------------
    // Transitive Predicate Generation
    //------------------------------------------

    if ( (sPredicate != NULL) && (aNode != NULL) )
    {
        IDE_TEST( init( aStatement,
                        & sTransitivePredicate )
                  != IDE_SUCCESS );

        IDE_TEST( addBasePredicate( aStatement,
                                    aQuerySet,
                                    sTransitivePredicate,
                                    aNode )
                  != IDE_SUCCESS );

        while ( sPredicate != NULL )
        {
            IDE_TEST( addBasePredicate( aStatement,
                                        aQuerySet,
                                        sTransitivePredicate,
                                        sPredicate )
                      != IDE_SUCCESS );

            sPredicate = sPredicate->next;
        }
        
        IDE_TEST( processPredicate( aStatement,
                                    aQuerySet,
                                    sTransitivePredicate,
                                    & sTransitiveMatrix,
                                    & sIsApplicable )
                  != IDE_SUCCESS );
        
        if ( sIsApplicable == ID_TRUE )
        {
            IDE_TEST( generatePredicate( aStatement,
                                         aQuerySet,
                                         sTransitiveMatrix,
                                         & sTransitiveNode )
                      != IDE_SUCCESS );
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

    *aTransitiveNode = sTransitiveNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTransMgr::isEquivalentPredicate( qcStatement  * aStatement,
                                    qmoPredicate * aPredicate1,
                                    qmoPredicate * aPredicate2,
                                    idBool       * aIsEquivalent )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     �������� ������ predicate���� �˻��Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sCompareNode1;
    qtcNode   * sCompareNode2;
    idBool      sIsEquivalent = ID_FALSE;
    idBool      sFound1 = ID_FALSE;
    idBool      sFound2 = ID_FALSE;
    UInt        sModuleId1  = 0;
    UInt        sModuleId2  = 0;
    UInt        i;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isEquivalentPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate1 != NULL );
    IDE_DASSERT( aPredicate2 != NULL );
    IDE_DASSERT( aIsEquivalent != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sCompareNode1 = aPredicate1->node;

    if ( ( sCompareNode1->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode1 = (qtcNode*) sCompareNode1->node.arguments;
    }
    else
    {
        // Nothing to do.
    }

    sCompareNode2 = aPredicate2->node;

    if ( ( sCompareNode2->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode2 = (qtcNode*) sCompareNode2->node.arguments;
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // �������� ������ predicate���� �˻�
    //------------------------------------------

    // OR��� ������ �ϳ��� �񱳿����ڸ��� �;��ϸ�
    // �������� �����Ϸ��� dependency�� ���� ���̴�.
    if ( ( sCompareNode1->node.next == NULL ) &&
         ( sCompareNode2->node.next == NULL ) &&
         ( qtc::dependencyEqual( & sCompareNode1->depInfo,
                                 & sCompareNode2->depInfo ) == ID_TRUE ) )
    {
        for ( i = 0; operatorModule[i].module != NULL; i++)
        {
            if ( sCompareNode1->node.module == operatorModule[i].module )
            {
                sModuleId1 = i;
                sFound1 = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            if ( sCompareNode2->node.module == operatorModule[i].module )
            {
                sModuleId2 = i;
                sFound2 = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( ( sFound1 == ID_TRUE ) &&
             ( sFound2 == ID_TRUE ) )
        {
            if ( sModuleId1 == sModuleId2 )
            {
                // {i1<1}�� {i1<1}�� ��
                // {i1=1}�� {i1=1}�� ��

                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       (qtcNode*) sCompareNode1->node.arguments,
                                                       (qtcNode*) sCompareNode2->node.arguments,
                                                       & sIsEquivalent )
                          != IDE_SUCCESS );

                if ( sIsEquivalent == ID_TRUE )
                {
                    IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                           (qtcNode*) sCompareNode1->node.arguments->next,
                                                           (qtcNode*) sCompareNode2->node.arguments->next,
                                                           & sIsEquivalent )
                              != IDE_SUCCESS );

                    // ���ڰ� 3���� ���� �¿캯�� �ٲ� �� ����.
                    if ( ( sCompareNode1->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                    {
                        if ( ( sCompareNode2->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) == 3 )
                        {
                            IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                                   (qtcNode*) sCompareNode1->node.arguments->next->next,
                                                                   (qtcNode*) sCompareNode2->node.arguments->next->next,
                                                                   & sIsEquivalent )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // ������ ������ �ٸ��� ���� �ٸ���.

                            sIsEquivalent = ID_FALSE;
                        }
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
            else
            {
                // Nothing to do.
            }

            if ( ( sIsEquivalent == ID_FALSE ) &&
                 ( operatorModule[sModuleId1].transposeModuleId == sModuleId2 ) )
            {
                // {i1<1}�� {1>i1}�� ��
                // {i1=1}�� {1=i1}�� ��

                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       (qtcNode*) sCompareNode1->node.arguments,
                                                       (qtcNode*) sCompareNode2->node.arguments->next,
                                                       & sIsEquivalent )
                          != IDE_SUCCESS );

                if ( sIsEquivalent == ID_TRUE )
                {
                    IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                           (qtcNode*) sCompareNode1->node.arguments->next,
                                                           (qtcNode*) sCompareNode2->node.arguments,
                                                           & sIsEquivalent )
                              != IDE_SUCCESS );
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
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // ���� OR ��尡 �ƴ϶�� �����ʴ�.
        
        // Nothing to do.
    }

    *aIsEquivalent = sIsEquivalent;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
    
IDE_RC
qmoTransMgr::isExistEquivalentPredicate( qcStatement  * aStatement,
                                         qmoPredicate * aPredicate,
                                         qmoPredicate * aPredicateList,
                                         idBool       * aIsExist )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     �������� ������ predicate�� �����ϴ��� �˻��Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;
    qmoPredicate  * sMorePredicate;
    idBool          sIsEquivalent;
    idBool          sIsExist = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoTransMgr::isExistEquivalentPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aIsExist != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sPredicate = aPredicateList;

    //------------------------------------------
    // �������� ������ predicate�� �����ϴ��� �˻�
    //------------------------------------------

    while ( sPredicate != NULL )
    {
        sMorePredicate = sPredicate;

        while ( sMorePredicate != NULL )
        {
            if ( aPredicate != sMorePredicate )
            {
                IDE_TEST( isEquivalentPredicate( aStatement,
                                                 aPredicate,
                                                 sMorePredicate,
                                                 & sIsEquivalent )
                          != IDE_SUCCESS );
                
                if ( sIsEquivalent == ID_TRUE )
                {
                    sIsExist = ID_TRUE;
                    break;
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

            sMorePredicate = sMorePredicate->more;
        }

        if ( sIsExist == ID_TRUE )
        {
            break;
        }
        else
        {
            sPredicate = sPredicate->next;
        }
    }

    *aIsExist = sIsExist;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
