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
 * $Id: sddTableSpace.cpp 86110 2019-09-02 04:52:04Z et16 $
 *
 * Description :
 *
 * �� ������ ��ũ�������� tablespace ��忡 ���� ���������̴�.
 *
 * 1. tablespace�� ����
 *
 *   - ��ũ�������� mutex ȹ��
 *   - sddDiskMgr::createTableSpace
 *     : ��ũ�����ڿ� ���� tablespace ��� �޸� �Ҵ��Ѵ�.
 *     + sddTableSpace::initialize �ʱ�ȭ
 *       : tablespace ���� ����
 *     + sddTableSpace::createDataFile
 *       : datafile ���� �� �ʱ�ȭ, datafile ����Ʈ�� �߰�
 *   - ��ũ�������� datafile ���°� ���ÿ� datafile ����Ʈ LRU���� �߰�
 *   - ��ũ�������� mutex Ǯ��
 *
 * 2. tablespace�� �ε�
 *
 *   - �α׾�Ŀ�κ��� tablespace ������ sddTableSpaceAttr�� ����
 *   - ��ũ�������� mutex ȹ��
 *   - ��ũ�����ڿ� ���� tablespace ��带 ����
 *   - sddTableSpaceNode::initialize �ʱ�ȭ
 *   - ��ũ�������� createDataFileNode�� ����
 *     sddTableSpaceNode::attachDataFile�� �ݺ�ȣ��
 *     (sddTableSpaceNode�� datafile ��带 ������ ��, datafile ����Ʈ�� �߰�)
 *   - ��ũ�������� datafile���°� ���ÿ� datafile LRU ����Ʈ���� �߰�
 *   - ��ũ�Ŵ����� mutex Ǯ��
 *
 * 3. tablespace�� ����
 *
 *   - ��ũ�������� mutex ȹ��
 *   - sddDiskMgr::removeTableSpace
 *     : ��ũ�����ڿ��� �ش� ���̺����̽��� datafile(s)�鿡 ���Ͽ�
 *       close �԰� ���ÿ� datafile LRU ����Ʈ���� ���� �� tablespace ��� ����
 *     + tablespace�� drop��忡 ���� sddTableSpace::closeDataFile �Ǵ�
 *       sddTableSpace::dropDataFile �ݺ�����
 *     + sddTableSpace::destory�� �����Ͽ� datafile ����Ʈ��
 *       ���� �޸� ����
 *   - ��ũ�Ŵ����� ���� TableSpace Node ����
 *   - ��ũ�Ŵ����� mutex Ǯ��
 *
 *
 * 4. tablespace�� ����
 *   - datafile �߰� �� ����(?)
 *   - tablespace�� ��� ������ ������ �Ӽ� ����
 *
 *
 **********************************************************************/
#include <idl.h> // for win32

#include <sddReq.h>
#include <smErrorCode.h>
#include <sddTableSpace.h>
#include <sddDataFile.h>
#include <sddDiskMgr.h>
#include <sddReq.h>
#include <sctTableSpaceMgr.h>
#include <smriChangeTrackingMgr.h>

/***********************************************************************
 * Description : tablespace ��� �ʱ�ȭ
 *
 * tablespace ��忡 ������ ���� �������� �Ӽ��� �����Ѵ�.
 * - tablespace��, tablespace ID, tablespace Ÿ��, max page ����,
 * online/offline ����, datafile �������� ������ �����Ѵ�.
 *
 * + 2nd. code design
 *   - tablespace ID ����
 *   - tablespace Ÿ��
 *   - tablespace ��
 *   - tablespace�� online/offline ����
 *   - tablespace�� ������ datafile�� ����
 *   - ������ datafile�� ���� ����Ʈ
 *   - tableSpace�� ���Ե� ��� page�� ����
 *   - tablespace�� ���� ����Ʈ
 ***********************************************************************/
IDE_RC sddTableSpace::initialize( sddTableSpaceNode   * aSpaceNode,
                                  smiTableSpaceAttr   * aSpaceAttr )
{
    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aSpaceAttr != NULL );
    IDE_DASSERT( aSpaceAttr->mAttrType == SMI_TBS_ATTR );
    IDE_DASSERT( aSpaceNode->mHeader.mLockItem4TBS == NULL );

    // Tablespace Node �ʱ�ȭ ( Disk/Memory ���� )
    IDE_TEST( sctTableSpaceMgr::initializeTBSNode( & aSpaceNode->mHeader,
                                                   aSpaceAttr )
              != IDE_SUCCESS );

    // �Ӽ� �÷��� �ʱ�ȭ
    aSpaceNode->mAttrFlag = aSpaceAttr->mAttrFlag;

    //tablespace�� ������ datafile �迭�ʱ�ȭ�� ���� ����.
    idlOS::memset( aSpaceNode->mFileNodeArr,
                   0x00,
                   ID_SIZEOF(aSpaceNode->mFileNodeArr) );
    aSpaceNode->mDataFileCount = 0;

    /* tableSpace�� ���Ե� ��� page�� ���� */
    aSpaceNode->mTotalPageCount  = 0;
    aSpaceNode->mNewFileID       = aSpaceAttr->mDiskAttr.mNewFileID;

    aSpaceNode->mExtPageCount = aSpaceAttr->mDiskAttr.mExtPageCount;

    /* ���ڷ� ���޵� Extent ��������� �����Ѵ�. */
    aSpaceNode->mExtMgmtType = aSpaceAttr->mDiskAttr.mExtMgmtType;

    /* ���ڷ� ���޵� Segment ��������� �����Ѵ�. */
    aSpaceNode->mSegMgmtType = aSpaceAttr->mDiskAttr.mSegMgmtType;

    /* Space Cache�� ���� Page Layer �ʱ�ȭ�� �Ҵ�Ǿ� �����ȴ�. */
    aSpaceNode->mSpaceCache = NULL;

    aSpaceNode->mMaxSmoNoForOffline = 0;

    // PRJ-1548 User Memory Tablespace
    aSpaceNode->mAnchorOffset = SCT_UNSAVED_ATTRIBUTE_OFFSET;

    // fix BUG-17456 Disk Tablespace online���� update �߻��� index ���ѷ���
    SM_LSN_INIT( aSpaceNode->mOnlineTBSLSN4Idx );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : tablespace ����� ��� �ڿ� ����
 *
 * ��忡 ���� datafile ����Ʈ�� �����ϰ� �׿� �Ҵ�Ǿ���
 * �޸𸮸� �����Ѵ�. �� �Լ��� ȣ��Ǳ����� ��ũ��������
 * mutex�� ȹ��� ���¿��� �Ѵ�.
 *
 * + 2nd. code design
 *   - tablespace ����� ��� datafile ��带 ��忡 ����
 *     �����Ѵ�.
 *   - tablespace �̸��� �Ҵ�Ǿ��� �޸𸮸� �����Ѵ�.
 ***********************************************************************/
