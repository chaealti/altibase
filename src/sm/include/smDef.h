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
 * $Id: smDef.h 90890 2021-05-26 08:38:25Z yoonhee.kim $
 **********************************************************************/

#ifndef _O_SM_DEF_H_
#define _O_SM_DEF_H_ 1

#include <idl.h>
#include <idv.h>
#include <smiDef.h>

/* ----------------------------------------------------------------------------
 *   For File Extension
 * --------------------------------------------------------------------------*/
#define SM_TABLE_BACKUP_EXT           ((SChar*)".tbk")





/* ------------------------------------------------
 * For recovery function map size
 * ----------------------------------------------*/
#define SM_MAX_RECFUNCMAP_SIZE       (200)





/* ----------------------------------------------------------------------------
 *   For Parallel Logging
 * --------------------------------------------------------------------------*/
/* SN�� �ִ밪 */
#define SM_SN_MAX  (ID_ULONG_MAX-1)
/* SN�� NULL�� */
#define SM_SN_NULL (ID_ULONG_MAX)
/* SN�� �ּҰ� */
#define SM_SN_MIN  (0)

/* Task-6549 LFG����
 * SM ���������� LFG�� ���� �Ǿ�����
 * ȣȯ�� ������ LogAnchor�� LogFile ������ LFGCount�� LFGID�� ���ܵξ���.
 * �̿� ���� LFGCount �Ǵ� LFGID�� �ʿ��Ұ��
 * ���� ���� ����ϵ��� �Ѵ�. */
#define SM_LFG_COUNT               (1)
#define SM_LFG_IDX                 (0)

typedef ULong   smSN;

//[TASK-6757]LFG,SN ���� smSN <-> smLSN
#define SM_LSN_OFFSET_BIT_SIZE      (SM_DVAR(32))   // 32 ��Ʈ
#define SM_LSN_OFFSET_MASK          (ID_UINT_MAX)

#define SM_MAKE_SN(LSN)                                 \
       ( ( (smSN)((LSN).mFileNo) << SM_LSN_OFFSET_BIT_SIZE ) | (LSN).mOffset )

#define SM_MAKE_LSN( LSN, SN )                               \
      { LSN.mFileNo = (smSN)SN >> SM_LSN_OFFSET_BIT_SIZE; \
        LSN.mOffset = (smSN)SN & SM_LSN_OFFSET_MASK; } 

/* BUG-35392 */
typedef UShort  smMagic;


/* ----------------------------------------------------------------------------
 *   NULL TRANS ID
 * --------------------------------------------------------------------------*/
# define SM_NULL_TID        ID_SINT_MAX
# define SM_NULL_TX_SLOT_NO ID_UINT_MAX





/* ----------------------------------------------------------------------------
 *   For Replication
 * --------------------------------------------------------------------------*/
typedef IDE_RC (*smGetMinSN)( const UInt * aRestartRedoFileNo, // BUG-14898
                              const UInt * aLastArchiveFileNo, // BUG-29115
                              smSN       * aSN );


/* BUG-26482 ��� �Լ��� CommitLog ��� ���ķ� �и��Ͽ� ȣ���մϴ�. */
typedef IDE_RC (*smIsReplCompleteBeforeCommit)( idvSQL      * aStatistics,
                                                const smTID   aTID,
                                                const smSN    aSN, 
                                                const UInt    aModeFlag );
typedef void   (*smIsReplCompleteAfterCommit)( idvSQL       * aStatistics,
                                               const smTID    aTID,
                                               const smSN     sBeginSN, 
                                               const smSN     sLastSN, 
                                               const UInt     aModeFlag,
                                               const smiCallOrderInCommitFunc );
/*PROJ-1670: Log Buffer for replication*/
typedef void   (*smCopyToRPLogBuf)( idvSQL * aStatistics,
                                    UInt     aSize,
                                    SChar  * aLogPtr,
                                    smLSN    aLSN );

/* PROJ-2453 Eager Replication performance enhancement */
typedef void   ( *smSendXLog )( const SChar  * aLogPtr );

typedef IDE_RC ( *smIsReplWaitGlobalTxAfterPrepare )( idvSQL       * aStatistics,
                                                      idBool         aIsRequestNode,
                                                      const smTID    aTID,
                                                      const smSN     aSN );

/* PROJ-1442 Replication Online �� DDL ��� */
/* QC_MAX_OBJECT_NAME_LEN --> 128 BUG-39579 */
#define SM_MAX_NAME_LEN ( 128 )

/* QC_MAX_PARTKEY_COND_VALUE_LEN BUG-45943 */
#define SM_MAX_PARTKEY_COND_VALUE_LEN  ( 4000 )

typedef IDE_RC (*smGetTbl4ReplFunc)( const void   * aMeta,
                                     ULong          aTableOID,
                                     const void  ** aTable);
typedef UInt (*smGetColumnCnt4ReplFunc)( const void * aTable );
typedef const smiColumn * (*smGetColumn4ReplFunc)( const void * aTable,
                                                   UInt         aColumnID );





/* ----------------------------------------------------------------------------
 *  log anchor�� ����
 * --------------------------------------------------------------------------*/
#define SM_LOGANCHOR_FILE_COUNT       (3)

#define SM_PINGPONG_COUNT   (2)  // ping & pong





/* ----------------------------------------------------------------------------
 *   For Parallel Page List(Database, Table)
 * --------------------------------------------------------------------------*/
/* Page List �ִ밹�� */
#if defined(SMALL_FOOTPRINT)
#define SM_MAX_PAGELIST_COUNT      (1)
#else
#define SM_MAX_PAGELIST_COUNT      (32)
#endif






/* ----------------------------------------------------------------------------
 *   (P)HYSICAL (T)IME(S)TAMP
 * --------------------------------------------------------------------------*/
/* DWFile�� �ִ� ���� */
#define SM_MAX_DWDIR_COUNT            (32)





/* ------------------------------------------------
 * LSN
 * ----------------------------------------------*/
#define SM_NULL_LSN

#define SM_LSN_INIT(aLSN)             \
{                                     \
    (aLSN).mFileNo = (aLSN).mOffset = 0;  \
}

#define SM_IS_LSN_INIT(aLSN)          \
    (((aLSN).mFileNo == 0) && ((aLSN).mOffset == 0))

#define SM_LSN_MAX(aLSN)                        \
{                                               \
    (aLSN).mFileNo = (aLSN).mOffset = ID_UINT_MAX;  \
}
#define SM_IS_LSN_MAX(aLSN)                   \
    ( (aLSN).mFileNo == ID_UINT_MAX &&        \
      (aLSN).mOffset == ID_UINT_MAX )

/* TASK-6549 LFG���ſ��� ȣȯ���� ���� 
 * LSN ������ LFGID�� ������ �����Ǿ�
 * LSN ������ LFGID�� �׻� �⺻ ���� �ǵ��� �����Ͽ���.*/
#define SM_SET_LSN(aLSN, aFileNo, aOffset)  \
{                               \
    (aLSN).mFileNo = (aFileNo); \
    (aLSN).mOffset = (aOffset); \
}

#define SM_GET_LSN(aDestLSN, aSrcLSN)       \
{                                           \
    (aDestLSN).mFileNo = (aSrcLSN).mFileNo; \
    (aDestLSN).mOffset = (aSrcLSN).mOffset; \
}





/* ----------------------------------------------------------------------------
 *   PAGE STRUCT DEFINE
 * --------------------------------------------------------------------------*/
#ifdef COMPILE_64BIT
#define SM_OID_BIT_SIZE    (SM_DVAR(64))   // 64 ��Ʈ : �� ��Ʈ ����
#define SM_DVAR(a)         (ID_ULONG(a))
#else
#define SM_OID_BIT_SIZE    (SM_DVAR(32))   // 32 ��Ʈ : �� ��Ʈ ����
#define SM_DVAR(a)         ((UInt)(a))
#endif

// �� �������� ũ�⸦ ��Ʈ ��� : Internal Tunable Parameter
// BIT �������� work-around ó��

#if defined(COMPILE_FOR_PAGE64)
#define SM_OFFSET_BIT_SIZE     (SM_DVAR(16))   // 2^(16-1) = 32K Byte : 15 = 32K
#define SM_ITEM_MIN_BIT_SIZE   (SM_DVAR(6))    // minimal var size is 64
#else
#if defined(SMALL_FOOTPRINT)
#define SM_OFFSET_BIT_SIZE     (SM_DVAR(12))   // 2^(12-1) =  4K Byte : 11 =  4K      
#else
#define SM_OFFSET_BIT_SIZE     (SM_DVAR(15))   // 2^(15-1) = 16K Byte : 14 = 16K    
#endif

#define SM_ITEM_MIN_BIT_SIZE   (SM_DVAR(5))    // minimal var size is 32
#endif

#define SM_VAR_COLUMN_BIT_SIZE SM_OFFSET_BIT_SIZE
#define SM_PAGE_BIT_SIZE       (SM_OID_BIT_SIZE - SM_OFFSET_BIT_SIZE)

#define SM_PAGE_SIZE           (SM_DVAR(1) << SM_OFFSET_BIT_SIZE) // �� ������ ũ��
#define SM_MAX_PAGE_COUNT      (SM_DVAR(1) << SM_PAGE_BIT_SIZE)   // �� ������ ����

#define SM_PAGE_MASK           ((SM_MAX_PAGE_COUNT - SM_DVAR(1)) << SM_OFFSET_BIT_SIZE)
#define SM_OFFSET_MASK         (SM_PAGE_SIZE - SM_DVAR(1))

// OID<->PID
// ToFix BUG-17191 : 4G�̻��� �޸𸮿� �ش�Ǵ� Page�� ���� OID<->PID��ȯ�� ����� �ȵ�
#define SM_MAKE_OID(pid, offset) ( (((vULong)(pid)) << SM_OFFSET_BIT_SIZE) | (offset) )
#define SM_MAKE_PID(a)           ( ((smOID)a)   >> SM_OFFSET_BIT_SIZE )
#define SM_MAKE_OFFSET(a)        ( ((smOID)a)   &  SM_OFFSET_MASK )





/* ----------------------------------------------------------------------------
 *                            LOCK ROW DEFINE
 * LOCK ROW�� smpSlotHeader�� mNext�κп��� ���� ���̴�.
 * --------------------------------------------------------------------------*/
#ifdef COMPILE_64BIT
#define SM_LOCK_MARK    (0x0000000000000002)
#else
#define SM_LOCK_MARK    (0x00000002)
#endif


typedef  ULong sdSID;

