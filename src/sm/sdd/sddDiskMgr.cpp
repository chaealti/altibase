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
 * $Id: sddDiskMgr.cpp 90899 2021-05-27 08:55:20Z jiwon.kim $
 *
 * Description :
 *
 * �� ������ ��ũ�����ڿ� ���� ���������̴�.
 *
 * ��ũ�����ڴ� ���۰����ڿ� �Բ� resource layer�� ���Ѵ�. ��κ�
 * ���۰����ڰ� ��ũ�����ڸ� ȣ���ϸ�, I/O �Ϸ�ÿ��� ���� �����ڿ���
 * �Ϸ� ����� �˷��� �ʿ䰡 �ִ�.
 *
 * # ����
 *
 * ���̺����̽� ����, �� ���̺����̽��� ����Ÿ ȭ�Ͽ� ���� ����,
 * ����Ÿ ȭ���� I/O�� ���õ� ������ �����Ѵ�.
 *
 * # �������
 *
 * 1. sddDiskMgr
 *   tablespace�� hash�� ���� ����Ʈ�� �����ϸ�, ��ü I/O�� ����
 *   ������ �����Ѵ�.
 *
 * 2. sddTableSpaceNode
 *   tablespace ��忡 ���õ� ������ �����Ѵ�.
 *
 * 3. sddDataFileNode
 *   �ϳ��� datafile ��忡 ���� ������ �����Ѵ�.
 *
 * - sddDiskMgr
 *                 ____________
 *          _______|Hash Table|
 *          |      |__________|
 *  ________|_____ ___________________  ___________________
 *  |sddDiskMgr  |_|sddTableSpaceNode|--|sddTableSpaceNode|
 *  |____________| |_________________|  |_________________|
 *          |      _________________  _________________
 *          |______|sddDataFileNode|__|sddDataFileNode|
 *        LRU-list |_______________|  |_______________|
 *
 * - sddTableSpaceNode
 *  ___________________ __________________  _________________
 *  |sddTableSpaceNode|_|sddDataFileNode |__|sddDataFileNode|
 *  |_________________| |________________|  |_______________|
 *
 *  - sddDataFileNode LRU-list
 * �ִ� �� �� �ִ� datafile�� ������ ������ �����Ƿ�, ���µ� datafile��
 * ����Ʈ�� LRU�� �����Ѵ�. ����Ʈ�� �������� ���� �������� ���µ�
 * datafile�̴�.
 *
 *  - sddTableSpaceNode List
 * ���� ����Ÿ���̽����� ������� ��� ���̺����̽��� ���� ����Ʈ�̴�.
 * �ý��� ���̺����̽�, ���� ���̺����̽��� �����Ѵ�.
 *
 *  - sddTableSpaceNode Hash
 * ��� tablespace�� �����ϱ� ���� �ؽ�����
 *
 * # ��ũ�������� Mutex
 *
 * ��ũ�������� mutex�� ������ list�� hash�� ���� ������ ��ȣ�Ѵ�.
 * tablespace�� mutex�� ������ �ʴ´�.
 *
 * # ���� ���� ����
 * sddTableSpaceNode�� sddDataFileNode�� mutex�� �ξ� sddDiskMgr��
 * contention�� ���δ�.(??)
 *
 * # ���� property
 *
 * - SMU_MAX_OPEN_FILE_COUNT  : �ִ� ������ �� �ִ� file ����
 * - SMU_OPEN_DATAFILE_WAIT_INTERVAL : file�� �����ϱ� ���� ����ϴ� �ð�
 *
 **********************************************************************/

#include <smu.h>
#include <smErrorCode.h>
#include <smiMain.h>
#include <sdd.h>
#include <sddReq.h>
#include <sctTableSpaceMgr.h>
#include <smriChangeTrackingMgr.h>
#include <smriBackupInfoMgr.h>

iduMutex       sddDiskMgr::mMutexVFL;         // VictimFileList�� Mutex
iduMutex       sddDiskMgr::mMutexOPC;         // OpenFileCount �� Mutex
iduMutex       sddDiskMgr::mGlobalPageCountCheckMutex;
smuList        sddDiskMgr::mVictimFileList;   // Close ��� datafile ����� ����Ʈ
UInt           sddDiskMgr::mOpenFileCount;    // ���µ� datafile ��� ����
UInt           sddDiskMgr::mVictimListNodeCount;    // ���µ� datafile ��� ����
UInt           sddDiskMgr::mMaxDataFilePageCount;  // ��� datafile�� maxsize
UInt           sddDiskMgr::mWaitThr4Open;     // datafile ���´�� thread ����

// ���� page write�ÿ� ����Ǵ� checksum
UInt           sddDiskMgr::mInitialCheckSum;

idBool         sddDiskMgr::mEnableWriteToOfflineDBF;
idBool         sddDiskMgr::mIsDirectIO;

/***********************************************************************
 * Description : ��ũ������ �ʱ�ȭ
 *
 * �ý��� startup�ÿ� ȣ��Ǿ�, ��ũ�����ڸ� �ʱ�ȭ�Ѵ�.
 *
 * + 2nd. code design
 *   - ��ũ�������� mutex ���� �� �ʱ�ȭ
 *   - tablespace ��带 ���� HASH�� ����
 *   - tablespace ����Ʈ �� tablespace ��� ���� �ʱ�ȭ�Ѵ�.
 *   - datafile LRU ����Ʈ �� LRU datafile ��� ���� �ʱ�ȭ�Ѵ�.
 *   - max open datafile�� �����Ѵ�.
 *   - max system datafile size�� �����Ѵ�.
 *   - next tablespace ID�� �ʱ�ȭ�Ѵ�.
 *   - open datafile CV�� �ʱ�ȭ�Ѵ�.
 *   - open datafile�� ���� ����÷��׸� ID_FALSE�� �Ѵ�.
 **********************************************************************/
