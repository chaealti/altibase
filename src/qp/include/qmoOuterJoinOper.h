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
 * $Id: qmoOuterJoinOper.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     PROJ-1653 Outer Join Operator (+)
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_OUTER_JOIN_OPER_H_
#define _O_QMO_OUTER_JOIN_OPER_H_ 1


#include <qc.h>
#include <qmoCnfMgr.h>
#include <qmsParseTree.h>

//---------------------------------------------------
// Outer Join Operator �� �����ϴ� ���� ����
//
// QMO_JOIN_OPER_NONE  : �ʱⰪ
// QMO_JOIN_OPER_FALSE : ���̺� (+)�� ����.
// QMO_JOIN_OPER_BOTH  : t1.i1(+) - t1.i2 = 1 �� ���� �� ���̺� �ٱ⵵ �ϰ� �Ⱥٱ⵵ ����.
//                       predicate level ������ QMO_JOIN_OPER_BOTH �� ������ �ȴ�.
//---------------------------------------------------

#define QMO_JOIN_OPER_MASK   0x03
#define QMO_JOIN_OPER_NONE   0x00
#define QMO_JOIN_OPER_FALSE  0x01
#define QMO_JOIN_OPER_TRUE   0x02
#define QMO_JOIN_OPER_BOTH   0x03   // QMO_JOIN_OPER_BOTH
                                    // = QMO_JOIN_OPER_FALSE|QMO_JOIN_OPER_TRUE

//---------------------------------------------------
// Predicate �� Outer Join Operator �� �ִ� ���̺��� 1 ���� ����
// (List,Range Type Predicate ����)
//---------------------------------------------------
#define QMO_JOIN_OPER_TABLE_CNT_PER_PRED              (1)

//---------------------------------------------------
// Outer Join Predicate �� dependency table �� 2 ������
// (List,Range Type Predicate ����)
//---------------------------------------------------
#define QMO_JOIN_OPER_DEPENDENCY_TABLE_CNT_PER_PRED   (2)


//---------------------------------------------------
// qtcNode.depInfo.depJoinOper[i] ���� �޾Ƽ�
// Outer Join Operator �� ������ �˻�
//---------------------------------------------------
# define QMO_JOIN_OPER_EXISTS( joinOper )                                   \
    (                                                                       \
       ( ( (joinOper & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_TRUE )          \
        || ( (joinOper & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_BOTH ) )      \
       ? ID_TRUE : ID_FALSE                                                 \
    )

//---------------------------------------------------
// Outer Join Operator �� ������ ���� �ݴ����� �˻�
//---------------------------------------------------
# define QMO_IS_JOIN_OPER_COUNTER( joinOper1, joinOper2 )                  \
    (                                                                      \
       (                                                                   \
         ( ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_TRUE )      \
          || ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_BOTH ) )  \
        && ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_FALSE )     \
       )                                                                   \
       ||                                                                  \
       (                                                                   \
         ( ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_TRUE )      \
          || ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_BOTH ) )  \
        && ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_FALSE )     \
       )                                                                   \
       ? ID_TRUE : ID_FALSE                                                \
    )

//---------------------------------------------------
// Outer Join Operator �� ������ ���� ������ �˻�.
// QMO_JOIN_OPER_TRUE �� QMO_JOIN_OPER_BOTH �� �Ѵ� Predicate ������
// Outer Join Operator �� �ִٴ� ��.
//---------------------------------------------------
# define QMO_JOIN_OPER_EQUALS( joinOper1, joinOper2 )                      \
    (                                                                      \
       (                                                                   \
           ( ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_TRUE )    \
            || ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_BOTH ) )\
        && ( ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_TRUE )    \
            || ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_BOTH ) )\
       )                                                                   \
       ||                                                                  \
       (                                                                   \
           ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_FALSE )     \
        && ( (joinOper2 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_FALSE )     \
       )                                                                   \
       ||                                                                  \
       (                                                                   \
           ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_NONE )      \
        && ( (joinOper1 & QMO_JOIN_OPER_MASK) == QMO_JOIN_OPER_NONE )      \
       )                                                                   \
       ? ID_TRUE : ID_FALSE                                                \
    )

