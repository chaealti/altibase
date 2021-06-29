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
 * $Id: sdrRedoMgr.h 89217 2020-11-11 12:31:50Z et16 $
 *
 * Description :
 *
 * �� ������ DRDB �������������� ����� �����ڿ� ���� ��� �����̴�.
 *
 * # ����
 *
 * DRDB�� ���� restart recovery ������ redoAll ������ ȿ��������
 * ó���ϱ� ���� �ڷᱸ���̴�.
 *
 * - redoAll ���������� �ǵ��� DRDB���� redo �α׵��� �Ľ��Ͽ� ������
 *   tablespace�� page�� �ݿ��ؾ� �Ѵ�.
 *   (physiological �α�)
 *
 * - ������ tablespace�� ���� page�� �ش��ϴ� redo �α׸�
 *   ��Ƽ� �Ѳ����� �ݿ��ϴ� ���� buffer �Ŵ����� I/O�� ���� �� �ִ�
 *   ������ ������ �ȴ�.
 *   (hash table ������� ����)
 *
 * # ����� ������ ����
 *
 *     ����� ������
 *     ________________       ______
 *     |_______________| -----|_____| �α��Ľ̹���
 *
 *
 *    restart recovery   1�� Hash
 *                       smuHash  sdrRedoRecvNode
 *                     ______|____  _________ __________ __________
 *                     |_________|--|(0,5)  |-| (2,10) |-| (3,25) |
 *                     |_________|  |_______| |________| |__*_____|
 *                     |_________|      |          |
 *                     |_________|      O          O sdrHashLogData
 *                     |_________|      O          O
 *                     |_________|                 O
 *                     |_________|--��-��
 *                     |_________|
 *                     |_________|--��-��-��-��
 *
 *    media recovery     2�� hash (spaceID, fileID)
 *                       smuHash  sdrRecvFileHashNode
 *                     ____|______  _________ _________
 *                     |_________|--| (1,0) |-| (2,0) |
 *                     |_________|  |from_to| |from_to|
 *                     |_________|
 *                     |_________|
 *                     |_________|
 *                     |_________|
 *                     |_________|--��-��
 *                     |_________|
 *                     |_________|--��-��-��-��
 *
 *  - ����� ������
 *    DRDB�� ���� redoAll������ �����ϴ� �ڷᱸ��
 *    redoAll �ܰ迡 �ʿ��� ���� ���� ���� �����Ѵ�.
 *
 *  - 1 �� Hash : ���ηα� ����
 *    restart recover �� media recovery�� ����ϴ� hash�̸�, hashkey�� (space ID, page ID) �̴�
 *
 *  - 2 �� Hash : ������������ ����
 *    media recovery�� ����ϴ� hash�̸�, hashkey�� (space ID, file ID) �̴�.
 *
 *  - sdrRedoHashNode
 *    hash ���Ͽ� chain���� ����� �ڷᱸ���̸�,Ư�� (space ID, pageID)
 *    �� ���� redo log�� ���Ḯ��Ʈ�� �����Ѵ�.
 *
 *  - sdrRedoLogData
 *    redo �α� ������ ���� �α׷��ڵ带 �����Ѵ�.
 *
 *  - sdrRecvFileHashNode
 *    ���� ����� ����Ÿ���Ͽ� ���� ���������� ������. 1�� �ؽ���
 *    HashNode�� �ش� ����� �����͸� �����Ͽ�, Media recovery�� ����
 *    �Ѵ�.
 **********************************************************************/

#ifndef _O_SDR_REDO_MGR_H_
#define _O_SDR_REDO_MGR_H_ 1

#include <smu.h>
#include <sdd.h>
#include <sdrDef.h>

class sdrRedoMgr
{
public:

    /* mutex, �ؽ����̺���� �ʱ�ȭ�ϴ� �Լ� */
    static IDE_RC initialize(UInt            aHashTableSize,
                             smiRecoverType  aRecvType);

    /* ����� �������� ���� */
    static IDE_RC destroy();

    /* PROJ-2162 RestartRiskReduction
     * RedoLogBody�� ��ȸ�ϸ� sdrRedoLogDataList�� ����� ��ȯ�� */
    static IDE_RC generateRedoLogDataList( smTID             aTransID,
                                           SChar           * aLogBuffer,
                                           UInt              aLogBufferSize,
                                           smLSN           * aBeginLSN,
                                           smLSN           * aEndLSN,
                                           void           ** aLogDataList );

    /* RedoLogList�� Free�� */
    static IDE_RC freeRedoLogDataList( void            * aLogDataList );

    /* PROJ-2162 RestartRiskReduction
     * OnlineREDO�� ���� DRDB Page�� ���� ������ */
    static IDE_RC generatePageUsingOnlineRedo( idvSQL     * aStatistics,
                                               scSpaceID    aSpaceID,
                                               scPageID     aPageID,
                                               idBool       aReadByDisk,
                                               UChar      * aOutPagePtr,
                                               idBool     * aSuccess );

    static IDE_RC addRedoLogToHashTable( void * aLogDataList );

    /* �ؽ� ���̺� ����� redo log��
       ������ �������� �ݿ� */
    static IDE_RC applyHashedLogRec(idvSQL * aStatistics);