IDE_RC sddTableSpace::destroy( sddTableSpaceNode* aSpaceNode )
{

    IDE_TEST( iduMemMgr::free(aSpaceNode->mHeader.mName) != IDE_SUCCESS );

    if( aSpaceNode->mHeader.mLockItem4TBS != NULL )
    {
        IDE_TEST( smLayerCallback::destroyLockItem( aSpaceNode->mHeader.mLockItem4TBS )
                  != IDE_SUCCESS );
        IDE_TEST( smLayerCallback::freeLockItem( aSpaceNode->mHeader.mLockItem4TBS )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : datafile ��� �� datafile ����(������)
 *
 * - DDL���� ���� tablespace�� datafile ���Ӹ� �ƴ϶� �����Ǵ�
 * datafile�鵵 �����Ѵ�. ����, �� �Լ��� ����Ǵ� ���ȿ��� ��������
 * ��ũ�����ڿ� tablespace�� ��ϵ��� ���� ��Ȳ�̹Ƿ�,
 * mutex�� ȹ���� �ʿ䵵 ����.
 *
 * - �α׾�Ŀ�� ���� �ʱ�ȭ�� datafile ��常 ����
 * �α׾�Ŀ�κ��� �ʱ�ȭ ������ �о� tablespace�� �Ѱ��� datafile
 * ��常 �����Ѵ�. �̴� �α׾�Ŀ�� ����Ǿ� �ִ� datafile ���
 * ������ �ش��ϴ� datafile���� �̹� �����ϱ� �����̴�.
 *
 * - �ϳ��� datafile create�� �ش� �̸��� datafile�� �̹�
 * �����ϴ��� �˻��ؾ� �Ѵ�.
 *
 * + 2nd. code design
 *   - for ( attribute ���� ��ŭ )
 *     {
 *        datafile ��带 ���� �޸� �Ҵ��Ѵ�.
 *        datafile ��带 �ʱ�ȭ�Ѵ�.
 *        ���� datafile�� �����Ѵ�.
 *        base ��忡 datafile ����Ʈ�� datafile ��带 �߰��Ѵ�.
 *     }
 ***********************************************************************/
IDE_RC sddTableSpace::createDataFiles(
                            idvSQL             * aStatistics,
                            void               * aTrans,
                            sddTableSpaceNode  * aSpaceNode,
                            smiDataFileAttr   ** aFileAttr,
                            UInt                 aFileAttrCnt,
                            smiTouchMode         aTouchMode,
                            UInt                 aMaxDataFilePageCount )
{
    UInt                i       = 0;
    UInt                j       = 0;
    UInt                sState  = 0;
    sddDataFileNode   * sFileNode      = NULL;
    sddDataFileNode  ** sFileNodeArray = NULL;
    smLSN               sCreateLSN;
    smLSN               sMustRedoToLSN;
    idBool              sCheckPathVal;
    idBool            * sFileArray          = NULL;
    idBool              sDataFileDescState  = ID_FALSE;
    SInt                sCurFileNodeState   = 0;
    smiDataFileDescSlotID     * sSlotID     = NULL;

    IDE_DASSERT( aFileAttr      != NULL );
    IDE_DASSERT( aFileAttrCnt    > 0 );

    // ��ũ�������� mMaxDataFileSize
    IDE_DASSERT( aMaxDataFilePageCount  > 0 );
    IDE_DASSERT( aSpaceNode             != NULL );
    IDE_DASSERT( (aTouchMode == SMI_ALL_NOTOUCH) ||
                 (aTouchMode == SMI_EACH_BYMODE) );

    SM_LSN_INIT( sCreateLSN );
    SM_LSN_INIT( sMustRedoToLSN );

    /* sddTableSpace_createDataFiles_calloc_FileNodeArray.tc */
    IDU_FIT_POINT("sddTableSpace::createDataFiles::calloc::FileNodeArray");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SDD,
                                aFileAttrCnt,
                                ID_SIZEOF(sddDataFileNode *),
                                (void**)&sFileNodeArray,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 1;

    /* sddTableSpace_createDataFiles_calloc_FileArray.tc */
    IDU_FIT_POINT("sddTableSpace::createDataFiles::calloc::FileArray");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SDD,
                                aFileAttrCnt,
                                ID_SIZEOF(idBool),
                                (void**)&sFileArray,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 2;

    for (i = 0; i < aFileAttrCnt; i++)
    {
        sCurFileNodeState = 0;

        /* ====================================
         * [1] ����Ÿ ȭ�� ��� �Ҵ�
         * ==================================== */

        /* sddTableSpace_createDataFiles_malloc_FileNode.tc */
        IDU_FIT_POINT("sddTableSpace::createDataFiles::malloc::FileNode");
        IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SDD,
                                    ID_SIZEOF(sddDataFileNode),
                                    (void**)&sFileNode,
                                    IDU_MEM_FORCE) != IDE_SUCCESS );
        sCurFileNodeState = 1;

        idlOS::memset(sFileNode, 0x00, ID_SIZEOF(sddDataFileNode));

        sFileNodeArray[i] = sFileNode;

        sCheckPathVal = (aTouchMode == SMI_ALL_NOTOUCH) ?
                        ID_FALSE: ID_TRUE;

        /* ====================================
         * [2] ����Ÿ ȭ�� ID �ο�
         * ==================================== */
        if (aTouchMode != SMI_ALL_NOTOUCH)
        {
            aFileAttr[i]->mID = getNewFileID( aSpaceNode );
        }

        /* ====================================
         * [3] ����Ÿ ȭ�� �ʱ�ȭ
         * ==================================== */
        IDE_TEST( sddDataFile::initialize(aSpaceNode->mHeader.mID,
                                          sFileNode,
                                          aFileAttr[i],
                                          aMaxDataFilePageCount,
                                          sCheckPathVal)
                  != IDE_SUCCESS );
        sCurFileNodeState = 2;

        /* ====================================
         * [4] ������ ����Ÿ ȭ�� ����/reuse
         * ==================================== */

        switch(aTouchMode)
        {
            case SMI_ALL_NOTOUCH :

                ///////////////////////////////////////////////////
                // ���������ÿ� Loganchor�κ��� ��ũ������ �ʱ�ȭ
                ///////////////////////////////////////////////////

                IDE_ASSERT( aTrans == NULL );

                // ���� ���̳ʸ� ������ ����Ÿ��������� �����Ѵ�.
                sFileNode->mDBFileHdr.mSmVersion =
                    smLayerCallback::getSmVersionIDFromLogAnchor();

                // DiskRedoLSN, DiskCreateLSN��
                // ����Ÿ��������� �����Ѵ�.
                sddDataFile::setDBFHdr( &(sFileNode->mDBFileHdr),
                                        sctTableSpaceMgr::getDiskRedoLSN(),
                                        &aFileAttr[i]->mCreateLSN,
                                        &sMustRedoToLSN,
                                        &aFileAttr[i]->mDataFileDescSlotID );
                break;

            // for create tablespace, add datafile - reuse phase
            case SMI_EACH_BYMODE :

                ///////////////////////////////////////////////////
                // ����Ÿ���� ���� ����
                ///////////////////////////////////////////////////

                IDE_ASSERT( aTrans != NULL );

                if ( sFileNode->mCreateMode == SMI_DATAFILE_CREATE )
                {
                    IDE_TEST_RAISE( idf::access( sFileNode->mName, F_OK ) == 0,
                                    error_already_exist_datafile );
                }

                IDE_TEST( smLayerCallback::writeLogCreateDBF( aStatistics,
                                                              aTrans,
                                                              aSpaceNode->mHeader.mID,
                                                              sFileNode,
                                                              aTouchMode,
                                                              aFileAttr[ i ], /* PROJ-1923 */
                                                              &sCreateLSN )
                          != IDE_SUCCESS );

                sFileNode->mState = SMI_FILE_CREATING | SMI_FILE_ONLINE;

                // !!CHECK RECOVERY POINT

                // ���� ���̳ʸ� ������ ����Ÿ��������� �����Ѵ�.
                sFileNode->mDBFileHdr.mSmVersion = smVersionID;

                //PROJ-2133 incremental backup 
                //change tracking���Ͽ� ���������ϵ��
                // temptablespace �� ���ؼ��� ����� �ʿ䰡 �����ϴ�.
                if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) &&
                     ( sctTableSpaceMgr::isTempTableSpace( aSpaceNode )
                       != ID_TRUE ) )
                {
                    IDE_TEST( smriChangeTrackingMgr::addDataFile2CTFile( 
                                                      aSpaceNode->mHeader.mID,
                                                      aFileAttr[i]->mID,
                                                      SMRI_CT_DISK_TBS,
                                                      &sSlotID )
                              != IDE_SUCCESS );

                    sDataFileDescState = ID_TRUE;
                }
                else
                {
                    //nothing to do
                }

                // �ֱ� ����� üũ����Ʈ������ ��
                // ����Ÿ��������� �����Ѵ�.
                // DiskRedoLSN, DiskCreateLSN��
                // ����Ÿ��������� �����Ѵ�.
                // CreateLSN�� SCT_UPDATE_DRDB_CREATE_DBF �α��� LSN
                // ��������� �����Ѵ�.
                sddDataFile::setDBFHdr( &(sFileNode->mDBFileHdr),
                                        sctTableSpaceMgr::getDiskRedoLSN(),
                                        &sCreateLSN,
                                        &sMustRedoToLSN,
                                        sSlotID );

                //PRJ-1149 , ���� ���� ������ LSN����� �Ѵ�.
                if(sFileNode->mCreateMode == SMI_DATAFILE_REUSE)
                {
                    /* BUG-32950 when creating tablespace with reuse clause,
                     *           can not check the same file.
                     * �̹� ���� �̸��� ���� ������ ������ �߰��ߴ���
                     * Ȯ���Ѵ�.
                     * 0��° ���� i - 1 ��°���� i�� ���Ѵ�. */
                    for ( j = 0; j < i; j++ )
                    {
                        IDE_TEST_RAISE(
                                idlOS::strcmp( sFileNodeArray[j]->mName,
                                               sFileNodeArray[i]->mName ) == 0,
                                error_already_exist_datafile );
                    }

                    IDE_TEST( sddDataFile::reuse( aStatistics,
                                                  sFileNode )
                              != IDE_SUCCESS );


                }
                else
                {
                    IDE_TEST( sddDataFile::create( aStatistics,
                                                   sFileNode )
                              != IDE_SUCCESS );
                    sFileArray[i] = ID_TRUE;
                }
                break;
            default :
                IDE_ASSERT(0);
                break;
        }
        /* ====================================
         * [5] ����Ÿ ȭ�� ��� ����Ʈ�� ����
         * ==================================== */
        addDataFileNode(aSpaceNode, sFileNode);
    }

    /*BUG-16197: Memory�� ��*/
    sState = 1;
    IDE_TEST( iduMemMgr::free( sFileArray ) != IDE_SUCCESS );
    sFileArray = NULL;

    sState = 0;
    IDE_TEST( iduMemMgr::free( sFileNodeArray ) != IDE_SUCCESS );
    sFileNodeArray = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_exist_datafile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistFile, sFileNode->mName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch (sState)
        {
            case 2:
                for( j = 0; j < aFileAttrCnt; j++ )
                {
                    if( sFileNodeArray[j] != NULL )
                    {
                        if( (sFileArray != NULL) && (sFileArray[j] == ID_TRUE) )
                        {
                            /*
                             * BUG-26215 
                             *  [SD]create tablespace�� ������ ������ autoextend on����
                             *  �����ǰ� ũ�⸦ property�� max������ ũ�� �Է��ϸ�
                             *  �����쿡�� ������ �����Ҽ� ����.
                             */
                            if( idf::access( sFileNodeArray[j]->mName, F_OK ) == 0 )
                            {
                                (void)idf::unlink(sFileNodeArray[j]->mName);
                            }
                        }

                        /* BUG-18176
                           ������ ������ DataFileNode�� ����
                           sddDataFile::initialize()���� �Ҵ��� �ڿ��� ��������� �Ѵ�.
                           i�� �Ҵ��� Ƚ���̱� ������
                           i���� ���� �� ������ �����ؾ� �ϰ�
                           i��°�� ���ؼ��� sCurFileNodeState�� �Ҵ� ������ �Ǵ��Ѵ�. */
                        if (j < i)
                        {
                            IDE_ASSERT( sddDataFile::destroy( sFileNodeArray[j] ) == IDE_SUCCESS );
                            IDE_ASSERT( iduMemMgr::free( sFileNodeArray[j] ) == IDE_SUCCESS );
                        }
                        else if (j == i)
                        {
                            switch (sCurFileNodeState)
                            {
                                case 2:
                                    IDE_ASSERT( sddDataFile::destroy( sFileNodeArray[j] ) == IDE_SUCCESS );
                                case 1:
                                    IDE_ASSERT( iduMemMgr::free( sFileNodeArray[j] ) == IDE_SUCCESS );
                                case 0:
                                    break;
                                default:
                                    IDE_ASSERT(0);
                                    break;
                            }
                        }
                        else
                        {
                            /* j �� i���� ũ�ٸ� �� ������ �� �ʿ䰡 ����. */
                            break;
                        }
                    }
                }
                IDE_ASSERT( iduMemMgr::free(sFileArray) == IDE_SUCCESS );
            case 1:
                IDE_ASSERT( iduMemMgr::free(sFileNodeArray) == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();

    if( sDataFileDescState == ID_TRUE )
    {
        IDE_ASSERT( smriChangeTrackingMgr::deleteDataFileFromCTFile( sSlotID ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 * Description : datafile ��� �� datafile ����(redo only)
 *
 * - restart recovery ���� redo ���̹Ƿ�, mutex�� ȹ���� �ʿ䵵 ����.
 *
 * - �α׾�Ŀ�� ���� �ʱ�ȭ�� datafile ��常 ����
 * �α׾�Ŀ�κ��� �ʱ�ȭ ������ �о� tablespace�� �Ѱ��� datafile
 * ��常 �����Ѵ�. �̴� �α׾�Ŀ�� ����Ǿ� �ִ� datafile ���
 * ������ �ش��ϴ� datafile���� �̹� �����ϱ� �����̴�.
 *
 * - �ϳ��� datafile create�� �ش� �̸��� datafile�� �̹�
 * �����ϴ��� �˻��ؾ� �Ѵ�.
 *
 * + 2nd. code design
 *   - for ( attribute ���� ��ŭ )
 *     {
 *        datafile ��带 ���� �޸� �Ҵ��Ѵ�.
 *        datafile ��带 �ʱ�ȭ�Ѵ�.
 *        ���� datafile�� �����Ѵ�.
 *        base ��忡 datafile ����Ʈ�� datafile ��带 �߰��Ѵ�.
 *     }
 ***********************************************************************/
IDE_RC sddTableSpace::createDataFile4Redo(
                            idvSQL            * aStatistics,
                            void              * aTrans,
                            sddTableSpaceNode * aSpaceNode,
                            smiDataFileAttr   * aFileAttr,
                            smLSN               aCurLSN,
                            smiTouchMode        aTouchMode,
                            UInt                aMaxDataFilePageCount )
{
    UInt                    i       = 0;
    UInt                    j       = 0;
    UInt                    sState  = 0;
    sddDataFileNode       * sFileNode       = NULL;
    sddDataFileNode      ** sFileNodeArray  = NULL;
    smLSN                   sCreateLSN;
    smLSN                   sMustRedoToLSN;
    idBool                  sCheckPathVal;
    idBool                * sFileArray          = NULL;
    idBool                  sDataFileDescState  = ID_FALSE;
    SInt                    sCurFileNodeState   = 0;
    smiDataFileDescSlotID * sSlotID             = NULL;
    smFileID                sNewFileID          = 0;

    IDE_DASSERT( aFileAttr      != NULL );

    // ��ũ�������� mMaxDataFileSize
    IDE_DASSERT( aMaxDataFilePageCount  > 0 );
    IDE_DASSERT( aSpaceNode             != NULL );
    IDE_DASSERT( (aTouchMode == SMI_ALL_NOTOUCH) ||
                 (aTouchMode == SMI_EACH_BYMODE) );

    SM_LSN_INIT( sCreateLSN );
    SM_LSN_INIT( sMustRedoToLSN );

    /* sddTableSpace_createDataFile4Redo_calloc_FileNodeArray.tc */
    IDU_FIT_POINT("sddTableSpace::createDataFile4Redo::calloc::FileNodeArray");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SDD,
                                1,
                                ID_SIZEOF(sddDataFileNode *),
                                (void**)&sFileNodeArray,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 1;

    /* sddTableSpace_createDataFile4Redo_calloc_FileArray.tc */
    IDU_FIT_POINT("sddTableSpace::createDataFile4Redo::calloc::FileArray");
    IDE_TEST( iduMemMgr::calloc(IDU_MEM_SM_SDD,
                                1,
                                ID_SIZEOF(idBool),
                                (void**)&sFileArray,
                                IDU_MEM_FORCE)
              != IDE_SUCCESS );
    sState = 2;

    sCurFileNodeState = 0;

    /* ====================================
     * [1] ����Ÿ ȭ�� ��� �Ҵ�
     * ==================================== */
    /* sddTableSpace_createDataFile4Redo_malloc_FileNode.tc */
    IDU_FIT_POINT("sddTableSpace::createDataFile4Redo::malloc::FileNode");
    IDE_TEST( iduMemMgr::malloc(IDU_MEM_SM_SDD,
                                ID_SIZEOF(sddDataFileNode),
                                (void**)&sFileNode,
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sCurFileNodeState = 1;

    idlOS::memset(sFileNode, 0x00, ID_SIZEOF(sddDataFileNode));

    sFileNodeArray[i] = sFileNode;

    sCheckPathVal = (aTouchMode == SMI_ALL_NOTOUCH) ?
        ID_FALSE: ID_TRUE;

    /* ====================================
     * [2] ����Ÿ ȭ�� ID �ο�
     * ==================================== */
    sNewFileID = aSpaceNode->mNewFileID;

    IDE_TEST( sNewFileID != aFileAttr->mID );
    ideLog::log( IDE_SM_0,
                 "redo createDataFile, FileID in Spacenode : %"ID_UINT32_FMT""
                 ", FileID in Log : %"ID_UINT32_FMT" ",
                 sNewFileID, aFileAttr->mID );

    if (aTouchMode != SMI_ALL_NOTOUCH)
    {
        aFileAttr->mID = getNewFileID( aSpaceNode );
    }

    /* ====================================
     * [3] ����Ÿ ȭ�� �ʱ�ȭ
     * ==================================== */
    IDE_TEST( sddDataFile::initialize(aSpaceNode->mHeader.mID,
                                      sFileNode,
                                      aFileAttr,
                                      aMaxDataFilePageCount,
                                      sCheckPathVal)
              != IDE_SUCCESS );
    sCurFileNodeState = 2;

    /* ====================================
     * [4] ������ ����Ÿ ȭ�� ����/reuse
     * ==================================== */

    switch(aTouchMode)
    {
        case SMI_ALL_NOTOUCH :

            ///////////////////////////////////////////////////
            // ���������ÿ� Loganchor�κ��� ��ũ������ �ʱ�ȭ
            ///////////////////////////////////////////////////

            IDE_ASSERT( aTrans == NULL );

            // ���� ���̳ʸ� ������ ����Ÿ��������� �����Ѵ�.
            sFileNode->mDBFileHdr.mSmVersion =
                smLayerCallback::getSmVersionIDFromLogAnchor();

            // DiskRedoLSN, DiskCreateLSN��
            // ����Ÿ��������� �����Ѵ�.
            sddDataFile::setDBFHdr( &(sFileNode->mDBFileHdr),
                                    sctTableSpaceMgr::getDiskRedoLSN(),
                                    &aFileAttr->mCreateLSN,
                                    &sMustRedoToLSN,
                                    &aFileAttr->mDataFileDescSlotID );
            break;

            // for create tablespace, add datafile - reuse phase
        case SMI_EACH_BYMODE :

            ///////////////////////////////////////////////////
            // ����Ÿ���� ���� ����
            ///////////////////////////////////////////////////

            IDE_ASSERT( aTrans != NULL );

            if(sFileNode->mCreateMode == SMI_DATAFILE_CREATE)
            {
                IDE_TEST_RAISE( idf::access( sFileNode->mName, F_OK) == 0,
                                error_already_exist_datafile );
            }

            SM_GET_LSN( sCreateLSN, aCurLSN );
            SM_GET_LSN( aFileAttr->mCreateLSN, aCurLSN );

            sFileNode->mState = SMI_FILE_CREATING | SMI_FILE_ONLINE;

            // ���� ���̳ʸ� ������ ����Ÿ��������� �����Ѵ�.
            sFileNode->mDBFileHdr.mSmVersion = smVersionID;

            //PROJ-2133 incremental backup 
            //change tracking���Ͽ� ���������ϵ��
            if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
            {
                IDE_TEST( smriChangeTrackingMgr::addDataFile2CTFile( 
                        aSpaceNode->mHeader.mID,
                        aFileAttr->mID,
                        SMRI_CT_DISK_TBS,
                        &sSlotID )
                    != IDE_SUCCESS );

                sDataFileDescState = ID_TRUE;
            }
            else
            {
                //nothing to do
            }

            // �ֱ� ����� üũ����Ʈ������ ��
            // ����Ÿ��������� �����Ѵ�.
            // DiskRedoLSN, DiskCreateLSN��
            // ����Ÿ��������� �����Ѵ�.
            // CreateLSN�� SCT_UPDATE_DRDB_CREATE_DBF �α��� LSN
            // ��������� �����Ѵ�.
            sddDataFile::setDBFHdr(
                &(sFileNode->mDBFileHdr),
                sctTableSpaceMgr::getDiskRedoLSN(),
                &sCreateLSN,
                &sMustRedoToLSN,
                sSlotID );

            //PRJ-1149 , ���� ���� ������ LSN����� �Ѵ�.
            if(sFileNode->mCreateMode == SMI_DATAFILE_REUSE)
            {
                /* BUG-32950 when creating tablespace with reuse clause,
                 *           can not check the same file.
                 * �̹� ���� �̸��� ���� ������ ������ �߰��ߴ���
                 * Ȯ���Ѵ�.
                 * 0��° ���� i - 1 ��°���� i�� ���Ѵ�. */
                for ( j = 0; j < i; j++ )
                {
                    IDE_TEST_RAISE(
                        idlOS::strcmp( sFileNodeArray[j]->mName,
                                       sFileNodeArray[i]->mName ) == 0,
                        error_already_exist_datafile );
                }

                IDE_TEST( sddDataFile::reuse( aStatistics,
                                              sFileNode )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( sddDataFile::create( aStatistics,
                                               sFileNode )
                          != IDE_SUCCESS );
                sFileArray[i] = ID_TRUE;
            }
            break;
        default :
            IDE_ASSERT(0);
            break;
    }
    /* ====================================
     * [5] ����Ÿ ȭ�� ��� ����Ʈ�� ����
     * ==================================== */
    addDataFileNode(aSpaceNode, sFileNode);

    /*BUG-16197: Memory�� ��*/
    sState = 1;
    IDE_TEST( iduMemMgr::free( sFileArray ) != IDE_SUCCESS );
    sFileArray = NULL;

    sState = 0;
    IDE_TEST( iduMemMgr::free( sFileNodeArray ) != IDE_SUCCESS );
    sFileNodeArray = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_exist_datafile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistFile, sFileNode->mName));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch (sState)
        {
            case 2:
                if( sFileNodeArray[0] != NULL )
                {
                    if( (sFileArray != NULL) && (sFileArray[0] == ID_TRUE) )
                    {
                        /*
                         * BUG-26215 
                         *  [SD]create tablespace�� ������ ������ autoextend on����
                         *  �����ǰ� ũ�⸦ property�� max������ ũ�� �Է��ϸ�
                         *  �����쿡�� ������ �����Ҽ� ����.
                         */
                        if( idf::access( sFileNodeArray[0]->mName, F_OK ) == 0 )
                        {
                            (void)idf::unlink(sFileNodeArray[0]->mName);
                        }
                    }

                    /* BUG-18176
                       ������ ������ DataFileNode�� ����
                       sddDataFile::initialize()���� �Ҵ��� �ڿ��� ��������� �Ѵ�.
                       i�� �Ҵ��� Ƚ���̱� ������
                       i���� ���� �� ������ �����ؾ� �ϰ�
                       i��°�� ���ؼ��� sCurFileNodeState�� �Ҵ� ������ �Ǵ��Ѵ�. */
                    if (0 < i)
                    {
                        IDE_ASSERT( sddDataFile::destroy( sFileNodeArray[0] ) == IDE_SUCCESS );
                        IDE_ASSERT( iduMemMgr::free( sFileNodeArray[0] ) == IDE_SUCCESS );
                    }
                    else if (0 == i)
                    {
                        switch (sCurFileNodeState)
                        {
                            case 2:
                                IDE_ASSERT( sddDataFile::destroy( sFileNodeArray[0] ) == IDE_SUCCESS );
                            case 1:
                                IDE_ASSERT( iduMemMgr::free( sFileNodeArray[0] ) == IDE_SUCCESS );
                            case 0:
                                break;
                            default:
                                IDE_ASSERT(0);
                                break;
                        }
                    }
                    else
                    {
                        /* 0 �� i���� ũ�ٸ� �� ������ �� �ʿ䰡 ����. */
                        break;
                    }
                }
                IDE_ASSERT( iduMemMgr::free(sFileArray) == IDE_SUCCESS );
            case 1:
                IDE_ASSERT( iduMemMgr::free(sFileNodeArray) == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();

    if( sDataFileDescState == ID_TRUE )
    {
        IDE_ASSERT( smriChangeTrackingMgr::deleteDataFileFromCTFile( sSlotID ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile ���� ����Ʈ�� ��� �߰�
 *
 * tablespace ��忡�� �����ϴ� datafile ���� ����Ʈ��
 * �������� datafile ����� �߰��Ѵ�.
 ***********************************************************************/
void sddTableSpace::addDataFileNode( sddTableSpaceNode*  aSpaceNode,
                                     sddDataFileNode*    aFileNode )
{
    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL );

    aSpaceNode->mFileNodeArr[ aFileNode->mID ] = aFileNode ;
}



void sddTableSpace::removeMarkDataFileNode( sddDataFileNode * aFileNode )
{
    IDE_ASSERT( aFileNode != NULL );

    aFileNode->mState |= SMI_FILE_DROPPING;

    return;
}

/***********************************************************************
 * Description : ��忡 ���� �ϳ��� datafile ��� �� datafile�� ����
 *
 * + 2nd. code design
 *   - if ( SMI_ALL_TOUCH  �̸� )
 *     {
 *         datafile�� �����Ѵ�. -> sddDataFile::delete
 *     }
 *   - touch ��忡 ���� datafile ��带 datafile ���Ḯ��Ʈ���� ����
 *     -> removeDataFileNode
 *   - datafile ��带 �����Ѵ�.
 *
 * + ����Ÿ ȭ���� ������ not-used ������ ȭ�Ͽ� ���ؼ��� �����ϹǷ�
 *   (1) ���� ����� �Ǵ� ȭ���� open�Ǿ� ���� �ʰ�
 *   (2) ���ۿ� fix�� �������� ������ �����Ѵ�.
 ***********************************************************************/
IDE_RC sddTableSpace::removeDataFile( idvSQL              * aStatistics,
                                      void                * aTrans,
                                      sddTableSpaceNode   * aSpaceNode,
                                      sddDataFileNode     * aFileNode,
                                      smiTouchMode          aTouchMode,
                                      idBool                aDoGhostMark )
{
    sctPendingOp      * sPendingOp;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aFileNode  != NULL );
    IDE_DASSERT( aTouchMode == SMI_ALL_NOTOUCH ||
                 aTouchMode == SMI_ALL_TOUCH   ||
                 aTouchMode == SMI_EACH_BYMODE );

    /* ====================================
     * [1] ����Ÿ ȭ�� ������ ���� �α�
     *     ���� ����ÿ��� �α����� ����
     * ==================================== */
    if ( aDoGhostMark == ID_TRUE )
    {
        IDE_ASSERT( aTrans != NULL );

        IDE_TEST( smLayerCallback::writeLogDropDBF(
                      aStatistics,
                      aTrans,
                      aSpaceNode->mHeader.mID,
                      aFileNode,
                      aTouchMode,
                      NULL ) != IDE_SUCCESS );


        /* ====================================
         * [2] ����Ÿ ȭ�� ����Ʈ���� �������� ����
         * ==================================== */
        IDE_TEST( sddDataFile::addPendingOperation(
                    aTrans,
                    aFileNode,
                    ID_TRUE, /* commit�� ���� */
                    SCT_POP_DROP_DBF,
                    &sPendingOp ) != IDE_SUCCESS );


        sPendingOp->mPendingOpFunc  = sddDiskMgr::removeFilePending;
        sPendingOp->mPendingOpParam = (void*)aFileNode;
        sPendingOp->mTouchMode      = aTouchMode;

        removeMarkDataFileNode( aFileNode );
    }
    else
    {
        IDE_TEST( sddDiskMgr::closeDataFile( aStatistics,
                                             aFileNode ) != IDE_SUCCESS );

        IDE_ASSERT( aTouchMode == SMI_ALL_NOTOUCH );
        IDE_ASSERT( sddDataFile::destroy(aFileNode) == IDE_SUCCESS );

        IDE_ASSERT( iduMemMgr::free(aFileNode) == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ��忡 ���� tablespace ����� ���
 *               datafile ��� �� datafile�� ����
 *
 * + 2nd. code design
 *   - while(��� datafile ����Ʈ�� ����)
 *     {
 *        if ( SMI_ALL_TOUCH �̸� )
 *        {
 *            datafile�� �����Ѵ�. -> sddDataFile::delete
 *        }
 *        datafile ��带 datafile ���Ḯ��Ʈ���� ���� -> removeDataFileNode
 *        datafile ��带 �����Ѵ�.
 *        datafile ����� �޸𸮸� �����Ѵ�.
 *     }
 ***********************************************************************/
IDE_RC sddTableSpace::removeAllDataFiles( idvSQL*             aStatistics,
                                          void*               aTrans,
                                          sddTableSpaceNode*  aSpaceNode,
                                          smiTouchMode        aTouchMode,
                                          idBool              aDoGhostMark)
{
    sddDataFileNode *sFileNode;
    UInt i;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aTouchMode == SMI_ALL_NOTOUCH ||
                 aTouchMode == SMI_ALL_TOUCH   ||
                 aTouchMode == SMI_EACH_BYMODE );

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL)
        {
            continue;
        }

        if( aDoGhostMark == ID_TRUE )
        {
            if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                continue;
            }
        }
        else // shutdown
        {
            IDE_DASSERT( aTrans == NULL );
        }

        IDE_TEST( removeDataFile(aStatistics,
                                 aTrans,
                                 aSpaceNode,
                                 sFileNode,
                                 aTouchMode,
                                 aDoGhostMark ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 * Description : datafile ����� remove ���ɿ��� Ȯ��
 *
 * tablespace�� removeDataFile���� ȣ��Ǵ� �Լ��ν� �����ϰ��� �ϴ� datafile
 * �����  on/offline ���ο� ������� usedpagelimit�� ������ ����� ����
 * �����, ���Ű� �����ϴ�.
 ***********************************************************************/
IDE_RC sddTableSpace::canRemoveDataFileNodeByName(
    sddTableSpaceNode* aSpaceNode,
    SChar*             aDataFileName,
    scPageID           aUsedPageLimit,
    sddDataFileNode**  aFileNode )
{

    idBool           sFound;
    sddDataFileNode* sFileNode;
    UInt i ;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );

    sFound     = ID_FALSE;
    *aFileNode = NULL;

    for (i = 0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL)
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ))
        {
            continue;
        }

        if (idlOS::strcmp(sFileNode->mName, aDataFileName) == 0)
        {
            sFound = ID_TRUE;

            /* ------------------------------------------------
             * aUsedPageLimit�� ������ datafile ��� ������
             * datafile ����� ���� ����, �׷��� �ʴٸ� NULL ��ȯ
             * ----------------------------------------------*/
            if ( sFileNode->mID > SD_MAKE_FID( aUsedPageLimit ))
            {
                *aFileNode = sFileNode;
            }
            else
            {
                *aFileNode = NULL;
            }
            break;
        }
    }

     /* �־��� datafile �̸��� ���� */
    IDE_TEST_RAISE( sFound != ID_TRUE, error_not_found_filenode );
    IDE_TEST_RAISE( *aFileNode == NULL, error_cannot_remove_filenode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aDataFileName));
    }
    IDE_EXCEPTION( error_cannot_remove_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_CannotRemoveDataFileNode,
                                aDataFileName));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*
 �ش� �������� �����ϴ� datafile ��� ��ȯ

 [IN]  aSpaceNode  - ���̺����̽� ���
 [IN]  aPageID     - �˻��� ������ID
 [OUT] aFileNode   - �������� �����ϴ� ����Ÿ���� ���
                     ��ȿ���� ���� ��� NULL ��ȯ
 [OUT] aPageOffset - ����Ÿ���� �������� ������ ������
 [OUT] aFstPageID  - ����Ÿ������ ù���� ������ID
 [IN]  aFatal      - ����Ÿ�� : FATAL �Ǵ� ABORT
*/
IDE_RC sddTableSpace::getDataFileNodeByPageID(
    sddTableSpaceNode* aSpaceNode,
    scPageID           aPageID,
    sddDataFileNode**  aFileNode,
    idBool             aFatal)
{
    idBool           sFound;
    sddDataFileNode* sFileNode;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL);

    sFileNode =
      (sddDataFileNode*)aSpaceNode->mFileNodeArr[ SD_MAKE_FID(aPageID)];

    if( sFileNode != NULL )
    {
        if ( ( sFileNode->mCurrSize > SD_MAKE_FPID( aPageID ) ) &&
             SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ) )
        {
            *aFileNode = sFileNode;
            sFound     = ID_TRUE;
        }
        else
        {
            *aFileNode = NULL;
            sFound     = ID_FALSE;
        }
    }
    else
    {
        *aFileNode = NULL;
        sFound     = ID_FALSE;
    }

    if( aFatal == ID_TRUE )
    {
        IDE_TEST_RAISE( sFound != ID_TRUE, error_fatal_not_found_filenode );
    }
    else
    {
        IDE_TEST_RAISE( sFound != ID_TRUE, error_abort_not_found_filenode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_fatal_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_NotFoundDataFile,
                                aPageID,
                                aSpaceNode->mHeader.mID,
                                aSpaceNode->mHeader.mType));
    }
    IDE_EXCEPTION( error_abort_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFile,
                                aPageID,
                                aSpaceNode->mHeader.mID,
                                aSpaceNode->mHeader.mType));
    }
    IDE_EXCEPTION_END;

    *aFileNode = NULL;

    return IDE_FAILURE;
}

/*
 �˻��� �������� �����ϴ� datafile ��� ��ȯ(No��������)

 [IN]  aSpaceNode  - ���̺����̽� ���
 [IN]  aPageID     - �˻��� ������ID
 [OUT] aFileNode   - �������� �����ϴ� ����Ÿ���� ���
                     ��ȿ���� ���� ��� NULL ��ȯ
*/
void sddTableSpace::getDataFileNodeByPageIDWithoutException(
                                     sddTableSpaceNode* aSpaceNode,
                                     scPageID           aPageID,
                                     sddDataFileNode**  aFileNode)
{
    sddDataFileNode* sFileNode;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL);

    sFileNode=
       (sddDataFileNode*)aSpaceNode->mFileNodeArr[ SD_MAKE_FID(aPageID)];

    /* BUG-35190 - [SM] during recover dwfile in media recovery phase,
     *             it can access a page in area that is not expanded.
     *
     * �̵�Ŀ���� ������ DW file ������ �õ��Ҷ�,
     * DW file�� ��� �������� �ֽ��� ��� datafile�� ���� �������� ������ �� �ִ�.
     * �� ��� ��� ������ ���� �ʰ�, �����ε� datafile ��忡 �������� �������� �����Ƿ�
     * NULL�� �������ش�. */
    if( sFileNode != NULL )
    {
        if ( ( sFileNode->mCurrSize > SD_MAKE_FPID( aPageID ) ) &&
             SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ) )
        {
            *aFileNode = sFileNode;
        }
        else
        {
            *aFileNode = NULL;
        }
    }
    else
    {
        *aFileNode = NULL;
    }

    return;
}

