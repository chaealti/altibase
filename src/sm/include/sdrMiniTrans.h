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
 * $Id: sdrMiniTrans.h 89495 2020-12-14 05:19:22Z emlee $
 *
 * Description :
 *
 * �� ������ mini-transaction�� ���� ��� �����̴�.
 *
 * # ����
 *
 * mini-transaction�̶� Ư�� �����۾��� ���� page fix�� ���� redo
 * �α׵��� ������ �ξ��ٰ� mini-transaction�� commit������ �����
 * �Ѳ����� �����ߴ� �α׸� �α����Ͽ� write �ϰ�, fix�Ǿ��� page��
 * unfix���Ѽ� actomic�� �����ϱ� ���� ��ü�̴�.
 * ���� mini-transaction�� mtx��� �Ѵ�
 *
 * # ����
 *
 * mtx�� ������ ���Ŀ� ��� ��� ���� �����Ӱ� ���ۿ� ����
 * latch�� mtx�� push�ȴ�.
 * ���� �� ������ �۾� �� �߰��� �״´ٸ� �� �۾��� � ���뵵
 * �α׿� �ݿ����� ������, mtx commit�ÿ��� �α׿� �� ������ �ݿ��ȴ�.
 * mtx commit�ÿ��� ���۵��� unfix�ǹǷ� mtx �߰��� �������� ���� ���
 * ���浵 ��ũ�� �ݿ����� �ʴ´�.
 * ���� �α׿� write�� ���İ� �Ǿ�� ��ũ�� ������ �������� �ݿ��� �� �ִ�.
 * �̴�Ʈ������� start�� commit�ϴ� ������ ������ atomic �ϴٰ�
 * �� �� �ִ�. �̸� ���� mtx ���ο����� �α� ���۸� �����Ѵ�.
 *
 * mtr start -> commit �Ǳ� ���� mtx�� ������ �۾��� �����Ѵ�.
 *    - ������Ʈ�� latch��带 ����
 *    - ���� ������ fix
 *    - �α׸� �α׹��ۿ�  ����
 *
 * commit�Ǹ� �̿� ���� �۾��� �Ѵ�.
 *    - ����� ������Ʈ�� latch ��带 ����
 *    - ���� ���� ������ unfix
 *    - �α׹��۸� ���� log�� write
 *
 * 1) mtx ���� ���
 *      __________
 *      | sdrMtx |_______________________________
 *      |________|                               |
 *     ______|_____________             mtx stack(latch stack)
 *     |                  |               _______|______
 *     | dynamic          |               |_latch item_|
 *     | log buffer       |               |____________|
 *     |                  |               |____________|
 *     |                  |               |____________|
 *     |__________________|               |____________|
 *
 *
 *    - sdrMtx
 *    mtx�� ��� ������ �����Ѵ�. atomic�� �����ؾ� �ϴ�
 *    �۾��� ���ؼ� mtx�� �����Ѵ�. ������ ���� ������Ұ� �ִ�.
 *
 *    1) log buffer
 *    dynamic ����(smuDynArray)�� mtx start ���Ŀ� ���꿡 ���� �߻��ϴ�
 *    �α׸� ����Ѵ�. mtx commit �ÿ� ���� �α����Ϲ��ۿ� write �Ѵ�.
 *
 *    2) sdrMtxStackInfo(Latch Stack)
 *    latch item�� �׾Ƶд�. �� item���� commit�ÿ� ��� �����ȴ�.
 *
 *    - sdrMtxLatchItem
 *    latchStack�� ����Ǵ� �����̴�. ������Ʈ�� latch ���� ������
 *    ������Ʈ�δ� BCB, ��ġ, ���ؽ��� �� �� �ִ�.
 *
 * 2) mtx�� ����Ǵ� DRDB�� �α��� ����
 *
 *  __header(16bytes)_  _________body_____
 * /__________________\/__________________\
 * | type | RID | len | log-specipic body |
 * |______|_____|_____|___________________|
 *
 * mtx start ���� �̷��� �αװ� �Ѱ� �̻� �ݺ��Ǿ� commit �� ������
 * ���δ�. mtx commit �ÿ� �α� ���۴� �α� �����ڿ� ���޵ȴ�.
 *
 * 3) �α� �����ڰ� ����ϴ� ���� �α��� ����
 *
 *  ______________ header__________________  ____body_________  tail
 * /_______________________________________\/_______________ _\/_____\
 * | type | TxID | flag | previous undo LSN | �������� Mtx Log |      |
 * |______|______|______|___________________|__________________|______|
 *
 * ���� �α�ȭ�Ͽ� ���̴� �α״� header�� ���� ���� DRDB �α׷� ������
 * body, �׸��� tail�� �����ȴ�.
 * �α� header�� ������ �����ϱ� ���ؼ��� type�� TxID�� �ʿ��ϸ� �̸�
 * mtx start�� �޾Ƽ� commit�� �α׿� write�� �� �����Ѵ�.
 *
 **********************************************************************/

