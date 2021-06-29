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
 * $Id: smiMisc.h 90444 2021-04-02 10:15:58Z minku.kang $
 **********************************************************************/

#ifndef _O_SMI_MISC_H_
# define _O_SMI_MISC_H_ 1

# include <idTypes.h>
# include <smiDef.h>
# include <smDef.h>
# include <smiLogRec.h>

#define SMI_MISC_TABLE_HEADER( aTable )                         \
        ((const smcTableHeader*)((const smpSlotHeader*)aTable+1))

/* PROJ-2433
 * direct key �ּұ��� */
#define SMI_MEM_INDEX_MIN_KEY_SIZE ( 8 )

/* FOR A4 : ������ smiMakeDatabaseFileName( )�̾��� ����
   Memory DB�� Disk DB�� �����ϱ� ���Ͽ� �Ʒ��� ���� ������
   Disk Table Space ���� �Լ����� smiTableSpace.cpp���� ���� */

/*---------------------------------------------*/
/* ������ ���̽� �⺻ ����                         */
/*---------------------------------------------*/

/* FOR A4 : Memory DB�� Disk DB�� �����ϱ� ���Ͽ� aTSType ���� �߰� */
UInt             smiGetPageSize( smiTableSpaceType aTSType );

/* Disk DB�� ���� �� ����� ���Ѵ�. */
ULong           smiGetDiskDBFullSize();

/* FOR A4 : sdbBufferMgr���� ����ϴ� buffer pool�� ������ ���� */
UInt             smiGetBufferPoolSize( );

// To Fix BUG-15361
// Database Name�� ȹ���Ѵ�.
SChar *          smiGetDBName( );

// PROJ-1579 NCHAR
// �����ͺ��̽� ĳ���� ���� ��´�.
SChar *          smiGetDBCharSet( );

// PROJ-1579 NCHAR
// ���ų� ĳ���� ���� ��´�.
SChar *          smiGetNationalCharSet( );

/*---------------------------------------------*/
/* ���̺� �⺻ �Լ�                               */
/*---------------------------------------------*/

const void*      smiGetCatalogTable( void );

/* FOR A4 : Memory Table�� Disk Table�� �����ϱ� ���Ͽ�
   aTableType ���� �߰� */
UInt             smiGetVariableColumnSize( UInt aTableType );
UInt             smiGetVariableColumnSize4DiskIndex();

UInt             smiGetVCDescInModeSize();

/* FOR A4 : Memory Table�� Disk Table�� �����ϱ� ���Ͽ�
   aTableType ���� �߰� */
UInt             smiGetRowHeaderSize( UInt aTableType );

/* FOR A4 : Memory Table�߿����� TableHandle�� ���ؼ��� ����ؾ� �� */
smSCN            smiGetRowSCN( const void* aRow );



/*---------------------------------------------*/
/* �ε��� Type ���� �Լ�                          */
/*---------------------------------------------*/

IDE_RC           smiFindIndexType( SChar* aName,
                                   UInt*  aType );

// BUG-17449
idBool           smiCanMakeIndex( UInt    aTableType,
                                  UInt    aIndexType );

const SChar*     smiGetIndexName( UInt aType );

idBool           smiGetIndexUnique( const void * aIndex );
    
/* FOR A4 : Memory Table�� Disk Table�� �����ϱ� ���Ͽ�
   aTableType ���� �߰� */
UInt             smiGetDefaultIndexType( void );

// �ε��� Ÿ���� Unique�� ���������� ����
idBool           smiCanUseUniqueIndex( UInt aIndexType );

// PROJ-1502 PARTITIONED DISK TABLE
// ��Һ� ������ �ε��� Ÿ������ üũ
idBool           smiGreaterLessValidIndexType( UInt aIndexType );

// PROJ-1704 MVCC Renewal
idBool           smiIsAgableIndex( const void * aIndex );

// To Fix BUG-16218
// �ε��� Ÿ���� Composite Index�� ���������� ����
idBool           smiCanUseCompositeIndex( UInt aIndexType );

/*---------------------------------------------*/
/* ���̺� ���� ���� �Լ�                          */
/*---------------------------------------------*/

