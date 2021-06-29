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
 * $Id: smrLogAnchorMgr.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * �α׾�Ŀ���� ������ ( system control ���� )
 *
 * # ����
 *
 * ����Ÿ���̽��� �����ÿ� �ʿ��� ������ �����ϴ�
 * Loganchor ������
 *
 * # ����
 *
 * 1. Loganchor ����
 *
 *   ������ ���� 3�κ����� �����ȴ�.
 *
 *   - Header : smrLogAnchor ����ü�� ǥ���Ǹ�,
 *              üũ���� tablespace �������� �����Ѵ�
 *   - Body   : ��� tablespace�� ������ �����Ѵ� (��������)
 *
 * 2. �ټ����� Loganchor ����
 *
 *   Loganchor ������ �����Ҷ����� ���ÿ� ���� ���� Loganchor ���Ͽ�
 *   ���Ͽ� flush�� �����Ͽ�, �ý��� ���ܻ�Ȳ���� ���ؼ� �ϳ��� Loganchor��
 *   �������� �ٸ� Loganchor ���纻�� ���Ͽ� �ý��� ������ �����ϴ�
 *
 *   - altibase�� ��� : altibase.properties ������ CREATE DATABASE ��������
 *                       �⺻ ������Ƽ�� ����Ͽ� ó���Ǹ�, ���� �����ÿ�
 *                       ��ȿ�� �˻縦 �ǽ��Ѵ�.
 *
 * 3. log anchor ����Ǵ� ����
 *
 *   - �ֱ� checkpoint ���� LSN
 *   - �ֱ� checkpoint �Ϸ�
 *   - ������ LSN
 *   - Stable Database ���� ��ȣ
 *   - ��������� ������ ������ �α����� ��ȣ
 *   - �����������
 *   - �� database ���� ����
 *   - ������ �α����Ϲ��� (���۷α׹�ȣ ~ ���α׹�ȣ)
 *   - �α� ����
 *   - tablespace ����
 *
 * 4. flush ������ checksum ���
 *
 *   - ���� ���� offset�� 4bytes������ ����
 *
 *
 * # ���� �ڷ� ����
 *
 *   - smrLogAnchor ����ü
 **********************************************************************/

#ifndef _O_SMR_LOG_ANCHORMGR_H_
#define _O_SMR_LOG_ANCHORMGR_H_ 1

#include <idu.h>
#include <smu.h>
#include <smm.h>
#include <sdd.h>
#include <smrDef.h>
#include <smriDef.h>
#include <smErrorCode.h>

#include <sdsFile.h>

/**
    �α׾�Ŀ Attribute�� ���� �ʱ�ȭ ó�� �ɼ�
 */
typedef enum smrAnchorAttrOption
{
    SMR_AAO_REWRITE_ATTR,
    SMR_AAO_LOAD_ATTR
} smrAnchorAttrOption ;


class smrLogAnchorMgr
{
public:

    /* loganchor �ʱ�ȭ */
    IDE_RC initialize();

    /* process�ܰ迡�� loganchor�ʱ�ȭ */
    IDE_RC initialize4ProcessPhase();

    /* loganchor ���� */
    IDE_RC destroy();

    /* loganchor���� ���� */
    IDE_RC create();

    /* loganchor���� ��� */
    IDE_RC backup( UInt   aWhich,
                   SChar* aBackupFilePath );
    // PRJ-1149.
    IDE_RC backup( idvSQL*  aStatistics,
                   SChar *  aBackupFilePath );

    /* loganchor flush */
    IDE_RC flushAll();
    /*repl recovery LSN ���� proj-1608*/
    IDE_RC updateReplRecoveryLSN( smLSN aReplRecoveryLSN );

    /* loganchor�� checkpoint ���� flush */
    IDE_RC updateChkptAndFlush( smLSN   * aBeginChkptLSN,
                                smLSN   * aEndChkptLSN,
                                smLSN   * aDiskRedoLSN,
                                smLSN   * aEndLSN,
                                UInt    * aFirstFileNo,
                                UInt    * aLastFileNo );

    /* �������� ������ �α������� �����Ѵ� */
    IDE_RC updateSVRStateAndFlush( smrServerStatus  aSvrStatus,
                                   smLSN           * aEndLSN,
                                   UInt            * aLstCrtLog );