/***********************************************************************
 * Description : �ش� ���� ID�� ������ datafile ��� ��ȯ
 *
 * tablespace ����� datafile ��� ���� ����Ʈ���� �־��� ����ID��
 * �ش��ϴ� datafile ��带 ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sddTableSpace::getDataFileNodeByID(
    sddTableSpaceNode*  aSpaceNode,
    UInt                aID,
    sddDataFileNode**   aFileNode)
{

    sddDataFileNode* sFileNode;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL );

    sFileNode = (sddDataFileNode*)aSpaceNode->mFileNodeArr[ aID ];

    if( ( sFileNode == NULL ) ||
        SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) ||
        ( aSpaceNode->mNewFileID <= aID ))
    {
        IDE_RAISE( error_not_found_filenode );
    }

    *aFileNode = sFileNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNodeByID,
                                aID));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
  PRJ-1548 User Memory Tablespace
  ����Ÿ������ �������� ������ Null�� ��ȯ�Ѵ�

  sctTableSpaceMgr::lock()�� ȹ���ϰ� ȣ��Ǿ�� �Ѵ�.
*/
void sddTableSpace::getDataFileNodeByIDWithoutException( 
                                                 sddTableSpaceNode  * aSpaceNode,
                                                 UInt                aFileID,
                                                 sddDataFileNode  ** aFileNode)
{
    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aFileNode != NULL );

    *aFileNode = (sddDataFileNode*)aSpaceNode->mFileNodeArr[ aFileID];

    return;
}

