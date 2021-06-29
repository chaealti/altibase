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
* $Id: smmManager.cpp 90522 2021-04-09 01:29:20Z emlee $
**********************************************************************/


/* ------------------------------------------------


   Initialize

   �������
   mDBDir - DB_DIR_COUNT������ array

    -----------
   | dbs0      |
    -----------
   | dbs1      |
    -----------
   | dbs2      |
    -----------
   | dbs3      |
    -----------
   | dbs4      |
    -----------
   | dbs5      |
   ------------
   | dbs6      |
    -----------
   | dbs7      |
    -----------

    mDBFile
    ȭ���� ����� �ִ� �ִ� ������ŭ �̸� databaseFile�� alloc�Ѵ�.
    ó������ ���� �� ������ ���� �� �ִ�.

    --------------
   | mDBFile ptr  |
    --------------
       |
       |
    -----------      -------------
   | pingpong0 | -  | databaeFile |
    -----------      -------------
       |                  |
       |             -------------
       |            | databaeFile |
       |             -------------
       |                  |
       |                  ...
    -----------
   | pingpong1 | - ...
    -----------




 * ----------------------------------------------*/

#include <smiFixedTable.h>
#include <sctTableSpaceMgr.h>
#include <smErrorCode.h>
#include <smc.h>
#include <smDef.h>
#include <smu.h>
#include <smm.h>
#include <smmReq.h>
#include <smpVarPageList.h>
#include <smiMain.h>
#include <svmExpandChunk.h>

/* ------------------------------------------------
 * [] global variable
 * ----------------------------------------------*/

void                   *smmManager::m_catTableHeader = NULL;
void                   *smmManager::m_catTempTableHeader = NULL;
smPCArr                 smmManager::mPCArray[SC_MAX_SPACE_ARRAY_SIZE]; // BUG-48513
smmGetPersPagePtrFunc   smmManager::mGetPersPagePtrFunc = NULL;

smmManager::smmManager()
{
}

/* smmManager�� �ʱ�ȭ �Ѵ�.
 *
 * �����ͺ��̽��� �̹� �����ϴ� ���, �����ͺ��̽��� MemBase�κ���
 * �����ͺ��̽� ������ �о�ͼ� �� ������ ���� smmManager�� �ʱ�ȭ�Ѵ�.
 *
 * ���� �����ͺ��̽��� �������� �ʴ� ���, smmManager�� �ʱ�ȭ��
 * �Ϻθ� �Ǹ�, �������� createDB�ÿ� initializeWithDBInfo�� ȣ���Ͽ�
 * �ʱ�ȭ�ȴ�.
 *
 * aOp [IN] �����ͺ��̽� ����������, �Ϲ� Startup�� ������� ����
 *
 */
IDE_RC smmManager::initializeStatic()
{
    idlOS::memset( mPCArray, 0, ID_SIZEOF(mPCArray) );

    IDE_TEST( smmFixedMemoryMgr::initializeStatic() != IDE_SUCCESS );

    // To Fix BUG-14185
     // Free Page List ������ �ʱ�ȭ
    IDE_TEST( smmFPLManager::initializeStatic( ) != IDE_SUCCESS );

    IDE_TEST( smmDatabase::initialize() != IDE_SUCCESS );

    m_catTableHeader        = NULL;
    m_catTempTableHeader    = NULL;

    IDE_TEST( smLayerCallback::prepareIdxFreePages() != IDE_SUCCESS );

    smpVarPageList::initAllocArray();

    smLayerCallback::setSmmCallbacksInSmx();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
/* DB���� ũ�Ⱑ OS�� ����ũ�� ���ѿ� �ɸ����� �ʴ��� üũ�Ѵ�.
 *
 */
IDE_RC smmManager::checkOSFileSize( vULong aDBFileSize )
{
#if !defined(WRS_VXWORKS)
    struct rlimit  limit;
    /* ------------------------------------------------------------------
     *  [4] Log(DB) File Size �˻�
     * -----------------------------------------------------------------*/
    IDE_TEST_RAISE(idlOS::getrlimit(RLIMIT_FSIZE, &limit) != 0,
                   getrlimit_error);

    IDE_TEST_RAISE( aDBFileSize -1 > limit.rlim_cur ,
                    check_OSFileLimit);


    return IDE_SUCCESS;

    IDE_EXCEPTION(check_OSFileLimit);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_OSFileSizeLimit_ERROR,
                                (ULong) aDBFileSize ));
    }
    IDE_EXCEPTION(getrlimit_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_GETLIMIT_ERROR));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;
#else
    return IDE_SUCCESS;
#endif

}

SInt
smmManager::getCurrentDB(smmTBSNode * aTBSNode)
{
    IDE_DASSERT( (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB == 0) ||
                 (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB == 1) );

    return aTBSNode->mTBSAttr.mMemAttr.mCurrentDB;
}

SInt
smmManager::getNxtStableDB( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB == 0) ||
                 (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB == 1) );

    return (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB + 1) % SM_PINGPONG_COUNT;
}



/* ����ڰ� �����Ϸ��� �����ͺ��̽� ũ�⿡ �����ϴ�
 * �����ͺ��̽� ������ ���� ������ Page ���� ����Ѵ�.
 *
 * ����ڰ� ������ ũ��� ��Ȯ�� ��ġ�ϴ� �����ͺ��̽��� ������ �� ����
 * ������, �ϳ��� �����ͺ��̽��� �׻� Expand Chunkũ���� �����
 * �����Ǳ� �����̴�.
 *
 * aDbSize         [IN] �����Ϸ��� �����ͺ��̽� ũ��
 * aChunkPageCount [IN] �ϳ��� Expand Chunk�� ���ϴ� Page�� ��
 *
 */
ULong smmManager::calculateDbPageCount( ULong aDbSize, ULong aChunkPageCount )
{
    vULong sCalculPageCount;
    vULong sRequestedPageCount;

    // aDbSize �� 0���� ���͵� �׾�� �ȵȴ�.
    // MM�ܿ��� �� �Լ� ȣ���Ŀ� ����ó���ϰ��ֱ� ����
    IDE_DASSERT( aChunkPageCount > 0  );

    sRequestedPageCount = aDbSize  / SM_PAGE_SIZE;

    // Expand Chunk Page���� ����� �ǵ��� ����.
    // BUG-15288 �� Max DB SIZE�� ���� �� ����.
    sCalculPageCount =
        aChunkPageCount * (sRequestedPageCount / aChunkPageCount);

    // 0�� �������� �����ϴ� META PAGE���� ������ �ʴ´�.
    // smmTBSCreate::createTBS���� aInitSize�� Meta Page�� �������� ����
    // ũ�⸦ �޾Ƽ� ���⿡ META PAGE���� ���ϱ� �����̴�.


    return sCalculPageCount ;
}

/*  DB File ��ü��� ���� ������ �������� �ʱ�ȭ�Ѵ�.
 *
 *  [IN] aTBSNode - Tablespace��  Node
 * aChunkPageCount  [IN] �ϳ��� Expand Chunk�� ���ϴ� Page�� ��
 */
IDE_RC smmManager::initDBFileObjects(smmTBSNode *       aTBSNode,
                                     UInt               aDBFilePageCount)
{
    UInt       i;
    UInt       j;
    ULong      sRemain;
    UInt       sChunkPageCount = smuProperty::getExpandChunkPageCount();
    scPageID   sFstPageID = 0;
    scPageID   sLstPageID = 0;
    scPageID   sPageCountPerFile;

    aTBSNode->mDBMaxPageCount =
        calculateDbPageCount( smuProperty::getMaxDBSize(), sChunkPageCount);

    sRemain = aTBSNode->mDBMaxPageCount % aDBFilePageCount;

    aTBSNode->mHighLimitFile = aTBSNode->mDBMaxPageCount / aDBFilePageCount;

    if ( sRemain > 0 )
    {
        aTBSNode->mHighLimitFile += 1;
    }

    for ( i=0 ; i < SMM_PINGPONG_COUNT ; i++ )
    {
        /* smmManager_initDBFileObjects_calloc_DBFile.tc */
        IDU_FIT_POINT("smmManager::initDBFileObjects::calloc::DBFile");
        IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                                   (SInt)aTBSNode->mHighLimitFile,
                                   ID_SIZEOF(smmDatabaseFile *),
                                   (void**)&(aTBSNode->mDBFile[i]))
                 != IDE_SUCCESS);
    }

    // fix BUG-17343 loganchor�� Stable/Unstable Chkpt Image��
    // ���� ���� ������ ����
    /* smmManager_initDBFileObjects_calloc_CrtDBFileInfo.tc */
    IDU_FIT_POINT("smmManager::initDBFileObjects::calloc::CrtDBFileInfo");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SMM,
                    (SInt)aTBSNode->mHighLimitFile,
                    ID_SIZEOF(smmCrtDBFileInfo),
                    (void**)&(aTBSNode->mCrtDBFileInfo))
                  != IDE_SUCCESS );

    initCrtDBFileInfo( aTBSNode );

    // �ִ� ����� �ִ� ȭ�ϱ��� databaseFile��ü�� �����ص�
    for (i = 0; i< aTBSNode->mHighLimitFile; i++)
    {
        for (j = 0; j < SMM_PINGPONG_COUNT; j++)
        {
            /* TC/FIT/Limit/sm/smm/smmManager_initDBFileObjects_malloc.sql */
            IDU_FIT_POINT_RAISE( "smmManager::initDBFileObjects::malloc",
                                  insufficient_memory );

            IDE_TEST_RAISE(iduMemMgr::malloc(
                                    IDU_MEM_SM_SMM,
                                    ID_SIZEOF(smmDatabaseFile),
                                    (void**)&(aTBSNode->mDBFile[ j ][ i ])) != IDE_SUCCESS,
                           insufficient_memory );

            sPageCountPerFile =
                smmManager::getPageCountPerFile( aTBSNode,
                                                 i );
            sLstPageID = sFstPageID + sPageCountPerFile - 1;

            IDE_TEST( createDBFileObject( aTBSNode,
                                          j,   // PINGPONG ��ȣ
                                          i,   // ����Ÿ���̽����Ϲ�ȣ
                                          sFstPageID,
                                          sLstPageID,
                                          (smmDatabaseFile*) aTBSNode->mDBFile[ j ][ i ] )
                      != IDE_SUCCESS );
        }

        // ù��° PageID�� ����Ѵ�.
        sFstPageID += sPageCountPerFile;
    }


    // BUGBUG-1548 Lock���� �ʱ�ȭ�� ��𿡼� ����?

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/**
    DB File ��ü�� �����Ѵ�.

    [IN] aTBSNode     - DB File��ü�� ������ Tablespace Node
    [IN] aPingPongNum - DB File�� ���� ��ȣ
    [IN] aFileNum     - DB File�� ���� ��ȣ
    [IN] aFstPageID   - DB File�� ������ Page ���� (����)
    [IN] aLstPageID   - DB File�� ������ Page ���� (��)
    [IN/OUT] aDBFileMemory - DB File ��ü�� �޸𸮸� �Ѱܹ޾�
                             �ش� �޸𸮿� DB File ��ü�� �ʱ�ȭ�Ѵ�.
 */