    /* �������� �������� �����Ѵ� */
    IDE_RC updateSVRStateAndFlush( smrServerStatus  aSvrStatus );

    /* ��ī�̺��带 �����Ѵ�  */
    IDE_RC updateArchiveAndFlush( smiArchiveMode   aArchiveMode );

    /* ������ ����ϴ� Ʈ����� ��Ʈ�� ������ �α׾�Ŀ�� �����Ѵ�. */
    IDE_RC updateTXSEGEntryCntAndFlush( UInt aEntryCnt );

    /* ��ũ/�޸� Redo LSN�� �����Ѵ�. */
    IDE_RC updateRedoLSN( smLSN * aDiskRedoLSN,
                          smLSN * aMemRedoLSN );

    /* incomplete media recovery�� �����ϴ� ��쿡
       reset��ų log lsn flush */
    IDE_RC updateResetLSN(smLSN *aResetLSN);

    /* ��� Online Tablespace�� ����
       Stable DB�� Switch�ϰ� Log Anchor�� Flush */
    IDE_RC switchAndUpdateStableDB4AllTBS();

    /*  loganchor�� TableSpace ������ �����ϴ� �Լ� */
    IDE_RC updateAllTBSAndFlush();

    IDE_RC updateAllSBAndFlush( void );
    
    /*  loganchor�� Memory TableSpace�� ���� Stable No�� �����ϴ� �Լ� */
    IDE_RC updateStableNoOfAllMemTBSAndFlush();

    /*  loganchor�� TBS Node ������ �����ϴ� �Լ� */
    IDE_RC updateTBSNodeAndFlush( sctTableSpaceNode  * aSpaceNode );

    /*  loganchor�� DBF Node ������ �����ϴ� �Լ� */
    IDE_RC updateDBFNodeAndFlush( sddDataFileNode    * aFileNode );

    /*  loganchor�� Chkpt Inage Node ������ �����ϴ� �Լ� */
    IDE_RC updateChkptImageAttrAndFlush( smmCrtDBFileInfo  * aCrtDBFileInfo,
                                         smmChkptImageAttr * aChkptImageAttr );

    /* ����� �ϳ��� Checkpint Path Node�� Loganchor�� �ݿ��Ѵ�. */
    IDE_RC updateChkptPathAttrAndFlush( smmChkptPathNode * aChkptPathNode );
    
    IDE_RC updateSBufferNodeAndFlush( sdsFileNode    * aFileNode );

    // fix BUG-20241
    /*  loganchor�� FstDeleteFileNo ������ �����ϴ� �Լ� */
    IDE_RC updateFstDeleteFileAndFlush();

    /*  loganchor�� TBS Node ������ �߰��ϴ� �Լ� */
    IDE_RC addTBSNodeAndFlush( sctTableSpaceNode*  aSpaceNode,
                               UInt*               aAnchorOffset );

    /*  loganchor�� DBF Node ������ �߰��ϴ� �Լ� */
    IDE_RC addDBFNodeAndFlush( sddTableSpaceNode* aSpaceNode,
                               sddDataFileNode*   aFileNode,
                               UInt*              aAnchorOffset );

    /*  loganchor�� ChkptPath Node ������ �߰��ϴ� �Լ� */
    IDE_RC addChkptPathNodeAndFlush( smmChkptPathNode*  aChkptPathNode,
                                     UInt*              aAnchorOffset );

    /*  loganchor�� Chkpt Inage Node ������ �߰��ϴ� �Լ� */
    IDE_RC addChkptImageAttrAndFlush( smmChkptImageAttr * aChkptImageAttr,
                                      UInt              * aAnchorOffset );

    IDE_RC addSBufferNodeAndFlush( sdsFileNode  * aFileNode,
                                   UInt         * aAnchorOffset );
    
    /* BUG-39764 : loganchor�� Last Created Logfile Num�� �����ϴ� �Լ� */
    IDE_RC updateLastCreatedLogFileNumAndFlush( UInt aLstCrtFileNo );

    //PROJ-2133 incremental backup
    //logAnchor Buffer�� ����� DataFileDescSlotID�� ��ȯ�Ѵ�.
    IDE_RC getDataFileDescSlotIDFromChkptImageAttr( 
                            UInt                       aReadOffset,                   
                            smiDataFileDescSlotID    * aDataFileDescSlotID );

