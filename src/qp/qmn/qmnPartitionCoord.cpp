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
 * $Id: qmnPartitionCoord.cpp 20233 2007-02-01 01:58:21Z shsuh $
 *
 * Description :
 *     Partition Coordinator(PCRD) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmnPartitionCoord.h>
#include <qmnScan.h>
#include <qmoUtil.h>
#include <qcuProperty.h>
#include <qmoPartition.h>
#include <qcg.h>

extern "C" int
compareChildrenIndex( const void* aElem1, const void* aElem2 )
{
/***********************************************************************
 *
 * Description :
 *    compare qcmColumn
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( aElem1 != NULL );
    IDE_DASSERT( aElem2 != NULL );

    //--------------------------------
    // compare partition OID
    //--------------------------------

    if( ((qmnChildrenIndex*)aElem1)->childOID >
        ((qmnChildrenIndex*)aElem2)->childOID )
    {
        return 1;
    }
    else if( ((qmnChildrenIndex*)aElem1)->childOID <
             ((qmnChildrenIndex*)aElem2)->childOID )
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

IDE_RC
qmnPCRD::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    PCRD ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncPCRD * sCodePlan = (qmncPCRD *) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD *) (aTemplate->tmplate.data + aPlan->offset);

    idBool        sJudge;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aPlan->left     == NULL );
    IDE_DASSERT( aPlan->right    == NULL );

    //---------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    sDataPlan->doIt = qmnPCRD::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_PCRD_INIT_DONE_MASK)
         == QMND_PCRD_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------------
    // Child Plan�� �ʱ�ȭ
    //------------------------------------------------

    if( sCodePlan->selectedPartitionCount > 0 )
    {
        IDE_DASSERT( sCodePlan->plan.children != NULL );

        sDataPlan->selectedChildrenCount = sCodePlan->selectedPartitionCount;

        sJudge = ID_TRUE;
    }
    else
    {
        sJudge = ID_FALSE;
    }

    if( sJudge == ID_TRUE )
    {
        if( sCodePlan->constantFilter != NULL )
        {
            IDE_TEST( qtc::judge( &sJudge,
                                  sCodePlan->constantFilter,
                                  aTemplate )
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

    if( sJudge == ID_TRUE )
    {
        //------------------------------------------------
        // ���� �Լ� ����
        //------------------------------------------------
        sDataPlan->doIt = qmnPCRD::doItFirst;

        *sDataPlan->flag &= ~QMND_PCRD_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_PCRD_ALL_FALSE_FALSE;

        // TODO1502:
        //  update, delete�� ���� flag�����ؾ� ��.
    }
    else
    {
        sDataPlan->doIt = qmnPCRD::doItAllFalse;

        *sDataPlan->flag &= ~QMND_PCRD_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_PCRD_ALL_FALSE_TRUE;

        // update, delete�� ���� flag�����ؾ���.
    }

    //------------------------------------------------
    // ���� Data Flag �� �ʱ�ȭ
    //------------------------------------------------

    // Subquery���ο� ���ԵǾ� ���� ��� �ʱ�ȭ�Ͽ�
    // In Subquery Key Range�� �ٽ� ������ �� �ֵ��� �Ѵ�.
    *sDataPlan->flag &= ~QMND_PCRD_INSUBQ_RANGE_BUILD_MASK;
    *sDataPlan->flag |= QMND_PCRD_INSUBQ_RANGE_BUILD_SUCCESS;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    PCRD�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ���� Child Plan �� �����ϰ�, ���� ��� ���� child plan�� ����
 *
 ***********************************************************************/

    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );
}

