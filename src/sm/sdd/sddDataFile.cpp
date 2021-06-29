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
 * $Id: sddDataFile.cpp 86490 2020-01-02 05:59:08Z et16 $
 *
 * Description :
 *
 * �� ������ ��ũ�������� datafile ��忡 ���� ���������̴�.
 *
 * # ����
 *
 * �ϳ��� ����Ÿ���̽� ȭ�Ͽ� ���� ������ �����Ѵ�.
 *
 * # ����Ÿ���̽� ȭ���� ũ��
 *     - ���̺� �����̽��� ũ�⸦ �ø���, 2������ ����� �ִ�.
 *       �� ����� ����ڰ� ���̺����̽��� ������ �� ���������ϴ�.
 *
 *        1. ȭ���� ũ�⸦ �ø���
 *           - AUTOEXTEND ON NEXT SIZE��
 *             �����ϸ� ������ ���ڶ� �� SIZE ��ŭ �߰��ȴ�.
 *        2. �� �ϳ��� ȭ���� �߰��ϱ�
 *           - AUTOEXTEND OFF��
 *             �����ϸ� �ڵ����� ������ �ø��� �ʴ´�. ��� ����ڰ�
 *             ��������� ALTER TABLESPACE ������ �̿��Ͽ� ������ �ø� ��
 *             �ִ�.
 *     - system tablespace�� ����Ÿȭ���� ������ ũ�⸦ ������ �� ȭ����
 *       ũ��� �� �̻� ������ �� ����. �Ϻ� �ý��ۿ����� ȭ���� ũ�Ⱑ
 *       Ư�� ũ��(�� 2G) �̻��̸�, ������ �ް��� ������ �� �ִ�.
 *       ���� system tablespace�� ��� �� ũ�� �̻����� ȭ���� �ø���
 *       �ʰ�, ���ο� ȭ���� �߰��Ѵ�.
 *     - �� ���� altibase cofig file���� ���� �����ϴ�.
 *     - ������� tablespace�� ����Ÿȭ�Ͽ� ���ؼ��� �̷��� ������ ���� ������,
 *       �̷� ������ ���� ��� ����ڰ� ó���ؾ� �Ѵ�.
 *     - 4.1.1.1 Tablespace User Interface ������ ����
 *
 * # ����Ÿ���̽� ȭ���� Naming
 *     - 4.1.1.1 Tablespace User Interface ������ ����
 *     - Databae�� ����Ÿȭ�� �⺻ ���� ���丮 �ȿ�
 *       "system, "undo", "temp"�� �����ϴ� ȭ�� �̸��� �� �� ����.
 *
 * # ���丮
 *     - ����Ÿ ȭ���� ����Ǵ� ���丮�� config ȭ�Ͽ� �����ȴ�.
 *     - SQL ������ ����Ÿ ȭ���� �̸��� ����� Ȥ�� ���� ��η�
 *       ������ �� �ִ�.
 *     - ����η� ������ ��, �����η� modify�� �õ����� ��,
 *       �̸� verify �� �� �־�� �Ѵ�. �̸� ���� ��� ��θ�
 *       ���� ��η� ������ ���̴�.
 *     - ����Ÿȭ���� �̸��� ���+�̸��̴�.
 *     - Ư�����ڰ� ���� �� Ȯ���ϴ� ������ �ʿ��ϴ�.
 *     - �̿� ���� �ڵ尡 sddDiskMgr::makeValidDataFilePath���� �����Ǿ��
 *       �� ����.
 **********************************************************************/

#include <idu.h>
#include <idl.h>
#include <ide.h>
#include <smu.h>
#include <smErrorCode.h>
#include <sddReq.h>
#include <sddDiskMgr.h>
#include <sddDataFile.h>
#include <sctTableSpaceMgr.h>
#include <smriChangeTrackingMgr.h>

