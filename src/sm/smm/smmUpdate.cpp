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
 * $Id: smmUpdate.cpp 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smmReq.h>
#include <smmUpdate.h>
#include <smmExpandChunk.h>
#include <sctTableSpaceMgr.h>
#include <smmTBSMultiPhase.h>

/* Update type:  SMR_PHYSICAL                      */
IDE_RC smmUpdate::redo_undo_PHYSICAL(smTID       /*a_tid*/,
                                     scSpaceID   a_spaceid,
                                     scPageID    a_pid,
                                     scOffset    a_offset,
                                     vULong      /*a_data*/,
                                     SChar     * a_pImage,
                                     SInt        a_nSize,
                                     UInt        /*aFlag*/ )
{

    SChar *s_pDataPtr;
    
    if ( a_nSize != 0 )
    {
        IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                           SM_MAKE_OID( a_pid, a_offset ),
                                           (void**)&s_pDataPtr )
                    == IDE_SUCCESS );

        idlOS::memcpy( s_pDataPtr, a_pImage, a_nSize );

        IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid)
                  != IDE_SUCCESS );

    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/* Update type:  SMR_SMM_MEMBASE_INFO                      */
IDE_RC smmUpdate::redo_SMM_MEMBASE_INFO(smTID       /*a_tid*/,
                                        scSpaceID   a_spaceid,
                                        scPageID    a_pid,
                                        scOffset    a_offset,
                                        vULong      /*a_data*/,
                                        SChar     * a_pImage,
                                        SInt        a_nSize,
                                        UInt        /*aFlag*/ )
{

    smmMemBase *sMemBase;

    IDE_ERROR_MSG( (a_pid == SMM_MEMBASE_PAGEID) && (a_nSize == ID_SIZEOF(smmMemBase)),
                   "a_pid : %"ID_UINT32_FMT, a_pid  );
    
    IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                       SM_MAKE_OID( a_pid, a_offset ),
                                       (void**)&sMemBase )
                == IDE_SUCCESS );
    idlOS::memcpy( sMemBase, a_pImage, ID_SIZEOF(smmMemBase) );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/* Update type:  SMR_SMM_MEMBASE_SET_SYSTEM_SCN                      */
IDE_RC smmUpdate::redo_SMM_MEMBASE_SET_SYSTEM_SCN(smTID       /*a_tid*/,
                                                  scSpaceID   a_spaceid,
                                                  scPageID    a_pid,
                                                  scOffset    a_offset,
                                                  vULong      /*a_data*/,
                                                  SChar     * a_pImage,
                                                  SInt        a_nSize,
                                                  UInt        /*aFlag*/ )
{

    smmMemBase *sMemBase;
    smSCN       s_systemSCN;

    IDE_ERROR_MSG( a_nSize == ID_SIZEOF(smSCN),
                   "a_nSize : %"ID_UINT32_FMT, a_nSize  );
    
    IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                       SM_MAKE_OID( a_pid, a_offset ),
                                       (void**)&sMemBase )
                == IDE_SUCCESS );

    idlOS::memcpy( &s_systemSCN, a_pImage, ID_SIZEOF(smSCN) );

    sMemBase->mSystemSCN = s_systemSCN;
    
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/* Update type:  SMR_SMM_MEMBASE_ALLOC_PERS
 *
 *   Membase�� ����ȭ�� Free Page List�� ������ ���뿡 ���� REDO/UNDO �ǽ�
 *
 *   before image :
 *                  aFPLNo
 *                  aBeforeMembase-> mFreePageLists[ aFPLNo ].mFirstFreePageID
 *                  aBeforeMembase-> mFreePageLists[ aFPLNo ].mFreePageCount
 *   after  image :
 *                  aFPLNo
 *                  aAfterPageID
 *                  aAfterPageCount
 */
IDE_RC smmUpdate::redo_undo_SMM_MEMBASE_ALLOC_PERS_LIST(smTID          /*a_tid*/,
                                                        scSpaceID      a_spaceid,
                                                        scPageID       a_pid,
                                                        scOffset       a_offset,
                                                        vULong         /*a_data*/,
                                                        SChar        * a_pImage,
                                                        SInt           /*a_nSize*/,
                                                        UInt           /*aFlag*/ )
{

    smmMemBase * sMembase;
    UInt         sFPLNo ;
    scPageID     sFirstFreePageID ;
    vULong       sFreePageCount ;
    
    IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                       SM_MAKE_OID( a_pid, a_offset ),
                                       (void**)&sMembase )
                == IDE_SUCCESS );

    idlOS::memcpy( & sFPLNo, 
                   a_pImage, 
                   ID_SIZEOF(UInt) );
    a_pImage += ID_SIZEOF(UInt);

    idlOS::memcpy( & sFirstFreePageID,
                   a_pImage,
                   ID_SIZEOF(scPageID) );
    a_pImage += ID_SIZEOF(scPageID);

    idlOS::memcpy( & sFreePageCount,
                   a_pImage,
                   ID_SIZEOF(vULong) );
    a_pImage += ID_SIZEOF(vULong);


    // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� ���� ������,
    // Redo�߿��� Free Page List�� Page���� üũ���� �ʴ´�.
    IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                            & sMembase->mFreePageLists[ sFPLNo ] )
                 == ID_TRUE );
    
    sMembase->mFreePageLists[ sFPLNo ].mFirstFreePageID = sFirstFreePageID ;
    sMembase->mFreePageLists[ sFPLNo ].mFreePageCount = sFreePageCount;

    // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� ���� ������,
    // Redo�߿��� Free Page List�� Page���� üũ���� �ʴ´�.
    IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                            & sMembase->mFreePageLists[ sFPLNo ] )
                 == ID_TRUE );
    

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid) != IDE_SUCCESS );


    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}




