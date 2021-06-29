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
 *     Cost ����
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qmoCost.h>
#include <qcg.h>
#include <qcgPlan.h>
#include <smiTableSpace.h>

// TASK-6699 TPC-H ���� ����
// �г�Ƽ���� TPC-H ����ð��� �����ϰ� ������ ���̴�.
//#define QMO_COST_NL_JOIN_PENALTY (6.0)
#define QMO_COST_HASH_JOIN_PENALTY (3.0)

extern mtfModule mtfNotLike;
extern mtfModule mtfLike;
extern mtfModule mtfOr;

/***********************************************************************
* Object Scan Cost
***********************************************************************/

SDouble qmoCost::getTableFullScanCost( qmoSystemStatistics * aSystemStat,
                                       qmoStatistics       * aTableStat,
                                       SDouble               aFilterCost,
                                       SDouble             * aMemCost,
                                       SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : full scan cost
 *
 *****************************************************************************/
    SDouble sCost;

    *aMemCost   = ( aTableStat->totalRecordCnt *
                    aTableStat->readRowTime ) + aFilterCost;

    *aDiskCost  = ( aSystemStat->multiIoScanTime * aTableStat->pageCnt );

    sCost       = *aMemCost + *aDiskCost;
    
    return sCost;
}

SDouble qmoCost::getTableRIDScanCost( qmoStatistics       * aTableStat,
                                      SDouble             * aMemCost,
                                      SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : rid scan cost
 *
 *****************************************************************************/

    SDouble sCost;

    *aMemCost  = aTableStat->readRowTime;

    // ���� hit ��Ȳ�� �����ϰ� ��ũ ����� 0 ���� ó���մϴ�.
    *aDiskCost = 0;

    sCost       = *aMemCost + *aDiskCost;
    
    return sCost;
}

SDouble qmoCost::getIndexScanCost( qcStatement         * aStatement,
                                   mtcColumn           * aColumns,
                                   qmoPredicate        * aJoinPred,
                                   qmoStatistics       * aTableStat,
                                   qmoIdxCardInfo      * aIndexStat,
                                   SDouble               aKeyRangeSelectivity,
                                   SDouble               aKeyFilterSelectivity,
                                   SDouble               aKeyRangeCost,
                                   SDouble               aKeyFilterCost,
                                   SDouble               aFilterCost,
                                   SDouble             * aMemCost,
                                   SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : index scan cost
 *               btree, rtree scan �� �ڽ�Ʈ�� ���Ѵ�.
 *****************************************************************************/

    SDouble     sCost;
    SDouble     sMemCost;
    SDouble     sDiskCost;
    UInt        i,j;
    mtcColumn * sTableColumn;
    mtcColumn * sKeyColumn;
    idBool      sWithTableScan = ID_FALSE;

    //--------------------------------------
    // table ��ĵ�� �ؾ��ϴ��� �Ǵ�
    //--------------------------------------

    if ( ( aColumns->column.flag & SMI_COLUMN_STORAGE_MASK )
         == SMI_COLUMN_STORAGE_DISK )
    {
        // BUG-39854
        // predicate �������� table scan �� �ϰ� �ȴ�.
        if ( aJoinPred == NULL )
        {
            sWithTableScan = ID_TRUE;
        }
        else
        {
            for ( i=0; i < aTableStat->columnCnt; i++ )
            {
                sTableColumn = aColumns + i;

                for ( j=0; j < aIndexStat->index->keyColCount; j++ )
                {
                    sKeyColumn = &(aIndexStat->index->keyColumns[j]);

                    if ( sTableColumn->column.id == sKeyColumn->column.id )
                    {
                        break;
                    }
                }

                // key �÷��� ������ �÷��� ���� ��� ���ǹ��� ���Ǵ� �÷����� Ȯ���Ѵ�.
                if ( j == aIndexStat->index->keyColCount )
                {
                    if ( (sTableColumn->flag & MTC_COLUMN_USE_COLUMN_MASK) ==
                         MTC_COLUMN_USE_COLUMN_TRUE )
                    {
                        sWithTableScan = ID_TRUE;
                        break;
                    }
                }
            } // for
        } // if
    }
    else
    {
        // BUG-38152
        // predicate �������� table scan �� �ϰ� �ȴ�.
        if ( aJoinPred == NULL )
        {
            sWithTableScan = ID_TRUE;
        }
        else
        {
            // BUG-36514
            // mem index �϶��� table scan cost �� �߰����� �ʴ´�.
            // �����͸� �̿��Ͽ� �ٷ� record �� ������ �����ϴ�.
            sWithTableScan = ID_FALSE;
        }
    }

    //--------------------------------------
    // Index scan cost
    //--------------------------------------
    sCost = getIndexBtreeScanCost( aStatement->mSysStat,
                                   aTableStat,
                                   aIndexStat,
                                   aKeyRangeSelectivity,
                                   aKeyRangeCost,
                                   aKeyFilterCost,
                                   aFilterCost,
                                   aMemCost,
                                   aDiskCost );

    //--------------------------------------
    // table scan cost
    //--------------------------------------
    if( sWithTableScan == ID_TRUE )
    {
        sCost += getIndexWithTableCost( aStatement->mSysStat,
                                        aTableStat,
                                        aIndexStat,
                                        aKeyRangeSelectivity,
                                        aKeyFilterSelectivity,
                                        &sMemCost,
                                        &sDiskCost );

        *aMemCost  = sMemCost  + *aMemCost;
        *aDiskCost = sDiskCost + *aDiskCost;
    }
    else
    {
        // nothing todo
    }

    //--------------------------------------
    // OPTIMIZER_MEMORY_INDEX_COST_ADJ ( BUG-43736 )
    //--------------------------------------
    *aMemCost *= QCG_GET_SESSION_OPTIMIZER_MEMORY_INDEX_COST_ADJ(aStatement);
    *aMemCost /= 100;

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_MEMORY_INDEX_COST_ADJ );

    //--------------------------------------
    // OPTIMIZER_DISK_INDEX_COST_ADJ
    //--------------------------------------
    *aDiskCost *= QCG_GET_SESSION_OPTIMIZER_DISK_INDEX_COST_ADJ(aStatement);
    *aDiskCost /= 100;

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_DISK_INDEX_COST_ADJ );

    sCost = *aMemCost + *aDiskCost;
    
    return sCost;
}