#ifndef _O_SDR_MINI_TRANS_H_
#define _O_SDR_MINI_TRANS_H_ 1

#include <sdrDef.h>

class sdrMiniTrans
{
public:

    /* mtx �ʱ�ȭ �� ���� */
    static IDE_RC begin( idvSQL*       aStatistics,
                         sdrMtx*       aMtx,
                         void*         aTrans,
                         sdrMtxLogMode aLogMode,
                         idBool        aUndoable,
                         UInt          aDLogAttr );


    static IDE_RC begin( idvSQL*          aStatistics,
                         sdrMtx*          aMtx,
                         sdrMtxStartInfo* aStartInfo,
                         idBool           aUndoable,
                         UInt             aDLogAttr );


    /* ------------------------------------------------
     * !!] mtx�� ȹ���� latchitem�� release�ϸ�, �ۼ���
     * �α׸� �α����Ͽ� ����ϰ�, ��� resource�� �����Ѵ�
     * ----------------------------------------------*/
    static IDE_RC commit( sdrMtx * aMtx,
                          UInt     aContType = 0,      /* in */
                          smLSN  * aEndLSN   = NULL,   /* in */
                          UInt     aRedoType = 0 );    /* in */

    static IDE_RC setDirtyPage(void*    aMtx,
                               UChar*   aPagePtr );

    /* �Ϲ� �������ӿ��� log�� �ȳ��涧 ����. (ex: Index Bottom-up Build) */
    static void setNologgingPersistent( sdrMtx* aMtx );

    static void makeStartInfo( sdrMtx* aMtx,
                               sdrMtxStartInfo * aStartInfo );

    /* Ư�� ���꿡 ���� NTA �α� ���� */
    static void setNTA( sdrMtx*   aMtx,
                        scSpaceID aSpaceID,
                        UInt      aOpType,
                        smLSN*    aPPrevLSN,
                        ULong   * aArrData,
                        UInt      aDataCount );

    static void setRefNTA( sdrMtx*   aMtx,
                           scSpaceID aSpaceID,
                           UInt      aOpType,
                           smLSN*    aPPrevLSN );

    static void setNullNTA( sdrMtx   * aMtx,
                            scSpaceID  aSpaceID,
                            smLSN*     aPPrevLSN );

    static void setCLR(sdrMtx* aMtx,
                       smLSN*  aPPrevLSN);

    /* Rollback�ϴ��� Undo�� ���ص� �� */
    static void setIgnoreMtxUndo( sdrMtx* aMtx )
    {
        /* BUG-32579 The MiniTransaction commit should not be used in
         * exception handling area.
         * 
         * �κ������� �αװ� ������ ��Ȳ������ MtxUndo�� ������ ��Ȳ
         * �� �ƴϴ�. */ 
        IDE_ASSERT( aMtx->mRemainLogRecSize == 0 );

        aMtx->mFlag &= ~SDR_MTX_IGNORE_UNDO_MASK;
        aMtx->mFlag |= SDR_MTX_IGNORE_UNDO_ON;
    }

    /* DML���� redo/undo �α� ��ġ�� ����Ѵ� */
    static void setRefOffset(sdrMtx* aMtx,
                             smOID   aTableOID = SM_NULL_OID);