    //logAnchor Buffer�� ����� DataFileDescSlotID�� ��ȯ�Ѵ�.
    IDE_RC getDataFileDescSlotIDFromDBFNodeAttr( 
                            UInt                       aReadOffset,                   
                            smiDataFileDescSlotID    * aDataFileDescSlotID );

    //PROJ-2133 incremental backup
    IDE_RC updateCTFileAttr( SChar          * aFileName,
                             smriCTMgrState * aCTMgrState,
                             smLSN          * aFlushLSN );

    /* BUG-43499  online backup�� restore�� �ʿ��� �ּ�(����)�� �α׸� Ȯ��
     * �����ؾ� �մϴ�. */
    IDE_RC updateMediaRecoveryLSN( smLSN * aMediaRecoveryLSN );

    inline SChar * getCTFileName()
        { return mLogAnchor->mCTFileAttr.mCTFileName; }

    inline smriCTMgrState getCTMgrState()
        { return mLogAnchor->mCTFileAttr.mCTMgrState; }

    inline smLSN getCTFileLastFlushLSN()
        { return mLogAnchor->mCTFileAttr.mLastFlushLSN; }

    //PROJ-2133 incremental backup
     IDE_RC updateBIFileAttr( SChar          * aFileName,
                              smriBIMgrState * aBIMgrState, 
                              smLSN          * aBackupLSN,
                              SChar          * aBackupDir,
                              UInt           * aDeleteArchLogFileNo );

    inline SChar * getBIFileName()
        { return mLogAnchor->mBIFileAttr.mBIFileName; }

    inline smriBIMgrState  getBIMgrState()
        { return mLogAnchor->mBIFileAttr.mBIMgrState; }

    inline smLSN getBIFileLastBackupLSN()
        { return mLogAnchor->mBIFileAttr.mLastBackupLSN; }

    inline smLSN getBIFileBeforeBackupLSN()
        { return mLogAnchor->mBIFileAttr.mBeforeBackupLSN; }

    inline SChar * getIncrementalBackupDir()
        { return mLogAnchor->mBIFileAttr.mBackupDir; }

    // PRJ-1548 User Memory Tablespace
    // interfaces scan loganchor
    // file must be opened
    static IDE_RC readLogAnchorHeader( iduFile *      aLogAnchorFile,
                                       UInt *         aCurOffset,
                                       smrLogAnchor * aHeader );

    // Loganchor�κ��� ���������� ù��° ��� �Ӽ� Ÿ�� ��ȯ
    static IDE_RC getFstNodeAttrType( iduFile *          aLogAnchorFile,
                                      UInt            *  aBeginOffset,
                                      smiNodeAttrType *  aAttrType );

    // Loganchor�κ��� ���� ��� �Ӽ� Ÿ�� ��ȯ
    static IDE_RC getNxtNodeAttrType( iduFile *          aLogAnchorFile,
                                      UInt               aNextOffset,
                                      smiNodeAttrType *  aNextAttrType );

    // Loganchor�κ��� TBS ��� �Ӽ� ��ȯ
    static IDE_RC readTBSNodeAttr( iduFile *           aLogAnchorFile,
                                   UInt *              aCurOffset,
                                   smiTableSpaceAttr * aTBSAttr );

    // Loganchor�κ��� DBF ��� �Ӽ� ��ȯ
    static IDE_RC readDBFNodeAttr( iduFile *           aLogAnchorFile,
                                    UInt *              aCurOffset,
                                    smiDataFileAttr *   aFileAttr );

    // Loganchor�κ��� CPATH ��� �Ӽ� ��ȯ
    static IDE_RC readChkptPathNodeAttr(
                           iduFile *          aLogAnchorFile,
                           UInt *             aCurOffset,
                           smiChkptPathAttr * aChkptPathAttr );

    // Loganchor�κ��� Checkpoint Image �Ӽ� ��ȯ
    static IDE_RC readChkptImageAttr(
                           iduFile *           aLogAnchorFile,
                           UInt *              aCurOffset,
                           smmChkptImageAttr * aChkptImageAttr );

    // PROJ-2102 Loganchor�κ��� Secondary Buffer Image �Ӽ� ��ȯ
    static IDE_RC readSBufferFileAttr(
                               iduFile               * aLogAnchorFile,
                               UInt                  * aCurOffset,
                               smiSBufferFileAttr    * aFileAttr );

