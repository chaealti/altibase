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
 
#include <ide.h>
#include <smErrorCode.h>
#include <svcDef.h>
#include <svcReq.h>
#include <svcRecord.h>
#include <svcRecordUndo.h>
#include <svrRecoveryMgr.h>

/* �Ʒ� svc log���� svrLog Ÿ���� ��ӹ޾ƾ� �Ѵ�.
   ��, svrLogType�� �� ù ����� �ݵ�� ������ �Ѵ�. */
typedef struct svcInsertLog
{
    svrUndoFunc mUndo;
    void      * mTransPtr;
    smOID       mTableOID;
    scSpaceID   mSpaceID;
    SChar     * mFixedRowPtr;
} svcInsertLog;

typedef struct svcUpdateLog
{
    svrUndoFunc mUndo;
    void      * mTransPtr;
    smOID       mTableOID;
    scSpaceID   mSpaceID;
    SChar     * mOldRowPtr;
    SChar     * mNewRowPtr;
    ULong       mNextVersion;
} svcUpdateLog;

typedef struct svcUpdateInpLog
{
    svrUndoFunc mUndo;
    void      * mTransPtr;
    scSpaceID   mSpaceID;
    smOID       mTableOID;
    smOID       mVarOID;
    SChar     * mFixedRowPtr;
    ULong       mModifyIdxBit;
} svcUpdateInpLog;

/* �� �α״� svcUpdateInpLog�� sublog�̴�.
   ���� svrUndoFunc�� �ʿ����.
   ������ svrLog Ÿ���� ����ϱ� ������
   dummy�� �д�. */
typedef struct svcUptInpColLog
{
    svrUndoFunc mDummy;
    UChar       mColType;
    UChar       mColMode;
    UShort      mColOffset;
    UInt        mValSize;

    /* PROJ-2174 Supporting LOB in the volatile tablespace
     * LOB�� ���� mPieceCount, mFirstLPCH�� �߰� */
    UInt        mLPCHCount;
    smcLPCH   * mFirstLPCH;

    SChar       mValue[SM_PAGE_SIZE];
} svcUptInpColLog;

typedef struct svcDeleteLog
{
    svrUndoFunc mUndo;
    void      * mTransPtr;
    smOID       mTableOID;
    SChar     * mRowPtr;
    ULong       mNextVersion;
} svcDeleteLog;

static IDE_RC undoInsert       (svrLogEnv *aEnv, svrLog *aInsertLog, svrLSN aSubLSN);
static IDE_RC undoUpdate       (svrLogEnv *aEnv, svrLog *aUpdateLog, svrLSN aSubLSN);
static IDE_RC undoUpdateInplace(svrLogEnv *aEnv, svrLog *aUptInpLog, svrLSN aSubLSN);
static IDE_RC undoDelete       (svrLogEnv *aEnv, svrLog *aDeleteLog, svrLSN aSubLSN);

/******************************************************************************
 * Description:
 *    volatile TBS�� �߻��� insert ���꿡 ���� �α��Ѵ�.
 ******************************************************************************/
