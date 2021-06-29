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
 * Description : SDSE(SharD SElect) Node
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <cm.h>
#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qmoUtil.h>
#include <qmnShardSelect.h>
#include <smi.h>
#include <qmxShard.h>

extern mtdModule mtdNull; /* TASK-7219 */

IDE_RC qmnSDSE::init( qcTemplate * aTemplate,
                      qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description : SDSE ����� �ʱ�ȭ
 *
 * Implementation : ���� �ʱ�ȭ�� ���� ���� ��� ���� �ʱ�ȭ ����
 *
 ***********************************************************************/

    sdiClientInfo * sClientInfo = NULL;
    qmncSDSE      * sCodePlan = NULL;
    qmndSDSE      * sDataPlan = NULL;
    idBool          sJudge = ID_TRUE;

    //-------------------------------
    // ���ռ� �˻�
    //-------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aPlan     != NULL );

    //-------------------------------
    // �⺻ �ʱ�ȭ
    //-------------------------------

    sCodePlan = (qmncSDSE*)aPlan;
    sDataPlan = (qmndSDSE*)(aTemplate->tmplate.data + aPlan->offset);
    sDataPlan->flag = &aTemplate->planFlag[sCodePlan->planID];

    // First initialization
    if ( ( *sDataPlan->flag & QMND_SDSE_INIT_DONE_MASK ) == QMND_SDSE_INIT_DONE_FALSE )
    {
        IDE_TEST( firstInit(aTemplate, sCodePlan, sDataPlan) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //-------------------------------
    // ������� ���� �ʱ�ȭ
    //-------------------------------

    sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    sdi::setDataNodePrepared( sClientInfo, sDataPlan->mDataInfo );

    //-------------------------------
    // doIt�Լ� ������ ���� Constant filter �� judgement
    //-------------------------------
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

    if ( sJudge == ID_TRUE )
    {
        //------------------------------------------------
        // ���� �Լ� ����
        //------------------------------------------------
        sDataPlan->doIt = qmnSDSE::doItFirst;
        *sDataPlan->flag &= ~QMND_SDSE_ALL_FALSE_MASK;
        *sDataPlan->flag |=  QMND_SDSE_ALL_FALSE_FALSE;
    }
    else
    {
        sDataPlan->doIt = qmnSDSE::doItAllFalse;
        *sDataPlan->flag &= ~QMND_SDSE_ALL_FALSE_MASK;
        *sDataPlan->flag |= QMND_SDSE_ALL_FALSE_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDSE::firstInit( qcTemplate * aTemplate,
                           qmncSDSE   * aCodePlan,
                           qmndSDSE   * aDataPlan )
{
/***********************************************************************
 *
 * Description : Data ������ ���� �Ҵ�
 *
 * Implementation :
 *
 ***********************************************************************/

    sdiClientInfo    * sClientInfo = NULL;
    sdiDataNode        sDataNodeArg;
    sdiBindParam     * sBindParams = NULL;
    UShort             sTupleID;
    UInt               i;
    UInt               sLobBindCount = 0;

    /* TASK-7219 */
    mtcColumn * sColumn = NULL;
    UInt        sCount  = 0;

    // Tuple ��ġ�� ����
    sTupleID = aCodePlan->tupleRowID;
    aDataPlan->plan.myTuple = &aTemplate->tmplate.rows[sTupleID];

    // BUGBUG
    aDataPlan->plan.myTuple->lflag &= ~MTC_TUPLE_STORAGE_MASK;
    aDataPlan->plan.myTuple->lflag |=  MTC_TUPLE_STORAGE_DISK;

    aDataPlan->nullRow        = NULL;
    aDataPlan->mCurrScanNode  = 0;
    aDataPlan->mScanDoneCount = 0;

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

    sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    if ( aDataPlan->mDataInfo->mInitialized == ID_FALSE )
    {
        idlOS::memset( &sDataNodeArg, 0x00, ID_SIZEOF(sdiDataNode) );

        // data�� ������ ����(tuple�� ����) buffer ���� �Ҵ�
        sDataNodeArg.mBufferLength = aDataPlan->plan.myTuple->rowOffset;
        for ( i = 0; i < SDI_NODE_MAX_COUNT; i++ )
        {
            sDataNodeArg.mBuffer[i] = (void*)( aTemplate->shardExecData.data + aCodePlan->mBuffer[i] );
            // �ʱ�ȭ
            idlOS::memset( sDataNodeArg.mBuffer[i], 0x00, sDataNodeArg.mBufferLength );
        }
        sDataNodeArg.mOffset = (UInt*)( aTemplate->shardExecData.data + aCodePlan->mOffset );
        sDataNodeArg.mMaxByteSize =
            (UInt*)( aTemplate->shardExecData.data + aCodePlan->mMaxByteSize );

        sDataNodeArg.mBindParamCount = aCodePlan->mShardParamCount;
        sDataNodeArg.mBindParams = (sdiBindParam*)
            ( aTemplate->shardExecData.data + aCodePlan->mBindParam );

        /* PROJ-2728 Sharding LOB */
        sDataNodeArg.mOutBindParams = (sdiOutBindParam*)
            ( aTemplate->shardExecData.data + aCodePlan->mOutBindParam );
        idlOS::memset( sDataNodeArg.mOutBindParams, 0x00,
                ID_SIZEOF(sdiOutBindParam) * aCodePlan->mShardParamCount );

        /* TASK-7219 Non-shard DML */
        sDataNodeArg.mOutRefBindData = ( void* )
            ( aTemplate->shardExecData.data + aCodePlan->mOutRefBindData );

        /* TASK-7219 */
        for ( i = 0, sColumn = aDataPlan->plan.myTuple->columns;
              i < aDataPlan->plan.myTuple->columnCount;
              i++, sColumn++ )
        {
            if ( ( sColumn->flag & MTC_COLUMN_NULL_TYPE_MASK ) == MTC_COLUMN_NULL_TYPE_TRUE )
            {
                /* Nothing to do */
            }
            else
            {
                sDataNodeArg.mOffset[ sCount ]      = sColumn->column.offset;
                sDataNodeArg.mMaxByteSize[ sCount ] = sColumn->column.size;

                sCount++;
            }
        }

        if ( sCount == 0 )
        {
            sDataNodeArg.mOffset[ 0 ]      = 0;
            sDataNodeArg.mMaxByteSize[ 0 ] = mtdNull.actualSize( NULL, NULL );

            sCount = 1;
        }
        else
        {
            /* Nothing to do */
        }

        sDataNodeArg.mColumnCount = sCount;

        IDE_TEST( setParamInfo( aTemplate,
                                aCodePlan,
                                sDataNodeArg.mBindParams,
                                sDataNodeArg.mOutRefBindData,
                                &sLobBindCount )
                  != IDE_SUCCESS );

        sDataNodeArg.mRemoteStmt = NULL;

        sDataNodeArg.mSVPStep = SDI_SVP_STEP_DO_NOT_NEED_SAVEPOINT;

        IDE_TEST( sdi::initShardDataInfo( aTemplate,
                                          aCodePlan->mShardAnalysis,
                                          sClientInfo,
                                          aDataPlan->mDataInfo,
                                          & sDataNodeArg )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aCodePlan->mShardParamCount > 0 )
        {
            IDE_TEST( aTemplate->stmt->qmxMem->alloc(
                          ID_SIZEOF(sdiBindParam) * aCodePlan->mShardParamCount,
                          (void**) & sBindParams )
                      != IDE_SUCCESS );

            IDE_TEST( setParamInfo( aTemplate,
                                    aCodePlan,
                                    sBindParams,
                                    aTemplate->shardExecData.data + aCodePlan->mOutRefBindData,
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
                                           aCodePlan->mShardParamCount,
                                           SDI_SVP_STEP_DO_NOT_NEED_SAVEPOINT )
                  != IDE_SUCCESS );
    }

    aDataPlan->lobBindCount = sLobBindCount;

    *aDataPlan->flag &= ~QMND_SDSE_INIT_DONE_MASK;
    *aDataPlan->flag |=  QMND_SDSE_INIT_DONE_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SHARD_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSDSE::firstInit",
                                  "Shard Info is not found" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDSE::setParamInfo( qcTemplate   * aTemplate,
                              qmncSDSE     * aCodePlan,
                              sdiBindParam * aBindParams,
                              void         * aOutRefBindData,
                              UInt         * aLobBindCount )
{
    qciBindParamInfo * sAllParamInfo = NULL;
    qciBindParam     * sBindParam = NULL;

    /* TASK-7219 Non-shard DML */
    qcShardParamInfo * sBindParamInfo = NULL;

    UShort             i = 0;
    UInt               sLobBindCount = 0;
    mtdLobType       * sLobValue;

    /* TASK-7219 Non-shard DML */
    mtcTuple  * sTuple  = NULL;
    mtcColumn * sColumn = NULL;

    UInt        sOutRefBindDataOffset = 0;

    // PROJ-2653
    sAllParamInfo  = aTemplate->stmt->pBindParam;

    for ( i = 0; i < aCodePlan->mShardParamCount; i++ )
    {
        sBindParamInfo = aCodePlan->mShardParamInfo + i; /* TASK-7219 Non-shard DML */

        if ( sBindParamInfo->mIsOutRefColumnBind == ID_TRUE )
        {
            /* TASK-7219 Non-shard DML */
            sTuple =  & aTemplate->tmplate.rows[ sBindParamInfo->mOutRefTuple ];
            sColumn = sTuple->columns + sBindParamInfo->mOffset;

            aBindParams[i].mId        = i + 1;
            aBindParams[i].mInoutType = CMP_DB_PARAM_INPUT;
            aBindParams[i].mType      = sColumn->module->id;

            aBindParams[i].mData      =
                ( (UChar*)aOutRefBindData ) + sOutRefBindDataOffset;

            sOutRefBindDataOffset += sColumn->column.size;

            aBindParams[i].mDataSize  = sColumn->column.size;
            aBindParams[i].mPrecision = sColumn->precision;
            aBindParams[i].mScale     = sColumn->scale;

            /* BUG-46623 padding ������ 0���� �ʱ�ȭ �ؾ� �Ѵ�. */
            aBindParams[i].padding    = 0;

            if ( ( sColumn->module->id == MTD_BLOB_ID ) ||
                 ( sColumn->module->id == MTD_CLOB_ID ) ||
                 ( sColumn->module->id == MTD_BLOB_LOCATOR_ID ) ||
                 ( sColumn->module->id == MTD_CLOB_LOCATOR_ID ) )
            {
                IDE_RAISE( ERR_LOB_COLUMN_PUSHED_FORCE );
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            IDE_DASSERT( sBindParamInfo->mOffset < aTemplate->stmt->pBindParamCount );

            sBindParam = &sAllParamInfo[sBindParamInfo->mOffset].param;

            if ( ( sBindParam->inoutType == CMP_DB_PARAM_INPUT ) ||
                 ( sBindParam->inoutType == CMP_DB_PARAM_INPUT_OUTPUT ) )
            {
                IDE_DASSERT( sAllParamInfo[sBindParamInfo->mOffset].isParamInfoBound == ID_TRUE );
                IDE_DASSERT( sAllParamInfo[sBindParamInfo->mOffset].isParamDataBound == ID_TRUE );
            }
            else
            {
                // Nothing to do.
            }

            aBindParams[i].mId        = i + 1;
            aBindParams[i].mInoutType = sBindParam->inoutType;
            aBindParams[i].mType      = sBindParam->type;
            aBindParams[i].mData      = sBindParam->data;
            aBindParams[i].mDataSize  = sBindParam->dataSize;
            aBindParams[i].mPrecision = sBindParam->precision;
            aBindParams[i].mScale     = sBindParam->scale;

            /* BUG-46623 padding ������ 0���� �ʱ�ȭ �ؾ� �Ѵ�. */
            aBindParams[i].padding    = 0;

            /*
             * SELECT ������ LOB Ÿ�� ���ε��� PSM ���ο����� �����ϴ�.
             * �� SELECT ���� �Ű�����(?)�� ��ġ�� ������� locator Ÿ��
             * ���ε��� �������� �����Ƿ� (0x2100C Conversion not applicable. ����)
             * CHAR �Ǵ� BINARY Ÿ������ ���ε��Ѵ�.
             *
             * CHAR/BINARY Ÿ���� ���ε� ������ �ִ� ũ�� ������ �ִ�.
             * PSM ���� LOB: LOB_OBJECT_BUFFER_SIZE ������Ƽ[32000, 104857600]
             * CHAR: 65,534 = MTD_CHAR_PRECISION_MAXIMUM
             * BINARY: 10,482,953(??) = LOB_OBJECT_BUFFER_SIZE �ִ밪
             */
            if ( ( sBindParam->type == MTD_BLOB_ID ) ||
                 ( sBindParam->type == MTD_CLOB_ID ) )
            {
                sLobBindCount++;

                sLobValue = (mtdLobType *) sBindParam->data;
                aBindParams[i].mData = &sLobValue->value;

                if ( sBindParam->type == MTD_CLOB_ID )
                {
                    aBindParams[i].mPrecision = IDL_MIN(
                        aBindParams[i].mPrecision, MTD_CHAR_PRECISION_MAXIMUM);
                }
                else
                {
                    /* Nothing to do. */
                }
            }
            else if ( ( sBindParam->type == MTD_BLOB_LOCATOR_ID ) ||
                      ( sBindParam->type == MTD_CLOB_LOCATOR_ID ) )
            {
                /* BUG-48181
                 * select�� ���,
                 *   * OUT ���ε����� ���� �޾ƿ��� �� ��ü�� ���ǹ� �ϴ�(�׻� NULL locator ��ȯ��).
                 *   * IN ���ε��� ���� copy ���� OUT ���ε��� �� �Ǳ� ������ �Ұ���.
                 * ��, addLobInfoForCopy, addLobInfoForOutBind�� �ʿ���� ������
                 * sLobBindCount�� ������Ű�� �ʾƵ� �ȴ�. */
                if ( sBindParam->inoutType == CMP_DB_PARAM_INPUT )
                {
                    aBindParams[i].mInoutType = CMP_DB_PARAM_OUTPUT;
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

    *aLobBindCount = sLobBindCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LOB_COLUMN_PUSHED_FORCE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmnSDSE::setParamInfo",
                                  "LOB column was pushed force for shard view." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
        
}

IDE_RC qmnSDSE::doIt( qcTemplate * aTemplate,
                      qmnPlan    * aPlan,
                      qmcRowFlag * aFlag )
{
    qmndSDSE * sDataPlan = (qmndSDSE*) (aTemplate->tmplate.data + aPlan->offset);

    return sDataPlan->doIt( aTemplate, aPlan, aFlag );
}

IDE_RC qmnSDSE::doItAllFalse( qcTemplate * aTemplate,
                              qmnPlan    * aPlan,
                              qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description : Constant Filter �˻��Ŀ� �����Ǵ� �Լ��� ���� �����ϴ�
 *               Record�� �������� �ʴ´�.
 *
 * Implementation : �׻� record ������ �����Ѵ�.
 *
 ***********************************************************************/

    qmncSDSE * sCodePlan = (qmncSDSE*)aPlan;
    qmndSDSE * sDataPlan = (qmndSDSE*)(aTemplate->tmplate.data + aPlan->offset);

    // ���ռ� �˻�
    IDE_DASSERT( sCodePlan->constantFilter != NULL );
    IDE_DASSERT( ( *sDataPlan->flag & QMND_SDSE_ALL_FALSE_MASK ) == QMND_SDSE_ALL_FALSE_TRUE );

    // ������ ������ Setting
    *aFlag &= ~QMC_ROW_DATA_MASK;
    *aFlag |= QMC_ROW_DATA_NONE;

    return IDE_SUCCESS;
}

IDE_RC qmnSDSE::doItFirst( qcTemplate * aTemplate,
                           qmnPlan    * aPlan,
                           qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description : data ������ ���� �ʱ�ȭ�� �����ϰ�
 *               data �� �������� ���� �Լ��� ȣ���Ѵ�.
 *
 * Implementation :
 *              - allocStmt
 *              - prepare
 *              - bindCol (PROJ-2638 ������ ����)
 *              - execute
 *
 ***********************************************************************/

    qmncSDSE       * sCodePlan = (qmncSDSE *)aPlan;
    qmndSDSE       * sDataPlan = (qmndSDSE *)(aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo  * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    // ������ ���� �˻�
    IDE_TEST( iduCheckSessionEvent( aTemplate->stmt->mStatistics ) != IDE_SUCCESS );

    // DataPlan �ʱ�ȭ
    sDataPlan->mCurrScanNode  = 0;
    sDataPlan->mScanDoneCount = 0;

    if ( sDataPlan->lobBindCount > 0 &&
         sCodePlan->mShardParamCount > 0 )
    {
        IDE_TEST( setLobInfo( aTemplate,
                              sCodePlan )
                  != IDE_SUCCESS );
    }

    //-------------------------------
    // Transformed out ref column bind 
    //-------------------------------

    // TASK-7219 Non-shard DML
    IDE_TEST( setTransformedOutRefBindValue( aTemplate,
                                             sCodePlan )
              != IDE_SUCCESS );

    //-------------------------------
    // ������ ����
    //-------------------------------

    IDE_TEST( sdi::decideShardDataInfo(
                  aTemplate,
                  &(aTemplate->tmplate.rows[aTemplate->tmplate.variableRow]),
                  sCodePlan->mShardAnalysis,
                  sClientInfo,
                  sDataPlan->mDataInfo,
                  sCodePlan->mQueryPos )
              != IDE_SUCCESS );

    /* PROJ-2733-DistTxInfo �л������� ��������. */ 
    sdi::calculateGCTxInfo( aTemplate,
                            sDataPlan->mDataInfo,
                            aTemplate->shardExecData.globalPSM,
                            sCodePlan->shardDataIndex );

    //-------------------------------
    // ����
    //-------------------------------

    IDE_TEST( sdi::executeSelect( aTemplate->stmt,
                                  sClientInfo,
                                  sDataPlan->mDataInfo )
              != IDE_SUCCESS );

    IDE_TEST( doItNext( aTemplate, aPlan, aFlag ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDSE::doItNext( qcTemplate * aTemplate,
                          qmnPlan    * aPlan,
                          qmcRowFlag * aFlag )
{
/***********************************************************************
 *
 * Description : data �� �������� �Լ��� �����Ѵ�.
 *
 *    Ư�� data node �� buffer �� ���� ��Ե� ��츦 �����Ͽ�,
 *    data node �� �� ���� ���ư��鼭 �����Ѵ�.
 *
 *    ����� ���� data node �� �ǳʶٸ�,
 *    ��� data node �� doIt ����� QMC_ROW_DATA_NONE(no rows)��
 *    �� �� ���� �����Ѵ�.
 *
 * Implementation :
 *              - fetch
 *
 ***********************************************************************/

    qmncSDSE       * sCodePlan = (qmncSDSE*)aPlan;
    qmndSDSE       * sDataPlan = (qmndSDSE*)(aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo  * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;
    sdiConnectInfo * sConnectInfo = NULL;
    sdiDataNode    * sDataNode = NULL;
    mtcColumn      * sColumn = NULL;
    idBool           sJudge = ID_FALSE;
    idBool           sExist = ID_FALSE;
    UInt             i;

    UInt             sRemoteStmtId;
    UInt             sMmSessId;
    UInt             sMmStmtId;
    UInt             sLocatorInfo = 0;
    UChar          * sRow = NULL;
    smLobLocator     sRemoteLobLocator;
    smLobLocator     sShardLobLocator;

    sMmSessId = qci::mSessionCallback.mGetSessionID(
            aTemplate->stmt->session->mMmSession );
    sMmStmtId = qci::mSessionCallback.mGetStmtId(
            QC_MM_STMT( aTemplate->stmt ) );

    //mmtCmsFetch.cpp:doFetch->qci::getFetchColumnInfo���� �����ǹǷ� ���⿡�� ������ �ʿ� ����
    //sLocatorInfo = MTC_LOB_LOCATOR_CLIENT_TRUE;

    while ( 1 )
    {
        if ( sDataPlan->mCurrScanNode == sClientInfo->mCount )
        {
            // ���� doIt�� ������ data node ���� ���� �Ǿ��ٸ�,
            // ù��° data node ���� �ٽ� doIt�ϵ��� �Ѵ�.
            sDataPlan->mCurrScanNode = 0;
        }
        else
        {
            // Nothing to do.
        }

        sConnectInfo = &(sClientInfo->mConnectInfo[sDataPlan->mCurrScanNode]);
        sDataNode = &(sDataPlan->mDataInfo->mNodes[sDataPlan->mCurrScanNode]);

        // ���� doIt�� ����� ������ data node �� skip�Ѵ�.
        if ( sDataNode->mState == SDI_NODE_STATE_EXECUTED )
        {
            sJudge = ID_FALSE;

            while ( sJudge == ID_FALSE )
            {
                // fetch client
                IDE_TEST( sdi::fetch( sConnectInfo, sDataNode, &sExist )
                          != IDE_SUCCESS );

                sRow = (UChar*)sDataNode->mBuffer[sDataPlan->mCurrScanNode];

                // �߸��� �����Ͱ� fetch�Ǵ� ��츦 ����Ѵ�.
                sColumn = sDataPlan->plan.myTuple->columns;
                for ( i = 0; i < sDataPlan->plan.myTuple->columnCount; i++, sColumn++ )
                {
                    /* TASK-7219 */
                    if ( ( sColumn->flag & MTC_COLUMN_NULL_TYPE_MASK ) == MTC_COLUMN_NULL_TYPE_TRUE )
                    {
                        sColumn->module->null( sColumn,
                                               (UChar *)sRow + sColumn->column.offset );
                    }
                    else
                    {
                        IDE_TEST_RAISE( sColumn->module->actualSize(
                                            sColumn,
                                            sRow + sColumn->column.offset ) >
                                        sColumn->column.size,
                                        ERR_INVALID_DATA_FETCHED );
                    }

                    /* PROJ-2728 Sharding LOB */
                    if ( ( sExist == ID_TRUE ) &&
                         ( sColumn->module->id == MTD_BLOB_LOCATOR_ID ||
                           sColumn->module->id == MTD_CLOB_LOCATOR_ID ) )
                    {
                        sRemoteLobLocator = * (smLobLocator *) ( sRow + sColumn->column.offset );
                        sRemoteStmtId = sdi::getRemoteStmtId(sDataNode);

                        IDE_TEST( smiLob::openShardLobCursor(
                                    (QC_SMI_STMT(aTemplate->stmt))->getTrans(),
                                    sMmSessId,
                                    sMmStmtId, // mmcStatement ID
                                    sRemoteStmtId,
                                    sConnectInfo->mNodeId,
                                    sColumn->module->id,
                                    sRemoteLobLocator, 
                                    sLocatorInfo,
                                    SMI_LOB_TABLE_CURSOR_MODE,
                                    &sShardLobLocator )
                                  != IDE_SUCCESS );

                        * (smLobLocator *) ( sRow + sColumn->column.offset ) = sShardLobLocator;

                        IDE_TEST( aTemplate->cursorMgr->addOpenedLobCursor( sShardLobLocator )
                                  != IDE_SUCCESS );
                    }
                }

                //------------------------------
                // Data ���� ���ο� ���� ó��
                //------------------------------

                if ( sExist == ID_TRUE )
                {
                    sDataPlan->plan.myTuple->row =
                        sDataNode->mBuffer[sDataPlan->mCurrScanNode];

                    // BUGBUG nullRID
                    SMI_MAKE_VIRTUAL_NULL_GRID( sDataPlan->plan.myTuple->rid );

                    sDataPlan->plan.myTuple->modify++;

                    if ( sCodePlan->filter != NULL )
                    {
                        IDE_TEST( qtc::judge( &sJudge,
                                              sCodePlan->filter,
                                              aTemplate )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        sJudge = ID_TRUE;
                    }

                    if ( sJudge == ID_TRUE )
                    {
                        if ( sCodePlan->subqueryFilter != NULL )
                        {
                            IDE_TEST( qtc::judge( &sJudge,
                                                  sCodePlan->subqueryFilter,
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
                        if ( sCodePlan->nnfFilter != NULL )
                        {
                            IDE_TEST( qtc::judge( &sJudge,
                                                  sCodePlan->nnfFilter,
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
                        *aFlag = QMC_ROW_DATA_EXIST;
                        sDataPlan->doIt = qmnSDSE::doItNext;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    sDataPlan->plan.myTuple->row =
                        sDataNode->mBuffer[sDataPlan->mCurrScanNode];

                    // a data node fetch complete
                    sDataNode->mState = SDI_NODE_STATE_FETCHED;
                    sDataPlan->mScanDoneCount++;
                    break;
                }
            }

            if ( sJudge == ID_TRUE )
            {
                IDE_DASSERT( *aFlag == QMC_ROW_DATA_EXIST );
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if ( sDataNode->mState < SDI_NODE_STATE_EXECUTED )
            {
                sDataPlan->mScanDoneCount++;
            }
            else
            {
                IDE_DASSERT( sDataNode->mState == SDI_NODE_STATE_FETCHED );
            }
        }

        if ( sDataPlan->mScanDoneCount == sClientInfo->mCount )
        {
            *aFlag = QMC_ROW_DATA_NONE;
            sDataPlan->doIt = qmnSDSE::doItFirst;
            break;
        }
        else
        {
            sDataPlan->mCurrScanNode++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DATA_FETCHED )
    {
        IDE_SET( ideSetErrorCode( mtERR_ABORT_INVALID_LENGTH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDSE::padNull( qcTemplate * aTemplate,
                         qmnPlan    * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSDSE * sCodePlan = (qmncSDSE*)aPlan;
    qmndSDSE * sDataPlan = (qmndSDSE*)(aTemplate->tmplate.data + aPlan->offset);

    if ( ( aTemplate->planFlag[sCodePlan->planID] & QMND_SDSE_INIT_DONE_MASK )
         == QMND_SDSE_INIT_DONE_FALSE )
    {
        // �ʱ�ȭ ���� ���� ��� �ʱ�ȭ ����
        IDE_TEST( aPlan->init( aTemplate, aPlan ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if ( ( sCodePlan->plan.flag & QMN_PLAN_STORAGE_MASK ) == QMN_PLAN_STORAGE_DISK )
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
            IDE_TEST( aTemplate->stmt->qmxMem->cralloc( sDataPlan->plan.myTuple->rowOffset,
                                                        (void**) &sDataPlan->nullRow )
                      != IDE_SUCCESS );

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
            // Nothing to do.
        }

        // Null Row ����
        idlOS::memcpy( sDataPlan->plan.myTuple->row,
                       sDataPlan->nullRow,
                       sDataPlan->plan.myTuple->rowOffset );

        // Null RID�� ����
        idlOS::memcpy( &sDataPlan->plan.myTuple->rid,
                       &sDataPlan->nullRID,
                       ID_SIZEOF(scGRID) );
    }
    else
    {
        //-----------------------------------
        // Memory Table�� ���
        //-----------------------------------
        // data node �� tuple�� �׻� disk tuple�̴�.
        IDE_DASSERT( 1 );
    }

    sDataPlan->plan.myTuple->modify++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmnSDSE::printPlan( qcTemplate   * aTemplate,
                           qmnPlan      * aPlan,
                           ULong          aDepth,
                           iduVarString * aString,
                           qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description : SDSE ����� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmncSDSE * sCodePlan = (qmncSDSE*)aPlan;
    qmndSDSE * sDataPlan = (qmndSDSE*)(aTemplate->tmplate.data + aPlan->offset);
    sdiClientInfo * sClientInfo = aTemplate->stmt->session->mQPSpecific.mClientInfo;

    sDataPlan->flag = & aTemplate->planFlag[sCodePlan->planID];

    //----------------------------
    // SDSE ��� ǥ��
    //----------------------------
    if ( QCU_TRCLOG_DETAIL_MTRNODE == 1 )
    {
        qmn::printSpaceDepth( aString, aDepth );
        iduVarStringAppend( aString, "SHARD-COORDINATOR [ " );
        iduVarStringAppendFormat( aString, "SELF: %"ID_INT32_FMT" ]\n",
                                  (SInt)sCodePlan->tupleRowID );
    }
    else
    {
        qmn::printSpaceDepth( aString, aDepth );
        iduVarStringAppend( aString, "SHARD-COORDINATOR\n" );
    }

    /* BUG-45899 */
    if ( sdi::isAnalysisInfoPrintable( aTemplate->stmt ) == ID_TRUE )
    {
        // non-shard query �� ���� ���
        if ( ( sCodePlan->mQueryPos != NULL ) &&
             ( aTemplate->stmt->mShardPrintInfo.mQueryType == SDI_QUERY_TYPE_NONSHARD ) )
        {
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppend( aString, "[ DISTRIBUTION QUERY ]\n" );
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppendFormat( aString,
                                      "%.*s",
                                      sCodePlan->mQueryPos->size,
                                      sCodePlan->mQueryPos->stmtText + sCodePlan->mQueryPos->offset );
            iduVarStringAppend( aString, "\n" );
        }
    }

    //----------------------------
    // Predicate ������ �� ���
    //----------------------------
    if ( ( QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE( aTemplate->stmt ) == 1 ) ||
         ( SDU_SHARD_REBUILD_PLAN_DETAIL_FORCE_ENABLE == 1 ) )
    {
        // Normal Filter ���
        if ( sCodePlan->filter != NULL )
        {
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppend( aString, " [ FILTER ]\n" );
            IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                                aString,
                                                aDepth + 1,
                                                sCodePlan->filter )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // Constant Filter
        if ( sCodePlan->constantFilter != NULL )
        {
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppend( aString, " [ CONSTANT FILTER ]\n" );
            IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                                aString,
                                                aDepth + 1,
                                                sCodePlan->constantFilter )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // Subquery Filter
        if ( sCodePlan->subqueryFilter != NULL )
        {
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppend( aString, " [ SUBQUERY FILTER ]\n" );
            IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                                aString,
                                                aDepth + 1,
                                                sCodePlan->subqueryFilter )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // NNF Filter
        if ( sCodePlan->nnfFilter != NULL )
        {
            qmn::printSpaceDepth( aString, aDepth );
            iduVarStringAppend( aString, " [ NOT-NORMAL-FORM FILTER ]\n" );
            IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                                aString,
                                                aDepth + 1,
                                                sCodePlan->nnfFilter )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( sClientInfo != NULL )
        {
            // �������� ���
            IDE_DASSERT( QMND_SDSE_INIT_DONE_TRUE == QMND_SDEX_INIT_DONE_TRUE );

            IDE_TEST( qmnSDEX::printDataInfo( aTemplate,
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
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------
    // Subquery ������ ���.
    //----------------------------
    // subquery�� constant filter, nnf filter, subquery filter���� �ִ�.
    // Constant Filter�� Subquery ���� ���
    if ( sCodePlan->constantFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->constantFilter,
                                          aDepth,
                                          aString,
                                          aMode )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // Subquery Filter�� Subquery ���� ���
    if ( sCodePlan->subqueryFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->subqueryFilter,
                                          aDepth,
                                          aString,
                                          aMode )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // NNF Filter�� Subquery ���� ���
    if ( sCodePlan->nnfFilter != NULL )
    {
        IDE_TEST( qmn::printSubqueryPlan( aTemplate,
                                          sCodePlan->nnfFilter,
                                          aDepth,
                                          aString,
                                          aMode )
                  != IDE_SUCCESS );
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
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2728 Sharding LOB */
IDE_RC qmnSDSE::setLobInfo( qcTemplate   * aTemplate,
                            qmncSDSE     * aCodePlan )
{
    qciBindParamInfo  * sAllParamInfo  = NULL;
    qciBindParam      * sBindParam     = NULL;
    sdiOutBindParam   * sOutBindParams = NULL;
    mtdLobType        * sLobValue      = NULL;
    qcShardParamInfo  * sShardBindInfo = NULL;  /* TASK-7219 Non-shard DML */
    UShort              i;                   /* TASK-7219 */

    sAllParamInfo  = aTemplate->stmt->pBindParam;
    sOutBindParams = (sdiOutBindParam*) ( aTemplate->shardExecData.data + aCodePlan->mOutBindParam );

    for ( i = 0; i < aCodePlan->mShardParamCount; i++ )
    {
        sShardBindInfo = aCodePlan->mShardParamInfo + i; /* TASK-7219 Non-shard DML */

        IDE_DASSERT( sShardBindInfo->mOffset < aTemplate->stmt->pBindParamCount );

        sBindParam = &sAllParamInfo[sShardBindInfo->mOffset].param;

        if ( sBindParam->type == MTD_CLOB_ID ||
             sBindParam->type == MTD_BLOB_ID )
        {
            /* �����δ� sdl::bindParam���� 
             * MTD_BLOB_ID -> MTD_BINARY_ID,
             * MTD_CLOB_ID -> MTD_VARCHAR_ID �� ���ε��ȴ� */
            sLobValue = (mtdLobType *) sBindParam->data;

            IDE_TEST_RAISE( sBindParam->type == MTD_CLOB_ID &&
                            sLobValue->length > MTD_CHAR_PRECISION_MAXIMUM,
                            ERR_CONVERSION_NOT_APPLICABLE );

            sOutBindParams[i].mIndicator = sLobValue->length;
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

IDE_RC qmnSDSE::setTransformedOutRefBindValue( qcTemplate * aTemplate,
                                               qmncSDSE   * aCodePlan )
{
    /* TASK-7219 Non-shard DML */
    UShort             i              = 0;
    qcShardParamInfo * sBindParamInfo = NULL;
    mtcTuple         * sTuple         = NULL;
    mtcColumn        * sColumn        = NULL;
    sdiBindParam     * sBindParams    = NULL;

    void             * sBindValue     = NULL;


    sBindParams = (sdiBindParam*)
        ( aTemplate->shardExecData.data + aCodePlan->mBindParam );

    for ( i = 0; i < aCodePlan->mShardParamCount; i++ )
    {
        sBindParamInfo = aCodePlan->mShardParamInfo + i;

        if ( sBindParamInfo->mIsOutRefColumnBind == ID_TRUE )
        {
            sTuple = & aTemplate->tmplate.rows[ sBindParamInfo->mOutRefTuple ];
            sColumn = sTuple->columns + sBindParamInfo->mOffset;

            /* Outer relation tuple read�� ������ ���� �����´�. */
            sBindValue = (UChar*)mtc::value( sColumn,
                                             sTuple->row,
                                             MTD_OFFSET_USE );

            /* ������ ���� transformed bind data ptr��ġ�� �����Ѵ�. */
            idlOS::memcpy( sBindParams[i].mData,
                           sBindValue,
                           sBindParams[i].mDataSize );

        }
        else
        {
            /* Nothing to do. */
        }
    }

    return IDE_SUCCESS;
}