    /* PROJ-2162 RestartRiskReduction
     * List�� ����� Log���� ��� �ݿ��Ѵ�. */
    static IDE_RC applyListedLogRec( sdrMtx         * aMtx,
                                     scSpaceID        aTargetSID,
                                     scPageID         aTargetPID,
                                     UChar          * aPagePtr,
                                     smLSN            aLSN,
                                     sdrRedoLogData * aLogDataList );


    /* BUGBUG - 9640 FILE ���꿡 ���� redo */
    static IDE_RC  redoFILEOPER(smTID        aTransID,
                                scSpaceID    aSpaceID,
                                UInt         aFileID,
                                UInt         aFOPType,
                                UInt         aAImgSize,
                                SChar*       aLogRec);

    /* BUGBUG - 7983 MRDB runtime memory�� ���� redo */
    static void redoRuntimeMRDB( void   * aTrans,
                                 SChar  * aLogRec );

    // Redo Log List���� Redo Log Data�� ��� �����Ѵ�.
    static IDE_RC clearRedoLogList( sdrRedoHashNode*  aRedoHashNode );

    /* ������ datafile�� �м��Ͽ� hash�� ���� */
    static IDE_RC addRecvFileToHash( sddDataFileHdr*   aDBFileHdr,
                                     SChar*            aFileName,
                                     smLSN*            aRedoFromLSN,
                                     smLSN*            aRedoToLSN );

    /* ���� hash��忡 �ش��ϴ� redo �α����� �˻� */
    static IDE_RC filterRecvRedoLog( sdrRecvFileHashNode* aHashNode,
                                     smLSN*               aBeginLSN,
                                     idBool*              aIsRedo );

    /* ���� ��������� �����ϰ� Hash Node ���� */
    static IDE_RC repairFailureDBFHdr( smLSN* aResetLogsLSN );

    /* ���� ������ Hash Node�� ���� */
    static IDE_RC removeAllRecvDBFHashNodes();

    static smiRecoverType getRecvType()
        { return mRecvType; }

    static void writeDebugInfo();

    /* PROJ-1923 private -> public ��ȯ */
    static idBool validateLogRec( sdrLogHdr * aLogHdr );
    /* BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�. */
    static smLSN     getLastUpdatedPageLSN() { return mLstDRDBPageUpdateLSN; };
    static scSpaceID getLastUpdatedSpaceID() { return mLstUpdatedSpaceID; };
    static scPageID  getLastUpdatedPageID()  { return mLstUpdatedPageID; };

private:

    static IDE_RC lock() { return mMutex.lock( NULL ); }
    static IDE_RC unlock() { return mMutex.unlock(); }

    /* ������  mParseBuffer�� ����� �α׸� �Ľ��� ��,
       �ؽ� ���̺� ���� */
    static void parseRedoLogHdr( SChar       * aLogRec,
                                 sdrLogType  * aLogType,
                                 scGRID      * aLogGRID,
                                 UInt        * aValueLen );


    static IDE_RC applyLogRec( sdrMtx         * aMtx,
                               UChar          * aPagePtr,
                               sdrRedoLogData * aLogData );

    /* BUGBUG - 7983 runtime memory�� ���� redo */
    static void applyLogRecToRuntimeMRDB( void*      aTrans,
                                          sdrLogType aLogType,
                                          SChar*     aValue,
                                          UInt       aValueLen );

    /* �ؽ� �Լ� */
    static UInt genHashValueFunc( void* aRID );

    /* �� �Լ� */
    static SInt compareFunc(void* aLhs, void* aRhs);

private:

    /* hashed log�� �������� �ݿ��� ��� ó���� ���� ������ �����ϴ� ����
       �ؽÿ� ������ �����ϱ� ���� ���� */
    static iduMutex          mMutex;
    static smiRecoverType    mRecvType;

    /* �Ľ̵� redo log�� �����ϴ� �ؽ� ����ü (1�� hash)*/
    static smuHashBase   mHash;

    /* ������ ����Ÿ���ϳ�带 �����ϴ� �ؽ� (2�� hash) */
    static smuHashBase   mRecvFileHash;

    /* �ؽ� ũ�� */
    static UInt          mHashTableSize;

    static UInt          mApplyPageCount; /* ������ Page ���� */

    /* 1���ؽ��� hash node�� ������ �ʿ���� ����� ����
       Ư�� �Ӱ�ġ�� �ٴٸ��� ��� �����ϰ� �ȴ� */
    static UInt          mNoApplyPageCnt;
    static UInt          mRecvFileCnt;    /* ������ ����Ÿ���� hash node ���� */

    /* for properties */
    static UInt          mMaxRedoHeapSize;    /* maximum heap memory */

    /* PROJ-2118 BUG Reporting - Debug Info for Fatal */
    static scSpaceID       mCurSpaceID;
    static scPageID        mCurPageID;
    static UChar         * mCurPagePtr;
    static sdrRedoLogData* mRedoLogPtr;
    static smLSN           mLstDRDBPageUpdateLSN;   // DRDB WAL Check�� ����
    static sddDataFileNode *mLstUpdatedDataFileNode;
    static scPageID         mLstUpdatedPageID;
    static scSpaceID        mLstUpdatedSpaceID;
};

#endif // _O_SDR_REDO_MGR_H_