IDE_RC sddDiskMgr::initialize( UInt  aMaxFilePageCnt )
{
    UInt  i;

    IDE_DASSERT( aMaxFilePageCnt > 0 );

    mMaxDataFilePageCount = aMaxFilePageCnt;

    /* ���µ� datafile ����� BASE ����Ʈ �� ���� */
    SMU_LIST_INIT_BASE(&mVictimFileList);
    mOpenFileCount       = 0;
    mVictimListNodeCount = 0;
    //PRJ-1149.

    mWaitThr4Open = 0;

    for(i = 0, mInitialCheckSum = 0; i < SD_PAGE_SIZE - ID_SIZEOF(UInt); i++)
    {
        mInitialCheckSum = smuUtility::foldUIntPair(mInitialCheckSum, 0);
    }

    // BUG-17158
    mEnableWriteToOfflineDBF = ID_FALSE;

    /* BUG-47398 AIX ���� iduMemMgr::free4malign() �Լ����� ������ ����
     * Direct IO ���� ���� */
    mIsDirectIO = ( smuProperty::getIOType() == 0 ) ? ID_FALSE : ID_TRUE;

    IDE_TEST( mMutexVFL.initialize( (SChar*)"CLOSE_VICTIM_FILE_LIST_MUTEX",
                                    IDU_MUTEX_KIND_NATIVE,
                                    IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( mMutexOPC.initialize( (SChar*)"OPEN_FILE_COUNT_MUTEX",
                                    IDU_MUTEX_KIND_NATIVE,
                                    IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( mGlobalPageCountCheckMutex.initialize(
                  (SChar*)"SDD_GLOBAL_PAGE_COUNT_CHECK_MUTEX",
                  IDU_MUTEX_KIND_POSIX,
                  IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Space Cache ���� */
void  sddDiskMgr::setSpaceCache( scSpaceID  aSpaceID,
                                 void     * aSpaceCache )
{
    sddTableSpaceNode*  sSpaceNode;

    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceID );

    setSpaceCache( sSpaceNode,
                   aSpaceCache );
    return;
}

/* Space Cache ���� */
void  sddDiskMgr::setSpaceCache( sddTableSpaceNode * aSpaceNode,
                                 void              * aSpaceCache )
{
    aSpaceNode->mSpaceCache = aSpaceCache;

    return;
}

/* Space Cache ��ȯ */
sdptbSpaceCache * sddDiskMgr::getSpaceCache( scSpaceID  aSpaceID )
{
    sddTableSpaceNode*  sSpaceNode;

    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceID );

    IDE_ASSERT( sSpaceNode != NULL );
    IDE_ASSERT( sSpaceNode->mHeader.mID == aSpaceID );

    return getSpaceCache( sSpaceNode );
}


/***********************************************************************
 * Description : ��ũ������ ����
 *
 * �ý��� shutdown�ÿ� ȣ��Ǿ�, ��ũ�����ڸ� �����Ѵ�.
 *
 * + 2nd. code design
 *   - ��ũ�������� mutex ȹ��
 *   - while( ���µ� datafile ��� LRU ����Ʈ�� ��� datafile ��忡 ���� )
 *     {
 *         datafile�� close �Ѵ�. -> closeDataFile
 *     }
 *   - while( ��� tablespace�� ���Ͽ� )
 *     {
 *       �� tablespace�� destroy�Ѵ� -> sddTableSpace::destroy(��常)
 *       ���̺� �����̽� �޸𸮸� �����Ѵ�.
 *     }
 *   - ��ũ������ mutex ����
 *   - open datafile CV�� destroy�Ѵ�.
 *   - ��ũ�������� mutex�� destroy�Ѵ�.
 *   - tablespace ��带 ���� hash�� destroy�Ѵ�. 
 **********************************************************************/
IDE_RC sddDiskMgr::destroy()
{
    UInt                i,j;
    smuList           * sNode;
    smuList           * sBaseNode;
    sddDataFileNode   * sFileNode;
    sddTableSpaceNode * sSpaceNode;
    // �� ������ ��� file node�� ��������� �ʴٸ� ��� node list �ȿ� �ִ�.
    // ���ü��� Ȯ���� �ʿ�� ����δ�.
    /* ------------------------------------------------
     * LRU ����Ʈ�� ��ȸ�ϸ鼭 ���µ� datafile���� ��� �ݴ´�.
     * ----------------------------------------------*/
    sBaseNode = &mVictimFileList;

    for(sNode = SMU_LIST_GET_FIRST(sBaseNode);
        sNode != sBaseNode;
        sNode = SMU_LIST_GET_FIRST(sBaseNode) )
    {
        sFileNode = (sddDataFileNode*)sNode->mData;

        if ( sFileNode->mIsOpened == ID_TRUE )
        {
            IDE_TEST( closeDataFile( NULL, sFileNode ) != IDE_SUCCESS );
        }
        else
        {
            removeNodeList( NULL,
                            sFileNode );
        }
    }

    // 0�� �Ǿ�� ������ Ȥ�� �ִٸ� ��ü ��ȸ ���� �õ��Ѵ�.
    if ( mOpenFileCount > 0 )
    {
        for( i = 0 ; i < sctTableSpaceMgr::getNewTableSpaceID() ; i++ )
        {
            sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getSpaceNodeBySpaceID( i );

            if( sSpaceNode == NULL )
            {
                continue;
            }

            for ( j = 0; j < sSpaceNode->mNewFileID ; j++ )
            {
                sFileNode = sSpaceNode->mFileNodeArr[j] ;

                if( sFileNode == NULL )
                {
                    continue;
                }
                if ( sFileNode->mIsOpened == ID_TRUE )
                {
                    IDE_TEST( closeDataFile( NULL, sFileNode ) != IDE_SUCCESS );
                }
            }
        }
        IDE_DASSERT( mOpenFileCount == 0 );
    }
    IDE_TEST( mMutexOPC.destroy() != IDE_SUCCESS );
    IDE_TEST( mMutexVFL.destroy() != IDE_SUCCESS );
    IDE_TEST( mGlobalPageCountCheckMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * TBS�� �ڷᱸ���� �����Ͽ� ������ ���ϸ��� �����ϴ����� �˾Ƴ���.
 * BUG-18044�� fix�ϱ����Ͽ� �ۼ��Ǿ�����.
 *
 * [IN]aSpaceNode         : Space Node
 * [IN]aDataFileAttr      : TBS�� �߰��� ���ϵ鿡 ���� ������ �����ϰ�����.
 * [IN]aDataFileAttrCount : aDataFileAttr�� ��Ұ���
 * [OUT]aExistFileName    : TBS�� ������ ���ϸ��� �����Ѵٸ� �� ���ϸ� ������.
 *                          (�����޽��� ��¿����� ����)
 * [OUT]aNameExist        : ������ ���ϸ��� �����ϴ°�?
 *
 */
IDE_RC sddDiskMgr::validateDataFileName(
                                     sddTableSpaceNode  * aSpaceNode,
                                     smiDataFileAttr   ** aDataFileAttr,
                                     UInt                 aDataFileAttrCount,
                                     SChar             ** aExistFileName,
                                     idBool            *  aNameExist )
{
    UInt                i,k;
    sddDataFileNode *   sFileNode=NULL;
    SChar               sNewFileName[SMI_MAX_DATAFILE_NAME_LEN];
    UInt                sNameLength;

    IDE_ASSERT( aSpaceNode          != NULL );
    IDE_ASSERT( aDataFileAttr       != NULL );
    IDE_ASSERT( aDataFileAttrCount  > 0 );
    IDE_ASSERT( aExistFileName      != NULL );
    IDE_ASSERT( aNameExist          != NULL );

    *aNameExist = ID_FALSE;

    for( i=0; i < aDataFileAttrCount; i++)
    {
        idlOS::strcpy( sNewFileName, (*aDataFileAttr[i]).mName );

        sNameLength = idlOS::strlen(sNewFileName);

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                            ID_TRUE,
                                            sNewFileName,
                                            &sNameLength,
                                            SMI_TBS_DISK) != IDE_SUCCESS );

        for (k=0; k < aSpaceNode->mNewFileID ; k++ )
        {
            sFileNode = aSpaceNode->mFileNodeArr[k] ;

            if( sFileNode == NULL )
            {
                continue;
            }

            if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                continue;
            }

            /*
             * ������ ���ϸ��� TBS�� �����ϴ��� �˻�
             */
            if (idlOS::strcmp(sFileNode->mName, sNewFileName) == 0)
            {
                *aNameExist = ID_TRUE;
                *aExistFileName = sFileNode->mName;
                IDE_CONT( return_anyway );
            }
        }
    }

    IDE_EXCEPTION_CONT( return_anyway );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : tablespace ���� �� �ʱ�ȭ (�������)
 *
 * ��ũ�����ڿ� ���ο� tabespace ��带 ����� �ʱ�ȭ �Ѵ�.
 * �ش� ��忡 datafile ��� �� ���� datafile�� �����Ѵ�.
 *
 * CREATE TABLESPACE ������ ����� �� sdpTableSpace::create����
 * ȣ��Ǿ� meta (0/1/2��) page �ʱ�ȭ ���� ������ ���̴�.
 *
 * [!!] ��ũ������ mutex �����ϱ� ���� �α׾�Ŀ�� FLUSH �Ѵ�.
 * 
 * tablespace ID�� ����Ǿ����� ���θ� �˱� ���ؼ���
 * create ������ system SCN�� tablespace�� ������ �ʿ䰡 �ִ�.
 *
 * + 2nd. code design
 *   - tablespace ��带 ���� �޸𸮸� �Ҵ��Ѵ�.
 *   - tablespace ��带 �ʱ�ȭ�Ѵ�. -> sddTableSpace::initialize
 *   - ��ũ������ mutex ȹ��
 *   - tablespace ID�� �Ҵ��ϰ�, �Ӽ��� �����Ѵ�.
 *   - tablespace ��忡 datafile�� ��带 �����, ���� datafile��
 *     create�Ѵ�. -> sddTableSpace::createDataFiles
 *   - HASH�� �߰��Ѵ�.
 *   - tablespace ��带 tablespace ����Ʈ�� �߰��Ѵ�.
 *   - �α׾�Ŀ FLUSH -> smrLogMgr::updateLogAnchorForTableSpace
 *   - ��ũ������ mutex ����
 **********************************************************************/
IDE_RC sddDiskMgr::createTableSpace( idvSQL             * aStatistics,
                                     void               * aTrans,
                                     smiTableSpaceAttr  * aTableSpaceAttr,
                                     smiDataFileAttr   ** aDataFileAttr,
                                     UInt                 aDataFileAttrCount,
                                     smiTouchMode         aTouchMode )
{
    UInt                sChkptBlockState    = 0;
    UInt                sStateTbs;
    UInt                sAllocState;
    sctPendingOp      * sPendingOp;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    UInt                i;
    idBool              sFileExist;
    UInt                sNameLength;
    SChar               sValidName[ SM_MAX_FILE_NAME ];

    IDE_DASSERT( aTrans             != NULL );
    IDE_DASSERT( aTableSpaceAttr    != NULL );
    IDE_DASSERT( aTableSpaceAttr->mAttrType == SMI_TBS_ATTR );
    IDE_DASSERT( aDataFileAttr      != NULL );
    IDE_DASSERT( aDataFileAttrCount != 0 );
    IDE_DASSERT( (aTouchMode == SMI_ALL_TOUCH ) ||
                 (aTouchMode == SMI_EACH_BYMODE) );

    sStateTbs         = 0;
    sAllocState       = 0;
    sSpaceNode        = NULL;

    /* tbs�� dbf �������� mutex ȹ�� */
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sStateTbs = 1;

    for (i = 0; i < aDataFileAttrCount ; i++ )
    {
        IDE_TEST( existDataFile( aDataFileAttr[i]->mName, &sFileExist ) != IDE_SUCCESS );

        if( sFileExist == ID_TRUE )
        {
            sNameLength = 0;

            idlOS::strncpy( (SChar*)sValidName, 
                            aDataFileAttr[i]->mName, 
                            SM_MAX_FILE_NAME );
            sNameLength = idlOS::strlen(sValidName);

            IDE_TEST( sctTableSpaceMgr::makeValidABSPath( ID_TRUE,
                                                          (SChar *)sValidName,
                                                          &sNameLength,
                                                          SMI_TBS_DISK )
                      != IDE_SUCCESS );

            IDE_RAISE( error_already_used_datafile );
        }
        else 
        {
            /* file not exist : nothing to do */
        }
    }

    /* sddDiskMgr_createTableSpace_malloc_SpaceNode.tc */
    IDU_FIT_POINT("sddDiskMgr::createTableSpace::malloc::SpaceNode");
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SDD,
                                ID_SIZEOF(sddTableSpaceNode),
                                (void **)&sSpaceNode,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sAllocState = 1;
    idlOS::memset(sSpaceNode, 0x00, ID_SIZEOF(sddTableSpaceNode));

    //PRJ-1149.
    aTableSpaceAttr->mDiskAttr.mNewFileID = 0;

    IDE_TEST( sctTableSpaceMgr::allocNewTableSpaceID(&aTableSpaceAttr->mID)
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::initialize(sSpaceNode, aTableSpaceAttr)
              != IDE_SUCCESS );
    sAllocState = 2;

    // FOR BUG-9640
    // SMR_DLT_FILEOPER : SCT_UPDATE_DRDB_CREATE_TBS
    // after image : tablespace attribute
    IDE_TEST( smLayerCallback::writeDiskTBSCreateDrop(
                                                  aStatistics,
                                                  aTrans,
                                                  SCT_UPDATE_DRDB_CREATE_TBS,
                                                  sSpaceNode->mHeader.mID,
                                                  aTableSpaceAttr,
                                                  NULL ) != IDE_SUCCESS );

    IDU_FIT_POINT_RAISE( "2.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );

    // PROJ-2133 incremental backup
    // DataFileDescSlot�� �Ҵ�Ǿ� checkpoint�� �߻��ϸ� CTBody�� flush�Ǿ�
    // ���Ͽ� �ݿ��ȴ�. ������ loganchor�� DataFileDescSlotID�� ����Ǳ� ����
    // ������ �״´ٸ� �Ҵ�� DataFileDescSlot�� ����Ҽ� ���Եȴ�.
    //
    // ��, logAnchor�� DataFileDescSlotID�� ����Ǳ� ������ CTBody�� flush �Ǹ�
    // �ȵȴ�.
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smLayerCallback::blockCheckpoint() != IDE_SUCCESS ); 
        sChkptBlockState = 1; 
    }


    IDE_TEST( sddTableSpace::createDataFiles(
                                   aStatistics,
                                   aTrans,
                                   sSpaceNode,
                                   aDataFileAttr,
                                   aDataFileAttrCount,
                                   aTouchMode,
                                   mMaxDataFilePageCount) // loganchor offset
              != IDE_SUCCESS );
    sAllocState = 3;

    IDU_FIT_POINT_RAISE( "3.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );

    for (i=0; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);

        IDE_TEST( sddDataFile::addPendingOperation(
                                              aTrans,
                                              sFileNode,
                                              ID_TRUE, /* commit�ÿ� ���� */
                                              SCT_POP_CREATE_DBF,
                                              &sPendingOp )
                  != IDE_SUCCESS );

        sPendingOp->mTouchMode = aTouchMode;

        IDU_FIT_POINT_RAISE( "4.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );

        // PRJ-1548 User Memory Tablespace
        // TBS Node�� X ����� ȹ���ϱ� ������ DBF Node�� X �����
        // ȹ���� �ʿ䰡 ����.
    }

    // PRJ-1548 User Memory Tablespace
    // �ý��� ���̺����̽��� ��� ���̺����̽� ��忡
    // ����� ȹ���� �ʿ䰡 ����.

    if ( sctTableSpaceMgr::isSystemTableSpace( sSpaceNode->mHeader.mID )
         != ID_TRUE )
    {
        // addTableSpaceNode�ϰ� �Ǹ� �ٸ� ������ TBS Node�� ���� ���� �����̰�
        // �������� TBS �� ���ؼ� ���� ����� ȹ���� �� �ְ� �ȴ�.
        // �̸� �����ϱ� ���ؼ� TBS List�� �߰��Ǳ� ���� ���� ����� ȹ���Ѵ�.
        // �̿� �Բ�, CREATE ���� TBS Node�� �˻��� �ȵǵ��� ó���� �ʿ䰡 �ִ�.
        // mutex�� 2PL���� lock coupling �ÿ� deadlock�� �߻��Ҽ� �ִ�.
        // �׷���, ���������� ���Ӱ� �����Ǵ� ��ü���� coupling��
        // deadlock�� �߻���Ű�� �ʴ´�.

        IDE_TEST( smLayerCallback::lockItem(
                         aTrans,
                         sSpaceNode->mHeader.mLockItem4TBS,
                         ID_FALSE,       /* intent */
                         ID_TRUE,        /* exclusive */
                         ID_ULONG_MAX,   /* lock wait micro sec. */
                         NULL,           /* locked */
                         NULL )          /* to be unlocked by transaction */
                  != IDE_SUCCESS );
    }

    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ��� ���� �޽����� ��ȯ���� �ʵ��� ����
    IDE_TEST_RAISE( sctTableSpaceMgr::checkExistSpaceNodeByName(
                                          sSpaceNode->mHeader.mName ) == ID_TRUE,
                    err_already_exist_tablespace_name );


    /* To Fix BUG-23701 [SD] Create Disk Tablespace�� SpaceNode�� ���°���
     * �����Ͽ� mSpaceNodeArr �� �߰��Ͽ��� ��. */
    sSpaceNode->mHeader.mState = SMI_TBS_CREATING | SMI_TBS_ONLINE;

    // TBS List�� �߰��ϸ鼭 TBS Count ������Ŵ
    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode*) sSpaceNode );
    sAllocState = 4;

    /* tablespace �������� mutex ���� */
    sStateTbs = 0;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                    aTrans,
                                    sSpaceNode->mHeader.mID,
                                    ID_TRUE, /* commit�ÿ� ���� */
                                    SCT_POP_CREATE_TBS)
              != IDE_SUCCESS );

    IDU_FIT_POINT( "2.PROJ-1552@sddDiskMgr::createTableSpace" );
    
    IDU_FIT_POINT_RAISE( "5.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );

    // PRJ-1548 User Memory Tablespace
    // ���ο� TBS Node�� DBF Node���� Loganchor�� �߰��Ѵ�.
    IDE_ASSERT( smLayerCallback::addTBSNodeAndFlush( (sctTableSpaceNode*)sSpaceNode )
                == IDE_SUCCESS );

    IDU_FIT_POINT( "3.PROJ-1552@sddDiskMgr::createTableSpace" );

    IDU_FIT_POINT_RAISE( "6.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );

    for( i = 0 ; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL );

        // BUGBUG
        // ���ο� DBF Node���� Loganchor�� �߰��Ѵ�.
        IDE_ASSERT( smLayerCallback::addDBFNodeAndFlush( sSpaceNode,
                                                         sFileNode )
                    == IDE_SUCCESS );

        IDU_FIT_POINT_RAISE( "7.PROJ-1548@sddDiskMgr::createTableSpace", err_ART );
    }

    //PROJ-2133 incremental backup
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        sChkptBlockState = 0; 
        IDE_TEST( smLayerCallback::unblockCheckpoint() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( err_ART );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ART));
    }
#endif
    IDE_EXCEPTION( error_already_used_datafile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_UseFileInOtherTBS, sValidName));
    }
    IDE_EXCEPTION( err_already_exist_tablespace_name )
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistTableSpaceName,
                                sSpaceNode->mHeader.mName));
    }

    IDE_EXCEPTION_END;

    if ( sStateTbs != 0 )
    {
        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
    }

    if( sChkptBlockState == 1)
    {
        IDE_ASSERT( smLayerCallback::unblockCheckpoint() == IDE_SUCCESS );
    }

    /* Tablespace�̸��� �̹� �ְų� ������ ������ �̹� �ְų�
       Ȥ�� �ٸ� ������ ���� Exception�� �߻��ϸ�
       sAllocState�� ���º��� �ڿ� ���� ó���� ����� �Ѵ�.
       BUG-18176 */

    switch (sAllocState)
    {
        case 3:
            /* sAllocState�� 3�̶�� createDataFiles() ȣ�� �����̱� ������
               ������ ���Ϻ��� ��� �����ؾ� �Ѵ�. */
            IDE_ASSERT(sddTableSpace::removeAllDataFiles(NULL,  /* idvSQL* */
                                                         NULL,  /* void *  */
                                                         sSpaceNode,
                                                         SMI_ALL_NOTOUCH,
                                                         ID_FALSE)
                       == IDE_SUCCESS);

        case 2:
            /* sAllocState�� 2��� sSpaceNode�� ���� initialize�� �����̱� ������
               destory()�� ȣ���ؼ� �ڿ��� �����ؾ� �Ѵ�. */
            IDE_ASSERT(sddTableSpace::destroy(sSpaceNode) == IDE_SUCCESS);

        case 1:
            IDE_ASSERT(iduMemMgr::free(sSpaceNode) == IDE_SUCCESS);
            break;

        default:
            /* sAllocState�� 4�̸� sSpaceNode�� �߰��� �����̱� ������
               undo�� ��� ������ �ǹǷ� ���⼭ ������ �ʴ´�. */
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : tablespace ���� �� �ʱ�ȭ
 *  PROJ-1923 ALTIBASE HDB Disaster Recovery
 *
 * ��ũ�����ڿ� ���ο� tablespace ��带 ����� �ʱ�ȭ �Ѵ�.
 *
 * CREATE TABLESPACE ������ ����� �� sdpTableSpace::create����
 * ȣ��Ǿ� meta (0/1/2��) page �ʱ�ȭ ���� ������ ���̴�.
 *
 * [!!] ��ũ������ mutex �����ϱ� ���� �α׾�Ŀ�� FLUSH �Ѵ�.
 *
 * tablespace ID�� ����Ǿ����� ���θ� �˱� ���ؼ���
 * create ������ system SCN�� tablespace�� ������ �ʿ䰡 �ִ�.
 *
 **********************************************************************/
IDE_RC sddDiskMgr::createTableSpace4Redo( void               * aTrans,
                                          smiTableSpaceAttr  * aTableSpaceAttr )
{
    sddTableSpaceNode * sSpaceNode;
    UInt                sAllocState     = 0;
    scSpaceID           sNewSpaceID     = 0;

    IDE_DASSERT( aTrans             != NULL );
    IDE_DASSERT( aTableSpaceAttr    != NULL );
    IDE_DASSERT( aTableSpaceAttr->mAttrType == SMI_TBS_ATTR );

    /* sddDiskMgr_createTableSpace4Redo_malloc_SpaceNode.tc */
    IDU_FIT_POINT("sddDiskMgr::createTableSpace4Redo::malloc::SpaceNode");
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SDD,
                                ID_SIZEOF(sddTableSpaceNode),
                                (void **)&sSpaceNode,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sAllocState = 1;
    idlOS::memset(sSpaceNode, 0x00, ID_SIZEOF(sddTableSpaceNode));

    //PRJ-1149
    aTableSpaceAttr->mDiskAttr.mNewFileID = 0;

    /* ������ mNewTableSpaceID�� ������ �α��� ���� ������ ���� ������ ��� �� */
    sNewSpaceID = sctTableSpaceMgr::getNewTableSpaceID();

    IDE_TEST( sNewSpaceID != aTableSpaceAttr->mID );

    IDE_TEST( sctTableSpaceMgr::allocNewTableSpaceID(&aTableSpaceAttr->mID)
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::initialize(sSpaceNode, aTableSpaceAttr)
              != IDE_SUCCESS );
    sAllocState = 2;

    /* ������ tablespace���� �����ϴ��� �˻��Ѵ�. */
    // BUG-26695 TBS Node�� ���°��� �����̹Ƿ� ���� ���
    // ���� �޽����� ��ȯ���� �ʵ��� ����
    IDE_TEST_RAISE( sctTableSpaceMgr::checkExistSpaceNodeByName( sSpaceNode->mHeader.mName ) == ID_TRUE, 
                    err_already_exist_tablespace_name );

    /* To Fix BUG-23701 [SD] Create Disk Tablespace�� SpaceNode�� ���°���
     * �����Ͽ� mSpaceNodeArr �� �߰��Ͽ��� ��. */
    sSpaceNode->mHeader.mState = SMI_TBS_CREATING | SMI_TBS_ONLINE;

    /* TBS List�� �߰��ϸ鼭 TBS Count ������Ŵ */
    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode *) sSpaceNode );
    sAllocState = 3;

    IDE_TEST( sctTableSpaceMgr::addPendingOperation( aTrans,
                                                     sSpaceNode->mHeader.mID, 
                                                     ID_TRUE, // commit�ÿ� ����
                                                     SCT_POP_CREATE_TBS)
              != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // ���ο� TBS Node�� DBF Node���� Loganchor�� �߰��Ѵ�.
    IDE_ASSERT( smLayerCallback::addTBSNodeAndFlush( (sctTableSpaceNode *)sSpaceNode )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_already_exist_tablespace_name )
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistTableSpaceName,
                                sSpaceNode->mHeader.mName));
    }

    IDE_EXCEPTION_END;

    switch (sAllocState)
    {
        case 2:
            /* sAllocState�� 2��� sSpaceNode�� ���� initialize�� �����̱� ������
             * destory()�� ȣ���ؼ� �ڿ��� �����ؾ� �Ѵ�. */
            IDE_ASSERT(sddTableSpace::destroy(sSpaceNode) == IDE_SUCCESS);

        case 1:
            IDE_ASSERT(iduMemMgr::free(sSpaceNode) == IDE_SUCCESS);
            break;

        default:
            /* sAllocState�� 3�̸� sSpaceNode�� �߰��� �����̱� ������
             * undo�� ��� ������ �ǹǷ� ���⼭ ������ �ʴ´�. */
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : DBF ���� �� �ʱ�ȭ
 *  PROJ-1923 ALTIBASE HDB Disaster Recovery
 *
 * ��ũ�����ڿ��� �ش� TBS�� datafile ��� �� ���� datafile�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sddDiskMgr::createDataFile4Redo( void              * aTrans,
                                        smLSN               aCurLSN,
                                        scSpaceID           aSpaceID,
                                        smiDataFileAttr   * aDataFileAttr )
{
    sddTableSpaceNode * sSpaceNode      = NULL;
    SChar             * sExistFileName  = NULL;
    sddDataFileNode   * sFileNode       = NULL;
    sctPendingOp      * sPendingOp      = NULL;
    idBool              sNameExist      = ID_FALSE;
    sdFileID            sNewFileIDSave  = 0;
    smiTableSpaceAttr   sDummySpaceAttr;
    sddTableSpaceNode   sDummySpaceNode;
    idBool              sFileExist      = ID_FALSE;
    SInt                rc              = 0;
    UInt                i               = 0;
    UInt                sNameLength     = 0;
    UInt                sChkptBlockState    = 0;
    SChar               sValidName[ SM_MAX_FILE_NAME ];

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceID ) == ID_TRUE );
    IDE_DASSERT( aDataFileAttr != NULL );

    /* ===================================
     * [1] dummy tablespace ���� �� �ʱ�ȭ
     * =================================== */
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode)
              != IDE_SUCCESS );

    IDE_TEST( validateDataFileName( sSpaceNode,
                                    &aDataFileAttr,
                                    1, // aDataFileAttrCount
                                    &sExistFileName, //�����޽��� ��¿�
                                    &sNameExist ) != IDE_SUCCESS);

    IDE_TEST_RAISE( sNameExist == ID_TRUE,
                    error_the_filename_is_in_use );

    // PRJ-1149.
    IDE_TEST_RAISE( SMI_TBS_IS_BACKUP(sSpaceNode->mHeader.mState),
                    error_forbidden_op_while_backup);

    // ������ mNewFileID�� ����� �� 1 ���� ��Ų��.
    // ������, DBF ���� �Ŀ� ����� mNewFieID��������
    // +1 �� ������ mNewFileID���� ��ó�� �Ѵ�.
    sNewFileIDSave          = sSpaceNode->mNewFileID ;
    sSpaceNode->mNewFileID += 1; // aDataFileAttrCount;

    idlOS::memset( &sDummySpaceAttr, 0x00, ID_SIZEOF(smiTableSpaceAttr));
    idlOS::memset( &sDummySpaceNode, 0x00, ID_SIZEOF(sddTableSpaceNode));
    sDummySpaceAttr.mID         = aSpaceID;
    sDummySpaceAttr.mAttrType   = SMI_TBS_ATTR;

    IDE_TEST( sddTableSpace::initialize(&sDummySpaceNode,
                                        &sDummySpaceAttr) != IDE_SUCCESS );

    sDummySpaceNode.mNewFileID = sSpaceNode->mNewFileID - 1; //aDataFileAttrCount;

    /* PROJ-2133 incremental backup
     * DataFileDescSlot�� �Ҵ�Ǿ� checkpoint�� �߻��ϸ� CTBody�� flush�Ǿ�
     * ���Ͽ� �ݿ��ȴ�. ������ loganchor�� DataFileDescSlotID�� ����Ǳ� ����
     * ������ �״´ٸ� �Ҵ�� DataFileDescSlot�� ����Ҽ� ���Եȴ�.
     *
     * ��, logAnchor�� DataFileDescSlotID�� ����Ǳ� ������ CTBody�� flush �Ǹ�
     * �ȵȴ�. */
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smLayerCallback::blockCheckpoint() != IDE_SUCCESS );
        sChkptBlockState = 1;
    }

    // ���� ���� �̸��� �ִ��� �˻��Ѵ�.
    // �ִٸ� ����� ����� �Ѵ�.
    // �ֳ��ϸ� Log Anchor���� ����� �ȵ� ���·� redo�� �����Ͽ����Ƿ�,
    // ������ DBF�� ���� �� ���� �����̴�.
    IDE_TEST( existDataFile( aDataFileAttr->mName, &sFileExist ) != IDE_SUCCESS );

    if( sFileExist == ID_TRUE )
    {
        // log Anchor�� ���� ���� ���� ��Ȳ���� �Ѿ� �����Ƿ�, ���ϸ� ����
        idlOS::strncpy( (SChar *)sValidName,
                        aDataFileAttr->mName,
                        SM_MAX_FILE_NAME );
        sNameLength = idlOS::strlen(sValidName);

        IDE_TEST( sctTableSpaceMgr::makeValidABSPath( ID_TRUE,
                                                      (SChar *)sValidName,
                                                      &sNameLength,
                                                      SMI_TBS_DISK )
                  != IDE_SUCCESS );

        // ���� ����
        rc = idf::unlink(sValidName);

        IDE_TEST_RAISE( rc != 0 , err_file_unlink );
    }
    else
    {
        /* do nothing */
    }

    /* ===================================
     * [2] ������ ����Ÿ ȭ�� ����
     * =================================== */
    IDE_TEST( sddTableSpace::createDataFile4Redo( NULL,
                                                  aTrans,
                                                  &sDummySpaceNode,
                                                  aDataFileAttr,
                                                  aCurLSN,
                                                  SMI_EACH_BYMODE,
                                                  mMaxDataFilePageCount)
              != IDE_SUCCESS );

    for (i = sNewFileIDSave; i < sSpaceNode->mNewFileID; i++ )
    {
        sFileNode = sDummySpaceNode.mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);
    }

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void **)&sSpaceNode)
              != IDE_SUCCESS );

    /* ====================================================================
     * [3] ����Ÿ ȭ�ϵ��� �ڽ��� ���� ���̺� �����̽��� ȭ�� ����Ʈ�� ���
     * ==================================================================== */

    // fix BUG-16116 [PROJ-1548] sddDiskMgr::createDataFiles()
    // ������ DBF Node�� ���� Memory ����
    // ���� For loop �ȿ��� EXCEPTION�� �߻��ϰ� �Ǹ� DBF Node
    // �޸� ������ ���� ������ �����Ƿ� exception �߻��ڵ��
    // �ڵ����� �ʴ´�.
    // �� ���Ŀ� Exception�� �߻��Ѵٸ�, Ʈ����� rollback�� ���ؼ�
    // �ڵ����� ������ ���̴�.

    for( i = sNewFileIDSave ; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sDummySpaceNode.mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);

        sFileNode->mState |= SMI_FILE_CREATING;
        sFileNode->mState |= SMI_FILE_ONLINE;

        sddTableSpace::addDataFileNode(sSpaceNode, sFileNode);
    }

    for( i = sNewFileIDSave ; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);

        IDE_TEST( sddDataFile::addPendingOperation( aTrans,
                                                    sFileNode,
                                                    ID_TRUE, /* commit�ÿ� ���� */
                                                    SCT_POP_CREATE_DBF,
                                                    &sPendingOp )
                  != IDE_SUCCESS );

        sPendingOp->mTouchMode = SMI_EACH_BYMODE; // aTouchMode;

        /* [4] �α� ��Ŀ ����
         * �ý��� ������ �α� ��Ŀ�� ������ �̿��Ͽ� ����Ÿ ȭ�� ����Ʈ�� �����ϸ�,
         * �� ��쿡�� �α� ��Ŀ�� �纯������ �ʴ´�. */
        IDE_ASSERT( smLayerCallback::addDBFNodeAndFlush( sSpaceNode,
                                                         sFileNode )
                    == IDE_SUCCESS );
    }

    // PROJ-2133 incremental backup
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        sChkptBlockState = 0;
        IDE_TEST( smLayerCallback::unblockCheckpoint() != IDE_SUCCESS );
    }

    IDE_TEST( sddTableSpace::destroy( &sDummySpaceNode ) != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // ���ο� TBS Node�� DBF Node���� Loganchor�� �߰��Ѵ�.
    IDE_ASSERT( smLayerCallback::updateTBSNodeAndFlush( (sctTableSpaceNode *)sSpaceNode )
                == IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION(error_forbidden_op_while_backup);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_forbiddenOpWhileBackup,
                                "ADD DATAFILE"));
    }
    IDE_EXCEPTION( error_the_filename_is_in_use )
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UseFileInTheTBS,
                                 sExistFileName,
                                 ((sctTableSpaceNode *)sSpaceNode)->mName));
    }
    IDE_EXCEPTION(err_file_unlink);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FileDelete, sValidName));
    }
    IDE_EXCEPTION_END;

    if( sChkptBlockState != 0 )
    {
        IDE_ASSERT( smLayerCallback::unblockCheckpoint()
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α׾�Ŀ�� ���� tablespace ��� ���� �� �ʱ�ȭ
 *
 * �α׾�Ŀ�� ����� tablespace ������ �̿��Ͽ� ��ũ�����ڿ� �ϳ���
 * tabespace ��带 �����ϰ� �ʱ�ȭ�Ѵ�. �α׾�Ŀ ������ �ʱ�ȭ�ÿ� ȣ��Ǿ�
 * tablespace ��常 �����Ͽ� HASH �� tablespace ��� ����Ʈ�� �߰�������,
 * ���� ȭ���� create���� �ʴ´�.
 *
 * + 2nd. code design
 *   - tablespace ��带 ���� �޸𸮸� �Ҵ��Ѵ�.
 *   - tablespace ��带 �ʱ�ȭ�Ѵ�. -> sddTableSpace::initialize
 *   - ��ũ�������� mutex ȹ��
 *   - ��ũ�������� HASH�� �߰��Ѵ�.
 *   - tablespace ��� ����Ʈ�� �߰��Ѵ�.
 *   - tablespace ��� �� space ID�� ���� ū ID�� ���ؼ�
 *     SMU_MAX_TABLESPACE_ID�� ���Ͽ� mNewTableSpaceID��
 *     �����Ѵ�.
 *   - ��ũ�������� mutex ����
 **********************************************************************/
IDE_RC sddDiskMgr::loadTableSpaceNode( idvSQL*           /* aStatistics*/,
                                       smiTableSpaceAttr* aTableSpaceAttr,
                                       UInt               aAnchorOffset )
{
    sddTableSpaceNode*   sSpaceNode;
    UInt                 sState = 0;

    IDE_DASSERT( aTableSpaceAttr != NULL );
    IDE_DASSERT( aTableSpaceAttr->mAttrType == SMI_TBS_ATTR );

    sSpaceNode = NULL;

    /* sddDiskMgr_loadTableSpaceNode_calloc_SpaceNode.tc */
    IDU_FIT_POINT("sddDiskMgr::loadTableSpaceNode::calloc::SpaceNode");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SDD,
                                1,
                                ID_SIZEOF(sddTableSpaceNode),
                                (void**)&sSpaceNode,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sddTableSpace::initialize( sSpaceNode,
                                         aTableSpaceAttr) != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    sSpaceNode->mAnchorOffset = aAnchorOffset;

    sctTableSpaceMgr::addTableSpaceNode( (sctTableSpaceNode*)sSpaceNode );
    sState = 0;
    /* �ѹ� SpaceNode Array�� ���� �� SpaceNode�� Service �߿��� ���� �� �� ����.*/

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( iduMemMgr::free( sSpaceNode ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  tablespace ���� (��� ���Ÿ� Ȥ�� ��� �� ���� ����)
 *
 * tablespace�� �����ϸ�, drop ��忡 ���� datafile���� ������ ���� �ִ�.
 *
 * - DDL���� DROP TABLESPACE�� ����� ȣ��ȴ�. �̴� �ش� tablespace��
 *   OFFLINE�̾�߸� �����ϴ�.
 * - OFFLINE ���� tablespace�� �ٲ� ��, �̹� datafile�� close �Ǿ� �ִ�.
 * - �� �Լ��� SDP �ܿ��� ȣ��Ǹ�, �� �۾��� �α׾�Ŀ FLUSH �۾��� ����ȴ�.
 *   �� �۾��� ���� ���� datafile LRU ����Ʈ�� ���� HASH�� ���� ������ �ʴ´�.
 *
 * + 2nd. code design
 *   - ��ũ������ mutex ȹ���Ѵ�.
 *   - HASH���� tablespace ��带 ã�´�.
 *   - if( �˻��� tablespace ��尡 ONLINE�̸� )
 *     {
 *        ��ũ������ mutex ����;
 *        return fail;
 *     }
 *   - tablespace ��带 ����Ʈ���� �����Ѵ�.
 *   - tablespace ��带 HASH���� �����Ѵ�.
 *   - �α׾�Ŀ FLUSH -> smrLogMgr::updateLogAnchorForTableSpace
 *   - ��ũ������ mutex �����Ѵ�.
 *   - tablespace ��带 destroy �Ѵ� -> sddTableSpace::destroy(aMode)
 *   - tablespace ����� �޸𸮸� �����Ѵ�.
 **********************************************************************/
IDE_RC sddDiskMgr::removeTableSpace( idvSQL            *aStatistics,
                                     void*              aTrans,
                                     scSpaceID          aTableSpaceID,
                                     smiTouchMode       aTouchMode )
{
    UInt                sState = 0;
    sddTableSpaceNode*  sSpaceNode;
    sctPendingOp     *  sPendingOp;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aTouchMode != SMI_EACH_BYMODE );

    // Tablespace Backup�� ���ü� Ȯ�� �ؾ� �Ѵ�.
    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
              != IDE_SUCCESS );
    sState = 1;

    // backup ���� ���̺� �����̽��� drop �Ҽ� ����
    IDE_TEST_RAISE( SMI_TBS_IS_BACKUP(sSpaceNode->mHeader.mState), 
                    error_forbidden_op_while_backup );

    // dbf ���Ŵ� commit ���Ľ������� tx�� ���� unlink�ϵ��� ó���Ѵ�.
    // SMR_DLT_FILEOPER : SCT_UPDATE_DRDB_DROP_DBF
    // before image : tablespace attribute
    IDE_TEST( sddTableSpace::removeAllDataFiles(aStatistics,
                                                aTrans,
                                                sSpaceNode,
                                                aTouchMode,
                                                ID_TRUE) /* GhostMark */
              != IDE_SUCCESS );

    // FOR BUG-9640
    // SMR_DLT_FILEOPER : SCT_UPDATE_DRDB_DROP_TBS
    // before image : tablespace attribute
    IDE_TEST( smLayerCallback::writeDiskTBSCreateDrop(
                                                  aStatistics,
                                                  aTrans,
                                                  SCT_UPDATE_DRDB_DROP_TBS,
                                                  sSpaceNode->mHeader.mID,
                                                  NULL, /* PROJ-1923 */
                                                  NULL ) != IDE_SUCCESS );

    /* ------------------------------------------------
     * !! CHECK RECOVERY POINT
     * case) dbf node�� ���ŵǰ�, anchor�� flush�ȵ� ���
     * redo�� tbs node  �����ϰ�, undo�� before tbs image��
     * �ٽ� �����Ѵ�.
     * ----------------------------------------------*/
    IDU_FIT_POINT( "1.PROJ-1552@sddDiskMgr::removeTableSpace" );

    sSpaceNode->mHeader.mState |= SMI_TBS_DROPPING;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    IDE_TEST( sctTableSpaceMgr::addPendingOperation( aTrans,
                                                     aTableSpaceID,
                                                     ID_TRUE, /* commit�ÿ� ���� */
                                                     SCT_POP_DROP_TBS,
                                                     &sPendingOp )
              != IDE_SUCCESS );

    /* BUG-29941 - SDP ��⿡ �޸� ������ �����մϴ�.
     * Commit Pending ���� ������ Space Cache�� �Ҵ�� �޸𸮸� �����ϵ���
     * PendingOpFunc�� ����Ѵ�. */
    sPendingOp->mPendingOpFunc = smLayerCallback::freeSpaceCacheCommitPending;

    // DROP TABLESPACE �߰����� �α׾�Ŀ �÷����� ���� �ʴ´�.

    IDU_FIT_POINT( "1.TASK-1842@sddDiskMgr::removeTableSpace" );

    IDU_FIT_POINT( "2.PROJ-1552@sddDiskMgr::removeTableSpace" );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_forbidden_op_while_backup);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_forbiddenOpWhileBackup,
                                "DROP TABLESPACE"));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : page �ǵ� (1)
 *
 * �ش� tablespace�� �ش� page�� disk�κ��� READ �Ѵ�.
 * Read�� �̹� TableSpaceLock�� ���� ��쿩�� SpaceNode Mutex�� ���� �ʿ䰡 ����.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + �ش� page�� ���Ե� datafile ��带 ã�´�
 *     -> sddTableSpace::getDataFileNodeByPageID
 *   + Read I/O�� �غ��Ѵ�. -> prepareIO
 *   + ��ũ������ mutex ����
 *   + Read I/O�� �����Ѵ�.-> sddDataFile::read
 *   + ��ũ������ mutex ȹ��
 *   + Read I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ������ mutex ����
 **********************************************************************/
IDE_RC sddDiskMgr::read( idvSQL     * aStatistics,
                         scSpaceID    aTableSpaceID,
                         scPageID     aPageID,
                         UChar      * aBuffer,
                         UInt       * aDataFileID )
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( aTableSpaceID )
                 == ID_FALSE );
    IDE_DASSERT( aBuffer != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(
                                    sSpaceNode,
                                    aPageID,
                                    &sFileNode,
                                    ID_FALSE /* could be failed */)
              != IDE_SUCCESS );

    IDE_ASSERT(sFileNode != NULL);
    //PRJ-1149.
    *aDataFileID =  sFileNode->mID;

    IDE_TEST( readPageFromDataFile( aStatistics,
                                    sFileNode,
                                    aPageID,
                                    1, /* Page Count */
                                    aBuffer )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aFileNode�� ����Ű�� ���Ͽ��� aPageID�� �д´�.
 *
 * PRJ-1149����
 * ����Ÿ ������ backup�ƴ� ���� �����϶� ����Ÿ ���Ϸκ���
 * page�� �о�´�.
 *
 * aStatistics - [IN] �������
 * aFileNode   - [IN] ���ϳ��
 * aSpaceID    - [IN] TableSpaceID
 * aPageID     - [IN] PageID
 * aPageCnt    - [IN] Page Count
 * aBuffer     - [IN] Page������ ����ִ� Buffer
 **********************************************************************/
IDE_RC  sddDiskMgr::readPageFromDataFile( idvSQL           * aStatistics,
                                          sddDataFileNode  * aFileNode,
                                          scPageID           aPageID,
                                          ULong              aPageCnt,
                                          UChar            * aBuffer )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;
 
    IDE_TEST( sddDataFile::read( aStatistics,
                                 aFileNode,
                                 SD_MAKE_FOFFSET( aPageID ),
                                 SD_PAGE_SIZE * aPageCnt,
                                 aBuffer )
              != IDE_SUCCESS);

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Page From Data File Failure\n"
                 "               Tablespace ID = %"ID_UINT32_FMT"\n"
                 "               File ID       = %"ID_UINT32_FMT"\n"
                 "               IO Count      = %"ID_UINT32_FMT"\n"
                 "               File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );

    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : aFileNode�� ����Ű�� ���Ͽ��� aPageID�� �д´�.
 *
 * PRJ-1149����
 * ����Ÿ ������ backup�ƴ� ���� �����϶� ����Ÿ ���Ϸκ���
 * page�� �о�´�.
 *
 * aStatistics - [IN] �������
 * aFileNode   - [IN] ���ϳ��
 * aSpaceID    - [IN] TableSpaceID
 * aPageID     - [IN] PageID
 * aPageCnt    - [IN] Page Count
 * aBuffer     - [IN] Page������ ����ִ� Buffer
 **********************************************************************/
IDE_RC  sddDiskMgr::readvPageFromDataFile( idvSQL           * aStatistics,
                                           sddDataFileNode  * aFileNode,
                                           scPageID           aPageID,
                                           iduFileIOVec     & aVec )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    IDE_TEST( sddDataFile::readv( aStatistics,
                                  aFileNode,
                                  SD_MAKE_FOFFSET( aPageID ),
                                  aVec)
              != IDE_SUCCESS);

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Page From Data File Failure\n"
                 "               Tablespace ID = %"ID_UINT32_FMT"\n"
                 "               File ID       = %"ID_UINT32_FMT"\n"
                 "               IO Count      = %"ID_UINT32_FMT"\n"
                 "               File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );


    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page �ǵ� (2)
 *
 * �ش� tablespace�� �ش� page���� pagecount��ŭ disk�κ��� READ �Ѵ�.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + frompid ���� topid���� ��� �о���϶����� ������ ����.
 *     + �ش� page�� ���Ե� datafile ��带 ã�´�
 *        -> sddTableSpace::getDataFileNodeByPageID
 *     + Read I/O�� �غ��Ѵ�. -> prepareIO
 *     + ��ũ������ mutex ����
 *     + Read I/O�� �����Ѵ�.-> sddDataFile::read
 *     + ��ũ�������� mutex ȹ���Ѵ�.
 *     + Read I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ�������� mutex �����Ѵ�.
 *
 * aStatistics - [IN] �������
 * aFileNode   - [IN] ���ϳ��
 * aSpaceID    - [IN] TableSpaceID
 * aFstPageID  - [IN] PageID
 * aBuffer     - [IN] Page������ ����ִ� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::read( idvSQL*       aStatistics,
                         scSpaceID     aTableSpaceID,
                         scPageID      aFstPageID,
                         ULong         aPageCount,
                         UChar*        aBuffer )
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( aTableSpaceID )
                 == ID_FALSE );
    IDE_DASSERT( aBuffer != NULL );

    sFileNode = NULL;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(sSpaceNode,
                                                     aFstPageID,
                                                     &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( readPageFromDataFile( aStatistics,
                                    sFileNode,
                                    aFstPageID,
                                    aPageCount,
                                    aBuffer )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : page �ǵ� (2)
 *
 * �ش� tablespace�� �ش� page���� pagecount��ŭ disk�κ��� READ �Ѵ�.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + frompid ���� topid���� ��� �о���϶����� ������ ����.
 *     + �ش� page�� ���Ե� datafile ��带 ã�´�
 *        -> sddTableSpace::getDataFileNodeByPageID
 *     + Read I/O�� �غ��Ѵ�. -> prepareIO
 *     + ��ũ������ mutex ����
 *     + Read I/O�� �����Ѵ�.-> sddDataFile::read
 *     + ��ũ�������� mutex ȹ���Ѵ�.
 *     + Read I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ�������� mutex �����Ѵ�.
 *
 * aStatistics - [IN] �������
 * aFileNode   - [IN] ���ϳ��
 * aSpaceID    - [IN] TableSpaceID
 * aFstPageID  - [IN] PageID
 * aBuffer     - [IN] Page������ ����ִ� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::readv( idvSQL*        aStatistics,
                          scSpaceID      aTableSpaceID,
                          scPageID       aFstPageID,
                          iduFileIOVec & aVec )
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( aTableSpaceID )
                 == ID_FALSE );

    sFileNode = NULL;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(sSpaceNode,
                                                     aFstPageID,
                                                     &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( sddDiskMgr::readvPageFromDataFile( aStatistics,
                                                 sFileNode,
                                                 aFstPageID,
                                                 aVec )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : page ��� (1)
 *
 * �ش� tablespace�� �ش� page�� disk�� WRITE �Ѵ�.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + �ش� page�� ���Ե� datafile ��带 ã�´�
 *     -> sddTableSpace::getDataFileNodeByPageID
 *   + Write I/O�� �غ��Ѵ�. -> prepareIO
 *   + ��ũ������ mutex ����
 *   + Write I/O�� �����Ѵ�.-> sddDataFile::write
 *   + ��ũ������ mutex ȹ��
 *   + Write I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ������ mutex ����
 **********************************************************************/
IDE_RC sddDiskMgr::write(idvSQL*    aStatistics,
                         scSpaceID  aTableSpaceID,
                         scPageID   aPageID,
                         UChar*     aBuffer )
{
    sddTableSpaceNode *sSpaceNode;
    sddDataFileNode   *sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( aTableSpaceID )
                 == ID_FALSE );
    IDE_DASSERT( aBuffer != NULL );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(
                                    sSpaceNode,
                                    aPageID,
                                    &sFileNode)
              != IDE_SUCCESS );

    // �Ϲ������δ� Offline TBS�� Write�� ��û���� �ʴ´�.
    // Offline���� Buffer�� �̸� �����Ѵ�.
    // �׷��Ƿ� lock�� ��� ���� �ʾƵ� �ȴ�.
    if ( SMI_FILE_STATE_IS_OFFLINE( sFileNode->mState ) )
    {
        // Restart Recovery �Ǵ� Media Recovery �ÿ���
        // Offline TBS�� ���� write�� �߻��� �� �ִ�.

        // ���� �������� ������ �����ϴ�.
        // 1) Media Recovery  (open/read/write)
        // : Loganchor���� Offline ����������,
        //   backup ������ Online ������ ��� ������ �ؾ��Ѵ�.

        // 2) RestartRecovery (open/read/write)
        // : Loganchor�� Online ����������,
        //   Redo�ϴٰ� ���������δ� Offline���·� �Ǵ� ���

        // 3) checkpoint�� Write DBF Hdr (Open/Write)

        // 4) identify �� read DBF Hdr (Open/Read)

        IDE_TEST_RAISE( isEnableWriteToOfflineDBF() == ID_FALSE ,
                        err_access_to_offline_datafile );
        // ��߿��� Abort ó���Ǿ�� �Ѵ�.
    }

    IDE_TEST( writePage2DataFile( aStatistics,
                                  sFileNode,
                                  aPageID,
                                  aBuffer )
              != IDE_SUCCESS);

    /* 
     * PROJ-2133 incremental backup
     * changeTracking�� �������� ���������Ͽ� write�� �Ŀ� ����Ǿ߸� �Ѵ�.
     * �׷��� ���� ���, changeTracking�Լ� ������ DataFileDescSlot�� tracking���°�
     * deactive���� active�� �ٲ��� flush�Ǵ� �������� ���� changeTracking��
     * ������� ���� �� �ִ�.
     *
     * ��, changeTracking�� �������� ���������Ͽ� write�� �� ��������������,
     * DataFileDescslot�� ���°� deactive���� active�ٲ�°�� �ش� ��������
     * ���� ������ level0 ����� ���Եȴ�.
     */
    if ( ( sctTableSpaceMgr::isTempTableSpace( sSpaceNode ) != ID_TRUE ) &&
         ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) )
    {
        IDE_TEST( smriChangeTrackingMgr::changeTracking4WriteDiskPage( sFileNode,
                                                                       aPageID ) 
                  != IDE_SUCCESS );
    }
    else
    {
        //nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_access_to_offline_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_MustBeDataFileOnlineMode,
                                  sFileNode->mID ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 * aFstPID ~ aFstPID + aPageCnt ������ �������� aFileNode�� ����Ű�� ����
 * �� ����Ѵ�.
 *
 * PRJ-1149����
 * ����Ÿ ������ backup�� �ƴ� ��������϶�
 * ����Ÿ���Ͽ� dirty page�� write�Ѵ�.
 *
 * aStatistics - [IN] ��� ����
 * aFileNode   - [IN] FileNode
 * aFstPID     - [IN] ù��° PageID
 * aPageCnt    - [IN] Write Page Count
 * aBuffer     - [IN] Page������ ���� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::writeMultiPage2DataFile(idvSQL           *  aStatistics,
                                           sddDataFileNode  *  aFileNode,
                                           scPageID            aFstPID,
                                           ULong               aPageCnt,
                                           UChar            *  aBuffer )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_DASSERT( aFileNode != NULL );
    IDE_DASSERT( aBuffer != NULL );

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    IDE_TEST( sddDataFile::write( aStatistics,
                                  aFileNode,
                                  SD_MAKE_FOFFSET( aFstPID ),
                                  aPageCnt * SD_PAGE_SIZE ,
                                  aBuffer,
                                  smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_WRITE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Write Page To Data File Failure\n"
                 "              Tablespace ID = %"ID_UINT32_FMT"\n"
                 "              File ID       = %"ID_UINT32_FMT"\n"
                 "              IO Count      = %"ID_UINT32_FMT"\n"
                 "              File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );

    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 * aFstPID ~ aFstPID + aPageCnt ������ �������� aFileNode�� ����Ű�� ����
 * �� ����Ѵ�.
 *
 * PRJ-1149����
 * ����Ÿ ������ backup�� �ƴ� ��������϶�
 * ����Ÿ���Ͽ� dirty page�� write�Ѵ�.
 *
 * aStatistics - [IN] ��� ����
 * aFileNode   - [IN] FileNode
 * aFstPID     - [IN] ù��° PageID
 * aPageCnt    - [IN] Write Page Count
 * aBuffer     - [IN] Page������ ���� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::writevMultiPage2DataFile( idvSQL           * aStatistics,
                                             sddDataFileNode  * aFileNode,
                                             scPageID           aFstPID,
                                             iduFileIOVec     & aVec )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_DASSERT( aFileNode != NULL );

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    IDE_TEST( sddDataFile::writev( aStatistics,
                                   aFileNode,
                                   SD_MAKE_FOFFSET( aFstPID ),
                                   aVec,
                                   smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_WRITE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Write Page To Data File Failure\n"
                 "              Tablespace ID = %"ID_UINT32_FMT"\n"
                 "              File ID       = %"ID_UINT32_FMT"\n"
                 "              IO Count      = %"ID_UINT32_FMT"\n"
                 "              File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );

    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}


/***********************************************************************
 * Description :
 * aFstPID ~ aFstPID + aPageCnt ������ �������� aFileNode�� ����Ű�� ����
 * �� ����Ѵ�.
 *
 * PRJ-1149����
 * ����Ÿ ������ backup�� �ƴ� ��������϶�
 * ����Ÿ���Ͽ� dirty page�� write�Ѵ�.
 *
 * aStatistics - [IN] ��� ����
 * aFileNode   - [IN] FileNode
 * aFstPID     - [IN] ù��° PageID
 * aPageCnt    - [IN] Write Page Count
 * aBuffer     - [IN] Page������ ���� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::writePage2DataFile(idvSQL            * aStatistics,
                                      sddDataFileNode   * aFileNode,
                                      scPageID            aPageID,
                                      UChar             * aBuffer )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_DASSERT( aFileNode != NULL );
    IDE_DASSERT( aBuffer != NULL );

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    IDE_TEST( sddDataFile::write( aStatistics,
                                  aFileNode,
                                  SD_MAKE_FOFFSET( aPageID ),
                                  SD_PAGE_SIZE ,
                                  aBuffer,
                                  smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_WRITE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Write Page To Data File Failure\n"
                 "              Tablespace ID = %"ID_UINT32_FMT"\n"
                 "              File ID       = %"ID_UINT32_FMT"\n"
                 "              IO Count      = %"ID_UINT32_FMT"\n"
                 "              File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );

    if ( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : sddDiskMgr::abortBackupTableSpace
 * PRJ-1149����
 * server shutdown�ÿ� �ϳ��� DISK tablespace��
 * backup begin �Ǿ� �ִ� ���(end backup�� ȣ������ �������)
 * page image �α׸� ��� ����Ÿ���̽��� �ݿ��Ͽ��� �ϰ�,
 * tablespace ���¸� online���� �������־�� �Ѵ�.
 **********************************************************************/
void sddDiskMgr::abortBackupAllTableSpace( idvSQL*  aStatistics )
{

    sddTableSpaceNode* sCurrSpaceNode;

    sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getFirstSpaceNode();

    while( sCurrSpaceNode != NULL )
    {
        IDE_ASSERT( (sCurrSpaceNode->mHeader.mState & SMI_TBS_DROPPED)
                    != SMI_TBS_DROPPED );

        if ( sctTableSpaceMgr::isDiskTableSpace( sCurrSpaceNode ) == ID_TRUE )
        {
            if ( SMI_TBS_IS_BACKUP(sCurrSpaceNode->mHeader.mState) )
            {
                abortBackupTableSpace( aStatistics,
                                       sCurrSpaceNode );
            }
        }

        sCurrSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getNextSpaceNode( sCurrSpaceNode->mHeader.mID );
    }
}

/***********************************************************************
 * Description : sddDiskMgr::abortBackupTableSpace
 * veritas ���� ���� alter tablespace begin backup���� abort�� ó����.
 * SpaceNode Lock���� ��ȣ �Ǿ�� �Ѵ�.
 **********************************************************************/
void sddDiskMgr::abortBackupTableSpace( idvSQL            * aStatistics,
                                        sddTableSpaceNode * aSpaceNode )
{
    sddDataFileNode*      sDataFileNode;
    UInt                  i;

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sDataFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sDataFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_BACKUP( sDataFileNode->mState ) )
        {
            completeFileBackup( aStatistics,
                                aSpaceNode,
                                sDataFileNode) ;
        }
    }

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    aSpaceNode->mHeader.mState &= ~SMI_TBS_BACKUP;

    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
}

/***********************************************************************
 * Description : sddDiskMgr::completeFileBackup
 * - ����Ÿ ���� backup(�� copy)�� ����ʿ�����. DataFileNode�� ���¸�
 * �����Ѵ�.
 *
 *   aStatistics      - [IN] �������
 *   aDataFileNode    - [IN] ��� backup�� DBFile�� DataFileNode
 **********************************************************************/
void sddDiskMgr::completeFileBackup( idvSQL*             aStatistics,
                                     sddTableSpaceNode * aSpaceNode,
                                     sddDataFileNode*    aDataFileNode )
{
    IDE_ASSERT( aDataFileNode    != NULL );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    IDE_DASSERT( SMI_FILE_STATE_IS_BACKUP( aDataFileNode->mState ) );
    IDE_DASSERT( SMI_FILE_STATE_IS_NOT_DROPPED( aDataFileNode->mState ) );

    aDataFileNode->mState &= ~SMI_FILE_BACKUP;

    sctTableSpaceMgr::wakeup4Backup( aSpaceNode );

    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
}

/***********************************************************************
 * Description : sddDiskMgr::prepareFileBackup
 * PRJ-1149����
 - ����Ÿ ���� ����� ���¸� �����Ѵ�.
  : online-> backup begin

  - File�� State�� ���� �ϹǷ� SpaceNode Mutex�� ��ƾ� �Ѵ�.

  - �׵��� write�� ���� �ݿ��ǵ��� sync�� �ѹ� ���ش�.
    �� Sync�� �� �־�� �ϴ� ���� �ƴϴ�.
    
    Open �Ǿ� ���� ������ �׳� �Ѿ��.
  
  - FileNode�� mState �� SpaceMutex�� ��ȣ�� �޾ƾ� �Ѵ�.

 **********************************************************************/
IDE_RC  sddDiskMgr::prepareFileBackup( idvSQL*            aStatistics,
                                       sddTableSpaceNode* aSpaceNode,
                                       sddDataFileNode  * aDataFileNode )
{
    UInt      sState = 0;

    sddDataFile::lockFileNode( aStatistics,
                               aDataFileNode );
    sState = 1;

    if (( aDataFileNode->mIsOpened == ID_TRUE) &&
        ( aDataFileNode->mIsModified == ID_TRUE ))
    {
        aDataFileNode->mIsModified = ID_FALSE;
        aDataFileNode->mIOCount++;

        sState = 0;
        sddDataFile::unlockFileNode( aDataFileNode );

        IDE_TEST_RAISE( sddDataFile::sync( aDataFileNode,
                                           smLayerCallback::setEmergency ) != IDE_SUCCESS,
                        error_sync_datafile);

        sddDataFile::lockFileNode( aStatistics,
                                   aDataFileNode );
        sState = 1;

        aDataFileNode->mIOCount--;
    }

    sState = 0;
    sddDataFile::unlockFileNode( aDataFileNode );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    aDataFileNode->mState |= SMI_FILE_BACKUP ;

    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(error_sync_datafile);
    {
        ideLog::log(SM_TRC_LOG_LEVEL_ABORT,
                    SM_TRC_DISK_FILE_SYNC_ERROR);
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sddDataFile::unlockFileNode( aDataFileNode );
    }

    return IDE_FAILURE;

}

/*
  ���̺����̽��� DBF ������ ����Ϸ��۾��� �����Ѵ�.


*/
IDE_RC sddDiskMgr::endBackupDiskTBS( idvSQL            * aStatistics,
                                     sddTableSpaceNode * aSpaceNode )
{
    sddDataFileNode*    sDataFileNode;
    UInt                i;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    // alter tablespace  backup begin A�� �ϰ���,
    // alter tablespace  backup end B�� �ϴ� ��츦 ���������̴�.
    IDE_TEST_RAISE( (aSpaceNode->mHeader.mState & SMI_TBS_BACKUP)
                    != SMI_TBS_BACKUP,
                    error_not_begin_backup);

    //���̺� �����̽��� backup�����̱⶧����
    // ����Ÿ ���� �迭�� �״�� ���� �ȴ�.
    /* ------------------------------------------------
     * ���̺� �����̽��� ����Ÿ ���Ͽ���
     * page image logging�ݿ�.
     * ----------------------------------------------*/

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sDataFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sDataFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
        {
            continue;
        }

        completeFileBackup( aStatistics,
                            aSpaceNode,
                            sDataFileNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_begin_backup );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotBeginBackup,
                                  aSpaceNode->mHeader.mID ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page ��� (2)
 *
 * �ش� tablespace�� �ش� page���� pagecount��ŭ disk�κ��� write �Ѵ�.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + frompid ���� topid���� ��� ����Ҷ����� ������ ����.
 *     + �ش� page�� ���Ե� datafile ��带 ã�´�
 *        -> sddTableSpace::getDataFileNodeByPageID
 *     + Write I/O�� �غ��Ѵ�. -> prepareIO
 *     + ��ũ������ mutex ����
 *     + Write I/O�� �����Ѵ�.-> sddDataFile::write
 *     + ��ũ�������� mutex ȹ���Ѵ�.
 *     + Write I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ�������� mutex �����Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aTableSpaceID - [IN] TableSpaceID
 * aFstPID       - [IN] First PID
 * aPageCount    - [IN] Write�� Page Count
 * aBuffer       - [IN] Write�� Page ������ ����ִ� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::writeMultiPage( idvSQL      * aStatistics,
                                   scSpaceID     aTableSpaceID,
                                   scPageID      aFstPID,
                                   ULong         aPageCount,
                                   UChar       * aBuffer )
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aBuffer != NULL );

    sFileNode      = NULL;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode)
                  != IDE_SUCCESS );


    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(sSpaceNode,
                                                     aFstPID,
                                                     &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( writeMultiPage2DataFile( aStatistics,
                                       sFileNode,
                                       aFstPID,
                                       aPageCount,
                                       aBuffer )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : page ��� (2)
 *
 * �ش� tablespace�� �ش� page���� pagecount��ŭ disk�κ��� write �Ѵ�.
 *
 * - 2nd. code design
 *   + ��ũ������ mutex ȹ��
 *   + ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   + frompid ���� topid���� ��� ����Ҷ����� ������ ����.
 *     + �ش� page�� ���Ե� datafile ��带 ã�´�
 *        -> sddTableSpace::getDataFileNodeByPageID
 *     + Write I/O�� �غ��Ѵ�. -> prepareIO
 *     + ��ũ������ mutex ����
 *     + Write I/O�� �����Ѵ�.-> sddDataFile::write
 *     + ��ũ�������� mutex ȹ���Ѵ�.
 *     + Write I/O �ϷḦ �����Ѵ�. -> completeIO
 *   + ��ũ�������� mutex �����Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aTableSpaceID - [IN] TableSpaceID
 * aFstPID       - [IN] First PID
 * aPageCount    - [IN] Write�� Page Count
 * aBuffer       - [IN] Write�� Page ������ ����ִ� Buffer
 **********************************************************************/
IDE_RC sddDiskMgr::writevMultiPage( idvSQL      * aStatistics,
                                    scSpaceID     aTableSpaceID,
                                    scPageID      aFstPID,
                                    iduFileIOVec& aVec )
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID )
                 == ID_TRUE );

    sFileNode = NULL;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode)
                  != IDE_SUCCESS );


    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(sSpaceNode,
                                                     aFstPID,
                                                     &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( writevMultiPage2DataFile( aStatistics,
                                        sFileNode,
                                        aFstPID,
                                        aVec )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile�� ���� I/O�� �ϱ� ���� �ش� datafile ��� ����
 *
 * read, write ���� IOCount�� �����ϰ�,
 * ���� open �Ǿ� ���� �ʴٸ� open�Ѵ�. 
 **********************************************************************/
IDE_RC sddDiskMgr::prepareIO( idvSQL          * aStatistics,
                              sddDataFileNode * aDataFileNode )
{
    IDE_DASSERT( aDataFileNode != NULL );

    // BUG-17158
    // Offline TBS�� ���ؼ� Prepare IO�� ���� �ʴ´�.
    // �ֳ��ϸ� Read�� Write�� ��Ȳ�� ���� ����ؾ� �ϱ� �����̴�.

    sddDataFile::prepareIO( aStatistics,
                            aDataFileNode );

    if ( aDataFileNode->mIsOpened == ID_FALSE )
    {
        return openDataFile( aStatistics,
                             aDataFileNode );
    }

    if ( aDataFileNode->mNode4LRUList.mNext != NULL )
    {
        // IOCount == 0 ���� VictimList�� ����Ǿ� �ִ� ���¿���
        // ���� IOCount++ �Ͽ� 1�� ó�� ������ ����̴�.
        // Victim List���� �����.
        removeNodeList( aStatistics,
                        aDataFileNode );
    }
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : datafile�� ���� I/O �Ϸ��� datafile ��� ����
 *
 * IO Count ���� �� DataFile ���� ���� �����Ѵ�.
 * ���� FD �� ������ ��Ȳ�̰�, close �����ϸ�, close File �Ѵ�.
 *
 **********************************************************************/
IDE_RC sddDiskMgr::completeIO( idvSQL          * aStatistics,
                               sddDataFileNode * aDataFileNode,
                               sddIOMode         aIOMode )
{
    IDE_DASSERT( aDataFileNode != NULL );

    sddDataFile::completeIO( aStatistics,
                             aDataFileNode,
                             aIOMode );

    if ( sddDataFile::getIOCount( aDataFileNode ) == 0 )
    {
        if (( idCore::acpAtomicGet32( &mWaitThr4Open ) > 0 ) ||
            ( mOpenFileCount >= smuProperty::getMaxOpenDataFileCount() ))
        {
            IDE_TEST( closeDataFile( aStatistics,
                                     aDataFileNode ) != IDE_SUCCESS );
        }
        else
        {
            if ( aDataFileNode->mNode4LRUList.mNext == NULL )
            {
                // �� ���̿� 0�� �ƴҼ��� �ִ�.
                // 0�� �ƴ� �� �־ �ϴ� �߰��Ѵ�.
                // 0�ε� List�� ���� ��츸 �ƴϸ� �ȴ�.
                addNodeList( aStatistics,
                             aDataFileNode );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ���̺����̽��� Dirty�� ����Ÿ������ Sync�Ѵ�. (WRAPPER-FUNCTION)

  [IN] aSpaceID : Sync�� ��ũ ���̺����̽� ID
*/
IDE_RC sddDiskMgr::syncTBSInNormal( idvSQL *   aStatistics,
                                    scSpaceID  aSpaceID )
{
    idBool             sIsLocked = ID_FALSE;
    sctTableSpaceNode* sSpaceNode;

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aSpaceID,
                                                               (void**)&sSpaceNode )
              != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    IDE_TEST( sddTableSpace::doActSyncTBSInNormal( aStatistics,
                                                   sSpaceNode,
                                                   NULL )
              != IDE_SUCCESS );

    sIsLocked = ID_FALSE;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if ( sIsLocked == ID_TRUE )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }
    return IDE_FAILURE;
}

/*
 * Dirty �Ǿ����� Sync�Ѵ�. drop file ���� ������� �ʴ´�.
 */
IDE_RC sddDiskMgr::syncFile( idvSQL *   aStatistics,
                             scSpaceID  aSpaceID,
                             sdFileID   aFileID )
{
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    idBool              sPreparedIO = ID_FALSE;
    idBool              sIsLocked   = ID_FALSE;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByID( sSpaceNode,
                                                  aFileID,
                                                  &sFileNode )
              != IDE_SUCCESS );

    sddDataFile::lockFileNode( aStatistics,
                               sFileNode );
    sIsLocked = ID_TRUE;

    // 1. close�Ǿ���. close �ϱ� ���� sync �Ͽ��� ���̴�.
    // 2. Is Modified �� False��, �ٸ� �������� Sync �Ͽ���.
    if (( sFileNode->mIsOpened   == ID_TRUE ) &&
        ( sFileNode->mIsModified == ID_TRUE ))
    {
        // �ٸ� ���� sync�� ���� ���� modified�� ���� �����Ѵ�.
        sFileNode->mIsModified = ID_FALSE;
        sFileNode->mIOCount++;
        sPreparedIO = ID_TRUE;

        sIsLocked = ID_FALSE;
        sddDataFile::unlockFileNode( sFileNode );
        

        IDE_TEST( sddDataFile::sync( sFileNode,
                                     smLayerCallback::setEmergency ) != IDE_SUCCESS );

        // �ݵ�� DiskMgr�� completeIO �Ͽ��� Victim List�� ���� �ȴ�.
        sPreparedIO = ID_FALSE;
        completeIO( aStatistics,
                    sFileNode,
                    SDD_IO_READ );
    }
    else
    {
        sIsLocked = ID_FALSE;
        sddDataFile::unlockFileNode( sFileNode );        
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLocked == ID_TRUE )
    {
        sddDataFile::unlockFileNode( sFileNode ); 
    }

    if ( sPreparedIO == ID_TRUE )
    {
        completeIO( aStatistics,
                    sFileNode,
                    SDD_IO_READ );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ���̺����̽��� ����Ÿ���� ��Ÿ����� üũ����Ʈ������ �����Ѵ�.
 * �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.
 *
 *   aSpaceNode - [IN] Sync�� TBS Node
 *   aActionArg - [IN] NULL
 **********************************************************************/
IDE_RC sddDiskMgr::doActUpdateAllDBFHdrInChkpt(
                                        idvSQL            * aStatistics,
                                        sctTableSpaceNode * aSpaceNode,
                                        void              * aActionArg )
{
    sddTableSpaceNode*  sSpaceNode;
    sddDataFileNode*    sFileNode;
    smLSN               sInitLSN;
    UInt                i;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg == NULL );
    ACP_UNUSED( aActionArg );

    SM_LSN_INIT( sInitLSN );

    while ( 1 )
    {
        if ( (sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) != ID_TRUE) ||
             (sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) == ID_TRUE) )
        {
            // ��ũ���̺����̽��� �ƴѰ�� �������� ����.
            // �ӽ� ���̺����̽��� ��� �������� ����.
            break;
        }

        if ( sctTableSpaceMgr::hasState( aSpaceNode,
                                         SCT_SS_SKIP_UPDATE_DBFHDR )
             == ID_TRUE )
        {
            // �޸����̺����̽��� DROPPED/DISCARDED ���
            // �������� ����.
            break;
        }

        sSpaceNode = (sddTableSpaceNode*)aSpaceNode;

        for (i=0; i < sSpaceNode->mNewFileID ; i++ )
        {
            sFileNode = sSpaceNode->mFileNodeArr[i] ;

            if( sFileNode == NULL )
            {
                continue;
            }

            // PRJ-1548 User Memory Tablespace
            // Ʈ����� Pending ���꿡�� DBF Node�� DROPPED�� ������ �Ŀ�
            // TBS List Latch�� �����ϸ�,
            // DBF Sync���� DBF Node�� ���¸� Ȯ���Ͽ� sync�� �����ϵ���
            // ó���� �� �ִ�.
            if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                continue;
            }

            // sync�� dbf ��忡 checkpoint ���� ����
            // DiskCreateLSN�� ������ �ʿ����.
            sddDataFile::setDBFHdr(
                            &(sFileNode->mDBFileHdr),
                            sctTableSpaceMgr::getDiskRedoLSN(),
                            NULL ,  // DiskCreateLSN
                            &sInitLSN,
                            NULL ); //DataFileDescSlotID

            IDE_ASSERT( sddDataFile::checkValuesOfDBFHdr(
                                     &(sFileNode->mDBFileHdr) )
                        == IDE_SUCCESS );

            IDE_ASSERT( writeDBFHdr( aStatistics,
                                     sFileNode ) == IDE_SUCCESS );
        }

        break;
    }

    return IDE_SUCCESS;
}


/*
  ��ũ ���̺����̽� ������ ��� Dirty�� ����Ÿ���� ������ Sync
  �ϰų�, üũ����Ʈ �����߿� ��ũ ����Ÿ������ ��Ÿ����� üũ����Ʈ
  ������ �����Ѵ�.

  [IN] aSyncType : ����̰ų� üũ����Ʈ���꿡 ���� SyncŸ��

*/
IDE_RC sddDiskMgr::syncAllTBS( idvSQL  *   aStatistics,
                               sddSyncType aSyncType )
{
    if ( aSyncType == SDD_SYNC_NORMAL)
    {
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      aStatistics,
                      sddTableSpace::doActSyncTBSInNormal,
                      NULL, /* Action Argument*/
                      SCT_ACT_MODE_LATCH ) != IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT( aSyncType == SDD_SYNC_CHKPT );

        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      aStatistics,
                      doActUpdateAllDBFHdrInChkpt,
                      NULL, /* Action Argument*/
                      SCT_ACT_MODE_LATCH ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ���������ÿ� ��� ��ũ ���̺����̽��� DBFile����
  �̵�� ���� �ʿ� ���θ� üũ�Ѵ�.
*/
IDE_RC sddDiskMgr::identifyDBFilesOfAllTBS( idvSQL *    aStatistics,
                                            idBool      aIsOnCheckPoint )
{
    sctActIdentifyDBArgs sIdentifyDBArgs;

    sIdentifyDBArgs.mIsValidDBF  = ID_TRUE;
    sIdentifyDBArgs.mIsFileExist = ID_TRUE;

    if ( aIsOnCheckPoint == ID_FALSE )
    {
        // ����������
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      aStatistics,
                      doActIdentifyAllDBFiles,
                      &sIdentifyDBArgs, /* Action Argument*/
                      SCT_ACT_MODE_NONE ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIdentifyDBArgs.mIsFileExist != ID_TRUE,
                        err_file_not_found );

        IDE_TEST_RAISE( sIdentifyDBArgs.mIsValidDBF != ID_TRUE,
                        err_need_media_recovery );
    }
    else
    {
        // üũ����Ʈ�������� ��Ÿ������� ���Ͽ�
        // ����� ����� �Ǿ����� ����Ÿ������ �ٽ� �о
        // �����Ѵ�.
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                      aStatistics,
                      doActIdentifyAllDBFiles,
                      &sIdentifyDBArgs, /* Action Argument*/
                      SCT_ACT_MODE_LATCH ) != IDE_SUCCESS );

        IDE_ASSERT( sIdentifyDBArgs.mIsValidDBF  == ID_TRUE );
        IDE_ASSERT( sIdentifyDBArgs.mIsFileExist == ID_TRUE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistFile ) );
    }
    IDE_EXCEPTION( err_need_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : datafile ���� (��� ���� ����) �Ǵ� �α׾�Ŀ��
 *               ���� datafile ��� ����
 *
 * - NOMAL ��� : �ϳ��� datafile�� create, open ��, tablespace �����
 * datafile ����Ʈ�� �߰��Ѵ�. ��, datafile�� create�� ���� ȭ����
 * �������� ���� ���̴�.
 * - FILE ��� : �ϳ��� datafile ��带 tablespace ��忡 �߰��ϰ�,
 * datafile�� ���� create������ �ʴ´�. �α׾�Ŀ�� ����� datafile ���
 *  ������ �ʱ�ȭ�Ҷ� ȣ��Ǹ�, datafile ��� �߰��Ŀ� file�� �������� �ʴ´�.
 *
 * + 2nd. code design
 *   - �ӽ����� dummy tablespace ��带 �����ϰ� �ʱ�ȭ�Ѵ�.
 *   - dummy tablespace ��忡 datafile���� �����ϰ� ����Ʈ�� �����Ѵ�.
 *     -> sddTableSpace::createDataFiles
 *   - ��ũ�������� mutex ȹ���Ѵ�.
 *   - HASH���� �ش� tablespace ��带 ã�´�.
 *   - dummy tablespace ����� datafile ��� ����Ʈ��
 *     ���� tablespace ����� datafile ��� ����Ʈ�� �߰��Ѵ�.
 *     ->sddTableSpace::addDataFileNode
 *   - if ( aMode != SMI_EACH_BYMODE )
 *     {
 *        �α׾�Ŀ FLUSH
 *     }
 *   - ��ũ�������� mutex �����Ѵ�.
 **********************************************************************/
IDE_RC sddDiskMgr::createDataFiles( idvSQL           * aStatistics,
                                    void             * aTrans,
                                    scSpaceID          aTableSpaceID,
                                    smiDataFileAttr ** aDataFileAttr,
                                    UInt               aDataFileAttrCount,
                                    smiTouchMode       aTouchMode )
{
    UInt                sState              = 0;
    UInt                sChkptBlockState    = 0;
    UInt                sTBSState;
    smiTableSpaceAttr   sDummySpaceAttr;
    sddTableSpaceNode   sDummySpaceNode;
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    UInt                i;
    sctPendingOp      * sPendingOp;
    sdFileID            sNewFileIDStart;
    sdFileID            sNewFileIDEnd;
    idBool              sNameExist      = ID_FALSE;
    SChar             * sExistFileName  = NULL;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aDataFileAttr != NULL );
    IDE_DASSERT( (aTouchMode == SMI_ALL_NOTOUCH) ||
                 (aTouchMode == SMI_EACH_BYMODE) );

    sTBSState  = 0;

    // fix BUG-16467 for avoid deadlock MutexForTBS->mMutex
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sTBSState = 1;

    /* ===================================
     * [1] dummy tablespace ���� �� �ʱ�ȭ
     * =================================== */

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    /*
     * BUG-18044
     *
     * ������ ���������ϸ��� ������ disk tbs�� add�ϴ°���
     * �����ϴ� ����
     */
    IDE_TEST( validateDataFileName( sSpaceNode,
                                    aDataFileAttr,
                                    aDataFileAttrCount,
                                    &sExistFileName,//�����޽��� ��¿�
                                    &sNameExist ) != IDE_SUCCESS);

    IDE_TEST_RAISE( sNameExist == ID_TRUE,
                    error_the_filename_is_in_use );

    // PRJ-1149.
    IDE_TEST_RAISE( SMI_TBS_IS_BACKUP(sSpaceNode->mHeader.mState),
                   error_forbidden_op_while_backup);

    // ���ο� ���ϵ��� ����� �ִ��� üũ�Ѵ�.
    if((sSpaceNode->mNewFileID + aDataFileAttrCount) >= SD_MAX_FID_COUNT )
    {
        IDE_RAISE( error_can_not_add_datafile_node );
    }

    sNewFileIDStart = sSpaceNode->mNewFileID ;
    sSpaceNode->mNewFileID += aDataFileAttrCount;
    sNewFileIDEnd   = sSpaceNode->mNewFileID;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    idlOS::memset(&sDummySpaceAttr, 0x00, ID_SIZEOF(smiTableSpaceAttr));
    idlOS::memset(&sDummySpaceNode, 0x00, ID_SIZEOF(sddTableSpaceNode));
    sDummySpaceAttr.mID = aTableSpaceID;
    sDummySpaceAttr.mAttrType = SMI_TBS_ATTR;

    IDE_TEST( sddTableSpace::initialize(&sDummySpaceNode,
                                        &sDummySpaceAttr) != IDE_SUCCESS );

    sDummySpaceNode.mNewFileID = sNewFileIDStart ;

    // PROJ-2133 incremental backup
    // DataFileDescSlot�� �Ҵ�Ǿ� checkpoint�� �߻��ϸ� CTBody�� flush�Ǿ�
    // ���Ͽ� �ݿ��ȴ�. ������ loganchor�� DataFileDescSlotID�� ����Ǳ� ����
    // ������ �״´ٸ� �Ҵ�� DataFileDescSlot�� ����Ҽ� ���Եȴ�.
    //
    // ��, logAnchor�� DataFileDescSlotID�� ����Ǳ� ������ CTBody�� flush �Ǹ�
    // �ȵȴ�.
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        IDE_TEST( smLayerCallback::blockCheckpoint() != IDE_SUCCESS ); 
        sChkptBlockState = 1; 
    }

    /* ===================================
     * [2] ������ ����Ÿ ȭ�� ����
     * =================================== */
    IDE_TEST( sddTableSpace::createDataFiles( aStatistics,
                                              aTrans,
                                              &sDummySpaceNode,
                                              aDataFileAttr,
                                              aDataFileAttrCount,
                                              aTouchMode,
                                              mMaxDataFilePageCount )
              != IDE_SUCCESS );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     sSpaceNode );
    sState = 1;
    /* ====================================================================
     * [3] ����Ÿ ȭ�ϵ��� �ڽ��� ���� ���̺� �����̽��� ȭ�� ����Ʈ�� ���
     * ==================================================================== */

    // fix BUG-16116 [PROJ-1548] sddDiskMgr::createDataFiles()
    // ������ DBF Node�� ���� Memory ����
    // ���� For loop �ȿ��� EXCEPTION�� �߻��ϰ� �Ǹ� DBF Node
    // �޸� ������ ���� ������ �����Ƿ� exception �߻��ڵ��
    // �ڵ����� �ʴ´�.
    // �� ���Ŀ� Exception�� �߻��Ѵٸ�, Ʈ����� rollback�� ���ؼ�
    // �ڵ����� ������ ���̴�.

    for (i = sNewFileIDStart ; i < sNewFileIDEnd ; i++ )
    {
        sFileNode = sDummySpaceNode.mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);

        sFileNode->mState |= SMI_FILE_CREATING;
        sFileNode->mState |= SMI_FILE_ONLINE;

        sddTableSpace::addDataFileNode( sSpaceNode, sFileNode );
    }

    for ( i = sNewFileIDStart ; i < sNewFileIDEnd ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        IDE_ASSERT( sFileNode != NULL);

        IDE_TEST( sddDataFile::addPendingOperation(
                    aTrans,
                    sFileNode,
                    ID_TRUE, /* commit�ÿ� ���� */
                    SCT_POP_CREATE_DBF,
                    &sPendingOp )
                != IDE_SUCCESS );

        sPendingOp->mTouchMode = aTouchMode;

        /*
          [4] �α� ��Ŀ ����
              �ý��� ������ �α� ��Ŀ�� ������ �̿��Ͽ�
              ����Ÿ ȭ�� ����Ʈ�� �����ϸ�,
              �� ��쿡�� �α� ��Ŀ�� �纯������ �ʴ´�.
        */
        if ( aTouchMode != SMI_ALL_NOTOUCH )
        {
            IDU_FIT_POINT( "1.PROJ-1552@sddDiskMgr::createDataFiles" );

            IDE_ASSERT( smLayerCallback::addDBFNodeAndFlush( sSpaceNode,
                                                             sFileNode )
                        == IDE_SUCCESS );

            IDU_FIT_POINT( "2.PROJ-1552@sddDiskMgr::createDataFiles" );
        }
        else
        {
            // nothing to do...
        }
    }

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    //PROJ-2133 incremental backup
    if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
    {
        sChkptBlockState = 0; 
        IDE_TEST( smLayerCallback::unblockCheckpoint() != IDE_SUCCESS ); 
    }

    sTBSState = 0;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::destroy( &sDummySpaceNode ) != IDE_SUCCESS );

    IDU_FIT_POINT( "1.BUG-31608@sddDiskMgr::createDataFiles" );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_forbidden_op_while_backup);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_forbiddenOpWhileBackup,
                                "ADD DATAFILE"));
    }

    IDE_EXCEPTION( error_can_not_add_datafile_node );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CANNOT_ADD_DataFile ));
    }

    IDE_EXCEPTION( error_the_filename_is_in_use )
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_UseFileInTheTBS,
                                 sExistFileName,
                                 ((sctTableSpaceNode *)sSpaceNode)->mName));
    }

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    if( sChkptBlockState != 0 )
    {
        IDE_ASSERT( smLayerCallback::unblockCheckpoint() 
                    == IDE_SUCCESS ); 
    }

    if ( sTBSState != 0 )
    {
        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS()
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α׾�Ŀ�κ��� datafile ��� ����
 **********************************************************************/
IDE_RC sddDiskMgr::loadDataFileNode( idvSQL          * aStatistics,
                                     smiDataFileAttr * aDataFileAttr,
                                     UInt              aAnchorOffset  )
{

    UInt               sState = 0;
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_DASSERT( aDataFileAttr != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aDataFileAttr->mSpaceID )
                 == ID_TRUE );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aDataFileAttr->mSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    IDE_DASSERT( sSpaceNode->mHeader.mID == aDataFileAttr->mSpaceID );

    /* ===================================
     * [2] ����Ÿ ȭ�� ��� ����
     * =================================== */
    IDE_TEST( sddTableSpace::createDataFiles( aStatistics, // statistics
                                              NULL, // transaction
                                              sSpaceNode,
                                              &aDataFileAttr,
                                              1,
                                              SMI_ALL_NOTOUCH,
                                              mMaxDataFilePageCount )
              != IDE_SUCCESS );

    // �Ҵ�� DBF ��带 �˻��Ѵ�.
    IDE_TEST( sddTableSpace::getDataFileNodeByID(
                                sSpaceNode,
                                aDataFileAttr->mID,
                                &sFileNode ) != IDE_SUCCESS );

    // Loganchor �������� �����Ѵ�.
    sFileNode->mAnchorOffset = aAnchorOffset;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;


    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : datafile ���� �� LRU ����Ʈ�� datafile ��� �߰�
 *
 * �ش� datafile ����� file�� open�Ѵ�.
 * Open FileNode Count �� ���� �� �� ����
 * �ٸ� ���µ� FileNode�� Close �ϰ� ��õ��Ѵ�.
 *
 **********************************************************************/
IDE_RC sddDiskMgr::openDataFile( idvSQL          * aStatistics,
                                 sddDataFileNode * aDataFileNode )
{
    UInt    sLockList = 0;
    UInt    sLockNode = 0;
    UInt    sOpenFileCountInc = 0;
    idBool  sIsDirectIO = isDirectIO() ;

    if ( sIsDirectIO == ID_TRUE )
    {
        /* BUG-47398 AIX ���� iduMemMgr::free4malign() �Լ����� ������ ����
         * Temp Table�� DirectIO�� ������� �ʴ´�. */
        if ( sctTableSpaceMgr::isTempTableSpace( aDataFileNode->mSpaceID ) == ID_TRUE )
        {
            sIsDirectIO = ID_FALSE;
        }
    }
    
    while( aDataFileNode->mIsOpened == ID_FALSE )
    {
        // Open File Count�� Property���� �Ѿ���� Ȯ���ϱ� ���� Mutex�� ��´�.
        // Count�� ���̴� �������� Lock�� ���� �ʴ´�.
        lockIncOpenFileCount( aStatistics );
        sLockList = 1;

        if ( mOpenFileCount < smuProperty::getMaxOpenDataFileCount() )
        {
            // ���� count�� �ø���.
            // Max �� ������ Lock�ȿ��� �ؾ� �Ѵ�.
            // Dec ������ Atomic �ȿ��� �ؾ� �Ѵ�.
            idCore::acpAtomicInc32( &mOpenFileCount );
            sOpenFileCountInc = 1;

            sLockList = 0;
            unlockIncOpenFileCount();

            sddDataFile::lockFileNode( aStatistics,
                                       aDataFileNode );
            sLockNode = 1;

            // Open�� ���ÿ� ��� �� �� �ִ�. File Node Mute�� ��ȣ�Ѵ�.
            // Lock�� ��� Ȯ���غ���.
            if ( aDataFileNode->mIsOpened == ID_FALSE )
            {
                IDE_TEST( sddDataFile::open( aDataFileNode,
                                             sIsDirectIO ) != IDE_SUCCESS );
                sOpenFileCountInc = 0;

                sLockNode = 0 ;
                sddDataFile::unlockFileNode( aDataFileNode );
            }
            else
            {
                sLockNode = 0 ;
                sddDataFile::unlockFileNode( aDataFileNode );

                sOpenFileCountInc = 0;
                idCore::acpAtomicDec32( &mOpenFileCount );
            }
        }
        else
        {
            sLockList = 0;
            unlockIncOpenFileCount( );

            // ������ �پ��ų� aDataFile �� �ٸ��̰� open������ ��ȯ
            IDE_TEST( closeDataFileInList( aStatistics,
                                           aDataFileNode ) != IDE_SUCCESS );
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sLockList == 1 )
    {
        unlockIncOpenFileCount();
    }

    if( sLockNode == 1 )
    {
        sddDataFile::unlockFileNode( aDataFileNode );
    }

    if( sOpenFileCountInc == 1 )
    {
        idCore::acpAtomicDec32( &mOpenFileCount );        
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile �ݱ� �� LRU ����Ʈ�κ��� datafile ��� ����
 *               Lock�� ��´�.
 **********************************************************************/
IDE_RC sddDiskMgr::closeDataFile( idvSQL          * aStatistics,
                                  sddDataFileNode * aDataFileNode )
{
    UInt sNodeLock = 0;

    IDE_DASSERT( aDataFileNode != NULL );

    sddDataFile::lockFileNode( aStatistics,
                               aDataFileNode );
    sNodeLock = 1;

    IDE_TEST( closeDataFileInternal( aStatistics,
                                     aDataFileNode) != IDE_SUCCESS );

    sNodeLock = 0 ;
    sddDataFile::unlockFileNode( aDataFileNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sNodeLock == 1 )
    {
        sddDataFile::unlockFileNode( aDataFileNode );   
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile �ݱ� �� LRU ����Ʈ�κ��� datafile ��� ����
 *               File Lock�� ���� ���·� ȣ��ȴ�.
 **********************************************************************/
IDE_RC sddDiskMgr::closeDataFileInternal( idvSQL          * aStatistics,
                                          sddDataFileNode * aDataFileNode )
{
    if (( aDataFileNode->mIsOpened == ID_TRUE ) &&
        ( aDataFileNode->mIOCount == 0 ))
    {
        IDE_TEST( sddDataFile::close( aDataFileNode ) != IDE_SUCCESS );

        // OpenFileCount�� �����ϱ� ���ؼ��� Lock�� ��ƾ� ������
        // ���̴µ��� Lock�� ���� �ʴ´�.
        idCore::acpAtomicDec32( &mOpenFileCount );

        removeNodeList( aStatistics,
                        aDataFileNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * Description : ��忡 ���� datafile ���� ( ��� ���� ���� )
 *
 * DDL�� alter tablespace add datafile �Ǵ� drop datafile�� ������ ������
 * �� ȣ��ȴ�.
 *
 * - SMI_ALL_NOTOUCH ��� : �ش� datafile ��常 �����Ѵ�.
 * - SMI_ALL_TOUCH ��� : �ش� datafile ��� �� ���� datafile�� �����Ѵ�.
 *
 * + 2nd. code design
 *   - ��ũ������ mutex ȹ��
 *   - HASH���� �ش� tablespace ��带 ã�´�. ��ã�� ��� ������ �����Ѵ�.
 *
 *   - tablespace ����� datafile ����Ʈ���� �ش� datafile�� �����Ѵ�.
 *   - �α׾�Ŀ FLUSH
 *   - ��ũ�������� mutex �����Ѵ�.
 **********************************************************************/
IDE_RC sddDiskMgr::removeDataFileFEBT( idvSQL             * aStatistics,
                                       void               * aTrans,
                                       sddTableSpaceNode  * aSpaceNode,
                                       sddDataFileNode    * aFileNode,
                                       smiTouchMode         aTouchMode )
{
    UInt        sState  = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTouchMode != SMI_EACH_BYMODE );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    IDU_FIT_POINT( "1.BUG-31608@sddDiskMgr::removeDataFileFEBT" );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );
    sState = 1;

    IDE_TEST_RAISE( SMI_TBS_IS_BACKUP(aSpaceNode->mHeader.mState),
                   error_forbidden_op_while_backup);

    // PROJ-1548
    // ����Ÿ���� ��忡 ���� DROP ������ TBS Node (X) -> TBS META PAGE�� (S)
    // ����� ȹ���ϱ� ������ DROP DBF ����� ���ÿ� ����� �� ����.
    // ������ ���� ������ üũ�غ��ƾ� �Ѵ�.
    // DBF Node�� DROPPED ���¶�� Exception ó���Ѵ�.
    IDE_TEST_RAISE( SMI_FILE_STATE_IS_DROPPED( aFileNode->mState ),
                    error_not_found_datafile_node );

    /* ==================================================
     * ������ ����Ÿ ȭ�� ����
     * removeDataFile�� on/offline�� ������� usedpagelimit��
     * ������ ����� ���� ��忡���� �����ؾ��Ѵ�.
     * ================================================== */
    IDE_TEST( sddTableSpace::removeDataFile(aStatistics,
                                            aTrans,
                                            aSpaceNode,
                                            aFileNode,
                                            aTouchMode,
                                            ID_TRUE /* aDoGhostMark */)
              != IDE_SUCCESS );

    IDE_ASSERT( sddTableSpace::getTotalPageCount(aSpaceNode) > 0 );

    // PROJ-1548
    // DROP DBF �߰����� Loganchor�� �÷������� �ʴ´�.

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_datafile_node );
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_NotFoundDataFileNode,
                                 aFileNode->mName ));
    }
    IDE_EXCEPTION(error_forbidden_op_while_backup);
    {
        IDE_SET(ideSetErrorCode( smERR_ABORT_forbiddenOpWhileBackup,
                                 "DROP DATAFILE" ));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;

}
/******************************************************************************
 * PRJ-1548 User Memory Tablespace
 *
 * DROP DBF �� ���� Pending ������ �����Ѵ�.
 *
 * [IN] aSpaceNode : TBS Node
 * [IN] aPendingOp : Pending ����ü
 *****************************************************************************/
IDE_RC sddDiskMgr::removeFilePending(
                            idvSQL            * aStatistics,
                            sctTableSpaceNode * aSpaceNode,
                            sctPendingOp      * aPendingOp  )
{
    UInt                    sState  = 0;
    sddDataFileNode       * sFileNode;
    scPageID                sFstPID;
    scPageID                sLstPID;
    smiDataFileDescSlotID   sDataFileDescSlotID;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aPendingOp != NULL );
    IDE_ASSERT( aPendingOp->mPendingOpParam != NULL );

    sFileNode  = (sddDataFileNode *)aPendingOp->mPendingOpParam;

    /* PRJ-1548 User Memory Tablespace
     * DBF Node�� ���°� ���ϻ������� loganchor�� ���ݿ��Ǵ� ���� ������ �����Ѵ�.
     *
     * " loganchor�� DROPPED ������ ���� �ݵ�� ������ �������� �ʴ´� "
     *
     * RESTART RECOVERY�� �����ϴ� ��쿡 COMMIT PENDING ������ �����ؼ�
     * ONLINE | DROPPING �� �ִ� �Ϸ�� Ʈ������� �־��� ��쿡 -> DROPPED��
     * ó�����־�� �ϸ�, ���ϵ� �����Ѵٸ� �Բ� �����ؾ��Ѵ�.
     * ������, DROPPED ���´� loganchor�� ������� �ʱ⶧����
     * ���������� �ʱ�ȭ���� ������, �̿� ���õ� �α׵��� ��� ���õ�����,
     * DROP ���꿡 ���� pending������ �����ϱ� ���� pending ���� ó���� �Ͽ���
     * �Ѵ�. */

    /* DROP TBS Node�ÿ� TBS Node�� X ����� ȹ���ϱ� ������ ����
     * ������ �Ҽ� ���� �����̰�, �ش� ���ϵ��� close�ϰ� buffer �󿡼� ���� page��
     * ��� ��ȿȭ��Ű�� ������ ������ open�Ǿ� ���� ���� ������,
     * sync checkpoint �߻����� ���� open�� �Ǿ� ���� �� �ִ� */

    /* fix BUG-13125 DROP TABLESPACE�ÿ� ���� ���������� Invalid���Ѿ� �Ѵ�.
     * ���ۻ� ����� ���� �������� ��ȿȭ ��Ų��. */
    IDE_TEST( getPageRangeInFileByID( aStatistics,
                                      sFileNode->mSpaceID,
                                      sFileNode->mID,
                                      &sFstPID,
                                      &sLstPID ) != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::discardPagesInRangeInSBuffer(
                  aStatistics,
                  sFileNode->mSpaceID,
                  sFstPID,
                  sLstPID )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::discardPagesInRange(
                  aStatistics,
                  sFileNode->mSpaceID,
                  sFstPID,
                  sLstPID )
              != IDE_SUCCESS );

    /* BUG-24129: Pending�������� File���� ������ �Ϸ��� Buffer Flusher�� IOB
     *            �� ����� ������ File�� Page�� ������ ��� �߻�
     *
     * File ���� Pending����� BufferPool�� �ִ� File�� ���õ� ��� ��������
     * Buffer���� ���������� Flusher�� IOB�� �̹� ����� BCB�� �������� ���Ѵ�.
     * �Ͽ� BufferPool���� File�� ���õ� ��� �������� ������ Flusher�� �����
     * BCB�� ���������� �𸣱⶧���� Flusher�� �۾����� Flush�۾��� �ѹ� ����
     * �ɶ����� ���⼭ ����Ѵ�. */
    smLayerCallback::wait4AllFlusher2Do1JobDoneInSBuffer();

    smLayerCallback::wait4AllFlusher2Do1JobDone();

retry:
    // Dead Lock ������ ���� SpaceNode -> FileNode ������ Mutex�� ��ƾ� �Ѵ�.
    // FileNode->mState�� �����ϱ� ���� SpaceNodeMutex�� ��ƾ� �ϴµ�
    // FileNode Mutex�� ��� ���� �̸� ��´�.
    sctTableSpaceMgr::lockSpaceNode( aStatistics ,
                                     aSpaceNode );
    sState = 1;

    sddDataFile::lockFileNode( aStatistics,
                               sFileNode );
    sState = 2;

    if ( sFileNode->mIOCount != 0 )
    {
        sState = 1;
        sddDataFile::unlockFileNode( sFileNode );

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

        /* checkpoint sync �������̹Ƿ� ����ϴ� ��õ� �Ѵ�. */
        idlOS::sleep(1);

        goto retry;
    }
    else
    {
        /* DROP commit���� ���꿡 ���ؼ� ������ �� �ִ� ��Ȳ��
         * checkpoint sync �������θ� ������ ���µɼ� �ִ�.
         * checkpoint sync �������� �ƴϹǷ� CLOSE �Ѵ�. */
    }

    IDE_TEST( closeDataFileInternal( aStatistics,
                                     sFileNode ) != IDE_SUCCESS );

    sFileNode->mState = SMI_FILE_DROPPED;

    sState = 1;
    sddDataFile::unlockFileNode( sFileNode );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    /* BUG-24086: [SD] Restart�ÿ��� File�̳� TBS�� ���� ���°� �ٲ���� ���
     * LogAnchor�� ���¸� �ݿ��ؾ� �Ѵ�.
     *
     * Restart Recovery�ÿ��� updateDBFNodeAndFlush���� �ʴ����� �ϵ��� ����.
     * */
    if ( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET )
    {
        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );
    }

    lockGlobalPageCountCheckMutex( aStatistics );
    sState = 3;

    if ( aPendingOp->mTouchMode == SMI_ALL_TOUCH )
    {
        if( idf::unlink((SChar *)(sFileNode->mName)) != 0 )
        {
            ideLog::log(SM_TRC_LOG_LEVEL_TRANS,
                        SM_TRC_TRANS_DBFILE_UNLINK_ERROR,
                        (SChar *)(sFileNode->mName), errno);
        }
        else
        {
            //======================================================
            // To Fix BUG-13924
            //======================================================
            ideLog::log(SM_TRC_LOG_LEVEL_TRANS,
                        SM_TRC_TRANS_DBFILE_UNLINK,
                        (SChar*)sFileNode->mName );
        }
    }
    else // SMI_ALL_NOTOUCH, SMI_EACH_BYMODE
    {
        if( aPendingOp->mTouchMode == SMI_EACH_BYMODE )
        {
            if( sFileNode->mCreateMode == SMI_DATAFILE_CREATE )
            {
                if( idf::unlink((SChar *)(sFileNode->mName)) != 0 )
                {
                    ideLog::log(SM_TRC_LOG_LEVEL_TRANS,
                                SM_TRC_TRANS_DBFILE_UNLINK_ERROR,
                                (SChar *)(sFileNode->mName), errno);
                }
                else
                {
                    //======================================================
                    // To Fix BUG-13924
                    //======================================================
                    ideLog::log(SM_TRC_LOG_LEVEL_TRANS,
                                SM_TRC_TRANS_DBFILE_UNLINK,
                                (SChar*)sFileNode->mName );
                }
            }
            else //SMI_DATAFILE_REUSE
            {
                /* nothing to do */
            }
        }
        else // SMI_ALL_NOTOUCH
        {
            /* nothing to do */
        }
    }

    /* PROJ-2133 incremental backup */
    if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE) &&
         ( sctTableSpaceMgr::isTempTableSpace( sFileNode->mSpaceID ) != ID_TRUE ) )
    {
        if( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET )
        {
            /* logAnchor�� DataFileDescSlotID�� ����� ��Ȳ ó�� */
            IDE_TEST( smLayerCallback::getDataFileDescSlotIDFromLogAncho4DBF(
                            sFileNode->mAnchorOffset,
                            &sDataFileDescSlotID )
                      != IDE_SUCCESS );

            /* DataFileDescSlot�� �̹� �Ҵ����� �Ȱ�� */
            IDE_TEST_CONT( smriChangeTrackingMgr::isAllocDataFileDescSlotID( 
                            &sDataFileDescSlotID ) != ID_TRUE,
                      already_dealloc_datafiledesc_slot );
 
            /* DataFileDescSlot�� �̹� �Ҵ����� �Ǿ����� loganchor����
             * DataFileDescSlotID�� �����ִ°�� Ȯ�� */
            IDE_TEST_CONT( smriChangeTrackingMgr::isValidDataFileDescSlot4Disk(
                            sFileNode, &sDataFileDescSlotID ) != ID_TRUE,
                            already_reuse_datafiledesc_slot); 
        }
        else
        {
            /* logAnchor�� DataFileDescSlotID�� ������� ������Ȳ ó��
             * ���� ���������ᰡ �ƴ� rollback�������� �ش�Ǵ� ��Ȳ�̴�. */

            /* DataFileDescSlot�� �̹� �Ҵ����� �Ȱ�� */
            IDE_TEST_CONT( smriChangeTrackingMgr::isAllocDataFileDescSlotID(
                                &sFileNode->mDBFileHdr.mDataFileDescSlotID )
                          != ID_TRUE,
                          already_dealloc_datafiledesc_slot );
            
            sDataFileDescSlotID.mBlockID = 
                            sFileNode->mDBFileHdr.mDataFileDescSlotID.mBlockID;
            sDataFileDescSlotID.mSlotIdx = 
                            sFileNode->mDBFileHdr.mDataFileDescSlotID.mSlotIdx;
        }

        IDE_TEST( smriChangeTrackingMgr::deleteDataFileFromCTFile( 
                        &sDataFileDescSlotID ) != IDE_SUCCESS );

        IDE_EXCEPTION_CONT( already_reuse_datafiledesc_slot );

        sFileNode->mDBFileHdr.mDataFileDescSlotID.mBlockID = 
                                            SMRI_CT_INVALID_BLOCK_ID;
        sFileNode->mDBFileHdr.mDataFileDescSlotID.mSlotIdx = 
                                            SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;

        if( sFileNode->mAnchorOffset != SCT_UNSAVED_ATTRIBUTE_OFFSET )
        {
            IDE_TEST( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                      != IDE_SUCCESS );    
        }
        else
        {
            /* nothing to do */
        }

        IDE_EXCEPTION_CONT( already_dealloc_datafiledesc_slot );
    }
    else
    {
        /* nothing to do */
    }

    sState = 0;
    unlockGlobalPageCountCheckMutex();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            unlockGlobalPageCountCheckMutex();
            break;
        case 2:
            sddDataFile::unlockFileNode( sFileNode );
        case 1:
            sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/******************************************************************************
 *  - PROJ-1671 Bitmap-based Tablespace And Segment Space Management
 *
 *  NextSize��ŭ �ø����ִ� ��ȿ�� ������ ã�´�.
 *  [IN ]
 *  [OUT]  sFileNode
 *****************************************************************************/
void sddDiskMgr::getExtendableSmallestFileNode( sddTableSpaceNode  * aSpaceNode,
                                                sddDataFileNode   ** aFileNode )
{

    UInt                i;
    UInt                sMinSize= ID_UINT_MAX;
    sddDataFileNode   * sMinSizeFileNode = NULL;
    sddDataFileNode   * sFileNodeTemp = NULL;
    UInt                sTempSize;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL );

    //XXX5 sort ���
    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNodeTemp = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNodeTemp == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNodeTemp->mState ) )
        {
            continue;
        }

        if(sFileNodeTemp->mIsAutoExtend == ID_TRUE)
        {
            /*
             * currũ�Ⱑ �������� ������ next�� ��������  max�� ���� �ʴ� ����
             * �� ã�´�.
             */
            sTempSize = sFileNodeTemp->mCurrSize ;

            if( sMinSize > sTempSize )
            {
                if( (sTempSize + sFileNodeTemp->mNextSize)
                    <= sFileNodeTemp->mMaxSize)
                {
                    sMinSizeFileNode = sFileNodeTemp;
                    sMinSize =sFileNodeTemp->mCurrSize;
                }

            }
        }

    }

    *aFileNode = sMinSizeFileNode;
}


