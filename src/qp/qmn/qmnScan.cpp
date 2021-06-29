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
 * $Id: qmnScan.cpp 90270 2021-03-21 23:20:18Z bethy $
 *
 * Description :
 *     SCAN Plan Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qtc.h>
#include <qmoKeyRange.h>
#include <qmoUtil.h>
#include <qmx.h>
#include <qmnScan.h>
#include <qmo.h>
#include <qdbCommon.h>
#include <qcmPartition.h>
#include <qcuTemporaryObj.h>
#include <qcsModule.h>

extern mtfModule mtfOr;
extern mtfModule mtfEqual;
extern mtfModule mtfEqualAny;
extern mtfModule mtfList;
extern mtdModule mtdBigint;

IDE_RC
qmnSCAN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SCAN ����� �ʱ�ȭ
 *
 * Implementation :
 *    - ���� �ʱ�ȭ�� ���� ���� ��� ���� �ʱ�ȭ ����
 *    - Constant Filter�� ���� ��� �˻�
 *    - Constant Filter�� ����� ���� ���� �Լ� ����
 *
 ***********************************************************************/

#define IDE_FN "qmnSCAN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    idBool sJudge;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnSCAN::doItDefault;
    sDataPlan->plan.mTID = QMN_PLAN_INIT_THREAD_ID;

    //------------------------------------------------
    // ���� �ʱ�ȭ ���� ���� �Ǵ�
    //------------------------------------------------

    if ( (*sDataPlan->flag & QMND_SCAN_INIT_DONE_MASK)
         == QMND_SCAN_INIT_DONE_FALSE )
    {
        // ���� �ʱ�ȭ ����
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Constant Filter�� ����
    //------------------------------------------------

    /* TASK-7307 DML Data Consistency in Shard */
    if ( ( QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) == ID_TRUE ) &&
         ( sCodePlan->tableRef->tableInfo->mIsUsable == ID_FALSE ) )
    {
        sJudge = ID_FALSE;
    }
    else if ( sCodePlan->method.constantFilter != NULL )
    {
        IDE_TEST( qtc::judge( & sJudge,
                              sCodePlan->method.constantFilter,
                              aTemplate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJudge = ID_TRUE;
    }

    //------------------------------------------------
    // Constant Filter�� ���� ���� �Լ� ����
    //------------------------------------------------

    if ( sJudge == ID_TRUE )
    {
        //---------------------------------
        // Constant Filter�� �����ϴ� ���
        //---------------------------------

        if ( ( sCodePlan->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
             == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_FALSE )
        {
            // ���� �Լ� ����
            sDataPlan->doIt = qmnSCAN::doItFirst;
        }
        else
        {
            sDataPlan->doIt = qmnSCAN::doItFirstFixedTable;
        }

        // Update, Delete�� ���� Flag ����
        *sDataPlan->flag &= ~QMND_SCAN_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_SCAN_ALL_FALSE_FALSE;
    }
    else
    {
        //-------------------------------------------
        // Constant Filter�� �������� �ʴ� ���
        // - �׻� ������ �������� �����Ƿ�
        //   ��� ����� �������� �ʴ� �Լ��� �����Ѵ�.
        //-------------------------------------------

        // ���� �Լ� ����
        sDataPlan->doIt = qmnSCAN::doItAllFalse;

        // Update, Delete�� ���� Flag ����
        *sDataPlan->flag &= ~QMND_SCAN_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_SCAN_ALL_FALSE_TRUE;
    }

    //------------------------------------------------
    // ���� Data Flag �� �ʱ�ȭ
    //------------------------------------------------

    // Subquery���ο� ���ԵǾ� ���� ��� �ʱ�ȭ�Ͽ�
    // In Subquery Key Range�� �ٽ� ������ �� �ֵ��� �Ѵ�.
    *sDataPlan->flag &= ~QMND_SCAN_INSUBQ_RANGE_BUILD_MASK;
    *sDataPlan->flag |= QMND_SCAN_INSUBQ_RANGE_BUILD_SUCCESS;

    // BUG-43721 IN-SUBQUERY KEY RANGE�� �ʱ�ȭ�� �ȵǾ ����� Ʋ���ϴ�.
    if ( ( sCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
         == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE )
    {
        resetExecInfo4Subquery( aTemplate, (qmnPlan*)sCodePlan );
    }
    else
    {
        // nothing to do.
    }

    if ( ( sCodePlan->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
         == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_FALSE )
    {
        //------------------------------------------------
        // PR-24281
        // select for update�� ��� init�ÿ� cursor�� �̸�
        // ���� record lock�� ȹ��
        //------------------------------------------------

        if ( ( sDataPlan->lockMode == SMI_LOCK_REPEATABLE ) &&
             ( sJudge == ID_TRUE ) )
        {
            /* BUG-41110 */
            IDE_TEST(makeRidRange(aTemplate, sCodePlan, sDataPlan)
                     != IDE_SUCCESS);

            // KeyRange, KeyFilter, Filter ����
            IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                             sCodePlan,
                                             sDataPlan ) != IDE_SUCCESS );

            // Cursor�� ����
            if ( ( *sDataPlan->flag & QMND_SCAN_CURSOR_MASK )
                 == QMND_SCAN_CURSOR_OPEN )
            {
                // �̹� ���� �ִ� ���
                IDE_TEST( restartCursor( sCodePlan, sDataPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                // ó�� ���� ���
                IDE_TEST( openCursor( aTemplate, sCodePlan, sDataPlan )
                          != IDE_SUCCESS );
            }

            // BUG-28533 SELECT FOR UPDATE�� ��� cursor�� �̸� ����ξ����Ƿ�
            // doItFirst�� ó�� ���� �� cursor�� restart �� �ʿ� ����
            *sDataPlan->flag &= ~QMND_SCAN_RESTART_CURSOR_MASK;
            *sDataPlan->flag |= QMND_SCAN_RESTART_CURSOR_FALSE;
        }
        else
        {
            *sDataPlan->flag &= ~QMND_SCAN_RESTART_CURSOR_MASK;
            *sDataPlan->flag |= QMND_SCAN_RESTART_CURSOR_TRUE;
        }
    }
    else
    {
        /* Nothing to do */
    }

    // PROJ-1071
    // addAccessCount ��������
    sDataPlan->mOrgModifyCnt      = sDataPlan->plan.myTuple->modify;
    sDataPlan->mOrgSubQFilterDepCnt = sDataPlan->subQFilterDepCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnSCAN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SCAN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

    qmndSCAN * sDataPlan = (qmndSCAN*)(aTemplate->tmplate.data + aPlan->offset);
    UInt       sAddAccessCnt;

    IDE_TEST(sDataPlan->doIt( aTemplate, aPlan, aFlag ) != IDE_SUCCESS);

    if ((*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_NONE )
    {
        sAddAccessCnt = ((sDataPlan->plan.myTuple->modify -
                          sDataPlan->mOrgModifyCnt) -
                         (sDataPlan->subQFilterDepCnt -
                          sDataPlan->mOrgSubQFilterDepCnt));

        // add access count
        (void)qmnSCAN::addAccessCount( (qmncSCAN*)aPlan,
                                       aTemplate,
                                       sAddAccessCnt);


        sDataPlan->mAccessCnt4Parallel += sAddAccessCnt;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnSCAN::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SCAN �� �ش��ϴ� Null Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *    Disk Table�� ���
 *        - Null Row�� ���ٸ�, Null Row���� �Ҵ� �� ȹ��
 *        - Null Row�� �ִٸ�, ����
 *    Memory Temp Table�� ���
 *        - Null Rowȹ��
 *
 *    Modify ����
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    // �ʱ�ȭ�� ������� ���� ��� �ʱ�ȭ�� ����
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_SCAN_INIT_DONE_MASK)
         == QMND_SCAN_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_DISK )
    {
        //-----------------------------------
        // Disk Table�� ���
        //-----------------------------------

        // Record ������ ���� ������ �ϳ��� �����ϸ�,
        // �̿� ���� pointer�� �׻� �����Ǿ�� �Ѵ�.

        if ( sDataPlan->nullRow == NULL )
        {
            //-----------------------------------
            // Null Row�� ������ ���� ���� ���
            //-----------------------------------

            // ���ռ� �˻�
            IDE_DASSERT( sDataPlan->plan.myTuple->rowOffset > 0 );

            IDU_FIT_POINT( "qmnSCAN::padNull::cralloc::nullRow",
                            idERR_ABORT_InsufficientMemory );

            // Null Row�� ���� ���� �Ҵ�
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                    sDataPlan->plan.myTuple->rowOffset,
                    (void**) & sDataPlan->nullRow ) != IDE_SUCCESS);

            // PROJ-1705
            // ��ũ���̺��� null row�� qp���� ����/�����صΰ� ����Ѵ�.
            IDE_TEST( qmn::makeNullRow( sDataPlan->plan.myTuple,
                                        sDataPlan->nullRow )
                      != IDE_SUCCESS );

            SMI_MAKE_VIRTUAL_NULL_GRID( sDataPlan->nullRID );
        }
        else
        {
            // �̹� Null Row�� ��������.
            // Nothing To Do
        }

        // Null Row ����
        idlOS::memcpy( sDataPlan->plan.myTuple->row,
                       sDataPlan->nullRow,
                       sDataPlan->plan.myTuple->rowOffset );

        // Null RID�� ����
        idlOS::memcpy( & sDataPlan->plan.myTuple->rid,
                       & sDataPlan->nullRID,
                       ID_SIZEOF(scGRID) );
    }
    else
    {
        if ( ( ( sCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
               QMNC_SCAN_REMOTE_TABLE_TRUE ) ||
             ( ( sCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_STORE_MASK ) ==
               QMNC_SCAN_REMOTE_TABLE_STORE_TRUE ) )
        {
            IDE_TEST( sDataPlan->cursor->getTableNullRow( (void **)&(sDataPlan->plan.myTuple->row),
                                                          &(sDataPlan->plan.myTuple->rid) )
                      != IDE_SUCCESS );
        }
        else
        {
            //-----------------------------------
            // Memory Table�� ���
            //-----------------------------------
            
            // NULL ROW�� �����´�.
            IDE_TEST( smiGetTableNullRow( sCodePlan->table,
                                          (void **) & sDataPlan->plan.myTuple->row,
                                          & sDataPlan->plan.myTuple->rid )
                      != IDE_SUCCESS );
        }
    }

    // Null Padding�� record�� ���� ����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * -----------------------------------------------------------------------------
 * PROJ-2402 Parallel Table Scan
 *
 * doItFirst ���� �ϴ� ������ ���ݺ� (cursor open) �� �����Ѵ�.
 * �� �κб����� serial �� ����Ǿ�� �ϰ�,
 * ���� read row �κ��� parallel �ϰ� �����Ѵ�.
 * -----------------------------------------------------------------------------
 */
IDE_RC qmnSCAN::readyIt( qcTemplate * aTemplate,
                         qmnPlan    * aPlan,
                         UInt         aTID )
{
    qmncSCAN* sCodePlan = (qmncSCAN*)aPlan;
    qmndSCAN* sDataPlan = (qmndSCAN*)(aTemplate->tmplate.data + aPlan->offset);

    UInt       sModifyCnt;

    // ----------------
    // TID ����
    // ----------------
    sDataPlan->plan.mTID = aTID;

    // ���� ACCESS count
    sModifyCnt = sDataPlan->plan.myTuple->modify;

    // ----------------
    // Tuple ��ġ�� ����
    // ----------------
    sDataPlan->plan.myTuple = &aTemplate->tmplate.rows[sCodePlan->tupleRowID];

    // ACCESS count ����
    sDataPlan->plan.myTuple->modify = sModifyCnt;

    // --------------------------------
    // PROJ-2444
    // parallel aggr �϶��� SCAN �� row �� PRLQ ���� ������ ���� �ʴ´�.
    // ���� ���ο� row ���۸� �Ҵ� �޾ƾ� �Ѵ�.
    // --------------------------------
    if ( sDataPlan->plan.mTID != QMN_PLAN_INIT_THREAD_ID )
    {

        IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                                   &aTemplate->tmplate,
                                   sCodePlan->tupleRowID ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ((*sDataPlan->flag & QMND_SCAN_ALL_FALSE_MASK) ==
        QMND_SCAN_ALL_FALSE_FALSE)
    {
        // ������ ���� �˻�
        IDE_TEST(iduCheckSessionEvent(aTemplate->stmt->mStatistics)
                 != IDE_SUCCESS);

        // cursor�� �̹� open�Ǿ� �ְ� doItFirst�� ó�� ȣ��ǹǷ� restart �ʿ� ����
        if ((*sDataPlan->flag & QMND_SCAN_RESTART_CURSOR_MASK) ==
            QMND_SCAN_RESTART_CURSOR_FALSE)
        {
            // Nothing to do.
        }
        else
        {
            IDE_TEST(makeRidRange(aTemplate, sCodePlan, sDataPlan)
                     != IDE_SUCCESS);

            // KeyRange, KeyFilter, Filter ����
            IDE_TEST(makeKeyRangeAndFilter(aTemplate, sCodePlan, sDataPlan)
                     != IDE_SUCCESS);

            // Cursor�� ����
            if ((*sDataPlan->flag & QMND_SCAN_CURSOR_MASK) ==
                QMND_SCAN_CURSOR_OPEN)
            {
                // �̹� ���� �ִ� ���
                IDE_TEST(restartCursor(sCodePlan, sDataPlan)
                         != IDE_SUCCESS);
            }
            else
            {
                // ó�� ���� ���
                IDE_TEST(openCursor(aTemplate, sCodePlan, sDataPlan)
                         != IDE_SUCCESS);
            }
        }

        // ���� �� doItFirst ���� �ÿ��� cursor�� restart
        *sDataPlan->flag &= ~QMND_SCAN_RESTART_CURSOR_MASK;
        *sDataPlan->flag |= QMND_SCAN_RESTART_CURSOR_TRUE;

        sDataPlan->doIt = qmnSCAN::doItNext;
    }
    else
    {
        sDataPlan->doIt = qmnSCAN::doItAllFalse;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC qmnSCAN::printPlan( qcTemplate   * aTemplate,
                           qmnPlan      * aPlan,
                           ULong          aDepth,
                           iduVarString * aString,
                           qmnDisplay     aMode )
{
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, sCodePlan );

    if ( ( ( sCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
           QMNC_SCAN_REMOTE_TABLE_TRUE ) ||
        ( ( sCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_STORE_MASK ) ==
          QMNC_SCAN_REMOTE_TABLE_STORE_TRUE ) )
    {
        IDE_TEST( printRemotePlan( aPlan, aDepth, aString )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( printLocalPlan( aTemplate, aPlan, aDepth, aString, aMode )
                  != IDE_SUCCESS );
    }

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        IDE_TEST( printPredicateInfo( aTemplate,
                                      sCodePlan,
                                      aDepth,
                                      aString,
                                      aMode )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //----------------------------
    // Subquery ������ ���
    // Subquery�� ������ ���� predicate���� ������ �� �ִ�.
    //     1. Variable Key Range
    //     2. Variable Key Filter
    //     3. Constant Filter
    //     4. Subquery Filter
    //----------------------------

    // Variable Key Range�� Subquery ���� ���
    if ( sMethod->varKeyRange != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sMethod->varKeyRange,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // Variable Key Filter�� Subquery ���� ���
    if ( sMethod->varKeyFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sMethod->varKeyFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // Constant Filter�� Subquery ���� ���
    if ( sMethod->constantFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sMethod->constantFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // Subquery Filter�� Subquery ���� ���
    if ( sMethod->subqueryFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sMethod->subqueryFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // NNF Filter�� Subquery ���� ���
    if ( sCodePlan->nnfFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->nnfFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Operator�� ��� ���� ���
    //----------------------------
    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    sCodePlan->plan.resultDesc )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * PROJ-1832 New database link
 */
IDE_RC qmnSCAN::printRemotePlan( qmnPlan      * aPlan,
                                 ULong          aDepth,
                                 iduVarString * aString )
{
    UInt i = 0;
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString, " " );
    }

    if ( ( sCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
         QMNC_SCAN_REMOTE_TABLE_TRUE )
    {
        iduVarStringAppend( aString, "SCAN ( REMOTE_TABLE ( " );
    }
    else
    {
        iduVarStringAppend( aString, "SCAN ( REMOTE_TABLE_STORE ( " );
    }

    iduVarStringAppend( aString, sCodePlan->databaseLinkName );
    iduVarStringAppend( aString, ", " );

    iduVarStringAppend( aString, sCodePlan->remoteQuery );
    iduVarStringAppend( aString, " )" );

    iduVarStringAppendFormat( aString, ", SELF_ID: %"ID_INT32_FMT" )",
                              (SInt)sCodePlan->tupleRowID );

    iduVarStringAppend( aString, "\n" );

    return IDE_SUCCESS;
}

IDE_RC
qmnSCAN::printLocalPlan( qcTemplate   * aTemplate,
                         qmnPlan      * aPlan,
                         ULong          aDepth,
                         iduVarString * aString,
                         qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    SCAN ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, sCodePlan );

    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    qcmIndex * sIndex;
    qcmIndex * sOrgIndex;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    qmn::printSpaceDepth(aString, aDepth);

    if( ( sCodePlan->flag & QMNC_SCAN_FOR_PARTITION_MASK )
        == QMNC_SCAN_FOR_PARTITION_TRUE )
    {
        //----------------------------
        // Table Partition Name ���
        //----------------------------

        iduVarStringAppend( aString,
                            "SCAN ( PARTITION: " );

        /* BUG-44520 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
         *           Partition Name �κп��� ������ ������ �� �ֽ��ϴ�.
         *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
         *  SCAN Node���� Partition Name�� �����ϵ��� �����մϴ�.
         */
        iduVarStringAppend( aString,
                            sCodePlan->partitionName );
    }
    else
    {
        //----------------------------
        // Table Owner Name ���
        //----------------------------

        iduVarStringAppend( aString,
                            "SCAN ( TABLE: " );

        if ( ( sCodePlan->tableOwnerName.name != NULL ) &&
             ( sCodePlan->tableOwnerName.size > 0 ) )
        {
            iduVarStringAppendLength( aString,
                                      sCodePlan->tableOwnerName.name,
                                      sCodePlan->tableOwnerName.size );
            iduVarStringAppend( aString, "." );
        }
        else
        {
            // Nothing to do.
        }

        //----------------------------
        // Table Name ���
        //----------------------------

        if ( ( sCodePlan->tableName.size <= QC_MAX_OBJECT_NAME_LEN ) &&
             ( sCodePlan->tableName.name != NULL ) &&
             ( sCodePlan->tableName.size > 0 ) )
        {
            iduVarStringAppendLength( aString,
                                      sCodePlan->tableName.name,
                                      sCodePlan->tableName.size );
        }
        else
        {
            // Nothing to do.
        }

        //----------------------------
        // Alias Name ���
        //----------------------------

        if ( sCodePlan->aliasName.name != NULL &&
             sCodePlan->aliasName.size > 0  &&
             sCodePlan->aliasName.name != sCodePlan->tableName.name )
        {
            // Table �̸� ������ Alias �̸� ������ �ٸ� ���
            // (alias name)
            iduVarStringAppend( aString,
                                " " );

            if ( sCodePlan->aliasName.size <= QC_MAX_OBJECT_NAME_LEN )
            {
                iduVarStringAppendLength( aString,
                                          sCodePlan->aliasName.name,
                                          sCodePlan->aliasName.size );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Alias �̸� ������ ���ų� Table �̸� ������ ������ ���
            // Nothing To Do
        }
    }

    //----------------------------
    // Access Method ���
    //----------------------------
    sIndex = sMethod->index;

    if ( sIndex != NULL )
    {
        // Index�� ���� ���
        iduVarStringAppend( aString,
                            ", INDEX: " );

        // PROJ-1502 PARTITIONED DISK TABLE
        // local index partition�� �̸���
        // partitioned index�� �̸����� ����ϱ� ����.
        if ( ( sCodePlan->flag & QMNC_SCAN_FOR_PARTITION_MASK )
             == QMNC_SCAN_FOR_PARTITION_TRUE )
        {
            /* BUG-44633 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
             *           Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
             *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
             *  SCAN Node���� Index ID�� �����ϵ��� �����մϴ�.
             */
            IDE_TEST( qcmPartition::getPartIdxFromIdxId(
                          sCodePlan->partitionIndexId,
                          sCodePlan->tableRef,
                          &sOrgIndex )
                      != IDE_SUCCESS );

            iduVarStringAppend( aString,
                                sOrgIndex->userName );
            iduVarStringAppend( aString,
                                "." );
            iduVarStringAppend( aString,
                                sOrgIndex->name );
        }
        else
        {
            iduVarStringAppend( aString,
                                sIndex->userName );
            iduVarStringAppend( aString,
                                "." );
            iduVarStringAppend( aString,
                                sIndex->name );
        }

        // PROJ-2242 print full scan or range scan
        if ( (*sDataPlan->flag & QMND_SCAN_INIT_DONE_MASK)
             == QMND_SCAN_INIT_DONE_TRUE )
        {
            // IN SUBQUERY KEYRANGE�� �ִ� ���:
            // keyrange����� �����ϸ� fetch����
            // ������ range scan �� �Ѵ�.
            if ( ( sCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
                 == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE )
            {
                (void) iduVarStringAppend( aString, ", RANGE SCAN" );
            }
            else
            {
                if ( sDataPlan->keyRange == smiGetDefaultKeyRange() )
                {
                    (void) iduVarStringAppend( aString, ", FULL SCAN" );
                }
                else
                {
                    (void) iduVarStringAppend( aString, ", RANGE SCAN" );
                }
            }
        }
        else
        {
            if ( ( sCodePlan->method.fixKeyRange == NULL ) &&
                 ( sCodePlan->method.varKeyRange == NULL ) )
            {
                iduVarStringAppend( aString, ", FULL SCAN" );
            }
            else
            {
                iduVarStringAppend( aString, ", RANGE SCAN" );
            }
        }

        // PROJ-2242 print index asc, desc
        if ( ( sCodePlan->flag & QMNC_SCAN_TRAVERSE_MASK )
             == QMNC_SCAN_TRAVERSE_FORWARD )
        {
            // nothing todo
            // asc �� ��쿡�� ������� �ʴ´�.
        }
        else
        {
            iduVarStringAppend( aString, " DESC" );
        }
    }
    else
    {
        if ( ( ( sCodePlan->flag & QMNC_SCAN_FORCE_RID_SCAN_MASK )
               == QMNC_SCAN_FORCE_RID_SCAN_TRUE ) ||
             ( sMethod->ridRange != NULL ) )
        {
            // PROJ-1789 PROWID
            iduVarStringAppend(aString, ", RID SCAN");
        }
        else
        {
            // Full Scan�� ���
            iduVarStringAppend(aString, ", FULL SCAN");
        }
    }

    //----------------------------
    // ���� Ƚ�� ���
    //----------------------------

    IDE_TEST( printAccessInfo( sCodePlan,
                               sDataPlan,
                               aString,
                               aMode ) != IDE_SUCCESS );

    //----------------------------
    // PROJ-1071
    // Thread ID
    //----------------------------
    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( ( (*sDataPlan->flag & QMND_SCAN_INIT_DONE_MASK)
               == QMND_SCAN_INIT_DONE_TRUE ) &&
             ( sDataPlan->plan.mTID != QMN_PLAN_INIT_THREAD_ID ) )
        {
            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                iduVarStringAppendFormat( aString,
                                          ", TID: %"ID_UINT32_FMT"",
                                          sDataPlan->plan.mTID );
            }
            else
            {
                iduVarStringAppend( aString, ", TID: BLOCKED" );
            }
        }
        else
        {
            // Parallel execution �� �ƴ� ��� ����� �����Ѵ�.
        }
    }
    else
    {
        // Planonly �� ��� ����� �����Ѵ�.
    }

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    //----------------------------
    // Plan ID ���
    //----------------------------
    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        qmn::printSpaceDepth(aString, aDepth);

        // sCodePlan �� ���� ����ϱ⶧���� qmn::printMTRinfo�� ������� ���Ѵ�.
        iduVarStringAppendFormat( aString,
                                  "[ SELF NODE INFO, "
                                  "SELF: %"ID_INT32_FMT" ]\n",
                                  (SInt)sCodePlan->tupleRowID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmnSCAN::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */ )
{
/***********************************************************************
 *
 * Description :
 *    �� �Լ��� ����Ǹ� �ȵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::doItAllFalse( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Filter ������ �����ϴ� Record�� �ϳ��� ���� ��� ���
 *
 *    Constant Filter �˻��Ŀ� �����Ǵ� �Լ��� ���� �����ϴ�
 *    Record�� �������� �ʴ´�.
 *
 * Implementation :
 *    �׻� record ������ �����Ѵ�.
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    /* TASK-7307 DML Data Consistency in Shard */
    // ���ռ� �˻�
    IDE_DASSERT( (sCodePlan->method.constantFilter != NULL) ||
                 ((QCG_CHECK_SHARD_DML_CONSISTENCY( aTemplate->stmt ) == ID_TRUE ) &&
                  (sCodePlan->tableRef->tableInfo->mIsUsable == ID_FALSE)) )
    IDE_DASSERT( ( *sDataPlan->flag & QMND_SCAN_ALL_FALSE_MASK )
                 == QMND_SCAN_ALL_FALSE_TRUE );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;
}


IDE_RC qmnSCAN::doItFirst( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SCAN�� ���� ���� �Լ�
 *    Cursor�� ���� ���� Record�� �д´�.
 *
 * Implementation :
 *    - Table �� IS Lock�� �Ǵ�.
 *    - Session Event Check (������ ���� Detect)
 *    - Key Range, Key Filter, Filter ����
 *    - Cursor Open
 *    - Record �б�
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST(readyIt(aTemplate, aPlan, QMN_PLAN_INIT_THREAD_ID) != IDE_SUCCESS);

    // table�̰ų� temporary table�� ���
    if ( ( *sDataPlan->flag & QMND_SCAN_CURSOR_MASK )
         == QMND_SCAN_CURSOR_OPEN )
    {
        // Record�� ȹ���Ѵ�.
        IDE_TEST( readRow( aTemplate, sCodePlan, sDataPlan, aFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnSCAN::doItNext;
    }
    else
    {
        sDataPlan->doIt = qmnSCAN::doItFirst;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnSCAN::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    SCAN�� ���� ���� �Լ�
 *    ���� Record�� �д´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    if ( ( *sDataPlan->flag & QMND_SCAN_CURSOR_MASK )
         == QMND_SCAN_CURSOR_OPEN )
    {
        // ���� Record�� �д´�.
        IDE_TEST( readRow( aTemplate, sCodePlan, sDataPlan, aFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }
    
    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnSCAN::doItFirst;
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
qmnSCAN::storeCursor( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� Cursor�� ��ġ�� �����Ѵ�.
 *    Merge Join�� ���� ���ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    // ���ռ� �˻�
    IDE_DASSERT( (*sDataPlan->flag & QMND_SCAN_CURSOR_MASK)
                 == QMND_SCAN_CURSOR_OPEN );

    IDE_TEST_RAISE( ( sCodePlan->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
                    == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE, ERR_FIXED_TABLE );

    // Cursor ������ ������.
    IDE_TEST( sDataPlan->cursor->getCurPos( & sDataPlan->cursorInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FIXED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSCAN::storeCursor",
                                  "Fixed Table" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::restoreCursor( qcTemplate * aTemplate,
                        qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    �̹� ����� Cursor�� �̿��Ͽ� Cursor ��ġ�� ������Ŵ
 *
 * Implementation :
 *    Cursor�� ���� �� �ƴ϶� record ���뵵 �����Ѵ�.
 *
 ***********************************************************************/
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    void  * sOrgRow;
    void  * sSearchRow;

    // ���ռ� �˻�
    IDE_DASSERT( (*sDataPlan->flag & QMND_SCAN_CURSOR_MASK)
                 == QMND_SCAN_CURSOR_OPEN );

    IDE_TEST_RAISE( ( sCodePlan->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
                    == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE, ERR_FIXED_TABLE );

    //-----------------------------
    // Cursor�� ����
    //-----------------------------
    
    IDE_TEST( sDataPlan->cursor->setCurPos( & sDataPlan->cursorInfo )
              != IDE_SUCCESS );

    //-----------------------------
    // Record�� ����
    //-----------------------------

    // To Fix PR-8110
    // ����� Cursor�� �����ϸ�, ���� ��ġ�� �������� �̵��Ѵ�.
    // ����, �ܼ��� Cursor�� Read�ϸ� �ȴ�.
    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;

    IDE_TEST(
        sDataPlan->cursor->readRow( (const void**) & sSearchRow,
                                    &sDataPlan->plan.myTuple->rid,
                                    SMI_FIND_NEXT )
        != IDE_SUCCESS );

    sDataPlan->plan.myTuple->row =
        (sSearchRow == NULL) ? sOrgRow : sSearchRow;

    // �ݵ�� ����� Cursor���� Row�� �����Ͽ��� �Ѵ�.
    IDE_ASSERT( sSearchRow != NULL );

    // Ŀ���� ���� ���� ���� �Լ�
    sDataPlan->doIt = qmnSCAN::doItNext;

    // Modify�� ����, record�� ����� �����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FIXED_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSCAN::restoreCursor",
                                  "Fixed Table" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                qmncSCAN   * aCodePlan,
                                qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�� ���� ���� Key Range, Key Filter, Filter�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmnCursorPredicate  sPredicateInfo;
    qmncScanMethod    * sMethod = getScanMethod( aTemplate, aCodePlan );

    //-------------------------------------
    // ���ռ� �˻�
    //-------------------------------------

    if ( ( aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
         == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE )
    {
        // IN Subquery Key Range�� ���� ��� ���� ������ �����ؾ� ��
        IDE_DASSERT( sMethod->fixKeyRange == NULL );
        IDE_DASSERT( sMethod->varKeyRange != NULL );
        IDE_DASSERT( sMethod->fixKeyFilter == NULL );
        IDE_DASSERT( sMethod->varKeyFilter == NULL );
    }

    //-------------------------------------
    // Predicate ������ ����
    //-------------------------------------

    sPredicateInfo.index = sMethod->index;
    sPredicateInfo.tupleRowID = aCodePlan->tupleRowID;

    // Fixed Key Range ������ ����
    sPredicateInfo.fixKeyRangeArea = aDataPlan->fixKeyRangeArea;
    sPredicateInfo.fixKeyRange = aDataPlan->fixKeyRange;
    sPredicateInfo.fixKeyRangeOrg = sMethod->fixKeyRange;
    sPredicateInfo.fixKeyRangeSize = aDataPlan->fixKeyRangeSize;

    // Variable Key Range ������ ����
    sPredicateInfo.varKeyRangeArea = aDataPlan->varKeyRangeArea;
    sPredicateInfo.varKeyRange = aDataPlan->varKeyRange;
    sPredicateInfo.varKeyRangeOrg = sMethod->varKeyRange;
    sPredicateInfo.varKeyRange4FilterOrg = sMethod->varKeyRange4Filter;
    sPredicateInfo.varKeyRangeSize = aDataPlan->varKeyRangeSize;

    // Fixed Key Filter ������ ����
    sPredicateInfo.fixKeyFilterArea = aDataPlan->fixKeyFilterArea;
    sPredicateInfo.fixKeyFilter = aDataPlan->fixKeyFilter;
    sPredicateInfo.fixKeyFilterOrg = sMethod->fixKeyFilter;
    sPredicateInfo.fixKeyFilterSize = aDataPlan->fixKeyFilterSize;

    // Variable Key Filter ������ ����
    sPredicateInfo.varKeyFilterArea = aDataPlan->varKeyFilterArea;
    sPredicateInfo.varKeyFilter = aDataPlan->varKeyFilter;
    sPredicateInfo.varKeyFilterOrg = sMethod->varKeyFilter;
    sPredicateInfo.varKeyFilter4FilterOrg = sMethod->varKeyFilter4Filter;
    sPredicateInfo.varKeyFilterSize = aDataPlan->varKeyFilterSize;

    // Not Null Key Range ������ ����
    sPredicateInfo.notNullKeyRange = aDataPlan->notNullKeyRange;

    // Filter ������ ����
    sPredicateInfo.filter = sMethod->filter;

    sPredicateInfo.filterCallBack = & aDataPlan->callBack;
    sPredicateInfo.callBackDataAnd = & aDataPlan->callBackDataAnd;
    sPredicateInfo.callBackData = aDataPlan->callBackData;

    /* PROJ-2632 */
    sPredicateInfo.mSerialFilterInfo  = aCodePlan->mSerialFilterInfo;
    sPredicateInfo.mSerialExecuteData = aDataPlan->mSerialExecuteData;

    //-------------------------------------
    // Key Range, Key Filter, Filter�� ����
    //-------------------------------------

    IDE_TEST( qmn::makeKeyRangeAndFilter( aTemplate,
                                          & sPredicateInfo )
              != IDE_SUCCESS );

    /* PROJ-2632 */
    if ( sPredicateInfo.mSerialExecuteData != NULL )
    {
        *aDataPlan->flag &= ~QMND_SCAN_SERIAL_EXECUTE_MASK;
        *aDataPlan->flag |= QMND_SCAN_SERIAL_EXECUTE_TRUE;
    }
    else
    {
        *aDataPlan->flag &= ~QMND_SCAN_SERIAL_EXECUTE_MASK;
        *aDataPlan->flag |= QMND_SCAN_SERIAL_EXECUTE_FALSE;
    }

    aDataPlan->keyRange = sPredicateInfo.keyRange;
    aDataPlan->keyFilter = sPredicateInfo.keyFilter;

    //-------------------------------------
    // IN SUBQUERY KEY RANGE �˻�
    //-------------------------------------

    if ( ( (aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
           == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
         ( aDataPlan->keyRange == smiGetDefaultKeyRange() ) )
    {
        // IN SUBQUERY Key Range�� �ִ� �����
        // �� �̻� Record�� �������� �ʾ� Key Range�� ��������
        // ���ϴ� ����̴�.  ���� Type Conversion������ ���Ͽ�
        // Key Range�� �������� ���ϴ� ���� ����.
        // ����, �� �̻� record�� �˻��ؼ��� �ȵȴ�.
        *aDataPlan->flag &= ~QMND_SCAN_INSUBQ_RANGE_BUILD_MASK;
        *aDataPlan->flag |= QMND_SCAN_INSUBQ_RANGE_BUILD_FAILURE;
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
qmnSCAN::firstInit( qcTemplate * aTemplate,
                    qmncSCAN   * aCodePlan,
                    qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    SCAN node�� Data ������ ����� ���� �ʱ�ȭ�� ����
 *
 * Implementation :
 *    - Data ������ �ֿ� ����� ���� �ʱ�ȭ�� ����
 *
 ***********************************************************************/

    qmnCursorInfo  * sCursorInfo;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    // Key Range�� Fixed KeyRange�� Variable KeyRange�� ȥ��� �� ����.
    IDE_DASSERT( (sMethod->fixKeyRange == NULL) ||
                 (sMethod->varKeyRange == NULL) );

    // Key Filter�� Fixed KeyFilter�� Variable KeyFilter�� ȥ��� �� ����.
    IDE_DASSERT( (sMethod->fixKeyFilter == NULL) ||
                 (sMethod->varKeyFilter == NULL) );

    //---------------------------------
    // SCAN ���� ������ �ʱ�ȭ
    //---------------------------------

    // Tuple ��ġ�� ����
    aDataPlan->plan.myTuple =
        & aTemplate->tmplate.rows[aCodePlan->tupleRowID];

    // PROJ-1382, jhseong, FixedTable and PerformanceView
    // FIX BUG-12167
    if ( ( aCodePlan->flag & QMNC_SCAN_TABLE_FV_MASK )
         == QMNC_SCAN_TABLE_FV_FALSE )
    {
        if ( aDataPlan->plan.myTuple->cursorInfo == NULL )
        {
            // Table�� IS Lock�� �Ǵ�.
            IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aTemplate->stmt))->getTrans(),
                                                aCodePlan->table,
                                                aCodePlan->tableSCN,
                                                SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                SMI_TABLE_LOCK_IS,
                                                ID_ULONG_MAX,
                                                ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                     != IDE_SUCCESS);
        }
        else
        {
            // BUG-42952 DML�� ��� IX Lock�� �Ǵ�.
            IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aTemplate->stmt))->getTrans(),
                                                aCodePlan->table,
                                                aCodePlan->tableSCN,
                                                SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                                SMI_TABLE_LOCK_IX,
                                                ID_ULONG_MAX,
                                                ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                     != IDE_SUCCESS);
        }
    }
    else
    {
        // do nothing
    }

    // ���ռ� �˻�
    // ���� ��ü�� ���� ������ Tuple ������ Plan�� ������ �����ϰ�
    // �����Ǿ� �ִ� �� �˻�
    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_DISK )
    {
        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_DISK );
    }
    else
    {
        IDE_DASSERT( (aDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK)
                     == MTC_TUPLE_STORAGE_MEMORY );
    }

    // Cursor Property�� ����
    // Session Event ������ �����ϱ� ���Ͽ� �����ؾ� �Ѵ�.

    idlOS::memcpy( & aDataPlan->cursorProperty,
                   & aCodePlan->cursorProperty,
                   ID_SIZEOF( smiCursorProperties ) );
    aDataPlan->cursorProperty.mStatistics =
        aTemplate->stmt->mStatistics;

    // BUG-10146 limit ���� host variable ���
    // aCodePlan�� limit ������ ������ cursorProperties�� �����Ѵ�.
    if( aCodePlan->limit != NULL )
    {
        IDE_TEST( qmsLimitI::getStartValue(
                      aTemplate,
                      aCodePlan->limit,
                      &aDataPlan->cursorProperty.mFirstReadRecordPos )
                  != IDE_SUCCESS );

        // limit�� start�� 1���� ����������,
        // recordPosition�� 0���� �����Ѵ�.
        aDataPlan->cursorProperty.mFirstReadRecordPos--;

        IDE_TEST( qmsLimitI::getCountValue(
                      aTemplate,
                      aCodePlan->limit,
                      &aDataPlan->cursorProperty.mReadRecordCount )
                  != IDE_SUCCESS );

    }

    // PROJ-1071
    IDE_TEST( aTemplate->stmt->qmxMem->alloc( ID_SIZEOF(smiTableCursor),
                                              (void**) & aDataPlan->cursor )
              != IDE_SUCCESS);

    // fix BUG-9052
    aDataPlan->subQFilterDepCnt = 0;

    //---------------------------------
    // Predicate ������ �ʱ�ȭ
    //---------------------------------

    // Fixed Key Range ������ �Ҵ�
    IDE_TEST( allocFixKeyRange( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // Fixed Key Filter ������ �Ҵ�
    IDE_TEST( allocFixKeyFilter( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // Variable Key Range ������ �Ҵ�
    IDE_TEST( allocVarKeyRange( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // Variable Key Filter ������ �Ҵ�
    IDE_TEST( allocVarKeyFilter( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // Not Null Key Range ������ �Ҵ�
    IDE_TEST( allocNotNullKeyRange( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    // PROJ-1789 PROWID
    IDE_TEST(allocRidRange(aTemplate, aCodePlan, aDataPlan) != IDE_SUCCESS);

    aDataPlan->keyRange = NULL;
    aDataPlan->keyFilter = NULL;

    //---------------------------------
    // Disk Table ���� ������ �ʱ�ȭ
    //---------------------------------

    // [Disk Table�� ���]
    //   Record�� �б� ���� ���� ����,
    //   Variable Column�� value pointer����
    //   ���� �۾��� ���� �Լ��� ���Ͽ� ó���Ѵ�.
    // [Memory Table�� ���]
    //   Variable Column�� value pointer����
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->tupleRowID ) != IDE_SUCCESS );

    // Disk Table�� ���
    //   Null Row�� ���� �������� ����� disk I/O�� �����ϱ�
    //   ���Ͽ� �ش� ������ �����Ͽ� ó���Ѵ�.
    //   ���� ȣ�� �� memory�� �Ҵ�޾� SM�κ��� null row�� ��� �´�.
    aDataPlan->nullRow = NULL;

    //---------------------------------
    // Trigger�� ���� ������ ����
    //---------------------------------

    aDataPlan->isNeedAllFetchColumn = ID_FALSE;

    //---------------------------------
    // cursor ���� ����
    //---------------------------------

    if ( aDataPlan->plan.myTuple->cursorInfo != NULL )
    {
        sCursorInfo = (qmnCursorInfo*) aDataPlan->plan.myTuple->cursorInfo;

        aDataPlan->updateColumnList = sCursorInfo->updateColumnList;
        aDataPlan->cursorType       = sCursorInfo->cursorType;

        /* PROJ-2626 Snapshot Export */
        if ( aTemplate->stmt->mInplaceUpdateDisableFlag == ID_TRUE )
        {
            aDataPlan->inplaceUpdate = ID_FALSE;
        }
        else
        {
            aDataPlan->inplaceUpdate = sCursorInfo->inplaceUpdate;
        }

        aDataPlan->lockMode         = sCursorInfo->lockMode;

        if ( sCursorInfo->isRowMovementUpdate == ID_TRUE )
        {
            aDataPlan->isNeedAllFetchColumn = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // DML�� �ƴ� ��� (select, select for update, dequeue)

        aDataPlan->updateColumnList = NULL;
        aDataPlan->cursorType       = SMI_SELECT_CURSOR;
        aDataPlan->inplaceUpdate    = ID_FALSE;
        aDataPlan->lockMode         = aCodePlan->lockMode;  // �׻� SMI_LOCK_READ�� �ƴϴ�.
    }

    /* PROJ-2402 */
    aDataPlan->mAccessCnt4Parallel = 0;

    /* BUG-42639 Monitoring query */
    if ( ( aCodePlan->flag & QMNC_SCAN_FAST_SELECT_FIXED_TABLE_MASK )
         == QMNC_SCAN_FAST_SELECT_FIXED_TABLE_TRUE )
    {
        aDataPlan->fixedTable.recRecord   = NULL;
        aDataPlan->fixedTable.traversePtr = NULL;
        SMI_FIXED_TABLE_PROPERTIES_INIT( &aDataPlan->fixedTableProperty );
    
        IDE_TEST_RAISE( ( aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
                          == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE, UNEXPECTED_ERROR );
    }
    else
    {
        /* Nothing to do */
    }


    /* PROJ-2632 */
    if ( aCodePlan->mSerialFilterInfo != NULL )
    {
        QTC_SERIAL_EXEUCTE_DATA_INITIALIZE( aDataPlan->mSerialExecuteData,
                                            aTemplate->tmplate.data,
                                            aCodePlan->mSerialFilterSize,
                                            aCodePlan->mSerialFilterCount,
                                            aCodePlan->mSerialFilterOffset );
    }
    else
    {
        aDataPlan->mSerialExecuteData = NULL;
    }

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_SCAN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SCAN_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( UNEXPECTED_ERROR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSCAN::firstInit",
                                  "The fixed table has insubquery key range" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::allocFixKeyRange( qcTemplate * aTemplate,
                           qmncSCAN   * aCodePlan,
                           qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1413
 *    Fixed Key Range�� ���� ������ �Ҵ�޴´�.
 *    Fixed Key Range�� �ϴ��� plan ������ ���� fixed key range��
 *    �����ǹǷ� variable key range�� ���������� �ڽŸ��� ������
 *    �������� ���� key range�� ������ �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    if ( sMethod->fixKeyRange != NULL )
    {
        IDE_DASSERT( sMethod->index != NULL );

        // Fixed Key Range�� ũ�� ���
        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 sMethod->fixKeyRange,
                                                 & aDataPlan->fixKeyRangeSize )
                  != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->fixKeyRangeSize > 0 );
        // Fixed Key Range�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;

        IDU_FIT_POINT( "qmnSCAN::allocFixKeyRange::cralloc::fixKeyRangeArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( sMemory->cralloc( aDataPlan->fixKeyRangeSize,
                                    (void**) & aDataPlan->fixKeyRangeArea )
                  != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->fixKeyRangeSize = 0;
        aDataPlan->fixKeyRangeArea = NULL;
        aDataPlan->fixKeyRange = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::allocFixKeyFilter( qcTemplate * aTemplate,
                            qmncSCAN   * aCodePlan,
                            qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1436
 *    Fixed Key Filter�� ���� ������ �Ҵ�޴´�.
 *    Fixed Key Filter�� �ϴ��� plan ������ ���� fixed key filter��
 *    �����ǹǷ� variable key filter�� ���������� �ڽŸ��� ������
 *    �������� ���� key filter�� ������ �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    if ( ( sMethod->fixKeyFilter != NULL ) &&
         ( sMethod->fixKeyRange != NULL ) )
    {
        IDE_DASSERT( sMethod->index != NULL );

        // Fixed Key Filter�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           sMethod->fixKeyFilter,
                                           & aDataPlan->fixKeyFilterSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->fixKeyFilterSize > 0 );

        // Fixed Key Filter�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;

        IDU_FIT_POINT( "qmnSCAN::allocFixKeyFilter::cralloc::fixKeyFilterArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( sMemory->cralloc( aDataPlan->fixKeyFilterSize,
                                    (void**) & aDataPlan->fixKeyFilterArea )
                  != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->fixKeyFilterSize = 0;
        aDataPlan->fixKeyFilterArea = NULL;
        aDataPlan->fixKeyFilter = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::allocVarKeyRange( qcTemplate * aTemplate,
                           qmncSCAN   * aCodePlan,
                           qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Variable Key Range�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    if ( sMethod->varKeyRange != NULL )
    {
        IDE_DASSERT( sMethod->index != NULL );

        // Variable Key Range�� ũ�� ���
        IDE_TEST( qmoKeyRange::estimateKeyRange( aTemplate,
                                                 sMethod->varKeyRange,
                                                 & aDataPlan->varKeyRangeSize )
                  != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->varKeyRangeSize > 0 );

        // Variable Key Range�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;

        IDU_FIT_POINT( "qmnSCAN::allocVarKeyRange::cralloc::varKeyRangeArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( sMemory->cralloc( aDataPlan->varKeyRangeSize,
                                          (void**) & aDataPlan->varKeyRangeArea )
                        != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->varKeyRangeSize = 0;
        aDataPlan->varKeyRangeArea = NULL;
        aDataPlan->varKeyRange = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmnSCAN::allocVarKeyFilter( qcTemplate * aTemplate,
                            qmncSCAN   * aCodePlan,
                            qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Variable Key Filter�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    if ( ( sMethod->varKeyFilter != NULL ) &&
         ( (sMethod->varKeyRange != NULL) ||
           (sMethod->fixKeyRange != NULL) ) ) // BUG-20679
    {
        IDE_DASSERT( sMethod->index != NULL );

        // Variable Key Filter�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           sMethod->varKeyFilter,
                                           & aDataPlan->varKeyFilterSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->varKeyFilterSize > 0 );

        // Variable Key Filter�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;
        IDU_LIMITPOINT("qmnSCAN::allocVarKeyFilter::malloc");
        IDE_TEST( sMemory->cralloc( aDataPlan->varKeyFilterSize,
                                    (void**) & aDataPlan->varKeyFilterArea )
                  != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->varKeyFilterSize = 0;
        aDataPlan->varKeyFilterArea = NULL;
        aDataPlan->varKeyFilter = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::allocNotNullKeyRange( qcTemplate * aTemplate,
                               qmncSCAN   * aCodePlan,
                               qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    NotNull Key Range�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;
    UInt        sSize;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    if ( ( (aCodePlan->flag & QMNC_SCAN_NOTNULL_RANGE_MASK) ==
           QMNC_SCAN_NOTNULL_RANGE_TRUE ) &&
         ( sMethod->fixKeyRange == NULL ) &&
         ( sMethod->varKeyRange == NULL ) )
    {
        // keyRange ������ ���� size ���ϱ�
        IDE_TEST( mtk::estimateRangeDefault( NULL,
                                             NULL,
                                             0,
                                             &sSize )
                  != IDE_SUCCESS );

        IDE_DASSERT( sSize > 0 );

        // Fixed Key Range�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;

        IDU_FIT_POINT( "qmnSCAN::allocNotNullKeyRange::cralloc::notNullKeyRange",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( sMemory->cralloc( sSize,
                                    (void**) & aDataPlan->notNullKeyRange )
                  != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->notNullKeyRange = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::openCursor( qcTemplate * aTemplate,
                     qmncSCAN   * aCodePlan,
                     qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�� ����.
 *
 * Implementation :
 *    Cursor�� ���� ���� ������ �����Ѵ�.
 *    Cursor�� ���� Cursor Manager�� ����Ѵ�.
 *    Cursor�� ���� ��ġ�� �̵���Ų��.
 *
 ***********************************************************************/

    const void            * sTableHandle = NULL;
    const void            * sIndexHandle = NULL;
    smSCN                   sTableSCN;
    UInt                    sTraverse;
    UInt                    sPrevious;
    UInt                    sCursorFlag;
    UInt                    sInplaceUpdate;
    void                  * sOrgRow;
    qmncScanMethod        * sMethod = getScanMethod( aTemplate, aCodePlan );
    idBool                  sIsDequeue = ID_FALSE;
    smiFetchColumnList    * sFetchColumnList = NULL;
    smiRange              * sRange; // KeyRange or RIDRange
    qmnCursorInfo         * sCursorInfo;
    idBool                  sIsMutexLock = ID_FALSE;
    smSCN                   sBaseTableSCN;
    qcStatement           * sStmt   = aTemplate->stmt;

    if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
          == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
         ((*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
          == QMND_SCAN_INSUBQ_RANGE_BUILD_FAILURE ) )
    {
        // Cursor�� ���� �ʴ´�.
        // �� �̻� IN SUBQUERY Key Range�� ������ �� ���� ����
        // record�� fetch���� �ʾƾ� �Ѵ�.
        // Nothing To Do
    }
    else
    {
        //-------------------------------------------------------
        //  Cursor�� ���� ���� ����
        //-------------------------------------------------------

        //---------------------------
        // Flag ������ ����
        //---------------------------

        // Traverse ������ ����
        if ( ( aCodePlan->flag & QMNC_SCAN_TRAVERSE_MASK )
             == QMNC_SCAN_TRAVERSE_FORWARD )
        {
            sTraverse = SMI_TRAVERSE_FORWARD;
        }
        else
        {
            sTraverse = SMI_TRAVERSE_BACKWARD;
        }

        // Previous ��� ���� ����
        if ( ( aCodePlan->flag & QMNC_SCAN_PREVIOUS_ENABLE_MASK )
             == QMNC_SCAN_PREVIOUS_ENABLE_TRUE )
        {
            sPrevious = SMI_PREVIOUS_ENABLE;
        }
        else
        {
            sPrevious = SMI_PREVIOUS_DISABLE;
        }

        // PROJ-1509
        // inplace update ���� ����
        // MEMORY table������,
        // trigger or foreign key�� �ִ� ���,
        // ���� ����/���� ���� �б� ���ؼ���
        // inplace update�� ���� �ʵ��� �ؾ� �Ѵ�.
        // �� ������ update cursor������ �ʿ��� ��������,
        // qp���� �̸� cursor���� �����ϸ� �ڵ尡 ��������,
        // insert, update, delete cursor�� ������� ������ sm���� �ѱ��.
        // < ���� > sm������ ���ǻ����̸�,
        //          sm���� �� ������ update cursor������
        //          ���Ǳ� ������ ���� ���ٰ� ��.
        if( aDataPlan->inplaceUpdate == ID_TRUE )
        {
            sInplaceUpdate = SMI_INPLACE_UPDATE_ENABLE;
        }
        else
        {
            sInplaceUpdate = SMI_INPLACE_UPDATE_DISABLE;
        }

        sCursorFlag =
            aDataPlan->lockMode | sTraverse | sPrevious | sInplaceUpdate;

        //---------------------------
        // Index Handle�� ȹ��
        //---------------------------

        if (sMethod->index != NULL)
        {
            sIndexHandle = sMethod->index->indexHandle;
        }
        else
        {
            sIndexHandle = NULL;
        }

        // PROJ-1407 Temporary Table
        if ( qcuTemporaryObj::isTemporaryTable(
                 aCodePlan->tableRef->tableInfo ) == ID_TRUE )
        {
            qcuTemporaryObj::getTempTableHandle( sStmt,
                                                 aCodePlan->tableRef->tableInfo,
                                                 &sTableHandle,
                                                 &sBaseTableSCN );
            
            IDE_TEST_CONT( sTableHandle == NULL, NORMAL_EXIT_EMPTY );

            IDE_TEST_RAISE( !SM_SCN_IS_EQ( &(aCodePlan->tableRef->tableSCN), &sBaseTableSCN ),
                            ERR_TEMPORARY_TABLE_EXIST );

            // Session Temp Table�� �����ϴ� ���
            sTableSCN = smiGetRowSCN( sTableHandle );

            if( sIndexHandle != NULL )
            {
                sIndexHandle = qcuTemporaryObj::getTempIndexHandle(
                    sStmt,
                    aCodePlan->tableRef->tableInfo,
                    sMethod->index->indexId );
                // �ݵ�� �����Ͽ��� �Ѵ�.
                IDE_ASSERT( sIndexHandle != NULL );
            }
        }
        else
        {
            sTableHandle = aCodePlan->table;
            sTableSCN    = aCodePlan->tableSCN;
        }

        //-------------------------------------------------------
        //  Cursor�� ����.
        //-------------------------------------------------------
        
        // Cursor�� �ʱ�ȭ
        aDataPlan->cursor->initialize();
        
        // PROJ-1618
        aDataPlan->cursor->setDumpObject( aCodePlan->dumpObject );
        
        //BUG-48230: DEQUEUE ���� ����
        if ( (aCodePlan->flag & QMNC_SCAN_TABLE_QUEUE_MASK)
             == QMNC_SCAN_TABLE_QUEUE_TRUE )
        {
            sIsDequeue = ID_TRUE;
        }
        else
        {
            sIsDequeue = ID_FALSE;
        }

        // PROJ-1705
        // ���ڵ���ġ�� ����Ǿ�� �� �÷���������
        if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
             == QMN_PLAN_STORAGE_DISK )
        {
            IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                          aTemplate,
                          aCodePlan->tupleRowID,
                          aDataPlan->isNeedAllFetchColumn,
                          ( sIndexHandle != NULL ) ? sMethod->index: NULL,
                          ID_TRUE,
                          & sFetchColumnList ) != IDE_SUCCESS );

            aDataPlan->cursorProperty.mFetchColumnList = sFetchColumnList;

            // select for update�� repeatable read ó���� ����
            // sm�� qp���� �Ҵ��� �޸𸮿����� �����ش�.
            // sm���� select for update�� repeatable readó���� �޸� ��� ���.
            aDataPlan->cursorProperty.mLockRowBuffer = (UChar*)aDataPlan->plan.myTuple->row;
            aDataPlan->cursorProperty.mLockRowBufferSize =
                        aTemplate->tmplate.rows[aCodePlan->tupleRowID].rowOffset;
        }
        else
        {
            // Nothing To Do
        }

        /*
         * SMI_CURSOR_PROP_INIT �� ������� �ʰ�
         * mIndexTypeID �� ���� setting �ϴ� ������
         * ������ mFetchColumnList, mLockRowBuffer �� �����ϱ� �����̴�.
         * SMI_CURSOR_PROP_INIT �� ����ϸ� �ٽ� �� �ʱ�ȭ �ǹǷ�
         */
        if (sMethod->ridRange != NULL)
        {
            aDataPlan->cursorProperty.mIndexTypeID =
                SMI_BUILTIN_GRID_INDEXTYPE_ID;
            sRange = aDataPlan->ridRange;
        }
        else
        {
            if ( sIndexHandle == NULL )
            {
                aDataPlan->cursorProperty.mIndexTypeID =
                    SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID;
            }
            else
            {
                aDataPlan->cursorProperty.mIndexTypeID =
                    (UChar)sMethod->index->indexTypeId;
            }

            sRange = aDataPlan->keyRange;
        }

        /* PROJ-1832 New database link */
        if ( ( ( aCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
                QMNC_SCAN_REMOTE_TABLE_TRUE ) ||
             ( ( aCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_STORE_MASK ) ==
               QMNC_SCAN_REMOTE_TABLE_STORE_TRUE ) )
        {
            aDataPlan->cursorProperty.mRemoteTableParam.mQcStatement = sStmt;
            aDataPlan->cursorProperty.mRemoteTableParam.mDkiSession =
                QCG_GET_DATABASE_LINK_SESSION( sStmt );
        }
        else
        {
            /* do nothing */
        }

        /* BUG-38290
         * Cursor open �� ���ü� ��� �ʿ��ϴ�.
         * Cursor �� SM ���� open �� transaction �� ����ϴµ�,
         * transaction �� ���ü� ��� ����Ǿ� ���� �����Ƿ�
         * QP ���� ���ü� ��� �ؾ� �Ѵ�.
         * ��, �̹� open �� cursor �� cursro manager �� �߰��ϴ�
         * addOpendCursor �Լ��� ���ü� ��� �ؾ� �Ѵ�.
         *
         * �̴� cursor open �� SCAN �� PCRD, CUNT ��� ���Ǵµ�,
         * Ư�� SCAN ��尡 parallel query �� ���� �ÿ� worker thread ��
         * ���� ���ÿ� ����� ���ɼ��� ���� �����̴�.
         *
         * ���� ���ü� ��� ���� cursor manager �� mutex �� ���ü���
         * ���� �Ѵ�.
         */
        IDE_TEST( sStmt->mCursorMutex.lock(NULL) != IDE_SUCCESS );
        sIsMutexLock = ID_TRUE;

        IDE_TEST( aDataPlan->cursor->open( QC_SMI_STMT( sStmt ),
                                           sTableHandle,
                                           sIndexHandle,
                                           sTableSCN,
                                           aDataPlan->updateColumnList,
                                           sRange,
                                           aDataPlan->keyFilter,
                                           &aDataPlan->callBack,
                                           sCursorFlag,
                                           aDataPlan->cursorType,
                                           &aDataPlan->cursorProperty,
                                           sIsDequeue ) 
                  != IDE_SUCCESS );
        
        // Cursor�� ���
        IDE_TEST( aTemplate->cursorMgr->addOpenedCursor(
                      sStmt->qmxMem,
                      aCodePlan->tupleRowID,
                      aDataPlan->cursor )
                  != IDE_SUCCESS );

        sIsMutexLock = ID_FALSE;
        IDE_TEST( sStmt->mCursorMutex.unlock() != IDE_SUCCESS );

        // Cursor�� �������� ǥ��
        *aDataPlan->flag &= ~QMND_SCAN_CURSOR_MASK;
        *aDataPlan->flag |= QMND_SCAN_CURSOR_OPEN;

        //-------------------------------------------------------
        //  Cursor�� ���� ��ġ�� �̵�
        //-------------------------------------------------------

        // Disk Table�� ���
        // Key Range �˻����� ���� �ش� row�� ������ ����� �� �ִ�.
        // �̸� ���� ���� ������ ��ġ�� �����ϰ� �̸� �����Ѵ�.
        sOrgRow = aDataPlan->plan.myTuple->row;
        
        IDE_TEST( aDataPlan->cursor->beforeFirst() != IDE_SUCCESS );
        
        aDataPlan->plan.myTuple->row = sOrgRow;

        // for empty session temporary table
        IDE_EXCEPTION_CONT( NORMAL_EXIT_EMPTY );

        //---------------------------------
        // cursor ���� ����
        //---------------------------------

        if ( aDataPlan->plan.myTuple->cursorInfo != NULL )
        {
            // DML������ ����ϴ� scan�� ���
            // cursor ������ �����ϰ� cursor�� �����Ѵ�.

            sCursorInfo = (qmnCursorInfo*) aDataPlan->plan.myTuple->cursorInfo;

            sCursorInfo->cursor = aDataPlan->cursor;

            // selected index
            sCursorInfo->selectedIndex = sMethod->index;

            /* PROJ-2359 Table/Partition Access Option */
            sCursorInfo->accessOption = aCodePlan->accessOption;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TEMPORARY_TABLE_EXIST )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QMN_INVALID_TEMPORARY_TABLE ));
    }
    IDE_EXCEPTION_END;

    if ( sIsMutexLock == ID_TRUE )
    {
        (void)sStmt->mCursorMutex.unlock();
        sIsMutexLock = ID_FALSE;
    }

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::restartCursor( qmncSCAN   * aCodePlan,
                               qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�� Restart�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void    * sOrgRow;
    smiRange* sRange;

    if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
          == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
         ((*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
          == QMND_SCAN_INSUBQ_RANGE_BUILD_FAILURE ) )
    {
        // Cursor�� ���� �ʴ´�.
        // �� �̻� IN SUBQUERY Key Range�� ������ �� ���� ����
        // record�� fetch���� �ʾƾ� �Ѵ�.
        // Nothing To Do
    }
    else
    {

        /* BUG-41490 */
        if (aDataPlan->ridRange != NULL)
        {
            sRange = aDataPlan->ridRange;
        }
        else
        {
            sRange = aDataPlan->keyRange;
        }

        // Disk Table�� ���
        // Key Range �˻����� ���� �ش� row�� ������ ����� �� �ִ�.
        // �̸� ���� ���� ������ ��ġ�� �����ϰ� �̸� �����Ѵ�.
        sOrgRow = aDataPlan->plan.myTuple->row;

        IDE_TEST( aDataPlan->cursor->restart( sRange,
                                              aDataPlan->keyFilter,
                                              &aDataPlan->callBack )
                  != IDE_SUCCESS);

        IDE_TEST( aDataPlan->cursor->beforeFirst() != IDE_SUCCESS );

        aDataPlan->plan.myTuple->row = sOrgRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::readRow(qcTemplate * aTemplate,
                        qmncSCAN   * aCodePlan,
                        qmndSCAN   * aDataPlan,
                        qmcRowFlag * aFlag)
{
/***********************************************************************
 *
 * Description :
 *    Cursor�κ��� ���ǿ� �´� Record�� �д´�.
 *
 * Implementation :
 *    Cursor�� �̿��Ͽ� KeyRange, KeyFilter, Filter�� �����ϴ�
 *    Record�� �д´�.
 *    ����, subquery filter�� �����Ͽ� �̸� �����ϸ� record�� �����Ѵ�.
 *
 *    [ Record ��ġ�� ���� ���� ]
 *        - Disk Table�� ���
 *             SM�� Filter �˻�, Data ���� ������ ����
 *             ���� ������ �Ҿ� ���� �� �ִ�.
 *             - SM�� Filter �˻� : plan.myTuple->row �����
 *             - Data ����        : sSearchRow �����
 *        -  Memory Table�� ����ϴ� ��쿡��
 *           ������ logic ���뿡 ���� ������ ������ �Ѵ�.
 *
 ***********************************************************************/

    idBool sJudge;
    void * sOrgRow;
    void * sSearchRow;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    //----------------------------------------------
    // 1. Cursor�� �̿��Ͽ� Record�� ����
    // 2. Subquery Filter ����
    // 3. Subquery Filter �� �����ϸ� ��� ����
    //----------------------------------------------

    sJudge = ID_FALSE;

    while ( sJudge == ID_FALSE )
    {
        //----------------------------------------
        // Cursor�� �̿��Ͽ� Record�� ����
        //----------------------------------------

        if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
              == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
             ((*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
              == QMND_SCAN_INSUBQ_RANGE_BUILD_FAILURE ) )
        {
            // Subquery ����� �����
            // IN SUBQUERY KEYRANGE�� ���� RANGE ������ ������ ���
            sSearchRow = NULL;
        }
        else
        {
            sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;
            IDE_TEST(
                aDataPlan->cursor->readRow( (const void**) & sSearchRow,
                                            &aDataPlan->plan.myTuple->rid,
                                            SMI_FIND_NEXT )
                != IDE_SUCCESS );
            aDataPlan->plan.myTuple->row =
                (sSearchRow == NULL) ? sOrgRow : sSearchRow;

            // Proj 1360 Queue
            // dequeue�������� �ش� row�� �����ؾ� �Ѵ�.
            if ( (sSearchRow != NULL) &&
                 ((aCodePlan->flag &  QMNC_SCAN_TABLE_QUEUE_MASK)
                  == QMNC_SCAN_TABLE_QUEUE_TRUE ))
            {
                IDE_TEST(aDataPlan->cursor->deleteRow() != IDE_SUCCESS);
            }
        }

        //----------------------------------------
        // IN SUBQUERY KEY RANGE�� ��õ�
        //----------------------------------------

        if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
              == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
             (sSearchRow == NULL) )
        {
            // IN SUBQUERY KEYRANGE�� ��� �ٽ� �õ��Ѵ�.
            IDE_TEST( reRead4InSubRange( aTemplate,
                                         aCodePlan,
                                         aDataPlan,
                                         & sSearchRow ) != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        //----------------------------------------
        // SUBQUERY FILTER ó��
        //----------------------------------------

        if ( sSearchRow == NULL )
        {
            sJudge = ID_FALSE;
            break;
        }
        else
        {
            // modify�� ����
            if ( sMethod->filter == NULL )
            {
                // modify�� ����.
                aDataPlan->plan.myTuple->modify++;
            }
            else
            {
                // SM�� ���Ͽ� filter���� �� modify ���� ������.

                // fix BUG-9052 BUG-9248

                // BUG-9052
                // SELECT * FROM T1 WHERE T1.I2 IN ( SELECT MAX(T3.I2)
                //                       FROM T2, T3
                //                       WHERE T2.I1 = T3.I1
                //                       AND T3.I1 = T1.I1
                //                       GROUP BY T2.I2 ) AND T1.I1 > 0;
                // subquery filter�� outer column ������
                // outer column�� ������ store and search��
                // ������ϵ��� �ϱ� ����
                // aDataPlan->plan.myTuple->modify��
                // aDataPlan->subQFilterDepCnt�� ���� ������Ų��.
                // printPlan()������ ACCESS count display��
                // DataPlan->plan.myTuple->modify����
                // DataPlan->subQFilterDepCnt ���� ���ֵ��� �Ѵ�.

                // BUG-9248
                // subquery filter�̿ܿ��� modify count���� �����ؼ�
                // �߰������ �������ؾ��ϴ� ��찡 ����

                aDataPlan->plan.myTuple->modify++;
                aDataPlan->subQFilterDepCnt++;
            }

            // Subquery Filter�� ����
            if ( sMethod->subqueryFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      sMethod->subqueryFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                sJudge = ID_TRUE;
            }

            // NNF Filter�� ����
            if ( aCodePlan->nnfFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      aCodePlan->nnfFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    if ( sJudge == ID_TRUE )
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_EXIST;
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::readRowFromGRID( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan,
                                 scGRID       aRowGRID,
                                 qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�κ��� rid�� �ش��ϴ� Record�� �д´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    //----------------------------------------------
    // ���ռ� �˻�
    //----------------------------------------------

    IDE_DASSERT( ( sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
                 == QMN_PLAN_STORAGE_DISK );

    IDE_DASSERT( ( sCodePlan->flag & QMNC_SCAN_FORCE_RID_SCAN_MASK )
                 == QMNC_SCAN_FORCE_RID_SCAN_TRUE );

    IDE_DASSERT( ( *sDataPlan->flag & QMND_SCAN_CURSOR_MASK )
                 == QMND_SCAN_CURSOR_OPEN );

    //----------------------------------------------
    // Cursor�� �̿��Ͽ� Record�� ����
    //----------------------------------------------

    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;

    IDE_TEST( sDataPlan->cursor->readRowFromGRID( (const void**) & sSearchRow,
                                                  aRowGRID )
              != IDE_SUCCESS );

    sDataPlan->plan.myTuple->row = (sSearchRow == NULL) ? sOrgRow : sSearchRow;
    sDataPlan->plan.myTuple->rid = aRowGRID;

    if ( sSearchRow != NULL )
    {
        // modify�� ����.
        sDataPlan->plan.myTuple->modify++;

        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_EXIST;
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::reRead4InSubRange( qcTemplate * aTemplate,
                            qmncSCAN   * aCodePlan,
                            qmndSCAN   * aDataPlan,
                            void      ** aRow )
{
/***********************************************************************
 *
 * Description :
 *     IN SUBQUERY KEYRANGE�� ���� ��� Record Read�� �ٽ� �õ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void  * sSearchRow = NULL;
    void  * sOrgRow;

    if ( (*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
         == QMND_SCAN_INSUBQ_RANGE_BUILD_SUCCESS )
    {
        //---------------------------------------------------------
        // [IN SUBQUERY KEY RANGE]
        // �˻��� Record�� ������, Subquery�� ����� ���� �����ϴ� ���
        //     - Key Range�� �ٽ� �����Ѵ�.
        //     - Record�� Fetch�Ѵ�.
        //     - ���� ������ Fetch�� Record�� �ְų�,
        //       Key Range ������ ������ ������ �ݺ��Ѵ�.
        //---------------------------------------------------------

        while ( (sSearchRow == NULL) &&
                ((*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
                 == QMND_SCAN_INSUBQ_RANGE_BUILD_SUCCESS ) )
        {
            /* BUG-41110 */
            IDE_TEST(makeRidRange(aTemplate, aCodePlan, aDataPlan)
                     != IDE_SUCCESS);

            // Key Range�� ������Ѵ�.
            IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                             aCodePlan,
                                             aDataPlan ) != IDE_SUCCESS );

            // Cursor�� ����.
            // �̹� Open�Ǿ� �����Ƿ�, Restart�Ѵ�.
            IDE_TEST( restartCursor( aCodePlan,
                                     aDataPlan ) != IDE_SUCCESS );


            if ( (*aDataPlan->flag & QMND_SCAN_INSUBQ_RANGE_BUILD_MASK)
                 == QMND_SCAN_INSUBQ_RANGE_BUILD_SUCCESS )
            {
                // Key Range ������ ������ ��� Record�� �д´�.
                sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

                IDE_TEST(
                    aDataPlan->cursor->readRow( (const void**) & sSearchRow,
                                                &aDataPlan->plan.myTuple->rid,
                                                SMI_FIND_NEXT )
                    != IDE_SUCCESS );

                aDataPlan->plan.myTuple->row =
                    (sSearchRow == NULL) ? sOrgRow : sSearchRow;
            }
            else
            {
                sSearchRow = NULL;
            }
        }
    }
    else
    {
        // sSearchRow = NULL;
    }

    *aRow = sSearchRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::printAccessInfo( qmncSCAN     * aCodePlan,
                          qmndSCAN     * aDataPlan,
                          iduVarString * aString,
                          qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     Access ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    ULong  sPageCount;

    IDE_DASSERT( aCodePlan != NULL );
    IDE_DASSERT( aDataPlan != NULL );
    IDE_DASSERT( aString   != NULL );

    if ( aMode == QMN_DISPLAY_ALL )
    {
        //----------------------------
        // explain plan = on; �� ���
        //----------------------------

        // ���� Ƚ�� ���
        if ( (*aDataPlan->flag & QMND_SCAN_INIT_DONE_MASK)
             == QMND_SCAN_INIT_DONE_TRUE )
        {
            if (aCodePlan->cursorProperty.mParallelReadProperties.mThreadCnt == 1)
            {
                // fix BUG-9052
                iduVarStringAppendFormat( aString,
                                          ", ACCESS: %"ID_UINT32_FMT"",
                                          (aDataPlan->plan.myTuple->modify -
                                          aDataPlan->subQFilterDepCnt) );
            }
            else
            {
                if (aDataPlan->mAccessCnt4Parallel == 0)
                {
                    aDataPlan->mAccessCnt4Parallel = 
                        ((aDataPlan->plan.myTuple->modify -
                          aDataPlan->mOrgModifyCnt) -
                         (aDataPlan->subQFilterDepCnt -
                          aDataPlan->mOrgSubQFilterDepCnt));
                }
                else
                {
                    /* nothing to do */
                }

                iduVarStringAppendFormat( aString,
                                          ", ACCESS: %"ID_UINT32_FMT"",
                                          aDataPlan->mAccessCnt4Parallel );
            }
        }
        else
        {
            iduVarStringAppend( aString,
                                ", ACCESS: 0" );
        }

        // Disk Page ���� ���
        if ( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
             == QMN_PLAN_STORAGE_DISK )
        {
            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                if ( ( ( *aDataPlan->flag & QMND_SCAN_INIT_DONE_MASK )
                                         == QMND_SCAN_INIT_DONE_TRUE ) ||
                     ( ( aCodePlan->flag & QMNC_SCAN_FOR_PARTITION_MASK )
                                        == QMNC_SCAN_FOR_PARTITION_FALSE ) )
                {
                    // Disk Table�� ���� ������ ��� ���
                    // SM���κ��� Disk Page Count�� ȹ��
                    IDE_TEST( smiGetTableBlockCount( aCodePlan->table,
                                                     & sPageCount )
                              != IDE_SUCCESS );

                    iduVarStringAppendFormat( aString,
                                              ", DISK_PAGE_COUNT: %"ID_UINT64_FMT"",
                                              sPageCount );
                }
                else
                {
                    /* BUG-44510 �̻�� Disk Partition�� SCAN Node�� ����ϴٰ�,
                     *           Page Count �κп��� ������ �����մϴ�.
                     *  Lock�� ���� �ʰ� smiGetTableBlockCount()�� ȣ���ϸ�, ������ ������ �� �ֽ��ϴ�.
                     *  �̻�� Disk Partition�� SCAN Node���� Page Count�� ������� �ʵ��� �����մϴ�.
                     */
                    iduVarStringAppendFormat( aString,
                                              ", DISK_PAGE_COUNT: ??" );
                }
            }
            else
            {
                // BUG-29209
                // DISK_PAGE_COUNT ���� �������� ����
                iduVarStringAppendFormat( aString,
                                          ", DISK_PAGE_COUNT: BLOCKED" );
            }
        }
        else
        {
            // Memory Table�� ���� ������ ��� ������� ����
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        iduVarStringAppend( aString,
                            ", ACCESS: ??" );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnSCAN::printPredicateInfo( qcTemplate   * aTemplate,
                             qmncSCAN     * aCodePlan,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // Key Range ������ ���
    IDE_TEST( printKeyRangeInfo( aTemplate,
                                 aCodePlan,
                                 aDepth,
                                 aString ) != IDE_SUCCESS );

    // Key Filter ������ ���
    IDE_TEST( printKeyFilterInfo( aTemplate,
                                  aCodePlan,
                                  aDepth,
                                  aString ) != IDE_SUCCESS );

    // Filter ������ ���
    IDE_TEST( printFilterInfo( aTemplate,
                               aCodePlan,
                               aDepth,
                               aString,
                               aMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnSCAN::printKeyRangeInfo( qcTemplate   * aTemplate,
                            qmncSCAN     * aCodePlan,
                            ULong          aDepth,
                            iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     Key Range�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt i;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    // Fixed Key Range ���
    if (sMethod->fixKeyRange4Print != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ FIXED KEY ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->fixKeyRange4Print)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Variable Key Range ���
    if (sMethod->varKeyRange != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }

        // BUG-18300
        if ( ( aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
             == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE )
        {
            iduVarStringAppend( aString,
                                " [ IN-SUBQUERY VARIABLE KEY ]\n" );
        }
        else
        {
            iduVarStringAppend( aString,
                                " [ VARIABLE KEY ]\n" );
        }

        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->varKeyRange)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // BUG-41591
    if ( sMethod->ridRange != NULL )
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString, " " );
        }

        iduVarStringAppend( aString, " [ RID FILTER ]\n" );

        IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                            aString,
                                            aDepth + 1,
                                            sMethod->ridRange )
                  != IDE_SUCCESS);

        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString, " " );
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

IDE_RC
qmnSCAN::printKeyFilterInfo( qcTemplate   * aTemplate,
                             qmncSCAN     * aCodePlan,
                             ULong          aDepth,
                             iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     Key Filter�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt i;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );

    // Fixed Key Filter ���
    if (sMethod->fixKeyFilter4Print != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ FIXED KEY FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->fixKeyFilter4Print)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Variable Key Filter ���
    if (sMethod->varKeyFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ VARIABLE KEY FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->varKeyFilter )
                 != IDE_SUCCESS);
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
qmnSCAN::printFilterInfo( qcTemplate   * aTemplate,
                          qmncSCAN     * aCodePlan,
                          ULong          aDepth,
                          iduVarString * aString,
                          qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *     Filter�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt i;
    qmncScanMethod * sMethod   = getScanMethod( aTemplate, aCodePlan );
    qmndSCAN       * sDataPlan;

    // Constant Filter ���
    if (sMethod->constantFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ CONSTANT FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->constantFilter )
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Normal Filter ���
    if (sMethod->filter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }

        /* BUG-48370 remote table fatal */
        if ( ( ( aCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_MASK ) ==
               QMNC_SCAN_REMOTE_TABLE_FALSE ) &&
             ( ( aCodePlan->flag & QMNC_SCAN_REMOTE_TABLE_STORE_MASK ) ==
               QMNC_SCAN_REMOTE_TABLE_STORE_FALSE ) &&
             ( aMode == QMN_DISPLAY_ALL ) )
        {
            sDataPlan = (qmndSCAN*)(aTemplate->tmplate.data + aCodePlan->plan.offset); /* PROJ-2632 */
            /* PROJ-2632 */
            if ( ( ( *sDataPlan->flag & QMND_SCAN_INIT_DONE_MASK ) == QMND_SCAN_INIT_DONE_TRUE ) &&
                 ( ( *sDataPlan->flag & QMND_SCAN_SERIAL_EXECUTE_MASK ) == QMND_SCAN_SERIAL_EXECUTE_TRUE ) )
            {
                if ( QCG_GET_SESSION_TRCLOG_DETAIL_INFORMATION( aTemplate->stmt ) == 0 )
                {
                    iduVarStringAppend( aString,
                                        " [ FILTER SERIAL EXECUTE ]\n" );
                }
                else
                {
                    iduVarStringAppendFormat( aString,
                                              " [ FILTER SERIAL EXECUTE, SIZE: %"ID_UINT32_FMT" ]\n",
                                              QTC_GET_SERIAL_EXECUTE_DATA_SIZE( aCodePlan->mSerialFilterSize ) );
                }
            }
            else
            {
                iduVarStringAppend( aString,
                                    " [ FILTER ]\n" );
            }
        }
        else
        {
                iduVarStringAppend( aString,
                                    " [ FILTER ]\n" );
        }

        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->filter)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Subquery Filter ���
    if (sMethod->subqueryFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ SUBQUERY FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          sMethod->subqueryFilter )
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // NNF Filter ���
    if (aCodePlan->nnfFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ NOT-NORMAL-FORM FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->nnfFilter )
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

qmncScanMethod *
qmnSCAN::getScanMethod( qcTemplate * aTemplate,
                        qmncSCAN   * aCodePlan )
{
    qmncScanMethod * sDefaultMethod = &aCodePlan->method;

    // sdf�� �޷� �ְ�, data ������ �ʱ�ȭ�Ǿ� ������
    // data ������ selected method�� ���´�.
    if( ( aTemplate->planFlag[aCodePlan->planID] &
          QMND_SCAN_SELECTED_METHOD_SET_MASK )
        == QMND_SCAN_SELECTED_METHOD_SET_TRUE )
    {
        IDE_DASSERT( aCodePlan->sdf != NULL );

        return qmo::getSelectedMethod( aTemplate,
                                       aCodePlan->sdf,
                                       sDefaultMethod );
    }
    else
    {
        return sDefaultMethod;
    }
}

IDE_RC
qmnSCAN::notifyOfSelectedMethodSet( qcTemplate * aTemplate,
                                    qmncSCAN   * aCodePlan )
{
    UInt *sFlag = &aTemplate->planFlag[aCodePlan->planID];

    *sFlag |= QMND_SCAN_SELECTED_METHOD_SET_TRUE;

    return IDE_SUCCESS;
}

IDE_RC
qmnSCAN::openCursorForPartition( qcTemplate * aTemplate,
                                 qmncSCAN   * aCodePlan,
                                 qmndSCAN   * aDataPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *
 * Implementation :
 *    - Session Event Check (������ ���� Detect)
 *    - Key Range, Key Filter, Filter ����
 *    - Cursor Open
 *
 ***********************************************************************/

    // ������ ���� �˻�
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    /* BUG-41110 */
    IDE_TEST(makeRidRange(aTemplate, aCodePlan, aDataPlan)
             != IDE_SUCCESS);

    // KeyRange, KeyFilter, Filter ����
    IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                     aCodePlan,
                                     aDataPlan ) != IDE_SUCCESS );

    // Cursor�� ����
    if ( ( *aDataPlan->flag & QMND_SCAN_CURSOR_MASK )
         != QMND_SCAN_CURSOR_OPEN )
    {
        // ó�� ���� ���
        IDE_TEST( openCursor( aTemplate, aCodePlan, aDataPlan )
                  != IDE_SUCCESS );

        // doItFirst�� cursor�� restart���� �ʴ´�.
        *aDataPlan->flag &= ~QMND_SCAN_RESTART_CURSOR_MASK;
        *aDataPlan->flag |= QMND_SCAN_RESTART_CURSOR_FALSE;
    }
    else
    {
        /* BUG-39399 remove search key preserved table
         * �̹� ���� �ִ� ���
         * join update row movement ����� �ߺ����� ������ �ϴ� ��찡
         * �ִ�. update ���� �� �ߺ� update üũ �Ͽ� ���� ó���ϱ� ������
         * ide_dassert�� ���� �Ѵ�.*/
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void
qmnSCAN::addAccessCount( qmncSCAN   * aPlan,
                         qcTemplate * aTemplate,
                         ULong        aCount )
{
    qmncSCAN     * sCodePlan  = (qmncSCAN*) aPlan;
    qcmTableInfo * sTableInfo = NULL;

    sTableInfo = sCodePlan->tableRef->tableInfo;

    if( ( sTableInfo != NULL ) &&   /* PROJ-1832 New database link */
        ( sTableInfo->tableType    != QCM_FIXED_TABLE ) &&
        ( sTableInfo->tableType    != QCM_PERFORMANCE_VIEW ) )
    {
        if( (aPlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                                   == QMN_PLAN_STORAGE_MEMORY )
        {
            // startup �ÿ� aTemplate->stmt->mStatistics�� NULL�̴�.
            if( aTemplate->stmt->mStatistics != NULL )
            {
                IDV_SQL_ADD( aTemplate->stmt->mStatistics,
                             mMemoryTableAccessCount,
                             aCount );

                IDV_SESS_ADD( aTemplate->stmt->mStatistics->mSess,
                              IDV_STAT_INDEX_MEMORY_TABLE_ACCESS_COUNT,
                              aCount );
            }
        }
    }
}

IDE_RC qmnSCAN::allocRidRange(qcTemplate* aTemplate,
                              qmncSCAN  * aCodePlan,
                              qmndSCAN  * aDataPlan)
{
    UInt            sRangeCnt;

    mtcNode*        sNode;
    mtcNode*        sNode2;

    iduMemory*      sMemory;
    qmncScanMethod* sMethod;

    sMethod = getScanMethod(aTemplate, aCodePlan);
    sMemory = QC_QMX_MEM(aTemplate->stmt);

    if (sMethod->ridRange != NULL)
    {
        IDE_ASSERT(sMethod->ridRange->node.module == &mtfOr);
        sNode = sMethod->ridRange->node.arguments;
        sRangeCnt = 0;

        while (sNode != NULL)
        {
            if (sNode->module == &mtfEqual)
            {
                sRangeCnt++;
            }
            else if (sNode->module == &mtfEqualAny)
            {
                IDE_ASSERT(sNode->arguments->next->module == &mtfList);

                sNode2 = sNode->arguments->next->arguments;
                while (sNode2 != NULL)
                {
                    sRangeCnt++;
                    sNode2 = sNode2->next;
                }
            }
            else
            {
                IDE_ASSERT(0);
            }
            sNode = sNode->next;
        }

        IDU_LIMITPOINT("qmnSCAN::allocRidRange::malloc");
        IDE_TEST(sMemory->cralloc(ID_SIZEOF(smiRange) * sRangeCnt,
                                  (void**)&aDataPlan->ridRange)
                 != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->ridRange = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::makeRidRange(qcTemplate* aTemplate,
                             qmncSCAN  * aCodePlan,
                             qmndSCAN  * aDataPlan)
{
    UInt            sRangeCnt;
    UInt            i;
    UInt            j;
    UInt            sPrevIdx;

    IDE_RC          sRc;

    mtcNode*        sNode;
    mtcNode*        sNode2;

    qtcNode*        sValueNode;
    qmncScanMethod* sMethod;
    smiRange*       sRange;

    sMethod = getScanMethod(aTemplate, aCodePlan);
    sRange  = aDataPlan->ridRange;

    if (sMethod->ridRange != NULL)
    {
        /*
         * ridRange�� ������ ���� �������� ����
         *
         * ��1) SELECT .. FROM .. WHERE _PROWID = 1 OR _PROWID = 2
         *
         *   [OR]
         *    |
         *   [=]----------[=]
         *    |            |
         *   [RID]--[1]   [RID]--[2]
         *
         * ��2) SELECT .. FROM .. WHERE _PROWID IN (1, 2)
         *
         *    [OR]
         *     |
         *   [=ANY]
         *     |
         *   [RID]--[LIST]
         *            |
         *           [1]---[2]
         *
         */

        /*
         * ridRange->node = 'OR'
         * ridRange->node.argements = '=' or '=ANY'
         */
        sNode = sMethod->ridRange->node.arguments;
        sRangeCnt = 0;

        while (sNode != NULL)
        {
            /*
             * smiRange data ����� ���
             *
             * 1) calculate �ϰ�
             * 2) converted node �� ����
             * 3) mtc::value �� �� ��������
             *
             * converted �� node �� �ݵ�� bigint �̾�� �Ѵ�.
             * SM �� �ν��ϴ� format �� scGRID(=mtdBigint) �̱� �����̴�.
             * conversion �� ���������� ����� bigint �� �ƴѰ���
             * rid scan �� �Ұ����ϰ� �̹� filter �� �з��Ǿ���.
             */

            if (sNode->module == &mtfEqual)
            {
                // BUG-41215 _prowid predicate fails to create ridRange
                if ( sNode->arguments->module == &gQtcRidModule )
                {
                    sValueNode  = (qtcNode*)(sNode->arguments->next);
                }
                else
                {
                    sValueNode  = (qtcNode*)(sNode->arguments);
                }

                sRc = QTC_TMPL_TUPLE(aTemplate, sValueNode)->
                    execute->calculate(&sValueNode->node,
                                       aTemplate->tmplate.stack,
                                       aTemplate->tmplate.stackRemain,
                                       NULL,
                                       &aTemplate->tmplate);
                IDE_TEST(sRc != IDE_SUCCESS);

                sValueNode = (qtcNode*)mtf::convertedNode( (mtcNode*)sValueNode, &aTemplate->tmplate );
                IDE_DASSERT( QTC_TMPL_COLUMN(aTemplate, sValueNode)->module == &mtdBigint );

                sRange[sRangeCnt].minimum.callback = mtk::rangeCallBack4Rid;
                sRange[sRangeCnt].maximum.callback = mtk::rangeCallBack4Rid;

                sRange[sRangeCnt].minimum.data =
                    (void*)mtc::value( QTC_TMPL_COLUMN(aTemplate, sValueNode),
                                       QTC_TMPL_TUPLE(aTemplate, sValueNode)->row,
                                       MTD_OFFSET_USE );

                sRange[sRangeCnt].maximum.data = sRange[sRangeCnt].minimum.data;
                sRangeCnt++;
            }
            else if (sNode->module == &mtfEqualAny)
            {
                sNode2 = sNode->arguments->next->arguments;
                while (sNode2 != NULL)
                {
                    sRc = QTC_TMPL_TUPLE(aTemplate, (qtcNode*)sNode2)->
                        execute->calculate(sNode2,
                                           aTemplate->tmplate.stack,
                                           aTemplate->tmplate.stackRemain,
                                           NULL,
                                           &aTemplate->tmplate);
                    IDE_TEST(sRc != IDE_SUCCESS);

                    sValueNode = (qtcNode*)mtf::convertedNode( sNode2, &aTemplate->tmplate );
                    IDE_DASSERT( QTC_TMPL_COLUMN(aTemplate, sValueNode)->module == &mtdBigint );

                    sRange[sRangeCnt].minimum.callback = mtk::rangeCallBack4Rid;
                    sRange[sRangeCnt].maximum.callback = mtk::rangeCallBack4Rid;

                    sRange[sRangeCnt].minimum.data =
                        (void*)mtc::value( QTC_TMPL_COLUMN(aTemplate, sValueNode),
                                           QTC_TMPL_TUPLE(aTemplate, sValueNode)->row,
                                           MTD_OFFSET_USE );

                    sRange[sRangeCnt].maximum.data = sRange[sRangeCnt].minimum.data;
                    sRangeCnt++;

                    sNode2 = sNode2->next;
                }
            }
            else
            {
                IDE_DASSERT(0);
            }

            sNode = sNode->next;
        }

        sPrevIdx = 0;
        sRange[sPrevIdx].next = NULL;
        sRange[sPrevIdx].prev = NULL;

        for (i = 1; i < sRangeCnt; i++)
        {
            /*
             * BUG-41211 �ߺ��� range ����
             */
            for (j = 0; j <= sPrevIdx; j++)
            {
                if (idlOS::memcmp(sRange[i].minimum.data,
                                  sRange[j].minimum.data,
                                  ID_SIZEOF(mtdBigintType)) == 0)
                {
                    break;
                }
            }

            if (j == sPrevIdx + 1)
            {
                sRange[sPrevIdx].next = &sRange[i];
                sRange[i].prev        = &sRange[sPrevIdx];
                sPrevIdx = i;
            }
            else
            {
                /* skip this range */
            }
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

#define QMN_SCAN_SUBQUERY_PREDICATE_MAX (4)

void qmnSCAN::resetExecInfo4Subquery(qcTemplate *aTemplate, qmnPlan *aPlan)
{
/***********************************************************************
 *
 * Description :
 *
 *    BUG-31378
 *    template �� �ִ� execInfo[] �迭 ��
 *    subquery node �� �ش��ϴ� ���Ҹ� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt             i;

    /*   
     * qmncSCAN ���� method �� ��� ���ؼ� ���� qmncSCAN::method �� �����ؼ��� �ȵȴ�.
     * qmnSCAN::getScanMethod() �� �̿��ؾ� �Ѵ�.
     */
    qmncSCAN        *sCodePlan = (qmncSCAN *)aPlan;
    qmncScanMethod  *sMethod = qmnSCAN::getScanMethod(aTemplate, sCodePlan);

    qtcNode  *sOutterNode;
    qtcNode  *sInnerNode;
    qtcNode  *sSubqueryWrapperNode;

    /*   
     * qmnSCAN::printLocalPlan() ���� �ּ��� ���ϸ�
     * Subquery�� ������ ���� predicate���� ������ �� �ִ� :
     *     1. Variable Key Range
     *     2. Variable Key Filter
     *     3. Constant Filter
     *     4. Subquery Filter
     */
    qtcNode  *sNode[QMN_SCAN_SUBQUERY_PREDICATE_MAX] = {NULL,};

    sNode[0] = sMethod->varKeyRange;
    sNode[1] = sMethod->varKeyFilter;
    sNode[2] = sMethod->constantFilter;
    sNode[3] = sMethod->subqueryFilter;
    /*
     * method �Ʒ��� �޸��� qtcNode ���� ������ �Ʒ��� ����.
     * () ���� qtcNode �� module name
     *
     * varKeyRange --- qtcNode                      : sNode[i]
     *                  (OR)
     *                    |
     *                 qtcNode                      : sOutterNode
     *                  (AND)
     *                    |
     *                 qtcNode                      : sInnerNode
     *              (=, <, >, ...)
     *                    |
     *                 qtcNode ------------- qtcNode  <--- (1)
     *           (COLUMN, VALUE, ...)   (SUBQUERY_WRAPPER)
     *                                          |
     *                                       qtcNode
     *                                      (SUBQUERY)
     *                                          |
     *                                       qtcNode
     *                                       (COLUMN)
     *
     * �� �Լ����� ã���� �ϴ� ���� SUBQUERY_WRAPPER ���(1��)�̴�.
     */
    for (i = 0; i < QMN_SCAN_SUBQUERY_PREDICATE_MAX; i++)
    {
        if (sNode[i] == NULL)
        {
            continue;
        }
        else
        {
            // nothing to do
        }
        /*
         * �Ʒ��� ��ø for loop �� qmoKeyRange::calculateSubqueryInRangeNode() �Լ�����
         * �״�� ������ ����.
         */
        for (sOutterNode = (qtcNode *)sNode[i]->node.arguments;
             sOutterNode != NULL;
             sOutterNode = (qtcNode *)sOutterNode->node.next)
        {
            for (sInnerNode = (qtcNode *)sOutterNode->node.arguments;
                 sInnerNode != NULL;
                 sInnerNode = (qtcNode *)sInnerNode->node.next)
            {
                if ((sInnerNode->lflag & QTC_NODE_SUBQUERY_RANGE_MASK)
                    == QTC_NODE_SUBQUERY_RANGE_TRUE)
                {
                    if (sNode[i]->indexArgument == 0)
                    {
                        sSubqueryWrapperNode = (qtcNode*)sInnerNode->node.arguments->next;
                    }
                    else
                    {
                        sSubqueryWrapperNode = (qtcNode*)sInnerNode->node.arguments;
                    }

                    if (sSubqueryWrapperNode->node.module == &qtc::subqueryWrapperModule)
                    {
                        /*
                         * BINGGO!!
                         */
                        aTemplate->tmplate.execInfo[sSubqueryWrapperNode->node.info] =
                                                        QTC_WRAPPER_NODE_EXECUTE_FALSE;
                    }
                    else
                    {
                    }
                }
                else
                {
                }
            }
        }
    }
}

IDE_RC qmnSCAN::doItFirstFixedTable( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan,
                                     qmcRowFlag * aFlag )
{
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);
    UInt                sModifyCnt;
    smiTrans          * sSmiTrans;
    mtkRangeCallBack  * sData;
    smiRange          * sRange;

    sDataPlan->plan.mTID = QMN_PLAN_INIT_THREAD_ID;

    // ���� ACCESS count
    sModifyCnt = sDataPlan->plan.myTuple->modify;

    // ----------------
    // Tuple ��ġ�� ����
    // ----------------
    sDataPlan->plan.myTuple = &aTemplate->tmplate.rows[sCodePlan->tupleRowID];

    // ACCESS count ����
    sDataPlan->plan.myTuple->modify = sModifyCnt;

    // ������ ���� �˻�
    IDE_TEST(iduCheckSessionEvent(aTemplate->stmt->mStatistics)
             != IDE_SUCCESS);

    IDE_TEST(makeRidRange(aTemplate, sCodePlan, sDataPlan)
             != IDE_SUCCESS);

    // KeyRange, KeyFilter, Filter ����
    IDE_TEST(makeKeyRangeAndFilter(aTemplate, sCodePlan, sDataPlan)
             != IDE_SUCCESS);

    sSmiTrans = QC_SMI_STMT( aTemplate->stmt )->getTrans();
    if ( ( *sDataPlan->flag & QMND_SCAN_CURSOR_MASK )
         == QMND_SCAN_CURSOR_CLOSED )
    {
        sDataPlan->fixedTable.memory.initialize( QC_QMX_MEM( aTemplate->stmt ) );

        sDataPlan->fixedTableProperty.mFirstReadRecordPos = sDataPlan->cursorProperty.mFirstReadRecordPos;
        sDataPlan->fixedTableProperty.mReadRecordCount = sDataPlan->cursorProperty.mReadRecordCount;
        sDataPlan->fixedTableProperty.mStatistics = aTemplate->stmt->mStatistics;
        sDataPlan->fixedTableProperty.mFilter = &sDataPlan->callBack;
        /* BUG-43006 FixedTable Indexing Filter */
        sDataPlan->fixedTableProperty.mKeyRange = sDataPlan->keyRange;

        if ( sDataPlan->keyRange != smiGetDefaultKeyRange() )
        {
            sData = (mtkRangeCallBack *)sDataPlan->keyRange->minimum.data;
            sDataPlan->fixedTableProperty.mMinColumn = &sData->columnDesc.column;

            sData = (mtkRangeCallBack *)sDataPlan->keyRange->maximum.data;
            sDataPlan->fixedTableProperty.mMaxColumn = &sData->columnDesc.column;

            for ( sRange = sDataPlan->fixedTableProperty.mKeyRange->next;
                  sRange != NULL;
                  sRange = sRange->next )
            {
                sData = (mtkRangeCallBack *)sRange->minimum.data;
                sData->columnDesc.column.offset = 0;
                sData = (mtkRangeCallBack *)sRange->maximum.data;
                sData->columnDesc.column.offset = 0;
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sSmiTrans != NULL )
        {
            sDataPlan->fixedTableProperty.mTrans = sSmiTrans->mTrans;
        }
        else
        {
            /* Nothing to do */
        }

        sDataPlan->fixedTable.memory.setContext( &sDataPlan->fixedTableProperty );

        IDE_TEST( smiFixedTable::build( aTemplate->stmt->mStatistics,
                                        (void *)sCodePlan->table,
                                        sCodePlan->dumpObject,
                                        &sDataPlan->fixedTable.memory,
                                        &sDataPlan->fixedTable.recRecord,
                                        &sDataPlan->fixedTable.traversePtr )
                  != IDE_SUCCESS );

        *sDataPlan->flag &= ~QMND_SCAN_CURSOR_MASK;
        *sDataPlan->flag |= QMND_SCAN_CURSOR_OPEN;
    }
    else
    {
        sDataPlan->fixedTable.memory.restartInit();

        sDataPlan->fixedTableProperty.mFirstReadRecordPos = sDataPlan->cursorProperty.mFirstReadRecordPos;
        sDataPlan->fixedTableProperty.mReadRecordCount = sDataPlan->cursorProperty.mReadRecordCount;
        sDataPlan->fixedTableProperty.mStatistics = aTemplate->stmt->mStatistics;
        sDataPlan->fixedTableProperty.mFilter = &sDataPlan->callBack;

        /* BUG-43006 FixedTable Indexing Filter */
        sDataPlan->fixedTableProperty.mKeyRange = sDataPlan->keyRange;
        if ( sDataPlan->keyRange != smiGetDefaultKeyRange() )
        {
            sData = (mtkRangeCallBack *)sDataPlan->keyRange->minimum.data;
            sDataPlan->fixedTableProperty.mMinColumn = &sData->columnDesc.column;

            sData = (mtkRangeCallBack *)sDataPlan->keyRange->maximum.data;
            sDataPlan->fixedTableProperty.mMaxColumn = &sData->columnDesc.column;

            for ( sRange = sDataPlan->fixedTableProperty.mKeyRange->next;
                  sRange != NULL;
                  sRange = sRange->next )
            {
                sData = (mtkRangeCallBack *)sRange->minimum.data;
                sData->columnDesc.column.offset = 0;
                sData = (mtkRangeCallBack *)sRange->maximum.data;
                sData->columnDesc.column.offset = 0;
            }
        }
        else
        {
            /* Nothing to do */
        }
        if ( sSmiTrans != NULL )
        {
            sDataPlan->fixedTableProperty.mTrans = sSmiTrans->mTrans;
        }
        else
        {
            /* Nothing to do */
        }

        sDataPlan->fixedTable.memory.setContext( &sDataPlan->fixedTableProperty );

        IDE_TEST( smiFixedTable::build( aTemplate->stmt->mStatistics,
                                        (void *)sCodePlan->table,
                                        sCodePlan->dumpObject,
                                        &sDataPlan->fixedTable.memory,
                                        &sDataPlan->fixedTable.recRecord,
                                        &sDataPlan->fixedTable.traversePtr )
                  != IDE_SUCCESS );
    }

    // Record�� ȹ���Ѵ�.
    IDE_TEST( readRowFixedTable( aTemplate, sCodePlan, sDataPlan, aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnSCAN::doItNextFixedTable;
    }
    else
    {
        sDataPlan->doIt = qmnSCAN::doItFirstFixedTable;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::doItNextFixedTable( qcTemplate * aTemplate,
                                    qmnPlan    * aPlan,
                                    qmcRowFlag * aFlag )
{
    qmncSCAN * sCodePlan = (qmncSCAN*) aPlan;
    qmndSCAN * sDataPlan =
        (qmndSCAN*) (aTemplate->tmplate.data + aPlan->offset);

    // Record�� ȹ���Ѵ�.
    IDE_TEST( readRowFixedTable( aTemplate, sCodePlan, sDataPlan, aFlag )
              != IDE_SUCCESS );

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
    {
        sDataPlan->doIt = qmnSCAN::doItNextFixedTable;
    }
    else
    {
        sDataPlan->doIt = qmnSCAN::doItFirstFixedTable;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSCAN::readRowFixedTable( qcTemplate * aTemplate,
                                   qmncSCAN   * aCodePlan,
                                   qmndSCAN   * aDataPlan,
                                   qmcRowFlag * aFlag )
{
    idBool           sJudge;
    void           * sOrgRow;
    void           * sSearchRow;
    qmncScanMethod * sMethod = getScanMethod( aTemplate, aCodePlan );
    smiFixedTableRecord * sCurRec;

    sJudge = ID_FALSE;

    while ( sJudge == ID_FALSE )
    {
        sOrgRow = sSearchRow = aDataPlan->plan.myTuple->row;

        IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
                  != IDE_SUCCESS );

        if ( aDataPlan->fixedTable.traversePtr == NULL )
        {
            sSearchRow = NULL;
        }
        else
        {
            sSearchRow = smiFixedTable::getRecordPtr( aDataPlan->fixedTable.traversePtr );
            sCurRec = ( smiFixedTableRecord * )aDataPlan->fixedTable.traversePtr;

            if ( sCurRec != NULL )
            {
                aDataPlan->fixedTable.traversePtr = ( UChar *)sCurRec->mNext;
            }
            else
            {
                aDataPlan->fixedTable.traversePtr = NULL;
            }
        }

        if ( sSearchRow == NULL )
        {
            aDataPlan->plan.myTuple->row = sOrgRow;
        }
        else
        {
            aDataPlan->plan.myTuple->row = sSearchRow;
        }

        if ( sSearchRow == NULL )
        {
            sJudge = ID_FALSE;
            break;
        }
        else
        {
            // modify�� ����
            if ( sMethod->filter == NULL )
            {
                // modify�� ����.
                aDataPlan->plan.myTuple->modify++;
            }
            else
            {
                // SM�� ���Ͽ� filter���� �� modify ���� ������.

                // fix BUG-9052 BUG-9248

                // BUG-9052
                // SELECT * FROM T1 WHERE T1.I2 IN ( SELECT MAX(T3.I2)
                //                       FROM T2, T3
                //                       WHERE T2.I1 = T3.I1
                //                       AND T3.I1 = T1.I1
                //                       GROUP BY T2.I2 ) AND T1.I1 > 0;
                // subquery filter�� outer column ������
                // outer column�� ������ store and search��
                // ������ϵ��� �ϱ� ����
                // aDataPlan->plan.myTuple->modify��
                // aDataPlan->subQFilterDepCnt�� ���� ������Ų��.
                // printPlan()������ ACCESS count display��
                // DataPlan->plan.myTuple->modify����
                // DataPlan->subQFilterDepCnt ���� ���ֵ��� �Ѵ�.

                // BUG-9248
                // subquery filter�̿ܿ��� modify count���� �����ؼ�
                // �߰������ �������ؾ��ϴ� ��찡 ����

                aDataPlan->plan.myTuple->modify++;
                aDataPlan->subQFilterDepCnt++;
            }


            // Subquery Filter�� ����
            if ( sMethod->subqueryFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      sMethod->subqueryFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                sJudge = ID_TRUE;
            }

            // NNF Filter�� ����
            if ( aCodePlan->nnfFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      aCodePlan->nnfFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    if ( sJudge == ID_TRUE )
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_EXIST;
    }
    else
    {
        // �����ϴ� Record ����
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