SDouble qmoCost::getIndexBtreeScanCost( qmoSystemStatistics * aSystemStat,
                                        qmoStatistics       * aTableStat,
                                        qmoIdxCardInfo      * aIndexStat,
                                        SDouble               aKeyRangeSelectivity,
                                        SDouble               aKeyRangeCost,
                                        SDouble               aKeyFilterCost,
                                        SDouble               aFilterCost,
                                        SDouble             * aMemCost,
                                        SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : getIndexBtreeScanCost
 *               sLeafPageCnt �� ��������� �˼� �����Ƿ� ������ ���� �����Ͽ� ����Ѵ�.
 *               ��ü ���ڵ� ���� / avgSlotCount
 *****************************************************************************/

    SDouble     sCost;
    SDouble     sLeafPageCnt;

    sLeafPageCnt = idlOS::ceil( aTableStat->totalRecordCnt / aIndexStat->avgSlotCount );

    *aMemCost  = ( aIndexStat->indexLevel * aTableStat->readRowTime ) +
                   aKeyRangeCost  +
                   aKeyFilterCost +
                   aFilterCost;

    if( aIndexStat->pageCnt > 0 )
    {
        *aDiskCost = ( aSystemStat->singleIoScanTime *
                      (aIndexStat->indexLevel + ( sLeafPageCnt * aKeyRangeSelectivity )) );
    }
    else
    {
        *aDiskCost = 0;
    }

    sCost      = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getIndexWithTableCost( qmoSystemStatistics * aSystemStat,
                                        qmoStatistics       * aTableStat,
                                        qmoIdxCardInfo      * aIndexStat,
                                        SDouble               aKeyRangeSelectivity,
                                        SDouble               aKeyFilterSelectivity,
                                        SDouble             * aMemCost,
                                        SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : getIndexWithTableCost
 *               index�� ���� �÷����� �����Ҷ� �߻��ϴ� ����� ����Ѵ�.
 *****************************************************************************/

    SDouble sCost;
    SDouble sIndexSelectivity = aKeyRangeSelectivity * aKeyFilterSelectivity;

    *aMemCost  = ( aTableStat->totalRecordCnt *
                   sIndexSelectivity *
                   aTableStat->readRowTime );

    if( aIndexStat->pageCnt > 0 )
    {
        *aDiskCost = ( aIndexStat->clusteringFactor *
                       sIndexSelectivity *
                       aSystemStat->singleIoScanTime );
    }
    else
    {
        *aDiskCost = 0;
    }

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

/***********************************************************************
* Join Cost
***********************************************************************/

SDouble qmoCost::getFullNestedJoinCost( qmgGraph    * aLeft,
                                        qmgGraph    * aRight,
                                        SDouble       aFilterCost,
                                        SDouble     * aMemCost,
                                        SDouble     * aDiskCost )
{
/******************************************************************************
 *
 * Description : full nested loop join cost
 *               left scan cost + ( left output * right scan cost )
 *****************************************************************************/

    SDouble        sCost;
    qmgCostInfo  * sLeftCostInfo;
    qmgCostInfo  * sRightCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    *aMemCost =  sLeftCostInfo->totalAccessCost +
               ( sLeftCostInfo->outputRecordCnt * sRightCostInfo->totalAccessCost ) +
                 aFilterCost;

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                ( sLeftCostInfo->outputRecordCnt * sRightCostInfo->totalDiskCost );

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getFullStoreJoinCost( qmgGraph            * aLeft,
                                       qmgGraph            * aRight,
                                       SDouble               aFilterCost,
                                       qmoSystemStatistics * aSystemStat,
                                       idBool                aUseRightDiskTemp,
                                       SDouble             * aMemCost,
                                       SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : full stored nested loop join cost
 *               left scan cost + right scan cost + right temp build cost
 *               ( left output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    // BUGBUG sRightTempScanCost �� ���� �����ؾ� �Ѵ�.
    // �޸� ��ũ �������̺��϶��� ����ؾ���
    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( sRightCostInfo->outputRecordCnt *
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        // Join �׷����� ���� ��� ���ڵ��� �б�ð��� �˼� ��� 1�� �����Ѵ�.
        sRightTempScanCost = sRightCostInfo->outputRecordCnt;
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightMemTempCost =  getMemStoreTempCost( aSystemStat,
                                                  sRightCostInfo );
        sRightDiskTempCost = 0;
    }
    else
    {
        sRightMemTempCost  = getMemStoreTempCost( aSystemStat,
                                                  sRightCostInfo );

        sRightDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                   sRightCostInfo->outputRecordCnt,
                                                   sRightCostInfo->recordSize,
                                                   QMO_COST_STORE_SORT_TEMP );
    }

    *aMemCost = sLeftCostInfo->totalAccessCost +
                sRightCostInfo->totalAccessCost +
                sRightMemTempCost +
              ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                aFilterCost;

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                  sRightCostInfo->totalDiskCost +
                  sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getIndexNestedJoinCost( qmgGraph            * aLeft,
                                         qmoStatistics       * aTableStat,
                                         qmoIdxCardInfo      * aIndexStat,
                                         qmoSystemStatistics * aSystemStat,
                                         SDouble               aRightMemCost,
                                         SDouble               aRightDiskCost,
                                         SDouble               aFilterCost,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : index nested loop join cost
 *               left scan cost + ( left output * right scan cost )
 *****************************************************************************/

    SDouble          sCost;
    SDouble          sTotalReadPageCnt;
    SDouble          sMaxReadPageCnt;
    qmgCostInfo    * sLeftCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);

    // TASK-6699 TPC-H ���� ����
    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    *aMemCost =  sLeftCostInfo->totalAccessCost +
               ( sLeftCostInfo->outputRecordCnt * (aRightMemCost + aSystemStat->mIndexNlJoinPenalty) ) +
                 aFilterCost;

    // BUG-37125 tpch plan optimization
    // Disk index �� ��� �ε����� page ������ buffer ������� ū ��찡 �ִ�.
    if( aIndexStat->pageCnt > 0 )
    {
        // �ߺ��� ������� ���� read page ����
        sTotalReadPageCnt = sLeftCostInfo->outputRecordCnt *
                                ( aRightDiskCost / aSystemStat->singleIoScanTime );

        // �ߺ��� ����� read page ����
        sMaxReadPageCnt = IDL_MIN( sTotalReadPageCnt,
                                   aTableStat->pageCnt + aIndexStat->pageCnt );

        // ���� ������� ū ��� ���� ���÷��̽��� �Ͼ��.
        if( smiGetBufferPoolSize() > sMaxReadPageCnt )
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sMaxReadPageCnt * aSystemStat->singleIoScanTime );
        }
        else
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sTotalReadPageCnt * aSystemStat->singleIoScanTime );
        }
    }
    else
    {
        *aDiskCost = 0;
    }

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getFullNestedJoin4SemiCost( qmgGraph    * aLeft,
                                             qmgGraph    * aRight,
                                             SDouble       aJoinSelectivty,
                                             SDouble       aFilterCost,
                                             SDouble     * aMemCost,
                                             SDouble     * aDiskCost )
{
/******************************************************************************
 *
 * Description : left scan cost +
 *               left output * ( semi ��� * ���õ� +
                                 full scan ��� * (1 - ���õ�) )
 *               semi ���      = right scan cost / 2
 *               full scan ��� = right scan cost
 *****************************************************************************/

    SDouble        sCost;
    qmgCostInfo  * sLeftCostInfo;
    qmgCostInfo  * sRightCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    *aMemCost = sLeftCostInfo->totalAccessCost +
                    sLeftCostInfo->outputRecordCnt *
                    ( ( sRightCostInfo->totalAccessCost / 2 ) * aJoinSelectivty +
                            sRightCostInfo->totalAccessCost * ( 1 - aJoinSelectivty ) ) +
                    aFilterCost;

    *aDiskCost = sLeftCostInfo->totalDiskCost +
                    sLeftCostInfo->outputRecordCnt *
                    ( ( sRightCostInfo->totalDiskCost / 2 ) * aJoinSelectivty +
                             sRightCostInfo->totalDiskCost * ( 1 - aJoinSelectivty ) );

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getIndexNestedJoin4SemiCost( qmgGraph            * aLeft,
                                              qmoStatistics       * aTableStat,
                                              qmoIdxCardInfo      * aIndexStat,
                                              qmoSystemStatistics * aSystemStat,
                                              SDouble               aFilterCost,
                                              SDouble             * aMemCost,
                                              SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : left scan cost + ( left output * index level )
                 semi,anti ������ ��� 1���� key ���� ���ؼ� index �� �ѹ��� ����Ѵ�.
 *****************************************************************************/

    SDouble          sCost;
    SDouble          sTotalReadPageCnt;
    SDouble          sMaxReadPageCnt;
    qmgCostInfo    * sLeftCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);

    // TASK-6699 TPC-H ���� ����
    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    *aMemCost = sLeftCostInfo->totalAccessCost +
                ( sLeftCostInfo->outputRecordCnt * 0.5 *
                    ( aIndexStat->indexLevel *
                      aTableStat->readRowTime +
                      aSystemStat->mIndexNlJoinPenalty ) ) +
                aFilterCost;

    if( aIndexStat->pageCnt > 0 )
    {
        // tpch Q21 semi index nl join cost �� ���߾�� �մϴ�.
        sTotalReadPageCnt = sLeftCostInfo->outputRecordCnt * 0.5 *
                            aIndexStat->indexLevel;

        sMaxReadPageCnt = IDL_MIN( sTotalReadPageCnt, aIndexStat->pageCnt );

        // ���� ������� ū ��� ���� ���÷��̽��� �Ͼ��.
        if( smiGetBufferPoolSize() > sMaxReadPageCnt )
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sMaxReadPageCnt * aSystemStat->singleIoScanTime );
        }
        else
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sTotalReadPageCnt * aSystemStat->singleIoScanTime );
        }
    }
    else
    {
        *aDiskCost = 0;
    }

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getFullNestedJoin4AntiCost( qmgGraph    * aLeft,
                                             qmgGraph    * aRight,
                                             SDouble       aJoinSelectivty,
                                             SDouble       aFilterCost,
                                             SDouble     * aMemCost,
                                             SDouble     * aDiskCost )
{
/******************************************************************************
 *
 * Description : left scan cost +
 *               left output * ( anti ��� * (1 - ���õ�) +
                                 full scan ��� * ���õ� )
 *               anti ���       = right scan cost / 2
 *               full scan ���  = right scan cost
 *****************************************************************************/

    SDouble        sCost;
    qmgCostInfo  * sLeftCostInfo;
    qmgCostInfo  * sRightCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    *aMemCost = sLeftCostInfo->totalAccessCost +
                    sLeftCostInfo->outputRecordCnt *
                    ( ( sRightCostInfo->totalAccessCost / 2 ) * ( 1 - aJoinSelectivty ) +
                            sRightCostInfo->totalAccessCost * aJoinSelectivty ) +
                    aFilterCost;

    *aDiskCost = sLeftCostInfo->totalDiskCost +
                    sLeftCostInfo->outputRecordCnt *
                    ( ( sRightCostInfo->totalDiskCost / 2 ) * ( 1 - aJoinSelectivty ) +
                             sRightCostInfo->totalDiskCost * aJoinSelectivty );

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getIndexNestedJoin4AntiCost( qmgGraph            * aLeft,
                                              qmoStatistics       * aTableStat,
                                              qmoIdxCardInfo      * aIndexStat,
                                              qmoSystemStatistics * aSystemStat,
                                              SDouble               aFilterCost,
                                              SDouble             * aMemCost,
                                              SDouble             * aDiskCost )
{
/******************************************************************************
*
* Description : same getIndexNestedJoin4SemiCost
*****************************************************************************/

    SDouble sCost;

    sCost = getIndexNestedJoin4SemiCost( aLeft,
                                         aTableStat,
                                         aIndexStat,
                                         aSystemStat,
                                         aFilterCost,
                                         aMemCost,
                                         aDiskCost );
    
    return sCost;
}

SDouble qmoCost::getInverseIndexNestedJoinCost( qmgGraph            * aLeft,
                                                qmoStatistics       * aTableStat,
                                                qmoIdxCardInfo      * aIndexStat,
                                                qmoSystemStatistics * aSystemStat,
                                                idBool                aUseLeftDiskTemp,
                                                SDouble               aFilterCost,
                                                SDouble             * aMemCost,
                                                SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : PROJ-2385 Inverse Index NL for Semi Join
 *
 *               left scan cost + left hashing cost + ( left output * right index level )
 *               Semi ������ ��� 1���� key ���� ���ؼ� index �� �ѹ��� ����Ѵ�.
 *****************************************************************************/

    SDouble          sCost;
    SDouble          sTotalReadPageCnt;
    SDouble          sMaxReadPageCnt;
    qmgCostInfo    * sLeftCostInfo;
    SDouble          sLeftMemTempCost;
    SDouble          sLeftDiskTempCost;

    sLeftCostInfo = &(aLeft->costInfo);

    // LEFT�� Distinct Hashing �ȴ�
    if ( aUseLeftDiskTemp == ID_FALSE )
    {
        sLeftMemTempCost = getMemHashTempCost( aSystemStat,
                                               sLeftCostInfo,
                                               1 ); // nodeCnt

        sLeftDiskTempCost = 0;
    }
    else
    {
        sLeftMemTempCost  = getMemHashTempCost( aSystemStat,
                                                sLeftCostInfo,
                                                1 ); // nodeCnt

        sLeftDiskTempCost = getDiskHashTempCost( aSystemStat,
                                                 sLeftCostInfo,
                                                 1, // nodeCnt
                                                 sLeftCostInfo->recordSize );
    }

    // TASK-6699 TPC-H ���� ����
    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    *aMemCost = sLeftCostInfo->totalAccessCost +
                sLeftMemTempCost +
                ( sLeftCostInfo->outputRecordCnt *
                    ( aIndexStat->indexLevel *
                      aTableStat->readRowTime +
                      aSystemStat->mIndexNlJoinPenalty ) ) +
                aFilterCost;

    if( aIndexStat->pageCnt > 0 )
    {
        sTotalReadPageCnt = sLeftCostInfo->outputRecordCnt *
                            aIndexStat->indexLevel;

        sMaxReadPageCnt = IDL_MIN( sTotalReadPageCnt, aIndexStat->pageCnt );

        // ���� ������� ū ��� ���� ���÷��̽��� �Ͼ��.
        if( smiGetBufferPoolSize() > sMaxReadPageCnt )
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sMaxReadPageCnt * aSystemStat->singleIoScanTime );
        }
        else
        {
            *aDiskCost = sLeftCostInfo->totalDiskCost +
                            ( sTotalReadPageCnt * aSystemStat->singleIoScanTime );
        }
    }
    else
    {
        *aDiskCost = 0;
    }

    *aDiskCost += sLeftDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getAntiOuterJoinCost( qmgGraph          * aLeft,
                                       SDouble             aRightMemCost,
                                       SDouble             aRightDiskCost,
                                       SDouble             aFilterCost,
                                       SDouble           * aMemCost,
                                       SDouble           * aDiskCost )
{
/******************************************************************************
 *
 * Description : anti join cost
 *               left scan cost + ( left output * right scan cost )
 *****************************************************************************/

    SDouble        sCost;
    qmgCostInfo  * sLeftCostInfo;

    sLeftCostInfo  = &(aLeft->costInfo);

    *aMemCost =  sLeftCostInfo->totalAccessCost +
               ( sLeftCostInfo->outputRecordCnt * aRightMemCost ) +
                 aFilterCost;

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                ( sLeftCostInfo->outputRecordCnt * aRightDiskCost );

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getOnePassHashJoinCost( qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         idBool                aUseRightDiskTemp,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : one pass hash join
 *               left scan cost + right scan cost + right temp build cost
 *               ( left output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;
    SDouble          sFilterLoop;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    // BUGBUG sRightTempScanCost �� ���� �����ؾ� �Ѵ�.
    // �޸� ��ũ �������̺��϶��� ����ؾ���
    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( aSystemStat->mHashTime +
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = aSystemStat->mHashTime;
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = 0;
    }
    else
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = getDiskHashTempCost( aSystemStat,
                                                  sRightCostInfo,
                                                  1, // nodeCnt
                                                  sRightCostInfo->recordSize );
    }

    // TASK-6699 TPC-H ���� ����
    // PROJ-2553 ���� �߰��� Hash Temp Table �� ����ȭ�� ����
    if ( sLeftCostInfo->outputRecordCnt >
         (QMC_MEM_PART_HASH_MAX_PART_COUNT * QMC_MEM_PART_HASH_AVG_RECORD_COUNT) )
    {
        sFilterLoop = sLeftCostInfo->outputRecordCnt / QMC_MEM_PART_HASH_MAX_PART_COUNT;
    }
    else
    {
        sFilterLoop = QMC_MEM_PART_HASH_AVG_RECORD_COUNT;
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightTempScanCost += QMO_COST_HASH_JOIN_PENALTY;

        *aMemCost =  sLeftCostInfo->totalAccessCost +
                     sRightCostInfo->totalAccessCost +
                     sRightMemTempCost +
                   ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                     aFilterCost * sFilterLoop;
    }
    else
    {
        // ���� ������ �״�� �̿��Ѵ�.
        *aMemCost =  sLeftCostInfo->totalAccessCost +
                     sRightCostInfo->totalAccessCost +
                     sRightMemTempCost +
                   ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                     aFilterCost;
    }

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                  sRightCostInfo->totalDiskCost +
                  sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;
    
    return sCost;
}

SDouble qmoCost::getTwoPassHashJoinCost( qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         idBool                aUseLeftDiskTemp,
                                         idBool                aUseRightDiskTemp,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : two pass hash join
 *               left scan cost + right scan cost +
 *               left temp build cost + right temp build cost
 *               ( left output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sLeftMemTempCost;
    SDouble          sLeftDiskTempCost;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sLeftTempScanCost;
    SDouble          sRightTempScanCost;
    SDouble          sFilterLoop;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    // BUGBUG sRightTempScanCost �� ���� �����ؾ� �Ѵ�.
    if( aLeft->type == QMG_SELECTION ||
        aLeft->type == QMG_SHARD_SELECT || // PROJ-2638
        aLeft->type == QMG_PARTITION )
    {
        sLeftTempScanCost = ( sLeftCostInfo->outputRecordCnt *
                              aLeft->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sLeftTempScanCost = sLeftCostInfo->outputRecordCnt;
    }

    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( aSystemStat->mHashTime +
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = aSystemStat->mHashTime;
    }

    if( aUseLeftDiskTemp == ID_FALSE )
    {
        sLeftMemTempCost = getMemHashTempCost( aSystemStat,
                                               sLeftCostInfo,
                                               1 ); // nodeCnt

        sLeftDiskTempCost = 0;
    }
    else
    {
        sLeftMemTempCost = getMemHashTempCost( aSystemStat,
                                               sLeftCostInfo,
                                               1 ); // nodeCnt

        sLeftDiskTempCost = getDiskHashTempCost( aSystemStat,
                                                 sLeftCostInfo,
                                                 1, // nodeCnt
                                                 sLeftCostInfo->recordSize );
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = 0;
    }
    else
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = getDiskHashTempCost( aSystemStat,
                                                  sRightCostInfo,
                                                  1, // nodeCnt
                                                  sRightCostInfo->recordSize );
    }

    // TASK-6699 TPC-H ���� ����
    // PROJ-2553 ���� �߰��� Hash Temp Table �� ����ȭ�� ����
    if ( sLeftCostInfo->outputRecordCnt >
         (QMC_MEM_PART_HASH_MAX_PART_COUNT * QMC_MEM_PART_HASH_AVG_RECORD_COUNT) )
    {
        sFilterLoop = sLeftCostInfo->outputRecordCnt / QMC_MEM_PART_HASH_MAX_PART_COUNT;
    }
    else
    {
        sFilterLoop = QMC_MEM_PART_HASH_AVG_RECORD_COUNT;
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightTempScanCost += QMO_COST_HASH_JOIN_PENALTY;

        *aMemCost =  sLeftCostInfo->totalAccessCost +
                     sRightCostInfo->totalAccessCost +
                     sLeftMemTempCost +
                     sRightMemTempCost +
                   ( sLeftTempScanCost + ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) ) +
                     aFilterCost * sFilterLoop;
    }
    else
    {
        *aMemCost =  sLeftCostInfo->totalAccessCost +
                     sRightCostInfo->totalAccessCost +
                     sLeftMemTempCost +
                     sRightMemTempCost +
                   ( sLeftTempScanCost + ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) ) +
                     aFilterCost;
    }

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                  sRightCostInfo->totalDiskCost +
                  sLeftDiskTempCost +
                  sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getInverseHashJoinCost( qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         idBool                aUseRightDiskTemp,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : inverse hash join (PROJ-2339)
 *
 *               left scan cost + right scan cost + right temp build cost
 *               ( left output * right temp scan cost )
 *               ( right output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;
    SDouble          sFilterLoop;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    // BUGBUG sRightTempScanCost �� ���� �����ؾ� �Ѵ�.
    // �޸� ��ũ �������̺��϶��� ����ؾ���
    if ( ( aRight->type == QMG_SELECTION ) ||
         ( aRight->type == QMG_SHARD_SELECT ) || // PROJ-2638
         ( aRight->type == QMG_PARTITION ) )
    {
        sRightTempScanCost = ( aSystemStat->mHashTime +
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = aSystemStat->mHashTime;
    }

    if ( aUseRightDiskTemp == ID_FALSE )
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = 0;
    }
    else
    {
        sRightMemTempCost = getMemHashTempCost( aSystemStat,
                                                sRightCostInfo,
                                                1 ); // nodeCnt

        sRightDiskTempCost = getDiskHashTempCost( aSystemStat,
                                                  sRightCostInfo,
                                                  1, // nodeCnt
                                                  sRightCostInfo->recordSize );
    }

    // TASK-6699 TPC-H ���� ����
    // PROJ-2553 ���� �߰��� Hash Temp Table �� ����ȭ�� ����
    if ( sLeftCostInfo->outputRecordCnt >
         (QMC_MEM_PART_HASH_MAX_PART_COUNT * QMC_MEM_PART_HASH_AVG_RECORD_COUNT) )
    {
        sFilterLoop = sLeftCostInfo->outputRecordCnt / QMC_MEM_PART_HASH_MAX_PART_COUNT;
    }
    else
    {
        sFilterLoop = QMC_MEM_PART_HASH_AVG_RECORD_COUNT;
    }

    if( aUseRightDiskTemp == ID_FALSE )
    {
        sRightTempScanCost += QMO_COST_HASH_JOIN_PENALTY;

        *aMemCost = sLeftCostInfo->totalAccessCost +
                    sRightCostInfo->totalAccessCost +
                    sRightMemTempCost +
                  ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                  ( sRightCostInfo->outputRecordCnt * sRightTempScanCost ) +
                    aFilterCost * sFilterLoop;
    }
    else
    {
        *aMemCost = sLeftCostInfo->totalAccessCost +
                    sRightCostInfo->totalAccessCost +
                    sRightMemTempCost +
                  ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                  ( sRightCostInfo->outputRecordCnt * sRightTempScanCost ) +
                    aFilterCost;
    }

    *aDiskCost = sLeftCostInfo->totalDiskCost +
                 sRightCostInfo->totalDiskCost +
                 sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getOnePassSortJoinCost( qmoJoinMethodCost   * aJoinMethodCost,
                                         qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : one pass sort join
 *               left scan cost + right scan cost + right temp build cost
 *               ( left output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;
    UInt             sFlag;
    UInt             sTempType;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( sRightCostInfo->outputRecordCnt *
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = sRightCostInfo->outputRecordCnt;
    }

    sFlag     = aJoinMethodCost->flag;
    sTempType = (sFlag & QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_RIGHT_NODE_MASK) )
    {
        case QMO_JOIN_RIGHT_NODE_NONE :
            sRightTempScanCost  = 0;
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            break;

        case QMO_JOIN_RIGHT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                           sRightCostInfo->outputRecordCnt,
                                                           sRightCostInfo->recordSize,
                                                           QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_RIGHT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                          sRightCostInfo,
                                                          1, // nodeCnt
                                                          sRightCostInfo->recordSize );
            }
            break;

        default :
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            IDE_DASSERT ( 0 );
    }

    *aMemCost =  sLeftCostInfo->totalAccessCost +
                 sRightCostInfo->totalAccessCost +
                 sRightMemTempCost +
               ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
                 aFilterCost;

    *aDiskCost = sLeftCostInfo->totalDiskCost +
                 sRightCostInfo->totalDiskCost +
                 sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getTwoPassSortJoinCost( qmoJoinMethodCost   * aJoinMethodCost,
                                         qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : two pass sort join
 *               left scan cost + right scan cost +
 *               left temp build cost + right temp build cost
 *               ( left output * right temp scan cost )
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sLeftMemTempCost;
    SDouble          sLeftDiskTempCost;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sLeftTempScanCost;
    SDouble          sRightTempScanCost;
    UInt             sFlag;
    UInt             sTempType;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    //--------------------------------------
    // temp table Scan Cost
    //--------------------------------------
    if( aLeft->type == QMG_SELECTION ||
        aLeft->type == QMG_SHARD_SELECT || // PROJ-2638
        aLeft->type == QMG_PARTITION )
    {
        sLeftTempScanCost = ( sLeftCostInfo->outputRecordCnt *
                              aLeft->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sLeftTempScanCost = sLeftCostInfo->outputRecordCnt;
    }

    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( sRightCostInfo->outputRecordCnt *
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = sRightCostInfo->outputRecordCnt;
    }

    //--------------------------------------
    // temp table build Cost
    //--------------------------------------
    sFlag     = aJoinMethodCost->flag;
    sTempType = (sFlag & QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_LEFT_NODE_MASK) )
    {
        case QMO_JOIN_LEFT_NODE_NONE :
            sLeftTempScanCost  = 0;
            sLeftMemTempCost   = 0;
            sLeftDiskTempCost  = 0;
            break;

        case QMO_JOIN_LEFT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_LEFT_STORAGE_DISK )
            {
                sLeftMemTempCost = getMemStoreTempCost( aSystemStat,
                                                        sLeftCostInfo );
                sLeftDiskTempCost = 0;
            }
            else
            {
                sLeftMemTempCost = getMemStoreTempCost( aSystemStat,
                                                        sLeftCostInfo );

                sLeftDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                          sLeftCostInfo->outputRecordCnt,
                                                          sLeftCostInfo->recordSize,
                                                          QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_LEFT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_LEFT_STORAGE_DISK )
            {
                sLeftMemTempCost = getMemSortTempCost( aSystemStat,
                                                       sLeftCostInfo,
                                                       1 ); // nodeCnt
                sLeftDiskTempCost = 0;
            }
            else
            {
                sLeftMemTempCost = getMemSortTempCost( aSystemStat,
                                                       sLeftCostInfo,
                                                       1 ); // nodeCnt

                sLeftDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                         sLeftCostInfo,
                                                         1, // nodeCnt
                                                         sLeftCostInfo->recordSize );
            }
            break;

        default :
            sLeftMemTempCost   = 0;
            sLeftDiskTempCost  = 0;
            IDE_DASSERT ( 0 );
    }


    sTempType = (sFlag & QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_RIGHT_NODE_MASK) )
    {
        case QMO_JOIN_RIGHT_NODE_NONE :
            sRightTempScanCost  = 0;
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            break;

        case QMO_JOIN_RIGHT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                           sRightCostInfo->outputRecordCnt,
                                                           sRightCostInfo->recordSize,
                                                           QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_RIGHT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt
                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                          sRightCostInfo,
                                                          1, // nodeCnt
                                                          sRightCostInfo->recordSize );
            }
            break;

        default :
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            IDE_DASSERT ( 0 );
    }

    *aMemCost =  sLeftCostInfo->totalAccessCost +
                 sRightCostInfo->totalAccessCost +
                 sLeftMemTempCost +
                 sRightMemTempCost +
               ( sLeftTempScanCost + ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) ) +
                 aFilterCost;

    *aDiskCost =  sLeftCostInfo->totalDiskCost +
                  sRightCostInfo->totalDiskCost +
                  sLeftDiskTempCost +
                  sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getInverseSortJoinCost( qmoJoinMethodCost   * aJoinMethodCost,
                                         qmgGraph            * aLeft,
                                         qmgGraph            * aRight,
                                         SDouble               aFilterCost,
                                         qmoSystemStatistics * aSystemStat,
                                         SDouble             * aMemCost,
                                         SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : inverse sort join
 *               left scan cost + right scan cost + right temp build cost +
 *               ( left output * right temp scan cost ) +
 *               right temp scan cost
 *****************************************************************************/

    SDouble          sCost;
    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;
    UInt             sFlag;
    UInt             sTempType;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    if( ( aRight->type == QMG_SELECTION ) ||
        ( aRight->type == QMG_SHARD_SELECT ) || // PROJ-2638
        ( aRight->type == QMG_PARTITION ) )
    {
        sRightTempScanCost = ( sRightCostInfo->outputRecordCnt *
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = sRightCostInfo->outputRecordCnt;
    }

    sFlag     = aJoinMethodCost->flag;
    sTempType = (sFlag & QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_RIGHT_NODE_MASK) )
    {
        case QMO_JOIN_RIGHT_NODE_NONE :
            sRightTempScanCost  = 0;
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            break;

        case QMO_JOIN_RIGHT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                           sRightCostInfo->outputRecordCnt,
                                                           sRightCostInfo->recordSize,
                                                           QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_RIGHT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                          sRightCostInfo,
                                                          1, // nodeCnt
                                                          sRightCostInfo->recordSize );
            }
            break;

        default :
            sRightMemTempCost   = 0;
            sRightDiskTempCost  = 0;
            IDE_DASSERT ( 0 );
    }

    // PROJ-2385 add sRightTempScanCost
    *aMemCost =  sLeftCostInfo->totalAccessCost +
                 sRightCostInfo->totalAccessCost +
                 sRightMemTempCost +
               ( sLeftCostInfo->outputRecordCnt * sRightTempScanCost ) +
               ( sRightCostInfo->outputRecordCnt * sRightTempScanCost ) +
                 aFilterCost;

    *aDiskCost = sLeftCostInfo->totalDiskCost +
                 sRightCostInfo->totalDiskCost +
                 sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