/*
 *  - sdptb�� ���� extendDataFile�̴�.
 *
 *
 *  - PROJ-1671 Bitmap-based Tablespace And Segment Space Management
 */
IDE_RC sddDiskMgr::extendDataFileFEBT(
                         idvSQL              * aStatistics,
                         void                * aTrans,
                         scSpaceID             aTableSpaceID,
                         sddDataFileNode     * aFileNode )
{
    UInt                sTransStatus = 0;
    sddTableSpaceNode * sSpaceNode;
    UInt                sState       = 0;
    UInt                sPreparedIO  = 0;
    ULong               sExtendSize;
    smLSN               sLsnNTA;

    IDE_DASSERT( aFileNode != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
                  != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // DDL�� ��� TBS Node�� (X) ����� ȹ���ϱ� ������, DML�� ������ �� ����.
    // �������, ADD DATAFILE�� DROP DATAFILE, RESIZE DATAFILE�� ���� �����
    // �ڵ�Ȯ�忬���� ���ÿ� �߻��� �� ����.
    // ��, Backup ������ DML�� ���ÿ� �߻��� �� �ִ�.

    IDU_FIT_POINT( "1.PROJ-1548@sddDiskMgr::extendDataFileFEBT" );

    sctTableSpaceMgr::lockSpaceNode( aStatistics ,
                                     sSpaceNode );
    sState = 1;

    /* BUG-32218
     * wait4Backup() �� ������ ������ ����� ����������
     * ���� ��� */
    // fix BUG-11337.
    // ����Ÿ ������ ������̸� �Ϸ��Ҷ����� ��� �Ѵ�.
    while ( SMI_FILE_STATE_IS_BACKUP( aFileNode->mState ) )
    {
        sctTableSpaceMgr::wait4Backup( sSpaceNode );

        IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
    }

    /* Ȯ���ų file node�� �ݵ�� autoextend on ����̾�� �Ѵ�. */
    IDE_DASSERT(aFileNode->mIsAutoExtend == ID_TRUE);

    if ((aFileNode->mCurrSize + aFileNode->mNextSize)
        <= aFileNode->mMaxSize )
    {
        sExtendSize = aFileNode->mNextSize;
    }
    else
    {
        sExtendSize = aFileNode->mMaxSize - aFileNode->mCurrSize;
    }

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = 1;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    if (aTrans != NULL)
    {
        // PRJ-1548 User Memory Tablespace
        // ���� ������� DBF Node�� DROPPED �����ϼ� ����.
        IDE_ASSERT( SMI_FILE_STATE_IS_NOT_DROPPED( aFileNode->mState ) );

        sLsnNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );

        IDE_TEST( smLayerCallback::writeLogExtendDBF( aStatistics,
                                                      aTrans,
                                                      aTableSpaceID,
                                                      aFileNode,
                                                      aFileNode->mCurrSize + sExtendSize,
                                                      NULL )
                  != IDE_SUCCESS );

        sTransStatus = 1;

        // BUG-17465
        // autoextend mode�� �ٲ�� �α��� �ؾ� �Ѵ�.
        /*
         * BUG-22571 ������ ũ�Ⱑ Ŀ�������� ��Ȳ������ v$datafiles�� autoextend mode����
         *           on�� ��찡 �ֽ��ϴ�.
         */
        if ( (aFileNode->mCurrSize + sExtendSize + aFileNode->mNextSize) > 
             aFileNode->mMaxSize )
        {
            IDE_TEST( smLayerCallback::writeLogSetAutoExtDBF( aStatistics,
                                                              aTrans,
                                                              aTableSpaceID,
                                                              aFileNode,
                                                              ID_FALSE,
                                                              0,
                                                              0,
                                                              NULL )
                      != IDE_SUCCESS );
        }
        // !!CHECK RECOVERY POINT
        IDU_FIT_POINT( "1.PROJ-1552@sddDiskMgr::extendDataFileFEBT" );
    }

    IDE_TEST( sddDataFile::extend(aStatistics, aFileNode, sExtendSize)
              != IDE_SUCCESS );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     sSpaceNode );
    sState = 1;

    sddDataFile::setCurrSize(aFileNode, (aFileNode->mCurrSize + sExtendSize));

    if ( aFileNode->mCurrSize == aFileNode->mMaxSize )
    {
        sddDataFile::setAutoExtendProp(aFileNode, ID_FALSE, 0, 0);
    }

    /* ��� ���� �缳�� */
    sSpaceNode->mTotalPageCount += sExtendSize;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    sPreparedIO = 0;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_WRITE ) != IDE_SUCCESS );

    // PRJ-1548 User Memory Tablespace
    // �ڵ�Ȯ�� �����Ͽ��ٸ� NTA �α�� DBF Node�� ����� �����Ѵ�.

    IDU_FIT_POINT( "2.PROJ-1552@sddDiskMgr::extendDataFileFEBT" );

    IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( aFileNode )
                == IDE_SUCCESS );

    IDU_FIT_POINT( "3.PROJ-1552@sddDiskMgr::extendDataFileFEBT" );

    if ( aTrans != NULL )
    {
        IDE_TEST( smLayerCallback::writeNTAForExtendDBF( aStatistics,
                                                         aTrans,
                                                         &sLsnNTA )
                  != IDE_SUCCESS );

        // !!CHECK RECOVERY POINT
        IDU_FIT_POINT( "4.PROJ-1552@sddDiskMgr::extendDataFileFEBT" );
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sPreparedIO != 0 )
    {
        //  CompleteIO�� Lock�� ���� �ʾƵ� ��
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_WRITE ) == IDE_SUCCESS );
    }

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    if (sTransStatus != 0)
    {
        IDE_ASSERT( smLayerCallback::undoTrans4LayerCall(
                        aStatistics,
                        aTrans,
                        &sLsnNTA )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : tablespace�� datafile ũ�� resize
 *
 * datafile�� resize ���� ������ ���࿡ ���� datafile�� ũ�⸦ ����ϰų�
 * Ȯ���Ѵ�.
 *
 * next size�� extent�� ũ���� ����� �� ���̴�. �� �۾��� write�� ���������� �ٸ�
 * �����尡 �����Ҽ� ������ mutex�� ��´�. �׷��� ������ �������� �Ѳ����� ȭ����
 * Ȯ���Ϸ��� �õ��� �� ���� �ִ�.
 *
 * + 2nd. code design
 *   - ��ũ�������� mutex ȹ���Ѵ�.
 *   * while
 *   * ��ũ�������� HASH���� tablespace ��带 ã�´�.
 *   * PID�� �˸� ������ �������ִ�.
 *     if( datafile ��尡 �̹� ���ϴ� ũ�� �̻��̴�. )
 *     {
 *         ��ũ�������� mutex �����Ѵ�.
 *         return fail;
 *     }
 *     datafile ��尡 online�̸�, datafile ����� max�����
 *     ������ �ް�, offline�̸� mMaxDataFileSize�� ������ �޴´�.
 *     ���� current size �� ���Ѵ�.
 *     Ȯ�� size�� ���Ѵ�.
 *   - write I/O�� �غ��Ѵ�. -> prepareIO
 *   - SD_PAGE_SIZE�� null buffer�� �����Ѵ�.
 *   - datafile ũ�⸦ Ȯ��ũ�⸸ŭ ������ ���鼭 SD_PAGE_SIZE
 *     ������ �ø���. -> write
 *   - datafile ��带 sync�Ѵ�.
 *   - datafile ����� ���� �缳�� -> sddDataFile::setSize
 *   - tablespace ����� ũ�⸦ �缳��
 *   - �α׾�Ŀ FLUSH
 *   - I/O �۾��� �Ϸ��Ѵ�. -> completeIO
 *   - ��ũ�������� mutex �����Ѵ�.
 **********************************************************************/

IDE_RC sddDiskMgr::alterResizeFEBT(
                   idvSQL          * aStatistics,
                   void             * aTrans,
                   scSpaceID          aTableSpaceID,
                   SChar            * aDataFileName, /*�̹� valid��*/
                   scPageID           aHWM,
                   ULong              aSizeWanted,
                   sddDataFileNode  * aFileNode)
{

    UInt               sState = 0;
    ULong              sExtendSize;
    idBool             sExtendFlag;
    UInt               sPreparedIO;
    SLong              sResizePageSize;
    sctPendingOp      *sPendingOp;
    sddTableSpaceNode* sSpaceNode;

    //���� ������ ũ��� aFileNode->mCurrSize �� �˼��ִ�.

    IDE_DASSERT( aTrans        != NULL );
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aSizeWanted > 0 );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aFileNode != NULL);

    sPreparedIO     = 0;
    sResizePageSize = 0;

    IDU_FIT_POINT( "1.BUG-32218@sddDiskMgr::alterResizeFEBT" );

    
    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
              != IDE_SUCCESS );
    sState = 1;

    /* resize�� ���� ȭ���� ũ��� �䱸�ϴ� ũ�Ⱑ �ٸ� ��쿡�� ����ȴ�.*/
    if( aFileNode->mCurrSize != aSizeWanted )
    {
        /* BUG-32218
         * wait4Backup() �� ������ ������ ����� ����������
         * ���� ��� */
        // fix BUG-11337.
        // ����Ÿ ������ ������̸� �Ϸ��Ҷ����� ��� �Ѵ�.
        while ( SMI_FILE_STATE_IS_BACKUP( aFileNode->mState ) )
        {
            sctTableSpaceMgr::wait4Backup( sSpaceNode );

            IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );
        }

        // PRJ-1548 User Memory Tablespace
        // DBF Node�� ���� Resize�Ϸ��� TBS�� (X) ����� ȹ��Ǿ� �ִ�.
        // ����Ÿ���� ��忡 ���� DROP ������ TBS Node�� (X) ->TBS META PAGE�� (S)
        // ����� ȹ���ϱ� ������ RESIZE ����� ���ÿ� ����� �� ����.
        // ���� ������ üũ�غ��ƾ� �Ѵ�.
        // DBF Node�� DROPPED ���¶�� Exception ó���Ѵ�.
        IDE_TEST_RAISE( SMI_FILE_STATE_IS_DROPPED( aFileNode->mState ),
                        error_not_found_datafile_node );

        /*
         * ���� HWM���� �� ���� ũ����� ��Ҹ� ��û�Ѵٸ� ����ó���ؾ��Ѵ�.
         * �� �Լ��� �պκп��� ���� ó���� �ϱ� ������ ���⼭�� assert�Ѵ�.
         */
        IDE_ASSERT( aSizeWanted > SD_MAKE_FPID( aHWM ) );

        /* ==================================================
         * ȭ�� Ȯ���� ��� ���� üũ
         * ȭ�� Ȯ������ ���� ȭ���� �ִ� ũ�⿡ �����ϴ� ���
         * auto extend mode�� off�� ������
         * =================================================== */

        // BUG-17415 autoextend on�� ��쿡�� maxsize�� üũ�Ѵ�.
        if (aFileNode->mIsAutoExtend == ID_TRUE)
        {
            IDE_ASSERT( aFileNode->mMaxSize <= mMaxDataFilePageCount );

            // BUG-29566 ������ ������ ũ�⸦ 32G �� �ʰ��Ͽ� �����ص�
            //           ������ ������� �ʽ��ϴ�.
            // Max Size�� �̹� �����Ǿ� �ִ� ���̹Ƿ� Autoextend on������
            // Max Size���� �������� �˻��ϸ� �ȴ�.
            IDE_TEST_RAISE( aSizeWanted > aFileNode->mMaxSize,
                            error_invalid_extend_filesize_maxsize );
        }

        // ��û�� File Size�� Maximum File Size�� �Ѵ��� ���Ѵ�..
        // Autoextend off �ÿ��� ���� Max Size�� �����Ƿ� �������ؾ��Ѵ�.
        if( mMaxDataFilePageCount < SD_MAX_FPID_COUNT )
        {
            // Os Limit �� 32G������ ���
            IDE_TEST_RAISE( aSizeWanted > mMaxDataFilePageCount,
                            error_invalid_extend_filesize_oslimit );
        }
        else
        {
            IDE_TEST_RAISE( aSizeWanted > SD_MAX_FPID_COUNT,
                            error_invalid_extend_filesize_limit );
        }

        /* ------------------------------------------------
         * ���ϴ� page ������ datafile ����� currsize����
         * ū ���� Ȯ���� �ϰ�, �׷��� ���� ���� ��Ҹ�
         * �Ѵ�. ����� ��� init size�� �Բ� ���Ѵ�.
         * ----------------------------------------------*/
        /* t: Ȯ��, f: ��� */

        if (aSizeWanted > aFileNode->mCurrSize) //Ȯ��
        {
           sExtendSize = aSizeWanted - aFileNode->mCurrSize;
           sExtendFlag = ID_TRUE;
        }
        else                                    //���
        {
           sExtendSize = aFileNode->mCurrSize - aSizeWanted;
           sExtendFlag = ID_FALSE;
        }

        IDE_ASSERT( sExtendSize != 0 );

        /* ------------------------------------------------
         * ������ ���� datafile ũ�⸦ �����Ͽ� ��� �Ǵ�
         * Ȯ�� ó���Ѵ�.
         * ----------------------------------------------*/
        IDE_TEST( prepareIO( aStatistics,
                             aFileNode ) != IDE_SUCCESS );
        sPreparedIO = 1;
        
        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

        if (sExtendFlag == ID_TRUE) // Ȯ��
        {
            if ( aTrans != NULL )
            {
                IDE_TEST( smLayerCallback::writeLogExtendDBF(
                              aStatistics,
                              aTrans,
                              aTableSpaceID,
                              aFileNode,
                              aFileNode->mCurrSize + sExtendSize,
                              NULL ) != IDE_SUCCESS );

            }

            sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                             sSpaceNode );
            sState = 1;
            // ���� Resize �����ϴ� ���Ͽ� ���ؼ� CREATING ���¸�
            // Oring ���ְ� Pending���� ó���Ѵ�.
            aFileNode->mState |= SMI_FILE_RESIZING;

            sState = 0;
            sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

            sResizePageSize = sExtendSize;

            IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          aFileNode,
                          ID_TRUE, /* commit�ÿ� ���� */
                          SCT_POP_ALTER_DBF_RESIZE,
                          &sPendingOp ) != IDE_SUCCESS );

            sPendingOp->mResizePageSize = sResizePageSize;

            IDE_TEST( sddDataFile::extend(aStatistics, aFileNode, sExtendSize)
                      != IDE_SUCCESS );

            // BUG-17465
            // maxsize�� �����ϸ� autoextend mode�� on���� off�� �ٲ�µ�
            // �̿� ���� �α��ؾ� �Ѵ�.
            if ( (aFileNode->mIsAutoExtend == ID_TRUE) &&
                 ((aFileNode->mCurrSize + sExtendSize + aFileNode->mNextSize) > 
                     aFileNode->mMaxSize) )
            {
                IDE_TEST( smLayerCallback::writeLogSetAutoExtDBF(
                              aStatistics,
                              aTrans,
                              aTableSpaceID,
                              aFileNode,
                              ID_FALSE,
                              0,
                              0,
                              NULL ) != IDE_SUCCESS );

            }
        }
        else // ���
        {
            if ( aTrans != NULL )
            {
                IDE_TEST( smLayerCallback::writeLogShrinkDBF(
                                             aStatistics,
                                             aTrans,
                                             aTableSpaceID,
                                             aFileNode,
                                             aSizeWanted,
                                             aFileNode->mCurrSize - sExtendSize,
                                             NULL )
                          != IDE_SUCCESS );

            }

            sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                             sSpaceNode );
            sState = 1;
            // ���� Resize �����ϴ� ���Ͽ� ���ؼ� CREATING ���¸�
            // Oring ���ְ� Pending���� ó���Ѵ�.
            aFileNode->mState |= SMI_FILE_RESIZING;

            sState = 0;
            sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

            // ���� Resizing ������ ������ �����Ѵ�. (Runtime ����)
            sResizePageSize = -sExtendSize;

            IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          aFileNode,
                          ID_TRUE, /* commit�ÿ� ���� */
                          SCT_POP_ALTER_DBF_RESIZE,
                          &sPendingOp ) != IDE_SUCCESS );

            sPendingOp->mPendingOpFunc    = sddDiskMgr::shrinkFilePending;
            sPendingOp->mPendingOpParam   = (void*)aFileNode;
            sPendingOp->mResizePageSize   = sResizePageSize;
            sPendingOp->mResizeSizeWanted = aSizeWanted;
        }

        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        sState = 1;

        /* ��� ���� �缳�� */
        if (sExtendFlag == ID_TRUE) //Ȯ��
        {
            sddDataFile::setCurrSize(
                         aFileNode,
                         (aFileNode->mCurrSize + sExtendSize));

            if ( aFileNode->mIsAutoExtend == ID_TRUE )
            {
                if ( aFileNode->mCurrSize == aFileNode->mMaxSize )
                {
                    sddDataFile::setAutoExtendProp(aFileNode,
                                                   ID_FALSE,
                                                   0,
                                                   0);
                }
            }
        }
        else
        {
            // Shrink ������ Pending���� ó���ȴ�.
        }

        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

        sPreparedIO = 0;
        IDE_TEST( completeIO( aStatistics,
                              aFileNode,
                              SDD_IO_WRITE ) != IDE_SUCCESS );

        IDU_FIT_POINT( "3.PROJ-1552@sddDiskMgr::alterResizeFEBT" );

        /* loganchor flush */
        IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( aFileNode )
                    == IDE_SUCCESS );

        IDU_FIT_POINT( "4.PROJ-1552@sddDiskMgr::alterResizeFEBT" );
    }
    else
    {
        sState = 0;
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_datafile_node );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aDataFileName));
    }
    IDE_EXCEPTION( error_invalid_extend_filesize_limit );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_InvalidExtendFileSize,
                                 aSizeWanted,
                                 (ULong)SD_MAX_FPID_COUNT) );
    }
    IDE_EXCEPTION( error_invalid_extend_filesize_oslimit );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidExtendFileSizeOSLimit,
                                aSizeWanted,
                                (ULong)mMaxDataFilePageCount));
    }
    IDE_EXCEPTION( error_invalid_extend_filesize_maxsize );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidExtendFileSizeMaxSize,
                                aSizeWanted,
                                aFileNode->mMaxSize));
    }

    IDE_EXCEPTION_END;

    if ( sPreparedIO != 0 )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_WRITE ) == IDE_SUCCESS );
    }

    if (sState != 0)
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ���µ� datafile �߿��� ������� �ʴ� datafile�� close�Ѵ�.
 *               ������ �ʴ°����� �����Ǵ� FileNode�� VictimFileList�� �� �ְ�,
 *               �̸� Ž���Ͽ� ������ ������� �ʴ� Node�� close�Ѵ�.
 *
 * ������ ���� ��� �Ͽ��� �� FileNode�� ��� Open�Ͽ��ų�,
 * OpenFileCount�� Max Count ���Ϸ� �������� �������´�.
 *
 **********************************************************************/
