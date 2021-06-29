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
 * $Id:$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer 
 * BCB���� �ּ����� ������ �����ؼ� frame�� ���� �����Ѵ�.
 * Secondary Buffer File �׸��� ���� ������ ����
 * Group ��  ������ �̿��� MetaTable�� �ۼ��Ѵ�. 

   |F|F|F|..|F|F|F|..|F|F|F|..|MetaTable|F|F|F|..|F|F|F|..
   |-Extent-|-Extent-|-Extent-|         |-Extent-|-Extent-|...
   |------     Group   -------|         |------     Group    ---- ...
 **********************************************************************/

#include <smDef.h>
#include <smErrorCode.h>
#include <sdb.h>
#include <sdpPhyPage.h>
#include <sds.h>
#include <sdsReq.h>

/***********************************************************************
 * Description :
 * Meta Table�� ���� ���� ������ Ȯ���Ѵ�.
 ***********************************************************************/
IDE_RC sdsMeta::initializeStatic( UInt            aGroupCnt,
                                  UInt            aExtCntInGroup,
                                  sdsFile       * aSBufferFile,
                                  sdsBufferArea * aSBufferArea,
                                  sdbBCBHash    * aHashTable )
{
    SInt    sState = 0; 
    UInt    i;

    /* �ִ� Group ���� */
    mMaxMetaTableCnt = aGroupCnt,
    /* �ϳ��� Group�� ���� Extent�� �� */
    mExtCntInGroup = aExtCntInGroup;
    /* �� Extent���� Frame(=meta)�� ������ IOB ������� ���� */
    mFrameCntInExt = smuProperty::getBufferIOBufferSize();

    mSBufferFile = aSBufferFile;
    mSBufferArea = aSBufferArea;
    mHashTable   = aHashTable;

    IDE_ASSERT( SDS_META_TABLE_SIZE > 
                (mExtCntInGroup * mFrameCntInExt * ID_SIZEOF(sdsMetaData)) );

    /* Meta ������ ������ ������ Ȯ�� : �� Group�� ũ��: �� 58KB  */
    IDE_TEST_RAISE( iduFile::allocBuff4DirectIO( IDU_MEM_SM_SDS,
                                                 SDS_META_TABLE_SIZE,
                                                 (void**)&mBasePtr,
                                                 (void**)&mBase  )
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 1;

    /* TC/FIT/Limit/sm/sds/sdsMeta_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsMeta::initialize::malloc1", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(sdsMetaData*) * 
                                       mExtCntInGroup,
                                       (void**)&mMetaData ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 2;

    /* TC/FIT/Limit/sm/sds/sdsMeta_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "sdsMeta::initialize::malloc2", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(sdsMetaData) *
                                       mFrameCntInExt *
                                       mExtCntInGroup,
                                       (void**)&mMetaData[0] ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 3;

    for( i = 1; i < mExtCntInGroup ; i++ )
    {
        mMetaData[i] = mMetaData[i-1] + mFrameCntInExt ;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSUFFICIENT_MEMORY );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            IDE_ASSERT( iduMemMgr::free( mMetaData[0] ) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( iduMemMgr::free( mMetaData ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( mBasePtr ) == IDE_SUCCESS );
        default:
            break;   
    }
 
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC sdsMeta::destroyStatic()
{
    IDE_ASSERT( iduMemMgr::free( mMetaData[0] ) == IDE_SUCCESS );
    IDE_ASSERT( iduMemMgr::free( mMetaData ) == IDE_SUCCESS );
    IDE_ASSERT( iduMemMgr::free( mBasePtr ) == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : smiStartupShutdown ���� ȣ��� 
 *  shutdown �� ��Ÿ ����Ÿ�� ������Ʈ ��  
 ***********************************************************************/
IDE_RC sdsMeta::finalize( idvSQL * aStatistics )
{
    sdsBCB    * sSBCB;
    UInt        sBCBIndex;
    UInt        sMetaTableIndex;
    UInt        sLastMetaTableNum;
    UInt        i;
    UInt        j;
    
    /* Group�� 0~MAX ���� ��ĵ�Ͽ� MetaTable �� ���� ������ */ 
    for( sMetaTableIndex = 0 ; sMetaTableIndex < mMaxMetaTableCnt ; sMetaTableIndex++ ) 
    {
        sBCBIndex = sMetaTableIndex * ( mFrameCntInExt * mExtCntInGroup );

        for( i = 0 ; i < mExtCntInGroup ; i++ )
        {
            for( j = 0 ; j < mFrameCntInExt ; j++ )
            {                 
                sSBCB = mSBufferArea->getBCB( sBCBIndex++ );

                mMetaData[i][j].mSpaceID = sSBCB->mSpaceID; 
                mMetaData[i][j].mPageID  = sSBCB->mPageID;
                mMetaData[i][j].mState   = sSBCB->mState;
                mMetaData[i][j].mPageLSN = sSBCB->mPageLSN;
            }
        }

        /* mBase�� ���� �ϱ� */
        (void)copyToMetaTable( sMetaTableIndex );
        /* mBase�� stable�� �������� */
        IDE_TEST( writeMetaTable( aStatistics, sMetaTableIndex )
                  != IDE_SUCCESS );
    }

    sLastMetaTableNum = mSBufferArea->getLastFlushExtent() / mExtCntInGroup;

    /* File Header �� Seq ���� */ 
    IDE_TEST( mSBufferFile->setAndWriteFileHdr( 
                aStatistics,
                NULL,               /* mRedoLSN       */
                NULL,               /* mCreateLSN     */
                NULL,               /* mFrameCntInExt */
                &sLastMetaTableNum )/* mLstGroupNum   */
            != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
   | SEQ | LSN | MetaData(0), ,,,,, MetaData(N) | LSN |
 * aMetaTableSeqNum   - [IN] Group�� ��ȣ 
 ***********************************************************************/
IDE_RC sdsMeta::writeMetaTable( idvSQL  * aStatistics, 
                                UInt      aMetaTableSeqNum )
{
    IDE_TEST( mSBufferFile->open( aStatistics ) != IDE_SUCCESS );
   
    IDE_TEST( mSBufferFile->write( aStatistics,
                            0,                                 /* ID */
                            getTableOffset( aMetaTableSeqNum ),
                            SDS_META_TABLE_PAGE_COUNT,         /* page count */
                            mBase ) 
              != IDE_SUCCESS );       

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
   | SEQ | LSN | MetaData(0), ,,,,, MetaData(N) | LSN |
 ***********************************************************************/
IDE_RC sdsMeta::copyToMetaTable( UInt aMetaTableSeqNum )
{
    smLSN    sChkLSN;
    UInt     sMetaDataLen;

    idlOS::memset( mBase , 0x00, SDS_META_TABLE_SIZE );

    SM_GET_LSN( sChkLSN, mMetaData[0][0].mPageLSN );
    /* 1. Seq */   
    idlOS::memcpy( mBase , &aMetaTableSeqNum, ID_SIZEOF(UInt) );
    /* 2. LSN */
    idlOS::memcpy( mBase + ID_SIZEOF(UInt), 
                   &sChkLSN, 
                   ID_SIZEOF(smLSN) );
    /* 3. MetaData */
    sMetaDataLen = ID_SIZEOF(sdsMetaData) * mExtCntInGroup * mFrameCntInExt ;
    
    idlOS::memcpy( mBase + ID_SIZEOF(UInt) + ID_SIZEOF(smLSN), 
                   mMetaData[0], 
                   sMetaDataLen );
    /* 4. LSN */
    idlOS::memcpy( mBase + ID_SIZEOF(UInt) + ID_SIZEOF(smLSN) + sMetaDataLen, 
                   &sChkLSN, 
                   ID_SIZEOF(smLSN) );

    return IDE_SUCCESS;
} 

/***********************************************************************
 * Description :
 * aMetaTableSeqNum  - [IN] �о���ϴ� MetaTable Num 
 * aMeta             - [OUT] MetaData�� ������ ����
 * aIsValidate       - [OUT] MetaTable�� ���� SBCB�� �����ص� �Ǵ���  
 ***********************************************************************/
IDE_RC sdsMeta::readAndCheckUpMetaTable( idvSQL         * aStatistics, 
                                         UInt             aMetaTableSeqNum,
                                         sdsMetaData    * aMeta,
                                         idBool         * aIsValidate ) 
{
    UInt      sReadMetaTableSeq;
    smLSN     sFstLSN;    
    smLSN     sLstLSN;    
    UInt      sMetaLen;

    IDE_TEST( mSBufferFile->open( aStatistics ) != IDE_SUCCESS );

    IDE_TEST( mSBufferFile->read( aStatistics,
                                  0,                              /* aFrameID */
                                  getTableOffset( aMetaTableSeqNum ),
                                  SDS_META_TABLE_PAGE_COUNT,      /* page count */
                                  (UChar*)mBase ) 
              != IDE_SUCCESS );       

    /* 1. Seq */
    idlOS::memcpy( &sReadMetaTableSeq, 
                   (UChar*)mBase, 
                   ID_SIZEOF(UInt) );
    /* 2. LSN */
    idlOS::memcpy( &sFstLSN, 
                   (SChar*)(mBase + ID_SIZEOF(UInt)), 
                   ID_SIZEOF(smLSN) );
    /* 3. MetaData */
    sMetaLen = ID_SIZEOF(sdsMetaData) * mExtCntInGroup * mFrameCntInExt ;
    idlOS::memcpy( (SChar*)aMeta, 
                   (SChar*)(mBase + ID_SIZEOF(UInt) + ID_SIZEOF(smLSN)), 
                   sMetaLen );
    /* 4. LSN */
    idlOS::memcpy( &sLstLSN, 
                   (SChar*)(mBase + ID_SIZEOF(UInt) + ID_SIZEOF(smLSN) + sMetaLen),
                   ID_SIZEOF(smLSN) );

    /* checksum�� �ٸ��� skip */
    IDE_TEST_CONT( smLayerCallback::isLSNEQ( &sFstLSN, &sLstLSN ) 
                    == ID_TRUE, 
                    CONT_SKIP );
    
    /* ChecksumLSN�� 0�� MetaData�� LSN�� �ٸ��� skip */
    IDE_TEST_CONT( smLayerCallback::isLSNEQ( &sFstLSN, &aMeta[0].mPageLSN ) 
                    == ID_TRUE,
                    CONT_SKIP );

    /* �о�� �ϴ� MetaTable �̶� ���� MetaTable�� ��ȣ�� �ٸ��� skip */
    IDE_TEST_CONT( sReadMetaTableSeq != aMetaTableSeqNum, 
                    CONT_SKIP );

    *aIsValidate = ID_TRUE;

    IDE_EXCEPTION_CONT( CONT_SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 * aLstMetaTableSeqNum - [OUT] �������� ����� MetaTable Seq ��ȣ
 * aRecoveryLSN        - [OUT] RecoveryLSN
 ***********************************************************************/
IDE_RC sdsMeta::readMetaInfo( idvSQL * aStatistics, 
                              UInt   * aLstMetaTableSeqNum,
                              smLSN  * aRecoveryLSN ) 
{
    IDE_TEST( mSBufferFile->open( aStatistics ) != IDE_SUCCESS );

    IDE_TEST( mSBufferFile->readHdrInfo( aStatistics, 
                                         aLstMetaTableSeqNum,
                                         aRecoveryLSN )
              != IDE_SUCCESS );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Meta Table�� �о BCB ������ �籸���Ѵ�.
************************************************************************/
IDE_RC sdsMeta::copyToSBCB( idvSQL      * aStatistics,
                            sdsMetaData * aMetaData,
                            UInt          aMetaTableNum,
                            smLSN       * aRecoveryLSN )
{
    SInt        i;
    UInt        sBCBIndex;
    sdsBCB    * sSBCB     = NULL;
    sdsBCB    * sExistBCB = NULL;
    sdbHashChainsLatchHandle  * sHashChainsHandle = NULL;

    /* �ش� MetaTable�� ������ MetaData���� �о� �;� �ϹǷ�
       �״��� Extent�� ù MetaData ��ȣ �����ͼ� �ϳ� ���� */        
    sBCBIndex = ( aMetaTableNum+1 ) * mFrameCntInExt * mExtCntInGroup;
    sBCBIndex--;

    i = mFrameCntInExt-1;

    for( ; i >= 0 ; i-- )
    {                   
        sSBCB = mSBufferArea->getBCB( sBCBIndex-- );

        if( ( aMetaData[i].mState == SDS_SBCB_FREE )  ||
            ( aMetaData[i].mState == SDS_SBCB_INIOB ) ||
            ( aMetaData[i].mState == SDS_SBCB_OLD ) )
        {
            continue;
        } 
        else 
        {
            IDE_ASSERT( ( aMetaData[i].mState == SDS_SBCB_CLEAN ) ||
                        ( aMetaData[i].mState == SDS_SBCB_DIRTY ) );
        }

        if ( smLayerCallback::isLSNGT( aRecoveryLSN, 
                                       &aMetaData[i].mPageLSN ) 
             == ID_TRUE) 
        {
            continue; 
        }

        sSBCB->mSpaceID  = aMetaData[i].mSpaceID; 
        sSBCB->mPageID   = aMetaData[i].mPageID;
        sSBCB->mState    = aMetaData[i].mState;
        SM_GET_LSN( sSBCB->mPageLSN, aMetaData[i].mPageLSN );

        /* ������ BCB�� hash�� ����.. */
        sHashChainsHandle = mHashTable->lockHashChainsXLatch( aStatistics,
                                                              sSBCB->mSpaceID,
                                                              sSBCB->mPageID );

        /* Meta Table�� �о Hash�� �籸�� �Ҷ��� backward�� ��ĵ�Ѵ� */
        /* �̹� ���Ե� ����Ÿ�� ���� �Ѵٸ� �װ� �� �ֽ� ����Ÿ �ΰ���. */
        mHashTable->insertBCB( sSBCB, (void**)&sExistBCB );

        if( sExistBCB != NULL )
        {
            /* ���� ���� �������� LSN�� �����ϴ� LSN���� ũ�� ���� */
            if ( smLayerCallback::isLSNGTE( &sSBCB->mPageLSN, 
                                            &sExistBCB->mPageLSN ) 
                 == ID_TRUE) 
            {
                mHashTable->removeBCB ( sExistBCB );
                (void)sExistBCB->setFree();

                mHashTable->insertBCB( sSBCB, (void**)&sExistBCB );
            }
            else 
            {
                (void)sSBCB->setFree();
            }     
        }

        mHashTable->unlockHashChainsLatch( sHashChainsHandle );
        sHashChainsHandle = NULL;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 * aLstMetaTableSeqNum - [IN] MetaTable �˻��� ���� BCB�� ������ ù Table ��ȣ 
 * aRecoveryLSN        - [IN] RecoveryLSN ���� ���� pageLSN�� ������ BCB�� ������ �ʿ� ����.
 ***********************************************************************/
IDE_RC sdsMeta::buildBCBByMetaTable( idvSQL * aStatistics, 
                                     UInt     aLstMetaTableSeqNum,
                                     smLSN  * aRecoveryLSN )
{
    UInt        i;
    UInt        sTargetMetaTableSeq;
    sdsMetaData sMetaData[SDS_META_DATA_MAX_COUNT_IN_GROUP]; 
    idBool      sIsValidate = ID_FALSE;

    sTargetMetaTableSeq = aLstMetaTableSeqNum;

    for( i = 0 ; i < mMaxMetaTableCnt ; i++ )
    { 
        IDE_TEST( readAndCheckUpMetaTable( aStatistics, 
                                           sTargetMetaTableSeq, 
                                           sMetaData,
                                           &sIsValidate ) 
                  != IDE_SUCCESS );
    
        if( sIsValidate == ID_FALSE )
        {
            /* �������� MetaTable�� �ƴϹǷ� ���� MetaTable�� �˻� �Ѵ�*/
            continue;
        }
        else
        {
            /* MetaTable�� ���� SBCB ������ �����Ѵ�. */
            /* nothing to do */
        }
        
        IDE_TEST( copyToSBCB( aStatistics, 
                              sMetaData, 
                              sTargetMetaTableSeq, 
                              aRecoveryLSN ) 
                  != IDE_SUCCESS ); 
        
        if( sTargetMetaTableSeq > 0 )
        {
            sTargetMetaTableSeq--;
        }
        else 
        {
            sTargetMetaTableSeq = mMaxMetaTableCnt-1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
} 

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC sdsMeta::buildBCB( idvSQL * aStatistics, idBool aIsRecoveryMode )
{
    UInt  sLstMetaTableSeqNum;
    smLSN sRecoveryLSN;

    if( aIsRecoveryMode != ID_TRUE ) /* ���� ���� */
    { 
        IDE_TEST( readMetaInfo( aStatistics, 
                                &sLstMetaTableSeqNum,
                                &sRecoveryLSN )
                  != IDE_SUCCESS );

        if( sLstMetaTableSeqNum != SDS_FILEHDR_SEQ_MAX )
        {
            IDE_TEST( sLstMetaTableSeqNum > mMaxMetaTableCnt ); 

            IDE_TEST( buildBCBByMetaTable( aStatistics,
                                           sLstMetaTableSeqNum,
                                           &sRecoveryLSN )   
                      != IDE_SUCCESS);
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
         /* ������ ����ÿ��� BCB �籸���� ���� ����. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : dump �Լ�. Meta table�� ����Ѵ�.
 * aPage     [IN]  - ����� ������ ���� 
 * aOutBuf   [OUT] - ������ ������ ������ ����
 * aOutSize  [IN]  - ������ limit ũ�� 
 ***********************************************************************/
IDE_RC sdsMeta::dumpMetaTable( const UChar    * aPage,
                               SChar          * aOutBuf, 
                               UInt          /* aOutSize */)
{
    UInt    i;
    sdsMetaData * sMetaData;

    IDE_ASSERT( aPage != NULL );
    IDE_ASSERT( aOutBuf != NULL );
  
    sMetaData = (sdsMetaData*)aPage;
    
    for( i = 0 ; i < SDS_META_DATA_MAX_COUNT_IN_GROUP ; i++ )
    {
        idlOS::sprintf( aOutBuf,
                        "%"ID_UINT32_FMT".\n",
                        i );

        idlOS::sprintf( aOutBuf,
                "Space ID        : %"ID_UINT32_FMT"\n"
                "Page ID         : %"ID_UINT32_FMT"\n",
                sMetaData[i].mSpaceID, 
                sMetaData[i].mPageID );

        switch( sMetaData[i].mState )
        {
            case SDS_SBCB_FREE:
                idlOS::sprintf( aOutBuf,
                                "Page State      : SDS_SBCB_FREE \n" );
                break;

            case  SDS_SBCB_CLEAN:
                idlOS::sprintf( aOutBuf,
                                "Page State      : SDS_SBCB_CLEAN \n" );
                break;

            case SDS_SBCB_DIRTY:
                idlOS::sprintf( aOutBuf,
                                "Page State      : SDS_SBCB_DIRTY \n" );
                break;

            case SDS_SBCB_INIOB:
                idlOS::sprintf( aOutBuf,
                                "Page State      : SDS_SBCB_INOIB \n" );
                break;

            case SDS_SBCB_OLD: 
                idlOS::sprintf( aOutBuf,
                                "Page State      : SDS_SBCB_REDIRTY \n" );
                break;

            default:
                idlOS::sprintf( aOutBuf, 
                                "Unknown State \n" );
                break;
        }

        idlOS::sprintf( aOutBuf,
                "PageLSN.FileNo  : %"ID_UINT32_FMT"\n"
                "PageLSN.Offset  : %"ID_UINT32_FMT"\n",
                sMetaData[i].mPageLSN.mFileNo,
                sMetaData[i].mPageLSN.mOffset );

        idlOS::sprintf( aOutBuf, " \n" );
    }  

    return IDE_SUCCESS;
}
