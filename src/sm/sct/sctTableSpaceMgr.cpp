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
 * $Id: sctTableSpaceMgr.cpp 17358 2006-07-31 03:12:47Z bskim $
 *
 * Description :
 *
 * ���̺����̽� ������ ( TableSpace Manager : sctTableSpaceMgr )
 *
 **********************************************************************/

#include <smDef.h>
#include <sctDef.h>
#include <smErrorCode.h>
#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <smu.h>
#include <sdd.h>
#include <smm.h>
#include <svm.h>
#include <sctReq.h>
#include <sctTableSpaceMgr.h>

iduMutex            sctTableSpaceMgr::mMtxCrtTBS;
iduMutex            sctTableSpaceMgr::mGlobalPageCountCheckMutex;
sctTableSpaceNode * sctTableSpaceMgr::mSpaceNodeArray[SC_MAX_SPACE_ARRAY_SIZE];
scSpaceID           sctTableSpaceMgr::mNewTableSpaceID;


// PRJ-1149 BACKUP & RECOVERY
// �̵����� �ʿ��� RedoLSN
smLSN          sctTableSpaceMgr::mDiskRedoLSN;
 // �̵����� �ʿ��� RedoLSN
smLSN          sctTableSpaceMgr::mMemRedoLSN;
extern smiGlobalCallBackList gSmiGlobalCallBackList;
/*
 * BUG-17285,17123
 * [PRJ-1548] offline�� TableSpace�� ���ؼ� ����Ÿ������
 *            �����ϴٰ� Error �߻��Ͽ� diff
 *
 * ���̺����̽��� Lock Validation Option ����
 */
sctTBSLockValidOpt sctTableSpaceMgr::mTBSLockValidOpt[ SMI_TBSLV_OPER_MAXMAX ] =
{
        SCT_VAL_INVALID,
        SCT_VAL_DDL_DML,
        SCT_VAL_DROP_TBS

};


/***********************************************************************
 * Description : ���̺����̽� ������ �ʱ�ȭ
 **********************************************************************/