/***********************************************************************
 * Description : �ش� ���ϸ��� ������ datafile ��� ��ȯ
 *
 * tablespace ����� datafile ��� ���� ����Ʈ���� �ش� ���ϸ�
 * �ش��ϴ� datafile ��带 ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sddTableSpace::getDataFileNodeByName( sddTableSpaceNode*  aSpaceNode,
                                             SChar*              aFileName,
                                             sddDataFileNode**   aFileNode)
{
    sddDataFileNode * sFileNode;
    UInt              i;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );

    *aFileNode = NULL;

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            continue;
        }

        if (idlOS::strcmp(sFileNode->mName, aFileName) == 0)
        {
            *aFileNode = sFileNode;
            break; // found
        }
    }

    IDE_TEST_RAISE( *aFileNode == NULL, error_not_found_filenode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * aCurFileID:������� �˻��� File ID
 *             (���Լ��� �� ���Ͼ��̵������ "��ȿ��"File Node�� �˾Ƴ���.)
***********************************************************************/
void sddTableSpace::getNextFileNode( sddTableSpaceNode* aSpaceNode,
                                     sdFileID           aCurFileID,
                                     sddDataFileNode**  aFileNode )
{
    UInt              i;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aFileNode != NULL );
    IDE_ASSERT( aCurFileID < SD_MAX_FID_COUNT );

    *aFileNode = NULL ;

    for (i = (aCurFileID+1) ; i < aSpaceNode->mNewFileID ; i++ )
    {
        if( aSpaceNode->mFileNodeArr[i] != NULL )
        {
            if( SMI_FILE_STATE_IS_NOT_DROPPED( aSpaceNode->mFileNodeArr[i]->mState ) )
            {
                 *aFileNode = aSpaceNode->mFileNodeArr[i] ;
                 break;
            }
        }
    }

    return;
}