IDE_RC sddDiskMgr::closeDataFileInList( idvSQL          * aStatistics,
                                        sddDataFileNode * aDataFileNode )
{
    UInt              sState = 1;
    sddDataFileNode*  sFileNode;
    smuList*          sNode;
    smuList*          sBaseNode;

    while( ( aDataFileNode->mIsOpened == ID_FALSE ) &&
           ( mOpenFileCount >= smuProperty::getMaxOpenDataFileCount() ))
    {
        if ( mVictimListNodeCount > 0 )
        {
            lockCloseVictimList( aStatistics );
            sState = 1;

            sBaseNode = &mVictimFileList;

            for ( sNode =  SMU_LIST_GET_LAST( sBaseNode );
                  sNode != sBaseNode;
                  sNode =  SMU_LIST_GET_PREV( sNode ) )
            {
                sFileNode = (sddDataFileNode*)sNode->mData;

                /* found */
                // IO Count�� 0�� �༮���� list������,
                // ���������� 0�� �ƴҼ��� �ִ�.
                if ( sFileNode->mIOCount == 0 )
                {
                    // List���� ������ �����Ѵ�.
                    // ���� closeDataFile���� ���� �� ���� �ְ� ���� �� ���� �ִ�.
                    // �����ߴٸ�, close�Ȱ�
                    // ���� �Ͽ��ٸ�,
                    // 1. �ٸ��̰� close�Ѱ��̴�.
                    // 2. ���� �ٽ� ��Ȱ�� �Ͽ���. IOCount >= 1
                    // ������̴� List���� �ٽ� ���� �� �ʿ䰡 ����.
                    // �̸� ������ ���� ������, ���� ������ ���� Close�Ϸ��� �õ��ϰ� �ȴ�.
                    SMU_LIST_DELETE( &sFileNode->mNode4LRUList );
                    sFileNode->mNode4LRUList.mNext = NULL;
                    sFileNode->mNode4LRUList.mPrev = NULL;

                    break;
                }
            }

            sState = 0;
            unlockCloseVictimList();

            if ( sNode != sBaseNode )
            {
                IDE_TEST( closeDataFile( aStatistics,
                                         sFileNode ) != IDE_SUCCESS );
                continue;
            }
        }

        idCore::acpAtomicInc32( &mWaitThr4Open );

        idlOS::sleep(0);

        idCore::acpAtomicDec32( &mWaitThr4Open );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unlockCloseVictimList();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : datafile ����� autoextend �Ӽ� ����
 *
 * fix BUG-15387
 * ����Ÿ���� �����°� NotUsed/Used/InUsed�� ���� �� �ִµ�
 * Used ���¸� �ƴ� ����Ÿ���Ͽ� ���ؼ��� ���� autoextend ��带
 * On/Off �� �� �ִ�.
 *
 * [ ���� ]
 *
 * [IN]  aTrans          - Ʈ����� ��ü
 * [IN]  aTableSpaceID   - �ش� Tablespace ID
 * [IN]  aDataFileName   - ����Ÿ���� ���
 * [IN]  aAutoExtendMode - �����ϰ����ϴ� Autoextend ��� ( On/Off )
 * [IN]  aHWM            - high water mark
 * [IN]  aNextSize       - Autoextend On �� Next Size  ( page size )
 * [IN]  aMaxSize        - Autoextend On �� Max Size   ( page size )
 * [OUT] aValidDataFileName - ����Ÿ���� ������

 **********************************************************************/
IDE_RC sddDiskMgr::alterAutoExtendFEBT(
                        idvSQL            * aStatistics,
                        void              * aTrans,
                        sddTableSpaceNode * aSpaceNode,
                        SChar             * aDataFileName,
                        sddDataFileNode   * aFileNode,
                        idBool              aAutoExtendMode,
                        ULong               aNextSize,
                        ULong               aMaxSize)
{
    UInt sState = 0;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );
    sState = 1;

    /*
     * �Ӽ� ������ ������ ��������� �˻��Ѵ�
     *
     *     1. autoextend on���� ������ ��� maxsize�� currsize����
     *        Ŀ�� �Ѵ�.
     *     2. autoextend on���� ������ ��� maxsize�� OS file limit
     *        ���� �۾ƾ� �Ѵ�.
     */

    /* Check maxsize validation */
    IDE_TEST_RAISE( (aAutoExtendMode == ID_TRUE) &&
                    (aFileNode->mCurrSize >= aMaxSize),
                    error_invalid_autoextend_filesize );

    IDE_TEST_RAISE( aMaxSize > getMaxDataFileSize(),
                    error_max_exceed_maxfilesize );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

    /* ------------------------------------------------
     * 1. �α�
     * ----------------------------------------------*/
    // To Fix BUG-13924

    // autoextend off�� ��� nextsize, maxsize�� 0�� �ȴ�.
    if (aAutoExtendMode == ID_FALSE)
    {
        aNextSize = 0;
        aMaxSize = 0;
    }

    if ( aTrans != NULL )
    {
        // PROJ-1548
        // ����Ÿ���� ��忡 ���� DROP ������ TBS Node�� (X) ����� ȹ���ϰ� �ִ�.
        // �׷��⶧���� AUTOEXTEND MODE ������ ���ÿ� ����� �� ����.
        // ������ ���� ������ üũ�غ��ƾ� �Ѵ�.
        // DBF Node�� DROPPED ���¶�� Exception ó���Ѵ�.
        IDE_TEST_RAISE( SMI_FILE_STATE_IS_DROPPED( aFileNode->mState ),
                        error_not_found_datafile_node );

        IDE_TEST( smLayerCallback::writeLogSetAutoExtDBF( aStatistics,
                                                          aTrans,
                                                          aSpaceNode->mHeader.mID,
                                                          aFileNode,
                                                          aAutoExtendMode,
                                                          aNextSize,
                                                          aMaxSize,
                                                          NULL )
                  != IDE_SUCCESS );

    }

    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );
    sState = 1;

    /* -------------------------------------------------------
     * 2. �˻��� datafile ����� autoextend ��带 �����Ѵ�.
     * ----------------------------------------------------- */
    sddDataFile::setAutoExtendProp(aFileNode,
                                   aAutoExtendMode,
                                   aNextSize,
                                   aMaxSize);

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

    /* -------------------------------------------------------
     * 3. �α� ��Ŀ�� �÷����Ѵ�.
     * ----------------------------------------------------- */
    IDU_FIT_POINT( "2.PROJ-1552@sddDiskMgr::alterAutoExtendFEBT" );

    IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( aFileNode )
                == IDE_SUCCESS );

    IDU_FIT_POINT( "3.PROJ-1552@sddDiskMgr::alterAutoExtendFEBT" );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_datafile_node );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aDataFileName));
    }
    IDE_EXCEPTION(error_invalid_autoextend_filesize);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidAutoExtFileSize,
                                aMaxSize,
                                aFileNode->mCurrSize));
    }
    IDE_EXCEPTION(error_max_exceed_maxfilesize);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_MaxExceedMaxFileSize,
                                aMaxSize,
                                (ULong)getMaxDataFileSize()));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;

}
/***********************************************************************
 * Description :  datafile ����� datafile �� �Ӽ� ����
 * BUG-10474�� ���ؼ� datafile rename�� startup control �ܰ迡��
 * tablespace�� offlin�ΰ�쿡�� �����ϴ�.
 **********************************************************************/