const void *     smiGetTable( smOID aTableId );

smOID            smiGetTableId( const void* aTable );

UInt             smiGetTableColumnCount( const void* aTable );

UInt             smiGetTableColumnSize( const void* aTable );

const void *     smiGetTableIndexes( const void * aTable,
                                     const UInt   aIdx );

const void *     smiGetTableIndexByID( const void * aTable,
                                       const UInt   aIndexId );

const void *     smiGetTablePrimaryKeyIndex( const void * aTable );

UInt             smiGetTableIndexCount( const void* aTable );

UInt             smiGetTableIndexSize( void );

const void *     smiGetTableInfo( const void * aTable );

UInt             smiGetTableInfoSize( const void* aTable );

IDE_RC           smiGetTableTempInfo( const void    * aTable,
                                      void         ** aRuntimeInfo );

void             smiSetTableTempInfo( const void * aTable,
                                      void       * aTempInfo );

/* BUG-42928 No Partition Lock */
void             smiSetTableReplacementLock( const void * aDstTable,
                                             const void * aSrcTable );
/* BUG-42928 No Partition Lock */
void             smiInitTableReplacementLock( const void * aTable );

/* For A4 : modified for Disk Table */
IDE_RC           smiGetTableNullRow( const void   * aTable,
                                     void        ** aRow,
                                     scGRID       * aGRID );



UInt             smiGetTableFlag(const void* aTable);

// PROJ-1665
UInt             smiGetTableLoggingMode(const void * aTable);
UInt             smiGetTableParallelDegree(const void * aTable);
UInt             smiGetTableMaxRow(const void * aTable);

// FOR A4
idBool           smiIsDiskTable(const void* aTable);
idBool           smiIsMemTable(const void* aTable);
idBool           smiIsUserMemTable( const void* aTable );
idBool           smiIsVolTable(const void* aTable);

IDE_RC           smiGetTableBlockCount( const void * aTable,
                                        ULong      * aBlockCnt );

IDE_RC           smiGetTableSpaceID(const void * aTable );

/* BUG-28752 lock table ... in row share mode ������ ������ �ʽ��ϴ�.
 * implicit/explicit lock�� �����Ͽ� �̴ϴ�. 
 * explicit Lock�� �Ŵ� ���� QP�� qmx::executeLockTable���Դϴ�. */ 
IDE_RC           smiValidateAndLockObjects( smiTrans          * aTrans,
                                            const void*         aTable,
                                            smSCN               aSCN,
                                            smiTBSLockValidType aTBSLvType,
                                            smiTableLockMode    aLockMode,
                                            ULong               aLockWaitMicroSec,
                                            idBool              aIsExplcitLock );

IDE_RC           smiValidateObjects( const void        * aTable,
                                     smSCN               aSCN);

/* PROJ-2268 Reuse Catalog Table Slot 
 * Table Slot�� ��Ȱ��Ǿ����� Ȯ���Ѵ�. */
IDE_RC           smiValidatePlanHandle( const void * aHandle,
                                        smSCN        aCachedSCN );

// ���̺� �����̽��� ���� Lock�� ȹ���ϰ� Validation�� �����Ѵ�.
IDE_RC          smiValidateAndLockTBS( smiStatement      * aStmt,
                                      scSpaceID             aSpaceID,
                                      smiTBSLockMode        aLockMode,
                                      smiTBSLockValidType   aTBSLvType,
                                      ULong                 aLockWaitMicroSec );


IDE_RC           smiFetchRowFromGRID(
                                     idvSQL                      * aStatistics,
                                     smiStatement                * aStatement,
                                     UInt                          aTableType,
                                     scGRID                        aRowGRID,
                                     const smiFetchColumnList    * aFetchColumnList,
                                     void                        * aTableHdr,
                                     void                        * aDestRowBuf );

IDE_RC           smiGetGRIDFromRowPtr( const void   * aRowPtr,
                                       scGRID       * aRowGRID );


/*---------------------------------------------*/
/* �ε��� ���� ���� �Լ�                           */
/*---------------------------------------------*/


UInt             smiGetIndexId( const void* aIndex );

UInt             smiGetIndexType( const void* aIndex );