/***********************************************************************
 * Description : datafile ��带 �ʱ�ȭ�Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::initialize(scSpaceID         aTableSpaceID,
                               sddDataFileNode*  aDataFileNode,
                               smiDataFileAttr*  aDataFileAttr,
                               UInt              aMaxDataFilePageCount,
                               idBool            aCheckPathValidate)
{
    SChar   sMutexName[128];
    UInt    sState = 0;

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aDataFileAttr != NULL );
    IDE_DASSERT( aMaxDataFilePageCount > 0 );
    IDE_DASSERT( aDataFileAttr->mAttrType == SMI_DBF_ATTR );

    aDataFileNode->mSpaceID      = aTableSpaceID;
    aDataFileNode->mID           = aDataFileAttr->mID;
    aDataFileNode->mState        = aDataFileAttr->mState;

    /* datafile ����� �������� �Ӽ� ���� */
    aDataFileNode->mIsAutoExtend = aDataFileAttr->mIsAutoExtend;
    aDataFileNode->mCreateMode   = aDataFileAttr->mCreateMode;
    aDataFileNode->mInitSize     = aDataFileAttr->mInitSize;
    aDataFileNode->mCurrSize     = aDataFileAttr->mCurrSize;
    aDataFileNode->mIsOpened     = ID_FALSE;
    aDataFileNode->mIOCount      = 0;
    aDataFileNode->mIsModified   = ID_FALSE;

    IDE_TEST( iduFile::allocBuff4DirectIO(
                                  IDU_MEM_SM_SMM,
                                  SM_PAGE_SIZE,
                                  (void**)&( aDataFileNode->mPageBuffPtr ),
                                  (void**)&( aDataFileNode->mAlignedPageBuff) )
             != IDE_SUCCESS);
    sState = 1;

    // BUG-17415 autoextend off�� ��� nextsize, maxsize�� 0�� �����Ѵ�.
    // smiDataFileAttr�� nextsize, maxsize�� �״�� �д�.
    if (aDataFileNode->mIsAutoExtend == ID_TRUE)
    {
        aDataFileNode->mNextSize = aDataFileAttr->mNextSize;
        aDataFileNode->mMaxSize  = aDataFileAttr->mMaxSize;
    }
    else
    {
        aDataFileNode->mNextSize = 0;
        aDataFileNode->mMaxSize = 0;
    }

    idlOS::memset( &(aDataFileNode->mDBFileHdr),
                   0x00,
                   ID_SIZEOF(sddDataFileHdr) );

    //PROJ-2133 incremental backup
    aDataFileNode->mDBFileHdr.mDataFileDescSlotID.mBlockID = 
                                        SMRI_CT_INVALID_BLOCK_ID; 
    aDataFileNode->mDBFileHdr.mDataFileDescSlotID.mSlotIdx = 
                                        SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;

    /* ------------------------------------------------
     * datafile ũ�� validation Ȯ��
     * ----------------------------------------------*/
    IDE_TEST_RAISE( aDataFileNode->mInitSize > aMaxDataFilePageCount,
                    error_invalid_filesize );
    IDE_TEST_RAISE( aDataFileNode->mCurrSize > aMaxDataFilePageCount,
                    error_invalid_filesize );
    IDE_TEST_RAISE( aDataFileNode->mMaxSize  > aMaxDataFilePageCount,
                    error_invalid_filesize );

    IDE_TEST_RAISE( (aDataFileNode->mIsAutoExtend == ID_TRUE) &&
                    ((aDataFileNode->mCurrSize > aDataFileNode->mMaxSize) ||
                     (aDataFileNode->mInitSize > aDataFileNode->mCurrSize)),
                    error_invalid_filesize );

    /* ------------------------------------------------
     * path�� ���� ������, ���ڿ� '/'�� ����ϸ�,
     * ����θ� ������ �����Ͽ� �����Ѵ�.
     * ----------------------------------------------*/

    if( aCheckPathValidate == ID_TRUE)
    {
        IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                    ID_TRUE,
                                    aDataFileAttr->mName,
                                    &aDataFileAttr->mNameLength,
                                    SMI_TBS_DISK)
                  != IDE_SUCCESS );
    }

    /* datafile �̸� */
    aDataFileNode->mName = NULL;

    /* TC/FIT/Limit/sm/sddDataFile_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sddDataFile::initialize::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(
                          IDU_MEM_SM_SDD,
                          aDataFileAttr->mNameLength + 1,
                          (void**)&(aDataFileNode->mName)) != IDE_SUCCESS,
                   insufficient_memory );

    idlOS::memset(aDataFileNode->mName, 0x00, aDataFileAttr->mNameLength + 1);
    idlOS::strcpy(aDataFileNode->mName, aDataFileAttr->mName);
    sState = 2;

    IDE_TEST( aDataFileNode->mFile.initialize( IDU_MEM_SM_SDD,
                                               smuProperty::getMaxOpenFDCount4File(),
                                               IDU_FIO_STAT_ON,
                                               IDV_WAIT_INDEX_LATCH_FREE_DRDB_FILEIO )
              != IDE_SUCCESS );

    IDE_TEST( aDataFileNode->mFile.setFileName(aDataFileNode->mName)
              != IDE_SUCCESS );

    /* datafile LRU ����Ʈ�� NODE ����Ʈ �ʱ�ȭ */
    SMU_LIST_INIT_NODE(&aDataFileNode->mNode4LRUList);
    aDataFileNode->mNode4LRUList.mData = (void*)(aDataFileNode);

    // PRJ-1548 User Memory Tablespace
    aDataFileNode->mAnchorOffset = SCT_UNSAVED_ATTRIBUTE_OFFSET;
    
    idlOS::sprintf( sMutexName, "DATAFILE_NODE_MUTEX_%"ID_UINT32_FMT"_%"ID_UINT32_FMT"",
                    aTableSpaceID,
                    aDataFileNode->mID );

    IDE_TEST( aDataFileNode->mMutex.initialize( sMutexName,
                                                IDU_MUTEX_KIND_NATIVE,
                                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_filesize );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFileSizeOnLogAnchor,
                                aDataFileAttr->mName,
                                aDataFileNode->mInitSize,
                                aDataFileNode->mCurrSize,
                                aDataFileNode->mMaxSize,
                                (ULong)aMaxDataFilePageCount));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        IDE_PUSH();
        switch( sState )
        {
            case 2:
                IDE_ASSERT( iduMemMgr::free( aDataFileNode->mName )
                            == IDE_SUCCESS );
            case 1:
                IDE_ASSERT( iduMemMgr::free( aDataFileNode->mPageBuffPtr )
                            == IDE_SUCCESS );
            default:
                break;
        }
        IDE_POP();
        aDataFileNode->mName = NULL;
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : datafile ��带 �����Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::destroy( sddDataFileNode* aDataFileNode )
{

    IDE_DASSERT( aDataFileNode != NULL );

    IDE_TEST( iduMemMgr::free( aDataFileNode->mPageBuffPtr )
              != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free( aDataFileNode->mName ) != IDE_SUCCESS );

    IDE_TEST( aDataFileNode->mFile.destroy() != IDE_SUCCESS );

    IDE_TEST( aDataFileNode->mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ���ο� datafile�� create�Ѵ�.
 datafile Layout�� ������ ����.
 ------------------------------------------------------------
|  |             |               |               |           |
 ------------------------------------------------------------
 ^            |                                       |
 |            -----------------------------------------
 file heaer     data pages

 **********************************************************************/
IDE_RC sddDataFile::create( idvSQL          * aStatistics,
                            sddDataFileNode * aDataFileNode,
                            sddDataFileHdr  * aDBFileHdr )
{
    UInt   sState = 0;

    IDE_DASSERT( aDataFileNode != NULL );

    /* ===========================================================
     * [1] disk �������� ������ �̸��� ���� ������ �����ϴ��� �˻�
     * =========================================================== */
    IDE_TEST_RAISE( idf::access(aDataFileNode->mName, F_OK) == 0,
                    error_already_exist_datafile );

    /* ===========================================================
     * [2] ȭ�� ���� �� ����
     * =========================================================== */
    IDE_TEST( aDataFileNode->mFile.createUntilSuccess(
                smLayerCallback::setEmergency )
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-47398 AIX ���� iduMemMgr::free4malign() �Լ����� ������ ����
     * �ٷ� close �ϹǷ� DirectIO ���δ� �߿������� �ʴ�, property�� ������.*/
    IDE_TEST( open( aDataFileNode,
                    sddDiskMgr::isDirectIO() ) != IDE_SUCCESS );
    sState = 2;

    /* ===========================================================
     * [3] PRJ-1149, ȭ����� ����.
     * =========================================================== */
    // BUG-17415 writing ���� ��ŭ�� �����.
    IDE_TEST( writeNewPages(aStatistics, aDataFileNode) != IDE_SUCCESS );

    IDE_TEST( writeDBFileHdr( aStatistics,
                              aDataFileNode,
                              aDBFileHdr )
              != IDE_SUCCESS );

    /* ===========================================================
     * [4] ����Ÿ ȭ�� sync �� close
     * =========================================================== */
    sState = 0;
    IDE_TEST( close( aDataFileNode ) != IDE_SUCCESS );

    //==========================================================================
    // To Fix BUG-13924
    //==========================================================================

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_DISK_FILE_CREATE,
                (aDataFileNode->mName == NULL ) ? "" : aDataFileNode->mName);

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_exist_datafile );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AlreadyExistFile, aDataFileNode->mName));
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_PUSH();
        switch (sState)
        {
            case 2:
                IDE_ASSERT( close( aDataFileNode ) == IDE_SUCCESS );

            case 1:
                IDE_ASSERT( idf::unlink( aDataFileNode->mName )
                            == IDE_SUCCESS );
            default:
                break;
        }
        IDE_POP();
    }

    return IDE_FAILURE;

}

