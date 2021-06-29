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
 *     PROJ-1653 Outer Join Operator (+)
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qcuSqlSourceInfo.h>
#include <qmsParseTree.h>
#include <qmo.h>
#include <qtc.h>
#include <qmoOuterJoinOper.h>

extern mtfModule mtfAnd;
extern mtfModule mtfOr;
extern mtfModule mtfEqual;
extern mtfModule mtfList;
extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;
extern mtfModule mtfEqualAll;
extern mtfModule mtfEqualAny;
extern mtfModule mtfGreaterEqualAll;
extern mtfModule mtfGreaterEqualAny;
extern mtfModule mtfGreaterThanAll;
extern mtfModule mtfGreaterThanAny;
extern mtfModule mtfLessEqualAll;
extern mtfModule mtfLessEqualAny;
extern mtfModule mtfLessThanAll;
extern mtfModule mtfLessThanAny;
extern mtfModule mtfNotEqual;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfNotEqualAny;
extern mtdModule mtdList;

extern mtfModule mtfGreaterThan;
extern mtfModule mtfLessThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfLessEqual;

/***********************************************************************
 *  ** ���� ����
 *
 *  Outer Join Operator �� ���� ����Ŭ�� ��Ƽ���̽��� ������� ������
 *
 *  1. ����Ŭ�� "(", "+", ")" ���� ���̿� white space �� �� �� �ִ�.
 *     ��Ƽ���̽��� �ȵȴ�. �����ڰ� �ϳ��� ��ū�̴�.
 *
 *     ��. ����Ŭ
 *         where t1.i1( + ) = 1 -- (O)
 *         where t1.i1(
 *                   + ) = 1 -- (O)
 *     ��. ��Ƽ���̽�
 *         where t1.i1(+) = 1 -- (O)
 *         where t1.i1( +) = 1 -- (X)
 *
 *  2. ����Ŭ�� ���� equal �����ڿ��� list argument �� ����� �� ����.
 *     ��Ƽ���̽��� �����ϴ�.
 *
 *     ��. ����Ŭ
 *         where (t1.i1(+),2) = (1,2) -- (X)
 *         where (t1.i1(+),2) = (t2.i1,2) -- (X)
 *     ��. ��Ƽ���̽�
 *         where (t1.i1(+),2) = (1,2) -- (O)
 *         where (t1.i1(+),2) = (t2.i1,2) -- (O)
 *
 *  3. ����Ŭ�� ���� order by, group by, on �� � outer join operator �� ����� �� ����.
 *     ��Ƽ���̽��� ���� �־ �ǹ̾��� ���� ���� ����ó���� ���� �ʴ´�.
 *
 *  4. ����Ŭ�� ��� subquery �� outer join �� �� ����. (������, rule �� ��Ȯ���� �ʴ�.)
 *     ��Ƽ���̴� Ư������� ��ȯ�� �߻��ؾ��� ��쿡 predicate �� subquery �� ������ �ȵȴ�.
 *     (��, Between/List Arg Style �� Predicate ����, (+)�� �����ϰ�
 *     dependency table �� 2 �� �̻��� �� subquery �� �Բ� ���Ǹ� �����̴�.)
 *
 *     ��. ����Ŭ
 *         where t1.i1(+) = (select 1 from dual) -- (X)
 *         where t1.i1(+) - (select 1 from dual) = 0 -- (O)
 *         where t1.i1=t2.i1(+) and t1.i1(+)=(select 1 from dual) -- (X)
 *         where ((select 1 from dual),2)=((t1.i1(+),t2.i1)) -- (O)
 *         where t1.i1(+)-(select 1 from dual)=0 or t1.i1(+)=2 -- (X)
 *     ��. ��Ƽ���̽�
 *         where t1.i1(+) = (select 1 from dual) -- (O)
 *         where t1.i1(+) - (select 1 from dual) = 0 -- (O)
 *         where t1.i1=t2.i1(+) and t1.i1(+)=(select 1 from dual) -- (O)
 *         where ((select 1 from dual),2)=((t1.i1(+),t2.i1)) -- (X)
 *         where t1.i1(+)-(select 1 from dual)=0 or t1.i1(+)=2 -- (O)
 ***********************************************************************/