#define QMO_IS_ANY_TYPE_MODULE( module )                                   \
    (                                                                      \
        ((module) == & mtfEqualAny )                                       \
     || ((module) == & mtfGreaterEqualAny )                                \
     || ((module) == & mtfGreaterEqualAny )                                \
     || ((module) == & mtfGreaterThanAny )                                 \
     || ((module) == & mtfLessEqualAny )                                   \
     || ((module) == & mtfLessThanAny )                                    \
     || ((module) == & mtfNotEqualAny )                                    \
       ? ID_TRUE : ID_FALSE                                                \
    )

#define QMO_IS_ALL_TYPE_MODULE( module )                                   \
    (                                                                      \
        ((module) == & mtfEqual )                                          \
     || ((module) == & mtfEqualAll )                                       \
     || ((module) == & mtfGreaterEqualAll )                                \
     || ((module) == & mtfGreaterThanAll )                                 \
     || ((module) == & mtfLessEqualAll )                                   \
     || ((module) == & mtfLessThanAll )                                    \
     || ((module) == & mtfNotEqual )                                       \
     || ((module) == & mtfNotEqualAll )                                    \
       ? ID_TRUE : ID_FALSE                                                \
    )

#define QMO_SRC_IS_LIST_MODULE( module )                                   \
    (                                                                      \
        ((module) == & mtfEqual )                                          \
     || ((module) == & mtfEqualAll )                                       \
     || ((module) == & mtfEqualAny )                                       \
     || ((module) == & mtfNotEqual )                                       \
     || ((module) == & mtfNotEqualAll )                                    \
     || ((module) == & mtfNotEqualAny )                                    \
       ? ID_TRUE : ID_FALSE                                                \
    )


//---------------------------------------------------
// Outer Join Operator �� ����ϴ� Join Predicate ��
// �����ϱ� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoJoinOperPred
{
    qtcNode          * node;   // OR�� �񱳿����� ������ predicate

    qmoJoinOperPred  * next;   // qmoJoinOperPred �� ����
} qmoJoinOperPred;


//---------------------------------------------------
// Join Relation �� �����ϱ� ���� �ڷᱸ��
//---------------------------------------------------

typedef struct qmoJoinOperRel
{
    qtcNode          * node;   // = qmoJoinOperPred->node
    qcDepInfo          depInfo;
} qmoJoinOperRel;


//---------------------------------------------------
// Outer Join Operator �� ����ϴ� Join Predicate ���� 
// �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoJoinOperGroup
{
    qmoJoinOperPred     * joinPredicate;    // join predicate

    qmoJoinOperRel      * joinRelation;     // array
    UInt                  joinRelationCnt;
    UInt                  joinRelationCntMax;

    // join group ���� ��� table���� dependencies�� ORing �� ��
    qcDepInfo             depInfo;
} qmoJoinOperGroup;


//---------------------------------------------------
// Outer Join Operator �� ����ϴ� �ڷᱸ���� ANSI Style �� �����ϴ�
// �������� Classifying, Grouping, Relationing �� �����ϱ� ����
// �ʿ��� �ӽ� ������ �ڷ� ����.
// ��ȯ�� �Ϸ�� �� Optimization ���ĺ��ʹ� �� �̻� �� �ڷᱸ����
// ������ �ʴ´�.
//
// *** Outer Join Operator �� ��ȯ�ϴ� �������� �����ϴ�
//     Join Grouping �� Join Relationing �� ���� optimization 
//     �������� �����ϴ� �װͰ� ������ ��������� ���� �����̴�
//     ȥ������ �ʱ⸦ �ٶ���.
//     �׷���, �ڷᱸ����� �Լ����� �ǵ��� �տ� joinOper ���
//     ���� �ٿ��� ����߰�, �� ���� ��������� ��� joinOper ��
//     ���� ���� ��� ���信 ���� �뵵�� �߽����� naming �ߴ�.
//---------------------------------------------------