IDE_RC sddDataFile::writeNewPages(idvSQL          *aStatistics,
                                  sddDataFileNode *aDataFileNode)
{
    UChar *sBufferPtr;
    UChar *sAlignedBuffer;
    UChar *sCurPos;
    ULong  i;
    ULong  sUnitSize;
    ULong  sWriteOffset;
    SInt   sAllocated = 0;

    IDE_DASSERT( aDataFileNode != NULL );

    sUnitSize = smuProperty::getDataFileWriteUnitSize();

    if (sUnitSize > aDataFileNode->mInitSize)
    {
        // ����Ϸ��� ������������ ũ�Ⱑ ���� �������� ������
        // ���� ������ ���������� ũ�Ⱑ �ȴ�.
        sUnitSize = aDataFileNode->mInitSize;
    }

    // ���� ������ �Ҵ�޴´�.
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDD,
                                           sUnitSize * SD_PAGE_SIZE,
                                           (void**)&sBufferPtr,
                                           (void**)&sAlignedBuffer ) != IDE_SUCCESS );
    sAllocated = 1;

    // ���� ������ �ʱ�ȭ�Ѵ�.
    //   1. ���� ù��° �������� �ʱ�ȭ�Ѵ�.
    idlOS::memset(sAlignedBuffer, 0x00, SD_PAGE_SIZE);
    idlOS::memcpy(sAlignedBuffer, &(sddDiskMgr::mInitialCheckSum), ID_SIZEOF(UInt));

    //   2. �ι�° ���������ʹ� ù��° �������� �����Ѵ�.
    for (i = 1, sCurPos = sAlignedBuffer + SD_PAGE_SIZE;
         i < sUnitSize;
         i++, sCurPos += SD_PAGE_SIZE)
    {
        idlOS::memcpy(sCurPos, sAlignedBuffer, SD_PAGE_SIZE);
    }

    // ������ ���Ͽ� ���۸� ����Ѵ�.
    //   1. sUnitSize ������ ����Ѵ�.
    for ( i = 0, sWriteOffset = SM_DBFILE_METAHDR_PAGE_OFFSET;
          i < aDataFileNode->mInitSize / sUnitSize;
          i++ )
    {
        /* BUG-18138 ���̺����̽� �����ϴٰ� ��ũ Ǯ ��Ȳ�� �Ǹ�
         * �ش� ������ ������ ���� �ʽ��ϴ�.
         *
         * ������ Disk IO�� �����Ҷ����� ����ؼ� ������ ��������ϴ�.
         * �ٷ� ������ �߻��ϸ� ����ó���ϵ��� �����Ͽ����ϴ�. */
        IDE_TEST( aDataFileNode->mFile.write(
                                      aStatistics,
                                      sWriteOffset,
                                      sAlignedBuffer,
                                      sUnitSize * SD_PAGE_SIZE )
                  != IDE_SUCCESS);

        sWriteOffset += sUnitSize * SD_PAGE_SIZE;

        IDU_FIT_POINT( "1.BUG-18138@sddDataFile::writeNewPages" );

        IDU_FIT_POINT( "1.BUG-12754@sddDataFile::writeNewPages" );

        // BUG-12754 : session ������ ������� �˻���
        IDE_TEST(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS);
    }

    //   2. aDataFileNode->mInitSize - sUnitSize * i + 1 ũ�⸸ŭ ����Ѵ�.
    //      ���⼭ 1�� datafile header�̴�.
    IDE_TEST( aDataFileNode->mFile.write(
                         aStatistics,
                         sWriteOffset,
                         sAlignedBuffer,
                         (aDataFileNode->mInitSize - sUnitSize * i + 1)
                         * SD_PAGE_SIZE )
              != IDE_SUCCESS );

    sAllocated = 0;
    IDE_TEST(iduMemMgr::free(sBufferPtr) != IDE_SUCCESS);
    sBufferPtr = NULL;
    sAlignedBuffer = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if (sAllocated == 1)
        {
            IDE_ASSERT(iduMemMgr::free(sBufferPtr) == IDE_SUCCESS);
            sBufferPtr = NULL;
            sAlignedBuffer = NULL;

        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ش� datafile�� reuse�Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::reuse( idvSQL          * aStatistics,
                           sddDataFileNode * aDataFileNode )
{
    UInt            sState = 0;
    ULong           sCurrSize;
    ULong           sFileSize;

    IDE_DASSERT( aDataFileNode != NULL );

    sFileSize = 0;

    // must be fullpath
    IDE_TEST( aDataFileNode->mFile.setFileName(aDataFileNode->mName) != IDE_SUCCESS );

    /* BUGBUG - ERROR MSG - datafile�� �����ϸ� reuse �׷��� ������ ���� */
    if ( idf::access(aDataFileNode->mName, F_OK) == 0 )
    {
        /* BUG-47398 AIX ���� iduMemMgr::free4malign() �Լ����� ������ ����
         * �ٷ� close �ϹǷ� DirectIO ���δ� �߿������� �ʴ�, property�� ������.*/
        IDE_TEST( open( aDataFileNode,
                        sddDiskMgr::isDirectIO() ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( aDataFileNode->mFile.getFileSize(&sFileSize) != IDE_SUCCESS );

        sCurrSize = (ULong)((sFileSize - SM_DBFILE_METAHDR_PAGE_SIZE) / SD_PAGE_SIZE);

        if ( sCurrSize > aDataFileNode->mInitSize )
        {
            IDE_TEST( aDataFileNode->mFile.truncate( SDD_CALC_PAGEOFFSET(aDataFileNode->mInitSize) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }

        /* ===========================================================
         * ����Ÿ ȭ���� ���� ũ�⸸ŭ �ʱ�ȭ
         * =========================================================== */
        // BUG-17415 writing ���� ��ŭ�� �����.
        IDE_TEST( writeNewPages(aStatistics, aDataFileNode) != IDE_SUCCESS );

        /* ===========================================================
         * PRJ-1149, ȭ����� ����.
         * =========================================================== */

        IDE_TEST( writeDBFileHdr( aStatistics,
                                  aDataFileNode,
                                  &(aDataFileNode->mDBFileHdr) )
                  != IDE_SUCCESS );

        /* ===========================================================
         * ����Ÿ ȭ�� sync �� close
         * =========================================================== */
        sState = 0;
        IDE_TEST( close( aDataFileNode ) != IDE_SUCCESS );
    }
    else
    {
        aDataFileNode->mCreateMode = SMI_DATAFILE_CREATE;

        IDE_TEST( create(aStatistics, aDataFileNode) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        (void)close( aDataFileNode );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : datafile�� extend ��Ų��.
 *
 * aStatistics   - [IN] ��� ����
 * aDataFileNode - [IN] Datafile Node
 * aExtendSize   - [IN] Ȯ���ϰ��� �ϴ� ũ��� Page Count�̴�.
 **********************************************************************/
IDE_RC sddDataFile::extend( idvSQL          * aStatistics,
                            sddDataFileNode * aDataFileNode,
                            ULong             aExtendSize )
{
    ULong  i;
    ULong  sWriteOffset;
    ULong  sExtendWriteOffset4CT;
    UChar* sBufferPtr;
    UChar* sAlignedBuffer;
    UInt   sWriteUnitSize;
    ULong  sRestSize;
    ULong  sExtendByte;
    UInt   sExtendedPageID;
    UInt   sIBChunkID;
    UInt   sExtendedPageCnt;
    UInt   sTotalExtendedPageCnt;
    UInt   sState = 0;

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aDataFileNode->mIsOpened == ID_TRUE );

    sExtendByte    = aExtendSize * SD_PAGE_SIZE;
    sWriteUnitSize = smuProperty::getDataFileWriteUnitSize() * SD_PAGE_SIZE;

    if( sExtendByte < sWriteUnitSize )
    {
        sWriteUnitSize = sExtendByte;
    }

    IDE_TEST( iduFile::allocBuff4DirectIO(
                  IDU_MEM_SM_SDD,
                  sWriteUnitSize,
                  (void**)&sBufferPtr,
                  (void**)&sAlignedBuffer )
              != IDE_SUCCESS );
    sState = 1;

    // ���� ������ �ʱ�ȭ�Ѵ�.
    //   1. ���� ù��° �������� �ʱ�ȭ�Ѵ�.
    idlOS::memset(sAlignedBuffer, 0x00, SD_PAGE_SIZE);
    idlOS::memcpy(sAlignedBuffer, &(sddDiskMgr::mInitialCheckSum), ID_SIZEOF(UInt));

    sWriteOffset            = SDD_CALC_PAGEOFFSET(aDataFileNode->mCurrSize);
    sExtendWriteOffset4CT   = sWriteOffset;

    //   2. �ι�° ���������ʹ� ù��° �������� �����Ѵ�.
    for( i = SD_PAGE_SIZE; i < sWriteUnitSize; i += SD_PAGE_SIZE )
    {
        idlOS::memcpy( sAlignedBuffer + i, sAlignedBuffer, SD_PAGE_SIZE );
    }

    // ������ ���Ͽ� ���۸� ����Ѵ�.
    //   1. sUnitSize ������ ����Ѵ�.
    for ( i = 0;
          i < ( sExtendByte / sWriteUnitSize );
          i++ )
    {
        IDU_FIT_POINT( "1.BUG-12754@sddDataFile::extend" );

        /* BUG-18138 ���̺����̽� �����ϴٰ� ��ũ Ǯ ��Ȳ�� �Ǹ�
         * �ش� ������ ������ ���� �ʽ��ϴ�.
         *
         * ������ Disk IO�� �����Ҷ����� ����ؼ� ������ ��������ϴ�.
         * �ٷ� ������ �߻��ϸ� ����ó���ϵ��� �����Ͽ����ϴ�. */
        IDE_TEST( aDataFileNode->mFile.write(
                      aStatistics,
                      sWriteOffset,
                      sAlignedBuffer,
                      sWriteUnitSize )
                  != IDE_SUCCESS);

        sWriteOffset += sWriteUnitSize;

        IDU_FIT_POINT( "1.BUG-18138@sddDataFile::extend" );

        // BUG-12754 : session ������ ������� �˻���
        IDE_TEST(iduCheckSessionEvent(aStatistics) != IDE_SUCCESS);
    }

    sRestSize = sExtendByte - ( sWriteUnitSize * i );

    if( sRestSize != 0 )
    {
        //   2. sExtendByte - sUnitSize * i ũ�⸸ŭ ����Ѵ�.
        IDE_TEST(aDataFileNode->mFile.write(
                     aStatistics,
                     sWriteOffset,
                     sAlignedBuffer,
                     sRestSize )
                 != IDE_SUCCESS);
    }

    IDE_TEST( aDataFileNode->mFile.syncUntilSuccess(
                  smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    // PROJ-2133 Incremental backup
    // temptablespace �� ���ؼ��� change tracking �� �ʿ䰡 �����ϴ�.
    if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) &&
         ( sctTableSpaceMgr::isTempTableSpace( aDataFileNode->mSpaceID ) != ID_TRUE ) )
    {
        sTotalExtendedPageCnt = ( sExtendByte / SD_PAGE_SIZE );

        for( sExtendedPageCnt = 0; 
             sExtendedPageCnt <  sTotalExtendedPageCnt;
             sExtendedPageCnt++,
             sExtendWriteOffset4CT += SD_PAGE_SIZE )
        {
            sExtendedPageID = (sExtendWriteOffset4CT - SM_DBFILE_METAHDR_PAGE_SIZE) 
                              / SD_PAGE_SIZE;

            sIBChunkID = smriChangeTrackingMgr::calcIBChunkID4DiskPage( 
                                                            sExtendedPageID );

            IDE_TEST( smriChangeTrackingMgr::changeTracking(
                            aDataFileNode,
                            NULL,
                            sIBChunkID )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* nothing to do */
    }

    //==========================================================================
    // To Fix BUG-13924
    //==========================================================================
    ideLog::log( SM_TRC_LOG_LEVEL_DISK,
                 SM_TRC_DISK_FILE_EXTEND,
                 sWriteOffset - sExtendByte,
                 sWriteOffset,
                 (aDataFileNode->mName == NULL) ? "" : aDataFileNode->mName);

    sState = 0;
    IDE_TEST( iduMemMgr::free( sBufferPtr )
              != IDE_SUCCESS );

    sBufferPtr     = NULL;
    sAlignedBuffer = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free( sBufferPtr ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : �ϳ��� datafile�� �����Ѵ�.
 *
 * + 2nd. code design
 *   - datafile�� open�Ѵ�.
 *   - datafile ��忡 open ���� ����
 *
 * BUG-47398 AIX ���� iduMemMgr::free4malign() �Լ����� ������ ����
 *           Direct IO ���� ȣ�� �Լ����� �����Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::open( sddDataFileNode* aDataFileNode,
                          idBool           aIsDirectIO )
{
    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aDataFileNode->mIsOpened != ID_TRUE );

    IDU_FIT_POINT( "BUG-46596@sddDataFile::open::exceptionTest");
    IDE_TEST( aDataFileNode->mFile.openUntilSuccess( aIsDirectIO ) != IDE_SUCCESS );

    aDataFileNode->mIsOpened = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile�� close�Ѵ�.
 *
 * + 2nd. code design
 *   - datafile�� �ݴ´�.
 *   - datafile ��忡 close ��带 �����Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::close( sddDataFileNode  *aDataFileNode )
{
    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( isOpened(aDataFileNode) == ID_TRUE );
    IDE_DASSERT( getIOCount(aDataFileNode) == 0 );

    if ( aDataFileNode->mIsModified == ID_TRUE )
    {
        IDE_TEST( sync( aDataFileNode,
                        smLayerCallback::setEmergency ) != IDE_SUCCESS );
        aDataFileNode->mIsModified = ID_FALSE;
    }

    // XXX ������ ������ ������ ����.. 
    aDataFileNode->mIsOpened = ID_FALSE;

    IDE_TEST( aDataFileNode->mFile.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : datafile�� ����Ѵ�.(truncate)
 **********************************************************************/
IDE_RC sddDataFile::truncate( sddDataFileNode*  aDataFileNode,
                              ULong             aNewDataFileSize )
{

    ULong sOldFileSize = 0;

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aDataFileNode->mIsOpened == ID_TRUE );

    // To Fix BUG-13924
    IDE_TEST( aDataFileNode->mFile.getFileSize( &sOldFileSize ) != IDE_SUCCESS );

    //BUGBUG.
    IDE_TEST( aDataFileNode->mFile.truncate( SDD_CALC_PAGEOFFSET(aNewDataFileSize) )
              != IDE_SUCCESS );

    IDE_TEST( aDataFileNode->mFile.syncUntilSuccess(
                  smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    //====================================================================
    // To Fix BUG-13924
    //====================================================================

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_DISK_FILE_TRUNCATE,
                sOldFileSize,
                SDD_CALC_PAGEOFFSET(aNewDataFileSize),
                (aDataFileNode->mName == NULL) ? "" : aDataFileNode->mName);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;


}


/***********************************************************************
 * Description : ���� datafile�� ������ Ʈ����� Ŀ�� ������ �����.
 **********************************************************************/
IDE_RC sddDataFile::addPendingOperation( void             *aTrans,
                                         sddDataFileNode  *aDataFileNode,
                                         idBool            aIsCommit,
                                         sctPendingOpType  aPendingOpType,
                                         sctPendingOp     **aPendingOp )
{
    sctPendingOp *sPendingOp;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aDataFileNode != NULL );

    IDE_TEST( sctTableSpaceMgr::addPendingOperation( aTrans,
                                                     aDataFileNode->mSpaceID,
                                                     aIsCommit,
                                                     aPendingOpType,
                                                     & sPendingOp )
              != IDE_SUCCESS );

    sPendingOp->mFileID        = aDataFileNode->mID;

    if ( aPendingOp != NULL )
    {
        *aPendingOp = sPendingOp;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description : datafile����� autoextend�� ��� ����
 *               autoexnted on/off, next size, maxsize�� �� �Լ���
 *               ���ؼ� �ٲ�߸� �Ѵ�.
 *               autoextend on/off�� ���� next size, maxsize��
 *               consistent�� ���� �ִ�.
 **********************************************************************/
void sddDataFile::setAutoExtendProp( sddDataFileNode* aDataFileNode,
                                     idBool           aAutoExtendMode,
                                     ULong            aNextSize,
                                     ULong            aMaxSize )
{
    IDE_DASSERT( aDataFileNode != NULL );

    if ( aAutoExtendMode == ID_FALSE )
    {
        // autoextend off�� ���
        // nextsize�� maxsize�� ��� 0 �̾�� �Ѵ�.
        // ������� ������ �ڵ�� ���� �� ����� �ϹǷ�
        // assert�� �����Ѵ�.
        IDE_ASSERT(aNextSize == 0);
        IDE_ASSERT(aMaxSize == 0);
    }

    //====================================================================
    // To Fix BUG-13924
    //====================================================================

    ideLog::log( SM_TRC_LOG_LEVEL_DISK,
                 SM_TRC_DISK_FILE_EXTENDMODE,
                 aDataFileNode->mIsAutoExtend,
                 aAutoExtendMode,
                 (aDataFileNode->mName == NULL) ? "" : aDataFileNode->mName );

    aDataFileNode->mIsAutoExtend = aAutoExtendMode;
    aDataFileNode->mNextSize = aNextSize;
    aDataFileNode->mMaxSize = aMaxSize;
}

/***********************************************************************
 * Description : datafile ����� currsize�� set�Ѵ�.
 **********************************************************************/
void sddDataFile::setCurrSize( sddDataFileNode*  aDataFileNode,
                               ULong             aSize )
{


    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aSize <= SD_MAX_PAGE_COUNT);

    //====================================================================
    // To Fix BUG-13924
    //====================================================================

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_DISK_FILE_CURRSIZE,
                aDataFileNode->mCurrSize,
                aSize,
                (aDataFileNode->mName == NULL) ? "" : aDataFileNode->mName);

    aDataFileNode->mCurrSize = aSize;

    return;

}

/***********************************************************************
 * Description : datafile ����� initsize�� set�Ѵ�.
 * truncate�� ���� ���Ѵ�.
 **********************************************************************/
void sddDataFile::setInitSize( sddDataFileNode*  aDataFileNode,
                               ULong             aSize )
{

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aSize <= SD_MAX_PAGE_COUNT );

    //====================================================================
    // To Fix BUG-13924
    //====================================================================

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_DISK_FILE_INITSIZE,
                aDataFileNode->mInitSize,
                aSize,
                (aDataFileNode->mName == NULL) ? "" : aDataFileNode->mName);

    aDataFileNode->mInitSize = aSize;

}

/***********************************************************************
 * Description : datafile ����� ���ο� datafile �̸� ����
 *
 * # ��, MEDIA RECOVERY���������� ���ο� ����Ÿ ���ϻ����� ���Ͽ�,
 *   ���� ���� ����� �̸��� �����Ѵ�.
 *   - alter database create datafile �Ǵ�
 *     alter database rename file�� �����Ҷ� �Ҹ�.
 **********************************************************************/
IDE_RC sddDataFile::setDataFileName( sddDataFileNode* aDataFileNode,
                                     SChar*           aNewName,
                                     idBool           aCheckAccess )
{

    SChar* sOldFileName;
    UInt   sStrLen;
    UInt   sState = 0;

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aNewName != NULL );

    sOldFileName = aDataFileNode->mName;

    if (aCheckAccess == ID_TRUE)
    {
        IDE_TEST_RAISE( idf::access(aNewName, F_OK) != 0,
                        error_not_exist_file );
        IDE_TEST_RAISE( idf::access(aNewName, R_OK) != 0,
                        error_no_read_perm_file );
        IDE_TEST_RAISE( idf::access(aNewName, W_OK) != 0,
                        error_no_write_perm_file );
    }

    sStrLen = idlOS::strlen(aNewName);

    /* sddDataFile_setDataFileName_malloc_Name.tc */
    IDU_FIT_POINT("sddDataFile::setDataFileName::malloc::Name");
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_SM_SDD,
                                sStrLen + 1,
                                (void**)&(aDataFileNode->mName),
                                IDU_MEM_FORCE) != IDE_SUCCESS );
    sState = 1;

    idlOS::memset(aDataFileNode->mName, 0x00, sStrLen);
    idlOS::strcpy(aDataFileNode->mName, aNewName);


    /* BUG-31401 [sm-disk-recovery] When changing a Data file name, 
     *           does not refresh FD of DataFileNode.
     * ������ ������ �ݾƾ�����, ���ſ� ����ϴ� FD�� ���� �� �ֽ��ϴ�.
     * �׷��� ������, ������ �������� ������ ������ �����մϴ�. */
    if( aDataFileNode->mIsOpened == ID_TRUE )
    {
        IDE_TEST( aDataFileNode->mFile.close() != IDE_SUCCESS );
        sState = 2;
    }

    IDE_TEST( aDataFileNode->mFile.setFileName(aDataFileNode->mName) != IDE_SUCCESS );

    if( sState == 2 )
    {
        IDE_TEST( aDataFileNode->mFile.open(
                ((smuProperty::getIOType() == 0) ?
                 ID_FALSE : ID_TRUE)) != IDE_SUCCESS );
        sState = 1;
    }

 
    //====================================================================
    // To Fix BUG-13924
    //====================================================================

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_DISK_FILE_RENAME,
                (sOldFileName == NULL) ? "" : sOldFileName, aNewName);

    if ( sOldFileName != NULL )
    {
        IDE_TEST( iduMemMgr::free(sOldFileName) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_exist_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath, aNewName ) );
    }
    IDE_EXCEPTION( error_no_read_perm_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NoReadPermFile, aNewName ) );
    }
    IDE_EXCEPTION( error_no_write_perm_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NoWritePermFile, aNewName ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( aDataFileNode->mFile.open(
                    ((smuProperty::getIOType() == 0) ?
                     ID_FALSE : ID_TRUE)) == IDE_SUCCESS );

            /* BUG-33353 
             * ���ܻ�Ȳ���� �Ҵ���� �޸𸮸� ���� ���� �ʰ� �־���. */
        case 1:
            (void)iduMemMgr::free( aDataFileNode->mName );

        default:
            break;
    }

    return IDE_FAILURE;
}
/***********************************************************************
 *   ����Ÿ���� HEADER�� üũ����Ʈ ������ ����Ѵ�.
 *
 *  [IN] aDataFileNode  : DBF Node ����Ʈ
 *  [IN] aRedoSN        : �̵����� �ʿ��� Disk Redo SN
 *  [IN] aDiskCreateLSN : �̵����� �ʿ��� Disk Create LSN
 *  [IN] aMustRedoToLSN : �̵����� �ʿ��� Must Redo To LSN
 *  [IN] aDataFileDescSlotID : change tracking�� ���� CT������ slotID
 **********************************************************************/
void sddDataFile::setDBFHdr( sddDataFileHdr*            aFileMetaHdr,
                             smLSN*                     aRedoLSN,
                             smLSN*                     aCreateLSN,
                             smLSN*                     aMustRedoToLSN,
                             smiDataFileDescSlotID*     aDataFileDescSlotID )
{
    IDE_DASSERT( aFileMetaHdr != NULL );

    if ( aRedoLSN != NULL )
    {
        SM_GET_LSN( aFileMetaHdr->mRedoLSN, *aRedoLSN );
    }

    if ( aCreateLSN != NULL )
    {
        SM_GET_LSN( aFileMetaHdr->mCreateLSN, *aCreateLSN );
    }

    if ( aMustRedoToLSN != NULL )
    {
        SM_GET_LSN( aFileMetaHdr->mMustRedoToLSN, *aMustRedoToLSN );
    }

    //PROJ-2133 incremental backup
    if ( aDataFileDescSlotID != NULL )
    {
        aFileMetaHdr->mDataFileDescSlotID.mBlockID = 
                                    aDataFileDescSlotID->mBlockID;

        aFileMetaHdr->mDataFileDescSlotID.mSlotIdx = 
                                    aDataFileDescSlotID->mSlotIdx;
    }

    return;
}

/***********************************************************************
 * Description : PRJ-1149  media recovery�� �ʿ����� �˻�
 * binary db version�� ����ġ�ϰų� create lsn�� ���� �߸��� ���
 * invalid file�̸�, oldest lst�� ����ġ�ϴ� ���� media recovery��
 * �ʿ��� ���� �Ǵ��Ѵ�.
 *
 *    aFileNode       - [IN]  Loganchor���� ���� FileNode
 *    aDBFileHdr      - [IN]  DBFile���� ���� DBFileHdr
 *    aIsMediaFailure - [OUT] MediaRecovery�� �ʿ������� ��ȯ�Ѵ�.
 **********************************************************************/
IDE_RC sddDataFile::checkValidationDBFHdr(
                         sddDataFileNode  * aFileNode,
                         sddDataFileHdr   * aDBFileHdr,
                         idBool           * aIsMediaFailure )
{
    UInt              sDBVer    = 0;
    UInt              sCtlVer   = 0;
    UInt              sFileVer  = 0;
    sddDataFileHdr*   sDBFileHdrOfNode;

    IDE_DASSERT( aFileNode       != NULL );
    IDE_DASSERT( aDBFileHdr      != NULL );
    IDE_DASSERT( aIsMediaFailure != NULL );

    sDBFileHdrOfNode = &(aFileNode->mDBFileHdr);

#ifdef DEBUG
    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_MRECOVERY_CHECK_DB_SID_FID,
                aFileNode->mSpaceID,
                aFileNode->mID);

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_MRECOVERY_CHECK_DISK_RLSN,
                sDBFileHdrOfNode->mRedoLSN.mFileNo,
                sDBFileHdrOfNode->mRedoLSN.mOffset,
                aDBFileHdr->mRedoLSN.mFileNo,
                aDBFileHdr->mRedoLSN.mOffset);

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_MRECOVERY_CHECK_DISK_CLSN,
                sDBFileHdrOfNode->mCreateLSN.mFileNo,
                sDBFileHdrOfNode->mCreateLSN.mOffset,
                aDBFileHdr->mCreateLSN.mFileNo,
                aDBFileHdr->mCreateLSN.mOffset);
#endif

    IDE_TEST_RAISE( checkValuesOfDBFHdr( sDBFileHdrOfNode )
                    != IDE_SUCCESS,
                    err_invalid_hdr );

    sDBVer   = smVersionID & SM_CHECK_VERSION_MASK;
    sCtlVer  = sDBFileHdrOfNode->mSmVersion & SM_CHECK_VERSION_MASK;
    sFileVer = aDBFileHdr->mSmVersion & SM_CHECK_VERSION_MASK;

    // [1] SM VERSION Ȯ��
    // ����Ÿ���ϰ� ���� ���̳ʸ��� ȣȯ���� �˻��Ѵ�.
    IDE_TEST_RAISE(sDBVer != sCtlVer, err_invalid_hdr);
    IDE_TEST_RAISE(sDBVer != sFileVer, err_invalid_hdr);

    // [2] CREATE SN Ȯ��
    // ����Ÿ������ Create SN�� Loganchor�� Create SN�� �����ؾ��Ѵ�.
    // �ٸ��� ���ϸ� ������ ������ �ٸ� ����Ÿ�����̴�.
    // CREATE LSNN
    IDE_TEST_RAISE( smLayerCallback::isLSNEQ( &(sDBFileHdrOfNode->mCreateLSN),
                                              &(aDBFileHdr->mCreateLSN) )
                    != ID_TRUE, err_invalid_hdr );

    // [3] Redo LSN Ȯ��
    if ( smLayerCallback::isLSNLTE(
                &sDBFileHdrOfNode->mRedoLSN,
                &aDBFileHdr->mRedoLSN) == ID_TRUE )
    {
        // ����Ÿ ������ REDO SN���� Loganchor�� Redo SN�� �� �۰ų�
        // ���ƾ� �Ѵ�.

        *aIsMediaFailure = ID_FALSE; // �̵�� ���� ���ʿ�

#ifdef DEBUG
        ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                    SM_TRC_MRECOVERY_CHECK_DB_SUCCESS);
#endif
    }
    else
    {
        // ����Ÿ ������ REDO SN���� Loganchor�� Redo SN�� �� ũ��
        // �̵�� ������ �ʿ��ϴ�.

        *aIsMediaFailure = ID_TRUE; // �̵�� ���� �ʿ�

#ifdef DEBUG
        ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                    SM_TRC_MRECOVERY_CHECK_NEED_MEDIARECOV);
#endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_hdr );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFileHdr,
                                aFileNode->mSpaceID,
                                aFileNode->mID));

        /* BUG-33353 */
        ideLog::log(IDE_SM_0,
                    SM_TRC_MRECOVERY_CHECK_DB_SID_FID,
                    aFileNode->mSpaceID,
                    aFileNode->mID);

        ideLog::log(IDE_SM_0,
                    SM_TRC_MRECOVERY_CHECK_DISK_RLSN,
                    sDBFileHdrOfNode->mRedoLSN.mFileNo,
                    sDBFileHdrOfNode->mRedoLSN.mOffset,
                    aDBFileHdr->mRedoLSN.mFileNo,
                    aDBFileHdr->mRedoLSN.mOffset);

        ideLog::log(IDE_SM_0,
                    SM_TRC_MRECOVERY_CHECK_DISK_CLSN,
                    sDBFileHdrOfNode->mCreateLSN.mFileNo,
                    sDBFileHdrOfNode->mCreateLSN.mOffset,
                    aDBFileHdr->mCreateLSN.mFileNo,
                    aDBFileHdr->mCreateLSN.mOffset);

        ideLog::log( IDE_SM_0,
                     "DBVer = %u"
                     ", CtrVer = %u"
                     ", FileVer = %u",
                     sDBVer, sCtlVer, sFileVer );
    }
    IDE_EXCEPTION_END;

    ideLog::log(SM_TRC_LOG_LEVEL_DISK,
                SM_TRC_MRECOVERY_CHECK_DB_FAILURE);

    return IDE_FAILURE;
}