#define SD_SLOTNUM_MASK       ( SD_PAGE_SIZE - ID_ULONG(1))

#define SD_MAKE_SLOTNUM(sid)  ((scSlotNum)((sid) & SD_SLOTNUM_MASK))

#define SD_NULL_SID           ((sdSID)0)

#if defined(SMALL_FOOTPRINT)
#define SD_SLOTNUM_BIT_SIZE     (ID_ULONG(12))   // 2^12 = 4K Byte : 12 = 4K
#else
#define SD_SLOTNUM_BIT_SIZE     (ID_ULONG(13))   // 2^13 = 8K Byte : 13 = 8K
#endif

// SID<->PID,SLOTNUM
#define SD_MAKE_SID(pid, slotnum) ( ((sdSID)(pid)<< SD_SLOTNUM_BIT_SIZE) | \
                                   (slotnum) )
// GRID -> SID
#define SD_MAKE_SID_FROM_GRID( aGRID )               \
    SD_MAKE_SID( SC_MAKE_PID(aGRID), SC_MAKE_SLOTNUM(aGRID) )





/* ----------------------------------------------------------------------------
 *   sdRID : Disk��� DB�� ������ DB ������ ��Ÿ���� ����Ÿ Ÿ��.
 *           RID�� PageID, Offset���� �����ȴ�.
 *           64��Ʈ ũ�⸦ ��
 *
 *           [ scPageID | scOffset ] = sdRID
 *               32bit      16bit    = 64bit
 *           PageID�� 32��Ʈ (4G)���� �����ǰ�,
 *           Offset�� �����Ǹ�(�Ϲ������� 15��Ʈ),
 *           32��Ʈ���� �� Offset�� �� �������� .
 *
 *           PageID�� 32��Ʈ�� ������ ������ ���� �� ũ�Ⱑ 32��Ʈ �̻��� ��쿡��
 *           scPageID�� �����ϱ� ���� ������ ULong�� �Ǿ�� �ϴµ�,
 *           �̷��� �Ǹ�, ��������� UInt�� ������ 2�谡 Ŀ�� �Ѵ�.
 *           ����, ��������� �����ϱ� ���� �ִ� 32��Ʈ�� �����Ͽ���.
 * --------------------------------------------------------------------------*/
typedef  ULong sdRID;

typedef  UShort  sdFileID;
typedef  UInt    sdFilePID;

#define SD_RID_BIT_SIZE        (ID_ULONG(64))   // 64 ��Ʈ : �� ��Ʈ ����

#if defined(SMALL_FOOTPRINT)
#define SD_OFFSET_BIT_SIZE     (ID_ULONG(12))   // 2^12 = 4K Byte : 12 = 4K
#else
#define SD_OFFSET_BIT_SIZE     (ID_ULONG(13))   // 2^13 = 8K Byte : 13 = 8K
#endif

#define SD_PAGE_BIT_SIZE       (ID_ULONG(32))   // 2^15 = 32K Byte : 15 = 32K

#define SD_PAGE_SIZE           (ID_ULONG(1) << SD_OFFSET_BIT_SIZE)//������ ũ��
#define SD_MAX_PAGE_COUNT      (ID_ULONG(1) << SD_PAGE_BIT_SIZE)  //������������

#define SD_OFFSET_MASK       ( SD_PAGE_SIZE - ID_ULONG(1))
#define SD_PAGE_MASK         ((SD_MAX_PAGE_COUNT - ID_ULONG(1)) << SD_OFFSET_BIT_SIZE)

// RID<->PID,OFFSET
#define SD_MAKE_RID(pid, offset) ( ((sdRID)(pid)<< SD_OFFSET_BIT_SIZE) | \
                                   (offset) )
#define SD_MAKE_RID_FROM_GRID( aGRID )               \
    SD_MAKE_RID( SC_MAKE_PID(aGRID), SC_MAKE_OFFSET(aGRID) )

#define SD_MAKE_PID(rid)                                                \
    ((scPageID)(((rid)  & SD_PAGE_MASK)  >> SD_OFFSET_BIT_SIZE))
#define SD_MAKE_OFFSET(rid)   ((scOffset)((rid)  & SD_OFFSET_MASK))

#define SD_NULL_RID           ((sdRID)0)
#define SD_NULL_PID           ((scPageID)0)

#define SD_PAGEID_BIT_SIZE     ((UInt)32)
#define SD_FID_BIT_SIZE        ((UInt)10)
#define SD_FPID_BIT_SIZE       ((UInt)22)
#define SD_CREATE_PID(fid,fpid) ( ((scPageID)(fid) << SD_FPID_BIT_SIZE) | (fpid) )

#define SD_MAX_FPID_COUNT     ( (UInt)1 << SD_FPID_BIT_SIZE )
#define SD_MAX_FID_COUNT      ( (UInt)1 << SD_FID_BIT_SIZE )
#define SD_FPID_MASK          ( SD_MAX_FPID_COUNT - (UInt)1 )
#define SD_MAX_FILE_SIZE      ( SD_MAX_FPID_COUNT * SD_PAGE_SIZE )
#define SD_FID_MASK           ( ( SD_MAX_FID_COUNT - (UInt)1 ) << SD_FPID_BIT_SIZE )
#define SD_MAKE_FID(pid)      (((UInt)pid & SD_FID_MASK) >> SD_FPID_BIT_SIZE )
#define SD_MAKE_FPID(pid)     ((UInt)pid & SD_FPID_MASK)
#define SD_MAKE_FOFFSET(pid)  ( SD_MAKE_FPID( pid ) * SD_PAGE_SIZE )
#define SD_MAX_DATAFILE_SIZE  ( SD_MAX_FPID_COUNT * SD_PAGE_SIZE )




/* ----------------------------------------------------------------------------
 *                            DBFILE HEADER OFFSET & SIZE
 * --------------------------------------------------------------------------*/
#define SM_DBFILE_METAHDR_PAGE_OFFSET (0)
#define SM_DBFILE_METAHDR_PAGE_SIZE   (SD_PAGE_SIZE) // 8KB





/* ----------------------------------------------------------------------------
 *                            DBFILE HEADER OFFSET & SIZE
 * --------------------------------------------------------------------------*/
#define SM_SBFILE_METAHDR_PAGE_OFFSET (0)
#define SM_SBFILE_METAHDR_PAGE_SIZE   (SD_PAGE_SIZE) // 8KB





/* ----------------------------------------------------------------------------
 *                            PAGE STRUCT DEFINE
 * --------------------------------------------------------------------------*/
#define SM_MEM_POOL_SIZE      ((UInt)(64 * 1024))
#define SM_MAX_DB_NAME        ((UInt)128)
#define SM_MAX_FILE_NAME      ((UInt)512)





/* ------------------------------------------------
 *  BUG-26939 inefficient variable slot size
 * ----------------------------------------------*/
#define SM_VAR_PAGE_LIST_COUNT ((UInt)18)


#if 0 // not used
/* mtdTypes.h MTD_CHAR_STORE_PRECISION_MAXIMUM(32000) �� ���� ���� 
 * mt Size ǥ�������� value �տ� 2 byte �߰��Ǵ� ���� ����Ͽ� +2�� ����. */
#define SM_CHAR_MAX_SIZE    ((UInt)32000)
#define SM_IS_LARGER_THAN_CHAR_MAX(COLSIZE)    ((COLSIZE) > (SM_CHAR_MAX_SIZE + 2))
#endif

/* ----------------------------------------------------------------------------
 *    for Index
 * --------------------------------------------------------------------------*/
#define SM_NULL_INDEX_ID    ((UInt)0)


/* ----------------------------------------------------------------------------
 *                            OID DEFINE
 * --------------------------------------------------------------------------*/

#define SM_NULL_OID           ((smOID)0)
#define SM_NULL_PID           ((scPageID)0)

// SM Page ID�� ���� �� �ִ� �ִ� ��
#define SM_MAX_PID            ((scPageID) ( ID_UINT_MAX - 1 ))
#define SM_SPECIAL_PID        ((scPageID) ( ID_UINT_MAX ))

//OID�� 8byte aligne�Ǿ��ֱ� ������ �׻� ������ 3bit�� 0�̴�.
//�� ��ũ�� �Լ��� ������ 3bit�� ��� 0�϶� true �׷��� ������ false�� �����Ѵ�.
#define SM_IS_OID(ID)         (!((ID) & 0x07))
#define SM_IS_NULL_OID(ID)    ((ID) == SM_NULL_OID)
#define SM_IS_VALID_OID(ID)   ( SM_IS_OID(ID) && ((ID) != SM_NULL_OID) )





/* ----------------------------------------------------------------------------
 *    for Variable Column
 * --------------------------------------------------------------------------*/
/* smVCDesc::flag, smVCDescInMode::flag
   Fixed Row�� ���� Row�� Variable Column�� ����Ǹ� In-Mode,
   ������ Variable Row ������ ����Ǹ� Out-Mode */
#define   SM_VCDESC_MODE_MASK  SMI_COLUMN_MODE_MASK
#define   SM_VCDESC_MODE_IN    SMI_COLUMN_MODE_IN
#define   SM_VCDESC_MODE_OUT   SMI_COLUMN_MODE_OUT

#define   SM_VCDESC_IS_MODE_IN(LobDesc)  ( ( ( (LobDesc)->flag) & SM_VCDESC_MODE_MASK) == \
                                           SM_VCDESC_MODE_IN )

/* whether LOB column is null or not */
#define   SM_VCDESC_NULL_LOB_MASK  (0x00010000)
#define   SM_VCDESC_NULL_LOB_FALSE (0x00000000)
#define   SM_VCDESC_NULL_LOB_TRUE  (0x00010000)

/* Variable Column Piece�� Free����? */
#define   SM_VCPIECE_FREE_MASK  (0x0000001)
#define   SM_VCPIECE_FREE_OK    (0x0000000)
#define   SM_VCPIECE_FREE_NO    (0x0000001)

/* Variable Piece Type */
#define   SM_VCPIECE_TYPE_MASK           (0x0000070)
#define   SM_VCPIECE_TYPE_MEMORY_COLUMN  (0x0000000)
#define   SM_VCPIECE_TYPE_DISK_COLUMN    (0x0000010)
#define   SM_VCPIECE_TYPE_TEMP_COLUMN    (0x0000020)
#define   SM_VCPIECE_TYPE_DISK_INDEX     (0x0000030)
#define   SM_VCPIECE_TYPE_MEMORY_INDEX   (0x0000040)
#define   SM_VCPIECE_TYPE_TEMP_INDEX     (0x0000050)
#define   SM_VCPIECE_TYPE_OTHER          (0x0000060)

