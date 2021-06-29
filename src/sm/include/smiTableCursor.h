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
 * $Id: smiTableCursor.h 89495 2020-12-14 05:19:22Z emlee $
 **********************************************************************/

#ifndef _O_SMI_TABLE_CURSOR_H_
# define _O_SMI_TABLE_CURSOR_H_ 1

# include <smiDef.h>
# include <smDef.h>
# include <smnDef.h>
# include <smlDef.h>
# include <sdcDef.h>
# include <smxTableInfoMgr.h>

/* FOR A4 : smiTableSpecificFuncs
 *    �� ����ü�� Disk Table�� Disk Temp Table,Memory Table����
 *    ���� �ٸ� ������ �ϴ�
 *    Ŀ�� �Լ����� �����Ͽ� �̸� �Լ������͸� ������ ���� ��������ν�
 *    ��Ÿ�Խÿ� Table Ÿ�Կ� ���� �񱳸� ������ ���̴�.
 *    ���⿡ ���� restart, insertRow, deleteRow �Լ���
 *    Memory Table�� Disk Table���� �����ϰ� �����Ѵ�.
 */
typedef struct tagTableSpecificFuncs
{
    IDE_RC (*openFunc)( smiTableCursor *     aCursor,
                        const void*          aTable,
                        smSCN                aSCN,
                        const smiRange*      aKeyRange,
                        const smiRange*      aKeyFilter,
                        const smiCallBack*   aRowFilter,
                        smlLockNode *        aCurLockNodePtr,
                        smlLockSlot *        aCurLockSlotPtr );

    IDE_RC (*closeFunc)( smiTableCursor * aCursor );

    IDE_RC (*insertRowFunc)( smiTableCursor  * aCursor,
                             const smiValue  * aValueList,
                             void           ** aRow,
                             scGRID          * aGRID );

    IDE_RC (*updateRowFunc)( smiTableCursor       * aCursor,
                             const smiValue       * aValueList,
                             const smiDMLRetryInfo* aRetryInfo );

    IDE_RC (*deleteRowFunc)( smiTableCursor       * aCursor,
                             const smiDMLRetryInfo* aRetryInfo );

    // PROJ-1509
    IDE_RC (*beforeFirstModifiedFunc )( smiTableCursor * aCursor,
                                        UInt             aFlag );

    IDE_RC (*readOldRowFunc )( smiTableCursor * aCursor,
                               const void    ** aRow,
                               scGRID         * aRowGRID );

    IDE_RC (*readNewRowFunc )( smiTableCursor * aCursor,
                               const void    ** aRow,
                               scGRID         * aRowGRID );

}smiTableSpecificFuncs;

typedef struct smiTableDMLFunc
{
    smTableCursorInsertFunc  mInsert;
    smTableCursorUpdateFunc  mUpdate;
    smTableCursorRemoveFunc  mRemove;
} smiTableDMLFunc;

typedef IDE_RC (*smiGetRemoteTableNullRowFunc)( smiTableCursor  * aCursor,
                                                void           ** aRow,
                                                scGRID          * aGRID );

/* Disk, Memory, Temp Table�� ���� ���� Interface�� �����
   ������ Table�� ���� �����ڸ� Function Pointer�� Cursor Open��
   �������ش�.*/

class smiTableCursor
{
    friend class smiTable;
    friend class smiStatement;

 public:
    /* For Statement */
    // �� Cursor�� ���� �ִ� Statement�� Cursor�� Linked List
    smiTableCursor*          mPrev;
    smiTableCursor*          mNext;

    // Cursor�� �о�� �� View�� �����ϴ� SCN
    smSCN                    mSCN;
    // Infinite
    smSCN                    mInfinite;
    smSCN                    mInfinite4DiskLob;
 
    //----------------------------------------
    // PROJ-1509
    // mFstUndoRecSID : cursor open ��, next undo record�� SID ����
    //
    // mCurUndoRecSID    : readOldRow() �Ǵ� readNewRow() ���� ��,
    //                     ���� undo record�� SID ����
    //
    // mLstUndoPID : cursor close ��, ������ undo record�� SID ����
    //----------------------------------------
    sdSID                       mFstUndoRecSID;
    sdSID                       mCurUndoRecSID;
    sdSID                       mLstUndoRecSID;