idBool           smiGetIndexRange( const void* aIndex );

idBool           smiGetIndexDimension( const void* aIndex );

const UInt*      smiGetIndexColumns( const void* aIndex );

const UInt*      smiGetIndexColumnFlags( const void* aIndex );

UInt             smiGetIndexColumnCount( const void* aIndex );

idBool           smiGetIndexBuiltIn( const void * aIndex );

UInt             smiEstimateMaxKeySize( UInt        aColumnCount,
                                        smiColumn * aColumns,
                                        UInt *      aMaxLengths );

UInt             smiGetIndexKeySizeLimit( UInt        aTableType,
                                          UInt        aIndexType );

/*---------------------------------------------*/
// For A4 : Variable Column Data retrieval
/*---------------------------------------------*/

const void* smiGetVarColumn( const void      * aRow,
                             const smiColumn * aColumn,
                             UInt            * aLength );

UInt smiGetVarColumnBufHeadSize( const smiColumn * aColumn );

// Variable Column Data Length retrieval
UInt smiGetVarColumnLength( const void*       aRow,
                            const smiColumn * aColumn );


/*---------------------------------------------*/
/* FOR A4 : Cursor ���� �Լ���                   */
/*---------------------------------------------*/

smiRange * smiGetDefaultKeyRange( );
    
smiCallBack * smiGetDefaultFilter( );


/* check point �����Լ� */
IDE_RC smiCheckPoint( idvSQL  * aStatistics,
                      idBool    aStart );

/* In-Doubt Ʈ������� ���� ��� */
IDE_RC smiXaRecover(SInt           *a_slotID, 
                    /* BUG-18981 */
                    ID_XID            *a_xid, 
                    timeval        *a_preparedTime,
                    smiCommitState *a_state); 

/*
 *  ���� Database�� selective loading ��� ���
 *  QP���� pin, unpin ������ ��� �˻��ؾ� ��.
 */
idBool isRuntimeSelectiveLoadingSupport();

/* ------------------------------------------------
 *  Status ������ ����ϱ� ����.
 * ----------------------------------------------*/

void smiGetTxLockInfo(smiTrans *aTrans, smTID *aOwnerList, UInt *aOwnerCount);


//  For A4 : TableSpace type���� Maximum fixed row size�� ��ȯ�Ѵ�.
//  slot header ����.
UInt smiGetMaxFixedRowSize(smiTableSpaceType aTblSpaceType );


// PROJ-1362
// LOB Column Piece�� ũ�⸦ ��ȯ�Ѵ�.
UInt smiGetLobColumnSize(UInt aTableType);

// LOB Column�� NULL(length == 0)���� Ȯ���Ѵ�.
idBool smiIsNullLobColumn(const void*       aRow,
                          const smiColumn * aColumn);


/*
  For A4 :
      TableSpace���� Database Size�� ��ȯ�Ѵ�.
*/
#if 0 //not used 
ULong    smiTBSSize( scSpaceID aTableSpaceID );
#endif

// Disk Segment�� �����ϱ� ���� �ּ� Extent ����
UInt smiGetMinExtPageCnt();

void smiSetEmergency(idBool aFlag);

void smiSwitchDDL(UInt aFlag);

UInt smiGetCurrTime();


/* ------------------------------------------------
 * interface for replication 
 * ----------------------------------------------*/
/* Callback Function�� Setting���ش�.*/
/* BUG-26482 ��� �Լ��� CommitLog ��� ���ķ� �и��Ͽ� ȣ���մϴ�. */
void smiSetCallbackFunction(
        smGetMinSN                   aGetMinSNFunc,
        smIsReplCompleteBeforeCommit aIsReplCompleteBeforeCommitFunc = NULL,
        smIsReplCompleteAfterCommit  aIsReplCompleteAfterCommitFunc  = NULL,
        smCopyToRPLogBuf             aCopyToRPLogBufFunc             = NULL,
        smSendXLog                   aSendXLogFunc                   = NULL,
        smIsReplWaitGlobalTxAfterPrepare aIsReplWaitGlobalTxAfterPrepare = NULL );

IDE_RC smiGetLstLSNByNoLatch(smLSN &aLSN);