/***********************************************************************
 * Description : autoextend ��带 ������ datafile ��带 ã�´�.
 *
 * �־��� autoextend �Ӽ��� ������ �� �ִ� datafile ��带
 * �˻��Ѵ�.
 * - �����ϰ����ϴ� autoextend �Ӽ��� ON�ΰ��
 *   datafile �̸��� ���� datafile ��带 �˻��� ����,
 *   �ش� ����� aUsedPageLimit�� �������� �ʴ� ���� ����� [NULL]
 *   �����ϸ� [�ش� datafile ���] ���� ����� [�ش� datafile ���]
 *   �� ��ȯ�Ѵ�.
 *   !! autoextend �Ӽ��� tablespace �߿� 1�������� datafile ���
 *   ���� ������ �����ϴ�.
 * - �����ϰ����ϴ� autoextend �Ӽ��� OFF�ΰ��
 *   datafile �̸��� ���� datafile ��带 �˻��ؼ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
IDE_RC sddTableSpace::getDataFileNodeByAutoExtendMode(
                         sddTableSpaceNode* aSpaceNode,
                         SChar*             aDataFileName,
                         idBool             aAutoExtendMode,
                         scPageID           aUsedPageLimit,
                         sddDataFileNode**  aFileNode )
{

    UInt             i;
    idBool           sFound;
    sddDataFileNode* sFileNode;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );

    sFound         = ID_FALSE;
    *aFileNode = NULL;

    for (i = 0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState) ||
            SMI_FILE_STATE_IS_CREATING( sFileNode->mState ) )
        {
            continue;
        }

        if (idlOS::strcmp(sFileNode->mName, aDataFileName) == 0)
        {
            if (aAutoExtendMode == ID_TRUE)
            {
                /* ------------------------------------------------
                 * aUsedPageLimit�� ������ datafile ��� �̰ų� �� ������
                 * datafile ����� ���� ����
                 * �׷��� �ʴٸ� NULL ��ȯ
                 * ----------------------------------------------*/
                if (sFileNode->mID >= SD_MAKE_FID(aUsedPageLimit) )
                {
                    sFound = ID_TRUE;
                    *aFileNode = sFileNode;
                }
                else
                {
                    /* Fix BUG-16962. */
                    IDE_RAISE( error_used_up_filenode );
                }
            }
            else
            {
                sFound = ID_TRUE;
                *aFileNode = sFileNode;
            }
            break;
        }
    }

     /* �־��� datafile �̸��� ���� */
    IDE_TEST_RAISE( sFound != ID_TRUE, error_not_found_filenode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_used_up_filenode );
    {
        IDE_SET(
             ideSetErrorCode(smERR_ABORT_AUTOEXT_ON_UNALLOWED_FOR_USED_UP_FILE,
                             aDataFileName)
               );
    }
    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aDataFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : tablespace ����� ������ ���
 ***********************************************************************/