#define   SM_VCPIECE_IS_DISK_INDEX(FLAG)   ( (FLAG & SM_VCPIECE_TYPE_MASK) ==  \
                                             SM_VCPIECE_TYPE_DISK_INDEX ) 

#define   SM_VCPIECE_IS_MEMORY_INDEX(FLAG) ( (FLAG & SM_VCPIECE_TYPE_MASK) ==  \
                                             SM_VCPIECE_TYPE_MEMORY_INDEX ) 


/* Fixed Row���� Variable Column�� ����Ű�� Column Descriptor*/
/* In Mode�� ����� ��� Column Desc */
typedef struct smVCDescInMode
{
    UInt          flag;   // variable column�� �Ӽ� (In, Out)
    UInt          length; // variable column�� ����Ÿ ����
} smVCDescInMode;

/* Out Mode�� ����� ��� Column Desc */
/* smcLobDesc�͵� ������ �����Ѵ�.    */
typedef struct smVCDesc
{
    UInt           flag;   // variable column�� �Ӽ� (In, Out)
    UInt           length; // variable column�� ����Ÿ ����

    smOID          fstPieceOID; // variable column�� ù��° piece oid
} smVCDesc;

/* smVCPieceHeader::flag
   Variable Column�� �����ϴ� �� Piece�� Header
 */
typedef struct smVCPieceHeader
{
    /* Alloced : Variable Piece�� ���� vc piece oid
       Freed : next free slot oid */
    smOID          nxtPieceOID;
    UShort         flag;   // variable piece info(Used, Free)
    union
    {
        UShort     length; // piece�� ����Ÿ ����
        UShort     colCount;// united var piece ������ �� ���Կ� ����� �÷� ���� �ǹ��Ѵ�
    };
} smVCPieceHeader;

// LOB In Mode Max Size (BUG-30101)
// idpDescResource �� IDP_MAX_LOB_IN_ROW_SIZE�� ������ ���̾�� �Ѵ�.
#define  SM_LOB_MAX_IN_ROW_SIZE   ( 4000 )

//PROJ-1362 PROJ-1362 Large Record & Internal LOB support
typedef enum smLobExecMode
{
    SM_LOB_EXEC_MODE_NONE = 0,
    SM_LOB_EXEC_MODE_BY_SQL,
    SM_LOB_EXEC_MODE_BY_API,
    SM_LOB_EXEC_MODE_MAX
} smLobExecMode;

typedef enum smLobWritePhase
{
    SM_LOB_WRITE_PHASE_NONE = 0,
    SM_LOB_WRITE_PHASE_PREPARE,
    SM_LOB_WRITE_PHASE_WRITE,
    SM_LOB_WRITE_PHASE_COPY,
    SM_LOB_WRITE_PHASE_FINISH
} smLobWritePhase;

/* PROJ-2728 Sharding LOB */
typedef struct smShardLobCursor
{
    UInt             mMmSessId;         // mmcSession ID
    UInt             mMmStmtId;         // mmcStatement ID
    UInt             mRemoteStmtId;     // Remote Statement ID
    UInt             mNodeId;           // Node ID
    SShort           mLobLocatorType;   // BLOB or CLOB
    smLobLocator     mRemoteLobLocator; // Remote Lob Locator
} smShardLobCursor;

typedef struct smLobViewEnv
{
    void*               mTable;     // smcTableHeader
    union
    {
        void*           mRow;       // memory row
        scGRID          mGRID;      // disk row grid
    };
    smiColumn           mLobCol;    // lob column
    smTID               mTID;       // Transaction id
    smSCN               mSCN;       // LobCursor SCN.
    smSCN               mInfinite;  // LobCursor Infinite
    smiLobCursorMode    mOpenMode;  // LobCursor open mode read or read write

    UInt                mWriteOffset;
    smLobWritePhase     mWritePhase;
    idBool              mWriteError;
   
    /*
     * For Disk LOB
     */
    
    idBool              mIsWritten; // RP���� in-mode ������Ʈ��
                                    // ��� Standby�� finishWrite����
                                    // ������ ������Ʈ�� �����ϱ�
                                    // ����
                                    //
                                    // in-mode ������Ʈ�� ��� Standby���� write�� ������ �� ����
                                    // ���� open lob cursor���� read �� �÷�����
                                    // finish write�ÿ� ���� �� �� ����.
                                    // 
                                    // 1. open lob cursor
                                    // 2. prepare for write
                                    // 3. write (������) -> ��� update �αװ� ����
                                    // 4. finish write
                                    // 5. close lob cursor
    
    UInt                mLastWriteOffset;
    scPageID            mLastWriteLeafNodePID;

    UInt                mLastReadOffset;
    scPageID            mLastReadLeafNodePID;

    ULong               mLobVersion;
    UShort              mColSeqInRowPiece;
    void*               mLobColBuf;

    /*
     * PROJ-2728 Sharding LOB
     */
    smShardLobCursor   *mShardLobCursor;
} smLobViewEnv;

// lob open function
typedef  IDE_RC (*smLobOpenFunc)();
// lob close function
typedef  IDE_RC (*smLobCloseFunc)(idvSQL*       aStatistics,
                                  void*         aTrans,
                                  smLobViewEnv* aLobViewEnv);
// lob read function
typedef  IDE_RC (*smLobReadFunc)(idvSQL*        aStatistics,
                                 void*          aTrans,
                                 smLobViewEnv*  aLobViewEnv,
                                 UInt           aOffset,
                                 UInt           aMount,
                                 UChar*         aPiece,
                                 UInt*          aReadLength);
// lob update function.
typedef  IDE_RC (*smLobWriteFunc)(idvSQL*        aStatistics,
                                  void*          aTrans,
                                  smLobViewEnv*  aLobViewEnv,
                                  smLobLocator   aLobLocator, // for replication
                                  UInt           aOffset,
                                  UInt           aPieceLen,
                                  UChar*         aPiece,
                                  idBool         aIsFromAPI,
                                  UInt           aContType); // for disk repl

typedef  IDE_RC (*smLobEraseFunc)(idvSQL*        aStatistics,
                                  void*          aTrans,
                                  smLobViewEnv*  aLobViewEnv,
                                  smLobLocator   aLobLocator, // for replication
                                  UInt           aOffset,
                                  UInt           aSize); // for disk repl

typedef  IDE_RC (*smLobTrimFunc)(idvSQL*        aStatistics,
                                 void*          aTrans,
                                 smLobViewEnv*  aLobViewEnv,
                                 smLobLocator   aLobLocator, // for replication
                                 UInt           aOffset); // for disk repl

// lob resize function
typedef IDE_RC (*smLobPrepare4WriteFunc)(
                       idvSQL*        aStatistics,
                       void*          aTrans,
                       smLobViewEnv*  aLobViewEnv,
                       smLobLocator   aLobLocator,
                       UInt           aOffset,
                       UInt           aNewSize);

typedef IDE_RC (*smLobFinishWriteFunc)(
                       idvSQL*       aStatistics,
                       void*         aTrans,
                       smLobViewEnv* aLobViewEnv,
                       smLobLocator  aLobLocator);

typedef IDE_RC (*smLobGetLobInfoFunc)(
                        idvSQL*        aStatistics,
                        void*          aTrans,
                        smLobViewEnv*  aLobViewEnv,
                        UInt*          aLobLen,
                        UInt*          aStoredMode,
                        idBool*        aIsNullLob);

// replication�� ���� Lob Cursor open/close log.
typedef  IDE_RC (*smLobWriteLog4LobCursorOpen)(
                       idvSQL*        aStatistics,
                       void*          aTrans,
                       smLobLocator   aLobLocator,
                       smLobViewEnv*  aLobViewEnv);


typedef struct smLobModule
{
    smLobOpenFunc                     mOpen;
    smLobReadFunc                     mRead;
    smLobWriteFunc                    mWrite;
    smLobEraseFunc                    mErase;
    smLobTrimFunc                     mTrim;
    smLobPrepare4WriteFunc            mPrepare4Write;
    smLobFinishWriteFunc              mFinishWrite;
    smLobGetLobInfoFunc               mGetLobInfo;
    smLobWriteLog4LobCursorOpen       mWriteLog4LobCursorOpen;
    smLobCloseFunc                    mClose;
}smLobModule;

typedef struct smLobCursor
{
    smLobCursorID      mLobCursorID;  // LOB CursorID
    smLobViewEnv       mLobViewEnv;
    UInt               mInfo;         // not null ����� QP���� �����.

    smShardLobCursor   mShardLobCursor;

    smLobCursor*       mPrev;
    smLobCursor*       mNext;
    smLobModule*       mModule;       // disk or  memory Lob Module.
} smLobCursor;





/* ----------------------------------------------------------------------------
 *                            For DISK LOG Attributes
 * --------------------------------------------------------------------------*/
# define SM_DISK_NTALOG_DATA_COUNT  (6)

# define SM_DLOG_ATTR_DEFAULT        (SM_DLOG_ATTR_NORMAL       | \
                                      SM_DLOG_ATTR_NONDML       | \
                                      SM_DLOG_ATTR_MTX_LOGBUFF  | \
                                      SM_DLOG_ATTR_REDOONLY )

# define SM_DLOG_ATTR_TRANS_MASK     (0x00000001)
# define SM_DLOG_ATTR_NORMAL         (0x00000000) // NORMAL �α� �Ӽ�
# define SM_DLOG_ATTR_REPLICATE      (0x00000001) // replication �α� �Ӽ�

# define SM_DLOG_ATTR_DML_MASK       (0x00000002)
# define SM_DLOG_ATTR_NONDML         (0x00000000)
# define SM_DLOG_ATTR_DML            (0x00000002) // DML ���� update �α� �Ӽ�

# define SM_DLOG_ATTR_WRITELOG_MASK  (0x00002000)
# define SM_DLOG_ATTR_MTX_LOGBUFF    (0x00000000) // Mtx�� Dynamic Array�� �α׹��۷� ���
# define SM_DLOG_ATTR_TRANS_LOGBUFF  (0x00002000) // Ʈ����� �α׹��۸� ���� �α׷� Write�Ѵ�.

# define SM_DLOG_ATTR_LOGTYPE_MASK   (0x00000F00)
# define SM_DLOG_ATTR_REDOONLY       (0x00000000) // redo Ÿ��
# define SM_DLOG_ATTR_UNDOABLE       (0x00000100) // insert/update/delete/lock undo Ÿ��
# define SM_DLOG_ATTR_NTA            (0x00000200) // NTA
# define SM_DLOG_ATTR_CLR            (0x00000300) // CLR
# define SM_DLOG_ATTR_REF_NTA        (0x00000400) // Referenced NTA
# define SM_DLOG_ATTR_DUMMY          (0x00000500) // insert/update/delete/lock undo Ÿ��

# define SM_DLOG_ATTR_EXCEPT_MASK               (0x00001000)
# define SM_DLOG_ATTR_EXCEPT_NONE               (0x00000000)
# define SM_DLOG_ATTR_EXCEPT_INSERT_APPEND_PAGE (0x00001000)



/* smcRecord::insertVersion Flag�� ���� */
#define SM_INSERT_ADD_OID_MASK        (0x00000002)
#define SM_INSERT_ADD_OID_OK          (0x00000002)
#define SM_INSERT_ADD_OID_NO          (0x00000000)

#define SM_INSERT_ADD_LPCH_OID_MASK    (0x00000001)
#define SM_INSERT_ADD_LPCH_OID_OK      (0x00000001)
#define SM_INSERT_ADD_LPCH_OID_NO      (0x00000000)

/* smcRecord::insertVersion Flag���� ��
   sdcRecord::insert������ Insert Undo Record Flag�μ� ���� */
#define SM_INSERT_NEED_INSERT_UNDOREC_MASK (0x00000008)
#define SM_INSERT_NEED_INSERT_UNDOREC_OK   (0x00000008)
#define SM_INSERT_NEED_INSERT_UNDOREC_NO   (0x00000000)

// PROJ-1502 PARTITIONED DISK TABLE
#define SM_UNDO_LOGGING_MASK         (0x00000004)
#define SM_UNDO_LOGGING_OK           (0x00000004)
#define SM_UNDO_LOGGING_NO           (0x00000000)


#define SM_INSERT_ADD_OID_IS_OK(FLAG)      ( (FLAG & SM_INSERT_ADD_OID_MASK)      \
                                            == SM_INSERT_ADD_OID_OK )
#define SM_INSERT_ADD_LPCH_OID_IS_OK(FLAG) ( (FLAG & SM_INSERT_ADD_LPCH_OID_MASK) \
                                             == SM_INSERT_ADD_LPCH_OID_OK )

#define SM_UNDO_LOGGING_IS_OK(FLAG)        ( (FLAG & SM_UNDO_LOGGING_MASK) == SM_UNDO_LOGGING_OK )

#define SM_INSERT_NEED_INSERT_UNDOREC_IS_OK(FLAG) ( (FLAG & SM_INSERT_NEED_INSERT_UNDOREC_MASK) \
                                                    == SM_INSERT_NEED_INSERT_UNDOREC_OK )

/*
 * [BUG-24353] volatile table�� modify column ������ ���ڵ尡 ������ϴ�.
 * - Volatile Table�� Compensation Log�� ������� �ʴ´�.
 */

/* BUG-42411 undo������ ȣ��Ǵ� restore�� LPCH OID�� ������� �ʽ��ϴ� 
 * resore �ܰ迡���� OID�� ������� �ʽ��ϴ�.
 * volatile+undo�� insert log�� ������� ������ 
 * �ٸ� ���(�Ϲ� table, update, trim... ) flag�� �����ʰ� log�� ����մϴ�.
 * undo �ܰ迡�� ȣ��Ǵ� �Լ����� xxxx_UNDO��� suffix�� ���� flag�� ���޵˴ϴ�. */
/* alter table xxx �� ���� backup & create ����� �̿� */
#define SM_FLAG_TABLE_RESTORE_UNDO (SM_INSERT_ADD_OID_NO | SM_INSERT_ADD_LPCH_OID_NO | SM_UNDO_LOGGING_NO)
#define SM_FLAG_TABLE_RESTORE      (SM_INSERT_ADD_OID_NO | SM_INSERT_ADD_LPCH_OID_OK | SM_UNDO_LOGGING_OK)

#define SM_FLAG_MAKE_NULLROW_UNDO  (SM_INSERT_ADD_OID_NO | SM_INSERT_ADD_LPCH_OID_NO | SM_UNDO_LOGGING_NO)
#define SM_FLAG_MAKE_NULLROW       (SM_INSERT_ADD_OID_NO | SM_INSERT_ADD_LPCH_OID_OK | SM_UNDO_LOGGING_OK)

#define SM_FLAG_INSERT_LOB_UNDO    (SM_INSERT_ADD_OID_OK | SM_INSERT_ADD_LPCH_OID_OK | SM_UNDO_LOGGING_NO)
#define SM_FLAG_INSERT_LOB         (SM_INSERT_ADD_OID_OK | SM_INSERT_ADD_LPCH_OID_OK | SM_UNDO_LOGGING_OK)

/* ============================================================================
 *                      Definition of SCN
 * ==========================================================================*/
/* SCN�� 8Bytes�̹Ƿ� Hex�� String ����ϸ� 16 ����Ʈ�� �ʿ� */
#define SM_SCN_STRING_LENGTH         (16)




/* ----------------------------------------------------------------------------
 *                Definition of SCN for 64bit
 * --------------------------------------------------------------------------*/
#ifdef COMPILE_64BIT

/* PROJ-2162 Restart Recovery Reduction
 * 64/32 ������� ULong ������ ��ȯ�Ͽ� �д� ��ũ�� */
# define SM_SCN_TO_LONG( src )             ((ULong)src)

# define SM_GET_SCN( dest, src )           *((smSCN *)(dest)) = *((smSCN *)(src));
# define SM_SET_SCN( dest, src )           *((smSCN *)(dest)) = *((smSCN *)(src));
# define SM_SET_MAX_SCN( dest, src )            \
    do {                                        \
        if ( SM_SCN_IS_GT( (src), (dest) ) )    \
        {                                       \
            SM_SET_SCN( (dest), (src) );        \
        }                                       \
    } while (0)

# define SM_INCREASE_SCN( scn )            *((smSCN *)(scn)) = *((smSCN *)(scn)) + 8;
# define SM_ADD_SCN( scn, increment )      *((smSCN *)(scn)) = *((smSCN *)(scn)) + (increment);


# define SM_GET_SCN_LOWVBIT(src)                          \
         (*((smSCN*)(src)) & ID_ULONG(0x0000000000000001))
# define SM_GET_SCN_HIGHVBIT(src)                         \
         (*((smSCN *)(src)) & ID_ULONG(0x8000000000000000))

#define SM_SCN_INIT              (ID_ULONG( 0x0000000000000000 ))
#define SM_SCN_INFINITE          (ID_ULONG( 0x8000000000000000 ))
#define SM_SCN_LOCK_ROW          (ID_ULONG( 0xFFFFFFFF00000000 ))
#define SM_SCN_MAX               (ID_ULONG( 0x7FFFFFFFFFFFFFE8 ))
#define SM_SCN_NULL_ROW          (ID_ULONG( 0x7FFFFFFFFFFFFFF0 ))
#define SM_SCN_FREE_ROW          (ID_ULONG( 0x7FFFFFFFFFFFFFF9 ))

#define SM_SCN_INF_DELETE_BIT    (ID_ULONG( 0x0000000100000000 ))
#define SM_SCN_INF_INC           (ID_ULONG( 0x0000000200000000 ))

#define SM_SCN_VIEW_BIT          (ID_ULONG( 0x0000000000000007 ))
#define SM_SCN_DELETE_BIT        (ID_ULONG( 0x0000000000000001 ))
#define SM_SCN_COMMIT_PARITY_BIT (ID_ULONG( 0x0000000000000002 ))
#define SM_SCN_COMMIT_LEGACY_BIT (ID_ULONG( 0x0000000000000004 ))

#define SM_CLEAR_SCN_LEGACY_BIT(scn)                         \
        *((smSCN *)(scn)) &= ~SM_SCN_COMMIT_LEGACY_BIT;     
#define SM_SET_SCN_LEGACY_BIT(scn)                           \
        *((smSCN *)(scn)) |= SM_SCN_COMMIT_LEGACY_BIT;

#define SM_CLEAR_SCN_VIEW_BIT(scn)                           \
        *((smSCN *)(scn)) &= ~SM_SCN_VIEW_BIT;                
#define SM_SET_SCN_VIEW_BIT(scn)                             \
        *((smSCN *)(scn)) |= SM_SCN_VIEW_BIT;                

#define SM_SCN_IS_VIEWSCN(scn)                               \
        ( (((scn) & SM_SCN_INFINITE) != SM_SCN_INFINITE) &&  \
          (((scn) & SM_SCN_VIEW_BIT ) == SM_SCN_VIEW_BIT) )

#define SM_SCN_IS_SYSTEMSCN(scn)                             \
        ( (((scn) & SM_SCN_INFINITE) != SM_SCN_INFINITE) &&  \
          (((scn) & SM_SCN_VIEW_BIT ) == 0x00) )

#define SM_SCN_IS_COMMITSCN(scn)                             \
        ( (((scn) & SM_SCN_INFINITE) != SM_SCN_INFINITE) &&  \
          (((scn) & SM_SCN_COMMIT_PARITY_BIT ) != SM_SCN_COMMIT_PARITY_BIT) )


// << PROJ-1591 
#define SM_SCN_CI_INIT      (ID_ULONG( 0x8000000000000000 ))
#define SM_SCN_CI_INFINITE  (ID_ULONG( 0xFFFFFFFFFFFFFFFF ))
#define SM_SCN_CI_MAX       (ID_ULONG( 0xFFFFFFFFFFFFFFFE ))

#define SM_INIT_CI_SCN( SCN )           *((smSCN *)(SCN)) = SM_SCN_CI_INIT;
#define SM_MAX_CI_SCN( SCN )            *((smSCN *)(SCN)) = SM_SCN_CI_MAX;
#define SM_SET_SCN_CI_INFINITE(SCN)     *((smSCN *)(SCN)) = SM_SCN_CI_INFINITE;

#define SM_SCN_IS_CI_INFINITE( SCN )     ((SCN) == SM_SCN_CI_INFINITE)
#define SM_SCN_IS_NOT_CI_INFINITE( SCN ) ((SCN) <  SM_SCN_CI_INFINITE)

#define SM_SCN_IS_CI_INIT(SCN)          ((SCN) == SM_SCN_CI_INIT)
#define SM_SCN_IS_CI_MAX(SCN)           ((SCN) == SM_SCN_CI_MAX)
// >> PROJ-1591

#define SM_INF_SCN_TID_MASK             (ID_ULONG( 0x00000000FFFFFFFF ))
#define SM_INIT_SCN( SCN )              *((smSCN *)(SCN)) = SM_SCN_INIT;
#define SM_MAX_SCN( SCN )               *((smSCN *)(SCN)) = SM_SCN_MAX;
#define SM_SET_SCN_INFINITE(SCN)        *((smSCN *)(SCN)) = (SM_SCN_INFINITE);
#define SM_SET_SCN_INFINITE_AND_TID(SCN, TID)                                 \
        *((smSCN *)(SCN)) = (SM_SCN_INFINITE | (TID & SM_INF_SCN_TID_MASK));

#define SM_ADD_INF_SCN( scn )                                                 \
        *((smSCN *)(scn)) = *((smSCN *)(scn)) + SM_SCN_INF_INC;

#define SM_SET_SCN_LOCK_ROW(SCN, TID)                                         \
        *((smSCN *)(SCN)) = (SM_SCN_LOCK_ROW | (TID & SM_INF_SCN_TID_MASK));
#define SM_MAKE_SCN_LOCK_ROW( TID )                                         \
        (SM_SCN_LOCK_ROW | (TID & SM_INF_SCN_TID_MASK))

#define SM_SET_SCN_DELETE_BIT(SCN)         {                                  \
    if( SM_SCN_IS_INFINITE( *((smSCN*)(SCN)) ) )                              \
    {                                                                         \
        *((smSCN *)(SCN)) |= SM_SCN_INF_DELETE_BIT;                           \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        *((smSCN *)(SCN)) |= SM_SCN_DELETE_BIT;                               \
    }                                                                         \
}

#define SM_CLEAR_SCN_DELETE_BIT(SCN)       {                                  \
    if( SM_SCN_IS_INFINITE( *((smSCN*)(SCN)) ) )                              \
    {                                                                         \
        *((smSCN *)(SCN)) &= ~SM_SCN_INF_DELETE_BIT;                          \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        *((smSCN *)(SCN)) &= ~SM_SCN_DELETE_BIT;                              \
    }                                                                         \
}

#define SM_GET_TID_FROM_INF_SCN(SCN)                                          \
      (SM_SCN_IS_INFINITE((SCN))?((SCN) & SM_INF_SCN_TID_MASK):SM_NULL_TID)

      

#define SM_SET_SCN_NULL_ROW(SCN)       *((smSCN *)(SCN))  = SM_SCN_NULL_ROW;
#define SM_SET_SCN_FREE_ROW(SCN)       *((smSCN *)(SCN))  = SM_SCN_FREE_ROW;

#define SM_SCN_IS_INFINITE( SCN )      ( ( SCN ) >= SM_SCN_INFINITE )
#define SM_SCN_IS_NOT_INFINITE( SCN )  ( ( SCN ) <  SM_SCN_INFINITE )

#define SM_SCN_IS_LOCK_ROW(SCN)     (((SCN)&SM_SCN_LOCK_ROW) == SM_SCN_LOCK_ROW )
#define SM_SCN_IS_NOT_LOCK_ROW(SCN) (((SCN)&SM_SCN_LOCK_ROW) != SM_SCN_LOCK_ROW )

#define SM_SCN_IS_FREE_ROW(SCN) ((SCN) == SM_SCN_FREE_ROW)
#define SM_SCN_IS_NULL_ROW(SCN) ((SCN) == SM_SCN_NULL_ROW)

#define SM_SCN_IS_INIT(SCN)      ((SCN) == SM_SCN_INIT)
#define SM_SCN_IS_NOT_INIT(SCN)  ((SCN) != SM_SCN_INIT)
#define SM_SCN_IS_MAX(SCN)       ((SCN) == SM_SCN_MAX)

#define SM_SCN_IS_DELETED( SCN )                                              \
        ( SM_SCN_IS_INFINITE( SCN ) ?                                         \
          ( ( ( SCN ) & SM_SCN_INF_DELETE_BIT ) == SM_SCN_INF_DELETE_BIT )    \
        : ( ( ( SCN ) & SM_SCN_DELETE_BIT ) == SM_SCN_DELETE_BIT ) )

#define SM_SCN_IS_NOT_DELETED( SCN )                                          \
        ( SM_SCN_IS_INFINITE( SCN ) ?                                         \
          ( ( ( SCN ) & SM_SCN_INF_DELETE_BIT ) != SM_SCN_INF_DELETE_BIT )    \
        : ( ( ( SCN ) & SM_SCN_DELETE_BIT ) != SM_SCN_DELETE_BIT ) )

#define SM_SCN_IS_GE( SCN1, SCN2 )  ( *(smSCN *)(SCN1) >= *(smSCN *)(SCN2) )
#define SM_SCN_IS_GT( SCN1, SCN2 )  ( *(smSCN *)(SCN1) >  *(smSCN *)(SCN2) )
#define SM_SCN_IS_EQ( SCN1, SCN2 )  ( *(smSCN *)(SCN1) == *(smSCN *)(SCN2) )
#define SM_SCN_IS_LT( SCN1, SCN2 )  ( *(smSCN *)(SCN1) <  *(smSCN *)(SCN2) )
#define SM_SCN_IS_LE( SCN1, SCN2 )  ( *(smSCN *)(SCN1) <= *(smSCN *)(SCN2) )

#define SM_SET_SCN_CONST(SCN1, SCN2) ( (*(smSCN *)(SCN1)) = SCN2 )





/* ----------------------------------------------------------------------------
 *                Definition of SCN for 32bit
 * --------------------------------------------------------------------------*/
#else

/* PROJ-2162 Restart Risk Reduction
 * 64/32 ������� ULong ������ ��ȯ�Ͽ� �д� ��ũ�� */
# define SM_SCN_TO_LONG( src )             (( ((ULong)((smSCN)src).mHigh) << 32 ) + \
                                           ( ((smSCN)src).mLow ))

# define SM_GET_SCN( dest, src ) {                                        \
    ID_SERIAL_BEGIN(((smSCN *)(dest))->mHigh = ((smSCN *)(src))->mHigh);  \
    ID_SERIAL_END(((smSCN *)(dest))->mLow = ((smSCN *)(src))->mLow);      \
}

# define SM_SET_SCN( dest, src ) {                                        \
    ID_SERIAL_BEGIN(((smSCN *)(dest))->mLow = ((smSCN *)(src))->mLow);    \
    ID_SERIAL_END(((smSCN *)(dest))->mHigh = ((smSCN *)(src))->mHigh);    \
}

/* BUG-17603
   SM_INCREASE_SCN����...
   mLow�� UInt�̰� 8�� UInt�̱� ������ mLow + 8�� UInt�̴�.
   ���� mLow�� overflow ������ mLow + 8 > mLow�� �˻��� �� �ִ�.

   ������ SM_ADD_SCN������
   increment�� ���� Ÿ���� ���� �𸣴� ��Ȳ����
   mLow + increment > mLow�� overflow �˻��ϴ� ���� �߸��� ����̴�.
   �̶� increment�� ULong���� ĳ������ ��, ID_UINT_MAX�� �Ѿ����
   �˻��ؾ� �Ѵ�.
*/

# define SM_INCREASE_SCN( scn )                                    \
    if( ((smSCN *)(scn))->mLow + 8 > ((smSCN *)(scn))->mLow )      \
    {                                                              \
        ((smSCN *)scn)->mLow = ((smSCN *)scn)->mLow + 8;           \
    }                                                              \
    else                                                           \
    {                                                              \
        ID_SERIAL_BEGIN(((smSCN *)(scn))->mLow = ((smSCN *)(scn))->mLow + 15);\
        ID_SERIAL_END(((smSCN *)(scn))->mHigh = ((smSCN *)(scn))->mHigh + 0x80000001);\
    }

# define SM_ADD_SCN( scn, increment )                            \
    if( ((smSCN *)scn)->mLow + (ULong)increment <= 0xFFFFFFFF)   \
    {                                                            \
        ((smSCN *)scn)->mLow = ((smSCN *)scn)->mLow + increment; \
    }                                                            \
    else                                                         \
    {                                                            \
        ID_SERIAL_BEGIN(((smSCN *)(scn))->mLow = ((smSCN *)(scn))->mLow + increment); \
        ID_SERIAL_END(((smSCN *)(scn))->mHigh = ((smSCN *)(scn))->mHigh + 1); \
    }

# define SM_GET_SCN_LOWVBIT(src)                         \
         (((smSCN *)(src))->mLow & 0x00000001)
# define SM_GET_SCN_HIGHVBIT(src)                        \
         (((smSCN *)(src))->mHigh & 0x80000000)

#define SM_SCN_HIGH_INFINITE           (0x80000000)
#define SM_SCN_LOW_INFINITE            (0x00000000)
#define SM_SCN_INFINITE                (ID_ULONG( 0x8000000000000000 ))
#define SM_SCN_INF_INC                 (0x00000002)

#define SM_SCN_HIGH_MAX                (0x7FFFFFFF)
#define SM_SCN_LOW_MAX                 (0xFFFFFFE8)
 
#define SM_SCN_HIGH_NULL_ROW           (0x7FFFFFFF)
#define SM_SCN_LOW_NULL_ROW            (0xFFFFFFF0)

#define SM_SCN_HIGH_FREE_ROW           (0x7FFFFFFF)
#define SM_SCN_LOW_FREE_ROW            (0xFFFFFFF9)

#define SM_SCN_LOCK_ROW                (0xFFFFFFFF)

#define SM_SCN_HIGH_INIT               (0x00000000)
#define SM_SCN_LOW_INIT                (0x00000000)

#define SM_SCN_VIEW_BIT                (0x00000007)
#define SM_SCN_HIGH_DELETE_BIT         (0x00000000)
#define SM_SCN_LOW_DELETE_BIT          (0x00000001)
#define SM_SCN_COMMIT_PARITY_BIT       (0x00000002)
#define SM_SCN_COMMIT_LEGACY_BIT       (0x00000004)

#define SM_UNSET_SCN_LEGACY_BIT(scn)                                        \
    ID_SERIAL_BEGIN(((smSCN *)(scn))->mHigh &= 0x7FFFFFFF);                 \
    ID_SERIAL_END(((smSCN *)(scn))->mLow &= ~SM_SCN_COMMIT_LEGACY_BIT);

#define SM_SET_SCN_LEGACY_BIT(scn)                                          \
    ID_SERIAL_BEGIN(((smSCN *)(scn))->mHigh |= 0x00000000);                 \
    ID_SERIAL_END(((smSCN *)(scn))->mLow |= SM_SCN_COMMIT_LEGACY_BIT);

#define SM_CLEAR_SCN_VIEW_BIT(scn)                                          \
        ((smSCN *)(scn))->mLow &= ~SM_SCN_VIEW_BIT;                         
#define SM_SET_SCN_VIEW_BIT(scn)                                            \
        ((smSCN *)(scn))->mLow |= SM_SCN_VIEW_BIT;                         

#define SM_SCN_IS_VIEWSCN(scn)                                              \
        ( (((scn).mHigh & SM_SCN_HIGH_INFINITE) != SM_SCN_HIGH_INFINITE) && \
          (((scn).mLow & SM_SCN_VIEW_BIT) == SM_SCN_VIEW_BIT) )

#define SM_SCN_IS_SYSTEMSCN(scn)                                            \
        ( (((scn).mHigh & SM_SCN_HIGH_INFINITE) != SM_SCN_HIGH_INFINITE) && \
          (((scn).mLow & SM_SCN_VIEW_BIT) == 0x00) )

#define SM_SCN_IS_COMMITSCN(scn)                                            \
        ((((scn).mHigh & SM_SCN_HIGH_INFINITE) != SM_SCN_HIGH_INFINITE) &&  \
         (((scn).mLow & SM_SCN_COMMIT_PARITY_BIT) != SM_SCN_COMMIT_PARITY_BIT))


// << PROJ-1591
#define SM_SCN_HIGH_CI_INFINITE       (0xFFFFFFFF)
#define SM_SCN_LOW_CI_INFINITE        (0xFFFFFFFF)

#define SM_SCN_HIGH_CI_INIT           (0x80000000)
#define SM_SCN_LOW_CI_INIT            (0x00000000)

#define SM_SCN_HIGH_CI_MAX            (0xFFFFFFFF)
#define SM_SCN_LOW_CI_MAX             (0xFFFFFFFE)


#define SM_INIT_CI_SCN( SCN ) {                                      \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mHigh =  SM_SCN_HIGH_CI_INIT); \
    ID_SERIAL_END(((smSCN *)(SCN))->mLow  =  SM_SCN_LOW_CI_INIT);    \
}

#define SM_MAX_CI_SCN( SCN ) {                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mHigh =  SM_SCN_HIGH_CI_MAX);  \
    ID_SERIAL_END(((smSCN *)(SCN))->mLow  =  SM_SCN_LOW_CI_MAX);     \
}

#define SM_SET_SCN_CI_INFINITE(SCN)                       \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow  = SM_SCN_LOW_CI_INFINITE);\
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_HIGH_CI_INFINITE);\
}

#define SM_SCN_IS_CI_INFINITE( SCN ) \
    ( (((SCN).mHigh) == SM_SCN_HIGH_CI_INFINITE) &&    \
      (((SCN).mLow)  == SM_SCN_LOW_CI_INFINITE) )

#define SM_SCN_IS_NOT_CI_INFINITE( SCN )  \
    ( (((SCN).mHigh) != SM_SCN_HIGH_CI_INFINITE) && \
      (((SCN).mLow)  != SM_SCN_LOW_CI_INFINITE) )

#define SM_SCN_IS_CI_INIT(SCN)  (((SCN).mHigh) == SM_SCN_HIGH_CI_INIT && \
                                 ((SCN).mLow)  == SM_SCN_LOW_CI_INIT)

#define SM_SCN_IS_CI_MAX(SCN)   (((SCN).mHigh) == SM_SCN_HIGH_CI_MAX && \
                                 ((SCN).mLow)  == SM_SCN_LOW_CI_MAX)
// >>  PROJ-1591 



# define SM_INIT_SCN( SCN ) {                                     \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mHigh =  SM_SCN_HIGH_INIT); \
    ID_SERIAL_END(((smSCN *)(SCN))->mLow  =  SM_SCN_LOW_INIT);    \
}
# define SM_MAX_SCN( SCN ) {                                      \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mHigh =  SM_SCN_HIGH_MAX);  \
    ID_SERIAL_END(((smSCN *)(SCN))->mLow  =  SM_SCN_LOW_MAX);     \
}
#define SM_SCN_IS_INFINITE( SCN )      ( ( (SCN).mHigh ) >= SM_SCN_HIGH_INFINITE )

#define SM_SCN_IS_NOT_INFINITE( SCN )  ( ( (SCN).mHigh ) <  SM_SCN_HIGH_INFINITE )

#define SM_SCN_IS_FREE_ROW(SCN)  (((SCN).mHigh) == SM_SCN_HIGH_FREE_ROW && \
                                  ((SCN).mLow) ==  SM_SCN_LOW_FREE_ROW )

#define SM_SCN_IS_LOCK_ROW(SCN)  (((SCN).mHigh) == SM_SCN_LOCK_ROW)

#define SM_SCN_IS_NULL_ROW(SCN)  (((SCN).mHigh) == SM_SCN_HIGH_NULL_ROW && \
                                  ((SCN).mLow)  == SM_SCN_LOW_NULL_ROW)

#define SM_SCN_IS_INIT(SCN)      (((SCN).mHigh) == SM_SCN_HIGH_INIT && \
                                  ((SCN).mLow)  == SM_SCN_LOW_INIT)
#define SM_SCN_IS_NOT_INIT(SCN)   (((SCN).mHigh) != SM_SCN_HIGH_INIT || \
                                  ((SCN).mLow)   != SM_SCN_LOW_INIT)
#define SM_SCN_IS_MAX(SCN)       (((SCN).mHigh) == SM_SCN_HIGH_MAX && \
                                  ((SCN).mLow)  == SM_SCN_LOW_MAX)

/* Infinite SCN�̸� mHigh�� delete bit�� �����ϰ�,
 * Infinite SCN�� �ƴϸ� mLow�� delete bit�� �����Ѵ�.
 * �ֳ��ϸ� Inifinte SCN�̸� mLow�� TID�� �����ϱ� �����̴�. */
#define SM_SCN_IS_DELETED( SCN )                                              \
        ( SM_SCN_IS_INFINITE( SCN ) ?                                         \
          ( ((SCN).mHigh & SM_SCN_LOW_DELETE_BIT) == SM_SCN_LOW_DELETE_BIT )  \
        : ( ((SCN).mLow & SM_SCN_LOW_DELETE_BIT) == SM_SCN_LOW_DELETE_BIT ) )

#define SM_SCN_IS_NOT_DELETED( SCN )                                          \
        ( SM_SCN_IS_INFINITE( SCN ) ?                                         \
          ( ((SCN).mHigh & SM_SCN_LOW_DELETE_BIT) != SM_SCN_LOW_DELETE_BIT )  \
        : ( ((SCN).mLow & SM_SCN_LOW_DELETE_BIT) != SM_SCN_LOW_DELETE_BIT ) )

# define SM_ADD_INF_SCN( scn )                                         \
        ((smSCN *)scn)->mHigh = ((smSCN *)scn)->mHigh + SM_SCN_INF_INC;

# define SM_SET_SCN_INFINITE(SCN)                       \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow  = SM_SCN_LOW_INFINITE);\
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_HIGH_INFINITE);\
}

# define SM_SET_SCN_INFINITE_AND_TID(SCN, TID)          \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow  = TID);\
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_HIGH_INFINITE);\
}

# define SM_SET_SCN_LOCK_ROW(SCN,TID)                   \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow = TID );     \
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_LOCK_ROW);\
}


# define SM_SET_SCN_DELETE_BIT(SCN)                       \
{                                                         \
    if( SM_SCN_IS_INFINITE( *((smSCN*)(SCN)) ) )          \
    {                                                     \
        ((smSCN *)(SCN))->mHigh |= SM_SCN_LOW_DELETE_BIT; \
    }                                                     \
    else                                                  \
    {                                                     \
        ((smSCN *)(SCN))->mLow |= SM_SCN_LOW_DELETE_BIT;  \
    }                                                     \
}

# define SM_CLEAR_SCN_DELETE_BIT(SCN)                      \
{                                                          \
    if( SM_SCN_IS_INFINITE( *((smSCN*)(SCN)) ) )           \
    {                                                      \
        ((smSCN *)(SCN))->mHigh &= ~SM_SCN_LOW_DELETE_BIT; \
    }                                                      \
    else                                                   \
    {                                                      \
        ((smSCN *)(SCN))->mLow  &= ~SM_SCN_LOW_DELETE_BIT; \
    }                                                      \
}

# define SM_GET_TID_FROM_INF_SCN(SCN)                                   \
    (SM_SCN_IS_INFINITE((SCN))?(SCN).mLow:SM_NULL_TID)
    

# define SM_SET_SCN_NULL_ROW(SCN)                       \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow = SM_SCN_LOW_NULL_ROW);     \
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_HIGH_NULL_ROW);      \
}

# define SM_SET_SCN_FREE_ROW(SCN)                   \
{                                                       \
    ID_SERIAL_BEGIN(((smSCN *)(SCN))->mLow = SM_SCN_LOW_FREE_ROW); \
    ID_SERIAL_END(((smSCN *)(SCN))->mHigh = SM_SCN_HIGH_FREE_ROW);  \
}

#define SM_SCN_IS_GT( SCN1, SCN2 )                                          \
        (((((smSCN *)(SCN1))->mHigh ) > (((smSCN *)(SCN2))->mHigh ) )  ||   \
        (((((smSCN *)(SCN1))->mHigh ) == (((smSCN *)(SCN2))->mHigh ) )  &&  \
         ((((smSCN *)(SCN1))->mLow ) > (((smSCN *)(SCN2))->mLow) )))

#define SM_SCN_IS_GE( SCN1, SCN2 )                                          \
        (((((smSCN *)(SCN1))->mHigh ) > (((smSCN *)(SCN2))->mHigh ) )  ||   \
        (((((smSCN *)(SCN1))->mHigh ) == (((smSCN *)(SCN2))->mHigh ) )  &&  \
         ((((smSCN *)(SCN1))->mLow ) >= (((smSCN *)(SCN2))->mLow))))

#define SM_SCN_IS_EQ( SCN1, SCN2 )                                          \
        (((((smSCN *)(SCN1))->mHigh ) == (((smSCN *)(SCN2))->mHigh ) )  &&  \
         ((((smSCN *)(SCN1))->mLow ) == (((smSCN *)(SCN2))->mLow) ) )

#define SM_SCN_IS_LT( SCN1, SCN2 )                                        \
        (((((smSCN *)(SCN1))->mHigh ) < (((smSCN *)(SCN2))->mHigh ) )  ||   \
        (((((smSCN *)(SCN1))->mHigh ) == (((smSCN *)(SCN2))->mHigh ) )  &&  \
         ((((smSCN *)(SCN1))->mLow ) < (((smSCN *)(SCN2))->mLow))))

#define SM_SCN_IS_LE( SCN1, SCN2 )                                          \
        (((((smSCN *)(SCN1))->mHigh ) < (((smSCN *)(SCN2))->mHigh ) )  ||   \
        (((((smSCN *)(SCN1))->mHigh ) == (((smSCN *)(SCN2))->mHigh ) )  &&  \
         ((((smSCN *)(SCN1))->mLow ) <= (((smSCN *)(SCN2))->mLow))))

#define SM_SET_SCN_CONST(SCN1, SCN2)                                      \
        ((smSCN*)(SCN1))->mHigh = ((ID_ULONG(0xFFFFFFFF00000000) & (SCN2)) >> 32);             \
        ((smSCN*)(SCN1))->mLow =  (ID_ULONG(0x00000000FFFFFFFF) & (SCN2));

/* ----------------------------------------------------------------------------
 *                End of Definition of SCN
 * --------------------------------------------------------------------------*/
#endif

/* PROJ-2734 �л굥��� */

/* BUG-48109 : GLOBAL TX LEVEL 1,2�� ��� �л������� FIRST STMT SCN ���� �̰����� �����ȴ�. */
#define SM_SCN_NON_GCTX_TX_FIRST_STMT_SCN (ID_ULONG( 0x0000000000000001 )) /* ULSD_NON_GCTX_TX_FIRST_STMT_SCN */

#define SM_SET_NON_GCTX_TX_FIRST_STMT_SCN( SCN )  \
        *((smSCN *)(SCN)) = SM_SCN_NON_GCTX_TX_FIRST_STMT_SCN;

#define SM_SCN_IS_NON_GCTX_TX_FIRST_STMT_SCN(SCN)     \
        ((SCN) == SM_SCN_NON_GCTX_TX_FIRST_STMT_SCN)
#define SM_SCN_IS_NOT_NON_GCTX_TX_FIRST_STMT_SCN(SCN) \
        ((SCN) != SM_SCN_NON_GCTX_TX_FIRST_STMT_SCN)


/* ----------------------------------------------------------------------------
 *   << SM_OID >>
 *
 * --------------------------------------------------------------------------*/
//# define SMX_SLOT_MASK (0x000003FF)

/**********************************************************
 * OID Action
 *********************************************************/
/* PROJ-2429 Dictionary based data compress for on-disk DB */
# define SM_OID_ACT_COMPRESSION                (0x00100000)

# define SM_OID_ACT_SAVEPOINT                  (0x00080000)
/*commit�Ǵ� ������ row�� ���� ��� ó���� ��. ���� ������ ���������� ����.
 *���� ���, insert ������ ��쿡,
 *Ʈ������� �۾��� �����ϴ� ���ȿ� ������ infiniteSCN��
 *Ʈ������� commit�Ҷ�, commitSCN���� �����ϴµ� ���� �Ѵ�.
 */
# define SM_OID_ACT_COMMIT                     (0x00040000)
/* COMMIT���� ���� delete thread�� ���ؼ� ����(AGING)�� �ȴ�. */
# define SM_OID_ACT_AGING_COMMIT               (0x00020000)
/* Rollback���� ���� delete thread�� ���ؼ� ����(AGING)�� �ȴ�. */
# define SM_OID_ACT_AGING_ROLLBACK             (0x00010000)
# define SM_OID_ACT_AGING_INDEX                (0x00008000)
# define SM_OID_ACT_CURSOR_INDEX               (0x00004000)

/**********************************************************
 * OID Type
 *********************************************************/
# define SM_OID_TYPE_MASK                      (0x00000F00)
# define SM_OID_TYPE_INSERT_FIXED_SLOT         (0x00000000)
# define SM_OID_TYPE_UPDATE_FIXED_SLOT         (0x00000100)
# define SM_OID_TYPE_DELETE_FIXED_SLOT         (0x00000300)
# define SM_OID_TYPE_VARIABLE_SLOT             (0x00000400)
# define SM_OID_TYPE_DROP_TABLE                (0x00000500)
# define SM_OID_TYPE_DROP_INDEX                (0x00000600)
# define SM_OID_TYPE_DELETE_TABLE_BACKUP       (0x00000700)
# define SM_OID_TYPE_FREE_FIXED_SLOT           (0x00000800)
# define SM_OID_TYPE_FREE_VAR_SLOT             (0x00000900)
# define SM_OID_TYPE_UNLOCK_FIXED_SLOT         (0x00000C00)
/* LPCH(LOB page control header)�� Free�Ҷ� ���*/
# define SM_OID_TYPE_FREE_LPCH                 (0x00000A00)
# define SM_OID_TYPE_NONE                      (0x00000B00)

/**********************************************************
 * OID Operation
 *********************************************************/
# define SM_OID_OP_MASK                        (0x000000FF)
# define SM_OID_OP_OLD_FIXED_SLOT              (0x00000000)
# define SM_OID_OP_CHANGED_FIXED_SLOT          (0x00000001)
# define SM_OID_OP_NEW_FIXED_SLOT              (0x00000002)
# define SM_OID_OP_DELETED_SLOT                (0x00000003)
# define SM_OID_OP_LOCK_FIXED_SLOT             (0x00000004)
# define SM_OID_OP_UNLOCK_FIXED_SLOT           (0x00000005)
# define SM_OID_OP_OLD_VARIABLE_SLOT           (0x00000006)
# define SM_OID_OP_NEW_VARIABLE_SLOT           (0x00000007)
# define SM_OID_OP_DROP_TABLE                  (0x00000008)
# define SM_OID_OP_DROP_TABLE_BY_ABORT         (0x00000009)
# define SM_OID_OP_DELETE_TABLE_BACKUP         (0x0000000A)
# define SM_OID_OP_DROP_INDEX                  (0x0000000B)
# define SM_OID_OP_UPATE_FIXED_SLOT            (0x0000000C)
# define SM_OID_OP_DDL_TABLE                   (0x0000000D)
/* BUG-27742 Partial Rollback�� LPCH�� �ι� Free�Ǵ� ���� */
# define SM_OID_OP_FREE_OLD_LPCH               (0x0000000E)
# define SM_OID_OP_FREE_NEW_LPCH               (0x0000000F)
# define SM_OID_OP_ALL_INDEX_DISABLE           (0x00000010)
# define SM_OID_OP_COUNT                       (0x00000011)
# define SM_OID_OP_CREATE_TABLE                (0x00000012)


/**********************************************************
 * OID Set
 *********************************************************/
# define SM_OID_NULL                  ( 0x00000000 )
# define SM_OID_OLD_UPDATE_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                         SM_OID_ACT_COMMIT             | \
                                        SM_OID_ACT_AGING_COMMIT       | \
                                        SM_OID_ACT_AGING_INDEX        | \
                                        SM_OID_TYPE_UPDATE_FIXED_SLOT | \
                                        SM_OID_OP_OLD_FIXED_SLOT        )

# define SM_OID_DELETE_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                    SM_OID_ACT_COMMIT             | \
                                    SM_OID_ACT_AGING_COMMIT       | \
                                    SM_OID_ACT_AGING_INDEX        | \
                                    SM_OID_TYPE_DELETE_FIXED_SLOT | \
                                    SM_OID_OP_DELETED_SLOT)

# define SM_OID_CHANGED_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                     SM_OID_ACT_COMMIT             | \
                                     SM_OID_ACT_CURSOR_INDEX       | \
                                     SM_OID_TYPE_UPDATE_FIXED_SLOT | \
                                     SM_OID_OP_CHANGED_FIXED_SLOT    )

# define SM_OID_NEW_INSERT_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                        SM_OID_ACT_COMMIT             | \
                                        SM_OID_ACT_AGING_ROLLBACK     | \
                                        SM_OID_ACT_CURSOR_INDEX       | \
                                        SM_OID_TYPE_INSERT_FIXED_SLOT | \
                                        SM_OID_OP_NEW_FIXED_SLOT        )

# define SM_OID_NEW_NULLROW_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT           |  \
                                         SM_OID_ACT_AGING_ROLLBACK      |  \
                                         SM_OID_TYPE_INSERT_FIXED_SLOT  |  \
                                         SM_OID_OP_NEW_FIXED_SLOT     )

# define SM_OID_NEW_UPDATE_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                        SM_OID_ACT_COMMIT             | \
                                        SM_OID_ACT_AGING_ROLLBACK     | \
                                        SM_OID_ACT_CURSOR_INDEX       | \
                                        SM_OID_TYPE_UPDATE_FIXED_SLOT | \
                                        SM_OID_OP_NEW_FIXED_SLOT        )

# define SM_OID_LOCK_FIXED_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                  SM_OID_ACT_COMMIT             | \
                                  SM_OID_OP_UNLOCK_FIXED_SLOT     )

# define SM_OID_OLD_VARIABLE_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                    SM_OID_ACT_AGING_COMMIT       | \
                                    SM_OID_TYPE_VARIABLE_SLOT     | \
                                    SM_OID_OP_OLD_VARIABLE_SLOT     )

# define SM_OID_NEW_VARIABLE_SLOT ( SM_OID_ACT_SAVEPOINT          | \
                                    SM_OID_ACT_AGING_ROLLBACK     | \
                                    SM_OID_TYPE_VARIABLE_SLOT     | \
                                    SM_OID_OP_NEW_VARIABLE_SLOT     )

# define SM_OID_CREATE_TABLE        ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_ACT_COMMIT             | \
                                      SM_OID_OP_CREATE_TABLE        )

# define SM_OID_DROP_TABLE          ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_ACT_COMMIT             | \
                                      SM_OID_ACT_AGING_COMMIT       | \
                                      SM_OID_TYPE_DROP_TABLE        | \
                                      SM_OID_OP_DROP_TABLE            )

# define SM_OID_DDL_TABLE           ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_ACT_COMMIT             | \
                                      SM_OID_OP_DDL_TABLE           )

# define SM_OID_DROP_TABLE_BY_ABORT ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_ACT_COMMIT             | \
                                      SM_OID_ACT_AGING_ROLLBACK     | \
                                      SM_OID_TYPE_DROP_TABLE        | \
                                      SM_OID_OP_DROP_TABLE_BY_ABORT   )

# define SM_OID_DELETE_ALTER_TABLE_BACKUP ( \
                                       SM_OID_ACT_AGING_COMMIT         | \
                                       SM_OID_ACT_AGING_ROLLBACK       | \
                                       SM_OID_TYPE_DELETE_TABLE_BACKUP | \
                                       SM_OID_OP_DELETE_TABLE_BACKUP)

# define SM_OID_DROP_INDEX          ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_ACT_COMMIT             | \
                                      SM_OID_ACT_AGING_COMMIT       | \
                                      SM_OID_TYPE_DROP_INDEX        | \
                                      SM_OID_OP_DROP_INDEX            )

# define SM_OID_UPDATE_FIXED_SLOT   ( SM_OID_ACT_SAVEPOINT          | \
                                      SM_OID_OP_UPATE_FIXED_SLOT    | \
                                      SM_OID_ACT_CURSOR_INDEX       )

# define SM_OID_OLD_LPCH   ( SM_OID_ACT_SAVEPOINT    | \
                             SM_OID_ACT_AGING_COMMIT | \
                             SM_OID_TYPE_FREE_LPCH | \
                             SM_OID_OP_FREE_OLD_LPCH )

# define SM_OID_NEW_LPCH   ( SM_OID_ACT_SAVEPOINT      | \
                             SM_OID_ACT_AGING_ROLLBACK | \
                             SM_OID_TYPE_FREE_LPCH | \
                             SM_OID_OP_FREE_NEW_LPCH )

# define SM_OID_ALL_INDEX_DISABLE ( SM_OID_ACT_SAVEPOINT          | \
                                    SM_OID_ACT_COMMIT             | \
                                    SM_OID_OP_ALL_INDEX_DISABLE   )

/* BUG-42724 */
# define SM_OID_XA_INSERT_UPDATE_ROLLBACK ( \
                                    SM_OID_ACT_SAVEPOINT          | \
                                    SM_OID_ACT_COMMIT             | \
                                    SM_OID_ACT_AGING_ROLLBACK     | \
                                    SM_OID_ACT_CURSOR_INDEX       | \
                                    SM_OID_OP_NEW_FIXED_SLOT      )

/* ----------------------------------------------------------------------------
 *   BUGBUG: VC In-Out Mode Test�� ���� ���� By Newdaily
 *
 * --------------------------------------------------------------------------*/
#define SM_COLUMN_INOUT_BASESIZE (4) // Byte





/* ----------------------------------------------------------------------------
 *   BUG-20805
 *   D$XXX
 * --------------------------------------------------------------------------*/
#define SM_DUMP_VALUE_LENGTH                          (24)
#define SM_DUMP_VALUE_BUFFER_SIZE                   (8192)
#define SM_DUMP_VALUE_DATE_FMT     ("YYYY-MM-DD HH:MI:SS")





/* ----------------------------------------------------------------------------
 * ideLog::log()�� ���Ǵ� ���
 * --------------------------------------------------------------------------*/
#define SM_TRC_LOG_LEVEL_FORCE     IDE_SM_0
#define SM_TRC_LOG_LEVEL_STARTUP   IDE_SM_1
#define SM_TRC_LOG_LEVEL_SHUTDOWN  IDE_SM_2
#define SM_TRC_LOG_LEVEL_FATAL     IDE_SM_3
#define SM_TRC_LOG_LEVEL_ABORT     IDE_SM_4
#define SM_TRC_LOG_LEVEL_WARNNING  IDE_SM_5

#define SM_TRC_LOG_LEVEL_MEMORY    IDE_SM_6   // smm
#define SM_TRC_LOG_LEVEL_DISK      IDE_SM_7   // sdd
#define SM_TRC_LOG_LEVEL_BUFFER    IDE_SM_8   // sdb
#define SM_TRC_LOG_LEVEL_MRECOV    IDE_SM_9   // smr
#define SM_TRC_LOG_LEVEL_DRECOV    IDE_SM_10  // sdr
#define SM_TRC_LOG_LEVEL_MPAGE     IDE_SM_11  // smp
#define SM_TRC_LOG_LEVEL_DPAGE     IDE_SM_12  // sdp
#define SM_TRC_LOG_LEVEL_MRECORD   IDE_SM_13  // smc
#define SM_TRC_LOG_LEVEL_DRECORD   IDE_SM_14  // sdc
#define SM_TRC_LOG_LEVEL_MINDEX    IDE_SM_15  // smn
#define SM_TRC_LOG_LEVEL_DINDEX    IDE_SM_16  // sdn
#define SM_TRC_LOG_LEVEL_TRANS     IDE_SM_17  // smx
#define SM_TRC_LOG_LEVEL_LOCK      IDE_SM_18  // sml
#define SM_TRC_LOG_LEVEL_MAGER     IDE_SM_19  // sma
#define SM_TRC_LOG_LEVEL_DAGER     IDE_SM_20  // sda
#define SM_TRC_LOG_LEVEL_THREAD    IDE_SM_21  // smt
#define SM_TRC_LOG_LEVEL_INTERFACE IDE_SM_22  // smi
#define SM_TRC_LOG_LEVEL_UTIL      IDE_SM_23  // smu

#define SM_TRC_LOG_LEVEL_DEBUG     IDE_SM_32





typedef IDE_RC (*smSeekFunc)( void         * aIterator,
                              const void  ** aRow );

/* FOR A4 : smiCursorProperties �������� ���� ���� */
typedef IDE_RC (*smInitFunc)( void                * aIterator,
                              void                * aTrans,
                              void                * aTable,
                              void                * aIndex,
                              void                * aDumpObject, // PROJ-1618
                              const smiRange      * aRange,
                              const smiRange      * aKeyFilter,
                              const smiCallBack   * aRowFilter,
                              UInt                  aFlag,
                              smSCN                 aSCN,
                              smSCN                 aInfinite,
                              idBool                aUntouchable,
                              smiCursorProperties * aProperties,
                              const smSeekFunc   ** aSeekFunc,
                              smiStatement        * aStatement );

typedef IDE_RC (*smDestFunc)( void * aIterator );

/***************************************************
 * smcTable�� insert, update, remove�� ���� �Լ��� *
 ***************************************************/
typedef IDE_RC (*smTableCursorInsertFunc)( idvSQL         *aStatistics,
                                           void           *aTrans,
                                           void           *aTableInfo,
                                           void           *aTableHeader,
                                           smSCN           aSCN,
                                           SChar         **aRetRowPtr,
                                           scGRID         *aRetInsertSlotGRID,
                                           const smiValue *aValueList,
                                           UInt            aFlag );

typedef IDE_RC (*smTableCursorUpdateFunc)( idvSQL                * aStatistics,
                                           void*                   aTrans,
                                           smSCN                   aViewSCN,
                                           void                  * aTableInfo,
                                           void                  * aTableHeader,
                                           SChar                 * aOldRow,
                                           scGRID                  aSlotGRID,
                                           SChar                ** aRetRow,
                                           scGRID                * aRetUpdateSlotGRID,
                                           const smiColumnList   * aColumnList,
                                           const smiValue        * aValueList,
                                           const smiDMLRetryInfo * aRetryInfo,
                                           smSCN                   aSCN,
                                           void                  * aLobInfo4Update,
                                           ULong                   aModifyIdxBit,
                                           idBool                  aForbiddenToRetry );


// modified for A4, cluster index������ prototype����.
typedef IDE_RC (*smTableCursorRemoveFunc)( idvSQL                *aStatistics,
                                           void                  *aTrans,
                                           smSCN                  aViewSCN,
                                           void                  *aTableInfo,
                                           void                  *aTableHeader,
                                           SChar                 *aRow,
                                           scGRID                 aRowGRID,
                                           smSCN                  aSCN,
                                           const smiDMLRetryInfo *aRetryInfo,
                                           idBool                 aIsDequeue,
                                           idBool                 aForbiddenToRetry );

typedef IDE_RC (*smTableCursorLockRowFunc)( smiIterator *aIterator );

/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * dumpci���� util������ ����� �� �ְ� Assert�� ����� ���� �ִ�
 * PageDump �Լ� Ÿ�� �߰�. *
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
 * PageDump�� OutBuf�� �̿��Ͽ� ��ȯ�ϴ� ���·� �ٲߴϴ�.*/

typedef IDE_RC (*smPageDump)( UChar  * aPage, 
                              SChar  * aOutBuf, 
                              UInt     aOutSize );

typedef IDE_RC (*smManageDtxInfo) ( ID_XID * aXID,
                                    UInt     aLocalTxId,
                                    UInt     aGlobalTxId,
                                    UInt     aBranchTxSize,
                                    UChar  * aBranchTxInfo,
                                    smLSN  * aPrepareLSN,
                                    smSCN  * aGlobalCommitSCN,
                                    UChar    aType );
typedef smLSN (*smGetDtxMinLSN) ( void );
typedef UInt (*smGetDtxGlobalTxId) ( UInt aLocalTxId );

typedef void (*smTransApplyShardMetaChangeFunc)( void * aMmSession);
/*******************************************************************************
 * SM Utility Macros
 ******************************************************************************/
#define SM_IS_FLAG_ON(set, f)   ( (((set) & (f)) == (f)) ? ID_TRUE : ID_FALSE )
#define SM_IS_FLAG_OFF(set, f)  ( (((set) & (f)) == (f)) ? ID_FALSE : ID_TRUE )
#define SM_SET_FLAG_ON(set, f)  ( (set) |= (f) )
#define SM_SET_FLAG_OFF(set, f) ( (set) &= ~(f) )

/* PROJ-2232 log multiplex */
#define SM_ARCH_MULTIPLEX_PATH_CNT    (10)
#define SM_LOG_MULTIPLEX_PATH_MAX_CNT (10)

/* BUG-48513 PCH Array �� memory �� volatile �� �ϳ��� ��Ĩ�ϴ�. */
typedef struct smPCHBase // Page Control Header : PCH
{
    /* DML(insert, update, delete)������ �����ϴ� Transaction��
     * ��� Mutex */

    /* BUG-31569 [sm-mem-page] When executing full scan, hold page X Latch
     * in MMDB */
    iduLatch            mPageMemLatch;
 
    /* BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�. */
    scPageID            mNxtScanPID; 
    scPageID            mPrvScanPID;
    ULong               mModifySeqForScan;
    
    void               *mFreePageHeader; // PROJ-1490

 } smPCHBase;

typedef struct smPCSlot // Page Control Slot
{
    void      * mPagePtr;
    smPCHBase * mPCH;
} smPCSlot;

typedef struct smPCArr  // Page Control Slot Array
{
    smPCSlot  * mPC;
    ULong       mMaxPageCount;
} smPCArr;

#endif /* _O_SM_DEF_H_ */

