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
 * $$Id:$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102  The Fast Secondary Buffer
 * file I/O �� ����Ÿ���� ����Ǵ� �������� ������ ���� 
 **********************************************************************/

#include <idu.h>
#include <idl.h>

#include <smDef.h>
#include <smErrorCode.h>
#include <smiDef.h>
#include <sddDef.h>
#include <sdbDef.h>
#include <smrDef.h>
#include <smu.h>
#include <sds.h>
#include <sdsReq.h>

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC sdsFile::initialize( UInt aGroupCnt, 
                            UInt aPageCnt )
{
    initializeStatic( aGroupCnt, aPageCnt );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC sdsFile::destroy()
{ 
    if( mFileNode != NULL )
    {
        close( NULL /*aStatistics*/ );

        destroyStatic();
    }
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC sdsFile::initializeStatic( UInt aGroupCnt, 
                                  UInt aPageCnt )
{
    mSBufferDirName = (SChar *)smuProperty::getSBufferDirectoryName();
    mFrameCntInExt = smuProperty::getBufferIOBufferSize();
    /* Secondary Buffer File�� �����ϴ� Group �� �� */
    mGroupCnt = aGroupCnt;    
    /* Group �� �����ϴ� page�� �� */
    mPageCntInGrp   = aPageCnt;
    /* Group �� �ϳ��� �����ϴ� MetaTable�� �����ϴ� page �� */
    mPageCntInMetaTable = mGroupCnt * SDS_META_TABLE_PAGE_COUNT;

    IDE_TEST( mFileMutex.initialize(
                        (SChar*)"SECONDARY_BUFFER_FILE_MUTEX",
                        IDU_MUTEX_KIND_POSIX,
                        IDV_WAIT_INDEX_LATCH_FREE_OTHERS )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC sdsFile::destroyStatic()
{
    IDE_TEST( freeFileNode() != IDE_SUCCESS );
    IDE_TEST( mFileMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ��� �Ҵ�
   aFilePageCnt - [IN]  ���� ��û�ϴ� ���� ũ��  
                        Secondary Buffer Size + MetaTable ũ��
   aFileName    - [IN]  ���� ��û�ϴ� �̸� 
   aFileNode    - [OUT] ������ ��带 ��ȯ
 ************************************************************************/
IDE_RC sdsFile::initFileNode( ULong           aFilePageCnt, 
                              SChar         * aFileName, 
                              sdsFileNode  ** aFileNode )
{
    if( aFileNode == NULL )
    {
        IDE_TEST( makeValidABSPath( aFileName ) != IDE_SUCCESS );
    }

    IDU_FIT_POINT_RAISE( "sdsFile::initFileNode::malloc1", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       ID_SIZEOF(sdsFileNode),
                                       (void**)&mFileNode ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );

    idlOS::memset( mFileNode, 0x00, ID_SIZEOF(sdsFileNode) );

    mFileNode->mFileHdr.mSmVersion  = smVersionID;
    SM_LSN_INIT( mFileNode->mFileHdr.mRedoLSN );
    SM_LSN_INIT( mFileNode->mFileHdr.mCreateLSN );
    mFileNode->mFileHdr.mFrameCntInExt      = SDS_FILEHDR_IOB_MAX;
    mFileNode->mFileHdr.mLstMetaTableSeqNum = SDS_FILEHDR_SEQ_MAX; 
 
    mFileNode->mState    = SMI_FILE_ONLINE;
    mFileNode->mPageCnt  = aFilePageCnt;
    idlOS::strcpy( mFileNode->mName, aFileName );
    mFileNode->mIsOpened = ID_FALSE;

    if( aFileNode != NULL )
    {
        *aFileNode = mFileNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSUFFICIENT_MEMORY );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
} 

/************************************************************************
 * Description : ��� ���� 
 ************************************************************************/
IDE_RC sdsFile::freeFileNode()
{
    IDE_TEST( mFileNode->mFile.destroy() != IDE_SUCCESS );
    IDE_TEST( iduMemMgr::free( mFileNode ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*******************************************************************************
 * Description :
   1. valid �� node ���� �� �˻� 
   2. Valid �� ���Ͼ����� ���� 
 ******************************************************************************/
IDE_RC sdsFile::identify( idvSQL  * aStatistics )
{
    sdsFileNode * sFileNode;
    idBool        sIsValidFileExist = ID_FALSE;
    idBool        sIsCreate         = ID_FALSE;

    getFileNode( &sFileNode );

    IDE_TEST( makeSureValidationNode( aStatistics,
                                      sFileNode, 
                                      &sIsValidFileExist,
                                      &sIsCreate )
              != IDE_SUCCESS );

    if( sIsValidFileExist != ID_TRUE )
    {
        IDE_TEST( create( aStatistics, sIsCreate ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ������ �����Ѵ�. 
 **********************************************************************/
IDE_RC sdsFile::create( idvSQL * aStatistics, idBool aIsCreate )
{
    sdsFileNode   * sFileNode;
    smLSN           sEndLSN;
    smLSN           sCreateLSN;
    ULong           sFileSize    = 0;
    ULong           sCurrPageCnt = 0;

    getFileNode( &sFileNode );
    IDE_ASSERT( sFileNode != NULL );

    sFileNode->mState = SMI_FILE_ONLINE;

    /* ���� �����ڰ� ���۵��� �ʾƼ� �α� ��Ŀ���� �ٷ� LSN�� �о�´�
       restart�� ȣ��ȴ�. */
    smLayerCallback::getEndLSN( &sEndLSN );
    SM_GET_LSN( sCreateLSN, sEndLSN );

    setFileHdr( &(sFileNode->mFileHdr),
                smLayerCallback::getDiskRedoLSN(),
                &sCreateLSN,
                &mFrameCntInExt,         
                NULL );  /* Sequence Num */

    IDE_TEST( sFileNode->mFile.initialize( 
                         IDU_MEM_SM_SDS,
                         smuProperty::getMaxOpenFDCount4File(),
                         IDU_FIO_STAT_ON,
                         IDV_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_FILEIO )
              != IDE_SUCCESS );

    IDE_TEST( sFileNode->mFile.setFileName(sFileNode->mName) != IDE_SUCCESS );

    /* ���� �̸��� �ִ��� �˻� ������. ����� ������ ��Ȱ�� */
    if( idf::access( sFileNode->mName, F_OK ) == 0 )
    {
        IDE_TEST( open( aStatistics ) != IDE_SUCCESS );

        IDE_TEST( sFileNode->mFile.getFileSize(&sFileSize) != IDE_SUCCESS );

        sCurrPageCnt = (ULong)((sFileSize - SM_DBFILE_METAHDR_PAGE_SIZE) / 
                       SD_PAGE_SIZE);

        if( sCurrPageCnt > sFileNode->mPageCnt )
        {
            IDE_TEST( sFileNode->mFile.truncate(
                        SDD_CALC_PAGEOFFSET(sFileNode->mPageCnt) )
                    != IDE_SUCCESS );
        }
    }
    else 
    {
        IDE_TEST( sFileNode->mFile.create() != IDE_SUCCESS );

        IDE_TEST( open( aStatistics ) != IDE_SUCCESS );
    }

    IDE_TEST( writeEmptyPages( aStatistics, sFileNode ) != IDE_SUCCESS );
    
    IDE_TEST( writeFileHdr( aStatistics, 
                            sFileNode )
            != IDE_SUCCESS );

    if( aIsCreate == ID_TRUE )
    {
        IDE_ASSERT( smLayerCallback::addSBufferNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );
    }
    else 
    {
        IDE_ASSERT( smLayerCallback::updateSBufferNodeAndFlush( sFileNode )
                    == IDE_SUCCESS );
    } 

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    IDE_SET( ideSetErrorCode( smERR_ABORT_CannotCreateSecondaryBuffer ) );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α׾�Ŀ�� �о node ���� 
 **********************************************************************/
IDE_RC sdsFile::load( smiSBufferFileAttr     * aFileAttr,
                      UInt                     aAnchorOffset )
{
    sdsFileNode  * sFileNode;
    UInt           sIOBSize;

    IDE_DASSERT( aFileAttr != NULL );

    IDE_TEST( initFileNode( aFileAttr->mPageCnt,
                            aFileAttr->mName,  
                            &sFileNode )
              != IDE_SUCCESS );
    /* �ε� ��Ŀ���� ������ �ִ� ���� �о� ä�� */
    sFileNode->mState = aFileAttr->mState;
    sFileNode->mFileHdr.mSmVersion = 
        smLayerCallback::getSmVersionFromLogAnchor();
    sFileNode->mAnchorOffset = aAnchorOffset;

    sIOBSize = smuProperty::getBufferIOBufferSize();

    /* �ε��Ŀ�ǰ��� ������Ƽ ������ ��带 �����Ͽ� 
       identify�����߿� fileHdr�� �� �� �Ѵ� */
    setFileHdr( &(sFileNode->mFileHdr),
                smLayerCallback::getDiskRedoLSN(),
                &aFileAttr->mCreateLSN,
                &sIOBSize,/* FrameCntInExt */
                NULL );   /* Sequence Num  */

    IDE_TEST( sFileNode->mFile.initialize( 
                IDU_MEM_SM_SDS,
                smuProperty::getMaxOpenFDCount4File(),
                IDU_FIO_STAT_ON,
                IDV_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_FILEIO )
            != IDE_SUCCESS );

    IDE_TEST( sFileNode->mFile.setFileName(sFileNode->mName) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Second Buffer File�� �����Ѵ�.
 **********************************************************************/
IDE_RC sdsFile::open( idvSQL * /*aStatistics*/ )
{
    sdsFileNode  * sFileNode;

    getFileNode( &sFileNode );

    if( sFileNode->mIsOpened == ID_FALSE )
    {
        IDE_TEST( sFileNode->mFile.open( ((smuProperty::getIOType() == 0) ?
                                         ID_FALSE : ID_TRUE))   /* DIRECT_IO */
                  != IDE_SUCCESS );

        sFileNode->mIsOpened = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : close�Ѵ�.
 **********************************************************************/
IDE_RC sdsFile::close( idvSQL *  /*aStatistics*/ )
{
    sdsFileNode     * sFileNode;

    getFileNode( &sFileNode );

    if( sFileNode->mIsOpened == ID_TRUE )
    {
        sync();
        
        IDE_ASSERT( sFileNode->mFile.close() == IDE_SUCCESS );

        sFileNode->mIsOpened = ID_FALSE;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : pageoffset���� readsize��ŭ data �ǵ�
 **********************************************************************/
IDE_RC sdsFile::read( idvSQL      * aStatistics,
                      UInt          aFrameID,
                      ULong         aWhere,
                      ULong         aPageCount,
                      UChar       * aBuffer )

{
    sdsFileNode  * sFileNode;
    ULong          sWhere; 
    ULong          sReadByteSize = SD_PAGE_SIZE * aPageCount;

    getFileNode( &sFileNode );

    sWhere = aWhere+SM_DBFILE_METAHDR_PAGE_SIZE;

    IDE_TEST( sFileNode->mFile.read( aStatistics,
                                     sWhere,
                                     (void*)aBuffer,
                                     sReadByteSize,
                                     NULL )  // setEmergency
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Page From Data File Failure\n"
                 "               FrameID       = %"ID_UINT32_FMT"",
                 aFrameID );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  pageoffset���� writesize��ŭ data ���
 **********************************************************************/
IDE_RC sdsFile::write( idvSQL      * aStatistics,
                       UInt          aFrameID,
                       ULong         aWhere,
                       ULong         aPageCount,
                       UChar       * aBuffer )
{
    sdsFileNode  * sFileNode;
    ULong          sWhere; 
    ULong          sWriteByteSize = SD_PAGE_SIZE * aPageCount; 

    getFileNode( &sFileNode);

    sWhere = aWhere+SM_DBFILE_METAHDR_PAGE_SIZE;

    IDE_TEST( sFileNode->mFile.writeUntilSuccess( 
                                 aStatistics,
                                 sWhere,
                                 (void*)aBuffer,
                                 sWriteByteSize,
                                 smLayerCallback::setEmergency ) /* setEmergency */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Write Page To Data File Failure\n"
                 "              PageID       = %"ID_UINT32_FMT"",
                 aFrameID );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : sync�Ѵ�.
 **********************************************************************/
IDE_RC sdsFile::sync()
{
    sdsFileNode  * sFileNode;

    getFileNode( &sFileNode );

    IDE_TEST( sFileNode->mFile.syncUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : checkpoint ������ FileHdr�� redoLSN���� �����Ѵ�.  
 ***********************************************************************/
IDE_RC sdsFile::syncAllSB( idvSQL  * aStatistics )
{
    IDE_TEST( open( aStatistics ) != IDE_SUCCESS );

    IDE_TEST( setAndWriteFileHdr( aStatistics,
                smLayerCallback::getDiskRedoLSN(),  /* aRedoLSN    */
                NULL,                               /* mCreateLSN  */
                NULL,                               /* aFrameCount */
                NULL )                              /* aSeqNum     */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : file create �� ���� �ʱ�ȭ  
 ***********************************************************************/
IDE_RC sdsFile::writeEmptyPages( idvSQL         * aStatistics,
                                 sdsFileNode    * aFileNode )
{
    UChar     * sBufferPtr;
    UChar     * sAlignedBuffer;
    ULong       sUnitCnt;
    ULong       sUnitPageCnt;
    ULong       sUnitNum;
    ULong       sPageCnt;
    ULong       sWriteOffset;
    SInt        sAllocated = 0;
    ULong       i;
    UChar     * sCurPos;
    UInt        sInitialCheckSum;  // ���� page write�ÿ� ����Ǵ� checksum

    IDE_DASSERT( aFileNode != NULL );

    sUnitPageCnt = smuProperty::getDataFileWriteUnitSize();
    
    sPageCnt = (aFileNode->mPageCnt);

    if( sUnitPageCnt > sPageCnt )
    {
        /* ����Ϸ��� ������������ ũ�Ⱑ ���� �������� ������
         * ���� ������ ���������� ũ�Ⱑ �ȴ�. 
         */
        sUnitPageCnt = sPageCnt;
    }
    sUnitNum = sPageCnt / sUnitPageCnt;

    /* CheckSum ��� */
    for( i = 0, sInitialCheckSum = 0; i < SD_PAGE_SIZE - ID_SIZEOF(UInt); i++ )
    {
        sInitialCheckSum = smuUtility::foldUIntPair(sInitialCheckSum, 0);
    }

    /* 1.���� ������ �Ҵ�޴´�. */
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDS,
                                           sUnitPageCnt * SD_PAGE_SIZE,
                                           (void**)&sBufferPtr,
                                           (void**)&sAlignedBuffer ) 
              != IDE_SUCCESS );
    sAllocated = 1;

    /* 2.���� ������ �ʱ�ȭ�Ѵ�. */
    /* 2-1. ���� ù��° �������� �ʱ�ȭ�Ѵ�.
     */
    idlOS::memset( sAlignedBuffer, 0x00, sUnitPageCnt * SD_PAGE_SIZE );
    idlOS::memcpy( sAlignedBuffer, &sInitialCheckSum, ID_SIZEOF(UInt) );

    /* 2-2. �ι�° ���������ʹ� ù��° �������� �����Ѵ�. */
    for (i = 1, sCurPos = sAlignedBuffer + SD_PAGE_SIZE;
         i < sUnitPageCnt;
         i++, sCurPos += SD_PAGE_SIZE)
    {
        idlOS::memcpy( sCurPos, sAlignedBuffer, SD_PAGE_SIZE );
    }

    /* 2-3.sUnitPageCnt ������ ���Ͽ� ���۸� ����Ѵ� */
    sWriteOffset = SM_DBFILE_METAHDR_PAGE_OFFSET;
    
    for( sUnitCnt = 0; sUnitCnt < sUnitNum ; sUnitCnt++ )
    {
        /* BUG-18138 */
        IDE_TEST( aFileNode->mFile.write( aStatistics,
                                          sWriteOffset,
                                          sAlignedBuffer,
                                          sUnitPageCnt * SD_PAGE_SIZE )
                  != IDE_SUCCESS);

        sWriteOffset += sUnitPageCnt * SD_PAGE_SIZE;

        /* BUG-12754 : session ������ ������� �˻��� */
        IDE_TEST( iduCheckSessionEvent(aStatistics) != IDE_SUCCESS );
    }

    /* 2-4.sUnitPageCnt ����� �ʱ�ȭ �ϰ� ���� ũ�� + 1(file header ����) ��  ����Ѵ�. */
    IDE_TEST( aFileNode->mFile.write( 
                       aStatistics,
                       sWriteOffset,
                       sAlignedBuffer,
                       (sPageCnt - (sUnitPageCnt * sUnitCnt) + 1) * SD_PAGE_SIZE )
              != IDE_SUCCESS);

    sAllocated = 0;
    IDE_TEST( iduMemMgr::free( sBufferPtr ) != IDE_SUCCESS );
    sBufferPtr     = NULL;
    sAlignedBuffer = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if( sAllocated == 1 )
        {
            IDE_ASSERT( iduMemMgr::free(sBufferPtr) == IDE_SUCCESS );
            sBufferPtr     = NULL;
            sAlignedBuffer = NULL;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : File Header�� ����Ѵ�.
 ***********************************************************************/
void sdsFile::getFileAttr( sdsFileNode          * aFileNode,
                           smiSBufferFileAttr   * aFileAttr )
{
    sdsFileHdr  * sFileHdr;

    IDE_DASSERT( aFileNode != NULL );
    IDE_DASSERT( aFileAttr != NULL );

    sFileHdr = &(aFileNode->mFileHdr);

    aFileAttr->mAttrType = SMI_SBUFFER_ATTR;
    idlOS::strcpy(aFileAttr->mName, aFileNode->mName);
    aFileAttr->mNameLength = idlOS::strlen(aFileNode->mName);
    aFileAttr->mPageCnt = aFileNode->mPageCnt;
    aFileAttr->mState = aFileNode->mState;

    SM_GET_LSN( aFileAttr->mCreateLSN, sFileHdr->mCreateLSN );

    return;
}

/***********************************************************************
 * Description :  �־��� SBuffer File�� FileHdr�� �о� �Ϻ� ������ ��ȯ�Ѵ�.
 * aLstMetaTableSeqNum [OUT] - ����� ������ Meta Seq Num
 * aRecoveryLSN        [OUT] - 
 * ********************************************************************/
IDE_RC sdsFile::readHdrInfo( idvSQL * aStatistics,
                             UInt   * aLstMetaTableSeqNum,
                             smLSN  * aRecoveryLSN )
{
    SChar       * sHeadPageBuffPtr = NULL;
    SChar       * sHeadPageBuff    = NULL;
    sdsFileNode * sFileNode;
    sdsFileHdr    sFileHdr;

    getFileNode( &sFileNode );
    /* ��� �������� �о�� ���� ���� */
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDS,
                                           SD_PAGE_SIZE,
                                           (void**)&sHeadPageBuffPtr,
                                           (void**)&sHeadPageBuff )
              != IDE_SUCCESS );

    /* File�� ���� IO�� �׻� DirectIO�� ����ؼ� Buffer, Size, Offset��
     * DirectIO Page ũ��� Align��Ų��. */
    IDE_TEST( sFileNode->mFile.read( aStatistics,
                                     SM_DBFILE_METAHDR_PAGE_OFFSET,
                                     sHeadPageBuff,
                                     SD_PAGE_SIZE,
                                     NULL /*RealReadSize*/ )
              != IDE_SUCCESS );

    idlOS::memcpy( &sFileHdr, sHeadPageBuff, ID_SIZEOF(sdsFileHdr) );

    *aLstMetaTableSeqNum = sFileHdr.mLstMetaTableSeqNum;
    SM_GET_LSN( *aRecoveryLSN, sFileHdr.mRedoLSN );

    IDE_TEST( iduMemMgr::free( sHeadPageBuffPtr ) != IDE_SUCCESS );
    sHeadPageBuffPtr = NULL;
    sHeadPageBuff = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Database File Header Failure\n"
                 "              File Name   = %s\n"
                 "              File Opened = %s",
                 sFileNode->mName,
                 (sFileNode->mIsOpened ? "Open" : "Close") );

    if( sHeadPageBuffPtr != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( sHeadPageBuffPtr ) == IDE_SUCCESS );
        sHeadPageBuffPtr = NULL;
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  �־��� SBuffer File�� FileHdr�� �д´�.
 * aFileHdr [OUT] - 
 * ********************************************************************/
IDE_RC sdsFile::readFileHdr( idvSQL     * aStatistics,
                             sdsFileHdr * aFileHdr )
{
    SChar       * sHeadPageBuffPtr = NULL;
    SChar       * sHeadPageBuff    = NULL;
    sdsFileNode * sFileNode;

    IDE_DASSERT( aFileHdr != NULL );

    getFileNode( &sFileNode );
    /* ��� �������� �о�� ���� ���� */
    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDS,
                                           SD_PAGE_SIZE,
                                           (void**)&sHeadPageBuffPtr,
                                           (void**)&sHeadPageBuff )
              != IDE_SUCCESS );

    /* File�� ���� IO�� �׻� DirectIO�� ����ؼ� Buffer, Size, Offset��
     * DirectIO Page ũ��� Align��Ų��. */
    IDE_TEST( sFileNode->mFile.read( aStatistics,
                                     SM_DBFILE_METAHDR_PAGE_OFFSET,
                                     sHeadPageBuff,
                                     SD_PAGE_SIZE,
                                     NULL /*RealReadSize*/ )
              != IDE_SUCCESS );

    idlOS::memcpy( aFileHdr, sHeadPageBuff, ID_SIZEOF(sdsFileHdr) );

    IDE_TEST( iduMemMgr::free( sHeadPageBuffPtr ) != IDE_SUCCESS );
    sHeadPageBuffPtr = NULL;
    sHeadPageBuff = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0,
                 "Read Database File Header Failure\n"
                 "              File Name   = %s\n"
                 "              File Opened = %s",
                 sFileNode->mName,
                 (sFileNode->mIsOpened ? "Open" : "Close") );

    if( sHeadPageBuffPtr != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( sHeadPageBuffPtr ) == IDE_SUCCESS );
        sHeadPageBuffPtr = NULL;
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  File Header�� ����Ѵ�.
 * aFileNode   - [IN] File node
 **********************************************************************/
IDE_RC sdsFile::writeFileHdr( idvSQL        * aStatistics,
                              sdsFileNode   * aFileNode )
{
    SChar *sHeadPageBuffPtr = NULL;
    SChar *sHeadPageBuff = NULL;

    IDE_DASSERT( aFileNode != NULL );

    IDE_TEST( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDS,
                                           SD_PAGE_SIZE,
                                           (void**)&sHeadPageBuffPtr,
                                           (void**)&sHeadPageBuff )
              != IDE_SUCCESS );

    idlOS::memset( sHeadPageBuff, 0x00, SD_PAGE_SIZE );
    idlOS::memcpy( sHeadPageBuff,
                   &( aFileNode->mFileHdr ),
                   ID_SIZEOF( sdsFileHdr ) );

    IDE_TEST( aFileNode->mFile.writeUntilSuccess( aStatistics,
                                                  SM_SBFILE_METAHDR_PAGE_OFFSET,
                                                  sHeadPageBuff,
                                                  SD_PAGE_SIZE,
                                                  smLayerCallback::setEmergency ) 
              != IDE_SUCCESS); 

    IDE_TEST( aFileNode->mFile.syncUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );

    IDE_TEST( iduMemMgr::free( sHeadPageBuffPtr ) != IDE_SUCCESS );
    sHeadPageBuffPtr = NULL;
    sHeadPageBuff = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sHeadPageBuffPtr != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( sHeadPageBuffPtr ) == IDE_SUCCESS );
        sHeadPageBuffPtr = NULL;
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : HEADER�� üũ����Ʈ ������ ����Ѵ�.
 * aRedoSN     [IN] �̵����� �ʿ��� Redo SN
 * aCreateLSN  [IN]  
 * aFrameCnt   [IN]
 * aSeqNum     [IN]
 **********************************************************************/
void sdsFile::setFileHdr( sdsFileHdr    * aFileHdr,
                          smLSN         * aRedoLSN,
                          smLSN         * aCreateLSN,
                          UInt          * aFrameCnt,
                          UInt          * aSeqNum )
{
    IDE_ASSERT( aFileHdr != NULL );

    if( aRedoLSN != NULL )
    {
        SM_GET_LSN( aFileHdr->mRedoLSN, *aRedoLSN );
    }
    if( aCreateLSN != NULL )
    {
        SM_GET_LSN( aFileHdr->mCreateLSN, *aCreateLSN );
    }
    if( aFrameCnt != NULL )
    {
        aFileHdr->mFrameCntInExt = *aFrameCnt;
    } 
    if( aSeqNum != NULL )
    {
        aFileHdr->mLstMetaTableSeqNum = *aSeqNum;
    }
    return;
}

/***********************************************************************
 * Description : Header ��Ϲ� ���Ͽ� ����
 **********************************************************************/
IDE_RC sdsFile::setAndWriteFileHdr( idvSQL * aStatistics,
                                    smLSN  * aRedoLSN,
                                    smLSN  * aCreateLSN,
                                    UInt   * aFrameCount,
                                    UInt   * aSeqNum )
{
    sdsFileNode * sFileNode;
    sdsFileHdr  * sFileHdr;

    getFileNode( &sFileNode );

    sFileHdr = &( sFileNode->mFileHdr );

    setFileHdr( sFileHdr, 
                aRedoLSN, 
                aCreateLSN, 
                aFrameCount, 
                aSeqNum );
    
    IDE_TEST( writeFileHdr( aStatistics,
                            sFileNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �̵�� ��Ŀ������ �����ѵ� RedoLSN �� ����ָ�
                 MetaTableSeqNum �� �ʱ�ȭ�� ���� �ش�  
                 MetaTableSeqNum �� �ʱ�ȭ �Ǿ������� secondary Buffer�� 
                 ������� ���������� �����ϰ� BCB�� rebuild ���� �ʴ´�.
 *  aResetLogsLSN  [IN] - �ҿ��������� �����Ǵ� ResetLogsLSN
 *                        �������� �ÿ��� NULL
 * ********************************************************************/
IDE_RC sdsFile::repairFailureSBufferHdr( idvSQL * aStatistics, 
                                         smLSN  * aResetLogsLSN )
{
    sdsFileNode  * sFileNode = NULL;
    sdsFileHdr   * sFileHdr  = NULL;

    getFileNode( &sFileNode );

    IDE_TEST_CONT( sFileNode == NULL, SKIP_SUCCESS );

    sFileHdr = &( sFileNode->mFileHdr );

    if( aResetLogsLSN != NULL )
    {
        SM_GET_LSN( sFileHdr->mRedoLSN, *aResetLogsLSN );
    }

    sFileHdr->mLstMetaTableSeqNum = SDS_FILEHDR_SEQ_MAX;

    IDE_TEST( open( aStatistics ) != IDE_SUCCESS );

    IDE_TEST( writeFileHdr( aStatistics,
                            sFileNode )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ��밡���� �������� �˻�  
 *  aFileNode           - [IN]  Loganchor���� ���� FileNode
 *  aCompatibleFrameCnt - [OUT] 
 **********************************************************************/
IDE_RC sdsFile::checkValidationHdr( idvSQL       * aStatistics,
                                    sdsFileNode  * aFileNode,
                                    idBool       * aCompatibleFrameCnt )
{
    UInt              sDBVer;
    UInt              sFileVer;
    sdsFileHdr      * sFileHdrbyNode;
    sdsFileHdr        sFileHdrbyFile;

    IDE_DASSERT( aFileNode       != NULL );

    sFileHdrbyNode = &(aFileNode->mFileHdr);

    IDE_TEST( open( aStatistics ) != IDE_SUCCESS );

    IDE_TEST( readFileHdr( NULL,  &sFileHdrbyFile ) != IDE_SUCCESS );

    IDE_TEST( close( aStatistics ) != IDE_SUCCESS );

    IDE_TEST_RAISE( checkValuesOfHdr( &sFileHdrbyFile ) 
                    != IDE_SUCCESS,
                    ERR_INVALID_HDR );

    sDBVer   = smVersionID & SM_CHECK_VERSION_MASK;
    sFileVer = sFileHdrbyFile.mSmVersion & SM_CHECK_VERSION_MASK;

    /* [1] SM VERSION Ȯ��
     * ����Ÿ���ϰ� ���� ���̳ʸ��� ȣȯ���� �˻��Ѵ�. 
     */
    IDE_TEST_RAISE( sDBVer != sFileVer, ERR_INVALID_HDR );

    /* [2] CREATE SN Ȯ�� 
     * ����Ÿ������ Create SN�� Loganchor�� Create SN�� �����ؾ��Ѵ�.
     * �ٸ��� ���ϸ� ������ ������ �ٸ� ����Ÿ�����̴�. 
     */
    IDE_TEST_RAISE( smLayerCallback::isLSNEQ( &(sFileHdrbyFile.mCreateLSN),
                                              &(sFileHdrbyNode->mCreateLSN) ) 
                    != ID_TRUE,
                    ERR_INVALID_HDR );

    /* [3] Redo LSN Ȯ��
     * Loganchor�� RedoLSN��   
     * ������ redoLSN����(üũ���νÿ� ������ DRDB Redo LSN)
     * �� �۰ų� ���ƾ� �Ѵ�. 
     */  
    IDE_TEST_RAISE( smLayerCallback::isLSNLTE( &(sFileHdrbyNode->mRedoLSN),
                                               &(sFileHdrbyFile.mRedoLSN) ) 
                    != ID_TRUE,
                    ERR_INVALID_HDR );

    /* [4] IOBȮ�� 
     * IOB�� Property�̹Ƿ� �ٲ�� �����Ƿ� 
     * �ٸ��� ������ �ʰ� ������ ����� �Ѵ�.
     */
    if( sFileHdrbyNode->mFrameCntInExt != sFileHdrbyFile.mFrameCntInExt )
    {
        *aCompatibleFrameCnt = ID_FALSE;
    } 


    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_INVALID_HDR );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidSecondaryBufferHdr) );

        ideLog::log( IDE_SERVER_0,
                     "Find Invalid Secondary Buffer File Header \n"
                     "Secondary Buffer File Header Hdr \n"
                     "Binary DB Version : < %"ID_xINT32_FMT".%"ID_xINT32_FMT".%"ID_xINT32_FMT" >\n"
                     "Create LSN        : <%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n"
                     "Redo LSN          : <%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n"
                     "MetaData Num      : < %"ID_UINT32_FMT">\n"
                     "Last MetaTable Sequence Number : < %"ID_UINT32_FMT" > \n"
                     "Loganchor \n"
                     "Create LSN        : <%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n"
                     "Redo LSN          : <%"ID_UINT32_FMT",%"ID_UINT32_FMT">\n",
                     ( ( sFileHdrbyFile.mSmVersion & SM_MAJOR_VERSION_MASK ) >> 24 ),
                     ( ( sFileHdrbyFile.mSmVersion & SM_MINOR_VERSION_MASK ) >> 16 ),
                     ( sFileHdrbyFile.mSmVersion  & SM_PATCH_VERSION_MASK),
                     sFileHdrbyFile.mCreateLSN.mFileNo,
                     sFileHdrbyFile.mCreateLSN.mOffset,
                     sFileHdrbyFile.mRedoLSN.mFileNo,
                     sFileHdrbyFile.mRedoLSN.mOffset,
                     sFileHdrbyFile.mFrameCntInExt,
                     sFileHdrbyFile.mLstMetaTableSeqNum,
                     sFileHdrbyNode->mCreateLSN.mFileNo,
                     sFileHdrbyNode->mCreateLSN.mOffset,
                     sFileHdrbyNode->mRedoLSN.mFileNo,
                     sFileHdrbyNode->mRedoLSN.mOffset );

    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  HEADERD�� ��ȿ���� �˻��Ѵ�.
 * aFileHdr       [IN] sdsFileHdr Ÿ���� ������
 **********************************************************************/
IDE_RC sdsFile::checkValuesOfHdr( sdsFileHdr*  aFileHdr )
{
    smLSN sCmpLSN;

    IDE_DASSERT( aFileHdr != NULL );

    SM_LSN_INIT( sCmpLSN );

    IDE_TEST( smLayerCallback::isLSNEQ( &aFileHdr->mRedoLSN, &sCmpLSN ) 
              == ID_TRUE );

    IDE_TEST( smLayerCallback::isLSNEQ( &aFileHdr->mCreateLSN, &sCmpLSN )
               == ID_TRUE );

    SM_LSN_MAX( sCmpLSN );
    IDE_TEST( smLayerCallback::isLSNEQ( &aFileHdr->mRedoLSN, &sCmpLSN ) 
              == ID_TRUE );

    IDE_TEST( smLayerCallback::isLSNEQ( &aFileHdr->mCreateLSN, &sCmpLSN ) 
              == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ����� �Է½� �����θ� �����
                 BUGBUG
                 sctTableSpaceMgr::makeValidABSPath ����
                 ������ ����� ��ġ�� �� �ߺ��� �κ��� ���̴� �۾� �ʿ�!!!!

  aValidName   - [IN/OUT] ����θ� �޾Ƽ� �����η� �����Ͽ� ��ȯ
 **********************************************************************/
IDE_RC sdsFile::makeValidABSPath( SChar  * aValidName )
{
#if !defined(VC_WINCE) && !defined(SYMBIAN)
    UInt    i;
    SChar*  sPtr;

    DIR*    sDir;
    UInt    sDirLength;
    UInt    sNameLength;
    SChar   sFilePath[SM_MAX_FILE_NAME];

    SChar*  sPath = NULL;

    IDE_ASSERT( aValidName  != NULL );

    // fix BUG-15502
    IDE_TEST_RAISE( idlOS::strlen(aValidName) == 0,
                    ERR_FILENAME_IS_NULL_STRING );

    sPath = sFilePath;
    sNameLength = idlOS::strlen( aValidName );

    /* ------------------------------------------------
     * datafile �̸��� ���� �ý��� ����� �˻�
     * ----------------------------------------------*/
#if defined(VC_WIN32)
    SInt  sIterator;
    for ( sIterator = 0; aValidName[sIterator] != '\0'; sIterator++ ) {
        if ( aValidName[sIterator] == '/' ) {
             aValidName[sIterator] = IDL_FILE_SEPARATOR;
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
    if (sPtr != &aValidName[0])
#else
    /* BUG-38278 invalid datafile path at windows server
     * �������� ȯ�濡�� '/' �� '\' �� ���۵Ǵ�
     * ��� �Է��� ������ ó���Ѵ�. */
    IDE_TEST_RAISE( sPtr == &aValidName[0], ERR_INVALID_FILEPATH_ABS );

    if ((aValidName[1] == ':' && sPtr != &aValidName[2]) ||
        (aValidName[1] != ':' && sPtr != &aValidName[0]))
#endif
    {
        /* ------------------------------------------------
         * �����(relative-path)�� ���
         * Disk TBS�̸� default disk db dir��
         * Memory TBS�̸� home dir ($ALTIBASE_HOME)��
         * �ٿ��� ������(absolute-path)�� �����.
         * ----------------------------------------------*/
        sDirLength = idlOS::strlen(aValidName) +
            idlOS::strlen(idp::getHomeDir());

        IDE_TEST_RAISE( sDirLength + 1 > SM_MAX_FILE_NAME,
                        ERR_TOO_LONG_FILEPATH );

        idlOS::snprintf(sPath, SM_MAX_FILE_NAME,
                "%s%c%s",
                idp::getHomeDir(),
                IDL_FILE_SEPARATOR,
                aValidName);

#if defined(VC_WIN32)
    for ( sIterator = 0; sPath[sIterator] != '\0'; sIterator++ ) {
        if ( sPath[sIterator] == '/' ) {
             sPath[sIterator] = IDL_FILE_SEPARATOR;
        }
    }
#endif
        idlOS::strcpy(aValidName, sPath);
        sNameLength = idlOS::strlen(aValidName);

        sPtr = idlOS::strchr(aValidName, IDL_FILE_SEPARATOR);
#ifndef VC_WIN32
        IDE_TEST_RAISE( sPtr != &aValidName[0], ERR_INVALID_FILEPATH_ABS );
#else
        IDE_TEST_RAISE( (aValidName[1] == ':' && sPtr != &aValidName[2]) ||
        (aValidName[1] != ':' && sPtr != &aValidName[0]), ERR_INVALID_FILEPATH_ABS );
#endif
    }

    /* ------------------------------------------------
     * ������, ���� + '/'�� ����ϰ� �׿� ���ڴ� ������� �ʴ´�.
     * (��������)
     * ----------------------------------------------*/
    for (i = 0; i < sNameLength; i++)
    {
        if (smuUtility::isAlNum(aValidName[i]) != ID_TRUE)
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
                            ERR_INVALID_FILEPATH_KEYWORD );

            if (aValidName[i] == '.')
            {
                if ((i + 1) != sNameLength)
                {
                    IDE_TEST_RAISE( aValidName[i+1] == '.',
                                    ERR_INVALID_FILEPATH_KEYWORD );
                    IDE_TEST_RAISE( aValidName[i+1] == IDL_FILE_SEPARATOR,
                                    ERR_INVALID_FILEPATH_KEYWORD );
                }
            }
        }
    }

    // [BUG-29812] IDL_FILE_SEPARATOR�� �Ѱ��� ���ٸ� �����ΰ� �ƴϴ�.
    IDE_TEST_RAISE( (sPtr = idlOS::strrchr(aValidName, IDL_FILE_SEPARATOR) )
                     == NULL,
                     ERR_INVALID_FILEPATH_ABS );

    // [BUG-29812] dir�� �����ϴ� Ȯ���Ѵ�.
    idlOS::strncpy(sPath, aValidName, SM_MAX_FILE_NAME);

    sDirLength = sPtr - aValidName;

    sDirLength = (sDirLength == 0) ? 1 : sDirLength;
    sPath[sDirLength] = '\0';

    // fix BUG-19977
    IDE_TEST_RAISE( idf::access(sPath, F_OK) != 0,
                    ERROR_NOT_EXIST_PATH );
    IDE_TEST_RAISE( idf::access(sPath, R_OK) != 0,
                    ERROR_NO_READ_PERM_PATH );
    IDE_TEST_RAISE( idf::access(sPath, W_OK) != 0,
                    ERROR_NO_WRITE_PERM_PATH );
    IDE_TEST_RAISE( idf::access(sPath, X_OK) != 0,
                    ERROR_NO_EXECUTE_PERM_PATH );

    sDir = idf::opendir(sPath);
    IDE_TEST_RAISE( sDir == NULL, 
                    ERR_OPEN_DIR ); /* BUGBUG - ERROR MSG */
    (void)idf::closedir(sDir);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FILENAME_IS_NULL_STRING );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_FileNameIsNullString));
    }
    IDE_EXCEPTION( ERROR_NOT_EXIST_PATH );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExistPath, sPath));
    }
    IDE_EXCEPTION( ERROR_NO_READ_PERM_PATH );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoReadPermFile, sPath));
    }
    IDE_EXCEPTION( ERROR_NO_WRITE_PERM_PATH );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoWritePermFile, sPath));
    }
    IDE_EXCEPTION( ERROR_NO_EXECUTE_PERM_PATH );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NoExecutePermFile, sPath));
    }
    IDE_EXCEPTION( ERR_INVALID_FILEPATH_ABS );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathABS));
    }
    IDE_EXCEPTION( ERR_INVALID_FILEPATH_KEYWORD );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidFilePathKeyWord));
    }
    IDE_EXCEPTION( ERR_OPEN_DIR );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_NotDir));
    }
    IDE_EXCEPTION( ERR_TOO_LONG_FILEPATH );
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

/******************************************************************************
 * Description : Node�� ������ �����Ѵ�.
                 Node�� ������ valid�� ������ �����ϴ��� Ȯ��  
 ******************************************************************************/
IDE_RC sdsFile::makeSureValidationNode( idvSQL         * aStatistics,
                                        sdsFileNode    * aFileNode,
                                        idBool         * aIsValidFileExist,
                                        idBool         * aIsCreate )
{
    SChar           sDir[SMI_MAX_SBUFFER_FILE_NAME_LEN];
    ULong           sFilePageCnt;
    idBool          sCompatibleFrameCnt = ID_TRUE;

    IDE_ASSERT( ( idlOS::strcmp(mSBufferDirName, "") != 0 ) &&
                ( mPageCntInGrp != 0 ) );
 
    /* �����̸� ���� */
    idlOS::memset( sDir, 0x00, ID_SIZEOF(sDir) );
    idlOS::snprintf( sDir, SMI_MAX_SBUFFER_FILE_NAME_LEN,
                     "%s%c%s.sbf", 
                     mSBufferDirName, 
                     IDL_FILE_SEPARATOR,
                     SDS_SECONDARY_BUFFER_FILE_NAME_PREFIX );

    /* ���� ������� Secondary Buffer Size + MetaTable ũ�� */
    sFilePageCnt = mPageCntInGrp + mPageCntInMetaTable;

    if( aFileNode == NULL ) 
    {
        IDE_TEST( initFileNode( sFilePageCnt, sDir, NULL ) != IDE_SUCCESS );
        *aIsCreate = ID_TRUE; 
    }
    else
    {
        if( (idlOS::strncmp(aFileNode->mName,sDir,idlOS::strlen(sDir)) == 0 )  &&
            (idlOS::strlen(sDir) == idlOS::strlen(aFileNode->mName) ) &&
            (aFileNode->mPageCnt == sFilePageCnt) ) 
        {
            if( idf::access( aFileNode->mName, F_OK) == 0 )
            {        
               IDE_TEST( checkValidationHdr( aStatistics, 
                                             aFileNode,
                                             &sCompatibleFrameCnt ) 
                         != IDE_SUCCESS );

               if( sCompatibleFrameCnt == ID_TRUE )
               {
                   *aIsValidFileExist = ID_TRUE;
               }
            }
            else 
            {   /* �Ӽ��� �״�� �̳� ������ ���ų� 
                   mFrameCntInExt �� ���ѻ�Ȳ�̶�� 
                   ������ ���� �����ؾ� �Ѵ�. */ 
                aFileNode->mState = SMI_FILE_CREATING;
            }
        } 
        else 
        {   
            /* ��ΰ� ������ �Ǿ��� ��� : */ 
            if( (idlOS::strncmp(aFileNode->mName,sDir,idlOS::strlen(sDir)) != 0 ) ||
                (idlOS::strlen(sDir) != idlOS::strlen(aFileNode->mName) ) )
            {
                IDE_TEST( makeValidABSPath( sDir ) != IDE_SUCCESS );

                idlOS::strcpy( aFileNode->mName, sDir );
            }
            /* ���� ũ�Ⱑ �ٲ� ��� :  */
            if( aFileNode->mPageCnt != sFilePageCnt )
            {
                aFileNode->mPageCnt = sFilePageCnt;
            }
            aFileNode->mState = SMI_FILE_CREATING;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