    // checkpoint image attribute size
    static UInt   getChkptImageAttrSize()
        { return ID_SIZEOF( smmChkptImageAttr ); }


    // interfaces for utils to scan loganchor

    /* loganchor���� checkpoint begin �α��� LSN ��ȯ */
    inline smLSN           getBeginChkptLSN()
        {return mLogAnchor->mBeginChkptLSN;}

    /* loganchor���� checkpoint end �α��� LSN ��ȯ */
    inline smLSN           getEndChkptLSN()
        {return mLogAnchor->mEndChkptLSN;}

    /* loganchor�� ���̳ʸ� ���� ID ��ȯ */
    inline UInt            getSmVersionID()
        { return mLogAnchor->mSmVersionID; }

    /* loganchor�� Disk Redo LSN ��ȯ */
    inline smLSN           getDiskRedoLSN()
        { return mLogAnchor->mDiskRedoLSN; }

    /* loganchor�� reset logs ��ȯ */
    inline void          getResetLogs(smLSN *aLSN)
        {
            SM_GET_LSN( *aLSN, mLogAnchor->mResetLSN );
        }

    /* loganchor���� end LSN ��ȯ */
    inline void          getEndLSN(smLSN* aLSN)
        {
            SM_GET_LSN( *aLSN, mLogAnchor->mMemEndLSN );
        }

    /* loganchor���� �������°� ��ȯ */
    inline smrServerStatus getStatus()
        { return mLogAnchor->mServerStatus; }

    /* loganchor���� �������°� ��ȯ */
    inline UInt getTXSEGEntryCnt()
        { return mLogAnchor->mTXSEGEntryCnt; }

    /* loganchor���� ����� ������ �α׹�ȣ ��ȯ */
    inline UInt            getLstLogFileNo()
        { return mLogAnchor->mMemEndLSN.mFileNo; }

    /* loganchor���� ������ ������ �α����� ��ȣ ��ȯ */
    inline void            getLstCreatedLogFileNo(UInt *aFileNo)
        {
            *aFileNo = mLogAnchor->mLstCreatedLogFileNo;
        }
    /* loganchor���� ���� ���� ��ȯ */
    inline UInt getLogAnchorFileCount()
        { return (SMR_LOGANCHOR_FILE_COUNT); }

    /* loganchor���� ���� path ��ȯ */
    inline SChar* getLogAnchorFilePath(UInt aWhichAnchor);
    /* loganchor���� �ش� database ���� ���� ��ȯ */
    inline UInt getDBFileCount(UInt aWhichDB);
    /* loganchor���� �������� �����ϱ� ������ �α����Ϲ�ȣ ��ȯ */
    inline void getFstDeleteLogFileNo(UInt* aFileNo);
    /* loganchor���� �������� �����ϱ� �� �α����Ϲ�ȣ ��ȯ */
    inline void getLstDeleteLogFileNo(UInt* aFileNo);

    /* loganchor���� �������� �����ϱ� ������ �α����Ϲ�ȣ ��ȯ */
    inline UInt  getFstDeleteLogFileNo();
    /* loganchor���� �������� �����ϱ� �� �α����Ϲ�ȣ ��ȯ */
    inline UInt  getLstDeleteLogFileNo();

    /* loganchor���� archive �α� ��� ��ȯ */
    inline smiArchiveMode getArchiveMode() {
                          return mLogAnchor->mArchiveMode; }

    /* proj-1608 recovery from replication repl recovery LSN�� ���� */
    inline smLSN  getReplRecoveryLSN() { return mLogAnchor->mReplRecoveryLSN; }

    /*  ���� loganchor���� �߿��� ��ȿ�� loganchor���Ϲ�ȣ ��ȯ */
    static IDE_RC checkAndGetValidAnchorNo(UInt* aWhich);

    /* Check Log Anchor Dir Exist */
    static IDE_RC checkLogAnchorDirExist();

    smrLogAnchorMgr();
    virtual ~smrLogAnchorMgr();

private:
    // �α׾�Ŀ�κ��� Tablespace�� Ư�� Ÿ���� Attribute���� �ε��Ѵ�.
    IDE_RC readAllTBSAttrs( UInt aWhichAnchor,
                            smiNodeAttrType  aAttrTypeToLoad );

