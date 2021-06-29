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
 * $Id: smnDef.h 89495 2020-12-14 05:19:22Z emlee $
 **********************************************************************/

#ifndef _O_SMN_DEF_H_
# define _O_SMN_DEF_H_ 1

# include <idTypes.h>
# include <iduStackMgr.h>
# include <smnIndexFile.h>
# include <smDef.h>
# include <smiDef.h>
# include <smpDef.h>
# include <smcDef.h>
# include <iduPtrList.h>

#define SMN_ITERATOR_POOL_SIZE    (64) 

/* smiIndexModule.flag                          */
# define SMN_RANGE_MASK               (0x00000001)
# define SMN_RANGE_ENABLE             (0x00000000)
# define SMN_RANGE_DISABLE            (0x00000001)

/* smiIndexModule.flag                          */
# define SMN_DIMENSION_MASK           (0x00000002)
# define SMN_DIMENSION_ENABLE         (0x00000000)
# define SMN_DIMENSION_DISABLE        (0x00000002)

/* smiIndexModule.flag                          */
/* Default Index�� ���������Ѱ�?                */
# define SMN_DEFAULT_MASK             (0x00000004)
# define SMN_DEFAULT_ENABLE           (0x00000000)
# define SMN_DEFAULT_DISABLE          (0x00000004)

/* smiIndexModule.flag                          */
# define SMN_BOTTOMUP_BUILD_MASK      (0x00000008)
# define SMN_BOTTOMUP_BUILD_ENABLE    (0x00000000)
# define SMN_BOTTOMUP_BUILD_DISABLE   (0x00000008)

/* smiIndexModule.flag                          */
# define SMN_FREE_NODE_LIST_MASK      (0x00000010)
# define SMN_FREE_NODE_LIST_DISABLE   (0x00000000)
# define SMN_FREE_NODE_LIST_ENABLE    (0x00000010)

# define SMN_SEQUENTIAL_ITERATOR      (smnnModule)
# define SMN_GRID_ITERATOR            (smnpModule)
# define SMN_FIXED_TABLE_ITERATOR     (smnfModule)

# define SVN_SEQUENTIAL_ITERATOR      (svnnModule)
# define SVN_GRID_ITERATOR            (svnpModule)

/* added for A4 */
/* disk table sequential iterator */
# define SDN_SEQUENTIAL_ITERATOR      (sdnnModule)
# define SDN_GRID_ITERATOR            (sdnpModule)

// PROJ-1362  LOB���� �ε��� ���� ���� Ǯ��.
// parallel index building, refine ��  smtPJMgr����
// index��� pointer ���, ÷�ڸ� ����ϵ��� ����.
# define SMN_NULL_POS     (-2)

// PROJ-1617 �ε��� ���� �м� �����
#define SMN_DUMP_VALUE_LENGTH                          (24)


/* ----------------------------------------------------------------------------
 *    for index 
 * --------------------------------------------------------------------------*/
// #define SMN_MAX_IDX_COLUMNS       (80) -- in A3
// #define SMN_MAX_IDX_COLUMNS       (32) -- in A4 --> SMI LAYER��...

typedef struct smcTableHeader smcTableHeader;
typedef struct smnIndexHeader smnIndexHeader;

// physical ager���� 
typedef IDE_RC (*smnFreeNodeFunc)( void* aNodes);
                
typedef struct smnNode
{
    smnNode*        mPrev;
    smnNode*        mNext;
    smSCN           mSCN;
    smnNode*        mFreeNodeList;
}
smnNode;

typedef struct smnSortStack
{
    void*             mLeftNode;
    void*             mRightNode;
    SInt              mLeftPos;
    SInt              mRightPos;
}
smnSortStack;

typedef struct smnpJobItem
{
    struct smnpJobItem  *mNext;
    struct smnpJobItem  *mPrev;
    iduMutex             mMutex;
    iduStackMgr          mStack;
    void                *mArrNode;
    idBool               mIsUnique;
    idBool               mIsFinish;
    idBool               mIsFirst;
    idBool               mIsPersIndex;
    UInt                 mJobProcessCnt;
    smcTableHeader      *mTableHeader;
    smnIndexHeader      *mIndexHeader;
    iduCond              mCV;
} smnpJobItem;

typedef IDE_RC (*smnGetPageFunc)( smcTableHeader * aTable,
                                  scPageID       * aPageID,
                                  idBool         * aLocked );

typedef IDE_RC (*smnGetRowFunc)( smcTableHeader   * aTable,
                                 scPageID           aPageID,
                                 SChar           ** aFence,
                                 SChar           ** aRow,
                                 idBool             aIsNeedValidation );