SDouble qmoCost::getMergeJoinCost( qmoJoinMethodCost   * aJoinMethodCost,
                                   qmgGraph            * aLeft,
                                   qmgGraph            * aRight,
                                   qmoAccessMethod     * aLeftSelMethod,
                                   qmoAccessMethod     * aRightSelMethod,
                                   SDouble               aFilterCost,
                                   qmoSystemStatistics * aSystemStat,
                                   SDouble             * aMemCost,
                                   SDouble             * aDiskCost )
{
/******************************************************************************
 *
 * Description : merge join
 *               left scan cost + right scan cost +
 *               left temp build cost + right temp build cost
 *****************************************************************************/

    SDouble          sCost;

    SDouble          sLeftAccessCost;
    SDouble          sLeftDiskCost;
    SDouble          sLeftMemTempCost;
    SDouble          sLeftDiskTempCost;
    SDouble          sLeftTempScanCost;

    SDouble          sRightAccessCost;
    SDouble          sRightDiskCost;
    SDouble          sRightMemTempCost;
    SDouble          sRightDiskTempCost;
    SDouble          sRightTempScanCost;

    qmgCostInfo    * sLeftCostInfo;
    qmgCostInfo    * sRightCostInfo;
    UInt             sFlag;
    UInt             sTempType;

    sLeftCostInfo  = &(aLeft->costInfo);
    sRightCostInfo = &(aRight->costInfo);

    //--------------------------------------
    // temp table scan Cost
    //--------------------------------------
    if( aLeft->type == QMG_SELECTION ||
        aLeft->type == QMG_SHARD_SELECT || // PROJ-2638
        aLeft->type == QMG_PARTITION )
    {
        sLeftTempScanCost = ( sLeftCostInfo->outputRecordCnt *
                              aLeft->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sLeftTempScanCost = sLeftCostInfo->outputRecordCnt;
    }

    if( aRight->type == QMG_SELECTION ||
        aRight->type == QMG_SHARD_SELECT || // PROJ-2638
        aRight->type == QMG_PARTITION )
    {
        sRightTempScanCost = ( sRightCostInfo->outputRecordCnt *
                               aRight->myFrom->tableRef->statInfo->readRowTime );
    }
    else
    {
        sRightTempScanCost = sRightCostInfo->outputRecordCnt;
    }

    //--------------------------------------
    // temp table build Cost
    //--------------------------------------
    sFlag     = aJoinMethodCost->flag;
    sTempType = (sFlag & QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_LEFT_NODE_MASK) )
    {
        case QMO_JOIN_LEFT_NODE_NONE :
            sLeftTempScanCost  = 0;
            sLeftMemTempCost   = 0;
            sLeftDiskTempCost  = 0;
            break;

        case QMO_JOIN_LEFT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_LEFT_STORAGE_DISK )
            {
                sLeftMemTempCost = getMemStoreTempCost( aSystemStat,
                                                        sLeftCostInfo );
                sLeftDiskTempCost = 0;
            }
            else
            {
                sLeftMemTempCost = getMemStoreTempCost( aSystemStat,
                                                        sLeftCostInfo );

                sLeftDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                          sLeftCostInfo->outputRecordCnt,
                                                          sLeftCostInfo->recordSize,
                                                          QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_LEFT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_LEFT_STORAGE_DISK )
            {
                sLeftMemTempCost = getMemSortTempCost( aSystemStat,
                                                       sLeftCostInfo,
                                                       1 ); // nodeCnt
                sLeftDiskTempCost = 0;
            }
            else
            {
                sLeftMemTempCost = getMemSortTempCost( aSystemStat,
                                                       sLeftCostInfo,
                                                       1 ); // nodeCnt

                sLeftDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                         sLeftCostInfo,
                                                         1, // nodeCnt
                                                         sLeftCostInfo->recordSize );
            }
            break;

        default :
            sLeftMemTempCost   = 0;
            sLeftDiskTempCost  = 0;
            IDE_DASSERT ( 0 );
    }


    sTempType = (sFlag & QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    switch ( (sFlag & QMO_JOIN_RIGHT_NODE_MASK) )
    {
        case QMO_JOIN_RIGHT_NODE_NONE :
            sRightTempScanCost = 0;
            sRightMemTempCost  = 0;
            sRightDiskTempCost = 0;
            break;

        case QMO_JOIN_RIGHT_NODE_STORE :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );
                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemStoreTempCost( aSystemStat,
                                                         sRightCostInfo );

                sRightDiskTempCost = getDiskStoreTempCost( aSystemStat,
                                                           sRightCostInfo->outputRecordCnt,
                                                           sRightCostInfo->recordSize,
                                                           QMO_COST_STORE_SORT_TEMP );
            }
            break;

        case QMO_JOIN_RIGHT_NODE_SORTING :

            if( sTempType != QMO_JOIN_METHOD_RIGHT_STORAGE_DISK )
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt
                sRightDiskTempCost = 0;
            }
            else
            {
                sRightMemTempCost = getMemSortTempCost( aSystemStat,
                                                        sRightCostInfo,
                                                        1 ); // nodeCnt

                sRightDiskTempCost = getDiskSortTempCost( aSystemStat,
                                                          sRightCostInfo,
                                                          1, // nodeCnt
                                                          sRightCostInfo->recordSize );
            }
            break;

        default :
            sRightMemTempCost  = 0;
            sRightDiskTempCost = 0;
            IDE_DASSERT ( 0 );
    }

    //--------------------------------------
    // change index
    //--------------------------------------
    if( aJoinMethodCost->leftIdxInfo != NULL )
    {
        sLeftAccessCost = aLeftSelMethod->accessCost;
        sLeftDiskCost   = aLeftSelMethod->diskCost;
    }
    else
    {
        sLeftAccessCost = sLeftCostInfo->totalAccessCost;
        sLeftDiskCost   = sLeftCostInfo->totalDiskCost;
    }

    if( aJoinMethodCost->rightIdxInfo != NULL )
    {
        sRightAccessCost = aRightSelMethod->accessCost;
        sRightDiskCost   = aRightSelMethod->diskCost;
    }
    else
    {
        sRightAccessCost = sRightCostInfo->totalAccessCost;
        sRightDiskCost   = sRightCostInfo->totalDiskCost;
    }

    *aMemCost =  sLeftAccessCost +
                 sRightAccessCost +
                 sLeftMemTempCost +
                 sRightMemTempCost +
               ( sLeftTempScanCost + sRightTempScanCost ) +
                 aFilterCost;

    *aDiskCost = sLeftDiskCost +
                 sRightDiskCost +
                 sLeftDiskTempCost +
                 sRightDiskTempCost;

    sCost       = *aMemCost + *aDiskCost;

    return sCost;
}