IDE_RC
qmoOuterJoinOper::transform2ANSIJoin( qcStatement  * aStatement,
                                      qmsQuerySet  * aQuerySet,
                                      qtcNode     ** aNormalForm )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : (+) �� �ִ� normalCNF �� ANSI Join ���·� ��ȯ
 *
 * Implementation :
 *
 *   �Ʒ��� ������ ������ ����
 *   Outer Join Operator(+) �� �ִ� normalCNF �� ������ ANSI Outer
 *   Join �� ������� ���� ���� �ڷᱸ�� ���·� ��ȯ�Ѵ�.
 *   ��, From Tree �� Left Outer Join ��带 ����ϴ� ���·�
 *   �����ϰ�, �׿� ������ onCondition ������ ������ �����Ǵ�
 *   normalCNF Predicate ���� �и��Ͽ� �Ű��ִ� ���� �۾��̴�.
 *   �� ������ Optimization �۾����� �̷��� ��ȯ�� From,
 *   normalCNF�� �̿��Ͽ� �����ϰ� �ȴ�.
 *
 *   1. sNormalForm �� ��� �߿��� between/any,all,in �� ����
 *      �񱳿����ڴ� �ܼ� �� �����ڳ��� ��ȯ�ؾ� �Ѵ�.
 *      �׷��� ������, �Ʒ��� ���� �ټ� ���̺��� ����
 *      Join Predicate �� ��� onCondition ���� ��ȯ�ϱⰡ ��ƴ�.
 *
 *      (t1.i1(+),t2.i1(+),1) in ((t2.i1,t3.i1,t3.i1(+)))
 *
 *   2. Normalize �� Tree �� predicate �� traverse �ϸ鼭
 *      Outer Join Operator �� ���õ� ���� ��������� �˻��Ѵ�.
 *      terminal node�� ���� ��������� qtc::estimateInternal
 *      �Լ����� ��ü node traverse �ÿ� �ɷ�����.
 *      predicate ���迡 ���� ��������� transformStruct2ANSIStyle
 *      �ܰ迡�� �˻��Ѵ�.
 *
 *   3. normalized �� where ���� Outer Join Operator�� ���� ���ǵ���
 *      ANSI ������ From, normalCNF ���� Transform
 *
 *  ** ����
 *   ���� ���� ��ȯ �������� join grouping, join relationing �̶�� ��
 *   �����ϴµ�, �̴� ���� optimization �ܰ迡�� �����ϴ� �װͰ���
 *   �����̴�.
 *   ȥ���� ���ϱ� ���� �ǵ��� �Լ��� ���� joinOperGrouping, joinOperRelationing ��
 *   ���� joinOper �� �ٿ��� ����Ͽ���.
 *   �ּ��������� joinOper �� �ٿ��� �����ϴ� ���� ����ϱ� ������
 *   join grouping, relationing ���� �״�� ����Ͽ���.
 ***********************************************************************/

    qtcNode            * sNormalForm = NULL;
    qmoJoinOperTrans   * sTrans      = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transform2ANSIJoin::__FT__" );

    IDE_FT_ASSERT( aStatement   != NULL );
    IDE_FT_ASSERT( aQuerySet    != NULL );
    IDE_FT_ASSERT( aNormalForm  != NULL );

    sNormalForm = (qtcNode *)*aNormalForm;

    // BUG-40042 oracle outer join property
    IDE_TEST_RAISE( QCU_OUTER_JOIN_OPER_TO_ANSI_JOIN == 0,
                    ERR_NOT_ALLOWED_OUTER_JOIN_OPER );

    // --------------------------------------------------------------
    // ��ȯ�� �ʿ��� qmoJoinOperTrans ����ü�� �Ҵ� �� �ʱ�ȭ
    // --------------------------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinOperTrans ),
                                             (void**) & sTrans )
              != IDE_SUCCESS );

    IDE_TEST( initJoinOperTrans( sTrans, sNormalForm )
              != IDE_SUCCESS );

    // --------------------------------------------------------------
    // in/between ���� ��带 �ܼ� Predicate ���� ��ȯ
    // --------------------------------------------------------------

    IDE_TEST( transNormalCNF4SpecialPred( aStatement,
                                          aQuerySet,
                                          sTrans )
              != IDE_SUCCESS );

    // --------------------------------------------------------------
    // Predicate ���� ������� �˻�
    // --------------------------------------------------------------

    IDE_TEST( validateJoinOperConstraints( aStatement,
                                           aQuerySet,
                                           sTrans->normalCNF )
              != IDE_SUCCESS );

    // --------------------------------------------------------------
    // ANSI ���·� �ڷ� ���� ��ȯ (From, normalCNF)
    // --------------------------------------------------------------

    IDE_TEST( transformStruct2ANSIStyle( aStatement,
                                         aQuerySet,
                                         sTrans )
              != IDE_SUCCESS );

    *aNormalForm = sTrans->normalCNF;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOWED_OUTER_JOIN_OPER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMO_NOT_ALLOWED_OUTER_JOIN_OPER ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::initJoinOperTrans( qmoJoinOperTrans * aTrans,
                                     qtcNode          * aNormalForm )
{
/***********************************************************************
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : qmoJoinOperTrans �ڷᱸ�� �ʱ�ȭ
 *
 * Implementation :
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::initJoinOperTrans::__FT__" );

    IDE_FT_ASSERT( aNormalForm   != NULL );

    aTrans->normalCNF      = aNormalForm;

    aTrans->oneTablePred   = NULL;
    aTrans->joinOperPred   = NULL;
    aTrans->generalPred    = NULL;
    aTrans->maxJoinOperGroupCnt = 0;
    aTrans->joinOperGroupCnt = 0;
    aTrans->joinOperGroup  = NULL;

    return IDE_SUCCESS;
}


IDE_RC
qmoOuterJoinOper::transNormalCNF4SpecialPred( qcStatement       * aStatement,
                                              qmsQuerySet       * aQuerySet,
                                              qmoJoinOperTrans  * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Ư���� ��带 �Ϲ� �� Predicate ���� ��ȯ
 *
 * Implementation :
 *    Outer Join Operator �� ����ϴ� Predicate �� ��
 *
 *     - Between, NotBetween �� ���� ������ ���ϴ�
 *       Between Style �� �񱳿����ڿ�,
 *     - Equal,EqualAny,EqualAll,NotEqual �� ���� List Argument ��
 *       ������ Style �� �񱳿����ڴ�
 *
 *    �ټ��� ���̺� dependency �� ���� �� �ְ�, Outer Join Operator ����
 *    ������ ���·� ������ �� �ִ�.
 *
 *    ��, (=,>,<,...) ��� ���� �Ϲ� �񱳿����ڴ� �ϳ��� Predicate ��
 *    �� �ϳ��� Outer Join Operator �� �ΰ� ������ dependency table �ۿ�
 *    ����� �� ���ٴ� ��Ȯ�� ��Ģ�� �����Ѵ�.
 *    �׷���, ���� �ΰ��� Style �� �񱳿����ڴ� �׷��� ��Ģ��
 *    ������ �� ���� ������ ������¸� ���� �� �ִ�.
 *
 *    �̷� ���� (+) Join ������ ANSI ���·� transform ��������
 *    �ش� Predicate �� onCondition ���� �ű� �� Predicate ��
 *    ������ �������� ����ó�� ������ ���� ��� �з��Ͽ� ������ �´�
 *    LEFT OUTER JOIN ���� �ٿ���� �Ѵ�.
 *
 *    ���� ��� �Ʒ��� ���� ���캸��.
 *    �Ʒ��� ������ �״�� onCondition ���� ������ �� ����.
 *    dependency table �� t1,t2,t3 ������ �����ϱ� ������,
 *    �ΰ��� ���̺��� Join ���踦 ǥ���ϴ� on ���� �� ���� ���� ���̴�.
 *    �׷��Ƿ�, Predicate �� �������� �и������ʴ� �� ��ȯ�� �Ұ����ϴ�.
 *
 *      (t1.i1(+),t2.i1(+),1) in ((t2.i1,t3.i1,t3.i1(+)))
 *
 *    ������ ���� ��ȯ�� �� Predicate ���� ������ �´� OUTER JOIN �����
 *    onConditio �� ��������� �Ѵ�.
 *
 *         t1.i1(+) = t2.i1
 *     and t2.i1(+) = t3.i1
 *     and 1 = t3.i1(+)
 *
 *    �̸� transform �������� �����ϸ� ����ġ�� ������ ������ �� �� �ִ�.
 *    �Ұ����Ѱ� �ƴ����� ����ġ�� ������ ���� ������.
 *    ���� ������ in ���� ���� �������, any, all �����ڰ� �ε�ȣ�� �Բ�
 *    ���Ǹ� ���� ����������.
 *
 *    ����, ������� �˻� �� transform �������� ��� Join Predicate ��
 *    �ܼ��� ������ ���¶�� �ϰ� ó���� �� �ֵ��� ������ 1 �ܰ���
 *    ��ȯ�۾��� ������ �ʿ䰡 �ִ�.
 *    �װ��� �� �Լ����� �ϴ� ���̴�.
 *
 *    �̷��� ��ȯ�۾��� �߰��� �����ϸ�, ������ ���� ������ �� Predicate
 *    ��ȯ�� ���� ������ �ణ �þ�Ƿ� �޸� ��뷮 ������ �������ϰ�
 *    ���� �� �ִ�.
 *    ������, �Ʒ��� ���� ���Ǵ� ���� ���� ������ ������ ����ȴ�.
 *
 *    ( 2�� �̻��� dependency table + 1 �� �̻��� Outer Join Operator
 *                                  + �ټ��� list argument )
 *
 *
 *    �� ��ȯ���� ���� ����Ǵ� ���� LIST Argument Style �� Predicate ��
 *    Ǯ� ��ȯ�� �� �űԷ� �����Ǵ� ��尡 ��û���� �������� �������̴�.
 *    �׷���, ����ڴ� ��κ� �Ʒ��� (1) ���·� ����� ��
 *    (2),(3) �� ���� ��������� �ʴ´�.
 *    (+)�� ����� ����� �����Ͽ� ������ �����⵵ �Ұ����� �Ӵ���,
 *    (2),(3) �� ���� ������ O �翡���� ���������� �ʴ´�.
 *
 *    (1) �� ���� �� �Լ����� ��ȯ���� �ʿ䰡 �����Ƿ� ������� �ʾƵ� �ȴ�.
 *    �ֳ��ϸ�, (1) �� ���� ���´� Join Predicate �� �ƴϹǷ�
 *    ���� transform �������� �����Ǵ� onCondition �� �׳� ���Ḹ ���ָ�
 *    �Ǳ� �����̴�.
 *
 *    (1) t1.i1(+) in (1,2,3,4)
 *    (2) (t1.i1(+),t2.i1) in ((t3.i1,t4.i1(+)),(t2.i1,t1.i2(+)))
 *    (3) t1.i1 in (t2.i1(+),1,2,3,4,5,6,7,8,...)
 *
 *
 *
 *    ��ȯ������ ���캸��. ((+) ��ȣ�� �����ߴ�.)
 *
 *    (AND)
 *      |
 *      V
 *    (OR)    --->    (OR)    ---------->    (OR)    --> ...
 *      |               |                      |
 *      V               V                      V
 *   (Equal)       (EqualAny)              (Between)
 *      |               |                      |
 *      V               V                      V
 *    (a1) -> (a2)   (list)   ->   (list)    (c1) -> (c2) -> (c3)
 *                      |            |
 *                      V            V
 *                    (a1) -> (a2) (list)
 *                                   |
 *                                   V
 *                                 (b1) -> (b2)
 *
 *
 *    ���� ���� CNF Tree �� ������ ���� ������ ����� �� ���� ���̴�.
 *
 *    where a1 = a2
 *      and (a1,a2) in ((b1,b2))
 *      and c1 between c2 and c3
 *
 *
 *    ���� ������ �Ʒ��� ���� ���·� ��ȯ�ؾ� �Ѵ�.
 *
 *    where a1 = a2
 *      and (a1 = b1  and a2 = b2)
 *      and (c1 >= c2 and c1 <= c3)
 *
 *    �̸� CNF Tree �� �ݿ��ϸ�,
 *
 *    (AND)
 *      |
 *      V
 *    (OR)   --->   (OR)   --->   (OR)  ----->  (OR)   -->   (OR)
 *      |             |             |             |            |
 *      V             V             V             V            V
 *   (Equal)       (Equal)       (Equal)    (GreaterEqual) (LessEqual)
 *      |             |             |             |            |
 *      V             V             V             V            V
 *    (a1) -> (a2)  (a1) -> (b1)  (a2) -> (b2)  (c1) -> (c2) (c1) -> (c3)
 *
 *
 *    �������� ���������� ���� Ư������� �ڸ��� ��ü�ϴ� ���·�
 *    ����������, ���� ���� �ÿ��� ��� ����Ʈ�� �Ǿտ�
 *    �������ָ� �ȴ�.
 *
 *    ���� a1 in (b1,2,3) �� ���� Predicate �� ������ ���� ��ȯ�� ���̴�.
 *    OR �� argument �� ��� ����Ǿ��ٴ� ���� ���� �ٸ� ���̴�.
 *
 *    (AND)
 *      |
 *      V
 *    (OR)
 *      |
 *      V
 *   (Equal)   -->  (Equal)   -->   (Equal)
 *      |              |               |
 *      V              V               V
 *    (a1) -> (b1)    (a1) -> (2)     (a1) -> (3)
 *
 *
 *
 *    ��ȯ�� �ʿ��� ���� ���ʿ��� ����� �з�
 *    (�Ʒ� ��޵� Ư���񱳿����ڴ� Between / LIST Argument Style ��
 *     �� ������ Ÿ���� ���Ѵ�.)
 *
 *    1. Predicate ��ȯ ���ʿ�
 *
 *       ��. (+)�� ������� �ʴ� ��� ���
 *       ��. (+)�� 1���̸鼭 dependency table �� 2�� �̸��� ��� ���
 *       ��. (+)�� dependency table ������ ������� �Ϲݿ������� ���
 *
 *    2. Predicate ��ȯ �ʿ� (Ư���񱳿����ڸ� ����� ��츸 �ش�)
 *
 *       ��. (+) �� 2�� �̻��� ���
 *       ��. (+)�� 1���̸鼭 dependency table �� 2���� �ʰ��� ���
 ***********************************************************************/

    qcuSqlSourceInfo   sqlInfo;
    qtcNode          * sSrcNode        = NULL;
    qtcNode          * sTrgNode        = NULL;
    qtcNode          * sCurNode        = NULL;
    qtcNode          * sTransStart     = NULL;
    qtcNode          * sTransEnd       = NULL;

    qtcNode          * sORStart        = NULL;
    qtcNode          * sCurOR          = NULL;
    qtcNode          * sPrevOR         = NULL;

    qtcNode          * sPredPrev       = NULL;
    qtcNode          * sPredCur        = NULL;

    qtcNode          * sTmpTransStart  = NULL;
    qtcNode          * sTmpTransEnd    = NULL;

    idBool             sIsTransFirst = ID_FALSE;
    idBool             sIsSpecialOperand = ID_FALSE;
    idBool             sIsTransformedOR = ID_FALSE;
    UInt               sJoinOperCount;
    UInt               sTargetListCount;
    UInt               sDepCount;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transNormalCNF4SpecialPred::__FT__" );

    IDE_FT_ASSERT( aStatement   != NULL );
    IDE_FT_ASSERT( aQuerySet    != NULL );
    IDE_FT_ASSERT( aTrans       != NULL );


    sORStart = (qtcNode *)aTrans->normalCNF->node.arguments;

    for ( sCurOR  = sORStart;
          sCurOR != NULL;
          sCurOR  = (qtcNode *)sCurOR->node.next )
    {
        sIsTransformedOR = ID_FALSE;

        for ( sPredCur  = (qtcNode *)sCurOR->node.arguments;
              sPredCur != NULL;
              sPredPrev = sPredCur,
              sPredCur  = (qtcNode *)sPredCur->node.next )
        {
            //----------------------------------------------------
            //    Between / LIST Argument Style �񱳿����� ������ ���캸��.
            //
            //    ex> t1.i1 between 1 and 10
            //          ^           ^      ^
            //          |           |      |
            //       source      target  target   (source 1 ��, target 2 ��)
            //
            //        t1.i1 in (1,2)
            //          ^        ^
            //          |        |
            //       source    target             (source 1 ��, target list)
            //
            //       (t1.i1,t2.i1) in ((1,2))
            //             ^             ^
            //             |             |
            //          source        target      (source list, target list)
            //
            //
            //    BETWEEN Style
            //    ======================================================
            //    mtfModule          : mtcName       : source  : target
            //    ======================================================
            //    mtfBetween         : BETWEEN       :   1     :   2 (not list)
            //    mtfNotBetween      : NOT BETWEEN   :   1     :   2 (not list)
            //    ======================================================
            //
            //
            //    LIST Argument Style
            //    ======================================================
            //    mtfModule          : mtcName       : source  : target
            //    ======================================================
            //    mtfEqual           : =             : list    :  list
            //    mtfEqualAll        : =ALL          : list    :  list
            //                                       :   1     :  list
            //    mtfEqualAny        : =ANY          : list    :  list
            //                                       :   1     :  list
            //    mtfGreaterEqualAll : >=ALL         :   1     :  list
            //    mtfGreaterEqualAny : >=ANY         :   1     :  list
            //    mtfGreaterThanAll  : >ALL          :   1     :  list
            //    mtfGreaterThanAny  : >ANY          :   1     :  list
            //    mtfLessEqualAll    : <=ALL         :   1     :  list
            //    mtfLessEqualAny    : <=ANY         :   1     :  list
            //    mtfLessThanAll     : <ALL          :   1     :  list
            //    mtfLessThanAny     : <ANY          :   1     :  list
            //    mtfNotEqual        : <>            : list    :  list
            //    mtfNotEqualAll     : <>ALL         : list    :  list
            //    mtfNotEqualAny     : <>ANY         : list    :  list
            //                                       :   1     :  list
            //    ======================================================
            //
            //
            //    ���� �پ��� ���� �߿��� source �� list ����� �� �ִ�
            //    ������ ������� (+)�� �Բ� ����� �� ��������� �ε��� �Ѵ�.
            //    �̴� ����ڰ� �׷��� ��������� ���� �Ӵ���, ���ʿ��� ��ȯ��
            //    ������� ���� ���ؼ��̴�.
            //
            //    mtfEqual           : =             : list    :  list
            //    mtfEqualAll        : =ALL          : list    :  list
            //    mtfEqualAny        : =ANY          : list    :  list
            //    mtfNotEqual        : <>            : list    :  list
            //    mtfNotEqualAll     : <>ALL         : list    :  list
            //    mtfNotEqualAny     : <>ANY         : list    :  list
            //
            //    ==> source �� list �̸鼭 (+) �� �ϳ��̻� ���Ǿ���
            //        dependency table �� 2 �� �̻� ���Ǿ��� ���,
            //        target �� list �� argument �� 2 �� �̻� ���� �� ����.
            //        (mtfEqual �� ���� ���� target list �� 2 �� �̻���
            //        ���� ��ü�� �������� �ʴ´�.)
            //
            //
            //
            //   ���� ��� ������ ���� ��ȯ��Ģ�� ������.
            //
            //   �⺻ ��Ģ�� ����,
            //   sPredCur �� ��ȯ�� �� list head �� tale �� sTransStart ��
            //   sTransEnd �� ����Ű���� �Ѵ�.
            //   �� sTransStart �� sTransEnd �� ������ normalCNF ��
            //   �����ְ�, ������ sPredCur �� �����Ѵ�.
            //
            //
            //   (1) mtfBetween
            //
            //       ** ��ȯ ��
            //       a1 between a2 and a3
            //
            //        (AND)
            //          |
            //          V
            //        (OR)  <- sCurOR
            //          |
            //          V
            //      (Between) <- sPredCur
            //          |
            //          V
            //        (a1) -> (a2) -> (a3)
            //
            //       ** ��ȯ ��
            //       a1 >= a2 and a1 <= a3
            //
            //        (AND)
            //          |  _______ sTransStart ____ sTransEnd
            //          V /                   /
            //        (OR)       ---->    (OR)
            //          |                   |
            //          V                   V
            //     (GreaterEqual)      (LessEqual)
            //          |                   |
            //          V                   V
            //        (a1) -> (a2)        (a1) -> (a3)
            //
            //
            //   (2) mtfNotBetween
            //
            //       ** ��ȯ ��
            //       a1 not between a2 and a3
            //
            //        (AND)
            //          |
            //          V
            //        (OR)
            //          |
            //          V
            //     (NotBetween)
            //          |
            //          V
            //        (a1) -> (a2) -> (a3)
            //
            //       ** ��ȯ ��
            //       a1 < a2 and a1 > a3
            //
            //        (AND)
            //          |
            //          V
            //        (OR)
            //          |
            //          V
            //     (LessThan)   -->   (GreaterThan)
            //          |                   |
            //          V                   V
            //        (a1) -> (a2)        (a1) -> (a3)
            //
            //
            //   (3) mtfEqualAll
            //
            //       ** ��ȯ ��
            //       (a1,a2) =ALL ((b1,b2))
            //
            //        (AND)
            //          |
            //          V
            //        (OR)
            //          |
            //          V
            //     (EqualAll)
            //          |
            //          V
            //       (list)   --->  (list)
            //          |              |
            //          V              V
            //        (a1) -> (a2)  (list) <= can't be more than 1 list
            //                         |
            //                         V
            //                       (b1) -> (b2)
            //
            //       ** ��ȯ ��
            //       a1 = b1 and a2 = b2
            //
            //        (AND)
            //          |
            //          V
            //        (OR)    --->    (OR)
            //          |               |
            //          V               V
            //       (Equal)         (Equal)
            //          |               |
            //          V               V
            //        (a1) -> (a2)    (b1) -> (b2)
            //
            //
            //   (4) mtfEqualAny
            //
            //       ** ��ȯ ��
            //       (a1,a2) =ANY ((b1,b2))
            //
            //        (AND)
            //          |
            //          V
            //        (OR)
            //          |
            //          V
            //     (EqualAny)
            //          |
            //          V
            //       (list)   --->  (list)
            //          |              |
            //          V              V
            //        (a1) -> (a2)  (list) <= can't be more than 1 list
            //                         |
            //                         V
            //                       (b1) -> (b2)
            //
            //       ** ��ȯ ��
            //       a1 = b1 and a2 = b2
            //
            //        (AND)
            //          |
            //          V
            //        (OR)    --->    (OR)
            //          |               |
            //          V               V
            //       (Equal)         (Equal)
            //          |               |
            //          V               V
            //        (a1) -> (a2)    (b1) -> (b2)
            //
            //
            //   �������� �� �Լ�����...
            //----------------------------------------------------

            //---------------------------------------------
            // Outer Join Operator �� ���� ���� ��ȯ�� �����ϸ� ��.
            //---------------------------------------------

            if ( ( sPredCur->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                    == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                //---------------------------------------------
                // Between / LIST Argument Style �񱳿��������� �˻�
                //
                // MTC_NODE_GROUP_COMPARISON_TRUE
                //   - mtfEqualAll
                //   - mtfEqualAny
                //   - mtfGreaterEqualAll
                //   - mtfGreaterEqualAny
                //   - mtfGreaterThanAll
                //   - mtfGreaterThanAny
                //   - mtfLessEqualAll
                //   - mtfLessEqualAny
                //   - mtfLessThanAll
                //   - mtfLessThanAny
                //   - mtfNotEqualAll
                //   - mtfNotEqualAny
                //---------------------------------------------

                if ( ( sPredCur->node.module == & mtfBetween )
                  || ( sPredCur->node.module == & mtfNotBetween )
                  || ( sPredCur->node.module == & mtfEqual )
                  || ( sPredCur->node.module == & mtfNotEqual )
                  || ( ( sPredCur->node.module->lflag & MTC_NODE_GROUP_COMPARISON_MASK )
                         == MTC_NODE_GROUP_COMPARISON_TRUE ) )
                {
                    sIsSpecialOperand = ID_TRUE;
                }
                else
                {
                    sIsSpecialOperand = ID_FALSE;;
                }


                //---------------------------------------------
                // Ư��������(Between / LIST Argument Style �񱳿�����)�� ���
                //---------------------------------------------

                if ( sIsSpecialOperand == ID_TRUE )
                {
                    sTargetListCount = 0;

                    sSrcNode = (qtcNode *)sPredCur->node.arguments;
                    sTrgNode = (qtcNode *)sPredCur->node.arguments->next;

                    //---------------------------------------------
                    // Source �� list �� �� �ִ� �����ڵ�
                    //---------------------------------------------

                    if ( QMO_SRC_IS_LIST_MODULE( sPredCur->node.module ) == ID_TRUE )
                    {
                        //---------------------------------------------
                        // mtfEqual,mtfNotEqual �����ڴ� Source �� list �� ��
                        // �ܿ��� ��ȯ���� ���� �����Ƿ� �׳� continue �Ѵ�.
                        // (t1.i1,t2.i1(+)) = (1,2) �� ���� ���� ����
                        // ��ȯ���־�� �Ѵ�.
                        //---------------------------------------------

                        if ( ( ( sPredCur->node.module == & mtfEqual )
                            || ( sPredCur->node.module == & mtfNotEqual ) )
                          && ( sSrcNode->node.module != & mtfList ) )
                        {
                            continue;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    //---------------------------------------------
                    // Source �� list �� �� ���� �����ڵ�
                    //---------------------------------------------
                    else
                    {
                        //---------------------------------------------
                        // ������ Ư�� �񱳿����ڵ��� Source �ʿ�
                        // list �� �� �� ����.
                        //---------------------------------------------

                        sSrcNode = (qtcNode *)sPredCur->node.arguments;

                        if ( sSrcNode->node.module == & mtfList )
                        {
                            sqlInfo.setSourceInfo( aStatement,
                                                   & sPredCur->position );
                            IDE_RAISE( ERR_INVALID_OPERATION_WITH_LISTS );
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }


                    //---------------------------------------------
                    // dependency table & Outer Join Operator �� ���� �˻�
                    //---------------------------------------------

                    sJoinOperCount = qtc::getCountJoinOperator( & sPredCur->depInfo );
                    sDepCount      = qtc::getCountBitSet( & sPredCur->depInfo );

                    if ( ( sJoinOperCount == QMO_JOIN_OPER_TABLE_CNT_PER_PRED )
                      && ( sDepCount < QMO_JOIN_OPER_DEPENDENCY_TABLE_CNT_PER_PRED ) )
                    {
                        //---------------------------------------------
                        // Outer Join Operator �� ������ 1 ���̰�,
                        // dependency table ������ 2 �� �̸��̸�
                        // ��ȯ�� ���ʿ��ϴ�.
                        //---------------------------------------------

                        continue;
                    }
                    else
                    {
                        //---------------------------------------------
                        // Source �� list �̸鼭 (+) �� ����ϰ�,
                        // dependency table �� 2 �� �̻��� ����
                        // Target list �� 2 �� �̻��� �� �� ���ٴ� ��������� �д�.
                        // (mtfEqual,mtfNotEqual �� ����)
                        //---------------------------------------------
                        if ( ( sPredCur->node.module != & mtfEqual )
                          && ( sPredCur->node.module != & mtfNotEqual ) )
                        {
                            if ( sSrcNode->node.module == & mtfList )
                            {
                                for ( sCurNode  = (qtcNode *)sTrgNode->node.arguments;
                                      sCurNode != NULL;
                                      sCurNode  = (qtcNode *)sCurNode->node.next )
                                {
                                    sTargetListCount++;
                                }

                                if ( sTargetListCount > 1 )
                                {
                                    sqlInfo.setSourceInfo( aStatement,
                                                           & sPredCur->position );
                                    IDE_RAISE( ERR_INVALID_OPERATION_WITH_LISTS );
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

                        //---------------------------------------------
                        // ��ȯ ��� Predicate ���� Subquery �� ����ϸ�
                        // �ȵǴ� ������ ��������� �д�.
                        //---------------------------------------------
                        if ( ( sPredCur->lflag & QTC_NODE_SUBQUERY_MASK )
                               == QTC_NODE_SUBQUERY_EXIST )
                        {
                            sqlInfo.setSourceInfo( aStatement,
                                                   & sPredCur->position );
                            IDE_RAISE(ERR_OUTER_JOINED_TO_SUBQUERY);
                        }
                        else
                        {
                            // Nothing to do.
                        }

                        //---------------------------------------------
                        // Style ���� ��ȯ �۾� ����
                        //
                        // sPredCur �� �����Ͽ� sPredCur �� ��ȯ�Ѵ�.
                        // sPredCur �� ��ȯ�� ����� sTransStart �� sTransEnd ��
                        // ��Ƽ� return �Ѵ�.
                        //---------------------------------------------

                        if ( ( sPredCur->node.module == & mtfBetween )
                          || ( sPredCur->node.module == & mtfNotBetween ) )
                        {
                            IDE_TEST( transJoinOperBetweenStyle( aStatement,
                                                                 aQuerySet,
                                                                 sPredCur,
                                                                 & sTransStart,
                                                                 & sTransEnd )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( transJoinOperListArgStyle( aStatement,
                                                                 aQuerySet,
                                                                 sPredCur,
                                                                 & sTransStart,
                                                                 & sTransEnd )
                                      != IDE_SUCCESS );
                        }

                        //---------------------------------------------
                        // ��ȯ �Ϸ��� ���� Predicate �� OR ��忡��
                        // ���� ����
                        //---------------------------------------------

                        if ( sPredCur == (qtcNode *)sCurOR->node.arguments )
                        {
                            sCurOR->node.arguments = sPredCur->node.next;
                        }
                        else
                        {
                            sPredPrev->node.next = sPredCur->node.next;
                        }

                        //---------------------------------------------
                        // OR ��尡 ������ �ִ� ��� Predicate �� ��ȯ�Ǿ
                        // ���̻� argument �� ���ٸ� OR ���� ���� normalCNF����
                        // ���ŵǾ�� �Ѵ�.
                        //---------------------------------------------

                        if ( sCurOR->node.arguments == NULL )
                        {
                            if ( sCurOR == (qtcNode *)aTrans->normalCNF->node.arguments )
                            {
                                aTrans->normalCNF->node.arguments = sCurOR->node.next;
                            }
                            else
                            {
                                sPrevOR->node.next = sCurOR->node.next;
                            }
                        }
                        else
                        {
                            // Nothing to do.
                        }

                        //---------------------------------------------
                        // ��ȯ�� ��� ������ �ϴ� �������� ������ ��� �����صд�.
                        //
                        //  (OR) -> (OR) -> ... -> (OR) -> (OR) -> NULL
                        //   ^                              ^
                        //   |                              |
                        // sTmpTransStart               sTmpTransEnd
                        //---------------------------------------------

                        if ( sIsTransFirst == ID_FALSE )
                        {
                            //---------------------------------------------
                            // normalCNF �� ��Ʋ�� ó�� ��ȯ�� �߻����� ��
                            //---------------------------------------------
                            sTmpTransStart = sTransStart;
                            sTmpTransEnd   = sTransEnd;

                            sIsTransFirst = ID_TRUE;
                        }
                        else
                        {
                            sTmpTransEnd->node.next = (mtcNode *)sTransStart;
                            sTmpTransEnd = sTransEnd;
                        }

                        //---------------------------------------------
                        // ������ OR ��� ���Ͽ��� ��ȯ�� Predicate ��
                        // �־��ٴ� ���� ǥ���ص�.
                        //---------------------------------------------

                        sIsTransformedOR = ID_TRUE;
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                //---------------------------------------------
                // Nothing to do.
                //
                // (+) �� ���� Predicate �� ���� ANSI ���·�
                // ��ȯ�� �Ŀ��� �״�� normalCNF �� �����ְ�
                // �ǹǷ� ���� �������.
                //---------------------------------------------
            }
        } // for sPredCur

        //---------------------------------------------
        // ������ OR ��� ������ Predicate �߿��� ��ȯ��
        // �߻��ߴٸ� OR ��忡 ���� �ٽ� estimation
        // �׸���, ���ŵ��� ���� ������ OR ��带 sPrevOR ��
        // ����صд�.
        // �̴� �Ʒ��� ���� ���ῡ�� (OR 2)�� ��ȯ�� ���
        // (OR 1)�� ����ص����ν� (OR 2)�� ����Ʈ�� ���ῡ��
        // ���� �����̴�.
        //
        // (OR 1) -> (OR 2) -> (OR 3)
        //---------------------------------------------
        if ( sIsTransformedOR == ID_TRUE )
        {
            if ( sCurOR->node.arguments != NULL )
            {
                sPrevOR = sCurOR;

                IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                         sCurOR )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sPrevOR = sCurOR;
        }
    } // for sCurOR


    //---------------------------------------------
    // ��ȯ�� �ѹ��̶� �߻��ߴٸ� �ӽ÷� �����ص�
    // sTmpTransStart �� sTmpTransEnd �� ���� ���̴�.
    // �̸� ������ normalCNF �� �Ǿտ� �������ش�.
    //---------------------------------------------

    if ( sIsTransFirst == ID_TRUE )
    {
        if ( ( sTmpTransStart == NULL )
          || ( sTmpTransEnd == NULL )
          || ( aTrans->normalCNF == NULL ) )
        {
            IDE_RAISE( ERR_INVALID_TRANSFORM_RESULT );
        }
        else
        {
            sTmpTransEnd->node.next = aTrans->normalCNF->node.arguments;
            aTrans->normalCNF->node.arguments = (mtcNode *)sTmpTransStart;

            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     aTrans->normalCNF )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_TRANSFORM_RESULT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transNormalCNF4SpecialPred",
                                  "normalCNF transform error" ));
    }
    IDE_EXCEPTION( ERR_INVALID_OPERATION_WITH_LISTS );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_INVALID_OPERATION_WITH_LISTS,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_OUTER_JOINED_TO_SUBQUERY );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_OUTER_JOINED_TO_SUBQUERY,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::validateJoinOperConstraints( qcStatement * aStatement,
                                               qmsQuerySet * aQuerySet,
                                               qtcNode     * aNormalCNF )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *    transNormalCNF4SpecialPred �� ���� ��ȯ�� �Ϸ�� Tree ��
 *    �� Predicate �� ���� ������� �˻�
 *
 * Implementation :
 *
 *    Predicate ������ �˻簡 �Ұ����� ��������� Predicate ����
 *    ���谡 �ϼ��Ǵ� transformStruct2ANSIStyle ������ �����Ѵ�.
 *
 *    (AND)
 *      |
 *      V
 *    (OR)  ->  (OR)  ->  (OR)  -> ...
 *      |         |         |
 *      V         V         V
 *    (Pred)    (Pred)    (Pred)   <--- ���� ������� �˻�
 *
 *
 *   (+) ��� �� ��������� �˻��ϴ� ��Ģ�� �˾ƺ���.
 *
 *   ��� Predicate �� �ϳ��� ����� �ٸ� ����� �񱳸�
 *   �ϴ� ���̴�. (��, selectivity �� �����Ѵ�.)
 *   �� ���� ����ü�� �񱳴���� �ݵ�� �����Ѵٴ� ���̴�.
 *   ��, =, between, in, >=, ... �� ��� Predicate �����ڴ�
 *   ��� ����ü�� �񱳴���� �ʿ��ϴ�.
 *
 *   (+) �� ������� �����Ͽ� �̷��� �񱳿��� �߿��� ����
 *   Join ���踦 ���� ���̴�.
 *   ��, ����ü�� �񱳴���� constant value �� �ƴ� ��츦
 *   ���Ѵ�.
 *   ���ݺ��� ����ü�� Join Source, �񱳴���� Join Target
 *   �̶�� ��Ī�ϰ� �����ϵ��� �Ѵ�.
 *   (���� Inner/Outer ���̺��̶�� �� ���������
 *    ���� �Ϲ����� ���� ���� �����Ѵ�.)
 *
 *
 *   (+)�� ����ϰ� Join ���踦 ������ Predicate ����
 *   ��Ȯ�� ��������� ������ ������ ����̴�.
 *
 *
 *   (1) Join Source �� Join Target ���ʿ� (+)�� ����� �� ����.
 *       ex> t1.i1(+) = t2.i1(+), 1 = t2.i1(+)+t3.i1(+)
 *
 *   (2) (+) �� ����ϴ� Join Source �� ���� Join Target ���̺���
 *       2 �� �̻��� �� ����.
 *       ex> t1.i1(+) = t2.i1+t3.i1, t1.i1 = t2.i1(+)+t3.i1
 *
 *   (3) (+) �� ����ϴ� ���̺��� �ڱ��ڽŰ� Outer Join �� �� ����.
 *       �Ʒ� �ι�° ex �� Join �� ������ �ŵ��Ͽ� �ᱹ �ڽŰ�
 *       ����Ǵ� ����̴�.
 *       ex> t1.i1(+) = t1.i2, t1.i2-t1.i1(+) = 10
 *       ex> t1.i1(+) = t2.i1 and t2.i1(+) = t3.i1 and t3.i1(+) = t1.i1
 *
 *
 *   ���� ���� Predicate ������ �ܼ������ Rule �� ������ �� �ִ� ����
 *   �̹� Between ���� List Argument �� ������ �񱳿����ڷ� ���� �̹�
 *   ��ȯ�۾��� �� ���·� �˻縦 �ϱ� �����̴�. (transNormalCNF4SpecialPred)
 ***********************************************************************/

    qcuSqlSourceInfo    sSqlInfo;
    qmsFrom           * sFrom    = NULL;
    qtcNode           * sCurOR   = NULL;
    qtcNode           * sCurPred = NULL;
    qtcNode           * sPred    = NULL;
    SInt                sJoinOperCount;
    SInt                sDepCount;
    UInt                sArgCount = 0;
    idBool              sIsArgCountMany = ID_FALSE;
    qcDepInfo           sCompareDep;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::validateJoinOperConstraints::__FT__" );

    IDE_FT_ASSERT( aStatement   != NULL );
    IDE_FT_ASSERT( aQuerySet    != NULL );
    IDE_FT_ASSERT( aNormalCNF   != NULL );


    //------------------------------------------------------
    // Outer Join Operator �� ANSI Join ������ �Բ� ���� �� ����.
    // �ٸ�, �̴� Where ������ �ش�ǰ� �ٸ� ���� ���Ե� ����
    // �׳� �����ϸ� �ȴ�. (on ���� ��������)
    //------------------------------------------------------

    for ( sFrom  = aQuerySet->SFWGH->from;
          sFrom != NULL;
          sFrom  = sFrom->next)
    {
        if ( sFrom->joinType != QMS_NO_JOIN )
        {
            sSqlInfo.setSourceInfo( aStatement,
                                   & aQuerySet->SFWGH->where->position );
            IDE_RAISE(ERR_JOIN_OPER_NOT_ALLOWED_WITH_ANSI_JOIN );
        }
        else
        {
            // Nothing to do.
        }
    }

    for ( sCurOR  = (qtcNode *)aNormalCNF->node.arguments;
          sCurOR != NULL;
          sCurOR  = (qtcNode *)sCurOR->node.next )
    {
        sPred = (qtcNode *)sCurOR->node.arguments;
        sArgCount = 1;

        while ( sPred->node.next != NULL )
        {
            ++sArgCount;
            sPred = (qtcNode *)sPred->node.next;
        }

        if ( sArgCount > 1 )
        {
            sIsArgCountMany = ID_TRUE;
        }
        else
        {
            sIsArgCountMany = ID_FALSE;
        }

        //------------------------------------------------------
        // OR �� argument ��尡 �ټ��� �� dependency ������ �񱳸�
        // ���� ������ �д�.
        // OR ������ �� predicate ���� dependency ������ ���� �޶󼭴� �ȵȴ�.
        // ���� ANSI ���������� ��ȯ �� OR ��尡 ��°�� �ϳ��� predicate ó��
        // ������ �� �ִ� �����̾�� ����� Ʋ������ �ʴ´�.
        //------------------------------------------------------
        qtc::dependencySetWithDep( & sCompareDep, & ((qtcNode*)sCurOR->node.arguments)->depInfo );

        for ( sCurPred  = (qtcNode *)sCurOR->node.arguments;
              sCurPred != NULL;
              sCurPred  = (qtcNode *)sCurPred->node.next )
        {
            //------------------------------------------------------
            // Predicate Level ���� Outer Join Operator �� �ִ� �͸�
            // ��������� �˻��ϸ� �ȴ�. (�⺻������� �˻�)
            //------------------------------------------------------

            if ( ( sCurPred->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                   == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                //------------------------------------------------------
                // terminal predicate ���� 2 �� �̻��� dependency table��
                // (+)�� ������� ���
                //------------------------------------------------------
                sJoinOperCount = qtc::getCountJoinOperator( &sCurPred->depInfo );

                if ( sJoinOperCount > QMO_JOIN_OPER_TABLE_CNT_PER_PRED )
                {
                    sSqlInfo.setSourceInfo( aStatement,
                                            & sCurPred->position );
                    IDE_RAISE(ERR_TOO_MANY_REFERENCE_OUTER_JOINED_TABLE);
                }
                else
                {
                    // Nothing to do.
                }

                //------------------------------------------------------
                // terminal predicate ���� (+) �� ����ϸ鼭
                // dependency table �� 2 ���� ���� �� ����.
                //------------------------------------------------------
                sDepCount = qtc::getCountBitSet( & sCurPred->depInfo );

                if ( sDepCount > QMO_JOIN_OPER_DEPENDENCY_TABLE_CNT_PER_PRED )
                {
                    sSqlInfo.setSourceInfo( aStatement,
                                            & sCurPred->position );
                    IDE_RAISE(ERR_TOO_MANY_REFERENCE_OUTER_JOINED_TABLE);
                }
                else
                {
                    // Nothing to do.
                }

                //------------------------------------------------------
                // predicate ������ t1.i1(+) - t1.i2 = 1 �� ���� �� ���̺���
                // ���� Outer Join �� ����� �� �� ����.
                //------------------------------------------------------
                if( qtc::isOneTableOuterEachOther( &sCurPred->depInfo )
                    == ID_TRUE )
                {
                    sSqlInfo.setSourceInfo( aStatement,
                                            & sCurPred->position );
                    IDE_RAISE(ERR_NOT_ALLOW_OUTER_JOIN_EACH_OTHER);
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

            //------------------------------------------------------
            // 1. OR ��� ���Ͽ��� outer join operator �� ������ ����
            //    ���� argument �� 1 ���� ���� ���� �������.
            // 2. OR ��� ���Ͽ��� outer join operator �� ���Ǿ��� ���,
            //    dependency table �� 2 �� �̻��� �� ����.
            // 3. OR ��� ���Ͽ��� outer join operator �� ���ȴٸ� dependency table ��
            //    1 ���� �� �ۿ� �����Ƿ�, OR ����� argument �� ������ ���
            //    outer join operator �� ���� ���� dependency table �̾���Ѵ�.
            //    dependency table �� 1 ���̸鼭 �� predicate ���� ���� dependency table ��
            //    ���� outer join operator ������ �޶󼭴� �ȵȴ�.
            //
            //    ��, ���� ��Ģ���� ������ ����ϸ�,
            //
            //    "OR ���� ������ ��� ��� �ϳ��� prediicate ���� ����ؾ� �Ѵ�.
            //     �ٸ��� ���ϸ�, ANSI outer join �������� �������� �� OR ������
            //     Ư�� argument �� on ���� �Ű����� � argument �� �״�� ����
            //     �� ���� ���ٴ� ���̴�.
            //     ��� argument �� �Բ� �Űܰ��ܳ� �����ְų� �� ���� �ϳ���.
            //     �̷��� Rule �� �����ϸ� ������ ������ �߻���Ű�� �ȴ�."
            //------------------------------------------------------
            if ( ( sCurOR->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                   == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                if ( sIsArgCountMany == ID_TRUE )
                {
                    sDepCount = qtc::getCountBitSet( & sCurPred->depInfo );

                    if ( sDepCount > 1 )
                    {
                        sSqlInfo.setSourceInfo( aStatement,
                                                & sCurPred->position );
                        IDE_RAISE(ERR_JOIN_OPER_NOT_ALLOWED_IN_OPERAND_OR);
                    }
                    else
                    {
                        if ( qtc::dependencyJoinOperEqual( & sCompareDep,
                                                           & sCurPred->depInfo )
                             == ID_FALSE )
                        {
                            sSqlInfo.setSourceInfo( aStatement,
                                                    & sCurPred->position );
                            IDE_RAISE(ERR_JOIN_OPER_NOT_ALLOWED_IN_OPERAND_OR);
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
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_JOIN_OPER_NOT_ALLOWED_WITH_ANSI_JOIN );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_ALLOWED_WITH_ANSI_JOIN,
                                  sSqlInfo.getErrMessage() ) );
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_TOO_MANY_REFERENCE_OUTER_JOINED_TABLE );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_TOO_MANY_REFERENCE_OUTER_JOINED_TABLE,
                                  sSqlInfo.getErrMessage() ) );
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_JOIN_OPER_NOT_ALLOWED_IN_OPERAND_OR );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_ALLOWED_IN_OPERAND_OR,
                                  sSqlInfo.getErrMessage() ) );
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_OUTER_JOIN_EACH_OTHER );
    {
        (void)sSqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_ALLOW_OUTER_JOIN_EACH_OTHER,
                                  sSqlInfo.getErrMessage() ) );
        (void)sSqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::transformStruct2ANSIStyle( qcStatement      * aStatement,
                                             qmsQuerySet      * aQuerySet,
                                             qmoJoinOperTrans * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Outer Join Operator�� ����ϴ� where ���� ANSI ���·� ��ȯ
 *
 * Implementation :
 *
 *    �� �Լ������� normalCNF �� ��� Predicate �� Outer Join Operator ����,
 *    Join/constant/onetable Predicate ����, �� Predicate ����
 *    dependency table �鰣�� ���踦 �ľ��Ͽ� �з� �� Grouping,
 *    normalCNF �� From Tree ��ȯ�� �����Ѵ�.
 *
 *    �� �Լ������� ������ ���� ���� �����Ѵ�.
 *
 *     1. Predicate �� ������ 3 ������ �з�
 *        ��. (+)���� predicate           : qmoJoinOperTrans->generalPred
 *        ��. (+)�ְ� one table predicate : qmoJoinOperTrans->oneTablePred
 *        ��. (+)�ְ� join predicate      : qmoJoinOperTrans->joinOperPred
 *     2. qmoJoinOperTrans->joinOperPred �� Grouping
 *        : qmoJoinOperTrans->joinOperGroup
 *     3. qmoJoinOperTrans->joinOperGroup ���� dependency info �� Relationing
 *        ��. �ߺ� Relation ����
 *        ��. qmoJoinOperTrans->joinOperGroup->joinRelation ��
 *            depend(TupleId) ���� ���� ������
 *     4. qmoJoinOperTrans �� predicate, grouping, relation ������ �̿��Ͽ�
 *        ANSI ���·� ��ȯ
 *
 *
 *    ��ȯ�������� �Ʒ��� �ڷᱸ���� ��� ����� �̿���� ���캸��.
 *
 *    -------------------------------------------------------
 *    *  qmoJoinOperGroup ���� joinRelation �� ���������δ� *
 *    *  Array �����̴�.                                    *
 *    *  ������ �����ϰ� �ϱ� ���� list �� ��ó�� �����Ѵ�. *
 *    -------------------------------------------------------
 *
 *    ================
 *    qmoJoinOperTrans    ----> (*pred1) -> (*pred2) -> ...
 *    ================    |
 *    | oneTablePred ------
 *    | joinOperPred ---------> (*pred5) -> (*pred4) -> (*pred3) -> ...
 *    | generalPred  ------
 *    |              |    |
 *    |              |    ----> (*pred6) -> (*pred7) -> ...
 *    | joinOperGroup------
 *    ================    |     ===================
 *                        ----> | qmoJoinOperGroup|  --> (*pred5) -> (*pred3) ->
 *                        |     ===================  |
 *                        |     | joinPredicate   ----
 *                        |     | joinRelation    -----> (*pred3) -> (*pred5) ->
 *                        |     ===================
 *                        |     ===================
 *                        ----> | qmoJoinOperGroup|  --> (*pred4) ->
 *                              ===================  |
 *                              | joinPredicate   ----
 *                              | joinRelation    -----> (*pred4) ->
 *                              ===================
 *
 *
 *    ������ ���� ������ ���� SQL �� �ִٰ� ��������.
 *
 *    where pred1  -> one table (+) predicate
 *      and pred2  -> one table (+) predicate
 *      and pred3  -> (+) join predicate
 *      and pred4  -> (+) join predicate
 *      and pred5  -> (+) join predicate
 *      and pred6  -> none (+) predicate
 *      and pred7  -> none (+) predicate
 *
 *
 *    (1) Predicate �� �з�
 *        ���� �� predicate �� ���� qtcNode �� depInfo ������ ����
 *        ������ ���� ������ �ڷᱸ�� ����Ʈ�� �����صд�.
 *
 *        ================
 *        qmoJoinOperTrans    ----> (*pred1) -> (*pred2) -> NULL
 *        ================    |
 *        | oneTablePred ------
 *        | joinOperPred ---------> (*pred5) -> (*pred4) -> (*pred3) -> NULL
 *        | generalPred  ------
 *        |              |    |
 *        |              |    ----> (*pred6) -> (*pred7) -> NULL
 *        ================
 *
 *    (2) (+) �� �ִ� Join Predicate ���� Grouping
 *        ������ �з��� ����Ʈ �� (+) �� �����ϰ� Join ���踦 ���� Predicate
 *        ���� ������ �ִ� joinOperPred ����Ʈ�� ������ Grouping �Ѵ�.
 *        (Join ���谡 �ִ� �͵鳢�� ���´ٴ� ���̴�)
 *
 *        joinOperPred ����Ʈ�� ��带 ��ȸ�ϸ� join ���谡 �ִ� �͵���
 *        ���ʷ� ������ qmoJoinOperGroup �� joinPredicate �� �����ϰ�
 *        ������ joinOperPred ������ �����Ѵ�.
 *
 *        ================
 *        qmoJoinOperTrans
 *        ================
 *        |              |
 *        | joinOperPred ---------> NULL
 *        |              |
 *        | joinOperGroup----
 *        ================  |     ===================
 *                          ----> | qmoJoinOperGroup|  --> (*pred5) -> (*pred3) -> NULL
 *                          |     ===================  |
 *                          |     | joinPredicate   ----
 *                          |     | joinRelation    -----> NULL
 *                          |     ===================
 *                          |     ===================
 *                          ----> | qmoJoinOperGroup|  --> (*pred4) -> NULL
 *                                ===================  |
 *                                | joinPredicate   ----
 *                                | joinRelation    -----> NULL
 *                                ===================
 *
 *    (3) Join Relationning
 *        (2) ���� Grouping �� ���� �ϼ��� �� Group �� joinPredicate ��
 *        ����Ʈ��带 ��ȸ�ϸ鼭 depInfo->depend ���� ���� �����ϰ�(Quick Sort),
 *        �ߺ��� dependency ������ ������ Predicate �� ���� �� ��
 *        joinRelation ����Ʈ�� �����Ѵ�.
 *                     
 *        ================
 *        qmoJoinOperTrans
 *        ================
 *        |              |
 *        | joinOperGroup----
 *        ================  |     ===================
 *                          ----> | qmoJoinOperGroup|  --> (*pred5) -> (*pred3) -> NULL
 *                          |     ===================  |
 *                          |     | joinPredicate   ----
 *                          |     | joinRelation    -----> (*pred3) -> (*pred5) -> NULL
 *                          |     ===================
 *                          |     ===================
 *                          ----> | qmoJoinOperGroup|  --> (*pred4) -> NULL
 *                                ===================  |
 *                                | joinPredicate   ----
 *                                | joinRelation    -----> (*pred4) -> NULL
 *                                ===================
 *
 *   (4) joinRelation ������ �̿��� ��ȯ
 *       ���� ���������� ��ȯ�۾��� �ؾ��Ѵ�.
 *       ��ȯ����� qmsSFWGH->from Tree �� qmoJoinOperTrans->normalCNF Tree �̴�.
 *       ������ �������� ��ȯ�۾��� �����Ѵ�.
 *
 *       (��) ù��° joinRelation ����� dependency ������ �̿��Ͽ�
 *            From ����Ʈ���� ���� ��带 ã�´�.
 *
 *       (��) ã�Ƴ� From ������ Left outer join ��� Tree ���·�
 *            ��ȯ�Ѵ�.
 *
 *       (��) ��ȯ�� Left outer join ����� onCondition �� ������
 *            Predicate �� ��� ã�Ƽ� �������ش�.
 *
 *            a. joinRelation �� �����ϴ� joinPredicate �� ã�Ƽ�
 *               onCondition �� ���� �� ����Ʈ���� �����Ѵ�.
 *            b. (1)���� �з��� �ξ��� oneTablePred �߿��� ���õ�
 *               ���� onCondtion �� ������ �� ����Ʈ���� �����Ѵ�.
 *
 *       (��) ���� joinRelation ��忡 ���� (��)�� �������� �ݺ��Ѵ�.
 *
 *       ** joinPredicate �� oneTablePred ��带 ����Ʈ���� ������ ����
 *          �ش� ��尡 ����Ű�� Predicate ��� ���� normalCNF ����
 *          ������ �־�� �Ѵ�.
 *          �ᱹ, where ������ ���� ������� ��� predicate ������
 *          ������ �ִ� normalCNF ��, ��ȯ�۾��� ������ Left Outer Join
 *          �� onCondition ���� �Ű��� ��带 ������ �͵鸸 ���� �ȴ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transformStruct2ANSIStyle::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aTrans     != NULL );


    //------------------------------------------
    // �� �ܰ迡�� max join group �� �˱�� �����
    // Query �� dependency table �� ������ max �� ��´�.
    //------------------------------------------

    // BUG-43897 �߸��� predicate�� ����Ͽ� ����� �����д�.
    aTrans->maxJoinOperGroupCnt = qtc::getCountBitSet( &aQuerySet->SFWGH->depInfo ) * 4;

    //------------------------------------------
    // Predicate�� �з�
    //------------------------------------------

    IDE_TEST( classifyJoinOperPredicate( aStatement, aQuerySet, aTrans )
              != IDE_SUCCESS );

    //------------------------------------------
    // Predicate �� �з��� �� joinOperPred �� ���ٸ�,
    // �Ʒ��� ������ ���ʿ��ϴ�.
    // ��, Outer Join Operator �� ����ߴ���,
    // constant Predicate �� one table predicate ��
    // �ִ� ���� ANSI ������ ������ ������ ����.
    //------------------------------------------

    if ( aTrans->joinOperPred != NULL )
    {
        //------------------------------------------
        // joinOperGroup �迭 ���� Ȯ�� �� �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoJoinOperGroup ) * aTrans->maxJoinOperGroupCnt,
                                                   (void **) & aTrans->joinOperGroup )
                  != IDE_SUCCESS );

        // joinOperGroup���� �ʱ�ȭ
        IDE_TEST( initJoinOperGroup( aStatement,
                                     aTrans->joinOperGroup,
                                     aTrans->maxJoinOperGroupCnt )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Join Predicate �� Grouping �� Relationing, Ordering
        //------------------------------------------

        IDE_TEST( joinOperOrdering( aStatement, aTrans )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Relationing �� List �� �̿��Ͽ� ANSI ���·� ��ȯ
        //  - From, normalCNF, On ��
        //------------------------------------------

        IDE_TEST( transformJoinOper( aStatement, aQuerySet, aTrans )
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


IDE_RC
qmoOuterJoinOper::isOneTableJoinOperPred( qmsQuerySet * aQuerySet,
                                          qtcNode     * aPredicate,
                                          idBool      * aIsTrue )
{
/***********************************************************************
 *
 * Description : one table predicate�� �Ǵ�
 *
 *     one table predicate: dependency table �� 1 ���� predicate
 *     ��) T1.i1 = 1
 *
 * Implementation :
 *
 ***********************************************************************/

    qcDepInfo  sAndDependencies;
    SInt       sDepCount = 0;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::isOneTableJoinOperPred::__FT__" );

    IDE_DASSERT( aPredicate        != NULL );
    IDE_DASSERT( aIsTrue           != NULL );

    // BUG-35468 �ܺ� ���� �÷��� depInfo�� �����Ѵ�.
    qtc::dependencyAnd( & aQuerySet->SFWGH->depInfo,
                        & aPredicate->depInfo,
                        & sAndDependencies );

    sDepCount = qtc::getCountBitSet( & sAndDependencies );

    if ( sDepCount == 1 )
    {
        *aIsTrue = ID_TRUE;
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoOuterJoinOper::isConstJoinOperPred( qmsQuerySet * aQuerySet,
                                       qtcNode     * aPredicate,
                                       idBool      * aIsTrue )
{
/***********************************************************************
 *
 * Description : constant predicate�� �Ǵ�
 *
 *     constant predicate�� dependency table �� ���� Predicate ���� ����
 *     ��)  1 = 1
 *
 * Implementation :
 *
 ***********************************************************************/

    qcDepInfo  sAndDependencies;
    SInt       sDepCount = 0;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::isConstJoinOperPred::__FT__" );

    IDE_DASSERT( aPredicate        != NULL );
    IDE_DASSERT( aIsTrue           != NULL );

    //--------------------------------------
    // constant predicate�� �Ǵ�
    // dependencies�� �ֻ��� ��忡�� �Ǵ��Ѵ�.
    //--------------------------------------

    // BUG-36358 �ܺ� ���� �÷��� depInfo�� �����Ѵ�.
    qtc::dependencyAnd( & aQuerySet->SFWGH->depInfo,
                        & aPredicate->depInfo,
                        & sAndDependencies );

    sDepCount = qtc::getCountBitSet( & sAndDependencies );

    if ( sDepCount == 0 )
    {
        *aIsTrue = ID_TRUE;
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoOuterJoinOper::createJoinOperPred( qcStatement      * aStatement,
                                      qtcNode          * aNode,
                                      qmoJoinOperPred ** aNewPredicate )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *    Outer Join Operator �� �����ϴ� Join Predicate �� ������ �ڷᱸ�� ����
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::createJoinOperPred::__FT__" );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoJoinOperPred ),
                                               (void**)aNewPredicate )
              != IDE_SUCCESS );

    (*aNewPredicate)->node = aNode;
    (*aNewPredicate)->next = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::initJoinOperGroup( qcStatement      * aStatement,
                                     qmoJoinOperGroup * aJoinGroup,
                                     UInt               aJoinGroupCnt )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Oper Group�� �ʱ�ȭ
 *
 * Implementation :
 *    Join Oper Group �� �ʱ�ȭ�ϸ鼭, �� Group ����
 *    joinRelation �� Array ���·� �̸� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    qmoJoinOperGroup * sCurJoinGroup = NULL;
    qmoJoinOperRel   * sCurJoinRel   = NULL;
    UInt               i, j;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::initJoinOperGroup::__FT__" );

    // �� Join Oper Group�� �ʱ�ȭ
    for ( i = 0;
          i < aJoinGroupCnt;
          i++ )
    {
        sCurJoinGroup = & aJoinGroup[i];

        sCurJoinGroup->joinPredicate = NULL;
        sCurJoinGroup->joinRelationCnt = 0;
        sCurJoinGroup->joinRelationCntMax = aJoinGroupCnt;

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoJoinOperRel ) * aJoinGroupCnt,
                                                   (void **)& sCurJoinGroup->joinRelation )
                  != IDE_SUCCESS );

        for ( j = 0;
              j < aJoinGroupCnt;
              j++ )
        {
            sCurJoinRel = & sCurJoinGroup->joinRelation[j];
            qtc::dependencyClear( & sCurJoinRel->depInfo );
        }

        qtc::dependencyClear( & sCurJoinGroup->depInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::classifyJoinOperPredicate( qcStatement       * aStatement,
                                             qmsQuerySet       * aQuerySet,
                                             qmoJoinOperTrans  * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : normalCNF�� Predicate �з�
 *
 * Implementation :
 *    qmoJoinOperTrans::normalCNF�� �� Predicate�� ���Ͽ� ������ ����
 *
 *    (1) Constant Predicate �з� -> generalPred �� ����
 *    (2) Outer Join Operator + One Table Predicate�� �з� -> oneTablePred �� ����
 *    (3) Outer Join Operator + Join Predicate�� �з� -> joinOperPred �� ����
 *    (4) Outer Join Operator �� ���� Predicate �з� -> generalPred �� ����
 *
 ***********************************************************************/

    qtcNode         * sNode    = NULL;
    qmoJoinOperPred * sNewPred = NULL;

    idBool            sIsConstant = ID_FALSE;
    idBool            sIsOneTable = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::classifyJoinOperPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTrans     != NULL );


    // Predicate �з� ��� Node
    if ( aTrans->normalCNF != NULL )
    {
        sNode = (qtcNode *)aTrans->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    while ( sNode != NULL )
    {
        // qmoJoinOperPred ����
        IDE_TEST( createJoinOperPred( aStatement,
                                      sNode,
                                      & sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Constant Predicate �˻�
        // constant Predicate �̸� ������ generalPred �� ����
        //-------------------------------------------------

        IDE_TEST( isConstJoinOperPred( aQuerySet,
                                       (qtcNode*)sNewPred->node,
                                       & sIsConstant )
                  != IDE_SUCCESS );

        if ( sIsConstant == ID_TRUE )
        {
            sNewPred->next      = aTrans->generalPred;
            aTrans->generalPred = sNewPred;
        }
        else
        {
            //---------------------------------------------
            // Predicate Node �� Outer Join Operator �� ������,
            // oneTable predicate �� twoTable predicate �� �з��Ͽ� ����.
            // Outer Join Operator �� ������ ������ generalPred �� ����.
            //---------------------------------------------

            if ( ( sNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                 == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                //---------------------------------------------
                // One Table Predicate �˻�
                //---------------------------------------------

                IDE_TEST( isOneTableJoinOperPred( aQuerySet,
                                                  (qtcNode*)sNewPred->node,
                                                  & sIsOneTable )
                          != IDE_SUCCESS );

                if ( sIsOneTable == ID_TRUE )
                {
                    sNewPred->next       = aTrans->oneTablePred;
                    aTrans->oneTablePred = sNewPred;
                }
                else
                {
                    //---------------------------------------------
                    // oneTablePredicate�� �ƴϸ�
                    // aTrans->joinOperPred�� ����
                    //---------------------------------------------

                    sNewPred->next       = aTrans->joinOperPred;
                    aTrans->joinOperPred = sNewPred;
                }
            }
            else
            {
                sNewPred->next      = aTrans->generalPred;
                aTrans->generalPred = sNewPred;
            }
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOuterJoinOper::joinOperGrouping( qmoJoinOperTrans  * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Group�� �з�
 *
 * Implemenation :
 *      qmoJoinOperTrans::joinOperPred �� �ִ� ��� Predicate ����
 *      Join Group ���� ����� qmoJoinOperTrans::joinOperGroup ��
 *      joinPredicate �� �����صд�.
 *
 ***********************************************************************/

    SInt                   sJoinGroupCnt;
    SInt                   sJoinGroupIndex;
    qmoJoinOperGroup     * sJoinGroup    = NULL;
    qmoJoinOperGroup     * sCurJoinGroup = NULL;
    qmoJoinOperPred      * sFirstPred    = NULL;
    qmoJoinOperPred      * sAddPred      = NULL;
    qmoJoinOperPred      * sCurPred      = NULL;
    qmoJoinOperPred      * sPrevPred     = NULL;

    qcDepInfo              sAndDependencies;
    idBool                 sIsInsert = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::joinOperGrouping::__FT__" );

    IDE_DASSERT( aTrans   != NULL );

    sJoinGroup       = aTrans->joinOperGroup;
    sJoinGroupIndex  = -1;
    sJoinGroupCnt    = 0;


    sFirstPred  = aTrans->joinOperPred;

    while ( sFirstPred != NULL )
    {

        //------------------------------------------
        // Join Group�� ������ �ʴ� ù��° Predicate ���� :
        //    Join Group�� ������ ���� ù��° Predicate��
        //    dependencies�� ���ο� Join Group�� dependencies�� ����
        //------------------------------------------

        ++sJoinGroupIndex;

        if ( sJoinGroupIndex >= (SInt)aTrans->maxJoinOperGroupCnt )
        {
            IDE_RAISE( ERR_UNEXPECTED_MODULE );
        }
        else
        {
            // Nothing to do.
        }

        sCurJoinGroup = & sJoinGroup[sJoinGroupIndex];
        sJoinGroupCnt = sJoinGroupIndex + 1;

        IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                     & sFirstPred->node->depInfo,
                                     & sCurJoinGroup->depInfo )
                  != IDE_SUCCESS );

        //------------------------------------------
        // ���� JoinGroup�� ������ ��� Join Predicate�� ã��
        // JoinGroup�� ���
        //------------------------------------------

        sCurPred  = sFirstPred;
        sPrevPred = NULL;
        sIsInsert = ID_FALSE;

        while ( sCurPred != NULL )
        {

            qtc::dependencyAnd( & sCurJoinGroup->depInfo,
                                & sCurPred->node->depInfo,
                                & sAndDependencies );

            if ( qtc::dependencyEqual( & sAndDependencies,
                                       & qtc::zeroDependencies )
                 == ID_FALSE )
            {
                //------------------------------------------
                // Join Group�� ������ Predicate
                //------------------------------------------

                // Join Group�� ���Ӱ� ����ϴ� Predicate�� ����
                sIsInsert = ID_TRUE;

                // Join Group�� ���
                IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                             & sCurPred->node->depInfo,
                                             & sCurJoinGroup->depInfo )
                          != IDE_SUCCESS );

                // Predicate�� Join Predicate���� ������ ����,
                // JoinGroup�� joinPredicate�� �����Ŵ

                if ( sCurJoinGroup->joinPredicate == NULL )
                {
                    sCurJoinGroup->joinPredicate = sCurPred;
                    sAddPred                     = sCurPred;
                }
                else
                {
                    IDE_FT_ASSERT( sAddPred != NULL );
                    sAddPred->next = sCurPred;
                    sAddPred       = sAddPred->next;
                }
                sCurPred       = sCurPred->next;
                sAddPred->next = NULL;

                if ( sPrevPred == NULL )
                {
                    sFirstPred = sCurPred;
                }
                else
                {
                    sPrevPred->next = sCurPred;
                }
            }
            else
            {
                // Join Group�� �������� ���� Predicate : nothing to do
                sPrevPred = sCurPred;
                sCurPred = sCurPred->next;
            }

            if ( ( sCurPred == NULL ) && ( sIsInsert == ID_TRUE ) )
            {
                //------------------------------------------
                // Join Group�� �߰� ����� Predicate�� �����ϴ� ���,
                // �߰��� Predicate�� ������ Join Predicate�� ������ �� ����
                //------------------------------------------

                sCurPred  = sFirstPred;
                sPrevPred = NULL;
                sIsInsert = ID_FALSE;
            }
            else
            {
                // �� �̻� ������ Predicate ���� : nothing to do
            }
        }
    }

    // ���� Join Group ���� ����
    aTrans->joinOperGroupCnt = sJoinGroupCnt;

    // aTrans�� joinOperPred�� joinGroup�� joinPredicate���� ��� �з���Ŵ
    aTrans->joinOperPred = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_MODULE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::joinOperGrouping",
                                  "join group count > max" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

extern "C" SInt
compareJoinOperRel( const void * aElem1,
                    const void * aElem2 )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Relation �� Quick Sort
 *
 * Implementation :
 *    ���� ������ ���� Join Relation ���� ��ϵǾ� ���� ��� ��
 *    table ���� ���� �����Ѵ�.
 *    (ù��°,�ι�° table ���� ����)
 *
 *    ������
 *    (2,3) -> (1,3) -> (2,5) -> (3,4) -> (1,2)
 *
 *    ������
 *    (1,2) -> (1,3) -> (2,3) -> (2,5) -> (3,4)
 *
 *   qmoJoinOperGroup->joinRelation �� array.
 *
 ***********************************************************************/

    if( qtc::getDependTable( &((qmoJoinOperRel *)aElem1)->depInfo,0 ) >
        qtc::getDependTable( &((qmoJoinOperRel *)aElem2)->depInfo,0 ) )
    {
        return 1;
    }
    else if( qtc::getDependTable( &((qmoJoinOperRel *)aElem1)->depInfo,0 ) <
             qtc::getDependTable( &((qmoJoinOperRel *)aElem2)->depInfo,0 ) )
    {
        return -1;
    }
    else
    {
        if( qtc::getDependTable( &((qmoJoinOperRel *)aElem1)->depInfo,1 ) >
            qtc::getDependTable( &((qmoJoinOperRel *)aElem2)->depInfo,1 ) )
        {
            return 1;
        }
        else if( qtc::getDependTable( &((qmoJoinOperRel *)aElem1)->depInfo,1 ) <
                 qtc::getDependTable( &((qmoJoinOperRel *)aElem2)->depInfo,1 ) )
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }
}

IDE_RC
qmoOuterJoinOper::joinOperRelationing( qcStatement          * aStatement,
                                       qmoJoinOperGroup     * aJoinGroup )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Group ���� Join Relation ����
 *
 * Implementation :
 *    �ϳ��� Join Group �� argument �� �޾Ƽ� �ش� Join Group ��
 *    joinPredicate �� ����� ��� ������ �ߺ��˻�, Sorting �� ����
 *    joinRelation �� ������� �����Ѵ�.
 *
 ***********************************************************************/

    qcuSqlSourceInfo     sqlInfo;
    qmoJoinOperPred    * sCurPred         = NULL;
    qmoJoinOperRel     * sCurRel          = NULL;
    qmoJoinOperRel     * sConnectRel      = NULL;
    qmoJoinOperRel     * sStartRel        = NULL;
    qmoJoinOperRel     * sAddRelPosition  = NULL;
    qtcNode            * sErrNode         = NULL;
    idBool               sExist           = ID_FALSE;
    qmoJoinOperRel       sTempRel;
    qcDepInfo            sAndDependencies;
    qcDepInfo            sConnectAndDep;
    qcDepInfo            sTotalRelDepOR;
    qcDepInfo            sCounterDep;
    UInt                 i, j;
    UChar                sJoinOper;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::joinOperRelationing::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aJoinGroup != NULL );

    //------------------------------------------
    // ����>
    // aJoinGroup->joinPredicate �� ����Ǿ� �ִ�
    // ��� Predicate �� dependency table �� 2��,
    // Outer Join operator �� 1 �� ������ �ִٴ� ���̺���ȴ�.
    //
    // �׷��� ���� �͵��� �̹� Predicate �� ������� �˻�
    // �������� �ɷ��� �����̴�.
    //------------------------------------------

    for ( sCurPred  = aJoinGroup->joinPredicate;
          sCurPred != NULL;
          sCurPred  = sCurPred->next )
    {
        //------------------------------------------
        // �̹� joinRelation �� �߰��� �Ͱ� depInfo �� �ߺ��Ǵ��� �˻�
        //------------------------------------------

        for ( i = 0 ; i < aJoinGroup->joinRelationCnt ; i++ )
        {
            sCurRel = (qmoJoinOperRel *)&aJoinGroup->joinRelation[i];

            if ( qtc::dependencyJoinOperEqual( & sCurRel->depInfo,
                                               & sCurPred->node->depInfo )
                 == ID_TRUE )
            {
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // �ߺ��� Predicate �� Relation �� ������
                sExist = ID_FALSE;
            }
        }

        if ( sExist == ID_FALSE )
        {
            IDE_FT_ASSERT( aJoinGroup->joinRelationCnt < aJoinGroup->joinRelationCntMax );

            //------------------------------------------
            // ���ο� Predicate �� Relation �� �߰��ϰ�
            // dependency ������ Oring �߰�
            //------------------------------------------
            sAddRelPosition = (qmoJoinOperRel *)&aJoinGroup->joinRelation[aJoinGroup->joinRelationCnt];
            qtc::dependencySetWithDep( & sAddRelPosition->depInfo, & sCurPred->node->depInfo );
            sAddRelPosition->node = sCurPred->node;

            ++aJoinGroup->joinRelationCnt;
        }
        else
        {
            // Nothing to do.
        }
    }

    //------------------------------------------
    // joinRelation �� ���� Predicate �� �߰��� �Ϸ�Ǿ����Ƿ�
    // joinRelation �������� table ���� ���� ������ �����Ѵ�.
    // ������ �����ϴ� ������ �ڿ��� ��ȯ���� ��
    // From �� Left Outer Join Tree �� �����ϰ� �Ǵµ�,
    // �� �� ������ �������� ã�� �� �ֵ��� �ϱ� �����̴�.
    //
    //    ������
    //    (2,3) -> (1,3) -> (2,5) -> (3,4) -> (1,2)
    //
    //    ������
    //    (1,2) -> (1,3) -> (2,3) -> (2,5) -> (3,4)
    //------------------------------------------

    sStartRel = (qmoJoinOperRel *)&aJoinGroup->joinRelation[0];

    if ( aJoinGroup->joinRelationCnt > 1 )
    {
        idlOS::qsort( sStartRel,
                      aJoinGroup->joinRelationCnt,
                      ID_SIZEOF(qmoJoinOperRel),
                      compareJoinOperRel );
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // sTotalRelDepOR �� �Ʒ����� ���ʴ�� �����Ǵ�
    // Relation ���� depInfo ������ ���ʷ� Oring �ؼ�
    // ������ �ֱ� ���� ���̴�.
    //------------------------------------------
    qtc::dependencyClear( & sTotalRelDepOR );

    //------------------------------------------
    // Relation �� �߿��� �ʼ������� �ϳ� �ִ�.
    // ���� Join Group ���� �� Relation ���� �ݵ�� �������� �ִٴ� ���̴�.
    // �� ���ǰ��� �������� ���ٸ� ���� Join Group ���� ���Ե� ��
    // ���� ���̴�.
    // �̿� ���Ҿ� joinRelation �� �������� ���� qsort �� ����
    // ���ĵǾ� �ֱ⸸ �ϸ� �ȵȴ�.
    // �� ����� �ٷ� �� ����� �������� ������ ���� ��ȯ��������
    // From Tree �� ���� �� �ִ�.
    //
    // ��, ������ ��츦 �����غ���.
    //    _      _    _
    // (1,4)->(2,3)->(2,4)
    //
    // qsort ���� Ư�� Join Group �� Relation �� ���� ���ٰ� �� ��,
    // �̸� �״�� �̿��ϸ� �ΰ��� �������� �ִ�.
    //         _        _
    //   1. (1,4) �� (2,3) �� tuple id �� ���� �������� �����Ƿ�
    //      ���� From Tree �� ������ �� �������� ã�� �� ���
    //      ANSI ���·� ��ȯ�� �����Ѵ�.
    //   2. �Ʒ��ʿ��� Join Looping �� ��Ȯ�ϰ� �˻��ϱ� ���ؼ���
    //      �������� ����� ������ �Ǿ� �־�� �Ѵ�.
    //                     _
    //      �׷��� ������ (2,4) predicate �� ���� �˻縦 �� ��,
    //       _                 _
    //      (2,4) �� �ݴ��� (2,4) �� �̹� sTotalRelDepOR �� �ֱ� ������
    //      ������ �߻���Ų��. (�������� �����ε��� ���̴�.)
    //
    // ����, ���� ���� �Ʒ��� ���� ���ĵǾ� �־�� �Ѵ�.
    // (1,4) �� (2,4)�� �������� 4 �̰�, (2,4)�� (2,3)�� �������� 2 �̴�.
    //    _    _        _
    // (1,4)->(2,4)->(2,3)
    //
    // �̷��� �ٷ� ���� Relation ���� ������ �� �ִ� �͵��� ���
    // �̿� �ٰ��ؼ� �����صΰ�, �� ���� ���������� qsort �� ����
    // ���ĵȴ�� �θ� �ȴ�.
    //------------------------------------------
    for ( i = 0 ; i < aJoinGroup->joinRelationCnt ; i++ )
    {
        sCurRel = (qmoJoinOperRel *)&aJoinGroup->joinRelation[i];

        if ( i+1 < aJoinGroup->joinRelationCnt )
        {
            IDE_FT_ASSERT( i+1 < aJoinGroup->joinRelationCntMax );

            sConnectRel = (qmoJoinOperRel *)&aJoinGroup->joinRelation[i+1];

            qtc::dependencyAnd( & sCurRel->depInfo,
                                & sConnectRel->depInfo,
                                & sConnectAndDep );

            if( qtc::dependencyEqual( & sConnectAndDep,
                                      & qtc::zeroDependencies )
                == ID_TRUE )
            {

                if ( i+2 < aJoinGroup->joinRelationCnt )
                {
                    for ( j = i+2 ; j < aJoinGroup->joinRelationCnt ; j++ )
                    {
                        IDE_FT_ASSERT( i+2 < aJoinGroup->joinRelationCntMax );

                        sConnectRel = (qmoJoinOperRel *)&aJoinGroup->joinRelation[i+2];

                        qtc::dependencyAnd( & sCurRel->depInfo,
                                            & sConnectRel->depInfo,
                                            & sConnectAndDep );

                        if( qtc::dependencyEqual( & sConnectAndDep,
                                                  & qtc::zeroDependencies )
                            == ID_FALSE )
                        {
                            idlOS::memcpy( (void*) &sTempRel,
                                           (void*) &aJoinGroup->joinRelation[i+1],
                                           ID_SIZEOF(qmoJoinOperRel) );

                            idlOS::memcpy( (void*) &aJoinGroup->joinRelation[i+1],
                                           (void*) &aJoinGroup->joinRelation[j],
                                           ID_SIZEOF(qmoJoinOperRel) );

                            idlOS::memcpy( (void*) &aJoinGroup->joinRelation[j],
                                           (void*) &sTempRel,
                                           ID_SIZEOF(qmoJoinOperRel) );
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
                // Nothing to do.
            }
        }

        //------------------------------------------
        // ������ ���� �ϳ��� ���̺��� 2 �� �̻��� ���̺��
        // Outer Join �Ǵ� ��� ����
        //
        // where t1.i1(+) = t2.i1
        //   and t1.i1(+) = t3.i1
        //
        // ������ ���� ����.
        //  _        _    _      _
        // (1,2)->(1,5)->(2,3)->(4,5)
        //
        // ������� sTotalRelDepOR �� Oring �� Predicate �� ���� ���ٸ�,
        // �� �������� sTotalRelDepOR ����� ������ ���� ���̴�.
        // (F:QMO_JOIN_OPER_FALSE, T:QMO_JOIN_OPER_TRUE, B:QMO_JOIN_OPER_BOTH)
        //
        //   1  2  3  4  5
        //   B  B  F  T  B
        //         _
        // ���⿡ (1,6) �� �߰��Ϸ��� �ϸ� ������ ���� �Ѵ�.
        //
        //   1  2  3  4  5  (AND)  1  6  = 1
        //   B  B  F  T  B         T  F    T(B)
        //              _
        // ���� ����� (1) �̴�.
        // �̷� ����� ������ �̴� �����̺��� 2 �� �̻��� ���̺��
        // Outer Join ���迡 �ִٴ� ���̹Ƿ� �����̴�.
        //
        // ��, �ռ� ���� predicate ������ 1 �� ���� outer join operator ��
        // �پ��ִ°� �־��µ�, 6 �� ������ predicate �� �����ٴ� �ǹ̰� �ǹǷ�
        // �ᱹ t1.i1(+)=t3.i1 and t1.i1(+)=t6.i1 ó�� t1.i1(+) �� �ΰ���
        // ���̺� outer join �ǰ� �ִٴ� �ǹ��̴�.
        //------------------------------------------

        qtc::dependencyClear( & sAndDependencies );

        qtc::dependencyJoinOperAnd( & sTotalRelDepOR,
                                    & sCurRel->depInfo,
                                    & sAndDependencies );

        if( qtc::dependencyEqual( & sAndDependencies,
                                  & qtc::zeroDependencies )
            == ID_FALSE )
        {
            sJoinOper = qtc::getDependJoinOper( &sAndDependencies, 0 );

            if ( QMO_JOIN_OPER_EXISTS( sJoinOper ) == ID_TRUE )
            {
                sErrNode = (qtcNode *) sCurRel->node->node.arguments;

                sqlInfo.setSourceInfo ( aStatement, & sErrNode->position );
                IDE_RAISE( ERR_ABORT_MAYBE_OUTER_JOINED_TO_AT_MOST_ONE_OTHER_TABLE );
            }
            else
            {
                // Nothing to do.
            }
        }

        //------------------------------------------
        // ## Join Looping
        //
        // ������ ���� Outer Join ���谡 looping �Ǿ� �ᱹ �ڽŰ�
        // Outer Join �� ��쵵 ������ �߻����Ѿ� �Ѵ�.
        //
        // where t1.i1(+) = t2.i1
        //   and t2.i1(+) = t1.i1
        //
        // where t1.i1(+) = t2.i1
        //   and t2.i1(+) = t3.i1
        //   and t3.i1(+) = t1.i1
        //
        // �̷� ���� ��� �����ұ� ?
        //  _        _    _ 
        // (1,2)->(1,5)->(2,3)
        //
        // ���� ���� Relation �� ���� sTotalRelDepOR �� ORing �� ������ ��,
        // ������ Current Relation �� ���� ��������� �˻��Ϸ��� �Ѵ�.
        //  _
        // (4,5)
        //
        // ���� �����ϱ� ? �����ϱ� ?
        //
        // �� �Ǵ��� ������ ��� Oring �Ǿ� �ִ� sTotalRelDepOR ��
        //      _                            _
        // 4 �� 5 �� �����ϴ��ķ� �Ǵ��Ѵ�. (4 �� 5 �� �ݴ�)
        // ��, Current Relation node �� dependency table �鿡 ���� (+) ��
        // ������ �ݴ� ������ ���� sTotalRelDepOR �� �����ϸ� �����̴�.
        //
        // �̴� ������ ���� ������ ���� Ȯ���ϸ� �ȴ�.
        //
        //                      (��)    (��)
        //   1  2  3  5  (AND)  4  5  =  5
        //   B  B  F  T         F  T     T
        //
        // ������ (��)�� (��)�� ����� ������ �����̴�.
        // ���� ���� �ٸ��Ƿ� �������̶�� �� �� �ִ�.
        //                               _
        // �׷� ���� ���� Relation node (3,5) �� ��� ?
        //
        //                         (��)     (��)
        //   1  2  3  4  5  (AND)  3  5  =  3  5
        //   B  B  F  T  B         F  T     F  T
        //
        // �� ���� (��)�� (��)�� ����� �����Ƿ� �����̴�.
        //------------------------------------------

        qtc::getJoinOperCounter( &sCounterDep, &sCurRel->depInfo );

        qtc::dependencyClear( & sAndDependencies );

        qtc::dependencyJoinOperAnd( & sTotalRelDepOR,
                                    & sCounterDep,
                                    & sAndDependencies );

        if ( qtc::dependencyJoinOperEqual( & sAndDependencies,
                                           & sCounterDep )
             == ID_TRUE )
        {
            sErrNode = (qtcNode *) sCurRel->node->node.arguments;

            sqlInfo.setSourceInfo ( aStatement, & sErrNode->position );
            IDE_RAISE( ERR_ABORT_NOT_ALLOW_OUTER_JOIN_EACH_OTHER );
        }

        //------------------------------------------
        // ���� ������׵��� relation node ���� �˻��ϱ� ����
        // �տ������� ���ʷ� �� node �� dependency ������
        // sTotalRelDepOR �� Oring �صд�.
        //------------------------------------------
        IDE_TEST( qtc::dependencyOr( & sTotalRelDepOR,
                                     & sCurRel->depInfo,
                                     & sTotalRelDepOR )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_NOT_ALLOW_OUTER_JOIN_EACH_OTHER )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_ALLOW_OUTER_JOIN_EACH_OTHER,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_ABORT_MAYBE_OUTER_JOINED_TO_AT_MOST_ONE_OTHER_TABLE )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_MAYBE_OUTER_JOINED_TO_AT_MOST_ONE_OTHER_TABLE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::joinOperOrdering( qcStatement      * aStatement,
                                    qmoJoinOperTrans * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Order�� ����
 *
 * Implemenation :
 *    (1) Join Group�� �з�
 *    (2) �� Join Group �� Join Relation�� ǥ��
 *    (3) �� Join Group �� Join Order�� ����
 *
 ***********************************************************************/

    qmoJoinOperGroup * sJoinGroup = NULL;
    UInt               sJoinGroupCnt;
    UInt               i;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::joinOperOrdering::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTrans     != NULL );


    //------------------------------------------
    // Join Grouping�� �з�
    //------------------------------------------

    IDE_TEST( joinOperGrouping( aTrans ) != IDE_SUCCESS );

    sJoinGroup       = aTrans->joinOperGroup;
    sJoinGroupCnt    = aTrans->joinOperGroupCnt;

    for ( i = 0; i < sJoinGroupCnt; i++ )
    {
        //------------------------------------------
        // Join Group �ϳ����� �����Ͽ�
        // Join Group ���� joinPredicate �� �����
        // ��� Predicate ���� �ߺ��˻�, Sort �� �����Ͽ�
        // joinRelation �� �����Ѵ�.
        //------------------------------------------

        IDE_TEST( joinOperRelationing( aStatement, & sJoinGroup[i] )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::isAvailableJoinOperRel( qcDepInfo  * aDepInfo,
                                          idBool     * aIsTrue )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Outer Join Operator �� ���� Join Relation �� ��ȿ���˻�
 *
 * Implemenation :
 *    Join Relation �� dependency ������ ��ȿ�Ϸ���
 *    dependency table �� 2 ���̸鼭 ���ʿ��� (+)��
 *    �־�� �Ѵ�.
 *
 ***********************************************************************/

    SInt   sJoinOperCnt;
    SInt   sDepCount;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::isAvailableJoinOperRel::__FT__" );

    sJoinOperCnt = qtc::getCountJoinOperator( aDepInfo );
    sDepCount    = qtc::getCountBitSet( aDepInfo );

    if ( ( sDepCount == QMO_JOIN_OPER_DEPENDENCY_TABLE_CNT_PER_PRED )
      && ( sJoinOperCnt == QMO_JOIN_OPER_TABLE_CNT_PER_PRED ) )
    {
        *aIsTrue = ID_TRUE;
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoOuterJoinOper::movePred2OnCondition( qcStatement      * aStatement,
                                        qmoJoinOperTrans * aTrans,
                                        qmsFrom          * aNewFrom,
                                        qmoJoinOperRel   * aJoinGroupRel,
                                        qmoJoinOperPred  * aJoinGroupPred )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : onCondition �� �ۼ�
 *
 * Implemenation :
 *   joinPredicate �� oneTablePred �߿��� aNewFrom �� �����ִ� ����
 *   aNewFrom �� onCondition �� �����Ѵ�.
 *   ���� joinPredicate �� oneTablePred list ���� ������ ��
 *   ������ ��尡 ����Ű�� normalCNF ��带 �и��Ͽ� onCondition �� �����Ѵ�.
 *
 ***********************************************************************/

    qmoJoinOperPred  * sCurStart       = NULL;
    qmoJoinOperPred  * sCur            = NULL;
    qmoJoinOperPred  * sPrev           = NULL;
    qmoJoinOperPred  * sAdd            = NULL;
    qtcNode          * sNorStart       = NULL;
    qtcNode          * sNorCur         = NULL;
    qtcNode          * sNorPrev        = NULL;
    qtcNode          * sCurOnCond      = NULL;

    qcDepInfo          sAndDependencies;
    qcNamePosition     sNullPosition;
    qtcNode          * sTmpNode[2];
    qtcNode          * sANDNode        = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::movePred2OnCondition::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aNewFrom   != NULL );
    IDE_DASSERT( aJoinGroupRel   != NULL );
    IDE_DASSERT( aJoinGroupPred  != NULL );


    //------------------------------------------------------
    // qmoJoinOperTrans->joinOperGroup->joinPredicate ���� aNewFrom ��
    // ������ �Ͱ� depInfo �� ���� node �� ã�Ƽ�, �̰��� ����Ű��
    // nomalizedCNF node �� ã�Ƽ� aNewFrom �� onCondition ��
    // ������ �� normalCNF ������ �����Ѵ�.
    //------------------------------------------------------

    sCurStart = aJoinGroupPred;

    for ( sCur = aJoinGroupPred ;
          sCur != NULL ;
          sPrev = sCur, sCur = (qmoJoinOperPred *)sCur->next )
    {
        if ( qtc::dependencyJoinOperEqual( & sCur->node->depInfo,
                                           & aJoinGroupRel->depInfo )
             == ID_FALSE )
        {
            continue;
        }

        if ( sCur == sCurStart )
        {
            sCurStart = sCur->next;
            // aTrans->joinOperGroup->joinPredicate = sCur->next;
        }
        else
        {
            sPrev->next = sCur->next;
        }

        sAdd = sCur;

        sNorStart = (qtcNode *)aTrans->normalCNF->node.arguments;

        for ( sNorCur = (qtcNode *)aTrans->normalCNF->node.arguments ;
              sNorCur != NULL ;
              sNorPrev = sNorCur,sNorCur = (qtcNode *)sNorCur->node.next )
        {
            if ( sNorCur == sAdd->node )
            {
                if ( sNorCur == sNorStart )
                {
                    if ( sNorCur->node.next != NULL )
                    {
                        aTrans->normalCNF->node.arguments = sNorCur->node.next;
                    }
                    else
                    {
                        aTrans->normalCNF = NULL;
                    }

                    sNorCur->node.next = NULL;
                }
                else
                {
                    sNorPrev->node.next = sNorCur->node.next;
                    sNorCur->node.next = NULL;
                }

                if ( aNewFrom->onCondition == NULL )
                {
                    //------------------------------------------------------
                    // ������� normalCNF �� predicate ��带 ó������
                    // aNewFrom ����� onCondition �� ����
                    //------------------------------------------------------

                    aNewFrom->onCondition = (qtcNode *)sNorCur->node.arguments;

                    //------------------------------------------------------
                    // ���� ������ ���� onCondition �� �������� ����صд�.
                    //------------------------------------------------------

                    sCurOnCond = aNewFrom->onCondition;
                }
                else
                {
                    //------------------------------------------------------
                    // onCondition �� �ΰ� �̻��� ��尡 �޸��� �Ǵ� ����
                    // AND ��带 �߰��� �� ��������� �Ѵ�.
                    // aNewFrom->onCondition �� ����Ű�� ��尡 AND ��尡
                    // �ƴ� ��쿡�� ���ָ� �ȴ�.
                    //------------------------------------------------------

                    if ( aNewFrom->onCondition->node.module != &mtfAnd )
                    {
                        //------------------------------------------------------
                        // AND ��带 �����Ѵ�.
                        //------------------------------------------------------
                        SET_EMPTY_POSITION(sNullPosition);

                        IDE_TEST( qtc::makeNode( aStatement,
                                                 sTmpNode,
                                                 & sNullPosition,
                                                 & mtfAnd )
                                  != IDE_SUCCESS);

                        sANDNode = sTmpNode[0];

                        // sANDNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
                        // sANDNode->node.lflag |= 1;

                        //------------------------------------------------------
                        // ������ AND ��带 onCondition �� ��ó���� �����ִ´�.
                        //------------------------------------------------------

                        sANDNode->node.arguments = (mtcNode *)aNewFrom->onCondition;
                        aNewFrom->onCondition = sANDNode;

                        //------------------------------------------------------
                        // sCurOnCond �� ������ �߰��� ��带 ����ϰ� �����Ƿ�
                        // �� �ڿ� �������ְ� �ٽ� �������� ����Ѵ�.
                        //------------------------------------------------------

                        sCurOnCond->node.next = sNorCur->node.arguments;
                        sCurOnCond = (qtcNode *)sCurOnCond->node.next;
                    }
                    else
                    {
                        //------------------------------------------------------
                        // ����ص� onCondition �� �������� �߰��� �����Ѵ�.
                        // �׸���, �߰��� ������ ��带 �ٽ� ����صд�.
                        //------------------------------------------------------

                        sCurOnCond->node.next = sNorCur->node.arguments;
                        sCurOnCond = (qtcNode *)sCurOnCond->node.next;
                    }
                }

                break;
            }
        }
    }

    //------------------------------------------------------
    // qmoJoinOperTrans->oneTablePred ������ (+)�� ���� ���� dependency table ��
    // ���� Predicate �� ã�Ƽ� ������ �� onCondition �� �����Ѵ�.
    // nomalizedCNF ������ �ش� Predicate �� ã�Ƽ� ������ ��
    // �̸� aNewFrom �� onCondition �� �����Ѵ�.
    //                                           _
    // ��, Left Outer Join �� dependency ������ (1,2) ���,
    //  _
    // (1) �� ���� one table predicate ��常 �ش� left outer join ��
    // on condition �� �����Ѵ�.
    //------------------------------------------------------

    sCurStart = aTrans->oneTablePred;

    for ( sCur = aTrans->oneTablePred ;
          sCur != NULL ;
          sPrev = sCur, sCur = (qmoJoinOperPred *)sCur->next )
    {
        //------------------------------------------------------
        // �ݵ�� depend ���� �� �ƴ϶� Outer Join Operator ���ε�
        // ���� ���� �ִ����� ã�ƾ� �Ѵ�.
        //
        // where t1.i1(+) = t2.i1
        //   and t2.i1(+) = 1  <- oneTablePred
        //
        // ���� ���� ���� oneTablePred �� �ִ� ���� ��ȯ �Ŀ���
        // �״�� normalCNF �� �����־�� �Ѵ�.
        // onCondition �� �����ϸ� ����� �޶�����.
        //------------------------------------------------------

        qtc::dependencyJoinOperAnd( & sCur->node->depInfo,
                                    & aJoinGroupRel->depInfo,
                                    & sAndDependencies );

        if( qtc::dependencyEqual( & sAndDependencies,
                                  & qtc::zeroDependencies )
            == ID_TRUE )
        {
            continue;
        }

        if ( sCur == sCurStart )
        {
            aTrans->oneTablePred = sCur->next;
        }
        else
        {
            sPrev->next = sCur->next;
        }

        sAdd = sCur;

        sNorStart = (qtcNode *)aTrans->normalCNF->node.arguments;

        for ( sNorCur = (qtcNode *)aTrans->normalCNF->node.arguments ;
              sNorCur != NULL ;
              sNorPrev = sNorCur, sNorCur = (qtcNode *)sNorCur->node.next )
        {
            if ( sNorCur == sAdd->node )
            {
                if ( sNorCur == sNorStart )
                {
                    if ( sNorCur->node.next != NULL )
                    {
                        aTrans->normalCNF->node.arguments = sNorCur->node.next;
                    }
                    else
                    {
                        aTrans->normalCNF = NULL;
                    }

                    sNorCur->node.next = NULL;
                }
                else
                {
                    sNorPrev->node.next = sNorCur->node.next;
                    sNorCur->node.next = NULL;
                }

                if ( aNewFrom->onCondition == NULL )
                {
                    //------------------------------------------------------
                    // ������� normalCNF �� predicate ��带 ó������
                    // aNewFrom ����� onCondition �� ����
                    //------------------------------------------------------

                    aNewFrom->onCondition = sNorCur;

                    //------------------------------------------------------
                    // ���� ������ ���� onCondition �� �������� ����صд�.
                    //------------------------------------------------------

                    sCurOnCond = aNewFrom->onCondition;
                }
                else
                {
                    //------------------------------------------------------
                    // onCondition �� �ΰ� �̻��� ��尡 �޸��� �Ǵ� ����
                    // AND ��带 �߰��� �� ��������� �Ѵ�.
                    // aNewFrom->onCondition �� ����Ű�� ��尡 AND ��尡
                    // �ƴ� ��쿡�� ���ָ� �ȴ�.
                    //------------------------------------------------------

                    if ( aNewFrom->onCondition->node.module != &mtfAnd )
                    {
                        //------------------------------------------------------
                        // AND ��带 �����Ѵ�.
                        //------------------------------------------------------
                        SET_EMPTY_POSITION(sNullPosition);

                        IDE_TEST( qtc::makeNode( aStatement,
                                                 sTmpNode,
                                                 & sNullPosition,
                                                 & mtfAnd )
                                  != IDE_SUCCESS);

                        sANDNode = sTmpNode[0];

                        // sANDNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
                        // sANDNode->node.lflag |= 1;

                        //------------------------------------------------------
                        // ������ AND ��带 onCondition �� ��ó���� �����ִ´�.
                        //------------------------------------------------------

                        sANDNode->node.arguments = (mtcNode *)aNewFrom->onCondition;
                        aNewFrom->onCondition = sANDNode;

                        //------------------------------------------------------
                        // sCurOnCond �� ������ �߰��� ��带 ����ϰ� �����Ƿ�
                        // �� �ڿ� �������ְ� �ٽ� �������� ����Ѵ�.
                        //------------------------------------------------------

                        sCurOnCond->node.next = (mtcNode*)sNorCur;
                        sCurOnCond = (qtcNode *)sCurOnCond->node.next;
                    }
                    else
                    {
                        //------------------------------------------------------
                        // ����ص� onCondition �� �������� �߰��� �����Ѵ�.
                        // �׸���, �߰��� ������ ��带 �ٽ� ����صд�.
                        //------------------------------------------------------

                        sCurOnCond->node.next = (mtcNode*)sNorCur;
                        sCurOnCond = (qtcNode *)sCurOnCond->node.next;
                    }
                }

                break;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    //------------------------------------------------------
    // ������ ����� onCondition �� normalCNF �� ���� estimate
    //------------------------------------------------------

    if ( aNewFrom->onCondition != NULL )
    {
        if ( aNewFrom->onCondition->node.module == &mtfAnd )
        {
            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     (qtcNode *)( aNewFrom->onCondition ) )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( aTrans->normalCNF != NULL )
    {
        IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                 (qtcNode *)( aTrans->normalCNF ) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::transJoinOperBetweenStyle( qcStatement * aStatement,
                                             qmsQuerySet * aQuerySet,
                                             qtcNode     * aInNode,
                                             qtcNode    ** aTransStart,
                                             qtcNode    ** aTransEnd )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implementation :
 *
 *    BETWEEN Style
 *    ======================================================
 *    mtfModule          : mtcName       : source  : target
 *    ======================================================
 *    mtfBetween         : BETWEEN       :   1     :   2 (not list)
 *    mtfNotBetween      : NOT BETWEEN   :   1     :   2 (not list)
 *    ======================================================
 *
 *    between �� ���� Source �� Target �� list �� �� �� ����.
 *
 *   (1) mtfBetween
 *
 *       ** ��ȯ ��
 *       a1 between a2 and a3
 *
 *        (AND)
 *          |
 *          V
 *        (OR)  <- aORNode
 *          |
 *          V
 *      (Between) <- aInNode
 *          |
 *          V
 *        (a1) -> (a2) -> (a3)
 *
 *       ** ��ȯ ��
 *       a1 >= a2 and a1 <= a3
 *
 *        (AND)
 *          |  _______ aTransStart ______ aTransEnd
 *          V /                   /
 *        (OR)       ---->    (OR)
 *          |                   |
 *          V                   V
 *     (GreaterEqual)      (LessEqual)
 *          |                   |
 *          V                   V
 *        (a1) -> (a2)        (a1) -> (a3)
 *
 *
 *   (2) mtfNotBetween
 *
 *       ** ��ȯ ��
 *       a1 not between a2 and a3
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *     (NotBetween)
 *          |
 *          V
 *        (a1) -> (a2) -> (a3)
 *
 *       ** ��ȯ ��
 *       a1 < a2 or a1 > a3
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *     (LessThan)   --->  (GreaterThan)
 *          |                   |
 *          V                   V
 *        (a1) -> (a2)        (a1) -> (a3)
 ***********************************************************************/

    qtcNode       * sOldSrc        = NULL;
    qtcNode       * sOldTrg        = NULL;
    qtcNode       * sOldTrgStart   = NULL;
    qtcNode       * sNewSrc        = NULL;
    qtcNode       * sNewTrg        = NULL;

    qtcNode       * sORNode        = NULL;
    qtcNode       * sOperNode      = NULL;

    qtcNode       * sPredStart     = NULL;
    qtcNode       * sPredEnd       = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transJoinOperBetweenStyle::__FT__" );

    IDE_DASSERT( aStatement  != NULL );
    IDE_DASSERT( aQuerySet   != NULL );
    IDE_DASSERT( aInNode     != NULL );
    IDE_DASSERT( aTransStart != NULL );
    IDE_DASSERT( aTransEnd   != NULL );

    if ( aInNode->node.module == & mtfBetween )
    {
        //---------------------------------------------
        // Between �������� Argument �� ��ȸ�ϸ�
        // src = trg1 and src = trg2 �� ���� ���·�
        // predicate �� �и��Ѵ�.
        // ���� �����ϴ� Predicate �� ����������
        // �űԷ� �����Ͽ� ����Ѵ�.
        //---------------------------------------------

        sOldSrc = (qtcNode *)aInNode->node.arguments;
        sOldTrgStart = (qtcNode *)sOldSrc->node.next;

        for ( sOldTrg  = (qtcNode *)sOldTrgStart;
              sOldTrg != NULL;
              sOldTrg  = (qtcNode *)sOldTrg->node.next )
        {
            //---------------------------------------------
            // ù��° Target ����� ��
            //---------------------------------------------
            if ( sOldTrg == sOldTrgStart )
            {
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             & mtfGreaterEqual,
                                             & sORNode,
                                             ID_TRUE )
                          != IDE_SUCCESS);
            }
            //---------------------------------------------
            // �ι�° Target ����� ��
            // Target Node �� �ΰ��ۿ� ����.
            //---------------------------------------------
            else
            {
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             & mtfLessEqual,
                                             & sORNode,
                                             ID_TRUE )
                          != IDE_SUCCESS);
            }

            if ( sOldTrg == (qtcNode *)sOldSrc->node.next )
            {
                sPredStart = sORNode;
            }
            else
            {
                IDE_DASSERT( sPredStart != NULL );

                sPredEnd = sORNode;
                sPredStart->node.next = (mtcNode *)sORNode;
            }

            /****************************************************
             * ������ ���� �ű� Tree �� �ϼ��Ǿ���.
             *
             *             _______ sPredStart  _______ sPredEnd
             *            /                   /
             *        (OR)       ---->    (OR)
             *          |                   |
             *          V                   V
             *     (GreaterEqual)      (LessEqual)
             *          |                   |
             *          V                   V
             *        (a1) -> (a2)        (a1) -> (a3)
             ****************************************************/
        }
    }
    else if ( aInNode->node.module == & mtfNotBetween )
    {

        sOldSrc = (qtcNode *)aInNode->node.arguments;
        sOldTrgStart = (qtcNode *)sOldSrc->node.next;

        for ( sOldTrg  = sOldTrgStart;
              sOldTrg != NULL;
              sOldTrg  = (qtcNode *)sOldTrg->node.next )
        {
            //---------------------------------------------
            // ù��° Target ����� ��
            //---------------------------------------------
            if ( sOldTrg == sOldTrgStart )
            {
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             & mtfLessThan,
                                             & sORNode,
                                             ID_FALSE )
                          != IDE_SUCCESS);
            }
            //---------------------------------------------
            // �ι�° Target ����� ��.
            // (Target Node �� �ΰ��ۿ� ����)
            //---------------------------------------------
            else
            {
                IDE_TEST( makeNewOperNodeTree( aStatement,
                                               aInNode,
                                               sOldSrc,
                                               sOldTrg,
                                               & sNewSrc,
                                               & sNewTrg,
                                               & mtfGreaterThan,
                                               & sOperNode )
                          != IDE_SUCCESS);

                sORNode->node.arguments->next = (mtcNode *)sOperNode;
                sOperNode->node.next = NULL;
            }

            //---------------------------------------------
            // OR ��尡 �Ѱ��̱� ������ Start �� End �� ����.
            //---------------------------------------------
            if ( sOldTrg == (qtcNode *)sOldSrc->node.next )
            {
                sPredStart = sORNode;
            }
            else
            {
                sPredEnd = sORNode;
            }

            /****************************************************
             * ������ ���� �ű� Tree �� �ϼ��Ǿ���.
             *
             *             _______ sPredStart, sPredEnd
             *            /
             *        (OR)
             *          |
             *          V
             *     (LessThan)   --->  (GreaterThan)
             *          |                   |
             *          V                   V
             *        (a1) -> (a2)        (a1) -> (a3)
             ****************************************************/
        }

        //---------------------------------------------
        // estimate ����.
        // OR ���� �ϳ��̱� ������ �������� �ѹ��� ����.
        //---------------------------------------------
        IDE_TEST( qtc::estimate( sORNode,
                                 QC_SHARED_TMPLATE(aStatement),
                                 aStatement,
                                 aQuerySet,
                                 aQuerySet->SFWGH,
                                 NULL )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_RAISE( ERR_UNEXPECTED_MODULE );
    }

    //---------------------------------------------
    // ��ȯ�� Tree �� Start OR ���� End OR ��带
    // ��ȯ�Ѵ�.
    //---------------------------------------------

    *aTransStart = sPredStart;
    *aTransEnd   = sPredEnd;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_MODULE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transJoinOperBetweenStyle",
                                  "Invalid comparison module" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::getCompModule4List( mtfModule    *  aModule,
                                      mtfModule    ** aOutModule )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implementation :
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::getCompModule4List::__FT__" );

    if ( ( aModule == & mtfEqual )
      || ( aModule == & mtfEqualAll )
      || ( aModule == & mtfEqualAny ) )
    {
        *aOutModule = &mtfEqual;
    }
    else if ( ( aModule == & mtfGreaterEqualAll )
           || ( aModule == & mtfGreaterEqualAny ) )
    {
        *aOutModule = &mtfGreaterEqual;
    }
    else if ( ( aModule == & mtfGreaterThanAll )
           || ( aModule == & mtfGreaterThanAny ) )
    {
        *aOutModule = &mtfGreaterThan;
    }
    else if ( ( aModule == & mtfLessEqualAll )
           || ( aModule == & mtfLessEqualAny ) )
    {
        *aOutModule = &mtfLessEqual;
    }
    else if ( ( aModule == & mtfLessThanAll )
           || ( aModule == & mtfLessThanAny ) )
    {
        *aOutModule = &mtfLessThan;
    }
    else if ( ( aModule == & mtfNotEqual )
           || ( aModule == & mtfNotEqualAll )
           || ( aModule == & mtfNotEqualAny ) )
    {
        *aOutModule = &mtfNotEqual;
    }
    else
    {
        IDE_RAISE( ERR_UNEXPECTED_MODULE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED_MODULE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::getCompModule4List",
                                  "Invalid comparison module" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::makeNewORNodeTree( qcStatement  * aStatement,
                                     qmsQuerySet  * aQuerySet,
                                     qtcNode      * aInNode,
                                     qtcNode      * aOldSrc,
                                     qtcNode      * aOldTrg,
                                     qtcNode     ** aNewSrc,
                                     qtcNode     ** aNewTrg,
                                     mtfModule    * aCompModule,
                                     qtcNode     ** aORNode,
                                     idBool         aEstimate )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implementation :
 *  ������ ���� Tree �� �����.
 *
 *        (OR)
 *          |
 *          V
 *       (Equal)
 *          |
 *          V
 *        (a1) -> (b1)
 ***********************************************************************/

    qtcNode       * sTmpNode[2];
    qtcNode       * sORNode     = NULL;
    qtcNode       * sOperNode   = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::makeNewORNodeTree::__FT__" );

    //---------------------------------------------
    // sOldSrc ��尡 Root �� �Ǵ� Tree ������
    // ��°�� �����Ѵ�.
    //---------------------------------------------
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     aOldSrc,
                                     aNewSrc,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    //---------------------------------------------
    // sOldTrg ��尡 Root �� �Ǵ� Tree ������
    // ��°�� �����Ѵ�.
    //---------------------------------------------
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     aOldTrg,
                                     aNewTrg,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );


    (*aNewSrc)->node.next = (mtcNode *)(*aNewTrg);
    (*aNewTrg)->node.next = NULL;

    //---------------------------------------------
    // ��ȯ ������ ��� �Ҵ�
    //---------------------------------------------
    IDE_TEST( qtc::makeNode( aStatement,
                             sTmpNode,
                             & aInNode->position,
                             aCompModule )
              != IDE_SUCCESS );


    //---------------------------------------------
    // ��� ��ȯ �����ڰ� argument �� 2 �� �ʿ�� �ϹǷ�
    // �����Ѵ�.
    //---------------------------------------------

    sOperNode = sTmpNode[0];
    sOperNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    sOperNode->node.lflag |= 2;

    sOperNode->node.arguments = (mtcNode *)(*aNewSrc);
    sOperNode->node.next = NULL;

    IDE_TEST( qtc::makeNode( aStatement,
                             sTmpNode,
                             & aInNode->position,
                             & mtfOr )
              != IDE_SUCCESS );

    //---------------------------------------------
    // OR �� �ʿ�� �ϴ� argument �� 1 ��
    //---------------------------------------------

    sORNode = sTmpNode[0];
    sORNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    sORNode->node.lflag |= 1;

    sORNode->node.arguments = (mtcNode *)sOperNode;
    sORNode->node.next = NULL;

    //---------------------------------------------
    // estimate ����
    //---------------------------------------------

    if ( aEstimate == ID_TRUE )
    {
        IDE_TEST( qtc::estimate( sORNode,
                                 QC_SHARED_TMPLATE(aStatement),
                                 aStatement,
                                 aQuerySet,
                                 aQuerySet->SFWGH,
                                 NULL )
                 != IDE_SUCCESS );
    }

    *aORNode = sORNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOuterJoinOper::makeNewOperNodeTree( qcStatement  * aStatement,
                                       qtcNode      * aInNode,
                                       qtcNode      * aOldSrc,
                                       qtcNode      * aOldTrg,
                                       qtcNode     ** aNewSrc,
                                       qtcNode     ** aNewTrg,
                                       mtfModule    * aCompModule,
                                       qtcNode     ** aOperNode )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implementation :
 *  ������ ���� Tree �� �����.
 *
 *       (Oper)
 *          |
 *          V
 *        (a1) -> (b1)
 ***********************************************************************/

    qtcNode       * sTmpNode[2];
    qtcNode       * sOperNode   = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::makeNewOperNodeTree::__FT__" );

    //---------------------------------------------
    // sOldSrc ��尡 Root �� �Ǵ� Tree ������
    // ��°�� �����Ѵ�.
    //---------------------------------------------
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     aOldSrc,
                                     aNewSrc,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    //---------------------------------------------
    // sOldTrg ��尡 Root �� �Ǵ� Tree ������
    // ��°�� �����Ѵ�.
    //---------------------------------------------
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     aOldTrg,
                                     aNewTrg,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );


    (*aNewSrc)->node.next = (mtcNode *)(*aNewTrg);
    (*aNewTrg)->node.next = NULL;

    //---------------------------------------------
    // ��ȯ ������ ��� �Ҵ�
    //---------------------------------------------
    IDE_TEST( qtc::makeNode( aStatement,
                             sTmpNode,
                              & aInNode->position,
                             aCompModule )
              != IDE_SUCCESS);


    //---------------------------------------------
    // ��� ��ȯ �����ڰ� argument �� 2 �� �ʿ�� �ϹǷ�
    // �����Ѵ�.
    //---------------------------------------------

    sOperNode = sTmpNode[0];
    sOperNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    sOperNode->node.lflag |= 2;

    sOperNode->node.arguments = (mtcNode *)(*aNewSrc);
    sOperNode->node.next = NULL;

    *aOperNode = sOperNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::transJoinOperListArgStyle( qcStatement * aStatement,
                                             qmsQuerySet * aQuerySet,
                                             qtcNode     * aInNode,
                                             qtcNode    ** aTransStart,
                                             qtcNode    ** aTransEnd )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implementation :
 *    �� �Լ������� ������ �����ڵ��� ��ȯ�� �����Ѵ�.
 *
 *    LIST Argument Style
 *    ======================================================
 *    mtfModule          : mtcName       : source  : target
 *    ======================================================
 *    mtfEqual           : =             : list    :  list
 *    mtfEqualAll        : =ALL          : list    :  list
 *                                       :   1     :  list
 *    mtfEqualAny        : =ANY          : list    :  list
 *                                       :   1     :  list
 *    mtfGreaterEqualAll : >=ALL         :   1     :  list
 *    mtfGreaterEqualAny : >=ANY         :   1     :  list
 *    mtfGreaterThanAll  : >ALL          :   1     :  list
 *    mtfGreaterThanAny  : >ANY          :   1     :  list
 *    mtfLessEqualAll    : <=ALL         :   1     :  list
 *    mtfLessEqualAny    : <=ANY         :   1     :  list
 *    mtfLessThanAll     : <ALL          :   1     :  list
 *    mtfLessThanAny     : <ANY          :   1     :  list
 *    mtfNotEqual        : <>            : list    :  list
 *    mtfNotEqualAll     : <>ALL         : list    :  list
 *    mtfNotEqualAny     : <>ANY         : list    :  list
 *                                       :   1     :  list
 *    ======================================================
 *
 *
 *   �Ʒ��� ���� ������� ��ȯ�� �����ϸ� �ȴ�.
 *   ��� �����ڸ� ���� ��� ��ȯ�غ���.
 *   Between Style �� �ٸ� ���̶�� list �� �� �� �ִٴ� ���̰�,
 *   list �� argument �� ������ �� �� �ִٴ� ���̴�.
 *
 *   (1) mtfEqualAll
 *
 *       ** ��ȯ ��
 *       (a1,a2) =ALL ((b1,b2))
 *
 *        (AND)
 *          |
 *          V
 *        (OR) <- aORNode
 *          |
 *          V
 *     (EqualAll) <- aInNode
 *          |
 *          V
 *       (list)   --->  (list)
 *          |              |
 *          V              V
 *        (a1) -> (a2)  (list) <= can't be more than 1 list
 *                         |
 *                         V
 *                       (b1) -> (b2)
 *
 *       ** ��ȯ ��
 *       a1 = b1 and a2 = b2
 *
 *        (AND)
 *          |
 *          V
 *        (OR)    --->    (OR)
 *          |               |
 *          V               V
 *       (Equal)         (Equal)
 *          |               |
 *          V               V
 *        (a1) -> (a2)    (b1) -> (b2)
 *
 *
 *   (2) mtfEqualAny
 *
 *       ** ��ȯ ��
 *       (a1,a2) =ANY ((b1,b2))
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *     (EqualAny)
 *          |
 *          V
 *       (list)   --->  (list)
 *          |              |
 *          V              V
 *        (a1) -> (a2)  (list)
 *                         |
 *                         V
 *                       (b1) -> (b2)
 *
 *       ** ��ȯ ��
 *       a1 = b1 and a2 = b2
 *
 *        (AND)
 *          |
 *          V
 *        (OR)    --->    (OR)
 *          |               |
 *          V               V
 *       (Equal)         (Equal)
 *          |               |
 *          V               V
 *        (a1) -> (a2)    (b1) -> (b2)
 *
 *
 *   (3) mtfGreaterEqualAll
 *
 *       ** ��ȯ ��
 *       a1 >=ALL (b1,b2)
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *   (GreaterEqualAll)
 *          |
 *          V
 *        (a1)  --->  (list)
 *                      |
 *                      V
 *                    (b1) -> (b2)
 *
 *       ** ��ȯ ��
 *       a1 >= b1 and a1 >= b2
 *
 *        (AND)
 *          |
 *          V
 *        (OR)    --->    (OR)
 *          |               |
 *          V               V
 *   (GreaterEqual)  (GreaterEqual)
 *          |               |
 *          V               V
 *        (a1) -> (b1)    (a1) -> (b2)
 *
 *
 *   (4) mtfGreaterEqualAny
 *
 *       ** ��ȯ ��
 *       a1 >=ANY (b1,b2)
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *   (GreaterEqualAll)
 *          |
 *          V
 *        (a1)  --->  (list)
 *                      |
 *                      V
 *                    (b1) -> (b2)
 *
 *       ** ��ȯ ��
 *       a1 >= b1 or a1 >= b2
 *
 *        (AND)
 *          |
 *          V
 *        (OR)
 *          |
 *          V
 *   (GreaterEqual) ->  (GreaterEqual)
 *          |                 |
 *          V                 V
 *        (a1) -> (b1)      (a1) -> (b2)
 *
 *
 *
 *   ���� ��ȯ���� ����� �Ͽ� ��ȯ ��Ģ�� ������.
 *   ũ�� ANY �� ALL Type ���� ������ �����غ� �� �ִ�.
 *
 *   (1) ANY Type
 *
 *   ANY �� List argument �鰣�� �񱳸� OR ���·� ���� ���̴�.
 *   Source �� List �� ���� List �� �ƴҶ��� �����ؼ� �����ؾ� �Ѵ�.
 *
 *   ��ȯ��
 *   ��. (a1,a2) =any ((b1,b2),(c1,c2))
 *   ��. (a1,a2) =any ((b1,b2)))
 *   ��.  a1     =any (b1,b2)
 *
 *   ��ȯ��
 *   ��. (a1=b1 and a2=b2) or (a1=c1 and a2=c2)
 *   ��. (a1=b1 and a2=b2)
 *   ��. (a1=b1) or (a1=b2)
 *
 *   ���� ����� (��)�� ���� ������׿��� �����ص� �ȴ�.
 *   �ֳ��ϸ�, �̹� �տ��� (+) ��� �ÿ� Source ���� List �̸�
 *   Target ���� 2 �� �̻��� List �� ����� �� ������ �����������
 *   �ɷ��±� �����̴�.
 *
 *   ����, =any, >=any, <>any �� ��� �ΰ��� ��ȯ���·� ������ �� �ִ�.
 *   �ϳ��� (pred) and (pred) �̰�, �ϳ��� (pred) or (pred) �����̴�.
 *
 *         (OR)          (OR)
 *          |              |
 *          V              V
 *      (=,>=,<>)  ->  (=,>=,<>)
 *          |              |
 *          V              V
 *        (a1) -> (b1)   (a2) -> (b2)
 *
 *
 *         (OR)
 *          |
 *          V
 *      (=,>=,<>)  ->  (=,>=,<>)
 *          |              |
 *          V              V
 *        (a1) -> (b1)   (a1) -> (b2)
 *
 *   (2) ALL Type
 *
 *   ALL �� List argument �鰣�� �񱳸� AND ���·� ���� ���̴�.
 *
 *   ��ȯ��
 *   ��. (a1,a2) =all ((b1,b2),(c1,c2))
 *   ��. (a1,a2) =all ((b1,b2)))
 *   ��.  a1     =all (b1,b2)
 *
 *   ��ȯ��
 *   ��. (a1=b1 and a2=b2) and (a1=c1 and a2=c2)
 *   ��. (a1=b1 and a2=b2)
 *   ��. (a1=b1) and (a1=b2)
 *
 *   ���������� (��)�� ���� ������׿��� �ɷ����Ƿ� ����� �ʿ䰡 ����.
 *   ALL Type �� ���� ANY Type ���� �޸� ��� �Ʒ��� ����
 *   ���·� �����ϰ� ��ȯ�� �� �ִ�.
 *   Source �� list �ΰ� �ƴѰ��� ���� * �� ���� ���� a1 �Ǵ� a2 ��
 *   �޶��� ���̴�.
 *
 *
 *         (OR)          (OR)
 *          |              |
 *          V              V
 *      (=,>=,<>)  ->  (=,>=,<>)
 *          |              |
 *          V              V
 *        (a1) -> (b1)  *(a2) -> (b2)
 *
 *         (OR)          (OR)
 *          |              |
 *          V              V
 *      (=,>=,<>)  ->  (=,>=,<>)
 *          |              |
 *          V              V
 *        (a1) -> (b1)  *(a1) -> (b2)
 *
 ***********************************************************************/

    qtcNode       * sOldSrc     = NULL;
    qtcNode       * sOldSrcList = NULL;
    qtcNode       * sOldTrgList = NULL;
    qtcNode       * sOldTrg     = NULL;
    qtcNode       * sNewSrc     = NULL;
    qtcNode       * sNewTrg     = NULL;
    qtcNode       * sNewPrevOR  = NULL;;

    qtcNode       * sORNode     = NULL;
    qtcNode       * sOperNode   = NULL;
    qtcNode       * sPrevOperNode = NULL;

    qtcNode       * sPredStart  = NULL;
    qtcNode       * sPredEnd    = NULL;
    UInt            sORArgCount = 0;
    mtfModule     * sCompModule = NULL;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transJoinOperListArgStyle::__FT__" );

    IDE_DASSERT( aStatement  != NULL );
    IDE_DASSERT( aQuerySet   != NULL );
    IDE_DASSERT( aInNode     != NULL );
    IDE_DASSERT( aTransStart != NULL );
    IDE_DASSERT( aTransEnd   != NULL );

    //---------------------------------------------
    // 1. ANY Type �������� ��ȯ
    //---------------------------------------------
    if ( QMO_IS_ANY_TYPE_MODULE( aInNode->node.module ) == ID_TRUE )
    {
        //---------------------------------------------
        // �� Source ���� list �̸�, target ����
        // list �� 2 ���̻��� �� ���� ������
        // �Ѱ���� �����ϰ� ��ȯ�ϸ� �ȴ�.
        // (�� �Լ��� ���� ���� �̹� �ɷ�����.)
        //---------------------------------------------
        if ( aInNode->node.arguments->module == & mtfList )
        {
            sOldSrcList = (qtcNode *)aInNode->node.arguments;
            sOldTrgList = (qtcNode *)aInNode->node.arguments->next->arguments;

            //---------------------------------------------
            // Source List �� argument ������ Target List ��
            // argument ������ �׻� �����Ƿ� �� �����Ǵ�
            // argument ���� ������ �����ڷ� ������ �ȴ�.
            //---------------------------------------------
            for ( sOldSrc  = (qtcNode *)sOldSrcList->node.arguments,
                  sOldTrg  = (qtcNode *)sOldTrgList->node.arguments;
                  sOldSrc != NULL;
                  sOldSrc  = (qtcNode *)sOldSrc->node.next,
                  sOldTrg  = (qtcNode *)sOldTrg->node.next )
            {

                //---------------------------------------------
                // ���� module �� �ָ� ��ȯ�� �� ����� ���� module �� ����
                //---------------------------------------------
                IDE_TEST( getCompModule4List( (mtfModule *)aInNode->node.module,
                                              & sCompModule)
                          != IDE_SUCCESS);

                /**********************************************
                 *  Looping �� �������� ������ ���� Tree �� �����.
                 *  �̷��� Tree �� OR ������ �����س�����.
                 *
                 *        (OR)
                 *          |
                 *          V
                 *       (Equal)
                 *          |
                 *          V
                 *        (a1) -> (b1)
                 **********************************************/
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             sCompModule,
                                             & sORNode,
                                             ID_TRUE )
                          != IDE_SUCCESS);

                if ( sOldSrc == (qtcNode *)sOldSrcList->node.arguments )
                {
                    sPredStart = sORNode;
                    sPredEnd = sORNode;
                }
                else
                {
                    sNewPrevOR->node.next = (mtcNode *)sORNode;
                    sPredEnd = sORNode;
                }

                sNewPrevOR = sORNode;
            }

            //---------------------------------------------
            // mtfList ���� ���� ���ʿ��� ��尡 �ȴ�.
            // �׷���, host ������ ����� �� �Ʒ��� ���� ���������� ������,
            // fixAfterBinding �ܰ迡�� module �� �������� �ʾҴٰ�
            // ������ �߻���Ų��.
            // �׸���, size �� 0 ���� �������ν� ���ʿ��� row ������
            // �Ҵ��� ���´�.
            //---------------------------------------------
            QTC_STMT_COLUMN( aStatement, sOldSrcList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldSrcList )->module = & mtdList;

            QTC_STMT_COLUMN( aStatement, sOldTrgList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldTrgList )->module = & mtdList;

            /**********************************************
             *  ������ ���� Tree �� �ϼ��Ǿ���.
             *
             *            ____ sPrevStart  ____ sPrevEnd
             *           /                /
             *        (OR)    --->    (OR)
             *          |               |
             *          V               V
             *       (Equal)         (Equal)
             *          |               |
             *          V               V
             *        (a1) -> (b1)    (a2) -> (b2)
             **********************************************/
        }

        //---------------------------------------------
        // Source �� list �� �ƴ� ��
        //---------------------------------------------
        else
        {
            sOldTrgList = (qtcNode *)aInNode->node.arguments->next;
            sORArgCount = 0;

            for ( sOldSrc  = (qtcNode *)aInNode->node.arguments,
                  sOldTrg  = (qtcNode *)sOldTrgList->node.arguments;
                  sOldTrg != NULL;
                  sOldTrg  = (qtcNode *)sOldTrg->node.next )
            {
                IDE_TEST( getCompModule4List( (mtfModule *)aInNode->node.module,
                                              & sCompModule)
                          != IDE_SUCCESS);

                if ( sOldTrg == (qtcNode *)sOldTrgList->node.arguments )
                {
                    IDE_TEST( makeNewORNodeTree( aStatement,
                                                 aQuerySet,
                                                 aInNode,
                                                 sOldSrc,
                                                 sOldTrg,
                                                 & sNewSrc,
                                                 & sNewTrg,
                                                 sCompModule,
                                                 & sORNode,
                                                 ID_FALSE )
                              != IDE_SUCCESS);

                    sORArgCount = 1;
                    sORNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
                    sORNode->node.lflag |= sORArgCount;

                    sPrevOperNode = (qtcNode *)sORNode->node.arguments;

                    //---------------------------------------------
                    // OR ���� �ϳ��� ����� ������ Start �� End �� �����ϴ�.
                    //---------------------------------------------
                    sPredStart = sORNode;
                    sPredEnd = sORNode;
                }
                else
                {
                    IDE_TEST( makeNewOperNodeTree( aStatement,
                                                   aInNode,
                                                   sOldSrc,
                                                   sOldTrg,
                                                   & sNewSrc,
                                                   & sNewTrg,
                                                   sCompModule,
                                                   & sOperNode )
                              != IDE_SUCCESS );

                    //---------------------------------------------
                    // �űԷ� ������ �񱳿����� ��带 ������ ����Ʈ��
                    // �����Ѵ�.
                    //---------------------------------------------
                    sORArgCount++;
                    sORNode->node.lflag |= sORArgCount;

                    sPrevOperNode->node.next = (mtcNode *)sOperNode;
                    sOperNode->node.next = NULL;

                    sPrevOperNode = sOperNode;
                }
            }

            //---------------------------------------------
            // estimate ����
            //---------------------------------------------
            IDE_TEST( qtc::estimate( sORNode,
                                     QC_SHARED_TMPLATE(aStatement),
                                     aStatement,
                                     aQuerySet,
                                     aQuerySet->SFWGH,
                                     NULL )
                      != IDE_SUCCESS );

            /**********************************************
             *  ������ ���� Tree �� �ϼ��Ǿ���.
             *
             *            ____ sPrevStart, sPrevEnd
             *           /
             *        (OR)
             *          |
             *          V
             *       (Equal)   -->   (Equal)   --> ...
             *          |               |
             *          V               V
             *        (a1) -> (b1)    (a1) -> (b2)
             **********************************************/

            QTC_STMT_COLUMN( aStatement, sOldTrgList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldTrgList )->module = & mtdList;
        }
    }

    //---------------------------------------------
    // 2. ALL Type �������� ��ȯ
    //    mtfEqual, mtfNotEqual �����ڴ� ALL Type �� �ƴ�����
    //    Source �� list �� ���� ���·� �����ϰ� ó�����ָ� �ȴ�.
    //---------------------------------------------
    else if ( QMO_IS_ALL_TYPE_MODULE( aInNode->node.module ) == ID_TRUE )
    {
        //---------------------------------------------
        // �� Source ���� list �̸�, target ����
        // list �� 2 ���̻��� �� ���� ������
        // �Ѱ���� �����ϰ� ��ȯ�ϸ� �ȴ�.
        // (�� �Լ��� ���� ���� �̹� �ɷ�����.)
        //---------------------------------------------
        if ( aInNode->node.arguments->module == & mtfList )
        {
            sOldSrcList = (qtcNode *)aInNode->node.arguments;

            if ( ( aInNode->node.module == & mtfEqual )
              || ( aInNode->node.module == & mtfNotEqual ) )
            {
                sOldTrgList = (qtcNode *)aInNode->node.arguments->next;
            }
            else
            {
                sOldTrgList = (qtcNode *)aInNode->node.arguments->next->arguments;
            }

            //---------------------------------------------
            // Source List �� argument ������ Target List ��
            // argument ������ �׻� �����Ƿ� �� �����Ǵ�
            // argument ���� ������ �����ڷ� ������ �ȴ�.
            //---------------------------------------------
            for ( sOldSrc  = (qtcNode *)sOldSrcList->node.arguments,
                  sOldTrg  = (qtcNode *)sOldTrgList->node.arguments;
                  sOldSrc != NULL;
                  sOldSrc  = (qtcNode *)sOldSrc->node.next,
                  sOldTrg  = (qtcNode *)sOldTrg->node.next )
            {

                IDE_TEST( getCompModule4List( (mtfModule *)aInNode->node.module,
                                              & sCompModule )
                          != IDE_SUCCESS );

                /**********************************************
                 *  Looping �� �������� ������ ���� Tree �� �����.
                 *  �̷��� Tree �� OR ������ �����س�����.
                 *
                 *        (OR)
                 *          |
                 *          V
                 *       (Equal)
                 *          |
                 *          V
                 *        (a1) -> (b1)
                 **********************************************/
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             sCompModule,
                                             & sORNode,
                                             ID_TRUE )
                          != IDE_SUCCESS );

                if ( sOldSrc == (qtcNode *)sOldSrcList->node.arguments )
                {
                    sPredStart = sORNode;
                    sPredEnd = sORNode;
                }
                else
                {
                    sNewPrevOR->node.next = (mtcNode *)sORNode;
                    sPredEnd = sORNode;
                }

                sNewPrevOR = sORNode;
            }

            /**********************************************
             *  ������ ���� Tree �� �ϼ��Ǿ���.
             *
             *            ____ sPrevStart  ____ sPrevEnd
             *           /                /
             *        (OR)    --->    (OR)
             *          |               |
             *          V               V
             *       (Equal)         (Equal)
             *          |               |
             *          V               V
             *        (a1) -> (b1)    (a2) -> (b2)
             **********************************************/

            QTC_STMT_COLUMN( aStatement, sOldSrcList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldSrcList )->module = & mtdList;

            QTC_STMT_COLUMN( aStatement, sOldTrgList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldTrgList )->module = & mtdList;
        }

        //---------------------------------------------
        // Source �� list �� �ƴ� ��
        //---------------------------------------------
        else
        {
            if ( ( aInNode->node.module == & mtfEqual )
              || ( aInNode->node.module == & mtfNotEqual ) )
            {
                IDE_RAISE( ERR_INVALID_MODULE );
            }
            else
            {
                // Nothing to do.
            }

            sOldTrgList = (qtcNode *)aInNode->node.arguments->next;

            for ( sOldSrc  = (qtcNode *)aInNode->node.arguments,
                  sOldTrg  = (qtcNode *)sOldTrgList->node.arguments;
                  sOldTrg != NULL;
                  sOldTrg  = (qtcNode *)sOldTrg->node.next )
            {

                IDE_TEST( getCompModule4List( (mtfModule *)aInNode->node.module,
                                              & sCompModule )
                          != IDE_SUCCESS );

                /**********************************************
                 *  Looping �� �������� ������ ���� Tree �� �����.
                 *  �̷��� Tree �� OR ������ �����س�����.
                 *
                 *        (OR)
                 *          |
                 *          V
                 *       (Equal)
                 *          |
                 *          V
                 *        (a1) -> (b1)
                 **********************************************/
                IDE_TEST( makeNewORNodeTree( aStatement,
                                             aQuerySet,
                                             aInNode,
                                             sOldSrc,
                                             sOldTrg,
                                             & sNewSrc,
                                             & sNewTrg,
                                             sCompModule,
                                             & sORNode,
                                             ID_TRUE )
                          != IDE_SUCCESS );

                if ( sOldTrg == (qtcNode *)sOldTrgList->node.arguments )
                {
                    sPredStart = sORNode;
                    sPredEnd = sORNode;
                }
                else
                {
                    sNewPrevOR->node.next = (mtcNode *)sORNode;
                    sPredEnd = sORNode;
                }

                sNewPrevOR = sORNode;
            }

            /**********************************************
             *  ������ ���� Tree �� �ϼ��Ǿ���.
             *
             *            ____ sPrevStart  ____ sPrevEnd
             *           /                /
             *        (OR)    --->    (OR)
             *          |               |
             *          V               V
             *       (Equal)         (Equal)
             *          |               |
             *          V               V
             *        (a1) -> (b1)    (a1) -> (b2)
             **********************************************/

            QTC_STMT_COLUMN( aStatement, sOldTrgList )->column.size = 0;
            QTC_STMT_COLUMN( aStatement, sOldTrgList )->module = & mtdList;
        }
    }
    else
    {
        IDE_RAISE( ERR_INVALID_MODULE );
    }

    //---------------------------------------------
    // ��ȯ�� Tree �� Start OR ���� End OR ��带
    // ��ȯ�Ѵ�.
    //---------------------------------------------

    *aTransStart = sPredStart;
    *aTransEnd   = sPredEnd;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_MODULE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transJoinOperListArgStyle",
                                  "Invalid comparison module" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::merge2ANSIStyleFromTree( qmsFrom     * aFromFrom,
                                           qmsFrom     * aToFrom,
                                           idBool        aFirstIsLeft,
                                           qcDepInfo   * aFindDepInfo,
                                           qmsFrom     * aNewFrom )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description :
 *
 * Implemenation :
 *    aFromFrom ��带 aToFrom ������ merge
 *    1. aFindDepInfo �� �̿��Ͽ� aToFrom ���� �������� ã�´�.
 *    2. �ش� ������ ��� aNewFrom �� ������ �� left, right �� �����Ѵ�.
 *
 *    aFirstIsLeft �� ù��° argument �� aFromFrom �� Left Outer ��
 *    left �������θ� ��Ÿ����. (ID_TRUE/ID_FALSE)
 *
 ***********************************************************************/

    qmsFrom    * sFromFrom = NULL;
    qmsFrom    * sToFrom   = NULL;
    qcDepInfo    sAndDependencies;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::merge2ANSIStyleFromTree::__FT__" );

    IDE_DASSERT( aFromFrom  != NULL );
    IDE_DASSERT( aToFrom    != NULL );
    IDE_DASSERT( aNewFrom   != NULL );

    sFromFrom  = aFromFrom;
    sToFrom    = aToFrom;


    //------------------------------------------------------
    // �� ���� From ��尣���� �������� �ݵ�� �־�� �Ѵ�.
    // �̸� depInfo �� ���� �˻��Ѵ�.
    //------------------------------------------------------

    qtc::dependencyAnd( aFindDepInfo,
                        & sToFrom->depInfo,
                        & sAndDependencies );

    IDE_TEST( qtc::dependencyEqual( & sAndDependencies,
                                    & qtc::zeroDependencies )
              == ID_TRUE );

    //------------------------------------------------------
    // left, right �� ��� leaf node �� ���� ���̻�
    // traverse �� �ʿ䰡 ����.
    //------------------------------------------------------

    if ( ( sToFrom->left->joinType == QMS_NO_JOIN )
      && ( sToFrom->right->joinType == QMS_NO_JOIN ) )
    {

        //------------------------------------------------------
        // �������� left ����� ���
        //------------------------------------------------------

        if( qtc::dependencyEqual( aFindDepInfo,
                                  & sToFrom->left->depInfo )
            == ID_TRUE )
        {
            if ( aFirstIsLeft == ID_TRUE )
            {
                aNewFrom->left = sFromFrom;
                aNewFrom->right = sToFrom->left;
                sToFrom->left = aNewFrom;
            }
            else
            {
                aNewFrom->left = sToFrom->left;
                aNewFrom->right = sFromFrom;
                sToFrom->left = aNewFrom;
            }
        }

        //------------------------------------------------------
        // �������� right ����� ���
        //------------------------------------------------------

        else if( qtc::dependencyEqual( aFindDepInfo,
                                       & sToFrom->right->depInfo )
                 == ID_TRUE )
        {
            if ( aFirstIsLeft == ID_TRUE )
            {
                aNewFrom->left = sFromFrom;
                aNewFrom->right = sToFrom->right;
                sToFrom->right = aNewFrom;
            }
            else
            {
                aNewFrom->left = sToFrom->right;
                aNewFrom->right = sFromFrom;
                sToFrom->right = aNewFrom;
            }
        }

        //------------------------------------------------------
        // �̵����� �ƴϸ� ����
        //------------------------------------------------------

        else
        {
            IDE_RAISE( ERR_RELATION_CONNECT_POINT );
        }

        //------------------------------------------------------
        // �����ϴ� ����� dependency ���� Oring
        //------------------------------------------------------

        IDE_TEST( qtc::dependencyOr( & aNewFrom->left->depInfo,
                                     & aNewFrom->right->depInfo,
                                     & aNewFrom->depInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & sToFrom->depInfo,
                                     & aNewFrom->depInfo,
                                     & sToFrom->depInfo )
                  != IDE_SUCCESS );
    }

    //------------------------------------------------------
    // right ���� leaf �̰�, left ���� tree �� ���
    //------------------------------------------------------

    else if ( ( sToFrom->left->joinType != QMS_NO_JOIN )
           && ( sToFrom->right->joinType == QMS_NO_JOIN ) )
    {
        //------------------------------------------------------
        // �������� left ����� ���
        //------------------------------------------------------

        qtc::dependencyAnd( aFindDepInfo,
                            & sToFrom->left->depInfo,
                            & sAndDependencies );

        if ( qtc::dependencyEqual( & sAndDependencies,
                                   & qtc::zeroDependencies )
             == ID_FALSE )
        {
            IDE_TEST( merge2ANSIStyleFromTree( sFromFrom,
                                               sToFrom->left,
                                               aFirstIsLeft,
                                               aFindDepInfo,
                                               aNewFrom )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �������� right ����� ���
        //------------------------------------------------------

        else
        {
            if ( aFirstIsLeft == ID_TRUE )
            {
                aNewFrom->left = sFromFrom;
                aNewFrom->right = sToFrom->right;
                sToFrom->right = aNewFrom;
            }
            else
            {
                aNewFrom->left = sToFrom->right;
                aNewFrom->right = sFromFrom;
                sToFrom->right = aNewFrom;
            }

            IDE_TEST( qtc::dependencyOr( & aNewFrom->left->depInfo,
                                         & aNewFrom->right->depInfo,
                                         & aNewFrom->depInfo )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �����ϴ� ����� dependency ���� Oring
        //------------------------------------------------------

        IDE_TEST( qtc::dependencyOr( & sToFrom->depInfo,
                                     & aNewFrom->depInfo,
                                     & sToFrom->depInfo )
                  != IDE_SUCCESS );
    }

    //------------------------------------------------------
    // right ���� tree �̰�, left ���� leaf �� ���
    //------------------------------------------------------

    else if ( ( sToFrom->left->joinType == QMS_NO_JOIN )
           && ( sToFrom->right->joinType != QMS_NO_JOIN ) )
    {
        //------------------------------------------------------
        // �������� right ����� ���
        //------------------------------------------------------

        qtc::dependencyAnd( aFindDepInfo,
                            & sToFrom->right->depInfo,
                            & sAndDependencies );

        if ( qtc::dependencyEqual( & sAndDependencies,
                                   & qtc::zeroDependencies )
             == ID_FALSE )
        {
            IDE_TEST( merge2ANSIStyleFromTree( sFromFrom,
                                               sToFrom->right,
                                               aFirstIsLeft,
                                               aFindDepInfo,
                                               aNewFrom )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �������� left ����� ���
        //------------------------------------------------------

        else
        {
            if ( aFirstIsLeft == ID_TRUE )
            {
                aNewFrom->left = sFromFrom;
                aNewFrom->right = sToFrom->left;
                sToFrom->left = aNewFrom;
            }
            else
            {
                aNewFrom->left = sToFrom->left;
                aNewFrom->right = sFromFrom;
                sToFrom->left = aNewFrom;
            }

            IDE_TEST( qtc::dependencyOr( & aNewFrom->left->depInfo,
                                         & aNewFrom->right->depInfo,
                                         & aNewFrom->depInfo )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �����ϴ� ����� dependency ���� Oring
        //------------------------------------------------------

        IDE_TEST( qtc::dependencyOr( & sToFrom->depInfo,
                                     & aNewFrom->depInfo,
                                     & sToFrom->depInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        //  ( ( sToFrom->left->joinType  != QMS_NO_JOIN )
        // && ( sToFrom->right->joinType != QMS_NO_JOIN ) )

        //------------------------------------------------------
        // �������� left ����� ���
        //------------------------------------------------------

        qtc::dependencyAnd( aFindDepInfo,
                            & sToFrom->left->depInfo,
                            & sAndDependencies );

        if ( qtc::dependencyEqual( & sAndDependencies,
                                   & qtc::zeroDependencies )
             == ID_FALSE )
        {
            IDE_TEST( merge2ANSIStyleFromTree( sFromFrom,
                                               sToFrom->left,
                                               aFirstIsLeft,
                                               aFindDepInfo,
                                               aNewFrom )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �������� right ����� ���
        //------------------------------------------------------

        else
        {
            IDE_TEST( merge2ANSIStyleFromTree( sFromFrom,
                                               sToFrom->right,
                                               aFirstIsLeft,
                                               aFindDepInfo,
                                               aNewFrom )
                      != IDE_SUCCESS );
        }

        //------------------------------------------------------
        // �����ϴ� ����� dependency ���� Oring
        //------------------------------------------------------

        IDE_TEST( qtc::dependencyOr( & sToFrom->depInfo,
                                     & aNewFrom->depInfo,
                                     & sToFrom->depInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RELATION_CONNECT_POINT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::merge2ANSIStyleFromTree",
                                  "No connection point for from tree" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::transformJoinOper2From( qcStatement      * aStatement,
                                          qmsQuerySet      * aQuerySet,
                                          qmoJoinOperTrans * aTrans,
                                          qmsFrom          * aNewFrom,
                                          qmoJoinOperRel   * aJoinGroupRel,
                                          qmoJoinOperPred  * aJoinGroupPred )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Join Relation -> From Tree
 *
 * Implemenation :
 *
 *    �Ʒ� ���� ���� ��ȯ����� ���캸��.
 *
 *    select t1.*, t2.*, t3.*, t4.*, t5.*
 *      from t1, t2, t3, t4, t5
 *     where t1.i1    = t2.i1(+)
 *       and t1.i1(+) = t5.i1
 *       and t2.i1    = t4.i1(+)
 *       and t3.i1(+) = t5.i1;
 *    
 *    ���� SQL ���� Join Relation �� �̾ƺ��� ������ ����.
 *    ������ ������ �ٲپ �Ʒ��� ���� table �� ������ ���ĵȴ�.
 *    (Join Group ���� �̹� ���ĵǾ� �� �Լ��� �Ѿ�´�.)
 *          _    _        _    _
 *       (1,2)->(1,5)->(2,4)->(3,5)
 *    
 *    ���� Join Relation �� �Ʒ��� ���� tree ������ ����� �� �ִ�.
 *    
 *                left
 *              /      \
 *            left    left
 *           /   \   /    \
 *         left   1  2    4
 *         /  \
 *        5    3
 *    
 *    ���� ���� tree �� �Ʒ��� ������ ���� �ϼ��ȴ�.
 *    ��� Join �� Left Outer Join ���θ� �����ϰ�,
 *    (+)�� ���� ���� ���̺��� ������ ��ġ��Ų��.
 *    Relation List �� �ϳ��� �����ͼ� Tree �� �������� ã�� �������ִ� �����̴�.
 *
 *    �ϳ��� Join Group ������ table ������ ���ĵ� Join Relation List ��
 *    �� List ���� dependency ���̺��� ���� ������踦 ���� �� �ۿ� ����.
 *    ����, ��������δ� �ϳ��� Join Group ����
 *    Join Relation List �� �ϳ��� tree �� �ϼ��ȴ�.
 *    
 *            _
 *    (��) (1,2)
 *    
 *                left           --> on t1.i1=t2.i1
 *              /      \
 *             1        2
 *    
 *          _
 *    (��) (1,5)
 *    
 *                left
 *              /      \
 *            left      2        --> on t5.i1=t1.i1
 *           /    \
 *          5      1
 *    
 *            _
 *    (��) (2,4)
 *    
 *                left
 *              /      \
 *            left    left       --> on t2.i1=t4.i1
 *           /    \   /   \
 *          5      1 2     4
 *    
 *          _
 *    (��) (3,5)
 *    
 *                left
 *              /      \
 *            left    left
 *           /    \   /   \
 *         left    1 2     4     --> on t5.i1=t3.i1
 *        /   \
 *       5     3
 *    
 *    ���� �������� tree ������ ANSI �������� ǥ���ϸ� �Ʒ��� ����.
 *    ���� ��ȸ�� �غ��� ������ (+)�� ����� ������ ����� ��Ȯ��
 *    ��ġ���� Ȯ���� �� �ִ�.
 *    �������� bottom up ���·� ������ ǥ�����ָ� �ȴ�.
 *    ���� tree ������ �񱳰� ������ ������ ǥ���ߴ�.
 *    
 *    select t1.*, t2.*, t3.*, t4.*, t5.*
 *      from                t5
 *                    left                         outer join
 *                          t3
 *                      on t5.i1=t3.i1
 *                left                             outer join
 *                     t1
 *                  on t5.i1=t1.i1
 *           left                                  outer join
 *                     t2
 *                left                             outer join
 *                     t4
 *                  on t2.i1=t4.i1
 *             on t1.i1=t2.i1;
 *
 ***********************************************************************/

    qmsFrom          * sFromStart      = NULL;
    qmsFrom          * sFromCur        = NULL;
    qmsFrom          * sFromLeft       = NULL;
    qmsFrom          * sFromRight      = NULL;
    qmsFrom          * sFromPrev       = NULL;
    qmsFrom          * sFromLeftPrev   = NULL;
    qmsFrom          * sFromRightPrev  = NULL;
    qtcNode          * sErrNode        = NULL;

    UShort             sLeft;
    UShort             sRight;
    qcDepInfo          sLeftDepInfo;
    qcDepInfo          sRightDepInfo;
    qcDepInfo          sAndDependencies;
    idBool             sFirstIsLeft;
    UChar              sJoinOper;

    qcuSqlSourceInfo   sqlInfo;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transformJoinOper2From::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aNewFrom   != NULL );
    IDE_DASSERT( aJoinGroupRel   != NULL );
    IDE_DASSERT( aJoinGroupPred  != NULL );


    sFromStart = (qmsFrom *)aQuerySet->SFWGH->from;


    //------------------------------------------------------
    // Join Relation �� �ΰ� dependency table �� ����
    // (+) ������ ���� Left, Right ���̺��� ����
    // (+) �� ���� ���� Left
    //------------------------------------------------------

    sJoinOper = qtc::getDependJoinOper( &aJoinGroupRel->depInfo, 0 );

    if ( QMO_JOIN_OPER_EXISTS( sJoinOper )
         == ID_FALSE )
    {
        sLeft = qtc::getDependTable( &aJoinGroupRel->depInfo,0 );
        sRight = qtc::getDependTable( &aJoinGroupRel->depInfo,1 );
    }
    else
    {
        sRight = qtc::getDependTable( &aJoinGroupRel->depInfo,0 );
        sLeft = qtc::getDependTable( &aJoinGroupRel->depInfo,1 );
    }

    //------------------------------------------------------
    // aNewFrom ��带 �߰��� ��ġ�� ã�� ���� traverse ��
    // ���� Left/Right table ��
    //------------------------------------------------------

    qtc::dependencySet( sLeft, &sLeftDepInfo );
    qtc::dependencySet( sRight, &sRightDepInfo );

    //------------------------------------------------------
    // ���� Left, Right ������ �̿��Ͽ� qmsFrom List ��
    // Ž���� �� ã�� qmsFrom type �� ���� �ٸ� ó�� ����
    //
    //  1. Left = QMS_NO_JOIN, Right = QMS_NO_JOIN
    //     => NewFrom(QMS_LEFT_OUTER_JOIN) �� Left, Right ����
    //
    //  2. Left != QMS_NO_JOIN, Right = QMS_NO_JOIN
    //     => Left ���� �̿��Ͽ� Tree Ž���Ͽ� ���� dependency table
    //        ������ ���� ��带 �߰��ϸ� NewFrom(QMS_LEFT_OUTER_JOIN)��
    //        �ش� ��� ��� ��ü�� �� Left,Right ����
    //
    //  3. Left = QMS_NO_JOIN, Right != QMS_NO_JOIN
    //     => Right ���� �̿��Ͽ� Tree Ž���Ͽ� ���� dependency table
    //        ������ ���� ��带 �߰��ϸ� NewFrom(QMS_LEFT_OUTER_JOIN)��
    //        �ش� ��� ��� ��ü�� �� Left,Right ����
    //
    //  4. Left != QMS_NO_JOIN, Right != QMS_NO_JOIN
    //     => Join Relation �� �ϳ��� Tree �� �ϼ��Ǳ� ������
    //        �̷� ���� �߻��� �� ����. (����)
    //------------------------------------------------------

    //------------------------------------------------------
    // sLeftDepInfo �� sRightDepInfo ������ �̿��Ͽ�
    // ������ ���̺��� ���Ե� From node �� ã�´�.
    //
    //     sFromRightPrev sFromLeftPrev
    //          |             |
    //          v             v
    // from -> (5) -> (2) -> (3) -> (1) -> (4) -> null
    //                 ^             ^
    //                 |             |
    //             sFromRight    sFromLeft
    //
    // �Ǵ�, �Ϻ� Join Relation �� �̹� tree ���·� ��ȯ�� ���¶��
    // ������ ���� ������ ���� �ִ�. (L �� QMS_LEFT_OUTER_JOIN)
    //
    //              sFromRight
    //                 |
    //     sFromRightPrev sFromLeftPrev
    //          |      |      |
    //          v      v      v
    // from -> (5) -> (L) -> (3) -> (1) -> (4) -> null
    //               /   \           ^
    //             (7)   (6)         |
    //                           sFromLeft
    //------------------------------------------------------

    sFromLeft  = NULL;
    sFromRight = NULL;

    for ( sFromCur  = aQuerySet->SFWGH->from;
          sFromCur != NULL;
          sFromPrev = sFromCur, sFromCur = sFromCur->next )
    {
        qtc::dependencyAnd( & sFromCur->depInfo,
                            & sLeftDepInfo,
                            & sAndDependencies );

        if( qtc::dependencyEqual( & sAndDependencies,
                                  & qtc::zeroDependencies )
            == ID_FALSE )
        {
            sFromLeft = sFromCur;
            sFromLeftPrev = sFromPrev;
        }
        else
        {
            // Nothing to do.
        }

        qtc::dependencyClear( & sAndDependencies );

        qtc::dependencyAnd( & sFromCur->depInfo,
                            & sRightDepInfo,
                            & sAndDependencies );

        if( qtc::dependencyEqual( & sAndDependencies,
                                  & qtc::zeroDependencies )
            == ID_FALSE )
        {
            sFromRight = sFromCur;
            sFromRightPrev = sFromPrev;
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sFromLeft != NULL ) && ( sFromRight != NULL ) )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( ( sFromLeft == NULL ) || ( sFromRight == NULL ) )
    {
        sErrNode = aQuerySet->SFWGH->where;
        sqlInfo.setSourceInfo ( aStatement, & sErrNode->position );

        IDE_RAISE( ERR_ABORT_NOT_ALLOW_OUTER_JOIN_INAPPROPRIATE_TABLE );
    }
    else
    {
        // Nothing to do.
    }

    /*------------------------------------------------------
     * Left, Right ��带 from list ���� ����
     *
     * from -> (3) -> (4) -> null
     *
     *   sFromRight sFromLeft
     *        |       |
     *        v       v
     *       (5)     (L)
     *              /   \
     *            (1)   (2)
     *-----------------------------------------------------*/

    //------------------------------------------------------
    // Left �� Right �� ��ġ�� ���� ó��
    //  0. Left �� Right�� ���� ��� �̴� NewFrom���� ������ Ʈ����
    //     ���� ���̺��� ��� �ִ� ��� (BUG-37693)
    //  1. Left �� list �� �Ǿ��̰� �� ���� Right �� �ٷ� �ö�
    //  2. Right �� list �� �Ǿ��̰� �� ���� Left �� �ٷ� �ö�
    //  3. Left �� Right �� list �� �߰��� �� ��
    //------------------------------------------------------

    if ( sFromLeft == sFromRight )
    {
        /* BUG-37693
         * �̹� Ʈ���� ������ from�� ��� ���� ���̺��� �ִ� ����
         * ���̻� Ʈ�� �����̳� ������ �ʿ� ����.
         */
        IDE_CONT(NORMAL_EXIT);
    }
    else
    {
        /* Nothing to do */
    }

    if ( sFromLeft == sFromStart )
    {
        if ( sFromLeft->next == sFromRight )
        {
            aQuerySet->SFWGH->from = sFromRight->next;

            sFromLeft->next  = NULL;
            sFromRight->next = NULL;
        }
        else
        {
            aQuerySet->SFWGH->from = sFromLeft->next;
            sFromLeft->next        = NULL;

            IDE_TEST_RAISE( sFromRightPrev == NULL, ERR_INVALID_RIGHT_PRIV );
            sFromRightPrev->next = sFromRight->next;
            sFromRight->next     = NULL;
        }
    }
    else
    {
        if ( sFromRight == sFromStart )
        {
            if ( sFromRight->next == sFromLeft )
            {
                aQuerySet->SFWGH->from = sFromLeft->next;

                sFromLeft->next  = NULL;
                sFromRight->next = NULL;
            }
            else
            {
                aQuerySet->SFWGH->from = sFromRight->next;
                sFromRight->next       = NULL;

                IDE_TEST_RAISE( sFromLeftPrev == NULL, ERR_INVALID_LEFT_PRIV );
                sFromLeftPrev->next = sFromLeft->next;
                sFromLeft->next     = NULL;
            }
        }
        else
        {
            if ( sFromLeft->next == sFromRight )
            {
                IDE_TEST_RAISE( sFromLeftPrev == NULL, ERR_INVALID_LEFT_PRIV );
                sFromLeftPrev->next = sFromRight->next;

                sFromLeft->next  = NULL;
                sFromRight->next = NULL;
            }
            else
            {
                if ( sFromRight->next == sFromLeft )
                {
                    IDE_TEST_RAISE( sFromRightPrev == NULL,
                                    ERR_INVALID_RIGHT_PRIV );
                    sFromRightPrev->next = sFromLeft->next;

                    sFromLeft->next  = NULL;
                    sFromRight->next = NULL;
                }
                else
                {
                    IDE_TEST_RAISE( sFromLeftPrev == NULL,
                                    ERR_INVALID_LEFT_PRIV );
                    sFromLeftPrev->next = sFromLeft->next;
                    sFromLeft->next     = NULL;

                    IDE_TEST_RAISE( sFromRightPrev == NULL,
                                    ERR_INVALID_RIGHT_PRIV);
                    sFromRightPrev->next = sFromRight->next;
                    sFromRight->next     = NULL;
                }
            }
        }
    }

    //------------------------------------------------------
    // Left, Right ����� joinType �� �°� ó��
    //------------------------------------------------------

    if ( ( sFromLeft->joinType  == QMS_NO_JOIN ) &&
         ( sFromRight->joinType == QMS_NO_JOIN ) )
    {
        /*------------------------------------------------------
         * From node list �� �Ʒ��� ���� ������ ���� ó���̴�.
         * sFromLeft �Ǵ� sFromRight �� list �� �Ǿ��� ���� ����Ѵ�.
         *
         *     sFromRightPrev sFromLeftPrev
         *          |             |
         *          v             v
         * from -> (5) -> (2) -> (3) -> (1) -> (4) -> null
         *                 ^             ^
         *                 |             |
         *             sFromRight    sFromLeft
         *
         * ó������ From List �� ������ ���� ����ȴ�.
         *
         * from -> (L) -> (5) -> (3) -> (4) -> null
         *        /   \
         *      (1)   (2)
         *-----------------------------------------------------*/

        //------------------------------------------------------
        // aNewFrom �� Left, Right ��� ����
        //------------------------------------------------------

        aNewFrom->left  = sFromLeft;
        aNewFrom->right = sFromRight;

        //------------------------------------------------------
        // �����ϴ� ����� dependency ���� Oring
        //------------------------------------------------------

        IDE_TEST( qtc::dependencyOr( & sFromLeft->depInfo,
                                     & sFromRight->depInfo,
                                     & aNewFrom->depInfo )
                  != IDE_SUCCESS );

        //------------------------------------------------------
        // aNewFrom �� onCondition �ۼ�
        //------------------------------------------------------

        IDE_TEST( movePred2OnCondition( aStatement,
                                        aTrans,
                                        aNewFrom,
                                        aJoinGroupRel,
                                        aJoinGroupPred )
                  != IDE_SUCCESS );

        //------------------------------------------------------
        // �ϼ��� tree �� from list �� �Ǿտ� ����
        //------------------------------------------------------

        aNewFrom->next         = aQuerySet->SFWGH->from;
        aQuerySet->SFWGH->from = aNewFrom;
    }
    else
    {
        if ( ( sFromLeft->joinType  != QMS_NO_JOIN ) &&
             ( sFromRight->joinType == QMS_NO_JOIN ) )
        {
            /*------------------------------------------------------
             * From node list �� �Ʒ��� ���� ������ ���� ó���̴�.
             *    _
             * (5,2) �� �߰�
             *
             *    sFromRight sFromLeft
             *          |      |
             *          v      v
             * from -> (5) -> (L) -> (3) -> (4) -> null
             *               /   \
             *             (1)   (2)
             *
             * ó������ From List �� ������ ���� ����ȴ�.
             *
             * from -> (L) -> (3) -> (4) -> null
             *        /   \
             *      (1)   (L) <- aNewFrom
             *           /   \
             *         (5)   (2)
             *-----------------------------------------------------*/

            //------------------------------------------------------
            // tree �� �������� ã�Ƽ� aNewFrom �� ����
            // Merge sFromRight -> sFromLeft
            //------------------------------------------------------

            sFirstIsLeft = ID_FALSE;

            IDE_TEST( merge2ANSIStyleFromTree( sFromRight,
                                               sFromLeft,
                                               sFirstIsLeft,
                                               & sLeftDepInfo,
                                               aNewFrom )
                      != IDE_SUCCESS );


            //------------------------------------------------------
            // aNewFrom �� onCondition �ۼ�
            //------------------------------------------------------

            IDE_TEST( movePred2OnCondition( aStatement,
                                            aTrans,
                                            aNewFrom,
                                            aJoinGroupRel,
                                            aJoinGroupPred )
                      != IDE_SUCCESS );

            //------------------------------------------------------
            // �ϼ��� tree �� from list �� �Ǿտ� ����
            //------------------------------------------------------

            sFromLeft->next        = aQuerySet->SFWGH->from;
            aQuerySet->SFWGH->from = sFromLeft;
        }
        else
        {
            if ( ( sFromLeft->joinType  == QMS_NO_JOIN ) &&
                 ( sFromRight->joinType != QMS_NO_JOIN ) )
            {
                /*------------------------------------------------------
                 * From node list �� �Ʒ��� ���� ������ ���� ó���̴�.
                 *
                 *  _
                 * (1,5) �� �߰�
                 *
                 *    sFromRight sFromLeft
                 *          |      |
                 *          v      v
                 * from -> (L) -> (5) -> (3) -> (4) -> null
                 *        /   \
                 *      (1)   (2)
                 *
                 * ó������ From List �� ������ ���� ����ȴ�.
                 *
                 * from -> (L) -> (3) -> (4) -> null
                 *        /   \
                 *      (L)   (2)
                 *     /   \
                 *   (5)   (1)
                 *       ^
                 *       |
                 *    aNewFrom
                 *-----------------------------------------------------*/

                //------------------------------------------------------
                // tree �� �������� ã�Ƽ� aNewFrom �� ����
                //------------------------------------------------------

                sFirstIsLeft = ID_TRUE;

                IDE_TEST( merge2ANSIStyleFromTree( sFromLeft,
                                                   sFromRight,
                                                   sFirstIsLeft,
                                                   & sRightDepInfo,
                                                   aNewFrom )
                             != IDE_SUCCESS );


                //------------------------------------------------------
                // aNewFrom �� onCondition �ۼ�
                //------------------------------------------------------

                IDE_TEST( movePred2OnCondition( aStatement,
                                                aTrans,
                                                aNewFrom,
                                                aJoinGroupRel,
                                                aJoinGroupPred )
                          != IDE_SUCCESS );

                //------------------------------------------------------
                // �ϼ��� tree �� from list �� �Ǿտ� ����
                //------------------------------------------------------

                sFromRight->next = aQuerySet->SFWGH->from;
                aQuerySet->SFWGH->from = sFromRight;
            }
            else
            {
                //------------------------------------------------------
                //  ( ( sFromLeft->qmsJoinType  != QMS_NO_JOIN )
                // && ( sFromRight->qmsJoinType != QMS_NO_JOIN ) )
                //
                // Join Relation �� �ϳ��� From Tree �� ��ȯ�� �Ǳ� ������
                // �̷� ���� ���� �� ����. -> ����
                //------------------------------------------------------

                IDE_RAISE( ERR_INVALID_FROM_JOIN_TYPE );
            }
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_NOT_ALLOW_OUTER_JOIN_INAPPROPRIATE_TABLE )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_ALLOW_OUTER_JOIN_INAPPROPRIATE_TABLE,
                                  sqlInfo.getErrMessage() ) );
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_INVALID_FROM_JOIN_TYPE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transformJoinOper2From",
                                  "Invalide join type" ));
    }
    IDE_EXCEPTION( ERR_INVALID_RIGHT_PRIV )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transformJoinOper2From",
                                  "sFromRightPrev is null" ));
    }
    IDE_EXCEPTION( ERR_INVALID_LEFT_PRIV )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOuterJoinOper::transformJoinOper2From",
                                  "sFromLeftPrev is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOuterJoinOper::transformJoinOper( qcStatement       * aStatement,
                                     qmsQuerySet       * aQuerySet,
                                     qmoJoinOperTrans  * aTrans )
{
/***********************************************************************
 *
 * PROJ-1653 Outer Join Operator (+)
 *
 * Description : Outer Join Operator �� ANSI ���·��� ��ȯ
 *
 * Implemenation :
 *    1. Join Relation List �� ��ȸ�ϸ� qmsFrom tree ����
 *    2. qmsFrom tree �� ������ Join Predicate �� ����Ű�� qtcNode ��带
 *       onCondition ���� �̵�
 *    3. oneTablePred ���� ���� ���̺� (+)�� �ִ� ���� ���
 *       ���������� onCondition ���� �̵�
 *
 ***********************************************************************/

    qmoJoinOperGroup    * sJoinGroup    = NULL;
    qmoJoinOperGroup    * sCurJoinGroup = NULL;
    qmoJoinOperPred     * sJoinPred     = NULL;
    qmoJoinOperRel      * sJoinRel      = NULL;
    qmsFrom             * sNewFrom      = NULL;
    qtcNode             * sNorCur       = NULL;

    UInt                  sJoinGroupCnt;
    UInt                  i, j;
    idBool                sAvailRel     = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinOper::transformJoinOper::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aTrans     != NULL );

    sJoinGroup    = aTrans->joinOperGroup;
    sJoinGroupCnt = aTrans->joinOperGroupCnt;


    for ( i = 0 ;
          i < sJoinGroupCnt ;
          i++ )
    {
        sCurJoinGroup = & sJoinGroup[i];
        sJoinPred = sCurJoinGroup->joinPredicate;

        for ( j = 0; j < sCurJoinGroup->joinRelationCnt; j++ )
        {
            sJoinRel = (qmoJoinOperRel *) & sCurJoinGroup->joinRelation[j];

            //------------------------------------------------------
            // Join Relation �� dependency ���� ��ȿ�� �˻�
            //------------------------------------------------------

            IDE_TEST( isAvailableJoinOperRel( & sJoinRel->depInfo,
                                              & sAvailRel )
                      != IDE_SUCCESS);

            //------------------------------------------------------
            // Join Relation �� �� ���̺� ���� From �ڷᱸ����
            // Left Outer Join ������ �ٲٱ� ���� qmsFrom ����ü �Ҵ�
            //------------------------------------------------------

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmsFrom ),
                                                     (void**)&sNewFrom )
                      != IDE_SUCCESS );

            //------------------------------------------------------
            // qmsFrom ����ü �ʱ�ȭ
            //------------------------------------------------------

            QCP_SET_INIT_QMS_FROM(sNewFrom);

            sNewFrom->joinType = QMS_LEFT_OUTER_JOIN;

            //------------------------------------------------------
            // Join Relation �ϳ��� �����Ͽ� ������ ������ ����
            //
            //  1. Left Outer Join Tree ���� (sNewFrom)
            //  2. aTrans ���� joinPredicate ���� depInfo �� ���� ����
            //     ã�Ƽ� �ش�Ǵ� normalCNF ��带 ã�� ���� onCondition �� ����.
            //     normalCNF ������ ����.
            //  3. aTrans ���� oneTablePred �� ���ؼ��� 2���� ���� ���� ����
            //
            //  aTrans �� �����ϴ� ���� normalCNF �� oneTablePred �� �����ϱ� ����.
            //------------------------------------------------------

            IDE_TEST( transformJoinOper2From( aStatement,
                                              aQuerySet,
                                              aTrans,
                                              sNewFrom,
                                              sJoinRel,
                                              sJoinPred )
                      != IDE_SUCCESS );
        }
    }

    //------------------------------------------------------
    // ������ normalCNF predicate node �� ��ȸ�ϸ�
    // ���������� dependency & joinOper Oring
    //------------------------------------------------------

    if ( aTrans->normalCNF != NULL )
    {
        for ( sNorCur  = (qtcNode *)aTrans->normalCNF->node.arguments;
              sNorCur != NULL ;
              sNorCur  = (qtcNode *)sNorCur->node.next )
        {
            IDE_TEST( qtc::dependencyOr( & aTrans->normalCNF->depInfo,
                                         & sNorCur->depInfo,
                                         & aTrans->normalCNF->depInfo )
                      != IDE_SUCCESS );
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