/*
   ����Ÿ���� HEADERD�� ��ȿ���� �˻��Ѵ�.

   [IN] aDBFileHdr : sddDataFileHdr Ÿ���� ������
*/
IDE_RC sddDataFile::checkValuesOfDBFHdr(
                          sddDataFileHdr*  aDBFileHdr )
{
    smLSN sCmpLSN;

    IDE_DASSERT( aDBFileHdr != NULL );

    // REDO LSN�� CREATE LSN�� 0�̰ų� ULONG_MAX ���� ���� �� ����.
    SM_LSN_INIT(sCmpLSN);
    IDE_TEST( smLayerCallback::isLSNEQ( &aDBFileHdr->mRedoLSN,
                                        &sCmpLSN ) == ID_TRUE );

    IDE_TEST( smLayerCallback::isLSNEQ( &aDBFileHdr->mCreateLSN,
                                        &sCmpLSN ) == ID_TRUE );

    SM_LSN_MAX(sCmpLSN);
    IDE_TEST( smLayerCallback::isLSNEQ( &aDBFileHdr->mRedoLSN,
                                        &sCmpLSN ) == ID_TRUE );

    IDE_TEST( smLayerCallback::isLSNEQ( &aDBFileHdr->mCreateLSN,
                                        &sCmpLSN ) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

void sddDataFile::getDataFileAttr( sddDataFileNode*   aDataFileNode,
                                   smiDataFileAttr*   aDataFileAttr )
{
    sddDataFileHdr * sFileMetaHdr;

    IDE_DASSERT( aDataFileNode != NULL );
    IDE_DASSERT( aDataFileAttr != NULL );

    sFileMetaHdr = &(aDataFileNode->mDBFileHdr);

    // PRJ-1548 User Memory Tablespace
    aDataFileAttr->mAttrType  = SMI_DBF_ATTR;
    aDataFileAttr->mSpaceID   = aDataFileNode->mSpaceID;
    aDataFileAttr->mID        = aDataFileNode->mID;

    idlOS::strcpy(aDataFileAttr->mName, aDataFileNode->mName);
    aDataFileAttr->mNameLength = idlOS::strlen(aDataFileNode->mName);

    aDataFileAttr->mIsAutoExtend = aDataFileNode->mIsAutoExtend;

    aDataFileAttr->mState        = aDataFileNode->mState;
    aDataFileAttr->mMaxSize      = aDataFileNode->mMaxSize;
    aDataFileAttr->mNextSize     = aDataFileNode->mNextSize;
    aDataFileAttr->mCurrSize     = aDataFileNode->mCurrSize;
    aDataFileAttr->mInitSize     = aDataFileNode->mInitSize;
    aDataFileAttr->mCreateMode   = aDataFileNode->mCreateMode;

    //PROJ-2133 incremental backup
    if ( ( smLayerCallback::isCTMgrEnabled() == ID_TRUE ) || 
         ( smLayerCallback::isCreatingCTFile() == ID_TRUE ) )
    {
        aDataFileAttr->mDataFileDescSlotID.mBlockID = 
                        aDataFileNode->mDBFileHdr.mDataFileDescSlotID.mBlockID;
        aDataFileAttr->mDataFileDescSlotID.mSlotIdx = 
                        aDataFileNode->mDBFileHdr.mDataFileDescSlotID.mSlotIdx;
    }
    else
    {
        aDataFileAttr->mDataFileDescSlotID.mBlockID = 
                                    SMRI_CT_INVALID_BLOCK_ID;
        aDataFileAttr->mDataFileDescSlotID.mSlotIdx = 
                                    SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;
    }
    //PRJ-1149 backup file header copy added.
    SM_GET_LSN( aDataFileAttr->mCreateLSN,
                sFileMetaHdr->mCreateLSN );
    return;
}

/*
 * Disk Data File Header�� ����Ѵ�.
 *
 * aDataFileNode - [IN] Data file node
 * aDBFileHdr    - [IN] ��ũ ����Ÿ������ ��Ÿ���
 *
 */
IDE_RC sddDataFile::writeDBFileHdr( idvSQL          * aStatistics,
                                    sddDataFileNode * aDataFileNode,
                                    sddDataFileHdr  * aDBFileHdr )
{
    SChar *sHeadPageBuffPtr = NULL;
    SChar *sHeadPageBuff = NULL;

    /* fix BUG-17825 Data File Header����� Process Failure��
     *               Startup�ȵ� �� ����
     *
     * ��� ���Ͻý��ۿ����� Block( 512 Bytes )�� ���� Atomic IO�� �����Ѵ�.
     * Write�ϰ��� �ϴ� �ڷᰡ Block ��迡 �ɸ����� �ʴ´ٸ� �����ϰ� ������
     * �ų� �ƿ� �������� �ʱ� ������ �ڷᰡ �������ɼ��� ����.
     * sddDataFileHdr�� ����Ÿ������ 0 offset�� ����ǰ� ũ�Ⱑ 512����Ʈ ����
     * �̱� ������  Block ��迡 ���ļ� ���Ե��� �����Ƿ� Atomic�ϰ�
     * Disk�� ������ �� �ִ�.
     *
     * ���� 512 Bytes �Ѵ´ٸ�, Double Write �������� �����Ǿ�� �Ѵ�. */
    
    /* PROJ-2133 incremental backup
     * ��������� backupinfo�� �߰����������� 512bytes�� �Ѱ� �ǳ� ������Ͽ���
     * �����ϴ� ������ �Ϲ����� ���������Ͽ��� �������ʴ´�.
     * ���� backup info���� ũ�⸦ datafileHdr���� ���� 512byte�� �Ѵ��� �˻��Ѵ�.
     */
    IDE_ASSERT( (ID_SIZEOF( sddDataFileHdr ) - ID_SIZEOF( smriBISlot )) < 512 );

    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDD,
                                           SD_PAGE_SIZE,
                                           (void**)&sHeadPageBuffPtr,
                                           (void**)&sHeadPageBuff )
              != IDE_SUCCESS );

    if ( aDBFileHdr != NULL )
    {
        idlOS::memcpy( sHeadPageBuff, aDBFileHdr, ID_SIZEOF( sddDataFileHdr ) );
    }
    else
    {
        idlOS::memcpy( sHeadPageBuff,
                       &( aDataFileNode->mDBFileHdr ),
                       ID_SIZEOF( sddDataFileHdr ) );
    }

    aDataFileNode->mFile.writeUntilSuccess(
                                    aStatistics,
                                    SM_DBFILE_METAHDR_PAGE_OFFSET,
                                    sHeadPageBuff,
                                    SM_DBFILE_METAHDR_PAGE_SIZE,
                                    smLayerCallback::setEmergency );

    IDE_TEST( iduMemMgr::free( sHeadPageBuffPtr ) != IDE_SUCCESS );
    sHeadPageBuffPtr = NULL;
    sHeadPageBuff = NULL;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if( sHeadPageBuffPtr != NULL )
    {
        IDE_PUSH();
        IDE_ASSERT( iduMemMgr::free( sHeadPageBuffPtr ) == IDE_SUCCESS );
        IDE_POP();

        sHeadPageBuffPtr = NULL;
    }
    return IDE_FAILURE;
}