IDE_RC sctTableSpaceMgr::initialize( )
{
    IDE_TEST( mGlobalPageCountCheckMutex.initialize(
                                  (SChar*)"SCT_GLOBAL_PAGE_COUNT_CHECK_MUTEX",
                                  IDU_MUTEX_KIND_POSIX,
                                  IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( mMtxCrtTBS.initialize( (SChar*)"CREATE_TABLESPACE_MUTEX",
                                     IDU_MUTEX_KIND_POSIX,
                                     IDV_WAIT_INDEX_LATCH_FREE_DRDB_TBS_CREATION )
              != IDE_SUCCESS );

    idlOS::memset( mSpaceNodeArray,
                   0x00,
                   ID_SIZEOF( mSpaceNodeArray ) );

    mNewTableSpaceID      = 0;

    // PRJ-1149 BACKUP & MEDIA RECOVERY
    SM_LSN_INIT( mDiskRedoLSN );

    SM_LSN_INIT( mMemRedoLSN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 * initialize���� �����ߴ� ��� �ڿ��� �ı��Ѵ�.
 **********************************************************************/
IDE_RC sctTableSpaceMgr::destroy()
{
    UInt               i;
    sctTableSpaceNode *sSpaceNode;

    /* ------------------------------------------------
     * tablespace �迭�� ��ȸ�ϸ鼭 ��� tablespace ��带
     * destroy�Ѵ�. (SMI_ALL_NOTOUCH ���)
     * ----------------------------------------------*/
    for( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        switch( getTBSLocation( sSpaceNode ) )
        {
            case SMI_TBS_MEMORY:
            case SMI_TBS_VOLATILE:
                {
                    // do nothing
                    // Memory, Volatile Tablespace�� ���
                    // smm/svmTBSStartupShutdown::destroyAllTBSNode����
                    // ��� TBS�� �ڿ��� �ı��ϰ�
                    // Tablespace Node�޸𸮵� �����Ѵ�.
                    //
                    // sctTableSpaceMgr�� sctTableSpaceNode�� ������ ��,
                    // �� ���� Resource�� smm/svmManager�� �Ҵ��Ͽ����Ƿ�,
                    // smm/svmManager�� �����ϴ� ���� ��Ģ�� �´�.

                    IDE_ASSERT(0);
                }
            case SMI_TBS_DISK:
                {
                    IDE_TEST( sddTableSpace::removeAllDataFiles(
                                  NULL, /* idvSQL* */
                                  NULL,
                                  (sddTableSpaceNode*)sSpaceNode,
                                  SMI_ALL_NOTOUCH,
                                  ID_FALSE) /* Don't ghost mark */
                              != IDE_SUCCESS );

                    //bug-15653
                    IDE_TEST( sctTableSpaceMgr::destroyTBSNode( sSpaceNode )
                              != IDE_SUCCESS );

                    /* BUG-18236 DB�����ÿ� ���� ������ �����մϴ�.
                     *
                     * mSpaceNodeArray�� Memory Tablespace Node�� �̹� Free�Ǿ����ϴ�.*/
                    IDE_TEST( iduMemMgr::free(sSpaceNode) != IDE_SUCCESS );
                }
                break;
            default:
                break;
        }
    }

    IDE_TEST( mMtxCrtTBS.destroy() != IDE_SUCCESS );

    IDE_TEST( mGlobalPageCountCheckMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    ( Disk/Memory ���� ) Tablespace Node�� �ʱ�ȭ�Ѵ�.

    [�˰���]
    (010) Tablespace �̸��� ����
    (020) Tablespace Attribute���� ����
    (030) Mutex �ʱ�ȭ
    (040) Lock Item �޸� �Ҵ�, �ʱ�ȭ
*/
IDE_RC sctTableSpaceMgr::initializeTBSNode( sctTableSpaceNode * aSpaceNode,
                                            smiTableSpaceAttr * aSpaceAttr )
{
    SChar sMutexName[128];
    UInt  sState = 0;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aSpaceAttr != NULL );


    ///////////////////////////////////////////////////////////
    // (010) Tablespace �̸��� ����
    aSpaceNode->mName = NULL;

    /* sctTableSpaceMgr_initializeTBSNode_malloc_Name.tc */
    IDU_FIT_POINT("sctTableSpaceMgr::initializeTBSNode::malloc::Name");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SCT,
                                 1,
                                 aSpaceAttr->mNameLength + 1,
                                 (void**)&(aSpaceNode->mName),
                                 IDU_MEM_FORCE ) 
              != IDE_SUCCESS );
    sState = 1;

    idlOS::strcpy( aSpaceNode->mName, aSpaceAttr->mName );

    ///////////////////////////////////////////////////////////
    // (020) Tablespace Attribute���� ����
    // tablespace�� ID
    aSpaceNode->mID  = aSpaceAttr->mID;
    // tablespace Ÿ��
    aSpaceNode->mType =  aSpaceAttr->mType;
    // tablespace�� online ����
    aSpaceNode->mState = aSpaceAttr->mTBSStateOnLA;
    // table create SCN�� 0���� �ʱ�ȭ �Ѵ�.
    SM_INIT_SCN( &( aSpaceNode->mMaxTblDDLCommitSCN ) );

    ///////////////////////////////////////////////////////////
    // (030) Mutex �ʱ�ȭ
    idlOS::sprintf( sMutexName, "TABLESPACE_%"ID_UINT32_FMT"_SYNC_MUTEX",
                    aSpaceAttr->mID );

    IDE_TEST( aSpaceNode->mSyncMutex.initialize( sMutexName,
                                                 IDU_MUTEX_KIND_POSIX,
                                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 2;

    ///////////////////////////////////////////////////////////
    // (040) Lock Item �ʱ�ȭ
    IDE_TEST( smLayerCallback::allocLockItem( &(aSpaceNode->mLockItem4TBS) )
              != IDE_SUCCESS );
    sState = 3;


    IDE_TEST( smLayerCallback::initLockItem( aSpaceAttr->mID, // TBS ID
                                             ID_ULONG_MAX,    // TABLE OID
                                             SMI_LOCK_ITEM_TABLESPACE,
                                             aSpaceNode->mLockItem4TBS )
              != IDE_SUCCESS );
    sState = 4;

    idlOS::sprintf( sMutexName, "TABLESPACE_NODE_MUTEX_%"ID_UINT32_FMT"",
                    aSpaceAttr->mID );

    IDE_TEST( aSpaceNode->mMutex.initialize( sMutexName,
                                             IDU_MUTEX_KIND_POSIX,
                                             IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 5;

    idlOS::sprintf( sMutexName, "TABLESPACE_BACKUP_COND_%"ID_UINT32_FMT"",
                    aSpaceAttr->mID );

    IDE_TEST_RAISE( aSpaceNode->mBackupCV.initialize( sMutexName )
                    != IDE_SUCCESS, error_cond_init);
    sState = 6;

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_init);
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondInit ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sState )
    {
        case 6:
            IDE_ASSERT( aSpaceNode->mBackupCV.destroy() == IDE_SUCCESS );
        case 5:
            IDE_ASSERT( aSpaceNode->mMutex.destroy() == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( smLayerCallback::destroyLockItem( aSpaceNode->mLockItem4TBS )
                        == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( smLayerCallback::freeLockItem( aSpaceNode->mLockItem4TBS )
                        == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( aSpaceNode->mSyncMutex.destroy() == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( iduMemMgr::free( aSpaceNode->mName )
                        == IDE_SUCCESS );

            aSpaceNode->mName = NULL;

        default :
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*
    ( Disk/Memory ���� ) Tablespace Node�� �ı��Ѵ�.

    initializeTBSNode�� �ʱ�ȭ ������ �������� �ı�

    [�˰���]
    (010) Lock Item �ı�, �޸� ����
    (020) Mutex �ı�
    (030) Tablespace �̸� �޸� ���� �ݳ�
*/
IDE_RC sctTableSpaceMgr::destroyTBSNode(sctTableSpaceNode * aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    /////////////////////////////////////////////////////////////
    // (010) Lock Item �ı�, �޸� ����
    IDE_ASSERT( aSpaceNode->mLockItem4TBS != NULL );

    IDE_ASSERT( smLayerCallback::destroyLockItem( aSpaceNode->mLockItem4TBS )
                == IDE_SUCCESS );

    IDE_ASSERT( smLayerCallback::freeLockItem( aSpaceNode->mLockItem4TBS )
                == IDE_SUCCESS );
    aSpaceNode->mLockItem4TBS = NULL;

    /////////////////////////////////////////////////////////////
    // (020) Mutex �ı�
    IDE_ASSERT( aSpaceNode->mBackupCV.destroy() == IDE_SUCCESS );
    IDE_ASSERT( aSpaceNode->mMutex.destroy() == IDE_SUCCESS );
    IDE_ASSERT( aSpaceNode->mSyncMutex.destroy() == IDE_SUCCESS );

    /////////////////////////////////////////////////////////////
    // (030) Tablespace �̸� �޸� ���� �ݳ�
    IDE_ASSERT( aSpaceNode->mName != NULL );
    IDE_ASSERT( iduMemMgr::free( aSpaceNode->mName )
                == IDE_SUCCESS );
    aSpaceNode->mName = NULL;


    return IDE_SUCCESS;
}

/*
    Tablespace�� Sync Mutex�� ȹ���Ѵ�.

    Checkpoint Thread�� �ش� Tablespace�� Checkpointing�� ���� ���ϵ����Ѵ�

    [IN] aSpaceNode - Sync Mutex�� ȹ���� Tablespace Node

 */
IDE_RC sctTableSpaceMgr::latchSyncMutex( sctTableSpaceNode * aSpaceNode )
{

    IDE_DASSERT( aSpaceNode != NULL );

    IDE_TEST( aSpaceNode->mSyncMutex.lock( NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace�� Sync Mutex�� Ǯ���ش�.

    Checkpoint Thread�� �ش� Tablesapce�� Checkpointing�� �ϵ��� ����Ѵ�.

    [IN] aSpaceNode - Sync Mutex�� Ǯ���� Tablespace Node
*/
IDE_RC sctTableSpaceMgr::unlatchSyncMutex( sctTableSpaceNode * aSpaceNode )
{
    IDE_ASSERT( aSpaceNode != NULL );

    IDE_TEST( aSpaceNode->mSyncMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 **********************************************************************/
void sctTableSpaceMgr::addTableSpaceNode( sctTableSpaceNode *aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    IDE_ASSERT( mSpaceNodeArray[aSpaceNode->mID] == NULL );

    mSpaceNodeArray[aSpaceNode->mID] = aSpaceNode;
}

void sctTableSpaceMgr::removeTableSpaceNode( sctTableSpaceNode *aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    IDE_ASSERT( mSpaceNodeArray[aSpaceNode->mID] != NULL );

    mSpaceNodeArray[aSpaceNode->mID] = NULL;
}



/*
   ��� : ���̺��� ���̺����̽��� ���Ͽ� INTENTION
          Lock And Validation

          smiValidateAndLockTable(), smiTable::lockTable, Ŀ�� open�� ȣ��

   ����

   [IN]  aTrans        : Ʈ�����(smxTrans)�� void* ��
   [IN]  aSpaceID      : Lock�� ȹ���� TBS ID
   [IN]  aTBSLvType    : ���̺����̽� Lock Validation Ÿ��
   [IN]  aIsIntent     : ������忡 ���� INTENTION ����
   [IN]  aIsExclusive  : ������忡 ���� Exclusive ����
   [IN]  aLockWaitMicroSec : ��ݿ�û�� Wait �ð�

*/
IDE_RC sctTableSpaceMgr::lockAndValidateTBS(
                        void                * aTrans,           /* [IN] */
                        scSpaceID             aSpaceID,         /* [IN] */
                        sctTBSLockValidOpt    aTBSLvOpt,        /* [IN] */
                        idBool                aIsIntent,        /* [IN] */
                        idBool                aIsExclusive,     /* [IN] */
                        ULong                 aLockWaitMicroSec )
{
    idBool                sLocked;

    IDE_DASSERT( aTrans != NULL );

    // PRJ-1548 User Memory Tablespace
    // ��ݰ����� ���� ������
    //
    IDE_TEST( lockTBSNodeByID( aTrans,
                               aSpaceID,
                               aIsIntent,
                               aIsExclusive,
                               aLockWaitMicroSec,
                               aTBSLvOpt,
                               &sLocked,
                               NULL) != IDE_SUCCESS );

    // ����, Trylock�� �õ��غ��� ����� ȹ������ ���� ��� ������
    // ���� üũ�ؼ� Exception�� �߻���Ų��.
    IDE_TEST( sLocked != ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
   ��� : ���̺��� Index Lob �÷����� ���̺����̽��鿡 ���Ͽ�
          INTENTION Lock And Validation

          smiValidateAndLockTable(), smiTable::lockTable, Ŀ�� open�� ȣ��

          �� �Լ��� Table�� ���� LockAnd Validate�� ������ �Ŀ�
          ����Ǿ�� �Ѵ�

   ����

   [IN]  aTrans        : Ʈ�����(smxTrans)�� void* ��
   [IN]  aTable        : ���̺����(smcTableHeader)��  void* ��
   [IN]  aTBSLvOpt     : ���̺����̽� Lock Validation Option
   [IN]  aIsIntent     : ������忡 ���� INTENTION ����
   [IN]  aIsExclusive  : ������忡 ���� Exclusive ����
   [IN]  aLockWaitMicroSec : ��ݿ�û�� Wait �ð�

*/
IDE_RC sctTableSpaceMgr::lockAndValidateRelTBSs(
                       void                * aTrans,           /* [IN] */
                       void                * aTable,           /* [IN] */
                       sctTBSLockValidOpt    aTBSLvOpt,        /* [IN] */
                       idBool                aIsIntent,        /* [IN] */
                       idBool                aIsExclusive,     /* [IN] */
                       ULong                 aLockWaitMicroSec )
{
    UInt                  sIdx;
    UInt                  sIndexCount;
    UInt                  sLobColCount;
    UInt                  sColCount;
    scSpaceID             sIndexTBSID;
    scSpaceID             sLobTBSID;
    scSpaceID             sLockedTBSID;
    scGRID              * sGRID;
    void                * sIndexHeader;
    smiColumn           * sColumn;
    UInt                  sColumnType;
    scSpaceID             sTableTBSID;
    idBool                sLocked;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTable != NULL );

    sIndexHeader = NULL;
    sIndexCount  = 0;

    // 1. ���̺� ���� ���̺����̽� ��忡 ���� ��� ��û
    sTableTBSID = smLayerCallback::getTableSpaceID( aTable );

    sLockedTBSID  = sTableTBSID; // �̹� ����� ȹ��� TableSpace

    if ( isDiskTableSpace( sTableTBSID ) == ID_TRUE )
    {
        // 2. ���̺��� �ε��� ���� ���̺����̽� ��忡 ���� ��� ��û
        //    ���̺� ���õ� �ε����� ����� ��� ���̺����̽� ��忡
        //    ���� ����� ȹ���Ѵ�.
        //    ����� ȹ���� TBS Node�� ���ؼ� �ߺ� ���ȹ���� ����Ѵ�.        i
        //    (�����̽�)
        //
        //    ��, �޸� �ε����� ���� ���̺����̽� ���� �������� �����Ƿ�
        //    ����� �ʿ����� �ʴ�.

        sIndexCount  = smLayerCallback::getIndexCount( aTable );

        // �ε��� TBS Node�� ����� ȹ���Ѵ�.
        for ( sIdx = 0 ; sIdx < sIndexCount ; sIdx++ )
        {
            sIndexHeader = (void*)smLayerCallback::getTableIndex( aTable, sIdx );

            sGRID = smLayerCallback::getIndexSegGRIDPtr( sIndexHeader );

            sIndexTBSID  = SC_MAKE_SPACE( *sGRID );

            // �ϴ�, ���������� �ߺ��� ��쿡 ���ؼ� ����� ȸ���ϵ���
            // ������ ó���ϱ��Ѵ�. (�����̽�)

            if ( sLockedTBSID != sIndexTBSID )
            {
                IDE_TEST( lockTBSNodeByID( aTrans,
                                           sIndexTBSID,
                                           aIsIntent,
                                           aIsExclusive,
                                           aLockWaitMicroSec,
                                           aTBSLvOpt,
                                           &sLocked,
                                           NULL ) != IDE_SUCCESS );

                // ����, Trylock�� �õ��غ��� ����� ȹ������ ���� ��� ������
                // ���� üũ�ؼ� Exception�� �߻���Ų��.
                IDE_TEST( sLocked != ID_TRUE );

                sLockedTBSID = sIndexTBSID;
            }
        }

        // 3. ���̺��� LOB Column ���� ���̺����̽� ��忡 ���� ��� ��û
        //    ���̺� ���õ� LOB Column �� ����� ��� ���̺����̽� ��忡
        //    ���� ����� ȹ���Ѵ�.
        //    ����� ȹ���� TBS Node�� ���ؼ� �ߺ� ���ȹ���� ����Ѵ�.
        //    (�����̽�)
        //
        //    ��, �޸� LOB Column���� ���̺����̽� ���� ���̺�� ������
        //    ���̺����̽��Ƿ� ����� �ʿ����.
        //    ���̺� �ε��� ������ ���Ѵ�.

        sLockedTBSID = sTableTBSID;

        // ���̺� Lob Column ������ ���Ѵ�.
        sColCount    = smLayerCallback::getColumnCount( aTable );
        sLobColCount = smLayerCallback::getTableLobColumnCount( aTable );

        // Lob Column�� TBS Node�� ����� ȹ���Ѵ�.
        for ( sIdx = 0 ; sIdx < sColCount ; sIdx++ )
        {
            if ( sLobColCount == 0 )
            {
                break;
            }

            sColumn =
                (smiColumn*)smLayerCallback::getTableColumn( aTable, sIdx );

            sColumnType =
                sColumn->flag & SMI_COLUMN_TYPE_MASK;

            if ( sColumnType == SMI_COLUMN_TYPE_LOB )
            {
                sLobColCount--;
                sLobTBSID  = sColumn->colSpace;
                IDE_ASSERT( sColumn->colSpace == SC_MAKE_SPACE(sColumn->colSeg) );

                // �ϴ�, ���������� �ߺ��� ��쿡 ���ؼ� ����� ȸ���ϵ���
                // ������ ó���ϱ��Ѵ�. (�����̽�)

                if ( sLockedTBSID != sLobTBSID )
                {
                    IDE_TEST( lockTBSNodeByID( aTrans,
                                               sLobTBSID,
                                               aIsIntent,
                                               aIsExclusive,
                                               aLockWaitMicroSec,
                                               aTBSLvOpt,
                                               &sLocked,
                                               NULL ) != IDE_SUCCESS );

                    // ����, Trylock�� �õ��غ��� ����� ȹ������ ���� ��� ������
                    // ���� üũ�ؼ� Exception�� �߻���Ų��.
                    IDE_TEST( sLocked != ID_TRUE );

                    sLockedTBSID = sLobTBSID;
                }
                else  /* sLockedTBSID == sLobTBSID */
                {
                    /* nothing to do */
                }
            }
            else /* ColumnType != SMI_COLUMN_TYPE_LOB */ 
            {
                /* nothing to do */
            }
        }

        IDE_ASSERT( sLobColCount == 0 );
    }
    else
    {
        // Memory TableSpace�� ���� Index�� Lob Column TableSpace��
        // ���̺�� �����ϱ� ������ ������� �ʴ´�.
        IDE_DASSERT( ( isMemTableSpace( sTableTBSID ) == ID_TRUE ) ||
                     ( isVolatileTableSpace( sTableTBSID ) == ID_TRUE ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ��� : ���̺����̽� ��忡 ���� ����� ȹ���Ѵ�.
   �����ּ� : lockTBSNode�� ����
*/
IDE_RC sctTableSpaceMgr::lockTBSNodeByID( void             * aTrans,
                                          scSpaceID          aSpaceID,
                                          idBool             aIsIntent,
                                          idBool             aIsExclusive,
                                          ULong              aLockWaitMicroSec,
                                          sctTBSLockValidOpt aTBSLvOpt,
                                          idBool           * aLocked,
                                          sctLockHier      * aLockHier )
{
    sctTableSpaceNode * sSpaceNode;

    IDE_DASSERT( aTrans != NULL );

    // 1. �ý��� ���̺����̽� ���� ����� ȹ������ �ʴ´�.
    //    ��, DBF Node�� ���ؼ��� ����� ȹ�����ָ�
    //    CREATE DBF�� AUTOEXTEND MODE, �ڵ�Ȯ��, RESIZE ���갣��
    //    ���ü��� ������ �� �ִ�.
    if ( isSystemTableSpace( aSpaceID ) == ID_TRUE )
    {
        if ( aLocked != NULL )
        {
            *aLocked = ID_TRUE;
        }

        IDE_CONT( skip_lock_system_tbs );
    }

    // 2. ���̺� �����̽� ��� �����͸� ȹ��
    // BUG-28748 ���ſ��� SpaceNode�� No Latch Hash�� �����Ͽ�
    // Space Node�� Ž���� ���ؼ� Mutex�� �ʿ��Ͽ�����,
    // ������ Array�� ���� �ϹǷ� Space Node�� �������� ���ؼ�
    // Mutex�� ���� �ʾƵ� �˴ϴ�.
    IDE_TEST( findSpaceNodeBySpaceID( aSpaceID,
                                      (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode->mID == aSpaceID );

    IDE_TEST( lockTBSNode( aTrans,
                           sSpaceNode,
                           aIsIntent,
                           aIsExclusive,
                           aLockWaitMicroSec,
                           aTBSLvOpt,
                           aLocked,
                           aLockHier ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_lock_system_tbs );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ��� : ���̺����̽� ��忡 ���� ����� ȹ���Ѵ�.
          TBS ID�� �˻��Ͽ� ���̺����̽� ��带 ��ȯ�Ͽ��� �Ѵ�.
          �׷��Ƿ�, TBS List Mutex�� ȹ���� �ʿ䰡 �ִ�.

   ����

   [IN]  aTrans             : Ʈ�����(smxTrans)�� void* ��
   [IN]  aSpaceID           : ���̺����̽� ID
   [IN]  aIsIntent          : ������忡 ���� INTENTION ����
   [IN]  aIsExclusive       : ������忡 ���� Exclusive ����
   [IN]  aLockWaitMicroSec  : ��ݿ�û�� Wait �ð�
   [IN]  aTBSLvOpt        : Lockȹ���� Tablespace�� ���� üũ�� ���׵�
   [OUT] aLocked            : Lockȹ�濩��
   [OUT] aLockHier          : ���ȹ���� LockSlot ������

*/
IDE_RC sctTableSpaceMgr::lockTBSNode( void              * aTrans,
                                      sctTableSpaceNode * aSpaceNode,
                                      idBool              aIsIntent,
                                      idBool              aIsExclusive,
                                      ULong               aLockWaitMicroSec,
                                      sctTBSLockValidOpt  aTBSLvOpt,
                                      idBool           *  aLocked,
                                      sctLockHier      *  aLockHier )
{
    void              * sLockSlot;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aSpaceNode != NULL );

    sLockSlot  = NULL;

    // 2. �ý��� ���̺����̽� ���� ����� ȹ������ �ʴ´�.
    //    ��, DBF Node�� ���ؼ��� ����� ȹ�����ָ�
    //    CREATE DBF�� AUTOEXTEND MODE, �ڵ�Ȯ��, RESIZE ���갣��
    //    ���ü��� ������ �� �ִ�.
    if ( isSystemTableSpace( aSpaceNode->mID ) == ID_TRUE )
    {
        if ( aLocked != NULL )
        {
            *aLocked = ID_TRUE;
        }
    }
    else
    {
        // 3. Lock �����ڿ� ��ݿ�û�� ȹ���Ѵ�.
        IDE_TEST( smLayerCallback::lockItem( aTrans,
                                             aSpaceNode->mLockItem4TBS,
                                             aIsIntent,
                                             aIsExclusive,
                                             aLockWaitMicroSec,
                                             aLocked,
                                             &sLockSlot )
                  != IDE_SUCCESS );

        if ( aLocked != NULL )
        {
            // ����, Trylock�� �õ��غ��� ����� ȹ������ ���� ���
            // ������ ���� üũ�ؼ� Exception�� �߻���Ų��.
            IDE_TEST( *aLocked == ID_FALSE );
        }

        if ( aLockHier != NULL )
        {
            // Short-Duration ����� ȹ���Ͽ� ����ϱ� ���Ѵٸ�
            // ���ȹ���� LockSlot �����͸� LockHier�� ����Ѵ�.
            aLockHier->mTBSNodeSlot = sLockSlot;
        }

        IDE_TEST( validateTBSNode( aSpaceNode,
                                   aTBSLvOpt ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ��� : ���̺����̽� ��忡 ���� ����� ȹ���Ѵ�.
   �����ּ� : ���� lockTBSNode�� ����
*/

IDE_RC sctTableSpaceMgr::lockTBSNodeByID( void              * aTrans,
                                          scSpaceID           aSpaceID,
                                          idBool              aIsIntent,
                                          idBool              aIsExclusive,
                                          sctTBSLockValidOpt  aTBSLvOpt )
{
    return lockTBSNodeByID( aTrans,
                            aSpaceID,
                            aIsIntent,
                            aIsExclusive,
                            sctTableSpaceMgr::getDDLLockTimeOut( (smxTrans*)aTrans ),
                            aTBSLvOpt,
                            NULL,
                            NULL );
}

/*
   ��� : ���̺����̽� ��忡 ���� ����� ȹ���Ѵ�.
   �����ּ� : ���� lockTBSNode�� ����
*/
IDE_RC sctTableSpaceMgr::lockTBSNode( void              * aTrans,
                                      sctTableSpaceNode * aSpaceNode,
                                      idBool              aIsIntent,
                                      idBool              aIsExclusive,
                                      sctTBSLockValidOpt  aTBSLvOpt )
{
    return lockTBSNode( aTrans,
                        aSpaceNode,
                        aIsIntent,
                        aIsExclusive,
                        sctTableSpaceMgr::getDDLLockTimeOut((smxTrans*)aTrans),
                        aTBSLvOpt,
                        NULL,
                        NULL );
}

/***********************************************************************
 * Description : tablespace�� new ID �Ҵ�
 *
 * tablespace ID�� ��ȯ�Ѵ�. ���̵�� 1�� �����ϴ� �����̰� tablespace ID��
 * ����Ǵ� ��찡 ���ٰ� ���Ѵ�. �α׸� ����
 * �� tablespace�� ����Ȱ����� �Ǵ��� ����� ���� �����̴�.
 * ���� Max Tablespace���� creates�Ǹ� �� �Ŀ��� tablespace�� create�� �� ����.
 *
 * + 2nd. code design
 *   - mNewTableSpaceID�� +1 �Ѵ�.
 **********************************************************************/
IDE_RC sctTableSpaceMgr::allocNewTableSpaceID( scSpaceID*   aNewID )
{

    IDE_DASSERT( aNewID != NULL );

    IDE_TEST_RAISE( mNewTableSpaceID == (SC_MAX_SPACE_COUNT -1),
                    error_not_enough_tablespace_id );

    *aNewID = mNewTableSpaceID++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_enough_tablespace_id );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotEnoughTableSpaceID,
                                  mNewTableSpaceID) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/**********************************************************************
 * Description : Tablespace ID�� SpaceNode�� ã�´�.
 * - �ش� Tablespace�� DROP�� ��� ������ �߻���Ų��.
 *
 * BUG-28748 ���ſ��� SpaceNode�� No Latch Hash�� �����Ͽ�
 * SpaceNode�� Ž���ϱ� ���ؼ��� �ݵ�� Mutex�� ��ƾ� �Ͽ�����,
 * ������ Array�� �����ϹǷ�, Space Node�� ã�� �Ϳ� ���ؼ���
 * Mutex�� ���� �ʾƵ� �˴ϴ�.
 *
 * [IN]  aSpaceID   - Tablespace ID
 * [OUT] aSpaceNode - Tablespace Node
 **********************************************************************/
IDE_RC sctTableSpaceMgr::findSpaceNodeBySpaceID( idvSQL   * aStatistics,
                                                 scSpaceID  aSpaceID,
                                                 void**     aSpaceNode,
                                                 idBool     aLockSpace )
{
    sctTableSpaceNode * sSpaceNode ;
    idBool              sIsLocked = ID_FALSE;

    IDE_ERROR( aSpaceNode != NULL );

    IDE_TEST_RAISE( aSpaceID >= mNewTableSpaceID,
                    error_not_found_tablespace_node );

    sSpaceNode  = mSpaceNodeArray[aSpaceID];
    *aSpaceNode = sSpaceNode; 

    IDE_TEST_RAISE( sSpaceNode == NULL,
                    error_not_found_tablespace_node );

    IDE_ASSERT_MSG( sSpaceNode->mID == aSpaceID,
                    "Node Space ID : %"ID_UINT32_FMT"\n"
                    "Req Space ID  : %"ID_UINT32_FMT"\n",
                    (*(sctTableSpaceNode**)aSpaceNode)->mID,
                    aSpaceID );

    if ( aLockSpace == ID_TRUE )
    {
        lockSpaceNode( aStatistics,
                       sSpaceNode );
        sIsLocked = ID_TRUE;
    }

    IDE_TEST_RAISE( SMI_TBS_IS_DROPPED( sSpaceNode->mState ),
                    error_not_found_tablespace_node );

    // Tablespace Drop Pending ���൵�� ����� ��쿡��
    // Drop�Ȱ����� ����
    IDE_TEST_RAISE( SMI_TBS_IS_DROP_PENDING( sSpaceNode->mState ),
                    error_not_found_tablespace_node );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID) );
    }
    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/*
   PRJ-1548 User Memory Tablespace
   ���̺����̽� ��� �˻�
   �������� �ʰų� DROPPED ������ ���� NULL�� ��ȯ�Ѵ�.

 [IN]  aSpaceID   - Tablespace ID
 [OUT] aSpaceNode - Tablespace Node
*/
sctTableSpaceNode* sctTableSpaceMgr::findSpaceNodeWithoutException( scSpaceID  aSpaceID,
                                                                    idBool     aUsingTBSAttr )
{
    sctTableSpaceNode *sSpaceNode;
    UInt               sTBSState;

    sSpaceNode = mSpaceNodeArray[aSpaceID];

    if ( sSpaceNode != NULL )
    {
        if ( (aUsingTBSAttr == ID_TRUE) &&
             (isMemTableSpace( sSpaceNode ) == ID_TRUE) )
        { 
            sTBSState = ((smmTBSNode*)sSpaceNode)->mTBSAttr.mTBSStateOnLA;
        }
        else
        {
            sTBSState = sSpaceNode->mState;
        }

        // Tablespace Drop Pending ���൵�� ����� ��쿡��
        // Drop�Ȱ����� ����
        if ( SMI_TBS_IS_DROPPED(sTBSState) ||
             SMI_TBS_IS_DROP_PENDING(sTBSState) )
        {
            sSpaceNode = NULL;
        }
    }

    return sSpaceNode;
}


/*
   Tablespace�� �����ϴ��� üũ�Ѵ�.

   ������ ��� ID_FALSE�� ��ȯ�Ѵ�.
     - �������� �ʴ� Tablespace
     - Drop�� Tablespace
   �� �ܿ��� ID_TRUE�� ��ȯ�Ѵ�.
     ( Offline, Discard�� Tablespace�� ��쿡�� ID_TRUE�� ��ȯ )
 */
idBool sctTableSpaceMgr::isExistingTBS( scSpaceID aSpaceID )
{
    sctTableSpaceNode *sSpaceNode;
    idBool             sExist;

    sSpaceNode = findSpaceNodeWithoutException( aSpaceID );

    if ( sSpaceNode == NULL )
    {
        sExist = ID_FALSE;
    }
    else
    {
        sExist = ID_TRUE;
    }
    return sExist;
}




/*
   Tablespace�� Memory�� Load�Ǿ����� üũ�Ѵ�.

   ������ ��� ID_FALSE�� ��ȯ�Ѵ�.
     - �������� �ʴ� Tablespace
     - Drop�� Tablespace
     - OFFLINE�� Tablespace
   �� �ܿ��� ID_TRUE�� ��ȯ�Ѵ�.
 */
idBool sctTableSpaceMgr::isOnlineTBS( scSpaceID aSpaceID )
{
    idBool sIsOnline;

    sctTableSpaceNode *sSpaceNode;
    sSpaceNode = findSpaceNodeWithoutException( aSpaceID );

    if ( sSpaceNode == NULL )
    {
        sIsOnline = ID_FALSE;
    }
    else
    {
        // findSpaceNodeWithoutException �� Drop�� Tablespace�� ���
        // TBSNode�� NULL�� �����Ѵ�.
        // TBSNode�� NULL�� �ƴϹǷ�, TBS�� ���°� DROPPED�� �� ����.
        IDE_ASSERT( ( sSpaceNode->mState & SMI_TBS_DROPPED )
                    != SMI_TBS_DROPPED );

        if ( SMI_TBS_IS_ONLINE(sSpaceNode->mState) )
        {
            sIsOnline = ID_TRUE;
        }
        else
        {
            // OFFLINE�̰ų� DISCARD�� TABLESPACE
            sIsOnline = ID_FALSE;
        }
    }

    return sIsOnline;
}

/* Tablespace�� ���� State�� �ϳ��� State�� ���ϴ��� üũ�Ѵ�.

   [IN] aSpaceID  - ���¸� üũ�� Tablespace�� ID
   [IN] aStateSet - �ϳ��̻��� Tablespace���¸� OR�� ���� State Set
 */
idBool sctTableSpaceMgr::hasState( scSpaceID   aSpaceID,
                                   sctStateSet aStateSet,
                                   idBool      aUsingTBSAttr )
{
    idBool             sRet = ID_FALSE;
    sctTableSpaceNode *sSpaceNode;
    UInt               sTBSState;

    IDE_DASSERT( aStateSet != SCT_SS_INVALID );

    sSpaceNode = findSpaceNodeWithoutException( aSpaceID, aUsingTBSAttr );

    // DROP�� Tablespace
    if ( sSpaceNode == NULL )
    {
        // StateSet�� DROPPED�� ������ ID_TRUE����
        if ( SMI_TBS_IS_DROPPED( aStateSet) )
        {
            sRet = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        if ( ( aUsingTBSAttr == ID_TRUE ) &&
             ( isMemTableSpace(aSpaceID) == ID_TRUE ) )
        { 
            sTBSState = ((smmTBSNode*)sSpaceNode)->mTBSAttr.mTBSStateOnLA;
        }
        else
        {
            sTBSState = sSpaceNode->mState;
        }

        // Tablespace�� State�� aStateSet�� ���ϴ� ����
        // State�� �ϳ��� ���ϴ� ��� ID_TRUE����
        if ( ( sTBSState & aStateSet ) != 0 )
        {
            sRet = ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }

    return sRet;
}

/* Tablespace�� ���� State�� �ϳ��� State�� ���ϴ��� üũ�Ѵ�.

   [IN] aSpaceNode - ���¸� üũ�� Tablespace�� Node
   [IN] aStateSet  - �ϳ��̻��� Tablespace���¸� OR�� ���� State Set
 */
idBool sctTableSpaceMgr::hasState( sctTableSpaceNode * aSpaceNode,
                                   sctStateSet         aStateSet )
{
    IDE_DASSERT( aSpaceNode != NULL );

    return isStateInSet( aSpaceNode->mState, aStateSet );
}




/* Tablespace�� State�� aStateSet���� State�� �ϳ��� �ִ��� üũ�Ѵ�.

   [IN] aTBSState  - ���¸� üũ�� Tablespace�� State
   [IN] aStateSet  - �ϳ��̻��� Tablespace���¸� OR�� ���� State Set
 */
idBool sctTableSpaceMgr::isStateInSet( UInt        aTBSState,
                                       sctStateSet aStateSet )
{
    idBool sHasState ;

    // Tablespace�� State�� aStateSet�� ���ϴ� ����
    // State�� �ϳ��� ���ϴ� ��� ID_TRUE����
    if ( ( aTBSState & aStateSet ) != 0 )
    {
        sHasState = ID_TRUE;
    }
    else
    {
        sHasState = ID_FALSE;
    }

    return sHasState;
}


/*
   Tablespace���� Table/Index�� Open�ϱ� ���� Tablespace��
   ��� �������� üũ�Ѵ�.

   [IN] aSpaceNode - Tablespace�� Node
   [IN] aValidate  -

   [����] �ش� Tablespace�� Lock�� ���� ä�� �� �Լ��� �ҷ���
           �� �Լ�ȣ������� ��Ȳ�� �״�� �������� ������ �� �ִ�.

   ������ ��� ������ �߻���Ų��.
     - �������� �ʴ� Tablespace
     - Drop�� Tablespace
     - Discard�� Tablespace
     - Offline Tablespace
 */
IDE_RC sctTableSpaceMgr::validateTBSNode( sctTableSpaceNode * aSpaceNode,
                                          sctTBSLockValidOpt  aTBSLvOpt )
{
    IDE_DASSERT( aSpaceNode != NULL );

    if ( ( aTBSLvOpt & SCT_VAL_CHECK_DROPPED ) == SCT_VAL_CHECK_DROPPED )
    {
        // DROP�� ���̺����̽��� ��� ���̺����̽���
        // �������� �ʴ´ٴ� Exception���� ó���Ѵ�.
        IDE_TEST_RAISE( SMI_TBS_IS_DROPPED(aSpaceNode->mState),
                        error_not_found_tablespace_node );
    }



    if ( ( aTBSLvOpt & SCT_VAL_CHECK_DISCARDED ) ==
         SCT_VAL_CHECK_DISCARDED )
    {
        // Discard�� Tablespace�� ���
        IDE_TEST_RAISE( SMI_TBS_IS_DISCARDED(aSpaceNode->mState),
                        error_unable_to_use_discarded_tbs );
    }


    if ( ( aTBSLvOpt & SCT_VAL_CHECK_OFFLINE ) ==
         SCT_VAL_CHECK_OFFLINE )
    {
        // Offline Tablespace�� ���
        IDE_TEST_RAISE( SMI_TBS_IS_OFFLINE(aSpaceNode->mState),
                        error_unable_to_use_offline_tbs );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_NotFoundTableSpaceNode,
                                 aSpaceNode->mID) );
    }
    IDE_EXCEPTION( error_unable_to_use_discarded_tbs );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_UNABLE_TO_USE_DISCARDED_TBS,
                                 aSpaceNode->mName ) );
    }
    IDE_EXCEPTION( error_unable_to_use_offline_tbs );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_UNABLE_TO_USE_OFFLINE_TBS,
                                 aSpaceNode->mName ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/**********************************************************************
 * Description : TBS���� �Է¹޾� ���̺����̽� Node�� ��ȯ�Ѵ�.
 *               �������� TBS Node�� ���� ��� NULL�� ��ȯ �Ͽ�����
 *               BUG-26695�� ���Ͽ� isExistTBSNodeByName()�� �뵵�� ����
 *               �ѷ� ���������Ǹ鼭 TBS Node�� ���� ��� ������ ��ȯ�ϴ� ������
 *               �����Ǿ���.
 *
 *   aName      - [IN]  TBS Node�� ã�� TBS�� �̸�
 *   aSpaceNode - [OUT] ã�� TBS Node�� ��ȯ
 **********************************************************************/
IDE_RC sctTableSpaceMgr::findSpaceNodeByName( SChar* aName,
                                              void** aSpaceNode,
                                              idBool aLockSpace )
{
    idBool             sIsLocked = ID_FALSE;
    UInt               i;
    idBool             sIsExist = ID_FALSE;
    sctTableSpaceNode *sSpaceNode;

    IDE_DASSERT( aName != NULL );
    IDE_DASSERT( aSpaceNode != NULL );

    for ( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        if ( aLockSpace == ID_TRUE )
        {
            lockSpaceNode( NULL, sSpaceNode );
            sIsLocked = ID_TRUE;
        }

        if ( !SMI_TBS_IS_DROPPED(sSpaceNode->mState) )
        {
            if ( idlOS::strcmp( sSpaceNode->mName, aName) == 0 )
            {
                *aSpaceNode = sSpaceNode;
                sIsExist = ID_TRUE;
                break;
            }
        }

        if ( aLockSpace == ID_TRUE )
        {
            sIsLocked = ID_FALSE;
            unlockSpaceNode( sSpaceNode );
        }
    }

    IDE_TEST_RAISE( sIsExist == ID_FALSE,
                    error_not_found_tablespace_node_by_name );

    // Tablespace Drop Pending ���൵�� ����� ��쿡��
    // Drop�Ȱ����� ����
    IDE_TEST_RAISE( SMI_TBS_IS_DROP_PENDING( sSpaceNode->mState ),
                    error_not_found_tablespace_node_by_name );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node_by_name );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNodeByName,
                                  aName) );
    }
    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description : TBS Node Name�� �Ѱܹ޾� �����ϴ����� Ȯ���Ѵ�.
 *
 *   aName - [IN] TBS Node�� �����ϴ��� Ȯ���� TBS�� Name
 **********************************************************************/
idBool sctTableSpaceMgr::checkExistSpaceNodeByName( SChar* aTableSpaceName )
{
    UInt               i;
    idBool             sIsExist = ID_FALSE;
    sctTableSpaceNode *sSpaceNode;

    IDE_DASSERT( aTableSpaceName != NULL );

    for ( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        if ( SMI_TBS_IS_DROPPED(sSpaceNode->mState) )
        {
            continue;
        }

        if ( idlOS::strcmp(sSpaceNode->mName, aTableSpaceName) == 0 )
        {
            sIsExist = ID_TRUE;
            break;
        }
    }

    return sIsExist;
}


/**********************************************************************
 * Description : for creating table on Query Processing
 **********************************************************************/
IDE_RC sctTableSpaceMgr::getTBSAttrByName( SChar*              aName,
                                           smiTableSpaceAttr*  aSpaceAttr )
{

    UInt               sState = 0;
    sctTableSpaceNode* sSpaceNode = NULL;

    IDE_DASSERT( aName != NULL );
    IDE_DASSERT( aSpaceAttr != NULL );

    IDE_TEST( findSpaceNodeByName( aName,
                                   (void**)&sSpaceNode,
                                   ID_TRUE ) // Lock
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sSpaceNode == NULL );

    IDE_TEST_RAISE( SMI_TBS_IS_CREATING(sSpaceNode->mState),
                    error_not_found_tablespace_node_by_name );

    switch( getTBSLocation( sSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode*)sSpaceNode,
                                               aSpaceAttr );
            break;
        case SMI_TBS_VOLATILE:
            svmManager::getTableSpaceAttr( (svmTBSNode*)sSpaceNode,
                                          aSpaceAttr );
            break;
        case SMI_TBS_MEMORY:
            smmManager::getTableSpaceAttr( (smmTBSNode*)sSpaceNode,
                                           aSpaceAttr );
            break;
        default:
            break;
    }

    sState = 0;
    unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_tablespace_node_by_name );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNodeByName,
                                  aName) );
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/**********************************************************************
 * Description : ���̺����̽� ID�� �ش��ϴ� ���̺����̽� �Ӽ��� ��ȯ
 **********************************************************************/
IDE_RC sctTableSpaceMgr::getTBSAttrByID( idvSQL           * aStatistics,
                                         scSpaceID          aID,
                                         smiTableSpaceAttr* aSpaceAttr )
{
    UInt               sState = 0;
    sctTableSpaceNode* sSpaceNode;

    IDE_DASSERT( aSpaceAttr != NULL );

    IDE_TEST( findAndLockSpaceNodeBySpaceID( aStatistics,
                                             aID,
                                             (void**)&sSpaceNode )
              != IDE_SUCCESS );
    sState = 1;

    switch( sctTableSpaceMgr::getTBSLocation( sSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sddTableSpace::getTableSpaceAttr( (sddTableSpaceNode*)sSpaceNode,
                                               aSpaceAttr );
            break;
        case SMI_TBS_MEMORY:
            smmManager::getTableSpaceAttr( (smmTBSNode*)sSpaceNode,
                                           aSpaceAttr );
            break;
        case SMI_TBS_VOLATILE:
            svmManager::getTableSpaceAttr( (svmTBSNode*)sSpaceNode,
                                          aSpaceAttr );
            break;
        default:
            break;
    }

    sState = 0;
    unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/*
    Tablespace Attribute�� flag �� ��ȯ�Ѵ�.
 */
IDE_RC sctTableSpaceMgr::getTBSAttrFlagByID( scSpaceID   aSpaceID,
                                             UInt      * aAttrFlagPtr )
{
    sctTableSpaceNode* sSpaceNode;

    IDE_DASSERT( aAttrFlagPtr != NULL );

    IDE_TEST( findSpaceNodeBySpaceID( aSpaceID,
                                      (void**)&sSpaceNode )
              != IDE_SUCCESS );

    *aAttrFlagPtr = getTBSAttrFlag( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UInt sctTableSpaceMgr::getTBSAttrFlag( sctTableSpaceNode* aSpaceNode )
{
    UInt sAttrFlag = SMI_TBS_NONE;

    switch( sctTableSpaceMgr::getTBSLocation( aSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sAttrFlag = sddTableSpace::getTBSAttrFlag( (sddTableSpaceNode*)aSpaceNode );
            break;
        case SMI_TBS_MEMORY:
            sAttrFlag = smmManager::getTBSAttrFlag( (smmTBSNode*)aSpaceNode );
            break;
        case SMI_TBS_VOLATILE:
            sAttrFlag = svmManager::getTBSAttrFlag( (svmTBSNode*)aSpaceNode );
            break;
        default:
            break;
    }

    return sAttrFlag;
}

/*
    Tablespace Attribute�� flag �� �����Ѵ�.
 */
void sctTableSpaceMgr::setTBSAttrFlag( sctTableSpaceNode* aSpaceNode,
                                       UInt               aAttrFlag )
{
    switch( sctTableSpaceMgr::getTBSLocation( aSpaceNode ) )
    {
        case SMI_TBS_DISK:
            sddTableSpace::setTBSAttrFlag( (sddTableSpaceNode*)aSpaceNode,
                                           aAttrFlag );
            break;
        case SMI_TBS_MEMORY:
            smmManager::setTBSAttrFlag( (smmTBSNode*)aSpaceNode,
                                        aAttrFlag );
            break;
        case SMI_TBS_VOLATILE:
            svmManager::setTBSAttrFlag( (svmTBSNode*)aSpaceNode,
                                        aAttrFlag );
            break;
        default:
            break;
    }
}


/*
    Tablespace�� Attribute Flag�κ��� �α� ���࿩�θ� ���´�

    [IN] aSpaceID - Tablespace�� ID
    [OUT] aDoComp - Log���� ����
 */
IDE_RC sctTableSpaceMgr::getSpaceLogCompFlag( scSpaceID aSpaceID,
                                              idBool   *aDoComp )
{
    IDE_DASSERT( aDoComp != NULL );

    UInt        sAttrFlag;
    idBool      sDoComp;

    if ( sctTableSpaceMgr::getTBSAttrFlagByID( aSpaceID,
                                               &sAttrFlag )
         == IDE_SUCCESS )
    {
        if ( ( sAttrFlag & SMI_TBS_ATTR_LOG_COMPRESS_MASK )
             == SMI_TBS_ATTR_LOG_COMPRESS_TRUE )
        {
            sDoComp = ID_TRUE;
        }
        else
        {
            sDoComp = ID_FALSE;
        }
    }
    else
    {
        // ���� Tablespace�� �������� ���� ����
        if ( ideGetErrorCode() == smERR_ABORT_NotFoundTableSpaceNode )
        {
            IDE_CLEAR();
            // �⺻������ �����ϸ� �α� ���� �ǽ�
            sDoComp = ID_TRUE;
        }
        else
        {
            IDE_RAISE( err_get_tbs_attr );
        }
    }

    *aDoComp = sDoComp;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_get_tbs_attr)
    {
        // do nothing. continue
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : refine �ܰ迡�� ȣ��:��� temp tablespace�� �ʱ�ȭ��Ų��.
 **********************************************************************/
IDE_RC sctTableSpaceMgr::resetAllTempTBS( void *aTrans )
{
    UInt                i;
    sctTableSpaceNode*  sSpaceNode;

    IDE_DASSERT( aTrans != NULL );

    for( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        if ( (sSpaceNode->mType == SMI_DISK_SYSTEM_TEMP) ||
             (sSpaceNode->mType == SMI_DISK_USER_TEMP) )
        {
            if ( (sSpaceNode->mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
            {
                IDE_TEST( smLayerCallback::resetTBS( NULL,
                                                     sSpaceNode->mID,
                                                     aTrans )
                          != IDE_SUCCESS );
            }
            else
            {
                // fix BUG-17501
                // ���������� user disk temp tablespace reset��������
                // DROOPED ������ TBS�� ���� Assert �ɸ� �ȵ�.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ����Ÿ ���� �ش� ����.
 * ����Ÿ file�̸����� ����Ÿ ���� ��带 ã�´�.
 * -> smiMediaRecovery class�� ���Ͽ� �Ҹ���.
 **********************************************************************/
void  sctTableSpaceMgr::getDataFileNodeByName( SChar            * aFileName,
                                               sddDataFileNode ** aFileNode,
                                               scSpaceID        * aSpaceID,
                                               scPageID         * aFstPageID,
                                               scPageID         * aLstPageID,
                                               SChar           ** aSpaceName )
{
    UInt                i;
    scPageID            sFstPageID;
    scPageID            sLstPageID;
    sctTableSpaceNode*  sSpaceNode;

    IDE_DASSERT( aFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );
    IDE_DASSERT( aSpaceID  != NULL );

    *aFileNode = NULL;

    for ( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        if ( isDiskTableSpace( sSpaceNode ) == ID_FALSE )
        {
            continue;
        }

        lockSpaceNode( NULL /* idvSQL* */,
                       sSpaceNode );

        if ( SMI_TBS_IS_DROPPED( sSpaceNode->mState ) )
        {
            unlockSpaceNode( sSpaceNode );
            continue;
        }

        if ( sddTableSpace::getPageRangeByName( (sddTableSpaceNode*)sSpaceNode,
                                                aFileName,
                                                aFileNode,
                                                &sFstPageID,
                                                &sLstPageID) == IDE_SUCCESS )
        {
            if ( aFstPageID != NULL )
            {
                *aFstPageID = sFstPageID;
            }

            if ( aLstPageID != NULL )
            {
                *aLstPageID = sLstPageID;
            }

            if ( aSpaceName != NULL )
            {
                *aSpaceName = sSpaceNode->mName;
            }

            *aSpaceID = sSpaceNode->mID;
            unlockSpaceNode( sSpaceNode );
            break;
        }

        unlockSpaceNode( sSpaceNode );
    }
}

/***********************************************************************
 * Description : Ʈ����� Ŀ�� ������ �����ϱ� ���� ������ ���
 *
 * Disk Tablespace, Memory Tablespace��� �� ��ƾ�� �̿��Ѵ�.
 *
 * [IN] aTrans     : Pending Operation�� �����ϰ� �� Transaction
 * [IN] aSpaceID   : Pending Operation���� ����� �Ǵ� Tablespace
 * [IN] aIsCommit  : Commit�ÿ� �����ϴ� Pending Operation�̶�� ID_TRUE
 * [IN] aPendingOpType : Pending Operation�� ����
 * [OUT] aPendingOp : ���� ����� Pending Operation
 *                    aPendingOp != NULL�� ��쿡�� �����ȴ�.
 **********************************************************************/
IDE_RC sctTableSpaceMgr::addPendingOperation( void               * aTrans,
                                              scSpaceID            aSpaceID,
                                              idBool               aIsCommit,
                                              sctPendingOpType     aPendingOpType,
                                              sctPendingOp      ** aPendingOp ) /* = NULL*/
{

    UInt         sState = 0;
    smuList      *sPendingOpList;
    sctPendingOp *sPendingOp;

    IDE_DASSERT( aTrans   != NULL );

    /* sctTableSpaceMgr_addPendingOperation_malloc_PendingOpList.tc */
    IDU_FIT_POINT("sctTableSpaceMgr::addPendingOperation::malloc::PendingOpList");
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_SM_SDD,
                                ID_SIZEOF(smuList) + 1,
                                (void**)&sPendingOpList,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sState = 1;

    SMU_LIST_INIT_BASE(sPendingOpList);

    /* sctTableSpaceMgr_addPendingOperation_calloc_PendingOp.tc */
    IDU_FIT_POINT("sctTableSpaceMgr::addPendingOperation::calloc::PendingOp");
    IDE_TEST(iduMemMgr::calloc( IDU_MEM_SM_SDD,
                                1,
                                ID_SIZEOF(sctPendingOp),
                                (void**)&sPendingOp,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sState = 2;

    sPendingOp->mIsCommit       = aIsCommit;
    sPendingOp->mPendingOpType  = aPendingOpType;
    sPendingOp->mTouchMode      = SMI_ALL_NOTOUCH; // meaningless
    sPendingOp->mSpaceID        = aSpaceID;
    sPendingOp->mResizePageSize = 0; // alter resize
    sPendingOp->mFileID         = 0; // meaningless

    SM_LSN_INIT( sPendingOp->mOnlineTBSLSN );

    sPendingOp->mPendingOpFunc  = NULL; // NULL�̸� �Լ� ȣ�� ����
    sPendingOp->mPendingOpParam = NULL; // NULL�̸� �������� ����

    sPendingOpList->mData = sPendingOp;

    smLayerCallback::addPendingOperation( aTrans, sPendingOpList );

    if ( aPendingOp != NULL )
    {
        *aPendingOp = sPendingOp;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sState )
    {
        case 2 :
            IDE_ASSERT( iduMemMgr::free(sPendingOp) == IDE_SUCCESS );
        case 1 :
            IDE_ASSERT( iduMemMgr::free(sPendingOpList) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*
   ��� : Ʈ������� ���̺����̽� ���� ���꿡 ����
   Commit/Rollback Pending Operation�� �����Ѵ�.

   [IN] aPendingOp : Pending ������
   [IN] aIsCommit  : Commit/Rollback ����
*/
IDE_RC sctTableSpaceMgr::executePendingOperation( idvSQL  * aStatistics,
                                                   void   * aPendingOp,
                                                  idBool    aIsCommit )
{
    idBool              sDoPending = ID_TRUE;
    UInt                sState     = 0;
    sctTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    sctPendingOp      * sPendingOp;

    sFileNode  = NULL;
    sSpaceNode = NULL;
    sPendingOp = (sctPendingOp*)aPendingOp;

    IDE_ASSERT( sPendingOp != NULL );

    IDE_TEST_CONT( sPendingOp->mIsCommit != aIsCommit, CONT_SKIP_PENDING );

    // TBS Node�� �������� �ʰų� DROPPED ������ ���� NULL�� ��ȯ�ȴ�.
    sSpaceNode = findSpaceNodeWithoutException( sPendingOp->mSpaceID );

    IDE_TEST_CONT( sSpaceNode == NULL, CONT_SKIP_PENDING );

    lockSpaceNode( aStatistics,
                   sSpaceNode );
    sState = 1;

    // Memory TBS�� Drop Pendingó���� Pending�Լ����� �����Ѵ�.
    if ( (isMemTableSpace( sPendingOp->mSpaceID ) == ID_TRUE) ||
         (isVolatileTableSpace( sPendingOp->mSpaceID ) == ID_TRUE) )
    {
        sState = 0;
        unlockSpaceNode( sSpaceNode );

        IDE_CONT( CONT_RUN_PENDING );
    }
    else
    {
        IDE_ERROR( isDiskTableSpace(sPendingOp->mSpaceID )  == ID_TRUE );
    }

    // ���ʿ��� DIFF�� �߻���Ű�� �ʰ� �ϱ� ���� indending���� ����
    // �Ʒ� �ڵ�� ��� Disk Tablespace�� Pending�Լ��� �Űܰ� �������
    switch( sPendingOp->mPendingOpType )
    {
        case SCT_POP_CREATE_TBS:
            IDE_ASSERT( SMI_TBS_IS_ONLINE(sSpaceNode->mState) );
            IDE_ASSERT( SMI_TBS_IS_CREATING(sSpaceNode->mState) );

            sSpaceNode->mState &= ~SMI_TBS_CREATING;
            break;

        case SCT_POP_DROP_TBS:
            // lock -> sync lock ��û�ϴ� ����
            // sync lock -> lock �ϴ� ������ �������� �ʵ��� �Ͽ�
            // �������°� �߻����� �ʵ��� �Ѵ�.

            // sync�� �������϶����� ����ϴٰ� lock�� ȹ��ȴ�.
            // sync lock�� ȹ���ϸ� ��� DBF Node�� ���ϵ� sync��
            // ����� �� ����,
            // sync lock�� ����Ѵ�.
            // ������ �����ϰ� sync �ϴ� ������ sync lock��
            // ȹ���ϰ� �Ǹ�
            // DROPPED �����̱⶧���� �������� �ʴ´�.

            // ONLINE/OFFLINE/DISCARD ���¿��� �Ѵ�.
            IDE_ASSERT( hasState( sSpaceNode,
                                  SCT_SS_HAS_DROP_TABLESPACE ) == ID_TRUE );
            IDE_ASSERT( SMI_TBS_IS_DROPPING(sSpaceNode->mState) );

            sSpaceNode->mState = SMI_TBS_DROPPED;

            break;

        case SCT_POP_ALTER_TBS_ONLINE:
            sddTableSpace::setOnlineTBSLSN4Idx( (sddTableSpaceNode*)sSpaceNode,
                                                &(sPendingOp->mOnlineTBSLSN) );
            break;

        case SCT_POP_ALTER_TBS_OFFLINE:
            IDE_ASSERT( (sSpaceNode->mState & SMI_TBS_DROPPING)
                        != SMI_TBS_DROPPING );
            // do nothing.  Pending �Լ����� ��� ó�� �ǽ�.
            break;

        case SCT_POP_ALTER_DBF_RESIZE:
            sddTableSpace::getDataFileNodeByIDWithoutException(
                                                (sddTableSpaceNode*)sSpaceNode,
                                                sPendingOp->mFileID,
                                                &sFileNode );

            if ( sFileNode != NULL )
            {
                // Commit Pending ���࿡�� �������ش�.
                sFileNode->mState &= ~SMI_FILE_RESIZING;

                // RUNTIME�� Commit ���꿡 ���ؼ��� ������ ���ҽ�Ų��
                if ( ( smLayerCallback::isRestart() == ID_FALSE ) &&
                     ( aIsCommit == ID_TRUE ) )
                {
                    ((sddTableSpaceNode*)sSpaceNode)->mTotalPageCount
                        += sPendingOp->mResizePageSize;
                }
            }
            else
            {
                sDoPending = ID_FALSE;
            }
            break;

        case SCT_POP_CREATE_DBF:
            sddTableSpace::getDataFileNodeByIDWithoutException(
                                                    (sddTableSpaceNode*)sSpaceNode,
                                                    sPendingOp->mFileID,
                                                    &sFileNode );

            if ( sFileNode != NULL )
            {
                IDE_ASSERT( SMI_FILE_STATE_IS_ONLINE( sFileNode->mState ) );
                IDE_ASSERT( SMI_FILE_STATE_IS_CREATING( sFileNode->mState ) );

                // create tablespace�� ���� ������ dbf�� SMI_FILE_CREATING���°� �ƴϸ�,
                // add datafile�� ���� ������ dbf�� SMI_FILE_CREATING�̿��� �Ѵ�.

                sFileNode->mState &= ~SMI_FILE_CREATING;

                // RUNTIME�� Commit ���꿡 ���ؼ��� ������ ���ҽ�Ų��
                if ( ( smLayerCallback::isRestart() == ID_FALSE ) &&
                     ( aIsCommit == ID_TRUE ) )
                {
                    ((sddTableSpaceNode*)sSpaceNode)->mDataFileCount++;
                    ((sddTableSpaceNode*)sSpaceNode)->mTotalPageCount += sFileNode->mCurrSize;
                }
            }
            else
            {
                sDoPending = ID_FALSE;
            }
            break;

        case SCT_POP_DROP_DBF:
            sddTableSpace::getDataFileNodeByIDWithoutException(
                                                    (sddTableSpaceNode*)sSpaceNode,
                                                    sPendingOp->mFileID,
                                                    &sFileNode );

            if ( sFileNode != NULL )
            {
                IDE_ASSERT( SMI_FILE_STATE_IS_ONLINE( sFileNode->mState ) ||
                            SMI_FILE_STATE_IS_OFFLINE( sFileNode->mState ) );

                IDE_ASSERT( SMI_FILE_STATE_IS_DROPPING( sFileNode->mState ) );

                // FIX BUG-13125 DROP TABLESPACE�ÿ� ���� ����������
                // Invalid���Ѿ� �Ѵ�.
                // DBF ���º����� removeFilePending���� ó���Ѵ�.

                IDE_ASSERT( sPendingOp->mPendingOpFunc != NULL );

                IDE_ASSERT( (UChar*)(sPendingOp->mPendingOpParam)
                            == (UChar*)sFileNode );

                // RUNTIME�� Commit ���꿡 ���ؼ��� ������ ���ҽ�Ų��
                if ( ( smLayerCallback::isRestart() == ID_FALSE ) &&
                     ( aIsCommit == ID_TRUE ) )
                {
                    ((sddTableSpaceNode*)sSpaceNode)->mDataFileCount--;
                    ((sddTableSpaceNode*)sSpaceNode)->mTotalPageCount -= sFileNode->mCurrSize;
                }
                else
                {
                    // Restart Recovery ������ Pending ������.
                }
            }
            else
            {
                sDoPending = ID_FALSE;
            }
            break;

        case SCT_POP_ALTER_DBF_ONLINE:
            sddTableSpace::getDataFileNodeByIDWithoutException(
                                                (sddTableSpaceNode*)sSpaceNode,
                                                sPendingOp->mFileID,
                                                &sFileNode );

            if ( sFileNode != NULL )
            {
                /* BUG-21056: Restart Redo�� DBF�� Online���� ����� FileNode��
                 * Online���� �Ǿ����� �� �ֽ��ϴ�.
                 *
                 * ����: Restart Redo Point�� DBF Online�������� �������� �� �ֱ�
                 * ����. */
                if ( smLayerCallback::isRestart() == ID_FALSE )
                {
                    IDE_ASSERT( SMI_FILE_STATE_IS_OFFLINE( sFileNode->mState ) );
                }

                IDE_ASSERT( sPendingOp->mPendingOpFunc == NULL );

                sFileNode->mState = sPendingOp->mNewDBFState;
            }
            else
            {
                sDoPending = ID_FALSE;
            }
            break;

        case SCT_POP_ALTER_DBF_OFFLINE:

            sddTableSpace::getDataFileNodeByIDWithoutException(
                                                (sddTableSpaceNode*)sSpaceNode,
                                                sPendingOp->mFileID,
                                                &sFileNode );

            if ( sFileNode != NULL )
            {
                IDE_ASSERT( sPendingOp->mPendingOpFunc == NULL );

                sFileNode->mState = sPendingOp->mNewDBFState;
            }
            else
            {
                sDoPending = ID_FALSE;
            }
            break;

        case SCT_POP_UPDATE_SPACECACHE:
            IDE_ASSERT( sPendingOp->mPendingOpFunc  != NULL );
            IDE_ASSERT( sPendingOp->mPendingOpParam != NULL );
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    sState = 0;
    unlockSpaceNode( sSpaceNode );

    // Loganchor ����
    switch( sPendingOp->mPendingOpType )
    {
        case SCT_POP_CREATE_TBS:
        case SCT_POP_DROP_TBS:
            /* PROJ-2386 DR
             * DR standby�� active�� ������ ������ loganchor�� update�Ѵ�. */
            if ( ((sddTableSpaceNode*)sSpaceNode)->mAnchorOffset
                 != SCT_UNSAVED_ATTRIBUTE_OFFSET )
            {
                IDE_ASSERT( smLayerCallback::updateTBSNodeAndFlush( sSpaceNode ) == IDE_SUCCESS );
            }
            else
            {
                // create TBS �����ϴ� ������ ���
                // ������ �ȵ� ��찡 �ִ�.
                // ��, Create TBS�� Undo �������� ��ϵ�
                // Pending(DROP_TBS)�� ���ؼ� ���������� ����
                // TBS Node �Ӽ��� update�Ϸ��� Loganchor��
                // ������ �� �ִ�.
                IDE_ASSERT( (sPendingOp->mPendingOpType == SCT_POP_DROP_TBS) &&
                            (aIsCommit == ID_FALSE) );
            }
            break;

        case SCT_POP_DROP_DBF:
            // pending ������ loganchor flush
            break;
        case SCT_POP_CREATE_DBF:
        case SCT_POP_ALTER_DBF_RESIZE:
        case SCT_POP_ALTER_DBF_OFFLINE:
        case SCT_POP_ALTER_DBF_ONLINE:
            if ( sFileNode != NULL )
            {
                /* BUG-24086: [SD] Restart�ÿ��� File�̳� TBS�� ���� ���°� �ٲ���� ���
                 * LogAnchor�� ���¸� �ݿ��ؾ� �Ѵ�.
                 *
                 * Restart Recovery�ÿ��� updateDBFNodeAndFlush���� �ʴ����� �ϵ��� ����.
                 * */
                if ( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET )
                {
                    IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode ) == IDE_SUCCESS );
                }
                else
                {
                    // create DBF �����ϴ� ������ ���
                    // ������ �ȵ� ��찡 �ִ�.
                    // ��, Create DBF�� Undo �������� ��ϵ�
                    // Pending(DROP_DBF)�� ���ؼ� ���������� ����
                    // DBF Node �Ӽ��� update�Ϸ��� Loganchor��
                    // ������ �� �ִ�.
                    IDE_ASSERT( aIsCommit == ID_FALSE );
                }
            }
            else
            {
                // FileNode�� �˻��� �ȵ� ��� Nothing To Do...
                IDE_ASSERT( sDoPending == ID_FALSE );
            }
            break;

        case SCT_POP_ALTER_TBS_ONLINE:
        case SCT_POP_ALTER_TBS_OFFLINE:
        case SCT_POP_UPDATE_SPACECACHE:
            // do nothing.  Pending �Լ����� ��� ó�� �ǽ�.
            break;

        default:
            IDE_ASSERT(0);
            break;

    }

    // ���ʿ��� DIFF�� �߻���Ű�� �ʰ� �ϱ� ���� indending���� ����

    IDE_EXCEPTION_CONT( CONT_RUN_PENDING );

    IDU_FIT_POINT( "2.PROJ-1548@sctTableSpaceMgr::executePendingOperation" );

    if ( ( sDoPending == ID_TRUE ) &&
         ( sPendingOp->mPendingOpFunc != NULL ) )
    {
        // ��ϵǾ� �ִ� Pending Operation�� �����Ѵ�.
        IDE_TEST( (*sPendingOp->mPendingOpFunc) ( aStatistics,
                                                  sSpaceNode,
                                                  sPendingOp )
                  != IDE_SUCCESS );
    }
    else
    {
        // ��ϵ� Pending Operation�� NULL �ΰ��
    }

    IDE_EXCEPTION_CONT( CONT_SKIP_PENDING );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/*
    ������ Tablespace�� ���� Ư�� Action�� �����Ѵ�.

    aAction    [IN] ������ Action�Լ�
    aActionArg [IN] Action�Լ��� ���� Argument
    aFilter    [IN] Action���� ���θ� �����ϴ� Filter

    - ��뿹��
      smmManager::restoreTBS
 */
IDE_RC sctTableSpaceMgr::doAction4EachTBS( idvSQL            * aStatistics,
                                           sctAction4TBS       aAction,
                                           void              * aActionArg,
                                           sctActionExecMode   aActionExecMode )
{
    UInt                sState = 0;
    UInt                sSpaceID;
    sctTableSpaceNode * sCurTBS;

    for( sSpaceID = 0 ; sSpaceID < mNewTableSpaceID ; sSpaceID++ )
    {
        sCurTBS = mSpaceNodeArray[sSpaceID];

        if ( sCurTBS != NULL )
        {
            if ( aActionExecMode == SCT_ACT_MODE_LATCH )
            {
                lockSpaceNode( aStatistics,
                               sCurTBS );
                sState = 1;
            }

            if ( SMI_TBS_IS_NOT_DROPPED( sCurTBS->mState ) )
            {
                IDE_TEST( (*aAction)( aStatistics,
                                      sCurTBS,
                                      aActionArg )
                          != IDE_SUCCESS );
            }
            
            if ( sState == 1 )
            {
                sState = 0;
                unlockSpaceNode( sCurTBS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        unlockSpaceNode( sCurTBS );
    }

    return IDE_FAILURE;
}

/*
 * BUG-34187 ������ ȯ�濡�� �����ÿ� �������ø� ȥ���ؼ� ��� �Ұ��� �մϴ�.
 */
#if defined(VC_WIN32)
void sctTableSpaceMgr::adjustFileSeparator( SChar * aPath )

{
    UInt  i;
    for ( i = 0 ; (aPath[i] != '\0') && (i < SMI_MAX_CHKPT_PATH_NAME_LEN) ; i++ )
    {
        if ( aPath[i] == '/' )
        {
            aPath[i] = IDL_FILE_SEPARATOR;
        }
        else
        {
            /* nothing to do */
        }
    }
}
#endif

/*
 * ��� datafile path �� validataion Ȯ�� �� ������ ����
 *
 * ����� �Է½� default db dir�� path�� �߰��Ͽ� �����θ� �����.
 * ����, datafile path�� ��ȿ���� �˻��ϰ�
 * datafile ���� �ý��ۿ������ "system", "temp", "undo"�� �����ϸ� �ȵȴ�.
 * sdsFile�� ���� ����� �Լ��� ���� �մϴ�.  ��������߻��� ���� �ʿ��մϴ�.
 *
 * + 2nd. code design
 * - filename�� Ư�����ڰ� �����ϴ��� �˻��Ѵ�.
 * - �ý��� ���� datafile �̸����� ����ϴ��� �˻��Ѵ�.
 *   for( system keyword ������ŭ )
 *   {
 *      if( ȭ�� �̸��� prefix�� ���ǵ� ���̴�)
 *      {
 *          return falure;
 *      }
 *   }
 * - ����ζ�� �����η� �����Ͽ� �����Ѵ�.
 * - �����ο� ���ؼ� file���� ������ dir�� �����ϴ��� Ȯ���Ѵ�.
 *
 * [IN]     aCheckPerm   : ���� ���� �˻� ����
 * [IN/OUT] aValidName   : ����θ� �޾Ƽ� �����η� �����Ͽ� ��ȯ
 * [OUT]    aNameLength  : �������� ����
 * [IN]     aTBSLocation : ���̺� �����̽��� ����[SMI_TBS_MEMORY | SMI_TBS_DISK]
 */
IDE_RC sctTableSpaceMgr::makeValidABSPath( idBool         aCheckPerm,
                                           SChar*         aValidName,
                                           UInt*          aNameLength,
                                           smiTBSLocation aTBSLocation )
{
#if !defined(VC_WINCE) && !defined(SYMBIAN)
    UInt    i;
    SChar*  sPtr;

    DIR*    sDir;
    UInt    sDirLength;

    SChar   sFilePath[SM_MAX_FILE_NAME + 1];
    SChar   sChkptPath[SMI_MAX_CHKPT_PATH_NAME_LEN + 1];

    SChar*  sPath = NULL;

    IDE_ASSERT( aValidName  != NULL );
    IDE_ASSERT( aNameLength != NULL );
    IDE_DASSERT( idlOS::strlen(aValidName) == *aNameLength );

    // BUG-29812
    // aTBSLocation�� SMI_TBS_MEMORY�� SMI_TBS_DISK�̾�� �Ѵ�.
    IDE_ASSERT( (aTBSLocation == SMI_TBS_MEMORY) ||
                (aTBSLocation == SMI_TBS_DISK) );

    // fix BUG-15502
    IDE_TEST_RAISE( idlOS::strlen(aValidName) == 0,
                    error_filename_is_null_string );

    if ( aTBSLocation == SMI_TBS_MEMORY )
    {
        sPath = sChkptPath;
    }
    else
    {
        sPath = sFilePath;
    }

    /* ------------------------------------------------
     * datafile �̸��� ���� �ý��� ����� �˻�
     * ----------------------------------------------*/
#if defined(VC_WIN32)
    SInt  sIterator;
    for ( sIterator = 0 ; aValidName[sIterator] != '\0' ; sIterator++ ) 
    {
        if ( aValidName[sIterator] == '/' ) 
        {
             aValidName[sIterator] = IDL_FILE_SEPARATOR;
        }
        else
        {
            /* nothing to do */
        }
    }
#endif

    sPtr = idlOS::strrchr(aValidName, IDL_FILE_SEPARATOR);
    if ( sPtr == NULL )
    {
        sPtr = aValidName; // datafile �� ����
    }
    else
    {
        // Do Nothing...
    }

    sPtr = idlOS::strchr(aValidName, IDL_FILE_SEPARATOR);
#ifndef VC_WIN32
    if ( sPtr != &aValidName[0] )
#else
    /* BUG-38278 invalid datafile path at windows server
     * �������� ȯ�濡�� '/' �� '\' �� ���۵Ǵ�
     * ��� �Է��� ������ ó���Ѵ�. */
    IDE_TEST_RAISE( sPtr == &aValidName[0], error_invalid_filepath_abs );

    if ( ( (aValidName[1] == ':') && (sPtr != &aValidName[2]) ) ||
         ( (aValidName[1] != ':') && (sPtr != &aValidName[0]) ) )
#endif
    {
        /* ------------------------------------------------
         * �����(relative-path)�� ���
         * Disk TBS�̸� default disk db dir��
         * Memory TBS�̸� home dir ($ALTIBASE_HOME)��
         * �ٿ��� ������(absolute-path)�� �����.
         * ----------------------------------------------*/
        if ( aTBSLocation == SMI_TBS_MEMORY )
        {
            sDirLength = idlOS::strlen(aValidName) +
                         idlOS::strlen(idp::getHomeDir());

            IDE_TEST_RAISE( (sDirLength + 1) > SMI_MAX_CHKPT_PATH_NAME_LEN,
                            error_too_long_chkptpath );

            idlOS::snprintf( sPath, 
                             SMI_MAX_CHKPT_PATH_NAME_LEN,
                             "%s%c%s",
                             idp::getHomeDir(),
                             IDL_FILE_SEPARATOR,
                             aValidName );
        }
        else if ( aTBSLocation == SMI_TBS_DISK )
        {
            sDirLength = idlOS::strlen(aValidName) +
                         idlOS::strlen(smuProperty::getDefaultDiskDBDir());

            IDE_TEST_RAISE( (sDirLength + 1) > SM_MAX_FILE_NAME,
                            error_too_long_filepath );

            idlOS::snprintf( sPath, 
                             SM_MAX_FILE_NAME,
                             "%s%c%s",
                             smuProperty::getDefaultDiskDBDir(),
                             IDL_FILE_SEPARATOR,
                             aValidName );
        }

#if defined(VC_WIN32)
    for ( sIterator = 0 ; sPath[sIterator] != '\0' ; sIterator++ ) 
    {
        if ( sPath[sIterator] == '/' ) 
        {
             sPath[sIterator] = IDL_FILE_SEPARATOR;
        }
        else
        {
            /* nothing to do */
        }
    }
#endif
        idlOS::strcpy(aValidName, sPath);
        *aNameLength = idlOS::strlen(aValidName);

        sPtr = idlOS::strchr(aValidName, IDL_FILE_SEPARATOR);
#ifndef VC_WIN32
        IDE_TEST_RAISE( sPtr != &aValidName[0], error_invalid_filepath_abs );
#else
        IDE_TEST_RAISE( ((aValidName[1] == ':') && (sPtr != &aValidName[2])) ||
                        ((aValidName[1] != ':') && (sPtr != &aValidName[0])), 
                        error_invalid_filepath_abs );
#endif
    }

    /* ------------------------------------------------
     * ������, ���� + '/'�� ����ϰ� �׿� ���ڴ� ������� �ʴ´�.
     * (��������)
     * ----------------------------------------------*/
    for ( i = 0 ; i < *aNameLength ; i++ )
    {
        if ( smuUtility::isAlNum(aValidName[i]) != ID_TRUE )
        {
            /* BUG-16283: Windows���� Altibase Home�� '(', ')' �� ��
               ��� DB ������ ������ �߻��մϴ�. */
            IDE_TEST_RAISE( (aValidName[i] != IDL_FILE_SEPARATOR) &&
                            (aValidName[i] != '-') &&
                            (aValidName[i] != '_') &&
                            (aValidName[i] != '.') &&
                            (aValidName[i] != ':') &&
                            (aValidName[i] != '(') &&
                            (aValidName[i] != ')') &&
                            (aValidName[i] != ' ')
                            ,
                            error_invalid_filepath_keyword );

            if ( aValidName[i] == '.' )
            {
                if ( (i + 1) != *aNameLength )
                {
                    IDE_TEST_RAISE( aValidName[i+1] == '.',
                                    error_invalid_filepath_keyword );
                    IDE_TEST_RAISE( aValidName[i+1] == IDL_FILE_SEPARATOR,
                                    error_invalid_filepath_keyword );
                }
                else
                {
                    /* nothing to do */
                }
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            /* nothing to do */
        }
    } // end of for

    // [BUG-29812] IDL_FILE_SEPARATOR�� �Ѱ��� ���ٸ� �����ΰ� �ƴϴ�.
   IDE_TEST_RAISE( (sPtr = idlOS::strrchr(aValidName, IDL_FILE_SEPARATOR))
                   == NULL,
                   error_invalid_filepath_abs );

    // [BUG-29812] dir�� �����ϴ� Ȯ���Ѵ�.
    if ( (aCheckPerm == ID_TRUE) && (aTBSLocation == SMI_TBS_DISK) )
    {
        idlOS::strncpy( sPath, aValidName, SM_MAX_FILE_NAME );

        sDirLength = sPtr - aValidName;

        sDirLength = ( sDirLength == 0 ) ? 1 : sDirLength;
        sPath[sDirLength] = '\0';

        // fix BUG-19977
        IDE_TEST_RAISE( idf::access(sPath, F_OK) != 0,
                        error_not_exist_path );

        IDE_TEST_RAISE( idf::access(sPath, R_OK) != 0,
                        error_no_read_perm_path );
        IDE_TEST_RAISE( idf::access(sPath, W_OK) != 0,
                        error_no_write_perm_path );
        IDE_TEST_RAISE( idf::access(sPath, X_OK) != 0,
                        error_no_execute_perm_path );

        sDir = idf::opendir(sPath);
        IDE_TEST_RAISE( sDir == NULL, error_open_dir ); /* BUGBUG - ERROR MSG */

        (void)idf::closedir(sDir);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_filename_is_null_string );
    {
        if ( aTBSLocation == SMI_TBS_DISK )
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_FileNameIsNullString));
        }
        else
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_CheckpointPathIsNullString));
        }
    }
    IDE_EXCEPTION( error_not_exist_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sPath));
    }
    IDE_EXCEPTION( error_no_read_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoReadPermFile, sPath));
    }
    IDE_EXCEPTION( error_no_write_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoWritePermFile, sPath));
    }
    IDE_EXCEPTION( error_no_execute_perm_path );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExecutePermFile, sPath));
    }
    IDE_EXCEPTION( error_invalid_filepath_abs );
    {
        if ( aTBSLocation == SMI_TBS_DISK )
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
        }
        else
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidCheckpointPathABS));
        }
    }
    IDE_EXCEPTION( error_invalid_filepath_keyword );
    {
        if ( aTBSLocation == SMI_TBS_DISK )
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathKeyWord));
        }
        else
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidCheckpointPathKeyWord));
        }
    }
    IDE_EXCEPTION( error_open_dir );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION( error_too_long_chkptpath );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TooLongCheckpointPath,
                                sPath,
                                idp::getHomeDir()));
    }
    IDE_EXCEPTION( error_too_long_filepath );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TooLongFilePath,
                                sPath,
                                smuProperty::getDefaultDiskDBDir()));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#else
    // Windows CE������ ������ �����ΰ� C:�� �������� �ʴ´�.
    return IDE_SUCCESS;
#endif
}

/* BUG-38621 
 * - �����θ� ����η� ��ȯ 
 * - makeValidABSPath(4)�� �����ؼ� ����.
 *
 * [IN/OUT] aName        : �����θ� �޾Ƽ� ����η� �����Ͽ� ��ȯ
 * [OUT]    aNameLength  : ������� ����
 * [IN]     aTBSLocation : ���̺� �����̽��� ����[SMI_TBS_MEMORY | SMI_TBS_DISK]
 */
IDE_RC sctTableSpaceMgr::makeRELPath( SChar         * aName,
                                      UInt          * aNameLength,
                                      smiTBSLocation  aTBSLocation )
{
#if !defined(VC_WINCE) && !defined(SYMBIAN)
    SChar   sDefaultPath[SM_MAX_FILE_NAME + 1]          = {0, };
    SChar   sChkptPath[SMI_MAX_CHKPT_PATH_NAME_LEN + 1] = {0, };
    SChar   sFilePath[SM_MAX_FILE_NAME]                 = {0, };
    SChar*  sPath;
    UInt    sPathSize;

    IDE_ASSERT( aName != NULL );
    IDE_ASSERT( (aTBSLocation == SMI_TBS_MEMORY) ||
                (aTBSLocation == SMI_TBS_DISK) );

    IDE_ASSERT( smuProperty::getRELPathInLog() == ID_TRUE );

    IDE_TEST_RAISE( idlOS::strlen( aName ) == 0,
                    error_filename_is_null_string );

    if ( aTBSLocation == SMI_TBS_MEMORY )
    {
        idlOS::strncpy( sDefaultPath,
                        idp::getHomeDir(),
                        ID_SIZEOF( sDefaultPath ) - 1 );
        sPath           = sChkptPath;
        sPathSize       = ID_SIZEOF( sChkptPath );
    }
    else
    {
        idlOS::strncpy( sDefaultPath,
                        (SChar *)smuProperty::getDefaultDiskDBDir(),
                        ID_SIZEOF( sDefaultPath ) - 1 );
        sPath           = sFilePath;
        sPathSize       = ID_SIZEOF( sFilePath );
    }

#if defined(VC_WIN32)
    SInt  sIterator;
    for ( sIterator = 0 ; aName[sIterator] != '\0' ; sIterator++ ) 
    {
        if ( aName[sIterator] == '/' ) 
        {
             aName[sIterator] = IDL_FILE_SEPARATOR;
        }
    }
    for ( sIterator = 0; sDefaultPath[sIterator] != '\0'; sIterator++ ) 
    {
        if ( sDefaultPath[sIterator] == '/' ) 
        {
             sDefaultPath[sIterator] = IDL_FILE_SEPARATOR;
        }
    }
#endif

    IDE_TEST_RAISE ( idlOS::strncmp( aName,
                                     sDefaultPath,
                                     idlOS::strlen( sDefaultPath ) )
                     != 0,
                     error_invalid_filepath_abs );

    if ( idlOS::strlen( aName ) > idlOS::strlen( sDefaultPath ) )
    {
        IDE_TEST_RAISE( *(aName + idlOS::strlen( sDefaultPath ))
                        != IDL_FILE_SEPARATOR,
                        error_invalid_filepath_abs );

        idlOS::snprintf( sPath,
                         sPathSize,
                         "%s",
                         ( aName + idlOS::strlen( sDefaultPath ) + 1 ) );
    }
    else
    {
        if ( idlOS::strlen( aName ) == idlOS::strlen( sDefaultPath ) )
        {
            *sPath = '\0';
        }
        else
        {
            /* cannot access here */
            IDE_ASSERT( 0 );
        }
    }

    idlOS::strcpy( aName, sPath );

    if ( aNameLength != NULL )
    {
        *aNameLength = idlOS::strlen( aName );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_filename_is_null_string );
    {
        if ( aTBSLocation == SMI_TBS_DISK )
        {
            IDE_SET( ideSetErrorCode( smERR_ABORT_FileNameIsNullString ) );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_ABORT_CheckpointPathIsNullString ) );
        }
    }
    IDE_EXCEPTION( error_invalid_filepath_abs );
    {
        if ( aTBSLocation == SMI_TBS_DISK )
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
        }
        else
        {
            IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidCheckpointPathABS));
        }
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
#else
    // Windows CE������ ������ �����ΰ� C:�� �������� �ʴ´�.
    return IDE_SUCCESS;
#endif
}

/*
 [  �޸�/��ũ ���� ]

 ��� ������ ���� ���̺����̽��� ���¸� ������·� �����Ѵ�.

 [ �˰��� ]
 1. Mgr Latch�� ��´�.
 2. �־��� ���̺� �����̽� ID�� table space node �� ã�´�.
 3. ���̺� �����̽� ���¸�  backup���� �����Ѵ�.
 4. ���̺� �����̽� ù��° ����Ÿ ������ ���¸� backup begin���� �����Ѵ�.
 5. ��ũ ���̺����̽��� ��� MIN PI ��带 �߰��Ѵ�.
 5. Mgr Latch�� Ǭ��.

 [ ���� ]
 [IN]  aStatistics : �������
 [IN]  aSpaceNode  : ����� ������ ���̺����̽�
*/
IDE_RC sctTableSpaceMgr::startTableSpaceBackup( idvSQL            * aStatistics,
                                                sctTableSpaceNode * aSpaceNode )
{
    idBool sIsLocked = ID_FALSE;

    IDE_DASSERT( aSpaceNode != NULL );

    // temp table space�� ������� �ʿ䰡 ����.
    IDE_TEST_RAISE( (aSpaceNode->mType == SMI_DISK_SYSTEM_TEMP) ||
                    (aSpaceNode->mType == SMI_DISK_USER_TEMP),
                    error_dont_need_backup_tempTableSpace);
  retry:
    lockSpaceNode( aStatistics,
                   aSpaceNode );
    sIsLocked = ID_TRUE;

    IDE_TEST_RAISE( SMI_TBS_IS_BACKUP( aSpaceNode->mState ),
                    error_already_backup_begin );

    if ( ( aSpaceNode->mState & SMI_TBS_BLOCK_BACKUP ) ==
         SMI_TBS_BLOCK_BACKUP )
    {
        sIsLocked = ID_FALSE;
        unlockSpaceNode( aSpaceNode );

        idlOS::sleep(1);

        goto retry;
    }

    // BACKUP ���� ����
    aSpaceNode->mState |= SMI_TBS_BACKUP;

    sIsLocked = ID_FALSE;
    unlockSpaceNode( aSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_dont_need_backup_tempTableSpace );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_DontNeedBackupTempTBS) );
    }
    IDE_EXCEPTION( error_already_backup_begin );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyBeginBackup,
                                  aSpaceNode->mID ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;
}


/*
  [ �޸�/��ũ ���� ]
  ALTER TABLESPACE END BACKUP.. ���� ����� ���̺����̽��� ������¸�
  �����Ѵ�.

  ��ũ ���̺����̽��� ��� MIN PI ��嵵 �Բ� �����Ѵ�.(���� BUG-15003)
  [IN] aStatistics : �������
  [IN] aSpaceNode  : ������¸� �����ϰ����ϴ� ���̺����̽�

*/
IDE_RC sctTableSpaceMgr::endTableSpaceBackup( idvSQL            * aStatistics,
                                              sctTableSpaceNode * aSpaceNode )
{
    idBool sIsLocked = ID_FALSE;

    lockSpaceNode( aStatistics,
                   aSpaceNode );
    sIsLocked = ID_TRUE;

    // alter tablespace  backup begin A�� �ϰ���,
    // alter tablespace  backup end B�� �ϴ� ��츦 ���������̴�.
    IDE_TEST_RAISE( SMI_TBS_IS_NOT_BACKUP( aSpaceNode->mState ),
                    error_not_begin_backup );

    aSpaceNode->mState &= ~SMI_TBS_BACKUP;

    wakeup4Backup( aSpaceNode );

    sIsLocked = ID_FALSE;
    unlockSpaceNode( aSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_begin_backup);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotBeginBackup,
                                  aSpaceNode->mID ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;
}


/*
   ����Ÿ���� ����� ����� üũ����Ʈ ������ �����Ѵ�.

   [IN] aDiskRedoLSN   : üũ����Ʈ�������� ������ ��ũ Redo LSN
   [IN] aMemRedoLSN : �޸� ���̺����̽��� Redo LSN �迭
*/
void sctTableSpaceMgr::setRedoLSN4DBFileMetaHdr( smLSN* aDiskRedoLSN,
                                                 smLSN* aMemRedoLSN )
{
    if ( aDiskRedoLSN != NULL  )
    {
        // Disk Redo LSN�� �����Ѵ�.
        SM_GET_LSN( mDiskRedoLSN, *aDiskRedoLSN);
    }

    if ( aMemRedoLSN != NULL  )
    {
        IDE_ASSERT( (aMemRedoLSN->mFileNo != ID_UINT_MAX) &&
                    (aMemRedoLSN->mOffset != ID_UINT_MAX) );

        //  �޸� ���̺����̽��� Redo LSN�� �����Ѵ�.
        SM_GET_LSN( mMemRedoLSN, *aMemRedoLSN );
    }
    else
    {
        /* nothing to do ... */
    }

    return;
}

/*
    Alter Tablespace Online/Offline�� ���� ����ó���� �����Ѵ�.

    [IN] aTBSNode  - Alter Online/Offline�Ϸ��� Tablespace
    [IN] aNewTBSState - ���� �����Ϸ��� ���� ( Online or Offline )

    [ �������� ]
      Tablespace�� X Lock�� ���� ���¿��� ȣ��Ǿ�� �Ѵ�.
      ���� : Tablespace�� X Lock�� ���� ���¿���
              �ٸ� DDL�� �Բ� �������� ������ �����ؾ�
              ���� üũ�� ��ȿ�ϴ�.

    [ �˰��� ]
      (e-010) system tablespace �̸� ����
      (e-020) Tablespace�� ���°� �̹� ���ο� �����̸� ����

    [ ���� ]
      DROP/DISCARD/OFFLINE�� Tablespace�� ��� lockTBSNode����
      ����ó���� �����̴�.
 */
IDE_RC sctTableSpaceMgr::checkError4AlterStatus( sctTableSpaceNode  * aTBSNode,
                                                 smiTableSpaceState   aNewTBSState )
{
    IDE_DASSERT( aTBSNode != NULL );

    // ���̺����̽��� ����� ȹ���ϰ� ���¸� ���� ������
    // �ٸ� Ʈ����ǿ� ���� ���̺����̽� ���°� ������� �ʴ´�.
    // latch ȹ���� �ʿ����.

    ///////////////////////////////////////////////////////////////////////////
    // (e-010) system tablespace �̸� ����
    IDE_TEST_RAISE( aTBSNode->mID <= SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP,
                    error_alter_status_of_system_tablespace);

    ///////////////////////////////////////////////////////////////////////////
    // (e-020) Tablespace�� ���°� �̹� ���ο� �����̸� ����
    //         Ex> �̹� ONLINE�����ε� ONLINE���·� �����Ϸ��� �ϸ� ����
    switch( aNewTBSState )
    {
        case SMI_TBS_ONLINE :
            IDE_TEST_RAISE( SMI_TBS_IS_ONLINE(aTBSNode->mState),
                            error_tbs_is_already_online );
            break;

        case SMI_TBS_OFFLINE :
            IDE_TEST_RAISE( SMI_TBS_IS_OFFLINE(aTBSNode->mState),
                            error_tbs_is_already_offline );
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_alter_status_of_system_tablespace);
    {
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_CANNOT_ALTER_STATUS_OF_SYSTEM_TABLESPACE));
    }
    IDE_EXCEPTION(error_tbs_is_already_online);
    {
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_TABLESPACE_IS_ALREADY_ONLINE));
    }
    IDE_EXCEPTION(error_tbs_is_already_offline);
    {
        IDE_SET(ideSetErrorCode(
                    smERR_ABORT_TABLESPACE_IS_ALREADY_OFFLINE));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*  Tablespace�� ���� �������� Backup�� �Ϸ�Ǳ⸦ ��ٸ� ��,
    Tablespace�� ��� �Ұ����� ���·� �����Ѵ�.

    [IN] aTBSNode           - ���¸� ������ Tablespace�� Node
    [IN] aTBSSwitchingState - �������� �÷���
                              (SMI_TBS_SWITCHING_TO_OFFLINE,
                               SMI_TBS_SWITCHING_TO_ONLINE )
 */
IDE_RC sctTableSpaceMgr::wait4BackupAndBlockBackup( idvSQL            * aStatistics,
                                                    sctTableSpaceNode * aTBSNode,
                                                    smiTableSpaceState  aTBSSwitchingState )
{
    idBool sIsLocked = ID_FALSE;

    IDE_DASSERT( aTBSNode != NULL );

    lockSpaceNode( aStatistics,
                   aTBSNode );
    sIsLocked = ID_TRUE;

    while ( SMI_TBS_IS_BACKUP( aTBSNode->mState ) )
    {
        wait4Backup( aTBSNode );

        IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
    }

    // Backup�� ���������� ����
    // Backup�� ���� �� ������ Tablespace�� ���¸� ����

    // ���� Oring�Ͽ� Add�� ���´� Tablespace Backup��
    // Blocking�ϴ� ��Ʈ�� �����־�� ��
    IDE_ASSERT( ( aTBSSwitchingState & SMI_TBS_BLOCK_BACKUP )
                == SMI_TBS_BLOCK_BACKUP );

    aTBSNode->mState |= aTBSSwitchingState ;

    sIsLocked = ID_FALSE;
    unlockSpaceNode( aTBSNode );

    // ��Ȳ ����

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        unlockSpaceNode( aTBSNode );
    }

    return IDE_FAILURE;
}

