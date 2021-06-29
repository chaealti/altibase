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
 
#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <smErrorCode.h>
#include <svm.h>
#include <smp.h>
#include <svp.h>
#include <svcDef.h>
#include <svcReq.h>
#include <svcRecord.h>
#include <svcRecordUndo.h>
#include <svcLob.h>
#include <sgmManager.h>

/***********************************************************************
 * Description : aHeader�� ����Ű�� Table�� ���ؼ� Insert�� �����Ѵ�.
 *
 * aStatistics     - [IN] None
 * aTrans          - [IN] Transaction Pointer
 * aTableInfoPtr   - [IN] Table Info Pointer(Table�� Transaction�� Insert,
 *                        Delete�� Record Count�� ���� ������ ������ �ִ�.
 * aHeader         - [IN] Table Header Pointer
 * aInfinite       - [IN] Insert�� ���ο� Record�� �Ҵ��ؾ� �Ǵµ� �� ��
 *                        Record Header�� SCN�� Setting�� ��
 * aRow            - [IN] Insert�ϱ� ���� ���Ӱ� �Ҵ�� Record�� Pointer
 *
 * aRetInsertSlotGRID - [IN] None
 * aValueList      - [IN] Value List
 * aAddOIDFlag     - [IN] 1.SM_INSERT_ADD_OID_OK : OID List�� Insert�� �۾����.
 *                        2.SM_INSERT_ADD_OID_NO : OID List�� Insert�� �۾��� ������� �ʴ´�.
                          1.SM_UNDO_LOGGING_OK : refineDB, createTable 
                          2.SM_UNDO_LOGGING_NO : undo�� Compensation Log�� ������� �ʴ´�.
 *  log structure:
 *   <update_log, NewOid, FixedRow_size, Fixed_Row, var_col1, var_col2...>
 ***********************************************************************/
