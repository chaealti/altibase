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
 * $Id: qmoOneMtrPlan.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 *
 * Description :
 *     Plan Generator
 *
 *     One-child Materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - SORT ���
 *         - HASH ���
 *         - GRAG ���
 *         - HSDS ���
 *         - LMST ���
 *         - VMTR ���
 *         - WNST ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcg.h>
#include <qcgPlan.h>
#include <qmo.h>
#include <qmoOneMtrPlan.h>
#include <qmsParseTree.h>
#include <qtcDef.h>
#include <qmv.h>

extern mtfModule mtfList;
extern mtfModule mtfLag;
extern mtfModule mtfLagIgnoreNulls;
extern mtfModule mtfLead;
extern mtfModule mtfLeadIgnoreNulls;
extern mtfModule mtfNtile;
extern mtfModule mtfGetBlobLocator;
extern mtfModule mtfGetClobLocator;
extern mtfModule mtfListagg;        /* BUG-46906 */
extern mtfModule mtfPercentileDisc; /* BUG-46922 */
extern mtfModule mtfPercentileCont; /* BUG-46922 */

// ORDER BY���� ���� sorting ��� column�� ������ �����Ǿ��ִ� ���
IDE_RC
qmoOneMtrPlan::initSORT( qcStatement     * aStatement ,
                         qmsQuerySet     * aQuerySet ,
                         UInt              aFlag,
                         qmsSortColumns  * aOrderBy ,
                         qmnPlan         * aParent,
                         qmnPlan        ** aPlan )
{
/***********************************************************************
 *
 * Description : SORT ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSORT�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - �˻� ��� �� ���� ��� flag ����
 *         - SORT����� �÷� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - �÷��� ������ ������ ���� 4������ �Ѵ�. �̿����� �������̽���
 *       �������ֵ��� �Ѵ�. (private���� ó��)
 *         - ORDER BY�� ���Ǵ� ��� (baseTable + orderby����)
 *         - ORDER BY�� ���ܻ�Ȳ (������ GRAG , HSDS�ϰ��
 *                                �� memory base�� �����Ѵ�.)
 *         - SORT-BASED GROUPING (baseTable + grouping ����)
 *         - SORT-BASED DISTINCTION (baseTable + target ����)
 *         - SORT-BASED JOIN (joinPredicate�� �÷�����)
 *         - SORT MERGE JOIN (joinPredicate�� �÷�����, sequential search)
 *         - STORE AND SEARCH (���� VIEW�� ����)
 *
 ***********************************************************************/

    qmncSORT          * sSORT;
    qmsSortColumns    * sSortColumn;
    qmcAttrDesc       * sItrAttr;
    qtcNode           * sNode;
    UInt                sDataNodeOffset;
    UInt                sColumnCount;
    UInt                sFlag = 0;
    SInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initSORT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSORT�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF(qmncSORT) ,
                                               (void **)& sSORT )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSORT ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SORT ,
                        qmnSORT ,
                        qmndSORT,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sSORT->flag  = QMN_PLAN_FLAG_CLEAR;

    sSORT->flag &= ~QMNC_SORT_SEARCH_MASK;
    sSORT->flag |= QMNC_SORT_SEARCH_SEQUENTIAL;

    sSORT->range = NULL;

    *aPlan = (qmnPlan *)sSORT;

    // Sorting key���� ǥ���Ѵ�.
    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    // Order by���� attribute���� �߰��Ѵ�.
    for ( sSortColumn = aOrderBy, sColumnCount = 0;
          sSortColumn != NULL;
          sSortColumn = sSortColumn->next, sColumnCount++ )
    {
        // Sorting ������ ǥ���Ѵ�.
        if ( sSortColumn->isDESC == ID_FALSE )
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_ASC;
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_DESC;
        }

        /* PROJ-2435 order by nulls first/last */
        if ( sSortColumn->nullsOption != QMS_NULLS_NONE )
        {
            if ( sSortColumn->nullsOption == QMS_NULLS_FIRST )
            {
                sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                sFlag |= QMC_ATTR_SORT_NULLS_ORDER_FIRST;
            }
            else
            {
                sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                sFlag |= QMC_ATTR_SORT_NULLS_ORDER_LAST;
            }
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_NULLS_ORDER_NONE;
        }

        /* BUG-36826 A rollup or cube occured wrong result using order by count_i3 */
        if ( ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK ) == QMO_MAKESORT_VALUE_TEMP_TRUE ) ||
             ( ( aFlag & QMO_MAKESORT_GROUP_EXT_VALUE_MASK ) == QMO_MAKESORT_GROUP_EXT_VALUE_TRUE ) )
        {
            for ( i = 1, sItrAttr = aParent->resultDesc;
                  i < sSortColumn->targetPosition;
                  i++ )
            {
                sItrAttr = sItrAttr->next;
            }

            if ( sItrAttr != NULL )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                &sSORT->plan.resultDesc,
                                                sItrAttr->expr,
                                                sFlag,
                                                QMC_APPEND_EXPRESSION_TRUE,
                                                ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            &sSORT->plan.resultDesc,
                                            sSortColumn->sortColumn,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
    }

    for ( sItrAttr = aParent->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        /* BUG-37146 together with rollup order by the results are wrong.
         * Value Temp�� ������ SortTemp�� ���� ��� ��� SortTemp�� value temp��
         * �����ؾ��Ѵ�. ���� ���� Plan�� exression�� ��� sort�� �߰��ϰ� ����
         * pushResultDesc �Լ����� passNode�� ���鵵�� �����Ѵ�.
         */
        if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK ) ==
               QMO_MAKESORT_VALUE_TEMP_TRUE )
        {
            if ( ( sItrAttr->expr->lflag & QTC_NODE_AGGREGATE_MASK ) ==
                 QTC_NODE_AGGREGATE_EXIST )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                &sSORT->plan.resultDesc,
                                                sItrAttr->expr,
                                                QMC_ATTR_SEALED_TRUE,
                                                QMC_APPEND_EXPRESSION_TRUE,
                                                ID_FALSE )
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
        /* BUG-35265 wrong result desc connect_by_root, sys_connect_by_path */
        if ( ( sItrAttr->expr->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
               == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            /* �����ؼ� ���� ������ ���� Parent �� �ٲ� �߸��� Tuple�� ����Ű�Եȴ� */
            IDU_FIT_POINT( "qmoOneMtrPlan::initSORT::alloc::Node",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                       (void**)&sNode )
                      != IDE_SUCCESS );
            idlOS::memcpy( sNode, sItrAttr->expr, ID_SIZEOF( qtcNode ) );

            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            &sSORT->plan.resultDesc,
                                            sNode,
                                            QMC_ATTR_SEALED_TRUE,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do*/
        }
    }

    // ORDER BY �� �� �����Ǵ� attribute���� �߰��Ѵ�.
    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                   aParent->resultDesc,
                                   & sSORT->plan.resultDesc )
              != IDE_SUCCESS );

    // BUG-36997
    // select distinct i1, i2+1 from t1 order by i1;
    // distinct �� ó���Ҷ� i2+1 �� �����ؼ� ������ ���´�.
    // ���� PASS ��带 �������־�� �Ѵ�.
    IDE_TEST( qmc::makeReferenceResult( aStatement,
                                        ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                        aParent->resultDesc,
                                        sSORT->plan.resultDesc )
              != IDE_SUCCESS );

    // ORDER BY������ �����Ǿ��ٴ� flag�� �� �̻� �������� �ʵ��� �����Ѵ�.
    for( sItrAttr = sSORT->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sItrAttr->flag &= ~QMC_ATTR_ORDER_BY_MASK;
        sItrAttr->flag |= QMC_ATTR_ORDER_BY_FALSE;
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sSORT->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// �ܼ��� �Ͻ����� ����� �����̳�, grouping/distinction�� ���� ��� ��ü�� sorting�� �ʿ��� ���
IDE_RC
qmoOneMtrPlan::initSORT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
    qmncSORT          * sSORT;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initSORT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSORT�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncSORT ),
                                               (void **)& sSORT )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSORT ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SORT ,
                        qmnSORT ,
                        qmndSORT,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sSORT->flag  = QMN_PLAN_FLAG_CLEAR;

    sSORT->flag &= ~QMNC_SORT_SEARCH_MASK;
    sSORT->flag |= QMNC_SORT_SEARCH_SEQUENTIAL;

    sSORT->range = NULL;

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   &sSORT->plan.resultDesc )
              != IDE_SUCCESS );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sSORT->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sSORT;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// Sort/merge join�� ���� predicate�� ���Ե� attribute�� sorting�� �ʿ��� ���
IDE_RC
qmoOneMtrPlan::initSORT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aRange,
                         idBool         aIsLeftSort,
                         idBool         aIsRangeSearch,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
    qmncSORT          * sSORT;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initSORT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSORT�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncSORT ),
                                               (void **)& sSORT )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSORT ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SORT ,
                        qmnSORT ,
                        qmndSORT,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sSORT->flag  = QMN_PLAN_FLAG_CLEAR;

    if( aIsRangeSearch == ID_TRUE )
    {
        sSORT->flag &= ~QMNC_SORT_SEARCH_MASK;
        sSORT->flag |= QMNC_SORT_SEARCH_RANGE;
        sSORT->range = aRange;
    }
    else
    {
        sSORT->flag &= ~QMNC_SORT_SEARCH_MASK;
        sSORT->flag |= QMNC_SORT_SEARCH_SEQUENTIAL;
        sSORT->range = NULL;
    }

    if( aRange != NULL )
    {
        IDE_TEST( appendJoinPredicate( aStatement,
                                       aQuerySet,
                                       aRange,
                                       aIsLeftSort,
                                       ID_FALSE, // allowDuplicateAttr?
                                       &sSORT->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sSORT->plan.resultDesc )
              != IDE_SUCCESS );

    // PROJ-2462 Result Cache
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sSORT->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sSORT;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeSORT( qcStatement       * aStatement ,
                         qmsQuerySet       * aQuerySet ,
                         UInt                aFlag,
                         qmgPreservedOrder * aChildPreservedOrder ,
                         qmnPlan           * aChildPlan ,
                         SDouble             aStoreRowCount,
                         qmnPlan           * aPlan )
{
    qmncSORT          * sSORT = (qmncSORT *)aPlan;
    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];
    qtcNode           * sPredicate[2];

    UShort              sTupleID;
    UShort              sColumnCount  = 0;

    qmcMtrNode        * sNewMtrNode   = NULL;
    qmcMtrNode        * sFirstMtrNode = NULL;
    qmcMtrNode        * sLastMtrNode  = NULL;

    mtcTemplate       * sMtcTemplate;
    qmcAttrDesc       * sItrAttr;
    UShort              sKeyCount     = 0;

    UShort       sOrgTable  = ID_USHORT_MAX;
    UShort       sOrgColumn = ID_USHORT_MAX;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeSORT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPlan != NULL );
    IDE_DASSERT( aPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndSORT));

    sSORT->mtrNodeOffset = sDataNodeOffset;
    sSORT->storeRowCount = aStoreRowCount;

    sSORT->plan.left = aChildPlan;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sSORT->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sSORT->myNode = NULL;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // GROUP BY �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKESORT_TEMP_TABLE_MASK) ==
        QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    // To Fix BUG-7730
    // Preserved Order�� ����Ǵ� ���,
    // Sorting ���� �������� �ö���� ������� �����ϱ� ����
    if ( ( aFlag & QMO_MAKESORT_PRESERVED_ORDER_MASK ) ==
         QMO_MAKESORT_PRESERVED_TRUE )
    {
        sSORT->flag &= ~QMNC_SORT_STORE_MASK;
        sSORT->flag |= QMNC_SORT_STORE_PRESERVE;
    }
    else
    {
        sSORT->flag &= ~QMNC_SORT_STORE_MASK;
        sSORT->flag |= QMNC_SORT_STORE_ONLY;
    }

    if( sSORT->range != NULL )
    {
        sSORT->flag &= ~QMNC_SORT_SEARCH_MASK;
        sSORT->flag |= QMNC_SORT_SEARCH_RANGE;
    }

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sSORT->plan.resultDesc,
                                     &aChildPlan->depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    // BUGBUG
    // PROJ-2179 Full store nested join�� ������ ���� ������ ��� result descriptor�� ������� �� �ִ�.
    // SELECT r.a FROM r FULL OUTER JOIN s ON r.a > 1 AND s.a = 3;
    // ���� �� row���� 1�� ä�����ִ� temp table�� �����Ѵ�.
    // ��Ģ������ �� ��� full stored nested join�� �����ؼ��� �ȵȴ�.
    if( aPlan->resultDesc == NULL )
    {
        IDE_TEST( qmg::makeDummyMtrNode( aStatement,
                                         aQuerySet,
                                         sTupleID,
                                         &sColumnCount,
                                         &sNewMtrNode )
                  != IDE_SUCCESS );

        sFirstMtrNode = sNewMtrNode;
        sLastMtrNode  = sNewMtrNode;
    }
    else
    {
        // Nothing to do.
    }

    // Sorting key�� �ƴ� ���
    IDE_TEST( makeNonKeyAttrsMtrNodes( aStatement,
                                       aQuerySet,
                                       sSORT->plan.resultDesc,
                                       sTupleID,
                                       &sFirstMtrNode,
                                       &sLastMtrNode,
                                       &sColumnCount )
              != IDE_SUCCESS );

    sSORT->baseTableCount = sColumnCount;

    for( sItrAttr = sSORT->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        // Sorting key �� ���
        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            sOrgTable  = sItrAttr->expr->node.table;
            sOrgColumn = sItrAttr->expr->node.column;

            // Sorting�� �ʿ����� ǥ��
            sSORT->flag &= ~QMNC_SORT_STORE_MASK;
            sSORT->flag |= QMNC_SORT_STORE_SORTING;

            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ( ( sItrAttr->flag & QMC_ATTR_CONVERSION_MASK )
                                                == QMC_ATTR_CONVERSION_TRUE ? ID_TRUE : ID_FALSE),
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            // BUG-43088 setColumnLocate ȣ�� ��ġ�������� ����
            // TC/Server/qp4/Bugs/PR-13286/PR-13286.sql diff �� �����ϱ� ����
            // ex) SELECT /*+ USE_ONE_PASS_SORT( D2, D1 ) */ d1.i1 FROM D1, D2
            //      WHERE D1.I1 > D2.I1 AND D1.I1 < D2.I1 + 10;
            //            ^^^^^ column locate ������ ����
            //   * sSORT->range
            //     AND
            //     |
            //     OR
            //     |
            //     < ------------------- >
            //     | (1)                 | (2)
            //     d1.i1 - +             d1.i1 - d2.i1
            //     (2,0)   |             (2,0)->(4,0) ���� ���� ����
            //             d2.i1 - 10
            // sSORT->range �� ��ȸ�Ͽ� sItrAttr->expr �� ������ ������ ��带 ã��
            // sNewMtrNode.dstNode �� ����� �� sItrAttr->expr(table,colomn)���� set
            if ( sSORT->range != NULL )
            {
                IDE_TEST( qmg::resetDupNodeToMtrNode( aStatement,
                                                      sOrgTable,
                                                      sOrgColumn,
                                                      sItrAttr->expr,
                                                      sSORT->range )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            sNewMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;

            // BUG-37993
            // qmgJoin::makePreservedOrder
            // Merge Join�� ��� ASCENDING���� �����ϴ�.
            if ( (aFlag & QMO_MAKESORT_METHOD_MASK) == QMO_MAKESORT_SORT_MERGE_JOIN )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
            else
            {
                if ( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK )
                        == QMC_ATTR_SORT_ORDER_ASC )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
                }

                /* PROJ-2435 order by nulls first/last */
                if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                     != QMC_ATTR_SORT_NULLS_ORDER_NONE )
                {
                    if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                         == QMC_ATTR_SORT_NULLS_ORDER_FIRST )
                    {
                        sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                        sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_FIRST;
                    }
                    else
                    {
                        sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                        sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_LAST;
                    }
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_NONE;
                }

                // Preserved order ����
                if( aChildPreservedOrder != NULL )
                {
                    IDE_TEST( qmg::setDirection4SortColumn( aChildPreservedOrder,
                                                            sKeyCount,
                                                            &( sNewMtrNode->flag ) )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            sKeyCount++;
        }
        else
        {
            // Nothing to do.
        }
    }

    sSORT->myNode = sFirstMtrNode;

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKESORT_TEMP_TABLE_MASK) ==
        QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sSORT->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSORT->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sSORT->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sSORT->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sSORT->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    for( sNewMtrNode = sSORT->myNode , sColumnCount = 0 ;
         sNewMtrNode != NULL;
         sNewMtrNode = sNewMtrNode->next , sColumnCount++ ) ;

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sPredicate[0] = sSORT->range;
    sMtrNode[0]  = sSORT->myNode;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sSORT->plan ,
                                            QMO_SORT_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            1 ,
                                            sPredicate ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sSORT->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sSORT->depTupleRowID = (UShort)sSORT->plan.dependency;

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sSORT->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sSORT->myNode )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sSORT->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sSORT->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sSORT->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);
    sSORT->plan.flag |= QMN_PLAN_MTR_EXIST_TRUE;

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sSORT->planID,
                               sSORT->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sSORT->myNode,
                               &sSORT->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeSORT",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initHASH( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aHashFilter,
                         idBool         aIsLeftHash,
                         idBool         aDistinction,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : HASH ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncHASH�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - �˻� ��� �� CHECK_NOTNULL , DISTINCT�� ���� flag ����
 *         - HASH����� �÷� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - �÷��� ������ ������ ���� 2������ �Ѵ�. �̿����� �������̽���
 *       �������ֵ��� �Ѵ�. (private���� ó��)
 *         - HASH-BASED JOIN (joinPredicate�� �÷�����)
 *         - STORE AND SEARCH (���� VIEW�� ����)
 *
 *     - filter�� filterConst ������ passNode������ �����Ѵ�.
 *
 ***********************************************************************/

    qmncHASH          * sHASH;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initHASH::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncHASH�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncHASH ),
                                               (void **)& sHASH )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sHASH ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_HASH ,
                        qmnHASH ,
                        qmndHASH,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sHASH->flag  = QMN_PLAN_FLAG_CLEAR;
    if( aDistinction == ID_TRUE )
    {
        sHASH->flag  &= ~QMNC_HASH_STORE_MASK;
        sHASH->flag  |= QMNC_HASH_STORE_DISTINCT;
    }
    else
    {
        sHASH->flag  &= ~QMNC_HASH_STORE_MASK;
        sHASH->flag  |= QMNC_HASH_STORE_HASHING;
    }

    if( aIsLeftHash == ID_FALSE )
    {
        sHASH->flag &= ~QMNC_HASH_SEARCH_MASK;
        sHASH->flag |= QMNC_HASH_SEARCH_RANGE;
    }
    else
    {
        sHASH->flag &= ~QMNC_HASH_SEARCH_MASK;
        sHASH->flag |= QMNC_HASH_SEARCH_SEQUENTIAL;
    }

    if( aHashFilter != NULL )
    {
        IDE_TEST( appendJoinPredicate( aStatement,
                                       aQuerySet,
                                       aHashFilter,
                                       aIsLeftHash,
                                       ID_TRUE,  // allowDuplicateAttr?
                                       &sHASH->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sHASH->plan.resultDesc )
              != IDE_SUCCESS );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sHASH->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sHASH;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// Store and search ��
IDE_RC
qmoOneMtrPlan::initHASH( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
    qmncHASH          * sHASH;
    UInt                sDataNodeOffset;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initHASH::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncHASH�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncHASH ) ,
                                               (void **)& sHASH )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sHASH ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_HASH ,
                        qmnHASH ,
                        qmndHASH,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sHASH->flag  = QMN_PLAN_FLAG_CLEAR;
    sHASH->flag  &= ~QMNC_HASH_STORE_MASK;
    sHASH->flag  |= QMNC_HASH_STORE_DISTINCT;

    sHASH->flag  &= ~QMNC_HASH_SEARCH_MASK;
    sHASH->flag  |= QMNC_HASH_SEARCH_STORE_SEARCH;

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   &sHASH->plan.resultDesc )
              != IDE_SUCCESS );

    for( sItrAttr = sHASH->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sItrAttr->flag &= ~QMC_ATTR_KEY_MASK;
        sItrAttr->flag |= QMC_ATTR_KEY_TRUE;
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sHASH->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sHASH;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeHASH( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aFlag ,
                         UInt           aTempTableCount ,
                         UInt           aBucketCount ,
                         qtcNode      * aFilter ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan,
                         idBool         aAllAttrToKey )
{
    qmncHASH          * sHASH = (qmncHASH *)aPlan;

    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];
    qtcNode           * sPredicate[2];
    qtcNode           * sNode;
    qtcNode           * sOptimizedNode;

    UShort              sTupleID;
    UShort              sColumnCount  = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sLastMtrNode  = NULL;
    qmcMtrNode        * sFirstMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeHASH::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPlan != NULL );
    IDE_DASSERT( aPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndHASH));

    sHASH->mtrNodeOffset = sDataNodeOffset;

    sHASH->plan.left = aChildPlan;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sHASH->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sHASH->bucketCnt     = aBucketCount;
    sHASH->tempTableCnt  = aTempTableCount;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKEHASH_TEMP_TABLE_MASK) ==
        QMO_MAKEHASH_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sHASH->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    /* PROJ-2385
     * Inverse Index NL������ Left Distinct HASH��
     * Filtering �� ��� Attribute�� Distinct Key�� ����ؾ� �Ѵ�.
     * �׷��� ���� ��� Distinct ����� �پ��� JOIN ���� ����� �پ���. */
    if ( aAllAttrToKey == ID_TRUE )
    {
        for ( sItrAttr = sHASH->plan.resultDesc;
              sItrAttr != NULL;
              sItrAttr = sItrAttr->next )
        {
            sItrAttr->flag &= ~QMC_ATTR_KEY_MASK;
            sItrAttr->flag |= QMC_ATTR_KEY_TRUE;
        }
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // myNode�� ����
    //----------------------------------

    // Hash key�� �ƴ� ���
    IDE_TEST( makeNonKeyAttrsMtrNodes( aStatement,
                                       aQuerySet,
                                       sHASH->plan.resultDesc,
                                       sTupleID,
                                       & sFirstMtrNode,
                                       & sLastMtrNode,
                                       & sColumnCount )
              != IDE_SUCCESS );

    sHASH->baseTableCount = sColumnCount;

    for( sItrAttr = sHASH->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        // Hash key�� ���
        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ( ( sItrAttr->flag & QMC_ATTR_CONVERSION_MASK )
                                                == QMC_ATTR_CONVERSION_TRUE ? ID_TRUE : ID_FALSE),
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_HASH_NEED_MASK;
            sNewMtrNode->flag |= QMC_MTR_HASH_NEED_TRUE;

            if( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
        }
    }

    sHASH->myNode = sFirstMtrNode;

    switch( aFlag & QMO_MAKEHASH_METHOD_MASK )
    {
        //----------------------------------
        // Hash-based JOIN���� ���Ǵ� ���
        //----------------------------------
        case QMO_MAKEHASH_HASH_BASED_JOIN:
        {
            switch( aFlag & QMO_MAKEHASH_POSITION_MASK )
            {
                case QMO_MAKEHASH_POSITION_LEFT:
                {
                    /* PROJ-2385
                     * LEFT HASH�� ���, Hashing Method�� ������ �ʿ� ����.
                     * - Inverse Index NL => STORE_DISTINCT
                     * - Two-Pass HASH    => STORE_HASHING
                     */ 

                    sHASH->filter      = NULL;
                    sHASH->filterConst = NULL;

                    sHASH->flag &= ~QMNC_HASH_SEARCH_MASK;
                    sHASH->flag |= QMNC_HASH_SEARCH_SEQUENTIAL;
                    break;
                }
                case QMO_MAKEHASH_POSITION_RIGHT:
                {
                    // RIGHT HASH�� ���, Hashing Method�� STORE_HASHING���� ����
                    sHASH->flag &= ~QMNC_HASH_STORE_MASK;
                    sHASH->flag |= QMNC_HASH_STORE_HASHING;

                    //�˻� ����� ����
                    sHASH->flag  &= ~QMNC_HASH_SEARCH_MASK;
                    sHASH->flag  |= QMNC_HASH_SEARCH_RANGE;

                    IDE_TEST( makeFilterConstFromPred( aStatement ,
                                                       aQuerySet,
                                                       sTupleID,
                                                       aFilter ,
                                                       &( sHASH->filterConst ) )
                              != IDE_SUCCESS );

                    // BUG-17921
                    IDE_TEST( qmoNormalForm::optimizeForm( aFilter,
                                                           & sOptimizedNode )
                              != IDE_SUCCESS );

                    sHASH->filter = sOptimizedNode;

                    break;
                }
                default:
                {
                    IDE_DASSERT(0);
                    break;
                }
            }
            break;
        }
        case QMO_MAKEHASH_STORE_AND_SEARCH:
        {
            //----------------------------------
            // Store and Search�� ���Ǵ� ���
            //----------------------------------

            //not null �˻� ����
            if( (aFlag & QMO_MAKEHASH_NOTNULLCHECK_MASK ) ==
                QMO_MAKEHASH_NOTNULLCHECK_TRUE )
            {
                sHASH->flag  &= ~QMNC_HASH_NULL_CHECK_MASK;
                sHASH->flag  |= QMNC_HASH_NULL_CHECK_TRUE;
            }
            else
            {
                sHASH->flag  &= ~QMNC_HASH_NULL_CHECK_MASK;
                sHASH->flag  |= QMNC_HASH_NULL_CHECK_FALSE;
            }

            //Filter�� �籸��
            IDE_TEST( makeFilterINSubquery( aStatement ,
                                            aQuerySet ,
                                            sTupleID,
                                            aFilter ,
                                            & sNode )
                      != IDE_SUCCESS );

            IDE_TEST( makeFilterConstFromPred( aStatement ,
                                               aQuerySet,
                                               sTupleID,
                                               sNode ,
                                               &(sHASH->filterConst) )
                      != IDE_SUCCESS );

            // BUG-17921
            IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                                   & sOptimizedNode )
                      != IDE_SUCCESS );

            sHASH->filter = sOptimizedNode;

            break;
        }
        default:
        {
            IDE_DASSERT(0);
            break;
        }
    }

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement ,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKEHASH_TEMP_TABLE_MASK) ==
        QMO_MAKEHASH_MEMORY_TEMP_TABLE )
    {
        sHASH->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sHASH->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sHASH->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sHASH->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sHASH->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sHASH->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sHASH->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    for (sNewMtrNode = sHASH->myNode , sColumnCount = 0 ;
         sNewMtrNode != NULL;
         sNewMtrNode = sNewMtrNode->next , sColumnCount++ ) ;

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sPredicate[0] = sHASH->filter;
    sMtrNode[0]  = sHASH->myNode;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       sTupleID,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sHASH->plan ,
                                            QMO_HASH_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            1 ,
                                            sPredicate ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sHASH->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sHASH->depTupleRowID = (UShort)sHASH->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sHASH->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sHASH->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sHASH->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);
    sHASH->plan.flag |= QMN_PLAN_MTR_EXIST_TRUE;

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sHASH->planID,
                               sHASH->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sHASH->myNode,
                               &sHASH->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeHASH",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initGRAG( qcStatement       * aStatement ,
                         qmsQuerySet       * aQuerySet ,
                         qmsAggNode        * aAggrNode ,
                         qmsConcatElement  * aGroupColumn,
                         qmnPlan           * aParent,
                         qmnPlan          ** aPlan )
{
/***********************************************************************
 *
 * Description : GRAG ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncGRAG�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - GRAG����� �÷� ���� (aggregation + grouping����)
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - aggregation�� ����������, stack�� �޾��ֱ⶧���� passNode��
 *       �������� �ʴ´�. �׷��� grouping�÷��� passNode�� �����ϰ�
 *       �������� order by , having�� ���ϼ� �����Ƿ� srcNode�� ����
 *       �Ͽ� �÷��� �����ϰ�, passNode�� ��ü �Ѵ�.
 *
 ***********************************************************************/

    qmncGRAG          * sGRAG;
    UInt                sDataNodeOffset;
    UInt                sFlag = 0;
    qmsConcatElement  * sGroupColumn;
    qmsAggNode        * sAggrNode;
    qtcNode           * sNode;
    qmcAttrDesc       * sResultDesc;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initGRAG::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncGRAG�� �Ҵ� �� �ʱ�ȭ
    IDU_FIT_POINT( "qmoOneMtrPlan::initGRAG::alloc::GRAG",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncGRAG ),
                                               (void **)& sGRAG )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sGRAG ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_GRAG ,
                        qmnGRAG ,
                        qmndGRAG,
                        sDataNodeOffset );

    sGRAG->plan.readyIt         = qmnGRAG::readyIt;

    *aPlan = (qmnPlan *)sGRAG;

    // BUG-41565
    // AGGR �Լ��� �������� �޷������� ����� Ʋ�����ϴ�.
    // ���� �÷����� �������� ������ qtcNode �� ���� �����ϱ� ������
    // ���� �÷��� result desc �� ���� �߰��� �־�� ���� qtcNode �� �����Ҽ� �ִ�.
    if ( aParent->type != QMN_GRAG )
    {
        // ���� �÷��� GRAG �̸� �߰����� �ʴ´�. �߸��� AGGR �� �߰��ϰԵ�
        // select max(count(i1)), sum(i1) from t1 group by i1; �϶�
        // GRAG1 -> max(count(i1)), sum(i1)
        // GRAG2 -> count(i1) �� ó���ȴ�.
        for( sResultDesc = aParent->resultDesc;
             sResultDesc != NULL;
             sResultDesc = sResultDesc->next )
        {
            // BUG-43288 �ܺ����� AGGR �Լ��� �߰����� �ʴ´�.
            if ( ( qtc::haveDependencies( &aQuerySet->outerDepInfo ) == ID_TRUE ) &&
                 ( qtc::dependencyContains( &aQuerySet->outerDepInfo,
                                            &sResultDesc->expr->depInfo ) == ID_TRUE ) )
            {
                continue;
            }
            else
            {
                // Nothing To Do
            }

            // nested aggr x, over x
            if ( (sResultDesc->expr->overClause == NULL) &&
                 (QTC_IS_AGGREGATE( sResultDesc->expr ) == ID_TRUE ) &&
                 (QTC_HAVE_AGGREGATE2( sResultDesc->expr ) == ID_FALSE ) )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                &sGRAG->plan.resultDesc,
                                                sResultDesc->expr,
                                                0,
                                                QMC_APPEND_EXPRESSION_TRUE,
                                                ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        // Nothing To Do
    }

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    for( sGroupColumn = aGroupColumn;
         sGroupColumn != NULL;
         sGroupColumn = sGroupColumn->next )
    {
        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sGRAG->plan.resultDesc,
                                        sGroupColumn->arithmeticOrList,
                                        sFlag,
                                        QMC_APPEND_EXPRESSION_TRUE | QMC_APPEND_ALLOW_CONST_TRUE,
                                        ID_FALSE )
                  != IDE_SUCCESS );
    }

    for( sAggrNode = aAggrNode;
         sAggrNode != NULL;
         sAggrNode = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;

        /* BUG-46906 BUG-46922 */
        IDE_TEST_RAISE( ( sNode->node.module == & mtfListagg ) ||
                        ( sNode->node.module == & mtfPercentileDisc ) ||
                        ( sNode->node.module == & mtfPercentileCont ),
                        ERR_UNEXPECTED );

        while( sNode->node.module == &qtc::passModule )
        {
            // Aggregate function�� having/order by���� �����ϴ� ��� pass node�� �����ȴ�.
            sNode = (qtcNode *)sNode->node.arguments;
        }

        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sGRAG->plan.resultDesc,
                                        sNode,
                                        0,
                                        QMC_APPEND_EXPRESSION_TRUE,
                                        ID_FALSE )
                  != IDE_SUCCESS );
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sGRAG->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::initGRAG",
                                  "mathematics temp is not support a group aggregate plan" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeGRAG( qcStatement      * aStatement ,
                         qmsQuerySet      * aQuerySet ,
                         UInt               aFlag,
                         qmsConcatElement * aGroupColumn,
                         UInt               aBucketCount ,
                         qmnPlan          * aChildPlan ,
                         qmnPlan          * aPlan )
{
    qmncGRAG          * sGRAG = (qmncGRAG *)aPlan;

    UInt                sDataNodeOffset;

    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sFirstMtrNode = NULL;
    qmcMtrNode        * sLastMtrNode = NULL;

    UShort              sAggrNodeCount = 0;
    UShort              sMtrNodeCount;
    mtcTemplate       * sMtcTemplate;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeGRAG::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndGRAG));

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sGRAG->plan.left = aChildPlan;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sGRAG->flag      = QMN_PLAN_FLAG_CLEAR;
    sGRAG->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // GROUP BY �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKEGRAG_TEMP_TABLE_MASK) ==
        QMO_MAKEGRAG_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

        // BUG-23689  group by�� ���� aggregation �����, �����÷�ó�� ����
        // ���ǰ� ��ũ�������̺�� ó���Ǵ���,
        // group by�� ���� aggregation�� ���� ó���� ���������� �޸��������̺����ϰ�,
        // �������� �ٽ� ��ũ�������̺�� ó����.
        // �� ��� ��ũ�÷��� ���� ���� ���޵� �� �ֵ��� ó���ؾ� ��.
        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            if( ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
                  == QMN_PLAN_STORAGE_DISK ) &&
                ( aGroupColumn == NULL ) )
            {
                sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
                sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    // PROJ-2444 parallel aggregation
    // parallel aggregation �� 2�ܰ�� ����ȴ�.
    // �̸� �����ϴ� flag �� �����Ѵ�.
    if ( (aFlag & QMO_MAKEGRAG_PARALLEL_STEP_MASK) ==
                  QMO_MAKEGRAG_PARALLEL_STEP_AGGR )
    {
        sGRAG->flag &= ~QMNC_GRAG_PARALLEL_STEP_MASK;
        sGRAG->flag |= QMNC_GRAG_PARALLEL_STEP_AGGR;
        sGRAG->plan.flag &= ~QMN_PLAN_GARG_PARALLEL_MASK;
        sGRAG->plan.flag |= QMN_PLAN_GARG_PARALLEL_TRUE;
    }
    else if ( (aFlag & QMO_MAKEGRAG_PARALLEL_STEP_MASK) ==
                       QMO_MAKEGRAG_PARALLEL_STEP_MERGE )
    {
        sGRAG->flag &= ~QMNC_GRAG_PARALLEL_STEP_MASK;
        sGRAG->flag |= QMNC_GRAG_PARALLEL_STEP_MERGE;
        sGRAG->plan.flag &= ~QMN_PLAN_GARG_PARALLEL_MASK;
        sGRAG->plan.flag |= QMN_PLAN_GARG_PARALLEL_TRUE;
    }
    else
    {
        sGRAG->flag &= ~QMNC_GRAG_PARALLEL_STEP_MASK;
        sGRAG->plan.flag &= ~QMN_PLAN_GARG_PARALLEL_MASK;
        sGRAG->plan.flag |= QMN_PLAN_GARG_PARALLEL_FALSE;
    }

    //----------------------------------
    // myNode�� ����
    // - for aggregation
    //----------------------------------
    sGRAG->myNode = NULL;

    //----------------------------------
    // PROJ-1473
    // aggrNode�� ���� mtrNode ��������
    // ���ǿ� ���� �÷��� ���� ����
    //----------------------------------

    sGRAG->baseTableCount = 0;

    for( sItrAttr = aPlan->resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        // Grouping key�� �ƴ� ���(aggregation)
        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_FALSE )
        {
            sAggrNodeCount++;

            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // group by���� TEMP_VAR_TYPE�� ������� �ʴ´�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
        }
    }

    for( sItrAttr = aPlan->resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        // Grouping key�� ���
        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;

            sNewMtrNode->flag &= ~QMC_MTR_GROUPING_MASK;
            sNewMtrNode->flag |= QMC_MTR_GROUPING_TRUE;

            sNewMtrNode->flag &= ~QMC_MTR_MTR_PLAN_MASK;
            sNewMtrNode->flag |= QMC_MTR_MTR_PLAN_TRUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // group by���� TEMP_VAR_TYPE�� ������� �ʴ´�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
        }
    }

    sGRAG->myNode = sFirstMtrNode;

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement ,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKEGRAG_TEMP_TABLE_MASK) ==
        QMO_MAKEGRAG_MEMORY_TEMP_TABLE )
    {
        sGRAG->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sGRAG->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

        // BUG-23689  group by�� ���� aggregation �����, �����÷�ó�� ����
        // ���ǰ� ��ũ�������̺�� ó���Ǵ���,
        // group by�� ���� aggregation�� ���� ó���� ���������� �޸��������̺����ϰ�,
        // �������� �ٽ� ��ũ�������̺�� ó����.
        // �� ��� ��ũ�÷��� ���� ���� ���޵� �� �ֵ��� ó���ؾ� ��.
        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {        
            if( ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
                  == QMN_PLAN_STORAGE_DISK ) &&
                ( aGroupColumn == NULL ) )
            {            
                sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
                sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;                
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
    }
    else
    {
        sGRAG->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sGRAG->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sGRAG->myNode )
              != IDE_SUCCESS);

    // PROJ-2444 parallel aggregation
    // qmg::setColumnLocate �Լ����� ���ø��� columnLocate �� �����Ѵ�.
    // GRAG �÷��� ������ �����ϴ� ��������
    // ����� columnLocate�� �����ϸ� �߸��� �÷��� �����ȴ�.
    if ( (aFlag & QMO_MAKEGRAG_PARALLEL_STEP_MASK) !=
                  QMO_MAKEGRAG_PARALLEL_STEP_AGGR )
    {
        //----------------------------------
        // PROJ-1473 column locate ����.
        //----------------------------------

        IDE_TEST( qmg::setColumnLocate( aStatement,
                                        sGRAG->myNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sGRAG->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    // ���� Column�� data ���� ����
    sGRAG->mtrNodeOffset = sDataNodeOffset;

    for( sNewMtrNode = sGRAG->myNode , sMtrNodeCount = 0 ;
         sNewMtrNode != NULL ;
         sNewMtrNode = sNewMtrNode->next , sMtrNodeCount++ ) ;

    sDataNodeOffset += sMtrNodeCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    // Aggregation Column �� data ���� ����
    sGRAG->aggrNodeOffset = sDataNodeOffset;

    // Data ���� Size ����
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sAggrNodeCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sMtrNode[0]  = sGRAG->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sGRAG->plan ,
                                            QMO_GRAG_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sGRAG->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sGRAG->depTupleRowID = (UShort)sGRAG->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sGRAG->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sGRAG->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sGRAG->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);
    sGRAG->plan.flag |= QMN_PLAN_MTR_EXIST_TRUE;

    // PROJ-2444
    if ( (aFlag & QMO_MAKEGRAG_PARALLEL_STEP_MASK) ==
                  QMO_MAKEGRAG_PARALLEL_STEP_AGGR )
    {
        if ( aBucketCount == 1 )
        {
            sGRAG->bucketCnt = aBucketCount;
        }
        else
        {
            sGRAG->bucketCnt = IDL_MAX( (aBucketCount / sGRAG->plan.mParallelDegree), 1024 );
        }
    }
    else
    {
        sGRAG->bucketCnt     = aBucketCount;
    }

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sGRAG->planID,
                               sGRAG->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sGRAG->myNode,
                               &sGRAG->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeGRAG",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initHSDS( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         idBool         aExplicitDistinct,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : HSDS ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncHSDS�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - HSDS����� ��뱸��
 *         - HSDS����� �÷� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - HSDS����� �÷��� baseTable�� �������� �ʴ´�.
 *     - �÷��� ������ ������ ���� 3������ �Ѵ�. �̿����� �������̽���
 *       �������ֵ��� �Ѵ�. (private���� ó��)
 *         - HASH-BASED DISTINCTION (target���� �̿�)
 *         - SET-UNION���� ���̴� ��� (������ VIEW��� �̿�)
 *         - IN�������� SUBQUERY KEYRANGE (������ VIEW��� �̿�)
 *     - HSDS ���蹮������ �ݿ����� �ʾ�����, HASH-BASED DISTINCTION����
 *       �̿�� �ÿ��� target�� ������ �̿��ϰ����� ������ dstNode �Ǵ�
 *       passNode�� target�� �ٽ� �޾��־�� ������ SORT��� ��� �ٷ�
 *       �̿��� �����ϴ�.
 *
 ***********************************************************************/

    qmncHSDS          * sHSDS;
    UInt                sDataNodeOffset;
    UInt                sFlag = 0;
    UInt                sOption = 0;
    idBool              sAppend;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initHSDS::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncHSDS�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncHSDS ),
                                               (void **)& sHSDS )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sHSDS ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_HSDS ,
                        qmnHSDS ,
                        qmndHSDS,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sHSDS;

    sOption &= ~QMC_APPEND_EXPRESSION_MASK;
    sOption |= QMC_APPEND_EXPRESSION_TRUE;

    sOption &= ~QMC_APPEND_ALLOW_CONST_MASK;
    sOption |= QMC_APPEND_ALLOW_CONST_TRUE;

    sOption &= ~QMC_APPEND_ALLOW_DUP_MASK;
    sOption |= QMC_APPEND_ALLOW_DUP_FALSE;

    if( aExplicitDistinct == ID_TRUE )
    {
        // SELECT DISTINCT���� ����� ���
        for( sItrAttr = aParent->resultDesc;
             sItrAttr != NULL;
             sItrAttr = sItrAttr->next )
        {
            if( ( sItrAttr->flag & QMC_ATTR_DISTINCT_MASK )
                == QMC_ATTR_DISTINCT_TRUE )
            {
                sAppend = ID_TRUE;
            }
            else
            {
                sAppend = ID_FALSE;
            }

            if( sAppend == ID_TRUE )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sHSDS->plan.resultDesc,
                                                sItrAttr->expr,
                                                sFlag,
                                                sOption,
                                                ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }

        // DISTINCT������ �����Ǿ��ٴ� flag�� �� �̻� �������� �ʵ��� �����Ѵ�.
        for( sItrAttr = sHSDS->plan.resultDesc;
             sItrAttr != NULL;
             sItrAttr = sItrAttr->next )
        {
            sItrAttr->flag &= ~QMC_ATTR_DISTINCT_MASK;
            sItrAttr->flag |= QMC_ATTR_DISTINCT_FALSE;
        }

        IDE_TEST( qmc::makeReferenceResult( aStatement,
                                            ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                            aParent->resultDesc,
                                            sHSDS->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // IN-SUBQUERY KEYRANGE �Ǵ� UNION�� ����
        // ������ VIEW�� �����ϹǷ� ���� PROJECTION�� ����� �����Ѵ�.
        IDE_TEST( qmc::copyResultDesc( aStatement,
                                       aParent->resultDesc,
                                       & sHSDS->plan.resultDesc )
                  != IDE_SUCCESS );
    }

    IDE_DASSERT( sHSDS->plan.resultDesc != NULL );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sHSDS->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeHSDS( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aFlag ,
                         UInt           aBucketCount ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan )
{
    qmncHSDS          * sHSDS = (qmncHSDS *)aPlan;
    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sLastMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;

    qmcMtrNode        * sFirstMtrNode = NULL;

    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeHSDS::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndHSDS));

    sHSDS->plan.left = aChildPlan;

    sHSDS->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sHSDS->flag      = QMN_PLAN_FLAG_CLEAR;
    sHSDS->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sHSDS->bucketCnt     = aBucketCount;
    sHSDS->myNode        = NULL;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if( (aFlag & QMO_MAKEHSDS_TEMP_TABLE_MASK) ==
        QMO_MAKEHSDS_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    //bug-7405 fixed
    if( QC_SHARED_TMPLATE(aStatement)->stmt == aStatement )
    {
        // �ֻ��� Query���� ���Ǵ� ���
        sHSDS->flag  &= ~QMNC_HSDS_IN_TOP_MASK;
        sHSDS->flag  |= QMNC_HSDS_IN_TOP_TRUE;

        // BUG-31997: When using temporary tables by RID, RID refers
        // to the invalid row.
        sHSDS->plan.flag  &= ~QMN_PLAN_TEMP_FIXED_RID_MASK;
        sHSDS->plan.flag  |= QMN_PLAN_TEMP_FIXED_RID_TRUE;
    }
    else
    {
        // �ֻ��� Query�� �ƴѰ��
        sHSDS->flag  &= ~QMNC_HSDS_IN_TOP_MASK;
        sHSDS->flag  |= QMNC_HSDS_IN_TOP_FALSE;

        // BUG-31997: When using temporary tables by RID, RID refers
        // to the invalid row.
        sHSDS->plan.flag  &= ~QMN_PLAN_TEMP_FIXED_RID_MASK;
        sHSDS->plan.flag  |= QMN_PLAN_TEMP_FIXED_RID_FALSE;
    }

    //----------------------------------
    // PROJ-1473
    // mtrNode ��������
    // ���ǿ� ���� �÷��� ���� ����
    //----------------------------------

    //----------------------------------
    // 1473TODO
    // �޸����̺� ���� ���̽����̺���������ʿ�
    // ��, ���ǿ� ���� �÷������� ���� �ʿ�.
    //----------------------------------

    sHSDS->baseTableCount = 0;

    sColumnCount = 0;

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                          aQuerySet,
                                          sItrAttr->expr,
                                          ID_FALSE,
                                          sTupleID,
                                          0,
                                          & sColumnCount,
                                          & sNewMtrNode )
                  != IDE_SUCCESS );

        sNewMtrNode->flag &= ~QMC_MTR_HASH_NEED_MASK;
        sNewMtrNode->flag |= QMC_MTR_HASH_NEED_TRUE;

        if( sFirstMtrNode == NULL )
        {
            sFirstMtrNode       = sNewMtrNode;
            sLastMtrNode        = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next  = sNewMtrNode;
            sLastMtrNode        = sNewMtrNode;
        }
    }

    sHSDS->myNode = sFirstMtrNode;

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKEHSDS_TEMP_TABLE_MASK) ==
        QMO_MAKEHSDS_MEMORY_TEMP_TABLE )
    {
        sHSDS->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sHSDS->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sHSDS->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sHSDS->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sHSDS->myNode )
              != IDE_SUCCESS);

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    for (sNewMtrNode = sHSDS->myNode , sColumnCount = 0 ;
         sNewMtrNode != NULL;
         sNewMtrNode = sNewMtrNode->next , sColumnCount++ ) ;
    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sMtrNode[0]  = sHSDS->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sHSDS->plan,
                                            QMO_HSDS_DEPENDENCY,
                                            sTupleID,
                                            NULL,
                                            0,
                                            NULL,
                                            1,
                                            sMtrNode )
                 != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sHSDS->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sHSDS->depTupleRowID = (UShort)sHSDS->plan.dependency;

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sHSDS->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sHSDS->myNode )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sHSDS->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sHSDS->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sHSDS->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sHSDS->planID,
                               sHSDS->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sHSDS->myNode,
                               &sHSDS->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeHSDS",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initLMST( qcStatement    * aStatement ,
                         qmsQuerySet    * aQuerySet ,
                         UInt             aFlag ,
                         qmsSortColumns * aOrderBy ,
                         qmnPlan        * aParent ,
                         qmnPlan       ** aPlan )
{
/***********************************************************************
 *
 * Description : LMST ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncLMST�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - LMST����� ��뱸��
 *         - LMST����� �÷� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - �÷��� ������ ������ ���� 3������ �Ѵ�. �̿����� �������̽���
 *       �������ֵ��� �Ѵ�. (private���� ó��)
 *         - LIMIT ORDER BY�� ���̴� ��� (orderBy���� �̿�)
 *         - STORE AND SEARCH (���� VIEW�� ����)
 *
 ***********************************************************************/

    qmncLMST          * sLMST;
    qmsSortColumns    * sSortColumn;
    qmcAttrDesc       * sItrAttr;
    qtcNode           * sNode;
    UInt                sDataNodeOffset;
    UInt                sColumnCount;
    UInt                sFlag = 0;
    SInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initLMST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParent != NULL );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncLMST ),
                                               (void **)& sLMST )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sLMST ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_LMST ,
                        qmnLMST ,
                        qmndLMST,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sLMST->flag      = QMN_PLAN_FLAG_CLEAR;

    // Sorting�� �ʿ����� ǥ��
    sLMST->flag &= ~QMNC_LMST_USE_MASK;
    sLMST->flag |= QMNC_LMST_USE_ORDERBY;

    *aPlan = (qmnPlan *)sLMST;

    // Sorting key���� ǥ���Ѵ�.
    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    // Order by���� attribute���� �߰��Ѵ�.
    for( sSortColumn = aOrderBy, sColumnCount = 0;
         sSortColumn != NULL;
         sSortColumn = sSortColumn->next, sColumnCount++ )
    {
        // Sorting ������ ǥ���Ѵ�.
        if( sSortColumn->isDESC == ID_FALSE )
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_ASC;
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_DESC;
        }

        /* PROJ-2435 order by nulls first/last */
        if ( sSortColumn->nullsOption != QMS_NULLS_NONE )
        {
            if ( sSortColumn->nullsOption == QMS_NULLS_FIRST )
            {
                sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                sFlag |= QMC_ATTR_SORT_NULLS_ORDER_FIRST;
            }
            else
            {
                sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                sFlag |= QMC_ATTR_SORT_NULLS_ORDER_LAST;
            }
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_NULLS_ORDER_NONE;
        }

        /* BUG-36826 A rollup or cube occured wrong result using order by count_i3 */
        if ( ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK ) == QMO_MAKESORT_VALUE_TEMP_TRUE ) ||
             ( ( aFlag & QMO_MAKESORT_GROUP_EXT_VALUE_MASK ) == QMO_MAKESORT_GROUP_EXT_VALUE_TRUE ) )
        {
            for ( i = 1, sItrAttr = aParent->resultDesc;
                  i < sSortColumn->targetPosition;
                  i++ )
            {
                sItrAttr = sItrAttr->next;
            }

            if ( sItrAttr != NULL )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sLMST->plan.resultDesc,
                                                sItrAttr->expr,
                                                sFlag,
                                                QMC_APPEND_EXPRESSION_TRUE,
                                                ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sLMST->plan.resultDesc,
                                            sSortColumn->sortColumn,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
    }

    // ORDER BY �� �� �����Ǵ� attribute���� �߰��Ѵ�.
    for ( sItrAttr = aParent->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        /* BUG-37146 together with rollup order by the results are wrong.
         * Value Temp�� ������ SortTemp�� ���� ��� ��� SortTemp�� value temp��
         * �����ؾ��Ѵ�. ���� ���� Plan�� exression�� ��� sort�� �߰��ϰ� ����
         * pushResultDesc �Լ����� passNode�� ���鵵�� �����Ѵ�.
         */
        if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK ) ==
             QMO_MAKESORT_VALUE_TEMP_TRUE )
        {
            if ( ( sItrAttr->expr->lflag & QTC_NODE_AGGREGATE_MASK ) ==
                 QTC_NODE_AGGREGATE_EXIST )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sLMST->plan.resultDesc,
                                                sItrAttr->expr,
                                                QMC_ATTR_SEALED_TRUE,
                                                QMC_APPEND_EXPRESSION_TRUE,
                                                ID_FALSE )
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

        /* BUG-35265 wrong result desc connect_by_root, sys_connect_by_path */
        if ( ( sItrAttr->expr->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
             == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            /* �����ؼ� ���� ������ ���� Parent �� �ٲ� �߸��� Tuple�� ����Ű�Եȴ� */
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                       (void**)&sNode )
                      != IDE_SUCCESS );
            idlOS::memcpy( sNode, sItrAttr->expr, ID_SIZEOF( qtcNode ) );

            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sLMST->plan.resultDesc,
                                            sNode,
                                            QMC_ATTR_SEALED_TRUE,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do*/
        }
    }

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                   aParent->resultDesc,
                                   &sLMST->plan.resultDesc )
              != IDE_SUCCESS );

    // BUG-36997
    // select distinct i1, i2+1 from t1 order by i1;
    // distinct �� ó���Ҷ� i2+1 �� �����ؼ� ������ ���´�.
    // ���� PASS ��带 �������־�� �Ѵ�.
    IDE_TEST( qmc::makeReferenceResult( aStatement,
                                        ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                        aParent->resultDesc,
                                        sLMST->plan.resultDesc )
              != IDE_SUCCESS );

    // ORDER BY������ �����Ǿ��ٴ� flag�� �� �̻� �������� �ʵ��� �����Ѵ�.
    for ( sItrAttr  = sLMST->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        sItrAttr->flag &= ~QMC_ATTR_ORDER_BY_MASK;
        sItrAttr->flag |= QMC_ATTR_ORDER_BY_FALSE;
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sLMST->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initLMST( qcStatement    * aStatement ,
                         qmsQuerySet    * aQuerySet ,
                         qmnPlan        * aParent ,
                         qmnPlan       ** aPlan )
{
    qmncLMST          * sLMST;
    UInt                sDataNodeOffset;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initLMST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncLMST ),
                                               (void **)& sLMST )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sLMST ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_LMST ,
                        qmnLMST ,
                        qmndLMST,
                        sDataNodeOffset );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sLMST->flag = QMN_PLAN_FLAG_CLEAR;
    sLMST->flag &= ~QMNC_LMST_USE_MASK;
    sLMST->flag |= QMNC_LMST_USE_STORESEARCH;

    *aPlan = (qmnPlan *)sLMST;

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   & sLMST->plan.resultDesc )
              != IDE_SUCCESS );

    for( sItrAttr = sLMST->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        sItrAttr->flag &= ~QMC_ATTR_KEY_MASK;
        sItrAttr->flag |= QMC_ATTR_KEY_TRUE;

        sItrAttr->flag &= ~QMC_ATTR_SORT_ORDER_MASK;
        sItrAttr->flag |= QMC_ATTR_SORT_ORDER_ASC;
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sLMST->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeLMST( qcStatement    * aStatement ,
                         qmsQuerySet    * aQuerySet ,
                         ULong            aLimitRowCount ,
                         qmnPlan        * aChildPlan ,
                         qmnPlan        * aPlan )
{
    qmncLMST          * sLMST = (qmncLMST*)aPlan;
    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount = 0;
    qmcMtrNode        * sNewMtrNode = NULL;
    qmcMtrNode        * sFirstMtrNode = NULL;
    qmcMtrNode        * sLastMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;

    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeLMST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );
    IDE_DASSERT( aPlan != NULL );

    //qmncLMST�� �Ҵ� �� �ʱ�ȭ
    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndLMST));

    sLMST->plan.left = aChildPlan;

    sLMST->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sLMST->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sLMST->limitCnt      = aLimitRowCount;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
            == QMN_PLAN_STORAGE_DISK )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sLMST->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    // Sorting key�� �ƴ� ���
    IDE_TEST( makeNonKeyAttrsMtrNodes( aStatement,
                                       aQuerySet,
                                       sLMST->plan.resultDesc,
                                       sTupleID,
                                       & sFirstMtrNode,
                                       & sLastMtrNode,
                                       & sColumnCount )
              != IDE_SUCCESS );

    sLMST->baseTableCount = sColumnCount;

    // PROJ-2362 memory temp ���� ȿ���� ����
    // limit sort���� TEMP_VAR_TYPE�� ������� �ʴ´�.
    for( sNewMtrNode = sFirstMtrNode;
         sNewMtrNode != NULL;
         sNewMtrNode = sNewMtrNode->next )
    {
        sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
        sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;
    }

    for( sItrAttr = aPlan->resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        // Sorting key �� ���
        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              &sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // limit sort���� TEMP_VAR_TYPE�� ������� �ʴ´�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK ) == QMC_ATTR_SORT_ORDER_ASC )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
            }

            /* PROJ-2435 order by nulls first/last */
            if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                 != QMC_ATTR_SORT_NULLS_ORDER_NONE )
            {
                if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                     == QMC_ATTR_SORT_NULLS_ORDER_FIRST )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_FIRST;
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_LAST;
                }
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_NONE;
            }

            if( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    sLMST->myNode = sFirstMtrNode;

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //LMST�� memory�� �����ü�� ����Ѵ�.
    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    sLMST->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
    sLMST->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;

    if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
            == QMN_PLAN_STORAGE_DISK )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sLMST->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sLMST->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sLMST->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    for ( sNewMtrNode  = sLMST->myNode , sColumnCount = 0 ;
          sNewMtrNode != NULL;
          sNewMtrNode  = sNewMtrNode->next , sColumnCount++ ) ;

    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sMtrNode[0]  = sLMST->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sLMST->plan ,
                                            QMO_LMST_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sLMST->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sLMST->depTupleRowID = (UShort)sLMST->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sLMST->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sLMST->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sLMST->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sLMST->planID,
                               sLMST->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sLMST->myNode,
                               &sLMST->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeLMST",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initVMTR( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aParent ,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : VMTR ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncVMTR�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - VMTR����� �÷� ����(���� VIEW�� ����)
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncVMTR          * sVMTR;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initVMTR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncVMTR�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncVMTR ),
                                               (void **)& sVMTR )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sVMTR ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_VMTR ,
                        qmnVMTR ,
                        qmndVMTR,
                        sDataNodeOffset );

    IDE_TEST( qmc::createResultFromQuerySet( aStatement,
                                             aQuerySet,
                                             & sVMTR->plan.resultDesc )
              != IDE_SUCCESS );

    if ( aParent != NULL )
    {
        /* PROJ-2462 Result Cache
         * Top Result Cache�� ��� VMTR�� ������ �׻� * Projection�� �����ȴ�.
         */
        if ( aParent->type == QMN_PROJ )
        {
            IDE_TEST( qmo::initResultCacheStack( aStatement,
                                                 aQuerySet,
                                                 sVMTR->planID,
                                                 ID_TRUE,
                                                 ID_TRUE )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmo::initResultCacheStack( aStatement,
                                                 aQuerySet,
                                                 sVMTR->planID,
                                                 ID_FALSE,
                                                 ID_TRUE )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( qmo::initResultCacheStack( aStatement,
                                             aQuerySet,
                                             sVMTR->planID,
                                             ID_FALSE,
                                             ID_TRUE )
                  != IDE_SUCCESS );
    }

    *aPlan = (qmnPlan *)sVMTR;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeVMTR( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aFlag ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : VMTR ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncVMTR�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - VMTR����� �÷� ����(���� VIEW�� ����)
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncVMTR          * sVMTR = (qmncVMTR*)aPlan;
    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount = 0;
    qtcNode           * sNode;
    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sLastMtrNode = NULL;

    mtcTemplate       * sMtcTemplate;

    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeVMTR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPlan != NULL );
    IDE_DASSERT( aChildPlan->type == QMN_VIEW );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndVMTR));

    sVMTR->plan.left        = aChildPlan;

    sVMTR->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sVMTR->flag      = QMN_PLAN_FLAG_CLEAR;
    sVMTR->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sVMTR->myNode    = NULL;

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------

    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    // To Fix PR-8493
    // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    if ( (aFlag & QMO_MAKEVMTR_TEMP_TABLE_MASK) ==
         QMO_MAKEVMTR_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sVMTR->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
                 != IDE_SUCCESS );

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qtcNode, & sNode )
                  != IDE_SUCCESS);

        idlOS::memcpy( sNode,
                       sItrAttr->expr,
                       ID_SIZEOF(qtcNode) );

        IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                          aQuerySet ,
                                          sItrAttr->expr,
                                          ID_FALSE,
                                          sTupleID ,
                                          0,
                                          & sColumnCount ,
                                          & sNewMtrNode )
                  != IDE_SUCCESS );

        if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) == QMC_ATTR_USELESS_RESULT_TRUE )
        {
            // PROJ-2469 Optimize View Materialization
            // �������� ������� �ʴ� MtrNode�� ���ؼ� flagǥ���Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |=  QMC_MTR_TYPE_USELESS_COLUMN;
        }
        else
        {
            //VMTR���� ������ qmcMtrNode�̹Ƿ� �����ϵ��� �Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;
        }
        
        // PROJ-2179
        sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
        sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

        // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
        sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
        sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

        //connect
        if( sVMTR->myNode == NULL )
        {
            sVMTR->myNode = sNewMtrNode;
            sLastMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
    }

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if ( (aFlag & QMO_MAKEVMTR_TEMP_TABLE_MASK) ==
         QMO_MAKEVMTR_MEMORY_TEMP_TABLE )
    {
        sVMTR->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sVMTR->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sVMTR->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sVMTR->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;
    }

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sVMTR->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sVMTR->myNode )
              != IDE_SUCCESS );

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sMtrNode[0]  = sVMTR->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sVMTR->plan ,
                                            QMO_VMTR_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sVMTR->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sVMTR->depTupleID = ( UShort )sVMTR->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sVMTR->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sVMTR->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sVMTR->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    if ( ( aFlag & QMO_MAKEVMTR_TOP_RESULT_CACHE_MASK )
         == QMO_MAKEVMTR_TOP_RESULT_CACHE_TRUE )
    {
        qmo::makeResultCacheStack( aStatement,
                                   aQuerySet,
                                   sVMTR->planID,
                                   sVMTR->plan.flag,
                                   sMtcTemplate->rows[sTupleID].lflag,
                                   sVMTR->myNode,
                                   &sVMTR->componentInfo,
                                   ID_TRUE );
    }
    else
    {
        qmo::makeResultCacheStack( aStatement,
                                   aQuerySet,
                                   sVMTR->planID,
                                   sVMTR->plan.flag,
                                   sMtcTemplate->rows[sTupleID].lflag,
                                   sVMTR->myNode,
                                   &sVMTR->componentInfo,
                                   ID_FALSE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeVMTR",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::initWNST( qcStatement        * aStatement,
                         qmsQuerySet        * aQuerySet,
                         UInt                 aSortKeyCount,
                         qmsAnalyticFunc   ** aAnalyticFuncListPtrArr,
                         UInt                 aFlag,
                         qmnPlan            * aParent,
                         qmnPlan           ** aPlan )
{
/***********************************************************************
 *
 * Description : WNST ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncWNST�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - �˻� ��� �� ���� ��� flag ����
 *         - WNST����� �÷� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 *    Graph���� �Ѱ��ִ� analytic function list ptr array ����
 *    ex ) SELECT i,
 *                sum(i1) over ( partition by i1 ),                 ==> a1
 *                sum(i2) over ( partition by i1,i2 ),              ==> a2
 *                sum(i2) over ( partition by i1 ),                 ==> a3
 *                sum(i3) over ( partition by i3 ),                 ==> a4
 *                sum(i2) over ( partition by i1 order by i2 )      ==> a5
 *                sum(i2) over ( partition by i1 order by i2 desc ) ==> a6
 *                sum(i3) over ( partition by i1 order by i2 )      ==> a7
 *         FROM t1;
 *
 *       < Sorting Key Pointer Array >
 *        +-------------------+
 *        |    |    |    |    |
 *        +-|----|-----|----|-+
 *          |    |     |    |
 *          |    |     |   (i1)-(i2 desc)
 *          |    |     |
 *          |   (i3)  (i1)-(i2 asc)
 *          |
 *        (i1)-(i2)
 *
 *       < Analytic Function Pointer Array >
 *        +-------------------+
 *        |    |    |    |    |
 *        +-|----|----|----|--+
 *          |    |    |    |
 *          |    |    |   (a6)
 *          |    |    |
 *          |    |   (a5)-(a7)
 *          |   (a4)
 *          |
 *        (a1)-(a2)-(a3)
 *
 * < �� �Լ� ���� ����� ����� WNST plan >
 * +--------+
 * | WNST   |
 * +--------+
 * | myNode |-->(baseTable/ -(i1)-(i2)-(i3)-(i2 asc)-(i2 desc)-(sum(i1))-(sum(i2))-(sum(i2))-(sum(i3))-(sum(i2))-(sum(i2))-(sum(i3))
 * |        |   baseColumn)
 * |        |
 * |        |
 * |        |   +---------------+
 * | wndNode|-->| wndNode       |
 * |        |   +---------------+
 * |        |   | overColumnNode|-->(i1,i2)
 * |        |   | aggrNode      |-->(sum(i1))-(sum(i2))-(sum(i2))
 * |        |   | aggrResultNode|-->(sum(i1))-(sum(i2))-(sum(i2))
 * |        |   | next ---+     |
 * |        |   +--------/------+
 * |        |   +-------V-------+
 * |        |   | wndNode       |
 * |        |   +---------------+
 * |        |   | overColumnNode|-->(i3)
 * |        |   | aggrNode      |-->(sum(i3))
 * |        |   | aggrResultNode|-->(sum(i3))
 * |        |   | next          |
 * |        |   +--------/------+
 * |        |   +-------V-------+
 * |        |   | wndNode       |
 * |        |   +---------------+
 * |        |   | overColumnNode|-->(i1,i2 asc)
 * |        |   | aggrNode      |-->(sum(i2))-(sum(i3))
 * |        |   | aggrResultNode|-->(sum(i2))-(sum(i3))
 * |        |   | next          |
 * |        |   +--------/------+
 * |        |   +-------V-------+
 * |        |   | wndNode       |
 * |        |   +---------------+
 * |        |   | overColumnNode|-->(i1,i2 desc)
 * |        |   | aggrNode      |-->(sum(i2))
 * |        |   | aggrResultNode|-->(sum(i2))
 * |        |   | next          |
 * |        |   +---------------+
 * |        |
 * |        |
 * |sortNode|-->(i1,i2)-(i3)-(i1,i2 asc)-(i1,i2 desc)
 * |        |
 * |distNode|-->(distinct i1)
 * +--------+
 *
 ***********************************************************************/

    qmncWNST          * sWNST;
    qmsAnalyticFunc   * sAnalyticFunc;
    UInt                sDataNodeOffset;
    UInt                sFlag = 0;
    qtcOverColumn     * sOverColumn;
    qtcNode           * sNode;
    UInt                i;
    qmcAttrDesc       * sItrAttr;
    mtcNode           * sArg;
    qtcNode           * sAnalyticArg;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initWNST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncWNST�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmncWNST) ,
                                            (void **)&sWNST )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sWNST,
                        QC_SHARED_TMPLATE(aStatement),
                        QMN_WNST,
                        qmnWNST,
                        qmndWNST,
                        sDataNodeOffset );

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    // Over ���� attribute���� key attribute�� �߰�
    for ( i = 0; i < aSortKeyCount; i++ )
    {
        // sorting key array���� i��° sorting key��
        // analytic function list pointer array���� i��°�� �ִ�
        // anlaytic function list���� sorting key �̹Ƿ�
        // ���߿��� ���� key count�� ������ partition by column�� ã����
        // �ȴ�.
        for ( sAnalyticFunc = aAnalyticFuncListPtrArr[i];
              sAnalyticFunc != NULL;
              sAnalyticFunc = sAnalyticFunc->next )
        {
            for ( sOverColumn = sAnalyticFunc->analyticFuncNode->overClause->overColumn;
                  sOverColumn != NULL;
                  sOverColumn = sOverColumn->next )
            {
                if ( ( sOverColumn->flag & QTC_OVER_COLUMN_MASK )
                     == QTC_OVER_COLUMN_ORDER_BY )
                {
                    sFlag &= ~QMC_ATTR_ANALYTIC_SORT_MASK;
                    sFlag |= QMC_ATTR_ANALYTIC_SORT_TRUE;
                }
                else
                {
                    sFlag &= ~QMC_ATTR_ANALYTIC_SORT_MASK;
                    sFlag |= QMC_ATTR_ANALYTIC_SORT_FALSE;
                }

                if ( ( sOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK )
                     == QTC_OVER_COLUMN_ORDER_ASC )
                {
                    sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
                    sFlag |= QMC_ATTR_SORT_ORDER_ASC;
                }
                else
                {
                    sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
                    sFlag |= QMC_ATTR_SORT_ORDER_DESC;
                }

                /* PROJ-243 Order by Nulls first/last */
                if ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                     == QTC_OVER_COLUMN_NULLS_ORDER_NONE )
                {
                    sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                    sFlag |= QMC_ATTR_SORT_NULLS_ORDER_NONE;
                }
                else
                {
                    if ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                         == QTC_OVER_COLUMN_NULLS_ORDER_FIRST )
                    {
                        sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                        sFlag |= QMC_ATTR_SORT_NULLS_ORDER_FIRST;
                    }
                    else
                    {
                        sFlag &= ~QMC_ATTR_SORT_NULLS_ORDER_MASK;
                        sFlag |= QMC_ATTR_SORT_NULLS_ORDER_LAST;
                    }
                }

                // BUG-34966 OVER���� column�鿡 pass node�� ������ �� �ִ�.
                IDE_TEST( qmc::duplicateGroupExpression( aStatement,
                                                         (qtcNode*)sOverColumn->node )
                          != IDE_SUCCESS );

                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sWNST->plan.resultDesc,
                                                sOverColumn->node,
                                                sFlag,
                                                QMC_APPEND_EXPRESSION_TRUE |
                                                QMC_APPEND_CHECK_ANALYTIC_TRUE |
                                                QMC_APPEND_ALLOW_CONST_TRUE,
                                                ID_FALSE )
                          != IDE_SUCCESS );

                /* BUG-37672 the result of windowing query is not correct. */
                if ( ( ( sOverColumn->node->lflag & QTC_NODE_AGGREGATE_MASK ) ==
                       QTC_NODE_AGGREGATE_ABSENT ) &&
                     ( sAnalyticFunc->analyticFuncNode->node.arguments != NULL ) )
                {
                    sAnalyticArg = (qtcNode *)sAnalyticFunc->analyticFuncNode->node.arguments;
                    if ( ( sAnalyticArg->node.arguments != NULL ) ||
                         ( sAnalyticArg->node.next != NULL ) )
                    {
                        /* ���� ���� ���̺������� üũ */
                        if ( qtc::dependencyContains( &sAnalyticArg->depInfo,
                                                      &sOverColumn->node->depInfo )
                             == ID_TRUE )
                        {
                            for ( sArg = sOverColumn->node->node.arguments;
                                  sArg != NULL;
                                  sArg = sArg->next )
                            {
                                IDE_TEST( qmc::appendAttribute( aStatement,
                                                                aQuerySet,
                                                                & sWNST->plan.resultDesc,
                                                                (qtcNode *)sArg,
                                                                0,
                                                                0,
                                                                ID_FALSE )
                                          != IDE_SUCCESS );
                            }
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
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }

    sFlag = 0;
    sFlag &= ~QMC_ATTR_ANALYTIC_FUNC_MASK;
    sFlag |= QMC_ATTR_ANALYTIC_FUNC_TRUE;

    // Analytic function�� attribute�� �߰�
    for( sAnalyticFunc = aQuerySet->analyticFuncList;
         sAnalyticFunc != NULL;
         sAnalyticFunc = sAnalyticFunc->next )
    {
        sNode = sAnalyticFunc->analyticFuncNode;

        // Argument�� attribute�� �߰��Ѵ�.
        if( sNode->node.arguments != NULL )
        {
            // PROJ-2527 WITHIN GROUP AGGR
            // WITHIN GROUP�� node�� resultDesc��� �ؾ� �Ѵ�.
            for ( sArg = sNode->node.arguments;
                  sArg != NULL;
                  sArg = sArg->next )
            {
                // BUG-34966 Argument�� GROUP BY���� column�̸鼭 ORDER BY�������� ���� ���
                //           argument�� ������ �����Ͽ� analytic function���� �����Ѵ�.
                IDE_TEST( qmc::duplicateGroupExpression( aStatement,
                                                         (qtcNode*)sArg )
                          != IDE_SUCCESS );

                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sWNST->plan.resultDesc,
                                                (qtcNode *)sArg,
                                                0,
                                                0,
                                                ID_FALSE )
                          != IDE_SUCCESS );
            }

            // Aggregate function�� argument�� pass node�� ������ ��� fast execution��
            // �����ؼ��� �ȵǹǷ� �ٽ� estimation�Ѵ�.
            // BUGBUG: SELECT clause���� GROUP BY clause�� ���� validation�ؾ� �Ѵ�.
            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     (qtcNode*)sNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
            // COUNT(*) �� ��� argument�� NULL�̴�.
        }

        // BUG-44046 recursive with�̸� conversion node�� ���� �Ѵ�.
        // parent�� conversion node�� ���� ��찡 �ִ�.
        if ( ( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
             ( ( aQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
        {
            sFlag &= ~QMC_ATTR_CONVERSION_MASK;
            sFlag |= QMC_ATTR_CONVERSION_TRUE;

            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sWNST->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sWNST->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
    }

    if( aParent != NULL )
    {
        // BUG-37277
        if ( ( aFlag & QMO_MAKEWNST_ORDERBY_GROUPBY_MASK ) ==
             QMO_MAKEWNST_ORDERBY_GROUPBY_TRUE )
        {
            for ( sItrAttr = aParent->resultDesc;
                  sItrAttr != NULL;
                  sItrAttr = sItrAttr->next )
            {
                if ( ( sItrAttr->expr->lflag & QTC_NODE_ANAL_FUNC_MASK ) ==
                     QTC_NODE_ANAL_FUNC_ABSENT )
                {
                    sItrAttr->flag &= ~QMC_ATTR_SEALED_MASK;
                    sItrAttr->flag |= QMC_ATTR_SEALED_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qmc::pushResultDesc( aStatement,
                                       aQuerySet,
                                       ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                       aParent->resultDesc,
                                       & sWNST->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sWNST->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sWNST;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeWNST( qcStatement          * aStatement,
                         qmsQuerySet          * aQuerySet,
                         UInt                   aFlag,
                         qmoDistAggArg        * aDistAggArg,
                         UInt                   aSortKeyCount,
                         qmgPreservedOrder   ** aSortKeyPtrArr,
                         qmsAnalyticFunc     ** aAnalFuncListPtrArr,
                         qmnPlan              * aChildPlan,
                         SDouble                aStoreRowCount,
                         qmnPlan              * aPlan )
{
    qmncWNST          * sWNST = (qmncWNST *)aPlan;
    UInt                sDataNodeOffset;
    qmcMtrNode        * sMtrNode[2];
    mtcTemplate       * sMtcTemplate;

    qmcWndNode       ** sWndNodePtrArr;
    UInt                sAnalFuncListPtrCount;
    qmcMtrNode       ** sSortMtrNodePtrArr;

    UShort              sTupleID;
    UShort              sColumnCount;
    UShort              sAggrTupleID;
    UShort              sAggrColumnCount;

    qmcWndNode        * sCurWndNode;
    qmcMtrNode        * sWndAggrNode;
    qmcMtrNode        * sCurAggrNode;

    qmcMtrNode        * sNewAggrNode;

    UInt                sWndNodeCount;
    UShort              sDistNodeCount;
    UInt                sAggrNodeCount;
    UInt                sSortNodeCount;

    UInt                sWndOverColumnNodeCount;
    UInt                sWndAggrNodeCount;
    UInt                sWndAggrResultNodeCount;

    qmcMtrNode        * sFirstAnalResultFuncMtrNode;
    qmcMtrNode        * sCurAnalResultFuncMtrNode;

    qmsAnalyticFunc   * sCurAnalyticFunc;
    UInt                i;

    UInt                sCurOffset;

    qtcOverColumn     * sOverColumn;
    mtcNode           * sNextNode;
    mtcNode           * sConversion;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeWNST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    // �⺻ �ʱ�ȭ
    sMtcTemplate      = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sColumnCount      = 0;
    sAggrColumnCount  = 0;

    sWndNodeCount     = 0;
    sDistNodeCount    = 0;
    sAggrNodeCount    = 0;
    sSortNodeCount    = 0;

    sWndOverColumnNodeCount = 0;
    sWndAggrNodeCount = 0;
    sWndAggrResultNodeCount = 0;
    sFirstAnalResultFuncMtrNode = NULL;
    sCurAnalResultFuncMtrNode = NULL;

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndWNST));

    sWNST->plan.left = aChildPlan;

    // Flag ���� ����
    sWNST->flag      = QMN_PLAN_FLAG_CLEAR;
    sWNST->plan.flag = QMN_PLAN_FLAG_CLEAR;

    if ( aSortKeyCount == 0 )
    {
        // sort key�� ���� ���, sorting �� �ʿ����
        sWNST->flag &= ~QMNC_WNST_STORE_MASK;
        sWNST->flag |= QMNC_WNST_STORE_ONLY;
    }
    else
    {
        if ( ( aFlag & QMO_MAKEWNST_PRESERVED_ORDER_MASK ) ==
             QMO_MAKEWNST_PRESERVED_ORDER_TRUE )
        {
            /* BUG-40354 pushed rank */
            if ( ( aFlag & QMO_MAKEWNST_PUSHED_RANK_MASK ) ==
                 QMO_MAKEWNST_PUSHED_RANK_TRUE )
            {
                sWNST->flag &= ~QMNC_WNST_STORE_MASK;
                sWNST->flag |= QMNC_WNST_STORE_LIMIT_PRESERVED_ORDER;
            }
            else
            {
                sWNST->flag &= ~QMNC_WNST_STORE_MASK;
                sWNST->flag |= QMNC_WNST_STORE_PRESERVED_ORDER;
            }
        }
        else
        {
            /* BUG-40354 pushed rank */
            if ( ( aFlag & QMO_MAKEWNST_PUSHED_RANK_MASK ) ==
                 QMO_MAKEWNST_PUSHED_RANK_TRUE )
            {
                sWNST->flag &= ~QMNC_WNST_STORE_MASK;
                sWNST->flag |= QMNC_WNST_STORE_LIMIT_SORTING;
            }
            else
            {
                sWNST->flag &= ~QMNC_WNST_STORE_MASK;
                sWNST->flag |= QMNC_WNST_STORE_SORTING;
            }
        }
    }

    sWNST->myNode        = NULL;
    sWNST->distNode      = NULL;
    sWNST->aggrNode      = NULL;
    sWNST->storeRowCount = aStoreRowCount;

    sWNST->sortKeyCnt = aSortKeyCount;

    //--------------------------
    // wndNode ���� �� �ʱ�ȭ
    //--------------------------
    if ( aSortKeyCount == 0 )
    {
        // Sort Key�� ������
        // Analytic Function Ptr Array�� 0��° ��ġ��
        // ��� analytic function���� list�� ����Ǿ� ����
        sAnalFuncListPtrCount = 1;
    }
    else
    {
        // Sort Key�� ������
        // Sort Key ������ŭ analytic function ptr array�� �����ϰ�
        // Sort Key�� �����Ǵ� ��ġ�� ���� sort key�� ������
        // analytic function list���� pointer�� ������ ����
        sAnalFuncListPtrCount = aSortKeyCount;
    }

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( (ID_SIZEOF(qmcWndNode*) *
                                              sAnalFuncListPtrCount),
                                             (void**) &sWndNodePtrArr )
              != IDE_SUCCESS);

    for ( i = 0; i < sAnalFuncListPtrCount; i ++ )
    {
        sWndNodePtrArr[i] = NULL;
    }
    sWNST->wndNode = sWndNodePtrArr;

    //--------------------------
    // sortNode ���� �� �ʱ�ȭ
    //--------------------------
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ( ID_SIZEOF(qmcMtrNode*) *
                                               sAnalFuncListPtrCount ),
                                             (void**) &sSortMtrNodePtrArr )
              != IDE_SUCCESS );

    for ( i = 0; i < sAnalFuncListPtrCount; i ++ )
    {
        sSortMtrNodePtrArr[i] = NULL;
    }
    sWNST->sortNode = sSortMtrNodePtrArr;

    // over���� ����־ sort key count�� 1�� �ȴ�.
    sWNST->sortKeyCnt = sAnalFuncListPtrCount;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sWNST->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Ʃ���� �Ҵ�
    //----------------------------------

    // Analytic Function Result�� �����ϱ� ���Ͽ�
    // �ʿ��� �߰� ����� �����ϱ� ���� tuple �Ҵ� ����
    IDE_TEST( qtc::nextTable( & sAggrTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    // temp table�� ���� tuple�� �Ҵ����
    IDE_TEST( qtc::nextTable(  &sTupleID ,
                               aStatement ,
                               NULL,
                               ID_TRUE,
                               MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    //----------------------------------
    // Tuple�� ���� ��ü ���� ����
    //----------------------------------
    if( (aFlag & QMO_MAKEWNST_TEMP_TABLE_MASK) ==
        QMO_MAKEWNST_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    sMtcTemplate->rows[sAggrTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sAggrTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
            == QMN_PLAN_STORAGE_DISK )
        {
            sMtcTemplate->rows[sAggrTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sAggrTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKEWNST_TEMP_TABLE_MASK) ==
        QMO_MAKEWNST_MEMORY_TEMP_TABLE )
    {
        sWNST->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sWNST->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
    }
    else
    {
        sWNST->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sWNST->plan.flag  |= QMN_PLAN_STORAGE_DISK;
    }

    //----------------------------------
    // myNode ����
    //     �Ʒ��� ���� �����ȴ�.
    //     [Base Table] + [Columns] + [Analytic Function]
    //----------------------------------

    IDE_TEST( makeMyNodeOfWNST( aStatement,
                                aQuerySet,
                                aPlan,
                                aAnalFuncListPtrArr,
                                sAnalFuncListPtrCount,
                                sTupleID,
                                sWNST,
                                & sColumnCount,
                                & sFirstAnalResultFuncMtrNode )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sFirstAnalResultFuncMtrNode == NULL,
                    ERR_MTR_NODE_NOT_FOUND );

    //----------------------------------
    // Temp Table�� ���� Tuple column�� �Ҵ�
    //----------------------------------

    // temp table�� tuple column �Ҵ�
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
        != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if( (aFlag & QMO_MAKEWNST_TEMP_TABLE_MASK) ==
        QMO_MAKEWNST_MEMORY_TEMP_TABLE )
    {
        sWNST->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sWNST->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sWNST->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sWNST->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    for ( i = 0; i < sAnalFuncListPtrCount; i++ )
    {
        for ( sCurAnalyticFunc  = aAnalFuncListPtrArr[i];
              sCurAnalyticFunc != NULL ;
              sCurAnalyticFunc  = sCurAnalyticFunc->next)
         {
             // analytic function�� argument
             IDE_TEST( qmg::changeColumnLocate( aStatement,
                                                aQuerySet,
                                                (qtcNode*)sCurAnalyticFunc->analyticFuncNode->node.arguments,
                                                sTupleID,
                                                ID_TRUE ) // aNext
                       != IDE_SUCCESS );

             // analytic function�� partition by column��
             for ( sOverColumn  =
                       sCurAnalyticFunc->analyticFuncNode->overClause->overColumn;
                   sOverColumn != NULL;
                   sOverColumn  = sOverColumn->next )
             {
                 IDE_TEST( qmg::changeColumnLocate( aStatement,
                                                    aQuerySet,
                                                    sOverColumn->node,
                                                    sTupleID,
                                                    ID_FALSE ) // aNext
                           != IDE_SUCCESS );
             }
         }
    }

    // mtcColumn , mtcExecute ������ ����
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sWNST->myNode )
              != IDE_SUCCESS);

    // column locate ����
    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sWNST->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // wndNode, distNode, aggrNode, sortNode ����
    //----------------------------------
    sCurAnalResultFuncMtrNode = sFirstAnalResultFuncMtrNode;

    for ( i = 0; i < sAnalFuncListPtrCount; i++ )
    {
        for ( sCurAnalyticFunc  = aAnalFuncListPtrArr[i];
              sCurAnalyticFunc != NULL ;
              sCurAnalyticFunc  = sCurAnalyticFunc->next)
        {
            //----------------------------------
            // wndNode ����
            //----------------------------------

            // ���� Partition By�� ������ wndNode �����ϴ��� �˻�
            sCurWndNode = NULL;
            IDE_TEST( existSameWndNode( aStatement,
                                        sTupleID,
                                        sWndNodePtrArr[i],
                                        sCurAnalyticFunc->analyticFuncNode,
                                        & sCurWndNode )
                      != IDE_SUCCESS );

            if ( sCurWndNode == NULL )
            {
                // �������� ���� ���, wndNode ����
                IDE_TEST( makeWndNode( aStatement,
                                       sTupleID,
                                       sWNST->myNode,
                                       sCurAnalyticFunc->analyticFuncNode,
                                       & sWndOverColumnNodeCount,
                                       & sCurWndNode )
                          != IDE_SUCCESS );
                sWndNodeCount++;

                // ���ο� wndNode�� sWNST�� wndNode�� �� �κп� ����
                sCurWndNode->next = sWndNodePtrArr[i];
                sWndNodePtrArr[i] = sCurWndNode;
            }
            else
            {
                // nothing to do
            }

            // wndNode�� aggrNode �߰�
            IDE_TEST( addAggrNodeToWndNode( aStatement,
                                            aQuerySet,
                                            sCurAnalyticFunc,
                                            sAggrTupleID,
                                            & sAggrColumnCount,
                                            sCurWndNode,
                                            & sWndAggrNode )
                      != IDE_SUCCESS );
            sWndAggrNodeCount++;

            // aggrNode�� ���Ͽ� distinct �˻�
            // distinct node �߰�
            if ( ( sCurAnalyticFunc->analyticFuncNode->node.lflag &
                   MTC_NODE_DISTINCT_MASK )
                 == MTC_NODE_DISTINCT_FALSE )
            {
                // Distinction�� �������� ���� ���
                sWndAggrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                sWndAggrNode->flag |= QMC_MTR_DIST_AGGR_FALSE;
                sWndAggrNode->myDist = NULL;
            }
            else
            {
                IDE_TEST_RAISE( (sCurWndNode->execMethod != QMC_WND_EXEC_PARTITION_UPDATE) &&
                                (sCurWndNode->execMethod != QMC_WND_EXEC_AGGR_UPDATE),
                                ERR_INVALID_ORDERBY );

                // Distinction�� �����ϴ� ���
                sWndAggrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                sWndAggrNode->flag |= QMC_MTR_DIST_AGGR_TRUE;

                //----------------------------------
                // distNode ����
                //----------------------------------

                IDE_TEST( qmg::makeDistNode(aStatement,
                                            aQuerySet,
                                            sWNST->plan.flag,
                                            aChildPlan,
                                            sAggrTupleID,
                                            aDistAggArg,
                                            sCurAnalyticFunc->analyticFuncNode,
                                            sWndAggrNode,
                                            & sWNST->distNode,
                                            & sDistNodeCount )
                          != IDE_SUCCESS );
            }

            // wndNode�� aggrResultNode �߰�
            IDE_TEST( addAggrResultNodeToWndNode( aStatement,
                                                  sCurAnalResultFuncMtrNode,
                                                  sCurWndNode )
                      != IDE_SUCCESS );
            sWndAggrResultNodeCount++;

            sCurAnalResultFuncMtrNode = sCurAnalResultFuncMtrNode->next;

            //----------------------------------
            // aggrNode ����
            //----------------------------------

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmcMtrNode ),
                                                     (void**)& sNewAggrNode)
                      != IDE_SUCCESS);

            *sNewAggrNode = *sWndAggrNode;

            sNewAggrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
            sNewAggrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE;

            sNewAggrNode->next = NULL;

            sAggrNodeCount++;

            if ( sWNST->aggrNode == NULL )
            {
                sWNST->aggrNode = sNewAggrNode;
            }
            else
            {
                for ( sCurAggrNode = sWNST->aggrNode;
                      sCurAggrNode->next != NULL;
                      sCurAggrNode = sCurAggrNode->next ) ;

                sCurAggrNode->next = sNewAggrNode;
            }
        }

        //----------------------------------
        // sortNode�� ����
        //----------------------------------

        IDE_TEST( qmg::makeSortMtrNode( aStatement,
                                        sTupleID,
                                        aSortKeyPtrArr[i],
                                        aAnalFuncListPtrArr[i],
                                        sWNST->myNode,
                                        & sSortMtrNodePtrArr[i],
                                        & sSortNodeCount )
                  != IDE_SUCCESS );
    }

    //----------------------------------
    // Aggregation�� ���� �Ҵ���� Tuple column�� �Ҵ�
    //----------------------------------

    // Analytic Function Result�� �����ϱ� ���Ͽ�
    // �ʿ��� �߰� ����� �����ϱ� ���� aggregation�� tuple column �Ҵ�
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sAggrTupleID ,
                                           sAggrColumnCount )
              != IDE_SUCCESS);

    // mtcColumn , mtcExecute ������ ����
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sWNST->aggrNode )
              != IDE_SUCCESS);

    // column locate ����
    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sWNST->aggrNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sWNST->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Data Offset ����
    //----------------------------------

    // mtrNodeOffset ����
    sWNST->mtrNodeOffset = sDataNodeOffset;

    // ���� ��尡 ����� ���� ����
    sCurOffset = sDataNodeOffset +
        idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    // distNodeOffset
    sWNST->distNodeOffset = sCurOffset;
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdDistNode)) * sDistNodeCount;

    // aggrNodeOffset
    sWNST->aggrNodeOffset = sCurOffset;
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdAggrNode)) * sAggrNodeCount;

    // sortNodeOffset
    // Pointer�� ���� ���� + sort node�� ���� ����
    sWNST->sortNodeOffset = sCurOffset;
    sCurOffset +=
        ( idlOS::align8(ID_SIZEOF(qmdMtrNode*)) * sAnalFuncListPtrCount ) +
        ( idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sSortNodeCount );

    // wndNodeOffset
    // wndNodeOffset + partition By Node offset + aggr node offset +
    // aggr result node offset
    sWNST->wndNodeOffset = sCurOffset;
    sCurOffset +=
        ( idlOS::align8(ID_SIZEOF(qmdWndNode*)) * sAnalFuncListPtrCount ) +
        ( idlOS::align8(ID_SIZEOF(qmdWndNode)) * sWndNodeCount ) +
        ( idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sWndOverColumnNodeCount ) +
        ( idlOS::align8(ID_SIZEOF(qmdAggrNode)) * sWndAggrNodeCount ) +
        ( idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sWndAggrResultNodeCount );

    // Sort Manager Offset
    if ( (aFlag & QMO_MAKEWNST_TEMP_TABLE_MASK) ==
         QMO_MAKEWNST_MEMORY_TEMP_TABLE )
    {
        sWNST->sortMgrOffset = sCurOffset;
        sCurOffset += idlOS::align8(ID_SIZEOF(qmcdSortTemp));
    }
    else
    {
        sWNST->sortMgrOffset = sCurOffset;
        sCurOffset +=
            idlOS::align8(ID_SIZEOF(qmcdSortTemp)) * sAnalFuncListPtrCount;
    }

    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sCurOffset;

    //----------------------------------
    // dependency ó�� �� subquery ó��
    //----------------------------------
    sMtrNode[0] = sWNST->myNode;

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sWNST->plan,
                                            QMO_WNST_DEPENDENCY,
                                            sTupleID,
                                            NULL, // Target
                                            0,
                                            NULL, // Predicate
                                            1,
                                            sMtrNode )
              != IDE_SUCCESS );

    // BUG-27526
    IDE_TEST_RAISE( sWNST->plan.dependency == ID_UINT_MAX,
                       ERR_INVALID_DEPENDENCY );

    sWNST->depTupleRowID = (UShort)sWNST->plan.dependency;

    //----------------------------------
    // aggrNode�� dst�� aggrResultNode�� src�� ����
    //----------------------------------
    sCurAnalResultFuncMtrNode = sFirstAnalResultFuncMtrNode;
    for ( sCurAggrNode = sWNST->aggrNode;
          sCurAggrNode != NULL;
          sCurAggrNode = sCurAggrNode->next )
    {
        idlOS::memcpy( sCurAnalResultFuncMtrNode->srcNode,
                       sCurAggrNode->dstNode,
                       ID_SIZEOF(qtcNode) );

        sCurAnalResultFuncMtrNode = sCurAnalResultFuncMtrNode->next;
    }

    //----------------------------------
    // aggrResultNode�� dst�� analytic function node�� ����
    // ( temp table�� ����� ���� target �Ǵ� order by���� ������ �ְ�
    //   �ϱ� ���� )
    //----------------------------------

    sCurAnalResultFuncMtrNode = sFirstAnalResultFuncMtrNode;

    for ( i = 0; i < sAnalFuncListPtrCount; i++ )
    {
        for( sCurAnalyticFunc  = aAnalFuncListPtrArr[i];
             sCurAnalyticFunc != NULL ;
             sCurAnalyticFunc  = sCurAnalyticFunc->next)
         {
             sNextNode = sCurAnalyticFunc->analyticFuncNode->node.next;
             sConversion = sCurAnalyticFunc->analyticFuncNode->node.conversion;

             // aggrResultNode�� dst ����
             idlOS::memcpy( sCurAnalyticFunc->analyticFuncNode,
                            sCurAnalResultFuncMtrNode->dstNode,
                            ID_SIZEOF(qtcNode) );

             // BUG-21912 : aggregation ����� ���� conversion��
             //             �ٽ� �������־�� ��
             sCurAnalyticFunc->analyticFuncNode->node.conversion =
                 sConversion;

             sCurAnalyticFunc->analyticFuncNode->node.next = sNextNode;

             sCurAnalResultFuncMtrNode = sCurAnalResultFuncMtrNode->next;
         }
    }

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sWNST->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sWNST->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sWNST->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sWNST->planID,
                               sWNST->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sWNST->myNode,
                               & sWNST->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MTR_NODE_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeWNST",
                                  "Materialized node error" ));
    }
    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeWNST",
                                  "Invalid dependency" ));
    }
    IDE_EXCEPTION( ERR_INVALID_ORDERBY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_INVALID_POSITION_IN_ORDERBY ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOneMtrPlan::makeFilterINSubquery( qcStatement  * aStatement ,
                                     qmsQuerySet  * aQuerySet ,
                                     UShort         aTupleID,
                                     qtcNode      * aInFilter ,
                                     qtcNode     ** aChangedFilter )
{
/***********************************************************************
 *
 * Description : IN subquery���� �ʿ��� filter�� �籸�����ش�.
 *
 * Implementation :
 *     - IN��  "="��
 *     - LIST�� ���� element�� ���ؼ� "=" ������ �����ֵ����Ѵ�.
 *       ��� ������ �ٽ� AND�� ������ �Ѵ�.
 *     - passNode�� target���� ���� �˾Ƴ���. HASH��忡�� �̸� ����
 *
 * ex)
 *      IN                                     =
 *      |                                      |
 *      i1  -  sub                            i1  -  passNode
 *
 *                        ------->
 *      IN                                    AND
 *      |                                      |
 *     LIST -  sub                             =    -    =
 *      |                                      |         |
 *    (i1,i2)                                 i1 - p    i2 - p
 *
 ***********************************************************************/

    qcNamePosition      sNullPosition;

    qtcNode           * sStartNode;
    qtcNode           * sNewNode[2];
    qtcNode           * sLastNode = NULL;
    qtcNode           * sFirstNode = NULL;
    qtcNode           * sArgNode;

    qtcNode           * sArg1;
    qtcNode           * sArg2;

    qmsTarget         * sTarget;

    extern mtfModule    mtfList;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeFilterINSubquery::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    IDE_TEST_RAISE( aInFilter == NULL, ERR_EMPTY_FILTER );

    // To Fix PR-8109
    // ��� Key Range�� DNF�� �����Ǿ� �־�� ��.
    IDE_TEST_RAISE( ( aInFilter->node.lflag & MTC_NODE_OPERATOR_MASK )
                    != MTC_NODE_OPERATOR_OR,
                    ERR_INVALID_FILTER );

    IDE_TEST_RAISE( aInFilter->node.arguments == NULL,
                    ERR_INVALID_FILTER );
    IDE_TEST_RAISE( ( aInFilter->node.arguments->lflag & MTC_NODE_OPERATOR_MASK )
                    != MTC_NODE_OPERATOR_AND,
                    ERR_INVALID_FILTER );

    // PROJ-1473
    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       aInFilter,
                                       aTupleID,
                                       ID_TRUE )
              != IDE_SUCCESS );

    //----------------------------------
    // Filter�� ó��
    // - IN , NOT IN�� ������ List���
    //     - ����Ʈ�� ������ŭ  , (= , !=)�� �����ϰ�
    //       passNode�� �������� , AND�� �����ش�.
    //
    // - LIST�� �ƴ϶��
    //     - (= , !=)�� �����ϰ�, passNode�� ����
    //
    // - passNode�� HASH��忡�� ������ Target�� �̸� ����
    //----------------------------------

    // To Fix PR-8109
    // IN Node�� ã�´�.
    IDE_FT_ASSERT( aInFilter->node.arguments != NULL );

    sStartNode   = (qtcNode *) aInFilter->node.arguments->arguments;

    //������ ����� argumnent�� LIST���� �ƴ����� �Ǻ�
    sArgNode = (qtcNode*)sStartNode->node.arguments;
    if ( sArgNode->node.module == &mtfList )
    {
        //List�� ���
        sArgNode = (qtcNode*)sArgNode->node.arguments;
    }
    else
    {
        //List�� �ƴѰ��
        //nothing to do
    }

    //Target�� �������־�� �� passNode�� �ֱ� �����̴�.
    for ( sTarget   = aQuerySet->target ;
          sArgNode != NULL && sTarget != NULL ;
          sArgNode  = (qtcNode*)sArgNode->node.next , sTarget = sTarget->next )
    {
        // "=" operator�� �������ش�.
        SET_EMPTY_POSITION( sNullPosition );
        IDE_TEST( qtc::makeNode( aStatement ,
                                 sNewNode ,
                                 &sNullPosition ,
                                 (const UChar*)"=" ,
                                 1 )
                  != IDE_SUCCESS );

        sNewNode[0]->indexArgument = 1;

        // Argument ����
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                 (void **) & sArg1 )
                  != IDE_SUCCESS );
        idlOS::memcpy( sArg1, sArgNode, ID_SIZEOF(qtcNode) );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                 (void **) & sArg2 )
                  != IDE_SUCCESS );
        idlOS::memcpy( sArg2, sTarget->targetColumn, ID_SIZEOF(qtcNode) );

        // ���� ���� ����
        sNewNode[0]->node.arguments = (mtcNode*)sArg1;
        sArg1->node.next = (mtcNode*) sArg2;
        sArg2->node.next = NULL;

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sNewNode[0] )
                  != IDE_SUCCESS );

        //connect
        if( sFirstNode == NULL )
        {
            sFirstNode = sNewNode[0];
            sLastNode = sNewNode[0];
        }
        else
        {
            sLastNode->node.next = (mtcNode*)sNewNode[0];
            sLastNode = sNewNode[0];
        }
    }

    //1�� �̻��� ������ ��尡 �����ɼ��� �����Ƿ� AND�� �������ش�.
    SET_EMPTY_POSITION( sNullPosition );
    IDE_TEST( qtc::makeNode( aStatement,
                             sNewNode,
                             & sNullPosition,
                             (const UChar*)"AND",
                             3 )
              != IDE_SUCCESS );

    sNewNode[0]->node.arguments = (mtcNode*)sFirstNode;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sNewNode[0] )
              != IDE_SUCCESS );

    // To Fix PR-8109
    // DNF�� �����ϱ� ���Ͽ� OR ��带 �����Ͽ� �����Ѵ�.
    sFirstNode = sNewNode[0];
    IDE_TEST( qtc::makeNode( aStatement,
                             sNewNode,
                             & sNullPosition,
                             (const UChar*)"OR",
                             2 )
              != IDE_SUCCESS );

    sNewNode[0]->node.arguments = (mtcNode*)sFirstNode;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sNewNode[0] )
              != IDE_SUCCESS );

    *aChangedFilter = sNewNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EMPTY_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeFilterINSubquery",
                                  "Filter is empty" ));
    }
    IDE_EXCEPTION( ERR_INVALID_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeFilterINSubquery",
                                  "Filter is invalid" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeFilterConstFromPred( qcStatement  * aStatement ,
                                        qmsQuerySet  * aQuerySet,
                                        UShort         aTupleID,
                                        qtcNode      * aFilter ,
                                        qtcNode     ** aFilterConst )
{
/***********************************************************************
 *
 * Description : �־��� filter�� ���� filterConst�� �����Ѵ�.
 *
 * Implementation :
 *
 *   To Fix PR-8109
 *
 *   Join Key Range�� �Ʒ��� ���� �׻� DNF�� �����ȴ�.
 *
 *    OR
 *    |
 *    AND
 *    |
 *    =      ->       =
 *
 ***********************************************************************/

    qtcNode           * sOperatorNode;

    qtcNode           * sNewNode;
    qtcNode           * sLastNode = NULL;
    qtcNode           * sFirstNode = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeFilterConstFromPred::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_TEST_RAISE( aFilter == NULL, ERR_EMPTY_FILTER );

    // To Fix PR-8109
    // ��� Key Range�� DNF�� �����Ǿ� �־�� ��.

    IDE_TEST_RAISE( ( aFilter->node.lflag & MTC_NODE_OPERATOR_MASK )
                    != MTC_NODE_OPERATOR_OR,
                    ERR_INVALID_FILTER );

    IDE_TEST_RAISE( aFilter->node.arguments == NULL,
                    ERR_INVALID_FILTER );
    IDE_TEST_RAISE( ( aFilter->node.arguments->lflag & MTC_NODE_OPERATOR_MASK )
                    != MTC_NODE_OPERATOR_AND,
                    ERR_INVALID_FILTER );

    // To Fix PR-8242
    // DNF�� ������ Join Key Range��
    // AND ��尡 �ϳ����� ������ �� �ִ�.
    IDE_TEST_RAISE( aFilter->node.arguments->next != NULL,
                    ERR_INVALID_FILTER );

    //----------------------------------
    // AND �����κ��� �� �����ڿ� ���Ͽ� �÷� ����
    //----------------------------------

    for ( sOperatorNode  = (qtcNode *) aFilter->node.arguments->arguments;
          sOperatorNode != NULL ;
          sOperatorNode  = (qtcNode*) sOperatorNode->node.next )
    {
        IDE_TEST( makeFilterConstFromNode( aStatement ,
                                           aQuerySet,
                                           aTupleID,
                                           sOperatorNode ,
                                           & sNewNode )
                  != IDE_SUCCESS );

        sNewNode->node.next = NULL;

        //connect
        if( sFirstNode == NULL )
        {
            sFirstNode = sNewNode;
            sLastNode = sNewNode;
        }
        else
        {
            sLastNode->node.next = (mtcNode*)sNewNode;
            sLastNode = sNewNode;
        }
    }

    *aFilterConst = sFirstNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EMPTY_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeFilterConstFromPred",
                                  "Filter is empty" ));
    }
    IDE_EXCEPTION( ERR_INVALID_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeFilterConstFromPred",
                                  "Filter is invalid" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeFilterConstFromNode( qcStatement  * aStatement ,
                                        qmsQuerySet  * aQuerySet,
                                        UShort         aTupleID,
                                        qtcNode      * aOperatorNode ,
                                        qtcNode     ** aNewNode )
{
/***********************************************************************
 *
 * Description : �־��� Operator ���� ���� �÷��� ã�� filterConst�� ��
 *               ��带 �����ؼ� �ش�
 *
 * Implementation :
 *
 *     - indexArgument�� �ش��ϴ� ��带 �����ؾ� �� ���
 *
 *       = (indexArgument = 1�ΰ��)
 *       |
 *       i1    -    i2
 *      (����)
 *
 ***********************************************************************/

    qtcNode           * sNode;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeFilterConstFromNode::__FT__" );

    //----------------------------------
    // indexArgument�� �ش����� �ʴ� �÷��� �����Ѵ�
    //----------------------------------
    if( aOperatorNode->indexArgument == 0 )
    {
        sNode = (qtcNode*)aOperatorNode->node.arguments->next;
    }
    else
    {
        sNode = (qtcNode*)aOperatorNode->node.arguments;
    }

    //----------------------------------
    // sNode�� �ش� �ϴ� �÷��� �����ؼ� ����Ѵ�.
    //----------------------------------
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qtcNode,
                            aNewNode )
              != IDE_SUCCESS);

    // validation�� ������ ��ġ������ �����ؾ� �� ���ο� ��ġ������ �����Ѵ�.
    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sNode,
                                       aTupleID,
                                       ID_FALSE )
                 != IDE_SUCCESS );

    idlOS::memcpy( *aNewNode , sNode , ID_SIZEOF(qtcNode) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::existSameWndNode( qcStatement    * aStatement,
                                 UShort           aTupleID,
                                 qmcWndNode     * aWndNode,
                                 qtcNode        * aAnalticFuncNode,
                                 qmcWndNode    ** aSameWndNode )
{
/***********************************************************************
 *
 * Description : partition by�� ������ wnd node�� �����ϴ��� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcWndNode        * sCurWndNode;
    qmcMtrNode        * sCurOverColumnNode;
    qtcOverColumn     * sCurOverColumn;
    qtcWindow         * sWindow;
    UShort              sTable;
    UShort              sColumn;
    idBool              sExistSameWndNode;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::existSameWndNode::__FT__" );

    sWindow = aAnalticFuncNode->overClause->window;

     // ���� Partition By Column�� ������ Wnd Node�� ã��
    for ( sCurWndNode = aWndNode;
          sCurWndNode != NULL;
          sCurWndNode = sCurWndNode->next )
    {
        /* PROJ-1804 Lag, Lead���� Function �� ���� �߰� */
        if ( ( aAnalticFuncNode->node.module == &mtfLag ) ||
             ( aAnalticFuncNode->node.module == &mtfLagIgnoreNulls ) ||
             ( aAnalticFuncNode->node.module == &mtfLead ) ||
             ( aAnalticFuncNode->node.module == &mtfLeadIgnoreNulls ) ||
             ( aAnalticFuncNode->node.module == &mtfNtile ) )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
        /* PROJ-1805 window clause�� �ϴ� ������ ���� �߰� */
        if ( sWindow != NULL )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }
        sExistSameWndNode = ID_TRUE;
        for ( sCurOverColumnNode = aWndNode->overColumnNode,
                  sCurOverColumn = aAnalticFuncNode->overClause->overColumn;
              ( sCurOverColumnNode != NULL ) &&
                  ( sCurOverColumn != NULL );
              sCurOverColumnNode = sCurOverColumnNode->next,
                  sCurOverColumn = sCurOverColumn->next )
        {
            // Į���� ���� (table, column) ������ ã��
            IDE_TEST( qmg::findColumnLocate( aStatement,
                                             aTupleID,
                                             sCurOverColumn->node->node.table,
                                             sCurOverColumn->node->node.column,
                                             & sTable,
                                             & sColumn )
                      != IDE_SUCCESS );

            if ( ( sCurOverColumnNode->srcNode->node.table == sTable )
                 &&
                 ( sCurOverColumnNode->srcNode->node.column == sColumn ) )
            {
                // BUG-33663 Ranking Function
                // partition by column���� ���ϰ� order by �÷����� ���Ѵ�.
                if ( (sCurOverColumnNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                     == QMC_MTR_SORT_ORDER_FIXED_TRUE )
                {
                    if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                         == QTC_OVER_COLUMN_ORDER_BY )
                    {
                        if ( ( (sCurOverColumnNode->flag & QMC_MTR_SORT_ORDER_MASK)
                               == QMC_MTR_SORT_ASCENDING )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_ASC ) )
                        {
                            // ������ ���, ���� Į���� �������� ��� �˻�
                        }
                        else
                        {
                            sExistSameWndNode = ID_FALSE;
                            break;
                        }

                        if ( ( (sCurOverColumnNode->flag & QMC_MTR_SORT_ORDER_MASK)
                               == QMC_MTR_SORT_DESCENDING )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_DESC ) )
                        {
                            // ������ ���, ���� Į���� �������� ��� �˻�
                        }
                        else
                        {
                            sExistSameWndNode = ID_FALSE;
                            break;
                        }
                    }
                    else
                    {
                        sExistSameWndNode = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                         == QTC_OVER_COLUMN_NORMAL )
                    {
                        // ������ ���, ���� Į���� �������� ��� �˻�
                    }
                    else
                    {
                        sExistSameWndNode = ID_FALSE;
                        break;
                    }
                }
            }
            else
            {
                sExistSameWndNode = ID_FALSE;
                break;
            }
        }

        if ( sExistSameWndNode == ID_TRUE )
        {
            if ( ( sCurOverColumnNode == NULL ) && ( sCurOverColumn == NULL ) )
            {
                *aSameWndNode = sCurWndNode;
                break;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::addAggrNodeToWndNode( qcStatement       * aStatement,
                                     qmsQuerySet       * aQuerySet,
                                     qmsAnalyticFunc   * aAnalyticFunc,
                                     UShort              aAggrTupleID,
                                     UShort            * aAggrColumnCount,
                                     qmcWndNode        * aWndNode,
                                     qmcMtrNode       ** aNewAggrNode )
{
/***********************************************************************
 *
 * Description : wndNode�� aggrNode�� �߰���
 *
 * Implementation :
 *
 *    ex ) SELECT SUM(i1) OVER ( PARTITION BY i2 ) FROM t1;
 *         �� �����϶� �Ʒ� �׸����� aggrNode�� �߰��ϴ� �Լ�
 *
 *     myNode-->(baseTable/baseColumn)->(i1)->(i2)->(sum(i1))
 *
 *               +---------------+
 *     wndNode-->| WndNode       |
 *               +---------------+
 *               | overColumnNode|-->(i2)
 *               | aggrNode      |-->(sum(i1))
 *               | aggrResultNode|
 *               | next          |
 *               +---------------+
 *
 ***********************************************************************/

    qtcNode    * sSrcNode;
    qmcMtrNode * sNewMtrNode;
    qmcMtrNode * sLastAggrNode;
    mtcNode    * sArgs;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::addAggrNodeToWndNode::__FT__" );

    //----------------------------
    // aggrNode�� src ����
    //----------------------------

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                               (void**)&( sSrcNode ) )
              != IDE_SUCCESS );

    // analytic function ���� ����
    *(sSrcNode) = *(aAnalyticFunc->analyticFuncNode);

    //----------------------------
    // aggrNode ���� �� dst ����
    //----------------------------

    sNewMtrNode = NULL;

    // PROJ-2179
    // PROJ-2527 WITHIN GROUP AGGR
    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       (qtcNode*)sSrcNode->node.arguments,
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                      aQuerySet,
                                      sSrcNode,
                                      ID_FALSE,
                                      aAggrTupleID,
                                      0,
                                      aAggrColumnCount,
                                      & sNewMtrNode )
              != IDE_SUCCESS );

    // PROJ-2179
    // Aggregate function�� ���ڵ��� ��ġ�� ���̻� ����Ǿ�� �ȵȴ�.
    for( sArgs = sSrcNode->node.arguments;
         sArgs != NULL;
         sArgs = sArgs->next )
    {
        sArgs->lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
        sArgs->lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE;
    }

    // ������ �����Ǿ�� ��
    IDE_DASSERT( sNewMtrNode != NULL );

    //----------------------------
    // ����
    //----------------------------

    if ( aWndNode->aggrNode == NULL )
    {
        aWndNode->aggrNode = sNewMtrNode;
    }
    else
    {
        for ( sLastAggrNode = aWndNode->aggrNode;
              sLastAggrNode->next != NULL;
              sLastAggrNode = sLastAggrNode->next ) ;

        sLastAggrNode->next = sNewMtrNode;
    }

    *aNewAggrNode   = sNewMtrNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::addAggrResultNodeToWndNode( qcStatement * aStatement,
                                           qmcMtrNode  * aAnalResultFuncMtrNode,
                                           qmcWndNode  * aWndNode )
{
/***********************************************************************
 *
 * Description : wndNode�� resultAggrNode�� �߰���
 *
 * Implementation :
 *
 *    ex ) SELECT SUM(i1) OVER ( PARTITION BY i2 ) FROM t1;
 *         �� �����϶� �Ʒ� �׸�����
 *         wndNode�� aggrResultNode�� �߰��ϴ� �Լ�
 *         wndNode�� aggrResultNode�� wndNode�� aggrNode�� �����
 *         ���������� ����Ǵ� ���� ���� �����̸�
 *         myNode�� aggrResultNode�� �����ϴ�.
 *
 *     myNode-->(baseTable/baseColumn)->(i1)->(i2)->(sum(i1))
 *                                                     |
 *               +---------------+                     |
 *     wndNode-->| wndNode       |                     |
 *               +---------------+                     |
 *               | overColumnNode|-->(i2)              |
 *               | aggrNode      |-->(sum(i1))         |
 *               | aggrResultNode|-->(sum(i1)) --------+ ���� ����
 *               | next          |
 *               +---------------+
 *
 ***********************************************************************/

    qmcMtrNode * sNewMtrNode;
    qmcMtrNode * sLastAggrResultNode;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::addAggrResultNodeToWndNode::__FT__" );

    // materialize node ����
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmcMtrNode ),
                                               (void**)& sNewMtrNode )
              != IDE_SUCCESS);

    // result function node ����
    *sNewMtrNode = *aAnalResultFuncMtrNode;
    sNewMtrNode->next = NULL;

    if ( aWndNode->aggrResultNode == NULL )
    {
        aWndNode->aggrResultNode = sNewMtrNode;
    }
    else
    {
        for ( sLastAggrResultNode = aWndNode->aggrResultNode;
              sLastAggrResultNode->next != NULL;
              sLastAggrResultNode = sLastAggrResultNode->next ) ;
        sLastAggrResultNode->next = sNewMtrNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeWndNode( qcStatement       * aStatement,
                            UShort              aTupleID,
                            qmcMtrNode        * aMtrNode,
                            qtcNode           * aAnalticFuncNode,
                            UInt              * aOverColumnNodeCount,
                            qmcWndNode       ** aNewWndNode )
{
/***********************************************************************
 *
 * Description : wnd node�� aggr node�� �߰���
 *
 * Implementation :
 *    ex ) SELECT SUM(i1) OVER ( PARTITION BY i2 ORDER BY i3 ) FROM t1;
 *         �� �����϶� �Ʒ� �׸����� wndNode�� �����ϴ� �Լ�
 *
 *     myNode-->(baseTable/baseColumn)->(i1)->(i2)->(i3 asc)-(sum(i1))
 *
 *               +---------------+
 *     wndNode-->| wndNode       |
 *               +---------------+
 *               | overColumnNode|-->(i2)-(i3 asc)
 *               | aggrNode      |
 *               | aggrResultNode|
 *               | next          |
 *               +---------------+
 *
 *
 ***********************************************************************/

    qtcOverColumn     * sCurOverColumn;
    qmcMtrNode        * sCurMtrNode;
    qmcWndNode        * sNewWndNode;
    qmcMtrNode        * sSameMtrNode;
    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sFirstOverColumnNode;
    qmcMtrNode        * sLastOverColumnNode;
    qtcWindow         * sWindow;
    mtcNode           * sNode;
    UShort              sTable;
    UShort              sColumn;
    idBool              sExistPartitionByColumn;
    idBool              sExistOrderByColumn;
    qmcWndExecMethod    sExecMethod;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeWndNode::__FT__" );

    // �ʱ�ȭ
    sSameMtrNode            = NULL;
    sFirstOverColumnNode    = NULL;
    sLastOverColumnNode     = NULL;
    sExistPartitionByColumn = ID_FALSE;
    sExistOrderByColumn     = ID_FALSE;
    sExecMethod             = QMC_WND_EXEC_NONE;

    //-----------------------------
    // Wnd Node ����
    //-----------------------------

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmcWndNode ),
                                               (void**) & sNewWndNode )
              != IDE_SUCCESS );

    sWindow = aAnalticFuncNode->overClause->window;
    //-----------------------------
    // overColumnNode ����
    //-----------------------------

    for ( sCurOverColumn  = aAnalticFuncNode->overClause->overColumn;
          sCurOverColumn != NULL;
          sCurOverColumn  = sCurOverColumn->next )
    {
        *aOverColumnNodeCount = *aOverColumnNodeCount + 1;

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmcMtrNode ),
                                                   (void**)& sNewMtrNode )
                  != IDE_SUCCESS);

        // BUG-34966 Pass node �� �� �����Ƿ� ���� ���� ��ġ�� �����Ѵ�.
        sNode = &sCurOverColumn->node->node;

        while( sNode->module == &qtc::passModule )
        {
            sNode = sNode->arguments;
        }

        // Į���� ���� (table, column) ������ ã��
        IDE_TEST( qmg::findColumnLocate( aStatement,
                                         aTupleID,
                                         sNode->table,
                                         sNode->column,
                                         & sTable,
                                         & sColumn )
                  != IDE_SUCCESS );

        for ( sCurMtrNode  = aMtrNode;
              sCurMtrNode != NULL;
              sCurMtrNode  = sCurMtrNode->next )
        {
            if ( ( ( sCurMtrNode->srcNode->node.table == sTable )
                   &&
                   ( sCurMtrNode->srcNode->node.column == sColumn ) ) ||
                ( ( sCurMtrNode->dstNode->node.table == sTable )
                   &&
                   ( sCurMtrNode->dstNode->node.column == sColumn ) ) )
            {
                if ( (sCurMtrNode->flag & QMC_MTR_BASETABLE_MASK)
                     != QMC_MTR_BASETABLE_TRUE )
                {
                    // table�� ǥ���ϱ� ���� column�� �ƴ� ���

                    // BUG-33663 Ranking Function
                    // mtr node�� partition column���� order column���� ����
                    if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                         == QTC_OVER_COLUMN_ORDER_BY )
                    {
                        sExistOrderByColumn = ID_TRUE;

                        if ( (sCurMtrNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                             == QMC_MTR_SORT_ORDER_FIXED_TRUE )
                        {
                            if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                                   == QTC_OVER_COLUMN_ORDER_ASC )
                                 &&
                                 ( (sCurMtrNode->flag & QMC_MTR_SORT_ORDER_MASK)
                                   == QMC_MTR_SORT_ASCENDING ) )
                            {
                                // BUG-42145 Nulls Option �� �ٸ� ��쵵
                                // üũ�ؾ��Ѵ�.
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_NONE ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_FIRST ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_LAST ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                            }
                            else
                            {
                                // Noting to do.
                            }

                            if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                                   == QTC_OVER_COLUMN_ORDER_DESC )
                                 &&
                                 ( (sCurMtrNode->flag & QMC_MTR_SORT_ORDER_MASK)
                                   == QMC_MTR_SORT_DESCENDING ) )
                            {
                                // BUG-42145 Nulls Option �� �ٸ� ��쵵
                                // üũ�ؾ��Ѵ�.
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_NONE ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_FIRST ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                                if ( ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                       == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) &&
                                     ( ( sCurMtrNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                       == QMC_MTR_SORT_NULLS_ORDER_LAST ) )
                                {
                                    sSameMtrNode = sCurMtrNode;
                                    break;
                                }
                                else
                                {
                                    /* Nothing to do */
                                }
                            }
                            else
                            {
                                // Noting to do.
                            }
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        sExistPartitionByColumn = ID_TRUE;

                        if ( (sCurMtrNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                             == QMC_MTR_SORT_ORDER_FIXED_FALSE )
                        {
                            sSameMtrNode = sCurMtrNode;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
                else
                {
                    // table�� ǥ���ϱ� ���� column�� ���
                    // �ٸ� Į���ӿ��� �ұ��ϰ� ���� �� ����
                }
            }
            else
            {
                // nothing to
            }
        }

        // Analytic Function�� ���� column�� materialize node��
        // �̹� qmg::makeColumn4Analytic()�� ���� �߰���
        IDE_TEST_RAISE( sSameMtrNode == NULL, ERR_INVALID_NODE );

        // mtr��� ����
        idlOS::memcpy( sNewMtrNode,
                       sSameMtrNode,
                       ID_SIZEOF(qmcMtrNode) );

        sNewMtrNode->next = NULL;

        if ( sFirstOverColumnNode == NULL )
        {
            sFirstOverColumnNode = sNewMtrNode;
            sLastOverColumnNode = sNewMtrNode;
        }
        else
        {
            sLastOverColumnNode->next = sNewMtrNode;
            sLastOverColumnNode       = sLastOverColumnNode->next;
        }
    }

    // BUG-33663 Ranking Function
    // window node�� ���۹���� �����Ѵ�.
    if ( (sExistPartitionByColumn == ID_TRUE) && (sExistOrderByColumn == ID_TRUE) )
    {
        if ( sWindow == NULL )
        {
            if ( ( aAnalticFuncNode->node.module == &mtfLag ) ||
                 ( aAnalticFuncNode->node.module == &mtfLagIgnoreNulls ) )
            {
                sExecMethod = QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LAG;
            }
            else if ( ( aAnalticFuncNode->node.module == &mtfLead ) ||
                      ( aAnalticFuncNode->node.module == &mtfLeadIgnoreNulls ) )
            {
                sExecMethod = QMC_WND_EXEC_PARTITION_ORDER_UPDATE_LEAD;
            }
            /* BUG43086 support ntile analytic function */
            else if ( aAnalticFuncNode->node.module == &mtfNtile )
            {
                sExecMethod = QMC_WND_EXEC_PARTITION_ORDER_UPDATE_NTILE;
            }
            else
            {
                // partition & order by aggregation and update
                sExecMethod = QMC_WND_EXEC_PARTITION_ORDER_UPDATE;
            }
        }
        else
        {
            sExecMethod = QMC_WND_EXEC_PARTITION_ORDER_WINDOW_UPDATE;
        }
    }
    else
    {
        if ( sExistPartitionByColumn == ID_TRUE )
        {
            // partition by aggregation and update
            sExecMethod = QMC_WND_EXEC_PARTITION_UPDATE;
        }
        else
        {
            if ( sExistOrderByColumn == ID_TRUE )
            {
                if ( sWindow == NULL )
                {
                    if ( ( aAnalticFuncNode->node.module == &mtfLag ) ||
                         ( aAnalticFuncNode->node.module == &mtfLagIgnoreNulls ) )
                    {
                        sExecMethod = QMC_WND_EXEC_ORDER_UPDATE_LAG;
                    }
                    else if ( ( aAnalticFuncNode->node.module == &mtfLead ) ||
                              ( aAnalticFuncNode->node.module == &mtfLeadIgnoreNulls ) )
                    {
                        sExecMethod = QMC_WND_EXEC_ORDER_UPDATE_LEAD;
                    }
                    /* BUG43086 support ntile analytic function */
                    else if ( aAnalticFuncNode->node.module == &mtfNtile )
                    {
                        sExecMethod = QMC_WND_EXEC_ORDER_UPDATE_NTILE;
                    }
                    else
                    {
                        // order by aggregation and update
                        sExecMethod = QMC_WND_EXEC_ORDER_UPDATE;
                    }
                }
                else
                {
                    sExecMethod = QMC_WND_EXEC_ORDER_WINDOW_UPDATE;
                }
            }
            else
            {
                // aggregation and update
                sExecMethod = QMC_WND_EXEC_AGGR_UPDATE;
            }
        }
    }

    if ( sWindow != NULL )
    {
        sNewWndNode->window.rowsOrRange = sWindow->rowsOrRange;
        if ( sWindow->isBetween == ID_TRUE )
        {
            sNewWndNode->window.startOpt = sWindow->start->option;
            if ( sWindow->start->value != NULL )
            {
                sNewWndNode->window.startValue = *sWindow->start->value;
            }
            else
            {
                sNewWndNode->window.startValue.number = 0;
                sNewWndNode->window.startValue.type   = QTC_OVER_WINDOW_VALUE_TYPE_NUMBER;
            }

            sNewWndNode->window.endOpt = sWindow->end->option;
            if ( sWindow->end->value != NULL )
            {
                sNewWndNode->window.endValue = *sWindow->end->value;
            }
            else
            {
                sNewWndNode->window.endValue.number = 0;
                sNewWndNode->window.endValue.type   = QTC_OVER_WINDOW_VALUE_TYPE_NUMBER;
            }
        }
        else
        {
            sNewWndNode->window.startOpt = sWindow->start->option;
            if ( sWindow->start->value != NULL )
            {
                sNewWndNode->window.startValue = *sWindow->start->value;
            }
            else
            {
                sNewWndNode->window.startValue.number = 0;
                sNewWndNode->window.startValue.type   = QTC_OVER_WINDOW_VALUE_TYPE_NUMBER;
            }
            sNewWndNode->window.endOpt = QTC_OVER_WINODW_OPT_CURRENT_ROW;
            sNewWndNode->window.endValue.number = 0;
            sNewWndNode->window.endValue.type   = QTC_OVER_WINDOW_VALUE_TYPE_NUMBER;
        }
    }
    else
    {
        sNewWndNode->window.rowsOrRange = QTC_OVER_WINDOW_NONE;
    }

    sNewWndNode->overColumnNode = sFirstOverColumnNode;
    sNewWndNode->aggrNode       = NULL;
    sNewWndNode->aggrResultNode = NULL;
    sNewWndNode->execMethod     = sExecMethod;
    sNewWndNode->next           = NULL;

    *aNewWndNode = sNewWndNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeWndNode",
                                  "Invalid node" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeMyNodeOfWNST( qcStatement      * aStatement,
                                 qmsQuerySet      * aQuerySet,
                                 qmnPlan          * aPlan,
                                 qmsAnalyticFunc ** aAnalFuncListPtrArr,
                                 UInt               aAnalFuncListPtrCnt,
                                 UShort             aTupleID,
                                 qmncWNST         * aWNST,
                                 UShort           * aColumnCountOfMyNode,
                                 qmcMtrNode      ** aFirstAnalResultFuncMtrNode)
{
/***********************************************************************
 *
 * Description : WNST�� myNode ����
 *
 * Implementation :
 *     WNST�� myNode�� temp table�� ����� Į�� �����ν�
 *     �Ʒ��� ���� Į�� ������� �����ȴ�.
 *
 *     [Base Table] + [Columns] + [Analytic Function]
 *
 *     < output >
 *         aMyNode        : WNST plan�� myNode(materialize node) ����
 *         baseTableCount : myNode�� baseTableCount
 *         aColumnCount   : myNode ��ü Į�� ����
 *         aFirstAnalResultFuncMtrNode : myNode�� analytic function
 *                                       result ����� Į���� �� ù��°
 *
 ***********************************************************************/

    qmcMtrNode      * sMyNode;
    UShort            sColumnCount;
    qmcMtrNode      * sLastMtrNode;
    qmsAnalyticFunc * sCurAnalyticFunc;
    qmcMtrNode      * sFirstAnalResultFuncMtrNode;

    UInt              i;
    qmcMtrNode      * sNewMtrNode = NULL;
    qmcAttrDesc     * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeMyNodeOfWNST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    // �⺻ �ʱ�ȭ
    sFirstAnalResultFuncMtrNode = NULL;
    sMyNode = NULL;

    //-----------------------------------------------
    // 1. Base Table ����
    //-----------------------------------------------

    sColumnCount = 0;
    sLastMtrNode = NULL;

    // Sorting key�� �ƴ� ���
    IDE_TEST( makeNonKeyAttrsMtrNodes( aStatement,
                                       aQuerySet,
                                       aPlan->resultDesc,
                                       aTupleID,
                                       & sMyNode,
                                       & sLastMtrNode,
                                       & sColumnCount )
              != IDE_SUCCESS );

    aWNST->baseTableCount  = sColumnCount;

    //-----------------------------------------------
    // 2. Column���� ����
    //    Analytic Function�� argument column��
    //    Partition By column
    //    ( myNode�� ���� �ߺ��Ǵ� Į���� ���� ��쿡��
    //      last mtr node�� next�� �����Ѵ�. )
    //-----------------------------------------------
    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        // Sorting key �� ���
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              aTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;

            if ( ( sItrAttr->flag & QMC_ATTR_ANALYTIC_SORT_MASK ) == QMC_ATTR_ANALYTIC_SORT_TRUE )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_FIXED_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ORDER_FIXED_TRUE;
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_FIXED_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ORDER_FIXED_FALSE;
            }

            if ( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK )
                  == QMC_ATTR_SORT_ORDER_ASC )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
            }

            /* PROJ-2435 Order by Nulls first/last */
            if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                  == QMC_ATTR_SORT_NULLS_ORDER_NONE )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_NONE;
            }
            else
            {
                if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                      == QMC_ATTR_SORT_NULLS_ORDER_FIRST )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_FIRST;
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_LAST;
                }
            }

            if( sMyNode == NULL )
            {
                sMyNode             = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sNewMtrNode;
                sLastMtrNode        = sNewMtrNode;
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    //-----------------------------------------------
    // 3. Analytic Function ����� ����� Į���� �����Ͽ�
    //    myNode�� ����
    //-----------------------------------------------

    for ( i = 0;
          i < aAnalFuncListPtrCnt;
          i++ )
    {
        for ( sCurAnalyticFunc  = aAnalFuncListPtrArr[i];
              sCurAnalyticFunc != NULL ;
              sCurAnalyticFunc  = sCurAnalyticFunc->next)
         {
             IDE_TEST( qmg::makeAnalFuncResultMtrNode( aStatement,
                                                       sCurAnalyticFunc,
                                                       aTupleID,
                                                       & sColumnCount,
                                                       & sMyNode,
                                                       & sLastMtrNode )
                       != IDE_SUCCESS );

             if ( sFirstAnalResultFuncMtrNode == NULL )
             {
                 sFirstAnalResultFuncMtrNode = sLastMtrNode;
             }
             else
             {
                 // nothing to do
             }
         }
    }

    aWNST->myNode = sMyNode;
    *aColumnCountOfMyNode = sColumnCount;
    *aFirstAnalResultFuncMtrNode = sFirstAnalResultFuncMtrNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::findPriorColumns( qcStatement * aStatement,
                                        qtcNode     * aNode,
                                        qtcNode     * aFirst )
{
    qtcNode   * sNode      = NULL;
    qtcNode   * sPrev      = NULL;
    qtcNode   * sTmp       = NULL;
    idBool      sIsSame    = ID_FALSE;
    mtcColumn * sMtcColumn = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::findPriorColumns::__FT__" );

    if ( ( ( aNode->lflag & QTC_NODE_PRIOR_MASK) ==
           QTC_NODE_PRIOR_EXIST ) &&
           ( aNode->node.module == &qtc::columnModule ))
    {
        sPrev = (qtcNode *)aFirst->node.next;
        for ( sNode  = sPrev;
              sNode != NULL;
              sNode  = (qtcNode *) sNode->node.next )
        {
            if ( sNode->node.column == aNode->node.column )
            {
                sIsSame = ID_TRUE;
                break;
            }
            else
            {
                /* Nothing to do */
            }
            sPrev = sNode;
        }

        if ( sIsSame == ID_FALSE )
        {
            sMtcColumn = QTC_TMPL_COLUMN( QC_SHARED_TMPLATE( aStatement ),
                                          aNode );

            IDE_TEST_RAISE( ( sMtcColumn->module->id == MTD_CLOB_ID ) ||
                            ( sMtcColumn->module->id == MTD_CLOB_LOCATOR_ID ) ||
                            ( sMtcColumn->module->id == MTD_BLOB_ID ) ||
                            ( sMtcColumn->module->id == MTD_BLOB_LOCATOR_ID ),
                            ERR_NOT_SUPPORT_PRIOR_LOB );

            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                    qtcNode,
                                    & sTmp )
                      != IDE_SUCCESS );
            idlOS::memcpy( sTmp, aNode, sizeof( qtcNode ) );
            sTmp->node.arguments = NULL;
            sTmp->node.next      = NULL;

            if ( sPrev == NULL )
            {
                aFirst->node.next = (mtcNode *)sTmp;
            }
            else
            {
                sPrev->node.next = (mtcNode *)sTmp;
            }
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

    if ( aNode->node.arguments != NULL )
    {
        IDE_TEST( findPriorColumns( aStatement,
                                    (qtcNode *)aNode->node.arguments,
                                    aFirst )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aNode->node.next != NULL )
    {
        IDE_TEST( findPriorColumns( aStatement,
                                    (qtcNode *)aNode->node.next,
                                    aFirst )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_PRIOR_LOB )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOW_PRIOR_LOB ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::findPriorPredAndSortNode( qcStatement  * aStatement,
                                                qtcNode      * aNode,
                                                qtcNode     ** aSortNode,
                                                qtcNode     ** aPriorPred,
                                                UShort         aFromTable,
                                                idBool       * aFind )
{
    qtcNode * sOperator = NULL;
    qtcNode * sNode1    = NULL;
    qtcNode * sNode2    = NULL;
    qtcNode * sTmpNode  = NULL;
    qtcNode * sTmpNode1 = NULL;
    qtcNode * sTmpNode2 = NULL;
    UInt      sCount    = 0;
    idBool    sFind     = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::findPriorPredAndSortNode::__FT__" );

    if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK ) ==
         MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sOperator = (qtcNode *)aNode->node.arguments;
        if ( sOperator != NULL && *aFind == ID_FALSE )
        {
            IDE_TEST( findPriorPredAndSortNode( aStatement,
                                                sOperator,
                                                aSortNode,
                                                aPriorPred,
                                                aFromTable,
                                                aFind )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
        sOperator = (qtcNode *)aNode->node.next;

        if ( sOperator != NULL && *aFind == ID_FALSE )
        {
            IDE_TEST( findPriorPredAndSortNode( aStatement,
                                                sOperator,
                                                aSortNode,
                                                aPriorPred,
                                                aFromTable,
                                                aFind )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        sOperator = aNode;
    }

    for ( ; sOperator != NULL ; sOperator = ( qtcNode * )sOperator->node.next )
    {
        sFind = ID_FALSE;
        if ( *aFind == ID_TRUE )
        {
            break;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sOperator->node.lflag & MTC_NODE_INDEX_JOINABLE_MASK ) !=
              MTC_NODE_INDEX_JOINABLE_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        sNode1 = ( qtcNode * )sOperator->node.arguments;

        if( sNode1 != NULL )
        {
            sNode2 = ( qtcNode * )sNode1->node.next;
        }
        else
        {
            sNode2 = NULL;
        }

        if ( (sNode1 != NULL) && (sNode2 != NULL) )
        {
            if ( ( (sNode1->lflag & QTC_NODE_PRIOR_MASK) ==
                   QTC_NODE_PRIOR_EXIST ) &&
                   ( sNode1->node.module == &qtc::columnModule ))
            {
                sCount++;
            }
            else
            {
                *aSortNode = sNode1;
            }
            if ( ( (sNode2->lflag & QTC_NODE_PRIOR_MASK) ==
                   QTC_NODE_PRIOR_EXIST ) &&
                   ( sNode2->node.module == &qtc::columnModule ))
            {
                sCount++;
            }
            else
            {
                *aSortNode = sNode2;
            }

            /*
             * 1. sortNode�� �÷��̾�� �Ѵ�.
             * 2. �÷��� conversion�� ����� �Ѵ�.
             * 3. �÷� dependency�� from table�̾�� �Ѵ�.
             */
            if ( ( sCount == 1 ) &&
                 ( ( *aSortNode )->node.module == &qtc::columnModule ) &&
                 ( ( *aSortNode )->node.conversion == NULL ) &&
                 ( ( *aSortNode )->node.table == aFromTable ) )
            {
                sFind = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }

            if ( sFind == ID_TRUE )
            {
                IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                        qtcNode,
                                        & sTmpNode )
                          != IDE_SUCCESS );
                idlOS::memcpy( sTmpNode, *aSortNode, sizeof( qtcNode ) );
                sTmpNode->node.next = NULL;
                *aSortNode = sTmpNode;

                IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                        qtcNode,
                                        & sTmpNode )
                          != IDE_SUCCESS );
                idlOS::memcpy( sTmpNode, sOperator, sizeof( qtcNode ) );
                IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                        qtcNode,
                                        & sTmpNode1 )
                          != IDE_SUCCESS );
                idlOS::memcpy( sTmpNode1, sNode1, sizeof( qtcNode ) );
                sTmpNode->node.arguments = ( mtcNode * )sTmpNode1;

                IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                        qtcNode,
                                        & sTmpNode2 )
                          != IDE_SUCCESS );
                idlOS::memcpy( sTmpNode2, sNode2, sizeof( qtcNode ) );
                sTmpNode1->node.next = ( mtcNode * )sTmpNode2;
                *aPriorPred          = sTmpNode;
                *aFind               = ID_TRUE;
                break;
            }
            else
            {
                *aSortNode = NULL;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::processStartWithPredicate( qcStatement * aStatement,
                                                 qmsQuerySet * aQuerySet,
                                                 qmncCNBY    * aCNBY,
                                                 qmoLeafInfo * aStartWith )
{
    qmncScanMethod sMethod;
    idBool         sIsSubQuery = ID_FALSE;
    idBool         sFind       = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::processStartWithPredicate::__FT__" );

    /* Start With Predicate �� ó�� */
    IDE_TEST( qmoOneNonPlan::processPredicate( aStatement,
                                               aQuerySet,
                                               aStartWith->predicate,
                                               aStartWith->constantPredicate,
                                               aStartWith->ridPredicate,
                                               aStartWith->index,
                                               aCNBY->myRowID,
                                               & sMethod.constantFilter,
                                               & sMethod.filter,
                                               & sMethod.lobFilter,
                                               & sMethod.subqueryFilter,
                                               & sMethod.varKeyRange,
                                               & sMethod.varKeyFilter,
                                               & sMethod.varKeyRange4Filter,
                                               & sMethod.varKeyFilter4Filter,
                                               & sMethod.fixKeyRange,
                                               & sMethod.fixKeyFilter,
                                               & sMethod.fixKeyRange4Print,
                                               & sMethod.fixKeyFilter4Print,
                                               & sMethod.ridRange,
                                               & sIsSubQuery )
              != IDE_SUCCESS );

    IDE_TEST( qmoOneNonPlan::postProcessScanMethod( aStatement,
                                                    & sMethod,
                                                    & sFind )
              != IDE_SUCCESS );

    if ( sMethod.constantFilter != NULL )
    {
        aCNBY->startWithConstant = sMethod.constantFilter;
    }
    else
    {
        aCNBY->startWithConstant = NULL;
    }

    if ( sMethod.filter != NULL )
    {
        aCNBY->startWithFilter = sMethod.filter;
    }
    else
    {
        aCNBY->startWithFilter = NULL;
    }

    if ( sMethod.subqueryFilter != NULL )
    {
        aCNBY->startWithSubquery = sMethod.subqueryFilter;
    }
    else
    {
        aCNBY->startWithSubquery = NULL;
    }

    if ( aStartWith->nnfFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aStartWith->nnfFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    aCNBY->startWithNNF = aStartWith->nnfFilter;

    /* Hierarchy�� �׻� inline view�� ����� ����ϱ� ������ lobFilter �� ���ü� ���� */
    IDE_TEST_RAISE ( sMethod.lobFilter != NULL, ERR_INTERNAL )

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INTERNAL )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                "qmoOneMtrPlan::processStartWithPredicate",
                                "lobFilter is not NULL"));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Connect By ������ Predicate�� ó���Ѵ�.
 *
 *  1. level Filter�� ó���Ѵ�.
 *  2. processPredicate �� ���ؼ� Filter�� ConstantFilter�� ��´�.
 *     �������� ������ �ʴ´�.
 *  3. ConstantFilter�� �����Ѵ�.
 *  4. Filter�� �ִٸ� Prior �� 1�� ���� ��带 ã�� Prior���� Sort Node�� �����Ѵ�.
 *     �̶� KeryRange ��嵵 ����ȴ�.
 *  5. makeCNBYMtrNode�� ���� baseMTR �� CMTR Materialize�� SortNode�� �����Ѵ�.
 *     �̸� ���ؼ� baseMTR�� Sort�ϰԵȴ�. �̴� ������ 1�� Row�� ���� Order Siblgins By
 *     �� �����ϱ� ���ؼ��̴�.
 *  6. Prior�� 1���� ��带 ã���� �̸� ���� KeyRange�� �����ϰ� ���� Sort Table��
 *     �װԵȴ�. �̸� ���� TupleID�� �Ҵ� �޴´�.
 *  7. Order Siblings by�� ���� Composite MtrNode ����
 *  8. CNF�� ������ Prior SortNode�� DNF�� ���·� ��ȯ
 *
 */
IDE_RC qmoOneMtrPlan::processConnectByPredicate( qcStatement    * aStatement,
                                                 qmsQuerySet    * aQuerySet,
                                                 qmsFrom        * aFrom,
                                                 qmncCNBY       * aCNBY,
                                                 qmoLeafInfo    * aConnectBy )
{
    qmncScanMethod sMethod;
    idBool         sIsSubQuery    = ID_FALSE;
    idBool         sFind          = ID_FALSE;
    qtcNode      * sNode          = NULL;
    qtcNode      * sSortNode      = NULL;
    qtcNode      * sPriorPred     = NULL;
    qtcNode      * sOptimizedNode = NULL;
    qmcMtrNode   * sMtrNode       = NULL;
    qmcMtrNode   * sTmp           = NULL;
    UInt           sCurOffset     = 0;
    qtcNode        sPriorTmp;
    qmoPredicate * sPred          = NULL;
    qmoPredicate * sPredMore      = NULL;

    qmcAttrDesc  * sItrAttr;
    qmcMtrNode   * sFirstMtrNode  = NULL;
    qmcMtrNode   * sLastMtrNode   = NULL;
    UShort         sColumnCount   = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::processConnectByPredicate::__FT__" );

    aCNBY->levelFilter       = NULL;
    aCNBY->rownumFilter      = NULL;
    aCNBY->connectByKeyRange = NULL;
    aCNBY->connectByFilter   = NULL;
    aCNBY->priorNode         = NULL;

    /* 1. Level Filter �� ó�� */
    if ( aConnectBy->levelPredicate != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aConnectBy->levelPredicate,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sNode )
                  != IDE_SUCCESS );

        /* BUG-17921 */
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                                & sOptimizedNode )
                   != IDE_SUCCESS );
        aCNBY->levelFilter = sOptimizedNode;

        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aCNBY->levelFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * 1-2 Rownum Filter �� ó��
     */
    if ( aConnectBy->connectByRownumPred != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aConnectBy->connectByRownumPred,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sNode )
                  != IDE_SUCCESS );

        /* BUG-17921 */
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );
        aCNBY->rownumFilter = sOptimizedNode;

        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aCNBY->rownumFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2641 Hierarchy Query Index
     * Connectby predicate���� prior Node�� ã�Ƽ�
     * Error�� ���� ��Ȳ�� üũ�ؾ��Ѵ�.
     */
    for ( sPred = aConnectBy->predicate; sPred != NULL; sPred = sPred->next )
    {
        for ( sPredMore = sPred;
              sPredMore != NULL;
              sPredMore = sPredMore->more )
        {
            sPriorTmp.node.next = NULL;
            IDE_TEST( findPriorColumns( aStatement,
                                        sPredMore->node,
                                        & sPriorTmp )
                      != IDE_SUCCESS );
            /* BUG-44759 processPredicae�� �����ϱ����� priorNode
             * �� �־���Ѵ�.
             */
            aCNBY->priorNode = (qtcNode *)sPriorTmp.node.next;
        }
    }

    /* 2. Connect By Predicate �� ó�� */
    IDE_TEST( qmoOneNonPlan::processPredicate( aStatement,
                                               aQuerySet,
                                               aConnectBy->predicate,
                                               aConnectBy->constantPredicate,
                                               aConnectBy->ridPredicate,
                                               aConnectBy->index,
                                               aCNBY->myRowID,
                                               & sMethod.constantFilter,
                                               & sMethod.filter,
                                               & sMethod.lobFilter,
                                               & sMethod.subqueryFilter,
                                               & sMethod.varKeyRange,
                                               & sMethod.varKeyFilter,
                                               & sMethod.varKeyRange4Filter,
                                               & sMethod.varKeyFilter4Filter,
                                               & sMethod.fixKeyRange,
                                               & sMethod.fixKeyFilter,
                                               & sMethod.fixKeyRange4Print,
                                               & sMethod.fixKeyFilter4Print,
                                               & sMethod.ridRange,
                                               & sIsSubQuery )
              != IDE_SUCCESS );

    /* 3. constantFilter ���� */
    if ( sMethod.constantFilter != NULL )
    {
        aCNBY->connectByConstant = sMethod.constantFilter;
    }
    else
    {
        aCNBY->connectByConstant = NULL;
    }

    /* BUG-39401 need subquery for connect by clause */
    if ( sMethod.subqueryFilter != NULL )
    {
        aCNBY->connectBySubquery = sMethod.subqueryFilter;
    }
    else
    {
        aCNBY->connectBySubquery = NULL;
    }

    if ( ( sMethod.varKeyRange == NULL ) &&
         ( sMethod.varKeyFilter == NULL ) &&
         ( sMethod.varKeyRange4Filter == NULL ) &&
         ( sMethod.varKeyFilter4Filter == NULL ) &&
         ( sMethod.fixKeyRange == NULL ) &&
         ( sMethod.fixKeyFilter == NULL ) )
    {
        aCNBY->mIndex = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST_RAISE( sMethod.lobFilter != NULL, ERR_NOT_SUPPORT_LOB_FILTER );

    if ( sMethod.filter != NULL )
    {
        aCNBY->connectByFilter = sMethod.filter;

        /* 4-1. find Prior column list */
        sPriorTmp.node.next = NULL;
        IDE_TEST( findPriorColumns( aStatement,
                                    sMethod.filter,
                                    & sPriorTmp )
                  != IDE_SUCCESS );
        aCNBY->priorNode = (qtcNode *)sPriorTmp.node.next;

        /* 4-2. find Prior predicate and Sort node */
        IDE_TEST( findPriorPredAndSortNode( aStatement,
                                            sMethod.filter,
                                            & sSortNode,
                                            & sPriorPred,
                                            aFrom->tableRef->table,
                                            & sFind )
                  != IDE_SUCCESS );

        /* 5. baseSort Node ���� */
        for ( sItrAttr  = aCNBY->plan.resultDesc;
              sItrAttr != NULL;
              sItrAttr  = sItrAttr->next )
        {
            if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
            {
                IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                                  aQuerySet,
                                                  sItrAttr->expr,
                                                  ID_FALSE,
                                                  aCNBY->baseRowID,
                                                  0,
                                                  & sColumnCount,
                                                  & sMtrNode )
                          != IDE_SUCCESS );

                sMtrNode->srcNode->node.table = aCNBY->baseRowID;

                sMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
                sMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;

                if ( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK )
                     == QMC_ATTR_SORT_ORDER_ASC )
                {
                    sMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                    sMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
                }
                else
                {
                    sMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                    sMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
                }

                if( sFirstMtrNode == NULL )
                {
                    sFirstMtrNode       = sMtrNode;
                    sLastMtrNode        = sMtrNode;
                }
                else
                {
                    sLastMtrNode->next  = sMtrNode;
                    sLastMtrNode        = sMtrNode;
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        aCNBY->baseSortNode = sFirstMtrNode;

        if ( ( sFind == ID_TRUE ) &&
             ( aCNBY->mIndex == NULL ) )
        {
            IDE_TEST( makeSortNodeOfCNBY( aStatement,
                                          aQuerySet,
                                          aCNBY,
                                          sSortNode )
                      != IDE_SUCCESS );

            /* CNF sPriorPredicate �� DNF ���·� ��ȯ */
            IDE_TEST( qmoNormalForm::normalizeDNF( aStatement,
                                                   sPriorPred,
                                                   & aCNBY->connectByKeyRange)
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* fix BUG-19074 Host ������ ������ Constant Expression �� ����ȭ */
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aCNBY->connectByFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    aCNBY->mVarKeyRange         = sMethod.varKeyRange;
    aCNBY->mVarKeyFilter        = sMethod.varKeyFilter;
    aCNBY->mVarKeyRange4Filter  = sMethod.varKeyRange4Filter;
    aCNBY->mVarKeyFilter4Filter = sMethod.varKeyFilter4Filter;
    aCNBY->mFixKeyRange         = sMethod.fixKeyRange;
    aCNBY->mFixKeyFilter        = sMethod.fixKeyFilter;
    aCNBY->mFixKeyRange4Print   = sMethod.fixKeyRange4Print;
    aCNBY->mFixKeyFilter4Print  = sMethod.fixKeyFilter4Print;

    sColumnCount = 0;
    for ( sTmp = aCNBY->sortNode; sTmp != NULL ; sTmp = sTmp->next )
    {
        sColumnCount++;
    }

    sCurOffset = aCNBY->mtrNodeOffset;

    // ���� ��尡 ����� ���� ����
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    sColumnCount = 0;
    for ( sTmp = aCNBY->baseSortNode; sTmp != NULL ; sTmp = sTmp->next )
    {
        sColumnCount++;
    }

    aCNBY->baseSortOffset = sCurOffset;
    sCurOffset = sCurOffset + idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    aCNBY->sortMTROffset = sCurOffset;
    sCurOffset += idlOS::align8(ID_SIZEOF(qmcdSortTemp));

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sCurOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_LOB_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMO_NOT_ALLOWED_LOB_FILTER ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::makeSortNodeOfCNBY( qcStatement * aStatement,
                                          qmsQuerySet * aQuerySet,
                                          qmncCNBY    * aCNBY,
                                          qtcNode     * aSortNode )
{
    UShort         sSortID       = 0;
    UShort         sColumnCount  = 0;
    UInt           sFlag         = 0;
    mtcTemplate  * sMtcTemplate  = NULL;
    qmcMtrNode   * sMtrNode      = NULL;
    qmcMtrNode   * sFirstMtrNode = NULL;
    qmcMtrNode   * sLastMtrNode  = NULL;
    qmcAttrDesc  * sAttr         = NULL;
    qmcAttrDesc  * sItrAttr      = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeSortNodeOfCNBY::__FT__" );

    sMtcTemplate = &QC_SHARED_TMPLATE(aStatement)->tmplate;

    /* 6. KeyRange�� �����ؼ� Sort�� ƩǮ ���� */

    // BUG-36472 (table tuple->intermediate tuple)
    IDE_TEST( qtc::nextTable( & sSortID,
                              aStatement,
                              NULL,
                              ID_FALSE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );
    sMtcTemplate->rows[sSortID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sSortID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    /* 7. Order Siblings by ��� ó�� */
    sFirstMtrNode = NULL;
    sLastMtrNode = NULL;
    sColumnCount = 0;

    IDE_TEST( qmc::findAttribute( aStatement,
                                  aCNBY->plan.resultDesc,
                                  aSortNode,
                                  ID_FALSE,
                                  & sAttr )
              != IDE_SUCCESS );

    if ( sAttr != NULL )
    {
        // PROJ-2179
        // Sort node�� materialize node�� ���� ó���� �׻� ��ġ�ϹǷ�
        // ���� siblings�� ���Ե� ��� key flag�� �����Ѵ�.
        sAttr->flag &= ~QMC_ATTR_KEY_MASK;
        sAttr->flag |= QMC_ATTR_KEY_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    aSortNode->node.table = aCNBY->baseRowID;

    if ( ( ( aCNBY->flag & QMNC_CNBY_CHILD_VIEW_MASK )
            == QMNC_CNBY_CHILD_VIEW_FALSE ) &&
         ( ( aCNBY->flag & QMNC_CNBY_JOIN_MASK )
           == QMNC_CNBY_JOIN_FALSE ) )
    {
        if ( ( aCNBY->plan.flag & QMN_PLAN_STORAGE_MASK )
             == QMN_PLAN_STORAGE_DISK )
        {
            sFlag &= ~QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_MASK;
            sFlag |= QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( qmg::makeBaseTableMtrNode( aStatement,
                                             aSortNode,
                                             sSortID,
                                             & sColumnCount,
                                             & sMtrNode )
                  != IDE_SUCCESS );
        sFirstMtrNode = sMtrNode;
        sLastMtrNode = sMtrNode;
    }
    else
    {
        /* Nothing to do */
    }
    IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                      aQuerySet,
                                      aSortNode,
                                      ID_FALSE,
                                      sSortID,
                                      sFlag,
                                      & sColumnCount,
                                      & sMtrNode )
              != IDE_SUCCESS );

    if ( sFirstMtrNode == NULL )
    {
        sFirstMtrNode = sMtrNode;
        sLastMtrNode = sMtrNode;
    }
    else
    {
        sLastMtrNode->next  = sMtrNode;
        sLastMtrNode        = sMtrNode;
    }

    for ( sItrAttr  = aCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sSortID,
                                              0,
                                              & sColumnCount,
                                              & sMtrNode )
                      != IDE_SUCCESS );

            sMtrNode->srcNode->node.table = aCNBY->baseRowID;

            if ( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK )
                    == QMC_ATTR_SORT_ORDER_ASC )
            {
                sMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
            else
            {
                sMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
            }

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode       = sMtrNode;
                sLastMtrNode        = sMtrNode;
            }
            else
            {
                sLastMtrNode->next  = sMtrNode;
                sLastMtrNode        = sMtrNode;
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    aCNBY->sortNode = sFirstMtrNode;

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------

    // BUG-36472 (table tuple->intermediate tuple)
    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sSortID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sSortID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sSortID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sSortID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sSortID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    aCNBY->sortRowID = sSortID;
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement,
                                         aCNBY->sortNode )
              != IDE_SUCCESS);

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    aCNBY->sortNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Pseduo Column�� Tuple ID ����.
 *
 */
IDE_RC qmoOneMtrPlan::setPseudoColumnRowID( qtcNode * aNode, UShort * aRowID )
{
    qtcNode * sNode = aNode;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::setPseudoColumnRowID::__FT__" );

    if ( sNode != NULL )
    {
        /*
         * BUG-17949
         * Group By Level �� ��� SFWGH->level �� passNode�� �޷��ִ�.
         */
        if ( sNode->node.module == &qtc::passModule )
        {
            sNode = ( qtcNode * )sNode->node.arguments;
        }
        else
        {
            /* Nothing to do */
        }

        *aRowID = sNode->node.table;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;
}

/**
 * Make CNBY Plan
 *
 *  Hierarchy �������� ���� CNBY Plan�� �����Ѵ�.
 *  �� Plan���� StartWith�� ConnectBy�� Order Siblings By ������ ��� ó���Ѵ�.
 *
 *  ���� CNBY Plan�� View�� CMTR�� Materialize�� Data�� ������ Hierarhcy�� ó���Ѵ�.
 *
 *  �Ѱ��� Table�� ��쵵 �̸� View�� Transformaion �ؼ� ó���Ѵ�.
 *  �̴� qmvHierTransform���� ó���Ѵ�.
 *        select * from t1 connect by prior id = pid;
 *    --> select * from (select * from t1 ) t1 connect by prior id = pid;
 */
IDE_RC qmoOneMtrPlan::initCNBY( qcStatement    * aStatement,
                                qmsQuerySet    * aQuerySet,
                                qmoLeafInfo    * aLeafInfo,
                                qmsSortColumns * aSiblings,
                                qmnPlan        * aParent,
                                qmnPlan       ** aPlan )
{
    qmncCNBY       * sCNBY;
    UInt             sDataNodeOffset;
    UInt             sFlag = 0;
    qtcNode        * sNode;
    qmsSortColumns * sSibling;
    qmcAttrDesc    * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initCNBY::__FT__" );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCNBY ),
                                               (void **)& sCNBY )
              != IDE_SUCCESS );

    idlOS::memset( sCNBY, 0x00, ID_SIZEOF(qmncCNBY));
    QMO_INIT_PLAN_NODE( sCNBY,
                        QC_SHARED_TMPLATE( aStatement),
                        QMN_CNBY,
                        qmnCNBY,
                        qmndCNBY,
                        sDataNodeOffset );

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    for ( sSibling  = aSiblings;
          sSibling != NULL;
          sSibling  = sSibling->next )
    {
        if ( aSiblings->isDESC == ID_FALSE )
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_ASC;
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_DESC;
        }

        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sCNBY->plan.resultDesc,
                                        sSibling->sortColumn,
                                        sFlag,
                                        QMC_APPEND_EXPRESSION_TRUE,
                                        ID_FALSE )
                  != IDE_SUCCESS );
    }

    // START WITH�� �߰�
    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    & sCNBY->plan.resultDesc,
                                    aLeafInfo[0].predicate )
              != IDE_SUCCESS );

    // CONNECT BY�� �߰�
    // PROJ-2469 Optimize View Materialization
    // BUG FIX : aLeafInfo[0] -> aLeafInfo[1]�� ����
    //                           CONNECT BY �� Predicate�� ������� �ʰ� START WITH�� �� ��
    //                           ����ϰ� �־���.
    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    & sCNBY->plan.resultDesc,
                                    aLeafInfo[1].predicate )
              != IDE_SUCCESS );

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sCNBY->plan.resultDesc )
              != IDE_SUCCESS );

    for ( sItrAttr  = sCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( ( sItrAttr->expr->lflag & QTC_NODE_LEVEL_MASK ) == QTC_NODE_LEVEL_EXIST ) ||
             ( ( sItrAttr->expr->lflag & QTC_NODE_ISLEAF_MASK ) == QTC_NODE_ISLEAF_EXIST ) )
        {
            sItrAttr->flag &= ~QMC_ATTR_TERMINAL_MASK;
            sItrAttr->flag |= QMC_ATTR_TERMINAL_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sItrAttr->expr->lflag & QTC_NODE_PRIOR_MASK ) == QTC_NODE_PRIOR_EXIST )
        {
            sItrAttr->flag &= ~QMC_ATTR_TERMINAL_MASK;
            sItrAttr->flag |= QMC_ATTR_TERMINAL_TRUE;

            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                       (void**)&sNode )
                      != IDE_SUCCESS );

            idlOS::memcpy( sNode, sItrAttr->expr, ID_SIZEOF( qtcNode ) );

            sNode->lflag &= ~QTC_NODE_PRIOR_MASK;
            sNode->lflag |= QTC_NODE_PRIOR_ABSENT;

            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sCNBY->plan.resultDesc,
                                            sNode,
                                            0,
                                            0,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    *aPlan = (qmnPlan *)sCNBY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::makeCNBY( qcStatement    * aStatement,
                                qmsQuerySet    * aQuerySet,
                                qmsFrom        * aFrom,
                                qmoLeafInfo    * aStartWith,
                                qmoLeafInfo    * aConnectBy,
                                qmnPlan        * aChildPlan,
                                qmnPlan        * aPlan )
{
    mtcTemplate  * sMtcTemplate    = NULL;
    qmncCNBY     * sCNBY = (qmncCNBY *)aPlan;
    UShort         sPriorID        = 0;
    UShort         sChildTupleID   = 0;
    UShort         sTupleID        = 0;
    UInt           sDataNodeOffset = 0;
    UInt           i               = 0;
    qtcNode      * sPredicate[9];

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeCNBY::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aFrom      != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8(aPlan->offset + ID_SIZEOF(qmndCNBY));

    sCNBY->plan.left     = aChildPlan;
    sCNBY->myRowID       = aFrom->tableRef->table;
    sTupleID             = sCNBY->myRowID;
    sCNBY->mtrNodeOffset = sDataNodeOffset;

    sCNBY->flag             = QMN_PLAN_FLAG_CLEAR;
    sCNBY->plan.flag        = QMN_PLAN_FLAG_CLEAR;
    sCNBY->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //loop�� ã�� �������� ���� ����
    if( (aQuerySet->SFWGH->hierarchy->flag & QMS_HIERARCHY_IGNORE_LOOP_MASK) ==
         QMS_HIERARCHY_IGNORE_LOOP_TRUE )
    {
        sCNBY->flag &= ~QMNC_CNBY_IGNORE_LOOP_MASK;
        sCNBY->flag |= QMNC_CNBY_IGNORE_LOOP_TRUE;
    }
    else
    {
        sCNBY->flag &= ~QMNC_CNBY_IGNORE_LOOP_MASK;
        sCNBY->flag |= QMNC_CNBY_IGNORE_LOOP_FALSE;
    }

    sCNBY->flag &= ~QMNC_CNBY_CHILD_VIEW_MASK;
    sCNBY->flag |= QMNC_CNBY_CHILD_VIEW_FALSE;

    if ( aFrom->tableRef->view != NULL )
    {
        sCNBY->flag &= ~QMNC_CNBY_CHILD_VIEW_MASK;
        sCNBY->flag |= QMNC_CNBY_CHILD_VIEW_TRUE;

        sChildTupleID = ((qmncCMTR *)aChildPlan)->myNode->dstNode->node.table;
        sCNBY->baseRowID = sChildTupleID;
        sCNBY->mIndex     = NULL;

        // Tuple.flag�� ����
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_TYPE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_TYPE_TABLE;

        // To Fix PR-8385
        // VSCN�� �����Ǵ� ��쿡�� in-line view�� �ϴ���
        // �Ϲ����̺�� ó���Ͽ��� �Ѵ�. ���� ������ view��� ���õȰ���
        // false�� �����Ѵ�.
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_VIEW_MASK;
        sMtcTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_VIEW_FALSE;

        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;

        sMtcTemplate->rows[sTupleID].lflag
            |=  (sMtcTemplate->rows[sChildTupleID].lflag & MTC_TUPLE_STORAGE_MASK);

        if ( ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
             &&
             ( ( sMtcTemplate->rows[sTupleID].lflag & MTC_TUPLE_STORAGE_MASK )
               == MTC_TUPLE_STORAGE_DISK ) )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_RID;
        }
        IDE_TEST( qtc::nextTable( & sPriorID,
                                  aStatement,
                                  aFrom->tableRef->tableInfo,
                                  ID_FALSE,
                                  MTC_COLUMN_NOTNULL_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        sCNBY->baseRowID    = aFrom->tableRef->table;
        sCNBY->mIndex       = aConnectBy->index;
        sCNBY->mTableHandle = aFrom->tableRef->tableInfo->tableHandle;
        sCNBY->mTableSCN    = aFrom->tableRef->tableSCN;

        if ( ( ( aFrom->tableRef->tableInfo->tableOwnerID == QC_SYSTEM_USER_ID ) &&
               ( idlOS::strMatch( aFrom->tableRef->tableInfo->name,
                                  idlOS::strlen(aFrom->tableRef->tableInfo->name),
                                  "SYS_DUMMY_",
                                  10 ) == 0 ) ) ||
             ( idlOS::strMatch( aFrom->tableRef->tableInfo->name,
                                idlOS::strlen(aFrom->tableRef->tableInfo->name),
                                "X$DUAL",
                                6 ) == 0 ) )
        {
            sCNBY->flag &= ~QMNC_CNBY_FROM_DUAL_MASK;
            sCNBY->flag |= QMNC_CNBY_FROM_DUAL_TRUE;
        }
        else
        {
            sCNBY->flag &= ~QMNC_CNBY_FROM_DUAL_MASK;
            sCNBY->flag |= QMNC_CNBY_FROM_DUAL_FALSE;
        }
        IDE_TEST( qtc::nextTable( & sPriorID,
                                  aStatement,
                                  aFrom->tableRef->tableInfo,
                                  smiIsDiskTable(sCNBY->mTableHandle),
                                  MTC_COLUMN_NOTNULL_TRUE )
                  != IDE_SUCCESS );
    }

    /* BUG-44382 clone tuple ���ɰ��� */
    // ���簡 �ʿ���
    qtc::setTupleColumnFlag( &(sMtcTemplate->rows[sPriorID]),
                             ID_TRUE,
                             ID_FALSE );

    aQuerySet->SFWGH->hierarchy->originalTable = sCNBY->myRowID;
    aQuerySet->SFWGH->hierarchy->priorTable    = sPriorID;
    sCNBY->priorRowID                          = sPriorID;

    /* BUG-48300 */
    if ( aQuerySet->SFWGH->level != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->level,
                                        & sCNBY->levelRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_LEVEL_MASK;
        sCNBY->flag |= QMNC_CNBY_LEVEL_TRUE;
    }

    /* BUG-48300 */
    if ( aQuerySet->SFWGH->isLeaf != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->isLeaf,
                                        & sCNBY->isLeafRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_ISLEAF_MASK;
        sCNBY->flag |= QMNC_CNBY_ISLEAF_TRUE;
    }
    /* BUG-48300 */
    if ( aQuerySet->SFWGH->cnbyStackAddr != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->cnbyStackAddr,
                                        & sCNBY->stackRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_FUNCTION_MASK;
        sCNBY->flag |= QMNC_CNBY_FUNCTION_TRUE;
    }

    IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->rownum,
                                    & sCNBY->rownumRowID )
              != IDE_SUCCESS );

    if ( aFrom->tableRef->view != NULL )
    {
        sMtcTemplate->rows[sPriorID].lflag &= ~MTC_TUPLE_ROW_ALLOCATE_MASK;
        sMtcTemplate->rows[sPriorID].lflag |= MTC_TUPLE_ROW_ALLOCATE_FALSE;
    }
    else
    {
        idlOS::memcpy( &QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPriorID],
                       &QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sCNBY->myRowID],
                       ID_SIZEOF(mtcTuple) );
    }

    /* PROJ-1473 */
    IDE_TEST( qtc::allocAndInitColumnLocateInTuple( QC_QMP_MEM( aStatement ),
                                                    &( QC_SHARED_TMPLATE( aStatement )->tmplate ),
                                                    sPriorID )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sCNBY->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    IDE_TEST( processStartWithPredicate( aStatement,
                                         aQuerySet,
                                         sCNBY,
                                         aStartWith )
              != IDE_SUCCESS );
    IDE_TEST( processConnectByPredicate( aStatement,
                                         aQuerySet,
                                         aFrom,
                                         sCNBY,
                                         aConnectBy )
              != IDE_SUCCESS );


    /* ������ �۾�1 */
    sPredicate[0] = sCNBY->startWithConstant;
    sPredicate[1] = sCNBY->startWithFilter;
    sPredicate[2] = sCNBY->startWithSubquery;
    sPredicate[3] = sCNBY->startWithNNF;
    sPredicate[4] = sCNBY->connectByConstant;
    sPredicate[5] = sCNBY->connectByFilter;
    /* fix BUG-26770
     * connect by LEVEL+to_date(:D1,'YYYYMMDD') <= to_date(:D2,'YYYYMMDD')+1;
     * ���Ǽ���� ��������������
     * level filter�� ���� ó���� ����,
     * level filter�� ���ѵ� host������ ������� ���� bindParamInfo ������, ������������.
     */
    sPredicate[6] = sCNBY->levelFilter;

    /* BUG-39041 The connect by clause is not support subquery. */
    sPredicate[7] = sCNBY->connectBySubquery;

    /* BUG-39434 The connect by need rownum pseudo column. */
    sPredicate[8] = sCNBY->rownumFilter;

    for ( i = 0;
          i < 9;
          i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }

    /* dependency ó�� �� subquery�� ó�� */
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sCNBY->plan ,
                                            QMO_CNBY_DEPENDENCY,
                                            sCNBY->myRowID ,
                                            NULL,
                                            9,
                                            sPredicate,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sCNBY->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sCNBY->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCNBY->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::initCMTR( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmnPlan     ** aPlan )
{
    qmncCMTR          * sCMTR;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initCMTR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncCMTR�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCMTR ),
                                               (void **)& sCMTR )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sCMTR ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_CMTR ,
                        qmnCMTR ,
                        qmndCMTR,
                        sDataNodeOffset );

    IDE_TEST( qmc::createResultFromQuerySet( aStatement,
                                             aQuerySet,
                                             & sCMTR->plan.resultDesc )
              != IDE_SUCCESS );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sCMTR->planID,
                                         ID_FALSE,
                                         ID_TRUE )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sCMTR;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * make CMTR
 *
 *  CNBY Plan�� ������ �ΰ� ���� ��带 QMN_VIEW�� ������ Plan��带 �����Ѵ�.
 *  View �� �ش��ϴ� ��� �����͸� �� CMTR�� �׾Ƴ��� CNBY�� ó���Ѵ�.
 *
 *  CMTR�� Memory Temp Table�� �����Ѵ�.
 */