/***********************************************************************
* Temp Table Cost
***********************************************************************/
SDouble qmoCost::getMemSortTempCost( qmoSystemStatistics * aSystemStat,
                                     qmgCostInfo         * aCostInfo,
                                     SDouble               aSortNodeCnt )
{
/******************************************************************************
 * Description : mem sort temp
 *               Quick sort �� ����Ѵ�.
 *****************************************************************************/

    SDouble sCost;
    SDouble sRowCnt;

    sRowCnt = aCostInfo->outputRecordCnt;

    //  Quick sort = 2Nlog(N)
    sCost  = 2 * sRowCnt * idlOS::log(sRowCnt) *
             aSystemStat->mCompareTime * aSortNodeCnt;

    sCost += getMemStoreTempCost( aSystemStat,
                                  aCostInfo );

    return sCost;
}

SDouble qmoCost::getMemHashTempCost( qmoSystemStatistics * aSystemStat,
                                     qmgCostInfo         * aCostInfo,
                                     SDouble               aHashNodeCnt )
{
/******************************************************************************
 * Description : hash sort temp
 *****************************************************************************/

    SDouble sCost;

    // BUG-40394 hash join cost�� �ʹ� �����ϴ�.
    // hash bucket �� 1/2 �� �����ϱ⶧���� 2.0 ���� �����Ѵ�.
    sCost  = aCostInfo->outputRecordCnt *
             aSystemStat->mHashTime *
             aHashNodeCnt * (2.0);

    sCost += getMemStoreTempCost( aSystemStat,
                                  aCostInfo );

    return sCost;
}