IDE_RC sddTableSpace::getPageRangeByName( sddTableSpaceNode* aSpaceNode,
                                          SChar*             aDataFileName,
                                          sddDataFileNode**  aFileNode,
                                          scPageID*          aFstPageID,
                                          scPageID*          aLstPageID )
{
    idBool           sFound;
    sddDataFileNode* sFileNode;
    UInt i;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aDataFileName != NULL );
    IDE_DASSERT( aFileNode != NULL );

    sFound         = ID_FALSE;
    *aFileNode = NULL;
    *aFstPageID    = 0;
    *aLstPageID    = 0;

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            continue;
        }

        if (idlOS::strcmp(sFileNode->mName, aDataFileName) == 0)
        {
            sFound         = ID_TRUE;
            *aFileNode = sFileNode;
            *aLstPageID    = *aFstPageID + (sFileNode->mCurrSize - 1);
            break; // found
        }

        *aFstPageID += sFileNode->mCurrSize;
    }

    IDE_TEST_RAISE( sFound != ID_TRUE, error_not_found_filenode );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNode,
                                aDataFileName));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    ����Ÿ���ϳ�带 �˻��Ͽ� ������ ������ ��´�
*/
IDE_RC sddTableSpace::getPageRangeInFileByID(
                            sddTableSpaceNode * aSpaceNode,
                            UInt                aFileID,
                            scPageID          * aFstPageID,
                            scPageID          * aLstPageID )
{
    sddDataFileNode* sFileNode = NULL;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aFstPageID != NULL );
    IDE_DASSERT( aLstPageID != NULL );

    sFileNode = aSpaceNode->mFileNodeArr[ aFileID ] ;

    IDE_TEST_RAISE( sFileNode == NULL, error_not_found_filenode );

    *aFstPageID = SD_CREATE_PID( aFileID,0);

    *aLstPageID = SD_CREATE_PID( aFileID, sFileNode->mCurrSize -1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_found_filenode );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotFoundDataFileNodeByID,
                                aFileID));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
  PRJ-1548 User Memory Tablespace

  ��� : ���̺����̽� ��忡 ����� �� ������ ���� ��ȯ

   sctTableSpaceMgr::lock()�� ȹ���� ���·� ȣ��Ǿ�� �Ѵ�.