IDE_RC sddDiskMgr::alterDataFileName( idvSQL*     aStatistics,
                                      scSpaceID   aTableSpaceID,
                                      SChar*      aOldFileName,
                                      SChar*      aNewFileName )
{

    UInt               sState = 0;
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;
    SChar              sValidNewDataFileName[SM_MAX_FILE_NAME + 1];
    SChar              sValidOldDataFileName[SM_MAX_FILE_NAME + 1];
    UInt               sNameLen;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aNewFileName != NULL );
    IDE_DASSERT( aOldFileName != NULL );

    sNameLen = 0;

    idlOS::memset(sValidNewDataFileName, 0x00, SM_MAX_FILE_NAME + 1);
    idlOS::strncpy(sValidNewDataFileName, aNewFileName, SM_MAX_FILE_NAME);
    sNameLen = idlOS::strlen(sValidNewDataFileName);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_TRUE,
                                sValidNewDataFileName,
                                &sNameLen,
                                SMI_TBS_DISK)
              != IDE_SUCCESS );

    idlOS::memset(sValidOldDataFileName, 0x00, SM_MAX_FILE_NAME + 1);
    idlOS::strncpy(sValidOldDataFileName, aOldFileName, SM_MAX_FILE_NAME);
    sNameLen = idlOS::strlen(sValidOldDataFileName);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_FALSE,
                                sValidOldDataFileName,
                                &sNameLen,
                                SMI_TBS_DISK)
              != IDE_SUCCESS );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;


    IDE_TEST( sddTableSpace::getDataFileNodeByName(sSpaceNode,
                                                   sValidOldDataFileName,
                                                   &sFileNode)
            != IDE_SUCCESS );

    IDE_TEST( sddDataFile::setDataFileName(sFileNode, sValidNewDataFileName)
            != IDE_SUCCESS );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    /* ------------------------------------------------
     * disable recovery check point
     * "alterDataFileName#before update loganchor"
     * startup control ���������� ����Ǵ� command�̱�
     * ������ recovery ����� �ƴմϴ�. loganchor�� �ݿ���
     * �ǰ� crash�� �Ǿ��ٸ� �ݿ��� �Ȱ��Դϴ�.
     * ----------------------------------------------*/

    /* loganchor flush */
    IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/**********************************************************************
 * Description : �ش� tablespace ����� total page ������ ��ȯ
 **********************************************************************/