    // �α׾�Ŀ ���ۿ� �α׾�Ŀ Attribute����
    // Valid�� Attribute�� ����� �ٽ� ����Ѵ�.
    IDE_RC readLogAnchorToBuffer(UInt aWhichAnchor);

   // �� Attribute���� �ʱ�ȭ �Ѵ�.
    IDE_RC readAttrFromLogAnchor( smrAnchorAttrOption aAttrOp,
                                  smiNodeAttrType     aAttrType,
                                  UInt                aWhichAnchor,
                                  UInt              * aReadOffset );

    // �� attr�� size�� ���ϴ� �Լ�
    UInt getAttrSize( smiNodeAttrType aAttrType );

    // TBS SpaceNode���� TBS Attr�� anchorOffset�� ��ȯ�Ѵ�.
    IDE_RC getTBSAttrAndAnchorOffset(
        scSpaceID          aSpaceID,
        smiTableSpaceAttr* aSpaceAttr,
        UInt             * aAnchorOffset );

    // log anchor buffer�� �� node���� �� �˻��Ѵ�.
    idBool checkLogAnchorBuffer();

    // log anchor buffer�� �������� Space Node�� �˻�
    idBool checkTBSAttr(  smiTableSpaceAttr* aSpaceAttrByAnchor,
                          UInt               aOffsetByAnchor );

    // log anchor buffer�� �������� DBFile Node�� �˻�
    idBool checkDBFAttr( smiDataFileAttr*   aFileAttrByAnchor,
                         UInt               aOffsetByAnchor );

    // log anchor buffer�� �������� Checkpoint Path Node�� �˻�
    idBool checkChkptPathAttr( smiChkptPathAttr*  aCPPathAttrByAnchor,
                               UInt               aOffsetByAnchor );

    // log anchor buffer�� �������� Checkpoint Image Node�� �˻�
    idBool checkChkptImageAttr( smmChkptImageAttr* aCPImgAttrByAnchor,
                                UInt               aOffsetByAnchor );

    // log anchor buffer�� �������� SeondaryBufferFile Node�� �˻�
    idBool checkSBufferFileAttr( smiSBufferFileAttr* aFileAttrByAnchor,
                                 UInt                aOffsetByAnchor );

    // Tablespace attritube�� �������� �˻�
    idBool cmpTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                              smiTableSpaceAttr* aSpaceAttrByAnchor );

    // Disk Tablespace attritube�� �������� �˻�
    void cmpDiskTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                                smiTableSpaceAttr* aSpaceAttrByAnchor,
                                idBool             sIsEqual,
                                idBool           * sIsValid );

    // Memory Tablespace attritube�� �������� �˻�
    void cmpMemTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                               smiTableSpaceAttr* aSpaceAttrByAnchor,
                               idBool             sIsEqual,
                               idBool           * sIsValid );

    // Volatile Tablespace attritube�� �������� �˻�
    void cmpVolTableSpaceAttr( smiTableSpaceAttr* aSpaceAttrByNode,
                               smiTableSpaceAttr* aSpaceAttrByAnchor,
                               idBool             sIsEqual,
                               idBool           * sIsValid );

    // DBFile attritube�� �������� �˻�
    idBool cmpDataFileAttr( smiDataFileAttr*   aFileAttrByNode,
                            smiDataFileAttr*   aFileAttrByAnchor );

    // Checkpoint Image attritube�� �������� �˻�
    idBool cmpChkptImageAttr( smmChkptImageAttr*   aImageAttrByNode,
                              smmChkptImageAttr*   aImageAttrByAnchor );

    // DBFile attritube�� �������� �˻�
    idBool cmpSBufferFileAttr( smiSBufferFileAttr*   aFileAttrByNode,
                               smiSBufferFileAttr*   aFileAttrByAnchor );


    // �޸�/��ũ ���̺����̽� �ʱ�ȭ
    IDE_RC initTableSpaceAttr( UInt                  aWhich,
                               smrAnchorAttrOption   aAttrOp,
                               UInt                * aReadOffset );

    // ��ũ ����Ÿ���� ��Ÿ��� �ʱ�ȭ
    IDE_RC initDataFileAttr( UInt                  aWhich,
                             smrAnchorAttrOption   aAttrOp,
                             UInt                * aReadOffset );

    // �޸� üũ����Ʈ PATH �ʱ�ȭ
    IDE_RC initChkptPathAttr( UInt                  aWhich,
                             smrAnchorAttrOption    aAttrOp,
                              UInt                * aReadOffset );

    // �޸� ����Ÿ���� ��Ÿ��� �ʱ�ȭ
    IDE_RC initChkptImageAttr( UInt                  aWhich,
                               smrAnchorAttrOption   aAttrOp,
                               UInt                * aReadOffset );

    IDE_RC initSBufferFileAttr( UInt                aWhich,
                                smrAnchorAttrOption aAttrOp,
                                UInt              * aReadOffset );

    /* FOR A4 */
    inline IDE_RC lock();
    inline IDE_RC unlock();

    /* loganchor ���� �ʱ�ȭ */
    inline IDE_RC allocBuffer(UInt  aBufferSize);

    /* loganchor ���� offset �ʱ�ȭ */
    inline void   initBuffer();

    /* loganchor ���� resize  */
    IDE_RC resizeBuffer( UInt aBufferSize );

    /* loganchor ���ۿ� ��� */
    IDE_RC writeToBuffer( void* aBuffer,
                          UInt  aWriteSize );

    /* loganchor ���ۿ� ��� */
    IDE_RC updateToBuffer( void *aBuffer,
                           UInt  aOffset,
                           UInt  aWriteSize );

    /* loganchor ���� ���� */
    IDE_RC freeBuffer();

    /* TBS Attr�� ����� offset */
    UInt getTBSAttrStartOffset();

    /* checksum ��� �Լ� */
    static inline UInt   makeCheckSum(UChar*  aBuffer,
                                      UInt    aBufferSize);