*/
ULong sddTableSpace::getTotalPageCount( sddTableSpaceNode* aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    return aSpaceNode->mTotalPageCount;
}

/*
  PRJ-1548 User Memory Tablespace

   ��� : ���̺����̽� ��忡 ����� �� DBF ���� ��ȯ

   sctTableSpaceMgr::lock()�� ȹ���� ���·� ȣ��Ǿ�� �Ѵ�.
*/
UInt sddTableSpace::getTotalFileCount( sddTableSpaceNode* aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    return aSpaceNode->mDataFileCount;
}


/*
   ��� : tablespace �Ӽ��� ��ȯ�Ѵ�.
*/
void sddTableSpace::getTableSpaceAttr(sddTableSpaceNode* aSpaceNode,
                                      smiTableSpaceAttr* aSpaceAttr)
{

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aSpaceAttr != NULL );

    aSpaceAttr->mAttrType  = SMI_TBS_ATTR;
    idlOS::strcpy(aSpaceAttr->mName, aSpaceNode->mHeader.mName);
    aSpaceAttr->mNameLength = idlOS::strlen(aSpaceNode->mHeader.mName);

    aSpaceAttr->mID                        = aSpaceNode->mHeader.mID;
    aSpaceAttr->mDiskAttr.mNewFileID       = aSpaceNode->mNewFileID;
    aSpaceAttr->mDiskAttr.mExtMgmtType     = aSpaceNode->mExtMgmtType;
    aSpaceAttr->mDiskAttr.mSegMgmtType     = aSpaceNode->mSegMgmtType;
    aSpaceAttr->mType                      = aSpaceNode->mHeader.mType;
    aSpaceAttr->mTBSStateOnLA              = aSpaceNode->mHeader.mState;
    aSpaceAttr->mDiskAttr.mExtPageCount    = aSpaceNode->mExtPageCount;
    aSpaceAttr->mAttrFlag                  = aSpaceNode->mAttrFlag;

    return;

}

/*
   ��� : tablespace ������ ����Ѵ�.
*/
IDE_RC sddTableSpace::dumpTableSpaceNode( sddTableSpaceNode* aSpaceNode )
{
    SChar  sTbsStateBuff[500 + 1];

    IDE_ERROR( aSpaceNode != NULL );

    idlOS::printf("[TBS-BEGIN]\n");
    idlOS::printf(" ID\t: %"ID_UINT32_FMT"\n", aSpaceNode->mHeader.mID );
    idlOS::printf(" Type\t: %"ID_UINT32_FMT"\n", aSpaceNode->mHeader.mType );
    idlOS::printf(" Name\t: %s\n", aSpaceNode->mHeader.mName );

    // klocwork SM
    idlOS::memset( sTbsStateBuff, 0x00, 500 + 1 );

    if( SMI_TBS_IS_OFFLINE(aSpaceNode->mHeader.mState))
    {
        idlOS::strncat(sTbsStateBuff,"OFFLINE | ", 500);
    }
    if( SMI_TBS_IS_ONLINE(aSpaceNode->mHeader.mState) )
    {
        idlOS::strncat(sTbsStateBuff,"ONLINE | ", 500);
    }
    if( SMI_TBS_IS_CREATING(aSpaceNode->mHeader.mState) )
    {
        idlOS::strncat(sTbsStateBuff,"CREATING | ", 500);
    }
    if( SMI_TBS_IS_DROPPING(aSpaceNode->mHeader.mState) )
    {
        idlOS::strncat(sTbsStateBuff,"DROPPING | ", 500);
    }
    if( SMI_TBS_IS_DROPPED(aSpaceNode->mHeader.mState) )
    {
        idlOS::strncat(sTbsStateBuff,"DROPPED | ", 500);
    }
    if( SMI_TBS_IS_BACKUP(aSpaceNode->mHeader.mState) )
    {
        idlOS::strncat(sTbsStateBuff,"BACKUP | ", 500);
    }

    sTbsStateBuff[idlOS::strlen(sTbsStateBuff) - 2] = 0;

    idlOS::printf(" STATE\t: %s\n", sTbsStateBuff);
    idlOS::printf(" Datafile Node Count : %"ID_UINT32_FMT"\n",
                  aSpaceNode->mDataFileCount );
    idlOS::printf(" Total Page Count : %"ID_UINT64_FMT"\n",
                  aSpaceNode->mTotalPageCount );
    idlOS::printf("[TBS-BEGIN]\n");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#if 0 //not used
/*
  ��� : tablespace ����� datafile ��� ����Ʈ�� ���
*/
void sddTableSpace::dumpDataFileList( sddTableSpaceNode* aSpaceNode )
{

    UInt i;
    sddDataFileNode*     sFileNode;

    IDE_DASSERT ( aSpaceNode != NULL );

    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        (void)sddDataFile::dumpDataFileNode( sFileNode );
    }

}
#endif
/*
  ��� : ���ο� DBF ID�� ��ȯ�ϰ� +1 ������Ų��.
*/
sdFileID sddTableSpace::getNewFileID( sddTableSpaceNode * aSpaceNode )
{
    IDE_DASSERT( aSpaceNode != NULL );

    return (aSpaceNode->mNewFileID)++;
}


/*
  PRJ-1548 User Memory Tablespace

  ���������� �������Ŀ� ���̺����̽���
  DataFileCount�� TotalPageCount�� ����Ͽ� �����Ѵ�.

  �� ������Ʈ���� sddTableSpaceNode�� mDataFileCount��
  mTotalPageCount�� RUNTIME ������ ����ϹǷ�
  ���������� �������Ŀ� ������ �������־�� �Ѵ�.
*/

void sddTableSpace::calculateFileSizeOfTBS( sddTableSpaceNode * aSpaceNode )
{
    UInt                 sFileCount = 0;
    ULong                sTotalPageCount = 0;
    sddDataFileNode*     sFileNode;
    UInt                 i;

    IDE_DASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE );

    for ( i = 0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL )
        {
            continue;
        }

        IDE_DASSERT( SMI_FILE_STATE_IS_NOT_CREATING( sFileNode->mState ) );
        IDE_DASSERT( SMI_FILE_STATE_IS_NOT_DROPPING( sFileNode->mState ) );
        IDE_DASSERT( SMI_FILE_STATE_IS_NOT_RESIZING( sFileNode->mState ) );

        if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
        {
            continue;
        }

        sFileCount++;
        sTotalPageCount += sFileNode->mCurrSize;
    }

    aSpaceNode->mDataFileCount = sFileCount;
    aSpaceNode->mTotalPageCount = sTotalPageCount;
}

/*
  PRJ-1548 User Memory Tablespace

  ���̺����̽��� ���¸� �����ϰ� ����� �����Ѵ�.
  CREATING ���̰ų� DROPPING ���� ��� TBS Mgr Latch�� Ǯ��
  ��� ����ϴٰ� Latch�� �ٽ� �õ��� ���� �ٽ� �õ��Ѵ�.

  �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.

  [IN] aSpaceNode : ����� TBS Node
  [IN] aActionArg : ����� �ʿ��� ����
*/