typedef IDE_RC (*smnInsertFunc)( idvSQL*           aStatistics,
                                 void*             aTrans,
                                 void*             aTable,
                                 void*             aIndex,
                                 smSCN             aInfiniteSCN,
                                 SChar*            aRow,
                                 // BUG-25022
                                 // in case of disk spatial index,
                                 // this parameter used as UndoRID
                                 SChar*            aNull, 
                                 idBool            aUniqueCheck,
                                 smSCN             aStmtViewSCN,
                                 void             *aRowSID,
                                 SChar**           aExistUniqueRow,
                                 ULong             aInsertWaitTime,
                                 idBool            aForbiddenToRetry );

// modified for A4,
// disk temp table�� ���� cluster index�� ���Ͽ� row�� �����
// ���� index�� �ٷ� �����ϱ� ���Ͽ� function signature�� ����.
// disk btree�� �Ʒ��� �Լ��� old key�鿡 limit SCN�� �����Ѵ�.
typedef IDE_RC (*smnDeleteFunc)( idvSQL*        aStatistics,
                                 void *         aTrans,
                                 void *         aIndex,
                                 SChar *        aRow,
                                 smiIterator *  aIterator,
                                 sdRID          aRowRID);

// added for A4,
// ���� key�� free�� GC�� �Ʒ��� �Լ��� ���� �����Ѵ�.
typedef IDE_RC (*smnFreeFunc)( void *         aIndex,
                               SChar *        aRow,
                               idBool         aIgnoreNotFoundKey,
                               idBool       * aExistKey );

typedef IDE_RC (*smnInsertRollbackFunc)( idvSQL * aStatistics,
                                         void   * aMtx,
                                         void   * aIndex,
                                         SChar  * aKeyValue,
                                         sdSID    aRowSID,
                                         UChar  * aRollbackContext,
                                         idBool   aIsDupKey );

typedef IDE_RC (*smnDeleteRollbackFunc)( idvSQL * aStatistics,
                                         void   * aMtx,
                                         void   * aIndex,
                                         SChar  * aKeyValue,
                                         sdSID    aRowSID,
                                         UChar  * aRollbackContext );

typedef IDE_RC (*smnAgingFunc)( idvSQL         * aStatistics,
                                void           * aTrans,
                                smcTableHeader * aHeader,
                                smnIndexHeader * aIndex );

// added for A4
// disk temp table������ cluster������ ���Ͽ� �߰���.
typedef IDE_RC (*smnInitMetaFunc)( UChar   * aMetaPage, 
                                   UInt      aBuildFlag,
                                   void    * aMtx );

typedef IDE_RC (*smnMakeDiskImageFunc) (smnIndexHeader* aIndex,
                                        smnIndexFile* aIndexFile);

typedef IDE_RC (*smnBuildIndexFunc) ( idvSQL*               aStatistics,
                                      void*                 aTrans,
                                      smcTableHeader*       aTable,
                                      smnIndexHeader*       aIndex,
                                      smnGetPageFunc        aGetPageFunc,
                                      smnGetRowFunc         aGetRowFunc,
                                      SChar*                aNullPtr,
                                      idBool                aIsNeedValidation,
                                      idBool                aIsEnableTable,
                                      UInt                  aParallelDegree,
                                      UInt                  aBuildFlag,
                                      UInt                  aTotalRecCnt );

// BUG-24403 : Disk Tablespace online���� update �߻��� index ���ѷ���
typedef void (*smnGetSmoNoFunc)(void  * aIndex,
                                ULong * aSmoNo );

// BUG-25351 : Ű���� �� �ε��� Build�� ���� ���ȭ �����丵�� �ʿ��մϴ�. 
typedef IDE_RC (*smnMakeKeyFromRow)(
                           idvSQL                * aStatistics,
                           sdrMtx                * aMtx,
                           sdrSavePoint          * aSP,
                           void                  * aTrans,
                           void                  * aTableHeader,
                           const smnIndexHeader  * aIndex,
                           const UChar           * aRow,
                           sdbPageReadMode         aPageReadMode,
                           scSpaceID               aTableSpaceID,
                           smFetchVersion          aFetchVersion,
                           sdRID                   aTssRID,
                           const smSCN           * aSCN,
                           const smSCN           * aInfiniteSCN,
                           UChar                 * aDestBuf,
                           idBool                * aIsDeletedRow,
                           idBool                * aIsPageLatchReleased );

typedef IDE_RC (*smnMakeKeyFromSmiValue)(
                           const smnIndexHeader   *aIndex,
                           const smiValue         *aValueList,
                           UChar                  *aDestBuf );

