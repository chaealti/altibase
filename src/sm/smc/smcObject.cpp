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
 * $Id: smcObject.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smp.h>
#include <smcDef.h>
#include <smcObject.h>
#include <smcReq.h>
#include <smx.h>

IDE_RC smcObject::createObject( void*              aTrans,
                                const void*        aInfo,
                                UInt               aInfoSize,
                                const void*        aTempInfo,
                                smcObjectType      aObjectType,
                                smcTableHeader**   aTable )
{

    smcTableHeader*          sHeader;
    smOID                    sFixOid;
    smcTableHeader           sHeaderArg;
    UInt                     sState = 0;
    scPageID                 sHeaderPageID = 0;
    SChar*                   sNewFixRowPtr;
    smSCN                    sInfiniteSCN;
    scPageID                 sPageID;
    smiSegAttr               sSegmentAttr;
    smiSegStorageAttr        sSegmentStoAttr;
    UInt                     sFlag;
    smTID                    sTID;
    
    sTID = smxTrans::getTransID( aTrans );

    // BUG-37607 Transaction ID should be recorded in the slot containing table header.
    SM_SET_SCN_INFINITE_AND_TID( &sInfiniteSCN, sTID );
    
    /* ---------------------------------------
     * [1] Catalog Table�� ���Ͽ� IX lock ��û
     * --------------------------------------*/
    IDE_TEST( smLayerCallback::lockTableModeIX( aTrans,
                                                SMC_TABLE_LOCK( SMC_CAT_TABLE ) )
              != IDE_SUCCESS );

    /* ------------------------------------------
     * [2] ���ο� object�� ���� Table Header������
     * Catalog Table���� �Ҵ�޴´�.
     * ------------------------------------------*/
    IDE_TEST( smpFixedPageList::allocSlot( aTrans,
                                           SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           NULL,
                                           SMC_CAT_TABLE->mSelfOID,
                                           &(SMC_CAT_TABLE->mFixed.mMRDB),
                                           &sNewFixRowPtr,
                                           sInfiniteSCN,
                                           SMC_CAT_TABLE->mMaxRow,
                                           SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT |
                                           SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER)
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID(sNewFixRowPtr);
    sFixOid = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sNewFixRowPtr ) );
    
    /* ----------------------------------------------------------------
     * [3] ���ο� Object�� ���� Table Header������
     * Catalog Table���� �Ҵ�޾����Ƿ�, New Version List�� �߰��Ѵ�.
     * ----------------------------------------------------------------*/
    sHeaderPageID = SM_MAKE_PID(sFixOid);
    sState        = 1;
    sHeader       = (smcTableHeader*)(sNewFixRowPtr + ID_SIZEOF(smpSlotHeader));

    /* ������� �ʴ� �Ӽ��� �ʱ�ȭ�� ���ش�. */
    idlOS::memset( &sSegmentAttr, 0x00, ID_SIZEOF(smiSegAttr));
    idlOS::memset( &sSegmentStoAttr, 0x00, ID_SIZEOF(smiSegStorageAttr));

    if ( aObjectType == SMI_OBJECT_DATABASE_LINK )
    {
        sFlag = SMI_TABLE_REPLICATION_DISABLE |
            SMI_TABLE_REMOTE |
            SMI_TABLE_LOCK_ESCALATION_DISABLE;
    }
    else
    {
        sFlag = SMI_TABLE_REPLICATION_DISABLE |
            SMI_TABLE_META |
            SMI_TABLE_LOCK_ESCALATION_DISABLE;
    }
    
    /* stack ������ table header �� �ʱ�ȭ */
    IDE_TEST( smcTable::initTableHeader( aTrans,
                                         SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA,
                                         0,
                                         0,
                                         0,
                                         sFixOid,
                                         sFlag,
                                         NULL,
                                         SMC_TABLE_OBJECT,
                                         aObjectType,
                                         ID_ULONG_MAX,
                                         sSegmentAttr,
                                         sSegmentStoAttr,
                                         0, // parallel degree
                                         &sHeaderArg )
              != IDE_SUCCESS );

    /* ���� table header ���� ���� */
    idlOS::memcpy( sHeader, &sHeaderArg, ID_SIZEOF(smcTableHeader));

    IDE_TEST( smcTable::initLockAndRuntimeItem( sHeader ) != IDE_SUCCESS);

    IDE_TEST( smLayerCallback::addOID( aTrans,
                                       SMC_CAT_TABLE->mSelfOID,
                                       sFixOid,
                                       SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                       SM_OID_NEW_INSERT_FIXED_SLOT )
              != IDE_SUCCESS );

    /* ----------------------------------------------------
     * [4] ���ο� object�� Info ������ ���Ͽ� ���� �����Ѵ�.
     * ----------------------------------------------------*/
    IDE_TEST( smcTable::setInfoAtTableHeader( aTrans,
                                              sHeader,
                                              aInfo,
                                              aInfoSize)
              != IDE_SUCCESS );
    
    /* ----------------------------------------------------------
     * [5] ���ο� object�� temp Info ������ ���Ͽ� ���� �����Ѵ�.
     * ---------------------------------------------------------*/
    sHeader->mRuntimeInfo = (void *)aTempInfo;

    /* -------------------------------------------------------------
     * [6] ���ο� object�� header�� ���� �������� ������������ ���
     * ------------------------------------------------------------*/
    IDE_TEST(smmDirtyPageMgr::insDirtyPage(SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                           sHeaderPageID)
             != IDE_SUCCESS);
    
    * aTable = sHeader;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if(sState == 1)
    {
        IDE_PUSH();
        (void)smmDirtyPageMgr::insDirtyPage(
            SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, sHeaderPageID);
        IDE_POP();
    }

    return IDE_FAILURE;
    
}

void smcObject::getObjectInfo( smcTableHeader*  aTable,
                               void**           aObjectInfo )
{
    smVCDesc         *sVCDesc;
    smVCPieceHeader  *sCurVCPieceHeader;
    smOID             sVCPieceOID;
    UInt              sOffset = 0;
    
    sVCDesc = &(aTable->mInfo);

    /* Info�� ������ Out-Mode�� ����. */
    if( sVCDesc->length != 0)
    {
        IDE_ASSERT( (sVCDesc->flag & SM_VCDESC_MODE_MASK)
                     == SM_VCDESC_MODE_OUT );

        sVCPieceOID = sVCDesc->fstPieceOID;

        IDE_ASSERT( sVCPieceOID != SM_NULL_OID );
        
        do
        {
            IDE_ASSERT( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                               sVCPieceOID,
                                               (void **)&sCurVCPieceHeader )
                        == IDE_SUCCESS );
            
            idlOS::memcpy((void *)((char *)(*aObjectInfo) + sOffset), 
                          (void *)(sCurVCPieceHeader+1),
                          sCurVCPieceHeader->length);
            
            sOffset += sCurVCPieceHeader->length;
            sVCPieceOID = sCurVCPieceHeader->nxtPieceOID;
        }
        while (sVCPieceOID != SM_NULL_OID);
    }
    else
    {
        *aObjectInfo = NULL;
    }
}