IDE_RC sddDiskMgr::getTotalPageCountOfTBS( idvSQL  *  aStatistics,
                                           scSpaceID  aTableSpaceID,
                                           ULong*     aTotalPageCount )
{

    UInt               sState = 0;
    sddTableSpaceNode* sSpaceNode;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aTotalPageCount != NULL );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    *aTotalPageCount = sddTableSpace::getTotalPageCount( sSpaceNode );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;

}

/**********************************************************************
 *  fix BUG-13646 tablespace�� page count�� extentũ�⸦ ��´�.
 **********************************************************************/
IDE_RC sddDiskMgr::getExtentAnTotalPageCnt( idvSQL  *  aStatistics,
                                            scSpaceID  aTableSpaceID,
                                            UInt*      aExtentPageCount,
                                            ULong*     aTotalPageCount )
{

    UInt               sState = 0;
    sddTableSpaceNode* sSpaceNode;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aTableSpaceID ) == ID_TRUE );
    IDE_DASSERT( aTotalPageCount != NULL );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aTableSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    *aTotalPageCount  = sddTableSpace::getTotalPageCount(sSpaceNode);
    *aExtentPageCount = sSpaceNode->mExtPageCount;

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description :
 **********************************************************************/
IDE_RC sddDiskMgr::existDataFile( idvSQL*   aStatistics,
                                  scSpaceID aID,
                                  SChar*    aName,
                                  idBool*   aExist)
{

    UInt               sState = 0;
    UInt               sNameLength;
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;
    SChar              sValidName[SM_MAX_FILE_NAME];

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aID ) == ID_TRUE );
    IDE_DASSERT( aName != NULL );

    sNameLength = 0;

    idlOS::strcpy((SChar*)sValidName, aName);
    sNameLength = idlOS::strlen(sValidName);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_TRUE,
                                (SChar*)sValidName,
                                &sNameLength,
                                SMI_TBS_DISK)
              != IDE_SUCCESS );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    /* ������⿡�� aExist�� ���� ���� ������ �����Ҽ� �ֵ��� �ؾ��Ѵ�. */
    (void)sddTableSpace::getDataFileNodeByName( sSpaceNode,
                                                sValidName,
                                                &sFileNode );

    *aExist = ((sFileNode != NULL) ? ID_TRUE : ID_FALSE );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Description :
 **********************************************************************/