void smiGetLstDeleteLogFileNo( UInt *aFileNo );
IDE_RC smiGetFirstNeedLFN( smSN         aMinLSN,
                           const UInt * aFirstFileNo,
                           const UInt * aEndFileNo,
                           UInt       * aNeedFirstFileNo );


// ���������� Log�� ����ϱ� ���� "����/�����" LSN���� �����Ѵ�.
IDE_RC smiGetLastUsedGSN( smSN *aSN );

/* BUG-43426 */
IDE_RC smiWaitAndGetLastValidGSN( smSN *aSN );

// ���������� ����� �α� ���ڵ��� LSN
IDE_RC smiGetLastValidGSN( smSN *aSN );
IDE_RC smiGetLastValidLSN( smLSN * aLSN );

// Transaction Table Size�� return�Ѵ�.
UInt smiGetTransTblSize();

// Archive Log ��θ� return �Ѵ�.
const SChar * smiGetArchiveDirPath();

// Archive Mode return �Ѵ�.
smiArchiveMode smiGetArchiveMode();


// SM�� �����ϴ� �ý����� ��������� �ݿ��Ѵ�.
// �� �Լ������� SM ������ �� ��⿡�� �ý��ۿ� �ݿ��Ǿ�� �ϴ�
// ��������� v$sysstat���� �ݿ��ϵ��� �ϸ� �ȴ�.
void smiApplyStatisticsForSystem();


/***********************************************************************

 Description :

 SM�� �� tablespace�� ������ �����Ѵ�.

 ALTER SYSTEM VERIFY �� ���� ���� SQL�� ����ڰ� ó���Ѵٸ�
 QP���� ���� �Ľ���  �� �Լ��� ȣ���Ѵ�.

 ����� SMI_VERIFY_TBS �� ����������, ���Ŀ� ����� �߰��� �� �ִ�.

 ȣ�� ��) smiVerifySM(SMI_VERIFY_TBS)

 **********************************************************************/
IDE_RC smiVerifySM( idvSQL*  aStatistics, UInt      aFlag );

/* BUG-35392 FastUnlockLogAllocMutex ����� ����ϴ��� ���� */
idBool smiIsFastUnlockLogAllocMutex();

//PROJ-1362 QP-Large Record & Internal LOB support
// table�� �÷������� �ε��� ���� ���� Ǯ��.
IDE_RC smiGetTableColumns( const void        * aTable,
                           const UInt          aIndex,
                           const smiColumn  ** aColumn );

/* alter system flush buffer_cache; */
IDE_RC smiFlushBufferPool( idvSQL * aStatistics );

IDE_RC smiFlushSBuffer( idvSQL * aStatistics );

IDE_RC smiFlusherOnOff(idvSQL *aStatistics, UInt aFlusherID, idBool aStart);

IDE_RC smiSBufferFlusherOnOff( idvSQL *aStatistics, 
                               UInt aFlusherID, 
                               idBool aStart );

IDE_RC smiUpdateIndexModule(void *aIndexModule);

/* PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
   DDL Statement Text�� �α�
 */
IDE_RC smiWriteDDLStmtTextLog( idvSQL*          aStatistics,
                               smiStatement   * aStmt,
                               smiDDLStmtMeta * aDDLStmtMeta,
                               SChar *          aStmtText,
                               SInt             aStmtTextLen );

/*PROJ-1608 recovery from replication*/
smSN smiGetReplRecoverySN();
IDE_RC smiSetReplRecoverySN( smSN aReplRecoverySN );
IDE_RC smiGetSyncedMinFirstLogSN( smSN *aSN );

void smiGetRemoveMaxLastLogSN( smSN   *aSN );

/* BUG-20727 */
IDE_RC smiExistPreparedTrans(idBool *aExistFlag);

/* PROJ-1915 */
ULong smiGetLogFileSize();
UInt  smiGetSmVersionID();

/* BUG-21800 */
SChar* smiGetPsmSvpName( void );

IDE_RC smiRebuildMinViewSCN( idvSQL * aStatistics );