typedef IDE_RC (*smnMemoryFunc)( void* aModule );

typedef IDE_RC (*smnFreeNodeListFunc)(idvSQL* aStatistics,
                                      smnIndexHeader* aIndex,
                                      void* aTrans);

// Proj-1911 �ǽð� Complete DDl - Alter table ����
// �ǽð� Alter DDL�� ���� �ε��� Į�� �缳�� �Լ�
typedef IDE_RC (*smnRebuildIndexColumn)(smnIndexHeader * aIndex,
                                        smcTableHeader * aTable, 
                                        void           * aIndexHeader );

/* PROJ-2162 RestartRiskReduction
 * Index�� Inconsistent Flag ������ */
typedef IDE_RC (*smnSetIndexConsistency)(smnIndexHeader * aIndex,
                                         idBool           aIsConsistent );

/* TASK-4990 changing the method of collecting index statistics */
typedef IDE_RC (*smnGatherStat)(idvSQL         * aStatistics,
                                void           * aTrans,
                                SFloat           aPercentage,
                                SInt             aDegree,
                                smcTableHeader * aTable,
                                void           * aTotalTableArg,
                                smnIndexHeader * aIndex,
                                void           * aStats,
                                idBool           aDynamicMode );


// modified for A4, temporary cluster index.
typedef IDE_RC (*smnCreateFunc)( idvSQL           * aStatistics,
                                 smcTableHeader   * aTable,
                                 smnIndexHeader   * aIndex,
                                 smiSegAttr       * aSegAttr,
                                 smiSegStorageAttr* aSegStoAttr,
                                 smnInsertFunc    * aInsertVersion,
                                 smnIndexHeader  ** aRebuildIndexHeader,
                                 ULong              aSmoNo );

typedef IDE_RC (*smnDropFunc)( smnIndexHeader* aIndex );

typedef IDE_RC (*smnGetPositionFunc)( smiIterator *      aIterator,
                                      smiCursorPosInfo * aPosInfo );
    
typedef IDE_RC (*smnSetPositionFunc)( smiIterator *      aIterator,
                                      smiCursorPosInfo * aPosInfo );


typedef IDE_RC (*smnAllocIteratorFunc)( void ** aIteratorMem );
    
typedef IDE_RC (*smnFreeIteratorFunc)( void * aIteratorMem);
//for truncate temp table 
typedef IDE_RC (*smnReInitFunc)(idvSQL* aStatistics, smnIndexHeader* aIndex );

typedef struct smnIndexModule
{
    /* FOR A4 : smnBuiltInIndices�� �̵�
      SChar*                name;
    */
    UInt                  mType;
    UInt                  mFlag;
    UInt                  mMaxKeySize;
    smnMemoryFunc         mPrepareIteratorMem;
    smnMemoryFunc         mReleaseIteratorMem;
    smnMemoryFunc         mPrepareFreeNodeMem;
    smnMemoryFunc         mReleaseFreeNodeMem;
    smnCreateFunc         mCreate;
    smnDropFunc           mDrop;

    // FOR A4 : smiTableCursor���� ���
    smTableCursorLockRowFunc mLockRow;

    smnDeleteFunc          mDelete;            //Disk Index������ ���� delete marking
    smnFreeFunc            mFreeSlot;          // ���� slot�� free -> GC, rollback����
    smnInsertRollbackFunc  mInsertKeyRollback; // ���Ե� Ű�� �ѹ�
    smnDeleteRollbackFunc  mDeleteKeyRollback; // ������ Ű�� ����
    smnAgingFunc           mAging;             // Node Aging�� Delayed CTS Stamping ����
    
    smInitFunc             mInitIterator;
    smDestFunc             mDestIterator;
    smnFreeNodeListFunc    mFreeAllNodeList;
    smnGetPositionFunc     mGetPosition;
    smnSetPositionFunc     mSetPosition;

    smnAllocIteratorFunc   mAllocIterator;
    smnFreeIteratorFunc    mFreeIterator;
    smnReInitFunc          mReInit;
    // added for A4
    // disk temp table������ cluster������ ���Ͽ� �߰���.
    smnInitMetaFunc        mInitMeta;
    
    // FOR A4 : �Ʒ��� �Լ������͵��� DRDB���� ��� ����
    smnMakeDiskImageFunc   mMakeDiskImage;

    smnBuildIndexFunc      mBuild;           // index build
    // BUG-24403 : Disk Tablespace online���� update �߻��� index ���ѷ���
    smnGetSmoNoFunc        mGetSmoNo;
    // BUG-25351 : Ű���� �� �ε��� Build�� ���� ���ȭ �����丵�� �ʿ��մϴ�. 
    smnMakeKeyFromRow        mMakeKeyFromRow;
    smnMakeKeyFromSmiValue   mMakeKeyFromSmiValue;
    // Proj-1911 �ǽð� Alter DDL : �ǽð� DDL�� �ε��� Column �籸���� �ʿ��մϴ�.
    smnRebuildIndexColumn    mRebuildIndexColumn;
    smnSetIndexConsistency   mSetIndexConsistency;
    smnGatherStat            mGatherStat;
}smnIndexModule;