/***********************************************************************
 * ������ DBFile�� DiskDataFileHeader�� ����Ѵ�.
 *
 * aBBFilePath - [IN] DB file�� ���
 * aDBFileHdr  - [IN] ��ũ ����Ÿ������ ��Ÿ���
 **********************************************************************/
IDE_RC sddDataFile::writeDBFileHdrByPath( SChar           * aDBFilePath,
                                          sddDataFileHdr  * aDBFileHdr )
{
    iduFile sFile;

    IDE_ASSERT( aDBFilePath != NULL );
    IDE_ASSERT( aDBFileHdr  != NULL );

    IDE_TEST( sFile.initialize( IDU_MEM_SM_SDD,
                                1, /* Max Open FD Count */
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    IDE_TEST( sFile.setFileName( aDBFilePath ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sFile.open() != IDE_SUCCESS , err_file_not_found);

    IDE_TEST_RAISE( sFile.write( NULL, /*idvSQL*/
                                 SM_DBFILE_METAHDR_PAGE_OFFSET,
                                 aDBFileHdr,
                                 ID_SIZEOF(sddDataFileHdr) )
                    != IDE_SUCCESS, err_dbfilehdr_write_failure );

    IDE_TEST( sFile.close()   != IDE_SUCCESS );
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_file_not_found );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath,
                                  aDBFilePath ) );
    }
    IDE_EXCEPTION( err_dbfilehdr_write_failure );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_Datafile_Header_Write_Failure,
                                  aDBFilePath ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#if 0 //not used