IDE_RC svcRecord::insertVersion( idvSQL         * /*aStatistics*/,
                                 void           * aTrans,
                                 void           * aTableInfoPtr,
                                 smcTableHeader * aHeader,
                                 smSCN            aInfinite,
                                 SChar         ** aRow,
                                 scGRID         * aRetInsertSlotGRID,
                                 const smiValue * aValueList,
                                 UInt             aAddOIDFlag )
{
    const smiColumn   * sCurColumn;
    SChar             * sNewFixRowPtr = NULL;
    const smiValue    * sCurValue;
    UInt                i;
    smOID               sFixOid;
    scPageID            sPageID             = SM_NULL_PID;
    UInt                sColumnCount        = 0;
    UInt                sUnitedVarColumnCount     = 0;
    idBool              sIsAddOID = ID_FALSE;
    smcLobDesc        * sCurLobDesc;
    smpPersPageHeader * sPagePtr;
    smiValue            sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];
    const smiColumn   * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    
    IDE_ERROR( aTrans     != NULL );
    IDE_ERROR( aHeader    != NULL );
    IDE_ERROR( aValueList != NULL );

    IDU_FIT_POINT( "1.PROJ-2118@svcRecord::insertVersion" );

    /* insert�ϱ� ���Ͽ� ���ο� Slot �Ҵ� ����.*/
    IDE_TEST( svpFixedPageList::allocSlot( aTrans,
                                           aHeader->mSpaceID,
                                           aTableInfoPtr,
                                           aHeader->mSelfOID,
                                           &(aHeader->mFixed.mVRDB),
                                           &sNewFixRowPtr,
                                           aInfinite,
                                           aHeader->mMaxRow,
                                           SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT)
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sFixOid = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * TableOID�� Get�Ѵ�.*/
    IDE_ASSERT( smmManager::getPersPagePtr( aHeader->mSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    IDE_ASSERT( sPagePtr->mTableOID == aHeader->mSelfOID );

    /* Insert�� Row�� ���Ͽ� version list �߰� */
    if ( SM_INSERT_ADD_OID_IS_OK(aAddOIDFlag) )
    {
        sIsAddOID = ID_TRUE;
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sFixOid,
                                           aHeader->mSpaceID,
                                           SM_OID_NEW_INSERT_FIXED_SLOT )
                  != IDE_SUCCESS);
    }
    else
    {
        /* nothing to do */
    }

    /* --------------------------------------------------------
     * [4] Row�� fixed ������ ���� �α� ��
     *     variable column�� ���� ���� ����Ÿ ����
     *     (�α׷��ڵ带 �����ϴ� �۾��̸�
     *      ���� ����Ÿ���ڵ��� �۾��� ������ �κп��� ������)
     * -------------------------------------------------------- */
    sColumnCount = aHeader->mColumnCount;
    sCurValue    = aValueList;

    for( i = 0; i < sColumnCount; i++ )
    {
        /* Variable column�� ���
            (1) Variabel page���� slot �Ҵ��Ͽ� ���� ����Ÿ ���ڵ�  ����.
            (2) ���ڵ��� fixed ������ smVCDesc ���� ���� */
        sCurColumn = smcTable::getColumn(aHeader,i);

        /* --------------------------------------------------------------------
         *  [4-1] Variable column�� ���
         *        (1) Variabel page���� slot �Ҵ��Ͽ� ���� ����Ÿ ���ڵ�  ����.
         *        (2) ���ڵ��� fixed ������ smVarColumn ���� ����
         * -------------------------------------------------------------------- */
        if( (sCurColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sCurColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                i,
                sCurColumn->size,
                sCurColumn->offset,
                sCurValue->length );
        }
        else
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sCurColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                i,
                sCurColumn->size,
                sCurColumn->offset,
                sCurValue->length );
        }

        IDU_FIT_POINT( "2.PROJ-2118@svcRecord::insertVersion" );

        switch( sCurColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SMI_COLUMN_TYPE_LOB:

                sCurLobDesc = (smcLobDesc *)(sNewFixRowPtr + sCurColumn->offset);

                /* prepare���� ���� LobDesc�� old version���� �ν��ϱ� ������ �ʱ�ȭ */
                SMC_LOB_DESC_INIT( sCurLobDesc );

                if( sCurValue->length > 0 )
                {
                    /* SQL�� ���� Lob �����͸� ���� ��� */
                    IDE_TEST( svcLob::reserveSpaceInternal(
                                                      aTrans,
                                                      (smcTableHeader *)aHeader,
                                                      (smiColumn *)sCurColumn,
                                                      sCurLobDesc->mLobVersion,
                                                      sCurLobDesc,
                                                      0,
                                                      sCurValue->length,
                                                      aAddOIDFlag,
                                                      ID_TRUE /* aIsFullWrite */)
                              != IDE_SUCCESS );

                    IDE_TEST( svcLob::writeInternal( (UChar *)sNewFixRowPtr,
                                                     (smiColumn *)sCurColumn,
                                                     0,
                                                     sCurValue->length,
                                                     (UChar *)sCurValue->value )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Here length is always 0. */
                    
                    if( sCurValue->value == NULL )
                    {
                        /* null */
                        sCurLobDesc->flag =
                            SM_VCDESC_MODE_IN | SM_VCDESC_NULL_LOB_TRUE;
                    }
                    else
                    {
                        /* empty (nothing to do) */
                    }
                }

                sCurLobDesc->length = sCurValue->length;
                
                break;

            case SMI_COLUMN_TYPE_VARIABLE:
                sUnitedVarColumns[sUnitedVarColumnCount]       = sCurColumn;
                sUnitedVarValues[sUnitedVarColumnCount].length = sCurValue->length;
                sUnitedVarValues[sUnitedVarColumnCount].value  = sCurValue->value;
                sUnitedVarColumnCount++;

                break;
            case SMI_COLUMN_TYPE_FIXED:

                IDU_FIT_POINT( "3.PROJ-2118@svcRecord::insertVersion" );

                // BUG-30104 Fixed Column�� length�� 0�� �� �� �����ϴ�.
                IDE_ERROR_MSG( sCurValue->length > 0,
                               "Table OID  : %"ID_vULONG_FMT"\n"
                               "Space ID   : %"ID_UINT32_FMT"\n"
                               "Column Seq : %"ID_UINT32_FMT"\n",
                               aHeader->mSelfOID,
                               aHeader->mSpaceID,
                               i );

                /* Fixed column�� ���
                   (1) ���ڵ��� fixed ������ ���� �� ���� */
                idlOS::memcpy(sNewFixRowPtr + sCurColumn->offset,
                              sCurValue->value,
                              sCurValue->length);

                break;

            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            default:
                ideLog::log( IDE_ERR_0,
                             "Invalid column[%u] "
                             "flag : %u "
                             "( TableOID : %lu )\n",
                             i,
                             sCurColumn->flag,
                             aHeader->mSelfOID );

                IDE_ASSERT( 0 );
                break;
        }

        sCurValue++ ;
    }

    if ( sUnitedVarColumnCount > 0)
    {
        IDE_TEST( insertUnitedVarColumns ( aTrans,
                                           aHeader,
                                           sNewFixRowPtr,
                                           sUnitedVarColumns,
                                           sIsAddOID,
                                           sUnitedVarValues,
                                           sUnitedVarColumnCount )
                != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    /*
     * [BUG-24353] volatile table�� modify column ������ ���ڵ尡 ������ϴ�.
     * - Volatile Table�� Compensation Log�� ������� �ʴ´�.
     */
    if( SM_UNDO_LOGGING_IS_OK(aAddOIDFlag) )
    {
        /* Insert�� ���� Log�� ����Ѵ�. */
        IDE_TEST( svcRecordUndo::logInsert( smLayerCallback::getLogEnv( aTrans ),
                                            aTrans,
                                            aHeader->mSelfOID,
                                            aHeader->mSpaceID,
                                            sNewFixRowPtr )
                  != IDE_SUCCESS);
    }

    /* BUG-14513: Fixed Slot�� �Ҵ�� Alloc Slot Log�� �������
       �ʵ��� ����. insert log�� Alloc Slot�� ���� Redo, Undo����.
       �� ������ insert log��� �� Slot header�� ���� Update����
    */
    svpFixedPageList::setAllocatedSlot( aInfinite, sNewFixRowPtr );

    if (aTableInfoPtr != NULL)
    {
        /* Record�� ���� �߰��Ǿ����Ƿ� Record Count�� ���������ش�.*/
        smLayerCallback::incRecCntOfTableInfo( aTableInfoPtr );
    }

    if(aRow != NULL)
    {
        *aRow = sNewFixRowPtr;
    }
    if(aRetInsertSlotGRID != NULL)
    {
        SC_MAKE_GRID( *aRetInsertSlotGRID,
                      aHeader->mSpaceID,
                      sPageID,
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INVALID_COLUMN_SIZE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-39399, BUG-39507
 *
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aHeader       - [IN] aRow�� ���� �ִ� Table Header Pointer
 * aRow          - [IN] Update�� Row.
 * aInfinite     - [IN] New Version�� Row SCN
 * aIsUpdatedBySameStmt - [OUT] ���� statement���� update �ߴ��� ����
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC svcRecord::isUpdatedVersionBySameStmt( void            * aTrans,
                                              smSCN             aViewSCN,
                                              smcTableHeader  * aHeader,
                                              SChar           * aRow,
                                              smSCN             aInfinite,
                                              idBool          * aIsUpdatedBySameStmt,
                                              idBool            aForbiddenToRetry )
{
    UInt        sState      = 0;
    scPageID    sPageID     = SMP_SLOT_GET_PID( aRow );

    /* 1. hold x latch */
    IDE_TEST( svmManager::holdPageXLatch( aHeader->mSpaceID, sPageID )
              != IDE_SUCCESS );
    sState = 1;

    /* 2. isUpdatedVersionBySameStmt - checkUpdatedVersionBySameStmt */
    IDE_TEST( checkUpdatedVersionBySameStmt( aTrans,
                                             aViewSCN,
                                             aHeader,
                                             aRow,
                                             aInfinite,
                                             aIsUpdatedBySameStmt,
                                             aForbiddenToRetry )
              != IDE_SUCCESS );

    /* 3. relase latch */
    sState = 0;
    IDE_TEST( svmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( svmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-39399, BUG-39507
 *
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] ViewSCN
 * aHeader       - [IN] aRow�� ���� �ִ� Table Header Pointer
 * aRow          - [IN] Delete�� Update��� Row
 * aInfiniteSCN  - [IN] New Version�� Row SCN
 * aIsUpdatedBySameStmt - [OUT] ���� statement���� update �ߴ��� ����
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC svcRecord::checkUpdatedVersionBySameStmt( void             * aTrans,
                                                 smSCN              aViewSCN,
                                                 smcTableHeader   * aHeader,
                                                 SChar            * aRow,
                                                 smSCN              aInfiniteSCN,
                                                 idBool           * aIsUpdatedBySameStmt,
                                                 idBool             aForbiddenToRetry )
{
    smSCN   sNxtRowSCN;
    smSCN   sInfiniteSCNWithDeleteBit;
    smTID   sNxtRowTID      = SM_NULL_TID;

    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aTrans  != NULL );
    IDE_ASSERT( aRow    != NULL );

    SM_INIT_SCN( &sNxtRowSCN );
    SM_INIT_SCN( &sInfiniteSCNWithDeleteBit );

    *aIsUpdatedBySameStmt   = ID_FALSE; /* BUG-39507 */
    SM_SET_SCN( &sInfiniteSCNWithDeleteBit, &aInfiniteSCN );
    SM_SET_SCN_DELETE_BIT( &sInfiniteSCNWithDeleteBit );

    /* BUG-15642 Record�� �ι� Free�Ǵ� ��찡 �߻���.:
     * �̷� ��찡 �߻��ϱ����� validateUpdateTargetRow����
     * üũ�Ͽ� ������ �ִٸ� Rollback��Ű�� �ٽ� retry�� �����Ŵ*/
    /* �� �Լ� �������� row�� ���ϴ� �κ��� �������� �ʴ´�.
     * �׷��� ������ row�� ���ϴ� �κ��� ���� �Ҵ��Ͽ�
     * �Լ������ڷ� �� �ʿ䰡 ����. */

   IDE_TEST_RAISE( validateUpdateTargetRow( aTrans,
                                            aHeader->mSpaceID,
                                            aViewSCN,
                                            (smpSlotHeader *)aRow,
                                            NULL /*aRetryInfo*/ )
                    != IDE_SUCCESS, already_modified );

    SMX_GET_SCN_AND_TID( ((smpSlotHeader *)aRow)->mLimitSCN,
                         sNxtRowSCN,
                         sNxtRowTID );

    /* Next Version�� Lock Row�� ��� */
    if( SM_SCN_IS_LOCK_ROW( sNxtRowSCN ) )
    {
        if ( sNxtRowTID == smLayerCallback::getTransID( aTrans ) )
        {
            /* aRow�� ���� Lock�� aTrans�� �̹� ��Ҵ�.*/
            /* BUG-39399, BUG-39507
             * 1. aInfiniteSCN�� sNxtRowSCN�� InfiniteSCN �� ���ٸ�,
             *    ���� statement���� update �� ����̴�.
             * 2. sNxtRowSCN�� aInfiniteSCN + SM_SCN_INF_DELETE_BIT �̸�
             *    ���� statement���� delete �� ����̴�. */
            if( SM_SCN_IS_EQ( &sNxtRowSCN, &aInfiniteSCN ) ||
                SM_SCN_IS_EQ( &sNxtRowSCN, &sInfiniteSCNWithDeleteBit ) )
            {
                *aIsUpdatedBySameStmt = ID_TRUE;
            }
            else
            {
                *aIsUpdatedBySameStmt = ID_FALSE;
            }
        }
        else
        {
            /* �ٸ� Ʈ������� lock�� ������Ƿ� �� stmt���� update ���� �� ����. */
            *aIsUpdatedBySameStmt = ID_FALSE;
        }
    }
    else
    {
        /* BUG-39399, BUG-39507
         * 1. aInfiniteSCN�� sNxtRowSCN�� InfiniteSCN �� ���ٸ�,
         *    ���� statement���� update �� ����̴�.
         * 2. sNxtRowSCN�� aInfiniteSCN + SM_SCN_INF_DELETE_BIT �̸�
         *    ���� statement���� delete �� ����̴�. */
        if( SM_SCN_IS_EQ( &sNxtRowSCN, &aInfiniteSCN ) ||
            SM_SCN_IS_EQ( &sNxtRowSCN, &sInfiniteSCNWithDeleteBit ) )
        {
            *aIsUpdatedBySameStmt = ID_TRUE;
        }
        else
        {
            *aIsUpdatedBySameStmt = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( already_modified );
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE ); 

            SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[UPDATE VALIDATION] "
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CreateSCN:%"ID_UINT64_FMT,
                             aHeader->mSelfOID,
                             SM_SCN_TO_LONG(aViewSCN),
                             SM_SCN_TO_LONG(((smpSlotHeader *)aRow)->mCreateSCN) );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) ); 

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode(smERR_RETRY_Already_Modified) );
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aOldRow�� Update�Ѵ�. Update�� MMDB�� MVCC�� ���ؼ�
 *               ���ο� Version�� ����� ����. Update�� ������ �Ǹ�
 *               aOldRow�� ���ο� Version�� ����Ű�� �ȴ�.
 *
 * aStatistics   - [IN] None
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aHeader       - [IN] aOldRow�� ���� �ִ� Table Header Pointer
 * aOldRow       - [IN] Update�� Row.
 * aOldGRID      - [IN] None
 * aRow          - [OUT] Update���� ������� New Version�� Row Pointer�� �Ѱ��ش�.
 * aRetSlotGRID  - [IN] None
 * aColumnList   - [IN] Update�� Column List
 * aValueList    - [IN] Update�� Column���� ���ο� Vaule List
 * aRetryInfo    - [IN] Retry�� ���� ������ column�� list
 * aInfinite     - [IN] New Version�� Row SCN
 * aModifyIdxBit - [IN] None
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC svcRecord::updateVersion( idvSQL               * /*aStatistics*/,
                                 void                 * aTrans,
                                 smSCN                  aViewSCN,
                                 void                 * /*aTableInfoPtr*/,
                                 smcTableHeader       * aHeader,
                                 SChar                * aOldRow,
                                 scGRID                 /*aOldGRID*/,
                                 SChar                **aRow,
                                 scGRID               * aRetSlotGRID,
                                 const smiColumnList  * aColumnList,
                                 const smiValue       * aValueList,
                                 const smiDMLRetryInfo* aRetryInfo,
                                 smSCN                  aInfinite,
                                 void                 * /*aLobInfo4Update*/,
                                 ULong                  /*aModifyIdxBit*/,
                                 idBool                 aForbiddenToRetry )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;
    const smiValue        * sCurValue;
    SChar                 * sNewFixRowPtr = NULL;
    smOID                   sFixOID;
    smOID                   sTargetOID;
    UInt                    sState = 0;
    UInt                    sColumnSeq;
    scPageID                sPageID = SM_NULL_PID;
    vULong                  sFixedRowSize;
    smpSlotHeader         * sOldSlotHeader = (smpSlotHeader*)aOldRow;
    smpPersPageHeader     * sPagePtr;
    smOID                   sFstVCPieceOID;
    scPageID                sNewFixRowPID;
    smcLobDesc            * sCurLobDesc;
    idBool                  sIsHeadNextChanged = ID_FALSE;
    ULong                   sBeforeLimit;
    UInt                    sUnitedVarColumnCount = 0;
    const smiColumn       * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    smiValue                sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];

    SM_MAX_SCN( &sBeforeLimit );

    IDE_ERROR( aTrans  != NULL );
    IDE_ERROR( aOldRow != NULL );
    IDE_ERROR( aHeader != NULL );

    IDU_FIT_POINT( "1.PROJ-2118@svcRecord::updateVersion" );

    /* BUGBUG: By Newdaily
       aColumnList�� aValueList�� NULL�� ��찡 ������.
       �̿� ���� ������ �ʿ��մϴ�.
    */

    /* !!Fixd Row�� Update�� ������ New Version�� ��������� */

    sOldSlotHeader = (smpSlotHeader*)aOldRow;
    sPageID     = SMP_SLOT_GET_PID(aOldRow);
    sTargetOID  = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( sOldSlotHeader ) );

    /* update�ϱ� ���Ͽ� old version�� ���� page�� ���Ͽ�
       holdPageXLatch  */
    IDE_TEST( svmManager::holdPageXLatch( aHeader->mSpaceID, sPageID )
              != IDE_SUCCESS );
    sState = 1;

    /* aOldRow�� �ٸ� Transaction�� �̹� Update�ߴ��� ����.*/
    IDE_TEST( recordLockValidation( aTrans,
                                    aViewSCN,
                                    aHeader->mSpaceID,
                                    &aOldRow,
                                    ID_ULONG_MAX,
                                    &sState,
                                    aRetryInfo,
                                    aForbiddenToRetry )
              != IDE_SUCCESS );

    // PROJ-1784 DML without retry
    // sOldFixRowPtr�� ���� �� �� �����Ƿ� ���� �ٽ� �����ؾ� ��
    // ( �̰�� page latch �� �ٽ� ����� )
    sOldSlotHeader = (smpSlotHeader*)aOldRow;
    sPageID     = SMP_SLOT_GET_PID(aOldRow);
    sTargetOID  = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( sOldSlotHeader ) );

    /* New Version�� �Ҵ��Ѵ�. */
    IDE_TEST( svpFixedPageList::allocSlot(aTrans,
                                          aHeader->mSpaceID,
                                          NULL,
                                          aHeader->mSelfOID,
                                          &(aHeader->mFixed.mVRDB),
                                          &sNewFixRowPtr,
                                          aInfinite,
                                          aHeader->mMaxRow,
                                          SMP_ALLOC_FIXEDSLOT_NONE)
              != IDE_SUCCESS );

    sNewFixRowPID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sFixOID       = SM_MAKE_OID( sNewFixRowPID,
                                 SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * TableOID�� Get�Ѵ�.*/
    IDE_ASSERT( smmManager::getPersPagePtr( aHeader->mSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    IDE_ASSERT( sPagePtr->mTableOID == aHeader->mSelfOID );

    /* Tx OID List�� Old Version �߰�. */
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       sTargetOID,
                                       aHeader->mSpaceID,
                                       SM_OID_OLD_UPDATE_FIXED_SLOT )
              != IDE_SUCCESS );

    /* PROJ-2174 Supporting LOB in the volatile tablespace
     * LOB CURSOR�� ���� ȣ��� ��� aColumnList�� aValueList��
     * �� �� NULL�̴�.
     * ���� �� ���� NULL�̸� LOB CURSOR�� ���� ȣ��� �Ǵ��ϰ�,
     * �ƴҰ�� TABLE CURSOR�� ���� ȣ��� �Ǵ��Ѵ�. */
    if ( (aColumnList == NULL) && (aValueList == NULL) )
    {
        /* LOB Cursor�� Update�� Cursor Close�ÿ� Row�� Insert���� �ʰ�
         * ������Ʈ�Ŀ� �ٷ� ������Ʈ�Ѵ�. ������ Cursor Close�ÿ� ��
         * Insert�� �����ʵ��� SM_OID_ACT_CURSOR_INDEX�� Clear�ؼ�
         * OID List�� �߰��Ѵ�.
         *
         * BUG-33127 [SM] when lob update operation is rolled back, index keys
         *           does not be deleted.
         *
         * lob update �� rollback �Ǵ� ��� SM_OID_ACT_CURSOR_INDEX �÷��׸�
         * �����ϱ� ������ Cursor close�� �ε����� Ű�� �������� �ʽ��ϴ�.
         * �׸��� rollback�� aging���� SM_OID_ACT_CURSOR_INDEX�÷��׷� ����
         * ���� �ʰ� �˴ϴ�.
         * ������ lob�� ��� �ε��� Ű�� �ٷ� �����ϱ� ������ rollback�� �߻��ϸ�
         * �ε��� Ű�� ������ �־�� �մϴ�.
         * ���� SM_OID_ACT_AGING_INDEX �÷��׸� �߰��Ͽ� rollback�� �ε�������
         * Ű�� �����ϵ��� �����մϴ�. */
        
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sFixOID,
                                           aHeader->mSpaceID,
                                           ( SM_OID_NEW_UPDATE_FIXED_SLOT & ~SM_OID_ACT_CURSOR_INDEX )
                                             | SM_OID_ACT_AGING_INDEX )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sFixOID,
                                           aHeader->mSpaceID,
                                           SM_OID_NEW_UPDATE_FIXED_SLOT )
                  != IDE_SUCCESS );
    }

    /* aOldRow�� Next Version�� New Version���� Setting.*/
    /* sPageID�� ���� latch�� Ǯ�� ���� �ݵ�� �ٲ�� �Ѵ�. */
    SMP_SLOT_SET_NEXT_OID( sOldSlotHeader, sFixOID );
    /* aOldRow->mNext�� �ٲٱ� ���� ���� �����س��´�. �̴� ���߿�
       �α��� �� ��ϵȴ�. */
    SM_SET_SCN( &sBeforeLimit, &(sOldSlotHeader->mLimitSCN ) );
    SM_SET_SCN( &(sOldSlotHeader->mLimitSCN), &aInfinite );
    sIsHeadNextChanged = ID_TRUE;

    sState = 0;
    IDE_TEST( svmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
              != IDE_SUCCESS );

    /* �� ������ ������ �Ǹ� Row�� Lock������ �ȴ�. �ֳĸ� Next Version��
       ������ ���� Transaction�� ���� New Version�� �ְ� �Ǹ� �ٸ� Transaction����
       Wait�ϰ� �Ǳ⶧���̴�. �׷��� ������ Page Latch ���� Ǯ����.*/

    sFixedRowSize = aHeader->mFixed.mVRDB.mSlotSize - SMP_SLOT_HEADER_SIZE;

    idlOS::memcpy( sNewFixRowPtr + SMP_SLOT_HEADER_SIZE,
                   aOldRow + SMP_SLOT_HEADER_SIZE,
                   sFixedRowSize);

    /* update�Ǵ� ���� ���� */
    sCurColumnList = aColumnList;
    sCurValue      = aValueList;

    /* Fixed Row�� ������ Variable Column�� ����Ǵ� Var Column��
       Update�Ǵ� Column�� New Version�� �����.*/
    for( sColumnSeq = 0 ;
         sCurColumnList != NULL;
         sCurColumnList = sCurColumnList->next, sCurValue++, sColumnSeq++ )
    {
        sColumn = sCurColumnList->column;

        if( (sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                sColumnSeq,
                sColumn->size,
                sColumn->offset,
                sCurValue->length );
        }
        else
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                sColumnSeq,
                sColumn->size,
                sColumn->offset,
                sCurValue->length );
        }

        IDU_FIT_POINT( "2.PROJ-2118@svcRecord::updateVersion" );

        switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SMI_COLUMN_TYPE_LOB:

                sCurLobDesc = (smcLobDesc*)(sNewFixRowPtr + sColumn->offset);

                /* SQL�� ���� Lob �����͸� ���� ��� */
                IDE_TEST( svcLob::reserveSpaceInternal(
                                      aTrans,
                                      (smcTableHeader *)aHeader,
                                      (smiColumn*)sColumn,
                                      0, /* aLobVersion */
                                      sCurLobDesc,
                                      0,
                                      sCurValue->length,
                                      SM_FLAG_INSERT_LOB,
                                      ID_TRUE /* aIsFullWrite */)
                          != IDE_SUCCESS );

                if( sCurValue->length > 0 )
                {
                    IDE_TEST( svcLob::writeInternal( (UChar*)sNewFixRowPtr,
                                                     (smiColumn*)sColumn,
                                                     0,
                                                     sCurValue->length,
                                                     (UChar*)sCurValue->value )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Here lenght is always 0. */
                    
                    if( sCurValue->value == NULL )
                    {
                        /* null */
                        sCurLobDesc->flag =
                            SM_VCDESC_MODE_IN | SM_VCDESC_NULL_LOB_TRUE;
                    }
                    else
                    {
                        /* empty (nothing to do) */
                    }
                }

                sCurLobDesc->length = sCurValue->length;

                break;

            case SMI_COLUMN_TYPE_VARIABLE:
                sUnitedVarColumns[sUnitedVarColumnCount]        =   sColumn;
                sUnitedVarValues[sUnitedVarColumnCount].length  =   sCurValue->length;
                sUnitedVarValues[sUnitedVarColumnCount].value   =   sCurValue->value;
                sUnitedVarColumnCount++;

                break;

            case SMI_COLUMN_TYPE_FIXED:

                IDU_FIT_POINT( "3.PROJ-2118@svcRecord::updateVersion" );

                // BUG-30104 Fixed Column�� length�� 0�� �� �� �����ϴ�.
                IDE_ERROR_MSG( sCurValue->length > 0,
                               "Table OID  : %"ID_vULONG_FMT"\n"
           "Space ID   : %"ID_UINT32_FMT"\n"
                               "Column Seq : %"ID_UINT32_FMT"\n",
                               aHeader->mSelfOID,
                               aHeader->mSpaceID,
                               sColumnSeq );

                idlOS::memcpy( sNewFixRowPtr + sColumn->offset,
                               sCurValue->value,
                               sCurValue->length );

                break;

            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            default:
                ideLog::log( IDE_ERR_0,
                             "Invalid column[%u] "
                             "flag : %u "
                             "( TableOID : %lu )\n",
                             sColumnSeq,
                             sColumn->flag,
                             aHeader->mSelfOID );

                IDE_ASSERT( 0 );
                break;
        }
    }

    if ( sUnitedVarColumnCount > 0)
    {
        /* ��� united var column �� ������Ʈ�Ѵٸ�, list �籸���� ��ĥ �ʿ䰡 ���� */
        if ( aHeader->mUnitedVarColumnCount == sUnitedVarColumnCount )
        {
            IDE_TEST( insertUnitedVarColumns ( aTrans,
                                               aHeader,
                                               sNewFixRowPtr,
                                               sUnitedVarColumns,
                                               ID_TRUE,
                                               sUnitedVarValues,
                                               sUnitedVarColumnCount )
                    != IDE_SUCCESS );

            IDE_TEST( deleteVC( aTrans,
                                aHeader,
                                ((smpSlotHeader*)aOldRow)->mVarOID ) );

        }
        else
        {
            IDE_TEST( svcRecord::updateUnitedVarColumns( aTrans,
                                                         aHeader,
                                                         aOldRow,
                                                         sNewFixRowPtr,
                                                         sUnitedVarColumns,
                                                         sUnitedVarValues,
                                                         sUnitedVarColumnCount )
                    != IDE_SUCCESS );
        }
    }
    else /* united var col update�� �߻����� ���� ��� */
    {
        sFstVCPieceOID = ((smpSlotHeader*)aOldRow)->mVarOID;
        ((smpSlotHeader*)sNewFixRowPtr)->mVarOID = sFstVCPieceOID;
    }
    /* undo log�� ����Ѵ�. */
    /* aOldRow->mNext�� �ٲٱ� ���� �α��ؾ� ���� ���� �� �� �ִ�. */
    IDE_TEST( svcRecordUndo::logUpdate( smLayerCallback::getLogEnv( aTrans ),
                                        aTrans,
                                        aHeader->mSelfOID,
                                        aHeader->mSpaceID,
                                        aOldRow,
                                        sNewFixRowPtr,
                                        sBeforeLimit )
              != IDE_SUCCESS );

    /* BUG-14513: Fixed Slot�� �Ҵ�� Alloc Slot Log�� �������
       �ʵ��� ����. Update log�� Alloc Slot�� ���� Redo, Undo����.
       �� ������ Update log��� �� Slot header�� ���� Update����
    */
    svpFixedPageList::setAllocatedSlot( aInfinite, sNewFixRowPtr );

    *aRow = (SChar*)sNewFixRowPtr;
    if(aRetSlotGRID != NULL)
    {
        SC_MAKE_GRID( *aRetSlotGRID,
                      aHeader->mSpaceID,
                      sPageID,
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INVALID_COLUMN_SIZE) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if( ideGetErrorCode() == smERR_RETRY_Row_Retry )
    {
        IDE_ASSERT( lockRowInternal( aTrans,
                                     aHeader,
                                     aOldRow ) == IDE_SUCCESS );
        *aRow = aOldRow;
    }

    if( sIsHeadNextChanged == ID_TRUE )
    {
        if( sState == 0 )
        {
            IDE_ASSERT( svmManager::holdPageXLatch( aHeader->mSpaceID,
                                                    sPageID )
                        == IDE_SUCCESS);
            sState = 1;
        }

        SM_SET_SCN( &(sOldSlotHeader->mLimitSCN), &sBeforeLimit );
        SMP_SLOT_INIT_POSITION( sOldSlotHeader );
        SMP_SLOT_SET_USED( sOldSlotHeader );
    }

    if( sState != 0 )
    {
        // PROJ-1784 DML without retry
        // aOldRow�� ���� �� �� �����Ƿ� PID�� �ٽ� �����;� ��
        sPageID = SMP_SLOT_GET_PID( aOldRow );
        IDE_ASSERT(svmManager::releasePageLatch(aHeader->mSpaceID,
                                                sPageID)
                   == IDE_SUCCESS);
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aOldRow�� Update�Ѵ�. MVCC�ƴ϶� Inplace�� Update�Ѵ�.
 *
 * aTrans           - [IN] Transaction Pointer
 * aHeader          - [IN] Table Header
 * aOldRow          - [IN] ���� value �� ���� old row
 * aNewRow          - [IN] �� value �� ������ new row
 * aColumns         - [IN] VC Column Description
 * aAddOIDFlag      - [IN] Variable Column�� �����ϴ� VC�� Transaction OID List��
 *                         Add�� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE.
 * aValues          - [IN] Variable Column�� Values
 * aVarcolumnCount  - [IN] Variable Column�� Value�� ����
 ***********************************************************************/
IDE_RC svcRecord::updateUnitedVarColumns( void                * aTrans,
                                          smcTableHeader      * aHeader,
                                          SChar               * aOldRow,
                                          SChar               * aNewRow,
                                          const smiColumn    ** aColumns,
                                          smiValue            * aValues,
                                          UInt                  aVarColumnCount )
{
    const smiColumn   * sCurColumn = NULL;
    const smiColumn   * sColumns[SMI_COLUMN_ID_MAXIMUM];

    smiValue            sValues[SMI_COLUMN_ID_MAXIMUM];

    UInt                sUnitedVarColCnt    = 0;
    UInt                sUpdateColIdx       = 0;
    UInt                sOldValueCnt        = 0;
    UInt                i                   = 0;
    UInt                sSumOfColCnt        = 0;
    UInt                sOffsetIdxInPiece   = 0;
    UShort            * sCurrOffsetPtr      = NULL;
    smOID               sOID                = SM_NULL_OID;

    smVCPieceHeader   * sVCPieceHeader      = NULL;

    IDE_ASSERT( aTrans      != NULL );
    IDE_ASSERT( aHeader     != NULL );
    IDE_ASSERT( aOldRow     != NULL );
    IDE_ASSERT( aNewRow     != NULL );
    IDE_ASSERT( aColumns    != NULL );


    sOID = ((smpSlotHeader*)aOldRow)->mVarOID;
    
    if ( sOID != SM_NULL_OID )
    {
        IDE_ASSERT( smmManager::getOIDPtr( aColumns[0]->colSpace,
                                           sOID,
                                           (void**)&sVCPieceHeader)
                    == IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    for ( i = 0 ; i < aHeader->mColumnCount ; i++) /* ������ ���� �� value list ���� */
    {
        sCurColumn = smcTable::getColumn( aHeader, i); // Column Idx

        if ( (sCurColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE )
        {
            sColumns[sUnitedVarColCnt]  = sCurColumn;

            /* update �� �÷��� �� value ī�� */
            if ( ( sUpdateColIdx < aVarColumnCount )
                    && ( sCurColumn->id == aColumns[sUpdateColIdx]->id ))
            {
                sValues[sUnitedVarColCnt].length = aValues[sUpdateColIdx].length;
                sValues[sUnitedVarColCnt].value = aValues[sUpdateColIdx].value;
                sUpdateColIdx++;
            }
            else /* �ٲ��� ���� �÷��� old value ī�� */
            {
                sOldValueCnt++;

                sOffsetIdxInPiece = sCurColumn->varOrder - sSumOfColCnt;

                while ( ( sVCPieceHeader != NULL ) 
                        && ( sOffsetIdxInPiece >= sVCPieceHeader->colCount ))
                {
                    if ( sVCPieceHeader->nxtPieceOID != SM_NULL_OID )
                    {
                        sOffsetIdxInPiece   -= sVCPieceHeader->colCount;
                        sSumOfColCnt        += sVCPieceHeader->colCount;
                        sOID                = sVCPieceHeader->nxtPieceOID;

                        IDE_ASSERT( smmManager::getOIDPtr( sCurColumn->colSpace,
                                                           sOID,
                                                           (void**)&sVCPieceHeader)
                                == IDE_SUCCESS );
                    }
                    else
                    {
                        sVCPieceHeader = NULL;
                        break;
                    }
                }

                if ( ( sVCPieceHeader != NULL )
                        && ( sOffsetIdxInPiece < sVCPieceHeader->colCount ) )
                {
                    sCurrOffsetPtr = (UShort*)(sVCPieceHeader + 1) + sOffsetIdxInPiece;

                    sValues[sUnitedVarColCnt].length    = ( *( sCurrOffsetPtr + 1 )) - ( *sCurrOffsetPtr );
                    sValues[sUnitedVarColCnt].value     = (SChar*)sVCPieceHeader + (*sCurrOffsetPtr);
                }
                else
                {
                    sValues[sUnitedVarColCnt].length    = 0;
                    sValues[sUnitedVarColCnt].value     = NULL;
                }
            }

            sUnitedVarColCnt++;
        }
        else
        {
            /* do nothing */
        }
    }

    IDE_ERROR( sUpdateColIdx + sOldValueCnt == sUnitedVarColCnt );
    IDE_TEST( insertUnitedVarColumns( aTrans,
                                      aHeader,
                                      aNewRow,
                                      sColumns,
                                      ID_TRUE,
                                      sValues,
                                      sUnitedVarColCnt )
            != IDE_SUCCESS );

    IDE_TEST( deleteVC( aTrans,
                        aHeader,
                        ((smpSlotHeader*)aOldRow)->mVarOID ) 
            != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : aOldRow�� Update�Ѵ�. MVCC�ƴ϶� Inplace�� Update�Ѵ�.
 *
 * aStatistics   - [IN] None
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aHeader       - [IN] aOldRow�� ���� �ִ� Table Header Pointer
 * aOldRow       - [IN] Update�� Row
 * aOldGRID      - [IN] None
 * aRow          - [OUT] aOldRow�� ���� ���� �Ѱ��ش�.
 * aRetSlotGRID  - [IN] None
 * aColumnList   - [IN] Update�� Column List
 * aValueList    - [IN] Update�� Column���� ���ο� Vaule List
 * aRetryInfo    - [IN] None
 * aInfinite     - [IN] None
 * aLogInfo4Update-[IN] None
 * aOldRecImage  - [IN] None
 * aModifyIdxBit - [IN] Update�� Index List
 * aForbiddenToRetry - [IN] None
 ***********************************************************************/
IDE_RC svcRecord::updateInplace(idvSQL                * /*aStatistics*/,
                                void                  * aTrans,
                                smSCN                   /*aViewSCN*/,
                                void                  * /*aTableInfoPtr*/,
                                smcTableHeader        * aHeader,
                                SChar                 * aRowPtr,
                                scGRID                  /*aOldGRID*/,
                                SChar                ** aRow,
                                scGRID                * aRetSlotGRID,
                                const smiColumnList   * aColumnList,
                                const smiValue        * aValueList,
                                const smiDMLRetryInfo * /*aRetryInfo*/,
                                smSCN                   /*aInfinite*/,
                                void                  * /*aLobInfo4Update*/,
                                ULong                   aModifyIdxBit,
                                idBool                  /*aForbiddenToRetry*/ )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;
    const smiValue        * sCurValue;
    smOID                   sFixedRowID;
    scPageID                sPageID = SM_NULL_PID;
    UInt                    sColumnSeq;
    smcLobDesc            * sCurLobDesc;
    UInt                    sUnitedVarColumnCount = 0;
    const smiColumn       * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    smiValue                sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];

    IDE_ERROR( aTrans      != NULL );
    IDE_ERROR( aHeader     != NULL );
    IDE_ERROR( aRowPtr     != NULL );
    IDE_ERROR( aRow        != NULL );
    IDE_ERROR( aColumnList != NULL );
    IDE_ERROR( aValueList  != NULL );

    /* inplace update�� ���� undo �α׸� ���� ����Ѵ�.
       �ֳ��ϸ� update�� ���� row�� ���� ���� �Ͼ�� ������
       �����Ͱ� ����Ǳ� ���� �α��� ���� �̷������ �Ѵ�. */
    IDE_TEST( svcRecordUndo::logUpdateInplace( smLayerCallback::getLogEnv( aTrans ),
                                               aTrans,
                                               aHeader->mSpaceID,
                                               aHeader->mSelfOID,
                                               aRowPtr,
                                               aColumnList,
                                               aModifyIdxBit )
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID(aRowPtr);

    if ( aModifyIdxBit != 0 )
    {
        /* Table�� Index���� aRowPtr�� �����. */
        IDE_TEST( smLayerCallback::deleteRowFromIndex( aRowPtr,
                                                       aHeader,
                                                       aModifyIdxBit )
                  != IDE_SUCCESS );
    }

    /* Fixed Row�� Row ID�� �����´�. */
    sFixedRowID = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowPtr ) );

    sCurColumnList = aColumnList;
    sCurValue      = aValueList;

    /* Update�� VC( Variable Column ) �� ���ο� VC�� �����.
       VC�� Update�� ���ο� Row�� ����� ������ ����ȴ�. */
    for( sColumnSeq = 0 ;
         sCurColumnList != NULL;
         sCurColumnList  = sCurColumnList->next, sCurValue++, sColumnSeq++ )
    {
        sColumn = sCurColumnList->column;

        if( (sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                sColumnSeq,
                sColumn->size,
                sColumn->offset,
                sCurValue->length );
        }
        else
        {
            IDE_ERROR_RAISE_MSG(
                ( (sCurValue->length <= sColumn->size) &&
                  ( ((sCurValue->length  > 0) && (sCurValue->value != NULL)) ||
                    ((sCurValue->length == 0) && (sCurValue->value == NULL)) ) ),
                err_invalid_column_size,
                "Table OID     : %"ID_vULONG_FMT"\n"
                "Column Seq    : %"ID_UINT32_FMT"\n"
                "Column Size   : %"ID_UINT32_FMT"\n"
                "Column Offset : %"ID_UINT32_FMT"\n"
                "Value Length  : %"ID_UINT32_FMT"\n",
                aHeader->mSelfOID,
                sColumnSeq,
                sColumn->size,
                sColumn->offset,
                sCurValue->length );
        }

        switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SMI_COLUMN_TYPE_LOB:

                sCurLobDesc = (smcLobDesc*)(aRowPtr + sColumn->offset);

                /* smcLob::prepare4WriteInternal���� sCurLobDesc�� �ٲܼ�
                 * �ֱ� ������ �����͸� �����ؼ� �Ѱܾ� �Ѵ�.
                 * �ֳ��ϸ� UpdateInplace�� �ϱ⶧���� �����Ϳ����� ����
                 * ������ �ϱ����� �α׸� ���� ����ؾ� �ϳ� ����� �α׸�
                 * ������� �ʰ� ���Ŀ� ��ϵǱ� �����̴�.
                 */

                /* SQL�� ���� Lob �����͸� ���� ��� */
                IDE_TEST( svcLob::reserveSpaceInternal(
                                      aTrans,
                                      (smcTableHeader *)aHeader,
                                      (smiColumn*)sColumn,
                                      0, /* aLobVersion */
                                      sCurLobDesc,
                                      0,
                                      sCurValue->length,
                                      SM_FLAG_INSERT_LOB,
                                      ID_TRUE /* aIsFullWrite */)
                          != IDE_SUCCESS );

                if( sCurValue->length > 0 )
                {
                    IDE_TEST( svcLob::writeInternal( (UChar*)aRowPtr,
                                                     (smiColumn*)sColumn,
                                                     0,
                                                     sCurValue->length,
                                                     (UChar*)sCurValue->value )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Here length is always 0. */
                    
                    if( sCurValue->value == NULL )
                    {
                        /* null */
                        sCurLobDesc->flag
                            = SM_VCDESC_MODE_IN | SM_VCDESC_NULL_LOB_TRUE;
                    }
                    else
                    {
                        /* empty (nothing to do) */
                    }
                }

                sCurLobDesc->length = sCurValue->length;

                break;

            case SMI_COLUMN_TYPE_VARIABLE:
                sUnitedVarColumns[sUnitedVarColumnCount]        =   sColumn;
                sUnitedVarValues[sUnitedVarColumnCount].length  =   sCurValue->length;
                sUnitedVarValues[sUnitedVarColumnCount].value   =   sCurValue->value;
                sUnitedVarColumnCount++;

                break;

            case SMI_COLUMN_TYPE_FIXED:

                // BUG-30104 Fixed Column�� length�� 0�� �� �� �����ϴ�.
                IDE_ERROR_MSG( sCurValue->length > 0,
                               "Table OID  : %"ID_vULONG_FMT"\n"
                               "Space ID   : %"ID_UINT32_FMT"\n"
                               "Column Seq : %"ID_UINT32_FMT"\n",
                               aHeader->mSelfOID,
                               aHeader->mSpaceID,
                               sColumnSeq );

                /* BUG-42407
                 * Fixed column�� ��� �׻� �÷� ���̰� ����.
                 * source �� destination �� ������ memcpy�� skip �Ѵ�. */
                if ( (aRowPtr + sColumn->offset) != (sCurValue->value) )
                {    
                    idlOS::memcpy(aRowPtr + sColumn->offset,
                                  sCurValue->value,
                                  sCurValue->length);
                }    
                else 
                {    
                    /* do nothing */
                }    

                break;

            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            default:
                ideLog::log( IDE_ERR_0,
                             "Invalid column[%u] "
                             "flag : %u "
                             "( TableOID : %lu )\n",
                             sColumnSeq,
                             sColumn->flag,
                             aHeader->mSelfOID );
                IDE_ASSERT(0);
                break;
        }
    }

    if ( sUnitedVarColumnCount > 0 )
    {
        IDE_TEST( svcRecord::updateUnitedVarColumns( aTrans,
                                                     aHeader,
                                                     aRowPtr,
                                                     aRowPtr,
                                                     sUnitedVarColumns,
                                                     sUnitedVarValues,
                                                     sUnitedVarColumnCount )
                != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    *aRow = (SChar*)aRowPtr;
    if(aRetSlotGRID != NULL)
    {
        SC_MAKE_GRID( *aRetSlotGRID,
                      aHeader->mSpaceID,
                      sPageID,
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowPtr ) );
    }

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       sFixedRowID,
                                       aHeader->mSpaceID,
                                       SM_OID_UPDATE_FIXED_SLOT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INVALID_COLUMN_SIZE) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * task-2399 Lock Row�� Delete Row�� ������.
 * ���� ���� ����� ������ �˰� ������ smcRecord::removeVersion�� ���� �ּ��� ����
 ***********************************************************************/
/***********************************************************************
 * Description : aRow�� ����Ű�� row�� �����Ѵ�.
 *
 * aStatistics    - [IN] None
 * aTrans         - [IN] Transaction Pointer
 * aViewSCN       - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aTableInfoPtr  - [IN] Table Info Pointer(Table�� Transaction�� Insert,
 *                        Delete�� Record Count�� ���� ������ ������ �ִ�.

 * aHeader      - [IN] Table Header Pointer
 * aRow         - [IN] ������ Row
 * aRetSlotRID  - [IN] None
 * aInfinite    - [IN] Cursor�� Infinite SCN
 * aIterator    - [IN] None
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC svcRecord::removeVersion( idvSQL               * /*aStatistics*/,
                                 void                 * aTrans,
                                 smSCN                  aViewSCN,
                                 void                 * aTableInfoPtr,
                                 smcTableHeader       * aHeader,
                                 SChar                * aRow,
                                 scGRID                 /*aSlotRID*/,
                                 smSCN                  aInfinite,
                                 const smiDMLRetryInfo* aRetryInfo,
                                 idBool                 aIsDequeue, 
                                 idBool                 aForbiddenToRetry )
{
    const smiColumn * sColumn;
    UInt              sLockState = 0;
    scPageID          sPageID;
    UInt              i;
    smOID             sRemoveOID;
    UInt              sColumnCnt;
    smpSlotHeader   * sSlotHeader = (smpSlotHeader*)aRow;
    smSCN             sDeleteSCN;
    ULong             sNxtSCN;
    smTID             sNxtTID;

    IDE_ERROR( aRow    != NULL );
    IDE_ERROR( aTrans  != NULL );
    IDE_ERROR( aHeader != NULL );

    IDU_FIT_POINT( "1.PROJ-2118@svcRecord::removeVersion" );

    sPageID     = SMP_SLOT_GET_PID(aRow);
    sRemoveOID  = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( sSlotHeader ) );
    SMX_GET_SCN_AND_TID( sSlotHeader->mLimitSCN, sNxtSCN, sNxtTID );

    if (( SM_SCN_IS_NOT_LOCK_ROW( sNxtSCN ) ) ||
        ( smxTrans::getTransID( aTrans ) != sNxtTID ))
    {
        /*  remove�ϱ� ���Ͽ� �� row�� ���� page�� ���Ͽ� holdPageXLatch */
        IDE_TEST( svmManager::holdPageXLatch( aHeader->mSpaceID,
                                              sPageID ) != IDE_SUCCESS );
        sLockState = 1;

        IDE_TEST( recordLockValidation(aTrans,
                                       aViewSCN,
                                       aHeader->mSpaceID,
                                       &aRow,
                                       ID_ULONG_MAX,
                                       &sLockState,
                                       aRetryInfo,
                                       aForbiddenToRetry )
                  != IDE_SUCCESS );

        // PROJ-1784 DML without retry
        // sOldFixRowPtr�� ���� �� �� �����Ƿ� ���� �ٽ� �����ؾ� ��
        // ( �̰�� page latch �� �ٽ� ����� )
        sSlotHeader = (smpSlotHeader*)aRow;
        sPageID     = SMP_SLOT_GET_PID(aRow);
        sRemoveOID  = SM_MAKE_OID( sPageID,
                                   SMP_SLOT_GET_OFFSET( sSlotHeader ) );
    }

    //remove ������ ����� �� �ִ� row�� ������ next������ ���ų�,
    //next�� �����Ѵٸ�, �װ��� lock�� ���� �����ϴ�.
    if( !( SM_SCN_IS_FREE_ROW( sSlotHeader->mLimitSCN ) ||
           SM_SCN_IS_LOCK_ROW( sSlotHeader->mLimitSCN ) ) )
    {
        ideLog::log( IDE_ERR_0,
                     "Table OID : %lu\n"
                     "Space ID  : %u\n"
                     "Page ID   : %u\n",
                     aHeader->mSelfOID,
                     aHeader->mSpaceID,
                     sPageID );

        ideLog::logMem( IDE_ERR_0,
                        (UChar*)sSlotHeader,
                        ID_SIZEOF( smpSlotHeader ),
                        "Slot Header" );

        svpFixedPageList::dumpSlotHeader( sSlotHeader );

        IDE_ASSERT( 0 );
    }

    //slotHeader�� mNext�� �� ���� �����Ѵ�. delete������ ����Ǹ�
    //�� ������ �����ϴ� Ʈ������� aInfinite������ ���õȴ�. (with deletebit)
    SM_SET_SCN( &sDeleteSCN, &aInfinite );
    SM_SET_SCN_DELETE_BIT( &sDeleteSCN );

    if ( aIsDequeue == ID_FALSE )
    {
        SM_SET_SCN( &sNxtSCN,  &( sSlotHeader->mLimitSCN ) );
    }
    else
    {
        SM_SET_SCN_FREE_ROW( &sNxtSCN );
    }

    /* delete�� ���� �α׸� ����Ѵ�. */
    IDE_TEST( svcRecordUndo::logDelete( smLayerCallback::getLogEnv( aTrans ),
                                        aTrans,
                                        aHeader->mSelfOID,
                                        aRow,
                                        sNxtSCN )
              != IDE_SUCCESS );

    IDL_MEM_BARRIER;

    SM_SET_SCN( &(sSlotHeader->mLimitSCN), &sDeleteSCN );

    if( sLockState == 1 )
    {
        IDE_TEST( svmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
                  != IDE_SUCCESS );
        sLockState = 0;
    }
    sColumnCnt = aHeader->mColumnCount;

    for(i=0 ; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn(aHeader, i);

        switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SMI_COLUMN_TYPE_LOB:
                /* LOB�� ���� Piece���� Delete Flag�� Delete�Ǿ��ٰ�
                 * �����Ѵ�.*/
                IDE_TEST( deleteLob( aTrans, (smcTableHeader *)aHeader, sColumn, aRow)
                          != IDE_SUCCESS );

                break;

            case SMI_COLUMN_TYPE_VARIABLE:
                /* United VC �� �ؿ��� �ѹ��� �� ����� */
                break;

            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                /* volitile �� geometry �� �������� �����Ƿ� large var�� ���ü� ���� */
                IDE_ASSERT( 0 );
                break;

            case SMI_COLUMN_TYPE_FIXED:
                break;

            default:
                break;
        }
    }

    IDE_TEST( deleteVC( aTrans, 
                        aHeader, 
                        ((smpSlotHeader*)aRow)->mVarOID ) 
            != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       sRemoveOID,
                                       aHeader->mSpaceID,
                                       SM_OID_DELETE_FIXED_SLOT)
              != IDE_SUCCESS );

    if (aTableInfoPtr != NULL)
    {
        smLayerCallback::decRecCntOfTableInfo( aTableInfoPtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sLockState != 0 )
    {
        IDE_PUSH();
        // PROJ-1784 DML without retry
        // aRow�� ���� �� �� �����Ƿ� PID�� �ٽ� �����;� ��
        sPageID = SMP_SLOT_GET_PID(aRow);
        IDE_ASSERT(svmManager::releasePageLatch(aHeader->mSpaceID,
                                                sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/*******************************************************************o****
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::nextOIDall( smcTableHeader * aHeader,
                              SChar          * aCurRow,
                              SChar         ** aNxtRow )
{

    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aNxtRow != NULL );

    return svpAllocPageList::nextOIDall( aHeader->mSpaceID,
                                         aHeader->mFixedAllocList.mVRDB,
                                         aCurRow,
                                         aHeader->mFixed.mVRDB.mSlotSize,
                                         aNxtRow );
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::freeVarRowPending( void            * aTrans,
                                     smcTableHeader  * aHeader,
                                     smOID             aPieceOID,
                                     SChar           * aRow )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(svpVarPageList::addFreeSlotPending(
                     aTrans,
                     aHeader->mSpaceID,
                     aHeader->mVar.mVRDB,
                     aPieceOID,
                     aRow)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::freeFixRowPending( void            * aTrans,
                                     smcTableHeader  * aHeader,
                                     SChar           * aRow )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(svpFixedPageList::addFreeSlotPending(
                     aTrans,
                     aHeader->mSpaceID,
                     &(aHeader->mFixed.mVRDB),
                     aRow)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::setFreeVarRowPending( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        smOID             aPieceOID,
                                        SChar           * aRow,
                                        smSCN             aSCN )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(svpVarPageList::freeSlot(aTrans,
                                          aHeader->mSpaceID,
                                          aHeader->mVar.mVRDB,
                                          aPieceOID,
                                          aRow,
                                          SMP_TABLE_NORMAL,
                                          aSCN )
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::setFreeFixRowPending( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        SChar           * aRow,
                                        smSCN             aSCN )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(svpFixedPageList::freeSlot(aTrans,
                                            aHeader->mSpaceID,
                                            &(aHeader->mFixed.mVRDB),
                                            aRow,
                                            SMP_TABLE_NORMAL,
                                            aSCN )
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::setSCN( SChar   *  aRow,
                          smSCN      aSCN )
{
    smpSlotHeader *sSlotHeader;

    sSlotHeader = (smpSlotHeader*)aRow;

    SM_SET_SCN( &(sSlotHeader->mCreateSCN), &aSCN );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC svcRecord::setDeleteBitOnHeader( smpSlotHeader * aRowHeader )
{
    SM_SET_SCN_DELETE_BIT( &(aRowHeader->mCreateSCN) );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : aRowHeader�� SCN�� Delete Bit�� Setting�Ѵ�.
 *
 * aTrans     - [IN]: Transaction Pointer
 * aRowHeader - [IN]: Row Header Pointer
 ***********************************************************************/
IDE_RC svcRecord::setDeleteBit( scSpaceID          aSpaceID,
                                void             * aRowHeader )
{
    scPageID    sPageID;
    UInt        sState = 0;

    sPageID = SMP_SLOT_GET_PID(aRowHeader);

    IDE_TEST(svmManager::holdPageXLatch(aSpaceID, sPageID) != IDE_SUCCESS);
    sState = 1;

    /* BUG-14953 : PK�� �ΰ���. Rollback�� SCN����
       �׻� ���Ѵ� Bit�� Setting�Ǿ� �־�� �Ѵ�. �׷���
       ������ Index�� Unique Check�� �������Ͽ� Duplicate
       Key�� ����.*/
    SM_SET_SCN_DELETE_BIT( &(((smpSlotHeader*)aRowHeader)->mCreateSCN) );

    IDE_TEST(svmManager::releasePageLatch(aSpaceID, sPageID) != IDE_SUCCESS);
    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(svmManager::releasePageLatch(aSpaceID,
                                                sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}


/* PROJ-2174 Supporting LOB in the volatile tablespace */
 /***********************************************************************
 * Description : aRID�� ����Ű�� Variable Column Piece�� Flag����
 *               aFlag�� �ٲ۴�.
 *
 * aTrans       - [IN] Transaction Pointer
 * aTableOID    - [IN] Table OID
 * aSpaceID     - [IN] Variable Column Piece �� ���� Tablespace�� ID
 * aVCPieceOID  - [IN] Variable Column Piece OID
 * aVCPiecePtr  - [IN] Variabel Column Piece Ptr
 * aVCPieceFlag - [IN] Variabel Column Piece Flag
 ***********************************************************************/
IDE_RC svcRecord::setFreeFlagAtVCPieceHdr( void       * aVCPiecePtr,
                                           UShort       aVCPieceFreeFlag )
{
    smVCPieceHeader   * sVCPieceHeader;
    UShort              sVCPieceFlag;

    /* fix BUG-27221 : [codeSonar] Null Pointer Dereference */
    IDE_ASSERT( aVCPiecePtr != NULL );

    sVCPieceHeader = (smVCPieceHeader *)aVCPiecePtr;

    sVCPieceFlag = sVCPieceHeader->flag;
    sVCPieceFlag &= ~SM_VCPIECE_FREE_MASK;
    sVCPieceFlag |= aVCPieceFreeFlag;

    sVCPieceHeader->flag = sVCPieceFlag;

    return IDE_SUCCESS;
}

/***********************************************************************
 * task-2399 Lock Row�� Delete Row�� ������.
 * ���� ���� ����� ������ �˰� ������ smcRecord::lockRow�� ���� �ּ��� ����
 ***********************************************************************/
/***********************************************************************
 * Description : select for update�� �����Ҷ� �Ҹ��� �Լ�. ��, select for update
 *          �� ������ row�� ���ؼ� Lock������ ����ȴ�. �̶�, smpSlotHeader��
 *          mNext�� LockRow���� ǥ���Ѵ�.  �������� ���� Ʈ������� �������� ���ȸ�
 *          ��ȿ�ϴ�.
 ***********************************************************************/
IDE_RC svcRecord::lockRow(void           * aTrans,
                          smSCN            aViewSCN,
                          smcTableHeader * aHeader,
                          SChar          * aRow,
                          ULong            aLockWaitTime,
                          idBool           aForbiddenToRetry )
                            
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;
    UInt           sState = 0;
    smSCN          sRowSCN;
    smTID          sRowTID;

    sSlotHeader = (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID(sSlotHeader);

    IDE_TEST(svmManager::holdPageXLatch(aHeader->mSpaceID,
                                        sPageID) != IDE_SUCCESS);
    sState = 1;

    SMX_GET_SCN_AND_TID( sSlotHeader->mLimitSCN, sRowSCN, sRowTID );

    //�̹� Lock�� �ɷ� ���� ��쿣 �ٽ� lock�� ���� �ʴ´�.
    if ( ( SM_SCN_IS_NOT_LOCK_ROW( sRowSCN ) ) || 
         ( sRowTID != smLayerCallback::getTransID( aTrans ) ) )
    {
        IDE_TEST(recordLockValidation( aTrans,
                                       aViewSCN,
                                       aHeader->mSpaceID,
                                       &aRow,
                                       aLockWaitTime,
                                       &sState,
                                       NULL, // Without Retry Info
                                       aForbiddenToRetry )
                 != IDE_SUCCESS );

        // PROJ-1784 retry info�� �����Ƿ� ������� �ʴ´�.
        IDE_ASSERT( aRow == (SChar*)sSlotHeader );

        IDE_TEST( lockRowInternal( aTrans,
                                   aHeader,
                                   aRow ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST(svmManager::releasePageLatch(aHeader->mSpaceID,
                                          sPageID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(svmManager::releasePageLatch(aHeader->mSpaceID,
                                                sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : record loc�� ��´�.
 *               �̹� record lock validation�� �� ���¿���
 *               Lock row�� �ϴ� ��� ���.
 *
 *     aTrans   - [IN] Transaction Pointer
 *     aHeader  - [IN] Table header
 *     aRow     - [IN] lock�� ���� row
 ***********************************************************************/
IDE_RC svcRecord::lockRowInternal(void           * aTrans,
                                  smcTableHeader * aHeader,
                                  SChar          * aRow)
{
    smpSlotHeader *sSlotHeader = (smpSlotHeader*)aRow;
    smOID          sRecOID;
    smSCN          sRowSCN;
    smTID          sRowTID;
    ULong          sTransID;

    SMX_GET_SCN_AND_TID( sSlotHeader->mLimitSCN, sRowSCN, sRowTID );

    //�̹� �ڽ��� LOCK�� �ɾ��ٸ� �ٽ� LOCK�� �ɷ��� ���� �ʴ´�.
    if ( ( SM_SCN_IS_LOCK_ROW( sRowSCN ) ) &&
         ( sRowTID == smLayerCallback::getTransID( aTrans ) ) )
    {
        /* do nothing */
    }
    else
    {
        IDE_DASSERT( SM_SCN_IS_FREE_ROW(sSlotHeader->mLimitSCN) );
    
        /* mNext ���濡 ���� physical log ��� */
        IDE_TEST( svcRecordUndo::logPhysical8( smLayerCallback::getLogEnv( aTrans ),
                                               (SChar*)&sSlotHeader->mLimitSCN,
                                               ID_SIZEOF(ULong) )
                  != IDE_SUCCESS );

        sTransID = (ULong)smLayerCallback::getTransID( aTrans );
        SMP_SLOT_SET_LOCK( sSlotHeader, sTransID );

        sRecOID = SM_MAKE_OID( SMP_SLOT_GET_PID(aRow),
                               SMP_SLOT_GET_OFFSET( sSlotHeader ) );

        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sRecOID,
                                           aHeader->mSpaceID,
                                           SM_OID_LOCK_FIXED_SLOT )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : lockRow�� ����� row�� ���� lock�� �����Ҷ� ȣ��Ǵ� �Լ�
 ***********************************************************************/
IDE_RC svcRecord::unlockRow( void           * aTrans,
                             SChar          * aRow )
{
    smpSlotHeader *sSlotHeader;
    smTID          sRowTID;

    sSlotHeader = (smpSlotHeader*)aRow;

    sRowTID = SMP_GET_TID( sSlotHeader->mLimitSCN );

    if ( ( sRowTID == smLayerCallback::getTransID( aTrans ) ) &&
         ( SMP_SLOT_IS_LOCK_TRUE( sSlotHeader ) ) )
    {
        SMP_SLOT_SET_UNLOCK( sSlotHeader );
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : table�� nullrow�� ����
 *
 * memory table�� null row�� �����ϰ�, table header��
 * nullrow�� OID�� assign �Ѵ�.
 *
 * - 2nd code design
 *   + table Ÿ�Կ� ���� NullRow�� data page�� insert �� �α�
 *   + insert�� nullrow�� ���Ͽ� NULL_ROW_SCN ���� �� �α�
 *     : SMR_LT_UPDATE�� SMR_SVC_PERS_UPDATE_FIXED_ROW
 *   + table header�� nullrow�� ���� ���� �� �α�
 *     : SMR_LT_UPDATE�� SMR_SVC_TABLEHEADER_SET_NULLROW
 ***********************************************************************/
IDE_RC svcRecord::makeNullRow( void*           aTrans,
                               smcTableHeader* aHeader,
                               smSCN           aInfinite,
                               const smiValue* aNullRow,
                               UInt            aLoggingFlag,
                               smOID*          aNullRowOID )
{
    scPageID        sNullRowPID;
    smSCN           sNullRowSCN;
    smOID           sNullRowOID;
    smpSlotHeader*  sNullSlotHeaderPtr;
    smpSlotHeader   sAfterSlotHeader;

    SM_SET_SCN_NULL_ROW( &sNullRowSCN );

    /* memory table�� nullrow�� �����Ѵ�.*/
    IDE_TEST( insertVersion( NULL,
                             aTrans,
                             NULL,
                             aHeader,
                             aInfinite,
                             (SChar**)&sNullSlotHeaderPtr,
                             NULL,
                             aNullRow,
                             aLoggingFlag )
              != IDE_SUCCESS );
    sNullSlotHeaderPtr->mVarOID = SM_NULL_OID;

    idlOS::memcpy(&sAfterSlotHeader, sNullSlotHeaderPtr, SMP_SLOT_HEADER_SIZE);
    sAfterSlotHeader.mCreateSCN = sNullRowSCN;

    sNullRowPID = SMP_SLOT_GET_PID(sNullSlotHeaderPtr);
    sNullRowOID = SM_MAKE_OID( sNullRowPID,
                               SMP_SLOT_GET_OFFSET( sNullSlotHeaderPtr ) );

    /* insert �� nullrow�� scn�� �����Ѵ�. */
    IDE_TEST(setSCN((SChar*)sNullSlotHeaderPtr,
                    sNullRowSCN) != IDE_SUCCESS);

    *aNullRowOID = sNullRowOID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �������� Variable Column�� �ϳ��� variable slot �� Insert�Ѵ�.
 *               �׻� out-mode�� �����ϸ� vcDesc�� fixed row slot header�� �ִ�.
 *
 *               ����: VC Piece�� �� Header�� ���ؼ��� Alloc�� logging��
 *                    ������ VC Piece�� Value�� ����� �κп� ���ؼ��� Logging
 *                    ���� �ʴ´�. �ֳ��ϸ� Insert, Update�� �� �κп� ����
 *                    Logging�� �ϱ� �����̴�. ���⼭ Logging�� Update�Ŀ�
 *                    �ϴ��� ������ ���� �� ������ Update������ Before Image��
 *                    �ǹ̰� ���� �����̴�.
 *
 * aTrans           - [IN] Transaction Pointer
 * aHeader          - [IN] Table Header
 * aFixedRow        - [IN] VC�� VCDesc�� ����� Fixed Row Pointer
 * aColumns         - [IN] VC Column Description
 * aAddOIDFlag      - [IN] Variable Column�� �����ϴ� VC�� Transaction OID List��
 *                         Add�� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE.
 * aValues          - [IN] Variable Column�� Values
 * aVarcolumnCount  - [IN] Variable Column�� Value�� ����
 ***********************************************************************/
IDE_RC svcRecord::insertUnitedVarColumns( void            *  aTrans,
                                          smcTableHeader  *  aHeader,
                                          SChar           *  aFixedRow,
                                          const smiColumn ** aColumns,
                                          idBool             aAddOIDFlag,
                                          smiValue        *  aValues,
                                          SInt               aVarColumnCount )
{
    smOID         sCurVCPieceOID    = SM_NULL_OID;
    smOID         sNxtVCPieceOID    = SM_NULL_OID;
    
    // Piece ũ�� �ʱ�ȭ : LastOffsetArray size
    UInt          sVarPieceLen      = ID_SIZEOF(UShort);
    
    UInt          sPrevVarPieceLen  = 0;
    UShort        sVarPieceColCnt   = 0;
    idBool        sIsTrailingNull   = ID_FALSE;
    SInt          i                 = ( aVarColumnCount - 1 ); /* ���� ������ Column ���� �����Ѵ�. */

    // BUG-43117
    // ������ �� ����(ex. 5�� col�̸� 4�� col) �÷��� �е� ũ��
    UInt          sPrvAlign         = 0;
    // ���� �÷��� ���� �е� ũ��
    UInt          sCurAlign         = 0;

    IDE_ASSERT( aTrans    != NULL );
    IDE_ASSERT( aHeader   != NULL );
    IDE_ASSERT( aFixedRow != NULL );
    IDE_ASSERT( aColumns  != NULL );
    IDE_ASSERT_MSG(( aAddOIDFlag == ID_TRUE ) ||
                   ( aAddOIDFlag == ID_FALSE ),
                   "Add OID Flag : %"ID_UINT32_FMT"\n",
                   aAddOIDFlag );

    /* =================================================================
     * Value����� SMP_VC_PIECE_MAX_SIZE�� �Ѵ� ��� ���� �������� ���� �����ϰ� ����
     * �ʴ´ٸ� �ϳ��� Variable Piece�� �����Ѵ�. �̶� VC�� ������ Piece�� ���� ����
     * �� �� ������ Piece���� �����Ѵ�. �̷��� �ϴ� ������ Value�� �պ��� ������ ���
     * ���� Piece�� OID�� �˼� ���� ������ Piece�� ���� Logging�� ������ ���� Piece
     * �� �մܿ� Piece�� ������ ��� �մ��� Piece�� nextPieceOID�� ���� logging��
     * ������ �����ؾ��Ѵ�. ������ �� ������ Piece���� �������� ������ ��� ���� Piece
     * �� Next Piece OID�� �˱� ������ Variable Piece�� AllocSlot�� next piece��
     * ���� oid logging�� ���� �Ͽ� logging���� ���� �� �ִ�.
     * �ϳ��� column�� ���� �ٸ� var piece�� ������ ������� �ʴ´�.
     * ================================================================ */

    /* BUG-44463 SM codesonar warning
     *           aVarColumnCount�� 1���� Ŭ���� �ش� �Լ��� �����Ƿ� 
     *           ���� i�� 0���� ū���� üũ���� �ʾƵ� �ȴ�.
     *           �ش� �Լ��� ���� ȣ��ǹǷ� ��������� ���� i ���� üũ�� �����Ѵ�. */

    /* BUG-43320 ������ Column�� NULL �̶��
     * Trailing null �̴�. */
    if ( aValues[i].length == 0 ) /* ���÷��̰� ���̰� 0�ΰ��, start of trailing null */
    {
        sIsTrailingNull = ID_TRUE;
        i--;
    }
    else
    {
        /* Nothing to do */
    }

    for ( ; i >= 0 ; i-- )
    {
        if ( sIsTrailingNull == ID_TRUE )
        {
            if ( aValues[i].length == 0 )
            {
                continue; /* continuation of trailing null */
            }
            else
            {
                sIsTrailingNull = ID_FALSE;
            }
        }
        else
        {
            /* nothing todo */
        }


        if ( i > 0 ) /* ù�÷� ������ ������ �÷��� */
        {
            sVarPieceColCnt++;

            // BUG-43117 : varPiece�� ���̸� ����

            // value�� ���� + offset array 1ĭ ũ��
            sVarPieceLen += aValues[i].length + ID_SIZEOF(UShort);

            // ������ �� ���� �÷��� �е� ũ�� ����   
            sPrvAlign = SMC_GET_COLUMN_PAD_LENGTH( aValues[i-1], aColumns[i-1] );            

            // ���� �÷��� �е� ũ�� ����              
            sCurAlign = SMC_GET_COLUMN_PAD_LENGTH( aValues[i], aColumns[i] );

            // ������ ���� �÷��� �е�ũ�� varPiece ���̿� �߰�
            sVarPieceLen += sCurAlign;  
            
            // ���� �� ���� �÷��� ����
            sPrevVarPieceLen = aValues[i - 1].length;

            /* ���� ���� �����Ǵ� varPiece�� �� ���� =
             * ������ varPiece ���� + �� ���� �÷��� ����(�е�+value����)
             *                      + �� ���� offset array ũ��
             */
            if ( ( sVarPieceLen + sPrvAlign + sPrevVarPieceLen + ID_SIZEOF(UShort) )  
                 <= SMP_VC_PIECE_MAX_SIZE )
            {
                /* Previous Column ���� ������ slot �� ���� ���� �ϴٸ� �׳� �Ѿ�� */
            }
            else
            {
                /* Previous Column�� ���ϸ� �ִ�ũ�⸦ �ѱ�Ƿ�, ���ݱ��� �÷��� insert */
                IDE_TEST( insertUnitedVarPiece( aTrans,
                                                aHeader,
                                                aAddOIDFlag,
                                                &aValues[i],
                                                aColumns,   /* BUG-43117 */
                                                sVarPieceLen,
                                                sVarPieceColCnt,
                                                sNxtVCPieceOID,
                                                &sCurVCPieceOID )
                        != IDE_SUCCESS );

                sNxtVCPieceOID  = sCurVCPieceOID;
                sVarPieceColCnt = 0;
                
                /* insert �����Ƿ� �ٽ� �ʱ�ȭ */
                sVarPieceLen    =  ID_SIZEOF(UShort);
            }
        }
        else /* ù�÷��� ������ ���, insert */
        {
            sVarPieceColCnt++; 

            // value�� ���� + offset array 1ĭ ũ��
            sVarPieceLen += aValues[i].length + ID_SIZEOF(UShort);

            //���� �÷��� �е� ũ�� ����
            sCurAlign = SMC_GET_COLUMN_PAD_LENGTH( aValues[i], aColumns[i] );
 
            sVarPieceLen += sCurAlign;  /* BUG-43117 */

            IDE_TEST( insertUnitedVarPiece( aTrans,
                                            aHeader,
                                            aAddOIDFlag,
                                            &aValues[i],
                                            aColumns,
                                            sVarPieceLen,
                                            sVarPieceColCnt,
                                            sNxtVCPieceOID,
                                            &sCurVCPieceOID )
                      != IDE_SUCCESS );
        }
    }

    /* fixed row ���� ù var piece �� �̾������� oid ���� */
    ((smpSlotHeader*)aFixedRow)->mVarOID = sCurVCPieceOID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �������� Variable Column�� �ϳ��� variable slot �� Insert�Ѵ�.
 *               �׻� out-mode�� �����ϸ� vcDesc�� fixed row slot header�� �ִ�.
 *
 *               ����: VC Piece�� �� Header�� ���ؼ��� Alloc�� logging��
 *                    ������ VC Piece�� Value�� ����� �κп� ���ؼ��� Logging
 *                    ���� �ʴ´�. �ֳ��ϸ� Insert, Update�� �� �κп� ����
 *                    Logging�� �ϱ� �����̴�. ���⼭ Logging�� Update�Ŀ�
 *                    �ϴ��� ������ ���� �� ������ Update������ Before Image��
 *                    �ǹ̰� ���� �����̴�.
 *
 * aTrans           - [IN] Transaction Pointer
 * aHeader          - [IN] Table Header
 * aFixedRow        - [IN] VC�� VCDesc�� ����� Fixed Row Pointer
 * aColumns         - [IN] VC Column Description
 * aAddOIDFlag      - [IN] Variable Column�� �����ϴ� VC�� Transaction OID List��
 *                         Add�� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE.
 * aValues          - [IN] Variable Column�� Values
 * aVarcolnCount    - [IN] Variable Column�� Value�� ����
 ***********************************************************************/
IDE_RC svcRecord::insertUnitedVarPiece( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        idBool            aAddOIDFlag,
                                        smiValue        * aValues,
                                        const smiColumn** aColumns, /* BUG-43117 */
                                        UInt              aTotalVarLen,
                                        UShort            aVarColCount,
                                        smOID             aNxtVCPieceOID,
                                        smOID           * aCurVCPieceOID )
{
    SChar       * sPiecePtr         = NULL;
    UShort      * sColOffsetPtr     = NULL;
    smiValue    * sCurValue         = NULL;
    UShort        sValueOffset      = 0;
    SInt          i                 = 0;

    /* PROJ-2419 allocSlot ���ο��� Variable Column��
     * NxtOID�� NULLOID��, Flag�� Used�� �ʱ�ȭ �Ǿ� �Ѿ�´�. */
    IDE_TEST( svpVarPageList::allocSlot( aTrans,
                                         aHeader->mSpaceID,
                                         aHeader->mSelfOID,
                                         aHeader->mVar.mVRDB,
                                         aTotalVarLen,
                                         aNxtVCPieceOID,
                                         aCurVCPieceOID,
                                         &sPiecePtr )
            != IDE_SUCCESS );

    /* VarPiece Header�� Count���� ���� */
    ((smVCPieceHeader*)sPiecePtr)->colCount = aVarColCount;

    /* VarPiece�� Offset Array�� ���� �ּҸ� ������ */
    sColOffsetPtr = (UShort*)(sPiecePtr + ID_SIZEOF(smVCPieceHeader));

    sValueOffset = ID_SIZEOF(smVCPieceHeader) + ( ID_SIZEOF(UShort) * (aVarColCount+1) );

    for (i = 0; i < aVarColCount; i++)
    {
        sCurValue   = &aValues[i];

        /* BUG-43287 length�� 0�� column, �� NULL Column�� ���
         * Column �ڽ��� align ���� ����Ͽ� align�� ���߸� �ȵȴ�.
         * ���� Column�� align ���� �� ū ���� ���
         * ���� Offset�� �޶����� NULL Column�ӿ��� Size�� 0�� �ƴϰԵǾ�
         * NULL Column�̶�� �Ǵ��� ���� ���ϰ� �Ǳ� ����
         * NULL�� ��� Column List ������ ���� ū align ������ align�� �����־�� �Ѵ�. */
        if ( sCurValue->length != 0 )
        {
            //BUG-43117 align problem
            sValueOffset = (UShort) idlOS::align( (UInt) sValueOffset, aColumns[i]->align);

            sColOffsetPtr[i] = sValueOffset;

            idlOS::memcpy( sPiecePtr + sValueOffset, sCurValue->value, sCurValue->length );

            sValueOffset += sCurValue->length;
        }
        else
        {
            sValueOffset = (UShort) idlOS::align( (UInt) sValueOffset, aColumns[i]->maxAlign );
            sColOffsetPtr[i] = sValueOffset;
        }
    }
    //var piece �� �� offset ����. ������ ���ϴµ� �ʿ�.
    sColOffsetPtr[aVarColCount] = sValueOffset;

    if ( aAddOIDFlag == ID_TRUE )
    {
        /* ���� �Ҵ���� variable piece�� versioning list�� �߰� */
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           *aCurVCPieceOID,
                                           aHeader->mSpaceID,
                                           SM_OID_NEW_VARIABLE_SLOT )
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
/***********************************************************************
 * Description : aRow�� ���ؼ� Delete�� Update�Ҽ��ִ��� �����Ѵ�.
 *
 *               :check
 *               if aRow doesn't have next version, OK
 *               else
 *                  if tid of next version is equal to aRow's, OK
 *                  else
 *                      if scn of next version is infinite,
 *                          wait for transaction of next version to end,
 *                          when wake up, goto check
 *                      end
 *                  end
 *                end
 *
 *
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] ViewSCN
 * aSpaceID      - [IN] SpaceID
 * aRow          - [IN] Delete�� Update��� Row
 * aLockWaitTime - [IN] Lock Wait�� Time Out�ð�.
 * aState        - [IN] function return�� ���� Row�� �����ִ� Page�� Latch��
 *                      ��� �ִٸ� 1, �ƴϸ� 0
 *
 * aRetryInfo    - [IN] Retry Info
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC svcRecord::recordLockValidation(void                   *aTrans,
                                       smSCN                   aViewSCN,
                                       scSpaceID               aSpaceID,
                                       SChar                 **aRow,
                                       ULong                   aLockWaitTime,
                                       UInt                   *aState,
                                       const smiDMLRetryInfo * aRetryInfo,
                                       idBool                  aForbiddenToRetry )
{
    smpSlotHeader * sCurSlotHeaderPtr;
    smpSlotHeader * sPrvSlotHeaderPtr;
    scPageID        sPageID;
    scPageID        sNxtPageID;
    smTID           sWaitTransID = SM_NULL_TID;
    smSCN           sNxtRowSCN;
    smTID           sNxtRowTID;
    smTID           sTransID;
    smOID           sNextOID;

    IDE_ASSERT( aTrans != NULL );
    IDE_ASSERT( aRow   != NULL );
    IDE_ASSERT( *aRow  != NULL );
    IDE_ASSERT( aState != NULL );

    sPrvSlotHeaderPtr = (smpSlotHeader *)*aRow;
    sCurSlotHeaderPtr = sPrvSlotHeaderPtr;

    /* BUG-15642 Record�� �ι� Free�Ǵ� ��찡 �߻���.:
     * �̷� ��찡 �߻��ϱ����� validateUpdateTargetRow����
     * üũ�Ͽ� ������ �ִٸ� Rollback��Ű�� �ٽ� retry�� �����Ŵ*/
    IDE_TEST_RAISE( validateUpdateTargetRow( aTrans,
                                             aSpaceID,
                                             aViewSCN,
                                             sCurSlotHeaderPtr,
                                             aRetryInfo )
                    != IDE_SUCCESS, already_modified );

    sPageID = SMP_SLOT_GET_PID(*aRow);

    sTransID = smLayerCallback::getTransID( aTrans );

    while(! SM_SCN_IS_FREE_ROW( sCurSlotHeaderPtr->mLimitSCN ) /* aRow�� �ֽŹ����ΰ�?*/)
    {
        SMX_GET_SCN_AND_TID( sCurSlotHeaderPtr->mLimitSCN, sNxtRowSCN, sNxtRowTID );

        /* Next Version�� Lock Row�� ��� */
        if( SM_SCN_IS_LOCK_ROW( sNxtRowSCN ) )
        {
            if( sNxtRowTID == sTransID )
            {
                /* aRow�� ���� Lock�� aTrans�� �̹� ��Ҵ�.*/
                break;
            }
            else
            {
                /*�ٸ� Ʈ������� lock�� ��Ҵٸ� �װ� �����⸦ ��ٸ���.*/
            }
        }
        else
        {
            if( SM_SCN_IS_NOT_INFINITE( sNxtRowSCN ) )
            {
                IDE_TEST_RAISE( aRetryInfo == NULL, already_modified );

                // �ٷ� ó���Ѵ�.
                IDE_TEST_RAISE( SM_SCN_IS_DELETED( sNxtRowSCN ), already_modified );

                sNextOID = SMP_SLOT_GET_NEXT_OID( sCurSlotHeaderPtr );

                IDE_ASSERT( SM_IS_VALID_OID( sNextOID ) );

                // Next Row �� �ִ� ��� ���󰣴�
                // Next Row�� Page ID�� �ٸ��� Page Latch�� �ٽ� ��´�.
                IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                                   sNextOID,
                                                   (void**)&sCurSlotHeaderPtr )
                            == IDE_SUCCESS );

                sNxtPageID = SMP_SLOT_GET_PID( sCurSlotHeaderPtr );

                IDE_ASSERT( sNxtPageID != SM_NULL_PID );

                if( sPageID != sNxtPageID )
                {
                    *aState = 0;
                    IDE_TEST( svmManager::releasePageLatch( aSpaceID, sPageID )
                              != IDE_SUCCESS );
                    // to Next

                    IDE_TEST(svmManager::holdPageXLatch( aSpaceID, sNxtPageID ) != IDE_SUCCESS);
                    *aState = 1;
                    sPageID = sNxtPageID;
                }
            } //if
        } // else

        sWaitTransID = sNxtRowTID;

        *aState = 0;
        IDE_TEST( svmManager::releasePageLatch( aSpaceID, sPageID )
                  != IDE_SUCCESS );

        /* Next Version�� Transaction�� ���������� ��ٸ���. */
        IDE_TEST( smLayerCallback::waitLockForTrans( aTrans,
                                                     sWaitTransID,
                                                     aSpaceID,
                                                     aLockWaitTime )
                  != IDE_SUCCESS );

        IDE_TEST(svmManager::holdPageXLatch(aSpaceID, sPageID) != IDE_SUCCESS);
        *aState = 1;
    } // while

    if( sCurSlotHeaderPtr != sPrvSlotHeaderPtr )
    {
        IDE_ASSERT( aRetryInfo != NULL );
        IDE_ASSERT( aRetryInfo->mIsRowRetry == ID_FALSE );

        // Cur�� prv�� ���Ѵ�.
        if( aRetryInfo->mStmtRetryColLst != NULL )
        {
            IDE_TEST_RAISE( isSameColumnValue( aSpaceID,
                                               aRetryInfo->mStmtRetryColLst,
                                               sPrvSlotHeaderPtr,
                                               sCurSlotHeaderPtr )
                            == ID_FALSE , already_modified );
        }

        if( aRetryInfo->mRowRetryColLst != NULL )
        {
            IDE_TEST_RAISE( isSameColumnValue( aSpaceID,
                                               aRetryInfo->mRowRetryColLst,
                                               sPrvSlotHeaderPtr,
                                               sCurSlotHeaderPtr )
                            == ID_FALSE , need_row_retry );
        }
    }

    *aRow = (SChar*)sCurSlotHeaderPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( already_modified );
    {
        if( aForbiddenToRetry == ID_TRUE )
        {
            IDE_DASSERT( ((smxTrans*)aTrans)->mIsGCTx == ID_TRUE );

            SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[RECORD VALIDATION] "
                             "SpaceID:%"ID_UINT32_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CreateSCN:%"ID_UINT64_FMT", "
                             "LimitSCN:%"ID_UINT64_FMT,
                             aSpaceID,
                             SM_SCN_TO_LONG(aViewSCN),
                             SM_SCN_TO_LONG(sCurSlotHeaderPtr->mCreateSCN),
                             SM_SCN_TO_LONG(sCurSlotHeaderPtr->mLimitSCN) );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode(smERR_RETRY_Already_Modified) );
        }
    }
    IDE_EXCEPTION( need_row_retry );
    {
        IDE_SET(ideSetErrorCode (smERR_RETRY_Row_Retry));
    }
    IDE_EXCEPTION_END;

    *aRow = (SChar*)sCurSlotHeaderPtr;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Column list�� �������� �ΰ��� row�� value�� ������
 *               ���Ѵ�.
 *
 * aSpaceID          - [IN] Space ID
 * aChkColumnList    - [IN] �� �� column �� list
 * aPrvSlotHeaderPtr - [IN] �� �� Record 1
 * aCurSlotHeaderPtr - [IN] �� �� Record 2
 *
 ***********************************************************************/
idBool svcRecord::isSameColumnValue( scSpaceID               /* aSpaceID */,
                                     const smiColumnList   * aChkColumnList,
                                     smpSlotHeader         * aPrvSlotHeaderPtr,
                                     smpSlotHeader         * aCurSlotHeaderPtr )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;

    SChar           * sPrvRowPtr = (SChar*)aPrvSlotHeaderPtr;
    SChar           * sCurRowPtr = (SChar*)aCurSlotHeaderPtr;
    SChar           * sPrvValue         = NULL;
    SChar           * sCurValue         = NULL;
    idBool            sIsSameColumnValue = ID_TRUE;
    smcLobDesc      * sPrvLobDesc;
    smcLobDesc      * sCurLobDesc;
    UInt              sPrvVCLength  = 0;
    UInt              sCurVCLength  = 0;

    IDE_ASSERT( sPrvRowPtr != NULL );
    IDE_ASSERT( sCurRowPtr != NULL );

    /* Fixed Row�� ������ Variable Column�� ����Ǵ� Var Column��
       Update�Ǵ� Column�� New Version�� �����.*/
    for( sCurColumnList = aChkColumnList;
         (( sCurColumnList != NULL ) &&
          ( sIsSameColumnValue != ID_FALSE )) ;
         sCurColumnList = sCurColumnList->next )
    {
        sColumn = sCurColumnList->column;

        switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
        {
            case SMI_COLUMN_TYPE_FIXED:

                if( idlOS::memcmp( sCurRowPtr + sColumn->offset,
                                   sPrvRowPtr + sColumn->offset,
                                   sColumn->size ) != 0 )
                {
                    sIsSameColumnValue = ID_FALSE;
                }

                break;

            case SMI_COLUMN_TYPE_VARIABLE:
                sPrvValue = sgmManager::getVarColumn( sPrvRowPtr, sColumn, &sPrvVCLength );
                sCurValue = sgmManager::getVarColumn( sCurRowPtr, sColumn, &sCurVCLength );

                if ( sPrvVCLength != sCurVCLength )
                {
                    sIsSameColumnValue = ID_FALSE;
                }
                else
                {
                    if ( idlOS::memcmp( sPrvValue,
                                        sCurValue, 
                                        sCurVCLength ) != 0 )
                    {
                        sIsSameColumnValue = ID_FALSE;
                    }
                    else
                    {
                        /* do nothing */
                    }
                }
                break;

            case SMI_COLUMN_TYPE_LOB:

                sPrvLobDesc = (smcLobDesc*)(sPrvRowPtr + sColumn->offset);
                sCurLobDesc = (smcLobDesc*)(sCurRowPtr + sColumn->offset);

                /* BUG-41952
                   1. SM_VCDESC_MODE_IN �� ���(��, in-mode LOB�� ���),
                      smcLobDesc ��ŭ ���ϸ� �ȵ˴ϴ�.
                      in-mode�� ��� smVCDescInMode�� ����ǰ�, �ڿ� �����Ͱ� ����˴ϴ�.
                   2. smVCDescInMode �� smcLobDesc �� �� ����ü�Դϴ�.
                      in-mode, out-mode ��� memcmp�� ����ϸ� �ȵǰ�,
                      ����ü ���� ����� ���� �� �ؾ� �մϴ�. */

                // In-mode�� out-mode�� ���������� ���Ǵ� �� ��
                if ( ( sPrvLobDesc->flag   != sCurLobDesc->flag   ) ||
                     ( sPrvLobDesc->length != sCurLobDesc->length ) )
                {
                    sIsSameColumnValue = ID_FALSE;
                    break;
                }

                if ( SM_VCDESC_IS_MODE_IN( sCurLobDesc) )
                {
                    // In-mode�� ��쿡�� ���� ���� ��
                    if ( idlOS::memcmp( ((UChar*)sPrvLobDesc) + ID_SIZEOF(smVCDescInMode),
                                        ((UChar*)sCurLobDesc) + ID_SIZEOF(smVCDescInMode),
                                        sCurLobDesc->length ) != 0 )
                    {
                        sIsSameColumnValue = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    IDE_DASSERT( ( sCurLobDesc->flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT );

                    /* Out-mode�� ��쿡�� �� ����ü�� ������ ���Ѵ�.
                       1. LOB ��ü�� ���ŵǾ��� ���,
                          mLobVersion�� 0�� �ǰ�, mFirstLPCH�� ���Ӱ� �Ҵ�Ǹ�,
                          �Ҵ�� mFirstLPCH[0]�� mOID�� fstPieceOID�� �Ҵ��� �ȴ�.
                          �׷��Ƿ� mFirstLPCH�� ���� fstPieceOID ���� �ٸ��� ���ϸ� �ȴ�.
                       2. LOB �κи� ���ŵǾ��� ���,
                          mLobVersion�� �����ϰ� �ǰ�, mLobVersion�� ���ϸ� �ȴ�.
                       3. LOB���� LPCH�� �߰��� ���,
                          mLPCHCount ���� �����ϹǷ�, mLPCHCount�� ���ϸ� �ȴ�.
                       ��, �� ����ü�� ��� ������ ���ϴ� ������ �÷� ���� ���� ���� �Ǵ� ���� */
                    if ( ( sPrvLobDesc->fstPieceOID != sCurLobDesc->fstPieceOID ) ||
                         ( sPrvLobDesc->mLPCHCount  != sCurLobDesc->mLPCHCount  ) ||
                         ( sPrvLobDesc->mLobVersion != sCurLobDesc->mLobVersion ) ||
                         ( sPrvLobDesc->mFirstLPCH  != sCurLobDesc->mFirstLPCH  ) )
                    {
                        sIsSameColumnValue = ID_FALSE;
                        break;
                    }
                }

                break;

            /* volatile���� Geometry�� �������� �ʴ´�. */
            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            default:
                IDE_ASSERT(0);
                break;
        }
    }

    return sIsSameColumnValue;

}

/***********************************************************************
 *  Update, delete, lock row�Ϸ��� row�� �������� Test�Ѵ�.
 *  ������ ���� üũ�� �����Ѵ�.
 *
 *  1. ������ Row�� Delete Bit�� Setting�� ���� ����.
 *  2. ������ Row�� Row�� SCN�� SM_SCN_ROLLBACK_NEW�� �ɼ��� ����.
 *  3. Row�� SCN�� ���Ѵ밡 �ƴ϶�� Row�� SCN�� �ݵ�� aViewSCN���� �۾ƾ� �Ѵ�.
 *
 *  aRowPtr   - [IN] Row Pointer
 *  aSpaceID  - [IN] Row�� Table�� ���� Tablespace�� ID
 *  aViewSCN  - [IN] View SCN
 *  aRowPtr   - [IN] ������ ������ Row Pointer
 *  aRetryInfo- [IN] Retry Info
 *
 ***********************************************************************/
IDE_RC svcRecord::validateUpdateTargetRow(void                  * aTrans,
                                          scSpaceID               aSpaceID,
                                          smSCN                   aViewSCN,
                                          void                  * aRowPtr,
                                          const smiDMLRetryInfo * aRetryInfo)
{
    smSCN           sRowSCN;
    smTID           sRowTID;
    smOID           sRowOID;
    smpSlotHeader * sSlotHeader;
    idBool          sIsError = ID_FALSE;
    void          * sLogSlotPtr;
    smxTrans      * sTrans = (smxTrans*)aTrans;

    sSlotHeader = (smpSlotHeader *)aRowPtr;

    SMX_GET_SCN_AND_TID( sSlotHeader->mCreateSCN, sRowSCN, sRowTID );

    /* 1. ������ Row�� Delete Bit�� Setting�� ���� ����. */
    if( SM_SCN_IS_DELETED( sRowSCN ) )
    {
        // Delete�� Row�� Update�Ϸ��� �Ѵ�.
        sIsError = ID_TRUE;

        ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                    "[##WARNING ##WARNING!!] The Row was already set deleted\n");
    }

    if( sIsError == ID_FALSE )
    {
        /* 2. ������ Row�� Row�� SCN�� SM_SCN_ROLLBACK_NEW�� �ɼ��� ����. */
        if( SM_SCN_IS_FREE_ROW( sRowSCN ) )
        {
            // �̹� Ager�� Free�� Row�� Free�Ϸ��� �Ѵ�.
            sIsError = ID_TRUE;
            ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                        "[##WARNING ##WARNING!!] The Row was already freed\n");
        }
    }

    if( sIsError == ID_FALSE )
    {
        /* 3. ���� Row�� SCN�� ���Ѵ���, Row�� TID�� �� ������ �����ϴ�
           Transaction�� �����ؾ� �Ѵ�. */
        if( SM_SCN_IS_INFINITE( sRowSCN ) )
        {
            if ( smLayerCallback::getTransID( aTrans ) != sRowTID )
            {
                sIsError = ID_TRUE;
                ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                            "[##WARNING ##WARNING!!] The Row is invalid target row\n");
            }
            else
            {
                /* PROJ-2694 Fetch Across Rollback
                 * holdable cursor open ���� �ڽ��� ������ row�� �������� ���
                 * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����. */
                if ( ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                     ( SM_SCN_IS_LT( &sRowSCN, &( sTrans->mCursorOpenInfSCN ) ) == ID_TRUE ) )
                {
                    sTrans->mIsReusableRollback = ID_FALSE;
                }
            }
        }
        else
        {
            /* 4. ���Ѵ밡 �ƴ϶�� Row�� SCN�� �ݵ�� aViewSCN���� �۾ƾ� �Ѵ�. */
            if( SM_SCN_IS_GT( &sRowSCN, &aViewSCN ) )
            {
                /* PROJ-1784 Row retry ������ View SCN���� Ŭ���� �ִ� */
                if( aRetryInfo == NULL )
                {
                    sIsError = ID_TRUE;
                    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                                "[##WARNING ##WARNING!!] This statement can't read this row\n");
                }
                else
                {
                    if( aRetryInfo->mIsRowRetry == ID_FALSE )
                    {
                        sIsError = ID_TRUE;
                        ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                                    "[##WARNING ##WARNING!!] This statement can't read this row\n");
                    }
                }
            }
        }
    }

    if( sIsError == ID_TRUE )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     "TID:[%d] View SCN:[%llx]\n",
                     smLayerCallback::getTransID( aTrans ),
                     SM_SCN_TO_LONG( aViewSCN ) );

        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     "[##WARNING ##WARNING!!] Target row's info\n");
        smcRecord::logSlotInfo(aRowPtr);

        sRowOID = SMP_SLOT_GET_NEXT_OID( sSlotHeader );

        if( SM_IS_VALID_OID( sRowOID ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                         "[##WARNING ##WARNING!!] Next row of target row info\n");
            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID, 
                                               sRowOID,
                                               &sLogSlotPtr )
                        == IDE_SUCCESS );
            smcRecord::logSlotInfo( sLogSlotPtr );
        }
        else
        {
            if( SMP_SLOT_IS_LOCK_TRUE( sSlotHeader ))
            {
                ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                             "[##WARNING ##WARNING!!] target row is locked \n");
            }
            else
            {
                if( SM_SCN_IS_FREE_ROW( sSlotHeader->mLimitSCN ) )
                {
                    ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                                 "[##WARNING ##WARNING!!] Next pointer of target row has null \n");
                }
                else
                {
                    ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                                 "[##WARNING ##WARNING!!] target row is deleted row \n");
                }
            }
        }
    }

    IDE_TEST_RAISE( sIsError == ID_TRUE, err_invalide_version);

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalide_version );
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFixedRow���� aColumn�� ����Ű�� Variable Column�� �����Ѵ�.
 *
 * aTrans     - [IN] Transaction Pointer
 * aHeader    - [IN] Table Header
 * aFstOID    - [IN] VC Piece �� ù OID
 ***********************************************************************/
IDE_RC svcRecord::deleteVC(void              *aTrans,
                           smcTableHeader    *aHeader,
                           smOID              aFstOID )
{
    smOID             sVCPieceOID       = SM_NULL_OID;
    smOID             sNxtVCPieceOID    = SM_NULL_OID;
    SChar            *sVCPiecePtr       = NULL;

    sVCPieceOID = aFstOID;

    /* VC�� �������� VC Piece������ ��� ���
       Piece�� ���ؼ� ���� �۾��� �����Ѵ�. */
    while ( sVCPieceOID != SM_NULL_OID )
    {
        IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                           sVCPieceOID,
                                           (void**)&sVCPiecePtr )
                == IDE_SUCCESS );

        sNxtVCPieceOID = ((smVCPieceHeader*)sVCPiecePtr)->nxtPieceOID;

        IDE_TEST( svcRecordUndo::logPhysical8( smLayerCallback::getLogEnv( aTrans ),
                                               (SChar*)&((smVCPieceHeader*)sVCPiecePtr)->flag,
                                               ID_SIZEOF(((smVCPieceHeader*)sVCPiecePtr)->flag) )
                  != IDE_SUCCESS );

        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sVCPieceOID,
                                           aHeader->mSpaceID,
                                           SM_OID_OLD_VARIABLE_SLOT )
                != IDE_SUCCESS );

        sVCPieceOID = sNxtVCPieceOID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * description : aRow���� aColumn�� ����Ű�� vc�� Header�� ���Ѵ�.
 *
 * aRow     - [in] fixed row pointer
 * aColumn  - [in] column desc
 ***********************************************************************/
IDE_RC svcRecord::getVCPieceHeader( const void      *  aRow,
                                    const smiColumn *  aColumn,
                                    smVCPieceHeader ** aVCPieceHeader,
                                    UShort          *  aOffsetIdx )
{
    UShort            sOffsetIdx        = ID_USHORT_MAX;
    smVCPieceHeader * sVCPieceHeader    = NULL;
    smOID             sOID              = SM_NULL_OID;

    IDE_ASSERT( aRow            != NULL );
    IDE_ASSERT( aColumn         != NULL );

    sOID        = ((smpSlotHeader*)aRow)->mVarOID;

    IDE_DASSERT( sOID != SM_NULL_OID );

    IDE_ASSERT( smmManager::getOIDPtr( aColumn->colSpace,
                                       sOID,
                                       (void**)&sVCPieceHeader)
            == IDE_SUCCESS );

    sOffsetIdx = aColumn->varOrder;

    while ( sOffsetIdx >= sVCPieceHeader->colCount )
    {
        if ( sVCPieceHeader->nxtPieceOID != SM_NULL_OID )
        {
            sOffsetIdx -= sVCPieceHeader->colCount;
            sOID    = sVCPieceHeader->nxtPieceOID;

            IDE_ASSERT( smmManager::getOIDPtr( aColumn->colSpace,
                                               sOID,
                                               (void**)&sVCPieceHeader)
                    == IDE_SUCCESS );
        }
        else
        {
            sVCPieceHeader  = NULL;
            sOffsetIdx      = ID_USHORT_MAX;
            break;
        }
    }

    *aOffsetIdx     = sOffsetIdx;
    *aVCPieceHeader = sVCPieceHeader;

    return IDE_SUCCESS;
}

/* PROJ-2174 Supporting LOB in the volatile tablespace */
 /***********************************************************************
 * Description : aRow�� aColumn�� ����Ű�� Lob Column�� �����.
 *
 * aTrans    - [IN] Transaction Pointer
 * aHeader   - [IN] Table Header
 * aColumn   - [IN] Table Column List
 * aRow      - [IN] Row Pointer
 ***********************************************************************/
IDE_RC svcRecord::deleteLob(void              * aTrans,
                            smcTableHeader    * aHeader,
                            const smiColumn   * aColumn,
                            SChar             * aRow)
{
    smVCDesc    * sVCDesc;
    smcLobDesc  * sCurLobDesc;

    IDE_DASSERT( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
                 == SMI_COLUMN_TYPE_LOB);

    sCurLobDesc = (smcLobDesc *)(aRow + aColumn->offset);

    if((sCurLobDesc->flag & SM_VCDESC_MODE_MASK) ==
       SM_VCDESC_MODE_OUT)
    {
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           (smOID)(sCurLobDesc->mFirstLPCH),
                                           aHeader->mSpaceID,
                                           SM_OID_OLD_LPCH )
                  != IDE_SUCCESS );
    }

    sVCDesc = getVCDesc(aColumn, aRow);

    if ( ( sVCDesc->flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT )
    {
        IDE_TEST(deleteVC(aTrans, aHeader, sVCDesc->fstPieceOID)
                != IDE_SUCCESS);
    }
    else
    {
        /* Nothing to do. inmode �� fixed record���� �Բ� ó���ȴ�. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: Variable Length Data�� �д´�. ���� ���̰� �ִ� Piece���� ����
 *              �۰� �о�� �� Piece�� �ϳ��� �������� �����Ѵٸ� �о�� �� ��ġ��
 *              ù��° ����Ʈ�� ��ġ�� �޸� �����͸� �����Ѵ�. �׷��� ���� ��� Row��
 *              ���� �����ؼ� �Ѱ��ش�.
 *
 * aRow         - [IN] �о���� �ķ��� ������ �ִ� Row Pointer
 * aColumn      - [IN] �о���� �÷��� ����
 * aLength      - [IN-OUT] IN : IsReturnLength == ID_FALSE
 *                         OUT: isReturnLength == ID_TRUE
 * aBuffer      - [IN] value�� �����.
 *
 ***********************************************************************/
SChar* svcRecord::getVarRow( const void      * aRow,
                             const smiColumn * aColumn,
                             UInt              /* aFstPos */,
                             UInt            * aLength,
                             SChar           * /* aBuffer */,
                             idBool            /* aIsReturnLength*/ )
{
    SChar           * sRet              = NULL;
    UShort          * sCurrOffsetPtr    = NULL;
    smVCPieceHeader * sVCPieceHeader    = NULL;
    UShort            sOffsetIdx        = 0;

    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT( aColumn != NULL );
    IDE_ASSERT_MSG( ( aColumn->flag & SMI_COLUMN_TYPE_MASK )
                    == SMI_COLUMN_TYPE_VARIABLE,
                    "flag : %"ID_UINT32_FMT"\n",
                    aColumn->flag );
    IDE_ASSERT_MSG( ( aColumn->flag & SMI_COLUMN_STORAGE_MASK )
                    == SMI_COLUMN_STORAGE_MEMORY,
                    "flag : %"ID_UINT32_FMT"\n",
                    aColumn->flag );


    if ( ((smpSlotHeader*)aRow)->mVarOID == SM_NULL_OID )
    {
        *aLength    = 0; 
        sRet        = NULL;
    }
    else
    {
        IDE_ASSERT( getVCPieceHeader( aRow, aColumn, &sVCPieceHeader, &sOffsetIdx ) == IDE_SUCCESS );

        if ( sVCPieceHeader == NULL ) 
        {
            /* training null */
            *aLength    = 0; 
            sRet        = NULL;
        }
        else
        {
            IDE_DASSERT( sOffsetIdx != ID_USHORT_MAX );
            IDE_DASSERT( sOffsetIdx < sVCPieceHeader->colCount );

            /* +1 �� ��� ����� �ǳʶٰ� offset array �� Ž���ϱ� �����̴�. */
            sCurrOffsetPtr = ((UShort*)(sVCPieceHeader + 1) + sOffsetIdx);

            IDE_DASSERT( *(sCurrOffsetPtr + 1) >= *sCurrOffsetPtr );

            /* next offset ���� ���� ���̰� �ȴ�. */
            *aLength    = ( *( sCurrOffsetPtr + 1 )) - ( *sCurrOffsetPtr );

            if ( *aLength == 0 )
            {
                /* �߰��� �� NULL value�� offset ���̰� 0 �̴� */
                sRet = NULL;
            }
            else
            {
                sRet = (SChar*)sVCPieceHeader + (*sCurrOffsetPtr);
            }
        }
    }

    return sRet;
}

/***********************************************************************
 * description : aRowPtr���� aColumn�� ����Ű�� value�� length�� ���Ѵ�.
 *
 * aRowPtr  - [in] fixed row pointer
 * aColumn  - [in] column desc
 ***********************************************************************/
UInt svcRecord::getVarColumnLen( const smiColumn    * aColumn,
                                 const SChar        * aRowPtr )
                                 
{
    smVCPieceHeader * sVCPieceHeader = NULL;
    UShort          * sCurrOffsetPtr = NULL;
    UShort            sLength        = 0;
    UShort            sOffsetIdx     = ID_USHORT_MAX;

    IDE_DASSERT( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
                 == SMI_COLUMN_TYPE_VARIABLE);

    if ( ((smpSlotHeader*)aRowPtr)->mVarOID == SM_NULL_OID )
    {
        sLength = 0;
    }
    else
    {
        IDE_ASSERT( getVCPieceHeader( aRowPtr, aColumn, &sVCPieceHeader, &sOffsetIdx ) == IDE_SUCCESS );

        if ( sVCPieceHeader == NULL )
        {
            sLength = 0;
        }
        else
        {
            IDE_DASSERT( sOffsetIdx != ID_USHORT_MAX );
            IDE_DASSERT( sOffsetIdx < sVCPieceHeader->colCount );

            /* +1 �� ��� ����� �ǳʶٰ� offset array �� Ž���ϱ� �����̴�. */
            sCurrOffsetPtr = ((UShort*)(sVCPieceHeader + 1) + sOffsetIdx);

            IDE_DASSERT( *(sCurrOffsetPtr + 1) >= *sCurrOffsetPtr );

            sCurrOffsetPtr = (UShort*)(sVCPieceHeader + 1) + sOffsetIdx; /* +1 headersize �ǳʶٱ�*/

            sLength = ( *( sCurrOffsetPtr + 1 )) - ( *sCurrOffsetPtr ); /* +1 �� next offset */
        }
    }

    return sLength;
}

/***********************************************************************
 * Description : aRowPtr�� ����Ű�� Row���� aColumn�� �ش��ϴ�
 *               Column�� ���̴� �����Ѵ�.
 *      
 * aRowPtr - [IN] Row Pointer
 * aColumn - [IN] Column����.
 ***********************************************************************/
UInt svcRecord::getColumnLen( const smiColumn * aColumn,
                              SChar           * aRowPtr )
{
    smVCDesc            * sVCDesc   = NULL;
    UInt                  sLength   = 0;
    
    switch ( (aColumn->flag & SMI_COLUMN_TYPE_MASK) )
    {
        case SMI_COLUMN_TYPE_LOB:
                sVCDesc = (smVCDesc*)getColumnPtr(aColumn, aRowPtr);
                sLength = sVCDesc->length;
                break;

        case SMI_COLUMN_TYPE_VARIABLE:
            sLength = getVarColumnLen( aColumn, (const SChar*)aRowPtr );
            break;

        case SMI_COLUMN_TYPE_FIXED:
            sLength = aColumn->size;
            break;

        case SMI_COLUMN_TYPE_VARIABLE_LARGE:
        default:
            IDE_ASSERT(0);
            break;
    }

    return sLength;
}
