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
 * $Id: smcRecord.cpp 90083 2021-02-26 00:58:48Z et16 $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <idv.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smp.h>
#include <smc.h>
#include <smcReq.h>
#include <smcLob.h>
#include <sgmManager.h>
#include <smx.h>

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
 * aAddOIDFlag     - [IN] 1. SM_INSERT_ADD_OID_OK : OID List�� Insert�� �۾����.
 *                        2. SM_INSERT_ADD_OID_NO : OID List�� Insert�� �۾��� ������� �ʴ´�.

 *  TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log can
 *                       be reduced to 1.
 *  SMR_SMC_PERS_INSERT_ROW �α׿� SMR_SMC_PERS_UPDATE_VERSION_ROW �α״�
 *  ���� ������ redo �Լ��� ����Ѵ�.
 *  BUG-32319���� SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION �α׿�
 *  SMR_SMC_PERS_UPDATE_VERSION �αװ� �ϳ��� �������� ������
 *  SMR_SMC_PERS_INSERT_ROW�� �α� ������ SMR_SMC_PERS_UPDATE_VERSION_ROW �α���
 *  ���˰� �����ϰ� �����ֱ� ���� OldVersion RowOID�� NextOID�� �߰��Ͽ���.
 *
 *  log structure:
 *   <update_log, NewOid, FixedRow_size, Fixed_Row, var_col1, var_col2...,
 *    OldVersion RowOID(NULL_OID), OldVersion NextOID(NULL_OID)>
 *  Logging�� Variable Column�� Fixed Row�� �α׸� Replication�� ���� �ϳ���
 *  �����Ѵ�.
 ***********************************************************************/