    static void* getTrans( sdrMtx *aMtx );
    static void* getMtxTrans( void *aMtx );

    static void* getStatSQL( void * aMtx ) { return (void*)((sdrMtx*)aMtx)->mStatSQL; }

    /* mtx�� �α��带 ��ȯ */
    static sdrMtxLogMode getLogMode(sdrMtx*        aMtx);

    static sdrMtxLoggingType getLoggingType(sdrMtx*        aMtx);

    static UInt getDLogAttr(sdrMtx *aMtx);

    /* page ����κп� ���� �α׸� ����Ѵ�. */
    static IDE_RC writeLogRec(sdrMtx*      aMtx,
                              UChar*       aWritePtr,
                              void*        aValue,
                              UInt         aLength,
                              sdrLogType   aLogType );

    static IDE_RC writeLogRec(sdrMtx*      aMtx,
                              scGRID       aWriteGRID,
                              void*        aValue,
                              UInt         aLength,
                              sdrLogType   aLogType );



    /* page�� ���� ����� �Բ� SDR_*_BYTES Ÿ�� �α׸�
       mtx �α� ���ۿ� ����Ѵ�. */
    static IDE_RC writeNBytes(void*           aMtx,
                              UChar*          aDest,
                              void*           aValue,
                              UInt            aLogType);

    /* ���� mtx �α� ������  offset����
       Ư�� ������ value�� write */
    static IDE_RC write(sdrMtx*  aMtx,
                        void*    aValue,
                        UInt     aLength );

    /* latch item�� mtx ���ÿ� push */
    static IDE_RC push(void*            aMtx,
                       void*            aObject,
                       UInt             aLatchMode );

    static IDE_RC pushPage(void*            aMtx,
                           void*            aObject,
                           UInt             aLatchMode );

    /* BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
     *
     * Free�Ϸ��� Extent���� Mini-Transaction Commit ���Ŀ� Cache�� �߰��Ǿ�� 
     * �մϴ�. Mini-Transaction�� Commit������ ���ԵǸ�, Commit������ ��Ȱ��
     * �Ǿ�����ϴ�.*/
    static IDE_RC addPendingJob(void              * aMtx,
                                idBool              aIsCommitJob,
                                sdrMtxPendingFunc   aPendingFunc,
                                void              * aData );

    static IDE_RC releaseLatchToSP( sdrMtx       *aMtx,
                                    sdrSavePoint *aSP );

    static IDE_RC rollback( sdrMtx       *aMtx );

    static void setSavePoint( sdrMtx       *aMtx,
                              sdrSavePoint *aSP );

    /* PROJ-2162 RestartRiskReduction
     * XƯ�� ���ÿ� Ư�� page ID�� �ش��ϴ� BCB�� ������ ���,
     * �ش� page frame�� ���� �����͸� ��ȯ */
    static UChar * getPagePtrFromPageID( sdrMtx     * aMtx,
                                         scSpaceID    aSpaceID,
                                         scPageID     aPageID );

    static sdrMtxLatchItem * existObject( sdrMtxStackInfo       * aLatchStack,
                                          void                  * aObject,
                                          sdrMtxItemSourceType    aType );

    static smLSN getEndLSN( void *aMtx );

    static idBool isLogWritten( void     *aMtx );
    static idBool isModeLogging( void     *aMtx );
    static idBool isModeNoLogging( void     *aMtx );

    static idBool isEmpty(sdrMtx *aMtx);

    static idBool isNologgingPersistent( void *aMtx );

    /* writeLogRec�Լ��� ���� SubLog�� ũ�⸦ �����س��� �װͺ��� ��������
     * Log�ۿ� ���� ��������, Mtx�� ���� ���̴�. */
    static idBool isRemainLogRec( void *aMtx );

    // for debug
    static void dump( sdrMtx *aMtx,
                      sdrMtxDumpMode   aDumpMode = SDR_MTX_DUMP_NORMAL );

    static void dumpHex( sdrMtx *aMtx );