/*
 * SMI_MAX_INDEXTYPE_ID�� SMI_INDEXIBLE_TABLE_TYPES�� ���� ���Ǵ� ����
 * smiIndexTypes�� SMI_TABLE_****�� �����.
 * 
 * [BUG-27049] [SM-MEMORY] �޸� �ε������� ���̺� Ÿ���� ������
 *             ��ũ��(SMN_GET_BASE_TABLE_TYPE)�� ������ �ֽ��ϴ�.
 */
#define SMN_GET_BASE_TABLE_TYPE(aTableType)      \
    (((aTableType) & SMI_TABLE_TYPE_MASK))
#define SMN_GET_BASE_TABLE_TYPE_ID(aTableType)   \
    (((aTableType) & SMI_TABLE_TYPE_MASK) >> 12)

#define SMN_MAKE_INDEX_MODULE_ID( aTableType, aIndexType )      \
    ( ( (aIndexType) * SMI_MAX_INDEXTYPE_ID ) +                 \
      SMN_GET_BASE_TABLE_TYPE_ID(aTableType) )
#define SMN_MAKE_INDEX_TYPE_ID( aIdxModuleID )      \
     ( (aIdxModuleID) / SMI_MAX_INDEXTYPE_ID ) 
#define SMN_MAKE_TABLE_TYPE_ID( aIdxModuleID )      \
     ( (aIdxModuleID) % SMI_MAX_INDEXTYPE_ID )

/* FOR A4 : smnBuiltInIndices
     ������ smnIndexModule�� ������ �迭�̴� �ε��� ��� ������
     �ε��� Ÿ�԰� ���̺� Ÿ�Կ� ���� 2���� �迭�� ������
     �̸� ���� �ε��� �̸��� ���� �и���
*/

// smnIndexType.mFlag
// �ε����� Unique ���� ����
# define SMN_INDEX_UNIQUE_MASK             (0x00000001)
# define SMN_INDEX_UNIQUE_ENABLE           (0x00000001)
# define SMN_INDEX_UNIQUE_DISABLE          (0x00000000)

// smnIndexType.mFlag
// To Fix BUG-16218
// �ε����� Composite Index ���� ����
# define SMN_INDEX_COMPOSITE_MASK          (0x00000002)
# define SMN_INDEX_COMPOSITE_ENABLE        (0x00000002)
# define SMN_INDEX_COMPOSITE_DISABLE       (0x00000000)

// smnIndexType.mFlag
// �ε����� ����� Comparision ���� ����
# define SMN_INDEX_NUMERIC_COMPARE_MASK    (0x00000004)
# define SMN_INDEX_NUMERIC_COMPARE_ENABLE  (0x00000004)
# define SMN_INDEX_NUMERIC_COMPARE_DISABLE (0x00000000)

// smnIndexType.mFlag
// �ε����� ����� Comparision ���� ����
# define SMN_INDEX_AGING_MASK              (0x00000008)
# define SMN_INDEX_AGING_ENABLE            (0x00000008)
# define SMN_INDEX_AGING_DISABLE           (0x00000000)

# define SMN_INDEX_DROP_FALSE              (0x0000)
# define SMN_INDEX_DROP_TRUE               (0x0001)

// bottom-up ����� index build�� �� phase
typedef enum smnBottomUpBuildPhase
{
    SMN_EXTRACT_AND_SORT = 0,
    SMN_MERGE_RUN,
    SMN_UNION_MERGE     // memory index only
} smnBottomUpBuildPhase;

typedef enum smnBuiltIn
{
    SMN_BUILTIN_INDEX = 0,
    SMN_EXTERNAL_INDEX
} smnBuiltIn;

typedef struct smnIndexType
{
    UShort                mTypeID;
    smnBuiltIn            mBuiltIn;
    UInt                  mFlag;
    SChar*                mName;
    smnIndexModule *      mModule[SMI_INDEXIBLE_TABLE_TYPES];
} smnIndexType;

#define SMN_MAX_INDEX_NAME_SIZE (40)

#define SMN_MAX_IDX_COUNT         IDL_MIN(((SMP_VC_PIECE_MAX_SIZE * SMC_MAX_INDEX_OID_CNT )/sizeof(smnIndexHeader)), 64)

// update�ÿ� ���� �ִ� index�� ǥ���ϴ� bit �迭�� byte size
#define SMN_MAX_IDX_UPTBIT_BYTES  ((SMN_MAX_IDX_COUNT/8) + 1)

#define MAX_MINMAX_VALUE_SIZE    (SMI_MAX_MINMAX_VALUE_SIZE)

#define SMN_RUNTIME_PARAMETERS                                             \
  idBool       mIsCreated;                                                 \
  idBool       mIsConsistent; /* PROJ-2162 */                              \
  iduMutex     mStatMutex;                                                 \
  UInt         mAtomicA; /* For no-latch read of stats */                  \
  UInt         mAtomicB; /* For no-latch read of stats */


typedef struct smnRuntimeHeader
{
    SMN_RUNTIME_PARAMETERS
}smnRuntimeHeader;

/* BUG-31845 [sm-disk-index] Debugging information is needed for 
 * PBT when fail to check visibility using DRDB Index.
 * �� �ڷᱸ���� ����ɰ��, smnManager::dumpCommonHeader
 * ���� �����Ǿ�� �մϴ�. */
typedef struct smnIndexHeader
{
    scGRID               mIndexSegDesc;
    // PRJ-1497, reserve area.
    ULong                mReserv;
    
    smOID                mTableOID;
    smOID                mSelfOID;  /* index header�� ���� ����Ų��. */
    smnRuntimeHeader   * mHeader;   /* related in smcTempPage... */
    smnIndexModule     * mModule;
    smnInsertFunc        mInsert;
    smiColumnList      * mColumnLst;
    SChar                mName[SMN_MAX_INDEX_NAME_SIZE+8];
    UInt                 mId;
    UInt                 mType;
    UInt                 mFlag;
    UShort               mDropFlag; /* PROJ-2433 mMaxKeySize �߰����� UInt -> UShort�� ���� */

    /* PROJ-2433
       mMaxKeySize : SQL���� ������ Direct Key Index�� �ִ���� (xx)
                     ex) CREATE INDEX index_name On table_name ( column ) DIRECTKEY MAXSIZE xx 

                    - MAXSIZE �� �����Ǿ� ���� ������, 
                      property __MEM_BTREE_DEFAULT_MAX_KEY_SIZE ������ ���õ� (default:8) */
    UShort               mMaxKeySize;

    UInt                 mColumnCount;
    UInt                 mColumns[SMI_MAX_IDX_COLUMNS];
    UInt                 mColumnFlags[SMI_MAX_IDX_COLUMNS]; // ascending/descending
    // added for A4, to store offset of key columns
    UInt                 mColumnOffsets[SMI_MAX_IDX_COLUMNS];
    // PROJ-1704 MVCC Renewal
    smiSegAttr           mSegAttr;
    //added for A4, disk b+ index segment descriptor.
    //PRJ-1671 BITMAP SEGMENT
    smiSegStorageAttr    mSegStorageAttr;

    /* TASK-4990 changing the method of collecting index statistics */
    smiIndexStat         mStat;
    /* BUG-42095 : ��� �� �� */
    smiCachedPageStat  * mLatestStat;
}smnIndexHeader;

// added for a4 : smaPhysicalAger is removed.
typedef struct smnFreeNodeList
{
    smnFreeNodeFunc  mFreeNodeFunc;
    smnNode*         mHead;
    smnNode*         mTail;
    smnNode*         mFreeNodeList;
    UInt             mAddCnt;
    UInt             mHandledCnt;
    
    struct smnFreeNodeList* mPrev;
    struct smnFreeNodeList* mNext;
}smnFreeNodeList;

/* TASK-6006 */
typedef struct smnRebuildIndexInfo
{
    smcTableHeader      * mTable;
    smnIndexHeader      * mIndexCursor;
}smnRebuildIndexInfo;

typedef enum smnRebuildMode
{
    SMN_INDEX_REBUILD_TABLE_MODE = 0,
    SMN_INDEX_REBUILD_INDEX_MODE
}smnRebuildMode;

#define SMN_INDEX_META_OFFSET ID_SIZEOF(sdpPhyPageHdr)

#define SMN_INDEX_PERSISTENCE_NOUSE     (0)
#define SMN_INDEX_PERSISTENCE_USE       (1)
#define SMN_INDEX_PERSISTENCE_FORCE     (2)

#endif /* _O_SMN_DEF_H_ */