/* Update type:  SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK

   Expand Chunk�Ҵ翡 ���� logical redo �ǽ� 
   
   smmManager::allocNewExpandChunk�� Logical Redo�� ����
   Membase �Ϻ� ����� ����� Before �̹����� �̿��Ѵ�.           
   
   after  image: aBeforeMembase->m_alloc_pers_page_count
                 aBeforeMembase->mCurrentExpandChunkCnt
                 aBeforeMembase->mExpandChunkPageCnt
                 aBeforeMembase->m_nDBFileCount[0]
                 aBeforeMembase->m_nDBFileCount[1]
                 aBeforeMembase->mFreePageListCount
                 [
                      aBeforeMembase-> mFreePageLists[i].mFirstFreePageID
                      aBeforeMembase-> mFreePageLists[i].mFreePageCount
                 ] ( aBeforeMembase->mFreePageListCount ��ŭ )
                 aExpandPageListID
                 aAfterChunkFirstPID                          
                 aAfterChunkLastPID
                 
   aExpandPageListID : Ȯ��� Chunk�� Page�� �Ŵ޸� Page List�� ID
                       UINT_MAX�� ��� ��� Page List�� ���� �й�ȴ�
*/                                                                   
IDE_RC smmUpdate::redo_SMM_MEMBASE_ALLOC_EXPAND_CHUNK( smTID       /*a_tid*/,
                                                       scSpaceID     a_spaceid,
                                                       scPageID      a_pid,
                                                       scOffset      a_offset,
                                                       vULong      /*a_data*/,
                                                       SChar        *a_pImage,
                                                       SInt        /*a_nSize*/,
                                                       UInt        /*aFlag*/ )
{

    smmMemBase    * sMembase;
    smmMemBase    * sOrgMembase;
    UInt            sOrgDBFileCount;
    
    // Log���� �о�� �����θ� Membase�� �����Ͽ�
    // Logical Redo�� �����Ѵ�.
    // �׸��� Logical Redo�� �����ϰ� �� ����� ���� Membase���� �ʵ尪���� 
    // ���� Membase�� �ʵ尪�� �����Ѵ�.
    //
    // �̷��� �ϴ� ������, Restart Recovery���߿��� Membase�� �������� ����
    // �ֱ� ������, Membase���� �ʵ��� Log���� �о�� �ʵ� ���� �ٸ�
    // �ʵ���� ��� 0���� �����س��� Logical Redo�� �����Ͽ�,
    // ����ġ ���� Membase �ʵ� ������ �̿��� Detect�ϱ� �����̴�.
    smmMemBase      sLogicalRedoMembase;
    
    UInt            sFreePageListCount;
    scPageID        sNewChunkFirstPID;
    scPageID        sNewChunkLastPID;
    scPageID        sFirstFreePageID;
    vULong          sFreePageCount;
    UInt            sExpandPageListID;    
    UInt            sDbFileNo;
    UInt            i;
    smmTBSNode    * sTBSNode;
    SInt            sWhichDB;
    idBool          sIsCreatedDBFile;

    IDE_ERROR_MSG( a_pid == SMM_MEMBASE_PAGEID,
                   "a_pid : %"ID_UINT32_FMT, a_pid  );
    
    
    idlOS::memset( & sLogicalRedoMembase,
                   0,
                   ID_SIZEOF( smmMemBase ) );
    
    // Logical Redo�� ������ �Ǿ ���������� �ϱ� ����
    // Logical Redo���� Membase�� ������ �α� ��� ��÷� �������´�.
    
    // Log�м� ���� ++++++++++++++++++++++++++++++++++++++++++++++
    idlOS::memcpy( & sLogicalRedoMembase.mAllocPersPageCount,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mAllocPersPageCount ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mAllocPersPageCount );
    
    idlOS::memcpy( & sLogicalRedoMembase.mCurrentExpandChunkCnt,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mCurrentExpandChunkCnt ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mCurrentExpandChunkCnt );

    idlOS::memcpy( & sLogicalRedoMembase.mExpandChunkPageCnt,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mExpandChunkPageCnt ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mExpandChunkPageCnt );
    
    idlOS::memcpy( & sLogicalRedoMembase.mDBFileCount[0],
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[0] ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[0] );

    idlOS::memcpy( & sLogicalRedoMembase.mDBFileCount[1],
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[1] ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[1] );

    
    idlOS::memcpy( & sFreePageListCount,
                   a_pImage,
                   ID_SIZEOF(UInt) );
    a_pImage += ID_SIZEOF(UInt);
    sLogicalRedoMembase.mFreePageListCount = sFreePageListCount;

    for ( i = 0 ;
          i < (UInt) sFreePageListCount ;
          i ++ )
    {
        idlOS::memcpy( & sFirstFreePageID,
                       a_pImage,
                       ID_SIZEOF(scPageID) );
        a_pImage += ID_SIZEOF(scPageID);
        
        idlOS::memcpy( & sFreePageCount,
                       a_pImage,
                       ID_SIZEOF(vULong) );
        a_pImage += ID_SIZEOF(vULong);

        
        sLogicalRedoMembase.mFreePageLists[i].mFirstFreePageID = sFirstFreePageID;
        sLogicalRedoMembase.mFreePageLists[i].mFreePageCount = sFreePageCount;

        // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� ���� ������,
        // Redo�߿��� Free Page List�� Page���� üũ���� �ʴ´�.
        IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                                & sLogicalRedoMembase.mFreePageLists[i] )
                     == ID_TRUE );
    } // end of for

    idlOS::memcpy( & sExpandPageListID, 
                   a_pImage, 
                   ID_SIZEOF(UInt) );
    a_pImage += ID_SIZEOF(UInt);
    
    idlOS::memcpy( & sNewChunkFirstPID, 
                   a_pImage, 
                   ID_SIZEOF(scPageID) );
    a_pImage += ID_SIZEOF(scPageID);

    idlOS::memcpy( & sNewChunkLastPID, 
                   a_pImage, 
                   ID_SIZEOF(scPageID) );
    a_pImage += ID_SIZEOF(scPageID);
    // Log�м� �� ------------------------------------------------

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( a_spaceid,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );

    sLogicalRedoMembase.mDBFilePageCount = sTBSNode->mMemBase->mDBFilePageCount;
    
    // Logical Redo ������ ���� Membase�� �ٲ�ģ��.
    sOrgMembase = sTBSNode->mMemBase;
    sTBSNode->mMemBase = & sLogicalRedoMembase;

    // Logical Redo ������ DB File�� 
    sOrgDBFileCount = sLogicalRedoMembase.mDBFileCount[0];

    // Logical Redo�� Expand Chunk�� �Ҵ��Ѵ�.
    IDE_TEST( smmManager::allocNewExpandChunk( sTBSNode,
                                               // aTrans�� NULL�� �Ѱܼ� Redo������ �˸���
                                               // Expand Chunk�Ҵ��߿� Log��Ϲ� Page���� üũ�� ���� �ʴ´�.
                                               NULL,
                                               sNewChunkFirstPID,
                                               sNewChunkLastPID )
              != IDE_SUCCESS );
 
    // Chunk�Ҵ� ���� ���� ���ܳ� DB���Ͽ� ���� ó��
    //
    // sOrgDBFileCount 
    //    logical redo������ DB File��  => ���� ������ ù��° DB File ��ȣ
    // sLogicalRedoMembase.m_nDBFileCount[0]-1
    //    �� Membase�� DB File�� -1 => ���� ������ ������ DB File ��ȣ
    //
    // ( m_nDBFileCount[0], [1] ��� �׻� ���� ������ �����ǹǷ�,
    //   �ƹ� ���̳� ����ص� �ȴ�. )
    for ( sDbFileNo  = sOrgDBFileCount; 
          sDbFileNo <= sLogicalRedoMembase.mDBFileCount[0]-1 ;
          sDbFileNo ++ )
    {
        // DB File�� �����ϸ� open�ϰ� ������ �����Ѵ�.
        // ���� ������ ��� ������ �ؾ��ϴ� ��쿡�� 
        // CreateLSN�� �̹� ������ allocNewExpandChunk���� 
        // Stable/Unstable Chkpt image�� Hdr�� ������ �س��ұ� ������
        // ���⼭ �׳� ���ϸ� �����ϸ� �ȴ�. 
        IDE_TEST( smmManager::openOrCreateDBFileinRecovery( sTBSNode,
                                                            sDbFileNo,
                                                            &sIsCreatedDBFile )
                  != IDE_SUCCESS );

        /* PROJ-2386 DR
         * DR Standby�� checkpoint������ ChkptImageHdr.mSpaceID�� �������� �ʾ�
         * TBS DROP ���� �ʴ� ���� ����. */
        if ( sIsCreatedDBFile == ID_TRUE )
        {
            for ( sWhichDB = 0;
                  sWhichDB < SMM_PINGPONG_COUNT;
                  sWhichDB++ )
            {
                IDE_TEST( smmManager::flushTBSMetaPage( sTBSNode, sWhichDB )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* nothing to do */
        }

        // ��� DB File�� �����ϴ� ��Ż Page Count ���.
        // Performance View���̸� ��Ȯ�� ��ġ���� �ʾƵ� �ȴ�.
        IDE_TEST( smmManager::calculatePageCountInDisk( sTBSNode )
                  != IDE_SUCCESS );
    }

    // To Fix BUG-15112
    // Restart Recovery�߿� Page Memory�� NULL�� Page�� ���� Redo��
    // �ش� �������� �׶� �׶� �ʿ��� ������ �Ҵ��Ѵ�.
    // ���⿡�� ������ �޸𸮸� �̸� �Ҵ��ص� �ʿ䰡 ����
    
    // Membase�� ������� �������´�.
    sTBSNode->mMemBase = sOrgMembase;
    
    IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                       SM_MAKE_OID( a_pid, a_offset ),
                                       (void**)&sMembase )
                == IDE_SUCCESS );
    IDE_ERROR_MSG( sMembase == sOrgMembase,
                   "sMembase    : %"ID_vULONG_FMT"\n"
                   "sOrgMembase : %"ID_vULONG_FMT"\n",
                   sMembase,
                   sOrgMembase );

    // Logical Redo�� ����� ������� Membase�� ������ ���� Membase�� ����
    sMembase->mAllocPersPageCount = sLogicalRedoMembase.mAllocPersPageCount;
    sMembase->mCurrentExpandChunkCnt = sLogicalRedoMembase.mCurrentExpandChunkCnt;
    
    
    IDE_DASSERT( sMembase->mExpandChunkPageCnt == 
                 sLogicalRedoMembase.mExpandChunkPageCnt );
    
    sMembase->mFreePageListCount = sLogicalRedoMembase.mFreePageListCount;

    sMembase->mDBFileCount[0] = sLogicalRedoMembase.mDBFileCount[0];
    
    sMembase->mDBFileCount[1] = sLogicalRedoMembase.mDBFileCount[1];
    
    IDE_DASSERT( sMembase->mFreePageListCount == sFreePageListCount );

    for ( i = 0 ;
          i < sFreePageListCount ;
          i ++ )
    {
        // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� ���� ������,
        // Redo�߿��� Free Page List�� Page���� üũ���� �ʴ´�.
        IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                                & sLogicalRedoMembase.mFreePageLists[i] )
                     == ID_TRUE );
        
        sMembase->mFreePageLists[i].mFirstFreePageID = 
            sLogicalRedoMembase.mFreePageLists[i].mFirstFreePageID ;
        
        sMembase->mFreePageLists[i].mFreePageCount =
            sLogicalRedoMembase.mFreePageLists[i].mFreePageCount ;
    }
    
    // 0 �� Page�� Dirty �������� �߰�
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Update type : SMR_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK
 *
 * Free Page List Info Page �� Free Page�� Next Free Page����� �Ϳ� ����
 * REDO / UNDO �ǽ�
 *
 * before image: Next Free PageID 
 * after  image: Next Free PageID 
 */