IDE_RC
qmnPCRD::doItNext( qcTemplate * aTemplate,
                   qmnPlan    * aPlan,
                   qmcRowFlag * aFlag )
{
    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    if ( ( sCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
         == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE )
    {
        // ���� Record�� �д´�.
        IDE_TEST( readRowWithIndex( aTemplate, sCodePlan, sDataPlan, aFlag )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���� Record�� �д´�.
        IDE_TEST( readRow( aTemplate, sCodePlan, sDataPlan, aFlag )
                  != IDE_SUCCESS );
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnPCRD::doItFirst;
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
qmnPCRD::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    // �ʱ�ȭ�� ������� ���� ��� �ʱ�ȭ�� ����
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_PCRD_INIT_DONE_MASK)
         == QMND_PCRD_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    qmx::copyMtcTupleForPartitionedTable( sDataPlan->plan.myTuple,
                                          &(aTemplate->tmplate.rows[sCodePlan->partitionedTupleID]) );

    if ( ( sDataPlan->plan.myTuple->lflag & MTC_TUPLE_STORAGE_MASK )
                                        == MTC_TUPLE_STORAGE_DISK )
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

            // Null Row�� ���� ���� �Ҵ�
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                          sDataPlan->plan.myTuple->rowOffset,
                          (void**) & sDataPlan->nullRow ) != IDE_SUCCESS );

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

        sDataPlan->plan.myTuple->row = sDataPlan->diskRow;

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
        //-----------------------------------
        // Memory Table�� ���
        //-----------------------------------

        // NULL ROW�� �����´�.
        IDE_TEST( smiGetTableNullRow( sDataPlan->plan.myTuple->tableHandle,
                                      (void **) & sDataPlan->plan.myTuple->row,
                                      & sDataPlan->plan.myTuple->rid )
                  != IDE_SUCCESS );

        // ���ռ� �˻�
        IDE_DASSERT( sDataPlan->plan.myTuple->row != NULL );

    }

    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong i;

    qmnChildren * sChildren;
    qcmIndex    * sIndex;

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //----------------------------
    // PartSCAN ��� ǥ��
    //----------------------------

    iduVarStringAppend( aString,
                        "PARTITION-COORDINATOR ( TABLE: " );

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
        iduVarStringAppend( aString, " " );

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

    //----------------------------
    // PARTITION ���� ���.
    //----------------------------
    iduVarStringAppendFormat( aString,
                              ", PARTITION: %"ID_UINT32_FMT"/%"ID_UINT32_FMT"",
                              sCodePlan->selectedPartitionCount,
                              sCodePlan->partitionCount );

    //----------------------------
    // Access Method ���
    //----------------------------

    // BUG-38192
    // global, local ���о��� ���õ� index �� ���

    sIndex = sCodePlan->index;

    if ( sIndex != NULL )
    {
        // To fix BUG-12742
        // explain plan = on�� ��� ���� ���� plan�� ����ϹǷ�
        // dataPlan�� keyrange�� ���� index��� ���θ� ����.
        // explain plan = only�� ��� optimize���� plan�� ����ϹǷ�
        // codePlan�� fix/variable keyrange�� ���� index��� ���θ� ����.

        if ( aMode == QMN_DISPLAY_ALL )
        {
            //----------------------------
            // explain plan = on; �� ���
            //----------------------------

            // IN SUBQUERY KEYRANGE�� �ִ� ���:
            // keyrange����� �����ϸ� fetch�����̹Ƿ�,
            // keyrange�� ������ index�� ����Ѵ�.
        }
        else
        {
            //----------------------------
            // explain plan = only; �� ���
            //----------------------------

            if ( ( ( sCodePlan->flag & QMNC_SCAN_FORCE_INDEX_SCAN_MASK )
                   == QMNC_SCAN_FORCE_INDEX_SCAN_FALSE ) &&
                 ( sCodePlan->fixKeyRange == NULL ) &&
                 ( sCodePlan->varKeyRange == NULL ) )
            {
                sIndex = NULL;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sIndex != NULL )
        {
            // Index�� ���� ���
            iduVarStringAppend( aString, ", INDEX: " );

            iduVarStringAppend( aString, sIndex->userName );
            iduVarStringAppend( aString, "." );
            iduVarStringAppend( aString, sIndex->name );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Full Scan�� ���
        iduVarStringAppend(aString, ", FULL SCAN");
    }

    IDE_TEST( printAccessInfo( sDataPlan,
                               aString,
                               aMode ) != IDE_SUCCESS );

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }

        // sCodePlan �� ���� ����ϱ⶧���� qmn::printMTRinfo�� ������� ���Ѵ�.
        iduVarStringAppendFormat( aString,
                                  "[ SELF NODE INFO, "
                                  "SELF: %"ID_INT32_FMT" ]\n",
                                  (SInt)sCodePlan->tupleRowID );
    }

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------

    if (QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1)
    {
        IDE_TEST( printPredicateInfo( aTemplate,
                                      sCodePlan,
                                      aDepth,
                                      aString ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Variable Key Range�� Subquery ���� ���
    if ( sCodePlan->varKeyRange != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->varKeyRange,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // Variable Key Filter�� Subquery ���� ���
    if ( sCodePlan->varKeyFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->varKeyFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // subquery ������ ���.
    // subquery�� constant filter, nnf filter, subquery filter���� �ִ�.
    // Constant Filter�� Subquery ���� ���
    if ( sCodePlan->constantFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->constantFilter,
                                          aDepth,
                                          aString,
                                          aMode ) != IDE_SUCCESS );
    }

    // Subquery Filter�� Subquery ���� ���
    if ( sCodePlan->subqueryFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->subqueryFilter,
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

    if ( QCU_TRCLOG_RESULT_DESC == 1 )
    {
        IDE_TEST( qmn::printResult( aTemplate,
                                    aDepth,
                                    aString,
                                    sCodePlan->plan.resultDesc )
                  != IDE_SUCCESS );
    }

    //----------------------------
    // Child Plan�� ���� ���
    //----------------------------

    // BUG-38192

    if ( QCU_TRCLOG_DISPLAY_CHILDREN == 1 )
    {
        for ( sChildren = sCodePlan->plan.children;
              sChildren != NULL;
              sChildren = sChildren->next )
        {
            IDE_TEST( sChildren->childPlan->printPlan( aTemplate,
                                                       sChildren->childPlan,
                                                       aDepth + 1,
                                                       aString,
                                                       aMode )
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

IDE_RC
qmnPCRD::doItDefault( qcTemplate * /* aTemplate */,
                      qmnPlan    * /* aPlan */,
                      qmcRowFlag * /* aFlag */)
{
/***********************************************************************
 *
 * Description :
 *    ȣ��Ǿ�� �ȵ�
 *
 * Implementation :
 *
 ***********************************************************************/

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::doItAllFalse( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    partition pruning�� ���� �����ϴ� partition�� �ϳ��� ���� ��� ���
 *    Filter ������ �����ϴ� Record�� �ϳ��� ���� ��� ���
 *
 *    Constant Filter �˻��Ŀ� �����Ǵ� �Լ��� ���� �����ϴ�
 *    Record�� �������� �ʴ´�.
 *
 * Implementation :
 *    �׻� record ������ �����Ѵ�.
 *
 ***********************************************************************/

    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    // BUG-48800
    if ( sCodePlan->mPrePruningPartHandle != NULL )
    {
        IDE_TEST( smiValidateObjects( sCodePlan->mPrePruningPartHandle,
                                      sCodePlan->mPrePruningPartSCN )
                  != IDE_SUCCESS );
    }

    // BUG-47599
    // emptyPartiton�� lock�� �ɸ� add partition op�� lock �浹�� �߻��մϴ�.
    if ( sCodePlan->tableRef->mEmptyPartRef != NULL )
    {
        IDE_TEST( smiValidateObjects( sCodePlan->tableRef->mEmptyPartRef->partitionHandle,
                                      sCodePlan->tableRef->mEmptyPartRef->partitionSCN )
                  != IDE_SUCCESS);
    }

    // ���ռ� �˻�
    IDE_DASSERT( ( sCodePlan->selectedPartitionCount == 0 ) ||
                 ( sCodePlan->constantFilter != NULL ) );
    IDE_DASSERT( ( *sDataPlan->flag & QMND_PCRD_ALL_FALSE_MASK )
                 == QMND_PCRD_ALL_FALSE_TRUE );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::firstInit( qcTemplate * aTemplate,
                    qmncPCRD   * aCodePlan,
                    qmndPCRD   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 *    firstInit()�� ȣ���� �� Partitioned Table Tuple�� ����ִ� ����,
 *    ���ʿ��� Partitioned Table�� ���̳�, ���Ŀ��� Table Partition�� ���̴�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmnCursorInfo       * sCursorInfo;
    UInt                  sRowOffset;

    //--------------------------------
    // ���ռ� �˻�
    //--------------------------------

    // Key Range�� Fixed KeyRange�� Variable KeyRange�� ȥ��� �� ����.
    IDE_DASSERT( (aCodePlan->fixKeyRange == NULL) ||
                 (aCodePlan->varKeyRange == NULL) );

    // Key Filter�� Fixed KeyFilter�� Variable KeyFilter�� ȥ��� �� ����.
    IDE_DASSERT( (aCodePlan->fixKeyFilter == NULL) ||
                 (aCodePlan->varKeyFilter == NULL) );

    //---------------------------------
    // PCRD ���� ������ �ʱ�ȭ
    //---------------------------------

    // Tuple ��ġ�� ����
    aDataPlan->plan.myTuple =
        & aTemplate->tmplate.rows[aCodePlan->tupleRowID];

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

    if ( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
         == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE )
    {
        // Index Tuple ��ġ�� ����
        aDataPlan->indexTuple =
            & aTemplate->tmplate.rows[aCodePlan->indexTupleRowID];

        // Cursor Property�� ����
        // Session Event ������ �����ϱ� ���Ͽ� �����ؾ� �Ѵ�.
        SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN( &(aDataPlan->cursorProperty), NULL );

        aDataPlan->cursorProperty.mStatistics = aTemplate->stmt->mStatistics;
    }
    else
    {
        aDataPlan->indexTuple = NULL;
    }

    // fix BUG-9052
    aDataPlan->subQFilterDepCnt = 0;

    //---------------------------------
    // Predicate ������ �ʱ�ȭ
    //---------------------------------

    // partition filter ������ �Ҵ�

    IDE_TEST( allocPartitionFilter( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

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

    aDataPlan->partitionFilter = NULL;
    aDataPlan->keyRange = NULL;
    aDataPlan->keyFilter = NULL;

    //---------------------------------
    // Partitioned Table ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->tupleRowID )
              != IDE_SUCCESS );

    aDataPlan->nullRow = NULL;
    aDataPlan->curChildNo = 0;
    aDataPlan->selectedChildrenCount = 0;

    if( aCodePlan->selectedPartitionCount > 0 )
    {
        // children area�� ����.
        IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                      aCodePlan->selectedPartitionCount * ID_SIZEOF(qmnChildren*),
                      (void**)&aDataPlan->childrenArea )
                  != IDE_SUCCESS );

        // intersect count array create
        IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                      aCodePlan->selectedPartitionCount * ID_SIZEOF(UInt),
                      (void**)&aDataPlan->rangeIntersectCountArray)
                  != IDE_SUCCESS );

        if ( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
             == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE )
        {
            // children index�� ����.
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          aCodePlan->selectedPartitionCount * ID_SIZEOF(qmnChildrenIndex),
                          (void**)&aDataPlan->childrenIndex )
                      != IDE_SUCCESS );
        }
        else
        {
            aDataPlan->childrenIndex = NULL;
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
         == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE )
    {
        // index table scan�� ���� row �Ҵ�
        IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                                   & aTemplate->tmplate,
                                   aCodePlan->indexTupleRowID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------
    // cursor ���� ����
    //---------------------------------

    // PROJ-2205 rownum in DML
    // row movement update�� ���
    if ( aDataPlan->plan.myTuple->cursorInfo != NULL )
    {
        // DML�� ������ ����� ���
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

        aDataPlan->isRowMovementUpdate = sCursorInfo->isRowMovementUpdate;
    }
    else
    {
        aDataPlan->updateColumnList = NULL;
        aDataPlan->cursorType       = SMI_SELECT_CURSOR;
        aDataPlan->inplaceUpdate    = ID_FALSE;
        aDataPlan->lockMode         = SMI_LOCK_READ;

        aDataPlan->isRowMovementUpdate = ID_FALSE;
    }

    aDataPlan->isNeedAllFetchColumn = ID_FALSE;

    /* PROJ-2464 hybrid partitioned table ����
     *  Partitioned Tuple�� Disk�� ���, Row Offset ��ŭ �޸𸮸� �Ҵ��Ѵ�.
     */
    if ( ( aTemplate->tmplate.rows[aCodePlan->tupleRowID].lflag & MTC_TUPLE_STORAGE_MASK )
                                                              == MTC_TUPLE_STORAGE_DISK )
    {
        sRowOffset = aTemplate->tmplate.rows[aCodePlan->partitionedTupleID].rowOffset;

        // ���ռ� �˻�
        IDE_DASSERT( sRowOffset > 0 );

        // Disk Row�� ���� ���� �Ҵ�
        IDU_FIT_POINT( "qmnPCRD::firstInit::alloc::diskRow", idERR_ABORT_InsufficientMemory );
        IDE_TEST( aTemplate->stmt->qmxMem->alloc( sRowOffset,
                                                  (void **) &aDataPlan->diskRow )
                  != IDE_SUCCESS );

        /* BUG-42380 setTuple����, ������ ���� Partitioned Table�� Tuple�� �����Ѵ�. */
        aTemplate->tmplate.rows[aCodePlan->partitionedTupleID].row = aDataPlan->diskRow;
    }
    else
    {
        aDataPlan->diskRow = NULL;
    }

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_PCRD_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_PCRD_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::printPredicateInfo( qcTemplate   * aTemplate,
                             qmncPCRD     * aCodePlan,
                             ULong          aDepth,
                             iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    // partition Key Range ������ ���
    IDE_TEST( printPartKeyRangeInfo( aTemplate,
                                     aCodePlan,
                                     aDepth,
                                     aString ) != IDE_SUCCESS );

    // partition Filter ������ ���
    IDE_TEST( printPartFilterInfo( aTemplate,
                                   aCodePlan,
                                   aDepth,
                                   aString ) != IDE_SUCCESS );

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
                               aString ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::printPartKeyRangeInfo( qcTemplate   * aTemplate,
                                qmncPCRD     * aCodePlan,
                                ULong          aDepth,
                                iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     partition Key Range�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    UInt i;

    // Fixed Key Range ���
    if (aCodePlan->partitionKeyRange != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ PARTITION KEY ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->partitionKeyRange)
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
qmnPCRD::printPartFilterInfo( qcTemplate   * aTemplate,
                              qmncPCRD     * aCodePlan,
                              ULong          aDepth,
                              iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *     partition Filter�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt i;

    // Fixed Key Filter ���
    if (aCodePlan->partitionFilter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ PARTITION FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->partitionFilter )
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
qmnPCRD::printKeyRangeInfo( qcTemplate   * aTemplate,
                            qmncPCRD     * aCodePlan,
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

    // Fixed Key Range ���
    if (aCodePlan->fixKeyRange4Print != NULL)
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
                                          aCodePlan->fixKeyRange4Print)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Variable Key Range ���
    if (aCodePlan->varKeyRange != NULL)
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
                                          aCodePlan->varKeyRange)
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
qmnPCRD::printKeyFilterInfo( qcTemplate   * aTemplate,
                             qmncPCRD     * aCodePlan,
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

    // Fixed Key Filter ���
    if (aCodePlan->fixKeyFilter4Print != NULL)
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
                                          aCodePlan->fixKeyFilter4Print)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Variable Key Filter ���
    if (aCodePlan->varKeyFilter != NULL)
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
                                          aCodePlan->varKeyFilter )
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
qmnPCRD::printFilterInfo( qcTemplate   * aTemplate,
                          qmncPCRD     * aCodePlan,
                          ULong          aDepth,
                          iduVarString * aString )
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

    // Constant Filter ���
    if (aCodePlan->constantFilter != NULL)
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
                                          aCodePlan->constantFilter )
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Normal Filter ���
    if (aCodePlan->filter != NULL)
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString,
                                " " );
        }
        iduVarStringAppend( aString,
                            " [ FILTER ]\n" );
        IDE_TEST(qmoUtil::printPredInPlan(aTemplate,
                                          aString,
                                          aDepth + 1,
                                          aCodePlan->filter)
                 != IDE_SUCCESS);
    }
    else
    {
        // Nothing To Do
    }

    // Subquery Filter ���
    if (aCodePlan->subqueryFilter != NULL)
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
                                          aCodePlan->subqueryFilter )
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

IDE_RC
qmnPCRD::allocPartitionFilter( qcTemplate * aTemplate,
                               qmncPCRD   * aCodePlan,
                               qmndPCRD   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Partition Filter�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    iduMemory * sMemory;

    if ( aCodePlan->partitionFilter != NULL )
    {
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           aCodePlan->partitionFilter,
                                           & aDataPlan->partitionFilterSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->partitionFilterSize > 0 );

        sMemory = aTemplate->stmt->qmxMem;

        IDE_TEST( sMemory->cralloc( aDataPlan->partitionFilterSize,
                                    (void**) &aDataPlan->partitionFilterArea )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->partitionFilterSize = 0;
        aDataPlan->partitionFilterArea = NULL;
        aDataPlan->partitionFilter = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::allocFixKeyRange( qcTemplate * aTemplate,
                           qmncPCRD   * aCodePlan,
                           qmndPCRD   * aDataPlan )
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

    if ( aCodePlan->fixKeyRange != NULL )
    {
        IDE_DASSERT( aCodePlan->index != NULL );

        // Fixed Key Range�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           aCodePlan->fixKeyRange,
                                           & aDataPlan->fixKeyRangeSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->fixKeyRangeSize > 0 );

        // Fixed Key Range�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;
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
qmnPCRD::allocFixKeyFilter( qcTemplate * aTemplate,
                            qmncPCRD   * aCodePlan,
                            qmndPCRD   * aDataPlan )
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

    if ( ( aCodePlan->fixKeyFilter != NULL ) &&
         ( aCodePlan->fixKeyRange != NULL ) )
    {
        IDE_DASSERT( aCodePlan->index != NULL );

        // Fixed Key Filter�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           aCodePlan->fixKeyFilter,
                                           & aDataPlan->fixKeyFilterSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->fixKeyFilterSize > 0 );

        // Fixed Key Filter�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;
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
qmnPCRD::allocVarKeyRange( qcTemplate * aTemplate,
                           qmncPCRD   * aCodePlan,
                           qmndPCRD   * aDataPlan )
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

    if ( aCodePlan->varKeyRange != NULL )
    {
        IDE_DASSERT( aCodePlan->index != NULL );

        // Variable Key Range�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           aCodePlan->varKeyRange,
                                           & aDataPlan->varKeyRangeSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->varKeyRangeSize > 0 );

        // Variable Key Range�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;
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
qmnPCRD::allocVarKeyFilter( qcTemplate * aTemplate,
                            qmncPCRD   * aCodePlan,
                            qmndPCRD   * aDataPlan )
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

    if ( ( aCodePlan->varKeyFilter != NULL ) &&
         ( (aCodePlan->varKeyRange != NULL) ||
           (aCodePlan->fixKeyRange != NULL) ) ) // BUG-20679
    {
        IDE_DASSERT( aCodePlan->index != NULL );

        // Variable Key Filter�� ũ�� ���
        IDE_TEST(
            qmoKeyRange::estimateKeyRange( aTemplate,
                                           aCodePlan->varKeyFilter,
                                           & aDataPlan->varKeyFilterSize )
            != IDE_SUCCESS );

        IDE_DASSERT( aDataPlan->varKeyFilterSize > 0 );

        // Variable Key Filter�� ���� ���� �Ҵ�
        sMemory = aTemplate->stmt->qmxMem;
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
qmnPCRD::allocNotNullKeyRange( qcTemplate * aTemplate,
                               qmncPCRD   * aCodePlan,
                               qmndPCRD   * aDataPlan )
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

    if ( ( (aCodePlan->flag & QMNC_SCAN_NOTNULL_RANGE_MASK) ==
           QMNC_SCAN_NOTNULL_RANGE_TRUE ) &&
         ( aCodePlan->fixKeyRange == NULL ) &&
         ( aCodePlan->varKeyRange == NULL ) )
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
qmnPCRD::doItFirst( qcTemplate * aTemplate,
                    qmnPlan    * aPlan,
                    qmcRowFlag * aFlag )
{
    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD *) (aTemplate->tmplate.data + aPlan->offset);

    qmnChildren         * sChild;
    qmndSCAN            * sChildDataScan;
    UInt                  i;

    // DML�� �ƴ� ���
    if ( sDataPlan->plan.myTuple->cursorInfo == NULL )
    {
        // Table�� IS Lock�� �Ǵ�.
        IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aTemplate->stmt))->getTrans(),
                                            sCodePlan->table,
                                            sCodePlan->tableSCN,
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
                                            sCodePlan->table,
                                            sCodePlan->tableSCN,
                                            SMI_TBSLV_DDL_DML, // TBS Validation �ɼ�
                                            SMI_TABLE_LOCK_IX,
                                            ID_ULONG_MAX,
                                            ID_FALSE ) // BUG-28752 ����� Lock�� ������ Lock�� �����մϴ�.
                 != IDE_SUCCESS);
    }

    // BUG-47599
    // emptyPartiton�� lock�� �ɸ� add partition op�� lock �浹�� �߻��մϴ�.
    if ( sCodePlan->tableRef->mEmptyPartRef != NULL )
    {
        IDE_TEST( smiValidateObjects( sCodePlan->tableRef->mEmptyPartRef->partitionHandle,
                                      sCodePlan->tableRef->mEmptyPartRef->partitionSCN )
                  != IDE_SUCCESS);
    }

    // ������ ���� �˻�
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    // PROJ-2205 rownum in DML
    // row movement update�� ��� first doIt�� child�� ��� cursor�� ����д�.
    if ( sDataPlan->isRowMovementUpdate == ID_TRUE )
    {
        sDataPlan->isNeedAllFetchColumn = ID_TRUE;

        IDE_TEST( makeChildrenArea( aTemplate, aPlan )
                  != IDE_SUCCESS );

        // ��� child plan�� cursor�� �̸� ���� ����ξ�� �Ѵ�.
        for( i = 0; i < sDataPlan->selectedChildrenCount; i++ )
        {
            sChild = sDataPlan->childrenArea[i];

            IDE_TEST( sChild->childPlan->init( aTemplate,
                                               sChild->childPlan )
                      != IDE_SUCCESS );

            sChildDataScan =
                (qmndSCAN*) (aTemplate->tmplate.data + sChild->childPlan->offset);

            IDE_TEST( qmnSCAN::openCursorForPartition(
                          aTemplate,
                          (qmncSCAN*)(sChild->childPlan),
                          sChildDataScan )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        if( sCodePlan->partitionFilter != NULL )
        {
            // partition filter ����
            IDE_TEST( makePartitionFilter( aTemplate,
                                           sCodePlan,
                                           sDataPlan ) != IDE_SUCCESS );

            // partition filter�� ����ٰ� ������ �� �ִ�.
            // host ������ ��� conversion������ �ȵ� �� ����.
            // �̶��� filtering�� ���� �ʴ´�.
            if( sDataPlan->partitionFilter != NULL )
            {
                IDE_TEST( partitionFiltering( aTemplate,
                                              sCodePlan,
                                              sDataPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( makeChildrenArea( aTemplate, aPlan )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( makeChildrenArea( aTemplate, aPlan )
                      != IDE_SUCCESS );
        }

        for ( i = 0; i < sDataPlan->selectedChildrenCount; i++ )
        {
            sChild = sDataPlan->childrenArea[i];
            IDE_TEST( sChild->childPlan->init( aTemplate,
                                               sChild->childPlan )
                      != IDE_SUCCESS );
        }
    }

    // ���õ� children�� �ִ� ��츸 ����.
    if( sDataPlan->selectedChildrenCount > 0 )
    {
        if ( ( sCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
             == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE )
        {
            // make children index
            IDE_TEST( makeChildrenIndex( sDataPlan )
                      != IDE_SUCCESS );

            // KeyRange, KeyFilter, Filter ����
            IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                             sCodePlan,
                                             sDataPlan ) != IDE_SUCCESS );

            // Cursor�� ����
            if ( ( *sDataPlan->flag & QMND_PCRD_INDEX_CURSOR_MASK )
                 == QMND_PCRD_INDEX_CURSOR_OPEN )
            {
                // �̹� ���� �ִ� ���
                IDE_TEST( restartIndexCursor( sCodePlan, sDataPlan )
                          != IDE_SUCCESS );
            }
            else
            {
                // ó�� ���� ���
                IDE_TEST( openIndexCursor( aTemplate, sCodePlan, sDataPlan )
                          != IDE_SUCCESS );
            }

            // Record�� ȹ���Ѵ�.
            IDE_TEST( readRowWithIndex( aTemplate, sCodePlan, sDataPlan, aFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            sDataPlan->curChildNo = 0;

            // Record�� ȹ���Ѵ�.
            IDE_TEST( readRow( aTemplate, sCodePlan, sDataPlan, aFlag )
                      != IDE_SUCCESS );
        }

        if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_EXIST )
        {
            sDataPlan->doIt = qmnPCRD::doItNext;
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // ���õ� children�� ���ٸ� �����ʹ� ����.
        *aFlag = QMC_ROW_DATA_NONE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::makePartitionFilter( qcTemplate * aTemplate,
                              qmncPCRD   * aCodePlan,
                              qmndPCRD   * aDataPlan )
{
    qmsTableRef * sTableRef;
    UInt          sCompareType;

    sTableRef = aCodePlan->tableRef;

    // PROJ-1872
    // Partition Key�� Partition Table �����Ҷ� ����
    // (Partition�� ������ ���ذ�)�� (Predicate�� value)��
    // ���ϹǷ� MTD Value���� compare�� �����
    // ex) i1 < 10 => Partition P1
    //     i1 < 20 => Partition P2
    //     i1 < 30 => Partition P3
    // SELECT ... FROM ... WHERE i1 = 5
    // P1, P2, P3���� P1 Partition�� �����ϱ� ����
    // Partition Key Range�� �����
    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;

    IDE_TEST( qmoKeyRange::makePartKeyRange(
                  aTemplate,
                  aCodePlan->partitionFilter,
                  sTableRef->tableInfo->partKeyColCount,
                  sTableRef->tableInfo->partKeyColBasicInfo,
                  sTableRef->tableInfo->partKeyColsFlag,
                  sCompareType,
                  aDataPlan->partitionFilterArea,
                  aDataPlan->partitionFilterSize,
                  &aDataPlan->partitionFilter )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::readRow( qcTemplate * aTemplate,
                  qmncPCRD   * aCodePlan,
                  qmndPCRD   * aDataPlan,
                  qmcRowFlag * aFlag )
{
    idBool sJudge;

    qmnPlan       * sChildPlan;
    qmndSCAN      * sChildDataPlan;

    //--------------------------------------------------
    // Data�� ������ ������ ��� Child�� �����Ѵ�.
    //--------------------------------------------------

    while ( 1 )
    {
        //----------------------------
        // ���� Child�� ����
        //----------------------------

        sChildPlan = aDataPlan->childrenArea[aDataPlan->curChildNo]->childPlan;

        sChildDataPlan = (qmndSCAN*)(aTemplate->tmplate.data +
                                         sChildPlan->offset );

        sChildDataPlan->isNeedAllFetchColumn = aDataPlan->isNeedAllFetchColumn;

        // PROJ-2002 Column Security
        // child�� doIt�ÿ��� plan.myTuple�� ������ �� �ִ�.
        aDataPlan->plan.myTuple->columns = sChildDataPlan->plan.myTuple->columns;
        aDataPlan->plan.myTuple->partitionTupleID =
            ((qmncSCAN*)sChildPlan)->tupleRowID;

        // PROJ-1502 PARTITIONED DISK TABLE
        aDataPlan->plan.myTuple->tableHandle = (void *)((qmncSCAN*)sChildPlan)->table;

        /* PROJ-2464 hybrid partitioned table ���� */
        aDataPlan->plan.myTuple->rowOffset  = sChildDataPlan->plan.myTuple->rowOffset;
        aDataPlan->plan.myTuple->rowMaximum = sChildDataPlan->plan.myTuple->rowMaximum;

        IDE_TEST( sChildPlan->doIt( aTemplate, sChildPlan, aFlag )
                  != IDE_SUCCESS );

        //----------------------------
        // Data ���� ���ο� ���� ó��
        //----------------------------

        sJudge = ID_FALSE;

        if ( (*aFlag & QMC_ROW_DATA_MASK) == QMC_ROW_DATA_EXIST )
        {
            //----------------------------
            // ���� Child�� Data�� �����ϴ� ���
            //----------------------------

            aDataPlan->plan.myTuple->row = sChildDataPlan->plan.myTuple->row;
            aDataPlan->plan.myTuple->rid = sChildDataPlan->plan.myTuple->rid;
            aDataPlan->plan.myTuple->modify++;

            // judge���..
            if( aCodePlan->subqueryFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      aCodePlan->subqueryFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                sJudge = ID_TRUE;
            }

            if ( aCodePlan->nnfFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      aCodePlan->nnfFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            if( sJudge == ID_TRUE )
            {
                *aFlag = QMC_ROW_DATA_EXIST;
                break;
            }
            else
            {
                // ���� flag�� none���� ������ ������ ������,
                // ���ڵ尡 ������ ����.
                *aFlag = QMC_ROW_DATA_NONE;
            }
        }
        else
        {
            //----------------------------
            // ���� Child�� Data�� ���� ���
            //----------------------------

            aDataPlan->curChildNo++;

            if ( aDataPlan->curChildNo == aDataPlan->selectedChildrenCount )
            {
                // ��� Child�� ������ ���
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::printAccessInfo( qmndPCRD     * aDataPlan,
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

    IDE_DASSERT( aDataPlan != NULL );
    IDE_DASSERT( aString   != NULL );

    if ( aMode == QMN_DISPLAY_ALL )
    {
        //----------------------------
        // explain plan = on; �� ���
        //----------------------------

        // ���� Ƚ�� ���
        if ( (*aDataPlan->flag & QMND_PCRD_INIT_DONE_MASK)
             == QMND_PCRD_INIT_DONE_TRUE )
        {
            // fix BUG-9052
            iduVarStringAppendFormat( aString,
                                      ", ACCESS: %"ID_UINT32_FMT"",
                                      (aDataPlan->plan.myTuple->modify
                                       - aDataPlan->subQFilterDepCnt) );
        }
        else
        {
            iduVarStringAppend( aString,
                                ", ACCESS: 0" );
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
}

IDE_RC
qmnPCRD::partitionFiltering( qcTemplate * aTemplate,
                             qmncPCRD   * aCodePlan,
                             qmndPCRD   * aDataPlan )
{
    qmsTableRef * sTableRef;

    sTableRef = aCodePlan->tableRef;

    IDE_TEST( qmoPartition::partitionFilteringWithPartitionFilter(
                  aTemplate->stmt,
                  aCodePlan->rangeSortedChildrenArray,
                  aDataPlan->rangeIntersectCountArray,
                  aCodePlan->selectedPartitionCount,
                  aCodePlan->partitionCount,
                  aCodePlan->plan.children,
                  sTableRef->tableInfo->partitionMethod,
                  aDataPlan->partitionFilter,
                  aDataPlan->childrenArea,
                  &aDataPlan->selectedChildrenCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::makeChildrenArea( qcTemplate * aTemplate,
                           qmnPlan    * aPlan )
{
    qmncPCRD    * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD    * sDataPlan =
        (qmndPCRD *) (aTemplate->tmplate.data + aPlan->offset);

    qmnChildren * sChild;
    UInt          i;

    qmsTableRef * sTableRef;
    sTableRef = sCodePlan->tableRef;

    /* PROJ-2249 method range �̸� ���ĵ� children�� �Ҵ� �Ѵ�. */
    if ( ( sTableRef->tableInfo->partitionMethod == QCM_PARTITION_METHOD_RANGE ) ||
         ( sTableRef->tableInfo->partitionMethod == QCM_PARTITION_METHOD_RANGE_USING_HASH ) )
    {
        for ( i = 0; i < sDataPlan->selectedChildrenCount; i++ )
        {
            sDataPlan->childrenArea[i] = sCodePlan->rangeSortedChildrenArray[i].children;
        }
    }
    else
    {
        for ( i = 0,
                 sChild = sCodePlan->plan.children;
             ( i < sDataPlan->selectedChildrenCount ) &&
                 ( sChild != NULL );
             i++,
                 sChild = sChild->next )
        {
            sDataPlan->childrenArea[i] = sChild;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmnPCRD::makeChildrenIndex( qmndPCRD   * aDataPlan )
{
    qmncSCAN  * sChildPlan;
    UInt        i;

    for ( i = 0; i < aDataPlan->selectedChildrenCount; i++ )
    {
        sChildPlan = (qmncSCAN*) aDataPlan->childrenArea[i]->childPlan;

        aDataPlan->childrenIndex[i].childOID  =
            sChildPlan->partitionRef->partitionOID;
        aDataPlan->childrenIndex[i].childPlan =
            (qmnPlan*) sChildPlan;
    }

    if ( aDataPlan->selectedChildrenCount > 1 )
    {
        idlOS::qsort( aDataPlan->childrenIndex,
                      aDataPlan->selectedChildrenCount,
                      ID_SIZEOF(qmnChildrenIndex),
                      compareChildrenIndex );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmnPCRD::storeCursor( qcTemplate * aTemplate,
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

    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    // ���ռ� �˻�
    IDE_DASSERT( ( sCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    IDE_DASSERT( (*sDataPlan->flag & QMND_PCRD_INDEX_CURSOR_MASK)
                 == QMND_PCRD_INDEX_CURSOR_OPEN );

    // Index Cursor ������ ������.
    IDE_TEST( sDataPlan->cursor.getCurPos( & sDataPlan->cursorInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::restoreCursor( qcTemplate * aTemplate,
                        qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    �̹� ����� Cursor�� �̿��Ͽ� Cursor ��ġ�� ������Ŵ
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncPCRD * sCodePlan = (qmncPCRD*) aPlan;
    qmndPCRD * sDataPlan =
        (qmndPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    void       * sOrgRow;
    void       * sIndexRow;
    qmcRowFlag   sRowFlag = QMC_ROW_INITIALIZE;

    // ���ռ� �˻�
    IDE_DASSERT( ( sCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    IDE_DASSERT( (*sDataPlan->flag & QMND_PCRD_INDEX_CURSOR_MASK)
                 == QMND_PCRD_INDEX_CURSOR_OPEN );

    //-----------------------------
    // Index Cursor�� ����
    //-----------------------------
    
    IDE_TEST( sDataPlan->cursor.setCurPos( & sDataPlan->cursorInfo )
              != IDE_SUCCESS );
    
    //-----------------------------
    // Index Record�� ����
    //-----------------------------

    // To Fix PR-8110
    // ����� Cursor�� �����ϸ�, ���� ��ġ�� �������� �̵��Ѵ�.
    // ����, �ܼ��� Cursor�� Read�ϸ� �ȴ�.
    sOrgRow = sIndexRow = sDataPlan->indexTuple->row;

    IDE_TEST(
        sDataPlan->cursor.readRow( (const void**) & sIndexRow,
                                   & sDataPlan->indexTuple->rid,
                                   SMI_FIND_NEXT )
        != IDE_SUCCESS );

    sDataPlan->indexTuple->row =
        (sIndexRow == NULL) ? sOrgRow : sIndexRow;

    // �ݵ�� ����� Cursor���� Row�� �����ؾ� �Ѵ�.
    IDE_ASSERT( sIndexRow != NULL );

    //-----------------------------
    // Table Record�� ����
    //-----------------------------

    // Index Row�� Table Row �б�
    IDE_TEST( readRowWithIndexRow( aTemplate,
                                   sDataPlan,
                                   & sRowFlag )
              != IDE_SUCCESS );

    // �ݵ�� �����ؾ��Ѵ�.
    IDE_TEST_RAISE( ( sRowFlag & QMC_ROW_DATA_MASK )
                    == QMC_ROW_DATA_NONE,
                    ERR_ABORT_RECORD_NOT_FOUND );

    // Ŀ���� ���� ���� ���� �Լ�
    sDataPlan->doIt = qmnPCRD::doItNext;

    // Modify�� ����, record�� ����� �����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_RECORD_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnPCRD::restoreCursor",
                                  "record not found" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::makeKeyRangeAndFilter( qcTemplate * aTemplate,
                                qmncPCRD   * aCodePlan,
                                qmndPCRD   * aDataPlan )
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

    //-------------------------------------
    // ���ռ� �˻�
    //-------------------------------------

    IDE_DASSERT( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    if ( ( aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK )
         == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE )
    {
        // IN Subquery Key Range�� ���� ��� ���� ������ �����ؾ� ��
        IDE_DASSERT( aCodePlan->fixKeyRange == NULL );
        IDE_DASSERT( aCodePlan->varKeyRange != NULL );
        IDE_DASSERT( aCodePlan->fixKeyFilter == NULL );
        IDE_DASSERT( aCodePlan->varKeyFilter == NULL );
    }

    //-------------------------------------
    // Predicate ������ ����
    //-------------------------------------

    sPredicateInfo.index = aCodePlan->index;
    sPredicateInfo.tupleRowID = aCodePlan->tupleRowID;  // partitioned table tuple id

    // Fixed Key Range ������ ����
    sPredicateInfo.fixKeyRangeArea = aDataPlan->fixKeyRangeArea;
    sPredicateInfo.fixKeyRange = aDataPlan->fixKeyRange;
    sPredicateInfo.fixKeyRangeOrg = aCodePlan->fixKeyRange;
    sPredicateInfo.fixKeyRangeSize = aDataPlan->fixKeyRangeSize;

    // Variable Key Range ������ ����
    sPredicateInfo.varKeyRangeArea = aDataPlan->varKeyRangeArea;
    sPredicateInfo.varKeyRange = aDataPlan->varKeyRange;
    sPredicateInfo.varKeyRangeOrg = aCodePlan->varKeyRange;
    sPredicateInfo.varKeyRange4FilterOrg = aCodePlan->varKeyRange4Filter;
    sPredicateInfo.varKeyRangeSize = aDataPlan->varKeyRangeSize;

    // Fixed Key Filter ������ ����
    sPredicateInfo.fixKeyFilterArea = aDataPlan->fixKeyFilterArea;
    sPredicateInfo.fixKeyFilter = aDataPlan->fixKeyFilter;
    sPredicateInfo.fixKeyFilterOrg = aCodePlan->fixKeyFilter;
    sPredicateInfo.fixKeyFilterSize = aDataPlan->fixKeyFilterSize;

    // Variable Key Filter ������ ����
    sPredicateInfo.varKeyFilterArea = aDataPlan->varKeyFilterArea;
    sPredicateInfo.varKeyFilter = aDataPlan->varKeyFilter;
    sPredicateInfo.varKeyFilterOrg = aCodePlan->varKeyFilter;
    sPredicateInfo.varKeyFilter4FilterOrg = aCodePlan->varKeyFilter4Filter;
    sPredicateInfo.varKeyFilterSize = aDataPlan->varKeyFilterSize;

    // Not Null Key Range ������ ����
    sPredicateInfo.notNullKeyRange = aDataPlan->notNullKeyRange;

    // Filter ������ ����
    sPredicateInfo.filter = aCodePlan->filter;

    sPredicateInfo.filterCallBack = & aDataPlan->callBack;
    sPredicateInfo.callBackDataAnd = & aDataPlan->callBackDataAnd;
    sPredicateInfo.callBackData = aDataPlan->callBackData;

    /* PROJ-2632 */
    sPredicateInfo.mSerialFilterInfo  = NULL;
    sPredicateInfo.mSerialExecuteData = NULL;

    //-------------------------------------
    // Key Range, Key Filter, Filter�� ����
    //-------------------------------------

    IDE_TEST( qmn::makeKeyRangeAndFilter( aTemplate,
                                          & sPredicateInfo )
              != IDE_SUCCESS );

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
        *aDataPlan->flag &= ~QMND_PCRD_INSUBQ_RANGE_BUILD_MASK;
        *aDataPlan->flag |= QMND_PCRD_INSUBQ_RANGE_BUILD_FAILURE;
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
qmnPCRD::openIndexCursor( qcTemplate * aTemplate,
                          qmncPCRD   * aCodePlan,
                          qmndPCRD   * aDataPlan )
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

    UInt   sTraverse;
    UInt   sPrevious;
    UInt   sCursorFlag;
    UInt   sInplaceUpdate;
    void * sOrgRow;

    smiFetchColumnList   * sFetchColumnList = NULL;
    qmnCursorInfo        * sCursorInfo;
    idBool                 sIsMutexLock = ID_FALSE;

    // ���ռ� �˻�
    IDE_DASSERT( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
          == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
         ((*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
          == QMND_PCRD_INSUBQ_RANGE_BUILD_FAILURE ) )
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

        //-------------------------------------------------------
        //  Cursor�� ����.
        //-------------------------------------------------------
        
        // Cursor�� �ʱ�ȭ
        aDataPlan->cursor.initialize();

        // PROJ-1705
        // ���ڵ���ġ�� ����Ǿ�� �� �÷���������
        IDE_DASSERT( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
                     == QMN_PLAN_STORAGE_DISK );

        IDE_TEST( qdbCommon::makeFetchColumnList4TupleID(
                      aTemplate,
                      aCodePlan->indexTupleRowID,
                      ID_TRUE,
                      aCodePlan->indexTableIndex,
                      ID_TRUE,
                      & sFetchColumnList )
                  != IDE_SUCCESS );

        aDataPlan->cursorProperty.mFetchColumnList = sFetchColumnList;

        /* BUG-39836 : repeatable read���� Disk DB����  B-tree index�� ������ 
         * partition table���� _PROWID Fetch�� ���� �� �� mLockRowBuffer ���� 
         * NULL�̱� ������ assert�Ǿ� natc���� FATAL �߻�. 
         * qmnSCAN::openCursor���� ������ �ּ��� �����Ͽ� mLockRowBuffer�� 
         * ���� �Ҵ�.  
         *
         * - select for update�� repeatable read ó���� ���� sm�� qp���� �Ҵ��� 
         * �޸𸮿����� �����ش�.
         * sm���� select for update�� repeatable readó���� �޸� ��� ���. 
         */
        aDataPlan->cursorProperty.mLockRowBuffer = (UChar*)aDataPlan->plan.myTuple->row;
        aDataPlan->cursorProperty.mLockRowBufferSize =
            aTemplate->tmplate.rows[aCodePlan->tupleRowID].rowOffset;       

        aDataPlan->cursorProperty.mIndexTypeID =
                (UChar)aCodePlan->indexTableIndex->indexTypeId;

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
        IDE_TEST( aTemplate->stmt->mCursorMutex.lock(NULL) != IDE_SUCCESS );
        sIsMutexLock = ID_TRUE;

        IDE_TEST( aDataPlan->cursor.open( QC_SMI_STMT( aTemplate->stmt ),
                                          aCodePlan->indexTableHandle,
                                          aCodePlan->indexTableIndex->indexHandle,
                                          aCodePlan->indexTableSCN,
                                          aDataPlan->updateColumnList,
                                          aDataPlan->keyRange,
                                          aDataPlan->keyFilter,
                                          smiGetDefaultFilter(),    // filter�� ���� ó���Ѵ�.
                                          sCursorFlag,
                                          aDataPlan->cursorType,
                                          & aDataPlan->cursorProperty,
                                          ID_FALSE )
                  != IDE_SUCCESS );

        // Cursor�� ���
        IDE_TEST( aTemplate->cursorMgr->addOpenedCursor(
                      aTemplate->stmt->qmxMem,
                      aCodePlan->indexTupleRowID,
                      & aDataPlan->cursor )
                  != IDE_SUCCESS );

        sIsMutexLock = ID_FALSE;
        IDE_TEST( aTemplate->stmt->mCursorMutex.unlock() != IDE_SUCCESS );

        // Cursor�� �������� ǥ��
        *aDataPlan->flag &= ~QMND_PCRD_INDEX_CURSOR_MASK;
        *aDataPlan->flag |= QMND_PCRD_INDEX_CURSOR_OPEN;

        //-------------------------------------------------------
        //  Cursor�� ���� ��ġ�� �̵�
        //-------------------------------------------------------

        // Disk Table�� ���
        // Key Range �˻����� ���� �ش� row�� ������ ����� �� �ִ�.
        // �̸� ���� ���� ������ ��ġ�� �����ϰ� �̸� �����Ѵ�.
        sOrgRow = aDataPlan->indexTuple->row;
        
        IDE_TEST( aDataPlan->cursor.beforeFirst() != IDE_SUCCESS );
        
        aDataPlan->indexTuple->row = sOrgRow;

        //---------------------------------
        // cursor ���� ����
        //---------------------------------

        // PROJ-2205 rownum in DML
        // row movement update�� ���
        if ( aDataPlan->plan.myTuple->cursorInfo != NULL )
        {
            // DML�� ������ ����� ���
            sCursorInfo = (qmnCursorInfo*) aDataPlan->plan.myTuple->cursorInfo;

            sCursorInfo->cursor = & aDataPlan->cursor;
            
            // selected index
            sCursorInfo->selectedIndex = aCodePlan->index;
            sCursorInfo->selectedIndexTuple = aDataPlan->indexTuple;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsMutexLock == ID_TRUE )
    {
        (void)aTemplate->stmt->mCursorMutex.unlock();
        sIsMutexLock = ID_FALSE;
    }

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::restartIndexCursor( qmncPCRD   * aCodePlan,
                             qmndPCRD   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�� Restart�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void * sOrgRow;

    // ���ռ� �˻�
    IDE_DASSERT( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 == QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
          == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
         ((*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
          == QMND_PCRD_INSUBQ_RANGE_BUILD_FAILURE ) )
    {
        // Cursor�� ���� �ʴ´�.
        // �� �̻� IN SUBQUERY Key Range�� ������ �� ���� ����
        // record�� fetch���� �ʾƾ� �Ѵ�.
        // Nothing To Do
    }
    else
    {
        sOrgRow = aDataPlan->indexTuple->row;

        // Disk Table�� ���
        // Key Range �˻����� ���� �ش� row�� ������ ����� �� �ִ�.
        // �̸� ���� ���� ������ ��ġ�� �����ϰ� �̸� �����Ѵ�.
        IDE_TEST( aDataPlan->
                  cursor.restart( aDataPlan->keyRange,
                                  aDataPlan->keyFilter,
                                  smiGetDefaultFilter() )   // filter�� ���� ó���Ѵ�.
                  != IDE_SUCCESS);
        
        IDE_TEST( aDataPlan->cursor.beforeFirst() != IDE_SUCCESS );

        aDataPlan->indexTuple->row = sOrgRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPCRD::readRowWithIndex( qcTemplate * aTemplate,
                                  qmncPCRD   * aCodePlan,
                                  qmndPCRD   * aDataPlan,
                                  qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Index Cursor�κ��� ���ǿ� �´� Record�� �а�
 *    OID, RID�� partition cursor���� record�� �д´�.
 *
 * Implementation :
 *    Cursor�� �̿��Ͽ� KeyRange, KeyFilter, Filter�� �����ϴ�
 *    Record�� �д´�.
 *    ����, subquery filter�� �����Ͽ� �̸� �����ϸ� record�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool       sJudge;
    idBool       sResult;
    void       * sOrgRow;
    void       * sIndexRow;
    qmcRowFlag   sRowFlag = QMC_ROW_INITIALIZE;

    //----------------------------------------------
    // 1. Cursor�� �̿��Ͽ� Record�� ����
    // 2. Subquery Filter ����
    // 3. Subquery Filter �� �����ϸ� ��� ����
    //----------------------------------------------

    sJudge = ID_FALSE;

    while ( sJudge == ID_FALSE )
    {
        //----------------------------------------
        // Index Cursor�� �̿��Ͽ� Index Row�� ����
        //----------------------------------------

        if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
              == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
             ((*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
              == QMND_PCRD_INSUBQ_RANGE_BUILD_FAILURE ) )
        {
            // Subquery ����� �����
            // IN SUBQUERY KEYRANGE�� ���� RANGE ������ ������ ���
            sIndexRow = NULL;
        }
        else
        {
            sOrgRow = sIndexRow = aDataPlan->indexTuple->row;

            IDE_TEST(
                aDataPlan->cursor.readRow( (const void**) & sIndexRow,
                                           & aDataPlan->indexTuple->rid,
                                           SMI_FIND_NEXT )
                != IDE_SUCCESS );

            aDataPlan->indexTuple->row =
                (sIndexRow == NULL) ? sOrgRow : sIndexRow;
        }

        //----------------------------------------
        // IN SUBQUERY KEY RANGE�� ��õ�
        //----------------------------------------

        if ( ((aCodePlan->flag & QMNC_SCAN_INSUBQ_KEYRANGE_MASK)
              == QMNC_SCAN_INSUBQ_KEYRANGE_TRUE ) &&
             (sIndexRow == NULL) )
        {
            // IN SUBQUERY KEYRANGE�� ��� �ٽ� �õ��Ѵ�.
            IDE_TEST( reReadIndexRow4InSubRange( aTemplate,
                                                 aCodePlan,
                                                 aDataPlan,
                                                 & sIndexRow )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        if ( sIndexRow == NULL )
        {
            sJudge = ID_FALSE;
            break;
        }
        else
        {
            //----------------------------------------
            // Index Row�� Table Row �б�
            //----------------------------------------

            IDE_TEST( readRowWithIndexRow( aTemplate,
                                           aDataPlan,
                                           & sRowFlag )
                      != IDE_SUCCESS );

            if ( ( sRowFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
            {
                continue;
            }
            else
            {
                // Nothing to do.
            }

            //----------------------------------------
            // FILTER ó��
            //----------------------------------------

            if ( aDataPlan->callBack.callback != smiGetDefaultFilter()->callback )
            {
                IDE_TEST( qtc::smiCallBack(
                              & sResult,
                              aDataPlan->plan.myTuple->row,
                              NULL,
                              0,
                              aDataPlan->plan.myTuple->rid,
                              aDataPlan->callBack.data )
                          != IDE_SUCCESS );

                if ( sResult == ID_FALSE )
                {
                    continue;
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

            //----------------------------------------
            // SUBQUERY FILTER ó��
            //----------------------------------------

            // modify�� ����
            if ( aCodePlan->filter == NULL )
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
            if ( aCodePlan->subqueryFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      aCodePlan->subqueryFilter,
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

IDE_RC
qmnPCRD::reReadIndexRow4InSubRange( qcTemplate * aTemplate,
                                    qmncPCRD   * aCodePlan,
                                    qmndPCRD   * aDataPlan,
                                    void      ** aIndexRow )
{
/***********************************************************************
 *
 * Description :
 *     IN SUBQUERY KEYRANGE�� ���� ��� Record Read�� �ٽ� �õ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    void       * sIndexRow;
    void       * sOrgRow;

    sIndexRow = NULL;

    if ( (*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
         == QMND_PCRD_INSUBQ_RANGE_BUILD_SUCCESS )
    {
        //---------------------------------------------------------
        // [IN SUBQUERY KEY RANGE]
        // �˻��� Record�� ������, Subquery�� ����� ���� �����ϴ� ���
        //     - Key Range�� �ٽ� �����Ѵ�.
        //     - Record�� Fetch�Ѵ�.
        //     - ���� ������ Fetch�� Record�� �ְų�,
        //       Key Range ������ ������ ������ �ݺ��Ѵ�.
        //---------------------------------------------------------

        while ( (sIndexRow == NULL) &&
                ((*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
                 == QMND_PCRD_INSUBQ_RANGE_BUILD_SUCCESS ) )
        {
            // Key Range�� ������Ѵ�.
            IDE_TEST( makeKeyRangeAndFilter( aTemplate,
                                             aCodePlan,
                                             aDataPlan ) != IDE_SUCCESS );


            // Cursor�� ����.
            // �̹� Open�Ǿ� �����Ƿ�, Restart�Ѵ�.
            IDE_TEST( restartIndexCursor( aCodePlan,
                                          aDataPlan ) != IDE_SUCCESS );


            if ( (*aDataPlan->flag & QMND_PCRD_INSUBQ_RANGE_BUILD_MASK)
                 == QMND_PCRD_INSUBQ_RANGE_BUILD_SUCCESS )
            {
                // Key Range ������ ������ ��� Record�� �д´�.
                sOrgRow = sIndexRow = aDataPlan->indexTuple->row;

                IDE_TEST(
                    aDataPlan->cursor.readRow( (const void**) & sIndexRow,
                                               & aDataPlan->indexTuple->rid,
                                               SMI_FIND_NEXT )
                    != IDE_SUCCESS );

                aDataPlan->indexTuple->row =
                    (sIndexRow == NULL) ? sOrgRow : sIndexRow;
            }
            else
            {
                sIndexRow = NULL;
            }
        }
    }
    else
    {
        // sIndexRow = NULL;
    }

    *aIndexRow = sIndexRow;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnPCRD::readRowWithIndexRow( qcTemplate * aTemplate,
                              qmndPCRD   * aDataPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *     oid, rid�� ��ϵ� index row�� table row�� �д´�.
 *
 * Implementation :
 *     - oid�� �ش��ϴ� child init
 *     - readRowFromGRID on child
 *
 ***********************************************************************/

    UInt               sColumnCount;
    mtcColumn        * sColumn;
    smOID              sPartOID;
    scGRID             sRowGRID;
    qmnChildrenIndex   sFindIndex;
    qmnChildrenIndex * sFound;
    qmncSCAN         * sChildPlan;
    qmndSCAN         * sChildDataPlan;

    //----------------------------
    // get oid, rid
    //----------------------------

    IDE_DASSERT( aDataPlan->indexTuple->columnCount >= 3 );

    sColumnCount = aDataPlan->indexTuple->columnCount;

    // oid
    sColumn = & aDataPlan->indexTuple->columns[sColumnCount - 2];
    sPartOID = *(smOID*)( (UChar*)aDataPlan->indexTuple->row + sColumn->column.offset );

    // rid
    sColumn = & aDataPlan->indexTuple->columns[sColumnCount - 1];
    sRowGRID = *(scGRID*)( (UChar*)aDataPlan->indexTuple->row + sColumn->column.offset );

    //----------------------------
    // get child with oid
    //----------------------------

    sFindIndex.childOID = sPartOID;

    sFound = (qmnChildrenIndex*) idlOS::bsearch( & sFindIndex,
                                                 aDataPlan->childrenIndex,
                                                 aDataPlan->selectedChildrenCount,
                                                 ID_SIZEOF(qmnChildrenIndex),
                                                 compareChildrenIndex );

    if ( sFound == NULL )
    {
        // ������ ���� ��� child�� ���� �� �ִ�.
        // select * from t1 partition (p1) where i1=1;

        // ������ ������ Setting
        *aFlag = QMC_ROW_DATA_NONE;
    }
    else
    {
        sChildPlan     = (qmncSCAN*) sFound->childPlan;
        sChildDataPlan = (qmndSCAN*)( aTemplate->tmplate.data + sChildPlan->plan.offset );

        //----------------------------
        // read row
        //----------------------------

        if ( ( *sChildDataPlan->flag & QMND_SCAN_CURSOR_MASK )
             == QMND_SCAN_CURSOR_CLOSED )
        {
            IDE_TEST( qmnSCAN::openCursorForPartition( aTemplate, sChildPlan, sChildDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // grid�� row�� �д´�.
        IDE_TEST( qmnSCAN::readRowFromGRID( aTemplate, (qmnPlan*)sChildPlan, sRowGRID, aFlag )
                  != IDE_SUCCESS );

        // Index Row�� �ִٸ� Table Row�� �ݵ�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( ( *aFlag & QMC_ROW_DATA_MASK )
                        == QMC_ROW_DATA_NONE,
                        ERR_ABORT_RECORD_NOT_FOUND );

        // PROJ-2002 Column Security
        // child�� doIt�ÿ��� plan.myTuple�� ������ �� �ִ�.
        aDataPlan->plan.myTuple->columns = sChildDataPlan->plan.myTuple->columns;
        aDataPlan->plan.myTuple->partitionTupleID = sChildPlan->tupleRowID;

        // PROJ-1502 PARTITIONED DISK TABLE
        aDataPlan->plan.myTuple->tableHandle = (void *)sChildPlan->table;

        /* PROJ-2464 hybrid partitioned table ���� */
        aDataPlan->plan.myTuple->rowOffset  = sChildDataPlan->plan.myTuple->rowOffset;
        aDataPlan->plan.myTuple->rowMaximum = sChildDataPlan->plan.myTuple->rowMaximum;

        aDataPlan->plan.myTuple->row = sChildDataPlan->plan.myTuple->row;
        aDataPlan->plan.myTuple->rid = sChildDataPlan->plan.myTuple->rid;

        // ������ ������ Setting
        *aFlag = QMC_ROW_DATA_EXIST;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ABORT_RECORD_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnPCRD::readRowWithIndexRow",
                                  "record not found" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
