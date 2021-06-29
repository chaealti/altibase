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
 * $Id: smcDef.h 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#ifndef _O_SMC_DEF_H_
#define _O_SMC_DEF_H_ 1

#include <smDef.h>
#include <smmDef.h>
#include <smpDef.h>
#include <sdpDef.h>
#include <sdcDef.h>

/* Memory Table�� State���� ������ ����ϴ� Index */
typedef enum
{
    SMC_INC_UNIQUE_VIOLATION_CNT, /* Unique Index Violation */
    SMC_INC_RETRY_CNT_OF_LOCKROW, /* Lock   Row Retry */
    SMC_INC_RETRY_CNT_OF_UPDATE,  /* Update Row Retry */
    SMC_INC_RETRY_CNT_OF_DELETE   /* Delete Row Retry */
} smcTblStatType;

struct smcTableHeader;

#define SMC_DROP_INDEX_LIST_POOL_SIZE  (128)
#define SMC_RESERV_SIZE (2)

// PROJ-1362 QP - Large Record & Internal LOB ����.
// ���� �ε��� ������  17������ 64���� Ȯ���ϱ� ���Ͽ�
// smcTableHeader�� mIndex�� array�� ó���Ѵ�.
#define SMC_MAX_INDEX_OID_CNT          (4)

#define SMC_CAT_TABLE ((smcTableHeader *)(smmManager::m_catTableHeader))
#define SMC_CAT_TEMPTABLE ((smcTableHeader *)(smmManager::m_catTempTableHeader))

typedef smiObjectType smcObjectType;

/* ----------------------------------------------------------------------------
 *    validate table
 * --------------------------------------------------------------------------*/
#define SMC_LOCK_MASK     (0x00000010)
#define SMC_LOCK_TABLE    (0x00000000)
#define SMC_LOCK_MUTEX    (0x00000010)

/* PROJ-2399 row template
 * column�� ���̸� �̸� �� �� ���� ������Ÿ���ΰ��(ex varchar,nibble��)
 * �ش� column�� ���̸� ID_USHORT_MAX�� �����ϱ� ���� ��ũ���̴�. */
#define SMC_UNDEFINED_COLUMN_LEN (ID_USHORT_MAX)

/* ----------------------------------------------------------------------------
 *    Sequence
 * --------------------------------------------------------------------------*/
#define SMC_INIT_SEQUENCE (ID_LONG(0x8000000000000000))

/***********************************************************************
 * Description : PROJ-2419. MVCC�� DML(Insert, Delete, Update) �α׿��� United Variable Column������
 *               Log�� Log Size�� ���Ѵ�. United Variable Log�� ������ ���� �����ȴ�.
 *
 *  �ϳ��� ������ united variable Column�� ���ؼ� 
 *  Var Log : Column Count(UInt), Column ID(1 ... n ) | LENGTH(UInt) | Fst Piece OID, Value 
 *
 *  aLength - [IN] Variable Column ����
 *  aCount  - [IN] united variable column �� �� column�� ����
 *  
 * < �α� ���� >
 *  First Piece OID ( smOID )
 * + Column Count   ( UShort )
 * + Column ID List ( SizeOF(Uint)*aColCount )
 * + pieceCount * (  Next Piece OID ( smOID )
 *                 + Column Count each Piece  ( UShort ))
 * + aLength : Value of All Piece
 ***********************************************************************/
#define SMC_UNITED_VC_LOG_SIZE(aLength, aColCount, aPieceCount)   ( ID_SIZEOF(smOID) \
                                                                  + ID_SIZEOF(UShort) \
                                                                  + (ID_SIZEOF(UInt) * aColCount) \
                                                                  + aPieceCount * ( ID_SIZEOF(smOID) + ID_SIZEOF(UShort)) \
                                                                  + aLength )

/* BUG-43604 ���� Variable Column���� UnitedVar ���·�  ��ĥ��
 * �� Column ������ Padding ���� �����Ѵ�.
 * NULL �������� ��� Column�� Align�� �ƴ� MaxAlign ���� ������ �����Ͽ��� �Ѵ�. */
#define SMC_GET_COLUMN_PAD_LENGTH(aValue, aColumn) ( ( aValue.length  > 0 ) ? \
                                                     ( aColumn->align - 1 ) : \
                                                     ( aColumn->maxAlign - 1 ) ) 

/*PROJ-2399 row template */
typedef struct smcColTemplate                                                                             
{        
    /* ����Ǵ� column�� ����, variable column�� ���
     * SMC_UNDEFINED_COLUMN_LEN�� ����ȴ�. */ 
    UShort   mStoredSize;
    /* column�� ���� ������ �����ϴ� ���� */
    UShort   mColLenStoreSize;
    /* �ش� column �տ� �����ϴ� ���尡��� variable column��
     * mColStartOffset ���� �����ϰ��ִ� ��ġ�� ����Ų��.*/
    UShort   mVariableColIdx; 
    /* �� row���� column�� ��ġ�ϴ� offset 
     * row�� �� rowpiece�� ����Ǿ��� ����� offset�̴�.
     * row�� ���� row piece�� ������������� ���� �ȴ�. */
    SShort   mColStartOffset;
}smcColTemplate;

typedef struct smcRowTemplate
{            
    /* row�� �����ϴ� column���� ���� */
    smcColTemplate * mColTemplate;
    /* row�� �����ϴ� variable column���� mColStartOffset
     * �����̴�. fetch�Ϸ��� column�� �տ� �����ϴ� non variable column��
     * ������ �� ������ ���� �ȴ�. */
    UShort         * mVariableColOffset; 
    /* table�� �� column �� */
    UShort           mTotalColCount;
    /* row�� �����ϴ� variable column�� �� */
    UShort           mVariableColCount; 
}smcRowTemplate; 

/* BUG-31206 improve usability of DUMPCI and DUMPDDF
 * smcTable::dumpTableHeaderByBuffer�� ���� ���ľ� �� */
typedef enum
{
    SMC_TABLE_NORMAL = 0,
    SMC_TABLE_TEMP,
    SMC_TABLE_CATALOG,
    SMC_TABLE_SEQUENCE,
    SMC_TABLE_OBJECT
} smcTableType;


typedef struct smcSequenceInfo
{
    SLong   mCurSequence;
    SLong   mStartSequence;
    SLong   mIncSequence;
    SLong   mSyncInterval;
    SLong   mMaxSequence;
    SLong   mMinSequence;
    SLong   mLstSyncSequence;
    UInt    mFlag;
    UInt    mAlign;
} smcSequenceInfo;

# define SMC_INDEX_TYPE_COUNT       (1024)

/* ------------------------------------------------
 * For A4 :
 * disk/memory ���̺��� ���� header
 *
 * disk table�� ��� fixed/variable column�� �������� �ʰ�
 * �������� �ʰ�, memory table���� �޸� disk table�� page list
 * entry�� fixed�� variable�� ������ �ʿ䰡 ����.
 * ----------------------------------------------*/
typedef struct smcTableHeader
{
    void             * mLock;    /* for normal table lock,
                                    smlLockItem */

    scSpaceID         mSpaceID;                                         

    /* ------------------------------------------------
     * FOR A4 : table type�� ���� main entry
     * sdpPageListEntry�� ������ ����
     *
     * ���� table header������ page list entry�� ���̺� type��
     * ���� union���� ó���ϰ�, disk type�� table entry��
     * sdpPageListEntry ����
     * ----------------------------------------------*/
    union
    {
        sdpPageListEntry  mDRDB;   /* disk page list entry */ 
        smpPageListEntry  mMRDB;   /* memory page list entry */
        smpPageListEntry  mVRDB;   /* volatile page list entry */
    } mFixed;

    // fixed memory page list�� �ش��ϴ� AllocPageList
    union
    {
        smpAllocPageListEntry mMRDB[SM_MAX_PAGELIST_COUNT];
        smpAllocPageListEntry mVRDB[SM_MAX_PAGELIST_COUNT];
    } mFixedAllocList;

    /* ------------------------------------------------
     * FOR A4 : disk table������ ������� ����
     * �ֳ��ϸ� variable column�� fixed page list����
     * ��� �����ϱ� ������ memory table���� ����ϴ�
     * variable column�� ���� page list�� �ʿ����.
     * ----------------------------------------------*/
    union
    {
        smpPageListEntry  mMRDB[SM_VAR_PAGE_LIST_COUNT];
        smpPageListEntry  mVRDB[SM_VAR_PAGE_LIST_COUNT];
    } mVar;

    /* BUG-13850 : var page list���� AllocPageList�� ����ȭ�Ǿ�
                   table header ũ�Ⱑ Ŀ���� �� page�� �Ѱ� �Ǿ�
                   AllocPageList�� page size�� ������� �����Ͽ� ����Ѵ�.*/
    union
    {
        smpAllocPageListEntry mMRDB[SM_MAX_PAGELIST_COUNT];
        smpAllocPageListEntry mVRDB[SM_MAX_PAGELIST_COUNT];
    } mVarAllocList;

    smcTableType       mType;    /* table Ÿ��  */
    smcObjectType      mObjType; /* object Ÿ�� */                        
    smOID              mSelfOID; /* table header�� ��ϵ� slot header�� ����Ŵ */

    smOID              mNullOID;

    /* Table Column���� */
    UInt               mColumnSize;     /* column ũ�� */
    UInt               mColumnCount;    /* column ���� */
    UInt               mLobColumnCount; /* lob column ���� */
    UInt               mUnitedVarColumnCount; /* United Variable column ���� */
    smVCDesc           mColumns;        /* column�� ����� Variable Column */

    smVCDesc           mInfo;           /* info�� ����� Varaiable Column */

    /* for sequence */
    smcSequenceInfo    mSequence;

    /* index management */
    smVCDesc           mIndexes[SMC_MAX_INDEX_OID_CNT]; /* list of created index */
    void             * mDropIndexLst; /* smnIndexHeader */
    UInt               mDropIndex;    /* dropindex list�� �߰��� index ���� */

    /* PROJ-1665
       - mFlag : table type ������ logging ���� ����
                 table type ���� ( SMI_TABLE_TYPE_MASK )
                 logging/nologging ���� ( SMI_INSERT_APPEND_LOGGING_MASK )
       - mParallelDegree  : parallel degree */
    UInt               mFlag;
    UInt               mParallelDegree;

    idBool             mIsConsistent; /* PROJ-2162 */

    /* To Fix BUG-17371  Aging�� �и���� System�� ������ �� Aging��
                         �и��� ������ �� ��ȭ��.

       Ager�� DDL Transaction�� ���� 1������ �����ߴ� ��Ȳ������
       iduSyncWord �����ε� ���ü� ��� �����߾���.

       ������ ���� ���� Ager�� ���ÿ� ����� �� �ִ� ��Ȳ�̴�.
       iduSyncWord��� iduLatch�� ����Ѵ�.
    */
    /* N���� Ager�� 1���� DDL�� ������ ���̴� RW Latch */
    iduLatch         * mIndexLatch ;

    ULong              mMaxRow;           /* max row */
    void             * mRuntimeInfo;      /* PROJ-1597 Temp record size ���� ����
                                             ���� mTempInfo�� �� ����� ����Ѵ�
                                             ���� runtime ������ ���� �� �ɹ��� ����Ѵ�. */

    /* TASK-4990 changing the method of collecting index statistics 
     * ���⼭ ����Ǵ� RowCount ���� ������� ������ ��ϵ� ������ ���� ������
     * ���̰� �ִ�. */
    smiTableStat       mStat;

    /* BUG-48588  table drop Ȯ�ο뵵. ���̺� ���� �� ���ž��� */
    smSCN              mTableCreateSCN;

    smcRowTemplate   * mRowTemplate;                   

    /* BUG-42928 No Partition Lock */
    void             * mReplacementLock;

    /* PROJ-1497 DB Migration�� �����ϱ� ���� Reserved ������ �߰� */
    ULong              mReserveArea[SMC_RESERV_SIZE];
} smcTableHeader;

/* BUG-42928 No Partition Lock */
#define SMC_TABLE_LOCK( tableHeader ) ( ( (tableHeader)->mReplacementLock != NULL ) ?   \
                                          (tableHeader)->mReplacementLock :             \
                                          (tableHeader)->mLock )

/* ------------------------------------------------
 * temp table�� ���� catalog table�� offset�� ��Ÿ����.
 * system 0�� page�� ������ ������ ����.
 *   ____________
 *   | membase  |
 *   |__________|
 *   |#         |--> catalog table for normal table
 *   |__________|
 *   |#         |--> catalog table for temp table
 *   |__________|
 *    ....
 * ----------------------------------------------*/
#define SMC_CAT_TEMPTABLE_OFFSET SMM_CACHE_ALIGN(SMM_CAT_TABLE_OFFSET              \
                                                 + ID_SIZEOF(struct smpSlotHeader) \
                                                 + ID_SIZEOF(struct smcTableHeader))

typedef struct smcTBSOnlineActionArgs
{
    void  * mTrans;
    ULong   mMaxSmoNo;
} smcTBSOnlineActionArgs;

// added for performance view.
typedef struct  smcTableInfoPerfV
{
    scSpaceID    mSpaceID;
    UInt         mType;
    //for memory  table.
    smOID        mTableOID;
    vULong       mMemPageCnt;
    ULong        mMemVarPageCnt;
    UInt         mMemSlotCnt;
    vULong       mMemSlotSize;
    vULong       mFixedUsedMem;
    vULong       mVarUsedMem;
    scPageID     mMemPageHead;

    //for disk  table
    ULong        mDiskTotalPageCnt;
    ULong        mDiskPageCnt;
    scPageID     mTableSegPID;

    scPageID     mTableMetaPage;
    sdRID        mFstExtRID;
    sdRID        mLstExtRID;
    UShort       mDiskPctFree;           // PCTFREE
    UShort       mDiskPctUsed;           // PCTUSED
    UShort       mDiskInitTrans;         // INITRANS ..
    UShort       mDiskMaxTrans;          // MAXTRANS ..
    UInt         mDiskInitExtents;       // Storage InitExtents 
    UInt         mDiskNextExtents;       // Storage NextExtents 
    UInt         mDiskMinExtents;        // Storage MinExtents 
    UInt         mDiskMaxExtents;        // Storage MaxExtents 

    //BUG-17371 [MMDB] Aging�� �и���� System�� ������ �� Aging��
    //�и��� ������ �� ��ȭ��.
    ULong       mOldVersionCount;       // Old Version����
    ULong       mStatementRebuildCount; // Statement Rebuild Count
    ULong       mUniqueViolationCount;  // Unique Violation Count
    ULong       mUpdateRetryCount;      // UpdateRow�� AlreadyModifiedError�� ���� RetryȽ��
    ULong       mDeleteRetryCount;      // DeleteRow�� AlreadyModifiedError�� ���� RetryȽ��
    ULong       mLockRowRetryCount;     // LockRow�� AlreadyModifiedError�� ���� RetryȽ��

    // TASK-2398 Log Compress
    //   �α� ���� ���� ( 1: ����, 0: ������� )
    UInt        mCompressedLogging;

    idBool      mIsConsistent; /* PROJ-2162 */
}smcTableInfoPerfV;

typedef struct  smcSequence4PerfV
{
    smOID        mSeqOID;
    smcSequenceInfo   mSequence;
}smcSequence4PerfV;

// PROJ-1362 QP - Large Record & Internal LOB ����.
// drop table pending�� ����ϰ� �� mColumns OID stack.
typedef struct smcOIDStack
{
    UInt   mLength;
    smOID* mArr;
}smcOIDStack;

/* Variable Column Log��Ͻ� After���� Before ����?*/
typedef enum
{
    SMC_VC_LOG_WRITE_TYPE_NONE = 0,
    SMC_VC_LOG_WRITE_TYPE_AFTERIMG,
    SMC_VC_LOG_WRITE_TYPE_BEFORIMG
} smcVCLogWrtOpt;

/* Update Inplace�� Log��Ͻ� After���� Before ����?*/
typedef enum
{
    SMC_UI_LOG_WRITE_TYPE_NONE = 0,
    SMC_UI_LOG_WRITE_TYPE_AFTERIMG,
    SMC_UI_LOG_WRITE_TYPE_BEFORIMG
} smcUILogWrtOpt;

/* smcRecordUpdate::makeLogFlag�� ���� flag */
/* BUG-14513 */
typedef enum
{
    SMC_MKLOGFLAG_NONE = 0,
    // Log Flag�� SMR_LOG_ALLOC_FIXEDSLOT_OK�� ���ϸ�.
    SMC_MKLOGFLAG_SET_ALLOC_FIXED_OK   = 0x00000001,
    // Log Flag�� SMR_LOG_ALLOC_FIXEDSLOT_NO�� ���ϸ�.
    SMC_MKLOGFLAG_SET_ALLOC_FIXED_NO   = 0x00000002,
    // Replication Sender�� LOG�� Skip�ϵ����Ѵ�.
    SMC_MKLOGFLAG_REPL_SKIP_LOG        = 0x00000004
} smcMakeLogFlagOpt;

/* ���� Log�� Replication Sender�� ����������? */
typedef enum
{
    SMC_LOG_REPL_SENDER_SEND = 0,
    SMC_LOG_REPL_SENDER_SEND_OK = 1,
    SMC_LOG_REPL_SENDER_SEND_NO = 2
} smcLogReplOpt;

/* smc ���� Function Argument */
#define SMC_WRITE_LOG_OK  (0x00000001)
#define SMC_WRITE_LOG_NO  (0x00000000)

typedef struct smcCatalogInfoPerfV
{
    smOID        mTableOID;
    UInt         mColumnCnt;
    UInt         mColumnVarSlotCnt;
    UInt         mIndexCnt;
    UInt         mIndexVarSlotCnt;
}smcCatalogInfoPerfV;

/******************************************************************
 *
 * PRJ-1362 MEMORY LOB
 *               ------------------------------
 * FixedRow         ... | smcLobDesc | ...
 *               ------------------------------
 * LobPieceControlHeader   | ��[smcLPCH|smcLPCH|smcLPCH|smcLPCH]
 *                         |       |       |       |       |
 * LobPiece                �� [varslot][varslot][varslot][varslot]
 *
 ******************************************************************/

 // Max In Mode LOB Size
#define  SMC_LOB_MAX_IN_ROW_STORE_SIZE   ( SM_LOB_MAX_IN_ROW_SIZE + ID_SIZEOF(smVCDescInMode) )

/* MEMORY LOB PIECE CONTROL HEADER */
typedef struct smcLPCH
{
    ULong        mLobVersion;
    smOID        mOID;         // Lob ����Ÿ piece�� OID
    void        *mPtr;         // Lob ����Ÿ piece�� ���� pointer (varslot)
}smcLPCH;

/* MEMORY LOB COLUMN DESCRIPTOR */
typedef struct smcLobDesc
{
    /* smVCDesc ����ü�� �����Ѵ�. --------------------------------- */
    UInt           flag;        // variable column�� �Ӽ� (In, Out)
    UInt           length;      // variable column�� ����Ÿ ����
    smOID          fstPieceOID; // variable column�� ù��° piece oid

    /* ------------------------------------------------------------- */
    UInt           mLPCHCount;  // Lob ����Ÿ piece ����
    ULong          mLobVersion; // Lob Version
    smcLPCH       *mFirstLPCH;  // ù��° LPCH(Lob Piece Control Header)
}smcLobDesc;

#define SMC_LOB_DESC_INIT( aLobDesc ) \
{   \
        aLobDesc->flag        = SM_VCDESC_MODE_IN; \
        aLobDesc->length      = 0; \
        aLobDesc->fstPieceOID = SM_NULL_OID; \
        aLobDesc->mLPCHCount  = 0; \
        aLobDesc->mLobVersion = 0; \
        aLobDesc->mFirstLPCH  = NULL; \
}

// lob column�� table���� ���° column������ ��Ÿ��.
typedef struct smcLobColIdxNode
{
    UInt       mColIdx;
    smuList    mListNode;
}smcLobColIdxNode;

typedef IDE_RC  (*smcFreeLobSegFunc)( idvSQL           *aStatistics,
                                      void*             aTrans,
                                      scSpaceID         aColSlotSpaceID,
                                      smOID             aColSlotOID,
                                      UInt              aColumnSize,
                                      sdrMtxLogMode     aLoggingMode );
typedef enum
{
    SMC_UPDATE_BY_TABLECURSOR,
    SMC_UPDATE_BY_LOBCURSOR
} smcUpdateOpt;


// BUG-20048
#define SM_TABLE_BACKUP_STR            "SM_TABLE_BACKUP"
#define SM_VOLTABLE_BACKUP_STR         "SM_VOLTABLE_BACKUP"
#define SM_TABLE_BACKUP_TYPE_SIZE      (40)

/* Table Backup File Header */
typedef struct smcBackupFileHeader
{
    // BUG-20048
    SChar           mType[SM_TABLE_BACKUP_TYPE_SIZE];
    SChar           mProductSignature[IDU_SYSTEM_INFO_LENGTH];
    SChar           mDbname[SM_MAX_DB_NAME]; // DB Name
    UInt            mVersionID;              // Altibase Version
    UInt            mCompileBit;             // Support Mode "32" or '"64"
    vULong          mLogSize;                // Logfile Size
    smOID           mTableOID;               // Table OID
    idBool          mBigEndian;              // big = ID_TRUE

    // PROJ-1579 NCHAR
    SChar           mDBCharSet[IDN_MAX_CHAR_SET_LEN];
    SChar           mNationalCharSet[IDN_MAX_CHAR_SET_LEN];
} smcBackupFileHeader;

typedef struct smcBackupFileTail
{
    smOID           mTableOID;               // Table OID
    ULong           mSize;                   // File Size
    UInt            mMaxRowSize;             // Max Row Size
} smcBackupFileTail;

typedef struct smcTmsCacheInfoPerfV
{
    scSpaceID      mSpaceID;           // TBS_ID
    scPageID       mSegPID;
    sdpHintPosInfo mHintPosInfo;
} smcTmsCacheInfoPerfV;

/* 
 * BUG-22677 DRDB����create table�� ������ recovery�� �ȵǴ� ��찡 ����. 
 */
#define SMC_IS_VALID_COLUMNINFO(fstPieceOID)  ( fstPieceOID  != SM_NULL_OID )

typedef IDE_RC (*smcAction4TBL)( idvSQL         * aStatistics,
                                 smcTableHeader * aHeader,
                                 void           * aActionArg );
typedef struct smcLobStatistics
{
    ULong  mOpen;
    ULong  mRead;
    ULong  mWrite;
    ULong  mErase;
    ULong  mTrim;
    ULong  mPrepareForWrite;
    ULong  mFinishWrite;
    ULong  mGetLobInfo;
    ULong  mClose;
} smcLobStatistics;

#endif /* _O_SMC_DEF_H_ */