IDE_RC smmUpdate::redo_undo_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK(
                                                          smTID          /*a_tid*/,
                                                          scSpaceID      a_spaceid,
                                                          scPageID       a_pid,
                                                          scOffset       a_offset,
                                                          vULong         /*a_data*/,
                                                          SChar        * a_pImage,
                                                          SInt           /*a_nSize*/,
                                                          UInt        /*aFlag*/ )
{

    smmFLISlot * sSlotAddr ;

    IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                       SM_MAKE_OID( a_pid, a_offset ),
                                       (void**)&sSlotAddr )
                == IDE_SUCCESS );

    idlOS::memcpy( & sSlotAddr->mNextFreePageID,
                   a_pImage,
                   ID_SIZEOF(scPageID) );
    

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/*
 * Update type : SCT_UPDATE_MRDB_CREATE_TBS
 *
 * tablespace ������ ���� redo ����
 *
 * Commit���� ������ Log Anchor�� Tablespace�� Flush�Ͽ��� ������
 * Tablespace�� Attribute�� Checkpoint Path�� ���� �����鿡 ����
 * ������ Redo�� ������ �ʿ䰡 ����.
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::redo_SCT_UPDATE_DRDB_CREATE_TBS �� �ּ� ����
 *    ( ���⿡ �ּ��� �˰��� ��,��,��,...�� ���� ��ȣ�� ����Ǿ� ����)
 */
