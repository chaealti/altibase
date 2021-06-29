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
 * $Id$
 *
 * Description :
 *
 * - incremental chunk change tracking manager
 *
 **********************************************************************/

#include <sddDef.h>
#include <sct.h>
#include <smu.h>
#include <smi.h>
#include <smr.h>
#include <smrReq.h>

smriCTFileHdrBlock  smriChangeTrackingMgr::mCTFileHdr;
iduFile             smriChangeTrackingMgr::mFile;
smriCTBody        * smriChangeTrackingMgr::mCTBodyPtr[SMRI_CT_MAX_CT_BODY_CNT];
smriCTBody        * smriChangeTrackingMgr::mCTBodyFlushBuffer = NULL;
smriCTHdrState      smriChangeTrackingMgr::mCTHdrState;
iduMutex            smriChangeTrackingMgr::mMutex;
UInt                smriChangeTrackingMgr::mBmpBlockBitmapSize;
UInt                smriChangeTrackingMgr::mCTBodyExtCnt;
UInt                smriChangeTrackingMgr::mCTExtMapSize;
UInt                smriChangeTrackingMgr::mCTBodySize;
UInt                smriChangeTrackingMgr::mCTBodyBlockCnt;
UInt                smriChangeTrackingMgr::mCTBodyBmpExtCnt;
UInt                smriChangeTrackingMgr::mBmpBlockBitCnt;
UInt                smriChangeTrackingMgr::mBmpExtBitCnt;

iduFile           * smriChangeTrackingMgr::mSrcFile;
iduFile           * smriChangeTrackingMgr::mDestFile;
SChar             * smriChangeTrackingMgr::mIBChunkBuffer;
ULong               smriChangeTrackingMgr::mCopySize;
smriCTTBSType       smriChangeTrackingMgr::mTBSType;
UInt                smriChangeTrackingMgr::mIBChunkCnt;
UInt                smriChangeTrackingMgr::mIBChunkID;
UInt                smriChangeTrackingMgr::mFileNo;
scPageID            smriChangeTrackingMgr::mSplitFilePageCount;
UInt                smriChangeTrackingMgr::mCurrChangeTrackingThreadCnt; // change tracking�� �������� thread�� ��

IDE_RC smriChangeTrackingMgr::initializeStatic()
{
    UInt    i;
    idBool  sFileState   = ID_FALSE;
    idBool  sMutexState  = ID_FALSE;
    idBool  sBufferState = ID_FALSE;

    /*change tracking���� ���*/
    idlOS::memset( &mCTFileHdr, 0x00, SMRI_CT_BLOCK_SIZE );

    mCTHdrState = SMRI_CT_MGR_STATE_NORMAL;

    /*CT body�� ���� pointer*/
    for( i = 0; i < SMRI_CT_MAX_CT_BODY_CNT; i++ )
    {
        mCTBodyPtr[i] = NULL;
    }
    
    /* BUG-40716
     * CTBody�� flush�Ҷ� ����� buffer�� �̸� �Ҵ��Ͽ�
     * checkpoint�ÿ� CTBody�� �޸� �Ҵ� ���з� ���� ������ �״� ��� ����
     */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                 1,
                                 ID_SIZEOF( smriCTBody ),
                                 (void**)&mCTBodyFlushBuffer )
              != IDE_SUCCESS );
    sBufferState = ID_TRUE;

    IDE_TEST( mFile.initialize( IDU_MEM_SM_SMR,
                                1,
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sFileState = ID_TRUE;

    IDE_TEST( mMutex.initialize( (SChar*)"CHANGE_TRACKING_MGR_MUTEX",
                                 IDU_MUTEX_KIND_POSIX,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sMutexState = ID_TRUE;
    
    mSrcFile            = NULL;
    mDestFile           = NULL;
    mIBChunkBuffer      = NULL;
    mCopySize           = 0;
    mTBSType            = SMRI_CT_NONE;
    mIBChunkCnt         = 0;
    mIBChunkID          = ID_UINT_MAX;
    mBmpBlockBitmapSize = smuProperty::getBmpBlockBitmapSize();
    mCTBodyExtCnt       = smuProperty::getCTBodyExtCnt();
    mCTBodyBmpExtCnt    = mCTBodyExtCnt - 2; /* meta ext�� empty ext�� ���� */
    mCTExtMapSize       = mCTBodyBmpExtCnt;
    mCTBodySize         = (mCTBodyExtCnt 
                          * SMRI_CT_EXT_BLOCK_CNT 
                          * SMRI_CT_BLOCK_SIZE );
    mCTBodyBlockCnt     = (mCTBodySize / SMRI_CT_BLOCK_SIZE);
    
    /*BmpBlock�� ������ bitmap�� bit ��*/
    mBmpBlockBitCnt     = (mBmpBlockBitmapSize * SMRI_CT_BIT_CNT);
    /*BmpExt�� ������ bitmap�� bit ��*/
    mBmpExtBitCnt       = (mBmpBlockBitCnt 
                          * SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK);

    //PROJ-2133 incremental backup
    mCurrChangeTrackingThreadCnt = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
   
    if ( sBufferState == ID_TRUE )
    {
        IDE_ASSERT( iduMemMgr::free( mCTBodyFlushBuffer ) == IDE_SUCCESS );
        mCTBodyFlushBuffer = NULL;
    }

    if( sFileState == ID_TRUE )
    {
        IDE_ASSERT( mFile.destroy() == IDE_SUCCESS );
    }

    if( sMutexState == ID_TRUE )
    {
        IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC smriChangeTrackingMgr::destroyStatic()
{
    idBool  sFileState  = ID_TRUE;
    idBool  sMutexState = ID_TRUE;

    /* BUG-40716 */
    if ( mCTBodyFlushBuffer != NULL )
    {
        IDE_TEST( iduMemMgr::free( mCTBodyFlushBuffer ) != IDE_SUCCESS );
        mCTBodyFlushBuffer = NULL;
    }
     
    IDE_TEST( mFile.destroy() != IDE_SUCCESS );
    sFileState = ID_FALSE;

    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );
    sMutexState = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
   
    if ( mCTBodyFlushBuffer != NULL )
    {
        IDE_ASSERT( iduMemMgr::free( mCTBodyFlushBuffer ) == IDE_SUCCESS );
        mCTBodyFlushBuffer = NULL;
    }

    if( sFileState == ID_TRUE )
    {
        IDE_ASSERT( mFile.destroy() == IDE_SUCCESS );
    }
    
    if( sMutexState == ID_TRUE )
    {
        IDE_ASSERT( mMutex.destroy() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * CT(change tracking)������ �����Ѵ�.
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createCTFile()
{
    smriCTBody        * sCTBody;
    SChar               sFileName[ SM_MAX_FILE_NAME ] = "\0";
    smriCTMgrState      sCTMgrState;
    idBool              sCTBodyState = ID_FALSE;
    smLSN               sFlushLSN;
    UInt                sState = 0;
   
    IDE_ERROR( ID_SIZEOF( smriCTFileHdrBlock )      == SMRI_CT_BLOCK_SIZE );
    IDE_ERROR( ID_SIZEOF( smriCTExtMapBlock )       == SMRI_CT_BLOCK_SIZE );
    IDE_ERROR( ID_SIZEOF( smriCTBmpExtHdrBlock )    == SMRI_CT_BLOCK_SIZE );
    IDE_ERROR( ID_SIZEOF( smriCTBmpBlock )          == SMRI_CT_BLOCK_SIZE );
    IDE_ERROR( ID_SIZEOF( smriCTDataFileDescBlock ) == SMRI_CT_BLOCK_SIZE );

    IDE_TEST( lockCTMgr() != IDE_SUCCESS );
    sState = 1;

    /* change tarcking ����� Ȱ��ȭ �Ǿ��ִ��� Ȯ�� */
    IDE_TEST_RAISE( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE, 
                    error_already_enabled_change_tracking );

    /* ���� �̸� ���� */
    idlOS::snprintf( sFileName,                     
                     SM_MAX_FILE_NAME,              
                     "%s%c%s",
                     smuProperty::getDBDir(0),      
                     IDL_FILE_SEPARATOR,            
                     SMRI_CT_FILE_NAME );

    /* 
     * change tracking  ���� ������ ���� ���������� ��Ȳ
     * �������� ������ �����ϸ� �����ϰ� ���� �����
     * */
    if( smrRecoveryMgr::isCreatingCTFile() == ID_TRUE )
    {
        if( idf::access( sFileName, F_OK ) == 0 )
        {
            IDE_TEST( idf::unlink( sFileName ) != IDE_SUCCESS );
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

    /* logAnchor�� CTFile�̸��� ������ �������̶�� ������ ���� */
    SM_LSN_INIT( sFlushLSN );
    sCTMgrState = SMRI_CT_MGR_FILE_CREATING;
    IDE_TEST( smrRecoveryMgr::updateCTFileAttrToLogAnchor( sFileName,
                                                           &sCTMgrState,
                                                           NULL ) //LastFlushLSN
              != IDE_SUCCESS );
    

    /* CTMgrHdr ���� �ʱ�ȭ */
    mCTHdrState  = SMRI_CT_MGR_STATE_NORMAL;
    IDE_TEST( createFileHdrBlock( &mCTFileHdr ) !=IDE_SUCCESS );

    IDE_TEST( mFile.setFileName( sFileName ) != IDE_SUCCESS );

    IDE_TEST( mFile.create() != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 3;

    /*CTFileHdr checksum ����� ���Ϸ� write */
    setBlockCheckSum( &mCTFileHdr.mBlockHdr );

    IDE_TEST( mFile.write( NULL, //aStatistics
                           SMRI_CT_HDR_OFFSET,
                           &mCTFileHdr,
                           SMRI_CT_BLOCK_SIZE )
              != IDE_SUCCESS );

    sState = 2;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    /* CTFile Body���� */
    IDE_TEST( extend( ID_FALSE, /* Ȯ���ϰ� ���Ͽ� ����� ���� ������ �Ǵ�*/
                      &sCTBody ) != IDE_SUCCESS );
    sCTBodyState = ID_TRUE;

    /* ������ �����ϴ� ��� ������������ CTFile�� ���*/
    IDE_TEST( addAllExistingDataFile2CTFile( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );

    IDE_TEST( flush() != IDE_SUCCESS );

    /* 
     * change tracking������ ���� �Ϸ�ǰ� change tracking manager�� Ȱ��ȭ ����
     * ������� ����
     */
    sCTMgrState = SMRI_CT_MGR_ENABLED;
    IDE_TEST( smrRecoveryMgr::updateCTFileAttrToLogAnchor( NULL,  //FileName
                                                           &sCTMgrState,
                                                           NULL ) //LastFlushLSN
              != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( unlockCTMgr() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_enabled_change_tracking );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }

    IDE_EXCEPTION_END;

    if( sCTBodyState == ID_TRUE )
    {
        IDE_ASSERT( unloadCTBody( sCTBody ) == IDE_SUCCESS );
    }

    switch( sState )
    {
        case 3:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( idf::unlink( sFileName ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( unlockCTMgr() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * CT(chang tracking)���� body�� �ʱ�ȭ �Ѵ�.
 * 
 * aCTBody    - [IN] changeTracking body
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createCTBody( smriCTBody * aCTBody )
{
    UInt                    sBlockIdx;
    UInt                    sBlockID;
    smriCTBmpExtHdrBlock  * sBmpExtHdrBlock;
    void                  * sBlock;
    UInt                    sBlockIdx4Exception = 0;

    IDE_DASSERT( aCTBody != NULL );
    
    /* CTbody�� ù��° block�� ID�� ���Ѵ�.*/
    sBlockID = mCTFileHdr.mCTBodyCNT * mCTBodyBlockCnt;

    for( sBlockIdx = 0; 
         sBlockIdx < mCTBodyBlockCnt; 
         sBlockIdx++, sBlockID++ )
    {
        sBlock = &aCTBody->mBlock[ sBlockIdx ];

        /* Meta Extent �ʱ�ȭ */
        if( sBlockIdx < SMRI_CT_EXT_BLOCK_CNT )
        {
            if( ( sBlockIdx % SMRI_CT_EXT_BLOCK_CNT ) == 0 )
            {
                IDE_TEST( createExtMapBlock( 
                            (smriCTExtMapBlock *) sBlock,
                            sBlockID )
                        != IDE_SUCCESS );

            }
            else
            {
                IDE_TEST( createDataFileDescBlock(
                            (smriCTDataFileDescBlock *)sBlock,
                            sBlockID )
                        != IDE_SUCCESS );

            }
        }

        /*BmpExt �ʱ�ȭ*/
        if( (sBlockIdx >= SMRI_CT_EXT_BLOCK_CNT) &&
            (sBlockIdx < mCTBodyBlockCnt) )
        {
            /*BmpExtHdr�� �ʱ�ȭ*/
            if( ( sBlockIdx % SMRI_CT_EXT_BLOCK_CNT ) == 0 )
            {
                IDE_TEST( createBmpExtHdrBlock( 
                           (smriCTBmpExtHdrBlock *)sBlock,
                           sBlockID )
                          != IDE_SUCCESS );
            }
            else /*Bmp�� �ʱ�ȭ*/
            {
                IDE_TEST( createBmpBlock(
                           (smriCTBmpBlock *)sBlock,
                           sBlockID )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for( sBlockIdx4Exception = 0; 
         sBlockIdx4Exception < sBlockIdx;
         sBlockIdx4Exception++ )
    {
        IDE_ASSERT( 
            aCTBody->mBlock[ sBlockIdx4Exception ].mBlockHdr.mMutex.destroy() 
            == IDE_SUCCESS );
    
        if( ( sBlockIdx4Exception != SMRI_CT_EXT_MAP_BLOCK_IDX ) &&
            ( ( sBlockIdx4Exception % SMRI_CT_EXT_BLOCK_CNT ) == 0 ) )
        {
            sBmpExtHdrBlock = 
                (smriCTBmpExtHdrBlock*)&aCTBody->mBlock[ sBlockIdx4Exception ];

            IDE_ASSERT( sBmpExtHdrBlock->mLatch.destroy(  ) 
                        == IDE_SUCCESS );
        }
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * CT(change tracking)��������� �ʱ�ȭ �Ѵ�.
 * 
 * aCTFileHdrBlock  - [IN] CT���� ��� ��
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createFileHdrBlock( 
                                    smriCTFileHdrBlock * aCTFileHdrBlock )
{
    UInt    sState = 0;
    SChar * sDBName; 

    IDE_DASSERT( aCTFileHdrBlock != NULL );

    IDE_TEST( createBlockHdr( &aCTFileHdrBlock->mBlockHdr,
                              SMRI_CT_FILE_HDR_BLOCK,
                              SMRI_CT_FILE_HDR_BLOCK_ID )
              != IDE_SUCCESS );

    aCTFileHdrBlock->mCTBodyCNT     = 0;
    aCTFileHdrBlock->mIBChunkSize   = 
                                smuProperty::getIncrementalBackupChunkSize();

    SM_LSN_INIT( aCTFileHdrBlock->mLastFlushLSN );

    IDE_TEST(aCTFileHdrBlock->mFlushCV.initialize((SChar *)"CT_BODY_FLUSH_COND") 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST(aCTFileHdrBlock->mExtendCV.initialize((SChar *)"CT_BODY_EXTEND_COND") 
              != IDE_SUCCESS );
    sState = 2;

    sDBName = smmDatabase::getDBName();

    idlOS::strncpy( aCTFileHdrBlock->mDBName, sDBName, SM_MAX_DB_NAME );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( aCTFileHdrBlock->mExtendCV.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( aCTFileHdrBlock->mFlushCV.destroy() == IDE_SUCCESS );
        default:
            break;
    
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ExtMap���� �ʱ�ȭ �Ѵ�.
 * 
 * aExtMapBlock - [IN] ExtMap Block�� Ptr
 * aBlockID     - [IN] �� ID
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createExtMapBlock( 
                                   smriCTExtMapBlock * aExtMapBlock,
                                   UInt                aBlockID )
{
    IDE_DASSERT( aExtMapBlock != NULL );
    IDE_DASSERT( aBlockID     != SMRI_CT_INVALID_BLOCK_ID );

    IDE_TEST( createBlockHdr( &aExtMapBlock->mBlockHdr,
                              SMRI_CT_EXT_MAP_BLOCK,
                              aBlockID )
              != IDE_SUCCESS );

    aExtMapBlock->mCTBodyID = aBlockID / mCTBodyBlockCnt;

    idlOS::memset( aExtMapBlock->mExtentMap,
                   0,
                   mCTExtMapSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * dataFileDesc���� �ʱ�ȭ �Ѵ�.
 * 
 * adataFileDescBlock   - [IN] dataFileDesc block�� Ptr
 * aBlockID             - [IN] �� ID
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createDataFileDescBlock(
                        smriCTDataFileDescBlock * aDataFileDescBlock,
                        UInt                      aBlockID )
{
    UInt    sSlotIdx;

    IDE_DASSERT( aDataFileDescBlock  != NULL );
    IDE_DASSERT( aBlockID            != SMRI_CT_INVALID_BLOCK_ID );

    IDE_TEST( createBlockHdr( &aDataFileDescBlock->mBlockHdr,
                              SMRI_CT_DATAFILE_DESC_BLOCK,
                              aBlockID )
              != IDE_SUCCESS );

    aDataFileDescBlock->mAllocSlotFlag = 0;
    
    for( sSlotIdx = 0; sSlotIdx < SMRI_CT_DATAFILE_DESC_SLOT_CNT; sSlotIdx++ )
    {
        IDE_TEST( createDataFileDescSlot( 
                                &aDataFileDescBlock->mSlot[ sSlotIdx ],
                                aBlockID,
                                sSlotIdx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * DataFileDesc slot�� �ʱ�ȭ �Ѵ�.
 * 
 * aDataFileDescSlot    - [IN] DataFiledescSlot�� Ptr
 * aBlockID             - [IN] �� ID
 * aSlotIdx             - [IN] Slot�� index��ȣ
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createDataFileDescSlot(
                        smriCTDataFileDescSlot * aDataFileDescSlot,
                        UInt                     aBlockID,
                        UInt                     aSlotIdx )
{
    UInt    sBmpExtListIdx;

    IDE_DASSERT( aDataFileDescSlot != NULL );
    IDE_DASSERT( aBlockID          != SMRI_CT_INVALID_BLOCK_ID );
    IDE_DASSERT( aSlotIdx          != SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX );

    aDataFileDescSlot->mSlotID.mBlockID     = aBlockID;
    aDataFileDescSlot->mSlotID.mSlotIdx     = aSlotIdx;
    aDataFileDescSlot->mTrackingState       = SMRI_CT_TRACKING_DEACTIVE;
    aDataFileDescSlot->mTBSType             = SMRI_CT_NONE;
    aDataFileDescSlot->mPageSize            = 0;
    aDataFileDescSlot->mBmpExtCnt           = 0;
    aDataFileDescSlot->mCurTrackingListID   = SMRI_CT_DIFFERENTIAL_LIST0;
    aDataFileDescSlot->mFileID              = 0;
    aDataFileDescSlot->mSpaceID             = SC_NULL_SPACEID;

    for( sBmpExtListIdx = 0; 
         sBmpExtListIdx < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; 
         sBmpExtListIdx++ ) 
    {

        idlOS::memset( 
              &aDataFileDescSlot->mBmpExtList[ sBmpExtListIdx ].mBmpExtListHint,
              SMRI_CT_INVALID_BLOCK_ID,
              ID_SIZEOF( UInt ) * SMRI_CT_BMP_EXT_LIST_HINT_CNT );

        aDataFileDescSlot->mBmpExtList[ sBmpExtListIdx ].mSetBitCount = 0;
    }


    return IDE_SUCCESS;
}

/***********************************************************************
 * BmpExtHdrBlock�� �ʱ�ȭ �Ѵ�.
 * 
 * aBmpExtHdrBlock  - [IN] BmpExtHdrBlock�� Ptr
 * aBlockID         - [IN] �� ID
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createBmpExtHdrBlock( 
                                        smriCTBmpExtHdrBlock * aBmpExtHdrBlock,
                                        UInt                   aBlockID )
{
    SChar   sLatchName[128];
    UInt    sState = 0;

    IDE_DASSERT( aBmpExtHdrBlock != NULL );
    IDE_DASSERT( aBlockID        != SMRI_CT_INVALID_BLOCK_ID );
    
    IDE_TEST( createBlockHdr( &aBmpExtHdrBlock->mBlockHdr,
                              SMRI_CT_BMP_EXT_HDR_BLOCK,
                              aBlockID )
              != IDE_SUCCESS );

    aBmpExtHdrBlock->mPrevBmpExtHdrBlockID        = SMRI_CT_INVALID_BLOCK_ID;
    aBmpExtHdrBlock->mNextBmpExtHdrBlockID        = SMRI_CT_INVALID_BLOCK_ID;
    aBmpExtHdrBlock->mCumBmpExtHdrBlockID         = SMRI_CT_INVALID_BLOCK_ID;
    aBmpExtHdrBlock->mType                        = SMRI_CT_FREE_BMP_EXT;
    aBmpExtHdrBlock->mDataFileDescSlotID.mBlockID = SMRI_CT_INVALID_BLOCK_ID;
    aBmpExtHdrBlock->mDataFileDescSlotID.mSlotIdx = 
                                        SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;
    aBmpExtHdrBlock->mBmpExtSeq                   = 0;
    SM_LSN_INIT(aBmpExtHdrBlock->mFlushLSN);
    
    idlOS::snprintf( sLatchName,
                     ID_SIZEOF(sLatchName),
                     "BMP_EXT_HDR_LATCH_%"ID_UINT32_FMT,
                     aBlockID % SMRI_CT_EXT_BLOCK_CNT );

    IDE_TEST( aBmpExtHdrBlock->mLatch.initialize( sLatchName )
              != IDE_SUCCESS );
    sState = 1;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( sState == 1 )
    {
        IDE_ASSERT( aBmpExtHdrBlock->mLatch.destroy( ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BmpBlock�� �ʱ�ȭ �Ѵ�.
 * 
 * aBmpBlock  - [IN] BmpBlock�� Ptr
 * aBlockID   - [IN] �� ID
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createBmpBlock( 
                                smriCTBmpBlock * aBmpBlock,
                                UInt             aBlockID )
{
    IDE_DASSERT( aBmpBlock   != NULL );
    IDE_DASSERT( aBlockID    != SMRI_CT_INVALID_BLOCK_ID );
    
    IDE_TEST( createBlockHdr( &aBmpBlock->mBlockHdr,
                              SMRI_CT_BMP_BLOCK,
                              aBlockID )
              != IDE_SUCCESS );

    idlOS::memset( aBmpBlock->mBitmap, 
                   0, 
                   mBmpBlockBitmapSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ������� �ʱ�ȭ �Ѵ�.
 * 
 * aBlockHdr    - [IN] �����
 * aBlockType   - [IN] ������
 * aBlockID     - [IN] �� ID
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::createBlockHdr( 
                                  smriCTBlockHdr    * aBlockHdr,
                                  smriCTBlockType     aBlockType,
                                  UInt                aBlockID )
{
    SChar   sMutexName[128];
    UInt    sState = 0;

    idlOS::snprintf( sMutexName,
                     ID_SIZEOF(sMutexName),
                     "CT_BLOCK_MUTEX_%"ID_UINT32_FMT,
                     aBlockID );

    /*
     * BUG-34125 Posix mutex must be used for cond_timedwait(), cond_wait().
     */
    IDE_TEST( aBlockHdr->mMutex.initialize( sMutexName,
                                            IDU_MUTEX_KIND_POSIX,
                                            IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    aBlockHdr->mCheckSum        = 0;
    aBlockHdr->mBlockType       = aBlockType;
    aBlockHdr->mBlockID         = aBlockID;
    aBlockHdr->mExtHdrBlockID   = aBlockID & SMRI_CT_EXT_HDR_ID_MASK;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
 
    if(sState == 1 )
    {
        IDE_ASSERT( aBlockHdr->mMutex.destroy() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * �̹� �����ϴ� ��� DataFile���� CT���Ͽ� ����Ѵ�.
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::addAllExistingDataFile2CTFile( 
                                                    idvSQL * aStatistics )
{
    UInt    sState = 0;

    // XXX �� �ʿ�����? �ϴ� ������
    IDE_TEST( sctTableSpaceMgr::lockForCrtTBS() != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( aStatistics,
                                                  addExistingDataFile2CTFile,
                                                  NULL,  //aActionArg
                                                  SCT_ACT_MODE_LATCH )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sctTableSpaceMgr::unlockForCrtTBS() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sctTableSpaceMgr::unlockForCrtTBS() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * �̹� �����ϴ� ��� DataFile���� CT���Ͽ� ����Ѵ�.
 * 
 * aStatistics  - [IN] 
 * aSpaceNode   - [IN]  tableSpace Node
 * aActionAtg   - [IN]  action�Լ��� ���޵� ����
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::addExistingDataFile2CTFile( 
                                        idvSQL                    * aStatistics, 
                                        sctTableSpaceNode         * aSpaceNode, 
                                        void                      */* aActionArg*/ )
{
    UInt                        i;
    smiDataFileDescSlotID     * sSlotID;
    smmDatabaseFile           * sDatabaseFile0;
    smmDatabaseFile           * sDatabaseFile1;
    sddDataFileNode           * sDataFileNode = NULL;
    sddTableSpaceNode         * sDiskSpaceNode;
    smmTBSNode                * sMemSpaceNode;
    smmChkptImageAttr           sChkptImageAttr;

    IDE_DASSERT( aSpaceNode != NULL );

    if( sctTableSpaceMgr::isMemTableSpace( aSpaceNode->mID ) == ID_TRUE )
    {
        sMemSpaceNode = (smmTBSNode *)aSpaceNode;        

        for( i = 0; i <= sMemSpaceNode->mLstCreatedDBFile; i++ )
        {
            IDE_TEST( smmManager::openAndGetDBFile( sMemSpaceNode,
                                          0,
                                          i,
                                          &sDatabaseFile0 )
                      != IDE_SUCCESS );

            IDE_TEST( smmManager::openAndGetDBFile( sMemSpaceNode,
                                          1,
                                          i,
                                          &sDatabaseFile1 )
                      != IDE_SUCCESS );

            /* DataFileDescSlot �Ҵ� */
            IDE_TEST( addDataFile2CTFile( aSpaceNode->mID,
                                          i,
                                          SMRI_CT_MEM_TBS,
                                          &sSlotID )
                      != IDE_SUCCESS );
    
            /* ChkptInageHdr�� slotID ���� */
            sDatabaseFile0->setChkptImageHdr( NULL, //MemRedoLSN
                                              NULL, //MemCreateLSN
                                              NULL, //SpaceID
                                              NULL, //SmVersion
                                              sSlotID ); 

            IDE_TEST( sDatabaseFile0->flushDBFileHdr() != IDE_SUCCESS );

            /* ChkptInageHdr�� slotID ���� */
            sDatabaseFile1->setChkptImageHdr( NULL, //MemRedoLSN
                                              NULL, //MemCreateLSN
                                              NULL, //SpaceID
                                              NULL, //SmVersion
                                              sSlotID );

            IDE_TEST( sDatabaseFile1->flushDBFileHdr() != IDE_SUCCESS );

            sDatabaseFile0->getChkptImageAttr( sMemSpaceNode, 
                                               &sChkptImageAttr );
        
            /* logAnchor�� slotID���� */
            IDE_ASSERT( smrRecoveryMgr::updateChkptImageAttrToAnchor( 
                                            &(sMemSpaceNode->mCrtDBFileInfo[i]),
                                            &sChkptImageAttr )
                        == IDE_SUCCESS );
            
        }
    }
    else 
    {
        if( ( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode ) == ID_TRUE ) &&
            ( sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) != ID_TRUE ) )
        {
            sDiskSpaceNode = (sddTableSpaceNode *)aSpaceNode;
            
            for( i = 0; i < sDiskSpaceNode->mNewFileID; i++ )
            {
                sDataFileNode = sDiskSpaceNode->mFileNodeArr[i];
 
                if( sDataFileNode == NULL)     
                {
                    continue;
                }
 
                if( SMI_FILE_STATE_IS_DROPPED( sDataFileNode->mState ) )
                {
                    continue;
                }
 
                /* DataFileDescSlot �Ҵ� */
                IDE_TEST( addDataFile2CTFile( aSpaceNode->mID,
                                              i,
                                              SMRI_CT_DISK_TBS,
                                              &sSlotID )
                          != IDE_SUCCESS );
                
                /* DataFileHdr�� slotID ���� */
                sDataFileNode->mDBFileHdr.mDataFileDescSlotID.mBlockID 
                                                            = sSlotID->mBlockID;
                sDataFileNode->mDBFileHdr.mDataFileDescSlotID.mSlotIdx 
                                                            = sSlotID->mSlotIdx;
 
                IDE_TEST( sddDiskMgr::writeDBFHdr( aStatistics,
                                                   sDataFileNode ) 
                          != IDE_SUCCESS );
 
                /* logAnchor�� slotID���� */
                IDE_ASSERT( smrRecoveryMgr::updateDBFNodeToAnchor( 
                                                        sDataFileNode )
                            == IDE_SUCCESS );
            }
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * CT(change tracking)������ �����Ѵ�.
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::removeCTFile()
{
    SChar               sNULLFileName[ SM_MAX_FILE_NAME ] = "\0";
    SChar               sFileName[ SM_MAX_FILE_NAME ];
    smriCTMgrState      sCTMgrState;
    smLSN               sFlushLSN;
    smrLogAnchorMgr     sAnchorMgr4ProcessPhase;
    smrLogAnchorMgr   * sAnchorMgr;
    UInt                sCTBodyCnt;
    UInt                sMutexState     = 0;
    UInt                sMutexlockState = 0;
    UInt                sState          = 0;
    idBool              sLockState      = ID_FALSE;
    idBool              sIsCTMgrEnable = ID_FALSE;
    
    IDE_TEST( lockCTMgr() != IDE_SUCCESS );
    sLockState = ID_TRUE;

    if( smiGetStartupPhase() == SMI_STARTUP_PROCESS )
    {
        IDE_TEST( sAnchorMgr4ProcessPhase.initialize4ProcessPhase() 
                  != IDE_SUCCESS ); 
        sState = 1;

        sAnchorMgr = &sAnchorMgr4ProcessPhase;

        IDE_TEST_RAISE(sAnchorMgr->getCTMgrState() != SMRI_CT_MGR_ENABLED,
                       err_change_traking_state); 

        idlOS::strncpy( sFileName, 
                        sAnchorMgr->getCTFileName(), 
                        SM_MAX_FILE_NAME );

        /* logAnchor�� change tracking���� ���� ���� */
        SM_LSN_INIT( sFlushLSN );
        sCTMgrState = SMRI_CT_MGR_DISABLED;
        IDE_TEST( sAnchorMgr->updateCTFileAttr( sNULLFileName,
                                                &sCTMgrState,                  
                                                &sFlushLSN )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sAnchorMgr->destroy() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST_RAISE(smrRecoveryMgr::isCTMgrEnabled() != ID_TRUE,
                       err_change_traking_state);
        sIsCTMgrEnable = ID_TRUE;
        sMutexState = 1;
        
        IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/) 
                  != IDE_SUCCESS ); 
        sMutexlockState = 1;

        IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );

        sAnchorMgr = smrRecoveryMgr::getLogAnchorMgr();
 
        /* logAnchor�� change tracking���� ���� ���� */
        SM_LSN_INIT( sFlushLSN );
        sCTMgrState = SMRI_CT_MGR_DISABLED;
        IDE_TEST( sAnchorMgr->updateCTFileAttr( sNULLFileName,
                                                &sCTMgrState,                  
                                                &sFlushLSN )
                  != IDE_SUCCESS );
        
        sMutexlockState = 0;
        IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() 
                  != IDE_SUCCESS ); 
    
        idlOS::strncpy( sFileName, 
                        mFile.getFileName(), 
                        SM_MAX_FILE_NAME );

        // changeTracking�� �������� thread�� �ִٸ� �Ϸ�ɶ� ���� ����Ѵ�.
        while( getCurrChangeTrackingThreadCnt() != 0 )
        {
            idlOS::sleep(1);
        } 
 
        for( sCTBodyCnt = 0; sCTBodyCnt < mCTFileHdr.mCTBodyCNT; sCTBodyCnt++ )
        {
            IDE_TEST( unloadCTBody( mCTBodyPtr[ sCTBodyCnt ] ) != IDE_SUCCESS );
        }

        sMutexState = 0;
        IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.destroy() != IDE_SUCCESS );
 
        idlOS::memset( &mCTFileHdr, 0, SMRI_CT_BLOCK_SIZE );
    }

    /* ������ ���� �Ѵ�. */
    if( idf::access( sFileName, F_OK ) == 0 )
    {
        IDE_TEST( idf::unlink( sFileName ) != IDE_SUCCESS );
    }

    sLockState = ID_FALSE;
    IDE_TEST( unlockCTMgr() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_change_traking_state );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ChangeTrackingState));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( sAnchorMgr->destroy() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    if( sMutexlockState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS ); 
    }

    switch( sMutexState )
    {
        case 1:
            if( sIsCTMgrEnable == ID_TRUE )
            {
                IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.destroy() 
                            == IDE_SUCCESS );
            }
        case 0:
            idlOS::memset( &mCTFileHdr, 0, SMRI_CT_BLOCK_SIZE );
        default:
            break;
    }
    
    if( sLockState == ID_TRUE )
    {
        IDE_ASSERT( unlockCTMgr() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ct(change tracking)manager�� �ʱ�ȭ�Ѵ�.
 * startup control �ܰ迡�� ȣ��ȴ�.
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::begin()
{
    SChar             * sFileName;
    SChar               sMutexName[128] = "\0";
    smLSN               sLastFlushLSN;
    UInt                sLoadCTBodyCNT  = 0;
    ULong               sCTBodyOffset;
    UInt                sState          = 0;
    UInt                sCTBodyIdx      = 0;

    mCTHdrState  = SMRI_CT_MGR_STATE_NORMAL;

    /* logAnchor���� �����̸��� �����´�. */
    sFileName = smrRecoveryMgr::getCTFileName();

    IDE_TEST_RAISE( idf::access( sFileName, F_OK ) != 0,
                    error_not_exist_change_tracking_file );

    IDE_TEST( mFile.setFileName( sFileName ) != IDE_SUCCESS );

    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 1;

    /* change tracking������ ������� �о�� */
    IDE_TEST( mFile.read( NULL,  //aStatistics
                          SMRI_CT_HDR_OFFSET,
                          (void *)&mCTFileHdr,
                          SMRI_CT_BLOCK_SIZE,
                          NULL ) //ReadSize
              != IDE_SUCCESS );

    IDE_TEST( checkBlockCheckSum( &mCTFileHdr.mBlockHdr ) != IDE_SUCCESS );

    sLastFlushLSN = smrRecoveryMgr::getCTFileLastFlushLSNFromLogAnchor();

    /* ��������� ����� LSN�� logAnchor�� ����� LSN�� ���Ͽ� valid��
     * change tracking�������� �˻�
     */
    IDE_TEST_RAISE( smrCompareLSN::isEQ( &mCTFileHdr.mLastFlushLSN, 
                                         &sLastFlushLSN ) != ID_TRUE,
                    error_invalid_change_tracking_file );
    
    idlOS::snprintf( sMutexName,
                     ID_SIZEOF(sMutexName),
                     "CT_BLOCK_MUTEX_%"ID_UINT32_FMT,
                     SMRI_CT_INVALID_BLOCK_ID );
    
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.initialize( sMutexName,
                                                      IDU_MUTEX_KIND_POSIX,
                                                      IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST(mCTFileHdr.mFlushCV.initialize((SChar *)"CT_BODY_FLUSH_COND") != IDE_SUCCESS);
    sState = 3;

    IDE_TEST(mCTFileHdr.mExtendCV.initialize((SChar *)"CT_BODY_EXTEND_COND") != IDE_SUCCESS);
    sState = 4;
    
    idlOS::memset( &mCTBodyPtr, 0x00, SMRI_CT_MAX_CT_BODY_CNT );

    /* 
     * CTBody�� �޸𸮷� �о� �´�.
     * �޸� �Ҵ�, checksum�˻�, mutex, latch�ʱ�ȭ
     */
    for( sLoadCTBodyCNT = 0, sCTBodyOffset = SMRI_CT_BLOCK_SIZE; 
         sLoadCTBodyCNT < mCTFileHdr.mCTBodyCNT; 
         sLoadCTBodyCNT++, sCTBodyOffset += mCTBodySize )
    {
        IDE_TEST( loadCTBody( sCTBodyOffset, sLoadCTBodyCNT ) != IDE_SUCCESS );
    }

    IDE_TEST( validateCTFile() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_exist_change_tracking_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath, sFileName ) );
    }

    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidChangeTrackingFile,sFileName ) );
    }
    IDE_EXCEPTION_END;

    for( sCTBodyIdx = 0; 
         sCTBodyIdx < sLoadCTBodyCNT ; 
         sCTBodyIdx++ )
    {
        IDE_ASSERT( unloadCTBody( mCTBodyPtr[ sCTBodyIdx ] ) 
                    == IDE_SUCCESS );
    }

    switch( sState )
    {
        case 4:
            IDE_ASSERT( mCTFileHdr.mExtendCV.destroy() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( mCTFileHdr.mFlushCV.destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.destroy() 
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ct(change tracking)Body�� �ʱ�ȭ�Ѵ�.
 *
 * aCTBodyOffset - [IN] �о�� body�� ���ϳ��� offset
 * aCTBodyIdx    - [IN] �о�� bpdy�� idx
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::loadCTBody( ULong aCTBodyOffset, UInt aCTBodyIdx )
{
    smriCTBody    * sCTBody;
    UInt            sState = 0;

    /* smriChangeTrackingMgr_loadCTBody_calloc_CTBody.tc */
    IDU_FIT_POINT("smriChangeTrackingMgr::loadCTBody::calloc::CTBody");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                 1,
                                 ID_SIZEOF( smriCTBody ),
                                 (void**)&sCTBody )
              != IDE_SUCCESS );
    sState = 1;
    
    IDE_TEST( mFile.read( NULL,  //aStatistics
                          aCTBodyOffset,
                          (void *)sCTBody,
                          mCTBodySize,
                          NULL ) //ReadSize
              != IDE_SUCCESS );

    sCTBody->mBlock = (smriCTBmpBlock *)sCTBody;
    
    IDE_TEST_RAISE( checkCTBodyCheckSum( sCTBody, mCTFileHdr.mLastFlushLSN )
                    != IDE_SUCCESS,
                    error_invalid_change_tracking_file );
    
    IDE_TEST( initBlockMutex( sCTBody ) != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( initBmpExtLatch( sCTBody ) != IDE_SUCCESS );
    sState = 3;

    mCTBodyPtr[ aCTBodyIdx ]  = sCTBody;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidChangeTrackingFile, 
                mFile.getFileName()));
    }

    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 3:
            IDE_ASSERT( destroyBmpExtLatch( sCTBody ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( destroyBlockMutex( sCTBody ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( sCTBody ) == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ct(change tracking)������ body�� �˻��Ѵ�.
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::validateCTFile()
{
    UInt                        sAllocSlotFlag;
    UInt                        sCTBodyIdx;
    UInt                        sSlotIdx;
    UInt                        sDataFileDescBlockIdx;
    smriCTBody                * sCTBody;
    smriCTDataFileDescBlock   * sDataFileDescBlockArea;
    smriCTDataFileDescBlock   * sDataFileDescBlock;
    smriCTDataFileDescSlot    * sSlotArea;
    smriCTDataFileDescSlot    * sSlot;

    for( sCTBodyIdx = 0; sCTBodyIdx < mCTFileHdr.mCTBodyCNT; sCTBodyIdx++ )
    {
        sCTBody = mCTBodyPtr[ sCTBodyIdx ];

        IDE_DASSERT( sCTBody != NULL );

        sDataFileDescBlockArea = sCTBody->mMetaExtent.mDataFileDescBlock;
        
        for( sDataFileDescBlockIdx = 0; 
             sDataFileDescBlockIdx < SMRI_CT_DATAFILE_DESC_BLOCK_CNT; 
             sDataFileDescBlockIdx++ )
        {
            sDataFileDescBlock = &sDataFileDescBlockArea[ sDataFileDescBlockIdx ];

            IDE_ERROR( sDataFileDescBlock != NULL );

            sSlotArea = sDataFileDescBlock->mSlot;

            for( sSlotIdx = 0; 
                 sSlotIdx < SMRI_CT_DATAFILE_DESC_SLOT_CNT; 
                 sSlotIdx++ )
            {
                sAllocSlotFlag = (1 << sSlotIdx);
                if( (sDataFileDescBlock->mAllocSlotFlag & sAllocSlotFlag) != 0 )
                {
                    sSlot = &sSlotArea[ sSlotIdx ];

                    IDE_ERROR( sSlot->mBmpExtCnt % 
                               SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT == 0 );

                    IDE_ERROR( validateBmpExtList( 
                                    sSlot->mBmpExtList,
                                    (sSlot->mBmpExtCnt / 
                                     SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT) ) 
                               == IDE_SUCCESS ); 
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ct(change tracking)������ DataFileDesc slot��
 * �Ҵ�� BmpExtList�� �˻��Ѵ�.
 * 
 * aBmpExtList      - [IN] BmpExtList
 * aBmpExtListLen   - [IN] BmpExtList�� ����
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::validateBmpExtList( 
                                        smriCTBmpExtList * aBmpExtList,
                                        UInt               aBmpExtListLen )
{
    smriCTBmpExtHdrBlock    * sBmpExtHdrBlock;
    smriCTBmpExtList        * sBmpExtList;
    UInt                      sFirstBmpExtBlockID;
    UInt                      sBmpExtListIdx;
    UInt                      sBmpExtListLen = 0;

    IDE_DASSERT( aBmpExtList != NULL );

    for( sBmpExtListIdx = 0; 
         sBmpExtListIdx < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; 
         sBmpExtListIdx++ )
    {
        sBmpExtListLen      = 0;
        sBmpExtList         = &aBmpExtList[ sBmpExtListIdx ];
        sFirstBmpExtBlockID = sBmpExtList->mBmpExtListHint[0];

        IDE_ERROR( sFirstBmpExtBlockID != SMRI_CT_INVALID_BLOCK_ID );

        getBlock( sFirstBmpExtBlockID, (void**)&sBmpExtHdrBlock );

        sBmpExtListLen++;

        while( sFirstBmpExtBlockID != sBmpExtHdrBlock->mNextBmpExtHdrBlockID )
        {
            IDE_ERROR( ( sBmpExtHdrBlock->mNextBmpExtHdrBlockID != 
                         SMRI_CT_INVALID_BLOCK_ID ) ||
                       ( sBmpExtHdrBlock->mPrevBmpExtHdrBlockID != 
                         SMRI_CT_INVALID_BLOCK_ID ) );

            getBlock( sBmpExtHdrBlock->mNextBmpExtHdrBlockID, 
                                (void**)&sBmpExtHdrBlock );

            sBmpExtListLen++;

            if( sBmpExtListLen < SMRI_CT_BMP_EXT_LIST_HINT_CNT )
            {
                IDE_ERROR( sBmpExtHdrBlock->mBlockHdr.mBlockID == 
                           sBmpExtList->mBmpExtListHint[ sBmpExtListLen - 1 ] );
            }
        }

        IDE_ERROR( sBmpExtListLen == aBmpExtListLen );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * CT(change tracking)manager�� �ı��Ѵ�.
 * ���� ����ÿ� ȣ��ȴ�.
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::end()
{
    UInt            sCTBodyCnt;
    UInt            sState = 1;
    
    // changeTracking�� �������� thread�� �ִٸ� �Ϸ�ɶ� ���� ����Ѵ�.
    while( getCurrChangeTrackingThreadCnt() != 0 )
    {
        idlOS::sleep(1);
    } 

    for( sCTBodyCnt = 0; sCTBodyCnt < mCTFileHdr.mCTBodyCNT; sCTBodyCnt++ )
    {
        IDE_TEST( unloadCTBody( mCTBodyPtr[ sCTBodyCnt ] ) != IDE_SUCCESS );
    }
     
    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.destroy() != IDE_SUCCESS );

    idlOS::memset( &mCTFileHdr, 0, SMRI_CT_BLOCK_SIZE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.destroy() == IDE_SUCCESS );
        case 0:
            idlOS::memset( &mCTFileHdr, 0, SMRI_CT_BLOCK_SIZE );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * CT(change tracking)body�� �ı��Ѵ�.
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::unloadCTBody( smriCTBody * aCTBody )
{
    UInt    sState;

    IDE_DASSERT( aCTBody != NULL );

    sState = 2;
    IDE_TEST( destroyBlockMutex( aCTBody ) != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( destroyBmpExtLatch( aCTBody ) != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( iduMemMgr::free( aCTBody ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( destroyBmpExtLatch( aCTBody ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( aCTBody ) == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Block mutex�� �ʱ�ȭ �Ѵ�.
 * 
 * aCTBody  - [IN] CTBody�� ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::initBlockMutex( smriCTBody * aCTBody )
{
    UInt                sBlockIdx;
    UInt                sBlockIdx4Exception;
    smriCTBmpBlock    * sBlock;
    SChar               sMutexName[128];

    IDE_DASSERT( aCTBody != NULL );
    
    for( sBlockIdx = 0; sBlockIdx < mCTBodyBlockCnt; sBlockIdx++ )
    {
        sBlock = &aCTBody->mBlock[ sBlockIdx ];

        idlOS::snprintf( sMutexName,
                         ID_SIZEOF(sMutexName),
                         "CT_BLOCK_MUTEX_%"ID_UINT32_FMT,
                         sBlock->mBlockHdr.mBlockID );

        IDE_TEST( sBlock->mBlockHdr.mMutex.initialize( 
                                            sMutexName,
                                            IDU_MUTEX_KIND_POSIX,
                                            IDV_WAIT_INDEX_NULL )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for( sBlockIdx4Exception = 0; 
         sBlockIdx4Exception < sBlockIdx; 
         sBlockIdx4Exception++ )
    {
        sBlock = &aCTBody->mBlock[ sBlockIdx4Exception ];
        IDE_ASSERT( sBlock->mBlockHdr.mMutex.destroy() == IDE_SUCCESS );
    }    

    return IDE_FAILURE;
}

/***********************************************************************
 * Block mutex�� �ı��Ѵ�.
 * 
 * aCTBody  - [IN] CTBody�� ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::destroyBlockMutex( smriCTBody * aCTBody )
{
    UInt                sBlockIdx;
    smriCTBmpBlock    * sBlock;
    
    IDE_DASSERT( aCTBody != NULL );

    for( sBlockIdx = 0; sBlockIdx < mCTBodyBlockCnt; sBlockIdx++ )
    {
        sBlock = &aCTBody->mBlock[ sBlockIdx ];
        IDE_TEST( sBlock->mBlockHdr.mMutex.destroy() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * BmpExtHdr block�� latch�� �ʱ�ȭ �Ѵ�.
 * 
 * aCTBody  - [IN] CTBody�� Ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::initBmpExtLatch( smriCTBody * aCTBody )
{
    UInt            sBmpExtIdx;
    smriCTBmpExt  * sBmpExt;
    SChar           sLatchName[128];
    UInt            sState = 0;
    UInt            sBmpExtIdx4Exception = 0;
    
    IDE_DASSERT( aCTBody != NULL );

    for( sBmpExtIdx = 0; sBmpExtIdx < mCTBodyBmpExtCnt; sBmpExtIdx++ )
    {
        sBmpExt = &aCTBody->mBmpExtent[ sBmpExtIdx ];

        idlOS::snprintf( 
                sLatchName,
                ID_SIZEOF(sLatchName),
                "BMP_EXT_HDR_LATCH_%"ID_UINT32_FMT,
                sBmpExt->mBmpExtHdrBlock.mBlockHdr.mBlockID 
                % SMRI_CT_EXT_BLOCK_CNT );

        IDE_TEST( sBmpExt->mBmpExtHdrBlock.mLatch.initialize( 
                                        sLatchName )
                  != IDE_SUCCESS );
        sState++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for( sBmpExtIdx4Exception = 0;
         sBmpExtIdx4Exception < sBmpExtIdx; 
         sBmpExtIdx4Exception++ )
    {
        sBmpExt = &aCTBody->mBmpExtent[ sBmpExtIdx4Exception ];
        IDE_ASSERT( sBmpExt->mBmpExtHdrBlock.mLatch.destroy(  ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BmpExtHdr block�� latch�� �ı� �Ѵ�.
 * 
 * aCTBody  - [IN] CTBody�� Ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::destroyBmpExtLatch( smriCTBody * aCTBody )
{
    UInt            sBmpExtIdx;
    smriCTBmpExt  * sBmpExt;
    
    IDE_DASSERT( aCTBody != NULL );

    for( sBmpExtIdx = 0; sBmpExtIdx < mCTBodyBmpExtCnt; sBmpExtIdx++ )
    {
        sBmpExt = &aCTBody->mBmpExtent[ sBmpExtIdx ];

        IDE_TEST( sBmpExt->mBmpExtHdrBlock.mLatch.destroy(  )
                    != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_SUCCESS;
}

/***********************************************************************
 * CT���� flush
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::flush()
{
    smLSN   sFlushLSN;
    UInt    sState = 0;

    ideLog::log( IDE_SM_0,
                 "===================================\n"
                 " flush change tracking file [start]\n"
                 "===================================\n" );

    IDE_TEST( startFlush() != IDE_SUCCESS );
    sState = 1;


    IDE_TEST( mFile.open() != IDE_SUCCESS );
    sState = 2;

    /* CT������ ��ũ�� ����� ���� LSN ���� loganchor��
     * CTFileHdr�� �����Ѵ�.*/
    smrLogMgr::getLstLSN( &sFlushLSN );
    IDE_TEST( smrRecoveryMgr::updateCTFileAttrToLogAnchor( NULL, //FileName
                                                           NULL, //CTMgrState
                                                           &sFlushLSN )
              != IDE_SUCCESS );


    SM_GET_LSN( mCTFileHdr.mLastFlushLSN, sFlushLSN );

    IDE_TEST( flushCTBody( sFlushLSN ) != IDE_SUCCESS );


    IDE_TEST( flushCTFileHdr() != IDE_SUCCESS );


    IDE_TEST( mFile.sync() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( mFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( completeFlush() != IDE_SUCCESS );

    ideLog::log( IDE_SM_0,
                 "======================================\n"
                 " flush change tracking file [complete]\n"
                 "======================================\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( mFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( completeFlush() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CT������ flush�ϱ����� �غ� �۾�
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::startFlush()
{
    UInt    sState = 0;

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/) 
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while ( mCTHdrState == SMRI_CT_MGR_STATE_EXTEND )
    {
        IDE_TEST_RAISE(mCTFileHdr.mExtendCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                       != IDE_SUCCESS, error_cond_wait );
    }

    while ( mCTHdrState == SMRI_CT_MGR_STATE_FLUSH )
    {
        IDE_TEST_RAISE(mCTFileHdr.mFlushCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                       != IDE_SUCCESS, error_cond_wait );
    }

    IDE_ASSERT( mCTHdrState == SMRI_CT_MGR_STATE_NORMAL );

    mCTHdrState = SMRI_CT_MGR_STATE_FLUSH;
    
    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CT���� flush �Ϸ� �۾�
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::completeFlush()
{
    UInt    sState = 0;

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_ASSERT( mCTHdrState != SMRI_CT_MGR_STATE_NORMAL );

    mCTHdrState = SMRI_CT_MGR_STATE_NORMAL;

    IDE_TEST_RAISE(mCTFileHdr.mFlushCV.broadcast() != IDE_SUCCESS,
                   error_cond_signal );

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CTFileHdr flush
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::flushCTFileHdr()
{
    setBlockCheckSum( &mCTFileHdr.mBlockHdr );

    IDE_TEST( mFile.write( NULL, //aStatistics
                           SMRI_CT_HDR_OFFSET,
                           &mCTFileHdr,
                           SMRI_CT_BLOCK_SIZE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * CTBody flush
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::flushCTBody( smLSN aFlushLSN )
{
    smriCTBody    * sCTBody;
    UInt            sCTBodyWriteOffset;
    UInt            sCTBodyIdx;

    IDE_ASSERT( mCTBodyFlushBuffer != NULL );

    for( sCTBodyIdx = 0; sCTBodyIdx < mCTFileHdr.mCTBodyCNT; sCTBodyIdx++ )
    {
        sCTBody = mCTBodyPtr[ sCTBodyIdx ];

        idlOS::memcpy( mCTBodyFlushBuffer, sCTBody, ID_SIZEOF( smriCTBody ) );
 
        setCTBodyCheckSum( mCTBodyFlushBuffer, aFlushLSN );
 
        sCTBodyWriteOffset = ( sCTBodyIdx * mCTBodySize ) 
                             + SMRI_CT_BLOCK_SIZE;
 
        IDE_ERROR( sCTBodyIdx == sCTBody->mMetaExtent.mExtMapBlock.mCTBodyID );
 
        IDE_TEST( mFile.write( NULL, //aStatistics
                               sCTBodyWriteOffset,
                               mCTBodyFlushBuffer, 
                               mCTBodySize ) 
                  != IDE_SUCCESS );
        
        idlOS::memset( mCTBodyFlushBuffer, 0x00, ID_SIZEOF( smriCTBody ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * CT���� extend
 * 
 * aFlushBody - [IN] extend�Ϸ��� ���Ϸ� �������� �� �Ǵ�
 * aCTBody    - [OUT] extend�� CTBody�� ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::extend( idBool        aFlushBody, 
                                      smriCTBody ** aCTBody )
{
    idBool          sIsNeedExtend;
    smriCTBody    * sCTBody = NULL;
    UInt            sCTBodyIdx;
    smLSN           sFlushLSN;
    UInt            sState      = 0;
    UInt            sFileState  = 0;

    IDE_DASSERT( aCTBody != NULL );

    ideLog::log( IDE_SM_0,
                 "====================================\n"
                 " extend change tracking file [start]\n"
                 "====================================\n" );

    IDE_TEST( startExtend( &sIsNeedExtend ) != IDE_SUCCESS );
    sState = 1;


    IDE_TEST_CONT( sIsNeedExtend == ID_FALSE, skip_extend );

    /* smriChangeTrackingMgr_extend_calloc_CTBody.tc */
    IDU_FIT_POINT("smriChangeTrackingMgr::extend::calloc::CTBody");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                 1,
                                 ID_SIZEOF( smriCTBody ),
                                 (void**)&sCTBody )
                  != IDE_SUCCESS );
    sState = 2;

    sCTBody->mBlock = (smriCTBmpBlock *)sCTBody;

    IDE_TEST( createCTBody( sCTBody ) != IDE_SUCCESS );

    mCTFileHdr.mCTBodyCNT++;
    
    sCTBodyIdx = mCTFileHdr.mCTBodyCNT - 1;

    mCTBodyPtr[ sCTBodyIdx ] = sCTBody;

    if( aFlushBody == ID_TRUE )
    {
        IDE_TEST( mFile.open() != IDE_SUCCESS );
        sFileState = 1;

        smrLogMgr::getLstLSN( &sFlushLSN );

        SM_GET_LSN( mCTFileHdr.mLastFlushLSN, sFlushLSN );
 
        IDE_TEST( smrRecoveryMgr::updateCTFileAttrToLogAnchor( NULL,//FileName
                                                               NULL,//CTMgrState
                                                               &sFlushLSN )
              != IDE_SUCCESS );

        IDE_TEST( flushCTBody( sFlushLSN ) != IDE_SUCCESS );
        
        IDE_TEST( flushCTFileHdr() != IDE_SUCCESS );

        IDE_TEST( mFile.sync() != IDE_SUCCESS );

        sFileState = 0;
        IDE_TEST( mFile.close() != IDE_SUCCESS );
    }
    else
    {
        //noting to do
    }

    IDE_EXCEPTION_CONT(skip_extend);


    sState = 0;
    IDE_TEST( completeExtend() != IDE_SUCCESS );


    ideLog::log( IDE_SM_0,
                 "=======================================\n"
                 " extend change tracking file [complete]\n"
                 "=======================================\n" );

    *aCTBody = sCTBody; 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            IDE_ASSERT( iduMemMgr::free( sCTBody ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( completeExtend() == IDE_SUCCESS );
        default:
            break;
    }

    if( sFileState == 1 )
    {
        IDE_ASSERT( mFile.close() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CT������ Extend�ϱ����� �غ� �۾�
 * 
 * aIsNeedExtend    - [OUT] extend�� �ʿ����� ����
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::startExtend( idBool * aIsNeedExtend )
{
    UInt    sState = 0;

    IDE_DASSERT( aIsNeedExtend   != NULL );

    *aIsNeedExtend = ID_TRUE;

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while ( mCTHdrState == SMRI_CT_MGR_STATE_EXTEND )
    {
        IDE_TEST_RAISE(mCTFileHdr.mExtendCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                       != IDE_SUCCESS, error_cond_wait );
        *aIsNeedExtend = ID_FALSE;
    }

    while ( mCTHdrState == SMRI_CT_MGR_STATE_FLUSH )
    {
        IDE_TEST_RAISE(mCTFileHdr.mFlushCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                       != IDE_SUCCESS, error_cond_wait);
    }

    IDE_ASSERT( mCTHdrState == SMRI_CT_MGR_STATE_NORMAL );

    mCTHdrState = SMRI_CT_MGR_STATE_EXTEND;

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CT���� Extend �Ϸ� �۾�
 * 
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::completeExtend()
{
    UInt    sState = 0;

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_ASSERT( mCTHdrState != SMRI_CT_MGR_STATE_NORMAL );

    mCTHdrState = SMRI_CT_MGR_STATE_NORMAL;

    IDE_TEST_RAISE( mCTFileHdr.mExtendCV.broadcast() != IDE_SUCCESS,
                    error_cond_signal );

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_signal);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondSignal));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
} 

/***********************************************************************
 * CT������ flush�� extend�� �Ϸ�Ǳ⸦ �����
 * 
 * �� �Լ��� ȣ���ϱ� ���ؼ��� CTFileHdr�� mutex�� ȹ���� �����̿��� �Ѵ�.
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::wait4FlushAndExtend()
{
    /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                 wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
    while ( mCTHdrState == SMRI_CT_MGR_STATE_FLUSH )
    {
        IDE_TEST_RAISE( mCTFileHdr.mFlushCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                        != IDE_SUCCESS, error_cond_wait );
    }

    while ( mCTHdrState == SMRI_CT_MGR_STATE_EXTEND )
    {
        IDE_TEST_RAISE(mCTFileHdr.mExtendCV.wait(&(mCTFileHdr.mBlockHdr.mMutex))
                       != IDE_SUCCESS, error_cond_wait );
    }

    IDE_ERROR( mCTHdrState == SMRI_CT_MGR_STATE_NORMAL );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_cond_wait);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ������������ CT���Ͽ� ���
 * �Լ� ���н� change tracking���� invalid
 *
 * aSpaceID     - [IN]  ������������ ���̺����̽� ID
 * aDataFileID  - [IN]  ������������ ID
 * aPageSize    - [IN]  ������������ ������ũ��
 * aSlotID      - [OUT] �Ҵ�� slot�� ID
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::addDataFile2CTFile( 
                                      scSpaceID                aSpaceID, 
                                      UInt                     aDataFileID,
                                      smriCTTBSType            aTBSType,
                                      smiDataFileDescSlotID ** aSlotID )
{
    smriCTDataFileDescSlot    * sSlot;
    smriCTBmpExt              * sDummyBmpExt;
    UInt                        sState = 0;

    IDE_DASSERT( aSlotID != NULL );

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/) 
              != IDE_SUCCESS ); 
    sState = 1;

    IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );

    /* �߰��Ǵ� ���������Ͽ� DataFileDescSlot�� �Ҵ��Ѵ�. */
    IDE_TEST( allocDataFileDescSlot( &sSlot ) !=IDE_SUCCESS );

    /* DataFileDescSlot�� BmpExt�� �߰��Ѵ�. */
    IDE_TEST( addBmpExt2DataFileDescSlot( sSlot,
                                          &sDummyBmpExt ) 
              != IDE_SUCCESS );

    /* DataFileDescSlot ���� ���� */
    sSlot->mSpaceID        = aSpaceID;

    if( aTBSType == SMRI_CT_MEM_TBS )
    {
        sSlot->mTBSType     = SMRI_CT_MEM_TBS;
        sSlot->mPageSize    = SM_PAGE_SIZE;
        sSlot->mFileID      = (UInt)aDataFileID;
    }
    else
    { 
        if( aTBSType == SMRI_CT_DISK_TBS )
        {
            sSlot->mTBSType     = SMRI_CT_DISK_TBS;
            sSlot->mPageSize    = SD_PAGE_SIZE;
            sSlot->mFileID      = (UInt)aDataFileID;
        }
        else
        {
            IDE_DASSERT(0);
            /* nothing to do */
        }
    }

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS ); 

    *aSlotID = &sSlot->mSlotID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ��������� ���� DataFile Desc slot�� �����´�.
 * �Ѽ��� ȣ��Ǳ� ���� CTFileHdr�� mutex�� ���� ���¿��� �Ѵ�.
 * DataFileDescSlot�� �Ҵ��� CTFileHdr�� mutex�� ��� ���� �Ǳ� ������
 * DataFileDescBlock�� mutex�� ȹ���� �ʿ䰡 ����.
 * 
 * aSlot    - [OUT] �Ҵ���� DataFile Desc slot�� ptr
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::allocDataFileDescSlot( 
                                        smriCTDataFileDescSlot ** aSlot )
{
    smriCTDataFileDescBlock * sDataFileDescBlock;
    smriCTBody              * sCTBody;
    UInt                      sCTBodyIdx;
    UInt                      sDataFileDescBlockIdx;
    UInt                      sSlotIdx = SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;

    IDE_DASSERT( aSlot != NULL );

    IDE_ERROR( 0 < mCTFileHdr.mCTBodyCNT );

    for( sCTBodyIdx = 0; sCTBodyIdx < mCTFileHdr.mCTBodyCNT; sCTBodyIdx++ )
    {
        sCTBody = mCTBodyPtr[ sCTBodyIdx ];

        for( sDataFileDescBlockIdx = 0; 
             sDataFileDescBlockIdx < SMRI_CT_DATAFILE_DESC_BLOCK_CNT; 
             sDataFileDescBlockIdx++ )
        {
            sDataFileDescBlock = 
                    &sCTBody->mMetaExtent.mDataFileDescBlock[ sDataFileDescBlockIdx ];
 
            if( SMRI_CT_CAN_ALLOC_DATAFILE_DESC_SLOT( 
                            sDataFileDescBlock->mAllocSlotFlag ) == ID_TRUE )
            {
                getFreeSlotIdx( sDataFileDescBlock->mAllocSlotFlag, &sSlotIdx );
 
                IDE_TEST_RAISE( sSlotIdx == 
                                SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX,
                                error_invalid_change_tracking_file );

                sDataFileDescBlock->mAllocSlotFlag |= ( 1 << sSlotIdx );
 
                IDE_CONT( datafile_descriptor_slot_is_allocated );
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    IDE_EXCEPTION_CONT(datafile_descriptor_slot_is_allocated);

    *aSlot = &sDataFileDescBlock->mSlot[ sSlotIdx ];
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidChangeTrackingFile, 
                mFile.getFileName() ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ��������� ���� DataFile Desc slot idx�� �����´�
 * 
 * aAllocSlotFlag   - [IN] �Ҵ��Ϸ��� slot�� �����ϴ� DataFileDesc ��
 * aSlotIdx         - [OUT] �Ҵ� �Ϸ��� DataFileDesc slot�� index
 * 
 **********************************************************************/
void smriChangeTrackingMgr::getFreeSlotIdx( UInt aAllocSlotFlag, 
                                            UInt * aSlotIdx )
{
    UInt    sSlotIdx;

    IDE_DASSERT( aAllocSlotFlag != SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX )
    IDE_DASSERT( aSlotIdx       != NULL );

    if( (aAllocSlotFlag & SMRI_CT_DATAFILE_DESC_SLOT_FLAG_FIRST  ) == 0 )
    {
        sSlotIdx = 0;
    }
    else
    {
        if( (aAllocSlotFlag & SMRI_CT_DATAFILE_DESC_SLOT_FLAG_SECOND  ) == 0 )
        {
            sSlotIdx = 1;
        }
        else
        {
            if( (aAllocSlotFlag & SMRI_CT_DATAFILE_DESC_SLOT_FLAG_THIRD  ) == 0 )
            {
                sSlotIdx = 2;
            }
            else
            {
                sSlotIdx = SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;
            }
        }
    }

    *aSlotIdx = sSlotIdx;

    return;
}

/***********************************************************************
 * DataFileDesc slot�� BmpExt�� �߰��Ѵ�.
 * 
 * aSlot            - [IN] BmpExt�� ���ε� �� slot
 * aAllocCurBmpExt  - [OUT] ���� �߰��� BmpExt��CurTrackingListID�� �ش��ϴ�
 *                          BmpExt�� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::addBmpExt2DataFileDescSlot( 
                                    smriCTDataFileDescSlot * aSlot,
                                    smriCTBmpExt          ** aAllocCurBmpExt )
{
    UInt                    i;
    smriCTBmpExtHdrBlock  * sBmpExtHdrBlock;
    smriCTBmpExt          * sBmpExt[ SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT ];

    IDE_DASSERT( aSlot           != NULL );
    IDE_DASSERT( aAllocCurBmpExt != NULL );
    
    for( i = 0; i < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; i++ )
    {
        IDE_TEST( allocBmpExt( &sBmpExt[i] ) != IDE_SUCCESS );

        sBmpExtHdrBlock                     = &sBmpExt[i]->mBmpExtHdrBlock;
        sBmpExtHdrBlock->mType              = (smriCTBmpExtType)i;
        sBmpExtHdrBlock->mDataFileDescSlotID.mBlockID   
                                            = aSlot->mSlotID.mBlockID;
        sBmpExtHdrBlock->mDataFileDescSlotID.mSlotIdx   
                                            = aSlot->mSlotID.mSlotIdx;
        
        addBmpExt2List( &aSlot->mBmpExtList[i], sBmpExt[i] );

        aSlot->mBmpExtCnt++;
    }
    
    sBmpExt[SMRI_CT_DIFFERENTIAL_BMP_EXT_0]->mBmpExtHdrBlock.mCumBmpExtHdrBlockID = 
                sBmpExt[SMRI_CT_CUMULATIVE_BMP_EXT]->mBmpExtHdrBlock.mBlockHdr.mBlockID;

    sBmpExt[SMRI_CT_DIFFERENTIAL_BMP_EXT_1]->mBmpExtHdrBlock.mCumBmpExtHdrBlockID = 
                sBmpExt[SMRI_CT_CUMULATIVE_BMP_EXT]->mBmpExtHdrBlock.mBlockHdr.mBlockID;

    *aAllocCurBmpExt = sBmpExt[ aSlot->mCurTrackingListID ];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * DataFileDesc slot�� BmpExt�� �Ҵ��Ѵ�.
 * 
 * aAllocBmpExt   - [OUT] �Ҵ�� BmpExt ��ȯ
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::allocBmpExt( smriCTBmpExt ** aAllocBmpExt )
{
    UInt                sExtMapIdx;
    UInt                sAllocBmpExtIdx;
    UInt                sCTBodyIdx;
    smriCTExtMapBlock * sExtMapBlock;
    smriCTBody        * sCTBody;
    smriCTBody        * sDummyCTBody;
    UInt                sScanStartCTBodyIdx = 0;
    idBool              sResult             = ID_FALSE;
    idBool              sMutexState         = ID_TRUE;

    IDE_DASSERT( aAllocBmpExt != NULL );

    while(1)
    {
        for( sCTBodyIdx = sScanStartCTBodyIdx; 
             sCTBodyIdx < mCTFileHdr.mCTBodyCNT; 
             sCTBodyIdx++ )
        {
            sCTBody = mCTBodyPtr[ sCTBodyIdx ];
 
            sExtMapBlock = &sCTBody->mMetaExtent.mExtMapBlock;
        
            for( sExtMapIdx = 0; sExtMapIdx < mCTExtMapSize; sExtMapIdx++)
            {
                if( sExtMapBlock->mExtentMap[ sExtMapIdx ] == SMRI_CT_EXT_FREE )
                {
                    sExtMapBlock->mExtentMap[ sExtMapIdx ] = SMRI_CT_EXT_USE;
                    sAllocBmpExtIdx                        = sExtMapIdx;
                    sResult                                = ID_TRUE;
                    IDE_CONT( bitmap_extent_is_allocated );
                }
                else
                {
                    /* nothing to do */
                }
            }
        }
 
        /* BmpExt�� ������ ��Ȳ CTBody�� Ȯ���Ѵ�. */
        if ( sResult == ID_FALSE )
        {
            IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );
            sMutexState = ID_FALSE;
 
            IDE_TEST( extend( ID_TRUE, &sDummyCTBody ) != IDE_SUCCESS );
 
            
            sMutexState = ID_TRUE;
            IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
                      != IDE_SUCCESS );
 
            sScanStartCTBodyIdx = mCTFileHdr.mCTBodyCNT - 1; 
        }
        else 
        {
            /* BmpExt�Ҵ翡 ������
             * sResult == ID_TRUE
             */
            break;
        }
    }   

    IDE_EXCEPTION_CONT(bitmap_extent_is_allocated);

    IDE_DASSERT( sResult == ID_TRUE );

    *aAllocBmpExt = &sCTBody->mBmpExtent[ sAllocBmpExtIdx ];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( sMutexState == ID_FALSE )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.lock( NULL ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BmpExtList�� BmpExt�� add�Ѵ�.
 * 
 * aBaseBmpExt  - [IN] BmpExtList�� ��� BmpExt
 * aNewBmpExt   - [IN] BmpExtList�� �߰��� BmpExt
 *
 **********************************************************************/
void smriChangeTrackingMgr::addBmpExt2List( 
                                    smriCTBmpExtList       * aBmpExtList,
                                    smriCTBmpExt           * aNewBmpExt )
{
    UInt                   sBaseBmpExtHdrBlockID;
    smriCTBmpExtHdrBlock * sBaseBmpExtHdrBlock;
    smriCTBmpExtHdrBlock * sNewBmpExtHdrBlock;
    smriCTBmpExtHdrBlock * sBasePrevBmpExtHdrBlock;
    
    IDE_DASSERT( aBmpExtList != NULL );
    IDE_DASSERT( aNewBmpExt  != NULL );
    
    sBaseBmpExtHdrBlockID   = aBmpExtList->mBmpExtListHint[0];
    sNewBmpExtHdrBlock      = &aNewBmpExt->mBmpExtHdrBlock;

    if( sBaseBmpExtHdrBlockID != SMRI_CT_INVALID_BLOCK_ID )
    {
        getBlock( sBaseBmpExtHdrBlockID, (void**)&sBaseBmpExtHdrBlock );

        sNewBmpExtHdrBlock->mNextBmpExtHdrBlockID = 
                                    sBaseBmpExtHdrBlock->mBlockHdr.mBlockID;
        sNewBmpExtHdrBlock->mPrevBmpExtHdrBlockID = 
                                    sBaseBmpExtHdrBlock->mPrevBmpExtHdrBlockID;

        getBlock( sBaseBmpExtHdrBlock->mPrevBmpExtHdrBlockID, 
                  (void**)&sBasePrevBmpExtHdrBlock );

        sBasePrevBmpExtHdrBlock->mNextBmpExtHdrBlockID = 
                                    sNewBmpExtHdrBlock->mBlockHdr.mBlockID;
        sBaseBmpExtHdrBlock->mPrevBmpExtHdrBlockID = 
                                    sNewBmpExtHdrBlock->mBlockHdr.mBlockID;

        sNewBmpExtHdrBlock->mBmpExtSeq = 
                                    sBasePrevBmpExtHdrBlock->mBmpExtSeq + 1;

        if( sNewBmpExtHdrBlock->mBmpExtSeq < SMRI_CT_BMP_EXT_LIST_HINT_CNT )
        {
            aBmpExtList->mBmpExtListHint[ sNewBmpExtHdrBlock->mBmpExtSeq ] = 
                                    sNewBmpExtHdrBlock->mBlockHdr.mBlockID;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        sNewBmpExtHdrBlock->mNextBmpExtHdrBlockID =
                                     sNewBmpExtHdrBlock->mBlockHdr.mBlockID;
        sNewBmpExtHdrBlock->mPrevBmpExtHdrBlockID =
                                     sNewBmpExtHdrBlock->mBlockHdr.mBlockID;
        sNewBmpExtHdrBlock->mBmpExtSeq    = 0;
        aBmpExtList->mBmpExtListHint[ 0 ] = 
                                    sNewBmpExtHdrBlock->mBlockHdr.mBlockID;

    }
        
    return;
}

/***********************************************************************
 * DataFileDesc slot�� ��ȯ�Ѵ�.
 * �Լ� ���н� change tracking���� invalid
 *
 * aSlotID  - [IN] �Ҵ������Ϸ��� slot�� ID
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::deleteDataFileFromCTFile( 
                                    smiDataFileDescSlotID * aSlotID )
{
    smriCTDataFileDescSlot    * sSlot; 
    UInt                        sState = 0;

    IDE_DASSERT( aSlotID != NULL );

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS ); 
    sState = 1;

    IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );

    getDataFileDescSlot( aSlotID, &sSlot );

    /* slot�� �Ҵ�� ��� BmpExt�� �����Ѵ�. */
    IDE_TEST( deleteBmpExtFromDataFileDescSlot( sSlot ) != IDE_SUCCESS );

    /* DataFileDescSlot ���� �ʱ�ȭ�� �Ҵ� ���� */
    IDE_TEST( deallocDataFileDescSlot( sSlot ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS ); 
    }
    
    return IDE_FAILURE;
}

/***********************************************************************
 * slot�� �Ҵ�� BmpExtList�� �ʱ�ȭ�Ѵ�.
 * 
 * aBmpExtList  - [IN] slot���� ��ȯ�Ϸ��� BmpExt���� List
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::deleteBmpExtFromDataFileDescSlot( 
                                            smriCTDataFileDescSlot * aSlot )
{
    smriCTBmpExtHdrBlock  * sBmpExtHdrBlock;
    smriCTBmpExtList      * sBmpExtList;
    UInt                    sBmpExtListLen;
    UInt                    sBmpExtHintIdx;
    UInt                    sDeleteBmpExtCnt;
    UInt                    i;

    IDE_DASSERT( aSlot != NULL );

    sBmpExtListLen = aSlot->mBmpExtCnt / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT;

    for( i = 0; i < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT; i++ )
    {
        sBmpExtList = &aSlot->mBmpExtList[i];

        for( sDeleteBmpExtCnt = 0; 
             sDeleteBmpExtCnt < sBmpExtListLen; 
             sDeleteBmpExtCnt++ )
        {
            IDE_TEST( deleteBmpExtFromList( sBmpExtList, &sBmpExtHdrBlock ) 
                      != IDE_SUCCESS );

            aSlot->mBmpExtCnt--;

            IDE_TEST( deallocBmpExt( sBmpExtHdrBlock ) != IDE_SUCCESS );
        }
    
        for( sBmpExtHintIdx = 0; 
             sBmpExtHintIdx < SMRI_CT_BMP_EXT_LIST_HINT_CNT; 
             sBmpExtHintIdx++ )
        {
            IDE_DASSERT( sBmpExtList->mBmpExtListHint[ sBmpExtHintIdx ] 
                         == SMRI_CT_INVALID_BLOCK_ID );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * BmpExtList�κ��� list�� ������ BmpExt�� �����Ѵ�.
 * 
 * aBmpExtList      - [IN] BmpExt�� �����Ϸ��� List
 * aBmpExtHdrBlock  - [OUT] List�κ��� ������ BmpExt�� BmpExtHdrBlock
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::deleteBmpExtFromList( 
                                    smriCTBmpExtList      * aBmpExtList,
                                    smriCTBmpExtHdrBlock ** aBmpExtHdrBlock )
{
    UInt                   sBaseBmpExtHdrBlockID;
    UInt                   sLastBmpExtHdrBlockID;
    UInt                   sLastPrevBmpExtHdrBlockID;
    smriCTBmpExtHdrBlock * sBaseBmpExtHdrBlock;
    smriCTBmpExtHdrBlock * sLastBmpExtHdrBlock;
    smriCTBmpExtHdrBlock * sLastPrevBmpExtHdrBlock;

    IDE_DASSERT( aBmpExtList         != NULL );
    IDE_DASSERT( aBmpExtHdrBlock     != NULL );

    sBaseBmpExtHdrBlockID = aBmpExtList->mBmpExtListHint[0];

    IDE_ERROR( sBaseBmpExtHdrBlockID != SMRI_CT_INVALID_BLOCK_ID );
    
    getBlock( sBaseBmpExtHdrBlockID, (void**)&sBaseBmpExtHdrBlock );

    sLastBmpExtHdrBlockID = sBaseBmpExtHdrBlock->mPrevBmpExtHdrBlockID;

    if( sBaseBmpExtHdrBlockID != sLastBmpExtHdrBlockID )
    {
        getBlock( sLastBmpExtHdrBlockID, (void**)&sLastBmpExtHdrBlock );

        sLastPrevBmpExtHdrBlockID = sLastBmpExtHdrBlock->mPrevBmpExtHdrBlockID;
        getBlock( sLastPrevBmpExtHdrBlockID, (void**)&sLastPrevBmpExtHdrBlock );

        sBaseBmpExtHdrBlock->mPrevBmpExtHdrBlockID = sLastPrevBmpExtHdrBlockID;
        sLastPrevBmpExtHdrBlock->mNextBmpExtHdrBlockID = sBaseBmpExtHdrBlockID;
        
    }
    else
    {
        /* 
         * aBmpExtList�� BmpExt�� 1�� �����ִ»�Ȳ, ������ list������ ������
         * �ʿ䰡 ����
         */
        sLastBmpExtHdrBlock = sBaseBmpExtHdrBlock;
    }

    if( sLastBmpExtHdrBlock->mBmpExtSeq < SMRI_CT_BMP_EXT_LIST_HINT_CNT )
    {
        IDE_ERROR( aBmpExtList->mBmpExtListHint[sLastBmpExtHdrBlock->mBmpExtSeq] 
                     != SMRI_CT_INVALID_BLOCK_ID );

        aBmpExtList->mBmpExtListHint[sLastBmpExtHdrBlock->mBmpExtSeq] = 
                                      SMRI_CT_INVALID_BLOCK_ID;
    }

    *aBmpExtHdrBlock = sLastBmpExtHdrBlock;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * BmpExt�� �Ҵ� �����Ѵ�.
 * 
 * aBmpExtHdrBlock  - [IN] �Ҵ������Ϸ��� BmpExtHdrBlock
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::deallocBmpExt( 
                                smriCTBmpExtHdrBlock  * aBmpExtHdrBlock )
{

    UInt                    sCTBodyIdx;
    UInt                    sBmpExtIdx;
    UInt                    sBmpBlockIdx;
    smriCTBody            * sCTBody;
    smriCTBmpExt          * sBmpExt;
    smriCTBmpBlock        * sBmpBlock;
    smriCTExtMapBlock     * sExtMapBlock;

    IDE_DASSERT( aBmpExtHdrBlock != NULL );    

    /* BmpExt�� ���� CTBody�� Idx�� ���Ѵ�. */
    sCTBodyIdx = aBmpExtHdrBlock->mBlockHdr.mBlockID / mCTBodyBlockCnt;

    /* CTBody������ BmpExt�� Idx���� ���Ѵ�. */
    sBmpExtIdx = ( aBmpExtHdrBlock->mBlockHdr.mBlockID 
                   - (sCTBodyIdx * mCTBodyBlockCnt) ) 
                   / SMRI_CT_EXT_BLOCK_CNT;

    sCTBody         = mCTBodyPtr[ sCTBodyIdx ];
    sExtMapBlock    = &sCTBody->mMetaExtent.mExtMapBlock;

    sBmpExt = &sCTBody->mBmpExtent[ sBmpExtIdx - 1 ];

    IDE_ERROR( sBmpExt->mBmpExtHdrBlock.mBlockHdr.mBlockID == 
                 aBmpExtHdrBlock->mBlockHdr.mBlockID );
    
    /* BmpExt�� ������ �ʱ�ȭ �Ѵ�. */
    sBmpExt->mBmpExtHdrBlock.mNextBmpExtHdrBlockID = SMRI_CT_INVALID_BLOCK_ID;
    sBmpExt->mBmpExtHdrBlock.mPrevBmpExtHdrBlockID = SMRI_CT_INVALID_BLOCK_ID;
    sBmpExt->mBmpExtHdrBlock.mCumBmpExtHdrBlockID  = SMRI_CT_INVALID_BLOCK_ID;
    sBmpExt->mBmpExtHdrBlock.mBmpExtSeq            = 0;
    sBmpExt->mBmpExtHdrBlock.mType                 = SMRI_CT_FREE_BMP_EXT;

    sBmpExt->mBmpExtHdrBlock.mDataFileDescSlotID.mBlockID = 
                                        SMRI_CT_INVALID_BLOCK_ID;
    sBmpExt->mBmpExtHdrBlock.mDataFileDescSlotID.mSlotIdx = 
                                        SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX;
    
    /*BmpBlock�� bitmap������ 0���� �ʱ�ȭ�Ѵ�. */
    for( sBmpBlockIdx = 0; 
         sBmpBlockIdx < SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK; 
         sBmpBlockIdx++ )
    {
        sBmpBlock = &sBmpExt->mBmpBlock[ sBmpBlockIdx ];
        idlOS::memset( &sBmpBlock->mBitmap, 0, mBmpBlockBitmapSize );
    }

    IDE_ERROR( sExtMapBlock->mExtentMap[ sBmpExtIdx - 1 ] == SMRI_CT_EXT_USE );

    /* extent map���� �ش� BmpExt�� �Ҵ��� �����Ѵ�. */
    sExtMapBlock->mExtentMap[ sBmpExtIdx - 1 ] = SMRI_CT_EXT_FREE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * �Ҵ������Ǵ� BmpExt�� �ʱ�ȭ�Ѵ�.
 * 
 * aSlot              - [IN] �Ҵ� �����Ǵ� DataFileDescSlot
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::deallocDataFileDescSlot( 
                                        smriCTDataFileDescSlot  * aSlot )
{
    smriCTDataFileDescBlock * sDataFileDescBlock;
    UInt                      sAllocSlotFlag = 0x1;

    IDE_DASSERT( aSlot != NULL );

    getBlock( aSlot->mSlotID.mBlockID, (void**)&sDataFileDescBlock ); 

    IDE_ERROR( aSlot->mSlotID.mBlockID == 
               sDataFileDescBlock->mBlockHdr.mBlockID );
    
    aSlot->mBmpExtCnt           = 0;
    aSlot->mSpaceID             = SC_NULL_SPACEID;
    aSlot->mFileID              = 0;
    aSlot->mPageSize            = 0;
    aSlot->mTBSType             = SMRI_CT_NONE;
    aSlot->mTrackingState       = SMRI_CT_TRACKING_DEACTIVE;
    aSlot->mCurTrackingListID   = 0;

    IDE_ERROR( ( sDataFileDescBlock->mAllocSlotFlag & 
               ( sAllocSlotFlag << aSlot->mSlotID.mSlotIdx ) ) != 0 );

    sDataFileDescBlock->mAllocSlotFlag &=
                             ~(sAllocSlotFlag << aSlot->mSlotID.mSlotIdx );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * differential bakcup���� ������ �������� BmpExtList�� switch�Ѵ�.
 * �Լ� ���н� changeTracking���� invaild
 * aSlot  - [IN] backup�� �����Ϸ��� datafile�� slot
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::switchBmpExtListID( 
                                        smriCTDataFileDescSlot * aSlot )
{
    UInt    sState = 0;
    UShort  sLockListID;

    IDE_DASSERT( aSlot != NULL );

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );

    sLockListID = aSlot->mCurTrackingListID;

    IDE_TEST( lockBmpExtHdrLatchX( aSlot, sLockListID ) != IDE_SUCCESS );

    aSlot->mCurTrackingListID = ( aSlot->mCurTrackingListID + 1 ) % 2;

    IDE_TEST( unlockBmpExtHdrLatchX( aSlot, sLockListID ) != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * DataFile Desc slot�� �Ҵ�� ��� BmpExtHdr�� latch�� ȹ���Ѵ�.
 * 
 * aSlot  - [IN] backup�� �����Ϸ��� datafile�� slot
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::lockBmpExtHdrLatchX( 
                                    smriCTDataFileDescSlot * aSlot,
                                    UShort                   aLockListID )
{
    UInt                    sFirstBmpExtHdrBlockID;
    UInt                    sBmpExtHdrBlockID;
    smriCTBmpExtHdrBlock  * sBmpExtHdrBlock;
    UInt                    i = 0;
    UInt                    sBmpExtCnt = 0;

    IDE_DASSERT( aSlot != NULL );

    sFirstBmpExtHdrBlockID = 
                aSlot->mBmpExtList[ aLockListID ].mBmpExtListHint[0];

    IDE_ERROR( sFirstBmpExtHdrBlockID != SMRI_CT_INVALID_BLOCK_ID );

    sBmpExtHdrBlockID = sFirstBmpExtHdrBlockID;

    do
    {
        getBlock( sBmpExtHdrBlockID, 
                            (void**)&sBmpExtHdrBlock );

        IDE_TEST( sBmpExtHdrBlock->mLatch.lockWrite( NULL,  //aStatistics
                                       NULL ) //aWeArgs
                  != IDE_SUCCESS );

        sBmpExtCnt++;

        sBmpExtHdrBlockID = sBmpExtHdrBlock->mNextBmpExtHdrBlockID;

    }while( sFirstBmpExtHdrBlockID != sBmpExtHdrBlockID );

    IDE_ERROR( sBmpExtCnt == ( aSlot->mBmpExtCnt 
                             / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    sBmpExtHdrBlockID = sFirstBmpExtHdrBlockID;

    for( i = 0 ; i < sBmpExtCnt; i++ )
    {
        getBlock( sBmpExtHdrBlockID, 
                  (void**)&sBmpExtHdrBlock );

        IDE_ASSERT( sBmpExtHdrBlock->mLatch.unlock( ) 
                    == IDE_SUCCESS );
        
        sBmpExtHdrBlockID = sBmpExtHdrBlock->mNextBmpExtHdrBlockID;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * DataFile Desc slot�� �Ҵ�� ��� BmpExtHdr�� latch�� �����Ѵ�.
 * 
 * aSlot  - [IN] backup�� �����Ϸ��� datafile�� slot
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::unlockBmpExtHdrLatchX( 
                                        smriCTDataFileDescSlot * aSlot,
                                        UShort                   aLockListID )
{
    UInt                    sFirstBmpExtHdrBlockID;
    UInt                    sBmpExtHdrBlockID;
    smriCTBmpExtHdrBlock  * sBmpExtHdrBlock;
    UInt                    sBmpExtCnt = 0;

    IDE_DASSERT( aSlot != NULL );

    sFirstBmpExtHdrBlockID = 
                aSlot->mBmpExtList[ aLockListID ].mBmpExtListHint[0];

    IDE_ERROR( sFirstBmpExtHdrBlockID != SMRI_CT_INVALID_BLOCK_ID );

    sBmpExtHdrBlockID = sFirstBmpExtHdrBlockID;

    do
    {
        getBlock( sBmpExtHdrBlockID, 
                            (void**)&sBmpExtHdrBlock );

        IDE_TEST( sBmpExtHdrBlock->mLatch.unlock( )
                  != IDE_SUCCESS );

        sBmpExtCnt++;

        sBmpExtHdrBlockID = sBmpExtHdrBlock->mNextBmpExtHdrBlockID;

    }while( sFirstBmpExtHdrBlockID != sBmpExtHdrBlockID );

    IDE_ERROR( sBmpExtCnt == ( aSlot->mBmpExtCnt 
                                 / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sBmpExtCnt != 0 )
    {
        for( ; 
             sBmpExtCnt < ( aSlot->mBmpExtCnt 
                            / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT ); 
             sBmpExtCnt++ )
        {
            getBlock( sBmpExtHdrBlock->mNextBmpExtHdrBlockID, 
                      (void**)&sBmpExtHdrBlock );
 
            IDE_ASSERT( sBmpExtHdrBlock->mLatch.unlock(  ) 
                        == IDE_SUCCESS );
        }
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Write Page �� ������ ����Ѵ�.
 * 
 * aDataFileNode - [IN] �ش� DataFile
 * aPageID       - [IN] �ش� PageID
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::changeTracking4WriteDiskPage( sddDataFileNode * aDataFileNode,
                                                            scPageID          aPageID )
{
    UInt  sIBChunkID;
    
    (void)idCore::acpAtomicInc32( &mCurrChangeTrackingThreadCnt );

    sIBChunkID = calcIBChunkID4DiskPage( aPageID );

    IDE_TEST( changeTracking( aDataFileNode,
                              NULL,
                              sIBChunkID ) 
              != IDE_SUCCESS );

    (void)idCore::acpAtomicDec32( &mCurrChangeTrackingThreadCnt );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
/***********************************************************************
 * IBChunk��ȭ�� �����Ѵ�.
 * �Լ� ���н� change tracking���� invalid
 * sddDataFileNode  - [IN] �����Ϸ��� datafile�� node
 * aDatabaseFile    - [IN] �����Ϸ��� database file
 * aIBChunkID       - [IN] �����Ϸ��� IBChunkID
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::changeTracking( 
                                  sddDataFileNode * aDataFileNode,
                                  smmDatabaseFile * aDatabaseFile,
                                  UInt              aIBChunkID )
{
    smriCTDataFileDescSlot    * sSlot;
    smiDataFileDescSlotID     * sDataFileDescSlotID;
    smriCTBmpExt              * sCurBmpExt;
    smriCTState                 sTrackingState;
    smmChkptImageHdr            sChkptImageHdr;
    scSpaceID                   sSpaceID;
    UInt                        sFileID;
    UInt                        sBeforeTrackingListID;
    UInt                        sAfterTrackingListID;
    UInt                        sState = 0;

    /* �޸�/��ũ ������������ �����ϰ� �ʿ��� ������ ��´� */
    if( aDataFileNode != NULL )
    {
        IDE_ERROR( aDatabaseFile == NULL );

        sDataFileDescSlotID = &aDataFileNode->mDBFileHdr.mDataFileDescSlotID;
        sSpaceID            = aDataFileNode->mSpaceID;
        sFileID             = (UInt)(aDataFileNode->mID);
    }
    else
    {
        IDE_ERROR( aDataFileNode == NULL );
        IDE_ERROR( aDatabaseFile != NULL );

        aDatabaseFile->getChkptImageHdr( &sChkptImageHdr );
        sDataFileDescSlotID = &sChkptImageHdr.mDataFileDescSlotID;
        sFileID             = aDatabaseFile->getFileNum();
        sSpaceID            = aDatabaseFile->getSpaceID();
    }
    
    IDE_ERROR_RAISE( (sDataFileDescSlotID->mBlockID 
                                     != SMRI_CT_INVALID_BLOCK_ID) ||
                     (sDataFileDescSlotID->mSlotIdx 
                                     != SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX),
                    error_invalid_change_tracking_file );

    while(1)
    {
        /* DataFileDescSlot�� �����´�. */
        getDataFileDescSlot( sDataFileDescSlotID, &sSlot );
 
        IDE_ERROR( (sSpaceID == sSlot->mSpaceID) && (sFileID == sSlot->mFileID) );
 
        /* DataFileDescSlot�� change tracking�� �Ҽ� �ִ� �������� Ȯ���Ѵ�. */
        sTrackingState = 
                    (smriCTState)idCore::acpAtomicGet32( &sSlot->mTrackingState );
 
        IDE_TEST_CONT( sTrackingState != SMRI_CT_TRACKING_ACTIVE, 
                        skip_change_tracking );

        sBeforeTrackingListID = sSlot->mCurTrackingListID;

 
        /* ���޵� IBChunkID�� �ش��ϴ� BmpExt�� �����´� */
        IDE_TEST( getCurBmpExt( sSlot, aIBChunkID, &sCurBmpExt ) 
                  != IDE_SUCCESS );
 
        /* 
         * change tracking�� bitmap�� switch�Ǵ� ����
         * �����ϱ����� ������ BmpExt�� read lock�� �Ǵ�.
         */
        IDE_TEST( sCurBmpExt->mBmpExtHdrBlock.mLatch.lockRead( NULL,  //aStatistics
                                      NULL ) //aWeArgs
                  != IDE_SUCCESS );
        sState = 1;
 
        /* 
         * readlock�� �ɱ����� bitmap switch�� �Ǿ����� Ȯ���ϱ�����
         * �ٽ� DataFileDescSlot�� �����´�.
         */
        getDataFileDescSlot( sDataFileDescSlotID, &sSlot ); 
 
        IDE_ERROR( (sSpaceID == sSlot->mSpaceID) && (sFileID == sSlot->mFileID) );
 
        sAfterTrackingListID = sSlot->mCurTrackingListID;
 
        /* BmpExt�� readlock�� ������� bitmap switch�� �Ǿ����� Ȯ�� */
        if( sBeforeTrackingListID != sAfterTrackingListID )
        {
            sState = 0;
            IDE_TEST( sCurBmpExt->mBmpExtHdrBlock.mLatch.unlock( ) 
                      != IDE_SUCCESS );
        }
        else
        {
            /* bitmap�� swit���� �ʾҴ�. ����ε� CurBmpExt�� ������ ���� */
            break;
        }
    }

    /* bit�� set�Ѵ�. */
    IDE_TEST( calcBitPositionAndSet( sCurBmpExt, aIBChunkID, sSlot ) != IDE_SUCCESS );
    
    sState = 0;
    IDE_TEST( sCurBmpExt->mBmpExtHdrBlock.mLatch.unlock(  ) 
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_change_tracking );


    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidChangeTrackingFile, 
                mFile.getFileName()));
    }
    IDE_EXCEPTION_END;
    
    if( sState == 1 )
    {
        IDE_ASSERT( sCurBmpExt->mBmpExtHdrBlock.mLatch.unlock(  ) 
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * IBChunkID�� ���ϴ� ���� �������� differential BmpExt�� �����´�.
 * 
 * aSlot        - [IN] �����Ϸ��� datafile�� DataFileDesc slot
 * aIBChunkID   - [IN] �����Ϸ��� IBChunkID
 * aCurBmpExt   - [OUT] IBChunkID�� ���ϴ� BmpExt
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::getCurBmpExt(       
                        smriCTDataFileDescSlot   * aSlot,
                        UInt                       aIBChunkID,
                        smriCTBmpExt            ** aCurBmpExt )
{
    UInt                sBmpExtSeq;
    smriCTBmpExt      * sCurBmpExt;
    smriCTBmpExt      * sAllocCurBmpExt;
    UInt                sState = 0;

    IDE_DASSERT( aSlot       != NULL );
    IDE_DASSERT( aCurBmpExt  != NULL );

    /* IBChunkID�� ���� BmpExt�� List���� ���° BmpExt���� ���Ѵ�. */
    sBmpExtSeq = aIBChunkID / mBmpExtBitCnt;


    /* BmpExtSeq�� �ش��ϴ� BmpExt�� �����´�. */
    IDE_TEST( getCurBmpExtInternal( aSlot, sBmpExtSeq, &sCurBmpExt ) 
              != IDE_SUCCESS );

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );
    sState = 1;

    while(1)
    {
        if( sCurBmpExt == NULL )
        {
            IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );
 
            /* 
             * CTHdr�� mutext�� ���� �Ŀ� �ٸ������忡���� BmpExt�� ���� �Ҵ�
             * �Ǿ����� Ȯ���Ѵ�.
             */
             IDE_TEST( getCurBmpExtInternal( aSlot, sBmpExtSeq, &sCurBmpExt ) 
                       != IDE_SUCCESS );
 
            if( sCurBmpExt != NULL )
            {
                /* IBChunkID�� ���� BmpExt�� ã����� */
            }
            else
            {
                /* 
                 * IBChunkID�� ���� BmpExt�� �� ã����� DataFileDescSlot�� ���ο�
                 * BmpExt�� �Ҵ��Ѵ�.
                 */
                IDE_TEST( addBmpExt2DataFileDescSlot( aSlot, 
                                                      &sAllocCurBmpExt ) 
                          != IDE_SUCCESS );
 
                sCurBmpExt = sAllocCurBmpExt;

                if( sBmpExtSeq == sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq )
                {
                    /* ����ε� BmpExtSeq�� ���� BmpExt�� �Ҵ� ���� ��� */
                    break;
                }
                else
                {
                    if( sBmpExtSeq > sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq )
                    {
                        /* BmpExt�� �Ҵ�޾����� IBCunkID�� ���� BmpExt�� �Ҵ���� ���Ѱ��.
                         * �������� BmpExt�� �Ҵ��ؾ��ϴ°�� �ٽ� ó������ ���ư�
                         * BmpExt�� �Ҵ�޴´�.
                         */
                        sCurBmpExt = NULL;
                    }
                    else
                    {
                        /* sBmpExtSeq���� �����Ҵ���� BmpExt�� mBmpExtSeq�� Ŭ �� ����. */
                        IDE_ERROR(0);
                    }
                }
            }
        }
        else
        {
            break;
        }
    }
 
    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    IDE_DASSERT( sBmpExtSeq == sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq );
    
    *aCurBmpExt = sCurBmpExt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 1:
            IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
        default:
            break;
    }   
    
    return IDE_FAILURE;
}

/***********************************************************************
 * aBmpExtSeq�� �ش��ϴ� ���� �������� differential BmpExt�� �����´�.
 * 
 * aSlot        - [IN] �����Ϸ��� datafile�� DataFileDesc slot
 * aBmpExtSeq   - [IN] ã������ BmpExt�� sequence ��ȣ
 * aCurBmpExt   - [OUT] aBmpExtSeq�� �ش��ϴ� BmpExt
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::getCurBmpExtInternal( 
                                        smriCTDataFileDescSlot   * aSlot,
                                        UInt                       aBmpExtSeq,
                                        smriCTBmpExt            ** aCurBmpExt )
{
    UInt            sBmpExtBlockID;
    UInt            sFirstBmpExtBlockID;
    smriCTBmpExt  * sCurBmpExt;

    IDE_DASSERT( aSlot       != NULL );
    IDE_DASSERT( aCurBmpExt  != NULL );

    /* 
     * IBChunkID�� ���� BmpExt�� SMRI_CT_BMP_EXT_LIST_HINT_CNT�̳��� �ִٸ�
     * mBmpExtListHint���� �ٷ� �����´�. �׷��� ������� BmpExtList�� Ž����
     * ã�´�.
     */
    if( aBmpExtSeq < SMRI_CT_BMP_EXT_LIST_HINT_CNT )
    {
        sBmpExtBlockID = 
                    aSlot->mBmpExtList[aSlot->mCurTrackingListID]
                                .mBmpExtListHint[aBmpExtSeq];

        if( sBmpExtBlockID == SMRI_CT_INVALID_BLOCK_ID )
        {
            sCurBmpExt = NULL;
        }
        else
        {
            getBlock( sBmpExtBlockID, (void**)&sCurBmpExt );
            
            IDE_ERROR( sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq == aBmpExtSeq );
        }
    }
    else
    {
        sFirstBmpExtBlockID = 
        aSlot->mBmpExtList[ aSlot->mCurTrackingListID ].mBmpExtListHint[0];

        IDE_ERROR( sFirstBmpExtBlockID != SMRI_CT_INVALID_BLOCK_ID );

        getBlock( sFirstBmpExtBlockID, (void**)&sCurBmpExt );
        
        while(1)
        {
            if( aBmpExtSeq == sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq )
            {
                break;
            }

            getBlock( sCurBmpExt->mBmpExtHdrBlock.mNextBmpExtHdrBlockID,
                      (void**)&sCurBmpExt );

            if( sCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq == 0 )
            {
                sCurBmpExt = NULL;
                break;
            }
        }
    }

    *aCurBmpExt = sCurBmpExt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * IBChunkID�� �ش��ϴ� bit�� ��ġ�� ���ϰ� bit�� set�Ѵ�.
 * 
 * aCurBmpExt   - [IN] bit�� set�� BmpExt
 * aIBChunkID   - [IN] bit�� ��ġ�� ���� IBChunkID
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::calcBitPositionAndSet( 
                                            smriCTBmpExt           * aCurBmpExt,
                                            UInt                     aIBChunkID,
                                            smriCTDataFileDescSlot * aSlot )
{
    smriCTBmpExt  * sCumBmpExt;
    UInt            sRelativeIBChunkID;
    UInt            sBmpBlockIdx;
    UInt            sBitOffset;
    UInt            sByteIdx;
    UInt            sBitPosition;

    IDE_DASSERT( aCurBmpExt != NULL );

    sRelativeIBChunkID  = aIBChunkID 
                          - ( mBmpExtBitCnt 
                          * aCurBmpExt->mBmpExtHdrBlock.mBmpExtSeq );

    sBmpBlockIdx        = sRelativeIBChunkID / mBmpBlockBitCnt;

    sBitOffset          = sRelativeIBChunkID 
                          - ( mBmpBlockBitCnt
                          * sBmpBlockIdx );
    
    sByteIdx        = sBitOffset / 8;
    sBitPosition    = sBitOffset % 8;

    IDE_ERROR( sBitOffset < mBmpBlockBitCnt );
    
    /* Differential Bitmap */
    IDE_TEST( setBit( aCurBmpExt, 
                      sBmpBlockIdx, 
                      sByteIdx, 
                      sBitPosition,
                      &aSlot->mBmpExtList[ aSlot->mCurTrackingListID ] )
              != IDE_SUCCESS ); 

    getBlock( aCurBmpExt->mBmpExtHdrBlock.mCumBmpExtHdrBlockID, 
                        (void**)&sCumBmpExt );

    IDU_FIT_POINT("smriChangeTrackingMgr::calcBitPositionAndSet::wait");

    /* Cumulative Bitmap */
    IDE_TEST( setBit( sCumBmpExt, 
                      sBmpBlockIdx, 
                      sByteIdx, 
                      sBitPosition,
                      &aSlot->mBmpExtList[ SMRI_CT_CUMULATIVE_LIST ] ) 
              != IDE_SUCCESS ); 

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * IBChunk�� bit�� set�Ѵ�.
 * 
 * aBmpExt          - [IN] bit�� set�� BmpExt
 * aBmpBlockIdx     - [IN] bit�� ���� BmpBlock�� Idx
 * aByteIdx         - [IN] bit�� ���� byte�� Idx
 * aBitPosition     - [IN] byte�������� bit��ġ
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::setBit( smriCTBmpExt     * aBmpExt,
                                      UInt               aBmpBlockIdx,
                                      UInt               aByteIdx,
                                      UInt               aBitPosition,
                                      smriCTBmpExtList * aBmpExtList )
{
    smriCTBmpBlock        * sBmpBlock;
    SChar                 * sTargetByte;
    SChar                   sTempByte = 0;
    UInt                    sState    = 0;

    IDE_DASSERT( aBmpExt != NULL );

    sBmpBlock       = &aBmpExt->mBmpBlock[ aBmpBlockIdx ];
    sTargetByte     = &sBmpBlock->mBitmap[ aByteIdx ];

    sTempByte |= (1 << aBitPosition);

    /* bit�� set�Ǿ����� ������쿡�� bit�� set�Ѵ�. */
    if( ( *sTargetByte & sTempByte ) == 0 )
    {
        IDE_TEST( sBmpBlock->mBlockHdr.mMutex.lock( NULL ) != IDE_SUCCESS );
        sState = 1;

        *sTargetByte |= sTempByte;
        (void)idCore::acpAtomicInc32( &aBmpExtList->mSetBitCount );

        sState = 0;
        IDE_TEST( sBmpBlock->mBlockHdr.mMutex.unlock() != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sBmpBlock->mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }
    
    return IDE_FAILURE;
}


/*********************************************************************** 
 * Bimap�� ��ĵ�ϰ� IBchunk�� �����Ѵ�.
 *
 * aDataFielDescSlot    - [IN] bitmap�� �˻��� DataFileDescSlot
 * aBackupInfo          - [IN] � backup�� �������������� ����
 * aSrcFile             - [IN] backup�� ������ ���� ����������
 * aDestFile            - [IN] ������ backup����
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::makeLevel1BackupFile( 
                              smriCTDataFileDescSlot * aDataFileDescSlot, 
                              smriBISlot             * aBackupInfo,
                              iduFile                * aSrcFile,
                              iduFile                * aDestFile,
                              smriCTTBSType            aTBSType,
                              scSpaceID                aSpaceID,
                              UInt                     aFileNo )
{
    smriCTBmpExt  * sBmpExt;
    UInt            sIBChunkSize;
    UInt            sBmpExtListLen;
    UInt            sBmpExtHdrBlockID;
    UInt            sFirstBmpExtHdrBlockID;
    UInt            sListID;
    smmTBSNode    * sTBSNode;
    UInt            sBmpExtCnt = 0;
    UInt            sState = 0;

    IDE_DASSERT( aDataFileDescSlot     != NULL );
    IDE_DASSERT( aBackupInfo           != NULL );
    IDE_DASSERT( aSrcFile              != NULL );
    IDE_DASSERT( aDestFile             != NULL );

    /* 
     * Differetion backup���� cumulative backup���� �Ǵ��Ͽ� �ش��ϴ� BmpExt��
     * �����´�.
     */
    if( (aBackupInfo->mBackupType & SMI_BACKUP_TYPE_DIFFERENTIAL) != 0 )
    {
        IDE_ERROR( (aBackupInfo->mBackupType & SMI_BACKUP_TYPE_CUMULATIVE) 
                    == 0 );

        getFirstDifBmpExt( aDataFileDescSlot, &sFirstBmpExtHdrBlockID );

        sListID = ( aDataFileDescSlot->mCurTrackingListID + 1 ) % 2;
    }
    else
    {
        IDE_ERROR( (aBackupInfo->mBackupType & SMI_BACKUP_TYPE_DIFFERENTIAL) 
                    == 0 );

        IDE_ERROR( (aBackupInfo->mBackupType & SMI_BACKUP_TYPE_CUMULATIVE) 
                    != 0 );

        getFirstCumBmpExt( aDataFileDescSlot, &sFirstBmpExtHdrBlockID );
        sListID = SMRI_CT_CUMULATIVE_LIST; 
    }

    IDE_ERROR( sFirstBmpExtHdrBlockID != SMRI_CT_INVALID_BLOCK_ID );

    getIBChunkSize( &sIBChunkSize );
    mSrcFile        = aSrcFile; 
    mDestFile       = aDestFile; 
    mCopySize       = aDataFileDescSlot->mPageSize * sIBChunkSize;
    mTBSType        = aDataFileDescSlot->mTBSType;
    mIBChunkCnt     = 0;
    mIBChunkID      = 0;

    mFileNo         = aFileNo;

    IDE_ERROR( mTBSType == aTBSType );

    /* MEM TBS�� ��� split�� �ι�° ������ pageid�� ���ϱ� ����
     * mSplitFilePageCount���� ���Ѵ�. */
    if( aTBSType == SMRI_CT_MEM_TBS )
    {
        IDE_ERROR( sctTableSpaceMgr::isMemTableSpace( aSpaceID ) == ID_TRUE );
        IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID , (void**)&sTBSNode )
                  != IDE_SUCCESS );
        mSplitFilePageCount = sTBSNode->mTBSAttr.mMemAttr.mSplitFilePageCount;
    }
    else
    {
        mSplitFilePageCount = 0;
    }


    /* IBChunk�� ������ ���۸� �Ҵ� �޴´�. */
    /* smriChangeTrackingMgr_makeLevel1BackupFile_calloc_IBChunkBuffer.tc */
    IDU_FIT_POINT("smriChangeTrackingMgr::makeLevel1BackupFile::calloc::IBChunkBuffer");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMR,
                                 1,
                                 mCopySize,
                                 (void**)&mIBChunkBuffer )
              != IDE_SUCCESS );              
    sState = 1;

    IDE_ERROR( aDataFileDescSlot->mBmpExtCnt % 
                        SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT == 0 );
    
    /* BmpExtList�� ���̸� ���Ѵ� */
    sBmpExtListLen = aDataFileDescSlot->mBmpExtCnt 
                     / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT;

    sBmpExtHdrBlockID = sFirstBmpExtHdrBlockID;

    for( sBmpExtCnt = 0; sBmpExtCnt < sBmpExtListLen; sBmpExtCnt++ )
    {
        getBlock( sBmpExtHdrBlockID, (void **)&sBmpExt ); 

        /* �� BmpExt�� ���� bitmap�� �˻��Ѵ�. */
        IDE_TEST( makeLevel1BackupFilePerEachBmpExt( 
                                    sBmpExt, 
                                    sBmpExt->mBmpExtHdrBlock.mBmpExtSeq )
                  != IDE_SUCCESS ); 

        sBmpExtHdrBlockID = sBmpExt->mBmpExtHdrBlock.mNextBmpExtHdrBlockID;
    }

    IDE_ERROR( sBmpExtHdrBlockID == sFirstBmpExtHdrBlockID );
    
    if( mIBChunkCnt != aDataFileDescSlot->mBmpExtList[ sListID ].mSetBitCount )
    {
        ideLog::log( IDE_DUMP_0,
                "==================================================================\n"
                " smriChangeTrackingMgr::makeLevel1BackupFile\n"
                "==================================================================\n"
                "IBChunkCnt             : %u\n"
                "SetBitCount            : %u\n"
                "ListID                 : %u\n"
                "==================================================================\n",
                mIBChunkCnt,
                aDataFileDescSlot->mBmpExtList[ sListID ].mSetBitCount,
                sListID );

        IDE_TEST(1);
    }

    IDE_TEST( mDestFile->syncUntilSuccess( smLayerCallback::setEmergency )
              != IDE_SUCCESS );
    aBackupInfo->mIBChunkCNT  = mIBChunkCnt;
    aBackupInfo->mIBChunkSize = sIBChunkSize;

    sState = 0;
    IDE_TEST( iduMemMgr::free( mIBChunkBuffer ) != IDE_SUCCESS );              

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    if( sState == 1 )
    {
        IDE_ASSERT( iduMemMgr::free( mIBChunkBuffer ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * �� BmpExt�� bit�� Ȯ���Ѵ�.
 *
 * aBmpExt      - [IN] �˻��� BmpExt
 * aBmpExtSeq   - [IN] BmpExtList������ BmpExt�� ��ġ
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::makeLevel1BackupFilePerEachBmpExt( 
                                                     smriCTBmpExt * aBmpExt, 
                                                     UInt           aBmpExtSeq )
{
    UInt                sBmpBlockIdx;
    smriCTBmpBlock    * sBmpBlock;

    IDE_DASSERT( aBmpExt     != NULL );

    for( sBmpBlockIdx = 0; 
         sBmpBlockIdx < SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK; 
         sBmpBlockIdx++ )
    {
        sBmpBlock = &aBmpExt->mBmpBlock[ sBmpBlockIdx ];
        /* �� BmpExtBlock�� �˻��Ѵ�. */
        IDE_TEST( makeLevel1BackupFilePerEachBmpBlock( sBmpBlock, 
                                                       aBmpExtSeq, 
                                                       sBmpBlockIdx )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * �� BmpBlock�� bit�� Ȯ���Ѵ�.
 *
 * aBmpBlock    - [IN] �˻��� BmpBlock
 * aBmpExtSeq   - [IN] BmpExtList������ BmpExt�� ��ġ
 * aBmpBlockIdx - [IN] BmpExt������ BmpBlock�� ��ġ
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::makeLevel1BackupFilePerEachBmpBlock(
                                         smriCTBmpBlock   * aBmpBlock, 
                                         UInt               aBmpExtSeq, 
                                         UInt               aBmpBlockIdx )
{
    UInt        sBmpByteIdx;
    SChar     * sBmpByte;

    IDE_DASSERT( aBmpBlock != NULL );

    for( sBmpByteIdx = 0; 
         sBmpByteIdx < mBmpBlockBitmapSize;
         sBmpByteIdx++ )
    {
        sBmpByte = &aBmpBlock->mBitmap[ sBmpByteIdx ];

        /* �� BmpByte�� �˻��Ѵ�. */
        IDE_TEST( makeLevel1BackupFilePerEachBmpByte( sBmpByte, 
                                                      aBmpExtSeq, 
                                                      aBmpBlockIdx, 
                                                      sBmpByteIdx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************** 
 * �� BmpByte�� bit�� Ȯ���ϰ� bit�� �ش��ϴ� IBChunk�� �����Ѵ�.
 *
 * aBmpBlock    - [IN] �˻��� BmpBlock
 * aBmpExtSeq   - [IN] BmpExtList������ BmpExt�� ��ġ
 * aBmpBlockIdx - [IN] BmpExt������ BmpBlock�� ��ġ
 * aBmpByteIdx  - [IN] BmpBlock������ BmpByte����ġ
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::makeLevel1BackupFilePerEachBmpByte( 
                                                      SChar * aBmpByte,
                                                      UInt    aBmpExtSeq,
                                                      UInt    aBmpBlockIdx,
                                                      UInt    aBmpByteIdx )
{
    UInt        sBmpBitIdx;
    UInt        sTempByte;
    UInt        sIBChunkID;

    IDE_DASSERT( aBmpByte    != NULL );

    for( sBmpBitIdx = 0; sBmpBitIdx < SMRI_CT_BIT_CNT ; sBmpBitIdx++ )
    {
        sTempByte = 0;
        sTempByte = ( 1 << sBmpBitIdx );
        
        /* BmpByte���� bit�� 1�� set�� ��ġ�� ã�´�. */
        if( ( *aBmpByte & sTempByte ) != 0 )
        {
            sIBChunkID = ( aBmpExtSeq * mBmpExtBitCnt )
                         + ( aBmpBlockIdx * mBmpBlockBitCnt )
                         + ( aBmpByteIdx * SMRI_CT_BIT_CNT )
                         + sBmpBitIdx;

            IDE_ASSERT( sIBChunkID == mIBChunkID );

            /* 1�� set�� IBChunk�� backup���Ϸ� ���� �Ѵ�. */
            IDE_TEST( copyIBChunk( sIBChunkID ) != IDE_SUCCESS );

            mIBChunkCnt++;
        }
        mIBChunkID++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************** 
 * IBChunk�� ����Ѵ�.
 * PROJ-2133 
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::copyIBChunk( UInt aIBChunkID )
{
    size_t              sReadOffset;
    size_t              sWriteOffset;
    ULong               sDestFileSize = 0;
    size_t              sReadSize;
    UInt                sIBChunkSize;
    UInt                i;
    UInt                sValidationPageCnt;
    sdpPhyPageHdr     * sDiskPageHdr;
    sdpPhyPageHdr     * sDiskPageHdr4Validation;
    smpPersPageHeader * sMemPageHdr;
    smpPersPageHeader * sMemPageHdr4Validation;
    scPageID            sPageIDFromIBChunkID;
    scPageID            sPageIDFromReadOffset;
    idBool              sIsValid = ID_FALSE;

    /* �������Ͽ��� IBChunk�� �о�� ��ġ�� ���Ѵ�. */
    sReadOffset = SM_DBFILE_METAHDR_PAGE_SIZE;
    sReadOffset += aIBChunkID * mCopySize;

    /* ������Ͽ��� IBChunk�� �� ��ġ�� ���Ѵ�. append olny */
    IDE_TEST( mDestFile->getFileSize( &sDestFileSize ) != IDE_SUCCESS );

    if( sDestFileSize <= SM_DBFILE_METAHDR_PAGE_SIZE )
    {
        sWriteOffset = SM_DBFILE_METAHDR_PAGE_SIZE;
    }
    else
    {
        sWriteOffset = sDestFileSize;
    }

    IDE_TEST( mSrcFile->read( NULL,  //aStatistics
                              sReadOffset, 
                              (void*)mIBChunkBuffer, 
                              mCopySize, 
                              &sReadSize ) 
              != IDE_SUCCESS );

    IDE_ERROR( sReadSize != 0 );

    getIBChunkSize( &sIBChunkSize );

    if( mTBSType == SMRI_CT_DISK_TBS )
    {
        IDE_ASSERT( (sReadSize % SD_PAGE_SIZE) == 0 );

        sDiskPageHdr = ( sdpPhyPageHdr * )mIBChunkBuffer;

        sPageIDFromIBChunkID = aIBChunkID * sIBChunkSize;

        sValidationPageCnt = sReadSize / SD_PAGE_SIZE;
        for( i = 0; i < sValidationPageCnt; i++ )
        {
            sDiskPageHdr4Validation = (sdpPhyPageHdr *)&mIBChunkBuffer[i * SD_PAGE_SIZE];

            if( SD_MAKE_FPID(sDiskPageHdr4Validation->mPageID) == sPageIDFromIBChunkID + i )
            {
                sIsValid = ID_TRUE;
            }
            else
            {
                IDE_ERROR( sDiskPageHdr4Validation->mPageType == SDP_PAGE_UNFORMAT );

                /* unformat page���� pageid�� ���õǾ� ���� �ʴ�. ����Ͽ� ����
                 * pageid�� �ִ´�.*/
                sDiskPageHdr4Validation->mPageID = sPageIDFromIBChunkID + i;
            }
        }

        sPageIDFromReadOffset = (sReadOffset - SM_DBFILE_METAHDR_PAGE_SIZE)
                                / SD_PAGE_SIZE ;

        IDE_DASSERT( sPageIDFromReadOffset == sPageIDFromIBChunkID );
        IDE_ERROR( (sIsValid == ID_TRUE) || (sPageIDFromReadOffset == sPageIDFromIBChunkID) );

        if( SD_MAKE_FPID(sDiskPageHdr->mPageID) != sPageIDFromIBChunkID )
        {
            ideLog::log( IDE_DUMP_0,
                         "==================================================================\n"
                         " Storage Manager Dump Info for Copy Incremental Backup Chunk(Disk)\n"
                         "==================================================================\n"
                         "pageType                : %u\n"
                         "pageID                  : %u\n"
                         "IBChunkID * IBChunkSize : %u\n"
                         "srcFileName             : %s\n"
                         "destFileName            : %s\n"
                         "==================================================================\n",
                         sDiskPageHdr->mPageType,
                         SD_MAKE_FPID(sDiskPageHdr->mPageID),
                         (ULong)aIBChunkID * sIBChunkSize,
                         mSrcFile->getFileName(),
                         mDestFile->getFileName());
        
            IDE_ASSERT(0)
        }
    }
    else
    {
        if( mTBSType == SMRI_CT_MEM_TBS )
        {
            IDE_ASSERT( (sReadSize % SM_PAGE_SIZE) == 0 );

            sMemPageHdr = (smpPersPageHeader *)mIBChunkBuffer;

            if( mFileNo == 0)
            {
                sPageIDFromIBChunkID = aIBChunkID * sIBChunkSize;

                sPageIDFromReadOffset = (sReadOffset - SM_DBFILE_METAHDR_PAGE_SIZE)
                                        / SM_PAGE_SIZE ;
            }
            else
            {
                /* chkpt�� �ι�° ���Ϻ��ʹ� ù��° pageid�� 0�� �ƴ�
                 * mSplitFilePageCount + 1�� ���� ������. */
                sPageIDFromIBChunkID = (aIBChunkID * sIBChunkSize)
                                       + ( mFileNo * mSplitFilePageCount )
                                       + 1;

                sPageIDFromReadOffset = ((sReadOffset - SM_DBFILE_METAHDR_PAGE_SIZE)
                                        / SM_PAGE_SIZE)
                                        + ( mFileNo * mSplitFilePageCount )
                                        + 1;
            }

            IDE_DASSERT( sPageIDFromReadOffset == sPageIDFromIBChunkID );

            sValidationPageCnt = sReadSize / SM_PAGE_SIZE;
            for( i = 0; i < sValidationPageCnt; i ++ )
            {
                sMemPageHdr4Validation = (smpPersPageHeader *)&mIBChunkBuffer[i * SM_PAGE_SIZE];

                if( sMemPageHdr4Validation->mSelfPageID == sPageIDFromIBChunkID + i )
                {
                    sIsValid = ID_TRUE;
                }
                else
                {
                    IDE_ERROR( ( SMP_GET_PERS_PAGE_TYPE( sMemPageHdr4Validation) != SMP_PAGETYPE_FIX ) &&
                               ( SMP_GET_PERS_PAGE_TYPE( sMemPageHdr4Validation) != SMP_PAGETYPE_VAR ) );

                    /* unformat page���� pageid�� ���õǾ� ���� �ʴ�. ����Ͽ� ����
                     * pageid�� �ִ´�.*/
                    sMemPageHdr4Validation->mSelfPageID = sPageIDFromIBChunkID + i;
                }
            }

            IDE_ERROR( (sIsValid == ID_TRUE) || (sPageIDFromReadOffset == sPageIDFromIBChunkID) );

            if( sMemPageHdr->mSelfPageID != sPageIDFromIBChunkID )
            {
                ideLog::log( IDE_DUMP_0,
                             "====================================================================\n"
                             " Storage Manager Dump Info for Copy Incremental Backup Chunk(Memory)\n"
                             "====================================================================\n"
                             "pageID                  : %u\n"
                             "IBChunkID * IBChunkSize : %u\n"
                             "srcFileName             : %s\n"
                             "destFileName            : %s\n"
                             "====================================================================\n",
                             sMemPageHdr->mSelfPageID,
                             (ULong)aIBChunkID * sIBChunkSize,
                             mSrcFile->getFileName(),
                             mDestFile->getFileName());
            
                IDE_ASSERT(0)
            }
        }
        else
        {
            IDE_ASSERT(0);
        }
    }

    IDE_TEST( mDestFile->write( NULL, //aStatistics
                                sWriteOffset,
                                mIBChunkBuffer,
                                sReadSize ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * incremental Backup�� �����Ѵ�.
 * 
 * DataFileDescSlot - [IN] ����� �����Ϸ��� ������������ DataFileDescSlot
 * SrcFile          - [IN] �������� �Ǵ� ����
 * DestFile         - [IN] �����Ǵ� �������
 * BackupInfo       - [IN/OUT] ��������� �ʿ��� ������ ������� ��� ���� ����
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::performIncrementalBackup(
                                    smriCTDataFileDescSlot * aDataFileDescSlot,
                                    iduFile                * aSrcFile,
                                    iduFile                * aDestFile,
                                    smriCTTBSType            aTBSType,
                                    scSpaceID                aSpaceID,
                                    UInt                     aFileNo,
                                    smriBISlot             * aBackupInfo )
{
    UInt    sNewBmpExtListID;

    /* switch�� BmpExtListID�� ���Ѵ�. */
    sNewBmpExtListID = (aDataFileDescSlot->mCurTrackingListID + 1) % 2;

    /* sNewBmpExtList�� bitmap���� 0���� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( initBmpExtListBlocks( aDataFileDescSlot, sNewBmpExtListID )
              != IDE_SUCCESS );

    /* BmpExtList�� switch�Ѵ�. */
    IDE_TEST( switchBmpExtListID( aDataFileDescSlot ) != IDE_SUCCESS );

    /* bitmap�� ��ĵ�ϰ� IBChunk�� ������Ϸ� �����Ѵ�. */
    IDE_TEST( makeLevel1BackupFile( aDataFileDescSlot,
                                    aBackupInfo,
                                    aSrcFile,
                                    aDestFile,
                                    aTBSType,
                                    aSpaceID,
                                    aFileNo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ���� �����´�.
 * 
 * DataFileDescSlotID - [IN] ������ ���� ID
 * aSpaceID           - [IN] tablespace ID
 * aDataFileNum       - [IN] dataFile Num( or ID)
 * aResult            - [OUT] ���� ������ out����
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::isNeedAllocDataFileDescSlot( 
                                    smiDataFileDescSlotID * aDataFileDescSlotID,
                                    scSpaceID               aSpaceID,
                                    UInt                    aDataFileNum, 
                                    idBool                * aResult )
{
    smriCTDataFileDescBlock * sDataFileDescBlock;
    smriCTDataFileDescSlot  * sDataFileDescSlot;
    UInt                      sDataFileDescSlotIdx = 0; 
    idBool                    sNeedDataFileDescSlotAlloc;

    getBlock( aDataFileDescSlotID->mBlockID,
              (void**)&sDataFileDescBlock );

    sDataFileDescSlot = 
        &sDataFileDescBlock->mSlot[ aDataFileDescSlotID->mSlotIdx ];
    
    sDataFileDescSlotIdx |= ( 1 << aDataFileDescSlotID->mSlotIdx );

    /* loganchor�� ���������� ����� DataFileDescSlotID�� ����Ǿ�������
     * CTFile�� flush���� �ʾ� DataFileDescSlotID�� �Ҵ���� ���� ����
     */
    if( (sDataFileDescBlock->mAllocSlotFlag & 
         sDataFileDescSlotIdx) == 0 )
    {
        sNeedDataFileDescSlotAlloc = ID_TRUE;
    }
    else
    {
        if( (aSpaceID != sDataFileDescSlot->mSpaceID) &&
            (aDataFileNum != sDataFileDescSlot->mFileID) )
        {
            /* DataFileDescSlot�� �ٸ� ���������Ͽ� ���� �̹� �������
             * ���� �߻��Ҽ� ����
             */
            IDE_ERROR_MSG( 0, 
                           "SpaceID                    : %"ID_UINT32_FMT"\n"
                           "FileID                     : %"ID_UINT32_FMT"\n"
                           "DataFileDescSlot->mSpaceID : %"ID_UINT32_FMT"\n"
                           "DataFileDescSlot->mFileID  : %"ID_UINT32_FMT"\n",
                           aSpaceID,
                           aDataFileNum,
                           sDataFileDescSlot->mSpaceID,
                           sDataFileDescSlot->mFileID );
        }
        else
        {
            /* DataFileDescSlot�� ����� �Ҵ�Ǿ��ִ� ���� */
            sNeedDataFileDescSlotAlloc = ID_FALSE;
        }
    }

    *aResult = sNeedDataFileDescSlotAlloc;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * DataFile desc Slot�� �����´�.
 * 
 * aSlotID - [IN] ������ Slot�� ID
 * aSlot   - [OUT] Slot�� ������ out����
 *
 *
 **********************************************************************/
void smriChangeTrackingMgr::getDataFileDescSlot( 
                                     smiDataFileDescSlotID    * aSlotID,
                                     smriCTDataFileDescSlot  ** aSlot )
{
    smriCTDataFileDescBlock   * sDataFileDescSlotBlock;

    IDE_DASSERT( aSlotID != NULL );
    IDE_DASSERT( aSlot   != NULL );

    getBlock( aSlotID->mBlockID, (void**)&sDataFileDescSlotBlock );

    *aSlot = &sDataFileDescSlotBlock->mSlot[ aSlotID->mSlotIdx ];

    return;
}

/***********************************************************************
 * ���� �����´�.
 * 
 * aBlockID - [IN] ������ ���� ID
 * aBlock   - [OUT] ���� ������ out����
 *
 **********************************************************************/
void smriChangeTrackingMgr::getBlock( UInt aBlockID, void ** aBlock )
{
    UInt            sCTBodyIdx;
    UInt            sBlockIdx;
    smriCTBody    * sCTBody;

    IDE_DASSERT( aBlock != NULL );

    sCTBodyIdx  = aBlockID / mCTBodyBlockCnt;
    sCTBody     = mCTBodyPtr[ sCTBodyIdx ] ;
    sBlockIdx   = aBlockID - ( sCTBodyIdx * mCTBodyBlockCnt );

    *aBlock = (void *)&sCTBody->mBlock[ sBlockIdx ];

    return;
}

/***********************************************************************
 * NewBmpExtList�� ù��° �� ID�� �����´�.
 * 
 * aSlot            - [IN] ������ �����ִ� slot
 * aBmpExtBlockID   - [OUT] ���� ������ out����
 *
 **********************************************************************/
void smriChangeTrackingMgr::getFirstDifBmpExt( 
                                   smriCTDataFileDescSlot   * aSlot,
                                   UInt                     * aBmpExtBlockID)
{
    UInt    sNewBmpExtListID;
    
    IDE_DASSERT( aSlot           != NULL );
    IDE_DASSERT( aBmpExtBlockID  != NULL );

    sNewBmpExtListID = ( aSlot->mCurTrackingListID + 1 ) % 2;

    *aBmpExtBlockID = 
                aSlot->mBmpExtList[ sNewBmpExtListID ].mBmpExtListHint[0];

    return;
}

/***********************************************************************
 * NewBmpExtList�� ù��° �� ID�� �����´�.
 * 
 * aSlot            - [IN] ������ �����ִ� slot
 * aBmpExtBlockID   - [OUT] ���� ������ out����
 *
 **********************************************************************/
void smriChangeTrackingMgr::getFirstCumBmpExt( 
                                    smriCTDataFileDescSlot   * aSlot,
                                    UInt                     * aBmpExtBlockID)
{

    IDE_DASSERT( aSlot           != NULL );
    IDE_DASSERT( aBmpExtBlockID  != NULL );

    *aBmpExtBlockID = 
                aSlot->mBmpExtList[SMRI_CT_CUMULATIVE_LIST].mBmpExtListHint[0];

    return;
}

/***********************************************************************
 * BmpExtList�� ���� ��� Bmp block���� �ʱ�ȭ �Ѵ�.
 *
 * aBmpExtList - [IN] �ʱ�ȭ�� BmpExt�� List
 *
 *
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::initBmpExtListBlocks( 
                                    smriCTDataFileDescSlot * aDataFileDescSlot,
                                    UInt                     aBmpExtListID )
{
    smriCTBmpExt      * sBmpExt;
    smriCTBmpBlock    * sBmpBlock;
    smriCTBmpExtList  * sBmpExtList;
    UInt                sFirstBmpExtBlockID;
    UInt                sBmpBlockIdx;
    UInt                sBmpExtCnt;
    UInt                sBmpExtListLen;
    UInt                sState = 0;

    IDE_DASSERT( aDataFileDescSlot != NULL );
    IDE_DASSERT( aBmpExtListID < SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT );

    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.lock( NULL /*aStatistics*/ ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( wait4FlushAndExtend() != IDE_SUCCESS );

    IDE_ERROR( aDataFileDescSlot->mBmpExtCnt % 
                        SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT == 0 );
    
    /* BmpExtList�� ���̸� ���Ѵ� */
    sBmpExtListLen = aDataFileDescSlot->mBmpExtCnt 
                     / SMRI_CT_DATAFILE_DESC_BMP_EXT_LIST_CNT;

    sBmpExtList = &aDataFileDescSlot->mBmpExtList[ aBmpExtListID ];

    sFirstBmpExtBlockID = sBmpExtList->mBmpExtListHint[0];

    getBlock( sFirstBmpExtBlockID, (void**)&sBmpExt );

    for( sBmpExtCnt = 0; sBmpExtCnt < sBmpExtListLen; sBmpExtCnt++ )
    {
        for( sBmpBlockIdx = 0; 
             sBmpBlockIdx < SMRI_CT_EXT_BLOCK_CNT_EXCEPT_META_BLOCK; 
             sBmpBlockIdx++ )
        {
            sBmpBlock = &sBmpExt->mBmpBlock[ sBmpBlockIdx ];
            idlOS::memset( sBmpBlock->mBitmap,
                           0,
                           mBmpBlockBitmapSize );
        }

        getBlock( sBmpExt->mBmpExtHdrBlock.mNextBmpExtHdrBlockID, 
                            (void**)&sBmpExt );
    }

    IDE_ERROR( sBmpExtListLen == sBmpExtCnt );

    IDU_FIT_POINT("smriChangeTrackingMgr::initBmpExtListBlocks::wait");

    sBmpExtList->mSetBitCount = 0;

    sState = 0;
    IDE_TEST( mCTFileHdr.mBlockHdr.mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mCTFileHdr.mBlockHdr.mMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * ���� checksum�� �˻��Ѵ�.
 * 
 * aBlockHdr    - [IN] CheckSum�� ����� ���� ���
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::checkBlockCheckSum( smriCTBlockHdr * aBlockHdr )
{

    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBlockHdr != NULL );

    sCheckSumStartPtr = (UChar *)( &aBlockHdr->mCheckSum ) +
                        ID_SIZEOF( aBlockHdr->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary( 
                                    sCheckSumStartPtr,
                                    SMRI_CT_BLOCK_SIZE -
                                    ID_SIZEOF( aBlockHdr->mCheckSum ) );

    IDE_TEST_RAISE( sCheckSumValue != aBlockHdr->mCheckSum, 
                    error_invalid_change_tracking_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidChangeTrackingFile, 
                mFile.getFileName()));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * ���� checksum���� ���� �����Ѵ�.
 * 
 * aBlockHdr    - [IN] CheckSum�� ����� ���� ���
 * 
 **********************************************************************/
void smriChangeTrackingMgr::setBlockCheckSum( smriCTBlockHdr * aBlockHdr )
{

    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBlockHdr != NULL );

    sCheckSumStartPtr = (UChar *)( &aBlockHdr->mCheckSum ) 
                        + ID_SIZEOF( aBlockHdr->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary( 
                                    sCheckSumStartPtr,
                                    SMRI_CT_BLOCK_SIZE -
                                    ID_SIZEOF( aBlockHdr->mCheckSum ) );
    aBlockHdr->mCheckSum = sCheckSumValue;

    return;
}

/***********************************************************************
 * CTBody�� checksum���� �˻��Ѵ�.
 * 
 * aCTBody    - [IN] CheckSum�� �˻��� CTBody�� ptr
 * aFlushLSN  - [IN] CT���� ����� ����� FlushLSN
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::checkCTBodyCheckSum( smriCTBody * aCTBody, 
                                                   smLSN        aFlushLSN )
{
    UInt                sExtIdx;
    smriCTMetaExt     * sMetaExt;
    smriCTBmpExt      * sBmpExt;

    IDE_DASSERT( aCTBody != NULL );

    sMetaExt = &aCTBody->mMetaExtent;

    IDE_TEST( smrCompareLSN::isEQ( &sMetaExt->mExtMapBlock.mFlushLSN, 
                                   &aFlushLSN) 
              != ID_TRUE );

    IDE_TEST( checkExtCheckSum( &sMetaExt->mExtMapBlock.mBlockHdr ) 
              != IDE_SUCCESS );

    for( sExtIdx = 0; sExtIdx < mCTBodyBmpExtCnt; sExtIdx++ )
    {
        sBmpExt = &aCTBody->mBmpExtent[ sExtIdx ];

        IDE_TEST( smrCompareLSN::isEQ( &sBmpExt->mBmpExtHdrBlock.mFlushLSN, 
                                       &aFlushLSN) 
                  != ID_TRUE );

        IDE_TEST( checkExtCheckSum( &sBmpExt->mBmpExtHdrBlock.mBlockHdr ) 
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * CTBody�� checksum���� ����ϰ� �����Ѵ�.
 * 
 * aCTBody    - [IN] CheckSum�� ����� CTBody�� ptr
 * aFlushLSN  - [IN] BmpExtHdrBlock�� ����� FlushLSN
 * 
 **********************************************************************/
void smriChangeTrackingMgr::setCTBodyCheckSum( smriCTBody * aCTBody, 
                                               smLSN        aFlushLSN )
{
    UInt                sExtIdx;
    smriCTMetaExt     * sMetaExt;
    smriCTBmpExt      * sBmpExt;

    IDE_DASSERT( aCTBody != NULL );

    sMetaExt = &aCTBody->mMetaExtent;

    SM_GET_LSN( sMetaExt->mExtMapBlock.mFlushLSN, aFlushLSN );

    setExtCheckSum( &sMetaExt->mExtMapBlock.mBlockHdr );

    for( sExtIdx = 0; sExtIdx < mCTBodyBmpExtCnt; sExtIdx++ )
    {
        sBmpExt = &aCTBody->mBmpExtent[ sExtIdx ];

        SM_GET_LSN( sBmpExt->mBmpExtHdrBlock.mFlushLSN, aFlushLSN );

        setExtCheckSum( &sBmpExt->mBmpExtHdrBlock.mBlockHdr );
    }

    return;
}

/***********************************************************************
 * Ext�� checksum���� �˻��Ѵ�.
 * 
 * aBlockHdr    - [IN] CheckSum�� ����� BmpExtHdrBlock
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::checkExtCheckSum( smriCTBlockHdr * aBlockHdr )
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBlockHdr != NULL );
    IDE_DASSERT( aBlockHdr->mBlockID % SMRI_CT_EXT_BLOCK_CNT == 0 );

    sCheckSumStartPtr = (UChar *)( &aBlockHdr->mCheckSum ) 
                        + ID_SIZEOF( aBlockHdr->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary( 
                            sCheckSumStartPtr,
                            SMRI_CT_EXT_SIZE -
                            ID_SIZEOF( aBlockHdr->mCheckSum ) );

    IDE_TEST( aBlockHdr->mCheckSum != sCheckSumValue );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Ext�� checksum���� ���� �����Ѵ�.
 * 
 * aBlockHdr    - [IN] CheckSum�� ����� BmpExtHdrBlock
 * 
 **********************************************************************/
void smriChangeTrackingMgr::setExtCheckSum( smriCTBlockHdr * aBlockHdr )
{
    UChar * sCheckSumStartPtr;
    UInt    sCheckSumValue;

    IDE_DASSERT( aBlockHdr != NULL );
    IDE_DASSERT( aBlockHdr->mBlockID % SMRI_CT_EXT_BLOCK_CNT == 0 );

    sCheckSumStartPtr = (UChar *)( &aBlockHdr->mCheckSum ) 
                        + ID_SIZEOF( aBlockHdr->mCheckSum );

    sCheckSumValue = smuUtility::foldBinary( 
                            sCheckSumStartPtr,
                            SMRI_CT_EXT_SIZE -
                            ID_SIZEOF( aBlockHdr->mCheckSum ) );

    aBlockHdr->mCheckSum = sCheckSumValue;

    return;
}

/***********************************************************************
 * MemBase�� ����� DBName�� CTFile�� ����� DBName�� ���Ѵ�.
 * 
 * aDBName    - [IN] MemBase�� ����� DBName
 * 
 **********************************************************************/
IDE_RC smriChangeTrackingMgr::checkDBName( SChar * aDBName )
{
    IDE_DASSERT( aDBName != NULL );

    IDE_TEST_RAISE( idlOS::strcmp( mCTFileHdr.mDBName, 
                                   (const char *)aDBName ) != 0,
                    error_invalid_change_tracking_file );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_change_tracking_file );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidChangeTrackingFile, 
                mFile.getFileName()));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