    sdRID                       mFstUndoExtRID;
    sdRID                       mCurUndoExtRID;
    sdRID                       mLstUndoExtRID;

    // Transaction�� All Cursor�� Double Linked List�� ����.
    smiTableCursor*          mAllPrev;
    smiTableCursor*          mAllNext;

 private:
    // Transaction Pointer & Flag Info
    smxTrans*                mTrans;
    UInt                     mTransFlag;

 public:
    // Cursor�� ���� �ִ� Statement
    smiStatement           * mStatement;
    // Cursor�� Open�� Table Header
    void*                    mTable;
    // PROJ-1618 Online Dump: DUMP TABLE�� Object
    void*                    mDumpObject;
    // Cursor�� Index�� �̿��� ��� Index Header Pointer�� ����Ŵ �ƴϸ� NULL
    void*                    mIndex;
    // Interator���� Memory �Ҵ� �� �ʱ�ȭ Routine�� ������ Index Module�� ����.
    void*                    mIndexModule;
    // Iterator Pointer
    smiIterator*             mIterator;

    /* BUG-43408, BUG-45368 */
    ULong                    mIteratorBuffer[ SMI_ITERATOR_SIZE / ID_SIZEOF(ULong) ];

    // Update�� Update�Ǵ� Column Pointer
    const smiColumnList*     mColumns;

    // SMI_LOCK_ ... + SMI_TRAVERSE ... + SMI_PREVIOUS_ ...
    UInt                     mFlag;
    // Table�� �ɷ��ִ� Lock Mode
    smlLockMode              mLockMode;
    // if mUntouchable == TRUE, ReadOnly, else Write.
    idBool                   mUntouchable;
    // if mUntouchable == FALSE and ���� Statement�� Cursor�߿� ����
    // Table�� ���� open�� Cursor�� ���� ���, mIsSoloCursor = ID_FALSE,
    // else mIsSoloCursor = ID_TRUE
    idBool                   mIsSoloCursor;
    // SMI_.._CURSOR (SELECT, INSERT, UPDATE, DELETE)
    UInt                     mCursorType;

    /* BUG-21866: Disk Table�� Insert�� Insert Undo Reco�� ������� ����.
     *
     * ID_FALSE�̸� Insert�� Undo Rec�� ������� �ʴ´�. Triger�� Foreign Key
     * �� ���� ���� Insert Undo Rec�� ���ʿ��ϴ�.
     * */
    idBool                   mNeedUndoRec;

    /* FOR A4 */
    smiCursorProperties      mOrgCursorProp;
    smiCursorProperties      mCursorProp;
    smiTableSpecificFuncs    mOps;

    idBool                   mIsDequeue;

    //----------------------------------------
    // PROJ-1509
    // mFstOidNode  : Ʈ������� oid list �߿��� cursor open ��� ������
    //                oid node
    // mFstOidCount : Ʈ������� oid list �߿��� cursor open ��� ������
    //                oid node�� ������ oid record ��ġ
    // mCurOidNode  : readOldRow() �Ǵ� readNewRow() ���� ��,
    //                ���� oid node ��ġ
    // mCurOidIndex : readOldRow() �Ǵ� readNewRow() ���� ��,
    //                ���� oid node ��ġ�� oid record ��ġ
    // mLstOidNode  : Ʈ������� oid list �߿��� cursor close ��� ������
    //                oid node ��ġ
    // mLstOidCount : Ʈ������� oid list �߿��� cursor close ��� ������
    //                oid node�� ������ record ��ġ
    //----------------------------------------
    void*                    mFstOidNode;
    UInt                     mFstOidCount;
    void*                    mCurOidNode;
    UInt                     mCurOidIndex;
    void*                    mLstOidNode;
    UInt                     mLstOidCount;
    /*
      Open�� Transaction�� OID List�� ������ ��ġ�� ����Ŵ.
      ex)
      Trans->1 Node (1,2,3) , 2 Node (1,2,3), 3 Node (1, 2).
      �̸� mOidNode = 3Node, mOidCount = 2�� �ȴ�.
    void*                    mOidNode;
    UInt                     mOidCount;
    */