public:

    /* loganchor���Ͽ� ���� I/O ó�� */
    iduFile            mFile[SMR_LOGANCHOR_FILE_COUNT];
    /*  mBuffer�� ����� loganchor �ڷᱸ���� ���� ������ */
    smrLogAnchor*      mLogAnchor;

    /* ------------------------------------------------
     * �α׾�Ŀ�� �����ϱ� ���� �ڷᱸ��
     * ----------------------------------------------*/
    // �޸�/��ũ ���̺����̽� �Ӽ�
    smiTableSpaceAttr  mTableSpaceAttr;
    // ��ũ ����Ÿ���� �Ӽ�
    smiDataFileAttr    mDataFileAttr;
    // �޸� üũ����Ʈ Path �Ӽ�
    smiChkptPathAttr   mChkptPathAttr;
    // �޸� ����Ÿ���� �Ӽ�
    smmChkptImageAttr  mChkptImageAttr;
    // Secondary Buffer ���� �Ӽ�
    smiSBufferFileAttr mSBufferFileAttr;
    /* ------------------------------------------------
     * ���������� loganchor ���Ͽ� write�ϱ� ���� ����
     * ----------------------------------------------*/
    UChar*            mBuffer;
    UInt              mBufferSize;
    UInt              mWriteOffset;

    /* ------------------------------------------------
     * A3������ ������ �Ͼ�� ��Ȳ�� ��� mutex��
     * ��� ������, A4������ tablespace���� DDL�� ����鼭
     * loganchor�� ���ſ� ���ü��� �����Ͽ��� ��
     * ----------------------------------------------*/
    iduMutex           mMutex;

    /* PROJ-2133 incremental backup */
    idBool             mIsProcessPhase;

    /* for property */
    static SChar*      mAnchorDir[SMR_LOGANCHOR_FILE_COUNT];

};

/***********************************************************************
 * Description : loganchor�������� mutex ȹ��
 **********************************************************************/
inline IDE_RC smrLogAnchorMgr::lock()
{

    return mMutex.lock( NULL );

}

/***********************************************************************
 * Description : loganchor�������� mutex ����
 **********************************************************************/
inline IDE_RC smrLogAnchorMgr::unlock()
{

    return mMutex.unlock();

}

/***********************************************************************
 * Description : loganchor���� �������� �����ϱ� ������ �α����Ϲ�ȣ ��ȯ
 **********************************************************************/