IDE_RC qmoOneMtrPlan::makeCMTR( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                UInt           aFlag,
                                qmnPlan      * aChildPlan,
                                qmnPlan      * aPlan )
{
    qmncCMTR        * sCMTR = (qmncCMTR *)aPlan;
    qmcMtrNode      * sNewMtrNode       = NULL;
    qmcMtrNode      * sLastMtrNode      = NULL;
    mtcTemplate     * sMtcTemplate      = NULL;
    UInt              sDataNodeOffset   = 0;
    UShort            sTupleID          = 0;
    UShort            sColumnCount      = 0;
    qmcMtrNode      * sMtrNode[2];
    qmcAttrDesc     * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeCMTR::__FT__" );

    sMtcTemplate = &QC_SHARED_TMPLATE(aStatement)->tmplate;

    IDE_TEST_RAISE( ( aFlag & QMO_MAKEVMTR_TEMP_TABLE_MASK )
                    != QMO_MAKEVMTR_MEMORY_TEMP_TABLE,
                    ERR_NOT_MEMORY_TEMP_TABLE);

    IDE_TEST_RAISE( aChildPlan->type != QMN_VIEW, ERR_NOT_VIEW );

    aPlan->offset    = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndCMTR));

    sCMTR->plan.left     = aChildPlan;
    sCMTR->mtrNodeOffset = sDataNodeOffset;

    sCMTR->flag      = QMN_PLAN_FLAG_CLEAR;
    sCMTR->plan.flag = QMN_PLAN_FLAG_CLEAR;
    sCMTR->myNode    = NULL;

    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_FALSE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sCMTR->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                          aQuerySet ,
                                          sItrAttr->expr,
                                          ID_FALSE,
                                          sTupleID ,
                                          0,
                                          & sColumnCount ,
                                          & sNewMtrNode )
                  != IDE_SUCCESS );

        if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) == QMC_ATTR_USELESS_RESULT_TRUE )
        {
            // PROJ-2469 Optimize View Materialization
            // �������� ������ �ʴ� MtrNode�� ���ؼ� flag ó���Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |=  QMC_MTR_TYPE_USELESS_COLUMN;    
        }
        else
        {
            //CMTR���� ������ qmcMtrNode�̹Ƿ� �����ϵ��� �Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;
        }

        // PROJ-2179
        sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
        sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

        // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
        sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
        sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

        //connect
        if( sCMTR->myNode == NULL )
        {
            sCMTR->myNode = sNewMtrNode;
            sLastMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
    }

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    sCMTR->plan.flag &= ~QMN_PLAN_STORAGE_MASK;
    sCMTR->plan.flag |= QMN_PLAN_STORAGE_MEMORY;

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sCMTR->myNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sCMTR->myNode )
              != IDE_SUCCESS );

    QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    sMtrNode[0] = sCMTR->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sCMTR->plan,
                                            QMO_CMTR_DEPENDENCY,
                                            sTupleID,
                                            NULL,
                                            0,
                                            NULL,
                                            1,
                                            sMtrNode )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sCMTR->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sCMTR->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCMTR->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sCMTR->planID,
                               sCMTR->plan.flag,
                               sMtcTemplate->rows[sTupleID].lflag,
                               sCMTR->myNode,
                               &sCMTR->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_MEMORY_TEMP_TABLE )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                "qmoOneMtrPlan::makeCMTR",
                                "aFlag is not QMO_MAKEVMTR_MEMORY_TEMP_TABLE"));
    }
    IDE_EXCEPTION( ERR_NOT_VIEW )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                "qmoOneMtrPlan::makeCMTR",
                                "aChildPlan->type != QMN_VIEW"));
    }
    IDE_EXCEPTION_END

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::makeNonKeyAttrsMtrNodes( qcStatement  * aStatement,
                                        qmsQuerySet  * aQuerySet,
                                        qmcAttrDesc  * aResultDesc,
                                        UShort         aTupleID,
                                        qmcMtrNode  ** aFirstMtrNode,
                                        qmcMtrNode  ** aLastMtrNode,
                                        UShort       * aMtrNodeCount )
{
/***********************************************************************
 *
 * Description :
 *    Result descriptor���� key�� �ƴ� attribute�鿡 ���ؼ���
 *    materialize node�� �����Ѵ�.
 *
 * Implementation :
 *
 **********************************************************************/

    mtcTemplate * sMtcTemplate;
    qmcMtrNode  * sNewMtrNode;
    qmcAttrDesc * sItrAttr;
    ULong         sFlag;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeNonKeyAttrsMtrNodes::__FT__" );

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    for ( sItrAttr  = aResultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_FALSE ) &&
               ( ( sItrAttr->flag & QMC_ATTR_ANALYTIC_FUNC_MASK )
                 == QMC_ATTR_ANALYTIC_FUNC_FALSE ) ) ||
             ( ( sMtcTemplate->rows[sItrAttr->expr->node.table].lflag &
                 MTC_TUPLE_TARGET_UPDATE_DELETE_MASK ) /* BUG-39399 remove search key preserved table */
               == MTC_TUPLE_TARGET_UPDATE_DELETE_TRUE ) )
        {
            IDE_DASSERT( sItrAttr->expr->node.module != &qtc::passModule );

            IDE_TEST( qmg::changeColumnLocate( aStatement,
                                               aQuerySet,
                                               sItrAttr->expr,
                                               aTupleID,
                                               ID_FALSE )
                      != IDE_SUCCESS );

            sFlag = sMtcTemplate->rows[sItrAttr->expr->node.table].lflag;

            // src, dst�� ��� disk�� ��쿡�� value�� materialization�� �����ϴ�.
            if( qmg::getMtrMethod( aStatement,
                                   sItrAttr->expr->node.table,
                                   aTupleID ) == ID_TRUE )
            {
                IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                                  aQuerySet,
                                                  sItrAttr->expr,
                                                  ID_FALSE,
                                                  aTupleID,
                                                  0,
                                                  aMtrNodeCount,
                                                  & sNewMtrNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // Surrogate-key(RID �Ǵ� pointer)�� �����Ѵ�.

                // �̹� surrogate-key�� �߰��Ǿ��ִ��� Ȯ���Ѵ�.
                if( qmg::existBaseTable( *aFirstMtrNode,
                                         qmg::getBaseTableType( sFlag ),
                                         sItrAttr->expr->node.table )
                        == ID_TRUE )
                {
                    continue;
                }
                else
                {
                    IDE_TEST( qmg::makeBaseTableMtrNode( aStatement,
                                                         sItrAttr->expr,
                                                         aTupleID,
                                                         aMtrNodeCount,
                                                         & sNewMtrNode )
                              != IDE_SUCCESS );
                }
            }

            if( *aFirstMtrNode == NULL )
            {
                *aFirstMtrNode        = sNewMtrNode;
                *aLastMtrNode         = sNewMtrNode;
            }
            else
            {
                (*aLastMtrNode)->next = sNewMtrNode;
                *aLastMtrNode         = sNewMtrNode;
            }
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
qmoOneMtrPlan::appendJoinPredicate( qcStatement  * aStatement,
                                    qmsQuerySet  * aQuerySet,
                                    qtcNode      * aJoinPredicate,
                                    idBool         aIsLeft,
                                    idBool         aAllowDup,
                                    qmcAttrDesc ** aResultDesc )
{
/***********************************************************************
 *
 * Description :
 *    Sort/hash join�� ���� join predicate���κ��� key�� ã��
 *    materialize node�� �����Ѵ�.
 *
 * Implementation :
 *
 **********************************************************************/
    UInt                sFlag = 0;
    UInt                sAppendOption = 0;
    qtcNode           * sOperatorNode;
    qtcNode           * sNode;
    qtcNode           * sArg;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::appendJoinPredicate::__FT__" );

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    sFlag &= ~QMC_ATTR_CONVERSION_MASK;
    sFlag |= QMC_ATTR_CONVERSION_TRUE;

    sAppendOption &= ~QMC_APPEND_EXPRESSION_MASK;
    sAppendOption |= QMC_APPEND_EXPRESSION_TRUE;

    if( aAllowDup == ID_TRUE )
    {
        sAppendOption &= ~QMC_APPEND_ALLOW_DUP_MASK;
        sAppendOption |= QMC_APPEND_ALLOW_DUP_TRUE;
    }
    else
    {
        sAppendOption &= ~QMC_APPEND_ALLOW_DUP_MASK;
        sAppendOption |= QMC_APPEND_ALLOW_DUP_FALSE;
    }

    // aJoinPredicate�� DNF�� �����Ǿ������Ƿ� ������ AND node���� ��ȸ�Ѵ�.
    for( sOperatorNode = (qtcNode *)aJoinPredicate->node.arguments->arguments;
         sOperatorNode != NULL;
         sOperatorNode = (qtcNode *)sOperatorNode->node.next )
    {
        sNode = (qtcNode*)sOperatorNode->node.arguments;

        if( aIsLeft == ID_FALSE )
        {
            //----------------------------------
            // indexArgument�� �ش��ϴ� �÷��� ������ ���
            //----------------------------------

            if( sOperatorNode->indexArgument == 0 )
            {
                // Nothing to do.
            }
            else
            {
                sNode = (qtcNode*)sNode->node.next;
            }
        }
        else
        {
            //----------------------------------
            // indexArgument�� �ش����� �ʴ� �÷��� ������ ���
            //----------------------------------

            if ( sOperatorNode->indexArgument == 0 )
            {
                sNode = (qtcNode*)sNode->node.next;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sNode->node.module == &mtfList )
        {
            for ( sArg  = (qtcNode *)sNode->node.arguments;
                  sArg != NULL;
                  sArg  = (qtcNode *)sArg->node.next )
            {
                IDE_TEST( appendJoinColumn( aStatement,
                                            aQuerySet,
                                            sArg,
                                            sFlag,
                                            sAppendOption,
                                            aResultDesc )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( appendJoinColumn( aStatement,
                                        aQuerySet,
                                        sNode,
                                        sFlag,
                                        sAppendOption,
                                        aResultDesc )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneMtrPlan::appendJoinColumn( qcStatement  * aStatement,
                                 qmsQuerySet  * aQuerySet,
                                 qtcNode      * aColumnNode,
                                 UInt           aFlag,
                                 UInt           aAppendOption,
                                 qmcAttrDesc ** aResultDesc )
{
    qtcNode * sCopiedNode;
    qtcNode * sPassNode;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::appendJoinColumn::__FT__" );

    if ( ( QMC_NEED_CALC( aColumnNode ) == ID_TRUE ) ||
         ( ( mtf::convertedNode( &aColumnNode->node , NULL) != &aColumnNode->node ) ) )
    {
        IDE_DASSERT( aColumnNode->node.module != &qtc::passModule );

        // Join predicate�� ���� expression�� ���
        // retrieve �� expression�� �������� �ʵ��� pass node�� �����Ѵ�.

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                   (void **)& sCopiedNode )
                  != IDE_SUCCESS );
        idlOS::memcpy( sCopiedNode, aColumnNode, ID_SIZEOF( qtcNode ) );

        IDE_TEST( qtc::makePassNode( aStatement ,
                                     NULL ,
                                     sCopiedNode ,
                                     & sPassNode )
                  != IDE_SUCCESS );

        sPassNode->node.next = sCopiedNode->node.next;
        idlOS::memcpy( aColumnNode, sPassNode, ID_SIZEOF(qtcNode) );

        aColumnNode = sCopiedNode;
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::appendAttribute( aStatement,
                                    aQuerySet,
                                    aResultDesc,
                                    aColumnNode,
                                    aFlag,
                                    aAppendOption,
                                    ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeMemoryValeTemp
 *
 *  Rollup, Cube Plan�� ���� Row�� ���� Plan�� �÷� �ִ� ���� �ƴ϶� ���� Row�� Group��
 *  ���� NULL�� �� �÷��� �����ϴ� ���ο� Row�� �� �� �ִ�.
 *  �̶� Memory Table�� ��쿡�� �׻� Pointer ������ ���� �װ� �Ǵ� �� ORDER BY ������ ���
 *  ������ SORT PLAN�� �����ȴ�. �� SORT Plan�� ������ Rollup �̳� Cube�� ���� �ϸ� �޸�
 *  �� ��� Pointer�� ������ ���ٵ� Rollup�̳� Cube�� ���� Plan���� �����ϴ� �ϳ��� Row��
 *  ���� �ϰ� �� ��� �� makeValueTemp �Լ��� ���� STORE�� Sort Temp�� �����ϰ�
 *  Rollup�̳� Cube���� ������ ROW�� ���� �߰��ؼ� �������� �� Row Pointer�� ���� �� ��
 *  �ֵ��� ���ش�.
 */
IDE_RC qmoOneMtrPlan::makeValueTempMtrNode( qcStatement * aStatement,
                                            qmsQuerySet * aQuerySet,
                                            UInt          aFlag,
                                            qmnPlan     * aPlan,
                                            qmcMtrNode ** aFirstNode,
                                            UShort      * aColumnCount )
{
    mtcTemplate * sMtcTemplate = NULL;
    qmcAttrDesc * sItrAttr = NULL;
    qmcMtrNode  * sFirstMtrNode = NULL;
    qmcMtrNode  * sLastMtrNode = NULL;
    qmcMtrNode  * sNewMtrNode = NULL;

    UShort        sTupleID = 0;
    UShort        sCount = 0;
    UShort        sColumnCount = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeValueTempMtrNode::__FT__" );

    sMtcTemplate = &QC_SHARED_TMPLATE( aStatement )->tmplate;

    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    if ( (aFlag & QMO_MAKESORT_TEMP_TABLE_MASK) ==
         QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_DISK;

        if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // disk temp�� ����ϴ� ��� TEMP_VAR_TYPE�� ������� �ʴ´�.
            if ( ( aPlan->flag & QMN_PLAN_STORAGE_MASK )
                 == QMN_PLAN_STORAGE_DISK )
            {
                sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;
            }
            else
            {
                // Nothing to do.
            }

            // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
            sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
            sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    for ( sItrAttr = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        if ( ( ( sItrAttr->flag & QMC_ATTR_GROUP_BY_EXT_MASK )
               == QMC_ATTR_GROUP_BY_EXT_TRUE ) &&
             ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK )
               == QMC_ATTR_KEY_FALSE ) )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            // BUG-36472
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // disk temp�� ����ϴ� ��� TEMP_VAR_TYPE�� ������� �ʴ´�.
            if ( ( aPlan->flag & QMN_PLAN_STORAGE_MASK )
                 == QMN_PLAN_STORAGE_DISK )
            {
                sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;
            }
            else
            {
                // Nothing to do.
            }

            // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
            sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
            sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aFirstNode = sFirstMtrNode;
    *aColumnCount = sCount;

    if ( sFirstMtrNode != NULL )
    {
        IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                               sMtcTemplate,
                                               sTupleID,
                                               sColumnCount )
                  != IDE_SUCCESS );

        /* BUG-36015 distinct sort temp problem */
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

        IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sFirstMtrNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmg::setColumnLocate( aStatement, sFirstMtrNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmg::setCalcLocate( aStatement, sFirstMtrNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 initROLL
 *
 *  Rollup�� Result Descript�� �߰��Ѵ�.
 *  GROUP BY �÷��� ���� �߰��ϰ� Rollup �÷��� �߰��ؼ� ���� Plan���� �� ������ sort �� ��
 *   �ֵ��� �Ѵ�.
 *  Aggregation �� �߰��Ѵ�.
 */
IDE_RC qmoOneMtrPlan::initROLL( qcStatement      * aStatement,
                                qmsQuerySet      * aQuerySet,
                                UInt             * aRollupCount,
                                qmsAggNode       * aAggrNode,
                                qmsConcatElement * aGroupColumn,
                                qmnPlan         ** aPlan )
{
    qmncROLL         * sROLL           = NULL;
    qmsConcatElement * sGroup          = NULL;
    qmsConcatElement * sSubGroup       = NULL;
    qmsAggNode       * sAggrNode       = NULL;
    qtcNode          * sNode           = NULL;
    qtcNode          * sListNode       = NULL;
    UInt               sDataNodeOffset = 0;
    UInt               sFlag           = 0;
    UInt               sRollupCount    = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initROLL::__FT__" );

    IDE_DASSERT( aStatement != NULL );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncROLL ),
                                               (void **)& sROLL )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sROLL,
                        QC_SHARED_TMPLATE( aStatement),
                        QMN_ROLL,
                        qmnROLL,
                        qmndROLL,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sROLL;

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
    sFlag |= QMC_ATTR_SORT_ORDER_ASC;

    for ( sGroup  = aGroupColumn;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        if ( sGroup->type == QMS_GROUPBY_NORMAL )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sROLL->plan.resultDesc,
                                            sGroup->arithmeticOrList,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE |
                                            QMC_APPEND_ALLOW_CONST_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    sFlag &= ~QMC_ATTR_GROUP_BY_EXT_MASK;
    sFlag |= QMC_ATTR_GROUP_BY_EXT_TRUE;
    for ( sGroup  = aGroupColumn;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        if ( sGroup->type != QMS_GROUPBY_NORMAL )
        {
            for ( sSubGroup = sGroup->arguments;
                  sSubGroup != NULL;
                  sSubGroup = sSubGroup->next )
            {
                sSubGroup->type = sGroup->type;
                if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    for ( sListNode  = (qtcNode *)sSubGroup->arithmeticOrList->node.arguments;
                          sListNode != NULL;
                          sListNode  = (qtcNode *)sListNode->node.next )
                    {
                        IDE_TEST( qmc::appendAttribute( aStatement,
                                                        aQuerySet,
                                                        & sROLL->plan.resultDesc,
                                                        sListNode,
                                                        sFlag,
                                                        QMC_APPEND_EXPRESSION_TRUE |
                                                        QMC_APPEND_ALLOW_DUP_TRUE |
                                                        QMC_APPEND_ALLOW_CONST_TRUE,
                                                        ID_FALSE )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    IDE_TEST( qmc::appendAttribute( aStatement,
                                                    aQuerySet,
                                                    & sROLL->plan.resultDesc,
                                                    sSubGroup->arithmeticOrList,
                                                    sFlag,
                                                    QMC_APPEND_EXPRESSION_TRUE |
                                                    QMC_APPEND_ALLOW_DUP_TRUE |
                                                    QMC_APPEND_ALLOW_CONST_TRUE,
                                                    ID_FALSE )
                              != IDE_SUCCESS );
                }
                ++sRollupCount;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    sFlag = 0;
    sFlag &= ~QMC_ATTR_GROUP_BY_EXT_MASK;
    sFlag |= QMC_ATTR_GROUP_BY_EXT_TRUE;

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while( sNode->node.module == &qtc::passModule )
        {
            sNode = ( qtcNode *)sNode->node.arguments;
        }

        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
            ( sNode->overClause == NULL ) )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sROLL->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE |
                                            QMC_APPEND_ALLOW_DUP_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sROLL->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aRollupCount = sRollupCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeROLL
 *
 *    qmnROLL Plan�� �����Ѵ�.
 *    Rollup ( �÷� ) �� ( n + 1 )���� �׷����� �̷���� �ִ�.
 *    Sort�� �ڷ���� �ѹ� �а� ���ϹǷν� n+1 ���� Group�� ó���Ѵ�.
 *
 *  - ������ SORT Plan�� ���ų� SCAN�� �� �� �ִ�.
 *  - ROLL Plan�� 5���� Tuple�� mtr Node�� ���� �� �� �ִ�.
 *  - mtrNode  - ���� Plan���� �ڷḦ �о� �����ϱ� ���� NODE
 *  - myNode   - ���� PLAN���� ���� NODE�̴�.
 *  - aggrNode - aggregation MTR NODE
 *  - distNode - aggregation�� distinct�� ������ ���� NODE
 *  - ValueTempNode - value store�� TEMP NODE
 */
IDE_RC qmoOneMtrPlan::makeROLL( qcStatement      * aStatement,
                                qmsQuerySet      * aQuerySet,
                                UInt               aFlag,
                                UInt               aRollupCount,
                                qmsAggNode       * aAggrNode,
                                qmoDistAggArg    * aDistAggArg,
                                qmsConcatElement * aGroupColumn,
                                qmnPlan          * aChildPlan,
                                qmnPlan          * aPlan )
{
    mtcTemplate      * sMtcTemplate    = NULL;
    qmncROLL         * sROLL           = NULL;
    qmcAttrDesc      * sItrAttr        = NULL;
    qmcMtrNode       * sNewMtrNode     = NULL;
    qmcMtrNode       * sFirstMtrNode   = NULL;
    qmcMtrNode       * sLastMtrNode    = NULL;
    qmsAggNode       * sAggrNode       = NULL;
    qmsConcatElement * sGroupBy;
    qmsConcatElement * sSubGroup;
    qmcMtrNode       * sMtrNode[3];
    qmcMtrNode         sDummyNode;
    qtcNode          * sNode;

    UChar       ** sGroup;
    UChar        * sTemp;
    UShort         sTupleID        = 0;
    UShort         sColumnCount    = 0;
    UShort         sAggrNodeCount  = 0;
    UShort         sDistNodeCount  = 0;
    UShort         sCount          = 0;
    UShort         sMemValueCount  = 0;
    UInt           sDataNodeOffset = 0;
    UInt           i;
    SInt           j;
    UInt           k;
    SInt           sRollupStart    = -1;
    idBool         sIsPartial      = ID_FALSE;
    UInt           sTempTuple      = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeROLL::__FT__" );

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aGroupColumn != NULL );
    IDE_DASSERT( aChildPlan   != NULL );
    IDE_DASSERT( aPlan        != NULL );

    sMtcTemplate    = &QC_SHARED_TMPLATE(aStatement)->tmplate;
    aPlan->offset   = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset + ID_SIZEOF( qmndROLL ));

    sROLL            = ( qmncROLL * )aPlan;
    sROLL->plan.left = aChildPlan;
    sROLL->flag      = QMN_PLAN_FLAG_CLEAR;
    sROLL->plan.flag = QMN_PLAN_FLAG_CLEAR;
    sROLL->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->groupingInfoAddr,
                                    & sROLL->groupingFuncRowID )
              != IDE_SUCCESS );

    if ( ( aFlag & QMO_MAKESORT_PRESERVED_ORDER_MASK )
         == QMO_MAKESORT_PRESERVED_TRUE )
    {
        sROLL->preservedOrderMode = ID_TRUE;
    }
    else
    {
        sROLL->preservedOrderMode = ID_FALSE;
    }

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sAggrNodeCount++;
        sNode = sAggrNode->aggr;
        if ( sNode->node.arguments != NULL )
        {
            IDE_TEST_RAISE ( ( sNode->node.arguments->lflag & MTC_NODE_OPERATOR_MASK )
                             == MTC_NODE_OPERATOR_SUBQUERY,
                             ERR_NOT_ALLOW_SUBQUERY );
        }
        else
        {
            /* Nothing to do */
        }
    }

    /* make mtrNode */
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    sTempTuple = sTupleID;
    if ( ( aFlag & QMO_MAKESORT_TEMP_TABLE_MASK ) ==
         QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sROLL->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sROLL->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

        /* PROJ-2462 Result Cache */
        if ( ( QC_SHARED_TMPLATE( aStatement )->resultCache.flag & QC_RESULT_CACHE_MASK )
             == QC_RESULT_CACHE_ENABLE )
        {
            if ( aQuerySet->SFWGH->hints->resultCacheType
                 != QMO_RESULT_CACHE_NO )
            {
                // Result Cache������ �׻� Value Temp�� �����ϵ��� �����Ѵ�.
                aFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                aFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( aQuerySet->SFWGH->hints->resultCacheType
                 == QMO_RESULT_CACHE )
            {
                // Result Cache������ �׻� Value Temp�� �����ϵ��� �����Ѵ�.
                aFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                aFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        sROLL->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sROLL->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            if ( ( sItrAttr->flag & QMC_ATTR_GROUP_BY_EXT_MASK )
                 == QMC_ATTR_GROUP_BY_EXT_TRUE )
            {
                if ( sRollupStart == -1 )
                {
                    sRollupStart = sCount;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                sIsPartial = ID_TRUE;
            }

            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            sNewMtrNode->flag &= ~QMC_MTR_GROUPING_MASK;
            sNewMtrNode->flag |= QMC_MTR_GROUPING_TRUE;
            sNewMtrNode->flag &= ~QMC_MTR_MTR_PLAN_MASK;
            sNewMtrNode->flag |= QMC_MTR_MTR_PLAN_TRUE;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sDummyNode.next    = NULL;
    sDummyNode.srcNode = NULL;
    sNewMtrNode        = &sDummyNode;
    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while( sNode->node.module == &qtc::passModule )
        {
            sNode = (qtcNode *)sNode->node.arguments;
        }

        if ( ( sNode->node.lflag & MTC_NODE_FUNCTON_GROUPING_MASK )
             == MTC_NODE_FUNCTON_GROUPING_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        /* Aggregate function�� �ƴ� node�� ���޵Ǵ� ��찡 �����Ѵ�.*/
        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if ( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
             ( sNode->overClause == NULL ) )
        {
            if ( sNode->node.arguments != NULL )
            {
                IDE_TEST( makeAggrArgumentsMtrNode( aStatement,
                                                    aQuerySet,
                                                    sTupleID,
                                                    & sColumnCount,
                                                    sNode->node.arguments,
                                                    & sNewMtrNode )
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
    }
    if ( sDummyNode.next != NULL )
    {
        sLastMtrNode->next = sDummyNode.next;
    }
    else
    {
        /* Nothing to do */
    }

    sROLL->mtrNode      = sFirstMtrNode;
    sROLL->mtrNodeCount = sColumnCount;

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sTupleID,
                                           sColumnCount )
              != IDE_SUCCESS );

    if ( ( aFlag & QMO_MAKESORT_TEMP_TABLE_MASK ) ==
         QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sROLL->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sROLL->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sROLL->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sROLL->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sROLL->mtrNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sROLL->mtrNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setCalcLocate( aStatement, sROLL->mtrNode )
              != IDE_SUCCESS );

    /* Parital Rollup �� ��� Rollup�� �÷��� ��ġ�� �����Ѵ�. */
    if ( sIsPartial == ID_TRUE )
    {
        sROLL->partialRollup = sRollupStart;
    }
    else
    {
        sROLL->partialRollup = -1;
    }

    sROLL->groupCount = aRollupCount + 1;

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ( ID_SIZEOF( UShort ) * sROLL->groupCount ),
                                               (void**) & sROLL->elementCount )
              != IDE_SUCCESS );

    sROLL->elementCount[aRollupCount] = 0;

    for ( sGroupBy  = aGroupColumn;
          sGroupBy != NULL;
          sGroupBy  = sGroupBy->next )
    {
        if ( sGroupBy->type == QMS_GROUPBY_ROLLUP )
        {
            for ( sSubGroup = sGroupBy->arguments, i = 0;
                  sSubGroup != NULL;
                  sSubGroup = sSubGroup->next, i++ )
            {
                if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    sROLL->elementCount[i] = sSubGroup->arithmeticOrList->node.lflag &
                        MTC_NODE_ARGUMENT_COUNT_MASK;
                }
                else
                {
                    sROLL->elementCount[i] = 1;
                }
            }
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    /**
     * Rollup���� ���Ǵ� Group�� �÷��� NULL���θ� ǥ���Ѵ�.
     * ���� Rollup���� ���� �÷��� 5 ����� ������ ���� ǥ�õȴ�.
     * ([0] 11111
     *  [1] 11110
     *  [2] 11100
     *  [3] 11000
     *  [4] 10000
     *  [5] 00000 )
     *  Rollup�� n+1���� �׷��� �����ǹǷ� 6���� �迭�� ������ �ȴ�.
     */
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( (ID_SIZEOF(UChar*) *
                                              (aRollupCount + 1)),
                                             (void**) &sGroup )
              != IDE_SUCCESS);
    sROLL->mtrGroup = sGroup;
    for ( i = 0; i < aRollupCount + 1; i++ )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->cralloc( (ID_SIZEOF(UChar) *
                                                    sROLL->mtrNodeCount ),
                                                   (void**) &sTemp )
                  != IDE_SUCCESS);
        sROLL->mtrGroup[i] = sTemp;

        for ( j = 0; j < sRollupStart; j++ )
        {
            sROLL->mtrGroup[i][j] = 0x1;
        }

        for ( k = 0; k < aRollupCount; k++ )
        {
            if ( k >= aRollupCount - i )
            {
                break;
            }
            else
            {
                for ( sCount = 0;
                      sCount < sROLL->elementCount[k];
                      sCount++ )
                {
                    sROLL->mtrGroup[i][j] = 0x1;
                    j++;
                }
            }
        }
    }

    /* make myNode */
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    sFirstMtrNode = NULL;
    sCount        = 0;
    sColumnCount  = 0;
    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );
            if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
                 == QMO_MAKESORT_VALUE_TEMP_FALSE )
            {
                // �������� �� table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
                sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
                sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;
            }
            else
            {
                /* Nothing to do */
            }

            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // myNode�� temp�� ������ �ʴ´�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }
    sROLL->myNode      = sFirstMtrNode;
    sROLL->myNodeCount = sCount;

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sTupleID,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sROLL->myNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sROLL->myNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setCalcLocate( aStatement, sROLL->myNode )
              != IDE_SUCCESS );

    /* AggrMTR Node ���� */
    sROLL->aggrNodeCount = sAggrNodeCount;

    if ( sAggrNodeCount > 0 )
    {
        IDE_TEST(  qtc::nextTable( & sTupleID,
                                   aStatement,
                                   NULL,
                                   ID_TRUE,
                                   MTC_COLUMN_NOTNULL_TRUE )
                   != IDE_SUCCESS );

        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

        sROLL->distNode = NULL;
        sFirstMtrNode   = NULL;
        sColumnCount    = 0;
        for ( sItrAttr  = aPlan->resultDesc;
              sItrAttr != NULL;
              sItrAttr  = sItrAttr->next )
        {
            if ( ( (sItrAttr->flag & QMC_ATTR_GROUP_BY_EXT_MASK)
                   == QMC_ATTR_GROUP_BY_EXT_TRUE ) &&
                 ( (sItrAttr->flag & QMC_ATTR_KEY_MASK)
                   == QMC_ATTR_KEY_FALSE ) )
            {
                IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                                  aQuerySet,
                                                  sItrAttr->expr,
                                                  ID_FALSE,
                                                  sTupleID,
                                                  0,
                                                  & sColumnCount,
                                                  & sNewMtrNode )
                          != IDE_SUCCESS );

                if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
                     == QMO_MAKESORT_VALUE_TEMP_FALSE )
                {
                    // �������� �� table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
                    sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
                    sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;
                }
                else
                {
                    /* Nothing to do */
                }
                sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE;

                if ( ( sItrAttr->expr->node.lflag & MTC_NODE_DISTINCT_MASK )
                     == MTC_NODE_DISTINCT_FALSE )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                    sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_FALSE;
                    sNewMtrNode->myDist = NULL;
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                    sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_TRUE;
                    sNewMtrNode->flag &= ~QMC_MTR_DIST_DUP_MASK;
                    sNewMtrNode->flag |= QMC_MTR_DIST_DUP_TRUE;

                    IDE_TEST( qmg::makeDistNode( aStatement,
                                                 aQuerySet,
                                                 sROLL->plan.flag,
                                                 aChildPlan,
                                                 sTupleID,
                                                 aDistAggArg,
                                                 sItrAttr->expr,
                                                 sNewMtrNode,
                                                 & sROLL->distNode,
                                                 & sDistNodeCount )
                              != IDE_SUCCESS );
                }
                if ( sFirstMtrNode == NULL )
                {
                    sFirstMtrNode = sNewMtrNode;
                    sLastMtrNode  = sNewMtrNode;
                }
                else
                {
                    sLastMtrNode->next = sNewMtrNode;
                    sLastMtrNode       = sNewMtrNode;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
        sROLL->aggrNode      = sFirstMtrNode;
        sROLL->distNodeCount = sDistNodeCount;
        IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                               sMtcTemplate,
                                               sTupleID,
                                               sColumnCount )
                  != IDE_SUCCESS );

        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

        IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sROLL->aggrNode )
                  != IDE_SUCCESS );
        if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
             == QMO_MAKESORT_VALUE_TEMP_FALSE )
        {
            IDE_TEST( qmg::setColumnLocate( aStatement, sROLL->aggrNode )
                      != IDE_SUCCESS );
            IDE_TEST( qmg::setCalcLocate( aStatement, sROLL->aggrNode )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        sROLL->aggrNode = NULL;
        sROLL->distNode = NULL;
        sROLL->distNodeCount = 0;
    }

    /* momory value temp�� ������ �ʿ䰡 ������� �̸� �����Ѵ�. */
    if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
         == QMO_MAKESORT_VALUE_TEMP_TRUE )
    {
        sFirstMtrNode  = NULL;
        IDE_TEST( makeValueTempMtrNode( aStatement,
                                        aQuerySet,
                                        aFlag,
                                        aPlan,
                                        & sFirstMtrNode,
                                        & sMemValueCount )
                  != IDE_SUCCESS );
        sROLL->valueTempNode = sFirstMtrNode;
    }
    else
    {
        sROLL->valueTempNode = NULL;
    }

    sROLL->mtrNodeOffset = sDataNodeOffset;
    sROLL->myNodeOffset  = sROLL->mtrNodeOffset +
                   idlOS::align8( ID_SIZEOF(qmdMtrNode) * sROLL->mtrNodeCount );
    sROLL->sortNodeOffset = sROLL->myNodeOffset +
                   idlOS::align8( ID_SIZEOF(qmdMtrNode) * sROLL->myNodeCount );
    sROLL->aggrNodeOffset = sROLL->sortNodeOffset +
               idlOS::align8( ID_SIZEOF(qmdMtrNode) * sROLL->myNodeCount );
    sROLL->valueTempOffset = sROLL->aggrNodeOffset
            + idlOS::align8( ID_SIZEOF(qmdAggrNode) * sROLL->aggrNodeCount );

    if ( sROLL->distNodeCount > 0 )
    {
        sROLL->distNodePtrOffset = sROLL->valueTempOffset +
                idlOS::align8( ID_SIZEOF(qmdMtrNode) * sMemValueCount );
        sROLL->distNodeOffset = sROLL->distNodePtrOffset +
                   idlOS::align8( ID_SIZEOF(qmdDistNode *) * sROLL->groupCount );
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize =
                   sROLL->distNodeOffset +
                   idlOS::align8( ID_SIZEOF(qmdDistNode) * sDistNodeCount *
                                  sROLL->groupCount );
    }
    else
    {
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize =
                   sROLL->valueTempOffset +
                   idlOS::align8( ID_SIZEOF(qmdMtrNode) * sMemValueCount );
    }

    sMtrNode[0] = sROLL->mtrNode;
    sMtrNode[1] = sROLL->aggrNode;
    sMtrNode[2] = sROLL->distNode;
    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sROLL->plan,
                                            QMO_ROLL_DEPENDENCY,
                                            sTupleID,
                                            NULL,
                                            0,
                                            NULL,
                                            3,
                                            sMtrNode )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sROLL->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sROLL->depTupleID = (UShort)sROLL->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sROLL->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sROLL->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sROLL->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sROLL->planID,
                               sROLL->plan.flag,
                               sMtcTemplate->rows[sTempTuple].lflag,
                               sROLL->valueTempNode,
                               & sROLL->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_SUBQUERY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDB_NOT_ALLOWED_SUBQUERY,
                                  "ROLLUP,CUBE Aggregation" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeROLL",
                                  "Invalid dependency " ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeAggrArgumentsMtr
 *
 *   Aggregation�� MTR ��� �� �����ϰ� ������ ����� TupleID�� �����Ѵ�.
 *   memory�� �ϰ��� pointer��
 *   �װ� �ǰ� Aggregation���� ���Ǵ� Arguments�� pointer�� �����ؼ� ����ϰ� �ǹǷ�
 *   �� ����� ���� �ȴ�.
 *   ������ Disk�� ��쿡 value�� �о����� �Ǵµ� �̶� ������ ���� ��찡 ���� �� �ִ�.
 *
 *   | i1 | i2 | i3 |
 *   ���� ���� sortNode�� �����Ǹ� myNode�� setColumnLocate �� TupleID��
 *   �����ϸ� Aggr(i2)�� i2�� dstTuple ���� myNode�� TupleID�� ����ȴ�.
 *   �׷��� �Ǹ� myNode�� i2 �������� NULL�� �� �� �� �ְ� �ʵ� �� �� �ִ�.
 *   �׷��� Aggr�� Arguemnts�� i2�� �׻� ���� ������ �־�� �Ѵ�.
 *   ���� ���� ���� Disk Sort Temp�� ���
 *   | i1 | i2 | i3 | i2 | �� ���� �����ؼ� Aggr�� Arguments�� i2�� ������ value
 *   �� ���� ���ش�. �׷��� Aggregaion�� Arguments�� TupleID�� ��� ���� ������
 *   TupleID�� �ٲ��ִ� �۾��� �� �Լ����� �ϰ� �ȴ�.
 *
 *   �� no_push_projection �ΰ�� RID�� �װ� �Ǵ� �� �� �� �����͸� �װ�
 *   ���ش�.
 */
IDE_RC qmoOneMtrPlan::makeAggrArgumentsMtrNode( qcStatement * aStatement,
                                                qmsQuerySet * aQuerySet,
                                                UShort        aTupleID,
                                                UShort      * aColumnCount,
                                                mtcNode     * aNode,
                                                qmcMtrNode ** aFirstMtrNode )
{
    mtcTemplate * sMtcTemplate;
    qmcMtrNode  * sLastMtrNode;
    qmcMtrNode  * sNewMtrNode;
    ULong         sFlag;
    idBool        sIsConnectByFunc = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeAggrArgumentsMtrNode::__FT__" );

    sMtcTemplate = &QC_SHARED_TMPLATE( aStatement )->tmplate;

    /* BUG-38495 Rollup error with subquery */
    IDE_TEST_CONT( aNode->module == &qtc::subqueryModule, NORMAL_EXIT );

    if ( aNode->module == &qtc::columnModule )
    {
        for ( sLastMtrNode        = *aFirstMtrNode;
              sLastMtrNode->next != NULL;
              sLastMtrNode        = sLastMtrNode->next );

        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           (qtcNode *)aNode,
                                           aTupleID,
                                           ID_FALSE )
                  != IDE_SUCCESS );
        if( qmg::getMtrMethod( aStatement,
                               aNode->table,
                               aTupleID ) == ID_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              (qtcNode *)aNode,
                                              ID_FALSE,
                                              aTupleID,
                                              0,
                                              aColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
        else
        {
            if ( sLastMtrNode->srcNode == NULL )
            {
                IDE_TEST( qmg::makeBaseTableMtrNode( aStatement,
                                                     (qtcNode *)aNode,
                                                     aTupleID,
                                                     aColumnCount,
                                                     & sNewMtrNode )
                          != IDE_SUCCESS );
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            else
            {
                sFlag = sMtcTemplate->rows[aNode->table].lflag;
                /* Surrogate-key(RID �Ǵ� pointer)�� �����Ѵ�.
                 * �̹� surrogate-key�� �߰��Ǿ��ִ��� Ȯ���Ѵ�.
                 */
                if( qmg::existBaseTable( (*aFirstMtrNode)->next,
                                         qmg::getBaseTableType( sFlag ),
                                         aNode->table )
                    == ID_TRUE )
                {
                    /* Nothing to do */
                }
                else
                {
                    IDE_TEST( qmg::makeBaseTableMtrNode( aStatement,
                                                         (qtcNode *)aNode,
                                                         aTupleID,
                                                         aColumnCount,
                                                         & sNewMtrNode )
                              != IDE_SUCCESS );
                    sLastMtrNode->next = sNewMtrNode;
                    sLastMtrNode       = sNewMtrNode;
                }
            }
        }
        /* BUG-37218 wrong result rollup when rollup use memory sort temp in disk table */
        aNode->lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
        aNode->lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE;
    }
    else
    {
        /* BUG-39611 support SYS_CONNECT_BY_PATH�� expression arguments
         * CONNECT BY  ������ ROLLUP���� ���� ����Ǵ� �����̰�,
         * SYS_CONNECT_BY_PATH�� ���� ��� CONNECT BY ���� ����� ���Ǿ���ϱ�
         * ������ ROLL UP������ �̸� �׾Ƽ� ó���ؾ��Ѵ�. ���� �Ʒ��� ����
         * MTR NODE�� ������ �ָ� ROLLUP������ SYS_CONNECT_BY_PATH�� �����
         * ���̰� aggregation������ �� ����� �����Ͽ� Calculate�� �����Ѵ�.
         */
        if ( ( aNode->lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
             == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            for ( sLastMtrNode        = *aFirstMtrNode;
                  sLastMtrNode->next != NULL;
                  sLastMtrNode        = sLastMtrNode->next );

            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              (qtcNode *)aNode,
                                              ID_FALSE,
                                              aTupleID,
                                              0,
                                              aColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE;

            /* BUG-39611 CONNECT_BY_FUNC�� ���� ���̹Ƿ� valueModuel�� �����Ѵ�. */
            sNewMtrNode->dstNode->node.module = &qtc::valueModule;

            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;

            sIsConnectByFunc = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( aNode->next != NULL )
    {
        IDE_TEST( makeAggrArgumentsMtrNode( aStatement,
                                            aQuerySet,
                                            aTupleID,
                                            aColumnCount,
                                            aNode->next,
                                            aFirstMtrNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( aNode->arguments != NULL ) &&
         ( sIsConnectByFunc == ID_FALSE ) )
    {
        IDE_TEST( makeAggrArgumentsMtrNode( aStatement,
                                            aQuerySet,
                                            aTupleID,
                                            aColumnCount,
                                            aNode->arguments,
                                            aFirstMtrNode )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 initCUBE
 *
 *  Cube�� Result Descript�� �߰��Ѵ�.
 *  GROUP BY �÷��� ���� �߰��ϰ� Cube �÷��� �߰��Ѵ�.
 *  Aggregation �� �߰��Ѵ�.
 */
IDE_RC qmoOneMtrPlan::initCUBE( qcStatement      * aStatement,
                                qmsQuerySet      * aQuerySet,
                                UInt             * aCubeCount,
                                qmsAggNode       * aAggrNode,
                                qmsConcatElement * aGroupColumn,
                                qmnPlan         ** aPlan )
{
    qmncCUBE         * sCUBE           = NULL;
    qmsConcatElement * sGroup          = NULL;
    qmsConcatElement * sSubGroup       = NULL;
    qmsAggNode       * sAggrNode       = NULL;
    qtcNode          * sNode           = NULL;
    qtcNode          * sListNode       = NULL;
    UInt               sDataNodeOffset = 0;
    UInt               sFlag           = 0;
    UInt               sCubeCount      = 0;
    UInt               sColumnCount    = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::initCUBE::__FT__" );

    IDE_DASSERT( aStatement != NULL );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCUBE ),
                                               (void **)& sCUBE )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sCUBE,
                        QC_SHARED_TMPLATE( aStatement),
                        QMN_CUBE,
                        qmnCUBE,
                        qmndCUBE,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sCUBE;

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
    sFlag |= QMC_ATTR_SORT_ORDER_ASC;

    for ( sGroup  = aGroupColumn;
          sGroup != NULL;
          sGroup  = sGroup->next )
    {
        if ( sGroup->type == QMS_GROUPBY_NORMAL )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sCUBE->plan.resultDesc,
                                            sGroup->arithmeticOrList,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE |
                                            QMC_APPEND_ALLOW_CONST_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
            ++sColumnCount;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sFlag &= ~QMC_ATTR_GROUP_BY_EXT_MASK;
    sFlag |= QMC_ATTR_GROUP_BY_EXT_TRUE;

    for ( sGroup = aGroupColumn; sGroup != NULL; sGroup = sGroup->next )
    {
        if ( sGroup->type != QMS_GROUPBY_NORMAL )
        {
            for ( sSubGroup  = sGroup->arguments;
                  sSubGroup != NULL;
                  sSubGroup  = sSubGroup->next )
            {
                sSubGroup->type = sGroup->type;
                if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    for ( sListNode  = (qtcNode *)sSubGroup->arithmeticOrList->node.arguments;
                          sListNode != NULL;
                          sListNode  = (qtcNode *)sListNode->node.next )
                    {
                        IDE_TEST( qmc::appendAttribute( aStatement,
                                                        aQuerySet,
                                                        & sCUBE->plan.resultDesc,
                                                        sListNode,
                                                        sFlag,
                                                        QMC_APPEND_EXPRESSION_TRUE |
                                                        QMC_APPEND_ALLOW_DUP_TRUE |
                                                        QMC_APPEND_ALLOW_CONST_TRUE,
                                                        ID_FALSE )
                                  != IDE_SUCCESS );
                        ++sColumnCount;
                    }
                }
                else
                {
                    IDE_TEST( qmc::appendAttribute( aStatement,
                                                    aQuerySet,
                                                    & sCUBE->plan.resultDesc,
                                                    sSubGroup->arithmeticOrList,
                                                    sFlag,
                                                    QMC_APPEND_EXPRESSION_TRUE |
                                                    QMC_APPEND_ALLOW_DUP_TRUE  |
                                                    QMC_APPEND_ALLOW_CONST_TRUE,
                                                    ID_FALSE )
                              != IDE_SUCCESS );
                    ++sColumnCount;
                }
                ++sCubeCount;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    sFlag = 0;
    sFlag &= ~QMC_ATTR_GROUP_BY_EXT_MASK;
    sFlag |= QMC_ATTR_GROUP_BY_EXT_TRUE;

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while( sNode->node.module == &qtc::passModule )
        {
            sNode = ( qtcNode *)sNode->node.arguments;
        }

        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if ( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
             ( sNode->overClause == NULL ) )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sCUBE->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE |
                                            QMC_APPEND_ALLOW_DUP_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_TEST_RAISE( ( sCubeCount > QMO_MAX_CUBE_COUNT ) ||
                    ( sColumnCount > QMO_MAX_CUBE_COLUMN_COUNT ),
                    ERR_INVALID_COUNT );

    /* PROJ-2462 Result Cache */
    IDE_TEST( qmo::initResultCacheStack( aStatement,
                                         aQuerySet,
                                         sCUBE->planID,
                                         ID_FALSE,
                                         ID_FALSE )
              != IDE_SUCCESS );

    *aCubeCount = sCubeCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_COUNT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_MAX_CUBE_COUNT ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeCUBE
 *
 *    CUBE Plan�� ������ memory Table�̸� pointer�� �װ� disk�� value�� Sort Temp��
 *    �׾Ƽ� Sort�� ������ �����ϴ� �������� Cube�� �����Ѵ�.
 *
 *    Cube( �÷� ) �� 2^n ��ŭ�� �׷���� ������ �ȴ�. �÷��� 10���� 2�� 10���̹Ƿ�
 *    1024���� �׷��� ���� �ϰ� �ȴ�. �ִ� �÷� ������ 15���� �����Ѵ�.
 *    Cube�� ( 2^(n-1) )��ŭ sort�� �����ϰ� �ȴ�.
 *
 *  - CUBE Plan�� 5���� Tuple�� mtr Node�� ���� �� �� �ִ�.
 *  - mtrNode  - Sort�뵵�� ���ȴ�.
 *  - myNode   - ���� PLAN���� ���� NODE�̴�.
 *  - aggrNode - aggregation MTR NODE
 *  - distNode - aggregation�� distinct�� ������ ���� NODE
 *  - valueTempNode - value store�� TEMP NODE
 */
IDE_RC qmoOneMtrPlan::makeCUBE( qcStatement      * aStatement,
                                qmsQuerySet      * aQuerySet,
                                UInt               aFlag,
                                UInt               aCubeCount,
                                qmsAggNode       * aAggrNode,
                                qmoDistAggArg    * aDistAggArg,
                                qmsConcatElement * aGroupColumn,
                                qmnPlan          * aChildPlan ,
                                qmnPlan          * aPlan )
{
    mtcTemplate      * sMtcTemplate    = NULL;
    qmncCUBE         * sCUBE           = NULL;
    qmcAttrDesc      * sItrAttr        = NULL;
    qmcMtrNode       * sNewMtrNode     = NULL;
    qmcMtrNode       * sFirstMtrNode   = NULL;
    qmcMtrNode       * sLastMtrNode    = NULL;
    qmcMtrNode         sDummyNode;
    qmsAggNode       * sAggrNode       = NULL;
    qmsConcatElement * sGroupBy;
    qmsConcatElement * sSubGroup;
    qmcMtrNode       * sMtrNode[3];
    qtcNode          * sNode;

    UShort         sTupleID        = 0;
    UShort         sColumnCount    = 0;
    UShort         sAggrNodeCount  = 0;
    UShort         sDistNodeCount  = 0;
    UShort         sCount          = 0;
    UShort         sMemValueCount  = 0;
    UInt           sDataNodeOffset = 0;
    SInt           sCubeStart      = -1;
    idBool         sIsPartial      = ID_FALSE;
    UInt           i;
    UInt           sTempTuple      = 0;

    IDU_FIT_POINT_FATAL( "qmoOneMtrPlan::makeCUBE::__FT__" );

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aGroupColumn != NULL );
    IDE_DASSERT( aChildPlan   != NULL );
    IDE_DASSERT( aPlan        != NULL );

    sMtcTemplate    = &QC_SHARED_TMPLATE( aStatement )->tmplate;
    aPlan->offset   = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset + ID_SIZEOF( qmndCUBE ));

    sCUBE            = ( qmncCUBE * )aPlan;
    sCUBE->plan.left = aChildPlan;
    sCUBE->flag      = QMN_PLAN_FLAG_CLEAR;
    sCUBE->plan.flag = QMN_PLAN_FLAG_CLEAR;
    sCUBE->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->groupingInfoAddr,
                                    & sCUBE->groupingFuncRowID )
              != IDE_SUCCESS );

    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sAggrNodeCount++;
        sNode = sAggrNode->aggr;
        if ( sNode->node.arguments != NULL )
        {
            IDE_TEST_RAISE ( ( sNode->node.arguments->lflag & MTC_NODE_OPERATOR_MASK )
                             == MTC_NODE_OPERATOR_SUBQUERY,
                             ERR_NOT_ALLOW_SUBQUERY );
        }
        else
        {
            /* Nothing to do */
        }
    }

    sTempTuple = sTupleID;
    if( (aFlag & QMO_MAKESORT_TEMP_TABLE_MASK) ==
        QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sCUBE->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sCUBE->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

        /* PROJ-2462 Result Cache */
        if ( ( QC_SHARED_TMPLATE( aStatement )->resultCache.flag & QC_RESULT_CACHE_MASK )
             == QC_RESULT_CACHE_ENABLE )
        {
            if ( aQuerySet->SFWGH->hints->resultCacheType
                 != QMO_RESULT_CACHE_NO )
            {
                // Result Cache������ �׻� Value Temp�� �����ϵ��� �����Ѵ�.
                aFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                aFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( aQuerySet->SFWGH->hints->resultCacheType
                 == QMO_RESULT_CACHE )
            {
                // Result Cache������ �׻� Value Temp�� �����ϵ��� �����Ѵ�.
                aFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                aFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        sCUBE->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sCUBE->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    for ( sItrAttr = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        if (( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            if ( ( sItrAttr->flag & QMC_ATTR_GROUP_BY_EXT_MASK )
                 == QMC_ATTR_GROUP_BY_EXT_TRUE )
            {
                if ( sCubeStart == -1 )
                {
                    sCubeStart = sCount;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                sIsPartial = ID_TRUE;
            }

            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            sNewMtrNode->flag &= ~QMC_MTR_GROUPING_MASK;
            sNewMtrNode->flag |= QMC_MTR_GROUPING_TRUE;
            sNewMtrNode->flag &= ~QMC_MTR_MTR_PLAN_MASK;
            sNewMtrNode->flag |= QMC_MTR_MTR_PLAN_TRUE;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }

    sDummyNode.next    = NULL;
    sDummyNode.srcNode = NULL;
    sNewMtrNode        = &sDummyNode;
    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while ( sNode->node.module == &qtc::passModule )
        {
            sNode = (qtcNode *)sNode->node.arguments;
        }

        if ( ( sNode->node.lflag & MTC_NODE_FUNCTON_GROUPING_MASK )
             == MTC_NODE_FUNCTON_GROUPING_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        /* Aggregate function�� �ƴ� node�� ���޵Ǵ� ��찡 �����Ѵ�. */
        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if ( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
             ( sNode->overClause == NULL ) )
        {
            if ( sNode->node.arguments != NULL )
            {
                IDE_TEST( makeAggrArgumentsMtrNode( aStatement,
                                                    aQuerySet,
                                                    sTupleID,
                                                    & sColumnCount,
                                                    sNode->node.arguments,
                                                    & sNewMtrNode )
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
    }
    if ( sDummyNode.next != NULL )
    {
        sLastMtrNode->next = sDummyNode.next;
    }
    else
    {
        /* Nothing to do */
    }

    sCUBE->mtrNode      = sFirstMtrNode;
    sCUBE->mtrNodeCount = sColumnCount;

    if ( sIsPartial == ID_TRUE )
    {
        sCUBE->partialCube = sCubeStart;
    }
    else
    {
        sCUBE->partialCube = -1;
    }

    /* Cube�� �� element�� ��� �÷����� �����Ǿ��ִ����� �����Ѵ�. */
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ( ID_SIZEOF( UShort ) * aCubeCount ),
                                             (void**) & sCUBE->elementCount )
              != IDE_SUCCESS );

    for ( sGroupBy  = aGroupColumn;
          sGroupBy != NULL;
          sGroupBy  = sGroupBy->next )
    {
        if ( sGroupBy->type == QMS_GROUPBY_CUBE )
        {
            for ( sSubGroup  = sGroupBy->arguments, i = 0;
                  sSubGroup != NULL;
                  sSubGroup  = sSubGroup->next, i++ )
            {
                if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    sCUBE->elementCount[i] = sSubGroup->arithmeticOrList->node.lflag &
                        MTC_NODE_ARGUMENT_COUNT_MASK;
                }
                else
                {
                    sCUBE->elementCount[i] = 1;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sTupleID,
                                           sColumnCount )
              != IDE_SUCCESS );

    if( (aFlag & QMO_MAKESORT_TEMP_TABLE_MASK) ==
        QMO_MAKESORT_MEMORY_TEMP_TABLE )
    {
        sCUBE->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sCUBE->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;
    }
    else
    {
        sCUBE->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sCUBE->plan.flag  |= QMN_PLAN_STORAGE_DISK;
        sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_DISK;

        if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
        {
            sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
        }
        else
        {
            // Nothing To Do
        }
    }
    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sCUBE->mtrNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sCUBE->mtrNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setCalcLocate( aStatement, sCUBE->mtrNode )
              != IDE_SUCCESS );

    /* sCUBE myNode */
    IDE_TEST( qtc::nextTable( & sTupleID,
                              aStatement,
                              NULL,
                              ID_TRUE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    sFirstMtrNode = NULL;
    sCount        = 0;
    sColumnCount  = 0;
    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                              aQuerySet,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sTupleID,
                                              0,
                                              & sColumnCount,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );
            if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
                 == QMO_MAKESORT_VALUE_TEMP_FALSE )
            {
                // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
                sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
                sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;
            }
            else
            {
                /* Nothing to do */
            }
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

            // PROJ-2362 memory temp ���� ȿ���� ����
            // myNode�� temp�� ������ �ʴ´�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if ( sFirstMtrNode == NULL )
            {
                sFirstMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            sCount++;
        }
        else
        {
            /* Nothing to do */
        }
    }
    sCUBE->myNode      = sFirstMtrNode;
    sCUBE->myNodeCount = sCount;

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sTupleID,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sCUBE->myNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sCUBE->myNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setCalcLocate( aStatement, sCUBE->myNode )
              != IDE_SUCCESS );

    /* aggrNode */
    sCUBE->aggrNodeCount = sAggrNodeCount;
    sCUBE->groupCount    = 0x1 << aCubeCount;
    sCUBE->cubeCount     = aCubeCount;

    if ( sAggrNodeCount > 0 )
    {
        IDE_TEST( qtc::nextTable( & sTupleID,
                                  aStatement,
                                  NULL,
                                  ID_TRUE,
                                  MTC_COLUMN_NOTNULL_TRUE )
                  != IDE_SUCCESS );

         sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
         sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

         sCUBE->distNode = NULL;
         sFirstMtrNode   = NULL;
         sColumnCount    = 0;
         sAggrNode = aAggrNode;
         for ( sItrAttr  = aPlan->resultDesc;
               sItrAttr != NULL;
               sItrAttr  = sItrAttr->next )
         {
             if ( ( ( sItrAttr->flag & QMC_ATTR_GROUP_BY_EXT_MASK)
                    == QMC_ATTR_GROUP_BY_EXT_TRUE ) &&
                  ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK)
                    == QMC_ATTR_KEY_FALSE ) )

             {
                 IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                                   aQuerySet,
                                                   sItrAttr->expr,
                                                   ID_FALSE,
                                                   sTupleID,
                                                   0,
                                                   & sColumnCount,
                                                   & sNewMtrNode )
                           != IDE_SUCCESS );

                 if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
                      == QMO_MAKESORT_VALUE_TEMP_FALSE )
                 {
                     sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
                     sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;
                 }
                 else
                 {
                     /* Nothing to do */
                 }

                 sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                 sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE;

                 if ( ( sItrAttr->expr->node.lflag & MTC_NODE_DISTINCT_MASK )
                      == MTC_NODE_DISTINCT_FALSE )
                 {
                     sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                     sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_FALSE;
                     sNewMtrNode->myDist = NULL;
                 }
                 else
                 {
                     sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                     sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_TRUE;
                     sNewMtrNode->flag &= ~QMC_MTR_DIST_DUP_MASK;
                     sNewMtrNode->flag |= QMC_MTR_DIST_DUP_TRUE;

                     IDE_TEST( qmg::makeDistNode( aStatement,
                                                  aQuerySet,
                                                  sCUBE->plan.flag,
                                                  aChildPlan,
                                                  sTupleID,
                                                  aDistAggArg,
                                                  sItrAttr->expr,
                                                  sNewMtrNode,
                                                  & sCUBE->distNode,
                                                  & sDistNodeCount )
                               != IDE_SUCCESS );
                 }
                 if ( sFirstMtrNode == NULL )
                 {
                     sFirstMtrNode = sNewMtrNode;
                     sLastMtrNode  = sNewMtrNode;
                 }
                 else
                 {
                     sLastMtrNode->next = sNewMtrNode;
                     sLastMtrNode       = sNewMtrNode;
                 }
                 sAggrNode = sAggrNode->next;
             }
             else
             {
                 /* Nothing to do */
             }
         }
         sCUBE->aggrNode      = sFirstMtrNode;
         sCUBE->distNodeCount = sDistNodeCount;
         IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                                sMtcTemplate,
                                                sTupleID,
                                                sColumnCount )
                   != IDE_SUCCESS );

         sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
         sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;
         sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
         sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;
         sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
         sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

         IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sCUBE->aggrNode )
                   != IDE_SUCCESS );
         if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
              == QMO_MAKESORT_VALUE_TEMP_FALSE )
         {
             IDE_TEST( qmg::setColumnLocate( aStatement, sCUBE->aggrNode )
                       != IDE_SUCCESS );
             IDE_TEST( qmg::setCalcLocate( aStatement, sCUBE->aggrNode )
                       != IDE_SUCCESS );
         }
         else
         {
             /* Nothing to do */
         }
    }
    else
    {
        sCUBE->aggrNode = NULL;
        sCUBE->distNode = NULL;
        sCUBE->distNodeCount = 0;
    }

    if ( ( aFlag & QMO_MAKESORT_VALUE_TEMP_MASK )
         == QMO_MAKESORT_VALUE_TEMP_TRUE )
    {
        sFirstMtrNode  = NULL;
        IDE_TEST( makeValueTempMtrNode( aStatement,
                                        aQuerySet,
                                        aFlag,
                                        aPlan,
                                        & sFirstMtrNode,
                                        & sMemValueCount )
                  != IDE_SUCCESS );
        sCUBE->valueTempNode = sFirstMtrNode;
    }
    else
    {
        sCUBE->valueTempNode = NULL;
    }
    sCUBE->mtrNodeOffset = sDataNodeOffset;
    sCUBE->myNodeOffset  = sCUBE->mtrNodeOffset +
        idlOS::align8(ID_SIZEOF(qmdMtrNode) * sCUBE->mtrNodeCount);
    sCUBE->sortNodeOffset = sCUBE->myNodeOffset +
        idlOS::align8(ID_SIZEOF(qmdMtrNode) * sCUBE->myNodeCount);
    sCUBE->aggrNodeOffset = sCUBE->sortNodeOffset +
        idlOS::align8(ID_SIZEOF(qmdMtrNode) * sCUBE->myNodeCount);
    sCUBE->valueTempOffset = sCUBE->aggrNodeOffset +
        idlOS::align8( ID_SIZEOF(qmdAggrNode) * sCUBE->aggrNodeCount );

    if ( sCUBE->distNodeCount > 0 )
    {
        sCUBE->distNodePtrOffset = sCUBE->valueTempOffset +
                idlOS::align8( ID_SIZEOF(qmdMtrNode) * sMemValueCount );
        sCUBE->distNodeOffset = sCUBE->distNodePtrOffset +
                idlOS::align8( ID_SIZEOF(qmdDistNode *) * ( sCUBE->cubeCount + 1 ) );
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize =
            sCUBE->distNodeOffset +
            idlOS::align8( ID_SIZEOF(qmdDistNode) * sCUBE->distNodeCount *
            ( sCUBE->cubeCount + 1 ) );
    }
    else
    {
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize = sCUBE->valueTempOffset +
                    idlOS::align8( ID_SIZEOF(qmdMtrNode) * sMemValueCount );
    }

    sMtrNode[0] = sCUBE->mtrNode;
    sMtrNode[1] = sCUBE->aggrNode;
    sMtrNode[2] = sCUBE->distNode;
    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sCUBE->plan,
                                            QMO_CUBE_DEPENDENCY,
                                            sTupleID,
                                            NULL,
                                            0,
                                            NULL,
                                            3,
                                            sMtrNode )
              != IDE_SUCCESS );
    IDE_TEST_RAISE( sCUBE->plan.dependency == ID_UINT_MAX,
                    ERR_INVALID_DEPENDENCY );

    sCUBE->depTupleID = (UShort)sCUBE->plan.dependency;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sCUBE->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sCUBE->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCUBE->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    /* PROJ-2462 Result Cache */
    qmo::makeResultCacheStack( aStatement,
                               aQuerySet,
                               sCUBE->planID,
                               sCUBE->plan.flag,
                               sMtcTemplate->rows[sTempTuple].lflag,
                               sCUBE->valueTempNode,
                               &sCUBE->componentInfo,
                               ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_SUBQUERY )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDB_NOT_ALLOWED_SUBQUERY,
                                 "ROLLUP,CUBE Aggregation" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_DEPENDENCY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoOneMtrPlan::makeCUBE",
                                  "Invalid dependency " ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::initCNBYForJoin( qcStatement    * aStatement,
                                       qmsQuerySet    * aQuerySet,
                                       qmoLeafInfo    * aLeafInfo,
                                       qmsSortColumns * aSiblings,
                                       qmnPlan        * aParent,
                                       qmnPlan       ** aPlan )
{
    qmncCNBY       * sCNBY           = NULL;
    UInt             sDataNodeOffset = 0;
    UInt             sFlag           = 0;
    qtcNode        * sNode           = NULL;
    qmsSortColumns * sSibling        = NULL;
    qmcAttrDesc    * sItrAttr        = NULL;
    idBool           sIsTrue         = ID_FALSE;

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCNBY ),
                                               (void **)& sCNBY )
              != IDE_SUCCESS );

    idlOS::memset( sCNBY, 0x00, ID_SIZEOF(qmncCNBY));
    QMO_INIT_PLAN_NODE( sCNBY,
                        QC_SHARED_TMPLATE( aStatement),
                        QMN_CNBY,
                        qmnCNBY,
                        qmndCNBY,
                        sDataNodeOffset );

    // CONNECT BY�� �߰�
    // PROJ-2469 Optimize View Materialization
    // BUG FIX : aLeafInfo[0] -> aLeafInfo[1]�� ����
    //                           CONNECT BY �� Predicate�� ������� �ʰ� START WITH�� �� ��
    //                           ����ϰ� �־���.
    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    & sCNBY->plan.resultDesc,
                                    aLeafInfo[1].predicate,
                                    sFlag )
              != IDE_SUCCESS );

    // START WITH�� �߰�
    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    & sCNBY->plan.resultDesc,
                                    aLeafInfo[0].predicate,
                                    sFlag )
              != IDE_SUCCESS );

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    for ( sSibling  = aSiblings;
          sSibling != NULL;
          sSibling  = sSibling->next )
    {
        if ( aSiblings->isDESC == ID_FALSE )
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_ASC;
        }
        else
        {
            sFlag &= ~QMC_ATTR_SORT_ORDER_MASK;
            sFlag |= QMC_ATTR_SORT_ORDER_DESC;
        }

        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sCNBY->plan.resultDesc,
                                        sSibling->sortColumn,
                                        sFlag,
                                        QMC_APPEND_EXPRESSION_TRUE,
                                        ID_FALSE )
                  != IDE_SUCCESS );
    }

    /**
     * select connect_by_root(t1.i1) from t1 left outer join t2 on t1.i2 = t2.i2 + 1 
     *  connect by prior t2.i1 = t1.i1 order by t1.i1;
     *  order by �� connect_by_root or sys_connect_by_path�� ���� ���Ȱ��
     *  sort���� �׾Ƽ� ó���ؾ��ϱ� ������ SEALED_TRUE�� expression�� ��ü��
     *  �ְԵȴ�. ������ �̴� sort���� ����ϱ� ���ؼ����� hierarchy query������
     *  �ƴϹǷ� �ɷ�����ϱ� ������ �Ʒ��� ���� ��� SEALED_FALSE�� parent�� 
     *  ��ȯ�ߴٰ� �ٽ� ������Ų��.
     */
    for ( sItrAttr = aParent->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        if ( ( sItrAttr->expr->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
               == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            if ( ( sItrAttr->flag & QMC_ATTR_SEALED_MASK )
                 == QMC_ATTR_SEALED_TRUE )
            {
                sIsTrue = ID_TRUE;
                sItrAttr->flag &= ~QMC_ATTR_SEALED_MASK;
                sItrAttr->flag |= QMC_ATTR_SEALED_FALSE;
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
    }

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sCNBY->plan.resultDesc )
              != IDE_SUCCESS );

    if ( sIsTrue == ID_TRUE )
    {
        for ( sItrAttr = aParent->resultDesc;
              sItrAttr != NULL;
              sItrAttr = sItrAttr->next )
        {
            if ( ( sItrAttr->expr->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
                   == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
            {
                sItrAttr->flag &= ~QMC_ATTR_SEALED_MASK;
                sItrAttr->flag |= QMC_ATTR_SEALED_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* Nothing to do */
    }
    sFlag &= ~QMC_ATTR_KEY_MASK;

    for ( sItrAttr  = sCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( ( sItrAttr->expr->lflag & QTC_NODE_LEVEL_MASK ) == QTC_NODE_LEVEL_EXIST ) ||
             ( ( sItrAttr->expr->lflag & QTC_NODE_ISLEAF_MASK ) == QTC_NODE_ISLEAF_EXIST ) )
        {
            sItrAttr->flag &= ~QMC_ATTR_TERMINAL_MASK;
            sItrAttr->flag |= QMC_ATTR_TERMINAL_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sItrAttr->expr->lflag & QTC_NODE_PRIOR_MASK ) == QTC_NODE_PRIOR_EXIST )
        {
            sItrAttr->flag &= ~QMC_ATTR_TERMINAL_MASK;
            sItrAttr->flag |= QMC_ATTR_TERMINAL_TRUE;

            IDU_FIT_POINT( "qmoOneMtrPlan::initCNBYForJoin::alloc::sNode",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                       (void**)&sNode )
                      != IDE_SUCCESS );

            idlOS::memcpy( sNode, sItrAttr->expr, ID_SIZEOF( qtcNode ) );

            sNode->lflag &= ~QTC_NODE_PRIOR_MASK;
            sNode->lflag |= QTC_NODE_PRIOR_ABSENT;
            sNode->depInfo.depCount = 1;
            sNode->depInfo.depend[0] = sNode->node.table;
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sCNBY->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            0,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

    }

    *aPlan = (qmnPlan *)sCNBY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneMtrPlan::makeCNBYForJoin( qcStatement    * aStatement,
                                       qmsQuerySet    * aQuerySet,
                                       qmsSortColumns * aSiblings,
                                       qmoLeafInfo    * aStartWith,
                                       qmoLeafInfo    * aConnectBy,
                                       qmnPlan        * aChildPlan,
                                       qmnPlan        * aPlan )
{
    mtcTemplate  * sMtcTemplate    = NULL;
    qmncCNBY     * sCNBY           = (qmncCNBY *)aPlan;
    UShort         sPriorID        = 0;
    UShort         sColumnCount    = 0;
    UShort         sOrgTable       = 0;
    UShort         sOrgColumn      = 0;
    UInt           sDataNodeOffset = 0;
    UInt           i               = 0;
    UInt           sCurOffset      = 0;
    qmcAttrDesc  * sItrAttr        = NULL;
    qmcMtrNode   * sNewMtrNode     = NULL;
    qmcMtrNode   * sLastMtrNode    = NULL;
    qmcMtrNode   * sLastMtrNode2   = NULL;
    qmoPredicate * sPred           = NULL;
    qmoPredicate * sPredMore       = NULL;
    ULong          sFlag           = 0;
    qtcNode      * sPredicate[9];

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    sMtcTemplate = &QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8(aPlan->offset + ID_SIZEOF(qmndCNBY));

    sCNBY->plan.left     = aChildPlan;
    sCNBY->mtrNodeOffset = sDataNodeOffset;

    sCNBY->flag             = QMN_PLAN_FLAG_CLEAR;
    sCNBY->plan.flag        = QMN_PLAN_FLAG_CLEAR;
    sCNBY->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //loop�� ã�� �������� ���� ����
    if ( ( aQuerySet->SFWGH->hierarchy->flag & QMS_HIERARCHY_IGNORE_LOOP_MASK ) ==
           QMS_HIERARCHY_IGNORE_LOOP_TRUE )
    {
        sCNBY->flag &= ~QMNC_CNBY_IGNORE_LOOP_MASK;
        sCNBY->flag |= QMNC_CNBY_IGNORE_LOOP_TRUE;
    }
    else
    {
        sCNBY->flag &= ~QMNC_CNBY_IGNORE_LOOP_MASK;
        sCNBY->flag |= QMNC_CNBY_IGNORE_LOOP_FALSE;
    }

    sCNBY->flag &= ~QMNC_CNBY_CHILD_VIEW_MASK;
    sCNBY->flag |= QMNC_CNBY_CHILD_VIEW_FALSE;
    sCNBY->flag &= ~QMNC_CNBY_JOIN_MASK;
    sCNBY->flag |= QMNC_CNBY_JOIN_TRUE;

    if ( aSiblings != NULL )
    {
        sCNBY->flag &= ~QMNC_CNBY_SIBLINGS_MASK;
        sCNBY->flag |= QMNC_CNBY_SIBLINGS_TRUE;
    }
    else
    {
        sCNBY->flag &= ~QMNC_CNBY_SIBLINGS_MASK;
        sCNBY->flag |= QMNC_CNBY_SIBLINGS_FALSE;
    }
    IDE_TEST( qtc::nextTable( &sCNBY->myRowID,
                              aStatement,
                              NULL,
                              ID_FALSE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );
    sCNBY->mIndex       = NULL;
    sCNBY->mTableHandle = NULL;
    sCNBY->mTableSCN    = 0;
    sCNBY->baseRowID    = sCNBY->myRowID;

    IDE_TEST( qtc::nextTable( &sPriorID,
                              aStatement,
                              NULL,
                              ID_FALSE,
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    aQuerySet->SFWGH->hierarchy->priorTable    = sPriorID;
    sCNBY->priorRowID                          = sPriorID;

    /* BUG-48300 */
    if ( aQuerySet->SFWGH->level != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->level,
                                        & sCNBY->levelRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_LEVEL_MASK;
        sCNBY->flag |= QMNC_CNBY_LEVEL_TRUE;
    }

    /* BUG-48300 */
    if ( aQuerySet->SFWGH->isLeaf != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->isLeaf,
                                        & sCNBY->isLeafRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_ISLEAF_MASK;
        sCNBY->flag |= QMNC_CNBY_ISLEAF_TRUE;
    }
    /* BUG-48300 */
    if ( aQuerySet->SFWGH->cnbyStackAddr != NULL )
    {
        IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->cnbyStackAddr,
                                        & sCNBY->stackRowID )
                  != IDE_SUCCESS );
        sCNBY->flag &= ~QMNC_CNBY_FUNCTION_MASK;
        sCNBY->flag |= QMNC_CNBY_FUNCTION_TRUE;
    }

    IDE_TEST( setPseudoColumnRowID( aQuerySet->SFWGH->rownum,
                                    & sCNBY->rownumRowID )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sCNBY->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    for ( sItrAttr  = sCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->expr->lflag & QTC_NODE_LOB_COLUMN_MASK ) == QTC_NODE_LOB_COLUMN_EXIST )
        {
            sFlag = sMtcTemplate->rows[sItrAttr->expr->node.table].lflag;

            if ( qmg::existBaseTable( sCNBY->mBaseMtrNode,
                                      qmg::getBaseTableType( sFlag ),
                                      sItrAttr->expr->node.table )
                    == ID_TRUE )
            {
                continue;
            }
            else
            {
                IDE_TEST( qmg::makeBaseTableMtrNode( aStatement,
                                                     sItrAttr->expr,
                                                     sCNBY->myRowID,
                                                     &sColumnCount,
                                                     &sNewMtrNode )
                          != IDE_SUCCESS );

                if ( sCNBY->mBaseMtrNode == NULL )
                {
                    sCNBY->mBaseMtrNode = sNewMtrNode;
                    sLastMtrNode  = sNewMtrNode;
                }
                else
                {
                    sLastMtrNode->next = sNewMtrNode;
                    sLastMtrNode       = sNewMtrNode;
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    for ( sItrAttr  = sCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( QTC_IS_PSEUDO( sItrAttr->expr ) == ID_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sItrAttr->expr->lflag & QTC_NODE_PRIOR_MASK ) == QTC_NODE_PRIOR_EXIST )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sItrAttr->expr->lflag & QTC_NODE_LOB_COLUMN_MASK ) == QTC_NODE_LOB_COLUMN_EXIST )
        {
            continue;
        }
        else
        {
            /* Nothing to do */
        }
        sOrgTable  = sItrAttr->expr->node.baseTable;
        sOrgColumn = sItrAttr->expr->node.baseColumn;
        IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                          aQuerySet ,
                                          sItrAttr->expr,
                                          ID_FALSE,
                                          sCNBY->myRowID ,
                                          0,
                                          & sColumnCount ,
                                          & sNewMtrNode )
                  != IDE_SUCCESS );

        for ( sPred = aStartWith->predicate; sPred != NULL; sPred = sPred->next )
        {
            for ( sPredMore = sPred; sPredMore != NULL; sPredMore = sPredMore->more )
            {
                changeNodeForCNBYJoin( sPredMore->node,
                                       sOrgTable,
                                       sOrgColumn,
                                       sNewMtrNode->dstNode->node.table,
                                       sNewMtrNode->dstNode->node.column );
            }
        }
        for ( sPred = aConnectBy->predicate; sPred != NULL; sPred = sPred->next )
        {
            for ( sPredMore = sPred; sPredMore != NULL; sPredMore = sPredMore->more )
            {
                changeNodeForCNBYJoin( sPredMore->node,
                                       sOrgTable,
                                       sOrgColumn,
                                       sNewMtrNode->dstNode->node.table,
                                       sNewMtrNode->dstNode->node.column );
            }
        }
        if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) == QMC_ATTR_USELESS_RESULT_TRUE )
        {
            // PROJ-2469 Optimize View Materialization
            // �������� ������ �ʴ� MtrNode�� ���ؼ� flag ó���Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_USELESS_COLUMN;
        }
        else
        {
            sFlag = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNewMtrNode->srcNode->node.table].lflag;

            if ( ( sFlag & MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK )
                   == MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE )
            {
                sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN;
            }
            else
            {
                //CMTR���� ������ qmcMtrNode�̹Ƿ� �����ϵ��� �Ѵ�.
                sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;
            }
        }

        // PROJ-2179
        sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
        sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

        // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
        sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
        sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

        //connect
        if ( sCNBY->mBaseMtrNode == NULL )
        {
            sCNBY->mBaseMtrNode = sNewMtrNode;
            sLastMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastMtrNode->next = sNewMtrNode;
            sLastMtrNode       = sNewMtrNode;
        }
    }

    for ( sItrAttr  = sCNBY->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            sOrgTable  = sItrAttr->expr->node.baseTable;
            sOrgColumn = sItrAttr->expr->node.baseColumn;
            IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                              aQuerySet ,
                                              sItrAttr->expr,
                                              ID_FALSE,
                                              sCNBY->myRowID ,
                                              0,
                                              & sColumnCount ,
                                              & sNewMtrNode )
                      != IDE_SUCCESS );

            if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) == QMC_ATTR_USELESS_RESULT_TRUE )
            {
                // PROJ-2469 Optimize View Materialization
                // �������� ������ �ʴ� MtrNode�� ���ؼ� flag ó���Ѵ�.
                sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TYPE_USELESS_COLUMN;
            }
            else
            {
                sFlag = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNewMtrNode->srcNode->node.table].lflag;
                if ( ( sFlag & MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK )
                       == MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                    sNewMtrNode->flag |= QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN;
                }
                else
                {
                    //CMTR���� ������ qmcMtrNode�̹Ƿ� �����ϵ��� �Ѵ�.
                    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                    sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;
                }
            }

            for ( sPred = aStartWith->predicate; sPred != NULL; sPred = sPred->next )
            {
                for ( sPredMore = sPred; sPredMore != NULL; sPredMore = sPredMore->more )
                {
                    changeNodeForCNBYJoin( sPredMore->node,
                                           sOrgTable,
                                           sOrgColumn,
                                           sNewMtrNode->dstNode->node.table,
                                           sNewMtrNode->dstNode->node.column );
                }
            }
            for ( sPred = aConnectBy->predicate; sPred != NULL; sPred = sPred->next )
            {
                for ( sPredMore = sPred; sPredMore != NULL; sPredMore = sPredMore->more )
                {
                    changeNodeForCNBYJoin( sPredMore->node,
                                           sOrgTable,
                                           sOrgColumn,
                                           sNewMtrNode->dstNode->node.table,
                                           sNewMtrNode->dstNode->node.column );
                }
            }
            sNewMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
            sNewMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;

            // PROJ-2179
            sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
            sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

            if ( ( sItrAttr->flag & QMC_ATTR_SORT_ORDER_MASK )
                    == QMC_ATTR_SORT_ORDER_ASC )
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_DESCENDING;
            }

            /* PROJ-2435 order by nulls first/last */
            if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                 != QMC_ATTR_SORT_NULLS_ORDER_NONE )
            {
                if ( ( sItrAttr->flag & QMC_ATTR_SORT_NULLS_ORDER_MASK )
                     == QMC_ATTR_SORT_NULLS_ORDER_FIRST )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_FIRST;
                }
                else
                {
                    sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                    sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_LAST;
                }
            }
            else
            {
                sNewMtrNode->flag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
                sNewMtrNode->flag |= QMC_MTR_SORT_NULLS_ORDER_NONE;
            }
            // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
            sItrAttr->expr->node.table  = sNewMtrNode->dstNode->node.table;
            sItrAttr->expr->node.column = sNewMtrNode->dstNode->node.column;

            //connect
            if ( sCNBY->mBaseMtrNode == NULL )
            {
                sCNBY->mBaseMtrNode = sNewMtrNode;
                sLastMtrNode  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode->next = sNewMtrNode;
                sLastMtrNode       = sNewMtrNode;
            }
            if ( sCNBY->baseSortNode == NULL )
            {
                sCNBY->baseSortNode = sNewMtrNode;
                sLastMtrNode2  = sNewMtrNode;
            }
            else
            {
                sLastMtrNode2->next = sNewMtrNode;
                sLastMtrNode2       = sNewMtrNode;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    sCurOffset = sCNBY->mtrNodeOffset;

    // ���� ��尡 ����� ���� ����
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    sCNBY->baseSortOffset = sCurOffset;

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sCNBY->myRowID,
                                           sColumnCount )
              != IDE_SUCCESS );

    sMtcTemplate->rows[sCNBY->myRowID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sCNBY->myRowID].lflag |= MTC_TUPLE_PLAN_TRUE;
    sMtcTemplate->rows[sCNBY->myRowID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sCNBY->myRowID].lflag |= MTC_TUPLE_PLAN_MTR_TRUE;

    sCNBY->plan.flag &= ~QMN_PLAN_STORAGE_MASK;
    sCNBY->plan.flag |= QMN_PLAN_STORAGE_MEMORY;

    IDE_TEST( qmg::copyMtcColumnExecute( aStatement, sCNBY->mBaseMtrNode )
              != IDE_SUCCESS );
    IDE_TEST( qmg::setColumnLocate( aStatement, sCNBY->mBaseMtrNode )
              != IDE_SUCCESS );

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           sMtcTemplate,
                                           sCNBY->priorRowID,
                                           sColumnCount )
              != IDE_SUCCESS );

    for ( i = 0; i < sMtcTemplate->rows[sCNBY->priorRowID].columnCount; i++ )
    {
        idlOS::memcpy( (void *) &(sMtcTemplate->rows[sCNBY->priorRowID].columns[i]),
                       (void *) &(sMtcTemplate->rows[sCNBY->myRowID].columns[i]),
                       ID_SIZEOF(mtcColumn) );

        /* BUG-47750  Connect by ������ Variable column �� join�� �Բ� ����� ��� FATAL */
        if ( ( ( sMtcTemplate->rows[sCNBY->priorRowID].columns[i].column.flag & SMI_COLUMN_TYPE_MASK )
               == SMI_COLUMN_TYPE_VARIABLE ) ||
               ( ( sMtcTemplate->rows[sCNBY->priorRowID].columns[i].column.flag & SMI_COLUMN_TYPE_MASK )
               == SMI_COLUMN_TYPE_VARIABLE_LARGE ) )
        {
            sMtcTemplate->rows[sCNBY->priorRowID].columns[i].column.flag &= ~SMI_COLUMN_TYPE_MASK;
            sMtcTemplate->rows[sCNBY->priorRowID].columns[i].column.flag |= SMI_COLUMN_TYPE_FIXED;
        }
    }

    IDE_TEST( processStartWithPredicate( aStatement,
                                         aQuerySet,
                                         sCNBY,
                                         aStartWith )
              != IDE_SUCCESS );

    IDE_TEST( processConnectByPredicateForJoin( aStatement,
                                                aQuerySet,
                                                sCNBY,
                                                aConnectBy )
              != IDE_SUCCESS );

    /* ������ �۾�1 */
    sPredicate[0] = sCNBY->startWithConstant;
    sPredicate[1] = sCNBY->startWithFilter;
    sPredicate[2] = sCNBY->startWithSubquery;
    sPredicate[3] = sCNBY->startWithNNF;
    sPredicate[4] = sCNBY->connectByConstant;
    sPredicate[5] = sCNBY->connectByFilter;
    /* fix BUG-26770
     * connect by LEVEL+to_date(:D1,'YYYYMMDD') <= to_date(:D2,'YYYYMMDD')+1;
     * ���Ǽ���� ��������������
     * level filter�� ���� ó���� ����,
     * level filter�� ���ѵ� host������ ������� ���� bindParamInfo ������, ������������.
     */
    sPredicate[6] = sCNBY->levelFilter;

    /* BUG-39041 The connect by clause is not support subquery. */
    sPredicate[7] = sCNBY->connectBySubquery;

    /* BUG-39434 The connect by need rownum pseudo column. */
    sPredicate[8] = sCNBY->rownumFilter;

    for ( i = 0;
          i < 9;
          i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }

    /* dependency ó�� �� subquery�� ó�� */
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sCNBY->plan ,
                                            QMO_CNBY_DEPENDENCY_FOR_JOIN,
                                            sCNBY->myRowID,
                                            NULL,
                                            9,
                                            sPredicate,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sCNBY->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sCNBY->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCNBY->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2509 Connect By Predicate ó�� */