/*
 * BUG-27949 [5.3.3] add datafile ����. DDL_LOCK_TIMEOUT �� 
 *           �����ϰ� �ֽ��ϴ�. (�ٷκ�������) 
 */
ULong smiGetDDLLockTimeOut(smiTrans * aTrans);

IDE_RC smiWriteDummyLog();

/*fix BUG-31514 While a dequeue rollback ,
  another dequeue statement which currenlty is waiting for enqueue event
  might lost the  event  */
void   smiGetLastSystemSCN( smSCN *aLastSystemSCN );
void   smiInaccurateGetLastSystemSCN( smSCN *aLastSystemSCN );
IDE_RC smiSetLastSystemSCN( smSCN * aLastSystemSCN, smSCN * aNewLastSystemSCN );

smSN  smiGetValidMinSNOfAllActiveTrans();

void smiInitDMLRetryInfo( smiDMLRetryInfo * aRetryInfo);

// PROJ-2264
const void * smiGetCompressionColumn( const void      * aRow,
                                      const smiColumn * aColumn,
                                      idBool            aUseColumnOffset,
                                      UInt            * aLength);
// PROJ-2397
void * smiGetCompressionColumnFromOID( smOID           * aCompressionRowOID,
                                       const smiColumn * aColumn,
                                       UInt            * aLength );

// PROJ-2375 Global Meta 
const void * smiGetCompressionColumnLight( const void * aRow,
                                           scSpaceID    aColSpace,
                                           UInt         aVcInOutBaseSize,
                                           UInt         aFlag,
                                           UInt       * aLength );
// PROJ-2264
void smiGetSmiColumnFromMemTableOID( smOID        aTableOID,
                                     UInt         aIndex,
                                     smiColumn ** aSmiColumn );

// PROJ-2264
idBool smiIsDictionaryTable( void * aTableHandle );

// PROJ-2264
void * smiGetTableRuntimeInfoFromTableOID( smOID aTableOID );

// PROJ-2402
IDE_RC smiPrepareForParallel( smiTrans * aTrans,
                              UInt     * aParallelReadGroupID );

idBool smiIsConsistentIndices( const void * aTable );

/* PROJ-2433 */
UInt smiGetMemIndexKeyMinSize();
UInt smiGetMemIndexKeyMaxSize();
idBool smiIsForceIndexDirectKey();

UInt smiForceIndexPersistenceMode(); /* BUG-41121 */

// PROJ-1969
IDE_RC smiGetLstLSN( smLSN      * aEndLSN );

/* PROJ-2462 Result Cache */
void smiGetTableModifyCount( const void   * aTable,
                             SLong        * aModifyCount );

IDE_RC smiWriteXaStartReqLog( ID_XID * aGlobalXID,
                              smTID    aTID,
                              smLSN  * aLSN );

IDE_RC smiWriteXaPrepareReqLog( ID_XID * aXID,
                                smTID    aTID,
                                UInt     aGlobalTxId,
                                UChar  * aBranchTxInfo,
                                UInt     aBranchTxInfoSize,
                                smLSN  * aLSN );

IDE_RC smiWriteXaEndLog( smTID    aTID,
                         UInt     aGlobalTxId );

/* BUG-48501 */
void smiSetGlobalTxId( smTID   aTID,
                       UInt    aGlobalTxId );

IDE_RC smiSet2PCCallbackFunction( smGetDtxMinLSN aGetDtxMinLSN,
                                  smManageDtxInfo aManageDtxInfo );

/* PROJ-2626 Memory DB �� Disk UNDO �� ���� �� ����� ���Ѵ�. */
void smiGetMemUsedSize( ULong * aMemSize );
IDE_RC smiGetDiskUndoUsedSize( ULong * aSize );

/* MM���� SM�� Callback�Լ��� �Ѱ��ֱ� ���� Callback ��� �Լ� */
IDE_RC smiSetSessionCallback( smiSessionCallback *aCallback );

/* PROJ-2733 Replica SCN ������ �������̽� */
void smiSetReplicaSCN( void );

void smiSetTransactionalDDLCallback( smiTransactionalDDLCallback * aTransactionalDDLCallback );

SInt smiGetDDLLockTimeOutProperty();

#endif /* _O_SMI_MISC_H_ */