    // Seek Function Pointer (FetchNext, FetchPrev)
    smSeekFunc              mSeek[2];
    
    // mTable�� ���� ������(Disk, Memory, Temp Table)
    smTableCursorInsertFunc  mInsert;
    smTableCursorUpdateFunc  mUpdate;
    smTableCursorRemoveFunc  mRemove;
    smTableCursorLockRowFunc mLockRow;


    const smSeekFunc*       mSeekFunc;
    ULong                   mModifyIdxBit;
    ULong                   mCheckUniqueIdxBit;
    SChar                 * mUpdateInplaceEscalationPoint;

    /* Table Info: ���� �� Cursor�� Table�� ���� Insert, Delet�� Record ����
       : PRJ-1496 */
    smxTableInfo*            mTableInfo;

    // Member �ʱ�ȭ
    void init( void );

    // PROJ-2068
    void                   * mDPathSegInfo;

    // �������� Insert�� Row Info
    scGRID                   mLstInsRowGRID;
    SChar                  * mLstInsRowPtr;

public:

    /* ������ �Լ� �Դϴ�. ����� �����մϴ�.       */
    idBool isOpened( void ) { return ID_TRUE; }

    void initialize();

   /****************************************/
   /* For A4 : Interface Function Pointers */
   /****************************************/

    /* For A4 :
       ���ڸ� ���̱� ���� Iterator���� ���ڸ� ���ְ� ���������� �����ϸ�
       Ŀ������ ���� Property���� �ϳ��� ����ü(smiCursorProperties)��
       ��� ������
    */
    IDE_RC open( smiStatement*        aStatement,
                 const void*          aTable,
                 const void*          aIndex,
                 smSCN                aSCN,
                 const smiColumnList* aColumns,
                 const smiRange*      aKeyRange,
                 const smiRange*      aKeyFilter,
                 const smiCallBack*   aRowFilter,
                 UInt                 aFlag,
                 smiCursorType        aCursorType,
                 smiCursorProperties* aProperties,
                 idBool               aIsDequeue = ID_FALSE );
    /*
      Cursor�� ������ġ�� �ٽ� �ű��.
      ���������� Iterator�� �ʱ�ȭ�Ѵ�.
    */
    IDE_RC restart( const smiRange*      aKeyRange,
                    const smiRange*      aKeyFilter,
                    const smiCallBack*   aRowFilter );
    // Cursor�� Close�Ѵ�.
    IDE_RC close( void );

    /* Cursor�� Key Range������ �����ϴ� ù��° Row�� �ٷ��� ��ġ��
       Iterator�� Move.*/
    IDE_RC beforeFirst( void );

    /* Cursor�� Key Range������ �����ϴ� ������ Row�� �ٷ� ���� ��ġ��
       Iterator�� Move.*/
    IDE_RC afterLast( void );

    /* FOR A4 : readRow
       Disk Table�� tuple�� ���� id�� �������ֱ� ���� RID�� ���� ��ȯ
    */

    /* Cursor�� Iterator�� aFlag�� ���� ���� �̵���Ű�鼭 ���ϴ�
       Row�� �о�´�.*/
    IDE_RC readRow( const void  ** aRow,
                    scGRID       * aGRID,
                    UInt           aFlag /* SMI_FIND ... */ );

    IDE_RC readRowFromGRID( const void  ** aRow,
                            scGRID         aRowGRID );

    IDE_RC setRowPosition( void   * aRow,
                           scGRID   aRowGRID );

    // Cursor�� Range���ǰ� Filter������ �����ϴ� ��� Row�� ������ ���Ѵ�.
    IDE_RC countRow( const void ** aRow,
                     SLong *       aRowCount );

    IDE_RC insertRow(const smiValue  * aValueList,
                     void           ** aRow,
                     scGRID          * aGRID);

    scGRID getLastInsertedRID( );

    // ���� Cursor�� ����Ű�� Row�� update�Ѵ�.
    IDE_RC updateRow( const smiValue        * aValueList,
                      const smiDMLRetryInfo * aRetryInfo );

    IDE_RC updateRow( const smiValue* aValueList )
    {
        return updateRow( aValueList,
                          NULL ); // retry info
    };