IDE_RC smmUpdate::redo_SMM_UPDATE_MRDB_CREATE_TBS( idvSQL     * /*aStatistics*/,
                                                   void       * aTrans,
                                                   smLSN        /* aCurLSN */,
                                                   scSpaceID    aSpaceID,
                                                   UInt         /* aFileID */,
                                                   UInt         aValueSize,
                                                   SChar      * aValuePtr,
                                                   idBool       /* aIsRestart */  )
{
    smmTBSNode            * sSpaceNode;
    sctPendingOp          * sPendingOp;
    smiTableSpaceAttr       sTableSpaceAttr;
    smiChkptPathAttrList  * sChkptPathList  = NULL;
    smiChkptPathAttrList  * sChkptPath      = NULL;
    UInt                    sChkptPathCount = 0;
    UInt                    sReadOffset     = 0;
    UInt                    i               = 0;
    UInt                    sMemAllocState  = 0;
    scSpaceID               sNewSpaceID     = 0;

    IDE_DASSERT( aTrans != NULL );

    // PROJ-1923���� After Image�� ����Ѵ�. ���� ������ After Image ��.
    IDE_ERROR_MSG( aValueSize >= (ID_SIZEOF(smiTableSpaceAttr) + ID_SIZEOF(UInt)),
                   "aValueSize : %"ID_UINT32_FMT,
                   aValueSize );

    // Loganchor�κ��� �ʱ�ȭ�� TBS List�� �˻��Ѵ�. 
    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    if ( sSpaceNode != NULL )
    {
        if ( SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) ) 
        {
            /* �˰��� (��)�� �ش��ϴ� CREATINIG ������ ��쿡�� �����Ƿ� 
             * ���¸� ONLINE���� ������ �� �ְ� Commit Pending ������ ����Ѵ�. */
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                        aTrans,
                                                        aSpaceID,
                                                        ID_TRUE, /* commit�ÿ� ���� */
                                                        SCT_POP_CREATE_TBS,
                                                        & sPendingOp )
                      != IDE_SUCCESS );

            sPendingOp->mPendingOpFunc = smmTBSCreate::createTableSpacePending;
            
            /* �˰��� (��)�� �ش��ϴ� ���� Rollback Pending �����̱� ������
             * undo_SCT_UPDATE_DRDB_CREATE_TBS()���� POP_DROP_TBS ���� ����Ѵ�. */
        }
        else
        {
            /* �˰��� (��) �� �ش��ϹǷ� ��������� �ʴ´�. */
        }
    }
    else
    {
        sNewSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

        /* �α׿��� �о�� spaceID�� ��Ÿ���� �о�� newSpaceID��
         * ������ (��)�� �ش��ϴ� ��� �� redo�� �����ؾ� �Ѵ�. */
        if ( aSpaceID == sNewSpaceID )
        {
            /* PROJ-1923 ALTIBASE HDB Disaster Recovery.
             * �� ����� �����ϱ� ���� �˰��� (��) �� �ش��ϴ� ��쿡��
             * ������� �����Ѵ�. */

            // �α׸� �о� ��
            sReadOffset = 0;

            idlOS::memcpy( (void *)&sTableSpaceAttr,
                           aValuePtr + sReadOffset,
                           ID_SIZEOF(smiTableSpaceAttr) );
            sReadOffset += ID_SIZEOF(smiTableSpaceAttr);

            idlOS::memcpy( (void *)&sChkptPathCount,
                           aValuePtr + sReadOffset,
                           ID_SIZEOF(UInt) );
            sReadOffset += ID_SIZEOF(UInt);

            // Check point path �� ��ϵǾ� �ִٸ�.
            if ( sChkptPathCount != 0 )
            {
                // Check point path �� �޸� �Ҵ�
                /* smmUpdate_redo_SMM_UPDATE_MRDB_CREATE_TBS_calloc_ChkptPathList.tc */
                IDU_FIT_POINT("smmUpdate::redo_SMM_UPDATE_MRDB_CREATE_TBS::calloc::ChkptPathList");
                IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMM,
                                             sChkptPathCount,
                                             ID_SIZEOF(smiChkptPathAttrList),
                                             (void **)&sChkptPathList )
                          != IDE_SUCCESS );
                sMemAllocState = 1;

                // sChkptPathCount ��ŭ smiChkptPathAttrList �� memcpy
                idlOS::memcpy( (void *)sChkptPathList,
                               aValuePtr + sReadOffset,
                               (size_t)ID_SIZEOF(smiChkptPathAttrList) * sChkptPathCount );
                sReadOffset += ID_SIZEOF(smiChkptPathAttrList) * sChkptPathCount;

                // ChkptPathAttrList�� next �� ����
                sChkptPath = sChkptPathList;
                for ( i = 0 ; i < sChkptPathCount ; i++ )
                {
                    if ( i == (sChkptPathCount -1) )
                    {
                        // �������� Next �� NULL�� �����ؾ� �Ѵ�.
                        sChkptPath->mNext = NULL;
                    }
                    else
                    {
                        // next �ּҰ��� ���� �����Ѵ�.
                        sChkptPath->mNext = sChkptPath + 1;
                        sChkptPath = sChkptPath->mNext;
                    }
                } // end of for
            }
            else
            {
                // check point path�� �Էµ��� ���� �α׶�� NULL�� ó��
                sChkptPathList = NULL;
            }

            // create tbs 4 redo ����
            IDE_TEST( smmTBSCreate::createTBS4Redo( aTrans,
                                                    &sTableSpaceAttr,
                                                    sChkptPathList )
                      != IDE_SUCCESS );

            if ( sMemAllocState != 0 )
            {
                sMemAllocState = 0;
                IDE_TEST( iduMemMgr::free( sChkptPathList ) != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            // do nothing
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sMemAllocState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sChkptPathList ) == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/*
 * Update type : SCT_UPDATE_MRDB_CREATE_TBS
 *
 * Memory tablespace ������ ���� undo ����.
 *
 *  - sddUpdate::redo_SCT_UPDATE_DRDB_CREATE_TBS �� �ּ� ����
 *    ( ���⿡ �ּ��� �˰��� ��,��,��,...�� ���� ��ȣ�� ����Ǿ� ����)
 */
IDE_RC smmUpdate::undo_SMM_UPDATE_MRDB_CREATE_TBS(
                                          idvSQL*            /*aStatistics*/,
                                          void*              /* aTrans*/,
                                          smLSN              /* aCurLSN */,
                                          scSpaceID          aSpaceID,
                                          UInt               /* aFileID */,
                                          UInt               /* aValueSize */,
                                          SChar*             /* aValuePtr */,
                                          idBool             /* aIsRestart */  )
{
    smmTBSNode         * sSpaceNode;

    // TBS List�� �˻��Ѵ�. 
    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );

    // RUNTIME�ÿ��� sSpaceNode ��ü�� ���ؼ� (X) ����� �����ֱ� ������ 
    // sctTableSpaceMgr::lock�� ȹ���� �ʿ䰡 ����. 
    if ( sSpaceNode != NULL ) // �̹� Undo�Ϸ�Ǿ� Dropó���� ��� SKIP
    {
        // Drop �� Tablespace�� ��� sSpaceNode�� NULL�� ���´�
        // ���� Drop�� Tablespace�� �� �� ����.
        IDE_DASSERT( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED) 
                     != SMI_TBS_DROPPED );

        // Create Tablespace NTA�� ������ ���� Abort�� ��쿡��
        // �� �Լ��� ���� Create Tablespace�� Undo�ȴ�.
        IDE_ERROR(  SMI_TBS_IS_CREATING(sSpaceNode->mHeader.mState) )

        // Lock����(STATE�ܰ�) ������ ��� ��ü �ı�, �޸� �ݳ�
        IDE_TEST( smmTBSMultiPhase::finiToStatePhase( sSpaceNode )
                  != IDE_SUCCESS );

        // Tablespace�� ���¸� DROPPED�� ���º���
        //
        // To Fix BUG-17323 �������� �ʴ� Checkpoint Path�����Ͽ�
        //                  Tablespace���� ������ Log Anchor��
        //                  Log File Group Count�� 0�� �Ǿ����
        //
        // => Tablespace Node�� ���� Log Anchor�� ��ϵ��� ���� ���
        //    Node���� ���¸� �������ش�.
        //    Log Anchor�� �ѹ��̶� ����� �Ǹ�
        //    sSpaceNode->mAnchorOffset �� 0���� ū ���� ������ �ȴ�.
        if ( sSpaceNode->mAnchorOffset > 0 )  
        {
            // Log Anchor�� ��ϵ� ���� �ִ� ��� 
            // - Restart Recovery�� �ƴѰ�쿡�� Log Anchor�� Flush
            IDE_TEST( smmTBSDrop::flushTBSStatusDropped( & sSpaceNode->mHeader )
                      != IDE_SUCCESS );
        }
        else
        {
            // Log Anchor�� �ѹ��� ��ϵ��� �ʴ� ���
            sSpaceNode->mHeader.mState = SMI_TBS_DROPPED;
        }
    }
    else
    {
        // RESTART �˰��� (��) �ش�
        // RUNTIME �˰��� (��) �ش�
        // nothing to do ...
    }

    // RUNTIME�ÿ� ������ �߻��ߴٸ� Rollback Pending�� ��ϵǾ��� ���̰�
    // Rollback Pending�� Loganchor�� �����Ѵ�. 
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;    
}

/*
 * Update type : SCT_UPDATE_MRDB_DROP_TBS
 *
 * Memory tablespace Drop�� ���� redo ����.
 *
 *   [�α� ����]
 *   After Image ��� ----------------------------------------
 *      smiTouchMode  aTouchMode
 */
