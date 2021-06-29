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
 * $Id: qmoKeyRange.cpp 86786 2020-02-27 08:04:12Z donovan.seo $
 *
 * Description : Key Range ������
 *
 *     < keyRange �������� >
 *     1. DNF ���·� ��� ��ȯ
 *     2. keyRange�� ũ�� ����
 *     3. keyRange ������ ���� �޸� �غ�
 *     4. keyRange ����
 *
 *     < keyRange ���������� ����Ǵ� ���� >
 *    --------------------------------------------------------------
 *    |           | fixed keyRange         |   variable keyRange   |
 *    --------------------------------------------------------------
 *    | prepare   | 1.DNF ���·� ��庯ȯ  | 1.DNF ���·� ��庯ȯ |
 *    | �ܰ�      | 2.keyRange ũ�� ����   |                       |
 *    |           | 3.�޸��غ�           |                       |
 *    |           | 4.keyRange����         |                       |
 *    |-------------------------------------------------------------
 *    | execution |                        | 2.keyRange ũ�� ����  |
 *    | �ܰ�      |                        | 3.�޸��غ�          |
 *    |           |                        | 4.keyRange ����       |
 *    --------------------------------------------------------------
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qtc.h>
#include <qmgProjection.h>
#include <qmoPredicate.h>
#include <qmoKeyRange.h>

extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;

extern mtfModule mtfInlist;