typedef struct qmoJoinOperTrans
{
    qtcNode       * normalCNF;   // normalized �� CNF �� ����Ŵ(=qmoCNF->normalCNF)

    //-------------------------------------------------------------------
    //  transform �� �����ϵ��� �ܼ��ϰ� ����� �Ʒ��� �ڷᱸ���� �̿��Ѵ�.
    //  Predicate �з� �� Outer Join Operator �� ������ oneTablePred/joinOperPred ��
    //  �����ϰ�, ���ų� constant predicate �̸� generalPred �� �����Ѵ�.
    //  generalPred �� ����� Predicate �� transform �Ŀ�
    //  �״�� where ��(normalCNF)�� �����ְ� �ȴ�.
    //  ����, Grouping �� ��� �ƴϴ�.
    //
    //   - oneTablePred : Outer Join Operator + one table dependency ������ pred list
    //   - joinOperPred : Outer Join Operator + two table ���� �̻��� pred list
    //   - generalPred  : constant predicate �����Ͽ� Outer Join Operator ��
    //                    ���� ��� Predicate
    //
    //   - maxJoinOperGroupCnt : �ִ� joinOperGroupCnt = BaseTableCnt
    //   - joinOperGroupCnt    : ���� joinOperGroupCnt
    //   - joinOperGroup       : joinOperGroup �迭
    //-------------------------------------------------------------------

    qmoJoinOperPred   * oneTablePred;
    qmoJoinOperPred   * joinOperPred;

    qmoJoinOperPred   * generalPred;

    UInt                maxJoinOperGroupCnt;
    UInt                joinOperGroupCnt;
    qmoJoinOperGroup  * joinOperGroup;

} qmoJoinOperTrans;


//---------------------------------------------------
// Outer Join Operator (+) ��� �����Ͽ� �űԷ� �߰��� �Լ�����
// ��Ƴ��� class
//---------------------------------------------------
class qmoOuterJoinOper
{
private:

    // qmoJoinOperTrans �ڷᱸ�� �ʱ�ȭ
    static IDE_RC    initJoinOperTrans( qmoJoinOperTrans * aTrans,
                                        qtcNode          * aNormalForm );

    // qmoJoinOperGroup �ڷᱸ�� �ʱ�ȭ
    static IDE_RC    initJoinOperGroup( qcStatement      * aStatement,
                                        qmoJoinOperGroup * aJoinGroup,
                                        UInt               aJoinGroupCnt );

    // LIST Type �� �񱳿����ڸ� �Ϲݿ����ڷ� ��ȯ �� ��ȯ�� �Ϲݿ����� string return
    static IDE_RC    getCompModule4List( mtfModule    * aModule,
                                         mtfModule   ** aCompModule );

    // avaliable �� Join Relation ���� �˻�
    static IDE_RC    isAvailableJoinOperRel( qcDepInfo  * aDepInfo,
                                             idBool     * aIsTrue );

    // make new OR Node Tree
    static IDE_RC    makeNewORNodeTree( qcStatement  * aStatement,
                                        qmsQuerySet  * aQuerySet,
                                        qtcNode      * aInNode,
                                        qtcNode      * aOldSrc,
                                        qtcNode      * aOldTrg,
                                        qtcNode     ** aNewSrc,
                                        qtcNode     ** aNewTrg,
                                        mtfModule    * aCompModule,
                                        qtcNode     ** aORNode,
                                        idBool         aEstimate );

    // make new Operand Node Tree
    static IDE_RC    makeNewOperNodeTree( qcStatement  * aStatement,
                                          qtcNode      * aInNode,
                                          qtcNode      * aOldSrc,
                                          qtcNode      * aOldTrg,
                                          qtcNode     ** aNewSrc,
                                          qtcNode     ** aNewTrg,
                                          mtfModule    * aCompModule,
                                          qtcNode     ** aOperNode );

    // normalize �� Tree ���� LIST/RANGE Type �� Ư���񱳿����� Predicate ����
    // �Ϲ� �񱳿����� ���·� ��ȯ
    static IDE_RC    transNormalCNF4SpecialPred( qcStatement       * aStatement,
                                                 qmsQuerySet       * aQuerySet,
                                                 qmoJoinOperTrans  * aCNF );

    // ��ȯ�� normalCNF Tree �� Predicate ���� ��ȸ�ϸ� ������� �˻�
    static IDE_RC    validateJoinOperConstraints( qcStatement * aStatement,
                                                  qmsQuerySet * aQuerySet,
                                                  qtcNode     * aNormalCNF );