/*  Table DDL(Create, Drop, Alter )�� �����ϴ� Transaction�� Commit�Ҷ�
    tablespace�� mMaxTblDDLCommitSCN���� �����Ѵ�.

    [IN] aSpaceID      - ���¸� ������ Tablespace�� Node
    [IN] aCommitSCN    - Table�� DDL�� �����ϴ� Transaction��
                         CommitSCN
 */
void sctTableSpaceMgr::updateTblDDLCommitSCN( scSpaceID aSpaceID,
                                              smSCN     aCommitSCN)
{
    sctTableSpaceNode *sSpaceNode;

    IDE_ASSERT( findAndLockSpaceNodeBySpaceID( NULL,
                                               aSpaceID,
                                               (void**)&sSpaceNode )
                == IDE_SUCCESS );

    /* ������ ������ SCN�� ũ�ٸ� �׷��� �д�. �׻� CrtTblCommitSCN��
     * ������ �Ѵ�. */
    if ( SM_SCN_IS_LT( &( sSpaceNode->mMaxTblDDLCommitSCN ),
                       &(aCommitSCN) ) )
    {
        SM_SET_SCN( &( sSpaceNode->mMaxTblDDLCommitSCN ),
                    &aCommitSCN );
    }

    unlockSpaceNode( sSpaceNode );
}

