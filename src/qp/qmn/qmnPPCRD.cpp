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
 * $Id: qmnPPCRD.cpp 90438 2021-04-02 08:20:57Z ahra.cho $
 *
 * Description :
 *     Parallel Partition Coordinator(PPCRD) Node
 **********************************************************************/

#include <ide.h>
#include <qmnPPCRD.h>
#include <qmnPRLQ.h>
#include <qmnScan.h>
#include <qmoUtil.h>
#include <qmoPartition.h>
#include <qcg.h>

/***********************************************************************
 *
 * Description :
 *    PPCRD ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::init( qcTemplate * aTemplate,
                       qmnPlan    * aPlan )
{
    qmncPPCRD * sCodePlan = (qmncPPCRD *) aPlan;
    qmndPPCRD * sDataPlan =
        (qmndPPCRD *) (aTemplate->tmplate.data + aPlan->offset);

    idBool        sJudge;

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aPlan->left  == NULL );
    IDE_DASSERT( aPlan->right == NULL );

    //---------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    sDataPlan->doIt = qmnPPCRD::doItDefault;

    // first initialization
    if ( (*sDataPlan->flag & QMND_PPCRD_INIT_DONE_MASK)
         == QMND_PPCRD_INIT_DONE_FALSE )
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

    if ( sCodePlan->selectedPartitionCount > 0 )
    {
        IDE_DASSERT( sCodePlan->plan.children != NULL );

        sDataPlan->selectedChildrenCount = sCodePlan->selectedPartitionCount;

        sJudge = ID_TRUE;
    }
    else
    {
        sJudge = ID_FALSE;
    }

    if ( sJudge == ID_TRUE )
    {
        if ( sCodePlan->constantFilter != NULL )
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

    if ( sJudge == ID_TRUE )
    {
        //------------------------------------------------
        // ���� �Լ� ����
        //------------------------------------------------
        sDataPlan->doIt = qmnPPCRD::doItFirst;

        *sDataPlan->flag &= ~QMND_PPCRD_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_PPCRD_ALL_FALSE_FALSE;

        // TODO1502:
        //  update, delete�� ���� flag�����ؾ� ��.
    }
    else
    {
        sDataPlan->doIt = qmnPPCRD::doItAllFalse;

        *sDataPlan->flag &= ~QMND_PPCRD_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_PPCRD_ALL_FALSE_TRUE;

        // update, delete�� ���� flag�����ؾ���.
    }

    //------------------------------------------------
    // ���� Data Flag �� �ʱ�ȭ
    //------------------------------------------------

    // Subquery���ο� ���ԵǾ� ���� ��� �ʱ�ȭ�Ͽ�
    // In Subquery Key Range�� �ٽ� ������ �� �ֵ��� �Ѵ�.
    *sDataPlan->flag &= ~QMND_PPCRD_INSUBQ_RANGE_BUILD_MASK;
    *sDataPlan->flag |= QMND_PPCRD_INSUBQ_RANGE_BUILD_SUCCESS;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *    PPCRD�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ���� Child Plan �� �����ϰ�, ���� ��� ���� child plan�� ����
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::doIt( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
    qmndPPCRD * sDataPlan =
        (qmndPPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );
}

IDE_RC qmnPPCRD::padNull( qcTemplate * aTemplate,
                          qmnPlan    * aPlan )
{
    qmncPPCRD * sCodePlan = (qmncPPCRD*) aPlan;
    qmndPPCRD * sDataPlan =
        (qmndPPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    // �ʱ�ȭ�� ������� ���� ��� �ʱ�ȭ�� ����
    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_PPCRD_INIT_DONE_MASK)
         == QMND_PPCRD_INIT_DONE_FALSE )
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

            IDU_FIT_POINT( "qmnPPCRD::padNull::cralloc::nullRow",
                            idERR_ABORT_InsufficientMemory );

            // Null Row�� ���� ���� �Ҵ�
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc(
                    sDataPlan->plan.myTuple->rowOffset,
                    (void**)&sDataPlan->nullRow ) != IDE_SUCCESS);

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

/***********************************************************************
 *
 * Description :
 *    ���� ������ ����Ѵ�.
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printPlan( qcTemplate   * aTemplate,
                            qmnPlan      * aPlan,
                            ULong          aDepth,
                            iduVarString * aString,
                            qmnDisplay     aMode )
{
    qmncPPCRD   * sCodePlan;
    qmndPPCRD   * sDataPlan;

    qmnChildren * sChildren;
    qcmIndex    * sIndex;
    qcTemplate  * sTemplate;
    UInt          i;

    sCodePlan = (qmncPPCRD*)aPlan;
    sDataPlan = (qmndPPCRD*)(aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = &aTemplate->planFlag[sCodePlan->planID];

    //----------------------------
    // Display ��ġ ����
    //----------------------------

    qmn::printSpaceDepth(aString, aDepth);

    //----------------------------
    // PartSCAN ��� ǥ��
    //----------------------------

    iduVarStringAppendLength( aString, "PARTITION-COORDINATOR ( TABLE: ", 31 );

    if ( ( sCodePlan->tableOwnerName.name != NULL ) &&
         ( sCodePlan->tableOwnerName.size > 0 ) )
    {
        iduVarStringAppendLength( aString,
                                  sCodePlan->tableOwnerName.name,
                                  sCodePlan->tableOwnerName.size );
        iduVarStringAppendLength( aString, ".", 1 );
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
        iduVarStringAppendLength( aString, " ", 1 );

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
    // PARALLEL ���.
    //----------------------------
    iduVarStringAppendLength( aString, ", PARALLEL", 10 );

    //----------------------------
    // PARTITION ���� ���.
    //----------------------------
    iduVarStringAppendFormat( aString,
                              ", PARTITION: %"ID_UINT32_FMT"/%"ID_UINT32_FMT"",
                              sCodePlan->selectedPartitionCount,
                              sCodePlan->partitionCount );

    IDE_DASSERT((sCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK) ==
                QMNC_SCAN_INDEX_TABLE_SCAN_FALSE);

    //----------------------------
    // Access Method ���
    //----------------------------

    // BUG-38192 : ���õ� index ���

    sIndex = sCodePlan->index;

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
        // Full Scan�� ���
        iduVarStringAppend(aString, ", FULL SCAN");
    }

    IDE_TEST( printAccessInfo( sDataPlan,
                               aString,
                               aMode ) != IDE_SUCCESS );

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString, sCodePlan->plan.qmgAllCost );

    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        qmn::printSpaceDepth(aString, aDepth);

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

    if ( QCU_TRCLOG_DISPLAY_CHILDREN == 1 )
    {
        // BUG-38192

        //----------------------------
        // Child PRLQ Plan�� ���� ���
        //----------------------------

        for ( sChildren = sCodePlan->plan.childrenPRLQ;
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

        //----------------------------
        // Child Plan�� ���� ���
        //----------------------------

        for ( sChildren = sCodePlan->plan.children;
              sChildren != NULL;
              sChildren = sChildren->next )
        {
            sTemplate = aTemplate;

            /*
             * BUG-38294
             * SCAN �� ����� template �� �Ѱ��־�� �Ѵ�.
             */
            if ((*sDataPlan->flag & QMND_PPCRD_INIT_DONE_MASK) ==
                QMND_PPCRD_INIT_DONE_TRUE)
            {
                for (i = 0; i < sDataPlan->mSCANToPRLQMapCount; i++)
                {
                    if (sDataPlan->mSCANToPRLQMap[i].mSCAN == sChildren->childPlan)
                    {
                        sTemplate =
                            qmnPRLQ::getTemplate(aTemplate,
                                                 sDataPlan->mSCANToPRLQMap[i].mPRLQ);
                        break;
                    }
                }
            }
            else
            {
                /* nothing to do */
            }
            IDE_TEST( sChildren->childPlan->printPlan( sTemplate,
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

IDE_RC qmnPPCRD::doItDefault( qcTemplate * /* aTemplate */,
                              qmnPlan    * /* aPlan */,
                              qmcRowFlag * /* aFlag */)
{
    /* ȣ��Ǿ�� �ȵ� */

    IDE_DASSERT( 0 );

    return IDE_FAILURE;
}

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
IDE_RC qmnPPCRD::doItAllFalse( qcTemplate * aTemplate,
                               qmnPlan    * aPlan,
                               qmcRowFlag * aFlag )
{
    qmncPPCRD * sCodePlan = (qmncPPCRD*) aPlan;
    qmndPPCRD * sDataPlan =
        (qmndPPCRD*) (aTemplate->tmplate.data + aPlan->offset);

    // BUG-48800
    if ( sCodePlan->mPrePruningPartHandle != NULL )
    {
        IDE_TEST( smiValidateObjects( sCodePlan->mPrePruningPartHandle,
                                      sCodePlan->mPrePruningPartSCN )
                  != IDE_SUCCESS );
    }

    // BUG-47599 emptyPartiton�� lock�� �ɸ� add partition op�� lock �浹�� �߻��մϴ�.
    if ( sCodePlan->tableRef->mEmptyPartRef != NULL )
    {
        IDE_TEST( smiValidateObjects( sCodePlan->tableRef->mEmptyPartRef->partitionHandle,
                                      sCodePlan->tableRef->mEmptyPartRef->partitionSCN )
                  != IDE_SUCCESS);
    }

    // ���ռ� �˻�
    IDE_DASSERT( ( sCodePlan->selectedPartitionCount == 0 ) ||
                 ( sCodePlan->constantFilter != NULL ) );
    IDE_DASSERT( ( *sDataPlan->flag & QMND_PPCRD_ALL_FALSE_MASK )
                 == QMND_PPCRD_ALL_FALSE_TRUE );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

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
IDE_RC qmnPPCRD::firstInit( qcTemplate * aTemplate,
                            qmncPPCRD  * aCodePlan,
                            qmndPPCRD  * aDataPlan )
{
    iduMemory* sMemory;
    UInt       sRowOffset;

    sMemory = aTemplate->stmt->qmxMem;

    // Tuple ��ġ�� ����
    aDataPlan->plan.myTuple = &aTemplate->tmplate.rows[aCodePlan->tupleRowID];

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

    IDE_DASSERT( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                 != QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

    aDataPlan->indexTuple = NULL;
    
    // fix BUG-9052
    aDataPlan->subQFilterDepCnt = 0;

    //---------------------------------
    // Predicate ������ �ʱ�ȭ
    //---------------------------------
    
    // partition filter ������ �Ҵ�

    IDE_TEST( allocPartitionFilter( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    aDataPlan->partitionFilter = NULL;
    
    //---------------------------------
    // Partitioned Table ���� ������ �ʱ�ȭ
    //---------------------------------
    
    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->tupleRowID )
              != IDE_SUCCESS );

    aDataPlan->nullRow = NULL;
    aDataPlan->lastChildNo = ID_UINT_MAX;
    aDataPlan->selectedChildrenCount = 0;

    if ( aCodePlan->selectedPartitionCount > 0 )
    {
        // children area�� ����.
        IDU_FIT_POINT( "qmnPPCRD::firstInit::alloc::childrenSCANArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->alloc(aCodePlan->selectedPartitionCount *
                                ID_SIZEOF(qmnChildren*),
                                (void**)&aDataPlan->childrenSCANArea)
                 != IDE_SUCCESS);

        // children PRLQ area�� ����.
        IDU_FIT_POINT( "qmnPPCRD::firstInit::alloc::childrenPRLQArea", 
                       idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->alloc(aCodePlan->PRLQCount *
                                ID_SIZEOF(qmnChildren),
                                (void**)&aDataPlan->childrenPRLQArea)
                 != IDE_SUCCESS);

        // intersect count array create
        IDU_FIT_POINT( "qmnPPCRD::firstInit::alloc::rangeIntersectCountArray", 
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->cralloc(aCodePlan->selectedPartitionCount *
                                  ID_SIZEOF(UInt),
                                  (void**)&aDataPlan->rangeIntersectCountArray)
                 != IDE_SUCCESS);

        IDE_DASSERT( ( aCodePlan->flag & QMNC_SCAN_INDEX_TABLE_SCAN_MASK )
                     != QMNC_SCAN_INDEX_TABLE_SCAN_TRUE );

        aDataPlan->childrenIndex = NULL;

        /* BUG-38294 */
        IDU_FIT_POINT( "qmnPPCRD::firstInit::alloc::SCANToPRLQMap", 
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->alloc(aCodePlan->selectedPartitionCount *
                                ID_SIZEOF(qmnPRLQChildMap),
                                (void**)&aDataPlan->mSCANToPRLQMap)
                 != IDE_SUCCESS);
        aDataPlan->mSCANToPRLQMapCount = 0;
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
    IDE_DASSERT( aDataPlan->plan.myTuple->cursorInfo == NULL );

    aDataPlan->updateColumnList = NULL;
    aDataPlan->cursorType       = SMI_SELECT_CURSOR;
    aDataPlan->inplaceUpdate    = ID_FALSE;
    aDataPlan->lockMode         = SMI_LOCK_READ;

    aDataPlan->isRowMovementUpdate = ID_FALSE;

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
        IDU_FIT_POINT( "qmnPPCRD::firstInit::alloc::diskRow", idERR_ABORT_InsufficientMemory );
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
    
    *aDataPlan->flag &= ~QMND_PPCRD_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_PPCRD_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *     Predicate�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printPredicateInfo( qcTemplate   * aTemplate,
                                     qmncPPCRD     * aCodePlan,
                                     ULong          aDepth,
                                     iduVarString * aString )
{
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

    // Filter ������ ���
    IDE_TEST( printFilterInfo( aTemplate,
                               aCodePlan,
                               aDepth,
                               aString ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *     partition Key Range�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printPartKeyRangeInfo( qcTemplate   * aTemplate,
                                        qmncPPCRD    * aCodePlan,
                                        ULong          aDepth,
                                        iduVarString * aString )
{
    // Fixed Key Range ���
    if (aCodePlan->partitionKeyRange != NULL)
    {
        qmn::printSpaceDepth(aString, aDepth);
        iduVarStringAppend( aString, " [ PARTITION KEY ]\n" );
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

/***********************************************************************
 *
 * Description :
 *     partition Filter�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printPartFilterInfo( qcTemplate   * aTemplate,
                                      qmncPPCRD    * aCodePlan,
                                      ULong          aDepth,
                                      iduVarString * aString )
{
    // Fixed Key Filter ���
    if (aCodePlan->partitionFilter != NULL)
    {
        qmn::printSpaceDepth(aString, aDepth);
        iduVarStringAppend( aString, " [ PARTITION FILTER ]\n" );
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

/***********************************************************************
 *
 * Description :
 *     Filter�� �� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printFilterInfo( qcTemplate   * aTemplate,
                                  qmncPPCRD    * aCodePlan,
                                  ULong          aDepth,
                                  iduVarString * aString )
{
    // Constant Filter ���
    if (aCodePlan->constantFilter != NULL)
    {
        qmn::printSpaceDepth(aString, aDepth);
        iduVarStringAppend( aString, " [ CONSTANT FILTER ]\n" );
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

    // Subquery Filter ���
    if (aCodePlan->subqueryFilter != NULL)
    {
        qmn::printSpaceDepth(aString, aDepth);
        iduVarStringAppend( aString, " [ SUBQUERY FILTER ]\n" );
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
        qmn::printSpaceDepth(aString, aDepth);
        iduVarStringAppend( aString, " [ NOT-NORMAL-FORM FILTER ]\n" );
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

/***********************************************************************
 *
 * Description :
 *    Partition Filter�� ���� ������ �Ҵ�޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::allocPartitionFilter( qcTemplate * aTemplate,
                                       qmncPPCRD  * aCodePlan,
                                       qmndPPCRD  * aDataPlan )
{
    iduMemory * sMemory;

    if ( aCodePlan->partitionFilter != NULL )
    {
        IDE_TEST(qmoKeyRange::estimateKeyRange(aTemplate,
                                               aCodePlan->partitionFilter,
                                               &aDataPlan->partitionFilterSize)
                 != IDE_SUCCESS);

        IDE_DASSERT( aDataPlan->partitionFilterSize > 0 );

        sMemory = aTemplate->stmt->qmxMem;

        IDU_FIT_POINT( "qmnPPCRD::allocPartitionFilter::alloc::partitionFilterArea",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST(sMemory->cralloc(aDataPlan->partitionFilterSize,
                                  (void**)&aDataPlan->partitionFilterArea)
                 != IDE_SUCCESS);
    }
    else
    {
        aDataPlan->partitionFilterSize = 0;
        aDataPlan->partitionFilterArea = NULL;
        aDataPlan->partitionFilter     = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnPPCRD::doItFirst( qcTemplate * aTemplate,
                            qmnPlan    * aPlan,
                            qmcRowFlag * aFlag )
{
    qmncPPCRD  * sCodePlan;
    qmndPPCRD  * sDataPlan;
    qmnPlan    * sChildPRLQPlan;
    qmnPlan    * sChildSCANPlan;
    qmndPRLQ   * sChildPRLQDataPlan;
    qmndPlan   * sChildSCANDataPlan;
    qmnChildren* sChild;
    UInt         sDiskRowOffset = 0;
    UInt         i;
    idBool       sExistDisk = ID_FALSE;

    sCodePlan = (qmncPPCRD*)aPlan;
    sDataPlan = (qmndPPCRD*)(aTemplate->tmplate.data + aPlan->offset);

    // Table�� IS Lock�� �Ǵ�.
    IDE_TEST(smiValidateAndLockObjects( (QC_SMI_STMT( aTemplate->stmt))->getTrans(),
                                        sCodePlan->table,
                                        sCodePlan->tableSCN,
                                        SMI_TBSLV_DDL_DML,
                                        SMI_TABLE_LOCK_IS,
                                        ID_ULONG_MAX,
                                        ID_FALSE )
             != IDE_SUCCESS);

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

    /*
     * PROJ-2205 rownum in DML
     * row movement update�� ���
     * first doIt�� child�� ��� cursor�� ����־� �ϴµ�..
     * parallel query ������ DML ���� ���ϹǷ� �׻� false
     */
    IDE_DASSERT( sDataPlan->isRowMovementUpdate == ID_FALSE);

    if ( sCodePlan->partitionFilter != NULL )
    {
        // partition filter ����
        IDE_TEST( makePartitionFilter( aTemplate,
                                       sCodePlan,
                                       sDataPlan ) != IDE_SUCCESS );

        if ( sDataPlan->partitionFilter != NULL )
        {
            IDE_TEST( partitionFiltering( aTemplate,
                                          sCodePlan,
                                          sDataPlan )
                      != IDE_SUCCESS );
        }
        else
        {
            /*
             * partition filter�� ����ٰ� ������ �� �ִ�.
             * host ������ ��� conversion������ �ȵ� �� ����.
             * �̶��� filtering�� ���� �ʴ´�.
             * => ����� �ø� ������..
             */
            IDE_DASSERT(0);
            makeChildrenSCANArea( aTemplate, aPlan );
        }
    }
    else
    {
        makeChildrenSCANArea( aTemplate, aPlan );
    }

    makeChildrenPRLQArea( aTemplate, aPlan );

    /*
     * PRLQ init,
     * PRLQ->mCurrentNode �� SCAN ����
     */
    for ( i = 0; 
          (i < sCodePlan->PRLQCount)  &&
          (i < sDataPlan->selectedChildrenCount);
          i++ )
    {
        sChild = &sDataPlan->childrenPRLQArea[i];
        IDE_TEST( sChild->childPlan->init( aTemplate,
                                           sChild->childPlan )
                  != IDE_SUCCESS );

        /*
         * PRLQ �� SCAN �� �����Ѵ�.
         *
         * sChild->childPlan : PRLQ
         * sDataPlan->childrenSCANArea[i]->childPlan : SCAN
         */
        qmnPRLQ::setChildNode( aTemplate,
                               sChild->childPlan,
                               sDataPlan->childrenSCANArea[i]->childPlan );

        /* BUG-38294 */
        sDataPlan->mSCANToPRLQMap[sDataPlan->mSCANToPRLQMapCount].mSCAN =
            sDataPlan->childrenSCANArea[i]->childPlan;
        sDataPlan->mSCANToPRLQMap[sDataPlan->mSCANToPRLQMapCount].mPRLQ =
            sChild->childPlan;
        sDataPlan->mSCANToPRLQMapCount++;

        /* PRLQ �� ������ ������ SCAN �� ��ȣ */
        sDataPlan->lastChildNo = i;
    }

    // ���õ� children�� �ִ� ��츸 ����.
    if ( sDataPlan->selectedChildrenCount > 0 )
    {
        /* SCAN init */
        for ( i = 0; i < sDataPlan->selectedChildrenCount; i++ )
        {
            sChild = sDataPlan->childrenSCANArea[i];
            IDE_TEST( sChild->childPlan->init( aTemplate,
                                               sChild->childPlan )
                      != IDE_SUCCESS );

            /* PROJ-2464 hybrid partitioned table ����
             *  - PRLQ�� �޸� �Ҵ��� ����ȭ�ϱ� ���ؼ�, Disk Partition�� ���ԵǾ����� �˻��Ѵ�.
             *    1. �Ϻ� PLAN�� Disk�� ���ԵǾ����� �˻��Ѵ�.
             *    2. PRLQ�� �߰����� ������ �����Ѵ�.
             */
            /* 1. �Ϻ� PLAN�� Disk�� ���ԵǾ����� �˻��Ѵ�. */
            if ( ( sChild->childPlan->flag & QMN_PLAN_STORAGE_MASK ) == QMN_PLAN_STORAGE_DISK )
            {
                sExistDisk         = ID_TRUE;
                sChildSCANDataPlan = (qmndPlan *)(aTemplate->tmplate.data + sChild->childPlan->offset);
                sDiskRowOffset     = sChildSCANDataPlan->myTuple->rowOffset;
            }
            else
            {
                /* Nothing to do */
            }
        }

        for (i = 0; (i < sCodePlan->PRLQCount) && (i < sDataPlan->selectedChildrenCount); i++)
        {
            sChild = &sDataPlan->childrenPRLQArea[i];

            /* 2. PRLQ�� �߰����� ������ �����Ѵ�. */
            qmnPRLQ::setPRLQInfo( aTemplate,
                                  sChild->childPlan,
                                  sExistDisk,
                                  sDiskRowOffset );

            // BUG-41931
            // child�� doIt�ÿ��� plan.myTuple�� ������ �� �����Ƿ�
            // myTuple�� columns / partitionTupleID�� child�� ������ �����ؾ� �Ѵ�.
            // PPCRD������, �� �۾��� doItNext() ������ �̷������ �־���.
            sChildPRLQPlan     = sChild->childPlan;
            sChildPRLQDataPlan = (qmndPRLQ*)( aTemplate->tmplate.data +
                                              sChildPRLQPlan->offset );

            // SCAN �÷��� ������ ���� �����ؾ���
            if ( sChildPRLQDataPlan->mCurrentNode->type == QMN_GRAG )
            {
                sChildSCANPlan = sChildPRLQDataPlan->mCurrentNode->left;
            }
            else
            {
                sChildSCANPlan = sChildPRLQDataPlan->mCurrentNode;
            }

            sChildSCANDataPlan = (qmndPlan*)( aTemplate->tmplate.data +
                                              sChildSCANPlan->offset );

            sDataPlan->plan.myTuple->columns = 
                sChildSCANDataPlan->myTuple->columns;

            sDataPlan->plan.myTuple->partitionTupleID =
                ((qmncSCAN*)sChildSCANPlan)->tupleRowID;

            sDataPlan->plan.myTuple->tableHandle = (void*)((qmncSCAN*)sChildSCANPlan)->table;

            /* PROJ-2464 hybrid partitioned table ���� */
            sDataPlan->plan.myTuple->rowOffset  = sChildSCANDataPlan->myTuple->rowOffset;
            sDataPlan->plan.myTuple->rowMaximum = sChildSCANDataPlan->myTuple->rowMaximum;

            IDE_TEST(qmnPRLQ::startIt(aTemplate,
                                      sChild->childPlan,
                                      NULL)
                     != IDE_SUCCESS);
        }

        sDataPlan->doIt = qmnPPCRD::doItNext;

        // Record�� ȹ���Ѵ�.
        IDE_TEST(sDataPlan->doIt(aTemplate, aPlan, aFlag) != IDE_SUCCESS);
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

IDE_RC qmnPPCRD::makePartitionFilter( qcTemplate * aTemplate,
                                      qmncPPCRD   * aCodePlan,
                                      qmndPPCRD   * aDataPlan )
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

/* ------------------------------------------------------------------
 * doIt function
 * ------------------------------------------------------------------
 * Queue �� ���ڵ尡 ����ִ� childrenPRLQArea �� �����Ѵ�.
 * Queue �� ��������� ���� childrenPRLQArea �� �����ϰ�,
 * ������ �������� childrenPRLQArea ���� �����Ѵ�.
 * ------------------------------------------------------------------
 */
IDE_RC qmnPPCRD::doItNext( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
    idBool    sJudge;
    qmnPlan * sChildPRLQPlan;
    qmnPlan * sChildSCANPlan;
    qmndPRLQ* sChildPRLQDataPlan;
    qmndPlan* sChildSCANDataPlan;
    mtcTuple* sPartitionTuple;

    qmncPPCRD* sCodePlan = (qmncPPCRD*)aPlan;
    qmndPPCRD* sDataPlan = (qmndPPCRD*)(aTemplate->tmplate.data + aPlan->offset);

    while ( 1 )
    {
        //----------------------------
        // ���� Child�� ����
        //----------------------------

        sChildPRLQPlan = sDataPlan->curPRLQ->childPlan;
        sChildPRLQDataPlan = (qmndPRLQ*)( aTemplate->tmplate.data +
                                          sChildPRLQPlan->offset );

        // PROJ-2444
        // SCAN �÷��� ������ ���� �����ؾ���
        if ( sChildPRLQDataPlan->mCurrentNode->type == QMN_GRAG )
        {
            sChildSCANPlan = sChildPRLQDataPlan->mCurrentNode->left;
        }
        else
        {
            sChildSCANPlan = sChildPRLQDataPlan->mCurrentNode;
        }

        sChildSCANDataPlan = (qmndPlan*)( aTemplate->tmplate.data +
                                          sChildSCANPlan->offset );

        // isNeedAllFetchColumn �� row movement update �ÿ� ID_TRUE �� �����ȴ�.
        // Parallel query �� row movement update �� �������� �ʱ� ������
        // isNeedAllFetchColumn �� �׻� �⺻���� ID_FALSE �̴�.
        // ���� ���� SCAN ���� �����ϴ� ������ �����ϵ��� �Ѵ�.
        // sChildSCANDataPlan->isNeedAllFetchColumn = aDataPlan->isNeedAllFetchColumn;

        // PROJ-2002 Column Security
        // child�� doIt�ÿ��� plan.myTuple�� ������ �� �ִ�.
        // BUGBUG1701
        // ���⼭ PPCRD �� plan.myTuple �� �����ص�
        // Thread �� ���� ����Ǵ� ������ plan.myTuple �� �����ؼ� �����ϹǷ�
        // �ǵ��Ѵ�� �۵����� ���� �� �ִ�.
        // �ٸ� ���⼭ �ϰ��� �ϴ� �۾��� SCAN ��忡�� doIt ��
        //  ���� ����� PPCRD �� plan.myTuple �� SCAN �� columns ��
        //  partitionTupleID �� �������� �ϴ� ���̹Ƿ� thread ����
        //  template ���� �ÿ� �� �κ��� �����ϸ� �� ������
        //  �ذ� �� ������ ���δ�.
        sDataPlan->plan.myTuple->columns = sChildSCANDataPlan->myTuple->columns;

        sDataPlan->plan.myTuple->partitionTupleID =
            ((qmncSCAN*)sChildSCANPlan)->tupleRowID;

        // PROJ-2444 Parallel Aggreagtion
        sPartitionTuple = &aTemplate->tmplate.rows[sDataPlan->plan.myTuple->partitionTupleID];

        /* BUG-39073 */
        sDataPlan->plan.myTuple->tableHandle = (void*)((qmncSCAN*)sChildSCANPlan)->table;

        /* PROJ-2464 hybrid partitioned table ���� */
        sDataPlan->plan.myTuple->rowOffset  = sChildSCANDataPlan->myTuple->rowOffset;
        sDataPlan->plan.myTuple->rowMaximum = sChildSCANDataPlan->myTuple->rowMaximum;

        IDE_TEST( sChildPRLQPlan->doIt( aTemplate, sChildPRLQPlan, aFlag )
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

            // PROJ-2444
            // �ڽ� PRLQ �� ����Ÿ�� ���� �о�´�.
            sDataPlan->plan.myTuple->rid = sChildPRLQDataPlan->mRid;
            sDataPlan->plan.myTuple->row = sChildPRLQDataPlan->mRow;

            // ��Ƽ���� Ʃ�ÿ��� ���� �����ؾ� �Ѵ�.
            // lob ���ڵ带 ������ partitionTupleID �� �̿��Ѵ�.
            sPartitionTuple->rid = sChildPRLQDataPlan->mRid;
            sPartitionTuple->row = sChildPRLQDataPlan->mRow;

            sDataPlan->plan.myTuple->modify++;

            // judge
            if ( sCodePlan->subqueryFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      sCodePlan->subqueryFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                sJudge = ID_TRUE;
            }

            if ( sCodePlan->nnfFilter != NULL )
            {
                IDE_TEST( qtc::judge( & sJudge,
                                      sCodePlan->nnfFilter,
                                      aTemplate )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            if ( sJudge == ID_TRUE )
            {
                if ( ( *aFlag & QMC_ROW_QUEUE_EMPTY_MASK )
                    == QMC_ROW_QUEUE_EMPTY_TRUE )
                {
                    // Record exists and Thread stoped
                    // OR Record exists and Serial executed

                    // BUGBUG1071: All rows read = Serial executed
                    // Serial execute ���� ��Ÿ���� flag �� ������ �ذ�� ��
                    // ���� PRLQ �� �д´�.
                    sDataPlan->prevPRLQ = sDataPlan->curPRLQ;
                    sDataPlan->curPRLQ  = sDataPlan->curPRLQ->next;
                }
                else
                {
                    // Record exists and Thread still running
                    // Nothing to do

                    // BUGBUG1071
                    // Tuning point;
                    // Ư�� record ���� �������� ã�Ƴ��ų�
                    // ���� ������ ����ؼ� PRLQ �� ���� �����ϵ��� �ϸ�
                    // ���� ��� ������ �� �� �ϴ�.
                    if ( sDataPlan->plan.myTuple->modify % 1000 == 0 )
                    {
                        sDataPlan->prevPRLQ = sDataPlan->curPRLQ;
                        sDataPlan->curPRLQ  = sDataPlan->curPRLQ->next;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }

                *aFlag = QMC_ROW_DATA_EXIST;
                break;
            }
            else
            {
                /* judge fail, try next row */
            }
        }
        else
        {
            /* QMC_ROW_DATA_NONE */

            if( ( *aFlag & QMC_ROW_QUEUE_EMPTY_MASK ) ==
                QMC_ROW_QUEUE_EMPTY_TRUE )
            {
                /* All rows read */

                if ( sDataPlan->lastChildNo + 1 ==
                     sDataPlan->selectedChildrenCount )
                {
                    // �ش� PRLQ �� ���ڵ嵵 ����
                    // �� �̻� ������ SCAN ��嵵 ����.
                    // ���� ���� ��󿡼� �����Ѵ�.

                    if ( sChildPRLQDataPlan->mThrObj != NULL )
                    {
                        // PRLQ �� thread �� ��ȯ�Ѵ�.
                        qmnPRLQ::returnThread( aTemplate,
                                               sDataPlan->curPRLQ->childPlan );
                    }

                    if ( sDataPlan->prevPRLQ == sDataPlan->curPRLQ )
                    {
                        // �� �̻� ������ PRLQ �� ���� ���
                        sDataPlan->curPRLQ = NULL;
                        sDataPlan->prevPRLQ = NULL;

                        *aFlag = QMC_ROW_DATA_NONE;
                        break;
                    }
                    else
                    {
                        // ���� PRLQ node �� list ���� �����Ѵ�.
                        sDataPlan->prevPRLQ->next = sDataPlan->curPRLQ->next;
                        sDataPlan->curPRLQ        = sDataPlan->curPRLQ->next;
                    }
                }
                else
                {
                    // �ش� PRLQ �� ���ڵ尡 ����.
                    // ���� ���� SCAN ��带 �Ҵ��ϰ� �����Ѵ�.
                    sDataPlan->lastChildNo++;
                    qmnPRLQ::setChildNode( aTemplate,
                                           sDataPlan->curPRLQ->childPlan,
                                           sDataPlan->childrenSCANArea[sDataPlan->lastChildNo]->childPlan );

                    /* BUG-38294 */
                    sDataPlan->mSCANToPRLQMap[sDataPlan->mSCANToPRLQMapCount].mSCAN =
                        sDataPlan->childrenSCANArea[sDataPlan->lastChildNo]->childPlan;
                    sDataPlan->mSCANToPRLQMap[sDataPlan->mSCANToPRLQMapCount].mPRLQ =
                        sDataPlan->curPRLQ->childPlan;
                    sDataPlan->mSCANToPRLQMapCount++;
                }
            }
            else
            {
                // SCAN �۾��� ���������� ��� queue �� ����ִ�.
                // ���� PRLQ �� �д´�.
                sDataPlan->prevPRLQ = sDataPlan->curPRLQ;
                sDataPlan->curPRLQ  = sDataPlan->curPRLQ->next;
            }
        }
    }

    if ( ( *aFlag & QMC_ROW_DATA_MASK ) == QMC_ROW_DATA_NONE )
    {
        // record�� ���� ���
        // ���� ������ ���� ���� ���� �Լ��� ������.
        sDataPlan->doIt = qmnPPCRD::doItFirst;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *     Access ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
IDE_RC qmnPPCRD::printAccessInfo( qmndPPCRD    * aDataPlan,
                                  iduVarString * aString,
                                  qmnDisplay     aMode )
{
    IDE_DASSERT( aDataPlan != NULL );
    IDE_DASSERT( aString   != NULL );

    if ( aMode == QMN_DISPLAY_ALL )
    {
        //----------------------------
        // explain plan = on; �� ���
        //----------------------------

        // ���� Ƚ�� ���
        if ( (*aDataPlan->flag & QMND_PPCRD_INIT_DONE_MASK)
             == QMND_PPCRD_INIT_DONE_TRUE )
        {
            // fix BUG-9052
            iduVarStringAppendFormat( aString,
                                      ", ACCESS: %"ID_UINT32_FMT"",
                                      (aDataPlan->plan.myTuple->modify
                                       - aDataPlan->subQFilterDepCnt) );
        }
        else
        {
            iduVarStringAppend( aString, ", ACCESS: 0" );
        }
    }
    else
    {
        //----------------------------
        // explain plan = only; �� ���
        //----------------------------

        iduVarStringAppend( aString, ", ACCESS: ??" );
    }

    return IDE_SUCCESS;
}

IDE_RC qmnPPCRD::partitionFiltering( qcTemplate * aTemplate,
                                     qmncPPCRD  * aCodePlan,
                                     qmndPPCRD  * aDataPlan )
{
    return qmoPartition::partitionFilteringWithPartitionFilter(
        aTemplate->stmt,
        aCodePlan->rangeSortedChildrenArray,
        aDataPlan->rangeIntersectCountArray,
        aCodePlan->selectedPartitionCount,
        aCodePlan->partitionCount,
        aCodePlan->plan.children,
        aCodePlan->tableRef->tableInfo->partitionMethod,
        aDataPlan->partitionFilter,
        aDataPlan->childrenSCANArea,
        &aDataPlan->selectedChildrenCount);
}

void qmnPPCRD::makeChildrenSCANArea( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan )
{
    qmncPPCRD  * sCodePlan;
    qmndPPCRD  * sDataPlan;
    qmnChildren* sChild;
    qmsTableRef* sTableRef;
    UInt         i;

    sCodePlan = (qmncPPCRD*)aPlan;
    sDataPlan = (qmndPPCRD*)(aTemplate->tmplate.data + aPlan->offset);
    sTableRef = sCodePlan->tableRef;

    /* PROJ-2249 method range �̸� ���ĵ� children�� �Ҵ� �Ѵ�. */
    if ( ( sTableRef->tableInfo->partitionMethod == QCM_PARTITION_METHOD_RANGE ) ||
         ( sTableRef->tableInfo->partitionMethod == QCM_PARTITION_METHOD_RANGE_USING_HASH ) )
    {
        for ( i = 0; i < sDataPlan->selectedChildrenCount; i++ )
        {
            sDataPlan->childrenSCANArea[i] =
                sCodePlan->rangeSortedChildrenArray[i].children;
        }
    }
    else
    {
        for ( i = 0, sChild = sCodePlan->plan.children;
              ( i < sDataPlan->selectedChildrenCount ) && ( sChild != NULL );
              i++, sChild = sChild->next )
        {
            sDataPlan->childrenSCANArea[i] = sChild;
        }
    }
}

void qmnPPCRD::makeChildrenPRLQArea( qcTemplate * aTemplate,
                                     qmnPlan    * aPlan )
{
    qmncPPCRD  * sCodePlan;
    qmndPPCRD  * sDataPlan;
    qmnChildren* sChild;
    qmnChildren* sPrev;
    UInt         i;

    sCodePlan = (qmncPPCRD*)aPlan;
    sDataPlan = (qmndPPCRD*)(aTemplate->tmplate.data + aPlan->offset);
    sChild    = NULL;
    sPrev     = NULL;

    if ( (sCodePlan->plan.childrenPRLQ != NULL) &&
         (sDataPlan->selectedChildrenCount > 0) )
    {
        for ( i = 0, sChild = sCodePlan->plan.childrenPRLQ;
              ( i < sCodePlan->PRLQCount ) &&
              ( i < sDataPlan->selectedChildrenCount ) &&
              ( sChild != NULL );
              i++, sChild = sChild->next )
        {
            sDataPlan->childrenPRLQArea[i].childPlan = sChild->childPlan;
            if ( sPrev == NULL )
            {
                sPrev = &sDataPlan->childrenPRLQArea[i];
            }
            else
            {
                sPrev->next = &sDataPlan->childrenPRLQArea[i];
                sPrev       = &sDataPlan->childrenPRLQArea[i];
            }
        }

        // PRLQ �� �ϳ��� ���ٸ� ������ �Ұ����ϴ�.
        IDE_DASSERT( sPrev != NULL );

        // ���� ����Ʈ�� �����.
        sPrev->next = sDataPlan->childrenPRLQArea;

        // curPRLQ, prevPRLQ �� �ʱ�ȭ
        sDataPlan->prevPRLQ = sPrev;
        sDataPlan->curPRLQ  = sDataPlan->childrenPRLQArea;
    }
    else
    {
        // ������ child ��尡 ����.
        sDataPlan->childrenPRLQArea = NULL;
        sDataPlan->prevPRLQ = NULL;
        sDataPlan->curPRLQ  = NULL;
    }
}