IDE_RC smcRecord::insertVersion( idvSQL*          /*aStatistics*/,
                                 void*            aTrans,
                                 void*            aTableInfoPtr,
                                 smcTableHeader*  aHeader,
                                 smSCN            aInfinite,
                                 SChar**          aRow,
                                 scGRID*          aRetInsertSlotGRID,
                                 const smiValue*  aValueList,
                                 UInt             aAddOIDFlag )
{
    const smiColumn   * sCurColumn    = NULL;
    SChar             * sNewFixRowPtr = NULL;
    // BUG-43117 : const smiValue* -> smiValue* (�ڵ� ������ �� ���� ���� ���)
    smiValue          * sCurValue     = NULL;
    UInt                i;
    smOID               sFixOid;
    smOID               sFstVCPieceOID;
    scPageID            sPageID                 = SM_NULL_PID;
    UInt                sAfterImageSize         = 0;
    UInt                sColumnCount            = 0;
    SInt                sUnitedVarColumnCount   = 0;
    UInt                sLargeVarCount          = 0;
    smpPersPageHeader * sPagePtr                = NULL;
    const smiColumn   * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    const smiColumn   * sLargeVarColumns[SMI_COLUMN_ID_MAXIMUM];
    smiValue            sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];
    smcLobDesc        * sCurLobDesc     = NULL;
    idBool              sIsAddOID = ID_FALSE;

    // BUG-47366 UnitedVar�� Alloc Log ����
    smOID               sVCPieceOID;
    SChar             * sVCPiecePtr    = NULL;
    smVCPieceHeader   * sVCPieceHeader = NULL;

    IDE_ERROR( aTrans     != NULL );
    IDE_ERROR( aHeader    != NULL );
    IDE_ERROR( aValueList != NULL );

    IDU_FIT_POINT( "1.PROJ-2118@smcRecord::insertVersion" );

    /* Log�� ����ϱ� ���� Buffer�� �ʱ�ȭ�Ѵ�. �� Buffer�� Transaction����
       �ϳ��� �����Ѵ�.*/
    smLayerCallback::initLogBuffer( aTrans );

    /* insert�ϱ� ���Ͽ� ���ο� Slot �Ҵ� ����.*/
    IDE_TEST( smpFixedPageList::allocSlot( aTrans,
                                           aHeader->mSpaceID,
                                           aTableInfoPtr,
                                           aHeader->mSelfOID,
                                           &(aHeader->mFixed.mMRDB),
                                           &sNewFixRowPtr,
                                           aInfinite,
                                           aHeader->mMaxRow,
                                           SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT)
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sFixOid = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
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

    /* TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log
     *                      can be reduced to 1.
     *
     * SMR_SMC_PERS_UPDATE_VERSION_ROW �α׿� ������ ���߾�� �Ѵ�.
     * sAfterImageSize = ID_SIZEOF(UShort)
     *                   + fixed_slot_size
     *                   + variable_slot_size
     *                   + ID_SIZEOF(smOID) <--- OldVersion RowOID(NULL_OID) */
    sAfterImageSize = aHeader->mFixed.mMRDB.mSlotSize - SMP_SLOT_HEADER_SIZE;
    sAfterImageSize += ID_SIZEOF(UShort);
    sAfterImageSize += ID_SIZEOF(ULong);

    /* --------------------------------------------------------
     * [4] Row�� fixed ������ ���� �α� ��
     *     variable column�� ���� ���� ����Ÿ ����
     *     (�α׷��ڵ带 �����ϴ� �۾��̸�
     *      ���� ����Ÿ���ڵ��� �۾��� ������ �κп��� ������)
     * -------------------------------------------------------- */
    sColumnCount    = aHeader->mColumnCount;

    // BUG-43117 : �ڵ� ������ �� ���� ���� ���
    sCurValue       = (smiValue *)aValueList;

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

        IDU_FIT_POINT( "3.PROJ-2118@smcRecord::insertVersion" );

        if( ( sCurColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
            != SMI_COLUMN_COMPRESSION_TRUE )
        {
            switch( sCurColumn->flag & SMI_COLUMN_TYPE_MASK )
            {
                case SMI_COLUMN_TYPE_LOB:
                    sCurLobDesc = (smcLobDesc*)(sNewFixRowPtr + sCurColumn->offset);

                    // prepare���� ���� LobDesc�� old version���� �ν��ϱ� ������ �ʱ�ȭ
                    SMC_LOB_DESC_INIT( sCurLobDesc );

                    if(sCurValue->length > 0)
                    {
                        // SQL�� ���� LOB ����Ÿ�� ���� ���
                        IDE_TEST( smcLob::reserveSpaceInternal(
                                              aTrans,
                                              aHeader,
                                              (smiColumn*)sCurColumn,
                                              sCurLobDesc->mLobVersion,
                                              sCurLobDesc,
                                              0,                /* aOffset */
                                              sCurValue->length,
                                              aAddOIDFlag,
                                              ID_TRUE )         /* aIsFullWrite */
                                  != IDE_SUCCESS );

                        IDE_TEST( smcLob::writeInternal(
                                      aTrans,
                                      aHeader,
                                      (UChar*)sNewFixRowPtr,
                                      (smiColumn*)sCurColumn,
                                      0,                     /* aOffset */
                                      sCurValue->length,
                                      (UChar*)sCurValue->value,
                                      ID_FALSE,              /* aIsWriteLog */
                                      ID_FALSE,              /* aIsReplSenderSend */
                                      (smLobLocator)NULL )   /* aLobLocator */
                                  != IDE_SUCCESS );

                        if(sCurLobDesc->flag == SM_VCDESC_MODE_OUT)
                        {
                            sAfterImageSize += getVCAMLogSize(sCurValue->length);
                            sLargeVarColumns[sLargeVarCount] = sCurColumn;
                            sLargeVarCount++;
                        }
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

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* Geometry Type �Ǵ� VARIABLE_LARGE Ÿ���� ������ ���
                     * UnitedVar�� �ƴ� PROJ-2419 ���� �� Variable ������ ����Ѵ�. */
                    sFstVCPieceOID = SM_NULL_OID;
                    
                    IDE_TEST( smcRecord::insertLargeVarColumn ( aTrans,
                                                                aHeader,
                                                                sNewFixRowPtr,
                                                                sCurColumn,
                                                                sIsAddOID,
                                                                sCurValue->value,
                                                                sCurValue->length,
                                                                &sFstVCPieceOID )
                              != IDE_SUCCESS );
                    
                    if ( sFstVCPieceOID != SM_NULL_OID )
                    {
                        /* Variable Column�� Out Mode�� �����. */
                        sAfterImageSize += getVCAMLogSize(sCurValue->length);
                        sLargeVarColumns[sLargeVarCount] = sCurColumn;
                        sLargeVarCount++;
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;

                case SMI_COLUMN_TYPE_FIXED:
                    IDU_FIT_POINT( "2.PROJ-2118@smcRecord::insertVersion" );

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
        }
        else
        {
            // PROJ-2264
            idlOS::memcpy(sNewFixRowPtr + sCurColumn->offset,
                          sCurValue->value,
                          sCurValue->length);
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
                                           sUnitedVarColumnCount,
                                           &sAfterImageSize )
                != IDE_SUCCESS );
    }
    else
    {
        /* United Var �� ��� NULL OID �� �α��Ѵ� */
        sAfterImageSize += ID_SIZEOF(smOID);
    }

    //PROJ-2419 insert UnitedVarColumn FitTest: before log write
    IDU_FIT_POINT( "1.PROJ-2419@smcRecord::insertVersion::before_log_write" );   
 
    /* Insert�� ���� Log�� ����Ѵ�. */
    IDE_TEST( smcRecordUpdate::writeInsertLog( aTrans,
                                               aHeader,
                                               sNewFixRowPtr,
                                               sAfterImageSize,
                                               sUnitedVarColumnCount,
                                               sUnitedVarColumns,
                                               sLargeVarCount,
                                               sLargeVarColumns )
              != IDE_SUCCESS );

    /* BUG-14513: Fixed Slot�� �Ҵ�� Alloc Slot Log�� �������
       �ʵ��� ����. insert log�� Alloc Slot�� ���� Redo, Undo����.
       �� ������ insert log��� �� Slot header�� ���� Update����
    */
    smpFixedPageList::setAllocatedSlot( aInfinite, sNewFixRowPtr );

    // BUG-47366 UnitedVar�� Alloc Log ����
    // insert UnitedVar ���� �߻��� alloc Slot���� Slot Header�� �ʱ�ȭ �ȵ�����
    // �ʱ�ȭ ���־�� �Ѵ�.
    if ( sUnitedVarColumnCount > 0 )
    {
        sVCPieceOID = ((smpSlotHeader*)sNewFixRowPtr)->mVarOID;
        while ( sVCPieceOID != SM_NULL_OID )
        {
            IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                               sVCPieceOID,
                                               (void**)&sVCPiecePtr )
                        == IDE_SUCCESS );
            sVCPieceHeader = (smVCPieceHeader*)sVCPiecePtr;
            
            sVCPieceHeader->flag &= ~SM_VCPIECE_FREE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_FREE_NO;
            sVCPieceHeader->flag &= ~SM_VCPIECE_TYPE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_TYPE_OTHER;

            IDE_TEST( smmDirtyPageMgr::insDirtyPage( aHeader->mSpaceID, SM_MAKE_PID( sVCPieceOID ) )
                      != IDE_SUCCESS);

            sVCPieceOID = sVCPieceHeader->nxtPieceOID;
        }
    }

    if (aTableInfoPtr != NULL)
    {
        /* Record�� ���� �߰��Ǿ����Ƿ� Record Count�� ���������ش�.*/
        smLayerCallback::incRecCntOfTableInfo( aTableInfoPtr );
    }

    
    //PROJ-2419 insert UnitedVarColumn FitTest: after log write
    IDU_FIT_POINT( "2.PROJ-2419@smcRecord::insertVersion::after_log_write" );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID, sPageID)
             != IDE_SUCCESS);

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

    IDE_PUSH();

    IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                             sPageID) == IDE_SUCCESS);

    IDE_POP();

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
IDE_RC smcRecord::isUpdatedVersionBySameStmt( void            * aTrans,
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
    IDE_TEST( smmManager::holdPageXLatch( aHeader->mSpaceID, sPageID )
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
    IDE_TEST( smmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( smmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
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
IDE_RC smcRecord::checkUpdatedVersionBySameStmt( void             * aTrans,
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
            // BUGBUG : lock row �� ���ÿ� �ٸ� stmt�� ��������??
            IDE_DASSERT( 0 );

            /* aRow�� ���� Lock�� aTrans�� �̹� ��Ҵ�.*/
            /* BUG-39399, BUG-39507
             * 1. aInfiniteSCN�� sNxtRowSCN�� InfiniteSCN �� ���ٸ�,
             *    ���� statement���� update �� ����̴�.
             * 2. sNxtRowSCN�� aInfiniteSCN + SM_SCN_INF_DELETE_BIT �̸�
             *    ���� statement���� delete �� ����̴�. */
            if( SM_SCN_IS_EQ( &sNxtRowSCN, &aInfiniteSCN ) ||
                SM_SCN_IS_EQ( &sNxtRowSCN, &sInfiniteSCNWithDeleteBit ) )
            {
                // BUGBUG : limitSCN �� (FREE) -> (LOCK | TID) -> (INFINITE | TID) �� ����Ǵµ�
                //          ����ϸ� LOCK �ε� INFINITE �ϱ��... 
                IDE_DASSERT( 0 ); 

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
 * aOldRID       - [IN] None
 * aRow          - [OUT] Update���� ������� New Version�� Row Pointer�� �Ѱ��ش�.
 * aRetSlotRID   - [IN] None
 * aColumnList   - [IN] Update�� Column List
 * aValueList    - [IN] Update�� Column���� ���ο� Vaule List
 * aRetryInfo    - [IN] Retry�� ���� ������ column�� list
 * aInfinite     - [IN] New Version�� Row SCN
 * aModifyIdxBit - [IN] None
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC smcRecord::updateVersion( idvSQL               * /* aStatistics */,
                                 void                 * aTrans,
                                 smSCN                  aViewSCN,
                                 void                 * /*aTableInfoPtr*/,
                                 smcTableHeader       * aHeader,
                                 SChar                * aOldRow,
                                 scGRID                 /* aOldGRID */,
                                 SChar               ** aRow,
                                 scGRID               * aRetSlotGRID,
                                 const smiColumnList  * aColumnList,
                                 const smiValue       * aValueList,
                                 const smiDMLRetryInfo* aRetryInfo,
                                 smSCN                  aInfinite,
                                 void                 * /*aLobInfo4Update*/,
                                 ULong                  /*aModifyIdxBit */,
                                 idBool                 aForbiddenToRetry )  
{
    return updateVersionInternal( aTrans,
                                  aViewSCN,
                                  aHeader,
                                  aOldRow,
                                  aRow,
                                  aRetSlotGRID,
                                  aColumnList,
                                  aValueList,
                                  aRetryInfo,
                                  aInfinite,
                                  SMC_UPDATE_BY_TABLECURSOR,
                                  aForbiddenToRetry );
}

/***********************************************************************
 * Description : aOldRow�� Update�Ѵ�. Update�� MMDB�� MVCC�� ���ؼ�
 *               ���ο� Version�� ����� ����. Update�� ������ �Ǹ�
 *               aOldRow�� ���ο� Version�� ����Ű�� �ȴ�.
 *
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aHeader       - [IN] aOldRow�� ���� �ִ� Table Header Pointer
 * aOldRow       - [IN] Update�� Row.
 * aRow          - [OUT] Update���� ������� New Version�� Row Pointer�� �Ѱ��ش�.
 * aRetSlotGRID  - [IN] None
 * aColumnList   - [IN] Update�� Column List
 * aValueList    - [IN] Update�� Column���� ���ο� Vaule List
 * aRetryInfo    - [IN] Retry�� ���� ������ column�� list
 * aInfinite     - [IN] New Version�� Row SCN
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC smcRecord::updateVersionInternal( void                 * aTrans,
                                         smSCN                  aViewSCN,
                                         smcTableHeader       * aHeader,
                                         SChar                * aOldRow,
                                         SChar               ** aRow,
                                         scGRID               * aRetSlotGRID,
                                         const smiColumnList  * aColumnList,
                                         const smiValue       * aValueList,
                                         const smiDMLRetryInfo* aRetryInfo,
                                         smSCN                  aInfinite,
                                         smcUpdateOpt           aOpt,
                                         idBool                 aForbiddenToRetry )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;
    const smiValue        * sCurValue;
    SChar                 * sOldFixRowPtr = aOldRow;
    SChar                 * sNewFixRowPtr = NULL;
    smOID                   sOldFixRowOID;
    smOID                   sNewFixRowOID;
    scPageID                sOldPageID = SC_NULL_PID;
    scPageID                sNewPageID = SC_NULL_PID;
    UInt                    sAftImageSize;
    UInt                    sBfrImageSize;
    UInt                    sState = 0;
    UInt                    sColumnSeq;
    smpPersPageHeader     * sPagePtr;

    vULong                  sFixedRowSize;
    smpSlotHeader         * sOldSlotHeader = NULL;

    smOID                   sFstVCPieceOID;
    idBool                  sIsReplSenderSend;
    idBool                  sIsLockRow      = ID_FALSE;
    smcLobDesc            * sCurLobDesc;
    smTID                   sTransID;

    UInt                    sUnitedVarColumnCount   = 0;
    UInt                    sLogVarCount            = 0;
    const smiColumn       * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    smiValue                sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];

    // BUG-47366 UnitedVar�� Alloc Log ����
    smOID                   sVCPieceOID;
    SChar                 * sVCPiecePtr    = NULL;
    smVCPieceHeader       * sVCPieceHeader = NULL;

    idBool                  sIsLockedNext = ID_FALSE;

    IDE_ERROR( aTrans  != NULL );
    IDE_ERROR( aOldRow != NULL );
    IDE_ERROR( aHeader != NULL );

    IDU_FIT_POINT( "1.PROJ-2118@smcRecord::updateVersionInternal" );

    /* BUGBUG: By Newdaily
       aColumnList�� aValueList�� NULL�� ��찡 ������.
       �̿� ���� ������ �ʿ��մϴ�.

       �߰�: By Clee
       PRJ-1362 LOB���� LOB update�ÿ� fixed-row�� ���� versioning����
       NULL�� ���ͼ� ��ü �÷��� ��� ������.

       IDE_DASSERT( aColumnList != NULL );
       IDE_DASSERT( aValueList != NULL );
    */

    /* !!Fixed Row�� Update�� ������ New Version�� ��������� */

    sOldFixRowPtr = aOldRow;

    /* Transaction Log Buffer�� �ʱ�ȭ�Ѵ�. */
    smLayerCallback::initLogBuffer( aTrans );
    sOldSlotHeader = (smpSlotHeader*)sOldFixRowPtr;
    sOldPageID     = SMP_SLOT_GET_PID(aOldRow);
    sOldFixRowOID  = SM_MAKE_OID( sOldPageID,
                                  SMP_SLOT_GET_OFFSET( sOldSlotHeader ) );

    sIsReplSenderSend =  smcTable::needReplicate( aHeader, aTrans );

    if( aColumnList == NULL )
    {
        /* BUG-15718: Replication Sender�� log�м��� ���� ���:
           Sender�� �о �������� �α��� Update log�̰�  Before Image Size�� 0�̸�
           ������ �׵����Ͽ���. ������ �̷��� Column�� ���� Update���� ����
           New Version�� ����� Update�α״� Sender�� Skip�ϵ��� �Ѵ�. */
        sIsReplSenderSend = ID_FALSE;
    }

    IDU_FIT_POINT( "1.BUG-42154@smcRecord::updateVersionInternal::beforelock" );

    /* update�ϱ� ���Ͽ� old version�� ���� page�� ���Ͽ�
       holdPageXLatch  */
    IDE_TEST( smmManager::holdPageXLatch( aHeader->mSpaceID, sOldPageID )
              != IDE_SUCCESS );
    sState = 1;

    /* sOldFixRowPtr�� �ٸ� Transaction�� �̹� Update�ߴ��� ����.*/
    IDE_TEST( recordLockValidation( aTrans,
                                    aViewSCN,
                                    aHeader,
                                    &sOldFixRowPtr,
                                    ID_ULONG_MAX,
                                    &sState,
                                    aRetryInfo,
                                    aForbiddenToRetry )
              != IDE_SUCCESS );

    // PROJ-1784 DML without retry
    // sOldFixRowPtr�� ���� �� �� �����Ƿ� ���� �ٽ� �����ؾ� ��
    // ( �̰�� page latch �� �ٽ� ����� )
    sOldSlotHeader = (smpSlotHeader*)sOldFixRowPtr;
    sOldPageID     = SMP_SLOT_GET_PID( sOldFixRowPtr );
    sOldFixRowOID  = SM_MAKE_OID( sOldPageID,
                                  SMP_SLOT_GET_OFFSET( sOldSlotHeader ) );

    /* BUG-33738 [SM] undo of SMC_PERS_UPDATE_VERSION_ROW log is wrong
     * �̹� lock row �� ��� update�� ���� rollback�� lock bit��
     * �������־�� �Ѵ�.
     * ���� �̹� lock bit�� ������ ������� Ȯ���Ѵ�. */
    if ( SMP_SLOT_IS_LOCK_TRUE(sOldSlotHeader) )
    {
        sIsLockRow = ID_TRUE;
    }
    else
    {
        sIsLockRow = ID_FALSE;

        /* sSlotHeader == aOldRow == sOldFixRowPtr
         * TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log
         *                      can be reduced to 1.
         * RecordLock�� ���� mNext�� Lock�� �ӽ÷� �����Ѵ�.
         * update �α� �� mNext�� NewVersion RowOID�� �����Ѵ�. */
        sTransID = smLayerCallback::getTransID( aTrans );
        SMP_SLOT_SET_LOCK( sOldSlotHeader, sTransID );
        sIsLockedNext = ID_TRUE;
    }

    sState = 0;
    IDE_TEST( smmManager::releasePageLatch(aHeader->mSpaceID, sOldPageID)
              != IDE_SUCCESS );

    IDU_FIT_POINT( "2.BUG-42154@smcRecord::updateVersionInternal::afterlock" );

    /* �� ������ ������ �Ǹ� Row�� Lock������ �ȴ�. �ֳĸ� Next Version��
       Lock�� ��ұ� �����̴�. ���� �ٸ� Transaction���� �ش� ���ڵ忡
       �����ϰ� �Ǹ� Wait�ȴ�. �׷��� ������ Page Latch ���� Ǯ����.*/

    /* New Version�� �Ҵ��Ѵ�. */
    IDE_TEST( smpFixedPageList::allocSlot(aTrans,
                                          aHeader->mSpaceID,
                                          NULL,
                                          aHeader->mSelfOID,
                                          &(aHeader->mFixed.mMRDB),
                                          &sNewFixRowPtr,
                                          aInfinite,
                                          aHeader->mMaxRow,
                                          SMP_ALLOC_FIXEDSLOT_NONE)
              != IDE_SUCCESS );

    sNewPageID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sNewFixRowOID = SM_MAKE_OID( sNewPageID,
                         SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr) );

    /* Tx OID List�� Old Version �߰�. */
    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       sOldFixRowOID,
                                       aHeader->mSpaceID,
                                       SM_OID_OLD_UPDATE_FIXED_SLOT )
              != IDE_SUCCESS );

    if(aOpt == SMC_UPDATE_BY_TABLECURSOR)
    {
        /* Tx OID List�� New Version �߰�. */
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sNewFixRowOID,
                                           aHeader->mSpaceID,
                                           SM_OID_NEW_UPDATE_FIXED_SLOT )
                  != IDE_SUCCESS );
    }
    else
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
                                           sNewFixRowOID,
                                           aHeader->mSpaceID,
                                           (SM_OID_NEW_UPDATE_FIXED_SLOT & ~SM_OID_ACT_CURSOR_INDEX) |
                                           SM_OID_ACT_AGING_INDEX )
                  != IDE_SUCCESS );
    }

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( smmManager::getPersPagePtr( aHeader->mSpaceID,
                                            sOldPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    IDE_ASSERT( sPagePtr->mTableOID == aHeader->mSelfOID );

    /* �� ������ ������ �Ǹ� Row�� Lock������ �ȴ�. �ֳĸ� Next Version��
       ������ ���� Transaction�� ���� New Version�� �ְ� �Ǹ� �ٸ� Transaction����
       Wait�ϰ� �Ǳ⶧���̴�. �׷��� ������ Page Latch ���� Ǯ����.*/
    sState = 2;

    /* ���ο� ������ ���� ������ �� �� ������� �ʴ� ���� copy.*/
    sFixedRowSize = aHeader->mFixed.mMRDB.mSlotSize - SMP_SLOT_HEADER_SIZE;

    idlOS::memcpy( sNewFixRowPtr + SMP_SLOT_HEADER_SIZE,
                   sOldFixRowPtr + SMP_SLOT_HEADER_SIZE,
                   sFixedRowSize);

    /* TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log
     *                      can be reduced to 1.
     * Before Image�� OldVersion RowOID(ULong)�� lock row(idBool) ���ΰ� ��ϵȴ�
     * After Image�� OldVersion RowOID�� ��ϵȴ�. */
    sBfrImageSize = ID_SIZEOF(ULong) + ID_SIZEOF(idBool);
    sAftImageSize = sFixedRowSize + ID_SIZEOF(UShort)/*Fixed Row Log Size*/ +
                    ID_SIZEOF(ULong);

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

        IDU_FIT_POINT( "3.PROJ-2118@smcRecord::updateVersionInternal" );

        if( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
            != SMI_COLUMN_COMPRESSION_TRUE )
        {
            switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
            {
                case SMI_COLUMN_TYPE_LOB:

                    sCurLobDesc = (smcLobDesc*)(sNewFixRowPtr + sColumn->offset);

                    // SQL�� ���� LOB ����Ÿ�� ���� ���
                    IDE_TEST( smcLob::reserveSpaceInternal(
                                  aTrans,
                                  aHeader,
                                  (smiColumn*)sColumn,
                                  0, /* aLobVersion */
                                  sCurLobDesc,
                                  0, /* aOffset */
                                  sCurValue->length,
                                  SM_FLAG_INSERT_LOB,
                                  ID_TRUE /* aIsFullWrite */)
                              != IDE_SUCCESS );

                    if( sCurValue->length > 0 )
                    {
                        IDE_TEST( smcLob::writeInternal(
                                      aTrans,
                                      aHeader,
                                      (UChar*)sNewFixRowPtr,
                                      (smiColumn*)sColumn,
                                      0, /* aOffset */
                                      sCurValue->length,
                                      (UChar*)sCurValue->value,
                                      ID_FALSE,  // aIsWriteLog
                                      ID_FALSE,  // aIsReplSenderSend
                                      (smLobLocator)NULL )     // aLobLocator
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Here length is always 0. */

                        if( sCurValue->value  == NULL )
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

                    if( sIsReplSenderSend == ID_TRUE )
                    {
                        /* Dumm Lob VC Log: Column ID(UInt) | Length(UInt) */
                        sBfrImageSize += (ID_SIZEOF(UInt) * 2);
                    }

                    if(sCurLobDesc->flag == SM_VCDESC_MODE_OUT)
                    {
                        sAftImageSize += getVCAMLogSize( sCurValue->length );
                    }

                    break;

                case SMI_COLUMN_TYPE_VARIABLE:
                    sUnitedVarColumns[sUnitedVarColumnCount]       = sColumn;
                    sUnitedVarValues[sUnitedVarColumnCount].length = sCurValue->length;
                    sUnitedVarValues[sUnitedVarColumnCount].value  = sCurValue->value;
                    sUnitedVarColumnCount++;

                    if ( sIsReplSenderSend == ID_TRUE )
                    {
                        /* Before VC Image Header: Column ID(UInt) | Length(UInt)
                           Body: Value */
                        sBfrImageSize += getVCBMLogSize( getColumnLen( sColumn,
                                                                       sOldFixRowPtr ) );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* Geometry Type �Ǵ� VARIABLE_LARGE Ÿ���� ������ ���
                     * UnitedVar�� �ƴ� PROJ-2419 ���� �� Variable ������ ����Ѵ�. */
                    IDE_TEST( deleteVC( aTrans, 
                                        aHeader, 
                                        sColumn, 
                                        sOldFixRowPtr )
                              != IDE_SUCCESS );

                    sFstVCPieceOID = SM_NULL_OID;

                    IDE_TEST( smcRecord::insertLargeVarColumn ( aTrans,
                                                                aHeader,
                                                                sNewFixRowPtr,
                                                                sColumn,
                                                                ID_TRUE /*Add OID*/,
                                                                sCurValue->value,
                                                                sCurValue->length,
                                                                &sFstVCPieceOID )
                              != IDE_SUCCESS );

                    if ( sFstVCPieceOID != SM_NULL_OID )
                    {
                        sAftImageSize += getVCAMLogSize( sCurValue->length );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    if ( sIsReplSenderSend == ID_TRUE )
                    {
                        /* Before VC Image Header: Column ID(UInt) | Length(UInt)
                           Body: Value */
                        sBfrImageSize += getVCBMLogSize( getColumnLen( sColumn,
                                                                       sOldFixRowPtr ) );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    break;

                case SMI_COLUMN_TYPE_FIXED:

                    IDU_FIT_POINT( "2.PROJ-2118@smcRecord::updateVersionInternal" );

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

                    if ( sIsReplSenderSend == ID_TRUE )
                    {
                        sBfrImageSize += getFCMVLogSize( sColumn->size );
                    }

                    break;

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
        else // SMI_COLUMN_COMPRESSION_TRUE
        {
            // PROJ-2264
            idlOS::memcpy( sNewFixRowPtr + sColumn->offset,
                           sCurValue->value,
                           sCurValue->length );

            if ( sIsReplSenderSend == ID_TRUE )
            {
                sBfrImageSize += getFCMVLogSize( ID_SIZEOF(smOID) );
            }
            else
            {
                /* Nothing to do */
            }
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
                                               sUnitedVarColumnCount,
                                               &sAftImageSize )
                    != IDE_SUCCESS );

            sLogVarCount = sUnitedVarColumnCount;

        }
        else
        {
            IDE_TEST( smcRecord::updateUnitedVarColumns( aTrans,
                                                         aHeader,
                                                         sOldFixRowPtr,
                                                         sNewFixRowPtr,
                                                         sUnitedVarColumns,
                                                         sUnitedVarValues,
                                                         sUnitedVarColumnCount,
                                                         &sLogVarCount,
                                                         &sAftImageSize)
                != IDE_SUCCESS );
        }
        /* BUG-47366 Before Image Log�� Old UnitedVar OID�� ���� �Ѵ�.
         * BUG-47723 OID Cnt ��ġ ���� */
        sBfrImageSize += ID_SIZEOF(smOID) ;

        sVCPieceOID = ((smpSlotHeader*)sOldFixRowPtr)->mVarOID;

        if ( sVCPieceOID != SM_NULL_OID )
        {
            /* BUG-47366 �� ��� OID�� ������ ����Ѵ�.
             * BUG-47723 OID Cnt ��ġ ���� */
            sBfrImageSize += ID_SIZEOF(UShort);
 
            do
            {
                sBfrImageSize += ID_SIZEOF(smOID);

                IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                                   sVCPieceOID,
                                                   (void**)&sVCPiecePtr )
                            == IDE_SUCCESS );
                sVCPieceHeader = (smVCPieceHeader*)sVCPiecePtr;
                sVCPieceOID = sVCPieceHeader->nxtPieceOID;
            }
            while ( sVCPieceOID != SM_NULL_OID );
        }
    }
    else /* united var col update�� �߻����� ���� ���, �ΰ����� �ٽ� ��������.  */
    {
        sFstVCPieceOID = ((smpSlotHeader*)sOldFixRowPtr)->mVarOID;
        ((smpSlotHeader*)sNewFixRowPtr)->mVarOID = sFstVCPieceOID;

        if ( sFstVCPieceOID == SM_NULL_OID )    /* �ƿ� united var col�� ����*/
        {
            sAftImageSize += ID_SIZEOF(smOID); /* NULL OID logging */

            /* BUG-47366 Before Image Log�� UnitedVar OID�� ���� �Ѵ�. */
            sBfrImageSize += ID_SIZEOF(smOID);
        }
        else                                    /* united var col �� ������ �� update�� �ش���� �ʾҴ�. */
        {                                       /* redo �ÿ� �����Ͽ� ó���ϱ����� OID�� �ְ� count 0 �� �α��Ѵ� */
            sAftImageSize += ID_SIZEOF(smOID) + ID_SIZEOF(UShort); /* OID + zero Count */
            /* BUG-47366 Before Image Log�� UnitedVar OID�� ���� �Ѵ�. */
            sBfrImageSize += ID_SIZEOF(smOID) + ID_SIZEOF(UShort);
        }
    }

    //PROJ-2419 update UnitedVarColumn FitTest: before log write
    IDU_FIT_POINT( "1.PROJ-2419@smcRecord::updateVersion::before_log_write" );

    IDE_TEST( smcRecordUpdate::writeUpdateVersionLog(
                  aTrans,
                  aHeader,
                  aColumnList,
                  sIsReplSenderSend,
                  sOldFixRowOID,
                  sOldFixRowPtr,
                  sNewFixRowPtr,
                  sIsLockRow,
                  sBfrImageSize,
                  sAftImageSize,
                  sLogVarCount ) //aHeader->mUnitedVarColumnCount �� ��ü�ϰ� ��������
              != IDE_SUCCESS );

    //PROJ-2419 update UnitedVarColumn FitTest: after log write
    IDU_FIT_POINT( "2.PROJ-2419@smcRecord::updateVersion::after_log_write" );

    IDL_MEM_BARRIER;

    /* sOldFixRowPtr�� Next Version�� New Version���� Setting.*/
    SMP_SLOT_SET_NEXT_OID( sOldSlotHeader, sNewFixRowOID );
    SM_SET_SCN( &(sOldSlotHeader->mLimitSCN), &aInfinite );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID, sOldPageID)
              != IDE_SUCCESS );

    /* BUG-14513: Fixed Slot�� �Ҵ�� Alloc Slot Log�� �������
       �ʵ��� ����. Update log�� Alloc Slot�� ���� Redo, Undo����.
       �� ������ Update log��� �� Slot header�� ���� Update����
    */
    smpFixedPageList::setAllocatedSlot( aInfinite, sNewFixRowPtr );

    // BUG-47366 UnitedVar�� Alloc Log ����
    // insert UnitedVar ���� �߻��� alloc Slot���� Slot Header�� �ʱ�ȭ �ȵ�����
    // �ʱ�ȭ ���־�� �Ѵ�.
    if ( sUnitedVarColumnCount > 0 )
    {
        sVCPieceOID = ((smpSlotHeader*)sNewFixRowPtr)->mVarOID;
        while ( sVCPieceOID != SM_NULL_OID )
        {
            IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                               sVCPieceOID,
                                               (void**)&sVCPiecePtr )
                        == IDE_SUCCESS );
            sVCPieceHeader = (smVCPieceHeader*)sVCPiecePtr;
            
            sVCPieceHeader->flag &= ~SM_VCPIECE_FREE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_FREE_NO;
            sVCPieceHeader->flag &= ~SM_VCPIECE_TYPE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_TYPE_OTHER;

            IDE_TEST( smmDirtyPageMgr::insDirtyPage( aHeader->mSpaceID, SM_MAKE_PID( sVCPieceOID ) )
                      != IDE_SUCCESS);

            sVCPieceOID = sVCPieceHeader->nxtPieceOID;
        }

        /* BUG-47366 deleteVC�� Log�� �����ϱ� ����
         * UpdateVersion Log�� ��ϵ� ���Ŀ� Old Version�� UnitedVar�� deleteVC�� �����Ѵ�. */
        IDE_TEST( deleteVC( aTrans, 
                            aHeader, 
                            ((smpSlotHeader*)sOldFixRowPtr)->mVarOID ,
                            ID_FALSE ) );
    }

    *aRow = (SChar*)sNewFixRowPtr;
    if(aRetSlotGRID != NULL)
    {
        SC_MAKE_GRID( *aRetSlotGRID,
                      aHeader->mSpaceID,
                      sNewPageID,
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );
    }

    sState = 0;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                            sNewPageID) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INVALID_COLUMN_SIZE) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ ��
    //Aging�� �и��� ������ �� ��ȭ��.
    //update�� inconsistency�� view�� ���� retry�� Ƚ��
    switch( ideGetErrorCode() )
    {
        case smERR_RETRY_Already_Modified:
        {
            SMX_INC_SESSION_STATISTIC( aTrans,
                                       IDV_STAT_INDEX_UPDATE_RETRY_COUNT,
                                       1 /* Increase Value */ );

            smcTable::incStatAtABort( aHeader, SMC_INC_RETRY_CNT_OF_UPDATE );
        }
        break;
        case smERR_RETRY_Row_Retry :

            IDE_ASSERT( lockRowInternal( aTrans,
                                         aHeader,
                                         sOldFixRowPtr ) == IDE_SUCCESS );

            *aRow = sOldFixRowPtr;

            break;
        default:
            break;
    }

    if ( sIsLockedNext == ID_TRUE )
    {
        SMP_SLOT_SET_UNLOCK( sOldSlotHeader );
    }

    switch(sState)
    {
        case 2:
            IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                                     sNewPageID)
                       == IDE_SUCCESS);
            break;

        case 1:
            // PROJ-1784 DML without retry
            // sOldFixRowPtr�� ���� �� �� �����Ƿ� PID�� �ٽ� �����;� ��
            sOldPageID = SMP_SLOT_GET_PID( sOldFixRowPtr );

            IDE_ASSERT(smmManager::releasePageLatch(aHeader->mSpaceID,
                                                    sOldPageID)
                       == IDE_SUCCESS);
            IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                                     sOldPageID)
                       == IDE_SUCCESS);
            break;
        default:
            break;
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
IDE_RC smcRecord::updateUnitedVarColumns( void                * aTrans,
                                          smcTableHeader      * aHeader,
                                          SChar               * aOldRow,
                                          SChar               * aNewRow,
                                          const smiColumn    ** aColumns,
                                          smiValue            * aValues,
                                          UInt                  aVarColumnCount,
                                          UInt                * aLogVarCount,
                                          UInt                * aImageSize )
{
    const smiColumn   * sCurColumn  = NULL;
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

    IDE_ASSERT( aTrans          != NULL );
    IDE_ASSERT( aHeader         != NULL );
    IDE_ASSERT( aOldRow         != NULL );
    IDE_ASSERT( aNewRow         != NULL );
    IDE_ASSERT( aColumns        != NULL );
    IDE_ASSERT( aValues         != NULL );
    IDE_ASSERT( aLogVarCount    != NULL );
    IDE_ASSERT( aImageSize      != NULL );

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
                                      sUnitedVarColCnt,
                                      aImageSize )
            != IDE_SUCCESS );

    *aLogVarCount = sUnitedVarColCnt;

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
IDE_RC smcRecord::updateInplace(idvSQL                  * /*aStatistics*/,
                                void                    * aTrans,
                                smSCN                     /*aViewSCN*/,
                                void                    * /*aTableInfoPtr*/,
                                smcTableHeader          * aHeader,
                                SChar                   * aRowPtr,
                                scGRID                    /*aOldGRID*/,
                                SChar                  ** aRow,
                                scGRID                  * aRetSlotGRID,
                                const smiColumnList     * aColumnList,
                                const smiValue          * aValueList,
                                const smiDMLRetryInfo * /*aRetryInfo */,
                                smSCN                   /*aInfinite*/,
                                void                  * /*aLobInfo4Update*/,
                                ULong                   aModifyIdxBit,
                                idBool                  /*aForbiddenToRetry*/ )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;
    const smiValue        * sCurValue;
    smOID                   sFstVCPieceOID = SM_NULL_OID;
    smVCDesc              * sCurVCDesc;
    smcLobDesc            * sCurLobDesc;
    smOID                   sFixOID;
    UInt                    sState  = 0;
    scPageID                sPageID = SM_NULL_PID ;
    ULong                   sRowBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];
    SChar                 * sRowPtrBuffer;
    SInt                    sStoreMode;
    UInt                    sColumnSeq;
    UInt                    sUnitedVarColumnCount = 0;
    UInt                    sDummyCount     = 0;
    UInt                    sDummySize      = 0;
    const smiColumn       * sUnitedVarColumns[SMI_COLUMN_ID_MAXIMUM];
    smiValue                sUnitedVarValues[SMI_COLUMN_ID_MAXIMUM];

    // BUG-47366 UnitedVar�� Alloc Log ����
    smOID                   sVCPieceOID;
    SChar                 * sVCPiecePtr    = NULL;
    smVCPieceHeader       * sVCPieceHeader = NULL;

    IDE_ERROR( aTrans      != NULL );
    IDE_ERROR( aHeader     != NULL );
    IDE_ERROR( aRowPtr     != NULL );
    IDE_ERROR( aRow        != NULL );
    IDE_ERROR( aColumnList != NULL );
    IDE_ERROR( aValueList  != NULL );

    sRowPtrBuffer = (SChar*)sRowBuffer;

    smLayerCallback::initLogBuffer( aTrans );

    sPageID = SMP_SLOT_GET_PID(aRowPtr);

    /* Fixed Row�� Row ID�� �����´�. */
    sFixOID = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowPtr ) );

    sCurColumnList = aColumnList;
    sCurValue      = aValueList;

    /* BUG-29424
       [valgrind] smcLob::writeInternal() ���� valgrind ������ �߻��մϴ�.
       sRowPtrBuffer�� smpSlotHeader�� �����ؾ� �մϴ�. */
    idlOS::memcpy(sRowPtrBuffer,
                  aRowPtr,
                  ID_SIZEOF(smpSlotHeader));

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

        if( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
            != SMI_COLUMN_COMPRESSION_TRUE )
        {
            switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
            {
                case SMI_COLUMN_TYPE_LOB:

                    sCurLobDesc = (smcLobDesc*)(sRowPtrBuffer + sColumn->offset);

                    idlOS::memcpy(sCurLobDesc,
                                  aRowPtr + sColumn->offset,
                                  ID_SIZEOF(smcLobDesc));

                    if ( SM_VCDESC_IS_MODE_IN(sCurLobDesc) )
                    {
                        /* ����Ÿ�� In-Mode�� ����Ǿ��� ��� ����Ÿ�� Fixed������
                           ����Ǿ��ִ�. */
                        idlOS::memcpy( (SChar*)sCurLobDesc + ID_SIZEOF(smVCDescInMode),
                                       aRowPtr + sColumn->offset + ID_SIZEOF(smVCDescInMode),
                                       sCurLobDesc->length );
                    }

                    /* smcLob::prepare4WriteInternal���� sCurLobDesc�� �ٲܼ� �ֱ⶧����
                       ����Ÿ�� �����ؼ� �Ѱܾ� �Ѵ�. �ֳ��ϸ� Update Inplace�� �ϱ⶧����
                       ����Ÿ������ ���� ������ �ϱ����� �α׸� ���� ����ؾ��ϳ� ����� �α׸�
                       ������� �ʰ� ���Ŀ� ��ϵǱ⶧���̴�.
                    */
                    // SQL�� ���� LOB ����Ÿ�� ���� ���
                    IDE_TEST( smcLob::reserveSpaceInternal( aTrans,
                                                            aHeader,
                                                            (smiColumn*)sColumn,
                                                            0, /* aLobVersion */
                                                            sCurLobDesc,
                                                            0, /* aOffset */
                                                            sCurValue->length,
                                                            SM_FLAG_INSERT_LOB,
                                                            ID_TRUE /* aIsFullWrite */)
                              != IDE_SUCCESS );

                    if( sCurValue->length > 0 )
                    {
                        IDE_TEST( smcLob::writeInternal(
                                      aTrans,
                                      aHeader,
                                      (UChar*)sRowPtrBuffer,
                                      (smiColumn*)sColumn,
                                      0, /* aOffset */
                                      sCurValue->length,
                                      (UChar*)sCurValue->value,
                                      ID_FALSE,  // aIsWriteLog
                                      ID_FALSE,  // aIsReplSenderSend
                                      smLobLocator(NULL) )     // aLobLocator
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
                    sUnitedVarColumns[sUnitedVarColumnCount]       = sColumn;
                    sUnitedVarValues[sUnitedVarColumnCount].length = sCurValue->length;
                    sUnitedVarValues[sUnitedVarColumnCount].value  = sCurValue->value;
                    sUnitedVarColumnCount++;

                    break;

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* Geometry Type �Ǵ� VARIABLE_LARGE Ÿ���� ������ ���
                     * UnitedVar�� �ƴ� PROJ-2419 ���� �� Variable ������ ����Ѵ�. */
                    IDE_TEST( deleteVC( aTrans, 
                                        aHeader, 
                                        sColumn, 
                                        aRowPtr )
                              != IDE_SUCCESS );

                    IDE_TEST( smcRecord::insertLargeVarColumn ( aTrans,
                                                                aHeader,
                                                                sRowPtrBuffer,
                                                                sColumn,
                                                                ID_TRUE /*Add OID*/,
                                                                sCurValue->value,
                                                                sCurValue->length,
                                                                &sFstVCPieceOID )
                              != IDE_SUCCESS );

                    break;

                case SMI_COLUMN_TYPE_FIXED:
                default:
                    break;
            }
        }
        else
        {
            // PROJ-2264
            // nothing to do
        }
    }

    if ( sUnitedVarColumnCount > 0 )
    {
        IDE_TEST( smcRecord::updateUnitedVarColumns( aTrans,
                                                     aHeader,
                                                     aRowPtr,
                                                     sRowPtrBuffer,
                                                     sUnitedVarColumns,
                                                     sUnitedVarValues,
                                                     sUnitedVarColumnCount,
                                                     &sDummyCount,
                                                     &sDummySize )
                != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( smcRecordUpdate::writeUpdateInplaceLog( aTrans,
                                                      aHeader,
                                                      aRowPtr,
                                                      aColumnList,
                                                      aValueList,
                                                      sRowPtrBuffer,
                                                      aModifyIdxBit )
              != IDE_SUCCESS );
    sState = 1;

    if ( aModifyIdxBit != 0)
    {
        /* Table�� Index���� aRowPtr�� �����. */
        IDE_TEST( smLayerCallback::deleteRowFromIndex( aRowPtr,
                                                       aHeader,
                                                       aModifyIdxBit )
                  != IDE_SUCCESS );
    }
    else
    {
        /* do nothing */
    }
    
    sCurColumnList = aColumnList;
    sCurValue      = aValueList;

    for( sColumnSeq = 0 ;
         sCurColumnList != NULL;
         sCurColumnList  = sCurColumnList->next, sCurValue++, sColumnSeq++ )
    {
        sColumn = sCurColumnList->column;

        if( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
            != SMI_COLUMN_COMPRESSION_TRUE )
        {
            switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
            {
                case SMI_COLUMN_TYPE_LOB:

                    /* Fixed Row�� ���ο� LobDesc�� ����Ű���� �Ѵ�.*/
                    sCurLobDesc = (smcLobDesc*)(sRowPtrBuffer + sColumn->offset);

                    sStoreMode = sCurLobDesc->flag & SM_VCDESC_MODE_MASK;

                    if( sStoreMode == SM_VCDESC_MODE_OUT )
                    {
                        idlOS::memcpy(aRowPtr + sColumn->offset,
                                      sCurLobDesc,
                                      ID_SIZEOF(smcLobDesc));
                    }
                    else
                    {
                        IDE_ASSERT_MSG( sStoreMode == SM_VCDESC_MODE_IN,
                                        "Table OID  : %"ID_vULONG_FMT"\n"
                                        "Space ID   : %"ID_UINT32_FMT"\n"
                                        "Page  ID   : %"ID_UINT32_FMT"\n"
                                        "Column seq : %"ID_UINT32_FMT"\n"
                                        "Store Mode : %"ID_UINT32_FMT"\n",
                                        aHeader->mSelfOID,
                                        aHeader->mSpaceID,
                                        sPageID,
                                        sColumnSeq,
                                        sStoreMode );

                        /* In - Mode�� ��� Value�� sRowPtrBuffer�� ����Ǿ� �ִ�.*/
                        idlOS::memcpy(aRowPtr + sColumn->offset,
                                      sCurLobDesc,
                                      ID_SIZEOF(smVCDescInMode) + sCurLobDesc->length);
                    }

                    break;

                case SMI_COLUMN_TYPE_VARIABLE:
                    break;

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* Geometry Type �Ǵ� VARIABLE_LARGE Ÿ���� ������ ���
                     * UnitedVar�� �ƴ� PROJ-2419 ���� �� Variable ������ ����Ѵ�. */
                    /* Fixed Row�� ���ο� VC�� ����Ű���� �Ѵ�.*/
                    sCurVCDesc = (smVCDesc*)(sRowPtrBuffer + sColumn->offset);

                    sStoreMode = sCurVCDesc->flag & SM_VCDESC_MODE_MASK;

                    if ( sStoreMode == SM_VCDESC_MODE_OUT )
                    {
                        idlOS::memcpy( aRowPtr + sColumn->offset,
                                       sCurVCDesc,
                                       ID_SIZEOF(smVCDesc) );
                    }
                    else
                    {
                        IDE_ASSERT_MSG( sStoreMode == SM_VCDESC_MODE_IN,
                                        "Table OID  : %"ID_vULONG_FMT"\n"
                                        "Space ID   : %"ID_UINT32_FMT"\n"
                                        "Page  ID   : %"ID_UINT32_FMT"\n"
                                        "Column seq : %"ID_UINT32_FMT"\n"
                                        "Store Mode : %"ID_UINT32_FMT"\n",
                                        aHeader->mSelfOID,
                                        aHeader->mSpaceID,
                                        sPageID,
                                        sColumnSeq,
                                        sStoreMode );

                        /* In - Mode�� ��� Value�� sRowPtrBuffer�� ����Ǿ� �ִ�.*/
                        idlOS::memcpy(aRowPtr + sColumn->offset,
                                      sCurVCDesc,
                                      ID_SIZEOF(smVCDescInMode) + sCurVCDesc->length);
                    }
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
        else
        {
            // PROJ-2264
            idlOS::memcpy(aRowPtr + sColumn->offset,
                          sCurValue->value,
                          sCurValue->length);
        }
    }

    // BUG-47366 UnitedVar�� Alloc Log ����
    // insert UnitedVar ���� �߻��� alloc Slot���� Slot Header�� �ʱ�ȭ �ȵ�����
    // �ʱ�ȭ ���־�� �Ѵ�.
    if ( sUnitedVarColumnCount > 0 )
    {
        sVCPieceOID = ((smpSlotHeader*)sRowPtrBuffer)->mVarOID;
        while ( sVCPieceOID != SM_NULL_OID )
        {
            IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                               sVCPieceOID,
                                               (void**)&sVCPiecePtr )
                        == IDE_SUCCESS );
            sVCPieceHeader = (smVCPieceHeader*)sVCPiecePtr;
            
            sVCPieceHeader->flag &= ~SM_VCPIECE_FREE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_FREE_NO;
            sVCPieceHeader->flag &= ~SM_VCPIECE_TYPE_MASK;
            sVCPieceHeader->flag |= SM_VCPIECE_TYPE_OTHER;

            IDE_TEST( smmDirtyPageMgr::insDirtyPage( aHeader->mSpaceID, SM_MAKE_PID( sVCPieceOID ) )
                      != IDE_SUCCESS);

            sVCPieceOID = sVCPieceHeader->nxtPieceOID;
        }

        /* �������� �������� �ϰ� ���� insert ������ united var �� old value �� �о�;��ϹǷ�
         * ���� �Է��ϰ� �����Ѵ� */
        IDE_TEST( deleteVC( aTrans,
                            aHeader,
                            ((smpSlotHeader*)aRowPtr)->mVarOID,
                            ID_FALSE ) 
                != IDE_SUCCESS );
    }

    /* United Var ī�� */
    ((smpSlotHeader*)aRowPtr)->mVarOID = ((smpSlotHeader*)sRowPtrBuffer)->mVarOID;

    *aRow = (SChar*)aRowPtr;

    if(aRetSlotGRID != NULL)
    {
        SC_MAKE_GRID( *aRetSlotGRID,
                      aHeader->mSpaceID,
                      sPageID,
                      SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowPtr ) );
    }

    sState = 0;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                            sPageID) != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       aHeader->mSelfOID,
                                       sFixOID,
                                       aHeader->mSpaceID,
                                       SM_OID_UPDATE_FIXED_SLOT )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_column_size );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INVALID_COLUMN_SIZE) );
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                                 sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * task-2399 Lock Row�� Delete Row�� ������.
 *
 *
 * *************************
 * ������ ���� ��� �� ������
 * *************************
 * �������� delete row������ �����ϸ�, delete ������ ������ row�� ���ο� ������
 * �����Ͽ� delete ǥ�ø� �� ��, mNext�� �����Ͽ���.
 * ���� row�� delete �Ǿ����� ���δ� mNext�� ����� row�� delete row������ Ȯ��
 * �ϸ� �˼� �־���, �ٸ� Ʈ������� ���� row�� �����ϴ��� ���θ� �����Ҷ���,
 * 'update row������ ���� ������� �������� �ڽ��� �����ϴ� view�� �����ϴ� ��'�� ����
 * ����� ������ �� �־���. rollback�� ����ɶ��� ���� ����� �����ϸ� �Ǿ���.( ����
 * ������ ������ aging)
 *
 * ������ �� ������ �׻� 1���� row�� �������� ���ܵξ�� �߰� (database�� ���� á����
 * ������ �����ϵ���), delete row�� �����Ҷ� ���ο� row�� �����ؾ� �߾���.
 * �̰��� �ҽ��ڵ带 ��ư� ����� ������ �Ǿ���, ���ɿ��� ������ ���ƴ�.  �׷��� delete
 * row������ ����ɶ� ���ο� ����(row)�� �������� �ʰ�, ���� ������ ����Ǵ� row��
 * delete������ ������ �����ϴ� ����� �����Ͽ���.
 *
 * *****************************
 * ���ο� ����� ����
 * *****************************
 * 'delete row������ ����Ǵ� ��'�� 'update row������ ����Ǵ� ��'�� ���� ���������� �ٸ�
 * ���� �ִ�.  �װ��� update row�� ������ ���ο� ������ ���� �ٽ� ���ο� ������ ����
 * �� �� ������, delete row������ ����� row�� ���ؼ��� ���̻� ���ο� ������ ��������
 * �ʴ´ٴ� ���̴�.
 * �׷��� ������ delete row������ ����� row�� mNext�� �ٸ� �뵵�� ����� �� �ְ� �ȴ�.
 * �ֳ��ϸ� ���̻� ���ο� ������ �������� �ʱ� �����̴�. �̰��� 'delete row������ ����� row'
 * �� ���� MVCC�� ������ �� �ֵ��� �ʿ��� ������ �����ϰ�, �� ������ ���õ� row�� delete
 * row ������ ����� row�� �Ǵ��ϸ� �ȴ�.
 *
 * ��, �����ϸ�...
 * ������ ��� : row.mNext = ���ο� ����(delete row)�� OID
 * ���ο� ��� : row.mNext = MVCC�� �����ϱ� ���� �ʿ��� ����.
 *
 * ���ο� ��Ŀ��� MVCC�� �����ϱ� ���� �ʿ��� ������ delete row������ ����� Ʈ�������
 * SCN��, ������ Ʈ������� TID�̴�. �̰��� delete row������ ������ Ʈ�������
 * ���� COMMIT���� �ʾ�����, ���� Ʈ������� ���� row�� delete�Ǿ����� ���θ� �Ǵ��Ҷ�,
 * �Ǵ� COMMIT    �Ͽ��ٸ�, �ٸ� Ʈ������� ���� row�� delete�Ǿ����� ���θ� �Ǵ��Ҷ�,
 * �ʿ��ϴ�.
 *
 * ���ο� ��Ŀ�����
 * mNext�� ������ SCN�� �����ϱ� ���� �������� ����ϰ�, TID�� �����ϱ� ���ؼ��� ����
 * row�� mTID������ �̿��Ѵ�.  �� delete ������ ����Ǵ� row�� mTID�� ����� ������
 * ���� Ʈ��������� ����ġ�� ���̴�.
 *
 * ***************************
 * ���ο� ����� ������ ������ ����
 * ***************************
 * mNext�� OID��ſ� SCN�� ���� ���� ������ ������
 * 1. oid�� �׻� 8byte align�Ǿ��ֱ� ������ ������ 3bit�� �׻� 0�̴�.
 * �׷��� ������ SCN�� �����Ҷ�, ������ 3byte�� 000�� �ƴ� ��쿡 �̰��� SCN���� �ν�
 * �� �� �ְ�, �̰�쿡 ���� row�� delete row������ ����� ������ �� �� �ִ�.
 *
 * ������ mTID������ ���ο� Ʈ������� id�� ����ϴ°��� ������ ������
 * 1. delete ������ �����ϴ� Ʈ������� ���� row�� ����Ǿ��ִ� Ʈ����ǰ� ���� Ʈ�����
 * �̰ų�
 * 2. row�� mTID�� ����� Ʈ������� �̹� ������ �Ϸ�� Ʈ������̴�.(�� ��쿡�� ������
 * mTID ������ �ǹ̰� ����.)
 * �׷��� ������ ���� row�� mTID������ ������ �� �ִ� ���̴�.
 *
 * *********************************
 * ���ο� ��Ŀ��� �߱�� ���ü� ���� ����
 * *********************************
 * ������ ���� �޶��� ���� �߿��� ������, ���ü��� ���õ� �����̴�.
 * write����( update, insert, delete)�� ������ ��ġ�� ��� ������ �����ϱ� ������
 * write���� ������ ���ü� ������ ��Ÿ���� ������,
 * read ����( checkSCN)�� ������ ��ġ�� ���� �ʰ� row�� �б� ������, ������ �����
 * �ִ�.
 *
 * �������� ����Ǵ� ���� ���� mNext����, �̰��� ���ο� ������ �����ǰ� �� ���Ŀ� �ٲ��
 * ���� ���� ������ ������ ���� �ʾҴ�.
 * ������ ������ mTID�� mNext�� ���� ���ϰ�, �������� �ٸ��� mNext�� 32bit �ӽ� ������
 * 64bit�� �ٲ���� ������ (��, mNext�� �ι��� ���ļ� ���Ѵ�. �������� 32bit������
 * 32bit������. ) ���ü��� ������ �Ű��� ���־�� �Ѵ�.
 *
 * �̰��� �ذ��ϱ� ���� row�� write������ �����Ҷ� mTID�� mNext�� �����ϴ� ������
 * �����Ͽ���, readƮ����ǿ����� mTID�� mNext�� �д� ������ �����ϴ� ������ ��������
 * �Ͽ���. mNext�� ������ �о���°Ͱ� ���� �͵� ������ ���߾���. ���ü��� ���õ� �ڼ���
 * ���� ���蹮���� �����ϸ� �ȴ�.
 *
 ***********************************************************************/