IDE_RC sddDiskMgr::existDataFile( SChar   * aName,
                                  idBool  * aExist)
{
    UInt                sNameLength;
    sddDataFileNode   * sFileNode   = NULL;
    scSpaceID           sSpaceID;
    scPageID            sFirstPID;
    scPageID            sLastPID;
    SChar               sValidName[ SM_MAX_FILE_NAME ];

    IDE_DASSERT( aName != NULL );

    sNameLength = 0;

    idlOS::strcpy((SChar *)sValidName, aName);
    sNameLength = idlOS::strlen(sValidName);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_TRUE,
                                (SChar*)sValidName,
                                &sNameLength,
                                SMI_TBS_DISK)
              != IDE_SUCCESS );

    sctTableSpaceMgr::getDataFileNodeByName( sValidName,
                                             &sFileNode,
                                             &sSpaceID,
                                             &sFirstPID,
                                             &sLastPID );

    *aExist = ((sFileNode != NULL) ? ID_TRUE : ID_FALSE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  ���̺����̽��� ����Ÿ������
  ������ ���� �������� ��ȿ�� ���θ� ��ȯ�Ѵ�.

   drop tablespace�� ����ó���� ����Ÿ������ ���� DROPPED �ϰ�
   ���̺����̽��� ���߿� DROPPED�� �Ǳ� ������ (�����˰���)
   Ŀ�� PENDING ���� ������ ���� Ʈ������� Ŀ�ԵǴ���
   �������� üũ�ؾ��ϴ� ��찡 �����Ѵ�.

   DELETE �����尡 Index/Table ���׸�Ʈ PENDING ���꿡�� ����Ÿ���ϵ�
   üũ�ϵ��� �Ͽ� DROPPED�� �Ȱ�� �����ϵ��� �ؾ��Ѵ�.

   [IN] aTableSpaceID  -  �˻��� ���̺����̽� ID
   [IN] aPageID        -  �˻��� ������ ID
   [IN] aIsValid        - ��ȿ�� ���������� ����

   [ ���� ]
   Disk Aging �ÿ� Table/Index�� ���� Free Segment �� �ؾ��� �� ���θ�
   �Ǵ��� �� ȣ��Ǵµ�, Latch�� ��� �Ǵ��ϱ� ������
   ��, DDL�� ���� Aging ó���̱� ������ ����ϰ� ȣ������� �ʴ´�.

   [ Call by ]
   smcTable::rebuildDRDBIndexHeader
   sdpPageList::freeIndexSegForEntry
   sdpPageList::freeTableSegForEntry
   sdpPageList::freeLobSeg
   sdpPageList::initEntryAtRuntime
   sdbDW::initEntryAtRuntime

 */
idBool sddDiskMgr::isValidPageID( idvSQL*    aStatistics,
                                  scSpaceID  aTableSpaceID,
                                  scPageID   aPageID )
{
    sddTableSpaceNode* sSpaceNode = NULL;
    sddDataFileNode*   sFileNode;
    idBool             sIsValid = ID_FALSE;

    // drop �� TBS�� NULL�� ��ȯ�ȴ�.
    sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::findSpaceNodeWithoutException( aTableSpaceID );

    if( sSpaceNode != NULL )
    {
        sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                         sSpaceNode );
        // fix BUG-17159
        // dropped/discarded tbs�� ���ؼ� invalid�ϴٶ�� �Ǵ��Ѵ�.
        if ( sctTableSpaceMgr::hasState(
                               &sSpaceNode->mHeader,
                               SCT_SS_INVALID_DISK_TBS) == ID_FALSE )
        {
            sddTableSpace::getDataFileNodeByPageIDWithoutException(
                                                sSpaceNode,
                                                aPageID,
                                                &sFileNode );

            if( sFileNode != NULL )
            {
                sIsValid = ID_TRUE;
            }
        }
        else
        {
            // Dropped, Discard �� TBS�� Valid ���� �ʴ�.
        }
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return sIsValid;
}


//***********************************************************************
// For Unit TestCase

/***********************************************************************
 * Description : �ش� datafile ��带 �˻��Ͽ� datafile�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sddDiskMgr::openDataFile( idvSQL *  aStatistics,
                                 scSpaceID aTableSpaceID,
                                 scPageID  aPageID)
{

    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode )
                  != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(
                                    sSpaceNode,
                                    aPageID,
                                    &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( prepareIO( aStatistics,
                         sFileNode ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : �ش� datafile ��带 �˻��Ͽ� datafile�� close�Ѵ�.
 ***********************************************************************/
IDE_RC sddDiskMgr::closeDataFile(scSpaceID aTableSpaceID,
                                 scPageID  aPageID)
{
    sddTableSpaceNode* sSpaceNode;
    sddDataFileNode*   sFileNode;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode)
                  != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode->mHeader.mID == aTableSpaceID );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(
                                    sSpaceNode,
                                    aPageID,
                                    &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( closeDataFile( NULL, sFileNode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : completeIO�� �Ͽ�,max open�����¿��� victim�� �ǵ��� �Ѵ�.
 * // Unit Test ��
 ***********************************************************************/
IDE_RC sddDiskMgr::completeIO( idvSQL*   aStatistics,
                               scSpaceID aTableSpaceID,
                               scPageID  aPageID )
{
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aTableSpaceID,
                                                        (void**)&sSpaceNode)
                  != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByPageID(
                                    sSpaceNode,
                                    aPageID,
                                    &sFileNode )
              != IDE_SUCCESS );

    IDE_TEST( completeIO( aStatistics,
                          sFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  PRJ-1149,  �־��� ����Ÿ ������ ����� �д´�.

  [IN]  aFileNode       - ����Ÿ���� ���
  [OUT] aDBFileHdr      - ����Ÿ���Ϸκ��� �ǵ��� ���ϸ�Ÿ��� ������ ������ ����
  [OUT] aIsMediaFailure - ��Ÿ������ Ȯ���� �� �����ʿ俩�� ��ȯ
*/
IDE_RC sddDiskMgr::checkValidationDBFHdr(
                       idvSQL           * aStatistics,
                        sddDataFileNode  * aFileNode,
                        sddDataFileHdr   * aDBFileHdr,
                        idBool           * aIsMediaFailure )
{
    IDE_DASSERT( aFileNode       != NULL );
    IDE_DASSERT( aDBFileHdr      != NULL );
    IDE_DASSERT( aIsMediaFailure != NULL );

    IDE_TEST( readDBFHdr( aStatistics,
                          aFileNode,
                          aDBFileHdr )
              != IDE_SUCCESS );

    // Loganchor�� ���Ϸκ��� �ǵ��� ���ϸ�Ÿ��������� ���Ͽ�
    // �̵����� �ʿ俩�θ� ��ȯ�Ѵ�.
    IDE_TEST(sddDataFile::checkValidationDBFHdr( aFileNode,
                                                 aDBFileHdr,
                                                 aIsMediaFailure )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : PROJ-1867  �־��� DB File�� DBFileHdr�� �д´�.
 *
 * [IN]  aFileNode       - ����Ÿ���� ���
 * [OUT] aDBFileHdr      - ����Ÿ���Ϸκ��� �ǵ��� ���ϸ�Ÿ��� ������ ������ ����
 * ********************************************************************/
IDE_RC sddDiskMgr::readDBFHdr( idvSQL           * aStatistics,
                               sddDataFileNode  * aFileNode,
                               sddDataFileHdr   * aDBFileHdr )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_DASSERT( aFileNode       != NULL );
    IDE_DASSERT( aDBFileHdr      != NULL );

    IDE_TEST( prepareIO( aStatistics,
                         aFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    /* File�� ���� IO�� �׻� DirectIO�� ����ؼ� Buffer, Size, Offset��
     * DirectIO Page ũ��� Align��Ų��. */
    IDE_TEST( aFileNode->mFile.read( aStatistics,
                                     SM_DBFILE_METAHDR_PAGE_OFFSET,
                                     aFileNode->mAlignedPageBuff,
                                     SD_PAGE_SIZE,
                                     NULL/*RealReadSize*/ )
              != IDE_SUCCESS );

    idlOS::memcpy( aDBFileHdr,
                   aFileNode->mAlignedPageBuff,
                   ID_SIZEOF(sddDataFileHdr) );

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Database File Header Failure\n"
                 "              Tablespace ID = %"ID_UINT32_FMT"\n"
                 "              File ID       = %"ID_UINT32_FMT"\n"
                 "              IO Count      = %"ID_UINT32_FMT"\n"
                 "              File Opened   = %s",
                 aFileNode->mSpaceID,
                 aFileNode->mID,
                 aFileNode->mIOCount,
                 (aFileNode->mIsOpened ? "Open" : "Close") );

    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aFileNode,
                                SDD_IO_READ ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����Ÿ ���� �ش� ����.
 * PRJ-1149,  �־��� ����Ÿ ���� ����� ����� �̿��Ͽ�
 * ����Ÿ ���� ������ �Ѵ�.
 **********************************************************************/
IDE_RC sddDiskMgr::writeDBFHdr( idvSQL         * aStatistics,
                                sddDataFileNode* aDataFileNode )
{
    idBool sPreparedIO = ID_FALSE;

    IDE_DASSERT( aDataFileNode != NULL);
    IDE_DASSERT( sddDataFile::checkValuesOfDBFHdr(
                 &(aDataFileNode->mDBFileHdr))
                 == IDE_SUCCESS );

    // BUG-17158
    // offline TBS�� DBF Hdr�� Checkpoint�� ������ �� �ְ�
    // �ϱ� ���� �����Ѵ�.
    mEnableWriteToOfflineDBF = ID_TRUE;

    IDE_TEST( prepareIO( aStatistics,
                         aDataFileNode ) != IDE_SUCCESS );
    sPreparedIO = ID_TRUE;

    IDU_FIT_POINT( "1.TASK-1842@sddDiskMgr::writeDBFHdr" );

    IDE_TEST( sddDataFile::writeDBFileHdr(
                  aStatistics,
                  aDataFileNode,
                  NULL )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "2.TASK-1842@sddDiskMgr::writeDBFHdr" );

    IDE_TEST(sddDataFile::sync( aDataFileNode,
                                smLayerCallback::setEmergency ) != IDE_SUCCESS);

    sPreparedIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aDataFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    mEnableWriteToOfflineDBF = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Write Database File Header Failure\n"
                 "               Tablespace ID = %"ID_UINT32_FMT"\n"
                 "               File ID       = %"ID_UINT32_FMT"\n"
                 "               IO Count      = %"ID_UINT32_FMT"\n"
                 "               File Opened   = %s",
                 aDataFileNode->mSpaceID,
                 aDataFileNode->mID,
                 aDataFileNode->mIOCount,
                 (aDataFileNode->mIsOpened ? "Open" : "Close") );

    if( sPreparedIO == ID_TRUE )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aDataFileNode,
                                SDD_IO_READ )
                    == IDE_SUCCESS );
    }

    mEnableWriteToOfflineDBF = ID_FALSE;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����Ÿ ���� ����
 **********************************************************************/
IDE_RC sddDiskMgr::copyDataFile(idvSQL            * aStatistics,
                                sddTableSpaceNode * aSpaceNode,
                                sddDataFileNode   * aDataFileNode,
                                SChar             * aBackupFilePath )
{
    idBool  sLocked = ID_FALSE;
    idBool  sPrepareIO = ID_FALSE;
    smLSN   sMustRedoToLSN;
    sddDataFileHdr   sDBFileHdr;

    IDE_DASSERT( aDataFileNode   != NULL );
    IDE_DASSERT( aBackupFilePath != NULL );

    /* BUG-41031
     * ������ ���°� SMI_FILE_CREATING, SMI_FILE_RESIZING, SMI_FILE_DROPPING�� ��� ��� ���� �ÿ� ��⸦ �ϰ� �ȴ�.
     * �׷��� ���� Ȯ��� backup���� ���ü� ��� lock�� �ƴ� ���°����θ� �ľ��ϱ� ������
     * ����� ����Ǵ� �߿� �ٽ� ���� ���� SMI_FILE_CREATING, SMI_FILE_RESIZING, SMI_FILE_DROPPING ���·� ����� �� �ִ�.
     * �׷��Ƿ� ���� ���� ���� �ٽ� �ٲ� ���¿��� ������ ������ ������ ��� trace log�� �ﵵ�� �Ѵ�. */
    if ( SMI_FILE_STATE_IS_DROPPED ( aDataFileNode->mState ) ||
         SMI_FILE_STATE_IS_CREATING( aDataFileNode->mState ) ||
         SMI_FILE_STATE_IS_RESIZING( aDataFileNode->mState ) ||
         SMI_FILE_STATE_IS_DROPPING( aDataFileNode->mState ) )
    {
        ideLog::log( IDE_SM_0,
                     "The data file has an invalid state.\n"
                     "               Tablespace ID = %"ID_UINT32_FMT"\n"
                     "               File ID       = %"ID_UINT32_FMT"\n"
                     "               State         = %08"ID_XINT32_FMT"\n"
                     "               IO Count      = %"ID_UINT32_FMT"\n"
                     "               File Opened   = %s\n",
                     "               File Modified = %s\n",
                     aDataFileNode->mSpaceID,
                     aDataFileNode->mID,
                     aDataFileNode->mState,
                     aDataFileNode->mIOCount,
                     ( aDataFileNode->mIsOpened ? "Open" : "Close" ),
                     ( aDataFileNode->mIsModified ? "Modified" : "Unmodified" ) );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( prepareIO( aStatistics,
                         aDataFileNode ) != IDE_SUCCESS );
    sPrepareIO = ID_TRUE;

    IDE_TEST(aDataFileNode->mFile.copy( aStatistics,
                                        aBackupFilePath )
             != IDE_SUCCESS);

    // Complete ��ü�� lock�� �ʿ����� ������
    // Complete�� Ǯ���� File Close �� �ٸ� �۾��� ����Ǳ� ����
    // ���� lock�� ��´�.
    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    sLocked    = ID_TRUE;
    sPrepareIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aDataFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    // backup�� DBFile�� DataFileHeader�� MustRedoToLSN�� �����Ѵ�.

    smLayerCallback::getLstLSN( &sMustRedoToLSN );

    sDBFileHdr = aDataFileNode->mDBFileHdr;

    sddDataFile::setDBFHdr( &sDBFileHdr,
                            NULL,   // DiskRedoLSN
                            NULL,   // DiskCreateLSN
                            &sMustRedoToLSN,
                            NULL ); //DataFileDescSlotID

    IDE_TEST( sddDataFile::writeDBFileHdrByPath( aBackupFilePath,
                                                 &sDBFileHdr )
              != IDE_SUCCESS );

    sLocked = ID_FALSE;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sPrepareIO == ID_TRUE  )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aDataFileNode,
                                SDD_IO_READ ) == IDE_SUCCESS );
    }

    if ( sLocked == ID_TRUE )
    {
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;

}
/***********************************************************************
 * Description : ����Ÿ ���� incremental backup
 * PROJ-2133
 **********************************************************************/
IDE_RC sddDiskMgr::incrementalBackup(idvSQL                 * aStatistics,
                                     sddTableSpaceNode      * aSpaceNode,
                                     smriCTDataFileDescSlot * aDataFileDescSlot,
                                     sddDataFileNode        * aDataFileNode,
                                     smriBISlot             * aBackupInfo )
{
    idBool              sLocked = ID_FALSE;
    idBool              sPrepareIO = ID_FALSE;
    smLSN               sMustRedoToLSN;
    sddDataFileHdr      sDBFileHdr;
    iduFile           * sSrcFile;
    iduFile             sDestFile;
    ULong               sBackupFileSize = 0;
    ULong               sBackupSize;
    ULong               sIBChunkSizeInByte;
    ULong               sRestIBChunkSizeInByte;
    UInt                sState             = 0;
    idBool              sBackupCreateState = ID_FALSE;

    IDE_DASSERT( aDataFileDescSlot  != NULL );
    IDE_DASSERT( aDataFileNode      != NULL );
    IDE_DASSERT( aBackupInfo        != NULL );

    sSrcFile = &aDataFileNode->mFile;
    
    IDE_TEST( sDestFile.initialize( IDU_MEM_SM_SDD,
                                    1,                             
                                    IDU_FIO_STAT_OFF,              
                                    IDV_WAIT_INDEX_NULL )          
              != IDE_SUCCESS );              
    sState = 1;

    IDE_TEST( sDestFile.setFileName( aBackupInfo->mBackupFileName ) 
              != IDE_SUCCESS );              

    IDE_TEST( sDestFile.create()    != IDE_SUCCESS );
    sBackupCreateState = ID_TRUE;

    IDE_TEST( sDestFile.open()      != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( prepareIO( aStatistics,
                         aDataFileNode ) != IDE_SUCCESS );
    sPrepareIO = ID_TRUE;
 
    /* Incremental backup�� �����Ѵ�.*/
    IDE_TEST( smriChangeTrackingMgr::performIncrementalBackup( 
                                                   aDataFileDescSlot,
                                                   sSrcFile,
                                                   &sDestFile,
                                                   SMRI_CT_DISK_TBS,
                                                   SC_NULL_SPACEID, //chkpt img file spaceID-not use in DRDB
                                                   0,               //chkpt img file number-not use in DRDB
                                                   aBackupInfo )
              != IDE_SUCCESS );

    // Complete ��ü�� lock�� �ʿ����� ������
    // Complete�� Ǯ���� File Close �� �ٸ� �۾��� ����Ǳ� ����
    // ���� lock�� ��´�.
    sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                     aSpaceNode );

    sLocked    = ID_TRUE;
    sPrepareIO = ID_FALSE;
    IDE_TEST( completeIO( aStatistics,
                          aDataFileNode,
                          SDD_IO_READ ) != IDE_SUCCESS );

    /* 
     * ������ ������ ũ��� ����� IBChunk�� ������ ���� backup�� ũ�Ⱑ
     * �������� Ȯ���Ѵ�
     */
    IDE_TEST( sDestFile.getFileSize( &sBackupFileSize ) != IDE_SUCCESS );

    sIBChunkSizeInByte = (ULong)SD_PAGE_SIZE * aBackupInfo->mIBChunkSize;

    sBackupSize = (ULong)aBackupInfo->mIBChunkCNT * sIBChunkSizeInByte;

    /* 
     * ������ Ȯ���̳� ��Ұ� IBChunk�� ũ�⸸ŭ �̷����� ������ �ֱ⶧����
     * ���� ����� ������ ũ��� mIBChunkCNT�� ����� �ƴҼ� �ִ�.
     * ������ ũ�Ⱑ mIBChunkCNT�� ����� �ǵ��� �������ش�.
     */
    if( sBackupFileSize > SM_DBFILE_METAHDR_PAGE_SIZE )
    {
        sBackupFileSize -= SM_DBFILE_METAHDR_PAGE_SIZE;

        sRestIBChunkSizeInByte = (ULong)sBackupFileSize % sIBChunkSizeInByte;

        IDE_ASSERT( sRestIBChunkSizeInByte % SD_PAGE_SIZE == 0 );

        if( sRestIBChunkSizeInByte != 0 )
        {
            sBackupFileSize += sIBChunkSizeInByte - sRestIBChunkSizeInByte;
        }
    }

    IDE_ASSERT( sBackupFileSize == sBackupSize );

    // backup�� DBFile�� DataFileHeader�� MustRedoToLSN�� �����Ѵ�.
    smLayerCallback::getLstLSN( &sMustRedoToLSN );

    sDBFileHdr = aDataFileNode->mDBFileHdr;

    sddDataFile::setDBFHdr( &sDBFileHdr,
                            NULL,   // DiskRedoLSN
                            NULL,   // DiskCreateLSN
                            &sMustRedoToLSN,
                            NULL ); //DataFileDescSlotID

    sLocked = ID_FALSE;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

    sState = 1;
    IDE_TEST( sDestFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sDestFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sPrepareIO == ID_TRUE  )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                aDataFileNode,
                                SDD_IO_READ ) == IDE_SUCCESS );
    }

    if ( sLocked == ID_TRUE )
    {
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( sDestFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sDestFile.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    if( sBackupCreateState == ID_TRUE )
    {
        IDE_ASSERT( idf::unlink( aBackupInfo->mBackupFileName ) == 0 );
    }

    return IDE_FAILURE;
}

/*
  PRJ-1548 User Memory Tablespace

  ���������� �������Ŀ� ��� ���̺����̽���
  DataFileCount�� TotalPageCount�� ����Ͽ� �����Ѵ�.

  �� ������Ʈ���� sddTableSpaceNode�� mDataFileCount��
  mTotalPageCount�� RUNTIME ������ ����ϹǷ�
  ���������� �������Ŀ� ������ �������־�� �Ѵ�.
*/
void sddDiskMgr::calculateFileSizeOfAllTBS()
{
    UInt                sSpaceID;
    UInt                sMaxTBSCount;
    sddTableSpaceNode * sSpaceNode;

    sMaxTBSCount = sctTableSpaceMgr::getNewTableSpaceID();

    for( sSpaceID = 0 ; sSpaceID < sMaxTBSCount ; sSpaceID++ )
    {
        sSpaceNode = (sddTableSpaceNode*)sctTableSpaceMgr::getSpaceNodeBySpaceID( sSpaceID );

        if ( sSpaceNode != NULL )
        {
            //BUG-48460: restart �� �� discard�� ���̺� �����̽��� üũ�ؾ� ��
            if (    SMI_TBS_IS_NOT_DROPPED( sSpaceNode->mHeader.mState )
                 && SMI_TBS_IS_NOT_DISCARDED( sSpaceNode->mHeader.mState ) )
            {
                if ( sctTableSpaceMgr::isDiskTableSpace( sSpaceNode ) == ID_TRUE )
                {
                    sddTableSpace::calculateFileSizeOfTBS( sSpaceNode );
                }
            }
        }
    }

    return;
}

/*
  PRJ-1548 User Memory Tablespace

  ��ũ ���̺����̽��� ��� ����Ѵ�.

  [IN] aTrans : Ʈ�����
  [IN] aBackupDir : ��� Dest. ���丮
*/
IDE_RC sddDiskMgr::backupAllDiskTBS( idvSQL    * aStatistics,
                                     SChar     * aBackupDir )
{
    sctActBackupArgs sActBackupArgs;

    IDE_DASSERT( aBackupDir != NULL );

    sActBackupArgs.mBackupDir           = aBackupDir;
    sActBackupArgs.mIsIncrementalBackup = ID_FALSE;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                aStatistics,
                                sddTableSpace::doActOnlineBackup,
                                (void*)&sActBackupArgs, /* Action Argument*/
                                SCT_ACT_MODE_NONE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  PROJ-2133 increemntal backup

  ��ũ ���̺����̽��� ��� ����Ѵ�.

  [IN] aTrans : Ʈ�����
  [IN] aBackupDir : ��� Dest. ���丮
*/
IDE_RC sddDiskMgr::incrementalBackupAllDiskTBS( idvSQL     * aStatistics,
                                                smriBISlot * aCommonBackupInfo,
                                                SChar      * aBackupDir )
{
    sctActBackupArgs sActBackupArgs;

    IDE_DASSERT( aBackupDir != NULL );

    sActBackupArgs.mBackupDir           = aBackupDir;
    sActBackupArgs.mCommonBackupInfo    = aCommonBackupInfo;
    sActBackupArgs.mIsIncrementalBackup = ID_TRUE;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS(
                                aStatistics,
                                sddTableSpace::doActOnlineBackup,
                                (void*)&sActBackupArgs, /* Action Argument*/
                                SCT_ACT_MODE_NONE ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  ���̺����̽��� Dirty�� ����Ÿ������ Sync�Ѵ�.
  �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.

  [IN] aSpaceNode : ������ TBS Node
  [IN] aActionArg : NULL
*/
IDE_RC sddDiskMgr::doActIdentifyAllDBFiles(
                                idvSQL             * aStatistics,
                                sctTableSpaceNode  * aSpaceNode,
                                void               * aActionArg )
{
    idBool                   sNeedRecovery;
    UInt                     i;
    sddTableSpaceNode*       sSpaceNode;
    sddDataFileNode*         sFileNode;
    sddDataFileHdr           sDBFileHdr;
    SChar                    sMsgBuf[ 512 ];
    sctActIdentifyDBArgs   * sIdentifyDBArgs;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    idlOS::memset( &sDBFileHdr, 0x00, ID_SIZEOF(sddDataFileHdr) );

    while ( 1 )
    {
        if ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) != ID_TRUE )
        {
            // ��ũ ���̺����̽��� �ƴ� ��� üũ���� �ʴ´�.
            break;
        }

        if ( sctTableSpaceMgr::hasState( aSpaceNode,
                                         SCT_SS_SKIP_IDENTIFY_DB )
             == ID_TRUE )
        {
            // ���̺����̽��� DISCARD�� ��� üũ���� �ʴ´�.
            // ���̺����̽��� ������ ��� üũ���� �ʴ´�.
            break;
        }

        sSpaceNode      = (sddTableSpaceNode*)aSpaceNode;
        sIdentifyDBArgs = (sctActIdentifyDBArgs*)aActionArg;

        for (i=0; i < sSpaceNode->mNewFileID ; i++ )
        {
            sFileNode = sSpaceNode->mFileNodeArr[i] ;

            if( sFileNode == NULL )
            {
                continue;
            }

            if ( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                // ����Ÿ������ ������ ��� �����Ѵ�.
                continue;
            }

            // PRJ-1548 User Memory Tablespace
            // CREATING ���� �Ǵ� DROPPING ������ ���ϳ���
            // �ݵ�� ������ �����ؾ� �Ѵ�.
            if ( idf::access( sFileNode->mName, F_OK|R_OK|W_OK ) != 0 )
            {
                // restart ���̸�
                // ��ũ �ӽ� ���̺����̽��� ��� �ٽ� ����� �ȴ�.
                // üũ����Ʈ �� �ٸ� ������(��߿�) ȣ��Ǹ�I ������ �����ϰ� ���� �ø�
                if ( ( smiGetStartupPhase() < SMI_STARTUP_SERVICE ) &&
                     ( sctTableSpaceMgr::isTempTableSpace( sSpaceNode ) == ID_TRUE ) )
                {
                    if ( sFileNode->mCurrSize > sFileNode->mInitSize )
                    {
                        sddDataFile::setCurrSize( sFileNode, sFileNode->mInitSize);
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    IDE_TEST( sddDataFile::reuse( aStatistics, sFileNode ) != IDE_SUCCESS );

                    /* loganchor flush */
                    IDE_TEST( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                              != IDE_SUCCESS );

                }
                else
                {
                    sIdentifyDBArgs->mIsFileExist = ID_FALSE;
                    sIdentifyDBArgs->mIsValidDBF  = ID_FALSE;

                    idlOS::snprintf( sMsgBuf, 
                                     SM_MAX_FILE_NAME,
                                     "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n"
                                     "               [%s-<DBF ID:%"ID_UINT32_FMT">] Datafile Not Found\n",
                                     sSpaceNode->mHeader.mName,
                                     sFileNode->mID );

                    IDE_CALLBACK_SEND_MSG(sMsgBuf);
                }
                continue;
            }

            if ( sctTableSpaceMgr::isTempTableSpace( sSpaceNode ) == ID_TRUE )
            {
                // ��ũ �ӽ� ���̺����̽��� ��� ����Ÿ������
                // �����ϴ��� Ȯ�θ� �Ѵ�.
                continue;
            }


            // version, oldest lsn, create lsn ��ġ���� ������
            // media recovery�� �ʿ��ϴ�.
            IDE_TEST_RAISE( checkValidationDBFHdr(
                                                aStatistics,
                                                sFileNode,
                                                &sDBFileHdr,
                                                &sNeedRecovery ) != IDE_SUCCESS,
                            err_data_file_header_invalid );

            if ( sNeedRecovery == ID_TRUE )
            {
                idlOS::snprintf( sMsgBuf, 
                                 SM_MAX_FILE_NAME,
                                 "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n"
                                 "               [%s-<DBF ID:%"ID_UINT32_FMT">] Mismatch Datafile Version\n",
                                 sSpaceNode->mHeader.mName,
                                 sFileNode->mID );
                IDE_CALLBACK_SEND_MSG( sMsgBuf );

                sIdentifyDBArgs->mIsValidDBF  = ID_FALSE;

                continue;
            }
        }

        break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_data_file_header_invalid )
    {
        /* BUG-33353 */
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidDatafileHeader,
                                  sFileNode->mName,
                                  sFileNode->mSpaceID,
                                  sFileNode->mID,
                                  (sFileNode->mDBFileHdr).mRedoLSN.mFileNo,
                                  (sFileNode->mDBFileHdr).mRedoLSN.mOffset,
                                  sDBFileHdr.mRedoLSN.mFileNo,
                                  sDBFileHdr.mRedoLSN.mOffset,
                                  (sFileNode->mDBFileHdr).mCreateLSN.mFileNo,
                                  (sFileNode->mDBFileHdr).mCreateLSN.mOffset,
                                  sDBFileHdr.mCreateLSN.mFileNo,
                                  sDBFileHdr.mCreateLSN.mOffset,
                                  smVersionID & SM_CHECK_VERSION_MASK,
                                  (sFileNode->mDBFileHdr).mSmVersion & SM_CHECK_VERSION_MASK,
                                  sDBFileHdr.mSmVersion & SM_CHECK_VERSION_MASK) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  ��ũ ���̺����̽��� �̵������� �ִ� ����Ÿ���� ����� �����.

  [IN]  aSpaceNode     - ���̺����̽� ���
  [IN]  aRecoveryType  - �̵��� Ÿ��
  [OUT] aDiskDBFCount  - �̵������� �߻��� ����Ÿ���ϳ�� ����
  [OUT] aFromRedoLSN   - �������� Redo LSN
  [OUT] aToRedoLSN     - �����Ϸ� Redo LSN
*/
IDE_RC sddDiskMgr::makeMediaRecoveryDBFList(
                            idvSQL            * aStatistics,
                            sctTableSpaceNode * aSpaceNode,
                            smiRecoverType      aRecoveryType,
                            UInt              * aFailureDBFCount,
                            smLSN             * aFromRedoLSN,
                            smLSN             * aToRedoLSN )
{
    sddDataFileHdr      sDBFileHdr;
    sddDataFileNode   * sFileNode;
    smLSN               sHashNodeFromLSN;
    smLSN               sHashNodeToLSN;
    idBool              sIsMediaFailure;
    SChar               sMsgBuf[ SM_MAX_FILE_NAME ];
    sddTableSpaceNode * sSpaceNode;
    UInt                i;

    IDE_DASSERT( aSpaceNode       != NULL );
    IDE_DASSERT( aFailureDBFCount != NULL );
    IDE_DASSERT( aFromRedoLSN     != NULL );
    IDE_DASSERT( aToRedoLSN       != NULL );

    idlOS::memset( &sDBFileHdr, 0x00, ID_SIZEOF(sddDataFileHdr) );

    sSpaceNode = (sddTableSpaceNode *)aSpaceNode;

    for (i=0; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            continue;
        }

        /* ------------------------------------------------
         * [1] ����Ÿ������ �����ϴ��� �˻�
         * ----------------------------------------------*/
        if ( idf::access( sFileNode->mName, F_OK|W_OK|R_OK ) != 0 )
        {
            idlOS::snprintf( sMsgBuf, 
                             SM_MAX_FILE_NAME,
                             "  [SM-WARNING] CANNOT IDENTIFY DATAFILE\n"
                             "               [%s-<DBF ID:%"ID_UINT32_FMT">] DATAFILE NOT FOUND\n",
                             aSpaceNode->mName,
                             sFileNode->mID );
            IDE_CALLBACK_SEND_MSG(sMsgBuf);

            IDE_RAISE( err_file_not_found );
        }

        /* ------------------------------------------------
         * [2] ����Ÿ���ϰ� ���ϳ��� ���̳ʸ������� �˻�
         * ----------------------------------------------*/
        IDE_TEST_RAISE( checkValidationDBFHdr(
                                            aStatistics,
                                            sFileNode,
                                            &sDBFileHdr,
                                            &sIsMediaFailure) != IDE_SUCCESS,
                        err_data_file_header_invalid );

        if ( sIsMediaFailure == ID_TRUE )
        {
            /*
               �̵�� ������ �����ϴ� ���

               ��ġ���� �ʴ� ���� ����(COMPLETE) ������
               �����ؾ� �Ѵ�.
            */
            IDE_TEST_RAISE( (aRecoveryType == SMI_RECOVER_UNTILTIME) ||
                            (aRecoveryType == SMI_RECOVER_UNTILCANCEL),
                            err_incomplete_media_recovery);
        }
        else
        {
            /*
              �̵������� ���� ����Ÿ����

              �ҿ�������(INCOMPLETE) �̵�� �����ÿ���
              ������� ������ ������� �����ϹǷ� REDO LSN��
              �α׾�Ŀ�� ��ġ���� �ʴ� ���� �������� �ʴ´�.

              �׷��ٰ� ���������ÿ� ��� ����Ÿ������
              ������ �־�� �Ѵٴ� ���� �ƴϴ�.
            */
        }

        if ( (aRecoveryType == SMI_RECOVER_COMPLETE) &&
             (sIsMediaFailure != ID_TRUE) )
        {
            // ���������� ������ ���� ����Ÿ���� ���
            // �̵�� ������ �ʿ����.
        }
        else
        {
            // ������ �����ϰ����ϴ� �������� �Ǵ�
            // ������ ���� �ҿ���������
            // ��� �̵�� ������ �����Ѵ�.

            // �ҿ��� ���� :
            // ����Ÿ���� ����� oldest lsn�� �˻��� ����,
            // ����ġ�ϴ� ��� ����������Ϸ� �����Ѵ�.

            // �������� :
            // ��� ����Ÿ������ ��������� �ȴ�.

            IDE_TEST( smLayerCallback::addRecvFileToHash(
                                           &sDBFileHdr,
                                           sFileNode->mName,
                                           &sHashNodeFromLSN,
                                           &sHashNodeToLSN )
                      != IDE_SUCCESS );

            if ( smLayerCallback::isLSNGT( aFromRedoLSN,
                                           &sHashNodeFromLSN )
                 == ID_TRUE)
            {
                SM_GET_LSN( *aFromRedoLSN, sHashNodeFromLSN );
            }
            if ( smLayerCallback::isLSNGT( &sHashNodeToLSN,
                                           aToRedoLSN )
                 == ID_TRUE)
            {
                SM_GET_LSN( *aToRedoLSN, sHashNodeToLSN );
            }

            *aFailureDBFCount = *aFailureDBFCount + 1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistFile ) );
    }
    IDE_EXCEPTION( err_data_file_header_invalid );
    {
        /* BUG-33353 */
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidDatafileHeader,
                                  sFileNode->mName,
                                  sFileNode->mSpaceID,
                                  sFileNode->mID,
                                  (sFileNode->mDBFileHdr).mRedoLSN.mFileNo,
                                  (sFileNode->mDBFileHdr).mRedoLSN.mOffset,
                                  sDBFileHdr.mRedoLSN.mFileNo,
                                  sDBFileHdr.mRedoLSN.mOffset,
                                  (sFileNode->mDBFileHdr).mCreateLSN.mFileNo,
                                  (sFileNode->mDBFileHdr).mCreateLSN.mOffset,
                                  sDBFileHdr.mCreateLSN.mFileNo,
                                  sDBFileHdr.mCreateLSN.mOffset,
                                  smVersionID & SM_CHECK_VERSION_MASK,
                                  (sFileNode->mDBFileHdr).mSmVersion & SM_CHECK_VERSION_MASK,
                                  sDBFileHdr.mSmVersion & SM_CHECK_VERSION_MASK) );
    }
    IDE_EXCEPTION( err_incomplete_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 *  ���̺� �����̽��� MustRedoToLSN�� �߿� ���� ū ���� ��ȯ�Ѵ�.
 *
 * aSpaceNode     - [IN]  ���̺����̽� ���
 * aMustRedoToLSN - [OUT] �����Ϸ� Redo LSN
 * aDBFileName    - [OUT] �ش� Must Redo to LSN�� ���� DBFile
 **********************************************************************/
IDE_RC sddDiskMgr::getMustRedoToLSN( idvSQL            * aStatistics,
                                     sctTableSpaceNode * aSpaceNode,
                                     smLSN             * aMustRedoToLSN,
                                     SChar            ** aDBFileName )
{
    sddDataFileHdr      sDBFileHdr;
    sddDataFileNode   * sFileNode;
    sddTableSpaceNode * sSpaceNode;
    smLSN               sMustRedoToLSN;
    SChar             * sDBFileName = NULL;
    UInt                i;

    IDE_DASSERT( aSpaceNode     != NULL );
    IDE_DASSERT( aMustRedoToLSN != NULL );
    IDE_DASSERT( aDBFileName    != NULL );

    *aDBFileName = NULL;
    SM_LSN_INIT( sMustRedoToLSN );

    sSpaceNode = (sddTableSpaceNode *)aSpaceNode;

    for (i=0; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            continue;
        }

        // ����Ÿ������ �����ϴ��� �˻�
        IDE_TEST_RAISE( idf::access( sFileNode->mName, F_OK|W_OK|R_OK )
                        != 0 , err_file_not_found );

        // DBFileHdr�� DBFile���� �о�´�.
        IDE_TEST_RAISE( readDBFHdr( aStatistics,
                                    sFileNode,
                                    &sDBFileHdr ) != IDE_SUCCESS,
                        err_dbfilehdr_read_failure );

        if ( smLayerCallback::isLSNGT( &sDBFileHdr.mMustRedoToLSN,
                                       &sMustRedoToLSN )
             == ID_TRUE )
        {
            SM_GET_LSN( sMustRedoToLSN, sDBFileHdr.mMustRedoToLSN );
            sDBFileName = idlOS::strrchr( sFileNode->mName,
                                          IDL_FILE_SEPARATOR );
        }
    }

    SM_GET_LSN( *aMustRedoToLSN, sMustRedoToLSN );
    *aDBFileName = sDBFileName + 1; // '/' �����ϰ� ��ȯ

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath,
                                  sFileNode->mFile.getFileName() ) );
    }
    IDE_EXCEPTION( err_dbfilehdr_read_failure );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_Datafile_Header_Read_Failure,
                                  sFileNode->mFile.getFileName() ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
  ����Ÿ������ ������ ������ ��ȯ�Ѵ� .