IDE_RC svcRecordUndo::logInsert(svrLogEnv  *aLogEnv,
                                void       *aTransPtr,
                                smOID       aTableOID,
                                scSpaceID   aSpaceID,
                                SChar      *aFixedRow)
{
    svcInsertLog sInsertLog;

    sInsertLog.mUndo        = undoInsert;
    sInsertLog.mTransPtr    = aTransPtr;
    sInsertLog.mTableOID    = aTableOID;
    sInsertLog.mSpaceID     = aSpaceID;
    sInsertLog.mFixedRowPtr = aFixedRow;

    IDE_TEST(svrLogMgr::writeLog(aLogEnv,
                                 (svrLog*)&sInsertLog,
                                 ID_SIZEOF(svcInsertLog))
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *    insert log�� �м��� undo�� �����Ѵ�.
 ******************************************************************************/
IDE_RC undoInsert(svrLogEnv * /*aEnv*/,
                  svrLog    * aInsertLog,
                  svrLSN     /*aSubLSN*/)
{
    svcInsertLog *sInsertLog = (svcInsertLog*)aInsertLog;

    IDE_TEST( smLayerCallback::undoInsertOfTableInfo( sInsertLog->mTransPtr,
                                                      sInsertLog->mTableOID )
              != IDE_SUCCESS );
 
    IDE_TEST(svcRecord::setDeleteBit(
               sInsertLog->mSpaceID,
               sInsertLog->mFixedRowPtr)
             != IDE_SUCCESS);
  
    return IDE_SUCCESS; 
 
    IDE_EXCEPTION_END;
 
    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *    volatile TBS�� �߻��� update ���꿡 ���� �α��Ѵ�.
 ******************************************************************************/
IDE_RC svcRecordUndo::logUpdate(svrLogEnv  *aLogEnv,
                                void       *aTransPtr,
                                smOID       aTableOID,
                                scSpaceID   aSpaceID,
                                SChar      *aOldFixedRow,
                                SChar      *aNewFixedRow,
                                ULong       aBeforeNext)
{
    svcUpdateLog sUpdateLog;

    sUpdateLog.mUndo        = undoUpdate;
    sUpdateLog.mTransPtr    = aTransPtr;
    sUpdateLog.mTableOID    = aTableOID;
    sUpdateLog.mSpaceID     = aSpaceID;
    sUpdateLog.mOldRowPtr   = aOldFixedRow;
    sUpdateLog.mNewRowPtr   = aNewFixedRow;
    sUpdateLog.mNextVersion = aBeforeNext;

    IDE_TEST(svrLogMgr::writeLog(aLogEnv,
                                 (svrLog*)&sUpdateLog,
                                 ID_SIZEOF(svcUpdateLog))
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *    update log�� ���� undo�� �����Ѵ�.
 *    - new fixed row�� ���� undo ����
 *    - old fixed row�� ���� undo ����
 *    - variable row���� physical log�� ó���ȴ�.
 ******************************************************************************/
IDE_RC undoUpdate(svrLogEnv  * /*aEnv*/,
                  svrLog     * aUpdateLog,
                  svrLSN     /*aSubLSN*/)
{
    smpSlotHeader *sSlotHeader;
    svcUpdateLog  *sUpdateLog = (svcUpdateLog*)aUpdateLog;

    IDE_TEST(svcRecord::setDeleteBit(
               sUpdateLog->mSpaceID,
               sUpdateLog->mNewRowPtr)
             != IDE_SUCCESS);

    /* old row�� ���� undo */
    sSlotHeader = (smpSlotHeader*)sUpdateLog->mOldRowPtr;

    SM_SET_SCN_FREE_ROW( &(sSlotHeader->mLimitSCN) );
    SMP_SLOT_INIT_POSITION( sSlotHeader );
    SMP_SLOT_SET_USED( sSlotHeader );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *     update inplace log�� ����Ѵ�.
 *
 * Implementation:
 *     ũ�� �ΰ��� �α׸� ����Ѵ�.
 *     svcUpdateInpLog�� svcUptInpColLog�̴�.
 *     ���� update inplace�� �⺻ �������� svcUpdateInpLog�� ���� ����ϰ�
 *     update�Ǵ� �÷��� ������ŭ svcUptIntColLog�� ����Ѵ�.
 *     �÷��� fixed�� variable in-mode�� variavle out-mode�Ŀ� ����
 *     svcUptInpColLog�� �����ϴ� ����� �ٸ���.
 ******************************************************************************/
IDE_RC svcRecordUndo::logUpdateInplace(svrLogEnv           * aEnv,
                                       void                * aTransPtr,
                                       scSpaceID             aSpaceID,
                                       smOID                 aTableOID,
                                       SChar               * aFixedRowPtr,
                                       const smiColumnList * aColumnList,
                                       ULong                 aModifyIdxBit )
{
    svcUpdateInpLog         sUptLog;
    svcUptInpColLog         sColLog;
    const smiColumnList   * sCurColumnList;
    const smiColumn       * sCurColumn;
    smVCDesc              * sVCDesc;
    smcLobDesc            * sLobDesc;

    /* sColLog.mValue�� ���̸� ��Ÿ����.
       mValueSize�� �׻� ���� �ʴ�. */
    SInt                    sWrittenSize;

    sUptLog.mUndo           = undoUpdateInplace;
    sUptLog.mTransPtr       = aTransPtr;
    sUptLog.mSpaceID        = aSpaceID;
    sUptLog.mTableOID       = aTableOID;
    sUptLog.mVarOID         = ((smpSlotHeader*)aFixedRowPtr)->mVarOID;
    sUptLog.mFixedRowPtr    = aFixedRowPtr;
    sUptLog.mModifyIdxBit   = aModifyIdxBit;

    /* svcUpdateInpLog �α׸� ����Ѵ�. */
    IDE_TEST(svrLogMgr::writeLog(aEnv,
                                 (svrLog*)&sUptLog,
                                 ID_SIZEOF(svcUpdateInpLog))
             != IDE_SUCCESS);

    /* update�� �÷����� ����Ʈ�� ��ȸ�ϸ鼭
       svcUptInpColLog �α׸� ����Ѵ�. */
    for (sCurColumnList = aColumnList;
         sCurColumnList != NULL;
         sCurColumnList = sCurColumnList->next)
    {
        sCurColumn = sCurColumnList->column;

        switch (sCurColumn->flag & SMI_COLUMN_TYPE_MASK)
        {
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SMI_COLUMN_TYPE_LOB:
                sColLog.mDummy = NULL; /* undo �Լ��� �ʿ����. */
                sColLog.mColType = SVC_COLUMN_TYPE_LOB;

                sVCDesc  = (smVCDesc *)(aFixedRowPtr + sCurColumn->offset);
                sLobDesc = (smcLobDesc *)(aFixedRowPtr + sCurColumn->offset);
                if (svcRecord::getVCStoreMode(sCurColumn, 
                                              sVCDesc->length)
                    == SM_VCDESC_MODE_OUT)
                {
                    sColLog.mColMode = SVC_COLUMN_MODE_OUT;
                    idlOS::memcpy(sColLog.mValue,
                                  &sVCDesc->fstPieceOID,
                                  ID_SIZEOF(smOID));
                    sWrittenSize = ID_SIZEOF(smOID);
                    
                }
                else
                {
                    sColLog.mColMode = SVC_COLUMN_MODE_IN;
                    idlOS::memcpy(sColLog.mValue,
                                  aFixedRowPtr + sCurColumn->offset
                                               + ID_SIZEOF(smVCDescInMode),
                                  sVCDesc->length);
                    sWrittenSize = sVCDesc->length;
                }
                sColLog.mValSize = sVCDesc->length;
                /* variable column���� ������ */
                sColLog.mLPCHCount = sLobDesc->mLPCHCount;
                sColLog.mFirstLPCH = sLobDesc->mFirstLPCH;

                break;
            case SMI_COLUMN_TYPE_VARIABLE:
                sColLog.mDummy = NULL; /* undo �Լ��� �ʿ����. */
                sColLog.mColType = SVC_COLUMN_TYPE_VARIABLE;

                /* variable column�� ���, in-mode, out-mode�� �Ǵ��ؾ� �Ѵ�. */
                sVCDesc = (smVCDesc*)(aFixedRowPtr + sCurColumn->offset);
                if (svcRecord::getVCStoreMode(sCurColumn,
                                              sVCDesc->length)
                    == SM_VCDESC_MODE_OUT)
                {
                    sColLog.mColMode = SVC_COLUMN_MODE_OUT;
                    idlOS::memcpy(sColLog.mValue,
                                  &sVCDesc->fstPieceOID,
                                  ID_SIZEOF(smOID));
                    sWrittenSize = ID_SIZEOF(smOID);
                }
                else
                {
                    sColLog.mColMode = SVC_COLUMN_MODE_IN;
                    idlOS::memcpy(sColLog.mValue,
                                  aFixedRowPtr + sCurColumn->offset
                                               + ID_SIZEOF(smVCDescInMode),
                                  sVCDesc->length);
                    sWrittenSize = sVCDesc->length;
                }
                sColLog.mValSize = sVCDesc->length;

                break;
            case SMI_COLUMN_TYPE_FIXED:
                /* fixed column�� ��� */
                sColLog.mColType = SVC_COLUMN_TYPE_FIXED;
                sColLog.mColMode = SVC_COLUMN_MODE_NA;
                sColLog.mValSize = sCurColumn->size;
                idlOS::memcpy(sColLog.mValue,
                              aFixedRowPtr + sCurColumn->offset,
                              sCurColumn->size);
                sWrittenSize = sCurColumn->size;

                break;
            case SMI_COLUMN_TYPE_VARIABLE_LARGE:
            default:
                IDE_ASSERT(0);
                break;
        }

        sColLog.mColOffset = sCurColumn->offset;

        IDE_TEST(svrLogMgr::writeSubLog(aEnv,
                                        (svrLog*)&sColLog,
                                        offsetof(svcUptInpColLog, mValue) +
                                        sWrittenSize)
                 != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *    update inplace log�� ���� undo�� �����Ѵ�.
 ******************************************************************************/
IDE_RC undoUpdateInplace(svrLogEnv * aEnv,
                         svrLog    * aUptInpLog,
                         svrLSN      aSubLSN)
{
    svrLSN              sDummyLSN;
    svcUpdateInpLog   * sUptInpLog = (svcUpdateInpLog *)aUptInpLog;
    svcUptInpColLog   * sColLog;
    smVCDescInMode    * sVCDescInMode;
    smVCDesc          * sVCDesc;
    smcLobDesc        * sLobDesc;
    smOID            sFixedRowOID;

    sFixedRowOID = SMP_SLOT_GET_OID( sUptInpLog->mFixedRowPtr );

    if ( sUptInpLog->mModifyIdxBit != 0 )
    {
        IDE_TEST( smLayerCallback::deleteRowFromTBIdx( sUptInpLog->mSpaceID,
                                                       sUptInpLog->mTableOID,
                                                       sFixedRowOID,
                                                       sUptInpLog->mModifyIdxBit )
                  != IDE_SUCCESS );
    }

    ((smpSlotHeader*)sUptInpLog->mFixedRowPtr)->mVarOID = sUptInpLog->mVarOID;

    while (aSubLSN != SVR_LSN_BEFORE_FIRST)
    {
        IDE_TEST(svrLogMgr::readLog(aEnv, 
                                    aSubLSN, 
                                    (svrLog**)&sColLog,
                                    &sDummyLSN, 
                                    &aSubLSN)
                 != IDE_SUCCESS);

        switch (sColLog->mColType)
        {
            case SVC_COLUMN_TYPE_FIXED:
                IDE_ASSERT(sColLog->mValSize > 0);

                idlOS::memcpy(sUptInpLog->mFixedRowPtr + sColLog->mColOffset,
                              sColLog->mValue,
                              sColLog->mValSize);
                break;

            case SVC_COLUMN_TYPE_VARIABLE:
                sVCDesc = (smVCDesc *)(sUptInpLog->mFixedRowPtr +
                                      sColLog->mColOffset);

                sVCDesc->length = sColLog->mValSize;

                if (sColLog->mColMode == SVC_COLUMN_MODE_IN)
                {
                    sVCDesc->flag = SM_VCDESC_MODE_IN;
                    if (sColLog->mValSize > 0)
                    {
                        sVCDescInMode = (smVCDescInMode*)sVCDesc;
                        idlOS::memcpy(sVCDescInMode + 1,
                                      sColLog->mValue,
                                      sColLog->mValSize);
                    }
                }
                else
                {
                    sVCDesc->flag = SM_VCDESC_MODE_OUT;
                    if (sColLog->mValSize > 0)
                    {
                        idlOS::memcpy(&(sVCDesc->fstPieceOID),
                                      sColLog->mValue,
                                      ID_SIZEOF(smOID));
                    }
                }
                break;
            
            /* PROJ-2174 Supporting LOB in the volatile tablespace */
            case SVC_COLUMN_TYPE_LOB:
                sVCDesc = (smVCDesc *)(sUptInpLog->mFixedRowPtr +
                                      sColLog->mColOffset);

                sVCDesc->length = sColLog->mValSize;

                if (sColLog->mColMode == SVC_COLUMN_MODE_IN)
                {
                    sVCDesc->flag = SM_VCDESC_MODE_IN;
                    if (sColLog->mValSize > 0)
                    {
                        sVCDescInMode = (smVCDescInMode *)sVCDesc;
                        idlOS::memcpy(sVCDescInMode + 1,
                                      sColLog->mValue,
                                      sColLog->mValSize);
                    }
                    else
                    {
                        /* do nothing */
                    }
                }
                else
                {
                    sVCDesc->flag = SM_VCDESC_MODE_OUT;
                    if (sColLog->mValSize > 0)
                    {
                        idlOS::memcpy(&(sVCDesc->fstPieceOID),
                                      sColLog->mValue,
                                      ID_SIZEOF(smOID));
                    }
                    else
                    {
                        /* do nothing */
                    }
                }

                sLobDesc = (smcLobDesc *)(sUptInpLog->mFixedRowPtr + 
                                          sColLog->mColOffset);

                /* variable column���� ������ */
                sLobDesc->mLPCHCount = sColLog->mLPCHCount;
                sLobDesc->mFirstLPCH = sColLog->mFirstLPCH;

                break;

            default:
                IDE_ASSERT(0);
                break;
        }
    }

    if ( sUptInpLog->mModifyIdxBit != 0 )
    {
        IDE_TEST( smLayerCallback::insertRow2TBIdx( sUptInpLog->mTransPtr,
                                                    sUptInpLog->mSpaceID,
                                                    sUptInpLog->mTableOID,
                                                    sFixedRowOID,
                                                    sUptInpLog->mModifyIdxBit )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *     delete log�� ����Ѵ�.
 ******************************************************************************/
IDE_RC svcRecordUndo::logDelete(svrLogEnv * aLogEnv,
                                void      * aTransPtr,
                                smOID       aTableOID,
                                SChar     * aRowPtr,
                                ULong       aNextVersion)
{
    svcDeleteLog    sDeleteLog;

    sDeleteLog.mUndo        = undoDelete;
    sDeleteLog.mTransPtr    = aTransPtr;
    sDeleteLog.mTableOID    = aTableOID;
    sDeleteLog.mRowPtr      = aRowPtr;

    sDeleteLog.mNextVersion = aNextVersion;

    IDE_TEST(svrLogMgr::writeLog(aLogEnv,
                                 (svrLog*)&sDeleteLog,
                                 ID_SIZEOF(svcDeleteLog))
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description:
 *    delete log�� ���� undo�� �����Ѵ�.
 ******************************************************************************/
IDE_RC undoDelete(svrLogEnv * /*aEnv*/,
                  svrLog    * aDeleteLog,
                  svrLSN     /*aSubLSN*/)
{
    svcDeleteLog  * sDeleteLog = (svcDeleteLog *)aDeleteLog;
    ULong           sHeaderBeforNext;
    smpSlotHeader * sSlotHeader;

    IDE_TEST( smLayerCallback::undoDeleteOfTableInfo( sDeleteLog->mTransPtr,
                                                      sDeleteLog->mTableOID )
              != IDE_SUCCESS );

    idlOS::memcpy(&sHeaderBeforNext, &(sDeleteLog->mNextVersion), ID_SIZEOF(ULong));

    sSlotHeader = (smpSlotHeader*)sDeleteLog->mRowPtr;

    SM_SET_SCN( &(sSlotHeader->mLimitSCN), &sHeaderBeforNext);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

