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
 * $Id: qmoTransitivity.h 23857 2007-11-02 02:36:53Z sungminee $
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation�� ���� �ڷ� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_TRANSITIVITY_H_
#define _O_QMO_TRANSITIVITY_H_ 1

#include <qc.h>
#include <qmoDependency.h>

#define QMO_OPERATION_MATRIX_SIZE   (12)

//---------------------------------------------------
// �����ϴ� Operator �迭�� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoTransOperatorModule
{
    mtfModule     * module;
    UInt            transposeModuleId; // ������ ��ȯ���� ���� ������
} qmoTransOperatorModule;

//---------------------------------------------------
// Predicate�� Operand�� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoTransOperand
{
    qtcNode          * operandFirst;
    qtcNode          * operandSecond;   // between, like escape�� ���
    
    UInt               id;         // operand Id (0~n)

    idBool             isIndexColumn;   // index column
    qcDepInfo          depInfo;
    
    qmoTransOperand  * next;
    
} qmoTransOperand;

//---------------------------------------------------
// Predicate�� Operator�� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoTransOperator
{
    qmoTransOperand  * left;
    qmoTransOperand  * right;
    
    UInt               id;         // operator Id

    qmoTransOperator * next;
    
} qmoTransOperator;

//---------------------------------------------------
// Transitive Predicate�� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoTransPredicate
{
    qmoTransOperator  * operatorList;
    qmoTransOperand   * operandList;

    qmoTransOperand   * operandSet;
    UInt                operandSetSize;

    qmoTransOperand  ** operandSetArray;   // operand set array

    qmoTransOperator  * genOperatorList;   // generated operator list
    qcDepInfo           joinDepInfo;
} qmoTransPredicate;

//---------------------------------------------------
// Transitive Predicate Matrix�� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoTransMatrixInfo
{
    UInt   mOperator;
    idBool mIsNewAdd;
} qmoTransMatrixInfo;

typedef struct qmoTransMatrix
{
    qmoTransPredicate  * predicate;  /* predicate */
    qmoTransMatrixInfo** matrix;     /* transitive predicate matrix */
    UInt                 size;       /* matrix (size X size) */
} qmoTransMatrix;


class qmoTransMgr
{
public:

    // Transitve Predicate ���� �Լ�
    static IDE_RC  processTransitivePredicate( qcStatement  * aStatement,
                                               qmsQuerySet  * aQuerySet,
                                               qtcNode      * aNode,
                                               idBool         aIsOnCondition,
                                               qtcNode     ** aTransitiveNode );

    // Transitve Predicate ���� �Լ�
    static IDE_RC  processTransitivePredicate( qcStatement   * aStatement,
                                               qmsQuerySet   * aQuerySet,
                                               qtcNode       * aNode,
                                               qmoPredicate  * aPredicate,
                                               qtcNode      ** aTransitiveNode );

    // qtcNode������ predicate�� qtcNode���·� ����
    static IDE_RC  linkPredicate( qtcNode      * aTransitiveNode,
                                  qtcNode     ** aNode );

    // qtcNode������ predicate�� qmoPredicate���·� ����
    static IDE_RC  linkPredicate( qcStatement   * aStatement,
                                  qtcNode       * aTransitiveNode,
                                  qmoPredicate ** aPredicate );

    // qmoPredicate������ predicate�� qmoPredicate���� ����
    static IDE_RC  linkPredicate( qmoPredicate  * aTransitivePred,
                                  qmoPredicate ** aPredicate );

    // �������� ������ predicate�� �����ϴ��� �˻�
    static IDE_RC  isExistEquivalentPredicate( qcStatement  * aStatement,
                                               qmoPredicate * aPredicate,
                                               qmoPredicate * aPredicateList,
                                               idBool       * aIsExist );
    
private:

    // �����ϴ� 12���� ������
    static const   qmoTransOperatorModule operatorModule[];

    // Transitive Operation Matrix
    static const   UInt operationMatrix[QMO_OPERATION_MATRIX_SIZE][QMO_OPERATION_MATRIX_SIZE];

    // �ʱ�ȭ �Լ�
    static IDE_RC  init( qcStatement        * aStatement,
                         qmoTransPredicate ** aTransPredicate );

    // qtcNode������ predicate���� operator list�� operand list�� ����
    static IDE_RC  addBasePredicate( qcStatement       * aStatement,
                                     qmsQuerySet       * aQuerySet,
                                     qmoTransPredicate * aTransPredicate,
                                     qtcNode           * aNode );

    // qmoPredicate������ predicate���� operator list�� operand list�� ����
    static IDE_RC  addBasePredicate( qcStatement       * aStatement,
                                     qmsQuerySet       * aQuerySet,
                                     qmoTransPredicate * aTransPredicate,
                                     qmoPredicate      * aPredicate );