IDE_RC qmoOneMtrPlan::processConnectByPredicateForJoin( qcStatement    * aStatement,
                                                        qmsQuerySet    * aQuerySet,
                                                        qmncCNBY       * aCNBY,
                                                        qmoLeafInfo    * aConnectBy )
{
    qmncScanMethod sMethod;
    idBool         sIsSubQuery    = ID_FALSE;
    idBool         sFind          = ID_FALSE;
    qtcNode      * sNode          = NULL;
    qtcNode      * sSortNode      = NULL;
    qtcNode      * sPriorPred     = NULL;
    qtcNode      * sOptimizedNode = NULL;
    qmcMtrNode   * sMtrNode       = NULL;
    qmcMtrNode   * sTmp           = NULL;
    UInt           sCurOffset     = 0;
    qtcNode        sPriorTmp;
    UShort         sColumnCount   = 0;
    qmoPredicate * sPred          = NULL;
    qmoPredicate * sPredMore      = NULL;
    qmcMtrNode   * sNewMtrNode    = NULL;

    aCNBY->levelFilter       = NULL;
    aCNBY->rownumFilter      = NULL;
    aCNBY->connectByKeyRange = NULL;
    aCNBY->connectByFilter   = NULL;
    aCNBY->priorNode         = NULL;

    /* 1. Level Filter �� ó�� */
    if ( aConnectBy->levelPredicate != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aConnectBy->levelPredicate,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sNode )
                  != IDE_SUCCESS );

        /* BUG-17921 */
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                                & sOptimizedNode )
                   != IDE_SUCCESS );
        aCNBY->levelFilter = sOptimizedNode;

        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aCNBY->levelFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39434 The connect by need rownum pseudo column.
     * 1-2 Rownum Filter �� ó��
     */
    if ( aConnectBy->connectByRownumPred != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aConnectBy->connectByRownumPred,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sNode )
                  != IDE_SUCCESS );

        /* BUG-17921 */
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );
        aCNBY->rownumFilter = sOptimizedNode;

        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aCNBY->rownumFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2641 Hierarchy Query Index
     * Connectby predicate���� prior Node�� ã�Ƽ�
     * Error�� ���� ��Ȳ�� üũ�ؾ��Ѵ�.
     */
    for ( sPred = aConnectBy->predicate; sPred != NULL; sPred = sPred->next )
    {
        for ( sPredMore = sPred;
              sPredMore != NULL;
              sPredMore = sPredMore->more )
        {
            sPriorTmp.node.next = NULL;
            IDE_TEST( findPriorColumns( aStatement,
                                        sPredMore->node,
                                        & sPriorTmp )
                      != IDE_SUCCESS );
            /* BUG-44759 processPredicae�� �����ϱ����� priorNode
             * �� �־���Ѵ�.
             */
            aCNBY->priorNode = (qtcNode *)sPriorTmp.node.next;

            if ( aCNBY->priorNode != NULL )
            {
                aQuerySet->SFWGH->hierarchy->originalTable = aCNBY->priorNode->node.table;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    IDE_TEST( qmoOneNonPlan::processPredicate( aStatement,
                                               aQuerySet,
                                               aConnectBy->predicate,
                                               aConnectBy->constantPredicate,
                                               aConnectBy->ridPredicate,
                                               aConnectBy->index,
                                               aCNBY->myRowID,
                                               & sMethod.constantFilter,
                                               & sMethod.filter,
                                               & sMethod.lobFilter,
                                               & sMethod.subqueryFilter,
                                               & sMethod.varKeyRange,
                                               & sMethod.varKeyFilter,
                                               & sMethod.varKeyRange4Filter,
                                               & sMethod.varKeyFilter4Filter,
                                               & sMethod.fixKeyRange,
                                               & sMethod.fixKeyFilter,
                                               & sMethod.fixKeyRange4Print,
                                               & sMethod.fixKeyFilter4Print,
                                               & sMethod.ridRange,
                                               & sIsSubQuery )
              != IDE_SUCCESS );

    /* 3. constantFilter ���� */
    if ( sMethod.constantFilter != NULL )
    {
        aCNBY->connectByConstant = sMethod.constantFilter;
    }
    else
    {
        aCNBY->connectByConstant = NULL;
    }

    /* BUG-39401 need subquery for connect by clause */
    if ( sMethod.subqueryFilter != NULL )
    {
        aCNBY->connectBySubquery = sMethod.subqueryFilter;
    }
    else
    {
        aCNBY->connectBySubquery = NULL;
    }

    if ( ( sMethod.varKeyRange == NULL ) &&
         ( sMethod.varKeyFilter == NULL ) &&
         ( sMethod.varKeyRange4Filter == NULL ) &&
         ( sMethod.varKeyFilter4Filter == NULL ) &&
         ( sMethod.fixKeyRange == NULL ) &&
         ( sMethod.fixKeyFilter == NULL ) )
    {
        aCNBY->mIndex = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST_RAISE( sMethod.lobFilter != NULL, ERR_NOT_SUPPORT_LOB_FILTER );

    if ( sMethod.filter != NULL )
    {
        aCNBY->connectByFilter = sMethod.filter;

        sPriorTmp.node.next = NULL;
        IDE_TEST( findPriorColumns( aStatement,
                                    sMethod.filter,
                                    & sPriorTmp )
                  != IDE_SUCCESS );
        aCNBY->priorNode = (qtcNode *)sPriorTmp.node.next;

        /* 4-2. find Prior predicate and Sort node */
        IDE_TEST( findPriorPredAndSortNode( aStatement,
                                            sMethod.filter,
                                            &sSortNode,
                                            &sPriorPred,
                                            aCNBY->myRowID,
                                            &sFind )
                  != IDE_SUCCESS );

        if ( ( sFind == ID_TRUE ) &&
             ( aCNBY->mIndex == NULL ) )
        {
            if ( ( aCNBY->flag & QMNC_CNBY_SIBLINGS_MASK )
                 == QMNC_CNBY_SIBLINGS_TRUE )
            {
                IDE_TEST( makeSortNodeOfCNBY( aStatement,
                                              aQuerySet,
                                              aCNBY,
                                              sSortNode )
                          != IDE_SUCCESS );
            }
            else
            {
                for ( sMtrNode  = aCNBY->mBaseMtrNode;
                      sMtrNode != NULL;
                      sMtrNode  = sMtrNode->next )
                {
                    if ( ( sMtrNode->dstNode->node.table == sSortNode->node.table ) &&
                         ( sMtrNode->dstNode->node.column == sSortNode->node.column ))
                    {
                        IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement), qmcMtrNode, &sNewMtrNode)
                                  != IDE_SUCCESS);
                        *sNewMtrNode = *sMtrNode;
                        aCNBY->baseSortNode = sNewMtrNode;
                        sNewMtrNode->next = NULL;

                        sNewMtrNode->flag &= ~QMC_MTR_SORT_NEED_MASK;
                        sNewMtrNode->flag |= QMC_MTR_SORT_NEED_TRUE;
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            if ( (((qtcNode*)(sPriorPred->node.arguments))->lflag & QTC_NODE_PRIOR_MASK) ==
                 QTC_NODE_PRIOR_EXIST )
            {
                sPriorPred->indexArgument = 1;
            }
            else
            {
                /* Nothing to do */
            }

            /* CNF sPriorPredicate �� DNF ���·� ��ȯ */
            IDE_TEST( qmoNormalForm::normalizeDNF( aStatement,
                                                   sPriorPred,
                                                   &aCNBY->connectByKeyRange)
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

    sCurOffset = aCNBY->baseSortOffset;
    sColumnCount = 0;
    for ( sTmp = aCNBY->baseSortNode; sTmp != NULL ; sTmp = sTmp->next )
    {
        sColumnCount++;
    }
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    aCNBY->mSortNodeOffset = sCurOffset;
    sColumnCount = 0;
    for ( sTmp = aCNBY->sortNode; sTmp != NULL ; sTmp = sTmp->next )
    {
        sColumnCount++;
    }
    // ���� ��尡 ����� ���� ����
    sCurOffset += idlOS::align8(ID_SIZEOF(qmdMtrNode)) * sColumnCount;

    aCNBY->sortMTROffset = sCurOffset;
    sCurOffset += idlOS::align8(ID_SIZEOF(qmcdSortTemp));
    aCNBY->mBaseSortMTROffset = sCurOffset;
    sCurOffset += idlOS::align8(ID_SIZEOF(qmcdSortTemp));

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sCurOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORT_LOB_FILTER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMO_NOT_ALLOWED_LOB_FILTER ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmoOneMtrPlan::changeNodeForCNBYJoin( qtcNode * aNode,
                                           UShort    aSrcTable,
                                           UShort    aSrcColumn,
                                           UShort    aDstTable,
                                           UShort    aDstColumn )
{
    if ( ( aNode->node.module == &qtc::columnModule ) &&
         ( QTC_IS_PSEUDO( aNode ) == ID_FALSE ) )
    {
        if ( ( aNode->node.baseTable == aSrcTable ) &&
             ( aNode->node.baseColumn == aSrcColumn ) )
        {
            aNode->node.table = aDstTable;
            aNode->node.column = aDstColumn;
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

    if ( aNode->node.arguments != NULL )
    {
        changeNodeForCNBYJoin( (qtcNode *)aNode->node.arguments,
                               aSrcTable,
                               aSrcColumn,
                               aDstTable,
                               aDstColumn );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aNode->node.next != NULL )
    {
        changeNodeForCNBYJoin( (qtcNode *)aNode->node.next,
                               aSrcTable,
                               aSrcColumn,
                               aDstTable,
                               aDstColumn );
    }
    else
    {
        /* Nothing to do */
    }
}