IDE_RC smmUpdate::redo_SMM_UPDATE_MRDB_DROP_TBS(
                                      idvSQL*            /*aStatistics*/,
                                      void*              aTrans,
                                      smLSN              /* aCurLSN */,
                                      scSpaceID          aSpaceID,
                                      UInt               /* aFileID */,
                                      UInt               aValueSize,
                                      SChar*             aValuePtr,
                                      idBool             /* aIsRestart */  )
{
    smmTBSNode        * sSpaceNode;
    sctPendingOp      * sPendingOp;
    smiTouchMode        sTouchMode;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aValueSize == ID_SIZEOF(smiTouchMode) );

    ACP_UNUSED( aValueSize );

    idlOS::memcpy( &sTouchMode, aValuePtr, ID_SIZEOF(smiTouchMode) );
    
    // �ش� Tablespace�� Node�� ã�´�.
    // Drop�� ��쿡�� ��带 �����Ѵ�.
    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceID );

    if ( sSpaceNode == NULL )
    {
        // �̹� DROP�� ���
        // Do Nothing
    }
    else
    {
        if ( SMI_TBS_IS_DROPPED(sSpaceNode->mHeader.mState) )
        {
            // �̹� DROP�� ���
            // Do Nothing
        }
        else
        {
            // Drop Tablespace Pending �����ϴٰ� ����� ��쵵 Pending����
            // - �� ��� SMI_TBS_DROP_PENDING�� ���¸� ������.
            // - �� ��쿡�� Drop Tablespace Pending�� �ٽ� �����Ѵ�.
            
            /* Transaction�� Commit�ÿ� Tablespace�� Drop �ϵ���
               Pending�Լ��� �޾��ش�. */
            
            // Pending�Լ� ���� ���� ���·� Tablespace�� STATE�� �����Ѵ�
            sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;
            sSpaceNode->mHeader.mState &= ~SMI_TBS_DROP_PENDING;
            
            // �˰��� (��), (��)�� �ش��ϴ� ��� Commit Pending ���� ���
            IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                      aTrans,
                                                      sSpaceNode->mHeader.mID,
                                                      ID_TRUE, /* commit�ÿ� ���� */
                                                      SCT_POP_DROP_TBS,
                                                      &sPendingOp )
                      != IDE_SUCCESS );
            
            // Pending �Լ� ����.
            // ó�� : Tablespace�� ���õ� ��� �޸𸮿� ���ҽ��� �ݳ��Ѵ�.
            sPendingOp->mPendingOpFunc = smmTBSDrop::dropTableSpacePending;
            sPendingOp->mTouchMode     = sTouchMode;
        }
        
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * Update type : SCT_UPDATE_MRDB_DROP_TBS
 *
 * tablespace ���ſ� ���� undo ����
 *
 * Disk Tablespace�� �����ϰ� ó���Ѵ�.
 *  - sddUpdate::undo_SCT_UPDATE_DRDB_DROP_TBS �� �ּ� ����
 * 
 * before image : tablespace attribute
 */
