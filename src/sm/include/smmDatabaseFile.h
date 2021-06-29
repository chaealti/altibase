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
 * $Id: smmDatabaseFile.h 85837 2019-07-14 23:44:48Z emlee $
 **********************************************************************/

#ifndef _O_SMM_DATABASE_FILE_H_
#define _O_SMM_DATABASE_FILE_H_ 1

#include <idu.h>
#include <smDef.h>
#include <smriDef.h>
#include <smmDef.h>

class smmDatabaseFile
{
public:
    // To Fix BUG-18434
    //        alter tablespace online/offline ���ü����� �������
    //
    // Page Buffer�� ���ٿ� ���� ���ü� ��� ���� Mutex
    iduMutex mPageBufferMutex;
    
    UChar *mAlignedPageBuffer;
    UChar *mPageBufferMemPtr;

    // ȭ���� ����� ���
    SChar *mDir;

    // ����Ÿ���� �Ӽ�
    scSpaceID           mSpaceID;       // ���̺����̽� ID
    UInt                mPingPongNum;   // ���� ��ȣ
    UInt                mFileNum;       // ���� ��ȣ

    // Media Recovery �ڷᱸ��
    smmChkptImageHdr    mChkptImageHdr;  // ����Ÿ���� ��Ÿ���
    idBool              mIsMediaFailure; // �̵��������� 
    scPageID            mFstPageID;      // ����Ÿ������ ù��° PID
    scPageID            mLstPageID;      // ����Ÿ������ ������ PID

    // �̵������� Redo LSN
    smLSN               mFromRedoLSN;
public:
    //PROJ-2133 incremental backup
    iduFile     mFile;

private:
    // Disk�� DBFile�� �����ϰ� File�� open�Ѵ�.
    IDE_RC createDBFileOnDiskAndOpen( idBool aUseDirectIO );
    // DBFile Header�� Checkpoint Image Header�� ����Ѵ�.
    IDE_RC setDBFileHeader( smmChkptImageHdr * aChkptImageHdr );

    // üũ����Ʈ �̹��� ���Ͽ� Checkpoint Image Header�� ��ϵ� ��� 
    // �̸� �о Tablespace ID�� ��ġ�ϴ��� ���ϰ� File�� Close�Ѵ�.
    IDE_RC readSpaceIdAndClose( scSpaceID   aSpaceID,
                                idBool    * aIsHeaderWritten,
                                idBool    * aSpaceIdMatches);
    
    // ������ ������ ��� �����Ѵ�.
    // �������� ���� ��� �ƹ��ϵ� ���� �ʴ´�.
    static IDE_RC removeFileIfExist( scSpaceID aSpaceID,
                                     const SChar * aFileName );


    // Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ����
    // File Read����
    IDE_RC readDIO(
               PDL_OFF_T  aWhere,
               void*      aBuffer,
               size_t     aSize);

//    // Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ����
//    // File Write����
//    IDE_RC writeDIO(
//               PDL_OFF_T aWhere,
//               void*     aBuffer,
//               size_t    aSize);

    // Direct I/O�� �̿��Ͽ� write�ϱ� ���� Align�� Buffer�� ������ ����
    IDE_RC copyDataForDirectWrite(
               void * aAlignedDst,
               void * aSrc,
               UInt   aSrcSize,
               UInt * aSizeToWrite );

    // Direct I/O�� ���� Disk Sector Size�� Align�� Buffer�� ����
    // File Write����  ( writeUntilSuccess���� )
    IDE_RC writeUntilSuccessDIO(
                PDL_OFF_T aWhere,
                void*     aBuffer,
                size_t    aSize,
                iduEmergencyFuncType aSetEmergencyFunc,
                idBool aKillServer = ID_FALSE);
    
public:

    smmDatabaseFile();

    IDE_RC initialize( scSpaceID    aSpaceID, 
                       UInt         aPingPongNum,
                       UInt         aFileNum,
                       scPageID   * aFstPageID = NULL,
                       scPageID   * aLstPageID = NULL );

    IDE_RC setFileName(SChar *aFilename);
    IDE_RC setFileName(SChar *aDir,
                       SChar *aDBName,
                       UInt   aPingPongNum,
                       UInt   aFileNum);

    const SChar* getFileName();

    IDE_RC syncUntilSuccess();

    ~smmDatabaseFile();
    IDE_RC destroy();

    /* Database������ �����Ѵ�.*/
    IDE_RC createDbFile (smmTBSNode       * aTBSNode,
                         SInt               aCurrentDB,
                         SInt               aDBFileNo,
                         UInt               aSize,
                         smmChkptImageHdr * aChkptImageHdr = NULL );

    // Checkpoint Image File�� Close�ϰ� Disk���� �����ش�.
    IDE_RC closeAndRemoveDbFile(scSpaceID       aSpaceID, 
                                idBool          aRemoveImageFiles, 
                                smmTBSNode    * aTBSNode );
    
//    IDE_RC readPage     (smmTBSNode * aTBSNode, 
//                         scPageID aPageID );

    IDE_RC readPage     (smmTBSNode * aTBSNode,
                         scPageID     aPageID,
                         UChar *      aPage);

    IDE_RC readPageWithoutCheck(smmTBSNode * aTBSNode,
                                scPageID     aPageID,
                                UChar *      aPage);

    IDE_RC readPages(PDL_OFF_T   aWhere,
                     void*   aBuffer,
                     size_t  aSize,
                     size_t *aReadSize);

    IDE_RC readPagesAIO(PDL_OFF_T   aWhere,
                        void*   aBuffer,
                        size_t  aSize,
                        size_t *aReadSize,
                        UInt    aUnitCount);

    IDE_RC writePage    (smmTBSNode * aTBSNode, scPageID aNum);
    IDE_RC writePage    (smmTBSNode * aTBSNode, 
                         scPageID aNum, 
                         UChar *aPage);
    /************************************
    * PROJ-2133 incremental backup begin
    ************************************/
    IDE_RC createUntilSuccess(iduEmergencyFuncType aSetEmergencyFunc,
                              idBool               aKillServer);

    IDE_RC getFileSize(ULong *aFileSize);

    IDE_RC copy(idvSQL *aStatSQL ,SChar *aFileName);

    IDE_RC close();

    idBool exist();
    /************************************
    * PROJ-2133 incremental backup end
    ************************************/

    idBool isOpen();
    IDE_RC open();

    void setDir(SChar *aDir);
    const SChar* getDir();

    // ���ο� Checkpoint Image������ ������ Checkpoint Path�� �����Ѵ�.
    static IDE_RC makeDBDirForCreate(smmTBSNode * aTBSNode,
                                     UInt   aDBFileNo,
                                     SChar *aDBDir );
    

    static idBool findDBFile(smmTBSNode * aTBSNode,
                             UInt         aPingPongNum,
                             UInt         aFileNum,
                             SChar  *     aFileName,
                             SChar **     aFileDir);


    // Ư�� DB������ Disk�� �ִ��� Ȯ���Ѵ�.
    static idBool isDBFileOnDisk( smmTBSNode * aTBSNode,
                                  UInt aPingPongNum,
                                  UInt aDBFileNo );

    // BUG-29607 Create DB, TBS �� �����̸� ������ �����ϴ�����
    //           ������ ������ �� ���� �����ؼ� Ȯ���մϴ�.
    static IDE_RC chkExistDBFileByNode( smmTBSNode * aTBSNode );
    static IDE_RC chkExistDBFileByProp( const SChar * aTBSName );
    static IDE_RC chkExistDBFile( const SChar * aTBSName,
                                  const SChar * aChkptPath );

    ///////////////////////////////////////////////////////////////
    // PRJ-1548 User Memory TableSpace ���� ����
    // ��� & �̵���

    // ����Ÿ���� ������� ���� 
    void   setChkptImageHdr( smLSN                    * aMemRedoLSN, 
                             smLSN                    * aMemCreateLSN,
                             scSpaceID                * aSpaceID,
                             UInt                     * aSmVersion,
                             smiDataFileDescSlotID    * aDataFileDescSlotID );

    // ����Ÿ���� ������� ��ȯ
    void   getChkptImageHdr( smmChkptImageHdr * aChkptImageHdr );

    // ����Ÿ���� �Ӽ� ��ȯ 
    void   getChkptImageAttr( smmTBSNode        * aTBSNode, 
                              smmChkptImageAttr * aChkptImageAttr );

    // ����Ÿ���� ��Ÿ����� ����Ѵ�. 
    IDE_RC flushDBFileHdr();

    // ����Ÿ���Ϸκ��� ��Ÿ����� �ǵ��Ѵ�.
    IDE_RC readChkptImageHdr( smmChkptImageHdr * aChkptImgageHdr );

    // ����Ÿ������ ��Ÿ����� �ǵ��Ͽ� �̵����� 
    // �ʿ����� �Ǵ��Ѵ�.
    IDE_RC checkValidationDBFHdr( smmChkptImageHdr  * aChkptImageHdr,
                                  idBool            * aIsMediaFailure );

    // Drop �Ϸ��� Checkpoint Path���� Checkpoint Image��
    // ��ȿ�� ��η� �Ű��������� Ȯ���Ѵ�.
    static IDE_RC checkChkptImgInDropCPath( smmTBSNode       * aTBSNode,
                                            smmChkptPathNode * aDropPathNode );

    // ����Ÿ���� HEADER�� ��ȿ���� �˻��Ѵ�. 
    IDE_RC checkValuesOfDBFHdr( smmChkptImageHdr  * aChkptImageHdr );


    // ����Ÿ���� HEADER�� ��ȿ���� �˻��Ѵ�. (ASSERT����)
    IDE_RC assertValuesOfDBFHdr( smmChkptImageHdr*  aChkptImageHdr );
    
    //  �̵����� �÷��׸� �����Ѵ�. 
    void   setIsMediaFailure( idBool   aFlag ) { mIsMediaFailure = aFlag; }
    //  �̵����� �÷��׸� ��ȯ�Ѵ�. 
    idBool getIsMediaFailure() { return mIsMediaFailure; }

    // ������ ����Ÿ���Ͽ� ���� �Ӽ��� �α׾�Ŀ�� �߰��Ѵ�. 
    IDE_RC addAttrToLogAnchorIfCrtFlagIsFalse( smmTBSNode * aTBSNode );

    // �̵��� �÷��� ���� �� ����� ���� ����ϱ�
    IDE_RC prepareMediaRecovery( smiRecoverType        aRecoveryType, 
                                 smmChkptImageHdr    * aChkptImageHdr,
                                 smLSN               * aFromRedoLSN,
                                 smLSN               * aToRedoLSN );

    // ����Ÿ���� ��ȣ ��ȯ
    UInt  getFileNum() { return mFileNum; }

    // ����Ÿ���� ������ ��ȯ
    SChar * getFileDir() { return mDir; }

    //���̺����̽� ��ȣ ��ȯ 
    scSpaceID getSpaceID() { return mSpaceID; }

    // ����Ÿ������ ������������ ��ȯ
    void  getPageRangeInFile( scPageID  * aFstPageID,
                              scPageID  * aLstPageID ) 
    { 
        *aFstPageID = mFstPageID;
        *aLstPageID = mLstPageID;
    }

    // ����Ÿ�����Ͽ� ���Ե�  ������ ���� üũ�Ѵ�. 
    idBool IsIncludePageInFile( scPageID  aPageID )
    {
        if ( (mFstPageID <= aPageID) && 
             (mLstPageID >= aPageID) )
        {
            return ID_TRUE;
        }

        return ID_FALSE;
    }

    /************************************
    * PROJ-2133 incremental backup begin
    ************************************/
    static IDE_RC incrementalBackup( smriCTDataFileDescSlot * aDataFileDescSlot,
                                     smmDatabaseFile        * aDatabaseFile,
                                     smriBISlot             * aBackupInfo );

    static IDE_RC setDBFileHeaderByPath( SChar            * aChkptImagePath,
                                         smmChkptImageHdr * aChkptImageHdr );
           
    IDE_RC isNeedCreatePingPongFile( smriBISlot * aBISlot,
                                     idBool     * aResult );

    static IDE_RC readChkptImageHdrByPath( SChar            * aChkptImagePath,
                                           smmChkptImageHdr * aChkptImageHdr );
    /************************************
    * PROJ-2133 incremental backup end
    ************************************/
};

inline idBool smmDatabaseFile::isOpen()
{
    return ( mFile.getCurFDCnt() == 0 ) ? ID_FALSE : ID_TRUE;
}

inline IDE_RC smmDatabaseFile::getFileSize( ULong * aFileSize )
{
    return mFile.getFileSize( aFileSize );
}

inline IDE_RC smmDatabaseFile::close()
{
    return mFile.close();
}

inline idBool smmDatabaseFile::exist()
{
    return mFile.exist();
}

inline IDE_RC smmDatabaseFile::copy( idvSQL * aStatistics, 
                                     SChar  * aFileName )
{
    return mFile.copy( aStatistics, aFileName );
}


inline IDE_RC smmDatabaseFile::createUntilSuccess( 
                                    iduEmergencyFuncType aSetEmergencyFunc,
                                    idBool aKillServer  )
{
    return mFile.createUntilSuccess( aSetEmergencyFunc, aKillServer );
}

#endif  // _O_SMM_DATABASE_FILE_H_