    // operator list�� operand list�� transitive predicate matrix�� �����ϰ� ���
    static IDE_RC  processPredicate( qcStatement        * aStatement,
                                     qmsQuerySet        * aQuerySet,
                                     qmoTransPredicate  * aTransPredicate,
                                     qmoTransMatrix    ** aTransMatrix,
                                     idBool             * aIsApplicable );

    // transitive predicate matrix�� transitive predicate�� ����
    static IDE_RC  generatePredicate( qcStatement     * aStatement,
                                      qmsQuerySet     * aQuerySet,
                                      qmoTransMatrix  * aTransMatrix,
                                      qtcNode        ** aTransitiveNode );

    // predicate��� operator list�� operand list�� ����
    static IDE_RC  createOperatorAndOperandList( qcStatement       * aStatement,
                                                 qmsQuerySet       * aQuerySet,
                                                 qmoTransPredicate * aTransPredicate,
                                                 qtcNode           * aNode,
                                                 idBool              aOnlyOneNode );

    // �ϳ��� predicate�� qmoTransOperator�� ����
    static IDE_RC  createOperatorAndOperand( qcStatement       * aStatement,
                                             qmoTransPredicate * aTransPredicate,
                                             qtcNode           * aCompareNode );

    // ��尡 index�� ���� �÷�������� �˻�
    static IDE_RC  isIndexColumn( qcStatement * aStatement,
                                  qtcNode     * aNode,
                                  idBool      * aIsIndexColumn );

    // ���� type�� �÷��� ���� type���� �˻�
    static IDE_RC  isSameCharType( qcStatement * aStatement,
                                   qtcNode     * aLeftNode,
                                   qtcNode     * aRightNode,
                                   idBool      * aIsSameType );

    // transitive predicate�� ������ base predicate �˻�
    static IDE_RC  isTransitiveForm( qcStatement * aStatement,
                                     qmsQuerySet * aQuerySet,
                                     qtcNode     * aCompareNode,
                                     idBool        aCheckNext,
                                     idBool      * aIsTransitiveForm );

    // operand list�� operand set�� ����
    static IDE_RC  createOperandSet( qcStatement       * aStatement,
                                     qmoTransPredicate * aTransPredicate,
                                     idBool            * aIsApplicable );

    // �����带 �����ϴ� operator�� ���ڸ� ����
    static IDE_RC  changeOperand( qmoTransPredicate * aTransPredicate,
                                  qmoTransOperand   * aFrom,
                                  qmoTransOperand   * aTo );

    // transitive predicate matrix�� �����ϰ� �ʱ�ȭ
    static IDE_RC  initializeTransitiveMatrix( qmoTransMatrix * aTransMatrix );

    // transitive predicate matrix�� ���
    static IDE_RC  calculateTransitiveMatrix( qcStatement    * aStatement,
                                              qmsQuerySet    * aQuerySet,
                                              qmoTransMatrix * aTransMatrix );

    // ������ transitive predicate�� bad transitive predicate���� �Ǵ�
    static IDE_RC  isBadTransitivePredicate( qcStatement       * aStatement,
                                             qmsQuerySet       * aQuerySet,
                                             qmoTransPredicate * aTransPredicate,
                                             UInt                aOperandId1,
                                             UInt                aOperandId2,
                                             UInt                aOperandId3,
                                             idBool            * aIsBad );

    // qmoTransOperator�� ����
    static IDE_RC  createNewOperator( qcStatement       * aStatement,
                                      qmoTransPredicate * aTransPredicate,
                                      UInt                aLeftId,
                                      UInt                aRightId,
                                      UInt                aNewOperatorId );

    // transitive predicate�� ����
    static IDE_RC  generateTransitivePredicate( qcStatement     * aStatement,
                                                qmsQuerySet     * aQuerySet,
                                                qmoTransMatrix  * aTransMatrix,
                                                qtcNode        ** aTransitiveNode );

    // �ϳ��� transitive predicate�� ����
    static IDE_RC  createTransitivePredicate( qcStatement        * aStatement,
                                              qmsQuerySet        * aQuerySet,
                                              qmoTransOperator   * aOperator,
                                              qtcNode           ** aTransitivePredicate );

    // �������� ������ predicate���� �˻�
    static IDE_RC  isEquivalentPredicate( qcStatement  * aStatement,
                                          qmoPredicate * aPredicate1,
                                          qmoPredicate * aPredicate2,
                                          idBool       * aIsEquivalent );

    static IDE_RC getJoinDependency( qmsFrom   * aFrom,
                                     qcDepInfo * aDepInfo );
};

#endif /* _O_QMO_TRANSITIVITY_H_ */