IDE_RC smmUpdate::undo_SMM_UPDATE_MRDB_DROP_TBS(
                                  idvSQL*            /*aStatistics*/,
                                  void*              /* aTrans */,
                                  smLSN              /* aCurLSN */,
                                  scSpaceID          aSpaceID,
                                  UInt               /* aFileID */,
                                  UInt               aValueSize,
                                  SChar*             /* aValuePtr */,
                                  idBool             /* aIsRestart */  )
{
    smmTBSNode       *  sSpaceNode;

    IDE_DASSERT( aValueSize == 0 );
    ACP_UNUSED( aValueSize );

    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    // RUNTIME�ÿ��� sSpaceNode ��ü�� ���ؼ� (X) ����� �����ֱ� ������ 
    // sctTableSpaceMgr::lock�� ȹ���� �ʿ䰡 ����. 
    if ( sSpaceNode != NULL )
    {
        if ( SMI_TBS_IS_DROPPING(sSpaceNode->mHeader.mState) )
        {
            // �˰��� RESTART (��), RUNTIME (��) �� �ش��ϴ� ����̴�. 
            // DROPPING�� ����, ONLINE ���·� �����Ѵ�. 
            sSpaceNode->mHeader.mState &= ~SMI_TBS_DROPPING;
        }
        
        IDE_ERROR( (sSpaceNode->mHeader.mState & SMI_TBS_CREATING) 
                    != SMI_TBS_CREATING );
    }
    else
    {
        // TBS List���� �˻��� ���� ������ �̹� Drop�� Tablespace�̴�.
        // �ƹ��͵� ���� �ʴ´�.
        // nothing to do...
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� Log Image�� �м��Ѵ�.

    [IN]  aValueSize     - Log Image �� ũ�� 
    [IN]  aValuePtr      - Log Image
    [OUT] aAutoExtMode   - Auto extent mode
    [OUT] aNextPageCount - Next page count
    [OUT] aMaxPageCount  - Max page count
 */
IDE_RC smmUpdate::getAlterAutoExtendImage( UInt       aValueSize,
                                           SChar    * aValuePtr,
                                           idBool   * aAutoExtMode,
                                           scPageID * aNextPageCount,
                                           scPageID * aMaxPageCount )
{
    IDE_DASSERT( aValuePtr != NULL );
    IDE_DASSERT( aValueSize == (UInt)( ID_SIZEOF(*aAutoExtMode) +
                                       ID_SIZEOF(*aNextPageCount) +
                                       ID_SIZEOF(*aMaxPageCount) ) );
    IDE_DASSERT( aAutoExtMode   != NULL );
    IDE_DASSERT( aNextPageCount != NULL );
    IDE_DASSERT( aMaxPageCount  != NULL );

    ACP_UNUSED( aValueSize );
    
    idlOS::memcpy( aAutoExtMode, aValuePtr, ID_SIZEOF(*aAutoExtMode) );
    aValuePtr += ID_SIZEOF(*aAutoExtMode);

    idlOS::memcpy( aNextPageCount, aValuePtr, ID_SIZEOF(*aNextPageCount) );
    aValuePtr += ID_SIZEOF(*aNextPageCount);

    idlOS::memcpy( aMaxPageCount, aValuePtr, ID_SIZEOF(*aMaxPageCount) );
    aValuePtr += ID_SIZEOF(*aMaxPageCount);

    return IDE_SUCCESS;
}



/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� REDO ����

    [ �α� ���� ]
    After Image   --------------------------------------------
      idBool              aAIsAutoExtend
      scPageID            aANextPageCount
      scPageID            aAMaxPageCount 
    
    [ ALTER_TBS_AUTO_EXTEND �� REDO ó�� ]
      (r-010) TBSNode.AutoExtend := AfterImage.AutoExtend
      (r-020) TBSNode.NextSize   := AfterImage.NextSize
      (r-030) TBSNode.MaxSize    := AfterImage.MaxSize
*/
IDE_RC smmUpdate::redo_SCT_UPDATE_MRDB_ALTER_AUTOEXTEND(
                                       idvSQL             * /*aStatistics*/,    
                                       void               * aTrans,
                                       smLSN                /* aCurLSN */,
                                       scSpaceID            aSpaceID,
                                       UInt                 /*aFileID*/,
                                       UInt                 aValueSize,
                                       SChar              * aValuePtr,
                                       idBool               /* aIsRestart */ )
{

    smmTBSNode       * sTBSNode;
    idBool             sAutoExtMode;
    scPageID           sNextPageCount;
    scPageID           sMaxPageCount;

    IDE_DASSERT( aTrans != NULL );

    ACP_UNUSED( aTrans );
    
    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAutoExtendImage ���� �ǽ�.
    IDE_TEST( getAlterAutoExtendImage( aValueSize,
                                       aValuePtr,
                                       & sAutoExtMode,
                                       & sNextPageCount,
                                       & sMaxPageCount ) != IDE_SUCCESS );

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sTBSNode != NULL )
    {
        sTBSNode->mTBSAttr.mMemAttr.mIsAutoExtend  = sAutoExtMode;
        sTBSNode->mTBSAttr.mMemAttr.mNextPageCount = sNextPageCount;
        sTBSNode->mTBSAttr.mMemAttr.mMaxPageCount  = sMaxPageCount;
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
    ALTER TABLESPACE TBS1 AUTOEXTEND .... �� ���� undo ����

    [ �α� ���� ]
    Before Image  --------------------------------------------
      idBool              aBIsAutoExtend
      scPageID            aBNextPageCount
      scPageID            aBMaxPageCount
      
    [ ALTER_TBS_AUTO_EXTEND �� UNDO ó�� ]
      (u-010) �α�ǽ� -> CLR ( ALTER_TBS_AUTO_EXTEND )
      (u-020) TBSNode.AutoExtend := BeforeImage.AutoExtend
      (u-030) TBSNode.NextSize   := BeforeImage.NextSize
      (u-040) TBSNode.MaxSize    := BeforeImage.MaxSize
*/
IDE_RC smmUpdate::undo_SCT_UPDATE_MRDB_ALTER_AUTOEXTEND(
                                       idvSQL*            /*aStatistics*/,
                                       void*                aTrans,
                                       smLSN                /* aCurLSN */,
                                       scSpaceID            aSpaceID,
                                       UInt                 /*aFileID*/,
                                       UInt                 aValueSize,
                                       SChar*               aValuePtr,
                                       idBool               aIsRestart )
{
    smmTBSNode       * sTBSNode;
    idBool             sAutoExtMode;
    scPageID           sNextPageCount;
    scPageID           sMaxPageCount;
    

    // BUGBUG-1548 �� Restart���߿��� aTrans == NULL�� �� �ִ���?
    IDE_ERROR( (aTrans != NULL) || (aIsRestart == ID_TRUE) );


    // aValueSize, aValuePtr �� ���� ���� DASSERTION��
    // getAlterAutoExtendImage ���� �ǽ�.
    IDE_TEST( getAlterAutoExtendImage( aValueSize,
                                       aValuePtr,
                                       & sAutoExtMode,
                                       & sNextPageCount,
                                       & sMaxPageCount ) != IDE_SUCCESS );

    sTBSNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sTBSNode != NULL )
    {
        sTBSNode->mTBSAttr.mMemAttr.mIsAutoExtend  = sAutoExtMode;
        sTBSNode->mTBSAttr.mMemAttr.mNextPageCount = sNextPageCount;
        sTBSNode->mTBSAttr.mMemAttr.mMaxPageCount  = sMaxPageCount;
        
        if ( aIsRestart == ID_FALSE )
        {
            // Log Anchor�� flush.
            // TableSpace �� ���� DDL�� undo���� Table Space�� Lock Item�� ���� �ִ�.
            IDE_TEST( smLayerCallback::updateTBSNodeAndFlush( (sctTableSpaceNode*) sTBSNode ) 
                      != IDE_SUCCESS );
        }
        else
        {
            // RESTART�ÿ��� Loganchor�� flush���� �ʴ´�.
        }
    }
    else
    {
        // �̹� Drop�� Tablespace�� ��� 
        // nothing to do ...
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
    Create Tablespace���� Checkpoint Image File�� ������ ���� Redo
    type : SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE
 */
IDE_RC smmUpdate::redo_SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE(
                                      idvSQL*            /*aStatistics*/,
                                      void*              /* aTrans */,
                                      smLSN              /* aCurLSN */,
                                      scSpaceID          /* aSpaceID */,
                                      UInt               /* aFileID */,
                                      UInt               /* aValueSize */,
                                      SChar*             /* aValuePtr */,
                                      idBool             /* aIsRestart */  )
{
    // Do Nothing ===============================================
    // Create Tablespace���� ������ Checkpoint Image File��
    // Create Tablespace�� Undo�� �� �����ֱ⸸ �ϰ�,
    // Redo���� �ƹ��͵� �� �ʿ䰡 ����.
    return IDE_SUCCESS;
}


/*
    Create Tablespace���� Checkpoint Image File�� ������ ���� Undo
    type : SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE

    �ش� File�� �����Ѵ�.
    
    [�α� ����]
    Before Image ��� ----------------------------------------
      UInt - Ping Pong Number
      UInt - DB File Number
 */
IDE_RC smmUpdate::undo_SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE(
                                          idvSQL*            /*aStatistics*/,
                                          void*              aTrans,
                                          smLSN              /* aCurLSN */,
                                          scSpaceID          aSpaceID,
                                          UInt               /* aFileID */,
                                          UInt               aValueSize,
                                          SChar*             aValuePtr,
                                          idBool             /* aIsRestart */  )
{
    smmTBSNode        * sSpaceNode;
    UInt                sPingPongNo;
    UInt                sDBFileNo;
    
    smmDatabaseFile   * sDBFilePtr;
    
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aValueSize == ID_SIZEOF(UInt) * 2 );

    ACP_UNUSED( aTrans );
    ACP_UNUSED( aValueSize );
    
    idlOS::memcpy( &sPingPongNo, aValuePtr, ID_SIZEOF(UInt) );
    aValuePtr += ID_SIZEOF(UInt);

    idlOS::memcpy( &sDBFileNo, aValuePtr, ID_SIZEOF(UInt) );
    aValuePtr += ID_SIZEOF(UInt);
    
    
    // TBS List���� �˻��Ѵ�. 
    sSpaceNode = (smmTBSNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aSpaceID );
    
    if ( sSpaceNode != NULL )
    {
        // DROPPED������ TBS�� findSpaceNodeWithoutException���� �ǳʶڴ�
        IDE_DASSERT( (sSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
                     != SMI_TBS_DROPPED );

        // DISCARD, OFFLINE, ONLINE Tablespace��� MEDIA System����
        // �ʱ�ȭ�� �Ǿ� �����Ƿ�, DB File��ü�� ������ �����ϴ�.
        if ( smmManager::getDBFile( sSpaceNode,
                                    sPingPongNo,
                                    sDBFileNo,
                                    SMM_GETDBFILEOP_SEARCH_FILE,
                                    &sDBFilePtr ) == IDE_SUCCESS )
        {
            // ������ �����ϴ� ��� 
            IDE_TEST( sDBFilePtr->closeAndRemoveDbFile( aSpaceID,
                                                        ID_TRUE, /* Remove File */
                                                        sSpaceNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // To Fix BUG-18272   TC/Server/sm4/PRJ-1548/dynmem/../suites/
            //                    restart/rt_ctdt_aa.sql ���൵�� �׽��ϴ�.
            //
            // SMM_UPDATE_MRDB_CREATE_CIMAGE_FILE �α������ �ǰ�
            // ������ ���� �������� ���� ����̴� => SKIP
            IDE_ERROR( ideGetErrorCode() == smERR_ABORT_NoExistFile );
            
            IDE_CLEAR();
        }
    }
    else
    {
        // �̹� Dropó���� ����̴�. �ƹ��͵� ���Ѵ�.
    }
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* 
   PRJ-1548 User Memory Tablespace ���䵵�� 
   
   Update type:  SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK

   Media Recovery �ÿ� Expand Chunk�Ҵ翡 ���� logical redo �ǽ� 
   
   1. Membase�� �����ϴ� 0�� ����Ÿ������ ������ ���� ��� 
      Membase�� �������־�� �Ѵ�.
      (Logical Membase�� ����Ͽ��� �ϰ� Membase�� Update�ؾ��� )
   2. 0������ ����Ÿ������ ������ �ִ� ���  
      Membase�� ������ �ʿ���� Chunk �� �������ָ� �ȴ�. 
     (Logical Membase ����Ͽ��� ������ Membase�� update�ϸ� �ȵ�  )

   smmManager::allocNewExpandChunk�� Logical Redo�� ����
   Membase �Ϻ� ����� ����� Before �̹����� �̿��Ѵ�.           
   
   after  image: aBeforeMembase->m_alloc_pers_page_count
                 aBeforeMembase->mCurrentExpandChunkCnt
                 aBeforeMembase->mExpandChunkPageCnt
                 aBeforeMembase->m_nDBFileCount[0]
                 aBeforeMembase->m_nDBFileCount[1]
                 aBeforeMembase->mFreePageListCount
                 [
                      aBeforeMembase-> mFreePageLists[i].mFirstFreePageID
                      aBeforeMembase-> mFreePageLists[i].mFreePageCount
                 ] ( aBeforeMembase->mFreePageListCount ��ŭ )
                 aExpandPageListID
                 aAfterChunkFirstPID                          
                 aAfterChunkLastPID                        
*/                                                                   
IDE_RC smmUpdate::redo4MR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK( scSpaceID     a_spaceid,
                                                          scPageID      a_pid,
                                                          scOffset      a_offset,
                                                          SChar        *a_pImage )
{

    smmMemBase * sMembase;
    smmMemBase * sOrgMembase;
    UInt         sOrgDBFileCount;
    
    // Log���� �о�� �����θ� Membase�� �����Ͽ�
    // Logical Redo�� �����Ѵ�.
    // �׸��� Logical Redo�� �����ϰ� �� ����� ���� Membase���� �ʵ尪���� 
    // ���� Membase�� �ʵ尪�� �����Ѵ�.
    //
    // �̷��� �ϴ� ������, Restart Recovery���߿��� Membase�� �������� ����
    // �ֱ� ������, Membase���� �ʵ��� Log���� �о�� �ʵ� ���� �ٸ�
    // �ʵ���� ��� 0���� �����س��� Logical Redo�� �����Ͽ�,
    // ����ġ ���� Membase �ʵ� ������ �̿��� Detect�ϱ� �����̴�.
    smmMemBase   sLogicalRedoMembase;
    
    UInt         sFreePageListCount;
    scPageID     sNewChunkFirstPID ;
    scPageID     sNewChunkLastPID  ;
    scPageID     sFirstFreePageID ;
    vULong       sFreePageCount ;
    UInt         sExpandPageListID;    
    UInt         sDbFileNo;
    UInt         i;
    smmTBSNode  *sTBSNode;
    idBool       sIsExistTBS;
    idBool       sRecoverMembase;
    idBool       sRecoverNewChunk;
    idBool       sDummy;

    IDE_DASSERT( a_pid == (scPageID)0 );
    IDE_DASSERT( a_pImage != NULL );

    // [ 1 ] 0�� �������� �����ϴ� ����Ÿ������ ������ �ִ��� 
    // �˻��Ͽ� Membase�� ������ �������� �����Ѵ�. 
    IDE_TEST( smmTBSMediaRecovery::findMatchFailureDBF( a_spaceid,
                                                        a_pid,
                                                        &sIsExistTBS,
                                                        &sRecoverMembase ) 
              != IDE_SUCCESS );

    // filter���� �������� �ʴ� ���̺����̽��� �α״� �ɷ��´�
    IDE_ERROR( sIsExistTBS == ID_TRUE );
    
    // Logical Membase �ʱ�ȭ 
    idlOS::memset( & sLogicalRedoMembase,
                   0,
                   ID_SIZEOF( smmMemBase ) );

    // Logical Redo�� ������ �Ǿ ���������� �ϱ� ����
    // Logical Redo���� Membase�� ������ �α� ��� ��÷� �������´�.

    // Log�м� ���� ++++++++++++++++++++++++++++++++++++++++++++++
    idlOS::memcpy( & sLogicalRedoMembase.mAllocPersPageCount,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mAllocPersPageCount ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mAllocPersPageCount );

    idlOS::memcpy( & sLogicalRedoMembase.mCurrentExpandChunkCnt,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mCurrentExpandChunkCnt ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mCurrentExpandChunkCnt );

    idlOS::memcpy( & sLogicalRedoMembase.mExpandChunkPageCnt,
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mExpandChunkPageCnt ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mExpandChunkPageCnt );

    idlOS::memcpy( & sLogicalRedoMembase.mDBFileCount[0],
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[0] ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[0] );

    idlOS::memcpy( & sLogicalRedoMembase.mDBFileCount[1],
                   a_pImage,
                   ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[1] ) );
    a_pImage += ID_SIZEOF( sLogicalRedoMembase.mDBFileCount[1] );

    idlOS::memcpy( & sFreePageListCount,
                   a_pImage,
                   ID_SIZEOF(UInt) );
    a_pImage += ID_SIZEOF(UInt);

    sLogicalRedoMembase.mFreePageListCount = sFreePageListCount;

    for ( i = 0 ;
          i < (UInt) sFreePageListCount ;
          i ++ )
    {
        idlOS::memcpy( &sFirstFreePageID,
                       a_pImage,
                       ID_SIZEOF(scPageID) );
        a_pImage += ID_SIZEOF( scPageID );

        idlOS::memcpy( &sFreePageCount,
                       a_pImage,
                       ID_SIZEOF(vULong) );
        a_pImage += ID_SIZEOF( vULong );


        sLogicalRedoMembase.mFreePageLists[i].mFirstFreePageID = sFirstFreePageID;
        sLogicalRedoMembase.mFreePageLists[i].mFreePageCount = sFreePageCount;

        // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� ���� ������,
        // Redo�߿��� Free Page List�� Page���� üũ���� �ʴ´�.
        IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                                & sLogicalRedoMembase.mFreePageLists[i] )
                     == ID_TRUE );
    }
    
    idlOS::memcpy( &sExpandPageListID,      
                   a_pImage, 
                   ID_SIZEOF(UInt) );
    a_pImage += ID_SIZEOF(UInt);    

    idlOS::memcpy( &sNewChunkFirstPID, 
                   a_pImage, 
                   ID_SIZEOF(scPageID) );
    a_pImage += ID_SIZEOF(scPageID);

    idlOS::memcpy( &sNewChunkLastPID, 
                   a_pImage, 
                   ID_SIZEOF(scPageID) );
    a_pImage += ID_SIZEOF(scPageID);
    // Log�м� �� ------------------------------------------------

    // [ 2 ] Chunk Ȯ������� �������� �����ϴ� ����Ÿ������ 
    // ������ �ִ��� �˻��Ͽ� ������ �������� �����Ѵ�. 
    // Chunk�� ����Ÿ���ϰ��� ���ļ� �������� �ʴ´�. 
    IDE_TEST( smmTBSMediaRecovery::findMatchFailureDBF( a_spaceid,
                                                        sNewChunkFirstPID,
                                                        &sIsExistTBS,
                                                        &sRecoverNewChunk ) 
              != IDE_SUCCESS );

    if ( (sRecoverMembase == ID_TRUE) || 
         (sRecoverNewChunk == ID_TRUE) )
    {
        IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( a_spaceid,
                                                            (void**)&sTBSNode )
                  != IDE_SUCCESS );

        // BUG-27456 Klocwork SM (4)
        IDE_ERROR( sTBSNode->mMemBase != NULL );

        sLogicalRedoMembase.mDBFilePageCount = sTBSNode->mMemBase->mDBFilePageCount;

        // Membase�� �����ϰų� 0������ ����Ÿ������ �����ϴ��� 
        // sLogical
        // Logical Redo ������ ���� Membase�� �ٲ�ģ��.
        sOrgMembase        = sTBSNode->mMemBase;
        sTBSNode->mMemBase = &sLogicalRedoMembase;

        // Logical Redo ������ DB File�� 
        sOrgDBFileCount = sLogicalRedoMembase.mDBFileCount[0];

        // Logical Redo�� Expand Chunk�� �Ҵ��Ѵ�.
        IDE_TEST( smmManager::allocNewExpandChunk4MR( sTBSNode,
                                                      sNewChunkFirstPID,
                                                      sNewChunkLastPID,
                                                      sRecoverMembase,
                                                      sRecoverNewChunk ) 
                  != IDE_SUCCESS );

        // Chunk�Ҵ� ���� ���� ���ܳ� DB���Ͽ� ���� ó��
        //
        // sOrgDBFileCount 
        //    logical redo������ DB File��  => ���� ������ ù��° DB File ��ȣ
        // sLogicalRedoMembase.m_nDBFileCount[0]-1
        //    �� Membase�� DB File�� -1 => ���� ������ ������ DB File ��ȣ
        //
        // ( m_nDBFileCount[0], [1] ��� �׻� ���� ������ �����ǹǷ�,
        //   �ƹ� ���̳� ����ص� �ȴ�. )
        for ( sDbFileNo  = sOrgDBFileCount ; 
              sDbFileNo <= sLogicalRedoMembase.mDBFileCount[0]-1 ;
              sDbFileNo ++ )
        {
            // BUGBUG 
            // �̵����ÿ��� �ݵ�� ������ �����Ѵٴ� ������ �ִ�. 
            // �׷��� ������ ���� �Լ��� ȣ���� �ʿ���� ������ ã��
            // Open�ϴ� �Լ��� ��ü�ϸ� �ȴ�. 

            // DB File�� �����ϸ� open�ϰ� ������ �����Ѵ�.
            IDE_TEST( smmManager::openOrCreateDBFileinRecovery( sTBSNode,
                                                                sDbFileNo,
                                                                &sDummy )
                      != IDE_SUCCESS );

            // ��� DB File�� �����ϴ� ��Ż Page Count ���.
            // Performance View���̸� ��Ȯ�� ��ġ���� �ʾƵ� �ȴ�.
            IDE_TEST( smmManager::calculatePageCountInDisk( sTBSNode )
                      != IDE_SUCCESS );
        }

        // To Fix BUG-15112
        // Restart Recovery�߿� Page Memory�� NULL�� Page�� ���� Redo��
        // �ش� �������� �׶� �׶� �ʿ��� ������ �Ҵ��Ѵ�.
        // ���⿡�� ������ �޸𸮸� �̸� �Ҵ��ص� �ʿ䰡 ����

        // Membase�� ������� �������´�.
        sTBSNode->mMemBase = sOrgMembase;

        if ( sRecoverMembase == ID_TRUE )
        {
            // Membase�� �̵�� ������ �ʿ��� ��� 
            IDE_ASSERT( smmManager::getOIDPtr( a_spaceid,
                                               SM_MAKE_OID( a_pid, a_offset ),
                                               (void**)&sMembase )
                        == IDE_SUCCESS );

            IDE_ERROR_MSG( sMembase == sOrgMembase,
                           "sMembase    : %"ID_vULONG_FMT"\n"
                           "sOrgMembase : %"ID_vULONG_FMT"\n",
                           sMembase,
                           sOrgMembase );

            // Logical Redo�� ����� ������� Membase�� ������
            // ���� Membase�� ����
            sMembase->mAllocPersPageCount = sLogicalRedoMembase.mAllocPersPageCount;
            sMembase->mCurrentExpandChunkCnt = sLogicalRedoMembase.mCurrentExpandChunkCnt;


            IDE_DASSERT( sMembase->mExpandChunkPageCnt == 
                         sLogicalRedoMembase.mExpandChunkPageCnt );

            sMembase->mFreePageListCount = sLogicalRedoMembase.mFreePageListCount;

            sMembase->mDBFileCount[0] = sLogicalRedoMembase.mDBFileCount[0];

            sMembase->mDBFileCount[1] = sLogicalRedoMembase.mDBFileCount[1];

            IDE_DASSERT( sMembase->mFreePageListCount ==  sFreePageListCount );

            for ( i = 0;
                  i < sFreePageListCount ;
                  i ++ )
            {
                // Redo�ϴ� Page���� �ٸ� Page�� Validity�� ������ �� 
                // ���� ������, Redo�߿��� Free Page List�� Page���� 
                // üũ���� �ʴ´�.
                IDE_DASSERT( smmFPLManager::isValidFPL( a_spaceid,
                                                        & sLogicalRedoMembase.mFreePageLists[i] )
                             == ID_TRUE );

                sMembase->mFreePageLists[i].mFirstFreePageID = 
                    sLogicalRedoMembase.mFreePageLists[i].mFirstFreePageID ;

                sMembase->mFreePageLists[i].mFreePageCount =
                    sLogicalRedoMembase.mFreePageLists[i].mFreePageCount ;
            }

            // 0 �� Page�� Dirty �������� �߰�
            IDE_TEST( smmDirtyPageMgr::insDirtyPage(a_spaceid, a_pid) 
                      != IDE_SUCCESS );
        }
        else
        {
            // Membase ������ �ʿ���� ���
        }
    }
    else
    {
        // �̵�� ������ �ʿ���� Chunk �� ��� 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

