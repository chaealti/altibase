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
 * Copyight 1999-2001, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: smiLogRec.cpp 90444 2021-04-02 10:15:58Z minku.kang $
 **********************************************************************/

#include <smErrorCode.h>
#include <smr.h>
#include <sdr.h>
#include <sdc.h>
#include <sdp.h>
#include <smiMisc.h>
#include <smiLogRec.h>
#include <smrLogHeadI.h>

extern smiGlobalCallBackList gSmiGlobalCallBackList;
/*
  smiAnalyzeLogFunc smiLogRec::mAnalyzeLogFunc[] =
  {
  NULL,                             // null
  analyzeInsertLogMemory,           // MTBL insert
  analyzeUpdateLogMemory,           // MTBL update
  analyzeDeleteLogMemory,           // MTBL delete
  analyzePKDisk,
  analyzeRedoInsertDisk,
  analyzeRedoDeleteDisk,
  analyzeRedoUpdateDisk,
  analyzeRedoUpdateInsertRowPieceDisk,
  NULL,                             //analyzeRedoUpdateDeleteRowPieceDisk
  analyzeRedoUpdateOverwriteDisk,
  NULL,                             //analyzeRedoUpdateDeleteFirstColumnDisk
  NULL,                             // anlzUndoInsertDisk
  NULL,                             // anlzUndoDeleteDisk
  analyzeUndoUpdateDisk,
  NULL,                             //analyzeUndoUpdateInsertRowPieceDisk,
  analyzeUndoUpdateDeleteRowPieceDisk,
  analyzeUndoUpdateOverwriteDisk,
  analyzeUndoUpdateDeleteFirstColumnDisk,
  analyzeWriteLobPieceLogDisk,      // DTBL Write LOB Piece
  analyzeLobCursorOpenMem,          // MTBL LOB Cursor Open
  analyzeLobCursorOpenDisk,         // DTBL LOB Cursor Close
  analyzeLobCursorClose,            // LOB Cursor Close
  analyzeLobPrepare4Write,          // LOB Prepare for Write
  analyzeLobFinish2Write,           // LOB Finish to Write
  analyzeLobPartialWriteMemory,     // MTBL LOB Partial Write
  analyzeLobPartialWriteDisk,       // DTBL LOB Partial Write
  NULL
  };
*/


/***************************************************************
 * Full XLog : Primary Key ���� �ڿ� ����
 * -----------------------------------------
 * | FXLog  | FXLog  |     Column DATA     |
 * | SIZE   | COUNT  |---------------------|
 * |        |        | ID | Length | Value |
 * -----------------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeFullXLogMemory( smiLogRec  * aLogRec,
                                         SChar      * aXLogPtr,
                                         UShort     * aColCount,
                                         UInt       * aCIDs,
                                         smiValue   * aBColValueArray,
                                         const void * aTable )
{
    UInt        i           = 0;
    UInt        sOffset     = 0;
    UInt        sDataSize   = 0;
    UInt        sFXLogCnt   = 0;
    UInt        sFXLogSize  = 0;
    SChar     * sFXLogPtr;

    const smiColumn * spCol  = NULL;

    /* Full XLog Size */
    sFXLogSize  = aLogRec->getUIntValue( aXLogPtr, sOffset );
    sOffset    += ID_SIZEOF(UInt);

    /* Full XLog Count */
    sFXLogCnt   = aLogRec->getUIntValue( aXLogPtr, sOffset );
    *aColCount  = sFXLogCnt;
    sOffset    += ID_SIZEOF(UInt);

    sFXLogPtr = aXLogPtr;

    for( i = 0 ; i < sFXLogCnt ; i++ )
    {
        /* Get Column ID */
        aCIDs[i] = aLogRec->getUIntValue(sFXLogPtr, sOffset) & SMI_COLUMN_ID_MASK ;
        sOffset += ID_SIZEOF(UInt);

        /* column value length */
        sDataSize = aLogRec->getUIntValue(sFXLogPtr, sOffset);
        sOffset += ID_SIZEOF(UInt);

        spCol = aLogRec->mGetColumn( aTable, aCIDs[i] );

        /* column value */

        if ( sDataSize <= spCol->size )
        {
            (aBColValueArray[i]).length  = sDataSize; //length 
        }
        else
        {
            (aBColValueArray[i]).length = spCol->size;
        }
        (aBColValueArray[i]).value   = sFXLogPtr + sOffset; // value

        sOffset += sDataSize;

        IDE_ASSERT( sOffset <= sFXLogSize );
   }

    return IDE_SUCCESS;
}


/***************************************************************
 * Primary Key ���� �α�
 * ----------------------------------
 * |  PK   |  PK    |               |
 * | Size  | Column |   Column Area |
 * |       | Count  |               |
 * ----------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzePKMem(smiLogRec  *aLogRec,
                               SChar      *aPKAreaPtr,
                               UInt       *aPKColCnt,
                               UInt       *aPKColSize,
                               UInt       *aPKCIDArray,
                               smiValue   *aPKColValueArray,
                               const void *aTable )
{
    UInt     sOffset;
    SChar   *sPKColPtr;

    sOffset = 0;

    /* Get Primary Key Area Size */
    *aPKColSize = aLogRec->getUIntValue(aPKAreaPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    /* Get Primary Key Column Count */
    *aPKColCnt = aLogRec->getUIntValue(aPKAreaPtr, sOffset);
    sOffset  += ID_SIZEOF(UInt);

    sPKColPtr = aPKAreaPtr + sOffset;

    /* Analyze Primary Key Column Area */
    IDE_TEST( analyzePrimaryKey( aLogRec,
                                 sPKColPtr,
                                 aPKColCnt,
                                 aPKCIDArray,
                                 aPKColValueArray,
                                 aTable )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiLogRec::analyzePrimaryKey( smiLogRec  * aLogRec,
                                     SChar      * aPKColImagePtr,
                                     UInt       * aPKColCnt,
                                     UInt       * aPKCIDArray,
                                     smiValue   * aPKColsArray,
                                     const void * aTable )
{
    UInt   i;
    UInt   sAnalyzedColSize = 0;   /* BUG-43565 */ 
    SChar *sPKColPtr;

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aPKColImagePtr != NULL );
    IDE_DASSERT( aPKColCnt != NULL );
    IDE_DASSERT( aPKCIDArray != NULL );
    IDE_DASSERT( aPKColsArray != NULL );

    IDE_DASSERT(*aPKColCnt > 0 );
    IDE_DASSERT((aLogRec->getChangeType() == SMI_CHANGE_MRDB_UPDATE) ||
                (aLogRec->getChangeType() == SMI_CHANGE_MRDB_DELETE) ||
                (aLogRec->getChangeType() == SMI_CHANGE_MRDB_LOB_CURSOR_OPEN));

    sPKColPtr = aPKColImagePtr;
    for(i = 0; i < *aPKColCnt; i++)
    {
        IDE_TEST(analyzePrimaryKeyColumn(aLogRec,
                                         sPKColPtr,
                                         &aPKCIDArray[i],
                                         &aPKColsArray[i],
                                         aTable,
                                         &sAnalyzedColSize )
                 != IDE_SUCCESS );

        /* BUG-43565 : �м��� �÷��� size�� ���� �� �� �����Ƿ� �����Ǳ� �� ũ�⸦ 
         * �̿��Ͽ� �����͸� PK�� ���� �÷����� �̵��Ѵ�. */
        sPKColPtr += ( ID_SIZEOF(UInt)      /*Length*/ 
                       + ID_SIZEOF(UInt)    /*CID*/ 
                       + sAnalyzedColSize   /*���� �� Col. size*/ );
}

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiLogRec::analyzePrimaryKeyColumn(smiLogRec  *aLogRec,
                                          SChar      *aPKColPtr,
                                          UInt       *aPKCid,
                                          smiValue   *aPKCol,
                                          const void *aTable,
                                          UInt       *aAnalyzedColSize )
{
    UInt    sDataSize;
    UInt    sCID;
    UInt    sOffset = 0;
    SChar   sErrorBuffer[256];
    
    const smiColumn * spCol  = NULL;

    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aPKColPtr != NULL );
    IDE_DASSERT( aPKCid != NULL );
    IDE_DASSERT( aPKCol != NULL );

    /* column value length */
    sDataSize = aLogRec->getUIntValue(aPKColPtr, 0);
    sOffset += ID_SIZEOF(UInt);

    /* Get Column ID */
    sCID = aLogRec->getUIntValue(aPKColPtr, sOffset) & SMI_COLUMN_ID_MASK ;
    sOffset += ID_SIZEOF(UInt);

    IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

    *aPKCid = sCID;
    aPKCol->value = aPKColPtr + sOffset;

    if ( aTable != NULL )
    {
        spCol = aLogRec->mGetColumn( aTable, sCID );

        if ( sDataSize <= spCol->size )
        {
            aPKCol->length = sDataSize;
        }
        else
        {
            aPKCol->length = spCol->size;
        }
    }
    else
    {
        aPKCol->length = sDataSize;
    }

    /* BUG-43565 : ���� �� Col. size ���� */
    *aAnalyzedColSize = sDataSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_LARGE_CID );
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzePrimaryKeyColumn] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN : %"ID_UINT32_FMT",%"ID_UINT32_FMT" ]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************
 * < PK �α� ���� >
 *
 *       |<-- sAnalyzePtr�� ������� ����
 *               |<------------- repeat ------------>|
 * ---------------------------------------------------
 * | sdr | PK    | PK     |        |        |        |
 * | Log | total | column | column | column | column |
 * | Hdr | size  | count  | id     | length | data   |
 * ---------------------------------------------------
 *
 * PK size�� 4K ���Ϸ� ���� �Ǿ� ������, �ϳ��� �α׿� ��� ��������.
 **************************************************************/
IDE_RC smiLogRec::analyzePKDisk(smiLogRec  *aLogRec,
                                UInt       *aPKColCnt,
                                UInt       *aPKCIDArray,
                                smiValue   *aPKColValueArray)
{
    UInt     i;
    SChar  * sAnalyzePtr;
    UShort   sPKSize;
    UShort   sLen;
    SChar    sErrorBuffer[256];

    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aPKColCnt != NULL );
    IDE_DASSERT( aPKCIDArray != NULL );
    IDE_DASSERT( aPKColValueArray != NULL );
    IDE_DASSERT( (aLogRec->getChangeType() == SMI_PK_DRDB) ||
                 (aLogRec->getChangeType() == SMI_CHANGE_DRDB_LOB_CURSOR_OPEN) );

    sAnalyzePtr = aLogRec->getAnalyzeStartPtr();

    // 1. Get PK total size
    sPKSize = aLogRec->getUShortValue(sAnalyzePtr);
    sAnalyzePtr += ID_SIZEOF(UShort);

    IDE_ASSERT(sPKSize > 0);

    // 2. Get PK count
    *aPKColCnt = aLogRec->getUShortValue(sAnalyzePtr);
    sAnalyzePtr += ID_SIZEOF(UShort);

    IDE_ASSERT(*aPKColCnt > 0 );

    for(i = 0; i < *aPKColCnt; i++)
    {
        // 3. Get PK Column ID
        aPKCIDArray[i] = aLogRec->getUIntValue(sAnalyzePtr) & SMI_COLUMN_ID_MASK;
        sAnalyzePtr += ID_SIZEOF(UInt);
        IDE_TEST_RAISE( aPKCIDArray[i] > SMI_COLUMN_ID_MAXIMUM, ERR_INVALID_CID );

        // 4. Get PK Column length
        analyzeColumnAndMovePtr((SChar **)&sAnalyzePtr,
                                &sLen,
                                NULL,
                                NULL);

        // 5. Get PK Column data
        aPKColValueArray[i].length = sLen;
        aPKColValueArray[i].value = (SChar *)sAnalyzePtr;

        sAnalyzePtr += sLen;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzePrimaryKeyColumn] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT"]",
                         aPKCIDArray[i], 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************
 *  PROJ-1705
 *
 *  column length �м� :
 *
 *  column length ������ 1~3byte�� ����Ǿ� �ִ�.
 *  �켱 1byte�� �д´�. �� ����,
 *  1. <0xFE>�̸�, �� �÷��� ���̴� 250�� �ʰ���
 *     ������ ������ 2byte�� �о� �м��Ѵ�.
 *  2. <0xFF>�̸�, �÷��� value�� NULL�� ������ �Ǵ��Ѵ�.
 *  3. <0xFD>�̸�, Out Mode LOB Descriptor�̴�.
 *  4. �� ���� ���̸�, 250 ������ �÷� �����̴�.
 *
 **************************************************************/
void smiLogRec::analyzeColumnAndMovePtr( SChar  ** aAnalyzePtr,
                                         UShort  * aColumnLen,
                                         idBool  * aIsOutModeLob,
                                         idBool  * aIsNull )
{
    SChar  * sLen;
    UShort   sLongLen;

    if ( aIsOutModeLob != NULL )
    {
        *aIsOutModeLob = ID_FALSE;
    }

    if ( aIsNull != NULL )
    {
        *aIsNull = ID_FALSE;
    }

    sLen = (SChar *)*aAnalyzePtr;
    *aAnalyzePtr += ID_SIZEOF(SChar);

    if ( (*sLen & 0xFF) == SMI_LONG_VALUE )
    {
        idlOS::memcpy( &sLongLen, *aAnalyzePtr, ID_SIZEOF(UShort) );
        IDE_DASSERT( sLongLen > SMI_SHORT_VALUE );
        *aAnalyzePtr += ID_SIZEOF(UShort);

        *aColumnLen = sLongLen;
    }
    else
    {
        if ( (*sLen & 0xFF) == SMI_OUT_MODE_LOB )
        {
            // PROJ-1862 In Mode Lob
            // Column Prefix�� Out Mode LOB�� ���
            if ( aIsOutModeLob != NULL )
            {
                *aIsOutModeLob = ID_TRUE;
            }

            *aColumnLen = ID_SIZEOF(sdcLobDesc);
        }
        else
        {
            if ( ((UShort)*sLen & 0xFF) == SMI_NULL_VALUE )
            {
                if ( aIsNull != NULL )
                {
                    *aIsNull = ID_TRUE;
                }

                *aColumnLen = 0;
            }
            else
            {
                IDE_DASSERT( ((UShort)*sLen & 0xFF) <= SMI_SHORT_VALUE );
                *aColumnLen = ((UShort)*sLen & 0xFF);
            }
        }
    }

    return;
}

