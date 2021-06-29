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
 

/*******************************************************************************
 * $Id: smiTableCursor.cpp 90083 2021-02-26 00:58:48Z et16 $
 ******************************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>

#include <smErrorCode.h>

#include <smDef.h>
#include <sdp.h>
#include <smc.h>
#include <sdc.h>
#include <sml.h>
#include <sct.h>
#include <smm.h>
#include <smn.h>
#include <smnbModule.h>
#include <sdn.h>
#include <sdnnDef.h>
#include <sm2x.h>
#include <sma.h>
#include <smi.h>
#include <smu.h>
#include <svcRecord.h>
#include <svnModules.h>
#include <sgmManager.h>
#include <smr.h>


extern smiGlobalCallBackList gSmiGlobalCallBackList;

extern "C" SInt gCompareSmiUpdateColumnListByColId( const void *aLhs,
                                                    const void *aRhs )
{
    const smiUpdateColumnList * sLhs = (const smiUpdateColumnList *)aLhs;
    const smiUpdateColumnList * sRhs = (const smiUpdateColumnList *)aRhs;

    IDE_DASSERT(aLhs != NULL);
    IDE_DASSERT(aRhs != NULL);

    if( sLhs->column->id == sRhs->column->id )
    {
        return 0;
    }
    else
    {
        if( sLhs->column->id > sRhs->column->id )
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
}

/*
 * [BUG-26923] [5.3.3 release] CodeSonar::Dangerous Function Cast
 * AA Functions ( SEEK )
 */
static IDE_RC smiTableCursorAAFunction( void );
static IDE_RC smiTableCursorSeekAAFunction( void*, const void* );

/*
 * NA1 Functions( INSERT, UPDATE, REMOVE )
 */
static IDE_RC smiTableCursorNA1Function( void );
static IDE_RC smiTableCursorInsertNA1Function( idvSQL*, void*, void*, void*,
                                               smSCN, SChar**, scGRID*,
                                               const smiValue*, UInt );
static IDE_RC smiTableCursorUpdateNA1Function( idvSQL*, void*, smSCN, void*,
                                               void*, SChar*, scGRID, SChar**,
                                               scGRID*, const smiColumnList*,
                                               const smiValue*, const smiDMLRetryInfo*,
                                               smSCN, void*, ULong, idBool );
static IDE_RC smiTableCursorRemoveNA1Function( idvSQL*, void*, smSCN, void*,
                                               void*, SChar*, scGRID, smSCN,
                                               const smiDMLRetryInfo*,
                                               idBool, idBool );

/*
 * NA2 Functions ( SEEK )
 */
static IDE_RC smiTableCursorNA2Function( void );
static IDE_RC smiTableCursorSeekNA2Function( void*, const void* );

/*
 * NA3 Functions ( LOCK_ROW )
 */
static IDE_RC smiTableCursorNA3Function( void );
static IDE_RC smiTableCursorLockRowNA3Function( smiIterator* );

static IDE_RC smiTableCursorDefaultCallBackFunction( idBool     * aResult,
                                                     const void *,
                                                     void       *, /* aDirectKey */
                                                     UInt        , /* aDirectKeyPartialSize */
                                                     const scGRID,
                                                     void       * );

/***********************************************************/
/* For A4 : Remote Query Specific Functions(PROJ-1068)     */
/***********************************************************/
IDE_RC (*smiTableCursor::openRemoteQuery)(smiTableCursor *     aCursor,
                                          const void*          aTable,
                                          smSCN                aSCN,
                                          const smiRange*      aKeyRange,
                                          const smiRange*      aKeyFilter,
                                          const smiCallBack*   aRowFilter,
                                          smlLockNode *        aCurLockNodePtr,
                                          smlLockSlot *        aCurLockSlotPtr);

IDE_RC (*smiTableCursor::closeRemoteQuery)(smiTableCursor * aCursor);

IDE_RC (*smiTableCursor::updateRowRemoteQuery)(smiTableCursor       * aCursor,
                                               const smiValue       * aValueList,
                                               const smiDMLRetryInfo* aRetryInfo);

IDE_RC (*smiTableCursor::deleteRowRemoteQuery)(smiTableCursor        * aCursor,
                                               const smiDMLRetryInfo * aRetryInfo);

IDE_RC (*smiTableCursor::beforeFirstModifiedRemoteQuery)(smiTableCursor * aCursor,
                                                         UInt             aFlag);

IDE_RC (*smiTableCursor::readOldRowRemoteQuery)(smiTableCursor * aCursor,
                                                const void    ** aRow,
                                                scGRID         * aRowGRID );

IDE_RC (*smiTableCursor::readNewRowRemoteQuery)(smiTableCursor * aCursor,
                                                const void    ** aRow,
                                                scGRID         * aRowGRID );

smiGetRemoteTableNullRowFunc smiTableCursor::mGetRemoteTableNullRowFunc;

// Transaction Isolation Level�� ���� Table Lock Mode����.
static const UInt smiTableCursorIsolationLock[4][8] =
{
    {/* SMI_ISOLATION_CONSISTENT                              */
        SMI_LOCK_READ,            /* SMI_LOCK_READ            */
        SMI_LOCK_WRITE,           /* SMI_LOCK_WRITE           */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE  /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SMI_ISOLATION_REPEATABLE                              */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_READ            */
        SMI_LOCK_WRITE,           /* SMI_LOCK_WRITE           */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE  /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SMI_ISOLATION_NO_PHANTOM                              */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_READ            */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_WRITE           */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE  /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SMI_ISOLATION_NO_PHANTOM                              */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_READ            */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_WRITE           */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_TABLE_EXCLUSIVE */
        SMI_LOCK_TABLE_EXCLUSIVE  /* SMI_LOCK_TABLE_EXCLUSIVE */
    }
};

// SMI Interface�� Lock Mode�� smlLockMgr�� LockMode�� ��ȯ
static const  smlLockMode smiTableCursorLockMode[5] =
{
    SML_ISLOCK, /* SMI_LOCK_READ            */
    SML_IXLOCK, /* SMI_LOCK_WRITE           */
    SML_IXLOCK, /* SMI_LOCK_REPEATABLE      */
    SML_SLOCK,  /* SMI_LOCK_TABLE_SAHRED    */
    SML_XLOCK   /* SMI_LOCK_TABLE_EXCLUSIVE */
};

// Table�� �ɷ� �ִ� Lock Mode�� ���� Cursor�� ������ ����.
static const UInt smiTableCursorLock[6][5] =
{
    {/* SML_NLOCK                                             */
        0,                        /* SMI_LOCK_READ            */
        0,                        /* SMI_LOCK_WRITE           */
        0,                        /* SMI_LOCK_REPEATABLE      */
        0,                        /* SMI_LOCK_TABLE_SAHRED    */
        0                         /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SML_SLOCK                                             */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_READ            */
        0,                        /* SMI_LOCK_WRITE           */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        0                         /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SML_XLOCK                                             */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_READ            */
        SMI_LOCK_TABLE_EXCLUSIVE, /* SMI_LOCK_WRITE           */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_REPEATABLE      */
        SMI_LOCK_TABLE_SHARED,    /* SMI_LOCK_TABLE_SAHRED    */
        SMI_LOCK_TABLE_EXCLUSIVE  /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SML_ISLOCK                                            */
        SMI_LOCK_READ,            /* SMI_LOCK_READ            */
        0,                        /* SMI_LOCK_WRITE           */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_REPEATABLE      */
        0,                        /* SMI_LOCK_TABLE_SAHRED    */
        0                         /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SML_IXLOCK                                            */
        0,                        /* SMI_LOCK_READ            */
        SMI_LOCK_WRITE,           /* SMI_LOCK_WRITE           */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_REPEATABLE      */
        0,                        /* SMI_LOCK_TABLE_SAHRED    */
        0                         /* SMI_LOCK_TABLE_EXCLUSIVE */
    },
    {/* SML_SIXLOCK                                           */
        SMI_LOCK_READ,            /* SMI_LOCK_READ            */
        SMI_LOCK_WRITE,           /* SMI_LOCK_WRITE           */
        SMI_LOCK_REPEATABLE,      /* SMI_LOCK_REPEATABLE      */
        0,                        /* SMI_LOCK_TABLE_SAHRED    */
        0                         /* SMI_LOCK_TABLE_EXCLUSIVE */
    }
};

static const smiCallBack smiTableCursorDefaultCallBack =
{
    0, smiTableCursorDefaultCallBackFunction, NULL
};

static const smiRange smiTableCursorDefaultRange =
{
    NULL, NULL, NULL,
    { 0, smiTableCursorDefaultCallBackFunction, NULL },
    { 0, smiTableCursorDefaultCallBackFunction, NULL }
};

static const smSeekFunc smiTableCursorSeekFunctions[6] =
{
    (smSeekFunc)smiTableCursorSeekNA2Function,
    (smSeekFunc)smiTableCursorSeekNA2Function,
    (smSeekFunc)smiTableCursorSeekNA2Function,
    (smSeekFunc)smiTableCursorSeekNA2Function,
    (smSeekFunc)smiTableCursorSeekAAFunction,
    (smSeekFunc)smiTableCursorSeekAAFunction
};

static const smiTableDMLFunc smiTableDMLFunctions[SMI_TABLE_TYPE_COUNT] =
{
    /* SMI_TABLE_META     */
    {
        (smTableCursorInsertFunc)   smcRecord::insertVersion,
        (smTableCursorUpdateFunc)   smcRecord::updateVersion,
        (smTableCursorRemoveFunc)   smcRecord::removeVersion,
    },
    /* SMI_TABLE_TEMP_LEGACY */
    {
        (smTableCursorInsertFunc)  NULL,
        (smTableCursorUpdateFunc)  NULL,
        (smTableCursorRemoveFunc)  NULL,
    },
    /* SMI_TABLE_MEMORY   */
    {
         (smTableCursorInsertFunc)  smcRecord::insertVersion,
         (smTableCursorUpdateFunc)  smcRecord::updateVersion,
         (smTableCursorRemoveFunc)  smcRecord::removeVersion,
    },
    /* SMI_TABLE_DISK     */
    {
        (smTableCursorInsertFunc)  sdcRow::insert,
        (smTableCursorUpdateFunc)  sdcRow::update,
        (smTableCursorRemoveFunc)  sdcRow::remove,
    },
    /* SMI_TABLE_FIXED    */
    {
        (smTableCursorInsertFunc)  NULL,
        (smTableCursorUpdateFunc)  NULL,
        (smTableCursorRemoveFunc)  NULL,
    },
    /* SMI_TABLE_VOLATILE */
    {
        (smTableCursorInsertFunc)  svcRecord::insertVersion,
        (smTableCursorUpdateFunc)  svcRecord::updateVersion,
        (smTableCursorRemoveFunc)  svcRecord::removeVersion,
    },
    /* SMI_TABLE_REMOTE   */
    {
        (smTableCursorInsertFunc)  NULL,
        (smTableCursorUpdateFunc)  NULL,
        (smTableCursorRemoveFunc)  NULL,
    }
};

static IDE_RC smMemoryFuncAA( void* /*aModule*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC smInitFuncAA( void                * /*aIterator*/,
                            void                * /*aTrans*/,
                            void                * /*aTable*/,
                            void                * /*aIndex*/,
                            void                * /*aDumpObject*/,
                            const smiRange      * /*aRange*/,
                            const smiRange      * /*aKeyFilter*/,
                            const smiCallBack   * /*aRowFilter*/,
                            UInt                  /*aFlag*/,
                            smSCN                 /*aSCN*/,
                            smSCN                 /*aInfinite*/,
                            idBool                /*aUntouchable*/,
                            smiCursorProperties * /*aProperties*/,
                            const smSeekFunc   ** /*aSeekFunc*/,
                            smiStatement        * /*aStatement*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC smDestFuncAA( void * /*aIterator*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC smAllocIteratorFuncAA( void ** /*aIteratorMem*/ )
{
    return IDE_SUCCESS;
}

static IDE_RC smFreeIteratorFuncAA( void * /*aIteratorMem*/ )
{
    return IDE_SUCCESS;
}

static smnIndexModule smDefaultModule = {
    0, /* Type */
    0, /* Flag */
    0, /* Max Key Size */
    (smnMemoryFunc) smMemoryFuncAA,
    (smnMemoryFunc) smMemoryFuncAA,
    (smnMemoryFunc) NULL,
    (smnMemoryFunc) NULL,
    (smnCreateFunc) NULL,
    (smnDropFunc)   NULL,

    (smTableCursorLockRowFunc) NULL,
    (smnDeleteFunc)            NULL,
    (smnFreeFunc)              NULL,
    (smnInsertRollbackFunc)    NULL,
    (smnDeleteRollbackFunc)    NULL,
    (smnAgingFunc)             NULL,
    (smInitFunc)               smInitFuncAA,
    (smDestFunc)               smDestFuncAA,
    (smnFreeNodeListFunc)      NULL,
    (smnGetPositionFunc)       NULL,
    (smnSetPositionFunc)       NULL,

    (smnAllocIteratorFunc)     smAllocIteratorFuncAA,
    (smnFreeIteratorFunc)      smFreeIteratorFuncAA,
    (smnReInitFunc)            NULL,
    (smnInitMetaFunc)          NULL,

    (smnMakeDiskImageFunc)     NULL,

    (smnBuildIndexFunc)        NULL,
    (smnGetSmoNoFunc)          NULL,
    (smnMakeKeyFromRow)        NULL,
    (smnMakeKeyFromSmiValue)   NULL,
    (smnRebuildIndexColumn)    NULL,
    (smnSetIndexConsistency)   NULL,
    (smnGatherStat)            NULL
};

/*
 * [BUG-26923] [5.3.3 release] CodeSonar::Dangerous Function Cast
 * AA Functions ( SEEK )
 */
static IDE_RC smiTableCursorAAFunction( void )
{
    return IDE_SUCCESS;
}
static IDE_RC smiTableCursorSeekAAFunction( void*, const void* )
{
    return smiTableCursorAAFunction();
}

/*
 * NA1 Functions ( INSERT, UPDATE, REMOVE )
 */
static IDE_RC smiTableCursorNA1Function( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiWriteNotApplicable ) );

    return IDE_FAILURE;
}
static IDE_RC smiTableCursorInsertNA1Function( idvSQL*, void*, void*, void*,
                                               smSCN, SChar**, scGRID*,
                                               const smiValue*, UInt )
{
    return smiTableCursorNA1Function();
}
static IDE_RC smiTableCursorUpdateNA1Function( idvSQL*, void*, smSCN, void*,
                                               void*, SChar*, scGRID, SChar**,
                                               scGRID*, const smiColumnList*,
                                               const smiValue*, const smiDMLRetryInfo*,
                                               smSCN, void*, ULong, idBool )
{
    return smiTableCursorNA1Function();
}
static IDE_RC smiTableCursorRemoveNA1Function( idvSQL*, void*, smSCN, void*,
                                               void*, SChar*, scGRID, smSCN,
                                               const smiDMLRetryInfo*,
                                               idBool, idBool )
{
    return smiTableCursorNA1Function();
}

/*
 * NA2 Functions ( SEEK )
 */
static IDE_RC smiTableCursorNA2Function( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;
}
static IDE_RC smiTableCursorSeekNA2Function( void*, const void* )
{
    return smiTableCursorNA2Function();
}

/*
 * NA3 Functions ( LOCK_ROW )
 */
static IDE_RC smiTableCursorNA3Function( void )
{
    IDE_SET( ideSetErrorCode( smERR_ABORT_smiTraverseNotApplicable ) );

    return IDE_FAILURE;
}
static IDE_RC smiTableCursorLockRowNA3Function( smiIterator* )
{
    return smiTableCursorNA3Function();
}

static IDE_RC smiTableCursorDefaultCallBackFunction( idBool     * aResult,
                                                     const void *,
                                                     void       *, /* aDirectKey */
                                                     UInt        , /* aDirectKeyPartialSize */
                                                     const scGRID,
                                                     void       * )
{
    *aResult = ID_TRUE;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Cursor�� Member�� �ʱ�ȭ
 **********************************************************************/
void smiTableCursor::init( void )
{
    mUntouchable = ID_TRUE;
    mTableInfo   = NULL;
    mDumpObject  = NULL;  // PROJ-1618
    mSeek[0]     =
    mSeek[1]     = (smSeekFunc)smiTableCursorSeekNA2Function;
    mInsert      = (smTableCursorInsertFunc)smiTableCursorInsertNA1Function;
    mUpdate      = (smTableCursorUpdateFunc)smiTableCursorUpdateNA1Function;
    mRemove      = (smTableCursorRemoveFunc)smiTableCursorRemoveNA1Function;
    mLockRow     = (smTableCursorLockRowFunc)smiTableCursorLockRowNA3Function;
    mSeekFunc    = smiTableCursorSeekFunctions;
}

/***********************************************************************
 * Description : Cursor�� Member�� �ʱ�ȭ
 **********************************************************************/
void smiTableCursor::initialize()
{
    mPrev = mNext = this;
    mAllPrev = mAllNext = this;

    mTrans          = NULL;
    mStatement      = NULL;
    mCursorType     = SMI_SELECT_CURSOR;
    mIterator       = NULL;

    // PROJ-1509
    mFstUndoRecSID    = SD_NULL_SID;
    mCurUndoRecSID    = SD_NULL_SID;
    mLstUndoRecSID    = SD_NULL_SID;

    mFstOidNode       = NULL;
    mFstOidCount      = 0;
    mCurOidNode       = NULL;
    mCurOidIndex      = 0;
    mLstOidNode       = NULL;
    mLstOidCount      = 0;

    // PROJ-2068
    mDPathSegInfo   = NULL;

    mNeedUndoRec  = ID_TRUE;

    SC_MAKE_NULL_GRID( mLstInsRowGRID );

    mLstInsRowPtr = NULL;

    // PROJ-2416
    mUpdateInplaceEscalationPoint = NULL;
    
    init();
}


/***********************************************************************
 * Description :
 *   FOR A4 :
 *
 *   1. Table�� type������ table specific function�� setting�Ѵ�.
 *      - mOps (Table Type�� ���� open, close, update, delete ...���� setting)
 *
 *   2. TableCursor�� ��� ���� �ʱ�ȭ
 *      - Trans, Table header, Index module(Sequential Iterator)
 *
 *   3. 1���������� setting�� cursor open �Լ��� ȣ���Ѵ�.
 *
 *   aStatement  - [IN] Cursor�� ���� �ִ� Statement
 *   aTable      - [IN] Cursor�� Open�� Table Handle(Table Header Row Pointer
 *   aIndex      - [IN] Cursor�� �̿��� Index Handle(Index Header Pointer)
 *   aSCN        - [IN] Table Row�� SCN���μ� Valid�Ҷ� Table�� SCN�� ����صξ��ٰ�
 *                      open�Ҷ� Table�� Row�� SCN�� ���ؼ� Table�� ���濬����
 *                      �߻��ߴ��� Check.
 *   aColumns    - [IN] Cursor�� Update Cursor�� �ܿ� Update�� Column List�� �ѱ�.
 *   aKeyRange   - [IN] Key Range
 *   aKeyFilter  - [IN] Key Filter : DRDB�� ���� �߰� �Ǿ����ϴ�. KeyRange�� ������
 *                      ��� ���ǹ��� Filter�� ��������� ���� Filter�߿� Index�� Column��
 *                      �� �ش��ϴ� ������ �ִٸ� �� ������ ���� �˻�� Index Node�� Key����
 *                      �̿������ν� �����ؾ��ϴ� Block�� ���� Disk I/O�� ���� �� �ִ�. ������
 *                      Filter�߿� Index Key�� �ش��ϴ� Filter�� Key Filter�и��Ͽ���.
 *   aRowFilter  - [IN] Filter
 *   aFlag       - [IN] 1. Cursor�� �� Table�� ���ؼ� �ʿ�� �ϴ� Lock Mode
 *                         ( SMI_LOCK_ ... )
 *                      2. Cursor�� ���ۼӼ�
 *                         ( SMI_PREVIOUS ... , SMI_TRAVERSE ... )
 *   aProperties - [IN]
 *   aIsDequeue  - [IN] deQueue ����
 **********************************************************************/
IDE_RC smiTableCursor::open( smiStatement*        aStatement,
                             const void*          aTable,
                             const void*          aIndex,
                             smSCN                aSCN,
                             const smiColumnList* aColumns,
                             const smiRange*      aKeyRange,
                             const smiRange*      aKeyFilter,
                             const smiCallBack  * aRowFilter,
                             UInt                 aFlag,
                             smiCursorType        aCursorType,
                             smiCursorProperties* aProperties,
                             idBool               aIsDequeue )
{
    UInt            sTableType;
    smxTableInfo  * sTableInfo;
    smlLockNode   * sCurLockNodePtr = NULL;
    smlLockSlot   * sCurLockSlotPtr = NULL;
    UInt            sState = 0;

    IDE_ERROR( aTable     != NULL );
    IDE_ERROR( aKeyRange  != NULL );
    IDE_ERROR( aKeyFilter != NULL );
    IDE_ERROR( aRowFilter != NULL );
    IDE_ERROR( aStatement != NULL );

    IDU_FIT_POINT_RAISE( "1.PROJ-1407@smiTableCursor::open", ERR_ART_ABORT );

    mTrans = (smxTrans*)(aStatement->getTrans())->mTrans;
    mTransFlag  = aStatement->getTrans()->mFlag;

    mCursorType = aCursorType;
    mIndexModule = &smDefaultModule;

    /* Table Header�� mTable�� Setting�Ѵ�. */
    mTable = (void*)((UChar*)aTable + SMP_SLOT_HEADER_SIZE );


    IDU_FIT_POINT_RAISE( "smiTableCursor::open::ERROR_INCONSISTENT_TABLE", ERROR_INCONSISTENT_TABLE );

    // PROJ-1665
    // DML�� ���� Lock�� ��� ���, Table�� Consistent ���� �˻�
    IDE_TEST_RAISE( ( smcTable::isTableConsistent( mTable ) != ID_TRUE) &&
                    ( smuProperty::getCrashTolerance() == 0 ),
                    ERROR_INCONSISTENT_TABLE );

    /* Direct Path Insert ���� �߿� �ٸ� DML�� ������ ���´�.
     * SELECT CURSOR�� ������� �ʴ� ������ commit ������ HWM�� �������� �ʱ�
     * ������ �ڽ��� INSERT�� �����͸� �ڽ��� �� �� �����Ƿ�, ������ ���Ἲ��
     * ��Ű�� ���� SELECT CURSOR�� ������� �ʴ´�. */
    if( (aFlag & SMI_INSERT_METHOD_MASK) != SMI_INSERT_METHOD_APPEND
        && ( ((smcTableHeader*)mTable)->mSelfOID != SM_NULL_OID ) )
    {
        IDE_TEST( mTrans->getTableInfo( ((smcTableHeader*)mTable)->mSelfOID,
                                        &sTableInfo,
                                        ID_TRUE) /* aIsSearch */
                  != IDE_SUCCESS );

        if( sTableInfo == NULL )
        {
            /* table info�� ã�� �� ������ DPath INSERT�� ���������� ����. */
        }
        else
        {
            IDE_TEST_RAISE( smxTableInfoMgr::isExistDPathIns(sTableInfo)
                                == ID_TRUE,
                            ERROR_DML_AFTER_INSERT_APPEND );
        }
    }
    else
    {
        /* do nothing */
    }

    /* Table Type�� ���´�.*/
    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    //BUG-48230: Dequeue ����
    mIsDequeue = aIsDequeue;

    /* PROJ-2162 */
    if( ( aIndex != NULL ) &&
        ( ( sTableType == SMI_TABLE_DISK ) || 
          ( sTableType == SMI_TABLE_MEMORY ) || 
          ( sTableType == SMI_TABLE_META ) || 
          ( sTableType == SMI_TABLE_VOLATILE ) ) )
    {
        IDE_TEST_RAISE( smnManager::getIsConsistentOfIndexHeader(
                                (void*)aIndex )
                        == ID_FALSE, ERROR_INCONSISTENT_INDEX );
    }


    /* Table Type�� ���� �����ڸ� �����Ѵ�. */
    switch(sTableType)
    {
        case SMI_TABLE_DISK:
            mOps.openFunc      = smiTableCursor::openDRDB;
            mOps.closeFunc     = smiTableCursor::closeDRDB;
            mOps.updateRowFunc = smiTableCursor::updateRowDRDB;
            mOps.deleteRowFunc = smiTableCursor::deleteRowDRDB;

            // PROJ-1509
            mOps.beforeFirstModifiedFunc =
                smiTableCursor::beforeFirstModifiedDRDB;
            mOps.readOldRowFunc  = smiTableCursor::readOldRowDRDB;
            mOps.readNewRowFunc  = smiTableCursor::readNewRowDRDB;

            break;

        case SMI_TABLE_FIXED:
            mOps.openFunc      = smiTableCursor::openMRVRDB;
            mOps.closeFunc     = smiTableCursor::closeMRVRDB;
            mOps.updateRowFunc = NULL;
            mOps.deleteRowFunc = NULL;                

            // PROJ-1509
            mOps.beforeFirstModifiedFunc =NULL;
            mOps.readOldRowFunc  = NULL;
            mOps.readNewRowFunc  = NULL;
            break;

        case SMI_TABLE_MEMORY:
        case SMI_TABLE_META:
            mOps.openFunc      = smiTableCursor::openMRVRDB;
            mOps.closeFunc     = smiTableCursor::closeMRVRDB;
            mOps.updateRowFunc = smiTableCursor::updateRowMRVRDB;
            mOps.deleteRowFunc = smiTableCursor::deleteRowMRVRDB;

            // PROJ-1509
            mOps.beforeFirstModifiedFunc =
                smiTableCursor::beforeFirstModifiedMRVRDB;
            mOps.readOldRowFunc  = smiTableCursor::readOldRowMRVRDB;
            mOps.readNewRowFunc  = smiTableCursor::readNewRowMRVRDB;
            break;
        /* PROJ-1594 Volatile TBS */
        case SMI_TABLE_VOLATILE:
            mOps.openFunc      = smiTableCursor::openMRVRDB;
            mOps.closeFunc     = smiTableCursor::closeMRVRDB;
            mOps.updateRowFunc = smiTableCursor::updateRowMRVRDB;
            mOps.deleteRowFunc = smiTableCursor::deleteRowMRVRDB;

            // PROJ-1509
            mOps.beforeFirstModifiedFunc =
                smiTableCursor::beforeFirstModifiedMRVRDB;
            mOps.readOldRowFunc  = smiTableCursor::readOldRowMRVRDB;
            mOps.readNewRowFunc  = smiTableCursor::readNewRowMRVRDB;
            break;
        /* PROJ-1068 Database Link */
        case SMI_TABLE_REMOTE:
            mOps.openFunc      = smiTableCursor::openRemoteQuery;
            mOps.closeFunc     = smiTableCursor::closeRemoteQuery;
            mOps.updateRowFunc = smiTableCursor::updateRowRemoteQuery;
            mOps.deleteRowFunc = smiTableCursor::deleteRowRemoteQuery;
            mIndexModule       = gSmnSequentialScan.mModule[SMN_GET_BASE_TABLE_TYPE_ID(SMI_TABLE_REMOTE)];

            // PROJ-1509
            mOps.beforeFirstModifiedFunc =
                smiTableCursor::beforeFirstModifiedRemoteQuery;
            mOps.readOldRowFunc  = smiTableCursor::readOldRowRemoteQuery;
            mOps.readNewRowFunc  = smiTableCursor::readNewRowRemoteQuery;
            break;
        case SMI_TABLE_TEMP_LEGACY:
        default:
            IDE_ASSERT(0);
            break;
    }

    if ((( aFlag & SMI_INSERT_METHOD_MASK) == SMI_INSERT_METHOD_APPEND ) &&
        ( smuProperty::getDPathInsertEnable() == ID_TRUE ))
    {
        // Direct-Path INSERT�� ���
        mOps.insertRowFunc = smiTableCursor::dpathInsertRow;
    }
    else
    {
        // �� ���� ���
        mOps.insertRowFunc = smiTableCursor::normalInsertRow;
    }

    /*
      Table Type Independent Function Call
       - TableCursor�� ��� ���� �ʱ�ȭ
         - Trans, Table header, index module...
    */
    IDE_TEST( doCommonJobs4Open(aStatement,
                                aIndex,
                                aColumns,
                                aFlag,
                                aProperties,
                                &sCurLockNodePtr,
                                &sCurLockSlotPtr) != IDE_SUCCESS );
    sState = 1;

    /* Table Type Dependent Function Call */
    IDE_TEST( mOps.openFunc( this,
                             aTable,
                             aSCN,
                             aKeyRange,
                             aKeyFilter,
                             aRowFilter,
                             sCurLockNodePtr,
                             sCurLockSlotPtr) != IDE_SUCCESS );

    /* PROJ-2162 RestartRiskReductino
     * DB�� Inconsistency �ѵ�,  META� �ƴѵ� �����Ϸ� �Ѵٸ� */
    IDE_ERROR( sTableType != SMI_TABLE_TEMP_LEGACY );
    IDE_TEST_RAISE( ( smrRecoveryMgr::getConsistency() == ID_FALSE ) &&
                    ( smuProperty::getCrashTolerance() == 0 ) &&
                    ( sTableType != SMI_TABLE_META ) &&
                    ( sTableType != SMI_TABLE_FIXED ),
                    ERROR_INCONSISTENT_DB );

    /* PROJ-2462 Result Cache */
    if ( ( mCursorType == SMI_INSERT_CURSOR ) ||
         ( mCursorType == SMI_DELETE_CURSOR ) ||
         ( mCursorType == SMI_UPDATE_CURSOR ) ||
         ( mCursorType == SMI_COMPOSITE_CURSOR ) )
    {
        sTableType = SMI_GET_TABLE_TYPE( ( smcTableHeader * )mTable );

        if ( ( sTableType == SMI_TABLE_DISK ) ||
             ( sTableType == SMI_TABLE_MEMORY ) ||
             ( sTableType == SMI_TABLE_VOLATILE ) ||
             ( sTableType == SMI_TABLE_META ) )
        {
            smcTable::addModifyCount( (smcTableHeader *)mTable );
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

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION( ERR_ART_ABORT );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_ART ) );
    }
#endif
    IDE_EXCEPTION( ERROR_INCONSISTENT_DB );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INCONSISTENT_DB) );
    }
    IDE_EXCEPTION( ERROR_INCONSISTENT_TABLE );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_INCONSISTENT_TABLE) );
    }
    IDE_EXCEPTION( ERROR_INCONSISTENT_INDEX );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_INDEX ));
    }
    IDE_EXCEPTION( ERROR_DML_AFTER_INSERT_APPEND );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_DML_AFTER_INSERT_APPEND) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if( ideGetErrorCode() == smERR_REBUILD_smiTableModified )
    {
        SMX_INC_SESSION_STATISTIC( mTrans,
                                   IDV_STAT_INDEX_STMT_REBUILD_COUNT,
                                   1 /* Increase Value */ );
    }

    if( sState == 1 )
    {
        mStatement->closeCursor( this );
        // To Fix PR-13919
        // mOps.openFunc() ���� error�� �߻��ϸ�
        // mIterator �޸𸮰� �������� �ʴ´�.
        if(mIterator != NULL)
        {
            // �Ҵ�� Iterator�� Free�Ѵ�.
            IDE_ASSERT( ((smnIndexModule*)mIndexModule)->mFreeIterator((void*)mIterator)
                        == IDE_SUCCESS );
            mIterator = NULL;
        }
        else
        {
            // Nothing To Do
        }
    }

    init();

    IDE_POP();

    return IDE_FAILURE;
}