SDouble qmoCost::getMemStoreTempCost( qmoSystemStatistics * aSystemStat,
                                      qmgCostInfo         * aCostInfo )
{
/******************************************************************************
 * Description : mem store temp
 *               output * store time
 *****************************************************************************/

    SDouble sCost;

    sCost = aCostInfo->outputRecordCnt *
            aSystemStat->mStoreTime;

    return sCost;
}

SDouble qmoCost::getDiskSortTempCost( qmoSystemStatistics * aSystemStat,
                                      qmgCostInfo         * aCostInfo,
                                      SDouble               aSortNodeCnt,
                                      SDouble               aSortNodeLen )
{
/******************************************************************************
 * Description : disk sort temp
 *               �������� run ���� �и��� ������ run�� quick sort �Ѵ�.
 *               ���ĵ� run���� heap sort �Ѵ�.
 *****************************************************************************/

    SDouble  sCost;
    SDouble  sRowCnt;
    SDouble  sSortCnt;
    SDouble  sRunCnt;

    sRowCnt = aCostInfo->outputRecordCnt;

    IDE_DASSERT( aSortNodeCnt > 0 );
    IDE_DASSERT( aSortNodeLen > 0 );

    // �ѹ��� sort �Ҽ� �ִ� row�� ����
    sSortCnt = IDL_MIN( smuProperty::getSortAreaSize()                 *
                       (smuProperty::getTempSortGroupRatio() / 100.0 ) *
                        aSortNodeLen,
                        smuProperty::getTempSortPartitionSize() );

    sRunCnt  = idlOS::ceil(aCostInfo->outputRecordCnt / sSortCnt);

    //  Quick sort = 2Nlog(N)
    sCost  = 2 * sSortCnt * idlOS::log(sSortCnt) *
             aSystemStat->mCompareTime *
             aSortNodeCnt *
             sRunCnt;

    // heap sort
    sCost += sRowCnt * idlOS::log(sRowCnt) *
             aSystemStat->mCompareTime *
             aSortNodeCnt *
             idlOS::log(sSortCnt);

    sCost += getDiskStoreTempCost( aSystemStat,
                                   aCostInfo->outputRecordCnt,
                                   aSortNodeLen,
                                   QMO_COST_STORE_SORT_TEMP );

    return sCost;
}