    /* BUG-39507 */
    IDE_RC isUpdatedRowBySameStmt( idBool     * aIsUpdatedRowBySameStmt );

    //PROJ-1362 Internal LOB
    IDE_RC updateRow( const smiValue        * aValueList,
                      const smiDMLRetryInfo * aRetryInfo,
                      void                 ** aRow,
                      scGRID                * aGRID );

    IDE_RC updateRow( const smiValue* aValueList,
                      void**          aRow,
                      scGRID*         aGRID )
    {
        return updateRow( aValueList,
                          NULL, // retry info
                          aRow,
                          aGRID );
    }


    // ���� Cursor�� ����Ű�� Row�� Delete�Ѵ�.
    IDE_RC deleteRow( const smiDMLRetryInfo * aRetryInfo );

    IDE_RC deleteRow()
    {
        return deleteRow( NULL );
    };

    // ���� Cursor�� ����Ű�� Row�� ���ؼ� Lock�� ��´�.
    IDE_RC lockRow();

    IDE_RC doCommonJobs4Open( smiStatement*        aStatement,
                              const void*          aIndex,
                              const smiColumnList* aColumns,
                              UInt                 aFlag,
                              smiCursorProperties* aProperties,
                              smlLockNode **       aCurLockNodePtr,
                              smlLockSlot **       aCurLockSlotPtr );

    inline IDE_RC getCurPos( smiCursorPosInfo * aPosInfo );

    inline IDE_RC setCurPos( smiCursorPosInfo * aPosInfo );

    IDE_RC getLastModifiedRow( void ** aRowBuf,
                               UInt    aLength );

    IDE_RC getModifiedRow( void  ** aRowBuf,
                           UInt     aLength,
                           void   * aRow,
                           scGRID   aRowGRID );

    IDE_RC getLastRow( const void ** aRow,
                       scGRID      * aRowGRID );

    // PROJ-1509
    IDE_RC beforeFirstModified( UInt aFlag );

    IDE_RC readOldRow( const void ** aRow,
                       scGRID      * aRowGRID );

    IDE_RC readNewRow( const void ** aRow,
                       scGRID      * aRowGRID );

    // PROJ-1618 Online Dump: Dump Table�� Object�� �����Ѵ�.
    void   setDumpObject( void * aDumpObject );

    IDE_RC getTableNullRow( void ** aRow, scGRID * aRid );

    // TASK-2398 Disk/Memory Log�и�
    // Hybrid Transaction �˻� �ǽ�
    IDE_RC checkHybridTrans( UInt             aTableType,
                             smiCursorType    aCursorType );

/****************************************/
/* For A4 : MRDB, VRDB Specific Functions     */
/****************************************/
    static IDE_RC openMRVRDB( smiTableCursor *     aCursor,
                              const void*          aTable,
                              smSCN                aSCN,
                              const smiRange*      aKeyRange,
                              const smiRange*      aKeyFilter,
                              const smiCallBack*   aRowFilter,
                              smlLockNode *        aCurLockNodePtr,
                              smlLockSlot *        aCurLockSlotPtr );

    static IDE_RC closeMRVRDB( smiTableCursor * aCursor );

    static IDE_RC updateRowMRVRDB( smiTableCursor       * aCursor,
                                   const smiValue       * aValueList,
                                   const smiDMLRetryInfo* aRetryInfo );

    static IDE_RC deleteRowMRVRDB( smiTableCursor        * aCursor,
                                   const smiDMLRetryInfo * aRetryInfo);

    // PROJ-1509
    static IDE_RC beforeFirstModifiedMRVRDB( smiTableCursor * aCursor,
                                             UInt             aFlag );

    static IDE_RC readOldRowMRVRDB( smiTableCursor * aCursor,
                                    const void    ** aRow,
                                    scGRID         * aRowGRID );

    static IDE_RC readNewRowMRVRDB( smiTableCursor * aCursor,
                                    const void    ** aRow,
                                    scGRID         * aRowGRID );

    static IDE_RC normalInsertRow(smiTableCursor  * aCursor,
                                  const smiValue  * aValueList,
                                  void           ** aRow,
                                  scGRID          * aGRID);