IDE_RC smiLogRec::readFrom( void  * aLogHeadPtr,
                            void  * aLogPtr,
                            smLSN * aLSN )
{
    smrLogHead    *sCommonHdr;

    static SInt sMMDBLogSize = ID_SIZEOF(smrUpdateLog) - ID_SIZEOF(smiLogHdr);
    static SInt sDRDBLogSize = ID_SIZEOF(smrDiskLog) - ID_SIZEOF(smiLogHdr);
    static SInt sLobLogSize = ID_SIZEOF(smrLobLog) - ID_SIZEOF(smiLogHdr);
    // Table Meta Log Record
    static SInt sTableMetaLogSize = ID_SIZEOF(smrTableMetaLog) - ID_SIZEOF(smiLogHdr);

    IDE_DASSERT( aLogPtr != NULL );
    IDE_DASSERT( aLSN != NULL );
    IDE_DASSERT( aLogHeadPtr != NULL );

    ACP_UNUSED( aLSN );

    mLogPtr           = (SChar *)aLogPtr;
    sCommonHdr        = (smrLogHead *)aLogHeadPtr;
    mLogUnion.mCommon = sCommonHdr;
    mRecordLSN        = smrLogHeadI::getLSN( sCommonHdr );

#ifdef DEBUG
    if ( !SM_IS_LSN_INIT ( *aLSN ) )
    {
        IDE_DASSERT( smrCompareLSN::isEQ( &mRecordLSN, aLSN ) ); 
    }
#endif

    /* BUG-35392 */
    if ( smrLogHeadI::isDummyLog( sCommonHdr ) == ID_FALSE )
    {   /* Dummy Log �� �ƴ� �Ϲ� �α� */

        switch(smrLogHeadI::getType(sCommonHdr))
        {
            case SMR_LT_MEMTRANS_COMMIT:
            case SMR_LT_DSKTRANS_COMMIT:
                mLogType = SMI_LT_TRANS_COMMIT;

                idlOS::memcpy( (SChar*)&( mLogUnion.mCommitLog ) + ID_SIZEOF(smiLogHdr),
                               mLogPtr + ID_SIZEOF(smiLogHdr), 
                               ID_SIZEOF(smrTransCommitLog) );
                break;

            case SMR_LT_MEMTRANS_GROUPCOMMIT:
                mLogType = SMI_LT_TRANS_GROUPCOMMIT;
                break;

            case SMR_LT_MEMTRANS_ABORT:
            case SMR_LT_DSKTRANS_ABORT:
                mLogType = SMI_LT_TRANS_ABORT;
                break;

            case SMR_LT_TRANS_PREABORT:
                mLogType = SMI_LT_TRANS_PREABORT;
                break;

            case SMR_LT_SAVEPOINT_SET:
                mLogType = SMI_LT_SAVEPOINT_SET;
                break;

            case SMR_LT_SAVEPOINT_ABORT:
                mLogType = SMI_LT_SAVEPOINT_ABORT;
                break;

            case SMR_LT_FILE_END:
                mLogType = SMI_LT_FILE_END;
                break;

            case SMR_LT_UPDATE:
                idlOS::memcpy( (SChar*)&(mLogUnion.mMemUpdate) + ID_SIZEOF(smiLogHdr),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               sMMDBLogSize );

                mLogType = SMI_LT_MEMORY_CHANGE;

                switch(mLogUnion.mMemUpdate.mType)
                {
                    case SMR_SMC_PERS_INSERT_ROW :
                        mChangeType = SMI_CHANGE_MRDB_INSERT;
                        mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrUpdateLog);
                        break;

                    case SMR_SMC_PERS_UPDATE_INPLACE_ROW :
                    case SMR_SMC_PERS_UPDATE_VERSION_ROW :
                        mChangeType = SMI_CHANGE_MRDB_UPDATE;
                        mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrUpdateLog);
                        break;

                    case SMR_SMC_PERS_DELETE_VERSION_ROW :
                        mChangeType = SMI_CHANGE_MRDB_DELETE;
                        mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrUpdateLog);
                        break;

                    case SMR_SMC_PERS_WRITE_LOB_PIECE :
                        mChangeType = SMI_CHANGE_MRDB_LOB_PARTIAL_WRITE;
                        mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrUpdateLog);
                        break;

                    default:
                        mLogType = SMI_LT_NULL;
                        break;
                }

                break;
                /*
                 * PROJ-1705 : �α� ���� (Outline)
                 *------------------------------------------------------------
                 * |<-- mLogPtr
                 *              |<--->| mRefOffset
                 *              (page alloc ���� RP���� �Ⱥ��� �α׵���
                 *               skip�ϱ� ����)
                 *                    |<--RP���� �ʿ���ϴ� DML�α��� ����
                 * -------------------------------------------------
                 * | smrDiskLog |      Log Body      |   LogTail   |
                 * -------------------------------------------------
                 */
            case SMR_DLT_REDOONLY:
            case SMR_DLT_UNDOABLE:
                idlOS::memcpy((SChar*)&(mLogUnion.mDisk) + ID_SIZEOF(smiLogHdr),
                              mLogPtr + ID_SIZEOF(smiLogHdr),
                              sDRDBLogSize);
                mLogType = SMI_LT_DISK_CHANGE;

                setChangeLogType(mLogPtr +
                                 SMR_LOGREC_SIZE(smrDiskLog) +
                                 mLogUnion.mDisk.mRefOffset);
                break;

            case SMR_LT_LOB_FOR_REPL:
                idlOS::memcpy( (SChar*)&(mLogUnion.mLob) + ID_SIZEOF(smiLogHdr),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               sLobLogSize );
                mLogType = SMI_LT_LOB_FOR_REPL;

                switch(mLogUnion.mLob.mOpType)
                {
                    case SMR_MEM_LOB_CURSOR_OPEN:
                        {
                            mChangeType = SMI_CHANGE_MRDB_LOB_CURSOR_OPEN;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    case SMR_DISK_LOB_CURSOR_OPEN:
                        {
                            mChangeType = SMI_CHANGE_DRDB_LOB_CURSOR_OPEN;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    case SMR_LOB_CURSOR_CLOSE:
                        {
                            mChangeType = SMI_CHANGE_LOB_CURSOR_CLOSE;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    case SMR_PREPARE4WRITE:
                        {
                            mChangeType = SMI_CHANGE_LOB_PREPARE4WRITE;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    case SMR_FINISH2WRITE:
                        {
                            mChangeType = SMI_CHANGE_LOB_FINISH2WRITE;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    case SMR_LOB_TRIM:
                        {
                            mChangeType = SMI_CHANGE_LOB_TRIM;
                            mAnalyzeStartPtr = mLogPtr + SMR_LOGREC_SIZE(smrLobLog);
                            break;
                        }
                    default:
                        {
                            mLogType = SMI_LT_NULL;
                            break;
                        }
                }
                break;

            case SMR_LT_DDL :           // DDL Transaction���� ǥ���ϴ� Log Record
                mLogType = SMI_LT_DDL;
                break;

            case SMR_LT_TABLE_META :    // Table Meta Log Record
                idlOS::memcpy( (SChar*)&(mLogUnion.mTableMetaLog) + ID_SIZEOF(smiLogHdr),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               sTableMetaLogSize );
                mLogType = SMI_LT_TABLE_META;
                break;

            case SMR_LT_DDL_QUERY_STRING :
                idlOS::memcpy( (SChar*)&( mLogUnion.mDDLStmtMeta ),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               ID_SIZEOF(smrDDLStmtMeta) );
                mLogType = SMI_LT_DDL_QUERY_STRING;
                break;

            /* PROJ-2747 Global Tx Consistency */
            case SMR_LT_XA_START_REQ :
                idlOS::memcpy( (SChar*)&( mLogUnion.mXaLog ),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               ID_SIZEOF(smrXaLog) );
                mLogType = SMI_LT_XA_START_REQ;
                break;

            case SMR_LT_XA_PREPARE_REQ :
                idlOS::memcpy( (SChar*)&( mLogUnion.mXaLog ),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               ID_SIZEOF(smrXaLog) );
                mLogType = SMI_LT_XA_PREPARE_REQ;
                break;

            case SMR_LT_XA_PREPARE :
                idlOS::memcpy( (SChar*)&( mLogUnion.mXaLog ),
                               mLogPtr + ID_SIZEOF(smiLogHdr),
                               ID_SIZEOF(smrXaLog) );
                mLogType = SMI_LT_XA_PREPARE;
                break;

            case SMR_LT_XA_END :
                mLogType = SMI_LT_XA_END;
                break;

            default:
                mLogType = SMI_LT_NULL;
                break;
        }
    }
    else
    {   /* Dummy Log �� �� */
        mLogType = SMI_LT_NULL;
    }

    return IDE_SUCCESS;
}

/***************************************************************************
 * PROJ-1705 [���ڵ� ���� ����ȭ]
 *  :  Disk Table�� DML �α� Ÿ���� RP��⿡�� ��� �� �� �ֵ���
 *     smi �α� Ÿ������ ��ȯ��Ų��.
 *
 *----------------------------------------------------------------------
 * [DML]    [sdrLogType]                         [smiChangeLogType]
 *----------------------------------------------------------------------
 * INSERT   SDR_SDC_INSERT_ROW_PIECE             SMI_REDO_DRDB_INSERT
 *----------------------------------------------------------------------
 * DELETE   SDR_SDC_DELETE_ROW_PIECE             SMI_REDO_DRDB_DELETE
 *----------------------------------------------------------------------
 * UPDATE   SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE  SMI_REDO_DRDB_UPDATE_INSERT_ROW_PIECE
 *          SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE  SMI_REDO_DRDB_UPDATE_DELETE_ROW_PIECE
 *          SDR_SDC_UPDATE_ROW_PIECE             SMI_REDO_DRDB_UPDATE
 *          SDR_SDC_OVERWRITE_ROW_PIECE          SMI_REDO_DRDB_UPDATE_OVERWRITE
 *          SDR_SDC_DELETE_FIRST_COLUMN_PIECE    SMI_REDO_DRDB_UPDATE_DELETE_FIRST_COLUMN
 *
 *----------------------------------------------------------------------
 * [DML]    [sdcUndoRecHdr.mRecType]             [smiChangeLogType]
 *----------------------------------------------------------------------
 * INSERT   SDC_UNDO_INSERT_ROW_PIECE            SMI_UNDO_DRDB_INSERT
 *----------------------------------------------------------------------
 * DELETE   SDC_UNDO_DELETE_ROW_PIECE            SMI_UNDO_DRDB_DELETE
 *----------------------------------------------------------------------
 * UPDATE   SDC_UNDO_INSERT_ROW_PIECE_FOR_UPDATE SMI_UNDO_DRDB_UPDATE_INSERT_ROW_PIECE
 *          SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE SMI_UNDO_DRDB_UPDATE_DELETE_ROW_PIECE
 *          SDC_UNDO_UPDATE_ROW_PIECE            SMI_UNDO_DRDB_UPDATE
 *          SDC_UNDO_OVERWRITE_ROW_PIECE         SMI_UNDO_DRDB_UPDATE_OVERWRITE
 *          SDC_UNDO_DELETE_FIRST_COLUMN_PIECE   SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN
 *
 ****************************************************************************
 *
 * REDO�� UNDO�α��� log type ��ġ�� �ٸ���.
 * + REDO�� ��� : sdrLogHdr.mType�� RP���� �з��ϴ� �α� Ÿ���� �����Ѵ�.
 * + UNDO�� ��� : sdrLogHdr.mType�� SDR_SDC_INSERT_UNDO_REC �α� Ÿ������ ������,
 *                  �̶����� �ٷ� �ڿ� �̾����� sdcUndoRecHdr�� type�� �о� �̷ν�
 *                  �����ؾ��Ѵ�. ó�����迡���� sdrLogHdr�ڿ� �ٷ� RP info ��
 *                  ������, �α�Ÿ���� ���������� �о�� �ϹǷ�, RP info ��
 *                  �� �ڷ� �������ȴ�.
 ****************************************************************************/

void smiLogRec::setChangeLogType(void* aLogHdr)
{
    sdrLogHdr      sLogHdr;
    sdcUndoRecType sUndoRecType;

    IDE_DASSERT( aLogHdr != NULL );

    idlOS::memcpy(&sLogHdr, aLogHdr, (UInt)ID_SIZEOF(sdrLogHdr));
    mAnalyzeStartPtr = (SChar*)aLogHdr;

    if ( sLogHdr.mType == SDR_SDC_INSERT_UNDO_REC )
    {
        // undo log type�� �б� ���ؼ��� sdrLogHdr�� �ǳʶ� ��, undo log �� size(UShort)��
        // skip�ϰ� �д´�.
        idlOS::memcpy( &sUndoRecType,
                       (UChar*)aLogHdr + ID_SIZEOF(sdrLogHdr) + ID_SIZEOF(UShort),
                       ID_SIZEOF(UChar) );

        switch(sUndoRecType)
        {
            case SDC_UNDO_INSERT_ROW_PIECE :
                mChangeType = SMI_UNDO_DRDB_INSERT;
                break;

            case SDC_UNDO_DELETE_ROW_PIECE :
                mChangeType = SMI_UNDO_DRDB_DELETE;
                break;

            case SDC_UNDO_INSERT_ROW_PIECE_FOR_UPDATE :
                mChangeType = SMI_UNDO_DRDB_UPDATE_INSERT_ROW_PIECE;
                break;

            case SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE :
                mChangeType = SMI_UNDO_DRDB_UPDATE_DELETE_ROW_PIECE;
                break;

            case SDC_UNDO_UPDATE_ROW_PIECE :
                mChangeType = SMI_UNDO_DRDB_UPDATE;
                break;

            case SDC_UNDO_OVERWRITE_ROW_PIECE :
                mChangeType = SMI_UNDO_DRDB_UPDATE_OVERWRITE;
                break;

            case SDC_UNDO_DELETE_FIRST_COLUMN_PIECE :
                mChangeType = SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN;
                break;

            default :
                mLogType = SMI_LT_NULL;
                mChangeType = SMI_CHANGE_NULL;
                break;
        }
    }
    else
    {
        switch(sLogHdr.mType)
        {
            case SDR_SDC_PK_LOG :
                mChangeType = SMI_PK_DRDB;
                break;

            case SDR_SDC_INSERT_ROW_PIECE :
            case SDR_SDC_INSERT_ROW_PIECE_FOR_DELETEUNDO :
                mChangeType = SMI_REDO_DRDB_INSERT;
                break;

            case SDR_SDC_DELETE_ROW_PIECE :
                mChangeType = SMI_REDO_DRDB_DELETE;
                break;

            case SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE :
                mChangeType = SMI_REDO_DRDB_UPDATE_INSERT_ROW_PIECE;
                break;

            case SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE :
                mChangeType = SMI_REDO_DRDB_UPDATE_DELETE_ROW_PIECE;
                break;

            case SDR_SDC_UPDATE_ROW_PIECE :
                mChangeType = SMI_REDO_DRDB_UPDATE;
                break;

            case SDR_SDC_OVERWRITE_ROW_PIECE :
                mChangeType = SMI_REDO_DRDB_UPDATE_OVERWRITE;
                break;

            case SDR_SDC_DELETE_FIRST_COLUMN_PIECE :
                mChangeType = SMI_REDO_DRDB_UPDATE_DELETE_FIRST_COLUMN;
                break;

            case SDR_SDC_LOB_WRITE_PIECE4DML :  // LOB Write for DML
                mChangeType = SMI_CHANGE_DRDB_LOB_PIECE_WRITE;
                SC_COPY_GRID(sLogHdr.mGRID, mRecordGRID );
                break;
                
            case SDR_SDC_LOB_WRITE_PIECE :      // LOB Piece Write
                mChangeType = SMI_CHANGE_DRDB_LOB_PARTIAL_WRITE;
                SC_COPY_GRID(sLogHdr.mGRID, mRecordGRID );
                break;
                
            case SDR_SDC_LOCK_ROW : // row lock , bug-34581
                mChangeType = SMI_CHANGE_DRDB_LOCK_ROW;
                break;

            default :
                mLogType = SMI_LT_NULL;
                mChangeType = SMI_CHANGE_NULL;
                break;
        }
    }
    return;
}

/* _________________________________________________________________________________________
 * | Fixed area | Fixed | United       | Large            |   LOB     | OldRowOID | NextOID |
 * |____size____|_area__|_var columns__|_variable columns_|___________|___________|_________|
 */
IDE_RC smiLogRec::analyzeInsertLogMemory( smiLogRec  *aLogRec,
                                          UShort     *aColCnt,
                                          UInt       *aCIDArray,
                                          smiValue   *aAColValueArray,
                                          idBool     *aDoWait )
{
    const void * sTable = NULL;

    SChar *sAfterImagePtr;
    SChar *sAfterImagePtrFence;

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aColCnt != NULL );
    IDE_DASSERT( aCIDArray != NULL );
    IDE_DASSERT( aAColValueArray != NULL );
    IDE_DASSERT( aDoWait != NULL );

    IDE_ASSERT( aLogRec->mGetTable( aLogRec->mMeta,
                                    (ULong)aLogRec->getTableOID(),
                                    &sTable)
                == IDE_SUCCESS );

    // �޸� �α״� ������ �ϳ��� �α׷� ���̹Ƿ�, *aDoWait�� ID_FALSE�� �ȴ�.
    *aDoWait = ID_FALSE;

    sAfterImagePtr = aLogRec->getLogPtr() + SMR_LOGREC_SIZE(smrUpdateLog);

    /* TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log
     *                      can be reduced to 1.
     * �α� �������� OldVersion RowOID�� �ֱ� ������ Fence�� �̸� �������ش�. */
    sAfterImagePtrFence = aLogRec->getLogPtr() +
                          + aLogRec->getLogSize()
                          - ID_SIZEOF(ULong)
                          - smiLogRec::getLogTailSize();
    
    IDE_TEST( analyzeInsertLogAfterImageMemory( aLogRec,
                                                sAfterImagePtr,
                                                sAfterImagePtrFence,
                                                aColCnt,
                                                aCIDArray,
                                                aAColValueArray,
                                                sTable )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiLogRec::analyzeInsertLogAfterImageMemory( smiLogRec  *aLogRec,
                                                    SChar      *aAfterImagePtr,
                                                    SChar      *aAfterImagePtrFence,
                                                    UShort     *aColCount,
                                                    UInt       *aCidArray,
                                                    smiValue   *aAfterColsArray,
                                                    const void *aTable )
{
    const smiColumn     * spCol             = NULL;

    SChar               * sFixedAreaPtr     = NULL;
    SChar               * sVarAreaPtr       = NULL;
    SChar               * sVarColPtr        = NULL;
    SChar               * sVarCIDPtr        = NULL;
    UShort                sFixedAreaSize;
    UInt                  sCID;
    UInt                  sAfterColSize;
    UInt                  i;
    UInt                  sOIDCnt;
    smVCDesc              sVCDesc;
    SChar                 sErrorBuffer[256];
    smOID                 sVCPieceOID           = SM_NULL_OID;
    UShort                sVarColCount          = 0;
    UShort                sVarColCountInPiece   = 0;
    UShort                sCurrVarOffset        = 0;
    UShort                sNextVarOffset        = 0;

    /*
      After Image : Fixed Row�� Variable Column�� ���� Log�� ���.
      Fixed Row Size(UShort) + Fixed Row Data
      + VCLOG(1) + VCLOG(2) ... + VCLOG (n)

      VCLOG : Variable Column�� �ϳ��� �����.
      1. SMC_VC_LOG_WRITE_TYPE_AFTERIMG & SM_VCDESC_MODE_OUT
      - Column ID(UInt) | Length(UInt) | Value | OID Cnt(UInt) | OID List |

      2. SMC_VC_LOG_WRITE_TYPE_AFTERIMG & SM_VCDESC_MODE_IN
      - None (After Image�� ���� Fixed Row�� In Mode�� ����ǰ�
      ���� Fixed Row�� ���� Logging�� ������ �����ϱ� ������
      VC�� ���� Logging�� ���ʿ�.
    */

    sFixedAreaPtr = aAfterImagePtr + SMI_LOGREC_MV_FIXED_ROW_DATA_OFFSET;

    /* Fixed Row�� ����:UShort */
    sFixedAreaSize = aLogRec->getUShortValue( aAfterImagePtr, SMI_LOGREC_MV_FIXED_ROW_SIZE_OFFSET );
    IDE_TEST_RAISE( sFixedAreaSize > SM_PAGE_SIZE,
                    err_too_big_fixed_area_size );

    sVarAreaPtr   = aAfterImagePtr + SMI_LOGREC_MV_FIXED_ROW_DATA_OFFSET + sFixedAreaSize;

    *aColCount = (UShort) aLogRec->mGetColumnCount(aTable);

    /* extract fixed fields */
    for (i=0; i < *aColCount; i++ )
    {
        spCol = aLogRec->mGetColumn(aTable, i);

        sCID = spCol->id & SMI_COLUMN_ID_MASK;
        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        if ( (spCol->flag & SMI_COLUMN_COMPRESSION_MASK ) == SMI_COLUMN_COMPRESSION_FALSE )
        {
            if ( (spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED )
            {
                aCidArray[sCID] = sCID;
                aAfterColsArray[sCID].length = spCol->size;
                aAfterColsArray[sCID].value  =
                    sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY));
            }
            else /* large var + lob */
            {
                if ( (spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE )
                {
                    /* Nothing to do
                     * united var �� fixed �������� �ƹ��͵� ������� �ʾҴ� */
                }
                else
                {
                    smiLogRec::getVCDescInAftImg(spCol, aAfterImagePtr, &sVCDesc);
                    if ( (sVCDesc.flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_IN )
                    {
                        aCidArray[sCID] = sCID;
                        
                        if ( ( SMI_IS_LOB_COLUMN(spCol->flag) == ID_TRUE ) && ( sVCDesc.length != 0 ) )
                        {
                            aAfterColsArray[sCID].length = sVCDesc.length + SMI_LOB_DUMMY_HEADER_LEN;
                            aAfterColsArray[sCID].value  =
                                sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY))
                                + ID_SIZEOF( smVCDescInMode ) - SMI_LOB_DUMMY_HEADER_LEN;
                        }
                        else
                        {
                            aAfterColsArray[sCID].length = sVCDesc.length;
                            aAfterColsArray[sCID].value  =
                                sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY))
                                + ID_SIZEOF( smVCDescInMode );
                        }
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }
        }
        else /* PROJ-2397 Compression Column Replication SMI_COLUMN_TYPE_FIXED */
        {
            aCidArray[sCID] = sCID;
            aAfterColsArray[sCID].length = ID_SIZEOF(smOID);
            aAfterColsArray[sCID].value =
                    (smOID *)( sFixedAreaPtr + ( spCol->offset - smiGetRowHeaderSize( SMI_TABLE_MEMORY ) ) );
        }
    }

    /* extract variabel fields */
    sVCPieceOID = aLogRec->getvULongValue( sVarAreaPtr );
    sVarAreaPtr += ID_SIZEOF(smOID);

    if ( sVCPieceOID != SM_NULL_OID )
    {
        sVarColCount = aLogRec->getUShortValue (sVarAreaPtr );
        sVarAreaPtr += ID_SIZEOF(UShort);

        sVarCIDPtr   = sVarAreaPtr;
        sVarAreaPtr += ID_SIZEOF(UInt) * sVarColCount;

        if ( sVarColCount > 0 )
        {
            /* next united var piece �� ã�Ƽ� �ݺ��ؾ��Ѵ�. */
            while ( (sVCPieceOID != SM_NULL_OID) )
            {
                /* united var piece �� �ѹ��� �Ѵ� */
                sVCPieceOID  = aLogRec->getvULongValue( sVarAreaPtr );
                sVarAreaPtr += ID_SIZEOF(smOID);

                sVarColCountInPiece  = aLogRec->getUShortValue ( sVarAreaPtr );
                sVarAreaPtr         += ID_SIZEOF(UShort);

                IDE_DASSERT( sVarColCountInPiece < SMI_COLUMN_ID_MAXIMUM );

                /* var piece ���� column�� �����Ѵ� */
                for ( i = 0 ; i < sVarColCountInPiece ; i++ )
                {
                    /* Column ID �� �տ� ���������Ƿ� ������ offset �� �����Ѵ� */
                    sCID = aLogRec->getUIntValue( sVarCIDPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET );

                    sVarCIDPtr += ID_SIZEOF(UInt);

                    if ( sCID == ID_UINT_MAX )
                    {
                        /* United var �� rp ������ �ʿ���� �÷� */
                        sNextVarOffset = aLogRec->getUShortValue( sVarAreaPtr, 
                                                                  (UInt)( ID_SIZEOF(UShort) * ( i + 1 ) ) );
                    }
                    else
                    {
                        sCID &= SMI_COLUMN_ID_MASK;
                        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

                        spCol = aLogRec->mGetColumn( aTable, sCID );

                        IDE_TEST_RAISE((spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED,
                                err_fixed_col_in_united_var_area);

                        aCidArray[sCID] = sCID;

                        /* value �� �պκп� �ִ� offset array���� offset�� �о�´� */
                        sCurrVarOffset = aLogRec->getUShortValue( sVarAreaPtr,
                                                                  (UInt)( ID_SIZEOF(UShort) * i ) );
                        sNextVarOffset = aLogRec->getUShortValue( sVarAreaPtr, 
                                                                  (UInt)( ID_SIZEOF(UShort) * ( i + 1 )));

                        if ( (UInt)( sNextVarOffset - sCurrVarOffset ) <= spCol->size )
                        {
                            aAfterColsArray[sCID].length = sNextVarOffset - sCurrVarOffset;
                        }
                        else
                        {
                            aAfterColsArray[sCID].length = spCol->size;
                        }

                        if ( sNextVarOffset == sCurrVarOffset )
                        {
                            aAfterColsArray[sCID].value = NULL;
                        }
                        else
                        {
                            aAfterColsArray[sCID].value = sVarAreaPtr + sCurrVarOffset - ID_SIZEOF(smVCPieceHeader);
                        }
                        
                    }
                }
                /* next piece �� offset �̵��Ѵ� */
                sVarAreaPtr += sNextVarOffset - ID_SIZEOF(smVCPieceHeader);
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* extract Large variable fields & LOB fields */
    for (sVarColPtr = sVarAreaPtr; sVarColPtr < aAfterImagePtrFence; )
    {
        sCID = aLogRec->getUIntValue( sVarColPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET )
            & SMI_COLUMN_ID_MASK ;
        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        spCol = aLogRec->mGetColumn(aTable, sCID);
        IDE_TEST_RAISE((spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED,
                       err_fixed_col_in_var_area);

        aCidArray[sCID] = sCID;

        sAfterColSize = aLogRec->getUIntValue(sVarColPtr, SMI_LOGREC_MV_COLUMN_SIZE_OFFSET);
        if ( ( SMI_IS_LOB_COLUMN(spCol->flag) == ID_TRUE ) && ( sAfterColSize != 0 ) )
        {
            aAfterColsArray[sCID].length = sAfterColSize + SMI_LOB_DUMMY_HEADER_LEN;
            aAfterColsArray[sCID].value  = sVarColPtr + SMI_LOGREC_MV_COLUMN_DATA_OFFSET - SMI_LOB_DUMMY_HEADER_LEN;
        }
        else
        {
            aAfterColsArray[sCID].length = sAfterColSize;
            aAfterColsArray[sCID].value  = sVarColPtr + SMI_LOGREC_MV_COLUMN_DATA_OFFSET;
        }
        sVarColPtr += SMI_LOGREC_MV_COLUMN_DATA_OFFSET + sAfterColSize;

        /* Variable/LOB Column Value �� ��쿡�� OID List�� �ǳʶپ�� �� */
        /* ���⿡�� OID count�� ����Ǿ� �����Ƿ�, Count�� ���� �� �� �� ��ŭ */
        /* �ǳʶٵ��� �Ѵ�. */
        sOIDCnt = aLogRec->getUIntValue(sVarColPtr, 0);
        sVarColPtr += ID_SIZEOF(UInt) + (sOIDCnt * ID_SIZEOF(smOID));
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_fixed_col_in_united_var_area);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] "
                         "Fixed Column [CID:%"ID_UINT32_FMT"] is in United Variable Area at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT"]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, sErrorBuffer ) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(err_fixed_col_in_var_area);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] "
                         "Fixed Column [CID:%"ID_UINT32_FMT"] is in Variable Area at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID,
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(ERR_TOO_LARGE_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(err_too_big_fixed_area_size);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] "
                         "Fixed Area Size [%"ID_UINT32_FMT"] is over page size [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT"]",
                         sFixedAreaSize, 
                         SM_PAGE_SIZE,
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiLogRec::analyzeUpdateLogMemory( smiLogRec  *aLogRec,
                                          UInt       *aPKColCnt,
                                          UInt       *aPKCIDArray,
                                          smiValue   *aPKColValueArray,
                                          UShort     *aColCnt,
                                          UInt       *aCIDArray,
                                          smiValue   *aBColValueArray,
                                          smiValue   *aAColValueArray,
                                          idBool     *aDoWait )
{
    SChar    *sBeforeImagePtr;
    SChar    *sBeforeImagePtrFence;
    SChar    *sAfterImagePtr;
    SChar    *sAfterImagePtrFence;
    SChar    *sPkImagePtr;

    UInt     sPKColSize;
    UShort   sAfterColCount;
    UInt     sAfterCids[ SMI_COLUMN_ID_MAXIMUM ];
    SChar    sErrorBuffer[256];

    UInt     i;

    const void * sTable = NULL;

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aPKColCnt != NULL );
    IDE_DASSERT( aPKCIDArray != NULL );
    IDE_DASSERT( aPKColValueArray != NULL );
    IDE_DASSERT( aColCnt != NULL );
    IDE_DASSERT( aCIDArray != NULL );
    IDE_DASSERT(aBColValueArray != NULL );
    IDE_DASSERT( aAColValueArray != NULL );
    IDE_DASSERT( aDoWait != NULL );

    IDE_ASSERT( aLogRec->mGetTable( aLogRec->mMeta,
                                        (ULong)aLogRec->getTableOID(),
                                        &sTable)
                   == IDE_SUCCESS );

    // �޸� �α״� ������ �ϳ��� �α׷� ���̹Ƿ�, *aDoWait�� ID_FALSE�� �ȴ�.
    *aDoWait = ID_FALSE;

    // fix PR-3409 PR-3556
    for(i=0 ; i < SMI_COLUMN_ID_MAXIMUM ; i++)
    {
        // 0���� �ʱ�ȭ�ϸ� �� �ɵ�? 0�� CID�� ���Ǵ� ����
        // MAX�� �߱�ȭ�ϴ� �Ϳ� ���ؼ� �����غ��� - by mycomman, review
        sAfterCids[i] = 0;
    }

    sBeforeImagePtr = aLogRec->getLogPtr() + SMR_LOGREC_SIZE(smrUpdateLog);
    sBeforeImagePtrFence = sBeforeImagePtr + aLogRec->getBfrImgSize();

    sAfterImagePtr = sBeforeImagePtrFence;
    sAfterImagePtrFence  = sAfterImagePtr + aLogRec->getAftImgSize();

    sPkImagePtr = sAfterImagePtrFence;

    if ( aLogRec->getLogUpdateType() == SMR_SMC_PERS_UPDATE_INPLACE_ROW )
    {
        /* BUG-47366 UpdateInplace Log�� Before Image ���� 
         * BeforeImage�� Size�� ��Ÿ���� UInt�� Log�� �߰��Ǿ���. */
        sBeforeImagePtrFence -= ID_SIZEOF(UInt);

        /* sModifyIdxBit BUG-47632,47615 */
        sBeforeImagePtr += ID_SIZEOF(ULong);

        IDE_TEST(analyzeColumnUIImageMemory(aLogRec,
                                            sBeforeImagePtr,
                                            sBeforeImagePtrFence,
                                            aColCnt,
                                            aCIDArray,
                                            aBColValueArray,
                                            sTable,
                                            ID_TRUE/* Before Image */)
                 != IDE_SUCCESS );

        IDE_TEST(analyzeColumnUIImageMemory(aLogRec,
                                            sAfterImagePtr,
                                            sAfterImagePtrFence,
                                            &sAfterColCount,
                                            aCIDArray,
                                            aAColValueArray,
                                            sTable,
                                            ID_FALSE/* After Image */)
                 != IDE_SUCCESS );

        /* Before image�� After image�� �ִ� Update column count�� �׻�
           ���ƾ߸� �Ѵ�. */
        //BUG-43744 : trailing NULL ��/���� ������Ʈ �ϸ� �ٸ� �� �ִ�.
    }
    else
    {
        if ( aLogRec->getLogUpdateType() == SMR_SMC_PERS_UPDATE_VERSION_ROW )
        {
            /* TASK-4690, BUG-32319
             * [sm-mem-collection] The number of MMDB update log can be
             * reduced to 1.
             *
             * Before �̹��� �������� OldVersion RowOID�� lock row ���ΰ� �ְ�
             * After �̹��� �������� OldVersion RowOID�� �����Ƿ�
             * Fence�� �̸� �������ش�. */
            sBeforeImagePtrFence = sBeforeImagePtrFence - ID_SIZEOF(ULong) - ID_SIZEOF(idBool);
            sAfterImagePtrFence  = sAfterImagePtrFence - ID_SIZEOF(ULong);

            IDE_TEST( analyzeColumnMVImageMemory( aLogRec,
                                                  sBeforeImagePtr,
                                                  sBeforeImagePtrFence,
                                                  aColCnt,
                                                  aCIDArray,
                                                  aBColValueArray,
                                                  sTable )
                      != IDE_SUCCESS );

            IDE_TEST( analyzeInsertLogAfterImageMemory( aLogRec,
                                                        sAfterImagePtr,
                                                        sAfterImagePtrFence,
                                                        &sAfterColCount,
                                                        sAfterCids,
                                                        aAColValueArray,
                                                        sTable )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_RAISE(err_no_matching_update_type);
        }
    }

    IDE_TEST( analyzePKMem( aLogRec,
                            sPkImagePtr,
                            aPKColCnt,
                            &sPKColSize,
                            aPKCIDArray,
                            aPKColValueArray,
                            sTable )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_no_matching_update_type);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeUpdateLogMemory] No matching update type [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT"]",
                         aLogRec->getLogUpdateType(), 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *      Befor  Image: ������ Update�Ǵ� Column�� ���ؼ�
 *         Fixed Column : Flag(SChar) | Offset(UInt)  |
 *                        ColumnID(UInt) | SIZE(UInt) | Value
 *
 *         Var   Column : Flag(SChar) | Offset(UInt) | ColumnID(UInt) | SIZE(UInt)
 *               SM_VCDESC_MODE_OUT:
 *                        | Value | OID
 *               SM_VCDESC_MODE_IN:
 *                        | Value
 *         LOB   Column : Flag(SChar) | Offset(UInt) | ColumnID(UInt) | SIZE(UInt)
 *                        | Piece Count(UInt) | firstLPCH * | Value | OID
 *
 *      After  Image: ������ Update�Ǵ� Column�� ���ؼ�
 *         Fixed Column : Flag(SChar) | Offset(UInt) | ColumnID(UInt) | SIZE(UInt)
 *                        | Value
 *
 *         Var/LOB Column : Flag(SChar) | Offset(UInt) | ColumnID(UInt) | SIZE(UInt)
 *               SM_VCDESC_MODE_OUT, LOB:
 *                        | Value | OID Count | OID ��...
 *               SM_VCDESC_MODE_IN:
 *                        | Value
 *
 */
IDE_RC smiLogRec::analyzeColumnUIImageMemory( smiLogRec  * aLogRec,
                                              SChar      * aColImagePtr,
                                              SChar      * aColImagePtrFence,
                                              UShort     * aColCount,
                                              UInt       * aCidArray,
                                              smiValue   * aColsArray,
                                              const void * aTable,
                                              idBool       aIsBefore)
{
    SChar  sFlag;
    UInt   i        = 0;

    UInt   sDataSize;
    UInt   sCID;
    UShort sCidCount = 0;
    SChar  sErrorBuffer[256];

    SChar *sColPtr;
    UInt   sOIDCnt;

    SChar *sVarAreaPtr          = aColImagePtr;
    SChar *sVarCIDPtr           = NULL;
    SChar *sVarCIDPence         = NULL;     //BUG-43744
    smOID  sVCPieceOID          = SM_NULL_OID;
    UShort sVarColCount         = 0;
    UShort sVarColCountInPiece  = 0;
    UShort sCurrVarOffset       = 0;
    UShort sNextVarOffset       = 0;

    const smiColumn    * spCol  = NULL;

    /* extract variabel fields */
    sVCPieceOID = aLogRec->getvULongValue( sVarAreaPtr );
    sVarAreaPtr += ID_SIZEOF(smOID);

    if ( sVCPieceOID != SM_NULL_OID )
    {
        sVarColCount = aLogRec->getUShortValue (sVarAreaPtr );
        sVarAreaPtr += ID_SIZEOF(UShort);

        sVarCIDPtr   = sVarAreaPtr;
        sVarAreaPtr += ID_SIZEOF(UInt) * sVarColCount;
        sVarCIDPence = sVarAreaPtr;

        if ( sVarColCount > 0 )
        {
            /* next united var piece �� ã�Ƽ� �ݺ��ؾ��Ѵ�. */
            while ( (sVCPieceOID != SM_NULL_OID) )
            {
                /* united var piece �� �ѹ��� �Ѵ� */
                sVCPieceOID = aLogRec->getvULongValue( sVarAreaPtr );
                sVarAreaPtr += ID_SIZEOF(smOID);

                sVarColCountInPiece = aLogRec->getUShortValue (sVarAreaPtr );
                sVarAreaPtr         += ID_SIZEOF(UShort);

                /* var piece ���� column�� �����Ѵ� */
                for ( i = 0 ; i < sVarColCountInPiece ; i++ )
                {
                    /* Column ID �� �տ� ���������Ƿ� ������ offset �� �����Ѵ� */
                    sCID = aLogRec->getUIntValue( sVarCIDPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET );

                    sVarCIDPtr += ID_SIZEOF(UInt);

                    if ( sCID == ID_UINT_MAX )
                    {
                        /* United var �� rp ������ �ʿ���� �÷� */
                        sNextVarOffset = aLogRec->getUShortValue( sVarAreaPtr, 
                                                                  (UInt)( ID_SIZEOF(UShort) * (i+1) ) );
                    }
                    else
                    {
                        sCID &= SMI_COLUMN_ID_MASK;

                        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

                        spCol = aLogRec->mGetColumn( aTable, sCID );

                        IDE_TEST_RAISE((spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED,
                                       err_fixed_col_in_united_var_area);

                        aCidArray[sCidCount] = sCID;

                        /* value �� �պκп� �ִ� offset array���� offset�� �о�´� */
                        sCurrVarOffset = aLogRec->getUShortValue( sVarAreaPtr,
                                                                  (UInt)( ID_SIZEOF(UShort) * i ) );
                        sNextVarOffset = aLogRec->getUShortValue( sVarAreaPtr, 
                                                                  (UInt)( ID_SIZEOF(UShort) * ( i + 1 )));


                        if ( (UInt)( sNextVarOffset - sCurrVarOffset ) <= spCol->size )
                        {
                            aColsArray[sCID].length = sNextVarOffset - sCurrVarOffset;
                        }
                        else
                        {
                            aColsArray[sCID].length = spCol->size;
                        }

                        if ( sNextVarOffset == sCurrVarOffset ) /* length 0 = null value */
                        {
                            aColsArray[sCID].value = NULL;
                        }
                        else
                        {
                            aColsArray[sCID].value  = sVarAreaPtr + sCurrVarOffset - ID_SIZEOF(smVCPieceHeader);
                        }
                        
                        /* BUG-43441 */
                        sCidCount++;
                    }
                }
                /* next piece �� offset �̵��Ѵ� */
                sVarAreaPtr += sNextVarOffset - ID_SIZEOF(smVCPieceHeader);
            }
            
            /* BUG-43744 : before img.�� ���, Ʈ���ϸ� ���� ������ ��, ������Ʈ
             * �� �÷��� Ʈ���ϸ� �ο� ���� �÷��̸� �� �÷��� ���� while ��������
             * ó������ ���� �� �ִ�. �� piece�� �÷� ����(sVarColCountInPiece)��
             * Ʈ���ϸ� ���� �������� �ʱ� �����̴�.
             */
            while ( sVarCIDPtr < sVarCIDPence )
            {
                sCID = aLogRec->getUIntValue( sVarCIDPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET );

                sCID &= SMI_COLUMN_ID_MASK;

                if ( sCID != ID_UINT_MAX )
                {
                    aCidArray[sCidCount] = sCID;
                    aColsArray[sCID].length = 0;
                    aColsArray[sCID].value = NULL;

                    sVarCIDPtr += ID_SIZEOF(UInt);
                    sCidCount++;
                }
                else
                {
                    sVarCIDPtr += ID_SIZEOF(UInt);
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    for ( sColPtr = sVarAreaPtr; sColPtr < aColImagePtrFence; )
    {
        /* Get Flag & Skip Flag, Offset*/
        sFlag = aLogRec->getSCharValue(sColPtr, 0);
        sColPtr += (ID_SIZEOF(SChar)/*Flag*/ + ID_SIZEOF(UInt)/*Offset*/);

        /* Get Column ID */
        sCID = aLogRec->getUIntValue(sColPtr, 0) & SMI_COLUMN_ID_MASK ;
        sColPtr += ID_SIZEOF(UInt);

        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        /* Get Size */
        sDataSize = aLogRec->getUIntValue(sColPtr, 0);
        sColPtr += ID_SIZEOF(UInt);

        /* LOB Column�� ���, Before image�� Piece count�� firstLPCH ptr�� ���� */
        if ( aIsBefore == ID_TRUE )
        {
            if ( ( SMI_IS_LOB_COLUMN(sFlag) == ID_TRUE ) &&
                 ( sDataSize != 0 ) )
            {
                sColPtr += ( ( ID_SIZEOF(UInt)     /*Piece Count*/
                               + ID_SIZEOF(void *) /*firstLPCH ptr */));
            }
            aCidArray[sCidCount] = sCID;
        }
        else
        {
            /* BUG-29234 before image �м��ÿ� aCidArray�� ���� ���������Ƿ�,
             * after image�м� �ÿ��� �� ���� �ʿ䰡 ����.
             * ��, before image�� after image�� CID�� �������� �����Ѵ�.
             */
            IDE_DASSERT( aCidArray[sCidCount] == sCID );
        }

        /* Get Value */
        switch ( sFlag & SMI_COLUMN_TYPE_MASK )
        {
            case SMI_COLUMN_TYPE_LOB:
                if ( sDataSize != 0 )
                {
                    aColsArray[sCID].length = sDataSize + SMI_LOB_DUMMY_HEADER_LEN;
                    aColsArray[sCID].value  = sColPtr   - SMI_LOB_DUMMY_HEADER_LEN;
                }
                else
                {
                    aColsArray[sCID].length = 0;
                    aColsArray[sCID].value  = NULL;
                }
                break;

            case SMI_COLUMN_TYPE_VARIABLE:
            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
                /* fall through */
            case SMI_COLUMN_TYPE_FIXED:
                aColsArray[sCID].length = sDataSize;
                aColsArray[sCID].value  = sColPtr;
                break;

            default:
                break;
        }

        // BUG-37433, BUG-40282
        // before�鼭 OutMode�̸鼭 LOB �� ���� Data�� Log�� ������ ����.
        // smcRecordUpdate::undo_SMC_PERS_UPDATE_INPLACE_ROW() �Լ� ����
        if ( ( aIsBefore                == ID_TRUE ) &&
             ( SMI_IS_OUT_MODE(sFlag)   == ID_TRUE ) &&
             ( SMI_IS_LOB_COLUMN(sFlag) == ID_TRUE ))
        {
            // do nothing
        }
        else
        {
            sColPtr += sDataSize;
        }

        if ( aIsBefore == ID_TRUE )
        {
            /* Before image���� OUT mode value�϶���, OID�� ���� */
            if ( (SMI_IS_VARIABLE_LARGE_COLUMN(sFlag) == ID_TRUE ) ||
               ( SMI_IS_LOB_COLUMN(sFlag) == ID_TRUE ) )
            {
                if ( SMI_IS_OUT_MODE(sFlag) == ID_TRUE )
                {
                    sColPtr += ID_SIZEOF(smOID);
                }
            }
        }
        else
        {
            /* After image���� OUT mode value�϶���, OID Count�� OID list�� �ְ�,
             * In mode�϶��� OID count, OID list�� ���� */
            if ( ( SMI_IS_VARIABLE_LARGE_COLUMN(sFlag) == ID_TRUE ) ||
                 ( SMI_IS_LOB_COLUMN(sFlag) == ID_TRUE ) )
            {
                if ( SMI_IS_OUT_MODE(sFlag) == ID_TRUE )
                {
                    sOIDCnt = aLogRec->getUIntValue(sColPtr, 0);
                    sColPtr += (sOIDCnt * ID_SIZEOF(smOID));    // OID List
                    sColPtr += ID_SIZEOF(UInt);                 // OID Count
                }
            }
        }

        sCidCount++;
    }

    IDE_TEST_RAISE( sCidCount == 0, err_cid_count_eq_zero );

    *aColCount = (UShort)sCidCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cid_count_eq_zero);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeColumnUIImageMemory] Column Count is 0(zero) at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, sErrorBuffer ) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(ERR_TOO_LARGE_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeColumnUIImageMemory] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(err_fixed_col_in_united_var_area);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] "
                         "Fixed Column [CID:%"ID_UINT32_FMT"] is in United Variable Area at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* For MVCC
 *
 *     Before Image : Sender�� �о ������ �α׿� ���ؼ��� ��ϵȴ�.
 *                    ������ Update�Ǵ� Column�� ���ؼ�
 *        Fixed Column : Column ID | SIZE | DATA
 *        Var   Column :
 *            1. SMC_VC_LOG_WRITE_TYPE_BEFORIMG & SM_VCDESC_MODE_OUT
 *               - Column ID(UInt) | Length(UInt) | Value
 *
 *            2. SMC_VC_LOG_WRITE_TYPE_BEFORIMG & SM_VCDESC_MODE_IN
 *               - Column ID(UInt) | Length(UInt) | Value
 *
 */
IDE_RC smiLogRec::analyzeColumnMVImageMemory( smiLogRec  * aLogRec,
                                              SChar      * aColImagePtr,
                                              SChar      * aColImagePtrFence,
                                              UShort     * aColCount,
                                              UInt       * aCidArray,
                                              smiValue   * aColsArray,
                                              const void * aTable )
{
    UInt  sDataSize;
    UInt  sCID;
    UShort sCidCount = 0;
    SChar sErrorBuffer[256];

    SChar * sColPtr;

    const smiColumn * spCol  = NULL;

    /* BUG-47366 */
    SChar * sBfrImgFence      = NULL;
    smOID   sVCPieceOID       = SM_NULL_OID;
    UShort  sVarColumnCnt     = 0;

    /* BUG-47366 UpdateVersionLog�� BeforeImage�� ����
     * OldVersion�� UnitedVar OID Array�� �߰��Ǿ���. */
    sBfrImgFence = aColImagePtrFence;

    sBfrImgFence -= ID_SIZEOF(smOID);
    idlOS::memcpy(&sVCPieceOID, sBfrImgFence, ID_SIZEOF(smOID));

    /* OID�� NULL�̸� �� �ʿ� ����. */
    if ( sVCPieceOID != SM_NULL_OID )
    {
        sBfrImgFence -= ID_SIZEOF(UShort);
        idlOS::memcpy(&sVarColumnCnt, sBfrImgFence, ID_SIZEOF(UShort));

        /* ������ VarColumn Cnt�� 0�̸� Old Version�� �׳� ����ϴ� ���̶� ����� ���� ����. */
        if ( sVarColumnCnt > 0 )
        {
            while( sVCPieceOID != SM_NULL_OID )
            {
                /* get next piece oid */
                sBfrImgFence -= ID_SIZEOF(smOID);
                idlOS::memcpy( &sVCPieceOID, sBfrImgFence, ID_SIZEOF(smOID) );
            }
        }
    }

    for ( sColPtr = aColImagePtr; sColPtr < sBfrImgFence; )
    {
        /* Get Column ID */
        sCID = aLogRec->getUIntValue(sColPtr, 0) & SMI_COLUMN_ID_MASK ;
        sColPtr += ID_SIZEOF(UInt);

        IDE_TEST_RAISE( sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        /* Get Size */
        sDataSize = aLogRec->getUIntValue(sColPtr, 0);
        sColPtr += ID_SIZEOF(UInt);

        spCol = aLogRec->mGetColumn( aTable, sCID );

        aCidArray[sCidCount] = sCID;
        aColsArray[sCID].value  = sColPtr;

        if ( sDataSize <= spCol->size )
        {
            aColsArray[sCID].length = sDataSize;
        }
        else
        {
            aColsArray[sCID].length = spCol->size;
        }
        
        sColPtr += sDataSize;
        sCidCount++;
    }

    IDE_TEST_RAISE(sCidCount == 0, err_cid_count_eq_zero);

    *aColCount = sCidCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cid_count_eq_zero);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeColumnMVImageMemory] Column Count is 0(zero) at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, sErrorBuffer ) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(ERR_TOO_LARGE_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeColumnMVImageMemory] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiLogRec::analyzeDeleteLogMemory( smiLogRec  *aLogRec,
                                          UInt       *aPKColCnt,
                                          UInt       *aPKCIDArray,
                                          smiValue   *aPKColValueArray,
                                          idBool     *aDoWait,
                                          UShort     *aColCnt,
                                          UInt       *aCIDs,
                                          smiValue   *aBColValueArray )
{
    const void * sTable = NULL;

    SChar     * sPkImagePtr;
    UInt        sPKColSize;
    SChar     * sFXLogPtr;
    SChar       sErrorBuffer[256];

    // Simple argument check code
    IDE_DASSERT( aLogRec          != NULL );
    IDE_DASSERT( aPKColCnt        != NULL );
    IDE_DASSERT( aPKCIDArray      != NULL );
    IDE_DASSERT( aPKColValueArray != NULL );
    IDE_DASSERT( aDoWait          != NULL );
    IDE_DASSERT( aColCnt          != NULL );
    IDE_DASSERT(aBColValueArray  != NULL );

    IDE_ASSERT( aLogRec->mGetTable( aLogRec->mMeta,
                                        (ULong)aLogRec->getTableOID(),
                                        &sTable)
                   == IDE_SUCCESS );

    // �޸� �α״� ������ �ϳ��� �α׷� ���̹Ƿ�, *aDoWait�� ID_FALSE�� �ȴ�.
    *aDoWait = ID_FALSE;

    IDE_TEST_RAISE( aLogRec->getLogUpdateType()
                    != SMR_SMC_PERS_DELETE_VERSION_ROW, err_no_matching_delete_type );

    sPkImagePtr = aLogRec->getLogPtr() + SMR_LOGREC_SIZE(smrUpdateLog);

    /* ���� Main���� Before Next �� After Next,�� 2���� ���ԵǾ��ִ�.*/
    sPkImagePtr += (ID_SIZEOF(ULong) * 2);

    IDE_TEST( analyzePKMem( aLogRec,
                            sPkImagePtr,
                            aPKColCnt,
                            &sPKColSize,
                            aPKCIDArray,
                            aPKColValueArray,
                            sTable )
              != IDE_SUCCESS );

    /* TASK-5030 */
    if ( aLogRec->needSupplementalLog() == ID_TRUE )
    {
        sFXLogPtr = sPkImagePtr + sPKColSize;

        IDE_TEST( analyzeFullXLogMemory( aLogRec,
                                         sFXLogPtr,
                                         aColCnt,
                                         aCIDs,
                                         aBColValueArray,
                                         sTable )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_no_matching_delete_type);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeDeleteLogMemory] No matching delete type [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         aLogRec->getLogUpdateType(), 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************
 * < sdrLogHdr ���� >
 *
 * typedef struct sdrLogHdr
 * {
 *     scGRID         mGRID;
 *     sdrLogType     mType;
 *     UInt           mLength;
 * } sdrLogHdr;
 *
 **************************************************************/
IDE_RC smiLogRec::analyzeHeader( smiLogRec * aLogRec,
                                 idBool    * aIsContinue )
{
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aIsContinue != NULL );

    // mContType�� ���� ������ �αװ� �̾����� �Ǵ��Ѵ�.
    *aIsContinue = ( (smrContType)aLogRec->getContType() == SMR_CT_END) ?
                   ID_FALSE : ID_TRUE;

    // Skip sdrLogHdr
    aLogRec->setAnalyzeStartPtr( (SChar *)(aLogRec->getAnalyzeStartPtr() +
                                 ID_SIZEOF(sdrLogHdr)));

    return IDE_SUCCESS;
}

/***************************************************************
 * < RP info  ���� >
 *
 *          |<------- repeat --------->|
 * -------------------------------------
 * |        |        |        | column |
 * | column | column | column | total  |
 * | count  | seq    | id     | length |
 * -------------------------------------
 *
 * UNDO LOG : total length ������ ����.
 **************************************************************/
IDE_RC smiLogRec::analyzeRPInfo( smiLogRec * aLogRec, SInt aLogType )
{
    UShort             i;
    SChar            * sAnalyzePtr;
    UShort             sColCntInRPInfo;
    UShort             sColSeq = 0;
    UShort             sTrailingNullColSeq;
    UInt               sColId;
    UInt               sColTotalLen;
    smiChangeLogType   sChangeLogType;

    sAnalyzePtr = aLogRec->getAnalyzeStartPtr();
    sChangeLogType = aLogRec->getChangeType();

    // RP info �� �� �ڿ� �����Ƿ�, �� ��ġ���� �̵�.
    if ( aLogType == SMI_UNDO_LOG )
    {
        sAnalyzePtr = (SChar *) aLogRec->getRPLogStartPtr4Undo(
                                (void *)sAnalyzePtr, sChangeLogType);
    }
    else if ( aLogType == SMI_REDO_LOG )
    {
        sAnalyzePtr = (SChar *) aLogRec->getRPLogStartPtr4Redo(
                                (void *)sAnalyzePtr, sChangeLogType);
    }
    else
    {
        IDE_ASSERT(ID_FALSE);
    }

    // 1. Get Column Count
    sColCntInRPInfo = aLogRec->getUShortValue(sAnalyzePtr);
    aLogRec->setUpdateColCntInRowPiece(sColCntInRPInfo);
    sAnalyzePtr += ID_SIZEOF(UShort);

    for(i=0; i < sColCntInRPInfo; i++)
    {
        sTrailingNullColSeq = sColSeq;
        // 2. Get column sequence
        sColSeq = aLogRec->getUShortValue(sAnalyzePtr);
        if ( sColSeq == ID_USHORT_MAX )
        {
            // trailing null�� ������Ʈ �ϴ� ����� before image�̸�,
            // overwrite update�� ��쿡�� �߻��ϸ�, ��ġ�� ���� ������ �������� seq�� �ֵ����Ѵ�.
            // IDE_DASSERT(aLogRec->getChangeType() == SMI_UNDO_DRDB_UPDATE_OVERWRITE);
            // BUGBUG - delete row piece for update ����� ���� ��찡 �ִ�. Ȯ���ʿ�. ���ۿ� �̻��� ����.
            sColSeq = sTrailingNullColSeq + 1;
        }
        aLogRec->setColumnSequence(sColSeq, i);  // seq�� �迭�� ���������� ����.
        sAnalyzePtr += ID_SIZEOF(UShort);

        // 3. Get column id
        sColId = aLogRec->getUIntValue(sAnalyzePtr);
        aLogRec->setColumnId(sColId, sColSeq);   // id�� �迭�� seq��ġ�� ����.
        sAnalyzePtr += ID_SIZEOF(UInt);

        // 4. Get column total length
        if ( aLogType == SMI_REDO_LOG )
        {
            SMI_LOGREC_READ_AND_MOVE_PTR( sAnalyzePtr, &sColTotalLen, ID_SIZEOF(UInt) );
            aLogRec->setColumnTotalLength((SInt)sColTotalLen, sColSeq); // totalLen�� �迭�� seq��ġ�� ����.
        }
        else
        {
            // undo log�� ���, total length�� SMI_UNDO_LOG(-1)�� ���� ���´�.
            aLogRec->setColumnTotalLength(SMI_UNDO_LOG, sColSeq);
        }
    }

    return IDE_SUCCESS;
}

/***************************************************************
 * < Undo info  ���� >
 *
 * --------------------------------------
 * |                 |                  |
 * |  sdcUndoRecHdr  |      scGRID      |
 * |                 |                  |
 * --------------------------------------
 *
 **************************************************************/
IDE_RC smiLogRec::analyzeUndoInfo(smiLogRec *  aLogRec)
{
    SChar          * sAnalyzePtr;

    IDE_DASSERT( aLogRec != NULL );

    // Skip sdcUndoRecHdr, scGRID
    sAnalyzePtr = aLogRec->getAnalyzeStartPtr() +
                    ID_SIZEOF(UShort) +           // size(2)
                    SDC_UNDOREC_HDR_SIZE  +
                    ID_SIZEOF(scGRID);

    aLogRec->setAnalyzeStartPtr(sAnalyzePtr);

    return IDE_SUCCESS;
}

/***************************************************************
 * < Update info  ���� >
 *
 * column desc set size : �ڿ� ������ column desc set�� ũ��
 * column desc set : 1~128 byte���� ���̰� �������̴�. RP������
 *                   �ʿ���� �����̹Ƿ� ��� skip�Ѵ�.
 *
 * ----------------------------------------------
 * |        |      |        | column   | column |
 * | opcode | size | column | desc     | desc   |
 * |        |      | count  | set size | set    |
 * ----------------------------------------------
 *
 **************************************************************/
IDE_RC smiLogRec::analyzeUpdateInfo(smiLogRec *  aLogRec)
{
    SChar          * sAnalyzePtr;
    SChar            sSize;

    IDE_DASSERT( aLogRec != NULL );

    sAnalyzePtr = aLogRec->getAnalyzeStartPtr();

    switch(aLogRec->getChangeType())
    {
        case SMI_UNDO_DRDB_UPDATE :
        case SMI_REDO_DRDB_UPDATE :
            // Skip flag(1), size(2), columnCount(2)
            sAnalyzePtr += (ID_SIZEOF(SChar) + ID_SIZEOF(UShort) + ID_SIZEOF(UShort));
            /* Skip column desc */
            sSize = (SChar) *sAnalyzePtr;
            sAnalyzePtr += (ID_SIZEOF(SChar) + sSize);
            break;
        case SMI_UNDO_DRDB_UPDATE_OVERWRITE :
        case SMI_REDO_DRDB_UPDATE_OVERWRITE :
        case SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN :
            // Skip flag(1), size(2)
            sAnalyzePtr += (ID_SIZEOF(SChar) + ID_SIZEOF(UShort));
            break;
        case SMI_UNDO_DRDB_UPDATE_DELETE_ROW_PIECE :
            // Skip size(2)
            sAnalyzePtr += ID_SIZEOF(UShort);
            break;
        /* TASK-5030 */
        case SMI_UNDO_DRDB_DELETE :
            // Skip size(2)
            sAnalyzePtr += ID_SIZEOF(UShort);
            break;
        default :
            IDE_DASSERT(ID_FALSE);
            break;
    }
    aLogRec->setAnalyzeStartPtr(sAnalyzePtr);

    return IDE_SUCCESS;
}

/***************************************************************
 * < Row image info  ���� >
 *
 *          |<- optional->|<--- repeat ---->|
 * ------------------------------------------
 * |        |      | next |        |        |
 * | row    | next | slot | column | column |
 * | header | PID  | num  | length | data   |
 * ------------------------------------------
 *
 **************************************************************/
IDE_RC smiLogRec::analyzeRowImage( iduMemAllocator * aAllocator,
                                   smiLogRec       * aLogRec,
                                   UInt            * aCIDArray,
                                   smiValue        * aColValueArray,
                                   smiChainedValue * aChainedColValueArray,
                                   UInt            * aChainedValueTotalLen,
                                   UInt            * aAnalyzedValueLen,
                                   UShort          * aAnalyzedColCnt )
{
    SChar           * sAnalyzePtr;
    SChar             sRowHdrFlag;
    UShort            sColCntInRowPiece;
    UInt              sTableColCnt = 0;
    const void      * sTable;
    const smiColumn * sColumn;
    UInt              sCID;
    SChar             sErrorBuffer[256];
    UInt              i = 0;

    IDE_DASSERT( aLogRec           != NULL );
    IDE_DASSERT( aAnalyzedColCnt   != NULL );
    IDE_DASSERT( aCIDArray         != NULL );
    IDE_DASSERT( aColValueArray    != NULL );
    IDE_DASSERT( aAnalyzedValueLen != NULL );

    sAnalyzePtr = aLogRec->getAnalyzeStartPtr();

    // 1. Analyze RowHeader

    // row piece���� �÷��� ����
    sAnalyzePtr += SDC_ROWHDR_COLCOUNT_OFFSET;
    sColCntInRowPiece = aLogRec->getUShortValue(sAnalyzePtr);
    sAnalyzePtr += SDC_ROWHDR_COLCOUNT_SIZE;

    // row header�� mFlag
    idlOS::memcpy( &sRowHdrFlag,
                   sAnalyzePtr,
                   ID_SIZEOF(SChar) );

    sAnalyzePtr = aLogRec->getAnalyzeStartPtr();
    sAnalyzePtr += SDC_ROWHDR_SIZE;

    // 2. Skip NextPID, NextSlotNum
    IDE_TEST( skipOptionalInfo(aLogRec, (void **)&sAnalyzePtr, sRowHdrFlag)
              != IDE_SUCCESS );

    // 3. analyze Column value
    IDE_TEST( analyzeColumnValue( aAllocator,
                                  aLogRec,
                                  (SChar **)&sAnalyzePtr,
                                  aCIDArray,
                                  aColValueArray,
                                  aChainedColValueArray,
                                  aChainedValueTotalLen,
                                  aAnalyzedValueLen,
                                  sColCntInRowPiece,
                                  sRowHdrFlag,
                                  aAnalyzedColCnt )
              != IDE_SUCCESS );

    // 5. Check trailing NULL
    if ( aLogRec->getChangeType() == SMI_REDO_DRDB_INSERT )
    {
        if ( (sRowHdrFlag & SDC_ROWHDR_H_FLAG) == SDC_ROWHDR_H_FLAG ) //insert�� ������ log piece
        {
            // Get Meta
            IDE_TEST_RAISE( aLogRec->mGetTable( aLogRec->mMeta,
                                                (ULong)aLogRec->getTableOID(),
                                                &sTable )
                            != IDE_SUCCESS, ERR_TABLE_NOT_FOUND );
            sTableColCnt = aLogRec->mGetColumnCount(sTable);
            IDE_DASSERT( sTableColCnt != 0 );

            if ( *aAnalyzedColCnt != sTableColCnt )
            {
                IDE_DASSERT( *aAnalyzedColCnt < sTableColCnt );

                for(i = 0; i < sTableColCnt; i++)
                {
                    sColumn = aLogRec->mGetColumn(sTable, i);
                    sCID = sColumn->id & SMI_COLUMN_ID_MASK;
                    IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_INVALID_CID);

                    if ( i < *aAnalyzedColCnt )
                    {
                        if ( (sColumn->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_LOB )
                        {
                            /* BUG-30118 : Lob�� �߰� �÷��̸鼭 trailing null�� �ִ� ��� ó��.
                             *
                             * inmode lob�� ���, lob�αװ� ���� ���� �ʰ� INSERT �α׷� ��� ó���մϴ�.
                             * outmode lob�� ���, INSERT�α� ���Ŀ� LOB_PIECE_WRITE�αװ� �ɴϴ�.
                             * ó����� : inmode lob�� �̹� �м��� �� ���·� CID�� CIDArray�� ��
                             * �ֽ��ϴ�. outmode lob�� ���� �α׵� �ȿ� �����̹Ƿ� CID�� CIDArray�� �����ϴ�.
                             * �÷��� CID�� CIDArray�� �� ������ �м��� inmode lob�÷����� �����մϴ�.
                             */
                            if ( isCIDInArray(aCIDArray, sCID, *aAnalyzedColCnt)
                                 == ID_TRUE )
                            {
                                // IN-MODE LOB. Nothing todo.
                            }
                            else
                            {
                                // OUT-MODE LOB
                                aCIDArray[*aAnalyzedColCnt] = sCID;
                                *aAnalyzedColCnt += 1;
                            }
                        }
                    }
                    else
                    {
                        aCIDArray[sCID] = sCID;
                        aColValueArray[sCID].length = 0;
                        aColValueArray[sCID].value  = NULL;
                        *aAnalyzedColCnt += 1;
                    }
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    aLogRec->setAnalyzeStartPtr(sAnalyzePtr);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_TABLE_NOT_FOUND);
    {
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(ERR_INVALID_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageMemory] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT"]",
                         sCID,
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************
 * + Skip non-update column image �� ���� ����
 *
 * update dml�� ���, ���� ������Ʈ �� �÷��� �ƴϾ,
 * �� �÷��� �������� ���� �ٸ� �÷����� ������ ���� �� �ִ�.
 * RP���� �ʿ��� ������ ������Ʈ �� �÷� ���̹Ƿ�, �� ����
 * �÷� ������ skip�Ѵ�.
 *
 * EX) RP  info   =>    C5�� seq : 3,  C8�� seq : 6
 *     Row image  =>    C3, C4, C5, C6, C7, C8 �� value
 *
 * ���� ���, �ش� row piece���� C3~C8�� �÷��� ��� �ִ�.
 * ���� ������Ʈ�� C5�� C8�� �߻��Ͽ�����, overwrite_row_piece
 * �� �߻��Ͽ�, row image ���� C3~C8�� image�� ��� ��������.
 * RP���� �ʿ��� ���� C5, C8�� image�̹Ƿ�, row image ����
 * C5�� seq�� �ش��ϴ� ����° image�� C8�� seq�� �ش��ϴ� 6��°
 * image�� �о��.
 *
 * log type�� update_row_piece�� ���� �� �ٸ���.
 * ������ ������Ʈ �� �÷��� image���� row image �� ����
 * ������ seq������ ã�� ����, RP Info�� ��ϵ� ������ ������
 * row image ���� �о��.
 *
 * EX) RP  info   =>    C5�� seq : 3,  C8�� seq : 6
 *     Row image  =>    C5, C8 �� value
 *
 **************************************************************/
IDE_RC smiLogRec::analyzeColumnValue( iduMemAllocator * aAllocator,
                                      smiLogRec       * aLogRec,
                                      SChar          ** aAnalyzePtr,
                                      UInt            * aCIDArray,
                                      smiValue        * aColValueArray,
                                      smiChainedValue * aChainedColValueArray,
                                      UInt            * aChainedValueTotalLen,
                                      UInt            * aAnalyzedValueLen,
                                      UShort            aColumnCountInRowPiece,
                                      SChar             aRowHdrFlag,
                                      UShort          * aAnalyzedColCnt )
{
    const void      * sTable;
    const smiColumn * sColumn; 
    SChar             sErrorBuffer[256];
    UInt              sCID;
    UShort            sColumnLen;
    SInt              sColumnTotalLen;
    SInt              sLogType;
    UShort            i;
    UInt              sCheckChainedValue;
    UShort            sColSeq;
    UInt              sAnalyzedValueLen = 0;
    UInt              sSaveAnalyzeLen = 0;
    UInt              sAnalyzeColCntInRowPiece = 0;
    idBool            sIsOutModeLob;
    idBool            sIsNull;

    IDE_TEST_RAISE( aLogRec->mGetTable( aLogRec->mMeta,
                                        (ULong)aLogRec->getTableOID(),
                                        &sTable )
                    != IDE_SUCCESS, ERR_TABLE_NOT_FOUND );

    for(i=0; i<aColumnCountInRowPiece; i++)
    {
        // row piece������ �м��� �÷��� ���� rp info ������ �÷�����
        // ��ġ�ϸ� column value�м��� �����Ѵ�.
        if ( aLogRec->getUpdateColCntInRowPiece() <= sAnalyzeColCntInRowPiece )
        {
            break;
        }

        sColSeq = aLogRec->getColumnSequence( sAnalyzeColCntInRowPiece );
        sCID = (UInt)aLogRec->getColumnId(sColSeq) & SMI_COLUMN_ID_MASK;

        sColumn = aLogRec->mGetColumn(sTable, sCID);

        /*
         * <TRAILING NULL�� ������Ʈ �ϴ� ���>
         * UPDATE_OVERWRITE �αװ� ���� ���� split�� �߻��� ����,
         * trailing null�� ��� �� �����̴�.
         * trailing null�� ���, sm������ �����͸� �������� �����Ƿ�
         * rowImage���� ������ ���� �ʴ´�.
         * RP info �� seq�� id���� ���� ID_USHORT_MAX�� ID_UINT_MAX��
         * �����Ͽ�, �̸� ������ rowImage�� �÷��̹��� �м��ܰ踦
         * skip�ϰ�, �ٷ� null�� ä�쵵�� �Ѵ�.
         */
        if ( (UInt)aLogRec->getColumnId(sColSeq) == ID_UINT_MAX )
        {
            IDE_DASSERT( aLogRec->getChangeType() == SMI_UNDO_DRDB_UPDATE_OVERWRITE );

            aChainedColValueArray[sCID].mColumn.value  = NULL;
            aChainedColValueArray[sCID].mColumn.length = 0;
            *aAnalyzedColCnt += 1;
            sAnalyzeColCntInRowPiece += 1;
            aChainedValueTotalLen[sCID] = 0;
            continue;
        }

        // Get column length
        analyzeColumnAndMovePtr( (SChar **)aAnalyzePtr,
                                 &sColumnLen,
                                 &sIsOutModeLob,
                                 &sIsNull );

        // PROJ-1862 In Mode Lob
        // ���� Out Mode LOB Descriptor�̸� �����Ѵ�.
        if ( sIsOutModeLob == ID_TRUE )
        {
            IDE_DASSERT( sColumnLen == ID_SIZEOF(sdcLobDesc) );
            IDE_CONT(SKIP_COLUMN_ANLZ);
        }

        /*
         * Skip non-update column image.
         * �Ʒ��� �� Ÿ���� ���� ������Ʈ ��� �÷����� row image �� ���´�.
         * ���� sColSeq�� i�� ��ġ���� ���� �� �ִ�.
         * ������ �� ���� Ÿ���� sColSeq�� �ش��ϴ� ��ġ�� �����ϴ� row image ��
         * value���� ���ƾ��Ѵ�.
         * DELETE ROW PIECE FOR UPDATE�� ���, �� ù �÷��� ���ؼ��� image�� ���⶧����
         * �׻� sColSeq�� i�� ����.
         */
        if ( ( aLogRec->getChangeType() != SMI_UNDO_DRDB_UPDATE ) &&
             ( aLogRec->getChangeType() != SMI_REDO_DRDB_UPDATE ) )
        {
            if ( sColSeq != i )
            {
                /* INSERT�ÿ� �߰��� lob�����Ͱ� ���ִ� ���, sColSeq != i�� �� �ִ�.
                   log descriptor��ŭ �ǳ� �ڴ�.
                   if (aLogRec->getChangeType() == SMI_REDO_DRDB_INSERT)
                   {
                   IDE_DASSERT(sColumnLen == sdcLobDescriptorSize);
                   }
                */

                IDE_CONT(SKIP_COLUMN_ANLZ);
            }
        }

        // Get column total length
        sColumnTotalLen = aLogRec->getColumnTotalLength(sColSeq);

        // Set log type
        if ( sColumnTotalLen == SMI_UNDO_LOG )
        {
            sLogType = SMI_UNDO_LOG;
        }
        else
        {
            sLogType = SMI_REDO_LOG;
        }

        // Set CID
        if ( sLogType == SMI_REDO_LOG )
        {
            aCIDArray[*aAnalyzedColCnt] = sCID;
        }
        else
        {
            /* TASK-5030 */
            if ( aLogRec->needSupplementalLog() == ID_TRUE )
            {
                aCIDArray[*aAnalyzedColCnt] = sCID;
            }
        }

        // NULL Value
        if ( sIsNull == ID_TRUE )
        {
            if ( sLogType == SMI_UNDO_LOG )
            {
                aChainedColValueArray[sCID].mColumn.value  = NULL;
                aChainedColValueArray[sCID].mColumn.length = 0;
                aChainedValueTotalLen[sCID] = 0;
            }
            else if ( sLogType == SMI_REDO_LOG )
            {
                aColValueArray[sCID].value  = NULL;
                aColValueArray[sCID].length = 0;
            }
            else
            {
                IDE_DASSERT(ID_FALSE);
            }

            *aAnalyzedColCnt += 1;
            sAnalyzeColCntInRowPiece += 1;

            continue;
        }

        sCheckChainedValue = checkChainedValue(aRowHdrFlag,
                                               sColSeq,
                                               aColumnCountInRowPiece,
                                               sLogType);

        /*
         * Set analyzedValueLen
         *
         * �Ҵ�� �������� value���� �� ��ġ�� ����ϱ� ���� �������
         * �м��� ���̸� �����Ѵ�.
         * First chained value�� ���, �м��� �����̹Ƿ�,
         * ������� �м��� ���̴� 0�̴�.
         */
        if ( ( sCheckChainedValue == SMI_FIRST_CHAINED_VALUE ) ||
             ( sCheckChainedValue == SMI_NON_CHAINED_VALUE ) )
        {
            sAnalyzedValueLen = 0;
        }
        else
        {
            sAnalyzedValueLen = *aAnalyzedValueLen;
        }

        // undo log
        if ( sLogType == SMI_UNDO_LOG )
        {
            IDE_TEST(copyBeforeImage(aAllocator,
                                     sColumn,
                                     aLogRec,
                                     *aAnalyzePtr,
                                     &(aChainedColValueArray[sCID]),
                                     &(aChainedValueTotalLen[sCID]),
                                     &sAnalyzedValueLen,
                                     sColumnLen,
                                     sCheckChainedValue)
                     != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST(copyAfterImage(aAllocator,
                                    sColumn,
                                    aAnalyzePtr,
                                    &(aColValueArray[sCID]),
                                    &sAnalyzedValueLen,
                                    sColumnLen,
                                    sColumnTotalLen,
                                    sCheckChainedValue)
                     != IDE_SUCCESS );
        }

        /*
         * ������� �м��� chained value�� ���̸� ���� log piece�м��� �����ؾ��Ѵ�.
         * �м����� ������ ����, �̸��Ҵ� ���� �������� ������ ������ ��ġ�� �� �� �ִ�.
         * �� row piece ������ �� �ʿ� chained value�� ������ ���, ���� ���� chained value�� ����
         * ������ �δ� �뵵�� ����Ѵ�.
         * FIRST_CHAINE_VALUE �Ǵ� MIDDLE_CHAINED_VALUE �϶�, ���� row piece�� ������ value�� ���縦
         * ����, ������� �м��� ���̸� �ѱ��.
         */
        if ( ( sCheckChainedValue != SMI_LAST_CHAINED_VALUE ) &&
             ( sCheckChainedValue != SMI_NON_CHAINED_VALUE ) )
        {
            sSaveAnalyzeLen = sAnalyzedValueLen;
        }

        // �ߺ�ī������ ���� ���� ����. �� value�� ���� First, Middle, Last piece�� ���� ��� ī����
        // �ؼ��� �ȵǹǷ�, Last piece�ϳ��� ���ؼ��� ��.
        if ( (sCheckChainedValue == SMI_LAST_CHAINED_VALUE ) ||
             (sCheckChainedValue == SMI_NON_CHAINED_VALUE ))
        {
            // ������� �м��� �÷� �� - �� ������ trailing null üũ���� ����Ѵ�.
            *aAnalyzedColCnt += 1;
        }

        sAnalyzeColCntInRowPiece += 1;

        IDE_EXCEPTION_CONT(SKIP_COLUMN_ANLZ);

        *aAnalyzePtr += sColumnLen;
    }

    if ( sSaveAnalyzeLen > 0 )
    {
        *aAnalyzedValueLen = sSaveAnalyzeLen;
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_TABLE_NOT_FOUND);
    {
        idlOS::snprintf( sErrorBuffer,
                         ID_SIZEOF(sErrorBuffer),
                         "[smiLogRec::analyzeColumnValue] Table Not Found (OID: %"ID_UINT64_FMT")",
                         (ULong)aLogRec->getTableOID() );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************
 *
 * Disk DML undo log�� value�м� �Լ�.
 *
 * Ư¡ :
 *  1. �м��ϴ� column value�� tatal length�� ���� ���Ѵ�.
 *  2. column value�� �α� ������ ����� ������ �����ϴ�.
 *     chained value�� ���, ó���� �����ϴ� value�� ��ü value�� ù�κ��̸�,
 *     �������� �����ϴ� value�� ��ü value�� ������ �κ��̴�.
 *
 * undo log value�� ������ �� ������ ���� ����� ������.
 *
 * 1. non-chained value
 *    �� ���, log piece���� column value�� length�� tatal length�� �����ϴ�.
 *    ���� pool���� �Ҵ���� ������ copy�� �� �ʿ䰡 �����Ƿ�,
 *    redo log�� value�� ����������, normal alloc�� �޴´�.
 *
 * 2. chained value
 *    column value�� �� ���̸� �𸣹Ƿ�, �Ź� �м� �ø����� malloc�� ���̱�
 *    ���� pool�� ���� pool_element_size��ŭ �Ҵ�޴´�.
 *
 *    2-1. ���� pool_element�� ������ �����Ϸ��� value�� ���̺��� Ŭ��.
 *         ������ value�� �����ϰ�, pool element�� ������ ��Ȳ�� �����Ѵ�.
 *
 *    2-2. ���� pool_element�� ������ �����Ϸ��� value�� ���̺��� ������.
 *         �켱 ���� ������ value�� �����Ѵ�. pool element�� ������ ��Ȳ�� full�̴�.
 *         ���� value�� ���縦 ����, ���ο� ������ �Ҵ� �޴´�.
 *         value�� ���簡 ��� ���� ������ �ݺ��Ѵ�.
 *
 **************************************************************/
IDE_RC smiLogRec::copyBeforeImage( iduMemAllocator * aAllocator,
                                   const smiColumn * aColumn,
                                   smiLogRec       * aLogRec,
                                   SChar           * aAnalyzePtr,
                                   smiChainedValue * aChainedValue,
                                   UInt            * aChainedValueTotalLen,
                                   UInt            * aAnalyzedValueLen,
                                   UShort            aColumnLen,
                                   UInt              aColStatus )
{
    smiChainedValue   * sChainedValue;
    SChar             * sAnalyzePtr = NULL;
    UInt                sColumnLen;
    UInt                sState          = 0;
    UInt                sRemainLen      = 0;
    UInt                sRemainSpace    = 0;
    UInt                sCopyLen        = 0;
    UInt                sChainedValuePoolSize   = 0;

    sChainedValue = aChainedValue;
    
    /*
     * PROJ-2047 Strengthening LOB
     * In LOB, the empty value and null value should be distinguished.
     * But the empty value and null value's length is 0.
     * Therefore the dummy header was added to LOB value.
     * In case of empty value, empty value length is 1.
     * In case of null value, null value is null and null value length is 0.
     * The receiver calculates the real value length.(value length - dummy header length)
     */
    sAnalyzePtr = aAnalyzePtr;
    sColumnLen  = aColumnLen;
    
    if ( SMI_IS_LOB_COLUMN(aColumn->flag) == ID_TRUE )
    {
        if ( (aColStatus == SMI_FIRST_CHAINED_VALUE) ||
             (aColStatus == SMI_NON_CHAINED_VALUE ) )
        {
            sAnalyzePtr -= SMI_LOB_DUMMY_HEADER_LEN;
            sColumnLen  += SMI_LOB_DUMMY_HEADER_LEN;
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

    // non-chained value
    // value�� ��ü ���̸� �˰� �����Ƿ� size��ŭ normal alloc�Ͽ� �����Ҵ� ��, �����Ѵ�.
    if ( aColStatus == SMI_NON_CHAINED_VALUE )
    {

        IDU_FIT_POINT( "smiLogRec::copyBeforeImage::SMI_NON_CHAINED_VALUE::malloc" );

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_RP_RPS,
                                     sColumnLen,
                                     (void **)&sChainedValue->mColumn.value,
                                     IDU_MEM_IMMEDIATE,
                                     aAllocator )
                  != IDE_SUCCESS );
        sState = 1;

        sChainedValue->mColumn.length  = sColumnLen;
        sChainedValue->mAllocMethod    = SMI_NORMAL_ALLOC;
        sChainedValue->mLink           = NULL;

        idlOS::memcpy( (void *)sChainedValue->mColumn.value,
                       sAnalyzePtr,
                       sColumnLen );

        *aChainedValueTotalLen = sColumnLen;

        IDE_CONT(SKIP_COPY_VALUE);
    }
    // chained value
    else
    {
        // �� log piece�� �м��ϸ�, �̹� linkedlist�� ������� �� ���� �� �ִ�.
        // Linkedlist�� ������ ������ �̵��Ѵ�.
        if ( aChainedValue->mAllocMethod != SMI_NON_ALLOCED )
        {
            while( sChainedValue->mLink != NULL )
            {
                sChainedValue = sChainedValue->mLink;
            }
        }

        /*
         * undo log�� ����� �÷� ������� ������ �α�Ǿ� �ִ�.
         * �α��� ó���� first_chained ���´�, �м��� ó���̹Ƿ�
         * ���� �Ҵ��� �Ѵ�. �� ���¿����� value�� chained value��
         * �� �պκ� value�̴�.
         */
        if ( aColStatus == SMI_FIRST_CHAINED_VALUE )
        {
            IDE_DASSERT( sChainedValue->mAllocMethod == SMI_NON_ALLOCED );

            IDE_TEST( aLogRec->chainedValueAlloc( aAllocator, aLogRec, &sChainedValue )
                      != IDE_SUCCESS );
        }

        // ������� ����� �÷� ����� ���̸� ������Ų��.
        *aAnalyzedValueLen     += sColumnLen;
        *aChainedValueTotalLen  = *aAnalyzedValueLen;
    }

    // ���� ����� ����, pool element size�� �޾ƿ´�.
    sChainedValuePoolSize = aLogRec->getChainedValuePoolSize();

    /*
     * pool element�� value�� copy�� ����, ���� ������ ����Ѵ�.
     * ������ smiChainedValue ���� smiValue�� ����� ������ �ִ�.
     * �� smiVale�� value�� length�� �� ��� �ȿ����� value�� length�� �ǹ��Ѵ�.
     * pool element size���� ���ݱ��� ������ ����� value�� ���̸�ŭ�� ����
     * ���� ������ ���Ѵ�.
     */
    sRemainSpace = sChainedValuePoolSize - sChainedValue->mColumn.length;

    // value size�� ���� ���� ũ�⺸�� ũ�Ƿ�, memory pool���� �߰� �Ҵ��� �޾ƾ��Ѵ�.
    if ( sRemainSpace < sColumnLen )
    {
        if ( sRemainSpace > 0 )
        {
            // �����ִ� ������ �켱 ���縦 �Ѵ�.
            idlOS::memcpy( (SChar *)sChainedValue->mColumn.value + sChainedValue->mColumn.length,
                           sAnalyzePtr, 
                           sRemainSpace );
        }
        // copyLen�� ������� ������ value�� ũ���̴�.
        sCopyLen = sRemainSpace;
        // pool element ���� ������� ������ Value�� ũ���̴�.
        sChainedValue->mColumn.length += sRemainSpace;
        sAnalyzePtr += sRemainSpace;

        // ������ value�� ũ�Ⱑ log piece�� column value�� ũ��� ������ ������ Loop.
        while( sCopyLen < sColumnLen )
        {
            // ������ ���� �Ҵ�޴´�. old node�� ���ڷ� �Ѱ��ָ�, new node�� �ּҸ�
            // ���� �޴´�
            IDE_TEST( aLogRec->chainedValueAlloc( aAllocator, aLogRec, &sChainedValue )
                      != IDE_SUCCESS );

            // log piece�� column value�� ũ�⿡�� ���ݱ��� ������ ũ���� ����
            // ������ ���� ������ �з��� ����Ѵ�.
            sRemainLen = sColumnLen - sCopyLen;

            // ���� �Ҵ���� pool element size����, �����ִ� ����ũ�Ⱑ �۴ٸ�,
            // log piece�� value ������ ���̴�.
            if ( sRemainLen < sChainedValuePoolSize )
            {
                idlOS::memcpy( (void *)sChainedValue->mColumn.value,
                               sAnalyzePtr, 
                               sRemainLen);
                sCopyLen += sRemainLen;
                sChainedValue->mColumn.length += sRemainLen;
                sAnalyzePtr += sRemainLen;
            }
            else
            {
                idlOS::memcpy( (void *)sChainedValue->mColumn.value,
                               sAnalyzePtr,
                               sChainedValuePoolSize );
                sCopyLen += sChainedValuePoolSize;
                sChainedValue->mColumn.length += sChainedValuePoolSize;
                sAnalyzePtr += sChainedValuePoolSize;
            }
        }
    }
    else
    {
        idlOS::memcpy( (SChar *)sChainedValue->mColumn.value + sChainedValue->mColumn.length,
                       sAnalyzePtr,
                       sColumnLen );
        sChainedValue->mColumn.length += sColumnLen;
    }

    IDE_EXCEPTION_CONT(SKIP_COPY_VALUE);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( (void *)sChainedValue->mColumn.value )
                        == IDE_SUCCESS );
            sChainedValue->mColumn.value = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***************************************************************
 *
 * Disk DML redo log�� value�м� �Լ�.
 *
 * Ư¡ :
 *  1. �м��ϴ� column value�� tatal length�� �˰� �ִ�.
 *  2. chained value piece�� �α� ������ �Ųٷ� �Ǿ��ִ�.
 *     chained value�� ���, ó���� �����ϴ� value�� ��ü value�� ù�κ��̸�,
 *     �������� �����ϴ� value�� ��ü value�� ù �κ��̴�.
 *
 ***************************************************************/
IDE_RC smiLogRec::copyAfterImage(iduMemAllocator  * aAllocator,
                                 const smiColumn  * aColumn,
                                 SChar           ** aAnalyzePtr,
                                 smiValue         * aColValue,
                                 UInt             * aAnalyzedValueLen,
                                 UShort             aColumnLen,
                                 SInt               aColumnTotalLen,
                                 UInt               aColStatus)
{
    UInt    sOffset = 0;
    SInt    sColumnTotalLen;

    IDE_DASSERT(aColumnTotalLen >= 0); // data�� null�� ��� 0.
    IDE_DASSERT(aColumnTotalLen >= (SInt)aColumnLen);

    sColumnTotalLen = aColumnTotalLen;
 
    /*
     * PROJ-2047 Strengthening LOB
     * In LOB, the empty value and null value should be distinguished.
     * But the empty value and null value's length is 0.
     * Therefore the dummy header was added to LOB value.
     * In case of empty value, empty value length is 1.
     * In case of null value, null value is null and null value length is 0.
     * The receiver calculates the real value length.(value length - dummy header length)
     */
    if ( SMI_IS_LOB_COLUMN(aColumn->flag) == ID_TRUE )
    {
        sColumnTotalLen += SMI_LOB_DUMMY_HEADER_LEN;
    }
    
    // non-chained value
    if ( aColStatus == SMI_NON_CHAINED_VALUE )
    {
        sOffset = sColumnTotalLen - (SInt)aColumnLen;
        
        if ( SMI_IS_LOB_COLUMN(aColumn->flag) == ID_TRUE )
        {
            IDE_DASSERT( sOffset == SMI_LOB_DUMMY_HEADER_LEN );
        }
        else
        {
            IDE_DASSERT( sOffset == 0 );
        }
        
        IDU_FIT_POINT( "smiLogRec::copyAfterImage::SMI_NON_CHAINED_VALUE::malloc" );

        IDE_TEST(iduMemMgr::malloc(IDU_MEM_RP_RPS,
                                   sColumnTotalLen,
                                   (void **)&aColValue->value,
                                   IDU_MEM_IMMEDIATE,
                                   aAllocator)
                 != IDE_SUCCESS );

        aColValue->length = sColumnTotalLen;
    }
    else
    {
        // chained value �м��� �����̹Ƿ�, total length��ŭ�� ������ �Ҵ��Ѵ�.
        if ( aColStatus == SMI_FIRST_CHAINED_VALUE )
        {
            IDU_FIT_POINT( "smiLogRec::copyAfterImage::SMI_CHAINED_VALUE::malloc" );
            
            IDE_TEST(iduMemMgr::malloc(IDU_MEM_RP_RPS,
                                       sColumnTotalLen,
                                       (void **)&aColValue->value,
                                       IDU_MEM_IMMEDIATE,
                                       aAllocator)
                     != IDE_SUCCESS );

            aColValue->length = sColumnTotalLen;

            // redo log������ chained value�� �� �� value���� �α�ǹǷ�,
            // ����� ������ �ں��� �Ѵ�.
            sOffset = sColumnTotalLen - (SInt)aColumnLen;
        }
        // chained value�� �߰� �Ǵ� ������ piece�� ���, ������� �м��� ���̸�ŭ
        // �� �� ������ ���ܵΰ� �����Ѵ�.
        else
        {
            IDE_DASSERT( aColValue->length == (UInt)sColumnTotalLen );
            
            IDE_DASSERT((aColStatus == SMI_MIDDLE_CHAINED_VALUE ) ||
                        (aColStatus == SMI_LAST_CHAINED_VALUE ));

            IDE_DASSERT((sColumnTotalLen - *aAnalyzedValueLen) >= aColumnLen);

            sOffset = sColumnTotalLen - (*aAnalyzedValueLen + (UInt)aColumnLen);
        }

        *aAnalyzedValueLen += (UInt)aColumnLen;
    }

    idlOS::memcpy((SChar *)aColValue->value + sOffset,
                  *aAnalyzePtr,
                  aColumnLen);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***************************************************************
 *
 * Row image info ����, nextPID �� nextSlotNum��
 * skip�ؾ����� �Ǵ��Ͽ�  ó���Ѵ�.
 *
 * flag�� <L>�� ���� �Ǿ� �ִ� ����,
 * log type��  SDC_UNDO_UPDATE_ROW_PIECE
 *             SDR_SDC_UPDATE_ROW_PIECE
 *             SDC_UNDO_DELETE_FIRST_COLUMN_PIECE
 * �� ���, �������� �ʴ´�.
 *
 **************************************************************/
IDE_RC smiLogRec::skipOptionalInfo(smiLogRec  * aLogRec,
                                   void      ** aAnalyzePtr,
                                   SChar        aRowHdrFlag)
{
    UInt    sLogType;
    SChar * sAnalyzePtr = (SChar *)*aAnalyzePtr;

    // Flag�� <L>�� ���, �α���� �ʴ� �����̴�.
    if ( (aRowHdrFlag & SDC_ROWHDR_L_FLAG) == SDC_ROWHDR_L_FLAG )
    {
        // Nothing to do.
    }
    else
    {
        sLogType = aLogRec->getChangeType();

        if ( (sLogType == SMI_UNDO_DRDB_UPDATE) ||
             (sLogType == SMI_REDO_DRDB_UPDATE) ||
             (sLogType == SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN) )
        {
            // Nothing to do.
        }
        else
        {
            sAnalyzePtr += ID_SIZEOF(scPageID)+ ID_SIZEOF(scSlotNum);
            *aAnalyzePtr = sAnalyzePtr;
        }
    }

    return IDE_SUCCESS;
}

/***************************************************************
 *
 * chainedValue�� ���¸� Ȯ���Ѵ�.
 * ���´� chained value �Ǵ� non-chained value�̸�,
 * chained value�� ���, �α� �м� ������ ��������
 * value�� ����, �߰�, ������ �����Ѵ�.
 *
 * ���°� :
 * A. chained value       1. SMI_FIRST_CHAINED_VALUE
 *                        2. SMI_MIDDLE_CHAINED_VALUE
 *                        3. SMI_LAST_CHAINED_VALUE
 * B. non-chained value   4. SMI_NON_CHAINED_VALUE
 *
 * ���� ���°����� ������ ������,
 * FIRST_CHAINED_VALUE ��� ����,
 * log�� ������ �������� chained value�� ó���̶�� ������
 * ���� value�� ù �κ��� �ƴ϶�� ���̴�.
 * �м��ÿ� ���� ����� �����ϰ� �ϱ� ���Ͽ� ���� �̸�����,
 * REDO LOG�� ��쿡�� value�� �� ���κ��� FIRST_CHAINED_VALUE�� �ȴ�.
 * �ݴ�� UNDO LOG�� ��쿡�� ���°��� value�� ���°� �����ϴ�.
 *
 **************************************************************/
UInt smiLogRec::checkChainedValue(SChar    aRowHdrFlag,
                                  UInt     aPosition,
                                  UShort   aColCntInRowPiece,
                                  SInt     aLogType)
{
    UInt sCheckChainedValue;

    // row piece���� �ݵ�� �ϳ� �̻��� �÷��� �����ؾ��ϹǷ�
    // -1�� �Ͽ��� ������ �� ���� ����.
    aColCntInRowPiece -= 1;

    // P�� N �÷��װ� ���ÿ� �����ϸ�, row piece���� �÷��� 1�� ���̸�,
    // chained value�� �߰� piece�̴�.
    if ( ( (aRowHdrFlag & SDC_ROWHDR_N_FLAG) == SDC_ROWHDR_N_FLAG ) &&
         ( (aRowHdrFlag & SDC_ROWHDR_P_FLAG) == SDC_ROWHDR_P_FLAG ) &&
         ( aColCntInRowPiece == 0 ) )
    {
        sCheckChainedValue = SMI_MIDDLE_CHAINED_VALUE;
    }
    else
    {
        // REDO LOG�� �α� ������ ����Ǵ� �÷� ������ �������̴�.
        // ����, ����� �� ���� ���� �α�ȴ�.
        if ( aLogType == SMI_REDO_LOG )
        {
            // row piece������ �������� ��ġ�ϴ� chained value
            if ( ( (aRowHdrFlag & SDC_ROWHDR_N_FLAG) == SDC_ROWHDR_N_FLAG ) &&
                 ( aPosition == aColCntInRowPiece))
            {
                sCheckChainedValue = SMI_LAST_CHAINED_VALUE;
            }
            // row piece������ ó���� ��ġ�ϴ� chained value
            else if ( ( (aRowHdrFlag & SDC_ROWHDR_P_FLAG) == SDC_ROWHDR_P_FLAG ) &&
                      ( aPosition == 0) )
            {
                sCheckChainedValue = SMI_FIRST_CHAINED_VALUE;
            }
            else
            {
                sCheckChainedValue = SMI_NON_CHAINED_VALUE;
            }
        }
        // UNDO LOG�� �α� ������ �������̴�.
        else
        {
            // row piece������ ó���� ��ġ�ϴ� chained value
            if ( ( (aRowHdrFlag & SDC_ROWHDR_P_FLAG) == SDC_ROWHDR_P_FLAG ) &&
                 ( aPosition == 0 ) )
            {
                sCheckChainedValue = SMI_LAST_CHAINED_VALUE;
            }
            // row piece������ �������� ��ġ�ϴ� chained value
            else if ( ( (aRowHdrFlag & SDC_ROWHDR_N_FLAG) == SDC_ROWHDR_N_FLAG ) &&
                      ( aPosition == aColCntInRowPiece ) )
            {
                sCheckChainedValue = SMI_FIRST_CHAINED_VALUE;
            }
            else
            {
                sCheckChainedValue = SMI_NON_CHAINED_VALUE;
            }
        }
    }

    return sCheckChainedValue;
}

/***************************************************************
 * LOB Piece write for DML
 * -------------------------------------------------
 * |           |  Piece  |  LOB  | Column | Total  |
 * | sdrLogHdr |   Len   | Piece |   ID   |  Len   |
 * |           | (UInt)  | Value | (UInt) | (UInt) |
 * -------------------------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeWriteLobPieceLogDisk( iduMemAllocator * aAllocator,
                                               smiLogRec * aLogRec,
                                               smiValue  * aAColValueArray,
                                               UInt      * aCIDArray,
                                               UInt      * aAnalyzedValueLen,
                                               UShort    * aAnalyzedColCnt,
                                               idBool    * aDoWait,
                                               UInt      * aLobCID,
                                               idBool      aIsAfterInsert )
{
    UInt        sPieceLen;
    SChar     * sValue;
    UInt        sCID;
    UInt        sTotalLen;
    SChar     * sOffsetPtr;

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aAColValueArray != NULL );
    IDE_DASSERT( aAnalyzedValueLen != NULL );
    IDE_DASSERT( aDoWait != NULL );

    // ��ũ �α��� mContType�� ���� ������ �αװ� �̾����� �Ǵ��Ѵ�.
    *aDoWait = ((smrContType)aLogRec->getContType() == SMR_CT_END) ?
               ID_FALSE : ID_TRUE;

    sOffsetPtr = aLogRec->getAnalyzeStartPtr();

    /* skip sdrLogHdr */
    sOffsetPtr += ID_SIZEOF(sdrLogHdr);

    /* Get Piece Length */
    sPieceLen = aLogRec->getUIntValue(sOffsetPtr);
    sOffsetPtr += ID_SIZEOF(UInt);

    /* Set Value Pointer */
    sValue = sOffsetPtr;
    sOffsetPtr += sPieceLen;

    /* Get Column ID */
    sCID = aLogRec->getUIntValue(sOffsetPtr) & SMI_COLUMN_ID_MASK;
    *aLobCID = sCID;
    sOffsetPtr += ID_SIZEOF(UInt);

    /* Get LOB Column Total length */
    sTotalLen = aLogRec->getUIntValue(sOffsetPtr) + SMI_LOB_DUMMY_HEADER_LEN;
    sOffsetPtr += ID_SIZEOF(UInt);

    /* �ش� LOB column value�� ó�� ���� */
    if ( aAColValueArray[sCID].value == NULL )
    {
        aAColValueArray[sCID].length = sTotalLen;

        if ( aIsAfterInsert == ID_FALSE )
        {
            /* PROJ-1705
             * CID�� �켱 �м��� �÷��� ���� �ڿ��� �־�д�.
             * �� ���ڵ忡 ���� ��� �м��� ���� ��, CID�� �����۾��� �ϱ⶧���� ������.
             * ��, insert�� ���, lob�� null �����Ͱ� �ԷµǴ� ��츦 ���� �̹� cid�� ���
             * �־����Ƿ� �����Ѵ�.
             */
            aCIDArray[*aAnalyzedColCnt] = sCID;

            /* PROJ-1705
             * insert�� ���, non-Lob �÷����� insert�۾��� ��ģ ��, ���� �м����� ����
             * lob�÷����� ī�����Ͽ� anlyzedColCnt�� ������Ų��.
             * ������ lob�� null�����Ͱ� insert�� ���, �αװ� ���� ���� �ʱ� ������
             * ������� �м��� ���¸����ε� (null����� �̹� �ʱ�ȭ �Ǿ� �����Ƿ�)
             * ����ȭ�� �����ϰԲ� �ϱ� �����̴�. �̹� �����Ǿ� �ֱ⶧���� �����Ѵ�.
             */
            *aAnalyzedColCnt += 1;
        }

        /* -> NULL, EMPTY �� Update �Ǵ� ��쿡�� ���ٸ��� ������ �����Ƿ�
           ���� �������� ����. */
        IDE_TEST_CONT(sTotalLen == SMI_LOB_DUMMY_HEADER_LEN, SKIP_UPDATE_TO_NULL);

        *aAnalyzedValueLen = SMI_LOB_DUMMY_HEADER_LEN;
        IDE_TEST(iduMemMgr::malloc( IDU_MEM_RP_RPS,
                                    sTotalLen,
                                    (void **)&aAColValueArray[sCID].value,
                                    IDU_MEM_IMMEDIATE,
                                    aAllocator)
                 != IDE_SUCCESS );
    }

    idlOS::memcpy((SChar *)aAColValueArray[sCID].value + *aAnalyzedValueLen, sValue, sPieceLen);
    *aAnalyzedValueLen += sPieceLen;

    IDE_EXCEPTION_CONT(SKIP_UPDATE_TO_NULL);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***************************************************************
 * LOB Prepare for Write log
 * --------------------------------
 * |  Table  | Column |  Primary  |
 * |   OID   |   ID   |    Key    |
 * | (smOID) | (UInt) |    Area   |
 * |(vULong) |        |           |
 * --------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeLobCursorOpenMem(smiLogRec  *aLogRec,
                                          UInt       *aPKColCnt,
                                          UInt       *aPKCIDArray,
                                          smiValue   *aPKColValueArray,
                                          ULong      *aTableOID,
                                          UInt       *aCID)
{
    SChar    *sAnlzPtr = aLogRec->getAnalyzeStartPtr();
    UInt      sOffset = 0;
    UInt      sPKSize;
    smOID     sTableOID;

    /* Argument �ּ�
       aLogRec : ���� Log Record
       aPKColCnt : Primary Key column count
       aPKCIDArray : Primary Key Column ID array
       aPKColValueArray : Primary Key Column Value array
       aTableOID : ���� LOB cursor�� open�ϴ� table OID
       aCID : open�� LOB Column ID
    */

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aPKColCnt != NULL );
    IDE_DASSERT( aPKCIDArray != NULL );
    IDE_DASSERT( aPKColValueArray != NULL );
    IDE_DASSERT( aTableOID != NULL );
    IDE_DASSERT( aCID != NULL );

    sTableOID = aLogRec->getvULongValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(vULong);
    *aTableOID = (ULong)sTableOID;

    *aCID = aLogRec->getUIntValue(sAnlzPtr, sOffset) & SMI_COLUMN_ID_MASK;

    sOffset += ID_SIZEOF(UInt);

    // PROJ-1705�� ���� PKLog�м� �Լ����� PKMem�� PKDisk�� ��������.
    // Lob�α״� PROJ-1705������ PK Log������ �����Ƿ�,
    // ������ �α� ���� �м� �Լ��� PKMem���� �м��ϵ��� �Ѵ�.
    IDE_TEST( analyzePKMem( aLogRec,
                            sAnlzPtr + sOffset,
                            aPKColCnt,
                            &sPKSize,
                            aPKCIDArray,
                            aPKColValueArray,
                            NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***************************************************************
 * PROJ-1705
 * analyzeLobCursorOpenMem����
 * PK������ ������ value�� �������°� �ٸ���.
 **************************************************************/
IDE_RC smiLogRec::analyzeLobCursorOpenDisk(smiLogRec * aLogRec,
                                           UInt       * aPKColCnt,
                                           UInt       * aPKCIDArray,
                                           smiValue   * aPKColValueArray,
                                           ULong      * aTableOID,
                                           UInt       * aCID)
{
    SChar * sAnlzPtr;
    smOID   sTableOID;

    /* Argument �ּ�
       aLogRec : ���� Log Record
       aPKColCnt : Primary Key column count
       aPKCIDArray : Primary Key Column ID array
       aPKColValueArray : Primary Key Column Value array
       aTableOID : ���� LOB cursor�� open�ϴ� table OID
       aCID : open�� LOB Column ID
    */

    // Simple argument check code
    IDE_ASSERT( aLogRec          != NULL );
    IDE_ASSERT( aPKColCnt        != NULL );
    IDE_ASSERT( aPKCIDArray      != NULL );
    IDE_ASSERT( aPKColValueArray != NULL );
    IDE_ASSERT( aTableOID        != NULL );
    IDE_ASSERT( aCID             != NULL );

    sAnlzPtr = aLogRec->getAnalyzeStartPtr();
    sTableOID = aLogRec->getvULongValue(sAnlzPtr);
    sAnlzPtr += ID_SIZEOF(vULong);
    *aTableOID = (ULong)sTableOID;

    *aCID = aLogRec->getUIntValue(sAnlzPtr) & SMI_COLUMN_ID_MASK;

    sAnlzPtr += ID_SIZEOF(UInt);

    aLogRec->setAnalyzeStartPtr(sAnlzPtr);

    return IDE_SUCCESS;
}

/***************************************************************
 * LOB Prepare for Write log
 * -----------------------------
 * | Offset |  Old    |  New   |
 * |        |  Size   |  Size  |
 * | (UInt) | (UInt)  | (UInt) |
 * -----------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeLobPrepare4Write( smiLogRec  *aLogRec,
                                           UInt       *aLobOffset,
                                           UInt       *aOldSize,
                                           UInt       *aNewSize )
{
    SChar    *sAnlzPtr = aLogRec->getAnalyzeStartPtr();
    UInt      sOffset = 0;

    /* Argument �ּ�
       aLogRec : ���� Log Record
       aLobOffset : LOB Piece�� ���� offset
       aOldSize : ������ LOB Piece�� Old Size
       aNewSize : ������ LOB Piece�� New Size
    */

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aLobOffset != NULL );
    IDE_DASSERT( aOldSize != NULL );
    IDE_DASSERT( aNewSize != NULL );

    *aLobOffset = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    *aOldSize = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    *aNewSize = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    return IDE_SUCCESS;
}

/***************************************************************
 * LOB Trim for Write log
 * ----------
 * | Offset |
 * |        |
 * | (UInt) |
 * ----------
 **************************************************************/
IDE_RC smiLogRec::analyzeLobTrim( smiLogRec   * aLogRec,
                                  UInt        * aLobOffset )
{
    SChar * sAnlzPtr    = aLogRec->getAnalyzeStartPtr();
    UInt    sOffset     = 0;
    ULong   sTemp       = 0;

    /* Argument �ּ�
       aLogRec : ���� Log Record
       aLobOffset : Trim offset
    */

    // Simple argument check code
    IDE_DASSERT( aLogRec     != NULL );
    IDE_DASSERT( aLobOffset  != NULL );

    /* BUG-39648 �α׿� 8����Ʈ�� ���� offset�� 4����Ʈ�� ĳ���� �Ѵ�. */
    sTemp = aLogRec->getULongValue(sAnlzPtr, sOffset);
    *aLobOffset = (UInt)sTemp;

    return IDE_SUCCESS;
}

/***************************************************************
 * Memory Partial Write Log
 * --------------------------------------
 * |        |        |   LOB Desc Area  |
 * | Before | After  |------------------|
 * | Image  | Image  |   LOB   |  LOB   |
 * |  Area  | Area   | Locator | Offset |
 * |        |        | (ULong) | (UInt) |
 * --------------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeLobPartialWriteMemory( iduMemAllocator * aAllocator,
                                                smiLogRec  *aLogRec,
                                                ULong      *aLobLocator,
                                                UInt       *aLobOffset,
                                                UInt       *aLobPieceLen,
                                                SChar     **aLobPiece )
{
    SChar      *sAnlzPtr = aLogRec->getAnalyzeStartPtr();
    UInt        sOffset = 0;
    SChar      *sAImgPtr;

    sOffset += aLogRec->getBfrImgSize();

    sAImgPtr = sAnlzPtr + sOffset;
    sOffset += aLogRec->getAftImgSize();

    *aLobPieceLen = aLogRec->getAftImgSize();
    *aLobLocator = aLogRec->getULongValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(ULong);

    *aLobOffset = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);


    IDU_FIT_POINT( "smiLogRec::analyzeLobPartialWriteMemory::malloc" );

    /* LOB Piece�� ���� �޸� �Ҵ�. ȣ���� �ʿ��� ����Ŀ� �� �޸𸮸�
       �ݵ�� free �����־�� �Ѵ�.
    */
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_RP_RPS,
                                *aLobPieceLen,
                                (void **)aLobPiece,
                                IDU_MEM_IMMEDIATE,
                                aAllocator )
             != IDE_SUCCESS );
    (void)idlOS::memcpy(*aLobPiece, sAImgPtr, *aLobPieceLen);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***************************************************************
 * Disk Partial Write Log
 * ----------------------------------------------
 * |  LOB  |  LOB   |   LOB   |        |        |
 * | Piece | Piece  | Locator | Offset | Amount |
 * |  Len  | Value  |         |        |        |
 * ----------------------------------------------
 **************************************************************/
IDE_RC smiLogRec::analyzeLobPartialWriteDisk( iduMemAllocator * aAllocator,
                                              smiLogRec  *aLogRec,
                                              ULong      *aLobLocator,
                                              UInt       *aLobOffset,
                                              UInt       *aAmount,
                                              UInt       *aLobPieceLen,
                                              SChar     **aLobPiece )
{
    SChar      *sAnlzPtr = aLogRec->getAnalyzeStartPtr();
    UInt        sOffset = 0;
    SChar      *sValue;

    sOffset += ID_SIZEOF(sdrLogHdr);

    *aLobPieceLen = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    sValue = sAnlzPtr + sOffset;
    sOffset += *aLobPieceLen;

    *aLobLocator = aLogRec->getULongValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(ULong);

    *aLobOffset = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    *aAmount = aLogRec->getUIntValue(sAnlzPtr, sOffset);
    sOffset += ID_SIZEOF(UInt);

    IDU_FIT_POINT( "smiLogRec::analyzeLobPartialWriteDisk::malloc" );

    /* LOB Piece�� ���� �޸� �Ҵ�. ȣ���� �ʿ��� ����Ŀ� �� �޸𸮸�
       �ݵ�� free �����־�� �Ѵ�.
    */
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_RP_RPS,
                                *aLobPieceLen,
                                (void **)aLobPiece,
                                IDU_MEM_IMMEDIATE,
                                aAllocator)
             != IDE_SUCCESS );
    (void)idlOS::memcpy(*aLobPiece, sValue, *aLobPieceLen);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC smiLogRec::analyzeTxSavePointSetLog( smiLogRec *aLogRec,
                                            UInt      *aSPNameLen,
                                            SChar     *aSPName)
{
    SChar * sLogPtr;

    sLogPtr = aLogRec->getLogPtr();

    sLogPtr += ID_SIZEOF(smiLogHdr);
    idlOS::memcpy( aSPNameLen, sLogPtr, ID_SIZEOF(UInt) );
    sLogPtr += ID_SIZEOF(UInt);

    idlOS::memcpy( aSPName,
                   sLogPtr,
                   *aSPNameLen );
    aSPName[*aSPNameLen] = '\0';

    return IDE_SUCCESS;
}


IDE_RC smiLogRec::analyzeTxSavePointAbortLog( smiLogRec *aLogRec,
                                              UInt      *aSPNameLen,
                                              SChar     *aSPName)
{
    SChar * sLogPtr;

    sLogPtr = aLogRec->getLogPtr();

    sLogPtr += ID_SIZEOF(smiLogHdr);
    idlOS::memcpy( aSPNameLen, sLogPtr, ID_SIZEOF(UInt) );
    sLogPtr += ID_SIZEOF(UInt);

    idlOS::memcpy( aSPName,
                   sLogPtr,
                   *aSPNameLen );
    aSPName[*aSPNameLen] = '\0';

    return IDE_SUCCESS;
}

idBool smiLogRec::needReplicationByType( void  * aLogHeadPtr,
                                         void  * aLogPtr,
                                         smLSN * aLSN )
{
    smrLogHead    *sCommonHdr;

    static SInt sMMDBLogSize = ID_SIZEOF(smrUpdateLog) - ID_SIZEOF(smiLogHdr);
    static SInt sLobLogSize  = ID_SIZEOF(smrLobLog) - ID_SIZEOF(smiLogHdr);
    // Table Meta Log Record 
    static SInt sTableMetaLogSize  = ID_SIZEOF(smrTableMetaLog) - ID_SIZEOF(smiLogHdr);

    IDE_DASSERT( aLogPtr != NULL );
    IDE_DASSERT( aLSN != NULL );
    
    ACP_UNUSED( aLSN );

    mLogPtr           = (SChar*)aLogPtr;
    sCommonHdr        = (smrLogHead*)aLogHeadPtr;
    mLogUnion.mCommon = sCommonHdr;
    mRecordLSN        = smrLogHeadI::getLSN( sCommonHdr );

#ifdef DEBUG
    if ( !SM_IS_LSN_INIT ( *aLSN ) )
    {
        IDE_DASSERT( smrCompareLSN::isEQ( &mRecordLSN, aLSN ) ); 
    }
#endif

    if ( needNormalReplicate() != ID_TRUE )
    {
        return ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }
    switch(smrLogHeadI::getType(sCommonHdr))
    {
        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_DSKTRANS_COMMIT:
            mLogType = SMI_LT_TRANS_COMMIT;
            break;

        case SMR_LT_MEMTRANS_GROUPCOMMIT:
            mLogType = SMI_LT_TRANS_GROUPCOMMIT;
            break;

        case SMR_LT_MEMTRANS_ABORT:
        case SMR_LT_DSKTRANS_ABORT:
            mLogType = SMI_LT_TRANS_ABORT;
            break;

        case SMR_LT_TRANS_PREABORT:
            mLogType = SMI_LT_TRANS_PREABORT;
            break;

        case SMR_LT_SAVEPOINT_SET:
            mLogType = SMI_LT_SAVEPOINT_SET;
            break;

        case SMR_LT_SAVEPOINT_ABORT:
            mLogType = SMI_LT_SAVEPOINT_ABORT;
            break;

        case SMR_LT_FILE_END:
            mLogType = SMI_LT_FILE_END;          // BUG-29837
            break;

        case SMR_LT_UPDATE:
            idlOS::memcpy( (SChar*)&(mLogUnion.mMemUpdate) + ID_SIZEOF(smiLogHdr),
                            mLogPtr + ID_SIZEOF(smiLogHdr),
                            sMMDBLogSize );

            mLogType = SMI_LT_MEMORY_CHANGE;

            switch(mLogUnion.mMemUpdate.mType)
            {
                case SMR_SMC_PERS_INSERT_ROW :
                    mChangeType = SMI_CHANGE_MRDB_INSERT;
                    break;

                case SMR_SMC_PERS_UPDATE_INPLACE_ROW :
                case SMR_SMC_PERS_UPDATE_VERSION_ROW :
                    mChangeType = SMI_CHANGE_MRDB_UPDATE;
                    break;

                case SMR_SMC_PERS_DELETE_VERSION_ROW :
                    mChangeType = SMI_CHANGE_MRDB_DELETE;
                    break;

                case SMR_SMC_PERS_WRITE_LOB_PIECE :
                    mChangeType = SMI_CHANGE_MRDB_LOB_PARTIAL_WRITE;
                    break;
                default:
                    mLogType = SMI_LT_NULL;
                    break;
            }

            break;

        case SMR_DLT_REDOONLY:
        case SMR_DLT_UNDOABLE:
            mLogType = SMI_LT_DISK_CHANGE;
            break;

        case SMR_LT_LOB_FOR_REPL:
            idlOS::memcpy( (SChar*)&(mLogUnion.mLob) + ID_SIZEOF(smiLogHdr),
                           mLogPtr + ID_SIZEOF(smiLogHdr),
                           sLobLogSize );
            mLogType = SMI_LT_LOB_FOR_REPL;

            switch(mLogUnion.mLob.mOpType)
            {
                case SMR_MEM_LOB_CURSOR_OPEN:
                {
                    mChangeType = SMI_CHANGE_MRDB_LOB_CURSOR_OPEN;
                    break;
                }
                case SMR_DISK_LOB_CURSOR_OPEN:
                {
                    mChangeType = SMI_CHANGE_DRDB_LOB_CURSOR_OPEN;
                    break;
                }
                case SMR_LOB_CURSOR_CLOSE:
                {
                    mChangeType = SMI_CHANGE_LOB_CURSOR_CLOSE;
                    break;
                }
                case SMR_PREPARE4WRITE:
                {
                    mChangeType = SMI_CHANGE_LOB_PREPARE4WRITE;
                    break;
                }
                case SMR_FINISH2WRITE:
                {
                    mChangeType = SMI_CHANGE_LOB_FINISH2WRITE;
                    break;
                }
                case SMR_LOB_TRIM:
                {
                    mChangeType = SMI_CHANGE_LOB_TRIM;
                    break;
                }
                default:
                {
                    mLogType = SMI_LT_NULL;
                    break;
                }
            }
            break;

        case SMR_LT_DDL :           // DDL Transaction���� ǥ���ϴ� Log Record
            mLogType = SMI_LT_DDL;
            break;

        case SMR_LT_TABLE_META :    // Table Meta Log Record
            idlOS::memcpy( (SChar*)&(mLogUnion.mTableMetaLog) + ID_SIZEOF(smiLogHdr),
                           mLogPtr + ID_SIZEOF(smiLogHdr),
                           sTableMetaLogSize );
            mLogType = SMI_LT_TABLE_META;
            break;

        case SMR_LT_DDL_QUERY_STRING :
            idlOS::memcpy( (SChar*)&(mLogUnion.mDDLStmtMeta),
                           mLogPtr + ID_SIZEOF(smiLogHdr),
                           ID_SIZEOF(smrDDLStmtMeta) );
            mLogType = SMI_LT_DDL_QUERY_STRING;
            break;

        default:
            mLogType = SMI_LT_NULL;
            break;
    }

    if ( (mLogType == SMI_LT_NULL) && (isBeginLog() != ID_TRUE ))
    {
        return ID_FALSE;
    }

    return ID_TRUE;
}

/*******************************************************************************
 * Description : Table Meta Log Record�� Body ũ�⸦ ��´�.
 ******************************************************************************/
UInt smiLogRec::getTblMetaLogBodySize()
{
    UInt sSize = getLogSize() - (SMR_LOGREC_SIZE(smrTableMetaLog)
                                 + ID_SIZEOF(smrLogTail));

    IDE_DASSERT( getLogSize() >= ( SMR_LOGREC_SIZE( smrTableMetaLog )
                                   + ID_SIZEOF( smrLogTail ) ) );

    return sSize;
};

/*******************************************************************************
 * Description : Table Meta Log Record�� Body�� ��´�.
 ******************************************************************************/
void * smiLogRec::getTblMetaLogBodyPtr()
{
    return (void *)(mLogPtr + SMR_LOGREC_SIZE(smrTableMetaLog));
}

/*******************************************************************************
 * Description : Table Meta Log Record�� Header�� ��´�.
 ******************************************************************************/
smiTableMeta * smiLogRec::getTblMeta()
{
    return (smiTableMeta *)&mLogUnion.mTableMetaLog.mTableMeta;
}

UInt smiLogRec::getDDLStmtMetaLogBodySize()
{
    UInt sSize = getLogSize() - ( ID_SIZEOF(smrLogHead) + ID_SIZEOF( smrDDLStmtMeta )
                                 + ID_SIZEOF(smrLogTail));

    IDE_DASSERT( getLogSize() > ( ID_SIZEOF(smrLogHead) + ID_SIZEOF( smrDDLStmtMeta )
                                  + ID_SIZEOF( smrLogTail ) ) );

    return sSize;
}

void * smiLogRec::getDDLStmtMetaLogBodyPtr()
{
    return (void *)( mLogPtr + ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrDDLStmtMeta) );
}

smiDDLStmtMeta * smiLogRec::getDDLStmtMeta()
{
    return (smiDDLStmtMeta *)&mLogUnion.mDDLStmtMeta;
}

/*******************************************************************************
 * Description : memory pool�� ���� ������ �Ҵ�ް� list�� �����Ų��.
 *           new node�� �����Ͽ� new node�� value�� pool�� ������ ������ �Ҵ��ϰ�,
 *           ���ڷ� ���� ����� �ּҴ� old node�ν� old node�� link�� new node�� �����Ѵ�.
 *           new node�� �ּҸ� ������.
 ******************************************************************************/
IDE_RC smiLogRec::chainedValueAlloc( iduMemAllocator  * aAllocator,
                                     smiLogRec        * aLogRec,
                                     smiChainedValue ** aChainedValue )
{
    smiChainedValue * sChainedValue ;
    smiChainedValue * sOldChainedValue = NULL;
    idBool            sChanedValueAllocFlag = ID_FALSE;

    IDE_ASSERT( *aChainedValue != NULL );

    // ù smiChainedValue���� �� ��ü�� new node�̴�.
    // �迭�̹Ƿ� �����Ҵ��� �ʿ����.
    if ( (*aChainedValue)->mAllocMethod == SMI_NON_ALLOCED )
    {
        sChainedValue = *aChainedValue;
    }
    else
    {
        IDU_FIT_POINT( "smiLogRec::chainedValueAlloc::valueFull:malloc" );

        // �� ����, ���ڷ� ���� smiChainedValue����� value�� full�����̴�.
        // smiChainedValue��带 ���� �����Ѵ�.
        IDE_TEST(iduMemMgr::malloc(IDU_MEM_RP_RPS,
                                   ID_SIZEOF(smiChainedValue),
                                   (void **)&sChainedValue,
                                   IDU_MEM_IMMEDIATE,
                                   aAllocator)
                 != IDE_SUCCESS );
        sChanedValueAllocFlag = ID_TRUE;

        // BUG-27329 CodeSonar::Uninitialized Variable (2)
        IDE_TEST( sChainedValue == NULL );

        // old node�� link�� ���� ��Ű��, new node�� �ʱ�ȭ�Ѵ�.
        sOldChainedValue = (*aChainedValue)->mLink;
        (*aChainedValue)->mLink = sChainedValue;
    }

    sChainedValue->mLink = NULL;
    sChainedValue->mColumn.length = 0;

    // smiChainedValue����� value�� pool�κ��� �����Ҵ��� �޴´�.
    IDE_TEST(aLogRec->mChainedValuePool->alloc((void **)&(sChainedValue->mColumn.value))
             != IDE_SUCCESS );
    sChainedValue->mAllocMethod = SMI_MEMPOOL_ALLOC;

    // new node�� �ּҷ� ��ü�Ѵ�.
    *aChainedValue = sChainedValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sChanedValueAllocFlag == ID_TRUE )
    {
        (*aChainedValue)->mLink = sOldChainedValue;
        (void)iduMemMgr::free(sChainedValue, aAllocator);
    }

    IDE_POP();

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *   - [IN]
 *   - [OUT]
 *
 **********************************************************************/
void * smiLogRec::getRPLogStartPtr4Undo(void             * aLogPtr,
                                        smiChangeLogType   aLogType )
{
    UChar  * sCurrLogPtr;
    UShort   sUptColCount;
    UChar    sColDescSetSize;
    UShort   sLoop;

    IDE_DASSERT(aLogPtr != NULL );
    /* TASK-5030 */
    IDE_DASSERT( ( aLogType == SMI_UNDO_DRDB_UPDATE )                     ||
                 ( aLogType == SMI_UNDO_DRDB_UPDATE_OVERWRITE )           ||
                 ( aLogType == SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN ) ||
                 ( aLogType == SMI_UNDO_DRDB_UPDATE_DELETE_ROW_PIECE )    ||
                 ( aLogType == SMI_UNDO_DRDB_DELETE ) );

    sCurrLogPtr = (UChar *)aLogPtr;

    sCurrLogPtr += ID_SIZEOF(UShort); // size(2) - undo info �� �� �տ� ��ġ
    sCurrLogPtr += SDC_UNDOREC_HDR_SIZE;

    sCurrLogPtr += ID_SIZEOF(scGRID);

    switch (aLogType)
    {
        case SMI_UNDO_DRDB_UPDATE :
            sCurrLogPtr += (1);     // flag(1)
            sCurrLogPtr += (2);     // size(2)

            // read update column count(2)
            SMI_LOGREC_READ_AND_MOVE_PTR( sCurrLogPtr,
                                          &sUptColCount,
                                          ID_SIZEOF(sUptColCount) );

            // read column desc set size(1)
            SMI_LOGREC_READ_AND_MOVE_PTR( sCurrLogPtr,
                                          &sColDescSetSize,
                                          ID_SIZEOF(sColDescSetSize) );

            // skip column desc set(1~128)
            sCurrLogPtr += sColDescSetSize;

            // read row header
            sCurrLogPtr += SDC_ROWHDR_SIZE;

            for( sLoop = 0; sLoop < sUptColCount; sLoop++ )
            {
                sCurrLogPtr = sdcRow::getNxtColPiece(sCurrLogPtr);
            }
            break;

        case SMI_UNDO_DRDB_UPDATE_OVERWRITE :

            sCurrLogPtr += (1);     // flag(1)
            sCurrLogPtr += (2);     // size(2)
            sCurrLogPtr += sdcRow::getRowPieceSize(sCurrLogPtr);
            break;

        case SMI_UNDO_DRDB_UPDATE_DELETE_FIRST_COLUMN :
            sCurrLogPtr += (1);     // flag(1)
            sCurrLogPtr += (2);     // size(2)
            sCurrLogPtr += SDC_ROWHDR_SIZE;
            sCurrLogPtr = sdcRow::getNxtColPiece(sCurrLogPtr);
            break;

        case SMI_UNDO_DRDB_UPDATE_DELETE_ROW_PIECE :
            sCurrLogPtr += (2);     // size(2)
            sCurrLogPtr += sdcRow::getRowPieceSize(sCurrLogPtr);
            break;

        /* TASK-5030 */
        case SMI_UNDO_DRDB_DELETE :
            sCurrLogPtr += (2);     // size(2)
            sCurrLogPtr += sdcRow::getRowPieceSize(sCurrLogPtr);
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    return (void *)sCurrLogPtr;
}


/***********************************************************************
 *
 * Description :
 *
 *   - [IN]
 *   - [OUT]
 *
 **********************************************************************/
void * smiLogRec::getRPLogStartPtr4Redo(void             * aLogPtr,
                                        smiChangeLogType   aLogType)
{
    UChar  * sCurrLogPtr;
    UShort   sUptColCount;
    UChar    sColDescSetSize;
    UShort   sLoop;

    IDE_DASSERT( aLogPtr != NULL );
    IDE_DASSERT( ( aLogType == SMI_REDO_DRDB_INSERT ) ||
                 ( aLogType == SMI_REDO_DRDB_UPDATE_INSERT_ROW_PIECE) ||
                 ( aLogType == SMI_REDO_DRDB_UPDATE) ||
                 ( aLogType == SMI_REDO_DRDB_UPDATE_OVERWRITE));

    sCurrLogPtr = (UChar *)aLogPtr;

    switch (aLogType)
    {

        case SMI_REDO_DRDB_INSERT :
        case SMI_REDO_DRDB_UPDATE_INSERT_ROW_PIECE :

            sCurrLogPtr += sdcRow::getRowPieceSize((UChar *)sCurrLogPtr);
            break;

        case SMI_REDO_DRDB_UPDATE :

            sCurrLogPtr += (1);     // flag(1)
            sCurrLogPtr += (2);     // size(2)

            // read update column count(2)
            SMI_LOGREC_READ_AND_MOVE_PTR(sCurrLogPtr,
                                         &sUptColCount,
                                         ID_SIZEOF(sUptColCount));

            // read column desc set size(1)
            SMI_LOGREC_READ_AND_MOVE_PTR(sCurrLogPtr,
                                         &sColDescSetSize,
                                         ID_SIZEOF(sColDescSetSize));

            // skip column desc set(1~128)
            sCurrLogPtr += sColDescSetSize;

            // read row header
            sCurrLogPtr += SDC_ROWHDR_SIZE;

            for( sLoop = 0; sLoop < sUptColCount; sLoop++ )
            {
                sCurrLogPtr = sdcRow::getNxtColPiece(sCurrLogPtr);
            }

            break;

        case SMI_REDO_DRDB_UPDATE_OVERWRITE :

            sCurrLogPtr += (1);     // flag(1)
            sCurrLogPtr += (2);     // size(2)
            sCurrLogPtr += sdcRow::getRowPieceSize(sCurrLogPtr);
            break;

        default:

            IDE_ASSERT(0);
            break;
    }

    return (void *)sCurrLogPtr;
}

/*******************************************************************************
 * Description : �ش� CID�� CIDArray�� �����ϴ� �� Ȯ���Ѵ�.
 ******************************************************************************/
idBool smiLogRec::isCIDInArray( UInt * aCIDArray, 
                                UInt   aCID, 
                                UInt aArraySize )
{
    idBool sIsCIDInArray = ID_FALSE;

    for ( ; aArraySize > 0 ; aArraySize-- )
    {
        if ( aCIDArray[aArraySize - 1] == aCID )
        {
            sIsCIDInArray = ID_TRUE;
            break;
        }
    }
    return sIsCIDInArray;
}

/**********************************************
 *     Log�� Head�� ����Ѵ�.
 *
 *     [IN] aLogHead - ����� Log�� Head
 *     [IN] aChkFlag  
 *     [IN] aModule - log module
 *     [IN] aLevel  - log level
 *     [USAGE]
 *     dumpLogHead(sLogHead, IDE_RP_0);
 **********************************************/
void smiLogRec::dumpLogHead( smiLogHdr * aLogHead, 
                             UInt aChkFlag, 
                             ideLogModule aModule, 
                             UInt aLevel )
{
    idBool sIsBeginLog = ID_FALSE;
    idBool sIsReplLog  = ID_FALSE;
    idBool sIsSvpLog   = ID_FALSE;
    smrLogHead * sLogHead = (smrLogHead*)aLogHead;

    if ( ( smrLogHeadI::getFlag( sLogHead ) & SMR_LOG_BEGINTRANS_MASK )
         == SMR_LOG_BEGINTRANS_OK )
    {
        sIsBeginLog = ID_TRUE;
    }
    else
    {
        sIsBeginLog = ID_FALSE;
    }
    
    if ( ( smrLogHeadI::getFlag( sLogHead ) & SMR_LOG_TYPE_MASK )
          == SMR_LOG_TYPE_NORMAL )
    {
        sIsReplLog = ID_TRUE;
    }
    else
    {
        sIsReplLog = ID_FALSE;
    }

    if ( ( smrLogHeadI::getFlag( sLogHead ) & SMR_LOG_SAVEPOINT_MASK )
          == SMR_LOG_SAVEPOINT_OK )
    {
        sIsSvpLog = ID_TRUE;
    }
    else
    {
        sIsSvpLog = ID_FALSE;
    }
    
    ideLog::log( aChkFlag, 
                 aModule, 
                 aLevel, 
                 "MAGIC: %"ID_UINT32_FMT", TID: %"ID_UINT32_FMT", "
                 "BE: %s, REP: %s, ISVP: %s, ISVP_DEPTH: %"ID_UINT32_FMT", "
                 "PLSN=<%"ID_UINT32_FMT", %"ID_UINT32_FMT">, "
                 "LT: < %"ID_UINT32_FMT" >, SZ: %"ID_UINT32_FMT" ",
                 smrLogHeadI::getMagic(sLogHead),
                 smrLogHeadI::getTransID(sLogHead),
                 (sIsBeginLog == ID_TRUE)?"Y":"N",
                 (sIsReplLog == ID_TRUE)?"Y":"N",
                 (sIsSvpLog == ID_TRUE)?"Y":"N",
                 smrLogHeadI::getReplStmtDepth(sLogHead),
                 smrLogHeadI::getPrevLSNFileNo(sLogHead),
                 smrLogHeadI::getPrevLSNOffset(sLogHead),
                 smrLogHeadI::getType(sLogHead),
                 smrLogHeadI::getSize(sLogHead));
}

IDE_RC smiLogRec::analyzeInsertLogDictionary( smiLogRec  *aLogRec,
                                              UShort     *aColCnt,
                                              UInt       *aCIDArray,
                                              smiValue   *aAColValueArray,
                                              idBool     *aDoWait )
{
    SChar *sAfterImagePtr;
    SChar *sAfterImagePtrFence;

    // Simple argument check code
    IDE_DASSERT( aLogRec != NULL );
    IDE_DASSERT( aColCnt != NULL );
    IDE_DASSERT( aCIDArray != NULL );
    IDE_DASSERT( aAColValueArray != NULL );
    IDE_DASSERT( aDoWait != NULL );

    // �޸� �α״� ������ �ϳ��� �α׷� ���̹Ƿ�, *aDoWait�� ID_FALSE�� �ȴ�.
    *aDoWait = ID_FALSE;

    sAfterImagePtr = aLogRec->getLogPtr() + SMR_LOGREC_SIZE(smrUpdateLog);

    /* TASK-4690, BUG-32319 [sm-mem-collection] The number of MMDB update log
     *                      can be reduced to 1.
     * �α� �������� OldVersion RowOID�� �ֱ� ������ Fence�� �̸� �������ش�. */
    sAfterImagePtrFence = aLogRec->getLogPtr()
                          + aLogRec->getLogSize()
                          - ID_SIZEOF(ULong)
                          - smiLogRec::getLogTailSize();

    IDE_TEST( analyzeInsertLogAfterImageDictionary( aLogRec,
                                                    sAfterImagePtr,
                                                    sAfterImagePtrFence,
                                                    aColCnt,
                                                    aCIDArray,
                                                    aAColValueArray )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiLogRec::analyzeInsertLogAfterImageDictionary( smiLogRec *aLogRec,
                                                        SChar     *aAfterImagePtr,
                                                        SChar     *aAfterImagePtrFence,
                                                        UShort    *aColCount,
                                                        UInt      *aCidArray,
                                                        smiValue  *aAfterColsArray )
{
    void                * sTableHandle;
    const smiColumn     * spCol                 = NULL;
    SChar               * sFixedAreaPtr;
    SChar               * sVarAreaPtr;
    SChar               * sVarColPtr;
    UShort                sFixedAreaSize;
    UInt                  sCID;
    UInt                  sAfterColSize;
    UInt                  i;
    UInt                  sOIDCnt;
    smVCDesc              sVCDesc;
    SChar                 sErrorBuffer[256];
    smOID                 sTableOID;
    smOID                 sVCPieceOID           = SM_NULL_OID;
    UShort                sCurrVarOffset        = 0;
    UShort                sNextVarOffset        = 0;

    sFixedAreaPtr = aAfterImagePtr + SMI_LOGREC_MV_FIXED_ROW_DATA_OFFSET;

    /* Fixed Row�� ����:UShort */
    sFixedAreaSize = aLogRec->getUShortValue( aAfterImagePtr, SMI_LOGREC_MV_FIXED_ROW_SIZE_OFFSET );
    IDE_TEST_RAISE( sFixedAreaSize > SM_PAGE_SIZE,
                    err_too_big_fixed_area_size );

    sVarAreaPtr   = aAfterImagePtr + SMI_LOGREC_MV_FIXED_ROW_DATA_OFFSET + sFixedAreaSize;

    sTableOID = aLogRec->getTableOID();
    sTableHandle = (void *)smiGetTable( sTableOID );

    *aColCount = 1;
    /* extract fixed fields */
    for ( i = 0 ; i < *aColCount ; i++ )
    {
        IDE_TEST( smiGetTableColumns( sTableHandle,
                                      i,
                                      &spCol )
                  != IDE_SUCCESS );

        sCID = spCol->id & SMI_COLUMN_ID_MASK;
        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);
        if ( (spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED )
        {
            aCidArray[sCID] = sCID;
            aAfterColsArray[sCID].length = spCol->size;
            aAfterColsArray[sCID].value  =
                    sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY));
        }
        else /* large var + lob */
        {
            if ( (spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_VARIABLE ) 
            {
                /* United var �� fixed ������ ���� ���� ���� */
                continue;
            }
            else
            {
                smiLogRec::getVCDescInAftImg(spCol, aAfterImagePtr, &sVCDesc);
                if ((sVCDesc.flag & SM_VCDESC_MODE_MASK) == SM_VCDESC_MODE_IN)
                {
                    aCidArray[sCID] = sCID;
                    if ( ( SMI_IS_LOB_COLUMN(spCol->flag) == ID_TRUE ) && ( sVCDesc.length != 0 ) )
                    {
                        aAfterColsArray[sCID].length = sVCDesc.length + SMI_LOB_DUMMY_HEADER_LEN;
                        aAfterColsArray[sCID].value  =
                            sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY))
                            + ID_SIZEOF( smVCDescInMode ) - SMI_LOB_DUMMY_HEADER_LEN;
                    }
                    else
                    {
                        aAfterColsArray[sCID].length = sVCDesc.length;
                        aAfterColsArray[sCID].value  =
                            sFixedAreaPtr + (spCol->offset - smiGetRowHeaderSize(SMI_TABLE_MEMORY))
                            + ID_SIZEOF( smVCDescInMode );
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }

    /* extract variabel fields */
    sVCPieceOID = aLogRec->getvULongValue( sVarAreaPtr );
    sVarAreaPtr += ID_SIZEOF(smOID);

    if ( sVCPieceOID != SM_NULL_OID )
    {
        /* sVarColCount = aLogRec->getUShortValue (sVarAreaPtr );           �׻� 1 */
        sVarAreaPtr += ID_SIZEOF(UShort);

        sCID = aLogRec->getUIntValue( sVarAreaPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET ) & SMI_COLUMN_ID_MASK ;
        sVarAreaPtr += ID_SIZEOF(UInt);

        /* sVCPieceOID  = aLogRec->getvULongValue( sVarAreaPtr );            �׻� null oid */
        sVarAreaPtr += ID_SIZEOF(smOID);

        /* sVarColCountInPiece  = aLogRec->getUShortValue ( sVarAreaPtr );  �׻� 1 */
        sVarAreaPtr += ID_SIZEOF(UShort);

        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        aCidArray[sCID] = sCID;

        /* value �� �պκп� �ִ� offset array���� offset�� �о�´� */
        sCurrVarOffset = aLogRec->getUShortValue( sVarAreaPtr, ID_SIZEOF(UShort) * 0 ); 
        sNextVarOffset = aLogRec->getUShortValue( sVarAreaPtr, ID_SIZEOF(UShort) * 1 ); 

        aAfterColsArray[sCID].length = sNextVarOffset - sCurrVarOffset;

        if ( sNextVarOffset == sCurrVarOffset )
        {
            aAfterColsArray[sCID].value = NULL;
        }
        else
        {
            aAfterColsArray[sCID].value  = sVarAreaPtr + sCurrVarOffset - ID_SIZEOF(smVCPieceHeader);
        }

        sVarAreaPtr += sNextVarOffset - ID_SIZEOF(smVCPieceHeader);
    }
    else
    {
        /* Nothing to do */
    }

    /* extract Large variable fields & LOB fields */
    for (sVarColPtr = sVarAreaPtr; sVarColPtr < aAfterImagePtrFence; )
    {
        sCID = aLogRec->getUIntValue( sVarColPtr, SMI_LOGREC_MV_COLUMN_CID_OFFSET )
               & SMI_COLUMN_ID_MASK;

        IDE_TEST_RAISE(sCID > SMI_COLUMN_ID_MAXIMUM, ERR_TOO_LARGE_CID);

        IDE_TEST( smiGetTableColumns( sTableHandle,
                                      sCID,
                                      &spCol )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( (spCol->flag & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_FIXED,
                        err_fixed_col_in_var_area );

        aCidArray[sCID] = sCID;

        sAfterColSize = aLogRec->getUIntValue(sVarColPtr, SMI_LOGREC_MV_COLUMN_SIZE_OFFSET);
        if ( ( SMI_IS_LOB_COLUMN(spCol->flag) == ID_TRUE ) && ( sAfterColSize != 0 ) )
        {
            aAfterColsArray[sCID].length = sAfterColSize + SMI_LOB_DUMMY_HEADER_LEN;
            aAfterColsArray[sCID].value  = sVarColPtr + SMI_LOGREC_MV_COLUMN_DATA_OFFSET - SMI_LOB_DUMMY_HEADER_LEN;
        }
        else
        {
            aAfterColsArray[sCID].length = sAfterColSize;
            aAfterColsArray[sCID].value  = sVarColPtr + SMI_LOGREC_MV_COLUMN_DATA_OFFSET;
        }
        sVarColPtr += SMI_LOGREC_MV_COLUMN_DATA_OFFSET + sAfterColSize;

        /* Variable/LOB Column Value �� ��쿡�� OID List�� �ǳʶپ�� �� */
        /* ���⿡�� OID count�� ����Ǿ� �����Ƿ�, Count�� ���� �� �� �� ��ŭ */
        /* �ǳʶٵ��� �Ѵ�. */
        sOIDCnt = aLogRec->getUIntValue(sVarColPtr, 0);
        sVarColPtr += ID_SIZEOF(UInt) + (sOIDCnt * ID_SIZEOF(smOID));
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_fixed_col_in_var_area);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageDictionary] "
                         "Fixed Column [CID:%"ID_UINT32_FMT"] is in Variable Area at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(ERR_TOO_LARGE_CID);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageDictionary] Too Large Column ID [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sCID, 
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION(err_too_big_fixed_area_size);
    {
        idlOS::snprintf( sErrorBuffer, 
                         256,
                         "[smiLogRec::analyzeInsertLogAfterImageDictionary] "
                         "Fixed Area Size [%"ID_UINT32_FMT"] is over page size [%"ID_UINT32_FMT"] at "
                         "[LSN: %"ID_UINT32_FMT",%"ID_UINT32_FMT")]",
                         sFixedAreaSize, 
                         SM_PAGE_SIZE,
                         aLogRec->getRecordLSN().mFileNo, 
                         aLogRec->getRecordLSN().mOffset );
        IDE_SET( ideSetErrorCode(smERR_ABORT_INTERNAL_ARG, sErrorBuffer) );
        IDE_ERRLOG(IDE_RP_0);

        IDE_CALLBACK_FATAL("[Repl] Check Error.");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/*  not used
smiLogType smiLogRec::getLogTypeFromLogHdr( smiLogHdr    * aLogHead )
{
    smiLogType      sLogType = SMI_LT_NULL;

    switch ( smrLogHeadI::getType( aLogHead ) )
    {
        case SMR_LT_MEMTRANS_COMMIT:
        case SMR_LT_DSKTRANS_COMMIT:
            sLogType = SMI_LT_TRANS_COMMIT;
            break;

        case SMR_LT_MEMTRANS_GROUPCOMMIT:
            sLogType = SMI_LT_TRANS_GROUPCOMMIT;
            break;

        case SMR_LT_MEMTRANS_ABORT:
        case SMR_LT_DSKTRANS_ABORT:
            sLogType = SMI_LT_TRANS_ABORT;
            break;

        case SMR_LT_TRANS_PREABORT:
            sLogType = SMI_LT_TRANS_PREABORT;
            break;

        case SMR_LT_SAVEPOINT_SET:
            sLogType = SMI_LT_SAVEPOINT_SET;
            break;

        case SMR_LT_SAVEPOINT_ABORT:
            sLogType = SMI_LT_SAVEPOINT_ABORT;
            break;

        case SMR_LT_FILE_END:
            sLogType = SMI_LT_FILE_END;
            break;

        case SMR_LT_UPDATE:
            sLogType = SMI_LT_MEMORY_CHANGE;
            break;

        case SMR_DLT_REDOONLY:
        case SMR_DLT_UNDOABLE:
            sLogType = SMI_LT_DISK_CHANGE;
            break;

        case SMR_LT_LOB_FOR_REPL:
            sLogType = SMI_LT_LOB_FOR_REPL;
            break;

        case SMR_LT_DDL :           // DDL Transaction���� ǥ���ϴ� Log Record
            sLogType = SMI_LT_DDL;
            break;

        case SMR_LT_TABLE_META :    // Table Meta Log Record
            sLogType = SMI_LT_TABLE_META;
            break;

        case SMR_LT_DDL_QUERY_STRING :
            sLogType = SMI_LT_DDL_QUERY_STRING;
            break;

        default:
            sLogType = SMI_LT_NULL;
            break;
    }

    return sLogType;
}
*/