IDE_RC sddTableSpace::doActOnlineBackup( idvSQL            * aStatistics,
                                         sctTableSpaceNode * aSpaceNode,
                                         void              * aActionArg )
{
    sctActBackupArgs * sActBackupArgs  = (sctActBackupArgs*)aActionArg;
    UInt               sState = 0;

    IDE_DASSERT( aSpaceNode != NULL );
    IDE_DASSERT( aActionArg != NULL );

    if ( (sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE) &&
         (sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) != ID_TRUE) )
    {
        if ( ( (aSpaceNode->mState & SMI_TBS_DROPPED)   != SMI_TBS_DROPPED ) &&
             ( (aSpaceNode->mState & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED ) )
        {
            /* ------------------------------------------------
             * disk table space backup ���� �����
             * ù��° ����Ÿ������ ���¸� backup���� �����Ѵ�.
             * ----------------------------------------------*/
            IDE_TEST( sctTableSpaceMgr::startTableSpaceBackup(
                          aStatistics,
                          aSpaceNode ) != IDE_SUCCESS);
            sState = 1;

            // Backup Flag�� �����ϰ� �ѹ� �� Ȯ���Ѵ�.
            // Discard�� �� �� ��� Drop�� �� ���� �ִ�.
            if ( (aSpaceNode->mState & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
            {
                IDE_DASSERT( SMI_TBS_IS_ONLINE(aSpaceNode->mState) );

                if ( sActBackupArgs->mIsIncrementalBackup == ID_TRUE )
                {
                    IDE_TEST( smLayerCallback::incrementalBackupDiskTBS(
                                  aStatistics,
                                  (sddTableSpaceNode*)aSpaceNode,
                                  sActBackupArgs->mBackupDir,
                                  sActBackupArgs->mCommonBackupInfo )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( smLayerCallback::backupDiskTBS(
                                  aStatistics,
                                  (sddTableSpaceNode*)aSpaceNode,
                                  sActBackupArgs->mBackupDir )
                              != IDE_SUCCESS );
                }
            }

            /* ------------------------------------------------
             * ���̺� �����̽� �� ���¸� backup���� online���� �����Ѵ�.
             * ----------------------------------------------*/
            sState = 0;
            IDE_TEST( sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                                             aSpaceNode ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        sctTableSpaceMgr::endTableSpaceBackup( aStatistics,
                                               aSpaceNode ) ;
    }

    return IDE_FAILURE;
}

/*
  ���̺����̽��� Dirty�� ����Ÿ������ Sync�Ѵ�.
  �� �Լ��� ȣ��Ǳ����� TBS Mgr Latch�� ȹ��� �����̴�.

  [IN] aSpaceNode : Sync�� TBS Node
  [IN] aActionArg : NULL
*/
IDE_RC sddTableSpace::doActSyncTBSInNormal( idvSQL            * aStatistics,
                                            sctTableSpaceNode * aSpaceNode,
                                            void              * /*aActionArg*/ )
{
    sddTableSpaceNode * sSpaceNode;
    sddDataFileNode   * sFileNode;
    UInt                i;

    IDE_DASSERT( aSpaceNode != NULL );

    while( 1 )
    {
        if( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) != ID_TRUE )
        {
            // Disk Table�� �ƴѰ�� �����Ѵ�.
            break;
        }

        if( sctTableSpaceMgr::hasState( aSpaceNode,
                                        SCT_SS_SKIP_SYNC_DISK_TBS )
            == ID_TRUE )
        {
            // ���̺����̽��� DROPPED/DISCARDED ��� �����Ѵ�.

            // DROP�� TBS�� ��� DROP �������� Buffer�� Page����
            // ��� Invalid ��Ű�� ���� DROPPED ���·� ����Ǳ�
            // ������ �� ���Ŀ� TBS�� ���õ� Page�� Dirty ��
            // ���� ����.
            // DISCARDED TBS�� Startup Control �ܰ迡�� �����ǹǷ�
            // Dirty Page�� �߻��� �� ����.
            break;
        }

        sSpaceNode = (sddTableSpaceNode*)aSpaceNode;

        // PRJ-1548 User Memory Tablespace
        // ���̺����̽��� �������� �ʰų�, DROPPED ������ ��쿡
        // sSpaceNode�� NULL�� ��ȯ�ȴ�.
        //
        // A. Ʈ����� Pending ���꿡 ���ؼ� TBS Node�� DROPPED ���°� �Ǿ��ٸ�,
        //    SYNC�� �����ϵ��� ó���Ѵ�.
        //   ���̺����̽� ��尡 �����Ǵ��� �ٷ� free���� �ʱ� �����̴�.
        //
        // B. �������� �ʴ� ���̺����̽��� ��쵵 �׳� �����Ѵ�.

        for ( i=0; i < sSpaceNode->mNewFileID ; i++ )
        {
            sFileNode = sSpaceNode->mFileNodeArr[i] ;

            if( sFileNode == NULL )
            {
                continue;
            }

            // PRJ-1548 User Memory Tablespace
            // Ʈ����� Pending ���꿡 ���ؼ� DBF Node�� DROPPED ���°�
            // �Ǿ��ٸ�, SYNC ���꿡�� ���� ����. ���º����� sctTableSpaceMgr::lock()
            // ���� ��ȣ�Ѵ�.

            if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                continue;
            }

            sddDataFile::lockFileNode( aStatistics,
                                       sFileNode );

            if ( (sFileNode->mIsOpened == ID_TRUE) &&
                 (sFileNode->mIsModified == ID_TRUE) )
            {
                // checkpoint sync�� prepareIO/completeIO�� ����ǹǷ�
                // checkpoint sync�� ���ؼ� ������ �� �ִ�.

                sFileNode->mIsModified = ID_FALSE ;

                /* BUG-24558: [SD] DBF Sync�� Close�� ���ÿ� �����
                 *
                 * DBF�� Sync�������� idBool�� ó�������� ���ÿ� Sync������
                 * �߻��� ���� �Ϸ�� Sync������ Sync Flag�� ID_FALSE��
                 * �Ͽ� �ι�° Sync������ ������������ File Open List����
                 * Victim���� �����Ǿ� Close�� �Ǵ� ������ ����.
                 * �Ͽ� Sync�� ������ ���ÿ� ��û�� �ɼ� �ֱ⶧���� ���� ���۽�
                 * IOCount�� ������Ű�� ������ ���ҽ�Ű�� �Ͽ���.
                 * Sync�� ������������ IOCount 0���� ũ�� �������������� �Ǵ� */
                sFileNode->mIOCount++;
                sddDataFile::unlockFileNode( sFileNode );

                /* prepareIO �� �ϱ����� space lock�� ���� unlock�ع�����
                 * �� ���̿� file�� drop���� �� �� �ִ�.*/
                sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
                // PRJ-1548 SM - User Memory TableSpace ���䵵��
                //
                // A. mIOCount�� 0�� �ƴ� ���� close�� �� ����.
                //    (���� sddDiskMgr::findVictim)
                // B. sync ���϶� DROPPED ���·� ����ɼ� ����.
                //    (���� sctTableSpaceMgr::executePendingOp)
                IDE_ASSERT( sddDataFile::sync( sFileNode,
                                               smLayerCallback::setEmergency ) == IDE_SUCCESS );

                // �ݵ�� DiskMgr�� completeIO �Ͽ��� Victim List�� ���� �ȴ�.
                sddDiskMgr::completeIO( aStatistics,
                                        sFileNode,
                                        SDD_IO_READ );
                /* completeIO ���� Lock�� ���� �ʾƵ� �ȴ�.
                 * prepareIO �� ���� file �� drop�� ���� �־ ��� �־�� ������
                 * completeIO�� �۾��� ���� �̹� �� �Ͽ����Ƿ� lock�� ���� �ʰ� �־ �ȴ�.
                 * prepareIO�� completeIO���� ��ü���� file lock�� ��´�.*/
                sctTableSpaceMgr::lockSpaceNode( aStatistics,
                                                 aSpaceNode );
            }
            else
            {
                sddDataFile::unlockFileNode( sFileNode );
                // DIRTY ���� ��尡 �ƴѰ��
                // Nothing To Do..
            }
        }

        break;
    }

    return IDE_SUCCESS;
}