    static IDE_RC dpathInsertRow(smiTableCursor  * aCursor,
                                 const smiValue  * aValueList,
                                 void           ** aRow,
                                 scGRID          * aGRID);

    /* PROJ-2264 */
    static IDE_RC insertRowWithIgnoreUniqueError( smiTableCursor  * aCursor,
                                                  smcTableHeader  * aTableHeader,
                                                  smiValue        * aValueList,
                                                  smOID           * aOID,
                                                  void           ** aRow );

    static IDE_RC getParallelDegree(smiTableCursor * aCursor,
                                    UInt           * aParallelDegree);

    /* TASK-5030 */
    static IDE_RC makeColumnNValueListMRDB(
                        smiTableCursor      * aCursor,
                        const smiValue      * aValueList,
                        smiUpdateColumnList * aUpdateColumnList,
                        UInt                  aUpdateColumnCount,
                        SChar               * aOldValueBuffer,
                        smiColumnList       * aNewColumnList,
                        smiValue            * aNewValueList);

    static IDE_RC makeFullUpdateMRDB( smiTableCursor  * aCursor,
                                      const smiValue  * aValueList,
                                      smiColumnList  ** aNewColumnList,
                                      smiValue       ** aNewValueList,
                                      SChar          ** aOldValueBuffer,
                                      idBool          * aIsReadBeforeImg );

    static IDE_RC destFullUpdateMRDB( smiColumnList  ** aNewColumnList,
                                      smiValue       ** aNewValueList,
                                      SChar          ** aOldValueBuffer );

/****************************************/
/* For A4 : DRDB Specific Functions     */
/****************************************/
    static IDE_RC openDRDB( smiTableCursor *     aCursor,
                            const void*          aTable,
                            smSCN                aSCN,
                            const smiRange*      aKeyRange,
                            const smiRange*      aKeyFilter,
                            const smiCallBack*   aRowFilter,
                            smlLockNode *        aCurLockNodePtr,
                            smlLockSlot *        aCurLockSlotPtr );

    static IDE_RC closeDRDB( smiTableCursor * aCursor );

    static IDE_RC updateRowDRDB( smiTableCursor       * aCursor,
                                 const smiValue       * aValueList,
                                 const smiDMLRetryInfo* aRetryInfo );

    static IDE_RC deleteRowDRDB( smiTableCursor        * aCursor,
                                 const smiDMLRetryInfo * aRetryInfo );

    // PROJ-1509
    static IDE_RC beforeFirstModifiedDRDB( smiTableCursor * aCursor,
                                           UInt             aFlag );

    static IDE_RC readOldRowDRDB( smiTableCursor * aCursor,
                                  const void    ** aRow,
                                  scGRID         * aRowGRID );

    static IDE_RC readNewRowDRDB( smiTableCursor * aCursor,
                                  const void    ** aRow,
                                  scGRID         * aRowGRID );

    // PROJ-1665
    static void * getTableHeaderFromCursor( void * aCursor );

    /* TASK-5030 */
    static IDE_RC makeSmiValueListInFetch(
                       const smiColumn   * aIndexColumn,
                       UInt                aCopyOffset,
                       const smiValue    * aColumnValue,
                       void              * aIndexInfo );

    static IDE_RC makeColumnNValueListDRDB(
                        smiTableCursor      * aCursor,
                        const smiValue      * aValueList,
                        sdcIndexInfo4Fetch  * aIndexInfo4Fetch,
                        smiFetchColumnList  * aFetchColumnList,
                        SChar               * aBeforeRowBufferSource,
                        smiColumnList       * aNewColumnList );

    static IDE_RC makeFetchColumnList( smiTableCursor      * aCursor,
                                       smiFetchColumnList  * aFetchColumnList,
                                       UInt                * aMaxRowSize );

    static IDE_RC makeFullUpdateDRDB( smiTableCursor      * aCursor,
                                      const smiValue      * aValueList,
                                      smiColumnList      ** aNewColumnList,
                                      smiValue           ** aNewValueList,
                                      SChar              ** aOldValueBuffer,
                                      idBool              * aIsReadBeforeImg );