/*
    Hybrid Transaction �˻� �ǽ�

    => Disk, Memory���̺� ����(Read/Write)�� Transaction�� Flag����
       => �ΰ��� �� ���ٵ� Transaction�� Hybrid Transaction

    => Meta ���̺� ����� Transaction�� Flag����
       => ���� Commit�� Log Flush�ǽ�
       => ����: smiDef.h�� SMI_TABLE_META_LOG_FLUSH_MASK �κ� �ּ� ����
 */
IDE_RC smiTableCursor::checkHybridTrans( UInt             aTableType,
                                         smiCursorType    aCursorType )
{
    IDE_DASSERT( mTrans != NULL );

    switch ( aTableType )
    {
        case SMI_TABLE_DISK:
            mTrans->setDiskTBSAccessed();

            break;

        case SMI_TABLE_MEMORY:
            mTrans->setMemoryTBSAccessed();
            break;

        case SMI_TABLE_META:
            if (aCursorType != SMI_SELECT_CURSOR )
            {
                // Meta Table�� ������ �Ǿ��� �� Log�� Flush���� ���θ�
                // Ȯ��.
                //
                // �� Flag�� ���� �ִ� ��쿡��
                // Meta Table������ Transaction��
                // Commit�ÿ� �α׸� Flush�ϵ��� �Ѵ�.
                if ( (((smcTableHeader*)mTable)->mFlag & SMI_TABLE_META_LOG_FLUSH_MASK )
                     == SMI_TABLE_META_LOG_FLUSH_TRUE )
                {
                    mTrans->setMetaTableModified();
                }
            }
            break;

        default:
            // Do nothing
            break;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Table Type�� ���� open������ ����Ǵ� �κа� �ٸ� �κ��� �ִµ�
 *               ���⼭�� ����Ǵ� �κ��� ó���Ѵ�.
 *
 *               1. ���� Member �ʱ�ȭ
 *               2. Table Lock ����.
 *               3. Statement�� Cursor ���
 *
 * aStatement  - [IN] Cursor�� ���� Statement
 * aIndex      - [IN] Index Handle (Index Header Pointer)
 * aColumns    - [IN] Update Cursor�� ��� Update�� Column List
 * aFlag       - [IN] 1. Cursor�� �� Table�� ���ؼ� �ʿ�� �ϴ� Lock Mode
 *                      ( SMI_LOCK_ ... )
 *                    2. Cursor�� ���ۼӼ�
 *                      ( SMI_PREVIOUS ... , SMI_TRAVERSE ... )
 *
 * aProperties     - [IN]
 * aCurLockNodePtr - [OUT] Table�� Lock����� �� Table�� Lock Item��  Lock
 *                         Node�� �����. �� Lock Node�� ���� Pointer
 * aCurLockSlotPtr - [OUT] Lock Node�� Lock Slot Pointer
 *
 **********************************************************************/
IDE_RC smiTableCursor::doCommonJobs4Open( smiStatement*        aStatement,
                                          const void*          aIndex,
                                          const smiColumnList* aColumns,
                                          UInt                 aFlag,
                                          smiCursorProperties* aProperties,
                                          smlLockNode **       aCurLockNodePtr,
                                          smlLockSlot **       aCurLockSlotPtr)
{
    idBool           sLocked;
    idBool           sIsExclusive;
    SInt             sIsolationFlag;
    smcTableHeader * sTable;
    idBool           sIsMetaTable = ID_FALSE;
    UInt             sTableType;
    UInt             sTableTypeID;
    UChar            sIndexTypeID;
    smlLockMode      sLockMode;
    smnIndexModule  * sIndexModule = (smnIndexModule*)mIndexModule;

    IDE_TEST_RAISE( mIterator != NULL, ERR_ALREADY_OPENED );

    mStatement    = aStatement;
    sTable        = (smcTableHeader*)mTable;
    mIndex        = (void*)aIndex;
    mIsSoloCursor = ID_FALSE;
    sTableType    = SMI_GET_TABLE_TYPE( sTable );
    sTableTypeID  = SMI_TABLE_TYPE_TO_ID( sTableType );
    sIndexTypeID  = aProperties->mIndexTypeID;

    IDE_ERROR( sIndexTypeID < SMI_INDEXTYPE_COUNT );
    IDE_ERROR( sTableTypeID < SMI_TABLE_TYPE_COUNT );

    if( (aIndex == NULL) && (mCursorType == SMI_INSERT_CURSOR) )
    {
        /* do nothing */
    }
    else
    {
        if( aIndex != NULL )
        {
            sIndexModule = ((smnIndexHeader*)aIndex)->mModule;
        }
        else
        {
            sIndexModule = gSmnAllIndex[sIndexTypeID]->mModule[sTableTypeID];
        }

        IDE_TEST_RAISE( sIndexModule == NULL, err_disabled_index );

        /* index module�� �ùٷ� ���� �Ǿ����� index�� table type���� ���� */
        IDE_ERROR( SMN_MAKE_INDEX_TYPE_ID(sIndexModule->mType) ==
                   (UInt)aProperties->mIndexTypeID );
        IDE_ERROR( SMN_MAKE_TABLE_TYPE_ID(sIndexModule->mType) ==
                   sTableTypeID );
    
        mIndexModule = sIndexModule;
    }

    /* FOR A4 :
       Iterator �޸� �Ҵ��� Index Module�� �ѱ�
       Insert�� ��쿡�� mIsertIterator�� ��� -- ���߿�...

       if flag�� Insert  OP�� ���õǾ� ������
       mIterator�� mInsertIterator�� �ּҸ� ����
       else
       mIterator�� mIndexModule->allocIterator()�� ���� ����
       endif
    */
    /* BUG-43408, BUG-45368 */
    mIterator = (smiIterator *)mIteratorBuffer;

    IDE_TEST(sIndexModule->mAllocIterator((void**)&mIterator)
             != IDE_SUCCESS );

    mColumns       = aColumns;
    mFlag          = aFlag;

    /* BUG-21738: smiTableCursor���� ��� ���� ���ڷ� mQPProp->mStatistics��
     * �߸�����ϰ� ����. */
    mOrgCursorProp = *aProperties;
    mCursorProp    = mOrgCursorProp;

    /*
        FOR A4 : FirstReadPos�� ReadRecordCount�� QP���� ����ؼ� �����ֱ�� ��.
        Replication������ QP�� ���� �ϰ��� ���� ������
        isRepl ���ڰ� �ʿ������.
    */
    /* Fixed Table�� ��� Lock Table ȣ���� �ʿ� ����. */
    if ( (sTableType == SMI_TABLE_META) || (sTableType == SMI_TABLE_FIXED) ||
         (sTableType == SMI_TABLE_REMOTE))
    {
        sIsMetaTable   = ID_TRUE;
        sIsolationFlag = SMI_ISOLATION_CONSISTENT;
    }
    else
    {
        if (( mFlag & SMI_TRANS_ISOLATION_IGNORE ) == SMI_TRANS_ISOLATION_IGNORE )
        {
            /* BUG-47758 Transaction�� Isolation level�� �����ϰ�
             * QP,RP�� ������ �����θ� Cursor�� Isolation level�� ���� */
            sIsolationFlag = SMI_ISOLATION_CONSISTENT;
        }
        else
        {
            /* Transaction Isolation level�� �����ؼ�
             * Table cursor�� isolation level �� ������*/
            sIsolationFlag = mTransFlag & SMI_ISOLATION_MASK;
        }
    }

    /*
       Meta Table�� Create�� ���Ŀ� DDL�� �߻����� �ʱ� ������ Table�� Lock
       �� ���� �ʴ´�.
    */
    if ( sIsMetaTable == ID_FALSE )
    {
        mFlag = (mFlag & ~SMI_LOCK_MASK) |
            smiTableCursorIsolationLock[sIsolationFlag][mFlag & SMI_LOCK_MASK];

        sIsExclusive = smlLockMgr::isNotISorS(
                       smiTableCursorLockMode[mFlag&SMI_LOCK_MASK]);

        // ���̺�� ���õ� ���̺����̽��鿡 ���Ͽ� INTENTION ����� ȹ���Ѵ�.
        IDE_TEST( sctTableSpaceMgr::lockAndValidateTBS(
                          (void*)mTrans,
                          smcTable::getTableSpaceID((void*)mTable),
                          SCT_VAL_DDL_DML,
                          ID_TRUE,   /* intent lock  ���� */
                          sIsExclusive, /* exclusive lock */
                          mCursorProp.mLockWaitMicroSec)
                  != IDE_SUCCESS );

         if ( (sTableType == SMI_TABLE_DISK) &&
              (mFlag & SMI_INSERT_METHOD_MASK) == SMI_INSERT_METHOD_APPEND )
         {
             sLockMode = SML_SIXLOCK;
         }
         else
         {
             sLockMode = smiTableCursorLockMode[mFlag&SMI_LOCK_MASK];
         }

        /* Table Lock ���� */
        IDE_TEST(smlLockMgr::lockTable(
                        mTrans->mSlotN,
                        (smlLockItem *)( SMC_TABLE_LOCK( (smcTableHeader *)mTable ) ),
                        sLockMode,
                        mCursorProp.mLockWaitMicroSec,
                        &mLockMode,
                        &sLocked,
                        aCurLockNodePtr,
                        aCurLockSlotPtr) != IDE_SUCCESS );

        IDE_TEST(sLocked != ID_TRUE);

        // ���̺�� ���õ� Index, Blob ���̺����̽��鿡 ���Ͽ�
        // INTENTION ����� ȹ���Ѵ�.
        IDE_TEST( sctTableSpaceMgr::lockAndValidateRelTBSs(
                          (void*)mTrans,
                          (void*)mTable,
                          SCT_VAL_DDL_DML,
                          ID_TRUE,   /* intent lock  ���� */
                          sIsExclusive, /* exclusive lock */
                          mCursorProp.mLockWaitMicroSec)
                  != IDE_SUCCESS );

        /*
           Table�� ��û�� Lock Mode�� �ٸ��� Lock�� ������ �� �ִ�.
           �ֳ��ϸ� �������� Transaction�� Table�� �پ��� Lock�� ���
           ���� �ϳ��� Transaction ���� ���� Mode�� Lock�� ������ �� �ֱ�
           �����̴�.
           ������ mFlag�� Table�� �ɷ��ִ� Lock Mode�� ��ȭ������� �Ѵ�.

           Ex) Table T1�� �ִٰ� ����.
               Tx2�� Lock Table T1 In S Mode �� ���� ��
               Tx1�� Lock Table T1 In IS Mode �� �ϸ�
               �̸� Tx1�� ��û�� Lock Mode�� S Mode�� �����ȴ�.
        */
        mFlag = (mFlag & (~SMI_LOCK_MASK)) |
            smiTableCursorLock[mLockMode][mFlag&SMI_LOCK_MASK];
    }
    else
    {
        mLockMode = SML_IXLOCK;
    }

    if( ((mFlag & SMI_LOCK_MASK) == SMI_LOCK_WRITE ) ||
        ((mFlag & SMI_LOCK_MASK) == SMI_LOCK_TABLE_EXCLUSIVE ) )
    {
        /* Change����(Insert, Update, Delete)�� �����ϴ� Cursor */
        // PROJ-2199 SELECT func() FOR UPDATE ����
        // SMI_STATEMENT_FORUPDATE �߰�
        if( ((mStatement->mFlag & SMI_STATEMENT_MASK) == SMI_STATEMENT_NORMAL) ||
            ((mStatement->mFlag & SMI_STATEMENT_MASK) == SMI_STATEMENT_FORUPDATE) )
        {
            IDE_TEST(mTrans->getTableInfo(
                         sTable->mSelfOID,
                         &mTableInfo) != IDE_SUCCESS);
        }

        mUntouchable = ID_FALSE;

        mInsert  = (smTableCursorInsertFunc)smiTableDMLFunctions[ sTableTypeID ].mInsert;
        mUpdate  = (smTableCursorUpdateFunc)smiTableDMLFunctions[ sTableTypeID ].mUpdate;
        mRemove  = (smTableCursorRemoveFunc)smiTableDMLFunctions[ sTableTypeID ].mRemove;
        mLockRow = (smTableCursorLockRowFunc)sIndexModule->mLockRow;
    }
    else
    {
        if((mFlag & SMI_LOCK_MASK) == SMI_LOCK_REPEATABLE)
        {
            /* Lock Record And LOB Update */
            mUntouchable = ID_FALSE;
        }
        else
        {
            /* ReadOnly Cursor */
            mUntouchable = ID_TRUE;
        }

        mInsert = (smTableCursorInsertFunc)smiTableCursorInsertNA1Function;
        mUpdate = (smTableCursorUpdateFunc)smiTableCursorUpdateNA1Function;
        mRemove = (smTableCursorRemoveFunc)smiTableCursorRemoveNA1Function;
    }

    if( (mFlag & SMI_INSERT_METHOD_MASK) == SMI_INSERT_METHOD_APPEND )
    {
        IDE_ERROR( ((smcTableHeader*)mTable)->mSelfOID != SM_NULL_OID );
        IDE_ERROR( mTableInfo != NULL );

        if( smxTableInfoMgr::isExistDPathIns(mTableInfo) == ID_TRUE )
        {
            /* �̹� DPath INSERT�� ������ table�� ǥ�õǾ� ����. */
        }
        else
        {
            /* ��� table�� ���� ������ DPath INSERT�� ������ ��, table info��
             * DPath INSERT�� ������ table�̶�� ǥ���ϰ�, abort�� �� ǥ�ø�
             * �ǵ��� NTA log�� ���ܵд�. */
            smxTableInfoMgr::setExistDPathIns( mTableInfo,
                                               ID_TRUE ); /* aExistDPathIns */

            /* rollback �Ǹ� mExistDPathIns �÷��׸� ID_FALSE�� ������ �ֱ�
             * ���� NTA log�� �����. */
            IDE_TEST( smrLogMgr::writeNTALogRec(
                                mTrans->mStatistics,
                                mTrans,
                                smxTrans::getTransLstUndoNxtLSNPtr(mTrans),
                                smcTable::getSpaceID(mTable),
                                SMR_OP_DIRECT_PATH_INSERT,
                                sTable->mSelfOID )
                      != IDE_SUCCESS );
        }
    }

    /* Statement�� cursor�� ����ϰ� infinite SCN �� undo no�� ���´�. */
    IDE_TEST( mStatement->openCursor( this, &mIsSoloCursor )
              != IDE_SUCCESS );

    /* PROJ-2694 Fetch Across Rollback 
     * rollback�� cursor ������ �������� ���θ� �Ǵ��ϱ� ���� 
     * holdable�� cursor open�� Infinite SCN�� �����صд�. */
    if( ( mTrans->mIsCursorHoldable ) == ID_TRUE )
    {
        SM_SET_SCN( &mTrans->mCursorOpenInfSCN, &mInfinite );

        /* ���� cursor open�� holdable�� �ƴ� ��쿡�� holdable�� �Ǵ����� �ʵ���
         * mIsCursorHoldable ������ false�� �����Ѵ�. */
        mTrans->mIsCursorHoldable = ID_FALSE;
    }

    return IDE_SUCCESS;

    /* Ŀ���� �̹� ���µǾ� �ֽ��ϴ�. */
    IDE_EXCEPTION( ERR_ALREADY_OPENED );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_smiCursorOpened ) );
    }
    IDE_EXCEPTION( err_disabled_index);
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_DISABLED_INDEX ));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if( (mIndexModule != NULL) && (mIterator != NULL) )
    {
        IDE_ASSERT(((smnIndexModule*)mIndexModule)->mFreeIterator((void*)mIterator)
                   == IDE_SUCCESS);

        mIterator = NULL;
    }

    init();

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� Open�� Cursor�� Close���� �ʰ� Key Range,Fileter�� �ٲ㼭
 *               �����ϱ� ���� ���ȵǾ���.���� ����Ÿ�� �о���̰� ã�°��� Iterator��
 *               �ϱ� ������ ���ο� Key Range�� Filter�� �̿��� Iterator�� �ʱ�ȭ�Ѵ�.
 *
 * aKeyRange   - [IN] ���ο� Key Range
 * aKeyFilter  - [IN] ���ο� Key Filter
 * aRowFilter  - [IN] Filter
 *
 **********************************************************************/
