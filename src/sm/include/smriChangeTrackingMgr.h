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
 * - Incremental chunk change tracking manager
 *
 **********************************************************************/

#ifndef _O_SMRI_CT_MANAGER_H
#define _O_SMRI_CT_MANAGER_H 1

#include <smr.h>
#include <smriDef.h>
#include <smiDef.h>
#include <sctDef.h>


class smriChangeTrackingMgr
{
private:
    /*change tracking���� ���*/
    static smriCTFileHdrBlock   mCTFileHdr;

    static smriCTHdrState       mCTHdrState;

    /*CT���� */
    static iduFile              mFile;

    /*CT body�� ���� pointer*/
    static smriCTBody         * mCTBodyPtr[SMRI_CT_MAX_CT_BODY_CNT];       

    /*CT body�� flush�ϱ� ���� buffer */
    static smriCTBody         * mCTBodyFlushBuffer;

    static iduMutex             mMutex;

    static UInt                 mBmpBlockBitmapSize;
    static UInt                 mCTBodyExtCnt;
    static UInt                 mCTExtMapSize;
    static UInt                 mCTBodySize;
    static UInt                 mCTBodyBlockCnt;
    static UInt                 mCTBodyBmpExtCnt;
    static UInt                 mBmpBlockBitCnt;
    static UInt                 mBmpExtBitCnt;

    static iduFile            * mSrcFile;
    static iduFile            * mDestFile;
    static SChar              * mIBChunkBuffer;
    static ULong                mCopySize;
    static smriCTTBSType        mTBSType;
    static UInt                 mIBChunkCnt;
    static UInt                 mIBChunkID;

    static UInt                 mFileNo;
    static scPageID             mSplitFilePageCount;
    
    static UInt                 mCurrChangeTrackingThreadCnt;

private:

    /*CTBody �ʱ�ȭ*/
    static IDE_RC createCTBody( smriCTBody * aCTBody );

    /*���� ��� �� �ʱ�ȭ*/
    static IDE_RC createFileHdrBlock( smriCTFileHdrBlock * aCTFileHdrBlock );

    /*extent map �� �ʱ�ȭ*/
    static IDE_RC createExtMapBlock( smriCTExtMapBlock * aExtMapBlock, 
                                     UInt                aBlockID );

    /*DataFile Desc �� �ʱ�ȭ*/
    static IDE_RC createDataFileDescBlock( 
                        smriCTDataFileDescBlock * aDataFileDescBlock,
                        UInt                      aBlockID );

    /*DataFile Desc slot �ʱ�ȭ*/
    static IDE_RC createDataFileDescSlot( 
                        smriCTDataFileDescSlot * aDataFileDescSlot,
                        UInt                     aBlockID,
                        UInt                     aSlotIdx );

    /*BmpExtHdr�� �ʱ�ȭ*/
    static IDE_RC createBmpExtHdrBlock( smriCTBmpExtHdrBlock * aBmpExtHdrBlock,
                                        UInt                   aBlockID );

    /*Bmp �� �ʱ�ȭ*/
    static IDE_RC createBmpBlock( smriCTBmpBlock * aBmpBlock,
                                  UInt             aBlockID );

    /*�� ��� �ʱ�ȭ*/
    static IDE_RC createBlockHdr( smriCTBlockHdr    * aBlockHdr, 
                                  smriCTBlockType     aBlockType, 
                                  UInt                aBlockID );

    /*�̹� ���� �ϴ� ��� ���̺����̽��� ���������ϵ��� CT���Ͽ� ���*/
    static IDE_RC addAllExistingDataFile2CTFile( idvSQL * aStatistic );

    /*�̹� �����ϴ� �� ���̺� �����̽��� ������������ CT���Ͽ� ���*/
    static IDE_RC addExistingDataFile2CTFile( 
                        idvSQL                      * aStatistic,
                        sctTableSpaceNode           * aSpaceNode,
                        void                        * aActionArg );

    static IDE_RC loadCTBody( ULong aCTBodyOffset, UInt aCTBodyIdx );
    static IDE_RC unloadCTBody( smriCTBody * aCTBody );

    /*CT���ϰ��� �Լ�*/
    static IDE_RC validateCTFile();
    static IDE_RC validateBmpExtList( smriCTBmpExtList * aBmpExtList, 
                                      UInt               aBmpExtListLen );

    static IDE_RC initBlockMutex( smriCTBody * aCTBody );
    static IDE_RC destroyBlockMutex( smriCTBody * aCTBody );

    static IDE_RC initBmpExtLatch( smriCTBody * aCTBody );
    static IDE_RC destroyBmpExtLatch( smriCTBody * aCTBody );

    /*flush�ϱ����� �غ�*/
    static IDE_RC startFlush();

    /*flush �Ϸ�*/
    static IDE_RC completeFlush();

    /*CT���� ��� flush*/
    static IDE_RC flushCTFileHdr(); 

    /*CTBody flush*/
    static IDE_RC flushCTBody( smLSN aFlushLSN );

    /*extend �ϱ����� �غ�*/
    static IDE_RC startExtend( idBool * aIsNeedExtend );

    /*extend �Ϸ�*/
    static IDE_RC completeExtend();

    /*CT������ flush,extend������ �Ϸ�Ǵ°��� ���*/
    static IDE_RC wait4FlushAndExtend();

    /*DataFile Desc slot�� �Ҵ�ٰ� �����´�.*/
    static IDE_RC allocDataFileDescSlot( smriCTDataFileDescSlot ** aSlot );
    
    /*��������� ���� DataFile Desc slot�� Idx�� �����´�.*/
    static void getFreeSlotIdx( UInt aAllocSlotFlag, UInt * aSlotIdx );

    /*DataFile Desc slot�� BmpExt�� �߰�*/
    static IDE_RC addBmpExt2DataFileDescSlot( 
                                    smriCTDataFileDescSlot * aSlot,
                                    smriCTBmpExt          ** sAllocCurBmpExt );

    /*����� BmpExt�� �Ҵ�*/
    static IDE_RC allocBmpExt( smriCTBmpExt ** aAllocBmpExt );

    /*�Ҵ� ���� BmpExt�� dataFile Desc slot�� BmpExtList�� �߰�*/
    static void addBmpExt2List( smriCTBmpExtList       * aBmpExtList,
                                smriCTBmpExt           * aNewBmpExt );

    /* DataFile Desc slot�� �ִ� BmpExtList�� �Ҵ�� BmpExt�� �Ҵ������ϰ� �ʱ�ȭ
     * �Ѵ�.*/
    static IDE_RC deleteBmpExtFromDataFileDescSlot( smriCTDataFileDescSlot * aSlot );

    static IDE_RC deallocDataFileDescSlot( smriCTDataFileDescSlot  * aSlot );

    static IDE_RC deleteBmpExtFromList( smriCTBmpExtList      * aBmpExtList,
                                      smriCTBmpExtHdrBlock ** aBmpExtHdrBlock );

    /*�Ҵ�Ǿ� ������̴� BmpExt�� �Ҵ��� �����Ѵ�.*/
    static IDE_RC deallocBmpExt( smriCTBmpExtHdrBlock  * sBmpExtHdrBlock );

    /*����� IBChunk�� �������� current BmpExt�� �����´�.*/
    static IDE_RC getCurBmpExt( 
                        smriCTDataFileDescSlot   * aSlot,
                        UInt                       aIBChunkID,
                        smriCTBmpExt            ** aCurBmpExt );
    
    /*BmpExtList���� aBmpExtSequence��°�� BmpExt�� �����´�.*/
    static IDE_RC getCurBmpExtInternal( smriCTDataFileDescSlot   * aSlot,
                                      UInt                       aBmpExtSeq,
                                      smriCTBmpExt            ** aCurBmpExt );

    /*IBChunkID�� ���εǴ� bit�� ��ġ�� ���ϰ� bit�� set�Ѵ�.*/
    static IDE_RC calcBitPositionAndSet( smriCTBmpExt             * aCurBmpExt,
                                         UInt                       aIBChunkID,
                                         smriCTDataFileDescSlot   * sSlot );

    /*bit�� set�ϱ����� �Լ�*/
    static IDE_RC setBit( smriCTBmpExt     * aBmpExt, 
                          UInt               aBmpBlockIdx, 
                          UInt               aByteIdx, 
                          UInt               aBitPosition,
                          smriCTBmpExtList * aBmpExtList );

    static IDE_RC makeLevel1BackupFilePerEachBmpExt( 
                                        smriCTBmpExt * aBmpExt,
                                        UInt           aBmpExtSeq );
    
    static IDE_RC makeLevel1BackupFilePerEachBmpBlock( 
                                         smriCTBmpBlock   * aBmpBlock,  
                                         UInt               aBmpExtSeq, 
                                         UInt               aBmpBlockIdx );

    static IDE_RC makeLevel1BackupFilePerEachBmpByte( SChar * aBmpByte,
                                                      UInt    aBmpExtSeq,
                                                      UInt    aBmpBlockIdx,
                                                      UInt    aBmpByteIdx );

    static IDE_RC copyIBChunk( UInt aIBChunkID );

    /*DifBmpExtList�� ù��° BmpExt�� �����´�.*/
    static void getFirstDifBmpExt( smriCTDataFileDescSlot   * aSlot,
                                   UInt                     * aBmpExtBlockID);

    /*CurBmpExtList�� ù��° BmpExt�� �����´�.*/
    static void getFirstCumBmpExt( smriCTDataFileDescSlot   * aSlot,
                                   UInt                     * aBmpExtBlockID);

    /*BlockID�� �̿��� �ش����� �����´�.*/
    static void getBlock( UInt aBlockID, void ** aBlock );

    static void setBlockCheckSum( smriCTBlockHdr * aBlockHdr );
    static IDE_RC checkBlockCheckSum( smriCTBlockHdr * aBlockHdr );

    static void setExtCheckSum( smriCTBlockHdr * aBlockHdr );
    static IDE_RC checkExtCheckSum( smriCTBlockHdr * aBlockHdr );

    static void setCTBodyCheckSum( smriCTBody * aCTBody, smLSN aFlushLSN );
    static IDE_RC checkCTBodyCheckSum( smriCTBody * aCTBody, smLSN aFlushLSN );

    inline static IDE_RC lockCTMgr()
    {
        return mMutex.lock ( NULL );
    }

    inline static IDE_RC unlockCTMgr()
    {
        return mMutex.unlock ();
    }

    static inline UInt getCurrChangeTrackingThreadCnt()
    { return idCore::acpAtomicGet32( &mCurrChangeTrackingThreadCnt ); };

public:

    static IDE_RC initializeStatic();
    static IDE_RC destroyStatic();

    static IDE_RC begin();
    static IDE_RC end();
 
    /*CT���� ����*/
    static IDE_RC createCTFile();
    static IDE_RC removeCTFile();

    /*flush �����Լ�*/
    static IDE_RC flush();

    /*extend ���� �Լ�*/
    static IDE_RC extend( idBool aFlushBody, smriCTBody ** aCTBody );

    /*������������ CT���Ͽ� ���*/
    static IDE_RC addDataFile2CTFile( scSpaceID                     aSpaceID, 
                                      UInt                          aDataFileID, 
                                      smriCTTBSType                 aTBSType,
                                      smiDataFileDescSlotID      ** aSlotID );

    /*CT���Ͽ� ��ϵǾ��ִ� ������������ ��������Ѵ�.*/
    //static IDE_RC deallocDataFileDescSlot( smiDataFileDescSlotID * aSlotID );
    static IDE_RC deleteDataFileFromCTFile( smiDataFileDescSlotID * aSlotID );


    /*differential bakcup���� ������ �������� BmpExtList�� switch�Ѵ�.*/
    static IDE_RC switchBmpExtListID( smriCTDataFileDescSlot * aSlot );

    static IDE_RC changeTracking( sddDataFileNode * sDataFileNode,
                                  smmDatabaseFile * aDatabaseFile,
                                  UInt              aIBChunkID );

    /*BmpExtList�� ���� ��� Bmp block���� �ʱ�ȭ �Ѵ�.*/
    static IDE_RC initBmpExtListBlocks( 
                            smriCTDataFileDescSlot * aDataFileDescSlot,
                            UInt                     sBmpExtListID );

    /* incrementalBackup�� �����Ѵ�. */
    static IDE_RC performIncrementalBackup(                        
                                smriCTDataFileDescSlot * aDataFileDescSlot,           
                                iduFile                * aSrcFile,
                                iduFile                * aDestFile,
                                smriCTTBSType            aTBSType,
                                scSpaceID                aSpaceID,
                                UInt                     aFileNo,
                                smriBISlot             * aBackupInfo );
    
    /* restart recovery�� DataFileDescSlot�� �Ҵ��� �ʿ䰡�ִ��� �˻��Ѵ�.*/
    static IDE_RC isNeedAllocDataFileDescSlot( 
                                    smiDataFileDescSlotID * aDataFileDescSlotID,
                                    scSpaceID               aSpaceID,
                                    UInt                    aFileNum,
                                    idBool                * aResult );

    /* DataFileSlotID�� �ش��ϴ� Slot�� �����´�. */
    static void getDataFileDescSlot( smiDataFileDescSlotID    * aSlotID, 
                                     smriCTDataFileDescSlot  ** aSlot );

    static IDE_RC makeLevel1BackupFile( smriCTDataFileDescSlot * aCTSlot,
                                        smriBISlot             * aBackupInfo,
                                        iduFile                * aSrcFile,
                                        iduFile                * aDestFile,
                                        smriCTTBSType            aTBSType,
                                        scSpaceID                aSpaceID,
                                        UInt                     aFileNo );

    /*DataFile Desc slot�� �Ҵ�� ��� BmpExtHdr�� latch�� ȹ���Ѵ�.*/
    static IDE_RC lockBmpExtHdrLatchX( smriCTDataFileDescSlot * aSlot,
                                       UShort                   alockListID );

    /*DataFile Desc slot�� �Ҵ�� ��� BmpExtHdr�� latch�� �����Ѵ�.*/
    static IDE_RC unlockBmpExtHdrLatchX( smriCTDataFileDescSlot * aSlot,
                                         UShort                   alockListID );

    static IDE_RC checkDBName( SChar * aDBName );

    inline static void getIBChunkSize( UInt * aIBChunkSize ) 
    { 
       *aIBChunkSize  = mCTFileHdr.mIBChunkSize; 
    }
    
    inline static UInt calcIBChunkID4DiskPage( scPageID aPageID )
    {
        return SD_MAKE_FPID(aPageID) / mCTFileHdr.mIBChunkSize;
    }

    inline static UInt calcIBChunkID4MemPage( scPageID aPageID )
    {
        return aPageID / mCTFileHdr.mIBChunkSize;
    }

    inline static idBool isAllocDataFileDescSlotID( 
                            smiDataFileDescSlotID * aSlotID )
    {
        idBool sResult;        

        if( (aSlotID->mBlockID != SMRI_CT_INVALID_BLOCK_ID) &&
            (aSlotID->mSlotIdx != SMRI_CT_DATAFILE_DESC_INVALID_SLOT_IDX) )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }

        return sResult;
    }

    inline static idBool isValidDataFileDescSlot4Mem( 
                                    smmDatabaseFile          * aDatabaseFile, 
                                    smiDataFileDescSlotID    * aSlotID )
    {
        idBool                      sResult;
        smriCTDataFileDescSlot    * sSlot;
        scSpaceID                   sSpaceID;
        UInt                        sFileNum;
        
        getDataFileDescSlot( aSlotID, &sSlot );

        sFileNum    = aDatabaseFile->getFileNum();
        sSpaceID    = aDatabaseFile->getSpaceID(); 
        
        if( ( sSlot->mSpaceID == sSpaceID ) &&
            ( sSlot->mFileID  == sFileNum ) )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
        
        return sResult;
    }

    inline static idBool isValidDataFileDescSlot4Disk( 
                                    sddDataFileNode          * aFileNode,
                                    smiDataFileDescSlotID    * aSlotID )
    {
        idBool                      sResult;
        smriCTDataFileDescSlot    * sSlot;
        
        getDataFileDescSlot( aSlotID, &sSlot );

        if( ( sSlot->mSpaceID == aFileNode->mSpaceID ) &&
            ( sSlot->mFileID == aFileNode->mID ) )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
        
        return sResult;
    }

    static IDE_RC changeTracking4WriteDiskPage( sddDataFileNode * aDataFileNode,
                                                scPageID          aPageID );
};
#endif //_O_SMRI_CT_MANAGER_H