    static IDE_RC destFullUpdateDRDB( smiColumnList  * aNewColumnList,
                                      smiValue       * aNewValueList,
                                      SChar          * aOldValueBuffer );

/***********************************************************/
/* For A4 : Remote Query Specific Functions(PROJ-1068)     */
/***********************************************************/
    static IDE_RC (*openRemoteQuery)(smiTableCursor *     aCursor,
                                     const void*          aTable,
                                     smSCN                aSCN,
                                     const smiRange*      aKeyRange,
                                     const smiRange*      aKeyFilter,
                                     const smiCallBack*   aRowFilter,
                                     smlLockNode *        aCurLockNodePtr,
                                     smlLockSlot *        aCurLockSlotPtr);

    static IDE_RC (*closeRemoteQuery)(smiTableCursor * aCursor);

    static IDE_RC (*updateRowRemoteQuery)(smiTableCursor       * aCursor,
                                          const smiValue       * aValueList,
                                          const smiDMLRetryInfo* aRetryInfo);

    static IDE_RC (*deleteRowRemoteQuery)(smiTableCursor        * aCursor,
                                          const smiDMLRetryInfo * aRetryInfo);

    // PROJ-1509
    static IDE_RC (*beforeFirstModifiedRemoteQuery)(smiTableCursor * aCursor,
                                                    UInt             aFlag);

    static IDE_RC (*readOldRowRemoteQuery)(smiTableCursor * aCursor,
                                           const void    ** aRow,
                                           scGRID         * aRowGRID );

    static IDE_RC (*readNewRowRemoteQuery)(smiTableCursor * aCursor,
                                           const void    ** aRow,
                                           scGRID         * aRowGRID );

    static IDE_RC setRemoteQueryCallback( smiTableSpecificFuncs * aFuncs,
                                          smiGetRemoteTableNullRowFunc aGetNullRowFunc );

    static const smiRange * getDefaultKeyRange( );

    static const smiCallBack * getDefaultFilter( );

private:
    static void makeUpdatedIndexList( smcTableHeader      * aTableHeader,
                                      const smiColumnList * aCols,
                                      ULong               * aModifyIdxBit,
                                      ULong               * aUniqueCheckIdxBit );

    static IDE_RC insertKeyIntoIndices( smiTableCursor * aCursor,
                                        SChar *          aRow,
                                        scGRID           aRowGRID,
                                        SChar *          aNullRow,
                                        SChar **         aExistUniqueRow );

    static IDE_RC insertKeyIntoIndices( smiTableCursor    *aCursor,
                                        scGRID             aRowGRID,
                                        const smiValue    *aValueList,
                                        idBool             aForce );

    static inline IDE_RC insertKeyIntoIndicesForInsert( smiTableCursor    *aCursor,
                                                        scGRID             aRowGRID,
                                                        const smiValue    *aValueList );

    static inline IDE_RC insertKeyIntoIndicesForUpdate( smiTableCursor    *aCursor,
                                                        scGRID             aRowGRID,
                                                        const smiValue    *aValueList );

    static IDE_RC insertKeysWithUndoSID( smiTableCursor * aCursor );

    static IDE_RC deleteKeys( smiTableCursor * aCursor,
                              scGRID           aRowGRID,
                              idBool           aForce );
    
    static inline IDE_RC deleteKeysForDelete( smiTableCursor * aCursor,
                                              scGRID           aRowGRID );
        
    static inline IDE_RC deleteKeysForUpdate( smiTableCursor * aCursor,
                                              scGRID           aRowGRID );

    // PROJ-1509
    static IDE_RC setNextUndoRecSID( sdpSegInfo     * aSegInfo,
                                     sdpExtInfo     * aExtInfo,
                                     smiTableCursor * aCursor,
                                     UChar          * aCurUndoRecHdr,
                                     idBool         * aIsFixPage );

    static IDE_RC setNextUndoRecSID4NewRow( sdpSegInfo     * aSegInfo,
                                            sdpExtInfo     * aExtInfo,
                                            smiTableCursor * aCursor,
                                            UChar          * aCurUndoRecHdr,
                                            idBool         * aIsFixPage );

    static IDE_RC closeDRDB4InsertCursor( smiTableCursor * aCursor );

    static IDE_RC closeDRDB4UpdateCursor( smiTableCursor * aCursor );

