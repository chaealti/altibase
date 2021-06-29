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
 *     SDEX(Shard DML EXecutor) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <cm.h>
#include <idl.h>
#include <ide.h>
#include <mtv.h>
#include <qcg.h>
#include <qmnShardDML.h>
#include <qmx.h>
#include <qmxShard.h>

IDE_RC qmnSDEX::init( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *    SDEX ����� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiClientInfo * sClientInfo = NULL;
    qmncSDEX      * sCodePlan = (qmncSDEX*)aPlan;
    qmndSDEX      * sDataPlan = (qmndSDEX*)(aTemplate->tmplate.data + aPlan->offset);

    //-------------------------------
    // ���ռ� �˻�
    //-------------------------------

    //-------------------------------
    // �⺻ �ʱ�ȭ
    //-------------------------------

    sDataPlan->flag = &aTemplate->planFlag[sCodePlan->planID];

    // First initialization
    if ( (*sDataPlan->flag & QMND_SDEX_INIT_DONE_MASK)
         == QMND_SDEX_INIT_DONE_FALSE)
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        //-----------------------------------
        // init lob info
        //-----------------------------------

        if ( sDataPlan->lobInfo != NULL )
        {
            (void) qmxShard::initLobInfo( sDataPlan->lobInfo );
        }
        else
        {
            // Nothing to do.
        }
    }

    //-------------------------------
    // ������� ���� �ʱ�ȭ
    //-------------------------------

    sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    sdi::setDataNodePrepared( sClientInfo, sDataPlan->mDataInfo );

    //------------------------------------------------
    // ���� �Լ� ����
    //------------------------------------------------

    sDataPlan->doIt = qmnSDEX::doIt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDEX::firstInit( qcTemplate * aTemplate,
                           qmncSDEX   * aCodePlan,
                           qmndSDEX   * aDataPlan )
{
/***********************************************************************
 *
 * Description :
 *    Data ������ ���� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiClientInfo * sClientInfo = NULL;
    sdiBindParam  * sBindParams = NULL;
    sdiDataNode     sDataNodeArg;
    sdiSVPStep      sSVPStep = SDI_SVP_STEP_DO_NOT_NEED_SAVEPOINT; /* BUG-47459 */

    UInt            sLobBindCount = 0;

    //-------------------------------
    // ������ �ʱ�ȭ
    //-------------------------------

    IDE_TEST_RAISE( aTemplate->shardExecData.execInfo == NULL,
                    ERR_NO_SHARD_INFO );

    aDataPlan->mDataInfo = ((sdiDataNodes*)aTemplate->shardExecData.execInfo)
        + aCodePlan->shardDataIndex;

    // shard linker �˻� & �ʱ�ȭ
    IDE_TEST( sdi::checkShardLinker( aTemplate->stmt ) != IDE_SUCCESS );

    //-------------------------------
    // shard ������ ���� �غ�
    //-------------------------------

    /* BUG-47459 */
    if ( QCG_GET_SESSION_IS_AUTOCOMMIT( aTemplate->stmt ) == ID_TRUE )
    {
        sSVPStep = SDI_SVP_STEP_DO_NOT_NEED_SAVEPOINT;
    }
    else
    {
        if ( ( aCodePlan->shardAnalysis->mSplitMethod == SDI_SPLIT_CLONE ) &&
             ( aTemplate->stmt->myPlan->parseTree->stmtKind == QCI_STMT_EXEC_PROC ) )
        {
            sSVPStep = SDI_SVP_STEP_PROCEDURE_SAVEPOINT;
        }            
    }

    sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    if ( aDataPlan->mDataInfo->mInitialized == ID_FALSE )
    {
        idlOS::memset( &sDataNodeArg, 0x00, ID_SIZEOF(sdiDataNode) );

        sDataNodeArg.mBindParamCount = aCodePlan->shardParamCount;
        sDataNodeArg.mBindParams = (sdiBindParam*)
            ( aTemplate->shardExecData.data + aCodePlan->bindParam );

        /* PROJ-2728 Sharding LOB */
        sDataNodeArg.mOutBindParams = (sdiOutBindParam*)
            ( aTemplate->shardExecData.data + aCodePlan->outBindParam );
        idlOS::memset( sDataNodeArg.mOutBindParams, 0x00,
                ID_SIZEOF(sdiOutBindParam) * aCodePlan->shardParamCount );

        // �ʱ�ȭ
        IDE_TEST( setParamInfo( aTemplate,
                                aCodePlan,
                                sDataNodeArg.mBindParams,
                                sDataNodeArg.mOutBindParams,
                                &sLobBindCount )
                  != IDE_SUCCESS );

        sDataNodeArg.mRemoteStmt = NULL;

        sDataNodeArg.mSVPStep = sSVPStep;

        IDE_TEST( sdi::initShardDataInfo( aTemplate,
                                          aCodePlan->shardAnalysis,
                                          sClientInfo,
                                          aDataPlan->mDataInfo,
                                          & sDataNodeArg )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aCodePlan->shardParamCount > 0 )
        {
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          ID_SIZEOF(sdiBindParam) * aCodePlan->shardParamCount,
                          (void**) & sBindParams )
                      != IDE_SUCCESS );

            IDE_TEST( setParamInfo( aTemplate,
                                    aCodePlan,
                                    sBindParams,
                                    aDataPlan->mDataInfo->mNodes->mOutBindParams,
                                    &sLobBindCount )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( sdi::reuseShardDataInfo( aTemplate,
                                           sClientInfo,
                                           aDataPlan->mDataInfo,
                                           sBindParams,
                                           aCodePlan->shardParamCount,
                                           SDI_SVP_STEP_DO_NOT_NEED_SAVEPOINT )
                  != IDE_SUCCESS );
    }

    /*
     * PROJ-2728 Sharding LOB
     */
    if ( sLobBindCount > 0 )
    {
        // PROJ-1362
        IDE_TEST( qmxShard::initializeLobInfo(
                    aTemplate->stmt,
                    &(aDataPlan->lobInfo),
                    sLobBindCount )
                  != IDE_SUCCESS );
    }
    else
    {
        aDataPlan->lobInfo = NULL;
    }
    aDataPlan->lobBindCount = sLobBindCount;

    *aDataPlan->flag &= ~QMND_SDEX_INIT_DONE_MASK;
    *aDataPlan->flag |= QMND_SDEX_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SHARD_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSDEX::firstInit",
                                  "Shard Info is not found" ) );
    }
    IDE_EXCEPTION_END;

    if ( aDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, aDataPlan->lobInfo );
    }

    return IDE_FAILURE;
}