IDE_RC smiTableCursor::restart( const smiRange *    aKeyRange,
                                const smiRange *    aKeyFilter,
                                const smiCallBack * aRowFilter )
{
    UInt sTableType;

    IDE_ERROR( aKeyRange  != NULL );
    IDE_ERROR( aKeyFilter != NULL );
    IDE_ERROR( aRowFilter != NULL );

    IDE_TEST_RAISE( mIterator == NULL, ERR_CLOSED );

    mCursorProp = mOrgCursorProp;

    // BUG-41560
    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    if ( sTableType != SMI_TABLE_FIXED )
    {
        IDE_TEST( ((smnIndexModule*)mIndexModule)->mDestIterator( mIterator ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* FOR A4 : smiCursorProperties�� Iterator���� �κ� ���� */
    IDE_TEST( ((smnIndexModule*)mIndexModule)->mInitIterator( mIterator,
                                                              mTrans,
                                                              mTable,
                                                              mIndex,
                                                              mDumpObject,
                                                              aKeyRange,
                                                              aKeyFilter,
                                                              aRowFilter,
                                                              mFlag,
                                                              mSCN,
                                                              mInfinite,
                                                              mUntouchable,
                                                              &mCursorProp,
                                                              &mSeekFunc,
                                                              mStatement )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CLOSED );
    IDE_SET( ideSetErrorCode( smERR_FATAL_smiCursorNotOpened ) );
    IDE_EXCEPTION_END;

    init();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Cursor�� Close�ϰ� �Ҵ�� Iterator�� Free�Ѵ�.
 *
 **********************************************************************/
IDE_RC smiTableCursor::close( void )
{
    SInt sState = 2;

    //BUG-27609 CodeSonar::Null Pointer Dereference (8)
    IDE_ERROR( this   != NULL );
    IDE_ERROR( mTrans != NULL );

    if(mTrans != NULL)
    {
        IDU_FIT_POINT( "smiTableCursor::close::closeFunc" );

        // Cursor�� Close�Ѵ�.
        IDE_TEST( mOps.closeFunc(this) != IDE_SUCCESS );
    }

    if(mStatement != NULL)
    {
        // Statement���� Cursor�� �����Ѵ�.
        mStatement->closeCursor( this );
    }

    sState = 1;

    if(mIterator != NULL)
    {
        // �Ҵ�� Iterator�� Free�Ѵ�.
        IDE_TEST(((smnIndexModule*)mIndexModule)->mFreeIterator((void*)mIterator)
                 != IDE_SUCCESS );
        mIterator = NULL;
    }

    sState = 0;

    // PROJ-1509
    // init();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 2 :
        {
            if(mStatement != NULL)
            {
                mStatement->closeCursor( this );
            }
        }
        case 1 :
        {
            if(mIterator != NULL)
            {
                IDE_ASSERT(((smnIndexModule*)mIndexModule)->mFreeIterator((void*)mIterator)
                           == IDE_SUCCESS);
                mIterator = NULL;
            }
        }
    }

    init();

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Cursor Key Range, Key Filter ������ �����ϴ� ù��° Data�� ã�Ƽ�
 *               Cursor�� ��ġ��Ų��.
 *
 **********************************************************************/
IDE_RC smiTableCursor::beforeFirst( void )
{
    IDE_DASSERT( checkVisibilityConsistency() == IDE_SUCCESS );

    if ( mIsDequeue == ID_FALSE )
    {
        // Queue �ƴ� ��
        IDE_TEST( mSeekFunc[SMI_FIND_BEFORE]( mIterator,
                                              (const void**)&mSeekFunc )
                  != IDE_SUCCESS );  

        mSeek[0] = mSeekFunc[0];
        mSeek[1] = mSeekFunc[1];
    }
    else
    {
        //BUG-48230: Queue �� ��
        if ( ( mFlag & SMI_TRAVERSE_MASK ) == SMI_TRAVERSE_FORWARD )
        {
            //SMI_TRAVERSE_FORWARD
            IDE_TEST( smnbBTree::beforeFirstQ( mIterator )
                      != IDE_SUCCESS );
            mSeek[0] = (smSeekFunc)smnbBTree::fetchNextQ;
            mSeek[1] = (smSeekFunc)smnbBTree::NA;
        }
        else
        {
            //SMI_TRAVERSE_BACKWARD
            IDE_TEST( smnbBTree::afterLastQ( mIterator )
                      != IDE_SUCCESS );

            mSeek[0] = (smSeekFunc)smnbBTree::fetchPrev;
            mSeek[1] = (smSeekFunc)smnbBTree::NA;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    init();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Cursor Key Range, Key Filter ������ �����ϴ� ������ Data�� ã�Ƽ�
 *               Cursor�� ��ġ��Ų��.
 *
 **********************************************************************/
IDE_RC smiTableCursor::afterLast( void )
{
    IDE_DASSERT( checkVisibilityConsistency() == IDE_SUCCESS );

    IDE_TEST( mSeekFunc[SMI_FIND_AFTER]( mIterator,
                                         (const void**)&mSeekFunc )
              != IDE_SUCCESS );

    mSeek[0] = mSeekFunc[0];
    mSeek[1] = mSeekFunc[1];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    init();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Cursor Key Range, Key Filter ������ �����ϴ� ���� Row��
 *               �о�´�.MMDB�� ��쿡�� Row Pointer��, DRDB�� ��쿡��
 *               aRow�� �Ҵ�� Buffer�� Row�� �����Ͽ� �ش�.
 *
 * aRow  - [OUT] if DRDB, row�� ����Ÿ�� aRow�� �����Ͽ� �ش�.MMDB��
 *                  Row�� Pointer�� �־��ش�.
 * aGRID  - [OUT] Row�� RID�� Setting�Ѵ�.
 * aFlag - ���� ��ġ���� ����, ����, ��������Ÿ�� �������� ���� �����Ѵ�.
 *
 *          1.SMI_FIND_NEXT : ���� Row�� Fetch
 *          2.SMI_FIND_PREV : ���� Row�� Fetch
 *          3.SMI_FIND_CURR : ���� Row�� Fetch
 *
 **********************************************************************/
IDE_RC smiTableCursor::readRow( const void  ** aRow,
                                scGRID       * aRowGRID,
                                UInt           aFlag )
{
    IDE_DASSERT( checkVisibilityConsistency() == IDE_SUCCESS );

    IDU_FIT_POINT( "smiTableCursor::readRow::mSeek" );

    IDE_TEST( mSeek[aFlag&SMI_FIND_MASK]( mIterator, aRow )
              != IDE_SUCCESS );

    SC_COPY_GRID( mIterator->mRowGRID, *aRowGRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    init();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: �־��� GRID�� �ش��ϴ� row�� fetch�Ѵ�.
 *
 * Parameters:
 *  aRow        - [IN] row�� ������ buffer
 *  aRowGRID    - [IN] fetch ��� row�� GRID
 **********************************************************************/
IDE_RC smiTableCursor::readRowFromGRID( const void  ** aRow,
                                        scGRID         aRowGRID )
{
    idvSQL    * sStatistics = smxTrans::getStatistics( mTrans );
    UInt        sTableType  = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );
    UChar     * sSlotPtr;
    idBool      sIsSuccess;
    idBool      sIsPageLatchReleased = ID_TRUE;
    idBool      sIsRowDeleted;

    IDE_ERROR( aRow != NULL );
    IDE_ERROR( SC_GRID_IS_NOT_NULL(aRowGRID) );
    IDE_ERROR( sTableType == SMI_TABLE_DISK );

    IDE_TEST( sdbBufferMgr::getPageByGRID( sStatistics,
                                           aRowGRID,
                                           SDB_S_LATCH,
                                           SDB_WAIT_NORMAL,
                                           NULL, /* aMtx */
                                           (UChar**)&sSlotPtr,
                                           &sIsSuccess )
              != IDE_SUCCESS );
    sIsPageLatchReleased = ID_FALSE;

    IDE_ERROR( sdpPhyPage::getPageType( sdpPhyPage::getHdr(sSlotPtr) )
               == SDP_PAGE_DATA);

    IDU_FIT_POINT( "smiTableCursor::readRowFromGRID::fetch" );

    IDE_TEST( sdcRow::fetch( sStatistics,
                             NULL, /* aMtx */
                             NULL, /* aSP */
                             mTrans,
                             SC_MAKE_SPACE(aRowGRID),
                             sSlotPtr,
                             ID_TRUE, /* aIsPersSlot */
                             SDB_SINGLE_PAGE_READ,
                             mCursorProp.mFetchColumnList,
                             SMI_FETCH_VERSION_CONSISTENT,
                             smxTrans::getTSSlotSID(mTrans),
                             &mSCN,
                             &mInfinite,
                             NULL, /* aIndexInfo4Fetch */
                             NULL, /* aLobInfo4Fetch */
                             ((smcTableHeader*)mTable)->mRowTemplate,
                             (UChar*)*aRow,
                             &sIsRowDeleted,
                             &sIsPageLatchReleased )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sIsRowDeleted == ID_TRUE, error_deletedrow );

    if( sIsPageLatchReleased == ID_FALSE )
    {
        sIsPageLatchReleased = ID_TRUE;
        IDE_TEST( sdbBufferMgr::releasePage( sStatistics,
                                             sSlotPtr )
                  != IDE_SUCCESS );
    }

    SC_COPY_GRID( aRowGRID, mIterator->mRowGRID );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( error_deletedrow );
    {
        ideLog::log( 
            IDE_SM_0, 
            "InternalError[%s:%d]\n"
            "GRID     : <%u,%u,%u>\n"
            "TSSRID   : %llu\n"
            "SCN      : %llu, %llu\n",
            (SChar *)idlVA::basename(__FILE__),
            __LINE__,
            SC_MAKE_SPACE( aRowGRID ),
            SC_MAKE_PID( aRowGRID ),
            SC_MAKE_OFFSET( aRowGRID ),
            smxTrans::getTSSlotSID(mTrans),
            SM_SCN_TO_LONG(mSCN),
            SM_SCN_TO_LONG(mInfinite) );
        ideLog::logCallStack( IDE_SM_0 );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INTERNAL_ARG, 
                                  "Disk Fetch by RID" ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( sStatistics,
                                               sSlotPtr )
                    == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: �־��� row, GRID�� �ش��ϴ� ���ڵ带 ��ġ��Ų��.
 *
 * Parameters:
 *  aRow     - [IN] fetch ��� row�� pointer
 *  aRowGRID - [IN] fetch ��� row�� GRID
 **********************************************************************/
IDE_RC smiTableCursor::setRowPosition( void   * aRow,
                                       scGRID   aRowGRID )
{
    UInt  sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    if ( sTableType == SMI_TABLE_DISK )
    {
        IDE_ERROR( SC_GRID_IS_NOT_NULL(aRowGRID) );
        
        SC_COPY_GRID( aRowGRID, mIterator->mRowGRID );
    }
    else
    {
        mIterator->curRecPtr = (SChar*) aRow;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ �����ϴ� Table�� Record ������ ���Ѵ�.
 *
 * aRow       - [IN]  if DRDB, aRow�� Buffer�� �Ҵ��ؼ� �Ѱ��־�� �Ѵ�.
 * aRowCount  - [OUT] Iterator�� ������ ������ �����ϴ� Record������ ���Ѵ�.
 **********************************************************************/
IDE_RC smiTableCursor::countRow( const void ** aRow,
                                 SLong *       aRowCount )
{
    *aRowCount = 0;

    while(1)
    {
        IDE_DASSERT( checkVisibilityConsistency() == IDE_SUCCESS );

        IDE_TEST( mSeek[SMI_FIND_NEXT]( mIterator, aRow )
                  != IDE_SUCCESS );
        if( *aRow == NULL )
        {
            break;
        }

        // To Fix PR-8113
        // ������ �켱 ���� ���� ������.
        (*aRowCount)++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    init();

    IDE_POP();

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : aValue�� �ش��ϴ� Row�� Table�� Insert�Ѵ�.
 * Implementation :
 *    mOps.insertRowFunc�� �Ϲ� insert�� normalInsertRow() �Լ���
 *    direct-path insert�� dpathInsertRow() �Լ��� ����Ǿ� ����
 *    - Input
 *      aValueList : insert �� value
 *      aFlag      : flag ����
 *    - Output
 *      aRow       : insert �� record pointer
 *      aGRID      : insert �� record GRID
 **********************************************************************/
IDE_RC smiTableCursor::insertRow( const smiValue  * aValueList,
                                  void           ** aRow,
                                  scGRID          * aGRID )
{
    IDU_FIT_POINT( "smiTableCursor::insertRow" );

    IDE_ERROR( this != NULL );
    IDE_ERROR( aValueList != NULL );

    IDE_TEST( mOps.insertRowFunc(this,
                                 aValueList,
                                 aRow,
                                 aGRID)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �Ϲ� Insert Row �Լ�
 * Implementation :
 *    cursor�� ���� insert�� ������
 *
 *    - Input
 *      aValueList : insert �� value
 *      aFlag      : flag ����
 *    - Output
 *      aRow       : insert �� record pointer
 *      aGRID      : insert �� record GRID
 **********************************************************************/
IDE_RC smiTableCursor::normalInsertRow(smiTableCursor  * aCursor,
                                       const smiValue  * aValueList,
                                       void           ** aRow,
                                       scGRID          * aGRID)
{
    UInt             sTableType;
    UInt             sFlag   = 0;
    SChar          * sRecPtr = NULL;
    scGRID           sRowGRID;
    smcTableHeader * sTable;
    SChar          * sNullPtr;
    UInt             sIndexCnt;

    
    sTable     = (smcTableHeader*)aCursor->mTable;
    sIndexCnt  = smcTable::getIndexCount(sTable);
    sTableType = SMI_GET_TABLE_TYPE( sTable );

    if( aCursor->mCursorProp.mIsUndoLogging == ID_TRUE )
    {
        sFlag = SM_FLAG_INSERT_LOB;
    }
    else
    {
        sFlag = SM_FLAG_INSERT_LOB_UNDO;
    }

    if( aCursor->mNeedUndoRec == ID_TRUE )
    {
        sFlag |= SM_INSERT_NEED_INSERT_UNDOREC_OK;
    }
    else
    {
        sFlag |= SM_INSERT_NEED_INSERT_UNDOREC_NO;
    }

    // mInfinite�� smiStatement���� ���õȴ�.
    IDE_TEST( aCursor->mInsert( aCursor->mCursorProp.mStatistics,
                                aCursor->mTrans,
                                aCursor->mTableInfo,
                                aCursor->mTable,
                                aCursor->mInfinite,
                                &sRecPtr,
                                &sRowGRID,
                                aValueList,
                                sFlag )
              != IDE_SUCCESS );

    // PROJ-1705
    if( sIndexCnt > 0 )
    {
        if( sTableType == SMI_TABLE_DISK )
        {
            IDE_TEST( insertKeyIntoIndicesForInsert( aCursor,
                                                     sRowGRID,
                                                     aValueList )
                      != IDE_SUCCESS );
        }

        /* BUG-31417
         * when insert after row trigger on memory table is called,
         * can not reference new inserted values of INSERT SELECT statement.
         */
        else if( (sTableType == SMI_TABLE_MEMORY) ||
                 (sTableType == SMI_TABLE_META) )
        {
            IDE_ERROR( smmManager::getOIDPtr( sTable->mSpaceID,
                                              sTable->mNullOID, 
                                              (void**)&sNullPtr)
                       == IDE_SUCCESS );

            IDE_TEST( insertKeyIntoIndices( aCursor,
                                            sRecPtr,
                                            SC_NULL_GRID,
                                            sNullPtr,
                                            NULL )
                      != IDE_SUCCESS );

            IDE_TEST( setAgingIndexFlag( aCursor, 
                                         sRecPtr, 
                                         SM_OID_ACT_AGING_INDEX )
                      != IDE_SUCCESS );
        }
        else 
        {
            IDE_ERROR( sTableType == SMI_TABLE_VOLATILE );
            
            IDE_ERROR( smmManager::getOIDPtr( sTable->mSpaceID,
                                              sTable->mNullOID, 
                                              (void**)&sNullPtr)
                       == IDE_SUCCESS );

            IDE_TEST( insertKeyIntoIndices( aCursor,
                                            sRecPtr,
                                            SC_NULL_GRID,
                                            sNullPtr,
                                            NULL )
                      != IDE_SUCCESS );

            IDE_TEST( setAgingIndexFlag( aCursor, 
                                         sRecPtr, 
                                         SM_OID_ACT_AGING_INDEX )
                      != IDE_SUCCESS );

        }
    }

    if ( aCursor->mCursorType != SMI_COMPOSITE_CURSOR )
    {
        if( sTableType == SMI_TABLE_DISK )
        {
            aCursor->mLstInsRowGRID = sRowGRID;
        }
        else
        {
            aCursor->mLstInsRowPtr  = sRecPtr;
        }
    }
    else
    {
        // nothing to do
    }

    if( sTableType == SMI_TABLE_DISK )
    {
        SC_COPY_GRID( sRowGRID, /* Source GRID */
                      *aGRID ); /* Dest   GRID */
    }
    else
    {
        *aRow = sRecPtr;
        SC_COPY_GRID( sRowGRID, *aGRID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    aCursor->init();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Direct-Path Insert Row �Լ�
 *
 * Implementation :
 *      (1) DPath Insert�� ���� TX ������ �ѹ��� �ʱ�ȭ ���� �ʾ�����,
 *          �ʱ�ȭ ���� �����Ѵ�.
 *      (2) DPath Insert�� �����Ѵ�.
 *
 * Parameters
 *      aCursor     - [IN] insert ������ cursor
 *      aValueList  - [IN] insert �� value
 ******************************************************************************/
IDE_RC smiTableCursor::dpathInsertRow(smiTableCursor  * aCursor,
                                      const smiValue  * aValueList,
                                      void           ** /*aRow*/,
                                      scGRID          * aGRID)
{
    idvSQL            * sStatistics;
    sdrMtxStartInfo     sStartInfo;
    smOID               sTableOID;
    scGRID              sRowGRID;

    IDE_ASSERT( SMI_TABLE_TYPE_IS_DISK( (smcTableHeader*)aCursor->mTable ) == ID_TRUE );

    IDU_FIT_POINT( "2.PROJ-1665@smiTableCursor::dpathInsertRow" );

    sStatistics = aCursor->mCursorProp.mStatistics;

    // TSS �Ҵ�
    sStartInfo.mTrans   = aCursor->mTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( smxTrans::allocTXSegEntry( sStatistics,
                                         &sStartInfo )
              != IDE_SUCCESS );

    // DPathEntry�� �Ҵ�޾Ƽ� Transaction�� �޾Ƶд�.
    IDE_TEST( smxTrans::allocDPathEntry( aCursor->mTrans )
              != IDE_SUCCESS );

    // DPathSegInfo�� �޾ƿ´�.
    if( aCursor->mDPathSegInfo == NULL )
    {
        sTableOID = smcTable::getTableOID( aCursor->mTable );
        IDE_TEST( sdcDPathInsertMgr::allocDPathSegInfo(
                                                sStatistics,
                                                aCursor->mTrans,
                                                sTableOID,
                                                &aCursor->mDPathSegInfo )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // insert�� ����
    //------------------------------------------
    IDE_TEST( sdcRow::insertAppend( sStatistics,
                                    aCursor->mTrans,
                                    aCursor->mDPathSegInfo,
                                    aCursor->mTable,
                                    aCursor->mInfinite,
                                    aValueList,
                                    &sRowGRID )
              != IDE_SUCCESS );

    
    //------------------------------------------
    // iterator ����
    //------------------------------------------
    SC_COPY_GRID( sRowGRID, aCursor->mLstInsRowGRID );

    SC_COPY_GRID( sRowGRID, *aGRID );
    
    IDU_FIT_POINT( "1.PROJ-1665@smiTableCursor::dpathInsertRow");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2264
IDE_RC smiTableCursor::insertRowWithIgnoreUniqueError( smiTableCursor  * aCursor,
                                                       smcTableHeader  * aTableHeader,
                                                       smiValue        * aValueList,
                                                       smOID           * aOID,
                                                       void           ** aRow )
{
    UInt              sTableType;
    SChar           * sRowPtr = NULL;
    scGRID            sRowGRID;
    SChar           * sNullPtr;
    UInt              sIndexCnt;
    smxSavepoint    * sISavepoint = NULL;
    scPageID          sPageID;
    smOID             sOID;
    SChar           * sExistUniqueRow;
    idBool            sUniqueOccurrence = ID_FALSE;
    smLSN             sLsnNTA;
    smSCN             sSCN;
    const smiColumn * sColumn;
    UInt              sState = 0;

    IDE_ERROR( aCursor      != NULL );
    IDE_ERROR( aTableHeader != NULL );
    // BUG-36718
    IDE_ERROR( aRow != NULL );

    sIndexCnt  = smcTable::getIndexCount( aTableHeader );
    sTableType = SMI_GET_TABLE_TYPE( aTableHeader );
    sColumn    = smcTable::getColumn(aTableHeader, 0);

    // BUG-39378
    // setImpSavepoint4Retry�Լ��� �̿��� ImpSavepoint�� ���� �Ѵ�.
    IDE_TEST( smxTrans::setImpSavepoint4Retry( aCursor->mTrans,
                                               (void**)&sISavepoint ) 
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smlLockMgr::lockTableModeIX( aCursor->mTrans,
                                           SMC_TABLE_LOCK( aTableHeader ) )
              != IDE_SUCCESS );

    sLsnNTA = smxTrans::getTransLstUndoNxtLSN( aCursor->mTrans );

    IDE_TEST( smcRecord::insertVersion( NULL,   //aStatistics
                                        aCursor->mTrans,
                                        aCursor->mTableInfo,
                                        aTableHeader,
                                        aCursor->mInfinite,
                                        &sRowPtr,
                                        NULL,   //aRetInsertSlotGRID
                                        aValueList,
                                        (SM_FLAG_INSERT_LOB |
                                         SM_INSERT_NEED_INSERT_UNDOREC_OK) )
              != IDE_SUCCESS );

    sPageID = SMP_SLOT_GET_PID( sRowPtr );
    sOID    = SM_MAKE_OID( sPageID,
                           SMP_SLOT_GET_OFFSET( (smpSlotHeader*)sRowPtr ) );

    if( sIndexCnt > 0 )
    {
        if( sTableType == SMI_TABLE_MEMORY )
        {
            SC_MAKE_NULL_GRID( sRowGRID );

            IDE_ERROR( smmManager::getOIDPtr( aTableHeader->mSpaceID,
                                              aTableHeader->mNullOID, 
                                              (void**)&sNullPtr)
                       == IDE_SUCCESS );

            if( insertKeyIntoIndices( aCursor,
                                      sRowPtr,
                                      sRowGRID,
                                      sNullPtr,
                                      &sExistUniqueRow ) != IDE_SUCCESS )
            {
                if( ideGetErrorCode() == smERR_ABORT_smnUniqueViolation )
                {
                    SMX_INC_SESSION_STATISTIC( aCursor->mTrans,
                                               IDV_STAT_INDEX_UPDATE_RETRY_COUNT,
                                               1 );

                    smcTable::incStatAtABort( aTableHeader,
                                              SMC_INC_UNIQUE_VIOLATION_CNT );

                    sUniqueOccurrence = ID_TRUE;

                    IDE_TEST( aCursor->mTrans->abortToImpSavepoint( sISavepoint )
                              != IDE_SUCCESS );
                }
                else
                {
                    // if others error
                    IDE_RAISE( IDE_EXCEPTION_END_LABEL );
                }
            }
            else
            {
                // if insert index success

                // PROJ-2429 Dictionary based data compress for on-disk DB
                // disk dictionary compression column�� ��� dictionary table�� 
                // ������ ���������� �ش� record�� rollback�Ǹ� �ȵȴ�. 
                // ���� rollback�� aging���� �ʵ��� ���� �ٸ� Tx�� �ش� record�� 
                // �� ���ְ� �ϱ� ���� record�� createSCN�� 0���� ���� �Ѵ�.
                if ( (sColumn->flag & SMI_COLUMN_COMPRESSION_TARGET_MASK)
                     == SMI_COLUMN_COMPRESSION_TARGET_DISK  )
                {
                    IDE_TEST( setAgingIndexFlag( aCursor, 
                                                 sRowPtr, 
                                                 SM_OID_ACT_COMPRESSION )
                              != IDE_SUCCESS );

                    SM_INIT_SCN( &sSCN );
                    IDE_TEST(smcRecord::setSCNLogging( aCursor->mTrans,
                                                       aTableHeader,
                                                       (SChar*)sRowPtr,
                                                       sSCN)
                             != IDE_SUCCESS);

                    IDE_TEST( smrLogMgr::writeNTALogRec(NULL, /* idvSQL* */
                                                        aCursor->mTrans,
                                                        &sLsnNTA)
                            != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( setAgingIndexFlag( aCursor, 
                                                 sRowPtr, 
                                                 SM_OID_ACT_AGING_INDEX )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // if not memory db
            IDE_ASSERT(0);
        }
    }

    sState = 0;
    IDE_TEST( aCursor->mTrans->unsetImpSavepoint( sISavepoint )
              != IDE_SUCCESS );

    if( sUniqueOccurrence == ID_TRUE )
    {
        *aOID = SM_MAKE_OID( SMP_SLOT_GET_PID(sExistUniqueRow),
                             SMP_SLOT_GET_OFFSET((smpSlotHeader*)sExistUniqueRow) );

        *aRow = sExistUniqueRow;
    }
    else
    {
        *aOID = sOID;
        *aRow = sRowPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-39378 ImpSavepoint is set twice in a smiStatment 
       when inserting a data to dictionary table */
    if ( sState == 1 )
    {
        IDE_ASSERT( aCursor->mTrans->unsetImpSavepoint( sISavepoint )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���������� Insert�� Record ID�� �����´�.
 **********************************************************************/
scGRID  smiTableCursor::getLastInsertedRID( )
{
    IDE_DASSERT( mCursorType == SMI_INSERT_CURSOR );
    IDE_DASSERT( SC_GRID_IS_NOT_NULL( mLstInsRowGRID ) );

    return mLstInsRowGRID;
}

/***********************************************************************
 * Description : ���� Cursor�� Iterator�� ����Ű�� Row�� update�Ѵ�.
 *
 * aValueList - [IN] �������� Column�� Value List
 * aRetryInfo - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::updateRow( const smiValue        * aValueList,
                                  const smiDMLRetryInfo * aRetryInfo )
{
    IDE_ERROR_RAISE( this      != NULL, ERR_CLOSED );
    IDE_ERROR_RAISE( mIterator != NULL, ERR_CLOSED );

    prepareRetryInfo( &aRetryInfo );

    IDU_FIT_POINT( "smiTableCursor::updateRow" );

    IDE_TEST( mOps.updateRowFunc( this,
                                  aValueList,
                                  aRetryInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CLOSED );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCursorNotOpened ) );
    }
    IDE_EXCEPTION_END;

    if( ideGetErrorCode() != smERR_RETRY_Row_Retry )
    {
        /* Row Retry�� cursor�� close���� �ʰ�,
         * ������ row�� retry�ϴ°��̴�..
         * Cursor�� ��� ���ǹǷ� init()�ϸ� �ȵȴ�. */
        init();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BUG-39399
 *              ���� Cursor�� Iterator�� ����Ű�� Row��
 *              ���� �̹� update �ߴ��� ���θ� Ȯ��.
 *
 * aValueList - [IN]  �������� Column�� Value List
 * aRetryInfo - [IN]  retry ����
 * aRow       - [OUT] update �� row�� pointer
 * aGRID      - [OUT] update �� row�� GRID
 * aIsUpdatedByMe   - [OUT] ID_TRUE �̸�, aRow�� �̹� ���� statement����
 *                          ���� �̹� update �� row�̴�.
 **********************************************************************/
IDE_RC smiTableCursor::isUpdatedRowBySameStmt( idBool   * aIsUpdatedBySameStmt )
{
    smiTableCursor    * sCursor     = NULL;
    UInt                sTableType  = 0;

    IDE_ERROR_RAISE( this      != NULL, ERR_CLOSED );
    IDE_ERROR_RAISE( mIterator != NULL, ERR_CLOSED );
    IDE_ERROR_RAISE( aIsUpdatedBySameStmt != NULL, ERR_CLOSED );

    sCursor = (smiTableCursor *)this;

    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader *)(sCursor->mTable) );

    switch( sTableType )
    {
        case SMI_TABLE_MEMORY :
            IDE_TEST( smcRecord::isUpdatedVersionBySameStmt(
                                            sCursor->mTrans,
                                            sCursor->mSCN,
                                            (smcTableHeader *)sCursor->mTable,
                                            sCursor->mIterator->curRecPtr,
                                            sCursor->mInfinite,
                                            aIsUpdatedBySameStmt,
                                            sCursor->mStatement->isForbiddenToRetry() )
                      != IDE_SUCCESS );

            break;

        case SMI_TABLE_DISK :
            IDE_TEST( sdcRow::isUpdatedRowBySameStmt(
                                            sCursor->mIterator->properties->mStatistics,
                                            sCursor->mTrans,
                                            sCursor->mSCN,
                                            sCursor->mTable,
                                            sCursor->mIterator->mRowGRID,
                                            sCursor->mInfinite,
                                            aIsUpdatedBySameStmt )
                      != IDE_SUCCESS );

            break;

        case SMI_TABLE_VOLATILE :
            IDE_TEST( svcRecord::isUpdatedVersionBySameStmt(
                                            sCursor->mTrans,
                                            sCursor->mSCN,
                                            (smcTableHeader *)sCursor->mTable,
                                            sCursor->mIterator->curRecPtr,
                                            sCursor->mInfinite,
                                            aIsUpdatedBySameStmt,
                                            sCursor->mStatement->isForbiddenToRetry() )
                      != IDE_SUCCESS );

            break;

        default :
            break;
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CLOSED );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCursorNotOpened ) );
    }
    IDE_EXCEPTION_END;

    if( ideGetErrorCode() != smERR_RETRY_Row_Retry )
    {
        /* Row Retry�� cursor�� close���� �ʰ�,
         * ������ row�� retry�ϴ°��̴�..
         * Cursor�� ��� ���ǹǷ� init()�ϸ� �ȵȴ�. */
        init();
    }

    return IDE_FAILURE;
}

//PROJ-1362 Internal LOB
/***********************************************************************
 * Description : ���� Cursor�� Iterator�� ����Ű�� Row�� update�Ѵ�.
 *
 * aValueList - [IN]  �������� Column�� Value List
 * aRetryInfo - [IN]  retry ����
 * aRow       - [OUT] update �� row�� pointer
 * aGRID      - [OUT] update �� row�� GRID
 **********************************************************************/
IDE_RC smiTableCursor::updateRow(const smiValue        * aValueList,
                                 const smiDMLRetryInfo * aRetryInfo,
                                 void                 ** aRow,
                                 scGRID                * aGRID )
{
    IDE_ERROR_RAISE( this      != NULL, ERR_CLOSED );
    IDE_ERROR_RAISE( mIterator != NULL, ERR_CLOSED );

    prepareRetryInfo( &aRetryInfo );

    IDE_TEST( mOps.updateRowFunc( this,
                                  aValueList,
                                  aRetryInfo )
              != IDE_SUCCESS );

    // mIterator�� curRecPtr, RowRID�� �����Ǿ� �ִ�.
    if( aRow != NULL)
    {
        *aRow = mIterator->curRecPtr;
    }

    if( aGRID != NULL)
    {
        SC_COPY_GRID( mIterator->mRowGRID, /* Source GRID */
                      *aGRID );            /* Dest   GRID */

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CLOSED );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCursorNotOpened ) );
    }
    IDE_EXCEPTION_END;

    if( ideGetErrorCode() != smERR_RETRY_Row_Retry )
    {
        /* Row Retry�� cursor�� close���� �ʰ�,
         * ������ row�� retry�ϴ°��̴�..
         * Cursor�� ��� ���ǹǷ� init()�ϸ� �ȵȴ�. */
        init();
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ���� Cursor�� Iterator�� ����Ű�� Row�� �����Ѵ�.
 *
 * aRetryInfo - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::deleteRow( const smiDMLRetryInfo * aRetryInfo )
{
    IDE_ERROR_RAISE( this      != NULL, ERR_CLOSED );
    IDE_ERROR_RAISE( mIterator != NULL, ERR_CLOSED );

    prepareRetryInfo( &aRetryInfo );

    IDU_FIT_POINT( "smiTableCursor::deleteRow" );

    IDE_TEST( mOps.deleteRowFunc( this,
                                  aRetryInfo ) != IDE_SUCCESS );

    mIterator->curRecPtr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CLOSED );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiCursorNotOpened ) );
    }
    IDE_EXCEPTION_END;

    if( ideGetErrorCode() != smERR_RETRY_Row_Retry )
    {
        /* Row Retry�� cursor�� close���� �ʰ�,
         * ������ row�� retry�ϴ°��̴�..
         * Cursor�� ��� ���ǹǷ� init()�ϸ� �ȵȴ�. */
        init();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���������� update�� row�� �ֽ� ������ �����´�.
 *
 *   aRow     - [OUT] �ֽ� ������ row
 *   aRowGRID - [OUT] disk GRID
 **********************************************************************/
IDE_RC smiTableCursor::getLastRow( const void ** aRow,
                                   scGRID      * aRowGRID )
{
    UInt               sTableType;
    idBool             sTrySuccess;
    scGRID             sRowGRID;
    UChar             *sRow;
    idBool             sIsRowDeleted;
    idBool             sIsPageLatchReleased = ID_TRUE;

    IDE_ERROR( this != NULL );
    IDE_ERROR( (mCursorType == SMI_INSERT_CURSOR) ||
               (mCursorType == SMI_DELETE_CURSOR) ||
               (mCursorType == SMI_UPDATE_CURSOR) ||
               (mCursorType == SMI_COMPOSITE_CURSOR) );

    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    IDE_ERROR( (sTableType == SMI_TABLE_DISK)   ||
               (sTableType == SMI_TABLE_MEMORY) ||
               (sTableType == SMI_TABLE_VOLATILE) );

    if( sTableType == SMI_TABLE_DISK )
    {
        if( mCursorType == SMI_INSERT_CURSOR )
        {
            SC_COPY_GRID( mLstInsRowGRID, sRowGRID );
        }
        else
        {
            IDE_ERROR( mIterator != NULL );
            SC_COPY_GRID( mIterator->mRowGRID, sRowGRID );
        }

        IDE_TEST( sdbBufferMgr::getPageByGRID( mCursorProp.mStatistics,
                                               sRowGRID,
                                               SDB_S_LATCH,
                                               SDB_WAIT_NORMAL,
                                               NULL, /* aMtx */
                                               (UChar**)&sRow,
                                               &sTrySuccess )
                  != IDE_SUCCESS );
        sIsPageLatchReleased = ID_FALSE;

        IDE_TEST( sdcRow::fetch(
                      mCursorProp.mStatistics,
                      NULL, /* aMtx */
                      NULL, /* aSP */
                      mTrans,
                      SC_MAKE_SPACE(sRowGRID),
                      (UChar*)sRow,
                      ID_TRUE, /* aIsPersSlot */
                      SDB_SINGLE_PAGE_READ,
                      mCursorProp.mFetchColumnList,
                      SMI_FETCH_VERSION_LAST,
                      SD_NULL_SID, /* aMyTSSlotSID */
                      NULL,        /* aMyStmtSCN */
                      NULL,        /* aInfiniteSCN */
                      NULL,        /* aIndexInfo4Fetch */
                      NULL,        /* aLobInfo4Fetch */
                      ((smcTableHeader*)mTable)->mRowTemplate,
                      (UChar*)*aRow,
                      &sIsRowDeleted,
                      &sIsPageLatchReleased )
                  != IDE_SUCCESS );

        /* BUG-23319
         * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
        /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
         * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
         * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
         * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
         * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
        if( sIsPageLatchReleased == ID_FALSE )
        {
            sIsPageLatchReleased = ID_TRUE;
            IDE_TEST( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                                 sRow )
                      != IDE_SUCCESS );
        }
        *aRowGRID = sRowGRID;
    }
    else
    {
        /* BUG-22282: TC/Server/mm4/Project/PROJ-1518/iloader/AfterTrigger��
         * ������ �����մϴ�.
         *
         * Cursor�� Insert�ϰ�� mIterator�� Null�̰� ������ Insert�� Row��
         * mLstInsRowPtr�� ����Ǿ� �ִ�.
         * */
        if( mCursorType == SMI_INSERT_CURSOR )
        {
            *aRow = mLstInsRowPtr;
        }
        else
        {
            IDE_ERROR( mIterator != NULL );
            // ���������� update, insert�� row
            *aRow = mIterator->curRecPtr;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                               sRow )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::getLastModifiedRow( void ** aRowBuf,
                                           UInt    aLength )
{
    UInt               sTableType;
    idBool             sTrySuccess;
    scGRID             sRowGRID;
    UChar             *sRow;
    idBool             sIsRowDeleted;
    SChar             *sLstModifiedRow;
    idBool             sIsPageLatchReleased = ID_TRUE;

    IDE_ERROR( this    != NULL );
    IDE_ERROR( aRowBuf != NULL );
    IDE_ERROR( aLength > 0 );
    IDE_ERROR( (mCursorType == SMI_INSERT_CURSOR) ||
               (mCursorType == SMI_DELETE_CURSOR) ||
               (mCursorType == SMI_UPDATE_CURSOR) ||
               (mCursorType == SMI_COMPOSITE_CURSOR) );

    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    IDE_ERROR( (sTableType == SMI_TABLE_DISK)   ||
               (sTableType == SMI_TABLE_MEMORY) ||
               (sTableType == SMI_TABLE_VOLATILE) );

    if( sTableType == SMI_TABLE_DISK )
    {
        if( mCursorType == SMI_INSERT_CURSOR )
        {
            SC_COPY_GRID( mLstInsRowGRID, sRowGRID );
        }
        else
        {
            IDE_ERROR( mIterator != NULL );
            SC_COPY_GRID( mIterator->mRowGRID, sRowGRID );
        }

        IDE_TEST( sdbBufferMgr::getPageByGRID( mCursorProp.mStatistics,
                                               sRowGRID,
                                               SDB_S_LATCH,
                                               SDB_WAIT_NORMAL,
                                               NULL, /* aMtx */
                                               (UChar**)&sRow,
                                               &sTrySuccess )
                  != IDE_SUCCESS );
        sIsPageLatchReleased = ID_FALSE;

        IDE_TEST( sdcRow::fetch(
                      mCursorProp.mStatistics,
                      NULL, /* aMtx */
                      NULL, /* aSP */
                      mTrans,
                      SC_MAKE_SPACE(sRowGRID),
                      (UChar*)sRow,
                      ID_TRUE, /* aIsPersSlot */
                      SDB_SINGLE_PAGE_READ,
                      mCursorProp.mFetchColumnList,
                      SMI_FETCH_VERSION_LAST,
                      SD_NULL_SID, /* aMyTSSlotSID */
                      NULL,        /* aMyStmtSCN */
                      NULL,        /* aInfiniteSCN */
                      NULL,        /* aIndexInfo4Fetch */
                      NULL,        /* aLobInfo4Fetch */
                      ((smcTableHeader*)mTable)->mRowTemplate,
                      (UChar*)*aRowBuf,
                      &sIsRowDeleted,
                      &sIsPageLatchReleased )
                  != IDE_SUCCESS );

        /* BUG-23319
         * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
        /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
         * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
         * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
         * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
         * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
        if( sIsPageLatchReleased == ID_FALSE )
        {
            sIsPageLatchReleased = ID_TRUE;
            IDE_TEST( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                                 sRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* BUG-22282: TC/Server/mm4/Project/PROJ-1518/iloader/AfterTrigger��
         * ������ �����մϴ�.
         *
         * Cursor�� Insert�ϰ�� mIterator�� Null�̰� ������ Insert�� Row��
         * mLstInsRowPtr�� ����Ǿ� �ִ�.
         * */
        if( mCursorType == SMI_INSERT_CURSOR )
        {
            sLstModifiedRow = mLstInsRowPtr;
        }
        else
        {
            IDE_ERROR( mIterator != NULL );
            sLstModifiedRow = mIterator->curRecPtr;
        }

        if( sTableType == SMI_TABLE_MEMORY )
        {
            IDE_ERROR( aLength <= ((smcTableHeader*)mTable)->mFixed.mMRDB.mSlotSize );
        }
        else
        {
            // BUG-27869 VOLATILE ���̺� �����̽����� Ʈ���Ÿ� ����ϸ�
            //           ASSERT �� �׽��ϴ�.
            // Volatile Table�� ���� ������ �����ϴ�. �߰��մϴ�.
            IDE_DASSERT( sTableType == SMI_TABLE_VOLATILE );

            IDE_ERROR( aLength <= ((smcTableHeader*)mTable)->mFixed.mVRDB.mSlotSize );
        }
        /* PROJ-2375 Global Meta
         * ���ۿ� �������� �ʰ� �����͸� �����Ѵ�. 
         */ 
        *aRowBuf = sLstModifiedRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                               sRow )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::getModifiedRow( void   ** aRowBuf,
                                       UInt      aLength,
                                       void    * aRow,
                                       scGRID    aRowGRID )
{
    UInt               sTableType;
    idBool             sTrySuccess;
    UChar             *sRow;
    idBool             sIsRowDeleted;
    idBool             sIsPageLatchReleased = ID_TRUE;

    IDE_ERROR( this    != NULL );
    IDE_ERROR( aRowBuf != NULL );
    IDE_ERROR( aLength > 0 );
    IDE_ERROR( mCursorType == SMI_COMPOSITE_CURSOR );

    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)mTable );

    IDE_ERROR( (sTableType == SMI_TABLE_DISK)   ||
               (sTableType == SMI_TABLE_MEMORY) ||
               (sTableType == SMI_TABLE_VOLATILE) );

    if( sTableType == SMI_TABLE_DISK )
    {
        IDE_TEST( sdbBufferMgr::getPageByGRID( mCursorProp.mStatistics,
                                               aRowGRID,
                                               SDB_S_LATCH,
                                               SDB_WAIT_NORMAL,
                                               NULL, /* aMtx */
                                               (UChar**)&sRow,
                                               &sTrySuccess )
                  != IDE_SUCCESS );
        sIsPageLatchReleased = ID_FALSE;

        IDE_TEST( sdcRow::fetch(
                      mCursorProp.mStatistics,
                      NULL, /* aMtx */
                      NULL, /* aSP */
                      mTrans,
                      SC_MAKE_SPACE(aRowGRID),
                      (UChar*)sRow,
                      ID_TRUE, /* aIsPersSlot */
                      SDB_SINGLE_PAGE_READ,
                      mCursorProp.mFetchColumnList,
                      SMI_FETCH_VERSION_LAST,
                      SD_NULL_SID, /* aMyTSSSID */
                      NULL,        /* aMyStmtSCN */
                      NULL,        /* aInfiniteSCN */
                      NULL,        /* aIndexInfo4Fetch */
                      NULL,        /* aLobInfo4Fetch */
                      ((smcTableHeader*)mTable)->mRowTemplate,
                      (UChar*)*aRowBuf,
                      &sIsRowDeleted,
                      &sIsPageLatchReleased)
                  != IDE_SUCCESS );

        /* BUG-23319
         * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
        /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
         * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
         * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
         * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
         * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
        if( sIsPageLatchReleased == ID_FALSE )
        {
            sIsPageLatchReleased = ID_TRUE;
            IDE_TEST( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                                 sRow )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        if( sTableType == SMI_TABLE_MEMORY )
        {
            IDE_ERROR( aLength <= ((smcTableHeader*)mTable)->mFixed.mMRDB.mSlotSize );
        }
        else
        {
            // BUG-27869 VOLATILE ���̺� �����̽����� Ʈ���Ÿ� ����ϸ�
            //           ASSERT �� �׽��ϴ�.
            // Volatile Table�� ���� ������ �����ϴ�. �߰��մϴ�.
            IDE_DASSERT( sTableType == SMI_TABLE_VOLATILE );

            IDE_ERROR( aLength <= ((smcTableHeader*)mTable)->mFixed.mVRDB.mSlotSize );
        }
        
        /* PROJ-2375 Global Meta
         * ���ۿ� �������� �ʰ� �����͸� �����Ѵ�. 
         */
        *aRowBuf = aRow;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( mCursorProp.mStatistics,
                                               sRow )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Update�� Column������ ����ȴ�. ������ Update�� Update�Ǵ�
 *               Column�̿��� Column�� Index�� �ɷ� �ִ� ��쿡�� Index��
 *               ���� Update�� ���ʿ��ϴ�. ������ ���⼭�� ��ü Index�߿���
 *               Update�� Update�� �ʿ��� Index���� ã�Ƽ� �Ѱ��ش�.
 *
 * aIndexOID     - [IN] Index Header�� ������ �ִ� Variable Column OID
 * aIndexLength  - [IN] �� Varcolumn�� ����
 * aCols         - [IN] Update�Ǵ� Column List
 * aModifyIdxBit - [IN-OUT] Update�ؾߵ��� Index�� Bit�� �̿��� ǥ���ؼ�
 *                 �Ѱ��ش�.
 *                 ex) 110000000000 �̸� ù��° Index, �ι�° Index��
 *                     Update����� �ȴ�.
 **********************************************************************/
void smiTableCursor::makeUpdatedIndexList( smcTableHeader       * aTableHeader,
                                           const smiColumnList  * aCols,
                                           ULong                * aModifyIdxBit,
                                           ULong                * aCheckUniqueIdxBit )
{

    UInt                  i;
    UInt                  j;
    UInt                  sIndexCnt;
    smnIndexHeader      * sIndexCursor;
    const smiColumnList * sColumn;
    ULong               * sBytePtr            = aModifyIdxBit;
    ULong               * sCheckUniqueBytePtr = aCheckUniqueIdxBit;
    ULong                 sBitMask            = ((ULong)1 << 63);
    idBool                sDoModify;

    *aModifyIdxBit      = (ULong)0;
    *aCheckUniqueIdxBit = (ULong)0;

    sIndexCnt =  smcTable::getIndexCount(aTableHeader);

    // ����޴� �ε������� key insert
    for( i = 0; i < sIndexCnt;i++ )
    {
        sIndexCursor = (smnIndexHeader*) smcTable::getTableIndex(aTableHeader,i);
        
        sDoModify = ID_FALSE;
        
        if( aCols != NULL )
        {
            for( j = 0; j < sIndexCursor->mColumnCount; j++ )
            {
                sColumn = aCols;
                while( sColumn != NULL )
                {
                    if( sIndexCursor->mColumns[j] == sColumn->column->id )
                    {
                        sDoModify = ID_TRUE;
                        break;
                    }//if sIndexCursor
                    sColumn = sColumn->next;
                }//while
                if(sColumn != NULL) // found same column
                {
                    break;
                }//if
            }//for j
        }
        else
        {
            sDoModify = ID_TRUE;
        }
        
        if( sDoModify == ID_TRUE ) // found same column
        {
            // Update�ؾߵǴ� Index �߰�.
            *sBytePtr |= sBitMask;
            
            if( ((sIndexCursor->mFlag & SMI_INDEX_UNIQUE_MASK) ==
                 SMI_INDEX_UNIQUE_ENABLE) ||
                ((sIndexCursor->mFlag & SMI_INDEX_LOCAL_UNIQUE_MASK) ==
                 SMI_INDEX_LOCAL_UNIQUE_ENABLE) )
            {
                *sCheckUniqueBytePtr |= sBitMask;

            }
        }//if
        
        sBitMask = sBitMask >> 1;
    }//for i

}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::insertKeysWithUndoSID( smiTableCursor * aCursor )
{
    UChar          * sPage;
    UChar          * sSlotDirPtr;
    idBool           sTrySuccess;
    UChar          * sURHdr;
    SInt             sSlotCount;
    SInt             i;
    scGRID           sRowGRID;
    SInt             sStartSlotSeq;
    idBool           sFixPage = ID_FALSE;
    SInt             sIndexCnt;
    ULong          * sBytePtr;
    ULong            sBitMask;
    smcTableHeader * sTable;
    sdcUndoRecType   sType;
    sdcUndoRecFlag   sFlag;
    smOID            sTableOID;
    sdpSegMgmtOp   * sSegMgmtOp;
    sdpSegInfo       sSegInfo;
    sdpExtInfo       sExtInfo;
    scPageID         sSegPID;
    sdRID            sCurExtRID;
    scPageID         sCurPageID;

    IDE_ERROR( aCursor->mFstUndoRecSID != SD_NULL_SID );
    IDE_ERROR( aCursor->mFstUndoExtRID != SD_NULL_RID );
    IDE_ERROR( aCursor->mLstUndoRecSID != SD_NULL_SID );

    IDV_SQL_OPTIME_BEGIN(
        aCursor->mCursorProp.mStatistics,
        IDV_OPTM_INDEX_DRDB_DML_INDEX_OPER );

    sTable = (smcTableHeader*)aCursor->mTable;

    sIndexCnt = smcTable::getIndexCount(sTable);

    IDE_TEST_CONT( sIndexCnt == 0, SKIP_INSERT_KEY );

    /* ������ Index�� �ִ��� üũ */
    sBytePtr = &aCursor->mModifyIdxBit;
    sBitMask = ((ULong)1 << 63);

    for( i = 0; i < sIndexCnt; i++ )
    {
        if( ((*sBytePtr) & sBitMask) != 0 )
        {
            break;
        }
        sBitMask = sBitMask >> 1;
    }

    IDE_TEST_CONT( i == sIndexCnt, SKIP_INSERT_KEY );

    sSegMgmtOp =
        sdpSegDescMgr::getSegMgmtOp( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    sSegPID =
        smxTrans::getUDSegPtr(aCursor->mTrans)->getSegPID();
    sCurExtRID = aCursor->mFstUndoExtRID;

    IDE_ASSERT( sSegMgmtOp->mGetSegInfo(
                    NULL,
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    sSegPID,
                    NULL, /* aTableHeader */
                    &sSegInfo ) == IDE_SUCCESS );

    IDE_TEST( sSegMgmtOp->mGetExtInfo( NULL,
                                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       sCurExtRID,
                                       &sExtInfo )
              != IDE_SUCCESS );

    sCurPageID    = SD_MAKE_PID( aCursor->mFstUndoRecSID );
    sStartSlotSeq = SD_MAKE_SLOTNUM( aCursor->mFstUndoRecSID );

    while( sCurPageID != SD_NULL_PID )
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID( aCursor->mCursorProp.mStatistics,
                                              SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                              sCurPageID,
                                              (UChar**)&sPage,
                                              &sTrySuccess )
                  != IDE_SUCCESS );
        sFixPage = ID_TRUE;

        IDE_ASSERT(((sdpPhyPageHdr*)sPage)->mPageType == SDP_PAGE_UNDO );

        IDE_ASSERT( sCurPageID >= sExtInfo.mFstDataPID  );

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sPage);
        sSlotCount  = sdpSlotDirectory::getCount(sSlotDirPtr);

        for( i = sStartSlotSeq; i < sSlotCount; i++ )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                               i,
                                                               &sURHdr )
                      != IDE_SUCCESS );

            SDC_GET_UNDOREC_HDR_1B_FIELD( sURHdr,
                                          SDC_UNDOREC_HDR_FLAG,
                                          &sFlag );
            SDC_GET_UNDOREC_HDR_FIELD( sURHdr,
                                       SDC_UNDOREC_HDR_TABLEOID,
                                       &sTableOID );

            // Undo Record�� TableOID�� SM_NULL_OID �̸� �������
            // Type�̹Ƿ� skip �Ѵ�.
            if ( sTableOID != sTable->mSelfOID )
            {
                // �ٸ� Ŀ���� �ۼ��� Undo Record �̹Ƿ� Skip �Ѵ�
                continue;
            }

            if ( SDC_UNDOREC_FLAG_IS_UNDO_FOR_HEAD_ROWPIECE(sFlag)
                 != ID_TRUE )
            {
                continue;
            }

            if ( SDC_UNDOREC_FLAG_IS_UNDO_FOR_LOB_UPDATE(sFlag)
                 == ID_TRUE )
            {
                continue;
            }

            if ( SDC_UNDOREC_FLAG_IS_VALID(sFlag)
                 != ID_TRUE )
            {
                continue;
            }

            SDC_GET_UNDOREC_HDR_1B_FIELD( sURHdr,
                                          SDC_UNDOREC_HDR_TYPE,
                                          &sType );

            IDE_ERROR(
                (sType == SDC_UNDO_INSERT_ROW_PIECE)      ||
                (sType == SDC_UNDO_UPDATE_ROW_PIECE)      ||
                (sType == SDC_UNDO_OVERWRITE_ROW_PIECE)   ||
                (sType == SDC_UNDO_CHANGE_ROW_PIECE_LINK) ||
                (sType == SDC_UNDO_DELETE_ROW_PIECE)      ||
                (sType == SDC_UNDO_LOCK_ROW) );

            if( (sType == SDC_UNDO_INSERT_ROW_PIECE)      ||
                (sType == SDC_UNDO_CHANGE_ROW_PIECE_LINK) ||
                (sType == SDC_UNDO_DELETE_ROW_PIECE) )
            {
                continue;
            }

            if( sdcUndoRecord::isExplicitLockRec(sURHdr)
                == ID_TRUE )
            {
                continue;
            }

            IDE_ERROR( (sType == SDC_UNDO_UPDATE_ROW_PIECE)      ||
                       (sType == SDC_UNDO_OVERWRITE_ROW_PIECE)   ||
                       (sType == SDC_UNDO_LOCK_ROW) );

            sdcUndoRecord::parseRowGRID((UChar*)sURHdr, &sRowGRID);
            IDE_ERROR( SC_GRID_IS_NOT_NULL( sRowGRID ) );

            IDE_ERROR( smcTable::getTableSpaceID(sTable) 
                       == SC_MAKE_SPACE( sRowGRID ) );

            IDE_TEST( smiTableCursor::insertKeyIntoIndices( aCursor,
                                                            NULL, /* aRow */
                                                            sRowGRID,
                                                            NULL,
                                                            NULL )
                      != IDE_SUCCESS );

            IDE_TEST( iduCheckSessionEvent(
                         aCursor->mIterator->properties->mStatistics )
                     != IDE_SUCCESS );
        }

        sFixPage = ID_FALSE;
        IDE_TEST(sdbBufferMgr::unfixPage(
                          aCursor->mCursorProp.mStatistics,
                          sPage ) != IDE_SUCCESS);

        if( sCurPageID == SD_MAKE_PID( aCursor->mLstUndoRecSID ) )
        {
            break;
        }

        IDE_TEST( sSegMgmtOp->mGetNxtAllocPage( NULL, /* idvSQL */
                                                SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                                &sSegInfo,
                                                NULL,
                                                &sCurExtRID,
                                                &sExtInfo,
                                                &sCurPageID )
                  != IDE_SUCCESS );
        sStartSlotSeq = 1;
    }

    IDE_EXCEPTION_CONT( SKIP_INSERT_KEY );

    IDV_SQL_OPTIME_END( aCursor->mCursorProp.mStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_INDEX_OPER );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sFixPage == ID_TRUE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::unfixPage(
                             aCursor->mCursorProp.mStatistics,
                             sPage ) == IDE_SUCCESS );
        IDE_POP();
    }

    IDV_SQL_OPTIME_END( aCursor->mCursorProp.mStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_INDEX_OPER );

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : MemInsert, Mem&Disk Update�� ���.
 **********************************************************************/
IDE_RC smiTableCursor::insertKeyIntoIndices( smiTableCursor * aCursor,
                                             SChar          * aRow,
                                             scGRID           aRowGRID,
                                             SChar          * aNullRow,
                                             SChar         ** aExistUniqueRow )
{
    idBool                sUniqueCheck;
    UInt                  i;
    UInt                  j;
    UInt                  sIndexCnt;
    smnIndexHeader      * sIndexCursor;
    ULong               * sCheckUniqueBytePtr;
    ULong                 sBitMask;
    ULong                 sKeyValueBuf[SD_PAGE_SIZE/ID_SIZEOF(ULong)];
    idBool                sIsRowDeleted;
    UChar               * sRow;
    idBool                sTrySuccess;
    sdSID                 sRowSID;
    idBool                sIsPageLatchReleased = ID_TRUE;
    idBool                sDiskIndex;
    idBool                sIsExistFreeKey = ID_FALSE;

    sIndexCnt = smcTable::getIndexCount(aCursor->mTable);

    sCheckUniqueBytePtr = &aCursor->mCheckUniqueIdxBit;
    sBitMask = ((ULong)1 << 63);

    if( SMI_TABLE_TYPE_IS_DISK((smcTableHeader*)aCursor->mTable)
        == ID_TRUE )
    {
        sDiskIndex = ID_TRUE;
    }
    else
    {
        sDiskIndex = ID_FALSE;
    }
    
    for(i =0 ; i < sIndexCnt; i++ )
    {
        if (( aCursor->mModifyIdxBit & sBitMask ) != 0 )
        {
            sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex(aCursor->mTable, i);
            if ( smnManager::isIndexEnabled( sIndexCursor ) == ID_TRUE )
            {
                if ( ( (*sCheckUniqueBytePtr) & sBitMask ) != 0 )
                {
                    sUniqueCheck = ID_TRUE;
                }
                else
                {
                    sUniqueCheck = ID_FALSE;
                }

                if ( sDiskIndex == ID_TRUE )
                {
                    IDE_TEST_RAISE( smnManager::getIsConsistentOfIndexHeader(
                                        (void*)sIndexCursor ) == ID_FALSE, 
                                    err_inconsistent_index );

                    IDE_TEST( sdbBufferMgr::getPageByGRID(
                                      aCursor->mCursorProp.mStatistics,
                                      aRowGRID,
                                      SDB_S_LATCH,
                                      SDB_WAIT_NORMAL,
                                      NULL, /* aMtx */
                                      (UChar**)&sRow,
                                      &sTrySuccess )
                              != IDE_SUCCESS );
                    sIsPageLatchReleased = ID_FALSE;

                    sRowSID = SD_MAKE_SID_FROM_GRID( aRowGRID );

                    IDE_TEST( sIndexCursor->mModule->mMakeKeyFromRow(
                                          aCursor->mCursorProp.mStatistics,
                                          NULL, /* aMtx */
                                          NULL, /* aSP */
                                          aCursor->mTrans,
                                          aCursor->mTable,
                                          sIndexCursor,
                                          (UChar*)sRow,
                                          SDB_SINGLE_PAGE_READ,
                                          SC_MAKE_SPACE(aRowGRID),
                                          SMI_FETCH_VERSION_LAST,
                                          SD_NULL_SID, /* aTssSID */
                                          NULL,        /* aSCN */
                                          NULL,        /* aInfiniteSCN */
                                          (UChar*)sKeyValueBuf,
                                          &sIsRowDeleted,
                                          &sIsPageLatchReleased)
                              != IDE_SUCCESS );
                    IDE_ASSERT( sIsRowDeleted == ID_FALSE );

                    /* BUG-23319
                     * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
                    /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
                     * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
                     * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
                     * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
                     * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
                    if ( sIsPageLatchReleased == ID_FALSE )
                    {
                        sIsPageLatchReleased = ID_TRUE;
                        IDE_TEST( sdbBufferMgr::releasePage(
                                      aCursor->mCursorProp.mStatistics,
                                      (UChar*)sRow )
                                  != IDE_SUCCESS );
                    }

                    IDE_TEST( sIndexCursor->mInsert(
                                  aCursor->mCursorProp.mStatistics,
                                  aCursor->mTrans,
                                  aCursor->mTable,
                                  sIndexCursor,
                                  aCursor->mInfinite,
                                  (SChar*)sKeyValueBuf,
                                  NULL,
                                  sUniqueCheck,
                                  aCursor->mSCN,
                                  &sRowSID,
                                  aExistUniqueRow,
                                  aCursor->mOrgCursorProp.mLockWaitMicroSec,
                                  aCursor->mStatement->isForbiddenToRetry() )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( sIndexCursor->mInsert(
                                  aCursor->mCursorProp.mStatistics,
                                  aCursor->mTrans,
                                  aCursor->mTable,
                                  sIndexCursor,
                                  aCursor->mInfinite,
                                  aRow,
                                  aNullRow,
                                  sUniqueCheck,
                                  aCursor->mSCN,
                                  &sRowSID,
                                  aExistUniqueRow,
                                  aCursor->mOrgCursorProp.mLockWaitMicroSec,
                                  aCursor->mStatement->isForbiddenToRetry() )
                              != IDE_SUCCESS );
                }
            }
        }
        sBitMask = sBitMask >> 1;
    }//for i

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inconsistent_index );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_INDEX ));
    }
    IDE_EXCEPTION_END;

    if( sDiskIndex == ID_FALSE )
    {
        /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure
         * of index aging. 
         * MemIndex�� ���� ������ �����ϸ�, �ƿ� �ش� Row�� ���� ��� Index
         * ������ ������ */

        /* i���� Index ������ �õ�������, i ���������� Index�� ����
         * Loop�� ���� �� */
        sBitMask = ((ULong)1 << 63);
        for( j = 0 ; j < i ; j ++ )
        {
            /* BUG-47615 ������ Index key �� �����Ѵ� */
            if (( aCursor->mModifyIdxBit & sBitMask ) != 0 )
            {
                sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex(
                    aCursor->mTable, j );

                if( smnManager::isIndexEnabled( sIndexCursor ) == ID_TRUE )
                {
                    IDE_ASSERT( sIndexCursor->mModule->mFreeSlot( sIndexCursor,
                                                                  aRow,
                                                                  ID_FALSE,    /*aIgnoreNotFoundKey*/
                                                                  &sIsExistFreeKey )
                                == IDE_SUCCESS );

                    IDE_ASSERT( sIsExistFreeKey == ID_TRUE );
                }
                else
                {
                    /* nothing to do ... */
                }
            }
            sBitMask = sBitMask >> 1;
        }
    }

    IDE_PUSH();
    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aCursor->mCursorProp.mStatistics,
                                               (UChar*)sRow )
                    == IDE_SUCCESS );
    }

    if ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolation)
    {
        SMX_INC_SESSION_STATISTIC( aCursor->mTrans,
                                   IDV_STAT_INDEX_UNIQUE_VIOLATION_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader *)aCursor->mTable, SMC_INC_UNIQUE_VIOLATION_CNT );
    }
    IDE_POP();

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Disk Insert��
 *
 *   - [IN]
 *   - [OUT]
 *
 **********************************************************************/
IDE_RC smiTableCursor::insertKeyIntoIndices( smiTableCursor    *aCursor,
                                             scGRID             aRowGRID,
                                             const smiValue    *aValueList,
                                             idBool             aForce )
{
    idBool                sUniqueCheck;
    UInt                  i;
    UInt                  sIndexCnt;
    smnIndexHeader      * sIndexCursor;
    ULong               * sBytePtr;
    ULong                 sBitMask;
    ULong                 sKeyValueBuf[SD_PAGE_SIZE/ID_SIZEOF(ULong)];
    sdSID                 sRowSID;

    IDE_DASSERT( SMI_TABLE_TYPE_IS_DISK( (smcTableHeader*)aCursor->mTable ) == ID_TRUE );

    sIndexCnt = smcTable::getIndexCount(aCursor->mTable);

    sBytePtr = &aCursor->mModifyIdxBit;
    sBitMask = ((ULong)1 << 63);
    for(i =0 ; i < sIndexCnt; i++ )
    {
        sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex(aCursor->mTable, i);

        /* BUG-35254 - the local index is broken when executing insert or delete
         *             using composite cursor.
         * Insert�� ��� mModifyIdxBit�� Ȯ���� �ʿ���� ������ ��� �ε�����
         * �����ؾ� �Ѵ�.
         * ������ Update�� ��� Ȯ���ؾ� �ϱ� ������ Insert�� ��� aForce��
         * ID_TRUE�� �Ѿ���Եǰ�, �̶����� mModifyIdxBit�� Ȯ������ �ʰ�
         * �ε����� �����Ѵ�. */
        if( ((sIndexCursor->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE) &&
            ( (aForce == ID_TRUE) ||
              (((*sBytePtr) & sBitMask) != 0) ) )
        {
            if( ((sIndexCursor->mFlag & SMI_INDEX_UNIQUE_MASK) ==
                 SMI_INDEX_UNIQUE_ENABLE) ||
                ((sIndexCursor->mFlag & SMI_INDEX_LOCAL_UNIQUE_MASK) ==
                 SMI_INDEX_LOCAL_UNIQUE_ENABLE) )
            {
                sUniqueCheck = ID_TRUE;
            }
            else
            {
                sUniqueCheck = ID_FALSE;
            }

            IDE_TEST_RAISE( smnManager::getIsConsistentOfIndexHeader(
                                              (void*)sIndexCursor ) == ID_FALSE, 
                            err_inconsistent_index );

            IDE_TEST( sIndexCursor->mModule->mMakeKeyFromSmiValue(
                                              sIndexCursor,
                                              aValueList,
                                              (UChar*)sKeyValueBuf )
                      != IDE_SUCCESS );

            sRowSID = SD_MAKE_SID_FROM_GRID( aRowGRID );
                
            IDE_TEST( sIndexCursor->mInsert( aCursor->mCursorProp.mStatistics,
                                             aCursor->mTrans,
                                             aCursor->mTable,
                                             sIndexCursor,
                                             aCursor->mInfinite,
                                             (SChar*)sKeyValueBuf,
                                             NULL, // aNull
                                             sUniqueCheck,
                                             aCursor->mSCN,
                                             &sRowSID,
                                             NULL,  //aExistUniqueRow
                                             aCursor->mOrgCursorProp.mLockWaitMicroSec,
                                             aCursor->mStatement->isForbiddenToRetry() )
                            
                      != IDE_SUCCESS );
        } //if
        sBitMask = sBitMask >> 1;
    } //for i

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_inconsistent_index);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_INDEX ));
    }
    IDE_EXCEPTION_END;

    if ( ideGetErrorCode() == smERR_ABORT_smnUniqueViolation)
    {
        SMX_INC_SESSION_STATISTIC( aCursor->mTrans,
                                   IDV_STAT_INDEX_UNIQUE_VIOLATION_COUNT,
                                   1 /* Increase Value */ );

        smcTable::incStatAtABort( (smcTableHeader *)aCursor->mTable, SMC_INC_UNIQUE_VIOLATION_CNT );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::deleteKeys( smiTableCursor * aCursor,
                                   scGRID           aRowGRID,
                                   idBool           aForce )
{
    ULong                 sKeyValueBuf[SD_PAGE_SIZE/ID_SIZEOF(ULong)];
    UInt                  i;
    UInt                  sIndexCnt;
    smnIndexHeader      * sIndexCursor;
    ULong               * sBytePtr;
    ULong                 sBitMask;
    smiIterator         * sIterator;
    sdrMtxStartInfo       sStartInfo;
    sdSID                 sRowSID;
    UChar               * sRow;
    idBool                sTrySuccess;
    idBool                sIsDeleted;
    idBool                sIsPageLatchReleased = ID_TRUE;

    IDE_ERROR( aCursor != NULL );

    sIndexCnt = smcTable::getIndexCount(aCursor->mTable);

    IDE_TEST_CONT( sIndexCnt == 0, RETURN_SUCCESS );

    sIterator = (smiIterator*)aCursor->mIterator;
    sBytePtr  = &aCursor->mModifyIdxBit;
    sBitMask  = ((ULong)1 << 63);

    sStartInfo.mTrans   = aCursor->mTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( smxTrans::allocTXSegEntry( aCursor->mCursorProp.mStatistics,
                                         &sStartInfo )
              != IDE_SUCCESS );

    sRowSID = SD_MAKE_SID_FROM_GRID( aRowGRID );

    for(i = 0 ; i < sIndexCnt ; i++ )
    {
        sIndexCursor = (smnIndexHeader*)smcTable::getTableIndex( aCursor->mTable, i );

        /* BUG-35254 - the local index is broken when executing insert or delete
         *             using composite cursor.
         * Delete�� ��� mModifyIdxBit�� Ȯ���� �ʿ���� ������ ��� �ε�����
         * �����ؾ� �Ѵ�.
         * ������ Update�� ��� Ȯ���ؾ� �ϱ� ������ Delete�� ��� aForce��
         * ID_TRUE�� �Ѿ���Եǰ�, �̶����� mModifyIdxBit�� Ȯ������ �ʰ�
         * �ε����� �����Ѵ�. */
        if( ((sIndexCursor->mFlag & SMI_INDEX_USE_MASK) == SMI_INDEX_USE_ENABLE) &&
            ( (aForce == ID_TRUE) ||
              (((*sBytePtr) & sBitMask) != 0) ) )
        {
            IDE_TEST_RAISE( smnManager::getIsConsistentOfIndexHeader(
                    (void*)sIndexCursor ) == ID_FALSE, 
                ERR_INCONSISTENT_INDEX );
            /*
             * Data Page�� Shard Latch�� ȹ���� ���¿��� �ε����� �����ϸ�
             * Delayed CTS Stamping�� Latch Escalation���з� ���ؼ�
             * Delayed CTS Stamping�� �����Ѵ�. ���� �ε��� �������� ȹ����
             * Latch�� �����ؾ� �Ѵ�.
             */
            IDE_TEST( sdbBufferMgr::getPageByGRID( aCursor->mCursorProp.mStatistics,
                                                   aRowGRID,
                                                   SDB_S_LATCH,
                                                   SDB_WAIT_NORMAL,
                                                   NULL, /* aMtx */
                                                   (UChar**)&sRow,
                                                   &sTrySuccess )
                      != IDE_SUCCESS );
            sIsPageLatchReleased = ID_FALSE;

            /*
             * ������ ���ڵ�� ���� ������ Old Image�� �����ϱ� ���ؼ�
             * SMI_FETCH_VERSION_LASTPREV �� �����Ѵ�. 
             */
            IDE_TEST( sIndexCursor->mModule->mMakeKeyFromRow(
                          sIterator->properties->mStatistics,
                          NULL, /* aMtx */
                          NULL, /* aSP */
                          aCursor->mTrans,
                          aCursor->mTable,
                          sIndexCursor,
                          (UChar*)sRow,
                          SDB_SINGLE_PAGE_READ,
                          SC_MAKE_SPACE(sIterator->mRowGRID),
                          SMI_FETCH_VERSION_LASTPREV,
                          smxTrans::getTSSlotSID( aCursor->mTrans ),
                          &aCursor->mSCN,
                          &aCursor->mInfinite,
                          (UChar*)sKeyValueBuf,
                          &sIsDeleted,
                          &sIsPageLatchReleased )
                      != IDE_SUCCESS );

            IDE_DASSERT( sIsDeleted == ID_FALSE );

            /* BUG-23319
             * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
            /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
             * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
             * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
             * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
             * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
            if( sIsPageLatchReleased == ID_FALSE )
            {
                sIsPageLatchReleased = ID_TRUE;
                IDE_TEST( sdbBufferMgr::releasePage( aCursor->mCursorProp.mStatistics,
                                                     (UChar*)sRow )
                          != IDE_SUCCESS );
            }

            IDE_TEST( sIndexCursor->mModule->mDelete( aCursor->mCursorProp.mStatistics,
                                                      aCursor->mTrans,
                                                      sIndexCursor,
                                                      (SChar*)sKeyValueBuf,
                                                      sIterator,
                                                      sRowSID )
                      != IDE_SUCCESS );
        }

        sBitMask = sBitMask >> 1;
    }

    IDE_EXCEPTION_CONT( RETURN_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INCONSISTENT_INDEX);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_INDEX ));
    }
    IDE_EXCEPTION_END;

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( aCursor->mCursorProp.mStatistics,
                                               (UChar*)sRow )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/****************************************/
/* PROJ-1509 : Foreign Key Revisit      */
/****************************************/

IDE_RC smiTableCursor::beforeFirstModified( UInt aFlag )
{
    IDE_ERROR( this != NULL );

    IDE_TEST( mOps.beforeFirstModifiedFunc( this, aFlag )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTableCursor::readOldRow( const void ** aRow,
                                   scGRID      * aRowGRID )
{
    IDE_ERROR( this != NULL );

    IDE_TEST( mOps.readOldRowFunc( this,
                                   aRow,
                                   aRowGRID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTableCursor::readNewRow( const void ** aRow,
                                   scGRID      * aRowGRID )
{
    IDE_ERROR( this != NULL );

    IDE_TEST( mOps.readNewRowFunc( this,
                                   aRow,
                                   aRowGRID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::openMRVRDB( smiTableCursor *    aCursor,
                                   const void*         aTable,
                                   smSCN               aSCN,
                                   const smiRange *    aKeyRange,
                                   const smiRange *    aKeyFilter,
                                   const smiCallBack * aRowFilter,
                                   smlLockNode *       aCurLockNodePtr,
                                   smlLockSlot *       aCurLockSlotPtr)
{
    smSCN        sSCN;
    smSCN        sCreateSCN;
    smTID        sDummyTID;
    smxTrans   * sTrans = aCursor->mTrans;

    /* Cursor�� ���� ���� Statement�� ViewSCN�� Table�� CreateSCN�� �� �ؼ�
     * Table�� Modify �Ǿ����� ���θ� Ȯ���Ѵ�.*/
    if ( ( ((smcTableHeader*)(aCursor->mTable))->mFlag & SMI_TABLE_PRIVATE_VOLATILE_MASK )
         == SMI_TABLE_PRIVATE_VOLATILE_FALSE )
    {
        if ( sTrans->mIsGCTx == ID_TRUE )
        {
            SM_SET_SCN( &sCreateSCN, &(((smpSlotHeader *)aTable)->mCreateSCN) );

            if ( SM_SCN_IS_INFINITE( sCreateSCN ) &&
                 /* BUG-48244 : ��(TX)�� 2PC�� TX�� Pending �Ϸ�ñ��� ����� �ʿ����. */
                 ( SMP_GET_TID( sCreateSCN ) != smxTrans::getTransID( sTrans ) ) )
            {
                /* PROJ-2733 
                 * commit���� ���� ���̺� �����Ҽ��� �ִ�. */
                IDE_TEST( smxTrans::waitPendingTx( sTrans,
                                                   sCreateSCN,
                                                   aCursor->mSCN )
                          != IDE_SUCCESS );
            }
        }

        IDE_DASSERT( SM_SCN_IS_FREE_ROW( ((smpSlotHeader*)aTable)->mLimitSCN ) == ID_TRUE );

        /* BUG-48244 */
        SM_SET_SCN( &sCreateSCN, &(((smpSlotHeader *)aTable)->mCreateSCN) );
        SMX_GET_SCN_AND_TID( sCreateSCN, sSCN, sDummyTID );

        IDE_TEST_RAISE( ( SM_SCN_IS_EQ(&sSCN, &aSCN) != ID_TRUE
                          && SM_SCN_IS_NOT_INIT(aSCN) ) ||
                        ( SM_SCN_IS_GT(&sSCN, &aCursor->mSCN)  &&
                          SM_SCN_IS_NOT_INFINITE(sSCN) ),
                        ERR_MODIFIED );
    }
    else
    {
        /* SMI_TABLE_PRIVATE_VOLATILE_TRUE == Temporary Table */
        /* BUG-33982, BUG-47606 Temporary Table�� ��� ��� �� �� �־�� �մϴ�.
         * Temporary Table �� ��� Transaction ���� ������, ���������� ����, �����.
         * �ش� Transaction ���� ���߿� Temporary Table�� �����ؼ� ����ϰ� Commit �ÿ� ������,
         * �ش� Transaction�� ���� ���� �� Statement���� Temporary Table�� ���� �����ؾ� ��.
         * �׷��� Temporary Table�� ���Ͽ� CreateSCN�� Ȯ������ ���� */
    }

    //---------------------------------
    // PROJ-1509
    // Ʈ������� oid list �߿��� cursor open ��� ������ oid node ���� ����
    //---------------------------------

    aCursor->mFstOidNode =
        aCursor->mTrans->getCurNodeOfNVL();

    if( aCursor->mFstOidNode != NULL )
    {
        aCursor->mFstOidCount = ((smxOIDNode*)aCursor->mFstOidNode)->mOIDCnt;
    }

    if(aCursor->mIndex != NULL)
    {
        IDE_TEST_RAISE((((smnIndexHeader*)aCursor->mIndex)->mFlag &
                        SMI_INDEX_USE_MASK)
                       != SMI_INDEX_USE_ENABLE, err_disabled_index);
    }

    IDE_TEST( ((smnIndexModule*)aCursor->mIndexModule)->mInitIterator( aCursor->mIterator,
                                                                       aCursor->mTrans,
                                                                       (void*)aCursor->mTable,
                                                                       (void*)aCursor->mIndex,
                                                                       aCursor->mDumpObject,
                                                                       aKeyRange,
                                                                       aKeyFilter,
                                                                       aRowFilter,
                                                                       aCursor->mFlag,
                                                                       aCursor->mSCN,
                                                                       aCursor->mInfinite,
                                                                       aCursor->mUntouchable,
                                                                       &aCursor->mCursorProp,
                                                                       &aCursor->mSeekFunc,
                                                                       aCursor->mStatement )
              != IDE_SUCCESS );

    if( aCursor->mUntouchable == ID_FALSE )
    {
        // �ϴ� ��� �ε����� key insert�ϵ��� �ص�
        // inplace update�� ��쿡 �ٽ� ����.
        makeUpdatedIndexList( ((smcTableHeader*)aCursor->mTable),
                              aCursor->mColumns,
                              &aCursor->mModifyIdxBit,
                              &aCursor->mCheckUniqueIdxBit );
        
        /* �޸𸮴� ��� �ε����� Ű �����ϵ��� �ؾ� �Ѵ�.
         * ID_ULONG_MAX = 0xFFFFFFFFFFFFFFFF = set all bit 1 */
        aCursor->mModifyIdxBit = ID_ULONG_MAX;
    }
    aCursor->mUpdateInplaceEscalationPoint = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_disabled_index );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_DISABLED_INDEX) );
    }
    IDE_EXCEPTION( ERR_MODIFIED );
    {
        if( aCurLockSlotPtr != NULL )
        {
            (void)smlLockMgr::unlockTable( aCursor->mTrans->mSlotN,
                                           aCurLockNodePtr,
                                           aCurLockSlotPtr );
        }

        if( aCursor->mStatement->isForbiddenToRetry() )
        {
            SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[TABLE VALIDATION] "
                             "TableOID:%"ID_vULONG_FMT", "
                             "CursorSCN:%"ID_UINT64_FMT", "
                             "TableSCN:%"ID_UINT64_FMT", %"ID_UINT64_FMT,
                             ((smcTableHeader*)(aCursor->mTable))->mSelfOID,
                             SM_SCN_TO_LONG(aCursor->mSCN),
                             SM_SCN_TO_LONG(sSCN),
                             SM_SCN_TO_LONG(aSCN) );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode(smERR_REBUILD_smiTableModified) );
        }
  
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : MRDB, VRDB�� Cursor�� Close�Ѵ�. �̶� Close�� Cursor�� Change����(
 *               insert, delete, update)�� �����ߴٸ� Transaction�� OID List
 *               �� close�ÿ� index�� insert�ؾߵǴ� row�� �ִ��� ���� ������
 *               insert�Ѵ�. insert, update�� cursor close�ÿ� index�� row��
 *               insert�Ѵ�.
 *
 * aCursor - [IN] Close�� Cursor.
 **********************************************************************/
IDE_RC smiTableCursor::closeMRVRDB( smiTableCursor * aCursor )
{
    smxTrans*       sTrans;
    smcTableHeader* sTable;
    smcTableHeader* sTableOfCursor;
    smxOIDNode*     sNode;
    smxOIDInfo*     sNodeFence;
    smxOIDInfo*     sNodeCursor;
    SChar*          sNullPtr;
    smxOIDNode*     sOIDHeadNodePtr;
    UInt            sIndexCnt;
    UInt            sState       = 1;
    UInt            sFstOidIndex = 0;
    scGRID          sRowGRID;
    SChar*          sRowPtrOfCursor;

    sTrans        = aCursor->mTrans;

    /* update inplace �� escalation �� ��� idx bit ���� �޶��� ���� �ʱ�ȭ. 
     * escaltion ���� �κ��� ó���ϱ� ���ؼ�.
     * ID_ULONG_MAX = 0xFFFFFFFFFFFFFFFF = set all bit 1 */
    aCursor->mModifyIdxBit = ID_ULONG_MAX;

    /*
     * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
     * ������ �ʿ䰡 �����ϴ�.
     */
    if( (smiStatement::getSkipCursorClose( aCursor->mStatement ) == ID_FALSE) &&
        ( aCursor->mUntouchable == ID_FALSE) )
    {
        sOIDHeadNodePtr = &(sTrans->mOIDList->mOIDNodeListHead);

        if( aCursor->mFstOidNode == NULL )
        {
            /*
              Cursor�� Open�ɶ� Transaction OID List�� �ϳ��� Node�� ������.
              ������ ������ġ���� �����ؾ� �ȴ�.
            */
            aCursor->mFstOidNode  = sOIDHeadNodePtr->mNxtNode;
            aCursor->mFstOidCount = 0;
        }

        /*
          aCursor->mOidNode�� Cursor�� Open�ɶ� Transaction OID List�� ������
          Node�� ����Ű�� �ִ�.
        */
        sTable = (smcTableHeader*)aCursor->mTable;
        sIndexCnt =  smcTable::getIndexCount(sTable);

        /* BUG-31417
         * when insert after row trigger on memory table is called,
         * can not reference new inserted values of INSERT SELECT statement.
         *
         * normalInsertRow �Լ����� Insert���� Key�� Index�� �ֵ��� �����Ͽ�
         * Insert Cursor Close�ÿ��� Key�� Index�� ���� �ʴ´�.
         */
        if( (aCursor->mFstOidNode != sOIDHeadNodePtr) &&
            (sIndexCnt  != 0) &&
            (aCursor->mCursorType != SMI_INSERT_CURSOR) )
        {
            IDE_ERROR( smmManager::getOIDPtr( sTable->mSpaceID,
                                              sTable->mNullOID, 
                                              (void**)&sNullPtr)
                       == IDE_SUCCESS );
            sNode         = (smxOIDNode*)aCursor->mFstOidNode;
            sFstOidIndex  = aCursor->mFstOidCount;

            do
            {
                IDE_ASSERT(sNode->mOIDCnt >= sFstOidIndex);
                sNodeCursor = sNode->mArrOIDInfo + sFstOidIndex;
                sNodeFence  = sNode->mArrOIDInfo + sNode->mOIDCnt;

                for( ; sNodeCursor != sNodeFence; sNodeCursor++ )
                {
                    /*
                      ���� OID List�� Table OID�� Cursor�� open�� Table�� ����,
                      Index Insert�� �ؾ��Ѵٸ� Insert����
                    */
                    IDE_ASSERT( smcTable::getTableHeaderFromOID( sNodeCursor->mTableOID,
                                                                 (void**)&sTableOfCursor )
                                == IDE_SUCCESS );
                    if( ( sTableOfCursor == sTable ) &&
                        ( sNodeCursor->mFlag & SM_OID_ACT_CURSOR_INDEX ) )
                    {
                        SC_MAKE_NULL_GRID( sRowGRID );
                        IDE_ASSERT( smmManager::getOIDPtr( 
                                        sNodeCursor->mSpaceID,
                                        sNodeCursor->mTargetOID, 
                                        (void**)&sRowPtrOfCursor)
                                    == IDE_SUCCESS );
                        /* 
                            PROJ-2416
                            inplace update�� row���ʹ�  �ʼ� �ε����� ���� �ϵ��� �ؼ� ���� ����
                            ID_ULONG_MAX�� �ʱ�ȭ�Ǿ��ִ� mModifyidxBit �� ���� �����Ѵ�.
                        */
                        if ( aCursor->mUpdateInplaceEscalationPoint == sRowPtrOfCursor )
                        {
                            makeUpdatedIndexList( ((smcTableHeader*)aCursor->mTable),
                                                  aCursor->mColumns,
                                                  &aCursor->mModifyIdxBit,
                                                  &aCursor->mCheckUniqueIdxBit );
                        }
                        else
                        {
                            /* do nothing */
                        }

                        if ( aCursor->mModifyIdxBit != 0 )
                        {
                            IDE_TEST( smiTableCursor::insertKeyIntoIndices(
                                          aCursor,
                                          (SChar *)sRowPtrOfCursor,
                                          sRowGRID,
                                          sNullPtr,
                                          NULL )
                                      != IDE_SUCCESS );
                            sNodeCursor->mFlag |= SM_OID_ACT_AGING_INDEX;
                        }
                        else
                        {
                            /* do nothing */
                        }
                    }
                }

                sNode = sNode->mNxtNode;
                sFstOidIndex = 0;

                /*
                  �߰��� Session�� ���������� Check�ؼ� ����������
                  ���� Transaction�� Abort��Ų��.
                */
                IDE_TEST(iduCheckSessionEvent( aCursor->mCursorProp.mStatistics )
                         != IDE_SUCCESS);
            }
            while( sNode != sOIDHeadNodePtr );
        }

        /*---------------------------------
         * PROJ-1509
         * Cursor close ��� Ʈ������� ������ oid node ���� ����
         *---------------------------------*/
        aCursor->mLstOidNode = sOIDHeadNodePtr->mPrvNode;

        aCursor->mLstOidCount =
            ((smxOIDNode*)(aCursor->mLstOidNode))->mOIDCnt;
    }

    sState = 0;
    IDE_TEST( ((smnIndexModule*)aCursor->mIndexModule)->mDestIterator(
                  aCursor->mIterator )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        /* BUG-22442: smiTableCursor::close���� error�߻��� Iterator�� ����
         *            Destroy�� ���� �ʰ� �ֽ��ϴ�. */
        IDE_ASSERT( ((smnIndexModule*)aCursor->mIndexModule)->mDestIterator(
                        aCursor->mIterator )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * TASK-5030
 * Description : Full XLog�� ���� update �� column list�� value list
 *      �� �����Ѵ�.
 *
 * aCursor              - [IN] table cursor
 * aValueList           - [IN] original value list
 * aUpdateColumnList    - [IN] sorted column list of target table
 * aUpdateColumnCount   - [IN] column count of target table
 * aOldValueBuffer      - [IN] buffer for old value
 * aNewColumnList       - [OUT] new column list
 * aNewValueList        - [OUT] new value list
 **********************************************************************/
IDE_RC smiTableCursor::makeColumnNValueListMRDB(
                            smiTableCursor      * aCursor,
                            const smiValue      * aValueList,
                            smiUpdateColumnList * aUpdateColumnList,
                            UInt                  aUpdateColumnCount,
                            SChar               * aOldValueBuffer,
                            smiColumnList       * aNewColumnList,
                            smiValue            * aNewValueList)
{
    UInt                i                   = 0;
    UInt                j                   = 0;
    UInt                sNewColumnPosition  = 0;
    UInt                sOldColumnLength    = 0;
    UInt                sRemainedReadSize   = 0;
    UInt                sReadPosition       = 0;
    UInt                sReadSize           = 0;
    UInt                sOldBufferOffset    = 0;
    idBool              sExistNewValue  = ID_FALSE;
    const smiColumn   * sColumn;
    void              * sOldColumnValue;
    SChar             * sOldFixRowPtr   = aCursor->mIterator->curRecPtr;

    /* new value�� �ִ°�� -> new value�� �״�� ���
     * new value���� Lob type�� �ƴѰ�� -> old value�� �о�� ���
     * new value���� Lob type�� ��� -> ���� */
    for( i = 0, j = 0, sNewColumnPosition = 0 ;
         i < smcTable::getColumnCount(aCursor->mTable) ;
         i++ )
    {
        sColumn = smcTable::getColumn(aCursor->mTable, i);

        /* compare old column with new column */
        if( j < aUpdateColumnCount )
        {
            if ( sColumn->id == (aUpdateColumnList + j)->column->id )
            {
                sExistNewValue = ID_TRUE;
            }
            else
            {
                sExistNewValue = ID_FALSE;
            }
        }
        else
        {
            sExistNewValue = ID_FALSE;
        }

        /* set column list and value list */
        if ( sExistNewValue == ID_TRUE )
        {
            /* new value�� ������ */
            (aNewColumnList + sNewColumnPosition)->column =
                                (aUpdateColumnList + j)->column;
            (aNewColumnList + sNewColumnPosition)->next   =
                                (aNewColumnList + (sNewColumnPosition + 1));

            (aNewValueList + sNewColumnPosition)->length  =
                (aValueList + ((aUpdateColumnList + j)->position) )->length;
            (aNewValueList + sNewColumnPosition)->value   =
                (aValueList + ((aUpdateColumnList + j)->position) )->value;

            j++;
            sNewColumnPosition++;
        }
        else
        {
            /* new value�� ������ */
            if( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
                != SMI_COLUMN_COMPRESSION_TRUE )
            {
                sOldColumnLength = smcRecord::getColumnLen( sColumn, sOldFixRowPtr );

                /* smiTableBakcup::appendRow() �����ؼ� �ۼ��Ͽ��� */
                switch( sColumn->flag & SMI_COLUMN_TYPE_MASK )
                {
                    case SMI_COLUMN_TYPE_LOB:
                        /* Lob type�� ���� */
                        continue;

                    case SMI_COLUMN_TYPE_VARIABLE:
                    case SMI_COLUMN_TYPE_VARIABLE_LARGE:

                        sRemainedReadSize = sOldColumnLength;
                        sReadPosition     = 0;

                        while( sRemainedReadSize > 0 )
                        {
                            if( sRemainedReadSize < SMP_VC_PIECE_MAX_SIZE )
                            {
                                sReadSize = sRemainedReadSize;
                            }
                            else
                            {
                                sReadSize = SMP_VC_PIECE_MAX_SIZE;
                            }

                            sOldColumnValue  = smcRecord::getVarRow( sOldFixRowPtr,
                                                                     sColumn,
                                                                     sReadPosition,  
                                                                     &sReadSize, 
                                                                     (SChar*)(aOldValueBuffer + sOldBufferOffset),
                                                                     ID_FALSE );

                            IDE_ERROR( sOldColumnValue != NULL );

                            /* value ���������� ���� */
                            if( sReadPosition == 0)
                            {
                                (aNewValueList + sNewColumnPosition)->value = sOldColumnValue;
                            }
                            else
                            {
                                /* do nothging */
                            }

                            sRemainedReadSize   -= sReadSize;
                            sReadPosition       += sReadSize;
                            sOldBufferOffset    += sReadSize;
                        }

                        break;

                    case SMI_COLUMN_TYPE_FIXED:
                        sOldColumnValue  = sOldFixRowPtr + sColumn->offset;

                        (aNewValueList + sNewColumnPosition)->value   = sOldColumnValue;

                        break;

                    default:
                        /* Column�� Variable �̰ų� Fixed �̾�� �Ѵ�. */
                        IDE_ERROR_MSG( 0,
                                       "sColumn->id    :%"ID_UINT32_FMT"\n"
                                       "sColumn->flag  :%"ID_UINT32_FMT"\n"
                                       "sColumn->offset:%"ID_UINT32_FMT"\n"
                                       "sColumn->vcInOu:%"ID_UINT32_FMT"\n"
                                       "sColumn->size  :%"ID_UINT32_FMT"\n"
                                       "sColType       :%"ID_UINT32_FMT"\n",
                                       sColumn->id,
                                       sColumn->flag,
                                       sColumn->offset,
                                       sColumn->vcInOutBaseSize,
                                       sColumn->size,
                                       sColumn->flag & SMI_COLUMN_TYPE_MASK );
                        break;
                }
            }
            else
            {
                /* BUG-39478 supplimental log���� update �� ��
                 * compressed column�� ���� ����� ���� �ʽ��ϴ�. */

                sOldColumnValue  = sOldFixRowPtr + sColumn->offset;

                (aNewValueList + sNewColumnPosition)->value = sOldColumnValue;
                sOldColumnLength  = ID_SIZEOF(smOID);
            }

            (aNewColumnList + sNewColumnPosition)->column = sColumn;
            (aNewColumnList + sNewColumnPosition)->next   = (aNewColumnList + (sNewColumnPosition + 1));

            (aNewValueList + sNewColumnPosition)->length  = sOldColumnLength;

            if( sOldColumnLength != 0 )
            {
                IDE_ASSERT( (aNewValueList + sNewColumnPosition)->value != NULL );
            }
            else
            {
                /* length�� 0�̸� value�� NULL�� ���� */
                (aNewValueList + sNewColumnPosition)->value   = NULL;
            }

            sNewColumnPosition++;
        }
    } /* end of for */

    (aNewColumnList + (sNewColumnPosition -1))->next = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * TASK-5030
 * Description : Full XLog�� ���� �ʿ��� �޸𸮸� �Ҵ�ް�, ���ο� 
 *      column list�� value list�� ���� �غ� �Ѵ�.
 *      before value �� LOB type�� �����Ѵ�.
 *
 * aCursor          - [IN] table cursor
 * aValueList       - [IN] original value list
 * aNewColumnList   - [OUT] new column list
 * aNewValueList    - [OUT] new value list
 * aIsReadBeforeImg - [OUT] before value�� �д���, �� �д���.
 *                          ID_TRUE�� ���, before�� �о���. 
 *                          �޸� ���� �ʿ�
 **********************************************************************/
IDE_RC smiTableCursor::makeFullUpdateMRDB( smiTableCursor  * aCursor,
                                           const smiValue  * aValueList,
                                           smiColumnList  ** aNewColumnList,
                                           smiValue       ** aNewValueList,
                                           SChar          ** aOldValueBuffer,
                                           idBool          * aIsReadBeforeImg )
{
    UInt                    sState              = 0;
    UInt                    sColumnCount        = 0;
    UInt                    sUpdateColumnCount  = 0;
    UInt                    i                   = 0;
    UInt                    j                   = 0;
    UInt                    sOldValueBufferSize = 0;
    UInt                    sOldColumnLength    = 0;
    idBool                  sExistNewValue      = ID_FALSE;
    UInt                    sOldValueCnt        = 0;
    smiColumnList         * sNewColumnList      = NULL;
    smiValue              * sNewValueList       = NULL;
    SChar                 * sOldValueBuffer     = NULL;
    const smiColumn       * sColumn             = NULL;
    const smiColumnList   * sCurrColumnList     = NULL;
    smiUpdateColumnList   * sUpdateColumnList   = NULL;
    SChar                 * sOldFixRowPtr       = aCursor->mIterator->curRecPtr;


    // ���̺��� �� �÷� ��
    sColumnCount = smcTable::getColumnCount( aCursor->mTable );

    // new value�� ��, �� update ������ ���� �� column�� �� ���
    sUpdateColumnCount  = 0;
    sCurrColumnList     = aCursor->mColumns;

    while( sCurrColumnList != NULL )
    {
        sUpdateColumnCount++;
        sCurrColumnList = sCurrColumnList->next;
    }

    IDE_ERROR( sUpdateColumnCount != 0 );

    IDU_FIT_POINT( "smiTableCursor::makeFullUpdateMRDB::calloc" );

    // new value column�� �����ϱ� ���� �޸� �Ҵ�
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                 sUpdateColumnCount,
                                 ID_SIZEOF(smiUpdateColumnList),
                                 (void**)&sUpdateColumnList )
              != IDE_SUCCESS );

    // ����
    sCurrColumnList = aCursor->mColumns;
    for ( i = 0 ; i < sUpdateColumnCount ; i++ )
    {
        (sUpdateColumnList + i)->column     = sCurrColumnList->column;
        (sUpdateColumnList + i)->position   = i;

        sCurrColumnList = sCurrColumnList->next;
    }

    idlOS::qsort( sUpdateColumnList,
                  sUpdateColumnCount,
                  ID_SIZEOF(smiUpdateColumnList),
                  gCompareSmiUpdateColumnListByColId );

    // before column�� ������ ������ ũ�⸦ ����. (lob�� ����)
    for( i = 0, j = 0 ; i < sColumnCount ; i++ )
    {
        sColumn = smcTable::getColumn(aCursor->mTable, i);

        if( j < sUpdateColumnCount )
        {
            /* after image���� �ش� �÷��� �ִ��� �˻� */
            if( sColumn->id == (sUpdateColumnList + j)->column->id )
            {
                j++;

                sExistNewValue = ID_TRUE;
            }
            else
            {
                sExistNewValue = ID_FALSE;
            }
        }
        else
        {
            sExistNewValue = ID_FALSE;
        }

        /* old value���� �޾ƿ;� �� ��� */
        if( sExistNewValue == ID_FALSE )
        {
            /* BUG-40650 suppliment log����
             * old value �� ��� ������ Ȯ���մϴ�. */
            sOldValueCnt++;

            /* BUG-39478 supplimental log���� update �� ��
             * compressed column�� ���� ����� ���� �ʽ��ϴ�.
             *
             * old value buffer�� variable column�� ���ؼ��� ���˴ϴ�. */
            if ( ( ( ( sColumn->flag & SMI_COLUMN_TYPE_MASK )
                     == SMI_COLUMN_TYPE_VARIABLE ) ||
                   ( ( sColumn->flag & SMI_COLUMN_TYPE_MASK )
                     == SMI_COLUMN_TYPE_VARIABLE_LARGE ) )
                &&
                ( ( sColumn->flag & SMI_COLUMN_COMPRESSION_MASK )
                  == SMI_COLUMN_COMPRESSION_FALSE ) )
            {
                sOldColumnLength     = smcRecord::getColumnLen( sColumn,
                                                                sOldFixRowPtr );
                sOldValueBufferSize += sOldColumnLength;
            }
        }
        else
        {
            /* do nothging */
        }

    } // end of 1st for loop

    if( sOldValueCnt != 0 )
    {
        // new column list alloc
        /* smiTableCursor_makeFullUpdateMRDB_calloc_NewColumnList.tc */
        IDU_FIT_POINT("smiTableCursor::makeFullUpdateMRDB::calloc::NewColumnList");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                     sColumnCount,
                                     ID_SIZEOF(smiColumnList),
                                     (void**) &sNewColumnList )
                  != IDE_SUCCESS);
        sState = 1;

        // new value list alloc
        /* smiTableCursor_makeFullUpdateMRDB_calloc_NewValueList.tc */
        IDU_FIT_POINT("smiTableCursor::makeFullUpdateMRDB::calloc::NewValueList");
        IDE_TEST( iduMemMgr::calloc ( IDU_MEM_SM_SMI,
                                      sColumnCount,
                                      ID_SIZEOF(smiValue),
                                      (void**) &sNewValueList )
                  != IDE_SUCCESS );
        sState = 2;

        /* BUG-40650 �ӽ� ���۸� ����ϴ� ��쿡�� �Ҵ��Ѵ�. */
        if( sOldValueBufferSize != 0 )
        {
            // before column value ����� ����
            /* smiTableCursor_makeFullUpdateMRDB_calloc_OldValueBuffer.tc */
            IDU_FIT_POINT("smiTableCursor::makeFullUpdateMRDB::calloc::OldValueBuffer");
            IDE_TEST( iduMemMgr::calloc ( IDU_MEM_SM_SMI,
                                          sOldValueBufferSize,
                                          ID_SIZEOF(SChar),
                                          (void**) &sOldValueBuffer )
                      != IDE_SUCCESS );
            sState = 3;
        }

        // set new columnlist and valuelist 
        IDE_TEST( makeColumnNValueListMRDB( aCursor,
                                            aValueList,
                                            sUpdateColumnList,
                                            sUpdateColumnCount,
                                            sOldValueBuffer,
                                            sNewColumnList,
                                            sNewValueList)
                  != IDE_SUCCESS );

        (*aNewColumnList)   = sNewColumnList;
        (*aNewValueList)    = sNewValueList;
        (*aOldValueBuffer)  = sOldValueBuffer;
        (*aIsReadBeforeImg) = ID_TRUE;
    }
    else
    {
        // before column value�� ������ �ʿ䰡 ���� ���
        (*aNewColumnList)   = NULL;
        (*aNewValueList)    = NULL;
        (*aOldValueBuffer)  = NULL;
        (*aIsReadBeforeImg) = ID_FALSE;
    }

    IDE_ERROR( sUpdateColumnList != NULL );

    IDE_TEST( iduMemMgr::free( sUpdateColumnList )
              != IDE_SUCCESS );
    sUpdateColumnList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            (void) iduMemMgr::free( sOldValueBuffer );

        case 2:
            (void) iduMemMgr::free( sNewValueList );

        case 1:
            (void) iduMemMgr::free( sNewColumnList );

        default:
            break;
    }

    if( sUpdateColumnList != NULL )
    {
        (void) iduMemMgr::free( sUpdateColumnList );
    }
    
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : aCursor�� ����Ű�� Row�� aValueList�� �����Ѵ�.
 *
 * aCursor     - [IN] Table Cursor
 * aValueList  - [IN] Update Value List
 * aRetryInfo  - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::updateRowMRVRDB( smiTableCursor       *  aCursor,
                                        const smiValue       *  aValueList,
                                        const smiDMLRetryInfo* aRetryInfo )
{
    UInt                    sLockEscMemSize;
    UInt                    sTableType;
    smTableCursorUpdateFunc sUpdateInpFunc;
    idvSQL                * sStatistics;
    ULong                   sUpdateMaxLogSize;
    idBool                  sIsUpdateInplace    = ID_FALSE;
    idBool                  sIsPrivateVol;

    smxTrans              * sTrans              = aCursor->mTrans;

    smiColumnList         * sNewColumnList      = NULL;
    smiValue              * sNewValueList       = NULL;
    SChar                 * sOldValueBuffer     = NULL;
    idBool                  sIsReadBeforeImg    = ID_FALSE;
    UInt                    sState              = 0;

    IDE_ERROR( aCursor->mIterator != NULL );
    IDE_TEST_RAISE( aCursor->mIterator->curRecPtr == NULL,
                    ERR_NO_SELECTED_ROW );

    /* TASK-5030 Ÿ DBMS�� ������ ���� ALA ��� �߰� */
    if ( smcTable::isSupplementalTable( (smcTableHeader*)(aCursor->mTable) ) == ID_TRUE )
    {
        IDE_TEST( makeFullUpdateMRDB( aCursor,
                                      aValueList,
                                      &sNewColumnList,
                                      &sNewValueList,
                                      &sOldValueBuffer,
                                      &sIsReadBeforeImg )
                  != IDE_SUCCESS );
        sState = 1;

        if( sIsReadBeforeImg == ID_FALSE )
        {
            /* before value�� �о�� �ʿ䰡 ���� ��� */
            sNewColumnList  = (smiColumnList *)aCursor->mColumns;
            sNewValueList   = (smiValue *)aValueList;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        sNewColumnList  = (smiColumnList *)aCursor->mColumns;
        sNewValueList   = (smiValue *)aValueList;
    }

    sLockEscMemSize = smuProperty::getLockEscMemSize();

    sTableType = SMI_GET_TABLE_TYPE( (smcTableHeader*)(aCursor->mTable) );

    /* PROJ-1407 Temporary Table
     * User temp table�� ��� inplace�� update�Ѵ�. */
    sIsPrivateVol = (( (((smcTableHeader*)(aCursor->mTable))->mFlag)
                       & SMI_TABLE_PRIVATE_VOLATILE_MASK )
                     == SMI_TABLE_PRIVATE_VOLATILE_TRUE ) ? ID_TRUE : ID_FALSE ;

    if(sTableType == SMI_TABLE_VOLATILE)
    {
        sUpdateInpFunc = (smTableCursorUpdateFunc)svcRecord::updateInplace;
    }
    else
    {
        sUpdateInpFunc = (smTableCursorUpdateFunc)smcRecord::updateInplace;
    }

    IDE_TEST_CONT( (sTrans->mFlag & SMI_TRANS_INPLACE_UPDATE_MASK)
                    == SMI_TRANS_INPLACE_UPDATE_DISABLE,
                    CONT_INPLACE_UPDATE_DISABLE );

    if(aCursor->mUpdate != sUpdateInpFunc)
    {
        if((aCursor->mIsSoloCursor == ID_TRUE) && (sTableType != SMI_TABLE_META))
        {
            // PRJ-1476.
            if(smuProperty::getTableLockEnable() == 1)
            {
                // BUG-11725
                if((sTrans->mUpdateSize > sLockEscMemSize) &&
                   (sLockEscMemSize > 0))
                {
                    sIsUpdateInplace = ID_TRUE;
                }

                if(aCursor->mLockMode == SML_XLOCK)
                {
                    sIsUpdateInplace = ID_TRUE;
                }
            }

            // PROJ-1407 Temporary Table
            if( sIsPrivateVol == ID_TRUE )
            {
                sIsUpdateInplace = ID_TRUE;
            }
        }

        if( sIsUpdateInplace == ID_TRUE )
        {
            /* inplace update ���� ������ ���� */
            aCursor->mUpdateInplaceEscalationPoint = aCursor->mIterator->curRecPtr;

            // To Fix BUG-14969
            if ( ( aCursor->mFlag & SMI_INPLACE_UPDATE_MASK )
                 == SMI_INPLACE_UPDATE_ENABLE )
            {
                if(aCursor->mLockMode != SML_XLOCK)
                {
                    if( smlLockMgr::lockTable(
                            sTrans->mSlotN,
                            (smlLockItem *)
                            SMC_TABLE_LOCK( (smcTableHeader *)aCursor->mTable ),
                            SML_XLOCK,
                            sTrans->getLockTimeoutByUSec(),
                            &aCursor->mLockMode ) != IDE_SUCCESS )
                    {
                        IDE_TEST( (sTrans->mFlag & SMI_TRANS_INPLACE_UPDATE_MASK)
                                    != SMI_TRANS_INPLACE_UPDATE_TRY )

                        sTrans->mFlag &= ~SMI_TRANS_INPLACE_UPDATE_MASK;
                        sTrans->mFlag |=  SMI_TRANS_INPLACE_UPDATE_DISABLE;
                        IDE_CONT( CONT_INPLACE_UPDATE_DISABLE );
                    }
                }

                makeUpdatedIndexList( ((smcTableHeader*)aCursor->mTable),
                                      aCursor->mColumns,
                                      &aCursor->mModifyIdxBit,
                                      &aCursor->mCheckUniqueIdxBit );

                aCursor->mUpdate = sUpdateInpFunc;
            }
            else
            {
                // mFlag�� SMI_INPLACE_DISABLE�� ���,
                // inplace update�� ����Ǹ� �ȵ�
                // < inplace update �ϸ� �ȵǴ� ���� >
                //   trigger�� foreign key�� �ִ� ���,
                //   ���� �� �� ���� �о�� �ϴµ� inplace update�ϸ�
                //   ���� �� �� ���� ���� �� ����.
                //   ���� QP���� trigger�� foreign key�� �ִ� ���,
                //   SMI_INPLACE_DISABLE flag�� �������ش�.
            }

        } // aCursor->mIsSolCursor

        sStatistics       = smxTrans::getStatistics( sTrans );
        sUpdateMaxLogSize = 
                gSmiGlobalCallBackList.getUpdateMaxLogSize( sStatistics );

        if( sUpdateMaxLogSize != 0 )
        {
            /* BUG-19080: 
             *     OLD Version�� ���� �����̻� ����� �ش� Transaction��
             *     Abort�ϴ� ����� �ʿ��մϴ�.
             *
             * TRX_UPDATE_MAX_LOGSIZE�� 0�̸� �� Property�� Disable�� ���̹Ƿ�
             * �����Ѵ�.
             *
             * */
            IDE_TEST_RAISE( sTrans->mUpdateSize > sUpdateMaxLogSize,
                            ERR_TOO_MANY_UPDATE );
        }
    }// aCursor->mUpdate != sUpdateInpFunc

    IDE_EXCEPTION_CONT( CONT_INPLACE_UPDATE_DISABLE );

    IDE_TEST( aCursor->mUpdate( aCursor->mIterator->properties->mStatistics,
                                aCursor->mTrans,
                                aCursor->mSCN,
                                aCursor->mTableInfo,
                                aCursor->mTable,
                                aCursor->mIterator->curRecPtr,
                                aCursor->mIterator->mRowGRID,
                                &(aCursor->mIterator->curRecPtr),
                                &(aCursor->mIterator->mRowGRID),
                                sNewColumnList,
                                sNewValueList,
                                aRetryInfo,
                                aCursor->mInfinite,
                                NULL, // aLobInfo4Update
                                aCursor->mModifyIdxBit,
                                aCursor->mStatement->isForbiddenToRetry() )
              != IDE_SUCCESS );

    if( sIsReadBeforeImg == ID_TRUE )
    {
        sState = 0;
        /* memory free */
        IDE_TEST( destFullUpdateMRDB( &sNewColumnList,
                                      &sNewValueList,
                                      &sOldValueBuffer)
                  != IDE_SUCCESS);
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SELECTED_ROW );
    IDE_SET( ideSetErrorCode( smERR_FATAL_smiNoSelectedRow ) );

    IDE_EXCEPTION( ERR_TOO_MANY_UPDATE );
    IDE_SET( ideSetErrorCode( smERR_ABORT_TOO_MANY_UPDATE_LOG,
                              sTrans->mUpdateSize,
                              sUpdateMaxLogSize ) );
    IDE_EXCEPTION_END;

    if( sIsReadBeforeImg == ID_TRUE )
    {
        if( sState == 1 )
        {
            /* memory free */
            (void)destFullUpdateMRDB( &sNewColumnList,
                                      &sNewValueList,
                                      &sOldValueBuffer );
        }
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCursor�� ����Ű�� Row�� �����Ѵ�.
 *
 * aCursor     - [IN] Table Cursor
 * aRetryInfo  - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::deleteRowMRVRDB( smiTableCursor        * aCursor,
                                        const smiDMLRetryInfo * aRetryInfo )
{
    /* =================================================================
     * If the delete operation is performed by a replicated transaction,
     * the delete row is made one per transaction not per cursor        *
     * =================================================================*/
    IDE_TEST_RAISE( aCursor->mIterator->curRecPtr == NULL,
                    ERR_NO_SELECTED_ROW );

    IDE_TEST( aCursor->mRemove( aCursor->mIterator->properties->mStatistics,
                                aCursor->mTrans,
                                aCursor->mSCN,
                                aCursor->mTableInfo,
                                aCursor->mTable,
                                aCursor->mIterator->curRecPtr,
                                aCursor->mIterator->mRowGRID,
                                aCursor->mInfinite,
                                aRetryInfo,
                                aCursor->mIsDequeue,
                                aCursor->mStatement->isForbiddenToRetry() )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SELECTED_ROW );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiNoSelectedRow ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC smiTableCursor::openDRDB( smiTableCursor *    aCursor,
                                 const void*         aTable,
                                 smSCN               aSCN,
                                 const smiRange *    aKeyRange,
                                 const smiRange *    aKeyFilter,
                                 const smiCallBack * aRowFilter,
                                 smlLockNode *       aCurLockNodePtr,
                                 smlLockSlot *       aCurLockSlotPtr)
{
    smSCN      sCreateSCN;
    smSCN      sSCN;
    smTID      sDummyTID;
    smxTrans * sTrans = aCursor->mTrans;

    if( aCursor->mCursorType == SMI_INSERT_CURSOR )
    {
        (void)gSmiGlobalCallBackList.checkNeedUndoRecord( aCursor->mStatement, 
                                                          (void*)aTable, 
                                                          &aCursor->mNeedUndoRec );
    }

    if ( sTrans->mIsGCTx == ID_TRUE )
    {
        SM_SET_SCN( &sCreateSCN, &(((smpSlotHeader *)aTable)->mCreateSCN) );

        if ( SM_SCN_IS_INFINITE( sCreateSCN ) &&
             /* BUG-48244 : ��(TX)�� 2PC�� TX�� Pending �Ϸ�ñ��� ����� �ʿ����. */
             ( SMP_GET_TID( sCreateSCN ) != smxTrans::getTransID( sTrans ) ) )
        {
            /* PROJ-2733
             * commit���� ���� ���̺� �����Ҽ��� �ִ�. */
            IDE_TEST( smxTrans::waitPendingTx( sTrans,
                                               sCreateSCN,
                                               aCursor->mSCN )
                      != IDE_SUCCESS );
        }
    }

    IDE_DASSERT( SM_SCN_IS_FREE_ROW( ((smpSlotHeader*)aTable)->mLimitSCN ) == ID_TRUE );

    /* Table Meta�� ����Ǿ����� üũ */
    SMX_GET_SCN_AND_TID( ((smpSlotHeader*)aTable)->mCreateSCN, sSCN, sDummyTID );

    IDE_TEST_RAISE( ( SM_SCN_IS_EQ(&sSCN, &aSCN) != ID_TRUE
                      && SM_SCN_IS_NOT_INIT(aSCN) ) ||
                    ( SM_SCN_IS_GT(&sSCN, &aCursor->mSCN)  &&
                      SM_SCN_IS_NOT_INFINITE(sSCN) ),
                    ERR_MODIFIED );

    /* Iterator �ʱ�ȭ */
    if(aCursor->mIndex != NULL)
    {
        IDE_TEST_RAISE( ( ((smnIndexHeader*)aCursor->mIndex)->mFlag &
                          SMI_INDEX_USE_MASK )
                        != SMI_INDEX_USE_ENABLE, err_disabled_index );

        IDE_TEST_RAISE( smnManager::getIsConsistentOfIndexHeader(
                                              (void*)aCursor->mIndex ) == ID_FALSE, 
                        err_inconsistent_index);
    }

    /* FOR A4 : Cursor Property ����, aIterator --> mIterator */
    IDE_TEST( ((smnIndexModule*)aCursor->mIndexModule)->mInitIterator( aCursor->mIterator,
                                                                       aCursor->mTrans,
                                                                       (void*)aCursor->mTable,
                                                                       (void*)aCursor->mIndex,
                                                                       aCursor->mDumpObject,
                                                                       aKeyRange,
                                                                       aKeyFilter,
                                                                       aRowFilter,
                                                                       aCursor->mFlag,
                                                                       aCursor->mSCN,
                                                                       aCursor->mInfinite,
                                                                       aCursor->mUntouchable,
                                                                       &aCursor->mCursorProp,
                                                                       &aCursor->mSeekFunc,
                                                                       aCursor->mStatement )
              != IDE_SUCCESS );

    if( aCursor->mUntouchable == ID_FALSE )
    {
        if( ( aCursor->mNeedUndoRec == ID_TRUE ) ||
            ( aCursor->mCursorType  != SMI_INSERT_CURSOR ) )
        {
            // Insert�� update undo segment���� next undo record�� �� ������
            // RID�� �̸� ���� ���´�. <- close�ÿ� ��⼭ ���� ó���ϸ� ��.
            aCursor->mTrans->getUndoCurPos( &aCursor->mFstUndoRecSID,
                                            &aCursor->mFstUndoExtRID );
        }

        // insert, Ȥ�� update�������� ������ �޴� Index ����Ʈ�� ���Ѵ�.
        makeUpdatedIndexList( ((smcTableHeader*)aCursor->mTable),
                              aCursor->mColumns,
                              &aCursor->mModifyIdxBit,
                              &aCursor->mCheckUniqueIdxBit );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_disabled_index );
    { 
        IDE_SET( ideSetErrorCode(smERR_ABORT_DISABLED_INDEX) );
    }
    IDE_EXCEPTION( ERR_MODIFIED );
    {
        if(aCurLockSlotPtr != NULL)
        {
            (void)smlLockMgr::unlockTable( aCursor->mTrans->mSlotN,
                                           aCurLockNodePtr,
                                           aCurLockSlotPtr );
        }

        if( aCursor->mStatement->isForbiddenToRetry() )
        {
            SChar sMsgBuf[SMI_MAX_ERR_MSG_LEN];
            idlOS::snprintf( sMsgBuf,
                             SMI_MAX_ERR_MSG_LEN,
                             "[TABLE VALIDATION] "
                             "TableOID:%"ID_vULONG_FMT", "
                             "CursorSCN:%"ID_UINT64_FMT", "
                             "TableSCN:%"ID_UINT64_FMT", %"ID_UINT64_FMT,
                             ((smcTableHeader*)(aCursor->mTable))->mSelfOID,
                             SM_SCN_TO_LONG(aCursor->mSCN),
                             SM_SCN_TO_LONG(sSCN),
                             SM_SCN_TO_LONG(aSCN) );

            IDE_SET( ideSetErrorCode(smERR_ABORT_StatementTooOld, sMsgBuf) );

            IDE_ERRLOG( IDE_SD_19 );
        }
        else
        {
            IDE_SET( ideSetErrorCode( smERR_REBUILD_smiTableModified ) );
        }
    }
    IDE_EXCEPTION( err_inconsistent_index);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_INDEX ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smiTableCursor::closeDRDB4InsertCursor( smiTableCursor * aCursor )
{
    scPageID        sUndoPageID;
    scSlotNum       sFstSlotNum;
    sdcTXSegEntry  *sEntry;

    sEntry      = aCursor->mTrans->getTXSegEntry();
    sUndoPageID = sEntry->mFstUndoPID;

    /***************************************************************************
     * BUG-30109 - Direct-Path INSERT�� ������ Cursor�� ���,
     * mDPathSegInfo ������ ���������� �Ҵ��� �������� ���Ͽ�
     * setDirty�� ������ �ش�.
     *
     *  mDPathSegInfo�� �Ҵ��Ͽ� �Ŵ޾� �ֱ� ���� ���ܰ� �߻��Ͽ� Transaction��
     * Abort�� ���, INSERT METHOD�� APPEND��� �ص� mDPathSegInfo�� NULL�� ��
     * �ִ�.
     **************************************************************************/
    if( (aCursor->mFlag & SMI_INSERT_METHOD_MASK) == SMI_INSERT_METHOD_APPEND &&
        (aCursor->mDPathSegInfo != NULL) )
    {
        sdcDPathInsertMgr::setDirtyLastAllocPage( aCursor->mDPathSegInfo );
    }

    IDE_TEST_CONT( aCursor->mNeedUndoRec == ID_FALSE,
                    SKIP_SET_UNDO_POSITION );

    if( sUndoPageID != SD_NULL_PID )
    {
        if ( aCursor->mFstUndoRecSID == SD_NULL_SID )
        {
            //----------------------------
            // open ���, transaction�� undo record�� �ϳ��� �������� �ʾ�
            // ù��° insert undo record sid�� �������� ���� ���,
            // �̸� ��������
            //----------------------------
            sFstSlotNum = sEntry->mFstUndoSlotNum;
            IDE_ASSERT( sFstSlotNum != SC_NULL_SLOTNUM );

            aCursor->mFstUndoRecSID = SD_MAKE_SID( sUndoPageID, sFstSlotNum );
            aCursor->mFstUndoExtRID = sEntry->mFstExtRID4UDS;
        }
        else
        {
            // nothing to do
        }

        //----------------------------
        // ���� cursor�� ������ ������ insert undo record rid ��ġ ���ϱ�
        //----------------------------
        aCursor->mTrans->getUndoCurPos( &aCursor->mLstUndoRecSID,
                                        &aCursor->mLstUndoExtRID );
    }

    IDE_EXCEPTION_CONT( SKIP_SET_UNDO_POSITION );

    return IDE_SUCCESS;
}

IDE_RC smiTableCursor::closeDRDB4UpdateCursor( smiTableCursor * aCursor )
{
    scPageID        sUndoPageID;
    scSlotNum       sFstSlotNum;
    sdcTXSegEntry  *sEntry;

    IDE_DASSERT( (aCursor->mCursorType == SMI_UPDATE_CURSOR) ||
                 (aCursor->mCursorType == SMI_COMPOSITE_CURSOR) );

    sEntry      = aCursor->mTrans->getTXSegEntry();
    sUndoPageID = sEntry->mFstUndoPID;

    if( sUndoPageID != SD_NULL_PID )
    {
        if ( aCursor->mFstUndoRecSID == SD_NULL_SID )
        {
            //----------------------------
            // open ���, transaction�� undo record�� �ϳ��� �������� �ʾ�
            // ù��° update undo record sid�� �������� ���� ���,
            // �̸� ��������
            //----------------------------
            sFstSlotNum = sEntry->mFstUndoSlotNum;
            IDE_ASSERT( sFstSlotNum != SC_NULL_SLOTNUM );

            aCursor->mFstUndoRecSID = SD_MAKE_SID( sUndoPageID, sFstSlotNum );
            aCursor->mFstUndoExtRID = sEntry->mFstExtRID4UDS;
        }
        else
        {
            // nothing to do
        }

        //----------------------------
        // ���� cursor�� ������ ������ update undo sid ��ġ ���ϱ�
        //----------------------------
        aCursor->mTrans->getUndoCurPos( &aCursor->mLstUndoRecSID,
                                        &aCursor->mLstUndoExtRID );

        IDE_TEST(smiTableCursor::insertKeysWithUndoSID( aCursor )
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smiTableCursor::closeDRDB4DeleteCursor( smiTableCursor * aCursor )
{
    scPageID        sUndoPageID;
    scSlotNum       sFstSlotNum;
    sdcTXSegEntry  *sEntry;

    IDE_DASSERT( aCursor->mCursorType == SMI_DELETE_CURSOR );

    sEntry      = aCursor->mTrans->getTXSegEntry();
    sUndoPageID = sEntry->mFstUndoPID;

    if( sUndoPageID != SD_NULL_PID )
    {
        if ( aCursor->mFstUndoRecSID == SD_NULL_SID )
        {
            //----------------------------
            // open ���, transaction�� undo record�� �ϳ��� �������� �ʾ�
            // ù��° insert undo record sid�� �������� ���� ���,
            // �̸� ��������
            //----------------------------
            sFstSlotNum = sEntry->mFstUndoSlotNum;
            IDE_ASSERT( sFstSlotNum != SC_NULL_SLOTNUM );

            aCursor->mFstUndoRecSID =
                SD_MAKE_SID( sUndoPageID, sFstSlotNum );
            aCursor->mFstUndoExtRID = sEntry->mFstExtRID4UDS;
        }
        else
        {
            // nothing to do
        }

        //----------------------------
        // ���� cursor�� ������ ������ undo record rid ��ġ ���ϱ�
        //----------------------------
        aCursor->mTrans->getUndoCurPos(
                        &aCursor->mLstUndoRecSID,
                        &aCursor->mLstUndoExtRID );
    }

    return IDE_SUCCESS;
}

IDE_RC smiTableCursor::closeDRDB( smiTableCursor * aCursor )
{
    UInt  sState     = 1;
    sdSID sTSSlotSID = smxTrans::getTSSlotSID( aCursor->mTrans );

    /*
     * [BUG-24187] Rollback�� statement�� Internal CloseCurosr��
     * ������ �ʿ䰡 �����ϴ�.
     */
    if( (smiStatement::getSkipCursorClose( aCursor->mStatement ) == ID_FALSE) &&
        (aCursor->mUntouchable == ID_FALSE) &&
        (sTSSlotSID != SD_NULL_SID) )
    {
        switch( aCursor->mCursorType )
        {
            case SMI_INSERT_CURSOR :
            {
                IDE_TEST( closeDRDB4InsertCursor( aCursor )
                          != IDE_SUCCESS );
                break;
            }
            case SMI_UPDATE_CURSOR :
            {
                IDE_TEST( closeDRDB4UpdateCursor( aCursor )
                          != IDE_SUCCESS );
                break;
            }
            case SMI_DELETE_CURSOR :
            {
                IDE_TEST( closeDRDB4DeleteCursor( aCursor )
                          != IDE_SUCCESS );
                break;
            }
            case SMI_COMPOSITE_CURSOR :
            {
                IDE_TEST( closeDRDB4UpdateCursor( aCursor )
                          != IDE_SUCCESS );
                IDE_TEST( closeDRDB4InsertCursor( aCursor )
                          != IDE_SUCCESS );
                break;
            }
            default :  // SMI_LOCKROW_CURSOR
            {
                break;
            }
        }
    }
    else
    {
        // nothing to do
    }

    sState = 0;
    IDE_TEST( ((smnIndexModule*)aCursor->mIndexModule)->mDestIterator(
                                                     aCursor->mIterator )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        /* BUG-22442: smiTableCursor::close���� error�߻��� Iterator�� ����
         *            Destroy�� ���� �ʰ� �ֽ��ϴ�. */
        IDE_ASSERT( ((smnIndexModule*)aCursor->mIndexModule)->mDestIterator(
                        aCursor->mIterator )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/******************************************************************
 * TASK-5030
 * Description :
 *  sdcRow�� �Ѱ��� ������ �������� smiValue���¸� �ٽ� �����Ѵ�.
 *  sdnbBTree::makeSmiValueListInFetch()�� �����Ͽ� �ۼ��Ͽ���
 *  
 *  aIndexColumn        - [IN]     �ε��� Į�� ����
 *  aCopyOffset         - [IN]     column�� ���� rowpiece�� ������ ����� �� �����Ƿ�,
 *                                 copy offset ������ ���ڷ� �ѱ��.
 *                                 aCopyOffset�� 0�̸� first colpiece�̰�
 *                                 aCopyOffset�� 0�� �ƴϸ� first colpiece�� �ƴϴ�.
 *  aColumnValue        - [IN]     ������ column value
 *  aKeyInfo            - [OUT]    �̾Ƴ� Row�� ���� ������ ������ ���̴�.
 *
 * ****************************************************************/
IDE_RC smiTableCursor::makeSmiValueListInFetch(
                       const smiColumn            * aIndexColumn,
                       UInt                         aCopyOffset,
                       const smiValue             * aColumnValue,
                       void                       * aIndexInfo )
{
    UShort               sColumnSeq;
    smiValue           * sValue;
    sdcIndexInfo4Fetch * sIndexInfo;

    IDE_DASSERT( aIndexColumn != NULL );
    IDE_DASSERT( aColumnValue != NULL );
    IDE_DASSERT( aIndexInfo   != NULL );

    sIndexInfo   = (sdcIndexInfo4Fetch *)aIndexInfo;
    sColumnSeq   = aIndexColumn->id & SMI_COLUMN_ID_MASK;
    sValue       = &sIndexInfo->mValueList[ sColumnSeq ];

    if( aCopyOffset == 0 ) //first col-piece
    {
        sValue->length = aColumnValue->length;
        sValue->value  = sIndexInfo->mBufferCursor;
    }
    else                   //first col-piece�� �ƴ� ���
    {
        sValue->length += aColumnValue->length;
    }

    if( 0 < aColumnValue->length ) //NULL�� ��� length�� 0
    {
        ID_WRITE_AND_MOVE_DEST( sIndexInfo->mBufferCursor,
                                aColumnValue->value,
                                aColumnValue->length );
    }

    return IDE_SUCCESS;
}


/***********************************************************************
 * TASK-5030
 * Description : Full XLog�� ���� new value list�� 
 *               new column list�� �����.
 *      sdnbBTree::makeKeyValueFromRow()�� �����ؼ� �ۼ�
 *
 * aCursor                  - [IN] table cursor
 * aValueList               - [IN] original value list (after value list)
 * aIndexInfo4Fetch         - [IN] for fetching before image
 * aFetchColumnList         - [IN] for fetching before image
 * aBeforeRowBufferSource   - [IN] buffer for before value
 * aNewColumnList           - [OUT] new column List
 **********************************************************************/
IDE_RC smiTableCursor::makeColumnNValueListDRDB(
                            smiTableCursor      * aCursor,
                            const smiValue      * aValueList,
                            sdcIndexInfo4Fetch  * aIndexInfo4Fetch,
                            smiFetchColumnList  * aFetchColumnList,
                            SChar               * aBeforeRowBufferSource,
                            smiColumnList       * aNewColumnList )
{
    UInt                    i               = 0;
    UInt                    j               = 0;
    idBool                  sIsRowDeleted;
    idBool                  sIsPageLatchReleased;
    idBool                  sTrySuccess;
    idBool                  sIsFindColumn   = ID_FALSE;
    smcTableHeader        * sTableHeader;
    smiColumn             * sTableColumn;
    UChar                 * sBeforeRowBuffer;
    smiFetchColumnList    * sCurrFetchColumnList;
    smiColumnList         * sCurrColumnList;
    smiColumnList         * sCurrUpdateColumnList;
    UChar                 * sRow;

    IDE_ASSERT( aCursor->mTable != NULL );

    sTableHeader     = (smcTableHeader *)(aCursor->mTable);

    sBeforeRowBuffer = (UChar*)idlOS::align8( (ULong)aBeforeRowBufferSource );

    aIndexInfo4Fetch->mTableHeader          = sTableHeader;
    aIndexInfo4Fetch->mCallbackFunc4Index   = smiTableCursor::makeSmiValueListInFetch; 
    aIndexInfo4Fetch->mBuffer               = (UChar*)sBeforeRowBuffer;
    aIndexInfo4Fetch->mBufferCursor         = (UChar*)sBeforeRowBuffer;
    aIndexInfo4Fetch->mFetchSize            = SDN_FETCH_SIZE_UNLIMITED;

    IDE_TEST( sdbBufferMgr::getPageByGRID( aCursor->mIterator->properties->mStatistics,
                                           aCursor->mIterator->mRowGRID,
                                           SDB_S_LATCH,
                                           SDB_WAIT_NORMAL,
                                           NULL, /* aMtx */
                                           (UChar**)&sRow,
                                           &sTrySuccess )
                != IDE_SUCCESS);

    IDE_TEST( sdcRow::fetch(
                        aCursor->mIterator->properties->mStatistics,
                        NULL, /* aMtx */
                        NULL, /* aSP */
                        aCursor->mTrans,
                        smcTable::getTableSpaceID(sTableHeader),
                        sRow,
                        ID_TRUE, /* aIsPersSlot */
                        SDB_SINGLE_PAGE_READ,
                        aFetchColumnList,
                        SMI_FETCH_VERSION_CONSISTENT,
                        smxTrans::getTSSlotSID( aCursor->mTrans ),
                        &aCursor->mSCN,
                        &aCursor->mInfinite,
                        aIndexInfo4Fetch,
                        NULL, /* aLobInfo4Fetch */
                        sTableHeader->mRowTemplate,
                        (UChar*)sBeforeRowBuffer,
                        &sIsRowDeleted,
                        &sIsPageLatchReleased
                        )
              != IDE_SUCCESS );

    if( sIsPageLatchReleased == ID_FALSE )
    {
        sIsPageLatchReleased = ID_TRUE;
        IDE_TEST( sdbBufferMgr::releasePage( aCursor->mIterator->properties->mStatistics,
                                             sRow )
                  != IDE_SUCCESS );
    }

    /* set new column list */
    sCurrUpdateColumnList = (smiColumnList *)aCursor->mColumns; /* column list for after image */
    sCurrFetchColumnList  = aFetchColumnList; /* column list for before image */
    for( i = 0, j = 0 ;
         i < sTableHeader->mColumnCount ;
         i++ )
    {
        sTableColumn  = (smiColumn *)smcTable::getColumn( sTableHeader, i );
        sIsFindColumn = ID_FALSE;

        /* There are no more columns */
        if( (sCurrUpdateColumnList == NULL ) && (sCurrFetchColumnList == NULL ))
        {
            break;
        }

        if( sCurrUpdateColumnList == NULL )
        {
            /* There are no more after image */
        }
        else
        {
            if( sTableColumn->id == sCurrUpdateColumnList->column->id )
            {
                /* in case of after image */
                (aNewColumnList + j)->column = sCurrUpdateColumnList->column;
                (aNewColumnList + j)->next   = aNewColumnList + (j + 1);

                sCurrUpdateColumnList = sCurrUpdateColumnList->next;
                sIsFindColumn         = ID_TRUE;
            }
            else
            {
                /* do nothing */
            }
        }

        if( sCurrFetchColumnList == NULL )
        {
            /* There are no more before image */
        }
        else
        {
            if( sTableColumn->id == sCurrFetchColumnList->column->id )
            {
                /* in case of before image */
                (aNewColumnList + j)->column = sCurrFetchColumnList->column;
                (aNewColumnList + j)->next   = aNewColumnList + (j + 1);

                sCurrFetchColumnList = sCurrFetchColumnList->next;
                sIsFindColumn        = ID_TRUE;
            }
            else
            {
                /* do nothing */
            }
        }

        if( sIsFindColumn == ID_FALSE )
        {
            /* in case of lob type column on a before image, it is ture */
            IDE_ASSERT( SMI_IS_LOB_COLUMN(sTableColumn->flag) == ID_TRUE );
        }
        else
        {
            j++;
        }
    }
    (aNewColumnList + (j -1))->next = NULL;

    /* merge after image with befre image */
    sCurrUpdateColumnList = (smiColumnList *)aCursor->mColumns;  /* column list for after image */
    for( i = 0, j = 0 ; i < sTableHeader->mColumnCount ; i++ )
    {
        sTableColumn = (smiColumn *)smcTable::getColumn( sTableHeader, i );

        if( sTableColumn->id == sCurrUpdateColumnList->column->id )
        {
            /* in case of after value */
            aIndexInfo4Fetch->mValueList[i] = aValueList[j];

            if( sCurrUpdateColumnList->next == NULL )
            {
                /* There are no more after values */
                break;
            }
            else
            {
                j++;
                sCurrUpdateColumnList = sCurrUpdateColumnList->next;
            }
        }
    }

    /* compaction a value list */
    for( i = 0, j = 0, sCurrColumnList = aNewColumnList;
         i < sTableHeader->mColumnCount ;
         i++ )
    {
        if( j != 0 )
        {
            IDE_ASSERT( i >= j );

            aIndexInfo4Fetch->mValueList[ i - j ] = aIndexInfo4Fetch->mValueList[ i ];
        }

        sTableColumn = (smiColumn *)smcTable::getColumn( sTableHeader, i );

        if( sTableColumn->id == sCurrColumnList->column->id )
        {
            if( sCurrColumnList->next != NULL )
            {
                sCurrColumnList = sCurrColumnList->next;
            }
        }
        else
        {
            /* do not match, need to increase a compaction count */
            j++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * TASK-5030
 * Description : before value�� ������ ���� ũ�⸦ ���ϰ�,
 *      Full XLog�� ���� Fetch Column List �� �����.
 *      ���⼭ ���� Fetch Column List�� makeColumnNValueList4DRDB()
 *      ���� ����Ѵ�.
 *      sdnManager::makeFetchColumnList() �� �����Ͽ� �ۼ�
 *      TASK-5030 �̿ܿ� ������� ����. 
 *      LOB Ÿ���� �䱸������ �ƴϹǷ� ����
 *
 * aCursor          - [IN] table cursor
 * aFetchColumnList - [OUT] new fetch column list
 * aMaxRowSize      - [OUT] before value�� ������ ������ ũ��
 **********************************************************************/
IDE_RC smiTableCursor::makeFetchColumnList( smiTableCursor      * aCursor,
                                            smiFetchColumnList  * aFetchColumnList,
                                            UInt                * aMaxRowSize )
{
    UInt                  i             = 0;
    UInt                  j             = 0;
    UInt                  sColumnCount  = 0;
    UInt                  sMaxRowSize   = 0;
    smiFetchColumnList  * sFetchColumnList;
    smiColumn           * sTableColumn;
    const smiColumnList * sUpdateColumnList;
    smcTableHeader      * sTableHeader;

    IDE_ERROR( aFetchColumnList != NULL );

    sFetchColumnList = aFetchColumnList;
    sTableHeader     = (smcTableHeader *)(aCursor->mTable);
    sColumnCount     = sTableHeader->mColumnCount;

    /* calculate the buffer size for storing before image */
    sUpdateColumnList = aCursor->mColumns;
    for( i = 0, j = 0 ; i < sColumnCount ; i++ )
    {
        sTableColumn = (smiColumn*)smcTable::getColumn( sTableHeader, i );

        if( sTableColumn->id == sUpdateColumnList->column->id )
        {
            /* this column exists in after image */
            if( sUpdateColumnList->next != NULL)
            {
                sUpdateColumnList = sUpdateColumnList->next;
            }
            else
            {
                /* do nothing */
            }

            /* ignore a column of after image */
            continue;
        }
        else
        {
            /* this column exists in before image */
            if( SMI_IS_LOB_COLUMN(sTableColumn->flag) == ID_TRUE )
            {
                /* ignore a column because it is lob type */
                continue;
            }
        }

        sFetchColumnList[ j ].column = sTableColumn;
        sFetchColumnList[ j ].columnSeq = SDC_GET_COLUMN_SEQ( sTableColumn );

        IDE_TEST( gSmiGlobalCallBackList.findCopyDiskColumnValue(
                sTableColumn,
                (smiCopyDiskColumnValueFunc*)
                &sFetchColumnList[ j ].copyDiskColumn )
            != IDE_SUCCESS );

        sFetchColumnList[ j ].next = &sFetchColumnList[ j + 1 ];

        if( sMaxRowSize <
            idlOS::align8( sTableColumn->offset + sTableColumn->size ) )
        {
            sMaxRowSize =
                idlOS::align8( sTableColumn->offset + sTableColumn->size );
        }

        j++;
    }

    sFetchColumnList[ j - 1 ].next = NULL;

    (*aMaxRowSize)      = sMaxRowSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smiTableCursor::makeFullUpdateDRDB( smiTableCursor    * aCursor,
                                           const smiValue    * aValueList,
                                           smiColumnList    ** aNewColumnList,
                                           smiValue         ** aNewValueList,
                                           SChar            ** aOldValueBuffer,
                                           idBool            * aIsReadBeforeImg )
{
    UInt                  sState          = 0;
    UInt                  sMaxRowSize     = 0;
    sdcIndexInfo4Fetch    sIndexInfo4Fetch;    
    smiColumnList       * sNewColumnList  = NULL;
    smiValue            * sNewValueList   = NULL;
    smcTableHeader      * sTableHeader    = NULL;
    SChar               * sOldValueBuffer = NULL;
    smiFetchColumnList  * sFetchColumnList= NULL;

    IDE_ERROR( aCursor->mTable != NULL );

    sTableHeader = (smcTableHeader *)(aCursor->mTable);

    /* smiTableCursor_makeFullUpdateDRDB_calloc_FetchColumnList.tc */
    IDU_FIT_POINT("smiTableCursor::makeFullUpdateDRDB::calloc::FetchColumnList");
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                 sTableHeader->mColumnCount,
                                 ID_SIZEOF(smiFetchColumnList),
                                 (void**) &sFetchColumnList )
              != IDE_SUCCESS );

    IDE_TEST( makeFetchColumnList( aCursor,
                                   sFetchColumnList,
                                   &sMaxRowSize )
              != IDE_SUCCESS );

    if ( sMaxRowSize == 0 )
    {
        /* do not need to read a before image */
        (*aNewColumnList)   = NULL;
        (*aNewValueList)    = NULL;
        (*aOldValueBuffer)  = NULL;
        (*aIsReadBeforeImg) = ID_FALSE;

    }
    else
    {
        /* need to read a before image and make new column and value list */
        /* smiTableCursor_makeFullUpdateDRDB_calloc_NewColumnList.tc */
        IDU_FIT_POINT("smiTableCursor::makeFullUpdateDRDB::calloc::NewColumnList");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                     sTableHeader->mColumnCount,
                                     ID_SIZEOF(smiColumnList),
                                     (void**) &sNewColumnList )
                  != IDE_SUCCESS);
        sState = 1;

        /* smiTableCursor_makeFullUpdateDRDB_calloc_NewValueList.tc */
        IDU_FIT_POINT("smiTableCursor::makeFullUpdateDRDB::calloc::NewValueList");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                     sTableHeader->mColumnCount,
                                     ID_SIZEOF(smiValue),
                                     (void**) &sNewValueList )
                  != IDE_SUCCESS );
        sState = 2;

        /* smiTableCursor_makeFullUpdateDRDB_calloc_OldValueBuffer.tc */
        IDU_FIT_POINT("smiTableCursor::makeFullUpdateDRDB::calloc::OldValueBuffer");
        IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SMI,
                                     1,
                                     sMaxRowSize + ID_SIZEOF(ULong), // for align
                                     (void**) &sOldValueBuffer )
                  != IDE_SUCCESS);
        sState = 3;

        IDE_TEST( makeColumnNValueListDRDB( aCursor,
                                            aValueList,
                                            &sIndexInfo4Fetch,
                                            sFetchColumnList,
                                            sOldValueBuffer,
                                            sNewColumnList )
                  != IDE_SUCCESS );

        idlOS::memcpy( sNewValueList,
                       sIndexInfo4Fetch.mValueList,
                       sTableHeader->mColumnCount * ID_SIZEOF(smiValue) );

        (*aNewColumnList)   = sNewColumnList;
        (*aNewValueList)    = sNewValueList;
        (*aOldValueBuffer)  = sOldValueBuffer;
        (*aIsReadBeforeImg) = ID_TRUE;
    }

    IDE_ERROR( sFetchColumnList != NULL );
    
    IDE_TEST( iduMemMgr::free( sFetchColumnList )
              != IDE_SUCCESS );
    sFetchColumnList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            (void) iduMemMgr::free( sOldValueBuffer );
        case 2:
            (void) iduMemMgr::free( sNewValueList );
        case 1:
            (void) iduMemMgr::free( sNewColumnList );

        default:
            break;
    }

    if( sFetchColumnList != NULL )
    {
        (void) iduMemMgr::free( sFetchColumnList );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ���� Cursor�� Iterator�� ����Ű�� Row�� update�Ѵ�.
 *
 * aCursor     - [IN] Table Cursor
 * aValueList  - [IN] Update Value List
 * aRetryInfo  - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::updateRowDRDB( smiTableCursor       * aCursor,
                                      const smiValue       * aValueList,
                                      const smiDMLRetryInfo* aRetryInfo )
{
    smiColumnList     * sNewColumnList = NULL;
    smiValue          * sNewValueList = NULL;
    SChar             * sOldValueBuffer = NULL;
    idBool              sIsReadBeforeImg = ID_FALSE;
    UInt                sState = 0;

    IDE_ERROR( aCursor->mTable != NULL );
    IDE_TEST_RAISE(
             SC_GRID_IS_NULL(aCursor->mIterator->mRowGRID),
             ERR_NO_SELECTED_ROW );

    /* TASK-5030 Ÿ DBMS�� ������ ���� ALA ��� �߰� */
    if ( smcTable::isSupplementalTable( (smcTableHeader*)(aCursor->mTable) ) == ID_TRUE )
    {
        IDE_TEST( makeFullUpdateDRDB( aCursor,
                                      aValueList,
                                      &sNewColumnList,
                                      &sNewValueList,
                                      &sOldValueBuffer,
                                      &sIsReadBeforeImg )
                  != IDE_SUCCESS );
        sState = 1;

        if( sIsReadBeforeImg == ID_FALSE )
        {
            /* before value�� �о�� �ʿ䰡 ���� ��� */
            sNewColumnList  = (smiColumnList *)aCursor->mColumns;
            sNewValueList   = (smiValue *)aValueList;
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        sNewColumnList  = (smiColumnList *)aCursor->mColumns;
        sNewValueList   = (smiValue *)aValueList;
    }

    IDE_TEST( aCursor->mUpdate( aCursor->mIterator->properties->mStatistics,
                                aCursor->mTrans,
                                aCursor->mSCN,
                                aCursor->mTableInfo,
                                aCursor->mTable,
                                aCursor->mIterator->curRecPtr,
                                aCursor->mIterator->mRowGRID,
                                &(aCursor->mIterator->curRecPtr),
                                NULL, /* aRetSlotRID */ 
                                sNewColumnList,
                                sNewValueList,
                                aRetryInfo,
                                aCursor->mInfinite,
                                NULL, /* aLobInfo4Update */
                                0,    /* aModifyIdxBit   */
                                aCursor->mStatement->isForbiddenToRetry() )
              != IDE_SUCCESS );
    /*
     * �ε��� Ű�� ���ڵ庸�� ���� �������� �ȵȴ�.
     * �ݵ�� lock validation�� ��ģ �Ŀ� �ε��� Ű�� �����ؾ� �Ѵ�.
     */
    if( SMI_TABLE_TYPE_IS_DISK( (smcTableHeader*)aCursor->mTable ) == ID_TRUE )
    {
        IDE_TEST( deleteKeysForUpdate( aCursor,
                                       aCursor->mIterator->mRowGRID )
                  != IDE_SUCCESS );
    }

    IDE_TEST(iduCheckSessionEvent(aCursor->mIterator->properties->mStatistics)
             != IDE_SUCCESS);

    if( sIsReadBeforeImg == ID_TRUE )
    {
        sState = 0;
        /* memory free */
        IDE_TEST( destFullUpdateDRDB( sNewColumnList,
                                      sNewValueList,
                                      sOldValueBuffer )
                  != IDE_SUCCESS);
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SELECTED_ROW );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiNoSelectedRow ) );
    }
    IDE_EXCEPTION_END;

    if( sIsReadBeforeImg == ID_TRUE )
    {
        if( sState == 1 )
        {
            (void)destFullUpdateDRDB( sNewColumnList,
                                      sNewValueList,
                                      sOldValueBuffer );
        }
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� Cursor�� Iterator�� ����Ű�� Row�� delete�Ѵ�.
 *
 * aCursor     - [IN] Table Cursor
 * aRetryInfo  - [IN] retry ����
 **********************************************************************/
IDE_RC smiTableCursor::deleteRowDRDB( smiTableCursor        * aCursor,
                                      const smiDMLRetryInfo * aRetryInfo )
{
    IDE_ERROR( aCursor->mTable != NULL );

    IDE_TEST_RAISE( SC_GRID_IS_NULL(aCursor->mIterator->mRowGRID),
                    ERR_NO_SELECTED_ROW );

    IDE_TEST( aCursor->mIsDequeue == ID_TRUE );

    IDE_TEST( aCursor->mRemove( aCursor->mIterator->properties->mStatistics,
                                aCursor->mTrans,
                                aCursor->mSCN,
                                aCursor->mTableInfo,
                                aCursor->mTable,
                                aCursor->mIterator->curRecPtr,
                                aCursor->mIterator->mRowGRID,
                                aCursor->mInfinite,
                                aRetryInfo,
                                aCursor->mIsDequeue,
                                aCursor->mStatement->isForbiddenToRetry() )
              != IDE_SUCCESS ); 

    /*
     * ������ Ű�� �ٽ� �����Ҽ� �ֱ� ������
     * �ε��� Ű�� ���ڵ庸�� ���� �������� �ȵȴ�.
     * �ݵ�� lock validation�� ��ģ �Ŀ� �ε��� Ű�� �����ؾ� �Ѵ�.
     */
    if( SMI_TABLE_TYPE_IS_DISK( (smcTableHeader*)aCursor->mTable ) == ID_TRUE )
    {
        IDE_TEST( deleteKeysForDelete( aCursor,
                                       aCursor->mIterator->mRowGRID )
                  != IDE_SUCCESS );
    }

    IDE_TEST(iduCheckSessionEvent(aCursor->mIterator->properties->mStatistics)
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_SELECTED_ROW );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_smiNoSelectedRow ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
const smiRange * smiTableCursor::getDefaultKeyRange( )
{
    return &smiTableCursorDefaultRange;
}

/***********************************************************************
 * Description :
 **********************************************************************/
const smiCallBack * smiTableCursor::getDefaultFilter( )
{
    return &smiTableCursorDefaultCallBack;
}


/*********************************************************
  PROJ-1509
  Function Description: beforeFirstModifiedMRVRDB
  Implementation :
       cursor�� ù��° ������ Oid ��ġ ���� ����
***********************************************************/
IDE_RC smiTableCursor::beforeFirstModifiedMRVRDB( smiTableCursor * aCursor,
                                                  UInt             aFlag )
{
    aCursor->mFlag &= ~SMI_FIND_MODIFIED_MASK;
    aCursor->mFlag |= aFlag;

    if ( ( aCursor->mFstOidNode == aCursor->mLstOidNode ) &&
         ( aCursor->mFstOidCount == aCursor->mLstOidCount ) )
    {
        // ���� cursor�� ���� ������ oid record�� ���� ���
        aCursor->mCurOidNode = NULL;
        aCursor->mCurOidIndex = 0;
    }
    else
    {
        if ( aCursor->mFstOidCount >= smuProperty::getOIDListCount() )
        {
            IDE_ERROR( aCursor->mFstOidNode != NULL );

            // mOidNode�� ������ record �� ���,
            // ���� mOidNode�� ù��° record�� �����ؾ� ��
            aCursor->mCurOidNode =
                ((smxOIDNode*)aCursor->mFstOidNode)->mNxtNode;
            aCursor->mCurOidIndex = 0;
        }
        else
        {
            aCursor->mCurOidNode = aCursor->mFstOidNode;
            aCursor->mCurOidIndex = aCursor->mFstOidCount;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: readOldRowMRVRDB
  Implementation :
      (1) beforeFirstModified() �Լ��� �̹� ����Ǿ����� �˻�
      (2) OldRow �� ã��
      (3) ��� ��ȯ
***********************************************************/
IDE_RC smiTableCursor::readOldRowMRVRDB( smiTableCursor * aCursor,
                                         const void    ** aRow,
                                         scGRID         * /* aGRID*/ )
{
    idBool       sFoundOldRow = ID_FALSE;
    smxOIDInfo * sCurOIDRec   = NULL;
    smOID        sCurRowOID   = SM_NULL_OID;
    scSpaceID    sSpaceID     = SC_NULL_SPACEID;
    UInt         sMaxOIDCnt   = smuProperty::getOIDListCount();

    //------------------------
    // (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
    //------------------------

    IDE_ERROR( ( aCursor->mFlag & SMI_FIND_MODIFIED_MASK )
               == SMI_FIND_MODIFIED_OLD );

    IDE_ERROR( aCursor->mCursorType != SMI_INSERT_CURSOR );

    //------------------------
    // (2) old row�� ã��
    //------------------------

    while(aCursor->mCurOidNode != NULL)
    {
        if ( ( aCursor->mCurOidNode == aCursor->mLstOidNode ) &&
             ( aCursor->mCurOidIndex == aCursor->mLstOidCount ) )  // BUG-23370
        {
            // cursor�� ������ undo record�� ��� �о ���̻�
            // �о�� �� undo record�� �������� ����
            aCursor->mCurOidNode = NULL;
            aCursor->mCurOidIndex = 0;
            break;
        }
        else
        {
            if( aCursor->mCurOidIndex < sMaxOIDCnt )
            {
                sCurOIDRec = &(((smxOIDNode*)aCursor->mCurOidNode)->
                               mArrOIDInfo[aCursor->mCurOidIndex]);

                //------------------------
                // ����  cursor�� ������ old record ���� �˻�
                //------------------------

                if ( sCurOIDRec->mTableOID == aCursor->mTableInfo->mTableOID )
                {
                    sCurRowOID = sCurOIDRec->mTargetOID;

                    // old record ���� �˻�
                    if (  ((sCurOIDRec->mFlag & SM_OID_OP_MASK) == SM_OID_OP_OLD_FIXED_SLOT)
                          /*delete row ������ ������ row�� readOldRow�� ���� �������� �Ѵ�.*/
                        ||((sCurOIDRec->mFlag & SM_OID_OP_MASK) == SM_OID_OP_DELETED_SLOT))
                    {
                        // old record�� ã��
                        sFoundOldRow = ID_TRUE;
                        sSpaceID = sCurOIDRec->mSpaceID;
                        IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                                           sCurRowOID, 
                                                           (void**)aRow )
                                    == IDE_SUCCESS );
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }
        }

        //------------------------
        // ���� oid record�� ����
        //------------------------

        aCursor->mCurOidIndex++;

        // To Fix BUG-14927
        if ( aCursor->mCurOidIndex > sMaxOIDCnt )
        {
            // ���� OID Node�� OID Record�� ��� �о�����
            // ���� OID Node�� ����
            aCursor->mCurOidNode =
                ((smxOIDNode*)(aCursor->mCurOidNode))->mNxtNode;
            aCursor->mCurOidIndex = 0;
        }
        else
        {
            // nothing to do
        }

        //------------------------
        // row�� ã�� ���, �ߴ�
        //------------------------

        if ( sFoundOldRow == ID_TRUE )
        {
            break;
        }
        else
        {
            // nothing to do
        }
    }

    //------------------------
    // (3) ��� ��ȯ
    //------------------------

    if ( sFoundOldRow == ID_TRUE )
    {
        IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                           sCurRowOID, 
                                           (void**)aRow )
                    == IDE_SUCCESS );
    }
    else
    {
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: readNewRowMRVRDB
  Implementation :
      (1) beforeFirstModified() �Լ��� �̹� ����Ǿ����� �˻�
      (2) NewRow �� ã��
      (3) ��� ��ȯ
***********************************************************/
IDE_RC smiTableCursor::readNewRowMRVRDB( smiTableCursor * aCursor,
                                         const void    ** aRow,
                                         scGRID         * /* aRowGRID*/ )
{
    idBool       sFoundNewRow = ID_FALSE;
    smxOIDInfo * sCurOIDRec   = NULL;
    smOID        sCurRowOID   = SM_NULL_OID;
    scSpaceID    sSpaceID     = SC_NULL_SPACEID;
    UInt         sMaxOIDCnt   = smuProperty::getOIDListCount();

    //------------------------
    // (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
    //------------------------

    IDE_ERROR( ( aCursor->mFlag & SMI_FIND_MODIFIED_MASK )
               == SMI_FIND_MODIFIED_NEW );

    IDE_ERROR( aCursor->mCursorType != SMI_DELETE_CURSOR );

    //------------------------
    // (2) new row�� ã��
    //------------------------

    while(aCursor->mCurOidNode != NULL)
    {
        if ( ( aCursor->mCurOidNode == aCursor->mLstOidNode ) &&
             ( aCursor->mCurOidIndex == aCursor->mLstOidCount ) )  // BUG-23370
        {
            // cursor�� ������ undo record�� ��� �о ���̻�
            // �о�� �� undo record�� �������� ����
            aCursor->mCurOidNode = NULL;
            aCursor->mCurOidIndex = 0;
            break;
        }
        else
        {
            if( aCursor->mCurOidIndex < sMaxOIDCnt )
            {
                sCurOIDRec = &(((smxOIDNode*)aCursor->mCurOidNode)->
                               mArrOIDInfo[aCursor->mCurOidIndex]);

                //------------------------
                // ����  cursor�� ������ new record ���� �˻�
                //------------------------

                if( sCurOIDRec->mTableOID == aCursor->mTableInfo->mTableOID )
                {

                    sCurRowOID = sCurOIDRec->mTargetOID;

                    // new record ���� �˻�
                    if ((sCurOIDRec->mFlag & SM_OID_OP_MASK)
                        == SM_OID_OP_NEW_FIXED_SLOT)
                    {
                        // new record�� ã��
                        sFoundNewRow = ID_TRUE;
                        sSpaceID = sCurOIDRec->mSpaceID;
                        IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                                           sCurRowOID, 
                                                           (void**)aRow )
                                    == IDE_SUCCESS );
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }
        }

        //------------------------
        // ���� oid record�� ����
        //------------------------

        aCursor->mCurOidIndex++;

        // To Fix BUG-14927
        if ( aCursor->mCurOidIndex > sMaxOIDCnt )
        {
            // ���� OID Node�� OID Record�� ��� �о�����
            // ���� OID Node�� ����
            aCursor->mCurOidNode =
                ((smxOIDNode*)(aCursor->mCurOidNode))->mNxtNode;
            aCursor->mCurOidIndex = 0;
        }
        else
        {
            // nothing to do
        }

        //------------------------
        // row�� ã�� ���, �ߴ�
        //------------------------

        if ( sFoundNewRow == ID_TRUE )
        {
            break;
        }
        else
        {
            // nothing to do
        }
    }

    //------------------------
    // (3) ��� ��ȯ
    //------------------------

    if ( sFoundNewRow == ID_TRUE )
    {
        IDE_ASSERT( smmManager::getOIDPtr( sSpaceID,
                                           sCurRowOID, 
                                           (void**)aRow )
                    == IDE_SUCCESS );
    }
    else
    {
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  Function Description: beforeFirstModifiedDRDB
  Implementation :
      cursor�� ù��° ������ undo record ��ġ ���� ����
***********************************************************/
IDE_RC smiTableCursor::beforeFirstModifiedDRDB( smiTableCursor * aCursor,
                                                UInt             aFlag )
{
    aCursor->mFlag &= ~SMI_FIND_MODIFIED_MASK;
    aCursor->mFlag |= aFlag;

    // ���� ���꿡 ���ؼ��� ����
    switch( aCursor->mCursorType )
    {
        case SMI_INSERT_CURSOR:
        {
            IDE_ERROR( (aFlag & SMI_FIND_MODIFIED_MASK)
                        == SMI_FIND_MODIFIED_NEW );

            aCursor->mCurUndoRecSID = aCursor->mFstUndoRecSID;
            aCursor->mCurUndoExtRID = aCursor->mFstUndoExtRID;

            // To Fix BUG-14990
            aCursor->mFlag &= ~SMI_READ_UNDO_PAGE_LIST_MASK;
            aCursor->mFlag |= SMI_READ_UNDO_PAGE_LIST_INSERT;
            break;
        }
        case SMI_DELETE_CURSOR:
        {
            IDE_ERROR( (aFlag & SMI_FIND_MODIFIED_MASK )
                        == SMI_FIND_MODIFIED_OLD );

            aCursor->mCurUndoRecSID = aCursor->mFstUndoRecSID;
            aCursor->mCurUndoExtRID = aCursor->mFstUndoExtRID;

            // To Fix BUG-14990
            aCursor->mFlag &= ~SMI_READ_UNDO_PAGE_LIST_MASK;
            aCursor->mFlag |= SMI_READ_UNDO_PAGE_LIST_UPDATE;
            break;
        }
        case SMI_UPDATE_CURSOR:
        case SMI_COMPOSITE_CURSOR:
        {
            aCursor->mCurUndoRecSID = aCursor->mFstUndoRecSID;
            aCursor->mCurUndoExtRID = aCursor->mFstUndoExtRID;

            // To Fix BUG-14990
            aCursor->mFlag &= ~SMI_READ_UNDO_PAGE_LIST_MASK;
            aCursor->mFlag |= SMI_READ_UNDO_PAGE_LIST_UPDATE;
            break;
        }
        case SMI_SELECT_CURSOR:
        {
            // To Fix BUG-14955
            // mCursorType ������ ���� cursor open �ÿ� QP�� ��� ������
            // ���� �����ؼ� SM���� �Ѱ��ִ� ���� ���� �ٶ����ϴ�.
            // �׷��� ���� mCursorType ������ updateRow(), insertRow(),
            // deleteRow() ȣ�� �ÿ� �� �Լ��� �ȿ��� �����ǵ��� �����Ǿ�
            // �ִ�.
            // ���� QP���� update�� ���Ͽ� cursor�� open�Ͽ���
            // ���ǿ� �´� row�� ���� ���� updateRow()�� ȣ����� �ʾ���
            // ���, cursor type�� SMI_SELECT_CURSOR�� �� �ִ�.
            // ( SMI_SELECT_CURSOR�� default type�̴�. )
            // To Fix BUG-14990
            aCursor->mFlag &= ~SMI_READ_UNDO_PAGE_LIST_MASK;
            aCursor->mFlag |= SMI_READ_UNDO_PAGE_LIST_NONE;
            break;
        }
        case SMI_LOCKROW_CURSOR:
        {
            // Referential constraint check�� ���� lock row Ŀ��������
            // �� �Լ��� �ҷ����� �ȵȴ�.
            IDE_ERROR(0);
            break;
        }
        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************
  function description: readNewRowDRDB
  implementation :
      (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
      (2) new row ��ȯ

***********************************************************/
IDE_RC smiTableCursor::readNewRowDRDB( smiTableCursor * aCursor,
                                       const void    ** aRow,
                                       scGRID*          aRowGRID )
{
    UChar         * sCurUndoPage;
    UChar         * sSlotDirPtr;
    UChar         * sCurUndoRecHdr;
    idBool          sTrySuccess;
    idBool          sExistNewRow    = ID_FALSE;
    idBool          sIsFixUndoPage  = ID_FALSE;
    scGRID          sUpdatedRowGRID;
    UChar         * sUpdatedRow;
    sdcUndoRecType  sType;
    sdcUndoRecFlag  sFlag;
    smOID           sTableOID;
    idBool          sIsRowDeleted;
    scSpaceID       sTableSpaceID = smcTable::getTableSpaceID(aCursor->mTable);
    sdpSegMgmtOp  * sSegMgmtOp;
    scPageID        sSegPID;
    sdpSegInfo      sSegInfo;
    sdpExtInfo      sExtInfo;
    scPageID        sPrvUndoPID = SD_NULL_PID;
    idBool          sIsPageLatchReleased = ID_TRUE;
    smxTrans      * sTrans;

    //------------------------
    // (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
    //------------------------

    IDE_ERROR( ( aCursor->mFlag & SMI_FIND_MODIFIED_MASK )
               == SMI_FIND_MODIFIED_NEW );

    IDE_ERROR( aCursor->mCursorType != SMI_DELETE_CURSOR );

    //------------------------
    // (2) new row ��ȯ
    //------------------------

    /* BUG-25176 readNewRow�� readOldRow���� Data�� ������� ���� ��츦 ������� ����
     *
     * �������, Foriegn Key ���Ե� ��ũ ���̺� INSERT INTO SELECT ..�� �����ؼ�
     * SELECT �Ǽ��� 0���� ��� Transaction Segment Entry�� NULL�̹Ƿ� ������
     * skip �Ѵ�. */

    sTrans = aCursor->mTrans;

    if ( sTrans->getTXSegEntry() == NULL )
    {
        *aRow = NULL;
        SC_MAKE_NULL_GRID( *aRowGRID ); 
        IDE_CONT( cont_skip_no_data );
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    sSegPID    = smxTrans::getUDSegPtr(sTrans)->getSegPID();
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_ASSERT( sSegMgmtOp->mGetSegInfo(
                    NULL,
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    sSegPID,
                    NULL, /* aTableHeader */
                    &sSegInfo ) == IDE_SUCCESS );

    IDE_TEST( sSegMgmtOp->mGetExtInfo( NULL,
                                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       aCursor->mCurUndoExtRID,
                                       &sExtInfo )
              != IDE_SUCCESS );

    while ( aCursor->mCurUndoRecSID != SD_NULL_SID )
    {
        if( aCursor->mCurUndoRecSID == aCursor->mLstUndoRecSID )
        {
            // cursor�� ������ undo record�� ��� �о ���̻�
            // �о�� �� undo record�� �������� ����
            aCursor->mCurUndoRecSID = SD_NULL_SID;
            aCursor->mCurUndoExtRID = SD_NULL_RID;
        }
        else
        {
            //------------------------
            //���� undo page list�� �о�� �� undo record�� �����ϴ� ���
            //------------------------

            if ( sPrvUndoPID != SD_MAKE_PID( aCursor->mCurUndoRecSID ) )
            {
                // ���� undo record�� ����� page fix
                IDE_TEST( sdbBufferMgr::fixPageByPID( aCursor->mCursorProp.mStatistics,
                                                      SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                                      SD_MAKE_PID( aCursor->mCurUndoRecSID ),
                                                      &sCurUndoPage,
                                                      &sTrySuccess )
                          != IDE_SUCCESS );

                sIsFixUndoPage = ID_TRUE;
                sPrvUndoPID    = SD_MAKE_PID( aCursor->mCurUndoRecSID );
            }
            else
            {
                IDE_ASSERT( sIsFixUndoPage == ID_TRUE );
            }

            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( sCurUndoPage );

            if( SD_MAKE_SLOTNUM(aCursor->mCurUndoRecSID) ==
                sdpSlotDirectory::getCount(sSlotDirPtr) )
            {
                IDE_ASSERT( aCursor->mCurUndoRecSID ==
                            aCursor->mFstUndoRecSID );
                /* mFstUndoRecSID�� ���� cursor��
                 * ������ undo record ���� ��ġ�� ����Ų��.
                 * ù��° undo record�� �����Ϸ��� �Ҷ�,
                 * mFstUndoRecSID�� ����Ű�� ���������� ������ �����Ͽ�
                 * ������ �� ������, ���� �������� ù��° undo record��
                 * �����ϰ� �ȴ�.
                 * �׷��� mFstUndoRecSID�� ������ �������� �ʴ� ��ġ�� ����Ű�� �ȴ�.
                 * �̷� ��쿡�� setNextUndoRecSID4NewRow() �Լ��� ȣ���ϰ�
                 * continue�� �Ѵ�. */
                IDE_TEST( setNextUndoRecSID4NewRow( &sSegInfo,
                                                    &sExtInfo,
                                                    aCursor,
                                                    sCurUndoPage,
                                                    &sIsFixUndoPage )
                          != IDE_SUCCESS );
                continue;
            }

            //-------------------------------------------------------
            // undo record�� �����ϴ� ���,
            // ���� cursor�� ���� ������ new row�� �����ϴ��� �˻�
            //-------------------------------------------------------
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                        sSlotDirPtr,
                                        SD_MAKE_SLOTNUM(aCursor->mCurUndoRecSID),
                                        &sCurUndoRecHdr)
                      != IDE_SUCCESS );

            SDC_GET_UNDOREC_HDR_FIELD( sCurUndoRecHdr,
                                       SDC_UNDOREC_HDR_TABLEOID,
                                       &sTableOID );

            if ( aCursor->mTableInfo->mTableOID == sTableOID )
            {
                SDC_GET_UNDOREC_HDR_1B_FIELD( sCurUndoRecHdr,
                                              SDC_UNDOREC_HDR_FLAG,
                                              &sFlag );

                if( ( SDC_UNDOREC_FLAG_IS_UNDO_FOR_HEAD_ROWPIECE(sFlag) == ID_TRUE ) &&
                    ( SDC_UNDOREC_FLAG_IS_VALID(sFlag) == ID_TRUE ) )
                {
                    SDC_GET_UNDOREC_HDR_1B_FIELD( sCurUndoRecHdr,
                                                  SDC_UNDOREC_HDR_TYPE,
                                                  &sType );

                    /* xxxx �Լ� ó�� */
                    switch( sType )
                    {
                        case SDC_UNDO_INSERT_ROW_PIECE :
                            sExistNewRow = ID_TRUE;
                            break;

                        case SDC_UNDO_UPDATE_ROW_PIECE    :
                        case SDC_UNDO_OVERWRITE_ROW_PIECE :
                            sExistNewRow = ID_TRUE;
                            break;

                        case SDC_UNDO_LOCK_ROW :
                            if( sdcUndoRecord::isExplicitLockRec(
                                   sCurUndoRecHdr) == ID_TRUE )
                            {
                                sExistNewRow = ID_FALSE;
                            }
                            else
                            {
                                sExistNewRow = ID_TRUE;;
                            }
                            break;

                        case SDC_UNDO_CHANGE_ROW_PIECE_LINK :
                        case SDC_UNDO_DELETE_ROW_PIECE :
                            sExistNewRow = ID_FALSE;
                            break;

                        default :
                            ideLog::log( 
                                IDE_SERVER_0,
                                "CurUndoRecSID  : %llu\n"
                                "          PID  : %u\n"
                                "          Slot : %u\n"
                                "sPrvUndoPID    : %u\n"
                                "sTableOID      : %lu\n"
                                "sFlag          : %u\n"
                                "sType          : %u\n"
                                ,
                                aCursor->mCurUndoRecSID,
                                SD_MAKE_PID( aCursor->mCurUndoRecSID ),
                                SD_MAKE_SLOTNUM( aCursor->mCurUndoRecSID ),
                                sPrvUndoPID,
                                sTableOID,
                                sFlag,
                                sType );
                            ideLog::logMem( IDE_SERVER_0,
                                            sCurUndoRecHdr,
                                            SDC_UNDOREC_HDR_MAX_SIZE );
                            ideLog::logMem( IDE_SERVER_0,
                                            sCurUndoPage,
                                            SD_PAGE_SIZE );

                            IDE_ASSERT(0);
                            break;
                    }
                }
                else
                {
                    sExistNewRow = ID_FALSE;
                }
            }
            else
            {
                // �ٸ� Ŀ���� �ۼ��� Undo Record�� ��� Skip �Ѵ�.
                sExistNewRow = ID_FALSE;
            }

            if ( sExistNewRow == ID_TRUE )
            {
                // ���� cursor�� ���� undo record �� ��� ,
                // undo record�� �̿��Ͽ� update�� row�� fix
                sdcUndoRecord::parseRowGRID( sCurUndoRecHdr,
                                             &sUpdatedRowGRID );

                IDE_ASSERT( SC_GRID_IS_NOT_NULL( sUpdatedRowGRID ) );

                IDE_ASSERT( sTableSpaceID  ==
                            SC_MAKE_SPACE( sUpdatedRowGRID ) );

                IDE_TEST( sdbBufferMgr::getPageByGRID(
                                        aCursor->mCursorProp.mStatistics,
                                        sUpdatedRowGRID,
                                        SDB_S_LATCH,
                                        SDB_WAIT_NORMAL,
                                        NULL, /* aMtx */
                                        (UChar**)&sUpdatedRow,
                                        &sTrySuccess )
                          != IDE_SUCCESS );
                sIsPageLatchReleased = ID_FALSE;

                //-------------------------------------------------------
                // New Row�� �����ϴ� ���, New Row�� ����
                //-------------------------------------------------------
                IDE_TEST( sdcRow::fetch(
                              aCursor->mCursorProp.mStatistics,
                              NULL, /* aMtx */
                              NULL, /* aSP */
                              aCursor->mTrans,
                              SC_MAKE_SPACE(sUpdatedRowGRID),
                              sUpdatedRow,
                              ID_TRUE, /* aIsPersSlot */
                              SDB_SINGLE_PAGE_READ,
                              aCursor->mCursorProp.mFetchColumnList,
                              SMI_FETCH_VERSION_LAST,
                              SD_NULL_SID, /* aMyTSSRID */
                              NULL,        /* aMyStmtSCN */
                              NULL,        /* aInfiniteSCN */
                              NULL,        /* aIndexInfo4Fetch */
                              NULL,        /* aLobInfo4Fetch */
                              ((smcTableHeader*)aCursor->mTable)->mRowTemplate,
                              (UChar*)*aRow,
                              &sIsRowDeleted,
                              &sIsPageLatchReleased )
                          != IDE_SUCCESS );

                /* BUG-23319
                 * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
                /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
                 * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
                 * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
                 * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
                 * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
                if( sIsPageLatchReleased == ID_FALSE )
                {
                    sIsPageLatchReleased = ID_TRUE;
                    IDE_TEST( sdbBufferMgr::releasePage(
                                  aCursor->mCursorProp.mStatistics,
                                  sUpdatedRow )
                              != IDE_SUCCESS );
                }

                SC_COPY_GRID( sUpdatedRowGRID, *aRowGRID );
            }
            else
            {
                // nothing to do
            }

            //-------------------------------------------------------
            // ���� undo record rid�� ������
            //-------------------------------------------------------
            IDE_TEST( setNextUndoRecSID4NewRow( &sSegInfo,
                                                &sExtInfo,
                                                aCursor,
                                                sCurUndoRecHdr,
                                                &sIsFixUndoPage )
                      != IDE_SUCCESS );

            if ( sExistNewRow == ID_TRUE )
            {
                //-------------------------------------------------------
                // new row ���� ã�� ���, new row �� ã�� �۾��� �ߴ�
                //-------------------------------------------------------
                break;
            }
            else
            {
                // ���� undo record�� �о�� �ϹǷ�
                // ��� ����
            }
        }
    }

    if ( sIsFixUndoPage == ID_TRUE )
    {
        sIsFixUndoPage = ID_FALSE;
        IDE_TEST(sdbBufferMgr::unfixPage( aCursor->mCursorProp.mStatistics,
                                          sCurUndoPage )
                 != IDE_SUCCESS);
    }

    if ( sExistNewRow == ID_FALSE )
    {
        // �� �̻� undo log record�� ���� ���
        *aRow = NULL;
        SC_MAKE_NULL_GRID( *aRowGRID );
    }
    else
    {
        // nothing to do
    }

    IDE_EXCEPTION_CONT( cont_skip_no_data );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if ( sIsFixUndoPage == ID_TRUE )
    {
        IDE_ASSERT(
            sdbBufferMgr::unfixPage(aCursor->mCursorProp.mStatistics,
                                    sCurUndoPage) == IDE_SUCCESS );
    }

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aCursor->mCursorProp.mStatistics,
                                               (UChar*)sUpdatedRow )
                    == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*********************************************************
  function description: readOldRowDRDB
  implementation :
      (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
      (2) old row ��ȯ

***********************************************************/
IDE_RC smiTableCursor::readOldRowDRDB( smiTableCursor * aCursor,
                                       const void    ** aRow,
                                       scGRID         * aRowGRID )
{
    UChar         * sCurUndoPage;
    UChar         * sSlotDirPtr;
    UChar         * sCurUndoRecHdr = NULL;
    idBool          sTrySuccess;
    idBool          sExistOldRow   = ID_FALSE;
    idBool          sIsFixUndoPage = ID_FALSE;
    sdcRowHdrInfo   sRowHdrInfo;
    scGRID          sUpdatedRowGRID;
    UChar         * sUpdatedRow;
    sdcUndoRecFlag  sFlag;
    sdcUndoRecType  sType;
    smOID           sTableOID;
    idBool          sIsRowDeleted;
    scSpaceID       sTableSpaceID
                    = smcTable::getTableSpaceID(aCursor->mTable);
    sdpSegMgmtOp  * sSegMgmtOp;
    sdpSegInfo      sSegInfo;
    sdpExtInfo      sExtInfo;
    scPageID        sSegPID;
    scPageID        sPrvUndoPID = SD_NULL_PID;
    idBool          sIsPageLatchReleased = ID_TRUE;
    smxTrans      * sTrans;

    //------------------------
    // (1) beforeFirstModified()�� ������ �����Ͽ����� �˻�
    //------------------------

    IDE_ERROR( ( aCursor->mFlag & SMI_FIND_MODIFIED_MASK )
               != SMI_FIND_MODIFIED_NEW );

    IDE_ERROR( aCursor->mCursorType != SMI_INSERT_CURSOR );

    //------------------------
    // (2) old row ��ȯ
    //------------------------
    /* BUG-25176 readNewRow���� Data�� ������� ���� ��츦 ������� ����
     * Transaction Segment Entry�� NULL�� ���� �������� ������, �����ϰ�
     * Ȯ���ϰ� ������ skip �Ѵ�. */
    sTrans = aCursor->mTrans;

    if ( sTrans->getTXSegEntry() == NULL )
    {
        *aRow = NULL;
        SC_MAKE_NULL_GRID( *aRowGRID ); 
        IDE_CONT( cont_skip_no_data );
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    sSegPID    = smxTrans::getUDSegPtr(sTrans)->getSegPID();
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_ASSERT( sSegMgmtOp->mGetSegInfo(
                    NULL,
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    sSegPID,
                    NULL, /* aTableHeader */
                    &sSegInfo ) == IDE_SUCCESS );

    IDE_TEST( sSegMgmtOp->mGetExtInfo( NULL,
                                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       aCursor->mCurUndoExtRID,
                                       &sExtInfo )
              != IDE_SUCCESS );

    while( aCursor->mCurUndoRecSID != SD_NULL_SID )
    {
        if ( aCursor->mCurUndoRecSID == aCursor->mLstUndoRecSID )
        {
            // cursor�� ������ undo record�� ��� �о�
            // ���̻� �о�� �� undo record�� ����
            aCursor->mCurUndoRecSID = SD_NULL_SID;
            aCursor->mCurUndoExtRID = SD_NULL_RID;
        }
        else
        {
            if ( sPrvUndoPID != SD_MAKE_PID( aCursor->mCurUndoRecSID ) )
            {
                // ���� undo record�� ����� page fix
                IDE_TEST( sdbBufferMgr::fixPageByPID( aCursor->mCursorProp.mStatistics,
                                                      SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                                      SD_MAKE_PID( aCursor->mCurUndoRecSID ),
                                                      &sCurUndoPage,
                                                      &sTrySuccess )
                          != IDE_SUCCESS );

                sIsFixUndoPage = ID_TRUE;
                sPrvUndoPID    = SD_MAKE_PID( aCursor->mCurUndoRecSID );
            }
            else
            {
                IDE_ASSERT( sIsFixUndoPage == ID_TRUE );
            }

            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sCurUndoPage);

            if( SD_MAKE_SLOTNUM(aCursor->mCurUndoRecSID) ==
                sdpSlotDirectory::getCount(sSlotDirPtr) )
            {
                IDE_ASSERT( aCursor->mCurUndoRecSID ==
                            aCursor->mFstUndoRecSID );
                /*
                 * mFstUndoRecSID�� ���� cursor��
                 * ������ undo record ���� ��ġ�� ����Ų��.
                 * ù��° undo record�� �����Ϸ��� �Ҷ�,
                 * mFstUndoRecSID�� ����Ű�� ���������� ������ �����Ͽ�
                 * ������ �� ������, ���� �������� ù��° undo record��
                 * �����ϰ� �ȴ�.
                 * �׷��� mFstUndoRecSID�� ������ �������� �ʴ� ��ġ�� ����Ű�� �ȴ�.
                 * �̷� ��쿡�� setNextUndoRecSID() �Լ��� ȣ���ϰ� continue�� �Ѵ�.
                 */
                IDE_TEST( setNextUndoRecSID( &sSegInfo,
                                             &sExtInfo,
                                             aCursor,
                                             sCurUndoPage,
                                             &sIsFixUndoPage )
                          != IDE_SUCCESS);
                continue;
            }

            //-------------------------------------------------------
            // undo record�� �����ϴ� ���,
            // ���� cursor�� ���� ������ old row�� �����ϴ��� �˻�
            //-------------------------------------------------------
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                        sSlotDirPtr,
                                        SD_MAKE_SLOTNUM(aCursor->mCurUndoRecSID),
                                        &sCurUndoRecHdr)
                      != IDE_SUCCESS );

            SDC_GET_UNDOREC_HDR_FIELD( sCurUndoRecHdr,
                                       SDC_UNDOREC_HDR_TABLEOID,
                                       &sTableOID);

            if ( aCursor->mTableInfo->mTableOID == sTableOID )
            {
                SDC_GET_UNDOREC_HDR_1B_FIELD( sCurUndoRecHdr,
                                              SDC_UNDOREC_HDR_FLAG,
                                              &sFlag );

                if( ( SDC_UNDOREC_FLAG_IS_UNDO_FOR_HEAD_ROWPIECE(sFlag) == ID_TRUE ) &&
                    ( SDC_UNDOREC_FLAG_IS_VALID(sFlag) == ID_TRUE ) )
                {
                    SDC_GET_UNDOREC_HDR_1B_FIELD( sCurUndoRecHdr,
                                                  SDC_UNDOREC_HDR_TYPE,
                                                  &sType );
 
                    /* xxx �Լ�ó�� */
                    switch( sType )
                    {
                        case SDC_UNDO_DELETE_ROW_PIECE :
                            sExistOldRow = ID_TRUE;
                            break;

                        case SDC_UNDO_UPDATE_ROW_PIECE    :
                        case SDC_UNDO_OVERWRITE_ROW_PIECE :
                            sExistOldRow = ID_TRUE;
                            break;

                        case SDC_UNDO_LOCK_ROW :
                            if( sdcUndoRecord::isExplicitLockRec(
                                   sCurUndoRecHdr) == ID_TRUE )
                            {
                                sExistOldRow = ID_FALSE;
                            }
                            else
                            {
                                sExistOldRow = ID_TRUE;;
                            }
                            break;

                        case SDC_UNDO_CHANGE_ROW_PIECE_LINK :
                        case SDC_UNDO_INSERT_ROW_PIECE :
                            sExistOldRow = ID_FALSE;
                            break;

                        default :
                            ideLog::log( 
                                IDE_SERVER_0,
                                "CurUndoRecSID  : %llu\n"
                                "          PID  : %u\n"
                                "          Slot : %u\n"
                                "sPrvUndoPID    : %u\n"
                                "sTableOID      : %lu\n"
                                "sFlag          : %u\n"
                                "sType          : %u\n"
                                ,
                                aCursor->mCurUndoRecSID,
                                SD_MAKE_PID( aCursor->mCurUndoRecSID ),
                                SD_MAKE_SLOTNUM( aCursor->mCurUndoRecSID ),
                                sPrvUndoPID,
                                sTableOID,
                                sFlag,
                                sType );
                            ideLog::logMem( IDE_SERVER_0,
                                            sCurUndoRecHdr,
                                            SDC_UNDOREC_HDR_MAX_SIZE );
                            ideLog::logMem( IDE_SERVER_0,
                                            sCurUndoPage,
                                            SD_PAGE_SIZE );


                            IDE_ASSERT(0);
                            break;
                    }
                }
                else
                {
                    sExistOldRow = ID_FALSE;
                }
            }
            else
            {
                // �ٸ� Ŀ���� �ۼ��� Undo Record�� ��� Skip �Ѵ�.
                sExistOldRow = ID_FALSE;
            }

            if ( sExistOldRow == ID_TRUE )
            {
                //------------------------
                // ���� cursor�� ���� undo record �� ���,
                // undo record�� �̿��Ͽ� update�� row�� fix
                //------------------------
                sdcUndoRecord::parseRowGRID(sCurUndoRecHdr, &sUpdatedRowGRID);

                sdcUndoRecord::parseRowHdr( sCurUndoRecHdr, &sRowHdrInfo );

                IDE_DASSERT( SC_GRID_IS_NOT_NULL( sUpdatedRowGRID ) );

                IDE_ASSERT( sTableSpaceID  ==
                            SC_MAKE_SPACE( sUpdatedRowGRID ) );

                IDE_TEST( sdbBufferMgr::getPageByGRID(
                                           aCursor->mCursorProp.mStatistics,
                                           sUpdatedRowGRID,
                                           SDB_S_LATCH,
                                           SDB_WAIT_NORMAL,
                                           NULL, /* aMtx */
                                           (UChar**)&sUpdatedRow,
                                           &sTrySuccess )
                          != IDE_SUCCESS );
                sIsPageLatchReleased = ID_FALSE;

                //-------------------------------------------------------
                // Old Row�� �����ϴ� ���, Old Row�� ����
                //-------------------------------------------------------
                IDE_TEST( sdcRow::fetch(
                              aCursor->mCursorProp.mStatistics,
                              NULL, /* aMtx */
                              NULL, /* aSP */
                              aCursor->mTrans,
                              SC_MAKE_SPACE(sUpdatedRowGRID),
                              sUpdatedRow,
                              ID_TRUE, /* aIsPersSlot */
                              SDB_SINGLE_PAGE_READ,
                              aCursor->mCursorProp.mFetchColumnList,
                              SMI_FETCH_VERSION_CONSISTENT,
                              smxTrans::getTSSlotSID( aCursor->mTrans ),
                              &aCursor->mSCN,
                              &aCursor->mInfinite,
                              NULL, /* aIndexInfo4Fetch */
                              NULL, /* aLobInfo4Fetch */
                              ((smcTableHeader*)aCursor->mTable)->mRowTemplate,
                              (UChar*)*aRow,
                              &sIsRowDeleted,
                              &sIsPageLatchReleased )
                          != IDE_SUCCESS );

                /* BUG-23319
                 * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
                /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
                 * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
                 * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
                 * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
                 * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
                if( sIsPageLatchReleased == ID_FALSE )
                {
                    sIsPageLatchReleased = ID_TRUE;
                    IDE_TEST( sdbBufferMgr::releasePage(
                                  aCursor->mCursorProp.mStatistics,
                                  (UChar*)sUpdatedRow ) != IDE_SUCCESS );
                }

                SC_COPY_GRID( sUpdatedRowGRID, *aRowGRID );
            }
            else
            {
                // nothing to do
            }

            //-------------------------------------------------------
            // ���� undo record rid�� ������
            //-------------------------------------------------------
            IDE_TEST( setNextUndoRecSID( &sSegInfo,
                                         &sExtInfo,
                                         aCursor,
                                         sCurUndoRecHdr,
                                         &sIsFixUndoPage )
                      != IDE_SUCCESS);

            if ( sExistOldRow == ID_TRUE )
            {
                //-------------------------------------------------------
                // old row ���� ã�� ���,
                // old row �� ã�� �۾��� �ߴ�
                //-------------------------------------------------------
                break;
            }
            else
            {
                // ���� undo record�� �о�� �ϹǷ�
                // ��� ����
            }
        }
    }

    if ( sIsFixUndoPage == ID_TRUE )
    {
        sIsFixUndoPage = ID_FALSE;
        IDE_TEST(sdbBufferMgr::unfixPage(aCursor->mCursorProp.mStatistics,
                                         sCurUndoPage)
                 != IDE_SUCCESS);
    }

    if ( sExistOldRow == ID_FALSE )
    {
        *aRow = NULL;
        SC_MAKE_NULL_GRID( *aRowGRID );
    }

    IDE_EXCEPTION_CONT( cont_skip_no_data );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if ( sIsFixUndoPage == ID_TRUE )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage(
                        aCursor->mCursorProp.mStatistics,
                        sCurUndoPage ) == IDE_SUCCESS );
    }

    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aCursor->mCursorProp.mStatistics,
                                               (UChar*)sUpdatedRow )
                    == IDE_SUCCESS );
    }
    IDE_POP();

    return IDE_FAILURE;

}

IDE_RC smiTableCursor::setNextUndoRecSID4NewRow( sdpSegInfo     * aSegInfo,
                                                 sdpExtInfo     * aExtInfo,
                                                 smiTableCursor * aCursor,
                                                 UChar          * aCurUndoRecHdr,
                                                 idBool         * aIsFixPage )
{
    IDE_TEST( setNextUndoRecSID( aSegInfo,
                                 aExtInfo,
                                 aCursor,
                                 aCurUndoRecHdr,
                                 aIsFixPage ) != IDE_SUCCESS );

    if ( aCursor->mCursorType == SMI_COMPOSITE_CURSOR )
    {
        if ( aCursor->mCurUndoRecSID == SD_NULL_SID )
        {
            if ( ( aCursor->mFlag & SMI_READ_UNDO_PAGE_LIST_MASK )
                 == SMI_READ_UNDO_PAGE_LIST_UPDATE )
            {
                aCursor->mFlag &= ~SMI_READ_UNDO_PAGE_LIST_MASK;
                aCursor->mFlag |= SMI_READ_UNDO_PAGE_LIST_INSERT;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smiTableCursor::setNextUndoRecSID( sdpSegInfo     * aSegInfo,
                                          sdpExtInfo     * aExtInfo,
                                          smiTableCursor * aCursor,
                                          UChar          * aCurUndoRec,
                                          idBool         * aIsFixPage )
{
    UChar         * sCurUndoRec;
    UChar         * sSlotDirPtr;
    scPageID        sCurUndoPageID;
    SInt            sUndoRecCnt;
    SInt            sCurRecSlotNum;
    UChar         * sNxtUndoPage;
    idBool          sTrySuccess;
    UInt            sState = 0;
    sdpSegMgmtOp  * sSegMgmtOp;

    IDE_ASSERT( *aIsFixPage == ID_TRUE );

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    sCurUndoRec    = aCurUndoRec;
    sCurUndoPageID = SD_MAKE_PID( aCursor->mCurUndoRecSID );
    sSlotDirPtr    = sdpPhyPage::getSlotDirStartPtr( sCurUndoRec );
    sUndoRecCnt    = sdpSlotDirectory::getCount( sSlotDirPtr );
    sCurRecSlotNum = SD_MAKE_SLOTNUM( aCursor->mCurUndoRecSID );

    sCurRecSlotNum++;
    if( sCurRecSlotNum < sUndoRecCnt )
    {
        //---------------------------------
        // ���� undo page���� ���� undo record�� ����
        //---------------------------------

        // ���� undo record�� SID�� mCurUndoRecSID�� ����
        aCursor->mCurUndoRecSID =
            SD_MAKE_SID( sCurUndoPageID, sCurRecSlotNum );
    }
    else
    {
        /*
         * ### sCurRecSlotNum == sUndoRecCnt + 1 �� ��� ###
         *
         *  ù��° undo record�� �����Ϸ��� �Ҷ�,
         * mFstUndoRecSID�� ����Ű�� �������� ������ �����Ͽ�
         * ������ �� ������, ���� �������� ù��° undo record��
         * �����ϰ� �ȴ�.
         * �׷��� mFstUndoRecSID�� ������ �������� �ʴ� ��ġ�� ����Ű�� �ȴ�.
         */
        IDE_ASSERT( (sCurRecSlotNum == sUndoRecCnt) ||
                    (sCurRecSlotNum == (sUndoRecCnt+1)) );

        //---------------------------------
        // ���� undo page���� �����ϴ� undo record ������ �Ѿ�� ���,
        // ���� undo page�� �̵�
        //---------------------------------

        if ( sCurUndoPageID == SD_MAKE_PID( aCursor->mLstUndoRecSID ) )
        {
            // To Fix BUG-14798
            //    last undo record rid�� cursor close���,
            //    ������ ������ ���� undo record�� rid �̴�.
            //    ���� undo page���� �����ϴ� undo record ������ �Ѿ��,
            //    ���� undo page�� ������ undo page �̶�� ����
            //    cursor close ���� undo record�� �������� �ʾ�����
            //    ���� cursor�� ������ undo record�� ��� �о����� �ǹ��Ѵ�.

            aCursor->mCurUndoRecSID = SD_NULL_SID;
            aCursor->mCurUndoExtRID = SD_NULL_RID;
        }
        else
        {
            IDE_TEST( sSegMgmtOp->mGetNxtAllocPage(
                                  NULL, /* idvSQL */
                                  SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                  aSegInfo,
                                  NULL,
                                  &aCursor->mCurUndoExtRID,
                                  aExtInfo,
                                  &sCurUndoPageID )
                      != IDE_SUCCESS );

            *aIsFixPage = ID_FALSE;
            IDE_TEST( sdbBufferMgr::unfixPage(
                                  aCursor->mCursorProp.mStatistics,
                                  sCurUndoRec ) != IDE_SUCCESS );

            // ���� undo page fix
             IDE_TEST( sdbBufferMgr::fixPageByPID( aCursor->mCursorProp.mStatistics,
                                                   SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                                   sCurUndoPageID,
                                                   (UChar**)&sNxtUndoPage,
                                                   &sTrySuccess )
                       != IDE_SUCCESS );
            sState = 1;

            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sNxtUndoPage);
            sUndoRecCnt = sdpSlotDirectory::getCount(sSlotDirPtr);
            IDE_ASSERT(sUndoRecCnt > 0);

            // ���� undo record�� RID�� mCurUndoRecRID�� ����
            aCursor->mCurUndoRecSID = SD_MAKE_SID( sCurUndoPageID, 1 );

            sState = 0;
            IDE_TEST( sdbBufferMgr::unfixPage(
                                    aCursor->mCursorProp.mStatistics,
                                    sNxtUndoPage ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT(
            sdbBufferMgr::unfixPage( aCursor->mCursorProp.mStatistics,
                                     sNxtUndoPage ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}



/*********************************************************************************/
/* For A4 : interface what to setRemote Query Specific Functions(PROJ-1068)     */
/*********************************************************************************/
IDE_RC smiTableCursor::setRemoteQueryCallback( smiTableSpecificFuncs *aFuncs,
                                               smiGetRemoteTableNullRowFunc aGetNullRowFunc )
{
    openRemoteQuery = aFuncs->openFunc;
    closeRemoteQuery = aFuncs->closeFunc;
    updateRowRemoteQuery = aFuncs->updateRowFunc;
    deleteRowRemoteQuery = aFuncs->deleteRowFunc;
    beforeFirstModifiedRemoteQuery = aFuncs->beforeFirstModifiedFunc;
    readOldRowRemoteQuery = aFuncs->readOldRowFunc;
    readNewRowRemoteQuery = aFuncs->readNewRowFunc;

    mGetRemoteTableNullRowFunc = aGetNullRowFunc;
    
    return IDE_SUCCESS;
}


/**********************************************************************
 * Description: aIterator�� ���� ����Ű�� �ִ� Row�� ���ؼ� XLock��
 *              ȹ���մϴ�.
 *
 * aIterator - [IN] Cursor�� Iterator
 *
 * Related Issue:
 *   BUG-19068: smiTableCursor�� ���簡��Ű�� �ִ� Row�� ���ؼ�
 *              Lock�� ������ �մ� Interface�� �ʿ��մϴ�.
 *
 *********************************************************************/
IDE_RC smiTableCursor::lockRow()
{
    return mLockRow( mIterator );
}


/**********************************************************************
 * Description: ViewSCN�� Statement/TableCursor/Iterator���� �������� 
 *              ���Ѵ�.
 *
 * this - aIterator - [IN] Cursor�� Iterator
 *
 * Related Issue:
 * BUG-31993 [sm_interface] The server does not reset Iterator ViewSCN 
 * after building index for Temp Table 
 *
 *********************************************************************/
IDE_RC smiTableCursor::checkVisibilityConsistency()
{
    idBool sConsistency = ID_TRUE;
    smSCN  sStatementSCN;
    smSCN  sCursorSCN;
    smSCN  sIteratorSCN;

    if( ( mStatement != NULL ) &&
        ( mIterator != NULL ) )
    {
        SM_SET_SCN( &sStatementSCN, &mStatement->mSCN );
        SM_SET_SCN( &sCursorSCN,    &mSCN             );
        SM_SET_SCN( &sIteratorSCN,  &mIterator->SCN   );

        SM_CLEAR_SCN_VIEW_BIT( &sStatementSCN );
        SM_CLEAR_SCN_VIEW_BIT( &sCursorSCN    );
        SM_CLEAR_SCN_VIEW_BIT( &sIteratorSCN  );

        if( ( !SM_SCN_IS_EQ( &sStatementSCN, &sCursorSCN ) ) || 
            ( !SM_SCN_IS_EQ( &sStatementSCN, &sIteratorSCN ) ) )
        {
            sConsistency = ID_FALSE;
        }
    }

    if( sConsistency == ID_FALSE )
    {
        ideLog::logCallStack( IDE_SM_0 );

        ideLog::log( IDE_SM_0,
                     "VisibilityConsistency - Tablecursor\n"
                     "StatmentSCN : 0x%llx\n"
                     "CursorSCN   : 0x%llx\n"
                     "IteratorSCN : 0x%llx\n",
                     SM_SCN_TO_LONG( mStatement->mSCN ),
                     SM_SCN_TO_LONG( mSCN ),
                     SM_SCN_TO_LONG( mIterator->SCN ) );

        ideLog::logMem( IDE_SM_0,
                        ( (UChar*) mStatement ) - 512 ,
                        ID_SIZEOF( smiStatement ) + 1024 );
        ideLog::logMem( IDE_SM_0,
                        ( (UChar*) this ) - 512,
                        ID_SIZEOF( smiTableCursor ) + 1024 );
        ideLog::logMem( IDE_SM_0,
                        ( (UChar*) mIterator ) - 512,
                        ID_SIZEOF( smiIterator ) + 1024 );

        IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;
}


IDE_RC smiTableCursor::destFullUpdateMRDB( smiColumnList  ** aNewColumnList,
                                           smiValue       ** aNewValueList,
                                           SChar          ** aOldValueBuffer )
{
    IDE_TEST( iduMemMgr::free( *aNewColumnList )  != IDE_SUCCESS );
    *aNewColumnList = NULL;

    IDE_TEST( iduMemMgr::free( *aNewValueList )   != IDE_SUCCESS );
    *aNewValueList = NULL;

    if( *aOldValueBuffer != NULL )
    {
        /* BUG-40650 �ӽ� ���۸� ����ϴ� ��쿡�� ��ȯ�Ѵ�. */
        IDE_TEST( iduMemMgr::free( *aOldValueBuffer ) != IDE_SUCCESS );
        *aOldValueBuffer = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smiTableCursor::destFullUpdateDRDB( smiColumnList  * aNewColumnList,
                                           smiValue       * aNewValueList,
                                           SChar          * aOldValueBuffer )
{
    IDE_TEST( iduMemMgr::free( aNewColumnList )  != IDE_SUCCESS );
    IDE_TEST( iduMemMgr::free( aNewValueList )   != IDE_SUCCESS );
    IDE_TEST( iduMemMgr::free( aOldValueBuffer ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-2264
IDE_RC smiTableCursor::setAgingIndexFlag( smiTableCursor * aCursor,
                                          SChar          * aRecPtr,
                                          UInt             aFlag )
{
    smxOIDNode     * sHead;
    smxOIDNode     * sNode;
    smxOIDInfo     * sNodeCursor;
    smOID            sOID;
    UInt             sOIDPos;
    smcTableHeader * sTable;

    sTable = (smcTableHeader*)aCursor->mTable;

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore 
     * the failure of index aging. */
    sHead = &( aCursor->mTrans->mOIDList->mOIDNodeListHead );
    sOID = SMP_SLOT_GET_OID( aRecPtr );

    /* BackTraverse */
    sNode   = sHead;
    sOIDPos = 0;
    do
    {
        if( sOIDPos > 0 ) /* remain oid */
        {
            sOIDPos --;
        }
        else
        {
            sNode   = sNode->mPrvNode;
            if( ( sNode == sHead ) ||
                ( sNode->mOIDCnt == 0 ) )
            {
                /* CircularList�̱� ������, ���ٴ� ���� ����ٴ� ��.
                 * ��� �ִٸ�, Rollback/Stamping/Aging ������ �ʿ� ���ٴ� ��
                 * ���� �׷� ��� ������.*/
                /* ������ Node�� ������ */
                ideLog::log( IDE_DUMP_0, 
                             "mSpaceID : %u\n"
                             "mOID     : %lu",
                             sTable->mSpaceID,
                             sOID );
                IDE_ERROR( 0 );
            }
            else
            {
                sOIDPos = sNode->mOIDCnt - 1;
            }
        }
        sNodeCursor = &sNode->mArrOIDInfo[ sOIDPos ];
    }
    while( sNodeCursor->mTargetOID != sOID ) ;

    IDE_ASSERT( ( sNodeCursor->mFlag & SM_OID_ACT_AGING_INDEX )
                == 0 );

    /* PROJ-2429 Dictionary based data compress for on-disk DB 
     * Flag�� ���� rollback���� �� aging �� ������ ����(SM_OID_ACT_AGING_INDEX) �ְ� 
     * ���� ���� ����(SM_OID_ACT_AGING_ROLLBACK, dictionary table�� ���) �ִ�.*/
    sNodeCursor->mFlag |= aFlag;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC smiTableCursor::getTableNullRow( void ** aRow, scGRID * aRid )
{
#ifdef  DEBUG
    UInt sTableType;

    sTableType = ((smcTableHeader *)mTable)->mFlag & SMI_TABLE_TYPE_MASK;

    IDE_DASSERT( sTableType == SMI_TABLE_REMOTE );
#endif
    IDE_TEST_RAISE( mGetRemoteTableNullRowFunc == NULL, NO_CALLBACK_FUNC );

    IDE_TEST( mGetRemoteTableNullRowFunc( this, aRow, aRid ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( NO_CALLBACK_FUNC )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NO_CALLBACK ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
