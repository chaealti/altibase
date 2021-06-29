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
 * $Id: qmnViewScan.cpp 85333 2019-04-26 02:34:41Z et16 $
 *
 * Description :
 *     VSCN(View SCaN) Node
 *
 *     ������ �𵨿��� Materialized View�� ����
 *     Selection�� �����ϴ� Node�̴�.
 *
 *     ���� VMTR ����� ���� ��ü�� ���� �ٸ� ������ �ϰ� �Ǹ�,
 *     Memory Temp Table�� ��� Memory Sort Temp Table ��ü��
 *        interface�� ���� ����Ͽ� �����ϸ�,
 *     Disk Temp Table�� ��� table handle�� index handle�� ���
 *        ������ Cursor�� ���� Sequetial Access�Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcuProperty.h>
#include <qmnViewMaterialize.h>
#include <qmnViewScan.h>

IDE_RC
qmnVSCN::init( qcTemplate * aTemplate,
               qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    VSCN ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::init"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];
    sDataPlan->doIt = qmnVSCN::doItDefault;
    
    // first initialization
    if ( (*sDataPlan->flag & QMND_VSCN_INIT_DONE_MASK)
         == QMND_VSCN_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // PROJ-2415 Grouping Sets Clause
        // VMTR�� Dependency ó���߰� �� ���� ����
        IDE_TEST( initForChild( aTemplate, sCodePlan, sDataPlan ) != IDE_SUCCESS );
    }
         
    if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        sDataPlan->doIt = qmnVSCN::doItFirstMem;
    }
    else
    {
        sDataPlan->doIt = qmnVSCN::doItFirstDisk;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::doIt( qcTemplate * aTemplate,
               qmnPlan    * aPlan,
               qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    VSCN�� ���� ����� �����Ѵ�.
 *
 * Implementation :
 *    ������ �Լ� �����͸� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::doIt"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );

#undef IDE_FN
}


IDE_RC
qmnVSCN::padNull( qcTemplate * aTemplate,
                  qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    ���� ��ü�� ���� null row�� setting�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::padNull"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);
    mtcColumn * sColumn;
    UInt        i;

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_VSCN_INIT_DONE_MASK)
         == QMND_VSCN_INIT_DONE_FALSE )
    {
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    if ( (sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        //----------------------------------
        // Memory Temp Table�� ���
        //----------------------------------

        sDataPlan->plan.myTuple->row = sDataPlan->nullRow;

        // PROJ-2362 memory temp ���� ȿ���� ����
        sColumn = sDataPlan->plan.myTuple->columns;
        for ( i = 0; i < sDataPlan->plan.myTuple->columnCount; i++, sColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sColumn->column.flag ) == ID_TRUE )
            {
                sColumn->module->null( sColumn,
                                       sColumn->column.value );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        //----------------------------------
        // Disk Temp Table�� ���
        //----------------------------------

        idlOS::memcpy( sDataPlan->plan.myTuple->row,
                       sDataPlan->nullRow,
                       sDataPlan->nullRowSize );
        idlOS::memcpy( & sDataPlan->plan.myTuple->rid,
                       & sDataPlan->nullRID,
                       ID_SIZEOF(scGRID) );
    }

    // Null Padding�� record�� ���� ����
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::printPlan( qcTemplate   * aTemplate,
                    qmnPlan      * aPlan,
                    ULong          aDepth,
                    iduVarString * aString,
                    qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *   VSCN ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::printPlan"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    SChar      sNameBuffer[ QC_MAX_OBJECT_NAME_LEN + 1 ];

    qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    ULong  i;

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString,
                            " " );
    }

    //-------------------------------
    // View�� �̸� �� alias name ���
    //-------------------------------

    iduVarStringAppend( aString,
                        "VIEW-SCAN ( " );

    if ( ( sCodePlan->viewName.name != NULL ) &&
         ( sCodePlan->viewName.size != QC_POS_EMPTY_SIZE ) )
    {
        iduVarStringAppend( aString,
                            "VIEW: " );

        if ( ( sCodePlan->viewOwnerName.name != NULL ) &&
             ( sCodePlan->viewOwnerName.size > 0 ) )
        {
            iduVarStringAppend( aString,
                                sCodePlan->viewOwnerName.name );
            iduVarStringAppend( aString,
                                "." );
        }
        else
        {
            // Nothing to do.
        }

        if ( sCodePlan->viewName.size <= QC_MAX_OBJECT_NAME_LEN )
        {
            idlOS::memcpy( sNameBuffer,
                           sCodePlan->viewName.name,
                           sCodePlan->viewName.size );
            sNameBuffer[sCodePlan->viewName.size] = '\0';

            iduVarStringAppend( aString,
                                sNameBuffer );
        }
        else
        {
            // Nothing to do.
        }

        if ( sCodePlan->aliasName.name != NULL &&
                        sCodePlan->aliasName.size != QC_POS_EMPTY_SIZE &&
                        sCodePlan->aliasName.name != sCodePlan->viewName.name )
        {
            // View �̸� ������ Alias �̸� ������ �ٸ� ���
            // (alias name)
            iduVarStringAppend( aString,
                                " " );

            if ( sCodePlan->aliasName.size <= QC_MAX_OBJECT_NAME_LEN )
            {
                idlOS::memcpy( sNameBuffer,
                               sCodePlan->aliasName.name,
                               sCodePlan->aliasName.size );
                sNameBuffer[sCodePlan->aliasName.size] = '\0';

                iduVarStringAppend( aString,
                                    sNameBuffer );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Alias �̸� ������ ���ų� View �̸� ������ ������ ���
            // Nothing To Do
        }

        iduVarStringAppend( aString,
                            ", " );
    }
    else
    {
        // Nothing to do.
    }

    //-------------------------------
    // Access ������ ���
    //-------------------------------

    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( (*sDataPlan->flag & QMND_VSCN_INIT_DONE_MASK)
             == QMND_VSCN_INIT_DONE_TRUE )
        {
            iduVarStringAppendFormat( aString,
                                      "ACCESS: %"ID_UINT32_FMT,
                                      sDataPlan->plan.myTuple->modify );
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "ACCESS: 0" );
        }
    }
    else
    {
        iduVarStringAppendFormat( aString,
                                  "ACCESS: ??" );
    }

    //----------------------------
    // Cost ���
    //----------------------------
    qmn::printCost( aString,
                    sCodePlan->plan.qmgAllCost );

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

    //-------------------------------
    // Child Plan�� ���
    //-------------------------------

    IDE_TEST( aPlan->left->printPlan( aTemplate,
                                      aPlan->left,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::doItDefault( qcTemplate * /* aTemplate */,
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

#define IDE_FN "qmnVSCN::doItDefault"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_DASSERT( 0 );

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::doItFirstMem( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Memory Temp Table ��� �� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::doItFirstMem"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    if ( ( *sDataPlan->flag & QMND_VSCN_INIT_DONE_MASK )
         == QMND_VSCN_INIT_DONE_FALSE )
    {
        IDE_TEST( qmnVSCN::init( aTemplate, aPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    // ���� Record ��ġ�κ��� ȹ��
    sDataPlan->recordPos = 0;

    sDataPlan->doIt = qmnVSCN::doItNextMem;

    return qmnVSCN::doItNextMem( aTemplate, aPlan, aFlag );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnVSCN::doItNextMem( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Memory Temp Table ��� �� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::doItNextMem"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    if ( sDataPlan->recordPos < sDataPlan->recordCnt )
    {
        IDE_TEST( qmcMemSort::getElement( sDataPlan->memSortMgr,
                                          sDataPlan->recordPos,
                                          & sDataPlan->plan.myTuple->row )
                  != IDE_SUCCESS );
        IDE_DASSERT( sDataPlan->plan.myTuple->row != NULL );

        IDE_TEST( setTupleSet( aTemplate,
                               sCodePlan,
                               sDataPlan )
                  != IDE_SUCCESS );
        
        sDataPlan->recordPos++;
        sDataPlan->plan.myTuple->modify++;

        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        *aFlag = QMC_ROW_DATA_NONE;
        sDataPlan->doIt = qmnVSCN::doItFirstMem;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::doItFirstDisk( qcTemplate * aTemplate,
                        qmnPlan    * aPlan,
                        qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table ��� �� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::doItFirstDisk"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics )
              != IDE_SUCCESS );

    IDE_TEST( openCursor( sDataPlan )
              != IDE_SUCCESS );

    sDataPlan->doIt = qmnVSCN::doItNextDisk;

    return qmnVSCN::doItNextDisk( aTemplate, aPlan, aFlag );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnVSCN::doItNextDisk( qcTemplate * aTemplate,
                       qmnPlan    * aPlan,
                       qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description :
 *    Disk Temp Table ��� �� ���� ���� �Լ�
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::doItNextDisk"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    // qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);

    void * sOrgRow;
    void * sSearchRow;

    // ���� ���� ������ ���� Pointer ���
    sOrgRow = sSearchRow = sDataPlan->plan.myTuple->row;

    // Cursor�� �̿��Ͽ� Record�� ����
    IDE_TEST( smiSortTempTable::fetch( sDataPlan->tempCursor,
                                       (UChar**) & sSearchRow,
                                       & sDataPlan->plan.myTuple->rid )
              != IDE_SUCCESS );

    if ( sSearchRow != NULL )
    {
        //------------------------------
        // ������ �����ϴ� ���
        //------------------------------

        sDataPlan->plan.myTuple->row = sSearchRow;
        sDataPlan->plan.myTuple->modify++;

        *aFlag &= ~QMC_ROW_DATA_MASK;
        *aFlag |= QMC_ROW_DATA_EXIST;
    }
    else
    {
        //------------------------------
        // �����Ͱ� ���� ���
        //------------------------------

        sDataPlan->plan.myTuple->row = sOrgRow;

        *aFlag = QMC_ROW_DATA_NONE;

        sDataPlan->doIt = qmnVSCN::doItFirstDisk;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qmnVSCN::firstInit( qcTemplate * aTemplate,
                    qmncVSCN   * aCodePlan,
                    qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    VMTR Child�� ������ ��,
 *    VSCN node�� Data ������ ����� ���� �ʱ�ȭ�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::firstInit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->plan.left->type == QMN_VMTR );

    //---------------------------------
    // Child ����
    //---------------------------------

    IDE_TEST( execChild( aTemplate, aCodePlan )
              != IDE_SUCCESS );

    //---------------------------------
    // Data Member�� �ʱ�ȭ
    //---------------------------------

    // Tuple �� �ʱ�ȭ
    // Tuple�� �����ϴ� column�� offset�� �����ϰ�,
    // Row�� ���� size��� �� ������ �Ҵ�޴´�.
    aDataPlan->plan.myTuple =
        & aTemplate->tmplate.rows[aCodePlan->tupleRowID];

    IDE_TEST( refineOffset( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    IDE_TEST( qmc::setRowSize( aTemplate->stmt->qmxMem,
                               & aTemplate->tmplate,
                               aCodePlan->tupleRowID )
              != IDE_SUCCESS );

    // Row Size ȹ��
    IDE_TEST( qmnVMTR::getNullRowSize( aTemplate,
                                       aCodePlan->plan.left,
                                       & aDataPlan->nullRowSize )
              != IDE_SUCCESS );
    
    // Null Row �ʱ�ȭ
    IDE_TEST( getNullRow( aTemplate, aCodePlan, aDataPlan )
              != IDE_SUCCESS );

    //---------------------------------
    // Temp Table ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( qmnVMTR::getCursorInfo( aTemplate,
                                      aCodePlan->plan.left,
                                      & aDataPlan->tableHandle,
                                      & aDataPlan->indexHandle,
                                      & aDataPlan->memSortMgr,
                                      & aDataPlan->memSortRecord )
              != IDE_SUCCESS );

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        // ���ռ� �˻�
        IDE_DASSERT( aDataPlan->tableHandle == NULL );
        IDE_DASSERT( aDataPlan->indexHandle == NULL );
        IDE_DASSERT( aDataPlan->memSortMgr != NULL );

        IDE_TEST( qmcMemSort::getNumOfElements( aDataPlan->memSortMgr,
                                                & aDataPlan->recordCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���ռ� �˻�
        IDE_DASSERT( aDataPlan->tableHandle != NULL );
        IDE_DASSERT( aDataPlan->memSortMgr == NULL );

        // Temp Cursor �ʱ�ȭ
        aDataPlan->tempCursor = NULL;

        // PROJ-1597 Temp record size ���� ����
        aDataPlan->plan.myTuple->tableHandle = aDataPlan->tableHandle;
    }

    //---------------------------------
    // �ʱ�ȭ �ϷḦ ǥ��
    //---------------------------------

    *aDataPlan->flag &= ~QMND_VSCN_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_VSCN_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnVSCN::initForChild( qcTemplate * aTemplate,
                              qmncVSCN   * aCodePlan,
                              qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-2415 Grouping Sets Clause�� ����
 *    VMTR�� Depedency�� ���� ����� ������, ���� firstInit���� �ѹ��� �����ϴ�
 *    VMTR Child�� ���� init������ �����ϵ��� ����.
 *    
 *    VMTR Child�� ������ ��,
 *    VSCN node�� Data ������ ����� ���� �缳���� ������.
 *
 * Implementation :
 *
 ***********************************************************************/
    //---------------------------------
    // ���ռ� �˻�
    //---------------------------------

    IDE_DASSERT( aCodePlan->plan.left->type == QMN_VMTR );
    
    //---------------------------------
    // Child ����
    //---------------------------------

    IDE_TEST( execChild( aTemplate, aCodePlan )
              != IDE_SUCCESS );

    //---------------------------------
    // Temp Table ���� ������ �ʱ�ȭ
    //---------------------------------

    IDE_TEST( qmnVMTR::getCursorInfo( aTemplate,
                                      aCodePlan->plan.left,
                                      & aDataPlan->tableHandle,
                                      & aDataPlan->indexHandle,
                                      & aDataPlan->memSortMgr,
                                      & aDataPlan->memSortRecord )
              != IDE_SUCCESS );

    if ( (aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK)
         == QMN_PLAN_STORAGE_MEMORY )
    {
        // ���ռ� �˻�
        IDE_DASSERT( aDataPlan->tableHandle == NULL );
        IDE_DASSERT( aDataPlan->indexHandle == NULL );
        IDE_DASSERT( aDataPlan->memSortMgr != NULL );

        IDE_TEST( qmcMemSort::getNumOfElements( aDataPlan->memSortMgr,
                                                & aDataPlan->recordCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���ռ� �˻�
        IDE_DASSERT( aDataPlan->tableHandle != NULL );
        IDE_DASSERT( aDataPlan->memSortMgr == NULL );

        // Temp Cursor �ʱ�ȭ
        aDataPlan->tempCursor = NULL;

        // PROJ-1597 Temp record size ���� ����
        aDataPlan->plan.myTuple->tableHandle = aDataPlan->tableHandle;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmnVSCN::refineOffset( qcTemplate * aTemplate,
                       qmncVSCN   * aCodePlan,
                       qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Tuple�� �����ϴ� Column�� offset ������
 *
 * Implementation :
 *    qmc::refineOffsets()�� ������� ����.
 *    VSCN ���� qmdMtrNode�� ������, VMTR�� ��� ������ �����ϱ⸸
 *    �Ѵ�.  ����, ���������� offset�� refine�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::refineOffset"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    mtcTuple  * sVMTRTuple;

    IDE_DASSERT( aDataPlan->plan.myTuple != NULL );
    IDE_DASSERT( aDataPlan->plan.myTuple->columnCount > 0 );

    IDE_TEST( qmnVMTR::getTuple( aTemplate,
                                 aCodePlan->plan.left,
                                 & sVMTRTuple )
              != IDE_SUCCESS );

    // PROJ-2362 memory temp ���� ȿ���� ����
    // VMTR�� columns�� �����Ѵ�.
    IDE_DASSERT( aDataPlan->plan.myTuple->columnCount == sVMTRTuple->columnCount );
    
    idlOS::memcpy( (void*)aDataPlan->plan.myTuple->columns,
                   (void*)sVMTRTuple->columns,
                   ID_SIZEOF(mtcColumn) * sVMTRTuple->columnCount );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
#undef IDE_FN
}

IDE_RC
qmnVSCN::execChild( qcTemplate * aTemplate,
                    qmncVSCN   * aCodePlan )
{
/***********************************************************************
 *
 * Description :
 *    Child Plan�� VMTR�� �ʱ�ȭ�Ѵ�.
 *    �� ��, VMTR�� ��� Data�� �����ϰ� �ȴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::execChild"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    //---------------------------------
    // Child VMTR�� ����
    //---------------------------------

    // VMTR�� �ʱ�ȭ ��ü�� ������ �ȴ�.
    IDE_TEST( aCodePlan->plan.left->init( aTemplate,
                                          aCodePlan->plan.left )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::getNullRow( qcTemplate * aTemplate,
                     qmncVSCN   * aCodePlan,
                     qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     VMTR�κ��� Null Row�� ȹ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    iduMemory * sMemory;

    // ���ռ� �˻�
    IDE_DASSERT( aDataPlan->nullRowSize > 0 );

    sMemory = aTemplate->stmt->qmxMem;

    if ( ( aCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK )
         == QMN_PLAN_STORAGE_DISK )
    {
        // Null Row�� ���� ���� �Ҵ�
        IDU_FIT_POINT( "qmnVSCN::getNullRow::cralloc::nullRow",
                        idERR_ABORT_InsufficientMemory );

        IDE_TEST( sMemory->cralloc( aDataPlan->nullRowSize,
                                    & aDataPlan->nullRow )
                  != IDE_SUCCESS);

        // Null Row ȹ��
        IDE_TEST( qmnVMTR::getNullRowDisk( aTemplate,
                                           aCodePlan->plan.left,
                                           aDataPlan->nullRow,
                                           & aDataPlan->nullRID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Null Row ȹ��
        IDE_TEST( qmnVMTR::getNullRowMemory( aTemplate,
                                             aCodePlan->plan.left,
                                             &aDataPlan->nullRow )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmnVSCN::openCursor( qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     Disk Temp Table ��� �� ȣ��Ǹ�,
 *     �ش� ������ �̿��Ͽ� Cursor�� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::openCursor"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    if( aDataPlan->tempCursor == NULL )
    {
        //-----------------------------------------
        // 1. Cursor�� ���� ���� ���
        //-----------------------------------------
        IDE_TEST( smiSortTempTable::openCursor( 
                aDataPlan->tableHandle,
                SMI_TCFLAG_FORWARD | 
                SMI_TCFLAG_ORDEREDSCAN |
                SMI_TCFLAG_IGNOREHIT,
                NULL,                         // Update Column����
                smiGetDefaultKeyRange(),      // Key Range
                smiGetDefaultKeyRange(),      // Key Filter
                smiGetDefaultFilter(),        // Filter
                &aDataPlan->tempCursor )
            != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------------
        // 2. Cursor�� ���� �ִ� ���
        //-----------------------------------------
        IDE_TEST( smiSortTempTable::restartCursor( 
                aDataPlan->tempCursor,
                SMI_TCFLAG_FORWARD | 
                SMI_TCFLAG_ORDEREDSCAN |
                SMI_TCFLAG_IGNOREHIT,
                smiGetDefaultKeyRange(),      // Key Range
                smiGetDefaultKeyRange(),      // Key Filter
                smiGetDefaultFilter() )         // Filter
            != IDE_SUCCESS );

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmnVSCN::setTupleSet( qcTemplate * aTemplate,
                      qmncVSCN   * aCodePlan,
                      qmndVSCN   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *     VSCN tuple ����
 *
 * Implementation :
 *     VMTR tuple�� �����ѵ� VSCN�� ����(copy)�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qmnVSCN::setTupleSet"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qmdMtrNode * sNode;
    mtcTuple   * sVMTRTuple;
    mtcColumn  * sMyColumn;
    mtcColumn  * sVMTRColumn;
    UInt         i;
    
    // PROJ-2362 memory temp ���� ȿ���� ����
    // VSCN tuple ����
    if ( aDataPlan->memSortRecord != NULL )
    {
        for ( sNode = aDataPlan->memSortRecord;
              sNode != NULL;
              sNode = sNode->next )
        {
            IDE_TEST( sNode->func.setTuple( aTemplate,
                                            sNode,
                                            aDataPlan->plan.myTuple->row )
                      != IDE_SUCCESS );
        }

        IDE_TEST( qmnVMTR::getTuple( aTemplate,
                                     aCodePlan->plan.left,
                                     & sVMTRTuple )
                  != IDE_SUCCESS );
        
        sMyColumn = aDataPlan->plan.myTuple->columns;
        sVMTRColumn = sVMTRTuple->columns;
        for ( i = 0; i < sVMTRTuple->columnCount; i++, sMyColumn++, sVMTRColumn++ )
        {
            if ( SMI_COLUMN_TYPE_IS_TEMP( sMyColumn->column.flag ) == ID_TRUE )
            {
                IDE_DASSERT( sVMTRColumn->module->actualSize(
                                 sVMTRColumn,
                                 sVMTRColumn->column.value )
                             <= sVMTRColumn->column.size );
                
                idlOS::memcpy( (SChar*)sMyColumn->column.value,
                               (SChar*)sVMTRColumn->column.value,
                               sVMTRColumn->module->actualSize(
                                   sVMTRColumn,
                                   sVMTRColumn->column.value ) );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qmnVSCN::touchDependency( qcTemplate * aTemplate,
                                 qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    dependency�� �����Ų��.
 *
 * Implementation :
 *
 ***********************************************************************/

    // qmncVSCN * sCodePlan = (qmncVSCN*) aPlan;
    qmndVSCN * sDataPlan =
        (qmndVSCN*) (aTemplate->tmplate.data + aPlan->offset);
    
    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;
}