IDE_RC
qmoKeyRange::estimateKeyRange( qcTemplate  * aTemplate,
                               qtcNode     * aNode,
                               UInt        * aRangeSize )
{
/***********************************************************************
 *
 * Description : Key Range�� ũ�⸦ �����Ѵ�.
 *
 *   keyRange�� ũ�� =   (1) �񱳿����ڿ� ���� ���� range size
 *                     + (2) and merge�� �ʿ��� �ִ� range size
 *                     + (3) or merge�� �ʿ��� �ִ� range size
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt      sPrevRangeCount;
    UInt      sRangeCount = 0;
    UInt      sCount;
    UInt      sRangeSize = 0;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::estimateKeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aRangeSize != NULL );

    //--------------------------------------
    // estimate size
    //--------------------------------------

    // aNode�� DNF�� ��ȯ�� ������ ����̴�.

    if( ( aNode->node.lflag &
          ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
        == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        // OR �� ������

        // OR ���� ��忡 ���� size ��
        // and merge�� or merge�� �ʿ��� �ִ� range count�� ���Ѵ�.

        sNode = (qtcNode *)(aNode->node.arguments);
        sPrevRangeCount = 1;

        while( sNode != NULL )
        {
            sCount = 0;
            IDE_TEST( estimateRange( aTemplate,
                                     sNode,
                                     &sCount,
                                     &sRangeSize )
                      != IDE_SUCCESS );

            // OR merge�� ���� �ִ� range ������ ���Ѵ�.
            sPrevRangeCount = sPrevRangeCount + sCount;
            sRangeCount = sRangeCount + sPrevRangeCount;

            sNode = (qtcNode *)(sNode->node.next);
        }
    }
    else
    {
        // Nothing To Do
    }

    // OR ���� ��忡 ���� ����� ������, ��ü range size ���
    // (1) AND ��忡 ���� size ��� ( �񱳿����� + and merge ) +
    // (2) OR  ��忡 ���� or merge�� ���� size ���            +
    // (3) OR  ��忡 ���� or merge�� range list�� qsort�ϱ� ����
    //     �ڷᱸ�� �迭�� ����� ���� size ���
    //     (fix BUG-9378)
    sRangeSize =
        sRangeSize +
        ( sRangeCount * idlOS::align8((UInt) ID_SIZEOF(smiRange) ) ) +
        ( sRangeCount * ID_SIZEOF(smiRange *) ) ;

    *aRangeSize = sRangeSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoKeyRange::makeKeyRange( qcTemplate  * aTemplate,
                           qtcNode     * aNode,
                           UInt          aKeyCount,
                           mtcColumn   * aKeyColumn,
                           UInt        * aKeyColsFlag,
                           UInt          aCompareType,
                           smiRange    * aRangeArea,
                           UInt          aRangeAreaSize, 
                           smiRange   ** aRange,
                           qtcNode    ** aFilter )
{
/***********************************************************************
 *
 * Description : Key Range�� �����Ѵ�.
 *
 *     Key Range�� �����ϱ� ���� public interface.
 *
 * Implementation :
 *
 *     Key Range�� Key Filter�� ���� range ������ ���� �����ϹǷ�,
 *     ���������δ� makeRange()��� �����Լ��� ȣ���Ѵ�.
 *     �̶�, �Է����ڷ� Key Range�� ���� range�� �����϶�� ������ �ѱ��.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makeKeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aKeyCount > 0 );
    IDE_DASSERT( aKeyColumn != NULL );
    IDE_DASSERT( aKeyColsFlag != NULL );
    IDE_DASSERT( aRangeArea != NULL );
    IDE_DASSERT( aRange != NULL );
    IDE_DASSERT( aFilter != NULL );

    //--------------------------------------
    // keyRange ����
    //--------------------------------------

    IDE_TEST( makeRange( aTemplate,
                         aNode,
                         aKeyCount,
                         aKeyColumn,
                         aKeyColsFlag,
                         ID_TRUE,
                         aCompareType,
                         aRangeArea,
                         aRangeAreaSize,
                         aRange,
                         aFilter ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoKeyRange::makeKeyFilter( qcTemplate  * aTemplate,
                            qtcNode     * aNode,
                            UInt          aKeyCount,
                            mtcColumn   * aKeyColumn,
                            UInt        * aKeyColsFlag,
                            UInt          aCompareType,
                            smiRange    * aRangeArea,
                            UInt          aRangeAreaSize,
                            smiRange   ** aRange,
                            qtcNode    ** aFilter )
{
/***********************************************************************
 *
 * Description : Key Filter�� �����Ѵ�.
 *
 *    Key Filter�� �����ϱ� ���� public interface.
 *
 *    Key Range������ �޸� Key Column�� ���ӵ� Column�� ������ �ʿ䰡
 *    ����.
 *
 * Implementation :
 *
 *     Key Range�� Key Filter�� ���� range ������ ���� �����ϹǷ�,
 *     ���������δ� makeRange()��� �����Լ��� ȣ���Ѵ�.
 *     �̶�, �Է����ڷ� Key Filter�� ���� range�� �����϶�� ������ �ѱ��.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makeKeyFilter::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aKeyCount > 0 );
    IDE_DASSERT( aKeyColumn != NULL );
    IDE_DASSERT( aKeyColsFlag != NULL );
    IDE_DASSERT( aRangeArea != NULL );
    IDE_DASSERT( aRange != NULL );
    IDE_DASSERT( aFilter != NULL );

    //--------------------------------------
    // keyFilter ����
    //--------------------------------------

    IDE_TEST( makeRange( aTemplate,
                         aNode,
                         aKeyCount,
                         aKeyColumn,
                         aKeyColsFlag,
                         ID_FALSE,
                         aCompareType,
                         aRangeArea,
                         aRangeAreaSize,
                         aRange,
                         aFilter )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoKeyRange::makeNotNullRange( void               * aPredicate,
                               mtcColumn          * aKeyColumn,
                               smiRange           * aRange )
{
/***********************************************************************
 *
 * Description : Indexable MIN, MAX ������ ���� Not Null Range ����
 *
 *     < Indexable MIN, MAX >
 *
 *     MIN(), MAX() aggregation�� ���, �ش� column�� �ε����� �����Ѵٸ�,
 *     �ε����� ����Ͽ� �� �Ǹ� fetch�����ν� ���ϴ� ����� ���� �� �ִ�.
 *     ��) select min(i1) from t1; index on T1(i1)
 *
 *     MAX()�� ���, NULL value�� �����ϸ� NULL value�� ���� ū ������
 *     fetch �ǹǷ�, not null range�� �����ؼ�,
 *     NULL ���� index scan��󿡼� �����Ѵ�.
 *
 * Implementation :
 *
 *     1. keyRange ������ ���� size ���ϱ�
 *     2. �޸� �Ҵ�ޱ�
 *     3. not null range ����
 *
 ***********************************************************************/

    mtkRangeInfo   sRangeInfo;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makeNotNullRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aKeyColumn != NULL );
    IDE_DASSERT( aRange     != NULL );

    //--------------------------------------
    // indexable MIN/MAX ������ ���� not null range ����
    //--------------------------------------

    // not null range ����
    sRangeInfo.column    = aKeyColumn;
    sRangeInfo.argument  =  0; // not used
    sRangeInfo.direction = MTD_COMPARE_ASCENDING; // not used
    sRangeInfo.columnIdx = 0; //NotNull�� ��� ù��° KeyColumn�� �ٷ�
    sRangeInfo.useOffset = 0;

    if ( ( aKeyColumn->column.flag & SMI_COLUMN_STORAGE_MASK )
             == SMI_COLUMN_STORAGE_DISK )
    {
        sRangeInfo.compValueType = MTD_COMPARE_STOREDVAL_MTDVAL;
    }
    else
    {
        if( (aKeyColumn->column.flag & SMI_COLUMN_TYPE_MASK)
             == SMI_COLUMN_TYPE_FIXED )
        {
            /*
             * PROJ-2433
             * Direct Key Index�� ���� key compare �Լ� type ����
             */
            if ( ( smiTable::getIndexInfo( ((qmnCursorPredicate *)aPredicate)->index->indexHandle ) &
                 SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
            {
                sRangeInfo.compValueType = MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL;
            }
            else
            {
                sRangeInfo.compValueType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
            }
        }
        else
        {
            IDE_DASSERT( ( (aKeyColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                           == SMI_COLUMN_TYPE_VARIABLE ) ||
                         ( (aKeyColumn->column.flag & SMI_COLUMN_TYPE_MASK)
                           == SMI_COLUMN_TYPE_VARIABLE_LARGE ) );

            /*
             * PROJ-2433
             * Direct Key Index�� ���� key compare �Լ� type ����
             */
            if ( ( smiTable::getIndexInfo( ((qmnCursorPredicate *)aPredicate)->index->indexHandle ) &
                 SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
            {
                sRangeInfo.compValueType = MTD_COMPARE_INDEX_KEY_MTDVAL;
            }
            else
            {
                sRangeInfo.compValueType = MTD_COMPARE_MTDVAL_MTDVAL;
            }
        }
    }

    IDE_TEST( mtk::extractNotNullRange( NULL,
                                        NULL,
                                        &sRangeInfo,
                                        aRange )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoKeyRange::estimateRange( qcTemplate  * aTemplate,
                            qtcNode     * aNode,
                            UInt        * aRangeCount,
                            UInt        * aRangeSize )
{
/***********************************************************************
 *
 * Description : Key Range�� ũ�⸦ �����Ѵ�.
 *
 *   keyRange�� ũ�� =   (1) �񱳿����ڿ� ���� ���� range size
 *                     + (2) and merge�� �ʿ��� �ִ� range size
 *                     + (3) or merge�� �ʿ��� �ִ� range size
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt      sIsNotRangeCnt = 0; // !=, not between ����
    UInt      sSize = 0;
    UInt      sPrevRangeCount;
    UInt      sCount;
    UInt      sAndArgCnt = 0;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::estimateRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aRangeCount != NULL );
    IDE_DASSERT( aRangeSize != NULL );

    //--------------------------------------
    // estimate size
    //--------------------------------------

    // aNode�� DNF�� ��ȯ�� ������ ����̴�.

    if( ( aNode->node.lflag &
               ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
             == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        // AND �� ������

        sNode = (qtcNode *)(aNode->node.arguments);
        sAndArgCnt++;
        sPrevRangeCount = 0;

        IDE_TEST( estimateRange( aTemplate,
                                 sNode,
                                 &sPrevRangeCount,
                                 aRangeSize )
                  != IDE_SUCCESS );

        if( sNode->node.next == NULL )
        {

            (*aRangeCount) = sPrevRangeCount;
        }
        else
        {
            sNode = (qtcNode *)(sNode->node.next);

            while( sNode != NULL )
            {
                sAndArgCnt ++;
                sCount = 0;
                IDE_TEST( estimateRange( aTemplate,
                                         sNode,
                                         &sCount,
                                         aRangeSize )
                          != IDE_SUCCESS );

                sPrevRangeCount = sPrevRangeCount + sCount - 1;
                (*aRangeCount) = (*aRangeCount) + sPrevRangeCount;

                // fix BUG-12254
                if( sCount > 1 )
                {
                    // !=, not between�� ���, �� ������ ���
                    sIsNotRangeCnt += 1;
                }
                else
                {
                    // Nothing To Do
                }

                sNode = (qtcNode *)(sNode->node.next);
            }

            if( sIsNotRangeCnt > 0 )
            {
                // range ���縦 ����
                // smiRange, mtkRangeCallBack�� ���� size ���
                // ���� : qmoKeyRange::makeRange() �Լ�
                IDE_TEST(  mtk::estimateRangeDefault( NULL,
                                                      NULL,
                                                      0,
                                                      & sSize )
                           != IDE_SUCCESS );

                // Not range�� ���� range ������ �ִ� notRangeCnt + 1 �� ��.
                // ��) i1 != 1 and i1 != 2 : range ���� 3��
                // --> i1 < 1 and 1 < i1 < 2 and i1 > 2
                // (1) not range�� composite index�� ��밡�� �������÷��ΰ��,
                //     ���� �÷������� composite range�� �����Ƿ�
                //     ���� not range ������ŭ�� ũ�Ⱑ �ʿ�
                sSize *= sIsNotRangeCnt;

                // (2) range �����, range���� mtkRangeCallBack �� ����
                //     not range ���� �÷������� composite ó����
                //     callBack�� ��� �����ؾ� ��.
                (*aRangeSize) += sSize;
                (*aRangeSize) +=
                    (sAndArgCnt-sIsNotRangeCnt) * 2 *
                    idlOS::align8((UInt) ID_SIZEOF(mtkRangeCallBack))
                    * sIsNotRangeCnt;
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        // �� ������

        // �񱳿����ڿ� ���� size estimate
        // BUG-42283 host variable predicate �� ���� estimateRange size �� 0 �̴�.
        // (ex) WHERE i1 = :a AND i2 = :a OR :b NOT LIKE 'a%'
        //                                   ^^^^^^^^^^^^^^^^
        if ( qtc::haveDependencies( &aNode->depInfo ) == ID_TRUE )
        {
            IDE_TEST( aTemplate->tmplate.rows[aNode->node.table].
                      execute[aNode->node.column].estimateRange(
                          NULL,
                          & aTemplate->tmplate,
                          0,
                          & sSize )
                      != IDE_SUCCESS );
        }
        else
        {
            sSize = 0;
        }

        //--------------------------------------
        // and merge size ����� ���� count ���
        // (1). !=, not between �� count=2
        // (2). inlist �� count=1000
        // (3). 1,2�� ������ ������ �񱳿����ڴ� count=1
        //--------------------------------------
        if( ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
              == MTC_NODE_OPERATOR_NOT_EQUAL )
            || ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_NOT_RANGED ) )
        {
            // !=, not between
            *aRangeCount = 2;
        }
        else
        {
            /* BUG-32622 inlist operator */
            if( aNode->node.module == &mtfInlist )
            {
                *aRangeCount = MTC_INLIST_KEYRANGE_COUNT_MAX;
            }
            else
            {
                *aRangeCount = 1;
            }
        }

        (*aRangeSize) = (*aRangeSize) + sSize;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoKeyRange::makeRange( qcTemplate  * aTemplate,
                        qtcNode     * aNode,
                        UInt          aKeyCount,
                        mtcColumn   * aKeyColumn,
                        UInt        * aKeyColsFlag,
                        idBool        aIsKeyRange,
                        UInt          aCompareType,
                        smiRange    * aRangeArea,
                        UInt          aRangeAreaSize,
                        smiRange   ** aRange,
                        qtcNode    ** aFilter )
{
/***********************************************************************
 *
 * Description :  Key Range�� �����Ѵ�.
 *
 *     keyRange�� keyFilter�� ���� range ������ ���� �����ϸ�,
 *     ��������,
 *     (1) keyRange  : Key Column�� ���ӵ� Column�� ����
 *     (2) keyFilter : Key Column�� ���ӵ� Column�� ������ �ʿ䰡 ����.
 *
 *     ��) index on T1(i1,i2,i3)
 *         . i1=1 and i2>1   : keyRange(O), keyFilter(O)
 *         . (i1,i3) = (1,1) : keyRange(X), keyFilter(O)
 *
 * Implementation :
 *
 *
 *
 ***********************************************************************/

    UInt         i;
    UInt         sCnt;
    UInt         sOffset = 0;
    UInt         sSize;
    UInt         sRangeCount;

    UChar      * sRangeStartPtr = (UChar*)aRangeArea;
    smiRange   * sRange = NULL;
    smiRange   * sNextRange = NULL;
    smiRange   * sCurRange = NULL;
    smiRange   * sLastRange = NULL;
    smiRange   * sLastCurRange = NULL;
    smiRange   * sRangeList = NULL;
    smiRange   * sLastRangeList = NULL;
    smiRange  ** sRangeListArray;

    qtcNode    * sAndNode;
    idBool       sIsExistsValue = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makeRange::__FT__" );

    //--------------------------------------
    // fix BUG-13939
    // in subquery keyRange or subquery keyRange �� ���ؼ���
    // �̸� subquery�� �����Ѵ�.
    //--------------------------------------

    IDE_TEST( calculateSubqueryInRangeNode( aTemplate,
                                            aNode,
                                            & sIsExistsValue )
              != IDE_SUCCESS );

    //------------------------------------------
    // sIsExistsValues : In subquery KeyRange�� ���,
    //                   keyRange�� ������ value�� �ִ����� ����
    //------------------------------------------
    if( sIsExistsValue == ID_TRUE )
    {
        //--------------------------------------
        // keyRange ����
        //--------------------------------------

        // ���ڷ� �Ѿ�� aNode�� �ֻ����� OR ����̴�.
        for( sAndNode = (qtcNode *)(aNode->node.arguments);
             sAndNode != NULL;
             sAndNode = (qtcNode *)(sAndNode->node.next) )
        {

            //-------------------------------------
            // �� AND ��忡 ����, �ε��� �÷������� range ����
            //-------------------------------------

            sRange = NULL;

            for( sCnt = 0; sCnt < aKeyCount; sCnt++ )
            {
                // To Fix BUG-10260
                IDE_TEST( makeRange4AColumn( aTemplate,
                                             sAndNode,
                                             &aKeyColumn[sCnt],
                                             sCnt,
                                             &aKeyColsFlag[sCnt],
                                             &sOffset,
                                             sRangeStartPtr,
                                             aRangeAreaSize,
                                             aCompareType,
                                             & sCurRange )
                          != IDE_SUCCESS );

                //----------------------------------
                // ���� �ε��� �÷����� ������� range�� ������,
                // keyRange�� ���, ���� �ε����÷��� ���� range ���� �ߴ�,
                // keyFilter�� ���, ���� �ε����÷��� ���� range ���� �õ�.
                //----------------------------------

                if( sCurRange == NULL )
                {
                    if( aIsKeyRange == ID_TRUE )
                    {
                        break;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    // Nothing To Do
                }

                if( sRange == NULL )
                {
                    sRange = sCurRange;
                }
                else
                {
                    // fix BUG-12254
                    if( sCurRange->next != NULL )
                    {
                        // ��: index on T1(i1, i2)
                        //     i1=1 and i2 not between 0 and 1
                        //     i1=1 and i2 != 1 �� ��쿡 ���� ó��
                        //
                        //   i1 = 1 and i2 != 1 �� ���� ���,
                        //
                        //   (1) i1 = 1 �� ���� range ����
                        //   (2) i2 != 1 �� ���� range ����
                        //      ==>  -���Ѵ� < i2 < 1 OR 1 < i2 < +���Ѵ�
                        //   (3) (1)�� (2)�� composite range ����
                        //      ==> ( i1 = 1 and -���Ѵ� < i2 < 1 )
                        //       or ( i1 = 1 and 1 < i2 < +���Ѵ� )
                        //   (3)�� ó���ϱ� ���ؼ�
                        //   i1 = 1 �� ���� range�� �ϳ� �� �ʿ��ϹǷ�,
                        //   copyRange���� i1=1 range�� �����ؼ� ���.

                        sRange->prev = NULL;
                        sRange->next = NULL;

                        sLastRange = sRange;
                        sLastCurRange = sCurRange;

                        // Not Range ���� - 1 ��ŭ range ����
                        while( sLastCurRange->next != NULL )
                        {
                            IDE_TEST( copyRange ( sRange,
                                                  &sOffset,
                                                  sRangeStartPtr,
                                                  aRangeAreaSize,
                                                  &sNextRange )
                                      != IDE_SUCCESS );

                            sNextRange->prev = sLastRange;
                            sNextRange->next = NULL;

                            sLastRange->next = sNextRange;
                            sLastRange = sLastRange->next;

                            sLastCurRange = sLastCurRange->next;
                        }

                        sLastRange = sRange;
                        sLastCurRange = sCurRange;

                        // �� range�� composite range ����
                        while( ( sLastRange != NULL )
                               && ( sLastCurRange != NULL ) )
                        {
                            IDE_TEST( mtk::addCompositeRange( sLastRange,
                                                              sLastCurRange )
                                      != IDE_SUCCESS );

                            sLastRange = sLastRange->next;
                            sLastCurRange = sLastCurRange->next;
                        }
                    }
                    else
                    {
                        IDE_TEST( mtk::addCompositeRange( sRange, sCurRange )
                                  != IDE_SUCCESS );
                    }
                }
            }

            sCurRange = sRange;

            if( sCurRange == NULL )
            {
                if( aIsKeyRange == ID_TRUE )
                {
                    // If one of OR keyrange is NULL, then All range is NULL.
                    // Otherwise, a result will be wrong.
                    // example : TC/Server/qp2/SelectStmt/Bugs/PR-4447/select5
                    sRange = NULL;
                    sRangeCount = 0;
                    break;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                // Nothing To Do
            }

            // ������ range���� ���Ḯ��Ʈ�� ����
            if( sRangeList == NULL )
            {
                sRangeList = sCurRange;

                for( sLastRangeList = sRangeList;
                     sLastRangeList->next != NULL;
                     sLastRangeList = sLastRangeList->next ) ;
            }
            else
            {
                sLastRangeList->next = sCurRange;

                for( sLastRangeList = sLastRangeList->next;
                     sLastRangeList->next != NULL;
                     sLastRangeList = sLastRangeList->next ) ;
            }
        }

        if( sRangeList != NULL )
        {
            // fix BUG-9378
            // KEY RANGE MERGE ���� ����

            // OR merge�� ���� range count�� ���Ѵ�.
            for( sLastRangeList = sRangeList, sRangeCount = 0;
                 sLastRangeList != NULL;
                 sLastRangeList = sLastRangeList->next, sRangeCount++ ) ;

            // OR��� ������ AND��尡 2���̻��̰�,
            // merge�� range�� �ΰ� �̻��� ��쿡 or merge�� �����Ѵ�.
            if( ( aNode->node.arguments->next != NULL ) &&
                ( sRangeCount > 1 ) )
            {
                // smiRange pointer �迭�� ���� ���� Ȯ��
                sSize = ID_SIZEOF(smiRange *) * sRangeCount;

                IDE_TEST_RAISE( sOffset + sSize > aRangeAreaSize, ERR_OUT_OF_BOUND );

                sRangeListArray = (smiRange **)(sRangeStartPtr + sOffset );
                IDE_TEST_RAISE( sRangeListArray == NULL,
                                ERR_INVALID_MEMORY_AREA );

                sOffset += sSize;

                for( i = 0; i < sRangeCount; i++ )
                {
                    sRangeListArray[i] = sRangeList;
                    sRangeList = sRangeList->next;
                }

                // or merge�� ���� ���� Ȯ��
                sSize =
                    idlOS::align8((UInt) ID_SIZEOF(smiRange)) * sRangeCount;

                IDE_TEST_RAISE( sOffset + sSize > aRangeAreaSize, ERR_OUT_OF_BOUND );

                sRange = (smiRange *)(sRangeStartPtr + sOffset);
                IDE_TEST_RAISE( sRange == NULL, ERR_INVALID_MEMORY_AREA );

                sOffset += sSize;

                // or merge
                // BUG-28934
                // data type�� Ư���� �°� key range�� ������ ��, �̸� merge�� ����
                // key range�� Ư���� �°� merge�ϴ� ����� �ʿ��ϴ�.
                IDE_TEST( aKeyColumn->module->mergeOrRangeList( sRange,
                                                                sRangeListArray,
                                                                sRangeCount )
                          != IDE_SUCCESS );
            }
            else
            {
                sRange = sRangeList;
            }
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    if( sRange != NULL )
    {
        *aRange  = sRange;
        *aFilter = NULL;
    }
    else
    {
        *aRange  = NULL;
        *aFilter = aNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_MEMORY_AREA)
    {
        IDE_SET(ideSetErrorCode(qpERR_FATAL_QMO_INVALID_MEMORY_AREA));
    }
    IDE_EXCEPTION(ERR_OUT_OF_BOUND)
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoKeyRange::makeRange",
                                  "Invalid memory area" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoKeyRange::calculateSubqueryInRangeNode( qcTemplate   * aTemplate,
                                           qtcNode      * aNode,
                                           idBool       * aIsExistsValue )
{
/***********************************************************************
 *
 * Description : Range�� ������ ����߿� subquery node�� ���� �����Ѵ�.
 *
 * Implementation :
 *
 * subquery keyRange�� �����ϱ� ���� ��� ��ȯ�� �Ʒ��� ���� �̷������.
 * keyRange �����ÿ��� index column������ range�� �����ϰ� �Ǵµ�,
 * index ù��° �÷��� ���� range ������,
 * �̿� �����Ǵ� subquery target column�� a1��
 * subquery�� ����Ǳ����̾ �������� a1�� ���� ���� ���ϰ� �ȴ�.
 * �̷��� �������� �ذ��ϱ� ����,
 * range�� �����ϱ� ���� range node�� ��� ������ subquery�� ���� �����Ѵ�.
 *
 * ��) index on( i1, i2 ) �̰�,
 *
 * 1) where ( i2, i1 ) = ( select a2, a1 from ... )
 *
 *    [ = ]                          [=] -------------------> [=]
 *      |                             |                        |
 *    [LIST]--->[subquery]           [i2] ----> [ind]         [i1]--> [ind]
 *      |           |          ==>                |                     |
 *    [i2]-->[i1] [a2]-->[a1]                 [subquery]               [a1]
 *                                                |
 *                                               [a2]
 * 2) where ( i2, i1 ) in ( select a2, a1 from ... )
 *
 *     [IN]                          [=] -------------------> [=]
 *      |                             |                        |
 *    [LIST]--->[subquery]           [i2] ----> [Wrapper]     [i1]--> [ind]
 *      |           |          ==>                 |                    |
 *    [i2]-->[i1] [a2]-->[a1]                  [Subquery]               |
 *                                                 |                    |
 *                                                [a2]                 [a1]
 *
 ***********************************************************************/

    qtcNode     * sAndNode;
    qtcNode     * sCompareNode;
    qtcNode     * sValueNode;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::calculateSubqueryInRangeNode::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // fix BUG-13939
    // in subquery keyRange or subquery keyRange �� ���ؼ���
    // �̸� subquery�� �����Ѵ�.
    //--------------------------------------

    for( sAndNode = (qtcNode *)(aNode->node.arguments);
         sAndNode != NULL;
         sAndNode = (qtcNode*)(sAndNode->node.next) )
    {
        for( sCompareNode = (qtcNode *)(sAndNode->node.arguments);
             sCompareNode != NULL;
             sCompareNode = (qtcNode *)(sCompareNode->node.next) )
        {
            if( ( sCompareNode->lflag & QTC_NODE_SUBQUERY_RANGE_MASK )
                == QTC_NODE_SUBQUERY_RANGE_TRUE )
            {
                if( aNode->indexArgument == 0 )
                {
                    sValueNode =
                        (qtcNode*)(sCompareNode->node.arguments->next);
                }
                else
                {
                    sValueNode = (qtcNode*)(sCompareNode->node.arguments);
                }

                IDE_TEST( qtc::calculate( sValueNode, aTemplate )
                          != IDE_SUCCESS );

                if( aTemplate->tmplate.stack->value == NULL )
                {
                    *aIsExistsValue = ID_FALSE;
                    break;
                }
                else
                {
                    *aIsExistsValue = ID_TRUE;
                }
            }
            else
            {
                *aIsExistsValue = ID_TRUE;
            }
        }

        if( *aIsExistsValue == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoKeyRange::makeRange4AColumn( qcTemplate   * aTemplate,
                                qtcNode      * aNode,
                                mtcColumn    * aKeyColumn,
                                UInt           aColumnIdx,
                                UInt         * aKeyColsFlag,
                                UInt         * aOffset,
                                UChar        * aRangeStartPtr,
                                UInt           aRangeAreaSize,
                                UInt           aCompareType,
                                smiRange    ** aRange )
{
/***********************************************************************
 *
 * Description : �ϳ��� �ε��� �÷��� ���� range�� �����Ѵ�.
 *
 * Implementation :
 *
 *   �ε����÷��� �����ϴ� ��� �񱳿����ڿ� ���� range�� �����.
 *
 *   �ε��� �÷��� ���� range�� �������� ���, range ������ �����Ѵ�.
 *   ��) i1>1 and i1<3 �� ���, 1 < i1 < 3 ���� range ���� ����.
 *
 ***********************************************************************/

    idBool         sIsSameGroupType = ID_FALSE;
    UInt           sSize;
    UInt           sFlag;
    UInt           sMergeCount;
    smiRange     * sRange = NULL;
    smiRange     * sPrevRange = NULL;
    smiRange     * sCurRange = NULL;
    mtcExecute   * sExecute;
    mtkRangeInfo   sInfo;
    qtcNode      * sNode;
    qtcNode      * sColumnNode;
    qtcNode      * sValueNode;
    qtcNode      * sColumnConversion;
    idBool         sFixedValue = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makeRange4AColumn::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aOffset != NULL );
    IDE_DASSERT( aRangeStartPtr != NULL );

    //--------------------------------------
    // range ����
    //--------------------------------------

    if( ( aNode->node.lflag &
          ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
        == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        // AND �� ������

        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( makeRange4AColumn( aTemplate,
                                         sNode,
                                         aKeyColumn,
                                         aColumnIdx,
                                         aKeyColsFlag,
                                         aOffset,
                                         aRangeStartPtr,
                                         aRangeAreaSize,
                                         aCompareType,
                                         & sCurRange ) != IDE_SUCCESS );

            if( sPrevRange == NULL )
            {
                if( sCurRange == NULL )
                {
                    sRange = NULL;
                }
                else
                {
                    sRange = sCurRange;
                    sPrevRange = sRange;
                }
            }
            else
            {
                if( sCurRange == NULL )
                {
                    sRange = sPrevRange;
                }
                else
                {
                    sMergeCount = getAndRangeCount( sPrevRange, sCurRange );

                    sSize =
                        idlOS::align8((UInt)ID_SIZEOF(smiRange)) * sMergeCount;

                    IDE_TEST_RAISE( sSize + *aOffset > aRangeAreaSize, ERR_OUT_OF_BOUND );

                    sRange = (smiRange*)(aRangeStartPtr + *aOffset);
                    IDE_TEST_RAISE( sRange == NULL, ERR_INVALID_MEMORY_AREA );

                    *aOffset += sSize;

                    // BUG-28934
                    // data type�� Ư���� �°� key range�� ������ ��, �̸� merge�� ����
                    // key range�� Ư���� �°� merge�ϴ� ����� �ʿ��ϴ�.
                    IDE_TEST( aKeyColumn->module->mergeAndRange( sRange,
                                                                 sPrevRange,
                                                                 sCurRange )
                              != IDE_SUCCESS );

                    sPrevRange = sRange;
                }
            }

            sNode = (qtcNode *)(sNode->node.next);
        } // end of while()
    } // AND ���������� ó��
    else
    {
        // �� ������

        //------------------------------------------
        // �񱳿������� indexArgument�� columnID�� index columnID�� ����,
        // �÷��� conversion�� �߻����� �ʾҴ��� �˻�.
        //------------------------------------------

        if( aNode->indexArgument == 0 )
        {
            sColumnNode = (qtcNode *)(aNode->node.arguments);
            sValueNode = (qtcNode *)(aNode->node.arguments->next);
        }
        else
        {
            sColumnNode = (qtcNode *)(aNode->node.arguments->next);
            sValueNode = (qtcNode *)(aNode->node.arguments);
        }

        // To Fix PR-8700
        if ( sColumnNode->node.module == &qtc::passModule )
        {
            // To Fix PR-8700
            // Sort Join���� ����
            // Pass Node�� ��� Indexable ������ �Ǵܽÿ���
            // Conversion ���θ� Ȯ������ �ʾƾ� �ϴ� �ݸ�,
            // Key Range ������ ���ؼ��� Argument�� �̿��Ͽ��� �Ѵ�.
            sColumnNode = (qtcNode *)
                mtf::convertedNode( (mtcNode *) sColumnNode,
                                    & aTemplate->tmplate );
            // fix BUG-12005
            // Sort Join ���� ���� pass node�� ���
            // mtkRangeInfo.isSameGroupType = ID_FALSE�� �Ǿ�� ��.
        }
        else
        {
            // fix BUG-12005
            // sm�� ������ callback�� ������ �ֱ� ���� ������
            // mtkRangeInfo.isSameGroupType�� �� ������ ����
            sColumnConversion = (qtcNode *)
                mtf::convertedNode( (mtcNode *) sColumnNode,
                                    & aTemplate->tmplate );

            if( sColumnNode == sColumnConversion )
            {
                // Nothing To
                //
                // ������ ������ type�̶� ��ȯ�� �ʿ� ���� �ʴ�.
                // ������
                // sIsSameGroupType = ID_FALSE;
            }
            else
            {
                sIsSameGroupType = ID_TRUE;
            }
        }

        if ( aTemplate->tmplate.rows[sColumnNode->node.table].
             columns[sColumnNode->node.column].column.id
             == aKeyColumn->column.id )
        {
            // value node�� �����ؼ�, value�� ��´�.

            // (1) IS NULL, IS NOT NULL�� ���,
            //     sValueNode == NULL
            // (2) BETWEEN, NOT BETWEEN�� ���,
            //     sValueNode�� sValueNode->next�� ���� �о�� �ϸ�,
            // (3) �� ���� �񱳿����ڴ�  sValueNode�� ���� �о�� �Ѵ�.
            for( sNode = sValueNode;
                 sNode != NULL && sNode != sColumnNode;
                 sNode = (qtcNode *)(sNode->node.next) )
            {
                // Bug-11320 fix
                // Between, Not between �Ӹ� �ƴ϶�,
                // Like �Լ��� ���ڰ� 3���� �� �ִ�.
                // ���� sNode�� null�� �ƴҶ�����
                // ��� calculate ����� �Ѵ�.
                // calculate�� ���� null�̸� �ߴ��Ѵ�.

                // fix BUG-13939
                if( ( aNode->lflag & QTC_NODE_SUBQUERY_RANGE_MASK )
                    == QTC_NODE_SUBQUERY_RANGE_TRUE )
                {
                    // Nothing To Do

                    // �̹� subquery�� ���������Ƿ�,
                    // calculate ���� �ʾƵ� ��.
                    sFixedValue = ID_FALSE;
                }
                else
                {
                    // To Fix PR-8259
                    IDE_TEST( qtc::calculate( sNode, aTemplate )
                              != IDE_SUCCESS );

                    // BUG-43758
                    // sFixedValue�� ID_TRUE�̸� MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL�� ������
                    // compressed column�� value�� �߸� �����ɴϴ�.
                    if ( ( ( aTemplate->tmplate.stack->column->column.flag
                             &SMI_COLUMN_TYPE_MASK )
                           != SMI_COLUMN_TYPE_FIXED ) ||
                         ( ( aTemplate->tmplate.stack->column->column.flag
                             &SMI_COLUMN_COMPRESSION_MASK )
                           == SMI_COLUMN_COMPRESSION_TRUE ) )

                    {
                        sFixedValue = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }

            // set Flag
            if( ( *aKeyColsFlag & SMI_COLUMN_ORDER_MASK )
                == SMI_COLUMN_ORDER_ASCENDING )
            {
                sFlag = MTD_COMPARE_ASCENDING;
            }
            else
            {
                sFlag = MTD_COMPARE_DESCENDING;
            }

            sExecute = &(aTemplate->tmplate.
                         rows[aNode->node.table].
                         execute[aNode->node.column]);

            // get keyRange size
            IDE_TEST( sExecute->estimateRange( NULL,
                                               &aTemplate->tmplate,
                                               0,
                                               &sSize ) != IDE_SUCCESS );

            IDE_TEST_RAISE( sSize + *aOffset > aRangeAreaSize, ERR_OUT_OF_BOUND );

            // extract keyRange
            sRange = (smiRange *)(aRangeStartPtr + *aOffset );
            IDE_TEST_RAISE( sRange == NULL, ERR_INVALID_MEMORY_AREA);

            *aOffset += sSize;

            sInfo.column          = aKeyColumn;
            sInfo.argument        = aNode->indexArgument;
            sInfo.direction       = sFlag;
            sInfo.isSameGroupType = sIsSameGroupType;
            sInfo.compValueType   = aCompareType;
            sInfo.columnIdx       = aColumnIdx;
            sInfo.useOffset       = 0;

            // fixed fixed�� ��� fixed compare�� ȣ���Ͽ� ������ �����Ѵ�.
            // BUG-43758
            // aKeyColumn�� compressed column�� ��� MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL�� �����ϸ�
            // compressed column�� value�� �߸� �����ɴϴ�.
            if ( ( aCompareType == MTD_COMPARE_MTDVAL_MTDVAL ) &&
                 ( ( aKeyColumn->column.flag & SMI_COLUMN_TYPE_MASK )
                   == SMI_COLUMN_TYPE_FIXED ) &&
                 ( ( aKeyColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK )
                   == SMI_COLUMN_COMPRESSION_FALSE ) &&
                 ( sFixedValue == ID_TRUE ) )
            {
                sInfo.compValueType = MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL;
            }
            else
            {
                // Nothing to do.
            }

            IDE_TEST( sExecute->extractRange( (mtcNode *)aNode,
                                              &( aTemplate->tmplate ),
                                              & sInfo,
                                              sRange )
                      != IDE_SUCCESS );

            if ( aNode->node.module == &mtfInlist )
            {
                *aOffset -= sSize;
                *aOffset += sInfo.useOffset;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            sRange = NULL;
        }
    }

    *aRange = sRange;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_MEMORY_AREA)
    {
        IDE_SET(ideSetErrorCode(qpERR_FATAL_QMO_INVALID_MEMORY_AREA));
    }
    IDE_EXCEPTION( ERR_OUT_OF_BOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoKeyRange::makeRange4AColumn",
                                  "Invalid Memory Area" ));
    }
        
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


UInt
qmoKeyRange::getRangeCount( smiRange * aRange )
{
/***********************************************************************
 *
 * Description : range�� count�� ��´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    //--------------------------------------
    // count ���
    //--------------------------------------

    UInt       sRangeCount = 0;
    smiRange * sRange;

    sRange = aRange;

    while( sRange != NULL )
    {
        sRangeCount++;
        sRange = sRange->next;
    }

    return sRangeCount;
}


UInt
qmoKeyRange::getAndRangeCount( smiRange * aRange1,
                               smiRange * aRange2 )
{
/***********************************************************************
 *
 * Description : AND merge�� ���� range count�� ��´�.
 *
 * Implementation :
 *
 *     ( range1 + range2 - 1 )
 *
 ***********************************************************************/


    //--------------------------------------
    // count ���
    //--------------------------------------

    UInt   sRange1Count;
    UInt   sRange2Count;

    sRange1Count = getRangeCount( aRange1 );
    sRange2Count = getRangeCount( aRange2 );

    return ( sRange1Count + sRange2Count - 1 );
}

IDE_RC
qmoKeyRange::copyRange( smiRange  * aRangeOrg,
                        UInt      * aOffset,
                        UChar     * aRangeStartPtr,
                        UInt        aRangeAreaSize,
                        smiRange ** aRangeNew )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *     ( range1 + range2 )
 *
 ***********************************************************************/

    UInt                sSize = 0;
    smiRange          * sRange;
    mtkRangeCallBack  * sCallBackOrg = NULL;
    mtkRangeCallBack  * sCallBackNew = NULL;
    mtkRangeCallBack  * sCallBackMinimum = NULL;
    mtkRangeCallBack  * sCallBackMaximum = NULL;
    mtkRangeCallBack  * sLastCallBack = NULL;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::copyRange::__FT__" );

    //--------------------------------------
    // range ����
    //--------------------------------------

    IDE_TEST( mtk::estimateRangeDefault( NULL,
                                         NULL,
                                         0,
                                         &sSize )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( *aOffset + sSize > aRangeAreaSize, ERR_OUT_OF_BOUND );

    sRange = (smiRange *)(aRangeStartPtr + (*aOffset));
    IDE_TEST_RAISE( sRange == NULL,
                    ERR_INVALID_MEMORY_AREA );

    (*aOffset) += sSize;

    idlOS::memcpy( sRange, aRangeOrg, sSize );

    for( sCallBackOrg =
             (mtkRangeCallBack *)aRangeOrg->minimum.data,
             sCallBackMinimum = NULL;
         sCallBackOrg != NULL;
         sCallBackOrg = sCallBackOrg->next )
    {
        sSize = idlOS::align8((UInt) ID_SIZEOF(mtkRangeCallBack));

        IDE_TEST_RAISE( *aOffset + sSize > aRangeAreaSize, ERR_OUT_OF_BOUND );

        sCallBackNew =
            (mtkRangeCallBack *)(aRangeStartPtr + (*aOffset) );

        (*aOffset) += sSize;

        idlOS::memcpy( sCallBackNew, sCallBackOrg, sSize );

        if( sCallBackMinimum == NULL )
        {
            sCallBackMinimum = sCallBackNew;
            sLastCallBack = sCallBackMinimum;
        }
        else
        {
            sLastCallBack->next = sCallBackNew;
            sLastCallBack = sLastCallBack->next;
        }
    }

    for( sCallBackOrg =
             (mtkRangeCallBack *)aRangeOrg->maximum.data,
             sCallBackMaximum = NULL;
         sCallBackOrg != NULL;
         sCallBackOrg = sCallBackOrg->next )
    {
        sSize = idlOS::align8((UInt) ID_SIZEOF(mtkRangeCallBack));

        IDE_TEST_RAISE( *aOffset + sSize > aRangeAreaSize, ERR_OUT_OF_BOUND );

        sCallBackNew =
            (mtkRangeCallBack *)(aRangeStartPtr + (*aOffset) );

        (*aOffset) += sSize;

        idlOS::memcpy( sCallBackNew, sCallBackOrg, sSize );

        if( sCallBackMaximum == NULL )
        {
            sCallBackMaximum = sCallBackNew;
            sLastCallBack = sCallBackMaximum;
        }
        else
        {
            sLastCallBack->next = sCallBackNew;
            sLastCallBack = sLastCallBack->next;
        }
    }

    sRange->minimum.data = (void *)sCallBackMinimum;
    sRange->maximum.data = (void *)sCallBackMaximum;

    *aRangeNew = sRange;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_MEMORY_AREA)
    {
        IDE_SET(ideSetErrorCode(qpERR_FATAL_QMO_INVALID_MEMORY_AREA));
    }
    IDE_EXCEPTION(ERR_OUT_OF_BOUND)
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoKeyRange::copyRange",
                                  "Invalid memory area" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoKeyRange::makePartKeyRange(
    qcTemplate          * aTemplate,
    qtcNode             * aNode,
    UInt                  aPartKeyCount,
    mtcColumn           * aPartKeyColumns,
    UInt                * aPartKeyColsFlag,
    UInt                  aCompareType,
    smiRange            * aRangeArea,
    UInt                  aRangeAreaSize,
    smiRange           ** aRange )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                partition keyrange�� �����Ѵ�.
 *
 *  Implementation : ������ keyrange���� ��ƾ�� �����ϴ�.
 *                   filter�� �ʿ�����Ƿ�, filter�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sFilter;

    IDU_FIT_POINT_FATAL( "qmoKeyRange::makePartKeyRange::__FT__" );

    IDE_TEST( makeRange( aTemplate,
                         aNode,
                         aPartKeyCount,
                         aPartKeyColumns,
                         aPartKeyColsFlag,
                         ID_TRUE,
                         aCompareType,
                         aRangeArea,
                         aRangeAreaSize,
                         aRange,
                         & sFilter )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