SDouble qmoCost::getDiskHashTempCost( qmoSystemStatistics * aSystemStat,
                                      qmgCostInfo         * aCostInfo,
                                      SDouble               aHashNodeCnt,
                                      SDouble               aHashNodeLen )
{
/******************************************************************************
 * Description : disk sort temp
 *               disk hash temp ���̺��� 2���� ������� �����Ѵ�.
 *               ����� UNIQUE ������� �� �����Ѵ�.
 *               ���� CLUSTER ����� ����ؾ� �Ѵ�.
 *****************************************************************************/

    SDouble sCost;

    IDE_DASSERT( aHashNodeCnt > 0 );
    IDE_DASSERT( aHashNodeLen > 0 );

    sCost  = aCostInfo->outputRecordCnt *
             aSystemStat->mHashTime *
             aHashNodeCnt;

    sCost += getDiskStoreTempCost( aSystemStat,
                                   aCostInfo->outputRecordCnt,
                                   aHashNodeLen,
                                   QMO_COST_STORE_HASH_TEMP );

    return sCost;
}

SDouble qmoCost::getDiskHashGroupCost( qmoSystemStatistics * aSystemStat,
                                       qmgCostInfo         * aCostInfo,
                                       SDouble               aGroupCnt,
                                       SDouble               aHashNodeCnt,
                                       SDouble               aHashNodeLen )
{
/******************************************************************************
 * Description : disk hash temp Group by
 *               hash group by �� ������ ���� �����Ѵ�.
 *               1. ������ group �� ���ؼ� 1���� ���ڵ常 �����Ѵ�.
 *               2. ������ group �� �ִ��� Ȯ���ϱ� ���� fetch �� �Ѵ�.
 *                   2.1 temp ������ ���ڸ��� �Ǹ� replace �� �Ͼ�� �ȴ�.
 *               cost = hash time +
 *                      replace time +
 *                      hash temp table store time
 *****************************************************************************/

    SDouble sCost;
    SDouble sUseAbleTempArea;
    SDouble sStoreSize;
    SDouble sReplaceCount;
    SDouble sReplaceRatio;

    IDE_DASSERT( aGroupCnt > 0 );
    IDE_DASSERT( aHashNodeCnt > 0 );
    IDE_DASSERT( aHashNodeLen > 0 );

    sCost  = aCostInfo->outputRecordCnt *
             aSystemStat->mHashTime *
             aHashNodeCnt;

    // Hash temp table �� ���� ũ�⸦ ���Ѵ�.
    sUseAbleTempArea = smuProperty::getHashAreaSize()    *
                     ( 1 - (smuProperty::getTempHashGroupRatio() / 100.0) );

    sStoreSize       = aGroupCnt * (aHashNodeLen + SMI_TR_HEADER_SIZE_FULL);

    ////////////////////////////////
    // ���۰� replace �Ǵ� Ƚ���� ���Ѵ�.
    ////////////////////////////////

    if(sStoreSize > sUseAbleTempArea)
    {
        // ������ group �� ���ڵ带 �������� fetch �ϴ°��
        // replace �� ���Ͼ�� �ȴ�. ������ ���� ���Ѵ�.
        // replace Ȯ�� =  1- (�׷� ���� / ��ü ���ڵ� ����)
        sReplaceRatio = 1 - (aGroupCnt / aCostInfo->outputRecordCnt);

        sReplaceRatio = IDL_MAX( sReplaceRatio, 0 );

        // replace �� �Ǵ� Ƚ���� temp �� �����ϰ� ���� ��������.
        sReplaceCount = aCostInfo->outputRecordCnt -
                        (sUseAbleTempArea / (aHashNodeLen + 24));

        sReplaceCount = IDL_MAX( sReplaceCount, 0 );

        sReplaceCount = sReplaceCount * sReplaceRatio;
    }
    else
    {
        sReplaceCount = 0;
    }

    // replace �� �߻��Ҷ����� IO �� �Ͼ�� �ȴ�.
    sCost += sReplaceCount *
             aSystemStat->singleIoScanTime;

    sCost += getDiskStoreTempCost( aSystemStat,
                                   aGroupCnt,
                                   aHashNodeLen,
                                   QMO_COST_STORE_HASH_TEMP );

    return sCost;
}