    static IDE_RC closeDRDB4DeleteCursor( smiTableCursor * aCursor );


    /* BUG-31993 [sm_interface] The server does not reset Iterator 
     *           ViewSCN after building index for Temp Table
     * ViewSCN�� Statement/TableCursor/Iterator���� �������� ���Ѵ�. */
    inline IDE_RC checkVisibilityConsistency();

    inline void prepareRetryInfo( const smiDMLRetryInfo ** aRetryInfo )
    {
        IDE_ASSERT( aRetryInfo != NULL );

        if( *aRetryInfo != NULL )
        {
            if( (*aRetryInfo)->mIsWithoutRetry == ID_FALSE )
            {
                *aRetryInfo = NULL;
            }
        }
    };

    // PROJ-2264
    static IDE_RC setAgingIndexFlag( smiTableCursor * aCursor,
                                     SChar          * aRecPtr,
                                     UInt             aFlag );

    static smiGetRemoteTableNullRowFunc mGetRemoteTableNullRowFunc;
};

/***********************************************************************
 * Description : Cursor�� ���� ��ġ�� �����´�.
 *
 * aPosInfo - [OUT] Cursor�� ���� ��ġ�� �Ѱܹ޴� ����.
 **********************************************************************/
inline IDE_RC smiTableCursor::getCurPos( smiCursorPosInfo * aPosInfo )
{
    // To Fix PR-8110
    IDE_DASSERT( mIndexModule != NULL );

    return ((smnIndexModule*)mIndexModule)->mGetPosition( mIterator,
                                                          aPosInfo );

}

/***********************************************************************
 * Description : Cursor�� ���� ��ġ�� aPosInfo�� ����Ű�� ��ġ�� �ű��.
 *
 * aPosInfo - [IN] Cursor�� ���ο� ��ġ
 **********************************************************************/
inline IDE_RC smiTableCursor::setCurPos( smiCursorPosInfo * aPosInfo )
{
    // To Fix PR-8110
    IDE_DASSERT( mIndexModule != NULL );

    return ((smnIndexModule*)mIndexModule)->mSetPosition( mIterator,
                                                          aPosInfo );
}

/***********************************************************************
 * Description :
 *
 *    PROJ-1618 Online Dump
 *
 *    Dump Table�� ���� Table Cursor�� ��쿡 ����ϸ�
 *    Dump Object ������ �����Ѵ�.
 *
 **********************************************************************/

inline void
smiTableCursor::setDumpObject( void * aDumpObject )
{
    mDumpObject = aDumpObject;
}

/***********************************************************************
 * PROJ-1655
 * Description : table cursor�� table header�� ��ȯ�Ѵ�.
 *
 **********************************************************************/
inline void * smiTableCursor::getTableHeaderFromCursor( void * aCursor )
{
    return (void*)(((smiTableCursor*)aCursor)->mTable);
}


inline IDE_RC smiTableCursor::insertKeyIntoIndicesForInsert( smiTableCursor    *aCursor,
                                                             scGRID             aRowGRID,
                                                             const smiValue    *aValueList )
{
    return insertKeyIntoIndices( aCursor,
                                 aRowGRID,
                                 aValueList,
                                 ID_TRUE /* aForce */ );
}

inline IDE_RC smiTableCursor::insertKeyIntoIndicesForUpdate( smiTableCursor    *aCursor,
                                                             scGRID             aRowGRID,
                                                             const smiValue    *aValueList )
{
    return insertKeyIntoIndices( aCursor,
                                 aRowGRID,
                                 aValueList,
                                 ID_FALSE /* aForce */ );
}

inline IDE_RC smiTableCursor::deleteKeysForDelete( smiTableCursor * aCursor,
                                                   scGRID           aRowGRID )
{
    return deleteKeys( aCursor,
                       aRowGRID,
                       ID_TRUE /* aForce */ );
}
        
inline IDE_RC smiTableCursor::deleteKeysForUpdate( smiTableCursor * aCursor,
                                                   scGRID           aRowGRID )
{
    return deleteKeys( aCursor,
                       aRowGRID,
                       ID_FALSE /* aForce */ );
}
 
#endif /* _O_SMI_TABLE_CURSOR_H_ */