UInt smrLogAnchorMgr::getFstDeleteLogFileNo()
{
    /* BUG-39289 : FST�� LST ���� ũ�ų� ���� ���� Lst������ �����Ѵ�. 
       FST�� LST + Log Delete Count�̱� �����̴�. */
    
    if ( mLogAnchor->mLstDeleteFileNo >= mLogAnchor->mFstDeleteFileNo )
    {
        return mLogAnchor->mLstDeleteFileNo;
    }
    else
    {
        return mLogAnchor->mFstDeleteFileNo;
    }

}

/***********************************************************************
 * Description : loganchor���� �������� ������ �� �α����Ϲ�ȣ ��ȯ
 **********************************************************************/
UInt smrLogAnchorMgr::getLstDeleteLogFileNo()
{
    return mLogAnchor->mLstDeleteFileNo;
}

/***********************************************************************
 * Description : loganchor���� �������� �����ϱ� ������ �α����Ϲ�ȣ ��ȯ
 **********************************************************************/
void smrLogAnchorMgr::getFstDeleteLogFileNo(UInt* aFileNo)
{
    /* BUG-39289  : FST�� LST ���� ũ�ų� ���� ���� LST������ �����Ѵ�. 
       FST�� LST + Log Delete Count�̱� �����̴�. */ 
    if ( mLogAnchor->mFstDeleteFileNo >= mLogAnchor->mLstDeleteFileNo )
    {
        *aFileNo = mLogAnchor->mLstDeleteFileNo;
    }
    else
    {
        *aFileNo = mLogAnchor->mFstDeleteFileNo;
    }

}

/***********************************************************************
 * Description : loganchor���� �������� ������ �� �α����Ϲ�ȣ ��ȯ
 **********************************************************************/
void smrLogAnchorMgr::getLstDeleteLogFileNo(UInt* aFileNo)
{
    *aFileNo = mLogAnchor->mLstDeleteFileNo;
}


/***********************************************************************
 * Description : loganchor���� ���� path ��ȯ
 **********************************************************************/
SChar* smrLogAnchorMgr::getLogAnchorFilePath(UInt aWhichAnchor)
{

    IDE_DASSERT( aWhichAnchor < SMR_LOGANCHOR_FILE_COUNT );

    return mFile[aWhichAnchor].getFileName();

}

/***********************************************************************
 * Description : loganchor ���۸� �Ҵ���
 **********************************************************************/
IDE_RC smrLogAnchorMgr::allocBuffer(UInt   aBufferSize)
{
    IDE_DASSERT( aBufferSize >= idlOS::align(ID_SIZEOF(smrLogAnchorMgr)));

    mBufferSize = aBufferSize;
    mBuffer     = NULL;

    /* TC/FIT/Limit/sm/smr/smrLogAnchorMgr_allocBuffer_malloc.sql */
    IDU_FIT_POINT_RAISE( "smrLogAnchorMgr::allocBuffer::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SMR,
                                      mBufferSize,
                                      (void**)&mBuffer) != IDE_SUCCESS ,
                    insufficient_memory );

    idlOS::memset(mBuffer, 0x00, mBufferSize);

    mLogAnchor   = (smrLogAnchor*)mBuffer;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : loganchor offset�� �ʱ�ȭ
 **********************************************************************/
void smrLogAnchorMgr::initBuffer()
{
    mWriteOffset = 0;
}

/***********************************************************************
 * Description : mBuffer�� ���� checksum�� ����Ͽ� ��ȯ
 * !!] 32bits or 64bits �Ͽ����� ������ ���� �����ؾ��Ѵ�.
 **********************************************************************/
UInt smrLogAnchorMgr::makeCheckSum(UChar* aBuffer,
                                   UInt   aBufferSize)
{

    SInt sLength;
    UInt sCheckSum;

    IDE_DASSERT( aBuffer != NULL );

    sCheckSum = 0;
    sLength   = (SInt)(aBufferSize - (UInt)ID_SIZEOF(smrLogAnchor));

    IDE_ASSERT( sLength >= 0 );

    sCheckSum = smuUtility::foldBinary((UChar*)(aBuffer + ID_SIZEOF(UInt)),
                                       aBufferSize - ID_SIZEOF(UInt));

    sCheckSum = sCheckSum & 0xFFFFFFFF;

    return sCheckSum;

}


#endif /* _O_SMR_LOG_ANCHORMGR_H_ */