    // normalCNF �� Join ���踦 ����Ͽ� From Tree �� normalCNF Tree ��ȯ
    static IDE_RC    transformStruct2ANSIStyle( qcStatement       * aStatement,
                                                qmsQuerySet       * aQuerySet,
                                                qmoJoinOperTrans  * aTrans );


    // constant predicate ���� �˻�
    static IDE_RC    isConstJoinOperPred( qmsQuerySet * aQuerySet,
                                          qtcNode     * aPredicate,
                                          idBool      * aIsTrue );

    // one table predicate ���� �˻�
    static IDE_RC    isOneTableJoinOperPred( qmsQuerySet * aQuerySet,
                                             qtcNode     * aPredicate,
                                             idBool      * aIsTrue );

    // qmoJoinOperPred �ڷᱸ�� ����
    static IDE_RC    createJoinOperPred( qcStatement      * aStatement,
                                         qtcNode          * aNode,
                                         qmoJoinOperPred ** aNewPredicate );

    // constant/onetable/join predicate �з�
    static IDE_RC    classifyJoinOperPredicate( qcStatement       * aStatement,
                                                qmsQuerySet       * aQuerySet,
                                                qmoJoinOperTrans  * aTrans );

    // Join Predicate �� Grouping, Relationing, Ordering
    static IDE_RC    joinOperOrdering( qcStatement       * aStatement,
                                       qmoJoinOperTrans  * aTrans );

    // Join Predicate �� Grouping
    static IDE_RC    joinOperGrouping( qmoJoinOperTrans  * aTrans );

    // Join Predicate �� Relationing
    static IDE_RC    joinOperRelationing( qcStatement      * aStatement,
                                          qmoJoinOperGroup * aJoinGroup );

    // Outer Join Operator ��  ���Ե� Join Predicate ���� ANSI ���·� ��ȯ
    static IDE_RC    transformJoinOper( qcStatement      * aStatement,
                                        qmsQuerySet      * aQuerySet,
                                        qmoJoinOperTrans * aTrans );

    // Outer Join Operator ���� From ���� ��ȯ
    static IDE_RC    transformJoinOper2From( qcStatement      * aStatement,
                                             qmsQuerySet      * aQuerySet,
                                             qmoJoinOperTrans * aTrans,
                                             qmsFrom          * sNewFrom,
                                             qmoJoinOperRel   * sJoinRel,
                                             qmoJoinOperPred  * sJoinPred );

    // Outer Join Operator ���� onCondition ���� ��ȯ
    static IDE_RC    movePred2OnCondition( qcStatement      * aStatement,
                                           qmoJoinOperTrans * aTrans,
                                           qmsFrom          * aNewFrom,
                                           qmoJoinOperRel   * aJoinGroupRel,
                                           qmoJoinOperPred  * aJoinGroupPred );

    // From Tree �� ��ȸ�ϸ� New From ��带 Merge
    static IDE_RC    merge2ANSIStyleFromTree( qmsFrom     * aFromFrom,
                                              qmsFrom     * aToFrom,
                                              idBool        aFirstIsLeft,
                                              qcDepInfo   * aDepInfo,
                                              qmsFrom     * aNewFrom );

    // normalCNF ���� RANGE Type �� Predicate ��� ��ȯ ����
    static IDE_RC    transJoinOperBetweenStyle( qcStatement * aStatement,
                                                qmsQuerySet * aQuerySet,
                                                qtcNode     * aInNode,
                                                qtcNode    ** aTransStart,
                                                qtcNode    ** aTransEnd );

    // normalCNF ���� LIST Type �� Predicate ��� ��ȯ ����
    static IDE_RC    transJoinOperListArgStyle( qcStatement * aStatement,
                                                qmsQuerySet * aQuerySet,
                                                qtcNode     * aInNode,
                                                qtcNode    ** aTransStart,
                                                qtcNode    ** aTransEnd );
 
public:

    static IDE_RC    transform2ANSIJoin( qcStatement  * aStatement,
                                         qmsQuerySet  * aQuerySet,
                                         qtcNode     ** aNormalForm );
};

#endif /* _O_QMO_OUTER_JOIN_OPER_H_ */