*/
IDE_RC  sddDiskMgr::getPageRangeInFileByID( idvSQL          * aStatistics,
                                            scSpaceID         aSpaceID,
                                            UInt              aFileID,
                                            scPageID        * aFstPageID,
                                            scPageID        * aLstPageID )
{
    UInt                sState = 0;
    sddTableSpaceNode * sSpaceNode;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceID ) == ID_TRUE );
    IDE_DASSERT( aFstPageID != NULL );
    IDE_DASSERT( aLstPageID != NULL );

    IDE_TEST( sctTableSpaceMgr::findAndLockSpaceNodeBySpaceID( aStatistics,
                                                               aSpaceID,
                                                               (void**)&sSpaceNode )
                  != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sddTableSpace::getPageRangeInFileByID( sSpaceNode,
                                                     aFileID,
                                                     aFstPageID,
                                                     aLstPageID)
              != IDE_SUCCESS );

    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( sSpaceNode );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aSpaceNode�� ��ũ Tablesapce��� Tablespace�� ����
 *               �ִ� ��� Datafile�� Max Open FD Count�� aMaxFDCnt4File
 *               �� �����Ѵ�.
 *
 * aSpaceNode     - [IN] Space Node Pointer
 * aMaxFDCnt4File - [IN] Max FD Count
 **********************************************************************/
IDE_RC  sddDiskMgr::setMaxFDCnt4AllDFileOfTBS( sctTableSpaceNode* aSpaceNode,
                                               UInt               aMaxFDCnt4File )
{
    sddTableSpaceNode*       sSpaceNode;
    sddDataFileNode*         sFileNode;
    sdFileID                 sFileID;
    UInt                     sState = 0;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    sctTableSpaceMgr::lockSpaceNode( NULL /* idvSQL* */,
                                     aSpaceNode );
    sState = 1;

    // 1. ���̺����̽��� DISCARD�� ��� üũ���� �ʴ´�.
    // 2. ���̺����̽��� ������ ��� üũ���� �ʴ´�.
    IDE_TEST_CONT( sctTableSpaceMgr::hasState( aSpaceNode,
                                                SCT_SS_SKIP_IDENTIFY_DB )
                    == ID_TRUE, err_tbs_skip );

    sSpaceNode = (sddTableSpaceNode*)aSpaceNode;

    for( sFileID = 0; sFileID < sSpaceNode->mNewFileID; sFileID++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[ sFileID ];

        if( sFileNode == NULL )
        {
            // ������ Drop�� ���.
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            // ����Ÿ������ ������ ��� �����Ѵ�.
            continue;
        }

        IDE_TEST( sddDataFile::setMaxFDCount( sFileNode,
                                              aMaxFDCnt4File )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( err_tbs_skip );
    
    sState = 0;
    sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    return IDE_FAILURE;
}

/*
  Offline TBS�� ���Ե� Table���� Runtime Index header����
  ��� free �Ѵ�.

  [ ���� ]
  Restart Reocvery ������ Undo All �Ϸ� ���Ŀ� ȣ��ȴ�.

  [ ���� ]
  [IN] aSpaceNode - TBS ���

 */
IDE_RC sddDiskMgr::finiOfflineTBSAction( idvSQL            * aStatistics ,
                                         sctTableSpaceNode * aSpaceNode,
                                         void              * /* aActionArg */ )
{
    IDE_DASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // Tablespace�� ���°� Offline�̸�
        if (SMI_TBS_IS_OFFLINE(aSpaceNode->mState) )
        {
            // Table�� Runtime Item  �� Runtime Index Header�� �����Ѵ�.
            IDE_TEST( smLayerCallback::alterTBSOffline4Tables( aStatistics,
                                                               aSpaceNode->mID )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To do..
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*

  RESIZE DBF �� ���� Shrink Pending ������ �����Ѵ�.

  [IN] aSpaceNode : TBS Node
  [IN] aPendingOp : Pending ����ü

*/
IDE_RC sddDiskMgr::shrinkFilePending( idvSQL            * aStatistics,
                                      sctTableSpaceNode * /*aSpaceNode*/,
                                      sctPendingOp      * aPendingOp  )
{
    sddDataFileNode   * sFileNode;
    scPageID            sFstPID;
    scPageID            sLstPID;
    UInt                sPreparedIO = 0;

    IDE_DASSERT( aPendingOp != NULL );
    IDE_DASSERT( aPendingOp->mPendingOpParam != NULL );

    sFileNode  = (sddDataFileNode*)aPendingOp->mPendingOpParam;

    // ���ۻ� ����� ���� �������� ��ȿȭ ��Ų��.
    // GC�� �������� ���ϵ��� �Ѵ�.
    IDE_TEST( getPageRangeInFileByID( aStatistics,
                                      sFileNode->mSpaceID,
                                      sFileNode->mID,
                                      &sFstPID,
                                      &sLstPID ) != IDE_SUCCESS );

    sFstPID = SD_CREATE_PID( sFileNode->mID, aPendingOp->mResizeSizeWanted );

    IDE_TEST( smLayerCallback::discardPagesInRangeInSBuffer(
                                      aStatistics,
                                      sFileNode->mSpaceID,
                                      sFstPID,
                                      sLstPID )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::discardPagesInRange(
                                      aStatistics,
                                      sFileNode->mSpaceID,
                                      sFstPID,
                                      sLstPID )
              != IDE_SUCCESS );

    IDE_TEST( prepareIO( aStatistics,
                         sFileNode ) != IDE_SUCCESS );
    sPreparedIO = 1;

    IDE_TEST( sddDataFile::truncate( sFileNode,
                                     aPendingOp->mResizeSizeWanted )
              != IDE_SUCCESS );

    sddDataFile::setCurrSize( sFileNode, aPendingOp->mResizeSizeWanted );

    if( aPendingOp->mResizeSizeWanted < sFileNode->mInitSize )
    {
        sddDataFile::setInitSize(sFileNode,
                                 aPendingOp->mResizeSizeWanted);
    }

    sPreparedIO = 0;
    IDE_TEST( completeIO( aStatistics,
                          sFileNode,
                          SDD_IO_WRITE ) != IDE_SUCCESS );

    /* loganchor flush */
    IDE_ASSERT( smLayerCallback::updateDBFNodeAndFlush( sFileNode )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sPreparedIO != 0 )
    {
        IDE_ASSERT( completeIO( aStatistics,
                                sFileNode,
                                SDD_IO_WRITE ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/* --------------------------------------------------------------------
 * PROJ-2118 BUG Reporting
 *
 * Description : Disk File���� Page�� ������ ������ trace file�� �ѷ��ش�.
 *
 * ----------------------------------------------------------------- */
IDE_RC sddDiskMgr::tracePageInFile( UInt            aChkFlag,
                                    ideLogModule    aModule,
                                    UInt            aLevel,
                                    scSpaceID       aSpaceID,
                                    scPageID        aPageID,
                                    const SChar   * aTitle )
{
    UInt         sDummyFileID;
    smLSN        sDummyLSN;
    smuAlignBuf  sPagePtr;
    UInt         sState = 0;

    IDE_TEST( smuUtility::allocAlignedBuf( IDU_MEM_SM_SDD,
                                           SD_PAGE_SIZE,
                                           SD_PAGE_SIZE,
                                           &sPagePtr )
              != IDE_SUCCESS );
    sState = 1;

    SM_LSN_INIT( sDummyLSN );

    IDE_TEST( read( NULL,
                    aSpaceID,
                    aPageID,
                    sPagePtr.mAlignPtr,
                    &sDummyFileID ) != IDE_SUCCESS );

    if( aTitle != NULL )
    {
        (void)smLayerCallback::tracePage( aChkFlag,
                                          aModule,
                                          aLevel,
                                          sPagePtr.mAlignPtr,
                                          "%s ( "
                                          "Space ID : %"ID_UINT32_FMT", "
                                          "Page ID : %"ID_UINT32_FMT" ):",
                                          aTitle,
                                          aSpaceID,
                                          aPageID );
    }
    else
    {
        (void)smLayerCallback::tracePage( aChkFlag,
                                          aModule,
                                          aLevel,
                                          sPagePtr.mAlignPtr,
                                          "Dump Disk Page In Data File ( "
                                          "Space ID : %"ID_UINT32_FMT", "
                                          "Page ID : %"ID_UINT32_FMT" ):",
                                          aSpaceID,
                                          aPageID );
    }

    sState = 0;
    IDE_TEST( smuUtility::freeAlignedBuf( &sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( smuUtility::freeAlignedBuf( &sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/* --------------------------------------------------------------------
 * BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�. 
 *           pageid �������� filename �˾ƿ��� ��� �߰�
 * ----------------------------------------------------------------- */
SChar* sddDiskMgr::getFileName( scSpaceID aSpaceID,
                                scPageID  aPageID )
{
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sDataFileNode;
    SChar             * sFileName = NULL;
    UInt                sFileIndex = SD_MAKE_FID(aPageID) ;
        
    if ( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID, 
                                                   (void**)&sSpaceNode )
              == IDE_SUCCESS )
    {
        if ( sSpaceNode != NULL )
        {
            if ( sFileIndex < sSpaceNode->mNewFileID )
            {
                sDataFileNode = sSpaceNode->mFileNodeArr[ sFileIndex ];

                if ( sDataFileNode != NULL )
                {
                    if ( sDataFileNode->mName != NULL )
                    {
                        sFileName = idlOS::strrchr( sDataFileNode->mName,
                                                IDL_FILE_SEPARATOR ) ;
                    }
                }
            }
        }
    }
    return sFileName;
}