/*  TableSpace�� ���ؼ� Drop Tablespace�� �����ϴ� Transaction��
    �ڽ��� ViewSCN�� ���� LockTable�� �ϴ� ���̿� �ٸ� Transaction��
    �� Tablespace�� ���ؼ� CreateTable, Drop Table�� �ߴ�����
    �˻��ؾ��Ѵ�.

    [IN] aSpaceID - ���¸� ������ Tablespace�� Node
    [IN] aViewSCN - Tablespace�� ���ؼ� DDL�� �����ϴ� Transaction��
                    ViewSCN
 */
IDE_RC sctTableSpaceMgr::canDropByViewSCN( scSpaceID aSpaceID,
                                           smSCN     aViewSCN )
{
    sctTableSpaceNode *sSpaceNode;

    IDE_ASSERT( findSpaceNodeBySpaceID( aSpaceID ,
                                        (void**)&sSpaceNode )
                == IDE_SUCCESS );

    /* ViewSCN�� MaxTblDDLCommitSCN���� �۴ٴ� ����
       ViewSCN�� ���� Lock Tablespace�� �ϴ� ���̿� �� Tablespace��
       ���ؼ� Create Table, Drop Table, Alter Table�� ���� ������
       �߻��� ���̴�. */
    if ( SM_SCN_IS_LT( &( aViewSCN ),
                       &( sSpaceNode->mMaxTblDDLCommitSCN ) ) )
    {
        IDE_RAISE( err_modified );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_modified );
    {
        IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTBSModified ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ��� ��ũ Tablesapce�� Tablespace�� ����
 *               �ִ� ��� Datafile�� Max Open FD Count�� aMaxFDCnt4File
 *               �� �����Ѵ�.
 *
 * aMaxFDCnt4File - [IN] Max FD Count
 **********************************************************************/
IDE_RC sctTableSpaceMgr::setMaxFDCntAllDFileOfAllDiskTBS( UInt aMaxFDCnt4File )
{
    UInt               i;
    sctTableSpaceNode *sSpaceNode;

    for ( i = 0 ; i < mNewTableSpaceID ; i++ )
    {
        sSpaceNode = mSpaceNodeArray[i];

        if ( sSpaceNode == NULL )
        {
            continue;
        }

        if ( isDiskTableSpace( sSpaceNode ) == ID_TRUE )
        {
            /* �� Tablespace���� Max Open FD Count�� �����Ѵ�. */
            IDE_TEST( sddDiskMgr::setMaxFDCnt4AllDFileOfTBS( sSpaceNode,
                                                             aMaxFDCnt4File )
                  != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   PROJ-1548
   DDL_LOCK_TIMEOUT ������Ƽ�� ���� ���ð��� ��ȯ�Ѵ�.
*/
ULong sctTableSpaceMgr::getDDLLockTimeOut( smxTrans * aTrans )
{
    SInt sDDLLockTimeout = 0;
    if ( aTrans != NULL )
    {
        if ( aTrans->mStatistics != NULL )
        {
            IDE_DASSERT(aTrans->mStatistics->mSess != NULL);
            if( aTrans->mStatistics->mSess->mSession != NULL )
            {
                sDDLLockTimeout = gSmiGlobalCallBackList.getDDLLockTimeout(aTrans->mStatistics->mSess->mSession);
            }
        }
    }
    else 
    {
        sDDLLockTimeout = smuProperty::getDDLLockTimeOut();
    }
    return (((sDDLLockTimeout == -1) ?
             ID_ULONG_MAX :
             sDDLLockTimeout *1000000) );
}