    static idBool validate( sdrMtx    *aMtx );


// Koo Wrapper
static IDE_RC unlock(void* aLatch, UInt, void*) { return (((iduLatch*)aLatch)->unlock());}

static IDE_RC dump(void*) { return IDE_SUCCESS; }


private:

    /* �ʱ�ȭ */
    static IDE_RC initialize( idvSQL*       aStatistics,
                              sdrMtx*       aMtx,
                              void*         aTrans,
                              sdrMtxLogMode aLogMode,
                              idBool        aUndoable,
                              UInt          aDLogAttr );


    static IDE_RC destroy( sdrMtx *aMtx );

    /* �α� header�� type�� RID�� �����Ѵ�.*/
    static IDE_RC initLogRec( sdrMtx*        aMtx,
                              scGRID         aWriteGRID,
                              UInt           aLength,
                              sdrLogType     aType);


    /* Tablespace�� Log Compression�� ���� �ʵ��� ������ ���
       �α� ������ ���� �ʵ��� ����
    */
    static IDE_RC checkTBSLogCompAttr( sdrMtx*      aMtx,
                                       scSpaceID    aSpaceID );

    static IDE_RC writeUndoLog( sdrMtx    *aMtx,
                                UChar     *aWritePtr,
                                UInt       aLength,
                                sdrLogType aType );


    /* �α� ����� ���� mtx �α� ���� �ʱ�ȭ */
    static IDE_RC initLogBuffer(sdrMtx* aMtx);

    static IDE_RC destroyLogBuffer(sdrMtx* aMtx);

    /* BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
     *
     * Free�Ϸ��� Extent���� Mini-Transaction Commit ���Ŀ� Cache�� �߰��Ǿ�� 
     * �մϴ�. Mini-Transaction�� Commit������ ���ԵǸ�, Commit������ ��Ȱ��
     * �Ǿ�����ϴ�.*/
    static IDE_RC executePendingJob(void   * aMtx,
                                    idBool   aDoCommitJob );

    static IDE_RC destroyPendingJob( void * aMtx);

    /*  mtx ������ �� item�� release �۾��� �����Ѵ�. */
    static IDE_RC releaseLatchItem(sdrMtx*           aMtx,
                                   sdrMtxLatchItem*  aItem);

    static IDE_RC releaseLatchItem4Rollback(sdrMtx          *aMtx,
                                            sdrMtxLatchItem *aItem);

    /* PROJ-2162 RestartRiskReduction
     * OnlineDRDBRedo����� ���� Page�� ������ �ʿ䰡 �ִ��� �����Ѵ�. */
    static idBool needMtxRollback( sdrMtx * aMtx );

    /* PROJ-2162 RestartRiskReduction
     * Mtx Commit���� DRDB Redo����� ������ */
    static IDE_RC validateDRDBRedo( sdrMtx          * aMtx,
                                    sdrMtxStackInfo * aMtxStack,
                                    smuDynArrayBase * aLogBuffer );

    static IDE_RC validateDRDBRedoInternal( 
        idvSQL          * aStatistics,
        sdrMtx          * aMtx,
        sdrMtxStackInfo * aMtxStack,
        ULong           * aSPIDArray,
        UInt            * aSPIDCount,
        scSpaceID         aSpaceID,
        scPageID          aPageID );

private:
    // �� mtx stack item�� type�� ���� ������ �Լ���
    // item�� ���� ������ ������.
    static sdrMtxLatchItemApplyInfo
                  mItemVector[SDR_MTX_RELEASE_FUNC_NUM];

#if defined(DEBUG)
    /* PROJ-2162 RestartRiskReduction
     * Mtx Rollback �׽�Ʈ�� ���� �ӽ� seq.
     * __SM_MTX_ROLLBACK_TEST ���� �����Ǹ� (0�� �ƴϸ�) �� ����
     * MtxCommit�ø��� �����Ѵ�. �׸��� �� Property���� �����ϸ�
     * ������ Rollback�� �õ��Ѵ�. */
    static UInt mMtxRollbackTestSeq;
#endif
};


#endif // _O_SDR_MINI_TRANS_H_