/***********************************************************************
 * Description : datafile ����� ������ ���
 **********************************************************************/
IDE_RC sddDataFile::dumpDataFileNode( sddDataFileNode* aDataFileNode )
{
    sddDataFileHdr*    sFileMetaHdr;
    SChar              sFileStatusBuff[500 + 1];

    IDE_ERROR( aDataFileNode != NULL );

    sFileMetaHdr = &( aDataFileNode->mDBFileHdr );

    idlOS::printf("[DUMPDBF-BEGIN]\n");
    idlOS::printf("[HEADER]OLSN: <%"ID_UINT32_FMT",%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n",
                  sFileMetaHdr->mRedoLSN.mFileNo,
                  sFileMetaHdr->mRedoLSN.mOffset);
    idlOS::printf("[HEADER]CLSN    : <%"ID_UINT32_FMT",%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n",
                  sFileMetaHdr->mCreateLSN.mFileNo,
                  sFileMetaHdr->mCreateLSN.mOffset);

    idlOS::printf("[GENERAL]DBF PATH: %s\n", aDataFileNode->mName );

    idlOS::printf("[GENERAL]Create Mode: %s\n",
                  (aDataFileNode->mCreateMode == SMI_DATAFILE_CREATE)?
                  (SChar*)"create":(SChar*)"reuse");

    idlOS::printf("[GENERAL]Init Size: %"ID_UINT64_FMT"\n", aDataFileNode->mInitSize);
    idlOS::printf("[GENERAL]Curr Size: %"ID_UINT64_FMT"\n", aDataFileNode->mCurrSize);
    idlOS::printf("[GENERAL]AutoExtend Mode: %s\n", (aDataFileNode->mIsAutoExtend == ID_TRUE)?
                  (SChar*)"on":(SChar*)"off");

    // klocwork SM
    idlOS::memset( sFileStatusBuff, 0x00, 500 + 1);

    if( SMI_FILE_STATE_IS_OFFLINE( aDataFileNode->mState) )
    {
        idlOS::strncat(sFileStatusBuff,"OFFLINE | ", 500);
    }
    if( SMI_FILE_STATE_IS_ONLINE( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"ONLINE | ", 500);
    }
    if( SMI_FILE_STATE_IS_CREATING( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"CREATING | ", 500);
    }
    if( SMI_FILE_STATE_IS_DROPPING( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"DROPPING | ", 500);
    }
    if( SMI_FILE_STATE_IS_RESIZING( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"RESIZING | ", 500);
    }
    if( SMI_FILE_STATE_IS_DROPPED( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"DROPPED | ", 500);
    }
    if( SMI_FILE_STATE_IS_BACKUP( aDataFileNode->mState ) )
    {
        idlOS::strncat(sFileStatusBuff,"BACKUP | ", 500);
    }

    sFileStatusBuff[idlOS::strlen(sFileStatusBuff) - 2] = 0;

    idlOS::printf("[GENERAL]STATE: %s\n",sFileStatusBuff);
    idlOS::printf("[GENERAL]Next Size: %"ID_UINT64_FMT"\n", aDataFileNode->mNextSize);
    idlOS::printf("[GENERAL]Max  Size: %"ID_UINT64_FMT"\n", aDataFileNode->mMaxSize);
    idlOS::printf("[GENERAL]Open Mode: %s\n", (aDataFileNode->mIsOpened == ID_TRUE)?
                  (SChar*)"open":(SChar*)"close");
    idlOS::printf("[GENERAL]I/O Count: %"ID_UINT32_FMT"\n", aDataFileNode->mIOCount);
    idlOS::printf("[GENERAL]Modified: %s\n", (aDataFileNode->mIsModified == ID_TRUE)?
                  (SChar*)"yes":(SChar*)"no");
    idlOS::printf("[DUMPDBF-END]\n");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif
