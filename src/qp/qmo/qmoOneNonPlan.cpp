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
 * $Id: qmoOneNonPlan.cpp 90978 2021-06-09 04:13:16Z donovan.seo $
 *
 * Description :
 *     Plan Generator
 *
 *     One-child Non-materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - SCAN ���
 *         - FILT ���
 *         - PROJ ���
 *         - GRBY ���
 *         - AGGR ���
 *         - CUNT ���
 *         - VIEW ���
 *         - VSCN ���
 *         - SCAN(for Partition) ��� PROJ-1502 PARTITIONED DISK TABLE
 *         - CNTR ��� PROJ-1405 ROWNUM
 *         - SDSE ���
 *         - SDEX ���
 *         - SDIN ���
 *
 *     ��� NODE���� �ʱ�ȭ �۾� , ���� �۾� , ������ �۾� ������ �̷��
 *     ����. �ʱ�ȭ �۾������� NODE�� �ڵ� ������ �Ҵ� �� �ʱ�ȭ �۾���
 *     �ϸ�, �����۾����� �� NODE���� �̷������ �� �۾� , �׸��� ������
 *     ���� ������ �۾������� data������ ũ��, dependencyó��, subquery��
 *     ó�� ���� �̷������.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcg.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmo.h>
#include <qmoOneNonPlan.h>
#include <qmoRidPred.h>
#include <qdnForeignKey.h>
#include <qmoPartition.h>
#include <qcuTemporaryObj.h>
#include <qmv.h>
#include <qmxResultCache.h>

extern mtfModule mtfEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfOr;
extern mtfModule mtfAnd;
extern mtfModule mtfAdd2;
extern mtfModule mtfSubtract2;
extern mtfModule mtfTo_char;

/*
 * PROJ-1832 New database link
 */
IDE_RC qmoOneNonPlan::allocAndCopyRemoteTableInfo(
    qcStatement             * aStatement,
    struct qmsTableRef      * aTableRef,
    SChar                  ** aDatabaseLinkName,
    SChar                  ** aRemoteQuery )
{
    UInt sLength = 0;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::allocAndCopyRemoteTableInfo::__FT__" );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( aTableRef->remoteTable->linkName.size + 1,
                                               (void **)aDatabaseLinkName )
              != IDE_SUCCESS );

    QC_STR_COPY( *aDatabaseLinkName, aTableRef->remoteTable->linkName );

    sLength = idlOS::strlen( aTableRef->remoteQuery );
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( sLength + 1,
                                             (void **)aRemoteQuery )
              != IDE_SUCCESS );

    idlOS::strncpy( *aRemoteQuery,
                    aTableRef->remoteQuery,
                    sLength );
    (*aRemoteQuery)[ sLength ] = '\0';

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1832 New database link
 */
IDE_RC qmoOneNonPlan::setCursorPropertiesForRemoteTable(
    qcStatement         * aStatement,
    smiCursorProperties * aCursorProperties,
    idBool                aIsStore,
    SChar               * aDatabaseLinkName,
    SChar               * aRemoteQuery,
    UInt                  aColumnCount,
    qcmColumn           * aColumns )
{
    UInt             i = 0;
    smiRemoteTable * sRemoteTable = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::setCursorPropertiesForRemoteTable::__FT__" );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( *sRemoteTable ),
                                               (void **)& sRemoteTable )
              != IDE_SUCCESS );

    /* BUG-37077 REMOTE_TABLE_STORE */
    sRemoteTable->mIsStore = aIsStore;

    sRemoteTable->mLinkName = aDatabaseLinkName;
    sRemoteTable->mRemoteQuery = aRemoteQuery;

    sRemoteTable->mColumnCount = aColumnCount;

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( *sRemoteTable->mColumns ) * aColumnCount,
                                             (void **)&( sRemoteTable->mColumns ) )
              != IDE_SUCCESS );

    for ( i  = 0;
          i < aColumnCount;
          i++ )
    {
        sRemoteTable->mColumns[i].mName = aColumns[i].name;
        sRemoteTable->mColumns[i].mTypeId = aColumns[i].basicInfo->module->id;
        sRemoteTable->mColumns[i].mPrecision = aColumns[i].basicInfo->precision;
        sRemoteTable->mColumns[i].mScale = aColumns[i].basicInfo->scale;
    }

    aCursorProperties->mRemoteTable = sRemoteTable;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmoOneNonPlan::makeSCAN( qcStatement  * aStatement ,
                                qmsQuerySet  * aQuerySet ,
                                qmsFrom      * aFrom ,
                                qmoSCANInfo  * aSCANInfo ,
                                qmnPlan      * aParent,
                                qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : SCAN ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - data ������ ũ�� ���
 *         - qmncSCAN�� �Ҵ� �� �ʱ�ȭ (display ���� , index ����)
 *         - limit ������ Ȯ��
 *         - select for update�� ���� ó��
 *     + ���� �۾�
 *         - Predicate�� �з�
 *             - constant�� ó��
 *             - �Է�Predicate������ index ������ ���� Predicate �з�
 *             - fixed , variable�� ����
 *             - qtcNode�� �� ��ȯ
 *             - smiRange���� ��ȯ
 *             - indexable min-max�� ó��
 *     + ������ �۾�
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncSCAN          * sSCAN;
    UInt                sDataNodeOffset;
    qmsParseTree      * sParseTree;
    qtcNode           * sPredicate[11];
    qcmIndex          * sIndices;
    UInt                i;
    UInt                sIndexCnt;
    qmoPredicate      * sCopiedPred;
    idBool              sScanLimit          = ID_TRUE;
    idBool              sInSubQueryKeyRange = ID_FALSE;
    mtcTuple          * sMtcTuple;
    idBool              sExistLobFilter     = ID_FALSE;
    qcDepInfo           sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeSCAN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aSCANInfo != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDU_FIT_POINT("qmoOneNonPlan::makeSCAN::alloc",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmncSCAN),
                                           (void**)& sSCAN)
             != IDE_SUCCESS);

    QMO_INIT_PLAN_NODE( sSCAN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SCAN ,
                        qmnSCAN ,
                        qmndSCAN,
                        sDataNodeOffset );

    // PROJ-2444
    sSCAN->plan.readyIt = qmnSCAN::readyIt;

    // BUG-15816
    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sIndices  = aFrom->tableRef->tableInfo->indices;
    sIndexCnt = aFrom->tableRef->tableInfo->indexCount;

    //----------------------------------
    // Table ���� ���� ����
    //----------------------------------

    sSCAN->tupleRowID = aFrom->tableRef->table;
    sSCAN->table    = aFrom->tableRef->tableInfo->tableHandle;
    sSCAN->tableSCN = aFrom->tableRef->tableSCN;

    /* PROJ-2359 Table/Partition Access Option */
    sSCAN->accessOption = aFrom->tableRef->tableInfo->accessOption;

    // PROJ-1502 PARTITIONED DISK TABLE
    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID].tableHandle =
        aFrom->tableRef->tableInfo->tableHandle;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sSCAN->flag = QMN_PLAN_FLAG_CLEAR;
    sSCAN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    if ( qcuTemporaryObj::isTemporaryTable( aFrom->tableRef->tableInfo ) == ID_TRUE )
    {
        sSCAN->flag &= ~QMNC_SCAN_TEMPORARY_TABLE_MASK;
        sSCAN->flag |= QMNC_SCAN_TEMPORARY_TABLE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &(sSCAN->cursorProperty), NULL );

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement ,
                                     sSCAN->tupleRowID ,
                                     &( sSCAN->plan.flag ) )
              != IDE_SUCCESS );

    // Previous Disable ����
    sSCAN->flag &= ~QMNC_SCAN_PREVIOUS_ENABLE_MASK;
    sSCAN->flag |= QMNC_SCAN_PREVIOUS_ENABLE_FALSE;

    // fix BUG-12167
    // �����ϴ� ���̺��� fixed or performance view������ ������ ����
    // �����ϴ� ���̺� ���� IS LOCK�� ������ ���� �Ǵ� ����.
    if ( ( aFrom->tableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
         ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
         ( aFrom->tableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) )
    {
        sSCAN->flag &= ~QMNC_SCAN_TABLE_FV_MASK;
        sSCAN->flag |= QMNC_SCAN_TABLE_FV_TRUE;

        if ( QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) == 1 )
        {
            sSCAN->flag &= ~QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK;
            sSCAN->flag |= QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE;
        }
        else
        {
            /* BUG-43006 FixedTable Indexing Filter
             * optimizer formance view propery �� 0�̶��
             * FixedTable �� index�� ���ٰ� ����������Ѵ�
             */
            sIndices  = NULL;
            sIndexCnt = 0;
        }
    }
    else
    {
        sSCAN->flag &= ~QMNC_SCAN_TABLE_FV_MASK;
        sSCAN->flag |= QMNC_SCAN_TABLE_FV_FALSE;
    }

    /* PROJ-1832 New database link */
    if ( aFrom->tableRef->remoteTable != NULL )
    {
        if ( aFrom->tableRef->remoteTable->mIsStore != ID_TRUE )
        {
            sSCAN->flag &= ~QMNC_SCAN_REMOTE_TABLE_MASK;
            sSCAN->flag |= QMNC_SCAN_REMOTE_TABLE_TRUE;
        }
        else
        {
            /* BUG-37077 REMOTE_TABLE_STORE */
            sSCAN->flag &= ~QMNC_SCAN_REMOTE_TABLE_STORE_MASK;
            sSCAN->flag |= QMNC_SCAN_REMOTE_TABLE_STORE_TRUE;
        }
    }
    else
    {
        sSCAN->flag &= ~QMNC_SCAN_REMOTE_TABLE_MASK;
        sSCAN->flag |= QMNC_SCAN_REMOTE_TABLE_FALSE;

        sSCAN->flag &= ~QMNC_SCAN_REMOTE_TABLE_STORE_MASK;
        sSCAN->flag |= QMNC_SCAN_REMOTE_TABLE_STORE_FALSE;
    }

    // PROJ-1705
    // partition table�� ���, tableRef->tableInfo->rowMovement ������ �ʿ��ѵ�,
    // �� ������ ���� �������� �ʰ�,
    // ��� ���̺� ���� code node�� tableRef������ �޾��ش�.
    sSCAN->tableRef = aFrom->tableRef;

    // PROJ-1618 Online Dump
    if ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE )
    {
        IDE_TEST_RAISE( aFrom->tableRef->mDumpObjList == NULL,
                        ERR_EMPTY_DUMP_OBJECT );
        sSCAN->dumpObject = aFrom->tableRef->mDumpObjList->mObjInfo;
    }
    else
    {
        sSCAN->dumpObject = NULL;
    }

    //----------------------------------
    // �ε��� ���� �� ���� ����
    //----------------------------------

    sSCAN->method.index = aSCANInfo->index;

    // Proj-1360 Queue
    // dequeue ������ ����� ���, ������ �������� ������
    // primary key�� �ش��ϴ� msgid ������ ���
    if ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE )
    {
        sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;
        sParseTree->queue->tableID = aFrom->tableRef->tableInfo->tableID;

        if ( aSCANInfo->index == NULL )
        {
            sSCAN->method.index = aFrom->tableRef->tableInfo->primaryKey;
        }
        else
        {
            /* nothing to do */
        }

        // fifo �ɼ��� �����Ǿ� ������ Ž�� ������ �����Ѵ�.
        if ( sParseTree->queue->isFifo == ID_TRUE )
        {
            sSCAN->flag &= ~QMNC_SCAN_TRAVERSE_MASK;
            sSCAN->flag |= QMNC_SCAN_TRAVERSE_FORWARD;
        }
        else
        {
            sSCAN->flag &= ~QMNC_SCAN_TRAVERSE_MASK;
            sSCAN->flag |= QMNC_SCAN_TRAVERSE_BACKWARD;
        }
    }
    else
    {
        // do nothing.
    }

    // BUG-36760 dequeue �ÿ� ������ �ε����� Ÿ����
    if ( sSCAN->method.index != NULL )
    {
        // To Fix PR-11562
        // Indexable MIN-MAX ����ȭ�� ����� ���
        // Preserved Order�� ���⼺�� ����, ���� �ش� ������
        // �������� �ʿ䰡 ����.
        // ���� �ڵ� ����

        //index ���� �� order by�� ���� traverse ���� ����
        //aSCANInfo->preservedOrder�� ���� �ε��� ����� �ٸ��� sSCAN->flag
        //�� BACKWARD�� �������־�� �Ѵ�.

        // fix BUG-31907
        // queue table�� ���� traverse direction�� sParseTree->queue->isFifo�� ���� �̹� �����Ǿ� �ִ�.
        if ( aStatement->myPlan->parseTree->stmtKind != QCI_STMT_DEQUEUE )
        {
            IDE_TEST( setDirectionInfo( &( sSCAN->flag ),
                                        aSCANInfo->index,
                                        aSCANInfo->preservedOrder )
                      != IDE_SUCCESS );
        }

        // To fix BUG-12742
        // index scan�� �����Ǿ� �ִ� ��츦 �����Ѵ�.
        if ( ( aSCANInfo->flag & QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK ) ==
             QMO_SCAN_INFO_FORCE_INDEX_SCAN_TRUE)
        {
            sSCAN->flag &= ~QMNC_SCAN_FORCE_INDEX_SCAN_MASK;
            sSCAN->flag |= QMNC_SCAN_FORCE_INDEX_SCAN_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // nothing to do
    }

    if( ( aSCANInfo->flag & QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK )
        == QMO_SCAN_INFO_NOTNULL_KEYRANGE_TRUE )
    {
        sSCAN->flag &= ~QMNC_SCAN_NOTNULL_RANGE_MASK;
        sSCAN->flag |= QMNC_SCAN_NOTNULL_RANGE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // Cursor Property�� ����
    //----------------------------------

    sSCAN->lockMode = SMI_LOCK_READ;
    sSCAN->cursorProperty.mLockWaitMicroSec = ID_ULONG_MAX;
    
    if ( (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT) ||
         (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE) ||
         (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE) )
    {
        sParseTree      = (qmsParseTree *)aStatement->myPlan->parseTree;
        if( sParseTree->forUpdate != NULL )
        {
            sSCAN->lockMode                         = SMI_LOCK_REPEATABLE;
            sSCAN->cursorProperty.mLockWaitMicroSec =
                sParseTree->forUpdate->lockWaitMicroSec;
            // Proj 1360 Queue
            // dequeue���� ���, row�� ������ ���� exclusive lock�� �䱸��
            if (sParseTree->forUpdate->isQueue == ID_TRUE)
            {
                sSCAN->flag   |= QMNC_SCAN_TABLE_QUEUE_TRUE;
                sSCAN->lockMode = SMI_LOCK_WRITE;
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

    /* PROJ-1832 New database link */
    if  ( ( ( sSCAN->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
            QMNC_SCAN_REMOTE_TABLE_TRUE ) ||
          ( ( sSCAN->flag & QMNC_SCAN_REMOTE_TABLE_STORE_MASK ) ==
            QMNC_SCAN_REMOTE_TABLE_STORE_TRUE ) )
    {
        IDE_TEST( allocAndCopyRemoteTableInfo( aStatement,
                                               aFrom->tableRef,
                                               &( sSCAN->databaseLinkName ),
                                               &( sSCAN->remoteQuery ) )
                  != IDE_SUCCESS );

        IDE_TEST( setCursorPropertiesForRemoteTable( aStatement,
                                                     &( sSCAN->cursorProperty ),
                                                     aFrom->tableRef->remoteTable->mIsStore,
                                                     sSCAN->databaseLinkName,
                                                     sSCAN->remoteQuery,
                                                     aFrom->tableRef->tableInfo->columnCount,
                                                     aFrom->tableRef->tableInfo->columns )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    /* PROJ-2402 Parallel Table Scan */
    sSCAN->cursorProperty.mParallelReadProperties.mThreadCnt =
        aSCANInfo->mParallelInfo.mDegree;
    sSCAN->cursorProperty.mParallelReadProperties.mThreadID =
        aSCANInfo->mParallelInfo.mSeqNo;
    
    //----------------------------------
    // Display ���� ����
    //----------------------------------

    IDE_TEST( qmg::setDisplayInfo( aFrom ,
                                   &( sSCAN->tableOwnerName ),
                                   &( sSCAN->tableName ),
                                   &( sSCAN->aliasName ) )
              != IDE_SUCCESS );

    /* BUG-44520 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Partition Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Partition Name�� �����ϵ��� �����մϴ�.
     */
    sSCAN->partitionName[0] = '\0';

    /* BUG-44633 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Index ID�� �����ϵ��� �����մϴ�.
     */
    sSCAN->partitionIndexId = 0;

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Predicate�� ó��
    //----------------------------------

    IDE_TEST( processPredicate( aStatement,
                                aQuerySet,
                                aSCANInfo->predicate,
                                aSCANInfo->constantPredicate,
                                aSCANInfo->ridPredicate,
                                aSCANInfo->index,
                                sSCAN->tupleRowID,
                                &( sSCAN->method.constantFilter ),
                                &( sSCAN->method.filter ),
                                &( sSCAN->method.lobFilter ),
                                &( sSCAN->method.subqueryFilter ),
                                &( sSCAN->method.varKeyRange ),
                                &( sSCAN->method.varKeyFilter ),
                                &( sSCAN->method.varKeyRange4Filter ),
                                &( sSCAN->method.varKeyFilter4Filter ),
                                &( sSCAN->method.fixKeyRange ),
                                &( sSCAN->method.fixKeyFilter ),
                                &( sSCAN->method.fixKeyRange4Print ),
                                &( sSCAN->method.fixKeyFilter4Print ),
                                &( sSCAN->method.ridRange ),
                                & sInSubQueryKeyRange )
              != IDE_SUCCESS );

    IDE_TEST( postProcessScanMethod( aStatement,
                                     & sSCAN->method,
                                     & sScanLimit )
              != IDE_SUCCESS );

    // BUG-20403 : table scan������ FILT�� �����ϴ���
    if ( sSCAN->method.lobFilter != NULL )
    {
        sExistLobFilter = ID_TRUE;
    }

    // queue���� nnf, lob, subquery filter�� �������� �ʴ´�.
    if ( (sSCAN->flag & QMNC_SCAN_TABLE_QUEUE_MASK) == QMNC_SCAN_TABLE_QUEUE_TRUE )
    {
        IDE_TEST_RAISE( ( aSCANInfo->nnfFilter != NULL ) ||
                        ( sSCAN->method.lobFilter != NULL ) ||
                        ( sSCAN->method.subqueryFilter != NULL ),
                        ERR_NOT_SUPPORT_FILTER );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // Predicate ���� Flag ���� ����
    //----------------------------------
    if ( sInSubQueryKeyRange == ID_TRUE )
    {
        sSCAN->flag &= ~QMNC_SCAN_INSUBQ_KEYRANGE_MASK;
        sSCAN->flag |= QMNC_SCAN_INSUBQ_KEYRANGE_TRUE;
    }
    else
    {
        sSCAN->flag &= ~QMNC_SCAN_INSUBQ_KEYRANGE_MASK;
        sSCAN->flag |= QMNC_SCAN_INSUBQ_KEYRANGE_FALSE;
    }

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sSCAN->sdf = aSCANInfo->sdf;

    if ( aSCANInfo->sdf != NULL )
    {
        IDE_DASSERT( sIndexCnt > 0 );

        // sdf�� basePlan�� �ܴ�.
        aSCANInfo->sdf->basePlan = &sSCAN->plan;

        // sdf�� index ������ŭ �ĺ��� �����Ѵ�.
        // ������ �ĺ��� filter/key range/key filter ������ �����Ѵ�.

        aSCANInfo->sdf->candidateCount = sIndexCnt;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmncScanMethod ) * sIndexCnt,
                                                 (void**)& aSCANInfo->sdf->candidate )
                  != IDE_SUCCESS );

        for ( i =0;
              i <sIndexCnt;
              i++ )
        {
            // selected index�� ���ؼ��� �տ��� sSCAN�� ���� �۾��� �����Ƿ�,
            // �ٽ� �۾��� �ʿ䰡 ����.
            // ��� ���ڸ��� full scan�� ���ؼ� �۾��� �Ѵ�.
            if ( &sIndices[i] != aSCANInfo->index )
            {
                aSCANInfo->sdf->candidate[i].index = &sIndices[i];
            }
            else
            {
                aSCANInfo->sdf->candidate[i].index = NULL;
            }

            IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM(aStatement),
                                                  aSCANInfo->sdf->predicate,
                                                  & sCopiedPred )
                      != IDE_SUCCESS );

            IDE_TEST( processPredicate( aStatement,
                                        aQuerySet,
                                        sCopiedPred,
                                        NULL, // aSCANInfo->constantPredicate,
                                        NULL, // aSCANInfo->ridPredicate
                                        aSCANInfo->sdf->candidate[i].index,
                                        sSCAN->tupleRowID,
                                        &( aSCANInfo->sdf->candidate[i].constantFilter ),
                                        &( aSCANInfo->sdf->candidate[i].filter ),
                                        &( aSCANInfo->sdf->candidate[i].lobFilter ),
                                        &( aSCANInfo->sdf->candidate[i].subqueryFilter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyRange ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyFilter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyRange4Filter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyFilter4Filter ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyRange ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyFilter ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyRange4Print ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyFilter4Print ),
                                        &( aSCANInfo->sdf->candidate[i].ridRange ),
                                        & sInSubQueryKeyRange ) // �ǹ̾��� ������.
                      != IDE_SUCCESS );

            IDE_TEST( postProcessScanMethod( aStatement,
                                             & aSCANInfo->sdf->candidate[i],
                                             & sScanLimit )
                      != IDE_SUCCESS );

            // BUG-20403 : table scan������ FILT�� �����ϴ���
            if( aSCANInfo->sdf->candidate[i].lobFilter != NULL )
            {
                sExistLobFilter = ID_TRUE;
            }
            else
            {
                // Nothing to do...
            }

            // queue���� nnf, lob, subquery filter�� �������� �ʴ´�.
            if ( (sSCAN->flag & QMNC_SCAN_TABLE_QUEUE_MASK) == QMNC_SCAN_TABLE_QUEUE_TRUE )
            {
                IDE_TEST_RAISE( ( aSCANInfo->nnfFilter != NULL ) ||
                                ( aSCANInfo->sdf->candidate[i].lobFilter != NULL ) ||
                                ( aSCANInfo->sdf->candidate[i].subqueryFilter != NULL ),
                                ERR_NOT_SUPPORT_FILTER );
            }
            else
            {
                // Nothing to do.
            }

            aSCANInfo->sdf->candidate[i].constantFilter =
                sSCAN->method.constantFilter;

            // fix BUG-19074
            //----------------------------------
            // sdf�� dependency ó��
            //----------------------------------

            sPredicate[0] = aSCANInfo->sdf->candidate[i].fixKeyRange;
            sPredicate[1] = aSCANInfo->sdf->candidate[i].varKeyRange;
            sPredicate[2] = aSCANInfo->sdf->candidate[i].varKeyRange4Filter;
            sPredicate[3] = aSCANInfo->sdf->candidate[i].fixKeyFilter;
            sPredicate[4] = aSCANInfo->sdf->candidate[i].varKeyFilter;
            sPredicate[5] = aSCANInfo->sdf->candidate[i].varKeyFilter4Filter;
            sPredicate[6] = aSCANInfo->sdf->candidate[i].filter;
            sPredicate[7] = aSCANInfo->sdf->candidate[i].subqueryFilter;

            IDE_TEST( qmoDependency::setDependency( aStatement,
                                                    aQuerySet,
                                                    & sSCAN->plan,
                                                    QMO_SCAN_DEPENDENCY,
                                                    sSCAN->tupleRowID,
                                                    NULL,
                                                    8,
                                                    sPredicate,
                                                    0,
                                                    NULL )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do...
    }

    //----------------------------------
    // NNF Filter ����
    //----------------------------------
    sSCAN->nnfFilter = aSCANInfo->nnfFilter;

    //----------------------------------
    // SCAN limit�� ����
    // bug-7792,20403 , ���� filter�� �����ϸ� default���� �����ϰ�
    // �׷��� ���� ��쿣 limit������ �����Ѵ�.
    //----------------------------------
    // fix BUG-25151 filter�� �ִ� ��쿡�� SCAN LIMIT ����
    // NNF FILTER�� �ִ� ��쿡�� scan limit�������� �ʴ´�.
    if( (sScanLimit == ID_FALSE) ||
        (sExistLobFilter == ID_TRUE) ||
        (sSCAN->nnfFilter != NULL) )
    {
        sSCAN->limit = NULL;

        // PR-13482
        // SCAN Limit�� �������� ���Ѱ��,
        // selection graph�� limit�� NULL�� ����� ���� ����
        aSCANInfo->limit = NULL;
    }
    else
    {
        sSCAN->limit = aSCANInfo->limit;
    }

    // �Ʒ��� �ǹ� ���� ������.
    // ������ qmnScan�� firstInit���� qmncSCAN�� limit ������ ������,
    // qmnd�� cursorProperty�� �� �ɹ��� ������.
    sSCAN->cursorProperty.mFirstReadRecordPos = 0;
    sSCAN->cursorProperty.mIsUndoLogging = ID_TRUE;
    sSCAN->cursorProperty.mReadRecordCount = ID_ULONG_MAX;

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // PROJ-1473
    // tuple�� column ������� ������. (��:view���η��� push projection����)
    //----------------------------------

    sMtcTuple = &(QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID]);

    // BUG-43705 lateral view�� simple view merging�� ���������� ����� �ٸ��ϴ�.
    if ( ( ( sMtcTuple->lflag & MTC_TUPLE_VIEW_PUSH_PROJ_MASK )
           == MTC_TUPLE_VIEW_PUSH_PROJ_TRUE ) &&
         ( ( sMtcTuple->lflag & MTC_TUPLE_LATERAL_VIEW_REF_MASK )
           == MTC_TUPLE_LATERAL_VIEW_REF_FALSE ) )
    {
        for ( i = 0;
              i < sMtcTuple->columnCount;
              i++ )
        {
            if ( ( sMtcTuple->columns[i].flag &
                   MTC_COLUMN_VIEW_COLUMN_PUSH_MASK )
                 == MTC_COLUMN_VIEW_COLUMN_PUSH_FALSE )
            {
                // BUG-25470
                // OUTER COLUMN REFERENCE�� �ִ� �÷���
                // �÷������� �������� �ʴ´�.
                if ( ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_USE_TARGET_MASK )
                       == MTC_COLUMN_USE_TARGET_TRUE )
                     &&
                     ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_USE_NOT_TARGET_MASK )
                       == MTC_COLUMN_USE_NOT_TARGET_FALSE )
                     &&
                     ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_OUTER_REFERENCE_MASK )
                       == MTC_COLUMN_OUTER_REFERENCE_FALSE ))
                {
                    // ���ǿ� ������ �ʴ� �÷�����
                    // view���η� push projection�� �����.
                    // ���ǿ� ���� �÷���������
                    sMtcTuple->columns[i].flag &= ~MTC_COLUMN_USE_COLUMN_MASK;
                    sMtcTuple->columns[i].flag |= MTC_COLUMN_USE_COLUMN_FALSE;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // ���ǿ� ���� �÷�
                // Nothing To Do
            }
        }
    }

    //----------------------------------
    // Host ������ ������
    // Constant Expression�� ����ȭ
    //----------------------------------
    if ( sSCAN->nnfFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    sSCAN->nnfFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] = sSCAN->method.fixKeyRange;
    sPredicate[1] = sSCAN->method.varKeyRange;
    sPredicate[2] = sSCAN->method.varKeyRange4Filter;
    sPredicate[3] = sSCAN->method.fixKeyFilter;
    sPredicate[4] = sSCAN->method.varKeyFilter;
    sPredicate[5] = sSCAN->method.varKeyFilter4Filter;
    sPredicate[6] = sSCAN->method.constantFilter;
    sPredicate[7] = sSCAN->method.filter;
    sPredicate[8] = sSCAN->method.subqueryFilter;
    sPredicate[9] = sSCAN->nnfFilter;
    sPredicate[10] = sSCAN->method.ridRange;

    //----------------------------------
    // PROJ-1473
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    for ( i  = 0;
          i <= 10;
          i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sSCAN->plan,
                                            QMO_SCAN_DEPENDENCY,
                                            sSCAN->tupleRowID,
                                            NULL,
                                            11,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    if ( aParent != NULL )
    {
        IDE_TEST( qmc::pushResultDesc( aStatement,
                                       aQuerySet,
                                       ID_FALSE,
                                       aParent->resultDesc,
                                       & sSCAN->plan.resultDesc )
                  != IDE_SUCCESS );

        // Join predicate�� push down�� ���, SCAN�� depInfo���� dependency�� �������� �� �ִ�.
        // Result descriptor�� ������ �����ϱ� ���� SCAN�� ID�� filtering�Ѵ�.
        qtc::dependencySet( sSCAN->tupleRowID, &sDepInfo );

        IDE_TEST( qmc::filterResultDesc( aStatement,
                                         & sSCAN->plan.resultDesc,
                                         & sDepInfo,
                                         &( aQuerySet->depInfo ) )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
        // UPDATE ���� ���� parent�� NULL�� �� �ִ�.
    }

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sSCAN->plan.mParallelDegree = aFrom->tableRef->mParallelDegree;

    // PROJ-2551 simple query ����ȭ
    // simple index scan�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleSCAN( aStatement, sSCAN ) != IDE_SUCCESS );

    /* PROJ-2632 */
    sSCAN->mSerialFilterOffset = QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize;
    sSCAN->mSerialFilterSize   = 0;
    sSCAN->mSerialFilterCount  = 0;
    sSCAN->mSerialFilterInfo   = NULL;

    IDE_TEST( buildSerialFilterInfo( aStatement,
                                     aQuerySet->SFWGH->hints,
                                     sSCAN->method.filter,
                                     &( sSCAN->mSerialFilterSize ),
                                     &( sSCAN->mSerialFilterCount ),
                                     &( sSCAN->mSerialFilterInfo ) )
              != IDE_SUCCESS );

    if ( sSCAN->mSerialFilterInfo != NULL)
    {
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize
            += QTC_GET_SERIAL_EXECUTE_DATA_SIZE( sSCAN->mSerialFilterSize );
    }
    else
    {
        /* Nothing to do */
    }

    *aPlan = (qmnPlan *)sSCAN;

    /* PROJ-2462 Result Cache */
    qmo::addTupleID2ResultCacheStack( aStatement,
                                      sSCAN->tupleRowID );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EMPTY_DUMP_OBJECT)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMO_EMPTY_DUMP_OBJECT));
    }
    IDE_EXCEPTION(ERR_NOT_SUPPORT_FILTER)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMO_NOT_ALLOWED_LOB_FILTER));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeSCAN4Partition( qcStatement     * aStatement,
                                   qmsQuerySet     * aQuerySet,
                                   qmsFrom         * aFrom,
                                   qmoSCANInfo     * aSCANInfo,
                                   qmsPartitionRef * aPartitionRef,
                                   qmnPlan        ** aPlan )
{
/***********************************************************************
 *
 * Description : SCAN ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - data ������ ũ�� ���
 *         - qmncSCAN�� �Ҵ� �� �ʱ�ȭ (display ���� , index ����)
 *         - limit ������ Ȯ��
 *         - select for update�� ���� ó��
 *     + ���� �۾�
 *         - Predicate�� �з�
 *             - constant�� ó��
 *             - �Է�Predicate������ index ������ ���� Predicate �з�
 *             - fixed , variable�� ����
 *             - qtcNode�� �� ��ȯ
 *             - smiRange���� ��ȯ
 *             - indexable min-max�� ó��
 *     + ������ �۾�
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncSCAN          * sSCAN;
    UInt                sDataNodeOffset;
    qmsParseTree      * sParseTree;
    qtcNode           * sPredicate[10];
    qcmIndex          * sIndices;
    UInt                i;
    UInt                sIndexCnt;
    qmoPredicate      * sCopiedPred;
    idBool              sScanLimit          = ID_TRUE;
    idBool              sInSubQueryKeyRange = ID_FALSE;
    mtcTuple          * sMtcTuple;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeSCAN4Partition::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aSCANInfo!= NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDU_FIT_POINT("qmoOneNonPlan::makeSCAN4Partition::alloc");
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncSCAN ),
                                               (void **)& sSCAN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSCAN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SCAN ,
                        qmnSCAN ,
                        qmndSCAN,
                        sDataNodeOffset );
    // PROJ-2444
    sSCAN->plan.readyIt         = qmnSCAN::readyIt;

    // BUG-15816
    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sIndices = aPartitionRef->partitionInfo->indices;
    sIndexCnt = aPartitionRef->partitionInfo->indexCount;

    //----------------------------------
    // Table / Partition ���� ���� ����
    //----------------------------------

    sSCAN->tupleRowID       = aPartitionRef->table;
    sSCAN->table            = aPartitionRef->partitionHandle;
    sSCAN->tableSCN         = aPartitionRef->partitionSCN;
    sSCAN->partitionRef     = aPartitionRef;
    sSCAN->tableRef         = aFrom->tableRef;

    /* PROJ-2359 Table/Partition Access Option */
    sSCAN->accessOption = aPartitionRef->partitionInfo->accessOption;

    // PROJ-1502 PARTITIONED DISK TABLE
    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID].tableHandle =
        aPartitionRef->partitionHandle;

    /* PROJ-2462 Result Cache */
    qmo::addTupleID2ResultCacheStack( aStatement,
                                      sSCAN->tupleRowID );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sSCAN->flag = QMN_PLAN_FLAG_CLEAR;
    sSCAN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement ,
                                     sSCAN->tupleRowID ,
                                     &( sSCAN->plan.flag ) )
              != IDE_SUCCESS );


    // tuple�� flag�� partition�� ���� tuple�̶�� ����.
    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID].lflag
        &= ~MTC_TUPLE_PARTITION_MASK;

    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID].lflag
        |= MTC_TUPLE_PARTITION_TRUE;

    // partition �� ���� scan�̶�� flag����
    sSCAN->flag &= ~QMNC_SCAN_FOR_PARTITION_MASK;
    sSCAN->flag |= QMNC_SCAN_FOR_PARTITION_TRUE;

    // Previous Disable ����
    sSCAN->flag &= ~QMNC_SCAN_PREVIOUS_ENABLE_MASK;
    sSCAN->flag |= QMNC_SCAN_PREVIOUS_ENABLE_FALSE;

    sSCAN->flag &= ~QMNC_SCAN_TABLE_FV_MASK;
    sSCAN->flag |= QMNC_SCAN_TABLE_FV_FALSE;

    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &(sSCAN->cursorProperty), NULL );

    //----------------------------------
    // �ε��� ���� �� ���� ����
    //----------------------------------

    sSCAN->method.index        = aSCANInfo->index;

    if( aSCANInfo->index != NULL )
    {
        // To Fix PR-11562
        // Indexable MIN-MAX ����ȭ�� ����� ���
        // Preserved Order�� ���⼺�� ����, ���� �ش� ������
        // �������� �ʿ䰡 ����.
        // ���� �ڵ� ����

        //index ���� �� order by�� ���� traverse ���� ����
        //aSCANInfo->preservedOrder�� ���� �ε��� ����� �ٸ��� sSCAN->flag
        //�� BACKWARD�� �������־�� �Ѵ�.
        IDE_TEST( setDirectionInfo( &( sSCAN->flag ) ,
                                    aSCANInfo->index ,
                                    aSCANInfo->preservedOrder )
                  != IDE_SUCCESS );

        // To fix BUG-12742
        // index scan�� �����Ǿ� �ִ� ��츦 �����Ѵ�.
        if ( ( aSCANInfo->flag & QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK )
             == QMO_SCAN_INFO_FORCE_INDEX_SCAN_TRUE )
        {
            sSCAN->flag &= ~QMNC_SCAN_FORCE_INDEX_SCAN_MASK;
            sSCAN->flag |= QMNC_SCAN_FORCE_INDEX_SCAN_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // nothing to do
    }

    // rid scan�� �����Ǿ� �ִ� ��츦 �����Ѵ�.
    if ( ( aSCANInfo->flag & QMO_SCAN_INFO_FORCE_RID_SCAN_MASK )
         == QMO_SCAN_INFO_FORCE_RID_SCAN_TRUE)
    {
        sSCAN->flag &= ~QMNC_SCAN_FORCE_RID_SCAN_MASK;
        sSCAN->flag |= QMNC_SCAN_FORCE_RID_SCAN_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    if ( ( aSCANInfo->flag & QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK )
         == QMO_SCAN_INFO_NOTNULL_KEYRANGE_TRUE )
    {
        sSCAN->flag &= ~QMNC_SCAN_NOTNULL_RANGE_MASK;
        sSCAN->flag |= QMNC_SCAN_NOTNULL_RANGE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // Cursor Property�� ����
    //----------------------------------

    SMI_CURSOR_PROP_INIT(&sSCAN->cursorProperty, NULL, NULL);

    sSCAN->lockMode = SMI_LOCK_READ;
    sSCAN->cursorProperty.mLockWaitMicroSec = ID_ULONG_MAX;

    if ( ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT ) ||
         ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE ) ||
         ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE ) )
    {
        sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
        if ( sParseTree->forUpdate != NULL )
        {
            sSCAN->lockMode                         = SMI_LOCK_REPEATABLE;
            sSCAN->cursorProperty.mLockWaitMicroSec =
                sParseTree->forUpdate->lockWaitMicroSec;
            // Proj 1360 Queue
            // dequeue���� ���, row�� ������ ���� exclusive lock�� �䱸��
            if (sParseTree->forUpdate->isQueue == ID_TRUE)
            {
                sSCAN->flag   |= QMNC_SCAN_TABLE_QUEUE_TRUE;
                sSCAN->lockMode = SMI_LOCK_WRITE;
            }
            else
            {
                /* Nothing to do */
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

    /* PROJ-2402 Parallel Table Scan */
    sSCAN->cursorProperty.mParallelReadProperties.mThreadCnt =
        aSCANInfo->mParallelInfo.mDegree;
    sSCAN->cursorProperty.mParallelReadProperties.mThreadID =
        aSCANInfo->mParallelInfo.mSeqNo;

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    IDE_TEST( qmg::setDisplayInfo( aFrom ,
                                   &( sSCAN->tableOwnerName ) ,
                                   &( sSCAN->tableName ) ,
                                   &( sSCAN->aliasName ) )
              != IDE_SUCCESS );

    /* BUG-44520 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Partition Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Partition Name�� �����ϵ��� �����մϴ�.
     */
    (void)idlOS::memcpy( sSCAN->partitionName,
                         aPartitionRef->partitionInfo->name,
                         idlOS::strlen( aPartitionRef->partitionInfo->name ) + 1 );

    /* BUG-44633 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
     *           Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  SCAN Node���� Index ID�� �����ϵ��� �����մϴ�.
     */
    if ( aSCANInfo->index != NULL )
    {
        sSCAN->partitionIndexId = aSCANInfo->index->indexId;
    }
    else
    {
        sSCAN->partitionIndexId = 0;
    }

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // Predicate�� ó��
    //----------------------------------

    IDE_TEST( processPredicate( aStatement,
                                aQuerySet,
                                aSCANInfo->predicate,
                                aSCANInfo->constantPredicate,
                                aSCANInfo->ridPredicate,
                                aSCANInfo->index,
                                sSCAN->tupleRowID,
                                &( sSCAN->method.constantFilter ),
                                &( sSCAN->method.filter ),
                                &( sSCAN->method.lobFilter ),
                                &( sSCAN->method.subqueryFilter ),
                                &( sSCAN->method.varKeyRange ),
                                &( sSCAN->method.varKeyFilter ),
                                &( sSCAN->method.varKeyRange4Filter ),
                                &( sSCAN->method.varKeyFilter4Filter ),
                                &( sSCAN->method.fixKeyRange ),
                                &( sSCAN->method.fixKeyFilter ),
                                &( sSCAN->method.fixKeyRange4Print ),
                                &( sSCAN->method.fixKeyFilter4Print ),
                                &( sSCAN->method.ridRange ),
                                & sInSubQueryKeyRange )
              != IDE_SUCCESS );

    IDE_TEST( postProcessScanMethod( aStatement,
                                     & sSCAN->method,
                                     & sScanLimit )
              != IDE_SUCCESS );

    // queue���� nnf, lob, subquery filter�� �������� �ʴ´�.
    if ( (sSCAN->flag & QMNC_SCAN_TABLE_QUEUE_MASK) == QMNC_SCAN_TABLE_QUEUE_TRUE )
    {
        IDE_TEST_RAISE( ( aSCANInfo->nnfFilter != NULL ) ||
                        ( sSCAN->method.lobFilter != NULL ) ||
                        ( sSCAN->method.subqueryFilter != NULL ),
                        ERR_NOT_SUPPORT_FILTER );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // Predicate ���� Flag ���� ����
    //----------------------------------
    if ( sInSubQueryKeyRange == ID_TRUE )
    {
        IDE_DASSERT(0);
    }
    else
    {
        sSCAN->flag &= ~QMNC_SCAN_INSUBQ_KEYRANGE_MASK;
        sSCAN->flag |= QMNC_SCAN_INSUBQ_KEYRANGE_FALSE;
    }

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sSCAN->sdf = aSCANInfo->sdf;

    if ( aSCANInfo->sdf != NULL )
    {
        IDE_DASSERT( sIndexCnt > 0 );

        // sdf�� basePlan�� �ܴ�.
        aSCANInfo->sdf->basePlan = &sSCAN->plan;

        // sdf�� index ������ŭ �ĺ��� �����Ѵ�.
        // ������ �ĺ��� filter/key range/key filter ������ �����Ѵ�.

        aSCANInfo->sdf->candidateCount = sIndexCnt;

        IDU_FIT_POINT( "qmoOneNonPlan::makeSCAN4Partition::alloc::candidate" );
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncScanMethod ) * sIndexCnt,
                                                   (void**)& aSCANInfo->sdf->candidate )
                  != IDE_SUCCESS );

        for ( i = 0;
              i < sIndexCnt;
              i++ )
        {
            // selected index�� ���ؼ��� �տ��� sSCAN�� ���� �۾��� �����Ƿ�,
            // �ٽ� �۾��� �ʿ䰡 ����.
            // ��� ���ڸ��� full scan�� ���ؼ� �۾��� �Ѵ�.
            if ( &sIndices[i] != aSCANInfo->index )
            {
                aSCANInfo->sdf->candidate[i].index = &sIndices[i];
            }
            else
            {
                aSCANInfo->sdf->candidate[i].index = NULL;
            }

            IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM(aStatement),
                                                  aSCANInfo->sdf->predicate,
                                                  & sCopiedPred )
                      != IDE_SUCCESS );

            IDE_TEST( processPredicate( aStatement,
                                        aQuerySet,
                                        sCopiedPred,
                                        NULL, //aSCANInfo->constantPredicate,
                                        NULL, // rid predicate
                                        aSCANInfo->sdf->candidate[i].index,
                                        sSCAN->tupleRowID,
                                        &( aSCANInfo->sdf->candidate[i].constantFilter ),
                                        &( aSCANInfo->sdf->candidate[i].filter ),
                                        &( aSCANInfo->sdf->candidate[i].lobFilter ),
                                        &( aSCANInfo->sdf->candidate[i].subqueryFilter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyRange ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyFilter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyRange4Filter ),
                                        &( aSCANInfo->sdf->candidate[i].varKeyFilter4Filter ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyRange ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyFilter ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyRange4Print ),
                                        &( aSCANInfo->sdf->candidate[i].fixKeyFilter4Print ),
                                        &( aSCANInfo->sdf->candidate[i].ridRange ),
                                        & sInSubQueryKeyRange ) // �ǹ̾��� ������.
                      != IDE_SUCCESS );

            IDE_TEST( postProcessScanMethod( aStatement,
                                             & aSCANInfo->sdf->candidate[i],
                                             & sScanLimit )
                      != IDE_SUCCESS );

            // queue���� nnf, lob, subquery filter�� �������� �ʴ´�.
            if ( (sSCAN->flag & QMNC_SCAN_TABLE_QUEUE_MASK) == QMNC_SCAN_TABLE_QUEUE_TRUE )
            {
                IDE_TEST_RAISE( ( aSCANInfo->nnfFilter != NULL ) ||
                                ( aSCANInfo->sdf->candidate[i].lobFilter != NULL ) ||
                                ( aSCANInfo->sdf->candidate[i].subqueryFilter != NULL ),
                                ERR_NOT_SUPPORT_FILTER );
            }
            else
            {
                // Nothing to do.
            }

            aSCANInfo->sdf->candidate[i].constantFilter =
                sSCAN->method.constantFilter;

            // fix BUG-19074
            //----------------------------------
            // sdf�� dependency ó��
            //----------------------------------

            sPredicate[0] = aSCANInfo->sdf->candidate[i].fixKeyRange;
            sPredicate[1] = aSCANInfo->sdf->candidate[i].varKeyRange;
            sPredicate[2] = aSCANInfo->sdf->candidate[i].varKeyRange4Filter;
            sPredicate[3] = aSCANInfo->sdf->candidate[i].fixKeyFilter;
            sPredicate[4] = aSCANInfo->sdf->candidate[i].varKeyFilter;
            sPredicate[5] = aSCANInfo->sdf->candidate[i].varKeyFilter4Filter;
            sPredicate[6] = aSCANInfo->sdf->candidate[i].filter;
            sPredicate[7] = aSCANInfo->sdf->candidate[i].subqueryFilter;

            IDE_TEST( qmoDependency::setDependency( aStatement,
                                                    aQuerySet,
                                                    & sSCAN->plan,
                                                    QMO_SCAN_DEPENDENCY,
                                                    sSCAN->tupleRowID,
                                                    NULL,
                                                    8,
                                                    sPredicate,
                                                    0,
                                                    NULL )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do...
    }

    //----------------------------------
    // SCAN limit�� ����
    // bug-7792 , filter�� �����ϸ� default���� �����ϰ�
    // �׷��� ���� ��쿣 limit������ �����Ѵ�.
    //----------------------------------
    if ( sScanLimit == ID_FALSE )
    {
        sSCAN->limit = NULL;

        // PR-13482
        // SCAN Limit�� �������� ���Ѱ��,
        // selection graph�� limit�� NULL�� ����� ���� ����
        aSCANInfo->limit = NULL;
    }
    else
    {
        sSCAN->limit = aSCANInfo->limit;
    }

    // �Ʒ��� �ǹ� ���� ������.
    // ������ qmnScan�� firstInit���� qmncSCAN�� limit ������ ������,
    // qmnd�� cursorProperty�� �� �ɹ��� ������.
    sSCAN->cursorProperty.mFirstReadRecordPos = 0;
    sSCAN->cursorProperty.mIsUndoLogging = ID_TRUE;
    sSCAN->cursorProperty.mReadRecordCount = ID_ULONG_MAX;

    //----------------------------------
    // NNF Filter ����
    //----------------------------------
    sSCAN->nnfFilter = aSCANInfo->nnfFilter;


    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //----------------------------------
    // PROJ-1473
    // tuple�� column ������� ������. (��:view���η��� push projection����)
    //----------------------------------

    sMtcTuple = &(QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSCAN->tupleRowID]);

    // BUG-43705 lateral view�� simple view merging�� ���������� ����� �ٸ��ϴ�.
    if ( ( ( sMtcTuple->lflag & MTC_TUPLE_VIEW_PUSH_PROJ_MASK )
           == MTC_TUPLE_VIEW_PUSH_PROJ_TRUE ) &&
         ( ( sMtcTuple->lflag & MTC_TUPLE_LATERAL_VIEW_REF_MASK )
           == MTC_TUPLE_LATERAL_VIEW_REF_FALSE ) )
    {
        for ( i = 0;
              i < sMtcTuple->columnCount;
              i++ )
        {
            if ( ( sMtcTuple->columns[i].flag &
                   MTC_COLUMN_VIEW_COLUMN_PUSH_MASK )
                 == MTC_COLUMN_VIEW_COLUMN_PUSH_FALSE )
            {
                // BUG-25470
                // OUTER COLUMN REFERENCE�� �ִ� �÷���
                // �÷������� �������� �ʴ´�.
                if ( ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_USE_TARGET_MASK )
                       == MTC_COLUMN_USE_TARGET_TRUE )
                     &&
                     ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_USE_NOT_TARGET_MASK )
                       == MTC_COLUMN_USE_NOT_TARGET_FALSE )
                     &&
                     ( ( sMtcTuple->columns[i].flag &
                         MTC_COLUMN_OUTER_REFERENCE_MASK )
                       == MTC_COLUMN_OUTER_REFERENCE_FALSE ))
                {
                    // ���ǿ� ������ �ʴ� �÷�����
                    // view���η� push projection�� �����.
                    // ���ǿ� ���� �÷���������

                    sMtcTuple->columns[i].flag &= ~MTC_COLUMN_USE_COLUMN_MASK;
                    sMtcTuple->columns[i].flag |= MTC_COLUMN_USE_COLUMN_FALSE;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // ���ǿ� ���� �÷�
                // Nothing To Do
            }
        }
    }

    //----------------------------------
    // Host ������ ������
    // Constant Expression�� ����ȭ
    //----------------------------------
    if ( sSCAN->nnfFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    sSCAN->nnfFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] = sSCAN->method.fixKeyRange;
    sPredicate[1] = sSCAN->method.varKeyRange;
    sPredicate[2] = sSCAN->method.varKeyRange4Filter;;
    sPredicate[3] = sSCAN->method.fixKeyFilter;
    sPredicate[4] = sSCAN->method.varKeyFilter;
    sPredicate[5] = sSCAN->method.varKeyFilter4Filter;
    sPredicate[6] = sSCAN->method.constantFilter;
    sPredicate[7] = sSCAN->method.filter;
    sPredicate[8] = sSCAN->method.subqueryFilter;
    sPredicate[9] = sSCAN->nnfFilter;

    //----------------------------------
    // PROJ-1473
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    for ( i  = 0;
          i <= 9;
          i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sSCAN->plan,
                                            QMO_SCAN_DEPENDENCY,
                                            sSCAN->tupleRowID,
                                            NULL,
                                            10,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    sSCAN->plan.resultDesc = NULL;

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sSCAN->plan.mParallelDegree = aFrom->tableRef->mParallelDegree;

    // PROJ-2551 simple query ����ȭ
    // simple index scan�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleSCAN( aStatement, sSCAN ) != IDE_SUCCESS );

    /* PROJ-2632 */
    sSCAN->mSerialFilterOffset = QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize;
    sSCAN->mSerialFilterSize   = 0;
    sSCAN->mSerialFilterCount  = 0;
    sSCAN->mSerialFilterInfo   = NULL;

    IDE_TEST( buildSerialFilterInfo( aStatement,
                                     aQuerySet->SFWGH->hints,
                                     sSCAN->method.filter,
                                     &( sSCAN->mSerialFilterSize ),
                                     &( sSCAN->mSerialFilterCount ),
                                     &( sSCAN->mSerialFilterInfo ) )
              != IDE_SUCCESS );

    if ( sSCAN->mSerialFilterInfo != NULL)
    {
        QC_SHARED_TMPLATE( aStatement )->tmplate.dataSize
            += QTC_GET_SERIAL_EXECUTE_DATA_SIZE( sSCAN->mSerialFilterSize );
    }
    else
    {
        /* Nothing to do */
    }

    *aPlan = (qmnPlan *)sSCAN;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_SUPPORT_FILTER)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMO_NOT_ALLOWED_LOB_FILTER));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::postProcessScanMethod( qcStatement    * aStatement,
                                      qmncScanMethod * aMethod,
                                      idBool         * aScanLimit )
{
    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::postProcessScanMethod::__FT__" );

    IDE_DASSERT( aMethod != NULL );

    // fix BUG-25151 filter�� �ִ� ��쿡�� SCAN LIMIT ����
    if ( aMethod->subqueryFilter != NULL )
    {
        *aScanLimit = ID_FALSE;
    }
    else
    {
        // Nothing To Do
    }

    if ( aMethod->filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    if ( aMethod->subqueryFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->subqueryFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // BUG-17506
    if ( aMethod->varKeyRange4Filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->varKeyRange4Filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // BUG-17506
    if ( aMethod->varKeyFilter4Filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->varKeyFilter4Filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    if ( aMethod->ridRange != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aMethod->ridRange )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initFILT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aPredicate ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : FILT ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncFILT�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - filter , constant Predicate�� �з�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncFILT          * sFILT;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initFILT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncFILT ),
                                               (void **)& sFILT )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sFILT ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_FILT ,
                        qmnFILT ,
                        qmndFILT,
                        sDataNodeOffset );

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    if ( aPredicate != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        & sFILT->plan.resultDesc,
                                        aPredicate )
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
                                   & sFILT->plan.resultDesc )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sFILT;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeFILT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aPredicate ,
                         qmoPredicate * aConstantPredicate ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan )
{
    qmncFILT          * sFILT = (qmncFILT *)aPlan;
    UInt                sDataNodeOffset;
    qtcNode           * sPredicate[10];
    qtcNode           * sNode;
    qtcNode           * sOptimizedNode;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeFILT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aPredicate != NULL || aConstantPredicate != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    sFILT->plan.left        = aChildPlan;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndFILT));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sFILT->flag = QMN_PLAN_FLAG_CLEAR;
    sFILT->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sFILT->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    if ( aPredicate != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( aPredicate,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sFILT->filter = sOptimizedNode;
    }
    else
    {
        sFILT->filter = NULL;
    }

    if ( aConstantPredicate != NULL )
    {
        // constant Predicate�� ó��
        IDE_TEST( qmoPred::linkPredicate( aStatement ,
                                          aConstantPredicate ,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sNode )
                  != IDE_SUCCESS );

        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sFILT->constantFilter = sOptimizedNode;
    }
    else
    {
        sFILT->constantFilter = NULL;
    }

    // TO Fix PR-10182
    // HAVING ���� �����ϴ� PRIOR Column�� ���ؼ��� ID�� �����Ͽ��� �Ѵ�.
    if ( sFILT->filter != NULL )
    {
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sFILT->filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // Host ������ ������
    // Constant Expression�� ����ȭ
    //----------------------------------

    if ( sFILT->filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    sFILT->filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sFILT->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    if ( sFILT->filter != NULL )
    {
        IDE_TEST( qmc::duplicateGroupExpression( aStatement,
                                                 sFILT->filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] = sFILT->constantFilter;
    sPredicate[1] = sFILT->filter;

    //----------------------------------
    // PROJ-1473
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[1],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sFILT->plan,
                                            QMO_FILT_DEPENDENCY,
                                            0,
                                            NULL,
                                            2,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sFILT->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sFILT->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sFILT->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initPROJ( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
    qmncPROJ          * sPROJ;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initPROJ::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncPROJ ),
                                               (void **)& sPROJ )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sPROJ ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_PROJ ,
                        qmnPROJ ,
                        qmndPROJ,
                        sDataNodeOffset );

    if ( aParent == NULL )
    {
        // �Ϲ����� query-set�� projection
        IDE_TEST( qmc::createResultFromQuerySet( aStatement,
                                                 aQuerySet,
                                                 & sPROJ->plan.resultDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Query-set�� �ֻ��� projection�� �ƴ� ���
        // Ex) view�� ���� projection
        IDE_TEST( qmc::copyResultDesc( aStatement,
                                       aParent->resultDesc,
                                       & sPROJ->plan.resultDesc )
                  != IDE_SUCCESS );
    }

    /* PROJ-2462 Result Cache */
    if ( ( aQuerySet->lflag & QMV_QUERYSET_RESULT_CACHE_INVALID_MASK )
         == QMV_QUERYSET_RESULT_CACHE_INVALID_TRUE )
    {
        qmo::flushResultCacheStack( aStatement );
    }
    else
    {
        /* Nothing to do */
    }

    *aPlan = (qmnPlan *)sPROJ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makePROJ( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         UInt           aFlag,
                         qmsLimit     * aLimit,
                         qtcNode      * aLoopNode,
                         qmnPlan      * aChildPlan,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncPROJ�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - flag�� ���� (top , non-top || limit || indexable min-max )
 *         - Top PROJ , Non-top PROJ�϶��� ó��
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - PROJ ���� Projection Graph �Ǵ� Set Graph�κ��� �����ȴ�.
 *     - Non-top�϶��� ó���� ���� ����� �� �ʿ��ϴ�.
 *
 ***********************************************************************/

    qmncPROJ          * sPROJ = (qmncPROJ *)aPlan;
    UInt                sDataNodeOffset;

    qmsTarget         * sTarget;
    qmsTarget         * sNewTarget;
    qmsTarget         * sFirstTarget       = NULL;
    qmsTarget         * sLastTarget        = NULL;

    qtcNode           * sNewNode;
    qtcNode           * sFirstNode         = NULL;
    qtcNode           * sLastNode          = NULL;

    UShort              sColumnCount;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makePROJ::__FT__" );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndPROJ));

    sPROJ->myTargetOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sPROJ->flag = QMN_PLAN_FLAG_CLEAR;
    sPROJ->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sPROJ->plan.left  = aChildPlan;
    sPROJ->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    // PROJ-2462 Result Cache
    if ( ( aFlag & QMO_MAKEPROJ_TOP_RESULT_CACHE_MASK )
         == QMO_MAKEPROJ_TOP_RESULT_CACHE_TRUE )
    {
        sPROJ->flag &= ~QMNC_PROJ_TOP_RESULT_CACHE_MASK;
        sPROJ->flag |= QMNC_PROJ_TOP_RESULT_CACHE_TRUE;

        sPROJ->limit = NULL;
    }
    else
    {
        // limit ������ ����
        sPROJ->limit = aLimit;
        if( sPROJ->limit != NULL )
        {
            sPROJ->flag &= ~QMNC_PROJ_LIMIT_MASK;
            sPROJ->flag |= QMNC_PROJ_LIMIT_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // loop ������ ����
    sPROJ->loopNode = aLoopNode;
    
    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    // indexable min-max flag����
    if( (aFlag & QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK) ==
        QMO_MAKEPROJ_INDEXABLE_MINMAX_TRUE )
    {
        sPROJ->flag &= ~QMNC_PROJ_MINMAX_MASK;
        sPROJ->flag |= QMNC_PROJ_MINMAX_TRUE;
    }
    else
    {
        sPROJ->flag &= ~QMNC_PROJ_MINMAX_MASK;
        sPROJ->flag |= QMNC_PROJ_MINMAX_FALSE;
    }

    /* PROJ-1071 Parallel Query */
    if ((aFlag & QMO_MAKEPROJ_QUERYSET_TOP_MASK) ==
        QMO_MAKEPROJ_QUERYSET_TOP_TRUE)
    {
        sPROJ->flag &= ~QMNC_PROJ_QUERYSET_TOP_MASK;
        sPROJ->flag |= QMNC_PROJ_QUERYSET_TOP_TRUE;
    }
    else
    {
        sPROJ->flag &= ~QMNC_PROJ_QUERYSET_TOP_MASK;
        sPROJ->flag |= QMNC_PROJ_QUERYSET_TOP_FALSE;
    }

    // To Fix BUG-8026
    if ( aQuerySet->SFWGH != NULL )
    {
        sPROJ->level     = aQuerySet->SFWGH->level;
        sPROJ->isLeaf    = aQuerySet->SFWGH->isLeaf;
        sPROJ->loopLevel = aQuerySet->SFWGH->loopLevel;
    }
    else
    {
        // nothing to do
        sPROJ->level     = NULL;
        sPROJ->isLeaf    = NULL;
        sPROJ->loopLevel = NULL;
    }

    sPROJ->nextValSeqs = aStatement->myPlan->parseTree->nextValSeqs;

    if ( (aFlag & QMO_MAKEPROJ_TOP_MASK ) == QMO_MAKEPROJ_TOP_TRUE )
    {
        //----------------------------------
        // Top PROJ�� ���
        //----------------------------------

        sPROJ->flag &= ~QMNC_PROJ_TOP_MASK;
        sPROJ->flag |= QMNC_PROJ_TOP_TRUE;

        //Top�� ��쿡�� ������ QuerySet�� Target�� �޾��ش�.
        sPROJ->myTarget    = aQuerySet->target;

        for ( sItrAttr  = sPROJ->plan.resultDesc, sColumnCount = 0;
              sItrAttr != NULL ;
              sItrAttr  = sItrAttr->next , sColumnCount++ )
        {
            //----------------------------------
            // Prior Node�� ó��
            //----------------------------------

            // To Fix BUG-8026
            if ( aQuerySet->SFWGH != NULL )
            {
                IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                                   aQuerySet ,
                                                   sItrAttr->expr )
                          != IDE_SUCCESS );
            }
            else
            {
                // nothing to do
            }
        }
    }
    else
    {
        //----------------------------------
        // Non-top PROJ�� ���
        //----------------------------------
        sPROJ->flag &= ~QMNC_PROJ_TOP_MASK;
        sPROJ->flag |= QMNC_PROJ_TOP_FALSE;

        for ( sTarget  = aQuerySet->target,
                  sColumnCount = 0,
                  sItrAttr = sPROJ->plan.resultDesc;
              sTarget != NULL;
              sTarget  = sTarget->next,
                  sColumnCount++,
                  sItrAttr = sItrAttr->next )
        {
            //----------------------------------
            // Prior Node�� ó��
            //----------------------------------
            IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                               aQuerySet ,
                                               sItrAttr->expr )
                      != IDE_SUCCESS );

            //----------------------------------
            // Bug-7948
            // TOP-PROJ�� �ƴ� ��쿡 Target�� �����ؼ� �����Ѵ�.
            // ���� ��� STORE and SEARCH�� ���
            //
            //            (sub-query)
            //               |
            //              (i1)     -    (i2)
            //               ^              ^
            //               |              |
            // target-> (qmsTarget)  -  (qmsTarget)
            //
            // ���� ���� ����Ű�� �ִ� ��� ������ �ϴ� Materialize��忡��
            // target�� ���� ��Ű�Ƿ� ������ PROJ�� ��� target�� ����Ű��
            // �ȴ�. (�߸��� ��Ȳ)
            //
            // ����, qmsTarget�� qmsTarget->targetColum�� ��� �����ؼ�
            // PROJ->myTraget�� �޾��ֵ��� �Ѵ�.
            //
            //----------------------------------

            //������ Target�� �����Ѵ�.
            //alloc
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                    qmsTarget ,
                                    & sNewTarget )
                      != IDE_SUCCESS);
            //memcpy
            idlOS::memcpy( sNewTarget , sTarget , ID_SIZEOF(qmsTarget) );
            sNewTarget->next = NULL;

            //������ TargetColumn�� �����Ѵ�.
            //alloc
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                    qtcNode ,
                                    & sNewNode )
                      != IDE_SUCCESS);

            idlOS::memcpy( sNewNode,
                           sItrAttr->expr,
                           ID_SIZEOF(qtcNode ) );

            sItrAttr->expr = sNewNode;

            sNewTarget->targetColumn = sNewNode;

            // BUG-37204
            // proj�� result desc �� pass ��尡 �޷��ִ� ���
            // view ���� �߸��� ��带 ���� �ϰ� �˴ϴ�.
            if ( sItrAttr->expr->node.module == &qtc::passModule )
            {
                // proj �� ���� �׷������� pass ��尡 �̹� �����Ǿ� �������� ���
                // PASS ��带 �����ؾ� �ϰ�
                // proj �� ���� �׷������� pass ��带 �����ϴ� ���
                // PASS ����� ���ڸ� �����ؾ� �Ѵ�.
                // 2���� ��츦 �����Ҽ� �����Ƿ� PASS ���� ���ڸ� ��� ������.
                // ���⼭ ����� ���纻�� proj ��尡 ����ϰ� �Ǹ�
                // ������ view ��尡 ������ �����Ͽ� ����ϰ� �ȴ�.
                IDE_TEST(STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                       qtcNode ,
                                       & sNewNode )
                         != IDE_SUCCESS);

                idlOS::memcpy( sNewNode,
                               sItrAttr->expr->node.arguments,
                               ID_SIZEOF(qtcNode ) );

                sItrAttr->expr->node.arguments = (mtcNode*)sNewNode;
            }
            else
            {
                // nothing todo.
            }

            //connect : qmsTarget
            if ( sFirstTarget == NULL )
            {
                sFirstTarget = sNewTarget;
                sLastTarget  = sNewTarget;
            }
            else
            {
                sLastTarget->next = sNewTarget;
                sLastTarget       = sNewTarget;
            }

            //connect : targetColumn
            if ( sFirstNode == NULL )
            {
                sFirstNode = sNewNode;
                sLastNode  = sNewNode;
            }
            else
            {
                sLastNode->node.next = (mtcNode*)sNewNode;
                sLastNode = sNewNode;
            }
        }
        //non-Top�� ��쿡�� ������ Target�� �޾��ش�.
        sPROJ->myTarget    = sFirstTarget;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    sPROJ->targetCount = sColumnCount;
    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdNode) );

    for ( sItrAttr  = sPROJ->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sItrAttr->expr,
                                           ID_USHORT_MAX,
                                           ID_FALSE )
                  != IDE_SUCCESS );
    }

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sPROJ->plan ,
                                            QMO_PROJ_DEPENDENCY,
                                            0 ,
                                            sPROJ->myTarget ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sPROJ->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sPROJ->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sPROJ->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    // PROJ-2551 simple query ����ȭ
    // simple target�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimplePROJ( aStatement, sPROJ )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initGRBY( qcStatement       * aStatement ,
                         qmsQuerySet       * aQuerySet ,
                         qmsAggNode        * aAggrNode ,
                         qmsConcatElement  * aGroupColumn,
                         qmnPlan          ** aPlan )
{
/***********************************************************************
 *
 * Description : GRBY ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncGRBY�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - Tuple Set�� ���
 *         - GRBY�÷��� ���� & passNode�� ó��
 *         - Tupple Set�� �÷� ������ �µ��� �Ҵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     -������ �ΰ��� ���� ����̵ȴ�.
 *         - Sort-Based Grouping
 *         - Sort-Based Distinction
 *     -����, Sort-Based Grouping���� ����� �ɶ����� group by ������
 *      ���� �̿��� �ǹǷ� �̴� order by �� having���� ����� �ɼ� ����
 *      �Ƿ�, srcNode�� �����Ͽ� �÷��� �����ϰ�, group by�� dstNode�Ǵ�
 *      passNode�� ��ü �Ѵ�.
 *
 ***********************************************************************/
    qmncGRBY          * sGRBY;
    UInt                sDataNodeOffset;
    UInt                sFlag = 0;

    qmsAggNode        * sAggrNode;
    qmsConcatElement  * sGroupColumn;
    qtcNode           * sNode;
    mtcNode           * sArg;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initGRBY::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncGRBY ),
                                               (void **)& sGRBY )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sGRBY ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_GRBY ,
                        qmnGRBY ,
                        qmndGRBY,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sGRBY;

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    for ( sGroupColumn  = aGroupColumn;
          sGroupColumn != NULL;
          sGroupColumn  = sGroupColumn->next )
    {
        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sGRBY->plan.resultDesc,
                                        sGroupColumn->arithmeticOrList,
                                        sFlag,
                                        QMC_APPEND_EXPRESSION_TRUE | QMC_APPEND_ALLOW_CONST_TRUE,
                                        ID_FALSE )
                  != IDE_SUCCESS );
    }

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while ( sNode->node.module == &qtc::passModule )
        {
            sNode = (qtcNode *)sNode->node.arguments;
        }

        // BUGBUG
        // Aggregate function�� �ƴ� node�� ���޵Ǵ� ��찡 �����Ѵ�.
        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if ( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
             ( sNode->overClause == NULL ) )
        {
            // Argument���� �߰��Ѵ�.
            for ( sArg  = sNode->node.arguments;
                  sArg != NULL;
                  sArg  = sArg->next )
            {
                // BUG-39975 group_sort improve performence
                // AGGR ��忡�� AGGR �Լ� ���ڿ� PASS ��带 ������
                // ���� PASS ��带 skip �ؾ� �Ѵ�.
                while ( sArg->module == &qtc::passModule )
                {
                    sArg = sArg->arguments;
                }

                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sGRBY->plan.resultDesc,
                                                (qtcNode *)sArg,
                                                0,
                                                0,
                                                ID_TRUE ) // BUG-46249
                          != IDE_SUCCESS );
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

// Sort-based distinct�� GRBY
IDE_RC
qmoOneNonPlan::initGRBY( qcStatement       * aStatement ,
                         qmsQuerySet       * aQuerySet ,
                         qmnPlan           * aParent,
                         qmnPlan          ** aPlan )
{
    qmncGRBY          * sGRBY;
    UInt                sDataNodeOffset;
    UInt                sFlag   = 0;
    UInt                sOption = 0;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initGRBY::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncGRBY�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncGRBY ),
                                               (void **)& sGRBY )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sGRBY ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_GRBY ,
                        qmnGRBY ,
                        qmndGRBY,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sGRBY;

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

    sOption &= ~QMC_APPEND_EXPRESSION_MASK;
    sOption |= QMC_APPEND_EXPRESSION_TRUE;

    sOption &= ~QMC_APPEND_ALLOW_CONST_MASK;
    sOption |= QMC_APPEND_ALLOW_CONST_TRUE;

    sOption &= ~QMC_APPEND_ALLOW_DUP_MASK;
    sOption |= QMC_APPEND_ALLOW_DUP_FALSE;

    for ( sItrAttr  = aParent->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        // Distinct ����� �ƴ� ��� �����Ѵ�.
        // Ex) order by���� expression
        if ( ( sItrAttr->flag & QMC_ATTR_DISTINCT_MASK )
             == QMC_ATTR_DISTINCT_TRUE )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sGRBY->plan.resultDesc,
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

    IDE_TEST( qmc::makeReferenceResult( aStatement,
                                        ( aParent->type == QMN_PROJ ? ID_TRUE : ID_FALSE ),
                                        aParent->resultDesc,
                                        sGRBY->plan.resultDesc )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeGRBY( qcStatement      * aStatement ,
                         qmsQuerySet      * aQuerySet ,
                         UInt               aFlag ,
                         qmnPlan          * aChildPlan ,
                         qmnPlan          * aPlan )
{
    qmncGRBY          * sGRBY = (qmncGRBY *)aPlan;
    UInt                sDataNodeOffset;

    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount  = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sFirstMtrNode = NULL;
    qmcMtrNode        * sLastMtrNode  = NULL;

    UInt                sFlag;

    mtcTemplate       * sMtcTemplate;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeGRBY::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndGRBY));

    sGRBY->plan.left        = aChildPlan;
    sGRBY->mtrNodeOffset    = sDataNodeOffset;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sGRBY->flag             = QMN_PLAN_FLAG_CLEAR;
    sGRBY->plan.flag        = QMN_PLAN_FLAG_CLEAR;

    sGRBY->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

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
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    // To Fix PR-8493
    // GROUP BY �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
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

    // To Fix PR-7940
    sGRBY->myNode = NULL;

    sFlag = 0;
    sFlag &= ~QMC_MTR_GROUPING_MASK;
    sFlag |= QMC_MTR_GROUPING_TRUE;

    sGRBY->baseTableCount = 0;

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        // Grouping key�� ���
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK )
             == QMC_ATTR_KEY_TRUE )
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
            // GRBY,AGGR,WNST�� �����Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if ( sFirstMtrNode == NULL )
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

    sGRBY->myNode = sFirstMtrNode;

    switch ( aFlag & QMO_MAKEGRBY_SORT_BASED_METHOD_MASK )
    {
        case QMO_MAKEGRBY_SORT_BASED_GROUPING :
            sGRBY->flag &= ~QMNC_GRBY_METHOD_MASK;
            sGRBY->flag |= QMNC_GRBY_METHOD_GROUPING;
            break;
        default:
            sGRBY->flag &= ~QMNC_GRBY_METHOD_MASK;
            sGRBY->flag |= QMNC_GRBY_METHOD_DISTINCTION;
    }

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    IDE_TEST( qtc::allocIntermediateTuple( aStatement ,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
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
                                         sGRBY->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sGRBY->myNode )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sGRBY->myNode )
              != IDE_SUCCESS );

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    for ( sNewMtrNode  = sGRBY->myNode , sColumnCount = 0 ;
          sNewMtrNode != NULL;
          sNewMtrNode  = sNewMtrNode->next , sColumnCount++ ) ;

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    sMtrNode[0]  = sGRBY->myNode;
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sGRBY->plan ,
                                            QMO_GRBY_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sGRBY->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sGRBY->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sGRBY->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoOneNonPlan::initAGGR( qcStatement       * aStatement ,
                         qmsQuerySet       * aQuerySet ,
                         qmsAggNode        * aAggrNode ,
                         qmsConcatElement  * aGroupColumn,
                         qmnPlan           * aParent,
                         qmnPlan          ** aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncAGGR�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - Tuple Set�� ���
 *         - AGGR �÷��� ���� & passNode�� ó��
 *             - myNode�� ����
 *               (aggregation�� ���� �÷� + grouping�� ���� �÷�)
 *             - distNode�� ����
 *               (myNode�� distinct�� ���� �÷�,
 *                qmvQTC::isEquivalentExpression()���� �ߺ��� expression ����)
 *              - Tuple Set�� �÷� ������ �µ��� �Ҵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *    - Aggregation ���� DISTINCT column�� ���ؼ��� ������ Temp Table��
 *      �����ǹǷ� ���� Tuple�Ҵ��� �ʿ��ϸ�, ���� expresion�� �ִ���
 *      �˻��غ��ƾ� �Ѵ�. ����, grouping �׷����� ���� distinct column��
 *      BucketCnt�� �޾� �鿩�� �Ѵ�. (�Է����ڰ� ���� ����)
 *      �̸� qmcMtrNode.bucketCnt�� �����Ѵ�.
 *
 *     -Grouping�� �÷� ������ group by ������
 *      ���� �̿��� �ǹǷ� �̴� order by �� having���� ����� �ɼ� ����
 *      �Ƿ�, srcNode�� �����Ͽ� �÷��� �����ϰ�, group by�� dstNode�Ǵ�
 *      passNode�� ��ü �Ѵ�.
 *
 ***********************************************************************/

    qmncAGGR          * sAGGR;
    UInt                sDataNodeOffset;
    UInt                sFlag = 0;

    qmsAggNode        * sAggrNode;
    qmsConcatElement  * sGroupColumn;
    qtcNode           * sNode;
    qmcAttrDesc       * sResultDesc;
    qmcAttrDesc       * sFind;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initAGGR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aAggrNode  != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncHSDS�� �Ҵ� �� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncAGGR ),
                                               (void **)& sAGGR )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sAGGR ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_AGGR ,
                        qmnAGGR ,
                        qmndAGGR,
                        sDataNodeOffset );

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    sFlag &= ~QMC_ATTR_KEY_MASK;
    sFlag |= QMC_ATTR_KEY_TRUE;

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
        for ( sResultDesc  = aParent->resultDesc;
              sResultDesc != NULL;
              sResultDesc  = sResultDesc->next )
        {
            // nested aggr x, over x
            if ( ( sResultDesc->expr->overClause == NULL ) &&
                 ( QTC_IS_AGGREGATE( sResultDesc->expr ) == ID_TRUE ) &&
                 ( QTC_HAVE_AGGREGATE2( sResultDesc->expr ) == ID_FALSE ) )
            {
                IDE_TEST( qmc::appendAttribute( aStatement,
                                                aQuerySet,
                                                & sAGGR->plan.resultDesc,
                                                sResultDesc->expr,
                                                sFlag,
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

    for ( sGroupColumn  = aGroupColumn;
          sGroupColumn != NULL;
          sGroupColumn  = sGroupColumn->next )
    {
        // �������� grouping�� ����� �����Ƿ� expression�̶� �ϴ���
        // function mask�� ������ �ʰ� ����� �����ϵ��� �Ѵ�.

        // BUG-43542 group by �÷��� ����϶��� result desc �� �־�� �Ѵ�.
        IDE_TEST( qmc::appendAttribute( aStatement,
                                        aQuerySet,
                                        & sAGGR->plan.resultDesc,
                                        sGroupColumn->arithmeticOrList,
                                        0,
                                        QMC_APPEND_EXPRESSION_TRUE |
                                        QMC_APPEND_ALLOW_CONST_TRUE,
                                        ID_FALSE )
                     != IDE_SUCCESS );
    }

    for ( sAggrNode  = aAggrNode;
          sAggrNode != NULL;
          sAggrNode  = sAggrNode->next )
    {
        sNode = sAggrNode->aggr;
        while ( sNode->node.module == &qtc::passModule )
        {
            sNode = (qtcNode *)sNode->node.arguments;
        }

        // BUGBUG
        // Aggregate function�� �ƴ� node�� ���޵Ǵ� ��찡 �����Ѵ�.
        /* BUG-35193  Window function �� �ƴ� aggregation �� ó���ؾ� �Ѵ�. */
        if ( ( QTC_IS_AGGREGATE( sNode ) == ID_TRUE ) &&
             ( sNode->overClause == NULL ) )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sAGGR->plan.resultDesc,
                                            sNode,
                                            sFlag,
                                            QMC_APPEND_EXPRESSION_TRUE,
                                            ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }

    // BUG-39975 group_sort improve performence
    // BUG-39857 �� ���������� group_sort �� ������ �ټ� ����߸��� �Ѵ�.
    // AGGR �Լ� ���ڿ� PASS ��带 �����Ͽ� ������ ����Ų��.
    for ( sResultDesc  = sAGGR->plan.resultDesc;
          sResultDesc != NULL;
          sResultDesc  = sResultDesc->next )
    {
        // count(*) �� ��� arguments �� null �̴�.
        if ( (QTC_IS_AGGREGATE( sResultDesc->expr ) == ID_TRUE) &&
             (sResultDesc->expr->node.arguments != NULL) )
        {
            IDE_TEST( qmc::findAttribute( aStatement,
                                          sAGGR->plan.resultDesc,
                                          (qtcNode*)sResultDesc->expr->node.arguments,
                                          ID_TRUE,
                                          & sFind )
                      != IDE_SUCCESS );

            if ( sFind != NULL )
            {
                IDE_TEST( qmc::makeReference( aStatement,
                                              ID_TRUE,
                                              sFind->expr,
                                              (qtcNode**)&( sResultDesc->expr->node.arguments ) )
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

    *aPlan = (qmnPlan *)sAGGR;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeAGGR( qcStatement      * aStatement ,
                         qmsQuerySet      * aQuerySet ,
                         UInt               aFlag ,
                         qmoDistAggArg    * aDistAggArg,
                         qmnPlan          * aChildPlan ,
                         qmnPlan          * aPlan )
{
    qmncAGGR          * sAGGR          = (qmncAGGR *)aPlan;
    UInt                sDataNodeOffset;

    qmcMtrNode        * sMtrNode[2];

    UShort              sTupleID;
    UShort              sColumnCount = 0;

    UShort              sMtrNodeCount;
    UShort              sAggrNodeCount = 0;
    UShort              sDistNodeCount = 0;

    qmcMtrNode        * sNewMtrNode;
    qmcMtrNode        * sFirstMtrNode = NULL;
    qmcMtrNode        * sLastMtrNode  = NULL;
    mtcTemplate       * sMtcTemplate;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeAGGR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndAGGR));

    sAGGR->plan.left = aChildPlan;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------
    sAGGR->flag      = QMN_PLAN_FLAG_CLEAR;
    sAGGR->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sAGGR->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

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
                              MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    // To Fix PR-8493
    // GROUP BY �÷��� ��ü ���θ� �����ϱ� ���ؼ���
    // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
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

    //GRAPH���� ������ �����ü�� ����Ѵ�.
    if ( ( aFlag & QMO_MAKEAGGR_TEMP_TABLE_MASK )
         == QMO_MAKEAGGR_MEMORY_TEMP_TABLE )
    {
        sAGGR->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sAGGR->plan.flag  |= QMN_PLAN_STORAGE_MEMORY;
    }
    else
    {
        sAGGR->plan.flag  &= ~QMN_PLAN_STORAGE_MASK;
        sAGGR->plan.flag  |= QMN_PLAN_STORAGE_DISK;
    }

    sAGGR->myNode = NULL;
    sAGGR->distNode = NULL;
    sAGGR->baseTableCount = 0;

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        // Aggregate function�� ���
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
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
            // GRBY,AGGR,WNST�� �����Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            // BUG-8076
            if ( ( sItrAttr->expr->node.lflag & MTC_NODE_DISTINCT_MASK )
                 == MTC_NODE_DISTINCT_FALSE )
            {
                //----------------------------------
                // Distinction�� �������� ���� ���
                //----------------------------------
                sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_FALSE;
                sNewMtrNode->myDist = NULL;
            }
            else
            {
                //----------------------------------
                // Distinction�� ���� �ϴ� ���
                //----------------------------------

                sNewMtrNode->flag &= ~QMC_MTR_DIST_AGGR_MASK;
                sNewMtrNode->flag |= QMC_MTR_DIST_AGGR_TRUE;

                //----------------------------------
                // To Fix BUG-7604
                // ������ Distinct Argument ���� ���� �˻�
                //    - ������ Distinct Argument�� ���� ���,
                //      �̸� �����Ͽ� ó���ϱ� ����
                //----------------------------------
                IDE_TEST( qmg::makeDistNode( aStatement,
                                             aQuerySet,
                                             sAGGR->plan.flag,
                                             aChildPlan,
                                             sTupleID,
                                             aDistAggArg,
                                             sItrAttr->expr,
                                             sNewMtrNode,
                                             & sAGGR->distNode,
                                             & sDistNodeCount )
                          != IDE_SUCCESS );
            }

            if ( sFirstMtrNode == NULL )
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

    sAGGR->flag &= ~QMNC_AGGR_GROUPED_MASK;
    sAGGR->flag |= QMNC_AGGR_GROUPED_FALSE;

    for ( sItrAttr  = aPlan->resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        // Grouping key�� ���
        if ( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_FALSE )
        {
            sAGGR->flag &= ~QMNC_AGGR_GROUPED_MASK;
            sAGGR->flag |= QMNC_AGGR_GROUPED_TRUE;

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
            // GRBY,AGGR,WNST�� �����Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE;

            if ( sFirstMtrNode == NULL )
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

    sAGGR->myNode = sFirstMtrNode;

    IDE_TEST( qtc::allocIntermediateTuple( aStatement ,
                                           & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                           sTupleID ,
                                           sColumnCount )
              != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;

    //myNode�� ���� Tuple�� �׻� �޸��̴�.
    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    if ( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        if ( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
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

    //�÷� ������ execute������ ����
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sAGGR->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-1473 column locate ����.
    //----------------------------------

    IDE_TEST( qmg::setColumnLocate( aStatement,
                                    sAGGR->myNode )
              != IDE_SUCCESS );

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    // ���� Column�� data ���� ����
    sAGGR->mtrNodeOffset  = sDataNodeOffset;

    for ( sNewMtrNode  = sAGGR->myNode , sMtrNodeCount = 0 ;
          sNewMtrNode != NULL ;
          sNewMtrNode  = sNewMtrNode->next , sMtrNodeCount++ ) ;

    // aggregation column�� data ���� ����
    sDataNodeOffset += sMtrNodeCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );
    sAGGR->aggrNodeOffset = sDataNodeOffset;

    // distinct column�� data���� ����
    sDataNodeOffset += sAggrNodeCount * idlOS::align8( ID_SIZEOF(qmdAggrNode) );
    sAGGR->distNodeOffset = sDataNodeOffset;

    // data ������ ũ�� ����
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sDistNodeCount * idlOS::align8( ID_SIZEOF(qmdDistNode) );

    //----------------------------------
    //dependency ó�� �� subquery ó��
    //----------------------------------

    sMtrNode[0]  = sAGGR->myNode;
    sMtrNode[1]  = sAGGR->distNode;
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sAGGR->plan ,
                                            QMO_AGGR_DEPENDENCY,
                                            sTupleID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            2 ,
                                            sMtrNode )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sAGGR->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sAGGR->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sAGGR->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initCUNT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : CUNT ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - data ������ ũ�� ���
 *         - qmncCUNT�� �Ҵ� �� �ʱ�ȭ (display ���� , index ����)
 *     + ���� �۾�
 *         - countNode�� ����
 *         - Predicate�� �з�
 *     + ������ �۾�
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *     - CUNT , HIER , SCAN�� �۾��� ��� �����ϹǷ� �̸� �ٸ� interface��
 *       ���� �ϵ��� �Ѵ�.
 *
 ***********************************************************************/

    qmncCUNT          * sCUNT;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initCUNT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement       != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    //qmncCUNT�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCUNT ),
                                               (void **)& sCUNT )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sCUNT ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_CUNT ,
                        qmnCUNT ,
                        qmndCUNT,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sCUNT;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sCUNT->plan.resultDesc )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeCUNT( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmsFrom      * aFrom ,
                         qmsAggNode   * aCountNode,
                         qmoLeafInfo  * aLeafInfo ,
                         qmnPlan      * aPlan )
{
    qmncCUNT          * sCUNT = (qmncCUNT *)aPlan;
    UInt                sDataNodeOffset;
    qmsParseTree      * sParseTree;
    qtcNode           * sPredicate[10];
    idBool              sInSubQueryKeyRange = ID_FALSE;
    qcmIndex          * sIndices;
    UInt                i;
    UInt                sIndexCnt;
    qmoPredicate      * sCopiedPred;
    qmsPartitionRef   * sPartitionRef       = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeCUNT::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aFrom            != NULL );
    IDE_DASSERT( aQuerySet        != NULL );
    IDE_DASSERT( aCountNode       != NULL );
    IDE_DASSERT( aCountNode->next == NULL );
    IDE_DASSERT( aLeafInfo != NULL );
    IDE_DASSERT( aLeafInfo->levelPredicate == NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sIndices = aFrom->tableRef->tableInfo->indices;
    sIndexCnt = aFrom->tableRef->tableInfo->indexCount;

    sParseTree      = (qmsParseTree *)aStatement->myPlan->parseTree;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndCUNT));

    // BUG-15816
    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    sCUNT->tupleRowID       = aFrom->tableRef->table;
    // BUG-17483 ��Ƽ�� ���̺� count(*) ����
    // tableHandel ��ſ� tableRef �� �����Ѵ�.
    sCUNT->tableRef         = aFrom->tableRef;
    sCUNT->tableSCN         = aFrom->tableRef->tableSCN;

    //----------------------------------
    // �ε��� ���� �� ���� ����
    //----------------------------------

    sCUNT->method.index        = aLeafInfo->index;
    
    //----------------------------------
    // Cursor Property�� ����
    //----------------------------------

    sCUNT->lockMode                           = SMI_LOCK_READ;
    
    SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &(sCUNT->cursorProperty), NULL );

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sCUNT->flag = QMN_PLAN_FLAG_CLEAR;
    sCUNT->plan.flag = QMN_PLAN_FLAG_CLEAR;

    if ( aLeafInfo->index != NULL )
    {
        // To fix BUG-12742
        // index scan�� �����Ǿ� �ִ� ��츦 �����Ѵ�.
        if ( aLeafInfo->forceIndexScan == ID_TRUE )
        {
            sCUNT->flag &= ~QMNC_CUNT_FORCE_INDEX_SCAN_MASK;
            sCUNT->flag |= QMNC_CUNT_FORCE_INDEX_SCAN_TRUE;
        }
        else
        {
            sCUNT->flag &= ~QMNC_CUNT_FORCE_INDEX_SCAN_MASK;
            sCUNT->flag |= QMNC_CUNT_FORCE_INDEX_SCAN_FALSE;
        }
    }
    else
    {
        // Nothing to do.
    }

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement ,
                                     sCUNT->tupleRowID ,
                                     &( sCUNT->plan.flag ) )
              != IDE_SUCCESS );

    //------------------------------------------------------------
    // ���� �۾�
    //------------------------------------------------------------

    //----------------------------------
    // COUNT(*) ����ȭ ��� ����
    //----------------------------------

    // bug-8337
    // A3�� ������ ������ �ʿ��Ͽ� ���� ������ CURSOR�� ���� ó��
    // sCUNT->flag        &= ~QMNC_CUNT_METHOD_MASK;
    // sCUNT->flag        |= QMNC_CUNT_METHOD_CURSOR;

    // To Fix PR-13329
    // COUNT(*) ����ȭ �� Handle�� ����� ������ Cursor�� ����� �������� ����
    // CURSOR �� ����ϴ� ���
    //    - ������ ����, SET�� ����, Fixed Table�� ���
    if ( ( aLeafInfo->constantPredicate  != NULL ) ||
         ( aLeafInfo->predicate          != NULL ) ||
         ( aLeafInfo->ridPredicate       != NULL ) ||
         ( sParseTree->querySet->setOp   != QMS_NONE ) )
    {
        sCUNT->flag &= ~QMNC_CUNT_METHOD_MASK;
        sCUNT->flag |= QMNC_CUNT_METHOD_CURSOR;
    }
    else
    {
        sCUNT->flag &= ~QMNC_CUNT_METHOD_MASK;
        sCUNT->flag |= QMNC_CUNT_METHOD_HANDLE;
    }

    // fix BUG-12167
    // �����ϴ� ���̺��� fixed or performance view������ ������ ����
    // �����ϴ� ���̺� ���� IS LOCK�� ������ ���� �Ǵ� ����.
    if( ( aFrom->tableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
        ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
        ( aFrom->tableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) )
    {
        sCUNT->flag &= ~QMNC_CUNT_TABLE_FV_MASK;
        sCUNT->flag |= QMNC_CUNT_TABLE_FV_TRUE;

        // To Fix PR-13329
        // Fixed Table�� ��� Handle�κ��� �� ������ ������ �ȵ�.
        sCUNT->flag &= ~QMNC_CUNT_METHOD_MASK;
        sCUNT->flag |= QMNC_CUNT_METHOD_CURSOR;

        /* BUG-42639 Monitoring query */
        if ( QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) == 1 )
        {
            sCUNT->flag &= ~QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK;
            sCUNT->flag |= QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE;
        }
        else
        {
            /* BUG-43006 FixedTable Indexing Filter
             * optimizer formance view propery �� 0�̶��
             * FixedTable �� index�� ���ٰ� ����������Ѵ�
             */
            sIndices  = NULL;
            sIndexCnt = 0;
        }
    }
    else
    {
        sCUNT->flag &= ~QMNC_CUNT_TABLE_FV_MASK;
        sCUNT->flag |= QMNC_CUNT_TABLE_FV_FALSE;
    }

    if ( qcuTemporaryObj::isTemporaryTable( aFrom->tableRef->tableInfo ) == ID_TRUE )
    {
        sCUNT->flag &= ~QMNC_SCAN_TEMPORARY_TABLE_MASK;
        sCUNT->flag |= QMNC_SCAN_TEMPORARY_TABLE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    // BUG-16651
    if ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE )
    {
        IDE_TEST_RAISE( aFrom->tableRef->mDumpObjList == NULL,
                        ERR_EMPTY_DUMP_OBJECT );
        sCUNT->dumpObject = aFrom->tableRef->mDumpObjList->mObjInfo;
    }
    else
    {
        sCUNT->dumpObject = NULL;
    }

    //count(*)��� ����
    //PR-8784 , �����ؼ� ����ϵ��� �Ѵ�.
    IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                            qtcNode ,
                            &( sCUNT->countNode ) )
              != IDE_SUCCESS);
    idlOS::memcpy( sCUNT->countNode ,
                   aCountNode->aggr ,
                   ID_SIZEOF(qtcNode) );

    if ( sCUNT->tableRef->partitionRef == NULL )
    {
        /* PROJ-2462 Result Cache */
        qmo::addTupleID2ResultCacheStack( aStatement,
                                          sCUNT->tupleRowID );
    }
    else
    {
        for ( sPartitionRef  = sCUNT->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next )
        {
            /* PROJ-2462 Result Cache */
            qmo::addTupleID2ResultCacheStack( aStatement,
                                              sPartitionRef->table );
        }
    }

    //----------------------------------
    // display ���� ����
    //----------------------------------

    IDE_TEST( qmg::setDisplayInfo( aFrom ,
                                   &( sCUNT->tableOwnerName ) ,
                                   &( sCUNT->tableName ) ,
                                   &( sCUNT->aliasName ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Predicate�� ó��
    //----------------------------------

    sCUNT->method.subqueryFilter = NULL;
    IDE_TEST( processPredicate( aStatement,
                                aQuerySet,
                                aLeafInfo->predicate,
                                aLeafInfo->constantPredicate,
                                aLeafInfo->ridPredicate,
                                aLeafInfo->index,
                                sCUNT->tupleRowID,
                                &( sCUNT->method.constantFilter ),
                                &( sCUNT->method.filter ),
                                &( sCUNT->method.lobFilter ),
                                NULL,
                                &( sCUNT->method.varKeyRange ),
                                &( sCUNT->method.varKeyFilter ),
                                &( sCUNT->method.varKeyRange4Filter ),
                                &( sCUNT->method.varKeyFilter4Filter ),
                                &( sCUNT->method.fixKeyRange ),
                                &( sCUNT->method.fixKeyFilter ),
                                &( sCUNT->method.fixKeyRange4Print ),
                                &( sCUNT->method.fixKeyFilter4Print ),
                                &( sCUNT->method.ridRange ),
                                & sInSubQueryKeyRange )
              != IDE_SUCCESS );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    sCUNT->sdf = aLeafInfo->sdf;

    if ( aLeafInfo->sdf != NULL )
    {
        IDE_DASSERT( sIndexCnt > 0 );

        // sdf�� baskPlan�� �ܴ�.
        aLeafInfo->sdf->basePlan = &sCUNT->plan;

        // sdf�� index ������ŭ �ĺ��� �����Ѵ�.
        // ������ �ĺ��� filter/key range/key filter ������ �����Ѵ�.

        aLeafInfo->sdf->candidateCount = sIndexCnt;

        IDU_FIT_POINT( "qmoOneNonPlan::makeCUNT::alloc::candidate" );
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncScanMethod ) * sIndexCnt,
                                                   (void**)& aLeafInfo->sdf->candidate )
                  != IDE_SUCCESS );

        for ( i = 0;
              i < sIndexCnt;
              i++ )
        {
            // selected index�� ���ؼ��� �տ��� sSCAN�� ���� �۾��� �����Ƿ�,
            // �ٽ� �۾��� �ʿ䰡 ����.
            // ��� ���ڸ��� full scan�� ���ؼ� �۾��� �Ѵ�.
            if ( &sIndices[i] != aLeafInfo->index )
            {
                aLeafInfo->sdf->candidate[i].index = &sIndices[i];
            }
            else
            {
                aLeafInfo->sdf->candidate[i].index = NULL;
            }

            IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM( aStatement ),
                                                  aLeafInfo->sdf->predicate,
                                                  & sCopiedPred )
                      != IDE_SUCCESS );

            IDE_TEST( processPredicate( aStatement,
                                        aQuerySet,
                                        sCopiedPred,
                                        NULL, //aLeafInfo->constantPredicate,
                                        NULL, // rid predicate
                                        aLeafInfo->sdf->candidate[i].index,
                                        sCUNT->tupleRowID,
                                        &( aLeafInfo->sdf->candidate[i].constantFilter ),
                                        &( aLeafInfo->sdf->candidate[i].filter ),
                                        &( aLeafInfo->sdf->candidate[i].lobFilter ),
                                        &( aLeafInfo->sdf->candidate[i].subqueryFilter ),
                                        &( aLeafInfo->sdf->candidate[i].varKeyRange ),
                                        &( aLeafInfo->sdf->candidate[i].varKeyFilter ),
                                        &( aLeafInfo->sdf->candidate[i].varKeyRange4Filter ),
                                        &( aLeafInfo->sdf->candidate[i].varKeyFilter4Filter ),
                                        &( aLeafInfo->sdf->candidate[i].fixKeyRange ),
                                        &( aLeafInfo->sdf->candidate[i].fixKeyFilter ),
                                        &( aLeafInfo->sdf->candidate[i].fixKeyRange4Print ),
                                        &( aLeafInfo->sdf->candidate[i].fixKeyFilter4Print ),
                                        &( aLeafInfo->sdf->candidate[i].ridRange ),
                                        & sInSubQueryKeyRange )
                      != IDE_SUCCESS );

            aLeafInfo->sdf->candidate[i].constantFilter =
                sCUNT->method.constantFilter;

            // fix BUG-19074
            IDE_TEST( postProcessCuntMethod( aStatement,
                                             & aLeafInfo->sdf->candidate[i] )
                      != IDE_SUCCESS );

            // fix BUG-19074
            //----------------------------------
            // sdf�� dependency ó��
            //----------------------------------

            sPredicate[0] = aLeafInfo->sdf->candidate[i].fixKeyRange;
            sPredicate[1] = aLeafInfo->sdf->candidate[i].varKeyRange;
            sPredicate[2] = aLeafInfo->sdf->candidate[i].varKeyRange4Filter;
            sPredicate[3] = aLeafInfo->sdf->candidate[i].fixKeyFilter;
            sPredicate[4] = aLeafInfo->sdf->candidate[i].varKeyFilter;
            sPredicate[5] = aLeafInfo->sdf->candidate[i].varKeyFilter4Filter;
            sPredicate[6] = aLeafInfo->sdf->candidate[i].constantFilter;
            sPredicate[7] = aLeafInfo->sdf->candidate[i].filter;

            //----------------------------------
            // PROJ-1473
            // dependency �������� predicate���� ��ġ���� ����.
            //----------------------------------
            IDE_TEST( qmoDependency::setDependency( aStatement,
                                                    aQuerySet,
                                                    & sCUNT->plan,
                                                    QMO_CUNT_DEPENDENCY,
                                                    sCUNT->tupleRowID,
                                                    NULL,
                                                    8,
                                                    sPredicate,
                                                    0,
                                                    NULL )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do...
    }

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] = sCUNT->method.fixKeyRange;
    sPredicate[1] = sCUNT->method.varKeyRange;
    sPredicate[2] = sCUNT->method.varKeyRange4Filter;
    sPredicate[3] = sCUNT->method.fixKeyFilter;
    sPredicate[4] = sCUNT->method.varKeyFilter;
    sPredicate[5] = sCUNT->method.varKeyFilter4Filter;
    sPredicate[6] = sCUNT->method.constantFilter;
    sPredicate[7] = sCUNT->method.filter;
    sPredicate[8] = sCUNT->method.ridRange;

    //----------------------------------
    // PROJ-1473
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    for ( i  = 0;
          i <= 8;
          i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sCUNT->plan,
                                            QMO_CUNT_DEPENDENCY,
                                            sCUNT->tupleRowID,
                                            NULL,
                                            9,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    //----------------------------------
    // Host ������ ������
    // Constant Expression�� ����ȭ
    //----------------------------------

    // fix BUG-19074
    IDE_TEST( postProcessCuntMethod( aStatement,
                                     & sCUNT->method )
              != IDE_SUCCESS );

    // Dependent Tuple Row ID ����
    sCUNT->depTupleRowID = (UShort)sCUNT->plan.dependency;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EMPTY_DUMP_OBJECT);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMO_EMPTY_DUMP_OBJECT));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::postProcessCuntMethod( qcStatement    * aStatement,
                                      qmncScanMethod * aMethod )
{
    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::postProcessCuntMethod::__FT__" );

    IDE_DASSERT( aMethod != NULL );

    if ( aMethod->filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // BUG-17506
    if ( aMethod->varKeyRange4Filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->varKeyRange4Filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // BUG-17506
    if ( aMethod->varKeyFilter4Filter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    aMethod->varKeyFilter4Filter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initVIEW( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : VIEW ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncVIEW�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - Store and Search �ΰ�� Tuple Set�� �Ҵ�
 *         - VIEW�� �÷��� ���� (myNode)
 *         - Display�� ���� ���� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 *
 * TO DO
 *     - myNode�� qtcNode���� �̹Ƿ� qtc::makeInternalColumn()���� �Ҵ�Ǵ�
 *       �˾Ƴ� Tuple ID�� ȣ�� �Ѵ�.
 *
 ***********************************************************************/

    qmncVIEW          * sVIEW;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initVIEW::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aQuerySet  != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncVIEW�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncVIEW ),
                                               (void **)& sVIEW )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sVIEW ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_VIEW ,
                        qmnVIEW ,
                        qmndVIEW,
                        sDataNodeOffset );

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   & sVIEW->plan.resultDesc )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sVIEW;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeVIEW( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmsFrom      * aFrom ,
                         UInt           aFlag ,
                         qmnPlan      * aChildPlan,
                         qmnPlan      * aPlan )
{
    qmncVIEW          * sVIEW = (qmncVIEW*)aPlan;
    UInt                sDataNodeOffset;

    UShort              sTupleID;
    UShort              sColumnCount;
    UInt                sViewaDependencyFlag    = 0;
    qtcNode           * sPrevNode = NULL;
    qtcNode           * sNewNode;
    qtcNode           * sConvertedNode;

    mtcTemplate       * sMtcTemplate;

    qmcAttrDesc       * sItrAttr;
    qmcAttrDesc       * sChildAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeVIEW::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aQuerySet  != NULL );
    IDE_FT_ASSERT( aChildPlan != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndVIEW));

    sVIEW->plan.left        = aChildPlan;

    //----------------------------------
    // Flag ���� ����
    //----------------------------------

    sVIEW->flag      = QMN_PLAN_FLAG_CLEAR;
    sVIEW->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sVIEW->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    // BUG-37507
    // ���� target �� ��� �ܺ� ���� �÷��� �ü� �ִ�. ���� ������ ���� ó���Ѵ�.
    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sVIEW->plan.resultDesc,
                                     & aChildPlan->depInfo,
                                     & aChildPlan->depInfo )
              != IDE_SUCCESS );

    //----------------------------------
    // Flag ���� ���� �� Tuple ID �Ҵ�
    //----------------------------------

    switch ( aFlag & QMO_MAKEVIEW_FROM_MASK )
    {
        case QMO_MAKEVIEW_FROM_PROJECTION:
            //----------------------------------
            // Projection���� ���� ������ ���
            // - �Ͻ��� VIEW�̹Ƿ� Tuple�� �Ҵ�
            //   �޴´�.
            //----------------------------------

            //dependency ������ ���� flag����
            sViewaDependencyFlag    = QMO_VIEW_IMPLICIT_DEPENDENCY;

            IDE_TEST( qtc::nextTable( & sTupleID,
                                      aStatement,
                                      NULL,
                                      ID_TRUE,
                                      MTC_COLUMN_NOTNULL_TRUE )
                      != IDE_SUCCESS );

            sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
            sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

            sColumnCount = 0;

            for( sItrAttr = aPlan->resultDesc;
                 sItrAttr != NULL;
                 sItrAttr = sItrAttr->next )
            {
                sColumnCount++;
            }

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
            sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;

            //----------------------------------
            // mtcTuple.lflag����
            //----------------------------------

            // VIEW�� Intermediate�̴�.
            sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_TYPE_MASK;
            sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_TYPE_INTERMEDIATE;

            // To Fix PR-7992
            // Implicite VIEW�� ���ؼ��� Depedendency ������ ����
            // VIEW���� �������� �ʴ´�.
            //   ����) qtcColumn.cpp�� qtcEstimate() �ּ� ����
            // ��� Column�� depedency ���� �Ŀ� VIEW���� �����Ѵ�.
            sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_VIEW_MASK;
            sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_VIEW_FALSE;

            //----------------------------------
            // mtcColumn������ ����
            // - target������ ���� �����Ѵ�.
            //----------------------------------

            for( sItrAttr = aPlan->resultDesc, sChildAttr = aChildPlan->resultDesc, sColumnCount = 0;
                 sItrAttr != NULL;
                 sItrAttr = sItrAttr->next, sChildAttr = sChildAttr->next, sColumnCount++ )
            {
                sConvertedNode =
                    (qtcNode*)mtf::convertedNode(
                        (mtcNode*)sItrAttr->expr,
                        &QC_SHARED_TMPLATE(aStatement)->tmplate );

                mtc::copyColumn( &(sMtcTemplate->rows[sTupleID].columns[sColumnCount]),
                                 &(QC_SHARED_TMPLATE(aStatement)->tmplate.rows
                                   [sConvertedNode->node.table].
                                   columns[sConvertedNode->node.column]) );

                // PROJ-2179
                // VIEW�� ������� ��� value node�� �ٲ��ش�.
                sItrAttr->expr->node.table      = sTupleID;
                sItrAttr->expr->node.column     = sColumnCount;
                sItrAttr->expr->node.module     = &qtc::valueModule;
                sItrAttr->expr->node.conversion = NULL;
                sItrAttr->expr->node.lflag      = qtc::valueModule.lflag;
                // PROJ-1718 Constant�� �ƴ� expression�� value module�� ��ȯ�� ���
                //           unparsing�� tree�� ��ȸ�ϵ��� �ϵ��� orgNode�� �������ش�.
                sItrAttr->expr->node.orgNode    = (mtcNode *)sChildAttr->expr;
            }

            break;

        case QMO_MAKEVIEW_FROM_SELECTION:
            //----------------------------------
            // Selection���� ���� ������ ���
            // - Created View�Ǵ� Inline View�� ���� ����� View�̴�.
            //----------------------------------

            //dependency ������ ���� flag����
            sViewaDependencyFlag    = QMO_VIEW_EXPLICIT_DEPENDENCY;

            //Ʃ�� �˾Ƴ���
            sTupleID         = aFrom->tableRef->table;

            break;

        case QMO_MAKEVIEW_FROM_SET:
            //----------------------------------
            // Set���� ���� ������ ���
            // - Set�� �ϳ��� ������ Table�� ó���ϱ� ���� �Ͻ��� View��
            // - �׷��� optimizer�� tuple�� �Ҵ��ѳ༮�� �ƴϹǷ� explicit
            //   �ϰ� ó�� �Ѵ�. (�̴� PROJ-1358�� ���Ͽ� ������ �����)
            //----------------------------------

            // To Fix PR-12791
            // SET ���κ��� �����Ǵ� VIEW ���� �Ͻ��� View ��.
            //dependency ������ ���� flag����
            sViewaDependencyFlag    = QMO_VIEW_IMPLICIT_DEPENDENCY;

            //Ʃ�� �˾Ƴ���
            sTupleID         = aQuerySet->target->targetColumn->node.table;
            break;
        default:
            IDE_FT_ASSERT( 0 );
    }

    //----------------------------------
    // mtcTuple.lflag����
    //----------------------------------
    // ���� ��ü�� ���� - VIEW�� �ϳ��� memory ������ ���
    sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_STORAGE_MEMORY;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;

    //----------------------------------
    // myNode�� ����
    //----------------------------------
    sVIEW->myNode = NULL;

    for ( sColumnCount = 0;
          sColumnCount < sMtcTemplate->rows[sTupleID].columnCount;
          sColumnCount++ )
    {
        IDE_TEST( qtc::makeInternalColumn( aStatement,
                                           sTupleID,
                                           sColumnCount,
                                           & sNewNode )
                  != IDE_SUCCESS);

        if ( sVIEW->myNode == NULL )
        {
            sVIEW->myNode  = sNewNode;
            sPrevNode      = sNewNode;
        }
        else
        {
            sPrevNode->node.next = (mtcNode *)sNewNode;
            sPrevNode            = sNewNode;
        }

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sNewNode )
                  != IDE_SUCCESS );
    }

    IDE_FT_ASSERT( sPrevNode != NULL );

    sPrevNode->node.next = NULL;

    // To Fix PR-7992
    // ��� Column�� �ùٸ� Depedency ���� �Ŀ� VIEW���� ������.
    sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_VIEW_MASK;
    sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_VIEW_TRUE;

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    if ( aFrom != NULL )
    {
        IDE_TEST( qmg::setDisplayInfo( aFrom ,
                                       &( sVIEW->viewOwnerName ) ,
                                       &( sVIEW->viewName ) ,
                                       &( sVIEW->aliasName ) )
                  != IDE_SUCCESS );
    }
    else
    {
        // To Fix PR-7992
        sVIEW->viewName.name = NULL;
        sVIEW->viewName.size = QC_POS_EMPTY_SIZE;

        sVIEW->aliasName.name = NULL;
        sVIEW->aliasName.size = QC_POS_EMPTY_SIZE;
    }

    //-------------------------------------------------------------
    // ������ �۾�
    //-------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sVIEW->plan,
                                            sViewaDependencyFlag,
                                            sTupleID,
                                            NULL,
                                            0,
                                            NULL,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sVIEW->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sVIEW->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sVIEW->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initVSCN( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmsFrom      * aFrom ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : VSCN ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncVSCN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - MTC_TUPLE_TYPE_TABLE ����
 *         - Display�� ���� ���� ����
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncVSCN          * sVSCN;
    UInt                sDataNodeOffset;
    UShort              sTupleID;
    qcDepInfo           sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initVSCN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncVSCN ),
                                               (void **)& sVSCN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sVSCN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_VSCN ,
                        qmnVSCN ,
                        qmndVSCN,
                        sDataNodeOffset );

    sTupleID = aFrom->tableRef->table;

    // PROJ-2179
    // Join �� table�� dependency�� ���Ե� �� �����Ƿ�
    // �� VSCN�� fetch�ϴ� table�� ���� dependency ������ ������ �����Ѵ�.

    qtc::dependencySet(sTupleID, &sDepInfo);

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sVSCN->plan.resultDesc )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     & sVSCN->plan.resultDesc,
                                     & sDepInfo,
                                     &( aQuerySet->depInfo ) )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sVSCN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeVSCN( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmsFrom      * aFrom,
                                qmnPlan      * aChildPlan,
                                qmnPlan      * aPlan )
{
    qmncVSCN          * sVSCN = (qmncVSCN *)aPlan;
    UInt                sDataNodeOffset;

    UShort              sChildTupleID;
    UShort              sTupleID;
    mtcTemplate       * sMtcTemplate;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeVSCN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aFrom      != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndVSCN));

    sVSCN->plan.left        = aChildPlan;

    //----------------------------------
    // �⺻ ���� ����
    //----------------------------------

    sVSCN->tupleRowID       = aFrom->tableRef->table;
    sTupleID = sVSCN->tupleRowID;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sVSCN->flag             = QMN_PLAN_FLAG_CLEAR;
    sVSCN->plan.flag        = QMN_PLAN_FLAG_CLEAR;

    // PROJ-2582 recursive with
    if ( ( aFrom->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
         != QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
    {
        IDE_DASSERT( aChildPlan != NULL );
        
        sVSCN->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);
    }
    else
    {
        // nothing to do
    }

    //-------------------------------------------------------------
    // ���� �۾�
    //-------------------------------------------------------------

    // Tuple.lflag�� ����
    sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_TYPE_MASK;
    sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_TYPE_TABLE;

    // Tuple.lflag�� ����
    sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_VSCN_MASK;
    sMtcTemplate->rows[sTupleID].lflag    |= MTC_TUPLE_VSCN_TRUE;

    // To Fix PR-8385
    // VSCN�� �����Ǵ� ��쿡�� in-line view�� �ϴ���
    // �Ϲ����̺�� ó���Ͽ��� �Ѵ�. ���� ������ view��� ���õȰ���
    // false�� �����Ѵ�.
    sMtcTemplate->rows[sTupleID].lflag    &= ~MTC_TUPLE_VIEW_MASK;
    sMtcTemplate->rows[sTupleID].lflag    |=  MTC_TUPLE_VIEW_FALSE;

    // PROJ-2582 recursive with
    if ( ( aFrom->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
         != QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
    {
        // ���� ��ü�� ���� - ������ VMTR�� ������ �̿�
        sChildTupleID = ((qmncVMTR*)aChildPlan)->myNode->dstNode->node.table;

        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |=
            (sMtcTemplate->rows[sChildTupleID].lflag & MTC_TUPLE_STORAGE_MASK);
    }
    else
    {
        // recursive with�� �޸𸮸� ����
        sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
        sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;
    }

    if ( ( aQuerySet->materializeType
           == QMO_MATERIALIZE_TYPE_VALUE )
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

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    IDE_TEST( qmg::setDisplayInfo( aFrom ,
                                   &( sVSCN->viewOwnerName ) ,
                                   &( sVSCN->viewName ) ,
                                   &( sVSCN->aliasName ) )
              != IDE_SUCCESS );

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sVSCN->plan ,
                                            QMO_VSCN_DEPENDENCY,
                                            sVSCN->tupleRowID ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ) != IDE_SUCCESS );

    // PROJ-2582 recursive with
    if ( ( aFrom->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
         != QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
    {
        /*
         * PROJ-1071 Parallel Query
         * parallel degree
         */
        sVSCN->plan.mParallelDegree = aChildPlan->mParallelDegree;
        sVSCN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
        sVSCN->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initCNTR( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmnPlan       * aParent ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : CNTR ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncCNTR�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - rownum�� ����
 *         - Stopkey Predicate�� ó��
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 ***********************************************************************/

    qmncCNTR          * sCNTR;
    UInt                sDataNodeOffset;
    qmcAttrDesc       * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initCNTR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncCNTR�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncCNTR ),
                                               (void **)& sCNTR )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sCNTR ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_CNTR ,
                        qmnCNTR ,
                        qmndCNTR,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sCNTR;

    /* BUG-39611 support SYS_CONNECT_BY_PATH's expression arguments
     * ���� Plan ���� ���� �Ǵ� result descript�� CONNECT_BY�� ���õ� FUNCTION��
     * ������ �����ϱ� ���� CNTR���� append�Ѵ�.
     */
    for ( sItrAttr = aParent->resultDesc;
          sItrAttr != NULL;
          sItrAttr = sItrAttr->next )
    {
        if ( ( sItrAttr->expr->node.lflag & MTC_NODE_FUNCTION_CONNECT_BY_MASK )
               == MTC_NODE_FUNCTION_CONNECT_BY_TRUE )
        {
            IDE_TEST( qmc::appendAttribute( aStatement,
                                            aQuerySet,
                                            & sCNTR->plan.resultDesc,
                                            sItrAttr->expr,
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
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sCNTR->plan.resultDesc )
              != IDE_SUCCESS );

    for ( sItrAttr  = sCNTR->plan.resultDesc;
          sItrAttr != NULL;
          sItrAttr  = sItrAttr->next )
    {
        if( ( sItrAttr->expr->lflag & QTC_NODE_ROWNUM_MASK )
                == QTC_NODE_ROWNUM_EXIST )
        {
            sItrAttr->flag &= ~QMC_ATTR_TERMINAL_MASK;
            sItrAttr->flag |= QMC_ATTR_TERMINAL_TRUE;
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
qmoOneNonPlan::makeCNTR( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmoPredicate * aStopkeyPredicate ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan )
{
    qmncCNTR          * sCNTR = (qmncCNTR *)aPlan;
    UInt                sDataNodeOffset;
    qtcNode           * sNode;
    qtcNode           * sPredicate[10];
    qtcNode           * sStopFilter;
    qtcNode           * sOptimizedStopFilter;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeCNTR::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    sCNTR->plan.left        = aChildPlan;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndCNTR));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sCNTR->flag = QMN_PLAN_FLAG_CLEAR;
    sCNTR->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sCNTR->plan.flag       |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    //----------------------------------
    // rownum�� tuple row ID ó��
    //----------------------------------

    sNode = aQuerySet->SFWGH->rownum;

    // BUG-17949
    // group by rownum�� ��� SFWGH->rownum�� passNode�� �޷��ִ�.
    if ( sNode->node.module == & qtc::passModule )
    {
        sNode = (qtcNode*) sNode->node.arguments;
    }
    else
    {
        // Nothing to do.
    }

    sCNTR->rownumRowID = sNode->node.table;

    //----------------------------------
    // stopFilter�� ó��
    //----------------------------------

    if ( aStopkeyPredicate != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aStopkeyPredicate,
                                          & sStopFilter )
                  != IDE_SUCCESS );

        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sStopFilter )
                  != IDE_SUCCESS );

        IDE_TEST( qmoNormalForm::optimizeForm( sStopFilter,
                                               & sOptimizedStopFilter )
                  != IDE_SUCCESS );

        sCNTR->stopFilter = sOptimizedStopFilter;
    }
    else
    {
        sCNTR->stopFilter = NULL;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // Host ������ ������
    // Constant Expression�� ����ȭ
    //----------------------------------

    if ( sCNTR->stopFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement ,
                                                    sCNTR->stopFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] = sCNTR->stopFilter;

    //----------------------------------
    // PROJ-1473
    // dependency �������� predicate���� ��ġ���� ����.
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sCNTR->plan,
                                            QMO_CNTR_DEPENDENCY,
                                            0,
                                            NULL,
                                            1,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sCNTR->plan.mParallelDegree = aChildPlan->mParallelDegree;
    sCNTR->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCNTR->plan.flag |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initINST( qcStatement   * aStatement ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : INST ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncINST�� �Ҵ� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmncINST          * sINST;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initINST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncINST ),
                                               (void **)& sINST )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sINST ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_INST ,
                        qmnINST ,
                        qmndINST,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sINST;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeINST( qcStatement   * aStatement ,
                         qmoINSTInfo   * aINSTInfo ,
                         qmnPlan       * aChildPlan ,
                         qmnPlan       * aPlan )
{
/***********************************************************************
 *
 * Description : INST ��带 �����Ѵ�.
 *
 * Implementation :
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncINST          * sINST = (qmncINST *)aPlan;
    UInt                sDataNodeOffset;
    qmsFrom             sFrom;
    qcmColumn         * sColumn;
    qmmValueNode      * sValue;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeINST::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndINST) );

    sINST->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sINST->flag = QMN_PLAN_FLAG_CLEAR;
    sINST->plan.flag = QMN_PLAN_FLAG_CLEAR;

    if ( aChildPlan != NULL )
    {
        sINST->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);
    }
    else
    {
        // Nothing to do.
    }

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement,
                                     aINSTInfo->tableRef->table,
                                     &( sINST->plan.flag ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    // tableRef�� ������ ��
    sFrom.tableRef = aINSTInfo->tableRef;

    IDE_TEST( qmg::setDisplayInfo( & sFrom,
                                   &( sINST->tableOwnerName ),
                                   &( sINST->tableName ),
                                   &( sINST->aliasName ) )
                 != IDE_SUCCESS );

    /* BUG-47625 Insert execution ������� */
    for ( sColumn = aINSTInfo->tableRef->tableInfo->columns;
          sColumn != NULL;
          sColumn = sColumn->next )
    {
        if ( ( sColumn->basicInfo->flag & MTC_COLUMN_NOTNULL_MASK )
             == MTC_COLUMN_NOTNULL_TRUE )
        {
            sINST->flag &= ~QMNC_INST_EXIST_NOTNULL_COLUMN_MASK;
            sINST->flag |= QMNC_INST_EXIST_NOTNULL_COLUMN_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sColumn->basicInfo->module->flag & MTD_ENCRYPT_TYPE_MASK)
               == MTD_ENCRYPT_TYPE_TRUE )
        {
            sINST->flag &= ~QMNC_INST_EXIST_ENCRYPT_COLUMN_MASK;
            sINST->flag |= QMNC_INST_EXIST_ENCRYPT_COLUMN_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    /* BUG-47625 Insert execution ������� */
    if ( aINSTInfo->rows->next == NULL )
    {
        for ( sValue = aINSTInfo->rows->values;
              sValue != NULL;
              sValue = sValue->next )
        {
            if ( sValue->timestamp == ID_TRUE )
            {
                sINST->flag &= ~QMNC_INST_EXIST_TIMESTAMP_MASK;
                sINST->flag |= QMNC_INST_EXIST_TIMESTAMP_TRUE;
            }
            else
            {
                /* Nothing to do */
            }

            if ( sValue->msgID == ID_TRUE )
            {
                sINST->flag &= ~QMNC_INST_EXIST_QUEUE_MASK;
                sINST->flag |= QMNC_INST_EXIST_QUEUE_TRUE;
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

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    //----------------------------------
    // insert ���� ����
    //----------------------------------

    // insert target ����
    sINST->tableRef = aINSTInfo->tableRef;

    // insert select ����
    if ( aChildPlan != NULL )
    {
        sINST->isInsertSelect = ID_TRUE;
    }
    else
    {
        sINST->isInsertSelect = ID_FALSE;
    }

    // multi-table insert ����
    sINST->isMultiInsertSelect = aINSTInfo->multiInsertSelect;

    // insert columns ����
    sINST->columns          = aINSTInfo->columns;
    sINST->columnsForValues = aINSTInfo->columnsForValues;

    // insert values ����
    sINST->rows           = aINSTInfo->rows;
    sINST->valueIdx       = aINSTInfo->valueIdx;
    sINST->canonizedTuple = aINSTInfo->canonizedTuple;
    sINST->compressedTuple= aINSTInfo->compressedTuple;
    sINST->queueMsgIDSeq  = aINSTInfo->queueMsgIDSeq;

    // insert hint ����
    sINST->hints = aINSTInfo->hints;

    /* PROJ-2464 hybrid partitioned table ����
     *  - Simple Query�� �����ϱ� ���ؼ�, Partition�� ������� �ʰ� �켱 �����Ѵ�.
     */
    if ( sINST->hints != NULL )
    {
        if ( ( ( sINST->hints->directPathInsHintFlag & SMI_INSERT_METHOD_MASK )
               == SMI_INSERT_METHOD_APPEND )
             &&
             ( ( sINST->plan.flag & QMN_PLAN_STORAGE_MASK )
               == QMN_PLAN_STORAGE_DISK ) )
        {
            //---------------------------------------------------
            // PROJ-1566
            // INSERT�� APPEND ������� ����Ǿ�� �ϴ� ���,
            // SIX Lock�� ȹ���ؾ� ��
            //---------------------------------------------------

            sINST->isAppend = ID_TRUE;
        }
        else
        {
            sINST->isAppend = ID_FALSE;
        }

        // PROJ-2673
        if ( aINSTInfo->insteadOfTrigger == ID_FALSE )
        {
            sINST->disableTrigger = sINST->hints->disableInsTrigger;
        }
        else
        {
            sINST->disableTrigger = ID_FALSE;
        }
    }
    else
    {
        sINST->isAppend = ID_FALSE;
        sINST->disableTrigger = ID_FALSE;
    }

    // sequence ����
    sINST->nextValSeqs = aINSTInfo->nextValSeqs;

    // instead of trigger ����
    sINST->insteadOfTrigger = aINSTInfo->insteadOfTrigger;

    // BUG-43063 insert nowait
    sINST->lockWaitMicroSec = aINSTInfo->lockWaitMicroSec;

    //----------------------------------
    // parent constraint ����
    //----------------------------------

    sINST->parentConstraints = aINSTInfo->parentConstraints;
    sINST->checkConstrList   = aINSTInfo->checkConstrList;

    //----------------------------------
    // return into ����
    //----------------------------------

    sINST->returnInto = aINSTInfo->returnInto;

    //----------------------------------
    // Default Expr ����
    //----------------------------------

    sINST->defaultExprTableRef = aINSTInfo->defaultExprTableRef;
    sINST->defaultExprColumns  = aINSTInfo->defaultExprColumns;

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    qtc::dependencyClear( & sINST->plan.depInfo );

    // PROJ-2551 simple query ����ȭ
    // simple insert�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleINST( aStatement, sINST )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initUPTE( qcStatement   * aStatement ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : UPTE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncUPTE�� �Ҵ� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmncUPTE          * sUPTE;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initUPTE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncUPTE ),
                                               (void **)& sUPTE )
        != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sUPTE ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_UPTE ,
                        qmnUPTE ,
                        qmndUPTE,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sUPTE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeUPTE( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmoUPTEInfo   * aUPTEInfo ,
                         qmnPlan       * aChildPlan ,
                         qmnPlan       * aPlan )
{
/***********************************************************************
 *
 * Description : UPTE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncUPTE          * sUPTE = (qmncUPTE *)aPlan;
    UInt                sDataNodeOffset;
    qcmTableInfo      * sTableInfo;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount = 0;
    qcmColumn         * sColumn;
    qmsFrom             sFrom;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeUPTE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    sTableInfo = aUPTEInfo->updateTableRef->tableInfo;
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndUPTE) );

    sUPTE->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sUPTE->flag = QMN_PLAN_FLAG_CLEAR;
    sUPTE->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sUPTE->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement,
                                     aUPTEInfo->updateTableRef->table,
                                     &( sUPTE->plan.flag ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    QCP_SET_INIT_QMS_FROM( (&sFrom) );
    sFrom.tableRef = aUPTEInfo->updateTableRef;

    IDE_TEST( qmg::setDisplayInfo( & sFrom,
                                   &( sUPTE->tableOwnerName ),
                                   &( sUPTE->tableName ),
                                   &( sUPTE->aliasName ) )
              != IDE_SUCCESS );

    /* PROJ-2204 Join Update, Delete
     * join update���� ǥ���Ѵ�. */
    if ( aQuerySet->SFWGH->from->tableRef->view != NULL )
    {
        sUPTE->flag &= ~QMNC_UPTE_VIEW_MASK;
        sUPTE->flag |= QMNC_UPTE_VIEW_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    // key preserved property
    if ( aQuerySet->SFWGH->preservedInfo != NULL )
    {
        if ( aQuerySet->SFWGH->preservedInfo->useKeyPreservedTable == ID_TRUE )
        {
            sUPTE->flag &= ~QMNC_UPTE_VIEW_KEY_PRESERVED_MASK;
            sUPTE->flag |= QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE;
        }
    }
    
    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    //----------------------------------
    // update ���� ����
    //----------------------------------

    // update target table ����
    sUPTE->tableRef = aUPTEInfo->updateTableRef;

    // update column ���� ����
    sUPTE->columns           = aUPTEInfo->columns;
    sUPTE->updateColumnList  = aUPTEInfo->updateColumnList;
    sUPTE->updateColumnCount = aUPTEInfo->updateColumnCount;
    sUPTE->mTableList        = NULL;
    sUPTE->mMultiTableCount  = 0;

    if ( aUPTEInfo->updateColumnCount > 0 )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( aUPTEInfo->updateColumnCount * ID_SIZEOF(UInt),
                                                   (void**) & sUPTE->updateColumnIDs )
                  != IDE_SUCCESS );

        // To Fix PR-10592
        // Update Column�� �������� ��� ��Ȯ�� ID�� �����ؾ� ��.
        for ( i = 0, sColumn = aUPTEInfo->columns;
              i < aUPTEInfo->updateColumnCount;
              i++, sColumn = sColumn->next )
        {
            sUPTE->updateColumnIDs[i] = sColumn->basicInfo->column.id;
        }
    }
    else
    {
        sUPTE->updateColumnIDs = NULL;
    }

    // update value ���� ����
    sUPTE->values         = aUPTEInfo->values;
    sUPTE->subqueries     = aUPTEInfo->subqueries;
    sUPTE->valueIdx       = aUPTEInfo->valueIdx;
    sUPTE->canonizedTuple = aUPTEInfo->canonizedTuple;
    sUPTE->compressedTuple= aUPTEInfo->compressedTuple;
    sUPTE->isNull         = aUPTEInfo->isNull;

    // sequence ����
    sUPTE->nextValSeqs    = aUPTEInfo->nextValSeqs;

    // instead of trigger ����
    sUPTE->insteadOfTrigger = aUPTEInfo->insteadOfTrigger;

    //----------------------------------
    // partition ���� ����
    //----------------------------------

    sUPTE->insertTableRef      = aUPTEInfo->insertTableRef;
    sUPTE->isRowMovementUpdate = aUPTEInfo->isRowMovementUpdate;

    // partition�� update column list ����
    if ( sUPTE->tableRef->partitionRef != NULL )
    {
        sPartitionCount = 0;
        for ( sPartitionRef  = sUPTE->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next )
        {
            sPartitionCount++;
        }

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                   (void **) &( sUPTE->updatePartColumnList ) )
                  != IDE_SUCCESS );

        for ( sPartitionRef  = sUPTE->tableRef->partitionRef, i = 0;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next, i++ )
        {
            IDE_TEST( qmoPartition::makePartUpdateColumnList( aStatement,
                                                              sPartitionRef,
                                                              sUPTE->updateColumnList,
                                                              &( sUPTE->updatePartColumnList[i] ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        sUPTE->updatePartColumnList = NULL;
    }

    // update type
    sUPTE->updateType = aUPTEInfo->updateType;

    //----------------------------------
    // cursor ���� ����
    //----------------------------------

    sUPTE->cursorType    = aUPTEInfo->cursorType;
    sUPTE->inplaceUpdate = aUPTEInfo->inplaceUpdate;

    //----------------------------------
    // limit ������ ����
    //----------------------------------

    sUPTE->limit = aUPTEInfo->limit;
    if ( sUPTE->limit != NULL )
    {
        sUPTE->flag &= ~QMNC_UPTE_LIMIT_MASK;
        sUPTE->flag |= QMNC_UPTE_LIMIT_TRUE;
    }
    else
    {
        sUPTE->flag &= ~QMNC_UPTE_LIMIT_MASK;
        sUPTE->flag |= QMNC_UPTE_LIMIT_FALSE;
    }

    //----------------------------------
    // constraint ����
    //----------------------------------

    // Foreign Key Referencing�� ���� Master Table�� �����ϴ� �� �˻�
    if ( qdnForeignKey::haveToCheckParent( sTableInfo,
                                           sUPTE->updateColumnIDs,
                                           aUPTEInfo->updateColumnCount )
         == ID_TRUE)
    {
        sUPTE->parentConstraints = aUPTEInfo->parentConstraints;
    }
    else
    {
        sUPTE->parentConstraints = NULL;
    }

    // Child Table�� Referencing ���� �˻�
    if ( qdnForeignKey::haveToOpenBeforeCursor( aUPTEInfo->childConstraints,
                                                sUPTE->updateColumnIDs,
                                                aUPTEInfo->updateColumnCount )
         == ID_TRUE )
    {
        sUPTE->childConstraints = aUPTEInfo->childConstraints;
    }
    else
    {
        sUPTE->childConstraints = NULL;
    }

    sUPTE->checkConstrList = aUPTEInfo->checkConstrList;

    //----------------------------------
    // return into ����
    //----------------------------------

    sUPTE->returnInto = aUPTEInfo->returnInto;

    //----------------------------------
    // Default Expr ����
    //----------------------------------

    sUPTE->defaultExprTableRef    = aUPTEInfo->defaultExprTableRef;
    sUPTE->defaultExprColumns     = aUPTEInfo->defaultExprColumns;
    sUPTE->defaultExprBaseColumns = aUPTEInfo->defaultExprBaseColumns;

    //----------------------------------
    // PROJ-1784 DML Without Retry
    //----------------------------------

    IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                    sUPTE->tableRef->table,
                                                    & sUPTE->whereColumnList )
              != IDE_SUCCESS );

    IDE_TEST( qdbCommon::makeSetClauseColumnList( aStatement,
                                                  sUPTE->tableRef->table,
                                                  & sUPTE->setColumnList )
              != IDE_SUCCESS );

    // partition�� where column list ����
    if ( sUPTE->tableRef->partitionRef != NULL )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                   (void **) &( sUPTE->wherePartColumnList ) )
                  != IDE_SUCCESS );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                   (void **) &( sUPTE->setPartColumnList ) )
                  != IDE_SUCCESS );

        for ( sPartitionRef  = sUPTE->tableRef->partitionRef, i = 0;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next, i++ )
        {
            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                            sPartitionRef->table,
                                                            &( sUPTE->wherePartColumnList[i] ) )
                      != IDE_SUCCESS );

            IDE_TEST( qdbCommon::makeSetClauseColumnList( aStatement,
                                                          sPartitionRef->table,
                                                          &( sUPTE->setPartColumnList[i] ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        sUPTE->wherePartColumnList = NULL;
        sUPTE->setPartColumnList   = NULL;
    }

    if ( aQuerySet->SFWGH->hints != NULL )
    {
        if( aQuerySet->SFWGH->hints->withoutRetry == ID_TRUE )
        {
            sUPTE->withoutRetry = ID_TRUE ;
        }
        else
        {
            sUPTE->withoutRetry = ( QCU_DML_WITHOUT_RETRY_ENABLE == 1 ) ? ID_TRUE : ID_FALSE ;
        }
    }
    else
    {
        sUPTE->withoutRetry = ID_FALSE;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sUPTE->plan ,
                                            QMO_UPTE_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    // PROJ-2551 simple query ����ȭ
    // simple insert�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleUPTE( aStatement, sUPTE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initDETE( qcStatement   * aStatement ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : DETE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncDETE�� �Ҵ� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmncDETE          * sDETE;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initDETE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncDETE ),
                                               (void **)& sDETE )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sDETE ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_DETE ,
                        qmnDETE ,
                        qmndDETE,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sDETE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeDETE( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmoDETEInfo   * aDETEInfo ,
                         qmnPlan       * aChildPlan ,
                         qmnPlan       * aPlan )
{
/***********************************************************************
 *
 * Description : DETE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncDETE          * sDETE = (qmncDETE *)aPlan;
    UInt                sDataNodeOffset;
    qmsFrom             sFrom;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeDETE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndDETE) );

    sDETE->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sDETE->flag = QMN_PLAN_FLAG_CLEAR;
    sDETE->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sDETE->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);
    sDETE->mTableList = NULL;
    sDETE->mMultiTableCount  = 0;
    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement,
                                     aDETEInfo->deleteTableRef->table,
                                     &( sDETE->plan.flag ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    QCP_SET_INIT_QMS_FROM( (&sFrom) );
    sFrom.tableRef = aDETEInfo->deleteTableRef;

    IDE_TEST( qmg::setDisplayInfo( & sFrom,
                                   &( sDETE->tableOwnerName ),
                                   &( sDETE->tableName ),
                                   &( sDETE->aliasName ) )
                 != IDE_SUCCESS );

    /* PROJ-2204 Join Update, Delete
     * join delete���� ǥ���Ѵ�. */
    if ( aQuerySet->SFWGH->from->tableRef->view != NULL )
    {
        sDETE->flag &= ~QMNC_DETE_VIEW_MASK;
        sDETE->flag |= QMNC_DETE_VIEW_TRUE;
    }
    else
    {
        // Nothing to do.
    }
    
    // key preserved property
    if ( aQuerySet->SFWGH->preservedInfo != NULL )
    {
        if ( aQuerySet->SFWGH->preservedInfo->useKeyPreservedTable == ID_TRUE )
        {
            sDETE->flag &= ~QMNC_DETE_VIEW_KEY_PRESERVED_MASK;
            sDETE->flag |= QMNC_DETE_VIEW_KEY_PRESERVED_TRUE;
        }
    }
        
    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    //----------------------------------
    // delete ���� ����
    //----------------------------------

    // delete target table ����
    sDETE->tableRef = aDETEInfo->deleteTableRef;

    // instead of trigger ����
    sDETE->insteadOfTrigger = aDETEInfo->insteadOfTrigger;

    //----------------------------------
    // limit ������ ����
    //----------------------------------

    sDETE->limit = aDETEInfo->limit;
    if ( sDETE->limit != NULL )
    {
        sDETE->flag &= ~QMNC_DETE_LIMIT_MASK;
        sDETE->flag |= QMNC_DETE_LIMIT_TRUE;
    }
    else
    {
        sDETE->flag &= ~QMNC_DETE_LIMIT_MASK;
        sDETE->flag |= QMNC_DETE_LIMIT_FALSE;
    }

    //----------------------------------
    // check constraint ����
    //----------------------------------

    sDETE->childConstraints = aDETEInfo->childConstraints;

    //----------------------------------
    // return into ����
    //----------------------------------

    sDETE->returnInto = aDETEInfo->returnInto;

    //----------------------------------
    // PROJ-1784 DML Without Retry
    //----------------------------------

    IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                    sDETE->tableRef->table,
                                                    & sDETE->whereColumnList )
              != IDE_SUCCESS );

    // partition�� where column list ����
    if ( sDETE->tableRef->partitionRef != NULL )
    {
        sPartitionCount = 0;
        for ( sPartitionRef  = sDETE->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next )
        {
            sPartitionCount++;
        }

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                   (void **) &( sDETE->wherePartColumnList ) )
                  != IDE_SUCCESS );

        for ( sPartitionRef  = sDETE->tableRef->partitionRef, i = 0;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next, i++ )
        {
            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                            sPartitionRef->table,
                                                            &( sDETE->wherePartColumnList[i] ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        sDETE->wherePartColumnList = NULL;
    }

    if ( aQuerySet->SFWGH->hints != NULL )
    {
        if( aQuerySet->SFWGH->hints->withoutRetry == ID_TRUE )
        {
            sDETE->withoutRetry = ID_TRUE ;
        }
        else
        {
            sDETE->withoutRetry = ( QCU_DML_WITHOUT_RETRY_ENABLE == 1 ) ? ID_TRUE : ID_FALSE ;
        }
    }
    else
    {
        sDETE->withoutRetry = ID_FALSE;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sDETE->plan ,
                                            QMO_DETE_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    // PROJ-2551 simple query ����ȭ
    // simple insert�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleDETE( aStatement, sDETE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::initMOVE( qcStatement   * aStatement ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : MOVE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncMOVE�� �Ҵ� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmncMOVE          * sMOVE;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initMOVE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncMOVE ),
                                               (void **)& sMOVE )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sMOVE ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_MOVE ,
                        qmnMOVE ,
                        qmndMOVE,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sMOVE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeMOVE( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmoMOVEInfo   * aMOVEInfo ,
                         qmnPlan       * aChildPlan ,
                         qmnPlan       * aPlan )
{
/***********************************************************************
 *
 * Description : MOVE ��带 �����Ѵ�.
 *
 * Implementation :
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncMOVE          * sMOVE = (qmncMOVE *)aPlan;
    UInt                sDataNodeOffset;

    qmsFrom             sFrom;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeMOVE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndMOVE) );

    sMOVE->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sMOVE->flag = QMN_PLAN_FLAG_CLEAR;
    sMOVE->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sMOVE->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement,
                                     aQuerySet->SFWGH->from->tableRef->table,
                                     &( sMOVE->plan.flag ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    // tableRef�� ������ ��
    sFrom.tableRef = aMOVEInfo->targetTableRef;

    IDE_TEST( qmg::setDisplayInfo( & sFrom,
                                   &( sMOVE->tableOwnerName),
                                   &( sMOVE->tableName),
                                   &( sMOVE->aliasName) )
              != IDE_SUCCESS );

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    //----------------------------------
    // move ���� ����
    //----------------------------------

    // source table ����
    sMOVE->tableRef = aQuerySet->SFWGH->from->tableRef;

    // target table ����
    sMOVE->targetTableRef = aMOVEInfo->targetTableRef;

    // insert ���� ����
    sMOVE->columns        = aMOVEInfo->columns;
    sMOVE->values         = aMOVEInfo->values;
    sMOVE->valueIdx       = aMOVEInfo->valueIdx;
    sMOVE->canonizedTuple = aMOVEInfo->canonizedTuple;
    sMOVE->compressedTuple= aMOVEInfo->compressedTuple;

    // sequence ����
    sMOVE->nextValSeqs    = aMOVEInfo->nextValSeqs;

    //----------------------------------
    // limit ������ ����
    //----------------------------------

    sMOVE->limit = aMOVEInfo->limit;
    if ( sMOVE->limit != NULL )
    {
        sMOVE->flag &= ~QMNC_MOVE_LIMIT_MASK;
        sMOVE->flag |= QMNC_MOVE_LIMIT_TRUE;
    }
    else
    {
        sMOVE->flag &= ~QMNC_MOVE_LIMIT_MASK;
        sMOVE->flag |= QMNC_MOVE_LIMIT_FALSE;
    }

    //----------------------------------
    // constraint ����
    //----------------------------------

    sMOVE->parentConstraints = aMOVEInfo->parentConstraints;
    sMOVE->childConstraints  = aMOVEInfo->childConstraints;
    sMOVE->checkConstrList   = aMOVEInfo->checkConstrList;

    //----------------------------------
    // Default Expr ����
    //----------------------------------

    sMOVE->defaultExprTableRef = aMOVEInfo->defaultExprTableRef;
    sMOVE->defaultExprColumns  = aMOVEInfo->defaultExprColumns;

    //----------------------------------
    // PROJ-1784 DML Without Retry
    //----------------------------------

    IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                    sMOVE->tableRef->table,
                                                    & sMOVE->whereColumnList )
              != IDE_SUCCESS );

    // partition�� where column list ����
    if ( sMOVE->tableRef->partitionRef != NULL )
    {
        sPartitionCount = 0;
        for ( sPartitionRef  = sMOVE->tableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next )
        {
            sPartitionCount++;
        }

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                   (void **) &( sMOVE->wherePartColumnList ) )
                  != IDE_SUCCESS );

        for ( sPartitionRef  = sMOVE->tableRef->partitionRef, i = 0;
              sPartitionRef != NULL;
              sPartitionRef  = sPartitionRef->next, i++ )
        {
            /* PROJ-2464 hybrid partitioned table ���� */
            IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                            sPartitionRef->table,
                                                            &( sMOVE->wherePartColumnList[i] ) )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        sMOVE->wherePartColumnList = NULL;
    }

    if ( aQuerySet->SFWGH->hints != NULL )
    {
        if( aQuerySet->SFWGH->hints->withoutRetry == ID_TRUE )
        {
            sMOVE->withoutRetry = ID_TRUE ;
        }
        else
        {
            sMOVE->withoutRetry = ( QCU_DML_WITHOUT_RETRY_ENABLE == 1 ) ? ID_TRUE : ID_FALSE ;
        }
    }
    else
    {
        sMOVE->withoutRetry = ID_FALSE;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sMOVE->plan ,
                                            QMO_MOVE_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::setDirectionInfo( UInt               * aFlag ,
                                 qcmIndex           * aIndex,
                                 qmgPreservedOrder  * aPreservedOrder )
{
/***********************************************************************
 *
 * Description : index�� preserved order�� ���� traverse direction��
 *               �����Ѵ�.
 *               preserved order�� not defined�� ��� full scan��
 *               ����ϴ� flag�� �����Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::setDirectionInfo::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------
    IDE_DASSERT( aFlag            != NULL );
    IDE_DASSERT( aIndex           != NULL );

    // Hierarchy�� ���ó�� preserved order�� �������� ���� ���
    if ( aPreservedOrder == NULL )
    {
        *aFlag &= ~QMNC_SCAN_TRAVERSE_MASK;
        *aFlag |= QMNC_SCAN_TRAVERSE_FORWARD;
    }
    else
    {
        IDE_DASSERT( aPreservedOrder->column ==
                     (UShort) (aIndex->keyColumns[0].column.id % SMI_COLUMN_ID_MAXIMUM) );

        if ( (aIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
             == SMI_COLUMN_ORDER_ASCENDING )
        {
            // index�� order�� ascending�϶�
            switch ( aPreservedOrder->direction )
            {
                case QMG_DIRECTION_NOT_DEFINED:
                    *aFlag &= ~QMNC_SCAN_FORCE_INDEX_SCAN_MASK;
                    *aFlag |= QMNC_SCAN_FORCE_INDEX_SCAN_FALSE;
                    /* fall through */
                case QMG_DIRECTION_ASC:
                case QMG_DIRECTION_ASC_NULLS_LAST:
                    *aFlag &= ~QMNC_SCAN_TRAVERSE_MASK;
                    *aFlag |= QMNC_SCAN_TRAVERSE_FORWARD;
                    break;
                case QMG_DIRECTION_DESC:
                case QMG_DIRECTION_DESC_NULLS_FIRST:
                    *aFlag &= ~QMNC_SCAN_TRAVERSE_MASK;
                    *aFlag |= QMNC_SCAN_TRAVERSE_BACKWARD;
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            // index�� order�� descending�϶�
            switch ( aPreservedOrder->direction )
            {
                case QMG_DIRECTION_NOT_DEFINED:
                    *aFlag &= ~QMNC_SCAN_FORCE_INDEX_SCAN_MASK;
                    *aFlag |= QMNC_SCAN_FORCE_INDEX_SCAN_FALSE;
                    /* fall through */
                case QMG_DIRECTION_DESC:
                case QMG_DIRECTION_DESC_NULLS_LAST:
                    *aFlag &= ~QMNC_SCAN_TRAVERSE_MASK;
                    *aFlag |= QMNC_SCAN_TRAVERSE_FORWARD;
                    break;
                case QMG_DIRECTION_ASC:
                case QMG_DIRECTION_ASC_NULLS_FIRST:
                    *aFlag &= ~QMNC_SCAN_TRAVERSE_MASK;
                    *aFlag |= QMNC_SCAN_TRAVERSE_BACKWARD;
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoOneNonPlan::setTableTypeFromTuple( qcStatement   * aStatement ,
                                      UInt            aTupleID ,
                                      UInt          * aFlag )
{
/***********************************************************************
 *
 * Description : �ش� Tuple�� ���� Storage �Ӽ��� ã�� �����Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::setTableTypeFromTuple::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFlag      != NULL );


    if ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aTupleID].lflag
           & MTC_TUPLE_STORAGE_MASK )
         == MTC_TUPLE_STORAGE_MEMORY )
    {
        *aFlag &= ~QMN_PLAN_STORAGE_MASK;
        *aFlag |= QMN_PLAN_STORAGE_MEMORY;
    }
    else
    {
        *aFlag &= ~QMN_PLAN_STORAGE_MASK;
        *aFlag |= QMN_PLAN_STORAGE_DISK;
    }

    return IDE_SUCCESS;
}

idBool
qmoOneNonPlan::isMemoryTableFromTuple( qcStatement   * aStatement ,
                                       UShort          aTupleID )
{
/***********************************************************************
 *
 * Description : �ش� Tuple�� ���� Storage�� �޸� ���̺����� ã�´�
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    if( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aTupleID].lflag
          & MTC_TUPLE_STORAGE_MASK ) ==
        MTC_TUPLE_STORAGE_MEMORY )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

IDE_RC
qmoOneNonPlan::classifyFixedNVariable( qcStatement    * aStatement ,
                                       qmsQuerySet    * aQuerySet ,
                                       qmoPredicate  ** aKeyPred ,
                                       qtcNode       ** aFixKey ,
                                       qtcNode       ** aFixKey4Print ,
                                       qtcNode       ** aVarKey ,
                                       qtcNode       ** aVarKey4Filter ,
                                       qmoPredicate  ** aFilter )
{
/***********************************************************************
 *
 * Description : FixKey(range ,filter) �� VarKey(range ,filter)�� �����Ѵ�
 *
 *
 * Implementation :
 *    - fixed , variable�� ����
 *        - �̴� qmoPredicate�� flag�� ������ �Ѵ�.
 *    - qtcNode���� ��ȯ
 *        - qmoPredicate -> qtcNode���� ��ȯ
 *        - DNF ������ ��� ��ȯ
 *
 *    - fixed �ΰ��
 *        - ��ȭ�� DNF ��� ����
 *        - display�� ���� fixXXX4Print�� CNF������ qtcNode ����
 *    - variable �ΰ��
 *        - ��ȭ�� DNF ��� ����
 *        - filter�� ���� ��츦 ���� CNF������ qtcNode ����
 *
 *    PROJ-1436 SQL Plan Cache
 *    proj-1436�������� fixed key�� ���Ͽ� prepare �������� �̸�
 *    smiRange�� �����Ͽ����� �̴� plan cache�� ���� ������
 *    smiRange�� �����ϰԵǾ� variable column�� ���Ͽ� smiValue.value��
 *    �����ϹǷ� ��ĩ ���ü� ������ �߻��� �� �ִ�. �׷��Ƿ�
 *    fixed key�� variable key�� ���������� execute ��������
 *    smiRange�� �����Ͽ� ������ ���ɼ��� ������ �Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate      * sLastFilter = NULL;
    qtcNode           * sKey        = NULL;
    qtcNode           * sDNFKey     = NULL;
    UInt                sEstDNFCnt;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::classifyFixedNVariable::__FT__" );

    IDE_DASSERT( *aKeyPred != NULL );

    // qtcNode���� ��ȯ
    IDE_TEST( qmoPred::linkPredicate( aStatement ,
                                      * aKeyPred ,
                                      & sKey )
              != IDE_SUCCESS );
    IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                       aQuerySet ,
                                       sKey )
              != IDE_SUCCESS );

    if ( sKey != NULL )
    {
        // To Fix PR-9481
        // CNF�� ������ Key Range Predicate�� DNF normalize�� ���
        // ��û���� ���� Node�� ��ȯ�� �� �ִ�.
        // �̸� �˻��Ͽ� ����ġ�� ���� ��ȭ�� �ʿ��� ��쿡��
        // Default Key Range���� �����ϰ� �Ѵ�.
        IDE_TEST( qmoNormalForm::estimateDNF( sKey, & sEstDNFCnt )
                  != IDE_SUCCESS );

        if ( sEstDNFCnt > QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement ) )
        {
            sDNFKey = NULL;
        }
        else
        {
            // DNF���·� ��ȯ
            IDE_TEST( qmoNormalForm::normalizeDNF( aStatement ,
                                                   sKey ,
                                                   & sDNFKey )
                      != IDE_SUCCESS );
        }

        // environment�� ���
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );
    }
    else
    {
        //nothing to do
    }

    if ( sDNFKey == NULL )
    {
        // To Fix PR-9481
        // Default Key Range�� �����ϰ� Key Range Predicate��
        // Filter�� �Ǿ�� �Ѵ�.

        if ( *aFilter == NULL )
        {
            *aFilter = *aKeyPred;
        }
        else
        {
            for ( sLastFilter        = *aFilter;
                  sLastFilter->next != NULL;
                  sLastFilter        = sLastFilter->next ) ;

            sLastFilter->next = *aKeyPred;
        }

        // fix BUG-10671
        // keyRange�� ������ �� ���� ���,
        // keyRange�� predicate���� filter���Ḯ��Ʈ�� �ű��
        // keyRange�� NULL�� �����ؾ� ��.
        *aKeyPred = NULL;

        *aFixKey = NULL;
        *aVarKey = NULL;

        *aFixKey4Print = NULL;
        *aVarKey4Filter = NULL;
    }
    else
    {
        // Fixed���� , variable���� flag�� ���� �Ǵ��Ѵ�.
        // �̴� Predicate �����ڷ� ���� ��� ó���Ǿ� ù qmoPredicate.flag��
        // ���յǾ� ���õȴ�.
        if ( ( (*aKeyPred)->flag & QMO_PRED_VALUE_MASK ) == QMO_PRED_FIXED  )
        {
            // Fixed Key Range/Filter�� ���

            *aFixKey = sDNFKey;
            *aVarKey = NULL;

            // Fixed Key ����� ���� ��������� �����Ѵ�.
            *aFixKey4Print = sKey;
            *aVarKey4Filter = NULL;
        }
        else
        {
            // Variable Key Range/Filter�� ���

            *aFixKey = NULL;
            *aVarKey = sDNFKey;

            *aFixKey4Print = NULL;
            // Range������ ������ ��� ����� Filter������ �����Ѵ�.
            *aVarKey4Filter = sKey;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::processPredicate( qcStatement     * aStatement,
                                        qmsQuerySet     * aQuerySet,
                                        qmoPredicate    * aPredicate,
                                        qmoPredicate    * aConstantPredicate,
                                        qmoPredicate    * aRidPredicate,
                                        qcmIndex        * aIndex,
                                        UShort            aTupleRowID,
                                        qtcNode        ** aConstantFilter,
                                        qtcNode        ** aFilter,
                                        qtcNode        ** aLobFilter,
                                        qtcNode        ** aSubqueryFilter,
                                        qtcNode        ** aVarKeyRange,
                                        qtcNode        ** aVarKeyFilter,
                                        qtcNode        ** aVarKeyRange4Filter,
                                        qtcNode        ** aVarKeyFilter4Filter,
                                        qtcNode        ** aFixKeyRange,
                                        qtcNode        ** aFixKeyFilter,
                                        qtcNode        ** aFixKeyRange4Print,
                                        qtcNode        ** aFixKeyFilter4Print,
                                        qtcNode        ** aRidRange,
                                        idBool          * aInSubQueryKeyRange)
{
/***********************************************************************
 *
 * Description : �־��� ������ ���� Predicate ó���� �Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    // 1. constant filter�� ó���Ѵ�.
    // 2. Predicate�� index������ �԰� keyRange , KeyFilter , filter ,
    //    lob filter, subquery filter�� �з� �Ѵ�.
    // 3. keyRange �� keyFilter�� �ٽ� fixed�� variable�� ������ �Ѵ�.
    // 4. �̸� qtcNode �Ǵ� smiRange�� �´� �ڷ� ������ ��ȯ �Ѵ�.

    qmoPredicate      * sKeyRangePred        = NULL;
    qmoPredicate      * sKeyFilterPred       = NULL;
    qmoPredicate      * sFilter              = NULL;
    qmoPredicate      * sLobFilter           = NULL;
    qmoPredicate      * sSubqueryFilter      = NULL;
    qmoPredicate      * sLastFilter          = NULL;
    qtcNode           * sVarKeyRange4Filter  = NULL;
    qtcNode           * sVarKeyFilter4Filter = NULL;
    qtcNode           * sNode;
    qtcNode           * sOptimizedNode;
    idBool              sIsMemory          = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::processPredicate::__FT__" );

    if ( aConstantPredicate != NULL )
    {
        // constant Predicate�� ó��
        IDE_TEST( qmoPred::linkPredicate( aStatement,
                                          aConstantPredicate,
                                          & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement,
                                           aQuerySet,
                                           sNode )
                  != IDE_SUCCESS );

        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        *aConstantFilter = sOptimizedNode;
    }
    else
    {
        *aConstantFilter = NULL;

    }

    if ( aRidPredicate != NULL )
    {
        IDE_TEST( makeRidRangePredicate( aRidPredicate,
                                         aPredicate,
                                         & aRidPredicate,
                                         & aPredicate,
                                         aRidRange )
                  != IDE_SUCCESS );
    }
    else
    {
        *aRidRange = NULL;
    }

    // qmoPredicate index�� ���� Predicate �з�
    sIsMemory = isMemoryTableFromTuple( aStatement , aTupleRowID );

    if ( aPredicate != NULL )
    {
        IDE_TEST( qmoPred::makeRangeAndFilterPredicate( aStatement,
                                                        aQuerySet,
                                                        sIsMemory,
                                                        aPredicate,
                                                        aIndex,
                                                        & sKeyRangePred,
                                                        & sKeyFilterPred,
                                                        & sFilter,
                                                        & sLobFilter,
                                                        & sSubqueryFilter )
                     != IDE_SUCCESS );
    }
    else
    {
        sKeyRangePred   = NULL;
        sKeyFilterPred  = NULL;
        sFilter         = NULL;
        sLobFilter      = NULL;
        sSubqueryFilter = NULL;
    }


    // fixed , variable����
    if ( sKeyRangePred != NULL )
    {

        if ( (sKeyRangePred->flag & QMO_PRED_INSUBQ_KEYRANGE_MASK )
             == QMO_PRED_INSUBQ_KEYRANGE_TRUE )
        {
            *aInSubQueryKeyRange = ID_TRUE;
        }
        else
        {
            *aInSubQueryKeyRange = ID_FALSE;
        }

        //keyrange�� ����
        IDE_TEST( classifyFixedNVariable( aStatement,
                                          aQuerySet,
                                          & sKeyRangePred,
                                          aFixKeyRange,
                                          aFixKeyRange4Print,
                                          aVarKeyRange,
                                          & sVarKeyRange4Filter,
                                          & sFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        *aVarKeyRange       = NULL;
        *aFixKeyRange       = NULL;
        *aFixKeyRange4Print = NULL;
    }

    if ( sKeyFilterPred != NULL )
    {
        if ( sKeyRangePred != NULL )
        {
            //bug-7165
            //variable key range�϶��� variable key filter�� �����Ѵ�.
            if ( ( sKeyRangePred->flag & QMO_PRED_VALUE_MASK )
                 == QMO_PRED_VARIABLE )
            {
                sKeyFilterPred->flag &= ~QMO_PRED_VALUE_MASK;
                sKeyFilterPred->flag |= QMO_PRED_VARIABLE;
            }
            else
            {
                //nothing to do
            }

            //keyFilter�� ����
            IDE_TEST( classifyFixedNVariable( aStatement,
                                              aQuerySet,
                                              & sKeyFilterPred,
                                              aFixKeyFilter,
                                              aFixKeyFilter4Print,
                                              aVarKeyFilter,
                                              & sVarKeyFilter4Filter,
                                              & sFilter )
                      != IDE_SUCCESS );
        }
        else
        {
            //nothing to do
            //key filter�� �����ϴ� ���

            if ( sFilter == NULL )
            {
                // predicate �з���, keyRange ���� keyFilter�� �з��� ���
                IDE_DASSERT(0);
            }
            else
            {
                // keyRange�� filter�� �з��� ���
                // (keyRange���� �޸� ũ�� �������� ����)
                for ( sLastFilter        = sFilter;
                      sLastFilter->next != NULL;
                      sLastFilter        = sLastFilter->next ) ;

                sLastFilter->next = sKeyFilterPred;
            }

            *aVarKeyFilter       = NULL;
            *aFixKeyFilter       = NULL;
            *aFixKeyFilter4Print = NULL;

            sKeyFilterPred = NULL;
        }
    }
    else
    {
        *aVarKeyFilter       = NULL;
        *aFixKeyFilter       = NULL;
        *aFixKeyFilter4Print = NULL;
    }

    if ( sFilter != NULL )
    {
        // filter�� ó��
        IDE_TEST( qmoPred::linkFilterPredicate( aStatement ,
                                                sFilter ,
                                                & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sNode )
                  != IDE_SUCCESS );

        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        *aFilter = sOptimizedNode;
    }
    else
    {
        *aFilter = NULL;
    }

    if ( sLobFilter != NULL )
    {
        IDE_FT_ASSERT( aLobFilter != NULL );

        // lobFilter�� ó��
        IDE_TEST( qmoPred::linkFilterPredicate( aStatement ,
                                                sLobFilter ,
                                                aLobFilter)
                  != IDE_SUCCESS );

        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           * aLobFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aLobFilter != NULL )
        {
            *aLobFilter = NULL;
        }
        else
        {
            // nothing to do
        }
    }

    if ( sSubqueryFilter != NULL )
    {
        // subqueryFilter�� ó��
        IDE_TEST( qmoPred::linkFilterPredicate( aStatement ,
                                                sSubqueryFilter ,
                                                & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sNode )
                  != IDE_SUCCESS );

        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        IDE_FT_ASSERT( aSubqueryFilter != NULL );

        // BUG-38971 subQuery filter �� ������ �ʿ䰡 �ֽ��ϴ�.
        if ( ( sOptimizedNode->node.lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_OR )
        {
            IDE_TEST( qmoPred::sortORNodeBySubQ( aStatement,
                                                 sOptimizedNode )
                      != IDE_SUCCESS );
        }
        else
        {
            //nothing to do
        }

        *aSubqueryFilter = sOptimizedNode;
    }
    else
    {
        if ( aSubqueryFilter != NULL )
        {
            *aSubqueryFilter = NULL;
        }
        else
        {
            //nothing to do
        }
    }

    if ( sVarKeyRange4Filter != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sVarKeyRange4Filter,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        *aVarKeyRange4Filter = sOptimizedNode;
    }
    else
    {
        *aVarKeyRange4Filter = NULL;
    }

    if ( sVarKeyFilter4Filter != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sVarKeyFilter4Filter,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        *aVarKeyFilter4Filter = sOptimizedNode;
    }
    else
    {
        *aVarKeyFilter4Filter = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeRidRangePredicate( qmoPredicate  * aInRidPred,
                                             qmoPredicate  * aInOtherPred,
                                             qmoPredicate ** aOutRidPred,
                                             qmoPredicate ** aOutOtherPred,
                                             qtcNode      ** aRidRange)
{
    IDE_RC sRc;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeRidRangePredicate::__FT__" );

    sRc = qmoRidPredExtractRangePred( aInRidPred,
                                      aInOtherPred,
                                      aOutRidPred,
                                      aOutOtherPred);
    IDE_TEST(sRc != IDE_SUCCESS);

    if ( *aOutRidPred == NULL )
    {
        *aRidRange = NULL;
    }
    else
    {
        *aRidRange = (*aOutRidPred)->node;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimplePROJ( qcStatement * aStatement,
                                qmncPROJ    * aPROJ )
{
/***********************************************************************
 *
 * Description : simple target �÷����� �˻��Ѵ�.
 *
 * Implementation :
 *     - �������� ������ Ÿ�Ը� �����ϴ�.
 *     - �����÷��� ����� �����ϴ�.
 *     - limit�� ����� �Ѵ�.
 *
 ***********************************************************************/

    qmncPROJ      * sPROJ = aPROJ;
    qmsTarget     * sTarget;
    qtcNode       * sNode;
    mtcColumn     * sColumn;
    mtcColumn     * sColumnArg;
    void          * sInfo;
    idBool          sIsSimple = ID_FALSE;
    qmnValueInfo  * sValueInfo = NULL;
    UInt          * sValueSizes = NULL;
    UInt          * sOffsets = NULL;
    UInt            sOffset = 0;
    UInt            i;
    qmncPCRD      * sPCRD = NULL;
    qmnChildren   * sChildren = NULL;
    qmncSCAN      * sSCAN = NULL;
    qcmColumn     * sQcmColumn = NULL;
    mtcColumn     * sTemp = NULL;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimplePROJ::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    // loop�� ����� �Ѵ�.
    if ( sPROJ->loopNode != NULL )
    {
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // scan limit ����� ��� limit�� ����
    if ( sPROJ->limit != NULL )
    {
        if ( ( sPROJ->limit->start.constant != 1 ) ||
             ( sPROJ->limit->count.constant == 0 ) ||
             ( sPROJ->limit->count.hostBindNode != NULL ) )
        {
            IDE_CONT( NORMAL_EXIT );
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

    // make simple values
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPROJ->targetCount * ID_SIZEOF( qmnValueInfo ),
                                               (void **) & sValueInfo )
              != IDE_SUCCESS );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPROJ->targetCount * ID_SIZEOF(UInt),
                                               (void **) & sValueSizes )
              != IDE_SUCCESS );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPROJ->targetCount * ID_SIZEOF(UInt),
                                               (void **) & sOffsets )
              != IDE_SUCCESS );

    sIsSimple = ID_TRUE;

    // simple target�� ���
    for ( sTarget  = sPROJ->myTarget, i = 0;
          sTarget != NULL;
          sTarget  = sTarget->next, i++ )
    {
        // target column�� �����÷��̰ų� ����̴�.
        sColumn = QTC_STMT_COLUMN( aStatement, sTarget->targetColumn );

        // �������� �������� �����ϴ�.
        if ( ( sColumn->module->id == MTD_SMALLINT_ID ) ||
             ( sColumn->module->id == MTD_BIGINT_ID ) ||
             ( sColumn->module->id == MTD_INTEGER_ID ) ||
             ( sColumn->module->id == MTD_CHAR_ID ) ||
             ( sColumn->module->id == MTD_VARCHAR_ID ) ||
             ( sColumn->module->id == MTD_NUMERIC_ID ) ||
             ( sColumn->module->id == MTD_FLOAT_ID ) ||
             ( sColumn->module->id == MTD_DATE_ID ) )
        {
            // Nothing to do.
        }
        else
        {
            sIsSimple = ID_FALSE;
            break;
        }

        // init value
        QMN_INIT_VALUE_INFO( &(sValueInfo[i]) );

        // set column
        sValueInfo[i].column = *sColumn;

        if ( QTC_IS_COLUMN( aStatement, sTarget->targetColumn ) == ID_TRUE )
        {
            // �����÷��� �ȵȴ�.
            if ( ( sColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK )
                 == SMI_COLUMN_COMPRESSION_TRUE )
            {
                sIsSimple = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }

            if ( sTarget->targetColumn->node.column == MTC_RID_COLUMN_ID )
            {
                // prowid
                sValueInfo[i].type = QMN_VALUE_TYPE_PROWID;

                sValueInfo[i].value.columnVal.table = sTarget->targetColumn->node.table;
                sValueInfo[i].value.columnVal.column = *sColumn;
            }
            else
            {
                // set target
                sValueInfo[i].type = QMN_VALUE_TYPE_COLUMN;

                sValueInfo[i].value.columnVal.table = sTarget->targetColumn->node.table;
                sValueInfo[i].value.columnVal.column = *sColumn;
            }
        }
        else
        {
            if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                                    sTarget->targetColumn )
                 == ID_TRUE )
            {
                // set target
                sValueInfo[i].type = QMN_VALUE_TYPE_CONST_VALUE;
                sValueInfo[i].value.constVal =
                    QTC_STMT_FIXEDDATA(aStatement, sTarget->targetColumn);
            }
            else
            {
                // BUG-43076 simple to_char
                if ( ( sTarget->targetColumn->node.module == &mtfTo_char ) &&
                     ( ( sTarget->targetColumn->node.lflag &
                         MTC_NODE_ARGUMENT_COUNT_MASK ) == 2 ) )
                {
                    sNode = (qtcNode*) sTarget->targetColumn->node.arguments;
                    sInfo = QTC_STMT_EXECUTE( aStatement, sTarget->targetColumn )
                        ->calculateInfo;
                    sColumnArg = QTC_STMT_COLUMN( aStatement, sNode );

                    if ( ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE ) &&
                         ( sColumnArg->module->id == MTD_DATE_ID ) &&
                         ( sInfo != NULL ) )
                    {
                        // set target
                        sValueInfo[i].type                   = QMN_VALUE_TYPE_TO_CHAR;
                        sValueInfo[i].value.columnVal.table  = sNode->node.table;
                        sValueInfo[i].value.columnVal.column = *sColumnArg;
                        sValueInfo[i].value.columnVal.info   = sInfo;
                    }
                    else
                    {
                        sIsSimple = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsSimple = ID_FALSE;
                    break;
                }
            }
        }

        // target tuple size
        sValueSizes[i] = idlOS::align8( sColumn->column.size );

        sOffsets[i]  = sOffset;
        sOffset     += sValueSizes[i];
    }

    if ( sPROJ->plan.left->type == QMN_PCRD )
    {
        sPCRD = (qmncPCRD*)sPROJ->plan.left;

        IDE_TEST_CONT( sPCRD->mIsSimple == ID_FALSE, NORMAL_EXIT );

        for ( sChildren = sPCRD->plan.children;
              sChildren != NULL;
              sChildren = sChildren->next )
        {
            sSCAN = (qmncSCAN*)sChildren->childPlan;

            IDU_FIT_POINT( "qmoOneNonPlan::checkSimplePROJ::alloc::sTemp",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPROJ->targetCount * ID_SIZEOF( mtcColumn ),
                                                       (void **)&sTemp )
                      != IDE_SUCCESS );

            for ( sTarget  = sPROJ->myTarget, i = 0;
                  sTarget != NULL;
                  sTarget  = sTarget->next, i++ )
            {
                sColumn = QTC_STMT_COLUMN( aStatement, sTarget->targetColumn );

                for ( sQcmColumn = sSCAN->partitionRef->partitionInfo->columns;
                      sQcmColumn != NULL;
                      sQcmColumn = sQcmColumn->next )
                {
                    if ( sQcmColumn->basicInfo->column.id == sColumn->column.id )
                    {
                        sTemp[i] = *sQcmColumn->basicInfo;
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
            sSCAN->mSimpleColumns = sTemp;
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    sPROJ->isSimple            = sIsSimple;
    sPROJ->simpleValues        = sValueInfo;
    sPROJ->simpleValueSizes    = sValueSizes;
    sPROJ->simpleResultOffsets = sOffsets;
    sPROJ->simpleResultSize    = sOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleSCAN( qcStatement  * aStatement,
                                qmncSCAN     * aSCAN )
{
/***********************************************************************
 *
 * Description : simple scan ������� �˻��Ѵ�.
 *
 * Implementation :
 *     - �������� ������ Ÿ�Ը� �����ϴ�.
 *     - ����� ȣ��Ʈ ������ �����ϴ�.
 *     - fixed key range�� variable key range�� �����ϴ�.
 *     - full scan�̳� index full scan�� �����ϴ�.
 *     - index key�� ��� ������� �ʾƵ� �����ϴ�.
 *
 ***********************************************************************/

    qmncSCAN      * sSCAN = aSCAN;
    idBool          sIsSimple = ID_FALSE;
    idBool          sIsUnique = ID_FALSE;
    idBool          sRidScan = ID_FALSE;
    UInt            sValueCount = 0;
    UInt            sCompareOpCount = 0;
    qmnValueInfo  * sValueInfo = NULL;
    UInt            sOffset = 0;
    qtcNode       * sORNode;
    qtcNode       * sCompareNode;
    qtcNode       * sColumnNode;
    qtcNode       * sValueNode;
    qcmIndex      * sIndex;
    UInt            i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleSCAN::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    // memory table�� ����
    if ( ( sSCAN->tableRef->remoteTable != NULL ) ||
         ( qcuTemporaryObj::isTemporaryTable( sSCAN->tableRef->tableInfo )
           == ID_TRUE ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( sSCAN->tableRef->tableInfo->tablePartitionType
         == QCM_PARTITIONED_TABLE )
    {
        if ( smiIsDiskTable( sSCAN->partitionRef->partitionHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( smiIsDiskTable( sSCAN->tableRef->tableHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // ��� limit�� ����
    if ( sSCAN->limit != NULL )
    {
        if ( ( sSCAN->limit->start.hostBindNode != NULL ) ||
             ( sSCAN->limit->count.constant == 0 ) ||
             ( sSCAN->limit->count.hostBindNode != NULL ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
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

    // simple scan�� �ƴ� ���
    if ( ( sSCAN->method.fixKeyFilter   != NULL ) ||
         ( sSCAN->method.varKeyFilter   != NULL ) ||
         ( sSCAN->method.constantFilter != NULL ) ||
         ( sSCAN->method.filter         != NULL ) ||
         ( sSCAN->method.subqueryFilter != NULL ) ||
         ( sSCAN->nnfFilter             != NULL ) ||
         /* BUG-45312 Simple Query ���� Index�� ���� ��� MIN, MAX�� ���� */
         ( ( sSCAN->flag & QMNC_SCAN_NOTNULL_RANGE_MASK )
           == QMNC_SCAN_NOTNULL_RANGE_TRUE ) ||
         /* BUG-45372 Simple Query FixedTable Index Bug */
         ( ( sSCAN->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
           == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( sSCAN->method.ridRange == NULL )
    {
        // simple full scan/index full scan�� ���
        if ( ( sSCAN->method.fixKeyRange == NULL ) &&
             ( sSCAN->method.varKeyRange == NULL ) )
        {
            // ���ڵ尡 ���� ��� simple�� ó������ �ʴ´�.
            // (�ϴ��� 1024000�� ������)
            if ( sSCAN->tableRef->statInfo->totalRecordCnt
                 <= QMO_STAT_TABLE_RECORD_COUNT * 100 )
            {
                sIsSimple = ID_TRUE;
            }
            else
            {
                sIsSimple = ID_FALSE;
            }

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }

        // index�� �ݵ�� �����ؾ� �Ѵ�.
        IDE_FT_ASSERT( sSCAN->method.index != NULL );

        // simple index scan�� ���
        sIndex = sSCAN->method.index;

        // ascending index�� ����
        for ( i = 0; i < sIndex->keyColCount; i++ )
        {
            if ( ( sIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                 != SMI_COLUMN_ORDER_ASCENDING )
            {
                sIsSimple = ID_FALSE;

                IDE_CONT( NORMAL_EXIT );
            }
            else
            {
                // Nothing to do.
            }
        }

        // make simple values
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ( sIndex->keyColCount + 1 ) * ID_SIZEOF( qmnValueInfo ), // min,max
                                                   (void **) & sValueInfo )
                  != IDE_SUCCESS );

        // fixed key range�� variable key range�� ���ÿ� �������� �ʴ´�.
        IDE_FT_ASSERT( ( ( sSCAN->method.fixKeyRange != NULL ) &&
                       ( sSCAN->method.varKeyRange == NULL ) ) ||
                     ( ( sSCAN->method.fixKeyRange == NULL ) &&
                       ( sSCAN->method.varKeyRange != NULL ) ) );

        if ( sSCAN->method.fixKeyRange != NULL )
        {
            sORNode = sSCAN->method.fixKeyRange;
        }
        else
        {
            sORNode = sSCAN->method.varKeyRange;
        }

        // OR�� AND�θ� �̷���� �ִ�.
        IDE_FT_ASSERT( sORNode->node.module == &mtfOr );
        IDE_FT_ASSERT( sORNode->node.arguments->module == &mtfAnd );

        // �ϳ��� AND�� �����ؾ� �Ѵ�.
        if ( sORNode->node.arguments->next != NULL )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }

        sIsSimple = ID_TRUE;

        for ( sCompareNode  = (qtcNode*)(sORNode->node.arguments->arguments), i = 0;
              sCompareNode != NULL;
              sCompareNode  = (qtcNode*)(sCompareNode->node.next), i++ )
        {
            /* BUG-49058 */
            if ( i >= ( sIndex->keyColCount + 1 ) )
            {
                sIsSimple = ID_FALSE;
                break;
            }

            // init value
            QMN_INIT_VALUE_INFO( &(sValueInfo[i]) );

            // =�� ���
            if ( sCompareNode->node.module == &mtfEqual )
            {
                // =������ =�� �ƴϾ��ٸ� simple�� �ƴϴ�.
                if ( sCompareOpCount == 0 )
                {
                    // set type
                    sValueInfo[i].op = QMN_VALUE_OP_EQUAL;
                }
                else
                {
                    sIsSimple = ID_FALSE;
                    break;
                }
            }
            else
            {
                // <, <=, >, >=�� ���
                if ( sCompareNode->node.module == &mtfLessThan )
                {
                    if ( sCompareNode->indexArgument == 0 )
                    {
                        // set type
                        sValueInfo[i].op = QMN_VALUE_OP_LT;
                    }
                    else
                    {
                        sValueInfo[i].op = QMN_VALUE_OP_GT;
                    }
                }
                else if ( sCompareNode->node.module == &mtfLessEqual )
                {
                    if ( sCompareNode->indexArgument == 0 )
                    {
                        // set type
                        sValueInfo[i].op = QMN_VALUE_OP_LE;
                    }
                    else
                    {
                        sValueInfo[i].op = QMN_VALUE_OP_GE;
                    }
                }
                else if ( sCompareNode->node.module == &mtfGreaterThan )
                {
                    if ( sCompareNode->indexArgument == 0 )
                    {
                        // set type
                        sValueInfo[i].op = QMN_VALUE_OP_GT;
                    }
                    else
                    {
                        sValueInfo[i].op = QMN_VALUE_OP_LT;
                    }
                }
                else if ( sCompareNode->node.module == &mtfGreaterEqual )
                {
                    if ( sCompareNode->indexArgument == 0 )
                    {
                        // set type
                        sValueInfo[i].op = QMN_VALUE_OP_GE;
                    }
                    else
                    {
                        sValueInfo[i].op = QMN_VALUE_OP_LE;
                    }
                }
                else
                {
                    sIsSimple = ID_FALSE;
                    break;
                }

                sCompareOpCount++;

                // 2���� ������ ����
                if ( sCompareOpCount <= 2 )
                {
                    // Nothing to do.
                }
                else
                {
                    sIsSimple = ID_FALSE;
                    break;
                }
            }

            if ( sCompareNode->indexArgument == 0 )
            {
                sColumnNode = (qtcNode*)(sCompareNode->node.arguments);
                sValueNode  = (qtcNode*)(sCompareNode->node.arguments->next);
            }
            else
            {
                sColumnNode = (qtcNode*)(sCompareNode->node.arguments->next);
                sValueNode  = (qtcNode*)(sCompareNode->node.arguments);
            }

            // check column and value
            IDE_TEST( checkSimpleSCANValue( aStatement,
                                            &( sValueInfo[i] ),
                                            sColumnNode,
                                            sValueNode,
                                            & sOffset,
                                            & sIsSimple )
                      != IDE_SUCCESS );

            if ( sIsSimple == ID_FALSE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }

            // ������ �ΰ��� �񱳿����� ���� �÷��̾�� �ϰ�
            // ��ȣ�� �ٸ� �񱳿����̾�� �Ѵ�.
            if ( sCompareOpCount == 2 )
            {
                if ( ( sValueInfo[i-1].column.column.id == sValueInfo[i].column.column.id ) &&
                     ( ( ( sValueInfo[i-1].op == QMN_VALUE_OP_LT ) &&
                         ( ( sValueInfo[i].op == QMN_VALUE_OP_GT ) ||
                           ( sValueInfo[i].op == QMN_VALUE_OP_GE ) ) )
                       ||
                       ( ( sValueInfo[i-1].op == QMN_VALUE_OP_LE ) &&
                         ( ( sValueInfo[i].op == QMN_VALUE_OP_GT ) ||
                           ( sValueInfo[i].op == QMN_VALUE_OP_GE ) ) )
                       ||
                       ( ( sValueInfo[i-1].op == QMN_VALUE_OP_GT ) &&
                         ( ( sValueInfo[i].op == QMN_VALUE_OP_LT ) ||
                           ( sValueInfo[i].op == QMN_VALUE_OP_LE ) ) )
                       ||
                       ( ( sValueInfo[i-1].op == QMN_VALUE_OP_GE ) &&
                         ( ( sValueInfo[i].op == QMN_VALUE_OP_LT ) ||
                           ( sValueInfo[i].op == QMN_VALUE_OP_LE ) ) ) ) )
                {
                    // Nothing to do.
                }
                else
                {
                    sIsSimple = ID_FALSE;
                    break;
                }
            }
            else
            {
                // Nothing to do.
            }

            // =������ ���� �÷��� �� �� ����, <,<=,>,>=�� ��� ���� �÷��� �����ϴ�.
            if ( sValueInfo[i].op == QMN_VALUE_OP_EQUAL )
            {
                if ( i > 0 )
                {
                    if ( sValueInfo[i - 1].column.column.id ==
                         sValueInfo[i].column.column.id )
                    {
                        sIsSimple = ID_FALSE;
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
                // 2���� ��� ������ ���� �÷��̾�� �Ѵ�.
                if ( sCompareOpCount == 2 )
                {
                    if ( sValueInfo[i - 1].column.column.id ==
                         sValueInfo[i].column.column.id )
                    {
                        // Nothing to do.
                    }
                    else
                    {
                        sIsSimple = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        IDE_TEST_CONT( sIsSimple == ID_FALSE, NORMAL_EXIT );

        sValueCount = i;

        // �ִ� 1���� ���
        if ( ( sIndex->keyColCount == sValueCount ) &&
             ( sIndex->isUnique == ID_TRUE ) &&
             ( sCompareOpCount == 0 ) )
        {
            sIsUnique = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        /* BUG-45376 Simple Query Index Bug */
        if ( sIndex->keyColCount < sValueCount )
        {
            sIsSimple = ID_FALSE;
        }
        else
        {
            for ( i = 0; i < sValueCount; i++ )
            {
                if ( sValueInfo[i].column.column.id != sIndex->keyColumns[i].column.id )
                {
                    sIsSimple = ID_FALSE;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }
    else
    {
        // rid scan�� ���
        sORNode = sSCAN->method.ridRange;

        // make simple values
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmnValueInfo ),
                                                   (void **)& sValueInfo )
                  != IDE_SUCCESS );

        sValueCount = 1;

        // OR�θ� �̷���� �ִ�.
        IDE_FT_ASSERT( sORNode->node.module == &mtfOr );

        // OR�� argument�� equal�� �ִ�.
        sCompareNode = (qtcNode*)(sORNode->node.arguments);

        // �ϳ��� equal�� ����Ѵ�.
        if ( ( sCompareNode->node.module == &mtfEqual ) &&
             ( sCompareNode->node.next == NULL ) )
        {
            // Nothing to do.
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        sIsSimple = ID_TRUE;
        sIsUnique = ID_TRUE;
        sRidScan  = ID_TRUE;

        if ( sCompareNode->indexArgument == 0 )
        {
            sColumnNode = (qtcNode*)(sCompareNode->node.arguments);
            sValueNode  = (qtcNode*)(sCompareNode->node.arguments->next);
        }
        else
        {
            sColumnNode = (qtcNode*)(sCompareNode->node.arguments->next);
            sValueNode  = (qtcNode*)(sCompareNode->node.arguments);
        }

        // init value
        QMN_INIT_VALUE_INFO( sValueInfo );

        // set type
        sValueInfo->op = QMN_VALUE_OP_EQUAL;

        // check column and value
        IDE_TEST( checkSimpleSCANValue( aStatement,
                                        sValueInfo,
                                        sColumnNode,
                                        sValueNode,
                                        & sOffset,
                                        & sIsSimple )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    sSCAN->isSimple             = sIsSimple;
    sSCAN->simpleValueCount     = sValueCount;
    sSCAN->simpleValues         = sValueInfo;
    sSCAN->simpleValueBufSize   = sOffset;
    sSCAN->simpleUnique         = sIsUnique;
    sSCAN->simpleRid            = sRidScan;
    sSCAN->simpleCompareOpCount = sCompareOpCount;
    sSCAN->mSimpleColumns       = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleSCANValue( qcStatement  * aStatement,
                                     qmnValueInfo * aValueInfo,
                                     qtcNode      * aColumnNode,
                                     qtcNode      * aValueNode,
                                     UInt         * aOffset,
                                     idBool       * aIsSimple )
{
    idBool      sIsSimple = ID_TRUE;
    mtcColumn * sColumn;
    mtcColumn * sConstColumn;
    mtcColumn * sValueColumn;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleSCANValue::__FT__" );

    // �����÷��� ���̾�� �Ѵ�.
    if ( QTC_IS_COLUMN( aStatement, aColumnNode ) == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }

    sColumn = QTC_STMT_COLUMN( aStatement, aColumnNode );

    // �������� �������� �����ϴ�.
    if ( ( sColumn->module->id == MTD_SMALLINT_ID ) ||
         ( sColumn->module->id == MTD_BIGINT_ID ) ||
         ( sColumn->module->id == MTD_INTEGER_ID ) ||
         ( sColumn->module->id == MTD_CHAR_ID ) ||
         ( sColumn->module->id == MTD_VARCHAR_ID ) ||
         ( sColumn->module->id == MTD_NUMERIC_ID ) ||
         ( sColumn->module->id == MTD_FLOAT_ID ) )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }

    aValueInfo->column = *sColumn;

    // �÷�, ����� ȣ��Ʈ������ �����ϴ�.
    if ( QTC_IS_COLUMN( aStatement, aValueNode ) == ID_TRUE )
    {
        // �÷��� ��� column node�� value node�� conversion��
        // ����� �Ѵ�.
        if ( ( aColumnNode->node.conversion != NULL ) ||
             ( aValueNode->node.conversion != NULL ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }

        sValueColumn = QTC_STMT_COLUMN( aStatement, aValueNode );

        // ���� Ÿ���̾�� �Ѵ�.
        if ( sColumn->module->id == sValueColumn->module->id )
        {
            aValueInfo->type = QMN_VALUE_TYPE_COLUMN;

            aValueInfo->value.columnVal.table  = aValueNode->node.table;
            aValueInfo->value.columnVal.column = *sValueColumn;
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                            aValueNode )
         == ID_TRUE )
    {
        // ����� ��� column node�� conversion�� ����� �Ѵ�.
        if ( aColumnNode->node.conversion != NULL )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }

        sConstColumn = QTC_STMT_COLUMN( aStatement, aValueNode );

        // ����� ���� Ÿ���̾�� �Ѵ�.
        if ( ( sColumn->module->id == sConstColumn->module->id ) ||
             ( ( sColumn->module->id == MTD_NUMERIC_ID ) &&
               ( sConstColumn->module->id == MTD_FLOAT_ID ) ) ||
             ( ( sColumn->module->id == MTD_FLOAT_ID ) &&
               ( sConstColumn->module->id == MTD_NUMERIC_ID ) ) )
        {
            aValueInfo->type = QMN_VALUE_TYPE_CONST_VALUE;
            aValueInfo->value.constVal =
                QTC_STMT_FIXEDDATA(aStatement, aValueNode);
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // post process�� ���������Ƿ� host const warpper�� �޸� �� �ִ�.
    if ( aValueNode->node.module == &qtc::hostConstantWrapperModule )
    {
        aValueNode = (qtcNode*) aValueNode->node.arguments;
    }
    else
    {
        // Nothing to do.
    }

    if ( qtc::isHostVariable( QC_SHARED_TMPLATE(aStatement),
                              aValueNode )
         == ID_TRUE )
    {
        // ȣ��Ʈ ������ ��� column node�� value node��
        // conversion node�� ���� Ÿ�����θ� bind�Ѵٸ�
        // ������ �� �ִ�.
        aValueInfo->type = QMN_VALUE_TYPE_HOST_VALUE;
        aValueInfo->value.id = aValueNode->node.column;

        // value buffer size
        (*aOffset) += idlOS::align8( sColumn->column.size );

        // param ���
        IDE_TEST( qmv::describeParamInfo( aStatement,
                                          sColumn,
                                          aValueNode )
                  != IDE_SUCCESS );

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // ������� �Դٸ� simple�� �ƴϴ�.
    sIsSimple = ID_FALSE;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    *aIsSimple = sIsSimple;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleINST( qcStatement * aStatement,
                                qmncINST    * aINST )
{
/***********************************************************************
 *
 * Description : simple insert ������� �˻��Ѵ�.
 *
 * Implementation :
 *     - insert values�� �����ϴ�.
 *     - ������, ������, ��¥���� �����ϴ�.
 *     - ���, ȣ��Ʈ����, sysdate�� �����ϴ�.
 *     - trigger, lob column, return into���� �ȵȴ�.
 *
 ***********************************************************************/

    qmncINST      * sINST = aINST;
    idBool          sIsSimple = ID_FALSE;
    UInt            sValueCount = 0;
    qmnValueInfo  * sValueInfo = NULL;
    UInt            sOffset = 0;
    qcmTableInfo  * sTableInfo = NULL;
    qmmValueNode  * sValueNode;
    qcmParentInfo * sParentInfo;
    UInt            i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleINST::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    sIsSimple = ID_TRUE;

    // ��Ÿ ��� �˻�
    if ( ( sINST->isInsertSelect == ID_TRUE ) ||
         ( sINST->isMultiInsertSelect == ID_TRUE ) ||
         ( sINST->insteadOfTrigger == ID_TRUE ) ||
         ( sINST->isAppend == ID_TRUE ) ||
         ( sINST->tableRef->tableInfo->triggerCount > 0 ) ||
         ( sINST->tableRef->tableInfo->lobColumnCount > 0 ) ||
         ( smiIsDiskTable( sINST->tableRef->tableHandle )
           == ID_TRUE ) ||
         ( qcuTemporaryObj::isTemporaryTable( sINST->tableRef->tableInfo )
           == ID_TRUE ) ||
         ( sINST->compressedTuple != UINT_MAX ) ||
         ( sINST->nextValSeqs != NULL ) ||
         ( sINST->defaultExprColumns != NULL ) ||
         ( sINST->checkConstrList != NULL ) ||
         ( sINST->returnInto != NULL ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( sINST->tableRef->tableInfo->tablePartitionType
         == QCM_PARTITIONED_TABLE )
    {
        sINST->flag &= ~QMNC_INST_PARTITIONED_MASK;
        sINST->flag |= QMNC_INST_PARTITIONED_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-43410 foreign key ����
    for ( sParentInfo  = sINST->parentConstraints;
          sParentInfo != NULL;
          sParentInfo  = sParentInfo->next )
    {
        if ( smiIsDiskTable( sParentInfo->parentTableRef->tableHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }
    }

    sTableInfo  = sINST->tableRef->tableInfo;
    sValueCount = sTableInfo->columnCount;

    // make simple values
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sValueCount * ID_SIZEOF( qmnValueInfo ),
                                               (void **) & sValueInfo )
              != IDE_SUCCESS );

    if ( sINST->rows->next != NULL )
    {
        sIsSimple = ID_FALSE;
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    for ( sValueNode = sINST->rows->values, i = 0;
          ( sValueNode != NULL ) && ( i < sValueCount );
          sValueNode = sValueNode->next, i++ )
    {
        // simple�� �ƴ� value
        if ( ( sValueNode->timestamp == ID_TRUE ) ||
             ( sValueNode->value == NULL ) )
        {
            sIsSimple = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        // init value
        QMN_INIT_VALUE_INFO( &(sValueInfo[i]) );

        //BUG-45715 support enqueue 
        if ( sValueNode->msgID == ID_TRUE )
        {
            sValueInfo[i].isQueue = ID_TRUE;
        }
        else
        {
            // nothing to do
        }

        // check column and value
        IDE_TEST( checkSimpleINSTValue( aStatement,
                                        &( sValueInfo[i] ),
                                        sTableInfo->columns[i].basicInfo,
                                        sValueNode,
                                        & sOffset,
                                        & sIsSimple )
                  != IDE_SUCCESS );

        if ( sIsSimple == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST_CONT( sIsSimple == ID_FALSE, NORMAL_EXIT );

    if ( ( sValueNode == NULL ) && ( i == sValueCount ) )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    sINST->isSimple           = sIsSimple;
    sINST->simpleValueCount   = sValueCount;
    sINST->simpleValues       = sValueInfo;
    sINST->simpleValueBufSize = sOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleINSTValue( qcStatement  * aStatement,
                                     qmnValueInfo * aValueInfo,
                                     mtcColumn    * aColumn,
                                     qmmValueNode * aValueNode,
                                     UInt         * aOffset,
                                     idBool       * aIsSimple )
{
    idBool          sIsSimple = ID_TRUE;
    mtcColumn     * sConstColumn;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleINSTValue::__FT__" );

    // �������� �������� �����ϴ�.
    if ( ( aColumn->module->id == MTD_SMALLINT_ID ) ||
         ( aColumn->module->id == MTD_BIGINT_ID ) ||
         ( aColumn->module->id == MTD_INTEGER_ID ) ||
         ( aColumn->module->id == MTD_CHAR_ID ) ||
         ( aColumn->module->id == MTD_VARCHAR_ID ) ||
         ( aColumn->module->id == MTD_NUMERIC_ID ) ||
         ( aColumn->module->id == MTD_FLOAT_ID ) ||
         ( aColumn->module->id == MTD_DATE_ID ) )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }

    aValueInfo->column = *aColumn;

    if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                            aValueNode->value )
         == ID_TRUE )
    {
        sConstColumn = QTC_STMT_COLUMN( aStatement, aValueNode->value );

        // ����� ���� Ÿ���̾�� �Ѵ�.
        if ( ( aColumn->module->id == sConstColumn->module->id ) ||
             ( ( aColumn->module->id == MTD_NUMERIC_ID ) &&
               ( sConstColumn->module->id == MTD_FLOAT_ID ) ) ||
             ( ( aColumn->module->id == MTD_FLOAT_ID ) &&
               ( sConstColumn->module->id == MTD_NUMERIC_ID ) ) )
        {
            aValueInfo->type = QMN_VALUE_TYPE_CONST_VALUE;
            aValueInfo->value.constVal =
                QTC_STMT_FIXEDDATA(aStatement, aValueNode->value);

            // canonize buffer size
            (*aOffset) += idlOS::align8( aColumn->column.size );
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( qtc::isHostVariable( QC_SHARED_TMPLATE(aStatement),
                              aValueNode->value )
         == ID_TRUE )
    {
        aValueInfo->type     = QMN_VALUE_TYPE_HOST_VALUE;
        aValueInfo->value.id = aValueNode->value->node.column;

        // value buffer size
        (*aOffset) += idlOS::align8( aColumn->column.size );

        // param ���
        IDE_TEST( qmv::describeParamInfo( aStatement,
                                          aColumn,
                                          aValueNode->value )
                  != IDE_SUCCESS );

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // sysdate�� �� �ִ�.
    if ( ( aColumn->module->id == MTD_DATE_ID ) &&
         ( QC_SHARED_TMPLATE(aStatement)->sysdate != NULL ) )
    {
        if ( ( aValueNode->value->node.table ==
               QC_SHARED_TMPLATE(aStatement)->sysdate->node.table ) &&
             ( aValueNode->value->node.column ==
               QC_SHARED_TMPLATE(aStatement)->sysdate->node.column ) )
        {
            aValueInfo->type = QMN_VALUE_TYPE_SYSDATE;
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // ������� �Դٸ� simple�� �ƴϴ�.
    sIsSimple = ID_FALSE;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    *aIsSimple = sIsSimple;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleUPTE( qcStatement  * aStatement,
                                qmncUPTE     * aUPTE )
{
/***********************************************************************
 *
 * Description : simple update ������� �˻��Ѵ�.
 *
 * Implementation :
 *     simple update�� ��� fast execute�� �����Ѵ�.
 *
 ***********************************************************************/

    qmncUPTE        * sUPTE = aUPTE;
    idBool            sIsSimple = ID_FALSE;
    qmnValueInfo    * sValueInfo = NULL;
    UInt              sOffset = 0;
    qcmColumn       * sColumnNode;
    qmmValueNode    * sValueNode;
    qcmParentInfo   * sParentInfo;
    qcmRefChildInfo * sChildInfo;
    UInt              i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleUPTE::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    sIsSimple = ID_TRUE;

    // ��Ÿ ��� �˻�
    if ( ( sUPTE->insteadOfTrigger == ID_TRUE ) ||
         ( sUPTE->tableRef->tableInfo->triggerCount > 0 ) ||
         ( sUPTE->compressedTuple != UINT_MAX ) ||
         ( sUPTE->subqueries != NULL ) ||
         ( sUPTE->nextValSeqs != NULL ) ||
         ( sUPTE->defaultExprColumns != NULL ) ||
         ( sUPTE->checkConstrList != NULL ) ||
         ( sUPTE->returnInto != NULL ) || 
         ( sUPTE->updateType == QMO_UPDATE_ROWMOVEMENT ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( sUPTE->tableRef->tableInfo->tablePartitionType
         == QCM_PARTITIONED_TABLE )
    {
        sUPTE->flag &= ~QMNC_UPTE_PARTITIONED_MASK;
        sUPTE->flag |= QMNC_UPTE_PARTITIONED_TRUE;

        if ( sUPTE->tableRef->partitionSummary != NULL )
        {
            if ( sUPTE->tableRef->partitionSummary->diskPartitionCount > 0 )
            {
                sIsSimple = ID_FALSE;
                IDE_CONT( NORMAL_EXIT );
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
        if ( smiIsDiskTable( sUPTE->tableRef->tableHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // scan limit ����� ��� limit�� ����
    if ( sUPTE->limit != NULL )
    {
        if ( ( sUPTE->limit->start.constant != 1 ) ||
             ( sUPTE->limit->count.constant == 0 ) ||
             ( sUPTE->limit->count.hostBindNode != NULL ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
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

    // BUG-43410 foreign key ����
    for ( sParentInfo  = sUPTE->parentConstraints;
          sParentInfo != NULL;
          sParentInfo  = sParentInfo->next )
    {
        if ( smiIsDiskTable( sParentInfo->parentTableRef->tableHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }
    }

    // BUG-43410 foreign key ����
    for ( sChildInfo  = sUPTE->childConstraints;
          sChildInfo != NULL;
          sChildInfo  = sChildInfo->next )
    {
        if ( ( sChildInfo->isSelfRef == ID_TRUE ) ||
             ( smiIsDiskTable( sChildInfo->childTableRef->tableHandle ) == ID_TRUE ) ||
             /* BUG-45370 Simple Foreignkey Child constraint trigger */
             ( sChildInfo->childTableRef->tableInfo->triggerCount > 0 ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }
    }

    // make simple values
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sUPTE->updateColumnCount * ID_SIZEOF( qmnValueInfo ),
                                               (void **) & sValueInfo )
              != IDE_SUCCESS );

    for ( sColumnNode = sUPTE->columns, sValueNode = sUPTE->values, i = 0;
          ( sColumnNode != NULL ) && ( sValueNode != NULL );
          sColumnNode = sColumnNode->next, sValueNode = sValueNode->next, i++ )
    {
        // init value
        QMN_INIT_VALUE_INFO( &(sValueInfo[i]) );

        IDE_TEST( checkSimpleUPTEValue( aStatement,
                                        &( sValueInfo[i] ),
                                        sColumnNode->basicInfo,
                                        sValueNode,
                                        & sOffset,
                                        & sIsSimple )
                     != IDE_SUCCESS );

        if ( sIsSimple == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST_CONT( sIsSimple == ID_FALSE, NORMAL_EXIT );

    if ( ( sColumnNode == NULL ) && ( sValueNode == NULL ) )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    sUPTE->isSimple           = sIsSimple;
    sUPTE->simpleValues       = sValueInfo;
    sUPTE->simpleValueBufSize = sOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleUPTEValue( qcStatement  * aStatement,
                                     qmnValueInfo * aValueInfo,
                                     mtcColumn    * aColumn,
                                     qmmValueNode * aValueNode,
                                     UInt         * aOffset,
                                     idBool       * aIsSimple )
{
    idBool          sIsSimple = ID_TRUE;
    mtcColumn     * sConstColumn;
    mtcColumn     * sSetMtcColumn;
    qtcNode       * sSetColumn;
    qtcNode       * sSetValue;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleUPTEValue::__FT__" );

    // simple value�� �ƴ�
    if ( ( aValueNode->timestamp == ID_TRUE ) ||
         ( aValueNode->msgID == ID_TRUE ) ||
         ( aValueNode->value == NULL ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    aValueInfo->column = *aColumn;

    // �������� �������� �����ϴ�.
    if ( ( aColumn->module->id == MTD_SMALLINT_ID ) ||
         ( aColumn->module->id == MTD_BIGINT_ID ) ||
         ( aColumn->module->id == MTD_INTEGER_ID ) ||
         ( aColumn->module->id == MTD_CHAR_ID ) ||
         ( aColumn->module->id == MTD_VARCHAR_ID ) ||
         ( aColumn->module->id == MTD_NUMERIC_ID ) ||
         ( aColumn->module->id == MTD_FLOAT_ID ) ||
         ( aColumn->module->id == MTD_DATE_ID ) )
    {
        // Nothing to do.
    }
    else
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }

    sSetValue = aValueNode->value;

    // =, +, -�� �����ϴ�.
    if ( ( sSetValue->node.module == &qtc::columnModule ) ||
         ( sSetValue->node.module == &qtc::valueModule ) )
    {
        // set i1 = 1
        aValueInfo->op = QMN_VALUE_OP_ASSIGN;
    }
    else if ( sSetValue->node.module == &mtfAdd2 )
    {
        // �������� �����ϴ�.
        if ( ( aColumn->module->id == MTD_SMALLINT_ID ) ||
             ( aColumn->module->id == MTD_BIGINT_ID ) ||
             ( aColumn->module->id == MTD_INTEGER_ID ) ||
             ( aColumn->module->id == MTD_NUMERIC_ID ) ||
             ( aColumn->module->id == MTD_FLOAT_ID ) )
        {
            // Nothing to do.
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        // set i1 = i1 + 1
        aValueInfo->op = QMN_VALUE_OP_ADD;

        sSetColumn = (qtcNode*)(sSetValue->node.arguments);
        sSetMtcColumn = QTC_STMT_COLUMN( aStatement, sSetColumn );

        if ( aColumn->column.id == sSetMtcColumn->column.id )
        {
            sSetValue = (qtcNode*)(sSetColumn->node.next);
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
    }
    else if ( sSetValue->node.module == &mtfSubtract2 )
    {
        // �������� �����ϴ�.
        if ( ( aColumn->module->id == MTD_SMALLINT_ID ) ||
             ( aColumn->module->id == MTD_BIGINT_ID ) ||
             ( aColumn->module->id == MTD_INTEGER_ID ) ||
             ( aColumn->module->id == MTD_NUMERIC_ID ) ||
             ( aColumn->module->id == MTD_FLOAT_ID ) )
        {
            // Nothing to do.
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        // set i1 = i1 - 1
        aValueInfo->op = QMN_VALUE_OP_SUB;

        sSetColumn = (qtcNode*)(sSetValue->node.arguments);
        sSetMtcColumn = QTC_STMT_COLUMN( aStatement, sSetColumn );

        if ( aColumn->column.id == sSetMtcColumn->column.id )
        {
            sSetValue = (qtcNode*)(sSetColumn->node.next);
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
    }
    else
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }

    if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                            sSetValue )
         == ID_TRUE )
    {
        sConstColumn = QTC_STMT_COLUMN( aStatement, sSetValue );

        // ����� ���� Ÿ���̾�� �Ѵ�.
        if ( ( aColumn->module->id == sConstColumn->module->id ) ||
             ( ( aColumn->module->id == MTD_NUMERIC_ID ) &&
               ( sConstColumn->module->id == MTD_FLOAT_ID ) ) ||
             ( ( aColumn->module->id == MTD_FLOAT_ID ) &&
               ( sConstColumn->module->id == MTD_NUMERIC_ID ) ) )
        {
            aValueInfo->type = QMN_VALUE_TYPE_CONST_VALUE;
            aValueInfo->value.constVal =
                QTC_STMT_FIXEDDATA(aStatement, sSetValue);

            // canonize buffer size, new value buffer size
            (*aOffset) += idlOS::align8( aColumn->column.size );
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( qtc::isHostVariable( QC_SHARED_TMPLATE(aStatement),
                              sSetValue )
         == ID_TRUE )
    {
        aValueInfo->type = QMN_VALUE_TYPE_HOST_VALUE;
        aValueInfo->value.id = sSetValue->node.column;

        // value buffer size, new value buffer size
        (*aOffset) += idlOS::align8( aColumn->column.size );

        // param ���
        IDE_TEST( qmv::describeParamInfo( aStatement,
                                          aColumn,
                                          sSetValue )
                  != IDE_SUCCESS );

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // sysdate�� �� �ִ�.
    if ( ( aColumn->module->id == MTD_DATE_ID ) &&
         ( QC_SHARED_TMPLATE(aStatement)->sysdate != NULL ) )
    {
        if ( ( aValueNode->value->node.table ==
               QC_SHARED_TMPLATE(aStatement)->sysdate->node.table ) &&
             ( aValueNode->value->node.column ==
               QC_SHARED_TMPLATE(aStatement)->sysdate->node.column ) )
        {
            aValueInfo->type = QMN_VALUE_TYPE_SYSDATE;
        }
        else
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    // ������� �Դٸ� simple�� �ƴϴ�.
    sIsSimple = ID_FALSE;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    *aIsSimple = sIsSimple;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::checkSimpleDETE( qcStatement  * aStatement,
                                qmncDETE     * aDETE )
{
/***********************************************************************
 *
 * Description : simple delete ������� �˻��Ѵ�.
 *
 * Implementation :
 *     simple delete�� ��� fast execute�� �����Ѵ�.
 *
 ***********************************************************************/

    qmncDETE        * sDETE = aDETE;
    qcmRefChildInfo * sChildInfo;
    idBool            sIsSimple = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::checkSimpleDETE::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    sIsSimple = ID_TRUE;

    if ( ( sDETE->insteadOfTrigger == ID_TRUE ) ||
         ( sDETE->tableRef->tableInfo->triggerCount > 0 ) ||
         ( sDETE->returnInto != NULL ) )
    {
        sIsSimple = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( sDETE->tableRef->tableInfo->tablePartitionType
         == QCM_PARTITIONED_TABLE )
    {
        sDETE->flag &= ~QMNC_DETE_PARTITIONED_MASK;
        sDETE->flag |= QMNC_DETE_PARTITIONED_TRUE;

        if ( sDETE->tableRef->partitionSummary != NULL )
        {
            if ( sDETE->tableRef->partitionSummary->diskPartitionCount > 0 )
            {
                sIsSimple = ID_FALSE;
                IDE_CONT( NORMAL_EXIT );
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
        if ( smiIsDiskTable( sDETE->tableRef->tableHandle ) == ID_TRUE )
        {
            sIsSimple = ID_FALSE;
            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }
    }

    // scan limit ����� ��� limit�� ����
    if ( sDETE->limit != NULL )
    {
        if ( ( sDETE->limit->start.constant != 1 ) ||
             ( sDETE->limit->count.constant == 0 ) ||
             ( sDETE->limit->count.hostBindNode != NULL ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
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

    // BUG-43410 foreign key ����
    for ( sChildInfo  = sDETE->childConstraints;
          sChildInfo != NULL;
          sChildInfo  = sChildInfo->next )
    {
        if ( ( sChildInfo->isSelfRef == ID_TRUE ) ||
             ( smiIsDiskTable( sChildInfo->childTableRef->tableHandle ) == ID_TRUE ) ||
             /* BUG-45370 Simple Foreignkey Child constraint trigger */
             ( sChildInfo->childTableRef->tableInfo->triggerCount > 0 ) )
        {
            sIsSimple = ID_FALSE;

            IDE_CONT( NORMAL_EXIT );
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    sDETE->isSimple = sIsSimple;

    return IDE_SUCCESS;
}

IDE_RC
qmoOneNonPlan::initDLAY( qcStatement  * aStatement ,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : DLAY ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncDLAY�� �Ҵ� �� �ʱ�ȭ
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncDLAY          * sDLAY;
    UInt                sDataNodeOffset;

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParent != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncDLAY�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncDLAY ),
                                               (void **)& sDLAY )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sDLAY ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_DLAY ,
                        qmnDLAY ,
                        qmndDLAY,
                        sDataNodeOffset );

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   & sDLAY->plan.resultDesc )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sDLAY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOneNonPlan::makeDLAY( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aChildPlan ,
                         qmnPlan      * aPlan )
{
    qmncDLAY          * sDLAY = (qmncDLAY *)aPlan;
    UInt                sDataNodeOffset;

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPlan != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    sDLAY->plan.left = aChildPlan;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndDLAY));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sDLAY->flag = QMN_PLAN_FLAG_CLEAR;
    sDLAY->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sDLAY->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    //dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aQuerySet,
                                            & sDLAY->plan,
                                            QMO_DLAY_DEPENDENCY,
                                            0,
                                            NULL,
                                            0,
                                            NULL,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    sDLAY->plan.mParallelDegree  = aChildPlan->mParallelDegree;
    sDLAY->plan.flag            &= ~QMN_PLAN_NODE_EXIST_MASK;
    sDLAY->plan.flag            |= (aChildPlan->flag & QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::initSDSE( qcStatement  * aStatement,
                                qmnPlan      * aParent,
                                qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : SDSE ��带 �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSDSE * sSDSE = NULL;
    UInt       sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initSDSE::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aParent    != NULL );
    IDE_FT_ASSERT( aPlan      != NULL );

    //-------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //-------------------------------------------------------------

    // qmncSDSE �� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmncSDSE ),
                                             (void**)& sSDSE )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSDSE,
                        QC_SHARED_TMPLATE(aStatement),
                        QMN_SDSE,
                        qmnSDSE,
                        qmndSDSE,
                        sDataNodeOffset );

    IDE_TEST( qmc::copyResultDesc( aStatement,
                                   aParent->resultDesc,
                                   & sSDSE->plan.resultDesc )
              != IDE_SUCCESS );

    // shard plan index
    sSDSE->shardDataIndex = QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount;
    QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount++;

    *aPlan = (qmnPlan *)sSDSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeSDSE( qcStatement       * aStatement,
                                qmnPlan           * aParent,
                                qcNamePosition    * aShardQuery,
                                sdiAnalyzeInfo    * aShardAnalysis,
                                qcShardParamInfo  * aShardParamInfo, /* TASK-7219 Non-shard DML */
                                UShort              aShardParamCount,
                                qmgGraph          * aGraph,
                                qmnPlan           * aPlan )
{
    mtcTemplate    * sTemplate = NULL;
    qmncSDSE       * sSDSE = (qmncSDSE*)aPlan;
    qtcNode        * sPredicate[4];
    qmncScanMethod   sMethod;
    idBool           sInSubQueryKeyRange = ID_FALSE;
    idBool           sScanLimit = ID_TRUE;
    UInt             sDataNodeOffset = 0;
    UShort           sTupleID = ID_USHORT_MAX;
    UInt             i = 0;
    mtcColumn      * sColumn      = NULL;
    UShort           sColumnCount = 0; /* TASK-7219 */

    UShort      sShardParamCount = 0;

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aParent    != NULL );
    IDE_FT_ASSERT( aGraph     != NULL );
    IDE_FT_ASSERT( aStatement->session != NULL );

    //----------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------

    sTemplate       = &QC_SHARED_TMPLATE(aStatement)->tmplate;
    aPlan->offset   = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset + ID_SIZEOF(qmndSDSE) );

    sSDSE->tableRef          = aGraph->myFrom->tableRef;
    sSDSE->mQueryPos         = aShardQuery;
    sSDSE->mShardAnalysis    = aShardAnalysis;
    sSDSE->mShardParamInfo   = aShardParamInfo; /* TASK-7219 Non-shard DML */
    sSDSE->mShardParamCount  = aShardParamCount;
    sSDSE->tupleRowID        = sTupleID = sSDSE->tableRef->table;

    if ( sSDSE->shardDataIndex != 0 )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialStmt = ID_TRUE;
    }

    if ( aStatement->calledByPSMFlag == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.globalPSM = ID_TRUE;
    }

    /* TASK-7219 Non-shard DML */
    if ( QCG_GET_SHARD_PARTIAL_EXEC_TYPE( aStatement ) == SDI_SHARD_PARTIAL_EXEC_TYPE_COORD )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialExecType = SDI_SHARD_PARTIAL_EXEC_TYPE_QUERY;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialExecType = SDI_SHARD_PARTIAL_EXEC_TYPE_NONE;
    }

    // shard exec data ����
    sSDSE->shardDataOffset = idlOS::align8( QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize );
    sSDSE->shardDataSize = 0;

    for ( i = 0; i < SDI_NODE_MAX_COUNT; i++ )
    {
        sSDSE->mBuffer[i] = sSDSE->shardDataOffset + sSDSE->shardDataSize;
        sSDSE->shardDataSize += idlOS::align8( sTemplate->rows[sTupleID].rowOffset );
    }

    /* TASK-7219 */
    for ( i = 0, sColumn = sTemplate->rows[ sSDSE->tableRef->table ].columns, sColumnCount = 0;
          i < sTemplate->rows[ sSDSE->tableRef->table ].columnCount;
          i++, sColumn++ )
    {
        if ( ( sColumn->flag & MTC_COLUMN_NULL_TYPE_MASK ) == MTC_COLUMN_NULL_TYPE_TRUE )
        {
            /* Nothing to do */
        }
        else
        {
            sColumnCount++;
        }
    }

    if ( sColumnCount == 0 )
    {
        sColumnCount = 1;
    }
    else
    {
        /* Nothing to do */
    }

    sSDSE->mOffset = sSDSE->shardDataOffset + sSDSE->shardDataSize;
    sSDSE->shardDataSize += idlOS::align8( ID_SIZEOF(UInt) * sColumnCount ); /* TASK-7219 */

    sSDSE->mMaxByteSize = sSDSE->shardDataOffset + sSDSE->shardDataSize;
    sSDSE->shardDataSize += idlOS::align8( ID_SIZEOF(UInt) * sColumnCount ); /* TASK-7219 */

    sSDSE->mBindParam = sSDSE->shardDataOffset + sSDSE->shardDataSize;
    sSDSE->shardDataSize += idlOS::align8( ID_SIZEOF(sdiBindParam) * aShardParamCount );

    /* PROJ-2728 */
    sSDSE->mOutBindParam = sSDSE->shardDataOffset + sSDSE->shardDataSize;
    sSDSE->shardDataSize += idlOS::align8( ID_SIZEOF(sdiOutBindParam) * aShardParamCount );

    sSDSE->mOutRefBindData = sSDSE->shardDataOffset + sSDSE->shardDataSize;
    sSDSE->shardDataSize += idlOS::align8( getOutRefBindDataSize( sTemplate,
                                                                  sSDSE ) );

    QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize =
        sSDSE->shardDataOffset + sSDSE->shardDataSize;

    // Flag ����
    sSDSE->flag      = QMN_PLAN_FLAG_CLEAR;
    sSDSE->plan.flag = QMN_PLAN_FLAG_CLEAR;

    // BUGBUG
    // SDSE ��ü�� disk �� �����ϳ�
    // SDSE �� ���� plan �� interResultType �� memory temp �� ����Ǿ�� �Ѵ�.
    sSDSE->plan.flag &= ~QMN_PLAN_STORAGE_MASK;
    sSDSE->plan.flag |=  QMN_PLAN_STORAGE_DISK;

    //PROJ-2402 Parallel Table Scan
    sSDSE->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sSDSE->plan.flag |=  QMN_PLAN_PRLQ_EXIST_FALSE;
    sSDSE->plan.flag |=  QMN_PLAN_MTR_EXIST_FALSE;

    // PROJ-1071 Parallel Query
    sSDSE->plan.mParallelDegree = 1;

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // mtcTuple.flag����
    //----------------------------------

    sTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_STORAGE_DISK;

    sTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_PLAN_TRUE;

    sTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_PLAN_MTR_FALSE;

    sTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_TYPE_MASK;
    sTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_TYPE_INTERMEDIATE;

    sTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
    sTemplate->rows[sTupleID].lflag |=  MTC_TUPLE_MATERIALIZE_VALUE;

    //----------------------------------
    // Predicate�� ó��
    //----------------------------------

    // Host ������ ������ Constant Expression�� ����ȭ
    if ( aGraph->nnfFilter != NULL )
    {
        IDE_TEST( qtc::optimizeHostConstExpression( aStatement,
                                                    aGraph->nnfFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmoOneNonPlan::processPredicate( aStatement,
                                               aGraph->myQuerySet,
                                               aGraph->myPredicate,
                                               aGraph->constantPredicate,
                                               NULL,
                                               NULL,
                                               sSDSE->tupleRowID,
                                               &( sMethod.constantFilter ),
                                               &( sMethod.filter ),
                                               &( sMethod.lobFilter ),
                                               &( sMethod.subqueryFilter ),
                                               &( sMethod.varKeyRange ),
                                               &( sMethod.varKeyFilter ),
                                               &( sMethod.varKeyRange4Filter ),
                                               &( sMethod.varKeyFilter4Filter ),
                                               &( sMethod.fixKeyRange ),
                                               &( sMethod.fixKeyFilter ),
                                               &( sMethod.fixKeyRange4Print ),
                                               &( sMethod.fixKeyFilter4Print ),
                                               &( sMethod.ridRange ),
                                               & sInSubQueryKeyRange )
              != IDE_SUCCESS );

    IDE_TEST( qmoOneNonPlan::postProcessScanMethod( aStatement,
                                                    & sMethod,
                                                    & sScanLimit )
                 != IDE_SUCCESS );

    IDE_FT_ASSERT( sInSubQueryKeyRange     == ID_FALSE );
    IDE_FT_ASSERT( sMethod.varKeyRange         == NULL );
    IDE_FT_ASSERT( sMethod.varKeyFilter        == NULL );
    IDE_FT_ASSERT( sMethod.varKeyRange4Filter  == NULL );
    IDE_FT_ASSERT( sMethod.varKeyFilter4Filter == NULL );
    IDE_FT_ASSERT( sMethod.fixKeyRange         == NULL );
    IDE_FT_ASSERT( sMethod.fixKeyFilter        == NULL );
    IDE_FT_ASSERT( sMethod.fixKeyRange4Print   == NULL );
    IDE_FT_ASSERT( sMethod.fixKeyFilter4Print  == NULL );
    IDE_FT_ASSERT( sMethod.ridRange            == NULL );

    sSDSE->constantFilter = sMethod.constantFilter;
    sSDSE->subqueryFilter = sMethod.subqueryFilter;
    sSDSE->filter         = sMethod.filter;
    sSDSE->lobFilter      = sMethod.lobFilter; // PROJ-2728, BUG-25916
    sSDSE->nnfFilter      = aGraph->nnfFilter;

    sPredicate[0] = sSDSE->constantFilter;
    sPredicate[1] = sSDSE->subqueryFilter;
    sPredicate[2] = sSDSE->nnfFilter;
    sPredicate[3] = sSDSE->filter;

    // PROJ-1437 dependency �������� predicate���� ��ġ������ ����.
    for ( i = 0; i <= 3; i++ )
    {
        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aGraph->myQuerySet,
                                           sPredicate[i],
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );

        for ( sShardParamCount = 0;
              sShardParamCount < sSDSE->mShardParamCount;
              sShardParamCount++ )
        {
            if ( sSDSE->mShardParamInfo[sShardParamCount].mIsOutRefColumnBind == ID_TRUE )
            {
                setOutRefBindReassign( sPredicate[i],
                                       &sSDSE->mShardParamInfo[sShardParamCount] );
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }

    //----------------------------------
    // dependency ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement,
                                            aGraph->myQuerySet,
                                            & sSDSE->plan,
                                            QMO_SDSE_DEPENDENCY,
                                            sSDSE->tupleRowID,
                                            NULL,
                                            4,
                                            sPredicate,
                                            0,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aGraph->myQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   & sSDSE->plan.resultDesc )
              != IDE_SUCCESS );

    //----------------------------------
    // PROJ-2462 Result Cache
    //----------------------------------

    qmo::addTupleID2ResultCacheStack( aStatement, sSDSE->tupleRowID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmoOneNonPlan::setOutRefBindReassign( qtcNode          * aFilter,
                                           qcShardParamInfo * aShardParamInfo )
{
    if ( aFilter != NULL )
    {
        if ( aFilter->node.module == &qtc::columnModule )
        {
            if ( ( aFilter->node.baseTable == aShardParamInfo->mOutRefTuple ) &&
                 ( aFilter->node.baseColumn == aShardParamInfo->mOffset ) )
            {
                aShardParamInfo->mOutRefTuple = aFilter->node.table;
                aShardParamInfo->mOffset  = aFilter->node.column;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* Nothing to do. */

        }

        setOutRefBindReassign( (qtcNode*)aFilter->node.next,
                               aShardParamInfo );

        setOutRefBindReassign( (qtcNode*)aFilter->node.arguments,
                               aShardParamInfo );
    }
    else
    {
        /* Nothing to do. */
    }
}

UInt qmoOneNonPlan::getOutRefBindDataSize( mtcTemplate * aTemplate,
                                           qmncSDSE    * aCodePlan )
{
    UInt sOutRefBindDataSize = 0;

    qcShardParamInfo * sBindParamInfo = NULL;

    UShort             i = 0;

    mtcTuple         * sTuple  = NULL;
    mtcColumn        * sColumn = NULL;

    for ( ;
          i < aCodePlan->mShardParamCount; i++ )
    {
        sBindParamInfo = aCodePlan->mShardParamInfo + i;

        if ( sBindParamInfo->mIsOutRefColumnBind == ID_TRUE )
        {
            sTuple = & aTemplate->rows[ sBindParamInfo->mOutRefTuple ];
            sColumn = sTuple->columns + sBindParamInfo->mOffset;

            sOutRefBindDataSize += sColumn->column.size;
        }
        else
        {
            /* Nothing to do. */
        }
    }

    return sOutRefBindDataSize;
}

IDE_RC qmoOneNonPlan::initSDEX( qcStatement  * aStatement,
                                qmnPlan     ** aPlan )
{
    qmncSDEX * sSDEX = NULL;
    UInt       sDataNodeOffset = 0;

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aPlan      != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncSDEX�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmncSDEX ),
                                             (void**)& sSDEX)
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSDEX,
                        QC_SHARED_TMPLATE(aStatement),
                        QMN_SDEX,
                        qmnSDEX,
                        qmndSDEX,
                        sDataNodeOffset );

    // shard plan index
    sSDEX->shardDataIndex = QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount;
    QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount++;

    *aPlan = (qmnPlan*)sSDEX;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeSDEX( qcStatement      * aStatement,
                                qcNamePosition   * aShardQuery,
                                sdiAnalyzeInfo   * aShardAnalysis,
                                qcShardParamInfo * aShardParamInfo, /* TASK-7219 Non-shard DML */
                                UShort             aShardParamCount,
                                qmnPlan          * aPlan )
{
    qmncSDEX * sSDEX = (qmncSDEX*)aPlan;
    UInt       sDataNodeOffset = 0;
    sdiShardAnalysis * sAnalysis       = NULL;

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement  != NULL );
    IDE_FT_ASSERT( aShardQuery != NULL );
    IDE_FT_ASSERT( aPlan       != NULL );

    //----------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8(aPlan->offset + ID_SIZEOF(qmndSDEX));

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    sSDEX->shardQuery.stmtText = aShardQuery->stmtText;
    sSDEX->shardQuery.offset   = aShardQuery->offset;
    sSDEX->shardQuery.size     = aShardQuery->size;

    sSDEX->shardAnalysis = aShardAnalysis;
    sSDEX->shardParamInfo = aShardParamInfo; /* TASK-7219 */
    sSDEX->shardParamCount = aShardParamCount;

    if ( sSDEX->shardDataIndex != 0 )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialStmt = ID_TRUE;
    }

    if ( aStatement->calledByPSMFlag == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.globalPSM = ID_TRUE;
    }

    /* TASK-7219 Non-shard DML */
    IDE_TEST( sdi::getParseTreeAnalysis( aStatement->myPlan->parseTree,
                                         &( sAnalysis ) )
              != IDE_SUCCESS );

    if ( sAnalysis->mAnalysisFlag.mTopQueryFlag[SDI_PARTIAL_COORD_EXEC_NEEDED] == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialExecType = SDI_SHARD_PARTIAL_EXEC_TYPE_COORD;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialExecType = SDI_SHARD_PARTIAL_EXEC_TYPE_NONE;
    }

    // shard exec data ����
    sSDEX->shardDataOffset = idlOS::align8( QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize );
    sSDEX->shardDataSize = 0;

    sSDEX->bindParam = sSDEX->shardDataOffset + sSDEX->shardDataSize;
    sSDEX->shardDataSize += idlOS::align8( ID_SIZEOF(sdiBindParam) * aShardParamCount );

    /* PROJ-2728 */
    sSDEX->outBindParam = sSDEX->shardDataOffset + sSDEX->shardDataSize;
    sSDEX->shardDataSize += idlOS::align8( ID_SIZEOF(sdiOutBindParam) * aShardParamCount );

    QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize =
        sSDEX->shardDataOffset + sSDEX->shardDataSize;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sSDEX->flag      = QMN_PLAN_FLAG_CLEAR;
    sSDEX->plan.flag = QMN_PLAN_FLAG_CLEAR;

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    qtc::dependencyClear( & sSDEX->plan.depInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::initSDIN( qcStatement   * aStatement ,
                                qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : Shard INST ��带 �����Ѵ�.
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSDIN�� �Ҵ� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmncSDIN          * sSDIN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::initSDIN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncSDIN ),
                                               (void **)& sSDIN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSDIN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_SDIN ,
                        qmnSDIN ,
                        qmndSDIN ,
                        sDataNodeOffset );

    // shard plan index
    sSDIN->shardDataIndex = QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount;
    QC_SHARED_TMPLATE(aStatement)->shardExecData.planCount++;

    *aPlan = (qmnPlan *)sSDIN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeSDIN( qcStatement    * aStatement ,
                                qmoINSTInfo    * aINSTInfo ,
                                qcNamePosition * aShardQuery ,
                                sdiAnalyzeInfo * aShardAnalysis ,
                                qmnPlan        * aChildPlan ,
                                qmnPlan        * aPlan )
{
/***********************************************************************
 *
 * Description : SDIN ��带 �����Ѵ�.
 *
 * Implementation :
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *
 ***********************************************************************/

    qmncSDIN          * sSDIN = (qmncSDIN *)aPlan;
    qcmTableInfo      * sTableForInsert;
    UInt                sDataNodeOffset;
    qmsFrom             sFrom;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoOneNonPlan::makeSDIN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aINSTInfo  != NULL );
    IDE_FT_ASSERT( aPlan      != NULL );

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndSDIN) );

    sSDIN->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sSDIN->flag = QMN_PLAN_FLAG_CLEAR;
    sSDIN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    if ( aChildPlan != NULL )
    {
        sSDIN->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);
    }
    else
    {
        // Nothing to do.
    }

    //Leaf Node�� tuple�� ���� memory ���� disk table������ ����
    //from tuple����
    IDE_TEST( setTableTypeFromTuple( aStatement ,
                                     aINSTInfo->tableRef->table ,
                                     &( sSDIN->plan.flag ) )
              != IDE_SUCCESS );

    //----------------------------------
    // Display ���� ����
    //----------------------------------

    // tableRef�� ������ ��
    sFrom.tableRef = aINSTInfo->tableRef;

    IDE_TEST( qmg::setDisplayInfo( & sFrom,
                                   &( sSDIN->tableOwnerName ),
                                   &( sSDIN->tableName ),
                                   &( sSDIN->aliasName ) )
              != IDE_SUCCESS );

    //----------------------------------------------------------------
    // ���� �۾�
    //----------------------------------------------------------------

    sTableForInsert = aINSTInfo->tableRef->tableInfo;

    //----------------------------------
    // insert ���� ����
    //----------------------------------

    // insert target ����
    sSDIN->tableRef = aINSTInfo->tableRef;

    // insert select ����
    if ( aChildPlan != NULL )
    {
        sSDIN->isInsertSelect = ID_TRUE;
    }
    else
    {
        sSDIN->isInsertSelect = ID_FALSE;
    }

    // insert columns ����
    sSDIN->columns          = aINSTInfo->columns;
    sSDIN->columnsForValues = aINSTInfo->columnsForValues;

    // insert values ����
    sSDIN->rows            = aINSTInfo->rows;
    sSDIN->valueIdx        = aINSTInfo->valueIdx;
    sSDIN->canonizedTuple  = aINSTInfo->canonizedTuple;
    sSDIN->queueMsgIDSeq   = aINSTInfo->queueMsgIDSeq;

    // sequence ����
    sSDIN->nextValSeqs = aINSTInfo->nextValSeqs;

    // shard query ����
    sSDIN->shardQuery.stmtText = aShardQuery->stmtText;
    sSDIN->shardQuery.offset   = aShardQuery->offset;
    sSDIN->shardQuery.size     = aShardQuery->size;

    // shard analysis ����
    sSDIN->shardAnalysis = aShardAnalysis;
    sSDIN->shardParamCount = 0;
    for ( i = 0; i < sTableForInsert->columnCount; i++ )
    {
        if ( (sTableForInsert->columns[i].flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
             == QCM_COLUMN_HIDDEN_COLUMN_FALSE )
        {
            sSDIN->shardParamCount++;
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sSDIN->shardDataIndex != 0 )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.partialStmt = ID_TRUE;
    }

    if ( aStatement->calledByPSMFlag == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->shardExecData.globalPSM = ID_TRUE;
    }

    // shard exec data ����
    sSDIN->shardDataOffset = idlOS::align8( QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize );
    sSDIN->shardDataSize = 0;

    sSDIN->bindParam = sSDIN->shardDataOffset + sSDIN->shardDataSize;
    sSDIN->shardDataSize += idlOS::align8( ID_SIZEOF(sdiBindParam) * sSDIN->shardParamCount );

    /* PROJ-2728 */
    sSDIN->outBindParam = sSDIN->shardDataOffset + sSDIN->shardDataSize;
    sSDIN->shardDataSize += idlOS::align8( ID_SIZEOF(sdiOutBindParam) * sSDIN->shardParamCount );

    QC_SHARED_TMPLATE(aStatement)->shardExecData.dataSize =
        sSDIN->shardDataOffset + sSDIN->shardDataSize;

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    // data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    qtc::dependencyClear( & sSDIN->plan.depInfo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::buildSerialFilterInfo( qcStatement          * aStatement,
                                             qmsHints             * aHints,
                                             qtcNode              * aFilter,
                                             UInt                 * aFilterSize,
                                             UInt                 * aFilterCount,
                                             mtxSerialFilterInfo ** aInfo )
{
/***********************************************************************
 *
 * Description :
 *  PROJ-2632
 *
 * Implementation :
 *
 ***********************************************************************/

    mtxSerialFilterInfo * sInfo       = NULL;
    mtxSerialFilterInfo * sHead       = NULL;
    UInt                  sFiterCount = 0;

    IDE_TEST_CONT( aFilter == NULL,
                   BUILD_ERROR );

    IDE_TEST_CONT( ( QC_SHARED_TMPLATE( aStatement )->flag & QC_TMP_DISABLE_SERIAL_FILTER_MASK )
                   == QC_TMP_DISABLE_SERIAL_FILTER_TRUE,
                   BUILD_ERROR );

    if ( aHints != NULL )
    {
        IDE_TEST_RAISE( aHints->mSerialFilter == QMS_SERIAL_FILTER_FALSE,
                        BUILD_ERROR );

        IDE_TEST_RAISE( ( ( aHints->mSerialFilter == QMS_SERIAL_FILTER_NONE ) &&
                          ( QCG_GET_SERIAL_EXECUTE_MODE( aStatement ) == 0 ) ),
                        BUILD_ERROR );
    }
    else
    {
        IDE_TEST_RAISE( QCG_GET_SERIAL_EXECUTE_MODE( aStatement ) == 0,
                        BUILD_ERROR );
    }

    IDE_TEST_RAISE( qtc::estimateSerializeFilter( aStatement,
                                                  &( aFilter->node ),
                                                  &( sFiterCount ) )
                     != IDE_SUCCESS, BUILD_ERROR );

    IDE_TEST_RAISE( sFiterCount <= 0,
                    BUILD_ERROR );

    IDE_TEST( qtc::allocateSerializeFilter( aStatement,
                                            sFiterCount,
                                            &( sInfo ) )
              != IDE_SUCCESS );

    sHead = sInfo;

    IDE_TEST_RAISE( qtc::recursiveSerializeFilter( aStatement,
                                                   &( aFilter->node ),
                                                   sInfo + sFiterCount,
                                                   &( sHead ) )
                   != IDE_SUCCESS, BUILD_ERROR );

    *aFilterSize  = sHead->mOffset + sHead->mHeader.mSize;
    *aFilterCount = sFiterCount;
    *aInfo        = sInfo;

    IDE_EXCEPTION_CONT( BUILD_ERROR );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::initMultiUPTE( qcStatement     * aStatement,
                                     qmsQuerySet     * aQuerySet,
                                     qmmMultiTables  * aTables,
                                     qmnPlan        ** aPlan )
{
    qmncUPTE        * sUPTE;
    UInt              sDataNodeOffset;
    qmmMultiTables  * sTmp;
    qtcNode         * sNode;

    IDU_FIT_POINT("qmoOneNonPlan::initMultiUPTE::alloc::sUPTE",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncUPTE ),
                                               (void **)& sUPTE )
        != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sUPTE ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_UPTE ,
                        qmnUPTE ,
                        qmndUPTE,
                        sDataNodeOffset );

    for ( sTmp = aTables; sTmp != NULL; sTmp = sTmp->mNext )
    {
        for ( sNode = sTmp->mColumnList;
              sNode != NULL;
              sNode = (qtcNode *)sNode->node.next )
        {
            IDE_TEST( qmc::appendPredicate( aStatement,
                                            aQuerySet,
                                            &sUPTE->plan.resultDesc,
                                            sNode )
                      != IDE_SUCCESS );
        }
    }

    *aPlan = (qmnPlan *)sUPTE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeMultiUPTE( qcStatement   * aStatement,
                                     qmsQuerySet   * aQuerySet,
                                     qmoUPTEInfo   * aUPTEInfo,
                                     qmnPlan       * aChildPlan,
                                     qmnPlan       * aPlan )
{
    qmncUPTE          * sUPTE = (qmncUPTE *)aPlan;
    UInt                sDataNodeOffset;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount = 0;
    qcmColumn         * sColumn;
    qmmMultiTables    * sTmp;
    UInt                i;

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndUPTE) );

    sUPTE->plan.left = aChildPlan;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sUPTE->flag = QMN_PLAN_FLAG_CLEAR;
    sUPTE->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sUPTE->plan.flag |= (aChildPlan->flag & QMN_PLAN_STORAGE_MASK);

    sUPTE->flag &= ~QMNC_UPTE_MULTIPLE_TABLE_MASK;
    sUPTE->flag |= QMNC_UPTE_MULTIPLE_TABLE_TRUE;

    // key preserved property
    if ( aQuerySet->SFWGH->preservedInfo != NULL )
    {
        if ( aQuerySet->SFWGH->preservedInfo->useKeyPreservedTable == ID_TRUE )
        {
            sUPTE->flag &= ~QMNC_UPTE_VIEW_KEY_PRESERVED_MASK;
            sUPTE->flag |= QMNC_UPTE_VIEW_KEY_PRESERVED_TRUE;
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

    //----------------------------------
    // update ���� ����
    //----------------------------------

    // update target table ����
    sUPTE->tableRef = aUPTEInfo->updateTableRef;
    // update column ���� ����
    sUPTE->columns           = aUPTEInfo->columns;
    sUPTE->updateColumnList  = aUPTEInfo->updateColumnList;
    sUPTE->updateColumnCount = aUPTEInfo->updateColumnCount;

    sUPTE->mTableList = aUPTEInfo->mTableList;

    for ( sTmp = sUPTE->mTableList, i = 0; sTmp != NULL; sTmp = sTmp->mNext )
    {
        i++;
    }
    sUPTE->mMultiTableCount = i;

    for ( sTmp = sUPTE->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        if ( sTmp->mColumnCount > 0 )
        {
            IDU_FIT_POINT("qmoOneNonPlan::makeMultiUPTE::alloc::mColumnIDs",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sTmp->mColumnCount * ID_SIZEOF(UInt),
                                                       (void**) &sTmp->mColumnIDs )
                      != IDE_SUCCESS );
            for ( i = 0, sColumn = sTmp->mColumns; i < sTmp->mColumnCount; i++, sColumn = sColumn->next )
            {
                sTmp->mColumnIDs[i] = sColumn->basicInfo->column.id;
            }
        }
        else
        {
            sTmp->mColumnIDs = NULL;
        }

        // partition�� update column list ����
        if ( sTmp->mTableRef->partitionRef != NULL )
        {
            sPartitionCount = 0;
            for ( sPartitionRef  = sTmp->mTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef  = sPartitionRef->next )
            {
                sPartitionCount++;
            }

            IDU_FIT_POINT("qmoOneNonPlan::makeMultiUPTE::alloc::mPartColumnList",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                       (void **) &( sTmp->mPartColumnList ) )
                      != IDE_SUCCESS );

            for ( sPartitionRef = sTmp->mTableRef->partitionRef, i = 0;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next, i++ )
            {
                IDE_TEST( qmoPartition::makePartUpdateColumnList( aStatement,
                                                                  sPartitionRef,
                                                                  sTmp->mUptColumnList,
                                                                  &( sTmp->mPartColumnList[i] ) )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            sTmp->mPartColumnList = NULL;
        }

        //----------------------------------
        // constraint ����
        //----------------------------------

        // Foreign Key Referencing�� ���� Master Table�� �����ϴ� �� �˻�
        if ( qdnForeignKey::haveToCheckParent( sTmp->mTableRef->tableInfo,
                                               sTmp->mColumnIDs,
                                               sTmp->mColumnCount )
             == ID_TRUE )
        {
            /* Nothing to do */
        }
        else
        {
            sTmp->mParentConst = NULL;
        }

        // Child Table�� Referencing ���� �˻�
        if ( qdnForeignKey::haveToOpenBeforeCursor( sTmp->mChildConst,
                                                    sTmp->mColumnIDs,
                                                    sTmp->mColumnCount )
             == ID_TRUE )
        {
            /* Nothing to do */
        }
        else
        {
            sTmp->mChildConst = NULL;
        }

        //----------------------------------
        // PROJ-1784 DML Without Retry
        //----------------------------------
        IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                        sTmp->mTableRef->table,
                                                        &sTmp->mWhereColumnList )
                  != IDE_SUCCESS );
        IDE_TEST( qdbCommon::makeSetClauseColumnList( aStatement,
                                                      sTmp->mTableRef->table,
                                                      &sTmp->mSetColumnList )
                  != IDE_SUCCESS );

        // partition�� where column list ����
        if ( sTmp->mTableRef->partitionRef != NULL )
        {
            IDU_FIT_POINT("qmoOneNonPlan::makeMultiUPTE::alloc::mWherePartColumnList",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                       (void **) &( sTmp->mWherePartColumnList ) )
                      != IDE_SUCCESS );
            IDU_FIT_POINT("qmoOneNonPlan::makeMultiUPTE::alloc::mSetPartColumnList",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                       (void **) &( sTmp->mSetPartColumnList ) )
                      != IDE_SUCCESS );

            for ( sPartitionRef  = sTmp->mTableRef->partitionRef, i = 0;
                  sPartitionRef != NULL;
                  sPartitionRef  = sPartitionRef->next, i++ )
            {
                /* PROJ-2464 hybrid partitioned table ���� */
                IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                                sPartitionRef->table,
                                                                &( sTmp->mWherePartColumnList[i] ) )
                          != IDE_SUCCESS );

                IDE_TEST( qdbCommon::makeSetClauseColumnList( aStatement,
                                                              sPartitionRef->table,
                                                              &( sTmp->mSetPartColumnList[i] ) )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            sTmp->mWherePartColumnList = NULL;
            sTmp->mSetPartColumnList = NULL;
        }
    }

    // update value ���� ����
    sUPTE->values         = aUPTEInfo->values;
    sUPTE->subqueries     = aUPTEInfo->subqueries;
    sUPTE->valueIdx       = aUPTEInfo->valueIdx;
    sUPTE->canonizedTuple = aUPTEInfo->canonizedTuple;
    sUPTE->compressedTuple= aUPTEInfo->compressedTuple;
    sUPTE->isNull         = aUPTEInfo->isNull;

    // sequence ����
    sUPTE->nextValSeqs    = aUPTEInfo->nextValSeqs;

    // instead of trigger ����
    sUPTE->insteadOfTrigger = aUPTEInfo->insteadOfTrigger;
    sUPTE->checkConstrList  = aUPTEInfo->checkConstrList;
    sUPTE->insertTableRef   = aUPTEInfo->insertTableRef;
    sUPTE->isRowMovementUpdate = aUPTEInfo->isRowMovementUpdate;
    // update type
    sUPTE->updateType = aUPTEInfo->updateType;
    sUPTE->cursorType    = aUPTEInfo->cursorType;
    sUPTE->inplaceUpdate = aUPTEInfo->inplaceUpdate;

    if ( aQuerySet->SFWGH->hints != NULL )
    {
        if( aQuerySet->SFWGH->hints->withoutRetry == ID_TRUE )
        {
            sUPTE->withoutRetry = ID_TRUE ;
        }
        else
        {
            sUPTE->withoutRetry = ( QCU_DML_WITHOUT_RETRY_ENABLE == 1 ) ? ID_TRUE : ID_FALSE ;
        }
    }
    else
    {
        sUPTE->withoutRetry = ID_FALSE;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sUPTE->plan ,
                                            QMO_UPTE_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    sUPTE->isSimple = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::initMultiDETE( qcStatement        * aStatement,
                                     qmsQuerySet        * aQuerySet,
                                     qmmDelMultiTables  * aTables,
                                     qmnPlan           ** aPlan )
{
    qmncDETE           * sDETE;
    UInt                 sDataNodeOffset;
    qmmDelMultiTables  * sTmp;
    qtcNode            * sNode;

    //qmncSCAN�� �Ҵ�� �ʱ�ȭ
    IDU_FIT_POINT("qmoOneNonPlan::makeMultiDETE::alloc::sDETE",
                  idERR_ABORT_InsufficientMemory);
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmncDETE ),
                                               (void **)& sDETE )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sDETE ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_DETE ,
                        qmnDETE ,
                        qmndDETE,
                        sDataNodeOffset );

    for ( sTmp = aTables; sTmp != NULL; sTmp = sTmp->mNext )
    {
        for ( sNode = sTmp->mColumnList;
              sNode != NULL;
              sNode = (qtcNode *)sNode->node.next )
        {
            IDE_TEST( qmc::appendPredicate( aStatement,
                                            aQuerySet,
                                            &sDETE->plan.resultDesc,
                                            sNode )
                      != IDE_SUCCESS );
        }
    }

    *aPlan = (qmnPlan *)sDETE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOneNonPlan::makeMultiDETE( qcStatement * aStatement,
                                     qmsQuerySet * aQuerySet,
                                     qmoDETEInfo * aDETEInfo,
                                     qmnPlan     * aChildPlan,
                                     qmnPlan     * aPlan )
{
    qmncDETE          * sDETE = (qmncDETE *)aPlan;
    UInt                sDataNodeOffset;
    qmsPartitionRef   * sPartitionRef;
    UInt                sPartitionCount;
    UInt                i;
    qmmDelMultiTables * sTmp;

    //----------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //----------------------------------------------------------------
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset = idlOS::align8( aPlan->offset +
                                     ID_SIZEOF(qmndDETE) );

    sDETE->plan.left = aChildPlan;

    sDETE->flag &= QMNC_DETE_MULTIPLE_TABLE_MASK;
    sDETE->flag |= QMNC_DETE_MULTIPLE_TABLE_TRUE;

    // key preserved property
    if ( aQuerySet->SFWGH->preservedInfo != NULL )
    {
        if ( aQuerySet->SFWGH->preservedInfo->useKeyPreservedTable == ID_TRUE )
        {
            sDETE->flag &= ~QMNC_DETE_VIEW_KEY_PRESERVED_MASK;
            sDETE->flag |= QMNC_DETE_VIEW_KEY_PRESERVED_TRUE;
        }
    }

    sDETE->mTableList = aDETEInfo->mTableList;
    sDETE->tableRef = NULL;
    sDETE->insteadOfTrigger = ID_FALSE;
    sDETE->limit = NULL;
    sDETE->childConstraints = aDETEInfo->childConstraints;
    sDETE->returnInto = NULL;
    sDETE->isSimple = ID_FALSE;

    for ( sTmp = sDETE->mTableList, i = 0; sTmp != NULL; sTmp = sTmp->mNext )
    {
        i++;
    }
    sDETE->mMultiTableCount = i;

    for ( sTmp = sDETE->mTableList;
          sTmp != NULL;
          sTmp = sTmp->mNext )
    {
        IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                        sTmp->mTableRef->table,
                                                        &sTmp->mWhereColumnList )
                  != IDE_SUCCESS );

        if ( sTmp->mTableRef->partitionRef != NULL )
        {
            sPartitionCount = 0;
            for ( sPartitionRef  = sTmp->mTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef  = sPartitionRef->next )
            {
                sPartitionCount++;
            }
            IDU_FIT_POINT("qmoOneNonPlan::makeMultiDETE::alloc::mWherePartColumnList",
                          idERR_ABORT_InsufficientMemory);
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sPartitionCount * ID_SIZEOF( smiColumnList* ),
                                                       (void **) &( sTmp->mWherePartColumnList ) )
                      != IDE_SUCCESS );
            for ( sPartitionRef  = sTmp->mTableRef->partitionRef, i = 0;
                  sPartitionRef != NULL;
                  sPartitionRef  = sPartitionRef->next, i++ )
            {
                /* PROJ-2464 hybrid partitioned table ���� */
                IDE_TEST( qdbCommon::makeWhereClauseColumnList( aStatement,
                                                                sPartitionRef->table,
                                                                &( sTmp->mWherePartColumnList[i] ) )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( aQuerySet->SFWGH->hints != NULL )
    {
        if( aQuerySet->SFWGH->hints->withoutRetry == ID_TRUE )
        {
            sDETE->withoutRetry = ID_TRUE ;
        }
        else
        {
            sDETE->withoutRetry = ( QCU_DML_WITHOUT_RETRY_ENABLE == 1 ) ? ID_TRUE : ID_FALSE ;
        }
    }
    else
    {
        sDETE->withoutRetry = ID_FALSE;
    }

    //----------------------------------------------------------------
    // ������ �۾�
    //----------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sDETE->plan ,
                                            QMO_DETE_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    sDETE->isSimple = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