SDouble qmoCost::getDiskStoreTempCost( qmoSystemStatistics * aSystemStat,
                                       SDouble               aStoreRowCnt,
                                       SDouble               aStoreNodeLen,
                                       UShort                aTempTableType )
{
/******************************************************************************
 * Description : disk store temp cost
 *               temp table �������� ����� ����ϴ� ����� �ٸ���.
 *****************************************************************************/

    SDouble sCost;
    SDouble sUseAbleTempArea;
    SDouble sStoreSize;
    SDouble sStoreNodeLen;

    IDE_DASSERT( aStoreNodeLen > 0 );

    switch( aTempTableType )
    {
        case QMO_COST_STORE_SORT_TEMP :
            sUseAbleTempArea = smuProperty::getSortAreaSize()    *
                               smuProperty::getTempSortGroupRatio() / 100.0;

            sStoreNodeLen    = aStoreNodeLen + SMI_TR_HEADER_SIZE_FULL;
            break;

        case QMO_COST_STORE_HASH_TEMP :
            sUseAbleTempArea = smuProperty::getHashAreaSize()    *
                               ( 1- (smuProperty::getTempHashGroupRatio() / 100.0) );

            sStoreNodeLen    = aStoreNodeLen + SMI_TR_HEADER_SIZE_FULL;
            break;

        default :
            sUseAbleTempArea = 0;
            sStoreNodeLen    = aStoreNodeLen;
            IDE_DASSERT( 0 );
    }

    sStoreSize = IDL_MAX( ( aStoreRowCnt * sStoreNodeLen) - sUseAbleTempArea, 0 );

    sCost  = aStoreRowCnt * aSystemStat->mStoreTime;

    // DISK IO cost
    // TASK-6699 TPC-H ���� ����
    // 10 : TPC-H 100SF �� ���������� ������ ���̴�.
    sCost += idlOS::ceil((sStoreSize / smiGetPageSize( SMI_DISK_USER_TEMP ))) *
             aSystemStat->singleIoScanTime * 10;

    return sCost;
}