/***********************************************************************
 * Description : aRow�� ����Ű�� row�� �����Ѵ�.
 *
 * aStatistics   - [IN] None
 * aTrans        - [IN] Transaction Pointer
 * aViewSCN      - [IN] �� ������ �����ϴ� Cursor�� ViewSCN
 * aTableInfoPtr - [IN] Table Info Pointer(Table�� Transaction�� Insert,
 *                        Delete�� Record Count�� ���� ������ ������ �ִ�.
 * aHeader       - [IN] Table Header Pointer
 * aRow          - [IN] ������ Row
 * aRetSlotGRID  - [IN] None
 * aInfinite     - [IN] Cursor�� Infinite SCN
 * aRetryInfo    - [IN] Retry�� ���� ������ column�� list
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC smcRecord::removeVersion( idvSQL               * /*aStatistics*/,
                                 void                 * aTrans,
                                 smSCN                  aViewSCN,
                                 void                 * aTableInfoPtr,
                                 smcTableHeader       * aHeader,
                                 SChar                * aRow,
                                 scGRID                /*aSlotGRID*/,
                                 smSCN                  aInfinite,
                                 const smiDMLRetryInfo* aRetryInfo,
                                 idBool                 aIsDequeue,   
                                 idBool                 aForbiddenToRetry )
{
    const smiColumn   * sColumn;
    UInt                sLockState = 0;
    scPageID            sPageID;
    UInt                i;
    smOID               sRemoveOID;
    smpPersPageHeader * sPagePtr;
    idBool              sImplFlagChange = ID_FALSE;
    UInt                sColumnCnt;
    smcMakeLogFlagOpt   sMakeLogOpt = SMC_MKLOGFLAG_SET_ALLOC_FIXED_NO;
    ULong               sDeleteSCN;
    ULong               sNxtSCN;
    smTID               sNxtTID;
    smpSlotHeader     * sSlotHeader = (smpSlotHeader*)aRow;

    IDE_ERROR( aRow    != NULL );
    IDE_ERROR( aTrans  != NULL );
    IDE_ERROR( aHeader != NULL );
    
    IDU_FIT_POINT( "1.PROJ-2118@smcRecord::removeVersion" );

    sPageID = SMP_SLOT_GET_PID( aRow );
    sRemoveOID = SM_MAKE_OID( sPageID,
                              SMP_SLOT_GET_OFFSET( sSlotHeader ) );
    SMX_GET_SCN_AND_TID( sSlotHeader->mLimitSCN, sNxtSCN, sNxtTID );

    /* BUG-48230: �ڽ��� �̹� lock row�� ������� record lock validation
     *            �� �ʿ䰡 ����. */
    if (( SM_SCN_IS_NOT_LOCK_ROW( sNxtSCN ) ) ||
        ( smxTrans::getTransID( aTrans ) != sNxtTID ))
    {
        /*  remove�ϱ� ���Ͽ� �� row�� ���� page�� ���Ͽ� holdPageXLatch */
        IDE_TEST( smmManager::holdPageXLatch( aHeader->mSpaceID,
                                              sPageID ) != IDE_SUCCESS );
        sLockState = 1;

        IDE_TEST( recordLockValidation( aTrans,
                                        aViewSCN,
                                        aHeader,
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
        sPageID = SMP_SLOT_GET_PID( aRow );
        sRemoveOID = SM_MAKE_OID( sPageID,
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

        smpFixedPageList::dumpSlotHeader( sSlotHeader );

        IDE_ASSERT( 0 );
    }

    //slotHeader�� mNext�� �� ���� �����Ѵ�. delete������ ����Ǹ�
    //�� ������ �����ϴ� Ʈ������� aInfinite������ ���õȴ�. (with deletebit)
    SM_GET_SCN( &sDeleteSCN, &aInfinite );
    SM_SET_SCN_DELETE_BIT( &sDeleteSCN );

    /* BUG-48230: deQueue�� ��� row�� lock�� �Ǿ� �ִ�. undo�ÿ� �� lock row��
     *            �������־�� �Ѵ�. */
    if ( aIsDequeue == ID_FALSE )
    {
        SM_SET_SCN( &sNxtSCN,  &( sSlotHeader->mLimitSCN ) );
    }
    else
    {
        SM_SET_SCN_FREE_ROW( &sNxtSCN );
    }

    /* remove�� target�� �Ǵ� row�� �׻� mNext�� ���̰� �ִ�. */
    IDE_TEST( smcRecordUpdate::writeRemoveVersionLog(
                  aTrans,
                  aHeader,
                  aRow,
                  sNxtSCN,
                  sDeleteSCN,
                  sMakeLogOpt,
                  &sImplFlagChange)
              != IDE_SUCCESS );


    SM_SET_SCN( &(sSlotHeader->mLimitSCN), &sDeleteSCN );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                            sPageID) != IDE_SUCCESS );

    if ( sLockState == 1 )
    {
        IDE_TEST( smmManager::releasePageLatch(aHeader->mSpaceID, sPageID)
                  != IDE_SUCCESS );
        sLockState = 0;
    }
 
    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * Page�� ��ϵ� TableOID�� ���κ��� ������ TableOID�� ���Ͽ� ������ */
    IDE_ASSERT( smmManager::getPersPagePtr( aHeader->mSpaceID,
                                            sPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    IDE_ASSERT( sPagePtr->mTableOID == aHeader->mSelfOID );

    sColumnCnt = aHeader->mColumnCount;

    for(i=0 ; i < sColumnCnt; i++ )
    {
        sColumn = smcTable::getColumn(aHeader, i);

        if( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
            != SMI_COLUMN_COMPRESSION_TRUE )
        {
            switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
            {
                case SMI_COLUMN_TYPE_LOB:
                    /* LOB�� ���� Piece���� Delete Flag�� Delete�Ǿ��ٰ�
                       �����Ѵ�.*/
                    IDE_TEST( deleteLob( aTrans, 
                                         aHeader,
                                         sColumn,
                                         aRow,
                                         ID_FALSE )
                              != IDE_SUCCESS );

                    break;

                case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                    /* Geometry Type �Ǵ� VARIABLE_LARGE Ÿ���� ������ ���
                     * UnitedVar�� �ƴ� PROJ-2419 ���� �� Variable ������ ����Ѵ�. */
                    IDE_TEST( deleteVC( aTrans, 
                                        aHeader, 
                                        sColumn,
                                        aRow,
                                        ID_FALSE )
                              != IDE_SUCCESS );
                    break;

                case SMI_COLUMN_TYPE_VARIABLE:
                case SMI_COLUMN_TYPE_FIXED:
                default:
                    break;
            }
        }
        else
        {
            // PROJ-2264
            // Nothing to do
        }
    }

    IDE_TEST( deleteVC( aTrans,
                        aHeader,
                        ((smpSlotHeader*)aRow)->mVarOID, 
                        ID_FALSE) 
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
    IDE_PUSH();

    //BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //Aging�� �и��� ������ �� ��ȭ��
    //remove�� inconsistency�� view�� ���� retry�� Ƚ��.
    if( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( aTrans,
                                   IDV_STAT_INDEX_DELETE_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( aHeader, SMC_INC_RETRY_CNT_OF_DELETE );
    }

    if( sLockState != 0 )
    {
        // PROJ-1784 DML without retry
        // aRow�� ���� �� �� �����Ƿ� PID�� �ٽ� �����;� ��
        sPageID = SMP_SLOT_GET_PID( aRow );

        IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID,
                                                 sPageID) == IDE_SUCCESS);
        IDE_ASSERT(smmManager::releasePageLatch(aHeader->mSpaceID,
                                                sPageID) == IDE_SUCCESS);
    }
    IDE_POP();


    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC smcRecord::nextOIDall( smcTableHeader * aHeader,
                              SChar          * aCurRow,
                              SChar         ** aNxtRow )
{

    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aNxtRow != NULL );

    return smpAllocPageList::nextOIDall( aHeader->mSpaceID,
                                         aHeader->mFixedAllocList.mMRDB,
                                         aCurRow,
                                         aHeader->mFixed.mMRDB.mSlotSize,
                                         aNxtRow );
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC smcRecord::freeVarRowPending( void            * aTrans,
                                     smcTableHeader  * aHeader,
                                     smOID             aPieceOID,
                                     SChar           * aRow )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(smpVarPageList::addFreeSlotPending(
                                             aTrans,
                                             aHeader->mSpaceID,
                                             aHeader->mVar.mMRDB,
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
IDE_RC smcRecord::freeFixRowPending( void            * aTrans,
                                     smcTableHeader  * aHeader,
                                     SChar           * aRow )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDE_TEST(smpFixedPageList::addFreeSlotPending(
                                         aTrans,
                                         aHeader->mSpaceID,
                                         &(aHeader->mFixed.mMRDB),
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
IDE_RC smcRecord::setFreeVarRowPending( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        smOID             aPieceOID,
                                        SChar           * aRow,
                                        smSCN             aSCN )
{
    smLSN sNTA;

    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        sNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );

        IDE_TEST(smpVarPageList::freeSlot(aTrans,
                                          aHeader->mSpaceID,
                                          aHeader->mVar.mMRDB,
                                          aPieceOID,
                                          aRow,
                                          &sNTA,
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
IDE_RC smcRecord::setFreeFixRowPending( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        SChar           * aRow,
                                        smSCN             aSCN )
{
    if(smcTable::isDropedTable(aHeader) == ID_FALSE)
    {
        IDU_FIT_POINT( "1.BUG-15969@smcRecord::setFreeFixRowPending" );

        IDE_TEST(smpFixedPageList::freeSlot(aTrans,
                                            aHeader->mSpaceID,
                                            &(aHeader->mFixed.mMRDB),
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
IDE_RC smcRecord::setSCN( scSpaceID  aSpaceID,
                          SChar   *  aRow,
                          smSCN      aSCN )
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;

    sSlotHeader = (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID( aRow );
    SM_SET_SCN( &(sSlotHeader->mCreateSCN), &aSCN );

    return smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID);
}

/***********************************************************************
 * Description :
 * PROJ-2429 Dictionary based data compress for on-disk DB
 * setSCN ������ �α��Ͽ� record�� refine�� �����Ǵ°��� ����
 ***********************************************************************/
IDE_RC smcRecord::setSCNLogging( void           * aTrans,
                                 smcTableHeader * aHeader,
                                 SChar          * aRow,
                                 smSCN            aSCN )
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;

    IDE_ASSERT( smcRecordUpdate::writeSetSCNLog( aTrans, 
                                                 aHeader,
                                                 aRow )
                == IDE_SUCCESS );

    sSlotHeader = (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID( aRow );
    SM_SET_SCN( &(sSlotHeader->mCreateSCN), &aSCN );

    return smmDirtyPageMgr::insDirtyPage(aHeader->mSpaceID, sPageID);
}

/***********************************************************************
 * Description : IN-DOUBT Transaction�� ������ ���ڵ忡 ���Ѵ� SCN��
 *               �����Ѵ�.
 *
 * [BUG-26415] XA Ʈ������� Partial Rollback(Unique Volation)�� Prepare
 *             Ʈ������� �����ϴ� ��� ���� �籸���� �����մϴ�.
 * : Insert ���� Unique Volation���� ������ ���� �ѹ�� ���ڵ���
 *   Delete Bit�� �����ؾ� �Ѵ�.( refine�ܰ迡�� ���ŵǾ�� �Ѵ� )
 * : �籸���ÿ��� ȣ��Ǵ� �Լ���.
 ***********************************************************************/
IDE_RC smcRecord::setSCN4InDoubtTrans( scSpaceID  aSpaceID,
                                       smTID      aTID,
                                       SChar   *  aRow )
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;
    smSCN          sSCN;
    ULong          sTID;

    sTID = (ULong)aTID;

    sSlotHeader = (smpSlotHeader*)aRow;

    SM_SET_SCN_INFINITE_AND_TID( &sSCN, sTID );

    sPageID = SMP_SLOT_GET_PID( aRow );

    if( SM_SCN_IS_DELETED( sSlotHeader->mCreateSCN ) )
    {
        SM_SET_SCN_DELETE_BIT( &sSCN );
    }

    SM_SET_SCN( &(sSlotHeader->mCreateSCN), &sSCN );

    return smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID);
}

/***********************************************************************
 * Description : row�� mNext�� SCN���� �����ϴ� �Լ�
 ***********************************************************************/
IDE_RC smcRecord::setRowNextToSCN( scSpaceID            aSpaceID,
                                   SChar              * aRow,
                                   smSCN                aSCN )
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;

    IDE_ASSERT( aRow != NULL );

    sSlotHeader = (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID(aRow);

    SM_SET_SCN( &(sSlotHeader->mLimitSCN), &aSCN );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID)
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aRowHeader�� Delete Bit�� �����Ѵ�.
 *
 * aSpaceID   - [IN] aRowHeader�� ���� Table�� �ִ� Table Space ID
 * aRowHeader - [IN] Row�� Pointer
 ***********************************************************************/
IDE_RC smcRecord::setDeleteBitOnHeader( scSpaceID       aSpaceID,
                                        smpSlotHeader * aRowHeader )
{
    return setDeleteBitOnHeaderInternal( aSpaceID,
                                         aRowHeader,
                                         ID_TRUE /* Set Delete Bit */ );
}

/***********************************************************************
 * Description : aRowHeader�� Delete Bit�� set/unset�Ѵ�.
 *
 * aSpaceID        - [IN] aRowHeader�� ���� Table�� �ִ� Table Space ID
 * aRowHeader      - [IN] Row�� Pointer
 * aIsSetDeleteBit - [IN] if aIsSetDeleteBit = ID_TRUE, set delete bit,
 *                        else unset delete bit of row.
 ***********************************************************************/
IDE_RC smcRecord::setDeleteBitOnHeaderInternal( scSpaceID       aSpaceID,
                                                void          * aRowHeader,
                                                idBool          aIsSetDeleteBit)
{
    scPageID       sPageID;
    smpSlotHeader *sSlotHeader = (smpSlotHeader*)aRowHeader;

    if( aIsSetDeleteBit == ID_TRUE )
    {
        SM_SET_SCN_DELETE_BIT( &(sSlotHeader->mCreateSCN) );
    }
    else
    {
        SM_CLEAR_SCN_DELETE_BIT( &(sSlotHeader->mCreateSCN) );
    }

    sPageID = SMP_SLOT_GET_PID(sSlotHeader);
    return smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID);
}

/***********************************************************************
 * Description : aRowHeader�� SCN�� Delete Bit�� Setting�Ѵ�.
 *
 * aTrans     - [IN]: Transaction Pointer
 * aRowHeader - [IN]: Row Header Pointer
 * aFlag      - [IN]: 1. SMC_WRITE_LOG_OK : log���.
 *                    2. SMC_WRITE_LOG_NO : log��Ͼ���.
 ***********************************************************************/
IDE_RC smcRecord::setDeleteBit( void             * aTrans,
                                scSpaceID          aSpaceID,
                                void             * aRowHeader,
                                SInt               aFlag)
{
    scPageID    sPageID;
    smOID       sRecOID;
    UInt        sState = 0;

    sPageID = SMP_SLOT_GET_PID(aRowHeader);
    sRecOID = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRowHeader ) );

    IDE_TEST(smmManager::holdPageXLatch(aSpaceID, sPageID) != IDE_SUCCESS);
    sState = 1;

    if(aFlag == SMC_WRITE_LOG_OK)
    {
        IDE_TEST(smrUpdate::setDeleteBitAtFixRow(NULL, /* idvSQL* */
                                                 aTrans,
                                                 aSpaceID,
                                                 sRecOID)
                 != IDE_SUCCESS);

    }

    /* BUG-14953 : PK�� �ΰ���. Rollback�� SCN����
       �׻� ���Ѵ� Bit�� Setting�Ǿ� �־�� �Ѵ�. �׷���
       ������ Index�� Unique Check�� �������Ͽ� Duplicate
       Key�� ����.*/
    SM_SET_SCN_DELETE_BIT( &(((smpSlotHeader*)aRowHeader)->mCreateSCN) );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID) != IDE_SUCCESS);

    IDE_TEST(smmManager::releasePageLatch(aSpaceID, sPageID) != IDE_SUCCESS);
    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                                 sPageID) == IDE_SUCCESS);
        IDE_ASSERT(smmManager::releasePageLatch(aSpaceID,
                                                sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * DESCRIPTION : ���̺� �����ÿ� NTA OP Ÿ���� SMR_OP_CREATE_TABLE ��
 * ���� udno ó���ÿ� �����ϴ� �Լ��ν� table header�� ����� fixed slot
 * �� ���� ó���� �Ѵ�.
 ***********************************************************************/
IDE_RC smcRecord::setDropTable( void   *  aTrans,
                                SChar  *  aRow )
{
    scPageID       sPageID;
    UInt           sState = 0;
    smpSlotHeader *sSlotHeader;
    smOID          sRecOID;

    sSlotHeader= (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID(aRow);
    sRecOID = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( sSlotHeader ) );

    IDE_TEST(smmManager::holdPageXLatch(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                        sPageID) != IDE_SUCCESS);
    sState = 1;

    IDE_TEST(smrUpdate::setDeleteBitAtFixRow(
                 NULL, /* idvSQL* */
                 aTrans,
                 SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                 sRecOID) != IDE_SUCCESS);


    IDE_TEST(smrUpdate::setDropFlagAtFixedRow(
                 NULL, /* idvSQL* */
                 aTrans,
                 SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                 sRecOID,
                 ID_TRUE) != IDE_SUCCESS);


    // BUG-27329 CodeSonar::Uninitialized Variable (2)
    SM_SET_SCN_DELETE_BIT( &(sSlotHeader->mCreateSCN) );

    SMP_SLOT_SET_DROP( sSlotHeader );

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sPageID) != IDE_SUCCESS);

    IDE_TEST(smmManager::releasePageLatch(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                          sPageID) != IDE_SUCCESS);
    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(
                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID)
                   == IDE_SUCCESS);
        IDE_ASSERT(smmManager::releasePageLatch(
                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sPageID)
                   == IDE_SUCCESS);
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aRID�� ����Ű�� Variable Column Piece�� Flag���� aFlag�� �ٲ۴�.
 *
 * aTrans       - [IN] Transaction Pointer
 * aTableOID    - [IN] Table OID
 * aSpaceID     - [IN] Variable Column Piece �� ���� Tablespace�� ID
 * aVCPieceOID  - [IN] Variable Column Piece OID
 * aVCPiecePtr  - [IN] Variabel Column Piece Ptr
 * aVCPieceFlag - [IN] Variabel Column Piece Flag
 ***********************************************************************/
IDE_RC smcRecord::setFreeFlagAtVCPieceHdr( void    * aTrans,
                                           smOID     aTableOID,
                                           scSpaceID aSpaceID,
                                           smOID     aVCPieceOID,
                                           void    * aVCPiecePtr,
                                           UShort    aVCPieceFreeFlag,
                                           idBool    aLogging /* BUG-46854: default �μ� �߰� */)
{
    smVCPieceHeader *sVCPieceHeader;
    scPageID         sPageID;
    UShort           sVCPieceFlag;

    // fix BUG-27221 : [codeSonar] Null Pointer Dereference
    IDE_ASSERT( aVCPiecePtr != NULL );

    sPageID        = SM_MAKE_PID(aVCPieceOID);
    sVCPieceHeader = (smVCPieceHeader *)aVCPiecePtr;

    sVCPieceFlag = sVCPieceHeader->flag;
    sVCPieceFlag &= ~SM_VCPIECE_FREE_MASK;
    sVCPieceFlag |= aVCPieceFreeFlag;

    // BUG-46854 : delete�� �� flag ���� �α� ���� ���� ( �̸� �� )
    if ( aLogging == ID_TRUE )
    { 
        IDE_TEST(smrUpdate::setFlagAtVarRow(NULL, /* idvSQL* */
                                            aTrans,
                                            aSpaceID,
                                            aTableOID,
                                            aVCPieceOID,
                                            sVCPieceHeader->flag,
                                            sVCPieceFlag)
                 != IDE_SUCCESS);
    }

    sVCPieceHeader->flag = sVCPieceFlag;

    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC smcRecord::setIndexDropFlag( void    * aTrans,
                                    smOID     aTableOID,
                                    smOID     aIndexOID,
                                    void    * aIndexHeader,
                                    UShort    aDropFlag )
{
    scPageID          sPageID;
    smnIndexHeader  * sIndexHeader;

    sPageID      = SM_MAKE_PID(aIndexOID);
    sIndexHeader = (smnIndexHeader *)aIndexHeader;

    IDE_TEST(smrUpdate::setIndexDropFlag(NULL, /* idvSQL* */
                                         aTrans,
                                         aTableOID,
                                         aIndexOID,
                                         sIndexHeader->mDropFlag,
                                         aDropFlag)
             != IDE_SUCCESS);


    sIndexHeader->mDropFlag = aDropFlag;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                            sPageID)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * task-2399 Lock Row�� Delete Row�� ������.
 *
 *
 * �������� Lock Row������ �����ϸ�, ���ο� ����(lock row)�� ����� lock ������
 * ����� row�� mNext�� �����Ͽ���.
 * ������ lock�� ��쿣 lock�� �ɸ� ����� scn�� �ʿ���� ������(MVCC�� ������� ����)
 * ���� �̷��� �� �ʿ䰡 ����. ��, ���� lock�� �ɷ��ִ��� ���θ� ǥ���ϸ�ǰ�, �̰���
 * ������ Ʈ������� �����ָ� �ȴ�.
 *
 * �̷��� �ϱ� ���ؼ� ���� row�� mTID�� mNext�� �̿��Ѵ�.
 * mTID���� Lock row������ ������ Ʈ������� tid�� �����ְ�, mNext�� Lock�� �ɷȴٴ�
 * ���� ǥ���� �ش�.
 *
 * �̷��� ����� ������ ������ ���� removeVersion�Լ� ���� ������ �ּ��� '���ο� �����
 * ������ ������ ����'�� '���ο� ��Ŀ��� �߱�� ���ü� ���� ����'�� �����ϸ� �ȴ�.
 *
 ***********************************************************************/
/***********************************************************************
 * Description : select for update�� �����Ҷ� �Ҹ��� �Լ�. ��, select for update
 *          �� ������ row�� ���ؼ� Lock������ ����ȴ�. �̶�, svpSlotHeader��
 *          mNext�� LockRow���� ǥ���Ѵ�.  �������� ���� Ʈ������� �������� ���ȸ�
 *          ��ȿ�ϴ�.
 ***********************************************************************/
IDE_RC smcRecord::lockRow(void           * aTrans,
                          smSCN            aViewSCN,
                          smcTableHeader * aHeader,
                          SChar          * aRow,
                          ULong            aLockWaitTime,
                          idBool           aForbiddenToRetry )
{
    smpSlotHeader * sSlotHeader;
    scPageID        sPageID;
    UInt            sState          = 0;
    smSCN           sRowSCN;
    smTID           sRowTID;
    
    sSlotHeader = (smpSlotHeader*)aRow;

    sPageID = SMP_SLOT_GET_PID(sSlotHeader);

    IDE_TEST(smmManager::holdPageXLatch(aHeader->mSpaceID,
                                        sPageID) != IDE_SUCCESS);
    sState = 1;

    SMX_GET_SCN_AND_TID( sSlotHeader->mLimitSCN, sRowSCN, sRowTID );

    //�̹� �ڽ��� LOCK�� �ɾ��ٸ� �ٽ� LOCK�� �ɷ��� ���� �ʴ´�.
    if ( ( SM_SCN_IS_LOCK_ROW( sRowSCN ) ) &&
         ( sRowTID == smLayerCallback::getTransID( aTrans ) ) )
    {
        /* do nothing */
    }
    else
    {
        IDE_TEST(recordLockValidation( aTrans,
                                       aViewSCN,
                                       aHeader,
                                       &aRow,
                                       aLockWaitTime,
                                       &sState,
                                       NULL, /* without retry info*/
                                       aForbiddenToRetry ) 
                 != IDE_SUCCESS );

        // PROJ-1784 retry info�� �����Ƿ� ������� �ʴ´�.
        IDE_ASSERT( aRow == (SChar*)sSlotHeader );

        IDE_TEST( lockRowInternal( aTrans,
                                   aHeader,
                                   aRow ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST(smmManager::releasePageLatch(aHeader->mSpaceID,
                                          sPageID) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_PUSH();
        sPageID = SMP_SLOT_GET_PID(aRow);
        IDE_ASSERT(smmManager::releasePageLatch(aHeader->mSpaceID,
                                                sPageID) == IDE_SUCCESS);
        IDE_POP();
    }

    /* BUG-24151: [SC] Update Retry, Delete Retry, Statement Rebuild Count��
     *            AWI�� �߰��ؾ� �մϴ�.*/
    if( ideGetErrorCode() == smERR_RETRY_Already_Modified )
    {
        SMX_INC_SESSION_STATISTIC( aTrans,
                                   IDV_STAT_INDEX_LOCKROW_RETRY_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( aHeader, SMC_INC_RETRY_CNT_OF_LOCKROW );
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
IDE_RC smcRecord::lockRowInternal(void           * aTrans,
                                  smcTableHeader * aHeader,
                                  SChar          * aRow)
{
    smpSlotHeader *sSlotHeader = (smpSlotHeader*)aRow;
    smOID          sRecOID;
    smSCN          sRowSCN;
    smTID          sRowTID;
    ULong          sTransID;
    ULong          sNxtSCN;

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
        
        sRecOID = SM_MAKE_OID( SMP_SLOT_GET_PID(aRow),
                               SMP_SLOT_GET_OFFSET( sSlotHeader ) );

        SM_GET_SCN( &sNxtSCN, &( sSlotHeader->mLimitSCN ) );

        /* BUG-17117 select ... For update�Ŀ� server kill, start�ϸ�
         * ���ڵ尡 ������ϴ�.
         *
         * Redo�ÿ��� Lock�� Ǯ���־�� �ϱ⶧���� redo�� AfterImage��
         * SM_NULL_OID�� �Ǿ�� �մϴ�. */
        IDE_TEST( smrUpdate::updateNextVersionAtFixedRow(
                      NULL, /* idvSQL* */
                      aTrans,
                      aHeader->mSpaceID,
                      aHeader->mSelfOID,
                      sRecOID,
                      sNxtSCN,
                      SM_NULL_OID )
              != IDE_SUCCESS );


        sTransID = (ULong)smLayerCallback::getTransID( aTrans );
        SMP_SLOT_SET_LOCK( sSlotHeader, sTransID );

        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aHeader->mSelfOID,
                                           sRecOID,
                                           aHeader->mSpaceID,
                                           SM_OID_LOCK_FIXED_SLOT )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : lockRow�� ����� row�� ���� lock�� �����Ҷ� ȣ��Ǵ� �Լ�
 ***********************************************************************/
IDE_RC smcRecord::unlockRow( void           * aTrans,
                             scSpaceID        aSpaceID,
                             SChar          * aRow )
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;
    smTID          sRowTID;

    sSlotHeader = (smpSlotHeader*)aRow;

    sRowTID = SMP_GET_TID( sSlotHeader->mLimitSCN );

    if ( ( sRowTID == smLayerCallback::getTransID( aTrans ) ) &&
         ( SMP_SLOT_IS_LOCK_TRUE( sSlotHeader ) ) )
    {
        SMP_SLOT_SET_UNLOCK(sSlotHeader);

        sPageID = SMP_SLOT_GET_PID(aRow);

        IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                               sPageID) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
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
 *     : SMR_LT_UPDATE�� SMR_SMC_PERS_UPDATE_FIXED_ROW
 *   + table header�� nullrow�� ���� ���� �� �α�
 *     : SMR_LT_UPDATE�� SMR_SMC_TABLEHEADER_SET_NULLROW
 ***********************************************************************/
IDE_RC smcRecord::makeNullRow( void*           aTrans,
                               smcTableHeader* aHeader,
                               smSCN           aInfiniteSCN,
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
    IDE_TEST( insertVersion( NULL, // aStatistics*
                             aTrans,
                             NULL, // aTableInfoPtr
                             aHeader,
                             aInfiniteSCN,
                             (SChar**)&sNullSlotHeaderPtr,
                             NULL, // aRetInsertSlotGRID
                             aNullRow,
                             aLoggingFlag )
              != IDE_SUCCESS );
    sNullSlotHeaderPtr->mVarOID = SM_NULL_OID;

    idlOS::memcpy(&sAfterSlotHeader, sNullSlotHeaderPtr, SMP_SLOT_HEADER_SIZE);
    sAfterSlotHeader.mCreateSCN = sNullRowSCN;

    sNullRowPID = SMP_SLOT_GET_PID((SChar *)sNullSlotHeaderPtr);
    sNullRowOID = SM_MAKE_OID( sNullRowPID,
                               SMP_SLOT_GET_OFFSET( sNullSlotHeaderPtr ) );

    /* insert�� nullrow header�� ������ �α��Ѵ�. */
    IDE_TEST(smrUpdate::updateFixedRowHead( NULL, /* idvSQL* */
                                            aTrans,
                                            aHeader->mSpaceID,
                                            sNullRowOID,
                                            sNullSlotHeaderPtr,
                                            &sAfterSlotHeader,
                                            ID_SIZEOF(smpSlotHeader) ) != IDE_SUCCESS);


    /* insert �� nullrow�� scn�� �����Ѵ�. */
    IDE_TEST(setSCN(aHeader->mSpaceID,
                    (SChar*)sNullSlotHeaderPtr,
                    sNullRowSCN) != IDE_SUCCESS);

    *aNullRowOID = sNullRowOID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 32k over Variable Column�� Table�� Variable Page List�� Insert�Ѵ�.
 *               ���� �׻� out-mode�� �����Ѵ�.
 *
 *               ����: VC Piece�� �� Header�� ���ؼ��� Alloc�� logging��
 *                    ������ VC Piece�� Value�� ����� �κп� ���ؼ��� Logging
 *                    ���� �ʴ´�. �ֳ��ϸ� Insert, Update�� �� �κп� ����
 *                    Logging�� �ϱ� �����̴�. ���⼭ Logging�� Update�Ŀ�
 *                    �ϴ��� ������ ���� �� ������ Update������ Before Image��
 *                    �ǹ̰� ���� �����̴�.
 *
 * aTrans       - [IN] Transaction Pointer
 * aHeader      - [IN] Table Header
 * aFixedRow    - [IN] VC�� VCDesc�� ����� Fixed Row Pointer
 * aColumn      - [IN] VC Column Description
 * aAddOIDFlag  - [IN] Variable Column�� �����ϴ� VC�� Transaction OID List��
 *                     Add�� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE.
 * aValue       - [IN] Variable Column�� Value
 * aLength      - [IN] Variable Column�� Value�� Length
 * aFstPieceOID - [OUT] Variable Column�� �����ϴ� ù��° VC Piece OID
 ***********************************************************************/
IDE_RC smcRecord::insertLargeVarColumn( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        SChar           * aFixedRow,
                                        const smiColumn * aColumn,
                                        idBool            aAddOIDFlag,
                                        const void      * aValue,
                                        UInt              aLength,
                                        smOID           * aFstPieceOID)
{
    UInt      sVCPieceLength;
    UInt      sValuePartLength = aLength;
    smOID     sCurVCPieceOID = SM_NULL_OID;
    smOID     sNxtVCPieceOID = SM_NULL_OID;
    SChar*    sPiecePtr;
    UInt      sOffset = 0;
    SChar*    sPartValue;
    smVCDesc* sVCDesc;

    smVCDescInMode *sVCDescInMode;

    IDE_ASSERT( aTrans    != NULL );
    IDE_ASSERT( aHeader   != NULL );
    IDE_ASSERT( aFixedRow != NULL );
    IDE_ASSERT( aColumn   != NULL );
    IDE_ASSERT_MSG(( aAddOIDFlag == ID_TRUE )||
                   ( aAddOIDFlag == ID_FALSE ),
                   "Add OID Flag : %"ID_UINT32_FMT"\n",
                   aAddOIDFlag );

    sVCDesc = getVCDesc(aColumn, aFixedRow);

    if(aLength <= aColumn->vcInOutBaseSize)
    {
        /* Store Value In In-Mode */
        sVCDescInMode = (smVCDescInMode*)sVCDesc;

        sVCDescInMode->length = aLength;
        sVCDescInMode->flag   = SM_VCDESC_MODE_IN;

        if(aLength != 0)
        {
            idlOS::memcpy(sVCDescInMode + 1, (SChar*)aValue, aLength);
        }
    }
    else
    {
        /* Store Value In Out-Mode */

        IDE_ASSERT( aFstPieceOID != NULL );

        /* =================================================================
         * Value�� SMP_VC_PIECE_MAX_SIZE�� �Ѵ� ��� ���� �������� ���� �����ϰ� ����
         * �ʴ´ٸ� �ϳ��� Variable Piece�� �����Ѵ�. �̶� VC�� ������ Piece�� ���� ����
         * �� �� ������ Piece���� �����Ѵ�. �̷��� �ϴ� ������ Value�� �պ��� ������ ���
         * ���� Piece�� OID�� �˼� ���� ������ Piece�� ���� Logging�� ������ ���� Piece
         * �� �մܿ� Piece�� ������ ��� �մ��� Piece�� nextPieceOID�� ���� logging��
         * ������ �����ؾ��Ѵ�. ������ �� ������ Piece���� �������� ������ ��� ���� Piece
         * �� Next Piece OID�� �˱� ������ Variable Piece�� AllocSlot�� next piece��
         * ���� oid logging�� ���� �Ͽ� logging���� ���� �� �ִ�.
         * ================================================================ */
        sVCPieceLength  = sValuePartLength % SMP_VC_PIECE_MAX_SIZE;
        sOffset         = (sValuePartLength / SMP_VC_PIECE_MAX_SIZE) * SMP_VC_PIECE_MAX_SIZE;

        if( sVCPieceLength == 0 )
        {
            sVCPieceLength  = SMP_VC_PIECE_MAX_SIZE;
            sOffset        -= SMP_VC_PIECE_MAX_SIZE;
        }

        while ( 1 )
        {
            /* [1-1] info ������ ���� variable column ���� ���� */
            sPartValue  = (SChar*)aValue + sOffset;

            /* [1-2] Value�� �����ϱ� ���� variable piece �Ҵ� */
            IDE_TEST( smpVarPageList::allocSlot( aTrans,
                                                 aHeader->mSpaceID,
                                                 aHeader->mSelfOID,
                                                 aHeader->mVar.mMRDB,
                                                 sVCPieceLength,
                                                 sNxtVCPieceOID,
                                                 &sCurVCPieceOID,
                                                 &sPiecePtr )
                      != IDE_SUCCESS );

            IDE_TEST( smpVarPageList::setValue( aHeader->mSpaceID,
                                                sCurVCPieceOID,
                                                sPartValue,
                                                sVCPieceLength)
                      != IDE_SUCCESS );

            if( aAddOIDFlag == ID_TRUE )
            {
                /* [1-3] ���� �Ҵ���� variable piece�� versioning list�� �߰� */
                IDE_TEST( smLayerCallback::addOID( aTrans,
                                                   aHeader->mSelfOID,
                                                   sCurVCPieceOID,
                                                   aHeader->mSpaceID,
                                                   SM_OID_NEW_VARIABLE_SLOT )
                          != IDE_SUCCESS );
            }

            /* [1-4] info ������ multiple page�� ���ļ� ����Ǵ� ���
             *       �� �������鳢���� ����Ʈ�� ����                  */
            sNxtVCPieceOID  = sCurVCPieceOID;

            sValuePartLength -= sVCPieceLength;
            if( sValuePartLength <= 0 )
            {
                IDE_ASSERT_MSG( sValuePartLength == 0,
                                "sValuePartLength : %"ID_UINT32_FMT"\n",
                                sValuePartLength );
                break;
            }

            sVCPieceLength  = SMP_VC_PIECE_MAX_SIZE;
            sOffset        -= SMP_VC_PIECE_MAX_SIZE;
        }

        sVCDesc->length = aLength;
        sVCDesc->flag   = SM_VCDESC_MODE_OUT;
        sVCDesc->fstPieceOID = sCurVCPieceOID;

        *aFstPieceOID = sCurVCPieceOID;
    }

    IDE_ASSERT_MSG( ( SM_VCDESC_IS_MODE_IN (sVCDesc) ) ||
                    ( sCurVCPieceOID != SM_NULL_OID ),
                    "Flag : %"ID_UINT32_FMT", "
                    "VCPieceOID : %"ID_UINT32_FMT"\n",
                    sVCDesc->flag ,
                    sCurVCPieceOID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �������� Variable Column�� �ϳ��� variable slot �� Insert�Ѵ�.
 *               �׻� out-mode�� �����ϸ� OID�� fixed row slot header�� �ִ�.
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
 * aFixedRow        - [IN] VC�� OID�� ����� Fixed Row Pointer
 * aColumns         - [IN] VC Column Description
 * aAddOIDFlag      - [IN] Variable Column�� �����ϴ� VC�� Transaction OID List��
 *                         Add�� ���ϸ� ID_TRUE, �ƴϸ� ID_FALSE.
 * aValues          - [IN] Variable Column�� Values
 * aVarcolumnCount  - [IN] Variable Column�� Value�� ����
 * aImageSize       - [OUT] insert�� united var �� �α� ũ��
 ***********************************************************************/
IDE_RC smcRecord::insertUnitedVarColumns( void            *  aTrans,
                                          smcTableHeader  *  aHeader,
                                          SChar           *  aFixedRow,
                                          const smiColumn ** aColumns,
                                          idBool             aAddOIDFlag,
                                          smiValue        *  aValues,
                                          SInt               aVarColumnCount, 
                                          UInt            *  aImageSize )
{
    smOID         sCurVCPieceOID    = SM_NULL_OID;
    smOID         sNxtVCPieceOID    = SM_NULL_OID;

    // Piece ũ�� �ʱ�ȭ : LastOffsetArray size
    UInt          sVarPieceLen      = ID_SIZEOF(UShort);

    UInt          sLogImageLen      = 0;
    UInt          sPrevVarPieceLen  = 0;
    UInt          sVarPieceCnt      = 0;
    UShort        sVarPieceColCnt   = 0;
    idBool        sIsTrailingNull   = ID_FALSE;
    SInt          i                 = ( aVarColumnCount - 1 ); /* ���� ������ Column ���� �����Ѵ�. */

    // BUG-43117
    // ������ �� ����(ex. 5�� col�̸� 4�� col) �÷��� �е� ũ��
    UInt          sPrvAlign         = 0;
    // ���� �÷��� ���� �е� ũ��
    UInt          sCurAlign         = 0;
    // varPiece ����� ������ ���� �е� ũ��� offset array�� ��ũ�Ⱑ ������ ���� logging �� vaPiece�� ����
    UInt          sVarPieceLen4Log  = 0;

    IDE_ASSERT( aTrans          != NULL );
    IDE_ASSERT( aHeader         != NULL );
    IDE_ASSERT( aFixedRow       != NULL );
    IDE_ASSERT( aColumns        != NULL );
    IDE_ASSERT( aVarColumnCount  > 0 );
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
            /* nothing to do */
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

            /* 2 = Previous Column offset +  End offset */  

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
                /* Previous Column�� ���ϸ� ���� ũ�⸦ �ѱ�Ƿ�, ���ݱ��� �÷��� insert */
                IDE_TEST( insertUnitedVarPiece( aTrans,
                                                aHeader,
                                                aAddOIDFlag,
                                                &aValues[i],
                                                aColumns, 
                                                (UInt)(sVarPieceLen),
                                                sVarPieceColCnt,
                                                sNxtVCPieceOID,
                                                &sCurVCPieceOID,
                                                &sVarPieceLen4Log )
                        != IDE_SUCCESS );

                sVarPieceCnt++;
               
                // ���� varPiece����, except header
                sLogImageLen += sVarPieceLen4Log;

                sNxtVCPieceOID  = sCurVCPieceOID;
                sVarPieceColCnt = 0;
                
                /* insert �����Ƿ� �ٽ� �ʱ�ȭ */
                sVarPieceLen    = ID_SIZEOF(UShort);
            }
        }
        else /* ù�÷��� ������ ���, ���� �÷� insert */
        {
            sVarPieceColCnt++; 
            
            // value�� ���� + offset array 1ĭ ũ��
            sVarPieceLen += aValues[i].length + ID_SIZEOF(UShort);
            
            //���� �÷��� �е� ũ�� ����  /* BUG-43117 */
            sCurAlign = SMC_GET_COLUMN_PAD_LENGTH( aValues[i], aColumns[i] );

            sVarPieceLen += sCurAlign;  

            IDE_TEST( insertUnitedVarPiece( aTrans,
                                            aHeader,
                                            aAddOIDFlag,
                                            &aValues[i],
                                            aColumns, /* BUG-43117 */
                                            (UInt)(sVarPieceLen),
                                            sVarPieceColCnt,
                                            sNxtVCPieceOID,
                                            &sCurVCPieceOID,
                                            &sVarPieceLen4Log  /* BUG-43117 */)
                      != IDE_SUCCESS );
            sVarPieceCnt++;

            // ���� varPiece����, header ����
            sLogImageLen += sVarPieceLen4Log;
        }
    }

    /* fixed row ���� ù var piece �� �̾������� oid ���� */
    ((smpSlotHeader*)aFixedRow)->mVarOID = sCurVCPieceOID;

    //zzzz coverage �� ���� if ������ �� ����
    if ( sCurVCPieceOID != SM_NULL_OID )
    {
        *aImageSize += SMC_UNITED_VC_LOG_SIZE( sLogImageLen, aVarColumnCount, sVarPieceCnt );
    }
    else
    {
        *aImageSize += ID_SIZEOF(smOID);
    }

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
 * aVarPieceLen4Log - [OUT] ���� varPiece����, header ����
 ***********************************************************************/
IDE_RC smcRecord::insertUnitedVarPiece( void            * aTrans,
                                        smcTableHeader  * aHeader,
                                        idBool            aAddOIDFlag,
                                        smiValue        * aValues,
                                        const smiColumn** aColumns, /* BUG-43117 */
                                        UInt              aTotalVarLen,
                                        UShort            aVarColCount,
                                        smOID             aNxtVCPieceOID,
                                        smOID           * aCurVCPieceOID,
                                        UInt            * aVarPieceLen4Log /* BUG-43117 */)
{
    SChar       * sPiecePtr         = NULL;
    UShort      * sColOffsetPtr     = NULL;     /* offsetArray ������ */
    smiValue    * sCurValue         = NULL;
    UShort        sValueOffset      = 0;
    UInt          i                 = 0;

    //PROJ-2419 insert UnitedVarPiece FitTest: before alloc slot
    IDU_FIT_POINT( "1.PROJ-2419@smcRecord::insertUnitedVarPiece::allocSlot" );

    /* PROJ-2419 allocSlot ���ο��� VarPiece Header��
     * NxtOID�� NULLOID��, Flag�� Used�� �ʱ�ȭ �Ǿ� �Ѿ�´�. */
    /* BUG-43379
     * smVCPieceHeader�� ũ��� slot size�� page�� �ʱ�ȭ ��ų �� ���Խ�Ų��.
     * ���� allocSlot �� ���� ����� �ʿ䰡 ����. �ڼ��� ������ 
     * smpVarPageList::initializePageListEntry�Լ���
     * smpVarPageList::initializePage �Լ��� ����. */
    IDE_TEST( smpVarPageList::allocSlot( aTrans,
                                         aHeader->mSpaceID,
                                         aHeader->mSelfOID,
                                         aHeader->mVar.mMRDB,
                                         aTotalVarLen,
                                         aNxtVCPieceOID,
                                         aCurVCPieceOID,
                                         &sPiecePtr,
                                         SM_VCPIECE_TYPE_OTHER,
                                         ID_FALSE ) // BUG-47366 UnitedVar�� Alloc Log ����
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
            sValueOffset = (UShort) idlOS::align( (UInt) sValueOffset, aColumns[i]->align );    

            sColOffsetPtr[i] = sValueOffset;

            //PROJ-2419 insert UnitedVarPiece FitTest: before mem copy 
            IDU_FIT_POINT( "2.PROJ-2419@smcRecord::insertUnitedVarPiece::memcpy" );

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

    //BUG-43117 : ���� varPiece����, header ����
    *aVarPieceLen4Log = sValueOffset - ID_SIZEOF(smVCPieceHeader);

    //PROJ-2419 insert UnitedVarPiece FitTest: before dirty page 
    IDU_FIT_POINT( "3.PROJ-2419@smcRecord::insertUnitedVarPiece::insDirtyPage" );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( aHeader->mSpaceID,
                                             SM_MAKE_PID(*aCurVCPieceOID))
            != IDE_SUCCESS );

    if ( aAddOIDFlag == ID_TRUE )
    {
        //PROJ-2419 insert UnitedVarPiece FitTest: before add OID 
        IDU_FIT_POINT( "4.PROJ-2419@smcRecord::insertUnitedVarPiece::addOID" );

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
        /* do nothing */
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
 * aRetryInfo    - [IN] Retry Info
 * aForbiddenToRetry - [IN] retry error �ø��°� ����, abort �߻� 
 ***********************************************************************/
IDE_RC smcRecord::recordLockValidation(void                  * aTrans,
                                       smSCN                   aViewSCN,
                                       smcTableHeader        * aHeader,
                                       SChar                ** aRow,
                                       ULong                   aLockWaitTime,
                                       UInt                  * aState,
                                       const smiDMLRetryInfo * aRetryInfo,
                                       idBool                  aForbiddenToRetry )
{
    smpSlotHeader * sCurSlotHeaderPtr;
    smpSlotHeader * sPrvSlotHeaderPtr;
    smpSlotHeader * sSlotHeaderPtr;
    scSpaceID       sSpaceID;
    scPageID        sPageID         = SM_NULL_PID;
    scPageID        sNxtPageID;
    smTID           sWaitTransID    = SM_NULL_TID;
    smSCN           sNxtRowSCN;
    smTID           sNxtRowTID      = SM_NULL_TID;
    smTID           sTransID        = SM_NULL_TID;
    smOID           sNextOID        = SM_NULL_OID;
    UInt            sSessionChkCnt  = 0;

    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aTrans  != NULL );
    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT( aState  != NULL );

    sSpaceID = aHeader->mSpaceID;

    sPrvSlotHeaderPtr = (smpSlotHeader *)*aRow;
    sCurSlotHeaderPtr = sPrvSlotHeaderPtr;

    /* BUG-15642 Record�� �ι� Free�Ǵ� ��찡 �߻���.:
     * �̷� ��찡 �߻��ϱ����� validateUpdateTargetRow����
     * üũ�Ͽ� ������ �ִٸ� Rollback��Ű�� �ٽ� retry�� �����Ŵ*/
    // �� �Լ� �������� row�� ���ϴ� �κ��� �������� �ʴ´�.  �׷��� ������ row�� ���ϴ�
    // �κ��� ���� �Ҵ��Ͽ� �Լ������ڷ� �� �ʿ䰡 ����.
    IDE_TEST_RAISE( validateUpdateTargetRow( aTrans,
                                             sSpaceID,
                                             aViewSCN,
                                             sCurSlotHeaderPtr,
                                             aRetryInfo )
                    != IDE_SUCCESS, already_modified );

    sPageID = SMP_SLOT_GET_PID(*aRow);

    sTransID = smLayerCallback::getTransID( aTrans );

    while( ! SM_SCN_IS_FREE_ROW( sCurSlotHeaderPtr->mLimitSCN ) /* aRow�� �ֽŹ����ΰ�?*/)
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
                /* �ٸ� Ʈ������� lock�� ������Ƿ� �װ� �����⸦ ��ٸ���. */
            }
        }
        else
        {
            /* check whether the record is already modified. */
            // PROJ-1784 DML without retry
            if( SM_SCN_IS_NOT_INFINITE( sNxtRowSCN ) )
            {
                IDE_TEST_RAISE( aRetryInfo == NULL, already_modified );

                // �ٷ� ó���Ѵ�.
                IDE_TEST_RAISE( SM_SCN_IS_DELETED( sNxtRowSCN ), already_modified );

                sNextOID = SMP_SLOT_GET_NEXT_OID( sCurSlotHeaderPtr );

                IDE_ASSERT( SM_IS_VALID_OID( sNextOID ) );

                // Next Row �� �ִ� ��� ���󰣴�
                // Next Row�� Page ID�� �ٸ��� Page Latch�� �ٽ� ��´�.
                IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                                   sNextOID,
                                                   (void**)&sCurSlotHeaderPtr )
                            == IDE_SUCCESS );

                sNxtPageID = SMP_SLOT_GET_PID( sCurSlotHeaderPtr );

                IDE_ASSERT( sNxtPageID != SM_NULL_PID );

                if( sPageID != sNxtPageID )
                {
                    *aState = 0;
                    IDE_TEST( smmManager::releasePageLatch( sSpaceID, sPageID )
                              != IDE_SUCCESS );
                    // to Next

                    IDE_TEST(smmManager::holdPageXLatch( sSpaceID, sNxtPageID ) != IDE_SUCCESS);
                    *aState = 1;
                    sPageID = sNxtPageID;
                }
            }

            /* BUG-39233
             * lock wait�� �ѹ� �ߴµ� ��� ���� Tx �� mLimitSCN�� �������ϰ� �ִ°�� */
            if( (sWaitTransID != SM_NULL_TID) &&
                (sWaitTransID == sNxtRowTID) )
            {
                sNextOID = SMP_SLOT_GET_NEXT_OID( sCurSlotHeaderPtr );

                // Next Row �� �ִ� ��� ���󰣴�.
                if( sNextOID != SM_NULL_OID )
                {
                    IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                                       sNextOID,
                                                       (void**)&sSlotHeaderPtr )
                                == IDE_SUCCESS );

                    // next�� mCreateSCN�� commit�� scn�̸� mLimitSCN�� ������ �����Ѵ�.
                    if( SM_SCN_IS_NOT_INFINITE( sSlotHeaderPtr->mCreateSCN ) )
                    {
                        // trc log
                        ideLog::log( IDE_SERVER_0,
                                     "recordLockValidation() invalid mLimitSCN\n"
                                     "TID %"ID_UINT32_FMT", "
                                     "Next Row TID %"ID_UINT32_FMT", "
                                     "SCN %"ID_XINT64_FMT"\n"
                                     "RetryInfo : %"ID_XPOINTER_FMT"\n",
                                     sTransID,
                                     sNxtRowTID,
                                     SM_SCN_TO_LONG( sNxtRowSCN ),
                                     aRetryInfo );

                        smpFixedPageList::dumpSlotHeader( sCurSlotHeaderPtr );

                        ideLog::logMem( IDE_SERVER_0,
                                        (UChar*)sCurSlotHeaderPtr,
                                        aHeader->mFixed.mMRDB.mSlotSize );

                        smpFixedPageList::dumpFixedPage( sSpaceID,
                                                         sPageID,
                                                         aHeader->mFixed.mMRDB.mSlotSize );

                        /* debug ��忡���� assert ��Ű��,
                         * release ��忡���� mLimitSCN ���� ��, already_modified�� fail ��Ŵ */
                        IDE_DASSERT( 0 );

                        // mLimitSCN�� ����
                        SM_SET_SCN( &(sCurSlotHeaderPtr->mLimitSCN), &(sSlotHeaderPtr->mCreateSCN) );

                        // set dirty page
                        IDE_TEST( smmDirtyPageMgr::insDirtyPage( sSpaceID, sPageID )
                                  != IDE_SUCCESS );

                        IDE_RAISE( already_modified );
                    }
                }
            }
            else
            {
                /* do nothing */
            }
        }//else

        sWaitTransID = sNxtRowTID;

        *aState = 0;
        IDE_TEST( smmManager::releasePageLatch( sSpaceID, sPageID )
                  != IDE_SUCCESS );

        IDU_FIT_POINT_RAISE( "1.BUG-39168@smcRecord::recordLockValidation",
                              abort_timeout );
        /* BUG-39168 �� �Լ����� ���ѷ����� ���� �������� ���Ͽ�
         * timeout �� Ȯ���ϴ� code �߰��մϴ�. */
        sSessionChkCnt++;
        IDE_TEST_RAISE( iduCheckSessionEvent( smxTrans::getStatistics( aTrans ) )
                        != IDE_SUCCESS, abort_timeout );

        /* Next Version�� Transaction�� ���������� ��ٸ���. */
        IDE_TEST( smLayerCallback::waitLockForTrans( aTrans,
                                                     sWaitTransID,
                                                     sSpaceID,
                                                     aLockWaitTime )
                  != IDE_SUCCESS );

        IDE_TEST(smmManager::holdPageXLatch(sSpaceID, sPageID) != IDE_SUCCESS);
        *aState = 1;
    } // end of while

    if( sCurSlotHeaderPtr != sPrvSlotHeaderPtr )
    {
        IDE_ASSERT( aRetryInfo != NULL );
        IDE_ASSERT( aRetryInfo->mIsRowRetry == ID_FALSE );

        // Cur�� prv�� ���Ѵ�.
        if( aRetryInfo->mStmtRetryColLst != NULL )
        {
            IDE_TEST_RAISE( isSameColumnValue( sSpaceID,
                                               aRetryInfo->mStmtRetryColLst,
                                               sPrvSlotHeaderPtr,
                                               sCurSlotHeaderPtr )
                            == ID_FALSE , already_modified );
        }

        if( aRetryInfo->mRowRetryColLst != NULL )
        {
            IDE_TEST_RAISE( isSameColumnValue( sSpaceID,
                                               aRetryInfo->mRowRetryColLst,
                                               sPrvSlotHeaderPtr,
                                               sCurSlotHeaderPtr )
                            == ID_FALSE , need_row_retry );
        }
    }

    *aRow = (SChar*)sCurSlotHeaderPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( abort_timeout );
    {
        /* BUG-47457
         * ù��°loop������, �� �Լ������� ������ session out�� �� �� �ִ�.
         * �ι�°loop������, ù��° loop �������� page x latch ������ session out �� �� �ִ�.
         * ����°loop���ʹ� �Ϲ������� ���� ����̴�. TRACE LOG�� �����. */
        if ( sSessionChkCnt >= 3 )
        {
            /* BUG-39168 �� �Լ����� ���ѷ����� ���� �������� ���Ͽ�
             * timeout �� Ȯ���ϴ� code �߰��մϴ�. */
            ideLog::log( IDE_SERVER_0,
                         "recordLockValidation() timeout\n"
                         "TID %"ID_UINT32_FMT", "
                         "Next Row TID %"ID_UINT32_FMT", "
                         "SCN %"ID_XINT64_FMT"\n"
                         "RetryInfo : %"ID_XPOINTER_FMT"\n",
                         sTransID,
                         sNxtRowTID,
                         SM_SCN_TO_LONG( sNxtRowSCN ),
                         aRetryInfo );

            smpFixedPageList::dumpSlotHeader( sCurSlotHeaderPtr );

            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)sCurSlotHeaderPtr,
                            aHeader->mFixed.mMRDB.mSlotSize );

            smpFixedPageList::dumpFixedPage( sSpaceID,
                                             sPageID,
                                             aHeader->mFixed.mMRDB.mSlotSize );
        }
    }
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
                             "TableOID:%"ID_vULONG_FMT", "
                             "ViewSCN:%"ID_UINT64_FMT", "
                             "CreateSCN:%"ID_UINT64_FMT", "
                             "LimitSCN:%"ID_UINT64_FMT,
                             sSpaceID,
                             aHeader->mSelfOID,
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
idBool smcRecord::isSameColumnValue( scSpaceID               aSpaceID,
                                     const smiColumnList   * aChkColumnList,
                                     smpSlotHeader         * aPrvSlotHeaderPtr,
                                     smpSlotHeader         * aCurSlotHeaderPtr )
{
    const smiColumn       * sColumn;
    const smiColumnList   * sCurColumnList;

    SChar           * sPrvRowPtr = (SChar*)aPrvSlotHeaderPtr;
    SChar           * sCurRowPtr = (SChar*)aCurSlotHeaderPtr;
    SChar           * sPrvValue  = NULL;
    SChar           * sCurValue  = NULL;
    smVCDesc        * sPrvVCDesc;
    smVCDesc        * sCurVCDesc;
    smOID             sPrvVCPieceOID;
    smOID             sCurVCPieceOID;
    smVCPieceHeader * sPrvVCPieceHeader = NULL;
    smVCPieceHeader * sCurVCPieceHeader = NULL;
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

                if ( idlOS::memcmp( sCurRowPtr + sColumn->offset,
                                    sPrvRowPtr + sColumn->offset,
                                    sColumn->size ) != 0 )
                {
                    sIsSameColumnValue = ID_FALSE;
                }
                else
                {
                    /* do nothing */
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

            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                /* PROJ-2419 */
                sPrvVCDesc = getVCDesc( sColumn, sPrvRowPtr );
                sCurVCDesc = getVCDesc( sColumn, sCurRowPtr );

                if ( sPrvVCDesc->length != sCurVCDesc->length )
                {
                    sIsSameColumnValue = ID_FALSE;
                    break;
                }
                else
                {
                    /* do nothing */
                }

                // ���ƾ� �ϴ��� Ȯ��
                IDE_ASSERT( ( sPrvVCDesc->flag & SM_VCDESC_MODE_MASK ) ==
                            ( sCurVCDesc->flag & SM_VCDESC_MODE_MASK ) );

                if ( sCurVCDesc->length != 0 )
                {
                    if ( SM_VCDESC_IS_MODE_IN( sCurVCDesc) )
                    {
                        if ( idlOS::memcmp( (smVCDescInMode*)sPrvVCDesc + 1,
                                            (smVCDescInMode*)sCurVCDesc + 1,
                                            sCurVCDesc->length ) != 0)
                        {
                            sIsSameColumnValue = ID_FALSE;
                            break;
                        }
                        else
                        {
                            /* do nothing */
                        }
                    }
                    else
                    {
                        IDE_DASSERT( ( sCurVCDesc->flag & SM_VCDESC_MODE_MASK ) == SM_VCDESC_MODE_OUT );

                        if ( sPrvVCDesc->fstPieceOID == sCurVCDesc->fstPieceOID )
                        {
                            // update�� ������
                            break;
                        }
                        else
                        {
                            /* do nothing */
                        }

                        sPrvVCPieceOID = sPrvVCDesc->fstPieceOID;
                        sCurVCPieceOID = sCurVCDesc->fstPieceOID;

                        while ( ( sPrvVCPieceOID != SM_NULL_OID ) &&
                                ( sCurVCPieceOID != SM_NULL_OID ) )
                        {
                            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                                               sPrvVCPieceOID,
                                                               (void**)&sPrvVCPieceHeader )
                                        == IDE_SUCCESS );

                            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                                               sCurVCPieceOID,
                                                               (void**)&sCurVCPieceHeader )
                                        == IDE_SUCCESS );

                            if ( sPrvVCPieceHeader->length != sCurVCPieceHeader->length )
                            {
                                sIsSameColumnValue = ID_FALSE;
                                break;
                            }
                            else
                            {
                                /* do nothing */
                            }

                            if ( idlOS::memcmp( sPrvVCPieceHeader + 1,
                                                sCurVCPieceHeader + 1,
                                                sCurVCPieceHeader->length ) != 0 )
                            {
                                sIsSameColumnValue = ID_FALSE;
                                break;
                            }
                            else
                            {
                                /* do nothing */
                            }

                            sPrvVCPieceOID = sPrvVCPieceHeader->nxtPieceOID;
                            sCurVCPieceOID = sCurVCPieceHeader->nxtPieceOID;
                        }

                        if ( sPrvVCPieceOID != sCurVCPieceOID )
                        {
                            // �� �� �ϳ��� null�� �ƴ� ���
                            sIsSameColumnValue = ID_FALSE;
                            break;
                        }
                        else
                        {
                            /* do nothing */
                        }
                    }
                }
                else
                {
                    /* do nothing */
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

                if ( SM_VCDESC_IS_MODE_IN( sCurLobDesc ) )
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
 *  3. ���� Row�� SCN�� ���Ѵ���, Row�� TID�� �� ������ �����ϴ�
 *     Transaction�� �����ؾ� �Ѵ�.
 *  4. ���Ѵ밡 �ƴ϶�� Row�� SCN�� �ݵ�� aViewSCN���� �۾ƾ� �Ѵ�.
 *
 *  aRowPtr   - [IN] Row Pointer
 *  aSpaceID  - [IN] Row�� Table�� ���� Tablespace�� ID
 *  aViewSCN  - [IN] View SCN
 *  aRowPtr   - [IN] ������ ������ Row Pointer
 *  aRetryInfo- [IN] Retry Info
 *
 ***********************************************************************/
 IDE_RC smcRecord::validateUpdateTargetRow( void                  * aTrans,
                                            scSpaceID               aSpaceID,
                                            smSCN                   aViewSCN,
                                            void                  * aRowPtr,
                                            const smiDMLRetryInfo * aRetryInfo )
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
                /* PROJ-2694 Fetch Across Rollback */
                if ( ( sTrans->mCursorOpenInfSCN != SM_SCN_INIT ) &&
                     ( SM_SCN_IS_LT( &sRowSCN, &(sTrans->mCursorOpenInfSCN) ) == ID_TRUE ) )
                {
                    /* holdable cursor open ���� �ڽ��� ������ row�� �������� ���
                     * �ش� Tx�� rollback�� cursor�� ��Ȱ���� �� ����. */
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
                     "TID:[%u] View SCN:[%llx]\n",
                     smLayerCallback::getTransID( aTrans ),
                     SM_SCN_TO_LONG( aViewSCN ) );

        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     "[##WARNING ##WARNING!!] Target row's info\n");
        logSlotInfo(aRowPtr);

        sRowOID = SMP_SLOT_GET_NEXT_OID( sSlotHeader );

        if( SM_IS_VALID_OID( sRowOID ) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                         "[##WARNING ##WARNING!!] Next row of target row info\n");
            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID, 
                                               sRowOID,
                                               (void**)&sLogSlotPtr )
                        == IDE_SUCCESS );
            logSlotInfo( sLogSlotPtr );
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
IDE_RC smcRecord::deleteVC( void              *aTrans,
                            smcTableHeader    *aHeader,
                            smOID              aFstOID,
                            idBool             aLogging )  
{
    smOID             sVCPieceOID       = aFstOID;
    smOID             sNxtVCPieceOID    = SM_NULL_OID;
    SChar            *sVCPiecePtr       = NULL;

    /* VC�� �������� VC Piece������ ��� ���
       Piece�� ���ؼ� ���� �۾��� �����Ѵ�. */
    while ( sVCPieceOID != SM_NULL_OID )
    {
        IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID, 
                                           sVCPieceOID,
                                           (void**)&sVCPiecePtr )
                == IDE_SUCCESS );

        sNxtVCPieceOID = ((smVCPieceHeader*)sVCPiecePtr)->nxtPieceOID;

        //PROJ-2419 delete VC
        IDU_FIT_POINT( "1.PROJ-2419@smcRecord::deleteVC::setFreeFlagAtVCPieceHdr" );

        IDE_TEST( setFreeFlagAtVCPieceHdr(aTrans,
                                          aHeader->mSelfOID,
                                          aHeader->mSpaceID,
                                          sVCPieceOID,
                                          sVCPiecePtr,
                                          SM_VCPIECE_FREE_OK,
                                          aLogging)
                != IDE_SUCCESS);

        //PROJ-2419 delete VC
        IDU_FIT_POINT( "2.PROJ-2419@smcRecord::deleteVC::addOID" );

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
 * Description : aRow�� aColumn�� ����Ű�� Lob Column�� �����.
 *
 * aTrans    - [IN] Transaction Pointer
 * aHeader   - [IN] Table Header
 * aColumn   - [IN] Table Column List
 * aRow      - [IN] Row Pointer
 ***********************************************************************/
IDE_RC smcRecord::deleteLob(void              *aTrans,
                            smcTableHeader    *aHeader,
                            const smiColumn   *aColumn,
                            SChar             *aRow,
                            idBool             aLogging)  //BUG-46854
{
    smVCDesc     *sVCDesc;
    smcLobDesc   *sCurLobDesc;

    IDE_ASSERT( aHeader != NULL );
    IDE_ASSERT( aColumn != NULL );
    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT_MSG( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
                    == SMI_COLUMN_TYPE_LOB,
                    "flag : %"ID_UINT32_FMT"\n",
                    aColumn->flag );

    sCurLobDesc = (smcLobDesc*)(aRow + aColumn->offset);

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
        IDE_TEST(deleteVC(aTrans, aHeader, sVCDesc->fstPieceOID, aLogging)
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
 * aRowPtr�� SlotHeader������ boot log�� �����.
 * aRow�� mTID�� mNext�� �������� ���� �� �����Ƿ�, ������ �ʴ� ���� ���ڷ�
 * ���� �޴´�.
 * aRow - [IN]: Row Pointer
 ***********************************************************************/
void smcRecord::logSlotInfo(const void *aRow)
{
    smpSlotHeader *sSlotHeader;
    scPageID       sPageID;
    smOID          sRecOID;
    smSCN          sSCN;
    smTID          sTID;
    smOID          sNextOID;

    sSlotHeader = (smpSlotHeader *)aRow;
    sPageID = SMP_SLOT_GET_PID(aRow);

    sRecOID = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( sSlotHeader ) );

    SMX_GET_SCN_AND_TID( sSlotHeader->mCreateSCN, sSCN, sTID );
    sNextOID = SMP_SLOT_GET_NEXT_OID( sSlotHeader );

    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                "TID:[%u] "
                "Row Info: OID [%lu] "
                "SCN:[%llx],"
                "Next:[%lu],"
                "Flag:[%llx]\n",
                sTID,
                sRecOID,
                SM_SCN_TO_LONG( sSCN ),
                sNextOID,
                SMP_SLOT_GET_FLAGS( sSlotHeader ) );
}

/***********************************************************************
 * description : aRow���� aColumn�� ����Ű�� vc�� Header�� ���Ѵ�.
 *
 * aRow             -   [in] fixed row pointer
 * aColumn          -   [in] column desc
 * aVCPieceHeader   -  [out] return VCPieceHeader pointer
 * aOffsetIdx       -  [out] return OffsetIdx in vc piece
 ***********************************************************************/
IDE_RC smcRecord::getVCPieceHeader( const void      *  aRow,
                                    const smiColumn *  aColumn,
                                    smVCPieceHeader ** aVCPieceHeader,
                                    UShort          *  aOffsetIdx )
{
    UShort            sOffsetIdx        = ID_USHORT_MAX;
    smVCPieceHeader * sVCPieceHeader    = NULL;
    smOID             sOID              = SM_NULL_OID;

    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT( aColumn != NULL );

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

/***********************************************************************
 * Description: Variable Length Data�� �д´�. ���� ���̰� �ִ� Piece���� ����
 *              �۰� �о�� �� Piece�� �ϳ��� �������� �����Ѵٸ� �о�� �� ��ġ��
 *              ù��° ����Ʈ�� ��ġ�� �޸� �����͸� �����Ѵ�. �׷��� ���� ��� Row��
 *              ���� �����ؼ� �Ѱ��ش�.
 *
 * aRow         - [IN] �о���� �ķ��� ������ �ִ� Row Pointer
 * aColumn      - [IN] �о���� �÷��� ����
 * aFstPos      - [IN] �о���� Row�� ù��° ��ġ
 * aLength      - [IN-OUT] IN : IsReturnLength == ID_FALSE
 *                         OUT: isReturnLength == ID_TRUE
 * aBuffer      - [IN] value�� �����.
 *
 ***********************************************************************/
SChar* smcRecord::getVarRow( const void      * aRow,
                             const smiColumn * aColumn,
                             UInt              aFstPos,
                             UInt            * aLength,
                             SChar           * aBuffer,
                             idBool            aIsReturnLength )
{
    smVCPieceHeader * sVCPieceHeader    = NULL;
    SChar           * sRet              = NULL;
    UShort          * sCurrOffsetPtr    = NULL;
    UShort            sOffsetIdx        = 0;

    IDE_ASSERT( aRow    != NULL );
    IDE_ASSERT( aColumn != NULL );
    IDE_ASSERT_MSG( ( ( aColumn->flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_VARIABLE ) ||
                    ( ( aColumn->flag & SMI_COLUMN_TYPE_MASK )
                      == SMI_COLUMN_TYPE_VARIABLE_LARGE ),
                    "flag : %"ID_UINT32_FMT"\n",
                    aColumn->flag );
    IDE_ASSERT_MSG( ( aColumn->flag & SMI_COLUMN_STORAGE_MASK )
                    == SMI_COLUMN_STORAGE_MEMORY,
                    "flag : %"ID_UINT32_FMT"\n",
                    aColumn->flag );

    if ( ( aColumn->flag & SMI_COLUMN_TYPE_MASK ) != SMI_COLUMN_TYPE_VARIABLE_LARGE )
    {    /* United Var �� �׻� aIsReturnLength�� ���� ���� length �� �����ص��ȴ�.  */
        if (( (smpSlotHeader*)aRow)->mVarOID == SM_NULL_OID )
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
    }
    else
    {
        sRet = getVarLarge( aRow,
                            aColumn,
                            aFstPos,
                            aLength,
                            aBuffer,
                            aIsReturnLength );
    }

    return sRet;
}

SChar* smcRecord::getVarLarge( const void      * aRow,
                               const smiColumn * aColumn,
                               UInt              aFstPos,
                               UInt            * aLength,
                               SChar           * aBuffer,
                               idBool            aIsReturnLength )
{
    smVCDesc * sVCDesc;
    SChar    * sRow;
    SChar    * sRet;
    
    sRow    = (SChar*)aRow + aColumn->offset;
    sVCDesc = (smVCDesc*)sRow;

    /* 32k �Ѵ� Large var value �� �߶� �д� ��쿡��
     * isReturnLength �� FALSE �� ���´� */
    if ( aIsReturnLength == ID_TRUE )
    {
        *aLength = sVCDesc->length;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sVCDesc->length != 0 )
    {
        if ( SM_VCDESC_IS_MODE_IN( sVCDesc) )
        {
            /* Varchar InMode�����*/
            sRet = (SChar*)sRow + ID_SIZEOF( smVCDescInMode );
        }
        else
        {
            /* Varchar OutMode�����*/
            sRet = smpVarPageList::getValue( aColumn->colSpace,
                                             aFstPos,
                                             *aLength,
                                             sVCDesc->fstPieceOID,
                                             aBuffer );
        }
    }
    else
    {
        /* Nothing to do */
        sRet = NULL;
    }
    return sRet;
}

/***********************************************************************
 * description : arow���� acolumn�� ����Ű�� value�� length�� ���Ѵ�.
 *
 * aRowPtr  - [in] fixed row pointer
 * aColumn  - [in] column desc
 ***********************************************************************/
UInt smcRecord::getVarColumnLen( const smiColumn    * aColumn,
                                 const SChar        * aRowPtr )
{
    smVCPieceHeader * sVCPieceHeader = NULL;
    smVCDesc        * sVCDesc        = NULL;
    UShort          * sCurrOffsetPtr = NULL;
    UShort            sLength        = 0;
    UShort            sOffsetIdx     = ID_USHORT_MAX;

    IDE_DASSERT( ( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
                   == SMI_COLUMN_TYPE_VARIABLE) ||
                 ( (aColumn->flag & SMI_COLUMN_TYPE_MASK)
                   == SMI_COLUMN_TYPE_VARIABLE_LARGE) );

    if ( (aColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE_LARGE )
    {
        sVCDesc = (smVCDesc*)(aRowPtr + aColumn->offset);
        sLength = sVCDesc->length;
    }
    else
    {
        if (( (smpSlotHeader*)aRowPtr)->mVarOID == SM_NULL_OID )
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
UInt smcRecord::getColumnLen( const smiColumn * aColumn,
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
        case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            sLength = getVarColumnLen( aColumn, (const SChar*)aRowPtr );
            break;

        case SMI_COLUMN_TYPE_FIXED:
            sLength = aColumn->size;
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return sLength;
}

UInt smcRecord::getUnitedVCLogSize( const smcTableHeader* aHeader, smOID aOID )
{
    UInt                sUnitedVCLogSize    = 0;
    UInt                sVCSize             = 0;
    smVCPieceHeader   * sVCPieceHeader      = NULL;
    smOID               sOID                = aOID;

    sUnitedVCLogSize += ID_SIZEOF(smOID);                         /* First Piece OID size */

    sUnitedVCLogSize += ID_SIZEOF(UShort);                        /* Column Count size */

    sUnitedVCLogSize += ID_SIZEOF(UInt) * aHeader->mUnitedVarColumnCount; /* Column id list size */

    IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                       sOID,
                                       (void**)&sVCPieceHeader)
            == IDE_SUCCESS );

    while ( sVCPieceHeader != NULL )
    {
        // end offset ������
        sVCSize = *((UShort*)(sVCPieceHeader + 1) + sVCPieceHeader->colCount) - ID_SIZEOF(smVCPieceHeader);

        sUnitedVCLogSize    += sVCSize;         /* value size */

        /* Next OID, Column count in this piece */
        sUnitedVCLogSize    += ID_SIZEOF(smOID) + ID_SIZEOF(UShort);

        if ( sVCPieceHeader->nxtPieceOID != SM_NULL_OID )
        {
            sOID    = sVCPieceHeader->nxtPieceOID;

            IDE_ASSERT( smmManager::getOIDPtr( aHeader->mSpaceID,
                                               sOID,
                                               (void**)&sVCPieceHeader)
                    == IDE_SUCCESS );
        }
        else
        {
            sVCPieceHeader = NULL;
        }
    }

    return sUnitedVCLogSize;
}

UInt smcRecord::getUnitedVCColCount( scSpaceID aSpaceID, smOID aOID )
{
    UInt                sRet            = 0;
    smVCPieceHeader   * sVCPieceHeader  = NULL;
    smOID               sOID            = aOID;

    IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                       sOID,
                                       (void**)&sVCPieceHeader)
            == IDE_SUCCESS );

    while ( sVCPieceHeader != NULL )
    {
        sRet += sVCPieceHeader->colCount;

        if ( sVCPieceHeader->nxtPieceOID != SM_NULL_OID )
        {
            sOID    = sVCPieceHeader->nxtPieceOID;

            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                               sOID,
                                               (void**)&sVCPieceHeader)
                    == IDE_SUCCESS );
        }
        else
        {
            sVCPieceHeader = NULL;
        }
    }

    return sRet;

}