IDE_RC qmnSDEX::setParamInfo( qcTemplate      * aTemplate,
                              qmncSDEX        * aCodePlan,
                              sdiBindParam    * aBindParams,
                              sdiOutBindParam * aOutBindParams,
                              UInt            * aLobBindCount )
{
    qciBindParamInfo * sAllParamInfo = NULL;
    qciBindParam     * sBindParam    = NULL;
    UShort             sBindOffset   = 0; /* TASK-7219 */
    UShort             i             = 0; /* TASK-7219 */

    UInt               sClientCount  = 0;
    UInt               sLobBindCount = 0;
    idBool             sNeedShadowData = ID_FALSE;
    mtdLobType       * sLobValue;

    sClientCount = aTemplate->stmt->session->mQPSpecific.mClientInfo->mCount;

    // PROJ-2653
    sAllParamInfo  = aTemplate->stmt->pBindParam;

    for ( i = 0; i < aCodePlan->shardParamCount; i++ )
    {
        sBindOffset = ( aCodePlan->shardParamInfo + i )->mOffset; /* TASK-7219 Non-shard DML */

        IDE_DASSERT( sBindOffset < aTemplate->stmt->pBindParamCount );

        sBindParam = &(sAllParamInfo[sBindOffset].param);

        if ( ( sBindParam->inoutType == CMP_DB_PARAM_INPUT ) ||
             ( sBindParam->inoutType == CMP_DB_PARAM_INPUT_OUTPUT ) )
        {
            IDE_DASSERT( sAllParamInfo[sBindOffset].isParamInfoBound == ID_TRUE );
            IDE_DASSERT( sAllParamInfo[sBindOffset].isParamDataBound == ID_TRUE );
        }
        else
        {
            // Nothing to do.
        }

        sNeedShadowData = ID_FALSE;

        aBindParams[i].mId        = i + 1;
        aBindParams[i].mInoutType = sBindParam->inoutType;
        aBindParams[i].mType      = sBindParam->type;
        aBindParams[i].mData      = sBindParam->data;
        aBindParams[i].mDataSize  = sBindParam->dataSize;
        aBindParams[i].mPrecision = sBindParam->precision;
        aBindParams[i].mScale     = sBindParam->scale;

        /* BUG-46623 padding ������ 0���� �ʱ�ȭ �ؾ� �Ѵ�. */
        aBindParams[i].padding    = 0;

        /* PROJ-2728 Sharding LOB */
        if ( ( sBindParam->type == MTD_BLOB_LOCATOR_ID ) ||
             ( sBindParam->type == MTD_CLOB_LOCATOR_ID ) )
        {
            sLobBindCount++;

            /* app.���� DML ���� �����, ����� �����Ѵ�. */
            if ( sBindParam->inoutType == CMP_DB_PARAM_INPUT )
            {
                /* app.���� locator�� IN binding�� ���,
                 * stand-alone������ ���� locator���� copy������,
                 * shard������ ��� node�κ��� locator�� �޾ƿ;� �ϹǷ�
                 * OUTPUT ���� ���ε��ؾ� �Ѵ�.
                 */
                aBindParams[i].mInoutType = CMP_DB_PARAM_OUTPUT;
            }
            else
            {
                // Nothing to do.
            }

            sNeedShadowData = ID_TRUE;
        }
        else if ( ( sBindParam->type == MTD_BLOB_ID ) ||
                  ( sBindParam->type == MTD_CLOB_ID ) )
        {
            sLobBindCount++;

            // PSM ���ο��� LOB Ÿ�� ������ ����� DML �����, 
            if ( sBindParam->inoutType == CMP_DB_PARAM_INPUT )
            {
                /* locator ���ε��� �Ұ����ϹǷ� indicator�� �Բ�
                 * SQL_C_CHAR �Ǵ� SQL_C_BINARY ���ε��Ѵ�.
                 * sdl::bindParam ����.
                 */
                if ( sBindParam->type == MTD_CLOB_ID )
                {
                    aBindParams[i].mPrecision = IDL_MIN(
                        aBindParams[i].mPrecision, MTD_CHAR_PRECISION_MAXIMUM);
                }
                else
                {
                    /* Nothing to do. */
                }

                sLobValue = (mtdLobType *) sBindParam->data;
                aBindParams[i].mData = &sLobValue->value;
            }
            else // CMP_DB_PARAM_OUTPUT
            {
                /* LOB OUT binding�� �� �� �ִ� ���
                 * 1. RETURN INTO ���� LOB Ÿ�� ������ ���� ���, 
                 *    �� �� ��� �÷��� Ÿ���� LOB�� �ƴϾ�� �Ѵ�.
                 *    * return into �� LOB Ÿ�� �÷��� ������.
                 * 2. PSM ������ OUT ���ڰ� �ִ� shard procedure �����,
                 *    locator ���ε��� �������� �����Ƿ�
                 *    BINARY|CHAR�� ���ε��ؼ� mtdBinaryType|mtdCharType����
                 *    ����� ���� �� mtdLobType���� ��ȯ�Ѵ�.
                 */
                if ( sBindParam->type == MTD_BLOB_ID )
                {
                    aBindParams[i].mType = MTD_BINARY_ID;
                }
                else
                {
                    aBindParams[i].mType = MTD_VARCHAR_ID;
                }
                sNeedShadowData = ID_TRUE;
            }
        }
        else
        {
            // Nothing to do.
        }

        if ( sNeedShadowData == ID_TRUE )
        {
            if ( aOutBindParams[i].mShadowData == NULL )
            {
                IDE_TEST( QC_QME_MEM( aTemplate->stmt)->alloc(
                               aBindParams[i].mDataSize * sClientCount,
                               (void**) &(aOutBindParams[i].mShadowData) )
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
            aOutBindParams[i].mShadowData = NULL;
        }
    }

    *aLobBindCount = sLobBindCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDEX::doIt( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * /*aFlag*/ )
{
    qmncSDEX       * sCodePlan = (qmncSDEX*)aPlan;
    qmndSDEX       * sDataPlan = (qmndSDEX*)(aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo  * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;
    vSLong           sNumRows = 0;

    if ( sDataPlan->lobBindCount > 0 &&
         sCodePlan->shardParamCount > 0 )
    {
        IDE_TEST( setLobInfo( aTemplate,
                              sCodePlan,
                              sDataPlan->lobInfo )
                  != IDE_SUCCESS );
    }
    //-------------------------------
    // ������ ����
    //-------------------------------

    IDE_TEST( sdi::decideShardDataInfo(
                  aTemplate,
                  &(aTemplate->tmplate.rows[aTemplate->tmplate.variableRow]),
                  sCodePlan->shardAnalysis,
                  sClientInfo,
                  sDataPlan->mDataInfo,
                  &(sCodePlan->shardQuery) )
              != IDE_SUCCESS );

    /* PROJ-2733-DistTxInfo �л������� ��������. */ 
    sdi::calculateGCTxInfo( aTemplate,
                            sDataPlan->mDataInfo,
                            aTemplate->shardExecData.globalPSM,
                            sCodePlan->shardDataIndex );

    //-------------------------------
    // ����
    //-------------------------------

    IDE_TEST( sdi::executeDML( aTemplate->stmt,
                               sClientInfo,
                               sDataPlan->mDataInfo,
                               sDataPlan->lobInfo,
                               & sNumRows )
              != IDE_SUCCESS );

    // result row count
    aTemplate->numRows += sNumRows;

    //-----------------------------------
    // clear lob info
    //-----------------------------------
    if ( sDataPlan->lobInfo != NULL )
    {
        (void) qmxShard::initLobInfo( sDataPlan->lobInfo );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sDataPlan->lobInfo != NULL )
    {
        (void)qmx::finalizeLobInfo( aTemplate->stmt, sDataPlan->lobInfo );
    }

    return IDE_FAILURE;
}

IDE_RC qmnSDEX::padNull( qcTemplate * aTemplate,
                         qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *     Child�� ���Ͽ� padNull()�� ȣ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSDEX * sCodePlan = (qmncSDEX*)aPlan;
    //qmndSDEX * sDataPlan =
    //    (qmndSDEX*) (aTemplate->tmplate.data + aPlan->offset);

    if ( (aTemplate->planFlag[sCodePlan->planID] & QMND_SDEX_INIT_DONE_MASK)
         == QMND_SDEX_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Child Plan�� ���Ͽ� Null Padding����
    if ( aPlan->left != NULL )
    {
        IDE_TEST( aPlan->left->padNull( aTemplate, aPlan->left )
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

IDE_RC qmnSDEX::printPlan( qcTemplate   * aTemplate,
                           qmnPlan      * aPlan,
                           ULong          aDepth,
                           iduVarString * aString,
                           qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    SDEX����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    //qmncSDEX * sCodePlan = (qmncSDEX*) aPlan;
    qmndSDEX * sDataPlan = (qmndSDEX*) (aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    //----------------------------
    // SDEX ��� ǥ��
    //----------------------------

    qmn::printSpaceDepth( aString, aDepth );
    iduVarStringAppend( aString, "SHARD-DML-EXECUTOR\n" );

    //----------------------------
    // ���� ������ �� ���
    //----------------------------

    if ( ( ( QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE(aTemplate->stmt) == 1 ) ||
           ( SDU_SHARD_REBUILD_PLAN_DETAIL_FORCE_ENABLE == 1 ) ) &&
         ( sClientInfo != NULL ) )
    {
        //---------------------------------------------
        // shard execution
        //---------------------------------------------

        IDE_TEST( printDataInfo( aTemplate,
                                 sClientInfo,
                                 sDataPlan->mDataInfo,
                                 aDepth + 1,
                                 aString,
                                 aMode,
                                 sDataPlan->flag )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDEX::printDataInfo( qcTemplate    * /* aTemplate */,
                               sdiClientInfo * aClientInfo,
                               sdiDataNodes  * aDataInfo,
                               ULong           aDepth,
                               iduVarString  * aString,
                               qmnDisplay      aMode,
                               UInt          * aInitFlag )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiConnectInfo * sConnectInfo = NULL;
    sdiDataNode    * sDataNode = NULL;
    SChar          * sPreIndex;
    SChar          * sIndex;
    UInt             i;

    qmn::printSpaceDepth( aString, aDepth );
    iduVarStringAppend( aString, "[ SHARD EXECUTION ]\n" );

    sConnectInfo = aClientInfo->mConnectInfo;
    sDataNode = aDataInfo->mNodes;

    for ( i = 0; i < aClientInfo->mCount; i++, sConnectInfo++, sDataNode++ )
    {
        if ( aMode == QMN_DISPLAY_ALL )
        {
            //----------------------------
            // explain plan = on; �� ���
            //----------------------------

            if ( ( *aInitFlag & QMND_SDEX_INIT_DONE_MASK )
                 == QMND_SDEX_INIT_DONE_TRUE )
            {
                if ( sDataNode->mState >= SDI_NODE_STATE_EXECUTED )
                {
                    qmn::printSpaceDepth( aString, aDepth );

                    if ( sDataNode->mExecCount == 1 )
                    {
                        iduVarStringAppendFormat( aString, "%s (executed)\n",
                                                  sConnectInfo->mNodeName );
                    }
                    else
                    {
                        iduVarStringAppendFormat( aString, "%s (%"ID_UINT32_FMT" executed)\n",
                                                  sConnectInfo->mNodeName,
                                                  sDataNode->mExecCount );
                    }

                    if ( sdi::getPlan( sConnectInfo, sDataNode ) == IDE_SUCCESS )
                    {
                        for ( sIndex = sPreIndex = sDataNode->mPlanText; *sIndex != '\0'; sIndex++ )
                        {
                            if ( *sIndex == '\n' )
                            {
                                qmn::printSpaceDepth( aString, aDepth + 1 );
                                iduVarStringAppend( aString, "::" );
                                iduVarStringAppendLength( aString,
                                                          sPreIndex,
                                                          sIndex - sPreIndex + 1 );
                                sPreIndex = sIndex + 1;
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
                else if ( sDataNode->mState == SDI_NODE_STATE_PREPARED )
                {
                    qmn::printSpaceDepth( aString, aDepth );

                    if ( sDataNode->mExecCount == 1 )
                    {
                        iduVarStringAppendFormat( aString, "%s (executed)\n",
                                                  sConnectInfo->mNodeName );
                    }
                    else if ( sDataNode->mExecCount > 1 )
                    {
                        iduVarStringAppendFormat( aString, "%s (%"ID_UINT32_FMT" executed)\n",
                                                  sConnectInfo->mNodeName,
                                                  sDataNode->mExecCount );
                    }
                    else
                    {
                        iduVarStringAppendFormat( aString, "%s (prepared)\n",
                                                  sConnectInfo->mNodeName );
                    }

                    if ( sDataNode->mExecCount >= 1 )
                    {
                        if ( sdi::getPlan( sConnectInfo, sDataNode ) == IDE_SUCCESS )
                        {
                            for ( sIndex = sPreIndex = sDataNode->mPlanText; *sIndex != '\0'; sIndex++ )
                            {
                                if ( *sIndex == '\n' )
                                {
                                    qmn::printSpaceDepth( aString, aDepth + 1 );
                                    iduVarStringAppend( aString, "::" );
                                    iduVarStringAppendLength( aString,
                                                              sPreIndex,
                                                              sIndex - sPreIndex + 1 );
                                    sPreIndex = sIndex + 1;
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
                else
                {
                    // SDI_NODE_STATE_PREPARE_CANDIDATED

                    // Nothing to do.
                }
            }
            else
            {
                qmn::printSpaceDepth( aString, aDepth );
                iduVarStringAppendFormat( aString, "%s\n",
                                          sConnectInfo->mNodeName );
            }
        }
        else
        {
            //----------------------------
            // explain plan = only; �� ���
            //----------------------------

            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppendFormat( aString, "%s\n",
                                      sConnectInfo->mNodeName );
        }
    }

    return IDE_SUCCESS;
}
/* BUG-47459 */
void qmnSDEX::shardStmtPartialRollbackUsingSavepoint( qcTemplate  * aTemplate,
                                                      qmnPlan     * aPlan )
{
    qmndSDEX        * sDataPlan = (qmndSDEX*)(aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo   * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    if ( ( sDataPlan->mDataInfo != NULL ) &&
         ( sDataPlan->mDataInfo->mInitialized == ID_TRUE ) )
    {
        sdi::shardStmtPartialRollbackUsingSavepoint( aTemplate->stmt,
                                                     sClientInfo, 
                                                     sDataPlan->mDataInfo );
    }
}

/* PROJ-2728 Sharding LOB */
IDE_RC qmnSDEX::setLobInfo( qcTemplate   * aTemplate,
                            qmncSDEX     * aCodePlan,
                            qmxLobInfo   * aLobInfo )
{
    sdiOutBindParam   * sOutBindParams = NULL;
    qciBindParamInfo  * sAllParamInfo  = NULL;
    qciBindParam      * sBindParam     = NULL;
    mtdLobType        * sLobValue      = NULL;
    UShort              sBindOffset    = 0; /* TASK-7219 */
    UShort              i              = 0; /* TASK-7219 */

    sAllParamInfo    = aTemplate->stmt->pBindParam;
    sOutBindParams = (sdiOutBindParam*) ( aTemplate->shardExecData.data + aCodePlan->outBindParam );

    for ( i = 0; i < aCodePlan->shardParamCount; i++ )
    {
        sBindOffset = ( aCodePlan->shardParamInfo + i )->mOffset; /* TASK-7219 Non-shard DML */
        sBindParam  = &(sAllParamInfo[sBindOffset].param);

        if ( ( sBindParam->type == MTD_BLOB_LOCATOR_ID ) ||
             ( sBindParam->type == MTD_CLOB_LOCATOR_ID ) )
        {
            if ( sBindParam->inoutType == CMP_DB_PARAM_INPUT )
            {
                (void) qmxShard::addLobInfoForCopy(
                              aLobInfo,
                              *((smLobLocator*) (sBindParam->data)),
                              i );
            }
            else // PARAM_OUTPUT
            {
                (void) qmxShard::addLobInfoForOutBind(
                              aLobInfo,
                              i );
            }
        }
        else if ( ( sBindParam->type == MTD_BLOB_ID ) ||
                  ( sBindParam->type == MTD_CLOB_ID ) )
        {
            if ( sBindParam->inoutType == CMP_DB_PARAM_INPUT )
            {
                sLobValue = (mtdLobType *) sBindParam->data;

                IDE_TEST_RAISE( sBindParam->type == MTD_CLOB_ID &&
                                sLobValue->length > MTD_CHAR_PRECISION_MAXIMUM,
                                ERR_CONVERSION_NOT_APPLICABLE );

                sOutBindParams[i].mIndicator = sLobValue->length;
            }
            else
            {
                (void) qmxShard::addLobInfoForOutBindNonLob(
                              aLobInfo,
                              i );
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CONVERSION_NOT_APPLICABLE );
    {
        IDE_SET(ideSetErrorCode(mtERR_ABORT_CONVERSION_NOT_APPLICABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