/***********************************************************************
* Filter Cost
***********************************************************************/
SDouble qmoCost::getKeyRangeCost( qmoSystemStatistics * aSystemStat,
                                  qmoStatistics       * aTableStat,
                                  qmoIdxCardInfo      * aIndexStat,
                                  qmoPredWrapper      * aKeyRange,
                                  SDouble               aKeyRangeSelectivity )
{
/******************************************************************************
 * Description : Key range cost
 *               key range ����� ������ŭ cost �����Ѵ�.
 *****************************************************************************/

    SDouble          sCost;
    qmoPredWrapper * sKeyRange;
    UInt             sKeyRangeNodeCnt = 0;
    SDouble          sLeafPageCnt;

    sLeafPageCnt = idlOS::ceil( aTableStat->totalRecordCnt /
                                aIndexStat->avgSlotCount );

    for( sKeyRange = aKeyRange;
         sKeyRange != NULL;
         sKeyRange = sKeyRange->next )
    {
        sKeyRangeNodeCnt++;
    }

    sCost = sLeafPageCnt *
            aKeyRangeSelectivity *
            aSystemStat->mMTCallTime *
            sKeyRangeNodeCnt;

    return sCost;
}

SDouble qmoCost::getKeyFilterCost( qmoSystemStatistics * aSystemStat,
                                   qmoPredWrapper      * aKeyFilter,
                                   SDouble               aLoopCnt )
{
/******************************************************************************
 * Description : Key filter cost
 *               key filter ����� ������ŭ cost �����Ѵ�.
 *****************************************************************************/

    SDouble          sCost;
    qmoPredWrapper * sKeyFilter;
    UInt             sKeyFilterNodeCnt = 0;

    for( sKeyFilter = aKeyFilter;
         sKeyFilter != NULL;
         sKeyFilter = sKeyFilter->next )
    {
        sKeyFilterNodeCnt++;
    }

    sCost = aLoopCnt * aSystemStat->mMTCallTime * sKeyFilterNodeCnt;

    return sCost;
}

UInt countFilterNode( mtcNode * aNode, SDouble * aSubQueryCost )
{
/*******************************************
 * Description : filter counter
 ********************************************/

    mtcNode * sNode  = aNode;
    mtcNode * sConvNode;
    UInt      sCount = 0;

    for( sNode   = aNode;
         sNode  != NULL;
         sNode   = sNode->next )
    {
        // Self Node
        // TPC-H 9 �� ���ǿ��� like ������ ���� �����ϰԵǸ� ������ �������� ��찡 �����Ѵ�.
        if( (sNode->module == &mtfLike) ||
            (sNode->module == &mtfNotLike) )
        {
            sCount += 12;
        }
        else
        {
            sCount += 1;
        }

        // TASK-6699 TPC-H ���� ����
        // QTC_NODE_CONVERSION_TRUE : 1+1 �� ���� ��������� ������ �Ϸ�� ����̴�.
        if( (((qtcNode*)sNode)->lflag & QTC_NODE_CONVERSION_MASK) == QTC_NODE_CONVERSION_TRUE )
        {
            continue;
        }
        else
        {
            // nothing todo.
        }

        if( (sNode->module == &(qtc::subqueryWrapperModule)) ||
            (sNode->module == &(qtc::subqueryModule)) )
        {
            IDE_DASSERT( ((qtcNode*)sNode)->subquery != NULL );

            if( ((qtcNode*)sNode)->subquery->myPlan->graph != NULL )
            {
                *aSubQueryCost += ((qtcNode*)sNode)->subquery->myPlan->graph->costInfo.totalAllCost;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            *aSubQueryCost += 0;
        }

        // Child Node
        sCount += countFilterNode( sNode->arguments, aSubQueryCost );

        // conversion Node
        for( sConvNode = sNode->conversion;
             sConvNode != NULL;
             sConvNode = sConvNode->conversion )
        {
            sCount++;
        }
    }

    return sCount;
}

SDouble qmoCost::getFilterCost4PredWrapper(
            qmoSystemStatistics * aSystemStat,
            qmoPredWrapper      * aFilter,
            SDouble               aLoopCnt )
{
/******************************************************************************
 * Description : filter cost
 *               filter ����� ������ŭ ����� �����Ѵ�.
 *               qmoPredWrapper �� filter �� ��������� ��� ����Լ�
 *****************************************************************************/

    SDouble          sCost = 0;
    qmoPredWrapper * sFilter;
    qmoPredicate     sTemp;

    for( sFilter = aFilter;
         sFilter != NULL;
         sFilter = sFilter->next )
    {
        // BUG-42480 qmoPredWrapper �� ��� qmoPredicate �� next �� ���󰡸� �ȵȴ�.
        idlOS::memcpy( &sTemp, sFilter->pred, ID_SIZEOF( qmoPredicate ) );
        sTemp.next = NULL;

        sCost += getFilterCost( aSystemStat,
                                &sTemp,
                                aLoopCnt );
    }

    return sCost;
}

SDouble qmoCost::getFilterCost( qmoSystemStatistics * aSystemStat,
                                qmoPredicate        * aFilter,
                                SDouble               aLoopCnt )
{
/******************************************************************************
 * Description : filter cost
 *               filter ����� ������ŭ ����� �����Ѵ�.
 *               qmoPredicate �� filter �� ��������� ��� ����Լ�
 *****************************************************************************/

    SDouble         sCost;
    SDouble         sSubQueryCost  = 0;

    qmoPredicate  * sPredicate;
    qmoPredicate  * sMorePredicate;
    mtcNode       * sNode;

    UInt            sFilterNodeCnt = 0;

    // predicate count
    for( sPredicate = aFilter;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        for( sMorePredicate = sPredicate;
             sMorePredicate != NULL;
             sMorePredicate = sMorePredicate->more )
        {
            // BUG-42480 OR, AND �϶� next �� ���󰡸� �ȵȴ�.
            if( ( sMorePredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                  == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                sNode = sMorePredicate->node->node.arguments;
            }
            else
            {
                sNode = (mtcNode*)sMorePredicate->node;
            }

            sFilterNodeCnt += countFilterNode( sNode, &sSubQueryCost );
        }
    }

    sCost  = aLoopCnt * aSystemStat->mMTCallTime * sFilterNodeCnt;

    // SubQuery�� 1���� ����ȴٰ� �����Ѵ�.
    // �پ��� ��찡 �����Ͽ� ����� �����ϱⰡ �����
    // �߸� �����Ѱ�� �ſ� �߸��� plan �� ���ü� �ִ�.
    sCost += sSubQueryCost;

    return sCost;
}

SDouble qmoCost::getTargetCost( qmoSystemStatistics * aSystemStat,
                                qmsTarget           * aTarget,
                                SDouble               aLoopCnt )
{
/******************************************************************************
 * Description : target cost
 *               target ����� ������ŭ ����� �����Ѵ�.
 *****************************************************************************/

    SDouble sCost;
    SDouble sSubQueryCost = 0;
    UInt    sNodeCnt      = 0;

    IDE_DASSERT(aTarget != NULL);

    /* BUG-40662 */
    sNodeCnt = countFilterNode( &(aTarget->targetColumn->node), &sSubQueryCost );

    sCost  = aLoopCnt * aSystemStat->mMTCallTime * sNodeCnt;

    // SubQuery�� 1���� ����ȴٰ� �����Ѵ�.
    // �پ��� ��찡 �����Ͽ� ����� �����ϱⰡ �����
    // �߸� �����Ѱ�� �ſ� �߸��� plan �� ���ü� �ִ�.
    sCost += sSubQueryCost;

    return sCost;
}

SDouble qmoCost::getAggrCost( qmoSystemStatistics * aSystemStat,
                              qmsAggNode          * aAggrNode,
                              SDouble               aLoopCnt )
{
    qmsAggNode * sAggr;
    SDouble      sCost;
    SDouble      sSubQueryCost = 0;
    UInt         sNodeCnt      = 0;

    for( sAggr = aAggrNode; sAggr != NULL; sAggr = sAggr->next )
    {
        sNodeCnt += countFilterNode( &(aAggrNode->aggr->node), &sSubQueryCost );
    }

    sCost = aLoopCnt * sNodeCnt * aSystemStat->mMTCallTime;

    return sCost;
}