IDE_RC smmManager::createDBFileObject( smmTBSNode       * aTBSNode,
                                       UInt               aPingPongNum,
                                       UInt               aFileNum,
                                       scPageID           aFstPageID,
                                       scPageID           aLstPageID,
                                       smmDatabaseFile  * aDBFileObj )
{
    SChar   sDBFileName[SM_MAX_FILE_NAME];
    SChar * sDBFileDir;
    idBool  sFound;


    aDBFileObj = new ( aDBFileObj ) smmDatabaseFile;

    IDE_TEST( aDBFileObj->initialize(
                  aTBSNode->mHeader.mID,
                  aPingPongNum,
                  aFileNum,
                  &aFstPageID,
                  &aLstPageID )
              != IDE_SUCCESS);

    sFound = smmDatabaseFile::findDBFile( aTBSNode,
                                          aPingPongNum,
                                          aFileNum,
                                          (SChar*)sDBFileName,
                                          &sDBFileDir);
    if ( sFound == ID_TRUE )
    {
        // To Fix BUG-17997 [�޸�TBS] DISCARD�� Tablespace��
        //                  DROP�ÿ� Checkpoint Image �������� ����
        //
        // MEDIA�ܰ� �ʱ�ȭ�ÿ� DB File��� �н��� �����Ѵ�.
        aDBFileObj->setFileName(sDBFileName);
        aDBFileObj->setDir(sDBFileDir);
    }
    else
    {
        // Discard�� Tablespace�� ��� DB File�� �������� ���� ���� �ִ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* DB File��ü�� ���� �ڷᱸ���� �����Ѵ�.

   aTBSNode [IN] Tablespace�� Node

   ���� : �� �Լ��� initDBFileObjects���� �ʱ�ȭ�� ������ �ı��Ѵ�.
 */
IDE_RC smmManager::finiDBFileObjects( smmTBSNode * aTBSNode )
{
    UInt   i;
    UInt   j;

    IDE_DASSERT( aTBSNode != NULL );

    // initDBFileObjects�� ȣ��� �������� �˻�.
    // ���� Field���� 0�� �ƴ� ���̾�� �Ѵ�.
    IDE_ASSERT( aTBSNode->mHighLimitFile != 0);
    IDE_ASSERT( aTBSNode->mDBMaxPageCount != 0);


    /* ------------------------------------------------
     *  Close all db file & memory free
     * ----------------------------------------------*/
    for (i = 0; i < aTBSNode->mHighLimitFile; i++)
    {
        for (j = 0; j < SMM_PINGPONG_COUNT; j++)
        {
            IDE_ASSERT( aTBSNode->mDBFile[j][i] != NULL );

            IDE_TEST(
                ((smmDatabaseFile*)aTBSNode->mDBFile[j][i])->destroy()
                != IDE_SUCCESS);


            IDE_TEST(iduMemMgr::free(aTBSNode->mDBFile[j][i])
                     != IDE_SUCCESS);

            aTBSNode->mDBFile[j][i] = NULL ;
        }
    }

    for (i = 0; i < SMM_PINGPONG_COUNT; i++)
    {
        IDE_ASSERT( aTBSNode->mDBFile[i] != NULL);


        IDE_TEST(iduMemMgr::free(aTBSNode->mDBFile[i])
                 != IDE_SUCCESS);

        aTBSNode->mDBFile[i] = NULL;
    }


    IDE_ASSERT( aTBSNode->mCrtDBFileInfo!= NULL );

    IDE_TEST(iduMemMgr::free(aTBSNode->mCrtDBFileInfo)
             != IDE_SUCCESS);

    aTBSNode->mCrtDBFileInfo= NULL;

    // initDBFileObjects���� �ʱ�ȭ�ߴ� Field���� 0���� ����
    aTBSNode->mHighLimitFile = 0;
    aTBSNode->mDBMaxPageCount = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smmManager::destroyStatic()
{

    smLayerCallback::unsetSmmCallbacksInSmx();

    /* ------------------------------------------------
     *  release all index free page list
     * ----------------------------------------------*/
    IDE_TEST( smLayerCallback::releaseIdxFreePages() != IDE_SUCCESS );

    IDE_TEST( smmDatabase::destroy() != IDE_SUCCESS );

    IDE_TEST( smmFPLManager::destroyStatic() != IDE_SUCCESS );

    IDE_TEST( smmFixedMemoryMgr::destroyStatic() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace�� ���� Page���� �ý��� ����

    aTBSNode [IN] �����Ϸ��� Tablespace�� Node

 */
IDE_RC smmManager::finiPageSystem(smmTBSNode * aTBSNode)
{
    IDE_DASSERT( aTBSNode != NULL );

    // To Fix BUG-14185
    // Free Page List ������ ����
    IDE_TEST( smmFPLManager::destroy( aTBSNode ) != IDE_SUCCESS );


    // Expand Chunk ������ ����
    IDE_TEST( smmExpandChunk::destroy( aTBSNode ) != IDE_SUCCESS );

    // BUGBUG-1548
    // Index Memory Pool�� TBS���� ���� �ΰ� Index�޸𸮸� �����ؾ���

    // �ش� Tablespace�� ���� Dirty Page�����ڸ� ����
    IDE_TEST( smmDirtyPageMgr::removeDPMgr( aTBSNode )
                != IDE_SUCCESS );

    // Tablespace�� Page �޸� �ݳ�
    IDE_TEST( freeAllPageMemAndPCH( aTBSNode ) != IDE_SUCCESS );

    // Page Memory Pool ����
    IDE_TEST(destroyPagePool( aTBSNode ) != IDE_SUCCESS );

    // PCH Memory Pool ����
    IDE_TEST(aTBSNode->mPCHMemPool.destroy() != IDE_SUCCESS);

    // PCH Array����
    IDE_TEST( freePCHArray( aTBSNode->mHeader.mID ) != IDE_SUCCESS );
    
    // BUG-19384 : table space offline�� ���ÿ� V$TABLESPACES��
    // ��ȸ�� �� mem base�� null�� �������� �ʾƼ� ������ �����
    IDU_FIT_POINT( "1.BUG-19384@smmManager::finiPageSystem" );

    // BUG-19299 : tablespace�� mem base�� null�� ����
    aTBSNode->mMemBase = NULL;

    // ���� Restore���� ���� ���·� �����Ѵ�
    aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET;

    // TBS�� DROP�Ǿ Lock Item�� �״�� �д�
    // ���� TBS�� Lock�� ��ٸ��� �ִ� Tx���� ���� �� �ֱ� ����
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Tablespace���� ��� Page Memory/PCH Entry�� �����Ѵ�.

    [IN] aTBSNode - Tablespace�� Node
 */
IDE_RC smmManager::freeAllPageMemAndPCH(smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    idBool sFreePageMemory;

    switch( aTBSNode->mRestoreType )
    {
        case SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET :
            // Media Recovery�� ���� PAGE�ܰ���� �ʱ�ȭ �Ͽ�����,
            // prepare/restore���� ���� tablespace.
            //
            // ���� prepare/restore���� �ʾ����Ƿ�
            // ������ page memory�� ����.
            sFreePageMemory = ID_FALSE;
            break;

        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
        case SMM_DB_RESTORE_TYPE_SHM_ATTACH :
            // Drop/Offline�� Pending���� ȣ��� ���
            // ȣ��� ��� �����޸� ��ü�� �����Ѵ�
            if ( sctTableSpaceMgr::hasState(
                     & aTBSNode->mHeader,
                     SCT_SS_FREE_SHM_PAGE_ON_DESTROY )
                 == ID_TRUE )
            {
                // Tablespace�� Page Memory�� �����Ѵ�.
                sFreePageMemory = ID_TRUE;
            }
            else
            {
                // Shutdown�� ONLINE Tablespace�� ���
                sFreePageMemory = ID_FALSE;
            }
            break;

        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            // �Ϲ� �޸��� ��� ������ Page memory�� ����
            sFreePageMemory = ID_TRUE;
            break;
        default :
            IDE_ASSERT(0);
    }

    IDE_TEST(freeAll(aTBSNode, sFreePageMemory)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Tablespace�� ���� ��ü(DB File) ���� �ý��� ����

    aTBSNode [IN] �����Ϸ��� Tablespace�� Node

 */
IDE_RC smmManager::finiMediaSystem(smmTBSNode * aTBSNode)
{
    IDE_DASSERT( aTBSNode != NULL );

    // Tablespace�� ���� prepare/restore���� ���� ���¿����Ѵ�.
    IDE_ASSERT( aTBSNode->mRestoreType
                == SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET );

//    IDE_TEST( freeMetaPage( aTBSNode ) != IDE_SUCCESS );

    IDE_TEST( finiDBFileObjects( aTBSNode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
    Tablespace Node�� �ı��Ѵ�.

    aTBSNode [IN] ������ Tablespace
 */
IDE_RC smmManager::finiMemTBSNode(smmTBSNode * aTBSNode)
{
    IDE_DASSERT(aTBSNode != NULL );

    // Tablespace�� ���� prepare/restore���� ���� ���¿����Ѵ�.
    IDE_ASSERT( aTBSNode->mRestoreType
                == SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET );

    // Disk, Memory Tablespace ���� �ı��� ȣ��
    // - Lock������ ������ TBSNode�� ��� ������ �ı�
    IDE_TEST( sctTableSpaceMgr::destroyTBSNode( & aTBSNode->mHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Tablespace�� ��� Checkpoint Image File���� �����ش�.

    ���� ������ �������� ���� ��� �����ϰ� �Ѿ��.

    [IN] aTBSNode           - Tablespace Node
    [IN] aRemoveImageFiles  - Checkpoint Image File���� ������ �� �� ����
 */


IDE_RC smmManager::closeAndRemoveChkptImages(smmTBSNode * aTBSNode,
                                             idBool       aRemoveImageFiles )
{
    smmDatabaseFile           * sDBFilePtr;
    UInt                        sWhichDB;
    UInt                        i;
    scSpaceID                   sSpaceID;


    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isMemTableSpace( aTBSNode->mHeader.mID ) == ID_TRUE );

    sSpaceID = aTBSNode->mHeader.mID;

    ideLog::log(SM_TRC_LOG_LEVEL_MRECOV,"[TBSID:%d-%s] closeAndRemove.. \n", sSpaceID, aTBSNode->mHeader.mName );


    for (sWhichDB = 0; sWhichDB < SMM_PINGPONG_COUNT; sWhichDB++)
    {
        for (i = 0; i <= aTBSNode->mLstCreatedDBFile; i++)
        {

            IDE_TEST( getDBFile( aTBSNode,
                                 sWhichDB,
                                 i, // DB File No
                                 SMM_GETDBFILEOP_NONE,
                                 &sDBFilePtr )
                      != IDE_SUCCESS );

            IDE_TEST( sDBFilePtr->closeAndRemoveDbFile( sSpaceID,
                                                        aRemoveImageFiles,
                                                        aTBSNode )
                      != IDE_SUCCESS );
        } // for ( i )
    } // for ( sWhichDB )


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Membase�� ����ִ� Meta Page(0��)�� Flush�Ѵ�.

    Create Tablespace�߿� ȣ��ȴ�.
    Meta Page������ Disk�� ������ �־�� Restore DB�� �����ϱ� �����̴�.
    �ڼ��� ������ smmTableSpace::createTableSpace�� (��5)�� ����

    [IN] aTBSNode - 0�� Page�� Flush�� Tablespace Node
    [IN] aWhichDB - 0��, 1�� Checkpoint Image�� ��� Flush����?
 */
IDE_RC smmManager::flushTBSMetaPage(smmTBSNode *   aTBSNode,
                                    UInt           aWhichDB)
{
    smmDatabaseFile * sFirstDBFilePtr ;

    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmManager::getDBFile(
                              aTBSNode,
                              aWhichDB,
                              0, // DB File No
                              SMM_GETDBFILEOP_NONE,
                              &sFirstDBFilePtr )
              != IDE_SUCCESS );

    // �̹� 0��° DB������ �̹� Create�ǰ� Open�� ���¿��� �Ѵ�.
    IDE_ASSERT( sFirstDBFilePtr->isOpen() == ID_TRUE );

    IDE_TEST( sFirstDBFilePtr->writePage( aTBSNode, SMM_MEMBASE_PAGEID )
              != IDE_SUCCESS );

    IDE_TEST( sFirstDBFilePtr->syncUntilSuccess() != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* 0�� Page�� ��ũ�κ��� �о MemBase�κ���
 * ������ ���� �������� �о�´�.
 *
 * aDbFilePageCount [OUT] �ϳ��� �����ͺ��̽� ������ ���ϴ� Page�� ��
 * aChunkPageCount  [OUT] �ϳ��� Expand Chunk�� ���ϴ� Page�� ��
 */
IDE_RC smmManager::readMemBaseInfo(smmTBSNode * aTBSNode,
                                   UInt       * aDbFilePageCount,
                                   UInt       * aChunkPageCount )
{
    IDE_ASSERT( aTBSNode != NULL );
    IDE_ASSERT( aDbFilePageCount != NULL );
    IDE_ASSERT( aChunkPageCount != NULL );

    smmMemBase sMemBase;

    IDE_TEST( smmManager::readMemBaseFromFile( aTBSNode,
                                               &sMemBase )
              != IDE_SUCCESS );

    * aDbFilePageCount = sMemBase.mDBFilePageCount ;
    * aChunkPageCount  = sMemBase.mExpandChunkPageCnt ;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/* 0�� Page�� ��ũ�κ��� �о MemBase�� �����Ѵ�.
 *
 * [IN] aTBSNode - Membase�� �о�� ���̺����̽��� ���
 * [OUT] aMemBase - Disk�κ��� ���� Membase�� ����� �޸𸮰���
 *
 */
IDE_RC smmManager::readMemBaseFromFile(smmTBSNode *   aTBSNode,
                                       smmMemBase *   aMemBase )
{
    IDE_ASSERT( aTBSNode != NULL );
    IDE_ASSERT( aMemBase != NULL );

    SChar           * sBasePage    = NULL;
    SChar           * sBasePagePtr = NULL;
    smmDatabaseFile   sDBFile0;
    SChar             sDBFileName[SM_MAX_FILE_NAME];
    SChar           * sDBFileDir;
    idBool            sFound ;
    UInt              sStage = 0;

    /* BUG-22188: DataFile���� IO����� Disk Sector�� Page Size�� Align��
     *            �����ּ� Buffer�� �Ҵ��ؾ� �մϴ�. */
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SMM,
                                           SM_PAGE_SIZE,
                                           (void**)&sBasePagePtr,
                                           (void**)&sBasePage )
              != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( sDBFile0.initialize(
                       aTBSNode->mHeader.mID,
                       0,      // PingPongNum.
                       0)      // FileNum
            != IDE_SUCCESS );
    sStage = 2;

    sFound =  smmDatabaseFile::findDBFile( aTBSNode,
                                           0, // PING-PONG
                                           0, // DB-File-No
                                           (SChar*)sDBFileName,
                                           &sDBFileDir);
    IDE_TEST_RAISE( sFound != ID_TRUE,
                    file_exist_error );

    sDBFile0.setFileName(sDBFileName);
    sDBFile0.setDir(sDBFileDir);

    IDE_TEST(sDBFile0.open() != IDE_SUCCESS);
    sStage = 3;


    IDE_TEST(sDBFile0.readPage( aTBSNode, SMM_MEMBASE_PAGEID, (UChar*)sBasePage )
             != IDE_SUCCESS);

    idlOS::memcpy( aMemBase,
                   ( smmMemBase * )((UChar*)sBasePage + SMM_MEMBASE_OFFSET ),
                   ID_SIZEOF(*aMemBase));

    sStage = 2;
    IDE_TEST(sDBFile0.close() != IDE_SUCCESS);

    sStage = 1;
    IDE_TEST(sDBFile0.destroy() != IDE_SUCCESS);

    sStage = 0;
    IDE_TEST( iduMemMgr::free( sBasePagePtr ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION(file_exist_error);
    {
        SChar sErrorFile[SM_MAX_FILE_NAME];

        idlOS::snprintf((SChar*)sErrorFile, SM_MAX_FILE_NAME, "%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                aTBSNode->mTBSAttr.mName,
                0,
                0);

        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistFile,
                                (SChar*)sErrorFile));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sStage)
    {
        case 3:
            IDE_ASSERT( sDBFile0.close() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sDBFile0.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( sBasePagePtr ) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}



/*
 * ������ ���̽��� Base Page (Page#0)�� ����Ű�� �����͵��� �����Ѵ�.
 * �� �Լ��� createdb�ÿ��� �Ҹ� �� ������,
 * aBasePage�� �����Ͱ� �ʱ�ȭ�Ǿ� ���� ���� ���� �ִ�.
 *
 * aBasePage     [IN] Base Page�� �ּ�
 *
 */
IDE_RC smmManager::setupCatalogPointers( smmTBSNode * aTBSNode,
                                         UChar *      aBasePage )
{
    IDE_DASSERT( aBasePage != NULL );
    IDE_DASSERT( aTBSNode != NULL );
    IDE_ASSERT( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC );

    smmDatabase::setDicMemBase((smmMemBase *)(aBasePage + SMM_MEMBASE_OFFSET));

    // table�� ���� ���� catalog table header
    m_catTableHeader = (void *)
                       ( aBasePage +
                         SMM_CAT_TABLE_OFFSET +
                         smLayerCallback::getSlotSize() );

    // temp table�� ���� ���� catalog table header
    m_catTempTableHeader = (void *)
                           ( aBasePage +
                             smLayerCallback::getCatTempTableOffset() +
                             smLayerCallback::getSlotSize() );


    return IDE_SUCCESS;
}

/*     
 * BUG-34530 
 * SYS_TBS_MEM_DIC���̺����̽� �޸𸮰� �����Ǵ���
 * DicMemBase�����Ͱ� NULL�� �ʱ�ȭ ���� �ʽ��ϴ�.
 *
 * Media recovery�� ���� �Ϸ�� memory tablespace������
 * DicMemBase�� catalog table header�� NULL�� �ʱ�ȭ�� 
 */
void smmManager::clearCatalogPointers()
{
    smmDatabase::setDicMemBase((smmMemBase *)NULL);

    // table�� ���� catalog table header
    m_catTableHeader = (void *)NULL;

    // temp table�� ���� catalog table header
    m_catTempTableHeader = (void *)NULL;
}

/*
 * ������ ���̽��� Base Page (Page#0)�� ����Ű�� �����͵��� �����Ѵ�.
 * �� �Լ��� createdb�ÿ��� �Ҹ� �� ������,
 * aBasePage�� �����Ͱ� �ʱ�ȭ�Ǿ� ���� ���� ���� �ִ�.
 *
 * aBasePage     [IN] Base Page�� �ּ�
 *
 */
IDE_RC smmManager::setupMemBasePointer( smmTBSNode * aTBSNode,
                                        UChar *      aBasePage )
{
    IDE_DASSERT( aBasePage != NULL );

    aTBSNode->mMemBase        = (smmMemBase *)(aBasePage + SMM_MEMBASE_OFFSET);

    return IDE_SUCCESS;
}


/*
 * ������ ���̽��� Base Page (Page#0) �� ������ ������ �����Ѵ�.
 * �̿� ���õ� �����δ�  MemBase�� Catalog Table������ �ִ�.
 *
 * aBasePage     [IN] Base Page�� �ּ�
 *
 */
IDE_RC smmManager::setupBasePageInfo( smmTBSNode * aTBSNode,
                                      UChar *      aBasePage )
{
    IDE_DASSERT( aBasePage != NULL );

    if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
    {
        setupCatalogPointers( aTBSNode, aBasePage );
    }
    setupMemBasePointer( aTBSNode, aBasePage );

    IDE_TEST(smmDatabase::checkVersion(aTBSNode->mMemBase) != IDE_SUCCESS);

    IDE_TEST_RAISE(smmDatabase::getAllocPersPageCount(aTBSNode->mMemBase) >
                   aTBSNode->mDBMaxPageCount,
                   page_size_overflow_error);


    // Expand Chunk ������ �ʱ�ȭ
    IDE_TEST( smmExpandChunk::setChunkPageCnt(aTBSNode,
                  smmDatabase::getExpandChunkPageCnt( aTBSNode->mMemBase ) )
              != IDE_SUCCESS );

    // Expand Chunk�� ���õ� Property �� üũ
    IDE_TEST( smmDatabase::checkExpandChunkProps(aTBSNode->mMemBase)
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION(page_size_overflow_error);
    {
        IDE_SET(ideSetErrorCode(
                    smERR_FATAL_Overflow_DB_Size,
                    (ULong)aTBSNode->mDBMaxPageCount,
                    (vULong)smmDatabase::getAllocPersPageCount(aTBSNode->mMemBase)));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Tablespace�� �ʱ� ������ ������ �ش��ϴ�
 *               Database File���� �����Ѵ�.
 *
 *               Create Tablespace�ÿ��� ȣ��ȴ�.
 *
 *               �� Data File���� ������ Logging�Ͽ�
 *               Create Tablespace�� Undo�� �ش� File�� ���쵵�� �Ѵ�.
 *
 *               0�� DB File�� DB File Header + Membase Page + Database Page.
 *               0���̿� DB File�� DB File Header + Database Page.
 *               ���� �����Ǵ� DB File�� �ִ� ũ���
 *               0�� DB File: aDBPageCnt+2 Page
 *               0���̿� DB File: aDBPageCnt+1 Page �̴�.
 *
 * [IN] aTrans - Create Tablespace�� �����ϴ� Transaction
 * [IN] aTBSNode - �������� Tablespace�� Node
 * [IN] aTBSName - ������ Tablespace�� �̸����μ�
 *                  DB File������ DB File Name���� ����Ѵ�.
 * [IN] aDBPageCnt - Tablespace�� �ʱ� ������ ����.
 *                   Membase�� ��ϵǴ� Meta Page ���� �����Ѵ�.
 **********************************************************************/
IDE_RC smmManager::createDBFile( void       * aTrans,
                                 smmTBSNode * aTBSNode,
                                 SChar      * aTBSName,
                                 vULong       aDBPageCnt,
                                 idBool       aIsNeedLogging )

{
    SChar    sCreateDBDir[SM_MAX_FILE_NAME];
    UInt     i;
    UInt     sDBFileNo = 0;
    UInt     sPageCnt;
    vULong   sDBFileSize;
    scPageID sPageCountPerFile;
    smmDatabaseFile * sDBFile ;

    IDE_DASSERT( aTBSName != NULL );
    IDE_DASSERT( aTBSName[0] != '\0' );

    IDE_TEST( smmDatabaseFile::makeDBDirForCreate( aTBSNode,
                                                   sDBFileNo,
                                                   (SChar*)sCreateDBDir )
              != IDE_SUCCESS );

    // BUG-29607 Create Tablespace����, ���� �̸��� File���� CP Path��ο�
    //           �����ϴ����� ������ ������ File�� Name���� �����ؼ� �˻��Ѵ�.
    IDE_TEST( smmDatabaseFile::chkExistDBFileByNode( aTBSNode )
              != IDE_SUCCESS );

    for ( i = 0; i < SMM_PINGPONG_COUNT; i++ )
    {
        sPageCnt = aDBPageCnt;
        sDBFileNo = 0;

        while(sPageCnt != 0)
        {
            sDBFile = (smmDatabaseFile*) aTBSNode->mDBFile[i][sDBFileNo];

            IDE_TEST(sDBFile->setFileName( sCreateDBDir,
                                           aTBSName,
                                           i,
                                           sDBFileNo) != IDE_SUCCESS);

            IDE_TEST_RAISE( sDBFile->exist() == ID_TRUE,  exist_file_error );

            // �� ���Ͽ� ����� �� �ִ� Page�� ��
            // 0�� ������ ��� membase�� ��ϵǴ� Metapage������ ���Ե� Page��
            sPageCountPerFile = smmManager::getPageCountPerFile( aTBSNode,
                                                                 sDBFileNo );


            // ������ ������ �ƴ� ���
            if( sPageCnt > sPageCountPerFile )
            {
                sDBFileSize = sPageCountPerFile * SM_PAGE_SIZE;
                sPageCnt   -= sPageCountPerFile;
            }
            else
            {
                /* ������ DB���� ���� */
                sDBFileSize = sPageCnt * SM_PAGE_SIZE;
                sPageCnt    = 0;
            }

            /* DB File Header�� ũ�⸸ŭ �����ش�*/
            sDBFileSize += SM_DBFILE_METAHDR_PAGE_SIZE;

            if( aIsNeedLogging == ID_TRUE )
            {
                // Create DB File�� ���� �αױ��
                // - redo�� : do nothing
                // - undo�� : DB File Close & remove
                IDE_TEST( smLayerCallback::writeMemoryDBFileCreate( NULL, /* idvSQL* */
                                                                    aTrans,
                                                                    aTBSNode->mHeader.mID,
                                                                    i,
                                                                    sDBFileNo )
                          != IDE_SUCCESS );
            }
            else
            {
                /* do nothign */
            }

            IDE_TEST( sDBFile->createDbFile( aTBSNode,
                                             i,
                                             sDBFileNo,
                                             sDBFileSize) != IDE_SUCCESS);

            // create tablespace �� ���� ������ �����Ǵ� ��쿡��
            // mLstCreatedDBFile�� ������ش�.
            if ( sDBFileNo > aTBSNode->mLstCreatedDBFile )
            {
                aTBSNode->mLstCreatedDBFile = sDBFileNo;
            }

            sDBFileNo++;
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION(exist_file_error);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AlreadyExistFile,
                                  sDBFile->getFileName() ));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/* --------------------------------------------------------------------------
 *   SECTION : Create & Load DB
 * -------------------------------------------------------------------------*/
/*
 * �����޸� Ǯ�� �ʱ�ȭ�Ѵ�.
 *
 * aTBSNode [IN] �����޸� Ǯ�� �ʱ�ȭ�� ���̺� �����̽� ���
 */
IDE_RC smmManager::initializeShmMemPool( smmTBSNode *  aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST( smmFixedMemoryMgr::initialize(aTBSNode) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;

}

/******************************************************************************
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 *****************************************************************************/
IDE_RC smmManager::createTBSPages4Redo( smmTBSNode   * aTBSNode,
                                        void         * aTrans,
                                        SChar        * aDBName,
                                        UInt           aDBFilePageCount,
                                        UInt           aCreatePageCount,
                                        SChar        * aDBCharSet,
                                        SChar        * aNationalCharSet )
{
    UInt        sState          = 0;
    scPageID    sTotalPageCount;
    vULong      sNewChunks;
    UInt        sChunkPageCount = smuProperty::getExpandChunkPageCount();

    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aDBName    != NULL );
    IDE_DASSERT( aDBFilePageCount   > 0 );
    IDE_DASSERT( aCreatePageCount   > 0 );

    // PROJ-1579 NCHAR
    IDE_DASSERT( aDBCharSet         != NULL );
    IDE_DASSERT( aNationalCharSet   != NULL );

    // �����ͺ��̽� ������ ũ�Ⱑ expand chunkũ���� ������� Ȯ��.
    IDE_ASSERT( aDBFilePageCount % sChunkPageCount == 0 );

    (void)invalidate(aTBSNode); // db�� inconsistency ���·� ����
    {
        ////////////////////////////////////////////////////////////////
        // (010) 0�� Meta Page (Membase ����)�� �ʱ�ȭ
        // 0�� Page�Ҵ�
        IDE_TEST( createTBSMetaPage( aTBSNode,
                                     aTrans,
                                     aDBName,
                                     aDBFilePageCount,
                                     aDBCharSet,
                                     aNationalCharSet,
                                     ID_FALSE /* aIsNeedLogging */ )
                  != IDE_SUCCESS );

        ///////////////////////////////////////////////////////////////
        // (020) Expand Chunk������ �ʱ�ȭ
        IDE_TEST( smmExpandChunk::setChunkPageCnt( aTBSNode,
                                                   sChunkPageCount )
                  != IDE_SUCCESS );

        // Expand Chunk�� ���õ� Property �� üũ
        IDE_TEST( smmDatabase::checkExpandChunkProps(aTBSNode->mMemBase)
                  != IDE_SUCCESS );

        //////////////////////////////////////////////////////////////////////
        // (030) �ʱ� Tablespaceũ�⸸ŭ Tablespace Ȯ��(Expand Chunk �Ҵ�)

        // ������ �����ͺ��̽� Page���� ���� ���� �Ҵ��� Expand Chunk �� ���� ����
        sNewChunks = smmExpandChunk::getExpandChunkCount(
                                    aTBSNode,
                                    aCreatePageCount - SMM_DATABASE_META_PAGE_CNT );

        // �ý��ۿ��� ���� �ϳ��� Tablespace����
        // ChunkȮ���� �ϵ��� �ϴ� Mutex
        // => �� ���� Tablespace�� ���ÿ� ChunkȮ���ϴ� ��Ȳ������
        //    ��� Tablespace�� �Ҵ��� Page ũ�Ⱑ MEM_MAX_DB_SIZE����
        //    ���� �� �˻��� �� ���� ����
        IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex()
                  != IDE_SUCCESS );
        sState = 1;

        /* BUG- 35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
         * MEM_MAX_DB_SIZE */
        if( smuProperty::getSeparateDicTBSSizeEnable() == ID_TRUE )
        {
            if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
            {
                IDE_TEST( smmFPLManager::getDicTBSPageCount( &sTotalPageCount ) 
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( smmFPLManager::getTotalPageCountExceptDicTBS( 
                                                            &sTotalPageCount )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( smmFPLManager::getTotalPageCount4AllTBS( 
                                                            &sTotalPageCount ) 
                      != IDE_SUCCESS );
        }

        // Tablespace�� Ȯ���� Database�� ��� Tablespace�� ����
        // �Ҵ�� Page���� ������ MEM_MAX_DB_SIZE ������Ƽ�� ����
        // ũ�⺸�� �� ũ�� ����
        IDE_TEST_RAISE( ( sTotalPageCount + ( sNewChunks * sChunkPageCount ) ) >
                        ( smuProperty::getMaxDBSize() / SM_PAGE_SIZE ),
                        error_unable_to_create_cuz_mem_max_db_size );

        sState = 0;
        IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex()
                  != IDE_SUCCESS );

        IDE_ASSERT( aTBSNode->mMemBase->mAllocPersPageCount <=
                    aTBSNode->mDBMaxPageCount );

        if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
        {
            // Service ���·� ���� ����
            smmDatabase::makeMembaseBackup();
        }

    } // ��ȣ �ȿ����� aTBSNode�� invalid
    (void)validate(aTBSNode); // db�� consistency ���·� �ǵ���.

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_unable_to_create_cuz_mem_max_db_size );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_CREATE_CUZ_MEM_MAX_DB_SIZE,
                     aTBSNode->mHeader.mName,
                     (ULong) ( ( aCreatePageCount * SM_PAGE_SIZE) / 1024),
                     (ULong) (smuProperty::getMaxDBSize()/1024),
                     (ULong) ( (sTotalPageCount * SM_PAGE_SIZE ) / 1024 )
                ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sState )
        {
            case 1:

                IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex()
                            == IDE_SUCCESS );

                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
   Tablespace�� Meta Page�� �ʱ�ȭ�ϰ� Free Page���� �����Ѵ�.

   ChunkȮ�忡 ���� �α��� �ǽ��Ѵ�.

   aTrans           [IN]
   aDBName          [IN] �����ͺ��̽� �̸�
   aCreatePageCount [IN] ������ �����ͺ��̽��� ���� Page�� ��
                         Membase�� ��ϵǴ� �����ͺ��̽�
                         Meta Page ���� �����Ѵ�.
                         �� ���� smiMain::smiCalculateDBSize()�� ���ؼ�
                         ������ ���̾�� �ϰ� smiGetMaxDBPageCount()����
                         ���� ������ Ȯ���� ���� ���̾�� �Ѵ�.
   aDbFilePageCount [IN] �ϳ��� �����ͺ��̽� ������ ���� Page�� ��
   aChunkPageCount  [IN] �ϳ��� Expand Chunk�� ���� Page�� ��


   [ �˰��� ]
   - (010) 0�� Meta Page (Membase ����)�� �ʱ�ȭ
   - (020) Expand Chunk������ �ʱ�ȭ
   - (030) �ʱ� Tablespaceũ�⸸ŭ Tablespace Ȯ��(Expand Chunk �Ҵ�)
 */
IDE_RC smmManager::createTBSPages( smmTBSNode   * aTBSNode,
                                   void         * aTrans,
                                   SChar        * aDBName,
                                   UInt           aDBFilePageCount,
                                   UInt           aCreatePageCount,
                                   SChar        * aDBCharSet,
                                   SChar        * aNationalCharSet )

{
    UInt        sState          = 0;
    scPageID    sTotalPageCount;
    vULong      sNewChunks;
    UInt        sChunkPageCount = smuProperty::getExpandChunkPageCount();

    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aDBName    != NULL );
    IDE_DASSERT( aDBFilePageCount   > 0 );
    IDE_DASSERT( aCreatePageCount   > 0 );

    // PROJ-1579 NCHAR
    IDE_DASSERT( aDBCharSet != NULL );
    IDE_DASSERT( aNationalCharSet != NULL );


    // �����ͺ��̽� ������ ũ�Ⱑ expand chunkũ���� ������� Ȯ��.
    IDE_ASSERT( aDBFilePageCount % sChunkPageCount == 0 );

    (void)invalidate(aTBSNode); // db�� inconsistency ���·� ����
    {

        ////////////////////////////////////////////////////////////////
        // (010) 0�� Meta Page (Membase ����)�� �ʱ�ȭ
        // 0�� Page�Ҵ�
        IDE_TEST( createTBSMetaPage( aTBSNode,
                                     aTrans,
                                     aDBName,
                                     aDBFilePageCount,
                                     aDBCharSet,
                                     aNationalCharSet,
                                     ID_TRUE /* aIsNeedLogging */ )
                  != IDE_SUCCESS );


        ///////////////////////////////////////////////////////////////
        // (020) Expand Chunk������ �ʱ�ȭ
        IDE_TEST( smmExpandChunk::setChunkPageCnt( aTBSNode,
                                                   sChunkPageCount )
                  != IDE_SUCCESS );

        // Expand Chunk�� ���õ� Property �� üũ
        IDE_TEST( smmDatabase::checkExpandChunkProps(aTBSNode->mMemBase)
                  != IDE_SUCCESS );


        //////////////////////////////////////////////////////////////////////
        // (030) �ʱ� Tablespaceũ�⸸ŭ Tablespace Ȯ��(Expand Chunk �Ҵ�)

        // ������ �����ͺ��̽� Page���� ���� ���� �Ҵ��� Expand Chunk �� ���� ����
        sNewChunks = smmExpandChunk::getExpandChunkCount(
                                        aTBSNode,
                                        aCreatePageCount - SMM_DATABASE_META_PAGE_CNT );


        // �ý��ۿ��� ���� �ϳ��� Tablespace����
        // ChunkȮ���� �ϵ��� �ϴ� Mutex
        // => �� ���� Tablespace�� ���ÿ� ChunkȮ���ϴ� ��Ȳ������
        //    ��� Tablespace�� �Ҵ��� Page ũ�Ⱑ MEM_MAX_DB_SIZE����
        //    ���� �� �˻��� �� ���� ����
        IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex()
                  != IDE_SUCCESS );
        sState = 1;

        /*
         * BUG- 35443 Add Property for Excepting SYS_TBS_MEM_DIC size from
         * MEM_MAX_DB_SIZE
         */
        if( smuProperty::getSeparateDicTBSSizeEnable() == ID_TRUE )
        {
            if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
            {
                IDE_TEST( smmFPLManager::getDicTBSPageCount( &sTotalPageCount ) 
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( smmFPLManager::getTotalPageCountExceptDicTBS( 
                                                            &sTotalPageCount )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            IDE_TEST( smmFPLManager::getTotalPageCount4AllTBS( 
                                                            &sTotalPageCount ) 
                      != IDE_SUCCESS );
        }

        // Tablespace�� Ȯ���� Database�� ��� Tablespace�� ����
        // �Ҵ�� Page���� ������ MEM_MAX_DB_SIZE ������Ƽ�� ����
        // ũ�⺸�� �� ũ�� ����
        IDE_TEST_RAISE( ( sTotalPageCount + ( sNewChunks * sChunkPageCount ) ) >
                        ( smuProperty::getMaxDBSize() / SM_PAGE_SIZE ),
                        error_unable_to_create_cuz_mem_max_db_size );

        // Ʈ������� NULL�� �Ѱܼ� �α��� ���� �ʵ��� �Ѵ�.
        // �ִ� Page���� �Ѿ���� �� �ӿ��� üũ�Ѵ�.
        IDE_TEST( allocNewExpandChunks( aTBSNode,
                                        aTrans, // �α��Ѵ�.
                                        sNewChunks )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex()
                  != IDE_SUCCESS );


        IDE_ASSERT( aTBSNode->mMemBase->mAllocPersPageCount <=
                    aTBSNode->mDBMaxPageCount );

        if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
        {
            // Service ���·� ���� ����
            smmDatabase::makeMembaseBackup();
        }

    }
    (void)validate(aTBSNode); // db�� consistency ���·� �ǵ���.



    return IDE_SUCCESS;

    IDE_EXCEPTION( error_unable_to_create_cuz_mem_max_db_size );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UNABLE_TO_CREATE_CUZ_MEM_MAX_DB_SIZE,
                     aTBSNode->mHeader.mName,
                     (ULong) ( ( aCreatePageCount * SM_PAGE_SIZE) / 1024),
                     (ULong) (smuProperty::getMaxDBSize()/1024),
                     (ULong) ( (sTotalPageCount * SM_PAGE_SIZE ) / 1024 )
                ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sState )
        {
            case 1:

                IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex()
                            == IDE_SUCCESS );

                break;

            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*
   Tablespace�� Meta Page ( 0�� Page)�� �ʱ�ȭ�Ѵ�.

   �� �Լ��� Create Tablespace���� Tablespace�� Meta ������ ���ϴ�
   0�� Page�� �ʱ�ȭ�Ѵ�.

   aDBName          [IN] �����ͺ��̽� �̸�
   aCreatePageCount [IN] ������ �����ͺ��̽��� ���� Page�� ��
                         Membase�� ��ϵǴ� �����ͺ��̽� Meta Page ����
                         �����Ѵ�.
   aDbFilePageCount [IN] �ϳ��� �����ͺ��̽� ������ ���� Page�� ��
   aChunkPageCount  [IN] �ϳ��� Expand Chunk�� ���� Page�� ��
    IDE_DASSERT( aTBSAttr->mAttrType == SMI_TBS_ATTR );

   [ �˰��� ]
   (010) 0�� Page�� �ּ� ȹ��
   (020) 0�� Page �� PID�� Page Type����
   (030) Membase�� Catalog Table�� �޸� ������ �����Ѵ�.
   (040) Membase�� ������ �ʱ�ȭ�Ѵ�.
   (050) MemBase ��ü�� �α��Ѵ�.
   (060) 0�� Page�� DirtyPage�� ���
 */
IDE_RC smmManager::createTBSMetaPage( smmTBSNode  * aTBSNode,
                                      void        * aTrans,
                                      SChar       * aDBName,
                                      scPageID      aDBFilePageCount,
                                      SChar       * aDBCharSet,
                                      SChar       * aNationalCharSet,
                                      idBool        aIsNeedLogging /* PROJ-1923 */ )
{
    void      * sBasePage ;
    UInt        sChunkPageCount = smuProperty::getExpandChunkPageCount();

    scSpaceID   sSpaceID = aTBSNode->mHeader.mID;

    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aDBName    != NULL );
    IDE_DASSERT( aDBFilePageCount   > 0 );

    // PROJ-1579 NCHAR
    IDE_DASSERT( aDBCharSet         != NULL );
    IDE_DASSERT( aNationalCharSet   != NULL );

    IDE_TEST(fillPCHEntry( aTBSNode, SMM_MEMBASE_PAGEID ) != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////
    // (010) 0�� Page�� �ּ� ȹ��
    sBasePage = mPCArray[ sSpaceID ].mPC[SMM_MEMBASE_PAGEID].mPagePtr;

    // Meta Page�� Disk�� ������ UMR(Uninitialized Memory Read) �߻����� �ʵ��� memset�ǽ�
    idlOS::memset( sBasePage, 0x0, SM_PAGE_SIZE );

    // ����� ����� �Ϻη� Garbage�� �ʱ�ȭ �ؼ� ������ �ʱ⿡ ����
#ifdef DEBUG_SMM_FILL_GARBAGE_PAGE
    idlOS::memset( sBasePage, 0x43, SM_PAGE_SIZE );
#endif


    ///////////////////////////////////////////////////////////////////////
    // (020) 0�� Page �� PID�� Page Type����
    //
    // (020)-1 �α�ǽ�
    if( aIsNeedLogging == ID_TRUE )
    {
        IDE_TEST( smLayerCallback::updateLinkAtPersPage( NULL, /* idvSQL* */
                                                         aTrans,
                                                         sSpaceID,
                                                         SMM_MEMBASE_PAGEID,// Page ID
                                                         SM_NULL_PID,   // Before PREV_PID
                                                         SM_NULL_PID,   // Before NEXT_PID
                                                         SM_NULL_PID,   // After PREV_PID
                                                         SM_NULL_PID )  // After PREV_PID
                  != IDE_SUCCESS );
    }
    else
    {
        // do nothing
    }


    // (020)-2 Page�� PID�� Page Type���� �ǽ�
    smLayerCallback::linkPersPage( sBasePage,
                                   SMM_MEMBASE_PAGEID,
                                   SM_NULL_PID,
                                   SM_NULL_PID );


    ///////////////////////////////////////////////////////////////////////
    // (030) Membase�� Catalog Table�� �޸� ������ �����Ѵ�.
    if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
    {
        setupCatalogPointers( aTBSNode, (UChar*)sBasePage );
    }


    IDE_TEST( setupMemBasePointer( aTBSNode,
                                   (UChar*) sBasePage )
              // version check ����!
              != IDE_SUCCESS);



    ///////////////////////////////////////////////////////////////////////
    // (040) Membase�� ������ �ʱ�ȭ�Ѵ�.
    IDE_TEST( smmDatabase::initializeMembase(
                                      aTBSNode,
                                      aDBName,
                                      aDBFilePageCount,
                                      sChunkPageCount,
                                      aDBCharSet,
                                      aNationalCharSet )
              != IDE_SUCCESS );


    ///////////////////////////////////////////////////////////////////////
    // (050) MemBase ��ü�� �α��Ѵ�.
    if( aIsNeedLogging == ID_TRUE )
    {
        IDE_TEST( smLayerCallback::setMemBaseInfo( NULL, /* idvSQL* */
                                                   aTrans,
                                                   sSpaceID,
                                                   aTBSNode->mMemBase )
                  != IDE_SUCCESS );
    }
    else
    {
        // do nothing
    }


    ///////////////////////////////////////////////////////////////////////
    // (060) 0�� Page�� DirtyPage�� ���
    IDE_TEST( smmDirtyPageMgr::insDirtyPage( sSpaceID, SMM_MEMBASE_PAGEID )
              != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   Memory Tablespace Node�� �ʵ带 �ʱ�ȭ�Ѵ�.

   [IN] aTBSNode - Tablespace��  Node
   [IN] aTBSAttr - Tablespace��  Attribute
 */
IDE_RC smmManager::initMemTBSNode( smmTBSNode         * aTBSNode,
                                   smiTableSpaceAttr  * aTBSAttr )
{
    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( aTBSAttr != NULL );

    // (10) Memory / Disk Tablespace ���� �ʱ�ȭ
    IDE_TEST( sctTableSpaceMgr::initializeTBSNode( & aTBSNode->mHeader,
                                                   aTBSAttr )
              != IDE_SUCCESS );

    // (20) Tablespace Attribute����
    idlOS::memcpy( &(aTBSNode->mTBSAttr),
                   aTBSAttr,
                   ID_SIZEOF(smiTableSpaceAttr) );

    // (30) Checkpoint Path List �ʱ�ȭ
    SMU_LIST_INIT_BASE( & aTBSNode->mChkptPathBase );

    // (50) Anchor Offset�ʱ�ȭ
    aTBSNode->mAnchorOffset = SCT_UNSAVED_ATTRIBUTE_OFFSET;

    // (60) mRestoreType�ʱ�ȭ := ���� Restore���� ��������.
    aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET;

    // (70) Page/File ������ 0���� ���� (MEDIA�ܰ迡�� �ʱ�ȭ��)
    aTBSNode->mDBMaxPageCount = 0;
    aTBSNode->mHighLimitFile = 0;

    // Dirty Page �����ڸ� NULL�� �ʱ�ȭ ( PAGE�ܰ迡�� �ʱ�ȭ�� )
    aTBSNode->mDirtyPageMgr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*  Tablespace�� ���� ��ü(DB File) ���� �ý��� �ʱ�ȭ

   [IN] aTBSNode - Tablespace��  Node
*/
IDE_RC smmManager::initMediaSystem( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // ���� prepare/restore�� �� ���°� �ƴϾ�� �Ѵ�.
    IDE_ASSERT( aTBSNode->mRestoreType ==
                SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET );


    scPageID     sDbFilePageCount;

    // Tablespace Attribute�� ��ϵ� DB File�� PAGE���� ����
    // �ϳ��� DB Fileũ�Ⱑ OS�� ���� ũ�� ���ѿ� �ɸ����� ���Ѵ�.
    sDbFilePageCount = aTBSNode->mTBSAttr.mMemAttr.mSplitFilePageCount;

    IDE_TEST( checkOSFileSize( sDbFilePageCount * SM_PAGE_SIZE )
              != IDE_SUCCESS );

    IDE_TEST( initDBFileObjects( aTBSNode, sDbFilePageCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/********************************************************************************
 * memory, volatile �� PCH Array �� �Ҵ��Ѵ�.
 * 
 ********************************************************************************/
IDE_RC smmManager::allocPCHArray( scSpaceID aSpaceID,
                                  UInt      aMaxPageCount )
{
    // PCH Array�ʱ�ȭ
    IDE_TEST(iduMemMgr::calloc(IDU_MEM_SM_SMM,
                               aMaxPageCount,
                               ID_SIZEOF(smPCSlot),
                               (void**)&mPCArray[aSpaceID].mPC)
             != IDE_SUCCESS);

    mPCArray[ aSpaceID ].mMaxPageCount = aMaxPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/********************************************************************************
 * memory, volatile �� PCH Array �� �����Ѵ�.
 * 
 ********************************************************************************/
IDE_RC smmManager::freePCHArray( scSpaceID aSpaceID )
{
    IDE_TEST( iduMemMgr::free( mPCArray[ aSpaceID ].mPC ) != IDE_SUCCESS );
    mPCArray[ aSpaceID ].mMaxPageCount = 0;
    mPCArray[ aSpaceID ].mPC           = NULL;
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*  Tablespace�� ���� Page���� �ý��� �ʱ�ȭ

    [IN] aTBSNode    - �ʱ�ȭ�� Tablespace Node

    ���� - initMediaSystem�� ȣ��� ���¿����� �� �Լ��� �������Ѵ�.

    ���� - initPageSystem�� prepare/restore�� ���� �ڷᱸ���� �غ��Ѵ�.
            page memory pool�����ڴ� prepare/restore�ÿ� �����ȴ�.
            �� �Լ������� page memory pool�����ڸ� �ʱ�ȭ���� �ʴ´�.
 */
IDE_RC smmManager::initPageSystem( smmTBSNode        * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // ���� prepare/restore�� �� ���°� �ƴϾ�� �Ѵ�.
    IDE_ASSERT( aTBSNode->mRestoreType ==
                SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET );

    // initMediaSystem�� ȣ��Ǿ����� �˻�.
    // initMediaSystem�� initDBFileObjects���� �̸� �����Ѵ�.
    IDE_ASSERT( aTBSNode->mDBMaxPageCount != 0 );
    IDE_ASSERT( aTBSNode->mHighLimitFile != 0 );

    // Dirty Page�����ڸ� �ʱ�ȭ
    IDE_TEST( smmDirtyPageMgr::createDPMgr( aTBSNode )
              != IDE_SUCCESS );

    // Free Page List������ �ʱ�ȭ
    IDE_TEST( smmFPLManager::initialize( aTBSNode ) != IDE_SUCCESS );

    // TablespaceȮ�� ChunK������ �ʱ�ȭ
    IDE_TEST( smmExpandChunk::initialize( aTBSNode ) != IDE_SUCCESS );

    /* smmManager_initPageSystem_calloc_PCHArray.tc */
    IDU_FIT_POINT("smmManager::initPageSystem::calloc::PCHArray");
    IDE_TEST( allocPCHArray( aTBSNode->mHeader.mID,
                             aTBSNode->mDBMaxPageCount ) != IDE_SUCCESS );

    // PCH Memory Pool�ʱ�ȭ
    IDE_TEST(aTBSNode->mPCHMemPool.initialize(
                 IDU_MEM_SM_SMM,
                 (SChar*)"PCH_MEM_POOL",
                 1,    // ����ȭ ���� �ʴ´�.
                 ID_SIZEOF(smmPCH),
                 1024, // �ѹ��� 1024���� PCH������ �� �ִ� ũ��� �޸𸮸� Ȯ���Ѵ�.
                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                 ID_TRUE,							/* UseMutex */
                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                 ID_FALSE,							/* ForcePooling */
                 ID_TRUE,							/* GarbageCollection */
                 ID_TRUE,                           /* HWCacheLine */
                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
             != IDE_SUCCESS);			


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/* �������� Expand Chunk�� �߰��Ͽ� �����ͺ��̽��� Ȯ���Ѵ�.
 *
 *
 *
 * aTrans            [IN] �����ͺ��̽��� Ȯ���ϰ��� �ϴ� Ʈ�����
 *                        CreateDB Ȥ�� logical redo���ΰ�� NULL�� ���´�.
 * aExpandChunkCount [IN] Ȯ���ϰ��� �ϴ� Expand Chunk�� ��
 */
IDE_RC smmManager::allocNewExpandChunks( smmTBSNode   * aTBSNode,
                                         void         * aTrans,
                                         UInt           aExpandChunkCount )
{
    UInt        i;
    scPageID    sChunkFirstPID;
    scPageID    sChunkLastPID;

    // �� �Լ��� Normal Processing������ �Ҹ���Ƿ�
    // Expand ChunkȮ�� �� �����ͺ��̽� Page����
    // �ִ� Page������ �Ѿ���� üũ�Ѵ�.
    //
    // mDBMaxPageCount�� MAXSIZE�� �ش��ϴ� Page Count�� ���Ѵ�.
    // �׷���, ������ ���� ����ڰ� MAXSIZE�� INITSIZE��
    // ���� �����ϴ� ��� MAXSIZE���ѿ� �ɷ���
    // Tablespace������ ���� ���ϴ� ������ �߻��Ѵ�.
    //
    // CREATE MEMORY TABLESPACE MEM_TBS SIZE 16M
    // AUTOEXTEND ON NEXT 8M MAXSIZE 16M;
    //
    // ���� ��� �ʱ�ũ�� 16M, Ȯ��ũ�� 16M������,
    // 32K(META PAGEũ��) + 16M > 16M ���� Ȯ���� ���� �ʰ� �ȴ�.
    // �Ҵ�� Page������ METAPAGE���� ���� MAXSIZEüũ�� �ؾ�
    // MAXSIZE��
    IDE_TEST_RAISE( aTBSNode->mMemBase->mAllocPersPageCount
                    - SMM_DATABASE_META_PAGE_CNT
                    + aExpandChunkCount * aTBSNode->mMemBase->mExpandChunkPageCnt
                    > aTBSNode->mDBMaxPageCount ,
                    max_page_error);

    // Expand Chunk�� �����ͺ��̽��� �߰��Ͽ� �����ͺ��̽��� Ȯ���Ѵ�.
    for( i = 0 ; i < aExpandChunkCount ; i++ )
    {
        // ���� �߰��� Chunk�� ù��° Page ID�� ����Ѵ�.
        // ���ݱ��� �Ҵ��� ��� Chunk�� �� Page���� �� Chunk�� ù��° Page ID�� �ȴ�.
        sChunkFirstPID = aTBSNode->mMemBase->mCurrentExpandChunkCnt *
                         aTBSNode->mMemBase->mExpandChunkPageCnt +
                         SMM_DATABASE_META_PAGE_CNT ;
        // ���� �߰��� Chunk�� ������ Page ID�� ����Ѵ�.
        sChunkLastPID  = sChunkFirstPID + aTBSNode->mMemBase->mExpandChunkPageCnt - 1;

        IDE_TEST( allocNewExpandChunk( aTBSNode,
                                       aTrans,
                                       sChunkFirstPID,
                                       sChunkLastPID )
                  != IDE_SUCCESS );
    } // end of for


    return IDE_SUCCESS;

    IDE_EXCEPTION(max_page_error);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TooManyPage,
                                (ULong)aTBSNode->mDBMaxPageCount ));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/* ���� �Ҵ�� Expand Chunk�ȿ� ���ϴ� Page���� PCH Entry�� �Ҵ��Ѵ�.
 * Chunk���� Free List Info Page�� Page Memory�� �Ҵ��Ѵ�.
 *
 * ����! 1. Alloc Chunk�� Logical Redo����̹Ƿ�, Physical �α����� �ʴ´�.
 *          Chunk���� Free Page�鿡 ���ؼ��� Page Memory�� �Ҵ����� ����
 *
 * aNewChunkFirstPID [IN] Chunk���� ù��° Page
 * aNewChunkLastPID  [IN] Chunk���� ������ Page
 */
IDE_RC smmManager::fillPCHEntry4AllocChunk(smmTBSNode * aTBSNode,
                                           scPageID     aNewChunkFirstPID,
                                           scPageID     aNewChunkLastPID )
{
    UInt       sFLIPageCnt = 0 ;
    scSpaceID  sSpaceID;
    scPageID   sPID = 0 ;

    sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( isValidPageID( sSpaceID, aNewChunkFirstPID )
                 == ID_TRUE );
    IDE_DASSERT( isValidPageID( sSpaceID, aNewChunkLastPID )
                 == ID_TRUE );

    for ( sPID = aNewChunkFirstPID ;
          sPID <= aNewChunkLastPID ;
          sPID ++ )
    {
        // Restart Recovery �ÿ� ���Դٸ�,
        // DB�� �̹� �ε��� �����̹Ƿ�, PCH�� �̹� �Ҵ�Ǿ� ���� �� �ִ�.

        if ( mPCArray[sSpaceID].mPC[sPID].mPCH == NULL )
        {
            // PCH Entry�� �Ҵ��Ѵ�.
            IDE_TEST( allocPCHEntry( aTBSNode, sPID ) != IDE_SUCCESS );
        }

        // BUG-47487: FLI ������ �Ҵ� ��ġ  
        // Free List Info Page�� Page �޸𸮸� �Ҵ��Ѵ�.
        if ( sFLIPageCnt < smmExpandChunk::getChunkFLIPageCnt(aTBSNode) )
        {
            sFLIPageCnt ++ ;

            // Restart Recovery�߿��� �ش� Page�޸𸮰� �̹� �Ҵ�Ǿ�
            // ���� �� �ִ�.
            //
            // allocAndLinkPageMemory ���� �̸� ����Ѵ�.
            // �ڼ��� ������ allocPageMemory�� �ּ��� ����

            IDE_TEST( allocAndLinkPageMemory( aTBSNode,
                                              NULL, // �α����� ����
                                              sPID,          // PID
                                              SM_NULL_PID,   // prev PID
                                              SM_NULL_PID )  // next PID
                      != IDE_SUCCESS );
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}



/* Ư�� ������ ������ŭ �����ͺ��̽��� Ȯ���Ѵ�.
 *
 * aTrans �� NULL�� ������, �� ���� Restart Redo���� �Ҹ��� ������
 * Normal Processing���� ������ �� ������ ���� ������ �ٸ��� �۵��Ѵ�.
 *    1. Logging �� �ʿ䰡 ����
 *    2. �Ҵ��� Page Count�� ������ �˻��� �ʿ䰡 ����
 *       by gamestar 2001/05/24
 *
 * ��� Free Page List�� ���� Latch�� ������ ���� ä�� �� �Լ��� ȣ��ȴ�.
 *
 * aTrans            [IN] �����ͺ��̽��� Ȯ���Ϸ��� Ʈ�����
 *                        CreateDB Ȥ�� logical redo���ΰ�� NULL�� ���´�.
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ù��° Page ID
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ������ Page ID
 */
IDE_RC smmManager::allocNewExpandChunk(smmTBSNode * aTBSNode,
                                       void       * aTrans,
                                       scPageID     aNewChunkFirstPID,
                                       scPageID     aNewChunkLastPID )
{
    UInt            sStage      = 0;
    UInt            sNewDBFileCount;
    smLSN           sCreateLSN;
    smLSN           sMemCreateLSN;
    UInt            sArrFreeListCount;
    smmPageList     sArrFreeList[ SMM_MAX_FPL_COUNT ];

    IDE_DASSERT( aTBSNode != NULL );

    // BUGBUG kmkim ����ó���� ����. ����ڰ� ������Ƽ �߸� �ٲپ�����
    // �ٷ� �˷��� �� �ֵ���.
    // ������ ����ڰ� PAGE�� ������ ������ Restart Recovery�ϸ� ���⼭ �װԵȴ�.
    IDE_ASSERT( aNewChunkFirstPID < aTBSNode->mDBMaxPageCount );
    IDE_ASSERT( aNewChunkLastPID  < aTBSNode->mDBMaxPageCount );

    // ��� Free Page List�� Latchȹ��
    IDE_TEST( smmFPLManager::lockAllFPLs(aTBSNode) != IDE_SUCCESS );
    sStage = 1;


    if ( aTrans != NULL )
    {
        // RunTime ��
        IDE_TEST( smLayerCallback::allocExpandChunkAtMembase(
                      NULL, /* idvSQL* */
                      aTrans,
                      aTBSNode->mTBSAttr.mID,
                      // BEFORE : Logical Redo �ϱ� ���� �̹����� Ȱ��.
                      aTBSNode->mMemBase,
                      ID_UINT_MAX, /* ID_UINT_MAX�� �����Ͽ� ���� Page List�� ���� �й�ǵ����� */
                      aNewChunkFirstPID, // �� Chunk���� PID
                      aNewChunkLastPID,  // �� Chunk�� PID
                      &sCreateLSN )      // CreateLSN
                  != IDE_SUCCESS );


        // ������ �����Ǿ�� �ϴ� ���
        // Last LSN�� ��´�.
        smLayerCallback::getLstLSN( &sMemCreateLSN );

        // Create LSN ���� ���� ���ϰ� �Ǵ� ArrCreateLSN ��
        // �������� �������� �ʴ´�.
        // ������ �����Ǵ� (Checkpoint)�� �����Ͽ� ���Ͽ�
        // ���������� ��ϵȴ�.
        SM_GET_LSN( sMemCreateLSN, sCreateLSN );
    }
    else
    {
        // Restart Recovery
        // ��߿� ������ �������� ���Ͽ� �޸� ������ ������
        // �þ ��� redo�ϸ鼭 CreateLSN Set�� �������־�� �Ѵ�.

        /* ������ �����Ǿ�� �ϴ� ��� LstCheckLSN�� ��´�.
         *
         * RedoLSNMgr�κ��� ��� CreateLSN ��
         * Next LSN(read �ؾ���)�̰ų�, invalid�� LSN�� �� �ִ�.
         * RedoLSNMgr �����ؼ��� �ݵ�� �����Ǿ�� ���ʿ䰡 ���µ�
         * ������ ���� ������� Scan�ϸ鼭 ��� �α��̱� �����̴�.
         *
         * �̷��� ������ CreateLSN�� �������� �̵�� ������ �̿��Ѵ�.
         */
        // �ش� �α׸� ���� ���� ���� RedoInfo ��
        // �α� offset�� ������Ű�� ������ �ش� ALLOC_EXPAND_CHUNK
        // �α׿� �ش��ϴ� LSN�� ��ȯ�Ѵ�.
        SM_GET_LSN( sMemCreateLSN ,
                    smLayerCallback::getLstCheckLogLSN() );
    }

    // Chunk�� ���� �Ҵ�� �� DB File�� ��� �� ���ܾ� �ϴ��� ���
    IDE_TEST( calcNewDBFileCount( aTBSNode,
                                  aNewChunkFirstPID,
                                  aNewChunkLastPID,
                                  & sNewDBFileCount )
              != IDE_SUCCESS );

    if ( sNewDBFileCount > 0 )
    {
        // ������ �����Ǿ�� �ϴ� ���
        // �ϳ��� Chunk�� �ϳ��� ���ϳ��� ���ԵǹǷ�
        // ������ �����Ǿ�� �ϴ� ��쿡�� ���� 1���� ���������ϴ�.
        IDE_ASSERT( sNewDBFileCount == 1 );

        // PRJ-1548 User Memory TableSpace
        // �̵����� ���� CreateLSN�� ����Ÿ������ ��Ÿ������� ����
        IDE_TEST( smmTBSMediaRecovery::setCreateLSN4NewDBFiles(
                                 aTBSNode,
                                 &sMemCreateLSN ) != IDE_SUCCESS );
    }
    else
    {
        // ������ �������� �ʰ�, CHUNK�� Ȯ��� ���
    }


    // �ϳ��� Expand Chunk�� ���ϴ� Page���� PCH Entry���� �����Ѵ�.
    IDE_TEST( fillPCHEntry4AllocChunk( aTBSNode,
                                       aNewChunkFirstPID,
                                       aNewChunkLastPID )
              != IDE_SUCCESS );


    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    sArrFreeListCount = aTBSNode->mMemBase->mFreePageListCount;

    // Logical Redo �� ���̹Ƿ� Physical Update( Next Free Page ID ����)
    // �� ���� �α��� ���� ����.
    IDE_TEST( smmFPLManager::distributeFreePages(
                  aTBSNode,
                  // Chunk���� ù��° Free Page
                  // Chunk�� �պκ��� Free List Info Page���� �����ϹǷ�,
                  // Free List Info Page��ŭ �ǳʶپ�� Free Page�� ���´�.
                  aNewChunkFirstPID +
                  smmExpandChunk::getChunkFLIPageCnt(aTBSNode),
                  // Chunk���� ������ Free Page
                  aNewChunkLastPID,
                  ID_TRUE, // set next free page, PRJ-1548
                  sArrFreeListCount,
                  sArrFreeList  )
              != IDE_SUCCESS );
    sStage = 2;


    // ����! smmUpdate::redo_SMMMEMBASE_ALLOC_EXPANDCHUNK ����
    // membase ������� Logical Redo�ϱ� ���� ������ �����س��� �� ��ƾ����
    // ���;� �Ѵ�.

    // ���ݱ��� �����ͺ��̽��� �Ҵ�� �� �������� ����
    aTBSNode->mMemBase->mAllocPersPageCount = aNewChunkLastPID + 1;
    aTBSNode->mMemBase->mCurrentExpandChunkCnt ++ ;


    // DB File �� ����
    aTBSNode->mMemBase->mDBFileCount[0]    += sNewDBFileCount;
    aTBSNode->mMemBase->mDBFileCount[1]    += sNewDBFileCount;

    // Logical Redo�� ���̹Ƿ� Phyical Update�� ���� �α����� �ʴ´�.
    IDE_TEST( smmFPLManager::appendPageLists2FPLs(
                                          aTBSNode,
                                          sArrFreeList,
                                          ID_TRUE, // aSetFreeListOfMembase
                                          ID_TRUE )// aSetNextFreePageOfFPL
              != IDE_SUCCESS );


    sStage = 1;
    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aTBSNode->mTBSAttr.mID,
                                            SMM_MEMBASE_PAGEID) != IDE_SUCCESS);




    sStage = 0;
    IDE_TEST( smmFPLManager::unlockAllFPLs(aTBSNode) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch (sStage)
    {
        case 2:
        IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aTBSNode->mTBSAttr.mID,
                                                  SMM_MEMBASE_PAGEID) == IDE_SUCCESS);
        case 1:
        IDE_ASSERT( smmFPLManager::unlockAllFPLs(aTBSNode) == IDE_SUCCESS );

        default:
            break;
    } 
    //BUG-15508 TASK-2000 mmdb module ������ �߰��� ���� ����Ʈ
    //���� SBUG-1, smmManager::allocNewExpandChunk���н� FATAL�� ó��
    //���Լ��� Logical redo�� �ϴ� �κ����� undo�� ���� �ʱ� ������ abort�� �Ǹ� �ȵȴ�.
    //���� ���̰�, �ٽ� recovery�ϴ� ���� ����.
    IDE_SET( ideSetErrorCode(
                 smERR_FATAL_ALLOC_NEW_EXPAND_CHUNK));


    return IDE_FAILURE;
}


/*
 * �����ͺ��̽� Ȯ�忡 ���� ���ο� Expand Chunk�� �Ҵ�ʿ� ����
 * ���� ���ܳ��� �Ǵ� DB������ ���� ����Ѵ�.
 *
 * aChunkFirstPID [IN] Expand Chunk�� ù��° Page ID
 * aChunkLastPID  [IN] Expand Chunk�� ������ Page ID
 */
IDE_RC smmManager::calcNewDBFileCount( smmTBSNode * aTBSNode,
                                       scPageID     aChunkFirstPID,
                                       scPageID     aChunkLastPID,
                                       UInt       * aNewDBFileCount)
{
    UInt    sFirstFileNo    = 0;
    UInt    sLastFileNo     = 0;

    IDE_DASSERT( isValidPageID( aTBSNode->mTBSAttr.mID, aChunkFirstPID )
                 == ID_TRUE );
    IDE_DASSERT( isValidPageID( aTBSNode->mTBSAttr.mID, aChunkLastPID )
                 == ID_TRUE );

    // �Ҵ���� ���������� ���� ����Ÿ ȭ���� ����
    // membase ���� �� �α븸 ����: ���� ȭ���� ������ checkpoint�� �����
    sFirstFileNo = getDbFileNo( aTBSNode, aChunkFirstPID );
    sLastFileNo  = getDbFileNo( aTBSNode, aChunkLastPID );

    // sFirstFileNo�� ������ ������ DB�����̶��
    if(aTBSNode->mMemBase->mDBFileCount[aTBSNode->mTBSAttr.mMemAttr.mCurrentDB]
       == ( sFirstFileNo + 1))
    {
        // ���� ������ DB������ �ٷ� �� ���� ����
        sFirstFileNo ++;
    }

    // ���� ������ DB���� ��
    *aNewDBFileCount = sLastFileNo - sFirstFileNo + 1;


    return IDE_SUCCESS;
}

/*
 * Page���� �޸𸮸� �Ҵ��Ѵ�.
 *
 * FLI Page�� Next Free Page ID�� ��ũ�� �������鿡 ����
 * PCH���� Page �޸𸮸� �Ҵ��ϰ� Page Header�� Prev/Next�����͸� �����Ѵ�.
 *
 * Free List Info Page���� Next Free Page ID�� �������
 * PCH�� Page���� Page Header�� Prev/Next��ũ�� �����Ѵ�.
 *
 * Free List Info Page�� Ư���� ���� �����Ͽ� �Ҵ�� ���������� ǥ���Ѵ�.
 *
 * Free Page�� ���� �Ҵ�Ǳ� ���� �Ҹ���� ��ƾ����, ���� Free Page����
 * PCH�� Page �޸𸮸� �Ҵ��Ѵ�.
 *
 * aTrans     [IN] Page�� �Ҵ�ް��� �ϴ� Ʈ�����
 * aHeadPID   [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPID   [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC smmManager::allocFreePageMemoryList( smmTBSNode * aTBSNode,
                                            void       * aTrans,
                                            scPageID     aHeadPID,
                                            scPageID     aTailPID,
                                            vULong     * aPageCount )
{
    scPageID   sPrevPID = SM_NULL_PID;
    scPageID   sNextPID;
    scPageID   sPID;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( isValidPageID( aTBSNode->mTBSAttr.mID, aHeadPID )
                 == ID_TRUE );
    IDE_DASSERT( isValidPageID( aTBSNode->mTBSAttr.mID, aTailPID )
                 == ID_TRUE );
    IDE_DASSERT( aPageCount != NULL );

    vULong   sProcessedPageCnt = 0;

    // BUGBUG kmkim �� Page�� Latch�� �ɾ�� �ϴ��� ��� �ʿ�.
    // ��¦�� �����غ� ����δ� Latch���� �ʿ� ����..

    // sHeadPage���� sTailPage������ ��� Page�� ����
    sPID = aHeadPID;
    while ( sPID != SM_NULL_PID )
    {
        // Next Page ID ����
        if ( sPID == aTailPID )
        {
            // ���������� link�� Page��� ���� Page�� NULL
            sNextPID = SM_NULL_PID ;
        }
        else
        {
            // �������� �ƴ϶�� ���� Page Free Page ID�� ���´�.
            IDE_TEST( smmExpandChunk::getNextFreePage( aTBSNode,
                                                       sPID,
                                                       & sNextPID )
                      != IDE_SUCCESS );
        }

        // Free Page�̴��� PCH�� �Ҵ�Ǿ� �־�� �Ѵ�.
        IDE_ASSERT( mPCArray[aTBSNode->mTBSAttr.mID].mPC[sPID].mPCH != NULL );

        // To Fix BUG-15107 Checkpoint�� Dirty Pageó������
        //                  ������ ������ ����
        // => Page�޸� �Ҵ�� �ʱ�ȭ�� mMutex�� ���
        //    Checkpoint�ÿ� �Ҵ縸 �ǰ� �ʱ�ȭ���� ���� �޸𸮸�
        //    ���� ���� ������ �Ѵ�.
        //
        // ������ �޸𸮸� �Ҵ��ϰ� �ʱ�ȭ
        IDE_TEST( allocAndLinkPageMemory( aTBSNode,
                                          aTrans, // �α�ǽ�
                                          sPID,
                                          sPrevPID,
                                          sNextPID ) != IDE_SUCCESS );

        // ���̺� �Ҵ�Ǿ��ٴ� �ǹ̷�
        // Page�� Next Free Page�� Ư���� ���� ����صд�.
        // ���� �⵿�� Page�� ���̺� �Ҵ�� Page����,
        // Free Page���� ���θ� �����ϱ� ���� ���ȴ�.
        IDE_TEST( smmExpandChunk::logAndSetNextFreePage(
                      aTBSNode,
                      aTrans, // �α�ǽ�
                      sPID,
                      SMM_FLI_ALLOCATED_PID )
                  != IDE_SUCCESS );

        sProcessedPageCnt ++ ;


        sPrevPID = sPID ;

        // sPID �� aTailPID�� ��,
        // ���⿡�� sPID�� SM_NULL_PID �� �����Ǿ� loop ����
        sPID = sNextPID ;
    }


    * aPageCount = sProcessedPageCnt;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/* PCH�� Page���� Page Header�� Prev/Next�����͸� �������
 * FLI Page�� Next Free Page ID�� �����Ѵ�.
 *
 * ���̺� �Ҵ�Ǿ��� Page�� Free Page�� �ݳ��Ǳ� ���� ����
 * �Ҹ���� ��ƾ�̴�.
 *
 * aTrans     [IN] �����ϰ��� �ϴ� Ʈ�����
 * aHeadPage  [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPage  [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC smmManager::linkFreePageList( smmTBSNode * aTBSNode,
                                     void       * aTrans,
                                     void       * aHeadPage,
                                     void       * aTailPage,
                                     vULong     * aPageCount )
{
    scPageID    sPID, sTailPID, sNextPID;
    vULong      sProcessedPageCnt = 0;
    void      * sPagePtr;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );
    IDE_DASSERT( aPageCount != NULL );

    sPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );
    // sHeadPage���� sTailPage������ ��� Page�� ����

    do
    {
        if ( sPID == sTailPID ) // ������ �������� ���
        {
            sNextPID = SM_NULL_PID ;
        }
        else  // ������ �������� �ƴ� ���
        {
            // Free List Info Page�� Next Free Page ID�� ����Ѵ�.
            IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                                    sPID,
                                                    &sPagePtr )
                        == IDE_SUCCESS );
            sNextPID = smLayerCallback::getNextPersPageID( sPagePtr );
        }

        // Free List Info Page�� Next Free Page ID ����
        IDE_TEST( smmExpandChunk::logAndSetNextFreePage( aTBSNode,
                                                         aTrans,  // �α�ǽ�
                                                         sPID,
                                                         sNextPID )
                  != IDE_SUCCESS );

        sProcessedPageCnt ++ ;

        sPID = sNextPID ;
    } while ( sPID != SM_NULL_PID );

    * aPageCount = sProcessedPageCnt;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}


/* PCH�� Page���� Page Header�� Prev/Next�����͸� �������
 * PCH �� Page �޸𸮸� �ݳ��Ѵ�.
 *
 * ���̺� �Ҵ�Ǿ��� Page�� Free Page�� �ݳ��� �Ŀ�
 * �Ҹ���� ��ƾ����, Page���� PCH�� Page �޸𸮸� �����Ѵ�.
 *
 * aHeadPage  [IN] �����ϰ��� �ϴ� ù��° Free Page
 * aTailPage  [IN] �����ϰ��� �ϴ� ������ Free Page
 * aPageCount [OUT] ����� �� ������ ��
 */
IDE_RC smmManager::freeFreePageMemoryList( smmTBSNode * aTBSNode,
                                           void       * aHeadPage,
                                           void       * aTailPage,
                                           vULong     * aPageCount )
{
    scPageID    sPID, sTailPID, sNextPID;
    vULong      sProcessedPageCnt = 0;
    void      * sPagePtr;

    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );
    IDE_DASSERT( aPageCount != NULL );

    sPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );
    // sHeadPage���� sTailPage������ ��� Page�� ����

    do
    {
        if ( sPID == sTailPID ) // ������ �������� ���
        {
            sNextPID = SM_NULL_PID ;
        }
        else  // ������ �������� �ƴ� ���
        {
            // Free List Info Page�� Next Free Page ID�� ����Ѵ�.
            IDE_ASSERT( smmManager::getPersPagePtr( aTBSNode->mTBSAttr.mID, 
                                                    sPID,
                                                    &sPagePtr )
                        == IDE_SUCCESS );
            sNextPID = smLayerCallback::getNextPersPageID( sPagePtr );
        }

        IDE_TEST( freePageMemory( aTBSNode, sPID ) != IDE_SUCCESS );

        sProcessedPageCnt ++ ;

        sPID = sNextPID ;
    } while ( sPID != SM_NULL_PID );

    * aPageCount = sProcessedPageCnt;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}




/** DB�κ��� Page�� ������ �Ҵ�޴´�.
 *
 * �������� Page�� ���ÿ� �Ҵ������ DB Page�Ҵ� Ƚ���� ���� �� ������,
 * �̸� ���� DB Free Page List ���� ���ü��� ����ų �� �ִ�.
 *
 * ���� Page���� ���� �����ϱ� ���� aHeadPage���� aTailPage����
 * Page Header�� Prev/Next�����ͷ� �������ش�.
 *
 * ���� ! �� �Լ����� NTA�α� ���� �� �����ϸ�, Logical Undo�Ǿ�
 * freePersPageList�� ȣ��ȴ�. �׷��Ƿ� aHeadPage���� aTailPage����
 * Page �޸𸮾��� Next ��ũ�� �������� �ȵȴ�.
 *
 * aTrans     [IN] �������� �Ҵ���� Ʈ����� ��ü
 * aPageCount [IN] �Ҵ���� �������� ��
 * aHeadPage  [OUT] �Ҵ���� ������ �� ù��° ������
 * aTailPage  [OUT] �Ҵ���� ������ �� ������ ������
 */
IDE_RC smmManager::allocatePersPageList (void      *  aTrans,
                                         scSpaceID    aSpaceID,
                                         UInt         aPageCount,
                                         void     **  aHeadPage,
                                         void     **  aTailPage,
                                         UInt      *  aAllocPageCnt )
{

    scPageID  sHeadPID = SM_NULL_PID;
    scPageID  sTailPID = SM_NULL_PID;
    vULong    sLinkedPageCount;
    smLSN     sNTALSN ;
    UInt      sPageListID;
    UInt      sStage = 0;
    UInt      sState = 0;
    smmTBSNode * sTBSNode;
    UInt      sPageCount = 0;
    UInt      sTotalPageCount = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aPageCount != 0 );
    IDE_DASSERT( aHeadPage != NULL );
    IDE_DASSERT( aTailPage != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sTBSNode)
              != IDE_SUCCESS );

    sNTALSN = smLayerCallback::getLstUndoNxtLSN( aTrans );

    // �������� Free Page List�� �ϳ��� �����Ѵ�.
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    if( aPageCount != 1 )
    {
        /* BUG-46861 TABLE_ALLOC_PAGE_COUNT ������Ƽ�� ����� �ټ��� �������� �Ҵ��� ��� �ټ��� ������ �Ҵ�����
           �Ҵ�� �������� MEM_MAX_DB_SIZE�� �Ѿ�� �ʵ��� �����ؾ� �Ѵ�.
           ����, TBS lock�� ���� �ʰ� ���� ������� DB�� ũ�⸦ üũ�ϱ� ������
           ���� ������� ����+���� �Ҵ��� ������ �ִ� ��� ���� ������ �������� �ʾƵ�
           �׿� ������ ���(1�� Chunk������ ������ ���̳� ���)���� ����ó�� 1���� �Ҵ��Ѵ�. */

        IDE_TEST( smmFPLManager::lockGlobalPageCountCheckMutex() != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( smmFPLManager::getTotalPageCount4AllTBS( & sTotalPageCount ) != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( smmFPLManager::unlockGlobalPageCountCheckMutex() != IDE_SUCCESS );

        if( ( smiGetStartupPhase() == SMI_STARTUP_SERVICE ) &&
                ( ( sTotalPageCount + aPageCount ) < 
                  ( ( smuProperty::getMaxDBSize() / SM_PAGE_SIZE ) - sTBSNode->mMemBase->mExpandChunkPageCnt ) ) )
        {
            sPageCount = aPageCount;
        }
        else
        {
            /* �Ҵ��� ����+�Ҵ��� ������ MEM_MAX_DB_SIZE�� �����ϰų� ���� ���� �߿��� ����ó�� �ϳ��� �Ҵ��Ѵ�. */
            sPageCount = 1;
        }
    }
    else
    {
        sPageCount = 1;
    }

    // sPageListID�� �ش��ϴ� Free Page List�� �ּ��� aPageCount����
    // Free Page�� �������� �����ϸ鼭 latch�� ȹ���Ѵ�.
    //
    // aPageCount��ŭ Free Page List�� ������ �����ϱ� ���ؼ�
    //
    // 1. Free Page List���� Free Page���� �̵���ų �� �ִ�. => Physical �α�
    // 2. Expand Chunk�� �Ҵ��� �� �ִ�.
    //     => Chunk�Ҵ��� Logical �α�.
    //       -> Recovery�� smmManager::allocNewExpandChunkȣ���Ͽ� Logical Redo
    IDE_TEST( smmFPLManager::lockListAndPreparePages( sTBSNode,
                                                      aTrans,
                                                      (smmFPLNo)sPageListID,
                                                      sPageCount )
              != IDE_SUCCESS );
    sStage = 1;

    // Ʈ������� ����ϴ� Free Page List���� Free Page���� �����.
    // DB Free Page List�� ���� �α��� ���⿡�� �̷������.
    IDE_TEST( smmFPLManager::removeFreePagesFromList( sTBSNode,
                                                      aTrans,
                                                      (smmFPLNo)sPageListID,
                                                      sPageCount,
                                                      & sHeadPID,
                                                      & sTailPID )
              != IDE_SUCCESS );

    // Head���� Tail���� ��� Page�� ����
    // Page Header�� Prev/Next ��ũ�� ���� �����Ų��.
    IDE_TEST( allocFreePageMemoryList ( sTBSNode,
                                        aTrans,
                                        sHeadPID,
                                        sTailPID,
                                        & sLinkedPageCount )
              != IDE_SUCCESS );

    IDE_ASSERT( sLinkedPageCount == sPageCount );

    IDE_ASSERT( smmManager::getPersPagePtr( sTBSNode->mTBSAttr.mID, 
                                            sHeadPID,
                                            aHeadPage )
                == IDE_SUCCESS );
    IDE_ASSERT( smmManager::getPersPagePtr( sTBSNode->mTBSAttr.mID, 
                                            sTailPID,
                                            aTailPage )
                == IDE_SUCCESS );

    // write NTA
    IDE_TEST( smLayerCallback::writeAllocPersListNTALogRec( NULL, /* idvSQL* */
                                                            aTrans,
                                                            & sNTALSN,
                                                            sTBSNode->mTBSAttr.mID,
                                                            sHeadPID,
                                                            sTailPID )
              != IDE_SUCCESS );



    // �������� �Ҵ���� Free Page List�� Latch�� Ǯ���ش�.
    sStage = 0;
    IDE_TEST( smmFPLManager::unlockFreePageList( sTBSNode,
                                                 (smmFPLNo)sPageListID )
              != IDE_SUCCESS );

    *aAllocPageCnt = sPageCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sStage )
    {
        case 1:
            // Fatal ������ �ƴ϶��, NTA �Ϲ�
            IDE_ASSERT( smLayerCallback::undoTrans( NULL, /* idvSQL* */
                                                    aTrans,
                                                    &sNTALSN )
                        == IDE_SUCCESS );

            IDE_ASSERT( smmFPLManager::unlockFreePageList(
                            sTBSNode,
                            (smmFPLNo)sPageListID ) == IDE_SUCCESS );
            break;
        default:
            break;
    }

    switch( sState )
    {
        case 1:
            IDE_ASSERT( smmFPLManager::unlockGlobalPageCountCheckMutex()
                        == IDE_SUCCESS );
            break;

        default:
            break;
    }

    IDE_POP();


    return IDE_FAILURE;
}


/*
 * �������� Page�� �Ѳ����� �����ͺ��̽��� �ݳ��Ѵ�.
 *
 * aHeadPage���� aTailPage����
 * Page Header�� Prev/Next�����ͷ� ����Ǿ� �־�� �Ѵ�.
 *
 * ���� Free Page���� ���� ���� Free Page List �� Page�� Free �Ѵ�.
 *
 * aTrans    [IN] Page�� �ݳ��Ϸ��� Ʈ�����
 * aHeadPage [IN] �ݳ��� ù��° Page
 * aHeadPage [IN] �ݳ��� ������ Page
 * aNTALSN   [IN] NTA�α׸� ���� �� ����� NTA���� LSN
 *
 */
IDE_RC smmManager::freePersPageList (void       * aTrans,
                                     scSpaceID    aSpaceID,
                                     void       * aHeadPage,
                                     void       * aTailPage,
                                     smLSN      * aNTALSN )
{
    UInt            sPageListID;
    vULong          sLinkedPageCount;
    vULong          sFreePageCount;
    scPageID        sHeadPID;
    scPageID        sTailPID;
    UInt            sStage      = 0;
    idBool          sIsInNTA    = ID_TRUE;
    smmTBSNode    * sTBSNode;

    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aHeadPage  != NULL );
    IDE_DASSERT( aTailPage  != NULL );
    IDE_DASSERT( aNTALSN    != NULL );

    IDE_TEST(sctTableSpaceMgr::findSpaceNodeBySpaceID(aSpaceID,
                                                      (void**)&sTBSNode)
             != IDE_SUCCESS);

    sHeadPID = smLayerCallback::getPersPageID( aHeadPage );
    sTailPID = smLayerCallback::getPersPageID( aTailPage );

    // Free List Info Page�ȿ� Free Page���� Link�� ����Ѵ�.
    IDE_TEST( linkFreePageList( sTBSNode,
                                aTrans,
                                aHeadPage,
                                aTailPage,
                                & sLinkedPageCount )
              != IDE_SUCCESS );

    // Free Page�� �ݳ��Ѵ�.
    // �̷��� �ص� allocFreePage���� Page���� ���� Free Page List����
    // ������ ������� ��ƾ������ Free Page List���� �뷱���� �ȴ�.

    // �������� Free Page List�� �ϳ��� �����Ѵ�.
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    // Page�� �ݳ��� Free Page List�� Latchȹ��
    IDE_TEST( smmFPLManager::lockFreePageList(sTBSNode, (smmFPLNo)sPageListID)
              != IDE_SUCCESS );
    sStage = 1;

    // Free Page List �� Page�� �ݳ��Ѵ�.
    // DB Free Page List�� ���� �α��� �߻��Ѵ�.
    IDE_TEST( smmFPLManager::appendFreePagesToList(
                                   sTBSNode,
                                   aTrans,
                                   sPageListID,
                                   sLinkedPageCount,
                                   sHeadPID,
                                   sTailPID,
                                   ID_TRUE, // aSetFreeListOfMembase
                                   ID_TRUE )// aSetNextFreePageOfFPL
              != IDE_SUCCESS );

    // write NTA
    IDE_TEST( smLayerCallback::writeNullNTALogRec( NULL, /* idvSQL* */
                                                   aTrans,
                                                   aNTALSN )
              != IDE_SUCCESS );


    sIsInNTA = ID_FALSE;

    // Page Memory Free�� NTA ����ǰ� ó���ϵ��� �Ѵ�
    //
    // �ֳ��ϸ�, Page Memory Free�ع����� ���� NTA�����ϸ�
    // Page�޸� �ٽ� �Ҵ�޾� DB���Ͽ��� �ε��ؾ� �ϴµ�,
    // �� �۾��� ����ġ �ʱ� �����̴�.
    if ( smLayerCallback::isRestartRecoveryPhase() == ID_FALSE )
    {
        // Restart Recovery���� �ƴҶ����� ������ �޸𸮸� free�Ѵ�.
        // Restart Recovery�߿��� free�� ������ �޸𸮿� ����
        // Redo/Undo�� �� �� �ֱ� �����̴�.

        IDE_TEST( freeFreePageMemoryList( sTBSNode,
                                          aHeadPage,
                                          aTailPage,
                                          & sFreePageCount )
                  != IDE_SUCCESS );

        IDE_ASSERT( sFreePageCount == sLinkedPageCount );
    }

    // ����! Page �ݳ� ���� Logging�� �ϰ�
    // Page Free�� �ϱ� ������ Flush�� �� �ʿ䰡 ����.

    sStage = 0;
    // Free Page List���� LatchǬ��
    IDE_TEST( smmFPLManager::unlockFreePageList( sTBSNode,
                                                 (smmFPLNo)sPageListID )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsInNTA == ID_TRUE )
    {
        sIsInNTA = ID_FALSE;
        // NTA�ȿ��� ������ �߻��ߴٸ� ���� �ҹ��ϰ�, NTA �ѹ�
        IDE_ASSERT( smLayerCallback::undoTrans( NULL, /* idvSQL* */
                                                aTrans,
                                                aNTALSN )
                    == IDE_SUCCESS );
    }

    switch( sStage )
    {
        case 1:

            IDE_ASSERT( smmFPLManager::unlockFreePageList(
                            sTBSNode,
                            (smmFPLNo)sPageListID )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }
    sStage = 0;

    IDE_POP();


    return IDE_FAILURE;
}


/* --------------------------------------------------------------------------
 *  SECTION : Latch Control
 * -------------------------------------------------------------------------*/

/*
 * Ư�� Page�� S��ġ�� ȹ���Ѵ�. ( ����� X��ġ�� �����Ǿ� �ִ� )
 */
IDE_RC
smmManager::holdPageSLatch(scSpaceID aSpaceID,
                           scPageID  aPageID )
{
    smmPCH            * sPCH ;
    smpPersPageHeader * sMemPagePtr;

    IDE_DASSERT( isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    IDE_TEST( smmManager::getPersPagePtr( aSpaceID,
                                          aPageID,
                                          (void**) &sMemPagePtr )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( SMP_GET_PERS_PAGE_INCONSISTENCY( sMemPagePtr ) 
                    == SMP_PAGEINCONSISTENCY_TRUE,
                    ERR_INCONSISTENT_PAGE );

    sPCH = getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH != NULL );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.lockRead( NULL,/* idvSQL* */
                                  NULL ) /* sWeArgs*/
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_PAGE )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aSpaceID,
                                  aPageID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * Ư�� Page�� X��ġ�� ȹ���Ѵ�.
 */
IDE_RC
smmManager::holdPageXLatch(scSpaceID aSpaceID,
                           scPageID  aPageID)
{
    smmPCH * sPCH;

    IDE_DASSERT( isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    sPCH = getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH != NULL );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.lockWrite( NULL,/* idvSQL* */
                                             NULL ) /* sWeArgs*/
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * Ư�� Page���� ��ġ�� Ǯ���ش�.
 */
IDE_RC
smmManager::releasePageLatch(scSpaceID aSpaceID,
                             scPageID  aPageID)
{
    smmPCH * sPCH;

    IDE_DASSERT( isValidPageID( aSpaceID, aPageID ) == ID_TRUE );

    sPCH = getPCH( aSpaceID, aPageID );

    IDE_DASSERT( sPCH );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sPCH->mPageMemLatch.unlock( ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    �Ϲ� �޸� Page Pool�� �ʱ�ȭ�Ѵ�.

    [IN] aTBSNode - Page Pool�� �ʱ�ȭ�� ���̺� �����̽�
*/
IDE_RC smmManager::initializeDynMemPool(smmTBSNode * aTBSNode)
{
    // BUG-47487: FLI ������ MemPool �ʱ�ȭ
    IDE_TEST(aTBSNode->mFLIMemPagePool.initialize(
                IDU_MEM_SM_SMM,
                (SChar*)"TEMP_MEMORY_POOL",
                1, /* MemList Count */
                SM_PAGE_SIZE,
                smuProperty::getTempPageChunkCount(),
                0, /* Cache Size */
                iduProperty::getDirectIOPageSize() /* Align Size */)
        != IDE_SUCCESS);

    /* BUG-16885: Stroage_Memory_Manager������ �޸𸮰� ������������
     * Ŀ���� ��찡 �߻�
     * Free Page List�� �������̴� ���� �ٸ� ����Ʈ�� 100M�� Free Page
     * �� �ִ��� �ڽ��� �����ϴ� ����Ʈ�� Memory�� ������ ���� �޸�
     * �� �Ҵ��Ͽ� ������������ �޸𸮰� Ŀ��. �׷��� mDynamicMemPagePool
     * �� ���ķ� �Ҵ��ϴ� ���� ���⶧���� ����Ʈ ������ 1�� ������.
     *
     * �Ʒ� mIndexMemPool�� �������� ������ ����Ʈ ������ 1�� ������.
     */
    IDE_TEST(aTBSNode->mDynamicMemPagePool.initialize(
                 IDU_MEM_SM_SMM,
                 (SChar*)"TEMP_MEMORY_POOL",
                 1, /* MemList Count */
                 SM_PAGE_SIZE,
                 smuProperty::getTempPageChunkCount(),
                 0, /* Cache Size */
                 iduProperty::getDirectIOPageSize() /* Align Size */)
             != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace���� ����� Page �޸� Ǯ�� �ʱ�ȭ�Ѵ�.

    �����޸� Key�� ���� ���ο� ����
    �����޸� Ȥ�� �Ϲݸ޸𸮸� ����ϵ��� Page Pool�� �ʱ�ȭ �Ѵ�.
 */
IDE_RC smmManager::initializePagePool( smmTBSNode * aTBSNode )
{

    if ( smuProperty::getShmDBKey() != 0) // �����޸� ����
    {
        // �����޸� Page Pool �ʱ�ȭ
        IDE_TEST( initializeShmMemPool( aTBSNode ) != IDE_SUCCESS );

        // ù��° ���� �޸� Chunk ����
        // ũ�� : SHM_PAGE_COUNT_PER_KEY ������Ƽ�� ������ Page����ŭ
        // TBSNode�� �� ù��° �����޸� Key�� �����ȴ�.
        IDE_TEST(smmFixedMemoryMgr::createFirstChunk(
                     aTBSNode,
                     smuProperty::getShmPageCountPerKey() )
                 != IDE_SUCCESS);

        aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_SHM_CREATE;
    }
    else // �Ϲݸ޸� ����
    {
        IDE_TEST( initializeDynMemPool( aTBSNode ) != IDE_SUCCESS );

        aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_DYNAMIC;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace�� �Ҵ�� Page Pool�� �ı��Ѵ�.

    [ �˰��� ]

    if ( �����޸� ��� )
       if ( Drop/Offline�� Tablespace )
          (010) �����޸� remove ( �ý��ۿ��� ���� )
       else // Shutdown�ϴ� ���
          (020) �����޸� detach
       fi
       (030) �����޸� Page ������ �ı�
    else // �Ϲ� �޸� ���
       (040) �Ϲݸ޸� Page ������ �ı�
    fi

    aTBSNode [IN] Page Pool�� �ı��� ���̺� �����̽�
 */
IDE_RC smmManager::destroyPagePool( smmTBSNode * aTBSNode )
{
    switch( aTBSNode->mRestoreType )
    {
        case SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET :
            // PAGE�ܰ���� �ʱ�ȭ �Ͽ�����,
            // prepare/restore���� ���� tablespace.
            //
            // ���� page memory pool�� �ʱ�ȭ���� ���� ����.
            // �ƹ��͵� �������� �ʴ´�.
            break;

        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
        case SMM_DB_RESTORE_TYPE_SHM_ATTACH :
            // Drop/Offline�� Pending���� ȣ��� ���
            // ȣ��� ��� �����޸� ��ü�� �����Ѵ�
            if ( sctTableSpaceMgr::hasState(
                     & aTBSNode->mHeader,
                     SCT_SS_FREE_SHM_PAGE_ON_DESTROY )
                 == ID_TRUE )
            {
                /////////////////////////////////////////////////
                // (010) �����޸� remove ( �ý��ۿ��� ���� )
                //
                // ���� �޸� ����� ��� �����޸𸮿����� �����ϰ�
                // �����޸� Key�� ���� Key�� ���
                IDE_TEST( smmFixedMemoryMgr::remove( aTBSNode )
                          != IDE_SUCCESS );
            }
            else
            {
                /////////////////////////////////////////////////
                // (020) �����޸� detach
                //
                // �����޸� Detach�ǽ�
                IDE_TEST( smmFixedMemoryMgr::detach( aTBSNode )
                          != IDE_SUCCESS );
            }

            /////////////////////////////////////////////////////
            // (030) �����޸� Page ������ �ı�
            //
            // TBSNode���� �����޸� ���� ���� ����
            IDE_TEST( smmFixedMemoryMgr::destroy( aTBSNode ) != IDE_SUCCESS );

            break;

        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            ////////////////////////////////////////////////////
            // (040) �Ϲݸ޸� Page ������ �ı�
            IDE_TEST(aTBSNode->mDynamicMemPagePool.destroy() != IDE_SUCCESS);
            // BUG-47487: FLI ������ memPool �ı�
            IDE_TEST(aTBSNode->mFLIMemPagePool.destroy() != IDE_SUCCESS);

            break;
        default :
            IDE_ASSERT(0);
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * smmManager::prepareDB�� ���� Action�Լ�
 */
IDE_RC smmManager::prepareTBSAction( idvSQL            * /*aStatistics*/,
                                     sctTableSpaceNode * aTBSNode,
                                     void              * aActionArg )
{
    idBool   sDoIt;

    IDE_DASSERT( aTBSNode != NULL );

    // CONTROL�ܰ迡�� DROPPED�� TBS�� loganchor�κ���
    // �о������ �ʱ� ������
    // PREPARE/RESTORE�߿� DROPPED������ TBS�� ���� �� ����.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_DROPPED ) != SMI_TBS_DROPPED );

    // Memory Tablespace�� DISCARD,OFFLINE Tablespace�� �����ϰ�
    // PREPARE/RESTORE�� �����Ѵ�.
    if(( sctTableSpaceMgr::isMemTableSpace(aTBSNode->mID) == ID_TRUE ) &&
       ( sctTableSpaceMgr::hasState(aTBSNode->mID, SCT_SS_SKIP_PREPARE )
         == ID_FALSE) )
    {
        IDE_DASSERT( ID_SIZEOF(void*) >= ID_SIZEOF(smmPrepareOption) );

        // �̵����� �����ϱ� ���� Startup Control �ܰ迡��
        // Memory TableSpace ��� �ʱ�ȭ�� Startup Control�ܰ迡��
        // LogAnchor �ʱ�ȭ �������� ó���ȴ�.

        switch ( (smmPrepareOption)(vULong)aActionArg )
        {
            case SMM_PREPARE_OP_DBIMAGE_NEED_MEDIA_RECOVERY:
            {
                IDE_TEST_RAISE ( smuProperty::getShmDBKey() != 0,
                    error_media_recovery_is_not_support_shm );

                if ( (isMediaFailureTBS( (smmTBSNode*)aTBSNode ) == ID_TRUE) ||
                     (aTBSNode->mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC) )
                {
                    // �̵��� �÷��װ� ID_TRUE�� ������ ����Ÿ������
                    // ���� ��� Prepare�� �����Ѵ�.
                    // ����, 0�� TBS�� ��� DicMemBase �ε��� ����
                    // ������ �Ѵ�.
                    sDoIt = ID_TRUE;
                }
                else
                {
                   // prepare ���� �ʴ´�.
                   sDoIt = ID_FALSE;
                }
                break;
            }
            default:
            {
                 // ����������
                sDoIt = ID_TRUE;
                break;
            }
        }

        if ( sDoIt == ID_TRUE )
        {
            IDE_TEST( prepareTBS( (smmTBSNode*) aTBSNode,
                        (smmPrepareOption)(vULong) aActionArg )
                    != IDE_SUCCESS );
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_media_recovery_is_not_support_shm );
    {
        IDE_SET( ideSetErrorCode(
                 smERR_ABORT_MEDIA_RECOVERY_IS_NOT_SUPPORT_SHARED_MEMORY) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * �����ͺ��̽� restore(Disk -> �޸𸮷� �ε� )�� ���� �غ��Ѵ�.
 *
 * aOp          [IN] Prepare �ɼ�/����
 */

IDE_RC smmManager::prepareDB ( smmPrepareOption aOp )
{

    IDE_DASSERT( ID_SIZEOF(void*) >= ID_SIZEOF(smmPrepareOption) );

    // ���� ������ �Ǵ� �̵�� ������
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  prepareTBSAction,
                                                  (void*) aOp,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
   Tablespace restore(Disk=>Memory)�� ���� �غ��۾��� �����Ѵ�.

   aTBSNode     [IN] �غ��۾��� ������ Tablespace
   aTempMemBase [IN] 0�� Page�� �о���� Buffer
                     �� �Լ��� ����� �Ŀ��� �� Buffer���� Membase��
                     ������ �ؾ� �ϱ� ������ �� �Լ����� stack������
                     ���� �� ��� ���ڷ� �޴´�.
   aOp          [IN] prepare option


   ������� -----------------------------------------------------------------
   - Restore ����� ����
     - �Ϲݸ޸� ��� ( SMM_DB_RESTORE_TYPE_DYNAMIC )
       - �Ϲ� �޸𸮿� Disk=>Memory�� Page�ε��ϴ� I/O�� ����
     - �����޸� ���
       - �����޸� Attach ( SMM_DB_RESTORE_TYPE_SHM_ATTACH )
         - ���� �����޸𸮿� Disk Image�� �ö�� �ִ� ��Ȳ
         - �����޸� Attach�� �����ϸ�,
           Disk=> Memory���� Page�� �ε��ϴ� I/O�� �ʿ����
       - �����޸� Create ( SMM_DB_RESTORE_TYPE_SHM_CREATE )
         - �����޸𸮸� ���� �����ϰ� Disk=>Memory����
           Page�� �ε��ϴ� I/O�� ����

   PROJ-1548 User Memory Tablespace ------------------------------------------

   [ �����޸� ���� Design ]

   �������� Tablespace�� �Ѱ����� ������ SHM_DB_KEY��� ������Ƽ��
   ����� �����޸� Ű�κ��� �����Ͽ� �����޸� ��������
   ��Ƽ���̽��� Attach�Ͽ���.

   Memory Tablespace�� �������� �ʿ� ����, ������ Tablespace����
   �̿� ���� ���� �����޸� Key�� �ʿ��ϴ�.

   Tablespace�� ���� �����޸� Key�� ������ ���ȴٰ� �÷��� ��
   �ش� Tablespace ù��° �����޸� ������ Attach�ϱ� ����
   �� �ʿ��ϴ�.

   �׷��� �� Tablespace ���� ���� �����޸� Key�� Durable�ϰ�
   ������ �ʿ䰡 �ִ�.

   Log Anchor�� Tablespace������ �� Tablespace���� ���� �����޸� Key��
   �����ϵ��� �Ѵ�.

   ��������
     - �Ϻ� Tablespace���� �����޸� ���� ��� �� ����
       ( ���⵵�� ���߱� ���� �����̸� ���� �߰����� ���� )
       - ��� Tablespace�� ���� �޸𸮷� ���ų�,
         ��� Tablespace�� �Ϲ� �޸𸮷� ��� �� �ִ�.
     - �Ϻ� Tablespace���� Attach�ϰ� �������� Create�� �� ����
       - ��� Tablespace�� �����޸𸮷κ��� Attach�ϰų�,
         ��� Tablespace�� �����޸𸮸� ���� �����ؾ� �Ѵ�.

   �ڷᱸ��
     - LogAnchor
       - TBSNode
         - TBSAttr
           - ShmKey : ���̺� �����̽��� ���� �����޸� Key
                      Log Anchor�� Durable�ϰ� ����ȴ�.

   ����
     - �����޸� ����� ���ؼ��� ����ڰ� SHM_DB_KEY������Ƽ��
       ���ϴ� �����޸� Key������ �����Ѵ�.

     - �� Tablespace�� �����޸� Key�� �ý����� �ڵ����� �����Ѵ�

   �˰��� ( restore����� ���� )
     - 1. SHM_DB_KEY == 0 �̸� �Ϲ� �޸𸮸� ����Ѵ�.
       - LogAnchor�� ShmKey := 0 ���� ( ���� ��� TBS�� ���� �ϰ� Flush )
     - 2. SHM_DB_KEY != 0 �̸�
        - 2.1 TBSNode.ShmKey �� �ش��ϴ� �����޸� ������ ����?
             Attach�ǽ�
        - 2.2 �����޸� ������ �������� ������
          - SHM_DB_KEY�κ��� 1�� �����ذ��� �����޸� ���� �ű� ����
 */

IDE_RC smmManager::prepareTBS (smmTBSNode *      aTBSNode,
                               smmPrepareOption  aOp )
{
    // ù��° prepare�� TBS�� restore mode
    // ��� tablespace�� restore mode�� �̿� ���ƾ� �Ѵ�.
    //
    // ( �Լ� ���ۺκ� �ּ��� ����� ������� üũ�� ���� ���ȴ�. )
    static smmDBRestoreType sFirstRestoreType = SMM_DB_RESTORE_TYPE_NONE;

    key_t             sTbsShmKey;
    smmShmHeader      sShmHeader;
    idBool            sShmExist;

    IDE_DASSERT( aTBSNode != NULL );

    /* -------------------------------
     * [3] Recovery ���� callback ����
     * ----------------------------- */
    if (smuProperty::getShmDBKey() == 0)
    {
        aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_DYNAMIC ;
        aTBSNode->mTBSAttr.mMemAttr.mShmKey = 0 ;

        // Restore��ģ ���� Restart Redo/Undo�� ������
        // Log Anchor�� ��� TBS�� ShmKey�� Flush�ȴ�.

        // BUGBUG-1548 ���� Flush�ϴ��� üũ�� ��

        // �Ϲ� �޸� Page Pool �ʱ�ȭ
        IDE_TEST( initializeDynMemPool(aTBSNode) != IDE_SUCCESS);
    }
    else
    {
        /* ------------------------------------------------
         * [3-1] Fixed Memory Manager Creation
         * ----------------------------------------------*/
        IDE_TEST( initializeShmMemPool(aTBSNode)
                  != IDE_SUCCESS);

        /* ------------------------------------------------
         * [3-2] Exist Check & Set
         * ----------------------------------------------*/
        sTbsShmKey = aTBSNode->mTBSAttr.mMemAttr.mShmKey;

        // ������ TBS�� Dynamic Memory�� Restore�� ��� TBSNode�� ShmKey=0
        if ( sTbsShmKey == 0 )
        {
            sShmExist = ID_FALSE;
        }
        else
        {
            IDE_TEST(smmFixedMemoryMgr::checkExist(
                         sTbsShmKey,
                         sShmExist,
                         &sShmHeader) != IDE_SUCCESS);
        }

        if ( sShmExist == ID_FALSE )
        {
            // Just Attach Shared Memory
            aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_SHM_CREATE;
        }
        else
        {
            aTBSNode->mRestoreType = SMM_DB_RESTORE_TYPE_SHM_ATTACH;
        }
    }

    if ( (aOp & SMM_PREPARE_OP_DONT_CHECK_RESTORE_TYPE) ==
         SMM_PREPARE_OP_DONT_CHECK_RESTORE_TYPE )
    {
        // ��� Tablespace�� ���� Restore Type���� Restore�Ǵ���
        // ����üũ�� ���� ����

        // ALTER TABLESPACE ONLINE�ÿ� ����� ���´�.
        // Startup�ÿ��� Shared Memory Attach�Ǿ��� Tablespace��
        // Alter Tablespace Offline�� Shared Memory�� ������ �����ϰ�
        // Alter Tablespace Online�� Shared Memory Create�� Restore�� �� �ֱ� ����

        // Do Nothing.
    }
    else
    {
        // �������� üũ
        if ( sFirstRestoreType == SMM_DB_RESTORE_TYPE_NONE ) // �� ó�� prepare?
        {
            sFirstRestoreType = aTBSNode->mRestoreType;

            // To Fix BUG-17293 Server Startup�� Tablespace������ŭ
            //                  Loading�޽����� ����
            // => �� Tablespace Loading�ø��� Message�� ������� �ʰ�,
            //    ������ Server start�� �� �ѹ��� Loading Message���.
            printLoadingMessage( sFirstRestoreType );
        }
        else // �ι� ° ���� prepare ?
        {
            // �� ó���� �׻� ���� Restore��忩�� ��
            IDE_TEST_RAISE( aTBSNode->mRestoreType != sFirstRestoreType,
                            error_invalid_shm_region );
        }
    }

    // �����޸� Attach�� ��� restore�� �ʿ����
    // prepare�ܰ迡�� attach�� �����Ѵ�.
    if ( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_SHM_ATTACH )
    {
        IDE_TEST(restoreAttachSharedDB(aTBSNode,
                                       &sShmHeader,
                                       aOp)
                 != IDE_SUCCESS);
    }
    else
    {
        // �����޸� Create��, �Ϲ� �޸𸮸���� ���
        //
        // Restore�ÿ� ���� Page Memory�� �Ҵ�/�ε�Ǹ�,
        // Prepare�ÿ��� �ƹ��� ó���� ���� �ʴ´�.
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_shm_region );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_INVALID_SHARED_MEMORY_DATABASE_TRIAL_TO_DIFFERENT_RESTORE_MODE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/*
    DB File Loading Message���

    [IN] aRestoreType - DB File Restore Type
 */
void smmManager::printLoadingMessage( smmDBRestoreType aRestoreType )
{
    switch ( aRestoreType )
    {
        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
            IDE_CALLBACK_SEND_SYM("                          : Created Shared Memory Version ");
            break;

        case SMM_DB_RESTORE_TYPE_SHM_ATTACH :
            IDE_CALLBACK_SEND_SYM("                          : Attached Shared Memory Version ");

        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            IDE_CALLBACK_SEND_SYM("                          : Dynamic Memory Version");
            break;

        default:
            IDE_ASSERT(0);

    }

    switch ( aRestoreType )
    {
        // Restore�� �ϴ� ��� Parallel Loading����,
        // Serial Loading���� ���
        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            switch(smuProperty::getRestoreMethod())
            {
                case 0:
                    IDE_CALLBACK_SEND_MSG(" => Serial Loading");
                    break;
                case 1:
                    IDE_CALLBACK_SEND_MSG(" => Parallel Loading");
                    break;
                default:
                    IDE_ASSERT(0);
            }
            break;
        default:
            // do nothing
            break;
    }
}

/*
    Alter TBS Online�� ���� Tablespace�� Prepare / Restore �Ѵ�.

    [IN] aTBSNode - Restore�� Tablespace�� Node

    [ ����ó�� Ư�̻��� ]
      - ��� Tablespace�� ���� Restore Type���� Restore�Ǵ���
        ����üũ�� ���� ����

      - ���� :  Startup�ÿ��� Shared Memory Attach�Ǿ��� Tablespace��
                Alter Tablespace Offline�� Shared Memory��
                ������ �����ϰ� Alter Tablespace Online��
                Shared Memory Create�� Restore�� �� �ֱ� ����
 */
IDE_RC smmManager::prepareAndRestore( smmTBSNode * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    // ��� Tablespace�� ���� Restore Type���� Restore�Ǵ���
    // ����üũ�� ���� ����
    IDE_TEST( prepareTBS( aTBSNode,
                          SMM_PREPARE_OP_DONT_CHECK_RESTORE_TYPE )
                  != IDE_SUCCESS );

    // Alter TBS Offline�� ��� �����޸� ��ü�� ������ ������
    // Alter TBS Online�ÿ��� �����޸� Attach�� ���� ���� ����.
    IDE_ASSERT( aTBSNode->mRestoreType != SMM_DB_RESTORE_TYPE_SHM_ATTACH );

    IDE_TEST( restoreTBS( aTBSNode, SMM_RESTORE_OP_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* loganchor�� checkpoint image attribute���� ���� ����
   dbfile ���� ��ȯ => restore db�ÿ� ����

 [IN] aTBSNode - Tablespace�� Node
 */
UInt smmManager::getRestoreDBFileCount( smmTBSNode      * aTBSNode )
{
    UInt sDBFileCount;

    IDE_DASSERT( aTBSNode != NULL );

    // loganchor(TBSNode)�������� loading�� �����Ѵ�.
    // +1�� �ϴ� ���� mLstCreateDBFile�� ���� ��ȣ�̱� �����̴�.
    sDBFileCount = aTBSNode->mLstCreatedDBFile + 1;

    return sDBFileCount;
}


/*
 * smmManager::restoreDB�� ���� Action�Լ�
 */
IDE_RC smmManager::restoreTBSAction( idvSQL*             /*aStatistics*/,
                                     sctTableSpaceNode * aTBSNode,
                                     void *              aActionArg )
{
    idBool      sDoIt;
    UInt        sState = 0;
    void       *sPageBuffer;
    void       *sAlignedPageBuffer;

    IDE_DASSERT( aTBSNode != NULL );

    sPageBuffer= NULL;
    sAlignedPageBuffer = NULL;

    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SMM,
                                           SM_PAGE_SIZE,
                                           (void**)&sPageBuffer,
                                           (void**)&sAlignedPageBuffer )
             != IDE_SUCCESS);
    sState = 1;

    // CONTROL�ܰ迡�� DROPPED�� TBS�� loganchor�κ���
    // �о������ �ʱ� ������
    // PREPARE/RESTORE�߿� DROPPED������ TBS�� ���� �� ����.
    IDE_ASSERT( ( aTBSNode->mState & SMI_TBS_DROPPED ) != SMI_TBS_DROPPED );

    // Memory Tablespace�� DISCARD,OFFLINE Tablespace�� �����ϰ�
    // PREPARE/RESTORE�� �����Ѵ�.
    if(( sctTableSpaceMgr::isMemTableSpace(aTBSNode->mID) == ID_TRUE ) &&
       ( sctTableSpaceMgr::hasState(aTBSNode->mID, SCT_SS_SKIP_RESTORE )
         == ID_FALSE) )
    {
        IDE_DASSERT( ID_SIZEOF(void*) >= ID_SIZEOF(smmRestoreOption) );

        switch ( (smmRestoreOption)(vULong)aActionArg )
        {
            case SMM_RESTORE_OP_NONE:
            case SMM_RESTORE_OP_DBIMAGE_NEED_RECOVERY:
            {
                // ���󱸵���
                sDoIt = ID_TRUE;
                break;
            }
            case SMM_RESTORE_OP_DBIMAGE_NEED_MEDIA_RECOVERY:
            {
                if ( isMediaFailureTBS( (smmTBSNode*)aTBSNode ) == ID_TRUE )
                {
                    // �̵��� �÷��װ� ID_TRUE�� ������ ����Ÿ������
                    // ���� ��� Restore�� �����Ѵ�.
                    sDoIt = ID_TRUE;
                }
                else
                {
                    if( aTBSNode->mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
                    {

                        // sys_mem_dic_tbs�� ���ؼ� membase�� �̵�����
                        // �����Ѵ�. dirty page write�ÿ� check membase��
                        // �����ϴ� ���� ����ؾ��Ѵ�.
                        IDE_TEST( openFstDBFilesAndSetupMembase(
                                                (smmTBSNode*)aTBSNode,
                                                (smmRestoreOption)(vULong)aActionArg,
                                                (UChar*)sAlignedPageBuffer)
                                  != IDE_SUCCESS );

                        // PCH ��Ʈ���� Page �޸𸮸� �Ҵ��ϰ� Page���� ����
                        IDE_TEST( fillPCHEntry( (smmTBSNode*)aTBSNode,
                                                 SMM_MEMBASE_PAGEID, // sPID
                                                 SMM_FILL_PCH_OP_COPY_PAGE,
                                                 (void*)((UChar*)sAlignedPageBuffer))
                                  != IDE_SUCCESS);

                        IDE_TEST(setupBasePageInfo(
                                  (smmTBSNode*)aTBSNode,
                                  (UChar*)mPCArray[((smmTBSNode*)aTBSNode)->mTBSAttr.mID].mPC[SMM_MEMBASE_PAGEID].mPagePtr )
                                  != IDE_SUCCESS);

                        smmDatabase::makeMembaseBackup();
                    }

                   // retore �� ���� �ʴ´�.
                   sDoIt = ID_FALSE;
                }
                break;
            }
            default:
            {
                IDE_ASSERT( 0 );
                break;
            }
        }

        if ( sDoIt == ID_TRUE )
        {
            IDE_TEST( restoreTBS( (smmTBSNode *) aTBSNode,
                                  (smmRestoreOption) (vULong) aActionArg )
                      != IDE_SUCCESS );
        }
    }

    sState = 0;
    IDE_TEST( iduMemMgr::free( sPageBuffer ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( iduMemMgr::free( sPageBuffer ) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/*
 * ��ũ �̹����κ��� �����ͺ��̽� �������� �ε��Ѵ�.
 *
 * aOp [IN] Restore �ɼ�/����
 */

IDE_RC smmManager::restoreDB ( smmRestoreOption aOp )
{
    IDE_DASSERT( ID_SIZEOF(void*) >= ID_SIZEOF(aOp) );

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  restoreTBSAction,
                                                  (void*) aOp,
                                                  SCT_ACT_MODE_NONE)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * ���̺����̽��� ù��° DB ������ ��� �����ϰ�,
 * Membase �� �����Ѵ�.
 *
 * aTBSNode [IN] TableSpace Node
 * aOp      [IN] Restore �ɼ�/����
 */

IDE_RC smmManager::openFstDBFilesAndSetupMembase( smmTBSNode * aTBSNode,
                                                  smmRestoreOption aOp,
                                                  UChar*       aPageBuffer )
{
    UInt              i;
    smmDatabaseFile * sDBFile;
    smmDatabaseFile * sFirstDBFile = NULL;
    UInt              sOpenLoop;

    if ( aOp == SMM_RESTORE_OP_DBIMAGE_NEED_MEDIA_RECOVERY )
    {
        // Media Recovery�ÿ��� Stable ������ Open�ϸ� �ȴ�.
        i = (UInt)aTBSNode->mTBSAttr.mMemAttr.mCurrentDB; // stable
        sOpenLoop = i+1;
    }
    else
    {
        // Restart�ÿ��� Stable/Unstable ��� Open�Ѵ�.
        i =  0;
        sOpenLoop = SMM_PINGPONG_COUNT;
    }
    /* --------------------------
     * [0] First DB File Open
     * ------------------------ */

    for (; i < sOpenLoop ; i++)
    {
        IDE_TEST( getDBFile( aTBSNode,
                             i,
                             0,
                             SMM_GETDBFILEOP_SEARCH_FILE,
                             & sDBFile )
                  != IDE_SUCCESS );

        if( i == (UInt)aTBSNode->mTBSAttr.mMemAttr.mCurrentDB )
        {
            sFirstDBFile = sDBFile;
        }
        
        // LogAnchor���� �ʱ�ȭ�ɶ��� ������ �������� �����Ƿ�,
        // ���ʷ� Open�ϴ� ���̴�.
        if (sDBFile->isOpen() != ID_TRUE )
        {
            IDE_TEST(sDBFile->open() != IDE_SUCCESS);
        }
    }

    // BUG-27456 Klocwork SM (4)
    IDE_ERROR_RAISE (sFirstDBFile != NULL, ERR_NOEXIST_FILE );

    /* ------------------------------------------------
     *  Read catalog page(PageID=0) &
     *  setup mMemBase & m_catTableHeader
     * ----------------------------------------------*/
    if ( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_SHM_ATTACH )
    {
        // Do nothing
        // prepareTBS���� attach�ϸ鼭 ��� ó���Ͽ���.
    }
    else
    {
        // �����޸� Create �Ǵ� �Ϲ� �޸𸮸���� ���
        //
        // 0�� Page�� ���� �޸𸮿� �ӽ������� �ε��Ͽ�
        // restore�ϴ� ���ȿ��� �����
        // Membase�� catalog table header�� ����.

        // restore�� �Ϸ�� �Ŀ� setBasePageInfo�� �ٽ� ȣ���Ͽ�
        // ���� 0�� Page�� �ּҸ� �̿��Ͽ�
        // Membase�� catalog table header�� �缳�� �ؾ��Ѵ�.
        IDE_TEST(sFirstDBFile->readPage(
                                   aTBSNode,
                                   SMM_MEMBASE_PAGEID,
                                   aPageBuffer )
                 != IDE_SUCCESS);

        IDE_TEST(setupBasePageInfo( aTBSNode,
                                    aPageBuffer )
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOEXIST_FILE );
    {
        SChar sErrorFile[SM_MAX_FILE_NAME];

        idlOS::snprintf( (SChar*)sErrorFile, SM_MAX_FILE_NAME, "%s-%"ID_UINT32_FMT"-0",
                         aTBSNode->mHeader.mName,
                         i );

        IDE_SET( ideSetErrorCode( smERR_ABORT_NoExistFile,
                                  (SChar*)sErrorFile ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ��ũ �̹����κ��� �����ͺ��̽� �������� �ε��Ѵ�.
 *
 * aOp [IN] Restore �ɼ�/����
 */

IDE_RC smmManager::restoreTBS ( smmTBSNode * aTBSNode, smmRestoreOption aOp )
{
    UInt              sFileIdx;
    SInt              sPingPong;
    idBool            sFound;
    UInt              sDBFileCount;
    SChar             sDBFileName[SM_MAX_FILE_NAME];
    SChar           * sDBFileDir;
    smmDatabaseFile * sDBFile;
    UInt              sState = 0;
    void            * sPageBuffer;
    void            * sAlignedPageBuffer;

    IDE_DASSERT( aTBSNode != NULL );

    sPageBuffer = NULL;
    sAlignedPageBuffer = NULL;

    /*
     * BUG-18828 memory tbs�� restoreTBS ���� align ���� ����
     * i/o buffer �� ����Ͽ� ���� ��������
     */
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SMM,
                                           SM_PAGE_SIZE,
                                           (void**)&sPageBuffer,
                                           (void**)&sAlignedPageBuffer )
             != IDE_SUCCESS);
    sState = 1;

    ideLog::log(IDE_SERVER_0,
                "Restoring Tablespace : %s\n",
                aTBSNode->mHeader.mName );

    /* ----------------------------------
     *  [1] open First DBFiles And Setup MemBase
     * -------------------------------- */
    IDE_TEST( openFstDBFilesAndSetupMembase( aTBSNode,
                                             aOp,
                                             (UChar*)sAlignedPageBuffer)
              != IDE_SUCCESS );

    /* ----------------------------------
     *  [2] set total page count in Disk
     * -------------------------------- */
    IDE_TEST( calculatePageCountInDisk(aTBSNode) != IDE_SUCCESS);

    /* ------------------------------------------------
     * [3] All DB File Open
     * ----------------------------------------------*/
    for ( sPingPong = 0 ; sPingPong < SMM_PINGPONG_COUNT; sPingPong++ )
    {
        sDBFileCount = getRestoreDBFileCount( aTBSNode );

        // ������ ���� ��쿡�� Open�Ѵ�.
        for ( sFileIdx = 1; sFileIdx < sDBFileCount; sFileIdx++ )
        {
            IDE_ASSERT( sFileIdx < aTBSNode->mHighLimitFile );

            if( smmManager::getCreateDBFileOnDisk( aTBSNode,
                                                   sPingPong,
                                                   sFileIdx ) == ID_TRUE )
            {

                sFound =  smmDatabaseFile::findDBFile( aTBSNode,
                                                       sPingPong,
                                                       sFileIdx,
                                                       (SChar*)sDBFileName,
                                                       &sDBFileDir );
                if ( sFound == ID_TRUE )
                {
                    IDE_TEST( openAndGetDBFile( aTBSNode,
                                                sPingPong,
                                                sFileIdx,
                                                & sDBFile )
                              != IDE_SUCCESS )
                }
            }
            else
            {
                /*  ��ũ�� ������ ����� ������ Ȯ���� �ʿ䵵 ����. */  
            }
        } //for
    } //for

    /* ------------------------------------------------
     * [2] ���� DB �ε�
     * ----------------------------------------------*/
    if ( smuProperty::getLogBufferType() == SMU_LOG_BUFFER_TYPE_MEMORY )
    {
        // log buffer type�� memory �� ��쿡��
        // SMM_DB_RESTORE_TYPE_DYNAMIC �� ����
        IDE_TEST_RAISE( ( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_SHM_CREATE) ||
                        ( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_SHM_ATTACH),
                        err_invalid_database_type );
    }

    (void)invalidate(aTBSNode); // db�� inconsistency ���·� ����
    {
        ideLog::log(IDE_SERVER_0,
                    "     BEGIN TABLESPACE[%"ID_UINT32_FMT"] RESTORATION\n",
                    aTBSNode->mHeader.mID);

        switch( aTBSNode->mRestoreType )
        {
        case SMM_DB_RESTORE_TYPE_DYNAMIC:
            IDE_TEST( restoreDynamicDB(aTBSNode )
                      != IDE_SUCCESS);
            break;
        case SMM_DB_RESTORE_TYPE_SHM_CREATE:
            IDE_TEST( restoreCreateSharedDB(aTBSNode, aOp ) != IDE_SUCCESS);
            break;
        case SMM_DB_RESTORE_TYPE_SHM_ATTACH:
            // do nothing : prepare�ÿ� �̹� Attach�Ϸ��Ͽ���
            break;
        default:
            IDE_CALLBACK_FATAL("error");
            idlOS::abort();
        }

        ideLog::log(IDE_SERVER_0,
                    "     END TABLESPACE[%"ID_UINT32_FMT"] RESTORATION\n",
                    aTBSNode->mHeader.mID);
    }
    (void)validate(aTBSNode); // db�� consistency ���·� �ǵ���.

    // copy membase to backup
    if( aTBSNode->mHeader.mID == SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC )
    {
        smmDatabase::makeMembaseBackup();
    }

#ifdef DEBUG
    // Recovery�� �ʿ����� ���� ���� Startup�� ���
    if ( (aOp & SMM_RESTORE_OP_DBIMAGE_NEED_RECOVERY) == 0 )
    {
        // DB�� �� �ε��� �����̹Ƿ�, Assertion����
        IDE_DASSERT( smmFPLManager::isAllFPLsValid(aTBSNode) == ID_TRUE );
    }
#endif

    sState = 0;
    IDE_TEST( iduMemMgr::free( sPageBuffer ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_database_type );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_INVALID_SHARED_MEMORY_DATABASE));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( iduMemMgr::free( sPageBuffer ) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;

}

/*
  PRJ-1548 User Memory Tablespace ���䵵��

  �̵��� �÷��װ� ID_TRUE�� ������ ����Ÿ������ ���� ���
  Media Failure ���̺����̽��̹Ƿ� ID_TRUE�� ��ȯ�Ѵ�.

  [IN] aTBSNode - check�� Tablespace�� Node

  [RETURN] ID_TRUE  - media failure ����
           ID_FALSE - ����

*/
idBool smmManager::isMediaFailureTBS( smmTBSNode * aTBSNode )
{
    UInt             sFileIdx;
    idBool           sResult;
    smmDatabaseFile *sDatabaseFile;

    IDE_DASSERT( aTBSNode != NULL );

    sResult = ID_FALSE;

    for ( sFileIdx = 0; sFileIdx <= aTBSNode->mLstCreatedDBFile ; sFileIdx ++ )
    {
        IDE_ASSERT(sFileIdx < aTBSNode->mHighLimitFile);

        IDE_ASSERT( getDBFile( aTBSNode,
                               smmManager::getCurrentDB( aTBSNode ),
                               sFileIdx,
                               SMM_GETDBFILEOP_NONE,
                               &sDatabaseFile )
                    == IDE_SUCCESS );

        if ( sDatabaseFile->getIsMediaFailure() == ID_TRUE )
        {
            // Media Failure ����
            sResult = ID_TRUE;
            break;
        }
        else
        {
            // �������
        }
    }

    return sResult;
}


/*
 * �Ϲ� �޸𸮷� �����ͺ��̽� �̹����� ������ �о���δ�.
 *
 * aOp [IN] Restore �ɼ�/����
 */
IDE_RC smmManager::restoreDynamicDB( smmTBSNode     * aTBSNode)
{

    IDE_ASSERT( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_DYNAMIC );

    switch(smuProperty::getRestoreMethod())
    {
        case 0:
            IDE_TEST(loadSerial2(aTBSNode) != IDE_SUCCESS);
            break;
        case 1:
            IDE_TEST(loadParallel(aTBSNode) != IDE_SUCCESS);
            break;
        default:
            IDE_CALLBACK_FATAL("Can't be here");
    }

    IDE_TEST(setupBasePageInfo(
                 aTBSNode,
                 (UChar*)mPCArray[aTBSNode->mTBSAttr.mID].mPC[SMM_MEMBASE_PAGEID].mPagePtr )
             != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ���� �޸𸮷� �����ͺ��̽� �̹����� ������ �о���δ�.
 *
 */
IDE_RC smmManager::restoreCreateSharedDB( smmTBSNode     * aTBSNode,
                                          smmRestoreOption aOp )
{
    scPageID            sNeedPage = 0;
    scPageID            sShmPageChunkCount;
    ULong               sShmChunkSize;

    // �̵�� ������ Shared Memory Version�� �������� �ʴ´�.
    // �̹� prepare �������� Error checking�� �Ͽ��� ������
    IDE_ASSERT( aOp != SMM_RESTORE_OP_DBIMAGE_NEED_MEDIA_RECOVERY );
    sShmChunkSize = smuProperty::getShmChunkSize();

    /* ----------------------------------------------------------------------
     * [Section - 1] DB�� ���� Page ������ ��´�.
     * --------------------------------------------------------------------*/

    /* --------------------------
     * [1-3] �ʿ��� page ���� ���ϱ�
     *       free page list count + echo table page count
     * ------------------------ */
    sNeedPage = aTBSNode->mMemBase->mAllocPersPageCount;

    /* -------------------------------------------------------------------
     * [Section - 2] Page ������ ������� �����޸� ���� ��  �ε�
     * -----------------------------------------------------------------*/

    /* --------------------------
     * [2-1] Shared Memory ����
     *
     * PR-1561 : [�������] shared memory���� 2GB�̻� create�� �� ����
     *
     *  = ���� �޸� ������ DB Startup�ÿ� 2G�̻��� DB�� ������ ���
     *    AIX���� �Ѳ����� 2G�� �����޸𸮸� �Ҵ���� �� ����.
     *    ����, Startup�� ������ ������Ƽ(�ִ� 2G)�� �뷮������
     *    �Ҵ�޵��� �Ѵ�.
     *    ������Ƽ�� : SMU_STARTUP_SHM_CHUNK_SIZE
     * ------------------------ */

    sShmPageChunkCount = (sShmChunkSize / SM_PAGE_SIZE);

    if (sNeedPage <= sShmPageChunkCount)
    {
        /* ------------------------------------------------
         *  case 1 : �ε��� DB�� ũ�Ⱑ ������Ƽ���� �۰ų� ����.
         * ----------------------------------------------*/
        // ù��° Chunk�����ϰ� �����޸� Key�� TBSNode�� ����
        IDE_TEST(smmFixedMemoryMgr::createFirstChunk(
                     aTBSNode,
                     sNeedPage) != IDE_SUCCESS);

        // Restore��ģ ����
        // Log Anchor�� ��� TBS�� ShmKey�� Flush�ؾ��Ѵ�.
        // BUGBUG-1548 ���� Flush�ϴ��� üũ�� ��
    }
    else
    {
        /* ------------------------------------------------
         *  case 2 : �ε��� DB�� ũ�Ⱑ ������Ƽ���� ŭ.
         *           so, split DB to shm chunk.
         * ----------------------------------------------*/
        // ù��° Chunk�����ϰ� �����޸� Key�� Log Anchor�� Flush
        IDE_TEST(smmFixedMemoryMgr::createFirstChunk(
                     aTBSNode,
                     sShmPageChunkCount) != IDE_SUCCESS);
        while(1)
        {
            sNeedPage -= sShmPageChunkCount;
            if (sNeedPage <= sShmPageChunkCount)
            {
                IDE_TEST(smmFixedMemoryMgr::extendShmPage(aTBSNode,
                                                          sNeedPage)
                         != IDE_SUCCESS);
                break;
            }
            else
            {
                IDE_TEST(smmFixedMemoryMgr::extendShmPage(aTBSNode,
                                                          sShmPageChunkCount)
                         != IDE_SUCCESS);
            }
        }
    }

    (void)invalidate(aTBSNode);
    {
        switch(smuProperty::getRestoreMethod())
        {
            case 0:
                IDE_TEST(loadSerial2( aTBSNode ) != IDE_SUCCESS);
                break;
            case 1:
                IDE_TEST(loadParallel( aTBSNode ) != IDE_SUCCESS);
                break;
            default:
                IDE_CALLBACK_FATAL("Can't be here");
        }

        IDE_TEST(setupBasePageInfo(
                     aTBSNode,
                     (UChar*)mPCArray[aTBSNode->mTBSAttr.mID].mPC[SMM_MEMBASE_PAGEID].mPagePtr)
                 != IDE_SUCCESS);
    }
    (void)validate(aTBSNode); // db�� consistency ���·� �ǵ���.


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* DB File�κ��� ���̺� �Ҵ�� Page���� �޸𸮷� �ε��Ѵ�.
 *
 * �� �Լ��� ���� �⵿�ÿ� ��ũ �̹��� ���Ϸκ��� Page�� �޸𸮷� ������ ��
 * ���ȴ�.
 *
 * �׸��� DB File�� �����ؾ� �� Page�� ������ �޾Ƽ�
 * PCH ��Ʈ���� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 * ����, DB File�κ��� �޸𸮷� �ε��� Page�� ���� �޾Ƽ� Page�� Disk�κ���
 * �о���δ�.
 *
 * ���̺� �Ҵ�� Page�� ���� DB File�� ��ϵ�����,
 * �ѹ��� ���̺� �Ҵ�� ���� ���� Free Page�� ����
 * �ƿ� DB File�� ������� ���� �ʱ� ������,
 * DB File�� ũ��� ���� �����ؾ� �ϴ� Page���� Ŀ������ ���� ���� �ִ�.
 *
 * �������, �ϳ��� DB���Ͽ� 1������ 20������ 20���� Page�� ��� �����ѵ�,
 * �� �� 1������ 10�������� ���̺� �Ҵ�Ǿ���, 11������ 20��������
 * �ѹ��� �������� ���� Free Page��� �����غ���.
 *
 * 1������ 10�������� ( Ȥ�� 10�� Page�ϳ��� ) Disk �̹����� ��ϵ� ����
 * �־ DB File�� 10���� Page�� ����� �� �ִ� ũ���� 320KB���� ũ�Ⱑ
 * ������ �ȴ�. ( �ϳ��� Page�� 32KB��� ���� )
 * �� �Լ��� 1������ 10�������� Page�� ���ؼ��� PCH��Ʈ���� �Ҵ��ϰ�,
 * Page �޸� ���� �Ҵ��� ��, Disk�κ��� ���� Page�� ������ �����Ѵ�.
 *
 * 11������ 20�������� �ѹ��� �������� ���� Free Page�� Disk�̹�����
 * ������� ���� ������, Free Page�� PCH ��Ʈ���� �����ؾ� �ϱ� ������,
 * �� �Լ��� 11������ 20�������� PCH ��Ʈ���� �Ҵ��ϰ� �ʱ�ȭ�� �Ѵ�.
 *
 * �� ������ loadDbFile�� ���ڴ� ������ ����.
 * ( aFileMinPID = 1, aFileMaxPID = 20, aLoadPageCount = 10 )
 *
 * aFileNumber    [IN] DB File ��ȣ - 0���� �����Ѵ�.
 * aFileMinPID    [IN] DB File�� ��ϵǾ�� �ϴ� Page ���� - ù��° Page ID
 * aFileMaxPID    [IN] DB File�� ��ϵǾ�� �ϴ� Page ���� - ������ Page ID
 * aLoadPageCount [IN] ù��° Page���� �����Ͽ� �޸𸮷� �о���� Page�� ��
 */


IDE_RC smmManager::loadDbFile( smmTBSNode *     aTBSNode,
                               UInt             aFileNumber,
                               scPageID         aFileMinPID,
                               scPageID         aFileMaxPID,
                               UInt             aLoadPageCount )
{
#ifdef DEBUG
    scSpaceID        sSpaceID;
#endif
    scPageID         sPID;
    scPageID         sLastLoadedPID;

#ifdef DEBUG
    sSpaceID = aTBSNode->mTBSAttr.mID;
#endif

    IDE_DASSERT( isValidPageID( sSpaceID, aFileMinPID )
                 == ID_TRUE );
    /* Max PID�� File�� ���� �� �ִ� �ִ�ũ�� �̱⶧���� Valid Page��
     * ������ �� �ִ�.
    IDE_DASSERT( isValidPageID( sSpaceID, aFileMaxPID )
                 == ID_TRUE );
    */

    IDE_TEST( loadDbPagesFromFile( aTBSNode,
                                   aFileNumber,
                                   aFileMinPID,
                                   aLoadPageCount ) != IDE_SUCCESS );

    // DB �̹����κ��� Load�� Page �� PID�� ���� ū��
    sLastLoadedPID = aFileMinPID + aLoadPageCount - 1;

    // DISK �̹����� �ѹ��� ������� ���� ���� ��� Page�� ���ؼ�
    // PCH Entry�Ҵ�
    for (sPID = sLastLoadedPID + 1; sPID <= aFileMaxPID; sPID++)
    {
        if ( sPID >= aTBSNode->mDBMaxPageCount )
        {
            // �ִ� ������ ������ ����ٸ� PCH Entry�Ҵ�����
            break;
        }
        else
        {
            // PCH�� �Ҵ��ϰ� Page�޸𸮴� �Ҵ����� �ʴ´�.
            IDE_TEST( allocPCHEntry( aTBSNode, sPID ) != IDE_SUCCESS );
        }
    }



    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * �ϳ��� DB���Ͽ� ���� ��� Page�� �޸� Page�� �ε��Ѵ�.
 *
 * - �ϳ��� ������ ������ ���� ���� ����(Chunk)�� ������ �ε��Ѵ�.
 * - �̷��� �ε��� ������ ũ��� RESTORE_CHUNK_PAGE_COUNT�� ���� �����ϴ�.
 * - RESTORE_CHUNK_PAGE_COUNT�� �ʹ� ũ�� �޸� �������� ���� Startup��
 *   ������ ���� �ִ�. ( BUG-15020 ���� )
 * - RESTORE_CHUNK_PAGE_COUNT�� �ʹ� ������ ����� I/O�������� ����
 *   Startup�� ������ ���ϵ� �� �ִ�.
 *
 * aFileNumber    [IN] DB File ��ȣ - 0���� �����Ѵ�.
 * aFileMinPID    [IN] DB File�� ��ϵǾ�� �ϴ� Page ���� - ù��° Page ID
 * aLoadPageCount [IN] ù��° Page���� �����Ͽ� �޸𸮷� �о���� Page�� ��
 */

IDE_RC smmManager::loadDbPagesFromFile(smmTBSNode *     aTBSNode,
                                       UInt             aFileNumber,
                                       scPageID         aFileMinPID,
                                       ULong            aLoadPageCount )
{
    UInt        sStage = 0;
    void      * sRealBuffer    = NULL; /* �Ҵ���� ûũ �޸� �ּ� */
    void      * sAlignedBuffer = NULL; /* AIO�� ���� Align�� �޸� �ּ� */
    SLong       sRemainPageCount;      /* ������ �� �ε��� (����)Page �� */
    scPageID    sChunkStartPID;        /* �ε��� ������ ù��° Page ID */
    UInt        sChunkPageCount;       /* �ε��� ������ ���� Page�� �� */

    IDE_DASSERT( isValidPageID( aTBSNode->mTBSAttr.mID, aFileMinPID )
                 == ID_TRUE );

    smmDatabaseFile *sDbFile;

    IDE_TEST( openAndGetDBFile( aTBSNode,
                                aTBSNode->mTBSAttr.mMemAttr.mCurrentDB,
                                aFileNumber,
                                &sDbFile )
              != IDE_SUCCESS );

    // Page���� �����ͺ��̽� ���Ͽ��� �о���� I/O�� �����Ѵ�.
    IDE_TEST(iduFile::allocBuff4DirectIO(
                 IDU_MEM_SM_SMM,
                 smuProperty::getRestoreBufferPageCount()
                 * SM_PAGE_SIZE,
                 (void**)&sRealBuffer,
                 (void**)&sAlignedBuffer)
             != IDE_SUCCESS);
    sStage = 1;


    sChunkStartPID   = aFileMinPID ;
    sRemainPageCount = aLoadPageCount;

    do
    {
        /* smuProperty::getRestoreBufferPageCount() �� sRemainPageCount��
           ���� ������ ����ŭ �ε� */
        sChunkPageCount =
            ( sRemainPageCount < smuProperty::getRestoreBufferPageCount() ) ?
            sRemainPageCount : smuProperty::getRestoreBufferPageCount();

        IDE_TEST( loadDbFileChunk( aTBSNode,
                                   sDbFile,
                                   sAlignedBuffer,
                                   aFileMinPID,
                                   sChunkStartPID,
                                   sChunkPageCount )
                  != IDE_SUCCESS );

        sChunkStartPID   += smuProperty::getRestoreBufferPageCount();
        sRemainPageCount -= smuProperty::getRestoreBufferPageCount();
    } while ( sRemainPageCount > 0 );

    sStage = 0;
    IDE_TEST(iduMemMgr::free(sRealBuffer) != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 1 :
            IDE_ASSERT( iduMemMgr::free(sRealBuffer) == IDE_SUCCESS );
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;

}

/* �����ͺ��̽� ������ �Ϻ� ����(Chunk) �ϳ��� �޸� �������� �ε��Ѵ�.
 *
 *
 * �����ͺ��̽� �������� ��ũ�κ��� �޸𸮷� �ø��� �۾���
 * Restart Recovery������ �̷������.
 * Ư�� Page�� Free Page ���� ���θ� ������ ���̺� �Ҵ�� Page����
 * �ε��ϰ� �Ǹ�, Restart Recovery������ Ư�� Page�� Free Page����
 * ���θ� �˱� ���� Free List Info Page�� �����ϰ� �ȴ�.
 *
 * �̶�, Recovery�� ���� ���� ���¿��� Free List Info Page�� �����ϰ�
 * �Ǿ�, Free List Info Page�� ������ ������ ��ϵǾ� ���� �� �ִ�,
 * �̷� ���� Free Page�� �ƴѵ��� Free Page�� �Ǵ��ϰų�,
 * ���̺� �Ҵ�� Page��  �ƴѵ��� ���̺� �Ҵ�� Page�� �Ǵ��Ͽ�
 * �ε��ؾ� �� Page�� �ε����� �ʰų�, ���ʿ��� Page�� �ε��ϰ� �Ǵ�
 * �������� �߻��Ѵ�.
 *
 * �ذ�å : Restart Recovery������ Page �ε��ÿ��� Free Page����
 *           ���θ� �� ���� ����.
 *
 *           �켱 Free Page������ ���� ���̶�� �����ϰ�
 *           DB File�� �ε��Ѵ�.
 *
 *           Free Page������ ���� ���� ���, ��, Restart Recovery
 *           �߿� Redo/Undo�� ���� Page�� �޸𸮿� ���� ���,
 *           �ش� Page�� DB File���� ������ �ε��Ѵ�.
 *
 *           Restart Recovery�Ŀ� Free Page�鿡 ���ؼ�
 *           Page �޸𸮸� �ݳ��Ѵ�.
 *
 * aDbFile         [IN] �ε��� ������ �������� �ִ� ������ ����
 * aAlignedBuffer  [IN] �����Ͱ� �ε�� �޸� ��ġ.
 *                      �����ּҰ� AIO�� ���� ���� Align�Ǿ� �ִ�.
 * aFileMinPID     [IN] DB File�� ��ϵǾ�� �ϴ� Page ���� - ù��° Page ID
 * aChunkStartPID  [IN] �ε��� ����(Chunk)�� ���� PID
 * aChunkPageCount [IN] �ε��� ����(Chunk)�� ������ ��
 *
 * ����! �� �Լ������� Chunk�� DB������ �ε��� ������,
 *        Expand Chunk�� �ƹ��� ������ ����.
 */

IDE_RC smmManager::loadDbFileChunk(smmTBSNode      * aTBSNode,
                                   smmDatabaseFile * aDbFile,
                                   void            * aAlignedBuffer,
                                   scPageID          aFileMinPID,
                                   scPageID          aChunkStartPID,
                                   ULong             aChunkPageCount )
{
#ifdef DEBUG
    scSpaceID        sSpaceID;
#endif
    scPageID         sPID ;
    size_t           sReadSize;    /* Data File���� ���� ���� Page �� */
    idBool           sIsFreePage ; /* �ϳ��� Page�� Free Page���� ���� */
    UInt             sPageOffset ; /* aAlignedBuffer�ȿ����� Page�� Offset */
    scPageID         sChunkEndPID; /* �ϳ��� ����(Chunk)�� ������ PID */

#ifdef DEBUG
    sSpaceID = aTBSNode->mTBSAttr.mID;
#endif
    IDE_DASSERT( aDbFile != NULL );
    IDE_DASSERT( aAlignedBuffer != NULL );
    IDE_DASSERT( isValidPageID(sSpaceID, aChunkStartPID )
                 == ID_TRUE );

#ifndef VC_WIN32
    if (smuProperty::getRestoreAIOCount() > 0 &&
        (aChunkPageCount * SM_PAGE_SIZE) > SMM_MIN_AIO_FILE_SIZE)
    {
        IDE_TEST(aDbFile->readPagesAIO( (aChunkStartPID - aFileMinPID) * SM_PAGE_SIZE,
                                        aAlignedBuffer,
                                        aChunkPageCount * SM_PAGE_SIZE,
                                        &sReadSize,
                                        smuProperty::getRestoreAIOCount()
                                        ) != IDE_SUCCESS);
        if( sReadSize != (aChunkPageCount * SM_PAGE_SIZE) )
        {
            ideLog::log(IDE_SERVER_0, "2. sReadSize(%lld), require(%lld)\n",
                        sReadSize,
                        aChunkPageCount * SM_PAGE_SIZE );
            IDE_RAISE( read_size_error );

//              IDE_TEST_RAISE(sReadSize != (aPageCount * SM_PAGE_SIZE),
//                             read_size_error);
        }

    }
    else
#endif
    {
        IDE_TEST(aDbFile->readPages( (aChunkStartPID - aFileMinPID) * SM_PAGE_SIZE,
                                     aAlignedBuffer,
                                     aChunkPageCount * SM_PAGE_SIZE,
                                     &sReadSize) != IDE_SUCCESS);

        if( sReadSize != (aChunkPageCount * SM_PAGE_SIZE) )
        {
            ideLog::log(IDE_SERVER_0,
                        "1. sReadSize(%llu), require(%llu)\n",
                        (ULong)sReadSize,
                        (ULong)aChunkPageCount * (ULong)(SM_PAGE_SIZE) );
            IDE_RAISE( read_size_error );
        }
    }

    sChunkEndPID = aChunkStartPID + aChunkPageCount - 1;

    // �ε��� ��� Page�� ����
    for ( sPID = aChunkStartPID;
          sPID <= sChunkEndPID;
          sPID ++ )
    {
        sIsFreePage = ID_FALSE ;

        // Page�� Load�ؾ� ���� �˻�
        IDE_TEST( smmExpandChunk::isFreePageID ( aTBSNode,
                                                 sPID,
                                                 & sIsFreePage )
                  != IDE_SUCCESS );

        // ���̺� �Ҵ���� ���� �����ͺ��̽� Free Page�� ���
        if ( sIsFreePage == ID_TRUE )
        {
            // PCH�� �Ҵ��ϰ� Page�޸𸮴� �Ҵ����� �ʴ´�.
            IDE_TEST( allocPCHEntry( aTBSNode, sPID ) != IDE_SUCCESS );
        }
        else // ���̺� �Ҵ�� Page�� ���
        {

            // DB File �ȿ��� Page�� Offset
            sPageOffset = ( sPID - aChunkStartPID ) * SM_PAGE_SIZE ;

            // Page Offset ���κ��� Page ũ�⸸ŭ ������ �����ص� �Ǵ��� üũ
            IDE_ASSERT( sPageOffset + SM_PAGE_SIZE - 1 < sReadSize );

            // PCH ��Ʈ���� Page �޸𸮸� �Ҵ��ϰ� Page���� ����
            IDE_TEST( fillPCHEntry(aTBSNode,
                                   sPID,
                                   SMM_FILL_PCH_OP_COPY_PAGE,
                                   (UChar *)aAlignedBuffer +
                                   sPageOffset
                                   ) != IDE_SUCCESS);
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION(read_size_error);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                    SM_TRC_MEMORY_LOADING_DATAFILE_FATAL);
        IDE_SET(ideSetErrorCode(smERR_FATAL_SysRead));
    }
    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/*
    [���ü� ����]
       Restart Recovery������ ��� �Ҹ���Ƿ�
       ���� Transaction���� ���ü� ��� ����� �ʿ䰡 ����.
 */
void smmManager::setLstCreatedDBFileToAllTBS ( )
{
    smmMemBase   sMemBase;
    smmTBSNode * sCurTBS;
    UInt         sDBFileCount;

    sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurTBS != NULL )
    {
        if ( sctTableSpaceMgr::isMemTableSpace(sCurTBS->mHeader.mID) == ID_TRUE )
        {

            sDBFileCount = 0;

            if (sCurTBS->mRestoreType == SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
            {
                // To Fix BUG-17997 [�޸�TBS] DISCARD�� Tablespace��
                //                  DROP�ÿ� Checkpoint Image �������� ����
                //
                // Restore���� ���� Tablespace�� DB�κ��� Membase�� �о
                // DB File������ �����Ѵ�.
                if ( smmManager::readMemBaseFromFile( sCurTBS,
                                                      &sMemBase )
                     == IDE_SUCCESS )
                {
                    sDBFileCount =
                        smmDatabase::getDBFileCount(
                            & sMemBase,
                            sCurTBS->mTBSAttr.mMemAttr.mCurrentDB);
                }
                else
                {
                    // Discard�� Tablespace�� ���
                    // �ƿ� DB������ ���� ���� �ִ�.

                    // �� ��� SKIP�Ѵ�.
                }
            }
            else
            {
                sDBFileCount =
                    smmDatabase::getDBFileCount(
                        sCurTBS->mMemBase,
                        sCurTBS->mTBSAttr.mMemAttr.mCurrentDB);
            }

            if ( sDBFileCount > 0 )
            {
                // ������ ������ DB���� ��ȣ�̹Ƿ�, DB���� �������� 1����
                sCurTBS->mLstCreatedDBFile = sDBFileCount - 1;
            }
        } // isMemTableSpace

        sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getNextSpaceNode( sCurTBS->mHeader.mID );
    }
}






/*
   �����ͺ��̽� ���� Page�� Free Page�� �Ҵ�� �޸𸮸� �����Ѵ�

   [���ü� ����]
      Restart Recovery������ ��� �Ҹ���Ƿ�
      ���� Transaction���� ���ü� ��� ����� �ʿ䰡 ����.
 */
IDE_RC smmManager::freeAllFreePageMemory()
{
    smmTBSNode * sCurTBS;

    sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurTBS != NULL )
    {
        if ( ( sctTableSpaceMgr::isMemTableSpace(sCurTBS->mHeader.mID) != ID_TRUE ) ||
             ( sCurTBS->mRestoreType == SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET ) )
        {
            sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getNextSpaceNode( sCurTBS->mHeader.mID );
            continue;
        }

        IDE_TEST( freeTBSFreePageMemory( sCurTBS )
                  != IDE_SUCCESS );

        sCurTBS = (smmTBSNode*)sctTableSpaceMgr::getNextSpaceNode( sCurTBS->mHeader.mID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * �����ͺ��̽� ���� Page�� Free Page�� �Ҵ�� �޸𸮸� �����Ѵ�
 *
 * �����ͺ��̽� Page�� Free Page���� Allocated Page������ Free List Info Page��
 * ���� �� �� �ִµ�, �� ������ Restart Recovery�� ������ ������ �Ϸ�ǹǷ�,
 * Restart Recovery�� �Ϸ�� �Ŀ� Free Page���� ���θ� �а��� ���� �ִ�.
 *
 * Restart Recovery������ Free Page����, Allocated Page���� �������� �ʰ�
 * ������ �������� ��ũ�κ��� �޸𸮷� �ε��Ѵ�.
 * �׸��� Restart Recovery�� �Ϸ�ǰ� ����, ���ʿ��ϰ� �ε�� Free Page����
 * Page �޸𸮸� �޸𸮸� �ݳ��Ѵ�.
 */
IDE_RC smmManager::freeTBSFreePageMemory(smmTBSNode * aTBSNode)
{
    scSpaceID sSpaceID;
    scPageID  sPID ;
    idBool    sIsFreePage;

    IDE_DASSERT( aTBSNode->mMemBase != NULL );

    sSpaceID = aTBSNode->mTBSAttr.mID;

    // BUGBUG kmkim Restart Recovery������ �Ҹ� ������ ASSERT�ɰ�.
    for ( sPID = SMM_DATABASE_META_PAGE_CNT;
          sPID < aTBSNode->mMemBase->mAllocPersPageCount;
          sPID ++ )
    {
        IDE_DASSERT( mPCArray[sSpaceID].mPC[ sPID ].mPCH != NULL );

        if ( mPCArray[sSpaceID].mPC[sPID].mPCH != NULL )
        {
            // BUG-31191 Page�� Alloc�Ǿ� �ִ� ��쿡�� �����Ѵ�.
            if ( mPCArray[sSpaceID].mPC[sPID].mPagePtr != NULL )
            {
                IDE_TEST( smmExpandChunk::isFreePageID ( aTBSNode,
                                                         sPID,
                                                         & sIsFreePage )
                          != IDE_SUCCESS );

                // ���̺� �Ҵ���� ���� �����ͺ��̽� Free Page�� ���
                if ( sIsFreePage == ID_TRUE )
                {
                    // ������ �޸𸮸� �ݳ��Ѵ�.
                    IDE_TEST( freePageMemory( aTBSNode, sPID ) != IDE_SUCCESS );
                }
            }
        }
    }

    // ���� �Ҵ�� Page�κ��� DB�� �ִ� ���� �� �ִ� Page������
    // Loop�� ���� Ȥ�� Page Memory�� �Ҵ�� Page�� �ִ��� �˻��Ѵ�.
    for ( sPID = aTBSNode->mMemBase->mAllocPersPageCount;
          sPID < aTBSNode->mDBMaxPageCount ;// BUG-15066 MEM_MAX_DB_SIZE ����� �ʵ���
          sPID ++ )
    {
        if ( mPCArray[sSpaceID].mPC[sPID].mPCH != NULL )
        {
            if ( mPCArray[sSpaceID].mPC[sPID].mPagePtr != NULL )
            {
                // Media Recovery�� Until Cancel�� ������ ��
                // Restart Recovery�Ϸ��ϸ�
                //
                // mAllocPersPageCount�� ��ġ�� Media Recovery���� ��������
                // �� �۾��� �� �ִ�.
                //
                // �� ��� mAllocPersPageCount ~ mDBMaxPageCount���̿�
                // Page�޸𸮰� �Ҵ�Ǿ� ���� �� �ִ�.
                // �̷��� ��� Page Memory�� �ݳ��� �־�� �Ѵ�.

                // ������ �޸𸮸� �ݳ��Ѵ�.
                IDE_TEST( freePageMemory( aTBSNode, sPID ) != IDE_SUCCESS );
            }
        }
    }

    // ���̺� �Ҵ�� �������̸鼭 �޸𸮰� ���� ��� �ִ��� üũ
    // Free Page�̸鼭 ������ �޸� ������ ��� �ִ��� üũ
    IDE_DASSERT( isAllPageMemoryValid(aTBSNode) == ID_TRUE );

    // Restart Recovery�� �Ϸ�� �����̹Ƿ�, Free Page List�� ���� Assertion����
    IDE_DASSERT( smmFPLManager::isAllFPLsValid(aTBSNode) == ID_TRUE );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}




/*
 * ���̺� �Ҵ�� �������̸鼭 �޸𸮰� ���� ��� �ִ��� üũ
 * Free Page�̸鼭 ������ �޸� ������ ��� �ִ��� üũ
 */
idBool smmManager::isAllPageMemoryValid(smmTBSNode * aTBSNode)
{
    void   * sPagePtr;
    idBool    sIsValid = ID_TRUE;
    scSpaceID sSpaceID;
    scPageID  sPID;
    idBool    sIsFreePage;

    IDE_DASSERT( aTBSNode->mMemBase != NULL );

    sSpaceID = aTBSNode->mTBSAttr.mID;

    // ���̺� �Ҵ�� �������̸鼭 �޸𸮰� ���� ��� �ִ��� üũ
    // ���̺� �Ҵ�� ���������� ���θ� Restart Recovery�� ������
    // Ȯ���� �����ϸ�, �� �Լ��� Restart Recovery�Ŀ�
    // ȣ��ǹǷ�, ���⿡�� üũ�� �ǽ��Ѵ�.
    for (sPID = 0; sPID < aTBSNode->mDBMaxPageCount; sPID++)
    {
        sPagePtr = mPCArray[sSpaceID].mPC[sPID].mPagePtr;

        if ( mPCArray[sSpaceID].mPC[sPID].mPCH != NULL)
        {
            if ( sPID < aTBSNode->mMemBase->mAllocPersPageCount )
            {
                IDE_ASSERT( smmExpandChunk::isFreePageID (aTBSNode,
                                                          sPID,
                                                          & sIsFreePage )
                            == IDE_SUCCESS );

                if ( sIsFreePage == ID_TRUE )
                {
                    // Free Page�ε� ������ �޸𸮰� ������ ����
                    if ( sPagePtr != NULL )
                    {
                        sIsValid = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    // Table�� �Ҵ�� ���������� ������ �޸𸮰� ������ ����
                    if ( sPagePtr == NULL )
                    {
                        sIsValid = ID_FALSE;
                        break;
                    }
                }
            }
            else
            {
                // ���� DB�� �Ҵ�� PAGE�� �ƴѵ�
                // ������ �޸𸮰� ������ ����
                if ( sPagePtr != NULL )
                {
                    sIsValid = ID_FALSE;
                    break;
                }
            }
        }
    }
    return sIsValid ;
}



/*
 * ��ũ���� �����ͺ��̽� �̹����� �޸𸮷� �ε��Ѵ� ( 1���� Thread�� ��� )
 *
 * aCurrentDB [IN] �ε� �Ϸ��� Ping-Pong �����ͺ��̽� ( 0 Ȥ�� 1 )
 * aOp [IN] Restore �ɼ�/����
 */
IDE_RC smmManager::loadSerial2( smmTBSNode     * aTBSNode)
{
    smmDatabaseFile *s_DbFile;
    ULong            s_nFileSize = 0;
    UInt             sDBFileCount;
    ULong            i;
    scPageID         sPageID = 0;
    vULong           sPageCountPerFile ;
    vULong           sWrittenPageCount;
    UInt             sCurrentDB = aTBSNode->mTBSAttr.mMemAttr.mCurrentDB;

    IDE_DASSERT( sCurrentDB == 0 || sCurrentDB == 1 );


    sDBFileCount = getRestoreDBFileCount( aTBSNode );

    /* ------------------------------------------------
     *  - ���õ� DB�� ���� �� ȭ���� ����
     * ----------------------------------------------*/
    for (i = 0; i < sDBFileCount; i++)
    {
        // �� ���Ͽ� ����� �� �ִ� Page�� ��
        sPageCountPerFile = smmManager::getPageCountPerFile( aTBSNode, i );

        // DB ������ Disk�� �����Ѵٸ�?
        if ( smmDatabaseFile::isDBFileOnDisk( aTBSNode, sCurrentDB, i )
             == ID_TRUE )
        {
            IDE_TEST( openAndGetDBFile( aTBSNode,
                                        sCurrentDB,
                                        i,
                                        &s_DbFile )
                      != IDE_SUCCESS );

            // ���� ���Ͽ� ��ϵ� Page�� ���� ���
            IDE_TEST(s_DbFile->getFileSize(&s_nFileSize) != IDE_SUCCESS);

            if ( s_nFileSize > SM_DBFILE_METAHDR_PAGE_SIZE )
            {
                sWrittenPageCount =
                    (s_nFileSize - SM_DBFILE_METAHDR_PAGE_SIZE)
                    / SM_PAGE_SIZE;

                // DB���Ϸκ��� Page�� �ε��Ѵ�.
                // ���� ��ϵ����� �ʾ����� �� ���Ͽ� ��ϵ� �� �ִ� Page�鿡
                // ���ؼ��� PCH ��Ʈ������ �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
                // �̿� ���� �ڼ��� ������ loadDbFile�� �ּ��� �����Ѵ�.
                IDE_TEST(loadDbFile(aTBSNode,
                                    i,
                                    sPageID,
                                    sPageID + sPageCountPerFile - 1,
                                    sWrittenPageCount ) != IDE_SUCCESS);
            }
            else
            {
                // To FIX BUG-18630
                // SM_DBFILE_METAHDR_PAGE_SIZE(8K)����
                // ���� DB������ ������ ��� Restart Recovery������
                //
                // 8K���� ���� ũ���� DB���Ͽ���
                // ������ �������� ��ϵ��� ���� ���̹Ƿ�
                // DB���� Restore�� SKIP�Ѵ�.
            }

        }
        else // DB File�� Disk�� ���� ���
        {
            if ( SMI_TBS_IS_DROPPED(aTBSNode->mHeader.mState) )
            {
                // Tablespace�� Drop�� Transaction�� Commit�� ��� ======
                // => nothing to do
                //
                // Drop Tablespace�� ���� Pending�۾� ���൵��
                // Server�� ����� ����̴�.
                //
                // Tablespace�� ���´� ( ONLINE | DROP_PENDING ) �̴�.
                //
                // �� ��쿡�� Drop Tablespace�� Pending�۾��� �����ϸ鼭
                // Checkpoint Image File�� �Ϻ�, Ȥ�� ���θ� �����Ͽ���
                // ���� �ִ�.
                //
                // �׷��Ƿ� File�� ���� ��� Load�� ���� �ʰ� SKIP�Ѵ�.
                //
                // ����> �ش� File�� ���� Page�� ���� Redo�� �߻��� ���
                //
                //   �� �Լ����� DB File Load�� ���� �ʾ����Ƿ�
                //   Page Memory�� NULL�̴�. ������ Redo�ÿ�
                //   Page Memory�� NULL�̸� Page�� �Ҵ��ϰ� Page�� �ʱ�ȭ
                //   �ϱ� ������ Redo�� �������� ����ȴ�.
                //
                //   ���� DROP TABLESPACE�α׸� ������ Tablespace��
                //   PAGE�ܰ踦 �����ϱ� ������ �ش� Page��
                //   �޸� �����ȴ�.
            }
            else
            {
                // �Ϲ����� ��� =========================================
                //
                // getRestoreDBFileCount�� Ȯ���ϰ� ������ DB File��
                // ���� ���� �����Ѵ�.
                // => DB File�� ���� ��Ȳ�� ���� �� ����.
                IDE_ASSERT(0);
            }
        }

        // PROJ-1490
        // DB���Ͼ��� Free Page�� Disk�� ���������� �ʰ�
        // �޸𸮷� �ö����� �ʴ´�.
        // �׷��Ƿ�, DB������ ũ��� DB���Ͽ� ����Ǿ�� �� Page���ʹ�
        // �ƹ��� ���谡 ����.
        //
        // �� DB������ ����ؾ��� Page�� ���� ����Ͽ�
        // �� DB������ �ε� ���� Page ID�� ����Ѵ�.
        sPageID += sPageCountPerFile;
    }

    aTBSNode->mStartupPID = sPageID;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * �ý����� �����޸𸮿� �����ϴ� �����ͺ��̽� ���������� ATTACH�Ѵ�.
 *
 * aTBSNode   [IN] Page�� Attach�� Tablespace
 * aShmHeader [IN] �����޸� ���
 * aOp        [IN] Prepare �ɼ�/����
 */
IDE_RC smmManager::restoreAttachSharedDB(smmTBSNode       * aTBSNode,
                                         smmShmHeader     * aShmHeader,
                                         smmPrepareOption   aOp )
{
    scSpaceID     sSpaceID;
    scPageID      i;
    smmTempPage * sPageSelf;
    smmTempPage * sCurPage;
    smmSCH      * sSCH;
    UChar       * sBasePage;

    IDE_DASSERT( aShmHeader != NULL );

    IDE_ASSERT( aTBSNode->mRestoreType == SMM_DB_RESTORE_TYPE_SHM_ATTACH );

    sSpaceID = aTBSNode->mTBSAttr.mID;

    /* ------------------------------------------------
     * [1] Base Shared Memory�� ���� ����
     * ----------------------------------------------*/
    IDE_TEST(smmFixedMemoryMgr::attach(aTBSNode, aShmHeader) != IDE_SUCCESS);

    // SHM Page�� ���󰡸鼭 [ Reverse Mapping ]
    //
    // Allocated Page�� ������ �޸𸮸� �Ҵ��Ͽ����Ƿ�,
    // Free Page�� ���ؼ��� fillPCHEntry�� ȣ����� �ʴ´�.
    sSCH = aTBSNode->mBaseSCH.m_next;
    while(sSCH != NULL)
    {
        sBasePage = (UChar *)sSCH->m_header + SMM_CACHE_ALIGNED_SHM_HEADER_SIZE;

        for (i = 0; i < sSCH->m_header->m_page_count; i++)
        {
            sCurPage  = (smmTempPage *)(sBasePage + (SM_PAGE_SIZE * i));
            sPageSelf = sCurPage->m_header.m_self;

            if ( sPageSelf == (smmTempPage *)SMM_SHM_LOCATION_FREE )
            {
                /* BUG-19583: shmutil�� Shared Memory DB������ �б⸸ �ؾ��ϴµ�
                 * Page Link�� �����ϴ� ��찡 �ֽ��ϴ�.
                 *
                 * Page�� Free�ϰ�� Free List�� �������� �����ϴ� �۾��� �ϴµ�
                 * Shmutil�� ��쿡�� �� �۾��� �ϸ� �ȵȴ�. ���� Read�� �ؾ�
                 * �Ѵ�.
                 * */
                if( aOp != SMM_PREPARE_OP_DONT_CHECK_DB_SIGNATURE_4SHMUTIL )
                {
                    IDE_TEST(smmFixedMemoryMgr::freeShmPage(
                                                     aTBSNode,
                                                     (smmTempPage *)sCurPage)
                             != IDE_SUCCESS);
                }
            }
            else
            {
                IDE_TEST( fillPCHEntry( aTBSNode,
                                        smLayerCallback::getPersPageID( sCurPage ),
                                        SMM_FILL_PCH_OP_SET_PAGE,
                                        sCurPage )
                          != IDE_SUCCESS );
            }
        }
        sSCH = sSCH->m_next;
    }
    IDE_TEST(setupBasePageInfo(
                 aTBSNode,
                 (UChar *)mPCArray[sSpaceID].mPC[SMM_MEMBASE_PAGEID].mPagePtr)
            != IDE_SUCCESS);

    if ( aOp ==  SMM_PREPARE_OP_DONT_CHECK_DB_SIGNATURE_4SHMUTIL )
    {
        // shmutil�� ȣ���� ����̴�.
        // Disk�� DB������ �������� ���� ���� �ִ� ��Ȳ�̹Ƿ�
        // DB Signature�� Check���� �ʴ´�
    }
    else
    {
        IDE_TEST(smmFixedMemoryMgr::checkDBSignature(
                     aTBSNode,
                     aTBSNode->mTBSAttr.mMemAttr.mCurrentDB) != IDE_SUCCESS);
    }


    /////////////////////////////////////////////////////////////////////
    // Free Page�� ���ؼ��� PCH Entry���� ������ �ʾҴ�.
    // Free Page���� PCH Entry�� �����Ѵ�.
    //
    // PCH��Ʈ���� �������� ���� ��� Page�鿡 ���� PCH ��Ʈ���� �������ش�.
    /////////////////////////////////////////////////////////////////////
/*
    mStartupPID = 0;

    for (i = 0; i < getDbFileCount( aDbNumber ) ; i++ )
    {
        mStartupPID += smmManager::getPageCountPerFile( i );
    }
*/
    // Checkpoint�� �߻����� ������, �Ҵ�� PAGE�ӿ��� �ұ��ϰ� DISK
    // ������ �������� �ʱ� ������ Disk���� ������ ���� mStartupPID��
    // ����س��� �ȵȴ�.
    aTBSNode->mStartupPID = aTBSNode->mMemBase->mAllocPersPageCount;

    // ��� �����ͺ��̽� Page�� ���� PCH ��Ʈ���� �����ǵ��� �Ѵ�.
    for ( i = 0;
          i < aTBSNode->mStartupPID ;
          i ++ )
    {
        if ( mPCArray[sSpaceID].mPC[i].mPCH == NULL )
        {
            IDE_TEST( allocPCHEntry( aTBSNode, i ) != IDE_SUCCESS );
        }
    }

    // To Fix BUG-15112
    // Restart Recovery�߿� Page Memory�� NULL�� Page�� ���� Redo��
    // �ش� �������� �׶� �׶� �ʿ��� ������ �Ҵ��Ѵ�.
    // ���⿡�� ������ �޸𸮸� �̸� �Ҵ��ص� �ʿ䰡 ����


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



// SyncDB
// 1. ���(CheckPoint)�� ��쿡�� ��� Online Tablespace�� ���ؼ�
// Sync�� �����Ѵ�.
// 2. �̵����߿��� ��� Online/Offline TableSpace �� �̵�� ����
// ����� �͸� Sync�� �����Ѵ�.
//
// [ ���� ]
// [IN] aSkipStateSet - Sync���� ���� TBS ���� ����
// [IN] aSyncLatch    - SyncLatch ȹ���� �ʿ��Ѱ��
IDE_RC smmManager::syncDB( sctStateSet aSkipStateSet,
                           idBool aSyncLatch )
{
    UInt         sStage = 0;

    sctTableSpaceNode * sSpaceNode;

    sSpaceNode = sctTableSpaceMgr::getFirstSpaceNode();

    while( sSpaceNode != NULL )
    {
        if ( sctTableSpaceMgr::isMemTableSpace( sSpaceNode ) == ID_TRUE )
        {
            if ( aSyncLatch == ID_TRUE )
            {
                // TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ��� ����
                IDE_TEST( sctTableSpaceMgr::latchSyncMutex( sSpaceNode )
                          != IDE_SUCCESS );
                sStage = 1;
            }

            if ( ((smmTBSNode*)sSpaceNode)->mRestoreType
                 != SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET )
            {
                // TBS�� Memory�� Loading �� ����

                if ( sctTableSpaceMgr::hasState(
                            sSpaceNode->mID,
                            aSkipStateSet ) == ID_TRUE )
                {
                    // TBS�� sync�� �� ���� ����
                    // 1. Checkpoint �� -> DISCARD/DROPPED/OFFLINE
                    // 2. Media Recovery �� -> DISCARD/DROPPED
                }
                else
                {
                    // TBS�� sync�� �� �ִ� ����
                    IDE_TEST( syncTBS( (smmTBSNode*) sSpaceNode )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // TBS�� Memory�� Restore ���� ���� ����
            }

            if ( aSyncLatch == ID_TRUE )
            {
                sStage = 0;
                IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( sSpaceNode )
                          != IDE_SUCCESS );
            }
        }
        sSpaceNode = sctTableSpaceMgr::getNextSpaceNode( sSpaceNode->mID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex(sSpaceNode)
                            == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC smmManager::syncTBS(smmTBSNode * aTBSNode)
{
    UInt sDBFileCount;
    UInt i;
    UInt sStableDB = (aTBSNode->mTBSAttr.mMemAttr.mCurrentDB + 1) % SM_PINGPONG_COUNT;

    sDBFileCount = getDbFileCount(aTBSNode, sStableDB);
    for ( i = 0; i < sDBFileCount; i++ )
    {
        if(((smmDatabaseFile*)aTBSNode->mDBFile[sStableDB][i])->isOpen()
           == ID_TRUE)
        {
            IDE_TEST(
                ((smmDatabaseFile*)aTBSNode->mDBFile[sStableDB][i])->syncUntilSuccess()
                     != IDE_SUCCESS);
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  alloc & free PCH + Page
 * ----------------------------------------------*/

IDE_RC smmManager::allocPCHEntry(smmTBSNode *  aTBSNode,
                                 scPageID      aPageID)
{

    SChar         sMutexName[128];
    smmPCH      * sCurPCH;
    scSpaceID     sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_ASSERT( sSpaceID < SC_MAX_SPACE_COUNT );

    IDE_DASSERT( isValidPageID( sSpaceID, aPageID ) == ID_TRUE );

    IDE_ASSERT(mPCArray[sSpaceID].mPC[aPageID].mPCH == NULL);

    /* smmManager_allocPCHEntry_alloc_CurPCH.tc */
    IDU_FIT_POINT("smmManager::allocPCHEntry::alloc::CurPCH");
    IDE_TEST( aTBSNode->mPCHMemPool.alloc((void **)&sCurPCH) != IDE_SUCCESS);
    mPCArray[sSpaceID].mPC[aPageID].mPCH = sCurPCH;

    /* ------------------------------------------------
     * [] mutex �ʱ�ȭ
     * ----------------------------------------------*/

    idlOS::snprintf( sMutexName,
                     128,
                     "SMMPCH_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_MUTEX",
                     (UInt) sSpaceID,
                     (UInt) aPageID );

    IDE_TEST(sCurPCH->mMutex.initialize( sMutexName,
                                         IDU_MUTEX_KIND_NATIVE,
                                         IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);


    idlOS::snprintf( sMutexName,
                     128,
                     "SMMPCH_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"_PAGE_MEMORY_LATCH",
                     (UInt) sSpaceID,
                     (UInt) aPageID );

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_ASSERT( sCurPCH->mPageMemLatch.initialize(  
                                      sMutexName,
                                      IDU_LATCH_TYPE_NATIVE )
                == IDE_SUCCESS );

    sCurPCH->m_dirty           = ID_FALSE;
    sCurPCH->m_dirtyStat       = SMM_PCH_DIRTY_STAT_INIT;
    sCurPCH->m_pnxtDirtyPCH    = NULL;
    sCurPCH->m_pprvDirtyPCH    = NULL;
    sCurPCH->mNxtScanPID       = SM_NULL_PID;
    sCurPCH->mPrvScanPID       = SM_NULL_PID;
    sCurPCH->mModifySeqForScan = 0;
    sCurPCH->mSpaceID          = sSpaceID;
    mPCArray[sSpaceID].mPC[aPageID].mPagePtr = NULL;

    // smmPCH.mFreePageHeader �ʱ�ȭ
    IDE_TEST( smLayerCallback::initializeFreePageHeader( sSpaceID, aPageID )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}
/* PCH ����
 *
 * aPID      [IN] PCH�� �����ϰ��� �ϴ� Page ID
 * aPageFree [IN] PCH�Ӹ� �ƴ϶� �� ���� Page �޸𸮵� ������ ������ ����
 */

IDE_RC smmManager::freePCHEntry(smmTBSNode * aTBSNode,
                                scPageID     aPID,
                                idBool       aPageFree )
{
    smmPCH    *sCurPCH;
    scSpaceID  sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( isValidPageID( sSpaceID, aPID ) == ID_TRUE );

    sCurPCH = (smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH;

    IDE_ASSERT(sCurPCH != NULL);

    // smmPCH.mFreePageHeader ����
    IDE_TEST( smLayerCallback::destroyFreePageHeader( sSpaceID, aPID )
              != IDE_SUCCESS );

    if (aPageFree == ID_TRUE )
    {
        // Free Page��� Page�޸𸮰� �̹� �ݳ��Ǿ�
        // �޸𸮸� Free���� �ʾƵ� �ȴ�.
        // Page �޸𸮰� �ִ� ��쿡�� �޸𸮸� �ݳ��Ѵ�.
        if ( mPCArray[sSpaceID].mPC[aPID].mPagePtr != NULL )
        {
            IDE_TEST( freePageMemory( aTBSNode, aPID ) != IDE_SUCCESS );
        }
    }


    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    IDE_TEST( sCurPCH->mPageMemLatch.destroy() != IDE_SUCCESS);

    IDE_TEST( sCurPCH->mMutex.destroy() != IDE_SUCCESS );

    // ���� �ý��ۿ� �����ϴ� Tablespace���
    // �����Ϸ��� PCH�� Dirty Page�� �ƴϾ�� �Ѵ�.
    IDE_ASSERT(sCurPCH->m_dirty == ID_FALSE);
    IDE_ASSERT((sCurPCH->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
               == SMM_PCH_DIRTY_STAT_INIT);

    sCurPCH->m_pnxtDirtyPCH    = NULL;
    sCurPCH->m_pprvDirtyPCH    = NULL;
    sCurPCH->mNxtScanPID       = SM_NULL_PID;
    sCurPCH->mPrvScanPID       = SM_NULL_PID;
    sCurPCH->mModifySeqForScan = 0;

    IDE_TEST( aTBSNode->mPCHMemPool.memfree(sCurPCH) != IDE_SUCCESS);

    mPCArray[ sSpaceID ].mPC[aPID].mPCH = NULL;
    mPCArray[ sSpaceID ].mPC[aPID].mPagePtr   = NULL;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
     Dirty Page�� PCH�� ��ϵ� Dirty Flag�� ��� �ʱ�ȭ�Ѵ�.

     [IN] aTBSNode - Dirty Page Flag�� ��� ������ Tablespace Node
 */
IDE_RC smmManager::clearDirtyFlag4AllPages(smmTBSNode * aTBSNode )
{
    UInt i;
    smmPCH   *sPCH;

    IDE_DASSERT( aTBSNode != NULL );

    for (i = 0; i < aTBSNode->mDBMaxPageCount; i++)
    {
        sPCH = getPCH(aTBSNode->mTBSAttr.mID, i);

        if (sPCH != NULL)
        {
            sPCH->m_dirty = ID_FALSE;
            sPCH->m_dirtyStat = SMM_PCH_DIRTY_STAT_INIT;
        }
    }

    return IDE_SUCCESS;
}

/*
 * �����ͺ��̽��� PCH, Page Memory�� ��� Free�Ѵ�.
 *
 * aPageFree [IN] Page Memory�� Free�� ���� ����.
 */
IDE_RC smmManager::freeAll(smmTBSNode * aTBSNode, idBool aPageFree)
{
    scPageID i;

    for (i = 0; i < aTBSNode->mDBMaxPageCount; i++)
    {
        smmPCH *sPCH = getPCH(aTBSNode->mTBSAttr.mID, i);

        if (sPCH != NULL)
        {
            IDE_TEST( freePCHEntry(aTBSNode, i, aPageFree) != IDE_SUCCESS);
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* �����ͺ��̽� Ÿ�Կ� ���� �����޸𸮳� �Ϲ� �޸𸮸� ������ �޸𸮷� �Ҵ��Ѵ�
 *
 * aPage [OUT] �Ҵ�� Page �޸�
 */
IDE_RC smmManager::allocDynOrShm( smmTBSNode   * aTBSNode,
                                  void        ** aPageMemHandle,
                                  smmTempPage ** aPage,
                                  idBool         aIsDataPage )
{
    IDE_DASSERT( aPage != NULL );

    *aPageMemHandle = NULL;
    *aPage          = NULL;

    switch( aTBSNode->mRestoreType )
    {
        // �Ϲ� �޸𸮷κ��� Page �޸𸮸� �Ҵ�
        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            /* smmManager_allocDynOrShm_alloc_Page.tc */
            IDU_FIT_POINT("smmManager::allocDynOrShm::alloc::Page");
            
            // BUG-47487: DATA / FLI ������ �޸� alloc �и�
            if ( aIsDataPage == ID_TRUE )
            {
                IDE_TEST( aTBSNode->mDynamicMemPagePool.alloc( aPageMemHandle,
                                                               (void **)aPage )
                          != IDE_SUCCESS);
            }
            else 
            {
                IDE_TEST( aTBSNode->mFLIMemPagePool.alloc( aPageMemHandle,
                                                               (void **)aPage )
                          != IDE_SUCCESS);
            }
            break;

        // ���� �޸𸮷κ��� Page �޸𸮸� �Ҵ�
        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
        case SMM_DB_RESTORE_TYPE_SHM_ATTACH :
            IDE_TEST( smmFixedMemoryMgr::allocShmPage( aTBSNode, aPage )
                      != IDE_SUCCESS );
            break;

        // ��� �޸𸮸� ����ؾ� �� �� �� �� ����.
        case SMM_DB_RESTORE_TYPE_NONE :
        default :
            IDE_ASSERT( 0 );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}


/* �����ͺ��̽�Ÿ�Կ����� Page�޸𸮸� �����޸𸮳� �Ϲݸ޸𸮷� �����Ѵ�.
 *
 * aPage [IN] ������ Page �޸�
 */
IDE_RC smmManager::freeDynOrShm( smmTBSNode  * aTBSNode,
                                 void        * aPageMemHandle,
                                 smmTempPage * aPage,
                                 idBool        aIsDataPage )
{
    IDE_DASSERT( aPage != NULL );

    switch( aTBSNode->mRestoreType )
    {
        // �Ϲ� �޸𸮿� Page �޸𸮸� ����
        case SMM_DB_RESTORE_TYPE_DYNAMIC :
            // BUG-47487: DATA / FLI ������ �޸� ���� �и� 
            if ( aIsDataPage == ID_TRUE )
            {
                IDE_TEST( aTBSNode->mDynamicMemPagePool.memFree( aPageMemHandle )
                          != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST( aTBSNode->mFLIMemPagePool.memFree( aPageMemHandle )
                          != IDE_SUCCESS);
            }
            break;

        // ���� �޸𸮿� Page �޸𸮸� ����
        case SMM_DB_RESTORE_TYPE_SHM_CREATE :
        case SMM_DB_RESTORE_TYPE_SHM_ATTACH :
            IDE_TEST( smmFixedMemoryMgr::freeShmPage( aTBSNode, aPage )
                      != IDE_SUCCESS );
            break;

        // ��� �޸𸮸� ����ؾ� �� �� �� �� ����.
        case SMM_DB_RESTORE_TYPE_NONE :
        default :
            IDE_ASSERT( 0 );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}

/* Ư�� Page�� PCH���� Page Memory�� �Ҵ��Ѵ�.
 *
 * aPID [IN] Page Memory�� �Ҵ��� Page�� ID
 */
IDE_RC smmManager::allocPageMemory( smmTBSNode * aTBSNode, scPageID aPID )
{
    scSpaceID sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( isValidPageID( sSpaceID, aPID ) == ID_TRUE );
    IDE_ASSERT( mPCArray[sSpaceID].mPC[aPID].mPCH != NULL );

    // Recovery�߿��� Redo/Undo�� ������ �޸𸮰� NULL�̸�
    // �׻� Page�޸𸮸� ���� �Ҵ��ϵ��� �Ǿ��ִ�.
    //
    // ex1> smrRecoveryMgr::redo�� SMR_LT_DIRTY_PAGE ó���κп���
    //     SMM_PID_PTR(sArrPageID[i]) => ���⿡�� �޸� �Ҵ�

    // ex2> REDO�� �� ������ ALTER TABLE�� Undo�ÿ�
    //      Page �޸� �Ҵ��� �õ��ϴµ�
    //      �� �� free page�� �޸𸮰� �Ҵ�Ǿ� ���� �� �ִ�.
    //
    // Recovery�� �ƴ� ��쿡�� Ȯ���Ѵ�.

    // BUG-23146 TC/Recovery/OnlineBackupRec/onlineBackupServerStop.sql ����
    // ������ ������ �����մϴ�.
    // Media Recovery������ page�� �Ҵ�Ǿ� �ִ� ��찡 �ֽ��ϴ�.
    // Restart Recovery�� ��쿡�� Ȯ������ �ʵ��� �Ǿ��ִ� �ڵ带
    // Media Recovery�� ��쿡�� Ȯ������ �ʵ��� �����մϴ�.
    if ( ( smLayerCallback::isRestartRecoveryPhase() == ID_FALSE ) &&
         ( smLayerCallback::isMediaRecoveryPhase() == ID_FALSE ) )
    {
        // Page Memory�� �Ҵ�Ǿ� ���� �ʾƾ� �Ѵ�.
        IDE_ASSERT( mPCArray[sSpaceID].mPC[aPID].mPagePtr == NULL );
    }

    if ( mPCArray[sSpaceID].mPC[aPID].mPagePtr == NULL )
    {
        // BUG-47487: DATA / FLI ������ ��� ������ alloc 
        if ( smmExpandChunk::isFLIPageID( aTBSNode, aPID ) == ID_TRUE )
        {
            // FLI
            IDE_TEST( allocDynOrShm( aTBSNode,
                                     &((smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH)->mPageMemHandle,
                                     (smmTempPage **) & mPCArray[sSpaceID].mPC[aPID].mPagePtr,
                                     ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            // DATA
            IDE_TEST( allocDynOrShm( aTBSNode,
                                     &((smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH)->mPageMemHandle,
                                     (smmTempPage **) & mPCArray[sSpaceID].mPC[aPID].mPagePtr )
                      != IDE_SUCCESS );
        }
    }

#ifdef DEBUG_SMM_FILL_GARBAGE_PAGE
    idlOS::memset( sPCH->mPagePtr, 0x43, SM_PAGE_SIZE );
#endif // DEBUG


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}


/*
 * ������ �޸𸮸� �Ҵ��ϰ�, �ش� Page�� �ʱ�ȭ�Ѵ�.
 * �ʿ��� ���, ������ �ʱ�ȭ�� ���� �α��� �ǽ��Ѵ�
 *
 * To Fix BUG-15107 Checkpoint�� Dirty Pageó������ ������ ������ ����
 * => ������ �޸��Ҵ�� �ʱ�ȭ�� ��� ó��,
 * => smmPCHEntry�� mPageMemMutex�� checkpoint�� page�Ҵ�tx���� ���ü� ����
 *
 * aTrans   [IN] ������ �ʱ�ȭ �α׸� ����� Ʈ�����
 *               aTrans == NULL�̸� �α����� �ʴ´�.
 * aPID     [IN] ������ �޸𸮸� �Ҵ��ϰ� �ʱ�ȭ�� ������ ID
 * aPrevPID [IN] �Ҵ��� �������� ���� Page ID
 * aNextPID [IN] �Ҵ��� �������� ���� Page ID
 */
IDE_RC smmManager::allocAndLinkPageMemory( smmTBSNode * aTBSNode,
                                           void     *   aTrans,
                                           scPageID     aPID,
                                           scPageID     aPrevPID,
                                           scPageID     aNextPID )
{
    UInt         sStage = 0;
    idBool       sPageIsDirty=ID_FALSE;
    smmPCH     * sPCH   = NULL;
    scSpaceID    sSpaceID = aTBSNode->mTBSAttr.mID;

    // ���� ���ռ� �˻� ( aTrans�� NULL�� ���� �ִ�. )
    IDE_DASSERT( aPID != SM_NULL_PID );
    IDE_DASSERT( isValidPageID( sSpaceID, aPID ) == ID_TRUE );

#ifdef DEBUG
    if ( aPrevPID != SM_NULL_PID )
    {
        IDE_DASSERT( isValidPageID( sSpaceID, aPrevPID ) == ID_TRUE );
    }

    if ( aNextPID != SM_NULL_PID )
    {
        IDE_DASSERT( isValidPageID( sSpaceID, aNextPID ) == ID_TRUE );
    }
#endif

    sPCH = (smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH;

    IDE_ASSERT( sPCH != NULL );

    // smrRecoveryMgr::chkptFlushMemDirtyPages ���� smrDirtyPageList��
    // add�ϴ� �ڵ�� �����ϴ� ������ �޸� ���ؽ�
    IDE_TEST( sPCH->mMutex.lock( NULL ) != IDE_SUCCESS );
    sStage = 1;

    // Page Memory�� �Ҵ��Ѵ�.
    IDE_TEST( allocPageMemory( aTBSNode, aPID ) != IDE_SUCCESS );

    if ( aTrans != NULL )
    {
        IDE_TEST( smLayerCallback::updateLinkAtPersPage( NULL, /* idvSQL* */
                                                         aTrans,
                                                         sSpaceID,
                                                         aPID,
                                                         SM_NULL_PID,
                                                         SM_NULL_PID,
                                                         aPrevPID,
                                                         aNextPID )
                  != IDE_SUCCESS );

    }


    sPageIsDirty = ID_TRUE;
    smLayerCallback::linkPersPage( mPCArray[sSpaceID].mPC[aPID].mPagePtr,
                                   aPID,
                                   aPrevPID,
                                   aNextPID );

    // ����! smrRecoveryMgr::chkptFlushMemDirtyPages ����
    // smmDirtyPageMgr�� ���ϴ� Mutex�� ���� ��� PageMemMutex�� ��� �ִ�.
    // ���� PageMemMutex�� ������� smmDirtyPageMgr�� Mutex�� ������
    // Dead Lock�� �߻��Ѵ�.
    //
    // Page Memory �Ҵ�� �ʱ�ȭ�� �ϴ� ���� Checkpoint�� �� Page��
    // ������ �ϴ� ���� mMutex�� �뵵�̹Ƿ�,
    // insDirtyPage���� mMutex�� Ǯ� �������.
    sStage = 0;
    IDE_TEST( sPCH->mMutex.unlock() != IDE_SUCCESS );

    sPageIsDirty = ID_FALSE;
   IDE_TEST( smmDirtyPageMgr::insDirtyPage( sSpaceID, aPID ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sPCH->mMutex.unlock() == IDE_SUCCESS );
            default:
                break;
        }

        if ( sPageIsDirty )
        {
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage( sSpaceID, aPID )
                        == IDE_SUCCESS );
        }
    }
    IDE_POP();


    return IDE_FAILURE;
}

/* Ư�� Page�� PCH���� Page Memory�� �����Ѵ�.
 *
 * aPID [IN] Page Memory�� �ݳ��� Page�� ID
 */
IDE_RC smmManager::freePageMemory( smmTBSNode * aTBSNode, scPageID aPID )
{
    smmPCH     * sPCH;
    UInt         sStage = 0;
    scSpaceID    sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( isValidPageID( sSpaceID, aPID ) == ID_TRUE );

    sPCH = (smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH;

    IDE_ASSERT( sPCH != NULL );

    // Free�� Page�̹Ƿ� Dirty Page��� �ϴ��� Disk�� ���� �ʿ�� ������,
    // ���� dirty page manager�� checkpoint ��ƾ��
    // �ѹ� �߰��� Dirty Page�� dirty page list���� �����ϱ� ���� �����̴�.
    //
    // Free Page�� Page �޸𸮴� �ݳ��Ͽ� NULL�� �����صΰ�
    // Dirty Page List���� �״�� dirty�� ���·� �д�.
    //
    // �׷��� checkpoint�ÿ� Dirty Page�� Page �޸𸮰� NULL�̸�,
    // Dirty Page�� ��ϵǾ��ٰ� Free Page�� �ݳ��� Page�̹Ƿ� �����Ѵ�.
    // ���߿� restart recovery�ÿ� �ش� Page�� Free Page�̹Ƿ�,
    // Disk���� �о������ �ʱ� ������,
    // �̿Ͱ��� Dirty Page�� Flush���� �ʰ� �����ϴ� ���� �����ϴ�.
    //
    // ������ ����, freePageMemory�Ϸ��� ������
    // checkpoint�� �������� �� �ִ� ����̴�.
    // sPID�� Dirty Page�̾ Dirty Page�� Page�޸𸮸� Flush�ϰ� �ִٸ�
    // Page �޸𸮸� �ٷ� �����ϸ� �ȵȴ�.

    IDE_TEST( sPCH->mMutex.lock( NULL /* idvSQL* */ )
              != IDE_SUCCESS );
    sStage = 1;

    // Page Memory�� �Ҵ�Ǿ� �־�� �Ѵ�.
    IDE_ASSERT( mPCArray[sSpaceID].mPC[aPID].mPagePtr != NULL );

    // BUG-47487: DATA / FLI ������ ��� ������ free 
    if ( smmExpandChunk::isFLIPageID( aTBSNode, aPID ) == ID_TRUE )
    {
        //FLI
        IDE_TEST( freeDynOrShm( aTBSNode,
                                sPCH->mPageMemHandle,
                                (smmTempPage*)mPCArray[sSpaceID].mPC[aPID].mPagePtr,
                                ID_FALSE )
                  != IDE_SUCCESS );
    }
    else
    {
        //DATA
        IDE_TEST( freeDynOrShm( aTBSNode,
                                sPCH->mPageMemHandle,
                                (smmTempPage*)mPCArray[sSpaceID].mPC[aPID].mPagePtr )
                  != IDE_SUCCESS );
    }

    mPCArray[sSpaceID].mPC[aPID].mPagePtr = NULL;

    // checkpoint�ÿ� smmDirtyPageList -> smrDirtyPageList �� Dirty Page��
    // �̵���Ű�µ�, ���� �̶� mPagePtr == NULL�̸�, �̵��� ���� �ʴ´�.
    // �׷� ���, smmDirtyPageList�� �߰��� Page�̹Ƿ�,
    // m_dirty == ID_TRUE������, smrDirtyPageList���� �߰����� ���� ä��
    // ��ġ�ȴ�.
    //
    // �׸��� �ش� Page�� ���Ǿ mPagePtr != NULL�� ä�� smmDirtyPageList��
    // �������� �ص�, m_dirty �� ID_TRUE���� Dirty Page�� �߰��� ���� �ʴ´�.
    //
    // �������� �޸𸮰� �����Ǿ� mPagePtr�� NULL�� �Ǵ� ��������
    // m_dirty �÷��׸� ID_FALSE�� �����Ͽ� ���� �������� �޸𸮰� �Ҵ�ǰ�,
    // smmDirtyPageList�� �������� �߰��ϰ��� �� ��, m_dirty �� ID_TRUE����
    // �߰����� �ʴ� �������� �̿��� �����Ѵ�.
    sPCH->m_dirty = ID_FALSE;

    sStage = 0;
    IDE_TEST( sPCH->mMutex.unlock() != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sPCH->mMutex.unlock() == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}



/*
 * Page�� PCH(Page Control Header)������ �����Ѵ�.
 *
 * aPID  [IN] PCH Entry�� �����Ϸ��� �ϴ� Page ID
 * aOP   [IN] ������ �����͸� �������� �ƴϸ� �޸� �����͸� �������� ����
 * aPage [IN] ��ũ�κ��� �о���� ������ ������
 */
IDE_RC smmManager::fillPCHEntry( smmTBSNode *       aTBSNode,
                                 scPageID           aPID,
                                 smmFillPCHOption   aFillOption,
                                                   /* =SMM_FILL_PCH_OP_NONE*/
                                 void             * aPage /* =NULL */ )
{
    smmPCH     * sPCH;
#ifdef DEBUG
    idBool   sIsFreePage;
#endif
    scSpaceID  sSpaceID = aTBSNode->mTBSAttr.mID;

    IDE_DASSERT( isValidPageID( sSpaceID, aPID ) == ID_TRUE );

    sPCH = (smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH;

    if ( sPCH == NULL )
    {
        IDE_TEST( allocPCHEntry( aTBSNode, aPID ) != IDE_SUCCESS );
        sPCH = (smmPCH*)mPCArray[sSpaceID].mPC[aPID].mPCH;

        IDE_ASSERT( sPCH != NULL );

        // ������ �����Ͱ� NULL�� �ƴ϶�� Restore������ ���Ѵ�.
        // Restore�� ��ũ���κ��� �޸𸮷� Page�� �о���̴� ������ ���Ѵ�.
        if ( aPage != NULL )
        {
            /* BUG-43789 Restore�� FPLIP�� Allocated �Ǿ��ٰ� �Ǿ� ������
             * �ش��ϴ� Page�� Flush ���� ���� ��� ���� �������� �� Page�� �� �ִ�.
             */
            if ( ((smpPersPageHeader*)aPage)->mSelfPageID == aPID )
            {
                switch ( aFillOption )
                {
                    case SMM_FILL_PCH_OP_COPY_PAGE :

                        IDE_TEST( allocPageMemory(aTBSNode, aPID ) != IDE_SUCCESS );
                        idlOS::memcpy( mPCArray[sSpaceID].mPC[aPID].mPagePtr, aPage, SM_PAGE_SIZE );
                        break;
                    case SMM_FILL_PCH_OP_SET_PAGE :
                        mPCArray[sSpaceID].mPC[aPID].mPagePtr = aPage;
                        break;
                        // ������ �����Ͱ� NULL�� �ƴ� ��쿡��
                        // aFillOption�� �׻� COPY_PAGE�� SET_PAGE��
                        // �ϳ��� ��쿩�� �Ѵ�.
                    case SMM_FILL_PCH_OP_NONE :
                    default:
                        IDE_ASSERT(0);
                }
            }
            else
            {
                /* BUG-43789 Page Header�� ID�� ��û�� PageID�� �ٸ����
                 * �ش� Page�� �״�� �о�� �ȵȴ�.
                 * Page Memory�� ���� �Ҵ��Ͽ� Page Header�� SelfID���� �ʱ�ȭ �Ͽ��ָ�
                 * Recovery �ܰ迡�� �������� Page�� ������ ���̴�. */
                IDE_TEST( allocPageMemory( aTBSNode, aPID ) != IDE_SUCCESS );

                /* BUG-44136 Memory Page loading �� flush���� ���� FLI Page��
                 *           ���� �Ҵ� �ߴٸ� �ʱ�ȭ �ؾ� �մϴ�.
                 * Meta, FLI Page�� recovery���� �����ϴ� ��찡 �ִ�.
                 * allocPageMemory�� mempool���� �������Ƿ� ������ ���� ����ִ�.
                 * �ּ��� �� ������ ������ ���� �ξ�� �Ѵ�.
                 */
                if( smmExpandChunk::isDataPageID( aTBSNode, aPID ) != ID_TRUE )
                {
                    idlOS::memset(  mPCArray[sSpaceID].mPC[aPID].mPagePtr , 0x0, SM_PAGE_SIZE );
                }

                smLayerCallback::linkPersPage(  mPCArray[sSpaceID].mPC[aPID].mPagePtr ,
                                               aPID,
                                               SM_NULL_PID,
                                               SM_NULL_PID );
            }
        }
        else
        {
            IDE_TEST( allocPageMemory( aTBSNode, aPID ) != IDE_SUCCESS );
        }
    }
    else // sPCH != NULL
    {
        // PCH�� �� �ִ� ��� Page �����͸� ó���� �� ����.
        // Page �����͸� �����Ϸ��� ��쿡�� �׻� PCH�� NULL�̾�� ��
        IDE_ASSERT( aPage == NULL );

#ifdef DEBUG
        IDE_TEST( smmExpandChunk::isFreePageID (aTBSNode,
                                                aPID,
                                                & sIsFreePage )
                  != IDE_SUCCESS );

        if ( sIsFreePage == ID_FALSE )
        {
            // PCH�� NULL�� �ƴϸ鼭 PCH�� Page�� NULL�̶�� ������ ���δ�.
            IDE_ASSERT( mPCArray[sSpaceID].mPC[aPID].mPagePtr != NULL );
        }
#endif
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;
}



/*
 * �ϳ��� Page�� �����Ͱ� ����Ǵ� �޸� ������ PCH Entry���� �����´�.
 *
 * aPID [IN] �������� ID
 * return Page�� �����Ͱ� ����Ǵ� �޸� ����
 */
/* BUG-32479 [sm-mem-resource] refactoring for handling exceptional case about
 * the SMM_OID_PTR and SMM_PID_PTR macro. */
IDE_RC smmManager::getPersPagePtrErr( scSpaceID    aSpaceID, 
                                      scPageID     aPID,
                                      void      ** aPersPagePtr )
{
    idBool       sIsFreePage ;
    smmTBSNode * sTBSNode = NULL;
    smmPCH     * sPCH;
    svmTBSNode * sVolTBSNode;
    IDE_RC       rc;

    // BUG-14343
    // DB ũ�⸦ ���̱� ���ؼ� free page�� loading ���� �ʾҴ�.
    // ������ recovery phase������ free page ���ٽ� page �޸𸮸� �Ҵ��� �־�� �Ѵ�.
    /*
        Free List Info Page�� Free Page�� �����Ǿ� �־
        Restart Recovery������ Page�� Load���� �ʾ�����,
        Redo���� �ش� Page�� ������ ��� Disk�κ��� Page�� Load�� �ʿ䰡 ����.

        Redo���� Load DB�ÿ� Page�� Free Page���� Load���� ���� ��� :

        1. �ش� Page�� Redo�Ϸ� �Ŀ��� Free Page���
            Redo�Ϸ��� �ش� Page���� ������ ������ ���� ����־ ���������
           �ش� Page�� Header�� ������ ������ �����Ǿ� ������ �ȴ�.

        2. �ش� Page�� Redo�Ϸ� �Ŀ� Alloced Page���
           Checkpoint�� �ش� Page�� alloced page��� ������ ����
           Dirty Page�� Disk�� �������� ���� �����̹Ƿ�
           Free => alloced page�� ����Ǵ� �αװ� redo��� ���Եȴ�.
           Disk�κ��� Page�� �ε����� �ʾƵ� redo�� ����
           Page Image�� ����� ���� �ȴ�.

           �� �� ���� ��Ȳ ��� Page�� Disk�κ��� �ε��� �ʿ䰡 ����.
           ������, Page Memory�� �Ҵ�Ǿ�� �ϸ�, Page Header��
           ������ ������ �����Ͱ� �����Ǿ� �־�� �Ѵ�.

           (ex> Page Header�� mSelf�� �����Ǿ� �־�� �ش� Page�� ����
                Redo�� ���� �������� ������ �� ����
                => Redo��ƾ���� Page Header�� mSelf�� ����ϱ� ���� )
    */

    // BUG-31191 isRestart()->isRestartRecoveryPhase()�� ����
    if ( (smLayerCallback::isRestartRecoveryPhase() == ID_TRUE ) ||
         (smLayerCallback::isMediaRecoveryPhase() == ID_TRUE) )
    {
        IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                            (void**)&sTBSNode ) 
                  != IDE_SUCCESS );
        IDE_ERROR( sTBSNode != NULL );
        IDE_ERROR_MSG( mPCArray[aSpaceID].mPC != NULL,
                       "aSpaceID : %"ID_UINT32_FMT,
                       aSpaceID );
        IDE_ERROR_MSG( mPCArray[aSpaceID].mPC[aPID].mPCH != NULL,
                       "aSpaceID : %"ID_UINT32_FMT"\n"
                       "aPID     : %"ID_UINT32_FMT"\n",
                       aSpaceID,
                       aPID );

        if ( mPCArray[aSpaceID].mPC[aPID].mPagePtr == NULL )
        {
            IDE_TEST( allocAndLinkPageMemory( sTBSNode,
                                              NULL, // �α����� ����
                                              aPID,          // PID
                                              SM_NULL_PID,   // prev PID
                                              SM_NULL_PID )  // next PID
                      != IDE_SUCCESS );
        }
    }

    if( isValidPageID( aSpaceID, aPID ) == ID_FALSE )
    {
        ideLog::log( IDE_SERVER_0,
                     SM_TRC_PAGE_PID_INVALID,
                     aSpaceID,
                     aPID,
                     mPCArray[aSpaceID].mMaxPageCount );
        IDE_ERROR( 0 );
    }

    sPCH = (smmPCH*)mPCArray[aSpaceID].mPC[aPID].mPCH;

    if ( sPCH == NULL )
    {
        /* PCH�� ���� �Ҵ���� ���ߴٸ�, Utility���� ����ϴ� ���� Ȯ��
         * �� ����. ���� Callback�� ������, callback���� ȣ�� �õ��غ�*/
        if( mGetPersPagePtrFunc != NULL )
        {
            IDE_TEST( mGetPersPagePtrFunc ( aSpaceID,
                                            aPID,
                                            aPersPagePtr ) 
                      != IDE_SUCCESS );

            IDE_CONT( RETURN_SUCCESS );
        }
        else
        {
            /*Noting to do ...*/
        }
 
        IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                            (void**)&sTBSNode )
                  != IDE_SUCCESS );
        IDE_ERROR( sTBSNode != NULL );

        ideLog::log(IDE_ERR_0,
                    "aSpaceID : %"ID_UINT32_FMT"\n"
                    "aPID     : %"ID_UINT32_FMT"\n",
                    aSpaceID,
                    aPID );

        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MEMORY_PCH_ARRAY_NULL1,
                    (ULong)aPID);

        ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                    SM_TRC_MEMORY_PCH_ARRAY_NULL2,
                    (ULong)mPCArray[aSpaceID].mMaxPageCount );

        if ( sctTableSpaceMgr::isMemTableSpace( sTBSNode ) == ID_TRUE )
        {
            if ( sTBSNode->mMemBase != NULL )
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                            SM_TRC_MEMORY_PCH_ARRAY_NULL3,
                            (ULong)sTBSNode->mMemBase->mAllocPersPageCount);
            }

            rc = smmExpandChunk::isFreePageID(sTBSNode, aPID, &sIsFreePage);
        }
        else
        {
            sVolTBSNode = (svmTBSNode*)sTBSNode;

            ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                        SM_TRC_MEMORY_PCH_ARRAY_NULL3,
                        (ULong)sVolTBSNode->mMemBase.mAllocPersPageCount);

            rc = svmExpandChunk::isFreePageID(sVolTBSNode, aPID, &sIsFreePage);
        }

        if ( rc == IDE_SUCCESS )
        {
            if (sIsFreePage == ID_TRUE)
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                            SM_TRC_MEMORY_PCH_ARRAY_NULL4,
                            (ULong)aPID);
            }
            else
            {
                ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                            SM_TRC_MEMORY_PCH_ARRAY_NULL5,
                            (ULong)aPID);
            }
        }
        else
        {
            ideLog::log(SM_TRC_LOG_LEVEL_MEMORY,
                        SM_TRC_MEMORY_PCH_ARRAY_NULL6);
        }

        IDE_ERROR( 0 );     // BUG-41347
    }
    
    /* BUGBUG: by newdaily
     * To Trace BUG-15969 */
    (*aPersPagePtr) = mPCArray[aSpaceID].mPC[aPID].mPagePtr;

    IDE_ERROR_MSG( (*aPersPagePtr) != NULL,
                   "aSpaceID : %"ID_UINT32_FMT"\n"
                   "aPID     : %"ID_UINT32_FMT"\n",
                   aSpaceID,
                   aPID );

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // BUG-41149
    // ASSERT�� ���� �ϰ�, trc �α׸� �߰��Ѵ�.
    ideLog::logCallStack( IDE_ERR_0 );

    return IDE_FAILURE;
}

/*
 * �����ͺ��̽� File�� Open�ϰ�, �����ͺ��̽� ���� ��ü�� �����Ѵ�
 *
 * aStableDB [IN] Ping/Pong DB ���� ( 0�̳� 1 )
 * aDBFileNo [IN] �����ͺ��̽� ���� ��ȣ ( 0 ���� ���� )
 * aDBFile   [OUT] �����ͺ��̽� ���� ��ü
 */
IDE_RC smmManager::openAndGetDBFile( smmTBSNode *      aTBSNode,
                                     SInt              aStableDB,
                                     UInt              aDBFileNo,
                                     smmDatabaseFile **aDBFile )
{

    IDE_DASSERT( aStableDB == 0 || aStableDB == 1 );
    IDE_DASSERT( aDBFileNo < aTBSNode->mHighLimitFile );
    IDE_DASSERT( aDBFile != NULL );

    IDE_TEST( getDBFile( aTBSNode,
                         aStableDB,
                         aDBFileNo,
                         SMM_GETDBFILEOP_SEARCH_FILE,
                         aDBFile )
              != IDE_SUCCESS );

    IDE_DASSERT( *aDBFile != NULL );

    if( (*aDBFile)->isOpen() != ID_TRUE )
    {
        IDE_TEST( (*aDBFile)->open() != IDE_SUCCESS );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * �����ͺ��̽� ���� ��ü�� �����Ѵ�.
 * ( �ʿ��ϴٸ� ��� DB ���丮���� DB������ ã�´� )
 *
 * aPingPongDBNum [IN] Ping/Pong DB ���� ( 0�̳� 1 )
 * aDBFileNo      [IN] �����ͺ��̽� ���� ��ȣ
 * aOp            [IN] getDBFile �ɼ�
 * aDBFile        [OUT] �����ͺ��̽� ���� ��ü
 */
IDE_RC smmManager::getDBFile( smmTBSNode *       aTBSNode,
                              UInt               aPingPongDBNum,
                              UInt               aDBFileNo,
                              smmGetDBFileOption aOp,
                              smmDatabaseFile ** aDBFile )
{

    SChar   sDBFileName[SM_MAX_FILE_NAME];
    SChar  *sDBFileDir;
    idBool  sFound;

    //BUG-27610	CodeSonar::Type Underrun, Overrun (2)
    IDE_ASSERT(  aPingPongDBNum <  SM_DB_DIR_MAX_COUNT );
    IDE_DASSERT( aDBFile != NULL );

    // ��� MEM_DB_DIR���� �����ͺ��̽� ������ ã�ƾ� �ϴ��� ����
    if( aOp == SMM_GETDBFILEOP_SEARCH_FILE )
    {
        sFound =  smmDatabaseFile::findDBFile( aTBSNode,
                                               aPingPongDBNum,
                                               aDBFileNo,
                                               (SChar*)sDBFileName,
                                               &sDBFileDir);

        IDE_TEST_RAISE( sFound != ID_TRUE,
                        file_exist_error );

        ((smmDatabaseFile*)aTBSNode->mDBFile[aPingPongDBNum][aDBFileNo])->setFileName(sDBFileName);
        ((smmDatabaseFile*)aTBSNode->mDBFile[aPingPongDBNum][aDBFileNo])->setDir(sDBFileDir);
    }

    /* BUG-32214 [sm] when server start to check the db file size.
     * cause of buffer overflow 
     * Table�� File ������ �ִ�ġ�� �Ѿ��. MEM_MAX_DB_SIZE�� �۱� ����. */
    IDE_TEST_RAISE( aDBFileNo >= aTBSNode->mHighLimitFile ,
                    error_invalid_mem_max_db_size );

    *aDBFile = (smmDatabaseFile*)aTBSNode->mDBFile[aPingPongDBNum][aDBFileNo];

    return IDE_SUCCESS;

    IDE_EXCEPTION(file_exist_error);
    {
        SChar sErrorFile[SM_MAX_FILE_NAME];

        idlOS::snprintf( (SChar*)sErrorFile, SM_MAX_FILE_NAME, "%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                         aTBSNode->mHeader.mName,
                         aPingPongDBNum,
                         aDBFileNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_NoExistFile,
                                  (SChar*)sErrorFile ) );
    }
    IDE_EXCEPTION( error_invalid_mem_max_db_size );
    {
        SChar sErrorFile[SM_MAX_FILE_NAME];

        idlOS::snprintf( (SChar*)sErrorFile, SM_MAX_FILE_NAME, "%s-%"ID_UINT32_FMT"-%"ID_UINT32_FMT"",
                         aTBSNode->mHeader.mName,
                         aPingPongDBNum,
                         aDBFileNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_DB_FILE_SIZE_EXCEEDS_LIMIT,
                                  (SChar*)sErrorFile ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smmManager::openOrCreateDBFileinRecovery( smmTBSNode * aTBSNode,
                                                 SInt         aDBFileNo,
                                                 idBool     * aIsCreated )
{
    SInt             sCurrentDB;
    smmDatabaseFile *sDBFile;
    IDE_RC           sSuccess;

    IDE_ASSERT( aIsCreated != NULL );

    for ( sCurrentDB=0 ; sCurrentDB<SMM_PINGPONG_COUNT; sCurrentDB++ )
    {
        sSuccess = getDBFile( aTBSNode,
                              sCurrentDB,
                              aDBFileNo,
                              SMM_GETDBFILEOP_SEARCH_FILE,
                              &sDBFile );

        if( sSuccess != IDE_SUCCESS )
        {
            IDE_TEST(((smmDatabaseFile*)aTBSNode->mDBFile[sCurrentDB][aDBFileNo])->createDbFile(
                         aTBSNode,
                         sCurrentDB,
                         aDBFileNo,
                         0/* DB File Header�� ���*/)
                     != IDE_SUCCESS);

            // fix BUG-17513
            // restart recovery�Ϸ����� loganchor resorting��������
            // ���� ������ Memory DBF�� ���� ������ ��������ʴ� ��� �߻�

            // create tablespace �� ���� ������ �����Ǵ� ��쿡��
            // mLstCreatedDBFile�� ������ش�.
            if ( (UInt)aDBFileNo > aTBSNode->mLstCreatedDBFile )
            {
                aTBSNode->mLstCreatedDBFile = (UInt)aDBFileNo;
            }
            *aIsCreated = ID_TRUE;
        }
        else
        {
            if (sDBFile->isOpen() != ID_TRUE)
            {
                IDE_TEST(sDBFile->open() != IDE_SUCCESS);
            }
            *aIsCreated = ID_FALSE;
        }
    } // for

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Disk�� �����ϴ� �����ͺ��̽� Page�� ���� ����Ѵ�
 * Performance View�� ��谪���� Reporting�� �뵵�� ���ǹǷ�
 * ��Ȯ�� �ʿ�� ����.
 *
 * aCurrentDB [IN] ���� ������� Ping/Pong�����ͺ��̽� �� �ϳ� ( 0 or 1 )
 */
IDE_RC smmManager::calculatePageCountInDisk( smmTBSNode * aTBSNode )
{

    UInt             s_DbFileCount;

    s_DbFileCount = getDbFileCount(aTBSNode,
                                   aTBSNode->mTBSAttr.mMemAttr.mCurrentDB);

    aTBSNode->mDBPageCountInDisk =
        s_DbFileCount * aTBSNode->mMemBase->mDBFilePageCount +
        SMM_DATABASE_META_PAGE_CNT;

    return IDE_SUCCESS;

}

/*
   �޸� ����Ÿ���� ���� Runtime ���� ����ü �ʱ�ȭ
*/
void smmManager::initCrtDBFileInfo( smmTBSNode * aTBSNode )
{
    UInt i;
    UInt j;

    IDE_DASSERT( aTBSNode != NULL );

    for ( i = 0; i < aTBSNode->mHighLimitFile; i ++ )
    {
        for (j = 0; j < SMM_PINGPONG_COUNT; j++)
        {
            (aTBSNode->mCrtDBFileInfo[ i ]).mCreateDBFileOnDisk[ j ]
                = ID_FALSE;
        }

        (aTBSNode->mCrtDBFileInfo[ i ]).mAnchorOffset
            = SCT_UNSAVED_ATTRIBUTE_OFFSET;
    }

    return;
}

/*
   �־��� ���Ϲ�ȣ�� �ش��ϴ� DBF�� �ϳ��� ������ �Ǿ����� ���ι�ȯ

   [ ���ù��� ]
   fix BUG-17343
   loganchor�� Stable/Unstable Chkpt Image�� ���� ���� ������ ����
*/
idBool smmManager::isCreateDBFileAtLeastOne(
                     idBool    * aCreateDBFileOnDisk )
{
    UInt    i;
    idBool  sIsCreate;

    IDE_DASSERT( aCreateDBFileOnDisk != NULL );

    sIsCreate = ID_FALSE;

    for ( i = 0 ; i < SMM_PINGPONG_COUNT; i++ )
    {
       if ( aCreateDBFileOnDisk[ i ] == ID_TRUE )
       {
           sIsCreate = ID_TRUE;
           break;
       }
    }

    return sIsCreate;
}


IDE_RC smmManager::initSCN()
{
    smmTBSNode * sTBSNode;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                  SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                  (void**)&sTBSNode ) != IDE_SUCCESS );
    IDE_ASSERT(sTBSNode != NULL);

    SM_ADD_SCN( &(sTBSNode->mMemBase->mSystemSCN),
                smuProperty::getSCNSyncInterval() );

    IDE_TEST( smLayerCallback::setSystemSCN( sTBSNode->mMemBase->mSystemSCN )
              != IDE_SUCCESS );

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                             SMM_MEMBASE_PAGEID ) 
              != IDE_SUCCESS );

    SM_SET_SCN(&smmDatabase::mLstSystemSCN, &sTBSNode->mMemBase->mSystemSCN);

    smmDatabase::makeMembaseBackup();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smmManager::loadParallel(smmTBSNode     * aTBSNode)
{
    SInt            sThrCnt;
    UInt            sState      = 0;
    smmPLoadMgr   * sLoadMgr    = NULL;

    /* TC/FIT/Limit/sm/smm/smmManager_loadParallel_malloc.sql */
    IDU_FIT_POINT_RAISE( "smmManager::loadParallel::malloc",
                          insufficient_memory );
	
    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMM,
                               ID_SIZEOF(smmPLoadMgr),
                               (void**)&(sLoadMgr)) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 1;

    sLoadMgr = new (sLoadMgr) smmPLoadMgr();

    sThrCnt = smuProperty::getRestoreThreadCount();

    IDE_TEST(sLoadMgr->initializePloadMgr(aTBSNode,
                                          sThrCnt)
             != IDE_SUCCESS);

    IDE_TEST(sLoadMgr->start() != IDE_SUCCESS);
    IDE_TEST(sLoadMgr->waitToStart() != IDE_SUCCESS);

    IDE_TEST(sLoadMgr->join() != IDE_SUCCESS);

    IDE_TEST(sLoadMgr->destroy() != IDE_SUCCESS);

    /* BUG-40933 thread �Ѱ��Ȳ���� FATAL���� ���� �ʵ��� ����
     * thread�� �ϳ��� �������� ���Ͽ� ABORT�� ��쿡
     * smmPLoadMgr thread join �� �� ����� Ȯ���Ͽ�
     * ABORT������ �� �� �ֵ��� �Ѵ�. */
    IDE_TEST(sLoadMgr->getResult() == ID_FALSE);

    IDE_TEST(iduMemMgr::free(sLoadMgr) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sLoadMgr ) == IDE_SUCCESS );
            sLoadMgr = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}


// Base Page ( 0�� Page ) �� Latch�� �Ǵ�
// 0�� Page�� �����ϴ� Transaction���� ������ �����Ѵ�.
IDE_RC smmManager::lockBasePage(smmTBSNode * aTBSNode)
{
    IDE_TEST( smmFPLManager::lockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );

    IDE_TEST( smmFPLManager::lockAllFPLs(aTBSNode) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL("Fatal Error at smmManager::lockBasePage");

    return IDE_FAILURE;
}

// Base Page ( 0�� Page ) ���� Latch�� Ǭ��.
// lockBasePage�� ���� Latch�� ��� �����Ѵ�
IDE_RC smmManager::unlockBasePage(smmTBSNode * aTBSNode)
{
    IDE_TEST( smmFPLManager::unlockAllFPLs(aTBSNode) != IDE_SUCCESS);
    IDE_TEST( smmFPLManager::unlockAllocChunkMutex(aTBSNode) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL("Fatal Error at smmManager::unlockBasePage");

    return IDE_FAILURE;
}




/* Ư�� ������ ������ŭ �����ͺ��̽��� Ȯ���Ѵ�.
 *
 * aTrans �� NULL�� ������, �� ���� Restart Redo���� �Ҹ��� ������
 * Normal Processing���� ������ �� ������ ���� ������ �ٸ��� �۵��Ѵ�.
 *    1. Logging �� �ʿ䰡 ����
 *    2. �Ҵ��� Page Count�� ������ �˻��� �ʿ䰡 ����
 *       by gamestar 2001/05/24
 *
 * ��� Free Page List�� ���� Latch�� ������ ���� ä�� �� �Լ��� ȣ��ȴ�.
 *
 * aTrans            [IN] �����ͺ��̽��� Ȯ���Ϸ��� Ʈ�����
 *                        CreateDB Ȥ�� logical redo���ΰ�� NULL�� ���´�.
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ù��° Page ID
 * aNewChunkFirstPID [IN] Ȯ���� �����ͺ��̽� Expand Chunk�� ������ Page ID
 */
IDE_RC smmManager::allocNewExpandChunk4MR( smmTBSNode * aTBSNode,
                                           scPageID     aNewChunkFirstPID,
                                           scPageID     aNewChunkLastPID,
                                           idBool       aSetFreeListOfMembase,
                                           idBool       aSetNextFreePageOfFPL )
{
    UInt              sStage;
    UInt              sNewDBFileCount;
    UInt              sArrFreeListCount;
    smmPageList       sArrFreeList[ SMM_MAX_FPL_COUNT ];

    IDE_DASSERT( aTBSNode   != NULL );

    sStage = 0;

    // BUGBUG kmkim ����ó���� ����. ����ڰ� ������Ƽ �߸� �ٲپ�����
    // �ٷ� �˷��� �� �ֵ���.
    // ������ ����ڰ� PAGE�� ������ ������ Restart Recovery�ϸ�
    // ���⼭ �װԵȴ�.
    IDE_ASSERT( aNewChunkFirstPID < aTBSNode->mDBMaxPageCount );
    IDE_ASSERT( aNewChunkLastPID < aTBSNode->mDBMaxPageCount );

    // ��� Free Page List�� Latchȹ��
    IDE_TEST( smmFPLManager::lockAllFPLs( aTBSNode ) != IDE_SUCCESS );
    sStage = 1;

    if ( aSetNextFreePageOfFPL == ID_TRUE )
    {
        // �ϳ��� Expand Chunk�� ���ϴ� Page���� PCH Entry���� �����Ѵ�.
        IDE_TEST( fillPCHEntry4AllocChunk( aTBSNode,
                                           aNewChunkFirstPID,
                                           aNewChunkLastPID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Chunk ������ �ʿ���� ��� PCHEntry�� page�� �Ҵ���
        // �ʿ䰡 ����.
    }

    IDE_ASSERT( aTBSNode->mMemBase != NULL );

    sArrFreeListCount = aTBSNode->mMemBase->mFreePageListCount;

    // Logical Redo �� ���̹Ƿ� Physical Update( Next Free Page ID ����)
    // �� ���� �α��� ���� ����.
    IDE_TEST( smmFPLManager::distributeFreePages(
                    aTBSNode,
                    aNewChunkFirstPID +
                    smmExpandChunk::getChunkFLIPageCnt(aTBSNode),
                    aNewChunkLastPID,
                    aSetNextFreePageOfFPL,
                    sArrFreeListCount,
                    sArrFreeList  )
                != IDE_SUCCESS );
    sStage = 2;

    // ����! smmUpdate::redo_SMMMEMBASE_ALLOC_EXPANDCHUNK ����
    // membase ������� Logical Redo�ϱ� ���� ������ �����س��� �� ��ƾ����
    // ���;� �Ѵ�.

    // ���ݱ��� �����ͺ��̽��� �Ҵ�� �� �������� ����
    aTBSNode->mMemBase->mAllocPersPageCount = aNewChunkLastPID + 1;
    aTBSNode->mMemBase->mCurrentExpandChunkCnt ++ ;

    // Chunk�� ���� �Ҵ�� �� DB File�� ��� �� ���ܾ� �ϴ��� ���
    IDE_TEST( calcNewDBFileCount( aTBSNode,
                                  aNewChunkFirstPID,
                                  aNewChunkLastPID,
                                  & sNewDBFileCount )
            != IDE_SUCCESS );

    // DB File �� ����
    aTBSNode->mMemBase->mDBFileCount[0]    += sNewDBFileCount;
    aTBSNode->mMemBase->mDBFileCount[1]    += sNewDBFileCount;


    // Logical Redo�� ���̹Ƿ� Phyical Update�� ���� �α����� �ʴ´�.
    IDE_TEST( smmFPLManager::appendPageLists2FPLs(
                                      aTBSNode,
                                      sArrFreeList,
                                      aSetFreeListOfMembase,
                                      aSetNextFreePageOfFPL )
              != IDE_SUCCESS );

    if ( aSetFreeListOfMembase == ID_TRUE )
    {
        sStage = 1;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage( aTBSNode->mTBSAttr.mID,
                                                 SMM_MEMBASE_PAGEID)
                  != IDE_SUCCESS);
    }
    else
    {
        // Membase ������ �ʿ���� ���
    }

    sStage = 0;
    IDE_TEST( smmFPLManager::unlockAllFPLs(aTBSNode) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sStage )
    {
        case 2 :
            if ( aSetFreeListOfMembase == ID_TRUE )
            {
                IDE_ASSERT( smmDirtyPageMgr::insDirtyPage( aTBSNode->mTBSAttr.mID,
                                                           SMM_MEMBASE_PAGEID)
                            == IDE_SUCCESS);
            }

        case 1 :
            IDE_ASSERT( smmFPLManager::unlockAllFPLs(aTBSNode) == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}


IDE_RC smmManager::setSystemStatToMemBase( smiSystemStat * aSystemStat )
{
    smmMemBase  * sMemBase;

    sMemBase = smmDatabase::getDicMemBase();

    IDE_DASSERT( sMemBase != NULL );

    sMemBase->mSystemStat.mCreateTV            =   aSystemStat->mCreateTV;
    sMemBase->mSystemStat.mSReadTime           =   aSystemStat->mSReadTime;
    sMemBase->mSystemStat.mMReadTime           =   aSystemStat->mMReadTime;
    sMemBase->mSystemStat.mDBFileMultiPageReadCount
                                    = aSystemStat->mDBFileMultiPageReadCount;
    sMemBase->mSystemStat.mHashTime            =   aSystemStat->mHashTime;
    sMemBase->mSystemStat.mCompareTime         =   aSystemStat->mCompareTime;
    sMemBase->mSystemStat.mStoreTime           =   aSystemStat->mStoreTime;

    IDE_TEST( smmDirtyPageMgr::insDirtyPage( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC, 
                                             SMM_MEMBASE_PAGEID )
                              != IDE_SUCCESS);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smmManager::getSystemStatFromMemBase( smiSystemStat * aSystemStat )
{
    smmMemBase  * sMemBase;

    sMemBase = smmDatabase::getDicMemBase();

    IDE_DASSERT( sMemBase != NULL );

    aSystemStat->mCreateTV      =   sMemBase->mSystemStat.mCreateTV;
    aSystemStat->mSReadTime     =   sMemBase->mSystemStat.mSReadTime;
    aSystemStat->mMReadTime     =   sMemBase->mSystemStat.mMReadTime;
    aSystemStat->mDBFileMultiPageReadCount  =
                    sMemBase->mSystemStat.mDBFileMultiPageReadCount;
    aSystemStat->mHashTime      =   sMemBase->mSystemStat.mHashTime;
    aSystemStat->mCompareTime   =   sMemBase->mSystemStat.mCompareTime;
    aSystemStat->mStoreTime     =   sMemBase->mSystemStat.mStoreTime;

    return IDE_SUCCESS;
}
