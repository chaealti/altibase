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
 **********************************************************************/

#include <smcLobUpdate.h>
#include <smcLob.h>
#include <smcReq.h>
#include <smc.h>
#include <smr.h>

/**********************************************************************
 * lobCursor�� open�� �Ϳ� ���� replication �α׸� �����.
 *
 * aTrans      [IN] �۾��ϴ� Ʈ����� ��ü
 * aLobLocator [IN] �۾��Ϸ��� Lob Locator
 * aLobViewEnv [IN] �۾��Ϸ��� LobViewEnv ��ü
 **********************************************************************/
IDE_RC smcLobUpdate::writeLog4CursorOpen(idvSQL*        /*aStatistics*/,
                                         void*          aTrans,
                                         smLobLocator   aLobLocator,
                                         smLobViewEnv*  aLobViewEnv)
{
    smTID           sTransID;
    smrLobLog       sLobCursorLog;
    smrLogType      sLogType;
    UInt            sPKSize;
    UInt            sOffset = 0;
    smcTableHeader* sTable;
    
    IDE_ERROR( aTrans != NULL );

    sTransID = smLayerCallback::getTransID( aTrans );
    
    sTable  = (smcTableHeader*)(aLobViewEnv->mTable);
    
    idlOS::memset(&sLobCursorLog, 0x00, SMR_LOGREC_SIZE(smrLobLog));

    smLayerCallback::initLogBuffer(aTrans);
    sOffset = 0;
    
    sLogType =  SMR_LT_LOB_FOR_REPL;

    sPKSize = smcRecordUpdate::getPrimaryKeySize(
        sTable,
        (SChar*)(aLobViewEnv->mRow) );
    
    smrLogHeadI::setType(&sLobCursorLog.mHead,sLogType);
    smrLogHeadI::setSize(&sLobCursorLog.mHead,
                         SMR_LOGREC_SIZE(smrLobLog) +
                         ID_SIZEOF(smOID)+   // table 
                         ID_SIZEOF(UInt) +   // lob column id
                         sPKSize+            // pk value.
                         ID_SIZEOF(smrLogTail) );
    
    smrLogHeadI::setTransID(&sLobCursorLog.mHead, sTransID);
    
    smrLogHeadI::setPrevLSN( &sLobCursorLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    smrLogHeadI::setFlag(&sLobCursorLog.mHead, SMR_LOG_TYPE_NORMAL);

    if( (smrLogHeadI::getFlag(&sLobCursorLog.mHead) & SMR_LOG_SAVEPOINT_MASK) 
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead,
                                       smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sLobCursorLog.mHead, 
                                       SMI_STATEMENT_DEPTH_NULL );
    }
    
    sLobCursorLog.mOpType = SMR_MEM_LOB_CURSOR_OPEN;
    sLobCursorLog.mLocator = aLobLocator;

    /* Write Lob Header */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 &sLobCursorLog,
                                                 sOffset,
                                                 SMR_LOGREC_SIZE( smrLobLog ) )
              != IDE_SUCCESS );
    sOffset += SMR_LOGREC_SIZE(smrLobLog);

    /* Write Table OID */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 &(sTable->mSelfOID),
                                                 sOffset,
                                                 ID_SIZEOF(smOID) )
              != IDE_SUCCESS );
    sOffset += ID_SIZEOF(smOID);
    
    /* Write column OID */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 &(aLobViewEnv->mLobCol.id),
                                                 sOffset,
                                                 ID_SIZEOF(UInt) )
              != IDE_SUCCESS );
    sOffset += ID_SIZEOF(UInt);

    /* Write primary key value */
    IDE_TEST( smcRecordUpdate::writePrimaryKeyLog( aTrans,
                                                   sTable,
                                                   (SChar*)(aLobViewEnv->mRow),
                                                   sPKSize,
                                                   sOffset)
              != IDE_SUCCESS );

    sOffset += sPKSize;
    
    /* Write log tail */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 & sLogType,
                                                 sOffset,
                                                 ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );
    
    IDE_TEST( smrLogMgr::writeLog( NULL, /* idvSQL* */
                                   aTrans,
                                   smLayerCallback::getLogBufferOfTrans( aTrans ),
                                   NULL,     // Previous LSN Ptr
                                   NULL,     // Log LSN Ptr
                                   NULL,     // End LSN Ptr
                                   sTable->mSelfOID ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

/**********************************************************************
 * lob ����Ÿ�� write�� �۾��� ���� �α� �ۼ�
 *
 * SMR_SMC_PERS_WRITE_LOB_PIECE
 * smrUpdateLog | after image | LobLocator(4Repl)
 *
 * aTrans            [IN] �۾��ϴ� Ʈ����� ��ü
 * aTable            [IN] �۾��Ϸ��� ���̺� ���
 * aLobLocator       [IN] �۾��Ϸ��� Lob Locator
 * aLobSpaceID       [IN] lob piece�� ����Ǵ� tablespace�� ID
 * aPageID           [IN] lob piece�� ���ϴ� page id
 * aPageOffset       [IN] page���� write�ϴ� ��ġ
 * aLobOffset        [IN] ��ü lob���� write�ϴ� ��ġ
 * aPieceLen         [IN] ������ lob piece�� ����
 * aPiece            [IN] ������ lob piece
 * aIsReplSenderSend [IN] Replication Sender�� �α׸� �������� ����
 **********************************************************************/
IDE_RC smcLobUpdate::writeLog4LobWrite(void*           aTrans,
                                       smcTableHeader* aTable,
                                       smLobLocator    aLobLocator,
                                       scSpaceID       aLobSpaceID,
                                       scPageID        aPageID,
                                       UShort          aPageOffset,
                                       UShort          aLobOffset,
                                       UShort          aPieceLen,
                                       UChar*          aPiece,
                                       idBool          aIsReplSenderSend)
{
    UInt              sLogSize;
    UInt              sLogFlag;
    UInt              sOffset;
    UInt              sLobOffset;
    smrUpdateLog      sUpdateLog;
    smrLogType        sLogType;
    /* BUG-16345: Replication LOB basic_test���� diff�߻��մϴ�.
     * sMakeLogFlag�� �ʱ�ȭ���� �ʾƼ� �Ʒ��ʿ��� Log�� Replication
     * Flag�� �߸� �����ǰ� �ֽ��ϴ�. */
    smcMakeLogFlagOpt sMakeLogFlag = SMC_MKLOGFLAG_NONE; 
    
    IDE_ERROR( aTrans != NULL );
    IDE_ERROR( aTable != NULL );
    IDE_ERROR( aPageID != SM_NULL_PID );
    IDE_ERROR( aPageOffset != 0 );
    IDE_ERROR( aPieceLen != 0 );
    IDE_ERROR( aPiece != NULL );

    sLogSize = SMR_LOGREC_SIZE(smrUpdateLog)
             + aPieceLen
             + ID_SIZEOF(smrLogTail);

    if( aIsReplSenderSend == ID_TRUE )
    {
        sLogSize += ID_SIZEOF(smLobLocator) + ID_SIZEOF(UInt);
    }

    /* Log Header �ʱ�ȭ */
    smrLogHeadI::setType(&sUpdateLog.mHead, SMR_LT_UPDATE);
    smrLogHeadI::setTransID( &sUpdateLog.mHead,
                             smLayerCallback::getTransID( aTrans ) );

    // BUG-13046 Itanium HP 11.23 BUG
#if defined(IA64_HP_HPUX)
    smrLogHeadI::setFlag(&sUpdateLog.mHead, 0);
#endif

    if( aIsReplSenderSend == ID_FALSE )
    {
        /* BUG-16013: Row�� ���� ���� ������ �������� �ұ��ϰ� Replication
         * ���� �α׸� ����ϰ� �ֽ��ϴ�.
         * aIsReplSenderSend�� FALSE�̸� �� Log�� Sender�� Skip�Ѵ�. */
        sMakeLogFlag = (smcMakeLogFlagOpt)(SMC_MKLOGFLAG_REPL_SKIP_LOG);
    }

    IDE_TEST( smcRecordUpdate::makeLogFlag(aTrans,
                                           aTable,
                                           SMR_SMC_PERS_WRITE_LOB_PIECE,
                                           sMakeLogFlag,
                                           &sLogFlag)
              != IDE_SUCCESS );

    smrLogHeadI::setFlag(&sUpdateLog.mHead, sLogFlag);

    if( (smrLogHeadI::getFlag(&sUpdateLog.mHead) & SMR_LOG_SAVEPOINT_MASK)
         == SMR_LOG_SAVEPOINT_OK)
    {
        smrLogHeadI::setReplStmtDepth( &sUpdateLog.mHead,
                                       smLayerCallback::getLstReplStmtDepth( aTrans ) );
    }
    else
    {
        smrLogHeadI::setReplStmtDepth( &sUpdateLog.mHead,
                                       SMI_STATEMENT_DEPTH_NULL );
    }

    SC_MAKE_GRID( sUpdateLog.mGRID, aLobSpaceID, aPageID, aPageOffset );

    /* set table oid */
    sUpdateLog.mData        = aTable->mSelfOID;
    sUpdateLog.mType        = SMR_SMC_PERS_WRITE_LOB_PIECE;
    smrLogHeadI::setSize(&sUpdateLog.mHead, sLogSize);
    sUpdateLog.mAImgSize    = aPieceLen;
    sUpdateLog.mBImgSize    = 0;

    smrLogHeadI::setPrevLSN( &sUpdateLog.mHead,
                             smLayerCallback::getLstUndoNxtLSN( aTrans ) );

    /* Begin Write Log */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 &(sUpdateLog),/* Update Log Header */
                                                 0,
                                                 SMR_LOGREC_SIZE(smrUpdateLog) )
              != IDE_SUCCESS );
    sOffset = SMR_LOGREC_SIZE(smrUpdateLog);

    /* Write After Image */
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 aPiece,
                                                 sOffset,
                                                 aPieceLen )
              != IDE_SUCCESS );
    sOffset += aPieceLen;

    if( aIsReplSenderSend == ID_TRUE )
    {
        /* Write Lob Locator */
        IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                     &aLobLocator,
                                                     sOffset,
                                                     ID_SIZEOF(smLobLocator) )
                  != IDE_SUCCESS );
        sOffset += ID_SIZEOF(smLobLocator);

        /* Write Lob Offset */
        sLobOffset = aLobOffset;

        IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                     &sLobOffset,
                                                     sOffset,
                                                     ID_SIZEOF(UInt) )
                  != IDE_SUCCESS );
        sOffset += ID_SIZEOF(UInt);
    }

    /* Write Log Tail */
    sLogType = smrLogHeadI::getType(&sUpdateLog.mHead);
    IDE_TEST( smLayerCallback::writeLogToBuffer( aTrans,
                                                 &sLogType,
                                                 sOffset,
                                                 ID_SIZEOF(smrLogType) )
              != IDE_SUCCESS );
    smrLogHeadI::setType(&sUpdateLog.mHead, sLogType);

    IDE_TEST( smLayerCallback::writeTransLog( aTrans, sUpdateLog.mData ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// SMR_SMC_PERS_WRITE_LOB_PIECE �α׿� ���� redo, undo �۾�
IDE_RC smcLobUpdate::redo_SMC_PERS_WRITE_LOB_PIECE(smTID    /*aTID*/,
                                                   scSpaceID  aSpaceID,
                                                   scPageID   aPID,
                                                   scOffset   aOffset,
                                                   vULong   /*aData*/,
                                                   SChar     *aImage,
                                                   SInt       aSize,
                                                   UInt     /*aFlag*/)
{
    IDE_ERROR(aPID != 0);
    IDE_ERROR(aOffset > 0);
    IDE_ERROR(aImage != NULL);

    IDE_ERROR(aSize > 0);

    UChar * sRowPtr;
    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                            aPID,
                                            (void**)&sRowPtr )
               == IDE_SUCCESS );
    sRowPtr += aOffset;

    idlOS::memcpy( sRowPtr,
                   aImage,
                   aSize);

    IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID, aPID)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

